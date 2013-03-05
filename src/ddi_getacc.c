/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Subroutines associated with the get-accumulate operation.
 *
 * Author: Ryan M. Olson
 * CVS $Id: ddi_getacc.c,v 1.1.1.1 2005/11/17 00:12:34 ryan Exp $
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"


/* -------------------------------------------------------------- *\
   DDI_GetAcc(handle,ilo,ihi,jlo,jhi,buff)
   ====================================
   [IN] handle - Handle of the distributed array to be accessed.
   [IN] ilo    - Lower row bound of the patch of array handle.
   [IN] ihi    - Upper row bound of the patch of array handle.
   [IN] jlo    - Lower col bound of the patch of array handle.
   [IN] jhi    - Upper col bound of the patch of array handle.
   [IN] buff   - Data segment to be operated on.
\* -------------------------------------------------------------- */
   void DDI_GetAcc(int handle,int ilo,int ihi,int jlo,int jhi,void *buff) {
      DDI_Patch Patch;

      Patch.ilo = ilo;
      Patch.ihi = ihi;
      Patch.jlo = jlo;
      Patch.jhi = jhi;
      
      DDI_GetAccP(handle,&Patch,buff);      
   }
  
 
/* -------------------------------------------------------------- *\
   DDI_GetAccP(handle,patch,buff)
   ============================
   [IN] handle - Handle of the distributed array to be accessed.
   [IN] patch  - structure containing ilo, ihi, jlo, jhi, etc.
   [IN] buff   - Data segment to be operated on.
\* -------------------------------------------------------------- */
   void DDI_GetAccP(int handle,DDI_Patch *patch,void *buff) {
   
   /* --------------- *\
      Local Variables
   \* --------------- */
      char ack=57;
      int i,np,me,nn,my,remote_id,nsubp;
      int ranks[MAX_NODES];
      DDI_Patch subp[MAX_NODES];
      char *working_buffer = (char *) buff;

    # if defined DDI_LAPI
      DDI_Patch *local_patch = NULL;
      lapi_cntr_t cntr[MAX_NODES];
    # endif
    
      STD_DEBUG((stdout,"%s: Entering DDI_GetAccP.\n",DDI_Id()))

   /* -------------------- *\
      Process OR Node Rank
   \* -------------------- */
      DDI_NProc(&np,&me);
      DDI_NNode(&nn,&my);


   /* ------------------------------------- *\
      Ensure the patch has the correct info
   \* ------------------------------------- */
      patch->oper   = DDI_GETACC;
      patch->handle = handle;


   /* ---------------------------------- *\
      Check calling arguments for errors
   \* ---------------------------------- */
    # if defined DDI_CHECK_ARGS
      if(handle < 0 || handle >= gv(ndda)) {
         fprintf(stdout,"%s: Invalid handle [%i] in DDI_GetAcc.\n",DDI_Id(),handle);
         Fatal_error(911);
      }
      
      if(patch->ilo > patch->ihi || patch->ilo < 0 || patch->ihi >= gv(nrow)[handle]) {
         fprintf(stdout,"%s: Invalid row dimensions during DDI_GetAcc => ilo=%i ihi=%i.\n",DDI_Id(),patch->ilo,patch->ihi);
         Fatal_error(911);
      }
      
      if(patch->jlo > patch->jhi || patch->jlo < 0 || patch->jhi >= gv(ncol)[handle]) {
         fprintf(stdout,"%s: Invalid colum dimensions during DDI_GetAcc => jlo=%i jhi=%i.\n",DDI_Id(),patch->jlo,patch->jhi);
         Fatal_error(911);
      }
    # endif


   /* ------------------------------ *\
      Log some simple profiling info
   \* ------------------------------ */
    # if defined DDI_COUNTERS
      gv(acc_profile).ncalls++;
      gv(acc_profile).nbytes += DDI_Patch_sizeof(patch);
    # endif


   /* ------------------------------------------------------- *\
      Determine where the pieces of the requested patch exist
   \* ------------------------------------------------------- */
      DDI_Subpatch(handle,patch,&nsubp,ranks,subp);
      MAX_DEBUG((stdout,"%s: %i subpatches.\n",DDI_Id(),nsubp))

      
   /* ------------------------------------------------------------------- *\
      Send data requests for all non-local pieces of the requested patch.
      Operate immediately to GetAcc a local portion of the patch.
   \* ------------------------------------------------------------------- */
      for(i=0; i<nsubp; i++) {
         ULTRA_DEBUG((stdout,"%s: GetAccumulating subpatch %i.\n",DDI_Id(),i))

      /* ------------------------------------------------------------- *\
         Using SysV, take advantage of shared-memory for a local patch
      \* ------------------------------------------------------------- */
       # if defined USE_SYSV

      /* ------------------------------------------------ *\
         Determine if the ith patch is local to 'my' node
      \* ------------------------------------------------ */
         if(ranks[i] == my) {
            MAX_DEBUG((stdout,"%s: Subpatch %i is local.\n",DDI_Id(),i))

         /* ---------------------------------------------------- *\
            Using LAPI, perform the local Getacc after all the data
            requests have been sent ==> maximize concurrency.
         \* ---------------------------------------------------- */
          # if defined DDI_LAPI
            local_patch = &subp[i];
            local_patch->cp_buffer_addr = working_buffer;
          # else
         /* --------------------------------------------- *\
            Otherwise, perform the local Getacc immediately.
         \* --------------------------------------------- */
            DDI_GetAcc_local(&subp[i],working_buffer);
          # endif

         /* ------------------------------------------------------- *\
            Move the working buffer to the next patch and continue.
         \* ------------------------------------------------------- */
            working_buffer += subp[i].size;
            continue;
         }
       # endif


      /* --------------------------------- *\
         If the current patch is NOT local 
      \* --------------------------------- */
         remote_id = ranks[i];


      /* ----------------------------------------------- *\
         Using LAPI, then include some extra information
      \* ----------------------------------------------- */
       # if defined DDI_LAPI
         subp[i].cp_lapi_id     = gv(lapi_map)[me];
         subp[i].cp_lapi_cntr   = (void *) &cntr[i];
         subp[i].cp_buffer_addr = (void *) working_buffer;
         LAPI_Setcntr(gv(lapi_hnd),&cntr[i],0);

         ULTRA_DEBUG((stdout,"%s: cp_lapi_id=%i.\n",DDI_Id(),gv(lapi_map)[me]))
         ULTRA_DEBUG((stdout,"%s: cp_lapi_cntr=%x.\n",DDI_Id(),&cntr[i]))
         ULTRA_DEBUG((stdout,"%s: cp_buffer_addr=%x.\n",DDI_Id(),working_buffer))
       # endif
      
      /* -------------------------------- *\
         Send data request for subpatch i
      \* -------------------------------- */
         MAX_DEBUG((stdout,"%s: Sending data request to node %i.\n",DDI_Id(),remote_id))
         DDI_Send_request(&subp[i],&remote_id,NULL);
         MAX_DEBUG((stdout,"%s: data request sent to global process %i.\n",DDI_Id(),remote_id))


      /* ------------------------------------------------------------ *\
         Receive an acknowledgement that the data server has raised
         a fence that will protect the distributed array from get or
         put access until all accumulates have finished.  This block-
         ing receive ensures that the current process executing this
         accumulate can *NOT* finish, until the fence has been raised 
      \* ------------------------------------------------------------ */
       # if !defined DDI_LAPI
       # if defined USE_SYSV
         MAX_DEBUG((stdout,"%s: Receiving remote fence ACK.\n",DDI_Id()))
         DDI_Recv(&ack,1,remote_id);
       # endif


      /* ---------------------------- *\
         Recv subpatch from remote_id
      \* ---------------------------- */
         MAX_DEBUG((stdout,"%s: Sending subpatch %i to %i.\n",DDI_Id(),i,remote_id))
         DDI_Send(working_buffer,subp[i].size,remote_id);
         DDI_Recv(working_buffer,subp[i].size,remote_id);
       # endif

      
      /* ------------ *\
         Shift buffer 
      \* ------------ */
         working_buffer += subp[i].size;
      }

   /* ----------------------------------------------------------- *\
      Using LAPI, perform the local Getaccumulate (if needed) as the
      remote processes are getting the data to Getaccumulate on the
      target processes.  Then wait for all the data to be copied
      out of the buffer before returning.
   \* ----------------------------------------------------------- */
    # if defined DDI_LAPI

   /* ------------------------------------ *\
      GetAccumulating local patch (if exists)
   \* ------------------------------------ */
      if(local_patch) DDI_GetAcc_local(local_patch,local_patch->cp_buffer_addr);

   /* ---------------------------------------------------------- *\
      Wait for all remote LAPI_Gets to finish copying local data
   \* ---------------------------------------------------------- */
      for(i=0; i<nsubp; i++) {
         if(subp[i].cp_lapi_cntr) {
            ULTRA_DEBUG((stdout,"%s: Wait for subpatch %i to be copied.\n",DDI_Id(),i))
            LAPI_Waitcntr(gv(lapi_hnd),&cntr[i],3,NULL);
            ULTRA_DEBUG((stdout,"%s: Subpatch %i copy completed.\n",DDI_Id(),i))
         }
      }
    # endif

      MAX_DEBUG((stdout,"%s: Leaving DDI_GetAccP.\n",DDI_Id()))
      
   }


/* -------------------------------------------------------------- *\
   DDI_GetAcc_local(patch,buff)
   =========================
   [IN] patch  - structure containing ilo, ihi, jlo, jhi, etc.
   [IN] buff   - Data segment to be operated on.
   
   GetAccumulates the subpatch specified by patch and stored in buff
   into the share-memory segment(s) of the local node.
\* -------------------------------------------------------------- */
   void DDI_GetAcc_local(const DDI_Patch* patch,void *buff) {
   
   /* --------------- *\
      Local Variables
   \* --------------- */
      DDA_Index *Index = gv(dda_index);
      int i,j,nrows,ncols,start_row,start_col;
      size_t dda_offset,size;
      double tmp,*dda,*dloc = (double *) buff;
      
      int handle = patch->handle;
      int ilo = patch->ilo;
      int ihi = patch->ihi;
      int jlo = patch->jlo;
      int jhi = patch->jhi;

      int trows = Index[handle].nrows;

    # if FULL_SMP
      int icpu,smpme,smpnp;
      DDI_SMP_NProc(&smpnp,&smpme);
    # endif

      MAX_DEBUG((stdout,"%s: Entering DDI_GetAcc_local.\n",DDI_Id()))

   /* ------------------------------------------------------------------ *\
      For FULL SMP implementations, loop on the number of SMP processors 
   \* ------------------------------------------------------------------ */
    # if FULL_SMP
      for(icpu=0; icpu<smpnp; icpu++) {
        Index = gv(smp_index)[icpu];
        jlo = Index[handle].jlo;
        jhi = Index[handle].jhi;
        if(jlo > patch->jhi || jhi < patch->jlo) continue;
        if(patch->jlo > jlo) jlo = patch->jlo;
        if(jhi > patch->jhi) jhi = patch->jhi;
    # endif

      nrows = ihi - ilo + 1;
      ncols = jhi - jlo + 1;
      size  = nrows*ncols;
       
      start_row = ilo - Index[handle].ilo;
      start_col = jlo - Index[handle].jlo;

   /* ---------------------------------------------------------- *\
      If the patch and the DD array have the same row dimensions
   \* ---------------------------------------------------------- */
      if(nrows == trows) {
         dda_offset = start_col*nrows;
         DDI_Acquire(Index,handle,DDI_WRITE_ACCESS,(void **) &dda);
         dda  += dda_offset;
         for(i=0; i<size; i++) {
            tmp = dda[i];
            dda[i]  += dloc[i];
            dloc[i]  = tmp;
         }
         DDI_Release(Index,handle,DDI_WRITE_ACCESS);
         dloc += size;
      } else {
   /* ----------------------------------------------- *\
      Otherwise, pack the local patch into the buffer
   \* ----------------------------------------------- */
         DDI_Acquire(Index,handle,DDI_WRITE_ACCESS,(void **) &dda);
         dda_offset = start_col*trows;
         dda += dda_offset;
         dda += start_row;
         size = nrows*sizeof(double);
         for(i=0; i<ncols; i++) {
            for(j=0; j<nrows; j++) {
               tmp = dda[j];
               dda[j] += dloc[j];
               dloc[j] = tmp;
            }
            dloc += nrows;
            dda  += trows;
         }
         DDI_Release(Index,handle,DDI_WRITE_ACCESS);
      }

    # if FULL_SMP
      } /* end for-loop on local cpus */
    # endif

    
   /* --------------------- *\
      Shared-memory counter 
   \* --------------------- */
    # if defined DDI_COUNTERS
      gv(acc_profile).ncalls_shmem++;
      gv(acc_profile).nbytes_shmem += patch->size;
    # endif

      MAX_DEBUG((stdout,"%s: Leaving DDI_GetAcc_local.\n",DDI_Id()))
   }



/* ----------------------------------------------------------------- *\
   DDI_GetAcc_server(patch,from)
   ==========================
   [IN] patch - structure containing ilo, ihi, jlo, jhi, etc.
   [IN] from  - rank of DDI process sending data to be accumulated.
   
   Used by the data server to accept incoming data and perform a
   local accumulate.  Note, the fence is raised to protect the array
   from local get/put operations until the accumulate has finished.
\* ----------------------------------------------------------------- */
   void DDI_GetAcc_server(const DDI_Patch *msg,int from) {
   
   /* --------------- *\
      Local Variables
   \* --------------- */
      char ack = 57;
      void *buffer = NULL;

   /* -------------------------------------------------------------------- *\
      Raise protective fence.  This is necessary because a compute process
      can finish with the DDI_Acc subroutine before the remote data server
      has finished accumulating the patch.
   \* -------------------------------------------------------------------- */
    # if defined USE_SYSV
      DDI_Fence_acquire(msg->handle);
      DDI_Send(&ack,1,from);
    # endif


   /* ----------------------------------------------------------------- *\
      If enough memory is available to receive all the data in a single
      message, then do so ... otherwise receive and update in batches.
      *TODO: Implement the second option*
   \* ----------------------------------------------------------------- */
      DDI_Memory_push(msg->size,&buffer,NULL);


   /* ----------------------- *\
      Receive and update data
   \* ----------------------- */
      DDI_Recv(buffer,msg->size,from);
      DDI_GetAcc_local(msg,buffer);
      DDI_Send(buffer,msg->size,from);


   /* ------------------- *\
      Free receive buffer
   \* ------------------- */
      DDI_Memory_pop(msg->size);


   /* --------------- *\
      Take down fence
   \* --------------- */
    # if defined USE_SYSV
      DDI_Fence_release(msg->handle);
    # endif

   }


/* ------------------------------------------------------------------------ *\
   DDI_GetAcc_lapi_server(patch,buffer)
   =================================
   [IN] patch  - structure containing ilo, ihi, jlo, jhi, etc.
   [IN] buffer - Data segment to be operated on.
   
   Called by the LAPI target process whose handlers act like a data server.
   This subroutine gets data from the originating process (via LAPI_Get).
   Once the data has been transferred to the target process, a local acc is
   performed through DDI_Acc_local.
\* ------------------------------------------------------------------------ */
 # if defined DDI_LAPI
   void DDI_GetAcc_lapi_server(DDI_Patch *patch, void *buffer) {

   /* --------------- *\
      Local Variables 
   \* --------------- */
      lapi_handle_t hndl;
      uint tgt;
      ulong len;
      void *tgt_addr,*org_addr;
      lapi_cntr_t *tgt_cntr,*org_cntr,*cmpl_cntr;
      lapi_cntr_t local_cntr;

      STD_DEBUG((stdout,"%s: Entering DDI_GetAcc_lapi_server.\n",DDI_Id()))

   /* ----------------------------------------- *\
      Set up the calling arguments for LAPI_Get 
   \* ----------------------------------------- */
      hndl      = gv(lapi_hnd);                        /* LAPI Handle */
      tgt       = patch->cp_lapi_id;                   /* Target for the LAPI_Get */
      len       = (ulong) patch->size;                 /* Amount of data to get */
      tgt_addr  = patch->cp_buffer_addr;               /* Addr at target to get */
      org_addr  = buffer;                              /* Local Addr for data that is got */
      tgt_cntr  = (lapi_cntr_t *) patch->cp_lapi_cntr; /* Target counter */ 
      org_cntr  = &local_cntr;                         /* Local counter -> incremented once
                                                          the LAPI_Get is completed */
      cmpl_cntr = NULL;
                                                  
      ULTRA_DEBUG((stdout,"%s: DDI_Patch -> ilo=%i ihi=%i jlo=%i jhi=%i size=%lu.\n",
                   DDI_Id(),patch->ilo,patch->ihi,patch->jlo,patch->jhi,patch->size))
      ULTRA_DEBUG((stdout,"%s: org buffer_addr=%x cntr_addr=%x.\n",DDI_Id(),org_addr,org_cntr))
      ULTRA_DEBUG((stdout,"%s: tgt id=%i buffer_addr=%x cntr_addr=%x.\n",DDI_Id(),tgt,tgt_addr,tgt_cntr))


   /* -------------------------------------------------------------------------- *\
      We are interested in when the LAPI_Get is finished, zero the local counter
   \* -------------------------------------------------------------------------- */
      if(LAPI_Setcntr(hndl,org_cntr,0) != LAPI_SUCCESS) {
         fprintf(stdout,"%s: LAPI_Setcntr failed in DDI_GetAcc_lapi_server.\n",DDI_Id());
         Fatal_error(911);
      }
      ULTRA_DEBUG((stdout,"%s: Initializing local LAPI counter.\n",DDI_Id()))


   /* --------------------------------------------------- *\
      Execute the LAPI_Get.  This is a non-blocking call.
   \* --------------------------------------------------- */
      if(LAPI_Get(hndl,tgt,len,tgt_addr,org_addr,tgt_cntr,org_cntr) != LAPI_SUCCESS) {
         fprintf(stdout,"%s: LAPI_Get failed in DDI_GetAcc_lapi_server.\n",DDI_Id());
         Fatal_error(911);
      }
      MAX_DEBUG((stdout,"%s: Executing LAPI_Get from %i.\n",DDI_Id(),tgt))


   /* ------------------------------------------------------------------------ *\
      Wait here until the local counter is incremented ==> LAPI_Get completed.
   \* ------------------------------------------------------------------------ */
      if(LAPI_Waitcntr(hndl,org_cntr,1,NULL) != LAPI_SUCCESS) {
         fprintf(stdout,"%s: LAPI_Waitcntr failed in DDI_GetAcc_lapi_server.\n",DDI_Id());
         Fatal_error(911);
      }
      MAX_DEBUG((stdout,"%s: LAPI_Get from %i completed.\n",DDI_Id(),tgt))
      

   /* -------------------------------------------------------------- *\
      Place the data (now local) into the shared-memory of the node.
   \* -------------------------------------------------------------- */
      MAX_DEBUG((stdout,"%s: LAPI handler calling DDI_GetAcc_local.\n",DDI_Id()))
      DDI_GetAcc_local(patch,buffer);
      MAX_DEBUG((stdout,"%s: LAPI handler completed DDI_GetAcc_local.\n",DDI_Id()))
      STD_DEBUG((stdout,"%s: Exiting DDI_GetAcc_lapi_server.\n",DDI_Id()))


   /* --------------------------------------------------- *\
      Execute the LAPI_Put.  This is a non-blocking call.
   \* --------------------------------------------------- */
      if(LAPI_Put(hndl,tgt,len,tgt_addr,org_addr,tgt_cntr,org_cntr,cmpl_cntr) != LAPI_SUCCESS) {
         fprintf(stdout,"%s: LAPI_Get failed in DDI_GetAcc_lapi_server.\n",DDI_Id());
         Fatal_error(911);
      }
      MAX_DEBUG((stdout,"%s: Executing LAPI_Put from %i.\n",DDI_Id(),tgt))


   /* ------------------------------------------------------------------------- *\
      Wait here until the local counter is incremented ==> LAPI_Put has copied.
   \* ------------------------------------------------------------------------- */
      if(LAPI_Waitcntr(hndl,org_cntr,1,NULL) != LAPI_SUCCESS) {
         fprintf(stdout,"%s: LAPI_Waitcntr failed in DDI_GetAcc_lapi_server.\n",DDI_Id());
         Fatal_error(911);
      }
      MAX_DEBUG((stdout,"%s: LAPI_Put copied data enroute to %i.\n",DDI_Id(),tgt))

   }
 # endif

