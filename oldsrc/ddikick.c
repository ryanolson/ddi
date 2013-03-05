/*
 * 28 Jan 03 - MWS,DGF,KRG - add 64 bit Sun version, change typings
 * 14 Jan 03 - MWS - trap for TERM signals
 *  7 Aug 02 - ZK  - add 64 bit HP version
 * 20 Jun 02 - FPA - add Itanium version
 * 18 Apr 02 - KRG - changes to signal handling, sleep calls added
 * 28 Mar 02 - BMB - clarify using alternate remote shells, exit after execv
 * 24 Jan 02 - BMB,MWS - hostname test just warns about multiple adapters
 *  6 Sep 01 - MWS - additional attempts to flush output properly
 * 21 Jun 01 - MWS - comments about how to use multiple adapters
 *  1 May 00 - MWS - wee change for 64 bit IBM
 * 25 Mar 00 - CC  - modifications for Cray SV1
 * 29 Aug 99 - MWS - deal with short hostnames from gethostname call
 *  6 Jun 99 - GDF - create kick off program for socket interface
 * 
 * ddikick.c  -->  ddikick.x
 *     form of the command line is: 
 * ddikick.x inputfile exepath exename scratchdir nhosts host0 host1 host2...
 * 
 * ------------------------------------------------------------------------
 *      DISTRIBUTED DATA INTERFACE - 'SPMD' DATA SERVER MODEL
 *           TCP/IP socket startup (kickoff) program
 *     Written by Graham Fletcher and Mike Schmidt in May 1999
 * ------------------------------------------------------------------------
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>

#ifdef LINUXIA64
/* On Mandrake 8.1 pthreads was in pth.h, On RedHat 7.2 it's in pthread.h */
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#endif

#define MXHOST 256

extern int errno;

static int nproc;
static pid_t childpids[2*MXHOST-1];

/* ---just a trace of machine dependency, check ddisoc.c for more detail--- */
#if (defined ABSOFT) ||                     \
    (defined COMPAQ) ||                     \
    (defined CRAY)   ||                     \
    (defined HPUX32) || (defined HPUX64) || \
    (defined LINUX)  || (defined MACOSX) || \
    (defined SGI32)  || (defined SGI64)  || \
    (defined SUN32)  || (defined SUN64)
#define getsockcast int
#endif
#if defined IBM32
#define getsockcast size_t
#endif
#if defined IBM64
#define getsockcast unsigned int
#endif
#if defined LINUXIA64
#define getsockcast socklen_t
#endif
/*
 *           signal handling routine
 *    Kurt says (April 2002) that Linux has added a SIGPARENT so that
 *    the children can learn if ddikick.x suddenly exits, but this is
 *    by no means a standard Unix signal.  So we do not know how to 
 *    program for the case of the parent exiting abruptly without a
 *    chance to terminate the children.
 */
void handleSIGNALS(sig)
int sig;
{
   int i, waitstatus, waitopts, naptime;
   /* restore default processing */
   (void) signal(SIGCHLD, SIG_DFL);
   (void) signal(SIGTERM, SIG_DFL);
   (void) signal(SIGHUP,  SIG_DFL);
   (void) signal(SIGINT,  SIG_DFL);
/*
 *    the sleep delays are to help with output buffering.
 */
   naptime = 1;
   (void) fflush(stdout);
   (void) fflush(stderr);
   (void) sleep(naptime);

   if (sig == SIGCHLD) {
      (void) fprintf(stderr,
     "ddikick: trapped unexpected termination of a child process, SIGCHLD. \n");
   } else if (sig == SIGTERM) {
      (void) fprintf(stderr,
     "ddikick: trapped a standard kill signal, SIGTERM. \n");
   } else if (sig == SIGHUP) {
      (void) fprintf(stderr,
     "ddikick: trapped signal SIGHUP. \n");
   } else if (sig == SIGINT) {
      (void) fprintf(stderr,
     "ddikick: trapped signal SIGINT. \n");
   } else {
      (void) fprintf(stderr,"ddikick: signal trapping confusion! \n");
   }
   (void) fflush(stdout);
   (void) fflush(stderr);
   (void) sleep(naptime);
   /*
    *    Terminate all children still running.
    *    Note that if INT can't kill them, we then proceed to use KILL.
    *    loops are split to send the signals, then wait to be sure the
    *    signals actually worked (ensures all procs are sent the signals).
    */
   for (i=0; i<nproc; i++) 
      { (void) kill(childpids[i], SIGINT); }
   (void) sleep(naptime);

   waitopts = 0;
   for (i=0; i<nproc; i++)
      { waitpid(childpids[i], &waitstatus, waitopts); }
   (void) sleep(naptime);

   for (i=0; i<nproc; i++)
      { (void) kill(childpids[i], SIGKILL); }
   (void) sleep(naptime);

   (void) fprintf(stderr,
      "ddikick: A signal has been sent to interrupt all child processes. \n");
   (void) fprintf(stderr,"ddikick: terminated abnormally. \n");
   (void) fflush(stdout);
   (void) fflush(stderr);
   exit(1);
}

/*
 *---------------------------------
 *    main program of 'ddikick'
 *---------------------------------
 */
int main(argc, argv)
  int argc;     /*  argc is the number of command line arguments */
  char ** argv; /*  argv is pointers to command line arguments   */
{
char fullexename[256], kickoffhost[256], fullremote[256],
     sport[8], si[8], sh[8], ack, kterm;
int i, j, nhosts, ihost, sock, port, on=1, naptime;
pid_t pid;
getsockcast len;
char *rargs[MXHOST+9];   /* kickoff name plus 9 args plus maximum hostnames */
int sockets[2*MXHOST-1]; /* 1 to every compute, 1 to every data server */
int waitstatus, waitopts;
struct sockaddr_in server;
struct hostent *remotehost;
/*
 *           get name of kickoff host
 *  This may not be a fully qualified hostname, believe it or not,
 *  as many people apparently choose to use a short form instead of
 *  matching their true Internet name.  Since the system call will
 *  will return the short name if one is being used, we need to force
 *  a translation to ensure we have a fully dotted Internet hostname.
 *  We also need to deal with the possibility that there isn't any
 *  assigned host name at all (perhaps an unconnected home computer).
 */
gethostname( kickoffhost, 256 );
remotehost = gethostbyname( kickoffhost );
if (remotehost != NULL) {
     (void) strncpy(kickoffhost, (char *) remotehost->h_name, 256);
     if(strcmp(fullremote,"loopback") == 0)
        (void) strncpy(kickoffhost, "localhost", 256);
  } else {
     (void) strncpy(kickoffhost, "localhost", 256);
  }
/*
 *           number of hosts 
 */
nhosts = atoi( argv[5] );
if (nhosts>MXHOST) {
   (void) printf("ddikick: maximum host name count of %d exceeded. \n",MXHOST);
   exit(8);
}
/*
 *           Echo arguments to help users see what is going on.
 *           In particular, verify enough host names were provided.
 */
(void) printf("Initiating %s compute processes for job %s \n",argv[5],argv[1]);
(void) printf("Executable %s will be run from directory %s \n",argv[3],argv[2]);
(void) printf("Working scratch directory on each host will be %s \n",argv[4]);
(void) fflush(stdout);
(void) fflush(stderr);
for (i=1; i<=nhosts; i++) {
   if(argv[i+5] == NULL) {
      (void) printf("error: Hostname %d was not supplied on command line \n",i);
      (void) fflush(stdout);
      (void) fflush(stderr);
      exit(8);
   }
}
/*
 *           make 2 processes per host (one compute process, one data server)
 *           convert number of hosts to character string for command line
 */
nproc = 2*nhosts;    
(void) sprintf( sh, "%d", nhosts );
/*
 *           signal handling
 */
(void) signal(SIGCHLD, handleSIGNALS);
(void) signal(SIGTERM, handleSIGNALS);
(void) signal(SIGHUP,  handleSIGNALS);
(void) signal(SIGINT,  handleSIGNALS);
/*
 * ----------------------------------------------------------
 *           generate the parallel processes
 * ----------------------------------------------------------
 */
for ( i=0; i<nproc; i++ ){ 
  if ( i < nhosts ) {
     ihost = i;                 /*   compute process  */
  } else {
     ihost = i-nhosts;          /*   data server process  */
  }
  /*
   *           create a socket 
   */
  sock = socket( AF_INET, SOCK_STREAM, 0 );
  /*
   *           make a socket name using wildcards
   */
  server.sin_family      = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port        = 0;
  /*
   *           'bind' assigns a port number if the socket name contains 
   *           wildcards (i.e. this is how you get a port number)
   */
  len = (getsockcast) sizeof( server );
  (void) bind( sock, (struct sockaddr *) &server, len );
  /*
   *            get the new socket name provided by 'bind'
   */
  (void) getsockname( sock, (struct sockaddr *) &server, &len ); 
  /*
   *           'listen' initiates connection, sets queue length to 1.
   *           Note the triad "socket/bind/listen" forms a "passive open".
   */
againlist:
  if (listen( sock, 1 ) < 0)
  {
        if (errno == EINTR)
                goto againlist;
        else
        (void) printf("Listen failed \n");
  }
  /*
   *            'ntoh' stands for network-to-host, 
   *            the 's' indicates short integer data type, 
   *            ntohs converts numbers from the network byte ordering 
   *            to the host byte ordering
   */
  port = ntohs( server.sin_port );
  /*
   *           convert port number to a character string 
   *           so it can be placed on the command line
   */
  (void) sprintf( sport, "%d", port );
  /*
   *           convert process ID to a character string for command line
   */
  (void) sprintf( si, "%d", i );
  /*
   *           print which type process we are generating
   */
  if (nhosts <= 4) {
     if ( i<nhosts ) {
        (void) printf(
         "Running %s on %s as compute process %d \n",argv[3],argv[ihost+6],i);
     } else {
        (void) printf(
         "Running %s on %s as data server %d \n",argv[3],argv[ihost+6],i);
     }
  } else {
     if (i == 0)        (void) printf(
         "Running %d compute processes on nodes %s ... ",nhosts,argv[ihost+6]);
     if (i == nhosts-1) (void) printf("%s \n",argv[ihost+6]);
     if (i == nhosts)   (void) printf(
         "Running %d data servers on nodes %s ... ",nhosts,argv[ihost+6]);
     if (i == nproc-1)  (void) printf("%s \n",argv[ihost+6]);
  }
  (void) fflush(stdout);
  (void) fflush(stderr);
  /*
   *           spawn the new process using 'fork'
   */
  pid = fork();
  if ( pid == 0 ) { 
    /*
     *    this code is executed by the new child created by 'fork'....
     */
    remotehost = gethostbyname( argv[ihost+6] );
    if (remotehost != NULL) {
         (void) strncpy(fullremote, (char *) remotehost->h_name, 256);
         if(strcmp(fullremote,"localhost") == 0)
                (void) strncpy(fullremote, kickoffhost, 256);
         if(strcmp(fullremote,"loopback") == 0)
                (void) strncpy(fullremote, kickoffhost, 256);
      } else {
         (void) strncpy(fullremote, argv[ihost+6], 256);
      }

     /*
      *  the very first computational process must be run by 'execvp'
      *  so that the environment variables are passed to it.  In the
      *  case of a custom network, such as Myrinet or Gigabit Ethernet
      *  to be use for the message passing, while the host name is that
      *  of a Fast Ethernet service network, we must not terminate the
      *  job.  Instead we will print a warning, and force use of execvp
      *  for the initial compute process, just below.
      */
    if ((i == 0) && (strcmp( kickoffhost, fullremote) != 0)) {
      (void) fprintf(stderr,
          "   There is possible host name confusion! \n");
      (void) fprintf(stderr,
          "   DDIKICK is running on host %s, but the \n",kickoffhost);
      (void) fprintf(stderr,
          "   first name in the hostname args translates to %s. \n",fullremote);
      (void) fprintf(stderr,
          "   DDIKICK will assume these are two network cards in one host, \n");
      (void) fprintf(stderr,
          "   and will start up compute process 0 on %s. \n",kickoffhost);
    }
    /*
     *      use execvp instead of remote shell if we are on the same host.
     *      remote shell does not pass the environment, execvp does, but
     *      of course execvp only fires up on the local box.
     */
    if ((i == 0) || (strcmp(kickoffhost, fullremote) == 0)
                 || (strcmp(argv[6],  argv[ihost+6]) == 0)) {
      rargs[0]   = argv[3];              /*   executable name     */
      rargs[1]   = argv[1];              /*   input file name     */
      rargs[2]   = kickoffhost;          /*   kickoff host name   */
      rargs[3]   = sport;                /*   kickoff port number */
      rargs[4]   = si;                   /*   this process ID     */
      rargs[5]   = argv[4];              /*   scratch directory   */
      rargs[6]   = sh;                   /*   number of hosts     */
      for ( j=0; j<nhosts; j++ )         /*   list of all         */
        rargs[j+7] = argv[j+6];          /*     remote hosts      */
      rargs[nhosts+7] = NULL;            /* null terminated string */
      /*
       *           'execvp' launches the executable (p = with path search)
       */
      (void) execvp(argv[3],rargs);
      /*
       *  execvp doesn't return, this next should never be reached.
       */
      (void) fprintf(stderr, "ddikick: execv of local process failed! \n");
      exit(1);
    } else {
      /*
       *   ---- use 'remote shell' to move new process to remote host ----
       *   The code uses the standard Unix command
       *      'rsh' and its associated .rhosts authentication.
       *   Some sites may not allow this, preferring the more secure
       *      'ssh' or 'ksh' authentications.
       *   If you need to use one of these replacements, simply change
       *   two non-comment lines below, namely 
       *      1. the starting element of 'rargs' 
       *      2. use the correct full path name in the execv below.
       *
       *   Set up the command-line arguments for the remote shell:
       */
      rargs[0]   = "rsh";                /*   perhaps ssh ?           */
      rargs[1]   = argv[ihost+6];        /*   remote host name        */ 
      (void) strncpy(fullexename, argv[2], 256);
      (void) strncat(fullexename,"/"     , 256-strlen(fullexename));
      (void) strncat(fullexename, argv[3], 256-strlen(fullexename));
      rargs[2]   = fullexename;          /*   executable's path/name  */
      rargs[3]   = argv[1];              /*   input file name         */
      /*
       *           pass info to the remote process on the command line  
       */
      rargs[4]   = kickoffhost;          /*   kickoff host name   */
      rargs[5]   = sport;                /*   kickoff port number */
      rargs[6]   = si;                   /*   this process ID     */
      rargs[7]   = argv[4];              /*   scratch directory   */
      rargs[8]   = sh;                   /*   number of hosts     */
      for ( j=0; j<nhosts; j++ )         /*   list of all         */
        rargs[j+9] = argv[j+6];          /*     remote hosts      */
      rargs[nhosts+9] = (char *) NULL;            /* null terminated string */
      /*
       *           'execv' launches the remote shell command 
       *           Perhaps, /usr/local/bin/ssh ?
       */
      (void) execv("/usr/bin/rsh",rargs);
      /* 
       *  execv doesn't return so we should not reach the following.
       *  If we do, its probably a pathname error, or something
       *  completely incurable, in which case we can only punt.
       */
      (void) fprintf(stderr, "ddikick: execv of remote shell failed! \n");
      exit(1);
    }
  } else {
     /*
      *    this code is executed by the parent process 'ddikick'....
      *    store the child process IDs from each 'fork'.
      */
     childpids[i] = pid;
  }

  /*
   *  ----- "fork" branches are now finished -----
   *  Since the child process was overlaid by execv or a remote shell
   *  of other processes, only the kickoff parent should now be left
   *  executing the following code, below the big 'fork' branch!
   */

  /*
   *           'accept' waits for a connection then returns a new socket
   *           save new socket for message passing to children
   */
againacc:
  sockets[i] = accept( sock, (struct sockaddr *) NULL, NULL );
  if (sockets[i] == -1)
  {
        if (errno == EINTR)
                goto againacc;
        else
        (void) printf("Accept failed \n");
  }
  /*
   *  don't "delay send to coalesce packets" to optimize latency
   */ 
  (void) setsockopt( sockets[i], IPPROTO_TCP, TCP_NODELAY,
                     (char *) &on, sizeof(on));
  /*
   *           'close' old socket no longer needed
   */
  close( sock );
  /*
   *           end of loop over processes
   */
}
/*
 * ------------------------------------------------------------------------
 *           establish child process interconnections 
 * ------------------------------------------------------------------------
 */
for ( i=1; i<nproc; i++ ){
  if ( i < nhosts ) {
     ihost = i;                 /*   compute process  */
  } else {
     ihost = i-nhosts;          /*   data server process  */
  }
  /*
   *         data-servers need not connect with each other 
   */
  for ( j=0; j<i && j<nhosts ; j++ ){
    /*
     *       kickoff relays the child process port numbers so they can connect
     */
    (void) recv( sockets[i], (void *) sport, sizeof(sport), 0 );
    (void) send( sockets[i], (void *) &ack, 1, 0 );          /*  ack msg */
    (void) send( sockets[j], (void *) sport, sizeof(sport), 0 );   
    (void) recv( sockets[j], (void *) &ack, 1, 0 );          /*  ack msg  */
  }
}
/*
 *           all done initializing child-to-child communications!
 */
(void) printf("Process initiation completed. \n");
(void) fflush(stdout);
(void) fflush(stderr);

/*
 *    Now hang around until all the compute processes have reported
 *    in with their termination messages.  First we await the news that
 *    all processes are close to terminating, then we switch off the
 *    special signal handling, then wait again until all compute processes
 *    report that they have been able to shut down their data servers.
 *    Then, it's sayonara, adios, hasta la vista babee.
 */
for ( i=0; i<nhosts; i++ ) {
   (void) recv(sockets[i], (void *) &kterm, sizeof(kterm), 0);
   (void) send(sockets[i], (void *) &ack, 1, 0); 
   }

/*  Kurt thinks INT should continue to be trapped...   */
(void) signal(SIGCHLD, SIG_DFL);

for ( i=0; i<nhosts; i++ ) {
   (void) recv(sockets[i], (void *) &kterm, sizeof(kterm), 0);
   (void) send(sockets[i], (void *) &ack, 1, 0); }

/*    for more info, see WIFCONTINUED in /usr/include/sys/wait.h  */
waitopts = 0;
for (i=0; i<nproc; i++)
   { waitpid(childpids[i], &waitstatus, waitopts); }

/*   this is paranoia, make very sure all children are really dead   */
(void) fflush(stdout);
(void) fflush(stderr);
naptime = 1;
(void) sleep(naptime);
for (i=0; i<nproc; i++)
   { (void) kill(childpids[i], SIGKILL); }

(void) fflush(stdout);
(void) fflush(stderr);
(void) printf("ddikick: all processes have ended gracefully. \n");
(void) fflush(stdout);
(void) fflush(stderr);
return(0);
}
