/* ------------------------------------------------------------------ *\
   Subroutine DDI_ZERO(HANDLE)
   ===========================
   Initializes all elements of an array to zero.
   
   Author: Ryan M. Olson
   CVS $Id: ddi_zero.c,v 1.1.1.1 2005/11/17 00:12:34 ryan Exp $
\* ------------------------------------------------------------------ */
 # include "ddi_base.h"
 
   void DDI_Zero(int handle) {
      int np,me;
      DDI_Patch msg;
      
      DDI_NProc(&np,&me);
      DDI_Sync(3091);
      
      msg.oper   = DDI_ZERO;
      msg.handle = handle;
      DDI_Send(&msg,sizeof(DDI_Patch),me+np);
      
      DDI_Sync(3092);
   }


