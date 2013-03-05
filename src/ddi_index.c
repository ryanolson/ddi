/* ------------------------------------------------------------------ *\
   Distributed Data Interface (DDI)
   Virtual Shared-Memory Object (DDI_Index.o)

   This object is meant to contain all the vital function necessary
   for managing the memory associated with distributed-data storage.
   Whether this memory is in the local address space of the compute
   process or the data server, or this memory is a System V shared-
   memory segment ... all the functions for indexing and managing
   the memory segment are contained herein.

   Author: Ryan M. Olson
   CVS $Id: ddi_index.c,v 1.5 2006/11/28 03:06:32 andrey Exp $
\* ------------------------------------------------------------------ */
 # include "ddi_base.h"

   static int *nwrites = NULL;

/* ------------------------------------------- *\
   DDI_Index_create
   ================
   Creates an entry in the shared-memory index
   for a new patch of a distributed array and
   reserves memory on the shared-memory stack
   for the patch of the distributed array.
\* ------------------------------------------- */
   void DDI_Index_create(const DDI_Patch *msg) { 
      void *buffer = NULL;
      int nrows,ncols,handle = msg->handle;
      DDA_Index *Index = gv(dda_index);
      size_t offset,size;
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(gv(ddi_working_comm));
      int i;

      if(!Index) {
         if(comm->me == 0 || comm->me == comm->np) {
            fprintf(stdout," DDI Error: Distributed memory was not initialized.\n");
            Fatal_error(911);
      }  }

      nrows = msg->ihi - msg->ilo + 1;
      ncols = msg->jhi - msg->jlo + 1;
      size  = nrows * ncols;

      DDI_Memory_push(size*sizeof(double),&buffer,&offset);

      Index[handle].ilo  = msg->ilo;
      Index[handle].ihi  = msg->ihi;
      Index[handle].jlo  = msg->jlo;
      Index[handle].jhi  = msg->jhi;
      Index[handle].size = size;
      Index[handle].ncols  = msg->size;
      Index[handle].nrows  = nrows;
      Index[handle].offset = offset;

#if defined DDI_ARMCI
      DDI_ARMCI_Index_create(Index,handle);
      DDI_ARMCI_Zero_local(handle);
#else
    # if defined USE_SYSV
      Index[handle].semid  = gv(dda_access);
    # endif

      DDI_Array_zero(handle);
#endif
   }


/* ------------------------------------------- *\
   DDI_Index_destroy
   =================
   Deletes an entry in the shared-memory index
   for an existing patch and also frees the
   memory in the shared-memory stack reserved
   for this patch of a distributed array.
\* ------------------------------------------- */
   void DDI_Index_destroy(const DDI_Patch *msg) {
      int handle = msg->handle;
      DDA_Index *Index = gv(dda_index);
      size_t size = Index[handle].size;

      Index[handle].ilo = 0;
      Index[handle].ihi = 0;
      Index[handle].jlo = 0;
      Index[handle].jhi = 0;
      Index[handle].size  = 0;
      Index[handle].ncols = 0;
      Index[handle].nrows = 0;
      Index[handle].semid = 0;
      Index[handle].offset = 0;

      DDI_Memory_pop(size*sizeof(double));
   }


/* --------------------------------------------- *\
   DDI_Array_zero
   ==============
   Initializes every element in an array to zero
\* --------------------------------------------- */
   void DDI_Array_zero(int handle) {
      double *da = NULL;
      const DDA_Index *Index = gv(dda_index);
      size_t i,size = Index[handle].size;

      DDI_Acquire(Index,handle,DDI_WRITE_ACCESS,(void **) &da);
      for(i=0; i<size; i++) da[i] = 0.0;
      DDI_Release(Index,handle,DDI_WRITE_ACCESS);
   }


/* ------------------------------------------------------------------- *\
   Acquire a WRITE or FULL lock on array 'handle'
   [IN] INDEX  - index of a distributed data segment
   [IN] HANDLE - handle of array w/in the DD segment to acquire
   [IN] LTYPE  - type of lock in which to access the array
   [OUT] ARRAY - starting pointer of array 'handle' in segment 'index'
\* ------------------------------------------------------------------- */
   void DDI_Acquire(const DDA_Index *index,int handle,int ltype,void **array) {
      char *buffer = (char *) index;
    # if defined USE_SYSV
      int error,semid = index[handle].semid;
      struct sembuf op;
    # endif

/*    DDI_PROFILE_START() */

    # if defined DDI_MAX_DEBUG
      if(ltype != DDI_READ_ACCESS && ltype != DDI_WRITE_ACCESS) {
         fprintf(stdout,"%s: Invalid lock type requested.\n",DDI_Id());
         Fatal_error(911);
      }
    # endif

   /* --------------------------------------------------------------------- *\
      Set pointer 'array' to the start of array 'handle' in segment 'index'
   \* --------------------------------------------------------------------- */
      buffer += index[handle].offset;
      *array  = (void *) buffer;

   /* --------------------------------------------------------------------- *\
      Acquire a WRITE or FULL lock on the array.  Note: semop is a blocking
      call, ie the process will stall until proper access has been granted.
   \* --------------------------------------------------------------------- */
    # if defined(USE_SYSV)
    # if defined(DDI_ONESIDED)
      if(ltype == DDI_READ_ACCESS) return;
      error = pthread_mutex_lock(&index[handle].mutex);
      PTHREAD_RC_CHECK(error,"lock in ddi_acquire");
    # else
      op.sem_num = handle;
      op.sem_op  = -ltype;
      op.sem_flg = 0;

      error = Semop(semid,&op,1);

      if(error == -1) {
         fprintf(stdout,"%s: Semop returned an error in DDI_Acquire.\n",DDI_Id());
         fprintf(stdout,"%s: Error on array %i, access type %i.\n",DDI_Id(),handle,ltype);
         Fatal_error(911);
      }
    # endif
    # endif

/*    DDI_PROFILE_STOP() */
   }


/* ------------------- *\
   DDI_Release
   ===========
\* ------------------- */
   void DDI_Release(const DDA_Index *index,int handle,int ltype) {
    # if defined(USE_SYSV)
    # if defined(DDI_ONESIDED)
      int error;
      if(ltype == DDI_READ_ACCESS) return;
      error = pthread_mutex_unlock(&index[handle].mutex);
      PTHREAD_RC_CHECK(error,"unlock in ddi_release");
    # else
      int error,semid = index[handle].semid;
      struct sembuf op;

      op.sem_num = handle;
      op.sem_op  = ltype;
      op.sem_flg = 0;

      error = Semop(semid,&op,1);

      if(error == -1) {
         fprintf(stdout,"%s: Semop returned an error in DDI_Release.\n",DDI_Id());
         Fatal_error(911);
      }
    # endif
    # endif
   }


/* ------------------------------------------------- *\
   Fencing access to the distributed-memory segments

   DDI_Fence_init - initialize parameters
   DDI_Fence_acquire
   DDI_Fence_release
   DDI_Fence_check

   _acquire will block all other operations except
   the acquiring operation.  normally we will just
   acquire a fence on a single node, but in there
   is an option to acquire a fence across all nodes,
   however, this will probably not be the best per-
   forming option.
\* ------------------------------------------------- */ 
 # if defined USE_SYSV

/* -------------------------- *\
   Aquire access to the fence
\* -------------------------- */
   void DDI_Fence_init() {
      int i;
      char *location = (char *) gv(dda_index); 

    # if FULL_SMP
      int np,me;
      DDI_SMP_NProc(&np,&me);
      location = (char *) gv(smp_index)[0];
    # endif

      location += MAX_DD_ARRAYS*sizeof(DDA_Index);
      nwrites   = (int *) location;
      location += MAX_DD_ARRAYS*sizeof(int);
      location += sizeof(size_t);
      location += sizeof(size_t);
      location += sizeof(size_t);
      gv(dlb_counter)  = (size_t *) location;  location += sizeof(size_t);
      gv(gdlb_counter) = (size_t *) location;

    # if FULL_SMP
      if(me != 0) return;
    # endif
      
      for(i=0; i<MAX_DD_ARRAYS; i++) nwrites[i] = 0;
      *gv(dlb_counter) = 0;
      *gv(gdlb_counter) = 0;

   }


/* ---------------- *\
   Set up the fence
\* ---------------- */
   void DDI_Fence_acquire(int handle) {
      int semid = gv(fence_access);
      struct sembuf op;

    # if defined(DDI_ONESIDED)
      int rc;
      DDA_Index *dda = (DDA_Index *) gv(smp_index)[0];
      rc = pthread_spin_lock(&dda[handle].fence);
      PTHREAD_RC_CHECK(rc,"spin lock in fence acquire");
    # else
      op.sem_num = handle;
      op.sem_op  = -DDI_WRITE_ACCESS;
      op.sem_flg = 0;
      if(Semop(semid,&op,1) == -1) {
         fprintf(stdout,"%s: Semop error on semid=%i in DDI_Fence_acquire.\n",DDI_Id(),semid);
         Fatal_error(911);
      }
    # endif

      ++nwrites[handle];
    
    # if defined(DDI_ONESIDED)
      rc = pthread_spin_unlock(&dda[handle].fence);
      PTHREAD_RC_CHECK(rc,"spin unlock in fence acquire");
    # else
      op.sem_op = DDI_WRITE_ACCESS;
      if(Semop(semid,&op,1) == -1) {
         fprintf(stdout,"%s: Semop error on semid=%i in DDI_Fence_acquire.\n",DDI_Id(),semid);
         Fatal_error(911);
      }
    # endif
   }

/* ------------------- *\
   Tear down the fence
\* ------------------- */
   void DDI_Fence_release(int handle) {
      int semid = gv(fence_access);
      struct sembuf op;

    # if defined(DDI_ONESIDED)
      int rc;
      DDA_Index *dda = (DDA_Index *) gv(smp_index)[0];
      rc = pthread_spin_lock(&dda[handle].fence);
      PTHREAD_RC_CHECK(rc,"spin lock in fence release");
    # else
      op.sem_num = handle;
      op.sem_op  = -DDI_WRITE_ACCESS;
      op.sem_flg = 0;
      if(Semop(semid,&op,1) == -1) {
         fprintf(stdout,"%s: Semop error semid=%i in DDI_Fence_release.\n",DDI_Id(),semid);
         Fatal_error(911);
      }
    # endif

      --nwrites[handle];

    # if defined(DDI_ONESIDED)
      rc = pthread_spin_unlock(&dda[handle].fence);
      PTHREAD_RC_CHECK(rc,"spin unlock in fence acquire");
    # else
      op.sem_op = DDI_WRITE_ACCESS;
      if(Semop(semid,&op,1) == -1) {
         fprintf(stdout,"%s: Semop error semid=%i in DDI_Fence_release.\n",DDI_Id(),semid);
         Fatal_error(911);
      }
    # endif
   }

/* ----------------------------------------------- *\
   Check to see if a fence exists.  If one exists,
   then return after it has been taken down.
\* ----------------------------------------------- */
   void DDI_Fence_check(int handle) {
      int condition,semid = gv(fence_access);
      struct sembuf acq,rel;
      struct timeval timer;

    # if defined(DDI_ONESIDED)
      int rc;
      DDA_Index *dda = (DDA_Index *) gv(smp_index)[0];
    # else
      timer.tv_sec  = 0;
      timer.tv_usec = 5;

      acq.sem_num = handle;
      acq.sem_op  = -DDI_READ_ACCESS;
      acq.sem_flg = 0;

      rel.sem_num = handle;
      rel.sem_op  = DDI_READ_ACCESS;
      rel.sem_flg = 0;
    # endif

      do {

       # if defined(DDI_ONESIDED)
         rc = pthread_spin_lock(&dda[handle].fence);
         PTHREAD_RC_CHECK(rc,"spin lock in fence release");
       # else
         if(Semop(semid,&acq,1) == -1) {
            fprintf(stdout,"%s: Semop error in DDI_Fence_check (acq); handle=%i.\n",DDI_Id(),handle);
            Fatal_error(911);
         }
       # endif

         condition = nwrites[handle];

       # if defined(DDI_ONESIDED)
         rc = pthread_spin_unlock(&dda[handle].fence);
         PTHREAD_RC_CHECK(rc,"spin unlock in fence release");
       # else
         if(Semop(semid,&rel,1) == -1) {
            fprintf(stdout,"%s: Semop error in DDI_Fence_check (rel); handle=%i.\n",DDI_Id(),handle);
            Fatal_error(911);
         }

         if(condition) select((int) NULL,NULL,NULL,NULL,&timer);
       # endif


      } while(condition > 0);

   }

 # endif
