 # include "ddi_base.h"

   void Comm_divide(int ngroups, int comm_id, int *new_comm_id) {

      int i,in,nt,npg,nr;
      const DDI_Comm *cur_comm = (const DDI_Comm *) Comm_find(comm_id);

      int *list = (int *) Malloc(ngroups*sizeof(int));

      if(ngroups <=0 || ngroups > cur_comm->nn) {
         fprintf(stdout,"%s: ngroups=%i (arg #1 of DDI_Comm_divide) is invalid.\n",DDI_Id,ngroups);
         Fatal_error(911);
      }

      nt  = cur_comm->nn;
      npg = nt / ngroups;
      nr  = nt % ngroups;

      for(i=0,in=0; i<ngroups; i++) {
         list[i] = npg;
         if(i < nr) list[i]++;
      }

      Comm_divide_custom(ngroups,list,comm_id,new_comm_id);

      free(list);
   }

   void Comm_divide_custom(int ngroups, int *list, int comm_id, int *new_comm_id) {

      int nt;
      int i,in,ip,mygroup,sp,ep,np;
      const DDI_Comm *cur_comm = (const DDI_Comm *) Comm_find(comm_id);

      int *my_ids      = NULL;
      int *sn_by_group = (int *) Malloc((ngroups+1)*sizeof(int));

      if(ngroups <=0 || ngroups > cur_comm->nn) {
         fprintf(stdout,"%s: ngroups=%i (arg #1 of DDI_Comm_divide) is invalid.\n",DDI_Id,ngroups);
         Fatal_error(911);
      }

      for(i=0,nt=0; i<ngroups; i++) nt += list[i];

      if(nt != cur_comm->nn) {
         fprintf(stdout," DDI: invalid list of groups sizes in divide_custom.\n");
         Fatal_error(911);
      } 

      for(i=0,in=0; i<ngroups; i++) {
         sn_by_group[i] = in;
         in += list[i];
      }
      sn_by_group[ngroups] = in;

      mygroup = 0;
      while(sn_by_group[mygroup+1] <= cur_comm->my && mygroup < ngroups) mygroup++;

      if(mygroup == ngroups) {
         fprintf(stdout,"%s: unable to find my spot in the new groups.\n",DDI_Id());
         Fatal_error(911);
      }

      DEBUG_OUT(LVL4,(stdout,"%s: mygroup=%i\n",DDI_Id(),mygroup))
      
      sp = cur_comm->node_master[sn_by_group[mygroup]];

      if(mygroup+1 == ngroups) ep = cur_comm->np;
      else                     ep = cur_comm->node_master[sn_by_group[mygroup+1]];
      np = ep - sp;

      my_ids = (int *) Malloc(np*sizeof(int));

      for(ip=0,i=0; ip<cur_comm->np; ip++) {
         if(cur_comm->global_pid[ip] >= sp && cur_comm->global_pid[ip] < ep) my_ids[i++]=ip;
      }

      if(i != np) {
         fprintf(stdout,"%s: could not find %i processes expected for the new comm.\n",DDI_Id(),np);
         Fatal_error(911);
      }

      Comm_create(np,my_ids,ngroups,mygroup,comm_id,new_comm_id);

      free(my_ids);
      free(sn_by_group);
   }

   void Comm_create(int np,int *ids, int ngroups, int mygroup, int comm_id, int *new_comm_id) {

      int i,ip,in,ismp,nn,nid,np_local,tmp;
      size_t size;

      const DDI_Comm *cur_comm   = (DDI_Comm *) Comm_find(comm_id);
      const DDI_Comm *comm_world = &gv(ddi_base_comm);
      DDI_Comm *new_comm = (DDI_Comm *) Malloc(sizeof(DDI_Comm));
      DDI_Comm *end_comm = (DDI_Comm *) Comm_find_end();

      DEBUG_ROOT(LVL1,(stdout," DDI: Entering DDI_Create_comm.\n")) 
      DEBUG_OUT(LVL2,(stdout,"%s: Entering DDI_Create_comm.\n",DDI_Id()))

      Comm_sync(123,cur_comm);

   /* ------------------------------- *\
      Add new_comm to the linked list
   \* ------------------------------- */
      new_comm->next = NULL;
      end_comm->next = (void *) new_comm;

   /* new_data->next = NULL; */
   /* end_data->next = (void *) new_data; */

      new_comm->ngroups = ngroups;
      new_comm->mygroup = mygroup;

      new_comm->id = *new_comm_id = gv(ddi_comm_id)++;
      new_comm->local_nid  = (int *) Malloc(np*sizeof(int));
      new_comm->global_pid = (int *) Malloc(np*sizeof(int));
      new_comm->global_nid = (int *) Malloc(np*sizeof(int));

      i = 0;
      if(np > 1) {
         do {
   
            if(ids[i] > cur_comm->np) {
               fprintf(stdout,"%s: Invalid id list in DDI_Comm_create.\n",DDI_Id());
               Fatal_error(911);
            }
   
            if(ids[i+1] < ids[i]) {
               tmp      = ids[i];
               ids[i]   = ids[i+1];
               ids[i+1] = tmp;
               if(i) i--; i--;
            }
   
         } while(++i < np-1);
      }

      Comm_sync(126,cur_comm);
      
      nn  = -1;
      nid = -1;
      np_local = 0;

      for(i=0; i<np; i++) {

         new_comm->global_pid[i] = cur_comm->global_pid[ids[i]];
         new_comm->global_nid[i] = cur_comm->global_nid[ids[i]];
         fflush(stdout);

         if(new_comm->global_nid[i] != nid) {
            nid = new_comm->global_nid[i];
            nn++;
         }

         new_comm->local_nid[i] = nn;
         
         if(nid == comm_world->my) np_local++;
         
      }

      nn++;
/*
      fprintf(stdout,"%s: new_comm->nn = %i.\n",DDI_Id(),nn);
      fprintf(stdout,"%s: new_comm->np_local = %i.\n",DDI_Id(),np_local);
*/
      Comm_sync(127,cur_comm);
      DEBUG_ROOT(LVL5,(stdout," comm_create - global_pid/global_nid formed.\n"))

      new_comm->smp_pid     = (int *) Malloc(np_local*sizeof(int));
      new_comm->node_master = (int *) Malloc(nn*sizeof(int));
      new_comm->global_dsid = (int *) Malloc(nn*sizeof(int));

      for(ip=0,in=-1,ismp=0,nid=-1; ip<np; ip++) {

         if(new_comm->global_nid[ip] != nid) {
            in++;
            nid = new_comm->global_nid[ip];
            new_comm->global_dsid[in] = comm_world->global_dsid[nid];
            new_comm->node_master[in] = new_comm->global_pid[ip];
         }

         if(new_comm->global_pid[ip] == comm_world->me) {
            new_comm->me = ip;
            new_comm->my = in;
         }
         
         if(nid == comm_world->my) {
            if(new_comm->global_pid[ip] == comm_world->me) new_comm->me_local = ismp;
            new_comm->smp_pid[ismp++] = new_comm->global_pid[ip];
         }
         
      }

      new_comm->nn = nn;
      new_comm->np = np;
      new_comm->np_local = ismp;

      DEBUG_OUT(LVL5,(stdout,"%s: np=%i, nn=%i, np_smp=%i, me=%i, my=%i, me_smp=%i.\n",
              DDI_Id(),new_comm->np,new_comm->nn,new_comm->np_local,
              new_comm->me,new_comm->my,new_comm->me_local))


    # if defined DDI_MPI
      new_comm->world_comm = cur_comm->world_comm;
      MPI_Comm_split(cur_comm->smp_comm,new_comm->global_pid[0],new_comm->me_local,&new_comm->smp_comm);
      MPI_Comm_split(cur_comm->compute_comm,new_comm->global_pid[0],new_comm->me,&new_comm->compute_comm);
      MPI_Comm_split(new_comm->compute_comm,new_comm->me_local,new_comm->my,&new_comm->node_comm);
    # endif
      
      DEBUG_OUT(LVL3,(stdout,"%s: Exiting DDI_Comm_create.\n",DDI_Id()))

   }


   void DDI_Comm_destroy(int commid) {
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(commid);

      DDI_Comm *curr_comm = &gv(ddi_base_comm);
      DDI_Comm *prev_comm = NULL;
/*
      DDI_Data *curr_data = &gv(ddi_base_data);
      DDI_Data *prev_data = NULL;
*/
      Comm_sync(124,comm);

      if(commid == DDI_COMM_WORLD) {
         fprintf(stdout,"%s: Cannot destroy DDI_COMM_WORLD.\n",DDI_Id());
         Fatal_error(911);
      }

      while(curr_comm->next && curr_comm->id != commid) {
         prev_comm = curr_comm;
         curr_comm = (DDI_Comm *) curr_comm->next;
      }

      if(curr_comm->id != commid) {
         fprintf(stdout,"%s: Error in DDI_Comm_destroy - Comm not found.\n",DDI_Id());
         Fatal_error(911);
      }
/*
      while(curr_data->next && curr_data->id != curr_comm->data_id) {
         prev_data = curr_data;
         curr_data = (DDI_Data *) curr_data->next;
      }

      if(curr_data->id != curr_comm->data_id) {
         fprintf(stdout,"%s: Error in DDI_Comm_destroy - Data not found.\n",DDI_Id());
         Fatal_error(911);
      }

    * ----------------------------------------------------------------------- *\
      Delete item from DDI_Data linked-list.
   \* ----------------------------------------------------------------------- *
      if(curr_comm->me_local == 0) shmdt((void *) curr_data->sync_array);
      prev_data->next = curr_data->next;
      free(curr_data);
*/
  
   /* ----------------------------------------------------------------------- *\
      Delete item from DDI_Comm linked-list.
   \* ----------------------------------------------------------------------- */
      free(curr_comm->smp_pid);
      free(curr_comm->local_nid);
      free(curr_comm->global_pid);
      free(curr_comm->global_nid);
      free(curr_comm->global_dsid);
      free(curr_comm->node_master);
      prev_comm->next = curr_comm->next;
      free(curr_comm);

   }

   void Comm_print(const DDI_Comm *comm) {

      int i;

      fprintf(stdout,"\n---------------------");
      fprintf(stdout,"\nDDI Communicator Info");
      fprintf(stdout,"\n---------------------\n");

      fprintf(stdout," id = %i\n",comm->id);
      fprintf(stdout," np = %i\n",comm->np);
      fprintf(stdout," me = %i\n",comm->me);
      fprintf(stdout," nn = %i\n",comm->nn);
      fprintf(stdout," my = %i\n",comm->my);
      fprintf(stdout," np_local = %i\n",comm->np_local);
      fprintf(stdout," me_local = %i\n",comm->me_local);
      fprintf(stdout," ngroups  = %i\n",comm->ngroups);
      fprintf(stdout," mygroup  = %i\n",comm->mygroup);

      fprintf(stdout," global_pid = {");
      for(i=0; i<comm->np; i++) fprintf(stdout,"%i,",comm->global_pid[i]);
      fprintf(stdout,"}\n");

      fprintf(stdout," global_nid = {");
      for(i=0; i<comm->np; i++) fprintf(stdout,"%i,",comm->global_nid[i]);
      fprintf(stdout,"}\n");

      fprintf(stdout," local_nid = {");
      for(i=0; i<comm->np; i++) fprintf(stdout,"%i,",comm->local_nid[i]);
      fprintf(stdout,"}\n");

      fprintf(stdout," global_dsid = {");
      for(i=0; i<comm->nn; i++) fprintf(stdout,"%i,",comm->global_dsid[i]);
      fprintf(stdout,"}\n");

      fprintf(stdout," node_master = {");
      for(i=0; i<comm->nn; i++) fprintf(stdout,"%i,",comm->node_master[i]);
      fprintf(stdout,"}\n");

    # if defined USE_SYSV
      fprintf(stdout," smp_pid = {");
      for(i=0; i<comm->np_local; i++) fprintf(stdout,"%i,",comm->smp_pid[i]);
      fprintf(stdout,"}\n\n");
    # endif

    # if defined DDI_MPI
      fprintf(stdout," world_comm   = %i\n",comm->world_comm);
      fprintf(stdout," compute_comm = %i\n",comm->compute_comm);

      fprintf(stdout," smp_comm = %i\n",comm->smp_comm);
      fprintf(stdout," node_comm = %i\n",comm->node_comm);
    # endif

   }
