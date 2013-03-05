#ifndef DDI_ARMCI_H
#define DDI_ARMCI_H

#include "ddi_base.h"

#if !defined DDI_MPI
#error "DDI ARMCI implementation requires MPI"
#endif

#if defined USE_SYSV
#error "DDI SYSV is not compatible with DDI ARMCI"
#endif

#include <mpi.h>
#include <armci.h>

/* ARMCI memory addresses */
void* gv(armci_mem_addr)[MAX_PROCESSORS];

/* ARMCI counter addresses */
int* gv(armci_cnt_addr)[MAX_PROCESSORS];

/* DDI_ARMCI remote memory structure and array */
typedef struct {
    size_t offset;
    int semid;
} DDA_ARMCI_Index;
extern DDA_ARMCI_Index gv(dda_armci_index)[MAX_DD_ARRAYS][MAX_PROCESSORS];

/* DDA to comm mapping */
extern int gv(dda_comm)[MAX_DD_ARRAYS];

/* DLB mutex */
extern int gv(dlb_access);

/* DDI_ARMCI initialization/finalization */
void DDI_ARMCI_Init(int argc,char *argv[]);
void DDI_ARMCI_MPI_Init(int argc,char *argv[]);
void DDI_ARMCI_MPI_global_rank(int *rank);
void DDI_ARMCI_MPI_Comm_init(DDI_Comm *comm, int nprocs, int *pids, int pid,
                                   int domain_nprocs, int *domain_pids, int domain_pid);
void DDI_ARMCI_Finalize();

/* DDI_ARMCI error */
void DDI_ARMCI_Error(char *message, int code);

/* DDI_ARMCI memory initialization */
void DDI_ARMCI_Memory(size_t size);

/* DDI_ARMCI counter functions */
void DDI_ARMCI_DLB_addr();
void DDI_ARMCI_DLBReset();
void DDI_ARMCI_DLBNext(size_t *counter);
void DDI_ARMCI_GDLB_addr();
void DDI_ARMCI_GDLBReset();
void DDI_ARMCI_GDLBNext(size_t *counter);

/* DDI_ARMCI remote memory acquire/release,lock/unlock */
void DDI_ARMCI_Acquire(const DDA_ARMCI_Index *index, int handle, int proc, int ltype, void **array, int *armci_proc_ptr);
void DDI_ARMCI_Release(const DDA_ARMCI_Index *index, int handle, int proc, int ltype);
void DDI_ARMCI_Lock(int mutex, int proc);
void DDI_ARMCI_Unlock(int mutex, int proc);
void DDI_ARMCI_Zero_local(int handle);

/* DDI_ARMCI barrer */
void DDI_ARMCI_Barrier(const DDI_Comm *comm);

/* DDI_ARMCI acc/put/get */
void DDI_ARMCI_Put(DDI_Patch *patch, void *buf);
void DDI_ARMCI_Get(DDI_Patch *patch, void *buf);
void DDI_ARMCI_Acc(DDI_Patch *patch, void *scale, void *buf);

/* DDI_ARMCI acc/put/get process */
inline int DDI_ARMCI_Get_proc(DDI_Patch *patch, void *buf, int proc);
inline int DDI_ARMCI_Put_proc(DDI_Patch *patch, void *buf, int proc);
inline int DDI_ARMCI_Acc_proc(DDI_Patch *patch, void *scale, void *buf, int proc);

/* DDI_ARMCI acc/put/get ARMCI_DOMAIN_SMP */
inline int DDI_ARMCI_Get_domain_SMP(DDI_Patch *patch, void *buf, int id);
inline int DDI_ARMCI_Put_domain_SMP(DDI_Patch *patch, void *buf, int id);
inline int DDI_ARMCI_Acc_domain_SMP(DDI_Patch *patch, void *scale, void *buf, int id);

#endif
