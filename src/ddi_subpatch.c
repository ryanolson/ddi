/* -------------------------------------------------------------------- *\
 * Distributed Data Interface
 * ==========================
 * 
 * Subroutines associated with determining the distribution by node of
 * a given patch.
 *
 * Author: Ryan M. Olson
 * extensive revisions by Andrey Asadchev in March 2007.
 * these allow for MPI-2/ARMCI which do not store the DDI arrays in
 * SystemV shared memory, but since they are one-sided, also do not have
 * data servers to store this in 'malloc' arrays in the data servers.
\* -------------------------------------------------------------------- */
 # include "ddi_base.h"


/* -------------------------------------------------------------------- *\
   DDI_Subpatch(handle,patch,nsubp,ranks,subp)
   ===========================================
   [IN]  handle - Handle of the DD array.
   [IN]  patch  - Dimensions of full patch.
   [OUT] nsubp  - Number of subpatches.
   [OUT] ranks  - ranks of nodes for each subpatch.
   [OUT] subp   - Array of subpatches disjoint by node.
   
   A patch of a distributed array may be spread across multiple nodes.
   This subroutine determines the set of subpatches disjoint by node
   needed to for the requested patch.
\* -------------------------------------------------------------------- */
   void DDI_Subpatch(int handle,const DDI_Patch* patch,int *nsubp,int *ranks,DDI_Patch *subp) {
      DDI_Subpatch_comm(handle,patch,nsubp,ranks,subp,DDI_WORKING_COMM);
   }

   void DDI_Subpatch_comm(int handle,const DDI_Patch* patch,int *nsubp,int *ranks,DDI_Patch *subp,int commid) {
      int i,np,me,nsub;
      DDI_Patch s,*t;
      const DDI_Comm *comm = (const DDI_Comm *) Comm_find(commid);

      np = comm->nn;
      me = comm->my;
      
      for(i=0,nsub=0; i<np; i++) {

         DDI_NDistribP(handle,i,&s);

         if(s.jlo > patch->jhi || s.jhi < patch->jlo) { } else {
            ranks[nsub] = i;
            t = &subp[nsub];
            t->oper   = patch->oper;
            t->handle = handle;
            t->ilo    = patch->ilo;
            t->ihi    = patch->ihi;
            t->jlo    = s.jlo;
            t->jhi    = s.jhi;
            if(patch->jlo > s.jlo) t->jlo = patch->jlo;
            if(s.jhi > patch->jhi) t->jhi = patch->jhi;
            
            t->size  = (t->jhi-t->jlo+1)*(t->ihi-t->ilo+1);
            t->size *= sizeof(double);            
          
          # if defined DDI_LAPI
            t->cp_lapi_id     = -1;
            t->cp_lapi_cntr   = NULL;
            t->cp_buffer_addr = NULL;
            t->ds_buffer_size = 0;
          # endif
            
            DEBUG_OUT(LVL6,(stdout,"%s: Subpatch #%i {%i,%i,%i,%i} of {%i,%i,%i,%i} for array %i is on %i.\n",
                      DDI_Id(),nsub,t->ilo,t->ihi,t->jlo,t->jhi,
                      patch->ilo,patch->ihi,patch->jlo,patch->jhi,patch->handle,ranks[nsub]))

            ++nsub;
         }

      }

      *nsubp = nsub;
   }

/* subpatch by SMP processes local to node */
void DDI_Subpatch_node(int handle,const DDI_Patch* patch,int *nsubp,int *ranks,DDI_Patch *subp, int node) {
    int i,np,me,nsub;
    DDI_Patch s,*t;
#ifdef DDI_ARMCI
    int commid = gv(dda_comm)[handle];
#else
    int commid = DDI_WORKING_COMM;
#endif
    const DDI_Comm *comm = (const DDI_Comm *) Comm_find(commid);
    
    np = comm->nn;
    me = comm->my;
    
    for(i=0,nsub=0; i<np; i++) {
	if (comm->local_nid[i] != node) continue;
	
	DDI_DistribP(handle,i,&s);
	
	if(s.jlo > patch->jhi || s.jhi < patch->jlo) { } else {
	    ranks[nsub] = i;
	    t = &subp[nsub];
	    t->oper   = patch->oper;
	    t->handle = handle;
	    t->ilo    = patch->ilo;
	    t->ihi    = patch->ihi;
	    t->jlo    = s.jlo;
	    t->jhi    = s.jhi;
	    if(patch->jlo > s.jlo) t->jlo = patch->jlo;
	    if(s.jhi > patch->jhi) t->jhi = patch->jhi;
            
	    t->size  = (t->jhi-t->jlo+1)*(t->ihi-t->ilo+1);
	    t->size *= sizeof(double);            
	    
# if defined DDI_LAPI
	    t->cp_lapi_id     = -1;
	    t->cp_lapi_cntr   = NULL;
	    t->cp_buffer_addr = NULL;
	    t->ds_buffer_size = 0;
# endif
	    
	    DEBUG_OUT(LVL6,(stdout,"%s: Subpatch #%i {%i,%i,%i,%i} of {%i,%i,%i,%i} for array %i is on %i.\n",
			    DDI_Id(),nsub,t->ilo,t->ihi,t->jlo,t->jhi,
			    patch->ilo,patch->ihi,patch->jlo,patch->jhi,patch->handle,ranks[nsub]));
	    
	    ++nsub;
	}
	
    }
    
    *nsubp = nsub;
    
}
