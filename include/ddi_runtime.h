#ifndef DDI_RUNTIME_H
#define DDI_RUNTIME_H

/* maximum length of build string */
#define DDI_MAX_BUILD 256

/* DDI version */
void DDI_Get_version(int *version, int *subversion);

/* DDI build */
void DDI_Get_build(char *build);

/* print DDI runtime */
void DDI_Runtime_print(FILE *stream);

/* print DDI POSIX runtime */
void DDI_POSIX_Runtime_print(FILE *stream);

#endif
