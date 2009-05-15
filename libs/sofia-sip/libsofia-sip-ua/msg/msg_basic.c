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

/**@ingroup msg_headers
 * @CFILE msg_basic.c
 * @brief Basic header handling.
 *
 * This file contains implementation of basic headers, that is, generic
 * headers like Subject or Organization containing non-structured text only,
 * numeric headers like Content-Length or Max-Forwards containing only an
 * 32-bit unsigned integer, or token list headers like Supported or Allow.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri Feb 23 19:51:55 2001 ppessi
 */

#include "config.h"

#include <sofia-sip/su_alloc.h>

#include <sofia-sip/msg.h>
#include <sofia-sip/bnf.h>
#include <sofia-sip/msg_parser.h>
#include <sofia-sip/msg_header.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>

#define msg_generic_update NULL

/* ====================================================================== */

/**@ingroup msg_headers
 * @defgroup msg_error Erroneous Headers
 *
 * The erroneous headers are stored in #msg_error_t structure.
 *
 * @note Parser may put other headers (like duplicate Content-Length
 * headers) into the list of erroneous headers. If the list of erroneous
 * headers is processed, the header type must be validated first by calling
 * msg_is_error() (or by other relevant tests).
 */

/**@ingroup msg_error
 * @typedef typedef struct msg_error_s msg_error_t;
 * Type for erroneous headers.
 */

isize_t msg_error_dup_xtra(msg_header_t const *h, isize_t offset);
char *msg_error_dup_one(msg_header_t *dst, msg_header_t const *src,
			  char *b, isize_t xtra);

msg_hclass_t msg_error_class[] =
MSG_HEADER_CLASS(msg_, error, "", "", er_common, append,
                 msg_error, msg_generic);

issize_t msg_error_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  return 0;
}

issize_t msg_error_e(char b[], isize_t bsiz, msg_header_t const *h, int flags)
{
  /* There is no way to encode an erroneous header */
  return 0;
}

isize_t msg_error_dup_xtra(msg_header_t const *h, isize_t offset)
{
  return msg_default_dup_xtra(h, offset);
}

char *msg_error_dup_one(msg_header_t *dst, msg_header_t const *src,
			  char *b, isize_t xtra)
{
  return msg_default_dup_one(dst, src, b, xtra);
}

/* ====================================================================== */

/**@ingroup msg_headers
 * @defgroup msg_unknown Unknown Headers
 *
 * The unknown headers are handled with #msg_unknown_t structure. The whole
 * unknown header including its name is included in the header value string
 * @a g_value.
 *
 * @note It is possible to speed up parsing process by creating a parser
 * which does understand only a minimum number of headers. If such a parser
 * is used, some well-known headers are not regocnized or parser, but they
 * are treated as unknown and put unparsed into the list of unknown headers.
 */

/**@ingroup msg_unknown
 * @typedef typedef struct msg_unknown_s msg_unknown_t;
 *
 * Type for unknown headers.
 */

msg_hclass_t msg_unknown_class[] =
MSG_HEADER_CLASS(msg_, unknown, "", "", un_common, append,
                 msg_unknown, msg_generic);

issize_t msg_unknown_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  msg_unknown_t *un = (msg_unknown_t *)h;

  if (msg_token_d(&s, &un->un_name) < 0 ||
      *s != ':')
    return -1;

  *s++ = '\0';
  skip_lws(&s);
  un->un_value = s;

  return 0;
}

issize_t msg_unknown_e(char b[], isize_t bsiz, msg_header_t const *h, int flags)
{
  char *b0 = b, *end = b + bsiz;
  msg_unknown_t *un = (msg_unknown_t *)h;
  int const compact = MSG_IS_COMPACT(flags);

  MSG_STRING_E(b, end, un->un_name);
  MSG_CHAR_E(b, end, ':');
  if (!compact) MSG_CHAR_E(b, end, ' ');
  MSG_STRING_E(b, end, un->un_value);

  return b - b0;
}

isize_t msg_unknown_dup_xtra(msg_header_t const *h, isize_t offset)
{
  msg_unknown_t const *un = (msg_unknown_t *)h;
  return offset + MSG_STRING_SIZE(un->un_name) + MSG_STRING_SIZE(un->un_value);
}

char *msg_unknown_dup_one(msg_header_t *dst, msg_header_t const *src,
			  char *b, isize_t xtra)
{
  msg_unknown_t *un = (msg_unknown_t *)dst;
  msg_unknown_t const *o = (msg_unknown_t *)src;
  char *end = b + xtra;

  MSG_STRING_DUP(b, un->un_name, o->un_name);
  MSG_STRING_DUP(b, un->un_value, o->un_value);

  assert(b <= end); (void)end;

  return b;
}

/* ====================================================================== */

/**@ingroup msg_headers
 * @defgroup msg_payload Message Body
 *
 * The payload object contains the message body. The message body has no
 * structure, but it is stored in the @a pl_data buffer as a byte array.
 * Multiple payload objects may be linked to a list.
 */

/**@ingroup msg_payload
 * @typedef typedef struct msg_payload_s msg_payload_t;
 *
 * The structure msg_payload_t contains representation of MIME message payload.
 *
 * The msg_payload_t is defined as follows:
 * @code
 * typedef struct msg_payload_s {
 *   msg_common_t    pl_common[1];      // Common fragment info
 *   msg_header_t   *pl_next;           // Next payload object
 *   char           *pl_data;           // Data - may contain zero bytes
 *   usize_t         pl_len;            // Length of message payload
 * } msg_payload_t;
 * @endcode
 */

msg_hclass_t msg_payload_class[1] =
MSG_HEADER_CLASS(msg_, payload, NULL, "", pl_common, append,
		 msg_payload, msg_generic);

/** Create a MIME payload */
msg_payload_t *msg_payload_create(su_home_t *home, void const *data, usize_t len)
{
  msg_header_t *h = msg_header_alloc(home, msg_payload_class, len + 1);

  if (h) {
    msg_payload_t *pl = (msg_payload_t *)h;
    char *b = msg_header_data(h->sh_common);

    if (data)
      memcpy(b, data, len);
    else
      memset(b, 0, len);
    b[len] = 0;

    h->sh_data = pl->pl_data = b;
    h->sh_len = pl->pl_len = len;

    return pl;
  }

  return NULL;
}

/** Parse payload. */
issize_t msg_payload_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  msg_payload_t *pl = (msg_payload_t *)h;

  pl->pl_len = slen;
  pl->pl_data = s;

  h->sh_len = slen;
  h->sh_data = s;

  return 0;
}

issize_t msg_payload_e(char b[], isize_t bsiz, msg_header_t const *h, int flags)
{
  msg_payload_t *pl = (msg_payload_t *)h;
  size_t len = pl->pl_len;

  if (bsiz > 0) {
    if (len < bsiz)
      memcpy(b, pl->pl_data, len), b[len] = '\0';
    else
      memcpy(b, pl->pl_data, bsiz - 1), b[bsiz - 1] = '\0';
  }

  return len;
}

isize_t msg_payload_dup_xtra(msg_header_t const *h, isize_t offset)
{
  msg_payload_t *pl = (msg_payload_t *)h;
  return offset + pl->pl_len + 1;
}

char *msg_payload_dup_one(msg_header_t *dst,
			  msg_header_t const *src,
			  char *b,
			  isize_t xtra)
{
  msg_payload_t *pl = (msg_payload_t *)dst;
  msg_payload_t const *o = (msg_payload_t const *)src;

  memcpy(pl->pl_data = b, o->pl_data, pl->pl_len = o->pl_len);

  dst->sh_data = pl->pl_data;
  dst->sh_len = pl->pl_len;

  pl->pl_data[pl->pl_len] = 0;	/* NUL terminate just in case */

  return b + pl->pl_len + 1;
}

usize_t msg_payload_length(msg_payload_t const *pl)
{
  /* XXX */
  return 0;
}

/* ====================================================================== */

/**@ingroup msg_headers
 * @defgroup msg_separator Message Separator
 *
 * An empty line separates headers from the message body. In order to avoid
 * modifying messages with integrity protection, the separator line has its
 * own header structure which is included in the msg_t structure.
 */

/**@ingroup msg_separator
 * @typedef typedef struct msg_separator_s msg_separator_t;
 *
 * The structure msg_separator_t contains representation of separator line
 * between message headers and body.
 *
 * The msg_separator_t is defined as follows:
 * @code
 * typedef struct msg_separator_s {
 *   msg_common_t    sep_common[1];     // Common fragment info
 *   msg_header_t   *sep_next;          // Pointer to next header
 *   char            sep_data[4];       // NUL-terminated separator
 * } msg_separator_t;
 * @endcode
 */

msg_hclass_t msg_separator_class[] =
MSG_HEADER_CLASS(msg_, separator, NULL, "", sep_common, single,
		 msg_default, msg_generic);

/** Calculate length of line ending (0, 1 or 2). @internal */
#define CRLF_TEST(s) ((s[0]) == '\r' ? ((s[1]) == '\n') + 1 : (s[0])=='\n')

/** Parse a separator line. */
issize_t msg_separator_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  int len = CRLF_TEST(s);
  msg_separator_t *sep = (msg_separator_t *)h;

  if (len == 0 && slen > 0)
    return -1;

  memcpy(sep->sep_data, s, len);
  sep->sep_data[len] = '\0';

  return 0;
}

/** Encode a separator line. */
issize_t msg_separator_e(char b[], isize_t bsiz, msg_header_t const *h, int flags)
{
  msg_separator_t const *sep = (msg_separator_t const *)h;
  size_t n = strlen(sep->sep_data);

  if (bsiz > n)
    strcpy(b, sep->sep_data);

  return (issize_t)n;
}

msg_separator_t *msg_separator_create(su_home_t *home)
{
  msg_separator_t *sep;

  sep = (msg_separator_t *)msg_header_alloc(home, msg_separator_class, 0);
  if (sep)
    strcpy(sep->sep_data, CRLF);

  return sep;
}

/* ====================================================================== */


