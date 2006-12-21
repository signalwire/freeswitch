/*============================================================================
                         xmlrpc_client_int.h
==============================================================================
  This header file defines the interface between modules inside
  xmlrpc-c.

  Use this in addition to xmlrpc.h, which defines the external
  interface.

  Copyright information is at the end of the file.
============================================================================*/


#ifndef  XMLRPC_INT_H_INCLUDED
#define  XMLRPC_INT_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


struct _xmlrpc_value {
    xmlrpc_type _type;
    int _refcount;

    /* Certain data types store their data directly in the xmlrpc_value. */
    union {
        xmlrpc_int32 i;
        xmlrpc_bool b;
        double d;
        /* time_t t */
        void *c_ptr;
    } _value;
    
    /* Other data types use a memory block.

       For a string, this is the characters of the string in UTF-8, plus
       a NUL added to the end.
    */
    xmlrpc_mem_block _block;

#ifdef HAVE_UNICODE_WCHAR
    xmlrpc_mem_block *_wcs_block;
        /* This is a copy of the string value in _block, but in UTF-16
           instead of UTF-8.  This member is not always present.  If NULL,
           it is not present.

           We keep this copy for convenience.  The value is totally
           redundant with _block.

           This member is always NULL when the data type is not string.
        */
#endif
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
    unsigned char key_hash;
    xmlrpc_value *key;
    xmlrpc_value *value;
} _struct_member;


void
xmlrpc_createXmlrpcValue(xmlrpc_env *    const envP,
                         xmlrpc_value ** const valPP);

const char *
xmlrpc_typeName(xmlrpc_type const type);


struct _xmlrpc_registry {
    int _introspection_enabled;
    xmlrpc_value *_methods;
    xmlrpc_value *_default_method;
    xmlrpc_value *_preinvoke_method;
};


/* When we deallocate a pointer in a struct, we often replace it with
** this and throw in a few assertions here and there. */
#define XMLRPC_BAD_POINTER ((void*) 0xDEADBEEF)


void
xmlrpc_traceXml(const char * const label, 
                const char * const xml,
                unsigned int const xmlLength);

void
xmlrpc_destroyStruct(xmlrpc_value * const structP);

void
xmlrpc_destroyArrayContents(xmlrpc_value * const arrayP);

const char * 
xmlrpc_makePrintable(const char * const input);

const char *
xmlrpc_makePrintableChar(char const input);


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

#ifdef HAVE_UNICODE_WCHAR
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
#endif /* __cplusplus */

#endif
