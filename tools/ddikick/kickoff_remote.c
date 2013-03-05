/* ----------------------------------------------------------------- *\
   GENERALIZED DISTRIBUTED DATA INTERFACE - 'MPMD' DATA SERVER MODEL
   =================================================================
   
   Kickoff_Remote(LOW,HIGH,DDINODES,INFO)
   ======================================
   [IN]     LOW      - Lower branch of nodal binary tree
   [IN/OUT] HIGH     - Upper branch of nodal binary tree
   [IN]     DDINODES - Parsed node information
   [IN]     INFO     - Command-line information
   
   Author: Ryan M. Olson
   CVS $Id: kickoff_remote.c,v 1.7 2004/01/08 21:16:24 ryan Exp $
\* ----------------------------------------------------------------- */
 # include "ddikick.h"

   void Kickoff_Remote(int *low,int *high,const Node_info* ddinodes,const Cmdline_info* info,char *remoteshell) {
        int i,ln,hn,bn,nargs,r=0;
        pid_t pid = 0;
        char sln[8],shn[8],portid[8],snodes[8],sprocs[8];
        char shost[256];
        char ddi[] = "-ddi";
        char scr[] = "-scr";
        char dditree[] = "-dditree";
        char *scratchdir = getenv("DDI_SCRATCH");
        char **rargs,**argv = info->argv;
  
     /* ---------------------------------------------- *\
        Determine the branch point on the binary tree.
     \* ---------------------------------------------- */
        ln = *low;
        hn = *high;
        bn = ln + (hn-ln)/2;
   
         
     /* ------------------------------------------------ *\
        Fork a new child process
        The original process is assigned a value for pid
        The new process ==> pid == 0.
     \* ------------------------------------------------ */
        pid = fork();

        if(pid == -1) {
          fprintf(stderr," ddikick.x: Fork failed (errno=%i).\n",errno);
          Fatal_error(911);
        }
 
     /* -------------------------------------- *\
        If I am the original process, do this:
     \* -------------------------------------- */
        if(pid != 0) {
           *high = bn;
           return;
        }
        
     /* ----------------------------------- *\
        This is executed by the new process
     \* ----------------------------------- */
        ln = bn;
        
     /* ---------------------------------------- *\
        Turn the binary tree limits into strings
     \* ---------------------------------------- */
        sprintf(sln, "%d", ln);
        sprintf(shn, "%d", hn);
        sprintf(snodes, "%d", info->nnodes);
        sprintf(sprocs, "%d", info->nprocs);
        sprintf(portid, "%d", info->kickoffport);
   
   
     /* ------------------------------------------ *\
        Get the name of the node in which to login
     \* ------------------------------------------ */
        strcpy(shost,ddinodes[ln].hostname);


     /* ---------------------------------------- *\
        Initialize arguments for the DDI process
     \* ---------------------------------------- */
        nargs = info->ddiarg + info->nnodes + 50;
        rargs = (char **) Malloc(nargs*sizeof(char*));

/*  
    From Brett Bode:   Feb 2007
    For desktop systems without SystemV memory set up,
    and without any rsh/ssh configuration, we'd still
    like to be able to run parallel INSIDE that desktop.
    The dodge is to set up multiple "localhost localhost..." for
    the hostname list, for we can't use :cpu= w/o shared memory.
    However, the multiple host names then look "remote" and we
    want to escape from needing to set up rsh/ssh.
    Use of the special host name "localhost" accomplishes this!
    Note that srtncmp returns 0 (false!) if the strings match,
                      but 1 (true) if they don't match.
*/
        if (strncmp(shost, "localhost", 9)) {
           rargs[r++] = remoteshell;          /*  remote shell string     */
           rargs[r++] = shost;                /*  node in which to login  */
        }
           
        for(i=0; i<info->ddiarg-1; i++)
        rargs[r++] = argv[i];

        rargs[r++] = ddi;                  /*   -ddi arg                 */
        rargs[r++] = snodes;               /*   number of nodes          */
        rargs[r++] = sprocs;               /*   number of processors     */
           
        for(i=0; i<info->nnodes; i++) 
        rargs[r++] = argv[info->nodearg+i];

        rargs[r++] = dditree;              /*   -dditree arg             */
        rargs[r++] = info->kickoffhost;    /*   kickoffhost name         */
        rargs[r++] = portid;               /*   kickoffhost server port  */
        rargs[r++] = sln;                  /*   low node on binary tree  */
        rargs[r++] = shn;                  /*   high node on binary tree */
        rargs[r++] = remoteshell;          /*   remote shell command     */
           
        if(scratchdir) {
           rargs[r++] = scr;               /*   -scr arg                 */
           rargs[r++] = scratchdir;        /*   scratch directory path   */
        }
	
        rargs[r] = NULL;


     /* ---------------------------------- *\
        Kickoff remote ddikick.x arguments
     \* ---------------------------------- */
      # if DDI_DEBUG
        DEBUG_START(DEBUG_STD)
        fprintf(stdout," ddikick.x: execvp command line: ");
        for(i=0; i<r; i++) fprintf(stdout,"%s ",rargs[i]);
        fprintf(stdout,"\n");
        DEBUG_END()
      # endif


     /* ---------------------------------------------------------- *\
        Forked process is morphed into a DDI_RSH process by execvp
     \* ---------------------------------------------------------- */
        if(Execvp(rargs[0],rargs) == -1) {
           fprintf(stderr,"ddikick.x: execvp failed in Kickoff_Remote.\n");
           fprintf(stderr,"Possible remedies include:\n");
           fprintf(stderr,"1. rsh may not be on your path, insert\n");
           fprintf(stderr,"      setenv DDI_RSH /usr/bin/rsh\n");
           fprintf(stderr,"2. check .rhost authentication file\n");
           fprintf(stderr,"3. if rsh is not allowed on your system, insert\n");
           fprintf(stderr,"      setenv DDI_RSH /full/path/to/ssh\n");
           fprintf(stderr,"4. check path leading to ddikick.x,\n");
           fprintf(stderr,"   and remote access to ddikick.x on all nodes.\n");
           exit(911);
        }
   }
