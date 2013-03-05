 # include "ddi_base.h"

/* ------------------------------------------ *\
   Static Global Variables for ddi_smp object
\* ------------------------------------------ */
   static int     gv(smp_data_id) = 1;
   SMP_Data*      gv(smp_base_data) = NULL;


   void DDI_SMP_Create(size_t size,int *handle) {
      *handle = SMP_create(size);
   }

   void DDI_SMP_Destroy(int handle) {
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_COMM_WORLD);
      Comm_sync_smp(comm);
      SMP_destroy(handle);
   }

   void *DDI_SMP_Array(int handle) {
      SMP_Data *data = (SMP_Data *) SMP_find(handle);
      return data->addr;
   }

   int SMP_create(size_t size) {

      int semflg = 0600;
      DDI_Comm *comm     = (DDI_Comm *) Comm_find(DDI_COMM_WORLD); /* hardcoded atm */
      SMP_Data *new_data = (SMP_Data *) Malloc(sizeof(SMP_Data));
      SMP_Data *end_data = (SMP_Data *) SMP_find_end();

      STD_DEBUG((stdout,"%s: Entered DDI_SMP_Create.\n",DDI_Id()))

      if(end_data) end_data->next = (void *) new_data;
      else gv(smp_base_data) = (SMP_Data *) new_data;

    # if defined USE_SYSV
      Comm_sync_smp(comm);
      if(comm->me_local == 0) { 
         new_data->handle = gv(smp_data_id)++; 
         new_data->shmid  = gv(shmid) = Shmget(IPC_PRIVATE,size,SHM_R|SHM_W);
         new_data->semid  = Semget(IPC_PRIVATE,1,semflg);
         new_data->size   = size;
         new_data->next   = NULL;
      }
      Comm_bcast_smp(new_data,sizeof(SMP_Data),0,comm);
      new_data->addr = Shmat(new_data->shmid,0,0);
      MAX_DEBUG((stdout,"%s: SMP memory [%i] shmid=%i, semid=%i, addr=%x.\n",
                 DDI_Id(),new_data->handle,new_data->shmid,new_data->semid,new_data->addr))
      Comm_sync_smp(comm);
      if(comm->me_local == 0) { Shmctl(new_data->shmid,IPC_RMID,NULL); gv(shmid) = 0; }
      Comm_sync_smp(comm);
    # else
      new_data->handle = gv(smp_data_id)++;
      new_data->size   = size;
      new_data->next   = NULL;
      new_data->addr   = Malloc(size);
    # endif
      
      return new_data->handle;
   }

   void SMP_destroy(int handle) {

      SMP_Data *node = (SMP_Data *) gv(smp_base_data);
      SMP_Data *prev = (SMP_Data *) gv(smp_base_data);

      DEBUG_OUT(LVL3,(stdout,"%s: smp_destroy - %i.\n",DDI_Id(),handle)) 
     
      if(node == NULL) {
         fprintf(stdout,"%s: Warning no SMP arrays to destroy.\n",DDI_Id());
         Fatal_error(911);
      }
     
      while(node) {
         if(node->handle == handle) {
          # if defined USE_SYSV
            MAX_DEBUG((stdout,"%s: detaching from shm addr %x, removing semid=%i.\n",
                    DDI_Id(),node->addr,node->semid))
            shmdt(node->addr);
            DDI_Sem_remove(node->semid);
          # else
            free(node->addr);
          # endif
            if(node == gv(smp_base_data)) gv(smp_base_data) = (SMP_Data *) node->next;
            else prev->next = node->next;
            free(node);
            return;
         }
         prev = (SMP_Data *) node;
         node = (SMP_Data *) node->next;
      }

      fprintf(stdout,"%s: Shared Memory handle %i was not found.\n",DDI_Id(),handle);
      Fatal_error(911);

   }
            

   void *SMP_find(int handle) {

      SMP_Data *data = (SMP_Data *) gv(smp_base_data);
      if(data == NULL) {
         fprintf(stdout,"%s: Warning.  No SMP Arrays allocated.\n",DDI_Id());
         Fatal_error(911);
      }

      do {
         if(data->handle == handle) return (void *) data;
         data = (SMP_Data *) data->next;
      } while(data);

      fprintf(stdout,"%s: Unable to find data struct associated with id %i.\n",DDI_Id(),handle);
      Fatal_error(911);

      return NULL;

   }


   void *SMP_find_end() {

      SMP_Data *data = (SMP_Data *) gv(smp_base_data);
      if(data == NULL) return NULL;

      while(data->next) {
         data = (SMP_Data *) data->next;
      }

      return (void *) data;

   }

