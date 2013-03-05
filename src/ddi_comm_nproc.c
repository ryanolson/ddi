/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Used to determine rank and number of compute processes within the
 * working scope.
 *
 * Author: Ryan M. Olson
 * CVS $Id: ddi_comm_nproc.c,v 1.3 2006/11/02 00:28:41 andrey Exp $
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"
 
 
/* -------------------------------------------------------------------- *\
   DDI_NProc(NP,ME)
   ================
   [OUT] NP    - Number of Processors within the current communicator.
   [OUT] ME    - Rank of calling process within the current communicator.

   using the working communicator.
\* -------------------------------------------------------------------- */
   void DDI_NProc(int *np,int *me) {
      
    # if defined DDI_SOC || defined DDI_MPI
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);
      *np = comm->np;
      *me = comm->me;
    # endif

    # if defined DDI_SEQ
      *np = 1;
      *me = 0;
    # endif

   }

/* -------------------------------------------------------------------- *\
   DDI_NProc_comm(comm,np,me)
   ==========================
   [OUT] NP    - Number of Processors within the current communicator.
   [OUT] ME    - Rank of calling process within the current communicator.
   [IN] COMMID - Communicator ID.
\* -------------------------------------------------------------------- */
   void DDI_NProc_comm(int comm_id,int *np,int *me) {
      
    # if defined DDI_SOC || defined DDI_MPI
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(comm_id);
      *np = comm->np;
      *me = comm->me;
    # endif

    # if defined DDI_SEQ
      *np = 1;
      *me = 0;
    # endif

   }


/* -------------------------------------------------------------------- *\
   Comm_size(COMMID)
   =================
   [IN] COMMID - Communicator Id.
   [RETURN]    - Number of compute processes in the communicator.
\* -------------------------------------------------------------------- */
   int Comm_size(int commid) {

    # if defined DDI_SOC || defined DDI_MPI
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(commid);
      return comm->np;
    # endif

    # if defined DDI_SEQ
      return 1;
    # endif

   }


/* -------------------------------------------------------------------- *\
   Comm_rank(COMMID)
   =================
   [IN] COMMID - Communicator Id.
   [RETURN]    - Rank of the calling process within the communicator.
\* -------------------------------------------------------------------- */
   int Comm_rank(int commid) {

    # if defined DDI_SOC || defined DDI_MPI
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(commid);
      return comm->me;
    # endif

    # if defined DDI_SEQ
      return 0;
    # endif

   }


/* -------------------------------------------------------------------- *\
   DDI_SMP_NProc(SMPNP,SMPME)
   ==========================
   [OUT] SMPNP - Number of Processors in scope on current node
   [OUT] SMPME - Rank of calling processor in scope on current node.
   [IN] COMMID - Communicator Id.

   using working communicator
\* -------------------------------------------------------------------- */
   void DDI_SMP_NProc(int* smpnp,int *smpme) {

    # if FULL_SMP || DDI_ARMCI_SMP
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);
      *smpnp = comm->np_local;
      *smpme = comm->me_local;
    # else
      *smpnp = 1;
      *smpme = 0;
    # endif

   }
  

/* -------------------------------------------------------------------- *\
   DDI_Comm_smp_nproc(SMPNP,SMPME,COMMID)
   ======================================
   [OUT] SMPNP - Number of Processors in scope on current node
   [OUT] SMPME - Rank of calling processor in scope on current node.
   [IN] COMMID - Communicator Id.
\* -------------------------------------------------------------------- */
   void DDI_Comm_smp_nproc(int* smpnp,int *smpme,int commid) {

    # if FULL_SMP || DDI_ARMCI_SMP
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(commid);
      *smpnp = comm->np_local;
      *smpme = comm->me_local;
    # else
      *smpnp = 1;
      *smpme = 0;
    # endif

   }


/* -------------------------------------------------------------------- *\
   Comm_smp_size(COMMID)
   =====================
   [IN] COMMID - Communicator Id.
   [RETURN]    - Number of 'local' compute processes on the calling
                 node in the communicator COMMID
\* -------------------------------------------------------------------- */
   int Comm_smp_size(int commid) {

    # if defined DDI_SOC || defined DDI_MPI
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(commid);
      return comm->np_local;
    # endif

    # if defined DDI_SEQ
      return 1;
    # endif

   }


/* -------------------------------------------------------------------- *\
   Comm_smp_rank(COMMID)
   =====================
   [IN] COMMID - Communicator Id.
   [RETURN]    - Rank of the 'local' compute process with the calling
                 node within the communicator COMMID.
\* -------------------------------------------------------------------- */
   int Comm_smp_rank(int commid) {

    # if defined DDI_SOC || defined DDI_MPI
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(commid);
      return comm->me_local;
    # endif

    # if defined DDI_SEQ
      return 0;
    # endif

   }
