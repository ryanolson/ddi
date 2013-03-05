/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Wrapper subroutines for System V IPC calls.
 *
 * Author: Ryan M. Olson
 * CVS $Id: sysv_ipc.c,v 1.2 2006/05/23 20:17:51 ryan Exp $
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"
 # if defined USE_SYSV

/* --------------------------- *\
   Wrapper function for shmget
\* --------------------------- */
   int Shmget(key_t key, size_t size, int flag) {
     int shmid;
     if((shmid = shmget(key,size,flag)) < 0) {
       fprintf(stdout,"%s: shmget returned an error.\n",DDI_Id());
       switch(errno) {
         case ENOSPC:
           fprintf(stdout," Error ENOSPC: The number of shared memory identifiers for the node has been reached.\n");
           break;
         case ENOMEM:
           fprintf(stdout," Error ENOMEM: Not enough memory to allocate %li bytes of shared memory.\n",(long)size);
           break;
         case EACCES:
           fprintf(stdout," Error EACCES\n");
           break;
         case EEXIST:
           fprintf(stdout," Error EEXIST\n");
           break;
         case EINVAL:
           fprintf(stdout," Error EINVAL: Attempting to create %lu bytes of shared memory.\n",(unsigned long) size);
           fprintf(stdout," Check system limits on the size of SysV shared memory segments.\n");
           fprintf(stdout,"\n");
           fprintf(stdout," The file ~/gamess/ddi/readme.ddi contains information on how to display\n");
           fprintf(stdout," the current SystemV memory settings, and how to increase their sizes.\n");
           fprintf(stdout," Increasing the setting requires the root password, and usually a sytem reboot.\n");
           fprintf(stdout,"\n");
           fflush(stdout);
           break;
         case ENOENT:
           fprintf(stdout," Error ENOENT\n");
           break;
         default:
           fprintf(stdout," unusual shmget errno=%d\n",errno); break;
           break;
       }
       Fatal_error(911);
     }
   
     return shmid;
   }
   

/* -------------------------- *\
   Wrapper function for shmat 
\* -------------------------- */
   void *Shmat(int shmid, void *addr, int flag) {
     void *shmaddr = NULL;
     void *error   = (void *) -1;
     const DDI_Comm *comm = (DDI_Comm *) &gv(ddi_base_comm);

   
     if((shmaddr = shmat(shmid,addr,flag)) == error) {
       fprintf(stdout," DDI Process %i: shmat returned an error.\n",comm->me);
       switch(errno) {
         case EINVAL:
           fprintf(stdout," Error EINVAL: shmid=%i is not a valid shared memory identifier.\n",shmid);
           break;
         case ENOMEM:
           fprintf(stdout," Error ENOMEM: Can't attach shared memory segment due to data space limits.\n");
           break;
         case EACCES:
           fprintf(stdout," Error EACCES: Permission to attach to the shared memory segment denied.\n");
           break;
         case EMFILE:
           fprintf(stdout," Error EMFILE: no. of shared memory segments exceeds system-imposed limit.\n");
           break;
         default:
           fprintf(stdout," unusual shmat errno=%d\n",errno); break;
           break;
       }
       fflush(stdout);
       Fatal_error(911);
     }
   
     return shmaddr;
   }
  
 
/* --------------------------- *\
   Wrapper function for shmctl 
\* --------------------------- */
   int Shmctl(int shmid, int cmd, struct shmid_ds *buff) {
     int ret;
     const DDI_Comm *comm = (DDI_Comm *) &gv(ddi_base_comm);
   
     if((ret = shmctl(shmid,cmd,buff)) != 0) {
       fprintf(stdout," DDI Process %i: shmctl return an error.\n",comm->me);
       switch(errno) {
         case EINVAL:
           fprintf(stdout," Error EINVAL: shmid is not a valid shared memory segment.\n");
           break;
         case EFAULT:
           fprintf(stdout," Error EFAULT: argument 3 is not a valid struct shmid_ds.\n");
           break;
         case EPERM:
           fprintf(stdout," Error EPREM: permission to access/change shared mem segment denied.\n");
           break;
         default:
           fprintf(stdout," unusual shmctl errno=%d\n",errno); break;
           break;
       }
       Fatal_error(911);
     }
   
     return ret;
   
   }


/* --------------------------- *\
   Wrapper function for semget
\* --------------------------- */
   int Semget(key_t key,int nsems,int semflg) {
      int ret;

      if((ret = semget(key,nsems,semflg)) == -1) {
         fprintf(stdout,"%s: semget return an error.\n",DDI_Id());
         switch(errno) {
           case EACCES: fprintf(stdout," semget errno=EACCES.\n"); break;
           case EINVAL: fprintf(stdout," semget errno=EINVAL.\n"); break;
           case ENOENT: fprintf(stdout," semget errno=ENOENT.\n"); break;
           case ENOSPC: fprintf(stdout," semget errno=ENOSPC -- check system limit for sysv semaphores.\n"); break;
           case ENOMEM: fprintf(stdout," semget errno=ENOMEM.\n"); break;
           case EEXIST: fprintf(stdout," semget errno=EEXIST.\n"); break;
           default:
             fprintf(stdout," unusual semget errno=%d\n",errno); break;
         }
         Fatal_error(911);
      }

      return ret;
   }


/* -------------------------- *\
   Wrapper function for semop
\* -------------------------- */
   int Semop(int semid,struct sembuf *opers,size_t nops) {
      int ret;

      if((ret = semop(semid,opers,nops)) == -1) {
         fprintf(stdout,"%s: semop return an error performing %i operation(s) on semid %i.\n",DDI_Id(),(int) nops,semid);
         switch(errno) {
           case EFBIG:  fprintf(stdout," semop errno=EFBIG.\n"); break;
           case E2BIG:  fprintf(stdout," semop errno=E2BIG.\n"); break;
           case EINTR:  fprintf(stdout," semop errno=EINTR.\n"); break;
           case EINVAL: fprintf(stdout," semop errno=EINVAL.\n"); break;
           case EACCES: fprintf(stdout," semop errno=EACCES.\n"); break;
           case EAGAIN: fprintf(stdout," semop errno=EAGAIN.\n"); break;
           case ENOSPC: fprintf(stdout," semop errno=ENOSPC.\n"); break;
           case ERANGE: fprintf(stdout," semop errno=ERANGE.\n"); break;
           case EFAULT: fprintf(stdout," semop errno=EFAULT.\n"); break;
           default:
              fprintf(stdout," unusual semop errno=%d\n",errno); break;
         }
      }

      return ret;
   }


/* -------------------
   DDI_Sem_oper
   ============
\* ------------------- */
   int DDI_Sem_oper(int semid,int semnum,int semop) {
      struct sembuf op;

      op.sem_op  = semop;
      op.sem_num = semnum;
      op.sem_flg = 0;

      return Semop(semid,&op,1);
   }


/* ------------------------------------------------- *\
   DDI_Sem_acquire
   ===============
   Acquire a user defined access level to a resource
\* ------------------------------------------------- */
   void DDI_Sem_acquire(int semid,int semnum,int access) {
      struct sembuf op;

      op.sem_op = -access;
      op.sem_num = semnum;
      op.sem_flg = 0;

      Semop(semid,&op,1);
   }


/* --------------------------------------------- *\
   DDI_Sem_release
   ===============
   Release an acquire access to a given resource
\* --------------------------------------------- */
   void DDI_Sem_release(int semid,int semnum,int access) {
      struct sembuf op;

      op.sem_op  = access;
      op.sem_num = semnum;
      op.sem_flg = 0;

      Semop(semid,&op,1);
   }


/* ------------------------------------------- *\
   DDI_Sem_remove
   ==============
   Remove a System V semaphore from the system
\* ------------------------------------------- */
   void DDI_Sem_remove(int semid) {
    # if defined MACOSX
      union semun arg;
      semctl(semid,0,IPC_RMID,arg);
    # else
      semctl(semid,0,IPC_RMID);
    # endif
   } 


/* -------------------------------------------------- *\
   DDI_Shm_remove
   ==============
   Remove a System V shared-memory id from the system
\* -------------------------------------------------- */
   void DDI_Shm_remove(int shmid) {
      Shmctl(shmid,IPC_RMID,NULL);
   }

 # else

   void DDI_Sysv_dummy(void) { return;  }

 # endif
