/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Subroutines associated with the accumulate operation.
 *
 * Author: Ryan M. Olson
 * CVS $Id: ddi_acc.c,v 1.11 2006/11/28 15:53:10 andrey Exp $
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"


/* -------------------------------------------------------------- *\
   DDI_Acc(handle,ilo,ihi,jlo,jhi,buff)
   ====================================
   [IN] handle - Handle of the distributed array to be accessed.
   [IN] ilo    - Lower row bound of the patch of array handle.
   [IN] ihi    - Upper row bound of the patch of array handle.
   [IN] jlo    - Lower col bound of the patch of array handle.
   [IN] jhi    - Upper col bound of the patch of array handle.
   [IN] buff   - Data segment to be operated on.
\* -------------------------------------------------------------- */
   void DDI_Acc(int handle,int ilo,int ihi,int jlo,int jhi,void *buff) {
      DDI_Patch Patch;

      Patch.ilo = ilo;
      Patch.ihi = ihi;
      Patch.jlo = jlo;
      Patch.jhi = jhi;
      
      DDI_AccP(handle,&Patch,buff);      
   }
  
 
/* -------------------------------------------------------------- *\
   DDI_AccP(handle,patch,buff)
   ============================
   [IN] handle - Handle of the distributed array to be accessed.
   [IN] patch  - structure containing ilo, ihi, jlo, jhi, etc.
   [IN] buff   - Data segment to be operated on.
\* -------------------------------------------------------------- */
      static int test = 0;
      static double time_start = 0;
      static double twait = 0;

   void DDI_AccP(int handle,DDI_Patch *patch,void *buff) {
   
   /* --------------- *\
      Local Variables
   \* --------------- */
      char ack=57;
      int i,np,me,nn,my,remote_id,nsubp;
      int ranks[MAX_NODES];
      DDI_Patch subp[MAX_NODES];
      char *working_buffer = (char *) buff;
      double scale = 1.0;
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);

    # if defined DDI_LAPI
      DDI_Patch *local_patch = NULL;
      lapi_cntr_t cntr[MAX_NODES];
    # endif

    # if defined CRAY_MPI
      int ack = 0;
      int ireq = 0;
      char *local_buffer;
      DDI_Patch *local_patch;
      MPI_Request req_req[MAX_NODES];
      MPI_Request req_msg[MAX_NODES];
      MPI_Request req_blk[MAX_NODES];
    # endif

    # if defined(DDI_ONESIDED)
      int ireq = 0;
      int idesc;
      char *local_buffer;
      DDI_Patch *local_patch;
      static char bigbuff[DDI_MAX_REQUEST_SIZE];
      double t1,t2;
    # endif

      STD_DEBUG((stdout,"%s: Entering DDI_AccP.\n",DDI_Id()))

   /* -------------------- *\
      Process OR Node Rank
   \* -------------------- */
      DDI_NProc(&np,&me);
      DDI_NNode(&nn,&my);


   /* ------------------------------------- *\
      Ensure the patch has the correct info
   \* ------------------------------------- */
      patch->oper   = DDI_ACC;
      patch->handle = handle;


   /* ---------------------------------- *\
      Check calling arguments for errors
   \* ---------------------------------- */
    # if defined DDI_CHECK_ARGS
      if(handle < 0 || handle >= gv(ndda)) {
         fprintf(stdout,"%s: Invalid handle [%i] in DDI_Acc.\n",DDI_Id(),handle);
         Fatal_error(911);
      }
      
      if(patch->ilo > patch->ihi || patch->ilo < 0 || patch->ihi >= gv(nrow)[handle]) {
         fprintf(stdout,"%s: Invalid row dimensions during DDI_Acc => ilo=%i ihi=%i.\n",DDI_Id(),patch->ilo,patch->ihi);
         Fatal_error(911);
      }
      
      if(patch->jlo > patch->jhi || patch->jlo < 0 || patch->jhi >= gv(ncol)[handle]) {
         fprintf(stdout,"%s: Invalid colum dimensions during DDI_Acc => jlo=%i jhi=%i.\n",DDI_Id(),patch->jlo,patch->jhi);
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

#if defined DDI_ARMCI
      DDI_ARMCI_Acc(patch,&scale,buff);
      return;
#endif

   /* ------------------------------------------------------- *\
      Determine where the pieces of the requested patch exist
   \* ------------------------------------------------------- */
      DDI_Subpatch(handle,patch,&nsubp,ranks,subp);
      MAX_DEBUG((stdout,"%s: %i subpatches.\n",DDI_Id(),nsubp));

      if(test++ == 0) {
        time_start = clock_highres();
      }
/*
      if(comm->me == 0) {
         if(test % comm->np == 0) {
            double t = clock_highres();
            printf("%d %6.2lf\n",test,(t-time_start));
         }
      }
*/
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
         DDI_Send_request(&subp[i],&remote_id,&req_req[ireq]);
         MPI_Irecv(&ack,1,MPI_BYTE,remote_id,0,comm->world_comm,&req_blk[ireq]);
         MPI_Isend(working_buffer,subp[i].size,MPI_BYTE,remote_id,0,comm->world_comm,&req_msg[ireq]);
       # elif defined(DDI_ONESIDED)
         idesc = ireq;
         if(ireq >= DDI_MAX_DESCRIPTORS) {
            idesc = ireq % DDI_MAX_DESCRIPTORS;
            t1 = clock_highres();
            cpReqWait(&cos_req[idesc]);
            t2 = clock_highres();
            twait += (t2-t1);
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

      if(local_patch) DDI_Acc_local(local_patch,local_buffer);

      if(ireq) {
       # if defined(CRAY_MPI)
         MPI_Waitall(ireq,req_req,MPI_STATUSES_IGNORE);
         MPI_Waitall(ireq,req_msg,MPI_STATUSES_IGNORE);
         MPI_Waitall(ireq,req_blk,MPI_STATUSES_IGNORE);
       # elif defined(DDI_ONESIDED)
         t1 = clock_highres();
         for(i=0; i<DDI_MAX_DESCRIPTORS; i++) cpReqWait(&cos_req[i]);
         t2 = clock_highres();
         twait = (t2-t1);
       # else
       # error uhoh-acc
       # endif
      }
      return; 
    # endif
    

   /* ------------------------------------------------------------------- *\
      Send data requests for all non-local pieces of the requested patch.
      Operate immediately to Acc a local portion of the patch.
   \* ------------------------------------------------------------------- */
      for(i=0; i<nsubp; i++) {
         ULTRA_DEBUG((stdout,"%s: Accumulating subpatch %i.\n",DDI_Id(),i))

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
            Using LAPI, perform the local acc after all the data
            requests have been sent ==> maximize concurrency.
         \* ---------------------------------------------------- */
          # if defined DDI_LAPI
            local_patch = &subp[i];
            local_patch->cp_buffer_addr = working_buffer;
          # else
         /* --------------------------------------------- *\
            Otherwise, perform the local acc immediately.
         \* --------------------------------------------- */
            DDI_Acc_local(&subp[i],working_buffer);
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
      /*
       # if !defined DDI_LAPI
       # if defined USE_SYSV
         MAX_DEBUG((stdout,"%s: Receiving remote fence ACK.\n",DDI_Id()))
         DDI_Recv(&ack,1,remote_id);
       # endif
         MAX_DEBUG((stdout,"%s: Sending subpatch %i to %i.\n",DDI_Id(),i,remote_id))
         DDI_Send(working_buffer,subp[i].size,remote_id);
       # endif
      */
       # if !defined DDI_LAPI
         DDI_Acc_remote(working_buffer,&subp[i],remote_id);
       # endif

      
      /* ------------ *\
         Shift buffer 
      \* ------------ */
         working_buffer += subp[i].size;
      }

   /* ----------------------------------------------------------- *\
      Using LAPI, perform the local accumulate (if needed) as the
      remote processes are getting the data to accumulate on the
      target processes.  Then wait for all the data to be copied
      out of the buffer before returning.
   \* ----------------------------------------------------------- */
    # if defined DDI_LAPI

   /* ------------------------------------ *\
      Accumulating local patch (if exists)
   \* ------------------------------------ */
      if(local_patch) DDI_Acc_local(local_patch,local_patch->cp_buffer_addr);

   /* ---------------------------------------------------------- *\
      Wait for all remote LAPI_Gets to finish copying local data
   \* ---------------------------------------------------------- */
      for(i=0; i<nsubp; i++) {
         if(subp[i].cp_lapi_cntr) {
            ULTRA_DEBUG((stdout,"%s: Wait for subpatch %i to be copied.\n",DDI_Id(),i))
            LAPI_Waitcntr(gv(lapi_hnd),&cntr[i],2,NULL);
            ULTRA_DEBUG((stdout,"%s: Subpatch %i copy completed.\n",DDI_Id(),i))
         }
      }
    # endif

      MAX_DEBUG((stdout,"%s: Leaving DDI_AccP.\n",DDI_Id()))
      
   }


/* -------------------------------------------------------------- *\
   DDI_Acc_local(patch,buff)
   =========================
   [IN] patch  - structure containing ilo, ihi, jlo, jhi, etc.
   [IN] buff   - Data segment to be operated on.
   
   Accumulates the subpatch specified by patch and stored in buff
   into the share-memory segment(s) of the local node.
\* -------------------------------------------------------------- */
   void DDI_Acc_local(const DDI_Patch* patch,void *buff) {
   
   /* --------------- *\
      Local Variables
   \* --------------- */
      DDA_Index *Index = gv(dda_index);
      int i,j,nrows,ncols,start_row,start_col;
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

      MAX_DEBUG((stdout,"%s: Entering DDI_Acc_local.\n",DDI_Id()))

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
         for(i=0; i<size; i++) dda[i] += dloc[i];
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
            for(j=0; j<nrows; j++) dda[j] += dloc[j];
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

      MAX_DEBUG((stdout,"%s: Leaving DDI_Acc_local.\n",DDI_Id()))
   }

static double *tacc = NULL;

/* ----------------------------------------------------------------- *\
   DDI_Acc_server(patch,from)
   ==========================
   [IN] patch - structure containing ilo, ihi, jlo, jhi, etc.
   [IN] from  - rank of DDI process sending data to be accumulated.
   
   Used by the data server to accept incoming data and perform a
   local accumulate.  Note, the fence is raised to protect the array
   from local get/put operations until the accumulate has finished.
\* ----------------------------------------------------------------- */
   void DDI_Acc_server(const DDI_Patch *msg,int from) {
   
   /* --------------- *\
      Local Variables
   \* --------------- */
      char ack = 57;
      DDI_Patch patch;
      char *buffer = NULL;
      int j,p;
      int nr,nc,np;
      int minr,lftr;
      int minc,lftc;
      size_t msg_size_base,msg_size,msg_size_max,remaining = msg->size;
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);
  
      int itimer = 0;
      double t1,t2;

      if(tacc == NULL) {
         tacc = (double *) malloc(10*sizeof(double));
         for(j=0; j<10; j++) tacc[j] = 0.0;
      }


   /* -------------------------------------------------------------------- *\
      Raise protective fence.  This is necessary because a compute process
      can finish with the DDI_Acc subroutine before the remote data server
      has finished accumulating the patch.
   \* -------------------------------------------------------------------- */
    # if defined(DDI_ONESIDED)
      cos_desc_t resp_desc;
      cos_mdesc_t *resp_mdesc = &msg->tag.response_mdesc;
      uint64_t ack_addr = resp_mdesc->addr; 

      t1 = clock_highres(); 
      DDI_Fence_acquire(msg->handle);
      t2 = clock_highres(); 
      tacc[itimer++] += (t2-t1);

      t1 = clock_highres();
      dsResponseStart(resp_mdesc, &resp_desc);
      t2 = clock_highres();
      tacc[itimer++] += (t2-t1);
    # elif defined USE_SYSV
      DDI_Fence_acquire(msg->handle);
      Comm_send(&ack,1,from,comm);
    # endif

      patch.handle = msg->handle;
      patch.ilo = msg->ilo;
      patch.ihi = msg->ihi;
      patch.jlo = msg->jlo;
      patch.jhi = msg->jhi;
      
      if(msg->size > MAX_DS_MSG_SIZE) {
         nr = msg->ihi-msg->ilo+1;
         nc = msg->jhi-msg->jlo+1;

       # if defined(CRAY_MPI) || defined(DDI_ONESIDED)
         fprintf(stdout,"%s: large accs not supported.\n",DDI_Id());
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
               t1 = clock_highres();
               networkRead(buffer, msg_size, &resp_desc);
               dsResponseWait(&resp_desc);
               t2 = clock_highres();
               tacc[itimer++] += (t2-t1);
               resp_mdesc->addr += msg_size;
               remaining -= msg_size;
               t1 = clock_highres();
               DDI_Acc_local(&patch,buffer);
               t2 = clock_highres();
               tacc[itimer++] += (t2-t1);
             # else
               Comm_recv(buffer,msg_size,from,comm);
               DDI_Acc_local(&patch,buffer);
               remaining -= msg_size;
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
               networkRead(buffer, msg_size, &resp_desc);
               dsResponseWait(&resp_desc);
               resp_mdesc->addr += msg_size;
               remaining -= msg_size;
               DDI_Acc_local(&patch,buffer);
             # else
               Comm_recv(buffer,msg_size,from,comm);
               DDI_Acc_local(&patch,buffer);
               remaining -= msg_size;
               if(remaining) Comm_send(&ack,1,from,comm);
             # endif
            }

            DDI_Memory_pop(msg_size_max);

         }

      } else {

         DDI_Memory_push(msg->size,(void **)&buffer,NULL);

       # if defined(DDI_ONESIDED)
         nc = msg->jhi - msg->jlo + 1;
         nr = msg->ihi - msg->ilo + 1;

         char *req_buffer = (char *) msg;
         char *data_addr = (char *) msg;
         data_addr += sizeof(DDI_Patch);
   
      // avoid race condition on volatile request buffer
         memcpy(&patch, msg, sizeof(DDI_Patch));
         msg = &patch;
   
      // if(msg->size < (10*ONE_KB) || nc < 10) {
         if(1) {

         // pull data
            t1 = clock_highres();
            resp_desc.event_type = EVENT_LOCAL | EVENT_REMOTE;
            cosGet(buffer, msg->size, &resp_desc);
            dsResponseWait(&resp_desc);
            t2 = clock_highres();
            tacc[itimer++] += (t2-t1);
   
         // acc it
            t1 = clock_highres();
            DDI_Acc_local(msg,buffer);
            t2 = clock_highres();
            tacc[itimer++] += (t2-t1);
   
         // finish
            t1 = clock_highres();
            DDI_Memory_pop(msg->size);
            dsResponseFinish(&resp_desc);
            t2 = clock_highres();
            tacc[itimer++] += (t2-t1);

         } else {

            
        

         }
       # else
       # error DDI_ONESIDED not defined
       # endif
      }

   /* --------------- *\
      Take down fence
   \* --------------- */
      t1 = clock_highres();
      DDI_Fence_release(msg->handle);
      t2 = clock_highres();
      tacc[itimer++] += (t2-t1);
   }

   extern int cos_me;
   void ddi_acc_print_timings() {
   // if(tacc)
   // printf("%5d %10.4lf %10.4lf %10.4lf %10.4lf %10.4lf %10.4lf %10.4lf\n",cos_me,
   // tacc[0],tacc[1],tacc[2],tacc[3],tacc[4],tacc[5],tacc[6]);
      if(twait)
      printf("%06d [cp] twait=%10.3lf\n",cos_me,twait);
   }

   void DDI_Acc_remote(void *buff,DDI_Patch *patch,int remote_id) {
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
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);

   /* ------------------------------------------------------------- *\
    * ensure the write fence is up before sending data to be acc'ed
   \* ------------------------------------------------------------- */
    # if defined USE_SYSV
      Comm_recv(&ack,1,from,comm);
    # endif
      
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

            DEBUG_OUT(LVL6,(stdout,"%s: remote_acc broken down by rows.\n",DDI_Id()));

            np = 2;
            while( ((nr/np)+((nr%np)?1:0) ) > MAX_DS_MSG_WORDS ) np++;

            minr = nr/np;
            lftr = nr%np;
            
            DEBUG_OUT(LVL6,(stdout,"%s: remote_acc np=%i,minr=%i,lftr=%i.\n",DDI_Id(),np,minr,lftr));

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

            DEBUG_OUT(LVL6,(stdout,"%s: remote_acc broken down by cols.\n",DDI_Id()));
            DEBUG_OUT(LVL6,(stdout,"%s: remote_acc np=%i,minc=%i,lftc=%i.\n",DDI_Id(),np,minc,lftc));

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

         DEBUG_OUT(LVL6,(stdout,"%s: remote_acc - vanilla - from=%i; size=%li.\n",DDI_Id(),from,msg->size))
         Comm_send(buffer,msg->size,from,comm);

      }
   }

/* ------------------------------------------------------------------------ *\
   DDI_Acc_lapi_server(patch,buffer)
   =================================
   [IN] patch  - structure containing ilo, ihi, jlo, jhi, etc.
   [IN] buffer - Data segment to be operated on.
   
   Called by the LAPI target process whose handlers act like a data server.
   This subroutine gets data from the originating process (via LAPI_Get).
   Once the data has been transferred to the target process, a local acc is
   performed through DDI_Acc_local.
\* ------------------------------------------------------------------------ */
 # if defined DDI_LAPI
   void DDI_Acc_lapi_server(DDI_Patch *patch, void *buffer) {

   /* --------------- *\
      Local Variables 
   \* --------------- */
      lapi_handle_t hndl;
      uint tgt;
      ulong len;
      void *tgt_addr,*org_addr;
      lapi_cntr_t *tgt_cntr,*org_cntr;
      lapi_cntr_t local_cntr;

      STD_DEBUG((stdout,"%s: Entering DDI_Acc_lapi_server.\n",DDI_Id()))

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
                                                  
      ULTRA_DEBUG((stdout,"%s: DDI_Patch -> ilo=%i ihi=%i jlo=%i jhi=%i size=%lu.\n",
                   DDI_Id(),patch->ilo,patch->ihi,patch->jlo,patch->jhi,patch->size))
      ULTRA_DEBUG((stdout,"%s: org buffer_addr=%x cntr_addr=%x.\n",DDI_Id(),org_addr,org_cntr))
      ULTRA_DEBUG((stdout,"%s: tgt id=%i buffer_addr=%x cntr_addr=%x.\n",DDI_Id(),tgt,tgt_addr,tgt_cntr))


   /* -------------------------------------------------------------------------- *\
      We are interested in when the LAPI_Get is finished, zero the local counter
   \* -------------------------------------------------------------------------- */
      if(LAPI_Setcntr(hndl,org_cntr,0) != LAPI_SUCCESS) {
         fprintf(stdout,"%s: LAPI_Setcntr failed in DDI_Acc_lapi_server.\n",DDI_Id());
         Fatal_error(911);
      }
      ULTRA_DEBUG((stdout,"%s: Initializing local LAPI counter.\n",DDI_Id()))


   /* --------------------------------------------------- *\
      Execute the LAPI_Get.  This is a non-blocking call.
   \* --------------------------------------------------- */
      if(LAPI_Get(hndl,tgt,len,tgt_addr,org_addr,tgt_cntr,org_cntr) != LAPI_SUCCESS) {
         fprintf(stdout,"%s: LAPI_Get failed in DDI_Acc_lapi_server.\n",DDI_Id());
         Fatal_error(911);
      }
      MAX_DEBUG((stdout,"%s: Executing LAPI_Get from %i.\n",DDI_Id(),tgt))


   /* ------------------------------------------------------------------------ *\
      Wait here until the local counter is incremented ==> LAPI_Get completed.
   \* ------------------------------------------------------------------------ */
      if(LAPI_Waitcntr(hndl,org_cntr,1,NULL) != LAPI_SUCCESS) {
         fprintf(stdout,"%s: LAPI_Waitcntr failed in DDI_Acc_lapi_server.\n",DDI_Id());
         Fatal_error(911);
      }
      MAX_DEBUG((stdout,"%s: LAPI_Get from %i completed.\n",DDI_Id(),tgt))
      

   /* -------------------------------------------------------------- *\
      Place the data (now local) into the shared-memory of the node.
   \* -------------------------------------------------------------- */
      MAX_DEBUG((stdout,"%s: LAPI handler calling DDI_Acc_local.\n",DDI_Id()))
      DDI_Acc_local(patch,buffer);
      MAX_DEBUG((stdout,"%s: LAPI handler completed DDI_Acc_local.\n",DDI_Id()))
      STD_DEBUG((stdout,"%s: Exiting DDI_Acc_lapi_server.\n",DDI_Id()))
   }
 # endif
