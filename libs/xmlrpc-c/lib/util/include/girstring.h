#ifndef GIRSTRING_H_INCLUDED
#define GIRSTRING_H_INCLUDED

#include <string.h>

#include "xmlrpc_config.h"
#include "bool.h"

bool
stripcaseeq(const char * const comparand,
            const char * const comparator);

static __inline__ bool
streq(const char * const comparator,
      const char * const comparand) {

    return (strcmp(comparand, comparator) == 0);
}

static __inline__ bool
memeq(const void * const comparator,
      const void * const comparand,
      size_t       const size) {

    return (memcmp(comparator, comparand, size) == 0);
}

#define MEMEQ(a,b,c) (memcmp(a, b, c) == 0)

#define MEMSSET(a,b) (memset(a, b, sizeof(*a)))

#define MEMSCPY(a,b) (memcpy(a, b, sizeof(*a)))

#define MEMSZERO(a) (MEMSSET(a, 0))

static __inline__ const char *
sdup(const char * const input) {
    return (const char *) strdup(input);
}

/* Copy string pointed by B to array A with size checking.  */
#define STRSCPY(A,B) \
	(strncpy((A), (B), sizeof(A)), *((A)+sizeof(A)-1) = '\0')
#define STRSCMP(A,B) \
	(strncmp((A), (B), sizeof(A)))

/* Concatenate string B onto string in array A with size checking */
#define STRSCAT(A,B) \
    (strncat((A), (B), sizeof(A)-strlen(A)), *((A)+sizeof(A)-1) = '\0')

#endif
