#ifndef XMLRPC_C_UTIL_INT_H_INCLUDED
#define XMLRPC_C_UTIL_INT_H_INCLUDED

/* This file contains facilities for use by Xmlrpc-c code, but not intended
   to be included in a user compilation.

   Names in here might conflict with other names in a user's compilation
   if included in a user compilation.

   The facilities may change in future releases.
*/

#include "util.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

/* When we deallocate a pointer in a struct, we often replace it with
** this and throw in a few assertions here and there. */
#define XMLRPC_BAD_POINTER ((void*) 0xDEADBEEF)

#endif
