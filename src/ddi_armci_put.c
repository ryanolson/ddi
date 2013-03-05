#include "ddi_armci.h"

void DDI_ARMCI_Put(DDI_Patch *patch, void *buf) {
    int handle = patch->handle;
    int commid = gv(dda_comm)[handle];
    int i,nops = 0;
    int nsubp,ranks[MAX_NODES];
    DDI_Patch subp[MAX_NODES];
    char *working_buffer = (char *) buf;

    DDI_Subpatch_comm(handle,patch,&nsubp,ranks,subp,commid);

    for(i=0; i<nsubp; i++) {
#if defined DDI_ARMCI_SMP
        nops += DDI_ARMCI_Put_domain_SMP(&subp[i],working_buffer,ranks[i]);
#else
        nops += DDI_ARMCI_Put_proc(&subp[i],working_buffer,ranks[i]);
#endif
        working_buffer += subp[i].size;
    }

#if defined DDI_ARMCI_IMPLICIT_NBPUT && defined DDI_ARMCI_IMPLICIT_WAIT
    // wait for implicit non-blocking operations
    ARMCI_WaitAll();
#endif

    return;
}

inline int DDI_ARMCI_Put_domain_SMP(DDI_Patch *patch, void *buf, int id) {
    int handle = patch->handle;
    int i,nsubp,nops;
    int ranks[MAX_SMP_PROCS];
    DDI_Patch subp[MAX_SMP_PROCS];
    char *src = (char*)buf;
    
    DDI_Subpatch_node(handle,patch,&nsubp,ranks,subp,id);
    
    for (i = 0; i < nsubp; ++i) {
	DDI_ARMCI_Put_proc(&subp[i],src,ranks[i]);
	src += subp[i].size;
    }
    
    nops = nsubp;
    return nops;
}

inline int DDI_ARMCI_Put_proc(DDI_Patch *patch, void *buf, int proc) {
    int handle = patch->handle;
    int nops = 1;

    DDA_ARMCI_Index *armci_index = gv(dda_armci_index)[handle];
    int trows,tcols,nrows,ncols;
    size_t offset;
    char *dst,*src = (char*)buf;
    int src_stride_arr[2],dst_stride_arr[2],count[2];
    int stride_levels = 1;
    int armci_proc;
    
    trows = gv(dda_index)[handle].nrows;
    tcols = gv(pcmap)[handle][proc+1] - gv(pcmap)[handle][proc];
    nrows = patch->ihi - patch->ilo + 1;
    ncols = patch->jhi - patch->jlo + 1;
    
    offset = (patch->jlo - gv(pcmap)[handle][proc])*trows + patch->ilo;
    offset *= sizeof(double);

    DDI_ARMCI_Acquire(armci_index,handle,proc,DDI_WRITE_ACCESS,(void**)&dst,&armci_proc);
    dst += offset;
    
    if (nrows == trows) {
#if defined DDI_ARMCI_IMPLICIT_NBPUT
	ARMCI_NbPut((void*)src, (void*)dst, patch->size, armci_proc, NULL);
#else
	ARMCI_Put((void*)src, (void*)dst, patch->size, armci_proc);
#endif
    }
    else {
	// i dimensions
	src_stride_arr[0] = sizeof(double)*nrows;
	dst_stride_arr[0] = sizeof(double)*trows;
	// j dimensions
	src_stride_arr[1] = src_stride_arr[0]*ncols;
	dst_stride_arr[1] = dst_stride_arr[0]*tcols;
	// block size count, first dimension must be in bytes
	count[0] = sizeof(double)*nrows;
	count[1] = ncols;
	stride_levels = 1;
	
#if defined DDI_ARMCI_IMPLICIT_NBPUT
	ARMCI_NbPutS((void*)src,src_stride_arr,(void*)dst,dst_stride_arr,count,stride_levels,armci_proc,NULL);
#else
	ARMCI_PutS((void*)src,src_stride_arr,(void*)dst,dst_stride_arr,count,stride_levels,armci_proc);
#endif
    }
    
    DDI_ARMCI_Release(armci_index,handle,proc,DDI_WRITE_ACCESS);

    return nops;
}
