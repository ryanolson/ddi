/* ----------------------------------------------------------------- *\
   GENERALIZED DISTRIBUTED DATA INTERFACE - 'MPMD' DATA SERVER MODEL
   =================================================================
   
   Error handling functions
   ========================
   void Filicide();        // kills all connected DDI children tasks
   const char *Info();     // prints who i am -- used in common fns.
   void Fatal_error(int);  // called on a fatal error
 
   Author: Ryan M. Olson
   CVS $Id: ddikick_error.c,v 1.5 2004/02/27 21:10:14 ryan Exp $
\* ----------------------------------------------------------------- */
 # include "ddikick.h"


   void Fatal_error(int signo) {

   /* --------------------------------------- *\
      Remote ddikick.x process just terminate
   \* --------------------------------------- */
      if(!gv(master)) exit(911);

   /* ------------------------ *\
      Turn off signal handling
   \* ------------------------ */
      signal(SIGTERM,SIG_IGN);
      signal(SIGPIPE,SIG_IGN);

   /* ------------- *\
      Flush buffers
   \* ------------- */
      fflush(stdout);
      fflush(stderr);

   /* -------------------------- *\
      Print out error message(s)
   \* -------------------------- */
      switch(signo) {
         case SIGTERM:
            fprintf(stderr," ddikick.x: Termination signal (SIGTERM) received.\n");
            break;
         case SIGPIPE:
            fprintf(stderr," ddikick.x: Unable to contact at least one DDI process (SIGPIPE).\n");
            break;
         default:
            fprintf(stderr," ddikick.x: Fatal error detected.\n");
            fprintf(stderr," The error is most likely to be in the application, so check for\n");
            fprintf(stderr," input errors, disk space, memory needs, application bugs, etc.\n");
            fprintf(stderr," ddikick.x will now clean up all processes, and exit...\n");
            break;
      }

   /* ------------------------ *\
      Kill DDI child processes
   \* ------------------------ */
      Filicide();

   /* -------------------- *\
      Flush Buffers & Exit
   \* -------------------- */
      sleep(1);
      fflush(stdout);
      fflush(stderr);
      fprintf(stdout," ddikick.x: Execution terminated due to error(s).\n");
      fflush(stdout);
      fflush(stderr);
      exit(911);
   }



/* ------------------------------------- *\
   Subroutine used to kill DDI processes
\* ------------------------------------- */
   void Filicide() {
      char c=0;
      int i,nsocks = gv(nprocs);
      int *sockets = gv(sockets);

      if(USING_DATA_SERVERS()) nsocks *= 2; 

      if(sockets == NULL) {
         fprintf(stdout," ddikick.x: No DDI processes to kill.\n");
      } else {
         fprintf(stdout," ddikick.x: Sending kill signal to DDI processes.\n");
         for(i=0; i<nsocks; i++) {
            if(sockets[i] < 0) continue;
          # if DDI_DEBUG
            DEBUG_START(DEBUG_MAX)
            fprintf(stdout," ddikick.x: Sending kill signal to DDI process %i.\n",i);
            DEBUG_END()
          # endif
            send(sockets[i],&c,1,MSG_OOB);
            close(sockets[i]);
            sockets[i] = -1;
         }
      }
   }


/* -------------------------------------------- *\
   Return a description of the current program.
   Used by DDI tasks in the common subroutines.
\* -------------------------------------------- */
   const char *Info() {
      static char str[256];
      sprintf(str,"%s"," ddikick.x");
      return str;
   }


   const char *DDI_Id() {
      return Info();
   }
