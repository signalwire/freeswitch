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
 * @CFILE msg_header_copy.c
 *
 * Copying and duplicating headers structures.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Jun 13 02:57:51 2000 ppessi
 *
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include <sofia-sip/su_alloc.h>

#include <sofia-sip/su.h>

#include "msg_internal.h"
#include "sofia-sip/msg.h"
#include "sofia-sip/msg_parser.h"
#include "sofia-sip/msg_header.h"

/** Calculate size of a parameter vector */
su_inline
size_t msg_params_copy_xtra(msg_param_t const pp[], size_t offset)
{
  size_t n = msg_params_count(pp);
  if (n) {
    MSG_STRUCT_SIZE_ALIGN(offset);
    offset += MSG_PARAMS_NUM(n + 1) * sizeof(pp[0]);
  }
  return offset;
}

/** Copy a vector of parameters */
su_inline
char *msg_params_copy(char *b, size_t size,
		      msg_param_t **dst,
		      msg_param_t const src[])
{
  size_t n = msg_params_count(src);

  if (n) {
    MSG_STRUCT_ALIGN(b);
    *dst = memcpy(b, src, (n + 1) * sizeof(src[0]));
    b += MSG_PARAMS_NUM(n + 1) * sizeof(src[0]);
  }
  else {
    *dst = NULL;
  }

  return b;
}

/**Copy a header object.
 *
 * The function @c msg_header_copy_as() shallowly copies a header object.
 *
 * @param home pointer to the memory home
 * @param hc   header class for the copied header
 * @param src  pointer to a header object
 *
 * @return
 * The function @c msg_header_copy_as() returns a pointer to the the shallow copy
 * of the header object, or @c NULL upon an error.
 */
static msg_header_t *msg_header_copy_one_as(su_home_t *home,
					    msg_hclass_t *hc,
					    msg_header_t const *src)
{
  msg_header_t *h;
  size_t size = hc->hc_size, xtra;
  msg_param_t const *params;
  char *end;

  if (hc->hc_params) {
    params = *(msg_param_t const **)((char const *)src + hc->hc_params);
    xtra = msg_params_copy_xtra(params, size) - size;
  }
  else {
    params = NULL;
    xtra = 0;
  }

  if (!(h = msg_header_alloc(home, hc, (isize_t)xtra)))
    return NULL;			/* error */

  memcpy(&h->sh_data, &src->sh_data, size - offsetof(msg_common_t, h_data));
  h->sh_next = NULL;
  if (params) {
    msg_param_t **pparams = (msg_param_t **)((char *)h + hc->hc_params);
    end = msg_params_copy((char *)h + size, xtra, pparams, params);
    if (!end) {
      su_free(home, h);
      return NULL;
    }
  }
  else
    end = (char *)h + size;

  assert(end == (char *)h + xtra + size);

  return h;
}

/**Copy a list of header objects.
 *
 * The function @c msg_header_copy_as() shallowly copies a list of header
 * objects, and casts them to the given header class.
 *
 * @param home pointer to the memory home
 * @param hc   header class
 * @param src  pointer to a list of header objects to be copied
 *
 * @return The function @c msg_header_copy_as() returns a pointer to the
 * first of the copied msg header object(s), or @c NULL upon an error.
 */
msg_header_t *msg_header_copy_as(su_home_t *home,
				 msg_hclass_t *hc,
				 msg_header_t const *src)
{
  msg_header_t *h, *rv = NULL, *prev = NULL;

  if (src == NULL || src == MSG_HEADER_NONE)
    return NULL;

  if (hc == NULL)
    hc = src->sh_class;

  for (; src; src = src->sh_next, prev = h) {
    if (!(h = msg_header_copy_one_as(home, hc, src)))
      break;

    if (!rv)
      rv = h;
    else
      prev->sh_next = h;
  }

  if (src) {
    /* Copy was not successful, free all copied headers in list */
    for (;rv; rv = h) {
      h = rv->sh_next;
      su_free(home, rv);
    }
  }

  return rv;
}

/** Copy a single header. */
msg_header_t *msg_header_copy_one(su_home_t *home, msg_header_t const *src)
{
  assert(MSG_HEADER_TEST(src));

  if (!src || !src->sh_class)
    return NULL;

  return msg_header_copy_one_as(home, src->sh_class, src);
}

/** Copy a header list. */
msg_header_t *msg_header_copy(su_home_t *home, msg_header_t const *src)
{
  assert(MSG_HEADER_TEST(src));

  if (!src || !src->sh_class)
    return NULL;

  return msg_header_copy_as(home, src->sh_class, src);
}

/** Duplicate a sigle header.
 *
 * Deeply copy a single header.
 *
 * @param home pointer to the memory home
 * @param src  pointer to asingle header object to be copied
 *
 * @return Return a pointer to the
 * the duplicated msg header object(s), or @c NULL upon an error.
 */
msg_header_t *msg_header_dup_one(su_home_t *home,
				 msg_header_t const *src)
{
  msg_hclass_t *hc;
  size_t size, xtra;
  msg_header_t *h;
  char *end;

  if (src == NULL || src == MSG_HEADER_NONE)
    return NULL;

  hc = src->sh_class;

  assert(hc);

  size = hc->hc_size;
  xtra = hc->hc_dxtra(src, size) - size;

  if (!(h = msg_header_alloc(home, hc, xtra)))
    return NULL;

  if (!(end = hc->hc_dup_one(h, src, (char *)h + size, xtra))) {
    su_free(home, h);
    return NULL;
  }

  if (hc->hc_update)
    msg_header_update_params(h->sh_common, 1);

  assert(end == (char *)h + size + xtra);

  return h;
}

/** Duplicate a header as class @a hc.
 *
 * The function @c msg_header_dup_as() casts a list of header headers to
 * given type, and then deeply copies the list.
 *
 * @param home pointer to the memory home
 * @param hc   header class
 * @param src  pointer to a list of header objects to be copied
 *
 * @return The function @c msg_header_copy_as() returns a pointer to the
 * first of the copied msg header object(s), or @c NULL upon an error.
 */
msg_header_t *msg_header_dup_as(su_home_t *home, msg_hclass_t *hc,
				msg_header_t const *src)
{
  msg_header_t *h, *rv = NULL, **prev;

  if (src == NULL || src == MSG_HEADER_NONE)
    return NULL;

  if (hc == NULL)
    hc = src->sh_class;

  assert(hc);

  for (prev = &rv; src; src = src->sh_next, prev = &h->sh_next) {
    size_t size = hc->hc_size;
    size_t xtra = hc->hc_dxtra(src, size) - size;
    char *end;

    if (!(h = msg_header_alloc(home, hc, (isize_t)xtra)))
      break;			/* error */

    if (!rv)
      rv = h;

    if (!(end = hc->hc_dup_one(h, src, (char *)h + size, xtra)))
      break;			/* error */

    if (hc->hc_update)
      msg_header_update_params(h->sh_common, 1);

    assert(end == (char *)h + size + xtra);

    *prev = h;
  }

  if (src) {
    /* Copy was not successful, free all duplicated headers in list */
    for (;rv; rv = h) {
      h = rv->sh_next;
      su_free(home, rv);
    }
  }

  return rv;
}

/** Duplicate a header list.
 *
 * The function @c msg_header_dup() deeply copies a list of message headers
 * objects.
 *
 * @param home pointer to the memory home
 * @param h    pointer to a list of header objects to be copied
 *
 * @return The function @c msg_header_dup() returns a pointer to the first
 * of the copied message header object(s), or @c NULL upon an error.
 */
msg_header_t *msg_header_dup(su_home_t *home, msg_header_t const *h)
{
  if (h == NULL || h == MSG_HEADER_NONE)
    return NULL;
  assert(MSG_HEADER_TEST(h));
  return msg_header_dup_as(home, h->sh_class, h);
}

/** Calculate extra size of a plain header. */
isize_t msg_default_dup_xtra(msg_header_t const *header, isize_t offset)
{
  return offset;
}

/**Duplicate a header object without external references.
 *
 * The function @c msg_default_dup_one() copies the contents of header
 * object @a src to @a h. The header object should not contain external
 * references (pointers).
 *
 * @param h     pointer to newly allocated header object
 * @param src   pointer to a header object to be duplicated
 * @param b     memory buffer used to copy (not used)
 * @param xtra  number bytes in buffer @a b (not used)
 *
 * @return The function @c msg_default_dup_one() returns a pointer to the
 * memory buffer @a b.
 */
char *msg_default_dup_one(msg_header_t *h,
			  msg_header_t const *src,
			  char *b,
			  isize_t xtra)
{
  size_t skip = offsetof(msg_numeric_t, x_value);  /* Skip common part */

  memcpy((char *)h + skip, (char const *)src + skip, h->sh_class->hc_size - skip);

  return b;
}

/* ====================================================================== */
/* Copying or duplicating all headers in a message */

static int msg_copy_chain(msg_t *msg, msg_t const *copied);
static int msg_dup_or_copy_all(msg_t *msg,
			       msg_t const *original,
			       msg_header_t *(*copy_one)(su_home_t *h,
							 msg_header_t const *));


/**Copy a message shallowly.
 *
 * @relatesalso msg_s
 *
 * Copy a message and the header structures. The copied message will share
 * all the strings with the original message. It will keep a reference to
 * the original message, and the original message is not destroyed until all
 * the copies have been destroyed.
 *
 * @param original message to be copied
 *
 * @retval pointer to newly copied message object when successful
 * @retval NULL upon an error
 */
msg_t *msg_copy(msg_t *original)
{
  if (original) {
    msg_t *copy = msg_create(original->m_class, original->m_object->msg_flags);

    if (copy) {
      if (original->m_chain
	  ? msg_copy_chain(copy, original) < 0
	  : msg_dup_or_copy_all(copy, original, msg_header_copy_one) < 0) {
	msg_destroy(copy), copy = NULL;
      }
      else
	msg_set_parent(copy, original);

      return copy;
    }
  }

  return NULL;
}

/** Copy header chain.
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 */
static
int msg_copy_chain(msg_t *msg, msg_t const *original)
{
  su_home_t *home = msg_home(msg);
  msg_pub_t *dst = msg->m_object;
  msg_header_t **tail;
  msg_header_t *dh;
  msg_header_t const *sh;
  msg_header_t **hh;

  tail = msg->m_tail;

  for (sh = original->m_chain; sh; sh = (msg_header_t const *)sh->sh_succ) {
    hh = msg_hclass_offset(msg->m_class, dst, sh->sh_class);
    if (!hh)
      break;
    while (*hh)
      hh = &(*hh)->sh_next;

    dh = msg_header_copy_one(home, sh);
    if (!dh)
      break;

    dh->sh_prev = tail, *tail = dh, tail = &dh->sh_succ;

    *hh = dh;
  }

  msg->m_tail = tail;

  if (sh)
    return -1;

  return 0;

}

/**Deep copy a message.
 *
 * @relatesalso msg_s
 *
 * Copy a message, the header structures and all the related strings. The
 * duplicated message does not share any (non-const) data with original.
 * Note that the cached representation (in h_data) is not copied.
 *
 * @param original message to be duplicated
 *
 * @retval pointer to newly duplicated message object when successful
 * @retval NULL upon an error
 */
msg_t *msg_dup(msg_t const *original)
{
  if (original) {
    msg_t *dup = msg_create(original->m_class, original->m_object->msg_flags);

    if (dup && msg_dup_or_copy_all(dup, original, msg_header_dup_one) < 0) {
      msg_destroy(dup), dup = NULL;
    }

    return dup;
  }

  return NULL;
}

/** Copy a complete message, not keeping the header chain structure.
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 */
static
int msg_dup_or_copy_all(msg_t *msg,
			msg_t const *original,
			msg_header_t *(*copy_one)(su_home_t *h,
						  msg_header_t const *))
{
  su_home_t *home = msg_home(msg);
  msg_pub_t *dst = msg->m_object;

  msg_pub_t const *src = original->m_object;
  msg_header_t * const *ssh;
  msg_header_t * const *end;
  msg_header_t const *sh;
  msg_header_t **hh;

  msg_header_t *h;

  assert(copy_one);

  end = (msg_header_t**)((char *)src + src->msg_size);

  for (ssh = &src->msg_request; ssh < end; ssh++) {
    sh = *ssh;
    if (!sh)
      continue;

    hh = msg_hclass_offset(msg->m_class, dst, sh->sh_class);
    if (hh == NULL)
	return -1;

    for (; sh; sh = sh->sh_next) {
      h = copy_one(home, sh);
      if (h == NULL)
	return -1;

      if (*hh) {
	/* If there is multiple instances of single headers,
	   put the extra headers into the list of erroneous headers */
	if (msg_is_single(h)) {
	  msg_error_t **e;
	  for (e = &dst->msg_error; *e; e = &(*e)->er_next)
	    ;
	  *e = (msg_error_t *)h;
	  continue;
	}

	while (*hh)
	  hh = &(*hh)->sh_next;
      }
      *hh = h;

      if (msg_is_list(sh))
	/* Copy only first list entry */
	break;
    }
  }

  return 0;
}
