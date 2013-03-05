/* ------------------------------------------------------------- *\
   F77_Extern(SUBROUTINE_NAME)
   ===========================
   Description: C Macro that converts the name of a C subroutine
   into an external name that can be called by FORTRAN 77/90/95.
   
   This is commonly used for subroutines that need to be called
   from both FORTRAN and C.  The subroutine is written in C and
   a FORTRAN wrapper function is created to call the C code.

   FORTRAN externals are machine dependent.  Subroutine objects
   names are generally augmented with 0, 1 or 2 underscores.
   
   _UNDERSCORES should be defined with the number of underscores
   requred by a particular machines FORTRAN. If this is unknown,
   compile a simple FORTRAN subroutine with the -c option and
   use 'nm' to look at the object file.

   Author: Ryan M. Olson
   Date: December 22, 2002
   CVS $Id: f77_extern.h,v 1.1.1.1 2005/11/17 00:12:33 ryan Exp $
\* ------------------------------------------------------------- */
   
#  if defined _UNDERSCORES
#    if _UNDERSCORES == 0
#       define F77_Extern(Funct) Funct
#    elif _UNDERSCORES == 1
#       define F77_Extern(Funct) Funct ## _
#    elif _UNDERSCORES == 2
#       define F77_Extern(Funct) Funct ## __
#    else
#       error "_UNDERSCORES not properly defined."
#    endif
#  else
#    error "_UNDERSCORES not defined."
#  endif
