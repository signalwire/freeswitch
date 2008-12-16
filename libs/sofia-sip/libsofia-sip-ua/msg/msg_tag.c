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

/**@CFILE msg_tag.c  Message tag classes
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri Feb 23 12:46:42 2001 ppessi
 *
 */

#include "config.h"

#include <assert.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>

#include <sofia-sip/su.h>

#define TAG_NAMESPACE "msg"

#include "msg_internal.h"
#include "sofia-sip/msg_header.h"
#include "sofia-sip/msg_parser.h"

#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_tag_inline.h>
#include <sofia-sip/su_tagarg.h>
#include "sofia-sip/msg_tag_class.h"

#define NONE ((void*)-1)

int msghdrtag_snprintf(tagi_t const *t, char b[], size_t size)
{
  msg_header_t const *h;

  assert(t);

  if (!t) {
    if (size) b[0] = 0;
    return 0;
  }

  h = (msg_header_t const *)t->t_value;

  if (h != MSG_HEADER_NONE && h != NULL) {
    return msg_header_field_e(b, size, h, 0);
  }
  else {
    return snprintf(b, size, "<NONE>");
  }
}

size_t msghdrtag_xtra(tagi_t const *t, size_t offset)
{
  msg_header_t const *h;
  size_t rv;
  msg_hclass_t *hc = (msg_hclass_t *)t->t_tag->tt_magic;
  assert(t);

  for (h = (msg_header_t const *)t->t_value, rv = offset;
       h != NULL;
       h = h->sh_next) {
    if (h == MSG_HEADER_NONE)
      break;
    MSG_STRUCT_SIZE_ALIGN(rv);
    if (hc)
      rv = hc->hc_dxtra(h, rv + h->sh_class->hc_size);
    else
      rv = h->sh_class->hc_dxtra(h, rv + h->sh_class->hc_size);
  }

  return rv - offset;
}

tagi_t *msghdrtag_dup(tagi_t *dst, tagi_t const *src, void **bb)
{
  msg_header_t const *o;
  msg_header_t *h, *h0 = NULL, **hh;
  msg_hclass_t *hc, *hc0 = (msg_hclass_t *)src->t_tag->tt_magic;
  char *b;
  size_t size;

  assert(src); assert(*bb);

  dst->t_tag = src->t_tag;
  dst->t_value = 0L;

  b = *bb;
  hh = &h0;

  for (o = (msg_header_t const *)src->t_value;
       o != NULL;
       o = o->sh_next) {
    if (o == MSG_HEADER_NONE) {
      *hh = (msg_header_t *)o;
      break;
    }

    /* XXX assert(msg_hdr_p(o)); */

    MSG_STRUCT_ALIGN(b);
    h = (msg_header_t *)b;
    hc = hc0 ? hc0 : o->sh_class;
    b += hc->hc_size;
    memset(h, 0, hc->hc_size);
    h->sh_class = hc;

    size = SIZE_MAX - (uintptr_t)b;
    if (size > ISSIZE_MAX)
      size = ISSIZE_MAX;
    b = hc->hc_dup_one(h, o, b, size);

    if (hc->hc_update)
      msg_header_update_params(h->sh_common, 0);

    *hh = h; hh = &h->sh_next;

    assert(b != NULL);
  }
  *bb = b;

  dst->t_value = (tag_value_t)h0;

  return dst + 1;
}


/** Convert a string to a header structure based on to the tag. */
int msghdrtag_scan(tag_type_t tt, su_home_t *home,
		   char const *s,
		   tag_value_t *return_value)
{
  msg_hclass_t *hc = (msg_hclass_t *)tt->tt_magic;
  msg_header_t *h;
  int retval;

  h = msg_header_make(home, hc, s);

  if (h)
    *return_value = (tag_value_t)h, retval = 1;
  else
    *return_value = (tag_value_t)NULL, retval = -1;

  return retval;
}


tagi_t *msgstrtag_filter(tagi_t *dst,
			 tagi_t const f[],
			 tagi_t const *src,
			 void **bb);

int msgobjtag_snprintf(tagi_t const *t, char b[], size_t size)
{
  msg_pub_t const *mo;

  assert(t);

  if (!t || !t->t_value) {
    if (size) b[0] = 0;
    return 0;
  }

  mo = (msg_pub_t *)t->t_value;

  return (int)msg_object_e(b, size, mo, MSG_DO_CANONIC);
}


size_t msgobjtag_xtra(tagi_t const *t, size_t offset)
{
  msg_header_t const *h;
  msg_pub_t const *mo;
  size_t rv;

  assert(t);

  mo = (msg_pub_t const *)t->t_value;

  if (mo == NULL || mo == NONE)
    return 0;

  rv = offset;

  MSG_STRUCT_SIZE_ALIGN(rv);
  rv += mo->msg_size;

  if (mo->msg_request)
    h = (msg_header_t const *)mo->msg_request;
  else
    h = (msg_header_t const *)mo->msg_status;

  for (; h; h = h->sh_succ) {
    MSG_STRUCT_SIZE_ALIGN(rv);
    rv += msg_header_size(h);
  }

  return rv - offset;
}


tagi_t *msgobjtag_dup(tagi_t *dst, tagi_t const *src, void **bb)
{
  msg_pub_t const *omo;
  msg_pub_t *mo;
  msg_header_t *h;
  msg_header_t const *o;
  char *b;

  assert(src); assert(*bb);

  omo = (msg_pub_t const *)src->t_value;

  dst->t_tag = src->t_tag;
  dst->t_value = 0;

  if (omo == NULL || omo == NONE) {
    dst->t_value = src->t_value;
    return dst + 1;
  }

  b = *bb;
  MSG_STRUCT_ALIGN(b);
  mo = (msg_pub_t *)b;
  b += omo->msg_size;

  memset(mo, 0, omo->msg_size);
  mo->msg_size = omo->msg_size;
  mo->msg_flags = omo->msg_flags;

  if (mo->msg_request)
    o = mo->msg_request;
  else
    o = mo->msg_status;

  for (; o; o = o->sh_succ) {
    size_t size;
    MSG_STRUCT_ALIGN(b);
    h = (msg_header_t *)b;
    b += o->sh_class->hc_size;
    memset(h, 0, o->sh_class->hc_size);
    h->sh_class = o->sh_class;
    size = SIZE_MAX - (uintptr_t)b;
    if (size > ISSIZE_MAX)
      size = ISSIZE_MAX;
    b = o->sh_class->hc_dup_one(h, o, b, size);
    if (o->sh_class->hc_update)
      msg_header_update_params(h->sh_common, 0);
    assert(b != NULL);
  }

  dst->t_value = (tag_value_t)mo;
  *bb = b;

  return dst + 1;
}

#if 0

int msgtag_multipart_xtra(tagi_t const *t, int offset)
{
  msg_header_t const *h;
  msg_multipart_t const *mp;
  int rv;

  assert(t);

  mp = (msg_multipart_t const *)t->t_value;

  if (mp == NULL || mp == NONE)
    return 0;

  rv = offset;

  MSG_STRUCT_SIZE_ALIGN(rv);
  rv += mo->msg_size;

  if (mo->msg_request)
    h = (msg_header_t const *)mo->msg_request;
  else
    h = (msg_header_t const *)mo->msg_status;

  for (; h; h = h->sh_succ) {
    MSG_STRUCT_SIZE_ALIGN(rv);
    rv += msg_header_size(h);
  }

  return rv - offset;
}


tagi_t *msgtag_multipart_dup(tagi_t *dst, tagi_t const *src, void **bb)
{
  msg_pub_t const *omo;
  msg_pub_t *mo;
  msg_header_t *h;
  msg_header_t const *o;
  char *b;

  assert(src); assert(*bb);

  omo = (msg_pub_t const *)src->t_value;

  dst->t_tag = src->t_tag;
  dst->t_value = 0;

  if (omo == NULL || omo == NONE) {
    dst->t_value = src->t_value;
    return dst + 1;
  }

  b = *bb;
  MSG_STRUCT_ALIGN(b);
  mo = (msg_pub_t *)b;
  b += omo->msg_size;

  memset(mo, 0, omo->msg_size);
  mo->msg_size = omo->msg_size;
  mo->msg_flags = omo->msg_flags;

  if (mo->msg_request)
    o = mo->msg_request;
  else
    o = mo->msg_status;

  for (; o; o = o->sh_succ) {
    size_t size;
    MSG_STRUCT_ALIGN(b);
    h = (msg_header_t *)b;
    b += o->sh_class->hc_size;
    memset(h, 0, o->sh_class->hc_size);
    h->sh_class = o->sh_class;
    size = SIZE_MAX - (uintptr_t)b;
    if (size > ISSIZE_MAX)
      size = ISSIZE_MAX;
    b = o->sh_class->hc_dup_one(h, o, b, size);
    if (o->sh_class->hc_update)
      msg_header_update_params(h->sh_common, 0);
    assert(b != NULL);
  }

  dst->t_value = (long)mo;
  *bb = b;

  return dst + 1;
}

#endif
