/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Subroutines to control dynamic load-balancing.
 *
 * Author: Ryan M. Olson
 * CVS $Id: ddi_dlb.c,v 1.8 2006/11/28 03:06:32 andrey Exp $
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"

 # if defined DDI_SHMEM && defined CRAY_MPI
   long shmem_lb_counter = 0;
 # endif 

/* -------------------------------------------------------- *\
   DDI_DLBRest()
   ==============
   
   Reset the global dynamic load-balance counter.
\* -------------------------------------------------------- */
   void DDI_DLBReset() {

      int np,me;
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);


    # if !defined USE_SYSV
      int remote_id = 0;
      DDI_Patch Patch;
    # endif

      DEBUG_ROOT(LVL1,(stdout," DDI: reseting dynamic load-balance counter.\n"))
      DEBUG_OUT(LVL3,(stdout,"%s: entering ddi_dlbreset.\n",DDI_Id()))

    # ifdef DS_SIGNAL
      if(comm->me_local == 1) {
         signal(SIGALRM,DS_Thread_main);
      }
    # endif

      DDI_NProc(&np,&me);
      Comm_sync(3021,comm);

    # if defined(DDI_ONESIDED)
      cpCounterReset();
    # elif defined DDI_ARMCI
      DDI_ARMCI_DLBReset();
    # else
      if(me == 0) {
       # if defined USE_SYSV
         DDI_DLBReset_local();
       # else
         Patch.oper = DDI_DLBRESET;
         DDI_Send_request(&Patch,&remote_id,NULL);
       # endif
      }
    # endif

      Comm_sync(3022,comm);

      DEBUG_OUT(LVL3,(stdout,"%s: leaving ddi_dlbreset.\n",DDI_Id()))
      DEBUG_ROOT(LVL2,(stdout," DDI: dynamic load-balance counter reset.\n"))
   }


/* -------------------------------------------------------- *\
   DDI_DLBRest_local()
   ===================
   
   Subroutine that actually resets the global counter.  May
   be called by the master compute process (if FULL_SMP),
   otherwise by the master data server (if !FULL_SMP).
\* -------------------------------------------------------- */
   void DDI_DLBReset_local() {
    # if defined DDI_LAPI
      gv(lapi_dlb_cntr) = 0;
    # elif defined DDI_SHMEM && defined CRAY_MPI
      DDI_Sem_acquire(gv(dlb_access),0,DDI_WRITE_ACCESS);
      shmem_lb_counter = 0;
      *gv(dlb_counter) = 0;
      DDI_Sem_release(gv(dlb_access),0,DDI_WRITE_ACCESS);
    # else
    # if FULL_SMP
      DDI_Sem_acquire(gv(dlb_access),0,DDI_WRITE_ACCESS);
      *gv(dlb_counter) = 0;
      DDI_Sem_release(gv(dlb_access),0,DDI_WRITE_ACCESS);
    # else
      *gv(dlb_counter) = 0;
    # endif
    # endif
   }


/* ---------------------------------------------------------- *\
   DDI_DLBNext(counter)
   ====================
   [OUT] counter - value of the load balance counter returned
                   to the calling process.

   An atomic operation that sets the value of counter to the
   value of the global load-balance counter, then increments
   the global counter.
\* --------------------------------------------------------- */
   void DDI_DLBNext(size_t *counter) {
      int nn,my,remote_id = 0;
      DDI_Patch Patch;
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);

      DEBUG_OUT(LVL3,(stdout,"%s: calling DDI_DLBNext\n",DDI_Id()))
    
    # if defined(DDI_ONESIDED)
      cpFetchAdd(counter);
      return;
    # endif

    # ifdef DS_SIGNAL
//    if(comm->me_local == 1) {
//       signal(SIGALRM,DS_Thread_main);
//    }
    # endif

    # if defined DDI_LAPI
      int me_global;
      lapi_cntr_t org_cntr;
      me_global         = comm->global_pid[0];
      uint tgt          = gv(lapi_map)[me_global];
      int *tgt_var      = gv(lapi_dlb_cntr_addr)[tgt];
      int  in_val       = 1;
      int  prev_tgt_val = -1;
    # endif
      size_t offset;
     
      DDI_NNode(&nn,&my);

    # if defined DDI_ARMCI
      DDI_ARMCI_DLBNext(counter);
      return;
    # endif

    # if defined DDI_LAPI
      if(LAPI_Setcntr(gv(lapi_hnd),&org_cntr,0) != LAPI_SUCCESS) {
         fprintf(stdout,"%s: LAPI_Setcntr failed in DDI_DLBNext.\n",DDI_Id());
         Fatal_error(911);
      }
      
      if(LAPI_Rmw(gv(lapi_hnd),FETCH_AND_ADD,tgt,tgt_var,&in_val,
                    &prev_tgt_val,&org_cntr) != LAPI_SUCCESS) {
         fprintf(stdout,"%s: LAPI_Rmw failed in DDI_DLBNext.\n",DDI_Id());
         Fatal_error(911);
      }
      
      if(LAPI_Waitcntr(gv(lapi_hnd),&org_cntr,1,NULL) != LAPI_SUCCESS) {
         fprintf(stdout,"%s: LAPI_Waitcntr failed in DDI_DLBNext.\n",DDI_Id());
         Fatal_error(911);
      }
      
      if(prev_tgt_val == -1) {
         fprintf(stdout,"%s: LAPI version of DDI_DLBNext is not working correctly.\n",DDI_Id());
         Fatal_error(911);
      } else {
         *counter = (size_t) prev_tgt_val;
      }
    # elif defined DDI_SHMEM && defined CRAY_MPI
   /* ------------------------------------------------------------ *\
      The Cray HD workaround is needed to overcome some problems
      in Portals HD that causes the program to hang.
   \* ------------------------------------------------------------ */
    # ifdef CRAY_HD_WORKAROUND
      if(gv(nd)) {
        if(my == 0) {
           DDI_DLBNext_local(counter);
        } else {
           Patch.oper = DDI_DLBNEXT;
           DDI_Send_request(&Patch,&remote_id,NULL);
           Comm_recv(counter,sizeof(size_t),remote_id,comm);
        }
      } else {
    # endif
        *counter = (size_t) shmem_long_finc(&shmem_lb_counter,0);
    # ifdef CRAY_HD_WORKAROUND
      }
    # endif
    # else
    # if FULL_SMP
      if(my == 0) {
         DDI_DLBNext_local(counter);
      } else {
         Patch.oper = DDI_DLBNEXT;
       # if defined(DDI_ONESIDED)
         cpReqInit(remote_id, &cos_req[0]);
         cpPrePostRecv(counter, sizeof(size_t), &cos_req[0]);
         cpCopyLocalDataMDesc(&cos_req[0], &Patch.tag.response_mdesc);
         cpReqSend(&Patch, sizeof(DDI_Patch), &cos_req[0]);
         cpReqWait(&cos_req[0]);
       # else
         DDI_Send_request(&Patch,&remote_id,NULL);
         Comm_recv(counter,sizeof(size_t),remote_id,comm);
       # endif
      }
    # else
      Patch.oper = DDI_DLBNEXT;
      DDI_Send_request(&Patch,&remote_id,NULL);
      Comm_recv(counter,sizeof(size_t),remote_id,comm);
    # endif
    # endif

      DEBUG_OUT(LVL3,(stdout,"%s: finishing DDI_DLBNext\n",DDI_Id()))
   }


/* ---------------------------------------------------------- *\
   DDI_DLBNext_local(counter)
   ==========================
   [OUT] counter - value of the load balance counter returned
                   to the calling process.

   Subroutine that performs the DDI_DLBNext operation.  May
   be called from the master compute process (if FULL_SMP)
   or from the master data server (if !FULL_SMP).
\* --------------------------------------------------------- */
   void DDI_DLBNext_local(size_t *counter) {

      size_t tmp_val;
  
    # if FULL_SMP
      DDI_Sem_acquire(gv(dlb_access),0,DDI_WRITE_ACCESS);
    # endif

      tmp_val          = *gv(dlb_counter);
      *counter         = tmp_val++;
      *gv(dlb_counter) = tmp_val;

    # if FULL_SMP
      DDI_Sem_release(gv(dlb_access),0,DDI_WRITE_ACCESS);
    # endif

   }
 
