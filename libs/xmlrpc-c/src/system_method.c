/* Copyright information is at end of file */

#include "xmlrpc_config.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/server.h"
#include "version.h"
#include "registry.h"
#include "method.h"

#include "system_method.h"


struct systemMethodReg {
/*----------------------------------------------------------------------------
   Information needed to register a system method
-----------------------------------------------------------------------------*/
    const char *   const methodName;
    xmlrpc_method2 const methodFunction;
    const char *   const signatureString;
    const char *   const helpText;
};



void 
xmlrpc_registry_disable_introspection(xmlrpc_registry * const registryP) {

    XMLRPC_ASSERT_PTR_OK(registryP);

    registryP->introspectionEnabled = false;
}



/*=========================================================================
  system.multicall
=========================================================================*/

static void
callOneMethod(xmlrpc_env *      const envP,
              xmlrpc_registry * const registryP,
              xmlrpc_value *    const rpcDescP,
              void *            const callInfo,
              xmlrpc_value **   const resultPP) {

    const char * methodName;
    xmlrpc_value * paramArrayP;

    XMLRPC_ASSERT_ENV_OK(envP);

    if (xmlrpc_value_type(rpcDescP) != XMLRPC_TYPE_STRUCT)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_TYPE_ERROR,
            "An element of the multicall array is type %u, but should "
            "be a struct (with members 'methodName' and 'params')",
            xmlrpc_value_type(rpcDescP));
    else {
        xmlrpc_decompose_value(envP, rpcDescP, "{s:s,s:A,*}",
                               "methodName", &methodName,
                               "params", &paramArrayP);
        if (!envP->fault_occurred) {
            /* Watch out for a deep recursion attack. */
            if (xmlrpc_streq(methodName, "system.multicall"))
                xmlrpc_env_set_fault_formatted(
                    envP,
                    XMLRPC_REQUEST_REFUSED_ERROR,
                    "Recursive system.multicall forbidden");
            else {
                xmlrpc_env env;
                xmlrpc_value * resultValP;

                xmlrpc_env_init(&env);
                xmlrpc_dispatchCall(&env, registryP, methodName, paramArrayP,
                                    callInfo,
                                    &resultValP);
                if (env.fault_occurred) {
                    /* Method failed, so result is a fault structure */
                    *resultPP = 
                        xmlrpc_build_value(
                            envP, "{s:i,s:s}",
                            "faultCode", (xmlrpc_int32) env.fault_code,
                            "faultString", env.fault_string);
                } else {
                    *resultPP = xmlrpc_build_value(envP, "(V)", resultValP);

                    xmlrpc_DECREF(resultValP);
                }
                xmlrpc_env_clean(&env);
            }
            xmlrpc_DECREF(paramArrayP);
            xmlrpc_strfree(methodName);
        }
    }
}



static void
getMethListFromMulticallPlist(xmlrpc_env *    const envP,
                              xmlrpc_value *  const paramArrayP,
                              xmlrpc_value ** const methlistPP) {

    if (xmlrpc_array_size(envP, paramArrayP) != 1)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR,
            "system.multicall takes one parameter, which is an "
            "array, each element describing one RPC.  You "
            "supplied %u arguments", 
            xmlrpc_array_size(envP, paramArrayP));
    else {
        xmlrpc_value * methlistP;

        xmlrpc_array_read_item(envP, paramArrayP, 0, &methlistP);

        XMLRPC_ASSERT_ENV_OK(envP);

        if (xmlrpc_value_type(methlistP) != XMLRPC_TYPE_ARRAY)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_TYPE_ERROR,
                "system.multicall's parameter should be an array, "
                "each element describing one RPC.  But it is type "
                "%u instead.", xmlrpc_value_type(methlistP));
        else
            *methlistPP = methlistP;

        if (envP->fault_occurred)
            xmlrpc_DECREF(methlistP);
    }
}



static xmlrpc_value *
system_multicall(xmlrpc_env *   const envP,
                 xmlrpc_value * const paramArrayP,
                 void *         const serverInfo,
                 void *         const callInfo) {

    xmlrpc_registry * registryP;
    xmlrpc_value * resultsP;
    xmlrpc_value * methlistP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_ARRAY_OK(paramArrayP);
    XMLRPC_ASSERT_PTR_OK(serverInfo);

    resultsP = NULL;  /* defeat compiler warning */

    /* Turn our arguments into something more useful. */
    registryP = (xmlrpc_registry*) serverInfo;

    getMethListFromMulticallPlist(envP, paramArrayP, &methlistP);
    if (!envP->fault_occurred) {
        /* Create an initially empty result list. */
        resultsP = xmlrpc_array_new(envP);
        if (!envP->fault_occurred) {
            /* Loop over our input list, calling each method in turn. */
            unsigned int const methodCount =
                xmlrpc_array_size(envP, methlistP);
            unsigned int i;
            for (i = 0; i < methodCount && !envP->fault_occurred; ++i) {
                xmlrpc_value * const methinfoP = 
                    xmlrpc_array_get_item(envP, methlistP, i);
            
                xmlrpc_value * resultP;
            
                XMLRPC_ASSERT_ENV_OK(envP);
            
                callOneMethod(envP, registryP, methinfoP, callInfo, &resultP);
            
                if (!envP->fault_occurred) {
                    /* Append this method result to our master array. */
                    xmlrpc_array_append_item(envP, resultsP, resultP);
                    xmlrpc_DECREF(resultP);
                }
            }
            if (envP->fault_occurred)
                xmlrpc_DECREF(resultsP);
            xmlrpc_DECREF(methlistP);
        }
    }
    return resultsP;
}



static struct systemMethodReg const methodMulticall = {
    "system.multicall",
    &system_multicall,
    "A:A",
    "Process an array of calls, and return an array of results.  Calls should "
    "be structs of the form {'methodName': string, 'params': array}. Each "
    "result will either be a single-item array containg the result value, or "
    "a struct of the form {'faultCode': int, 'faultString': string}.  This "
    "is useful when you need to make lots of small calls without lots of "
    "round trips.",
};


/*=========================================================================
   system.listMethods
=========================================================================*/


static void
createMethodListArray(xmlrpc_env *      const envP,
                      xmlrpc_registry * const registryP,
                      xmlrpc_value **   const methodListPP) {
/*----------------------------------------------------------------------------
   Create as an XML-RPC array value a list of names of methods registered
   in registry 'registryP'.

   This is the type of value that the system.listMethods method is supposed
   to return.
-----------------------------------------------------------------------------*/
    xmlrpc_value * methodListP;

    methodListP = xmlrpc_array_new(envP);

    if (!envP->fault_occurred) {
        xmlrpc_methodNode * methodNodeP;
        for (methodNodeP = registryP->methodListP->firstMethodP;
             methodNodeP && !envP->fault_occurred;
             methodNodeP = methodNodeP->nextP) {
            
            xmlrpc_value * methodNameVP;
            
            methodNameVP = xmlrpc_string_new(envP, methodNodeP->methodName);
            
            if (!envP->fault_occurred) {
                xmlrpc_array_append_item(envP, methodListP, methodNameVP);
                
                xmlrpc_DECREF(methodNameVP);
            }
        }
        if (envP->fault_occurred)
            xmlrpc_DECREF(methodListP);
    }
    *methodListPP = methodListP;
}



static xmlrpc_value *
system_listMethods(xmlrpc_env *   const envP,
                   xmlrpc_value * const paramArrayP,
                   void *         const serverInfo,
                   void *         const callInfo ATTR_UNUSED) {

    xmlrpc_registry * const registryP = serverInfo;

    xmlrpc_value * retvalP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_VALUE_OK(paramArrayP);
    XMLRPC_ASSERT_PTR_OK(serverInfo);

    xmlrpc_decompose_value(envP, paramArrayP, "()");
    if (!envP->fault_occurred) {
        if (!registryP->introspectionEnabled)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INTROSPECTION_DISABLED_ERROR,
                "Introspection is disabled in this server "
                "for security reasons");
        else
            createMethodListArray(envP, registryP, &retvalP);
    }
    return retvalP;
}



static struct systemMethodReg const methodListMethods = {
    "system.listMethods",
    &system_listMethods,
    "A:",
    "Return an array of all available XML-RPC methods on this server.",
};



/*=========================================================================
  system.methodExist
==========================================================================*/

static void
determineMethodExistence(xmlrpc_env *      const envP,
                         const char *      const methodName,
                         xmlrpc_registry * const registryP,
                         xmlrpc_value **   const existsPP) {

    xmlrpc_methodInfo * methodP;

    xmlrpc_methodListLookupByName(registryP->methodListP, methodName,
                                  &methodP);

    *existsPP = xmlrpc_bool_new(envP, !!methodP);
}
    


static xmlrpc_value *
system_methodExist(xmlrpc_env *   const envP,
                   xmlrpc_value * const paramArrayP,
                   void *         const serverInfo,
                   void *         const callInfo ATTR_UNUSED) {

    xmlrpc_registry * const registryP = serverInfo;

    xmlrpc_value * retvalP;
    
    const char * methodName;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_VALUE_OK(paramArrayP);
    XMLRPC_ASSERT_PTR_OK(serverInfo);

    xmlrpc_decompose_value(envP, paramArrayP, "(s)", &methodName);

    if (!envP->fault_occurred) {
        determineMethodExistence(envP, methodName, registryP, &retvalP);

        xmlrpc_strfree(methodName);
    }

    return retvalP;
}



static struct systemMethodReg const methodMethodExist = {
    "system.methodExist",
    &system_methodExist,
    "s:b",
    "Tell whether a method by a specified name exists on this server",
};



/*=========================================================================
  system.methodHelp
=========================================================================*/


static void
getHelpString(xmlrpc_env *      const envP,
              const char *      const methodName,
              xmlrpc_registry * const registryP,
              xmlrpc_value **   const helpStringPP) {

    xmlrpc_methodInfo * methodP;

    xmlrpc_methodListLookupByName(registryP->methodListP, methodName,
                                  &methodP);

    if (!methodP)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_NO_SUCH_METHOD_ERROR,
            "Method '%s' does not exist", methodName);
    else
        *helpStringPP = xmlrpc_string_new(envP, methodP->helpText);
}
    


static xmlrpc_value *
system_methodHelp(xmlrpc_env *   const envP,
                  xmlrpc_value * const paramArrayP,
                  void *         const serverInfo,
                  void *         const callInfo ATTR_UNUSED) {

    xmlrpc_registry * const registryP = serverInfo;

    xmlrpc_value * retvalP;
    
    const char * methodName;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_VALUE_OK(paramArrayP);
    XMLRPC_ASSERT_PTR_OK(serverInfo);
    
    xmlrpc_decompose_value(envP, paramArrayP, "(s)", &methodName);

    if (!envP->fault_occurred) {
        if (!registryP->introspectionEnabled)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INTROSPECTION_DISABLED_ERROR,
                "Introspection is disabled in this server "
                "for security reasons");
        else
            getHelpString(envP, methodName, registryP, &retvalP);

        xmlrpc_strfree(methodName);
    }

    return retvalP;
}


static struct systemMethodReg const methodMethodHelp = {
    "system.methodHelp",
    &system_methodHelp,
    "s:s",
    "Given the name of a method, return a help string.",
};



/*=========================================================================
  system.methodSignature
==========================================================================*/

static void
buildNoSigSuppliedResult(xmlrpc_env *    const envP,
                         xmlrpc_value ** const resultPP) {

    xmlrpc_env env;

    xmlrpc_env_init(&env);

    *resultPP = xmlrpc_string_new(&env, "undef");
    if (env.fault_occurred)
        xmlrpc_faultf(envP, "Unable to construct 'undef'.  %s",
                      env.fault_string);

    xmlrpc_env_clean(&env);
}
    


static void
buildSignatureValue(xmlrpc_env *              const envP,
                    struct xmlrpc_signature * const signatureP,
                    xmlrpc_value **           const sigValuePP) {

    xmlrpc_value * sigValueP;
    unsigned int i;

    sigValueP = xmlrpc_array_new(envP);

    {
        xmlrpc_value * retTypeVP;

        retTypeVP = xmlrpc_string_new(envP, signatureP->retType);

        xmlrpc_array_append_item(envP, sigValueP, retTypeVP);

        xmlrpc_DECREF(retTypeVP);
    }
    for (i = 0; i < signatureP->argCount && !envP->fault_occurred; ++i) {
        xmlrpc_value * argTypeVP;

        argTypeVP = xmlrpc_string_new(envP, signatureP->argList[i]);
        if (!envP->fault_occurred) {
            xmlrpc_array_append_item(envP, sigValueP, argTypeVP);

            xmlrpc_DECREF(argTypeVP);
        }
    }

    if (envP->fault_occurred)
        xmlrpc_DECREF(sigValueP);

    *sigValuePP = sigValueP;
}

                    

static void
getSignatureList(xmlrpc_env *      const envP,
                 xmlrpc_registry * const registryP,
                 const char *      const methodName,
                 xmlrpc_value **   const signatureListPP) {
/*----------------------------------------------------------------------------
  Get the signature list array for method named 'methodName' from registry
  'registryP'.

  If there is no signature information for the method in the registry,
  return *signatureListPP == NULL.

  Nonexistent method is considered a failure.
-----------------------------------------------------------------------------*/
    xmlrpc_methodInfo * methodP;

    xmlrpc_methodListLookupByName(registryP->methodListP, methodName,
                                  &methodP);

    if (!methodP)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_NO_SUCH_METHOD_ERROR,
            "Method '%s' does not exist", methodName);
    else {
        if (!methodP->signatureListP->firstSignatureP)
            *signatureListPP = NULL;
        else {
            xmlrpc_value * signatureListP;

            signatureListP = xmlrpc_array_new(envP);

            if (!envP->fault_occurred) {
                struct xmlrpc_signature * signatureP;
                for (signatureP = methodP->signatureListP->firstSignatureP;
                     signatureP && !envP->fault_occurred;
                     signatureP = signatureP->nextP) {
                    
                    xmlrpc_value * signatureVP = NULL;
                    
                    buildSignatureValue(envP, signatureP, &signatureVP);
                    
                    xmlrpc_array_append_item(envP,
                                             signatureListP, signatureVP);
                    
                    xmlrpc_DECREF(signatureVP);
                }
                if (envP->fault_occurred)
                    xmlrpc_DECREF(signatureListP);
            }
            *signatureListPP = signatureListP;
        }
    }
}



/* Microsoft Visual C in debug mode produces code that complains about
   returning an undefined value from system_methodSignature().  It's a bogus
   complaint, because this function is defined to return nothing meaningful
   those cases.  So we disable the check.
*/
#pragma runtime_checks("u", off)



static xmlrpc_value *
system_methodSignature(xmlrpc_env *   const envP,
                       xmlrpc_value * const paramArrayP,
                       void *         const serverInfo,
                       void *         const callInfo ATTR_UNUSED) {

    xmlrpc_registry * const registryP = (xmlrpc_registry *) serverInfo;

    xmlrpc_value * retvalP;
    const char * methodName;
    xmlrpc_env env;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_VALUE_OK(paramArrayP);
    XMLRPC_ASSERT_PTR_OK(serverInfo);

    xmlrpc_env_init(&env);

    /* Turn our arguments into something more useful. */
    xmlrpc_decompose_value(&env, paramArrayP, "(s)", &methodName);
    if (env.fault_occurred)
        xmlrpc_env_set_fault_formatted(
            envP, env.fault_code,
            "Invalid parameter list.  %s", env.fault_string);
    else {
        if (!registryP->introspectionEnabled)
            xmlrpc_env_set_fault(envP, XMLRPC_INTROSPECTION_DISABLED_ERROR,
                                 "Introspection disabled on this server");
        else {
            xmlrpc_value * signatureListP;

            getSignatureList(envP, registryP, methodName, &signatureListP);

            if (!envP->fault_occurred) {
                if (signatureListP)
                    retvalP = signatureListP;
                else
                    buildNoSigSuppliedResult(envP, &retvalP);
            }
        }
        xmlrpc_strfree(methodName);
    }
    xmlrpc_env_clean(&env);

    return retvalP;
}



#pragma runtime_checks("u", restore)



static struct systemMethodReg const methodMethodSignature = {
    "system.methodSignature",
    &system_methodSignature,
    "A:s",
    "Given the name of a method, return an array of legal signatures. "
    "Each signature is an array of strings.  The first item of each signature "
    "is the return type, and any others items are parameter types.",
};




/*=========================================================================
  system.shutdown
==========================================================================*/

static xmlrpc_value *
system_shutdown(xmlrpc_env *   const envP,
                xmlrpc_value * const paramArrayP,
                void *         const serverInfo,
                void *         const callInfo) {
    
    xmlrpc_registry * const registryP = (xmlrpc_registry *) serverInfo;

    xmlrpc_value * retvalP;
    const char * comment;
    xmlrpc_env env;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_VALUE_OK(paramArrayP);
    XMLRPC_ASSERT_PTR_OK(serverInfo);

    xmlrpc_env_init(&env);

    /* Turn our arguments into something more useful. */
    xmlrpc_decompose_value(&env, paramArrayP, "(s)", &comment);
    if (env.fault_occurred)
        xmlrpc_env_set_fault_formatted(
            envP, env.fault_code,
            "Invalid parameter list.  %s", env.fault_string);
    else {
        if (!registryP->shutdownServerFn)
            xmlrpc_env_set_fault(
                envP, 0, "This server program is not capable of "
                "shutting down");
        else {
            registryP->shutdownServerFn(
                &env, registryP->shutdownContext, comment, callInfo);

            if (env.fault_occurred)
                xmlrpc_env_set_fault(envP, env.fault_code, env.fault_string);
            else {
                retvalP = xmlrpc_int_new(&env, 0);
                
                if (env.fault_occurred)
                    xmlrpc_faultf(envP,
                                  "Failed to construct return value.  %s",
                                  env.fault_string);
            }
        }
        xmlrpc_strfree(comment);
    }
    xmlrpc_env_clean(&env);

    return retvalP;
}



static struct systemMethodReg const methodShutdown = {
    "system.shutdown",
    &system_shutdown,
    "i:s",
    "Shut down the server.  Return code is always zero.",
};



/*=========================================================================
  system.capabilities
=========================================================================*/

static void
constructCapabilities(xmlrpc_env *      const envP,
                      xmlrpc_registry * const registryP ATTR_UNUSED,
                      xmlrpc_value **   const capabilitiesPP) {

    *capabilitiesPP =
        xmlrpc_build_value(
            envP, "{s:s,s:i,s:i,s:i,s:i}",
            "facility", "xmlrpc-c",
            "version_major", XMLRPC_VERSION_MAJOR,
            "version_minor", XMLRPC_VERSION_MINOR,
            "version_point", XMLRPC_VERSION_POINT,
            "protocol_version", 2
            );

}



static xmlrpc_value *
system_capabilities(xmlrpc_env *   const envP,
                    xmlrpc_value * const paramArrayP,
                    void *         const serverInfo,
                    void *         const callInfo ATTR_UNUSED) {
    
    xmlrpc_registry * const registryP = serverInfo;

    xmlrpc_value * retvalP;
    
    unsigned int paramCount;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_VALUE_OK(paramArrayP);
    XMLRPC_ASSERT_PTR_OK(serverInfo);

    paramCount = xmlrpc_array_size(envP, paramArrayP);

    if (paramCount > 0)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_INDEX_ERROR,
            "There are no parameters.  You supplied %u", paramCount);
    else
        constructCapabilities(envP, registryP, &retvalP);

    return retvalP;
}



static struct systemMethodReg const methodCapabilities = {
    "system.capabilities",
    &system_capabilities,
    "S:",
    "Return the capabilities of XML-RPC server.  This includes the "
    "version number of the XML-RPC For C/C++ software"
};



/*=========================================================================
  system.getCapabilities
=========================================================================*/

/* This implements a standard.
   See http://tech.groups.yahoo.com/group/xml-rpc/message/2897 .
*/

static void
listCapabilities(xmlrpc_env *      const envP,
                 xmlrpc_registry * const registryP ATTR_UNUSED,
                 xmlrpc_value **   const capabilitiesPP) {

    *capabilitiesPP =
        xmlrpc_build_value(
            envP, "{s:{s:s,s:i}}",
            "introspect",
              "specUrl",
                "http://xmlrpc-c.sourceforge.net/xmlrpc-c/introspection.html",
              "specVersion",
                 1
            );
}



static xmlrpc_value *
system_getCapabilities(xmlrpc_env *   const envP,
                       xmlrpc_value * const paramArrayP,
                       void *         const serverInfo,
                       void *         const callInfo ATTR_UNUSED) {
    
    xmlrpc_registry * const registryP = serverInfo;

    xmlrpc_value * retvalP;
    
    unsigned int paramCount;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_VALUE_OK(paramArrayP);
    XMLRPC_ASSERT_PTR_OK(serverInfo);

    paramCount = xmlrpc_array_size(envP, paramArrayP);

    if (paramCount > 0)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_INDEX_ERROR,
            "There are no parameters.  You supplied %u", paramCount);
    else
        listCapabilities(envP, registryP, &retvalP);

    return retvalP;
}



static struct systemMethodReg const methodGetCapabilities = {
    "system.getCapabilities",
    &system_getCapabilities,
    "S:",
    "Return the list of standard capabilities of XML-RPC server.  "
    "See http://tech.groups.yahoo.com/group/xml-rpc/message/2897"
};



/*============================================================================
  Installer of system methods
============================================================================*/

static void
registerSystemMethod(xmlrpc_env *           const envP,
                     xmlrpc_registry *      const registryP,
                     struct systemMethodReg const methodReg) {

    xmlrpc_env env;
    xmlrpc_env_init(&env);
    
    xmlrpc_registry_add_method2(
        &env, registryP, methodReg.methodName,
        methodReg.methodFunction,
        methodReg.signatureString, methodReg.helpText, registryP);
    
    if (env.fault_occurred)
        xmlrpc_faultf(envP, "Failed to register '%s' system method.  %s",
                      methodReg.methodName, env.fault_string);
    
    xmlrpc_env_clean(&env);
}



void
xmlrpc_installSystemMethods(xmlrpc_env *      const envP,
                            xmlrpc_registry * const registryP) {
/*----------------------------------------------------------------------------
   Install the built-in methods (system.*) into registry 'registryP'.
-----------------------------------------------------------------------------*/
    if (!envP->fault_occurred)
        registerSystemMethod(envP, registryP, methodListMethods);

    if (!envP->fault_occurred)
        registerSystemMethod(envP, registryP, methodMethodExist);

    if (!envP->fault_occurred)
        registerSystemMethod(envP, registryP, methodMethodHelp);

    if (!envP->fault_occurred) 
        registerSystemMethod(envP, registryP, methodMethodSignature);

    if (!envP->fault_occurred)
        registerSystemMethod(envP, registryP, methodMulticall);

    if (!envP->fault_occurred)
        registerSystemMethod(envP, registryP, methodShutdown);

    if (!envP->fault_occurred)
        registerSystemMethod(envP, registryP, methodCapabilities);

    if (!envP->fault_occurred)
        registerSystemMethod(envP, registryP, methodGetCapabilities);
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

