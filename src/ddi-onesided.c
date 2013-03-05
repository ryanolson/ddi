
#include "ddi_base.h"

typedef struct ddi_onesided_dbuff_s {
        int active;
        void *addr;
        size_t length;
        DDI_Patch msginfo;
        cos_desc_t comm_desc;
} dbuff_t;


int ddi_onesided_ds_handler(void *);

static double tget = 0.0;
static double tput = 0.0;
static double tacc = 0.0;
static double tnext = 0.0;
static double twait = 0.0;

static size_t iacc = 0;
static size_t sacc = 0.0;

dbuff_t *dbuff = NULL;

void
ddi_onesided_init()
{       
        int i;
        cos_parameters_t cos_params;

        cos_params.options        = ONESIDED_DS_PER_SMPD;
        cos_params.nDataServers   = 1;
        cos_params.maxDescriptors = DDI_MAX_DESCRIPTORS;
        cos_params.maxRequestSize = DDI_MAX_REQUEST_SIZE;
        cos_params.dsHandlerFunc  = ddi_onesided_ds_handler;
 
        COS_Init( &cos_params );
 
        for(i=0; i<DDI_MAX_DESCRIPTORS; i++) cpReqCreate(&cos_req[i]);
 
        int nn, my;
        unsigned long mask;
        unsigned int len = sizeof(mask);
 
        DDI_NNode(&nn,&my);
 
        if(my == 0) {    
           sched_getaffinity(0, len, (cpu_set_t *) &mask);
           printf("%d [cp] get_affinity mask = %p\n", cos_me, mask);
        }
}



int 
ddi_onesided_ds_handler(void *buffer)
{
        size_t counter_value = 0;
        DDI_Patch *msg = (DDI_Patch *) buffer;
        int from = msg->tag.response_mdesc.id.rank;
        double t1,t2;

      # ifdef DBUFFER
        int ibuff = 0;
        int active = 0;
        if(dbuff == NULL) {
           dbuff = (dbuff_t *) malloc(2*sizeof(dbuff_t));
           bzero(dbuff,2*sizeof(dbuff_t));
        }

        do {
     // if ibuff is active wait on it and finish the request
           if(dbuff[ibuff].active) {
              t1 = clock_highres();
              dsDescWait(&dbuff[ibuff].comm_desc);
              t2 = clock_highres();
              twait += (t2-t1);
              if(dbuff[ibuff].msginfo.oper == DDI_ACC) {
                 t1 = clock_highres();
                 iacc++; sacc += dbuff[ibuff].msginfo.size;
                 DDI_Acc_local(&dbuff[ibuff].msginfo, dbuff[ibuff].addr);
                 DDI_Fence_release(dbuff[ibuff].msginfo.handle);
                 t2 = clock_highres();
                 tacc += (t2-t1);
              } else
              if(dbuff[ibuff].msginfo.oper == DDI_PUT) {
                 DDI_Put_local(&dbuff[ibuff].msginfo, dbuff[ibuff].addr);
                 dsDescInit(&dbuff[ibuff].msginfo.tag.response_mdesc, &dbuff[ibuff].comm_desc);
                 dbuff[ibuff].comm_desc.event_type = EVENT_LOCAL | EVENT_REMOTE;
                 cosPut(NULL, 0, &dbuff[ibuff].comm_desc);
                 t1 = clock_highres();
                 dsDescWait(&dbuff[ibuff].comm_desc);
                 t2 = clock_highres();
                 twait += (t2-t1);
              } 
              dbuff[ibuff].active = 0;
              active--;
           }

           if(msg) {
           // fence accs
              if(msg->oper == DDI_ACC) DDI_Fence_acquire(msg->handle);
           // copy the data request to the dbuff queue 
              if(dbuff[ibuff].length < msg->size) {
                 if(dbuff[ibuff].addr) free(dbuff[ibuff].addr);
                 dbuff[ibuff].addr = (void *) malloc(MAX(msg->size,(1024*ONE_KB)));
                 dbuff[ibuff].length = MAX(msg->size,(1024*ONE_KB));
                 bzero(dbuff[ibuff].addr,dbuff[ibuff].length);
              }
           // save the msg request
              memcpy(&dbuff[ibuff].msginfo, msg, sizeof(DDI_Patch));

           // as soon as we issue the get; we enable the remote PE to fire a new request
           // thus, to prevent multiple requests overlapping from the same PE check the
           // request queue prior to issuing the get
              dsGetNextRequest(&msg);

           // initialize the comm descriptor
              dsDescInit(&dbuff[ibuff].msginfo.tag.response_mdesc, &dbuff[ibuff].comm_desc);

              if(dbuff[ibuff].msginfo.oper == DDI_ACC) {
                 dbuff[ibuff].comm_desc.event_type = EVENT_LOCAL | EVENT_REMOTE;
                 cosGet(dbuff[ibuff].addr, dbuff[ibuff].msginfo.size, &dbuff[ibuff].comm_desc);
              } else
              if(dbuff[ibuff].msginfo.oper == DDI_PUT) {
                 dbuff[ibuff].comm_desc.event_type = EVENT_LOCAL;
                 cosGet(dbuff[ibuff].addr, dbuff[ibuff].msginfo.size, &dbuff[ibuff].comm_desc);
              } else 
              if(dbuff[ibuff].msginfo.oper == DDI_GET) {
                 DDI_Get_local(&dbuff[ibuff].msginfo, dbuff[ibuff].addr);
                 dbuff[ibuff].comm_desc.event_type = EVENT_LOCAL | EVENT_REMOTE;
                 cosPut(dbuff[ibuff].addr, dbuff[ibuff].msginfo.size, &dbuff[ibuff].comm_desc);
              } else {
                 cosError("unknown operation",911);
              }

              dbuff[ibuff].active++;
              active++;
           }
           ibuff = !ibuff;
        } while(active);
      # else
      # error "no double buffering
      # endif

        return 0;
}

extern void ddi_acc_print_timings();

void ddi_onesided_print_times()
{
//      if(twait > 0.0)
//      printf("%d [ds] acc=%10.3lf wait=%10.3lf; iacc=%ld; sacc=%ld\n",cos_me,tacc,twait,iacc,sacc);
     // fflush(stdout);
     // ddi_acc_print_timings();
}


#if 0
static void
ddi_onesided_req_msg(void *buffer, DDI_Patch *msginfo, int remote_node, cos_request_t *req)
{
        long ncols = msginfo->jhi-msginfo->jlo+1;
        long nrows = msginfo->ihi-msginfo->ilo+1;
        long nwords = ncols*nrows;
        size_t datalen = nwords*sizeof(double);
        size_t msgsize = sizeof(DDI_Patch);

        cpReqInit(remote_node, req);
        cpPrePostRecv(buffer, datalen, req);
        cpCopyLocalDataMDesc(req, &msginfo->tag.response_mdesc);
        cpReqSend(msginfo, msgsize, req);
}
#endif
