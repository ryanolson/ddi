/* ----------------------------------------------------------------- *\
   GENERALIZED DISTRIBUTED DATA INTERFACE - 'MPMD' DATA SERVER MODEL
   =================================================================
   
   ddikick.x program
   =================

   DDI kickoff program.  Used to spawn DDI processes to local/remote
   nodes, to initialize DDI processes, and to clean up DDI processes
   in the event of an error.
   
   Author: Ryan M. Olson
   CVS $Id: ddikick.c,v 1.3 2006/05/29 19:29:53 ryan Exp $
\* ----------------------------------------------------------------- */
 # include "ddikick.h"


/* ------------------------------ *\
   Global variables for ddikick.x
\* ------------------------------ */
   DDI_Comm gv(ddi_base_comm);
   int      gv(master)  = 1;
   int      gv(nprocs)  = 0;
   int     *gv(sockets) = NULL;
   int      gv(ppn)     = 1;
   char    *gv(netext)  = NULL;
   int      gv(nnodes)[1];


/* ---------------------------------------------------------- *\
 * Main Kickoff Program -- Syntax                             *
 * ==============================                             *
 * ddikick.x /path/executable [executable-options]            *
 *           -ddi nn np { node1-info, node2-info, ... }       *
\* ---------------------------------------------------------- */
   int main(int argc, char *argv[]) {

    /* --------------- *\
       Local Variables
    \* --------------- */
       size_t size;
       Node_info *ddinodes;
       Cmdline_info info;
       int i,ln,hn,nprocs,nnodes;
       int start,nsocks,ncpus,master=1;
       pthread_t accept_thread;
       pthread_attr_t thread_attr;
       char kickoffhost[256];
       char envstr[256];
       char *remoteshell = NULL;
       struct hostent *remotehost;


    /* ---------------------------------------------- *\
       Separate the ddi info from the executable info
    \* ---------------------------------------------- */
       info.argc = argc;
       info.argv = argv;
       
       for(i=0; i<argc && strcmp(argv[i],"-ddi") != 0; i++);
       if(i==argc) {
          fprintf(stderr," Fatal Error: DDI arguments not found on the command-line.\n");
          fflush(stdout); fflush(stderr);
          exit(911);
       }
       
       info.ddiarg  = ++i;
       info.nnodes  = atoi(argv[i++]);
       info.nprocs  = atoi(argv[i++]);
       info.nodearg = i;
       
       ln = 0;
       hn = info.nnodes;
       nprocs = info.nprocs;
       nnodes = info.nnodes;
       gv(nprocs) = nprocs;
       gv(nnodes)[0] = nnodes;
       gv(ddi_base_comm).np = nprocs;
       gv(ddi_base_comm).nn = nnodes;


    /* -------------------------------------------- *\
       Check to ensure compile-time limits are met.
    \* -------------------------------------------- */
       if(nnodes > MAX_NODES) {
          fprintf(stdout," DDI: Compile limit MAX_NODES = %i\n",MAX_NODES);
          fprintf(stdout," DDI: Requested number of nodes = %i\n",nnodes);
          fprintf(stdout," DDI: Increase MAX_NODES and recompile DDI.\n");
          fflush(stdout);
          exit(911);
       }

       nsocks = nprocs;
       if(USING_DATA_SERVERS()) nsocks *= 2;
    
       ULTRA_DEBUG((stdout," ddikick.x: finished with -ddi argument.\n"))
   
       for(i=info.ddiarg; i<argc && strcmp(argv[i],"-dditree") != 0; i++);
       if(i != argc) {
          gv(master) = master = 0;
          info.kickoffhost = (char *) strdup(argv[++i]);
          info.kickoffport = atoi(argv[++i]);
          ln = atoi(argv[++i]);
          hn = atoi(argv[++i]);
          remoteshell = argv[++i];
       } else {
          if((remoteshell = getenv("DDI_RSH")) == NULL) {
              remoteshell = (char *) strdup("rsh");
          }
       }

       ULTRA_DEBUG((stdout," ddikick.x: finished with -dditree argument\n"))

       for(i=info.ddiarg,gv(ppn)=1; i<argc-1 && strcmp(argv[i],"-ppn") != 0; i++);
       if(++i < argc) gv(ppn) = atoi(argv[i]);

       ULTRA_DEBUG((stdout," ddikick.x: finished with -ppn argument\n"))
/*
       for(i=info.ddiarg,gv(netext)=NULL; i<argc-1 && strcmp(argv[i],"-netext") != 0; i++);
       if(++i < argv) gv(netext) = (char *) strdup(argv[i]);
*/

    /* ----------------------------- *\
       Check for a scratch directory
    \* ----------------------------- */
       for(i=0; i<argc-1 && strcmp(argv[i],"-scr") != 0; i++);
       if(++i < argc) {
          sprintf(envstr,"%s=%s","DDI_SCRATCH",argv[i]);
          if(putenv(envstr)) {
             fprintf(stdout,"ddikick.x: putenv failed.\n");
             Fatal_error(911);
          }
       }
       ULTRA_DEBUG((stdout," ddikick.x: finished with -scr argument.\n")) 


    /* ---------------------------------------- *\
       Allocate memory for the node information
    \* ---------------------------------------- */
       ddinodes = (Node_info *) Malloc(nnodes*sizeof(Node_info));


    /* ---------------------------------- *\
       Parse through the node information
    \* ---------------------------------- */
       if( Parse_node_args(argc,argv,info.nodearg,info.nnodes,ddinodes) < 0 ) {
         fprintf(stderr," ddikick.x : Unable to parse the node arguments.\n");
         Fatal_error(911);
       }
       ULTRA_DEBUG((stdout," ddikick.x: finished parsing arguments.\n")) 

       for(i=0,ncpus=0; i<nnodes; i++) {
        # if !defined USE_SYSV
          if(ddinodes[i].cpus > 1) {
             fprintf(stdout,"%s %s\n%s %s\n%s\n",
                            "ddikick error: inconsistent hostlist arguments.",
                            "hostname:cpus=X was detected with X > 1.  This",
                            "style of input is only allowed if DDI is using",
                            "shared-memory.  Either fix the hostlist or",
                            "recompile DDI to use shared-memory.");
             Fatal_error(911);
          }
        # endif
          ncpus += ddinodes[i].cpus;
       }

       if( ncpus != nprocs ) {
         fprintf(stderr," Unable to confirm the total number of cpus.\n");
         fprintf(stderr," The command-line requests %i cpus.\n",info.nprocs);
         fprintf(stderr," The total number of cpus from the node arguments is %i.\n",ncpus);
         Fatal_error(911);
       }


    /* ------------------------------------------------------- *\
       If this is the master kickoff program, print the banner
    \* ------------------------------------------------------- */
       if(master) {
/*
          fprintf(stdout," ----------------------------------------------------------------- \n");
          fprintf(stdout," GENERALIZED DISTRIBUTED DATA INTERFACE - 'SPMD' DATA SERVER MODEL \n");
          fprintf(stdout,"              TCP/IP socket startup (kickoff) program              \n");
          fprintf(stdout,"                Written by Ryan M. Olson in May 2003               \n");
          fprintf(stdout,"                     Modified from original code                   \n");
          fprintf(stdout,"             Written by Graham Fletcher and Mike Schmidt           \n");
          fprintf(stdout," ----------------------------------------------------------------- \n");
*/
          fprintf(stdout,"\n Distributed Data Interface kickoff program.");
          fprintf(stdout,"\n Initiating %i compute processes on %i nodes to run the following command:\n ",nprocs,nnodes);
          for(i=1; i<info.ddiarg-1; i++) fprintf(stdout,"%s ",argv[i]);
          fprintf(stdout,"\n\n"); fflush(stdout);


       /* ----------------------------------------- *\
          Turn on special signal handling functions
       \* ----------------------------------------- */
          signal(SIGTERM,Fatal_error);
          signal(SIGPIPE,Fatal_error);
          signal(SIGINT, Fatal_error);


       /* ----------------------------------------------------------------- *\
          Get the fully qualified internet name for the master kickoff host
       \* ----------------------------------------------------------------- */
          strncpy(kickoffhost,ddinodes[0].hostname,256); 
          remotehost = Gethostbyname(kickoffhost);
          if(remotehost == NULL) {
             strncpy(kickoffhost, "localhost", 256);
          } else {
             strncpy(kickoffhost, (char *) remotehost->h_name, 256);
             if(strcmp(kickoffhost,"loopback") == 0) strncpy(kickoffhost,"localhost",256);
          }

          info.kickoffsock = -1;
          info.kickoffport = -1;
          info.kickoffhost = (char *) strdup(kickoffhost);
          info.ports       = (int *) Malloc(nsocks*sizeof(int));
          info.sockets     = (int *) Malloc(nsocks*sizeof(int));

          ULTRA_DEBUG((stdout," ddikick.x: kickoff host = %s\n",info.kickoffhost))


       /* ------------------------------------ *\
          Setup global variables for cleaning
          up DDI tasks if a fatal error occurs
       \* ------------------------------------ */
          gv(nprocs)  = info.nprocs;
          gv(sockets) = info.sockets;

          for(i=0; i<nsocks; i++) {
              info.ports[i]    = -1;
              info.sockets[i]  = -1;
          }
                    

       /* ----------------------------------------- *\
          Create a socket for accepting connections
       \* ----------------------------------------- */
          info.kickoffsock = SOC_Create(SERVER_SOCKET,&info.kickoffport,NULL);

        # if DDI_DEBUG
          DEBUG_START(DEBUG_MIN)
          fprintf(stdout," Master Kickoff Host %s is accepting connections on port %i.\n",kickoffhost,info.kickoffport);
          fprintf(stdout," Awaiting connections from %i GDDI processes.\n",nsocks);
          DEBUG_END()
        # endif


       /* ------------------------------------------------- *\
          Create a thread to accept DDI process connections
       \* ------------------------------------------------- */
          pthread_attr_init(&thread_attr);
          pthread_attr_setscope(&thread_attr, PTHREAD_SCOPE_SYSTEM);
          if( pthread_create(&accept_thread,&thread_attr,Accept_tasks,(void *)&info) != 0 ) {
            fprintf(stderr," ddikick.x : Unable to create thread to asynchronous accept socket connections.\n");
            Fatal_error(911);
          }

        # if DDI_DEBUG
          DEBUG_START(DEBUG_MAX)
          fprintf(stdout," ddikick.x : Thread created on %s:%i to accept connections.\n",info.kickoffhost,info.kickoffport);
          DEBUG_END()
        # endif

       } 
        
          
    /* ------------------------------------------------ *\
       Kickoff remote DDI tasks
       ========================
       If PBS is available, check and see if the job is
       running under PBS control, i.e. the environment
       variable PBS_ENVIRONMENT is set.  If so, kickoff
       the DDI tasks using the PBS Task Management API.

       Otherwise, fork/spawn remote ddikick.x processes
       using DDI_RSH (remote shell) whose mission it is
       to create the local intra-node DDI tasks.
    \* ------------------------------------------------ */
     # if defined USE_PBS
       if(getenv("PBS_ENVIRONMENT") != NULL) {
          Kickoff_PBS(ddinodes,&info);
       } else {
     # endif

       while((hn-ln) > 1) {
           Kickoff_Remote(&ln,&hn,ddinodes,&info,remoteshell);
       }

     # if defined USE_PBS
       }
     # endif


    /* ----------------------------------- *\
       Fork off local intra-node DDI tasks
    \* ----------------------------------- */
       for(i=0,start=0; i<ln; i++) start += ddinodes[i].cpus; 

       for(i=0; i<ddinodes[ln].cpus; i++) {
           Kickoff_Local(start+i,ln,ddinodes,&info);
       }

       if(USING_DATA_SERVERS())
       for(i=0; i<ddinodes[ln].cpus; i++) {
           Kickoff_Local(start+i+nprocs,ln,ddinodes,&info);
       }
       

    /* ------------------------------------------ -------------------- *\
       Wait until DDI tasks have successfully begun execution.
       The master ddikick.x waits until all DDI tasks have checked in.
       Remote ddikick.x processes are done, so they terminate.
    \* --------------------------------------------------------------- */
       if(master) {
          
          if( pthread_join(accept_thread,NULL) != 0 ) {
             fprintf(stderr," ddikick.x: pthread_join failed.\n");
             Fatal_error(911);
          }
          
        # if DDI_DEBUG
          DEBUG_START(DEBUG_MIN)
          fprintf(stdout," All DDI processes have completed the initial check-in.\n");
          DEBUG_END()
        # endif
          
       } else {
        
        # if DDI_DEBUG
          DEBUG_START(DEBUG_STD)
          fprintf(stdout," ddikick.x: Remote kickoff process on node %i completed.\n",ln);
          DEBUG_END()
        # endif
         
       /* ------------------------------------------- *\
          ddikick.x process on remote nodes terminate
       \* ------------------------------------------- */
          return 0;
  
       }


    /* ------------------------------------------------------- *\
       Broadcast the list of server ports to all DDI processes
    \* ------------------------------------------------------- */
       size = nsocks*sizeof(int);
       for(i=0; i<nsocks; i++) {
          send(info.sockets[i],info.ports,size,0);
       }


    /* ------------------------------------ *\
       Finished kicking off DDI processes.
       Wait for all the DDI task to finish.
    \* ------------------------------------ */
       Finalize_tasks(info.sockets,nsocks);


    /* --------- *\
       Stop now!
    \* --------- */
       sleep(1);
       fflush(stdout); fflush(stderr);
       fprintf(stdout," ddikick.x: exited gracefully.\n");
       fflush(stdout); fflush(stderr);
       return 0;
   }
