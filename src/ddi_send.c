/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Subroutines associated with the send operation.
 *
 * Author: Ryan M. Olson
 * CVS $Id: ddi_send.c,v 1.4 2006/05/27 03:55:48 ryan Exp $
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"


/* --------------------------------------------------------- *\
   DDI_Send_request(buff,to)
   =========================
   [IN] buff - address containing a DDI_Patch object.
   [IN] to   - target DDI process to send the data request.
   
   This subroutine is only called by a compute process, and
   is used only to transmit a data request to a data server.
   It uses TCP/IP, MPI-1, or LAPI to issue the data request.
\* --------------------------------------------------------- */
   void DDI_Send_request(void *buff,int *to,DDI_Request *req) {
      DDI_Send_request_comm(buff,to,req,DDI_WORKING_COMM);
   }
   
   void DDI_Send_request_comm(void *buff,int *to,DDI_Request *req, int commid) {
      char ack;
      size_t size = sizeof(DDI_Patch);
      int i,np,me,nn,my;

      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(commid);
    
      np = comm->np;
      me = comm->me;
      nn = comm->nn;
      my = comm->my; 

      *to = comm->global_dsid[*to];
      DEBUG_OUT(LVL3,(stdout,"%s: sending request to global process %i.\n",DDI_Id(),*to))

   /* ------------------------------------------------------------ *\
      Using TCP/IP sockets, this is always a synchronous operation
   \* ------------------------------------------------------------ */
    # if defined DDI_SOC
      Send(gv(sockets)[*to],buff,size,0);
      Recv(gv(sockets)[*to],&ack,1,0);
    # endif


   /* -------------------------------------------------------------- *\
      Using LAPI, this sends an active message to the target process
      causing an interrupt signal to be issued.  The target process
      now acts like a data server and handles the data request. This
      call is non-blocking, because the once the active message is
      sent, the target process is in control of the data and the 
      originating compute process only needs to wait until all the
      target process have finished and tells 'this' originating
      process that it can continue.  These are slightly different
      for get, put and accumulates.
   \* -------------------------------------------------------------- */
    # if defined DDI_LAPI
      DDI_Patch *patch = (DDI_Patch *) buff;
      uint tgt = gv(lapi_map)[*to];
      void *hdr_hndlr = (void *) gv(am_hndlr)[tgt];
      void *udata = NULL;
      ulong udata_len = 0;
      lapi_cntr_t *org_cntr = (lapi_cntr_t *) patch->cp_lapi_cntr;
      lapi_cntr_t *tgt_cntr = NULL;
      lapi_cntr_t *cmpl_cntr = NULL;
   
      if(LAPI_Amsend(gv(lapi_hnd),tgt,hdr_hndlr,buff,size,udata,udata_len,
                     tgt_cntr,org_cntr,cmpl_cntr) != LAPI_SUCCESS) {
          fprintf(stdout,"%s: lapi_amsend error in ddi_send_request.\n",DDI_Id());
          Fatal_error(911);
      }
    # endif


   /* ---------------------------------- *\
      The stand-alone MPI version of DDI
   \* ---------------------------------- */
    # if defined(DDI_MPI)
    #  if defined(DDI_ONESIDED)
       assert(0);
       cosError("this should not be called",911);
    #  elif defined(CRAY_MPI)
       if(req == NULL) {
          MPI_Send(buff,size,MPI_BYTE,*to,37,comm->world_comm);
       } else {
          MPI_Isend(buff,size,MPI_BYTE,*to,37,comm->world_comm,req);
       }
    #  elif !defined(DDI_SOC) && !defined(DDI_LAPI)
       DEBUG_OUT(LVL3,(stdout,"%s: calling mpi_ssend.\n",DDI_Id()))
       MPI_Ssend(buff,size,MPI_BYTE,*to,0,comm->world_comm);
    #  endif
    # endif

   /* ------------------------------------------------------------- *\
      Reverse look up the group rank of the global rank to which to
      the data request is being sent.
   \* ------------------------------------------------------------- */
   /*
      if(gv(scope) == DDI_GROUP) {
         for(i=0; i<np; i++) if(DDI_Id_global_proc(i+np) == *to) *to = i+np;
      }
   */

      DEBUG_OUT(LVL3,(stdout,"%s: leaving ddi_send_request.\n",DDI_Id()))

   }

