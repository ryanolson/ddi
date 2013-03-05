/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Subroutines associated with signal handling.
 *
 * Author: Ryan M. Olson
 * CVS $Id: ddi_signals.c,v 1.1.1.1 2005/11/17 00:12:34 ryan Exp $
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"


/* -------------------------------------------------- *\
   Signal handling function for SIGURG.
   Recieve a message from the kickoff program, which:
   1) Resets the probe counter, or
   2) Kill the DDI process.
\* -------------------------------------------------- */
   void _sigurg_hndlr(int signo) {
   # if defined DDI_SOC
     char msg = 1;
     recv(gv(kickoffsock),&msg,1,MSG_OOB);
     if(msg != 1) Fatal_error(signo);
   # endif
   }


/* ---------------------------------------------------- *\
   Signal handling function for many common DDI errors.
   If called, this function kills the DDI process.
\* ---------------------------------------------------- */
   void Fatal_error(int signo) {
    
    # if defined DDI_MPI
      int i;
      char ack=0;
    # endif
 
      int me,np;
      SMP_Data *smp_data = NULL;
      DDI_NProc(&np,&me);

   /* -------------------------------------------------------- *\
      A fatal error has occurred and the program must quit ...
      Trapping further signals might interfer with the cleanup
   \* -------------------------------------------------------- */
      signal(SIGURG,SIG_IGN);
      signal(SIGPIPE,SIG_IGN);

      switch(signo) {
      case SIGFPE:
        fprintf(stdout,"%s: trapped a floating point error (SIGFPE).\n",DDI_Id());
        break;
      case SIGTERM:
        fprintf(stdout,"%s: trapped a termination signal (SIGTERM).\n",DDI_Id());
        break;
      case SIGURG:
        fprintf(stdout,"%s: terminated upon request.\n",DDI_Id());
        break;
      case SIGSEGV:
        fprintf(stdout,"%s: trapped a segmentation fault (SIGSEGV).\n",DDI_Id());
        break;
      case SIGPIPE:
        fprintf(stdout,"%s: SIGPIPE trapped.\n",DDI_Id());
        break;
      case SIGINT:
        fprintf(stdout,"%s: SIGINT trapped.\n",DDI_Id());
        break;
      case SIGILL:
        fprintf(stdout,"%s: SIGILL trapped.\n",DDI_Id());
        break;
      case SIGQUIT:
        fprintf(stdout,"%s: SIGQUIT trapped.\n",DDI_Id());
        break;
      case SIGXCPU:
        fprintf(stdout,"%s: process exceeded CPU time limit (SIGXCPU).\n",DDI_Id());
        break;
      default:
        fprintf(stdout," A fatal error occurred on%s.\n",DDI_Id());
        break;
      };

      fflush(stdout);
      fflush(stderr);


   /* --------------------------------------------------------------------------- *\
      If running the mixed code, kill the other processes by SIGURG messages over
      TCP sockets, rather than dealing with the implementation specific MPI_ABORT
   \* --------------------------------------------------------------------------- */ 
    # if defined DDI_MPI && defined DDI_SOC
      if(signo != SIGURG) { /* kill the compute processes */
         fprintf(stdout,"%s: Killing remaining DDI processes.\n",DDI_Id()); fflush(stdout);
         for(i=0; i<np; i++) {
            if(gv(sockets)[i] < 0 || i == me) continue;
            Send(gv(sockets)[i],&ack,1,MSG_OOB);
            STD_DEBUG((stdout,"%s: Sending SIGURG to %i\n",DDI_Id(),i))
         }
      }
      
      if(me < np && gv(sockets)[me+np] > 0) {
         Send(gv(sockets)[me+np],&ack,1,MSG_OOB);
         STD_DEBUG((stdout,"%s: Sending SIGURG to my data server %i.\n",DDI_Id(),me+np))
      }

      sleep(1);
    # endif


   /* -------------------------------------- *\
      If used, delete the System V semaphore
   \* -------------------------------------- */
    # if defined USE_SYSV
      DDI_Sem_remove(gv(dda_access));
      DDI_Sem_remove(gv(fence_access));
      if(gv(shmid) != 0) DDI_Shm_remove(gv(shmid));
    # endif

    # if FULL_SMP   
      DDI_Sem_remove(gv(dlb_access));
    # endif

   /* ---------------------------------------------------------- *\
      Clean up semaphores associated with shared-memory segments
   \* ---------------------------------------------------------- */
      while(smp_data = (SMP_Data *) SMP_find_end()) {
         fprintf(stdout,"%s: Cleaning up leftover SMP Array %i.\n",DDI_Id(),smp_data->handle);
         SMP_destroy(smp_data->handle);
      }

   /* ------------------------------------------------------------------------------- *\
      If only using MPI, the cleanup is likely to be incomplete.  At the very least,
      the node on which the primary error occurred will be properly cleaned, however,
      some MPI implemenations send a hard kill (SIGKILL) to remote processes on
      MPI_Abort, which can not be trapped and those nodes will likely have semaphores
      remaining on them.  Note to administrators, if you have the ability to control
      your MPI implemenation(s), it is better to send a SIGTERM to the processes to
      allow proper cleanup, then a few seconds later, if they are not dead, send the
      very fatal SIGKILL (-9).
   \* ------------------------------------------------------------------------------- */
    # if defined DDI_MPI || !defined DDI_SOC
   // MPI_Abort(MPI_COMM_WORLD,signo);
      if(getenv("DDI_DUMP_CORE")) abort();
      exit(signo);
    # endif

    # if defined DDI_DUMP_CORE
      abort();
    # else
      exit(signo);
    # endif
   }

