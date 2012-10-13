/* These are some dynamic memory allocation facilities.  They are essentially
   an extension to C, as they do allocations with a cognizance of C 
   variables.  You can use them to make C read more like a high level
   language.

   Before including this, you must define an __inline__ macro if your
   compiler doesn't recognize it as a keyword.
*/

#ifndef MALLOCVAR_INCLUDED
#define MALLOCVAR_INCLUDED

#include "xmlrpc_config.h"

#include <limits.h>
#include <stdlib.h>

static __inline__ void
mallocProduct(void **      const resultP, 
              unsigned int const factor1,
              size_t       const factor2) {
/*----------------------------------------------------------------------------
   malloc a space whose size in bytes is the product of 'factor1' and
   'factor2'.  But if that size cannot be represented as an unsigned int,
   return NULL without allocating anything.  Also return NULL if the malloc
   fails.

   If either factor is zero, malloc a single byte.

   Note that malloc() actually takes a size_t size argument, so the
   proper test would be whether the size can be represented by size_t,
   not unsigned int.  But there is no reliable indication available to
   us, like UINT_MAX, of what the limitations of size_t are.  We
   assume size_t is at least as expressive as unsigned int and that
   nobody really needs to allocate more than 4GB of memory.
-----------------------------------------------------------------------------*/
    if (factor1 == 0 || factor2 == 0)
        *resultP = malloc(1);
    else {
        if (UINT_MAX / factor2 < factor1) 
            *resultP = NULL;
        else 
            *resultP = malloc(factor1 * factor2); 
    }
}



static __inline__ void
reallocProduct(void **      const blockP,
               unsigned int const factor1,
               unsigned int const factor2) {

    void * const oldBlockP = *blockP;

    void * newBlockP;
    
    if (UINT_MAX / factor2 < factor1) 
        newBlockP = NULL;
    else 
        newBlockP = realloc(oldBlockP, factor1 * factor2); 

    if (newBlockP)
        *blockP = newBlockP;
    else {
        free(oldBlockP);
        *blockP = NULL;
    }
}


/* IMPLEMENTATION NOTE:  There are huge strict aliasing pitfalls here
   if you cast pointers, e.g. (void **)
*/

#define MALLOCARRAY(arrayName, nElements) do { \
    void * array; \
    mallocProduct(&array, nElements, sizeof(arrayName[0])); \
    arrayName = array; \
} while (0)

#define REALLOCARRAY(arrayName, nElements) do { \
    void * array = arrayName; \
    reallocProduct(&array, nElements, sizeof(arrayName[0])); \
    arrayName = array; \
} while (0)


#define MALLOCARRAY_NOFAIL(arrayName, nElements) \
do { \
    MALLOCARRAY(arrayName, nElements); \
    if ((arrayName) == NULL) \
        abort(); \
} while(0)

#define REALLOCARRAY_NOFAIL(arrayName, nElements) \
do { \
    REALLOCARRAY(arrayName, nElements); \
    if ((arrayName) == NULL) \
        abort(); \
} while(0)


#define MALLOCVAR(varName) \
    varName = malloc(sizeof(*varName))

#define MALLOCVAR_NOFAIL(varName) \
    do {if ((varName = malloc(sizeof(*varName))) == NULL) abort();} while(0)

#endif

