#include "mysystem.h"
#include "common.h"

int Parse_node_args(int argc,char **argv,int nodearg,int nnodes,Node_info *ddinodes) {
  int i;
  int iarg;
  int inet;
  char str[256];
  char str2[256];
  char *sub = NULL;
  char *sav = NULL;

  for(i=0,iarg=nodearg; i<nnodes; i++,iarg++) {
    ddinodes[i].cpus=1;
    ddinodes[i].nics=0;
    strncpy(str,argv[iarg],256);

/*  fprintf(stdout,"\nNode #%i Info: %s\n",i,str);  */

    if( (sub = (char*) strtok_r(str,":",&sav)) != NULL) {
       ddinodes[i].hostname = (char *) strdup(str);
/*     fprintf(stdout,"Node #%i hostname = %s\n",i,sub); fflush(stdout); */
    }
    
    while( (sub = (char*) strtok_r(NULL,":=",&sav)) != NULL) {
      
      if(strcmp(sub,"cpus") == 0) {
         if( (sub = (char*) strtok_r(NULL,":=",&sav)) == NULL) {
           fprintf(stderr," Parse error: Unable to read the number of cpus for node argument %i.\n",i);
           fflush(stderr);
           return -1;
         }
         ddinodes[i].cpus=atoi(sub);
         if( ddinodes[i].cpus <= 0 ) {
           fprintf(stderr," Parse Error: Invalid value for cpus=%i in node argument %i.\n",ddinodes[i].cpus,i);
           fflush(stderr);
           return -1;
         }

/*       fprintf(stdout,"Node #%i ncpus = %i\n",i,ddinodes[i].cpus); fflush(stdout); */
      } else if(strcmp(sub,"netext") == 0) {
         if( (sub = (char*) strtok_r(NULL,":",&sav)) != NULL) {
            inet = 0;
            strcpy(str2,sub);
            if( (sub = (char*) strtok(str2,",")) != NULL) {
               ddinodes[i].netext[inet] = (char *) strdup(sub);
               inet++;
            }

            while( (sub = (char*)strtok(NULL,",")) != NULL) {
               ddinodes[i].netext[inet] = (char *) strdup(sub);
               inet++;
            }
         }

         ddinodes[i].nics = inet;

      } else {
         fprintf(stdout," ERROR: unrecognized option found after hostname, :%s,\n",sub);
         fprintf(stdout," but the only valid options are :cpus and :netext.\n");
         fflush(stdout);
         exit(911);
      }


   /* ------------------------------------------ *\
      TODO: Add in scratch disk/directory option
   \* ------------------------------------------ */

    }

  }

  return 0;
}
