#ifndef REGISTRY_H_INCLUDED
#define REGISTRY_H_INCLUDED

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/server.h"

void
xmlrpc_dispatchCall(struct _xmlrpc_env *     const envP, 
                    struct xmlrpc_registry * const registryP,
                    const char *             const methodName, 
                    struct _xmlrpc_value *   const paramArrayP,
                    void *                   const callInfoP,
                    struct _xmlrpc_value **  const resultPP);

#endif
