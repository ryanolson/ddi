/* -------------------------------------------------------- *\
   Create a TCP/IP Socket.  There are two types of sockets:
   
   Type = SERVER_SOCKET
   ====================
   Creates a socket, gets a port number for the new socket,
   then begins to listen, i.e. connections can be accepted 
   on the new socket.
   
   Note: "New" connections can not be optimized until they 
   are accepted.
   
   Type = CLIENT_SOCKET
   ====================
   Creates a socket and sets the destination address to the
   hostname and port provided as calling arguments.
   
   Author: Ryan M. Olson
   CVS $Id: soc_create.c,v 1.1.1.1 2005/11/17 00:12:34 ryan Exp $
\* -------------------------------------------------------- */
 # include "mysystem.h"
 # include "common.h"
   
   int SOC_Create(int type,int *port,const char *hostname) {

    # if defined DDI_SOC
      int on = 1;
      int sock;
      socklen_t len;
      char myhost[256];
      struct sockaddr_in server;
      struct hostent *hp;
      struct sockaddr_in netsoc;

    # if defined SOC_BUFFER_SIZE
      int buffer = 0;
    # endif

   /* --------------- *\
      Create a Socket
   \* --------------- */
      sock = socket(AF_INET,SOCK_STREAM,0);

   /* ----------------------- *\
      Optimize the new socket
   \* ----------------------- */
      setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,(void *) &on,sizeof(int));

   /* ---------------------- *\
      Set socket buffer size
   \* ---------------------- */
    # if defined SOC_BUFFER_SIZE
      buffer = SOC_BUFFER_SIZE;
      setsockopt(sock,SOL_SOCKET,SO_RCVBUF,(void *) &buffer,sizeof(int));
      setsockopt(sock,SOL_SOCKET,SO_SNDBUF,(void *) &buffer,sizeof(int));
    # endif

   /* -------------------- *\
      Set socket ownership
   \* -------------------- */
      fcntl(sock,F_SETOWN,getpid());

      if(type == SERVER_SOCKET) {
         
      /* ---------------------------------- *\
         Make a Socket Name using Wildcards
      \* ---------------------------------- */
         server.sin_family       =  AF_INET;
         server.sin_addr.s_addr  =  INADDR_ANY;
         server.sin_port         =  0;
    
      /* -------------------------------------------------------- *\
         'bind' assigns a port number if the socket name contains
         wildcards (i.e. this is how you get a port number
      \* -------------------------------------------------------- */   
         len = (socklen_t) sizeof(server);
         bind(sock, (struct sockaddr *) &server, len);
               
      /* ------------------------------------------ *\
         Get the new socket name provided by 'bind'
      \* ------------------------------------------ */    
         getsockname(sock, (struct sockaddr *) &server, &len);
      
      /* ------------------------------------------------------ *\
         Turn the socket on, i.e. it can now accept connections
      \* ------------------------------------------------------ */
         listen(sock,5);
         
      /* ---------------------------- *\
         Save the value of the port &
         Return the new SERVER_SOCKET
      \* ---------------------------- */
         *port = ntohs(server.sin_port);
         return sock;
         
         
      } else if(type == CLIENT_SOCKET) {
      
      /* -------------------- *\
         Initialize Host Info
      \* -------------------- */
         hp = Gethostbyname(hostname);
         netsoc.sin_family = AF_INET;
         netsoc.sin_port = htons((ushort) *port);
         bcopy((char *)hp->h_addr,(char *)&netsoc.sin_addr, hp->h_length);

      /* ------------------------------------ *\
         Finalize the socket by connecting it
      \* ------------------------------------ */
         if(Connect(sock,(struct sockaddr *) &netsoc,sizeof(netsoc)) < 0) {
            gethostname(myhost,256);
            fprintf(stdout," TCP: Connect failed. %s -> %s:%i.\n",myhost,hostname,*port);
            Fatal_error(911); 
         }
         return sock;
         
      } else {
      
         fprintf(stdout,"SOC_Create Error: Unknown type of socket.\n");
         fflush(stdout);
         fflush(stderr);
         exit(1);
         
      }

    # endif

      return -1;
   }
