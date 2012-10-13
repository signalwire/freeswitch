#ifndef XMLRPC_C_STRING_INT_H_INCLUDED
#define XMLRPC_C_STRING_INT_H_INCLUDED


#include <stdarg.h>
#include <string.h>

#include "xmlrpc_config.h"
#include "c_util.h"
#include "bool.h"

#ifdef __cplusplus
extern "C" {
#endif

XMLRPC_DLLEXPORT
bool
xmlrpc_strnomem(const char * const string);

XMLRPC_DLLEXPORT
const char *
xmlrpc_strnomemval(void);

XMLRPC_DLLEXPORT
void
xmlrpc_vasprintf(const char ** const retvalP,
                 const char *  const fmt,
                 va_list             varargs);

XMLRPC_DLLEXPORT
void XMLRPC_PRINTF_ATTR(2,3)
xmlrpc_asprintf(const char ** const retvalP, const char * const fmt, ...);

XMLRPC_DLLEXPORT
const char *
xmlrpc_strdupsol(const char * const string);

XMLRPC_DLLEXPORT
void
xmlrpc_strfree(const char * const string);

XMLRPC_DLLEXPORT
const char *
xmlrpc_strdupnull(const char * const string);

XMLRPC_DLLEXPORT
void
xmlrpc_strfreenull(const char * const string);

static __inline__ bool
xmlrpc_streq(const char * const a,
             const char * const b) {
    return (strcmp(a, b) == 0);
}

static __inline__ bool
xmlrpc_memeq(const void * const a,
             const void * const b,
             size_t       const size) {

    return (memcmp(a, b, size) == 0);
}

/* strcasecmp doesn't exist on some systems without _BSD_SOURCE, so
   xmlrpc_strcaseeq() can't either.
*/
#ifdef _BSD_SOURCE

static __inline__ bool
xmlrpc_strcaseeq(const char * const a,
                 const char * const b) {
#if HAVE_STRCASECMP
    return (strcasecmp(a, b) == 0);
#elif HAVE__STRICMP
    return (_stricmp(a, b) == 0);
#elif HAVE_STRICMP
    return (stricmp(a, b) == 0);
#else
    #error "This platform has no known case-independent string compare fn"
#endif
}
#endif

static __inline__ bool
xmlrpc_strneq(const char * const a,
              const char * const b,
              size_t       const len) {
    return (strncmp(a, b, len) == 0);
}

XMLRPC_DLLEXPORT
const char * 
xmlrpc_makePrintable(const char * const input);

XMLRPC_DLLEXPORT
const char *
xmlrpc_makePrintable_lp(const char * const input,
                        size_t       const inputLength);

XMLRPC_DLLEXPORT
const char *
xmlrpc_makePrintableChar(char const input);

/*----------------------------------------------------------------*/
/* Standard string functions with destination array size checking */
/*----------------------------------------------------------------*/
#define STRSCPY(A,B) \
	(strncpy((A), (B), sizeof(A)), *((A)+sizeof(A)-1) = '\0')
#define STRSCMP(A,B) \
	(strncmp((A), (B), sizeof(A)))
#define STRSCAT(A,B) \
    (strncat((A), (B), sizeof(A)-strlen(A)), *((A)+sizeof(A)-1) = '\0')

/* We could do this, but it works only in GNU C 
#define SSPRINTF(TARGET, REST...) \
  (snprintf(TARGET, sizeof(TARGET) , ## REST)) 

Or this, but it works only in C99 compilers, which leaves out MSVC
before 2005 and can't handle the zero variable argument case except
by an MSVC extension:

#define SSPRINTF(TARGET, ...) \
  (snprintf(TARGET, sizeof(TARGET) , __VA_ARGS__)) 

*/

#ifdef __cplusplus
}
#endif

#endif
