/* Copyright information is at end of file */

#include "xmlrpc_config.h"

#undef PACKAGE
#undef VERSION

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include "bool.h"
#include "mallocvar.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/client.h"
#include "xmlrpc-c/client_int.h"
/* transport_config.h defines XMLRPC_DEFAULT_TRANSPORT,
    MUST_BUILD_WININET_CLIENT, MUST_BUILD_CURL_CLIENT,
    MUST_BUILD_LIBWWW_CLIENT 
*/
#include "transport_config.h"
#include "version.h"

struct xmlrpc_client {
/*----------------------------------------------------------------------------
   This represents a client object.
-----------------------------------------------------------------------------*/
    bool myTransport;
        /* The transport described below was created by this object;
           No one else knows it exists and this object is responsible
           for destroying it.
        */
    struct xmlrpc_client_transport *   transportP;
    struct xmlrpc_client_transport_ops transportOps;
    xmlrpc_dialect                     dialect;
};



struct xmlrpc_call_info {
    /* This is all the information needed to finish executing a started
       RPC.

       You don't need this for an RPC the user executes synchronously,
       because then you can just use the storage in which the user passed
       his arguments.  But for asynchronous, the user will take back his
       storage, and we need to keep this info in our own.
    */

    struct {
        /* This are arguments to pass to the completion function.  It
           doesn't make sense to use them for anything else.  In fact, it
           really doesn't make sense for them to be arguments to the
           completion function, but they are historically.  */
        const char *   serverUrl;
        const char *   methodName;
        xmlrpc_value * paramArrayP;
        void *         userData;
    } completionArgs;
    xmlrpc_response_handler completionFn;

    
    /* The serialized XML data passed to this call. We keep this around
    ** for use by our source_anchor field. */
    xmlrpc_mem_block *serialized_xml;
};



/*=========================================================================
   Global Constant Setup/Teardown
=========================================================================*/

static void
callTransportSetup(xmlrpc_env *           const envP,
                   xmlrpc_transport_setup       setupFn) {

    if (setupFn)
        setupFn(envP);
}



static void
setupTransportGlobalConst(xmlrpc_env * const envP) {

#if MUST_BUILD_WININET_CLIENT
    if (!envP->fault_occurred)
        callTransportSetup(envP,
                           xmlrpc_wininet_transport_ops.setup_global_const);
#endif
#if MUST_BUILD_CURL_CLIENT
    if (!envP->fault_occurred)
        callTransportSetup(envP,
                           xmlrpc_curl_transport_ops.setup_global_const);
#endif
#if MUST_BUILD_LIBWWW_CLIENT
    if (!envP->fault_occurred)
        callTransportSetup(envP,
                           xmlrpc_libwww_transport_ops.setup_global_const);
#endif
}



static void
callTransportTeardown(xmlrpc_transport_teardown teardownFn) {

    if (teardownFn)
        teardownFn();
}



static void
teardownTransportGlobalConst(void) {

#if MUST_BUILD_WININET_CLIENT
    callTransportTeardown(
        xmlrpc_wininet_transport_ops.teardown_global_const);
#endif
#if MUST_BUILD_CURL_CLIENT
    callTransportTeardown(
        xmlrpc_curl_transport_ops.teardown_global_const);
#endif
#if MUST_BUILD_LIBWWW_CLIENT
    callTransportTeardown(
        xmlrpc_libwww_transport_ops.teardown_global_const);
#endif
}



/*=========================================================================
   Global stuff (except the global client)
=========================================================================*/

static unsigned int constSetupCount = 0;


void
xmlrpc_client_setup_global_const(xmlrpc_env * const envP) {
/*----------------------------------------------------------------------------
   Set up pseudo-constant global variables (they'd be constant, except that
   the library loader doesn't set them.  An explicit call from the loaded
   program does).

   This function is not thread-safe.  The user is supposed to call it
   (perhaps cascaded down from a multitude of higher level libraries)
   as part of early program setup, when the program is only one thread.
-----------------------------------------------------------------------------*/
    if (constSetupCount == 0)
        setupTransportGlobalConst(envP);

    ++constSetupCount;
}



void
xmlrpc_client_teardown_global_const(void) {
/*----------------------------------------------------------------------------
   Complement to xmlrpc_client_setup_global_const().

   This function is not thread-safe.  The user is supposed to call it
   (perhaps cascaded down from a multitude of higher level libraries)
   as part of final program cleanup, when the program is only one thread.
-----------------------------------------------------------------------------*/
    assert(constSetupCount > 0);

    --constSetupCount;

    if (constSetupCount == 0)
        teardownTransportGlobalConst();
}



unsigned int const xmlrpc_client_version_major = XMLRPC_VERSION_MAJOR;
unsigned int const xmlrpc_client_version_minor = XMLRPC_VERSION_MINOR;
unsigned int const xmlrpc_client_version_point = XMLRPC_VERSION_POINT;


/*=========================================================================
   Client Create/Destroy
=========================================================================*/

static void
getTransportOps(
    xmlrpc_env *                                const envP,
    const char *                                const transportName,
    const struct xmlrpc_client_transport_ops ** const opsPP) {

    if (false) {
    }
#if MUST_BUILD_WININET_CLIENT
    else if (strcmp(transportName, "wininet") == 0)
        *opsPP = &xmlrpc_wininet_transport_ops;
#endif
#if MUST_BUILD_CURL_CLIENT
    else if (strcmp(transportName, "curl") == 0)
        *opsPP = &xmlrpc_curl_transport_ops;
#endif
#if MUST_BUILD_LIBWWW_CLIENT
    else if (strcmp(transportName, "libwww") == 0)
        *opsPP = &xmlrpc_libwww_transport_ops;
#endif
    else
        xmlrpc_faultf(envP, "Unrecognized XML transport name '%s'",
                      transportName);
}



struct xportParms {
    const void * parmsP;
    size_t size;
};



static void
getTransportParmsFromClientParms(
    xmlrpc_env *                      const envP,
    const struct xmlrpc_clientparms * const clientparmsP,
    unsigned int                      const parmSize,
    struct xportParms *               const xportParmsP) {

    if (parmSize < XMLRPC_CPSIZE(transportparmsP) ||
        clientparmsP->transportparmsP == NULL) {

        xportParmsP->parmsP = NULL;
        xportParmsP->size   = 0;
    } else {
        xportParmsP->parmsP = clientparmsP->transportparmsP;
        if (parmSize < XMLRPC_CPSIZE(transportparm_size))
            xmlrpc_faultf(envP, "Your 'clientparms' argument contains the "
                          "transportparmsP member, "
                          "but no transportparms_size member");
        else
            xportParmsP->size = clientparmsP->transportparm_size;
    }
}



static void
getTransportInfo(
    xmlrpc_env *                                const envP,
    const struct xmlrpc_clientparms *           const clientparmsP,
    unsigned int                                const parmSize,
    const char **                               const transportNameP,
    struct xportParms *                         const transportParmsP,
    const struct xmlrpc_client_transport_ops ** const transportOpsPP,
    xmlrpc_client_transport **                  const transportPP) {

    const char * transportNameParm;
    xmlrpc_client_transport * transportP;
    const struct xmlrpc_client_transport_ops * transportOpsP;

    if (parmSize < XMLRPC_CPSIZE(transport))
        transportNameParm = NULL;
    else
        transportNameParm = clientparmsP->transport;
    
    if (parmSize < XMLRPC_CPSIZE(transportP))
        transportP = NULL;
    else
        transportP = clientparmsP->transportP;

    if (parmSize < XMLRPC_CPSIZE(transportOpsP))
        transportOpsP = NULL;
    else
        transportOpsP = clientparmsP->transportOpsP;

    if ((transportOpsP && !transportP) || (transportP && ! transportOpsP))
        xmlrpc_faultf(envP, "'transportOpsP' and 'transportP' go together. "
                      "You must specify both or neither");
    else if (transportNameParm && transportP)
        xmlrpc_faultf(envP, "You cannot specify both 'transport' and "
                      "'transportP' transport parameters.");
    else if (transportP)
        *transportNameP = NULL;
    else if (transportNameParm)
        *transportNameP = transportNameParm;
    else
        *transportNameP = xmlrpc_client_get_default_transport(envP);

    *transportOpsPP = transportOpsP;
    *transportPP    = transportP;

    if (!envP->fault_occurred) {
        getTransportParmsFromClientParms(
            envP, clientparmsP, parmSize, transportParmsP);
        
        if (!envP->fault_occurred) {
            if (transportParmsP->parmsP && !transportNameParm)
                xmlrpc_faultf(
                    envP,
                    "You specified transport parameters, but did not "
                    "specify a transport type.  Parameters are specific "
                    "to a particular type.");
        }
    }
}



static void
getDialectFromClientParms(
    const struct xmlrpc_clientparms * const clientparmsP,
    unsigned int                      const parmSize,
    xmlrpc_dialect *                  const dialectP) {
    
    if (parmSize < XMLRPC_CPSIZE(dialect))
        *dialectP = xmlrpc_dialect_i8;
    else
        *dialectP = clientparmsP->dialect;
}
            


static void 
clientCreate(
    xmlrpc_env *                               const envP,
    bool                                       const myTransport,
    const struct xmlrpc_client_transport_ops * const transportOpsP,
    struct xmlrpc_client_transport *           const transportP,
    xmlrpc_dialect                             const dialect,
    xmlrpc_client **                           const clientPP) {

    XMLRPC_ASSERT_PTR_OK(transportOpsP);
    XMLRPC_ASSERT_PTR_OK(transportP);
    XMLRPC_ASSERT_PTR_OK(clientPP);

    if (constSetupCount == 0) {
        xmlrpc_faultf(envP,
                      "You have not called "
                      "xmlrpc_client_setup_global_const().");
        /* Impl note:  We can't just call it now because it isn't
           thread-safe.
        */
    } else {
        xmlrpc_client * clientP;

        MALLOCVAR(clientP);

        if (clientP == NULL)
            xmlrpc_faultf(envP, "Unable to allocate memory for "
                          "client descriptor.");
        else {
            clientP->myTransport  = myTransport;
            clientP->transportOps = *transportOpsP;
            clientP->transportP   = transportP;
            clientP->dialect      = dialect;
            
            *clientPP = clientP;
        }
    }
}



static void
createTransportAndClient(
    xmlrpc_env *     const envP,
    const char *     const transportName,
    const void *     const transportparmsP,
    size_t           const transportparmSize,
    int              const flags,
    const char *     const appname,
    const char *     const appversion,
    xmlrpc_dialect   const dialect,
    xmlrpc_client ** const clientPP) {

    const struct xmlrpc_client_transport_ops * transportOpsP;

    getTransportOps(envP, transportName, &transportOpsP);
    if (!envP->fault_occurred) {
        xmlrpc_client_transport * transportP;
        
        /* The following call is not thread-safe */
        transportOpsP->create(
            envP, flags, appname, appversion,
            transportparmsP, transportparmSize,
            &transportP);
        if (!envP->fault_occurred) {
            bool const myTransportTrue = true;

            clientCreate(envP, myTransportTrue, transportOpsP, transportP,
                         dialect, clientPP);
            
            if (envP->fault_occurred)
                transportOpsP->destroy(transportP);
        }
    }
}



void 
xmlrpc_client_create(xmlrpc_env *                      const envP,
                     int                               const flags,
                     const char *                      const appname,
                     const char *                      const appversion,
                     const struct xmlrpc_clientparms * const clientparmsP,
                     unsigned int                      const parmSize,
                     xmlrpc_client **                  const clientPP) {
    
    XMLRPC_ASSERT_PTR_OK(clientPP);

    if (constSetupCount == 0) {
        xmlrpc_faultf(envP,
                      "You have not called "
                      "xmlrpc_client_setup_global_const().");
        /* Impl note:  We can't just call it now because it isn't
           thread-safe.
        */
    } else {
        const char * transportName;
        struct xportParms transportparms;
        const struct xmlrpc_client_transport_ops * transportOpsP;
        xmlrpc_client_transport * transportP;
        xmlrpc_dialect dialect;
        
        getTransportInfo(envP, clientparmsP, parmSize, &transportName, 
                         &transportparms, &transportOpsP, &transportP);
        
        getDialectFromClientParms(clientparmsP, parmSize, &dialect);
            
        if (!envP->fault_occurred) {
            if (transportName)
                createTransportAndClient(envP, transportName,
                                         transportparms.parmsP,
                                         transportparms.size,
                                         flags, appname, appversion, dialect,
                                         clientPP);
            else {
                bool myTransportFalse = false;
                clientCreate(envP, myTransportFalse,
                             transportOpsP, transportP, dialect, clientPP);
            }
        }
    }
}



void 
xmlrpc_client_destroy(xmlrpc_client * const clientP) {

    XMLRPC_ASSERT_PTR_OK(clientP);

    if (clientP->myTransport)
        clientP->transportOps.destroy(clientP->transportP);

    free(clientP);
}



/*=========================================================================
   Call/Response Utilities
=========================================================================*/

static void
makeCallXml(xmlrpc_env *               const envP,
            const char *               const methodName,
            xmlrpc_value *             const paramArrayP,
            xmlrpc_dialect             const dialect,
            xmlrpc_mem_block **        const callXmlPP) {

    XMLRPC_ASSERT_VALUE_OK(paramArrayP);
    XMLRPC_ASSERT_PTR_OK(callXmlPP);

    if (methodName == NULL)
        xmlrpc_faultf(envP, "method name argument is NULL pointer");
    else {
        xmlrpc_mem_block * callXmlP;

        callXmlP = XMLRPC_MEMBLOCK_NEW(char, envP, 0);
        if (!envP->fault_occurred) {
            xmlrpc_serialize_call2(envP, callXmlP, methodName, paramArrayP,
                                   dialect);

            *callXmlPP = callXmlP;

            if (envP->fault_occurred)
                XMLRPC_MEMBLOCK_FREE(char, callXmlP);
        }
    }    
}



/*=========================================================================
   Synchronous Call
=========================================================================*/

void
xmlrpc_client_transport_call2(
    xmlrpc_env *               const envP,
    xmlrpc_client *            const clientP,
    const xmlrpc_server_info * const serverP,
    xmlrpc_mem_block *         const callXmlP,
    xmlrpc_mem_block **        const respXmlPP) {

    XMLRPC_ASSERT_PTR_OK(clientP);
    XMLRPC_ASSERT_PTR_OK(serverP);
    XMLRPC_ASSERT_PTR_OK(callXmlP);
    XMLRPC_ASSERT_PTR_OK(respXmlPP);

    clientP->transportOps.call(
        envP, clientP->transportP, serverP, callXmlP,
        respXmlPP);
}



static void
parseResponse(xmlrpc_env *       const envP,
              xmlrpc_mem_block * const respXmlP,
              xmlrpc_value **    const resultPP,
              int *              const faultCodeP,
              const char **      const faultStringP) {

    xmlrpc_env respEnv;

    xmlrpc_env_init(&respEnv);

    xmlrpc_parse_response2(
        &respEnv,
        XMLRPC_MEMBLOCK_CONTENTS(char, respXmlP),
        XMLRPC_MEMBLOCK_SIZE(char, respXmlP),
        resultPP, faultCodeP, faultStringP);

    if (respEnv.fault_occurred)
        xmlrpc_env_set_fault_formatted(
            envP, respEnv.fault_code,
            "Unable to make sense of XML-RPC response from server.  "
            "%s.  Use XMLRPC_TRACE_XML to see for yourself",
            respEnv.fault_string);

    xmlrpc_env_clean(&respEnv);
}



void
xmlrpc_client_call2(xmlrpc_env *               const envP,
                    struct xmlrpc_client *     const clientP,
                    const xmlrpc_server_info * const serverInfoP,
                    const char *               const methodName,
                    xmlrpc_value *             const paramArrayP,
                    xmlrpc_value **            const resultPP) {

    xmlrpc_mem_block * callXmlP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(clientP);
    XMLRPC_ASSERT_PTR_OK(serverInfoP);
    XMLRPC_ASSERT_PTR_OK(paramArrayP);

    makeCallXml(envP, methodName, paramArrayP, clientP->dialect, &callXmlP);
    
    if (!envP->fault_occurred) {
        xmlrpc_mem_block * respXmlP;
        
        xmlrpc_traceXml("XML-RPC CALL", 
                        XMLRPC_MEMBLOCK_CONTENTS(char, callXmlP),
                        XMLRPC_MEMBLOCK_SIZE(char, callXmlP));
        
        clientP->transportOps.call(
            envP, clientP->transportP, serverInfoP, callXmlP, &respXmlP);
        if (!envP->fault_occurred) {
            int faultCode;
            const char * faultString;

            xmlrpc_traceXml("XML-RPC RESPONSE", 
                            XMLRPC_MEMBLOCK_CONTENTS(char, respXmlP),
                            XMLRPC_MEMBLOCK_SIZE(char, respXmlP));
            
            parseResponse(envP, respXmlP, resultPP, &faultCode, &faultString);
            
            if (!envP->fault_occurred) {
                if (faultString) {
                    xmlrpc_env_set_fault_formatted(
                        envP, faultCode,
                        "RPC failed at server.  %s", faultString);
                    xmlrpc_strfree(faultString);
                } else
                    XMLRPC_ASSERT_VALUE_OK(*resultPP);
            }
            XMLRPC_MEMBLOCK_FREE(char, respXmlP);
        }
        XMLRPC_MEMBLOCK_FREE(char, callXmlP);
    }
}



static void
clientCall2f_va(xmlrpc_env *               const envP,
                xmlrpc_client *            const clientP,
                const char *               const serverUrl,
                const char *               const methodName,
                const char *               const format,
                xmlrpc_value **            const resultPP,
                va_list                          args) {

    xmlrpc_value * argP;
    xmlrpc_env argenv;
    const char * suffix;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(serverUrl);
    XMLRPC_ASSERT_PTR_OK(methodName);
    XMLRPC_ASSERT_PTR_OK(format);
    XMLRPC_ASSERT_PTR_OK(resultPP);

    /* Build our argument value. */
    xmlrpc_env_init(&argenv);
    xmlrpc_build_value_va(&argenv, format, args, &argP, &suffix);
    if (argenv.fault_occurred)
        xmlrpc_env_set_fault_formatted(
            envP, argenv.fault_code, "Invalid RPC arguments.  "
            "The format argument must indicate a single array, and the "
            "following arguments must correspond to that format argument.  "
            "The failure is: %s",
            argenv.fault_string);
    else {
        XMLRPC_ASSERT_VALUE_OK(argP);
        
        if (*suffix != '\0')
            xmlrpc_faultf(envP, "Junk after the argument specifier: '%s'.  "
                          "There must be exactly one argument.",
                          suffix);
        else {
            xmlrpc_server_info * serverInfoP;

            serverInfoP = xmlrpc_server_info_new(envP, serverUrl);
            
            if (!envP->fault_occurred) {
                /* Perform the actual XML-RPC call. */
                xmlrpc_client_call2(envP, clientP,
                                    serverInfoP, methodName, argP, resultPP);
                if (!envP->fault_occurred)
                    XMLRPC_ASSERT_VALUE_OK(*resultPP);
                xmlrpc_server_info_free(serverInfoP);
            }
        }
        xmlrpc_DECREF(argP);
    }
    xmlrpc_env_clean(&argenv);
}



void
xmlrpc_client_call2f(xmlrpc_env *    const envP,
                     xmlrpc_client * const clientP,
                     const char *    const serverUrl,
                     const char *    const methodName,
                     xmlrpc_value ** const resultPP,
                     const char *    const format,
                     ...) {

    va_list args;

    va_start(args, format);
    clientCall2f_va(envP, clientP, serverUrl,
                    methodName, format, resultPP, args);
    va_end(args);
}



/*=========================================================================
   Asynchronous Call
=========================================================================*/

static void 
callInfoSetCompletion(xmlrpc_env *              const envP,
                      struct xmlrpc_call_info * const callInfoP,
                      const char *              const serverUrl,
                      const char *              const methodName,
                      xmlrpc_value *            const paramArrayP,
                      xmlrpc_response_handler         completionFn,
                      void *                    const userData) {

    callInfoP->completionFn = completionFn;
    callInfoP->completionArgs.userData = userData;
    callInfoP->completionArgs.serverUrl = strdup(serverUrl);
    if (callInfoP->completionArgs.serverUrl == NULL)
        xmlrpc_faultf(envP, "Couldn't get memory to store server URL");
    else {
        callInfoP->completionArgs.methodName = strdup(methodName);
        if (callInfoP->completionArgs.methodName == NULL)
            xmlrpc_faultf(envP, "Couldn't get memory to store method name");
        else {
            callInfoP->completionArgs.paramArrayP = paramArrayP;
            xmlrpc_INCREF(paramArrayP);
        }
        if (envP->fault_occurred)
            xmlrpc_strfree(callInfoP->completionArgs.serverUrl);
    }
}



static void
callInfoCreate(xmlrpc_env *               const envP,
               const char *               const methodName,
               xmlrpc_value *             const paramArrayP,
               xmlrpc_dialect             const dialect,
               const char *               const serverUrl,
               xmlrpc_response_handler          completionFn,
               void *                     const userData,
               struct xmlrpc_call_info ** const callInfoPP) {
/*----------------------------------------------------------------------------
   Create a call_info object.  A call_info object represents an XML-RPC
   call.
-----------------------------------------------------------------------------*/
    struct xmlrpc_call_info * callInfoP;

    XMLRPC_ASSERT_PTR_OK(serverUrl);
    XMLRPC_ASSERT_PTR_OK(methodName);
    XMLRPC_ASSERT_VALUE_OK(paramArrayP);
    XMLRPC_ASSERT_PTR_OK(callInfoPP);

    MALLOCVAR(callInfoP);
    if (callInfoP == NULL)
        xmlrpc_faultf(envP, "Couldn't allocate memory for xmlrpc_call_info");
    else {
        xmlrpc_mem_block * callXmlP;

        makeCallXml(envP, methodName, paramArrayP, dialect, &callXmlP);

        if (!envP->fault_occurred) {
            xmlrpc_traceXml("XML-RPC CALL", 
                            XMLRPC_MEMBLOCK_CONTENTS(char, callXmlP),
                            XMLRPC_MEMBLOCK_SIZE(char, callXmlP));
            
            callInfoP->serialized_xml = callXmlP;
            
            *callInfoPP = callInfoP;

            callInfoSetCompletion(envP, callInfoP, serverUrl, methodName,
                                  paramArrayP, completionFn, userData);

            if (envP->fault_occurred)
                free(callInfoP);
        }
    }
}



static void 
callInfoDestroy(struct xmlrpc_call_info * const callInfoP) {

    XMLRPC_ASSERT_PTR_OK(callInfoP);

    if (callInfoP->completionFn) {
        xmlrpc_DECREF(callInfoP->completionArgs.paramArrayP);
        xmlrpc_strfree(callInfoP->completionArgs.methodName);
        xmlrpc_strfree(callInfoP->completionArgs.serverUrl);
    }
    if (callInfoP->serialized_xml)
         xmlrpc_mem_block_free(callInfoP->serialized_xml);

    free(callInfoP);
}



void 
xmlrpc_client_event_loop_finish(xmlrpc_client * const clientP) {

    XMLRPC_ASSERT_PTR_OK(clientP);

    clientP->transportOps.finish_asynch(
        clientP->transportP, timeout_no, 0);
}



void 
xmlrpc_client_event_loop_finish_timeout(xmlrpc_client * const clientP,
                                        xmlrpc_timeout  const timeout) {

    XMLRPC_ASSERT_PTR_OK(clientP);

    clientP->transportOps.finish_asynch(
        clientP->transportP, timeout_yes, timeout);
}



static void
asynchComplete(struct xmlrpc_call_info * const callInfoP,
               xmlrpc_mem_block *        const responseXmlP,
               xmlrpc_env                const transportEnv) {
/*----------------------------------------------------------------------------
   Complete an asynchronous XML-RPC call request.

   This includes calling the user's RPC completion routine.

   'transportEnv' describes an error that the transport
   encountered in processing the call.  If the transport successfully
   sent the call to the server and processed the response but the
   server failed the call, 'transportEnv' indicates no error, and the
   response in *responseXmlP might very well indicate that the server
   failed the request.
-----------------------------------------------------------------------------*/
    xmlrpc_env env;
    xmlrpc_value * resultP;

    xmlrpc_env_init(&env);

    resultP = NULL;  /* Just to quiet compiler warning */

    if (transportEnv.fault_occurred)
        xmlrpc_env_set_fault_formatted(
            &env, transportEnv.fault_code,
            "Client transport failed to execute the RPC.  %s",
            transportEnv.fault_string);

    if (!env.fault_occurred) {
        int faultCode;
        const char * faultString;

        xmlrpc_parse_response2(&env,
                               XMLRPC_MEMBLOCK_CONTENTS(char, responseXmlP),
                               XMLRPC_MEMBLOCK_SIZE(char, responseXmlP),
                               &resultP, &faultCode, &faultString);

        if (!env.fault_occurred) {
            if (faultString) {
                xmlrpc_env_set_fault_formatted(
                    &env, faultCode,
                    "RPC failed at server.  %s", faultString);
                xmlrpc_strfree(faultString);
            }
        }
    }
    /* Call the user's completion function with the RPC result */
    (*callInfoP->completionFn)(callInfoP->completionArgs.serverUrl, 
                               callInfoP->completionArgs.methodName, 
                               callInfoP->completionArgs.paramArrayP,
                               callInfoP->completionArgs.userData,
                               &env, resultP);

    if (!env.fault_occurred)
        xmlrpc_DECREF(resultP);

    callInfoDestroy(callInfoP);

    xmlrpc_env_clean(&env);
}



void
xmlrpc_client_start_rpc(xmlrpc_env *             const envP,
                        struct xmlrpc_client *   const clientP,
                        xmlrpc_server_info *     const serverInfoP,
                        const char *             const methodName,
                        xmlrpc_value *           const argP,
                        xmlrpc_response_handler        completionFn,
                        void *                   const userData) {
    
    struct xmlrpc_call_info * callInfoP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(clientP);
    XMLRPC_ASSERT_PTR_OK(serverInfoP);
    XMLRPC_ASSERT_PTR_OK(methodName);
    XMLRPC_ASSERT_VALUE_OK(argP);

    callInfoCreate(envP, methodName, argP, clientP->dialect,
                   serverInfoP->serverUrl, completionFn, userData,
                   &callInfoP);

    if (!envP->fault_occurred)
        clientP->transportOps.send_request(
            envP, clientP->transportP, serverInfoP,
            callInfoP->serialized_xml,
            &asynchComplete, callInfoP);
    
    if (envP->fault_occurred)
        callInfoDestroy(callInfoP);
    else {
        /* asynchComplete() will destroy *callInfoP */
    }
}



void 
xmlrpc_client_start_rpcf(xmlrpc_env *    const envP,
                         xmlrpc_client * const clientP,
                         const char *    const serverUrl,
                         const char *    const methodName,
                         xmlrpc_response_handler responseHandler,
                         void *          const userData,
                         const char *    const format,
                         ...) {

    va_list args;
    xmlrpc_value * paramArrayP;
    const char * suffix;

    XMLRPC_ASSERT_PTR_OK(serverUrl);
    XMLRPC_ASSERT_PTR_OK(format);

    /* Build our argument array. */
    va_start(args, format);
    xmlrpc_build_value_va(envP, format, args, &paramArrayP, &suffix);
    va_end(args);
    if (!envP->fault_occurred) {
        if (*suffix != '\0')
            xmlrpc_faultf(envP, "Junk after the argument "
                          "specifier: '%s'.  "
                          "There must be exactly one arument.",
                          suffix);
        else {
            xmlrpc_server_info * serverInfoP;

            serverInfoP = xmlrpc_server_info_new(envP, serverUrl);
            if (!envP->fault_occurred) {
                xmlrpc_client_start_rpc(
                    envP, clientP,
                    serverInfoP, methodName, paramArrayP,
                    responseHandler, userData);
            }
            xmlrpc_server_info_free(serverInfoP);
        }
        xmlrpc_DECREF(paramArrayP);
    }
}



/*=========================================================================
   Miscellaneous
=========================================================================*/

const char * 
xmlrpc_client_get_default_transport(xmlrpc_env * const envP ATTR_UNUSED) {

    return XMLRPC_DEFAULT_TRANSPORT;
}



void
xmlrpc_client_set_interrupt(xmlrpc_client * const clientP,
                            int *           const interruptP) {

    if (clientP->transportOps.set_interrupt)
        clientP->transportOps.set_interrupt(clientP->transportP, interruptP);
}



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
** SUCH DAMAGE.
*/
