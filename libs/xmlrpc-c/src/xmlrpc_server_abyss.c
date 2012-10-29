/* Copyright information is at the end of the file */

#include "xmlrpc_config.h"

#define _XOPEN_SOURCE 600  /* For strdup(), sigaction */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#ifdef _WIN32
#  include <io.h>
#else
#  include <signal.h>
#  include <sys/wait.h>
#  include <grp.h>
#endif

#include "bool.h"
#include "int.h"
#include "mallocvar.h"
#include "xmlrpc-c/abyss.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/server.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/server_abyss.h"



struct xmlrpc_server_abyss {
    TServer       abyssServer;
    TChanSwitch * chanSwitchP;
    bool          shutdownEnabled;
        /* User wants system.shutdown to succeed */
};



static void
dieIfFaultOccurred(xmlrpc_env * const envP) {

    if (envP->fault_occurred) {
        fprintf(stderr, "Unexpected XML-RPC fault: %s (%d)\n",
                envP->fault_string, envP->fault_code);
        exit(1);
    }
}



static void
initAbyss(xmlrpc_env * const envP) {

    const char * error;
    AbyssInit(&error);
    if (error) {
        xmlrpc_faultf(envP, "Failed to initialize the Abyss library.  %s",
                      error);
        xmlrpc_strfree(error);
    }
}



static void
termAbyss(void) {

    AbyssTerm();
}



static unsigned int globallyInitialized = 0;
    /* Initialization count */


void
xmlrpc_server_abyss_global_init(xmlrpc_env * const envP) {
    
    /* Note that this is specified as not thread safe; user calls it at
       the beginning of his program, when it is only one thread.
    */

    XMLRPC_ASSERT_ENV_OK(envP);
    
    if (globallyInitialized == 0)
        initAbyss(envP);

    ++globallyInitialized;
}



void
xmlrpc_server_abyss_global_term(void) {

    /* Note that this is specified as not thread safe; user calls it at
       the end of his program, when it is only one thread.
    */

    XMLRPC_ASSERT(globallyInitialized > 0);

    --globallyInitialized;

    if (globallyInitialized == 0)
        termAbyss();
}



static void
validateGlobalInit(xmlrpc_env * const envP) {

    if (!globallyInitialized)
        xmlrpc_faultf(envP, "libxmlrpc_server_abyss has not been globally "
                      "initialized.  See xmlrpc_server_abyss_init()");
}



static void
addAuthCookie(xmlrpc_env * const envP,
              TSession *   const abyssSessionP,
              const char * const authCookie) {

    const char * cookieResponse;
    
    xmlrpc_asprintf(&cookieResponse, "auth=%s", authCookie);
    
    if (xmlrpc_strnomem(cookieResponse))
        xmlrpc_faultf(envP, "Insufficient memory to generate cookie "
                      "response header.");
    else {
        ResponseAddField(abyssSessionP, "Set-Cookie", cookieResponse);
    
        xmlrpc_strfree(cookieResponse);
    }
}   
    


static void 
sendResponse(xmlrpc_env *      const envP,
             TSession *        const abyssSessionP, 
             const char *      const body, 
             size_t            const len,
             bool              const chunked,
             ResponseAccessCtl const accessControl) {
/*----------------------------------------------------------------------------
   Generate an HTTP response containing body 'body' of length 'len'
   characters.

   This is meant to run in the context of an Abyss URI handler for
   Abyss session 'abyssSessionP'.
-----------------------------------------------------------------------------*/
    const char * http_cookie = NULL;
        /* This used to set http_cookie to getenv("HTTP_COOKIE"), but
           that doesn't make any sense -- environment variables are not
           appropriate for this.  So for now, cookie code is disabled.
           - Bryan 2004.10.03.
        */

    /* Various bugs before Xmlrpc-c 1.05 caused the response to be not
       chunked in the most basic case, but chunked if the client explicitly
       requested keepalive.  I think it's better not to chunk, because
       it's simpler, so I removed this in 1.05.  I don't know what the
       purpose of chunking would be, and an original comment suggests
       the author wasn't sure chunking was a good idea.

       In 1.06 we added the user option to chunk.
    */
    if (chunked)
        ResponseChunked(abyssSessionP);

    ResponseStatus(abyssSessionP, 200);

    if (http_cookie)
        /* There's an auth cookie, so pass it back in the response. */
        addAuthCookie(envP, abyssSessionP, http_cookie);

    if ((size_t)(uint32_t)len != len)
        xmlrpc_faultf(envP, "XML-RPC method generated a response too "
                      "large for Abyss to send");
    else {
        uint32_t const abyssLen = (uint32_t)len;

        /* See discussion below of quotes around "utf-8" */
        ResponseContentType(abyssSessionP, "text/xml");
        ResponseContentLength(abyssSessionP, abyssLen);
        ResponseAccessControl(abyssSessionP, accessControl);
        
        if (ResponseWriteStart(abyssSessionP))
			if (ResponseWriteBody(abyssSessionP, body, abyssLen))
				if (ResponseWriteEnd(abyssSessionP))
					return;

        xmlrpc_faultf(envP, "socket send() problem");
    }
}



/* From 0.9.10 (May 2001) through 1.17 (December 2008), the content-type
   header said charset="utf-8" (i.e. with the value of 'charset' an HTTP quoted
   string).  Before 0.9.10, the header didn't have charset at all.

   We got a complaint in January 2009 that some client didn't understand that,
   saying

     apache2: XML-RPC: xmlrpcmsg::parseResponse: invalid charset encoding of
     received response: "UTF-8"

   And that removing the quotation marks fixes this.

   From what I can tell, the module is wrong to distinguish between the
   two, but I don't think it hurts anything to use a basic HTTP token instead
   of an HTTP quoted string here, so starting in 1.18, we do.  */


static void
sendError(TSession *   const abyssSessionP, 
          unsigned int const status,
          const char * const explanation) {
/*----------------------------------------------------------------------------
  Send an error response back to the client.
-----------------------------------------------------------------------------*/
    ResponseStatus(abyssSessionP, (uint16_t) status);
    ResponseError2(abyssSessionP, explanation);
}



static void
traceChunkRead(TSession * const abyssSessionP) {

    fprintf(stderr, "XML-RPC handler got a chunk of %u bytes\n",
            (unsigned int)SessionReadDataAvail(abyssSessionP));
}



static void
refillBufferFromConnection(xmlrpc_env * const envP,
                           TSession *   const abyssSessionP,
                           const char * const trace) {
/*----------------------------------------------------------------------------
   Get the next chunk of data from the connection into the buffer.
-----------------------------------------------------------------------------*/
    bool succeeded;

    succeeded = SessionRefillBuffer(abyssSessionP);

    if (!succeeded)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_TIMEOUT_ERROR, "Timed out waiting for "
            "client to send its POST data");
    else {
        if (trace)
            traceChunkRead(abyssSessionP);
    }
}



static void
getBody(xmlrpc_env *        const envP,
        TSession *          const abyssSessionP,
        size_t              const contentSize,
        const char *        const trace,
        xmlrpc_mem_block ** const bodyP) {
/*----------------------------------------------------------------------------
   Get the entire body, which is of size 'contentSize' bytes, from the
   Abyss session and return it as the new memblock *bodyP.

   The first chunk of the body may already be in Abyss's buffer.  We
   retrieve that before reading more.
-----------------------------------------------------------------------------*/
    xmlrpc_mem_block * body;

    if (trace)
        fprintf(stderr, "XML-RPC handler processing body.  "
                "Content Size = %u bytes\n", (unsigned)contentSize);

    body = xmlrpc_mem_block_new(envP, 0);
    if (!envP->fault_occurred) {
        size_t bytesRead;
        const char * chunkPtr;
        size_t chunkLen;

        bytesRead = 0;

        while (!envP->fault_occurred && bytesRead < contentSize) {
            SessionGetReadData(abyssSessionP, contentSize - bytesRead, 
                               &chunkPtr, &chunkLen);
            bytesRead += chunkLen;

            assert(bytesRead <= contentSize);

            XMLRPC_MEMBLOCK_APPEND(char, envP, body, chunkPtr, chunkLen);
            if (bytesRead < contentSize)
                refillBufferFromConnection(envP, abyssSessionP, trace);
        }
        if (envP->fault_occurred)
            xmlrpc_mem_block_free(body);
    }
    *bodyP = body;
}



static void
storeCookies(TSession *     const httpRequestP,
             const char **  const errorP) {
/*----------------------------------------------------------------------------
   Get the cookie settings from the HTTP headers and remember them for
   use in responses.
-----------------------------------------------------------------------------*/
    const char * const cookie = RequestHeaderValue(httpRequestP, "cookie");
    if (cookie) {
        /* 
           Setting the value in an environment variable doesn't make
           any sense.  So for now, cookie code is disabled.
           -Bryan 04.10.03.

        setenv("HTTP_COOKIE", cookie, 1);
        */
    }
    /* TODO: parse HTTP_COOKIE to find auth pair, if there is one */

    *errorP = NULL;
}




static void
processContentLength(TSession *    const httpRequestP,
                     size_t *      const inputLenP,
                     bool *        const missingP,
                     const char ** const errorP) {
/*----------------------------------------------------------------------------
  Make sure the content length is present and non-zero.  This is
  technically required by XML-RPC, but we only enforce it because we
  don't want to figure out how to safely handle HTTP < 1.1 requests
  without it.
-----------------------------------------------------------------------------*/
    const char * const content_length = 
        RequestHeaderValue(httpRequestP, "content-length");

    if (content_length == NULL) {
        *missingP = TRUE;
        *errorP = NULL;
    } else {
        *missingP = FALSE;
        *inputLenP = 0;  /* quiet compiler warning */
        if (content_length[0] == '\0')
            xmlrpc_asprintf(errorP, "The value in your content-length "
                            "HTTP header value is a null string");
        else {
            unsigned long contentLengthValue;
            char * tail;
        
            contentLengthValue = strtoul(content_length, &tail, 10);
        
            if (*tail != '\0')
                xmlrpc_asprintf(errorP, "There's non-numeric crap in "
                                "the value of your content-length "
                                "HTTP header: '%s'", tail);
            else if (contentLengthValue < 1)
                xmlrpc_asprintf(errorP, "According to your content-length "
                                "HTTP header, your request is empty (zero "
                                "length)");
            else if ((unsigned long)(size_t)contentLengthValue 
                     != contentLengthValue)
                xmlrpc_asprintf(errorP, "According to your content-length "
                                "HTTP header, your request is too big to "
                                "process; we can't even do arithmetic on its "
                                "size: %s bytes", content_length);
            else {
                *errorP = NULL;
                *inputLenP = (size_t)contentLengthValue;
            }
        }
    }
}



static void
traceHandlerCalled(TSession * const abyssSessionP) {
    
    const char * methodDesc;
    const TRequestInfo * requestInfoP;

    fprintf(stderr, "xmlrpc_server_abyss URI path handler called.\n");

    SessionGetRequestInfo(abyssSessionP, &requestInfoP);

    fprintf(stderr, "URI = '%s'\n", requestInfoP->uri);

    switch (requestInfoP->method) {
    case m_unknown: methodDesc = "unknown";   break;
    case m_get:     methodDesc = "get";       break;
    case m_put:     methodDesc = "put";       break;
    case m_head:    methodDesc = "head";      break;
    case m_post:    methodDesc = "post";      break;
    case m_delete:  methodDesc = "delete";    break;
    case m_trace:   methodDesc = "trace";     break;
    case m_options: methodDesc = "m_options"; break;
    default:        methodDesc = "?";
    }
    fprintf(stderr, "HTTP method = '%s'\n", methodDesc);

    if (requestInfoP->query)
        fprintf(stderr, "query (component of URL)='%s'\n",
                requestInfoP->query);
    else
        fprintf(stderr, "URL has no query component\n");
}



static void
processCall(TSession *            const abyssSessionP,
            size_t                const contentSize,
            xmlrpc_call_processor       xmlProcessor,
            void *                const xmlProcessorArg,
            bool                  const wantChunk,
            ResponseAccessCtl     const accessControl,
            const char *          const trace) {
/*----------------------------------------------------------------------------
   Handle an RPC request.  This is an HTTP request that has the proper form
   to be an XML-RPC call.

   The text of the call is available through the Abyss session
   'abyssSessionP'.

   Its content length is 'contentSize' bytes.
-----------------------------------------------------------------------------*/
    xmlrpc_env env;

    if (trace)
        fprintf(stderr,
                "xmlrpc_server_abyss URI path handler processing RPC.\n");

    xmlrpc_env_init(&env);

    if (contentSize > xmlrpc_limit_get(XMLRPC_XML_SIZE_LIMIT_ID))
        xmlrpc_env_set_fault_formatted(
            &env, XMLRPC_LIMIT_EXCEEDED_ERROR,
            "XML-RPC request too large (%u bytes)", (unsigned)contentSize);
    else {
        xmlrpc_mem_block * body = NULL;
        /* Read XML data off the wire. */
        getBody(&env, abyssSessionP, contentSize, trace, &body);
        if (!env.fault_occurred) {
            xmlrpc_mem_block * output;

            /* Process the RPC. */
            xmlProcessor(
                &env, xmlProcessorArg,
                XMLRPC_MEMBLOCK_CONTENTS(char, body),
                XMLRPC_MEMBLOCK_SIZE(char, body),
                abyssSessionP,
                &output);
            if (!env.fault_occurred) {
                /* Send out the result. */
                sendResponse(&env, abyssSessionP, 
                             XMLRPC_MEMBLOCK_CONTENTS(char, output),
                             XMLRPC_MEMBLOCK_SIZE(char, output),
                             wantChunk, accessControl);
                
                XMLRPC_MEMBLOCK_FREE(char, output);
            }
            XMLRPC_MEMBLOCK_FREE(char, body);
        }
    }
    if (env.fault_occurred) {
        uint16_t httpResponseStatus;
        if (env.fault_code == XMLRPC_TIMEOUT_ERROR)
            httpResponseStatus = 408;  /* Request Timeout */
        else
            httpResponseStatus = 500;  /* Internal Server Error */

        sendError(abyssSessionP, httpResponseStatus, env.fault_string);
    }

    xmlrpc_env_clean(&env);
}



static void
processXmlrpcCall(xmlrpc_env *        const envP,
                  void *              const arg,
                  const char *        const callXml,
                  size_t              const callXmlLen,
                  TSession *          const abyssSessionP,                  
                  xmlrpc_mem_block ** const responseXmlPP) {

    xmlrpc_registry * const registryP = arg;

    xmlrpc_registry_process_call2(envP, registryP,
                                  callXml, callXmlLen, abyssSessionP,
                                  responseXmlPP);

}



static const char * trace_abyss;


struct uriHandlerXmlrpc {
/*----------------------------------------------------------------------------
   This is the part of an Abyss HTTP request handler (aka URI handler)
   that is specific to the Xmlrpc-c handler.
-----------------------------------------------------------------------------*/
    xmlrpc_registry *       registryP;
    const char *            uriPath;  /* malloc'ed */
    bool                    chunkResponse;
        /* The handler should chunk its response whenever possible */
    xmlrpc_call_processor * xmlProcessor;
    void *                  xmlProcessorArg;
    ResponseAccessCtl       accessControl;
};



static void
termAccessControl(ResponseAccessCtl * const accessCtlP) {

    xmlrpc_strfreenull(accessCtlP->allowOrigin);
}



static void
termUriHandler(void * const arg) {

    struct uriHandlerXmlrpc * const uriHandlerXmlrpcP = arg;

    xmlrpc_strfree(uriHandlerXmlrpcP->uriPath);
    termAccessControl(&uriHandlerXmlrpcP->accessControl);
    free(uriHandlerXmlrpcP);
}



static void
handleXmlRpcCallReq(TSession *           const abyssSessionP,
                    const TRequestInfo * const requestInfoP ATTR_UNUSED,
                    xmlrpc_call_processor      xmlProcessor,
                    void *               const xmlProcessorArg,
                    bool                 const wantChunk,
                    ResponseAccessCtl    const accessControl) {
/*----------------------------------------------------------------------------
   Handle the HTTP request described by *requestInfoP, which arrived over
   Abyss HTTP session *abyssSessionP, which is an XML-RPC call
   (i.e. a POST request to /RPC2 or whatever other URI our server is
   supposed to handle).

   Handle it by feeding the XML which is its content to 'xmlProcessor'
   along with argument 'xmlProcessorArg'.
-----------------------------------------------------------------------------*/
    /* We used to reject the call if content-type was not present and
       text/xml, on some security theory (a firewall may block text/xml with
       the intent of blocking XML-RPC.  Now, we believe that is silly, and we
       have seen an incorrectly implemented client that says text/plain.
    */
    const char * error;

    assert(requestInfoP->method == m_post);

    storeCookies(abyssSessionP, &error);
    if (error) {
        sendError(abyssSessionP, 400, error);
        xmlrpc_strfree(error);
    } else {
        const char * error;
        bool missing;
        size_t contentSize;

        processContentLength(abyssSessionP, 
                             &contentSize, &missing, &error);
        if (error) {
            sendError(abyssSessionP, 400, error);
            xmlrpc_strfree(error);
        } else {
            if (missing)
                sendError(abyssSessionP, 411, "You must send a "
                          "content-length HTTP header in an "
                          "XML-RPC call.");
            else
                processCall(abyssSessionP, contentSize,
                            xmlProcessor, xmlProcessorArg,
                            wantChunk, accessControl,
                            trace_abyss);
        }
    }
}



static void
handleXmlRpcOptionsReq(TSession *        const abyssSessionP,
                       ResponseAccessCtl const accessControl) {

    ResponseAddField(abyssSessionP, "Allow", "POST");
    
    ResponseAccessControl(abyssSessionP, accessControl);
    ResponseContentLength(abyssSessionP, 0);
    ResponseStatus(abyssSessionP, 200);
    if (ResponseWriteStart(abyssSessionP))
		ResponseWriteEnd(abyssSessionP);
}



static void
handleIfXmlrpcReq(void *        const handlerArg,
                  TSession *    const abyssSessionP,
                  abyss_bool *  const handledP) {
/*----------------------------------------------------------------------------
   Our job is to look at this HTTP request that the Abyss server is
   trying to process and see if we can handle it.  If it's an XML-RPC
   call for this XML-RPC server, we handle it.  If it's not, we refuse
   it and Abyss can try some other handler.

   Our return code is TRUE to mean we handled it; FALSE to mean we didn't.

   Note that failing the request counts as handling it, and not handling
   it does not mean we failed it.

   This is an Abyss HTTP Request handler -- type handleReqFn3.
-----------------------------------------------------------------------------*/
    struct uriHandlerXmlrpc * const uriHandlerXmlrpcP = handlerArg;

    const TRequestInfo * requestInfoP;

    if (trace_abyss)
        traceHandlerCalled(abyssSessionP);

    SessionGetRequestInfo(abyssSessionP, &requestInfoP);

    /* Note that requestInfoP->uri is not the whole URI.  It is just
       the "file name" part of it.
    */
    if (!xmlrpc_streq(requestInfoP->uri, uriHandlerXmlrpcP->uriPath))
        /* It's not for the path (e.g. "/RPC2") that we're supposed to
           handle.
        */
        *handledP = FALSE;
    else {
        *handledP = TRUE;

        switch (requestInfoP->method) {
        case m_post:
            handleXmlRpcCallReq(abyssSessionP, requestInfoP,
                                uriHandlerXmlrpcP->xmlProcessor,
                                uriHandlerXmlrpcP->xmlProcessorArg,
                                uriHandlerXmlrpcP->chunkResponse,
                                uriHandlerXmlrpcP->accessControl);
            break;
        case m_options:
            handleXmlRpcOptionsReq(abyssSessionP,
                                   uriHandlerXmlrpcP->accessControl);
            break;
        default:
            sendError(abyssSessionP, 405,
                      "POST is the only HTTP method this server understands");
                /* 405 = Method Not Allowed */
        }
    }
    if (trace_abyss)
        fprintf(stderr, "xmlrpc_server_abyss URI path handler returning.\n");
}


/* This doesn't include what the user's method function requires */
#define HANDLE_XMLRPC_REQ_STACK 1024



/*=========================================================================
**  xmlrpc_server_abyss_default_handler
**=========================================================================
**  This handler returns a 404 Not Found for all requests. See the header
**  for more documentation.
*/

static xmlrpc_bool 
xmlrpc_server_abyss_default_handler(TSession * const sessionP) {

    const TRequestInfo * requestInfoP;

    const char * explanation;

    if (trace_abyss)
        fprintf(stderr, "xmlrpc_server_abyss default handler called.\n");

    SessionGetRequestInfo(sessionP, &requestInfoP);

    xmlrpc_asprintf(
        &explanation,
        "This XML-RPC For C/C++ Abyss XML-RPC server "
        "responds to only one URI path.  "
        "I don't know what URI path that is, "
        "but it's not the one you requested: '%s'.  (Typically, it's "
        "'/RPC2')", requestInfoP->uri);

    sendError(sessionP, 404, explanation);

    xmlrpc_strfree(explanation);

    return TRUE;
}



static void
setHandler(xmlrpc_env *              const envP,
           TServer *                 const srvP,
           struct uriHandlerXmlrpc * const uriHandlerXmlrpcP,
           size_t                    const xmlProcessorMaxStackSize) {
    
    abyss_bool success;

    trace_abyss = getenv("XMLRPC_TRACE_ABYSS");
                                 
    {
        size_t const stackSize = 
            HANDLE_XMLRPC_REQ_STACK + xmlProcessorMaxStackSize;
        struct ServerReqHandler3 const handlerDesc = {
            /* .term               = */ &termUriHandler,
            /* .handleReq          = */ &handleIfXmlrpcReq,
            /* .userdata           = */ uriHandlerXmlrpcP,
            /* .handleReqStackSize = */ stackSize
        };
        ServerAddHandler3(srvP, &handlerDesc, &success);
    }
    if (!success)
        xmlrpc_faultf(envP, "Abyss failed to register the Xmlrpc-c request "
                      "handler.  ServerAddHandler3() failed.");

    if (envP->fault_occurred){
		free((void *)uriHandlerXmlrpcP->uriPath);
        free(uriHandlerXmlrpcP);
	}
}



static void
interpretHttpAccessControl(
    const xmlrpc_server_abyss_handler_parms * const parmsP,
    unsigned int                              const parmSize,
    ResponseAccessCtl *                       const accessCtlP) {

    if (parmSize >= XMLRPC_AHPSIZE(allow_origin) && parmsP->allow_origin)
        accessCtlP->allowOrigin = xmlrpc_strdupsol(parmsP->allow_origin);
    else
        accessCtlP->allowOrigin = NULL;

    if (parmSize >= XMLRPC_AHPSIZE(access_ctl_expires)
        && parmsP->access_ctl_expires) {
        accessCtlP->expires = true;

        if (parmSize >= XMLRPC_AHPSIZE(access_ctl_max_age))
            accessCtlP->maxAge = parmsP->access_ctl_max_age;
        else
            accessCtlP->maxAge = 0;
    }            
}



void
xmlrpc_server_abyss_set_handler3(
    xmlrpc_env *                              const envP,
    TServer *                                 const srvP,
    const xmlrpc_server_abyss_handler_parms * const parmsP,
    unsigned int                              const parmSize) {

    struct uriHandlerXmlrpc * uriHandlerXmlrpcP;
    size_t xmlProcessorMaxStackSize;

    MALLOCVAR_NOFAIL(uriHandlerXmlrpcP);

    if (!envP->fault_occurred) {
        if (parmSize >= XMLRPC_AHPSIZE(xml_processor))
            uriHandlerXmlrpcP->xmlProcessor = parmsP->xml_processor;
        else
            xmlrpc_faultf(envP, "Parameter too short to contain the required "
                          "'xml_processor' member");
    }
    if (!envP->fault_occurred) {
        if (parmSize >= XMLRPC_AHPSIZE(xml_processor_arg))
            uriHandlerXmlrpcP->xmlProcessorArg = parmsP->xml_processor_arg;
        else
            xmlrpc_faultf(envP, "Parameter too short to contain the required "
                          "'xml_processor_arg' member");
    }
    if (!envP->fault_occurred) {
        if (parmSize >= XMLRPC_AHPSIZE(xml_processor_max_stack))
            xmlProcessorMaxStackSize = parmsP->xml_processor_max_stack;
        else
            xmlrpc_faultf(envP, "Parameter too short to contain the required "
                          "'xml_processor_max_stack' member");
    }
    if (!envP->fault_occurred) {
        if (parmSize >= XMLRPC_AHPSIZE(uri_path) && parmsP->uri_path)
            uriHandlerXmlrpcP->uriPath = xmlrpc_strdupsol(parmsP->uri_path);
        else
            uriHandlerXmlrpcP->uriPath = xmlrpc_strdupsol("/RPC2");

        if (parmSize >= XMLRPC_AHPSIZE(chunk_response) &&
            parmsP->chunk_response)
            uriHandlerXmlrpcP->chunkResponse = parmsP->chunk_response;
        else
            uriHandlerXmlrpcP->chunkResponse = false;
        
        interpretHttpAccessControl(parmsP, parmSize,
                                   &uriHandlerXmlrpcP->accessControl);

        if (envP->fault_occurred)
            termAccessControl(&uriHandlerXmlrpcP->accessControl);
    }
    if (!envP->fault_occurred)
        setHandler(envP, srvP, uriHandlerXmlrpcP, xmlProcessorMaxStackSize);

    if (envP->fault_occurred)
        free(uriHandlerXmlrpcP);
}



void
xmlrpc_server_abyss_set_handler2(
    TServer *         const srvP,
    const char *      const uriPath,
    xmlrpc_call_processor   xmlProcessor,
    void *            const xmlProcessorArg,
    size_t            const xmlProcessorMaxStackSize,
    xmlrpc_bool       const chunkResponse) {

    xmlrpc_env env;
    xmlrpc_server_abyss_handler_parms parms;

    xmlrpc_env_init(&env);

    parms.xml_processor = xmlProcessor;
    parms.xml_processor_arg = xmlProcessorArg;
    parms.xml_processor_max_stack = xmlProcessorMaxStackSize;
    parms.uri_path = uriPath;
    parms.chunk_response = chunkResponse;

    xmlrpc_server_abyss_set_handler3(&env, srvP,
                                     &parms, XMLRPC_AHPSIZE(chunk_response));
    
    if (env.fault_occurred)
        abort();

    xmlrpc_env_clean(&env);
}



void
xmlrpc_server_abyss_set_handler(xmlrpc_env *      const envP,
                                TServer *         const srvP,
                                const char *      const uriPath,
                                xmlrpc_registry * const registryP) {

    xmlrpc_server_abyss_handler_parms parms;

    parms.xml_processor = &processXmlrpcCall;
    parms.xml_processor_arg = registryP;
    parms.xml_processor_max_stack = xmlrpc_registry_max_stackSize(registryP);
    parms.uri_path = uriPath;

    xmlrpc_server_abyss_set_handler3(envP, srvP,
                                     &parms, XMLRPC_AHPSIZE(uri_path));
}

    

void
xmlrpc_server_abyss_set_default_handler(TServer * const srvP) {

    ServerDefaultHandler(srvP, xmlrpc_server_abyss_default_handler);
}
    


static void
setHandlersRegistry(TServer *         const srvP,
                    const char *      const uriPath,
                    xmlrpc_registry * const registryP,
                    bool              const chunkResponse,
                    const char *      const allowOrigin,
                    bool              const expires,
                    unsigned int      const maxAge) {

    xmlrpc_env env;
    xmlrpc_server_abyss_handler_parms parms;

    xmlrpc_env_init(&env);

    parms.xml_processor = &processXmlrpcCall;
    parms.xml_processor_arg = registryP;
    parms.xml_processor_max_stack = xmlrpc_registry_max_stackSize(registryP),
    parms.uri_path = uriPath;
    parms.chunk_response = chunkResponse;
    parms.allow_origin = allowOrigin;
    parms.access_ctl_expires = expires;
    parms.access_ctl_max_age = maxAge;

    xmlrpc_server_abyss_set_handler3(
        &env, srvP, &parms, XMLRPC_AHPSIZE(access_ctl_max_age));
    
    if (env.fault_occurred)
        abort();

    xmlrpc_env_clean(&env);

    xmlrpc_server_abyss_set_default_handler(srvP);
}



void
xmlrpc_server_abyss_set_handlers2(TServer *         const srvP,
                                  const char *      const uriPath,
                                  xmlrpc_registry * const registryP) {

    setHandlersRegistry(srvP, uriPath, registryP, false, NULL, false, 0);
}



void
xmlrpc_server_abyss_set_handlers(TServer *         const srvP,
                                 xmlrpc_registry * const registryP) {

    setHandlersRegistry(srvP, "/RPC2", registryP, false, NULL, false, 0);
}



/*============================================================================
  createServer()
============================================================================*/

static void
setAdditionalServerParms(const xmlrpc_server_abyss_parms * const parmsP,
                         unsigned int                      const parmSize,
                         TServer *                         const serverP) {

    if (parmSize >= XMLRPC_APSIZE(keepalive_timeout) &&
        parmsP->keepalive_timeout > 0)
        ServerSetKeepaliveTimeout(serverP, parmsP->keepalive_timeout);
    if (parmSize >= XMLRPC_APSIZE(keepalive_max_conn) &&
        parmsP->keepalive_max_conn > 0)
        ServerSetKeepaliveMaxConn(serverP, parmsP->keepalive_max_conn);
    if (parmSize >= XMLRPC_APSIZE(timeout) &&
        parmsP->timeout > 0)
        ServerSetTimeout(serverP, parmsP->timeout);
    if (parmSize >= XMLRPC_APSIZE(dont_advertise))
        ServerSetAdvertise(serverP, !parmsP->dont_advertise);
}



static void
extractServerCreateParms(
    xmlrpc_env *                      const envP,
    const xmlrpc_server_abyss_parms * const parmsP,
    unsigned int                      const parmSize,
    bool *                            const socketBoundP,
    unsigned int *                    const portNumberP,
    TOsSocket *                       const socketFdP,
    const char **                     const logFileNameP) {
                   

    if (parmSize >= XMLRPC_APSIZE(socket_bound))
        *socketBoundP = parmsP->socket_bound;
    else
        *socketBoundP = FALSE;

    if (*socketBoundP) {
        if (parmSize < XMLRPC_APSIZE(socket_handle))
            xmlrpc_faultf(envP, "socket_bound is true, but server parameter "
                          "structure does not contain socket_handle (it's too "
                          "short)");
        else
            *socketFdP = parmsP->socket_handle;
    } else {
        if (parmSize >= XMLRPC_APSIZE(port_number))
            *portNumberP = parmsP->port_number;
        else
            *portNumberP = 8080;

        if (*portNumberP > 0xffff)
            xmlrpc_faultf(envP,
                          "TCP port number %u exceeds the maximum possible "
                          "TCP port number (65535)",
                          *portNumberP);
    }
    if (!envP->fault_occurred) {
        if (parmSize >= XMLRPC_APSIZE(log_file_name) &&
            parmsP->log_file_name)
            *logFileNameP = strdup(parmsP->log_file_name);
        else
            *logFileNameP = NULL;
    }
}



static void
chanSwitchCreateOsSocket(TOsSocket      const socketFd,
                         TChanSwitch ** const chanSwitchPP,
                         const char **  const errorP) {

#ifdef WIN32
    ChanSwitchWinCreateWinsock(socketFd, chanSwitchPP, errorP);
#else
    ChanSwitchUnixCreateFd(socketFd, chanSwitchPP, errorP);
#endif

}



static void
createServerBoundSocket(xmlrpc_env *   const envP,
                        TOsSocket      const socketFd,
                        const char *   const logFileName,
                        TServer *      const serverP,
                        TChanSwitch ** const chanSwitchPP) {

    TChanSwitch * chanSwitchP;
    const char * error;
    
    chanSwitchCreateOsSocket(socketFd, &chanSwitchP, &error);
    if (error) {
        xmlrpc_faultf(envP, "Unable to create Abyss socket out of "
                      "file descriptor %d.  %s", socketFd, error);
        xmlrpc_strfree(error);
    } else {
        ServerCreateSwitch(serverP, chanSwitchP, &error);
        if (error) {
            xmlrpc_faultf(envP, "Abyss failed to create server.  %s", error);
            xmlrpc_strfree(error);
        } else {
            *chanSwitchPP = chanSwitchP;
                    
            ServerSetName(serverP, "XmlRpcServer");
            
            if (logFileName)
                ServerSetLogFileName(serverP, logFileName);
        }
        if (envP->fault_occurred)
            ChanSwitchDestroy(chanSwitchP);
    }
}



static void
createServerBare(xmlrpc_env *                      const envP,
                 const xmlrpc_server_abyss_parms * const parmsP,
                 unsigned int                      const parmSize,
                 TServer *                         const serverP,
                 TChanSwitch **                    const chanSwitchPP) {
/*----------------------------------------------------------------------------
   Create a bare server.  It will need further setup before it is ready
   to use.
-----------------------------------------------------------------------------*/
    bool socketBound;
    unsigned int portNumber = 0;
    TOsSocket socketFd = 0;
    const char * logFileName;

    extractServerCreateParms(envP, parmsP, parmSize,
                             &socketBound, &portNumber, &socketFd,
                             &logFileName);

    if (!envP->fault_occurred) {
        if (socketBound)
            createServerBoundSocket(envP, socketFd, logFileName,
                                    serverP, chanSwitchPP);
        else {
            abyss_bool success;

            success = ServerCreate(serverP, "XmlRpcServer", portNumber,
                                   DEFAULT_DOCS, logFileName);

            if (!success)
                xmlrpc_faultf(envP, "Failed to create an Abyss server object");
            
            *chanSwitchPP = NULL;
        }
        if (logFileName)
            xmlrpc_strfree(logFileName);
    }
}



static const char *
uriPathParm(const xmlrpc_server_abyss_parms * const parmsP,
            unsigned int                      const parmSize) {
    
    const char * uriPath;

    if (parmSize >= XMLRPC_APSIZE(uri_path) && parmsP->uri_path)
        uriPath = parmsP->uri_path;
    else
        uriPath = "/RPC2";

    return uriPath;
}



static bool
chunkResponseParm(const xmlrpc_server_abyss_parms * const parmsP,
                  unsigned int                      const parmSize) {

    return
        parmSize >= XMLRPC_APSIZE(chunk_response) &&
        parmsP->chunk_response;
}    



static const char *
allowOriginParm(const xmlrpc_server_abyss_parms * const parmsP,
                unsigned int                      const parmSize) {

    return
        parmSize >= XMLRPC_APSIZE(allow_origin) ?
        parmsP->allow_origin : NULL;
}    



static bool
expiresParm(const xmlrpc_server_abyss_parms * const parmsP,
            unsigned int                      const parmSize) {

    return
        parmSize >= XMLRPC_APSIZE(access_ctl_expires) ?
        parmsP->access_ctl_expires : false;
}    



static unsigned int
maxAgeParm(const xmlrpc_server_abyss_parms * const parmsP,
           unsigned int                      const parmSize) {

    return
        parmSize >= XMLRPC_APSIZE(access_ctl_max_age) ?
        parmsP->access_ctl_max_age : 0;
}    



static void
createServer(xmlrpc_env *                      const envP,
             const xmlrpc_server_abyss_parms * const parmsP,
             unsigned int                      const parmSize,
             TServer *                         const abyssServerP,
             TChanSwitch **                    const chanSwitchPP) {

    createServerBare(envP, parmsP, parmSize, abyssServerP, chanSwitchPP);

    if (!envP->fault_occurred) {
        setAdditionalServerParms(parmsP, parmSize, abyssServerP);
        
        setHandlersRegistry(abyssServerP, uriPathParm(parmsP, parmSize),
                            parmsP->registryP,
                            chunkResponseParm(parmsP, parmSize),
                            allowOriginParm(parmsP, parmSize),
                            expiresParm(parmsP, parmSize),
                            maxAgeParm(parmsP, parmSize));
        
        ServerInit(abyssServerP);
    }
}



static bool
enableShutdownParm(const xmlrpc_server_abyss_parms * const parmsP,
                   unsigned int                      const parmSize) {

    return
        parmSize >= XMLRPC_APSIZE(enable_shutdown) &&
        parmsP->enable_shutdown;
}



static xmlrpc_server_shutdown_fn shutdownAbyss;

static void
shutdownAbyss(xmlrpc_env * const faultP,
              void *       const context,
              const char * const comment ATTR_UNUSED,
              void *       const callInfo ATTR_UNUSED) {
/*----------------------------------------------------------------------------
   Tell Abyss to wrap up whatever it's doing and shut down.

   This is a server shutdown function to be registered in the method
   registry, for use by the 'system.shutdown' system method.

   After we return, Abyss will finish up the system.shutdown and any
   other connections that are in progress, then the call to
   ServerRun() etc. will return.

   *faultP is the result of the shutdown request, not whether we
   succeeded or failed.  We are not allowed to fail.
-----------------------------------------------------------------------------*/
    xmlrpc_server_abyss_t * const serverP = context;

    xmlrpc_env_init(faultP);
    
    if (!serverP->shutdownEnabled)
        xmlrpc_env_set_fault_formatted(
            faultP, XMLRPC_REQUEST_REFUSED_ERROR,
            "Shutdown by client is disabled on this server.");
    else
        ServerTerminate(&serverP->abyssServer);
}



/*=============================================================================
  xmlrpc_server_abyss object methods
=============================================================================*/

void
xmlrpc_server_abyss_create(xmlrpc_env *                      const envP,
                           const xmlrpc_server_abyss_parms * const parmsP,
                           unsigned int                      const parmSize,
                           xmlrpc_server_abyss_t **          const serverPP) {

    xmlrpc_server_abyss_t * serverP;

    XMLRPC_ASSERT_ENV_OK(envP);

    validateGlobalInit(envP);

    if (!envP->fault_occurred) {
        if (parmSize < XMLRPC_APSIZE(registryP))
            xmlrpc_faultf(envP,
                          "You must specify members at least up through "
                          "'registryP' in the server parameters argument.  "
                          "That would mean the parameter size would be >= %u "
                          "but you specified a size of %u",
                          (unsigned)XMLRPC_APSIZE(registryP), parmSize);
        else {
            MALLOCVAR(serverP);

            if (serverP == NULL)
                xmlrpc_faultf(envP, "Unable to allocate memory for "
                              "server descriptor.");
            else {
                createServer(envP, parmsP, parmSize,
                             &serverP->abyssServer, &serverP->chanSwitchP);
            
                if (!envP->fault_occurred) {
                    serverP->shutdownEnabled =
                        enableShutdownParm(parmsP, parmSize);

                    xmlrpc_registry_set_shutdown(
                        parmsP->registryP, &shutdownAbyss, serverP);
                
                    if (envP->fault_occurred)
                        free(serverP);
                    else
                        *serverPP = serverP;
                }
            }
        }
    }
}



void
xmlrpc_server_abyss_destroy(xmlrpc_server_abyss_t * const serverP) {

    XMLRPC_ASSERT(globallyInitialized);

    ServerFree(&serverP->abyssServer);

    if (serverP->chanSwitchP)
        ChanSwitchDestroy(serverP->chanSwitchP);

    free(serverP);
}



void
xmlrpc_server_abyss_use_sigchld(xmlrpc_server_abyss_t * const serverP) {

    ServerUseSigchld(&serverP->abyssServer);
}



void
xmlrpc_server_abyss_run_server(xmlrpc_env *            const envP ATTR_UNUSED,
                               xmlrpc_server_abyss_t * const serverP) {

    ServerRun(&serverP->abyssServer);
}



void
xmlrpc_server_abyss_terminate(
    xmlrpc_env *            const envP ATTR_UNUSED,
    xmlrpc_server_abyss_t * const serverP) {

    ServerTerminate(&serverP->abyssServer);
}



void
xmlrpc_server_abyss_reset_terminate(
    xmlrpc_env *            const envP ATTR_UNUSED,
    xmlrpc_server_abyss_t * const serverP) {

    ServerResetTerminate(&serverP->abyssServer);
}



static void 
sigchld(int const signalClass ATTR_UNUSED) {
/*----------------------------------------------------------------------------
   This is a signal handler for a SIGCHLD signal (which informs us that
   one of our child processes has terminated).

   The only child processes we have are those that belong to the Abyss
   server (and then only if the Abyss server was configured to use
   forking as a threading mechanism), so we respond by passing the
   signal on to the Abyss server.  And reaping the dead child.
-----------------------------------------------------------------------------*/
#ifndef WIN32
    /* Reap zombie children / report to Abyss until there aren't any more. */

    bool childrenLeft;
    bool error;

    assert(signalClass == SIGCHLD);

    error = false;
    childrenLeft = true;  /* initial assumption */
    
    /* Reap defunct children until there aren't any more. */
    while (childrenLeft && !error) {
        int status;
        pid_t pid;

        pid = waitpid((pid_t) -1, &status, WNOHANG);
    
        if (pid == 0)
            childrenLeft = false;
        else if (pid < 0) {
            /* because of ptrace */
            if (errno != EINTR)   
                error = true;
        } else
            ServerHandleSigchld(pid);
    }
#endif /* WIN32 */
}


struct xmlrpc_server_abyss_sig {

    /* A description of the state of the process' signal handlers before
       functions in this library messed with them; useful for restoring
       them later.
    */
#ifndef WIN32
    struct sigaction pipe;
    struct sigaction chld;
#else
    int dummy;
#endif
};



static void
setupSignalHandlers(xmlrpc_server_abyss_sig * const oldHandlersP) {
#ifndef WIN32
    struct sigaction mysigaction;
    
    sigemptyset(&mysigaction.sa_mask);
    mysigaction.sa_flags = 0;

    /* This signal indicates connection closed in the middle */
    mysigaction.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &mysigaction, &oldHandlersP->pipe);
    
    /* This signal indicates a child process (request handler) has died */
    mysigaction.sa_handler = sigchld;
    sigaction(SIGCHLD, &mysigaction, &oldHandlersP->chld);
#endif
}    



static void
restoreSignalHandlers(const xmlrpc_server_abyss_sig * const oldHandlersP) {
#ifndef WIN32

    sigaction(SIGPIPE, &oldHandlersP->pipe, NULL);
    sigaction(SIGCHLD, &oldHandlersP->chld, NULL);

#endif
}



void
xmlrpc_server_abyss_setup_sig(
    xmlrpc_env *               const envP,
    xmlrpc_server_abyss_t *    const serverP,
    xmlrpc_server_abyss_sig ** const oldHandlersPP) {

    xmlrpc_server_abyss_sig * oldHandlersP;

    XMLRPC_ASSERT_ENV_OK(envP);

    validateGlobalInit(envP);

    if (!envP->fault_occurred) {
        MALLOCVAR(oldHandlersP);

        if (oldHandlersP == NULL)
            xmlrpc_faultf(envP, "Unable to allocate memory to save signal "
                          "handling state.");
        else {
            setupSignalHandlers(oldHandlersP);

            xmlrpc_server_abyss_use_sigchld(serverP);
        }
        if (oldHandlersPP)
            *oldHandlersPP = oldHandlersP;
        else
            free(oldHandlersP);
    }
}



void
xmlrpc_server_abyss_restore_sig(
    const xmlrpc_server_abyss_sig * const oldHandlersP) {

    restoreSignalHandlers(oldHandlersP);
}
                                 


static void
runServerDaemon(TServer *  const serverP,
                runfirstFn const runfirst,
                void *     const runfirstArg) {

    xmlrpc_server_abyss_sig oldHandlers;

    setupSignalHandlers(&oldHandlers);

    ServerUseSigchld(serverP);

    ServerDaemonize(serverP);
    
    /* We run the user supplied runfirst after forking, but before accepting
       connections (helpful when running with threads)
    */
    if (runfirst)
        runfirst(runfirstArg);

    ServerRun(serverP);

    restoreSignalHandlers(&oldHandlers);
}



static void
oldHighLevelAbyssRun(xmlrpc_env *                      const envP,
                     const xmlrpc_server_abyss_parms * const parmsP,
                     unsigned int                      const parmSize) {
/*----------------------------------------------------------------------------
   This is the old deprecated interface, where the caller of the 
   xmlrpc_server_abyss API supplies an Abyss configuration file and
   we use it to daemonize (fork into the background, chdir, set uid, etc.)
   and run the Abyss server.

   The new preferred interface, implemented by normalLevelAbyssRun(),
   instead lets Caller set up the process environment himself and pass
   Abyss parameters in memory.  That's a more conventional and
   flexible API.
-----------------------------------------------------------------------------*/
    TServer server;
    abyss_bool success;

    success = ServerCreate(&server, "XmlRpcServer", 8080, DEFAULT_DOCS, NULL);

    if (!success)
        xmlrpc_faultf(envP, "Failed to create Abyss server object");
    else {
        runfirstFn runfirst;
        void * runfirstArg;

        assert(parmSize >= XMLRPC_APSIZE(config_file_name));
    
        ConfReadServerFile(parmsP->config_file_name, &server);
        
        assert(parmSize >= XMLRPC_APSIZE(registryP));
    
        setHandlersRegistry(&server, "/RPC2", parmsP->registryP, false, NULL,
                            false, 0);
        
        ServerInit(&server);
    
        if (parmSize >= XMLRPC_APSIZE(runfirst_arg)) {
            runfirst    = parmsP->runfirst;
            runfirstArg = parmsP->runfirst_arg;
        } else {
            runfirst    = NULL;
            runfirstArg = NULL;
        }
        runServerDaemon(&server, runfirst, runfirstArg);

        ServerFree(&server);
    }
}



static void
normalLevelAbyssRun(xmlrpc_env *                      const envP,
                    const xmlrpc_server_abyss_parms * const parmsP,
                    unsigned int                      const parmSize) {
    
    xmlrpc_server_abyss_t * serverP;

    xmlrpc_server_abyss_create(envP, parmsP, parmSize, &serverP);

    if (!envP->fault_occurred) {
        xmlrpc_server_abyss_sig * oldHandlersP;

        xmlrpc_server_abyss_setup_sig(envP, serverP, &oldHandlersP);

        if (!envP->fault_occurred) {
            xmlrpc_server_abyss_run_server(envP, serverP);

            xmlrpc_server_abyss_restore_sig(oldHandlersP);

            free(oldHandlersP);
        }
        xmlrpc_server_abyss_destroy(serverP);
    }
}



void
xmlrpc_server_abyss(xmlrpc_env *                      const envP,
                    const xmlrpc_server_abyss_parms * const parmsP,
                    unsigned int                      const parmSize) {
/*----------------------------------------------------------------------------
   Note that this is not re-entrant and not thread-safe, due to the
   global library initialization.  If you want to run a server inside
   a thread of a multi-threaded program, use
   xmlrpc_server_abyss_create() instead.  As required by that
   subroutine, your program will contain a call to
   xmlrpc_server_abyss_global_init() early in your program, when it is only
   one thread.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT_ENV_OK(envP);

    xmlrpc_server_abyss_global_init(envP);

    if (!envP->fault_occurred) {
        if (parmSize < XMLRPC_APSIZE(registryP))
            xmlrpc_faultf(envP,
                          "You must specify members at least up through "
                          "'registryP' in the server parameters argument.  "
                          "That would mean the parameter size would be >= %u "
                          "but you specified a size of %u",
                          (unsigned)XMLRPC_APSIZE(registryP), parmSize);
        else {
            if (parmsP->config_file_name)
                oldHighLevelAbyssRun(envP, parmsP, parmSize);
            else
                normalLevelAbyssRun(envP, parmsP, parmSize);
            
        }
        xmlrpc_server_abyss_global_term();
    }
}



/*=========================================================================
  XML-RPC Server Method Registry

  This is an old deprecated form of the server facilities that uses
  global variables.
=========================================================================*/

/* These global variables must be treated as read-only after the
   server has started.
*/

static TServer globalSrv;
    /* When you use the old interface (xmlrpc_server_abyss_init(), etc.),
       this is the Abyss server to which they refer.  Obviously, there can be
       only one Abyss server per program using this interface.
    */

static xmlrpc_registry * builtin_registryP;



void 
xmlrpc_server_abyss_init_registry(void) {

    /* This used to just create the registry and Caller would be
       responsible for adding the handlers that use it.

       But that isn't very modular -- the handlers and registry go
       together; there's no sense in using the built-in registry and
       not the built-in handlers because if you're custom building
       something, you can just make your own regular registry.  So now
       we tie them together, and we don't export our handlers.  
    */
    xmlrpc_env env;

    xmlrpc_env_init(&env);
    builtin_registryP = xmlrpc_registry_new(&env);
    dieIfFaultOccurred(&env);
    xmlrpc_env_clean(&env);

    setHandlersRegistry(&globalSrv, "/RPC2", builtin_registryP, false, NULL,
                        false, 0);
}



xmlrpc_registry *
xmlrpc_server_abyss_registry(void) {

    /* This is highly deprecated.  If you want to mess with a registry,
       make your own with xmlrpc_registry_new() -- don't mess with the
       internal one.
    */
    return builtin_registryP;
}



/* A quick & easy shorthand for adding a method. */
void 
xmlrpc_server_abyss_add_method(char *        const method_name,
                               xmlrpc_method const method,
                               void *        const user_data) {
    xmlrpc_env env;

    xmlrpc_env_init(&env);
    xmlrpc_registry_add_method(&env, builtin_registryP, NULL, method_name,
                               method, user_data);
    dieIfFaultOccurred(&env);
    xmlrpc_env_clean(&env);
}



void
xmlrpc_server_abyss_add_method_w_doc(char *        const method_name,
                                     xmlrpc_method const method,
                                     void *        const user_data,
                                     char *        const signature,
                                     char *        const help) {

    xmlrpc_env env;
    xmlrpc_env_init(&env);
    xmlrpc_registry_add_method_w_doc(
        &env, builtin_registryP, NULL, method_name,
        method, user_data, signature, help);
    dieIfFaultOccurred(&env);
    xmlrpc_env_clean(&env);    
}



void 
xmlrpc_server_abyss_init(int          const flags ATTR_UNUSED, 
                         const char * const config_file) {

    abyss_bool success;

    success = ServerCreate(&globalSrv, "XmlRpcServer", 8080,
                           DEFAULT_DOCS, NULL);

    if (!success)
        abort();
    else {
        ConfReadServerFile(config_file, &globalSrv);

        xmlrpc_server_abyss_init_registry();
            /* Installs /RPC2 handler and default handler that use the
               built-in registry.
            */

        ServerInit(&globalSrv);
    }
}



void 
xmlrpc_server_abyss_run_first(runfirstFn const runfirst,
                              void *     const runfirstArg) {
    
    runServerDaemon(&globalSrv, runfirst, runfirstArg);
}



void 
xmlrpc_server_abyss_run(void) {
    runServerDaemon(&globalSrv, NULL, NULL);
}



/*
** Copyright (C) 2001 by First Peer, Inc. All rights reserved.
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
** SUCH DAMAGE.
**
** There is more copyright information in the bottom half of this file. 
** Please see it for more details. 
*/
