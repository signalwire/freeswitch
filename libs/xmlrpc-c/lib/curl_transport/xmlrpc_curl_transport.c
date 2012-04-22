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

#include "xmlrpc_config.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>
#if !MSVCRT
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

#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/select_int.h"
#include "xmlrpc-c/client_int.h"
#include "xmlrpc-c/transport.h"
#include "xmlrpc-c/time_int.h"
#include "version.h"

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>
#include <curl/multi.h>

#if MSVCRT
#if defined(_DEBUG)
#  include <crtdbg.h>
#  define new DEBUG_NEW
#  define malloc(size) _malloc_dbg( size, _NORMAL_BLOCK, __FILE__, __LINE__)
#  undef THIS_FILE
   static char THIS_FILE[] = __FILE__;
#endif
#endif


#define CMAJOR LIBCURL_VERSION_MAJOR
#define CMINOR LIBCURL_VERSION_MINOR
#if CMAJOR > 7 || (CMAJOR == 7 && CMINOR >= 10)
  #define HAVE_CURL_NOSIGNAL 1
#else
  #define HAVE_CURL_NOSIGNAL 0
#endif
#if CMAJOR > 7 || (CMAJOR == 7 && CMINOR >= 12)
  #define HAVE_CURL_STRERROR 1
#else
  #define HAVE_CURL_STRERROR 0
#endif

#undef CMAJOR
#undef CMINOR



typedef struct rpc rpc;

typedef struct curlTransaction curlTransaction;



struct curlSetup {

    /* This is all client transport properties that are implemented as
       simple Curl session properties (i.e. the transport basically just
       passes them through to Curl without looking at them).

       People occasionally want to replace all this with something where
       the Xmlrpc-c user simply does the curl_easy_setopt() call and this
       code need not know about all these options.  Unfortunately, that's
       a significant modularity violation.  Either the Xmlrpc-c user
       controls the Curl object or he doesn't.  If he does, then he
       shouldn't use libxmlrpc_client -- he should just copy some of this
       code into his own program.  If he doesn't, then he should never see
       the Curl library.

       Speaking of modularity: the only reason this is a separate struct
       is to make the code easier to manage.  Ideally, the fact that these
       particular properties of the transport are implemented by simple
       Curl session setup would be known only at the lowest level code
       that does that setup.
    */

    const char * networkInterface;
        /* This identifies the network interface on the local side to
           use for the session.  It is an ASCIIZ string in the form
           that the Curl recognizes for setting its CURLOPT_INTERFACE
           option (also the --interface option of the Curl program).
           E.g. "9.1.72.189" or "giraffe-data.com" or "eth0".  

           It isn't necessarily valid, but it does have a terminating NUL.

           NULL means we have no preference.
        */
    xmlrpc_bool sslVerifyPeer;
        /* In an SSL connection, we should authenticate the server's SSL
           certificate -- refuse to talk to him if it isn't authentic.
           This is equivalent to Curl's CURLOPT_SSL_VERIFY_PEER option.
        */
    xmlrpc_bool sslVerifyHost;
        /* In an SSL connection, we should verify that the server's
           certificate (independently of whether the certificate is
           authentic) indicates the host name that is in the URL we
           are using for the server.
        */

    const char * sslCert;
    const char * sslCertType;
    const char * sslCertPasswd;
    const char * sslKey;
    const char * sslKeyType;
    const char * sslKeyPasswd;
    const char * sslEngine;
    bool         sslEngineDefault;
    unsigned int sslVersion;
    const char * caInfo;
    const char * caPath;
    const char * randomFile;
    const char * egdSocket;
    const char * sslCipherList;
    unsigned int timeout;
        /* 0 = no Curl timeout.  This is in milliseconds. */
};


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



/*============================================================================
      locks
==============================================================================
   This is the beginnings of a lock abstraction that will allow this
   transport to be used with locks other than pthread locks
============================================================================*/

struct lock {
    pthread_mutex_t theLock;
    void (*lock)(struct lock *);
    void (*unlock)(struct lock *);
    void (*destroy)(struct lock *);
};

typedef struct lock lock;

static void
lock_pthread(struct lock * const lockP) {
    pthread_mutex_lock(&lockP->theLock);
}

static void
unlock_pthread(struct lock * const lockP) {
    pthread_mutex_unlock(&lockP->theLock);
}

static void
destroyLock_pthread(struct lock * const lockP) {
    pthread_mutex_destroy(&lockP->theLock);
    free(lockP);
}


static struct lock *
createLock_pthread(void) {
    struct lock * lockP;
    MALLOCVAR(lockP);
    if (lockP) {
        pthread_mutex_init(&lockP->theLock, NULL);
        lockP->lock    = &lock_pthread;
        lockP->unlock  = &unlock_pthread;
        lockP->destroy = &destroyLock_pthread;
    }
    return lockP;
}



/*=============================================================================
    curlTransaction
=============================================================================*/

typedef void finishCurlTransactionFn(xmlrpc_env * const, curlTransaction * const);

struct curlTransaction {
    /* This is all stuff that really ought to be in a Curl object, but
       the Curl library is a little too simple for that.  So we build
       a layer on top of Curl, and define this "transaction," as an
       object subordinate to a Curl "session."  A Curl session has
       zero or one transactions in progress.  The Curl session
       "private data" is a pointer to the CurlTransaction object for
       the current transaction.
    */
    CURL * curlSessionP;
        /* Handle for the Curl session that hosts this transaction.
           Note that only one transaction at a time can use a particular
           Curl session, so this had better not be a session that some other
           transaction is using simultaneously.
        */
    finishCurlTransactionFn * finish;
    rpc * rpcP;
        /* The RPC which this transaction serves.  (If this structure
           were a true extension of the Curl library as described above,
           this would be a void *, since the Curl library doesn't know what
           an RPC is, but since we use it only for that, we might as well
           use the specific type here).
        */
    CURLcode result;
        /* Result of the transaction (succeeded, TCP connect failed, etc.).
           A properly executed HTTP transaction (request & response) counts
           as a successful transaction.  When 'result' show success,
           curl_easy_get_info() tells you whether the transaction succeeded
           at the HTTP level.
        */
    char curlError[CURL_ERROR_SIZE];
        /* Error message from Curl */
    struct curl_slist * headerList;
        /* The HTTP headers for the transaction */
    const char * serverUrl;  /* malloc'ed - belongs to this object */
};



static void
interpretCurlEasyError(const char ** const descriptionP,
                       CURLcode      const code) {

#if HAVE_CURL_STRERROR
    *descriptionP = strdup(curl_easy_strerror(code));
#else
    xmlrpc_asprintf(descriptionP, "Curl error code (CURLcode) %d", code);
#endif
}



static void
interpretCurlMultiError(const char ** const descriptionP,
                        CURLMcode     const code) {

#if HAVE_CURL_STRERROR
    *descriptionP = strdup(curl_multi_strerror(code));
#else
    xmlrpc_asprintf(descriptionP, "Curl error code (CURLMcode) %d", code);
#endif
}



static void
addHeader(xmlrpc_env * const envP,
          struct curl_slist ** const headerListP,
          const char *         const headerText) {

    struct curl_slist * newHeaderList;
    newHeaderList = curl_slist_append(*headerListP, headerText);
    if (newHeaderList == NULL)
        xmlrpc_faultf(envP,
                      "Could not add header '%s'.  "
                      "curl_slist_append() failed.", headerText);
    else
        *headerListP = newHeaderList;
}



static void
addContentTypeHeader(xmlrpc_env *         const envP,
                     struct curl_slist ** const headerListP) {
    
    addHeader(envP, headerListP, "Content-Type: text/xml");
}



static void
addUserAgentHeader(xmlrpc_env *         const envP,
                   struct curl_slist ** const headerListP,
                   const char *         const userAgent) {

    if (userAgent) {
        /* Note: Curl has a CURLOPT_USERAGENT option that does some of this
           work.  We prefer to be totally in control, though, so we build
           the header explicitly.
        */
    
        curl_version_info_data * const curlInfoP =
            curl_version_info(CURLVERSION_NOW);
        char curlVersion[32];
        const char * userAgentHeader;
        
        snprintf(curlVersion, sizeof(curlVersion), "%u.%u.%u",
                (curlInfoP->version_num >> 16) && 0xff,
                (curlInfoP->version_num >>  8) && 0xff,
                (curlInfoP->version_num >>  0) && 0xff
            );
                  
        xmlrpc_asprintf(&userAgentHeader,
                        "User-Agent: %s Xmlrpc-c/%s Curl/%s",
                        userAgent, XMLRPC_C_VERSION, curlVersion);
        
        if (userAgentHeader == xmlrpc_strsol)
            xmlrpc_faultf(envP, "Couldn't allocate memory for "
                          "User-Agent header");
        else {
            addHeader(envP, headerListP, userAgentHeader);
            
            xmlrpc_strfree(userAgentHeader);
        }
    }
}



static void
addAuthorizationHeader(xmlrpc_env *         const envP,
                       struct curl_slist ** const headerListP,
                       const char *         const hdrValue) {

    const char * authorizationHeader;
            
    xmlrpc_asprintf(&authorizationHeader, "Authorization: %s", hdrValue);
    
    if (authorizationHeader == xmlrpc_strsol)
        xmlrpc_faultf(envP, "Couldn't allocate memory for "
                      "Authorization header");
    else {
        addHeader(envP, headerListP, authorizationHeader);
        
        xmlrpc_strfree(authorizationHeader);
    }
}



static void
createCurlHeaderList(xmlrpc_env *               const envP,
                     const char *               const authHdrValue,
                     const char *               const userAgent,
                     struct curl_slist **       const headerListP) {

    struct curl_slist * headerList;

    headerList = NULL;  /* initial value - empty list */

    addContentTypeHeader(envP, &headerList);
    if (!envP->fault_occurred) {
        addUserAgentHeader(envP, &headerList, userAgent);
        if (!envP->fault_occurred) {
            if (authHdrValue)
                addAuthorizationHeader(envP, &headerList, authHdrValue);
        }
    }
    if (envP->fault_occurred)
        curl_slist_free_all(headerList);
    else
        *headerListP = headerList;
}



static void
assertConstantsMatch(void) {
/*----------------------------------------------------------------------------
   There are some constants that we define as part of the Xmlrpc-c
   interface that are identical to constants in the Curl interface to
   make curl option setting work.  This function asserts such
   formally.
-----------------------------------------------------------------------------*/
    assert(XMLRPC_SSLVERSION_DEFAULT == CURL_SSLVERSION_DEFAULT);
    assert(XMLRPC_SSLVERSION_TLSv1   == CURL_SSLVERSION_TLSv1);
    assert(XMLRPC_SSLVERSION_SSLv2   == CURL_SSLVERSION_SSLv2);
    assert(XMLRPC_SSLVERSION_SSLv3   == CURL_SSLVERSION_SSLv3);
}



static size_t 
collect(void *  const ptr, 
        size_t  const size, 
        size_t  const nmemb,  
        FILE  * const stream) {
/*----------------------------------------------------------------------------
   This is a Curl output function.  Curl calls this to deliver the
   HTTP response body to the Curl client.  Curl thinks it's writing to
   a POSIX stream.
-----------------------------------------------------------------------------*/
    xmlrpc_mem_block * const responseXmlP = (xmlrpc_mem_block *) stream;
    char * const buffer = ptr;
    size_t const length = nmemb * size;

    size_t retval;
    xmlrpc_env env;

    xmlrpc_env_init(&env);
    xmlrpc_mem_block_append(&env, responseXmlP, buffer, length);
    if (env.fault_occurred)
        retval = (size_t)-1;
    else
        /* Really?  Shouldn't it be like fread() and return 'nmemb'? */
        retval = length;
    
    return retval;
}



static int
curlProgress(void * const contextP,
             double const dltotal  ATTR_UNUSED,
             double const dlnow    ATTR_UNUSED,
             double const ultotal  ATTR_UNUSED,
             double const ulnow    ATTR_UNUSED) {
/*----------------------------------------------------------------------------
   This is a Curl "progress function."  It's something various Curl
   functions call every so often, including whenever something gets
   interrupted by the process receiving, and catching, a signal.
   There are two purposes of a Curl progress function: 1) lets us log
   the progress of a long-running transaction such as a big download,
   e.g. by displaying a progress bar somewhere.  In Xmlrpc-c, we don't
   implement this purpose.  2) allows us to tell the Curl function,
   via our return code, that calls it that we don't want to wait
   anymore for the operation to complete.

   In Curl versions before March 2007, we get called once per second
   and signals have no effect.  In current Curl, we usually get called
   immediately after a signal gets caught while Curl is waiting to
   receive a response from the server.  But Curl doesn't properly
   synchronize with signals, so it may miss one and then we don't get
   called until the next scheduled one-per-second call.

   All we do is tell Caller it's time to give up if the transport's
   client says it is via his "interrupt" flag.

   This function is not as important as it once was.  This module used
   to use curl_easy_perform(), which can be interrupted only via this
   progress function.  But because of the above-mentioned failure of
   Curl to properly synchronize signals (and Bryan's failure to get
   Curl developers to accept code to fix it), we now use the Curl
   "multi" facility instead and do our own pselect().  But
   This function still normally gets called by curl_multi_perform(),
   which the transport tries to call even when the user has requested
   interruption, because we don't trust our ability to abort a running
   Curl transaction.  curl_multi_perform() reliably winds up a Curl
   transaction when this function tells it to.
-----------------------------------------------------------------------------*/
    unsigned int * const interruptP = contextP;

    /* We require anyone setting us up as the Curl progress function to
       supply an interrupt flag:
    */
    assert(contextP);

    return *interruptP != 0 ? 1 : 0;
}



static void
setupAuth(xmlrpc_env *               const envP ATTR_UNUSED,
          CURL *                     const curlSessionP,
          const xmlrpc_server_info * const serverInfoP,
          const char **              const authHdrValueP) {
/*----------------------------------------------------------------------------
   Set the options in the Curl session 'curlSessionP' to set up the HTTP
   authentication described by *serverInfoP.

   But we have an odd special function for backward compatibility, because
   this code dates to a time when libcurl did not have the ability to
   handle authentication, but we provided such function nonetheless by
   building our own Authorization: header.  But we did this only for
   HTTP basic authentication.

   So the special function is this: if libcurl is too old to have
   authorization options and *serverInfoP allows basic authentication,
   return as *basicAuthHdrParamP an appropriate parameter for the
   Authorization: Basic: HTTP header.  Otherwise, return
   *basicAuthHdrParamP == NULL.
-----------------------------------------------------------------------------*/
    if (serverInfoP->allowedAuth.basic) {
        CURLcode rc;
        rc = curl_easy_setopt(curlSessionP, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);

        if (rc == CURLE_OK)
            *authHdrValueP = NULL;
        else {
            *authHdrValueP = strdup(serverInfoP->basicAuthHdrValue);
            if (*authHdrValueP == NULL)
                xmlrpc_faultf(envP, "Unable to allocate memory for basic "
                              "authentication header");
        }
    } else
        *authHdrValueP = NULL;

    /* We don't worry if libcurl is too old for these other kinds of
       authentication; they're only defined as _allowed_
       authentication methods, for when client and server are capable
       of using it, and unlike with basic authentication, we have no
       historical commitment to consider an old libcurl as capable of
       doing these.
    */
    
    if (serverInfoP->userNamePw)
        curl_easy_setopt(curlSessionP, CURLOPT_USERPWD,
                         serverInfoP->userNamePw);

    if (serverInfoP->allowedAuth.digest)
        curl_easy_setopt(
            curlSessionP, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST);
    if (serverInfoP->allowedAuth.gssnegotiate)
        curl_easy_setopt(
            curlSessionP, CURLOPT_HTTPAUTH, CURLAUTH_GSSNEGOTIATE);
    if (serverInfoP->allowedAuth.ntlm)
        curl_easy_setopt(
            curlSessionP, CURLOPT_HTTPAUTH, CURLAUTH_NTLM);
}


static void
setCurlTimeout(CURL *       const curlSessionP ATTR_UNUSED,
               unsigned int const timeout ATTR_UNUSED) {

#if HAVE_CURL_NOSIGNAL
    unsigned int const timeoutMs = (timeout + 999)/1000;

    curl_easy_setopt(curlSessionP, CURLOPT_NOSIGNAL, 1);

    assert((long)timeoutMs == (int)timeoutMs);
        /* Calling requirement */
    curl_easy_setopt(curlSessionP, CURLOPT_TIMEOUT, (long)timeoutMs);
#else
    abort();
#endif
}



static void
setupCurlSession(xmlrpc_env *               const envP,
                 curlTransaction *          const curlTransactionP,
                 xmlrpc_mem_block *         const callXmlP,
                 xmlrpc_mem_block *         const responseXmlP,
                 const xmlrpc_server_info * const serverInfoP,
                 const char *               const userAgent,
                 int *                      const interruptP,
                 const struct curlSetup *   const curlSetupP) {
/*----------------------------------------------------------------------------
   Set up the Curl session for the transaction *curlTransactionP so that
   a subsequent curl_easy_perform() would perform said transaction.

   The data curl_easy_perform() would send for that transaction would 
   be the contents of *callXmlP; the data curl_easy_perform() gets back
   would go into *responseXmlP.

   'interruptP' is a pointer to an interrupt flag -- a flag that becomes
   nonzero when the user wants to abandon this Curl session.  NULL means
   there is no interrupt flag; user will never want to abandon the session.
-----------------------------------------------------------------------------*/
    CURL * const curlSessionP = curlTransactionP->curlSessionP;

    assertConstantsMatch();

    curl_easy_setopt(curlSessionP, CURLOPT_POST, 1);
    curl_easy_setopt(curlSessionP, CURLOPT_URL, curlTransactionP->serverUrl);

    XMLRPC_MEMBLOCK_APPEND(char, envP, callXmlP, "\0", 1);
    if (!envP->fault_occurred) {
        curl_easy_setopt(curlSessionP, CURLOPT_POSTFIELDS, 
                         XMLRPC_MEMBLOCK_CONTENTS(char, callXmlP));
        curl_easy_setopt(curlSessionP, CURLOPT_WRITEFUNCTION, collect);
        curl_easy_setopt(curlSessionP, CURLOPT_FILE, responseXmlP);
        curl_easy_setopt(curlSessionP, CURLOPT_HEADER, 0);
        curl_easy_setopt(curlSessionP, CURLOPT_ERRORBUFFER, 
                         curlTransactionP->curlError);
        if (interruptP) {
            curl_easy_setopt(curlSessionP, CURLOPT_NOPROGRESS, 0);
            curl_easy_setopt(curlSessionP, CURLOPT_PROGRESSFUNCTION,
                             curlProgress);
            curl_easy_setopt(curlSessionP, CURLOPT_PROGRESSDATA, interruptP);
        } else
            curl_easy_setopt(curlSessionP, CURLOPT_NOPROGRESS, 1);
        
        curl_easy_setopt(curlSessionP, CURLOPT_SSL_VERIFYPEER,
                         curlSetupP->sslVerifyPeer);
        curl_easy_setopt(curlSessionP, CURLOPT_SSL_VERIFYHOST,
                         curlSetupP->sslVerifyHost ? 2 : 0);

        if (curlSetupP->networkInterface)
            curl_easy_setopt(curlSessionP, CURLOPT_INTERFACE,
                             curlSetupP->networkInterface);
        if (curlSetupP->sslCert)
            curl_easy_setopt(curlSessionP, CURLOPT_SSLCERT,
                             curlSetupP->sslCert);
        if (curlSetupP->sslCertType)
            curl_easy_setopt(curlSessionP, CURLOPT_SSLCERTTYPE,
                             curlSetupP->sslCertType);
        if (curlSetupP->sslCertPasswd)
            curl_easy_setopt(curlSessionP, CURLOPT_SSLCERTPASSWD,
                             curlSetupP->sslCertPasswd);
        if (curlSetupP->sslKey)
            curl_easy_setopt(curlSessionP, CURLOPT_SSLKEY,
                             curlSetupP->sslKey);
        if (curlSetupP->sslKeyType)
            curl_easy_setopt(curlSessionP, CURLOPT_SSLKEYTYPE,
                             curlSetupP->sslKeyType);
        if (curlSetupP->sslKeyPasswd)
            curl_easy_setopt(curlSessionP, CURLOPT_SSLKEYPASSWD,
                             curlSetupP->sslKeyPasswd);
        if (curlSetupP->sslEngine)
            curl_easy_setopt(curlSessionP, CURLOPT_SSLENGINE,
                             curlSetupP->sslEngine);
        if (curlSetupP->sslEngineDefault)
            /* 3rd argument seems to be required by some Curl */
            curl_easy_setopt(curlSessionP, CURLOPT_SSLENGINE_DEFAULT, 1l);
        if (curlSetupP->sslVersion != XMLRPC_SSLVERSION_DEFAULT)
            curl_easy_setopt(curlSessionP, CURLOPT_SSLVERSION,
                             curlSetupP->sslVersion);
        if (curlSetupP->caInfo)
            curl_easy_setopt(curlSessionP, CURLOPT_CAINFO,
                             curlSetupP->caInfo);
        if (curlSetupP->caPath)
            curl_easy_setopt(curlSessionP, CURLOPT_CAPATH,
                             curlSetupP->caPath);
        if (curlSetupP->randomFile)
            curl_easy_setopt(curlSessionP, CURLOPT_RANDOM_FILE,
                             curlSetupP->randomFile);
        if (curlSetupP->egdSocket)
            curl_easy_setopt(curlSessionP, CURLOPT_EGDSOCKET,
                             curlSetupP->egdSocket);
        if (curlSetupP->sslCipherList)
            curl_easy_setopt(curlSessionP, CURLOPT_SSL_CIPHER_LIST,
                             curlSetupP->sslCipherList);

        if (curlSetupP->timeout)
            setCurlTimeout(curlSessionP, curlSetupP->timeout);

        {
            const char * authHdrValue;
                /* NULL means we don't have to construct an explicit
                   Authorization: header.  non-null means we have to
                   construct one with this as its value.
                */

            setupAuth(envP, curlSessionP, serverInfoP, &authHdrValue);
            if (!envP->fault_occurred) {
                struct curl_slist * headerList;
                createCurlHeaderList(envP, authHdrValue, userAgent,
                                     &headerList);
                if (!envP->fault_occurred) {
                    curl_easy_setopt(
                        curlSessionP, CURLOPT_HTTPHEADER, headerList);
                    curlTransactionP->headerList = headerList;
                }
                if (authHdrValue)
                    xmlrpc_strfree(authHdrValue);
            }
        }
    }
}



static void
curlTransaction_create(xmlrpc_env *               const envP,
                       CURL *                     const curlSessionP,
                       const xmlrpc_server_info * const serverP,
                       xmlrpc_mem_block *         const callXmlP,
                       xmlrpc_mem_block *         const responseXmlP,
                       const char *               const userAgent,
                       const struct curlSetup *   const curlSetupStuffP,
                       rpc *                      const rpcP,
                       int *                      const interruptP,
                       curlTransaction **         const curlTransactionPP) {

    curlTransaction * curlTransactionP;

    MALLOCVAR(curlTransactionP);
    if (curlTransactionP == NULL)
        xmlrpc_faultf(envP, "No memory to create Curl transaction.");
    else {
        curlTransactionP->finish       = NULL;
        curlTransactionP->curlSessionP = curlSessionP;
        curlTransactionP->rpcP         = rpcP;

        curlTransactionP->serverUrl = strdup(serverP->serverUrl);
        if (curlTransactionP->serverUrl == NULL)
            xmlrpc_faultf(envP, "Out of memory to store server URL.");
        else {
            setupCurlSession(envP, curlTransactionP,
                             callXmlP, responseXmlP,
                             serverP, userAgent, interruptP,
                             curlSetupStuffP);
            
            if (envP->fault_occurred)
                xmlrpc_strfree(curlTransactionP->serverUrl);
        }
        if (envP->fault_occurred)
            free(curlTransactionP);
    }
    *curlTransactionPP = curlTransactionP;
}



static void
curlTransaction_destroy(curlTransaction * const curlTransactionP) {

    curl_slist_free_all(curlTransactionP->headerList);
    xmlrpc_strfree(curlTransactionP->serverUrl);

    free(curlTransactionP);
}



/*=============================================================================
    curlMulti
=============================================================================*/

typedef struct {
/*----------------------------------------------------------------------------
   This is an extension to Curl's CURLM object.  The extensions are:

   1) It has a lock so multiple threads can use it simultaneously.

   2) Its "select" file descriptor vectors are self-contained.  CURLM
      requires the user to maintain them separately.

   Plus, it manages Curl transactions (curlTransaction) instead of Curl
   sessions (CURL).
-----------------------------------------------------------------------------*/
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
} curlMulti;



static curlMulti *
curlMulti_create(void) {

    curlMulti * retval;
    curlMulti * curlMultiP;

    MALLOCVAR(curlMultiP);

    if (curlMultiP == NULL)
        retval = NULL;
    else {
        curlMultiP->lockP = createLock_pthread();

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



static void
curlMulti_destroy(curlMulti * const curlMultiP) {

    curl_multi_cleanup(curlMultiP->curlMultiP);
    
    curlMultiP->lockP->destroy(curlMultiP->lockP);

    free(curlMultiP);
}



static void
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

    curlMultiP->lockP->lock(curlMultiP->lockP);

    rc = curl_multi_perform(curlMultiP->curlMultiP, runningHandlesP);

    curlMultiP->lockP->unlock(curlMultiP->lockP);

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



static void
curlMulti_addHandle(xmlrpc_env *       const envP,
                    curlMulti *        const curlMultiP,
                    curlTransaction  * const curlTransactionP) {

    CURLMcode rc;

    curlMultiP->lockP->lock(curlMultiP->lockP);

    rc = curl_multi_add_handle(curlMultiP->curlMultiP,
                               curlTransactionP->curlSessionP);
    
    curlMultiP->lockP->unlock(curlMultiP->lockP);

    if (rc != CURLM_OK) {
        const char * reason;
        interpretCurlMultiError(&reason, rc);
        xmlrpc_faultf(envP, "Could not add Curl session to the "
                      "curl multi manager.  curl_multi_add_handle() "
                      "failed: %s", reason);
        xmlrpc_strfree(reason);
    }
}


static void
curlMulti_removeHandle(curlMulti *       const curlMultiP,
                       curlTransaction * const curlTransactionP) {

    curlMultiP->lockP->lock(curlMultiP->lockP);

    curl_multi_remove_handle(curlMultiP->curlMultiP,
                             curlTransactionP->curlSessionP);
    
    curlMultiP->lockP->unlock(curlMultiP->lockP);
}



static void
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

    curlMultiP->lockP->lock(curlMultiP->lockP);
    
    privateCurlMsgP = curl_multi_info_read(curlMultiP->curlMultiP,
                                           &remainingMsgCount);
        
    if (privateCurlMsgP == NULL)
        *endOfMessagesP = true;
    else {
        *endOfMessagesP = false;
        *curlMsgP = *privateCurlMsgP;
    }    
    curlMultiP->lockP->unlock(curlMultiP->lockP);
}



static void
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
    
    curlMultiP->lockP->lock(curlMultiP->lockP);

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

    curlMultiP->lockP->unlock(curlMultiP->lockP);

    if (rc != CURLM_OK) {
        const char * reason;
        interpretCurlMultiError(&reason, rc);
        xmlrpc_faultf(envP, "Impossible failure of curl_multi_fdset(): %s",
                      reason);
        xmlrpc_strfree(reason);
    }
}



static void
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

    selectTimeoutMillisec = 0; /* quiet compiler warning */

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
    }
    break;
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
                                  &curlTransactionP);

                curlTransactionP->result = curlMsg.data.result;

                if (curlTransactionP->finish)
                    curlTransactionP->finish(envP, curlTransactionP);
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
curlMulti_finish(xmlrpc_env *       const envP,
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



/*===========================================================================*/


static void
startCurlTransaction(xmlrpc_env *      const envP,
                     curlTransaction * const curlTransactionP,
                     curlMulti *       const curlMultiP,
                     finishCurlTransactionFn finish) {

    curlTransactionP->finish = finish;

    /* A Curl session is serial -- it processes zero or one transaction
       at a time.  We use the "private" attribute of the Curl session to
       indicate which transaction it is presently processing.  This is
       important when the transaction finishes, because libcurl will just
       tell us that something finished on a particular session, not that
       a particular transaction finished.
    */
    curl_easy_setopt(curlTransactionP->curlSessionP, CURLOPT_PRIVATE,
                     curlTransactionP);

    curlMulti_addHandle(envP, curlMultiP, curlTransactionP);
}



static void
getCurlTransactionError(curlTransaction * const curlTransactionP,
                        xmlrpc_env *      const envP) {

    if (curlTransactionP->result != CURLE_OK) {
        /* We've seen Curl just return a null string for an explanation
           (e.g. when TCP connect() fails because IP address doesn't exist).
        */
        const char * explanation;

        if (strlen(curlTransactionP->curlError) == 0)
            interpretCurlEasyError(&explanation, curlTransactionP->result);
        else
            xmlrpc_asprintf(&explanation, "%s", curlTransactionP->curlError);

        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_NETWORK_ERROR, "libcurl failed to execute the "
            "HTTP POST transaction.  %s", explanation);

        xmlrpc_strfree(explanation);
    } else {
        CURLcode res;
        long http_result;
        
        res = curl_easy_getinfo(curlTransactionP->curlSessionP,
                                CURLINFO_HTTP_CODE, &http_result);
    
        if (res != CURLE_OK)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INTERNAL_ERROR, 
                "Curl performed the HTTP POST request, but was "
                "unable to say what the HTTP result code was.  "
                "curl_easy_getinfo(CURLINFO_HTTP_CODE) says: %s", 
                curlTransactionP->curlError);
        else {
            if (http_result != 200)
                xmlrpc_env_set_fault_formatted(
                    envP, XMLRPC_NETWORK_ERROR,
                    "HTTP response code is %ld, not 200",
                    http_result);
        }
    }
}



static void
performCurlTransaction(xmlrpc_env *      const envP,
                       curlTransaction * const curlTransactionP,
                       curlMulti *       const curlMultiP,
                       int *             const interruptP) {

    void * const finish = NULL;
        /* We don't need a finish function because we're going to wait here
           for the transaction to complete and then do the next step
           ourselves.
        */

    startCurlTransaction(envP, curlTransactionP, curlMultiP, finish);

    /* Failure here just means something screwy in the multi manager;
       Above does not even begin to perform the HTTP transaction
    */

    if (!envP->fault_occurred) {
        xmlrpc_timespec const dummy = {0,0};

        curlMulti_finish(envP, curlMultiP, timeout_no, dummy, interruptP);

        /* Failure here just means something screwy in the multi
           manager; any failure of the HTTP transaction would have been
           recorded in *curlTransactionP.
        */

        if (!envP->fault_occurred) {
            /* Curl session completed OK.  But did HTTP transaction
               work?
            */
            getCurlTransactionError(curlTransactionP, envP);
        }
        /* If the CURL transaction is still going, removing the handle
           here aborts it.  At least it's supposed to.  From what I've
           seen in the Curl code in 2007, I don't think it does.  I
           couldn't get Curl maintainers interested in the problem,
           except to say, "If you're right, there's a bug."
        */
        curlMulti_removeHandle(curlMultiP, curlTransactionP);
    }
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
    const char * userAgent;
        /* Prefix for the User-Agent HTTP header, reflecting facilities
           outside of Xmlrpc-c.  The actual User-Agent header consists
           of this prefix plus information about Xmlrpc-c.  NULL means
           none.

           This is constant.
        */
    struct curlSetup curlSetupStuff;
        /* This is constant */
    int * interruptP;
        /* Pointer to a value that user sets to nonzero to indicate he wants
           the transport to give up on whatever it is doing and return ASAP.

           NULL means none -- transport never gives up.
        */
};



struct rpc {
    struct xmlrpc_client_transport * transportP;
        /* The client XML transport that transports this RPC */
    curlTransaction * curlTransactionP;
        /* The object which does the HTTP transaction, with no knowledge
           of XML-RPC or Xmlrpc-c.
        */
    xmlrpc_mem_block * responseXmlP;
        /* Where the response XML for this RPC should go or has gone. */
    xmlrpc_transport_asynch_complete complete;
        /* Routine to call to complete the RPC after it is complete HTTP-wise.
           NULL if none.
        */
    struct xmlrpc_call_info * callInfoP;
        /* User's identifier for this RPC */
};


static void
lockSyncCurlSession(struct xmlrpc_client_transport * const transportP) {
    transportP->syncCurlSessionLockP->lock(transportP->syncCurlSessionLockP);
}



static void
unlockSyncCurlSession(struct xmlrpc_client_transport * const transportP) {
    transportP->syncCurlSessionLockP->unlock(transportP->syncCurlSessionLockP);
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
            if ((curlXportParmsP->timeout + 999) / 1000 > LONG_MAX)
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

    transportP->syncCurlSessionLockP = createLock_pthread();
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

    assertConstantsMatch();

    MALLOCVAR(transportP);
    if (transportP == NULL)
        xmlrpc_faultf(envP, "Unable to allocate transport descriptor.");
    else {
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
createRpc(xmlrpc_env *                     const envP,
          struct xmlrpc_client_transport * const clientTransportP,
          CURL *                           const curlSessionP,
          const xmlrpc_server_info *       const serverP,
          xmlrpc_mem_block *               const callXmlP,
          xmlrpc_mem_block *               const responseXmlP,
          xmlrpc_transport_asynch_complete       complete, 
          struct xmlrpc_call_info *        const callInfoP,
          rpc **                           const rpcPP) {

    rpc * rpcP;

    MALLOCVAR(rpcP);
    if (rpcP == NULL)
        xmlrpc_faultf(envP, "Couldn't allocate memory for rpc object");
    else {
        rpcP->transportP   = clientTransportP;
        rpcP->callInfoP    = callInfoP;
        rpcP->complete     = complete;
        rpcP->responseXmlP = responseXmlP;

        curlTransaction_create(envP,
                               curlSessionP,
                               serverP,
                               callXmlP, responseXmlP, 
                               clientTransportP->userAgent,
                               &clientTransportP->curlSetupStuff,
                               rpcP,
                               clientTransportP->interruptP,
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



static finishCurlTransactionFn finishRpcCurlTransaction;

static void
finishRpcCurlTransaction(xmlrpc_env *      const envP ATTR_UNUSED,
                         curlTransaction * const curlTransactionP) {
/*----------------------------------------------------------------------------
  Handle the event that a Curl transaction for an asynchronous RPC has
  completed on the Curl session identified by 'curlSessionP'.

  Tell the requester of the RPC the results.

  Remove the Curl session from its Curl multi manager and destroy the
  Curl session, the XML response buffer, the Curl transaction, and the RPC.
-----------------------------------------------------------------------------*/
    rpc * const rpcP = curlTransactionP->rpcP;
    struct xmlrpc_client_transport * const transportP = rpcP->transportP;

    {
        xmlrpc_env env;

        xmlrpc_env_init(&env);

        getCurlTransactionError(curlTransactionP, &env);

        rpcP->complete(rpcP->callInfoP, rpcP->responseXmlP, env);

        xmlrpc_env_clean(&env);
    }

    curlMulti_removeHandle(transportP->asyncCurlMultiP, curlTransactionP);

    curl_easy_cleanup(curlTransactionP->curlSessionP);

    XMLRPC_MEMBLOCK_FREE(char, rpcP->responseXmlP);

    destroyRpc(rpcP);
}



static void
startRpc(xmlrpc_env * const envP,
         rpc *        const rpcP) {

    startCurlTransaction(envP,
                         rpcP->curlTransactionP,
                         rpcP->transportP->asyncCurlMultiP,
                         &finishRpcCurlTransaction);
}



static void 
sendRequest(xmlrpc_env *                     const envP, 
            struct xmlrpc_client_transport * const clientTransportP,
            const xmlrpc_server_info *       const serverP,
            xmlrpc_mem_block *               const callXmlP,
            xmlrpc_transport_asynch_complete       complete,
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
                      callXmlP, responseXmlP, complete, callInfoP,
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

    curlMulti_finish(&env, clientTransportP->asyncCurlMultiP,
                     timeoutType, waitTimeoutTime,
                     clientTransportP->interruptP);

    /* If the above fails, it is catastrophic, because it means there is
       no way to complete outstanding Curl transactions and RPCs, and
       no way to release their resources.

       We should at least expand this interface some day to push the
       problem back up the user, but for now we just do this Hail Mary
       response.

       Note that a failure of curlMult_finish() does not mean that
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
                  NULL, NULL,
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
