#ifndef BASE64_INT_H_INCLUDED
#define BASE64_INT_H_INCLUDED

#include "xmlrpc-c/c_util.h"

XMLRPC_DLLEXPORT
void
xmlrpc_base64Encode(const char * const chars,
                    char *       const base64);

#endif
