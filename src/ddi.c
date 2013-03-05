/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Global objects
 * 
 * Author: Ryan M. Olson
 * changed 1/2005 by Dmitri Fedorov and Ikegami-san
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"


/* -------------------------------------- *\
   DDI Global Variables -- Identification
\* -------------------------------------- */
   int *gv(global_proc_map) = NULL;
   int *gv(global_node_map) = NULL;
   int *gv(master_map)      = NULL;

 # ifdef DDI_MPI
   int *np_by_node;
   int *nc_by_node;
   int *nd_by_node;
   int **ranks_by_node;
 # endif

   int gv(np) = -1;  /* total number of ranked processes  */
   int gv(nc) = -1;  /* total number of compute processes */
   int gv(nd) = -1;  /* total number of data servers      */

/* ---------------------------------------- *\
   DDI Global Variables -- Distributed Data
\* ---------------------------------------- */
   int gv(ndda);
   int gv(nrow)[MAX_DD_ARRAYS];
   int gv(ncol)[MAX_DD_ARRAYS];
   int gv(ddatype)[MAX_DD_ARRAYS];
   int gv(dda_output) = 1;
   int gv(ncmap)[MAX_DD_ARRAYS][MAX_NODES+1];
   int gv(pcmap)[MAX_DD_ARRAYS][MAX_PROCESSORS+1];
 
 
/* ------------------------------------------ *\
   DDI Global Variables -- Distributed Memory
\* ------------------------------------------ */
   DDA_Index *gv(dda_index) = NULL;
   void*  gv(rma_addr)[MAX_PROCESSORS];
   size_t gv(rma_offset)[MAX_DD_ARRAYS][MAX_PROCESSORS];
   DDA_Index *gv(smp_index)[MAX_SMP_PROCS];

 # if defined USE_SYSV
   int gv(shmid);
   int gv(dda_access);
   int gv(fence_access);
   int *gv(fence_addr);
   void *gv(buffer_addr);
 # endif

 # if FULL_SMP
   int gv(ddi_sync);
   int gv(ddi_buffer);
 # endif


/* ------------------------------------------ *\
   Global Variables -- Non-blocking Send/Recv
\* ------------------------------------------ */
   DDI_Request gv(isend_req);
   DDI_Request gv(irecv_req);


/* ---------------------------------- *\
   Global Variables -- Load Balancing
\* ---------------------------------- */
   size_t *gv(dlb_counter);
   size_t *gv(gdlb_counter);
   
 # ifdef USE_SYSV
   int gv(dlb_access);
 # endif


/* ------------------------------------ *\
   Global Variables -- TCP/IP Variables
\* ------------------------------------ */
 # if defined DDI_SOC
   int *gv(sockets);
   int  gv(serversock);
   int  gv(serverport);
   int  gv(kickoffsock);
 # endif

   Node_info gv(ddinodes)[MAX_NODES];
   Proc_info gv(ddiprocs)[2*MAX_PROCESSORS];


/* ------------------------------------- *\
   Global Variables -- MPI Communicators
\* ------------------------------------- */
 # if defined DDI_MPI
/*
   MPI_Comm gv(World_comm);
   MPI_Comm gv(Compute_comm);
   MPI_Comm gv(DDI_World_comm);
   MPI_Comm gv(DDI_Compute_comm);
   MPI_Comm gv(SMP_World_comm);
   MPI_Comm gv(SMP_Compute_comm);
   MPI_Comm gv(SMP_Masters_comm);
   MPI_Comm gv(GRP_Compute_comm);
   MPI_Comm gv(GRP_Masters_comm);
*/
 # endif


/* ------------------------------------------------------ *\
   Global Variables -- LAPI Specific Variables & Pointers
\* ------------------------------------------------------ */
 # if defined DDI_LAPI
   lapi_handle_t gv(lapi_hnd);
   int           gv(lapi_dlb_cntr);
   int           gv(lapi_gdlb_cntr);
   uint *        gv(lapi_map);
   void **       gv(am_hndlr);
   int **        gv(lapi_dlb_cntr_addr);
   int **        gv(lapi_gdlb_cntr_addr);
 # endif
 

/* ----------------------- *\
   Simple Profile Counters
\* ----------------------- */
 # ifdef DDI_COUNTERS
   DDI_Profile_counter gv(get_profile);
   DDI_Profile_counter gv(put_profile);
   DDI_Profile_counter gv(acc_profile);
   DDI_Profile_counter gv(bcast_profile);
 # endif


/* ------------------------------ *\
   Global Variables -- DDI Timers
\* ------------------------------ */
   struct rusage  gv(cpu_timer);
   struct timeval gv(wall_timer);

