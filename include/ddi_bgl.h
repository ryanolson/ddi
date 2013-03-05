#ifndef DDI_BGL_H
#define DDI_BGL_H

#include "ddi_base.h"
#include <stdio.h>

#define DDI_FILE_IO_RANK_(c)     DDI_BGL_File_IO_rank((c))
#define DDI_FILE_IONODE_RANK_(c) DDI_BGL_File_IONode_rank((c))
#define DDI_RUNTIME_PRINT_(s)    DDI_BGL_Runtime_print(s)

/** I/O functions. */
int DDI_BGL_File_IO_rank(DDI_Comm *comm);
int DDI_BGL_File_IONode_rank(DDI_Comm *comm);

/** print DDI Blue Gene/L runtime */
void DDI_BGL_Runtime_print(FILE *stream);

#endif
