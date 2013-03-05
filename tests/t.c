#include "mpi.h"
#include "ddi.h"

int main(int argc,char *argv[]) {

   int i,np,me,ncols;
   int nn,my;
   int handle;
   size_t counter;
   double a[10];
   DDI_Patch patch;

   DDI_Init(argc,argv);
// DebugOutput(0);
   DDI_Memory(50);
   DDI_NProc(&np,&me);
   DDI_NNode(&nn,&my);

   DDI_Create(10,np,&handle);
   DDI_DistribP(handle,me,&patch);

   ncols = patch.jhi-patch.jlo+1;

// if(me == 0) Comm_patch_print(&patch);

   for(i=0; i<10; i++) a[i] = 93*1.0;

   for(i=patch.jlo; i<=patch.jhi; i++) {
      DDI_Put(handle,patch.ilo,patch.ihi,i,i,&a);
   }

   for(i=0; i<10; i++) a[i] = -1.0;

   counter = -1;
   DDI_DLBReset();
   do {
      DDI_DLBNext(&counter);
      if(counter % 200 == 0) {
         fprintf(stdout,"%s: counter=%i\n",DDI_Id(),counter);
         fflush(stdout);
      }
      if(my == 0) usleep(20);
   } while (counter < 30000);

// MPI_Barrier(MPI_COMM_WORLD);
// if(me) sleep(2);
// else  DS_Thread_init();
// MPI_Barrier(MPI_COMM_WORLD);

// fprintf(stdout,"%s: first get\n",DDI_Id());
// fflush(stdout);

   DDI_Sync(10);

   if(me == 0) printf("done with dlb test\n");
   fflush(stdout);

   DDI_Sync(11);

   DDI_Get(handle,patch.ilo,patch.ihi,0,0,a);

   for(i=0; i<10; i++) {
      if(a[i] != 93.0) fprintf(stdout," %i: a[%i]=%lf != 93\n",me,i,a[i]);
      a[i] = 1.0;
      fflush(stdout);
   }

   if(me == 0) printf("%s: done with get\n",DDI_Id());
   fflush(stdout);

   DDI_Sync(10);

   if(me == 0) fprintf(stdout,"%s: starting acc\n",DDI_Id());
   fflush(stdout);

   DDI_Sync(12);

   DDI_Acc(handle,patch.ilo,patch.ihi,0,0,a);

   if(me == 0) fprintf(stdout,"%s: finished acc; syncing\n",DDI_Id());
   fflush(stdout);

   DDI_Sync(14);

   if(me==0) fprintf(stdout,"%s: finished acc; new get\n",DDI_Id());
   fflush(stdout);

   DDI_Sync(20);

   DDI_Get(handle,patch.ilo,patch.ihi,0,0,a);

   for(i=0; i<10; i++) {
      if(a[i] != 93.0+np) fprintf(stdout," %i: a[%i]=%lf !=%d \n",me,i,a[i],np+93);
      a[i] = 1.0;
      fflush(stdout);
   }

   DDI_Sync(15);
   if(me==0) fprintf(stdout,"%i: tested get\n",me);
   fflush(stdout);
   DDI_Sync(20);
// DDI_Put(handle,patch.ilo,patch.ihi,0,np-1,a);

   if(me==0) fprintf(stdout,"%s: finished global put\n",DDI_Id());
   fflush(stdout);

   DDI_Destroy(handle);

// fprintf(stdout,"%s: after destroy\n",DDI_Id());
// fflush(stdout);


   DDI_Finalize();
   return 0;
}
