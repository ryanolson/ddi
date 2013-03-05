/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Subroutines associated with forming communication subgroups.
 *
 * Author: Ryan M. Olson
 * changed 1/2005 by Dmitri Fedorov and Ikegami-san
\* -------------------------------------------------------------------- */ 
 # include "ddi_base.h"


/* ----------------------------------------------------------- *\
   DDI_Group_create
   ================
   Subdivides the global set of nodes/processes into 'ngroups'
   subgroups that are evenly divided.
   Type == 0 ==> divide the set of nodes (default)
   Type == 1 ==> divide the set of processes.
\* ----------------------------------------------------------- */
   void DDI_Group_create(int *handle,int type,int ngroups) {
      int i,me,np,my,nn;
      int nt,nr,npg;
      int group_size[MAX_PROCESSORS];

      DDI_NProc(&np,&me);
      DDI_NNode(&nn,&my);

      if(ngroups <= 0) {
         fprintf(stdout," Error calling DDI_Group_create: NGroups=%i must be > 0.\n",ngroups);
         Fatal_error(911);
      }

      nt = nn;
      if(type == 1) nt = np;

      npg = nt / ngroups;
      nr  = nt % ngroups;

      for(i=0; i<ngroups; i++) {
         group_size[i] = npg;
         if(i < nr) group_size[i]++;
      }

      DDI_Group_create_custom(handle,type,ngroups,group_size);
   }


/* ----------------------------------------------------------- *\
   DDI_Group_create_custom
   =======================
   Subdivides the global set of nodes/processes into 'ngroups'
   subgroups that are evenly divided.
   Type == 0 ==> divide the set of nodes (default)
   Type == 1 ==> divide the set of processes.
\* ----------------------------------------------------------- */
   void DDI_Group_create_custom(int *handle,int type,int ngroups,int *group_size) {
      int i,me,np,my,nn,smpnp,smpme;
      int in,ip,nt,nt_test;
      int grpnp,grpme,grpnn;
      int masterp,mastern,totalps,totalns,mygroup;

      DDI_Sync(3051);
      DDI_NProc(&np,&me);
      DDI_NNode(&nn,&my);
      DDI_SMP_NProc(&smpnp,&smpme);

      nt = nn;
      if(type == 1) { nt = np; }

      if(gv(scope) != DDI_WORLD && me == 0) {
         fprintf(stdout," DDI Error: Subgroup creation must take place in the WORLD scope.\n");
         Fatal_error(911);
      }
         
      if(type != 0 && me == 0) {
         fprintf(stdout," DDI Error: This option is not yet implemented.\n");
         Fatal_error(911);
      }

      if(ngroups < 1 || ngroups > nt) {
         fprintf(stdout," Error in DDI_Group_create_custom: arg[3] is out of range.\n");
         fprintf(stdout," ngroups=%i, but should be 1 <= ngroups <= %i.\n",ngroups,nt);
         Fatal_error(911);
      }

      for(i=0,nt_test=0; i<ngroups; i++) nt_test += group_size[i];

      if(nt_test != nt && me == 0) {
         fprintf(stdout," Error in DDI_Group_create_custom.\n");
         fprintf(stdout," The sum of the values in arg[4] != total # of nodes/procs.\n");
         Fatal_error(911);
      }

      if(gv(master_map)) free(gv(master_map));
      gv(master_map) = Malloc(ngroups*sizeof(int));
      for(i=0; i<ngroups; i++) gv(master_map)[i] = 0;

      mygroup = 0;
      masterp = 0;
      mastern = 0;
      totalps = 0;
      totalns = group_size[0];
      for(in=0; in<totalns; in++) totalps += gv(ddinodes)[in].cpus;
      
      while(totalns <= my) {
         masterp = totalps;
         mastern = totalns;
         for(in=totalns,totalns+=group_size[++mygroup]; in<totalns; in++) {
            totalps += gv(ddinodes)[in].cpus;
         }
      }

      gv(nprocs)[DDI_GROUP] = grpnp = totalps - masterp;
      gv(myproc)[DDI_GROUP] = grpme = me - masterp;
      gv(nnodes)[DDI_GROUP] = grpnn = group_size[mygroup];
      gv(mynode)[DDI_GROUP] =         my - mastern;
      gv(nprocs)[DDI_MASTERS] = ngroups;
      gv(myproc)[DDI_MASTERS] = mygroup;
      gv(nnodes)[DDI_MASTERS] = ngroups;
      gv(mynode)[DDI_MASTERS] = mygroup;
      gv(ngroups) = ngroups;
      gv(mygroup) = mygroup;

      if(grpme == 0) gv(master_map)[mygroup] = me;
      DDI_GSum(gv(master_map),ngroups*sizeof(int),sizeof(int));

   /* ------------------------------------------------------------------ *\
      Currently, we only form subgroups of smp nodes, but in the future
      when we have the ability to split smp nodes by process, then we'll
      need to properly set gv(smp_np) and gv(smp_me).
   \* ------------------------------------------------------------------ */
#if defined USE_SYSV || defined DDI_ARMCI_SMP
      gv(smp_np)[DDI_GROUP] = smpnp;
      gv(smp_me)[DDI_GROUP] = smpme;
#endif

      if(gv(global_node_map)) free(gv(global_node_map));
      gv(global_node_map) = Malloc(grpnn*sizeof(int));
      if(gv(global_proc_map)) free(gv(global_proc_map));
      if(USING_DATA_SERVERS()) gv(global_proc_map) = Malloc(2*grpnp*sizeof(int));
      else                     gv(global_proc_map) = Malloc(grpnp*sizeof(int));

      for(in=0; in<grpnn; in++) gv(global_node_map)[in] = mastern+in;
      for(ip=0; ip<grpnp; ip++) {
         gv(global_proc_map)[ip] = masterp+ip;
         if(USING_DATA_SERVERS()) gv(global_proc_map)[ip+grpnp] = masterp+ip+np;
      }

/*
      int grpmy;
      grpmy = gv(mynode)[DDI_GROUP];
      fprintf(stdout,"%s:grpnp=%i\n%s:grpme=%i\n",DDI_Id(),grpnp,DDI_Id(),grpme);
      fprintf(stdout,"%s:grpnn=%i\n%s:grpmy=%i\n",DDI_Id(),grpnn,DDI_Id(),grpmy);
      fprintf(stdout,"%s:mygroup=%i\n%s:ngroups=%i\n",DDI_Id(),mygroup,DDI_Id(),ngroups);
      fflush(stdout);
*/

    # if defined DDI_MPI
      masterp = 0;
      if(grpme == 0) masterp = 1;
      MPI_Comm_split(gv(DDI_Compute_comm),mygroup,0,&gv(GRP_Compute_comm));
      MPI_Comm_split(gv(DDI_Compute_comm),masterp,0,&gv(GRP_Masters_comm));
    # endif
   }


/* ---------------------------------------------------------- *\
   DDI_NGroup(NGROUPS,MYGROUP)
   ===========================
   [OUT] NGROUPS - Number of groups in current scope
   [OUT] MYGROUP - Rank of current group [0,NGROUPS)

   Returns the rank and number of groups in the current scope
\* ---------------------------------------------------------- */
   void DDI_NGroup(int *ngroups,int *mygroup) {
      if(gv(scope) == DDI_WORLD) {
         *ngroups = 1;
         *mygroup = 0;
      } else {
         *ngroups = gv(ngroups);
         *mygroup = gv(mygroup);
      }
   }


/* ---------------------------------------------------------- *\
   DDI_Scope
   =========
   Changes the working communication scope.  
\* ---------------------------------------------------------- */
   int DDI_Scope(int scope) {

      int np,me;

    # if defined DDI_CHECK_ARGS
      if(!(scope == DDI_WORLD || scope == DDI_GROUP || scope == DDI_MASTERS)) {
         fprintf(stdout,"%s: invalid argument in DDI_Scope.\n",DDI_Id());
         Fatal_error(911);
      }
    # endif

      if(gv(scope) == DDI_MASTERS && scope != DDI_MASTERS) {
         gv(scope) = DDI_GROUP;
         DDI_Sync(3052);
      }

      if(scope == DDI_MASTERS) {
         DDI_Scope(DDI_GROUP);
         DDI_NProc(&np,&me);
         if(me == 0) {
            gv(scope) = DDI_MASTERS;
            return 1;
         } else {
            DDI_Sync(3053);
         }
      } else {
         DDI_Sync(3054);
         gv(scope) = scope;
       # if defined DDI_MPI
         if(scope == DDI_WORLD) gv(Compute_comm) = gv(DDI_Compute_comm);
         if(scope == DDI_GROUP) gv(Compute_comm) = gv(GRP_Compute_comm);
       # endif
         DDI_Sync(3055);
      }
      return 0;
   }

   int ddi_ascope_(intf77 *scope) { }
