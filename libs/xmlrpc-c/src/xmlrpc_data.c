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
        xmlrpc_mem_block_clean(&valueP->_block);
        break;

    case XMLRPC_TYPE_STRING:
#ifdef HAVE_UNICODE_WCHAR
        if (valueP->_wcs_block)
            xmlrpc_mem_block_free(valueP->_wcs_block);
#endif /* HAVE_UNICODE_WCHAR */
        xmlrpc_mem_block_clean(&valueP->_block);
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
        break;

    case XMLRPC_TYPE_NIL:
        break;

    case XMLRPC_TYPE_DEAD:
        XMLRPC_ASSERT(FALSE); /* Can't happen, per entry conditions */

    default:
        XMLRPC_ASSERT(FALSE); /* There are no other possible values */
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
xmlrpc_typeName(xmlrpc_type const type) {

    switch(type) {

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
    case XMLRPC_TYPE_DEAD:     return "DEAD";
    default:                   return "???";
    }
}



static void
verifyNoNulls(xmlrpc_env * const envP,
              const char * const contents,
              unsigned int const len) {
/*----------------------------------------------------------------------------
   Verify that the character array 'contents', which is 'len' bytes long,
   does not contain any NUL characters, which means it can be made into
   a passable ASCIIZ string just by adding a terminating NUL.

   Fail if the array contains a NUL.
-----------------------------------------------------------------------------*/
    unsigned int i;

    for (i = 0; i < len && !envP->fault_occurred; i++)
        if (contents[i] == '\0')
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_TYPE_ERROR, 
                "String must not contain NUL characters");
}



#ifdef HAVE_UNICODE_WCHAR

static void
verifyNoNullsW(xmlrpc_env *    const envP,
               const wchar_t * const contents,
               unsigned int    const len) {
/*----------------------------------------------------------------------------
   Same as verifyNoNulls(), but for wide characters.
-----------------------------------------------------------------------------*/
    unsigned int i;

    for (i = 0; i < len && !envP->fault_occurred; i++)
        if (contents[i] == '\0')
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_TYPE_ERROR, 
                "String must not contain NUL characters");
}
#endif



static void
validateType(xmlrpc_env *         const envP,
             const xmlrpc_value * const valueP,
             xmlrpc_type          const expectedType) {

    if (valueP->_type != expectedType) {
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_TYPE_ERROR, "Value of type %s supplied where "
            "type %s was expected.", 
            xmlrpc_typeName(valueP->_type), xmlrpc_typeName(expectedType));
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

static void
accessStringValue(xmlrpc_env *         const envP,
                  const xmlrpc_value * const valueP,
                  size_t *             const lengthP,
                  const char **        const contentsP) {
    
    validateType(envP, valueP, XMLRPC_TYPE_STRING);
    if (!envP->fault_occurred) {
        unsigned int const size = 
            XMLRPC_MEMBLOCK_SIZE(char, &valueP->_block);
        const char * const contents = 
            XMLRPC_MEMBLOCK_CONTENTS(char, &valueP->_block);
        unsigned int const len = size - 1;
            /* The memblock has a null character added to the end */

        verifyNoNulls(envP, contents, len);

        *lengthP = len;
        *contentsP = contents;
    }
}
             


void
xmlrpc_read_string(xmlrpc_env *         const envP,
                   const xmlrpc_value * const valueP,
                   const char **        const stringValueP) {
/*----------------------------------------------------------------------------
   Read the value of an XML-RPC string as an ASCIIZ string.

   Return the string in newly malloc'ed storage that Caller must free.

   Fail if the string contains null characters (which means it wasn't
   really a string, but XML-RPC doesn't seem to understand what a string
   is, and such values are possible).
-----------------------------------------------------------------------------*/
    size_t length;
    const char * contents;

    accessStringValue(envP, valueP, &length, &contents);

    if (!envP->fault_occurred) {
        char * stringValue;
            
        stringValue = malloc(length+1);
        if (stringValue == NULL)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INTERNAL_ERROR, "Unable to allocate space "
                "for %u-character string", length);
        else {
            memcpy(stringValue, contents, length);
            stringValue[length] = '\0';

            *stringValueP = stringValue;
        }
    }
}



void
xmlrpc_read_string_old(xmlrpc_env *         const envP,
                       const xmlrpc_value * const valueP,
                       const char **        const stringValueP) {

    size_t length;
    accessStringValue(envP, valueP, &length, stringValueP);
}



void
xmlrpc_read_string_lp(xmlrpc_env *         const envP,
                      const xmlrpc_value * const valueP,
                      size_t *             const lengthP,
                      const char **        const stringValueP) {

    validateType(envP, valueP, XMLRPC_TYPE_STRING);
    if (!envP->fault_occurred) {
        unsigned int const size = 
            XMLRPC_MEMBLOCK_SIZE(char, &valueP->_block);
        const char * const contents = 
            XMLRPC_MEMBLOCK_CONTENTS(char, &valueP->_block);

        char * stringValue;

        stringValue = malloc(size);
        if (stringValue == NULL)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INTERNAL_ERROR, "Unable to allocate %u bytes "
                "for string.", size);
        else {
            memcpy(stringValue, contents, size);
            *stringValueP = stringValue;
            *lengthP = size - 1;  /* Size includes terminating NUL */
        }
    }
}



void
xmlrpc_read_string_lp_old(xmlrpc_env *         const envP,
                          const xmlrpc_value * const valueP,
                          size_t *             const lengthP,
                          const char **        const stringValueP) {

    validateType(envP, valueP, XMLRPC_TYPE_STRING);
    if (!envP->fault_occurred) {
        *lengthP =      XMLRPC_MEMBLOCK_SIZE(char, &valueP->_block) - 1;
        *stringValueP = XMLRPC_MEMBLOCK_CONTENTS(char, &valueP->_block);
    }
}



#ifdef HAVE_UNICODE_WCHAR
static void
setupWcsBlock(xmlrpc_env *   const envP,
              xmlrpc_value * const valueP) {
/*----------------------------------------------------------------------------
   Add a wcs block (wchar_t string) to the indicated xmlrpc_value if it
   doesn't have one already.
-----------------------------------------------------------------------------*/
    if (!valueP->_wcs_block) {
        char * const contents = 
            XMLRPC_MEMBLOCK_CONTENTS(char, &valueP->_block);
        size_t const len = 
            XMLRPC_MEMBLOCK_SIZE(char, &valueP->_block) - 1;
        valueP->_wcs_block = 
            xmlrpc_utf8_to_wcs(envP, contents, len + 1);
    }
}



static void
accessStringValueW(xmlrpc_env *     const envP,
                   xmlrpc_value *   const valueP,
                   size_t *         const lengthP,
                   const wchar_t ** const stringValueP) {

    validateType(envP, valueP, XMLRPC_TYPE_STRING);
    if (!envP->fault_occurred) {
        setupWcsBlock(envP, valueP);

        if (!envP->fault_occurred) {
            wchar_t * const wcontents = 
                XMLRPC_MEMBLOCK_CONTENTS(wchar_t, valueP->_wcs_block);
            size_t const len = 
                XMLRPC_MEMBLOCK_SIZE(wchar_t, valueP->_wcs_block) - 1;
            
            verifyNoNullsW(envP, wcontents, len);

            *lengthP = len;
            *stringValueP = wcontents;
        }
    }
}


              
void
xmlrpc_read_string_w(xmlrpc_env *     const envP,
                     xmlrpc_value *   const valueP,
                     const wchar_t ** const stringValueP) {

    size_t length;
    const wchar_t * wcontents;
    
    accessStringValueW(envP, valueP, &length, &wcontents);

    if (!envP->fault_occurred) {
        wchar_t * stringValue;
        stringValue = malloc((length + 1) * sizeof(wchar_t));
        if (stringValue == NULL)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INTERNAL_ERROR, 
                "Unable to allocate space for %u-byte string", 
                length);
        else {
            memcpy(stringValue, wcontents, length * sizeof(wchar_t));
            stringValue[length] = '\0';
            
            *stringValueP = stringValue;
        }
    }
}



void
xmlrpc_read_string_w_old(xmlrpc_env *     const envP,
                         xmlrpc_value *   const valueP,
                         const wchar_t ** const stringValueP) {

    size_t length;

    accessStringValueW(envP, valueP, &length, stringValueP);
}



void
xmlrpc_read_string_w_lp(xmlrpc_env *     const envP,
                        xmlrpc_value *   const valueP,
                        size_t *         const lengthP,
                        const wchar_t ** const stringValueP) {

    validateType(envP, valueP, XMLRPC_TYPE_STRING);
    if (!envP->fault_occurred) {
        setupWcsBlock(envP, valueP);

        if (!envP->fault_occurred) {
            wchar_t * const wcontents = 
                XMLRPC_MEMBLOCK_CONTENTS(wchar_t, valueP->_wcs_block);
            size_t const size = 
                XMLRPC_MEMBLOCK_SIZE(wchar_t, valueP->_wcs_block);

            wchar_t * stringValue;
            
            stringValue = malloc(size * sizeof(wchar_t));
            if (stringValue == NULL)
                xmlrpc_env_set_fault_formatted(
                    envP, XMLRPC_INTERNAL_ERROR, 
                    "Unable to allocate space for %u-byte string", 
                    size);
            else {
                memcpy(stringValue, wcontents, size * sizeof(wchar_t));
                
                *lengthP      = size - 1; /* size includes terminating NUL */
                *stringValueP = stringValue;
            }
        }
    }
}



void
xmlrpc_read_string_w_lp_old(xmlrpc_env *     const envP,
                            xmlrpc_value *   const valueP,
                            size_t *         const lengthP,
                            const wchar_t ** const stringValueP) {

    validateType(envP, valueP, XMLRPC_TYPE_STRING);
    if (!envP->fault_occurred) {
        setupWcsBlock(envP, valueP);

        if (!envP->fault_occurred) {
            wchar_t * const wcontents = 
                XMLRPC_MEMBLOCK_CONTENTS(wchar_t, valueP->_wcs_block);
            size_t const size = 
                XMLRPC_MEMBLOCK_SIZE(wchar_t, valueP->_wcs_block);
            
            *lengthP      = size - 1;  /* size includes terminatnig NUL */
            *stringValueP = wcontents;
        }
    }
}
#endif



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
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INTERNAL_ERROR, "Unable to allocate %u bytes "
                "for byte string.", size);
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
xmlrpc_read_cptr(xmlrpc_env *         const envP,
                 const xmlrpc_value * const valueP,
                 void **              const ptrValueP) {

    validateType(envP, valueP, XMLRPC_TYPE_C_PTR);
    if (!envP->fault_occurred)
        *ptrValueP = valueP->_value.c_ptr;
}



xmlrpc_type xmlrpc_value_type (xmlrpc_value* value)
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



#ifdef HAVE_UNICODE_WCHAR
#define MAKE_WCS_BLOCK_NULL(val) ((val)->_wcs_block = NULL)
#else
#define MAKE_WCS_BLOCK_NULL(val) while (0) do {};
#endif



xmlrpc_value *
xmlrpc_string_new_lp(xmlrpc_env * const envP, 
                     size_t       const length,
                     const char * const value) {

    xmlrpc_value * valP;

    xmlrpc_createXmlrpcValue(envP, &valP);

    if (!envP->fault_occurred) {
        valP->_type = XMLRPC_TYPE_STRING;
        MAKE_WCS_BLOCK_NULL(valP);
        XMLRPC_MEMBLOCK_INIT(char, envP, &valP->_block, length + 1);
        if (!envP->fault_occurred) {
            char * const contents =
                XMLRPC_MEMBLOCK_CONTENTS(char, &valP->_block);
            memcpy(contents, value, length);
            contents[length] = '\0';
        }
        if (envP->fault_occurred)
            free(valP);
    }
    return valP;
}



xmlrpc_value *
xmlrpc_string_new(xmlrpc_env * const envP,
                  const char * const value) {

    return xmlrpc_string_new_lp(envP, strlen(value), value);
}


#ifdef HAVE_UNICODE_WCHAR
xmlrpc_value *
xmlrpc_string_w_new_lp(xmlrpc_env *    const envP, 
                       size_t          const length,
                       const wchar_t * const value) {

    xmlrpc_value * valP;

    /* Initialize our XML-RPC value. */
    xmlrpc_createXmlrpcValue(envP, &valP);

    if (!envP->fault_occurred) {
        valP->_type = XMLRPC_TYPE_STRING;

        /* Build our wchar_t block first. */
        valP->_wcs_block =
            XMLRPC_MEMBLOCK_NEW(wchar_t, envP, length + 1);
        if (!envP->fault_occurred) {
            wchar_t * const wcs_contents =
                XMLRPC_MEMBLOCK_CONTENTS(wchar_t, valP->_wcs_block);

            xmlrpc_mem_block * utf8_block;

            memcpy(wcs_contents, value, length * sizeof(wchar_t));
            wcs_contents[length] = '\0';
    
            /* Convert the wcs block to UTF-8. */
            utf8_block = xmlrpc_wcs_to_utf8(envP, wcs_contents, length + 1);
            if (!envP->fault_occurred) {
                char * const utf8_contents =
                    XMLRPC_MEMBLOCK_CONTENTS(char, utf8_block);
                size_t const utf8_len = XMLRPC_MEMBLOCK_SIZE(char, utf8_block);

                /* XXX - We need an extra memcopy to initialize _block. */
                XMLRPC_MEMBLOCK_INIT(char, envP, &valP->_block, utf8_len);
                if (!envP->fault_occurred) {
                    char * contents;
                    contents = XMLRPC_MEMBLOCK_CONTENTS(char, &valP->_block);
                    memcpy(contents, utf8_contents, utf8_len);
                }
                XMLRPC_MEMBLOCK_FREE(char, utf8_block);
            }
            if (envP->fault_occurred)
                XMLRPC_MEMBLOCK_FREE(wchar_t, valP->_wcs_block);
        }
        if (envP->fault_occurred)
            free(valP);
    }
    return valP;
}



xmlrpc_value *
xmlrpc_string_w_new(xmlrpc_env *    const envP,
                    const wchar_t * const value) {
    return xmlrpc_string_w_new_lp(envP, wcslen(value), value);
}
#endif

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

    xmlrpc_value * valP;

    xmlrpc_createXmlrpcValue(envP, &valP);

    if (!envP->fault_occurred) {
        valP->_type = XMLRPC_TYPE_C_PTR;
        valP->_value.c_ptr = value;
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
