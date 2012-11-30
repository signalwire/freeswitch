/*=============================================================================
    curlTransaction
=============================================================================*/

#define _XOPEN_SOURCE 600  /* Make sure strdup() is in <string.h> */

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "mallocvar.h"

#include "xmlrpc-c/util.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/client.h"
#include "xmlrpc-c/client_int.h"
#include "version.h"

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#include "curlversion.h"

#include "curltransaction.h"


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
    curlt_finishFn * finish;
    curlt_progressFn * progress;
    void * userContextP;
        /* Meaningful to our client; opaque to us */
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
addHeader(xmlrpc_env *         const envP,
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



static const char *
xmlrpcUserAgentPart(bool const reportIt) {

    const char * retval;
    
    if (reportIt) {
        curl_version_info_data * const curlInfoP =
            curl_version_info(CURLVERSION_NOW);
        char curlVersion[32];
        
        snprintf(curlVersion, sizeof(curlVersion), "%u.%u.%u",
                 (curlInfoP->version_num >> 16) && 0xff,
                 (curlInfoP->version_num >>  8) && 0xff,
                 (curlInfoP->version_num >>  0) && 0xff
            );

        xmlrpc_asprintf(&retval,
                        "Xmlrpc-c/%s Curl/%s",
                        XMLRPC_C_VERSION, curlVersion);
    } else
        xmlrpc_asprintf(&retval, "%s", "");

    return retval;
}



static void
addUserAgentHeader(xmlrpc_env *         const envP,
                   struct curl_slist ** const headerListP,
                   bool                 const reportXmlrpc,
                   const char *         const userAgent) {
/*----------------------------------------------------------------------------
   Add a User-Agent HTTP header to the Curl header list *headerListP,
   if appropriate.
   
   'reportXmlrpc' means we want to tell the client what XML-RPC agent
   is being used -- Xmlrpc-c and layers below.

   'userAgent' is a string describing the layers above Xmlrpc-c.  We
   assume it is in the proper format to be included in a User-Agent
   header.  (We should probably fix that some day -- take ownership
   of that format).
-----------------------------------------------------------------------------*/
    if (reportXmlrpc || userAgent) {
        /* Add the header */

        /* Note: Curl has a CURLOPT_USERAGENT option that does some of this
           work.  We prefer to be totally in control, though, so we build
           the header explicitly.
        */

        const char * const xmlrpcPart = xmlrpcUserAgentPart(reportXmlrpc);

        if (xmlrpc_strnomem(xmlrpcPart))
            xmlrpc_faultf(envP, "Couldn't allocate memory for "
                          "User-Agent header");
        else {
            const char * const userPart = userAgent ? userAgent : "";
            const char * const space = userAgent && reportXmlrpc ? " " : "";

            const char * userAgentHeader;

            xmlrpc_asprintf(&userAgentHeader,
                            "User-Agent: %s%s%s",
                            userPart, space, xmlrpcPart);
        
            if (xmlrpc_strnomem(userAgentHeader))
                xmlrpc_faultf(envP, "Couldn't allocate memory for "
                              "User-Agent header");
            else {
                addHeader(envP, headerListP, userAgentHeader);
            
                xmlrpc_strfree(userAgentHeader);
            }
            xmlrpc_strfree(xmlrpcPart);
        }
    }
}



static void
addAuthorizationHeader(xmlrpc_env *         const envP,
                       struct curl_slist ** const headerListP,
                       const char *         const hdrValue) {

    const char * authorizationHeader;
            
    xmlrpc_asprintf(&authorizationHeader, "Authorization: %s", hdrValue);
    
    if (xmlrpc_strnomem(authorizationHeader))
        xmlrpc_faultf(envP, "Couldn't allocate memory for "
                      "Authorization header");
    else {
        addHeader(envP, headerListP, authorizationHeader);
        
        xmlrpc_strfree(authorizationHeader);
    }
}



/*
  In HTTP 1.1, the client can send the header "Expect: 100-continue", which
  tells the server that the client isn't going to send the body until the
  server tells it to by sending a "continue" response (HTTP response code 100).
  The server is obligated to send that response.

  However, many servers are broken and don't send the Continue response.

  Early libcurl did not send the Expect: header, thus worked fine with such
  broken servers.  But as of ca. 2007, libcurl sends the Expect:, and waits
  for the response, when the body is large.  It gives up after 3 seconds and
  sends the body anyway.

  To accomodate the broken servers and for backward compatibility, we always
  force libcurl not to send the Expect and consequently not to wait for the
  response, using the hackish (but according to libcurl design) method of
  including an entry in our explicit header list that is an Expect: header
  with an empty argument.  This causes libcurl not to send any Expect: header.
  This is since 1.19; we may find there are also servers and/or libcurl levels
  that can't work with that.

  We may find a case where the Expect/Continue protocol is desirable.  If we
  do, we should add a transport option to request the function and let libcurl
  do its thing when the user requests it.

  The purpose of Expect/Continue is to save the client the trouble of
  generating and/or sending the body when the server is just going to reject
  the transaction based on the headers -- like maybe because the body is
  too big.
*/


static void
addExpectHeader(xmlrpc_env *         const envP,
                struct curl_slist ** const headerListP) {

    addHeader(envP, headerListP, "Expect:");
        /* Don't send Expect header.  See explanation above. */
}



static void
createCurlHeaderList(xmlrpc_env *               const envP,
                     const char *               const authHdrValue,
                     bool                       const dontAdvertise,
                     const char *               const userAgent,
                     struct curl_slist **       const headerListP) {

    struct curl_slist * headerList;

    headerList = NULL;  /* initial value - empty list */

    addContentTypeHeader(envP, &headerList);
    if (!envP->fault_occurred) {
        addUserAgentHeader(envP, &headerList, !dontAdvertise, userAgent);
        if (!envP->fault_occurred) {
            if (authHdrValue)
                addAuthorizationHeader(envP, &headerList, authHdrValue);
        }
        if (!envP->fault_occurred)
            addExpectHeader(envP, &headerList);
    }
    if (envP->fault_occurred)
        curl_slist_free_all(headerList);

    *headerListP = headerList;
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
             double const dltotal,
             double const dlnow,
             double const ultotal,
             double const ulnow) {
/*----------------------------------------------------------------------------
   This is a Curl "progress function."  It's something various Curl functions
   call every so often, including whenever something gets interrupted by the
   process receiving, and catching, a signal.  There are two purposes of a
   Curl progress function: 1) lets us log the progress of a long-running
   transaction such as a big download, e.g. by displaying a progress bar
   somewhere.  2) allows us to tell the Curl function, via our return code,
   that calls it that we don't want to wait anymore for the operation to
   complete.

   In Curl versions before March 2007, we get called once per second and
   signals have no effect.  In current Curl, we usually get called immediately
   after a signal gets caught while Curl is waiting to receive a response from
   the server.  But Curl doesn't properly synchronize with signals, so it may
   miss one and then we don't get called until the next scheduled
   one-per-second call.

   All we do is pass the call through to the curlTransaction's progress
   function (the one that the creator of the curlTransaction registered).

   This function is not as important as it once was for interrupting purposes.
   This module used to use curl_easy_perform(), which can be interrupted only
   via this progress function.  But because of the above-mentioned failure of
   Curl to properly synchronize signals (and Bryan's failure to get Curl
   developers to accept code to fix it), we now use the Curl "multi" facility
   instead and do our own pselect().  But This function still normally gets
   called by curl_multi_perform(), which the transport tries to call even when
   the user has requested interruption, because we don't trust our ability to
   abort a running Curl transaction.  curl_multi_perform() reliably winds up a
   Curl transaction when this function tells it to.
-----------------------------------------------------------------------------*/
    curlTransaction * const curlTransactionP = contextP;

    bool abort;

    /* We require anyone setting us up as the Curl progress function to
       supply a progress function:
    */
    assert(curlTransactionP);
    assert(curlTransactionP->progress);

    curlTransactionP->progress(curlTransactionP->userContextP,
                               dltotal, dlnow, ultotal, ulnow,
                               &abort);

    return abort;
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
assertConstantsMatch(void) {
/*----------------------------------------------------------------------------
   There are some constants that we define as part of the Xmlrpc-c
   interface that are identical to constants in the Curl interface to
   make curl option setting work.  This function asserts such
   formally.
-----------------------------------------------------------------------------*/
    assert(XMLRPC_SSLVERSION_DEFAULT == CURL_SSLVERSION_DEFAULT);
    assert(XMLRPC_SSLVERSION_TLSv1   == CURL_SSLVERSION_TLSv1  );
    assert(XMLRPC_SSLVERSION_SSLv2   == CURL_SSLVERSION_SSLv2  );
    assert(XMLRPC_SSLVERSION_SSLv3   == CURL_SSLVERSION_SSLv3  );

    assert(XMLRPC_HTTPAUTH_BASIC        == CURLAUTH_BASIC       );
    assert(XMLRPC_HTTPAUTH_DIGEST       == CURLAUTH_DIGEST      );
    assert(XMLRPC_HTTPAUTH_GSSNEGOTIATE == CURLAUTH_GSSNEGOTIATE);
    assert(XMLRPC_HTTPAUTH_NTLM         == CURLAUTH_NTLM        );

    assert(XMLRPC_HTTPPROXY_HTTP   == CURLPROXY_HTTP   );
    assert(XMLRPC_HTTPPROXY_SOCKS5 == CURLPROXY_SOCKS5 );
}



static void
setupCurlSession(xmlrpc_env *               const envP,
                 curlTransaction *          const curlTransactionP,
                 xmlrpc_mem_block *         const callXmlP,
                 xmlrpc_mem_block *         const responseXmlP,
                 const xmlrpc_server_info * const serverInfoP,
                 bool                       const dontAdvertise,
                 const char *               const userAgent,
                 const struct curlSetup *   const curlSetupP) {
/*----------------------------------------------------------------------------
   Set up the Curl session for the transaction *curlTransactionP so that
   a subsequent curl_easy_perform() would perform said transaction.

   The data curl_easy_perform() would send for that transaction would 
   be the contents of *callXmlP; the data curl_easy_perform() gets back
   would go into *responseXmlP.

   *serverInfoP tells what sort of authentication to set up.  This is
   an embarassment, as the xmlrpc_server_info type is part of the
   Xmlrpc-c interface.  Some day, we need to replace this with a type
   (probably identical) not tied to Xmlrpc-c.
-----------------------------------------------------------------------------*/
    CURL * const curlSessionP = curlTransactionP->curlSessionP;

    assertConstantsMatch();

    /* A Curl session is serial -- it processes zero or one transaction
       at a time.  We use the "private" attribute of the Curl session to
       indicate which transaction it is presently processing.  This is
       important when the transaction finishes, because libcurl will just
       tell us that something finished on a particular session, not that
       a particular transaction finished.
    */

    /* It is out policy to do a libcurl call only where necessary, I.e.  not
       to set what is the default anyhow.  The reduction in calls may save
       some time, but mostly, it will save us encountering rare bugs or
       suffering from backward incompatibilities in future libcurl.  I.e. we
       don't exercise any more of libcurl than we have to.
    */

    curl_easy_setopt(curlSessionP, CURLOPT_PRIVATE, curlTransactionP);

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
        if (curlTransactionP->progress) {
            curl_easy_setopt(curlSessionP, CURLOPT_NOPROGRESS, 0);
            curl_easy_setopt(curlSessionP, CURLOPT_PROGRESSFUNCTION,
                             curlProgress);
            curl_easy_setopt(curlSessionP, CURLOPT_PROGRESSDATA,
                             curlTransactionP);
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

        if (curlSetupP->proxy)
            curl_easy_setopt(curlSessionP, CURLOPT_PROXY, curlSetupP->proxy);
        if (curlSetupP->proxyAuth != CURLAUTH_BASIC)
            /* Note that the Xmlrpc-c default and the Curl default are
               different.  Xmlrpc-c is none, while Curl is basic.  One reason
               for this is that it makes our extensible parameter list scheme,
               wherein zero always means default, easier.
            */
            curl_easy_setopt(curlSessionP, CURLOPT_PROXYAUTH,
                             curlSetupP->proxyAuth);
        if (curlSetupP->proxyPort)
            curl_easy_setopt(curlSessionP, CURLOPT_PROXYPORT,
                             curlSetupP->proxyPort);
        if (curlSetupP->proxyUserPwd)
            curl_easy_setopt(curlSessionP, CURLOPT_PROXYUSERPWD,
                             curlSetupP->proxyUserPwd);
        if (curlSetupP->proxyType)
            curl_easy_setopt(curlSessionP, CURLOPT_PROXYTYPE,
                             curlSetupP->proxyType);

        if (curlSetupP->verbose)
            curl_easy_setopt(curlSessionP, CURLOPT_VERBOSE, 1l);

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
                createCurlHeaderList(envP, authHdrValue,
                                     dontAdvertise, userAgent,
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



void
curlTransaction_create(xmlrpc_env *               const envP,
                       CURL *                     const curlSessionP,
                       const xmlrpc_server_info * const serverP,
                       xmlrpc_mem_block *         const callXmlP,
                       xmlrpc_mem_block *         const responseXmlP,
                       bool                       const dontAdvertise,
                       const char *               const userAgent,
                       const struct curlSetup *   const curlSetupStuffP,
                       void *                     const userContextP,
                       curlt_finishFn *           const finish,
                       curlt_progressFn *         const progress,
                       curlTransaction **         const curlTransactionPP) {

    curlTransaction * curlTransactionP;

    MALLOCVAR(curlTransactionP);
    if (curlTransactionP == NULL)
        xmlrpc_faultf(envP, "No memory to create Curl transaction.");
    else {
        curlTransactionP->finish       = finish;
        curlTransactionP->curlSessionP = curlSessionP;
        curlTransactionP->userContextP = userContextP;
        curlTransactionP->progress     = progress;

        curlTransactionP->serverUrl = strdup(serverP->serverUrl);
        if (curlTransactionP->serverUrl == NULL)
            xmlrpc_faultf(envP, "Out of memory to store server URL.");
        else {
            setupCurlSession(envP, curlTransactionP,
                             callXmlP, responseXmlP,
                             serverP, dontAdvertise, userAgent,
                             curlSetupStuffP);
            
            if (envP->fault_occurred)
                xmlrpc_strfree(curlTransactionP->serverUrl);
        }
        if (envP->fault_occurred)
            free(curlTransactionP);
    }
    *curlTransactionPP = curlTransactionP;
}



void
curlTransaction_destroy(curlTransaction * const curlTransactionP) {

    curl_slist_free_all(curlTransactionP->headerList);
    xmlrpc_strfree(curlTransactionP->serverUrl);

    free(curlTransactionP);
}



static void
interpretCurlEasyError(const char ** const descriptionP,
                       CURLcode      const code) {

#if HAVE_CURL_STRERROR
    *descriptionP = strdup(curl_easy_strerror(code));
#else
    xmlrpc_asprintf(descriptionP, "Curl error code (CURLcode) %d", code);
#endif
}



void
curlTransaction_getError(curlTransaction * const curlTransactionP,
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
            "HTTP POST transaction, explaining:  %s", explanation);

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



void
curlTransaction_finish(xmlrpc_env *      const envP,
                       curlTransaction * const curlTransactionP,
                       CURLcode          const result) {

    curlTransactionP->result = result;

    if (curlTransactionP->finish)
        curlTransactionP->finish(envP, curlTransactionP->userContextP);
}



CURL *
curlTransaction_curlSession(curlTransaction * const curlTransactionP) {

    return curlTransactionP->curlSessionP;

}
