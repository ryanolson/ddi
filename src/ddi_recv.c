/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Global objects
 * 
 * Author: Ryan M. Olson
 * CVS $Id: ddi_recv.c,v 1.5 2006/05/27 17:10:00 ryan Exp $
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"
 

/* ------------------------------------------------------------------------- *\
   DDI_Recv_request(buff,from)
   ===========================
   [OUT] buff - address in which incoming data is passed.
   [IN]  from - rank of process sending data stream.
   
   Used to receive a data request sent by DDI_Send_request.  This subroutine
   is only called from a data server.
\* ------------------------------------------------------------------------- */
   void DDI_Recv_request(void *buff,int *from) {

    # if defined DDI_SOC
      char ack=37;
      size_t size = sizeof(DDI_Patch);
      DDI_Recvany(buff,size,from);
      Send(gv(sockets)[*from],&ack,1,0);
    # endif

   /* ---------------------------------- *\
      The stand-alone MPI version of DDI
   \* ---------------------------------- */

   /*
      This routine is the one that is responsible for the poor
      performance of a 100% MPI-1 model.  The "receive from anywhere"
      option caused by MPI_ANY_SOURCE typically is implemented by
      repeated checking (polling) on all open MPI processes.  This
      can sometimes be influenced by looking for options to control
      the polling mechanism, at the "mpirun" time.  However, a more
      practical solution is to use the "mixed" model, which uses
      our TCP/IP code for handling the small control messages, which
      do no polling at all.  If your adapter allows TCP/IP to coexist
      with the MPI-1, by all means try "mixed" over "mpi" for DDI.

      Here's a short note from Ryan, 
         Having talking to some MPI people at past SC conferences,
         it seems that the Irecv/wait combination has the best 
         chance to be implemented in a blocking fashion.  However, 
         I never worked out all the details to make it happen.

      What I think this means is that you set up an IRECV on every
      possible source, and then loop around "polling" with the wait
      yourself, perhaps with a delay from "sleep" added before the
      loop repeats itself.

    */

    # if defined DDI_MPI && !defined DDI_SOC & !defined DDI_LAPI
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);
      MPI_Status status;
      size_t size = sizeof(DDI_Patch);
      DEBUG_OUT(LVL4,(stdout,"%s: call mpi_recv from any source.\n",DDI_Id()))
      MPI_Recv(buff,size,MPI_BYTE,MPI_ANY_SOURCE,0,comm->world_comm,&status);
      *from = status.MPI_SOURCE;
      DEBUG_OUT(LVL4,(stdout,"%s: received request from %i.\n",DDI_Id(),*from))
    # endif

   }
