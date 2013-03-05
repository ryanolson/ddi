#include "ddi_base.h"

void DDI_NGroup(int* ngroups,int *mygroup) {
   const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);

   *ngroups = comm->ngroups;
   *mygroup = comm->mygroup;
}

void DDI_Scope(int comm_id) {

   const DDI_Comm *cur_comm = (DDI_Comm *) Comm_find(DDI_WORKING_COMM);
   const DDI_Comm *new_comm = (DDI_Comm *) Comm_find(comm_id);

   DEBUG_OUT(LVL2,(stdout,"%s: entering DDI_Scope.\n",DDI_Id()))

   Comm_sync(7545,cur_comm);
   gv(ddi_working_comm) = comm_id;
   Comm_sync(7550,new_comm);

#if defined DDI_ARMCI
   DDI_ARMCI_DLB_addr();
#endif

   DEBUG_OUT(LVL2,(stdout,"%s: leaving DDI_Scope.\n",DDI_Id()))
}

void ddi_ascope_(int *scope) { }

void DDI_Group_create(int ngroups,int *world,int *group, int *master) {

   const DDI_Comm *world_comm,*group_comm,*master_comm;

   int i,np,mygroup;
   int *list,*ids;
   size_t offset;

   *world     = DDI_WORKING_COMM;
   world_comm = (const DDI_Comm *) Comm_find(*world);

   /* check the number of groups .. make sure they can be created. */
   if(ngroups <= 0 || ngroups > world_comm->nn) {
      fprintf(stdout,"%s: ngroups=%i (first arg of DDI_Group_create) is invalid.\n",DDI_Id(),ngroups);
      Fatal_error(911);
   }

   Comm_divide(ngroups,DDI_WORKING_COMM,group);
   group_comm = (const DDI_Comm *) Comm_find(*group);

   list = (int *) Malloc(world_comm->np*sizeof(int));
   for(i=0; i<world_comm->np; i++) list[i] = 0;
   list[world_comm->me] = group_comm->me;
   Comm_gsum(list,world_comm->np,sizeof(int),world_comm);

   Comm_sync(324,world_comm);
   if(world_comm->me == 0) {
      fprintf(stdout," list = { ");
      for(i=0; i<world_comm->np-1; i++) fprintf(stdout,"%i,",list[i]);
      fprintf(stdout,"%i }",list[world_comm->np-1]);
      fflush(stdout);
   }
   Comm_sync(325,world_comm);
  

   if(group_comm->me == 0) {
      mygroup = 0;
      ids = (int *) Malloc(ngroups*sizeof(int));
      for(i=0,np=0; i<world_comm->np; i++) if(list[i] == 0) ids[np++] = i;
      if(np != ngroups) {
         fprintf(stdout,"%s: unable to verify the groups masters.\n",DDI_Id());
         Fatal_error(911);
      }
   } else {
      mygroup = 0;
      ids = (int *) Malloc((world_comm->np - ngroups)*sizeof(int));
      for(i=0,np=0; i<world_comm->np; i++) if(list[i] != 0) ids[np++] = i;
      if(np != world_comm->np-ngroups) {
         fprintf(stdout,"%s: unable to verify non-group masters.\n",DDI_Id());
         Fatal_error(911);
      }
   }

   Comm_create(np,ids,2,mygroup,*world,master);
   master_comm = (const DDI_Comm *) Comm_find(*master);
   
   free(list);
   free(ids);

   Comm_sync(1234,world_comm);
/*
   if(world_comm->me == 0) {
      Comm_print(world_comm);
      Comm_print(group_comm);
      Comm_print(master_comm);
   }
   Comm_sync(1235,world_comm);
*/

}

void DDI_Group_create_custom(int ngroups,int *grp_list,int *world,int *group, int *master) {

   const DDI_Comm *world_comm,*group_comm,*master_comm;

   int i,np,mygroup;
   int *list,*ids;

   *world     = DDI_WORKING_COMM;
   world_comm = (const DDI_Comm *) Comm_find(*world);

   /* check the number of groups .. make sure they can be created. */
   if(ngroups <= 0 || ngroups > world_comm->nn) {
      fprintf(stdout,"%s: ngroups=%i (first arg of DDI_Group_create) is invalid.\n",DDI_Id(),ngroups);
      Fatal_error(911);
   }

   Comm_divide_custom(ngroups,grp_list,DDI_WORKING_COMM,group);
   group_comm = (const DDI_Comm *) Comm_find(*group);

   list = (int *) Malloc(world_comm->np*sizeof(int));
   for(i=0; i<world_comm->np; i++) list[i] = 0;
   list[world_comm->me] = group_comm->me;
   Comm_gsum(list,world_comm->np,sizeof(int),world_comm);

   Comm_sync(324,world_comm);
   if(world_comm->me == 0) {
      fprintf(stdout," list = { ");
      for(i=0; i<world_comm->np-1; i++) fprintf(stdout,"%i,",list[i]);
      fprintf(stdout,"%i }",list[world_comm->np-1]);
      fflush(stdout);
   }
   Comm_sync(325,world_comm);
  

   if(group_comm->me == 0) {
      mygroup = 0;
      ids = (int *) Malloc(ngroups*sizeof(int));
      for(i=0,np=0; i<world_comm->np; i++) if(list[i] == 0) ids[np++] = i;
      if(np != ngroups) {
         fprintf(stdout,"%s: unable to verify the groups masters.\n",DDI_Id());
         Fatal_error(911);
      }
   } else {
      mygroup = 0;
      ids = (int *) Malloc((world_comm->np - ngroups)*sizeof(int));
      for(i=0,np=0; i<world_comm->np; i++) if(list[i] != 0) ids[np++] = i;
      if(np != world_comm->np-ngroups) {
         fprintf(stdout,"%s: unable to verify non-group masters.\n",DDI_Id());
         Fatal_error(911);
      }
   }

   Comm_create(np,ids,2,mygroup,*world,master);
   master_comm = (const DDI_Comm *) Comm_find(*master);
   
   free(list);
   free(ids);

   Comm_sync(1234,world_comm);
/*
   if(world_comm->me == 0) {
      Comm_print(world_comm);
      Comm_print(group_comm);
      Comm_print(master_comm);
   }
   Comm_sync(1235,world_comm);
*/
}
