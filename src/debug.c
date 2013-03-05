
#include "ddi_base.h"

# if defined DDI_DEBUG
  static int flag=DDI_DEBUG;
# else
  static int flag=0;
# endif

int DebugValue() {
    return flag;
}

void DebugOutput(int value) 
{
/*
   int np=0,me=0;
   DDI_Patch msg;
*/
   flag = value;
}
