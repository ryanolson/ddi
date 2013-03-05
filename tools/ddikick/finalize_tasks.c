/* ----------------------------------------------------------------- *\
   GENERALIZED DISTRIBUTED DATA INTERFACE - 'MPMD' DATA SERVER MODEL
   =================================================================
   
   Finalize_tasks(SOCKETS,NSOCKS)
   ==============================
   [IN] SOCKETS - array of sockets connecting to DDI tasks.
   [IN] NSOCKS  - number of DDI task / open sockets.

   Finalize_tasks stealthfully monitors the status of running DDI
   tasks without consuming any CPU time.

   Author: Ryan M. Olson
   CVS $Id: finalize_tasks.c,v 1.4 2004/02/27 21:10:14 ryan Exp $
\* ----------------------------------------------------------------- */
 # include "ddikick.h"

   void Finalize_tasks(int *sockets,int nsocks) {
      char ack=57;
      int i,rank,maxfd,opensockets=nsocks;
      fd_set recvlist;
  
      while(opensockets) {

      /* ------------------------------------ *\
         Build an fd_set for the open sockets
      \* ------------------------------------ */
         maxfd = 0;
         FD_ZERO(&recvlist);
         for(i=0; i<nsocks; i++) {
            if(sockets[i] < 0) continue;
            maxfd = max(maxfd,sockets[i]);
            FD_SET(sockets[i],&recvlist);
         }
         maxfd++;
 
      
      /* ----------------------------- *\
         Wait for an incomming message
      \* ----------------------------- */
         select(maxfd,&recvlist,NULL,NULL,NULL);


      /* -------------------------------------- *\
         Message(s) received from a DDI process
      \* -------------------------------------- */
         for(i=0; i<nsocks; i++) {
            if(sockets[i] < 0) continue;
            if(FD_ISSET(sockets[i],&recvlist)) { 

            /* ------------------------------------------ *\
               socket[i] is readable ==> incoming message
            \* ------------------------------------------ */
               if( recv(sockets[i],&rank,sizeof(int),0) == 0 ) {

               /* ------------------------------------------------------------- *\
                  EOF received over socket ==> Unexpected closure of the socket
               \* ------------------------------------------------------------- */
                  fprintf(stdout," ddikick.x: application process %i quit unexpectedly.\n",i);
                  sockets[i] = -1;
                  Fatal_error(911);

               } else {
        
               /* ----------------------------------- *\
                  Proper termination message received
               \* ----------------------------------- */
                  send(sockets[i],&ack,1,0);
                  Recv(sockets[i],&ack,1,0);
                  FD_CLR(sockets[i],&recvlist);
                  sockets[i] = -1;
                  --opensockets;

                  MAX_DEBUG((stdout," ddikick.x: %i terminated properly.\n",i))
      }  }  }  }

      return;
   }


