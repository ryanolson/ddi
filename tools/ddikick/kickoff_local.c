/* ----------------------------------------------------------------- *\
   GENERALIZED DISTRIBUTED DATA INTERFACE - 'MPMD' DATA SERVER MODEL
   =================================================================
   
   Kickoff_Local(RANK,NODE,DDINODES,INFO)
   ======================================
   [IN] RANK     - Rank of local process to be spawned
   [IN] NODE     - Rank of local node
   [IN] DDINODES - Parsed node information
   [IN] INFO     - Command-line information
   
   Author: Ryan M. Olson
   CVS $Id: kickoff_local.c,v 1.5 2004/01/07 22:54:30 ryan Exp $
\* ----------------------------------------------------------------- */
 # include "ddikick.h"

   void Kickoff_Local(int rank,int node,const Node_info *ddinodes,const Cmdline_info *info) {
        int i,r,iarg,nargs;
        pid_t pid = 0;
        char ddiinfo[] = "-ddi";
        char procid[8],portid[8],nodeid[8],snodes[8],sprocs[8];
        char **rargs,**argv = info->argv;
   
     /* --------------------------------------------------------------- *\
        Turn the rank of the processor, the rank of the node and the
        # of processors on this node into a string for the command line
     \* --------------------------------------------------------------- */ 
        sprintf(procid, "%d", rank);
        sprintf(nodeid, "%d", node);
        
     /* ------------------------------------------------------- *\
        Turn the socket port into a string for the command line
     \* ------------------------------------------------------- */
        sprintf(portid, "%d", info->kickoffport);
        
     /* --------------------------------------------------- *\
        Turn the number of nodes & processors into a string
     \* --------------------------------------------------- */
        sprintf(snodes, "%d", info->nnodes);
        sprintf(sprocs, "%d", info->nprocs);     
   
     /* ----------------------------------------- *\
        Fork a new child process
        The original process is assigned a value.
        The new process has a value of 0.
     \* ----------------------------------------- */
        pid = fork();

        if(pid == -1) {
           fprintf(stderr," ddikick.x error: fork failed (errno=%i).\n",errno);
           Fatal_error(911);
        }
   
     /* ------------------------- *\
        Original process persists 
     \* ------------------------- */
        if(pid != 0) {
           return;
        }

           
     /* ----------------------------------- *\
        This is executed by the new process
     \* ----------------------------------- */
      # if DDI_DEBUG
        DEBUG_START(DEBUG_STD)
        fprintf(stdout,"Attemping to create DDI process %i on local node %i.\n",rank,node);
        DEBUG_END()
      # endif
   
   
     /* ---------------------------------------- *\
        Initialize arguments for the DDI process
     \* ---------------------------------------- */
        nargs = info->ddiarg + info->nnodes + 8;
        rargs = (char **) Malloc(nargs*sizeof(char*));
   
        for(i=1,r=0; i<info->ddiarg-1; i++) rargs[r++] = argv[i];
   
        rargs[r++] = ddiinfo;
        rargs[r++] = info->kickoffhost;    /*   kickoff host name     */
        rargs[r++] = portid;               /*   kickoff port number   */
        rargs[r++] = nodeid;               /*   rank of this node     */
        rargs[r++] = procid;               /*   rank of this process  */
        rargs[r++] = snodes;               /*   number of nodes       */
        rargs[r++] = sprocs;               /*   number of processors  */
  
        for(i=0,iarg=info->nodearg; i<info->nnodes; i++,iarg++) {
           rargs[r++] = argv[iarg];
        }   
          
        rargs[r] = NULL;


     /* ----------------------------------------------- *\
        Kickoff local ddi task command-line & arguments
     \* ----------------------------------------------- */
      # if DDI_DEBUG
        DEBUG_START(DEBUG_MAX)
        fprintf(stdout,"DDI Process %i Command Line: ",rank);
        for(i=0; i<r; i++) fprintf(stdout,"%s ",rargs[i]);
        fprintf(stdout,"\n");
        DEBUG_END()
      # endif

           
     /* ---------------------------------------------------- *\
        'execvp' launches the executable (p ==> path search)
     \* ---------------------------------------------------- */
        if( Execvp(argv[1],rargs) == -1 ) {
          fprintf(stderr," ddikick.x error: execvp failed in Kickoff_Local.\n");
          exit(911);
        }
   }
