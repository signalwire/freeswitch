/*=========================================================================
  XML-RPC Server Method Registry
===========================================================================
  These are the functions that implement the XML-RPC method registry.

  A method registry is a list of XML-RPC methods for a server to
  implement, along with the details of how to implement each -- most
  notably a function pointer for a function that executes the method.

  To build an XML-RPC server, just add a communication facility.

  Copyright information is at end of file

=========================================================================*/

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "xmlrpc_config.h"
#include "bool.h"
#include "mallocvar.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/server.h"
#include "method.h"
#include "system_method.h"
#include "version.h"

#include "registry.h"


unsigned int const xmlrpc_server_version_major = XMLRPC_VERSION_MAJOR;
unsigned int const xmlrpc_server_version_minor = XMLRPC_VERSION_MINOR;
unsigned int const xmlrpc_server_version_point = XMLRPC_VERSION_POINT;



void
xmlrpc_server_version(unsigned int * const majorP,
                      unsigned int * const minorP,
                      unsigned int * const pointP) {

    *majorP = XMLRPC_VERSION_MAJOR;
    *minorP = XMLRPC_VERSION_MINOR;
    *pointP = XMLRPC_VERSION_POINT;
}



xmlrpc_registry *
xmlrpc_registry_new(xmlrpc_env * const envP) {

    xmlrpc_registry * registryP;

    XMLRPC_ASSERT_ENV_OK(envP);
    
    MALLOCVAR(registryP);

    if (registryP == NULL)
        xmlrpc_faultf(envP, "Could not allocate memory for registry");
    else {
        registryP->introspectionEnabled  = true;
        registryP->defaultMethodFunction = NULL;
        registryP->preinvokeFunction     = NULL;
        registryP->shutdownServerFn      = NULL;
        registryP->dialect               = xmlrpc_dialect_i8;

        xmlrpc_methodListCreate(envP, &registryP->methodListP);
        if (!envP->fault_occurred)
            xmlrpc_installSystemMethods(envP, registryP);

        if (envP->fault_occurred)
            free(registryP);
    }
    return registryP;
}



void 
xmlrpc_registry_free(xmlrpc_registry * const registryP) {

    XMLRPC_ASSERT_PTR_OK(registryP);

    xmlrpc_methodListDestroy(registryP->methodListP);

    free(registryP);
}



static void 
registryAddMethod(xmlrpc_env *      const envP,
                  xmlrpc_registry * const registryP,
                  const char *      const methodName,
                  xmlrpc_method1          method1,
                  xmlrpc_method2          method2,
                  const char *      const signatureString,
                  const char *      const help,
                  void *            const userData,
                  size_t            const stackSize) {

    const char * const helpString =
        help ? help : "No help is available for this method.";

    xmlrpc_methodInfo * methodP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(registryP);
    XMLRPC_ASSERT_PTR_OK(methodName);
    XMLRPC_ASSERT(method1 != NULL || method2 != NULL);

    xmlrpc_methodCreate(envP, method1, method2, userData,
                        signatureString, helpString, stackSize, &methodP);

    if (!envP->fault_occurred) {
        xmlrpc_methodListAdd(envP, registryP->methodListP, methodName,
                             methodP);

        if (envP->fault_occurred)
            xmlrpc_methodDestroy(methodP);
    }
}



void 
xmlrpc_registry_add_method_w_doc(
    xmlrpc_env *      const envP,
    xmlrpc_registry * const registryP,
    const char *      const host ATTR_UNUSED,
    const char *      const methodName,
    xmlrpc_method1    const method,
    void *            const serverInfo,
    const char *      const signatureString,
    const char *      const help) {

    XMLRPC_ASSERT(host == NULL);

    registryAddMethod(envP, registryP, methodName, method, NULL,
                      signatureString, help, serverInfo, 0);
}



void 
xmlrpc_registry_add_method(xmlrpc_env *      const envP,
                           xmlrpc_registry * const registryP,
                           const char *      const host,
                           const char *      const methodName,
                           xmlrpc_method1    const method,
                           void *            const serverInfoP) {

    xmlrpc_registry_add_method_w_doc(
        envP, registryP, host, methodName,
        method, serverInfoP, "?", "No help is available for this method.");
}



void
xmlrpc_registry_add_method2(xmlrpc_env *      const envP,
                            xmlrpc_registry * const registryP,
                            const char *      const methodName,
                            xmlrpc_method2          method,
                            const char *      const signatureString,
                            const char *      const help,
                            void *            const serverInfo) {

    registryAddMethod(envP, registryP, methodName, NULL, method,
                      signatureString, help, serverInfo, 0);
}



void
xmlrpc_registry_add_method3(
    xmlrpc_env *                       const envP,
    xmlrpc_registry *                  const registryP,
    const struct xmlrpc_method_info3 * const infoP) {

    registryAddMethod(envP, registryP, infoP->methodName, NULL,
                      infoP->methodFunction,
                      infoP->signatureString, infoP->help, infoP->serverInfo,
                      infoP->stackSize);
}



void 
xmlrpc_registry_set_default_method(
    xmlrpc_env *          const envP ATTR_UNUSED,
    xmlrpc_registry *     const registryP ATTR_UNUSED,
    xmlrpc_default_method const function,
    void *                const userData) {

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(registryP);
    XMLRPC_ASSERT_PTR_OK(function);

    /* Note: this may be the first default method, or it may be a replacement
       of the current one.
    */

    registryP->defaultMethodFunction = function;
    registryP->defaultMethodUserData = userData;
}



/* This is our guess at what a method function requires when the user
   doesn't say.
*/
#define METHOD_FUNCTION_STACK 128*1024



static size_t
methodStackSize(const xmlrpc_methodInfo * const methodP) {

    return methodP->stackSize ==
        0 ? METHOD_FUNCTION_STACK : methodP->stackSize;
}



size_t
xmlrpc_registry_max_stackSize(xmlrpc_registry * const registryP) {
/*----------------------------------------------------------------------------
   Return the maximum amount of stack required by the methods in registry
   *registryP.

   If there are no methods, return 0.
-----------------------------------------------------------------------------*/
    xmlrpc_methodNode * p;
    size_t stackSize;

    for (p = registryP->methodListP->firstMethodP, stackSize = 0;
         p;
         p = p->nextP) {
        
        stackSize = MAX(stackSize, methodStackSize(p->methodP));
    }
    return stackSize;
}



void 
xmlrpc_registry_set_preinvoke_method(
    xmlrpc_env *            const envP ATTR_UNUSED,
    xmlrpc_registry *       const registryP ATTR_UNUSED,
    xmlrpc_preinvoke_method const function,
    void *                  const userData) {

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(registryP);
    XMLRPC_ASSERT_PTR_OK(function);

    registryP->preinvokeFunction = function;
    registryP->preinvokeUserData = userData;
}



void
xmlrpc_registry_set_shutdown(xmlrpc_registry *           const registryP,
                             xmlrpc_server_shutdown_fn * const shutdownFn,
                             void *                      const context) {

    XMLRPC_ASSERT_PTR_OK(registryP);
    XMLRPC_ASSERT_PTR_OK(shutdownFn);

    registryP->shutdownServerFn = shutdownFn;

    registryP->shutdownContext = context;
}



void
xmlrpc_registry_set_dialect(xmlrpc_env *      const envP,
                            xmlrpc_registry * const registryP,
                            xmlrpc_dialect    const dialect) {

    if (dialect != xmlrpc_dialect_i8 &&
        dialect != xmlrpc_dialect_apache)
        xmlrpc_faultf(envP, "Invalid dialect argument -- not of type "
                      "xmlrpc_dialect.  Numerical value is %u", dialect);
    else
        registryP->dialect = dialect;
}



static void
callNamedMethod(xmlrpc_env *        const envP,
                xmlrpc_methodInfo * const methodP,
                xmlrpc_value *      const paramArrayP,
                void *              const callInfoP,
                xmlrpc_value **     const resultPP) {

    if (methodP->methodFnType2)
        *resultPP =
            methodP->methodFnType2(envP, paramArrayP,
                                   methodP->userData, callInfoP);
    else {
        assert(methodP->methodFnType1);
        *resultPP =
            methodP->methodFnType1(envP, paramArrayP, methodP->userData);
    }
}



void
xmlrpc_dispatchCall(xmlrpc_env *      const envP, 
                    xmlrpc_registry * const registryP,
                    const char *      const methodName, 
                    xmlrpc_value *    const paramArrayP,
                    void *            const callInfoP,
                    xmlrpc_value **   const resultPP) {

    if (registryP->preinvokeFunction)
        registryP->preinvokeFunction(envP, methodName, paramArrayP,
                                     registryP->preinvokeUserData);

    if (!envP->fault_occurred) {
        xmlrpc_methodInfo * methodP;

        xmlrpc_methodListLookupByName(registryP->methodListP, methodName,
                                      &methodP);

        if (methodP)
            callNamedMethod(envP, methodP, paramArrayP, callInfoP, resultPP);
        else {
            if (registryP->defaultMethodFunction)
                *resultPP = registryP->defaultMethodFunction(
                    envP, callInfoP, methodName, paramArrayP,
                    registryP->defaultMethodUserData);
            else {
                /* No matching method, and no default. */
                xmlrpc_env_set_fault_formatted(
                    envP, XMLRPC_NO_SUCH_METHOD_ERROR,
                    "Method '%s' not defined", methodName);
            } 
        }
    }
    /* For backward compatibility, for sloppy users: */
    if (envP->fault_occurred)
        *resultPP = NULL;
}



/*=========================================================================
**  xmlrpc_registry_process_call
**=========================================================================
**
*/

static void
serializeFault(xmlrpc_env *       const envP,
               xmlrpc_env         const fault,
               xmlrpc_mem_block * const responseXmlP) {

    xmlrpc_env env;

    xmlrpc_env_init(&env);

    xmlrpc_serialize_fault(&env, responseXmlP, &fault);

    if (env.fault_occurred)
        xmlrpc_faultf(envP,
                      "Executed XML-RPC method completely and it "
                      "generated a fault response, but we failed "
                      "to encode that fault response as XML-RPC "
                      "so we could send it to the client.  %s",
                      env.fault_string);

    xmlrpc_env_clean(&env);
}



void
xmlrpc_registry_process_call2(xmlrpc_env *        const envP,
                              xmlrpc_registry *   const registryP,
                              const char *        const callXml,
                              size_t              const callXmlLen,
                              void *              const callInfo,
                              xmlrpc_mem_block ** const responseXmlPP) {

    xmlrpc_mem_block * responseXmlP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(callXml);
    
    xmlrpc_traceXml("XML-RPC CALL", callXml, callXmlLen);

    /* Allocate our output buffer.
    ** If this fails, we need to die in a special fashion. */
    responseXmlP = XMLRPC_MEMBLOCK_NEW(char, envP, 0);
    if (!envP->fault_occurred) {
        const char * methodName;
        xmlrpc_value * paramArrayP;
        xmlrpc_env fault;
        xmlrpc_env parseEnv;

        xmlrpc_env_init(&fault);
        xmlrpc_env_init(&parseEnv);

        xmlrpc_parse_call(&parseEnv, callXml, callXmlLen, 
                          &methodName, &paramArrayP);

        if (parseEnv.fault_occurred)
            xmlrpc_env_set_fault_formatted(
                &fault, XMLRPC_PARSE_ERROR,
                "Call XML not a proper XML-RPC call.  %s",
                parseEnv.fault_string);
        else {
            xmlrpc_value * resultP;
            
            xmlrpc_dispatchCall(&fault, registryP, methodName, paramArrayP,
                                callInfo, &resultP);

            if (!fault.fault_occurred) {
                xmlrpc_serialize_response2(envP, responseXmlP,
                                           resultP, registryP->dialect);

                xmlrpc_DECREF(resultP);
            } 
            xmlrpc_strfree(methodName);
            xmlrpc_DECREF(paramArrayP);
        }
        if (!envP->fault_occurred && fault.fault_occurred)
            serializeFault(envP, fault, responseXmlP);

        xmlrpc_env_clean(&parseEnv);
        xmlrpc_env_clean(&fault);

        if (envP->fault_occurred)
            XMLRPC_MEMBLOCK_FREE(char, responseXmlP);
        else {
            *responseXmlPP = responseXmlP;
            xmlrpc_traceXml("XML-RPC RESPONSE", 
                            XMLRPC_MEMBLOCK_CONTENTS(char, responseXmlP),
                            XMLRPC_MEMBLOCK_SIZE(char, responseXmlP));
        }
    }
}



xmlrpc_mem_block *
xmlrpc_registry_process_call(xmlrpc_env *      const envP,
                             xmlrpc_registry * const registryP,
                             const char *      const host ATTR_UNUSED,
                             const char *      const callXml,
                             size_t            const callXmlLen) {

    xmlrpc_mem_block * responseXmlP;

    xmlrpc_registry_process_call2(envP, registryP, callXml, callXmlLen, NULL,
                                  &responseXmlP);

    return responseXmlP;
}



/* Copyright (C) 2001 by First Peer, Inc. All rights reserved.
** Copyright (C) 2001 by Eric Kidd. All rights reserved.
** Copyright (C) 2001 by Luke Howard. All rights reserved.
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
