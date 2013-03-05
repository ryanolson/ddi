#ifndef DDI_UTIL_H
#define DDI_UTIL_H

#include <stdlib.h>

/** Join string from with strin to by string delim */
void join(char *to, const char *from, const char *delim);

/** Convert at most len-1 characters of Fortran string to
    NULL terminated C string */
void strf2c(char *cstr, char *fstr, size_t len);

#endif
