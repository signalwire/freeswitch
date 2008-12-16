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

/**@CFILE http_tag_class.c  HTTP tag classes
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri Feb 23 12:46:42 2001 ppessi
 */

#include "config.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include <sofia-sip/su.h>

/* Avoid casting http_t to msg_pub_t and http_header_t to msg_header_t  */
#define MSG_PUB_T struct http_s
#define MSG_HDR_T union http_header_u

#include <sofia-sip/http_parser.h>

#include <sofia-sip/http_tag.h>
#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_tag_inline.h>
#include <sofia-sip/http_tag_class.h>
#include <sofia-sip/su_tagarg.h>

tag_class_t httphdrtag_class[1] =
  {{
    sizeof(httphdrtag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     msghdrtag_xtra,
    /* tc_dup */      msghdrtag_dup,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ msghdrtag_snprintf,
    /* tc_filter */   httptag_filter,
    /* tc_ref_set */  t_ptr_ref_set,
    /* tc_scan */     msghdrtag_scan,
  }};

tag_class_t httpstrtag_class[1] =
  {{
    sizeof(httpstrtag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     t_str_xtra,
    /* tc_dup */      t_str_dup,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ t_str_snprintf,
    /* tc_filter */   NULL /* msgtag_str_filter */,
    /* tc_ref_set */  t_ptr_ref_set,
    /* tc_scan */     msghdrtag_scan,
  }};

tag_class_t httpmsgtag_class[1] =
  {{
    sizeof(httpmsgtag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     msgobjtag_xtra,
    /* tc_dup */      msgobjtag_dup,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ msgobjtag_snprintf,
    /* tc_filter */   NULL /* httptag_http_filter */,
    /* tc_ref_set */  t_ptr_ref_set,
    /* tc_scan */     t_str_scan
  }};

/** Filter a HTTP header structure. */
tagi_t *httptag_filter(tagi_t *dst,
		      tagi_t const f[],
		      tagi_t const *src,
		      void **bb)
{
  tagi_t stub[2] = {{ NULL }};
  tag_type_t sctt, tt = f->t_tag;
  msg_hclass_t *hc = (msg_hclass_t *)tt->tt_magic;

  assert(src);

  sctt = src->t_tag;

  if (sctt && sctt->tt_class == httpmsgtag_class) {
    http_t const *http;
    msg_mclass_t const *mc;
    http_header_t const *h, **hh;

    http = (http_t const *)src->t_value;
    if (http == NULL)
      return dst;

    mc = (void *)http->http_common->h_class;
    hh = (void *)msg_hclass_offset(mc, http, hc);

    if (hh == NULL ||
	(char *)hh >= ((char *)http + http->http_size) ||
	(char *)hh < (char *)&http->http_request)
      return dst;

    h = *hh;

    if (h == NULL)
      return dst;

    stub[0].t_tag = tt;
    stub[0].t_value = (tag_value_t)h;
    src = stub; sctt = tt;
  }

  if (tt != sctt)
    return dst;

  if (!src->t_value)
    return dst;
  else if (dst) {
    return t_dup(dst, src, bb);
  }
  else {
    *bb = (char *)*bb + t_xtra(src, (size_t)*bb);
    return dst + 1;
  }
}

/** Duplicate headers from taglist and add them to the HTTP message.
 *
 * Return the number of headers added to the HTTP message.
 */
int http_add_tl(msg_t *msg, http_t *http,
	       tag_type_t tag, tag_value_t value, ...)
{
  tagi_t const *t;
  ta_list ta;
  int retval = 0;

  if (msg == NULL)
    return -1;
  if (http == NULL)
    http = msg_object(msg);

  ta_start(ta, tag, value);

  for (t = ta_args(ta); t; t = tl_next(t)) {
    if (!(tag = t->t_tag) || !(value = t->t_value))
      continue;

    if (HTTPTAG_P(tag)) {
      msg_hclass_t *hc = (msg_hclass_t *)tag->tt_magic;
      http_header_t *h = (http_header_t *)value, **hh;

      if (h == HTTP_NONE) { /* Remove header(s) */
	if (hc == NULL)
	  break;

	hh = msg_hclass_offset(msg_mclass(msg), http, hc);
	if (!hh)
	  break;

	while (*hh) {
	  msg_header_remove(msg, http, *hh);
	}
      }
      else if (h == NULL) {
	continue;
      } else {
	if (tag == httptag_header)
	  hc = h->sh_class;

	if (msg_header_add_dup_as(msg, http, hc, h) < 0)
	  break;
      }
    }
    else if (HTTPTAG_STR_P(tag)) {
      msg_hclass_t *hc = (msg_hclass_t *)tag->tt_magic;
      char const *s = (char const *)value;
      if (s && msg_header_add_make(msg, http, hc, s) < 0)
	break;
    }
    else if (tag == httptag_header_str) {
      if (msg_header_add_str(msg, http, (char const *)value) < 0)
	break;
    }
    else
      continue;

    retval++;
  }

  ta_end(ta);

  if (t)
    return -1;

  return retval;
}
