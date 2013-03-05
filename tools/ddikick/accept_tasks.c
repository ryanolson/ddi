/* ----------------------------------------------------------------- *\
   GENERALIZED DISTRIBUTED DATA INTERFACE - 'MPMD' DATA SERVER MODEL
   =================================================================
   
   Accept_tasks(MYARG)
   ===================
   [IN] MYARG - pointer to command-line information

   Used to asynchronously accept TCP socket connections to DDI
   processes and to receive the value of the server port for each
   DDI task.

   Author: Ryan M. Olson
   CVS $Id: accept_tasks.c,v 1.7 2004/01/07 22:19:20 ryan Exp $
\* ----------------------------------------------------------------- */
 # include "ddikick.h"

   void *Accept_tasks(void *myarg) {
      fd_set readlist;
      struct timeval timer;
      int sock,rank,port,maxrank,on=1,timeouts=0;
      Cmdline_info *info = (Cmdline_info *) myarg;
      
      int mysock   = info->kickoffsock;
      int *ports   = info->ports;
      int *sockets = info->sockets;

      int nsocks   = info->nprocs;
      if(USING_DATA_SERVERS()) nsocks *= 2;
  
      maxrank = nsocks;
 
      timer.tv_sec  = 60;
      timer.tv_usec = 0;

      
   /* ------------------------------------------------------------ *\
      Our receptionist is now on the clock.
      He/she cannot go home until all the workers have checked-in.
   \* ------------------------------------------------------------ */
      while(nsocks) {
         FD_ZERO(&readlist);
         FD_SET(mysock,&readlist);

         
      /* ----------------------------------------------------- *\
         Wait here until there is an incoming call on 'mysock'
      \* ----------------------------------------------------- */
         while(select(mysock+1,&readlist,NULL,NULL,&timer) == 0) {
            if(++timeouts == DDIKICK_TIMEOUT) {
               fprintf(stderr," ddikick.x: Timed out while waiting for DDI processes to check in.\n");
               Fatal_error(911);
         }  }

         
      /* --------------------------------------------------------- *\
         The receptionist picks up the phone from the incoming
         call on 'mysock' and routes the call to a new line 'sock'
      \* --------------------------------------------------------- */
         sock = Accept(mysock,NULL,NULL);

         
      /* ----------------------------------------------------- *\
         Remove the builtin TCP delay & set send/recv timeouts
      \* ----------------------------------------------------- */
         setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,(void *)&on,sizeof(int));
         /* set send timeout -- future work */
         /* set recv timeout -- future work */

         
      /* ------------------------------------- *\
         The caller now introduces him/herself
      \* ------------------------------------- */
         Recv(sock,&rank,sizeof(int),0);
         Recv(sock,&port,sizeof(int),0);

         if(rank < 0 || rank > maxrank) {
            fprintf(stdout," ddikick.x : received connection with improper introduction.\aborting ...\n");
            Fatal_error(911);
         }
 
         STD_DEBUG((stdout," ddikick.x : %i checked in; receiving via port %i (Remaining=%i).\n",rank,port,nsocks-1))

 
      /* ----------------------------------- *\
         Save caller information, i.e. 
         worker 'nodeid' has now checked-in!
      \* ----------------------------------- */
         if(sockets[rank] > 0) {
            fprintf(stdout," ddikick.x : Multiple processes checking in with the same ID\nAborting ...\n");
            Fatal_error(911);
         }
         
         ports[rank] = port;
         sockets[rank] = sock;
         --nsocks;
      }
 
      STD_DEBUG((stdout," ddikick.x : All the nodes have checked in!!\n"))
      return NULL;
   }


