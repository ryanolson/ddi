/* ------------------------------------------------------------------------ *\
   Debugging Routines
   ==================
   Description:  A common set of macros and functions used for debugging C
   code.  The function DebugOutput is called to set the level of output.
  
   Author: Ryan M. Olson
   Date: November 2002
   CVS $Id: debug.h,v 1.3 2006/05/27 03:55:47 ryan Exp $
\* ------------------------------------------------------------------------ */

/* ----------------------------- *\
   Debugging Function Prototypes
\* ----------------------------- */
   int  DebugValue();
   void DebugOutput(int);
   
/* ---------------- *\
   Debugging Macros
\* ---------------- */
 # define DEBUG_OFF    0
 # define DEBUG_MIN    1
 # define DEBUG_STD    5
 # define DEBUG_MAX   10
 # define DEBUG_ULTRA 15

 # define LVL1         1
 # define LVL2         5
 # define LVL3        10
 # define LVL4        40
 # define LVL5        50
 # define LVL6        60
 # define LVL7        70
 # define LVL8        80
 # define LVL9        90

/* ------------------------- *\
   Debugging Macro Functions
\* ------------------------- */
 # if defined DDI_DEBUG

 # if defined DDI_DEBUG_CPU
 # define DDI_SPECIFIC_CPU (gv(ddi_base_comm).me == DDI_DEBUG_CPU)
 # else
 # define DDI_SPECIFIC_CPU (1)
 # endif

 # define DDI_CHECK_ARGS

 # define DEBUG_START(a)    if(DebugValue()>=(a) && DDI_SPECIFIC_CPU) {
 # define DEBUG_BLOCK(a)  } if(DebugValue()>=(a) && DDI_SPECIFIC_CPU) {
 # define DEBUG_ELSEIF(a) } else if(DebugValue()>=(a) && DDI_SPECIFIC_CPU) { 
 # define DEBUG_END()     } fflush(stdout); fflush(stderr);

 # define DEBUG_OUT(lvl,str) if(DebugValue() >= lvl && DDI_SPECIFIC_CPU) fprintf str ; fflush(stdout);
 # define MAX_DEBUG(str) if(DebugValue() >= DEBUG_MAX && DDI_SPECIFIC_CPU) fprintf str ; fflush(stdout);
 # define STD_DEBUG(str) if(DebugValue() >= DEBUG_STD && DDI_SPECIFIC_CPU) fprintf str ; fflush(stdout);
 # define MIN_DEBUG(str) if(DebugValue() >= DEBUG_MIN && DDI_SPECIFIC_CPU) fprintf str ; fflush(stdout);
 # define ULTRA_DEBUG(str) if(DebugValue() >= DEBUG_ULTRA && DDI_SPECIFIC_CPU) fprintf str ; fflush(stdout);

 # define IS_ROOT (((DDI_Comm *) Comm_find(gv(ddi_working_comm)))->me == 0)
 # define DEBUG_ROOT(lvl,str) if(IS_ROOT) { DEBUG_OUT(lvl,str) }
 # define MAX_DEBUG_ROOT(str) if(IS_ROOT) { MAX_DEBUG(str) }
 # define STD_DEBUG_ROOT(str) if(IS_ROOT) { STD_DEBUG(str) }
 # define MIN_DEBUG_ROOT(str) if(IS_ROOT) { MIN_DEBUG(str) }

 # include <assert.h>

 # else

/* ----------------------------------------------- *\
   If debugging is off, remove debugging functions
\* ----------------------------------------------- */
 # define DEBUG_OUT(lvl,str)
 # define MAX_DEBUG(str)
 # define STD_DEBUG(str)
 # define MIN_DEBUG(str)
 # define ULTRA_DEBUG(str)

 # define DEBUG_ROOT(lvl,str)
 # define MAX_DEBUG_ROOT(str)
 # define STD_DEBUG_ROOT(str)
 # define MIN_DEBUG_ROOT(str) 

 # endif

