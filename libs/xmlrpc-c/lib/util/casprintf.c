//#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "xmlrpc_config.h"  /* For HAVE_ASPRINTF, __inline__ */
#include "casprintf.h"



static __inline__ void
simpleVasprintf(char **      const retvalP,
                const char * const fmt,
                va_list            varargs) {
/*----------------------------------------------------------------------------
   This is a poor man's implementation of vasprintf(), of GNU fame.
-----------------------------------------------------------------------------*/
    size_t const initialSize = 4096;
    char * result;

    result = malloc(initialSize);
    if (result != NULL) {
        size_t bytesNeeded;
        bytesNeeded = XMLRPC_VSNPRINTF(result, initialSize, fmt, varargs);
        if (bytesNeeded > initialSize) {
            free(result);
            result = malloc(bytesNeeded);
            if (result != NULL)
                XMLRPC_VSNPRINTF(result, bytesNeeded, fmt, varargs);
        } else if (bytesNeeded == initialSize) {
            if (result[initialSize-1] != '\0') {
                /* This is one of those old systems where vsnprintf()
                   returns the number of bytes it used, instead of the
                   number that it needed, and it in fact needed more than
                   we gave it.  Rather than mess with this highly unlikely
                   case (old system and string > 4095 characters), we just
                   treat this like an out of memory failure.
                */
                free(result);
                result = NULL;
            }
        }
    }
    *retvalP = result;
}



const char * const strsol = "[Insufficient memory to build string]";



void
cvasprintf(const char ** const retvalP,
           const char *  const fmt,
           va_list             varargs) {

    char * string;

#if HAVE_ASPRINTF
    vasprintf(&string, fmt, varargs);
#else
    simpleVasprintf(&string, fmt, varargs);
#endif

    if (string == NULL)
        *retvalP = strsol;
    else
        *retvalP = string;
}



void GNU_PRINTF_ATTR(2,3)
casprintf(const char ** const retvalP, const char * const fmt, ...) {

    va_list varargs;  /* mysterious structure used by variable arg facility */

    va_start(varargs, fmt); /* start up the mysterious variable arg facility */

    cvasprintf(retvalP, fmt, varargs);

    va_end(varargs);
}



void
strfree(const char * const string) {

    if (string != strsol)
        free((void *)string);
}
