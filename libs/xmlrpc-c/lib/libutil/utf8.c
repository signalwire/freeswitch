/* Copyright (C) 2001 by Eric Kidd. All rights reserved.
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


/*=========================================================================
**  XML-RPC UTF-8 Utilities
**=========================================================================
**  Routines for validating, encoding and decoding UTF-8 data.  We try to
**  be very, very strict about invalid UTF-8 data.
**
**  All of the code in this file assumes that your machine represents
**  wchar_t as a 16-bit (or wider) character containing UCS-2 data.  If this
**  assumption is incorrect, you may need to replace this file.
**
**  For lots of information on Unicode and UTF-8 decoding, see:
**    http://www.cl.cam.ac.uk/~mgk25/unicode.html
*/

#include <assert.h>
#include "int.h"

#include "xmlrpc_config.h"
#include "bool.h"
#include "xmlrpc-c/base.h"

/*=========================================================================
**  Tables and Constants
**=========================================================================
**  We use a variety of tables and constants to help decode and validate
**  UTF-8 data.
*/

static unsigned char utf8SeqLength[256] = {

  /* utf8SeqLength[B] is the number of bytes in a UTF-8 sequence that starts
     with byte B.  Except zero indicates an illegal initial byte.

     Fredrik Lundh's UTF-8 decoder Python 2.0 uses a similar table.  But since
     Python 2.0 has the icky CNRI license, I generated this table from scratch
     and wrote my own decoder.
  */

          /* 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F  */
  /* 0 */    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  /* 1 */    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  /* 2 */    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  /* 3 */    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  /* 4 */    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  /* 5 */    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  /* 6 */    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  /* 7 */    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  /* 8 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  /* 9 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  /* A */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  /* B */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  /* C */    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  /* D */    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  /* E */    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  /* F */    4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 0, 0
};

/* The minimum legal character value for a UTF-8 sequence of the given
** length.  We have to check this to avoid accepting "overlong" UTF-8
** sequences, which use more bytes than necessary to encode a given
** character.  Such sequences are commonly used by evil people to bypass
** filters and security checks.  This table is based on the UTF-8-test.txt
** file by Markus Kuhn <mkuhn@acm.org>. */
static uint32_t const utf8_min_char_for_length[] = {
    0,          /* Length 0: Not used (meaningless) */
    0x0000,     /* Length 1: Not used (special-cased) */
    0x0080,     /* Length 2 */
    0x0800,     /* Length 3 */
    0x00010000, /* Length 4 */
    0x00200000, /* Length 5 */
    0x04000000  /* Length 6 */
};

/* This is the maximum legal 16-byte (UCS-2) character.  Again, this
** information is based on UTF-8-test.txt. */
#define UCS2_MAX_LEGAL_CHARACTER (0xFFFD)

/* First and last UTF-16 surrogate characters.  These are *not* legal UCS-2
** characters--they're used to code for UCS-4 characters when using
** UTF-16.  They should never appear in decoded UTF-8 data!  Again, these
** could hypothetically be used to bypass security measures on some machines.
** Based on UTF-8-test.txt. */
#define UTF16_FIRST_SURROGATE (0xD800)
#define UTF16_LAST_SURROGATE  (0xDFFF)

/* Is the character 'c' a UTF-8 continuation character? */
#define IS_CONTINUATION(c) (((c) & 0xC0) == 0x80)

#define MAX_ENCODED_BYTES (3)
    /* Maximum number of bytes needed to encode in UTF-8 a character
       in the Basic Multilingual Plane.
    */


#if HAVE_UNICODE_WCHAR


static void
validateContinuation(xmlrpc_env * const envP,
                     char         const c) {

    if (!IS_CONTINUATION(c))
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_INVALID_UTF8_ERROR,
            "UTF-8 multibyte sequence contains character 0x%02x, "
            "which does not indicate continuation.", c);
}



static void
validateUtf16(xmlrpc_env * const envP,
              wchar_t      const wc) {

    if (wc > UCS2_MAX_LEGAL_CHARACTER)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_INVALID_UTF8_ERROR,
            "UCS-2 characters > U+FFFD are illegal.  String contains 0x%04x",
            (unsigned)wc);
    else if (UTF16_FIRST_SURROGATE <= wc && wc <= UTF16_LAST_SURROGATE)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_INVALID_UTF8_ERROR,
            "UTF-16 surrogates may not appear in UTF-8 data.  "
            "String contains %04x", (unsigned)wc);
}



/* Microsoft Visual C in debug mode produces code that complains about
   returning an undefined value from xmlrpc_datetime_new_str().  It's a bogus
   complaint, because this function is defined to return nothing meaningful
   those cases.  So we disable the check.
*/
#pragma runtime_checks("u", off)

static void
decodeMultibyte(xmlrpc_env * const envP,
                const char * const utf8_seq,
                size_t       const length,
                wchar_t *    const wcP) {
/*----------------------------------------------------------------------------
   Decode the multibyte UTF-8 sequence which is 'length' characters
   at 'utf8_data'.

   Return the character in UTF-16 format as *wcP.
-----------------------------------------------------------------------------*/
    wchar_t wc;

    assert(utf8_seq[0] & 0x80); /* High bit set: this is multibyte seq */

    switch (length) {
    case 2:
        /* 110xxxxx 10xxxxxx */
        validateContinuation(envP, utf8_seq[1]);

        if (!envP->fault_occurred)
            wc = ((((wchar_t) (utf8_seq[0] & 0x1F)) <<  6) |
                  (((wchar_t) (utf8_seq[1] & 0x3F))));
        break;
                
    case 3:
        /* 1110xxxx 10xxxxxx 10xxxxxx */
        validateContinuation(envP, utf8_seq[1]);
        if (!envP->fault_occurred) {
            validateContinuation(envP, utf8_seq[2]);
            if (!envP->fault_occurred)
                wc = ((((wchar_t) (utf8_seq[0] & 0x0F)) << 12) |
                      (((wchar_t) (utf8_seq[1] & 0x3F)) <<  6) |
                      (((wchar_t) (utf8_seq[2] & 0x3F))));
        }
        break;

    case 4:
        /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
    case 5:
        /* 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
    case 6:
        /* 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx */
        /* This would require more than 16 bits in UTF-16, so
           it can't be represented in UCS-2, so it's beyond
           our capability.  Characters in the BMP fit in 16
           bits.
        */
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_INVALID_UTF8_ERROR,
            "UTF-8 string contains a character not in the "
            "Basic Multilingual Plane (first byte 0x%02x)",
            utf8_seq[0]);
        break;

    default:
        xmlrpc_faultf(envP,
                      "Internal error: Impossible UTF-8 sequence length %u",
                      (unsigned)length);
    }

    if (!envP->fault_occurred)
        validateUtf16(envP, wc);

    if (!envP->fault_occurred)
        if ((uint32_t)wc < utf8_min_char_for_length[length])
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INVALID_UTF8_ERROR,
                "Overlong UTF-8 sequence not allowed");

    *wcP = wc;
}

#pragma runtime_checks("u", restore)



static void 
decodeUtf8(xmlrpc_env * const envP,
           const char * const utf8_data,
           size_t       const utf8_len,
           wchar_t *    const ioBuff,
           size_t *     const outBuffLenP) {
/*----------------------------------------------------------------------------
  Decode to UCS-2 (or validate as UTF-8 that can be decoded to UCS-2)
  a UTF-8 string.  To validate, set ioBuff and outBuffLenP to NULL.
  To decode, allocate a sufficiently large buffer, pass it as ioBuff,
  and pass a pointer as as outBuffLenP.  The data will be written to
  the buffer, and the length to outBuffLenP.

  We assume that wchar_t holds a single UCS-2 character in native-endian
  byte ordering.
-----------------------------------------------------------------------------*/
    size_t utf8Cursor;
    size_t outPos;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(utf8_data);
    XMLRPC_ASSERT((!ioBuff && !outBuffLenP) || (ioBuff && outBuffLenP));

    for (utf8Cursor = 0, outPos = 0;
         utf8Cursor < utf8_len && !envP->fault_occurred;
        ) {

        char const init = utf8_data[utf8Cursor];
            /* Initial byte of the UTF-8 sequence */

        wchar_t wc;

        if ((init & 0x80) == 0x00) {
            /* Convert ASCII character to wide character. */
            wc = init;
            ++utf8Cursor;
        } else {
            /* Look up the length of this UTF-8 sequence. */
            size_t const length = utf8SeqLength[(unsigned char) init];

            if (length == 0)
                xmlrpc_env_set_fault_formatted(
                    envP, XMLRPC_INVALID_UTF8_ERROR,
                    "Unrecognized UTF-8 initial byte value 0x%02x", init);
            else {
                /* Make sure we have enough bytes to convert. */
                if (utf8Cursor + length > utf8_len) {
                    xmlrpc_env_set_fault_formatted(
                        envP, XMLRPC_INVALID_UTF8_ERROR,
                        "Invalid UTF-8 sequence indicates a %u-byte sequence "
                        "when only %u bytes are left in the string",
                        (unsigned)length, (unsigned)(utf8_len - utf8Cursor));
                } else {
                    decodeMultibyte(envP, &utf8_data[utf8Cursor], length, &wc);
                    
                    /* Advance to the end of the sequence. */
                    utf8Cursor += length;
                }
            }
        }

        if (!envP->fault_occurred) {
            /* If we have a buffer, write our character to it. */
            if (ioBuff)
                ioBuff[outPos++] = wc;
        }
    }

    if (outBuffLenP)
        *outBuffLenP = envP->fault_occurred ? 0 : outPos;
}



xmlrpc_mem_block *
xmlrpc_utf8_to_wcs(xmlrpc_env * const envP,
                   const char * const utf8_data,
                   size_t       const utf8_len) {
/*----------------------------------------------------------------------------
  Decode UTF-8 string to a "wide character string".  This function
  returns an xmlrpc_mem_block with an element type of wchar_t.  Don't
  try to intepret the block in a bytewise fashion--it won't work in
  any useful or portable fashion.

   For backward compatibility, we return a meaningful value even when we
   fail.  We return NULL when we fail.
-----------------------------------------------------------------------------*/
    xmlrpc_mem_block * wcsP;
    size_t wcs_length;

    /* Allocate a memory block large enough to hold any possible output.
       We assume that each byte of the input may decode to a whcar_t.
    */
    wcsP = XMLRPC_MEMBLOCK_NEW(wchar_t, envP, utf8_len);
    if (!envP->fault_occurred) {
        /* Decode the UTF-8 data. */
        decodeUtf8(envP, utf8_data, utf8_len,
                   XMLRPC_MEMBLOCK_CONTENTS(wchar_t, wcsP),
                   &wcs_length);
        if (!envP->fault_occurred) {
            /* We can't have overrun our buffer. */
            XMLRPC_ASSERT(wcs_length <= utf8_len);

            /* Correct the length of the memory block. */
            XMLRPC_MEMBLOCK_RESIZE(wchar_t, envP, wcsP, wcs_length);
        }
        if (envP->fault_occurred)
            XMLRPC_MEMBLOCK_FREE(wchar_t, wcsP);
    }
    if (envP->fault_occurred)
        return NULL;
    else
        return wcsP;
}



xmlrpc_mem_block *
xmlrpc_wcs_to_utf8(xmlrpc_env *    const envP,
                   const wchar_t * const wcs_data,
                   size_t          const wcs_len) {
/*----------------------------------------------------------------------------
   Encode a "wide character string" as UTF-8.

   For backward compatibility, we return a meaningful value even when we
   fail.  We return NULL when we fail.
-----------------------------------------------------------------------------*/
    size_t const estimate = wcs_len * MAX_ENCODED_BYTES;
        /* Our conservative estimate of how big the output will be;
           i.e. we know it won't be larger than this.  For the estimate,
           we assume that every wchar might encode to the maximum length.
        */
    xmlrpc_mem_block * utf8P;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(wcs_data);

    utf8P = XMLRPC_MEMBLOCK_NEW(char, envP, estimate);
    if (!envP->fault_occurred) {
        unsigned char * const buffer =
            XMLRPC_MEMBLOCK_CONTENTS(unsigned char, utf8P);
        size_t bytesUsed;
        size_t i;

        bytesUsed = 0;
        for (i = 0; i < wcs_len && !envP->fault_occurred; ++i) {
            wchar_t const wc = wcs_data[i];
            if (wc <= 0x007F)
                buffer[bytesUsed++] = wc & 0x7F;
            else if (wc <= 0x07FF) {
                /* 110xxxxx 10xxxxxx */
                buffer[bytesUsed++] = 0xC0 | (wc >> 6);
                buffer[bytesUsed++] = 0x80 | (wc & 0x3F);
            } else if (wc <= 0xFFFF) {
                /* 1110xxxx 10xxxxxx 10xxxxxx */
                buffer[bytesUsed++] = 0xE0 | (wc >> 12);
                buffer[bytesUsed++] = 0x80 | ((wc >> 6) & 0x3F);
                buffer[bytesUsed++] = 0x80 | (wc & 0x3F);
            } else
                xmlrpc_faultf(envP, 
                              "Don't know how to encode UCS-4 characters yet");
        }
        if (!envP->fault_occurred) {
            XMLRPC_ASSERT(bytesUsed <= estimate);

            XMLRPC_MEMBLOCK_RESIZE(char, envP, utf8P, bytesUsed);
        }
        if (envP->fault_occurred)
            XMLRPC_MEMBLOCK_FREE(char, utf8P);
    }

    if (envP->fault_occurred)
        return NULL;
    else 
        return utf8P;
}



#else /* HAVE_UNICODE_WCHAR */

xmlrpc_mem_block *
xmlrpc_utf8_to_wcs(xmlrpc_env * const envP,
                   const char * const utf8_data ATTR_UNUSED,
                   size_t       const utf8_len ATTR_UNUSED) {

    xmlrpc_faultf(envP, "INTERNAL ERROR: xmlrpc_utf8_to_wcs() called "
                  "on a system that doesn't do Unicode!");

    return NULL;
}
#endif /* HAVE_UNICODE_WCHAR */


void
xmlrpc_force_to_utf8(char * const buffer) {
/*----------------------------------------------------------------------------
   Force the contents of 'buffer' to be valid UTF-8, any way possible.
   The buffer ends with a NUL character, and the mutation does not make
   it longer.

   The most common reason for a string that's supposed to be UTF-8 not
   to be UTF-8 is that it was supposed to be ASCII but instead
   includes garbage with the high bit on (ASCII characters always have
   the high bit off), or maybe a primitive 8-bit ASCII extension.
   Therefore, we force it to UTF-8 by replacing some bytes that have
   the high bit set with DEL (0x7F).  That would leave the other
   characters meaningful.
-----------------------------------------------------------------------------*/
    char * p;

    for (p = &buffer[0]; *p;) {
        unsigned int const length = utf8SeqLength[(unsigned char) *p];

        bool forceDel;
        uint32_t decoded;

        forceDel = false;  /* initial value */

        switch (length) {
        case 1:
            /* One-byte UTF-8 characters are easy.  */
            decoded = *p;
            break;
        case 2:
            /* 110xxxxx 10xxxxxx */
            if (!*(p+1) || !(*p+2))
                forceDel = true;
            else if (!IS_CONTINUATION(*(p+1)))
                forceDel = true;
            else
                decoded =
                    ((uint32_t)(*(p+0) & 0x1F) << 6) |
                    ((uint32_t)(*(p+1) & 0x3F) << 0);
            break;
        case 3:
            /* 1110xxxx 10xxxxxx 10xxxxxx */
            if (!*(p+1) || !(*p+2) || !(*p+3))
                forceDel = true;
            else if (!IS_CONTINUATION(*(p+1)) || !IS_CONTINUATION(*(p+2)))
                forceDel = true;
            else
                decoded =
                    ((uint32_t)(*(p+0) & 0x0F) << 12) |
                    ((uint32_t)(*(p+1) & 0x3F) <<  6) |
                    ((uint32_t)(*(p+2) & 0x3F) <<  0);
            break;
        default:
            forceDel = true;
        }

        if (!forceDel) {
            if (decoded > UCS2_MAX_LEGAL_CHARACTER)
                forceDel = true;
            else if (UTF16_FIRST_SURROGATE <= decoded &&
                     decoded <= UTF16_LAST_SURROGATE)
                forceDel = true;
            else if (decoded < utf8_min_char_for_length[length])
                forceDel = true;
        }

        if (forceDel) {
            /* Not a valid UTF-8 character, so replace the first byte
               with a nice simple ASCII DEL.
            */
            *p = 0x7F;
            p += 1;
        } else
            p += length;
    }
}



void
xmlrpc_force_to_xml_chars(char * const buffer) {
/*----------------------------------------------------------------------------
   Modify 'buffer' so that it contains nothing but valid XML
   characters.  The buffer ends with a NUL character, and the mutation
   does not make it longer.

   Note that the valid characters in an XML document are all Unicode
   codepoints except the ASCII control characters, plus CR, LF, and
   Tab.

   We change all non-XML characters to DEL (0x7F).

   Assume input is valid UTF-8.
-----------------------------------------------------------------------------*/
    char * p;

    for (p = &buffer[0]; *p;) {
        unsigned int const length = utf8SeqLength[(unsigned char) *p];

        if (length == 1) {
            if (*p < 0x20 && *p != '\r' && *p != '\n' && *p != '\t')
                /* Not valid XML.  Force to DEL */
                *p = 0x7f;
        } else {
            /* We assume here that all other UTF-8 characters are
               valid XML, but it's apparently not actually true.
            */
        }

        {
            unsigned int i;
            /* Advance to next UTF-8 character */
            for (i = 0; i < length && *p; ++i)
                ++p;
        }
    }
}



void 
xmlrpc_validate_utf8(xmlrpc_env * const envP,
                     const char * const utf8_data,
                     size_t       const utf8_len) {
/*----------------------------------------------------------------------------
   Validate that a string is valid UTF-8.
-----------------------------------------------------------------------------*/
    xmlrpc_env env;

    xmlrpc_env_init(&env);

#if HAVE_UNICODE_WCHAR
    decodeUtf8(&env, utf8_data, utf8_len, NULL, NULL);
#else
    /* We don't have a convenient way to validate, so we just fake it and
       call it valid.
    */
#endif

    if (env.fault_occurred) {
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_INVALID_UTF8_ERROR,
            "%" XMLRPC_PRId64 "-byte "
            "supposed UTF-8 string is not valid UTF-8.  %s",
            (XMLRPC_INT64)utf8_len, env.fault_string);
    }
    xmlrpc_env_clean(&env);
}
