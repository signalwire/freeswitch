/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/**@CFILE base64.c
 *
 * Implementation of BASE64 encoding and decoding functions.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include "sofia-sip/base64.h"

static unsigned char const code[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#define B64NOP 128
#define B64EOF  64

/**Decode a BASE64-encoded string.
 *
 * The function base64_d() decodes a string @a b64s encoded with BASE64. It
 * stores the result in the buffer @a buf of @a bsiz bytes.
 *
 * If the @a buf is NULL, the function just returns the length of decoded
 * data. In any case, no decoded data is stored in @a buf beyond @a bsiz.
 * The function always returns the full length of decodable data.
 *
 * @param buf  Buffer to store decoded data
 * @param bsiz Size of @a buf
 * @param b64s Base64-encoded string.
 *
 * @return Length of data that can be decoded in bytes.
 *
 * @sa <a href="http://www.ietf.org/rfc/rfc2045.txt">RFC 2045</a>,
 * <i>"Multipurpose Internet Mail Extensions (MIME) Part One:
 * Format of Internet Message Bodies"</i>,
 * N. Freed, N. Borenstein, November 1996.
 *
 * @par Example
 * The following example code decodes a string of BASE64 data into a
 * memory area allocated from heap:
 * @code
 * int decoder(char const *encoded, void **return_decoded)
 * {
 *   int len = base64_d(NULL, 0, encoded);
 *   void *decoded = malloc(len);
 *   base64_d(decoded, len, encoded);
 *   *return_decoded = decoded;
 *   return len;
 * }
 * @endcode
 */
isize_t base64_d(char buf[], isize_t bsiz, char const *b64s)
{
  static unsigned char decode[256] = "";
  unsigned char const *s = (unsigned char const *)b64s;
  unsigned char c, b1, b2 = B64EOF, b3 = B64EOF, b4 = B64EOF;
  unsigned long w;
  isize_t i, len = 0, total_len = 0;

  if (b64s == NULL)
    return 0;

  if (decode['\0'] != B64EOF) {
    /* Prepare decoding table */
    for (i = 1; i < 256; i++)
      decode[i] = B64NOP;

    for (i = 0; i < 64; i++) {
      decode[code[i]] = (unsigned char)i;
    }
    decode['='] = B64EOF;
    decode['\0'] = B64EOF;
  }

  /* Calculate length */
  while ((c = decode[*s++]) != B64EOF) {
    if (c != B64NOP)
      len++;
  }

  total_len = len = len * 3 / 4;

  if (buf == NULL || bsiz == 0)
    return total_len;

  if (len > bsiz)
    len = bsiz;

  for (i = 0, s = (unsigned char const *)b64s; i < len; ) {

    while ((b1 = decode[*s++]) == B64NOP)
      ;
    if (b1 != B64EOF)
      while ((b2 = decode[*s++]) == B64NOP)
	;
    if (b2 != B64EOF)
      while ((b3 = decode[*s++]) == B64NOP)
	;
    if (b3 != B64EOF)
      while ((b4 = decode[*s++]) == B64NOP)
	;

    if (((b1 | b2 | b3 | b4) & (B64NOP|B64EOF)) == 0) {
      /* Normal case, 4 B64 chars to 3 data bytes */
      w = (b1 << 18) | (b2 << 12) | (b3 << 6) | b4;
      buf[i++] = (unsigned char)(w >> 16);
      buf[i++] = (unsigned char)(w >> 8);
      buf[i++] = (unsigned char)(w);
      continue;
    }
    else {
      /* EOF */
      if ((b1 | b2) & B64EOF) {
	/* fputs("base64dec: strange eof ===\n", stderr); */
	break;
      }
      buf[i++] = (b1 << 2) | (b2 >> 4);
      if (b3 != B64EOF) {
	buf[i++] = ((b2 & 15) << 4) | ((b3 >> 2) & 15);
	if (b4 != B64EOF) {
	  buf[i++] = ((b3 & 3) << 6) | b4;
	}
      }
      break;
    }
  }

#if 0
  printf("base64_d returns, decoded %d bytes\n", total_len);
  for (i = 0; i < len; i++)
    printf("%02x", buf[i]);
  printf("\n");
#endif

  return total_len;
}

/**Encode data with BASE64.
 *
 * The function base64_e() encodes @a dsiz bytes of @a data into @a buf.
 *
 * @note The function base64_e() uses at most @a bsiz bytes from @a buf.
 *
 * If @a bsiz is zero, the function just returns the length of BASE64
 * encoding, excluding the final @c NUL.
 *
 * If encoded string is longer than that @a bsiz, the function terminates
 * string with @c NUL at @a buf[bsiz-1], but returns the length of encoding as
 * usual.
 *
 * @param buf  buffer for encoded data
 * @param bsiz size of @a buffer
 * @param data data to be encoded
 * @param dsiz size of @a data
 *
 * @return The function base64_e() return length of encoded string,
 * excluding the final NUL.
 *
 * @sa <a href="http://www.ietf.org/rfc/rfc2045.txt">RFC 2045</a>,
 * <i>"Multipurpose Internet Mail Extensions (MIME) Part One:
 * Format of Internet Message Bodies"</i>,
 * N. Freed, N. Borenstein, November 1996.
 *
 */
isize_t base64_e(char buf[], isize_t bsiz, void *data, isize_t dsiz)
{
  unsigned char *s = (unsigned char *)buf;
  unsigned char *b = (unsigned char *)data;
  unsigned long w;

  isize_t i, n, slack = (unsigned)dsiz % 3;
  isize_t dsize = dsiz - slack, bsize = bsiz;

  if (bsize == 0)
    s = NULL;

  for (i = 0, n = 0; i < dsize; i += 3, n += 4) {
    w = (b[i] << 16) | (b[i+1] << 8) | b[i+2];

    if (s) {
      if (n + 4 < bsize) {
	s[n + 0] = code[(w >> 18) & 63];
	s[n + 1] = code[(w >> 12) & 63];
	s[n + 2] = code[(w >> 6) & 63];
	s[n + 3] = code[(w) & 63];
      } else {
	if (n + 1 < bsize)
	  s[n + 0] = code[(w >> 18) & 63];
	if (n + 2 < bsize)
	  s[n + 1] = code[(w >> 12) & 63];
	if (n + 3 < bsize)
	  s[n + 2] = code[(w >> 6) & 63];
	s[bsize - 1] = '\0';
	s = NULL;
      }
    }
  }

  if (slack) {
    if (s) {
      if (slack == 2)
	w = (b[i] << 16) | (b[i+1] << 8);
      else
	w = (b[i] << 16);

      if (n + 1 < bsize)
	s[n + 0] = code[(w >> 18) & 63];
      if (n + 2 < bsize)
	s[n + 1] = code[(w >> 12) & 63];
      if (n + 3 < bsize)
	s[n + 2] = (slack == 2) ? code[(w >> 6) & 63] : '=';
      if (n + 3 < bsize)
	s[n + 3] = '=';
      if (n + 4 >= bsize)
	s[bsize - 1] = '\0', s = NULL;
    }
    n += 4;
  }

  if (s)
    s[n] = '\0';

  return n;
}
