/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * 
 * 
 * Author: Ryan M. Olson
 * CVS $Id: ddi_id.c,v 1.2 2006/05/23 20:17:50 ryan Exp $
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"


/* -------------------------------------------------------------------- *\
   DDI_Id
   ======
   
   Returns a string with process id.  Usually used for debugging.
\* -------------------------------------------------------------------- */
   const char *DDI_Id() {
      int np,me;
      static char str[256];
      const DDI_Comm *comm = (DDI_Comm *) &gv(ddi_base_comm);
   
      DDI_NProc(&np,&me);
   
      if(gv(ddi_working_comm) == DDI_COMM_WORLD) {
         sprintf(str," DDI Process %i",me);
         return str;
      }
   
      sprintf(str," DDI Process %i (%i)",me,comm->me);
      return str;
   }

/* -------------------------------------------------------------------- *\
   DDI_Patch_info(patch)
   =====================
   [IN] patch -- address of patch
   
   Returns a string containing the dimensions of the patch
\* -------------------------------------------------------------------- */
 # if DDI_DEBUG
   const char *DDI_Patch_info(const DDI_Patch *p) {
      static char str[256];
      sprintf(str,"Array[%i] {%i,%i,%i%i}",p->handle,p->ilo,p->ihi,p->jlo,p->jhi);
      return str;
   }
 # endif

