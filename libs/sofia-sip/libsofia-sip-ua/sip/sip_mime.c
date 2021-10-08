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

/**@CFILE sip_mime.c
 *
 * MIME-related SIP headers
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Tue Jun 13 02:57:51 2000 ppessi
 */

#include "config.h"

/* Avoid casting sip_t to msg_pub_t and sip_header_t to msg_header_t */
#define MSG_PUB_T       struct sip_s
#define MSG_HDR_T       union sip_header_u

#include "sofia-sip/sip_parser.h"
#include "sofia-sip/msg_mime_protos.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

/* ====================================================================== */

/**@SIP_HEADER sip_accept Accept Header
 *
 * The @b Accept request-header field can be used to specify certain media
 * types which are acceptable for the response. Its syntax is defined in
 * [H14.1, S10.6] as follows:
 *
 * @code
 *    Accept         =  "Accept" HCOLON
 *                       [ accept-range *(COMMA accept-range) ]
 *    accept-range   =  media-range *(SEMI accept-param)
 *    media-range    =  ( "*" "/" "*"
 *                      / ( m-type SLASH "*" )
 *                      / ( m-type SLASH m-subtype )
 *                      ) *( SEMI m-parameter )
 *    accept-param   =  ("q" EQUAL qvalue) / generic-param
 *    qvalue         =  ( "0" [ "." 0*3DIGIT ] )
 *                      / ( "1" [ "." 0*3("0") ] )
 *    generic-param  =  token [ EQUAL gen-value ]
 *    gen-value      =  token / host / quoted-string
 * @endcode
 *
 *
 * The parsed Accept header is stored in #sip_accept_t structure.
 */

/**@ingroup sip_accept
 * @typedef typedef struct sip_accept_s sip_accept_t;
 *
 * The structure #sip_accept_t contains representation of SIP
 * @Accept header.
 *
 * The #sip_accept_t is defined as follows:
 * @code
 * typedef struct sip_accept_s {
 *   sip_common_t        ac_common[1]; // Common fragment info
 *   sip_accept_t       *ac_next;      // Pointer to next @Acceptheader
 *   char const         *ac_type;      // Pointer to type/subtype
 *   char const         *ac_subtype;   // Points after first slash in type
 *   msg_param_t const  *ac_params;    // List of parameters
 *   char const         *ac_q;         // Value of q parameter
 * } sip_accept_t;
 * @endcode
 */

#define sip_accept_dup_xtra msg_accept_dup_xtra
#define sip_accept_dup_one  msg_accept_dup_one
#define sip_accept_update   msg_accept_update

msg_hclass_t sip_accept_class[] =
SIP_HEADER_CLASS(accept, "Accept", "", ac_params, apndlist, accept);

issize_t sip_accept_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return msg_accept_d(home, h, s, slen);
}

issize_t sip_accept_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  return msg_accept_e(b, bsiz, h, flags);
}

#if SIP_HAVE_ACCEPT_DISPOSITION
/* ====================================================================== */

/**@SIP_HEADER sip_accept_disposition Accept-Disposition Header
 *
 * The Accept-Disposition header field is used to indicate what content
 * disposition types are acceptable to a client or server.  Its syntax is
 * defined in draft-lennox-sip-reg-payload-01.txt section 3.2 as follows:
 *
 * @code
 *    Accept-Disposition = "Accept-Disposition" ":"
 *                         #( (disposition-type | "*") *( ";" generic-param ) )
 * @endcode
 *
 *
 * The parsed Accept-Disposition header
 * is stored in #sip_accept_disposition_t structure.
 */

msg_hclass_t sip_accept_disposition_class[] =
SIP_HEADER_CLASS(accept_disposition, "Accept-Disposition", "",
		 ad_params, apndlist, accept_disposition);

issize_t sip_accept_disposition_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
	sip_accept_disposition_t *ad;

	assert(h);

	for(;;) {
		ad = (sip_accept_disposition_t *)h;
		/* Ignore empty entries (comma-whitespace) */
		while (*s == ',')
			s += span_lws(s + 1) + 1;

		/* "Accept:" #(type/subtyp ; *(parameters))) */
		if (/* Parse protocol */
			sip_version_d(&s, &ad->ad_type) == -1 ||
			(ad->ad_subtype = strchr(ad->ad_type, '/')) == NULL ||
			(*s == ';' && msg_params_d(home, &s, &ad->ad_params) == -1))
			return -1;

		if (ad->ad_subtype) ad->ad_subtype++;

		msg_parse_next_field_without_recursion();
	}

}

issize_t sip_accept_disposition_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  char *b0 = b, *end = b + bsiz;
  sip_accept_disposition_t const *ad = h->sh_accept_disposition;

  MSG_STRING_E(b, end, ad->ad_type);
  MSG_PARAMS_E(b, end, ad->ad_params, flags);
  MSG_TERM_E(b, end);

  return b - b0;
}
#endif

/* ====================================================================== */

/**@SIP_HEADER sip_accept_encoding Accept-Encoding Header
 *
 * The Accept-Encoding header is similar to Accept, but restricts the
 * content-codings that are acceptable in the response.  Its syntax is
 * defined in [H14.3, S10.7] as follows:
 *
 * @code
 *    Accept-Encoding  =  "Accept-Encoding" HCOLON
 *                         [ encoding *(COMMA encoding) ]
 *    encoding         =  codings *(SEMI accept-param)
 *    codings          =  content-coding / "*"
 *    content-coding   =  token
 * @endcode
 *
 *
 * The parsed Accept-Encoding header
 * is stored in #sip_accept_encoding_t structure.
 */

/**@ingroup sip_accept_encoding
 * @typedef typedef struct msg_accept_any_s sip_accept_encoding_t;
 *
 * The structure #sip_accept_encoding_t contains representation of SIP
 * @AcceptEncoding header.
 *
 * The #sip_accept_encoding_t is defined as follows:
 * @code
 * typedef struct {
 *   msg_common_t        aa_common[1]; // Common fragment info
 *   sip_accept_encoding_t *aa_next;   // Pointer to next @AcceptEncoding header
 *   char const            *aa_value;  // Encoding token
 *   msg_param_t const     *aa_params; // List of parameters
 *   char const            *aa_q;      // Value of q parameter
 * } sip_accept_encoding_t;
 * @endcode
 */

#define sip_accept_encoding_dup_xtra msg_accept_any_dup_xtra
#define sip_accept_encoding_dup_one  msg_accept_any_dup_one
#define sip_accept_encoding_update   msg_accept_any_update

msg_hclass_t sip_accept_encoding_class[] =
SIP_HEADER_CLASS(accept_encoding, "Accept-Encoding", "",
		 aa_params, apndlist, accept_encoding);

issize_t sip_accept_encoding_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  issize_t retval = msg_accept_encoding_d(home, h, s, slen);

  if (retval == -2) {
    /* Empty Accept-Encoding list is not an error */
    sip_accept_encoding_t *aa = (sip_accept_encoding_t *)h;
    aa->aa_value = "";
    retval = 0;
  }

  return retval;
}

issize_t sip_accept_encoding_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  return msg_accept_encoding_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@SIP_HEADER sip_accept_language Accept-Language Header
 *
 * The Accept-Language header can be used to allow the client to indicate to
 * the server in which language it would prefer to receive reason phrases,
 * session descriptions or status responses carried as message bodies.  Its
 * syntax is defined in [H14.4, S10.8] as follows:
 *
 * @code
 *    Accept-Language  =  "Accept-Language" HCOLON
 *                         [ language *(COMMA language) ]
 *    language         =  language-range *(SEMI accept-param)
 *    language-range   =  ( ( 1*8ALPHA *( "-" 1*8ALPHA ) ) / "*" )
 * @endcode
 *
 *
 * The parsed Accept-Language header
 * is stored in #sip_accept_language_t structure.
 */

/**@ingroup sip_accept_language
 * @typedef typedef struct msg_accept_any_s sip_accept_language_t;
 *
 * The structure #sip_accept_language_t contains representation of SIP
 * @AcceptLanguage header.
 *
 * The #sip_accept_language_t is defined as follows:
 * @code
 * typedef struct {
 *   msg_common_t        aa_common[1]; // Common fragment info
 *   sip_accept_language_t *aa_next;   // Pointer to next <language>
 *   char const            *aa_value;  // Language-range
 *   msg_param_t const     *aa_params; // List of accept-parameters
 *   char const            *aa_q;      // Value of q parameter
 * } sip_accept_language_t;
 * @endcode
 */

#define sip_accept_language_dup_xtra msg_accept_any_dup_xtra
#define sip_accept_language_dup_one  msg_accept_any_dup_one
#define sip_accept_language_update   msg_accept_any_update

msg_hclass_t sip_accept_language_class[] =
SIP_HEADER_CLASS(accept_language, "Accept-Language", "",
		 aa_params, apndlist, accept_language);

issize_t sip_accept_language_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  int retval = msg_accept_language_d(home, h, s, slen);

  if (retval == -2) {
    /* Empty Accept-Language list is not an error */
    ((sip_accept_language_t *)h)->aa_value = "";
    retval = 0;
  }

  return retval;
}

issize_t sip_accept_language_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  return msg_accept_language_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@SIP_HEADER sip_content_disposition Content-Disposition Header
 *
 * The Content-Disposition header field describes how the message body or,
 * in the case of multipart messages, a message body part is to be
 * interpreted by the UAC or UAS.  Its syntax is defined in @RFC3261
 * as follows:
 *
 * @code
 *    Content-Disposition   =  "Content-Disposition" HCOLON
 *                             disp-type *( SEMI disp-param )
 *    disp-type             =  "render" / "session" / "icon" / "alert"
 *                             / disp-extension-token
 *    disp-param            =  handling-param / generic-param
 *    handling-param        =  "handling" EQUAL
 *                             ( "optional" / "required"
 *                             / other-handling )
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
 *    disposition-type         =  "script" | "sip-cgi" | token
 *    disposition-param        =  action-param
 *                             |  modification-date-param
 *                             |  generic-param
 *    action-param             =  "action" "=" action-value
 *    action-value             =  "store" | "remove" | token
 *    modification-date-param  =  "modification-date" "=" quoted-date-time
 *    quoted-date-time         =  <"> SIP-date <">
 * @endcode
 *
 * The parsed Content-Disposition header
 * is stored in #sip_content_disposition_t structure.
 */

/**@ingroup sip_content_disposition
 * @typedef struct msg_content_disposition_s sip_content_disposition_t;
 *
 * The structure #sip_content_disposition_t contains representation of an
 * @ContentDisposition header.
 *
 * The #sip_content_disposition_t is defined as follows:
 * @code
 * typedef struct msg_content_disposition_s
 * {
 *   msg_common_t       cd_common[1];  // Common fragment info
 *   msg_error_t       *cd_next;       // Link to next (dummy)
 *   char const        *cd_type;       // Disposition type
 *   msg_param_t const *cd_params;     // List of parameters
 *   char const        *cd_handling;   // Value of @b handling parameter
 *   unsigned           cd_required:1; // True if handling=required
 *   unsigned           cd_optional:1; // True if handling=optional
 * } sip_content_disposition_t;
 * @endcode
 */

static msg_xtra_f sip_content_disposition_dup_xtra;
static msg_dup_f sip_content_disposition_dup_one;
#define sip_content_disposition_update msg_content_disposition_update

msg_hclass_t sip_content_disposition_class[] =
SIP_HEADER_CLASS(content_disposition, "Content-Disposition", "", cd_params,
		 single, content_disposition);

issize_t sip_content_disposition_d(su_home_t *home, sip_header_t *h,
				   char *s, isize_t slen)
{
  return msg_content_disposition_d(home, h, s, slen);
}

issize_t sip_content_disposition_e(char b[], isize_t bsiz,
				   sip_header_t const *h, int f)
{
  return msg_content_disposition_e(b, bsiz, h, f);
}

static
isize_t sip_content_disposition_dup_xtra(sip_header_t const *h, isize_t offset)
{
  return msg_content_disposition_dup_xtra(h, offset);
}

/** Duplicate one #sip_content_disposition_t object */
static
char *sip_content_disposition_dup_one(sip_header_t *dst,
				      sip_header_t const *src,
				      char *b, isize_t xtra)
{
  return msg_content_disposition_dup_one(dst, src, b, xtra);
}


/* ====================================================================== */

/**@SIP_HEADER sip_content_encoding Content-Encoding Header
 *
 * The Content-Encoding header indicates what additional content codings
 * have been applied to the entity-body.  Its syntax is defined in @RFC3261
 * as follows:
 *
 * @code
 * Content-Encoding  =  ( "Content-Encoding" / "e" ) HCOLON
 *                      content-coding *(COMMA content-coding)
 * content-coding    =  token
 * @endcode
 *
 * The parsed Content-Encoding header
 * is stored in #sip_content_encoding_t structure.
 */

/**@ingroup sip_content_encoding
 * @typedef struct msg_list_s sip_content_encoding_t;
 *
 * The structure #sip_content_encoding_t contains representation of an
 * @ContentEncoding header.
 *
 * The #sip_content_encoding_t is defined as follows:
 * @code
 * typedef struct msg_list_s
 * {
 *   msg_common_t       k_common[1];  // Common fragment info
 *   msg_list_t        *k_next;	      // Link to next header
 *   msg_param_t       *k_items;      // List of items
 * } sip_content_encoding_t;
 * @endcode
 */

msg_hclass_t sip_content_encoding_class[] =
SIP_HEADER_CLASS_LIST(content_encoding, "Content-Encoding", "e", list);

issize_t sip_content_encoding_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return msg_list_d(home, h, s, slen);
}

issize_t sip_content_encoding_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  return msg_list_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@SIP_HEADER sip_content_language Content-Language Header
 *
 * The Content-Language header @RFC2616 section 14.12 describes the natural language(s) of
 * the intended audience for the enclosed entity. Note that this might not
 * be equivalent to all the languages used within the entity-body. Its
 * syntax is defined in @RFC3261 as follows:
 *
 * @code
 *    Content-Language  =  "Content-Language" HCOLON
 *                         language-tag *(COMMA language-tag)
 *    language-tag      =  primary-tag *( "-" subtag )
 *    primary-tag       =  1*8ALPHA
 *    subtag            =  1*8ALPHA
 * @endcode
 *
 * The parsed Content-Language header
 * is stored in #sip_content_language_t structure.
 */

/**@ingroup sip_content_language
 * @typedef typedef struct msg_content_language_s sip_content_language_t;
 *
 * The structure #sip_content_language_t contains representation of
 * @ContentLanguage header.
 *
 * The #sip_content_language_t is defined as follows:
 * @code
 * typedef struct {
 *   msg_common_t            k_common[1]; // Common fragment info
 *   msg_content_language_t *k_next;      // (Content-Encoding header)
 *   msg_param_t            *k_items;     // List of languages
 * } sip_content_language_t;
 * @endcode
 */

msg_hclass_t sip_content_language_class[] =
SIP_HEADER_CLASS_LIST(content_language, "Content-Language", "", list);

issize_t sip_content_language_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return msg_list_d(home, h, s, slen);
}

issize_t sip_content_language_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  return msg_list_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@SIP_HEADER sip_content_type Content-Type Header
 *
 * The Content-Type header indicates the media type of the message-body sent
 * to the recipient.  Its syntax is defined in [H3.7, S] as
 * follows:
 *
 * @code
 * Content-Type     =  ( "Content-Type" / "c" ) HCOLON media-type
 * media-type       =  m-type SLASH m-subtype *(SEMI m-parameter)
 * m-type           =  discrete-type / composite-type
 * discrete-type    =  "text" / "image" / "audio" / "video"
 *                     / "application" / extension-token
 * composite-type   =  "message" / "multipart" / extension-token
 * extension-token  =  ietf-token / x-token
 * ietf-token       =  token
 * x-token          =  "x-" token
 * m-subtype        =  extension-token / iana-token
 * iana-token       =  token
 * m-parameter      =  m-attribute EQUAL m-value
 * m-attribute      =  token
 * m-value          =  token / quoted-string
 * @endcode
 *
 * The parsed Content-Type header is stored in #sip_content_type_t structure.
 */

/**@ingroup sip_content_type
 * @typedef typedef struct sip_content_type_s sip_content_type_t;
 *
 * The structure #sip_content_type_t contains representation of SIP
 * @ContentType header.
 *
 * The #sip_content_type_t is defined as follows:
 * @code
 * typedef struct sip_content_type_s {
 *   sip_common_t        c_common[1];  // Common fragment info
 *   sip_unknown_t      *c_next;       // Dummy link to next
 *   char const         *c_type;       // Pointer to type/subtype
 *   char const         *c_subtype;    // Points after first slash in type
 *   msg_param_t const  *c_params;     // List of parameters
 * } sip_content_type_t;
 * @endcode
 *
 * The whitespace in the @a c_type is always removed when parsing.
 */

static msg_xtra_f sip_content_type_dup_xtra;
static msg_dup_f sip_content_type_dup_one;
#define sip_content_type_update NULL

msg_hclass_t sip_content_type_class[] =
SIP_HEADER_CLASS(content_type, "Content-Type", "c", c_params,
		 single, content_type);

issize_t sip_content_type_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return msg_content_type_d(home, (msg_header_t *)h, s, slen);
}

issize_t sip_content_type_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  return msg_content_type_e(b, bsiz, (msg_header_t const *)h, flags);
}

static
isize_t sip_content_type_dup_xtra(sip_header_t const *h, isize_t offset)
{
  return msg_content_type_dup_xtra((msg_header_t *)h, offset);
}

/** Duplicate one #sip_content_type_t object */
static
char *sip_content_type_dup_one(sip_header_t *dst, sip_header_t const *src,
			       char *b, isize_t xtra)
{
  return msg_content_type_dup_one((msg_header_t *)dst,
				  (msg_header_t const *)src,
				  b, xtra);
}

/* ====================================================================== */

/**@SIP_HEADER sip_mime_version MIME-Version Header
 *
 * MIME-Version header indicates what version of the MIME protocol was used
 * to construct the message.  Its syntax is defined in [H19.4.1, S10.28]
 * as follows:
 *
 * @code
 *    MIME-Version  =  "MIME-Version" HCOLON 1*DIGIT "." 1*DIGIT
 * @endcode
 *
 * The parsed MIME-Version header is stored in #sip_mime_version_t structure.
 */

/**@ingroup sip_mime_version
 * @typedef struct msg_generic_s sip_mime_version_t;
 *
 * The structure #sip_mime_version_t contains representation of an
 * @MIMEVersion header.
 *
 * The #sip_mime_version_t is defined as follows:
 * @code
 * typedef struct msg_generic_s
 * {
 *   msg_common_t   g_common[1];    // Common fragment info
 *   msg_generic_t *g_next;	    // Link to next header
 *   char const    *g_string;	    // Header value
 * } sip_mime_version_t;
 * @endcode
 */

msg_hclass_t sip_mime_version_class[] =
SIP_HEADER_CLASS_G(mime_version, "MIME-Version", "", single);

issize_t sip_mime_version_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return sip_generic_d(home, h, s, slen);
}

issize_t sip_mime_version_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  return sip_generic_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@SIP_HEADER sip_warning Warning Header
 *
 * The Warning response-header field is used to carry additional information
 * about the status of a response. Its syntax is defined in @RFC3261 as
 * follows:
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
 *
 * The parsed Warning header is stored in #sip_warning_t structure.
 */

/**@ingroup sip_warning
 * @typedef struct msg_warning_s sip_warning_t;
 *
 * The structure #sip_warning_t contains representation of an
 * @Warning header.
 *
 * The #sip_warning_t is defined as follows:
 * @code
 * typedef struct msg_warning_s
 * {
 *   msg_common_t        w_common[1];  // Common fragment info
 *   msg_warning_t      *w_next;       // Link to next @Warning header
 *   unsigned            w_code;       // Warning code
 *   char const         *w_host;       // Hostname or pseudonym
 *   char const         *w_port;       // Port number
 *   char const         *w_text;       // Warning text
 * } sip_warning_t;
 * @endcode
 */

#define sip_warning_dup_xtra msg_warning_dup_xtra
#define sip_warning_dup_one msg_warning_dup_one
#define sip_warning_update NULL

msg_hclass_t sip_warning_class[] =
SIP_HEADER_CLASS(warning, "Warning", "", w_common, append, warning);

issize_t sip_warning_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return msg_warning_d(home, h, s, slen);
}
issize_t sip_warning_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  return msg_warning_e(b, bsiz, h, f);
}
