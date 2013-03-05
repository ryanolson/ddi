#include "ddi_base.h"
#include "ddi_armci.h"

void* gv(armci_mem_addr)[MAX_PROCESSORS];
DDA_ARMCI_Index gv(dda_armci_index)[MAX_DD_ARRAYS][MAX_PROCESSORS];
int gv(dda_comm)[MAX_DD_ARRAYS];
int gv(dlb_access);

void DDI_ARMCI_Memory_init(size_t size) {
  int code;
  const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_COMM_WORLD);
  
  // malloc ARMCI memory
  code = ARMCI_Malloc((void*)gv(armci_mem_addr),size);
  if (code > 0) {
    ARMCI_Error("ARMCI_Malloc failed",code);
    Fatal_error(911);
  }
  gv(dda_index) = (DDA_Index*)gv(armci_mem_addr)[comm->me];

  // malloc ARMCI counter block and set addresses
  code = ARMCI_Malloc((void*)gv(armci_cnt_addr),sizeof(int)*2);
  if (code > 0) {
    ARMCI_Error("ARMCI_Malloc failed",code);
    Fatal_error(911);
  }
  ARMCI_PutValueInt(0, (void*)(gv(armci_cnt_addr)[comm->me]+0), comm->me);
  ARMCI_PutValueInt(0, (void*)(gv(armci_cnt_addr)[comm->me]+1), comm->me);
  DDI_ARMCI_DLB_addr();
  DDI_ARMCI_GDLB_addr();
  
  // create mutexes
  code = ARMCI_Create_mutexes(MAX_DD_ARRAYS+1);
  if (code > 0) {
    ARMCI_Error("ARMCI_Create_mutexes failed",code);
    Fatal_error(911);
  }
  gv(dlb_access) = MAX_DD_ARRAYS;
}

// figure out remote memory address of dynamic load balancer
void DDI_ARMCI_DLB_addr() {
    int *cnt_ptr;
    const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);
    
    cnt_ptr = gv(armci_cnt_addr)[comm->global_pid[0]];
    gv(dlb_counter) = (size_t*)cnt_ptr;
}

void DDI_ARMCI_DLBReset() {
    const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);

    // reset counter
    if (comm->me == 0) ARMCI_PutValueInt(0, (void*)gv(dlb_counter), comm->global_pid[0]);
}

void DDI_ARMCI_DLBNext(size_t *counter) {
    int tmp;
    const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);

    // increment counter
    ARMCI_Rmw(ARMCI_FETCH_AND_ADD,&tmp,(void*)gv(dlb_counter),1,comm->global_pid[0]);
    *counter = (size_t)tmp;
}

// figure out remote memory address of global dynamic load balancer
void DDI_ARMCI_GDLB_addr() {
    int *cnt_ptr;
    const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_COMM_WORLD);

    cnt_ptr = gv(armci_cnt_addr)[comm->global_pid[0]];
    cnt_ptr += 1;
    gv(gdlb_counter) = (size_t*)cnt_ptr;
}

void DDI_ARMCI_GDLBReset() {
    const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_COMM_WORLD);

    // reset counter
    if (comm->me == 0) ARMCI_PutValueInt(0, (void*)gv(gdlb_counter), 0);
}

void DDI_ARMCI_GDLBNext(size_t *counter) {
    int tmp;
    const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);

    // increment and broadcast global counter
    if (comm->me == 0) ARMCI_Rmw(ARMCI_FETCH_AND_ADD,&tmp,(void*)gv(gdlb_counter),1,0);
    MPI_Bcast(&tmp, sizeof(int), MPI_BYTE, 0, comm->compute_comm);
    *counter = (size_t)tmp;
}

void DDI_ARMCI_Index_create(DDA_Index *index, int handle) {
  DDA_ARMCI_Index *armci_index = gv(dda_armci_index)[handle];
  const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);

  armci_index[comm->me].offset = index[handle].offset;
  armci_index[comm->me].semid = handle;
  gv(dda_comm)[handle] = DDI_WORKING_COMM;

  MPI_Allgather(&armci_index[comm->me],sizeof(DDA_ARMCI_Index),MPI_BYTE,
		armci_index,sizeof(DDA_ARMCI_Index),MPI_BYTE,
		comm->compute_comm);
}

void DDI_ARMCI_Acquire(const DDA_ARMCI_Index *index, int handle, int proc, int ltype, void **array, int *armci_proc_ptr) {
  char *buf = NULL;
  int semid = index[proc].semid;
  int commid = gv(dda_comm)[handle];
  const DDI_Comm *comm = (const DDI_Comm *) Comm_find(commid);
  int armci_proc = comm->global_pid[proc];
  
# if defined DDI_MAX_DEBUG
  if(ltype != DDI_READ_ACCESS && ltype != DDI_WRITE_ACCESS) {
    fprintf(stdout,"%s: Invalid lock type requested.\n",DDI_Id());
    Fatal_error(911);
  }
# endif

  buf = (char*)gv(armci_mem_addr)[armci_proc];
  buf += index[proc].offset;
  *array  = (void*)buf;

#if defined DDI_ARMCI_LOCK
  DDI_ARMCI_Lock(semid,proc);
#endif

  *armci_proc_ptr = armci_proc;
}

void DDI_ARMCI_Release(const DDA_ARMCI_Index *index, int handle, int proc, int ltype) {
  int semid = index[proc].semid;
  int commid = gv(dda_comm)[handle];
  const DDI_Comm *comm = (const DDI_Comm *) Comm_find(commid);
  int armci_proc = comm->global_pid[proc];

#if defined DDI_ARMCI_LOCK
  DDI_ARMCI_Unlock(semid,armci_proc);
#endif
}

void DDI_ARMCI_Lock(int mutex, int armci_proc) {
    ARMCI_Lock(mutex,armci_proc);
}

void DDI_ARMCI_Unlock(int mutex, int armci_proc) {
    ARMCI_Unlock(mutex,armci_proc);
}

void DDI_ARMCI_Zero_local(int handle) {
    double *da = NULL;
    const DDA_Index *Index = gv(dda_index);
    size_t i,size = Index[handle].size;

    const DDA_ARMCI_Index *armci_index = gv(dda_armci_index)[handle];
    int commid = gv(dda_comm)[handle];
    const DDI_Comm *comm = (const DDI_Comm *) Comm_find(commid);
    int armci_proc, proc = comm->me;

    DDI_ARMCI_Acquire(armci_index,handle,proc,DDI_WRITE_ACCESS,(void **) &da, &armci_proc);
    for (i=0; i<size; i++) da[i] = 0.0;
    DDI_ARMCI_Release(armci_index,handle,proc,DDI_WRITE_ACCESS);
}

void DDI_ARMCI_Barrier(const DDI_Comm *comm) {
    if (comm == (const DDI_Comm *)Comm_find(DDI_COMM_WORLD)) {
	ARMCI_Barrier();
    }
    else {
	ARMCI_AllFence();
	MPI_Barrier(comm->compute_comm);
    }
}

void DDI_ARMCI_Finalize() {
  int code;
  const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_COMM_WORLD);
  
#if defined DDI_ARMCI_FREE
  code = ARMCI_Free((void*)(gv(armci_mem_addr)[comm->me]));
  if (code > 0) fprintf(stderr,"ARMCI_Free(%p) failed: %i",gv(armci_mem_addr)[comm->me]);

  code = ARMCI_Free((void*)(gv(armci_cnt_addr)[comm->me]));
  if (code > 0) fprintf(stderr,"ARMCI_Free(%p) failed: %i",gv(armci_cnt_addr)[comm->me]);
#endif

  code = ARMCI_Destroy_mutexes();
  if (code > 0) fprintf(stderr,"ARMCI_Destory_mutexes failed: %i",code);

  ARMCI_Finalize();
}

void DDI_ARMCI_Error(char *message, int code) {
  ARMCI_Error(message,code);
}
