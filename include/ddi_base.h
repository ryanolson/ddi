/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Private/Internal Prototypes and Data Structures
 * 
 * Author: Ryan M. Olson
 * CVS $Id: ddi_base.h,v 1.8 2006/06/19 23:33:51 ryan Exp $
\* -------------------------------------------------------------------- */
 # ifndef __DDI_BASE_H__
 # define __DDI_BASE_H__

/* ------------------- *\
   DDI Default Options
\* ------------------- */
 # if (!defined DDI_SOC)   && (!defined DDI_MPI) && \
      (!defined DDI_SHMEM) && (!defined DDI_SEQ)
 # error "Declare a message passing library or none!"
 # error "But for goodness sakes ... declare something!!"
 # endif


/* ------------------------------------ *\
   Maximum number of distributed arrays
   To be safe, make divisible by 8.
\* ------------------------------------ */
 # if !defined MAX_DD_ARRAYS
 # define MAX_DD_ARRAYS 25
 # endif


/* ---------------------------- *\
   Shared-memory buffer (bytes)
   raised from 1 MB (ONE_MB) to 10 MB by MWS in March 2007,
   but machines with small memory should compile for 1 MB.
\* ---------------------------- */
 # if !defined DDI_BUFFER_SIZE
 # define DDI_BUFFER_SIZE TEN_MB 
 # endif


/* ------------------------- *\
   Runtime Option Parameters
\* ------------------------- */
 # define DDI_STANDARD_RUN   0
 # define DDI_DEBUG_RUN      1
 # define DDI_CHECK_RUN      2


/* ------------------------- *\
   Access control parameters
\* ------------------------- */
 # define DDI_WRITE_LOCK     1
 # define DDI_FULL_LOCK   1000

 # define DDI_READ_ACCESS    DDI_WRITE_LOCK 
 # define DDI_WRITE_ACCESS   DDI_FULL_LOCK


/* ---------------------------------------------------------- *\
   Macro used for global variable -- Ensures unique namespace
\* ---------------------------------------------------------- */
 # define gv(a) __ddi_common__ ## a ## __

 # define PTHREAD_RC_CHECK(rc,msg) do { if(rc) { printf("%s fail; rc=%d\n",msg,rc); abort(); } } while(0)

/* ------------- *\
   Include Files
\* ------------- */
 # include "mysystem.h"
 # include "common.h"
 # include "ddi.h"

/* ----------------------------------------- *\
   Structure for Defining a DDI Communicator
\* ----------------------------------------- */
   typedef struct {
     int id;
     int nn;           /* number of nodes                             */
     int np;           /* number of processes                         */
     int np_local;     /* number of local processes                   */
     int my;           /* node rank                                   */
     int me;           /* process rank                                */
     int me_local;     /* local process rank                          */
     int ngroups;      /* number of ddi groups running concurrently   */
     int mygroup;      /* rank of the ddi group on which this comm
                          operates                                    */
  /* int data_id;       * handle for the data set associated w/ comm  */
     int *smp_pid;     /* global process ids of all local processes   */
     int *local_nid;   /* local node ids of allthe proccesses mapped  */
     int *global_pid;  /* global process ids of all processes mapped  */
     int *global_nid;  /* global node ids of all processes mapped     */
     int *global_dsid; /* global process ids of data servers by node  */
     int *node_master; /* global process ids of each node master      */
     void *next;       /* pointer to next communicator (linked-list)  */

   # ifdef DDI_MPI
     MPI_Comm compute_comm; /* communicator for all compute processes      */
     MPI_Comm world_comm;   /* communicator containing the ddi ordered
                               list of compute proceseses and data servers */
     MPI_Comm smp_comm;     /* communicator for all smp compute processes  */
     MPI_Comm smp_world;    /* SMP_World_comm from Init_mpi */
     MPI_Comm node_comm; /* communicator containing the master process
                               on each node                                */
   # endif
  
   } DDI_Comm;

/* ----------------------------------------- *\
   Structure for Defining a DDI List
\* ----------------------------------------- */
   typedef struct {
      int np;
      int me;
      int tag;
      int root;
      const int *pids;
    # if defined DDI_MPI
      MPI_Comm comm;
    # endif
   } DDI_List;


/* ----------------------------------------- *\
   Structure for a cache safe integer array
\* ----------------------------------------- */
   typedef struct {
      volatile int  val;
      char cache_line_offset[CACHE_LINE_SIZE];
   } CS_int;


/* -------------------------------------------------- *\
   Structure for a DDI communicator buffer/sync array
\* -------------------------------------------------- */
   typedef struct {
      int     id;          /* linked-list id                       */
      void*   next;        /* pointer to next DDI_Data struct */
      void   *buffer;      /* shared-memory buffer                 */
      size_t  buffer_size; /* shared-memory buffer size (bytes)    */
      CS_int *sync_array;  /* cache-safe int array for syncs       */
      int     sync_num;    /* sync number -- important for syncing */
   } DDI_Data;


/* --------------------------------- *\
   Structure for Shared-memory array
\* --------------------------------- */
   typedef struct {
      int    handle;
      int    shmid;
      int    semid;
      void  *addr;
      size_t size;
      void  *next;
   } SMP_Data;


/* --------------------------------- *\
   Structure for Shared-memory array
\* --------------------------------- */
   typedef struct {
      long   offset;
      size_t size;
   } DB_Entry;


/* -------------------------------------------------------- *\
   Structure for Indexing a Distributed Data Memory Segment
\* -------------------------------------------------------- */
   typedef struct {
     size_t size,offset;
     pthread_mutex_t mutex;
     pthread_spinlock_t fence;
     int ilo,ihi,jlo,jhi,ncols,nrows,semid;
   } DDA_Index;

/* -------------------------------------------------- *\
   Structure for Non-blocking DDI_ISend and DDI_IRecv
\* -------------------------------------------------- */
 # if defined DDI_SOC && !defined DDI_MPI
   typedef struct {
     int         to; 
     size_t      size;
     void       *buffer;
     pthread_t   hnd_thread;
   } DDI_Request;
 # else
   typedef MPI_Request DDI_Request;
 # endif


/* memory global variables...added for Blue Gene */
static char   *gv(mem_addr)  = NULL;
static size_t *gv(mem_total) = NULL;
static size_t *gv(mem_used)  = NULL;
static size_t *gv(mem_max)   = NULL;

# if defined DDI_LAPI
static size_t gv(mem_heap_max) = 0;
static size_t gv(mem_heap_total) = 0;
# endif


/* ---------------------------------- *\
   Global Variables -- Identification
\* ---------------------------------- */
   extern int *gv(global_proc_map);
   extern int *gv(global_node_map);
   extern int *gv(master_map);

   extern int gv(np);
   extern int gv(nc);
   extern int gv(nd);

 # ifdef DDI_MPI
   extern int *np_by_node;
   extern int *nc_by_node;
   extern int *nd_by_node;
   extern int **ranks_by_node;
 # endif


/* --------------------------------- *\
   Global Variables -- Communicators
\* --------------------------------- */
   extern int gv(ddi_comm_id);
   extern int gv(ddi_data_id);
   extern int gv(ddi_working_comm);
   extern DDI_Comm gv(ddi_base_comm);
   extern DDI_Data gv(ddi_base_data);


/* --------------------------------- *\
   Global Variables -- Shared-Memory
\* --------------------------------- */
   extern SMP_Data *gv(smp_base_data);


/* ------------------------------------ *\
   Global Variables -- Distributed Data
\* ------------------------------------ */
   extern int gv(ndda);                      /* Number of DD arrays in use */
   extern int gv(nrow)[MAX_DD_ARRAYS];       /* Number of rows in each array */
   extern int gv(ncol)[MAX_DD_ARRAYS];       /* Number of columns in each array */
   extern int gv(ddatype)[MAX_DD_ARRAYS];    /* DD array type */
   extern int gv(dda_output);                /* Flag to turn on/off create messages */
   extern int gv(ncmap)[MAX_DD_ARRAYS][MAX_NODES+1];      /* DD column map by node */
   extern int gv(pcmap)[MAX_DD_ARRAYS][MAX_PROCESSORS+1]; /* DD column map by process */


/* -------------------------------------- *\
   Global Variables -- Distributed Memory
\* -------------------------------------- */
   extern DDA_Index *gv(dda_index);
   extern DDA_Index *gv(smp_index)[MAX_SMP_PROCS];

 # if defined USE_SYSV 
   extern int gv(shmid);
   extern int gv(dda_access);
   extern int gv(fence_access);
   extern int *gv(fence_addr);
   extern void *gv(buffer_addr);
 # endif

 # if FULL_SMP
   extern int gv(ddi_sync);
   extern int gv(ddi_buffer);
 # endif


/* ---------------------------------- *\
   Global Variables -- Load Balancing
\* ---------------------------------- */
   extern size_t *gv(dlb_counter);
   extern size_t *gv(gdlb_counter);

 # if FULL_SMP
   extern int gv(dlb_access);
 # endif

/* ------------------------------------ *\
   Global Variables -- TCP/IP Variables
\* ------------------------------------ */
 # if defined DDI_SOC
   extern int *gv(sockets);        /* TCP socket values */
   extern int gv(serverport);      /* TCP port on which to receive connections */
   extern int gv(serversock);      /* TCP socket listening and acceping connections */
   extern int gv(kickoffsock);     /* TCP socket to DDI kickoff program */
 # endif


/* ------------------------------------------------ *\
   Global Variables -- Node and Process Information
\* ------------------------------------------------ */
   extern Node_info gv(ddinodes)[MAX_NODES];
   extern Proc_info gv(ddiprocs)[2*MAX_PROCESSORS];


/* ------------------------------------------ *\
   Global Variables -- Non-blocking Send/Recv
\* ------------------------------------------ */
   extern DDI_Request gv(isend_req);
   extern DDI_Request gv(irecv_req);


/* ------------------------------------- *\
   Global Variables -- MPI and DS Thread
\* ------------------------------------- */
 # if defined DDI_MPI
 # if defined CRAYXT
   pthread_t         *ds_thread;
   pthread_mutex_t   *mpi_realm_mutex;
 # endif
 # endif


/* ---------------------------------- *\
   Global Variables -- LAPI Variables
\* ---------------------------------- */
 # if defined DDI_LAPI
   extern lapi_handle_t gv(lapi_hnd);
   extern int           gv(lapi_dlb_cntr);
   extern int           gv(lapi_gdlb_cntr);
   extern uint *        gv(lapi_map);
   extern void **       gv(am_hndlr);
   extern int **        gv(lapi_dlb_cntr_addr);
   extern int **        gv(lapi_gdlb_cntr_addr);
 # endif


/* ------------------------------------ *\
   Global Variables -- Profile Counters
\* ------------------------------------ */
   typedef struct {
      size_t ncalls;
      size_t nbytes;
      size_t ncalls_local;
      size_t nbytes_local;

    # if defined DDI_PROFILE
      struct timeval timer;
    # endif

   } DDI_Profile_counter;

 # ifdef DDI_COUNTERS
   extern DDI_Profile_counter gv(get_profile);
   extern DDI_Profile_counter gv(put_profile);
   extern DDI_Profile_counter gv(acc_profile);
   extern DDI_Profile_counter gv(bcast_profile);
 # endif


/* ------------------------------ *\
   Global Variables -- DDI Timers
\* ------------------------------ */
   extern struct rusage  gv(cpu_timer);
   extern struct timeval gv(wall_timer);


/* ---------------------------------------------- *\
   Private Subroutine Prototypes -- Shared Memory
\* ---------------------------------------------- */
 # ifdef USE_SYSV
   void SMP_Init();
 # endif


/* ------------------------------------------------------ *\
   Private Subroutine Prototypes -- Memory Initialization
\* ------------------------------------------------------ */
   void DDI_Memory_init(size_t);
   void DDI_Memory_finalize();
   void DDI_Memory_server(size_t);
   void DDI_Memory_push(size_t,void **,size_t *);
   void DDI_Memory_pop(size_t);


/* --------------------------------------------------------- *\
   Private Subroutine Prototypes -- Indexing & Fence Control
\* --------------------------------------------------------- */
   void DDI_Index_create(const DDI_Patch*);
   void DDI_Index_destroy(const DDI_Patch*);
   void DDI_Fence_init();
   void DDI_Fence_check(int);
   void DDI_Fence_acquire(int);
   void DDI_Fence_release(int);
   

/* -------------------------------------------------------- *\
   Private Subroutine Prototypes -- Communication & Control
\* -------------------------------------------------------- */
   void DDI_Server();
   void DDI_Array_zero(int);
   void DDI_Send_request(void*,int*,DDI_Request*);
   void DDI_Recv_request(void*,int*);
   void DDI_Send_request_comm(void*,int*,DDI_Request*,int);

   void DDI_Get_local(const DDI_Patch*,void *);
   void DDI_Put_local(const DDI_Patch*,void *);
   void DDI_Acc_local(const DDI_Patch*,void *);
   void DDI_GetAcc_local(const DDI_Patch*,void *);
  
   void DDI_Get_remote(void *,DDI_Patch *,int,int);
   void DDI_Put_remote(void *,DDI_Patch *,int,int);
   void DDI_Acc_remote(void *,DDI_Patch *,int);

   void DDI_Get_server(const DDI_Patch*,int);
   void DDI_Put_server(const DDI_Patch*,int);
   void DDI_Acc_server(const DDI_Patch*,int);
   void DDI_GetAcc_server(const DDI_Patch*,int);

   void DDI_Subpatch(int,const DDI_Patch*,int*,int*,DDI_Patch*);
   void DDI_Subpatch_comm(int,const DDI_Patch*,int*,int*,DDI_Patch*,int);
   void DDI_Subpatch_SMP(int,const DDI_Patch*,int*,int*,DDI_Patch*,int node);
   

/* ------------------------------------------------------------- *\
   Private Subroutine Prototypes -- LAPI Communication & Control
\* ------------------------------------------------------------- */
 # if defined DDI_LAPI
   void *DDI_AM_Header_hndlr(lapi_handle_t*,void*,uint*,uint*,compl_hndlr_t**,void**);
   void  DDI_AM_Compl_hndlr(lapi_handle_t*,void*);
 
   void DDI_Get_lapi_server(DDI_Patch *,void *);
   void DDI_Put_lapi_server(DDI_Patch *,void *);
   void DDI_Acc_lapi_server(DDI_Patch *,void *);
   void DDI_GetAcc_lapi_server(DDI_Patch *,void *);
 # endif

/* -------------------------------------------------- *\
   Private Subroutine Prototypes -- MPI and DS Thread
\* -------------------------------------------------- */
 # ifdef CRAY_MPI
   void DS_Thread_init();
   void DS_Thread_finalize();

   void DS_Thread_main();

   void DS_Thread_process(const DDI_Patch*,int);

   void Comm_acquire_realm();
   void Comm_release_realm();
 # endif

   void Comm_patch_print(const DDI_Patch*);

   
/* ------------------------------------------------------- *\
   Private Subroutine Prototypes -- Dynamic Load Balancing
\* ------------------------------------------------------- */
   void DDI_DLBReset_local();
   void DDI_GDLBReset_local();
   void DDI_DLBNext_local(size_t*);
   void DDI_GDLBNext_local(size_t*);


/* ----------------------------------------------- *\
   Private Subroutine Prototypes -- Access Control
\* ----------------------------------------------- */
   void DDI_Acquire(const DDA_Index*,int,int,void**);
   void DDI_Release(const DDA_Index*,int,int);
   int  DDI_Sem_oper(int,int,int);
   void DDI_Sem_acquire(int,int,int);
   void DDI_Sem_release(int,int,int);
   void DDI_Sem_remove(int);
   void DDI_Shm_remove(int);


/* ---------------------------------------------- *\
   Private Subroutine Prototypes -- Miscellaneous
\* ---------------------------------------------- */
   const char *DDI_Id();
   int         DDI_Id_global_proc(int);
   int         DDI_Id_global_node(int);
   int         DDI_Id_local_proc(int);
   int         DDI_Id_local_node(int);
   const char *Info();


/* ---------------------------------------------- *\
   Private Subroutine Prototypes -- Communicators
\* ---------------------------------------------- */
   void  Comm_init();
   void *Comm_find(int);
   void *Comm_find_end();

   void *Data_find(int);
   void *Data_find_end();

   int  Comm_size(int);
   int  Comm_rank(int);
   int  Comm_smp_size(int);
   int  Comm_smp_rank(int);
   void Comm_print(const DDI_Comm *);
  
   void Comm_send(void*,size_t,int,const DDI_Comm*);
   void Comm_recv(void*,size_t,int,const DDI_Comm*);
   void Comm_send_(void*,size_t,int,const DDI_Comm*);
   void Comm_recv_(void*,size_t,int,const DDI_Comm*);

   void Comm_sync(int,const DDI_Comm*);
   void Comm_sync_list(const DDI_List*);
   void Comm_sync_smp(const DDI_Comm*);
   void Comm_sync_node(int,const DDI_Comm*);

   void Comm_gsum(void*,size_t,int,const DDI_Comm*);
   void Comm_gsum_smp(void*,size_t,int,const DDI_Comm*);
   void Comm_gsum_node(void*,size_t,int,const DDI_Comm*);
   void Comm_gsum_list(void*,size_t,void*,size_t,int,const DDI_List*);

   void Comm_bcast(void*,size_t,int,const DDI_Comm *);
   void Comm_bcast_smp(void*,size_t,int,const DDI_Comm*);
   void Comm_bcast_node(void*,size_t,int,const DDI_Comm*);
   void Comm_bcast_list(void*,size_t,const DDI_List*);

   void Comm_send_list(void*,size_t,int,const DDI_List*);
   void Comm_recv_list(void*,size_t,int,const DDI_List*);
   void Comm_send_list_(void*,size_t,int,const DDI_List*);
   void Comm_recv_list_(void*,size_t,int,const DDI_List*);

   void Comm_divide(int,int,int*);
   void Comm_divide_custom(int,int*,int,int*);
   void Comm_create(int,int*,int,int,int,int*);

   void Vec_sum_d(double*,const double*,size_t);
   void Vec_sum_l(long*,const long*,size_t);
   void Vec_sum_i(int*,const int*,size_t);


/* ---------------------------------------------- *\
   Private Subroutine Prototypes -- Shared-Memory
\* ---------------------------------------------- */
   void  SMP_init();
   void *SMP_find(int);
   void *SMP_find_end();

   int  SMP_create(size_t);
   void SMP_destroy(int);
   

/* ---------------------------------------------- *\
   Private Subroutine Prototypes -- Shared-Memory
\* ---------------------------------------------- */
   void DB_Init();
   void DB_Close();
   int  DB_Create(size_t);
   void DB_Read(int,size_t,void*);
   void DB_Write(int,size_t,void*);

   void DB_Create_server(DDI_Patch*,int);
   void DB_Read_server(DDI_Patch*,int);
   void DB_Write_server(DDI_Patch*,int);


/* ------------------------------------------------ *\
   Private Subroutine Prototypes -- Signal Handlers
\* ------------------------------------------------ */
 # if defined DDI_SOC
   void _sigurg_hndlr(int);
 # endif
 
/* DDI ARMCI implemetation header */
#if defined DDI_ARMCI
#include "ddi_armci.h"
#endif

/* DDI Blue Gene/L header */
#ifdef DDI_BGL
#include "ddi_bgl.h"
#endif

/* Runtime functions. */
#include "ddi_runtime.h"

 # ifdef DDI_ONESIDED
 # include "ddi-onesided.h"
 # endif

 # endif
