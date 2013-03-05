/* ------------------------------------------------------------------------ *\
 *    GENERALIZED DISTRIBUTED DATA INTERFACE - 'MPMD' DATA SERVER MODEL     *
 *             Common Prototypes and Data Structures Shared                 *
 *               by the Kickoff Program and the DDI Tasks                   *
\* ------------------------------------------------------------------------ */
 # ifndef __COMMON_H__
 # define __COMMON_H__

/* -------------------------------------------------------- *\
   Common values
\* -------------------------------------------------------- */
 # define ONE_KB           1024
 # define ONE_MB        1048576
 # define TEN_MB       10485760
 # define ONE_GB     1073741824
 # define ONE_MWORD     1000000

 # if !defined MAX_PTP_MSG_SIZE
 # define MAX_PTP_MSG_SIZE ONE_GB
 # endif

 # if !defined MAX_DS_MSG_WORDS
 # define MAX_DS_MSG_WORDS ONE_MWORD*20
 # endif

 # if !defined MAX_DS_MSG_SIZE
 # define MAX_DS_MSG_SIZE  MAX_DS_MSG_WORDS*sizeof(double) 
 # endif

 # if !defined CACHE_LINE_SIZE
 # define CACHE_LINE_SIZE 256*ONE_KB
 # endif

/* -------------------------------------------------------- *\
   Limits on the number of nodes, processors per node, etc.
\* -------------------------------------------------------- */
 # if !defined MAX_NODES
 # define MAX_NODES 64
 # endif

 # if !defined MAX_SMP_PROCS
 # define MAX_SMP_PROCS 4
 # endif

 # define MAX_PROCESSORS MAX_NODES*MAX_SMP_PROCS

 # if defined USE_SYSV && MAX_SMP_PROCS > 1
 # define FULL_SMP 1
 # else
 # define FULL_SMP 0
 # endif

 # if defined DDI_MPI && defined DDI_SOC && !defined USE_TCP_LOCAL
 # define USE_TCP_LOCAL
 # endif

 # if defined DDI_LAPI || defined DDI_MPI2 || defined DDI_ARMCI || defined DDI_ONESIDED
 #  define USING_DATA_SERVERS() 0
 # else
 #  if !FULL_SMP || defined DDI_MPI
 #   ifdef CRAY_MPI 
 #    define USING_DATA_SERVERS() (gv(ddi_base_comm).nn > 1 && gv(nd))
 #   else
 #    define USING_DATA_SERVERS() 1
 #   endif
 #  else
 #   define USING_DATA_SERVERS() (gv(ddi_base_comm).nn > 1 && gv(nd))
 #  endif
 # endif


/* ---------------------------------- *\
   Structure to hold node information
\* ---------------------------------- */
   typedef struct {
     char *hostname;       /* hostname for the node */
     int  nodemaster;      /* rank of master on node */
     int  cpus;            /* number of cpus on node */
     int  nics;            /* number of HPC nics */
     int  nets;            /* number of unique network names */
     int  myds;            /* rank of data server for inter-node communication */
     char *netext[8];      /* extensions to the hostname for premium network */
     char *network[8];     /* used when the premium network has completely
                              different hostnames than the standard network */
   } Node_info;


/* ---------------------------------------------- *\
   Structure to hold the data for one ddi process
\* ---------------------------------------------- */
   typedef struct {
     int port;
     int node;
     char *hostname;
   } Proc_info;


/* ------------------------------------------------------- *\
   Mid-level subroutine that creates client/server sockets
\* ------------------------------------------------------- */
 # if defined DDI_SOC
 # define SERVER_SOCKET  10000
 # define CLIENT_SOCKET  10001
   int SOC_Create(int,int*,const char*);
 # endif


/* --------------------------------------------------------------------------------- *\
   Subroutine for extracting DDI information from the command-line arguments & files
\* --------------------------------------------------------------------------------- */
   int Parse_node_args(int,char**,int,int,Node_info*);


/* ---------------------------- *\
   Common Fatal_error prototype
\* ---------------------------- */
   void Fatal_error(int);


 # endif
