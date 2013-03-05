 # include "ddi_base.h"

/* ----------------------------------- *\
   DDI Communicators Global Structures
\* ----------------------------------- */
   int gv(ddi_data_id)      = 17;
   int gv(ddi_comm_id)      = DDI_COMM_WORLD;
   int gv(ddi_working_comm) = DDI_COMM_WORLD;
   DDI_Comm gv(ddi_base_comm);
   DDI_Data gv(ddi_base_data);

/* ---------------------------------------- *\
   DDI Communicators Initialization Routine
\* ---------------------------------------- */
   void Comm_init() {

      DDI_Comm *comm = (DDI_Comm *) &gv(ddi_base_comm);
      DDI_Data *data = (DDI_Data *) &gv(ddi_base_data);

      int i;
      int ddi_np,ddi_me;
      int ddi_nn,ddi_my;
      int smp_np,smp_me;

      DDI_NNode(&ddi_nn,&ddi_my);
      DDI_NProc(&ddi_np,&ddi_me);
      DDI_SMP_NProc(&smp_np,&smp_me);

      comm->id = gv(ddi_comm_id)++;
      comm->nn = ddi_nn;
      comm->my = ddi_my;
      comm->np = ddi_np;
      comm->me = ddi_me;
      comm->np_local = smp_np;
      comm->me_local = smp_me;
      comm->ngroups  = 1;
      comm->mygroup  = 0;

      DEBUG_OUT(LVL3,(stdout,"%s: np=%i; me=%i; nn=%i; my=%i; smp_np=%i; smp_me=%i.\n",
                      DDI_Id(),ddi_np,ddi_me,ddi_nn,ddi_my,smp_np,smp_me))

    # if defined USE_SYSV || defined DDI_ARMCI_SMP
      comm->next        = NULL;
      comm->smp_pid     = (int *) Malloc(smp_np*sizeof(int));
      comm->local_nid   = (int *) Malloc(ddi_np*sizeof(int));
      comm->global_pid  = (int *) Malloc(ddi_np*sizeof(int));
      comm->global_nid  = (int *) Malloc(ddi_np*sizeof(int));
      comm->global_dsid = (int *) Malloc(ddi_nn*sizeof(int));
      comm->node_master = (int *) Malloc(ddi_nn*sizeof(int));

      for(i=0; i<ddi_np; i++) {
         comm->global_pid[i] = i;
         comm->global_nid[i] = gv(ddiprocs)[i].node;
         comm->local_nid[i]  = gv(ddiprocs)[i].node;
      }

      for(i=0; i<ddi_nn; i++) {
         comm->global_dsid[i] = gv(ddinodes)[i].myds;
         comm->node_master[i] = gv(ddinodes)[i].nodemaster;
      }

      if(comm->me < comm->np) {
         for(i=0; i<smp_np; i++) {
            comm->smp_pid[i] = comm->node_master[comm->my] + i;
         }
      }
    # else
      comm->next        = NULL;
      comm->smp_pid     = NULL;
      comm->local_nid   = (int *) Malloc(ddi_np*sizeof(int));
      comm->global_pid  = (int *) Malloc(ddi_np*sizeof(int));
      comm->global_dsid = (int *) Malloc(ddi_np*sizeof(int));
      comm->global_nid  = comm->global_pid;
      comm->node_master = comm->global_pid;

      for(i=0; i<ddi_np; i++) {
         comm->local_nid[i]   = i;
         comm->global_pid[i]  = i;
#if defined DDI_ARMCI
         comm->global_dsid[i] = i;
#else
         comm->global_dsid[i] = i+ddi_np;
#endif
      }
    # endif


   /* ----------------------------------------------------------- *\
    * Data servers have all the information they need to operate.
   \* ----------------------------------------------------------- */
      if(ddi_me >= ddi_np) return;

   /* ----------------------------------------------------------- *\
    * At somepoint, someone might want to have a shared-memory
    * segment associated with each communicator.  Here is where
    * the initial shared-memory seg for the initial communicator
    * would be created.  For now it is commented out.
   \* ----------------------------------------------------------- */
   /*  comm->data_id = Comm_init_data(comm); */

   }


/* the follow code is commented out by #ifdef IMPLEMENT_SHARED_MEMORY_FOR_DDI_COMMS
 * a small amount of work is needed to fully implement this subroutine
 * it should prove each communicator with a shared-memory segment and whatever
 * else might be needed for efficient shared-memory operations
 */
#ifdef IMPLEMENT_SHARED_MEMORY_FOR_DDI_COMMS
   void Comm_init_data(const DDI_Comm *comm) {

      DDI_List global_list;

   /* ------------------------------------------------------ *\
    * Global Synchronization List via Sockets;
   \* ------------------------------------------------------ */
      global_list.np   = comm->np;
      global_list.me   = comm->me;
      global_list.root = 0;
      global_list.pids = (const int *) comm->global_pid;
    # if defined DDI_MPI
      global_list.comm = comm->compute_comm;
    # endif


   /* -------------------------------------------------------------- *\
    * Sync global compute processes - all processes gsum via sockets
   \* -------------------------------------------------------------- */
      global_list.tag  = 1234;
      Comm_sync_list(&global_list);
      

   /* ----------------------------------------------------- *\
      Initialize shared-memory associated with communicator
   \* ----------------------------------------------------- */
      comm->data_id = data->id = gv(ddi_data_id)++;
      data->next        = NULL;
      data->buffer      = NULL;
      data->buffer_size = 0;
      data->sync_array  = NULL;
      data->sync_num    = 0;

      if(comm->np_local > 1) {

       # if defined USE_SYSV
      /* ---------------------------------------------- *\
         List for broadcasting initial info via sockets
      \* ---------------------------------------------- */
         list.np   = comm->np_local;
         list.me   = comm->me_local;
         list.root = 0;
         list.pids = (const int*) comm->smp_pid;
       # if defined DDI_MPI
         list.comm = comm->smp_comm;
       # endif
 
      /* ------------------------------------------- *\
         Get and attach to the shared-memory segment
      \* ------------------------------------------- */
         if(DDI_BUFFER_SIZE <= 0 && comm->me == 0) {
            fprintf(stdout," DDI Warning: DDI_BUFFER_SIZE not specified. \
                             Shared-memory will not be used.\n");
            Fatal_error(911);
         }
         size = DDI_BUFFER_SIZE + comm->np_local*sizeof(CS_int);

      /* ----------------------------------------------------------------- *\
       * Allocate SysV Shared-Memory for the communicator's data structure
      \* ----------------------------------------------------------------- */
         if(comm->me_local == 0) gv(shmid) = Shmget(IPC_PRIVATE,size,SHM_R|SHM_W);

      /* ---------------------------------------------------------- *\
       * Broadcast the shm identifer to all local compute processes
      \* ---------------------------------------------------------- */
         Comm_bcast_list(&gv(shmid),sizeof(int),&list);

      /* ------------------------------------- *\
       * Attached to the shared-memory segment
      \* ------------------------------------- */
         tmp_array = (CS_int *) Shmat(gv(shmid),0,0);

      /* ------------------------------------------------------------------ *\
       * Initialize shared-memory segment associated with the communicator.
       * This shared-memory segement contains two pieces:
       * 1) sync_array -> cache safe array of integers for fast sync'ing
       * 2) buffer -> shared buffer for bcast/gsum
      \* ------------------------------------------------------------------ */
         data->sync_array  = (CS_int *) tmp_array;
         data->buffer      = (void *) &tmp_array[comm->np_local];
         data->buffer_size = DDI_BUFFER_SIZE;
        
      /* -------------------------- *\
       * Sync smp compute processes
      \* -------------------------- */
         list.tag  = 1235;
         Comm_sync_list(&list);
      
      /* ---------------------------------------------------- *\
       * Initialize the SMP sync array -- used for fast syncs
      \* ---------------------------------------------------- */
         if(comm->me_local == 0) {
            Shmctl(gv(shmid),IPC_RMID,NULL);
            for(i=0; i<comm->np_local; i++) data->sync_array[i].val = 0;
         }
         
      /* -------------------------- *\
       * Sync smp compute processes
      \* -------------------------- */
         list.tag  = 1236;
         Comm_sync_list(&list);

      /* ----------------------------------------------------------------- *\
       * Reaching this point ensures that all the necessary processes have
       * attached to the shard-memory segment and that the newly created
       * shared-memory segment will be removed from the system once all
       * attached processes have detached.  gv(shmid) the global shared-
       * memory cleanup variable can safely be set to zero.
      \* ----------------------------------------------------------------- */
         gv(shmid) = 0;
       # else
         fprintf(stdout,"%s: Comm_init - This is a serious error.");
         Fatal_error(911);
       # endif

      } else {

      /* ---------------------------------------------------------------- *\
       * Single processor nodes do not need shared-memory communicator
       * data structures; instead, we only need a single local buffer.
       * ---------------------------------------------------------------- */
         data->buffer      = Malloc(DDI_BUFFER_SIZE);
         data->buffer_size = DDI_BUFFER_SIZE;
      }

   /* -------------------------------------------------------------- *\
    * Sync global compute processes - all processes gsum via sockets
   \* -------------------------------------------------------------- */
      global_list.tag = 1237;
      Comm_sync_list(&global_list);

      DEBUG_OUT(LVL3,(stdout,"%s: Leaving Comm_init.\n",DDI_Id()))

   }
#endif


   void *Comm_find(int comm_id) {

     DDI_Comm *comm = &gv(ddi_base_comm);

     do { 
        if(comm->id == comm_id) return comm;
        comm = (DDI_Comm *) comm->next;
     } while(comm);

     fprintf(stdout,"%s: Unable to find communicator associated with id %i.\n",DDI_Id(),comm_id);
     Fatal_error(911);

     return NULL;

   }

   void *Data_find(int data_id) {

     DDI_Data *data = &gv(ddi_base_data);

     do {
        if(data->id == data_id) return data;
        data = (DDI_Data *) data->next;
     } while(data);

     fprintf(stdout,"%s: Unable to find data struct associated with id %i.\n",DDI_Id(),data_id);
     Fatal_error(911);

     return NULL;

   }

   void *Comm_find_end() {

      DDI_Comm *comm = &gv(ddi_base_comm);

      while(comm->next) {
         comm = (DDI_Comm *) comm->next;
      }

      return (void *) comm;
   }

   void *Data_find_end() {

      DDI_Data *data = &gv(ddi_base_data);

      while(data->next) {
         data = (DDI_Data *) data->next;
      }

      return (void *) data;
   }

