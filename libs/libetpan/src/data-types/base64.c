/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2005 - Juergen Graf
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the libEtPan! project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id: base64.c,v 1.3 2005/06/01 12:21:57 smarinier Exp $
 */

#include "base64.h"

#include <stdlib.h>

#define OUTPUT_SIZE 513
#define CHAR64(c)  (((c) < 0 || (c) > 127) ? -1 : index_64[(c)])

static char index_64[128] = {
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
    52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1,-1,-1,-1,
    -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
    -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
};

static char basis_64[] =
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

LIBETPAN_EXPORT
char * encode_base64(const char * in, int len)
{
  char * output, * tmp;
  unsigned char oval;
  int out_len;

  out_len = ((len + 2) / 3 * 4) + 1;

  if ((len > 0) && (in == NULL))
    return NULL;

  output = malloc(out_len);
  if (!output)
    return NULL;
    
  tmp = output;
  while (len >= 3) {
    *tmp++ = basis_64[in[0] >> 2];
    *tmp++ = basis_64[((in[0] << 4) & 0x30) | (in[1] >> 4)];
    *tmp++ = basis_64[((in[1] << 2) & 0x3c) | (in[2] >> 6)];
    *tmp++ = basis_64[in[2] & 0x3f];
    in += 3;
    len -= 3;
  }
  if (len > 0) {
    *tmp++ = basis_64[in[0] >> 2];
    oval = (in[0] << 4) & 0x30;
    if (len > 1) oval |= in[1] >> 4;
    *tmp++ = basis_64[oval];
    *tmp++ = (len < 2) ? '=' : basis_64[(in[1] << 2) & 0x3c];
    *tmp++ = '=';
  }

  *tmp = '\0';
    
  return output;
}

LIBETPAN_EXPORT
char * decode_base64(const char * in, int len)
{
  char * output, * out;
  int i, c1, c2, c3, c4, out_len;

  out_len = 0;
  
  output = malloc(OUTPUT_SIZE);
  if (output == NULL)
    return NULL;
  out = output;

  if (in[0] == '+' && in[1] == ' ')
    in += 2;
  
  for (i = 0; i < (len / 4); i++) {
    c1 = in[0];
    c2 = in[1];
    c3 = in[2];
    c4 = in[3];
    if (CHAR64(c1) == -1 || CHAR64(c2) == -1 || 
        (c3 != '=' && CHAR64(c3) == -1) || 
        (c4 != '=' && CHAR64(c4) == -1))
      return NULL;

    in += 4;
    *output++ = (CHAR64(c1) << 2) | (CHAR64(c2) >> 4);
    if (++out_len >= OUTPUT_SIZE)
      return NULL;

    if (c3 != '=') {
      *output++ = ((CHAR64(c2) << 4) & 0xf0) | (CHAR64(c3) >> 2);
      if (++out_len >= OUTPUT_SIZE)
        return NULL;
      
      if (c4 != '=') {
        *output++ = ((CHAR64(c3) << 6) & 0xc0) | CHAR64(c4);  
        if (++out_len >= OUTPUT_SIZE)
          return NULL;
      }
    }
  }
  
  *output = 0;
  
  return out;
}


