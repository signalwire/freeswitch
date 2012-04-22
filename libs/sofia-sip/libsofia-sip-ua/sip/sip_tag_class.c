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

/**@SIP_TAG
 *
 * @CFILE sip_tag_class.c  SIP Tag classes
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Fri Feb 23 12:46:42 2001 ppessi
 */

#include "config.h"

#include "sofia-sip/sip_parser.h"

#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_tag_inline.h>
#include <sofia-sip/sip_tag_class.h>
#include <sofia-sip/sip_tag.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_strlst.h>

#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

/** Tag class for tags containing SIP headers. @HIDE
 *
 * Tags in this class are not automatically added to the message with
 * sip_add_tl() or sip_add_tagis().
 */
tag_class_t sipexthdrtag_class[1] =
  {{
    sizeof(siphdrtag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     msghdrtag_xtra,
    /* tc_dup */      msghdrtag_dup,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ msghdrtag_snprintf,
    /* tc_filter */   siptag_filter,
    /* tc_ref_set */  t_ptr_ref_set,
    /* tc_scan */     msghdrtag_scan,
  }};


/** Tag class for SIP header tags. @HIDE */
tag_class_t siphdrtag_class[1] =
  {{
    sizeof(siphdrtag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     msghdrtag_xtra,
    /* tc_dup */      msghdrtag_dup,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ msghdrtag_snprintf,
    /* tc_filter */   siptag_filter,
    /* tc_ref_set */  t_ptr_ref_set,
    /* tc_scan */     msghdrtag_scan,
  }};

/** Tag class for SIP header string tags. @HIDE */
tag_class_t sipstrtag_class[1] =
  {{
    sizeof(sipstrtag_class),
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

/** Tag class for SIP message tags. @HIDE */
tag_class_t sipmsgtag_class[1] =
  {{
    sizeof(sipmsgtag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     msgobjtag_xtra,
    /* tc_dup */      msgobjtag_dup,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ msgobjtag_snprintf,
    /* tc_filter */   NULL /* siptag_sip_filter */,
    /* tc_ref_set */  t_ptr_ref_set,
  }};


/** Filter a for SIP header tag.
 *
 * @param[in] dst tag list for filtering result. May be NULL.
 * @param[in] f   filter tag
 * @param[in] src tag item from source list.
 * @param[in,out] bb pointer to pointer of mempory area used to dup
 *                   the filtering result
 *
 * This function is also used to calculate size for filtering result.
 */
tagi_t *siptag_filter(tagi_t *dst,
		      tagi_t const f[],
		      tagi_t const *src,
		      void **bb)
{
  tagi_t stub[2] = {{ NULL }};
  tag_type_t srctt, tt = f->t_tag;
  msg_hclass_t *hc = (msg_hclass_t *)tt->tt_magic;

  assert(src);

  srctt = src->t_tag;

  /* Match filtered header with a header from a SIP message */
  if (srctt && srctt->tt_class == sipmsgtag_class) {
    sip_t const *sip = (sip_t const *)src->t_value;
    sip_header_t const **hh, *h;

    if (sip == NULL)
      return dst;

    hh = (sip_header_t const **)
      msg_hclass_offset((msg_mclass_t *)sip->sip_common->h_class,
			(msg_pub_t *)sip, hc);

    /* Is header present in the SIP message? */
    if (hh == NULL ||
	(char *)hh >= ((char *)sip + sip->sip_size) ||
	(char *)hh < (char *)&sip->sip_request)
      return dst;

    h = *hh;

    if (h == NULL)
      return dst;

    stub[0].t_tag = tt;
    stub[0].t_value = (tag_value_t)h;
    src = stub; srctt = tt;
  }

  if (tt != srctt)
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

/** Duplicate headers from taglist and add them to the SIP message. */
int sip_add_tl(msg_t *msg, sip_t *sip,
	       tag_type_t tag, tag_value_t value, ...)
{
  tagi_t const *t;
  ta_list ta;
  int retval;

  ta_start(ta, tag, value);

  t = ta_args(ta);

  retval = sip_add_tagis(msg, sip, &t);

  ta_end(ta);
  return retval;
}

/** Add duplicates of headers from taglist to the SIP message. */
int sip_add_tagis(msg_t *msg, sip_t *sip, tagi_t const **inout_list)
{
  tagi_t const *t;
  tag_type_t tag;
  tag_value_t value;

  if (!msg || !inout_list)
    return -1;

  if (sip == NULL)
    sip = sip_object(msg);

  for (t = *inout_list; t; t = t_next(t)) {
    tag = t->t_tag, value = t->t_value;

    if (tag == NULL || tag == siptag_end) {
      t = t_next(t);
      break;
    }

    if (!value)
      continue;

    if (SIPTAG_P(tag)) {
      msg_hclass_t *hc = (msg_hclass_t *)tag->tt_magic;
      msg_header_t *h = (msg_header_t *)value, **hh;

      if (h == SIP_NONE) {	/* Remove header */
	hh = msg_hclass_offset(msg_mclass(msg), (msg_pub_t *)sip, hc);
	if (hh != NULL &&
	    (char *)hh < ((char *)sip + sip->sip_size) &&
	    (char *)hh >= (char *)&sip->sip_request) {
	  while (*hh)
	    msg_header_remove(msg, (msg_pub_t *)sip, *hh);
	}
	continue;
      }

      if (tag == siptag_header)
	hc = h->sh_class;

      if (msg_header_add_dup_as(msg, (msg_pub_t *)sip, hc, h) < 0)
	break;
    }
    else if (SIPTAG_STR_P(tag)) {
      msg_hclass_t *hc = (msg_hclass_t *)tag->tt_magic;
      char const *s = (char const *)value;
      if (s && msg_header_add_make(msg, (msg_pub_t *)sip, hc, s) < 0)
	return -1;
    }
    else if (tag == siptag_header_str) {
      if (msg_header_add_str(msg, (msg_pub_t *)sip, (char const *)value) < 0)
	return -1;
    }
  }

  *inout_list = t;

  return 0;
}

static char const *append_escaped(su_strlst_t *l,
				  msg_hclass_t *hc,
				  char const *s);

/** Convert tagged SIP headers to a URL-encoded headers list.
 *
 * The SIP URI can contain a query part separated with the "?", which
 * specifies SIP headers that are included in the request constructed
 * from the URI. For example, using URI @code <sip:example.com?subject=test>
 * would include @Subject header with value "test" in the request.
 *
 * @param home memory home used to allocate query string (if NULL, use malloc)
 * @param tag, value, ... list of tagged arguments
 *
 * @bug This function returns NULL if SIPTAG_REQUEST(), SIPTAG_STATUS(),
 * SIPTAG_HEADER(), SIPTAG_UNKNOWN(), SIPTAG_ERROR(), SIPTAG_SEPARATOR(), or
 * any corresponding string tag is included in the tag list. It ignores
 * SIPTAG_SIP().
 *
 * @par Example
 * @code
 * url->url_headers =
 *   sip_headers_as_url_query(home, SIPTAG_REPLACES(replaces), TAG_END());
 * @endcode
 *
 * @since New in @VERSION_1_12_4.
 *
 * @sa
 * url_query_as_header_string(), sip_url_query_as_taglist(),
 * nta_msg_request_complete(),
 * @RFC3261 section 19.1.1 "Headers", #url_t, url_s#url_headers
 */
char *sip_headers_as_url_query(su_home_t *home,
			       tag_type_t tag, tag_value_t value,
			       ...)
{
  ta_list ta;
  tagi_t const *t;
  su_strlst_t *l = su_strlst_create(home);
  su_home_t *lhome = su_strlst_home(l);
  char const *retval = "";

  if (!l)
    return NULL;

  ta_start(ta, tag, value);

  for (t = ta_args(ta); t && retval; t = t_next(t)) {
    msg_hclass_t *hc;

    if (t->t_value == 0 || t->t_value == -1)
      continue;

    hc = (msg_hclass_t *)t->t_tag->tt_magic;

    if (SIPTAG_P(t->t_tag)) {
      sip_header_t const *h = (sip_header_t const *)t->t_value;
      char *s = sip_header_as_string(lhome, h);

      retval = append_escaped(l, hc, s);

      if (retval != s)
	su_free(lhome, s);
    }
    else if (SIPTAG_STR_P(t->t_tag)) {
      retval = append_escaped(l, hc, (char const *)t->t_value);
    }
  }

  ta_end(ta);

  if (retval)
    retval = su_strlst_join(l, home, "");

  su_strlst_destroy(l);

  return (char *)retval;
}

/* "[" / "]" / "/" / "?" / ":" / "+" / "$" */
#define HNV_UNRESERVED "[]/?+$"
#define HNV_RESERVED ":=,;"

/* Append a string to list and url-escape it if needed */
static
char const *append_escaped(su_strlst_t *l,
			   msg_hclass_t *hc,
			   char const *s)
{
  char const *name;

  if (hc == NULL)
    return NULL;

  if (hc->hc_hash == sip_payload_hash)
    name = "body";
  else				/* XXX - could we use short form? */
    name = hc->hc_name;

  if (name == NULL)
    return NULL;

  if (s) {
    su_home_t *lhome = su_strlst_home(l);
    size_t slen;
    isize_t elen;
    char *n, *escaped;
    char *sep = su_strlst_len(l) > 0 ? "&" : "";

    n = su_sprintf(lhome, "%s%s=", sep, name);
    if (!su_strlst_append(l, n))
      return NULL;

    for (;*n; n++)
      if (isupper(*n))
	*n = tolower(*n);

    slen = strlen(s); elen = url_esclen(s, HNV_RESERVED);

    if ((size_t)elen == slen)
      return su_strlst_append(l, s);

    escaped = su_alloc(lhome, elen + 1);
    if (escaped)
      return su_strlst_append(l, url_escape(escaped, s, HNV_RESERVED));
  }

  return NULL;
}

/** Convert URL query to a tag list.
 *
 * SIP headers encoded as URL @a query is parsed returned as a tag list.
 * Unknown headers are encoded as SIPTAG_HEADER_STR().
 *
 * @param home memory home used to alloate string (if NULL, malloc() it)
 * @param query query part from SIP URL
 * @param parser optional SIP parser used
 *
 * @sa sip_add_tl(), sip_add_tagis(), SIPTAG_HEADER_STR(),
 * sip_headers_as_url_query(), url_query_as_header_string(),
 * @RFC3261 section 19.1.1 "Headers", #url_t, url_s#url_headers
 *
 * @NEW_1_12_4.
 *
 * @bug Extension headers are ignored. The @a parser parameter is not used
 * at the moment.
 */
tagi_t *sip_url_query_as_taglist(su_home_t *home, char const *query,
				 msg_mclass_t const *parser)
{
  tagi_t *retval = NULL;
  char *s;
  su_strlst_t *l;
  isize_t N;
  size_t i, j, n;

  if (query == NULL || query[0] == '\0' || query[0] == '&')
    return NULL;

  s = su_strdup(home, query); if (!s) return NULL;
  l = su_strlst_split(home, s, "&");
  N = su_strlst_len(l);

  if (N == 0)
    goto error;

  retval = su_zalloc(home, (N + 1) * sizeof (*retval));
  if (retval == NULL)
    goto error;

  for (i = 0; i < N; i++) {
    char const *hnv;
    char *value;
    tag_type_t t;
    tag_value_t v;
    msg_hclass_t *hc = NULL;

    hnv = su_strlst_item(l, i);
    n = hnv ? strcspn(hnv, "=") : 0;
    if (n == 0)
      break;

    if (n == 4 && su_casenmatch(hnv, "body", 4))
      t = siptag_payload, hc = sip_payload_class;
    else {
      for (j = 0; (t = sip_tag_list[j]); j++) {
	hc = (msg_hclass_t *)sip_tag_list[j]->tt_magic;
	if (n == 1 && su_casenmatch(hnv, hc->hc_short, 1))
	  break;
	else if (n == (size_t)hc->hc_len &&
		 su_casenmatch(hnv, hc->hc_name, n))
	  break;
      }
    }

    value = (char *)hnv + n;
    *value++ = ':';
    n = url_unescape_to(value, value, SIZE_MAX);
    value[n] = '\0';

    if (t) {
      msg_header_t *h = msg_header_make(home, hc, value);
      if (!h)
	break;
      v = (tag_value_t)h;
    }
    else {
      char *s;
      s = su_alloc(home, n + 1);
      if (!s)
	break;
      memcpy(s, value, n + 1);
      t = siptag_header_str;
      v = (tag_value_t)s;
    }
    retval[i].t_tag = t, retval[i].t_value = v;
  }

  retval[i].t_tag = NULL, retval[i].t_value = (tag_value_t)0;

  if (i < N) {
    for (j = 0; j < i; j++) {
      if (retval[i].t_tag == siptag_header_str)
	su_free(home, (void *)retval[i].t_value);
      else
	msg_header_free_all(home, (msg_header_t *)retval[i].t_value);
    }
    su_free(home, retval);
    retval = NULL;
  }

 error:
  su_free(home, s);
  su_strlst_destroy(l);

  return retval;
}

