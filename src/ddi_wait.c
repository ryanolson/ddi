 # include "ddi_base.h"

   void DDI_Wait(const int req_val) {

    # if defined DDI_MPI
      MPI_Status status;
    # endif
  
      DDI_Request *req = &gv(irecv_req);
      if(req_val)  req = &gv(isend_req);
 
    # if defined DDI_SOC && !defined DDI_MPI
      if(pthread_join(req->hnd_thread,NULL) == -1) {
         fprintf(stdout,"%s: pthread_join failed in DDI_Wait.\n",DDI_Id());
         Fatal_error(911);
      }
    # endif

    # if defined DDI_MPI
      MPI_Wait(req,&status);
    # endif

   }
