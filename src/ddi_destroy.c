/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Collective operation used to remove/destroy a distributed array.
 *
 * Author: Ryan M. Olson
 * CVS $Id: ddi_destroy.c,v 1.3 2006/11/02 00:28:41 andrey Exp $
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"


/* -------------------------------------------------------- *\
   DDI_Destroy(handle)
   ===================
   [IN] handle - Handle of the array to destroy.
\* -------------------------------------------------------- */   
   void DDI_Destroy(int handle) {
   
   /* --------------- *\
      Local Variables
   \* --------------- */
      int np,me,nn,my;
# ifndef USE_SYSV
      int remote_id;
# endif
      DDI_Patch msg;
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);

      DDI_NProc(&np,&me);
      DDI_NNode(&nn,&my);

# ifndef USE_SYSV
      remote_id = my;
# endif

      DEBUG_ROOT(LVL1,(stdout," DDI: Entering DDI_Destroy(%i).\n",handle))
      DEBUG_OUT(LVL3,(stdout,"%s: Entering DDI_Destroy.\n",DDI_Id()))

   /* ------------------------- *\
      Synchronize DDI processes
   \* ------------------------- */
      Comm_sync(3011,comm);


   /* --------------------- *\
      Common error handling
   \* --------------------- */
      if(me==0) {
         if(handle >= gv(ndda) || handle < 0) {
            fprintf(stderr," DDI Error in DDI_Destroy:  Array[%i] does not exist.\n",handle);
            Fatal_error(911);
         }
      
         if(handle+1 != gv(ndda)) {
            fprintf(stderr,"\n");
            fprintf(stderr," DDI Warning: Destroying arrays out of sequence!\n");
            fprintf(stderr," DDI_DESTROY was called to delete array %i,\n",handle);
            fprintf(stderr," but DDI arrays from %i to %i now exist.\n",0,gv(ndda)-1);
            fprintf(stderr," Arrays greater than %i may become corrupt.\n",handle);
            fprintf(stderr,"\n");
            fflush(stderr);
         }

         /* --------------------------------------------------------------------- *\
            TODO: When using groups, check to make sure the array to be destroyed
            was created within the same scope as the DDI_Destroy was called,
            otherwise, this is a serious error. 
         \* --------------------------------------------------------------------- */

      }


   /* -------------------------------------------------------------------------- *\
      Decrement DDI array counter & send DDI_DESTROY command to "my" data server
   \* -------------------------------------------------------------------------- */
      gv(ndda)--;


   /* ------------------------------------ *\
      Destroy the Distributed Memory Array 
   \* ------------------------------------ */
      msg.handle = handle;
      msg.oper   = DDI_DESTROY;

# if defined USE_SYSV || defined DDI_ARMCI
      DDI_Index_destroy(&msg);
# else
      DDI_Send_request(&msg,&remote_id);
# endif

 
   /* --------------------------------- *\
      Synchronize DDI compute processes
   \* --------------------------------- */
      Comm_sync(3012,comm);

      DEBUG_OUT(LVL3,(stdout,"%s: Leaving DDI_Destroy.\n",DDI_Id()))
      DEBUG_ROOT(LVL2,(stdout," DDI: Array[%i] successfully destroyed.\n",handle)) 
   }
