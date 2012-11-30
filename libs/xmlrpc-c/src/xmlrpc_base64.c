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
** SUCH DAMAGE.
**
** There is more copyright information in the bottom half of this file. 
** Please see it for more details. */


/*=========================================================================
**  XML-RPC Base64 Utilities
**=========================================================================
**  This code was swiped from Jack Jansen's code in Python 1.5.2 and
**  modified to work with our data types.
*/

#include "xmlrpc_config.h"

#include "bool.h"
#include "xmlrpc-c/base.h"

#define CRLF    "\015\012"
#define CR      '\015'
#define LF      '\012'


/***********************************************************
Copyright 1991, 1992, 1993, 1994 by Stichting Mathematisch Centrum,
Amsterdam, The Netherlands.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the names of Stichting Mathematisch
Centrum or CWI or Corporation for National Research Initiatives or
CNRI not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior
permission.

While CWI is the initial source for this software, a modified version
is made available by the Corporation for National Research Initiatives
(CNRI) at the Internet address ftp://ftp.python.org.

STICHTING MATHEMATISCH CENTRUM AND CNRI DISCLAIM ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL STICHTING MATHEMATISCH
CENTRUM OR CNRI BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.

******************************************************************/

static char table_a2b_base64[] = {
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
    52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1, 0,-1,-1, /* Note PAD->0 */
    -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
    -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
};

#define BASE64_PAD '='
#define BASE64_MAXBIN 57    /* Max binary chunk size (76 char line) */
#define BASE64_LINE_SZ 128      /* Buffer size for a single line. */    

static unsigned char const table_b2a_base64[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";



static xmlrpc_mem_block *
base64Encode(xmlrpc_env *          const envP,
             const unsigned char * const binData,
             size_t                const binLen,
             bool                  const wantNewlines) {

    size_t chunkStart, chunkLeft;
    unsigned char * asciiData;
    int leftbits;
    unsigned char thisCh;
    unsigned int leftchar;
    xmlrpc_mem_block * outputP;
    unsigned char lineBuffer[BASE64_LINE_SZ];
    const unsigned char * cursor;

    /* Create a block to hold our lines when we finish them. */
    outputP = xmlrpc_mem_block_new(envP, 0);
    XMLRPC_FAIL_IF_FAULT(envP);

    /* Deal with empty data blocks gracefully. Yuck. */
    if (binLen == 0) {
        if (wantNewlines)
            XMLRPC_MEMBLOCK_APPEND(char, envP, outputP, CRLF, 2);
        goto cleanup;
    }

    /* Process our binary data in line-sized chunks. */
    for (chunkStart = 0, cursor = &binData[0];
         chunkStart < binLen;
         chunkStart += BASE64_MAXBIN) {

        /* Set up our per-line state. */
        asciiData = &lineBuffer[0];
        chunkLeft = binLen - chunkStart;
        if (chunkLeft > BASE64_MAXBIN)
            chunkLeft = BASE64_MAXBIN;
        leftbits = 0;
        leftchar = 0;

        for(; chunkLeft > 0; --chunkLeft, ++cursor) {
            /* Shift the data into our buffer */
            leftchar = (leftchar << 8) | *cursor;
            leftbits += 8;

            /* See if there are 6-bit groups ready */
            while (leftbits >= 6) {
                thisCh = (leftchar >> (leftbits-6)) & 0x3f;
                leftbits -= 6;
                *asciiData++ = table_b2a_base64[thisCh];
            }
        }
        if (leftbits == 2) {
            *asciiData++ = table_b2a_base64[(leftchar&3) << 4];
            *asciiData++ = BASE64_PAD;
            *asciiData++ = BASE64_PAD;
        } else if (leftbits == 4) {
            *asciiData++ = table_b2a_base64[(leftchar&0xf) << 2];
            *asciiData++ = BASE64_PAD;
        } 

        /* Append a courtesy CRLF. */
        if (wantNewlines) {
            *asciiData++ = CR;
            *asciiData++ = LF;
        }
    
        /* Save our line. */
        XMLRPC_MEMBLOCK_APPEND(char, envP, outputP, lineBuffer,
                               asciiData - &lineBuffer[0]);
        XMLRPC_FAIL_IF_FAULT(envP);
    }

 cleanup:
    if (envP->fault_occurred) {
        if (outputP)
            xmlrpc_mem_block_free(outputP);
        return NULL;
    }
    return outputP;
}



xmlrpc_mem_block *
xmlrpc_base64_encode(xmlrpc_env *          const envP,
                     const unsigned char * const binData,
                     size_t                const binLen) {

    return base64Encode(envP, binData, binLen, true);
}



xmlrpc_mem_block *
xmlrpc_base64_encode_without_newlines(xmlrpc_env *          const envP,
                                      const unsigned char * const binData,
                                      size_t                const binLen) {

    return base64Encode(envP, binData, binLen, false);
}



xmlrpc_mem_block *
xmlrpc_base64_decode(xmlrpc_env * const envP,
                     const char * const asciiData,
                     size_t       const acsiiLen) {

    unsigned char * binData;
    int leftbits;
    unsigned char thisCh;
    unsigned int leftchar;
    size_t npad;
    size_t binLen, bufferSize;
    xmlrpc_mem_block * outputP;
    const char * nextCharP;
    size_t remainingLen;

    /* Create a block to hold our chunks when we finish them.
    ** We overestimate the size now, and fix it later. */
    bufferSize = ((acsiiLen + 3) / 4) * 3;
    outputP = xmlrpc_mem_block_new(envP, bufferSize);
    XMLRPC_FAIL_IF_FAULT(envP);

    /* Set up our decoder state. */
    leftbits = 0;
    leftchar = 0;
    npad = 0;
    binData = XMLRPC_MEMBLOCK_CONTENTS(unsigned char, outputP);
    binLen = 0;

    for (remainingLen = acsiiLen, nextCharP = asciiData;
         remainingLen > 0; 
         --remainingLen, ++nextCharP) {

        /* Skip some punctuation. */
        thisCh = (*nextCharP & 0x7f);
        if (thisCh == '\r' || thisCh == '\n' || thisCh == ' ')
            continue;
        if (thisCh == BASE64_PAD)
            ++npad;
        thisCh = table_a2b_base64[(*nextCharP) & 0x7f];

        /* XXX - We just throw away invalid characters. Is this right? */
        if (thisCh == (unsigned char) -1)
            continue;

        /* Shift it in on the low end, and see if there's a byte ready for
           output.
        */
        leftchar = (leftchar << 6) | (thisCh);
        leftbits += 6;
        if (leftbits >= 8) {
            leftbits -= 8;
            XMLRPC_ASSERT(binLen < bufferSize);
            *binData++ = (leftchar >> leftbits) & 0xFF;
            leftchar &= ((1 << leftbits) - 1);
            ++binLen;
        }
    }

    /* Check that no bits are left. */
    if (leftbits)
        XMLRPC_FAIL(envP, XMLRPC_PARSE_ERROR, "Incorrect Base64 padding");

    /* Check to make sure we have a sane amount of padding. */
    if (npad > binLen || npad > 2)
        XMLRPC_FAIL(envP, XMLRPC_PARSE_ERROR, "Malformed Base64 data");

    /* Remove any padding and set the correct size. */
    binLen -= npad;
    XMLRPC_MEMBLOCK_RESIZE(char, envP, outputP, binLen);
    XMLRPC_ASSERT(!envP->fault_occurred);

                     cleanup:
    if (envP->fault_occurred) {
        if (outputP)
            xmlrpc_mem_block_free(outputP);
        return NULL;
    }
    return outputP;
}
