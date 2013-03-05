#include "ddi_bgl.h"
#include "ddi_runtime.h"

#include <rts.h>
#include <bglpersonality.h>

/** Rank in I/O  */
int DDI_BGL_File_IO_rank(DDI_Comm *comm) {
    BGLPersonality personality;

    rts_get_personality(&personality, sizeof(personality));
    return BGLPersonality_rankInPset(&personality);
}

/** Rank of I/O node */
int DDI_BGL_File_IONode_rank(DDI_Comm *comm) {
    BGLPersonality personality;
    
    rts_get_personality(&personality, sizeof(personality));
    return BGLPersonality_psetNum(&personality);
}

/** print DDI Blue Gene/L runtime */
void DDI_BGL_Runtime_print(FILE *stream) {
    BGLPersonality personality;
    int rank,nprocs;
    int dim = 0,torus_dim = 0;
    char topology[] = "torus";
    char *topology_axis, mesh_axis[] = "XYZ", torus_axis[] = "XYZ";
    char *bglmpi_eager = NULL;
    char *bglmpi_mapping = NULL;
    char *bglmpi_pacing = NULL;
    
    rts_get_personality(&personality, sizeof(personality));
    MPI_Comm_size(MPI_COMM_WORLD,&nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);

    /* determine mesh */
    dim = 0;
    strcpy(topology,"mesh");
    strcpy(mesh_axis,"");
    if (BGLPersonality_xSize(&personality) > 1) { ++dim; strcat(mesh_axis,"X"); }
    if (BGLPersonality_ySize(&personality) > 1) { ++dim; strcat(mesh_axis,"Y"); }
    if (BGLPersonality_zSize(&personality) > 1) { ++dim; strcat(mesh_axis,"Z"); }
    if (dim == 0) { dim = 1; strcpy(mesh_axis,"X"); }
    topology_axis = mesh_axis;
    
    /* determine torus */
    torus_dim = 0;
    strcpy(torus_axis,"");
    if (BGLPersonality_isTorusX(&personality)) { ++torus_dim; strcat(torus_axis,"X"); }
    if (BGLPersonality_isTorusY(&personality)) { ++torus_dim; strcat(torus_axis,"Y"); }
    if (BGLPersonality_isTorusZ(&personality)) { ++torus_dim; strcat(torus_axis,"Z"); }
    if (torus_dim > 0) { dim = torus_dim; strcpy(topology,"torus"); topology_axis = torus_axis; }

    /* determine BGLMPI_MAPPING */
    bglmpi_eager = getenv("BGLMPI_EAGER");
    bglmpi_mapping = getenv("BGLMPI_MAPPING");
    bglmpi_pacing = getenv("BGLMPI_PACING");
    
    /* print DDI Posix runtime */
    DDI_POSIX_Runtime_print(stream);

    /* print DDI Blue Gene/L runtime */
    fprintf(stream,"%i compute nodes, %s mode, %i I/O nodes\n",
	    BGLPersonality_numComputeNodes(&personality),
	    BGLPersonality_virtualNodeMode(&personality) ? "VN" : "CO",
	    BGLPersonality_numIONodes(&personality));
    fprintf(stream,"%i-D %s(%s) <%i,%i,%i>\n",
	    dim,topology,topology_axis,
	    BGLPersonality_xSize(&personality),
	    BGLPersonality_ySize(&personality),
	    BGLPersonality_zSize(&personality));
    if (bglmpi_eager) fprintf(stream,"BGLMPI_EAGER=%s\n", bglmpi_eager);
    if (bglmpi_mapping) fprintf(stream,"BGLMPI_MAPPING=%s\n", bglmpi_mapping);
    if (bglmpi_pacing) fprintf(stream,"BGLMPI_PACING=%s\n", bglmpi_pacing);
    fprintf(stream,"MPI %i/%i <%i,%i,%i> %iMHz %iMB\n",
	    rank,nprocs,
	    BGLPersonality_xCoord(&personality),
	    BGLPersonality_yCoord(&personality),
	    BGLPersonality_zCoord(&personality),
	    BGLPersonality_clockHz(&personality)/1000000,
	    BGLPersonality_DDRSize(&personality)/(1024*1024));

}
