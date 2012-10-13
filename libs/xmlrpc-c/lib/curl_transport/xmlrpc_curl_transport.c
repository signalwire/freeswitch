/*=============================================================================
                           xmlrpc_curl_transport
===============================================================================
   Curl-based client transport for Xmlrpc-c

   By Bryan Henderson 04.12.10.

   Contributed to the public domain by its author.
=============================================================================*/

/*----------------------------------------------------------------------------
   Curl global variables:

   Curl maintains some minor information in process-global variables.
   One must call curl_global_init() to initialize them before calling
   any other Curl library function.  This is not state information --
   it is constants.  They just aren't the kind of constants that the
   library loader knows how to set, so there has to be this explicit
   call to set them up.  The matching function curl_global_cleanup()
   returns resources these use (to wit, the constants live in
   malloc'ed storage and curl_global_cleanup() frees the storage).

   So our setup_global_const transport operation calls
   curl_global_init() and our teardown_global_const calls
   curl_global_cleanup().

   The Curl library is supposed to maintain a reference count for the
   global constants so that multiple modules using the library and
   independently calling curl_global_init() and curl_global_cleanup()
   are not a problem.  But today, it just keeps a flag "I have been
   initialized" and the first call to curl_global_cleanup() destroys
   the constants for everybody.  Therefore, the user of the Xmlrpc-c
   Curl client XML transport must make sure not to call
   teardownGlobalConstants until everything else in his program is
   done using the Curl library.

   Note that curl_global_init() is not threadsafe (with or without the
   reference count), therefore our setup_global_const is not, and must
   be called when no other thread in the process is running.
   Typically, one calls it right at the beginning of the program.

   There are actually two other classes of global variables in the
   Curl library, which we are ignoring: debug options and custom
   memory allocator function identities.  Our code never changes these
   global variables from default.  If something else in the user's
   program does, User is responsible for making sure it doesn't
   interfere with our use of the library.

   Note that when we say what the Curl library does, we're also
   talking about various other libraries Curl uses internally, and in
   fact much of what we're saying about global variables springs from
   such subordinate libraries as OpenSSL and Winsock.
-----------------------------------------------------------------------------*/

#define _XOPEN_SOURCE 600  /* Make sure strdup() is in <string.h> */

#include "xmlrpc_config.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <signal.h>

#ifdef WIN32
#include "curllink.h"
#endif

#include "bool.h"
#include "girmath.h"
#include "mallocvar.h"
#include "linklist.h"
#include "girstring.h"
#include "pthreadx.h"

#include "xmlrpc-c/util.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/select_int.h"
#include "xmlrpc-c/client_int.h"
#include "xmlrpc-c/transport.h"
#include "xmlrpc-c/time_int.h"

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>
#include <curl/multi.h>

#include "lock.h"
#include "lock_pthread.h"
#include "curltransaction.h"
#include "curlmulti.h"
#include "curlversion.h"

#if MSVCRT
#if defined(_DEBUG)
#  include <crtdbg.h>
#  define new DEBUG_NEW
#  define malloc(size) _malloc_dbg( size, _NORMAL_BLOCK, __FILE__, __LINE__)
#  undef THIS_FILE
   static char THIS_FILE[] = __FILE__;
#endif
#endif


typedef struct rpc rpc;



static int
timeDiffMillisec(xmlrpc_timespec const minuend,
                 xmlrpc_timespec const subtractor) {

    unsigned int const million = 1000000;

    return (minuend.tv_sec - subtractor.tv_sec) * 1000 +
        (minuend.tv_nsec - subtractor.tv_nsec + million/2) / million;
}



static bool
timeIsAfter(xmlrpc_timespec const comparator,
            xmlrpc_timespec const comparand) {

    if (comparator.tv_sec > comparand.tv_sec)
        return true;
    else if (comparator.tv_sec < comparand.tv_sec)
        return false;
    else {
        /* Seconds are equal */
        if (comparator.tv_nsec > comparand.tv_nsec)
            return true;
        else
            return false;
    }
}



static void
addMilliseconds(xmlrpc_timespec   const addend,
                unsigned int      const adder,
                xmlrpc_timespec * const sumP) {
    
    unsigned int const million = 1000000;
    unsigned int const billion = 1000000000;

    xmlrpc_timespec sum;

    sum.tv_sec  = addend.tv_sec + adder / 1000;
    sum.tv_nsec = addend.tv_nsec + (adder % 1000) * million;

    if ((uint32_t)sum.tv_nsec >= billion) {
        sum.tv_sec += 1;
        sum.tv_nsec -= billion;
    }
    *sumP = sum;
}



struct xmlrpc_client_transport {
    CURL * syncCurlSessionP;
        /* Handle for a Curl library session object that we use for
           all synchronous RPCs.  An async RPC has one of its own,
           and consequently does not share things such as persistent
           connections and cookies with any other RPC.
        */
    lock * syncCurlSessionLockP;
        /* Hold this lock while accessing or using *syncCurlSessionP.
           You're using the session from the time you set any
           attributes in it or start a transaction with it until any
           transaction has finished and you've lost interest in any
           attributes of the session.
        */
    curlMulti * syncCurlMultiP;
        /* The Curl multi manager that this transport uses to execute
           Curl transactions for RPCs requested via the synchronous
           interface.  The fact that there is never more than one such
           transaction going at a time might make you wonder why a
           "multi" manager is needed.  The reason is that it is the only
           interface in libcurl that gives us the flexibility to execute
           the transaction with proper interruptibility.  The only Curl
           transaction ever attached to this multi manager is
           'syncCurlSessionP'.
           
           This is constant (the handle, not the object).
        */
    curlMulti * asyncCurlMultiP;
        /* The Curl multi manager that this transport uses to execute
           Curl transactions for RPCs requested via the asynchronous
           interface.  Note that there may be multiple such Curl transactions
           simultaneously and one can't wait for a particular one to finish;
           the collection of asynchronous RPCs are an indivisible mass.
           
           This is constant (the handle, not the object).
        */
    bool dontAdvertise;
        /* Don't identify to the server the XML-RPC engine we are using.  If
           false, include a User-Agent HTTP header in all requests that
           identifies the Xmlrpc-c and Curl libraries.

           See also 'userAgent'.

           This is constant.
        */
    const char * userAgent;
        /* Information to include in a User-Agent HTTP header, reflecting
           facilities outside of Xmlrpc-c.  

           Null means none.

           The full User-Agent header value is this information (if
           'userAgent' is non-null) followed by identification of Xmlrpc-c
           and Curl (if 'dontAdvertise' is false).  If 'userAgent' is null
           and 'dontAdvertise' is true, we put no User-Agent header at all
           in the request.

           This is constant.
        */
    struct curlSetup curlSetupStuff;
        /* This is constant */
    int * interruptP;
        /* Pointer to a value that user sets to nonzero to indicate he wants
           the transport to give up on whatever it is doing and return ASAP.

           NULL means none -- transport never gives up.

           This is constant.
        */
};



struct rpc {
    struct xmlrpc_client_transport * transportP;
        /* The client XML transport that transports this RPC */
    curlTransaction * curlTransactionP;
        /* The object which does the HTTP transaction, with no knowledge
           of XML-RPC or Xmlrpc-c.
        */
    CURL * curlSessionP;
        /* The Curl session to use for the Curl transaction to perform
           the RPC.
        */
    xmlrpc_mem_block * responseXmlP;
        /* Where the response XML for this RPC should go or has gone. */
    xmlrpc_transport_asynch_complete complete;
        /* Routine to call to complete the RPC after it is complete HTTP-wise.
           NULL if none.
        */
    xmlrpc_transport_progress progress;
        /* Routine to call periodically to report the progress of transporting
           the call and response.  NULL if none.
        */
    struct xmlrpc_call_info * callInfoP;
        /* User's identifier for this RPC */
};


static void
lockSyncCurlSession(struct xmlrpc_client_transport * const transportP) {
    transportP->syncCurlSessionLockP->acquire(
        transportP->syncCurlSessionLockP);
}



static void
unlockSyncCurlSession(struct xmlrpc_client_transport * const transportP) {
    transportP->syncCurlSessionLockP->release(
        transportP->syncCurlSessionLockP);
}



static void
initWindowsStuff(xmlrpc_env * const envP ATTR_UNUSED) {

#if defined (WIN32)
    /* This is CRITICAL so that cURL-Win32 works properly! */
    
    /* So this commenter says, but I wonder why.  libcurl should do the
       required WSAStartup() itself, and it looks to me like it does.
       -Bryan 06.01.01
    */
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;
    wVersionRequested = MAKEWORD(1, 1);
    
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_INTERNAL_ERROR,
            "Winsock startup failed.  WSAStartup returned rc %d", err);
    else {
        if (LOBYTE(wsaData.wVersion) != 1 || HIBYTE(wsaData.wVersion) != 1) {
            /* Tell the user that we couldn't find a useable */ 
            /* winsock.dll. */ 
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INTERNAL_ERROR, "Winsock reported that "
                "it does not implement the requested version 1.1.");
        }
        if (envP->fault_occurred)
            WSACleanup();
    }
#endif
}



static void
termWindowsStuff(void) {

#if defined (WIN32)
    WSACleanup();
#endif
}



static bool
curlHasNosignal(void) {

    bool retval;

#if HAVE_CURL_NOSIGNAL
    curl_version_info_data * const curlInfoP =
        curl_version_info(CURLVERSION_NOW);

    retval = (curlInfoP->version_num >= 0x070A00);  /* 7.10.0 */
#else
    retval = false;
#endif
    return retval;
}



static xmlrpc_timespec
pselectTimeout(xmlrpc_timeoutType const timeoutType,
               xmlrpc_timespec    const timeoutDt) {
/*----------------------------------------------------------------------------
   Return the value that should be used in the select() call to wait for
   there to be work for the Curl multi manager to do, given that the user
   wants to timeout according to 'timeoutType' and 'timeoutDt'.
-----------------------------------------------------------------------------*/
    unsigned int const million = 1000000;
    unsigned int selectTimeoutMillisec;
    xmlrpc_timespec retval;

    /* We assume there is work to do at least every 3 seconds, because
       the Curl multi manager often has retries and other scheduled work
       that doesn't involve file handles on which we can select().
    */
    switch (timeoutType) {
    case timeout_no:
        selectTimeoutMillisec = 3000;
        break;
    case timeout_yes: {
        xmlrpc_timespec nowTime;
        int timeLeft;

        xmlrpc_gettimeofday(&nowTime);
        timeLeft = timeDiffMillisec(timeoutDt, nowTime);

        selectTimeoutMillisec = MIN(3000, MAX(0, timeLeft));
    } break;
    }
    retval.tv_sec = selectTimeoutMillisec / 1000;
    retval.tv_nsec = (uint32_t)((selectTimeoutMillisec % 1000) * million);

    return retval;
}        



static void
processCurlMessages(xmlrpc_env * const envP,
                    curlMulti *  const curlMultiP) {
        
    bool endOfMessages;

    endOfMessages = false;   /* initial assumption */

    while (!endOfMessages && !envP->fault_occurred) {
        CURLMsg curlMsg;

        curlMulti_getMessage(curlMultiP, &endOfMessages, &curlMsg);

        if (!endOfMessages) {
            if (curlMsg.msg == CURLMSG_DONE) {
                curlTransaction * curlTransactionP;

                curl_easy_getinfo(curlMsg.easy_handle, CURLINFO_PRIVATE,
                                  (void *)&curlTransactionP);

                curlTransaction_finish(envP,
                                       curlTransactionP, curlMsg.data.result);
            }
        }
    }
}



static void
waitForWork(xmlrpc_env *       const envP,
            curlMulti *        const curlMultiP,
            xmlrpc_timeoutType const timeoutType,
            xmlrpc_timespec    const deadline,
            sigset_t *         const sigmaskP) {
/*----------------------------------------------------------------------------
   Wait for the Curl multi manager to have work to do, time to run out,
   or a signal to be received (and caught), whichever comes first.

   Update the Curl multi manager's file descriptor sets to indicate what
   work we found for it to do.

   Wait under signal mask *sigmaskP.  The point of this is that Caller
   can make sure that arrival of a signal of a certain class
   interrupts our wait, even if the signal arrives shortly before we
   begin waiting.  Caller blocks that signal class, then checks
   whether a signal of that class has already been received.  If not,
   he calls us with *sigmaskP indicating that class NOT blocked.
   Thus, if a signal of that class arrived any time after Caller
   checked, we will return immediately or when the signal arrives,
   whichever is sooner.  Note that we can provide this service only
   because pselect() has the same atomic unblock/wait feature.
   
   If sigmaskP is NULL, wait under whatever the current signal mask
   is.
-----------------------------------------------------------------------------*/
    fd_set readFdSet;
    fd_set writeFdSet;
    fd_set exceptFdSet;
    int maxFd;

    curlMulti_fdset(envP, curlMultiP,
                    &readFdSet, &writeFdSet, &exceptFdSet, &maxFd);
    if (!envP->fault_occurred) {
        if (maxFd == -1) {
            /* There are no Curl file descriptors on which to wait.
               So either there's work to do right now or all transactions
               are already complete.
            */
        } else {
            xmlrpc_timespec const pselectTimeoutArg =
                pselectTimeout(timeoutType, deadline);

            int rc;

            rc = xmlrpc_pselect(maxFd+1, &readFdSet, &writeFdSet, &exceptFdSet,
                                &pselectTimeoutArg, sigmaskP);
            
            if (rc < 0 && errno != EINTR)
                xmlrpc_faultf(envP, "Impossible failure of pselect() "
                              "with errno %d (%s)",
                              errno, strerror(errno));
            else {
                /* Believe it or not, the Curl multi manager needs the
                   results of our pselect().  So hand them over:
                */
                curlMulti_updateFdSet(curlMultiP,
                                      readFdSet, writeFdSet, exceptFdSet);
            }
        }
    }
}



static void
waitForWorkInt(xmlrpc_env *       const envP,
               curlMulti *        const curlMultiP,
               xmlrpc_timeoutType const timeoutType,
               xmlrpc_timespec    const deadline,
               int *              const interruptP) {
/*----------------------------------------------------------------------------
   Same as waitForWork(), except we guarantee to return if a signal handler
   sets or has set *interruptP, whereas waitForWork() can miss a signal
   that happens before or just after it starts.

   We mess with global state -- the signal mask -- so we might mess up
   a multithreaded program.  Therefore, don't call this if
   waitForWork() will suffice.
-----------------------------------------------------------------------------*/
    sigset_t callerBlockSet;
#ifdef WIN32
    waitForWork(envP, curlMultiP, timeoutType, deadline, &callerBlockSet);
#else
    sigset_t allSignals;

    assert(interruptP != NULL);

    sigfillset(&allSignals);

    sigprocmask(SIG_BLOCK, &allSignals, &callerBlockSet);
    
    if (*interruptP == 0)
        waitForWork(envP, curlMultiP, timeoutType, deadline, &callerBlockSet);

    sigprocmask(SIG_SETMASK, &callerBlockSet, NULL);
#endif
}



static void
doCurlWork(xmlrpc_env * const envP,
           curlMulti *  const curlMultiP,
           bool *       const transStillRunningP) {
/*----------------------------------------------------------------------------
   Do whatever work is ready to be done by the Curl multi manager
   identified by 'curlMultiP'.  This typically is transferring data on
   an HTTP connection because the server is ready.

   For each transaction for which the multi manager finishes all the
   required work, complete the transaction by calling its
   "finish" routine.

   Return *transStillRunningP false if this work completes all of the
   manager's transactions so that there is no reason to call us ever
   again.
-----------------------------------------------------------------------------*/
    bool immediateWorkToDo;
    int runningHandles;

    immediateWorkToDo = true;  /* initial assumption */

    while (immediateWorkToDo && !envP->fault_occurred) {
        curlMulti_perform(envP, curlMultiP,
                          &immediateWorkToDo, &runningHandles);
    }

    /* We either did all the work that's ready to do or hit an error. */

    if (!envP->fault_occurred) {
        /* The work we did may have resulted in asynchronous messages
           (asynchronous to the thing they refer to, not to us, of course).
           In particular the message "Curl transaction has completed".
           So we process those now.
        */
        processCurlMessages(envP, curlMultiP);

        *transStillRunningP = runningHandles > 0;
    }
}



static void
finishCurlMulti(xmlrpc_env *       const envP,
                curlMulti *        const curlMultiP,
                xmlrpc_timeoutType const timeoutType,
                xmlrpc_timespec    const deadline,
                int *              const interruptP) {
/*----------------------------------------------------------------------------
   Prosecute all the Curl transactions under the control of
   *curlMultiP.  E.g. send data if server is ready to take it, get
   data if server has sent some, wind up the transaction if it is
   done.

   Don't return until all the Curl transactions are done or we time out.

   The *interruptP flag alone will not interrupt us.  We will wait in
   spite of it for all Curl transactions to complete.  *interruptP
   just gives us a hint that the Curl transactions are being
   interrupted, so we know there is work to do for them.  (The way it
   works is Caller sets up a "progress" function that checks the same
   interrupt flag and reports "kill me."  When we see the interrupt
   flag, we call that progress function and get the message).
-----------------------------------------------------------------------------*/
    bool rpcStillRunning;
    bool timedOut;

    rpcStillRunning = true;  /* initial assumption */
    timedOut = false;
    
    while (rpcStillRunning && !timedOut && !envP->fault_occurred) {

        if (interruptP) {
            waitForWorkInt(envP, curlMultiP, timeoutType, deadline,
                           interruptP);
        } else 
            waitForWork(envP, curlMultiP, timeoutType, deadline, NULL);

        if (!envP->fault_occurred) {
            xmlrpc_timespec nowTime;

            /* doCurlWork() (among other things) finds Curl
               transactions that user wants to abort and finishes
               them.
            */
            doCurlWork(envP, curlMultiP, &rpcStillRunning);
            
            xmlrpc_gettimeofday(&nowTime);
            
            timedOut = (timeoutType == timeout_yes &&
                        timeIsAfter(nowTime, deadline));
        }
    }
}



static void
getTimeoutParm(xmlrpc_env *                          const envP,
               const struct xmlrpc_curl_xportparms * const curlXportParmsP,
               size_t                                const parmSize,
               unsigned int *                        const timeoutP) {
               
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(timeout))
        *timeoutP = 0;
    else {
        if (curlHasNosignal()) {
            /* libcurl takes a 'long' in milliseconds for the timeout value */
            if ((unsigned)(long)(curlXportParmsP->timeout) !=
                curlXportParmsP->timeout)
                xmlrpc_faultf(envP, "Timeout value %u is too large.",
                              curlXportParmsP->timeout);
            else
                *timeoutP = curlXportParmsP->timeout;
        } else
            xmlrpc_faultf(envP, "You cannot specify a 'timeout' parameter "
                          "because the Curl library is too old and is not "
                          "capable of doing timeouts except by using "
                          "signals.  You need at least Curl 7.10");
    }
}



static void
setVerbose(bool * const verboseP) {

    const char * const xmlrpcTraceCurl = getenv("XMLRPC_TRACE_CURL");

    if (xmlrpcTraceCurl)
        *verboseP = true;
    else
        *verboseP = false;
}



static void
getXportParms(xmlrpc_env *                          const envP,
              const struct xmlrpc_curl_xportparms * const curlXportParmsP,
              size_t                                const parmSize,
              struct xmlrpc_client_transport *      const transportP) {
/*----------------------------------------------------------------------------
   Get the parameters out of *curlXportParmsP and update *transportP
   to reflect them.

   *curlXportParmsP is a 'parmSize' bytes long prefix of
   struct xmlrpc_curl_xportparms.

   curlXportParmsP is something the user created.  It's designed to be
   friendly to the user, not to this program, and is encumbered by
   lots of backward compatibility constraints.  In particular, the
   user may have coded and/or compiled it at a time that struct
   xmlrpc_curl_xportparms was smaller than it is now!

   Also, the user might have specified something invalid.

   So that's why we don't simply attach a copy of *curlXportParmsP to
   *transportP.

   To the extent that *curlXportParmsP is too small to contain a parameter,
   we return the default value for that parameter.

   Special case:  curlXportParmsP == NULL means there is no input at all.
   In that case, we return default values for everything.
-----------------------------------------------------------------------------*/
    struct curlSetup * const curlSetupP = &transportP->curlSetupStuff;

    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(user_agent))
        transportP->userAgent = NULL;
    else if (curlXportParmsP->user_agent == NULL)
        transportP->userAgent = NULL;
    else
        transportP->userAgent = strdup(curlXportParmsP->user_agent);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(dont_advertise))
        transportP->dontAdvertise = false;
    else
        transportP->dontAdvertise = curlXportParmsP->dont_advertise;
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(network_interface))
        curlSetupP->networkInterface = NULL;
    else if (curlXportParmsP->network_interface == NULL)
        curlSetupP->networkInterface = NULL;
    else
        curlSetupP->networkInterface =
            strdup(curlXportParmsP->network_interface);

    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(no_ssl_verifypeer))
        curlSetupP->sslVerifyPeer = true;
    else
        curlSetupP->sslVerifyPeer = !curlXportParmsP->no_ssl_verifypeer;
        
    if (!curlXportParmsP || 
        parmSize < XMLRPC_CXPSIZE(no_ssl_verifyhost))
        curlSetupP->sslVerifyHost = true;
    else
        curlSetupP->sslVerifyHost = !curlXportParmsP->no_ssl_verifyhost;

    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(ssl_cert))
        curlSetupP->sslCert = NULL;
    else if (curlXportParmsP->ssl_cert == NULL)
        curlSetupP->sslCert = NULL;
    else
        curlSetupP->sslCert = strdup(curlXportParmsP->ssl_cert);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(sslcerttype))
        curlSetupP->sslCertType = NULL;
    else if (curlXportParmsP->sslcerttype == NULL)
        curlSetupP->sslCertType = NULL;
    else
        curlSetupP->sslCertType = strdup(curlXportParmsP->sslcerttype);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(sslcertpasswd))
        curlSetupP->sslCertPasswd = NULL;
    else if (curlXportParmsP->sslcertpasswd == NULL)
        curlSetupP->sslCertPasswd = NULL;
    else
        curlSetupP->sslCertPasswd = strdup(curlXportParmsP->sslcertpasswd);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(sslkey))
        curlSetupP->sslKey = NULL;
    else if (curlXportParmsP->sslkey == NULL)
        curlSetupP->sslKey = NULL;
    else
        curlSetupP->sslKey = strdup(curlXportParmsP->sslkey);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(sslkeytype))
        curlSetupP->sslKeyType = NULL;
    else if (curlXportParmsP->sslkeytype == NULL)
        curlSetupP->sslKeyType = NULL;
    else
        curlSetupP->sslKeyType = strdup(curlXportParmsP->sslkeytype);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(sslkeypasswd))
        curlSetupP->sslKeyPasswd = NULL;
    else if (curlXportParmsP->sslkeypasswd == NULL)
        curlSetupP->sslKeyPasswd = NULL;
    else
        curlSetupP->sslKeyPasswd = strdup(curlXportParmsP->sslkeypasswd);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(sslengine))
        curlSetupP->sslEngine = NULL;
    else if (curlXportParmsP->sslengine == NULL)
        curlSetupP->sslEngine = NULL;
    else
        curlSetupP->sslEngine = strdup(curlXportParmsP->sslengine);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(sslengine_default))
        curlSetupP->sslEngineDefault = false;
    else
        curlSetupP->sslEngineDefault = !!curlXportParmsP->sslengine_default;
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(sslversion))
        curlSetupP->sslVersion = XMLRPC_SSLVERSION_DEFAULT;
    else
        curlSetupP->sslVersion = curlXportParmsP->sslversion;
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(cainfo))
        curlSetupP->caInfo = NULL;
    else if (curlXportParmsP->cainfo == NULL)
        curlSetupP->caInfo = NULL;
    else
        curlSetupP->caInfo = strdup(curlXportParmsP->cainfo);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(capath))
        curlSetupP->caPath = NULL;
    else if (curlXportParmsP->capath == NULL)
        curlSetupP->caPath = NULL;
    else
        curlSetupP->caPath = strdup(curlXportParmsP->capath);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(randomfile))
        curlSetupP->randomFile = NULL;
    else if (curlXportParmsP->randomfile == NULL)
        curlSetupP->randomFile = NULL;
    else
        curlSetupP->randomFile = strdup(curlXportParmsP->randomfile);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(egdsocket))
        curlSetupP->egdSocket = NULL;
    else if (curlXportParmsP->egdsocket == NULL)
        curlSetupP->egdSocket = NULL;
    else
        curlSetupP->egdSocket = strdup(curlXportParmsP->egdsocket);
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(ssl_cipher_list))
        curlSetupP->sslCipherList = NULL;
    else if (curlXportParmsP->ssl_cipher_list == NULL)
        curlSetupP->sslCipherList = NULL;
    else
        curlSetupP->sslCipherList = strdup(curlXportParmsP->ssl_cipher_list);

    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(proxy))
        curlSetupP->proxy = NULL;
    else if (curlXportParmsP->proxy == NULL)
        curlSetupP->proxy = NULL;
    else
        curlSetupP->proxy = strdup(curlXportParmsP->proxy);

    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(proxy_port))
        curlSetupP->proxyPort = 8080;
    else
        curlSetupP->proxyPort = curlXportParmsP->proxy_port;

    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(proxy_auth))
        curlSetupP->proxyAuth = CURLAUTH_BASIC;
    else
        curlSetupP->proxyAuth = curlXportParmsP->proxy_auth;

    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(proxy_userpwd))
        curlSetupP->proxyUserPwd = NULL;
    else if (curlXportParmsP->proxy_userpwd == NULL)
        curlSetupP->proxyUserPwd = NULL;
    else
        curlSetupP->proxyUserPwd = strdup(curlXportParmsP->proxy_userpwd);

    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(proxy_type))
        curlSetupP->proxyType = CURLPROXY_HTTP;
    else
        curlSetupP->proxyType = curlXportParmsP->proxy_type;

    getTimeoutParm(envP, curlXportParmsP, parmSize, &curlSetupP->timeout);
}



static void
freeXportParms(const struct xmlrpc_client_transport * const transportP) {

    const struct curlSetup * const curlSetupP = &transportP->curlSetupStuff;

    if (curlSetupP->sslCipherList)
        xmlrpc_strfree(curlSetupP->sslCipherList);
    if (curlSetupP->egdSocket)
        xmlrpc_strfree(curlSetupP->egdSocket);
    if (curlSetupP->randomFile)
        xmlrpc_strfree(curlSetupP->randomFile);
    if (curlSetupP->caPath)
        xmlrpc_strfree(curlSetupP->caPath);
    if (curlSetupP->caInfo)
        xmlrpc_strfree(curlSetupP->caInfo);
    if (curlSetupP->sslEngine)
        xmlrpc_strfree(curlSetupP->sslEngine);
    if (curlSetupP->sslKeyPasswd)
        xmlrpc_strfree(curlSetupP->sslKeyPasswd);
    if (curlSetupP->sslKeyType)
        xmlrpc_strfree(curlSetupP->sslKeyType);
    if (curlSetupP->sslKey)
        xmlrpc_strfree(curlSetupP->sslKey);
    if (curlSetupP->sslCertPasswd)
        xmlrpc_strfree(curlSetupP->sslCertPasswd);
    if (curlSetupP->sslCertType)
        xmlrpc_strfree(curlSetupP->sslCertType);
    if (curlSetupP->sslCert)
        xmlrpc_strfree(curlSetupP->sslCert);
    if (curlSetupP->networkInterface)
        xmlrpc_strfree(curlSetupP->networkInterface);
    if (transportP->userAgent)
        xmlrpc_strfree(transportP->userAgent);
    if (curlSetupP->proxy)
        xmlrpc_strfree(curlSetupP->proxy);
    if (curlSetupP->proxyUserPwd)
        xmlrpc_strfree(curlSetupP->proxyUserPwd);
}



static void
createSyncCurlSession(xmlrpc_env * const envP,
                      CURL **      const curlSessionPP) {
/*----------------------------------------------------------------------------
   Create a Curl session to be used for multiple serial transactions.
   The Curl session we create is not complete -- it still has to be
   further set up for each particular transaction.

   We can't set up anything here that changes from one transaction to the
   next.

   We don't bother setting up anything that has to be set up for an
   asynchronous transaction because code that is common between synchronous
   and asynchronous transactions takes care of that anyway.

   That leaves things, such as cookies, that don't exist for
   asynchronous transactions, and are common to multiple serial
   synchronous transactions.
-----------------------------------------------------------------------------*/
    CURL * const curlSessionP = curl_easy_init();

    if (curlSessionP == NULL)
        xmlrpc_faultf(envP, "Could not create Curl session.  "
                      "curl_easy_init() failed.");
    else {
        /* The following is a trick.  CURLOPT_COOKIEFILE is the name
           of the file containing the initial cookies for the Curl
           session.  But setting it is also what turns on the cookie
           function itself, whereby the Curl library accepts and
           stores cookies from the server and sends them back on
           future requests.  We don't have a file of initial cookies, but
           we want to turn on cookie function, so we set the option to
           something we know does not validly name a file.  Curl will
           ignore the error and just start up cookie function with no
           initial cookies.
        */
        curl_easy_setopt(curlSessionP, CURLOPT_COOKIEFILE, "");

        *curlSessionPP = curlSessionP;
    }
}



static void
destroySyncCurlSession(CURL * const curlSessionP) {

    curl_easy_cleanup(curlSessionP);
}



static void
makeSyncCurlSession(xmlrpc_env *                     const envP,
                    struct xmlrpc_client_transport * const transportP) {

    transportP->syncCurlSessionLockP = curlLock_create_pthread();
    if (transportP->syncCurlSessionLockP == NULL)
        xmlrpc_faultf(envP, "Unable to create lock for "
                      "synchronous Curl session.");
    else {
        createSyncCurlSession(envP, &transportP->syncCurlSessionP);

        if (!envP->fault_occurred) {
            /* We'll need a multi manager to actually execute this session: */
            transportP->syncCurlMultiP = curlMulti_create();
        
            if (transportP->syncCurlMultiP == NULL)
                xmlrpc_faultf(envP, "Unable to create Curl multi manager for "
                              "synchronous RPCs");

            if (envP->fault_occurred)
                destroySyncCurlSession(transportP->syncCurlSessionP);
        }
        if (envP->fault_occurred)
            transportP->syncCurlSessionLockP->destroy(
                transportP->syncCurlSessionLockP); 
    }
}



static void
unmakeSyncCurlSession(struct xmlrpc_client_transport * const transportP) {

    curlMulti_destroy(transportP->syncCurlMultiP);

    destroySyncCurlSession(transportP->syncCurlSessionP);

    transportP->syncCurlSessionLockP->destroy(
        transportP->syncCurlSessionLockP); 
}



static void 
create(xmlrpc_env *                      const envP,
       int                               const flags ATTR_UNUSED,
       const char *                      const appname ATTR_UNUSED,
       const char *                      const appversion ATTR_UNUSED,
       const void *                      const transportparmsP,
       size_t                            const parm_size,
       struct xmlrpc_client_transport ** const handlePP) {
/*----------------------------------------------------------------------------
   This does the 'create' operation for a Curl client transport.
-----------------------------------------------------------------------------*/
    const struct xmlrpc_curl_xportparms * const curlXportParmsP = 
        transportparmsP;

    struct xmlrpc_client_transport * transportP;

    MALLOCVAR(transportP);
    if (transportP == NULL)
        xmlrpc_faultf(envP, "Unable to allocate transport descriptor.");
    else {
        setVerbose(&transportP->curlSetupStuff.verbose);

        transportP->interruptP = NULL;

        transportP->asyncCurlMultiP = curlMulti_create();
        
        if (transportP->asyncCurlMultiP == NULL)
            xmlrpc_faultf(envP, "Unable to create Curl multi manager for "
                          "asynchronous RPCs");
        else {
            getXportParms(envP, curlXportParmsP, parm_size, transportP);
            
            if (!envP->fault_occurred) {
                makeSyncCurlSession(envP, transportP);
                
                if (envP->fault_occurred)
                    freeXportParms(transportP);
            }
            if (envP->fault_occurred)
                curlMulti_destroy(transportP->asyncCurlMultiP);
        }
        if (envP->fault_occurred)
            free(transportP);
    }
    *handlePP = transportP;
}



static void
setInterrupt(struct xmlrpc_client_transport * const clientTransportP,
             int *                            const interruptP) {

    clientTransportP->interruptP = interruptP;
}



static void
assertNoOutstandingCurlWork(curlMulti * const curlMultiP) {

    xmlrpc_env env;
    bool immediateWorkToDo;
    int runningHandles;
    
    xmlrpc_env_init(&env);
    
    curlMulti_perform(&env, curlMultiP, &immediateWorkToDo, &runningHandles);
    
    /* We know the above was a no-op, since we're asserting that there
       is no outstanding work.
    */
    XMLRPC_ASSERT(!env.fault_occurred);
    XMLRPC_ASSERT(!immediateWorkToDo);
    XMLRPC_ASSERT(runningHandles == 0);
    xmlrpc_env_clean(&env);
}



static void 
destroy(struct xmlrpc_client_transport * const clientTransportP) {
/*----------------------------------------------------------------------------
   This does the 'destroy' operation for a Curl client transport.

   An RPC is a reference to a client XML transport, so you may not
   destroy a transport while RPCs are running.  To ensure no
   asynchronous RPCs are running, you must successfully execute the
   transport 'finishAsync' method, with no interruptions or timeouts
   allowed.  To speed that up, you can set the transport's interrupt
   flag to 1 first, which will make all outstanding RPCs fail
   immediately.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT(clientTransportP != NULL);

    assertNoOutstandingCurlWork(clientTransportP->asyncCurlMultiP);
        /* We know this is true because a condition of destroying the
           transport is that there be no outstanding asynchronous RPCs.
        */
    assertNoOutstandingCurlWork(clientTransportP->syncCurlMultiP);
        /* This is because a condition of destroying the transport is
           that no transport method be running.  The only way a
           synchronous RPC can be in progress is for the 'perform' method
           to be running.
        */

    unmakeSyncCurlSession(clientTransportP);

    curlMulti_destroy(clientTransportP->asyncCurlMultiP);

    freeXportParms(clientTransportP);

    free(clientTransportP);
}



static void
performCurlTransaction(xmlrpc_env *      const envP,
                       curlTransaction * const curlTransactionP,
                       curlMulti *       const curlMultiP,
                       int *             const interruptP) {

    curlMulti_addHandle(envP, curlMultiP,
                        curlTransaction_curlSession(curlTransactionP));

    /* Failure here just means something screwy in the multi manager;
       Above does not even begin to perform the HTTP transaction
    */

    if (!envP->fault_occurred) {
        xmlrpc_timespec const dummy = {0,0};

        finishCurlMulti(envP, curlMultiP, timeout_no, dummy, interruptP);

        /* Failure here just means something screwy in the multi
           manager; any failure of the HTTP transaction would have been
           recorded in *curlTransactionP.
        */

        if (!envP->fault_occurred) {
            /* Curl session completed OK.  But did HTTP transaction
               work?
            */
            curlTransaction_getError(curlTransactionP, envP);
        }
        /* If the CURL transaction is still going, removing the handle
           here aborts it.  At least it's supposed to.  From what I've
           seen in the Curl code in 2007, I don't think it does.  I
           couldn't get Curl maintainers interested in the problem,
           except to say, "If you're right, there's a bug."
        */
        curlMulti_removeHandle(curlMultiP,
                               curlTransaction_curlSession(curlTransactionP));
    }
}



static void
startRpc(xmlrpc_env * const envP,
         rpc *        const rpcP) {

    curlMulti_addHandle(envP,
                        rpcP->transportP->asyncCurlMultiP,
                        curlTransaction_curlSession(rpcP->curlTransactionP));
}



static curlt_finishFn   finishRpcCurlTransaction;
static curlt_progressFn curlTransactionProgress;



static void
createRpc(xmlrpc_env *                     const envP,
          struct xmlrpc_client_transport * const clientTransportP,
          CURL *                           const curlSessionP,
          const xmlrpc_server_info *       const serverP,
          xmlrpc_mem_block *               const callXmlP,
          xmlrpc_mem_block *               const responseXmlP,
          xmlrpc_transport_asynch_complete       complete, 
          xmlrpc_transport_progress              progress,
          struct xmlrpc_call_info *        const callInfoP,
          rpc **                           const rpcPP) {

    rpc * rpcP;

    MALLOCVAR(rpcP);
    if (rpcP == NULL)
        xmlrpc_faultf(envP, "Couldn't allocate memory for rpc object");
    else {
        rpcP->transportP   = clientTransportP;
        rpcP->curlSessionP = curlSessionP;
        rpcP->callInfoP    = callInfoP;
        rpcP->complete     = complete;
        rpcP->progress     = progress;
        rpcP->responseXmlP = responseXmlP;

        curlTransaction_create(envP,
                               curlSessionP,
                               serverP,
                               callXmlP, responseXmlP, 
                               clientTransportP->dontAdvertise,
                               clientTransportP->userAgent,
                               &clientTransportP->curlSetupStuff,
                               rpcP,
                               complete ? &finishRpcCurlTransaction : NULL,
                               progress ? &curlTransactionProgress : NULL,
                               &rpcP->curlTransactionP);
        if (!envP->fault_occurred) {
            if (envP->fault_occurred)
                curlTransaction_destroy(rpcP->curlTransactionP);
        }
        if (envP->fault_occurred)
            free(rpcP);
    }
    *rpcPP = rpcP;
}



static void 
destroyRpc(rpc * const rpcP) {

    XMLRPC_ASSERT_PTR_OK(rpcP);

    curlTransaction_destroy(rpcP->curlTransactionP);

    free(rpcP);
}



static void
performRpc(xmlrpc_env * const envP,
           rpc *        const rpcP,
           curlMulti *  const curlMultiP,
           int *        const interruptP) {

    performCurlTransaction(envP, rpcP->curlTransactionP, curlMultiP,
                           interruptP);
}



static curlt_finishFn finishRpcCurlTransaction;

static void
finishRpcCurlTransaction(xmlrpc_env * const envP ATTR_UNUSED,
                         void *       const userContextP) {
/*----------------------------------------------------------------------------
  Handle the event that a Curl transaction for an asynchronous RPC has
  completed on the Curl session identified by 'curlSessionP'.

  Tell the requester of the RPC the results.

  Remove the Curl session from its Curl multi manager and destroy the
  Curl session, the XML response buffer, the Curl transaction, and the RPC.
-----------------------------------------------------------------------------*/
    rpc * const rpcP = userContextP;
    curlTransaction * const curlTransactionP = rpcP->curlTransactionP;
    struct xmlrpc_client_transport * const transportP = rpcP->transportP;

    curlMulti_removeHandle(transportP->asyncCurlMultiP,
                           curlTransaction_curlSession(curlTransactionP));

    {
        xmlrpc_env env;

        xmlrpc_env_init(&env);

        curlTransaction_getError(curlTransactionP, &env);

        rpcP->complete(rpcP->callInfoP, rpcP->responseXmlP, env);

        xmlrpc_env_clean(&env);
    }

    curl_easy_cleanup(rpcP->curlSessionP);

    XMLRPC_MEMBLOCK_FREE(char, rpcP->responseXmlP);

    destroyRpc(rpcP);
}



static curlt_progressFn curlTransactionProgress;

static void
curlTransactionProgress(void * const context,
                        double const dlTotal,
                        double const dlNow,
                        double const ulTotal,
                        double const ulNow,
                        bool * const abortP) {
/*----------------------------------------------------------------------------
   This is equivalent to a Curl "progress function" (the curlTransaction
   object just passes through the call from libcurl).

   The curlTransaction calls this once a second telling us how much
   data has transferred.  If the transport user has set up a progress
   function, we call that with this progress information.  That 
   function might e.g. display a progress bar.

   Additionally, the curlTransaction gives us the opportunity to tell it
   to abort the transaction, which we do if the user has set his
   "interrupt" flag (which he registered with the transport when he
   created it).
-----------------------------------------------------------------------------*/
    rpc * const rpcP = context;
    struct xmlrpc_client_transport * const transportP = rpcP->transportP;

    struct xmlrpc_progress_data progressData;

    assert(rpcP);
    assert(transportP);
    assert(rpcP->progress);

    progressData.response.total = dlTotal;
    progressData.response.now   = dlNow;
    progressData.call.total     = ulTotal;
    progressData.call.now       = ulNow;

    rpcP->progress(rpcP->callInfoP, progressData);

    if (transportP->interruptP)
        *abortP = *transportP->interruptP;
    else
        *abortP = false;
}



static void 
sendRequest(xmlrpc_env *                     const envP, 
            struct xmlrpc_client_transport * const clientTransportP,
            const xmlrpc_server_info *       const serverP,
            xmlrpc_mem_block *               const callXmlP,
            xmlrpc_transport_asynch_complete       complete,
            xmlrpc_transport_progress              progress,
            struct xmlrpc_call_info *        const callInfoP) {
/*----------------------------------------------------------------------------
   Initiate an XML-RPC rpc asynchronously.  Don't wait for it to go to
   the server.

   Unless we return failure, we arrange to have complete() called when
   the rpc completes.

   This does the 'send_request' operation for a Curl client transport.
-----------------------------------------------------------------------------*/
    rpc * rpcP;
    xmlrpc_mem_block * responseXmlP;

    responseXmlP = XMLRPC_MEMBLOCK_NEW(char, envP, 0);
    if (!envP->fault_occurred) {
        CURL * const curlSessionP = curl_easy_init();
    
        if (curlSessionP == NULL)
            xmlrpc_faultf(envP, "Could not create Curl session.  "
                          "curl_easy_init() failed.");
        else {
            createRpc(envP, clientTransportP, curlSessionP, serverP,
                      callXmlP, responseXmlP, complete, progress, callInfoP,
                      &rpcP);
            
            if (!envP->fault_occurred) {
                startRpc(envP, rpcP);
                
                if (envP->fault_occurred)
                    destroyRpc(rpcP);
            }
            if (envP->fault_occurred)
                curl_easy_cleanup(curlSessionP);
        }
        if (envP->fault_occurred)
            XMLRPC_MEMBLOCK_FREE(char, responseXmlP);
    }
    /* If we're returning success, the user's eventual finish_asynch
       call will destroy this RPC, Curl session, and response buffer
       and remove the Curl session from the Curl multi manager.
       (If we're returning failure, we didn't create any of those).
    */
}



static void 
finishAsynch(
    struct xmlrpc_client_transport * const clientTransportP,
    xmlrpc_timeoutType               const timeoutType,
    xmlrpc_timeout                   const timeout) {
/*----------------------------------------------------------------------------
   Wait for the Curl multi manager to finish the Curl transactions for
   all outstanding RPCs and destroy those RPCs.

   But give up if a) too much time passes as defined by 'timeoutType'
   and 'timeout'; or b) the transport client requests interruption
   (i.e. the transport's interrupt flag becomes nonzero).  Normally, a
   signal must get our attention for us to notice the interrupt flag.

   This does the 'finish_asynch' operation for a Curl client transport.

   It would be cool to replace this with something analogous to the
   Curl asynchronous interface: Have something like curl_multi_fdset()
   that returns a bunch of file descriptors on which the user can wait
   (along with possibly other file descriptors of his own) and
   something like curl_multi_perform() to finish whatever RPCs are
   ready to finish at that moment.  The implementation would be little
   more than wrapping curl_multi_fdset() and curl_multi_perform().

   Note that the user can call this multiple times, due to timeouts,
   but must eventually call it once with no timeout so he
   knows that all the RPCs are finished.  Either that or terminate the
   process so it doesn't matter if RPCs are still going.
-----------------------------------------------------------------------------*/
    xmlrpc_env env;

    xmlrpc_timespec waitTimeoutTime;
        /* The datetime after which we should quit waiting */

    xmlrpc_env_init(&env);
    
    if (timeoutType == timeout_yes) {
        xmlrpc_timespec waitStartTime;
        xmlrpc_gettimeofday(&waitStartTime);
        addMilliseconds(waitStartTime, timeout, &waitTimeoutTime);
    }

    finishCurlMulti(&env, clientTransportP->asyncCurlMultiP,
                    timeoutType, waitTimeoutTime,
                    clientTransportP->interruptP);

    /* If the above fails, it is catastrophic, because it means there is
       no way to complete outstanding Curl transactions and RPCs, and
       no way to release their resources.

       We should at least expand this interface some day to push the
       problem back up to the user, but for now we just do this Hail Mary
       response.

       Note that a failure of finish_curlMulti() does not mean that
       a session completed with an error or an RPC completed with an
       error.  Those things are reported up through the user's 
       xmlrpc_transport_asynch_complete routine.  A failure here is
       something that stopped us from calling that.

       Note that a timeout causes a successful completion,
       but without finishing all the RPCs!
    */

    if (env.fault_occurred)
        fprintf(stderr, "finishAsync() failed.  Xmlrpc-c Curl transport "
                "is now in an unknown state and may not be able to "
                "continue functioning.  Specifics of the failure: %s\n",
                env.fault_string);

    xmlrpc_env_clean(&env);
}



static void
call(xmlrpc_env *                     const envP,
     struct xmlrpc_client_transport * const clientTransportP,
     const xmlrpc_server_info *       const serverP,
     xmlrpc_mem_block *               const callXmlP,
     xmlrpc_mem_block **              const responseXmlPP) {

    xmlrpc_mem_block * responseXmlP;
    rpc * rpcP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(serverP);
    XMLRPC_ASSERT_PTR_OK(callXmlP);
    XMLRPC_ASSERT_PTR_OK(responseXmlPP);

    responseXmlP = XMLRPC_MEMBLOCK_NEW(char, envP, 0);
    if (!envP->fault_occurred) {
        /* Only one RPC at a time can use a Curl session, so we have to
           hold the lock as long as our RPC exists.
        */
        lockSyncCurlSession(clientTransportP);
        createRpc(envP, clientTransportP, clientTransportP->syncCurlSessionP,
                  serverP,
                  callXmlP, responseXmlP,
                  NULL, NULL, NULL,
                  &rpcP);

        if (!envP->fault_occurred) {
            performRpc(envP, rpcP, clientTransportP->syncCurlMultiP,
                       clientTransportP->interruptP);

            *responseXmlPP = responseXmlP;

            destroyRpc(rpcP);
        }
        unlockSyncCurlSession(clientTransportP);
        if (envP->fault_occurred)
            XMLRPC_MEMBLOCK_FREE(char, responseXmlP);
    }
}



static void
setupGlobalConstants(xmlrpc_env * const envP) {
/*----------------------------------------------------------------------------
   See longwinded discussion of the global constant issue at the top of
   this file.
-----------------------------------------------------------------------------*/
    initWindowsStuff(envP);

    if (!envP->fault_occurred) {
        CURLcode rc;

        rc = curl_global_init(CURL_GLOBAL_ALL);
        
        if (rc != CURLE_OK)
            xmlrpc_faultf(envP, "curl_global_init() failed with code %d", rc);
    }
}



static void
teardownGlobalConstants(void) {
/*----------------------------------------------------------------------------
   See longwinded discussionof the global constant issue at the top of
   this file.
-----------------------------------------------------------------------------*/
    curl_global_cleanup();

    termWindowsStuff();
}



struct xmlrpc_client_transport_ops xmlrpc_curl_transport_ops = {
    &setupGlobalConstants,
    &teardownGlobalConstants,
    &create,
    &destroy,
    &sendRequest,
    &call,
    &finishAsynch,
    &setInterrupt,
};
