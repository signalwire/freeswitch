/*============================================================================
                         base_int.h
==============================================================================
  This header file defines the interface between modules inside
  xmlrpc-c.

  Use this in addition to xmlrpc.h, which defines the external
  interface.

  Copyright information is at the end of the file.
============================================================================*/


#ifndef  XMLRPC_C_BASE_INT_H_INCLUDED
#define  XMLRPC_C_BASE_INT_H_INCLUDED

#include "xmlrpc_config.h"
#include "bool.h"
#include "int.h"

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/util_int.h>

#ifdef __cplusplus
extern "C" {
#endif


struct _xmlrpc_value {
    xmlrpc_type _type;
    int _refcount;

    /* Certain data types store their data directly in the xmlrpc_value. */
    union {
        xmlrpc_int32 i;
        xmlrpc_int64 i8;
        xmlrpc_bool b;
        double d;
        /* time_t t */
        void * c_ptr;
    } _value;
    
    /* Other data types use a memory block.

       For a string, this is the characters of the lines of the string
       in UTF-8, with lines delimited by either CR, LF, or CRLF, plus
       a NUL added to the end.  The characters of the lines may be any
       character representable in UTF-8, even the ones that are not
       legal XML (XML doesn't allow ASCII control characters except
       tab, CR, LF).  But note that a line can't contain CR or LF
       because that would form a line delimiter.  To disambiguate:
       CRLF together is always one line delimiter.
       
       This format for string is quite convenient because it is also
       the format of that part of an XML document which is the
       contents of a <string> element (except of course that for the
       non-XML characters, we have to stretch the definition of XML).

       For base64, this is bytes of the byte string, directly.

       For datetime, this is in the same format as the contents of
       a <dateTime.iso8601> XML element.  That really ought to be changed
       to time_t some day.
    */
    xmlrpc_mem_block _block;

    xmlrpc_mem_block *_wcs_block;
        /* This is a copy of the string value in _block, but in UTF-16
           instead of UTF-8.  This member is not always present.  If NULL,
           it is not present.

           We keep this copy for convenience.  The value is totally
           redundant with _block.

           This member is always NULL when the data type is not string.

           This member is always NULL on a system that does not have
           Unicode wchar functions.
        */
};

#define XMLRPC_ASSERT_VALUE_OK(val) \
    XMLRPC_ASSERT((val) != NULL && (val)->_type != XMLRPC_TYPE_DEAD)

/* A handy type-checking routine. */
#define XMLRPC_TYPE_CHECK(env,v,t) \
    do \
        if ((v)->_type != (t)) \
            XMLRPC_FAIL(env, XMLRPC_TYPE_ERROR, "Expected " #t); \
    while (0)


typedef struct {
    uint32_t keyHash;
    xmlrpc_value * key;
    xmlrpc_value * value;
} _struct_member;


void
xmlrpc_createXmlrpcValue(xmlrpc_env *    const envP,
                         xmlrpc_value ** const valPP);

const char *
xmlrpc_typeName(xmlrpc_type const type);

void
xmlrpc_traceXml(const char * const label, 
                const char * const xml,
                unsigned int const xmlLength);

void
xmlrpc_destroyString(xmlrpc_value * const stringP);

void
xmlrpc_destroyStruct(xmlrpc_value * const structP);

void
xmlrpc_destroyArrayContents(xmlrpc_value * const arrayP);

/*----------------------------------------------------------------------------
   The following are for use by the legacy xmlrpc_parse_value().  They don't
   do proper memory management, so they aren't appropriate for general use,
   but there are old users that do xmlrpc_parse_value() and compensate for
   the memory management, so we have to continue to offer this style of
   memory management.

   In particular, the functions that return xmlrpc_values don't increment
   the reference count, and the functions that return strings don't allocate
   new memory for them.
-----------------------------------------------------------------------------*/

void
xmlrpc_read_datetime_str_old(xmlrpc_env *         const envP,
                             const xmlrpc_value * const valueP,
                             const char **        const stringValueP);

void
xmlrpc_read_string_old(xmlrpc_env *         const envP,
                       const xmlrpc_value * const valueP,
                       const char **        const stringValueP);

void
xmlrpc_read_string_lp_old(xmlrpc_env *         const envP,
                          const xmlrpc_value * const valueP,
                          size_t *             const lengthP,
                          const char **        const stringValueP);

#if XMLRPC_HAVE_WCHAR
void
xmlrpc_read_string_w_old(xmlrpc_env *     const envP,
                         xmlrpc_value *   const valueP,
                         const wchar_t ** const stringValueP);

void
xmlrpc_read_string_w_lp_old(xmlrpc_env *     const envP,
                            xmlrpc_value *   const valueP,
                            size_t *         const lengthP,
                            const wchar_t ** const stringValueP);
#endif

void
xmlrpc_read_base64_old(xmlrpc_env *           const envP,
                       const xmlrpc_value *   const valueP,
                       size_t *               const lengthP,
                       const unsigned char ** const byteStringValueP);


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

#ifdef __cplusplus
}
#endif

#endif
