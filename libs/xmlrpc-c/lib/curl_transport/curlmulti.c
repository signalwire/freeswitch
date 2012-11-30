/*=============================================================================
                               curlMulti
===============================================================================
   This is an extension to Curl's CURLM object.  The extensions are:

   1) It has a lock so multiple threads can use it simultaneously.

   2) Its "select" file descriptor vectors are self-contained.  CURLM
      requires the user to maintain them separately.
=============================================================================*/

#define _XOPEN_SOURCE 600  /* Make sure strdup() is in <string.h> */

#include "xmlrpc_config.h"

#include <stdlib.h>
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>
#include <curl/multi.h>

#include "mallocvar.h"
#include "xmlrpc-c/util.h"
#include "xmlrpc-c/string_int.h"

#include "curlversion.h"
#include "lock.h"
#include "lock_pthread.h"

#include "curlmulti.h"



static void
interpretCurlMultiError(const char ** const descriptionP,
                        CURLMcode     const code) {

#if HAVE_CURL_STRERROR
    *descriptionP = strdup(curl_multi_strerror(code));
#else
    xmlrpc_asprintf(descriptionP, "Curl error code (CURLMcode) %d", code);
#endif
}



struct curlMulti {
    CURLM * curlMultiP;
    lock * lockP;
        /* Hold this lock while accessing or using *curlMultiP.  You're
           using the multi manager whenever you're calling a Curl
           library multi manager function.
        */
    /* The following file descriptor sets are an integral part of the
       CURLM object; Our curlMulti_fdset() routine binds them to the
       CURLM object, and said object expects us to use them in a very
       specific way, including doing a select() on them.  It is very,
       very messy.
    */
    fd_set readFdSet;
    fd_set writeFdSet;
    fd_set exceptFdSet;
};



curlMulti *
curlMulti_create(void) {

    curlMulti * retval;
    curlMulti * curlMultiP;

    MALLOCVAR(curlMultiP);

    if (curlMultiP == NULL)
        retval = NULL;
    else {
        curlMultiP->lockP = curlLock_create_pthread();

        if (curlMultiP->lockP == NULL)
            retval = NULL;
        else {
            curlMultiP->curlMultiP = curl_multi_init();
            if (curlMultiP->curlMultiP == NULL)
                retval = NULL;
            else
                retval = curlMultiP;

            if (retval == NULL)
                curlMultiP->lockP->destroy(curlMultiP->lockP);
        }
        if (retval == NULL)
            free(curlMultiP);
    }
    return retval;
}



void
curlMulti_destroy(curlMulti * const curlMultiP) {

    curl_multi_cleanup(curlMultiP->curlMultiP);
    
    curlMultiP->lockP->destroy(curlMultiP->lockP);

    free(curlMultiP);
}



void
curlMulti_perform(xmlrpc_env * const envP,
                  curlMulti *  const curlMultiP,
                  bool *       const immediateWorkToDoP,
                  int *        const runningHandlesP) {
/*----------------------------------------------------------------------------
   Do whatever work is ready to be done under the control of multi
   manager 'curlMultiP'.  E.g. if HTTP response data has recently arrived
   from the network, process it as an HTTP response.

   Iff this results in some work being finished from our point of view,
   return *immediateWorkToDoP.  (Caller can query the multi manager for
   messages and find out what it is).

   Return as *runningHandlesP the number of Curl easy handles under the
   multi manager's control that are still running -- yet to finish.
-----------------------------------------------------------------------------*/
    CURLMcode rc;

    curlMultiP->lockP->acquire(curlMultiP->lockP);

    rc = curl_multi_perform(curlMultiP->curlMultiP, runningHandlesP);

    curlMultiP->lockP->release(curlMultiP->lockP);

    if (rc == CURLM_CALL_MULTI_PERFORM) {
        *immediateWorkToDoP = true;
    } else {
        *immediateWorkToDoP = false;

        if (rc != CURLM_OK) {
            const char * reason;
            interpretCurlMultiError(&reason, rc);
            xmlrpc_faultf(envP, "Impossible failure of curl_multi_perform(): "
                          "%s", reason);
            xmlrpc_strfree(reason);
        }
    }
}        



void
curlMulti_addHandle(xmlrpc_env *       const envP,
                    curlMulti *        const curlMultiP,
                    CURL *             const curlSessionP) {

    CURLMcode rc;

    curlMultiP->lockP->acquire(curlMultiP->lockP);

    rc = curl_multi_add_handle(curlMultiP->curlMultiP, curlSessionP);
    
    curlMultiP->lockP->release(curlMultiP->lockP);

    if (rc != CURLM_OK) {
        const char * reason;
        interpretCurlMultiError(&reason, rc);
        xmlrpc_faultf(envP, "Could not add Curl session to the "
                      "curl multi manager.  curl_multi_add_handle() "
                      "failed: %s", reason);
        xmlrpc_strfree(reason);
    }
}


void
curlMulti_removeHandle(curlMulti *       const curlMultiP,
                       CURL *            const curlSessionP) {

    curlMultiP->lockP->acquire(curlMultiP->lockP);

    curl_multi_remove_handle(curlMultiP->curlMultiP, curlSessionP);
    
    curlMultiP->lockP->release(curlMultiP->lockP);
}



void
curlMulti_getMessage(curlMulti * const curlMultiP,
                     bool *      const endOfMessagesP,
                     CURLMsg *   const curlMsgP) {
/*----------------------------------------------------------------------------
   Get the next message from the queue of things the Curl multi manager
   wants to say to us.

   Return the message as *curlMsgP.

   Iff there are no messages in the queue, return *endOfMessagesP == true.
-----------------------------------------------------------------------------*/
    int remainingMsgCount;
    CURLMsg * privateCurlMsgP;
        /* Note that this is a pointer into the multi manager's memory,
           so we have to use it under lock.
        */

    curlMultiP->lockP->acquire(curlMultiP->lockP);
    
    privateCurlMsgP = curl_multi_info_read(curlMultiP->curlMultiP,
                                           &remainingMsgCount);
        
    if (privateCurlMsgP == NULL)
        *endOfMessagesP = true;
    else {
        *endOfMessagesP = false;
        *curlMsgP = *privateCurlMsgP;
    }    
    curlMultiP->lockP->release(curlMultiP->lockP);
}



void
curlMulti_fdset(xmlrpc_env * const envP,
                curlMulti *  const curlMultiP,
                fd_set *     const readFdSetP,
                fd_set *     const writeFdSetP,
                fd_set *     const exceptFdSetP,
                int *        const maxFdP) {
/*----------------------------------------------------------------------------
   Set the CURLM object's file descriptor sets to those in the
   curlMulti object, update those file descriptor sets with the
   current needs of the multi manager, and return the resulting values
   of the file descriptor sets.

   This is a bizarre operation, but is necessary because of the nonmodular
   way in which the Curl multi interface works with respect to waiting
   for work with select().
-----------------------------------------------------------------------------*/
    CURLMcode rc;
    
    curlMultiP->lockP->acquire(curlMultiP->lockP);

    /* curl_multi_fdset() doesn't _set_ the fdsets.  It adds to existing
       ones (so you can easily do a select() on other fds and Curl
       fds at the same time).  So we have to clear first:
    */
    FD_ZERO(&curlMultiP->readFdSet);
    FD_ZERO(&curlMultiP->writeFdSet);
    FD_ZERO(&curlMultiP->exceptFdSet);

    /* WARNING: curl_multi_fdset() doesn't just update the fdsets pointed
       to by its arguments.  It makes the CURLM object remember those
       pointers and refer back to them later!  In fact, curl_multi_perform
       expects its caller to have done a select() on those masks.  No,
       really.  The man page even admits it.

       Inspection of the Libcurl code in March 2007 indicates that
       this isn't actually true -- curl_multi_fdset() updates your
       fdset and doesn't remember the pointer at all.  I.e. it's just
       what you would expect.  The man pages still says it's as
       described above.  My guess is that Libcurl was fixed at some
       time and the man page not updated.  In any case, we have to
       work with old Libcurl if at all possible, so we still maintain
       these fdsets as if they belong to the CURLM object.
    */

    rc = curl_multi_fdset(curlMultiP->curlMultiP,
                          &curlMultiP->readFdSet,
                          &curlMultiP->writeFdSet,
                          &curlMultiP->exceptFdSet,
                          maxFdP);

    *readFdSetP   = curlMultiP->readFdSet;
    *writeFdSetP  = curlMultiP->writeFdSet;
    *exceptFdSetP = curlMultiP->exceptFdSet;

    curlMultiP->lockP->release(curlMultiP->lockP);

    if (rc != CURLM_OK) {
        const char * reason;
        interpretCurlMultiError(&reason, rc);
        xmlrpc_faultf(envP, "Impossible failure of curl_multi_fdset(): %s",
                      reason);
        xmlrpc_strfree(reason);
    }
}



void
curlMulti_updateFdSet(curlMulti * const curlMultiP,
                      fd_set      const readFdSet,
                      fd_set      const writeFdSet,
                      fd_set      const exceptFdSet) {
/*----------------------------------------------------------------------------
   curl_multi_perform() expects the file descriptor sets, which were bound
   to the CURLM object via a prior curlMulti_fdset(), to contain the results
   of a recent select().  This subroutine provides you a way to supply those.
-----------------------------------------------------------------------------*/
    curlMultiP->readFdSet   = readFdSet;
    curlMultiP->writeFdSet  = writeFdSet;
    curlMultiP->exceptFdSet = exceptFdSet;
}

                      

