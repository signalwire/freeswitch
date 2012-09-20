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

/**@ingroup msg_mime
 * @CFILE msg_mime.c
 *
 * MIME-related headers and MIME multipart bodies for SIP/HTTP/RTSP.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Jun 13 02:57:51 2000 ppessi
 *
 *
 */

#include "config.h"

#define _GNU_SOURCE 1

#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_string.h>

#include "msg_internal.h"
#include "sofia-sip/msg.h"
#include "sofia-sip/msg_mime.h"

#include <sofia-sip/su_uniqueid.h>
#include <sofia-sip/su_errno.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

#if !HAVE_MEMMEM
void *memmem(const void *haystack, size_t haystacklen,
	     const void *needle, size_t needlelen);
#endif

/** Protocol version of MIME */
char const msg_mime_version_1_0[] = "MIME/1.0";

#include <sofia-sip/msg_parser.h>
#include <sofia-sip/msg_mime_protos.h>

/** Define a header class for headers without any extra data to copy */
#define MSG_HEADER_CLASS_G(c, l, s, kind) \
  MSG_HEADER_CLASS(msg_, c, l, s, g_common, kind, msg_generic, msg_generic)

#define msg_generic_update NULL

/** Define a header class for a msg_list_t kind of header */
#define MSG_HEADER_CLASS_LIST(c, l, s, kind) \
  MSG_HEADER_CLASS(msg_, c, l, s, k_items, kind, msg_list, msg_list)

#define msg_list_update NULL

/* ====================================================================== */

/** Calculate length of line ending (0, 1 or 2). @internal */
#define CRLF_TEST(b) ((b)[0] == '\r' ? ((b)[1] == '\n') + 1 : (b)[0] =='\n')

/**@ingroup msg_mime
 * @defgroup msg_multipart MIME Multipart Body
 *
 * Representing MIME multipart bodies and their manipulation.
 *
 * The #msg_multipart_t is an object for storing MIME multipart message
 * bodies. It includes message components used for framing and identifying
 * message parts. Its syntax is defined in  @RFC2046 as follows:
 *
 * @code
 *
 *   multipart-body := [preamble CRLF]
 *                     dash-boundary transport-padding CRLF
 *                     body-part *encapsulation
 *                     close-delimiter transport-padding
 *                     [CRLF epilogue]
 *
 *   preamble := discard-text
 *
 *   discard-text := *(*text CRLF)
 *                   ; May be ignored or discarded.
 *
 *   dash-boundary := "--" boundary
 *                    ; boundary taken from the value of boundary parameter
 *                    ; of the Content-Type field.
 *
 *   boundary := 0*69<bchars> bcharsnospace
 *
 *   bchars := bcharsnospace / " "
 *
 *   bcharsnospace := DIGIT / ALPHA / "'" / "(" / ")" /
 *                    "+" / "_" / "," / "-" / "." /
 *                    "/" / ":" / "=" / "?"
 *
 *   transport-padding := *LWSP-char
 *                        ; Composers MUST NOT generate non-zero length
 *                        ; transport padding, but receivers MUST be able to
 *                        ; handle padding added by message transports.
 *
 *   body-part := <"message" as defined in @RFC822, with all header fields
 *                 optional, not starting with the specified dash-boundary,
 *                 and with the delimiter not occurring anywhere in the body
 *                 part. Note that the semantics of a part differ from the
 *                 semantics of a message, as described in the text.>
 *
 *   encapsulation := delimiter transport-padding CRLF
 *                    body-part
 *
 *   close-delimiter := delimiter "--"
 *
 *   delimiter := CRLF dash-boundary
 *
 *   epilogue := discard-text
 *
 * @endcode
 *
 * @par Parsing a Multipart Message
 *
 * When a message body contains a multipart entity (in other words, it has a
 * MIME media type of "multipart"), the application can split the multipart
 * entity into body parts
 *
 * The parsing is relatively simple, the application just gives a memory
 * home object, a Content-Type header object and message body object as an
 * argument to msg_multipart_parse() function:
 * @code
 *    if (sip->sip_content_type &&
 *        su_casenmatch(sip->sip_content_type, "multipart/", 10)) {
 *      msg_multipart_t *mp;
 *
 *      if (sip->sip_multipart)
 *        mp = sip->sip_multipart;
 *      else
 *        mp = msg_multipart_parse(msg_home(msg),
 *                                 sip->sip_content_type,
 *                                 (sip_payload_t *)sip->sip_payload);
 *
 *      if (mp)
 *        ... processing multipart ...
 *      else
 *        ... error handling ...
 *    }
 * @endcode
 *
 * The resulting list of msg_multipart_t structures contain the parts of the
 * multipart entity, each part represented by a separate #msg_multipart_t
 * structure. Please note that in order to make error recovery possible, the
 * parsing is not recursive - if multipart contains another multipart, the
 * application is responsible for scanning for it and parsing it.
 *
 * @par Constructing a Multipart Message
 *
 * Constructing a multipart body is a bit more hairy. The application needs
 * a message object (#msg_t), which is used to buffer the encoding of
 * multipart components.
 *
 * As an example, let us create a "multipart/mixed" multipart entity with a
 * HTML and GIF contents, and convert it into a #sip_payload_t structure:
 * @code
 *   msg_t *msg = msg_create(sip_default_mclass, 0);
 *   su_home_t *home = msg_home(msg);
 *   sip_t *sip = sip_object(msg);
 *   sip_content_type_t *c;
 *   msg_multipart_t *mp = NULL;
 *   msg_header_t *h = NULL;
 *   char *b;
 *   size_t len, offset;
 *
 *   mp = msg_multipart_create(home, "text/html;level=3", html, strlen(html));
 *   mp->mp_next = msg_multipart_create(home, "image/gif", gif, giflen);
 *
 *   c = sip_content_type_make(home, "multipart/mixed");
 *
 *   // Add delimiters to multipart, and boundary parameter to content-type
 *   if (msg_multipart_complete(home, c, mp) < 0)
 *     return -1;		// Error
 *
 *   // Combine multipart components into the chain
 *   h = NULL;
 *   if (msg_multipart_serialize(&h, mp) < 0)
 *     return -1;		// Error
 *
 *   // Encode all multipart components
 *   len = msg_multipart_prepare(msg, mp, 0);
 *   if (len < 0)
 *     return -1;		// Error
 *
 *   pl = sip_payload_create(home, NULL, len);
 *
 *   // Copy each element from multipart to pl_data
 *   b = pl->pl_data;
 *   for (offset = 0, h = mp; offset < len; h = h->sh_succ) {
 *     memcpy(b + offset, h->sh_data, h->sh_len);
 *     offset += h->sh_len;
 *   }
 * @endcode
 *
 */

/**Create a part for MIME multipart entity.
 *
 * The function msg_multipart_create() allocates a new #msg_multipart_t
 * object from memory home @a home. If @a content_type is non-NULL, it makes
 * a #msg_content_type_t header object and adds the header to the
 * #msg_multipart_t object. If @a dlen is nonzero, it allocates a
 * msg_payload_t structure of @a dlen bytes for the payload of the newly
 * created #msg_multipart_t object. If @a data is non-NULL, it copies the @a
 * dlen bytes of of data to the payload of the newly created
 * #msg_multipart_t object.
 *
 * @return A pointer to the newly created #msg_multipart_t object, or NULL
 * upon an error.
 */
msg_multipart_t *msg_multipart_create(su_home_t *home,
				      char const *content_type,
				      void const *data,
				      isize_t dlen)
{
  msg_multipart_t *mp;

  mp = (msg_multipart_t *)msg_header_alloc(home, msg_multipart_class, 0);

  if (mp) {
    if (content_type)
      mp->mp_content_type = msg_content_type_make(home, content_type);
    if (dlen)
      mp->mp_payload = msg_payload_create(home, data, dlen);

    if ((!mp->mp_content_type && content_type) ||
	(!mp->mp_payload && dlen)) {
      su_free(home, mp->mp_content_type);
      su_free(home, mp->mp_payload);
      su_free(home, mp);
      mp = NULL;
    }
  }

  return mp;
}

/** Convert boundary parameter to a search string. */
static char *
msg_multipart_boundary(su_home_t *home, char const *b)
{
  char *boundary;

  if (!b || !(boundary = su_alloc(home, 2 + 2 + strlen(b) + 2 + 1)))
    return NULL;

  strcpy(boundary, CR LF "--");

  if (b[0] == '"') /* " See http://bugzilla.gnome.org/show_bug.cgi?id=134216 */

    msg_unquote(boundary + 4, b);
  else
    strcpy(boundary + 4, b);


  strcat(boundary + 4, CR LF);

  return boundary;
}


/** Boundary chars. */
static char const bchars[] =
"'()+_,-./:=?"
"0123456789"
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
" ";

#define bchars_len (sizeof(bchars) - 1)

/** Search for a suitable boundary from MIME. */
static char *
msg_multipart_search_boundary(su_home_t *home, char const *p, size_t len)
{
  size_t m;
  unsigned crlf;
  char const *end = p + len;
  char *boundary;

  if (len < 2)
    return NULL;

  /* Boundary looks like LF -- string SP* [CR] LF */
  if (memcmp("--", p, 2) == 0) {
    /* We can be at boundary beginning, there is no CR LF */
    m = 2 + su_memspn(p + 2, len - 2, bchars, bchars_len);
    if (m + 2 >= len)
      return NULL;
    crlf = p[m] == '\r' ? 1 + (p[m + 1] == '\n') : (p[m] == '\n');
    while (p[m - 1] == ' ' || p[m - 1] == '\t')
      m--;
    if (m > 2 && crlf) {
      boundary = su_alloc(home, 2 + m + 2 + 1);
      if (boundary) {
	memcpy(boundary, CR LF, 2);
	memcpy(boundary + 2, p, m);
	strcpy(boundary + m + 2, CR LF);
      }
      return boundary;
    }
  }

  /* Look for LF -- */
  for (;(p = memmem(p, end - p, LF "--", 3)); p += 3) {
    len = end - p;
    m = 3 + su_memspn(p + 3, len - 3, bchars, bchars_len);
    if (m + 2 >= len)
      return NULL;
    crlf = p[m] == '\r' ? 1 + (p[m + 1] == '\n') : (p[m] == '\n');
    while (p[m - 1] == ' ' || p[m - 1] == '\t')
      m--;
    m--;
    if (m > 2 && crlf) {
      boundary = su_alloc(home, 2 + m + 2 + 1);
      if (boundary) {
	memcpy(boundary, CR LF, 2);
	memcpy(boundary + 2, p + 1, m);
	strcpy(boundary + 2 + m, CR LF);
      }
      return boundary;
    }
  }

  return NULL;
}

/** Parse a MIME multipart.
 *
 * The function msg_multipart_parse() parses a MIME multipart message. The
 * common syntax of multiparts is described in @RFC2046 (section 7).
 *
 * @param[in,out] home home for allocating structures
 * @param[in]     c    content-type header for multipart
 * @param[in]     pl   payload structure for multipart
 *
 * After parsing, the @a pl will contain the plain-text preamble (if any).
 *
 * @note If no @b Content-Type header is given, the msg_multipart_parse()
 * tries to look for a suitable boundary. Currently, it takes first
 * boundary-looking string and uses that, so it can be fooled with, for
 * instance, signature @c "--Pekka".
 */
msg_multipart_t *msg_multipart_parse(su_home_t *home,
				     msg_content_type_t const *c,
				     msg_payload_t *pl)
{
  msg_multipart_t *mp = NULL, *all = NULL, **mmp = &all;
  /* Dummy msg object */
  msg_t msg[1] = {{{ SU_HOME_INIT(msg) }}};
  size_t len, m, blen;
  char *boundary, *p, *next, save;
  char *b, *end;
  msg_param_t param;

  p = pl->pl_data; len = pl->pl_len; end = p + len;

  su_home_init(msg_home(msg));
  msg->m_class = msg_multipart_mclass;
  msg->m_tail = &msg->m_chain;

  /* Get boundary from Content-Type */
  if (c && (param = msg_header_find_param(c->c_common, "boundary=")))
    boundary = msg_multipart_boundary(msg_home(msg), param);
  else
    boundary = msg_multipart_search_boundary(msg_home(msg), p, len);

  if (!boundary)
    return NULL;

  m = strlen(boundary) - 2, blen = m - 1;

  /* Find first delimiter */
  if (memcmp(boundary + 2, p, m - 2) == 0)
    b = p, p = p + m - 2, len -= m - 2;
  else if ((p = memmem(p, len, boundary + 1, m - 1))) {
    if (p != pl->pl_data && p[-1] == '\r')
      b = --p, p = p + m, len -= m;
    else
      b = p, p = p + m - 1, len -= m - 1;
  }
  else {
    su_home_deinit(msg_home(msg));
    return NULL;
  }

  /* Split multipart into parts */
  for (;;) {
    while (p[0] == ' ')
      p++;

    p += p[0] == '\r' ? 1 + (p[1] == '\n') : (p[0] == '\n');

    len = end - p;

    if (len < blen)
      break;

    next = memmem(p, len, boundary + 1, m = blen);

    if (!next)
      break;			/* error */

    if (next != p && next[-1] == '\r')
      next--, m++;

    mp = (msg_multipart_t *)msg_header_alloc(msg_home(msg), msg_multipart_class, 0);
    if (mp == NULL)
      break;			/* error */
    *mmp = mp; mmp = &mp->mp_next;

    /* Put delimiter transport-padding CRLF here */

	*b = '\0';
    mp->mp_common->h_len = p - b;
	b += strlen(boundary) - 2;
    mp->mp_common->h_data = b;

    /* .. and body-part here */
    mp->mp_data = p;
    mp->mp_len = next - p;

    if (next[m] == '-' && next[m + 1] == '-') {
      /* We found close-delimiter */
      assert(mp);
      if (!mp)
		  break;			/* error */
      mp->mp_close_delim = (msg_payload_t *)
		  msg_header_alloc(msg_home(msg), msg_payload_class, 0);
      if (!mp->mp_close_delim)
		  break;			/* error */
      /* Include also transport-padding and epilogue in the close-delimiter */
	  *next = '\0';
      mp->mp_close_delim->pl_len = p + len - next;
	  next += strlen(boundary) - 2;
      mp->mp_close_delim->pl_data = next;
	  
      break;
    }

    b = next; p = next + m;
  }

  if (!mp || !mp->mp_close_delim) {
    su_home_deinit(msg_home(msg));
    /* Delimiter error */
    return NULL;
  }

  /* Parse each part */
  for (mp = all; mp; mp = mp->mp_next) {
    msg->m_object = (msg_pub_t *)mp; p = mp->mp_data; next = p + mp->mp_len;

    if (msg->m_tail)
      mp->mp_common->h_prev = msg->m_tail,
	*msg->m_tail = (msg_header_t *)mp;

    msg->m_chain = (msg_header_t *)mp;
    msg->m_tail = &mp->mp_common->h_succ;

    save = *next; *next = '\0';	/* NUL-terminate this part */

    for (len = next - p; len > 0; len -= m, p += m) {
      if (IS_CRLF(p[0])) {
	m = msg_extract_separator(msg, (msg_pub_t*)mp, p, len, 1);
	assert(m > 0);

	p += m; len -= m;

	if (len > 0) {
	  m = msg_extract_payload(msg, (msg_pub_t*)mp, NULL, len, p, len, 1);
	  assert(m > 0);
	  assert(len == m);
	}
	break;
      }

      m = msg_extract_header(msg, (msg_pub_t*)mp, p, len, 1);

      if (m <= 0) {
	assert(m > 0);
	/* Xyzzy */
      }
    }

    *next = save; /* XXX - Should we leave the payload NUL-terminated? */
  }

  /* Postprocess */
  blen = strlen(boundary);

  for (mp = all; mp; mp = mp->mp_next) {
    mp->mp_data = boundary;
    mp->mp_len = (unsigned)blen; /* XXX */

    if (!(mp->mp_payload || mp->mp_separator)) continue;
	
    if (mp->mp_close_delim) {
      msg_header_t **tail;

      if (mp->mp_payload)
	tail = &mp->mp_payload->pl_common->h_succ;
      else
	tail = &mp->mp_separator->sep_common->h_succ;

      assert(msg->m_chain == (msg_header_t *)mp);
      assert(*tail == NULL);

      mp->mp_close_delim->pl_common->h_prev = tail;
      *tail = (msg_header_t *)mp->mp_close_delim;
    }
  }

  msg_fragment_clear(pl->pl_common);
  pl->pl_len = all->mp_data - (char *)pl->pl_data;

  su_home_move(home, msg_home(msg)); su_home_deinit(msg_home(msg));

  return all;
}

/**Add all missing parts to the multipart.
 *
 * Add missing components such as boundaries between body parts, separators
 * between body-part headers and data, and close-delimiter after last
 * body-part to the multipart message.
 *
 * @param[in,out] home home for allocating structures
 * @param[in,out] c    content-type header for multipart
 * @param[in,out] mp   pointer to first multipart structure
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 *
 * @ERRORS
 * @ERROR EBADMSG
 * The @b Content-Type header @a c is malformed, or multipart message
 * contains a malformed @b Content-Type header.
 * @ERROR ENOMEM
 * A memory allocation failed.
 * @ERROR EINVAL
 * The function msg_multipart_complete() was given invalid arguments.
 */
int msg_multipart_complete(su_home_t *home,
			   msg_content_type_t *c,
			   msg_multipart_t *mp)
{
  char *boundary;
  char const *b;
  size_t blen, m;

  if (c == NULL || mp == NULL)
    return (errno = EINVAL), -1;

  if (!(b = msg_header_find_param(c->c_common, "boundary="))) {
    /* Generate boundary */
    enum { tlen = 16 * 4 / 3 };
    char token[sizeof("boundary=") + tlen + 1];

    if (mp->mp_data) {
      b = mp->mp_data;
      m = mp->mp_len;

      if (strncmp(b, CR LF "--", 4) == 0)
	b += 4, m -= 4;
      else if (strncmp(b, "--", 2) == 0)
	b += 2, m -= 2;
      else
	return (errno = EBADMSG), -1;
      /* XXX - quoting? */
      b = su_sprintf(home, "boundary=\"%.*s\"", (int)m, b);
    }
    else {
      strcpy(token, "boundary=");
      msg_random_token(token + strlen("boundary="), (size_t)tlen, NULL, 0);
      b = su_strdup(home, token);
    }

    if (!b)
      return -1;

    msg_params_replace(home, (msg_param_t **)&c->c_params, b);

    b += strlen("boundary=");
  }

  if (!(boundary = msg_multipart_boundary(home, b)))
    return -1;

  blen = strlen(boundary); m = blen - 2;

  for (; mp; mp = mp->mp_next) {
    if (mp->mp_data == NULL) {
      mp->mp_data = boundary;
      mp->mp_len = (unsigned)blen; /* XXX */
    } else {
      if (mp->mp_len < 3)
	return -1;
      if (mp->mp_data[0] == '\r' && mp->mp_data[1] == '\n') {
	if (mp->mp_len < m || memcmp(mp->mp_data + 2, boundary + 2, m - 2))
	  return -1;
      } else if (mp->mp_data[0] == '\n') {
	if (mp->mp_len < m - 1 || memcmp(mp->mp_data + 1, boundary + 2, m - 2))
	  return -1;
      } else {
	if (mp->mp_len < m - 2 || memcmp(mp->mp_data, boundary + 2, m - 2))
	  return -1;
      }
    }

    if (mp->mp_next == NULL) {
      if (!mp->mp_close_delim)
	mp->mp_close_delim = msg_payload_format(home, "%.*s--" CR LF,
						(int)m, boundary);
      if (!mp->mp_close_delim)
	return -1;
    }
    else if (mp->mp_close_delim) {
      msg_payload_t *e = mp->mp_close_delim;

      mp->mp_close_delim = NULL;

      if (e->pl_common->h_prev)
	*e->pl_common->h_prev = e->pl_common->h_succ;
      if (e->pl_common->h_succ)
	e->pl_common->h_succ->sh_prev = e->pl_common->h_prev;
    }

    mp->mp_common->h_data = mp->mp_data;
    mp->mp_common->h_len = mp->mp_len;

    if (!mp->mp_separator)
      if (!(mp->mp_separator = msg_separator_make(home, CR LF)))
	return -1;

    if (mp->mp_multipart) {
      c = mp->mp_content_type;
      if (c == NULL)
	return (errno = EBADMSG), -1;

      if (msg_multipart_complete(home, c, mp->mp_multipart) < 0)
	return -1;
    }

    if (!mp->mp_payload)
      if (!(mp->mp_payload = msg_payload_create(home, NULL, 0)))
	return -1;
  }

  return 0;
}

/** Serialize a multipart message.
 *
 */
msg_header_t *msg_multipart_serialize(msg_header_t **head0,
				      msg_multipart_t *mp)
{
  msg_header_t *h_succ_all = NULL;
  msg_header_t *h, **head, **hh, *h0, *h_succ;
  void *hend;

#define is_in_chain(h) ((h) && ((msg_frg_t*)(h))->h_prev != NULL)
#define insert(head, h) \
  ((h)->sh_succ = *(head), *(head) = (h), \
   (h)->sh_prev = (head), (head) = &(h)->sh_succ)

  if (mp == NULL || head0 == NULL)
    return NULL;

  h_succ_all = *head0; head = head0;

  for (; mp; mp = mp->mp_next) {
    h0 = (msg_header_t *)mp;

    assert(mp->mp_separator); assert(mp->mp_payload);
    assert(mp->mp_next || mp->mp_close_delim);

    if (!mp->mp_separator || !mp->mp_payload ||
	(!mp->mp_next && !mp->mp_close_delim))
      return NULL;

    if ((void *)mp == h_succ_all)
      h_succ_all = NULL;

    *head0 = h0; h0->sh_prev = head;

    if (is_in_chain(mp->mp_separator))
      hend = mp->mp_separator;
    else if (is_in_chain(mp->mp_payload))
      hend = mp->mp_payload;
    else if (is_in_chain(mp->mp_multipart))
      hend = mp->mp_multipart;
    else if (is_in_chain(mp->mp_close_delim))
      hend = mp->mp_close_delim;
    else if (is_in_chain(mp->mp_next))
      hend = mp->mp_next;
    else
      hend = NULL;

    /* Search latest header in chain */
    for (head = &mp->mp_common->h_succ;
	 *head && *head != hend;
	 head = &(*head)->sh_succ)
      ;

    h_succ = *head;

    /* Serialize headers */
    for (hh = &((msg_pub_t*)mp)->msg_request;
	 (char *)hh < (char *)&mp->mp_separator;
	 hh++) {
      h = *hh; if (!h) continue;
      for (h = *hh; h; h = h->sh_next) {
	if (h == h_succ || !is_in_chain(h)) {
	  *head = h; h->sh_prev = head; head = &h->sh_succ;
	  while (*head && *head != hend)
	    head = &(*head)->sh_succ;
	  if (h == h_succ)
	    h_succ = *head;
	}
	else {
	  /* XXX Check that h is between head and hend */
	}
      }
    }

    if (!is_in_chain(mp->mp_separator)) {
      insert(head, (msg_header_t *)mp->mp_separator);
    } else {
      assert(h_succ == (msg_header_t *)mp->mp_separator);
      mp->mp_separator->sep_common->h_prev = head;
      *head = (msg_header_t *)mp->mp_separator;
      head = &mp->mp_separator->sep_common->h_succ;
      h_succ = *head;
    }

    if (!is_in_chain(mp->mp_payload)) {
      insert(head, (msg_header_t *)mp->mp_payload);
    } else {
      assert(h_succ == (msg_header_t *)mp->mp_payload);
      mp->mp_payload->pl_common->h_prev = head;
      *head = (msg_header_t *)mp->mp_payload;
      head = &mp->mp_payload->pl_common->h_succ;
      h_succ = *head;
    }

    if (mp->mp_multipart) {
      if ((*head = h_succ))
	h_succ->sh_prev = head;
      if (!(h = msg_multipart_serialize(head, mp->mp_multipart)))
	return NULL;
      head = &h->sh_succ; h_succ = *head;
    }

    if (mp->mp_close_delim) {
      if (!is_in_chain(mp->mp_close_delim)) {
	insert(head, (msg_header_t*)mp->mp_close_delim);
      } else {
	assert(h_succ == (msg_header_t *)mp->mp_close_delim);
	mp->mp_close_delim->pl_common->h_prev = head;
	*head = (msg_header_t *)mp->mp_close_delim;
	head = &mp->mp_close_delim->pl_common->h_succ;
      }

      if (h_succ_all)
	*head = h_succ_all, h_succ_all->sh_prev = head;

      return (msg_header_t *)mp->mp_close_delim;
    }

    *head = h_succ;

    head0 = head;
  }

  assert(!mp);

  return NULL;
}

/** Encode a multipart.
 *
 * @return The size of multipart in bytes, or -1 upon an error.
 */
issize_t msg_multipart_prepare(msg_t *msg, msg_multipart_t *mp, int flags)
{
  if (!mp || !mp->mp_data)
    return -1;

  if (!mp->mp_common->h_data ||
      mp->mp_common->h_len != mp->mp_len - 2 ||
      memcmp(mp->mp_common->h_data, mp->mp_data + 2, mp->mp_len - 2)) {
    mp->mp_common->h_data = mp->mp_data + 2;
    mp->mp_common->h_len = mp->mp_len - 2;
  }

  return msg_headers_prepare(msg, (msg_header_t *)mp, flags);
}

/** Decode a multipart. */
issize_t msg_multipart_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  su_home_t tmphome[1] = { SU_HOME_INIT(tmphome) };
  msg_payload_t pl[1];
  msg_multipart_t *mp, *result;

  assert(h && msg_is_multipart(h));

  msg_payload_init(pl);

  result = (msg_multipart_t *)h;

  pl->pl_data = s;
  pl->pl_len = slen;

  mp = msg_multipart_parse(tmphome, NULL, pl);

  if (mp) {
    *result = *mp;

    if (result->mp_common->h_succ->sh_prev)
      result->mp_common->h_succ->sh_prev =
	&result->mp_common->h_succ;

    su_free(tmphome, mp);

    su_home_move(home, tmphome);
  }

  su_home_deinit(tmphome);

  return mp ? 0 : -1;
}

/** Encode a multipart.
 *
 * Please note that here we just encode a element, the msg_multipart_t
 * itself.
 */
issize_t msg_multipart_e(char b[], isize_t bsiz, msg_header_t const *h, int flags)
{
  return msg_payload_e(b, bsiz, h, flags);
}

/** Calculate extra size of a multipart */
isize_t msg_multipart_dup_xtra(msg_header_t const *h, isize_t offset)
{
  msg_multipart_t const *mp = (msg_multipart_t *)h;
  msg_header_t const * const *hh;

  offset = msg_payload_dup_xtra(h, offset);

  for (hh = (msg_header_t const **)&((msg_pub_t *)mp)->msg_request;
       (char *)hh <= (char *)&mp->mp_close_delim;
       hh++) {
    for (h = *hh; h; h = h->sh_next) {
      MSG_STRUCT_SIZE_ALIGN(offset);
      offset = h->sh_class->hc_dxtra(h, offset + h->sh_class->hc_size);
    }
  }

  return offset;
}

/** Duplicate one msg_multipart_t object */
char *msg_multipart_dup_one(msg_header_t *dst, msg_header_t const *src,
			    char *b, isize_t xtra)
{
  msg_multipart_t const *mp = (msg_multipart_t *)src;
  msg_header_t *h, **hh;
  char *end = b + xtra;

  b = msg_payload_dup_one(dst, src, b, xtra);

  for (hh = &((msg_pub_t*)mp)->msg_request;
       (char *)hh <= (char *)&mp->mp_close_delim;
       hh++) {
    for (h = *hh; h; h = h->sh_next) {
      MSG_STRUCT_ALIGN(b);
      dst = (msg_header_t *)b;
      memset(dst, 0, sizeof dst->sh_common);
      dst->sh_class = h->sh_class;
      b = h->sh_class->hc_dup_one(dst, h, b + h->sh_class->hc_size, end - b);
      if (h->sh_class->hc_update)
	msg_header_update_params(h->sh_common, 0);
      assert(b <= end);
    }
  }

  return b;
}

#if 0
msg_hclass_t msg_multipart_class[] =
MSG_HEADER_CLASS(msg_, multipart, NULL, "", mp_common, append, msg_multipart);
#endif

/**Calculate Q value.
 *
 * The function msg_q_value() converts q-value string @a q to numeric value
 * in range (0..1000).  Q values are used, for instance, to describe
 * relative priorities of registered contacts.
 *
 * @param q q-value string ("1" | "." 1,3DIGIT)
 *
 * @return
 * The function msg_q_value() returns an integer in range 0 .. 1000.
 */
unsigned msg_q_value(char const *q)
{
  unsigned value = 0;

  if (!q)
    return 500;
  if (q[0] != '0' && q[0] != '.' && q[0] != '1')
    return 500;
  while (q[0] == '0')
    q++;
  if (q[0] >= '1' && q[0] <= '9')
    return 1000;
  if (q[0] == '\0')
    return 0;
  if (q[0] != '.')
    /* Garbage... */
    return 500;

  if (q[1] >= '0' && q[1] <= '9') {
    value = (q[1] - '0') * 100;
    if (q[2] >= '0' && q[2] <= '9') {
      value += (q[2] - '0') * 10;
      if (q[3] >= '0' && q[3] <= '9') {
	value += (q[3] - '0');
	if (q[4] > '5' && q[4] <= '9')
	  /* Round upwards */
	  value += 1;
	else if (q[4] == '5')
	  value += value & 1; /* Round to even */
      }
    }
  }

  return value;
}

/** Parse media type (type/subtype).
 *
 * The function msg_mediatype_d() parses a mediatype string.
 *
 * @param[in,out] ss    string to be parsed
 * @param[out]    type  value result for media type
 *
 * @retval 0 when successful,
 * @retval -1 upon an error.
 */
issize_t msg_mediatype_d(char **ss, char const **type)
{
  char *s = *ss;
  char const *result = s;
  size_t l1 = 0, l2 = 0, n;

  /* Media type consists of two tokens, separated by / */

  l1 = span_token(s);
  for (n = l1; IS_LWS(s[n]); n++)
    {}
  if (s[n] == '/') {
    for (n++; IS_LWS(s[n]); n++)
      {}
    l2 = span_token(s + n);
    n += l2;
  }

  if (l1 == 0 || l2 == 0)
    return -1;

  /* If there is extra ws between tokens, compact version */
  if (n > l1 + 1 + l2) {
    s[l1] = '/';
    memmove(s + l1 + 1, s + n - l2, l2);
    s[l1 + 1 + l2] = 0;
  }

  s += n;

  while (IS_WS(*s)) *s++ = '\0';

  *ss = s;

  if (type)
    *type = result;

  return 0;
}

/* ====================================================================== */

/**@ingroup msg_mime
 * @defgroup msg_accept Accept Header
 *
 * The @b Accept request-header field can be used to specify certain media
 * types which are acceptable for the response. Its syntax is defined in
 * [H14.1, S20.1] as follows:
 *
 * @code
 *    Accept         = "Accept" ":" #( media-range [ accept-params ] )
 *
 *    media-range    = ( "*" "/" "*"
 *                     | ( type "/" "*" )
 *                     | ( type "/" subtype ) ) *( ";" parameter )
 *
 *    accept-params  = ";" "q" "=" qvalue *( accept-extension )
 *
 *    accept-extension = ";" token [ "=" ( token | quoted-string ) ]
 * @endcode
 *
 */

/**@ingroup msg_accept
 * @typedef typedef struct msg_accept_s msg_accept_t;
 *
 * The structure msg_accept_t contains representation of an @b Accept
 * header.
 *
 * The msg_accept_t is defined as follows:
 * @code
 * typedef struct msg_accept_s {
 *   msg_common_t        ac_common[1]; // Common fragment info
 *   msg_accept_t       *ac_next;      // Pointer to next Accept header
 *   char const         *ac_type;      // Pointer to type/subtype
 *   char const         *ac_subtype;   // Points after first slash in type
 *   msg_param_t const  *ac_params;    // List of parameters
 *   msg_param_t         ac_q;         // Value of q parameter
 * } msg_accept_t;
 * @endcode
 */

msg_hclass_t msg_accept_class[] =
MSG_HEADER_CLASS(msg_, accept, "Accept", "", ac_params, apndlist,
		 msg_accept, msg_accept);

issize_t msg_accept_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
	msg_accept_t *ac;

	for(;;) {
		ac = (msg_accept_t *)h;

		while (*s == ',')   /* Ignore empty entries (comma-whitespace) */
			*s = '\0', s += span_lws(s + 1) + 1;

		if (*s == '\0') {
			/* Empty Accept list is not an error */
			ac->ac_type = ac->ac_subtype = "";
			return 0;
		}

		/* "Accept:" #(type/subtyp ; *(parameters))) */
		if (msg_mediatype_d(&s, &ac->ac_type) == -1)
			return -1;
		if (!(ac->ac_subtype = strchr(ac->ac_type, '/')))
			return -1;
		ac->ac_subtype++;

		if (*s == ';' && msg_params_d(home, &s, &ac->ac_params) == -1)
			return -1;
  
		msg_parse_next_field_without_recursion();
	}

}

issize_t msg_accept_e(char b[], isize_t bsiz, msg_header_t const *h, int flags)
{
  char *b0 = b, *end = b + bsiz;
  msg_accept_t const *ac = (msg_accept_t *)h;

  assert(msg_is_accept(h));

  if (ac->ac_type) {
    MSG_STRING_E(b, end, ac->ac_type);
    MSG_PARAMS_E(b, end, ac->ac_params, flags);
  }
  MSG_TERM_E(b, end);

  return b - b0;
}

isize_t msg_accept_dup_xtra(msg_header_t const *h, isize_t offset)
{
  msg_accept_t const *ac = (msg_accept_t *)h;

  if (ac->ac_type) {
    MSG_PARAMS_SIZE(offset, ac->ac_params);
    offset += MSG_STRING_SIZE(ac->ac_type);
  }

  return offset;
}

/** Duplicate one msg_accept_t object */
char *msg_accept_dup_one(msg_header_t *dst, msg_header_t const *src,
		      char *b, isize_t xtra)
{
  msg_accept_t *ac = (msg_accept_t *)dst;
  msg_accept_t const *o = (msg_accept_t *)src;
  char *end = b + xtra;

  if (o->ac_type) {
    b = msg_params_dup(&ac->ac_params, o->ac_params, b, xtra);
    MSG_STRING_DUP(b, ac->ac_type, o->ac_type);
    if ((ac->ac_subtype = strchr(ac->ac_type, '/')))
      ac->ac_subtype++;
  }

  assert(b <= end); (void)end;

  return b;
}

/** Update parameter(s) for Accept header. */
int msg_accept_update(msg_common_t *h,
		      char const *name, isize_t namelen,
		      char const *value)
{
  msg_accept_t *ac = (msg_accept_t *)h;

  if (name == NULL) {
    ac->ac_q = NULL;
  }
  else if (namelen == 1 && su_casenmatch(name, "q", 1)) {
    /* XXX - check for invalid value? */
    ac->ac_q = value;
  }

  return 0;
}

/* ====================================================================== */

/** Decode an Accept-* header. */
issize_t msg_accept_any_d(su_home_t *home,
			  msg_header_t *h,
			  char *s, isize_t slen)
{
  /** @relatesalso msg_accept_any_s */
	msg_accept_any_t *aa;

	for(;;) {
		aa = (msg_accept_any_t *)h;
		while (*s == ',')   /* Ignore empty entries (comma-whitespace) */
			*s = '\0', s += span_lws(s + 1) + 1;

		if (*s == '\0')
			return -2;			/* Empty list */

		/* "Accept-*:" 1#(token *(SEMI accept-param)) */
		if (msg_token_d(&s, &aa->aa_value) == -1)
			return -1;

		if (*s == ';' && msg_params_d(home, &s, &aa->aa_params) == -1)
			return -1;
		
		msg_parse_next_field_without_recursion();
	}

}

/** Encode an Accept-* header field. */
issize_t msg_accept_any_e(char b[], isize_t bsiz, msg_header_t const *h, int f)
{
  /** @relatesalso msg_accept_any_s */
  char *b0 = b, *end = b + bsiz;
  msg_accept_any_t const *aa = (msg_accept_any_t *)h;

  MSG_STRING_E(b, end, aa->aa_value);
  MSG_PARAMS_E(b, end, aa->aa_params, flags);
  MSG_TERM_E(b, end);

  return b - b0;
}

/** Calculate extra memory used by accept-* headers. */
isize_t msg_accept_any_dup_xtra(msg_header_t const *h, isize_t offset)
{
  /** @relatesalso msg_accept_any_s */
  msg_accept_any_t const *aa = (msg_accept_any_t *)h;

  MSG_PARAMS_SIZE(offset, aa->aa_params);
  offset += MSG_STRING_SIZE(aa->aa_value);

  return offset;
}

/** Duplicate one msg_accept_any_t object. */
char *msg_accept_any_dup_one(msg_header_t *dst, msg_header_t const *src,
			     char *b, isize_t xtra)
{
  /** @relatesalso msg_accept_any_s */
  msg_accept_any_t *aa = (msg_accept_any_t *)dst;
  msg_accept_any_t const *o = (msg_accept_any_t *)src;
  char *end = b + xtra;

  b = msg_params_dup(&aa->aa_params, o->aa_params, b, xtra);
  MSG_STRING_DUP(b, aa->aa_value, o->aa_value);

  assert(b <= end); (void)end;

  return b;
}

/** Update parameter(s) for Accept-* header. */
int msg_accept_any_update(msg_common_t *h,
			  char const *name, isize_t namelen,
			  char const *value)
{
  msg_accept_any_t *aa = (msg_accept_any_t *)h;

  if (name == NULL) {
    aa->aa_q = NULL;
  }
  else if (namelen == 1 && su_casenmatch(name, "q", 1)) {
    aa->aa_q = value;
  }

  return 0;
}

/* ====================================================================== */

/**@ingroup msg_mime
 * @defgroup msg_accept_charset Accept-Charset Header
 *
 * The Accept-Charset header is similar to Accept, but restricts the
 * character set that are acceptable in the response.  Its syntax is
 * defined in [H14.2] as follows:
 *
 * @code
 *    Accept-Charset = "Accept-Charset" ":"
 *            1#( ( charset | "*" )[ ";" "q" "=" qvalue ] )
 * @endcode
 *
 */

/**@ingroup msg_accept_charset
 * @typedef typedef struct msg_accept_charset_s msg_accept_charset_t;
 *
 * The structure msg_accept_encoding_t contains representation of @b
 * Accept-Charset header.
 *
 * The msg_accept_charset_t is defined as follows:
 * @code
 * typedef struct {
 *   msg_common_t        aa_common[1]; // Common fragment info
 *   msg_accept_any_t   *aa_next;      // Pointer to next Accept-Charset
 *   char const         *aa_value;     // Charset
 *   msg_param_t const  *aa_params;    // Parameter list
 *   char const         *aa_q;	       // Q-value
 * } msg_accept_charset_t;
 * @endcode
 */

msg_hclass_t msg_accept_charset_class[1] =
 MSG_HEADER_CLASS(msg_, accept_charset, "Accept-Charset", "",
		  aa_params, apndlist, msg_accept_any, msg_accept_any);

issize_t msg_accept_charset_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  return msg_accept_any_d(home, h, s, slen);
}

issize_t msg_accept_charset_e(char b[], isize_t bsiz, msg_header_t const *h, int f)
{
  assert(msg_is_accept_charset(h));
  return msg_accept_any_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@ingroup msg_mime
 * @defgroup msg_accept_encoding Accept-Encoding Header
 *
 * The Accept-Encoding header is similar to Accept, but restricts the
 * content-codings that are acceptable in the response.  Its syntax is
 * defined in [H14.3, S20.2] as follows:
 *
 * @code
 *    Accept-Encoding  = "Accept-Encoding" ":"
 *                       1#( codings [ ";" "q" "=" qvalue ] )
 *    codings          = ( content-coding | "*" )
 *    content-coding   = token
 * @endcode
 *
 */

/**@ingroup msg_accept_encoding
 * @typedef typedef struct msg_accept_encoding_s msg_accept_encoding_t;
 *
 * The structure msg_accept_encoding_t contains representation of @b
 * Accept-Encoding header.
 *
 * The msg_accept_encoding_t is defined as follows:
 * @code
 * typedef struct {
 *   msg_common_t        aa_common[1]; // Common fragment info
 *   msg_accept_any_t   *aa_next;      // Pointer to next Accept-Encoding
 *   char const         *aa_value;     // Content-coding
 *   msg_param_t const  *aa_params;    // Parameter list
 *   char const         *aa_q;	       // Q-value
 * } msg_accept_encoding_t;
 * @endcode
 */

msg_hclass_t msg_accept_encoding_class[1] =
 MSG_HEADER_CLASS(msg_, accept_encoding, "Accept-Encoding", "",
		  aa_params, apndlist, msg_accept_any, msg_accept_any);

issize_t msg_accept_encoding_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  return msg_accept_any_d(home, h, s, slen);
}

issize_t msg_accept_encoding_e(char b[], isize_t bsiz, msg_header_t const *h, int f)
{
  return msg_accept_any_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@ingroup msg_mime
 * @defgroup msg_accept_language Accept-Language Header
 *
 * The Accept-Language header allows the client to indicate to the server in
 * which language it would prefer to receive reason phrases, session
 * descriptions or status responses carried as message bodies. Its syntax is
 * defined in [H14.4, S20.3] as follows:
 *
 * @code
 *    Accept-Language = "Accept-Language" ":"
 *                      1#( language-range [ ";" "q" "=" qvalue ] )
 *
 *    language-range  = ( ( 1*8ALPHA *( "-" 1*8ALPHA ) ) | "*" )
 * @endcode
 *
 */

/**@ingroup msg_accept_language
 * @typedef typedef struct msg_accept_language_s msg_accept_language_t;
 *
 * The structure msg_accept_language_t contains representation of @b
 * Accept-Language header.
 *
 * The msg_accept_language_t is defined as follows:
 * @code
 * typedef struct {
 *   msg_common_t        aa_common[1]; // Common fragment info
 *   msg_accept_any_t   *aa_next;      // Pointer to next Accept-Encoding
 *   char const         *aa_value;     // Language-range
 *   msg_param_t const  *aa_params;    // Parameter list
 *   char const         *aa_q;	       // Q-value
 * } msg_accept_language_t;
 * @endcode
 */

msg_hclass_t msg_accept_language_class[1] =
 MSG_HEADER_CLASS(msg_, accept_language, "Accept-Language", "",
		  aa_params, apndlist, msg_accept_any, msg_accept_any);

issize_t msg_accept_language_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  return msg_accept_any_d(home, h, s, slen);
}

issize_t msg_accept_language_e(char b[], isize_t bsiz, msg_header_t const *h, int f)
{
  assert(msg_is_accept_language(h));
  return msg_accept_any_e(b, bsiz, h, f);
}


/* ====================================================================== */

/**@ingroup msg_mime
 * @defgroup msg_content_disposition Content-Disposition Header
 *
 * The Content-Disposition header field describes how the message body or,
 * in the case of multipart messages, a message body part is to be
 * interpreted by the UAC or UAS.  Its syntax is defined in [S20.11]
 * as follows:
 *
 * @code
 *    Content-Disposition   =  "Content-Disposition" ":"
 *                             disposition-type *( ";" disposition-param )
 *    disposition-type      =  "render" | "session" | "icon" | "alert"
 *                         |   disp-extension-token
 *    disposition-param     =  "handling" "="
 *                             ( "optional" | "required" | other-handling )
 *                         |   generic-param
 *    other-handling        =  token
 *    disp-extension-token  =  token
 * @endcode
 *
 * The Content-Disposition header was extended by
 * draft-lennox-sip-reg-payload-01.txt section 3.1 as follows:
 *
 * @code
 *    Content-Disposition      =  "Content-Disposition" ":"
 *                                disposition-type *( ";" disposition-param )
 *    disposition-type        /=  "script" | "sip-cgi" | token
 *    disposition-param       /=  action-param
 *                             /  modification-date-param
 *    action-param             =  "action" "=" action-value
 *    action-value             =  "store" | "remove" | token
 *    modification-date-param  =  "modification-date" "=" quoted-date-time
 *    quoted-date-time         =  <"> SIP-date <">
 * @endcode
 */

/**@ingroup msg_content_disposition
 * @typedef struct msg_content_disposition_s msg_content_disposition_t;
 *
 * The structure msg_content_disposition_t contains representation of an @b
 * Content-Disposition header.
 *
 * The msg_content_disposition_t is defined as follows:
 * @code
 * typedef struct msg_content_disposition_s
 * {
 *   msg_common_t       cd_common[1];  // Common fragment info
 *   msg_error_t       *cd_next;       // Link to next (dummy)
 *   char const        *cd_type;       // Disposition type
 *   msg_param_t const *cd_params;     // List of parameters
 *   msg_param_t        cd_handling;   // Value of @b handling parameter
 *   unsigned           cd_required:1; // True if handling=required
 *   unsigned           cd_optional:1; // True if handling=optional
 * } msg_content_disposition_t;
 * @endcode
 */

msg_hclass_t msg_content_disposition_class[] =
MSG_HEADER_CLASS(msg_, content_disposition, "Content-Disposition", "",
		 cd_params, single, msg_content_disposition,
		 msg_content_disposition);

issize_t msg_content_disposition_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  msg_content_disposition_t *cd = (msg_content_disposition_t *)h;

  if (msg_token_d(&s, &cd->cd_type) < 0 ||
      (*s == ';' && msg_params_d(home, &s, &cd->cd_params) < 0))
      return -1;

  if (cd->cd_params)
    msg_header_update_params(cd->cd_common, 0);

  return 0;
}

issize_t msg_content_disposition_e(char b[], isize_t bsiz, msg_header_t const *h, int f)
{
  char *b0 = b, *end = b + bsiz;
  msg_content_disposition_t const *cd = (msg_content_disposition_t *)h;

  assert(msg_is_content_disposition(h));

  MSG_STRING_E(b, end, cd->cd_type);
  MSG_PARAMS_E(b, end, cd->cd_params, f);

  MSG_TERM_E(b, end);

  return b - b0;
}

isize_t msg_content_disposition_dup_xtra(msg_header_t const *h, isize_t offset)
{
  msg_content_disposition_t const *cd = (msg_content_disposition_t *)h;

  MSG_PARAMS_SIZE(offset, cd->cd_params);
  offset += MSG_STRING_SIZE(cd->cd_type);

  return offset;
}

/** Duplicate one msg_content_disposition_t object */
char *msg_content_disposition_dup_one(msg_header_t *dst,
				     msg_header_t const *src,
				     char *b, isize_t xtra)
{
  msg_content_disposition_t *cd = (msg_content_disposition_t *)dst;
  msg_content_disposition_t const *o = (msg_content_disposition_t *)src;
  char *end = b + xtra;

  b = msg_params_dup(&cd->cd_params, o->cd_params, b, xtra);
  MSG_STRING_DUP(b, cd->cd_type, o->cd_type);

  assert(b <= end); (void)end;

  return b;
}

/** Update Content-Disposition parameters */
int msg_content_disposition_update(msg_common_t *h,
				   char const *name, isize_t namelen,
				   char const *value)
{
  msg_content_disposition_t *cd = (msg_content_disposition_t *)h;

  if (name == NULL) {
    cd->cd_handling = NULL, cd->cd_required = 0, cd->cd_optional = 0;
  }
  else if (namelen == strlen("handling") &&
	   su_casenmatch(name, "handling", namelen)) {
    cd->cd_handling = value;
    cd->cd_required = su_casematch(value, "required");
    cd->cd_optional = su_casematch(value, "optional");
  }

  return 0;
}

/* ====================================================================== */

/**@ingroup msg_mime
 * @defgroup msg_content_encoding Content-Encoding Header
 *
 * The Content-Encoding header indicates what additional content codings
 * have been applied to the entity-body. Its syntax is defined in [H14.11]
 * and [S20.12] as follows:
 *
 * @code
 *    Content-Encoding = ( "Content-Encoding" / "e" ) ":" 1#content-coding
 *    content-coding   = token
 * @endcode
 */

/**@ingroup msg_content_encoding
 * @typedef struct msg_list_s msg_content_encoding_t;
 *
 * The structure msg_content_encoding_t contains representation of an @b
 * Content-Encoding header.
 *
 * The msg_content_encoding_t is defined as follows:
 * @code
 * typedef struct msg_list_s
 * {
 *   msg_common_t       k_common[1];  // Common fragment info
 *   msg_list_t        *k_next;	      // Link to next header
 *   msg_param_t       *k_items;      // List of items
 * } msg_content_encoding_t;
 * @endcode
 */

msg_hclass_t msg_content_encoding_class[] =
  MSG_HEADER_CLASS_LIST(content_encoding, "Content-Encoding", "e", list);

issize_t msg_content_encoding_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  msg_content_encoding_t *e = (msg_content_encoding_t *)h;
  return msg_commalist_d(home, &s, &e->k_items, msg_token_scan);
}

issize_t msg_content_encoding_e(char b[], isize_t bsiz, msg_header_t const *h, int f)
{
  assert(msg_is_content_encoding(h));
  return msg_list_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@ingroup msg_mime
 * @defgroup msg_content_language Content-Language Header
 *
 * The Content-Language header describes the natural language(s) of the
 * intended audience for the enclosed message body. Note that this might not
 * be equivalent to all the languages used within the message-body. Its
 * syntax is defined in [H14.12, S20.13] as follows:
 *
 * @code
 *    Content-Language  = "Content-Language" ":" 1#language-tag
 * @endcode
 * or
 * @code
 *    Content-Language  =  "Content-Language" HCOLON
 *                         language-tag *(COMMA language-tag)
 *    language-tag      =  primary-tag *( "-" subtag )
 *    primary-tag       =  1*8ALPHA
 *    subtag            =  1*8ALPHA
 * @endcode
 *
 */

/**@ingroup msg_content_language
 * @typedef typedef struct msg_content_language_s msg_content_language_t;
 *
 * The structure msg_content_language_t contains representation of @b
 * Content-Language header.
 *
 * The msg_content_language_t is defined as follows:
 * @code
 * typedef struct {
 *   msg_common_t            k_common[1]; // Common fragment info
 *   msg_content_language_t *k_next;      // (Content-Encoding header)
 *   msg_param_t            *k_items;     // List of languages
 * } msg_content_language_t;
 * @endcode
 */

msg_hclass_t msg_content_language_class[] =
MSG_HEADER_CLASS_LIST(content_language, "Content-Language", "", list);

issize_t msg_content_language_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  msg_content_language_t *k = (msg_content_language_t *)h;
  return msg_commalist_d(home, &s, &k->k_items, msg_token_scan);
}

issize_t msg_content_language_e(char b[], isize_t bsiz, msg_header_t const *h, int f)
{
  assert(msg_is_content_language(h));
  return msg_list_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@ingroup msg_mime
 * @defgroup msg_content_length Content-Length Header
 *
 * The Content-Length header indicates the size of the message-body in
 * decimal number of octets.  Its syntax is defined in [S10.18] as
 * follows:
 *
 * @code
 *    Content-Length  =  ( "Content-Length" / "l" ) HCOLON 1*DIGIT
 * @endcode
 *
 */

/**@ingroup msg_content_length
 * @typedef typedef struct msg_content_length_s msg_content_length_t;
 *
 * The structure msg_content_length_t contains representation of a
 * Content-Length header.
 *
 * The msg_content_length_t is defined as follows:
 * @code
 * typedef struct msg_content_length_s {
 *   msg_common_t   l_common[1];        // Common fragment info
 *   msg_error_t   *l_next;             // Link to next (dummy)
 *   unsigned long  l_length;           // Numeric value
 * } msg_content_length_t;
 * @endcode
 */

#define msg_content_length_d msg_numeric_d
#define msg_content_length_e msg_numeric_e

msg_hclass_t msg_content_length_class[] =
MSG_HEADER_CLASS(msg_, content_length, "Content-Length", "l",
		 l_common, single_critical, msg_default, msg_generic);

/**@ingroup msg_content_length
 * Create a @b Content-Length header object.
 *
 * The function msg_content_length_create() creates a Content-Length
 * header object with the value @a n.  The memory for the header is
 * allocated from the memory home @a home.
 *
 * @param home  memory home
 * @param n     payload size in bytes
 *
 * @return
 * The function msg_content_length_create() returns a pointer to newly
 * created @b Content-Length header object when successful or NULL upon
 * an error.
 */
msg_content_length_t *msg_content_length_create(su_home_t *home, uint32_t n)
{
  msg_content_length_t *l = (msg_content_length_t *)
    msg_header_alloc(home, msg_content_length_class, 0);

  if (l)
    l->l_length = n;

  return l;
}


/* ====================================================================== */

/**@ingroup msg_mime
 * @defgroup msg_content_md5 Content-MD5 Header
 *
 * The Content-MD5 header is an MD5 digest of the entity-body for the
 * purpose of providing an end-to-end message integrity check (MIC) of the
 * message-body. Its syntax is defined in [@RFC1864, H14.15] as follows:
 *
 * @code
 *      Content-MD5   = "Content-MD5" ":" md5-digest
 *      md5-digest   = <base64 of 128 bit MD5 digest as per @RFC1864>
 * @endcode
 */

/**@ingroup msg_content_md5
 * @typedef struct msg_generic_s msg_content_md5_t;
 *
 * The structure msg_content_md5_t contains representation of an @b
 * Content-MD5 header.
 *
 * The msg_content_md5_t is defined as follows:
 * @code
 * typedef struct msg_generic_s
 * {
 *   msg_common_t   g_common[1];    // Common fragment info
 *   msg_generic_t *g_next;	    // Link to next header
 *   char const    *g_string;	    // Header value
 * } msg_content_md5_t;
 * @endcode
 */

#define msg_content_md5_d msg_generic_d
#define msg_content_md5_e msg_generic_e
msg_hclass_t msg_content_md5_class[] =
MSG_HEADER_CLASS_G(content_md5, "Content-MD5", "", single);

/* ====================================================================== */

/**@ingroup msg_mime
 * @defgroup msg_content_id Content-ID Header
 *
 * The Content-ID header is an unique identifier of an entity-body. The
 * Content-ID value may be used for uniquely identifying MIME entities in
 * several contexts, particularly for caching data referenced by the
 * message/external-body mechanism. Its syntax is defined in [RFC2045] as
 * follows:
 *
 * @code
 *      Content-ID   = "Content-ID" ":" msg-id
 *      msg-id       = [CFWS] "<" id-left "@" id-right ">" [CFWS]
 *      id-left      = dot-atom-text / no-fold-quote / obs-id-left
 *      id-right     = dot-atom-text / no-fold-literal / obs-id-right
 * @endcode
 */

/**@ingroup msg_content_id
 * @typedef msg_generic_t msg_content_id_t;
 * Content-ID Header Structure.
 * @code
 * typedef struct
 * {
 *   msg_common_t      g_common[1];    // Common fragment info
 *   msg_content_id_t *g_next;	       // Link to next header
 *   char const       *g_string;       // Header value
 * }
 * @endcode
 */

#define msg_content_id_d msg_generic_d
#define msg_content_id_e msg_generic_e
msg_hclass_t msg_content_id_class[] =
MSG_HEADER_CLASS_G(content_id, "Content-ID", "", single);

/* ====================================================================== */

/**@ingroup msg_mime
 * @defgroup msg_content_type Content-Type Header
 *
 * The @b Content-Type header indicates the media type of the message-body
 * sent to the recipient. Its syntax is defined in [H3.7, S20.15]
 * as follows:
 *
 * @code
 *    Content-Type  = ( "Content-Type" | "c" ) ":" media-type
 *    media-type    = type "/" subtype *( ";" parameter )
 *    type          = token
 *    subtype       = token
 * @endcode
 */

/**@ingroup msg_content_type
 * @typedef typedef struct msg_content_type_s msg_content_type_t;
 *
 * The structure msg_content_type_t contains representation of @b
 * Content-Type header.
 *
 * The msg_content_type_t is defined as follows:
 * @code
 * typedef struct msg_content_type_s {
 *   msg_common_t        c_common[1];  // Common fragment info
 *   msg_unknown_t      *c_next;       // Dummy link to next
 *   char const         *c_type;       // Pointer to type/subtype
 *   char const         *c_subtype;    // Points after first slash in type
 *   msg_param_t const  *c_params;     // List of parameters
 * } msg_content_type_t;
 * @endcode
 *
 * The @a c_type is always void of whitespace, that is, there is no
 * whitespace around the slash.
 */

#define msg_content_type_update NULL

msg_hclass_t msg_content_type_class[] =
MSG_HEADER_CLASS(msg_, content_type, "Content-Type", "c", c_params,
		 single, msg_content_type, msg_content_type);

issize_t msg_content_type_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  msg_content_type_t *c;

  assert(h);

  c = (msg_content_type_t *)h;

  /* "Content-type:" type/subtyp *(; parameter))) */
  if (msg_mediatype_d(&s, &c->c_type) == -1 || /* compacts token / token */
      (c->c_subtype = strchr(c->c_type, '/')) == NULL ||
      (*s == ';' && msg_params_d(home, &s, &c->c_params) == -1) ||
      (*s != '\0'))
    return -1;

  c->c_subtype++;

  return 0;
}

issize_t msg_content_type_e(char b[], isize_t bsiz, msg_header_t const *h, int flags)
{
  char *b0 = b, *end = b + bsiz;
  msg_content_type_t const *c = (msg_content_type_t *)h;

  assert(msg_is_content_type(h));

  MSG_STRING_E(b, end, c->c_type);
  MSG_PARAMS_E(b, end, c->c_params, flags);
  MSG_TERM_E(b, end);

  return b - b0;
}

isize_t msg_content_type_dup_xtra(msg_header_t const *h, isize_t offset)
{
  msg_content_type_t const *c = (msg_content_type_t *)h;

  MSG_PARAMS_SIZE(offset, c->c_params);
  offset += MSG_STRING_SIZE(c->c_type);

  return offset;
}

/** Duplicate one msg_content_type_t object */
char *msg_content_type_dup_one(msg_header_t *dst, msg_header_t const *src,
			       char *b, isize_t xtra)
{
  msg_content_type_t *c = (msg_content_type_t *)dst;
  msg_content_type_t const *o = (msg_content_type_t *)src;
  char *end = b + xtra;

  b = msg_params_dup(&c->c_params, o->c_params, b, xtra);
  MSG_STRING_DUP(b, c->c_type, o->c_type);

  c->c_subtype = c->c_type ? strchr(c->c_type, '/') : NULL;
  if (c->c_subtype)
    c->c_subtype++;

  assert(b <= end); (void)end;

  return b;
}

/* ====================================================================== */

/**@ingroup msg_mime
 * @defgroup msg_mime_version MIME-Version Header
 *
 * MIME-Version header indicates what version of the protocol was used
 * to construct the message.  Its syntax is defined in [H19.4.1, S20.24]
 * as follows:
 *
 * @code
 *    MIME-Version   = "MIME-Version" ":" 1*DIGIT "." 1*DIGIT
 * @endcode
 */

/**@ingroup msg_mime_version
 * @typedef struct msg_generic_s msg_mime_version_t;
 *
 * The structure msg_mime_version_t contains representation of an @b
 * MIME-Version header.
 *
 * The msg_mime_version_t is defined as follows:
 * @code
 * typedef struct msg_generic_s
 * {
 *   msg_common_t   g_common[1];    // Common fragment info
 *   msg_generic_t *g_next;	    // Link to next header
 *   char const    *g_string;	    // Header value
 * } msg_mime_version_t;
 * @endcode
 */

msg_hclass_t msg_mime_version_class[] =
MSG_HEADER_CLASS_G(mime_version, "MIME-Version", "", single);

issize_t msg_mime_version_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  return msg_generic_d(home, h, s, slen);
}

issize_t msg_mime_version_e(char b[], isize_t bsiz, msg_header_t const *h, int f)
{
  assert(msg_is_mime_version(h));
  return msg_generic_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@ingroup msg_mime
 * @defgroup msg_content_location Content-Location Header
 *
 *
 */

/**@ingroup msg_content_location
 * @typedef struct msg_generic_s msg_content_location_t;
 *
 * The structure msg_content_location_t contains representation of an @b
 * Content-Location header.
 *
 * The msg_content_location_t is defined as follows:
 * @code
 * typedef struct msg_generic_s
 * {
 *   msg_common_t   g_common[1];    // Common fragment info
 *   msg_generic_t *g_next;	    // Link to next header
 *   char const    *g_string;	    // Header value
 * } msg_content_location_t;
 * @endcode
 */

#define msg_content_location_d msg_generic_d
#define msg_content_location_e msg_generic_e
msg_hclass_t msg_content_location_class[] =
MSG_HEADER_CLASS_G(content_location, "Content-Location", "", single);


/* ====================================================================== */
#if 0
/**@ingroup msg_mime
 * @defgroup msg_content_base Content-Base Header
 *
 * @RFC2617:
 * Content-Base was deleted from the specification: it was not
 * implemented widely, and there is no simple, safe way to introduce it
 * without a robust extension mechanism. In addition, it is used in a
 * similar, but not identical fashion in MHTML [45].
 *
 */


/**@ingroup msg_content_base
 * @typedef msg_generic_t msg_content_base_t;
 * Content-Base Header Structure.
 * @code
 * typedef struct
 * {
 *   msg_common_t        g_common[1];    // Common fragment info
 *   msg_content_base_t *g_next;	 // Link to next header
 *   char const         *g_string;       // Header value
 * }
 * @endcode
 */

#define msg_content_base_d msg_generic_d
#define msg_content_base_e msg_generic_e
msg_hclass_t msg_content_base_class[] =
MSG_HEADER_CLASS_G(content_base, "Content-Base", "", single);

#endif

/* ====================================================================== */

/**@ingroup msg_mime
 * @defgroup msg_content_transfer_encoding Content-Transfer-Encoding Header
 *
 *
 */

/**@ingroup msg_content_transfer_encoding
 * @typedef struct msg_generic_s msg_content_transfer_encoding_t;
 *
 * The structure msg_content_transfer_encoding_t contains representation of
 * an @b Content-Transfer-Encoding header.
 *
 * The msg_content_transfer_encoding_t is defined as follows:
 * @code
 * typedef struct msg_generic_s
 * {
 *   msg_common_t   g_common[1];    // Common fragment info
 *   msg_generic_t *g_next;	    // Link to next header
 *   char const    *g_string;	    // Header value
 * } msg_content_transfer_encoding_t;
 * @endcode
 */


#define msg_content_transfer_encoding_d msg_generic_d
#define msg_content_transfer_encoding_e msg_generic_e
msg_hclass_t msg_content_transfer_encoding_class[] =
MSG_HEADER_CLASS_G(content_transfer_encoding, "Content-Transfer-Encoding",
		   "", single);

/* ====================================================================== */

/**@ingroup msg_mime
 * @defgroup msg_warning Warning Header
 *
 * The Warning response-header field is used to carry additional information
 * about the status of a response. Its syntax is defined in [S20.43]
 * as follows:
 *
 * @code
 *    Warning        =  "Warning" HCOLON warning-value *(COMMA warning-value)
 *    warning-value  =  warn-code SP warn-agent SP warn-text
 *    warn-code      =  3DIGIT
 *    warn-agent     =  hostport / pseudonym
 *                      ;  the name or pseudonym of the server adding
 *                      ;  the Warning header, for use in debugging
 *    warn-text      =  quoted-string
 *    pseudonym      =  token
 * @endcode
 */

/**@ingroup msg_warning
 * @typedef struct msg_warning_s msg_warning_t;
 *
 * The structure msg_warning_t contains representation of an @b
 * Warning header.
 *
 * The msg_warning_t is defined as follows:
 * @code
 * typedef struct msg_warning_s
 * {
 *   msg_common_t        w_common[1];  // Common fragment info
 *   msg_warning_t      *w_next;       // Link to next Warning header
 *   unsigned            w_code;       // Warning code
 *   char const         *w_host;       // Hostname or pseudonym
 *   char const         *w_port;       // Port number
 *   char const         *w_text;       // Warning text
 * } msg_warning_t;
 * @endcode
 */

issize_t msg_warning_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
	msg_warning_t *w;
  char *text;

  for(;;) {
	  w = (msg_warning_t *)h;
	  while (*s == ',')   /* Ignore empty entries (comma-whitespace) */
		  *s = '\0', s += span_lws(s + 1) + 1;

	  /* Parse protocol */
	  if (!IS_DIGIT(*s))
		  return -1;
	  w->w_code = strtoul(s, &s, 10);
	  skip_lws(&s);

	  /* Host (and port) */
	  if (msg_hostport_d(&s, &w->w_host, &w->w_port) == -1)
		  return -1;
	  if (msg_quoted_d(&s, &text) == -1)
		  return -1;
	  if (msg_unquote(text, text) == NULL)
		  return -1;

	  w->w_text = text;
	  
	  msg_parse_next_field_without_recursion();
  }

}

issize_t msg_warning_e(char b[], isize_t bsiz, msg_header_t const *h, int f)
{
  msg_warning_t const *w = (msg_warning_t *)h;
  char const *port = w->w_port;
  int n;
  size_t m;

  n = snprintf(b, bsiz, "%03u %s%s%s ",
	       w->w_code, w->w_host, port ? ":" : "", port ? port : "");
  if (n < 0)
    return n;

  m = msg_unquoted_e((size_t)n < bsiz ? b + n : NULL, bsiz - n, w->w_text);

  if (b && n + m < bsiz)
    b[n + m] = '\0';

  return n + m;
}

isize_t msg_warning_dup_xtra(msg_header_t const *h, isize_t offset)
{
  msg_warning_t const *w = (msg_warning_t *)h;

  offset += MSG_STRING_SIZE(w->w_host);
  offset += MSG_STRING_SIZE(w->w_port);
  offset += MSG_STRING_SIZE(w->w_text);

  return offset;
}

char *msg_warning_dup_one(msg_header_t *dst,
			  msg_header_t const *src,
			  char *b,
			  isize_t xtra)
{
  msg_warning_t *w = (msg_warning_t *)dst;
  msg_warning_t const *o = (msg_warning_t *)src;
  char *end = b + xtra;

  w->w_code = o->w_code;
  MSG_STRING_DUP(b, w->w_host, o->w_host);
  MSG_STRING_DUP(b, w->w_port, o->w_port);
  MSG_STRING_DUP(b, w->w_text, o->w_text);

  assert(b <= end); (void)end;

  return b;
}

#define msg_warning_update NULL

msg_hclass_t msg_warning_class[] =
  MSG_HEADER_CLASS(msg_, warning, "Warning", "", w_common, append,
		   msg_warning, msg_warning);
