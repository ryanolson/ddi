#include "ddi_armci.h"

/** DDI ARMCI initialization */
void DDI_ARMCI_Init(int argc,char **argv) {
  int code;
  int pid;
  int i,j,k;
  int nprocs,*pids;
  int domain_my_id;
  int domain_nprocs,*domain_pids,domain_pid,domain_count;
  int myds;
  DDI_Comm *comm = (DDI_Comm*)&gv(ddi_base_comm);

  // initialize message passing
  DDI_ARMCI_MPI_Init(argc,argv);
  DDI_ARMCI_MPI_global_rank(&pid);

  code = ARMCI_Init();
  if (code > 0) DDI_ARMCI_Error("ARMCI_Init failed",code);
  if (pid == 0) fprintf(stderr,"DDI ARMCI initialized\n");

  Init_scratch(argc,argv);
  
  // SMP domain
  domain_nprocs = armci_domain_nprocs(ARMCI_DOMAIN_SMP, -1);
  domain_my_id = armci_domain_my_id(ARMCI_DOMAIN_SMP);
  domain_pids = (int*)Malloc(sizeof(int)*domain_nprocs);
  for (i = 0; i < domain_nprocs; ++i) {
    domain_pids[i] = armci_domain_glob_proc_id(ARMCI_DOMAIN_SMP,domain_my_id,i);
    if (domain_pids[i] == pid) domain_pid = i;
  }

  // total number of processes and their pids ordered by SMP domains
  domain_count = armci_domain_count(ARMCI_DOMAIN_SMP);
  nprocs = 0;
  for (i = 0; i < domain_count; ++i) nprocs += armci_domain_nprocs(ARMCI_DOMAIN_SMP, i);
  
// compile time limits
#if defined DDI_ARMCI_SMP
  if (domain_nprocs > MAX_SMP_PROCS) DDI_ARMCI_Error("Compile time limit MAX_SMP_PROCS exceeded",911);
  if (domain_count > MAX_NODES) DDI_ARMCI_Error("Compile time limit MAX_NODES exceeded",911);
#else
  if (nprocs > MAX_NODES) DDI_ARMCI_Error("Compile time limit MAX_NODES exceeded",911);
#endif
  if (nprocs > MAX_PROCESSORS) DDI_ARMCI_Error("Compile time limit MAX_PROCESSORS exceeded",911);

  pids = (int*)Malloc(sizeof(int)*nprocs);
  for (i = 0, k = 0; i < domain_count; ++i)
    for (j = 0; j < armci_domain_nprocs(ARMCI_DOMAIN_SMP, i); ++j, ++k) {
      pids[k] = armci_domain_glob_proc_id(ARMCI_DOMAIN_SMP, i, j);
      gv(ddiprocs)[k].node = i;
    }

  // node information
  for (i = 0; i < domain_count; ++i) {
    myds = (pid % armci_domain_nprocs(ARMCI_DOMAIN_SMP,i));    
    gv(ddinodes)[i].cpus       = armci_domain_nprocs(ARMCI_DOMAIN_SMP,i);
    gv(ddinodes)[i].myds       = armci_domain_glob_proc_id(ARMCI_DOMAIN_SMP,i,myds);
    gv(ddinodes)[i].nodemaster = armci_domain_glob_proc_id(ARMCI_DOMAIN_SMP,i,0);
  }

  // initialize communicator
  DDI_ARMCI_MPI_Comm_init(comm,nprocs,pids,pid,domain_nprocs,domain_pids,domain_pid);

}

/** Initialize MPI */
void DDI_ARMCI_MPI_Init(int argc,char *argv[]) {
  int code;

  code = MPI_Init(&argc,&argv);
  if (code != MPI_SUCCESS) DDI_ARMCI_Error("MPI initialization failed",code);
}

/** MPI global rank */
void DDI_ARMCI_MPI_global_rank(int *rank) {
  MPI_Comm_rank(MPI_COMM_WORLD,rank);
}

/** Initialize MPI communicators */ 
void DDI_ARMCI_MPI_Comm_init(DDI_Comm *comm, int nprocs, int *pids, int pid,
				   int domain_nprocs, int *domain_pids, int domain_pid) {
  int domain_master = 0;
  int domain_my_id = armci_domain_my_id(ARMCI_DOMAIN_SMP);
  int i;

  MPI_Group Comm_World_grp;
  MPI_Group SMP_World_grp;
  MPI_Group SMP_Compute_grp;
  MPI_Group DDI_World_grp;
  MPI_Group DDI_Compute_grp;
  MPI_Comm SMP_World_comm;
  MPI_Comm SMP_Compute_comm;
  MPI_Comm SMP_Masters_comm;
  MPI_Comm DDI_World_comm;
  MPI_Comm DDI_Compute_comm;

  // SMP domain communicator
  MPI_Comm_group(MPI_COMM_WORLD,&Comm_World_grp);
  MPI_Group_incl(Comm_World_grp,domain_nprocs,domain_pids,&SMP_World_grp);
  MPI_Comm_create(MPI_COMM_WORLD,SMP_World_grp,&SMP_World_comm);
  MPI_Barrier(MPI_COMM_WORLD);

  // SMP domain masters communicator
  if (pid == armci_domain_glob_proc_id(ARMCI_DOMAIN_SMP, domain_my_id, 0)) domain_master = 1;
  MPI_Comm_split(MPI_COMM_WORLD,domain_master,0,&SMP_Masters_comm);
  MPI_Barrier(MPI_COMM_WORLD);

  // DDI_Compute_comm communicator
  MPI_Group_incl(Comm_World_grp,nprocs,pids,&DDI_Compute_grp);
  MPI_Comm_create(MPI_COMM_WORLD,DDI_Compute_grp,&DDI_Compute_comm);
  MPI_Barrier(MPI_COMM_WORLD);

  // DDI_World_comm communicator
  MPI_Group_incl(Comm_World_grp,nprocs,pids,&DDI_World_grp);
  MPI_Comm_create(MPI_COMM_WORLD,DDI_World_grp,&DDI_World_comm);
  MPI_Barrier(MPI_COMM_WORLD);

  // SMP_Compute_comm communicator
  MPI_Group_intersection(DDI_Compute_grp,SMP_World_grp,&SMP_Compute_grp);
  MPI_Comm_create(MPI_COMM_WORLD,SMP_Compute_grp,&SMP_Compute_comm);
  MPI_Barrier(MPI_COMM_WORLD);

  // populate comm
  comm->np           = nprocs;
  comm->me           = pid;
  comm->nn           = armci_domain_count(ARMCI_DOMAIN_SMP);
  comm->my           = domain_my_id;
  comm->id           = DDI_COMM_WORLD;
  comm->np_local     = domain_nprocs;
  comm->me_local     = domain_pid;
  comm->smp_comm     = SMP_Compute_comm;
  comm->world_comm   = DDI_World_comm;
  comm->compute_comm = DDI_Compute_comm;
  comm->node_comm    = SMP_Masters_comm;

#if !defined DDI_ARMCI_SMP
  comm->nn       = comm->np;
  comm->my       = comm->me;
  comm->np_local = 1;
  comm->me_local = 0;
  comm->node_comm = DDI_Compute_comm;
#endif
}
