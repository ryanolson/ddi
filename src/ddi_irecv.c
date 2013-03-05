/* ------------------------------------------------------------------ *\
   Subroutine DDI_IRecv(BUFF,SIZE,TYPE,FROM,TAG,REQ)
   =================================================
   [IN]  BUFF - Buffer in which to recieve
   [IN]  SIZE - Size of message in bytes.
   [IN]  FROM - Rank of processor (within current scope) from whom the
                message is to be recieved.
   [OUT] REQ  - Request tag.
   
   Author: Ryan M. Olson
   CVS $Id: ddi_irecv.c,v 1.2 2006/05/27 03:55:48 ryan Exp $
\* ------------------------------------------------------------------ */
 # include "ddi_base.h"

   static void *DDI_IRecv_thread(void *);

   void DDI_IRecv(void *buffer,size_t size,int to,int *req_val) {

      DDI_Request *req = &gv(irecv_req);
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);

    # if defined DDI_SOC && !defined DDI_MPI
      pthread_attr_t thread_attr;
      pthread_attr_init(&thread_attr);
      pthread_attr_setscope(&thread_attr,PTHREAD_SCOPE_SYSTEM);
      req->to     = to;
      req->size   = size;
      req->buffer = buffer;
      if(pthread_create(&req->hnd_thread,&thread_attr,DDI_IRecv_thread,req) == -1) {
         fprintf(stderr,"%s: pthread_create failed in DDI_IRecv.\n",DDI_Id());
         Fatal_error(911);
      }
    # endif

    # if defined DDI_MPI
      MPI_Irecv(buffer,size,MPI_BYTE,to,1,comm->compute_comm,req);
    # endif

      *req_val = 0;

   }
 

 # ifndef DDI_MPI 
   static void *DDI_IRecv_thread(void *myarg) {
      DDI_Request *req = (DDI_Request *) myarg;
      DDI_Recv(req->buffer,req->size,req->to);
      ULTRA_DEBUG((stdout,"%s: irecv_thread finished.\n",DDI_Id()))
      return NULL;
   }   
 # endif

