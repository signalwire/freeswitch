#ifndef SELECT_INT_H_INCLUDED
#define SELECT_INT_H_INCLUDED

#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <signal.h>

#include "xmlrpc-c/c_util.h"
#include "xmlrpc-c/time_int.h"
#ifdef WIN32
#ifndef sigset_t
typedef int sigset_t;
#endif
#endif

XMLRPC_DLLEXPORT
int
xmlrpc_pselect(int                     const n,
               fd_set *                const readfdsP,
               fd_set *                const writefdsP,
               fd_set *                const exceptfdsP,
               const xmlrpc_timespec * const timeoutP,
               sigset_t *              const sigmaskP);

#endif
