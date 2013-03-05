/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Prototypes and Data Structures
 * 
 * Author: Ryan M. Olson
 * CVS Version: $Id: ddi.h,v 1.4 2006/06/19 23:33:50 ryan Exp $
\* -------------------------------------------------------------------- */
 # ifndef __DDI_H__
 # define __DDI_H__

 # include <stdio.h>
 # include <stdlib.h>
 # include "debug.h"

 # ifdef DDI_ONESIDED
 # include "ddi-onesided.h"
 # endif

 # define DDI_VERSION 3

/* ----------------- *\
   DDI Communicators
\* ----------------- */
 # define DDI_COMM_WORLD 0
 # define DDI_WORKING_COMM gv(ddi_working_comm)

/* -------------- *\
   DDI Data Types
\* -------------- */
 # define DDI_DOUBLE 0
 # define DDI_INT    sizeof(int)
 # define DDI_LONG   sizeof(long)


/* ------------------- *\
   DDI Data Structures
\* ------------------- */
   typedef struct {
     int oper;
     int handle;
     int ilo;
     int ihi;
     int jlo;
     int jhi;
     size_t size;
     
   # if defined DDI_LAPI
     int    cp_lapi_id;
     void  *cp_lapi_cntr;
     void  *cp_buffer_addr;
     size_t ds_buffer_size;
   # endif

   # if defined DDI_ONESIDED
     msg_tag_t tag;
   # endif
     
   } DDI_Patch;


/* ----------------------- *\
   DDI Function Prototypes
\* ----------------------- */
   void DDI_Init(int,char**);
   void DDI_Memory(size_t);
   void DDI_Finalize();
   void DDI_Sync(int);
   void DDI_BCast(void*,size_t,int);
   void DDI_GSum(void*,size_t,int);
   void DDI_Create(int,int,int*);
   void DDI_Create_custom(int,int,int*,int*);
   void DDI_Destroy(int);
   void DDI_Zero(int);
   void DDI_Output(int);
   void DDI_Get(int,int,int,int,int,void*);
   void DDI_Put(int,int,int,int,int,void*);
   void DDI_Acc(int,int,int,int,int,void*);
   void DDI_GetAcc(int,int,int,int,int,void*);
   void DDI_GetP(int,DDI_Patch*,void*);
   void DDI_PutP(int,DDI_Patch*,void*);
   void DDI_AccP(int,DDI_Patch*,void*);   
   void DDI_GetAccP(int,DDI_Patch*,void*);   
   void DDI_DLBReset();
   void DDI_DLBNext(size_t*);
   void DDI_Send(void*,size_t,int);
   void DDI_Recv(void*,size_t,int);
   void DDI_Recvany(void*,size_t,int*);

   void DDI_GetP_comm(int,DDI_Patch*,void*,int);
   void DDI_PutP_comm(int,DDI_Patch*,void*,int);

/* ----------------- *\
   New SMP Functions
\* ----------------- */
   void DDI_SMP_NProc(int*,int*);
   void DDI_SMP_Create(size_t,int*);
   void DDI_SMP_Destroy(int);
   void *DDI_SMP_Array(int);

/* ---------------------------- *\
   New non-blocking subroutines
\* ---------------------------- */
   void DDI_ISend(void *,size_t,int,int*);
   void DDI_IRecv(void *,size_t,int,int*);
   void DDI_Wait(int);

/* ---------------------- *\
   New Group DDI routines
\* ---------------------- */
   void DDI_Scope(int);
   void DDI_Group_create(int,int*,int*,int*);
   void DDI_Group_create_custom(int,int*,int*,int*,int*);
   void DDI_GDLBReset();
   void DDI_GDLBNext(size_t*);

/* -------------------------------------- *\
   New dynamic load-balancing subroutines
\* -------------------------------------- */
   void DDI_ProcDLB_create(int*);
   void DDI_ProcDLB_destroy(int);
   void DDI_ProcDLB_reset(int);
   void DDI_ProcDLB_next(int,int,int*);

/* ---------------------- *\
   New timing subroutines
\* ---------------------- */
   void DDI_Timer_reset();
   void DDI_Timer_output();

/* -------------------------- *\
   Possible Inlined Functions
\* -------------------------- */
   void DDI_NProc(int*,int*);
   void DDI_NNode(int*,int*);
   void DDI_NGroup(int*,int*);
   void DDI_Distrib(int,int,int*,int*,int*,int*);
   void DDI_DistribP(int,int,DDI_Patch*);
   void DDI_NDistrib(int,int,int*,int*,int*,int*);
   void DDI_NDistribP(int,int,DDI_Patch*);


/* -------------------------- *\
   DDI Communicator Routines
\* -------------------------- */
   void DDI_Comm_divide(int,int,int*);
   void DDI_Comm_create(int,int*,int,int,int,int*);
   void DDI_Comm_destory(int);

   void DDI_NProc_comm(int,int*,int*);
   void DDI_NNode_comm(int,int*,int*);
   void DDI_Sync_comm(int,int);
   void DDI_GSum_comm(void*,size_t,int,int);
   void DDI_Send_comm(void*,size_t,int,int);
   void DDI_Recv_comm(void*,size_t,int,int);
   void DDI_BCast_comm(void*,size_t,int,int);

   void DDI_Sync_smp(int);
   void DDI_GSum_smp(void*,size_t,int);
   void DDI_BCast_smp(void*,size_t,int);

   void DDI_Sync_node(int);
   void DDI_GSum_node(void*,size_t,int);
   void DDI_BCast_node(void*,size_t,int);

/* -------------- *\
   DDI Parameters
\* -------------- */
 # define DDI_WORLD   0
 # define DDI_GROUP   1
 # define DDI_MASTERS 2


/* -------------- *\
   DDI Operations
\* -------------- */
 # define DDI_MEMORY      0
 # define DDI_CREATE      1
 # define DDI_DESTROY     2
 # define DDI_GET         3
 # define DDI_PUT         4
 # define DDI_ACC         5
 # define DDI_GETACC      6
 # define DDI_DLBRESET    7
 # define DDI_DLBNEXT     8
 # define DDI_GDLBRESET   9
 # define DDI_GDLBNEXT   10
 # define DDI_QUIT       11
 # define DDI_ZERO       12 
 # define DDI_FENCE      13
 # define DDI_DEBUGFLAG  20
 # define DB_CREATE_ENTRY  30
 # define DB_READ_ENTRY    31
 # define DB_WRITE_ENTRY   32

 # endif
