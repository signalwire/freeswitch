#define _XOPEN_SOURCE 600  /* Get pselect() in <sys/select.h> */

#ifdef WIN32
#include <winsock.h>
#else
/* In some systems (SUS), the select() interface comes from <sys/time.h>;
   in others, from <sys/select.h>, and other from both.  Including both
   in this order appears to work on all.
*/
#include <sys/time.h>
#include <sys/select.h>
#endif 
#include <signal.h>

#include "xmlrpc_config.h"

#include "xmlrpc-c/select_int.h"


/* xmlrpc_pselect() is just for use with sockets.  In a POSIX system,
   it technically works for any file descriptor, but in Windows, select()
   is part of the socket facility.
*/

int
xmlrpc_pselect(int                     const n,
               fd_set *                const readfdsP,
               fd_set *                const writefdsP,
               fd_set *                const exceptfdsP,
               const xmlrpc_timespec * const timeoutP,
               sigset_t *              const sigmaskP) {

    int retval;

#if HAVE_PSELECT
#if !HAVE_TIMESPEC
  #error "Impossible configuration -- has pselect(), but not struct timespec"
#else
    retval = pselect(n, readfdsP, writefdsP, exceptfdsP, timeoutP, sigmaskP);
#endif
#else /* HAVE_PSELECT */
    struct timeval timeout;
    
    timeout.tv_sec  = timeoutP->tv_sec;
    timeout.tv_usec = timeoutP->tv_nsec/1000;
#ifdef WIN32
    retval = select(n, readfdsP, writefdsP, exceptfdsP, &timeout);
#else
    {
       sigset_t origmask;
       sigprocmask(SIG_SETMASK, sigmaskP, &origmask);
       retval = select(n, readfdsP, writefdsP, exceptfdsP, &timeout);
       sigprocmask(SIG_SETMASK, &origmask, NULL);
    }
#endif
#endif

    return retval;
}
