/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Gracefully shuts down DDI by removing the System V semaphores,
 * collecting and printing coutner and timing information, and 
 * terminating the message passing library or libraries.
 * 
 * Author: Ryan M. Olson
 * changed 1/2005 by Dmitri Fedorov and Ikegami-san
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"

/* -------------------------------------------------------- *\
   DDI_Finalize()
   ==============
   
   Called to terminate the instance of DDI.
\* -------------------------------------------------------- */
   void DDI_Finalize() {

   /* --------------- *\
      Local Variables
   \* --------------- */
      char ack;
      int i,np,me,nn,my,nsocks,remote_id;
      int code;
      DDI_Patch msg;
      SMP_Data *smp_data = NULL;
      struct rusage mytiming;
      struct rusage *timings = NULL;
      struct timeval cpu_total;

      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_COMM_WORLD);

    # if defined DDI_COUNTERS
      size_t size = sizeof(DDI_Profile_counter);
      DDI_Profile_counter *profile_get,*profile_put,*profile_acc;
      DDI_Profile_counter  get_total,put_total,acc_total;
    # endif
      
      DEBUG_ROOT(LVL1,(stdout," DDI: Entering DDI_Finalize.\n"))
      DEBUG_OUT(LVL3,(stdout,"%s: Entering DDI_Finalize.\n",DDI_Id()))

      DDI_NProc(&np,&me);
      DDI_NNode(&nn,&my);
      remote_id = my;

      nsocks = np;
      if(USING_DATA_SERVERS()) nsocks += gv(nd);
      
   /* ----------------------------------------------- *\
      Inform the data server that we're closing shop.
   \* ----------------------------------------------- */
      if(me < np) {
         fflush(stdout);

         Comm_sync(3031,comm);

         if(USING_DATA_SERVERS()) {

          # ifdef CRAY_MPI
            if(me == 0)
            fprintf(stdout," test criteria = %i\n",gv(nd)/comm->nn);
            if(comm->me_local < gv(nd)/comm->nn) { 
         /* if(comm->me_local == 0) { */
          # endif

            STD_DEBUG((stdout,"%s: Sending DDI_QUIT signal to my data server.\n",DDI_Id()))
            msg.oper = DDI_QUIT;
            DDI_Send_request(&msg,&remote_id,NULL);
            DEBUG_OUT(LVL3,(stdout,"%s: waiting on ack byte.\n",DDI_Id()))
            Comm_recv(&ack,1,remote_id,comm);
            DEBUG_OUT(LVL3,(stdout,"%s: received ack byte.\n",DDI_Id()))

          # ifdef CRAY_MPI
            }
          # endif

            Comm_sync(3032,comm);
         } else {
           DDI_Memory_finalize();
         }

      /* ------------------- *\
         Remove all SysV IPC
      \* ------------------- */
       # if defined USE_SYSV
         DDI_Sem_remove(gv(dda_access));
         DDI_Sem_remove(gv(fence_access));
       # endif

       # if FULL_SMP
         DDI_Sem_remove(gv(dlb_access));
       # endif

      /* ---------------------------------------------------------- *\
         Clean up semaphores associated with shared-memory segments
      \* ---------------------------------------------------------- */
         DEBUG_OUT(LVL3,(stdout,"%s: cleaning up smp arrays.\n",DDI_Id()))
         while((smp_data=(SMP_Data *) SMP_find_end()) != NULL) {
            fprintf(stdout,"%s: Cleaning up leftover SMP Array %i.\n",DDI_Id(),smp_data->handle);
            SMP_destroy(smp_data->handle);
         }
         DEBUG_OUT(LVL3,(stdout,"%s: finished cleaning up smp arrays.\n",DDI_Id()))

      }


   /* ------------------------------------------------ *\
      Collect DDI Profiling information from all nodes
   \* ------------------------------------------------ */
      if(me == 0) {

       # if defined DDI_COUNTERS
         profile_get = (DDI_Profile_counter *) Malloc(np*size);
         profile_put = (DDI_Profile_counter *) Malloc(np*size);
         profile_acc = (DDI_Profile_counter *) Malloc(np*size);
         memcpy(profile_get,&gv(get_profile),size);
         memcpy(profile_put,&gv(put_profile),size);
         memcpy(profile_acc,&gv(acc_profile),size);
       # endif

         timings     = (struct rusage *) Malloc(nsocks*sizeof(struct rusage));
         getrusage(RUSAGE_SELF,timings);
      }


   /* ------------------------------------------ *\
      TCP/IP method for doing a gather operation
   \* ------------------------------------------ */
    # if defined DDI_SOC && !defined DDI_MPI
      if(me == 0) {
         for(i=1; i<np; i++) { 
           Comm_send(&ack,1,i,comm);
           Comm_recv(&timings[i],sizeof(struct rusage),i,comm);

         # if defined DDI_COUNTERS
           Comm_recv(&profile_get[i],size,i,comm);
           Comm_recv(&profile_put[i],size,i,comm);
           Comm_recv(&profile_acc[i],size,i,comm);
         # endif
         }

         if(USING_DATA_SERVERS()) for(i=np; i<2*np; i++) {
           Comm_send(&ack,1,i,comm); 
           Comm_recv(&timings[i],sizeof(struct rusage),i,comm);
         }

      } else {
         getrusage(RUSAGE_SELF,&mytiming);

         Comm_recv(&ack,1,0,comm);
         Comm_send(&mytiming,sizeof(struct rusage),0,comm);

         if(me < np) {
          # if defined DDI_COUNTERS
            Comm_send(&gv(get_profile),size,0,comm);
            Comm_send(&gv(put_profile),size,0,comm);
            Comm_send(&gv(acc_profile),size,0,comm);
          # endif
      }  }

    # else

   /* ------------------------ *\
      Otherwise use MPI_Gather
   \* ------------------------ */
      getrusage(RUSAGE_SELF,&mytiming);

      MPI_Gather(&mytiming,sizeof(struct rusage),MPI_BYTE,
                 timings,sizeof(struct rusage),MPI_BYTE,0,comm->world_comm);

    # if defined DDI_COUNTERS
      MPI_Gather(&gv(get_profile),size,MPI_BYTE,profile_get,size,MPI_BYTE,0,comm->compute_comm);
      MPI_Gather(&gv(put_profile),size,MPI_BYTE,profile_put,size,MPI_BYTE,0,comm->compute_comm);
      MPI_Gather(&gv(acc_profile),size,MPI_BYTE,profile_acc,size,MPI_BYTE,0,comm->compute_comm);
    # endif

    # endif


   /* ------------------------- *\
      Initialize final counters
   \* ------------------------- */
    # if defined DDI_COUNTERS
      get_total.ncalls = get_total.nbytes = 0;
      put_total.ncalls = put_total.nbytes = 0;
      acc_total.ncalls = acc_total.nbytes = 0;
      get_total.ncalls_local = get_total.nbytes_local = 0;
      put_total.ncalls_local = put_total.nbytes_local = 0;
      acc_total.ncalls_local = acc_total.nbytes_local = 0;

    # if defined SOMETHING_OR_OTHER
      get_total.tv_sec  = put_total.tv_sec  = acc_total.tv_sec  = 0;
      put_total.tv_usec = acc_total.tv_usec = 0;
    # endif


   /* ----------------- *\
      Sum over counters
   \* ----------------- */
      for(i=0; i<np; i++) {
         get_total.ncalls += profile_get[i].ncalls;
         get_total.nbytes += profile_get[i].nbytes;
         put_total.ncalls += profile_put[i].ncalls;
         put_total.nbytes += profile_put[i].nbytes;
         acc_total.ncalls += profile_acc[i].ncalls;
         acc_total.nbytes += profile_acc[i].nbytes;
         get_total.ncalls_local += profile_get[i].ncalls_local;
         get_total.nbytes_local += profile_get[i].nbytes_local;
         put_total.ncalls_local += profile_put[i].ncalls_local;
         put_total.nbytes_local += profile_put[i].nbytes_local;
         acc_total.ncalls_local += profile_acc[i].ncalls_local;
         acc_total.nbytes_local += profile_acc[i].nbytes_local;

       # if defined SOMETHING_OR_OTHER
         get_total.tv_sec  += profile_get[i].tv_sec;
         get_total.tv_usec += profile_get[i].tv_usec;
         if(get_total.tv_usec > 1000000) {
            get_total.tv_sec++;
            get_total.tv_usec -= 1000000;
         }
         put_total.tv_sec  += profile_put[i].tv_sec
       # endif

      } 
    # endif


   /* ------------------------------------------------------------ *\
      Inform the kickoff program that the DDI process has finished
   \* ------------------------------------------------------------ */
    # if defined DDI_SOC && !defined DDI_MPI
      send(gv(kickoffsock),&me,sizeof(int),0);
      recv(gv(kickoffsock),&ack,1,0);
    # endif

   /* ------------------------ *\
      Print out profiling info
   \* ------------------------ */
      if(me == 0) {
       # if defined DDI_COUNTERS
         fprintf(stdout,"\n ----------------------------------------");
         fprintf(stdout,"\n Distributed-Data Interface Statistics   ");
         fprintf(stdout,"\n ========================================");
         fprintf(stdout,"\n MEMDDI Used      = %s","TODO"); 
         fprintf(stdout,"\n MEMDDI Available = %s","TODO");
         fprintf(stdout,"\n ");
         fprintf(stdout,"\n DDI_Get");
         fprintf(stdout,"\n  NCalls = %li (%li)",get_total.ncalls,get_total.ncalls_local);
         fprintf(stdout,"\n  NBytes = %li (%li)",get_total.nbytes,get_total.nbytes_local);
         fprintf(stdout,"\n ");
         fprintf(stdout,"\n DDI_Put");
         fprintf(stdout,"\n  NCalls = %li (%li)",put_total.ncalls,put_total.ncalls_local);
         fprintf(stdout,"\n  NBytes = %li (%li)",put_total.nbytes,put_total.nbytes_local);
         fprintf(stdout,"\n ");
         fprintf(stdout,"\n DDI_Acc");
         fprintf(stdout,"\n  NCalls = %li (%li)",acc_total.ncalls,acc_total.ncalls_local);
         fprintf(stdout,"\n  NBytes = %li (%li)",acc_total.nbytes,acc_total.nbytes_local);
         fprintf(stdout,"\n ");
         fprintf(stdout,"\n DDI_BCast");
         fprintf(stdout,"\n  NCalls = %lu",gv(bcast_profile).ncalls);
         fprintf(stdout,"\n  NBytes = %lu",gv(bcast_profile).nbytes);
         fprintf(stdout,"\n ----------------------------------------\n");
         fflush(stdout);
       # endif

         fprintf(stdout,"\n ----------------------------------------");
         fprintf(stdout,"\n CPU timing information for all processes");
         fprintf(stdout,"\n ========================================");

         for(i=0; i<nsocks; i++) {
            cpu_total.tv_sec  = timings[i].ru_utime.tv_sec  + timings[i].ru_stime.tv_sec;
            cpu_total.tv_usec = timings[i].ru_utime.tv_usec + timings[i].ru_stime.tv_usec;
            if(cpu_total.tv_usec > 1000000) {
               cpu_total.tv_sec++;
               cpu_total.tv_usec -= 1000000;
            }

            fprintf(stdout,"\n %i: %d.%.6d + %d.%.6d = %d.%.6d",i,
               (int)timings[i].ru_utime.tv_sec,(int)timings[i].ru_utime.tv_usec,
               (int)timings[i].ru_stime.tv_sec,(int)timings[i].ru_stime.tv_usec,
               (int)cpu_total.tv_sec,(int)cpu_total.tv_usec);
         }

         fprintf(stdout,"\n ----------------------------------------\n");

      }


   /* ----------------- *\
      Flush the buffers
   \* ----------------- */
      fflush(stdout);
      fflush(stderr);

   /* ----------------- *\
      Return the memory
   \* ----------------- */
    # if defined DDI_COUNTERS
      free(profile_get);
      free(profile_put);
      free(profile_acc);
    # endif
      free(timings);
      if(gv(master_map)) free(gv(master_map));
      if(gv(global_node_map)) free(gv(global_node_map));
      if(gv(global_proc_map)) free(gv(global_proc_map));

    # if defined DDI_ARMCI
      DDI_ARMCI_Finalize();
    # endif
      
   /* ----------------------------------------- *\
      Finalize Closure with the kickoff program
   \* ----------------------------------------- */
    # if defined DDI_SOC && !defined DDI_MPI
      send(gv(kickoffsock),&ack,1,0);
    # endif

    # if defined DDI_SHMEM
  /*  shmem_finalize(); */
    # endif

    # ifdef DDI_ONESIDED
      extern void ddi_onesided_print_times();
      if(NUMA_MPI_GetSmpdRank(MPI_COMM_WORLD) == 0) {
         ddi_onesided_print_times();
      }
      COS_Finalize();
    # endif

    # if defined DDI_MPI
      MPI_Barrier(MPI_COMM_WORLD);
   /* ------------------------------------------------- *\
      Free the global variables initialized in Init_mpi
   \* ------------------------------------------------- */
      free(np_by_node);
      free(nc_by_node);
      free(nd_by_node);
      free(ranks_by_node[0]); /* this is the ranks array from Init_mpi */
      free(ranks_by_node);
   
      MPI_Finalize();
    # endif
    
    # if defined DDI_LAPI
      LAPI_Gfence(gv(lapi_hnd));
      LAPI_Term(gv(lapi_hnd));
    # endif

   }
