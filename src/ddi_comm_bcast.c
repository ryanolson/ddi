/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Broadcast subroutine with communicator option.  Collective operation.
 * 
 * Author: Ryan M. Olson
 * modified March 2007 by MWS to chunk the MPI_BCAST calls.
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"


/* -------------------------------------------------------------------- *\
   DDI_BCast(buffer,size,root)
   =====================================
   [IN/OUT] buffer - send buffer from process 'root'.
                   - receive buffer on all other processes.
   [IN]     size   - size of buffer (in bytes).
   [IN]     root   - rank of process that has the send buffer.
   [IN]     comm   - communicator

   If comm not used in the calling arguments, then the subroutine uses
   the "working" communicator.
\* -------------------------------------------------------------------- */
   void DDI_BCast(void *buffer,size_t size,int root) {
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);

      DEBUG_ROOT(LVL1,(stdout," DDI: starting DDI_BCast.\n"))
      DEBUG_OUT(LVL3,(stdout,"%s: starting DDI_BCast.\n",DDI_Id()))

      Comm_bcast(buffer,size,root,comm);

      DEBUG_OUT(LVL3,(stdout,"%s: leaving DDI_BCast.\n",DDI_Id()))
      DEBUG_ROOT(LVL2,(stdout," DDI: leaving DDI_BCast.\n"))

    # if defined DDI_COUNTERS
      gv(bcast_profile).ncalls++;
      gv(bcast_profile).nbytes += size;
    # endif

   }

   void DDI_BCast_smp(void *buffer,size_t size,int root) {
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);

      DEBUG_ROOT(LVL1,(stdout," DDI: starting DDI_BCast_smp.\n"))
      DEBUG_OUT(LVL3,(stdout,"%s: starting DDI_BCast_smp.\n",DDI_Id()))

      Comm_bcast_smp(buffer,size,root,comm);

      DEBUG_OUT(LVL3,(stdout,"%s: leaving DDI_BCast_smp.\n",DDI_Id()))
      DEBUG_ROOT(LVL2,(stdout," DDI: leaving DDI_BCast_smp.\n"))

   }

   void DDI_BCast_node(void *buffer,size_t size,int root) {
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);

      DEBUG_ROOT(LVL1,(stdout," DDI: starting DDI_BCast_node.\n"))
      DEBUG_OUT(LVL3,(stdout,"%s: starting DDI_BCast_node.\n",DDI_Id()))

      Comm_bcast_node(buffer,size,root,comm);

      DEBUG_OUT(LVL3,(stdout,"%s: leaving DDI_BCast_node.\n",DDI_Id()))
      DEBUG_ROOT(LVL2,(stdout," DDI: leaving DDI_BCast_node.\n"))

   }

/* -------------------------------------------------------------------- *\
   DDI_BCast_comm(buffer,size,root,comm)
   =====================================
   [IN/OUT] buffer - send buffer from process 'root'.
                   - receive buffer on all other processes.
   [IN]     size   - size of buffer (in bytes).
   [IN]     root   - rank of process that has the send buffer.
   [IN]     comm   - DDI communicator Id.
\* -------------------------------------------------------------------- */
   void DDI_BCast_comm(void *buffer,size_t size,int root,int commid) {
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(commid);

      DEBUG_ROOT(LVL1,(stdout," DDI: starting DDI_Comm_bcast.\n"))
      DEBUG_OUT(LVL3,(stdout,"%s: starting DDI_Comm_bcast.\n",DDI_Id()))

      Comm_bcast(buffer,size,root,comm);

      DEBUG_OUT(LVL3,(stdout,"%s: leaving DDI_Comm_bcast.\n",DDI_Id()))
      DEBUG_ROOT(LVL2,(stdout," DDI: leaving DDI_Comm_bcast.\n"))

    # if defined DDI_COUNTERS
      gv(bcast_profile).ncalls++;
      gv(bcast_profile).nbytes += size;
    # endif

   }


   void Comm_bcast(void *buffer,size_t size,int root,const DDI_Comm *comm) {

   /* --------------- *\
      Local Variables
   \* --------------- */
      DDI_List list;
      int root_local;
      char *buff = (char *) buffer;

    # if defined DDI_MPI && !defined USE_DDI_COLLECTIVE_ROUTINES
      size_t remaining, buffer_size, working_size;
    # endif

      DEBUG_OUT(LVL4,(stdout,"%s: starting Comm_bcast.\n",DDI_Id()))

   /* ----------------------------- *\
    * Sanity check on the arguments
   \* ----------------------------- */
    # if defined DDI_CHECK_ARGS
      if(root < 0 || root > comm->np) {
         fprintf(stdout,"%s: bcast error - invalid value for root (%i).\n",DDI_Id(),root);
         Fatal_error(911);
      }

      if(buffer == NULL) {
         fprintf(stdout,"%s: bcast error - null pointer for arg 1.\n",DDI_Id());
         Fatal_error(911);
      }
    # endif

   /* ---------------------------------- *\
      Single process returns immediately
   \* ---------------------------------- */
      if(comm->np == 1) return;

   /* ---------------------------------------------------------------- *\
    * Decide whether to use MPI or traditional DDI collective routines
   \* ---------------------------------------------------------------- */
    # if defined DDI_MPI && !defined USE_DDI_COLLECTIVE_ROUTINES
      DEBUG_OUT(LVL5,(stdout,"%s: performing MPI_Bcast.\n",DDI_Id()))

   /*    no buffer allocated, so can increase message length by 10x  */
      remaining = size;
      buffer_size = 10*DDI_BUFFER_SIZE;

      while (remaining) {
                                     working_size = buffer_size;
         if(remaining < buffer_size) working_size = remaining;
         
         MPI_Bcast(buff,working_size,MPI_BYTE,root,comm->compute_comm);

         remaining -= working_size;
         buff      += working_size;
      }
    # else
   /* --------------------------------------------------------------------- *\
      Broadcast within the root node to guarantee the node master had data.
   \* --------------------------------------------------------------------- */
      DEBUG_OUT(LVL5,(stdout,"%s: performing collective routine - bcast.\n",DDI_Id()))
      DEBUG_OUT(LVL6,(stdout,"%s: intra-node bcast.\n",DDI_Id()))
      if(comm->local_nid[root] == comm->my && comm->np_local > 1) {

         root_local = 0;
         while(comm->smp_pid[root_local] != comm->global_pid[root] &&
               root_local < comm->np_local) ++root_local;

         DEBUG_OUT(LVL6,(stdout,"%s: root_local=%i.\n",DDI_Id(),root_local))

         if(root_local >= comm->np_local) {
            fprintf(stdout,"%s: Could not find root process in SMP list on node.\n",DDI_Id());
            Fatal_error(911);
         }

         Comm_bcast_smp(buffer,size,root_local,comm);
      }

   /* ------------------------ *\
      Single node returns here
   \* ------------------------ */
      if(comm->nn == 1) return;

   /* ----------------------------------- *\
      Broadcast amoungst the node masters
   \* ----------------------------------- */
      list.np   = comm->nn;
      list.me   = comm->my;
      list.pids = comm->node_master;
      list.root = comm->local_nid[root];
    # if defined DDI_MPI
      list.comm = comm->node_comm;
    # endif

      DEBUG_OUT(LVL6,(stdout,"%s: inter-node bcast.\n",DDI_Id()))
      DEBUG_OUT(LVL6,(stdout,"%s: root_node=%i.\n",DDI_Id(),comm->local_nid[root]))
      
      if(comm->me_local == 0) Comm_bcast_list(buffer,size,&list);

   /* ------------------------------------------------ *\
      Broadcast within the node for all non-root nodes
   \* ------------------------------------------------ */
      DEBUG_OUT(LVL6,(stdout,"%s: bcast to smp processes from masters.\n",DDI_Id()))
      if(comm->local_nid[root] != comm->my && comm->np_local > 1) {
         Comm_bcast_smp(buffer,size,0,comm);
      }
    # endif

      DEBUG_OUT(LVL4,(stdout,"%s: Exiting Comm_bcast.\n",DDI_Id()))
   }
 

   void Comm_bcast_list(void *buff,size_t size,const DDI_List *list) {

      int me,from,to,l_node,h_node,b_node;

    # if defined DDI_MPI && !defined USE_DDI_COLLECTIVE_ROUTINES
      size_t remaining, buffer_size, working_size;
      char *buffer = (char *) buff;
    # endif

      DEBUG_OUT(LVL6,(stdout,"%s: entering Comm_bcast_list.\n",DDI_Id()))

    # if defined DDI_CHECK_ARGS
      if(list->root < 0 || list->root > list->np) {
         fprintf(stdout,"%s: Comm_bcast_list - invalid value for root (%i).\n",DDI_Id(),list->root);
         Fatal_error(911);
      }
    # endif

      me = ((list->me - list->root) + list->np) % list->np; 
      l_node = 0;
      h_node = list->np;

   /* ---------------------------------------------------------------- *\
    * Decide whether to use MPI or traditional DDI collective routines
   \* ---------------------------------------------------------------- */
    # if defined DDI_MPI && !defined USE_DDI_COLLECTIVE_ROUTINES
      DEBUG_OUT(LVL7,(stdout,"%s: calling MPI_Bcast.\n",DDI_Id()))

      remaining = size;
      buffer_size = 10*DDI_BUFFER_SIZE;

      while (remaining) {
                                     working_size = buffer_size;
         if(remaining < buffer_size) working_size = remaining;

         MPI_Bcast(buffer,working_size,MPI_BYTE,list->root,list->comm);

         remaining -= working_size;
         buffer    += working_size;
      }
    # else
   /* --------------------------------------------------------------------- *\
    * Using the DDI algroithm, perform a binary tree bcast and PTP messages
   \* --------------------------------------------------------------------- */
      DEBUG_OUT(LVL7,(stdout,"%s: performing b-tree bcast.\n",DDI_Id()))
      while((h_node - l_node) > 1) {

         b_node = l_node + (h_node - l_node)/2;
  
         to   = b_node;
         from = l_node;
   
         if(list->root) {
            to   = (b_node + list->root) % list->np;
            from = (l_node + list->root) % list->np;
         }

         if(me == l_node) Comm_send_list(buff,size,to,list);
         if(me == b_node) Comm_recv_list(buff,size,from,list);

         if(me <  b_node) h_node = b_node;
         if(me >= b_node) l_node = b_node;

      }
    # endif

      DEBUG_OUT(LVL6,(stdout,"%s: leaving Comm_bcast_list.\n",DDI_Id()))
   }

   void Comm_bcast_node(void *buff,size_t size,int root,const DDI_Comm *comm) {
      DDI_List list;

      DEBUG_OUT(LVL6,(stdout,"%s: entering Comm_bcast_smp.\n",DDI_Id()))

      list.np   = comm->nn;
      list.me   = comm->my;
      list.root = root;
      list.pids = comm->node_master;
    # if defined DDI_MPI
      list.comm = comm->node_comm;
    # endif

      Comm_bcast_list(buff,size,&list);

      DEBUG_OUT(LVL6,(stdout,"%s: leaving Comm_bcast_smp.\n",DDI_Id()))
   }

   void Comm_bcast_smp(void *buff,size_t size,int root,const DDI_Comm *comm) {
      DDI_List list;

      DEBUG_OUT(LVL6,(stdout,"%s: entering Comm_bcast_smp.\n",DDI_Id()))

      list.np   = comm->np_local;
      list.me   = comm->me_local;
      list.root = root;
      list.pids = comm->smp_pid;
    # if defined DDI_MPI
      list.comm = comm->smp_comm;
    # endif

      Comm_bcast_list(buff,size,&list);

      DEBUG_OUT(LVL6,(stdout,"%s: leaving Comm_bcast_smp.\n",DDI_Id()))
   }


/*
      int i;
      size_t working_size,remaining=size;
      DDI_Data *data = (DDI_Data *) Data_find(comm->data_id);

      
      if(comm->np_local == 1) return;

      char  *pdata       = (char *) buff;
      void  *buffer      = data->buffer;
      size_t buffer_size = data->buffer_size;

    # if defined DDI_CHECK_ARGS
      if(root < 0 || root > comm->np_local) {
        fprintf(stdout,"%s: Invalid argument for root=%i in Comm_bcast_smp.\n",DDI_Id(),root);
        Fatal_error(911);
      }
    # endif

      while(remaining) {

         working_size = buffer_size;
         if(remaining < buffer_size) working_size = remaining;

         Comm_sync_smp_fast(comm,data);
         if(comm->me_local == root) memcpy(buffer,pdata,working_size);
         Comm_sync_smp_fast(comm,data);
         if(comm->me_local != root) memcpy(pdata,buffer,working_size);

         remaining -= working_size;
         pdata     += working_size;

      }

      Comm_sync_smp_fast(comm,data);

      DEBUG_OUT(LVL6,(stdout,"%s: leaving Comm_bcast_smp.\n",DDI_Id()))
*/  


