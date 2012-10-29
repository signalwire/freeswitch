/* Copyright information is at end of file */

/* Implementation note:

   The printf format specifiers we use appear to be entirely standard,
   except for the "long long" one, which is %I64 on Windows and %lld
   everywhere else.  We could use the C99 standard macro PRId64 for that,
   but on at least one 64-bit-long GNU compiler, PRId64 is "ld", which is
   considered to be incompatible with long long.  So we have XMLRPC_PRId64.
*/

#include "xmlrpc_config.h"

#include <assert.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>

#include "int.h"
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"
#include "double.h"

#define CRLF "\015\012"
#define XML_PROLOGUE "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"CRLF
#define APACHE_URL "http://ws.apache.org/xmlrpc/namespaces/extensions"
#define XMLNS_APACHE "xmlns:ex=\"" APACHE_URL "\""


static void
addString(xmlrpc_env *       const envP,
          xmlrpc_mem_block * const outputP,
          const char *       const string) {

    XMLRPC_MEMBLOCK_APPEND(char, envP, outputP, string, strlen(string));
}



static void 
formatOut(xmlrpc_env *       const envP,
          xmlrpc_mem_block * const outputP,
          const char *       const formatString,
          ...) {
/*----------------------------------------------------------------------------
  A lightweight print routine for use with various serialization
  functions.

  Use this routine only for printing small objects -- it uses a
  fixed-size internal buffer and returns an error on overflow.  In
  particular, do NOT use this routine to print XML-RPC string values!
-----------------------------------------------------------------------------*/
    va_list args;
    char buffer[128];
    int rc;

    XMLRPC_ASSERT_ENV_OK(envP);

    va_start(args, formatString);

    rc = XMLRPC_VSNPRINTF(buffer, sizeof(buffer), formatString, args);

    /* Old vsnprintf() (and Windows) fails with return value -1 if the full
       string doesn't fit in the buffer.  New vsnprintf() puts whatever will
       fit in the buffer, and returns the length of the full string
       regardless.  For us, this truncation is a failure.
    */

    if (rc < 0)
        xmlrpc_faultf(envP, "formatOut() overflowed internal buffer");
    else {
        unsigned int const formattedLen = rc;

        if (formattedLen + 1 >= (sizeof(buffer)))
            xmlrpc_faultf(envP, "formatOut() overflowed internal buffer");
        else
            XMLRPC_MEMBLOCK_APPEND(char, envP, outputP, buffer, formattedLen);
    }
    va_end(args);
}



static void 
assertValidUtf8(const char * const str ATTR_UNUSED,
                size_t       const len ATTR_UNUSED) {
/*----------------------------------------------------------------------------
   Assert that the string 'str' of length 'len' is valid UTF-8.
-----------------------------------------------------------------------------*/
#if !defined NDEBUG
    /* Check the assertion; if it's false, issue a message to
       Standard Error, but otherwise ignore it.
    */
    xmlrpc_env env;

    xmlrpc_env_init(&env);
    xmlrpc_validate_utf8(&env, str, len);
    if (env.fault_occurred)
        fprintf(stderr, "*** xmlrpc-c WARNING ***: %s (%s)\n",
                "Xmlrpc-c sending corrupted UTF-8 data to network",
                env.fault_string);
    xmlrpc_env_clean(&env);
#endif
}



static size_t
escapedSize(const char * const chars,
            size_t       const len) {
    
    size_t size;
    size_t i;

    size = 0;
    for (i = 0; i < len; ++i) {
        if (chars[i] == '<')
            size += 4; /* &lt; */
        else if (chars[i] == '>')
            size += 4; /* &gt; */
        else if (chars[i] == '&')
            size += 5; /* &amp; */
        else if (chars[i] == '\r')
            size += 6; /* &#x0d; */
        else
            size += 1;
    }
    return size;
}



static void
escapeForXml(xmlrpc_env *        const envP, 
             const char *        const chars,
             size_t              const len,
             xmlrpc_mem_block ** const outputPP) {
/*----------------------------------------------------------------------------
   Escape & and < in a UTF-8 string so as to make it suitable for the
   content of an XML element.  I.e. turn them into entity references
   &amp; and &lt;.

   Also change > to &gt;, even though not required for XML, for
   symmetry.

   &lt; etc. are known in XML as "entity references."
   
   Also Escape CR as &#x0d; .  While raw CR _is_ allowed in the content
   of an XML element, it has a special meaning -- it means line ending.
   Our input uses LF for for line endings.  Since it also means line ending
   in XML, we just pass it through to our output like it were a regular
   character.

   &#x0d; is known in XML as a "character reference."

   We assume chars[] is is ASCII.  That isn't right -- we should
   handle all valid UTF-8.  Someday, we must do something more complex
   and copy over multibyte characters verbatim.  (The code here could
   erroneously find that e.g. the 2nd byte of a UTF-8 character is a
   CR).
-----------------------------------------------------------------------------*/
    xmlrpc_mem_block * outputP;
    size_t outputSize;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(chars != NULL);

    assertValidUtf8(chars, len);

    /* Note that in UTF-8, any byte that has high bit of zero is a
       character all by itself (every byte of a multi-byte UTF-8 character
       has the high bit set).  Also, the Unicode code points < 128 are
       identical to the ASCII ones.
    */

    outputSize = escapedSize(chars, len);

    outputP = XMLRPC_MEMBLOCK_NEW(char, envP, outputSize);
    if (!envP->fault_occurred) {
        char * p;
        size_t i;
        p = XMLRPC_MEMBLOCK_CONTENTS(char, outputP); /* Start at beginning */

        for (i = 0; i < len; i++) {
            if (chars[i] == '<') {
                memcpy(p, "&lt;", 4);
                p += 4;
            } else if (chars[i] == '>') {
                memcpy(p, "&gt;", 4);
                p += 4;
            } else if (chars[i] == '&') {
                memcpy(p, "&amp;", 5);
                p += 5;
            } else if (chars[i] == '\r') {
                memcpy(p, "&#x0d;", 6);
                p += 6;
            } else {
                /* Either a plain character or a LF line delimiter */
                *p = chars[i];
                p += 1;
            }
        }
        *outputPP = outputP;
        assert(p == XMLRPC_MEMBLOCK_CONTENTS(char, outputP) + outputSize);

        if (envP->fault_occurred)
            XMLRPC_MEMBLOCK_FREE(char, outputP);
    }
}



static void 
serializeUtf8MemBlock(xmlrpc_env *       const envP,
                      xmlrpc_mem_block * const outputP,
                      xmlrpc_mem_block * const inputP) {
/*----------------------------------------------------------------------------
   Append the characters in *inputP to the XML stream in *outputP.

   *inputP contains Unicode characters in UTF-8.

   We assume *inputP ends with a NUL character that marks end of
   string, and we ignore that.  (There might also be NUL characters
   inside the string, though).
-----------------------------------------------------------------------------*/
    xmlrpc_mem_block * escapedP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(outputP != NULL);
    XMLRPC_ASSERT(inputP != NULL);

    escapeForXml(envP,
                 XMLRPC_MEMBLOCK_CONTENTS(const char, inputP),
                 XMLRPC_MEMBLOCK_SIZE(const char, inputP) - 1,
                    /* -1 is for the terminating NUL */
                 &escapedP);
    if (!envP->fault_occurred) {
        const char * const contents =
            XMLRPC_MEMBLOCK_CONTENTS(const char, escapedP);
        size_t const size = XMLRPC_MEMBLOCK_SIZE(char, escapedP);
    
        XMLRPC_MEMBLOCK_APPEND(char, envP, outputP, contents, size);

        XMLRPC_MEMBLOCK_FREE(const char, escapedP);
    }
}



static void 
xmlrpc_serialize_base64_data(xmlrpc_env *       const envP,
                             xmlrpc_mem_block * const output,
                             unsigned char *    const data, 
                             size_t             const len) {
/*----------------------------------------------------------------------------
   Encode the 'len' bytes at 'data' in base64 ASCII and append the result to
   'output'.
-----------------------------------------------------------------------------*/
    xmlrpc_mem_block * encoded;

    encoded = xmlrpc_base64_encode(envP, data, len);
    if (!envP->fault_occurred) {
        unsigned char * const contents =
            XMLRPC_MEMBLOCK_CONTENTS(unsigned char, encoded);
        size_t const size = 
            XMLRPC_MEMBLOCK_SIZE(unsigned char, encoded);
        
        XMLRPC_MEMBLOCK_APPEND(char, envP, output, contents, size);
        
        XMLRPC_MEMBLOCK_FREE(char, encoded);
    }
}



static void
serializeDatetime(xmlrpc_env *       const envP,
                  xmlrpc_mem_block * const outputP,
                  xmlrpc_value *     const valueP) {
/*----------------------------------------------------------------------------
   Add to *outputP the content of a <value> element to represent
   the datetime value *valueP.  I.e.
   "<dateTime.iso8601> ... </dateTime.iso8601>".
-----------------------------------------------------------------------------*/

    addString(envP, outputP, "<dateTime.iso8601>");
    if (!envP->fault_occurred) {
        char dtString[64];

        snprintf(dtString, sizeof(dtString),
                 "%u%02u%02uT%02u:%02u:%02u",
                 valueP->_value.dt.Y,
                 valueP->_value.dt.M,
                 valueP->_value.dt.D,
                 valueP->_value.dt.h,
                 valueP->_value.dt.m,
                 valueP->_value.dt.s);

        if (valueP->_value.dt.u != 0) {
            char usecString[64];
            assert(valueP->_value.dt.u < 1000000);
            snprintf(usecString, sizeof(usecString), ".%06u",
                     valueP->_value.dt.u);
            STRSCAT(dtString, usecString);
        }
        addString(envP, outputP, dtString);

        if (!envP->fault_occurred) {
            addString(envP, outputP, "</dateTime.iso8601>");
        }
    }
}



static void
serializeStructMember(xmlrpc_env *       const envP,
                      xmlrpc_mem_block * const outputP,
                      xmlrpc_value *     const memberKeyP,
                      xmlrpc_value *     const memberValueP,
                      xmlrpc_dialect     const dialect) {
    
    addString(envP, outputP, "<member><name>");

    if (!envP->fault_occurred) {
        serializeUtf8MemBlock(envP, outputP, &memberKeyP->_block);

        if (!envP->fault_occurred) {
            addString(envP, outputP, "</name>"CRLF);

            if (!envP->fault_occurred) {
                xmlrpc_serialize_value2(envP, outputP, memberValueP, dialect);

                if (!envP->fault_occurred) {
                    addString(envP, outputP, "</member>"CRLF);
                }
            }
        }
    }
}



static void 
serializeStruct(xmlrpc_env *       const envP,
                xmlrpc_mem_block * const outputP,
                xmlrpc_value *     const structP,
                xmlrpc_dialect     const dialect) {
/*----------------------------------------------------------------------------
   Add to *outputP the content of a <value> element to represent
   the structure value *valueP.  I.e. "<struct> ... </struct>".
-----------------------------------------------------------------------------*/
    addString(envP, outputP, "<struct>"CRLF);
    if (!envP->fault_occurred) {
        unsigned int const size = xmlrpc_struct_size(envP, structP);
        if (!envP->fault_occurred) {
            unsigned int i;
            for (i = 0; i < size && !envP->fault_occurred; ++i) {
                xmlrpc_value * memberKeyP;
                xmlrpc_value * memberValueP;

                xmlrpc_struct_get_key_and_value(envP, structP, i,
                                                &memberKeyP, &memberValueP);
                if (!envP->fault_occurred) {
                    serializeStructMember(envP, outputP,
                                          memberKeyP, memberValueP, dialect);
                }
            }
            addString(envP, outputP, "</struct>");
        }
    }
}



static void
serializeArray(xmlrpc_env *       const envP,
               xmlrpc_mem_block * const outputP,
               xmlrpc_value *     const valueP,
               xmlrpc_dialect     const dialect) {
/*----------------------------------------------------------------------------
   Add to *outputP the content of a <value> element to represent
   the array value *valueP.  I.e. "<array> ... </array>".
-----------------------------------------------------------------------------*/
    int const size = xmlrpc_array_size(envP, valueP);

    if (!envP->fault_occurred) {
        addString(envP, outputP, "<array><data>"CRLF);
        if (!envP->fault_occurred) {
            int i;
            /* Serialize each item. */
            for (i = 0; i < size && !envP->fault_occurred; ++i) {
                xmlrpc_value * const itemP =
                    xmlrpc_array_get_item(envP, valueP, i);
                if (!envP->fault_occurred) {
                    xmlrpc_serialize_value2(envP, outputP, itemP, dialect);
                    if (!envP->fault_occurred)
                        addString(envP, outputP, CRLF);
                }
            }
        }
    }
    if (!envP->fault_occurred)
        addString(envP, outputP, "</data></array>");
} 



static void
formatValueContent(xmlrpc_env *       const envP,
                   xmlrpc_mem_block * const outputP,
                   xmlrpc_value *     const valueP,
                   xmlrpc_dialect     const dialect) {
/*----------------------------------------------------------------------------
   Add to *outputP the content of a <value> element to represent
   value *valueP.  E.g. "<int>42</int>"
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT_ENV_OK(envP);

    switch (valueP->_type) {
    case XMLRPC_TYPE_INT:
        formatOut(envP, outputP, "<i4>%d</i4>", valueP->_value.i);
        break;

    case XMLRPC_TYPE_I8: {
        const char * const elemName =
            dialect == xmlrpc_dialect_apache ? "ex:i8" : "i8";
        formatOut(envP, outputP, "<%s>%" PRId64 "</%s>",
                  elemName, valueP->_value.i8, elemName);
    } break;

    case XMLRPC_TYPE_BOOL:
        formatOut(envP, outputP, "<boolean>%s</boolean>",
                  valueP->_value.b ? "1" : "0");
        break;

    case XMLRPC_TYPE_DOUBLE: {
        const char * serializedValue;
        xmlrpc_formatFloat(envP, valueP->_value.d, &serializedValue);
        if (!envP->fault_occurred) {
            addString(envP, outputP, "<double>");
            if (!envP->fault_occurred) {
                addString(envP, outputP, serializedValue);
                if (!envP->fault_occurred)
                    addString(envP, outputP, "</double>");
            }
            xmlrpc_strfree(serializedValue);
        }
    } break;

    case XMLRPC_TYPE_DATETIME:
        serializeDatetime(envP, outputP, valueP);
        break;

    case XMLRPC_TYPE_STRING:
        addString(envP, outputP, "<string>");
        if (!envP->fault_occurred) {
            serializeUtf8MemBlock(envP, outputP, &valueP->_block);
            if (!envP->fault_occurred)
                addString(envP, outputP, "</string>");
        }
        break;

    case XMLRPC_TYPE_BASE64: {
        unsigned char * const contents =
            XMLRPC_MEMBLOCK_CONTENTS(unsigned char, &valueP->_block);
        size_t const size =
            XMLRPC_MEMBLOCK_SIZE(unsigned char, &valueP->_block);
        addString(envP, outputP, "<base64>"CRLF);
        if (!envP->fault_occurred) {
            xmlrpc_serialize_base64_data(envP, outputP, contents, size);
            if (!envP->fault_occurred)
                addString(envP, outputP, "</base64>");
        }
    } break;      

    case XMLRPC_TYPE_ARRAY:
        serializeArray(envP, outputP, valueP, dialect);
        break;

    case XMLRPC_TYPE_STRUCT:
        serializeStruct(envP, outputP, valueP, dialect);
        break;

    case XMLRPC_TYPE_C_PTR:
        xmlrpc_faultf(envP, "Tried to serialize a C pointer value.");
        break;

    case XMLRPC_TYPE_NIL: {
        const char * const elemName =
            dialect == xmlrpc_dialect_apache ? "ex:nil" : "nil";
        formatOut(envP, outputP, "<%s/>", elemName);
    } break;

    case XMLRPC_TYPE_DEAD:
        xmlrpc_faultf(envP, "Tried to serialize a dead value.");
        break;

    default:
        xmlrpc_faultf(envP, "Invalid xmlrpc_value type: %d", valueP->_type);
    }
}



void 
xmlrpc_serialize_value2(xmlrpc_env *       const envP,
                        xmlrpc_mem_block * const outputP,
                        xmlrpc_value *     const valueP,
                        xmlrpc_dialect     const dialect) {
/*----------------------------------------------------------------------------
   Generate the XML to represent XML-RPC value 'valueP' in XML-RPC.

   Add it to *outputP.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(outputP != NULL);
    XMLRPC_ASSERT_VALUE_OK(valueP);

    addString(envP, outputP, "<value>");

    if (!envP->fault_occurred) {
        formatValueContent(envP, outputP, valueP, dialect);

        if (!envP->fault_occurred)
            addString(envP, outputP, "</value>");
    }
}



void 
xmlrpc_serialize_value(xmlrpc_env *       const envP,
                       xmlrpc_mem_block * const outputP,
                       xmlrpc_value *     const valueP) {

    xmlrpc_serialize_value2(envP, outputP, valueP, xmlrpc_dialect_i8);
}



void 
xmlrpc_serialize_params2(xmlrpc_env *       const envP,
                         xmlrpc_mem_block * const outputP,
                         xmlrpc_value *     const paramArrayP,
                         xmlrpc_dialect     const dialect) {
/*----------------------------------------------------------------------------
   Serialize the parameter list of an XML-RPC call.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(outputP != NULL);
    XMLRPC_ASSERT_VALUE_OK(paramArrayP);

    addString(envP, outputP, "<params>"CRLF);
    if (!envP->fault_occurred) {
        /* Serialize each parameter. */
        int const paramCount = xmlrpc_array_size(envP, paramArrayP);
        if (!envP->fault_occurred) {
            int paramSeq;
            for (paramSeq = 0;
                 paramSeq < paramCount && !envP->fault_occurred;
                 ++paramSeq) {

                addString(envP, outputP, "<param>");
                if (!envP->fault_occurred) {
                    xmlrpc_value * const itemP =
                        xmlrpc_array_get_item(envP, paramArrayP, paramSeq);
                    if (!envP->fault_occurred) {
                        xmlrpc_serialize_value2(envP, outputP, itemP, dialect);
                        if (!envP->fault_occurred)
                            addString(envP, outputP, "</param>"CRLF);
                    }
                }
            }
        }
    }

    if (!envP->fault_occurred)
        addString(envP, outputP, "</params>"CRLF);
}



void 
xmlrpc_serialize_params(xmlrpc_env *       const envP,
                        xmlrpc_mem_block * const outputP,
                        xmlrpc_value *     const paramArrayP) {
/*----------------------------------------------------------------------------
   Serialize the parameter list of an XML-RPC call in the original
   "i8" dialect.
-----------------------------------------------------------------------------*/
    xmlrpc_serialize_params2(envP, outputP, paramArrayP, xmlrpc_dialect_i8);
}



/*=========================================================================
**  xmlrpc_serialize_call
**=========================================================================
**  Serialize an XML-RPC call.
*/                

void 
xmlrpc_serialize_call2(xmlrpc_env *       const envP,
                       xmlrpc_mem_block * const outputP,
                       const char *       const methodName,
                       xmlrpc_value *     const paramArrayP,
                       xmlrpc_dialect     const dialect) {
/*----------------------------------------------------------------------------
   Serialize an XML-RPC call of method named 'methodName' with parameter
   list *paramArrayP.  Use XML-RPC dialect 'dialect'.

   Append the call XML ot *outputP.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(outputP != NULL);
    XMLRPC_ASSERT(methodName != NULL);
    XMLRPC_ASSERT_VALUE_OK(paramArrayP);
    
    addString(envP, outputP, XML_PROLOGUE);
    if (!envP->fault_occurred) {
        const char * const xmlns =
            dialect == xmlrpc_dialect_apache ? " " XMLNS_APACHE : "";
        formatOut(envP, outputP, "<methodCall%s>"CRLF"<methodName>", xmlns);
        if (!envP->fault_occurred) {
            xmlrpc_mem_block * encodedP;
            escapeForXml(envP, methodName, strlen(methodName), &encodedP);
            if (!envP->fault_occurred) {
                const char * const contents =
                    XMLRPC_MEMBLOCK_CONTENTS(char, encodedP);
                size_t const size = XMLRPC_MEMBLOCK_SIZE(char, encodedP);
                XMLRPC_MEMBLOCK_APPEND(char, envP, outputP, contents, size);
                if (!envP->fault_occurred) {
                    addString(envP, outputP, "</methodName>"CRLF);
                    if (!envP->fault_occurred) {
                        xmlrpc_serialize_params2(envP, outputP, paramArrayP,
                                                 dialect);
                        if (!envP->fault_occurred)
                            addString(envP, outputP, "</methodCall>"CRLF);
                    }
                }
                XMLRPC_MEMBLOCK_FREE(char, encodedP);
            }
        }
    }
}



void 
xmlrpc_serialize_call(xmlrpc_env *       const envP,
                      xmlrpc_mem_block * const outputP,
                      const char *       const methodName,
                      xmlrpc_value *     const paramArrayP) {

    xmlrpc_serialize_call2(envP, outputP, methodName, paramArrayP,
                           xmlrpc_dialect_i8);
}



void 
xmlrpc_serialize_response2(xmlrpc_env *       const envP,
                           xmlrpc_mem_block * const outputP,
                           xmlrpc_value *     const valueP,
                           xmlrpc_dialect     const dialect) {
/*----------------------------------------------------------------------------
  Serialize a result response to an XML-RPC call.

  The result is 'valueP'.

  Add the response XML to *outputP.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(outputP != NULL);
    XMLRPC_ASSERT_VALUE_OK(valueP);

    addString(envP, outputP, XML_PROLOGUE);
    if (!envP->fault_occurred) {
        const char * const xmlns =
            dialect == xmlrpc_dialect_apache ? " " XMLNS_APACHE : "";
        formatOut(envP, outputP,
                  "<methodResponse%s>"CRLF"<params>"CRLF"<param>", xmlns);
        if (!envP->fault_occurred) {
            xmlrpc_serialize_value2(envP, outputP, valueP, dialect);
            if (!envP->fault_occurred) {
                addString(envP, outputP,
                          "</param>"CRLF"</params>"CRLF
                          "</methodResponse>"CRLF);
            }
        }
    }    
}



void 
xmlrpc_serialize_response(xmlrpc_env *       const envP,
                          xmlrpc_mem_block * const outputP,
                          xmlrpc_value *     const valueP) {

    xmlrpc_serialize_response2(envP, outputP, valueP, xmlrpc_dialect_i8);
}



void 
xmlrpc_serialize_fault(xmlrpc_env *       const envP,
                       xmlrpc_mem_block * const outputP,
                       const xmlrpc_env * const faultP) {
/*----------------------------------------------------------------------------
   Serialize a fault response to an XML-RPC call.

   'faultP' is the fault.

   Add the response XML to *outputP.
-----------------------------------------------------------------------------*/
    xmlrpc_value * faultStructP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(outputP != NULL);
    XMLRPC_ASSERT(faultP != NULL);
    XMLRPC_ASSERT(faultP->fault_occurred);

    faultStructP = xmlrpc_build_value(envP, "{s:i,s:s}",
                                      "faultCode",
                                      (xmlrpc_int32) faultP->fault_code,
                                      "faultString", faultP->fault_string);
    if (!envP->fault_occurred) {
        addString(envP, outputP, XML_PROLOGUE);
        if (!envP->fault_occurred) {
            addString(envP, outputP, "<methodResponse>"CRLF"<fault>"CRLF);
            if (!envP->fault_occurred) {
                xmlrpc_serialize_value(envP, outputP, faultStructP);
                if (!envP->fault_occurred) {
                    addString(envP, outputP,
                              CRLF"</fault>"CRLF"</methodResponse>"CRLF);
                }
            }
        }
        xmlrpc_DECREF(faultStructP);
    }
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
