/* Copyright information is at end of file. */

#include "xmlrpc_config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>

#include "bool.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/util.h"
#include "xmlrpc-c/xmlparser.h"
#include "parse_value.h"


/* Notes about XML-RPC XML documents:

   Contain CDATA: methodName, i4, int, boolean, string, double,
                  dateTime.iso8601, base64, name

   We attempt to validate the structure of the XML document carefully.
   We also try *very* hard to handle malicious data gracefully, and without
   leaking memory.

   The CHECK_NAME and CHECK_CHILD_COUNT macros examine an XML element, and
   invoke XMLRPC_FAIL if something looks wrong.
*/

#define CHECK_NAME(env,elem,name) \
    do \
        if (!xmlrpc_streq((name), xml_element_name(elem))) \
            XMLRPC_FAIL2(env, XMLRPC_PARSE_ERROR, \
             "Expected element of type <%s>, found <%s>", \
                         (name), xml_element_name(elem)); \
    while (0)

#define CHECK_CHILD_COUNT(env,elem,count) \
    do \
        if (xml_element_children_size(elem) != (count)) \
            XMLRPC_FAIL3(env, XMLRPC_PARSE_ERROR, \
             "Expected <%s> to have %d children, found %d", \
                         xml_element_name(elem), (count), \
                         xml_element_children_size(elem)); \
    while (0)

static xml_element *
get_child_by_name (xmlrpc_env *env, xml_element *parent, char *name)
{
    size_t child_count, i;
    xml_element **children;

    children = xml_element_children(parent);
    child_count = xml_element_children_size(parent);
    for (i = 0; i < child_count; i++) {
        if (xmlrpc_streq(xml_element_name(children[i]), name))
            return children[i];
    }
    
    xmlrpc_env_set_fault_formatted(env, XMLRPC_PARSE_ERROR,
                                   "Expected <%s> to have child <%s>",
                                   xml_element_name(parent), name);
    return NULL;
}



static void
setParseFault(xmlrpc_env * const envP,
              const char * const format,
              ...) {

    va_list args;
    va_start(args, format);
    xmlrpc_set_fault_formatted_v(envP, XMLRPC_PARSE_ERROR, format, args);
    va_end(args);
}



/*=========================================================================
**  convert_params
**=========================================================================
**  Convert an XML element representing a list of params into an
**  xmlrpc_value (of type array).
*/

static xmlrpc_value *
convert_params(xmlrpc_env *        const envP,
               const xml_element * const elemP) {
/*----------------------------------------------------------------------------
   Convert an XML element representing a list of parameters (i.e.  a
   <params> element) to an xmlrpc_value of type array.  Note that an
   array is normally represented in XML by a <value> element.  We use
   type xmlrpc_value to represent the parameter list just for convenience.
-----------------------------------------------------------------------------*/
    xmlrpc_value *array, *item;
    int size, i;
    xml_element **params, *param, *value;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(elemP != NULL);

    /* Set up our error-handling preconditions. */
    array = item = NULL;

    /* Allocate an array to hold our parameters. */
    array = xmlrpc_build_value(envP, "()");
    XMLRPC_FAIL_IF_FAULT(envP);

    /* We're responsible for checking our own element name. */
    CHECK_NAME(envP, elemP, "params");    

    /* Iterate over our children. */
    size = xml_element_children_size(elemP);
    params = xml_element_children(elemP);
    for (i = 0; i < size; ++i) {
        unsigned int const maxNest = xmlrpc_limit_get(XMLRPC_NESTING_LIMIT_ID);

        param = params[i];
        CHECK_NAME(envP, param, "param");
        CHECK_CHILD_COUNT(envP, param, 1);

        value = xml_element_children(param)[0];

        CHECK_NAME(envP, value, "value");

        xmlrpc_parseValue(envP, maxNest, value, &item);
        XMLRPC_FAIL_IF_FAULT(envP);

        xmlrpc_array_append_item(envP, array, item);
        xmlrpc_DECREF(item);
        item = NULL;
        XMLRPC_FAIL_IF_FAULT(envP);
    }

 cleanup:
    if (envP->fault_occurred) {
        if (array)
            xmlrpc_DECREF(array);
        if (item)
            xmlrpc_DECREF(item);
        return NULL;
    }
    return array;
}



static void
parseCallXml(xmlrpc_env *   const envP,
             const char *   const xmlData,
             size_t         const xmlLen,
             xml_element ** const callElemPP) {
/*----------------------------------------------------------------------------
   Parse the XML of an XML-RPC call.
-----------------------------------------------------------------------------*/
    xml_element * callElemP;
    xmlrpc_env env;

    xmlrpc_env_init(&env);
    xml_parse(&env, xmlData, xmlLen, &callElemP);
    if (env.fault_occurred)
        xmlrpc_env_set_fault_formatted(
            envP, env.fault_code, "Call is not valid XML.  %s",
            env.fault_string);
    else {
        if (!xmlrpc_streq(xml_element_name(callElemP), "methodCall"))
            setParseFault(envP,
                          "XML-RPC call should be a <methodCall> element.  "
                          "Instead, we have a <%s> element.",
                          xml_element_name(callElemP));

        if (!envP->fault_occurred)
            *callElemPP = callElemP;

        if (envP->fault_occurred)
            xml_element_free(callElemP);
    }
    xmlrpc_env_clean(&env);
}



static void
parseMethodNameElement(xmlrpc_env *  const envP,
                       xml_element * const nameElemP,
                       const char ** const methodNameP) {
    
    XMLRPC_ASSERT(xmlrpc_streq(xml_element_name(nameElemP), "methodName"));

    if (xml_element_children_size(nameElemP) > 0)
        setParseFault(envP, "A <methodName> element should not have "
                      "children.  This one has %u of them.",
                      xml_element_children_size(nameElemP));
    else {
        const char * const cdata = xml_element_cdata(nameElemP);

        xmlrpc_validate_utf8(envP, cdata, strlen(cdata));

        if (!envP->fault_occurred) {
            *methodNameP = strdup(cdata);
            if (*methodNameP == NULL)
                xmlrpc_faultf(envP,
                              "Could not allocate memory for method name");
        }
    }
}            



static void
parseCallChildren(xmlrpc_env *    const envP,
                  xml_element *   const callElemP,
                  const char **   const methodNameP,
                  xmlrpc_value ** const paramArrayPP ) {
/*----------------------------------------------------------------------------
  Parse the children of a <methodCall> XML element *callElemP.  They should
  be <methodName> and <params>.
-----------------------------------------------------------------------------*/
    size_t const callChildCount = xml_element_children_size(callElemP);

    xml_element * nameElemP;
        
    XMLRPC_ASSERT(xmlrpc_streq(xml_element_name(callElemP), "methodCall"));
    
    nameElemP = get_child_by_name(envP, callElemP, "methodName");
    
    if (!envP->fault_occurred) {
        parseMethodNameElement(envP, nameElemP, methodNameP);
            
        if (!envP->fault_occurred) {
            /* Convert our parameters. */
            if (callChildCount > 1) {
                xml_element * paramsElemP;

                paramsElemP = get_child_by_name(envP, callElemP, "params");
                    
                if (!envP->fault_occurred)
                    *paramArrayPP = convert_params(envP, paramsElemP);
            } else {
                /* Workaround for Ruby XML-RPC and old versions of
                   xmlrpc-epi.  Future improvement: Instead of looking
                   at child count, we should just check for existence
                   of <params>.
                */
                *paramArrayPP = xmlrpc_array_new(envP);
            }
            if (!envP->fault_occurred) {
                if (callChildCount > 2)
                    setParseFault(envP, "<methodCall> has extraneous "
                                  "children, other than <methodName> and "
                                  "<params>.  Total child count = %u",
                                  callChildCount);
                    
                if (envP->fault_occurred)
                    xmlrpc_DECREF(*paramArrayPP);
            }
            if (envP->fault_occurred)
                xmlrpc_strfree(*methodNameP);
        }
    }
}



void 
xmlrpc_parse_call(xmlrpc_env *    const envP,
                  const char *    const xmlData,
                  size_t          const xmlLen,
                  const char **   const methodNameP,
                  xmlrpc_value ** const paramArrayPP) {
/*----------------------------------------------------------------------------
  Given some XML text, attempt to parse it as an XML-RPC call.
  Return as *methodNameP the name of the method identified in the call
  and as *paramArrayPP the parameter list as an XML-RPC array.
  Caller must free() and xmlrpc_DECREF() these, respectively).
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(xmlData != NULL);
    XMLRPC_ASSERT(methodNameP != NULL && paramArrayPP != NULL);

    /* SECURITY: Last-ditch attempt to make sure our content length is
       legal.  XXX - This check occurs too late to prevent an attacker
       from creating an enormous memory block, so you should try to
       enforce it *before* reading any data off the network.
     */
    if (xmlLen > xmlrpc_limit_get(XMLRPC_XML_SIZE_LIMIT_ID))
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_LIMIT_EXCEEDED_ERROR,
            "XML-RPC request too large.  Max allowed is %u bytes",
            xmlrpc_limit_get(XMLRPC_XML_SIZE_LIMIT_ID));
    else {
        xml_element * callElemP;
        parseCallXml(envP, xmlData, xmlLen, &callElemP);
        if (!envP->fault_occurred) {
            parseCallChildren(envP, callElemP, methodNameP, paramArrayPP);

            xml_element_free(callElemP);
        }
    }
    if (envP->fault_occurred) {
        /* Should not be necessary, but for backward compatibility: */
        *methodNameP  = NULL;
        *paramArrayPP = NULL;
    }
}



static void
interpretFaultCode(xmlrpc_env *   const envP,
                   xmlrpc_value * const faultCodeVP,
                   int *          const faultCodeP) {
                   
    xmlrpc_env fcEnv;
    xmlrpc_env_init(&fcEnv);

    xmlrpc_read_int(&fcEnv, faultCodeVP, faultCodeP);
    if (fcEnv.fault_occurred)
        xmlrpc_faultf(envP, "Invalid value for 'faultCode' member.  %s",
                      fcEnv.fault_string);

    xmlrpc_env_clean(&fcEnv);
}



static void
interpretFaultString(xmlrpc_env *   const envP,
                     xmlrpc_value * const faultStringVP,
                     const char **  const faultStringP) {

    xmlrpc_env fsEnv;
    xmlrpc_env_init(&fsEnv);

    xmlrpc_read_string(&fsEnv, faultStringVP, faultStringP);

    if (fsEnv.fault_occurred)
        xmlrpc_faultf(envP, "Invalid value for 'faultString' member.  %s",
                      fsEnv.fault_string);

    xmlrpc_env_clean(&fsEnv);
}



static void
interpretFaultValue(xmlrpc_env *   const envP,
                    xmlrpc_value * const faultVP,
                    int *          const faultCodeP,
                    const char **  const faultStringP) {
                
    if (faultVP->_type != XMLRPC_TYPE_STRUCT)
        setParseFault(envP,
                      "<value> element of <fault> response is not "
                      "of structure type");
    else {
        xmlrpc_value * faultCodeVP;
        xmlrpc_env fvEnv;

        xmlrpc_env_init(&fvEnv);

        xmlrpc_struct_read_value(&fvEnv, faultVP, "faultCode", &faultCodeVP);
        if (!fvEnv.fault_occurred) {
            interpretFaultCode(&fvEnv, faultCodeVP, faultCodeP);
            
            if (!fvEnv.fault_occurred) {
                xmlrpc_value * faultStringVP;

                xmlrpc_struct_read_value(&fvEnv, faultVP, "faultString",
                                         &faultStringVP);
                if (!fvEnv.fault_occurred) {
                    interpretFaultString(&fvEnv, faultStringVP, faultStringP);

                    xmlrpc_DECREF(faultStringVP);
                }
            }
            xmlrpc_DECREF(faultCodeVP);
        }
        if (fvEnv.fault_occurred)
            setParseFault(envP, "Invalid struct for <fault> value.  %s",
                          fvEnv.fault_string);

        xmlrpc_env_clean(&fvEnv);
    }
}



static void
parseFaultElement(xmlrpc_env *        const envP,
                  const xml_element * const faultElement,
                  int *               const faultCodeP,
                  const char **       const faultStringP) {
                  
    unsigned int const maxRecursion =
        xmlrpc_limit_get(XMLRPC_NESTING_LIMIT_ID);

    XMLRPC_ASSERT(xmlrpc_streq(xml_element_name(faultElement), "fault"));

    if (xml_element_children_size(faultElement) != 1)
        setParseFault(envP, "<fault> element should have 1 child, "
                      "but it has %u.",
                      xml_element_children_size(faultElement));
    else {
        xml_element * const faultValueP =
            xml_element_children(faultElement)[0];
        const char * const elemName = xml_element_name(faultValueP);

        if (!xmlrpc_streq(elemName, "value"))
            setParseFault(envP,
                          "<fault> contains a <%s> element.  "
                          "Only <value> makes sense.",
                          elemName);
        else {
            xmlrpc_value * faultVP;

            xmlrpc_parseValue(envP, maxRecursion, faultValueP, &faultVP);
        
            if (!envP->fault_occurred) {
                interpretFaultValue(envP, faultVP, faultCodeP, faultStringP);
                
                xmlrpc_DECREF(faultVP);
            }
        }
    }
}



static void
parseParamsElement(xmlrpc_env *        const envP,
                   const xml_element * const paramsElementP,
                   xmlrpc_value **     const resultPP) {

    xmlrpc_value * paramsVP;
    xmlrpc_env env;

    xmlrpc_env_init(&env);

    XMLRPC_ASSERT(xmlrpc_streq(xml_element_name(paramsElementP), "params"));

    paramsVP = convert_params(envP, paramsElementP);

    if (!envP->fault_occurred) {
        int arraySize;
        xmlrpc_env sizeEnv;

        XMLRPC_ASSERT_ARRAY_OK(paramsVP);
        
        xmlrpc_env_init(&sizeEnv);

        arraySize = xmlrpc_array_size(&sizeEnv, paramsVP);
        /* Since it's a valid array, as asserted above, can't fail */
        XMLRPC_ASSERT(!sizeEnv.fault_occurred);

        if (arraySize != 1)
            setParseFault(envP, "Contains %d items.  It should have 1.",
                          arraySize);
        else {
            xmlrpc_array_read_item(envP, paramsVP, 0, resultPP);
        }
        xmlrpc_DECREF(paramsVP);
        xmlrpc_env_clean(&sizeEnv);
    }
    if (env.fault_occurred)
        xmlrpc_env_set_fault_formatted(
            envP, env.fault_code,
            "Invalid <params> element.  %s", env.fault_string);

    xmlrpc_env_clean(&env);
}



static void
parseMethodResponseElt(xmlrpc_env *        const envP,
                       const xml_element * const methodResponseEltP,
                       xmlrpc_value **     const resultPP,
                       int *               const faultCodeP,
                       const char **       const faultStringP) {
    
    XMLRPC_ASSERT(xmlrpc_streq(xml_element_name(methodResponseEltP),
                               "methodResponse"));

    if (xml_element_children_size(methodResponseEltP) == 1) {
        xml_element * const child =
            xml_element_children(methodResponseEltP)[0];
        
        if (xmlrpc_streq(xml_element_name(child), "params")) {
            /* It's a successful response */
            parseParamsElement(envP, child, resultPP);
            *faultStringP = NULL;
        } else if (xmlrpc_streq(xml_element_name(child), "fault")) {
            /* It's a failure response */
            parseFaultElement(envP, child, faultCodeP, faultStringP);
        } else
            setParseFault(envP,
                          "<methodResponse> must contain <params> or <fault>, "
                          "but contains <%s>.", xml_element_name(child));
    } else
        setParseFault(envP,
                      "<methodResponse> has %u children, should have 1.",
                      xml_element_children_size(methodResponseEltP));
}



void
xmlrpc_parse_response2(xmlrpc_env *    const envP,
                       const char *    const xmlData,
                       size_t          const xmlDataLen,
                       xmlrpc_value ** const resultPP,
                       int *           const faultCodeP,
                       const char **   const faultStringP) {
/*----------------------------------------------------------------------------
  Given some XML text, attempt to parse it as an XML-RPC response.

  If the response is a regular, valid response, return a new reference
  to the appropriate value as *resultP and return NULL as
  *faultStringP and nothing as *faultCodeP.

  If the response is valid, but indicates a failure of the RPC, return the
  fault string in newly malloc'ed space as *faultStringP and the fault
  code as *faultCodeP and nothing as *resultP.

  If the XML text is not a valid response or something prevents us from
  parsing it, return a description of the error as *envP and nothing else.
-----------------------------------------------------------------------------*/
    xml_element * response;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(xmlData != NULL);

    /* SECURITY: Last-ditch attempt to make sure our content length is legal.
    ** XXX - This check occurs too late to prevent an attacker from creating
    ** an enormous memory block, so you should try to enforce it
    ** *before* reading any data off the network. */
    if (xmlDataLen > xmlrpc_limit_get(XMLRPC_XML_SIZE_LIMIT_ID))
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_LIMIT_EXCEEDED_ERROR,
            "XML-RPC response too large.  Our limit is %u characters.  "
            "We got %u characters",
            xmlrpc_limit_get(XMLRPC_XML_SIZE_LIMIT_ID), xmlDataLen);
    else {
        xmlrpc_env env;
        xmlrpc_env_init(&env);

        xml_parse(&env, xmlData, xmlDataLen, &response);

        if (env.fault_occurred)
            setParseFault(envP, "Not valid XML.  %s", env.fault_string);
        else {
            /* Pick apart and verify our structure. */
            if (xmlrpc_streq(xml_element_name(response), "methodResponse")) {
                parseMethodResponseElt(envP, response,
                                       resultPP, faultCodeP, faultStringP);
            } else
                setParseFault(envP, "XML-RPC response must consist of a "
                              "<methodResponse> element.  "
                              "This has a <%s> instead.",
                              xml_element_name(response));
            
            xml_element_free(response);
        }
        xmlrpc_env_clean(&env);
    }
}



xmlrpc_value *
xmlrpc_parse_response(xmlrpc_env * const envP,
                      const char * const xmlData,
                      size_t       const xmlDataLen) {
/*----------------------------------------------------------------------------
   This exists for backward compatibility.  It is like
   xmlrpc_parse_response2(), except that it merges the concepts of a
   failed RPC and an error in executing the RPC.
-----------------------------------------------------------------------------*/
    xmlrpc_value * retval;
    xmlrpc_value * result;
    const char * faultString;
    int faultCode;

    xmlrpc_parse_response2(envP, xmlData, xmlDataLen,
                           &result, &faultCode, &faultString);
    
    if (envP->fault_occurred)
        retval = NULL;
    else {
        if (faultString) {
            xmlrpc_env_set_fault(envP, faultCode, faultString);
            xmlrpc_strfree(faultString);
            retval = NULL;
        } else
            retval = result;  /* transfer reference */
    }
    return retval;
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
** SUCH DAMAGE. */
