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

/**@ingroup msg_parser
 * @CFILE msg_mclass.c
 *
 * Message factory object.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Jun  5 14:34:24 2002 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>

#include <stdarg.h>
#include <sofia-sip/su_tagarg.h>

#include <sofia-sip/su.h>
#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_string.h>

#include "msg_internal.h"
#include "sofia-sip/msg_parser.h"
#include "sofia-sip/msg_mclass.h"
#include "sofia-sip/msg_mclass_hash.h"

/** Clone a message class.
 *
 * @relatesalso msg_mclass_s
 *
 * The function msg_mclass_clone() makes a copy of message class object @a
 * old. It is possible to resize the hash table by giving a non-zero @a
 * newsize. If @a newsize is 0, the size of hash table is not changed. If @a
 * empty is true, the copied message class object will not recognize any
 * headers. This is useful if more fine-grained control of parsing process
 * is required, for instance.
 *
 * @param[in] old      pointer to the message class object to be copied
 * @param[in] newsize  size of hash table in the copied object
 * @param[in] empty    if true, resulting copy does not contain any headers
 *
 * @return
 * The function msg_mclass_clone() returns a pointer to a newly
 * copied message class object, or NULL upon an error.
 * The returned message class object can be freed with free().
 *
 * @ERRORS
 * @ERROR ENOMEM
 * A memory allocation failed.
 * @ERROR EINVAL
 * The function was given invalid arguments.
 *
 * @note The empty parser can handle request/status line. All headers are
 * put into list of unknown headers (unless they are malformed, and they are
 * put into list of erronous headers). However, SIP, RTSP, and HTTP
 * protocols all require that the parser recognizes @b Content-Length header
 * before they can extract the message body from the data received from
 * network.
 *
 */
msg_mclass_t *msg_mclass_clone(msg_mclass_t const *old, int newsize, int empty)
{
  size_t size, shortsize;
  msg_mclass_t *mc;
  int identical;
  unsigned short i;

  if (newsize == 0)
    newsize = old->mc_hash_size;

  if (newsize < old->mc_hash_used ||
      (unsigned)newsize > USHRT_MAX / sizeof(msg_header_t *)) {
    errno = EINVAL;
    return NULL;
  }

  size = offsetof(msg_mclass_t, mc_hash[newsize]);
  if (old->mc_short)
    shortsize = MC_SHORT_SIZE * (sizeof old->mc_short[0]);
  else
    shortsize = 0;
  mc = malloc(size + shortsize);
  identical = newsize == old->mc_hash_size && !empty;

  if (mc) {
    if (!identical) {
      memcpy(mc, old, offsetof(msg_mclass_t, mc_hash));
      memset(mc->mc_hash, 0, size - offsetof(msg_mclass_t, mc_hash));
      mc->mc_short = NULL;
      mc->mc_hash_size = newsize;
      mc->mc_hash_used = 0;
      for (i = 0; !empty && i < old->mc_hash_size; i++) {
	msg_mclass_insert(mc, &old->mc_hash[i]);
      }
    }
    else {
      memcpy(mc, old, size);
      mc->mc_short = NULL;
    }

    if (shortsize) {
      if (empty)
	mc->mc_short = memset((char *)mc + size, 0, shortsize);
      else
	mc->mc_short = memcpy((char *)mc + size, old->mc_short, shortsize);
    }
  }

  return mc;
}

/**Add a new header to the message class.
 *
 * @relatesalso msg_mclass_s
 *
 * Insert a header class @a hc to the message class object @a mc. If the
 * given @a offset of the header in @ref msg_pub_t "public message
 * structure" is zero, the function extends the public message structure in
 * order to store newly inserted header there.
 *
 * @param[in,out] mc       pointer to a message class object
 * @param[in]     hc       pointer to a header class object
 * @param[in]     offset   offset of the header in
 *                         @ref msg_pub_t "public message structure"
 *
 * If the @a offset is 0, the msg_mclass_insert_header() increases size of
 * the public message structure and places the header at the end of message.
 *
 * @return Number of collisions in hash table, or -1 upon an error.
 *
 * @deprecated Use msg_mclass_insert_with_mask() instead.
 */
int msg_mclass_insert_header(msg_mclass_t *mc,
			     msg_hclass_t *hc,
			     unsigned short offset)
{
  return msg_mclass_insert_with_mask(mc, hc, offset, 0);
}

/**Add a new header to the message class.
 *
 * @relatesalso msg_mclass_s
 *
 * Insert a header class @a hc to the message class @a mc. If the given @a
 * offset of the header in @ref msg_pub_t "public message structure" is
 * zero, extend the size of the public message structure in order to store
 * headers at the end of structure.
 *
 * @param[in,out] mc       pointer to a message class
 * @param[in]     hc       pointer to a header class
 * @param[in]     offset   offset of the header in
 *                         @ref msg_pub_t "public message structure"
 * @param[in]     flags    classification flags for the header
 *
 * @return Number of collisions in hash table, or -1 upon an error.
 */
int msg_mclass_insert_with_mask(msg_mclass_t *mc,
				msg_hclass_t *hc,
				unsigned short offset,
				unsigned short flags)
{
  msg_href_t hr[1];

  if (mc == NULL || hc == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (msg_hclass_offset(mc, NULL, hc))
    return (void)(errno = EEXIST), -1;

  if (offset == 0)
    offset = mc->mc_msize, mc->mc_msize += sizeof(msg_header_t *);

  assert(offset < mc->mc_msize);

  hr->hr_class = hc;
  hr->hr_offset = offset;
  hr->hr_flags = flags;

  return msg_mclass_insert(mc, hr);
}

/** Add a header reference to the message class.
 *
 * @relatesalso msg_mclass_s
 *
 * @param[in,out] mc       pointer to a message class object
 * @param[in]     hr       header reference object
 *
 * @return Number of collisions in hash table, or -1 upon an error.
 */
int msg_mclass_insert(msg_mclass_t *mc, msg_href_t const *hr)
{
  int j, j0;
  int N;
  int collisions = 0;
  msg_hclass_t *hc;

  if (mc == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (hr == NULL || (hc = hr->hr_class) == NULL)
    return 0;

  /* Add short form */
  if (mc->mc_short && hc->hc_short[0]) {
    char compact = hc->hc_short[0];
    msg_href_t *shorts = (msg_href_t *)mc->mc_short;

    if (compact < 'a' || compact > 'z')
      return -1;

    if (shorts[compact - 'a'].hr_class &&
	shorts[compact - 'a'].hr_class != hc)
      return -1;

    shorts[compact - 'a'] = *hr;
  }

  N = mc->mc_hash_size;
  j0 = msg_header_name_hash(hc->hc_name, NULL) % N;

  for (j = j0; mc->mc_hash[j].hr_class; ) {
    collisions++;
    if (mc->mc_hash[j].hr_class == hc)
      return -1;
    j = (j + 1) % N;
    if (j == j0)
      return -1;
  }

  mc->mc_hash[j] = hr[0];
  mc->mc_hash_used++;

  return collisions;
}

/** Calculate length of line ending (0, 1 or 2). @internal */
#define CRLF_TEST(cr, lf) ((cr) == '\r' ? ((lf) == '\n') + 1 : (cr)=='\n')

/**Search for a header class.
 *
 * @relatesalso msg_mclass_s
 *
 * The function msg_find_hclass() searches for a header class from a message
 * class based on the contents of the header to be parsed. The buffer @a s
 * should point to the first character in the header name.
 *
 * @param[in]  mc   message class object
 * @param[in]  s    header contents
 * @param[out] return_start_of_content start of header content (may be NULL)
 *
 * @return The function msg_find_hclass() returns a pointer to a header
 * reference structure. A pointer to a header reference for unknown headers
 * is returned, if the header is not included in the message class.
 *
 * @par
 * The return-value parameter @a return_start_of_content will contain the
 * start of the header contents within @a s, or 0 upon an error parsing the
 * header name and following colon.
 *
 * @par
 * Upon a fatal error, a NULL pointer is returned.
 */
msg_href_t const *msg_find_hclass(msg_mclass_t const *mc,
				  char const *s,
				  isize_t *return_start_of_content)
{
  msg_href_t const *hr;
  short i, N, m;
  isize_t len;

  assert(mc);

  N = mc->mc_hash_size;

  i = msg_header_name_hash(s, &len) % N;

  if (len == 0 || len > HC_LEN_MAX) {
    if (return_start_of_content)
      *return_start_of_content = 0;
    return mc->mc_error;
  }

  m = (short)len;

  if (m == 1 && mc->mc_short) {
    short c = s[0];
    if (c >= 'a' && c <= 'z')
      hr = &mc->mc_short[c - 'a'];
    else if (c >= 'A' && c <= 'Z')
      hr = &mc->mc_short[c - 'A'];
    else
      hr = mc->mc_unknown;

    if (hr->hr_class == NULL)
      hr = mc->mc_unknown;
  }
  else {
    msg_hclass_t *hc;

    /* long form */
    for (hr = NULL; (hc = mc->mc_hash[i].hr_class); i = (i + 1) % N) {
      if (m == hc->hc_len && su_casenmatch(s, hc->hc_name, m)) {
	hr = &mc->mc_hash[i];
	break;
      }
    }

    if (hr == NULL)
      hr = mc->mc_unknown;
  }

  if (!return_start_of_content)	/* Just header name */
    return hr;

  if (s[len] == ':') {		/* Fast path */
    *return_start_of_content = ++len;
    return hr;
  }

  if (IS_LWS(s[len])) {
    int crlf = 0;
    do {
      len += span_ws(s + len + crlf) + crlf; /* Skip lws before colon */
      crlf = CRLF_TEST(s[len], s[len + 1]);
    }
    while (IS_WS(s[len + crlf]));
  }

  if (s[len++] != ':')		/* Colon is required in header */
    len = 0;

  *return_start_of_content = len;

  return hr;
}
