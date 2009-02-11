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

#ifndef SIP_PARSER_H
/** Defined when <sofia-sip/sip_parser.h> has been included.*/
#define SIP_PARSER_H


/**@ingroup sip_parser
 * @file sofia-sip/sip_parser.h
 *
 * SIP parser provider interface.
 *
 * This file contains functions and macros used to create a SIP parser using
 * generic text message parser, and to define new SIP header classes.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Thu Mar  8 15:13:11 2001 ppessi
 */

#ifndef SU_ALLOC_H
#include <sofia-sip/su_alloc.h>
#endif

#ifndef MSG_H
#include <sofia-sip/msg.h>
#endif

#ifndef MSG_PARSER_H
#include <sofia-sip/msg_parser.h>
#endif

#ifndef SIP_H
#include <sofia-sip/sip.h>
#endif

#ifndef SIP_HEADER_H
#include <sofia-sip/sip_header.h>
#endif

#ifndef SOFIA_SIP_SU_STRING_H
#include <sofia-sip/su_string.h>
#endif

SOFIA_BEGIN_DECLS

/* ---------------------------------------------------------------------------
 * 1) Macros for defining boilerplate functions and structures for each header
 */

#define SIP_HDR_TEST(x)    ((x)->sh_class)

/** Define a header class for a SIP header. @HIDE */
#define SIP_HEADER_CLASS(c, l, s, params, kind, dup)		\
  MSG_HEADER_CLASS(sip_, c, l, s, params, kind, sip_ ## dup, sip_ ## dup)

/** Define a header class for a critical SIP header. @HIDE */
#define SIP_HEADER_CLASS_C(c, l, s, params, kind, dup)	\
  MSG_HEADER_CLASS_C(sip_, c, l, s, params, kind, sip_ ## dup, sip_ ## dup)

/** Define a header class for headers without any extra data to copy. @HIDE  */
#define SIP_HEADER_CLASS_G(c, l, s, kind) \
  MSG_HEADER_CLASS(sip_, c, l, s, g_common, kind, msg_generic, sip_null)

/** Define a header class for a msg_list_t kind of header. @HIDE */
#define SIP_HEADER_CLASS_LIST(c, l, s, kind) \
  MSG_HEADER_CLASS(sip_, c, l, s, k_items, kind, msg_list, sip_null)

/** Define a authorization header class. @HIDE */
#define SIP_HEADER_CLASS_AUTH(c, l, kind) \
  MSG_HEADER_CLASS(sip_, c, l, "", au_params, kind, msg_auth, sip_null)

#define sip_null_update NULL
#define sip_any_update NULL

/* ---------------------------------------------------------------------------
 * 2) Prototypes for internal decoding/encoding functions
 */

/* Version string */
SOFIAPUBFUN int sip_version_d(char **ss, char const **ver);
SOFIAPUBFUN isize_t sip_version_xtra(char const *version);
SOFIAPUBFUN void sip_version_dup(char **pp, char const **dd, char const *s);

/* Transport identifiers */
#define SIP_TRANSPORT_LEN(s) SIP_STRING_SIZE((s))
SOFIAPUBFUN issize_t sip_transport_d(char **ss, char const **ttransport);
SOFIAPUBFUN isize_t sip_transport_xtra(char const *transport);
SOFIAPUBFUN void sip_transport_dup(char **pp, char const **dd, char const *s);

/* Method */
SOFIAPUBFUN sip_method_t sip_method_d(char **ss, char const **nname);

/* Call-ID */
SOFIAPUBFUN char *sip_word_at_word_d(char **ss);

/** Extract SIP message body, including separator line. */
SOFIAPUBFUN issize_t sip_extract_body(msg_t *, sip_t *, char b[], isize_t bsiz, int eos);

SOFIAPUBFUN issize_t sip_any_route_d(su_home_t *, sip_header_t *, char *s, isize_t slen);
SOFIAPUBFUN issize_t sip_any_route_e(char [], isize_t, sip_header_t const *, int flags);
SOFIAPUBFUN isize_t sip_any_route_dup_xtra(sip_header_t const *h, isize_t offset);
SOFIAPUBFUN char *sip_any_route_dup_one(sip_header_t *dst,
					sip_header_t const *src,
					char *b, isize_t xtra);
#define sip_any_route_update NULL

SOFIAPUBFUN issize_t sip_name_addr_d(su_home_t *home,
				     char **inout_s,
				     char const **return_display,
				     url_t *out_url,
				     msg_param_t const **return_params,
				     char const **return_comment);

SOFIAPUBFUN issize_t sip_name_addr_e(char b[], isize_t bsiz,
				     int flags,
				     char const *display,
				     int always_ltgt, url_t const url[],
				     msg_param_t const params[],
				     char const *comment);

SOFIAPUBFUN isize_t sip_name_addr_xtra(char const *display, url_t const *addr,
				       msg_param_t const params[],
				       isize_t offset);

SOFIAPUBFUN char *sip_name_addr_dup(char const **d_display, char const *display,
				    url_t *d_addr, url_t const *addr,
				    msg_param_t const **d_params,
				    msg_param_t const params[],
				    char *b, isize_t xtra);


/* ---------------------------------------------------------------------------
 * 3) Compatibility macros and functions
 */

#define sip_generic_d		msg_generic_d
#define sip_generic_e		msg_generic_e

#define sip_numeric_d		msg_numeric_d
#define sip_numeric_e		msg_numeric_e

#define sip_any_copy_xtra	msg_default_copy_xtra
#define sip_any_copy_one	msg_default_copy_one
#define sip_any_dup_xtra	msg_default_dup_xtra
#define sip_any_dup_one		msg_default_dup_one

#define sip_generic_dup_xtra	msg_generic_dup_xtra
#define sip_generic_dup_one	msg_generic_dup_one


#define	sip_auth_d              msg_auth_d
#define	sip_auth_e              msg_auth_e

#define sip_header_dup_as	msg_header_dup_as
#define sip_header_alloc        msg_header_alloc
#define sip_header_copy_as	msg_header_copy_as

#define SIP_ALIGN               MSG_ALIGN
#define SIP_STRUCT_SIZE_ALIGN   MSG_STRUCT_SIZE_ALIGN
#define SIP_STRUCT_ALIGN        MSG_STRUCT_ALIGN

#define sip_comment_d		msg_comment_d
#define sip_quoted_d(ss, qq)	msg_quoted_d(ss, qq)

#define SIP_CHAR_E              MSG_CHAR_E
#define SIP_STRING_LEN          MSG_STRING_LEN
#define SIP_STRING_E		MSG_STRING_E
#define SIP_STRING_DUP		MSG_STRING_DUP
#define SIP_STRING_SIZE		MSG_STRING_SIZE
#define SIP_NAME_E		MSG_NAME_E

/* Parameters */
#define SIP_PARAM_MATCH		MSG_PARAM_MATCH
#define SIP_PARAM_MATCH_P	MSG_PARAM_MATCH_P

/* Parameter lists */
#define SIP_N_PARAMS            MSG_N_PARAMS
#define sip_params_d		msg_params_d
#define sip_params_dup		msg_params_dup
#define SIP_PARAMS_NUM		MSG_PARAMS_NUM
#define SIP_PARAMS_E		MSG_PARAMS_E
#define SIP_PARAMS_SIZE		MSG_PARAMS_SIZE
#define sip_params_count	msg_params_count
#define sip_params_copy_xtra	msg_params_copy_xtra
#define sip_params_copy		msg_params_copy

SOFIAPUBFUN int sip_generic_xtra(sip_generic_t const *g);

SOFIAPUBFUN sip_generic_t *sip_generic_dup(su_home_t *home,
					   msg_hclass_t *hc,
					   sip_generic_t const *u);

SOFIAPUBFUN sip_generic_t *sip_generic_copy(su_home_t *home,
					    msg_hclass_t *hc,
					    sip_generic_t const *o);

SOFIA_END_DECLS

#endif
