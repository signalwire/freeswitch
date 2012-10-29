#ifndef CURLMULTI_H_INCLUDED
#define CURLMULTI_H_INCLUDED

#include "bool.h"
#include "xmlrpc-c/util.h"

#include "curltransaction.h"

typedef struct curlMulti curlMulti;

curlMulti *
curlMulti_create(void);

void
curlMulti_destroy(curlMulti * const curlMultiP);

void
curlMulti_perform(xmlrpc_env * const envP,
                  curlMulti *  const curlMultiP,
                  bool *       const immediateWorkToDoP,
                  int *        const runningHandlesP);

void
curlMulti_addHandle(xmlrpc_env *       const envP,
                    curlMulti *        const curlMultiP,
                    CURL *             const curlSessionP);

void
curlMulti_removeHandle(curlMulti *       const curlMultiP,
                       CURL *            const curlSessionP);

void
curlMulti_getMessage(curlMulti * const curlMultiP,
                     bool *      const endOfMessagesP,
                     CURLMsg *   const curlMsgP);

void
curlMulti_fdset(xmlrpc_env * const envP,
                curlMulti *  const curlMultiP,
                fd_set *     const readFdSetP,
                fd_set *     const writeFdSetP,
                fd_set *     const exceptFdSetP,
                int *        const maxFdP);

void
curlMulti_updateFdSet(curlMulti * const curlMultiP,
                      fd_set      const readFdSet,
                      fd_set      const writeFdSet,
                      fd_set      const exceptFdSet);

#endif
