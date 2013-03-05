/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Subroutine to issue a receive from any compute process.  If multiple
 * incoming message are received concurrently, only one will be
 * received.  To get the others, additional calls to DDI_Recvany or
 * DDI_Recv will need to be issued.
 *
 * Author: Ryan M. Olson
 * CVS $Id: ddi_recvany.c,v 1.2 2006/05/23 20:17:51 ryan Exp $
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"


/* -------------------------------------------------------------------- *\
   DDI_Recvany(buff,size,from)
   ===========================
   [OUT] buff - address of receive buffer.
   [IN]  size - size of receive buffer.
   [OUT] from - rank of sending processs.
   
   Waits for an incoming message from any of the compute processes.
\* -------------------------------------------------------------------- */
   void DDI_Recvany(void *buff,size_t size,int *from) {

   /* ----------------------------- *\
      TCP/IP Socket Implemenatation
   \* ----------------------------- */
    # if defined DDI_SOC
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);
    
   /* --------------- *\
      Local Variables
   \* --------------- */
    # if defined SOC_SYNC
      char ack=37;
    # endif

      int i,j,maxfd;
      int np = comm->np;
      fd_set recvlist;
      static int last = 0;
   
      DEBUG_OUT(LVL3,(stdout,"%s: entering recvany.\n",DDI_Id()))

   /* -------------------------------------------- *\
      Loop on the following until data is received:
   \* -------------------------------------------- */
      for(*from=-1; *from == -1;) {
      
      /* ------------------------------------------------------- *\
         Clear the file descriptor set and initialize the values
         to the value of the sockets you will be waiting on.
      \* ------------------------------------------------------- */
         FD_ZERO(&recvlist);
         for(i=0,maxfd=0; i<np; i++) {
           if( gv(sockets)[i] < 0 ) continue;
           maxfd = max(maxfd,gv(sockets)[i]);
           FD_SET(gv(sockets)[i],&recvlist);
         }
         maxfd++;


      /* ----------------------------------------------------------- *\
         TCP/IP select call puts the process asleep until there is
         incoming activity on one of the sockets in the recvlist set
      \* ----------------------------------------------------------- */
         DEBUG_OUT(LVL3,(stdout,"%s: before select.\n",DDI_Id()))
         select(maxfd,&recvlist,NULL,NULL,NULL);
         DEBUG_OUT(LVL3,(stdout,"%s: after select.\n",DDI_Id()))
      
      
      /* ------------------------------------------------------------ *\
         Scan through the recvlist set (starting at the last receive)
         to find which socket has data coming in on it.
      \* ------------------------------------------------------------ */
         for(i=0; i<np; i++) {
            j = (last + i) % np;
            if(gv(sockets)[j] < 0) continue;
            if(FD_ISSET(gv(sockets)[j],&recvlist)) {
            
            /* ------------------------------------------------------ *\
               The jth socket has an incoming message to be received.
            \* ------------------------------------------------------ */
               if(Recv(gv(sockets)[j],buff,size,0) == -1) {
               
               /* ----------------------------------------------------- *\
                  If an error occurs in the receive, we turn off this
                  socket.  If using TCP/IP sockets only, the ddikick
                  program will be responsible of killing off processes,
                  otherwise self-terminate.
               \* ----------------------------------------------------- */
                  gv(sockets)[j] = -1;
                  *from = -1;
                  
                # if defined DDI_MPI
                  Fatal_error(911);
                # endif
                
               } else {
               
               /* -------------------------------------------------- *\
                  If data is successfully received, return the value
                  of the process the data came from and return.
               \* -------------------------------------------------- */
                  *from = last = j;
                  return;
               }
            }
         }
      }
      
    # endif

   }

