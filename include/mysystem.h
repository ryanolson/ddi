/* ----------------------------------------------------------- *\
   Include file containing common include files and prototypes
   for wrappers to system calls.

   Author: Ryan M. Olson
   CVS $Id: mysystem.h,v 1.2 2006/06/15 18:39:23 ryan Exp $
\* ----------------------------------------------------------- */

/* --------------------------------------- *\
   System Dependent definitions: Sun SPARC
\* --------------------------------------- */
 # if (defined SUN32 || defined SUN64)
 # define _REENTRANT
 # endif

 # define NO_COLLECTIVE_SMP

/* --------------------------- *\
   Commonly used include files
\* --------------------------- */
 # include <stdio.h>
 # include <fcntl.h>
 # include <errno.h>
 # include <unistd.h>
 # include <stdlib.h>
 # include <signal.h>
 # include <string.h>
 # include <strings.h>


/* ---------------------------------------------- *\
   System includes: come before standard includes
\* ---------------------------------------------- */
 # include <sys/param.h>
 # include <sys/time.h>
 # include <sys/types.h>
 # include <sys/resource.h>
 # include <unistd.h>

/* ------------------------------------------ *\
   System V IPC Includes & Wrapper functions
   Subroutine definitions found in sysv_ipc.c
\* ------------------------------------------ */
 # if defined USE_SYSV

 # if defined __CYGWIN__ 
 #   define _KERNEL
 # endif

 # include <sys/ipc.h>
 # include <sys/shm.h>
 # include <sys/sem.h>

   int   Shmget(key_t,size_t,int);
   int   Shmctl(int,int,struct shmid_ds*);
   void *Shmat(int,void*,int);

   int   Semget(key_t,int,int);
   int   Semop(int,struct sembuf*,size_t);

 # if defined __CYGWIN__ 
 #   if !defined SHM_R 
 #     define SHM_R IPC_R
 #   endif
 #   if !defined SHM_W 
 #     define SHM_W IPC_W
 #   endif
 # endif

 # endif


/* -------- *\
   pThreads
\* -------- */
 # if (!defined CRAYXT3 && !defined IBMBG)
 # include <pthread.h>
 # endif


/* --------------------------------------------- *\
   TCP Sockets Includes & Wrapper functions
   Subroutine definitions found in tcp_sockets.c
\* --------------------------------------------- */
 # if defined DDI_SOC
 # include <netdb.h>
 # include <sys/socket.h>
 # include <netinet/in.h>
 # include <netinet/tcp.h>

   int     Accept(int,struct sockaddr*,socklen_t*);
   int     Connect(int,const struct sockaddr*,socklen_t);
   ssize_t Recv(int,void*,size_t,int);  /* Fully blocking receive -- MSG_WAITALL */
   ssize_t Send(int,const void*,size_t,int);
 # endif


/* ---------------- *\
   Microsleep timer
\* ---------------- */
/* int usleep(useconds_t); */


/* --- *\
   MPI
\* --- */
 # if defined DDI_MPI
 # include <mpi.h>
 # endif


/* ---------------- *\
   LAPI -- IBM SP's
\* ---------------- */
 # if defined DDI_LAPI
 # include <lapi.h>
 # endif


/* --------------------------------------------- *\
   Wrapper functions for standard system calls
   Subroutines definitions found in std_system.c
\* --------------------------------------------- */
   void *Malloc(size_t);
   int Chdir(const char *);
   int Execvp(const char *,char *const argv[]);

 # if defined DDI_SOC
   struct hostent *Gethostbyname(const char*);
 # endif

/* ----------------------- *\
   Define Max & Min Macros
\* ----------------------- */
 # define max(a,b) ((a) > (b) ? (a) : (b))
 # define min(a,b) ((a) < (b) ? (a) : (b))

