/*
 *  3 Sep 03 - MWD - if ISEND/IRECV/WAIT not compiled, bomb jobs out
 * 16 Jun 03 - RMO - include ISEND/IRECV/WAIT functionality
 * 28 Jan 03 - MWS - include 64 bit Sun version
 * 12 Aug 02 - ZK  - include 64 bit HP version
 * 20 Jun 02 - FPA - include Itanium version
 * 17 Apr 02 - MWS - alter SGI version
 * 24 Jan 02 - MWS - include modifications for Absoft compiler
 *  1 May 00 - MWS - wee change for 64 bit IBM
 * 25 Mar 00 - CC  - modifications for Cray SV1
 *  6 Jun 99 - GDF/MWS - low level socket message passing calls implemented
 *
 * ddisoc.c
 * -----------------------------------------------------------------------
 *       DISTRIBUTED DATA INTERFACE - `SPMD' DATA SERVER MODEL
 *     TCP/IP socket initialization and message passing routines
 *      Written by Graham Fletcher and Mike Schmidt in May 1999
 *  
 *   This file contains machine dependent code, in "ifdef" clauses.
 *   To port this to another machine, you have to settle five issues:
 *      1. Include files to define the 'fd' macros used in soc_rcvany,
 *         and to define the 'timeval' structure must be located.
 *      2. FORTRAN to C calling convention, specifically does the
 *         FORTRAN compiler generate upper or lower case subroutine
 *         names, and does it put underscores at the end of these?
 *      3. length of FORTRAN integer arguments might be 32 or 64 bits.
 *      4. check man page of getsockname for its 3rd argument's declaration
 *      5. Think about TCP/IP system buffers.  To see default value, run
 *              sizesize = sizeof(size);
 *              getsockopt( sock, SOL_SOCKET, SO_RCVBUF, &size,
 *                          (getsockcast *) &sizesize);
 *              printf("default SO_RCVBUF[kick] is %d \n",size);
 *         for the first socket call (pipe back to kickoff program).
 *         The default value is a reasonable guess, otherwise optimize
 *         to minimize global sum time.  Set to 0 to accept system default.
 * -----------------------------------------------------------------------
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

/*
 *           globally accessible values
 */
#define MXHOST 256
extern int errno;
static int sockets[512],kickoffsock,myproc,nproc;

/*
 *    declarations for the non-blocking point-to-point communications
 */
#ifdef SKIPTHREADS
#else
   void *soc_thread_send();
   void *soc_thread_recv();
   typedef struct { void* buff; int size,node; } soc_msg;
   static soc_msg socmsg_send;
   static soc_msg socmsg_recv;
#endif

/*   ----- begin machine dependency section ------ */

/*  this is for AXP machines, whether labeled Digital, Compaq, or HP  */
#ifdef COMPAQ
#include <sys/select.h>
#include <sys/time.h>
#define SOC_INIT      soc_init_
#define SOC_NPROC     soc_nproc_
#define SOC_SEND      soc_send_
#define SOC_RECV      soc_recv_
#define SOC_ISEND     soc_isend_
#define SOC_IRECV     soc_irecv_
#define SOC_WAIT      soc_wait_
#define SOC_GSEND     soc_gsend_
#define SOC_GRECV     soc_grecv_
#define SOC_RCVANY    soc_rcvany_
#define SOC_WAKEKICK  soc_wakekick_
#define SOC_KILLKICK  soc_killkick_
#define FORTINT long
#define getsockcast int
#define SOCK_BUFF_SIZE 0  /* unoptimized */
#endif

/*
 * The following works with the Cray SV1.
 */
#ifdef CRAY
#include <sys/select.h>
#include <sys/time.h>
#define SOC_INIT      SOC_INIT
#define SOC_NPROC     SOC_NPROC
#define SOC_SEND      SOC_SEND
#define SOC_RECV      SOC_RECV
#define SOC_ISEND     SOC_ISEND
#define SOC_ISEND     SOC_ISEND
#define SOC_WAIT      SOC_WAIT
#define SOC_GSEND     SOC_GSEND
#define SOC_GRECV     SOC_GRECV
#define SOC_RCVANY    SOC_RCVANY
#define SOC_WAKEKICK  SOC_WAKEKICK
#define SOC_KILLKICK  SOC_KILLKICK
#define iargc_        IARGC
#define getarg_       GETARG
#define FORTINT long
#define getsockcast int
#define SOCK_BUFF_SIZE 0 /* unoptimized */
#endif

/*  this is for HP-UX machines  */
#if (defined HPUX32) || (defined HPUX64)
#include <sys/time.h>
#define SOC_INIT      soc_init
#define SOC_NPROC     soc_nproc
#define SOC_SEND      soc_send
#define SOC_RECV      soc_recv
#define SOC_ISEND     soc_isend
#define SOC_IRECV     soc_irecv
#define SOC_WAIT      soc_wait
#define SOC_GSEND     soc_gsend
#define SOC_GRECV     soc_grecv
#define SOC_RCVANY    soc_rcvany
#define SOC_WAKEKICK  soc_wakekick
#define SOC_KILLKICK  soc_killkick
#define getsockcast int
#define SOCK_BUFF_SIZE 0  /* unoptimized */
#endif
#ifdef HPUX32
#define FORTINT int
#endif
#ifdef HPUX64
#define FORTINT long
#endif

/*   by this we mean any RS/6000 except not the SP parallel machine   */
/*   and of course this does not mean the AS/400 or S/390  */
#if (defined IBM32) || (defined IBM64)
#include <sys/select.h>
#include <sys/times.h>
#define SOC_INIT      soc_init
#define SOC_NPROC     soc_nproc
#define SOC_SEND      soc_send
#define SOC_RECV      soc_recv
#define SOC_ISEND     soc_isend
#define SOC_IRECV     soc_irecv
#define SOC_WAIT      soc_wait
#define SOC_GSEND     soc_gsend
#define SOC_GRECV     soc_grecv
#define SOC_RCVANY    soc_rcvany
#define SOC_WAKEKICK  soc_wakekick
#define SOC_KILLKICK  soc_killkick
#define SOCK_BUFF_SIZE 0  /* default 128K is best for fast or gigabit */
#endif
#ifdef IBM32
#define FORTINT int
#define getsockcast size_t
#endif
#ifdef IBM64
#define FORTINT long
#define getsockcast unsigned int
#endif

/*    Absoft's compiler, which sometimes shows up on Linux/Mac OS X  */

#ifdef ABSOFT
#include <sys/select.h>
#include <sys/time.h>
#define FORTINT int
#define getsockcast int
#define SOCK_BUFF_SIZE 0 /* unoptimized */
#endif

/*
 * The following is for RedHat Linux, using f2c/gcc compiler.
 * For g77, use ?? (not tested)
 * For Portland Group pgf77: use only 1 trailing underscore (not tested)
 * We have heard that Slackware Linux lacks select.h, and that code works
 * if its include line is simply commented out.
 */
#if (defined LINUX) || (defined MACOSX)
#include <sys/select.h>
#include <sys/time.h>
#define SOC_INIT      soc_init__
#define SOC_NPROC     soc_nproc__
#define SOC_SEND      soc_send__
#define SOC_RECV      soc_recv__
#define SOC_ISEND     soc_isend__
#define SOC_IRECV     soc_irecv__
#define SOC_WAIT      soc_wait__
#define SOC_GSEND     soc_gsend__
#define SOC_GRECV     soc_grecv__
#define SOC_RCVANY    soc_rcvany__
#define SOC_WAKEKICK  soc_wakekick__
#define SOC_KILLKICK  soc_killkick__
#define FORTINT int
#define getsockcast int
#define SOCK_BUFF_SIZE 32768  /* half RedHat 5.1 default of 64K is better */
#endif

/*
 * The following is for Linux on Itanium, using GCC 3.0.4 or later.
 *   note the inclusion of <inttypes.h> and the type declaration of
 *   FORTINT versus 32-bit linux.  FPA 04/2002
 */
#ifdef LINUXIA64
#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#define SOC_INIT      soc_init_
#define SOC_NPROC     soc_nproc_
#define SOC_SEND      soc_send_
#define SOC_RECV      soc_recv_
#define SOC_ISEND     soc_isend_
#define SOC_IRECV     soc_irecv_
#define SOC_WAIT      soc_wait_
#define SOC_GSEND     soc_gsend_
#define SOC_GRECV     soc_grecv_
#define SOC_RCVANY    soc_rcvany_
#define SOC_WAKEKICK  soc_wakekick_
#define SOC_KILLKICK  soc_killkick_
#define FORTINT intptr_t
#define getsockcast socklen_t
#define SOCK_BUFF_SIZE 32768  /* half RedHat 5.1 default of 64K is better */
#endif

#if (defined SGI32) || (defined SGI64)
#include <sys/select.h>
#include <sys/time.h>
#define SOC_INIT      soc_init_
#define SOC_NPROC     soc_nproc_
#define SOC_SEND      soc_send_
#define SOC_RECV      soc_recv_
#define SOC_ISEND     soc_isend_
#define SOC_IRECV     soc_irecv_
#define SOC_WAIT      soc_wait_
#define SOC_GSEND     soc_gsend_
#define SOC_GRECV     soc_grecv_
#define SOC_RCVANY    soc_rcvany_
#define SOC_WAKEKICK  soc_wakekick_
#define SOC_KILLKICK  soc_killkick_
#define getsockcast int
#define SOCK_BUFF_SIZE 0 /* unoptimized */
#endif
#ifdef SGI32
#define FORTINT int
#endif
#ifdef SGI64
#define FORTINT long
#endif

#if (defined SUN32) || (defined SUN64)
#include <sys/select.h>
#include <sys/time.h>
#define SOC_INIT      soc_init_
#define SOC_NPROC     soc_nproc_
#define SOC_SEND      soc_send_
#define SOC_RECV      soc_recv_
#define SOC_ISEND     soc_isend_
#define SOC_IRECV     soc_irecv_
#define SOC_WAIT      soc_wait_
#define SOC_GSEND     soc_gsend_
#define SOC_GRECV     soc_grecv_
#define SOC_RCVANY    soc_rcvany_
#define SOC_WAKEKICK  soc_wakekick_
#define SOC_KILLKICK  soc_killkick_
#define getsockcast int
#define SOCK_BUFF_SIZE 0 /* unoptimized */
#endif
#ifdef SUN32
#define FORTINT int
#endif
#ifdef SUN64
#define FORTINT long
#endif

/*   ----- end machine dependency section ------ */

/*
 *           signal handling routine
 */
void handleSIG(sig)
int sig;
{
   signal(SIGINT, SIG_IGN);   /* restore default processing */
   fprintf(stderr,"child process %d: interrupted by parent \n",myproc);
   (void) fflush(stdout);
   (void) fflush(stderr);
   exit(1);
}

/*
 * -----------------------------------------------------------------------
 *           soc_INIT        child processes call this first
 * -----------------------------------------------------------------------
 */
void SOC_INIT()
{
   extern int iargc_();
   extern char *strdup();
   extern void SOC_INIT2();
/*
               Cray doesn't do getarg like anyone else,
               so we have to look at some ugly machine
               dependent code here.  Note that 6 other
               brands were all the same and thus all 
               machine dependent code was encapsulated in
               one place.  It is not exactly pleasing to
               have this exception here.
*/
#ifndef CRAY
   extern void getarg_();
   int argc, i, lenarg, lenmax=200;
   char *argv[MXHOST+7], arg[200];
#else
   extern int getarg_();
   int argc, i, junk, lenarg, lenmax=25;
   char *argv[MXHOST+7], arg[25*8];
#endif
/*
 *  form of command line passed by 'ddikick' is:
 *  exe jobname kickoff-host port myproc scratchdir nhosts host0 host1 host2...
 *  Obtain this argument list, then call the real initialization routine.
 */
   argc = iargc_() + 1;
   for (i=0; i<argc; i++) {
#ifndef CRAY
      getarg_(&i, arg, lenmax);
#else
      junk = getarg_(&i, arg, &lenmax);
#endif
      for(lenarg = lenmax-2; lenarg && (arg[lenarg] == ' '); lenarg--);
      lenarg++;
      arg[lenarg] = (int) NULL;
      argv[i] = strdup(arg);
   }
   argv[argc] = NULL;
   SOC_INIT2( argc, argv);
}

void SOC_INIT2( argc, argv )
     int argc;                  /*  number of command line arguments  */ 
     char *argv[MXHOST+7];      /*  pointers to command line arguments  */ 
{
int i, j, nhosts, port, sock, ihost, on=1, rc, size;
getsockcast len;
struct sockaddr_in netsoc;
struct hostent *hp;
char sport[8], ack;
/*
 *           get command line arguments
 */
hp = gethostbyname( argv[2] );         /*  kickoff host name   */
port   =      atoi( argv[3] );         /*  kickoff port number */
myproc =      atoi( argv[4] );         /*  my process ID       */
nhosts =      atoi( argv[6] );         /*  number of hosts     */

/*
 *           data-server model (2 processes per host)
 */
nproc = 2*nhosts;
/*
 *           define a handler for interrupt signals from the parent
 */
signal(SIGINT, handleSIG);
/*
 *           --------------------------
 *           CONNECT TO KICKOFF PROCESS
 *           --------------------------
 *
 *           create socket 
 */
sock = socket( AF_INET, SOCK_STREAM, 0 );
setsockopt( sock, IPPROTO_TCP, TCP_NODELAY, (char *) &on, sizeof(on));
/*
 *           construct the socket name
 */
netsoc.sin_family = AF_INET;
/*
 *           fill in sockaddr structure
 */
bcopy( (char *) hp->h_addr, (char *) &netsoc.sin_addr, hp->h_length );
/*
 *           `hton' stands for host-to-network
 *           `s' indicates (unsigned) short integer data type
 *           htons converts host byte ordering to the 
 *           network byte ordering for ports
 */
netsoc.sin_port = htons( (ushort) port );
/*
 *           connect on the named socket 
 */
againcon1:
 if (connect( sock, (struct sockaddr *) &netsoc, sizeof(netsoc) )< 0)
 {
        if (errno == EINTR)
           goto againcon1;
        else
        {
           (void) printf(
              "Connect to kickoff socket by process %d has failed, error %d\n",
               myproc,errno);
           (void) fflush(stdout);
        }
 }
/*
 *           save kickoff socket
 */
kickoffsock = sock;
/*
 *          initialize sockets array to zero
 */
for( i=0; i<nproc; i++ ) sockets[i] = 0;
/*
 *
 * -----------------------------------------------------------------------
 *          ESTABLISH CHILD PROCESS INTERCONNECTIONS
 * -----------------------------------------------------------------------
 * The following quote from "Unix Network Programming. Networking APIs: 
 * sockets and XTI" by W.Richard Stevens, Prentice-Hall, 1998
 *   An everyday analogy for establishing a TCP connection is the telephone.
 *   The "socket" function is the equivalent of having a telephone to use.
 *   "bind" is telling other people your telephone number so they can call
 *   you.  "listen" is turning on the ringer so that you will hear when
 *   an incoming call arrives.  "connect" requires that we have the other
 *   person's telephone number and dial it.  "accept" is when the person
 *   being called answers the phone.  Having the client's identity returned
 *   by "accept" is similar to Caller ID, except that the caller's phone
 *   is learned only after answering the call.
 * helps establish an overview of what this code does.  The book serves as
 * an outstanding reference for various details of TCP/IP system calls.
 *
 *          n.b. data-servers need not interconnect 
 */
for( i=1; i<nproc; i++ ){
  if ( i < nhosts ) {
     ihost = i;                   /*   compute process  */
  } else {
     ihost = i-nhosts;            /*   data server process  */
  }
  for( j=0; j<i && j<nhosts ; j++ ){
    if ( myproc == i ){
      /*
       *  make a "socket"
       */
      sock = socket( AF_INET, SOCK_STREAM, 0 );
      /*
       *  1. possible tuning of network buffer sizes.
       *  2. don't "delay send to coalesce packets" to optimize latency
       */
      if (SOCK_BUFF_SIZE > 0) {
         size = SOCK_BUFF_SIZE;
         setsockopt( sock,SOL_SOCKET,SO_RCVBUF,(char *) &size,sizeof(size));
         setsockopt( sock,SOL_SOCKET,SO_SNDBUF,(char *) &size,sizeof(size));
      }
      setsockopt( sock, IPPROTO_TCP, TCP_NODELAY, (char *) &on, sizeof(on) );
      /*
       *  send port number via kickoff and accept
       */
      netsoc.sin_family      = AF_INET;
      netsoc.sin_addr.s_addr = INADDR_ANY;
      netsoc.sin_port        = 0;
      bind( sock, (struct sockaddr *) &netsoc, sizeof( netsoc ) );
      len = (getsockcast) sizeof( netsoc );
      getsockname( sock, (struct sockaddr *) &netsoc, &len );
      port = ntohs( netsoc.sin_port );
      sprintf( sport, "%d", port );
      /*
       *     complete the socket/bind/listen "passive open" sequence.
       */
againlist:
      if (listen( sock, 1 ) < 0)
      {
            if (errno == EINTR)
                    goto againlist;
            else
            (void) fprintf(stderr, "Listen failed\n");
      }
      /*
       *   now we can send the port number by way of the kickoff program
       *   so that the other end can execute its "connect" to our "accept".
       */
      send( kickoffsock, (void *) sport, sizeof(sport), 0 );
      recv( kickoffsock, (void *) &ack, 1, 0 );   /*  acknowledgement  */

againacc:
      sockets[j] = accept( sock, (struct sockaddr *) NULL, NULL );
      if (sockets[j] < 0 )
      {
            if (errno == EINTR)
                    goto againacc;
            else
            (void) fprintf(stderr, "Accept failed\n");
      }
      /*
       *  parameters are supposed to be inherited from the so-called
       *  listening socket, but here we repeat to be absolutely certain.
       */
      if (SOCK_BUFF_SIZE > 0) {
         size = SOCK_BUFF_SIZE;
         setsockopt( sock,SOL_SOCKET,SO_RCVBUF,(char *) &size,sizeof(size));
         setsockopt( sock,SOL_SOCKET,SO_SNDBUF,(char *) &size,sizeof(size));
      }
      setsockopt( sockets[j],IPPROTO_TCP,TCP_NODELAY,(char *) &on,sizeof(on) );
      /*
       *  and we now close the initial "listening socket".
       */
      close( sock );    
    } 
      else if ( myproc == j )
    {
      /*
       *  make a socket, and optimize it
       */
      sock = socket( AF_INET, SOCK_STREAM, 0 );
      if (SOCK_BUFF_SIZE > 0) {
         size = SOCK_BUFF_SIZE;
         setsockopt( sock,SOL_SOCKET,SO_RCVBUF,(char *) &size,sizeof(size));
         setsockopt( sock,SOL_SOCKET,SO_SNDBUF,(char *) &size,sizeof(size));
      }
      setsockopt( sock, IPPROTO_TCP, TCP_NODELAY, (char *) &on, sizeof(on));
      /*
       *  recv port number via kickoff and connect back
       */
      netsoc.sin_family      = AF_INET;
      recv( kickoffsock, (void *) sport, sizeof(sport), 0 );
      send( kickoffsock, (void *) &ack, 1, 0 );   /*  acknowledgement  */

      hp   = gethostbyname( argv[ihost+7] );
      bcopy( (char *) hp->h_addr, (char *) &netsoc.sin_addr, hp->h_length );
      port = atoi( sport );
      netsoc.sin_port = htons( (ushort) port );
againcon2:
      if (connect( sock, (struct sockaddr *) &netsoc, sizeof(netsoc)) < 0 )
      {
      if (errno == EINTR)
         goto againcon2;
      else
         (void) fprintf(stderr,
            "Process %d to %d connect failed, error=%d, sock=%d\n",
                i,j,errno,sock);
         (void) fflush(stderr);
      }
      sockets[i] = sock;
    }
  }
}
/*
 *          that's it! child processes can now exchange messages
 */

/*
 *          change to working directory
 */
   rc = chdir(argv[5]);
   if (rc != 0) {
      printf("ddiinit: error changing to working directory %s on node %d \n",
                 argv[5],myproc);
      (void) fflush(stdout);
      exit(rc);
   }
}

/*
 * -----------------------------------------------------------------------
 *           parallel environment FORTRAN interface
 * -----------------------------------------------------------------------
 */
void SOC_NPROC( nnodes, mynode )
FORTINT *nnodes, *mynode;
{
*nnodes = nproc/2;   /*   data-server model  */
*mynode = myproc;
}
/*
 * -----------------------------------------------------------------------
 *           synchronous SEND subroutine with FORTRAN interface 
 * -----------------------------------------------------------------------
 */
void SOC_SEND( buff, msglen, to )
char *buff;
FORTINT *msglen, *to;
{
int nbytes, idest;
char ack;
nbytes = *msglen;
idest = *to;
send( sockets[idest], buff, nbytes, 0 );
recv( sockets[idest], &ack, 1, 0 );  /* synch acknowledgement */
}

/*
 * -----------------------------------------------------------------------
 *           synchronous RECEIVE subroutine with FORTRAN interface
 * -----------------------------------------------------------------------
 */
void SOC_RECV( buff, msglen, from )
char *buff;
FORTINT *msglen, *from;
{
int nsent, nbytes, ifrom;
char ack;

nbytes = *msglen;
ifrom = *from;
while( nbytes > 0 ) {
    nsent = recv( sockets[ifrom], buff, nbytes, 0 );
    buff   += nsent;
    nbytes -= nsent;
  }
send( sockets[ifrom], &ack, 1, 0 );  /* synch acknowledgement */
}

/*
 *    if a thread library doesn't exist, e.g. on a very ancient Unix,
 *    such as my 1995 release of Digital Unix version 3.2, use of
 *    the following code will permit linking of GAMESS and execution
 *    of anything that doesn't need asynchronous communications.
 */
/* start of the asynchronous routines' conditional compilation */

#ifdef SKIPTHREADS

void SOC_ISEND(buff,msglen,to,ireqsnd)
char *buff;
FORTINT *msglen, *to;
void *ireqsnd;
{
fflush(stdout);
fprintf(stdout,"call to unimplemented DDI_ISEND routine....\n");
fflush(stdout);
exit(1);
}
void SOC_IRECV(buff,msglen,from,ireqrcv)
char *buff;
FORTINT *msglen, *from;
void *ireqrcv;
{
fflush(stdout);
fprintf(stdout,"call to unimplemented DDI_IRECV routine....\n");
fflush(stdout);
exit(1);
}
void SOC_WAIT(ireq)
void *ireq;
{return;}

#else

/*
 * -----------------------------------------------------------------------
 *           non-blocking SEND subroutine with FORTRAN interface 
 *
 *       The implementation here starts up a thread, and this 
 *       independent "process" carries out a normal TCP/IP send.
 *       The termination of the thread is the sign that the send
 *       has been completed, detected by WAIT.  Since the thread
 *       is using the same socket, no other message should be
 *       sent in the same direction between this pair of nodes,
 *       until the WAIT for this ISEND is completed.
 * -----------------------------------------------------------------------
 */
void SOC_ISEND(buff,msglen,to,ireqsnd)
char *buff;
FORTINT *msglen, *to;
void *ireqsnd;
{
pthread_attr_t thread_attr;

socmsg_send.buff = (void *) buff;
socmsg_send.size = (int) *msglen;
socmsg_send.node = (int) *to;

pthread_attr_init(&thread_attr);
pthread_attr_setscope(&thread_attr,PTHREAD_SCOPE_SYSTEM);

if( pthread_create((pthread_t *) ireqsnd,
                    &thread_attr,
                    soc_thread_send,
                   (void *) NULL) != 0 ) {
   printf("SOC_ISEND: pthread_create error\n");
   fflush(stdout);
   }
}

/*   this is the thread started by the socket ISEND     */
void *soc_thread_send(myarg)
void *myarg;
{ 
int idest, nbytes;
char *buff;

idest  =          socmsg_send.node;
buff   = (char *) socmsg_send.buff;
nbytes =          socmsg_send.size;

send( sockets[idest], buff, nbytes, 0 );

return(NULL);
}

/*
 * -----------------------------------------------------------------------
 *           non-blocking RECV subroutine with FORTRAN interface 
 *
 *       See comments in SOC_ISEND, which apply here exactly the same.
 * -----------------------------------------------------------------------
 */
void SOC_IRECV(buff,msglen,from,ireqrcv)
char *buff;
FORTINT *msglen, *from;
void *ireqrcv;
{
pthread_attr_t thread_attr;

socmsg_recv.buff = (void *) buff;
socmsg_recv.size = (int) *msglen;
socmsg_recv.node = (int) *from;

pthread_attr_init(&thread_attr);
pthread_attr_setscope(&thread_attr,PTHREAD_SCOPE_SYSTEM);

if( pthread_create((pthread_t *) ireqrcv,
                    &thread_attr,
                    soc_thread_recv,
                   (void *) NULL) != 0 ) {
   printf("SOC_IRECV: pthread_create error\n");
   fflush(stdout);
   }
}

/*   this is the thread started by the socket IRECV     */
void *soc_thread_recv(myarg)
void *myarg;
{
int nsent, nbytes, ifrom;
char *buff;

ifrom  =          socmsg_recv.node;
buff   = (char *) socmsg_recv.buff;
nbytes =          socmsg_recv.size;

while( nbytes > 0 ) {
    nsent = recv( sockets[ifrom], buff, nbytes, 0 );
    buff   += nsent;
    nbytes -= nsent;
  }

return(NULL);
}

/*
 * -----------------------------------------------------------------------
 *           WAIT subroutine with FORTRAN interface
 *
 *     WAIT for the non-blocking ISEND or IRECV to be finished.
 *     The implementation just tests for the exit of the thread
 *     which is doing the communication, and hence the argument
 *     is the thread identifier that was returned when either
 *     ISEND or IWAIT was invoked.
 * -----------------------------------------------------------------------
 */
void SOC_WAIT(ireq)
void *ireq;
{
pthread_t *pireq = (pthread_t *) ireq;
unsigned long *luval = NULL;

if( pthread_join(*pireq,NULL) != 0 ) {
   luval = (unsigned long *) ireq;
   fprintf(stdout,"SOC_WAIT: pthread_join error on req #%lu\n",*luval);
   fflush(stdout);
  }
}
#endif   /* end of the asynchronous routines' conditional compilation */

/*
 * -----------------------------------------------------------------------
 *           global SEND routine with FORTRAN interface.
 *           This point-to-point routine is to be used in the individual
 *           messages implementing global sum and broadcast only.
 *
 *           The bulk data transfer is preceeded by a short message
 *           giving the length of the bulk transfer.  It looks as if
 *           this is to be used for error checking on the other end,
 *           but the actual purpose is to interpose a short message.
 *           This paces the sequence of messages through the binary
 *           tree so that nodes do not have simultaneous demands on
 *           the buffer space for bulk messages.  Or so we think, this
 *           really works, but has elements of black magic about it.
 *           Note also that the ack byte of SEND/RECV is omitted.
 * -----------------------------------------------------------------------
 */
void SOC_GSEND( buff, msglen, node )
FORTINT *node, *msglen;
char *buff;
{
int nbytes, idest;
char nsent;

nbytes = *msglen;
idest = *node;

nsent = nbytes;
send( sockets[idest], &nsent, sizeof(nsent), 0 );
send( sockets[idest], buff, nbytes, 0 );
}
/*
 * -----------------------------------------------------------------------
 *           global operation RECEIVE routine with FORTRAN interface.
 *           see comment in SOC_GSEND routine regarding the strategy.
 * -----------------------------------------------------------------------
 */
void SOC_GRECV( buff, msglen, node )
FORTINT *node, *msglen;
char *buff;
{
int nrecv, nbytes, ifrom;
char nsent;

nbytes = *msglen;
ifrom = *node;

/*    it is an error if nsent exceeds msglen, but we don't test this  */
recv( sockets[ifrom], &nsent, sizeof(nsent), 0 );

while( nbytes > 0 ) {
    nrecv = recv( sockets[ifrom], buff, nbytes, 0 );
    buff   += nrecv;
    nbytes -= nrecv;
  }
}
/*
 * -----------------------------------------------------------------------
 *           synchronous RECEIVE on any socket 
 * -----------------------------------------------------------------------
 */
void SOC_RCVANY( buff, msglen, node )
FORTINT *msglen, *node;
char *buff;
{
int anynode, nready, maxsoc, i;
fd_set readlist;
struct timeval timeout;

/*
 *           wait for messages using select
 */
FD_ZERO( &readlist );
maxsoc = 0;
for( i=0; i<nproc; i++ )
{
  if( sockets[i] > maxsoc ) maxsoc = sockets[i];
  if( sockets[i] != 0 ) FD_SET( sockets[i], &readlist );
}
select( maxsoc+1, &readlist, NULL, NULL, NULL );
/*
 *           poll sockets for input
 */
timeout.tv_sec  =  0;
timeout.tv_usec =  0;
anynode         = -1;
nready          =  0;
while( nready <= 0 )
{
  anynode++;
  if ( anynode == nproc ) anynode=0;
  if ( sockets[anynode] != 0 )
  {
    FD_ZERO( &readlist );
    FD_SET( sockets[anynode], &readlist );
    nready = select( sockets[anynode]+1, &readlist,
    (fd_set *) NULL, (fd_set *) NULL, &timeout );
  }
}
*node = anynode;
SOC_RECV( buff, msglen, node );  
}

/* -----------------------------------------------------------------------
 *           notify kickoff program if normal end is about to occur.
 * -----------------------------------------------------------------------
 * Send a message to kickoff program so it knows we are near termination.
 * The actual content of the message is irrelevant, as the kickoff program
 * just expects to hear from every compute process so it knows that it
 * should suspend special signal handling.
 */
void SOC_WAKEKICK(istat)
FORTINT *istat;
{
char kterm, ack;
int istatus;
istatus = *istat;
kterm = -1;
if(istatus == 0) {
   send( kickoffsock, (void *) &kterm, sizeof(kterm), 0);
   recv( kickoffsock, (void *) &ack, 1, 0 );  /* synch acknowledgement */
   }
}

/* -----------------------------------------------------------------------
 *    terminating compute processes report this to the kickoff program.
 * -----------------------------------------------------------------------
 * Send a message to kickoff program so it knows our data server is
 * now terminated, wait for acknowledgement message back, then we are
 * outtahere.  Actual message content is irrelevant.
 */
void SOC_KILLKICK()
{
char kterm, ack;
kterm = -1;
send( kickoffsock, (void *) &kterm, sizeof(kterm), 0);
recv( kickoffsock, (void *) &ack, 1, 0 );  /* synch acknowledgement */
}
