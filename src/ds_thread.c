#include "ddi_base.h"

pthread_t *ds_thread = NULL;
pthread_mutex_t *mpi_realm_mutex = NULL;

static int NRequests;
static int *index_ds = NULL;
static DDI_Patch *p = NULL;
static MPI_Status *s = NULL;
static MPI_Request *r = NULL;

void DS_main();

void DS_Thread_init() {
   int err;
   int i,ir;
   int np,me;
   int size = sizeof(DDI_Patch);
   const DDI_Comm *comm = (const DDI_Comm *) &gv(ddi_base_comm);

 # ifdef DS_SIGNAL
   struct timeval t;
   struct itimerval timer;
 # endif

 # ifdef DS_THREAD
   pthread_attr_t thread_attr;
   pthread_attr_init(&thread_attr);
   pthread_attr_setscope(&thread_attr, PTHREAD_SCOPE_SYSTEM);
 # endif

/* initialize requests */
   DDI_NProc(&np,&me);
   index_ds = (int *) Malloc(np*sizeof(int));
   p = (DDI_Patch *) Malloc(np*sizeof(DDI_Patch));
   s = (MPI_Status *) Malloc(np*sizeof(MPI_Status));
   r = (MPI_Request *) Malloc(np*sizeof(MPI_Request));

/* post irecvs */

 # ifdef DS_SIGNAL
   signal(SIGALRM,   SIG_IGN);
// signal(SIGVTALRM, DS_Thread_main);
// signal(SIGPROF,   DS_Thread_main);
   timer.it_interval.tv_sec  = 0;
   timer.it_interval.tv_usec = 10000;
   timer.it_value.tv_sec     = 0;
   timer.it_value.tv_usec    = 10000;
// alarm(1);
   if(comm->me_local == 1) {
      for(i=0,ir=0; i<np; i++) if(i != me) {
         MPI_Irecv(&p[ir],size,MPI_BYTE,i,37,comm->world_comm,&r[ir]); ir++;
      }
      NRequests = ir;
// the ds is ignored until needed
//    signal(SIGALRM,DS_Thread_main);
      setitimer(ITIMER_REAL,&timer,NULL);
   }
 # endif

 # ifdef CRAY_TEST
   for(i=0; i<np; i++) {
      MPI_Irecv(&p[i],size,MPI_BYTE,i,37,comm->world_comm,&r[i]);
   }
   NRequests = np;
   DS_main();
 # endif


 # ifdef DS_THREAD
   if(ds_thread == NULL) {
      ds_thread = (pthread_t *) Malloc(sizeof(pthread_t));
   } else {
      fprintf(stdout," error: ds_thread != NULL\n");
      Fatal_error(911);
   }

   if(mpi_realm_mutex == NULL) { 
      mpi_realm_mutex = (pthread_mutex_t *) Malloc(sizeof(pthread_mutex_t));
      err = pthread_mutex_init(mpi_realm_mutex, NULL);
      if(err) {
         fprintf(stdout," error initializing mutex (%x); err=%i\n",mpi_realm_mutex, err);
         Fatal_error(911);
      }
   } else {
      fprintf(stdout," error: mpi_realm_mutex != NULL.\n");
      Fatal_error(911);
   }

   if( pthread_create(ds_thread,&thread_attr,DS_Thread_main,NULL) != 0 ) {
     fprintf(stderr," DS_Thread_init: Unable to create test thread.\n");
     Fatal_error(911);
   }
 # endif
}


void DS_Thread_finalize() {

 # ifdef DS_SIGNAL
   int np,me,nn,my,remote_id,i,ack;
   DDI_Patch msg;
   DDI_Request req;
   const DDI_Comm *comm = (const DDI_Comm *) &gv(ddi_base_comm);

   DDI_SMP_NProc(&np,&me);
   DDI_NNode(&nn,&my);
   msg.oper = DDI_QUIT;

   MPI_Barrier(comm->world_comm);

   for(i=0; i<nn; i++) {
      if(i == my && me == 1) continue;
      remote_id = i;
      DDI_Send_request(&msg,&remote_id,&req);
      MPI_Wait(&req,MPI_STATUS_IGNORE);
      MPI_Recv(&ack,1,MPI_INT,remote_id,19,comm->world_comm,MPI_STATUS_IGNORE);
   }

   MPI_Barrier(comm->world_comm);
 # endif

 # ifdef DS_THREAD
   if( pthread_join(*ds_thread,NULL) != 0 ) {
     fprintf(stderr," DS_Thread_finalize: pthread_join failed.\n");
     Fatal_error(911);
   }
 # endif
  
}


void Comm_acquire_realm() {
   int err;
   err = pthread_mutex_lock(mpi_realm_mutex);
   if(err) {
      fprintf(stdout," some error in _lock. err=%i\n",err);
      Fatal_error(911);
   }
}

void Comm_release_realm() {
   pthread_mutex_unlock(mpi_realm_mutex); 
}

# ifdef DS_THREAD
void* DS_Thread_main(void* arg) {
# endif

# ifdef DS_SIGNAL
void DS_Thread_main(int signo) {
# endif

void DS_main() {

 # ifdef DS_THREAD
   int run = 1;
   unsigned int sleep = 5000;
 # endif

 # ifdef CRAY_TEST
   int run = 1;
 # endif

   void *buffer;
   size_t counter_value;
   int i,from,nfinished,ack,quit=0;
   size_t size = sizeof(DDI_Patch);
   const DDI_Patch *msg = NULL;
   const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_COMM_WORLD);
   int last;

 # ifdef DS_SIGNAL
   signal(SIGALRM,SIG_IGN);
 # endif

/* Initialize Data Request IRecvs 
   fence = (int *) Malloc(np*sizeof(int));
   fprintf(stdout,"thread: issuing recvs\n");
   fflush(stdout);
   Comm_acquire_realm();
   for(i=0,ir=0; i<np; i++) if(i != me) {
      MPI_Irecv(&p[ir],size,MPI_BYTE,i,37,comm->world_comm,&r[ir]); ir++;
   } 
   Comm_release_realm();
   fprintf(stdout,"thread: recvs issued\n");
   fflush(stdout);

   Data Server Thread */

 # ifdef DS_THREAD
   while(run) {
      Comm_acquire_realm();
 # endif

 # ifdef CRAY_TEST
   last = -1;
   nfinished = 0;
   while(run) {
 # endif

      do {
         MPI_Testsome(NRequests,r,&nfinished,index_ds,s);
         if(nfinished != last) {
            last = nfinished;
            fprintf(stdout,"thread[%i]: nfinished=%i\n",comm->me,nfinished);
            fflush(stdout);
         }
         for(i=0; i<nfinished; i++) {
            msg   = &p[index_ds[i]];
            from  = s[i].MPI_SOURCE;

//          Comm_patch_print(msg);

            switch(msg->oper) {

            case DDI_FENCE:
//             fence[from] = 1;
            break;
            
            case DDI_GET:
//             fprintf(stdout,"thread: get memory.\n");
//             fflush(stdout);
               DDI_Memory_push(msg->size,(void **)&buffer,NULL);                        
//             fprintf(stdout,"thread: local get.\n");
//             fflush(stdout);
               DDI_Get_local(msg,buffer);
//             fprintf(stdout,"thread: send local buffer back to %i.\n",from);
//             fflush(stdout);
               MPI_Send(buffer,msg->size,MPI_BYTE,from,17,comm->world_comm);
//             fprintf(stdout,"thread: release memory.\n");
//             fflush(stdout);
               DDI_Memory_pop(msg->size); 
//             fprintf(stdout,"thread: get finished.\n");
//             fflush(stdout);
            break;

            case DDI_PUT:
//             fprintf(stdout,"thread: put memory.\n");
//             fflush(stdout);
               DDI_Memory_push(msg->size,(void **)&buffer,NULL);                        
//             fprintf(stdout,"thread: put recv.\n");
//             fflush(stdout);
               MPI_Recv(buffer,msg->size,MPI_BYTE,from,17,comm->world_comm,MPI_STATUS_IGNORE);
//             fprintf(stdout,"thread: put local.\n");
//             fflush(stdout);
               DDI_Put_local(msg,buffer);
               MPI_Send(&ack,1,MPI_INT,from,19,comm->world_comm);
               DDI_Memory_pop(msg->size); 
            break;
               
            case DDI_ACC:
//             fprintf(stdout,"thread: acc memory.\n");
//             fflush(stdout);
               DDI_Memory_push(msg->size,(void **)&buffer,NULL);                        
//             fprintf(stdout,"thread: acc recv.\n");
//             fflush(stdout);
               MPI_Recv(buffer,msg->size,MPI_BYTE,from,17,comm->world_comm,MPI_STATUS_IGNORE);
//             fprintf(stdout,"thread: acc local.\n");
//             fflush(stdout);
               DDI_Acc_local(msg,buffer);                                               
//             fprintf(stdout,"thread: acc ack.\n");
//             fflush(stdout);
               MPI_Send(&ack,1,MPI_INT,from,19,comm->world_comm);
               DDI_Memory_pop(msg->size); 
//             fprintf(stdout,"thread: acc finished.\n");
//             fflush(stdout);
            break;

            case DDI_GETACC:
               fprintf(stdout,"%s: getacc not implemented\n",DDI_Id());
               Fatal_error(911);
            break;

            case DDI_DLBNEXT:
               DDI_DLBNext_local(&counter_value);
               MPI_Send(&counter_value,sizeof(size_t),MPI_BYTE,from,17,comm->world_comm);
            break;

            case DDI_QUIT:
               quit = 1;
               MPI_Send(&ack,1,MPI_INT,from,19,comm->world_comm);
            break;

            }
            
            if(!quit) MPI_Irecv(&p[index_ds[i]],size,MPI_BYTE,from,37,comm->world_comm,&r[index_ds[i]]);
         }

       # ifdef DS_THREAD
         if(nfinished) sleep = 5000;
       # endif

      } while(nfinished > 0);

 # ifdef CRAY_TEST
   }
 # endif
 
 # ifdef DS_THREAD
      Comm_release_realm();
      sleep += 5000;
      if(sleep > 25000) sleep = 250000;
      usleep(sleep);
   } 
 # endif 

 # ifdef DS_SIGNAL
   signal(SIGALRM,DS_Thread_main);
// alarm(1);
 # endif

 # ifdef DS_THREAD
   return NULL;
 # endif
}


void Comm_patch_print(const DDI_Patch *patch) { 
   fprintf(stdout," *** DDI_Patch Printout\n");
   fprintf(stdout," *** oper   = %i\n",patch->oper);
   fprintf(stdout," *** handle = %i\n",patch->handle);
   fprintf(stdout," *** ilo    = %i\n",patch->ilo);
   fprintf(stdout," *** ihi    = %i\n",patch->ihi);
   fprintf(stdout," *** jlo    = %i\n",patch->jlo);
   fprintf(stdout," *** jhi    = %i\n",patch->jhi);
   fprintf(stdout," *** size   = %li\n",patch->size);
   fprintf(stdout," \n");
   fflush(stdout);
}

