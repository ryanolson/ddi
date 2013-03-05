/* ----------------------------------------------------------------- *\
   GENERALIZED DISTRIBUTED DATA INTERFACE - 'MPMD' DATA SERVER MODEL
   =================================================================
   
   Kickoff_PBS(DDINODES,INFO)
   ==========================
   [IN] DDINODES - Node information.
   [IN] INFO     - Command-line information.
   
   Using command-line and the parsed node information, Kickoff_PBS
   spawns DDI processes through the PBS Task Management (TM) API.
   
   Author: Ryan M. Olson
   CVS $Id: kickoff_pbs.c,v 1.8 2004/03/29 20:12:23 ryan Exp $
\* ----------------------------------------------------------------- */
 # include "ddikick.h"
 # if defined USE_PBS 

   void Kickoff_PBS(const Node_info *ddinodes,const Cmdline_info *info) {
      char ddiinfo[] = "-ddi";
      char procid[8];
      char portid[8];
      char nodeid[8];
      char snodes[8];
      char sprocs[8];
      char **rargs;
      char **argv = info->argv;
      int i,j,r,iarg,nargs = info->ddiarg + info->nnodes + 8;
      int inode,ncpus,np = info->nprocs;
      int ntests;

      if(info->nnodes == 1) return;

      int tm_errno;
      tm_task_id *tid;
      tm_event_t *spawn;
      tm_event_t polled;
      struct tm_roots roots;
      tm_node_id *nodelist;


   /* ---------------------------------- *\
      Initialize PBS Task Management API
   \* ---------------------------------- */
      if(tm_init(0, &roots) != TM_SUCCESS) {
         fprintf(stderr, " ddikick.x: tm_init failed\n");
         Fatal_error(911);
      }

      if(tm_nodeinfo(&nodelist, &np) != TM_SUCCESS) {
         fprintf(stderr, " ddikick.x: tm_nodeinfo failed.\n");
         Fatal_error(911);
      }

      tid   = (tm_task_id *) Malloc(2*np*sizeof(tm_task_id)); 
      spawn = (tm_event_t *) Malloc(2*np*sizeof(tm_event_t));

      for(i=0; i<2*np; i++) {
         *(tid + i)   = TM_NULL_TASK;
         *(spawn + i) = TM_NULL_EVENT;
      }


   /* ----------------------------------------- *\
      Initialize arguments to kickoff DDI tasks
   \* ----------------------------------------- */
      rargs = (char **) Malloc(nargs*sizeof(char*));

      sprintf(portid, "%d", info->kickoffport);
      sprintf(snodes, "%d", info->nnodes);
      sprintf(sprocs, "%d", info->nprocs);     

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


   /* ------------------------ *\
      Spawn DDI tasks to nodes
   \* ------------------------ */
      ncpus=ddinodes[0].cpus+ddinodes[1].cpus;
      for(i=ddinodes[0].cpus,inode=1; i<np; i++) {
         
         if(i == ncpus) ncpus += ddinodes[++inode].cpus;
         
         sprintf(nodeid,"%d",inode);
         sprintf(procid,"%d",i);

       # if DDI_DEBUG
         DEBUG_START(DEBUG_MAX)
         fprintf(stdout,"DDI Process %i PBS tm_spawn arguments: ",i);
         for(iarg=0; iarg<r; iarg++) fprintf(stdout,"%s ",rargs[iarg]);
         fprintf(stdout,"\n");
         DEBUG_END()
       # endif

      /* ------------------------- *\
         Spawn DDI Compute Process
      \* ------------------------- */
         if(tm_spawn(r,rargs,NULL,*(nodelist+i),(tid+i),spawn+i) != TM_SUCCESS) {
            fprintf(stderr," ddikick.x: tm_spawn failed.\n");
            Fatal_error(911);
         }


      /* ---------------------------------- *\
         No data server on single node runs
      \* ---------------------------------- */
         if(info->nnodes == 1) continue;


       # if DDI_DEBUG
         DEBUG_START(DEBUG_MAX)
         fprintf(stdout,"DDI Process %i PBS tm_spawn arguments: ",j);
         for(iarg=0; iarg<r; iarg++) fprintf(stdout,"%s ",rargs[iarg]);
         fprintf(stdout,"\n");
         DEBUG_END()
       # endif

         j = i+np;
         sprintf(procid,"%d",j);
         
      /* --------------------- *\
         Spawn DDI Data Server
      \* --------------------- */
         if(tm_spawn(r,rargs,NULL,*(nodelist+i),(tid+j),spawn+j) != TM_SUCCESS) {
            fprintf(stderr," ddikick.x: tm_spawn failed.\n");
            Fatal_error(911);
      }  }


   /* -------------------------------------------------------- *\
      Poll PBS to ensure each DDI process started successfully
   \* -------------------------------------------------------- */
      ntests = np-ddinodes[0].cpus;
      if(USING_DATA_SERVERS())  ntests *= 2;

      for(i=ntests; i--; ) {
         if(tm_poll(TM_NULL_EVENT,&polled,1,&tm_errno) != TM_SUCCESS) {
            fprintf(stderr," ddikick.x: tm_poll failed.\n");
            Fatal_error(911);
         }
         
         for(j=0; j<np; j++) {
            if(polled == *(spawn+j)) {
               if(tm_errno) {
                  fprintf(stderr," ddikick.x: error spawning DDI task %i.\n",j);
                  Fatal_error(911);
               } else {
                # if DDI_DEBUG
                  DEBUG_START(DEBUG_MAX)
                  fprintf(stdout," ddikick.x: DDI task %i started.\n",j);
                  DEBUG_END()
                # endif
            }  }

            if(info->nnodes == 1) continue;

            if(polled == *(spawn+j+np)) {
               if(tm_errno) {
                  fprintf(stderr," ddikick.x: error spawning DDI task %i.\n",j+np);
                  Fatal_error(911);
               } else {
                # if DDI_DEBUG
                  DEBUG_START(DEBUG_MAX)
                  fprintf(stdout," ddikick.x: DDI task %i started.\n",j+np);
                  DEBUG_END()
                # endif
      }  }  }  }

      
   /* -------------------------------------- *\
      Close the link to the PBS Task Manager
   \* -------------------------------------- */
      tm_finalize();


   /* ---------------- *\
      Free used memory
   \* ---------------- */
      free(tid);
      free(spawn);
      free(rargs);      
   }

 # else

   void PBS_Kickoff_dummy(void) { return; }

 # endif
