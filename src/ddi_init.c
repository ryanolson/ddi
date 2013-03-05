/* ------------------------------------------------------------------ *\
 * Distributed Data Interface
 * ==========================
 * 
 * Initializes the message passing library, shared memory segments,
 * sets global DDI variables, and separates the processes into
 * compute processes and data servers.
 *
 * Author: Ryan M. Olson
 * CVS $Id: ddi_init.c,v 1.9 2006/11/21 21:39:10 andrey Exp $
\* ------------------------------------------------------------------ */
 # include "ddi_base.h"

/* ---------------------------- *\
   Local Functions for DDI_Init
\* ---------------------------- */
 # if defined DDI_MPI
   static void Init_mpi(int,char**);
 # endif

 # if defined DDI_SOC
   static void Init_soc(int,char**);
   static void Init_soc_accept(int,int,int,int*);
   static void Init_soc_create(int,int,int,int*,int*);
 # endif

 # if defined DDI_LAPI
   static void Init_lapi();
 # endif
 
 # if defined USE_SYSV
   static void Init_smp();
 # endif

 # if defined DDI_COUNTERS
   static void Init_counters();
 # endif

   void Init_scratch(int,char**);

/* ---------------------------------------------------- *\
   DDI_Init(argc,argv)
   ===================
   [IN] int    argc  --  number of arguments
   [IN] char **argv  --  list of arguments
    
   Initialize message-passing library and DDI variables
\* ---------------------------------------------------- */
   void DDI_Init(int argc,char **argv) {
      char *header = NULL;
    # if defined DDI_SOC || defined DDI_MPI
      int np,me;

   /* ---------------------------------- *\
      Initialize Message Passing Library
   \* ---------------------------------- */
#if defined DDI_ARMCI
      DDI_ARMCI_Init(argc,argv);
#else

    # if defined DDI_MPI
      Init_mpi(argc,argv);
    # endif

    # if defined DDI_SHMEM
      shmem_init();
    # endif

    # if defined DDI_SOC
      Init_soc(argc,argv);
    # endif

    # if defined DDI_LAPI
      Init_lapi();
    # endif

    # if defined DDI_ONESIDED
      ddi_onesided_init();
    # endif

    # if defined USE_SYSV
      Init_smp();
    # endif

# endif 

   /* --------------------------------- *\
      Determine Rank and Number of CPUs
   \* --------------------------------- */
      DDI_NProc(&np,&me);

   /* --------------------------- *\
      Initialize Profile Counters
   \* --------------------------- */
    # if defined DDI_COUNTERS
      Init_counters();
    # endif

   /* ------------------------ *\
    * Initialize communicators
   \* ------------------------ */
      Comm_init();


   /* ----------------------- *\
      Initialize Data Servers
   \* ----------------------- */
      if(me >= np) DDI_Server();


#ifdef DDI_BGL
      /* Print runtime header if requested.
	 This works on any POSIX system. */
      header = getenv("DDI_HEADER");
      if(me == 0 && header != NULL) {
	  if (strcmp(header,"RUNTIME") == 0) {
	      DDI_Runtime_print(stdout);
	      fprintf(stdout,"\n");
	  }
	  fflush(stdout);
      }
#endif

    # ifdef CRAY_MPI
//    DDI_SMP_NProc(&np,&me);
//    if(me == 1) DS_Thread_init();
    # endif

   /* ----------------------------- *\
      Synchronize Compute Processes
   \* ----------------------------- */
      DDI_Sync(3061);

    # endif
   }



/* ------------------------------------------ *\
\* ------------------------------------------ */
 # if defined DDI_MPI
   static void Init_mpi(int targc,char *targv[]) {

    # ifndef HOSTNAME_LEN
    # define HOSTNAME_LEN  96
    # endif

      int argc = targc;
      char **argv = targv;
      int i,j,np,me,nc,nd,ndpn;
      int np_local,me_local;
      int nnodes,mynode,master;
      int icp,ids,cpus,myds,ext;
      int *ranks,*disp,*world;
      int *ranks_local;

/* --- moved to global variables --- 
      int *np_by_node;
      int *nc_by_node;
      int *nd_by_node;
      int **ranks_by_node;
 * --------------------------------- */

      int me_mpi,me_ddi,rbn;

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

      char hostname[HOSTNAME_LEN],*c,*hostnames;

      DDI_Comm *comm = (DDI_Comm *) &gv(ddi_base_comm);

   /* -------------- *\
      Initialize MPI
   \* -------------- */
      if(MPI_Init(&argc,&argv) != MPI_SUCCESS) {
         fprintf(stdout," DDI: MPI_Init failed.\n");
         fflush(stdout); exit(911);
      }

   /* -------------------------------- *\
    * Initialize DDI working directory
   \* -------------------------------- */
      Init_scratch(argc,argv);


   /* ------------------------------------------ *\
      Determine Rank and Number of MPI Processes
   \* ------------------------------------------ */
      MPI_Comm_size(MPI_COMM_WORLD,&np);
      MPI_Comm_rank(MPI_COMM_WORLD,&me);


   /* -------------------------------------- *\
      For debugging purposes, set gv(myproc)
   \* -------------------------------------- */
      comm->me = me;
      DEBUG_ROOT(LVL1,(stdout," DDI: MPI initialized.  %i MPI processes.\n",np))


   /* ---------------------------------------------------- *\
      MPI-1 requires data servers unless it is using LAPI.
      MPI-2 does not require data servers at all.
      ----------------------------------------------------
      nc = 0  ==> standard data server model (cp:ds::1:1).
      nc = np ==> specialized model such as LAPI || MPI-2.
   \* ---------------------------------------------------- */
      nc = 0;
    # if defined DDI_LAPI || defined DDI_MPI2 || defined CRAY_MPI || defined DDI_ONESIDED
      nc = np;
    # endif


   /* ------------------------------------------ *\
      Standard MPI-1 model (nc=0) ==> cp:ds::1:1
   \* ------------------------------------------ */
      if(nc == 0) {
         if((np % 2) && (me == 0)) {
            fprintf(stdout," Error: Expecting an even number of MPI processes (cp:ds::1:1).\n");
            Fatal_error(911);
         }
         
         nc = nd = np/2;
      }


   /* ------------------------------------------------ *\
      MPI-2 or MPI-1/LAPI model (nc=np) ==> cp:ds::1:0
   \* ------------------------------------------------ */
      if(nc == np) nd = 0;
      
      
   /* ------------------------------------------------------------- *\
      Check to make sure the job complies with compile time limits.
   \* ------------------------------------------------------------- */
      if(nc > MAX_PROCESSORS) {
         
         if(me == 0) {
            fprintf(stdout," DDI: \"Houston, we have a problem.\"\n");
            fprintf(stdout," DDI: MAX_NODES = %i\n",MAX_NODES);
            fprintf(stdout," DDI: MAX_SMP_PROCS = %i\n",MAX_SMP_PROCS);
            fprintf(stdout," DDI: MAX_PROCESSORS = MAX_NODES * MAX_SMP_PROCS = %i\n",MAX_PROCESSORS);
            fprintf(stdout," DDI: MPI reports %i processes ==> %i processors.\n",np,nc);
            fprintf(stdout," DDI: Please correct the limits and recompile DDI.\n");
            fflush(stdout);
         }
         
         MPI_Barrier(MPI_COMM_WORLD);
         MPI_Finalize();
         exit(0);
      }
      
      
   /* ------------------------------------------------------------------- *\
      Non-Standard MPI-1 Model (nc < np && ((nc | np) || (np-nc | np)))
      Can be used to vary the number of data server per node by assigning
      a number of data servers each compute process or a number of data
      server per node.  This code has not been implemented.
   \* ------------------------------------------------------------------- */
      if(nc != nd && nc != np) {
         fprintf(stdout," DDI: This should never have been executed.\n");
         Fatal_error(911);
      }
      

   /* ---------------------------------- *\
      System command to get the hostname
   \* ---------------------------------- */
      gethostname(hostname,HOSTNAME_LEN);
      DEBUG_OUT(LVL4,(stdout," MPI Process %i: hostname=%s\n",me,hostname))


   /* -------------------------------------------- *\
      Gather all the hostnames into a single array
   \* -------------------------------------------- */
      hostnames = (char *) Malloc(np*HOSTNAME_LEN);
      MPI_Allgather(hostname, HOSTNAME_LEN,MPI_BYTE,
                    hostnames,HOSTNAME_LEN,MPI_BYTE,MPI_COMM_WORLD);


   /* -------------------------------------- *\
      Determine all MPI Process on "my" node
   \* -------------------------------------- */
      ranks = (int *) Malloc(np*sizeof(int));
      for(i=0,np_local=0,c=hostnames; i<np; i++,c+=HOSTNAME_LEN) {
         if(strcmp(hostname,c) == 0) ranks[np_local++] = i;
      }
      DEBUG_OUT(LVL4,(stdout," MPI Process %i: %i local MPI processes.\n",me,np_local))

      ranks_local = (int *) Malloc(np_local*sizeof(int));
      memcpy(ranks_local,ranks,np_local*sizeof(int));


   /* ----------------------------- *\
      Create SMP_World communicator
   \* ----------------------------- */
      MPI_Comm_group(MPI_COMM_WORLD,&Comm_World_grp);
      MPI_Group_incl(Comm_World_grp,np_local,ranks_local,&SMP_World_grp);
      MPI_Comm_create(MPI_COMM_WORLD,SMP_World_grp,&SMP_World_comm);

      MPI_Barrier(MPI_COMM_WORLD);
      DEBUG_ROOT(LVL3,(stdout," DDI: SMP_World_comm created.\n"))

   /* ------------------------------ *\
      Create SMP_Master communicator
   \* ------------------------------ */
      MPI_Comm_rank(SMP_World_comm,&me_local);

      master = 0;
      if(me_local == 0) master = 1;

      MPI_Comm_split(MPI_COMM_WORLD,master,0,&SMP_Masters_comm);

      MPI_Barrier(MPI_COMM_WORLD);
      DEBUG_ROOT(LVL3,(stdout," DDI: SMP_Master_comm created.\n"))

   /* --------------------------------------------------------------------------- *\
      Create Compute_comm and World_comm communicators
      ================================================
      First gather the node information, then sort that information by node (not
      guarenteed to be sorted).  Next assign compute processes and data servers
      (if they exist), and finally create the communicators.
   \* --------------------------------------------------------------------------- */
      MPI_Comm_size(SMP_Masters_comm,&nnodes);
      MPI_Comm_rank(SMP_Masters_comm,&mynode);
      MPI_Bcast(&nnodes,1,MPI_INT,0,SMP_World_comm);
      MPI_Bcast(&mynode,1,MPI_INT,0,SMP_World_comm);
      
      MPI_Barrier(MPI_COMM_WORLD);
      DEBUG_ROOT(LVL3,(stdout," DDI: There are %i nodes.\n",nnodes))

   /* --------------------------------------- *\
      Check compile-time limits for MAX_NODES
   \* --------------------------------------- */
      if(nnodes > MAX_NODES) {
      
         if(me == 0) {
            fprintf(stdout," DDI: MAX_NODES = %i\n",MAX_NODES);
            fprintf(stdout," DDI: MPI topology suggests %i nodes.\n",nnodes);
            fprintf(stdout," DDI: Increase MAX_NODES and recompile DDI.\n");
            fflush(stdout);
         }
         
         MPI_Barrier(MPI_COMM_WORLD);
         MPI_Finalize();
         exit(0);
      }


   /* ----------------------- *\
      Gather node information
   \* ----------------------- */
      np_by_node = (int *) Malloc(nnodes*sizeof(int));
      ranks_by_node = (int **) Malloc(nnodes*sizeof(int*));

      if(me_local == 0) {
         DEBUG_OUT(LVL4,(stdout," MPI Process %i: Node %i master.\n",me,mynode))
	      
         MPI_Allgather(&np_local,1,MPI_INT,np_by_node,1,MPI_INT,SMP_Masters_comm);

         for(i=0,j=0; i<nnodes; i++) j += np_by_node[i];
         if(j != np) {
            fprintf(stdout," DDI Error: Sum of PPN over all nodes != NP\n");
            Fatal_error(911);
         }

         disp = (int *) Malloc(nnodes*sizeof(int));
         for(i=1,disp[0]=0; i<nnodes; i++) disp[i] = disp[i-1] + np_by_node[i-1];

         MPI_Allgatherv(ranks_local,np_local,MPI_INT,ranks,np_by_node,disp,MPI_INT,
                        SMP_Masters_comm);
         free(disp);
      }

      MPI_Bcast(np_by_node,nnodes,MPI_INT,0,SMP_World_comm);
      MPI_Bcast(ranks,np,MPI_INT,0,SMP_World_comm);

      MPI_Barrier(MPI_COMM_WORLD);
      DEBUG_ROOT(LVL3,(stdout," DDI: Node topology determined.\n"))

      ranks_by_node[0] = ranks;
      for(i=1; i<nnodes; i++) ranks_by_node[i] = (ranks_by_node[i-1] + np_by_node[i-1]);


   /* --------------------------------------------------------------------------- *\
      Each MPI process has a list of MPI ranks sorted by node.  The list of ranks
      for a particular node is sorted from lowest to highest rank, where the rank
      corresponds to the value in MPI_COMM_WORLD communicator. Next determine the 
      number of compute processes/node.  Data servers/node can be inferred.
   \* --------------------------------------------------------------------------- */
      nc_by_node = (int *) Malloc(nnodes*sizeof(int));
      nd_by_node = (int *) Malloc(nnodes*sizeof(int));

      if(nc == nd) {

      /* ------------------------------------------------------------- *\
         There are a given number of data servers per compute process.
         Now the ratio must be 1:1.  CP:DS:1:N not implemented (yet).
      \* ------------------------------------------------------------- */
         j = nd/nc + 1;  /* j represents the number of MPI process per compute process */

         for(i=0; i<nnodes; i++) {

            if((np_by_node[i] % j)) {
               fprintf(stdout," DDI: For every CP requested there should be %i MPI processes.\n",j);
               fprintf(stdout," DDI Error: np on node %i is not divisible by %i.\n",i,j);
               Fatal_error(911);
            }

            nc_by_node[i] = np_by_node[i] / j;
            nd_by_node[i] = np_by_node[i] - nc_by_node[i];
         }

      }
      
      
      if(nc == np) {
   
       # ifdef CRAY_MPI
      /* ------------------------------------------------------------- *\
         The environmental variable DDI_DS_PER_NODE is used to control
         the number of MPI processes that become data servers.
      \* ------------------------------------------------------------- */
         if(me == 0) {
           if(getenv("DDI_DS_PER_NODE")) {
             ndpn = atoi(getenv("DDI_DS_PER_NODE"));
             if(nnodes == 1) ndpn = 0;
           } else {
             ndpn = 0;
           }
         }
         MPI_Bcast(&ndpn,1,MPI_INT,0,MPI_COMM_WORLD);

      /* -------------------------------------------------------- *\
         If DDI_DS_PER_NODE is invalid, then shutdown gracefully. 
      \* -------------------------------------------------------- */
         if(ndpn < 0 || ndpn > MAX_SMP_PROCS-1) {
           if(me == 0) {
             fprintf(stdout,"%s: DDI_DS_PER_NODE=%i is invalid.\n",DDI_Id(),ndpn);
             fprintf(stdout,"%s: The value must between 0 and %i.\n",DDI_Id(),MAX_SMP_PROCS-1);
             fflush(stdout);
             sleep(1);
           }
           MPI_Finalize();
         }

         nd = nnodes*ndpn;
         nc = np - nd;
       # endif

      /* --------------------------------------------- *\
         MPI-2 or MPI-1/LAPI model ==> no data servers
      \* --------------------------------------------- */
         for(i=0; i<nnodes; i++) {
             nc_by_node[i] = np_by_node[i];
             nd_by_node[i] = 0;
           # ifdef CRAY_MPI
             nc_by_node[i] = np_by_node[i]-ndpn;
             nd_by_node[i] = ndpn;

          /* ------------------------------------------- *\
             Sanity check - Ensure >1 CP exists per node
          \* ------------------------------------------- */
             if(nc_by_node[i] <= 0) {
               if(me == 0) {
                 fprintf(stdout," ERROR: There are no CPs assigned to node %i.\n",i);
                 fprintf(stdout," The total number of processes on node %i = %i.\n",i,np_by_node[i]);
                 fprintf(stdout," Attempted to reserver %i processes as data servers.\n",ndpn);
                 fflush(stdout);
                 sleep(1);
               }
               MPI_Finalize();
             } 
           # endif
         }
         
      } 

      gv(np) = np;
      gv(nc) = nc;
      gv(nd) = nd;

      DEBUG_ROOT(LVL3,(stdout," DDI: There are %i DDI compute processes.\n",nc))
      DEBUG_ROOT(LVL3,(stdout," DDI: There are %i DDI data servers.\n",nd))


   /* -------------------------------------------------------------------- *\
      Create a list of ranks that will eventually become the communicators
   \* -------------------------------------------------------------------- */
      world = (int *) Malloc(np*sizeof(int));

      for(i=0,icp=0,ids=nc; i<nnodes; i++) { 
         for(j=0; j<np_by_node[i]; j++) {
            if(j<nc_by_node[i]) world[icp++] = ranks_by_node[i][j];
            else                world[ids++] = ranks_by_node[i][j];
         }
      }

      MPI_Barrier(MPI_COMM_WORLD);
      DEBUG_OUT(LVL4,(stdout," MPI Process %i: nc=%i; np=%i.\n",me,nc,np))


   /* ------------------------------------ *\
      Create DDI_Compute_comm communicator
   \* ------------------------------------ */
      MPI_Group_incl(Comm_World_grp,nc,world,&DDI_Compute_grp);
      MPI_Comm_create(MPI_COMM_WORLD,DDI_Compute_grp,&DDI_Compute_comm);


   /* ---------------------------------- *\
      Create DDI_World_comm communicator
   \* ---------------------------------- */
      MPI_Group_incl(Comm_World_grp,np,world,&DDI_World_grp);
      MPI_Comm_create(MPI_COMM_WORLD,DDI_World_grp,&DDI_World_comm);


   /* ------------------------------------ *\
      Create SMP_Compute_comm communicator
   \* ------------------------------------ */
      MPI_Group_intersection(DDI_Compute_grp,SMP_World_grp,&SMP_Compute_grp);
      MPI_Comm_create(MPI_COMM_WORLD,SMP_Compute_grp,&SMP_Compute_comm);

      DEBUG_ROOT(LVL3,(stdout," DDI: finished forming communicators.\n"))

   /* ------------------------------------ *\
      Finished creating MPI communicators.
      Initialize internal DDI structures.
   \* ------------------------------------ */
      MPI_Comm_rank(DDI_World_comm,&me);
      comm->np = nc;
      comm->me = me;
      comm->nn = nnodes;
      comm->my = mynode;

      MPI_Comm_rank(MPI_COMM_WORLD,&me_mpi); 
      MPI_Comm_rank(DDI_World_comm,&me_ddi); 

      DEBUG_OUT(LVL3,(stdout," MPI Process %i = DDI Process %i\n",me_mpi,me_ddi))
      
      comm->id           = DDI_COMM_WORLD;
      comm->smp_comm     = SMP_Compute_comm;
      comm->world_comm   = DDI_World_comm;
      comm->compute_comm = DDI_Compute_comm;
      comm->node_comm    = SMP_Masters_comm;
      comm->smp_world    = SMP_World_comm;

    # if !defined USE_SYSV 
      comm->nn = nc;
      comm->my = me;
      if(comm->my >= nc) comm->my -= nc;
      comm->node_comm    = DDI_Compute_comm;
    # endif


   /* -------------------------------------------------------------------- *\
      Check for network extention.  The extension would be appended to the
      hostname if it becomes necessary to form a TCP/IP socket to the host
   \* -------------------------------------------------------------------- */
    # ifdef DDI_SOC
      for(i=0,ext=0; i<argc && strcmp("-netext",argv[i]) != 0; i++);
      if(i != argc) ext = ++i;
    # endif


   /* ---------------------------------------------------------------- *\
      Scan through the list of hostnames and extract the node topology
   \* ---------------------------------------------------------------- */
      MPI_Allgather(hostname, HOSTNAME_LEN,MPI_BYTE,
                    hostnames,HOSTNAME_LEN,MPI_BYTE,DDI_World_comm);

      MPI_Allgather(&me,1,MPI_INT,ranks_local,1,MPI_INT,SMP_World_comm);
      if(me_local == 0) {
         disp = (int *) Malloc(nnodes*sizeof(int));
         for(i=1,disp[0]=0; i<nnodes; i++) disp[i] = disp[i-1] + np_by_node[i-1];
         MPI_Allgatherv(ranks_local,np_local,MPI_INT,ranks,np_by_node,disp,MPI_INT,
                        SMP_Masters_comm);
         free(disp);
      }
      MPI_Bcast(ranks,np,MPI_INT,0,SMP_World_comm);

      for(i=0; i<nnodes; i++) {

         cpus = nc_by_node[i];
         master = ranks_by_node[i][0];

      /* ---------------------------------------------------------------------- *\
         For each node, one data server is chosen from the all the data servers
         on that node in a round-robin manner based on the rank of the process.
      \* ---------------------------------------------------------------------- */
         if(nd_by_node[i]) myds = cpus + (me % nd_by_node[i]);
         else              myds = -1;
 
 
      /* --------------------------------------------------------------- *\
         Using LAPI or MPI-2, we have no data servers, but we still need
         to know which compute process to interrupt to get, put, or acc!
      \* --------------------------------------------------------------- */
       # if defined DDI_LAPI
         myds = (me % nc_by_node[i]);
       # endif

       # if defined DDI_ONESIDED
         myds = 0;
       # endif


      /* ------------------------------------------------------ *\
         Sanity check: myds must correspond to a rank on node i
      \* ------------------------------------------------------ */
         if(nd && (myds < 0 || myds >= np_by_node[i])) {
           if(me == 0) {
             fprintf(stdout," ERROR: Unable to assign a DS for node %i.\n",i);
             fprintf(stdout," Please report this error to:\n");
             fprintf(stdout," - mike@si.msg.chem.istate.edu and/or ryan@cray.com");
             fprintf(stdout," myds=%i; np_by_node[%i]=%i\n",myds,i,np_by_node[i]);
             fflush(stdout);
             sleep(1);
           }
           MPI_Finalize();
         }


      /* ----------------------------------------------------- *\
         For each remote node, assign a data server rank         
      \* ----------------------------------------------------- */
         if(nd) gv(ddinodes)[i].myds       = ranks_by_node[i][myds];
         else   gv(ddinodes)[i].myds       = -1;

       # ifdef DDI_ONESIDED
         gv(ddinodes)[i].myds       = i;
       # endif


      /* --------------------------------- *\
         Save these values in gv(ddinodes)
      \* --------------------------------- */
         gv(ddinodes)[i].cpus       = cpus;
         gv(ddinodes)[i].nodemaster = master;


      /* ----------------------------------------------------------------- *\
         Dig up the hostname of the node and append any network extensions
      \* ----------------------------------------------------------------- */
       # ifdef DDI_SOC
         c = (hostnames + master*HOSTNAME_LEN);
         if(ext) strcat(c,argv[ext]);
       # endif


      /* ----------------------------------------------------------------------- *\
         All the ddi processes on the node share the same node rank and hostname
      \* ----------------------------------------------------------------------- */
         for(j=0; j<np_by_node[i]; j++) {
            rbn = ranks_by_node[i][j];
            gv(ddiprocs)[rbn].node = i;

          # ifdef DDI_SOC
            gv(ddiprocs)[rbn].hostname = (char *) strdup(c);
          # endif

          # if !defined USE_SYSV
            gv(ddiprocs)[rbn].node = rbn;
            if(rbn >= comm->np) gv(ddiprocs)[rbn].node -= comm->np;
          # endif

         }

      }


   /* ------------------------- *\
      Free any Malloc'ed Memory
   \* ------------------------- */
      free(hostnames);
      free(world);
      free(ranks_local);


   /* ---------------------------- *\
      Do NOT free global variables
   \* ---------------------------- */
/* --- moved to ddi_finalize
      free(ranks);
      free(np_by_node);
      free(nc_by_node);
      free(nd_by_node);
      free(ranks_by_node);
*/


   /* ---------------------------------- *\
      Synchronize processes and continue
   \* ---------------------------------- */
      MPI_Barrier(MPI_COMM_WORLD);
      DEBUG_ROOT(LVL3,(stdout," DDI: Init_mpi finished.\n"))
   }
 # endif
/* ---------------------------------------- *\
   End of MPI dependent initialize routines
\* ---------------------------------------- */


/* -------------------------------------------- *\
\* -------------------------------------------- */
 # if defined DDI_SOC
   static void Init_soc(int argc,char** argv) {
	   
   /* --------------- *\
      Local Variables
   \* --------------- */
      int i,j,iproc,nodearg,me;
      int kickport,nodeid,procid,nn,np,nsocks;
      int *ports,*sockets;
      char kickhost[256];
      int not_it;
      int ncpus,cpus,nics;
      char tmpstr[256];

      DDI_Comm *comm = (DDI_Comm *) &gv(ddi_base_comm);

   /* ---------------------- *\
      Assign signal handlers
   \* ---------------------- */
      signal(SIGTERM, Fatal_error);     /* handles a terminate signal */
      signal(SIGFPE,  Fatal_error);     /* handles a floating point error in the DDI process */
      signal(SIGABRT, Fatal_error);
      signal(SIGSEGV, Fatal_error);
      signal(SIGINT,  Fatal_error);
      signal(SIGILL,  Fatal_error);
      signal(SIGQUIT, Fatal_error);
      signal(SIGPIPE, Fatal_error);
      signal(SIGXCPU, Fatal_error);
      signal(SIGURG,  _sigurg_hndlr);   /* handles a OOB (out-of-band) message */
    # if defined DDI_MPI
      signal(SIGURG,  Fatal_error);
    # endif

   
   /* ---------------------------------- *\
      Find where the DDI arguments begin 
   \* ---------------------------------- */
    # if defined DDI_SOC && !defined DDI_MPI
      for(i=0,not_it=1; i<argc && not_it; i++) not_it = strcmp(argv[i],"-ddi");
      if(i==argc) {
         fprintf(stdout," DDI: Invalid command-line arguments!\n Are you using the ddikick program??\n");
         Fatal_error(911);
      }

      strcpy(kickhost,argv[i++]); 
      kickport = atoi(argv[i++]);               /*  kickoff port number */
      nodeid   = atoi(argv[i++]);               /*  rank of current node */
      procid   = atoi(argv[i++]);               /*  rank of current proc */
      nn       = atoi(argv[i++]);               /*  number of nodes */
      np       = atoi(argv[i++]);               /*  number of procs */


   /* --------------------------------------------------------------- *\
    * Initialize DDI_COMM_WORLD communicator.  This is not a complete
    * initialization; the remaining bits and pieces will be set up in
    * Comm_init().
   \* --------------------------------------------------------------- */
      comm->id = DDI_COMM_WORLD;
      comm->nn = nn;
      comm->my = nodeid;
      comm->np = np;
      comm->me = procid;


   /* -------------------------- *\
      Initialize Local Variables
   \* -------------------------- */
      me       = procid;
      nodearg  = i;
      nsocks   = np;
      if(USING_DATA_SERVERS()) nsocks *= 2;


      DEBUG_OUT(LVL1,(stdout," DDI Process %i: Execution begun on node %i.\n",procid,nodeid))
      DEBUG_OUT(LVL3,(stdout," The master kickoff program responds on %s:%i\n",kickhost,kickport))

 
   /* ------------------------------------------------ *\
      Parse command-line arguments for DDI information
   \* ------------------------------------------------ */
      if(Parse_node_args(argc,argv,nodearg,nn,gv(ddinodes)) < 0 ) {
         fprintf(stderr,"%s: Error while parsing node arguments.\n",DDI_Id());
         Fatal_error(911);
      }


   /* ------------------------------------------------ *\
    * Parse command-line for scratch/working directory
   \* ------------------------------------------------ */
      Init_scratch(argc,argv);


   /* ----------------------------------------------------------- *\
    * Using parsed information, initialize global data structures
   \* ----------------------------------------------------------- */
      for(i=0,iproc=0,ncpus=0; i<nn; i++) {
        gv(ddinodes)[i].nodemaster = iproc;
        cpus = gv(ddinodes)[i].cpus;
        nics = gv(ddinodes)[i].nics;
        gv(ddinodes)[i].myds = (me % cpus) + ncpus + np;
        DEBUG_OUT(LVL4,(stdout,"%s: ddinodes[%i] = { nodemaster=%i cpus=%i nics=%i }\n",DDI_Id(),i,iproc,cpus,nics))
        ncpus += cpus;
      # if !defined USE_SYSV
        if(cpus > 1 && comm->me == 0) {
           if(comm->me == 0) 
           fprintf(stdout,"%s %s\n%s %s\n",
                          "Error: detected :cpus in the ddi argument list, however," 
                          "DDI was not compiled using -DUSE_SYSV.  The program is"
                          "confused.  Either recompile DDI using shared-memory or"
                          "generate a proper argument list.");
           Fatal_error(911);
        }
      # endif
        for(j=0; j<cpus; j++,iproc++) {
           gv(ddiprocs)[iproc].node    = i;
           gv(ddiprocs)[iproc+np].node = i;
           strcpy(tmpstr,gv(ddinodes)[i].hostname);
           if(nics) {
             strcat(tmpstr,gv(ddinodes)[i].netext[j%nics]);
           }

           DEBUG_OUT(LVL4,(stdout,"%s: ddiprocs[%i] = { node=%i hostname=%s }\n",DDI_Id(),iproc,i,tmpstr))

           gv(ddiprocs)[iproc].hostname    = (char *) strdup(tmpstr);

         # if FULL_SMP
           if(nn == 1) continue;
         # endif

           gv(ddiprocs)[iproc+np].hostname = (char *) strdup(tmpstr);

        }
      }

    # else

   /* --------------------------------------------------------------- *\
      When mixing MPI and TCP/IP, MPI has been previously initialized
   \* --------------------------------------------------------------- */
      DDI_NProc(&np,&me);
      nsocks = np;
      if(USING_DATA_SERVERS()) nsocks += np;
    # endif
      

   /* ----------------------------------- *\
      Allocate memory for sockets & ports
   \* ----------------------------------- */
      sockets = (int *) Malloc(nsocks*sizeof(int));
      ports   = (int *) Malloc(nsocks*sizeof(int));

      gv(sockets) = sockets;
 
      for(i=0; i<nsocks; i++) {
        sockets[i] = -1;
        ports[i]   = -1;
      }
   
   
   /* ----------------------------------------------------------------- *\
      Create a server socket for accepting connections to DDI processes
   \* ----------------------------------------------------------------- */
      gv(serversock) = SOC_Create(SERVER_SOCKET,&gv(serverport),NULL);
      DEBUG_OUT(LVL3,(stdout,"%s: Receiving connections on port %d\n",DDI_Id(),gv(serverport)))


   /* --------------------------------------------- *\
      Connect to Master Kickoff Program.
      Socket ownership is required to handle SIGURG
      Use Premium network if available.
   \* ---------------------------------------------- */
    # if defined DDI_SOC && !defined DDI_MPI
      gv(kickoffsock) = SOC_Create(CLIENT_SOCKET,&kickport,kickhost);


   /* ---------------------------------- *\
      Report information back to ddikick
   \* ---------------------------------- */
      Send(gv(kickoffsock),&comm->me,sizeof(int),0);
      Send(gv(kickoffsock),&gv(serverport),sizeof(int),0);


   /* ------------------------------------------------- *\
      Wait until all DDI processes have checked in.
      The master ddikick.x will broadcast the ddi info.
   \* ------------------------------------------------- */
      Recv(gv(kickoffsock),ports,nsocks*sizeof(int),0);
      DEBUG_OUT(LVL3,(stdout,"%s: Received the list of server ports.\n",DDI_Id()))

    # else

      DEBUG_OUT(LVL3,(stdout,"%s: using MPI to gather TCP/IP port info.\n",DDI_Id()))
      MPI_Allgather(&gv(serverport),1,MPI_INT,ports,1,MPI_INT,comm->world_comm);

    # endif

 
   /* ------------------------------------------------------- *\
      Now starts the process of connecting the DDI processes:
      Each DDI compute process actively connects to all DDI
      processes whose ranks are greater than its own, but
      only after it accepts connections from lower ranks.
   \* ------------------------------------------------------- */
      Init_soc_accept(np,me,gv(serversock),sockets);
      Init_soc_create(np,me,nsocks,ports,sockets);
      
   }


/* -------------------------------------- *\
\* -------------------------------------- */
   static void Init_soc_accept(int np, int me,int recvsock, int *sockets) {
      int on=1;
      fd_set readlist;

    # if defined SOC_BUFFER_SIZE
      int buffer = 0;
    # endif
     
      int isock = 0;
      int tsock = 0;
      int procid = 0;
      int nsocks = np; 
      int maxrank = np;

      if(me < np) nsocks = me; 
      if(USING_DATA_SERVERS()) maxrank *= 2;
      
      
   /* ---------------------------------- *\
      Start accepting socket connections 
   \* ---------------------------------- */
      while(isock < nsocks) {
         FD_ZERO(&readlist);
         FD_SET(recvsock,&readlist);
         
      /* --------------------------------------------------- *\
         Wait here until there is an incoming call on 'port'
      \* --------------------------------------------------- */
         select(recvsock+1,&readlist,NULL,NULL,NULL);

         
      /* --------------------------------------------------- *\
         The receptionist picks up the phone from the call
         on 'port' and routes the call to a new line 'tsock'
      \* --------------------------------------------------- */
         tsock = Accept(recvsock,NULL,NULL);

 
      /* ----------------------------------------------------- *\
         Remove the builtin TCP delay & set send/recv timeouts
      \* ----------------------------------------------------- */
         setsockopt(tsock,IPPROTO_TCP,TCP_NODELAY,(void *) &on,sizeof(int));

         
      /* ---------------------- *\
         Set socket buffer size
      \* ---------------------- */
       # if defined SOC_BUFFER_SIZE
         buffer = SOC_BUFFER_SIZE;
         setsockopt(tsock,SOL_SOCKET,SO_RCVBUF,(void *) &buffer,sizeof(int));
         setsockopt(tsock,SOL_SOCKET,SO_SNDBUF,(void *) &buffer,sizeof(int));
       # endif

  
      /* ------------------------------------- *\
         The caller now introduces him/herself
      \* ------------------------------------- */
         if(Recv(tsock,&procid,sizeof(int),0) == 0) {
            fprintf(stdout,"%s: Detected a problem on another DDI process.\n",DDI_Id());
            Fatal_error(911);
         }

         if(procid < 0 || procid > maxrank) {
            fprintf(stdout,"%s: Improper socket introduction ... abort ...\n",DDI_Id());
            Fatal_error(911);
         }

 
      /* ---------------------------------------------------------------------- *\
         Save caller information, i.e. DDI process 'procid' has now checked-in!
      \* ---------------------------------------------------------------------- */
         if(sockets[procid] == -1) {  /* Everything is OK */
            sockets[procid] = tsock;
            isock++;

            MAX_DEBUG((stdout,"%s: Accepted connection from DDI Process %i.\n",DDI_Id(),procid))

         } else {  /* Not everything is OK :( */
            fprintf(stdout,"%s: Multiple DDI processes connecting with the same rank.\n",DDI_Id());
            Fatal_error(911);
         }
      }

      DEBUG_OUT(LVL3,(stdout,"%s: All socket accepts completed.\n",DDI_Id()))
   }


/* ------------------------------------- *\
\* ------------------------------------- */
   static void Init_soc_create(int np,int me,int nsocks,int *ports,int *sockets) {
      int i,mynode;
      char *hostname = NULL;
      char localhost[] = "localhost";
      const DDI_Comm *comm = (DDI_Comm *) Comm_find(gv(ddi_working_comm));
      mynode = comm->my;

      if(me >= np) return;

/*    for(i=nsocks; --i > me; ) {  */       /* initialize sockets 1 proc at a time  */
      for(i=me+1; i<nsocks; i++) {          /* initialize sockets semi-concurrently */
         gv(ddiprocs)[i].port = ports[i];
         hostname = gv(ddiprocs)[i].hostname;
         if(gv(ddiprocs)[i].node == mynode) hostname = localhost;

         DEBUG_OUT(LVL4,(stdout,"%s: Connecting to %i (%s:%i)\n",DDI_Id(),i,hostname,ports[i]))

         sockets[i] = SOC_Create(CLIENT_SOCKET,&ports[i],hostname);

         if(sockets[i] <= 0) {
           fprintf(stderr,"%s: Error creating socket connection to DDI Process %i.\n",DDI_Id(),i);
           Fatal_error(911);
         }

      /* ----------------------------------------- *\
         The calling process must introduce itself
      \* ----------------------------------------- */
         Send(sockets[i],&me,sizeof(int),0);
         DEBUG_OUT(LVL4,(stdout,"%s: Socket connected established to %i.\n",DDI_Id(),i))
      }

      DEBUG_OUT(LVL3,(stdout,"%s: Finished making connections.\n",DDI_Id()))
   }
 # endif
/* -------------------------------------------- *\
   End of TCP/IP socket initialization routines
\* -------------------------------------------- */




/* ---------------------------------------------------------- *\
   LAPI is a specialized low-level API for the IBM SP switch.
   Using this, we do NOT need data servers!
\* ---------------------------------------------------------- */
 # ifdef DDI_LAPI
   static void Init_lapi() {
      
   /* --------------- *\
      Local Variables
   \* --------------- */
      int np,me,lapi_me;
      char *memaddr,lapi_msg[LAPI_MAX_ERR_STRING];
      lapi_info_t info;

      const DDI_Comm *comm = (const DDI_Comm *) &gv(ddi_base_comm);
      
      DDI_NProc(&np,&me);
      DEBUG_ROOT(LVL1,(stdout," DDI: initializing LAPI.\n"))
      DEBUG_OUT(LVL2,(stdout,"%s: initialzing LAPI.\n",DDI_Id()))
      
   /* ------------------------- *\
      Initialize LAPI variables
   \* ------------------------- */
      bzero(&info,sizeof(lapi_info_t));
      info.err_hndlr = NULL;
      gv(lapi_map) = (uint *) Malloc(np*sizeof(uint));
      memaddr      = (char *) Malloc(3*np*sizeof(void*));
      gv(am_hndlr) = (void **) memaddr;  memaddr += np*sizeof(void*);
      gv(lapi_dlb_cntr_addr)  = (int **) memaddr;  memaddr += np*sizeof(int*);
      gv(lapi_gdlb_cntr_addr) = (int **) memaddr;  memaddr += np*sizeof(int*);
      
      
   /* --------------- *\
      Initialize LAPI
   \* --------------- */
      if(LAPI_Init(&gv(lapi_hnd),&info) != LAPI_SUCCESS) {
         fprintf(stdout,"%s: lapi_init failed.\n",DDI_Id());
         Fatal_error(911);
      }


   /* -------------------------------------------------------------------- *\
      Debugging -- LAPI performs error checking on calling arguments.
      Release Mode -- Turn off LAPI error checking (unnecessary overhead).
   \* -------------------------------------------------------------------- */
    # if !defined DDI_DEBUG
      if(LAPI_Senv(gv(lapi_hnd),ERROR_CHK,0) != LAPI_SUCCESS) {
         fprintf(stdout,"%s: failed to turn off LAPI error checking.\n",DDI_Id());
         Fatal_error(911);
      }
    # endif


   /* ------------------------------------------------------------------- *\
      Get LAPI Task Id and gather that into an array ordered by MPI rank.
      Just in case the LAPI id's do not match the MPI ranks.
   \* ------------------------------------------------------------------- */
      if(LAPI_Qenv(gv(lapi_hnd),TASK_ID,&lapi_me) != LAPI_SUCCESS) {
         fprintf(stdout,"%s: lapi_qenv failed to get task id.\n",DDI_Id());
         Fatal_error(911);
      }
      MPI_Allgather(&lapi_me,1,MPI_INT,gv(lapi_map),1,MPI_INT,comm->world_comm);
      DEBUG_OUT(LVL3,(stdout,"%s: LAPI Id = %i.\n",DDI_Id(),lapi_me))
 
 
   /* ---------------------------------------------------- *\
      Exchange addresses for AM Header Handler subroutines
   \* ---------------------------------------------------- */
      LAPI_Address_init(gv(lapi_hnd),(void *) &DDI_AM_Header_hndlr,gv(am_hndlr));
      LAPI_Address_init(gv(lapi_hnd),(void *) &gv(lapi_dlb_cntr),(void **)gv(lapi_dlb_cntr_addr));
      LAPI_Address_init(gv(lapi_hnd),(void *) &gv(lapi_gdlb_cntr),(void **)gv(lapi_gdlb_cntr_addr));
     
      
   /* -------------------------- *\
      Synchronize LAPI processes
   \* -------------------------- */
      LAPI_Gfence(gv(lapi_hnd));
      DEBUG_ROOT(LVL2,(stdout," DDI: LAPI initialized.\n"))
   }
 # endif




/* --------------------------------------- *\
\* --------------------------------------- */
 # if defined USE_SYSV
   static void Init_smp() {

      DDI_Comm *comm = (DDI_Comm *) &gv(ddi_base_comm);

      int smpnp = 1;
      int smpme = 0;

    # if FULL_SMP

   /* -------------------------------------------------------------------------------------- *\
      The new and improved Init_mpi can now dedicate an arbituary number of MPI processes to
      be data servers per node.  This means all the previous maps that used ME-NP for ranks 
      larger than NP is busted and needs to be fixed.  The socket calls should be updated
      with the new global variables instead of the old gv(ddinodes) and gv(ddiprocs).
   \* -------------------------------------------------------------------------------------- */
    # ifdef DDI_MPI
      smpnp = nc_by_node[comm->my];
      if(comm->me >= comm->np) {
         smpme = 0;
         while(smpme < nd_by_node[comm->my]) {
            if(comm->me == ranks_by_node[comm->my][smpnp+smpme]) break;
            smpme++;
         }
         if(smpme >= nd_by_node[comm->my]) {
            fprintf(stdout,"%s: smpme=%i >= nd_by_node[%i]=%i\n",smpme,comm->my,nd_by_node[comm->my]);
            fprintf(stdout,"%s: error determining smpme on node %i.\n",DDI_Id(),comm->my);
            Fatal_error(911);
         }
      } else {
         while(smpme < nc_by_node[comm->my]) {
            if(comm->me == ranks_by_node[comm->my][smpme]) break;
            smpme++;
         }
         if(smpme >= nc_by_node[comm->my] ){
            fprintf(stdout,"%s: error determining smpme on node %i.\n",DDI_Id(),comm->my);
            fprintf(stdout,"%s: smpme=%i >= nc_by_node[%i]=%i\n",smpme,comm->my,nc_by_node[comm->my]);
            Fatal_error(911);
         }
      }
    # else
   /* ------------------------------------------------------ *\
      The OLD way
   \* ------------------------------------------------------ */
      int i,me,np,my,nn,ncpus;
      DDI_NProc(&np,&me);
      DDI_NNode(&nn,&my);

      smpnp = gv(ddinodes)[my].cpus;
      for(i=0,ncpus=0; i<my; i++) ncpus += gv(ddinodes)[i].cpus; 
   
      if( me < np ) { smpme = me-ncpus; } 
      else          { smpme = (me-np)-ncpus; }
      
      if(smpnp > MAX_SMP_PROCS) {
        if(me==0) {
          fprintf(stdout," DDI Error: Could not initialize %i shared memory segments.\n",smpnp);
          fprintf(stdout," DDI was compiled to support %i shared memory segments.\n",MAX_SMP_PROCS);
          fprintf(stdout," Solution: Increase MAXCPUS in 'compddi' and recompile DDI.\n");
        }
        Fatal_error(911);
      }
   
    # endif
    # endif
      DEBUG_OUT(LVL3,(stdout,"%s: SMP Initialization complete (smpnp=%i; smpme=%i).\n",DDI_Id(),smpnp,smpme))

      comm->np_local = smpnp;
      comm->me_local = smpme;
 
   }
 # endif
/* --------------------------------------- *\
   End of System V initialization routines
\* --------------------------------------- */


/* --------------------------------------- *\
\* --------------------------------------- */
 # if defined DDI_COUNTERS
   static void Init_counters() {

      gv(get_profile).ncalls = gv(get_profile).nbytes = 0;
      gv(put_profile).ncalls = gv(put_profile).nbytes = 0;
      gv(acc_profile).ncalls = gv(acc_profile).nbytes = 0;

      gv(get_profile).ncalls_shmem = gv(get_profile).nbytes_shmem = 0;
      gv(put_profile).ncalls_shmem = gv(put_profile).nbytes_shmem = 0;
      gv(acc_profile).ncalls_shmem = gv(acc_profile).nbytes_shmem = 0;

    # if defined DDI_EXTRA_COUNTERS
      gv(get_profile).ncalls_local = gv(get_profile).nbytes_local = 0;
      gv(put_profile).ncalls_local = gv(put_profile).nbytes_local = 0;
      gv(acc_profile).ncalls_local = gv(acc_profile).nbytes_local = 0;

      gv(get_profile).ncalls_remote = gv(get_profile).nbytes_remote = 0;
      gv(put_profile).ncalls_remote = gv(put_profile).nbytes_remote = 0;
      gv(acc_profile).ncalls_remote = gv(acc_profile).nbytes_remote = 0;

      gv(get_profile).ncalls_global = gv(get_profile).nbytes_global = 0;
      gv(put_profile).ncalls_global = gv(put_profile).nbytes_global = 0;
      gv(acc_profile).ncalls_global = gv(acc_profile).nbytes_global = 0;

      gv(get_profile).ncalls_elocal = gv(get_profile).nbytes_global = 0;
      gv(put_profile).ncalls_elocal = gv(put_profile).nbytes_global = 0;
      gv(acc_profile).ncalls_elocal = gv(acc_profile).nbytes_global = 0;

      gv(get_profile).ncalls_eremote = gv(get_profile).nbytes_eremote = 0;
      gv(put_profile).ncalls_eremote = gv(put_profile).nbytes_eremote = 0;
      gv(acc_profile).ncalls_eremote = gv(acc_profile).nbytes_eremote = 0;
    # endif

      gv(bcast_profile).ncalls = gv(bcast_profile).nbytes = 0;
      gv(bcast_profile).ncalls_shmem = gv(bcast_profile).nbytes_shmem = 0;

   }
 # endif
/* -------------------------------------- *\
   End of counter initialization routines
\* -------------------------------------- */


/* ------------------------------- *\
\* ------------------------------- */
   void Init_scratch(int argc,char *argv[]) {
      int i;
      char *scrdir = NULL;

   /* ---------------------------------- *\
      Check node arguments (Priority #1)
   \* ---------------------------------- */


   /* -------------------------------- *\
      Check command line (Priority #2) 
   \* -------------------------------- */
      for(i=0; i<argc; i++) {
        if(strcmp(argv[i],"-scr") == 0) {
           Chdir(argv[i+1]);
           return;
        }
      }


   /* ---------------------------------------- *\
      Check environment variable (Priority #3)
   \* ---------------------------------------- */
      if((scrdir=getenv("DDI_SCRATCH")) != NULL)  Chdir(scrdir);

   }

