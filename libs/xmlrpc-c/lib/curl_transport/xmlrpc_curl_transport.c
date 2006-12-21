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
   any other Curl library function.

   But note that one of those global variables tells that
   curl_global_init() has been called, and if you call it again,
   it's just a no-op.  So we can call it for each of many transport
   object constructors.

   The actual global variables appear to be constant so that it's OK
   for them to be shared among multiple objects and threads.  And we never
   want other than the defaults.  They're things like the identity of
   the malloc routine.

   Note that not only could many Xmlrpc-c Curl XML transport
   objects be using Curl in the same process, but the Xmlrpc-c user may
   have uses of his own.  So this whole fragile thing works only as long
   as the user doesn't need to set these global variables to something
   different from the defaults (because those are what we use).

   curl_global_cleanup() reverts the process back to a state in which
   nobody can use the Curl library.  So we can't ever call it.  This
   means there will be some memory leakage, but since curl_global_init
   only ever has effect once, the amount of leakage is trivial.  The
   user can do his own curl_global_cleanup() if he really cares.

   Some of the Curl global variables are actually the SSL library global
   variables.  The SSL library has the same disease.
-----------------------------------------------------------------------------*/


#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "xmlrpc_config.h"

#include "bool.h"
#include "mallocvar.h"
#include "linklist.h"
#include "sstring.h"
#include "casprintf.h"
#include "pthreadx.h"
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/client.h"
#include "xmlrpc-c/client_int.h"
#include "version.h"

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#if defined (WIN32) && defined(_DEBUG)
#  include <crtdbg.h>
#  define new DEBUG_NEW
#  define malloc(size) _malloc_dbg( size, _NORMAL_BLOCK, __FILE__, __LINE__)
#  undef THIS_FILE
   static char THIS_FILE[] = __FILE__;
#endif /*WIN32 && _DEBUG*/



struct xmlrpc_client_transport {
    pthread_mutex_t listLock;
    struct list_head rpcList;
        /* List of all RPCs that exist for this transport.  An RPC exists
           from the time the user requests it until the time the user 
           acknowledges it is done.
        */
    CURL * syncCurlSessionP;
        /* Handle for a Curl library session object that we use for
           all synchronous RPCs.  An async RPC has one of its own,
           and consequently does not share things such as persistent
           connections and cookies with any other RPC.
        */
    pthread_mutex_t syncCurlSessionLock;
        /* Hold this lock while accessing or using *syncCurlSessionP.
           You're using the session from the time you set any
           attributes in it or start a transaction with it until any
           transaction has finished and you've lost interest in any
           attributes of the session.
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
    const char * userAgent;
        /* Prefix for the User-Agent HTTP header, reflecting facilities
           outside of Xmlrpc-c.  The actual User-Agent header consists
           of this prefix plus information about Xmlrpc-c.  NULL means
           none.
        */
};

typedef struct {
    /* This is all stuff that really ought to be in a Curl object,
       but the Curl library is a little too simple for that.  So we
       build a layer on top of Curl, and define this "transaction," as
       an object subordinate to a Curl "session."
       */
    CURL * curlSessionP;
        /* Handle for the Curl session that hosts this transaction */
    char curlError[CURL_ERROR_SIZE];
        /* Error message from Curl */
    struct curl_slist * headerList;
        /* The HTTP headers for the transaction */
    const char * serverUrl;  /* malloc'ed - belongs to this object */
} curlTransaction;



typedef struct {
    struct list_head link;  /* link in transport's list of RPCs */
    CURL * curlSessionP;
        /* The Curl session we use for this transaction.  Note that only
           one RPC at a time can use a particular Curl session, so this
           had better not be a session that some other RPC is using
           simultaneously.
        */
    curlTransaction * curlTransactionP;
        /* The object which does the HTTP transaction, with no knowledge
           of XML-RPC or Xmlrpc-c.
        */
    xmlrpc_mem_block * responseXmlP;
    xmlrpc_bool threadExists;
    pthread_t thread;
    xmlrpc_transport_asynch_complete complete;
        /* Routine to call to complete the RPC after it is complete HTTP-wise.
           NULL if none.
        */
    struct xmlrpc_call_info * callInfoP;
        /* User's identifier for this RPC */
} rpc;



static size_t 
collect(void *  const ptr, 
        size_t  const size, 
        size_t  const nmemb,  
        FILE  * const stream) {
/*----------------------------------------------------------------------------
   This is a Curl output function.  Curl calls this to deliver the
   HTTP response body.  Curl thinks it's writing to a POSIX stream.
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



static void
initWindowsStuff(xmlrpc_env * const envP) {

#if defined (WIN32)
    /* This is CRITICAL so that cURL-Win32 works properly! */
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
#else
    if (0)
        envP->fault_occurred = TRUE;  /* Avoid unused parm warning */
#endif
}



static void
getXportParms(xmlrpc_env *  const envP,
              const struct xmlrpc_curl_xportparms * const curlXportParmsP,
              size_t        const parmSize,
              const char ** const networkInterfaceP,
              xmlrpc_bool * const sslVerifyPeerP,
              xmlrpc_bool * const sslVerifyHostP,
              const char ** const userAgentP) {

    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(user_agent))
        *userAgentP = NULL;
    else if (curlXportParmsP->user_agent == NULL)
        *userAgentP = NULL;
    else
        *userAgentP = strdup(curlXportParmsP->user_agent);
    
    if (!curlXportParmsP || 
        parmSize < XMLRPC_CXPSIZE(no_ssl_verifypeer))
        *sslVerifyPeerP = TRUE;
    else
        *sslVerifyPeerP = !curlXportParmsP->no_ssl_verifypeer;
    
    if (!curlXportParmsP || 
        parmSize < XMLRPC_CXPSIZE(no_ssl_verifyhost))
        *sslVerifyHostP = TRUE;
    else
        *sslVerifyHostP = !curlXportParmsP->no_ssl_verifyhost;
    
    if (!curlXportParmsP || parmSize < XMLRPC_CXPSIZE(network_interface))
        *networkInterfaceP = NULL;
    else if (curlXportParmsP->network_interface == NULL)
        *networkInterfaceP = NULL;
    else {
        *networkInterfaceP = strdup(curlXportParmsP->network_interface);
        if (*networkInterfaceP == NULL)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INTERNAL_ERROR,
                "Unable to allocate space for network interface name.");
        if (envP->fault_occurred)
            strfree(*networkInterfaceP);
    }
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

   That leaves things such as cookies that don't exist for asynchronous
   transactions, and are common to multiple serial synchronous
   transactions.
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
create(xmlrpc_env *                      const envP,
       int                               const flags ATTR_UNUSED,
       const char *                      const appname ATTR_UNUSED,
       const char *                      const appversion ATTR_UNUSED,
       const struct xmlrpc_xportparms *  const transportparmsP,
       size_t                            const parm_size,
       struct xmlrpc_client_transport ** const handlePP) {
/*----------------------------------------------------------------------------
   This does the 'create' operation for a Curl client transport.
-----------------------------------------------------------------------------*/
    struct xmlrpc_curl_xportparms * const curlXportParmsP = 
        (struct xmlrpc_curl_xportparms *) transportparmsP;

    struct xmlrpc_client_transport * transportP;

    initWindowsStuff(envP);

    MALLOCVAR(transportP);
    if (transportP == NULL)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_INTERNAL_ERROR, 
            "Unable to allocate transport descriptor.");
    else {
        pthread_mutex_init(&transportP->listLock, NULL);
        
        list_make_empty(&transportP->rpcList);

        /*
         * This is the main global constructor for the app. Call this before
         * _any_ libcurl usage. If this fails, *NO* libcurl functions may be
         * used, or havoc may be the result.
         */
        curl_global_init(CURL_GLOBAL_ALL);

        /* The above makes it look like Curl is not re-entrant.  We should
           check into that.
        */

        getXportParms(envP, curlXportParmsP, parm_size,
                      &transportP->networkInterface,
                      &transportP->sslVerifyPeer,
                      &transportP->sslVerifyHost,
                      &transportP->userAgent);

        if (!envP->fault_occurred) {
            pthread_mutex_init(&transportP->syncCurlSessionLock, NULL);
            createSyncCurlSession(envP, &transportP->syncCurlSessionP);

            if (envP->fault_occurred)
                strfree(transportP->networkInterface);
        }                 
        if (envP->fault_occurred)
            free(transportP);
    }
    *handlePP = transportP;
}



static void
termWindowStuff(void) {

#if defined (WIN32)
    WSACleanup();
#endif
}



static void 
destroy(struct xmlrpc_client_transport * const clientTransportP) {
/*----------------------------------------------------------------------------
   This does the 'destroy' operation for a Curl client transport.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT(clientTransportP != NULL);

    XMLRPC_ASSERT(list_is_empty(&clientTransportP->rpcList));

    destroySyncCurlSession(clientTransportP->syncCurlSessionP);
    pthread_mutex_destroy(&clientTransportP->syncCurlSessionLock);

    if (clientTransportP->networkInterface)
        strfree(clientTransportP->networkInterface);
    if (clientTransportP->userAgent)
        strfree(clientTransportP->userAgent);

    /* We want to call curl_global_cleanup() now, but can't.  See
       explanation of Curl global variables at the top of this file.
    */

    pthread_mutex_destroy(&clientTransportP->listLock);

    termWindowStuff();

    free(clientTransportP);
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
        curl_version_info_data * const curlInfoP =
            curl_version_info(CURLVERSION_NOW);
        char curlVersion[32];
        const char * userAgentHeader;
        
        snprintf(curlVersion, sizeof(curlVersion), "%u.%u.%u",
                (curlInfoP->version_num >> 16) && 0xff,
                (curlInfoP->version_num >>  8) && 0xff,
                (curlInfoP->version_num >>  0) && 0xff
            );
                  
        casprintf(&userAgentHeader,
                  "User-Agent: %s Xmlrpc-c/%s Curl/%s",
                  userAgent, XMLRPC_C_VERSION, curlVersion);
        
        if (userAgentHeader == NULL)
            xmlrpc_faultf(envP, "Couldn't allocate memory for "
                          "User-Agent header");
        else {
            addHeader(envP, headerListP, userAgentHeader);
            
            strfree(userAgentHeader);
        }
    }
}



static void
addAuthorizationHeader(xmlrpc_env *         const envP,
                       struct curl_slist ** const headerListP,
                       const char *         const basicAuthInfo) {

    if (basicAuthInfo) {
        const char * authorizationHeader;
            
        casprintf(&authorizationHeader, "Authorization: %s", basicAuthInfo);
            
        if (authorizationHeader == NULL)
            xmlrpc_faultf(envP, "Couldn't allocate memory for "
                          "Authorization header");
        else {
            addHeader(envP, headerListP, authorizationHeader);

            strfree(authorizationHeader);
        }
    }
}



static void
createCurlHeaderList(xmlrpc_env *               const envP,
                     const xmlrpc_server_info * const serverP,
                     const char *               const userAgent,
                     struct curl_slist **       const headerListP) {

    struct curl_slist * headerList;

    headerList = NULL;  /* initial value - empty list */

    addContentTypeHeader(envP, &headerList);
    if (!envP->fault_occurred) {
        addUserAgentHeader(envP, &headerList, userAgent);
        if (!envP->fault_occurred) {
            addAuthorizationHeader(envP, &headerList, 
                                   serverP->_http_basic_auth);
        }
    }
    if (envP->fault_occurred)
        curl_slist_free_all(headerList);
    else
        *headerListP = headerList;
}



static void
setupCurlSession(xmlrpc_env *       const envP,
                 curlTransaction *  const curlTransactionP,
                 xmlrpc_mem_block * const callXmlP,
                 xmlrpc_mem_block * const responseXmlP,
                 const char *       const networkInterface,
                 xmlrpc_bool        const sslVerifyPeer,
                 xmlrpc_bool        const sslVerifyHost) {

    CURL * const curlSessionP = curlTransactionP->curlSessionP;

    curl_easy_setopt(curlSessionP, CURLOPT_POST, 1);
    if (networkInterface)
        curl_easy_setopt(curlSessionP, CURLOPT_INTERFACE, networkInterface);
    curl_easy_setopt(curlSessionP, CURLOPT_URL, curlTransactionP->serverUrl);
    curl_easy_setopt(curlSessionP, CURLOPT_SSL_VERIFYPEER, sslVerifyPeer);
    curl_easy_setopt(curlSessionP, CURLOPT_SSL_VERIFYHOST,
                     sslVerifyHost ? 2 : 0);
    XMLRPC_MEMBLOCK_APPEND(char, envP, callXmlP, "\0", 1);
    if (!envP->fault_occurred) {
        curl_easy_setopt(curlSessionP, CURLOPT_POSTFIELDS, 
                         XMLRPC_MEMBLOCK_CONTENTS(char, callXmlP));
        
        curl_easy_setopt(curlSessionP, CURLOPT_FILE, responseXmlP);
        curl_easy_setopt(curlSessionP, CURLOPT_HEADER, 0 );
        curl_easy_setopt(curlSessionP, CURLOPT_WRITEFUNCTION, collect);
        curl_easy_setopt(curlSessionP, CURLOPT_ERRORBUFFER, 
                         curlTransactionP->curlError);
        curl_easy_setopt(curlSessionP, CURLOPT_NOPROGRESS, 1);
        
        curl_easy_setopt(curlSessionP, CURLOPT_HTTPHEADER, 
                         curlTransactionP->headerList);
    }
}



static void
createCurlTransaction(xmlrpc_env *               const envP,
                      CURL *                     const curlSessionP,
                      const xmlrpc_server_info * const serverP,
                      xmlrpc_mem_block *         const callXmlP,
                      xmlrpc_mem_block *         const responseXmlP,
                      const char *               const networkInterface,
                      xmlrpc_bool                const sslVerifyPeer,
                      xmlrpc_bool                const sslVerifyHost,
                      const char *               const userAgent,
                      curlTransaction **         const curlTransactionPP) {

    curlTransaction * curlTransactionP;

    MALLOCVAR(curlTransactionP);
    if (curlTransactionP == NULL)
        xmlrpc_faultf(envP, "No memory to create Curl transaction.");
    else {
        curlTransactionP->curlSessionP = curlSessionP;

        curlTransactionP->serverUrl = strdup(serverP->_server_url);
        if (curlTransactionP->serverUrl == NULL)
            xmlrpc_faultf(envP, "Out of memory to store server URL.");
        else {
            createCurlHeaderList(envP, serverP, userAgent,
                                 &curlTransactionP->headerList);
            
            if (!envP->fault_occurred)
                setupCurlSession(envP, curlTransactionP,
                                 callXmlP, responseXmlP,
                                 networkInterface, 
                                 sslVerifyPeer, sslVerifyHost);
            
            if (envP->fault_occurred)
                strfree(curlTransactionP->serverUrl);
        }
        if (envP->fault_occurred)
            free(curlTransactionP);
    }
    *curlTransactionPP = curlTransactionP;
}



static void
destroyCurlTransaction(curlTransaction * const curlTransactionP) {

    curl_slist_free_all(curlTransactionP->headerList);
    strfree(curlTransactionP->serverUrl);

    free(curlTransactionP);
}



static void
performCurlTransaction(xmlrpc_env *      const envP,
                       curlTransaction * const curlTransactionP) {

    CURL * const curlSessionP = curlTransactionP->curlSessionP;

    CURLcode res;

    res = curl_easy_perform(curlSessionP);
    
    if (res != CURLE_OK)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_NETWORK_ERROR, "Curl failed to perform "
            "HTTP POST request.  curl_easy_perform() says: %s", 
            curlTransactionP->curlError);
    else {
        CURLcode res;
        long http_result;
        res = curl_easy_getinfo(curlSessionP, CURLINFO_HTTP_CODE, 
                                &http_result);

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
                    envP, XMLRPC_NETWORK_ERROR, "HTTP response: %ld",
                    http_result);
        }
    }
}



static void
doAsyncRpc2(void * const arg) {

    rpc * const rpcP = arg;

    xmlrpc_env env;

    xmlrpc_env_init(&env);

    performCurlTransaction(&env, rpcP->curlTransactionP);

    rpcP->complete(rpcP->callInfoP, rpcP->responseXmlP, env);

    xmlrpc_env_clean(&env);
}



#ifdef WIN32

static unsigned __stdcall 
doAsyncRpc(void * arg) {
    doAsyncRpc2(arg);
    return 0;
}

#else
static void *
doAsyncRpc(void * arg) {
    doAsyncRpc2(arg);
    return NULL;
}

#endif



static void
createThread(xmlrpc_env * const envP,
             void * (*threadRoutine)(void *),
             rpc *        const rpcP,
             pthread_t *  const threadP) {

    int rc;

    rc = pthread_create(threadP, NULL, threadRoutine, rpcP);
    switch (rc) {
    case 0: 
        break;
    case EAGAIN:
        xmlrpc_faultf(envP, "pthread_create() failed.  "
                      "System resources exceeded.");
        break;
    case EINVAL:
        xmlrpc_faultf(envP, "pthread_create() failed.  "
                      "Parameter error");
        break;
    case ENOMEM:
        xmlrpc_faultf(envP, "pthread_create() failed.  "
                      "No memory for new thread.");
        break;
    default:
        xmlrpc_faultf(envP, "pthread_create() failed.  "
                      "Unrecognized error code %d.", rc);
        break;
    }
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
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_INTERNAL_ERROR,
            "Couldn't allocate memory for rpc object");
    else {
        rpcP->callInfoP = callInfoP;
        rpcP->complete  = complete;
        rpcP->responseXmlP = responseXmlP;
        rpcP->threadExists = FALSE;

        rpcP->curlSessionP = curlSessionP;
        createCurlTransaction(envP,
                              curlSessionP,
                              serverP,
                              callXmlP, responseXmlP, 
                              clientTransportP->networkInterface, 
                              clientTransportP->sslVerifyPeer,
                              clientTransportP->sslVerifyHost,
                              clientTransportP->userAgent,
                              &rpcP->curlTransactionP);
        if (!envP->fault_occurred) {
            list_init_header(&rpcP->link, rpcP);
            pthread_mutex_lock(&clientTransportP->listLock);
            list_add_head(&clientTransportP->rpcList, &rpcP->link);
            pthread_mutex_unlock(&clientTransportP->listLock);
            
            if (envP->fault_occurred)
                destroyCurlTransaction(rpcP->curlTransactionP);
        }
        if (envP->fault_occurred)
            free(rpcP);
    }
    *rpcPP = rpcP;
}



static void 
destroyRpc(rpc * const rpcP) {

    XMLRPC_ASSERT_PTR_OK(rpcP);
    XMLRPC_ASSERT(!rpcP->threadExists);

    destroyCurlTransaction(rpcP->curlTransactionP);

    list_remove(&rpcP->link);

    free(rpcP);
}



static void
performRpc(xmlrpc_env * const envP,
           rpc *        const rpcP) {

    performCurlTransaction(envP, rpcP->curlTransactionP);
}



static void
startRpc(xmlrpc_env * const envP,
         rpc *        const rpcP) {

    createThread(envP, &doAsyncRpc, rpcP, &rpcP->thread);
    if (!envP->fault_occurred)
        rpcP->threadExists = TRUE;
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
                      callXmlP, responseXmlP,
                      complete, callInfoP,
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
    /* The user's eventual finish_asynch call will destroy this RPC,
       Curl session, and response buffer
    */
}



static void * 
finishRpc(struct list_head * const headerP, 
          void *             const context ATTR_UNUSED) {
    
    rpc * const rpcP = headerP->itemP;

    if (rpcP->threadExists) {
        void *status;
        int result;

        result = pthread_join(rpcP->thread, &status);
        
        rpcP->threadExists = FALSE;
    }

    XMLRPC_MEMBLOCK_FREE(char, rpcP->responseXmlP);

    curl_easy_cleanup(rpcP->curlSessionP);

    destroyRpc(rpcP);

    return NULL;
}



static void 
finishAsynch(
    struct xmlrpc_client_transport * const clientTransportP ATTR_UNUSED,
    xmlrpc_timeoutType               const timeoutType ATTR_UNUSED,
    xmlrpc_timeout                   const timeout ATTR_UNUSED) {
/*----------------------------------------------------------------------------
   Wait for the threads of all outstanding RPCs to exit and destroy those
   RPCs.

   This does the 'finish_asynch' operation for a Curl client transport.
-----------------------------------------------------------------------------*/
    /* We ignore any timeout request.  Some day, we should figure out how
       to set an alarm and interrupt running threads.
    */

    pthread_mutex_lock(&clientTransportP->listLock);

    list_foreach(&clientTransportP->rpcList, finishRpc, NULL);

    pthread_mutex_unlock(&clientTransportP->listLock);
}



static void
call(xmlrpc_env *                     const envP,
     struct xmlrpc_client_transport * const clientTransportP,
     const xmlrpc_server_info *       const serverP,
     xmlrpc_mem_block *               const callXmlP,
     xmlrpc_mem_block **              const responsePP) {

    xmlrpc_mem_block * responseXmlP;
    rpc * rpcP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(serverP);
    XMLRPC_ASSERT_PTR_OK(callXmlP);
    XMLRPC_ASSERT_PTR_OK(responsePP);

    responseXmlP = XMLRPC_MEMBLOCK_NEW(char, envP, 0);
    if (!envP->fault_occurred) {
        pthread_mutex_lock(&clientTransportP->syncCurlSessionLock);
        createRpc(envP, clientTransportP, clientTransportP->syncCurlSessionP,
                  serverP,
                  callXmlP, responseXmlP,
                  NULL, NULL,
                  &rpcP);

        if (!envP->fault_occurred) {
            performRpc(envP, rpcP);

            *responsePP = responseXmlP;
            
            destroyRpc(rpcP);
        }
        pthread_mutex_unlock(&clientTransportP->syncCurlSessionLock);
        if (envP->fault_occurred)
            XMLRPC_MEMBLOCK_FREE(char, responseXmlP);
    }
}



struct xmlrpc_client_transport_ops xmlrpc_curl_transport_ops = {
    &create,
    &destroy,
    &sendRequest,
    &call,
    &finishAsynch,
};
