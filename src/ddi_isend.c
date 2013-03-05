/* ------------------------------------------------------------------ *\
   Subroutine DDI_ISend(BUFF,SIZE,TYPE,TO,TAG,REQ)
   ===============================================
   [IN]  BUFF - Buffer to be sent.
   [IN]  SIZE - Size of buffer in bytes.
   [IN]  TO   - Rank of processor (within current scope) to whom the
                message is to be sent.
   
   Author: Ryan M. Olson
   CVS $Id: ddi_isend.c,v 1.2 2006/05/27 03:55:48 ryan Exp $
\* ------------------------------------------------------------------ */
 # include "ddi_base.h"

   static void *DDI_ISend_thread(void *);
 
   void DDI_ISend(void *buffer,size_t size,int to,int *req_val) {

      int global_pid;
      DDI_Request *req = &gv(isend_req);
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);
    
    # if defined DDI_CHECK_ARGS
      if(to < 0 || to > comm->np) {
         fprintf(stdout,"%s: can not send to ddi process %i.\n",DDI_Id(),to);
         Fatal_error(911);
      }
    # endif

      
    # if defined DDI_SOC && !defined DDI_MPI
      pthread_attr_t thread_attr;
      pthread_attr_init(&thread_attr);
      pthread_attr_setscope(&thread_attr,PTHREAD_SCOPE_SYSTEM);
      req->to     = to;
      req->size   = size;
      req->buffer = buffer;
      if(pthread_create(&req->hnd_thread,&thread_attr,DDI_ISend_thread,req) == -1) {
         fprintf(stderr,"%s: pthread_create failed in DDI_ISend.\n",DDI_Id());
         Fatal_error(911);
      }
    # endif
     
    # if defined DDI_MPI
      MPI_Isend(buffer,size,MPI_BYTE,to,1,comm->compute_comm,req);
    # endif
     
      ULTRA_DEBUG((stdout,"%s: non-blocking send to %i issued.\n",DDI_Id(),to))

      *req_val = 1;
   }


 # ifndef DDI_MPI  
   static void *DDI_ISend_thread(void *myarg) {
      DDI_Request *req = (DDI_Request *) myarg;
      DDI_Send(req->buffer,req->size,req->to);
      ULTRA_DEBUG((stdout,"%s: isend_thread finished.\n",DDI_Id()))
      return NULL;
   }  
 # endif
 
