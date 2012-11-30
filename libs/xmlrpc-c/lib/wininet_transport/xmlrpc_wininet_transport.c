/*=============================================================================
                           xmlrpc_wininet_transport
===============================================================================
   WinInet-based client transport for Xmlrpc-c.  Copyright information at
   the bottom of this file.
   
=============================================================================*/

#include "xmlrpc_config.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stddef.h>
#include <windows.h>
#include <wininet.h>

#include "bool.h"
#include "mallocvar.h"
#include "linklist.h"
#include "casprintf.h"
#include "pthreadx.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/client.h"
#include "xmlrpc-c/client_int.h"
#include "xmlrpc-c/transport.h"

#if defined(_DEBUG)
#   include <crtdbg.h>
#   define new DEBUG_NEW
#   define malloc(size) _malloc_dbg( size, _NORMAL_BLOCK, __FILE__, __LINE__)
#   undef THIS_FILE
    static char THIS_FILE[] = __FILE__;
#endif


static HINTERNET hSyncInternetSession = NULL;

/* Declare WinInet status callback. */
void CALLBACK
statusCallback(HINTERNET     const hInternet,
               unsigned long const dwContext,
               unsigned long const dwInternetStatus,
               void *        const lpvStatusInformation,
               unsigned long const dwStatusInformationLength);


struct xmlrpc_client_transport {
    pthread_mutex_t listLock;
    struct list_head rpcList;
        /* List of all RPCs that exist for this transport.  An RPC exists
           from the time the user requests it until the time the user 
           acknowledges it is done.
        */
    int allowInvalidSSLCerts;
        /* Flag to specify if we ignore invalid SSL Certificates.  If this
           is set to zero, calling a XMLRPC server with an invalid SSL
           certificate will fail.  This is the default behavior of the other
           transports, but invalid certificates were allowed in pre 1.2 
           wininet xmlrpc-c transports.
        */
};

typedef struct {
    unsigned long http_status;
    HINTERNET hHttpRequest;
    HINTERNET hURL;
    INTERNET_PORT nPort;
    char szHostName[255];   
    char szUrlPath[255];
    BOOL bUseSSL;
    char * headerList;
    BYTE * pSendData;
    xmlrpc_mem_block * pResponseData;
} winInetTransaction;

typedef struct {
    struct list_head link;  /* link in transport's list of RPCs */
    winInetTransaction * winInetTransactionP;
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
    struct xmlrpc_client_transport * clientTransportP;
} rpc;



static void
createWinInetHeaderList(xmlrpc_env *               const envP,
                        const xmlrpc_server_info * const serverP,
                        char **                    const headerListP) {

    const char * const szContentType = "Content-Type: text/xml\r\n";

    char * szHeaderList;

    /* Send an authorization header if we need one. */
    if (serverP->allowedAuth.basic) {
        /* Make the header with content type and authorization   */
        /* NOTE: A newline is required between each added header */
        szHeaderList = malloc(strlen(szContentType) + 17 +
                              strlen(serverP->basicAuthHdrValue) + 1);
        
        if (szHeaderList == NULL)
            xmlrpc_faultf(envP,
                          "Couldn't allocate memory for authorization header");
        else {
            memcpy(szHeaderList, szContentType, strlen(szContentType));
            memcpy(szHeaderList + strlen(szContentType),"\r\nAuthorization: ",
                   17);
            memcpy(szHeaderList + strlen(szContentType) + 17,
                   serverP->basicAuthHdrValue,
                   strlen(serverP->basicAuthHdrValue) + 1);
        }
    } else {
        /* Just the content type header is needed */
        szHeaderList = malloc(strlen(szContentType) + 1);
        
        if (szHeaderList == NULL)
            xmlrpc_faultf(envP,
                          "Couldn't allocate memory for standard header");
        else 
            memcpy(szHeaderList, szContentType, strlen(szContentType) + 1);
    }
    *headerListP = szHeaderList;
}



static void
createWinInetTransaction(xmlrpc_env *               const envP,
                         const xmlrpc_server_info * const serverP,
                         xmlrpc_mem_block *         const callXmlP,
                         xmlrpc_mem_block *         const responseXmlP,
                         winInetTransaction **      const winInetTranPP) {

    winInetTransaction * winInetTransactionP;

    MALLOCVAR(winInetTransactionP);
    if (winInetTransactionP == NULL)
        xmlrpc_faultf(envP, "No memory to create WinInet transaction.");
    else {
        char szExtraInfo[255];
        char szScheme[100];
        URL_COMPONENTS uc;
        BOOL succeeded;

        /* Init to defaults */
        winInetTransactionP->http_status   = 0;
        winInetTransactionP->hHttpRequest  = NULL;
        winInetTransactionP->hURL          = NULL;
        winInetTransactionP->headerList    = NULL;
        winInetTransactionP->pSendData     = NULL;
        winInetTransactionP->pResponseData = responseXmlP;

        /* Parse the URL and store results into the winInetTransaction */

        memset(&uc, 0, sizeof(uc));
        uc.dwStructSize      = sizeof (uc);
        uc.lpszScheme        = szScheme;
        uc.dwSchemeLength    = 100;
        uc.lpszHostName      = winInetTransactionP->szHostName;
        uc.dwHostNameLength  = 255;
        uc.lpszUrlPath       = winInetTransactionP->szUrlPath;
        uc.dwUrlPathLength   = 255;
        uc.lpszExtraInfo     = szExtraInfo;
        uc.dwExtraInfoLength = 255;
        succeeded = InternetCrackUrl(serverP->serverUrl,
                                     strlen(serverP->serverUrl),
                                     ICU_ESCAPE, &uc);
        if (!succeeded)
            xmlrpc_faultf(envP, "Unable to parse the server URL.");
        else {
            winInetTransactionP->nPort =
                uc.nPort ? uc.nPort : INTERNET_DEFAULT_HTTP_PORT;
            if (_strnicmp(uc.lpszScheme, "https", 5) == 0)
                winInetTransactionP->bUseSSL=TRUE;
            else
                winInetTransactionP->bUseSSL=FALSE;
            createWinInetHeaderList(envP, serverP,
                                    &winInetTransactionP->headerList);

            XMLRPC_MEMBLOCK_APPEND(char, envP, callXmlP, "\0", 1);
            if (!envP->fault_occurred) {
                winInetTransactionP->pSendData =
                    XMLRPC_MEMBLOCK_CONTENTS(char, callXmlP);
            }
        }
            
        if (envP->fault_occurred)
            free(winInetTransactionP);
    }
    *winInetTranPP = winInetTransactionP;
}



static void
destroyWinInetTransaction(winInetTransaction * const winInetTransactionP) {

    XMLRPC_ASSERT_PTR_OK(winInetTransactionP);

    if (winInetTransactionP->hHttpRequest)
        InternetCloseHandle(winInetTransactionP->hHttpRequest);

    if (winInetTransactionP->hURL)
        InternetCloseHandle(winInetTransactionP->hURL);

    if (winInetTransactionP->headerList)
        free(winInetTransactionP->headerList);

    free(winInetTransactionP);
}



static void
get_wininet_response(xmlrpc_env *         const envP,
                     winInetTransaction * const winInetTransactionP) {

    unsigned long dwLen;
    INTERNET_BUFFERS inetBuffer;
    unsigned long dwFlags;
    unsigned long dwErr; 
    unsigned long nExpected;
    void * body;
    BOOL bOK;
    PVOID pMsgMem;

    pMsgMem = NULL; /* initial value */
    dwErr = 0; /* initial value */
    body = NULL;  /* initial value */
    dwLen = sizeof(unsigned long);  /* initial value */

    inetBuffer.dwStructSize    = sizeof (INTERNET_BUFFERS);
    inetBuffer.Next            = NULL;
    inetBuffer.lpcszHeader     = NULL;
    inetBuffer.dwHeadersTotal  = 0;
    inetBuffer.dwHeadersLength = 0;
    inetBuffer.dwOffsetHigh    = 0;
    inetBuffer.dwOffsetLow     = 0;
    inetBuffer.dwBufferLength  = 0;

    /* Note that while Content-Length is optional in HTTP 1.1, it is
       required by XML-RPC.  Following fails if server didn't send it.
    */

    bOK = HttpQueryInfo(winInetTransactionP->hHttpRequest, 
                        HTTP_QUERY_CONTENT_LENGTH|HTTP_QUERY_FLAG_NUMBER, 
                        &inetBuffer.dwBufferTotal, &dwLen, NULL);
    if (!bOK) {
        LPTSTR pMsg;
        dwErr = GetLastError ();
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                      FORMAT_MESSAGE_FROM_SYSTEM, 
                      NULL,
                      dwErr,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPTSTR) &pMsgMem,
                      1024,NULL);

        pMsg = pMsgMem ? (LPTSTR)pMsgMem : "Sync HttpQueryInfo failed.";
        XMLRPC_FAIL(envP, XMLRPC_NETWORK_ERROR, pMsg);
    }

    if (inetBuffer.dwBufferTotal == 0)
        XMLRPC_FAIL(envP, XMLRPC_NETWORK_ERROR, "WinInet returned no data");

    inetBuffer.lpvBuffer = calloc(inetBuffer.dwBufferTotal, sizeof(TCHAR));
    body = inetBuffer.lpvBuffer;
    dwFlags = IRF_SYNC;
    nExpected = inetBuffer.dwBufferTotal;
    inetBuffer.dwBufferLength = nExpected;
    InternetQueryDataAvailable(winInetTransactionP->hHttpRequest,
                               &inetBuffer.dwBufferLength, 0, 0);
    
    /* Read Response from InternetFile */
    do {
        if (inetBuffer.dwBufferLength != 0)
            bOK = InternetReadFileEx(winInetTransactionP->hHttpRequest,
                                     &inetBuffer, dwFlags, 1);

        if (!bOK)
            dwErr = GetLastError();

        if (dwErr) {
            if (dwErr == WSAEWOULDBLOCK || dwErr == ERROR_IO_PENDING) {
                /* Non-block socket operation wait 10 msecs */
                SleepEx(10, TRUE);
                /* Reset dwErr to zero for next pass */
                dwErr = 0;
            } else {
                LPTSTR pMsg;
                FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                               FORMAT_MESSAGE_FROM_SYSTEM, 
                               NULL,
                               dwErr,
                               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                               (LPTSTR) &pMsgMem,
                               1024,NULL);
                pMsg = pMsgMem ?
                    (LPTSTR)pMsgMem : "ASync InternetReadFileEx failed.";
                XMLRPC_FAIL(envP, XMLRPC_NETWORK_ERROR, pMsg);
            }
        }
        
        if (inetBuffer.dwBufferLength) {
            TCHAR * const oldBufptr = inetBuffer.lpvBuffer;

            inetBuffer.lpvBuffer = oldBufptr + inetBuffer.dwBufferLength;
            nExpected -= inetBuffer.dwBufferLength;
            /* Adjust inetBuffer.dwBufferLength when it is greater than the */
            /* expected end of file */
            if (inetBuffer.dwBufferLength > nExpected)
                inetBuffer.dwBufferLength = nExpected; 

        } else
            inetBuffer.dwBufferLength = nExpected;
        dwErr = 0;
    } while (nExpected != 0);

    /* Add to the response buffer. */ 
    xmlrpc_mem_block_append(envP, winInetTransactionP->pResponseData, body,
                            inetBuffer.dwBufferTotal);
    XMLRPC_FAIL_IF_FAULT(envP);

 cleanup:
    /* Since the XMLRPC_FAIL calls goto cleanup, we must handle */
    /* the free'ing of the memory here. */
    if (pMsgMem != NULL)
        LocalFree(pMsgMem);

    if (body)
        free(body);
}



static void
performWinInetTransaction(
    xmlrpc_env *                     const envP,
    winInetTransaction *             const winInetTransactionP,
    struct xmlrpc_client_transport * const clientTransportP) {

    const char * const acceptTypes[] = {"text/xml", NULL};

    unsigned long queryLen;
    LPVOID pMsgMem;
    BOOL succeeded;

    unsigned long lastErr;
    unsigned long reqFlags;

    pMsgMem = NULL;  /* initial value */

    reqFlags = INTERNET_FLAG_NO_UI;  /* initial value */
    
    winInetTransactionP->hURL =
        InternetConnect(hSyncInternetSession, 
                        winInetTransactionP->szHostName,
                        winInetTransactionP->nPort, 
                        NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);

    /* Start our request running. */
    if (winInetTransactionP->bUseSSL == TRUE)
        reqFlags |=
            INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID;

    winInetTransactionP->hHttpRequest =
        HttpOpenRequest(winInetTransactionP->hURL, "POST",
                        winInetTransactionP->szUrlPath, "HTTP/1.1", NULL,
                        (const char **)&acceptTypes,
                        reqFlags, 1);
    
    XMLRPC_FAIL_IF_NULL(winInetTransactionP->hHttpRequest, envP,
                        XMLRPC_INTERNAL_ERROR,
                        "Unable to open the requested URL.");

    succeeded =
        HttpAddRequestHeaders(winInetTransactionP->hHttpRequest,
                              winInetTransactionP->headerList, 
                              strlen (winInetTransactionP->headerList),
                              HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);

    if (!succeeded)
        XMLRPC_FAIL(envP, XMLRPC_INTERNAL_ERROR,
                    "Could not set Content-Type.");

    {
        /* By default, a request times out after 30 seconds.  We don't want
           it to timeout at all, since we don't know what the user is doing.
        */
        DWORD dwTimeOut = 0x7FFFFFFF;  /* Approximation of infinity */
        InternetSetOption(winInetTransactionP->hHttpRequest,
                          INTERNET_OPTION_RECEIVE_TIMEOUT,
                          &dwTimeOut, sizeof(dwTimeOut));
    }
Again:
    /* Send the requested XML remote procedure command */ 
    succeeded = HttpSendRequest(winInetTransactionP->hHttpRequest, NULL, 0, 
                                winInetTransactionP->pSendData, 
                                strlen(winInetTransactionP->pSendData));
    if (!succeeded) {
        LPTSTR pMsg;

        lastErr = GetLastError();

        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_ALLOCATE_BUFFER |
                      FORMAT_MESSAGE_IGNORE_INSERTS, 
                      NULL,
                      lastErr,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPTSTR) &pMsgMem,
                      0, NULL);

        if (pMsgMem == NULL) {
            switch (lastErr) {
            case ERROR_INTERNET_CANNOT_CONNECT:
                pMsg = "Sync HttpSendRequest failed: Connection refused.";
                break;
            case ERROR_INTERNET_CLIENT_AUTH_CERT_NEEDED:
                pMsg = "Sync HttpSendRequest failed: "
                    "Client authorization certificate needed.";
                break;

            /* The following conditions are recommendations that microsoft */
            /* provides in their knowledge base. */

            /* HOWTO: Handle Invalid Certificate Authority Error with
               WinInet (Q182888)
            */
            case ERROR_INTERNET_INVALID_CA:
                if (clientTransportP->allowInvalidSSLCerts){
                    OutputDebugString(
                        "Sync HttpSendRequest failed: "
                        "The function is unfamiliar with the certificate "
                        "authority that generated the server's certificate. ");
                    reqFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA;

                    InternetSetOption(winInetTransactionP->hHttpRequest,
                                      INTERNET_OPTION_SECURITY_FLAGS,
                                      &reqFlags, sizeof(reqFlags));

                    goto Again;
                } else
                    pMsg = "Invalid or unknown/untrusted "
                        "SSL Certificate Authority.";
                break;

            /* HOWTO: Make SSL Requests Using WinInet (Q168151) */
            case ERROR_INTERNET_SEC_CERT_CN_INVALID:
                if (clientTransportP->allowInvalidSSLCerts) {
                    OutputDebugString(
                        "Sync HttpSendRequest failed: "
                        "The SSL certificate common name (host name field) "
                        "is incorrect\r\n "
                        "for example, if you entered www.server.com "
                        "and the common name "
                        "on the certificate says www.different.com. ");
                    
                    reqFlags = INTERNET_FLAG_IGNORE_CERT_CN_INVALID;

                    InternetSetOption(winInetTransactionP->hHttpRequest,
                                      INTERNET_OPTION_SECURITY_FLAGS,
                                      &reqFlags, sizeof(reqFlags));
                    
                    goto Again;
                } else
                    pMsg = "The SSL certificate common name "
                        "(host name field) is incorrect.";
                break;

            case ERROR_INTERNET_SEC_CERT_DATE_INVALID:
                if (clientTransportP->allowInvalidSSLCerts) {
                    OutputDebugString(
                        "Sync HttpSendRequest failed: "
                        "The SSL certificate date that was received "
                        "from the server is "
                        "bad. The certificate is expired. ");

                    reqFlags = INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;

                    InternetSetOption(winInetTransactionP->hHttpRequest,
                                      INTERNET_OPTION_SECURITY_FLAGS,
                                      &reqFlags, sizeof(reqFlags));

                    goto Again;
                } else
                    pMsg = "The SSL certificate date that was received "
                        "from the server is invalid.";
                break;

            default:
                pMsg = (LPTSTR)pMsgMem = LocalAlloc(LPTR, MAX_PATH);
                sprintf(pMsg, "Sync HttpSendRequest failed: "
                        "GetLastError (%d)", lastErr);
                break;

            }
        } else
            pMsg = (LPTSTR)pMsgMem; 

        XMLRPC_FAIL(envP, XMLRPC_NETWORK_ERROR, pMsg);
    }

    queryLen = sizeof(unsigned long);  /* initial value */

    succeeded = HttpQueryInfo(winInetTransactionP->hHttpRequest, 
                              HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_STATUS_CODE,
                              &winInetTransactionP->http_status,
                              &queryLen, NULL);
    if (!succeeded) {
        LPTSTR pMsg;

        lastErr = GetLastError();
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                      FORMAT_MESSAGE_FROM_SYSTEM, 
                      NULL,
                      lastErr,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPTSTR) &pMsgMem,
                      1024, NULL);

        pMsg = pMsgMem ? (LPTSTR)pMsgMem : "Sync HttpQueryInfo failed.";
        XMLRPC_FAIL(envP, XMLRPC_NETWORK_ERROR, pMsg);
    }

    /* Make sure we got a "200 OK" message from the remote server. */
    if (winInetTransactionP->http_status != 200) {
        unsigned long msgLen;
        char errMsg[1024];
        errMsg[0] = '\0';
        msgLen = 1024;  /* initial value */

        HttpQueryInfo(winInetTransactionP->hHttpRequest,
                      HTTP_QUERY_STATUS_TEXT, errMsg, &msgLen, NULL);

        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_NETWORK_ERROR,
            "HTTP error #%d occurred\n %s",
            winInetTransactionP->http_status, errMsg);
        goto cleanup;
    }
    /* Read the response. */    
    get_wininet_response(envP, winInetTransactionP);
    XMLRPC_FAIL_IF_FAULT(envP);

 cleanup:
    /* Since the XMLRPC_FAIL calls goto cleanup, we must handle */
    /* the free'ing of the memory here. */
    if (pMsgMem)
        LocalFree(pMsgMem);
}



static void *
doAsyncRpc(void * const arg) {

    rpc * const rpcP = arg;

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    performWinInetTransaction(&env, rpcP->winInetTransactionP,
                              rpcP->clientTransportP );

    rpcP->complete(rpcP->callInfoP, rpcP->responseXmlP, env);

    xmlrpc_env_clean(&env);

    return NULL;
}



static void
createRpcThread(xmlrpc_env * const envP,
                rpc *        const rpcP,
                pthread_t *  const threadP) {

    int rc;

    rc = pthread_create(threadP, NULL, doAsyncRpc, rpcP);
    switch (rc) {
    case 0: 
        break;
    case EAGAIN:
        xmlrpc_faultf(envP, "pthread_create() failed:  "
                      "System Resources exceeded.");
        break;
    case EINVAL:
        xmlrpc_faultf(envP, "pthread_create() failed:  "
                      "Param Error for attr.");
        break;
    case ENOMEM:
        xmlrpc_faultf(envP, "pthread_create() failed:  "
                      "No memory for new thread.");
        break;
    default:
        xmlrpc_faultf(envP, "pthread_create() failed: "
                      "Unrecognized error code %d.", rc);
        break;
    }
}



static void
rpcCreate(xmlrpc_env *                     const envP,
          struct xmlrpc_client_transport * const clientTransportP,
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
        rpcP->callInfoP    = callInfoP;
        rpcP->complete     = complete;
        rpcP->responseXmlP = responseXmlP;
        rpcP->threadExists = FALSE;

        createWinInetTransaction(envP, serverP, callXmlP, responseXmlP, 
                                 &rpcP->winInetTransactionP);
        if (!envP->fault_occurred) {
            if (complete) {
                createRpcThread(envP, rpcP, &rpcP->thread);
                if (!envP->fault_occurred)
                    rpcP->threadExists = TRUE;
            }
            if (!envP->fault_occurred) {
                list_init_header(&rpcP->link, rpcP);
                pthread_mutex_lock(&clientTransportP->listLock);
                list_add_head(&clientTransportP->rpcList, &rpcP->link);
                pthread_mutex_unlock(&clientTransportP->listLock);
            }
            if (envP->fault_occurred)
                    destroyWinInetTransaction(rpcP->winInetTransactionP);
        }
        if (envP->fault_occurred)
            free(rpcP);
    }
    *rpcPP = rpcP;
}



static void 
rpcDestroy(rpc * const rpcP) {

    XMLRPC_ASSERT_PTR_OK(rpcP);
    XMLRPC_ASSERT(!rpcP->threadExists);

    destroyWinInetTransaction(rpcP->winInetTransactionP);

    list_remove(&rpcP->link);

    free(rpcP);
}



static void * 
finishRpc(struct list_head * const headerP, 
          void *             const context ATTR_UNUSED) {
    
    rpc * const rpcP = headerP->itemP;

    if (rpcP->threadExists) {
        void * status;
        int result;

        result = pthread_join(rpcP->thread, &status);
        
        rpcP->threadExists = FALSE;
    }

    XMLRPC_MEMBLOCK_FREE(char, rpcP->responseXmlP);

    rpcDestroy(rpcP);

    return NULL;
}


/* Used for debugging purposes to track the status of
   your request.
*/
void CALLBACK
statusCallback (HINTERNET     const hInternet,
                unsigned long const dwContext,
                unsigned long const dwInternetStatus,
                void *        const lpvStatusInformation,
                unsigned long const dwStatusInformationLength) {

    switch (dwInternetStatus) {
    case INTERNET_STATUS_RESOLVING_NAME:
        OutputDebugString("INTERNET_STATUS_RESOLVING_NAME\r\n");
        break;

    case INTERNET_STATUS_NAME_RESOLVED:
        OutputDebugString("INTERNET_STATUS_NAME_RESOLVED\r\n");
        break;

    case INTERNET_STATUS_HANDLE_CREATED:
        OutputDebugString("INTERNET_STATUS_HANDLE_CREATED\r\n");
        break;

    case INTERNET_STATUS_CONNECTING_TO_SERVER:
        OutputDebugString("INTERNET_STATUS_CONNECTING_TO_SERVER\r\n");
        break;

    case INTERNET_STATUS_REQUEST_SENT:
        OutputDebugString("INTERNET_STATUS_REQUEST_SENT\r\n");
        break;

    case INTERNET_STATUS_SENDING_REQUEST:
        OutputDebugString("INTERNET_STATUS_SENDING_REQUEST\r\n");
        break;

    case INTERNET_STATUS_CONNECTED_TO_SERVER:
        OutputDebugString("INTERNET_STATUS_CONNECTED_TO_SERVER\r\n");
        break;

    case INTERNET_STATUS_RECEIVING_RESPONSE:
        OutputDebugString("INTERNET_STATUS_RECEIVING_RESPONSE\r\n");
        break;

    case INTERNET_STATUS_RESPONSE_RECEIVED:
        OutputDebugString("INTERNET_STATUS_RESPONSE_RECEIVED\r\n");
        break;

    case INTERNET_STATUS_CLOSING_CONNECTION:
        OutputDebugString("INTERNET_STATUS_CLOSING_CONNECTION\r\n");
        break;

    case INTERNET_STATUS_CONNECTION_CLOSED:
        OutputDebugString("INTERNET_STATUS_CONNECTION_CLOSED\r\n");
        break;

    case INTERNET_STATUS_HANDLE_CLOSING:
        OutputDebugString("INTERNET_STATUS_HANDLE_CLOSING\r\n");
        break;

    case INTERNET_STATUS_CTL_RESPONSE_RECEIVED:
        OutputDebugString("INTERNET_STATUS_CTL_RESPONSE_RECEIVED\r\n");
        break;

    case INTERNET_STATUS_REDIRECT:
        OutputDebugString("INTERNET_STATUS_REDIRECT\r\n");
        break;

    case INTERNET_STATUS_REQUEST_COMPLETE:
        /* This indicates the data is ready. */
        OutputDebugString("INTERNET_STATUS_REQUEST_COMPLETE\r\n");
        break;

    default:
        OutputDebugString("statusCallback, default case!\r\n");
        break;
     }
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
   This does the 'create' operation for a WinInet client transport.
-----------------------------------------------------------------------------*/
    const struct xmlrpc_wininet_xportparms * const wininetXportParmsP = 
        transportparmsP;

    struct xmlrpc_client_transport * transportP;

    MALLOCVAR(transportP);
    if (transportP == NULL)
        xmlrpc_faultf(envP, "Unable to allocate transport descriptor.");
    else {
        pthread_mutex_init(&transportP->listLock, NULL);
        
        list_make_empty(&transportP->rpcList);

        if (hSyncInternetSession == NULL)
            hSyncInternetSession =
                InternetOpen("xmlrpc-c wininet transport",
                             INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);

        if (!wininetXportParmsP ||
            parm_size < XMLRPC_WXPSIZE(allowInvalidSSLCerts))
            transportP->allowInvalidSSLCerts = 0;
        else
            transportP->allowInvalidSSLCerts =
                wininetXportParmsP->allowInvalidSSLCerts;

        *handlePP = transportP;
    }
}



static void 
destroy(struct xmlrpc_client_transport * const clientTransportP) {
/*----------------------------------------------------------------------------
   This does the 'destroy' operation for a WinInet client transport.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT(clientTransportP != NULL);

    XMLRPC_ASSERT(list_is_empty(&clientTransportP->rpcList));

    if (hSyncInternetSession)
        InternetCloseHandle(hSyncInternetSession);
    hSyncInternetSession = NULL;

    pthread_mutex_destroy(&clientTransportP->listLock);

    free(clientTransportP);
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

   This does the 'send_request' operation for a WinInet client transport.
-----------------------------------------------------------------------------*/
    rpc * rpcP;
    xmlrpc_mem_block * responseXmlP;

    responseXmlP = XMLRPC_MEMBLOCK_NEW(char, envP, 0);
    if (!envP->fault_occurred) {
        rpcCreate(envP, clientTransportP, serverP, callXmlP, responseXmlP,
                  complete, callInfoP,
                  &rpcP);

        if (envP->fault_occurred)
            XMLRPC_MEMBLOCK_FREE(char, responseXmlP);
    }
    /* The user's eventual finish_asynch call will destroy this RPC
       and response buffer
    */
}



static void 
finishAsynch(struct xmlrpc_client_transport * const clientTransportP,
             xmlrpc_timeoutType               const timeoutType ATTR_UNUSED,
             xmlrpc_timeout                   const timeout ATTR_UNUSED) {
/*----------------------------------------------------------------------------
   Wait for the threads of all outstanding RPCs to exit and destroy those
   RPCs.

   This does the 'finish_asynch' operation for a WinInet client transport.
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
        rpcCreate(envP, clientTransportP, serverP, callXmlP, responseXmlP,
                  NULL, NULL, &rpcP);
        if (!envP->fault_occurred) {
            performWinInetTransaction(envP, rpcP->winInetTransactionP,
                                      clientTransportP);
            
            *responsePP = responseXmlP;
            
            rpcDestroy(rpcP);
        }
        if (envP->fault_occurred)
            XMLRPC_MEMBLOCK_FREE(char, responseXmlP);
    }
}



struct xmlrpc_client_transport_ops xmlrpc_wininet_transport_ops = {
    NULL,
    NULL,
    &create,
    &destroy,
    &sendRequest,
    &call,
    &finishAsynch,
    NULL,
};



/* Copyright (C) 2001 by First Peer, Inc. All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission. 
**  
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE. */
