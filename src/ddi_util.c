#include "ddi_util.h"
#include <string.h>
#include <stdlib.h>

/** Join string from with strin to by string delim */
void join(char *to, const char *from, const char *delim) {
    if (strlen(to) > 0) strcat(to,delim);
    strcat(to,from);
}

/** Convert at most len-1 characters of Fortran string to
    NULL terminated C string */
void strf2c(char *cstr, char *fstr, size_t len) {
    strncpy(cstr, fstr, len-1);
    cstr[len-1] = '\0';
}
