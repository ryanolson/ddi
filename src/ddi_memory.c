/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Subroutines for handling distributed memory.
 *
 * Author: Ryan M. Olson
 * CVS $Id: ddi_memory.c,v 1.6 2006/11/28 06:20:12 andrey Exp $
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"

 # if defined(USE_SYSV)
   static void DDI_Memory_sysv(size_t);
 # if defined(CRAY_MPI)
   static void DDI_Memory_sysv_cray(size_t);
 # endif
 # endif

   static void ddi_initialize_dda_index(DDA_Index*);

/* -------------------------------------------------------------------- *\
   DDI_Memory(size)
   ================
   [IN] size - total amount of aggregate distributed-memory to be used.
   
   Used to initialize memory for distributed storage.
\* -------------------------------------------------------------------- */
   void DDI_Memory(size_t size) {
   
   /* --------------- *\
      Local Variables
   \* --------------- */
     int code;
      char ack=57;
      int np,me,nn,my,remote_id;
      size_t mempp;
      long   mempp_l;
      double mempp_d;
      DDI_Patch msg;
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_COMM_WORLD);
 
   /* --------------- *\
      Get started ...
   \* --------------- */
      DDI_NProc(&np,&me);
      DDI_NNode(&nn,&my);

      DEBUG_ROOT(LVL1,(stdout," DDI: Enter DDI_Memory.\n"))
      DEBUG_OUT(LVL3,(stdout,"%s: Entering DDI_Memory.\n",DDI_Id()))

    # if defined DDI_CHECK_ARGS
      if(DDI_WORKING_COMM != DDI_COMM_WORLD) {
         if(me == 0) fprintf(stdout," DDI: Memory must be initialized in DDI_COMM_WORLD.\n");
         Fatal_error(911);
      }
    # endif

   /* ---------------------- *\
    * Sync compute processes
    * ---------------------- */
      Comm_sync(3071,comm);

   /* ------------------------------------------------------------------------------------------- *\
      Divide total aggregate memory amoungst DDI compute processes on double precision boundaries
   \* ------------------------------------------------------------------------------------------- */
      if(size) {
         mempp_d  = (double) size;
         mempp_d *= (double) sizeof(double);
         mempp_d *= (double) 1000000;
         mempp_d /= (double) np;
      } else {
         mempp_d  = MAX_DD_ARRAYS*sizeof(DDA_Index);
         mempp_d += MAX_DD_ARRAYS*sizeof(int);
         mempp_d += 5*sizeof(size_t);
         mempp_d += 2*CACHE_LINE_SIZE;
      /* mempp_d  = 131072; */
      }
 
    # if defined _32BIT
      if(mempp_d >= 2147483648) {
         if(me == 0) {
            fprintf(stdout,"\n");
            fprintf(stdout,"DDI wants more than 2GB of memory per CPU,\n");
            fprintf(stdout,"but you are running on a 32 bit system.\n\n");
            fprintf(stdout,"This job will require a 64 bit address space,\n");
            fprintf(stdout,"or a larger number of processors so that\n");
            fprintf(stdout,"MEMDDI/p falls below 2 GB per processor.\n");
            fprintf(stdout,"\n");
         }
         Fatal_error(911);
      }
    # endif


   /* ------------------------------------------------------------------------------- *\
      The following bit of code fixes a compiler error on IBM64 under AIX 4.3.3 using
      xlc (C for AIX 4.4).  size_t = double fails, yet size_t = long = double works.
   \* ------------------------------------------------------------------------------- */
      mempp_l = (long)   mempp_d;
      mempp   = (size_t) mempp_l;
      mempp  += (mempp % sizeof(double));
      mempp  += MAX_DS_MSG_SIZE;
      DEBUG_ROOT(LVL1,(stdout," DDI: Requesting %lu bytes of memory per compute process.\n",mempp));

   /* ----------------------- *\
      Send DDI_MEMORY request
   \* ----------------------- */
      remote_id = my;
      msg.oper  = DDI_MEMORY;
      msg.size  = mempp;

      if(USING_DATA_SERVERS()) {
       # ifdef CRAY_MPI
         if(comm->me_local < gv(nd)/comm->nn) {
         msg.size  = MAX_DS_MSG_SIZE + MAX_DD_ARRAYS*(sizeof(int)+sizeof(DDA_Index)) + 5*sizeof(size_t);
         msg.size += 2*CACHE_LINE_SIZE;
       # endif
         DEBUG_OUT(LVL3,(stdout,"%s: Sending DDI_MEMORY request to data server on node %i.\n",DDI_Id(),remote_id))
         DDI_Send_request(&msg,&remote_id,NULL);
       # ifdef CRAY_MPI
         }
       # endif
      }
 
 
   /* ---------------------------------------------------------- *\
      Compute process initialize memory when using shared-memory
   \* ---------------------------------------------------------- */
    # if defined USE_SYSV || defined DDI_ARMCI
      DDI_Memory_init(mempp);
    # endif

      
   /* -------------------------- *\
      Synchronize w/ Data Server
   \* -------------------------- */
      if(USING_DATA_SERVERS()) {
       # ifdef CRAY_MPI
         if(comm->me_local < gv(nd)/comm->nn) {
       # endif
         DEBUG_OUT(LVL4,(stdout,"%s: CP waiting for ACK from DS.\n",DDI_Id()))
         Comm_recv(&ack,1,remote_id,comm);
         DEBUG_OUT(LVL4,(stdout,"%s: CP received ACK from DS.  Memory initialized.\n",DDI_Id()))
       # ifdef CRAY_MPI
         }
       # endif
      } 
 
   /* ---------------------------- *\
      Synchronize Compute Processes
   \* ---------------------------- */
      Comm_sync(3072,comm);
 
      DEBUG_ROOT(LVL1,(stdout," DDI: Distributed Memory Initialized.\n",DDI_Id()))

    # if defined(DDI_ONESIDED)
   // NUMA_MPI_MemorySnapshot(MPI_COMM_WORLD);
    # endif
    }
 

/* -------------------------------------------------------------------- *\
   DDI_Memory_init(size)
   =====================
   [IN] size - amount of local memory to be initialized.
               
   Initializes local memory segment for distributed-memory storage
\* -------------------------------------------------------------------- */
   void DDI_Memory_init(size_t size) {
   
   /* --------------- *\
      Local Variables
   \* --------------- */
      char *ptr = NULL;
      size_t nbytes,offset;
      
   /* -------------------------------------- *\
      Amount of memory reserved for indexing
   \* -------------------------------------- */
      nbytes  = MAX_DD_ARRAYS*sizeof(DDA_Index);
      nbytes += MAX_DD_ARRAYS*sizeof(int);
      nbytes += 5*sizeof(size_t);
      nbytes += CACHE_LINE_SIZE;
      nbytes += (nbytes % sizeof(double));

      DEBUG_OUT(LVL2,(stdout,"%s: Enter DDI_Memory_init.\n",DDI_Id()));
      DEBUG_OUT(LVL3,(stdout,"%s: Requesting %lu bytes; %lu bytes for indexing.\n",DDI_Id(),size,nbytes));

   /* ----------------------------------------------------------- *\
      Allocate ARMCI RMA memory,
      or allocate System V shared-memory segments and semaphores,
      or get the memory via a malloc call.
   \* ----------------------------------------------------------- */
#if defined DDI_ARMCI
      DDI_ARMCI_Memory_init(size);
#elif defined USE_SYSV
      DDI_Memory_sysv(size);
#else
      gv(dda_index) = (DDA_Index *) Malloc(size);
#endif

   /* ------------------------------ *\
      initialize dda_index structure
   \* ------------------------------ */
      ddi_initialize_dda_index(gv(dda_index));

   /* ----------------------------------- *\
      Initialize distributed-memory stack
   \* ----------------------------------- */
      gv(mem_addr) = ptr = (char *) gv(dda_index); 

   /* ---------------------------------------------------- *\
      Initialize distributed-memory segment stack counters
   \* ---------------------------------------------------- */
      ptr += MAX_DD_ARRAYS*sizeof(DDA_Index);    
      ptr += MAX_DD_ARRAYS*sizeof(int);

      gv(mem_total)    = (size_t *) ptr;  ptr += sizeof(size_t);
      gv(mem_used)     = (size_t *) ptr;  ptr += sizeof(size_t);
      gv(mem_max)      = (size_t *) ptr;  ptr += sizeof(size_t);

#if defined DDI_ARMCI
      /* gv(dlb_counter) and gv(gdlb_counter) are initialized
         in DDI_ARMCI_Memory_init */
      *gv(mem_total) = size;
      *gv(mem_used)  = *gv(mem_max) = 0;
#else
      gv(dlb_counter)  = (size_t *) ptr;  ptr += sizeof(size_t);
      gv(gdlb_counter) = (size_t *) ptr;  ptr += sizeof(size_t);
      *gv(mem_total) = size;
      *gv(mem_used)  = *gv(mem_max) = 0;
      *gv(dlb_counter) = *gv(gdlb_counter) = 0;
#endif

   /* ------------------------------------ *\
      Initialize the fencing segment index
   \* ------------------------------------ */
    # if defined USE_SYSV
      DDI_Fence_init();
    # endif

   /* ------------------------------------- *\
      Reserve the first nbytes for indexing
   \* ------------------------------------- */
      DDI_Memory_push(nbytes,NULL,NULL);
      DEBUG_OUT(LVL3,(stdout,"%s: DDI_Memory_init completed.\n",DDI_Id()))
   }
  


/* -------------------------------------------------------------------- *\
   DDI_Memory_finalize()
   =====================
   
   Called by DDI_Finalize to ensure all the memory has been properly
   released.  This subroutine will also warn the user of possible
   memory leaks.
\* -------------------------------------------------------------------- */
   void DDI_Memory_finalize() {
   
   /* --------------- *\
      Local Variables
   \* --------------- */
      int me,np,mwords;
      double mbytes;
      size_t nbytes;
      int code;

      DDI_NProc(&np,&me);

      
   /* --------------------------------------------- *\
      If distributed memory was not used, then exit
   \* --------------------------------------------- */
      if(gv(mem_addr) == NULL) return;

#if defined DDI_ARMCI
      code = ARMCI_Free(gv(mem_addr));
      if (code > 0) printf("ARMCI_Free(gv(mem_addr)) failed\n");
#endif


   /* --------------------------------------------- *\
      Deallocate memory used for DDI array indexing
   \* --------------------------------------------- */
      nbytes  = MAX_DD_ARRAYS*sizeof(DDA_Index);
      nbytes += MAX_DD_ARRAYS*sizeof(int);
      nbytes += 5*sizeof(size_t);
      nbytes += CACHE_LINE_SIZE;
      nbytes += (nbytes % sizeof(double));

      *gv(mem_used) -= nbytes;

      if((me==np || me==0) && gv(mem_total)) {
         if(*gv(mem_used) != 0) {
            fprintf(stdout,"\n");
            fprintf(stdout,"\n");
            fprintf(stdout," DDI Warning:  Memory leak(s) detected.\n");
            fprintf(stdout," %lu bytes remain on the memory stack.\n",*gv(mem_used));
            fprintf(stdout,"\n");
            fprintf(stdout,"\n");
         }
         mbytes = ((double) *gv(mem_max))/(1024*1024);
         mwords = (int) mbytes/8;
         fprintf(stdout," DDI: %lu bytes (%.1lf MB / %i MWords) used by master data server.\n",(unsigned long) *gv(mem_max),mbytes,mwords);

       # if defined DDI_LAPI && defined TRACK_HEAP
         mbytes = ((double) gv(mem_heap_max))/(1024*1024);
         fprintf(stdout," DDI: Max Malloc = %.1lf MB.\n",mbytes);
       # endif

         fflush(stdout);
      }
   } 


/* -------------------------------------------------------------------- *\
   DDI_Memory_server()
   ===================
   
   Used by the data server to initialize distributed-memory.
\* -------------------------------------------------------------------- */
   void DDI_Memory_server(size_t size) {

   /* -------------------------------- *\
      If using System V shared-memory.
   \* -------------------------------- */
    # if defined USE_SYSV
      char ack=57;
      void *addr = NULL;
      int shmid,fence_access,dlb_access;
      int i,rank,np,me,smp_np,smp_me;
      char *ptr = NULL;
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_COMM_WORLD);

    # ifdef CRAY_MPI
      int *shmid_all;
    # endif

      np = comm->np;
      me = comm->me;
      smp_np = comm->np_local;
      smp_me = comm->me_local;


   /* --------------------------------------------------------------------------- *\
      One SysV SHMEM segment shared between a compute process and its data server
      SMP Machines are treated as multiple single process nodes.
   \* --------------------------------------------------------------------------- */
    # if !FULL_SMP
      rank = me-np;
      Comm_recv(&shmid,sizeof(int),rank,comm);
      Comm_recv(&fence_access,sizeof(int),rank,comm);

      DEBUG_OUT(LVL3,(stdout,"%s: received shmid=%i; fence_acces=%i from %i.\n",
                             DDI_Id(),shmid,fence_access,rank))
      addr = Shmat(shmid,0,0);

      Comm_send(&ack,1,rank,comm);

      gv(dda_index)    = (DDA_Index *) addr;
      gv(fence_access) = fence_access;
    # else
    # ifdef CRAY_MPI
      MPI_Barrier(comm->smp_world);
      shmid_all = (int *) Malloc(smp_np*sizeof(int));
      MPI_Barrier(comm->smp_world);
      MPI_Bcast(shmid_all,smp_np,MPI_INT,0,comm->smp_world);
      MPI_Bcast(&dlb_access,1,MPI_INT,0,comm->smp_world);
      MPI_Bcast(&fence_access,1,MPI_INT,0,comm->smp_world);
      for(i=0; i<smp_np; i++) {
        gv(smp_index)[i] = (DDA_Index *) Shmat(shmid_all[i],0,0);
      }
   /* ------------------------------------------------------------------------ *\
      This next line will break the general implentation using DDI_DS_PER_NODE,
      because multiple DS will use the same dda_index and hence initiate a 
      race condition on the buffer stack.  The fix to this problem is to Malloc
      separate buffer space on the data server and not used the shared-memory
      stack.  However, this also presents a problem for large messages, we need
      to come up with a better way to deal with very large get/put/acc. Ask Kim
      what the maximum number of outstanding MPI_Requests per process and per
      node are.  Are they tunable?
   \* ------------------------------------------------------------------------ */
      gv(dda_index)    = gv(smp_index)[comm->me_local];
      gv(ddi_sync)     = fence_access;
      gv(fence_access) = fence_access;
      gv(dlb_access)   = dlb_access;
      MPI_Barrier(comm->smp_world);
      free(shmid_all);
   /* ------------------------------------------------------------------------ *\
      Take action to prevent a runtime error in light of the above known issue
   \* ------------------------------------------------------------------------ */
/*
      if(gv(nd) > comm->nn) {
        if(comm->me == 0) {
          fprintf(stdout," ERROR: DDI does not support DDI_DS_PER_NODE > 1 at");
          fprintf(stdout," this time.  Please check your run scripts and\n");
          fprintf(stdout," make sure that DDI_DS_PER_NODE is either 0 or 1.\n\n");
          fflush(stdout);
          sleep(1);
        }
        DDI_Finalize();
      }
*/
    # else
      for(i=0,rank=me-np-smp_me; i<smp_np; i++,rank++) {
         Comm_recv(&shmid,sizeof(int),rank,comm);
         Comm_recv(&fence_access,sizeof(int),rank,comm);
         Comm_recv(&dlb_access,sizeof(int),rank,comm);
   
         DEBUG_OUT(LVL3,(stdout,"%s: received shmid=%i; fence_acces=%i; dlb_access=%i from %i.\n",
                                DDI_Id(),shmid,fence_access,dlb_access,rank))

      /* ------------------------------------------ *\
         Attach to shared-memory segment from irank
      \* ------------------------------------------ */
         addr = Shmat(shmid,0,0);
   
      /* ------------------------------------------------- *\
         Acknowledge successfully shared-memory attachment
      \* ------------------------------------------------- */
         Comm_send(&ack,1,rank,comm);
   
      /* --------------------------------------- *\
         Keep track of all shared-memory indexes
      \* --------------------------------------- */
         gv(smp_index)[i] = (DDA_Index *) addr;
   
         if(i == 0) {
            gv(fence_access) = fence_access;
            gv(dlb_access) = dlb_access;
         }
     
         if(i == smp_me) {
            gv(dda_index)  = (DDA_Index *) addr;
         }
      }
    # endif
    # endif

   /* ------------------------------------------------------------- *\
      Initialize addresses for data objects stored in shared-memory
   \* ------------------------------------------------------------- */
      gv(mem_addr) = ptr = (char *) gv(dda_index);

      ptr += MAX_DD_ARRAYS*sizeof(DDA_Index);
      ptr += MAX_DD_ARRAYS*sizeof(int);

      gv(mem_total)    = (size_t *) ptr;  ptr += sizeof(size_t);
      gv(mem_used)     = (size_t *) ptr;  ptr += sizeof(size_t);
      gv(mem_max)      = (size_t *) ptr;  ptr += sizeof(size_t);
      gv(dlb_counter)  = (size_t *) ptr;  ptr += sizeof(size_t);
      gv(gdlb_counter) = (size_t *) ptr;  ptr += sizeof(size_t);

    # ifdef CRAY_MPI
   /* ------------------------------------------------------------ *\
      If we decide to used our own Malloced buffer instead of the
      shared memory stack, we should initialize some of the memory
      management variables here.  Note: these would normally get
      set my the CP and would automatic be set on the DS because
      they are shared quantities.
   \* ------------------------------------------------------------ */
/*
      *gv(mem_total) = size;
      *gv(mem_used)  = *gv(mem_max) = 0;
*/
    # endif

   /* ---------------- *\
      Initialize Fence
   \* ---------------- */
      DDI_Fence_init();
      
      
   /* --------------------------------------------------- *\
      End of shared-memory initialize on the data server.
   \* --------------------------------------------------- */
   
    # else
   
   /* -------------------------------------------------------------- *\
      If not using System V shared-memory implies that only the data
      server can/will initialize memory for distributed data.  So do
      so now ...
   \* -------------------------------------------------------------- */
      DDI_Memory_init(size);
   
    # endif
    
      MAX_DEBUG((stdout,"%s: DDI_Memory_server completed.\n",DDI_Id()))
   }


/* -------------------------------------------------------------------- *\
   DDI_Memory_sysv(size)
   =====================
   [IN] size - amount of local shared-memory to be allocated.
   Initialize System V shared-memory and semaphores.
\* -------------------------------------------------------------------- */
 # if defined USE_SYSV
   static void DDI_Memory_sysv(size_t size) {
     char ack=57;
     int i,j,np,me,nn,my;
     int smp_np,smp_me;
     int irank,jrank,to;
     int semflg = 0600;
     int shmid,tshmid,dda_access,fence_access,dlb_access;
     const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_COMM_WORLD);

   # ifdef CRAY_MPI
     int *shmid_all;
   # endif

     np = comm->np;
     me = comm->me;
     nn = comm->nn;
     my = comm->my;
     smp_np = comm->np_local;
     smp_me = comm->me_local;

     DEBUG_OUT(LVL2,(stdout,"%s: Entering DDI_Memory_sysv.\n",DDI_Id()))
     Comm_sync(3073,comm);

  /* ------------------------------ *\
     Create a shared-memory segment
  \* ------------------------------ */
     gv(shmid) = shmid = Shmget(IPC_PRIVATE,size,SHM_R|SHM_W);
     gv(dda_index)     = Shmat(shmid,0,0);
     bzero((void*) gv(dda_index), size);

  /* ------------------------------------------- *\
     Create semaphores
     1) Access Semaphore -- 1 per handle per CPU
        + 1 extra for the shared-memory buffer.
  \* ------------------------------------------- */
     gv(dda_access) = dda_access = Semget(IPC_PRIVATE,MAX_DD_ARRAYS+1,semflg);

  /* ------------------------------------------- *\
     2) Fence semaphore -- 1 per handle per node
        + 1 extra for process synchronization
  \* ------------------------------------------- */
     if(smp_me == 0)
     gv(fence_access) = fence_access = Semget(IPC_PRIVATE,MAX_DD_ARRAYS+1,semflg);

  /* --------------------------------------------- *\
     3) Dynamic load-balance counter -- 1 per node
  \* --------------------------------------------- */
   # if FULL_SMP
     if(smp_me == 0)
     gv(dlb_access) = dlb_access = Semget(IPC_PRIVATE,1,semflg);

  /* ----------------------------------------------------- *\
     4) Specialized semaphores to control fences and syncs
  \* ----------------------------------------------------- */
     gv(ddi_buffer) = dda_access;
     gv(ddi_sync)   = fence_access;
   # endif

     DEBUG_OUT(LVL3,(stdout,"%s: shmid=%i; fence_acces=%i; dlb_access=%i.\n",
                            DDI_Id(),shmid,fence_access,dlb_access))
     DEBUG_OUT(LVL2,(stdout,"%s: Finished allocating SysV IPCs.\n",DDI_Id()))

  /* --------------------- *\
     Initialize semaphores
  \* --------------------- */
     for(i=0; i<=MAX_DD_ARRAYS; i++) {
        if(DDI_Sem_oper(dda_access,i,DDI_WRITE_ACCESS) == -1) {
           fprintf(stdout,"%s: DDI_Sem_oper failed for dda_access #%i.\n",DDI_Id(),i);
           Fatal_error(911);
        }
     }

     if(smp_me == 0) {
        for(i=0; i<MAX_DD_ARRAYS; i++) {
           if(DDI_Sem_oper(fence_access,i,DDI_WRITE_ACCESS) == -1) {
              fprintf(stdout,"%s: DDI_Sem_oper failed for fence_access $%i.\n",DDI_Id(),i);
              Fatal_error(911);
           }
        }

      # if FULL_SMP
        if(DDI_Sem_oper(gv(ddi_sync),MAX_DD_ARRAYS,smp_np) == -1 ) {
           fprintf(stdout,"%s: DDI_Sem_oper failed for ddi_sync #%i.\n",DDI_Id(),MAX_DD_ARRAYS);
           Fatal_error(911);
        }

        if(DDI_Sem_oper(gv(dlb_access),0,DDI_WRITE_ACCESS) == -1) {
           fprintf(stdout,"%s: DDI_Sem_oper failed for dlb_access #0.\n",DDI_Id());
           Fatal_error(911);
        }
      # endif
     }
     

  /* ---------------------------------------------------------------- *\
     Distributed System V IPC information to all intra-node processes
  \* ---------------------------------------------------------------- */
     DEBUG_OUT(LVL2,(stdout,"%s: distributed sysv ipc ids.\n",DDI_Id()))
   # if !FULL_SMP
     to = me+np;
     Comm_send(&shmid,sizeof(int),to,comm);
     Comm_send(&fence_access,sizeof(int),to,comm);
     Comm_recv(&ack,1,to,comm);
   # else

   # ifdef CRAY_MPI
     MPI_Barrier(comm->smp_world);
     shmid_all = (int *) Malloc(smp_np*sizeof(int));
     MPI_Gather(&shmid,1,MPI_INT,shmid_all,1,MPI_INT,0,comm->smp_comm);
     MPI_Barrier(comm->smp_world);
     MPI_Bcast(shmid_all,smp_np,MPI_INT,0,comm->smp_world);
     MPI_Bcast(&dlb_access,1,MPI_INT,0,comm->smp_world);
     MPI_Bcast(&fence_access,1,MPI_INT,0,comm->smp_world);
     for(i=0; i<smp_np; i++) {
        if(comm->me_local == i) {
           gv(smp_index)[i] = (DDA_Index *) gv(dda_index);
        } else {
           gv(smp_index)[i] = (DDA_Index *) Shmat(shmid_all[i],0,0);
        }
        gv(ddi_sync)     = fence_access;
        gv(fence_access) = fence_access;
        gv(dlb_access)   = dlb_access;
     }
     MPI_Barrier(comm->smp_world);
     free(shmid_all);
   # else
     for(i=0,irank=me-smp_me; i<smp_np; i++,irank++) {
        if(irank == me) {
           gv(smp_index)[i] = gv(dda_index);
           for(j=smp_np,jrank=me-smp_me; j--; jrank++) {

           /* --------------------------------------------- *\
              Send SysV IPC info to data server (if exists)
           \* --------------------------------------------- */
              if(USING_DATA_SERVERS()) {
                 to = jrank+np;
                 Comm_send(&shmid,sizeof(int),to,comm);
                 Comm_send(&fence_access,sizeof(int),to,comm);
                 Comm_send(&dlb_access,sizeof(int),to,comm);
                 Comm_recv(&ack,1,to,comm);
              }

           /* ------------------------------------------------------ *\
              Send SysV IPC info to compute process (that's not me!) 
           \* ------------------------------------------------------ */
              if(jrank == me) continue;
              Comm_send(&shmid,sizeof(int),jrank,comm);
              Comm_send(&fence_access,sizeof(int),jrank,comm);
              Comm_send(&dlb_access,sizeof(int),jrank,comm);
              Comm_recv(&ack,1,jrank,comm);
           }

        } else {

        /* ---------------------------- *\
           Receive SysV IPC information
        \* ---------------------------- */
           Comm_recv(&tshmid,sizeof(int),irank,comm);
           Comm_recv(&fence_access,sizeof(int),irank,comm);
           Comm_recv(&dlb_access,sizeof(int),irank,comm);


        /* --------------------------------------------------- *\
           Attach to shared memory segment associated w/ irank
        \* --------------------------------------------------- */
           gv(smp_index)[i] = (DDA_Index *) Shmat(tshmid,0,0);
           if(i == 0) {
              gv(ddi_sync)     = fence_access;
              gv(fence_access) = fence_access;
              gv(dlb_access)   = dlb_access;
           }

        /* -------------------------------- *\
           Synchronize with sending process
        \* -------------------------------- */
           Comm_send(&ack,1,irank,comm);

     }  }

     DEBUG_OUT(LVL3,(stdout,"%s: finished distributing sysv info.\n",DDI_Id()))
     
  /* ----------------------------------- *\
     Synchronize Local Compute Processes 
  \* ----------------------------------- */
     Comm_sync(3074,comm);

   # endif
   # endif

  /* ----------------------------------------------------------------------------- *\
     All local & remote DDI processes have attached to "my" shared memory segment.
     The shmid will now be removed from the system so that no other processes will
     be able to attach and so that the segment will be deleted once all DDI
     processes attached to the segment have terminated.
  \* ----------------------------------------------------------------------------- */
     Shmctl(shmid,IPC_RMID,NULL);
     gv(shmid) = 0; 

     DEBUG_OUT(LVL3,(stdout,"%s: Removing shmid=%i from the system\n",DDI_Id(),shmid))
   
  /* ------------------------------------------------------------------------- *\
     Note: Sadly System V semaphores can not be removed from the system in the
     same manner as SysV shared memory segments.  Luckily, the removal of SysV
     semaphores is less critical (w/ regard to resources) than shared memory
     segments. Semaphore removal is done in DDI_Finalize and also Fatal_error,
     in case of abnormal termination.
  \* ------------------------------------------------------------------------- */

   }
 
 # ifdef CRAY_MPI
   static void DDI_Memory_sysv_cray(size_t size) {
     char ack=57;
     int i,j,np,me,nn,my;
     int smp_np,smp_me;
     int irank,jrank,to;
     int semflg = 0600;
     int shmid,tshmid,dda_access,fence_access,dlb_access;
     const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_COMM_WORLD);

   # ifdef CRAY_MPI
     int *shmid_all;
   # endif

     np = comm->np;
     me = comm->me;
     nn = comm->nn;
     my = comm->my;
     smp_np = comm->np_local;
     smp_me = comm->me_local;

     DEBUG_OUT(LVL2,(stdout,"%s: Entering DDI_Memory_sysv.\n",DDI_Id()))
     Comm_sync(3073,comm);

  /* ------------------------------ *\
     Create a shared-memory segment
  \* ------------------------------ */
     gv(shmid) = shmid = Shmget(IPC_PRIVATE,size,SHM_R|SHM_W);
     gv(dda_index)     = Shmat(shmid,0,0);
     Shmctl(shmid,IPC_RMID,NULL); gv(shmid)=0;
     bzero((void*) gv(dda_index), size);

  /* ---------------------------------------------------------------- *\
     Distributed System V IPC information to all intra-node processes
  \* ---------------------------------------------------------------- */
     DEBUG_OUT(LVL2,(stdout,"%s: distributed sysv ipc ids.\n",DDI_Id()))
   
   # ifdef CRAY_MPI
     MPI_Barrier(comm->smp_world);
     shmid_all = (int *) Malloc(smp_np*sizeof(int));
     MPI_Gather(&shmid,1,MPI_INT,shmid_all,1,MPI_INT,0,comm->smp_comm);
     MPI_Barrier(comm->smp_world);
     MPI_Bcast(shmid_all,smp_np,MPI_INT,0,comm->smp_world);
     MPI_Bcast(&dlb_access,1,MPI_INT,0,comm->smp_world);
     MPI_Bcast(&fence_access,1,MPI_INT,0,comm->smp_world);
     for(i=0; i<smp_np; i++) {
        if(comm->me_local == i) {
           gv(smp_index)[i] = (DDA_Index *) gv(dda_index);
        } else {
           gv(smp_index)[i] = (DDA_Index *) Shmat(shmid_all[i],0,0);
        }
        gv(ddi_sync)     = fence_access;
        gv(fence_access) = fence_access;
        gv(dlb_access)   = dlb_access;
     }
     MPI_Barrier(comm->smp_world);
     free(shmid_all);
   # endif
   }
 # endif
 # endif


/* -------------------------------------------------------------------------- *\
   DDI_Memory_push(size,addr,offset)
   =================================
   [IN]  size - amount of memory to reserve on the stack.
   [OUT] addr - set to the starting address of the "newly"  resevered memory.
   [OUT] offset - starting address in # of bytes from beginning of the stack.
   
   Pushes the distributed-memory stack.
\* -------------------------------------------------------------------------- */
   void DDI_Memory_push(size_t size,void **addr,size_t *offset) {
     
      int np,me;
      double need_d;
      DEBUG_OUT(LVL4,(stdout,"%s: Entering DDI_Memory_push.\n",DDI_Id()))

      if(addr != NULL) *addr = (void *) &gv(mem_addr)[*gv(mem_used)];
      if(offset != NULL) *offset = *gv(mem_used);

      *gv(mem_used) += size;
      *gv(mem_max)   = max(*gv(mem_max),*gv(mem_used));

      if(*gv(mem_used) >= *gv(mem_total)) {
         fprintf(stdout,"%s: Insufficient distributed memory,\n",DDI_Id());
         fprintf(stdout," unable to create DDI array number %i.\n",gv(ndda));
         DDI_NProc(&np,&me);
         if (gv(ndda) > 0) {
            need_d = *gv(mem_used) - size;
            need_d /= (double) 8.0;
            need_d *= (double) np;
            need_d /= (double) 1000000.0;
            fprintf(stdout," Previously created DDI arrays (numbered %i to %i) consume %.0lf MWords.\n",0,gv(ndda)-1,need_d);
         }
         need_d  = size;
         need_d /= (double) 8.0;
         need_d *= (double) np;
         need_d /= (double) 1000000.0;
         fprintf(stdout," DDI array %i will require %.0lf MWords more.\n",gv(ndda),need_d);
         need_d  = *gv(mem_used) - *gv(mem_total);
         need_d /= (double) 8.0;
         need_d *= (double) np;
         need_d /= (double) 1000000.0;
         fprintf(stdout," Therefore, increase MEMDDI by at least %.0lf MWords.\n",need_d);
         fprintf(stdout,"\n");
         fprintf(stdout," This job may still require additional DDI arrays,\n");
         fprintf(stdout," so execution of EXETYP=CHECK is recommended.\n");
         fprintf(stdout,"\n");
         Fatal_error(911);
      }

      DEBUG_OUT(LVL4,(stdout,"%s: Leaving DDI_Memory_push.\n",DDI_Id()))
   }


/* -------------------------------------------------------------------- *\
   DDI_Memory_pop(size)
   ====================
   [IN] size - number of bytes to free from the end of the stack.
   
   Frees a segment on the distributed-memory stack.
\* -------------------------------------------------------------------- */
   void DDI_Memory_pop(size_t size) {
      *gv(mem_used) -= size;
   }


/* -------------------------------------------------------------------- *\
   DDI_Memory_avail(size)
   ======================
   [OUT] size - set to the number of bytes free on the memory stack.
   Returns the amount of free memory on the stack
\* -------------------------------------------------------------------- */
   void DDI_Memory_avail(size_t *size) {
      *size = *gv(mem_total) - *gv(mem_used);
   }
   
   
/* -------------------------------------------------------------------- *\
   DDI_Memory_heap_malloc(buffer,size)
   ===================================
   [IN/OUT] buffer - address of newly malloc'ed memory.
   [IN]     size   - size of memory to malloc.
   
   Malloc's a segment of memory.  This is should be a very temporary
   segment of memory.  Currently, we only use heap memory when using
   LAPI, because a stack can not handle concurrent data requests.
\* -------------------------------------------------------------------- */
   void DDI_Memory_heap_malloc(void **buffer,size_t size) {
    # if defined TRACK_HEAP
      DDI_Sem_acquire(gv(dda_access),MAX_DD_ARRAYS,DDI_WRITE_ACCESS);
      gv(mem_heap_total) += size;
      gv(mem_heap_max) = max(gv(mem_heap_max),gv(mem_heap_total));
      DDI_Sem_release(gv(dda_access),MAX_DD_ARRAYS,DDI_WRITE_ACCESS);
    # endif
      *buffer = (void *) Malloc(size);
   }
   
   
/* -------------------------------------------------------------------- *\
   DDI_Memory_heap_free(buffer,size)
   =================================
   [IN/OUT] buffer - address of newly malloc'ed memory.
   [IN]     size   - size of memory to malloc.
   
   Frees the malloc'ed memory
\* -------------------------------------------------------------------- */
   void DDI_Memory_heap_free(void *buffer,size_t size) {
    # if defined TRACK_HEAP
      DDI_Sem_acquire(gv(dda_access),MAX_DD_ARRAYS,DDI_WRITE_ACCESS);
      gv(mem_heap_total) -= size;
      DDI_Sem_release(gv(dda_access),MAX_DD_ARRAYS,DDI_WRITE_ACCESS);
    # endif
      free(buffer);
   }


static void
ddi_initialize_dda_index(DDA_Index *dda)
{
        int i, pt_rc;
        pthread_mutexattr_t mattr;

        pt_rc = pthread_mutexattr_init(&mattr);
        PTHREAD_RC_CHECK(pt_rc,"mutexattr_init");

        pt_rc = pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
        PTHREAD_RC_CHECK(pt_rc,"mutexattr_setpshared");

        for(i=0; i<MAX_DD_ARRAYS; i++) {
           pt_rc = pthread_mutex_init(&dda[i].mutex, &mattr);
           PTHREAD_RC_CHECK(pt_rc,"mutex_init");

           pt_rc = pthread_spin_init(&dda[i].fence, PTHREAD_PROCESS_SHARED);
           PTHREAD_RC_CHECK(pt_rc,"spinlock fence init");
        }
}
