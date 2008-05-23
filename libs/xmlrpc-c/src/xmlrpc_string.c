/*=============================================================================
                              xmlrpc_string
===============================================================================
  Routines for the "string" type of xmlrpc_value.

  By Bryan Henderson.

  Contributed to the public domain by its author.
=============================================================================*/

#include "xmlrpc_config.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "bool.h"
#include "mallocvar.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"



void
xmlrpc_destroyString(xmlrpc_value * const valueP) {

    if (valueP->_wcs_block)
        xmlrpc_mem_block_free(valueP->_wcs_block);

    xmlrpc_mem_block_clean(&valueP->_block);
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

    for (i = 0; i < len && !envP->fault_occurred; ++i)
        if (contents[i] == '\0')
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_TYPE_ERROR, 
                "String must not contain NUL characters");
}



#if HAVE_UNICODE_WCHAR

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
validateStringType(xmlrpc_env *         const envP,
                   const xmlrpc_value * const valueP) {

    if (valueP->_type != XMLRPC_TYPE_STRING) {
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_TYPE_ERROR, "Value of type %s supplied where "
            "string type was expected.", 
            xmlrpc_type_name(valueP->_type));
    }
}



static void
accessStringValue(xmlrpc_env *         const envP,
                  const xmlrpc_value * const valueP,
                  size_t *             const lengthP,
                  const char **        const contentsP) {
    
    validateStringType(envP, valueP);
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
   Read the value of an XML-RPC string as an ASCIIZ string, with
   LF for line delimiters.

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
            
        MALLOCARRAY(stringValue, length + 1);
        if (stringValue == NULL)
            xmlrpc_faultf(envP, "Unable to allocate space "
                          "for %u-character string", length);
        else {
            memcpy(stringValue, contents, length);
            stringValue[length] = '\0';

            *stringValueP = stringValue;
        }
    }
}



static unsigned int
lineDelimCount(const char * const start,
               const char * const end) {
    
    unsigned int count;
    const char * p;

    for (p = start, count = 0; p < end; ) {
        const char * const nlPos = memchr(p, '\n', end-p);
        if (nlPos) {
            ++count;
            p = nlPos + 1;
        } else
            p = end;
    }

    return count;
}



static void
copyAndConvertLfToCrlf(xmlrpc_env *  const envP,
                       size_t        const srcLen,
                       const char *  const src,
                       size_t *      const dstLenP,
                       const char ** const dstP) {
    
    const char * const srcEnd = src + srcLen;
    unsigned int const nLineDelim = lineDelimCount(src, srcEnd);
    size_t const dstLen = srcLen + nLineDelim;
    char * dst;

    MALLOCARRAY(dst, dstLen + 1);
    if (dst == NULL)
        xmlrpc_faultf(envP, "Unable to allocate space "
                      "for %u-character string", dstLen + 1);
    else {
        const char * p;  /* source pointer */
        char * q;        /* destination pointer */

        for (p = &src[0], q = &dst[0]; p < srcEnd; ++p) {
            if (*p == '\n')
                *q++ = '\r';

            *q++ = *p;
        }
        XMLRPC_ASSERT(q == dst + dstLen);

        *q = '\0';

        *dstP    = dst;
        *dstLenP = dstLen;
    }
}



void
xmlrpc_read_string_crlf(xmlrpc_env *         const envP,
                        const xmlrpc_value * const valueP,
                        const char **        const stringValueP) {
/*----------------------------------------------------------------------------
   Same as xmlrpc_read_string(), but return CRLF instead of LF for
   line delimiters.
-----------------------------------------------------------------------------*/
    size_t length;
    const char * contents;

    accessStringValue(envP, valueP, &length, &contents);

    if (!envP->fault_occurred) {
        size_t stringLen;
        
        copyAndConvertLfToCrlf(envP, length, contents,
                               &stringLen, stringValueP);
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

    validateStringType(envP, valueP);
    if (!envP->fault_occurred) {
        unsigned int const size = 
            XMLRPC_MEMBLOCK_SIZE(char, &valueP->_block);
        const char * const contents = 
            XMLRPC_MEMBLOCK_CONTENTS(char, &valueP->_block);

        char * stringValue;

        stringValue = malloc(size);
        if (stringValue == NULL)
            xmlrpc_faultf(envP, "Unable to allocate %u bytes for string.",
                          size);
        else {
            memcpy(stringValue, contents, size);
            *stringValueP = stringValue;
            *lengthP = size - 1;  /* Size includes terminating NUL */
        }
    }
}



void
xmlrpc_read_string_lp_crlf(xmlrpc_env *         const envP,
                           const xmlrpc_value * const valueP,
                           size_t *             const lengthP,
                           const char **        const stringValueP) {

    validateStringType(envP, valueP);
    if (!envP->fault_occurred) {
        unsigned int const size = 
            XMLRPC_MEMBLOCK_SIZE(char, &valueP->_block); /* Includes NUL */
        const char * const contents = 
            XMLRPC_MEMBLOCK_CONTENTS(char, &valueP->_block);

        copyAndConvertLfToCrlf(envP, size-1, contents,
                               lengthP, stringValueP);
    }
}



void
xmlrpc_read_string_lp_old(xmlrpc_env *         const envP,
                          const xmlrpc_value * const valueP,
                          size_t *             const lengthP,
                          const char **        const stringValueP) {

    validateStringType(envP, valueP);
    if (!envP->fault_occurred) {
        *lengthP =      XMLRPC_MEMBLOCK_SIZE(char, &valueP->_block) - 1;
        *stringValueP = XMLRPC_MEMBLOCK_CONTENTS(char, &valueP->_block);
    }
}



static __inline__ void
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



#if HAVE_UNICODE_WCHAR

static void
accessStringValueW(xmlrpc_env *     const envP,
                   xmlrpc_value *   const valueP,
                   size_t *         const lengthP,
                   const wchar_t ** const stringValueP) {

    validateStringType(envP, valueP);
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
        MALLOCARRAY(stringValue, length + 1);
        if (stringValue == NULL)
            xmlrpc_faultf(envP, "Unable to allocate space for %u-byte string", 
                          length);
        else {
            memcpy(stringValue, wcontents, length * sizeof(wchar_t));
            stringValue[length] = '\0';
            
            *stringValueP = stringValue;
        }
    }
}



static unsigned int
lineDelimCountW(const wchar_t * const start,
                const wchar_t * const end) {
    
    unsigned int count;
    const wchar_t * p;

    count = 0;
    p = start;

    while (p && p < end) {
        /* We used to use memchr(), but Windows doesn't have it */
        p = wcsstr(p, L"\n");
        if (p && p < end) {
            ++count; /* count the newline */
            ++p;  /* skip the newline */
        }
    }

    return count;
}



static void
wCopyAndConvertLfToCrlf(xmlrpc_env *     const envP,
                        size_t           const srcLen,
                        const wchar_t *  const src,
                        size_t *         const dstLenP,
                        const wchar_t ** const dstP) {

    const wchar_t * const srcEnd = src + srcLen;
    unsigned int const nLineDelim = lineDelimCountW(src, srcEnd);
    size_t const dstLen = srcLen + nLineDelim;
    wchar_t * dst;

    MALLOCARRAY(dst, dstLen + 1);
    if (dst == NULL)
        xmlrpc_faultf(envP, "Unable to allocate space "
                      "for %u-character string", dstLen + 1);
    else {
        const wchar_t * p;  /* source pointer */
        wchar_t * q;        /* destination pointer */

        for (p = &src[0], q = &dst[0]; p < srcEnd; ++p) {
            if (*p == '\n')
                *q++ = '\r';

            *q++ = *p;
        }
        XMLRPC_ASSERT(q == dst + dstLen);

        *q = '\0';

        *dstP    = dst;
        *dstLenP = dstLen;
    }
}



void
xmlrpc_read_string_w_crlf(xmlrpc_env *     const envP,
                          xmlrpc_value *   const valueP,
                          const wchar_t ** const stringValueP) {

    size_t size;
    const wchar_t * contents;
    
    accessStringValueW(envP, valueP, &size, &contents);

    if (!envP->fault_occurred) {
        size_t stringLen;

        wCopyAndConvertLfToCrlf(envP, size, contents,
                                &stringLen, stringValueP);
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

    validateStringType(envP, valueP);
    if (!envP->fault_occurred) {
        setupWcsBlock(envP, valueP);

        if (!envP->fault_occurred) {
            wchar_t * const wcontents = 
                XMLRPC_MEMBLOCK_CONTENTS(wchar_t, valueP->_wcs_block);
            size_t const size = 
                XMLRPC_MEMBLOCK_SIZE(wchar_t, valueP->_wcs_block);

            wchar_t * stringValue;

            MALLOCARRAY(stringValue, size); 
            if (stringValue == NULL)
                xmlrpc_faultf(envP,
                              "Unable to allocate space for %u-byte string", 
                              (unsigned int)size);
            else {
                memcpy(stringValue, wcontents, size * sizeof(wchar_t));
                
                *lengthP      = size - 1; /* size includes terminating NUL */
                *stringValueP = stringValue;
            }
        }
    }
}



void
xmlrpc_read_string_w_lp_crlf(xmlrpc_env *     const envP,
                             xmlrpc_value *   const valueP,
                             size_t *         const lengthP,
                             const wchar_t ** const stringValueP) {

    validateStringType(envP, valueP);
    if (!envP->fault_occurred) {
        setupWcsBlock(envP, valueP);

        if (!envP->fault_occurred) {
            size_t const size = 
                XMLRPC_MEMBLOCK_SIZE(wchar_t, valueP->_wcs_block);
            wchar_t * const wcontents = 
                XMLRPC_MEMBLOCK_CONTENTS(wchar_t, valueP->_wcs_block);

            wCopyAndConvertLfToCrlf(envP, size-1, wcontents,
                                   lengthP, stringValueP);
        }
    }
}



void
xmlrpc_read_string_w_lp_old(xmlrpc_env *     const envP,
                            xmlrpc_value *   const valueP,
                            size_t *         const lengthP,
                            const wchar_t ** const stringValueP) {

    validateStringType(envP, valueP);
    if (!envP->fault_occurred) {
        setupWcsBlock(envP, valueP);

        if (!envP->fault_occurred) {
            wchar_t * const wcontents = 
                XMLRPC_MEMBLOCK_CONTENTS(wchar_t, valueP->_wcs_block);
            size_t const size = 
                XMLRPC_MEMBLOCK_SIZE(wchar_t, valueP->_wcs_block);
            
            *lengthP      = size - 1;  /* size includes terminating NUL */
            *stringValueP = wcontents;
        }
    }
}
#endif   /* HAVE_UNICODE_WCHAR */



static void
validateUtf(xmlrpc_env * const envP,
            const char * const value,
            size_t       const length) {

#if HAVE_UNICODE_WCHAR
    xmlrpc_validate_utf8(envP, value, length);
#endif
}



static void
copyLines(xmlrpc_env *       const envP,
          const char *       const src,
          size_t             const srcLen,
          xmlrpc_mem_block * const dstP) {
/*----------------------------------------------------------------------------
   Copy the string 'src', 'srcLen' characters long, into 'dst', where
   'dst' is the internal representation of string xmlrpc_value contents,
   and 'src' has lines separated by LF, CR, and/or CRLF.

   Note that the source format differs from the destination format in
   that in the destination format, lines are separated only by newline
   (LF).

   It is tempting to believe that if we just put the user's line
   delimiters in the xmlrpc_value here (i.e. where user has CRLF, the
   xmlrpc_value also has CRLF), the user's line delimiters would go
   all the way across to the XML-RPC partner.  But that won't work,
   because the XML processor on the other side will, following Section
   2.11 of the XML spec, normalize all line endings to LF anyhow.  So
   then you might ask, why do we bother to do all the work to convert
   them here?  Because: besides just being logically cleaner, this way
   xmlrpc_read_string() gets the proper value -- the same one the
   XML-RPC partner would see.
-----------------------------------------------------------------------------*/
    /* Destination format is sometimes smaller than source (because
       CRLF turns into LF), but never smaller.  So we allocate
       destination space equal to source size (plus one for
       terminating NUL), but don't necessarily use it all.
    */

    /* To convert LF, CR, and CRLF to LF, all we have to do is
       copy everything up to a CR verbatim, then insert an LF and
       skip the CR and any following LF, and repeat.
    */

    XMLRPC_MEMBLOCK_INIT(char, envP, dstP, srcLen + 1);

    if (!envP->fault_occurred) {
        const char * const srcEnd = &src[srcLen];
        char * const contents = XMLRPC_MEMBLOCK_CONTENTS(char, dstP);

        const char * srcCursor;
        char * dstCursor;

        for (srcCursor = &src[0], dstCursor = &contents[0];
             srcCursor < srcEnd;) {

            char * const crPos = memchr(srcCursor, '\r', srcEnd - srcCursor);

            if (crPos) {
                size_t const copyLen = crPos - srcCursor;
                memcpy(dstCursor, srcCursor, copyLen);
                srcCursor += copyLen;
                dstCursor += copyLen;

                *(dstCursor++) = '\n';
            
                XMLRPC_ASSERT(*srcCursor == '\r');
                ++srcCursor;  /* Move past CR */
                if (*srcCursor == '\n')
                    ++srcCursor;  /* Move past LF */
            } else {
                size_t const remainingLen = srcEnd - srcCursor;
                memcpy(dstCursor, srcCursor, remainingLen);
                srcCursor += remainingLen;
                dstCursor += remainingLen;
            }
        }

        *dstCursor++ = '\0';

        XMLRPC_ASSERT((unsigned)(dstCursor - &contents[0]) <= srcLen + 1);

        XMLRPC_MEMBLOCK_RESIZE(char, envP, dstP, dstCursor - &contents[0]);
    }
}



static void
copySimple(xmlrpc_env *       const envP,
           const char *       const src,
           size_t             const srcLen,
           xmlrpc_mem_block * const dstP) {
/*----------------------------------------------------------------------------
   Copy the string 'src', 'srcLen' characters long, into 'dst', where
   'dst' is the internal representation of string xmlrpc_value contents,
   and 'src', conveniently enough, is in the exact same format.

   To wit, 'src' has lines separated by LFs only -- no CR or CRLF.
-----------------------------------------------------------------------------*/
    XMLRPC_MEMBLOCK_INIT(char, envP, dstP, srcLen + 1);
    if (!envP->fault_occurred) {
        char * const contents = XMLRPC_MEMBLOCK_CONTENTS(char, dstP);
        
        memcpy(contents, src, srcLen);
        contents[srcLen] = '\0';
    }
}



enum crTreatment { CR_IS_LINEDELIM, CR_IS_CHAR };

static xmlrpc_value *
stringNew(xmlrpc_env *     const envP, 
          size_t           const length,
          const char *     const value,
          enum crTreatment const crTreatment) {

    xmlrpc_value * valP;

    validateUtf(envP, value, length);

    if (!envP->fault_occurred) {
        xmlrpc_createXmlrpcValue(envP, &valP);

        if (!envP->fault_occurred) {
            valP->_type = XMLRPC_TYPE_STRING;
            valP->_wcs_block = NULL;

            /* Note that copyLines() works for strings with no CRs, but
               it's slower.
            */
            if (memchr(value, '\r', length) && crTreatment == CR_IS_LINEDELIM)
                copyLines(envP, value, length, &valP->_block);
            else
                copySimple(envP, value, length, &valP->_block);

            if (envP->fault_occurred)
                free(valP);
        }
    }
    return valP;
}



xmlrpc_value *
xmlrpc_string_new_lp(xmlrpc_env * const envP, 
                     size_t       const length,
                     const char * const value) {

    return stringNew(envP, length, value, CR_IS_LINEDELIM);
}



xmlrpc_value *
xmlrpc_string_new_lp_cr(xmlrpc_env * const envP, 
                        size_t       const length,
                        const char * const value) {

    return stringNew(envP, length, value, CR_IS_CHAR);
}



xmlrpc_value *
xmlrpc_string_new(xmlrpc_env * const envP,
                  const char * const value) {
    
    return stringNew(envP, strlen(value), value, CR_IS_LINEDELIM);
}



xmlrpc_value *
xmlrpc_string_new_cr(xmlrpc_env * const envP,
                     const char * const value) {

    return stringNew(envP, strlen(value), value, CR_IS_CHAR);
}



xmlrpc_value *
xmlrpc_string_new_va(xmlrpc_env * const envP,
                     const char * const format,
                     va_list            args) {

    const char * formattedString;
    xmlrpc_value * retvalP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(format != NULL);
    
    xmlrpc_vasprintf(&formattedString, format, args);

    if (formattedString == xmlrpc_strsol) {
        xmlrpc_faultf(envP, "Out of memory building formatted string");
        retvalP = NULL;  /* defeat compiler warning */
    } else
        retvalP = xmlrpc_string_new(envP, formattedString);

    xmlrpc_strfree(formattedString);

    return retvalP;
}



xmlrpc_value *
xmlrpc_string_new_f(xmlrpc_env * const envP,
                    const char * const format,
                    ...) {

    va_list args;
    xmlrpc_value * retval;
    
    va_start(args, format);
    
    retval = xmlrpc_string_new_va(envP, format, args);
    
    va_end(args);

    return retval;
}



#if HAVE_UNICODE_WCHAR

static xmlrpc_value *
stringWNew(xmlrpc_env *     const envP, 
           size_t           const length,
           const wchar_t *  const value,
           enum crTreatment const crTreatment) {

    xmlrpc_value * valP;
    xmlrpc_mem_block * utf8P;

    valP = NULL;  /* defeat compiler warning */

    utf8P = xmlrpc_wcs_to_utf8(envP, value, length);
    if (!envP->fault_occurred) {
        char * const utf8_value = XMLRPC_MEMBLOCK_CONTENTS(char, utf8P);
        size_t const utf8_len   = XMLRPC_MEMBLOCK_SIZE(char, utf8P);
        
        if (!envP->fault_occurred) {
            valP = stringNew(envP, utf8_len, utf8_value, crTreatment);

            XMLRPC_MEMBLOCK_FREE(char, utf8P);
        }
    }
    return valP;
}



xmlrpc_value *
xmlrpc_string_w_new_lp(xmlrpc_env *    const envP, 
                       size_t          const length,
                       const wchar_t * const value) {

    return stringWNew(envP, length, value, CR_IS_LINEDELIM);
}




xmlrpc_value *
xmlrpc_string_w_new_lp_cr(xmlrpc_env *    const envP, 
                          size_t          const length,
                          const wchar_t * const value) {

    return stringWNew(envP, length, value, CR_IS_CHAR);
}




xmlrpc_value *
xmlrpc_string_w_new(xmlrpc_env *    const envP,
                    const wchar_t * const value) {

    return stringWNew(envP, wcslen(value), value, CR_IS_LINEDELIM);
}



xmlrpc_value *
xmlrpc_string_w_new_cr(xmlrpc_env *    const envP,
                       const wchar_t * const value) {

    return stringWNew(envP, wcslen(value), value, CR_IS_CHAR);
}

#endif   /* HAVE_UNICODE_WCHAR */
