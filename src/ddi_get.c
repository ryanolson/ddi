/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Subroutines associated with the distributed-memory get operation.
 *
 * Author: Ryan M. Olson
 * CVS $Id: ddi_get.c,v 1.5 2006/06/15 18:39:23 ryan Exp $
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"


/* ------------------------------------------------------------------ *\
   DDI_Get(handle,ilo,ihi,jlo,jhi,buff)
   ====================================
   [IN]  handle - Handle of the distributed array to be accessed.
   [IN]  ilo    - Lower row bound of the patch of array handle.
   [IN]  ihi    - Upper row bound of the patch of array handle.
   [IN]  jlo    - Lower col bound of the patch of array handle.
   [IN]  jhi    - Upper col bound of the patch of array handle.
   [OUT] buff   - Destination buffer for patch.
\* ------------------------------------------------------------------ */
   void DDI_Get(int handle,int ilo,int ihi,int jlo,int jhi,void *buff) {
      DDI_Patch Patch;
      
      Patch.ilo = ilo;
      Patch.ihi = ihi;
      Patch.jlo = jlo;
      Patch.jhi = jhi;
      
      DDI_GetP(handle,&Patch,buff);      
   }
  
 
/* ------------------------------------------------------------------ *\
   DDI_GetP(handle,patch,buff)
   ===========================
   [IN]  handle - Handle of the distributed array to be accessed.
   [IN]  patch  - structure containing ilo, ihi, jlo, jhi, etc.
   [OUT] buff   - Destination buffer for patch.
\* ------------------------------------------------------------------ */
   void DDI_GetP(int handle,DDI_Patch *patch,void *buff) {
      DDI_GetP_comm(handle,patch,buff,DDI_WORKING_COMM);
   }

   void DDI_GetP_comm(int handle, DDI_Patch *patch, void *buff,int commid) {
  
   /* --------------- *\
      Local Variables
   \* --------------- */
      int i,np,me,nn,my,remote_id;
      int nsubp,ranks[MAX_NODES];
      DDI_Patch subp[MAX_NODES];
      char *working_buffer = (char *) buff;
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(commid);

    # if defined DDI_LAPI
      DDI_Patch *local_patch = NULL;
      lapi_cntr_t cntr[MAX_NODES];
    # endif

    # if defined CRAY_MPI
      int ireq = 0;
      DDI_Patch *local_patch;
      char *local_buffer;
      MPI_Request req_req[MAX_NODES];
      MPI_Request req_msg[MAX_NODES];
    # endif

    # if defined(DDI_ONESIDED)
      DDI_Patch *local_patch;
      char *local_buffer;
      int ireq = 0;
      int idesc;
    # endif

      STD_DEBUG((stdout,"%s: Entering DDI_GetP.\n",DDI_Id()))

   /* -------------------- *\
      Process OR Node Rank
   \* -------------------- */
      np = comm->np;
      me = comm->me;
      nn = comm->nn;
      my = comm->my;


   /* ------------------------------------- *\
      Ensure the patch has the correct info
   \* ------------------------------------- */
      patch->oper   = DDI_GET;
      patch->handle = handle;


   /* ---------------------------------- *\
      Check calling arguments for errors
   \* ---------------------------------- */
    # if defined DDI_CHECK_ARGS
      if(handle < 0 || handle >= gv(ndda)) {
         fprintf(stdout,"%s: Invalid handle [%i] in DDI_Get.\n",DDI_Id(),handle);
         Fatal_error(911);
      }
      
      if(patch->ilo > patch->ihi || patch->ilo < 0 || patch->ihi >= gv(nrow)[handle]) {
         fprintf(stdout,"%s: Invalid row dimensions during DDI_Get => ilo=%i ihi=%i.\n",DDI_Id(),patch->ilo,patch->ihi);
         Fatal_error(911);
      }
      
      if(patch->jlo > patch->jhi || patch->jlo < 0 || patch->jhi >= gv(ncol)[handle]) {
         fprintf(stdout,"%s: Invalid colum dimensions during DDI_Get => jlo=%i jhi=%i.\n",DDI_Id(),patch->jlo,patch->jhi);
         Fatal_error(911);
      }
    # endif


   /* ------------------------------ *\
      Log some simple profiling info
   \* ------------------------------ */
    # if defined DDI_COUNTERS
      gv(get_profile).ncalls++;
      gv(get_profile).nbytes += DDI_Patch_sizeof(patch);
    # endif

#if defined DDI_ARMCI
      DDI_ARMCI_Get(patch,buff);
      return;
#endif


   /* ------------------------------------------------------- *\
      Determine where the pieces of the requested patch exist
   \* ------------------------------------------------------- */
      DDI_Subpatch_comm(handle,patch,&nsubp,ranks,subp,commid);
      MAX_DEBUG((stdout,"%s: %i subpatches.\n",DDI_Id(),nsubp))

    # if defined(CRAY_MPI) || defined(DDI_ONESIDED)
      local_patch = NULL;
      for(i=0,ireq=0; i<nsubp; i++) {
         if(ranks[i] == my) {
            local_patch  = &subp[i];
            local_buffer = working_buffer;
            working_buffer += subp[i].size;
            continue;
         }

         remote_id = ranks[i];

       # if defined(CRAY_MPI)
         DDI_Send_request_comm(&subp[i],&remote_id,&req_req[ireq],commid);
         MPI_Irecv(working_buffer,subp[i].size,MPI_BYTE,remote_id,0,comm->world_comm,&req_msg[ireq]);
       # elif defined(DDI_ONESIDED)
         idesc = ireq;
         if(ireq >= DDI_MAX_DESCRIPTORS) {
            idesc = ireq % DDI_MAX_DESCRIPTORS;
            cpReqWait(&cos_req[idesc]);
         }
         cpReqInit(remote_id, &cos_req[idesc]);
         cpPrePostRecv(working_buffer, subp[i].size, &cos_req[idesc]);
         cpCopyLocalDataMDesc(&cos_req[idesc], &subp[i].tag.response_mdesc);
         cpReqSend(&subp[i], sizeof(DDI_Patch), &cos_req[idesc]);
       # else
       # error uhoh
       # endif

         ireq++; 
         ranks[i] = -1;
         working_buffer += subp[i].size;
      }

      if(local_patch) {
         DDI_Get_local(local_patch,local_buffer); 
      }

      if(ireq) {
       # if defined(CRAY_MPI)
         MPI_Waitall(ireq,req_req,MPI_STATUSES_IGNORE);
         MPI_Waitall(ireq,req_msg,MPI_STATUSES_IGNORE);
       # elif defined(DDI_ONESIDED)
         for(i=0; i<DDI_MAX_DESCRIPTORS; i++) cpReqWait(&cos_req[i]);
       # else
       # error uhoh
       # endif
      }
      return;
    # endif;



   /* ------------------------------------------------------------------- *\
      Send data requests for all non-local pieces of the requested patch.
      Local operations are implementation dependent.
   \* ------------------------------------------------------------------- */
      for(i=0; i<nsubp; i++) {
         ULTRA_DEBUG((stdout,"%s: Getting subpatch %i.\n",DDI_Id(),i))

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
            Using LAPI, perform the local get after all the data
            requests have been sent ==> maximize concurrency.
         \* ---------------------------------------------------- */
          # if defined DDI_LAPI
            local_patch = &subp[i];
            local_patch->cp_buffer_addr = working_buffer;
          # else
         /* --------------------------------------------- *\
            Otherwise, perform the local get immediately.
         \* --------------------------------------------- */
            DDI_Get_local(&subp[i],working_buffer);
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
         subp[i].cp_lapi_id     = gv(lapi_map)[me];        /* who i am in the lapi scheme      */
         subp[i].cp_lapi_cntr   = (void *) &cntr[i];       /* pointer to lapi counter          */
         subp[i].cp_buffer_addr = (void *) working_buffer; /* where to return the subpatch     */
         LAPI_Setcntr(gv(lapi_hnd),&cntr[i],0);            /* lapi counter - signal completion */
         
         ULTRA_DEBUG((stdout,"%s: cp_lapi_id=%i.\n",DDI_Id(),gv(lapi_map)[me]))
         ULTRA_DEBUG((stdout,"%s: cp_lapi_cntr=%x.\n",DDI_Id(),&cntr[i]))
         ULTRA_DEBUG((stdout,"%s: cp_buffer_addr=%x.\n",DDI_Id(),working_buffer))
       # endif

      
      /* -------------------------------- *\
         Send data request for subpatch i
      \* -------------------------------- */
         MAX_DEBUG((stdout,"%s: Sending data request to node %i.\n",DDI_Id(),remote_id))
         DDI_Send_request_comm(&subp[i],&remote_id,NULL,commid);
         MAX_DEBUG((stdout,"%s: data request sent to global process %i.\n",DDI_Id(),remote_id))


      /* ---------------------------------------------------------------------- *\
         Recv subpatch from remote_id, unless using LAPI.  LAPI takes advantage
         of direct memory access and will (via LAPI_Put) return the data to the
         compute process from the target DDI process.
      \* ---------------------------------------------------------------------- */
       # if !defined DDI_LAPI
       /*
         MAX_DEBUG((stdout,"%s: Receiving subpatch %i from %i.\n",DDI_Id(),i,remote_id))
         DDI_Recv(working_buffer,subp[i].size,remote_id,commid);
         MAX_DEBUG((stdout,"%s: Finished with DDI_Recv from %i.\n",DDI_Id(),remote_id))
       */
       # endif

      /* ---------------------------------------------------------------------- *\
       * we used to receive a single message from the data server, regardless
       * or the size of the patch.  this can lead to inconsistent values of
       * memddi needed, especially if the program is doing very LARGE get/put/
       * accumulates.  we now employ a buffered strategy.
      \* ---------------------------------------------------------------------- */
       # if !defined DDI_LAPI
         DDI_Get_remote(working_buffer,&subp[i],remote_id,commid);
       # endif

      /* ------------ *\
         Shift buffer 
      \* ------------ */
         working_buffer += subp[i].size;

         ULTRA_DEBUG((stdout,"%s: Finished getting subpatch %i.\n",DDI_Id(),i))
      }
      
      
   /* ------------------------------------------------------- *\
      Using LAPI, now perform the local get (if needed) as
      the remote target processes are returning the data from
      the local get's performed on the target processes, then
      wait until each remote LAPI_Put operation has completed
   \* ------------------------------------------------------- */
    # if defined DDI_LAPI

   /* --------------------------- *\
      Get local patch (if exists)
   \* --------------------------- */
      if(local_patch) DDI_Get_local(local_patch,local_patch->cp_buffer_addr);

   /* ----------------------------------------- *\
      Wait for all remote LAPI_Puts to complete
   \* ----------------------------------------- */
      for(i=0; i<nsubp; i++) {
         if(subp[i].cp_lapi_cntr) {      
            ULTRA_DEBUG((stdout,"%s: Wait for subpatch %i.\n",DDI_Id(),i))
            LAPI_Waitcntr(gv(lapi_hnd),&cntr[i],2,NULL);
            ULTRA_DEBUG((stdout,"%s: Subpatch %i completed.\n",DDI_Id(),i))
         }      
      }
    # endif

      MAX_DEBUG((stdout,"%s: Leaving DDI_GetP.\n",DDI_Id()))
   }


/* ------------------------------------------------------------------ *\
   DDI_GetP(handle,patch,buff)
   ===========================
   [IN]  handle - Handle of the distributed array to be accessed.
   [OUT] buff   - Destination buffer for patch.
   
   Gets and packs the subpatch from the shared-memory segment(s) on
   the local node and stores that data (stride 1) into buff.
\* ------------------------------------------------------------------ */
   void DDI_Get_local(const DDI_Patch* patch,void *buff) {
   
      DDA_Index *Index = gv(dda_index);
      int i,nrows,ncols,start_row,start_col;
      size_t dda_offset,size;
      double *dda,*dloc = (double *) buff;
      
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

      MAX_DEBUG((stdout,"%s: Enter DDI_Get_local.\n",DDI_Id()))

   /* -------------------------------------------------------------- *\
      When using shared-memory, ensure there are no requested writes
   \* -------------------------------------------------------------- */
    # if defined USE_SYSV 
      if(USING_DATA_SERVERS()) DDI_Fence_check(handle);
      MAX_DEBUG((stdout,"%s: Passed DDI_Fence_check.\n",DDI_Id()))
    # endif


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
         DDI_Acquire(Index,handle,DDI_READ_ACCESS,(void **) &dda);
         dda  += dda_offset;
         memcpy(dloc,dda,size*sizeof(double));
         DDI_Release(Index,handle,DDI_READ_ACCESS);
         dloc += size;
      } else {
   /* ----------------------------------------------- *\
      Otherwise, pack the local patch into the buffer
   \* ----------------------------------------------- */
         DDI_Acquire(Index,handle,DDI_READ_ACCESS,(void **) &dda);
         dda_offset = start_col*trows;
         dda += dda_offset;
         dda += start_row;
         size = nrows*sizeof(double);
         for(i=0; i<ncols; i++) {
            memcpy(dloc,dda,size);
            dloc += nrows;
            dda  += trows;
         }
         DDI_Release(Index,handle,DDI_READ_ACCESS);
      }

    # if FULL_SMP
      } /* end for-loop on local cpus */
    # endif

    
   /* ---------------------- *\
      Shared-memory counters 
   \* ---------------------- */
    # if defined DDI_COUNTERS
      gv(get_profile).ncalls_shmem++;
      gv(get_profile).nbytes_shmem += patch->size;
    # endif

      MAX_DEBUG((stdout,"%s: Exit DDI_Get_local.\n",DDI_Id()))
   }


/* ------------------------------------------------------------------ *\
   DDI_Get_server(patch,from)
   ==========================
   [IN] patch - structure containing ilo, ihi, jlo, jhi, etc.
   [IN] from  - rank of DDI process sending data to be accumulated.
   
   Used by the data server to handle a get request.  First the data
   is gathered into a local buffer through a DDI_Get_local operation,
   then the data is transfered back to the requesting process 'from'.
\* ------------------------------------------------------------------ */
   void DDI_Get_server(const DDI_Patch *msg,int from) {
      char ack = 57;
      DDI_Patch patch;
      char *buffer = NULL;
      int j,p;
      int nr,nc,np;
      int minr,lftr;
      int minc,lftc;
      size_t msg_size_base,msg_size,msg_size_max,remaining = msg->size;
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);
     
      patch.handle = msg->handle;
      patch.ilo = msg->ilo;
      patch.ihi = msg->ihi;
      patch.jlo = msg->jlo;
      patch.jhi = msg->jhi;

    # if defined(DDI_ONESIDED)
      cos_desc_t resp_desc;
      cos_mdesc_t *resp_mdesc = &msg->tag.response_mdesc;
      dsResponseStart(resp_mdesc, &resp_desc);
    # endif

      if(msg->size > MAX_DS_MSG_SIZE) {
         nr = msg->ihi-msg->ilo+1;
         nc = msg->jhi-msg->jlo+1;

       # ifdef CRAY_MPI
         fprintf(stdout,"%s: large gets not supported.\n",DDI_Id());
         Fatal_error(911);
       # endif

         if(nr > MAX_DS_MSG_WORDS) {

            np = 2;
            while( ((nr/np)+((nr%np)?1:0) ) > MAX_DS_MSG_WORDS ) np++;

            minr = nr/np;
            lftr = nr%np;
            
            msg_size_base = minr * sizeof(double);
            msg_size_max  = msg_size_base;
            if(lftr) msg_size_max += sizeof(double);

            DDI_Memory_push(msg_size_max,(void **)&buffer,NULL);

            for(j=0; j<nc; j++) {
               patch.jlo = msg->jlo + j;
               patch.jhi = msg->jlo + j;
               patch.ilo = msg->ilo;
               patch.ihi = msg->ilo-1;
            for(p=0; p<np; p++) {
               patch.ilo = patch.ihi + 1;
               patch.ihi = patch.ilo + minr - 1;
               if(p < lftr) patch.ihi++;
               msg_size = msg_size_base;
               if(p < lftr) msg_size += sizeof(double);

               DDI_Get_local(&patch,buffer);

             # if defined(DDI_ONESIDED)
               resp_desc.event_type = EVENT_LOCAL | EVENT_REMOTE;
               cosPut(buffer, msg_size, &resp_desc);
               dsResponseWait(&resp_desc);
               resp_mdesc->addr += msg_size;
               remaining -= msg_size;
             # else
               Comm_send(buffer,msg_size,from,comm);
               remaining -= msg_size;
               if(remaining) Comm_recv(&ack,1,from,comm);
             # endif
             
            }}

            DDI_Memory_pop(msg_size_max);

         } else {

            np = 2;
            while( nr*((nc/np)+((nc%np)?1:0)) > MAX_DS_MSG_WORDS ) np++;

            minc = nc/np;
            lftc = nc%np;
            msg_size_base = nr*minc* sizeof(double);
            msg_size_max  = msg_size_base;
            if(lftc) msg_size_max += nr*sizeof(double);

            DDI_Memory_push(msg_size_max,(void **)&buffer,NULL);

            patch.ilo = msg->ilo;
            patch.ihi = msg->ihi;
            patch.jlo = msg->jlo;
            patch.jhi = patch.jlo - 1;
            for(p=0; p<np; p++) {
               patch.jlo = patch.jhi + 1;
               patch.jhi = patch.jlo + minc - 1;
               if(p < lftc) patch.jhi++;
               msg_size = msg_size_base;
               if(p < lftc) msg_size += nr*sizeof(double);

               DDI_Get_local(&patch,buffer);
             # if defined(DDI_ONESIDED)
               resp_desc.event_type = EVENT_LOCAL;
               cosPut(buffer, msg_size, &resp_desc);
               dsResponseWait(&resp_desc);
               resp_mdesc->addr += msg_size;
               remaining -= msg_size;
             # else
               Comm_send(buffer,msg_size,from,comm);
               remaining -= msg_size;
               if(remaining) Comm_recv(&ack,1,from,comm);
             # endif
            }

            DDI_Memory_pop(msg_size_max);

         }

      } else {

         DDI_Memory_push(msg->size,(void **)&buffer,NULL);
         DDI_Get_local(msg,buffer);
       # if defined(DDI_ONESIDED)
         resp_desc.event_type = EVENT_LOCAL | EVENT_REMOTE;
         cosPut(buffer, msg->size, &resp_desc);
         dsResponseWait(&resp_desc);
       # else
         Comm_send(buffer,msg->size,from,comm);
       # endif
         DDI_Memory_pop(msg->size);

      }

    # if defined(DDI_ONESIDED)
      dsResponseFinish(&resp_desc);
    # endif
   }



   void DDI_Get_remote(void *buff,DDI_Patch *patch,int remote_id,int commid) {

      char ack = 39;
      const DDI_Patch *msg = (const DDI_Patch *) patch;
      char *buffer = (char *) buff;
      int j,p;
      int nr,nc,np;
      int minr,lftr;
      int minc,lftc;
      int from = remote_id;
      size_t msg_size,msg_size_base;
      size_t remaining = msg->size;
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(commid);

   /* ----------------------------------------------------------------------- *\
      large message can not be fully buffed on the data server due to memory
      limitations, therefore, large message from the data server must be 
      split into smaller pieces that can be easily buffered and sent.
   \* ----------------------------------------------------------------------- */
      if(msg->size > MAX_DS_MSG_SIZE) {
         nr = msg->ihi-msg->ilo+1;
         nc = msg->jhi-msg->jlo+1;

         DEBUG_OUT(LVL6,(stdout,"%s: nr=%i; nc=%i.\n",DDI_Id(),nr,nc));
      /* --------------------------------------------------- *\
         this message will be received in multiple segements
      \* --------------------------------------------------- */
         if(nr > MAX_DS_MSG_WORDS) {

            DEBUG_OUT(LVL6,(stdout,"%s: remote_get broken down by rows.\n",DDI_Id()));

            np = 2;
            while( ((nr/np)+((nr%np)?1:0) ) > MAX_DS_MSG_WORDS ) np++;

            minr = nr/np;
            lftr = nr%np;
            
            DEBUG_OUT(LVL6,(stdout,"%s: remote_get np=%i,minr=%i,lftr=%i.\n",DDI_Id(),np,minr,lftr));

            msg_size_base = minr * sizeof(double);

            for(j=0; j<nc; j++) {
            for(p=0; p<np; p++) {
               msg_size = msg_size_base;
               if(p < lftr) msg_size += sizeof(double);

               Comm_recv(buffer,msg_size,from,comm);

               remaining -= msg_size;
               buffer    += msg_size;

               if(remaining) Comm_send(&ack,1,from,comm);
            }}

         } else {

            np = 2;
            while( nr*((nc/np)+((nc%np)?1:0)) > MAX_DS_MSG_WORDS ) np++;

            minc = nc/np;
            lftc = nc%np;
            msg_size_base = nr*minc* sizeof(double);

            DEBUG_OUT(LVL6,(stdout,"%s: remote_get broken down by cols.\n",DDI_Id()));
            DEBUG_OUT(LVL6,(stdout,"%s: remote_get np=%i,minc=%i,lftc=%i.\n",DDI_Id(),np,minc,lftc));

            for(p=0; p<np; p++) {
               msg_size = msg_size_base;
               if(p < lftc) msg_size += nr*sizeof(double);

               Comm_recv(buffer,msg_size,from,comm);

               remaining -= msg_size;
               buffer    += msg_size;
               
               if(remaining) Comm_send(&ack,1,from,comm);
            }

         }

      } else {

         DEBUG_OUT(LVL6,(stdout,"%s: remote_get - vanilla - from=%i; size=%li.\n",DDI_Id(),from,msg->size))
         Comm_recv(buffer,msg->size,from,comm);

      }

   }

/* ------------------------------------------------------------------------ *\
   DDI_Get_lapi_server(patch,buffer)
   ==================================
   [IN]  patch  - structure containing ilo, ihi, jlo, jhi, etc.
   [OUT] buffer - Data segment to be operated on.
   
   Called by the LAPI target process whose handlers act like a data server.
   This subroutine performs a local get for the subpatch on local node, ie
   where the local node is the target of a LAPI active message.  This patch
   is stored locally in 'buffer' is then transferred back to the origin of
   the LAPI active message via a LAPI_Put operation.
\* ------------------------------------------------------------------------ */
 # if defined DDI_LAPI
   void DDI_Get_lapi_server(DDI_Patch *patch, void *buffer) {
   
   /* --------------- *\
      Local Variables
   \* --------------- */
      uint tgt;
      ulong len;
      void *tgt_addr,*org_addr;
      lapi_handle_t hndl;
      lapi_cntr_t *tgt_cntr,*org_cntr,*cmpl_cntr;
      lapi_cntr_t local_cntr;

      STD_DEBUG((stdout,"%s: Entering DDI_Get_lapi_server.\n",DDI_Id()))
 
   /* ------------------------------------------------------------- *\
      Retrieve the data from each shared memory segment on the node
   \* ------------------------------------------------------------- */
      MAX_DEBUG((stdout,"%s: LAPI handler calling DDI_Get_local.\n",DDI_Id()))
      DDI_Get_local(patch,buffer);
      MAX_DEBUG((stdout,"%s: LAPI handler finished with DDI_Get_local.\n",DDI_Id()))


   /* ------------------------------------- *\
      Set up calling arguments for LAPI_Put
   \* ------------------------------------- */
      hndl      = gv(lapi_hnd);
      tgt       = (uint)   patch->cp_lapi_id;
      len       = (ulong)  patch->size;
      tgt_addr  = (void *) patch->cp_buffer_addr;
      org_addr  = (void *) buffer;
      tgt_cntr  = (lapi_cntr_t *) patch->cp_lapi_cntr;
      org_cntr  = &local_cntr;
      cmpl_cntr = NULL;
      
      ULTRA_DEBUG((stdout,"%s: DDI_Patch -> ilo=%i ihi=%i jlo=%i jhi=%i size=%lu.\n",
                   DDI_Id(),patch->ilo,patch->ihi,patch->jlo,patch->jhi,patch->size))
      ULTRA_DEBUG((stdout,"%s: org buffer_addr=%x cntr_addr=%x.\n",DDI_Id(),org_addr,org_cntr))
      ULTRA_DEBUG((stdout,"%s: tgt id=%i buffer_addr=%x cntr_addr=%x.\n",DDI_Id(),tgt,tgt_addr,tgt_cntr))
 
      
   /* ----------------------------------------------------------------- *\
      We are interested in 1 of the 3 possible counters, i.e. org_cntr.
      Here we initialize org_cntr to zero.
   \* ----------------------------------------------------------------- */
      if(LAPI_Setcntr(hndl,&local_cntr,0) != LAPI_SUCCESS) {
         fprintf(stdout,"%s: LAPI_Setcntr failed in DDI_Get_lapi_server.\n",DDI_Id());
         Fatal_error(911);
      }
      ULTRA_DEBUG((stdout,"%s: Initializing local LAPI counter.\n",DDI_Id()))
      
      
   /* --------------------------------------------------- *\
      Execute the LAPI_Put.  This is a non-blocking call.
   \* --------------------------------------------------- */
      if(LAPI_Put(hndl,tgt,len,tgt_addr,org_addr,tgt_cntr,org_cntr,cmpl_cntr) != LAPI_SUCCESS) {
         fprintf(stdout,"%s: LAPI_Put failed in DDI_Get_lapi_server.\n",DDI_Id());
         Fatal_error(911);
      }
      MAX_DEBUG((stdout,"%s: Performing LAPI_Put to %i.\n",DDI_Id(),tgt))
      
      
   /* ---------------------------------------------------------------------- *\
      We do not have to wait for the LAPI_Put to complete on the target end,
      we only need to wait until all the data has been copied from the local
      buffer, thus we wait on 'org_cntr' and not 'cmpl_cntr'.
   \* ---------------------------------------------------------------------- */
      if(LAPI_Waitcntr(hndl,&local_cntr,1,NULL) != LAPI_SUCCESS) {
         fprintf(stdout,"%s: LAPI_Waitcntr failed in DDI_Get_lapi_server.\n",DDI_Id());
         Fatal_error(911);
      }
      MAX_DEBUG((stdout,"%s: LAPI_Put to %i is finished with local data.\n",DDI_Id(),tgt))
      STD_DEBUG((stdout,"%s: Leaving DDI_Get_lapi_server.\n",DDI_Id()))
   }
 # endif
