/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Used to determine rank and number of nodes within the working scope.
 *
 * Author: Ryan M. Olson
 * CVS $Id: ddi_comm_nnode.c,v 1.3 2006/11/02 00:28:40 andrey Exp $
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"


/* ------------------------------------------------------------------ *\
   DDI_NNode(NP,MY)
   ================
   [OUT] NP - Number of Processors within current communication scope
   [OUT] ME - Rank of calling processor within current comm. scope.

   using working communicator
\* ------------------------------------------------------------------ */
   void DDI_NNode(int *nn,int *my) {
    # if defined DDI_SOC || defined DDI_MPI
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);
      *nn = comm->nn;
      *my = comm->my;
    # endif

    # if defined DDI_SEQ
      *nn = 1;
      *my = 0;
    # endif

   }
 
/* ------------------------------------------------------------------ *\
   DDI_Comm_nnode(NP,MY,COMMID)
   ============================
   [OUT] NP - Number of Processors within current communication scope
   [OUT] ME - Rank of calling processor within current comm. scope.
   [IN] COMMID - Communicator Id.
\* ------------------------------------------------------------------ */
   void DDI_NNode_comm(int commid,int *nn,int *my) {

    # if FULL_SMP || DDI_ARMCI_SMP
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(commid);
      *nn = comm->nn;
      *my = comm->my;
    # else
      DDI_NProc_comm(commid,nn,my);
    # endif

    # if defined DDI_SEQ
      *nn = 1;
      *my = 0;
    # endif

   }
 
