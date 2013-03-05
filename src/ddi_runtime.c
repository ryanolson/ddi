#include <sys/utsname.h>
#include "ddi_base.h"
#include "ddi_runtime.h"
#include "ddi_util.h"

void DDI_Get_version(int *version, int *subversion) {
    *version = DDI_VERSION;
    *subversion = 0;
}

/** DDI build string */
void DDI_Get_build(char *build) {

#if defined USE_SYSV
    join(build,"SysV","/");
#endif

#if FULL_SMP
    join(build,"SMP","/");
#endif

#if defined DDI_SOC
    join(build,"Sockets","/");
#endif

#if defined DDI_MPI
#if defined DDI_MPI2
    join(build,"MPI2","/");
#else
    join(build,"MPI","/");
#endif
#endif

#if defined DDI_LAPI
    join(build,"LAPI","/");
#endif

#if defined DDI_ARMCI
    join(build,"ARMCI","/");
#endif

#ifdef DDI_FILE_MPI
    join(build,"MPI-IO","/");
#endif

}

/*
   These functions should work on any POSIX,
   but they need to be tested to avoid breaking
   some exotic system.
*/

#ifdef DDI_BGL

/** print DDI runtime */
void DDI_Runtime_print(FILE *stream) {
#ifdef DDI_RUNTIME_PRINT_
    DDI_RUNTIME_PRINT_(stream);
#else
    DDI_POSIX_Runtime_print(stream);
#endif
}

/** print DDI POSIX runtime */
void DDI_POSIX_Runtime_print(FILE *stream) {
    int version, subversion;
    char build[DDI_MAX_BUILD];
    struct utsname uts;

    DDI_Get_version(&version, &subversion);
    DDI_Get_build(build);
    uname(&uts);

    fprintf(stream,"DDI %i.%i %s %s %s %s\n",
	    version, subversion, build, uts.sysname, uts.release, uts.machine);
}

#endif
