/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Subroutines associated with synchronizing the compute processes.
 *
 * Author: Ryan M. Olson
 * CVS $Id: ddi_timer.c,v 1.1.1.1 2005/11/17 00:12:34 ryan Exp $
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"


/* -------------------------------------------------------------------- *\
   DDI_Timer_reset()
   =================
   Reset the cpu timer within the current operating scope.
\* -------------------------------------------------------------------- */
   void DDI_Timer_reset() {
      getrusage(RUSAGE_SELF,&gv(cpu_timer));
      gettimeofday(&gv(wall_timer),NULL);
   }


/* -------------------------------------------------------------------- *\
   DDI_Timer_output()
   ==================
   Synchronous barrier on compute processes, but also collects total
   cpu time from each compute process and prints the totals stdout.
\* -------------------------------------------------------------------- */
   void DDI_Timer_output() {

      int i,me,np;
      struct rusage mycputime;
      struct rusage *timings = NULL;
      struct timeval cpu_total;
      struct timeval wall_total;

      DDI_NProc(&np,&me);
      DDI_Sync(3081);

      if(me == 0) {
         timings = (struct rusage *) Malloc(np*sizeof(struct rusage));
         getrusage(RUSAGE_SELF,timings);
         gettimeofday(&wall_total,NULL);
      } else {
         getrusage(RUSAGE_SELF,&mycputime);
         timings = &mycputime;
      }

      timings->ru_utime.tv_sec  -= gv(cpu_timer).ru_utime.tv_sec;
      timings->ru_utime.tv_usec -= gv(cpu_timer).ru_utime.tv_usec;
      if(timings->ru_utime.tv_usec < 0) {
         timings->ru_utime.tv_sec--;
         timings->ru_utime.tv_usec += 1000000;
      }

      timings->ru_stime.tv_sec  -= gv(cpu_timer).ru_stime.tv_sec;
      timings->ru_stime.tv_usec -= gv(cpu_timer).ru_stime.tv_usec;
      if(timings->ru_stime.tv_usec < 0) {
         timings->ru_stime.tv_sec--;
         timings->ru_stime.tv_usec += 1000000;
      } 

      wall_total.tv_sec  -= gv(wall_timer).tv_sec;
      wall_total.tv_usec -= gv(wall_timer).tv_usec;
      if(wall_total.tv_usec < 0) {
         wall_total.tv_sec--;
         wall_total.tv_usec += 1000000;
      }

      if(me == 0) {
         for(i=1; i<np; i++) DDI_Recv(&timings[i],sizeof(struct rusage),i);

         fprintf(stdout,"\n ------------------------------------------------");
         fprintf(stdout,"\n CPU timing information for all compute processes");
         fprintf(stdout,"\n ================================================");

         for(i=0; i<np; i++) {


            cpu_total.tv_sec  = timings[i].ru_utime.tv_sec  + timings[i].ru_stime.tv_sec;
            cpu_total.tv_usec = timings[i].ru_utime.tv_usec + timings[i].ru_stime.tv_usec;
            if(cpu_total.tv_usec > 1000000) {
               cpu_total.tv_sec++;
               cpu_total.tv_usec -= 1000000;
            }

            fprintf(stdout,"\n %4i: %d.%.6d + %d.%.6d = %d.%.6d",i,
               (int)timings[i].ru_utime.tv_sec,(int)timings[i].ru_utime.tv_usec,
               (int)timings[i].ru_stime.tv_sec,(int)timings[i].ru_stime.tv_usec,
               (int)cpu_total.tv_sec,(int)cpu_total.tv_usec);
         }

         fprintf(stdout,"\n Wall: %d.%.6d", 
                (int) wall_total.tv_sec, (int) wall_total.tv_usec);
         fprintf(stdout,"\n ================================================\n\n");
                  
         fflush(stdout);
         free(timings);

      } else {

         DDI_Send(&mycputime,sizeof(struct rusage),0);

      }

      DDI_Sync(3082);
   }

