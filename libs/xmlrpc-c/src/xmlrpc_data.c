/* Copyright information is at end of file */

#include "xmlrpc_config.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "bool.h"
#include "mallocvar.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"



static void
destroyCptr(xmlrpc_value * const valueP) {

    if (valueP->_value.cptr.dtor)
        valueP->_value.cptr.dtor(valueP->_value.cptr.dtorContext,
                                 valueP->_value.cptr.objectP);
}



static void
destroyValue(xmlrpc_value * const valueP) {

    /* First, we need to destroy this value's contents, if any. */
    switch (valueP->_type) {
    case XMLRPC_TYPE_INT:
        break;
        
    case XMLRPC_TYPE_BOOL:
        break;

    case XMLRPC_TYPE_DOUBLE:
        break;

    case XMLRPC_TYPE_DATETIME:
        xmlrpc_destroyDatetime(valueP);
        break;

    case XMLRPC_TYPE_STRING:
        xmlrpc_destroyString(valueP);
        break;
        
    case XMLRPC_TYPE_BASE64:
        xmlrpc_mem_block_clean(&valueP->_block);
        break;

    case XMLRPC_TYPE_ARRAY:
        xmlrpc_destroyArrayContents(valueP);
        break;
        
    case XMLRPC_TYPE_STRUCT:
        xmlrpc_destroyStruct(valueP);
        break;

    case XMLRPC_TYPE_C_PTR:
        destroyCptr(valueP);
        break;

    case XMLRPC_TYPE_NIL:
        break;

    case XMLRPC_TYPE_I8:
        break;

    case XMLRPC_TYPE_DEAD:
        XMLRPC_ASSERT(false); /* Can't happen, per entry conditions */

    default:
        XMLRPC_ASSERT(false); /* There are no other possible values */
    }

    /* Next, we mark this value as invalid, to help catch refcount
        ** errors. */
    valueP->_type = XMLRPC_TYPE_DEAD;

    /* Finally, we destroy the value itself. */
    free(valueP);
}



/*=========================================================================
**  Reference Counting
**=========================================================================
**  Some simple reference-counting code. The xmlrpc_DECREF routine is in
**  charge of destroying values when their reference count equals zero.
*/

void 
xmlrpc_INCREF (xmlrpc_value * const valueP) {

    XMLRPC_ASSERT_VALUE_OK(valueP);
    XMLRPC_ASSERT(valueP->_refcount > 0);
    
    ++valueP->_refcount;
}



void 
xmlrpc_DECREF (xmlrpc_value * const valueP) {

    XMLRPC_ASSERT_VALUE_OK(valueP);
    XMLRPC_ASSERT(valueP->_refcount > 0);
    XMLRPC_ASSERT(valueP->_type != XMLRPC_TYPE_DEAD);

    valueP->_refcount--;

    /* If we have no more refs, we need to deallocate this value. */
    if (valueP->_refcount == 0)
        destroyValue(valueP);
}



/*=========================================================================
    Utiltiies
=========================================================================*/

const char *
xmlrpc_type_name(xmlrpc_type const type) {

    switch (type) {

    case XMLRPC_TYPE_INT:      return "INT";
    case XMLRPC_TYPE_BOOL:     return "BOOL";
    case XMLRPC_TYPE_DOUBLE:   return "DOUBLE";
    case XMLRPC_TYPE_DATETIME: return "DATETIME";
    case XMLRPC_TYPE_STRING:   return "STRING";
    case XMLRPC_TYPE_BASE64:   return "BASE64";
    case XMLRPC_TYPE_ARRAY:    return "ARRAY";
    case XMLRPC_TYPE_STRUCT:   return "STRUCT";
    case XMLRPC_TYPE_C_PTR:    return "C_PTR";
    case XMLRPC_TYPE_NIL:      return "NIL";
    case XMLRPC_TYPE_I8:       return "I8";
    case XMLRPC_TYPE_DEAD:     return "DEAD";
    default:                   return "???";

    }
}



static void
validateType(xmlrpc_env *         const envP,
             const xmlrpc_value * const valueP,
             xmlrpc_type          const expectedType) {

    if (valueP->_type != expectedType) {
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_TYPE_ERROR, "Value of type %s supplied where "
            "type %s was expected.", 
            xmlrpc_type_name(valueP->_type), xmlrpc_type_name(expectedType));
    }
}



/*=========================================================================
    Extracting XML-RPC value
===========================================================================
  These routines extract XML-RPC values into ordinary C data types.

  For array and struct values, see the separates files xmlrpc_array.c
  and xmlrpc_struct.c.
=========================================================================*/

void 
xmlrpc_read_int(xmlrpc_env *         const envP,
                const xmlrpc_value * const valueP,
                xmlrpc_int32 *       const intValueP) {

    validateType(envP, valueP, XMLRPC_TYPE_INT);
    if (!envP->fault_occurred)
        *intValueP = valueP->_value.i;
}



void
xmlrpc_read_bool(xmlrpc_env *         const envP,
                 const xmlrpc_value * const valueP,
                 xmlrpc_bool *        const boolValueP) {

    validateType(envP, valueP, XMLRPC_TYPE_BOOL);
    if (!envP->fault_occurred)
        *boolValueP = valueP->_value.b;
}



void
xmlrpc_read_double(xmlrpc_env *         const envP,
                   const xmlrpc_value * const valueP,
                   xmlrpc_double *      const doubleValueP) {
    
    validateType(envP, valueP, XMLRPC_TYPE_DOUBLE);
    if (!envP->fault_occurred)
        *doubleValueP = valueP->_value.d;

}



/* datetime stuff is in xmlrpc_datetime.c */

/* string stuff is in xmlrpc_string.c */



void
xmlrpc_read_base64(xmlrpc_env *           const envP,
                   const xmlrpc_value *   const valueP,
                   size_t *               const lengthP,
                   const unsigned char ** const byteStringValueP) {

    validateType(envP, valueP, XMLRPC_TYPE_BASE64);
    if (!envP->fault_occurred) {
        size_t const size = 
            XMLRPC_MEMBLOCK_SIZE(char, &valueP->_block);
        const char * const contents = 
            XMLRPC_MEMBLOCK_CONTENTS(char, &valueP->_block);

        char * byteStringValue;

        byteStringValue = malloc(size);
        if (byteStringValue == NULL)
            xmlrpc_faultf(envP,
                          "Unable to allocate %u bytes for byte string.",
                          (unsigned)size);
        else {
            memcpy(byteStringValue, contents, size);
            *byteStringValueP = (const unsigned char *)byteStringValue;
            *lengthP = size;
        }
    }
}



void
xmlrpc_read_base64_old(xmlrpc_env *           const envP,
                       const xmlrpc_value *   const valueP,
                       size_t *               const lengthP,
                       const unsigned char ** const byteStringValueP) {

    validateType(envP, valueP, XMLRPC_TYPE_BASE64);
    if (!envP->fault_occurred) {
        *lengthP =
            XMLRPC_MEMBLOCK_SIZE(char, &valueP->_block);
        *byteStringValueP = (const unsigned char *)
            XMLRPC_MEMBLOCK_CONTENTS(char, &valueP->_block);
    }
}



void
xmlrpc_read_base64_size(xmlrpc_env *           const envP,
                        const xmlrpc_value *   const valueP,
                        size_t *               const lengthP) {

    validateType(envP, valueP, XMLRPC_TYPE_BASE64);
    if (!envP->fault_occurred)
        *lengthP = XMLRPC_MEMBLOCK_SIZE(char, &valueP->_block);
}



void
xmlrpc_read_cptr(xmlrpc_env *         const envP,
                 const xmlrpc_value * const valueP,
                 void **              const ptrValueP) {

    validateType(envP, valueP, XMLRPC_TYPE_C_PTR);
    if (!envP->fault_occurred)
        *ptrValueP = valueP->_value.cptr.objectP;
}



void
xmlrpc_read_nil(xmlrpc_env *   const envP,
                xmlrpc_value * const valueP) {
/*----------------------------------------------------------------------------
   Read out the value of a nil value.  It doesn't have one, of course, so
   this is essentially a no-op.  But it does validate the type and is
   necessary to match all the other types.
-----------------------------------------------------------------------------*/
    validateType(envP, valueP, XMLRPC_TYPE_NIL);
}



void 
xmlrpc_read_i8(xmlrpc_env *         const envP,
               const xmlrpc_value * const valueP,
               xmlrpc_int64 *       const intValueP) {

    validateType(envP, valueP, XMLRPC_TYPE_I8);
    if (!envP->fault_occurred)
        *intValueP = valueP->_value.i8;
}



xmlrpc_type xmlrpc_value_type (xmlrpc_value* const value)
{
    XMLRPC_ASSERT_VALUE_OK(value);
    return value->_type;
}



void
xmlrpc_createXmlrpcValue(xmlrpc_env *    const envP,
                         xmlrpc_value ** const valPP) {
/*----------------------------------------------------------------------------
   Create a blank xmlrpc_value to be filled in.

   Set the reference count to 1.
-----------------------------------------------------------------------------*/
    xmlrpc_value * valP;

    MALLOCVAR(valP);
    if (!valP)
        xmlrpc_env_set_fault(envP, XMLRPC_INTERNAL_ERROR,
                             "Could not allocate memory for xmlrpc_value");
    else
        valP->_refcount = 1;

    *valPP = valP;
}



xmlrpc_value *
xmlrpc_int_new(xmlrpc_env * const envP, 
               xmlrpc_int32 const value) {

    xmlrpc_value * valP;

    xmlrpc_createXmlrpcValue(envP, &valP);

    if (!envP->fault_occurred) {
        valP->_type    = XMLRPC_TYPE_INT;
        valP->_value.i = value;
    }
    return valP;
}



xmlrpc_value *
xmlrpc_i8_new(xmlrpc_env * const envP, 
              xmlrpc_int64 const value) {

    xmlrpc_value * valP;

    xmlrpc_createXmlrpcValue(envP, &valP);

    if (!envP->fault_occurred) {
        valP->_type     = XMLRPC_TYPE_I8;
        valP->_value.i8 = value;
    }
    return valP;
}



xmlrpc_value *
xmlrpc_bool_new(xmlrpc_env * const envP, 
                xmlrpc_bool  const value) {

    xmlrpc_value * valP;

    xmlrpc_createXmlrpcValue(envP, &valP);

    if (!envP->fault_occurred) {
        valP->_type = XMLRPC_TYPE_BOOL;
        valP->_value.b = value;
    }
    return valP;
}



xmlrpc_value *
xmlrpc_double_new(xmlrpc_env * const envP, 
                  double       const value) {

    xmlrpc_value * valP;

    xmlrpc_createXmlrpcValue(envP, &valP);

    if (!envP->fault_occurred) {
        valP->_type = XMLRPC_TYPE_DOUBLE;
        valP->_value.d = value;
    }
    return valP;
}



xmlrpc_value *
xmlrpc_base64_new(xmlrpc_env *          const envP, 
                  size_t                const length,
                  const unsigned char * const value) {

    xmlrpc_value * valP;

    xmlrpc_createXmlrpcValue(envP, &valP);

    if (!envP->fault_occurred) {
        valP->_type = XMLRPC_TYPE_BASE64;

        xmlrpc_mem_block_init(envP, &valP->_block, length);
        if (!envP->fault_occurred) {
            char * const contents = 
                xmlrpc_mem_block_contents(&valP->_block);
            memcpy(contents, value, length);
        }
        if (envP->fault_occurred)
            free(valP);
    }
    return valP;
}



/* array stuff is in xmlrpc_array.c */



xmlrpc_value *
xmlrpc_cptr_new(xmlrpc_env *    const envP,
                void *          const value) {

    return xmlrpc_cptr_new_dtor(envP, value, NULL, NULL);
}



xmlrpc_value *
xmlrpc_cptr_new_dtor(xmlrpc_env *        const envP,
                     void *              const value,
                     xmlrpc_cptr_dtor_fn const dtor,
                     void *              const dtorContext) {

    xmlrpc_value * valP;

    xmlrpc_createXmlrpcValue(envP, &valP);

    if (!envP->fault_occurred) {
        valP->_type = XMLRPC_TYPE_C_PTR;
        valP->_value.cptr.objectP     = value;
        valP->_value.cptr.dtor        = dtor;
        valP->_value.cptr.dtorContext = dtorContext;
    }
    return valP;
}



xmlrpc_value *
xmlrpc_nil_new(xmlrpc_env *    const envP) {
    xmlrpc_value * valP;

    xmlrpc_createXmlrpcValue(envP, &valP);
    if (!envP->fault_occurred)
        valP->_type = XMLRPC_TYPE_NIL;

    return valP;
}



/* Copyright (C) 2001 by First Peer, Inc. All rights reserved.
** Copyright (C) 2001 by Eric Kidd. All rights reserved.
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
