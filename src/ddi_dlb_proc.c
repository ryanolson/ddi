/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Subroutines to control dynamic load-balancing.
 *
 * Author: Ryan M. Olson
 * CVS $Id: ddi_dlb_proc.c,v 1.2 2006/05/27 17:10:00 ryan Exp $
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"

   void DDI_ProcDLB_create(int *hnd) {
      int np,me;
      DDI_NProc(&np,&me);
      DEBUG_ROOT(LVL1,(stdout," DDI: creating process dlb counter.\n"))
      DDI_Create(1,np,hnd);
      DEBUG_ROOT(LVL2,(stdout," DDI: process dlb counter handle=%i.\n",*hnd))
   }

   void DDI_ProcDLB_destroy(int hnd) {
      DEBUG_ROOT(LVL1,(stdout," DDI: destory process dlb counter handle=%i.\n",hnd))
      DDI_Destroy(hnd);
      DEBUG_ROOT(LVL2,(stdout," DDI: process dlb counter handle=%i destroyed.\n",hnd))
   }

   void DDI_ProcDLB_next(int hnd, int proc, int *counter) {
      DDI_Patch patch;
      double increment = 1.0;

      DEBUG_OUT(LVL1,(stdout,"%s: entering procdlb_next.\n",DDI_Id()))

      patch.handle = hnd;
      patch.ilo    = 0;
      patch.ihi    = 0;
      patch.jlo    = proc;
      patch.jhi    = proc;

      DEBUG_OUT(LVL1,(stdout,"%s: procdlb_next calling ddi_getaccp.\n",DDI_Id()))
      DDI_GetAccP(hnd,&patch,&increment);
      *counter = (int) increment;

      DEBUG_OUT(LVL2,(stdout,"%s: leaving procdlb_next.\n",DDI_Id()))
   }

   void DDI_ProcDLB_reset(int hnd) {
      int np,me;
      DDI_Patch patch;
      double zero = 0.0;
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);
      
      DEBUG_ROOT(LVL1,(stdout," DDI: resetting proc dlb counter %i.\n",hnd))
      DEBUG_OUT(LVL3,(stdout,"%s: entering procdlb_reset.\n",DDI_Id()))

      DDI_NProc(&np,&me);
      patch.handle = hnd; 
      patch.ilo    = 0;
      patch.ihi    = 0;
      patch.jlo    = me;
      patch.jhi    = me;

      Comm_sync(3023,comm);
      DEBUG_OUT(LVL1,(stdout,"%s: procdlb_reset calling ddi_putp.\n",DDI_Id()))
      DDI_PutP(hnd,&patch,&zero);
      Comm_sync(3024,comm);

      DEBUG_OUT(LVL3,(stdout,"%s: leaving procdlb_reset.\n",DDI_Id()))
      DEBUG_ROOT(LVL2,(stdout," DDI: proc dlb counter %i reset.\n",hnd))

   }

