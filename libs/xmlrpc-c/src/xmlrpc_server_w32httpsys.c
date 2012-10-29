/* Copyright information is at end of file. */

/* COMPILATION NOTE:

   Note that the Platform SDK headers and link libraries for Windows XP SP2 or
   newer are required to compile Xmlrpc-c for this module.  If you are not
   using this XML-RPC server program, it is safe to exclude the
   xmlrpc_server_w32httpsys.c file from the xmlrpc project and you will not
   have this dependency.  You can get the latest platform SDK at
   http://www.microsoft.com/msdownload/platformsdk/sdkupdate/ Be sure after
   installation to choose the program to "register the PSDK directories with
   Visual Studio" so the newer headers are found.
*/

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

/* Declare that we require the Windows XP SP2 or better version of the
   interface to Windows.

   Microsoft recommends
   (http://msdn.microsoft.com/en-us/library/aa383745(VS.85).aspx) defining
   NTDDI_VERSION instead of _WIN32_WINNT for this purpose, but as it was
   invented recently, it's pretty useless.  Windows header files from old
   Windows SDKs won't know what to do with it.
*/
#define _WIN32_WINNT 0x0502

/* See compilation note above if the compiler doesn't find this header file */
#include <http.h>
#include <strsafe.h>

#include "xmlrpc_config.h"
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/server.h"
#include "xmlrpc-c/server_w32httpsys.h"
#include "version.h"

#pragma comment( lib, "httpapi" )
#pragma message( "Compiling HTTPS server ..." )

/* XXX - This variable is *not* currently threadsafe. Once the server has
** been started, it must be treated as read-only. */
static xmlrpc_registry *global_registryP;

//set TRUE if you want a log
static BOOL g_bDebug;
//set log filename
static char g_fLogFile[MAX_PATH];
//do you want OutputDebugString() to be called?
static BOOL g_bDebugString;

//
// Macros.
//
#define INITIALIZE_HTTP_RESPONSE( resp, status, reason )                    \
    do                                                                      \
    {                                                                       \
        RtlZeroMemory( (resp), sizeof(*(resp)) );                           \
        (resp)->StatusCode = (status);                                      \
        (resp)->pReason = (reason);                                         \
        (resp)->ReasonLength = (USHORT) strlen(reason);                     \
    } while (FALSE)


#define ADD_KNOWN_HEADER(Response, HeaderId, RawValue)                      \
    do                                                                      \
    {                                                                       \
        (Response).Headers.KnownHeaders[(HeaderId)].pRawValue = (RawValue); \
        (Response).Headers.KnownHeaders[(HeaderId)].RawValueLength =        \
            (USHORT) strlen(RawValue);                                      \
    } while(FALSE)

#define ALLOC_MEM(cb) HeapAlloc(GetProcessHeap(), 0, (cb))
#define FREE_MEM(ptr) HeapFree(GetProcessHeap(), 0, (ptr))

//
// Prototypes for Internal Functions.
//
DWORD
DoReceiveRequests(
    HANDLE hReqQueue,
    const xmlrpc_server_httpsys_parms * const parmsP
    );

DWORD
SendHttpResponse(
    IN HANDLE        hReqQueue,
    IN PHTTP_REQUEST pRequest,
    IN USHORT        StatusCode,
    IN PSTR          pReason,
    IN PSTR          pEntity
    );

DWORD
SendHttpResponseAuthRequired(
    IN HANDLE        hReqQueue,
    IN PHTTP_REQUEST pRequest
    );

void
processRPCCall(
    xmlrpc_env *     const envP,
    IN HANDLE        hReqQueue,
    IN PHTTP_REQUEST pRequest
    );

__inline void TraceA(const char *format, ...);
__inline void TraceW(const wchar_t *format, ...);


//
// External Function Implementation.
//

void
xmlrpc_server_httpsys(
    xmlrpc_env *                        const envP,
    const xmlrpc_server_httpsys_parms * const parmsP,
    unsigned int                        const parm_size
    )
{
    ULONG           retCode;
    HANDLE          hReqQueue      = NULL;
    HTTPAPI_VERSION HttpApiVersion = HTTPAPI_VERSION_1;
    WCHAR           wszURL[35];

    XMLRPC_ASSERT_ENV_OK(envP);

    if (parm_size < XMLRPC_HSSIZE(authfn))
    {
        xmlrpc_faultf(envP,
                      "You must specify members at least up through "
                      "'authfn' in the server parameters argument.  "
                      "That would mean the parameter size would be >= %u "
                      "but you specified a size of %u",
                      XMLRPC_HSSIZE(authfn), parm_size);
        return;
    }

    //Set logging options
    if (parmsP->logLevel>0)
        g_bDebug=TRUE;
    else
        g_bDebug=FALSE;

    if (parmsP->logLevel>1)
        g_bDebugString=TRUE;
    else
        g_bDebugString=FALSE;

    if (!parmsP->logFile)
        g_bDebug=FALSE;
    else
        StringCchPrintfA(g_fLogFile,MAX_PATH,parmsP->logFile);

    //construct the URL we are listening on
    if (parmsP->useSSL!=0)
        StringCchPrintf(wszURL,35,L"https://+:%u/RPC2",parmsP->portNum);
    else
        StringCchPrintf(wszURL,35,L"http://+:%u/RPC2",parmsP->portNum);

    global_registryP = parmsP->registryP;

    // Initialize HTTP APIs.
    retCode = HttpInitialize(HttpApiVersion,
                             HTTP_INITIALIZE_SERVER,    // Flags
                             NULL                       // Reserved
        );
    if (retCode != NO_ERROR)
    {
        xmlrpc_faultf(envP, "HttpInitialize failed with %lu",
                      retCode);
        return;
    }

    // Create a Request Queue Handle
    retCode = HttpCreateHttpHandle(&hReqQueue,        // Req Queue
                                   0                  // Reserved
        );
    if (retCode != NO_ERROR)
    { 
        xmlrpc_faultf(envP, "HttpCreateHttpHandle failed with %lu", retCode);
        goto CleanUp;
    }

    retCode = HttpAddUrl(hReqQueue,   // Req Queue
                         wszURL,      // Fully qualified URL
                         NULL         // Reserved
        );

    if (retCode != NO_ERROR)
    {
        xmlrpc_faultf(envP, "HttpAddUrl failed with %lu", retCode);
        goto CleanUp;
    }

    TraceW(L"we are listening for requests on the following url: %ws",
           wszURL);

    // Loop while receiving requests
    for(;;)
    {
        TraceW(L"Calling DoReceiveRequests()");
        retCode = DoReceiveRequests(hReqQueue, parmsP);
        if(NO_ERROR == retCode)
        {
            TraceW(L"DoReceiveRequests() returned NO_ERROR, breaking");
            break;
        }
    }

CleanUp:

    TraceW(L"Tearing down the server.", wszURL);

    // Call HttpRemoveUrl for the URL that we added.
    HttpRemoveUrl( hReqQueue, wszURL );

    // Close the Request Queue handle.
    if(hReqQueue)
        CloseHandle(hReqQueue);

    // Call HttpTerminate.
    HttpTerminate(HTTP_INITIALIZE_SERVER, NULL);
    return;
}

//
// Internal Function Implementations.
//

__inline void TraceA(const char *format, ...)
{
    if(g_bDebug)
    {
        if (format)
        {
            va_list arglist;
            char str[4096];

            va_start(arglist, format);
            StringCchVPrintfA(str, sizeof(str), format, arglist);
            StringCbCatA(str, sizeof(str), "\n");
            if (g_fLogFile)
            {
                FILE *fout = fopen(g_fLogFile, "a+t");
                if (fout)
                {
                    fprintf(fout, str);
                    fclose(fout);
                }
            }

            printf(str);

            if (g_bDebugString)
            {
                
                OutputDebugStringA(str);
            }

            va_end(arglist);
        }
    }
}

__inline void TraceW(const wchar_t *format, ...)
{
    if(g_bDebug)
    {
        if (format)
        {
            va_list arglist;
            wchar_t str[4096];

            va_start(arglist, format);
            StringCchVPrintfW(str, 4096, format, arglist);
            StringCbCatW(str, sizeof(str), L"\n");
            if (g_fLogFile)
            {
                FILE *fout = fopen(g_fLogFile, "a+t");
                if (fout)
                {
                    fwprintf(fout, str);
                    fclose(fout);
                }
            }
            
            wprintf(str);
            
            if (g_bDebugString)
            {               
                OutputDebugStringW(str);
            }

            va_end(arglist);
        }
    }
}

/*
 * This is a blocking function that merely sits on the request queue
 * for our URI and processes them one at a time.  Once a request comes
 * in, we check it for content-type, content-length, and verb.  As long
 * as the initial validations are done, we pass the request to the 
 * processRPCCall() function, which collects the body of the request
 * and processes it.  If we get an error back other than network type,
 * we are responsible for notifing the client.
 */
DWORD
DoReceiveRequests(
    IN HANDLE hReqQueue,
    const xmlrpc_server_httpsys_parms * const parmsP
    )
{
    ULONG              result;
    HTTP_REQUEST_ID    requestId;
    DWORD              bytesRead;
    PHTTP_REQUEST      pRequest;
    PCHAR              pRequestBuffer;
    ULONG              RequestBufferLength;
    xmlrpc_env          env;
    char                szHeaderBuf[255];
    long                lContentLength;

    // Allocate a 2K buffer. Should be good for most requests, we'll grow 
    // this if required. We also need space for a HTTP_REQUEST structure.
    RequestBufferLength = sizeof(HTTP_REQUEST) + 2048;
    pRequestBuffer      = (PCHAR) ALLOC_MEM( RequestBufferLength );
    if (pRequestBuffer == NULL)
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    pRequest = (PHTTP_REQUEST)pRequestBuffer;

    // Wait for a new request -- This is indicated by a NULL request ID.
    HTTP_SET_NULL_ID( &requestId );
    for(;;)
    {
        RtlZeroMemory(pRequest, RequestBufferLength);

        result = HttpReceiveHttpRequest(
            hReqQueue,          // Req Queue
            requestId,          // Req ID
            0,                  // Flags
            pRequest,           // HTTP request buffer
            RequestBufferLength,// req buffer length
            &bytesRead,         // bytes received
            NULL                // LPOVERLAPPED
            );

        if(NO_ERROR == result)
        {
            // Got a request with a filled buffer.
            switch(pRequest->Verb)
            {
                case HttpVerbPOST:

                    TraceW(L"Got a POST request for %ws",
                           pRequest->CookedUrl.pFullUrl);              
                    
                    //Check if we need use authorization.
                    if(parmsP->authfn)
                    {
                        xmlrpc_env_init(&env);
                        if(pRequest->Headers.KnownHeaders[
                            HttpHeaderAuthorization].RawValueLength
                           < 6)
                        {
                            xmlrpc_env_set_fault(
                                &env, XMLRPC_REQUEST_REFUSED_ERROR, 
                                "Authorization header too short.");
                        }
                        else
                        {
                            //unencode the headers
                            if(_strnicmp(
                                "basic",
                                pRequest->Headers.KnownHeaders[
                                    HttpHeaderAuthorization].pRawValue,5)
                               !=0)
                            {
#ifndef  NDEBUG
                                PCHAR pTmp = (PCHAR)
                                    ALLOC_MEM(pRequest->Headers.KnownHeaders[
                                        HttpHeaderAuthorization
                                        ].RawValueLength + 1 );
                                if( pTmp ) {
                                    strncpy(pTmp,
                                            pRequest->Headers.KnownHeaders[
                                                HttpHeaderAuthorization
                                                ].pRawValue,
                                            pRequest->Headers.KnownHeaders[
                                                HttpHeaderAuthorization
                                                ].RawValueLength );
                                    pTmp[pRequest->Headers.KnownHeaders[
                                        HttpHeaderAuthorization
                                        ].RawValueLength] = 0;
                                    TraceA("Got HEADER [%s]",pTmp);
                                    FREE_MEM(pTmp);
                                }
#endif   /* #ifndef NDEBUG */
                                xmlrpc_env_set_fault(
                                    &env, XMLRPC_REQUEST_REFUSED_ERROR, 
                                    "Authorization header does not start "
                                    "with type 'basic'!");
                            }
                            else
                            {
                                xmlrpc_mem_block * decoded;
                                
                                decoded =
                                    xmlrpc_base64_decode(
                                        &env,
                                        pRequest->Headers.KnownHeaders[
                                            HttpHeaderAuthorization
                                            ].pRawValue+6,
                                        pRequest->Headers.KnownHeaders[
                                            HttpHeaderAuthorization
                                            ].RawValueLength-6);
                                if(!env.fault_occurred)
                                {
                                    char *pDecodedStr;
                                    char *pUser;
                                    char *pPass;
                                    char *pColon;
                                    
                                    pDecodedStr = (char*)
                                        malloc(decoded->_size+1);
                                    memcpy(pDecodedStr,
                                           decoded->_block,
                                           decoded->_size);
                                    pDecodedStr[decoded->_size]='\0';
                                    pUser = pPass = pDecodedStr;
                                    pColon=strchr(pDecodedStr,':');
                                    if(pColon)
                                    {
                                        *pColon='\0';
                                        pPass=pColon+1;
                                        //The authfn should set env to
                                        //fail if auth is denied.
                                        parmsP->authfn(&env,pUser,pPass);
                                    }
                                    else
                                    {
                                        xmlrpc_env_set_fault(
                                            &env,
                                            XMLRPC_REQUEST_REFUSED_ERROR, 
                                            "Decoded auth not of the correct "
                                            "format.");
                                    }
                                    free(pDecodedStr);
                                }
                                if(decoded)
                                    XMLRPC_MEMBLOCK_FREE(char, decoded);
                            }
                        }
                        if(env.fault_occurred)
                        {
                            //request basic authorization, as the user
                            //did not provide it.
                            xmlrpc_env_clean(&env);
                            TraceW(L"POST request did not provide valid "
                                   L"authorization header.");
                            result =
                                SendHttpResponseAuthRequired( hReqQueue,
                                                              pRequest);
                            break;
                        }
                        xmlrpc_env_clean(&env);
                    }
                    
                    //Check content type to make sure it is text/xml.
                    memcpy(szHeaderBuf,
                           pRequest->Headers.KnownHeaders[
                               HttpHeaderContentType
                               ].pRawValue,
                           pRequest->Headers.KnownHeaders[
                               HttpHeaderContentType
                               ].RawValueLength);
                    szHeaderBuf[pRequest->Headers.KnownHeaders[
                        HttpHeaderContentType
                        ].RawValueLength] = '\0';
                    if (_stricmp(szHeaderBuf,"text/xml")!=0)
                    {
                        //We handle only text/xml data.  Anything else
                        //is not valid.
                        TraceW(L"POST request had an unrecognized "
                               L"content-type: %s", szHeaderBuf);
                        result = SendHttpResponse(
                            hReqQueue, 
                            pRequest,
                            400,
                            "Bad Request",
                            NULL
                            );
                        break;
                    }

                    //Check content length to make sure it exists and
                    //is not too big.
                    memcpy(szHeaderBuf,
                           pRequest->Headers.KnownHeaders[
                               HttpHeaderContentLength
                               ].pRawValue,
                           pRequest->Headers.KnownHeaders[
                               HttpHeaderContentLength
                               ].RawValueLength);
                    szHeaderBuf[pRequest->Headers.KnownHeaders[
                        HttpHeaderContentLength
                        ].RawValueLength]='\0';
                    lContentLength = atol(szHeaderBuf);
                    if (lContentLength<=0)
                    {
                        //Make sure a content length was supplied.
                        TraceW(L"POST request did not include a "
                               L"content-length", szHeaderBuf);
                        result = SendHttpResponse(
                            hReqQueue, 
                            pRequest,
                            411,
                            "Length Required",
                            NULL
                            );
                        break;
                    }                       
                    if((size_t) lContentLength >
                       xmlrpc_limit_get(XMLRPC_XML_SIZE_LIMIT_ID))
                    {
                        //Content-length is too big for us to handle
                        TraceW(L"POST request content-length is too big "
                               L"for us to handle: %d bytes",
                               lContentLength);
                        result = SendHttpResponse(
                            hReqQueue, 
                            pRequest,
                            500,
                            "content-length too large",
                            NULL
                            );
                        break;
                    }

                    //our initial validations of POST, content-type,
                    //and content-length all check out.  Collect and
                    //pass the complete buffer to the XMLRPC-C library
                    
                    xmlrpc_env_init(&env);
                    processRPCCall(&env,hReqQueue, pRequest);
                    if (env.fault_occurred) 
                    {
                        //if we fail and it is anything other than a
                        //network error, we should return a failure
                        //response to the client.
                        if (env.fault_code != XMLRPC_NETWORK_ERROR)
                        {
                            if (env.fault_string)
                                result = SendHttpResponse(
                                    hReqQueue, 
                                    pRequest,
                                    500,
                                    env.fault_string,
                                    NULL
                                    );
                            else
                                result = SendHttpResponse(
                                    hReqQueue, 
                                    pRequest,
                                    500,
                                    "Unknown Error",
                                    NULL
                                    );
                        }
                    }
                    
                    xmlrpc_env_clean(&env);
                    break;

                default:
                    //We handle only POST data.  Anything else is not valid.
                    TraceW(L"Got an unrecognized Verb request for URI %ws",
                           pRequest->CookedUrl.pFullUrl);
            
                    result = SendHttpResponse(
                        hReqQueue, 
                        pRequest,
                        405,
                        "Method Not Allowed",
                        NULL
                        );
                    break;
            }
            if(result != NO_ERROR)
            {
                break;
            }

            // Reset the Request ID so that we pick up the next request.
            HTTP_SET_NULL_ID( &requestId );
        }
        else if(result == ERROR_MORE_DATA)
        {
            // The input buffer was too small to hold the request headers
            // We have to allocate more buffer & call the API again. 
            //
            // When we call the API again, we want to pick up the request
            // that just failed. This is done by passing a RequestID.
            // This RequestID is picked from the old buffer.
            requestId = pRequest->RequestId;

            // Free the old buffer and allocate a new one.
            RequestBufferLength = bytesRead;
            FREE_MEM( pRequestBuffer );
            pRequestBuffer = (PCHAR) ALLOC_MEM( RequestBufferLength );

            if (pRequestBuffer == NULL)
            {
                result = ERROR_NOT_ENOUGH_MEMORY;
                break;
            }

            pRequest = (PHTTP_REQUEST)pRequestBuffer;

        }
        else if(ERROR_CONNECTION_INVALID == result && 
                !HTTP_IS_NULL_ID(&requestId))
        {
            // The TCP connection got torn down by the peer when we were
            // trying to pick up a request with more buffer. We'll just move
            // onto the next request.            
            HTTP_SET_NULL_ID( &requestId );
        }
        else
        {
            break;
        }

    } // for(;;)

    if(pRequestBuffer)
    {
        FREE_MEM( pRequestBuffer );
    }

    return result;
}

/*
 * SendHttpResponse sends a text/html content type back with
 * the user specified status code and reason.  Used for returning
 * errors to clients.
 */
DWORD
SendHttpResponse(
    IN HANDLE        hReqQueue,
    IN PHTTP_REQUEST pRequest,
    IN USHORT        StatusCode,
    IN PSTR          pReason,
    IN PSTR          pEntityString
    )
{
    HTTP_RESPONSE   response;
    HTTP_DATA_CHUNK dataChunk;
    DWORD           result;
    DWORD           bytesSent;
    CHAR            szServerHeader[20];

    // Initialize the HTTP response structure.
    INITIALIZE_HTTP_RESPONSE(&response, StatusCode, pReason);

    ADD_KNOWN_HEADER(response, HttpHeaderContentType, "text/html");
    
    StringCchPrintfA(szServerHeader, sizeof(szServerHeader), "Xmlrpc-c/%s",
                     XMLRPC_C_VERSION);
    ADD_KNOWN_HEADER(response, HttpHeaderServer, szServerHeader);
   
    if(pEntityString)
    {
        // Add an entity chunk
        dataChunk.DataChunkType           = HttpDataChunkFromMemory;
        dataChunk.FromMemory.pBuffer      = pEntityString;
        dataChunk.FromMemory.BufferLength = (ULONG) strlen(pEntityString);

        response.EntityChunkCount         = 1;
        response.pEntityChunks            = &dataChunk;
    }

    // Since we are sending all the entity body in one call, we don't have 
    // to specify the Content-Length.
    result = HttpSendHttpResponse(
        hReqQueue,           // ReqQueueHandle
        pRequest->RequestId, // Request ID
        0,                   // Flags
        &response,           // HTTP response
        NULL,                // pReserved1
        &bytesSent,          // bytes sent   (OPTIONAL)
        NULL,                // pReserved2   (must be NULL)
        0,                   // Reserved3    (must be 0)
        NULL,                // LPOVERLAPPED (OPTIONAL)
        NULL                 // pReserved4   (must be NULL)
        );

    if(result != NO_ERROR)
    {
        TraceW(L"HttpSendHttpResponse failed with %lu", result);
    }

    return result;
}

/* SendHttpResponseAuthRequired sends a 401 status code requesting
 * authorization
 */

DWORD
SendHttpResponseAuthRequired(
    IN HANDLE        hReqQueue,
    IN PHTTP_REQUEST pRequest
    )
{
    HTTP_RESPONSE   response;
    DWORD           result;
    DWORD           bytesSent;
    CHAR            szServerHeader[20];

    // Initialize the HTTP response structure.
    INITIALIZE_HTTP_RESPONSE(&response, 401, "Authentication Required");

    // Add the WWW_Authenticate header.
    ADD_KNOWN_HEADER(response, HttpHeaderWwwAuthenticate,
                     "Basic realm=\"xmlrpc\"");
    
    StringCchPrintfA(szServerHeader, sizeof(szServerHeader), "Xmlrpc-c/%s",
                     XMLRPC_C_VERSION);
    ADD_KNOWN_HEADER(response, HttpHeaderServer, szServerHeader);
   
    // Since we are sending all the entity body in one call, we don't have 
    // to specify the Content-Length.
    result = HttpSendHttpResponse(
        hReqQueue,           // ReqQueueHandle
        pRequest->RequestId, // Request ID
        0,                   // Flags
        &response,           // HTTP response
        NULL,                // pReserved1
        &bytesSent,          // bytes sent   (OPTIONAL)
        NULL,                // pReserved2   (must be NULL)
        0,                   // Reserved3    (must be 0)
        NULL,                // LPOVERLAPPED (OPTIONAL)
        NULL                 // pReserved4   (must be NULL)
        );

    if(result != NO_ERROR)
    {
        TraceW(L"SendHttpResponseAuthRequired failed with %lu", result);
    }

    return result;
}

/*
 * processRPCCall() is called after some validations.  The assumption is that
 * the request is an HTTP post of content-type text/xml with a content-length
 * that is less than the maximum the library can handle.
 *
 * The caller should check the error status, and if the error was other than
 * a network type, respond back to the client to let them know the call failed.
 */
void
processRPCCall(
    xmlrpc_env *     const envP,
    IN HANDLE        hReqQueue,
    IN PHTTP_REQUEST pRequest
    )
{
    HTTP_RESPONSE   response;
    DWORD           result;
    DWORD           bytesSent;
    PUCHAR          pEntityBuffer;
    ULONG           EntityBufferLength;
    ULONG           BytesRead;
#define MAX_ULONG_STR ((ULONG) sizeof("4294967295"))
    CHAR            szContentLength[MAX_ULONG_STR];
    CHAR            szServerHeader[20];
    HTTP_DATA_CHUNK dataChunk;
    ULONG           TotalBytesRead = 0;
    xmlrpc_mem_block * body;
    xmlrpc_mem_block * output;

    BytesRead  = 0;
    body       = NULL;
    output     = NULL;

    // Allocate some space for an entity buffer.
    EntityBufferLength = 2048;  
    pEntityBuffer      = (PUCHAR) ALLOC_MEM( EntityBufferLength );
    if (pEntityBuffer == NULL)
    {
        xmlrpc_faultf(envP, "Out of Memory");
        goto Done;
    }

    // NOTE: If we had passed the HTTP_RECEIVE_REQUEST_FLAG_COPY_BODY
    //       flag with HttpReceiveHttpRequest(), the entity would have
    //       been a part of HTTP_REQUEST (using the pEntityChunks field).
    //       Since we have not passed that flag, we can be assured that 
    //       there are no entity bodies in HTTP_REQUEST.
    if(pRequest->Flags & HTTP_REQUEST_FLAG_MORE_ENTITY_BODY_EXISTS)
    {
        //Allocate some space for an XMLRPC memory block.
        body = xmlrpc_mem_block_new(envP, 0);
        if (envP->fault_occurred) 
            goto Done;

        // The entity body can be sent over multiple calls. Let's collect all
        // of these in a buffer and send the buffer to the xmlrpc-c library 
        do
        {
            // Read the entity chunk from the request.
            BytesRead = 0; 
            result = HttpReceiveRequestEntityBody(
                hReqQueue,
                pRequest->RequestId,
                0,
                pEntityBuffer,
                EntityBufferLength,
                &BytesRead,
                NULL
                );
            switch(result)
            {
                case NO_ERROR:
                    if(BytesRead != 0)
                    {
                        XMLRPC_MEMBLOCK_APPEND(char, envP, body, 
                                               pEntityBuffer, BytesRead);
                        if(envP->fault_occurred)
                            goto Done;                      
                    }
                    break;

                case ERROR_HANDLE_EOF:
                    // We have read the last request entity body. We can now 
                    // process the suppossed XMLRPC data.
                    if(BytesRead != 0)
                    {
                        XMLRPC_MEMBLOCK_APPEND(char, envP, body, 
                                               pEntityBuffer, BytesRead);
                        if(envP->fault_occurred)
                            goto Done;
                    }

                    // We will send the response over multiple calls. 
                    // This is achieved by passing the 
                    // HTTP_SEND_RESPONSE_FLAG_MORE_DATA flag.
                    
                    // NOTE: Since we are accumulating the TotalBytesRead in 
                    //       a ULONG, this will not work for entity bodies that
                    //       are larger than 4 GB. To work with large entity
                    //       bodies, we would have to use a ULONGLONG.
                    TraceA("xmlrpc_server RPC2 handler processing "
                           "RPC request.");
                                        
                    // Process the RPC.
                    xmlrpc_registry_process_call2(
                        envP, global_registryP,
                        XMLRPC_MEMBLOCK_CONTENTS(char, body),
                        XMLRPC_MEMBLOCK_SIZE(char, body),
                        NULL,
                        &output);
                    if (envP->fault_occurred) 
                        goto Done;

                    // Initialize the HTTP response structure.
                    INITIALIZE_HTTP_RESPONSE(&response, 200, "OK");

                    //Add the content-length
                    StringCchPrintfA(szContentLength,MAX_ULONG_STR, "%lu",
                                     XMLRPC_MEMBLOCK_SIZE(char, output));
                    ADD_KNOWN_HEADER(
                            response, 
                            HttpHeaderContentLength, 
                            szContentLength );

                    //Add the content-type
                    ADD_KNOWN_HEADER(response, HttpHeaderContentType,
                                     "text/xml");
                    
                    StringCchPrintfA(szServerHeader, sizeof(szServerHeader),
                                     "Xmlrpc-c/%s", XMLRPC_C_VERSION);
                    ADD_KNOWN_HEADER(response, HttpHeaderServer,
                                     szServerHeader);

                    //send the response
                    result = HttpSendHttpResponse(
                        hReqQueue,           // ReqQueueHandle
                        pRequest->RequestId, // Request ID
                        HTTP_SEND_RESPONSE_FLAG_MORE_DATA,
                        &response,           // HTTP response
                        NULL,                // pReserved1
                        &bytesSent,          // bytes sent (optional)
                        NULL,                // pReserved2
                        0,                   // Reserved3
                        NULL,                // LPOVERLAPPED
                        NULL                 // pReserved4
                        );
                    if(result != NO_ERROR)
                    {
                        TraceW(L"HttpSendHttpResponse failed with %lu",
                               result);
                        xmlrpc_env_set_fault_formatted(
                            envP, XMLRPC_NETWORK_ERROR,
                            "HttpSendHttpResponse failed with %lu", result);
                        goto Done;
                    }

                    // Send entity body from a memory chunk.
                    dataChunk.DataChunkType = HttpDataChunkFromMemory;
                    dataChunk.FromMemory.BufferLength =
                        (ULONG)XMLRPC_MEMBLOCK_SIZE(char, output);
                    dataChunk.FromMemory.pBuffer =
                        XMLRPC_MEMBLOCK_CONTENTS(char, output);

                    result = HttpSendResponseEntityBody(
                        hReqQueue,
                        pRequest->RequestId,
                        0,                    // This is the last send.
                        1,                    // Entity Chunk Count.
                        &dataChunk,
                        NULL,
                        NULL,
                        0,
                        NULL,
                        NULL
                        );
                    if(result != NO_ERROR)
                    {
                        TraceW(L"HttpSendResponseEntityBody failed "
                               L"with %lu", result);
                        xmlrpc_env_set_fault_formatted(
                                envP, XMLRPC_NETWORK_ERROR,
                                "HttpSendResponseEntityBody failed with %lu",
                                result);
                        goto Done;
                    }
                    goto Done;
                    break;
                default:
                    TraceW(L"HttpReceiveRequestEntityBody failed with %lu",
                           result);
                    xmlrpc_env_set_fault_formatted(
                                envP, XMLRPC_NETWORK_ERROR,
                                "HttpReceiveRequestEntityBody failed "
                                "with %lu", result);
                    goto Done;
            }
        } while(TRUE);
    }
    else
    {
        // This request does not have an entity body. 
        TraceA("Received a bad request (no body in HTTP post).");
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR,
            "Bad POST request (no body)");
        goto Done;
    }

Done:

    if(pEntityBuffer)
        FREE_MEM(pEntityBuffer);

    if(output)
        XMLRPC_MEMBLOCK_FREE(char, output);

    if(body)
        XMLRPC_MEMBLOCK_FREE(char, body);

    return;
}



/* Copyright (C) 2005 by Steven A. Bone, sbone@pobox.com. All rights reserved.
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

