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

/* The number of bytes in a UTF-8 sequence starting with the character used
** as the array index.  A zero entry indicates an illegal initial byte.
** This table was generated using a Perl script and information from the
** UTF-8 standard.
**
** Fredrik Lundh's UTF-8 decoder Python 2.0 uses a similar table.  But
** since Python 2.0 has the icky CNRI license, I regenerated this
** table from scratch and wrote my own decoder. */
static unsigned char utf8_seq_length[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 0, 0
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
decode_utf8(xmlrpc_env * const envP,
            const char * const utf8_data,
            size_t       const utf8_len,
            wchar_t *    const ioBuff,
            size_t *     const outBuffLenP) {
/*----------------------------------------------------------------------------
  Decode to UCS-2 (or validates as UTF-8 that can be decoded to UCS-2)
  a UTF-8 string.  To validate, set ioBuff and outBuffLenP to NULL.
  To decode, allocate a sufficiently large buffer, pass it as ioBuff,
  and pass a pointer as as outBuffLenP.  The data will be written to
  the buffer, and the length to outBuffLenP.

  We assume that wchar_t holds a single UCS-2 character in native-endian
  byte ordering.
-----------------------------------------------------------------------------*/
    size_t i, length, out_pos;
    char init, con1, con2;
    wchar_t wc;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(utf8_data);
    XMLRPC_ASSERT((!ioBuff && !outBuffLenP) ||
                  (ioBuff && outBuffLenP));

    /* Suppress GCC warning about possibly undefined variable. */
    wc = 0;

    i = 0;
    out_pos = 0;
    while (i < utf8_len) {
        init = utf8_data[i];
        if ((init & 0x80) == 0x00) {
            /* Convert ASCII character to wide character. */
            wc = init;
            i++;
        } else {
            /* Look up the length of this UTF-8 sequence. */
            length = utf8_seq_length[(unsigned char) init];
            
            /* Check to make sure we have enough bytes to convert. */
            if (i + length > utf8_len)
                XMLRPC_FAIL(envP, XMLRPC_INVALID_UTF8_ERROR,
                            "Truncated UTF-8 sequence");
            
            /* Decode a multibyte UTF-8 sequence. */
            switch (length) {
            case 0:
                XMLRPC_FAIL(envP, XMLRPC_INVALID_UTF8_ERROR,
                            "Invalid UTF-8 initial byte");
                
            case 2:
                /* 110xxxxx 10xxxxxx */
                con1 = utf8_data[i+1];
                if (!IS_CONTINUATION(con1))
                    XMLRPC_FAIL(envP, XMLRPC_INVALID_UTF8_ERROR,
                                "UTF-8 sequence too short");
                wc = ((((wchar_t) (init & 0x1F)) <<  6) |
                      (((wchar_t) (con1 & 0x3F))));
                break;
                
            case 3:
                /* 1110xxxx 10xxxxxx 10xxxxxx */
                con1 = utf8_data[i+1];
                con2 = utf8_data[i+2];
                if (!IS_CONTINUATION(con1) || !IS_CONTINUATION(con2))
                    XMLRPC_FAIL(envP, XMLRPC_INVALID_UTF8_ERROR,
                                "UTF-8 sequence too short");
                wc = ((((wchar_t) (init & 0x0F)) << 12) |
                      (((wchar_t) (con1 & 0x3F)) <<  6) |
                      (((wchar_t) (con2 & 0x3F))));
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
                    "Basic Multilingual Plane (first byte %08x)",
                    init);
                goto cleanup;
                
            default:
                XMLRPC_ASSERT("Error in UTF-8 decoder tables");
            }
            
            /* Advance to the end of the sequence. */
            i += length;
            
            /* Check for illegal UCS-2 characters. */
            if (wc > UCS2_MAX_LEGAL_CHARACTER)
                XMLRPC_FAIL(envP, XMLRPC_INVALID_UTF8_ERROR,
                            "UCS-2 characters > U+FFFD are illegal");
            
            /* Check for UTF-16 surrogates. */
            if (UTF16_FIRST_SURROGATE <= wc && wc <= UTF16_LAST_SURROGATE)
                XMLRPC_FAIL(envP, XMLRPC_INVALID_UTF8_ERROR,
                            "UTF-16 surrogates may not appear in UTF-8 data");
            
            /* Check for overlong sequences. */
            if ((uint32_t)wc < utf8_min_char_for_length[length])
                XMLRPC_FAIL(envP, XMLRPC_INVALID_UTF8_ERROR,
                            "Overlong UTF-8 sequence not allowed");
        }
        
        /* If we have a buffer, write our character to it. */
        if (ioBuff) {
            ioBuff[out_pos++] = wc;
        }
    }
    
    /* Record the number of characters we found. */
    if (outBuffLenP)
        *outBuffLenP = out_pos;
    
            cleanup:
    if (envP->fault_occurred) {
        if (outBuffLenP)
            *outBuffLenP = 0;
    }
}



void 
xmlrpc_validate_utf8(xmlrpc_env * const env,
                     const char * const utf8_data,
                     size_t       const utf8_len) {
/*----------------------------------------------------------------------------
   Validate that a string is valid UTF-8.
-----------------------------------------------------------------------------*/

    decode_utf8(env, utf8_data, utf8_len, NULL, NULL);
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
        decode_utf8(envP, utf8_data, utf8_len,
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
        unsigned char * const buffer = XMLRPC_MEMBLOCK_CONTENTS(char, utf8P);
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
        uint const length = utf8_seq_length[(unsigned char) *p];

        bool forceDel;
        uint32_t decoded;

        forceDel = false;
        decoded  = 0;  /* suppress compiler warning; valid when !forceDel */

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
        uint const length = utf8_seq_length[(unsigned char) *p];

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







