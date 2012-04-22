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

extern const char * const xmlrpc_strsol;

void
xmlrpc_vasprintf(const char ** const retvalP,
                 const char *  const fmt,
                 va_list             varargs);

void GNU_PRINTF_ATTR(2,3)
xmlrpc_asprintf(const char ** const retvalP, const char * const fmt, ...);

const char *
xmlrpc_strdupnull(const char * const string);

void
xmlrpc_strfree(const char * const string);

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

static __inline__ bool
xmlrpc_strneq(const char * const a,
              const char * const b,
              size_t       const len) {
    return (strncmp(a, b, len) == 0);
}

const char * 
xmlrpc_makePrintable(const char * const input);

const char *
xmlrpc_makePrintable_lp(const char * const input,
                        size_t       const inputLength);

const char *
xmlrpc_makePrintableChar(char const input);

#ifdef __cplusplus
}
#endif

#endif
