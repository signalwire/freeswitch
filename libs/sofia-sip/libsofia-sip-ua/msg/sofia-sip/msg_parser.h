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

#ifndef MSG_PARSER_H
/** Defined when <sofia-sip/msg_parser.h> has been included. */
#define MSG_PARSER_H

/**@ingroup msg_parser
 * @file sofia-sip/msg_parser.h
 *
 * Message parser interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Aug 21 16:03:45 2001 ppessi
 *
 */

#ifndef SU_ALLOC_H
#include <sofia-sip/su_alloc.h>
#endif
#ifndef MSG_HEADER_H
#include <sofia-sip/msg_header.h>
#endif
#ifndef BNF_H
#include <sofia-sip/bnf.h>
#endif
#ifndef URL_H
#include <sofia-sip/url.h>
#endif

SOFIA_BEGIN_DECLS

/* ---------------------------------------------------------------------------
 * 1) Header class definitions.
 */

/* Do not use keywords until you fix msg_kind_foo_critical thing! */ \
#if HAVE_STRUCT_KEYWORDS && 0
/** Define a header class */
#define MSG_HEADER_CLASS(pr, c, l, s, params, kind, dup, upd)	\
  {{								\
    hc_hash:	pr##c##_hash,					\
    hc_parse:	pr##c##_d,					\
    hc_print:	pr##c##_e,					\
    hc_dxtra:	dup##_dup_xtra,					\
    hc_dup_one: dup##_dup_one,					\
    hc_update:	upd##_update,					\
    hc_name:	l,						\
    hc_len:	sizeof(l) - 1,					\
    hc_short:	s,						\
    hc_size:	MSG_ALIGN(sizeof(pr##c##_t), sizeof(void*)),	\
    hc_params:	offsetof(pr##c##_t, params),			\
    hc_kind:	msg_kind_##kind,				\
  }}
#else
/** Define a header class */
#define MSG_HEADER_CLASS(pr, c, l, s, params, kind, dup, upd)	\
  {{ \
     pr##c##_hash, \
     pr##c##_d, \
     pr##c##_e, \
     dup##_dup_xtra, \
     dup##_dup_one, \
     upd##_update, \
     l, \
     sizeof(l) - 1, \
     s, \
     MSG_ALIGN(sizeof(pr##c##_t), sizeof(void*)), \
     offsetof(pr##c##_t, params), \
     msg_kind_##kind, \
  }}
#endif

/* Mark headers critical for understanding the message */
#define msg_kind_single_critical msg_kind_single, 1
#define msg_kind_list_critical   msg_kind_list, 1

SOFIAPUBFUN issize_t msg_extract_header(msg_t *msg, msg_pub_t *mo,
				   char b[], isize_t bsiz, int eos);
SOFIAPUBFUN issize_t msg_extract_separator(msg_t *msg, msg_pub_t *mo,
					   char b[], isize_t bsiz, int eos);
SOFIAPUBFUN issize_t msg_extract_payload(msg_t *msg, msg_pub_t *mo,
					 msg_header_t **return_payload,
					 usize_t body_len,
					 char b[], isize_t bsiz, int eos);

/* ---------------------------------------------------------------------------
 * 2) Header processing methods for common headers.
 */

SOFIAPUBFUN int msg_firstline_d(char *s, char **ss2, char **ss3);

SOFIAPUBFUN isize_t msg_default_dup_xtra(msg_header_t const *header, isize_t offset);
SOFIAPUBFUN char *msg_default_dup_one(msg_header_t *dst,
				      msg_header_t const *src,
				      char *b,
				      isize_t xtra);

SOFIAPUBFUN issize_t msg_numeric_d(su_home_t *, msg_header_t *h, char *s, isize_t slen);
SOFIAPUBFUN issize_t msg_numeric_e(char [], isize_t, msg_header_t const *, int);

SOFIAPUBFUN issize_t msg_list_d(su_home_t *, msg_header_t *h, char *s, isize_t slen);
SOFIAPUBFUN issize_t msg_list_e(char [], isize_t, msg_header_t const *, int);
SOFIAPUBFUN isize_t msg_list_dup_xtra(msg_header_t const *h, isize_t offset);
SOFIAPUBFUN char *msg_list_dup_one(msg_header_t *dst,
				   msg_header_t const *src,
				   char *b, isize_t xtra);

SOFIAPUBFUN issize_t msg_generic_d(su_home_t *, msg_header_t *, char *, isize_t);
SOFIAPUBFUN issize_t msg_generic_e(char [], isize_t, msg_header_t const *, int);
SOFIAPUBFUN isize_t msg_generic_dup_xtra(msg_header_t const *h, isize_t offset);
SOFIAPUBFUN char *msg_generic_dup_one(msg_header_t *dst,
				      msg_header_t const *src,
				      char *b,
				      isize_t xtra);

SOFIAPUBFUN isize_t msg_unknown_dup_xtra(msg_header_t const *h, isize_t offset);
SOFIAPUBFUN char *msg_unknown_dup_one(msg_header_t *dst,
				      msg_header_t const *src,
				      char *b, isize_t xtra);

SOFIAPUBFUN isize_t msg_error_dup_xtra(msg_header_t const *h, isize_t offset);
SOFIAPUBFUN char *msg_error_dup_one(msg_header_t *dst,
				    msg_header_t const *src,
				    char *b, isize_t xtra);

SOFIAPUBFUN issize_t msg_payload_d(su_home_t *, msg_header_t *h, char *s, isize_t slen);
SOFIAPUBFUN issize_t msg_payload_e(char b[], isize_t bsiz, msg_header_t const *, int f);
SOFIAPUBFUN isize_t msg_payload_dup_xtra(msg_header_t const *h, isize_t offset);
SOFIAPUBFUN char *msg_payload_dup_one(msg_header_t *dst,
				      msg_header_t const *src,
				      char *b, isize_t xtra);

SOFIAPUBFUN issize_t msg_separator_d(su_home_t *, msg_header_t *, char *, isize_t);
SOFIAPUBFUN issize_t msg_separator_e(char [], isize_t, msg_header_t const *, int);

SOFIAPUBFUN issize_t msg_auth_d(su_home_t *, msg_header_t *h, char *s, isize_t slen);
SOFIAPUBFUN issize_t msg_auth_e(char b[], isize_t bsiz, msg_header_t const *h, int f);
SOFIAPUBFUN isize_t msg_auth_dup_xtra(msg_header_t const *h, isize_t offset);
SOFIAPUBFUN char *msg_auth_dup_one(msg_header_t *dst, msg_header_t const *src,
				   char *b, isize_t xtra);

/* ---------------------------------------------------------------------------
 * 2) Macros and prototypes for building header decoding/encoding functions.
 */

#define MSG_HEADER_DATA(h) ((char *)(h) + (h)->sh_class->hc_size)

#define MSG_HEADER_TEST(h) ((h) && (h)->sh_class)

su_inline void *msg_header_data(msg_frg_t *h);

SOFIAPUBFUN int msg_hostport_d(char **ss,
			       char const **return_host,
			       char const **return_port);

SOFIAPUBFUN issize_t msg_token_d(char **ss, char const **return_token);
SOFIAPUBFUN issize_t msg_uint32_d(char **ss, uint32_t *return_value);
SOFIAPUBFUN issize_t msg_comment_d(char **ss, char const **return_comment);
SOFIAPUBFUN issize_t msg_quoted_d(char **ss, char **return_unquoted);
SOFIAPUBFUN issize_t msg_unquoted_e(char *b, isize_t bsiz, char const *s);

SOFIAPUBFUN issize_t msg_parse_next_field(su_home_t *home, msg_header_t *prev,
					  char *s, isize_t slen);

#define msg_parse_next_field_without_recursion() {				\
		msg_header_t *prev = h;									\
		msg_hclass_t *hc = prev->sh_class;						\
		char *end = s + slen;									\
																\
		if (*s && *s != ',')									\
			return -1;											\
																\
		if (msg_header_update_params(prev->sh_common, 0) < 0)	\
			return -1;											\
																\
		while (*s == ',')										\
			*s = '\0', s += span_lws(s + 1) + 1;				\
																\
		if (*s == 0)											\
			return 0;											\
																\
		h = msg_header_alloc(home, hc, 0);						\
		if (!h)													\
			return -1;											\
																\
		prev->sh_succ = h, h->sh_prev = &prev->sh_succ;			\
		prev->sh_next = h;										\
		slen = end - s;											\
	}															



/** Terminate encoding. @HI */
#define MSG_TERM_E(p, e) ((p) < (e) ? (p)[0] = '\0' : '\0')

/** Encode a character. @HI */
#define MSG_CHAR_E(p, e, c) (++(p) < (e) ? ((p)[-1]=(c)) : (c))

/** Calculate separator and string length. @HI */
#define MSG_STRING_LEN(s, sep_size) ((s) ? (strlen(s) + sep_size) : 0)

/** Encode a string. @HI */
#define MSG_STRING_E(p, e, s) do { \
  size_t _n = strlen(s); if (p + _n+1 < e) memcpy(p, s, _n+1); p+= _n; } while(0)

/** Duplicate string. @HI */
#define MSG_STRING_DUP(p, d, s) \
  (void)((s)?((p)=(char*)memccpy((void *)((d)=(char*)p),(s),0,INT_MAX))\
	    :((d)=NULL))

/* Solaris has broken memccpy - it considers last argument as signed */

/** Calculate string size. @HI */
#define MSG_STRING_SIZE(s) ((s) ? (strlen(s) + 1) : 0)

SOFIAPUBFUN issize_t msg_commalist_d(su_home_t *, char **ss,
				     msg_param_t **append_list,
				     issize_t (*scanner)(char *s));
SOFIAPUBFUN issize_t msg_token_scan(char *start);
SOFIAPUBFUN issize_t msg_attribute_value_scanner(char *s);

SOFIAPUBFUN issize_t msg_any_list_d(su_home_t *, char **ss,
				    msg_param_t **append_list,
				    issize_t (*scanner)(char *s),
				    int sep);

/** Encode a comma-separated parameter list */
#define MSG_COMMALIST_E(b, end, params, compact) do { \
  char const * const *p_; char const * c_ = ""; \
  for (p_ = (params); p_ && *p_; p_++, c_ = (compact ? "," : ", ")) \
    { MSG_STRING_E(b, (end), c_); MSG_STRING_E(b, (end), *p_); } \
} while(0)

/* Parameter lists */

SOFIAPUBFUN int msg_header_update_params(msg_common_t *h, int clear);

/** Match a parameter with any value. @HI */
#define MSG_PARAM_MATCH(v, s, name) \
  (strncasecmp(s, name "=", sizeof(name)) == 0 ? (v = s + sizeof(name)) : NULL)

/** Match a parameter with known value. @HI */
#define MSG_PARAM_MATCH_P(v, s, name) \
  ((strncasecmp((s), name "", sizeof(name) - 1) == 0 &&			\
    ((s)[sizeof(name) - 1] == '=' || (s)[sizeof(name) - 1] == '\0')) ? \
   ((v) = 1) : 0)

/** Calculate allocated number of items in parameter list. @HI */
#define MSG_PARAMS_NUM(n) (((n) + MSG_N_PARAMS - 1) & (size_t)(0 - MSG_N_PARAMS))

/** Parse a semicolong-separated attribute-value list. @HI */
SOFIAPUBFUN issize_t msg_avlist_d(su_home_t *, char **ss,
				  msg_param_t const **return_params);

/** Parse a semicolon-separated parameter list starting with semicolon. @HI */
SOFIAPUBFUN issize_t msg_params_d(su_home_t *, char **ss,
				  msg_param_t const **return_params);

/** Encode a list of parameters. */
SOFIAPUBFUN isize_t msg_params_e(char b[], isize_t bsiz, msg_param_t const pparams[]);

/** Join list of parameters */
SOFIAPUBFUN issize_t msg_params_join(su_home_t *,
				     msg_param_t **dst,
				     msg_param_t const *src,
				     unsigned prune,
				     int dup);

/** Encode a list of parameters. @HI */
#define MSG_PARAMS_E(b, end, params, flags) \
  (b) += msg_params_e((b), (size_t)((b) < (end) ? (end) - (b) : 0), (params))

/** Calculate extra size of parametes. @HI */
#define MSG_PARAMS_SIZE(rv, params) (rv = msg_params_dup_xtra(params, rv))

/** Duplicate a parameter list */
SOFIAPUBFUN char *msg_params_dup(msg_param_t const **d, msg_param_t const *s,
				 char *b, isize_t xtra);

/** Count number of parameters in the list */
su_inline isize_t msg_params_count(msg_param_t const params[])
{
  if (params) {
    size_t n;
    for (n = 0; params[n]; n++)
      ;
    return n;
  }
  else {
    return 0;
  }
}

/** Calculate memory size required by parameter list */
su_inline isize_t msg_params_dup_xtra(msg_param_t const params[], isize_t offset)
{
  isize_t n = msg_params_count(params);
  if (n) {
    MSG_STRUCT_SIZE_ALIGN(offset);
    offset += MSG_PARAMS_NUM(n + 1) * sizeof(msg_param_t);
    for (n = 0; params[n]; n++)
      offset += strlen(params[n]) + 1;
  }
  return offset;
}

/** Return pointer to extra data after header structure */
su_inline void *msg_header_data(msg_frg_t *h)
{
  if (h)
    return (char *)h + h->h_class->hc_size;
  else
    return NULL;
}

SOFIA_END_DECLS

#endif /** MSG_PARSER_H */
