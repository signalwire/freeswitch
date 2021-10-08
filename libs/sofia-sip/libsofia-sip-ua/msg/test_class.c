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

/**@ingroup test_msg
 * @file test_class.c
 *
 * Message class for testing parser and transports.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Mar  5 11:57:20 2002 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#define TAG_NAMESPACE "tst"

#include "test_class.h"
#include <sofia-sip/msg_parser.h>
#include <sofia-sip/msg_mclass.h>
#include "test_protos.h"
#include <sofia-sip/msg_addr.h>

extern msg_mclass_t const msg_test_mclass[1];

extern msg_mclass_t const *msg_test_default(void)
{
  return msg_test_mclass;
}

#define msg_generic_update NULL

/**@ingroup test_msg
 * @defgroup msg_test_request Request Line for Testing
 */

static msg_xtra_f msg_request_dup_xtra;
static msg_dup_f msg_request_dup_one;

msg_hclass_t msg_request_class[] =
MSG_HEADER_CLASS(msg_, request, NULL, "", rq_common,
		 single_critical, msg_request, msg_generic);

/** Decode a request line */
issize_t msg_request_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  msg_request_t *rq = (msg_request_t *)h;
  char *uri, *version;

  if (msg_firstline_d(s, &uri, &version) < 0 || !uri ||
      url_d(rq->rq_url, uri) < 0)
    return -1;

  rq->rq_method_name = s;
  rq->rq_version = version;

  return 0;
}

/**Encode a request line. */
issize_t msg_request_e(char b[], isize_t bsiz, msg_header_t const *h, int flags)
{
  msg_request_t const *rq = (msg_request_t const *)h;

  return snprintf(b, bsiz, "%s " URL_FORMAT_STRING " %s" CRLF,
		  rq->rq_method_name,
		  URL_PRINT_ARGS(rq->rq_url),
		  rq->rq_version);
}

isize_t msg_request_dup_xtra(msg_header_t const *h, isize_t offset)
{
  isize_t rv = offset;
  msg_request_t const *rq = (msg_request_t const *)h;

  rv += url_xtra(rq->rq_url);
  rv += MSG_STRING_SIZE(rq->rq_method_name);
  rv += MSG_STRING_SIZE(rq->rq_version);

  return rv;
}

/** Duplicate one request header. */
char *msg_request_dup_one(msg_header_t *dst, msg_header_t const *src,
			  char *b, isize_t xtra)
{
  msg_request_t *rq = (msg_request_t *)dst;
  msg_request_t const *o = (msg_request_t const *)src;
  char *end = b + xtra;

  URL_DUP(b, end, rq->rq_url, o->rq_url);

  MSG_STRING_DUP(b, rq->rq_method_name, o->rq_method_name);
  MSG_STRING_DUP(b, rq->rq_version, o->rq_version);

  assert(b <= end);

  return b;
}

/**@ingroup test_msg
 * @defgroup msg_test_status Status Line for Testing
 */

static msg_xtra_f msg_status_dup_xtra;
static msg_dup_f msg_status_dup_one;

msg_hclass_t msg_status_class[1] =
MSG_HEADER_CLASS(msg_, status, NULL, "", st_common,
		 single_critical, msg_status, msg_generic);

/** Parse status line */
issize_t msg_status_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  msg_status_t *st = (msg_status_t *)h;
  char *status, *phrase;
  unsigned long code;

  if (msg_firstline_d(s, &status, &phrase) < 0 ||
      (code = strtoul(status, &status, 10)) >= 1000 || *status)
    return -1;

  st->st_status = code;
  st->st_phrase = phrase;
  st->st_version = s;

  return 0;
}

issize_t msg_status_e(char b[], isize_t bsiz, msg_header_t const *h, int flags)
{
  msg_status_t const *st = (msg_status_t const *)h;
  int status = st->st_status;

  if (status > 999 || status < 100)
    status = 0;

  return snprintf(b, bsiz, "%s %03u %s" CRLF,
		  st->st_version, status, st->st_phrase);
}

/** Extra size of a msg_status_t object. */
isize_t msg_status_dup_xtra(msg_header_t const *h, isize_t offset)
{
  isize_t rv = offset;
  msg_status_t const *st = (msg_status_t const *)h;
  rv += MSG_STRING_SIZE(st->st_version);
  rv += MSG_STRING_SIZE(st->st_phrase);
  return rv;
}

/** Duplicate one status header. */
char *msg_status_dup_one(msg_header_t *dst, msg_header_t const *src,
			 char *b, isize_t xtra)
{
  msg_status_t *st = (msg_status_t *)dst;
  msg_status_t const *o = (msg_status_t const *)src;
  char *end = b + xtra;

  MSG_STRING_DUP(b, st->st_version, o->st_version);
  st->st_status = o->st_status;
  MSG_STRING_DUP(b, st->st_phrase, o->st_phrase);

  assert(b <= end); (void)end;

  return b;
}

msg_hclass_t test_numeric_class[] =
  MSG_HEADER_CLASS(msg_, numeric, "Numeric", "", x_common,
		   single, msg_generic, msg_generic);

msg_hclass_t test_auth_class[] =
  MSG_HEADER_CLASS(msg_, auth, "Auth", "", au_params,
		   append, msg_auth, msg_generic);

/** Extract the message body, including separator line.
 *
 * @param[in,out] msg  message object
 * @param[in,out] pub  public message structure
 * @param[in]     b    buffer containing unparsed data
 * @param[in]     bsiz buffer size
 * @param[in]     eos  true if buffer contains whole message
 *
 * @retval -1     error
 * @retval 0      message is incomplete
 * @retval other  number of bytes extracted
 */
issize_t msg_test_extract_body(msg_t *msg, msg_pub_t *pub,
			       char b[], isize_t bsiz, int eos)
{
  msg_test_t *tst = (msg_test_t *)pub;
  ssize_t m = 0;
  size_t body_len;

  if (!(tst->msg_flags & MSG_FLG_BODY)) {
    /* We are looking at a potential empty line */
    m = msg_extract_separator(msg, (msg_pub_t *)tst, b, bsiz, eos);
    if (m == 0 || m == -1)
      return m;
    tst->msg_flags |= MSG_FLG_BODY;
    b += m;
    bsiz -= m;
  }

  if (tst->msg_content_length)
    body_len = tst->msg_content_length->l_length;
  else if (MSG_IS_MAILBOX(tst->msg_flags)) /* message fragments */
    body_len = 0;
  else if (eos)
    body_len = bsiz;
  else
    return -1;

  if (body_len == 0) {
    tst->msg_flags |= MSG_FLG_COMPLETE;
    return m;
  }

  if (m)
    return m;

  if ((m = msg_extract_payload(msg, (msg_pub_t *)tst, NULL, body_len, b, bsiz, eos) ) == -1)
    return -1;

  tst->msg_flags |= MSG_FLG_FRAGS;
  if (bsiz >= body_len)
    tst->msg_flags |= MSG_FLG_COMPLETE;
  return m;
}

msg_href_t const msg_content_length_href[1] =
  {{
    msg_content_length_class,
    offsetof(msg_test_t, msg_content_length)
  }};

#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_tag_inline.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/msg_tag_class.h>

tagi_t *tsttag_filter(tagi_t *dst,
		      tagi_t const f[],
		      tagi_t const *src,
		      void **bb);

/** Tag class for test header tags. @HIDE */
tag_class_t tsthdrtag_class[1] =
  {{
    sizeof(tsthdrtag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     msghdrtag_xtra,
    /* tc_dup */      msghdrtag_dup,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ msghdrtag_snprintf,
    /* tc_filter */   tsttag_filter,
    /* tc_ref_set */  t_ptr_ref_set,
    /* tc_scan */     msghdrtag_scan,
  }};

/** Tag class for TST header string tags. @HIDE */
tag_class_t tststrtag_class[1] =
  {{
    sizeof(tststrtag_class),
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
    /* tc_scan */     t_str_scan
  }};

/** Tag class for TST message tags. @HIDE */
tag_class_t tstmsgtag_class[1] =
  {{
    sizeof(tstmsgtag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     msgobjtag_xtra,
    /* tc_dup */      msgobjtag_dup,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ msgobjtag_snprintf,
    /* tc_filter */   NULL /* tsttag_tst_filter */,
    /* tc_ref_set */  t_ptr_ref_set,
  }};

tag_typedef_t tsttag_header =
	{{ TAG_NAMESPACE, "header", tsthdrtag_class, 0 }};

tag_typedef_t tsttag_header_str = STRTAG_TYPEDEF(header_str);

/** Filter a TST header structure. */
tagi_t *tsttag_filter(tagi_t *dst,
		      tagi_t const f[],
		      tagi_t const *src,
		      void **bb)
{
  tagi_t stub[2] = {{ NULL }};
  tag_type_t sctt, tt = f->t_tag;
  msg_hclass_t *hc = (msg_hclass_t *)tt->tt_magic;

  assert(src);

  sctt = src->t_tag;

  if (sctt && sctt->tt_class == tstmsgtag_class) {
    msg_test_t const *tst = (msg_test_t const *)src->t_value;
    msg_mclass_t *mc = (msg_mclass_t *)tst->msg_ident;
    msg_header_t const **hh = (msg_header_t const **)
      msg_hclass_offset(mc, (msg_pub_t *)tst, hc);
    msg_header_t const *h;

    if (tst == NULL ||
	(char *)hh >= ((char *)tst + tst->msg_size) ||
	(char *)hh < (char const *)&tst->msg_request)
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

/** Add duplicates of headers from taglist to the TST message. */
int tst_add_tl(msg_t *msg, msg_test_t *tst,
	       tag_type_t tag, tag_value_t value, ...)
{
  tagi_t const *t;
  ta_list ta;

  ta_start(ta, tag, value);

  for (t = ta_args(ta); t; t = tl_next(t)) {
    if (!(tag = t->t_tag) || !(value = t->t_value))
      continue;

    if (TSTTAG_P(tag)) {
      msg_hclass_t *hc = (msg_hclass_t *)tag->tt_magic;
      msg_header_t *h = (msg_header_t *)value, **hh;

      if (h == NULL)
	;
      else if (h == MSG_HEADER_NONE) {	/* Remove header */
	hh = msg_hclass_offset(msg_mclass(msg), (msg_pub_t *)tst, hc);
	while (hh && *hh)
	  msg_header_remove(msg, (msg_pub_t *)tst, *hh);
      } else if (msg_header_add_dup_as(msg, (msg_pub_t *)tst, hc, h) < 0)
	break;
    }
    else if (TSTTAG_STR_P(tag)) {
      msg_hclass_t *hc = (msg_hclass_t *)tag->tt_magic;
      char const *s = (char const *)value;
      if (s && msg_header_add_make(msg, (msg_pub_t *)tst, hc, s) < 0)
	break;
    }
    else if (tag == tsttag_header_str) {
      if (msg_header_add_str(msg, (msg_pub_t *)tst, (char const *)value) < 0)
	break;
    }
  }

  ta_end(ta);

  return t ? -1 : 0;
}

