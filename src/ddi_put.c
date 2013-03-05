/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Subroutines associated with the distributed-memory put operation.
 *
 * Author: Ryan M. Olson
 * CVS $Id: ddi_put.c,v 1.3 2006/06/15 18:39:23 ryan Exp $
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"
 

/* -------------------------------------------------------------------- *\
   DDI_Put(handle,ilo,ihi,jlo,jhi,buff)
   ====================================
   [IN] handle - Handle of the distributed array to be accessed.
   [IN] ilo    - Lower row bound of the patch of array handle.
   [IN] ihi    - Upper row bound of the patch of array handle.
   [IN] jlo    - Lower col bound of the patch of array handle.
   [IN] jhi    - Upper col bound of the patch of array handle.
   [IN] buff   - Data segment to be operated on.
\* -------------------------------------------------------------------- */
   void DDI_Put(int handle,int ilo,int ihi,int jlo,int jhi,void *buff) {
      DDI_Patch Patch;
      
      Patch.ilo = ilo;
      Patch.ihi = ihi;
      Patch.jlo = jlo;
      Patch.jhi = jhi;
      
      DDI_PutP(handle,&Patch,buff);      
   }
  
 
/* -------------------------------------------------------------------- *\
   DDI_PutP(handle,patch,buff)
   ============================
   [IN] handle - Handle of the distributed array to be accessed.
   [IN] patch  - structure containing ilo, ihi, jlo, jhi, etc.
   [IN] buff   - Data segment to be operated on.
\* -------------------------------------------------------------------- */
   void DDI_PutP(int handle,DDI_Patch *patch,void *buff) {
      DDI_PutP_comm(handle,patch,buff,DDI_WORKING_COMM);
   }

   void DDI_PutP_comm(int handle,DDI_Patch *patch,void *buff,int commid) {
   
   /* --------------- *\
      Local Variables
   \* --------------- */
      char ack=57;
      int i,np,me,nn,my,remote_id,nsubp;
      int ranks[MAX_NODES];
      DDI_Patch subp[MAX_NODES];
      char *working_buffer = (char *) buff;
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(commid);

    # if defined DDI_LAPI
      DDI_Patch *local_patch = NULL;
      lapi_cntr_t cntr[MAX_NODES];
    # endif

    # if defined CRAY_MPI
      int ireq = 0;
      char *local_buffer;
      DDI_Patch *local_patch;
      MPI_Request req_req[MAX_NODES];
      MPI_Request req_msg[MAX_NODES];
      MPI_Request req_blk[MAX_NODES];
    # endif

    # if defined(DDI_ONESIDED)
      int ireq = 0;
      int idesc = 0;
      char *local_buffer;
      DDI_Patch *local_patch;
    # endif

      STD_DEBUG((stdout,"%s: Entering DDI_PutP.\n",DDI_Id()))

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
      patch->oper   = DDI_PUT;
      patch->handle = handle;


   /* ---------------------------------- *\
      Check calling arguments for errors
   \* ---------------------------------- */
    # if defined DDI_CHECK_ARGS
      if(handle < 0 || handle >= gv(ndda)) {
         fprintf(stdout,"%s: Invalid handle [%i] in DDI_Put.\n",DDI_Id(),handle);
         Fatal_error(911);
      }
      
      if(patch->ilo > patch->ihi || patch->ilo < 0 || patch->ihi >= gv(nrow)[handle]) {
         fprintf(stdout,"%s: Invalid row dimensions during DDI_Put => ilo=%i ihi=%i.\n",DDI_Id(),patch->ilo,patch->ihi);
         Fatal_error(911);
      }
      
      if(patch->jlo > patch->jhi || patch->jlo < 0 || patch->jhi >= gv(ncol)[handle]) {
         fprintf(stdout,"%s: Invalid colum dimensions during DDI_Put => jlo=%i jhi=%i.\n",DDI_Id(),patch->jlo,patch->jhi);
         Fatal_error(911);
      }
    # endif


   /* ------------------------------ *\
      Log some simple profiling info
   \* ------------------------------ */
    # if defined DDI_COUNTERS
      gv(put_profile).ncalls++;
      gv(put_profile).nbytes += DDI_Patch_sizeof(patch);
    # endif

#if defined DDI_ARMCI
      DDI_ARMCI_Put(patch,buff);
      return;
#endif

   /* ------------------------------------------------------- *\
      Determine where the pieces of the requested patch exist
   \* ------------------------------------------------------- */
      DDI_Subpatch_comm(handle,patch,&nsubp,ranks,subp,commid);
      MAX_DEBUG((stdout,"%s: %i subpatches.\n",DDI_Id(),nsubp))

    # if defined(CRAY_MPI) || defined(DDI_ONESIDED)
      local_patch = NULL;
      for(i=0, ireq=0; i<nsubp; i++) {
         if(ranks[i] == my) {
            local_patch  = &subp[i];
            local_buffer = working_buffer;
            working_buffer += subp[i].size;
            continue;
         }

         remote_id = ranks[i];

       # if defined(CRAY_MPI)
         DDI_Send_request_comm(&subp[i],&remote_id,&req_req[ireq],commid);
         MPI_Irecv(&ack,1,MPI_BYTE,remote_id,0,comm->world_comm,&req_blk[ireq]);
         MPI_Isend(working_buffer,subp[i].size,MPI_BYTE,remote_id,0,comm->world_comm,&req_msg[ireq]);
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
       # error uhoh-put
       # endif

         ireq++;
         ranks[i] = -1;
         working_buffer += subp[i].size;
      }

      if(local_patch) {
         DDI_Put_local(local_patch,local_buffer);
      }

      if(ireq) {
       # if defined(CRAY_MPI)
         MPI_Waitall(ireq,req_req,MPI_STATUSES_IGNORE);
         MPI_Waitall(ireq,req_msg,MPI_STATUSES_IGNORE);
         MPI_Waitall(ireq,req_blk,MPI_STATUSES_IGNORE);
       # elif defined(DDI_ONESIDED)
         for(i=0; i<DDI_MAX_DESCRIPTORS; i++) cpReqWait(&cos_req[i]);
       # else
       # error uhoh-put
       # endif
      }
      return;
    # endif



   /* ------------------------------------------------------------------- *\
      Send data requests for all non-local pieces of the requested patch.
      Local operations are implementation dependent.
   \* ------------------------------------------------------------------- */
      for(i=0; i<nsubp; i++) {
         ULTRA_DEBUG((stdout,"%s: Putting subpatch %i.\n",DDI_Id(),i))

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
            Using LAPI, perform the local put after all the data
            requests have been sent ==> maximize concurrency.
         \* ---------------------------------------------------- */
          # if defined DDI_LAPI
            local_patch = &subp[i];
            local_patch->cp_buffer_addr = working_buffer;
          # else
         /* --------------------------------------------- *\
            Otherwise, perform the local put immediately.
         \* --------------------------------------------- */
            DDI_Put_local(&subp[i],working_buffer);
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


      /* ------------------------------------------------- *\
         If LAPI is in use, include some extra information
      \* ------------------------------------------------- */
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
         MAX_DEBUG((stdout,"%s: sending data request to node %i.\n",DDI_Id(),remote_id))
         DDI_Send_request_comm(&subp[i],&remote_id,NULL,commid); 
         MAX_DEBUG((stdout,"%s: data request sent to global process %i.\n",DDI_Id(),remote_id))


      /* ---------------------------- *\
         Recv subpatch from remote_id
      \* ---------------------------- */
      /*
       # if !defined DDI_LAPI
         MAX_DEBUG((stdout,"%s: Sending subpatch %i to %i.\n",DDI_Id(),i,remote_id))
         DDI_Send(working_buffer,subp[i].size,remote_id);
         
       # if defined USE_SYSV
         MAX_DEBUG((stdout,"%s: Waiting for put ACK from %i.\n",DDI_Id(),remote_id))
         DDI_Recv(&ack,1,remote_id);
         MAX_DEBUG((stdout,"%s: Received put ACK from %i.\n",DDI_Id(),remote_id))
       # endif
       # endif
      */
       # if !defined DDI_LAPI
         DDI_Put_remote(working_buffer,&subp[i],remote_id,commid);
       # endif

      /* ------------ *\
         Shift buffer 
      \* ------------ */
         working_buffer += subp[i].size;
      }
      
      
   /* -------------------------------------------------------- *\
      Using LAPI, now perform the local put (if needed) as the
      remote process are performing a LAPI_Get of data out of
      my local buffer.  Then wait until all remote LAPI_Gets
      have completed.
   \* ----------------------------------------------------- */
    # if defined DDI_LAPI
    
   /* --------------------------- *\
      Put local patch (if exists)
   \* --------------------------- */
      if(local_patch) DDI_Put_local(local_patch,local_patch->cp_buffer_addr);
      
   /* ----------------------------------------- *\
      Wait for all remtoe LAPI_Gets to complete
   \* ----------------------------------------- */
      for(i=0; i<nsubp; i++) {
         if(subp[i].cp_lapi_cntr) {
            ULTRA_DEBUG((stdout,"%s: Wait for subpatch %i.\n",DDI_Id(),i))
            LAPI_Waitcntr(gv(lapi_hnd),&cntr[i],2,NULL);
            ULTRA_DEBUG((stdout,"%s: Subpatch %i completed.\n",DDI_Id(),i))
         }
      }
    # endif
    
      MAX_DEBUG((stdout,"%s: Leaving DDI_PutP.\n",DDI_Id()))
 
   }


/* ----------------------------------------------------------- *\
   DDI_Put_local(patch,buff)
   =========================
   [IN] patch  - structure containing ilo, ihi, jlo, jhi, etc.
   [IN] buff   - Data segment to be operated on.
   
   Puts the subpatch specified by patch and stored in buff
   into the share-memory segment(s) of the local node.
\* ----------------------------------------------------------- */
   void DDI_Put_local(const DDI_Patch* patch,void *buff) {
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

      MAX_DEBUG((stdout,"%s: Entering DDI_Put_local.\n",DDI_Id()))

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
         DDI_Acquire(Index,handle,DDI_WRITE_ACCESS,(void **) &dda);
         dda  += dda_offset;
         memcpy(dda,dloc,size*sizeof(double));
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
            memcpy(dda,dloc,size);
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
      gv(put_profile).ncalls_shmem++;
      gv(put_profile).nbytes_shmem += patch->size;
    # endif

      MAX_DEBUG((stdout,"%s: Leaving DDI_Put_local.\n",DDI_Id()))
   }



/* ----------------------------------------------------------------- *\
   DDI_Put_server(patch,from)
   ==========================
   [IN] patch - structure containing ilo, ihi, jlo, jhi, etc.
   [IN] from  - rank of DDI process sending data to be accumulated.
   
   Used by the data server to accept incoming data and perform a
   local put.
\* ---------------------------------------------------------------- */
   void DDI_Put_server(const DDI_Patch *msg,int from) {
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
      cos_desc_t get_desc, ack_desc;
      cos_mdesc_t *resp_mdesc = &msg->tag.response_mdesc;
//    dsResponseStart(resp_mdesc, &resp_desc);
      dsDescInit(resp_mdesc, &get_desc);
      dsDescInit(resp_mdesc, &ack_desc);
      uint64_t ack_addr = resp_mdesc->addr;
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

             # if defined(DDI_ONESIDED)
               resp_desc.event_type = EVENT_LOCAL;
               networkRead(buffer, msg_size, &resp_desc);
               dsResponseWait(&resp_desc);
               resp_mdesc->addr += msg_size;
               remaining -= msg_size;
               DDI_Put_local(&patch,buffer);
             # else
               Comm_recv(buffer,msg_size,from,comm);
               remaining -= msg_size;
               DDI_Put_local(&patch,buffer);
               if(remaining) Comm_send(&ack,1,from,comm);
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

             # if defined(DDI_ONESIDED)
               resp_desc.event_type = EVENT_LOCAL;
               networkRead(buffer, msg_size, &resp_desc);
               dsResponseWait(&resp_desc);
               resp_mdesc->addr += msg_size;
               remaining -= msg_size;
               DDI_Put_local(&patch,buffer);
             # else
               Comm_recv(buffer,msg_size,from,comm);
               DDI_Put_local(&patch,buffer);
               remaining -= msg_size;
               if(remaining) Comm_send(&ack,1,from,comm);
             # endif
            }

            DDI_Memory_pop(msg_size_max);
         }

      } else {

         DDI_Memory_push(msg->size,(void **)&buffer,NULL);
       # if defined(DDI_ONESIDED)
//       resp_desc.event_type = EVENT_LOCAL; 
//       cosGet(buffer, msg->size, &resp_desc);
//       dsResponseWait(&resp_desc);
         get_desc.event_type = EVENT_LOCAL;
         cosGet(buffer, msg->size, &get_desc);
         dsDescWait(&get_desc);
       # else
         Comm_recv(buffer,msg->size,from,comm);
       # endif
         DDI_Put_local(msg,buffer);
         DDI_Memory_pop(msg->size);

      }

    # if defined(DDI_ONESIDED)
//    resp_mdesc->addr = ack_addr;
//    resp_desc.event_type = EVENT_LOCAL | EVENT_REMOTE;
//    cosPut(NULL, 0, &resp_desc);
//    dsResponseFinish(&resp_desc);
      ack_desc.event_type = EVENT_LOCAL | EVENT_REMOTE;
      cosPut(NULL, 0, &ack_desc);
      dsDescWait(&ack_desc);
    # elif defined USE_SYSV
      Comm_send(&ack,1,from,comm);
    # endif

   }

   void DDI_Put_remote(void *buff,DDI_Patch *patch,int remote_id,int commid) {
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

         fprintf(stdout,"%s: nr=%i; nc=%i.\n",DDI_Id(),nr,nc);
      /* --------------------------------------------------- *\
         this message will be received in multiple segements
      \* --------------------------------------------------- */
         if(nr > MAX_DS_MSG_WORDS) {

            fprintf(stdout,"%s: remote_put broken down by rows.\n",DDI_Id());

            np = 2;
            while( ((nr/np)+((nr%np)?1:0) ) > MAX_DS_MSG_WORDS ) np++;

            minr = nr/np;
            lftr = nr%np;
            
            fprintf(stdout,"%s: remote_put np=%i,minr=%i,lftr=%i.\n",DDI_Id(),np,minr,lftr);

            msg_size_base = minr * sizeof(double);

            for(j=0; j<nc; j++) {
            for(p=0; p<np; p++) {
               msg_size = msg_size_base;
               if(p < lftr) msg_size += sizeof(double);

               Comm_send(buffer,msg_size,from,comm);

               remaining -= msg_size;
               buffer    += msg_size;

               if(remaining) Comm_recv(&ack,1,from,comm);
            }}

         } else {

            np = 2;
            while( nr*((nc/np)+((nc%np)?1:0)) > MAX_DS_MSG_WORDS ) np++;

            minc = nc/np;
            lftc = nc%np;
            msg_size_base = nr*minc* sizeof(double);

            fprintf(stdout,"%s: remote_put broken down by cols.\n",DDI_Id());
            fprintf(stdout,"%s: remote_put np=%i,minc=%i,lftc=%i.\n",DDI_Id(),np,minc,lftc);

            for(p=0; p<np; p++) {
               msg_size = msg_size_base;
               if(p < lftc) msg_size += nr*sizeof(double);

               Comm_send(buffer,msg_size,from,comm);

               remaining -= msg_size;
               buffer    += msg_size;
               
               if(remaining) Comm_recv(&ack,1,from,comm);
            }

         }

      } else {

         DEBUG_OUT(LVL6,(stdout,"%s: remote_put - vanilla - from=%i; size=%li.\n",DDI_Id(),from,msg->size))
         Comm_send(buffer,msg->size,from,comm);

      }

    # if defined USE_SYSV
      Comm_recv(&ack,1,from,comm);
    # endif

   }
/* ------------------------------------------------------------------------ *\
   DDI_Put_lapi_server(patch,buffer)
   =================================
   [IN] patch  - structure containing ilo, ihi, jlo, jhi, etc.
   [IN] buffer - Data segment to be operated on.
   
   Called by the LAPI target process whose handlers act like a data server.
   This subroutine gets data from the originating process (via LAPI_Get).
   Once the data has been transferred to the target process, a local put is
   performed through DDI_Put_local.
\* ------------------------------------------------------------------------ */
 # if defined DDI_LAPI
    void DDI_Put_lapi_server(DDI_Patch *patch, void *buffer) {

   /* --------------- *\
      Local Variables 
   \* --------------- */
      lapi_handle_t hndl;
      uint tgt;
      ulong len;
      void *tgt_addr,*org_addr;
      lapi_cntr_t *tgt_cntr,*org_cntr;
      lapi_cntr_t local_cntr;
      
      STD_DEBUG((stdout,"%s: Entering DDI_Put_lapi_server.\n",DDI_Id()))
      
   /* ----------------------------------------- *\
      Set up the calling arguments for LAPI_Get 
   \* ----------------------------------------- */
      hndl      = gv(lapi_hnd);          /* LAPI Handle */
      tgt       = patch->cp_lapi_id;     /* Target for the LAPI_Get */
      len       = (ulong) patch->size;   /* Amount of data to get */
      tgt_addr  = patch->cp_buffer_addr; /* Addr at target to get */
      org_addr  = buffer;                /* Local Addr for data that is got */
      tgt_cntr  = patch->cp_lapi_cntr;   /* Target counter */
      org_cntr  = &local_cntr;           /* Local counter -> incremented once
                                            the LAPI_Get is completed */
      
      ULTRA_DEBUG((stdout,"%s: DDI_Patch -> ilo=%i ihi=%i jlo=%i jhi=%i size=%lu.\n",
                   DDI_Id(),patch->ilo,patch->ihi,patch->jlo,patch->jhi,patch->size))
      ULTRA_DEBUG((stdout,"%s: org buffer_addr=%x cntr_addr=%x.\n",DDI_Id(),org_addr,org_cntr))
      ULTRA_DEBUG((stdout,"%s: tgt id=%i buffer_addr=%x cntr_addr=%x.\n",DDI_Id(),tgt,tgt_addr,tgt_cntr))
 
 
   /* -------------------------------------------------------------------------- *\
      We are interested in when the LAPI_Get is finished, zero the local counter
   \* -------------------------------------------------------------------------- */
      if(LAPI_Setcntr(hndl,org_cntr,0) != LAPI_SUCCESS) {
         fprintf(stdout,"%s: lapi_setcntr failed in DDI_Put_lapi_server.\n",DDI_Id());
         Fatal_error(911);
      }
      ULTRA_DEBUG((stdout,"%s: Initializing local LAPI counter.\n",DDI_Id()))


   /* --------------------------------------------------- *\
      Execute the LAPI_Get.  This is a non-blocking call.
   \* --------------------------------------------------- */
      if(LAPI_Get(hndl,tgt,len,tgt_addr,org_addr,tgt_cntr,org_cntr) != LAPI_SUCCESS) {
         fprintf(stdout,"%s: lapi_put failed in DDI_Put_lapi_server.\n",DDI_Id());
         Fatal_error(911);
      }
      MAX_DEBUG((stdout,"%s: Performing LAPI_Get from %i.\n",DDI_Id(),tgt))


   /* ------------------------------------------------------------------------ *\
      Wait here until the local counter is incremented ==> LAPI_Get completed.
   \* ------------------------------------------------------------------------ */
      if(LAPI_Waitcntr(hndl,org_cntr,1,NULL) != LAPI_SUCCESS) {
         fprintf(stdout,"%s: lapi_waitcntr failed in DDI_Put_lapi_server.\n",DDI_Id());
         Fatal_error(911);
      }
      MAX_DEBUG((stdout,"%s: LAPI_Put to %i is finished with local data.\n",DDI_Id(),tgt))


   /* -------------------------------------------------------------- *\
      Place the data (now local) into the shared-memory of the node.
   \* -------------------------------------------------------------- */
      MAX_DEBUG((stdout,"%s: LAPI handler calling DDI_Put_local.\n",DDI_Id()))
      DDI_Put_local(patch,buffer);
      MAX_DEBUG((stdout,"%s: LAPI handler finished with DDI_Put_local.\n",DDI_Id()))
      STD_DEBUG((stdout,"%s: Leaving DDI_Get_lapi_server.\n",DDI_Id()))      
   }
 # endif
