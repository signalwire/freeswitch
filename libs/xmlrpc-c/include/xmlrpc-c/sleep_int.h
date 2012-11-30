#ifndef SLEEP_INT_H_INCLUDED
#define SLEEP_INT_H_INCLUDED

#include "xmlrpc-c/c_util.h"

#ifdef __cplusplus
extern "C" {
#endif

XMLRPC_DLLEXPORT
void
xmlrpc_millisecond_sleep(unsigned int const milliseconds);

#ifdef __cplusplus
}
#endif

#endif
