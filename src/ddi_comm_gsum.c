/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Global Sum ==> Collective operation.
 *
 * There are 4 variant of DDI_GSum that can be called from the DDI
 * API:
 *
 * 1) DDI_GSum - performs a global sum over all compute processes in
 *    the "working" communicator.
 *
 * 2) DDI_GSum_smp - performs a global sum over all "local" compute
 *    processes within the same node within the "working" communicator
 *
 * 3) DDI_GSum_node - performs a global sum using the master process
 *    (me_local == 0) on each node within the "working" communicator
 *
 * 4) DDI_GSum_comm - performs a global sum over all compute processes
 *    within the user specifed communicator.
 *
 * There are 4 internal routines that perform the above operations:
 *
 * 1) Comm_gsum - general gsum using over all compute processes within
 *    a specified communicator.  uses both _smp and _node routines.
 *
 * 2) Comm_gsum_smp - not implemented in an efficient way.  someone
 *    should provide a means to do "real" shared-memory gsums.  to
 *    provide commuicators with a sysv shared-memory buffer, see
 *    ddi_comm.c
 *
 * 3) Comm_gsum_node - uses _list to perform a gsum over the master
 *    processes on each node.
 *
 * 4) Comm_gsum_list - performs a global sum over an general list of
 *    processes.  Used to implement the _smp and _node gsums.
 *
 * This files also contains 3 simple routines for summing ints, longs
 * and doubles.
 * 
 * Author: Ryan M. Olson
 * CVS $Id: ddi_comm_gsum.c,v 1.5 2006/06/19 23:33:51 ryan Exp $
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"

   static char buffer[DDI_BUFFER_SIZE];

   void DDI_GSum(void* data,size_t n,int type) {
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);
      
      DEBUG_ROOT(LVL1,(stdout," DDI: Entering DDI_GSum.\n"))
      DEBUG_OUT(LVL3,(stdout,"%s: Entering DDI_GSum.\n",DDI_Id()))

      Comm_gsum(data,n,type,comm);

      DEBUG_OUT(LVL3,(stdout,"%s: Leaving DDI_GSum.\n",DDI_Id()))
      DEBUG_ROOT(LVL2,(stdout," DDI: Leaving DDI_GSum.\n"))
   }

   void DDI_GSum_node(void* data,size_t n,int type) {
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);
      DEBUG_ROOT(LVL1,(stdout," DDI: Entering DDI_GSum_node.\n"))
      DEBUG_OUT(LVL3,(stdout,"%s: Entering DDI_GSum_node.\n",DDI_Id()))

      Comm_gsum_node(data,n,type,comm);

      DEBUG_OUT(LVL3,(stdout,"%s: Leaving DDI_GSum_node.\n",DDI_Id()))
      DEBUG_ROOT(LVL2,(stdout," DDI: Leaving DDI_GSum_node.\n"))
   }

   void DDI_GSum_smp(void* data,size_t n,int type) {
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(DDI_WORKING_COMM);
      DEBUG_ROOT(LVL1,(stdout," DDI: Entering DDI_GSum_smp.\n"))
      DEBUG_OUT(LVL3,(stdout,"%s: Entering DDI_GSum_smp.\n",DDI_Id()))

      Comm_gsum_smp(data,n,type,comm);

      DEBUG_OUT(LVL3,(stdout,"%s: Leaving DDI_GSum_smp.\n",DDI_Id()))
      DEBUG_ROOT(LVL2,(stdout," DDI: Leaving DDI_GSum_smp.\n"))
   }

   void DDI_GSum_comm(void *data,size_t n,int type,int comm_id) {
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(comm_id);

      DEBUG_ROOT(LVL1,(stdout," DDI: Entering DDI_GSum_comm.\n"))
      DEBUG_OUT(LVL3,(stdout,"%s: Entering DDI_GSum_comm.\n",DDI_Id()))

      Comm_gsum(data,n,type,comm);

      DEBUG_OUT(LVL3,(stdout,"%s: Leaving DDI_GSum_comm.\n",DDI_Id()))
      DEBUG_ROOT(LVL2,(stdout," DDI: Leaving DDI_GSum_comm.\n"))
   }


   void Comm_gsum(void *data,size_t n,int type,const DDI_Comm *comm) {

   /* --------------- *\
    * Local Variables
   \* --------------- */
      int i,data_size;
      DDI_List list;
      size_t buffer_size = DDI_BUFFER_SIZE;

      DEBUG_OUT(LVL4,(stdout,"%s: Entering Comm_gsum.\n",DDI_Id()))

   /* ------------------------------- *\
      One process returns immediately
   \* ------------------------------- */
      if(comm->np == 1) return;

   /* ----------------------------- *\
    * Sanity check on the arguments
   \* ----------------------------- */
    # ifdef DDI_CHECK_ARGS
      if(type != 0 && type != sizeof(int) && type != sizeof(long)) {
         fprintf(stdout,"%s: possible problem with type arg in Comm_gsum.\n",DDI_Id());
         Fatal_error(911);
      }
    # endif

   /* ----------------- *\
      Common data sizes
   \* ----------------- */
      if(type == 0)      data_size = sizeof(double);
      else if(type == 4) data_size = sizeof(int);
      else               data_size = sizeof(long);
      buffer_size /= data_size;

   /* ------------------- *\
      Intra-node sumation
   \* ------------------- */
      DEBUG_OUT(LVL5,(stdout,"%s: intra-node gsum.\n",DDI_Id()))
      if(comm->np_local > 1) Comm_gsum_smp(data,n,type,comm);

   /* ----------------------------------------- *\
      Inter-node sumation amoungst node masters
   \* ----------------------------------------- */
      DEBUG_OUT(LVL5,(stdout,"%s: inter-node gsum.\n",DDI_Id()))
      if(comm->nn > 1 && comm->me_local == 0) Comm_gsum_node(data,n,type,comm);

   /* -------------------- *\
      Intra-node Broadcast
   \* -------------------- */
      DEBUG_OUT(LVL5,(stdout,"%s: intra-node bcast.\n",DDI_Id()))
      if(comm->np_local > 1) Comm_bcast_smp(data,n*data_size,0,comm);

      DEBUG_OUT(LVL4,(stdout,"%s: Leaving Comm_gsum.\n",DDI_Id()))
   }


   void Comm_gsum_list(void *data,  size_t size_d,
                       void *buffer,size_t size_b,
                       int type, const DDI_List *list) {

   /* --------------- *\
      Local Variables
   \* --------------- */
      int me = list->me;
      int np = list->np; 
      size_t remaining = size_d;
      char *pdata = (char *) data;

      int above,below_l,below_r;
      size_t working_size,buffer_size;
      int data_size;
      size_t msg_size;

      DEBUG_OUT(LVL6,(stdout,"%s: Entering Comm_gsum_list.\n",DDI_Id()))

      if(list->np == 1) return;

      if(type == 0)      data_size = sizeof(double);
      else if(type == 4) data_size = sizeof(int);
      else               data_size = sizeof(long);

      buffer_size = size_b;

   /* ------------------------------------------------------------------ *\
      DDI algorith using PTP messages - pass data up a tree.  Note this
      is not a binary tree.
   \* ------------------------------------------------------------------ */
      DEBUG_OUT(LVL7,(stdout,"%s: performing gsum using ptp messages.\n",DDI_Id()))
      while(remaining) {

         working_size = buffer_size;
         if(remaining < buffer_size) working_size = remaining;

         msg_size = working_size*data_size;

      /* -------------------- *\
       * Use MPI if available
      \* -------------------- */
       # if defined DDI_MPI && !defined USE_DDI_COLLECTIVE_ROUTINES
         DEBUG_OUT(LVL7,(stdout,"%s: performing MPI_Allreduce.\n",DDI_Id()))
         memcpy(buffer,pdata,msg_size);
         if(type == 0) {
            MPI_Allreduce(buffer,pdata,working_size,MPI_DOUBLE,MPI_SUM,list->comm);
         } else if(type == 4) {
            MPI_Allreduce(buffer,pdata,working_size,MPI_INT,MPI_SUM,list->comm);
         } else {
            MPI_Allreduce(buffer,pdata,working_size,MPI_LONG,MPI_SUM,list->comm);
         }
       # else
      /* -------------------------------------------- *\
       * Otherwise use our traditional tree-like gsum
      \* -------------------------------------------- */
         above   = (me - 1)/2;
         below_l = 2*me + 1;
         below_r = 2*me + 2;

         if(below_l < np) {
            Comm_send_list(&msg_size,sizeof(size_t),below_l,list);
            Comm_recv_list(buffer,msg_size,below_l,list);
            if(type == 0)      Vec_sum_d((double*)pdata,(const double*)buffer,working_size);
            else if(type == 4) Vec_sum_i((int*)pdata,(const int*)buffer,working_size);
            else               Vec_sum_l((long*)pdata,(const long*)buffer,working_size);
         }
   
         if(below_r < np) {
            Comm_send_list(&msg_size,sizeof(size_t),below_r,list);
            Comm_recv_list(buffer,msg_size,below_r,list);
            if(type == 0)      Vec_sum_d((double*)pdata,(const double*)buffer,working_size);
            else if(type == 4) Vec_sum_i((int*)pdata,(const int*)buffer,working_size);
            else               Vec_sum_l((long*)pdata,(const long*)buffer,working_size);
         }

         if(me != 0) {
            Comm_recv_list(&msg_size,sizeof(size_t),above,list);
            Comm_send_list(pdata,msg_size,above,list);
         }
       # endif

         remaining -= working_size;
         pdata     += msg_size;

      }

   /* ---------------------------------------- *\
      Broadcast summed data from the root node
   \* ---------------------------------------- */
    # if !defined DDI_MPI || defined USE_DDI_COLLECTIVE_ROUTINES
      Comm_bcast_list(data,size_d*data_size,list);
    # endif

      DEBUG_OUT(LVL6,(stdout,"%s: Leaving Comm_gsum_list.\n",DDI_Id()))
   }


   void Comm_gsum_node(void *data,size_t n,int type,const DDI_Comm *comm) {

      size_t buffer_size,data_size;
      DDI_List list;

      DEBUG_OUT(LVL6,(stdout,"%s: Entering Comm_gsum_node.\n",DDI_Id()))

      list.np   = comm->nn;
      list.me   = comm->my;
      list.root = 0;
      list.pids = comm->node_master;
    # if defined DDI_MPI
      list.comm = comm->node_comm;
    # endif

      if(type == 0)      data_size = sizeof(double);
      else if(type == 4) data_size = sizeof(int);
      else               data_size = sizeof(long);
      buffer_size = DDI_BUFFER_SIZE / data_size;

      Comm_gsum_list(data,n,buffer,buffer_size,type,&list);

      DEBUG_OUT(LVL6,(stdout,"%s: Leaving Comm_gsum_node.\n",DDI_Id()))
   }


   void Comm_gsum_smp(void *data,size_t size_d,int type,const DDI_Comm *comm) {

      size_t buffer_size,data_size;
      DDI_List list;

      DEBUG_OUT(LVL6,(stdout,"%s: Entering Comm_gsum_smp.\n",DDI_Id()))

      list.np = comm->np_local;
      list.me = comm->me_local;
      list.root = 0;
      list.pids = comm->smp_pid;
    # if defined DDI_MPI
      list.comm = comm->smp_comm;
    # endif

      if(type == 0)      data_size = sizeof(double);
      else if(type == 4) data_size = sizeof(int);
      else               data_size = sizeof(long);
      buffer_size = DDI_BUFFER_SIZE / data_size;

      Comm_gsum_list(data,size_d,buffer,buffer_size,type,&list);

      DEBUG_OUT(LVL6,(stdout,"%s: Leaving Comm_gsum_smp.\n",DDI_Id()))
/*
      int i,j;
      char *buffer = NULL;
      char *pdata  = (char *) data;
      size_t working_size,buffer_size,remaining=size_d;
      int data_size;
      DDI_Data *comm_data = Data_find(comm->data_id);

      DEBUG_OUT(LVL6,(stdout,"%s: Entering Comm_gsum_smp.\n",DDI_Id()))

      if(type == 0)      data_size = sizeof(double);
      else if(type == 4) data_size = sizeof(int);
      else               data_size = sizeof(long);

      buffer      = (char *) comm_data->buffer;
      buffer_size = comm_data->buffer_size / data_size;

      DEBUG_OUT(LVL7,(stdout,"%s: shared-memory buffer_size=%i.\n",DDI_Id()))
      
      while(remaining) {

         working_size = buffer_size;
         if(remaining < buffer_size) working_size = remaining;

         DEBUG_OUT(LVL8,(stdout,"%s: remaining=%i;working_size=%i.\n",DDI_Id(),remaining,working_size))

         Comm_sync_smp_fast(comm,comm_data);
         if(comm->me_local == 0) memcpy(buffer,pdata,working_size*data_size);

         for(i=1; i<comm->np_local; i++) {
            Comm_sync_smp_fast(comm,comm_data);
            if(i == comm->me_local) {
               if(type == 0)      Vec_sum_d((double*)buffer,(const double*)pdata,working_size);
               else if(type == 4) Vec_sum_i((int*)buffer,(const int*)pdata,working_size);
               else               Vec_sum_l((long*)buffer,(const long*)pdata,working_size);
            }
         }

         Comm_sync_smp_fast(comm,comm_data);
         memcpy(pdata,buffer,working_size*data_size);

         remaining -= working_size;
         pdata     += working_size*data_size;

      }

      Comm_sync_smp_fast(comm,comm_data);

      DEBUG_OUT(LVL6,(stdout,"%s: Leaving Comm_gsum_smp.\n",DDI_Id()))
*/

   }


   void Vec_sum_d(double *dest,const double *src,size_t size) {
      size_t i;
      for(i=0; i<size; i++) dest[i] += src[i];
   }


   void Vec_sum_i(int *dest,const int *src,size_t size) {
      size_t i;
      for(i=0; i<size; i++) dest[i] += src[i];
   }


   void Vec_sum_l(long *dest,const long *src,size_t size) {
      size_t i;
      for(i=0; i<size; i++) dest[i] += src[i];
   }

