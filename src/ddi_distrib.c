/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Used to determine which process or node owns which portions of a
 * distributed array.  
 *
 * Author: Ryan M. Olson
 * CVS $Id: ddi_distrib.c,v 1.3 2006/11/02 00:28:41 andrey Exp $
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"
 

/* -------------------------------------------------------- *\
   DDI_Distrib(handle,rank,ilo,ihi,jlo,jhi)
   ========================================
   [IN]  handle - handle of array
   [IN]  rank   - rank of compute process
   [OUT] ilo    - lowest row dimension found on rank.
   [OUT] ihi    - highest row dimension found on rank.
   [OUT] jlo    - lowest column dimension found on rank.
   [OUT] jhi    - highest column dimension found on rank.
\* -------------------------------------------------------- */
   void DDI_Distrib(int handle,int rank,int *ilo,int *ihi,int *jlo,int *jhi) {
      DDI_Patch patch;
      DDI_DistribP(handle,rank,&patch);
      
      *ilo = patch.ilo;
      *ihi = patch.ihi;
      *jlo = patch.jlo;
      *jhi = patch.jhi;
   }


/* -------------------------------------------------------- *\
   DDI_DistribP(handle,rank,patch)
   ===============================
   [IN]  handle - handle of array
   [IN]  rank   - rank of compute process
   [OUT] patch  - patch of array 'handle' stored on compute
                - process 'rank'
\* -------------------------------------------------------- */
   void DDI_DistribP(int handle,int rank,DDI_Patch *patch) {
       patch->handle = handle;
       patch->ilo    = 0;
       patch->ihi    = gv(nrow)[handle]-1;
       patch->jlo    = gv(pcmap)[handle][rank];
       patch->jhi    = gv(pcmap)[handle][rank+1]-1;
   }


/* -------------------------------------------------------- *\
   DDI_NDistrib(handle,rank,ilo,ihi,jlo,jhi)
   =========================================
   [IN]  handle - handle of array
   [IN]  rank   - rank of a node
   [OUT] ilo    - lowest row dimension found on rank.
   [OUT] ihi    - highest row dimension found on rank.
   [OUT] jlo    - lowest column dimension found on rank.
   [OUT] jhi    - highest column dimension found on rank.
\* -------------------------------------------------------- */
   void DDI_NDistrib(int handle,int rank,int *ilo,int *ihi,int *jlo,int *jhi) {
      DDI_Patch patch;
      DDI_NDistribP(handle,rank,&patch);
      
      *ilo = patch.ilo;
      *ihi = patch.ihi;
      *jlo = patch.jlo;
      *jhi = patch.jhi;
   }


/* -------------------------------------------------------- *\
   DDI_NDistribP(handle,rank,patch)
   ================================
   [IN]  handle - handle of array
   [IN]  rank   - rank of a node
   [OUT] patch  - patch of array 'handle' stored on node
                - 'rank'
\* -------------------------------------------------------- */
   void DDI_NDistribP(int handle,int rank,DDI_Patch *patch) {
     # if FULL_SMP || DDI_ARMCI_SMP
       patch->handle = handle;
       patch->ilo    = 0;
       patch->ihi    = gv(nrow)[handle]-1;
       patch->jlo    = gv(ncmap)[handle][rank];
       patch->jhi    = gv(ncmap)[handle][rank+1]-1;
     # else
       DDI_DistribP(handle,rank,patch);
     # endif
   }

