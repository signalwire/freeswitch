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

/**@CFILE sip_security.c
 *
 * Security-related SIP header handling.
 *
 * This file contains implementation of headers related to HTTP authentication
 * (@RFC2617):
 * @ref sip_authorization "Authorization",
 * @ref sip_authentication_info "Authentication-Info",
 * @ref sip_proxy_authenticate "Proxy-Authenticate",
 * @ref sip_proxy_authentication_info "Proxy-Authentication-Info",
 * @ref sip_proxy_authorization "Proxy-Authorization", and
 * @ref sip_www_authenticate "WWW-Authenticate".
 *
 * There is also implementation of headers related to security agreement
 * (@RFC3329):
 * @ref sip_security_client "Security-Client",
 * @ref sip_security_server "Security-Server", and
 * @ref sip_security_verify "Security-Verify" headers.
 *
 * The implementation of @ref sip_privacy "Privacy" header (@RFC3323) is
 * also here.
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

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

/* ====================================================================== */

/**@SIP_HEADER sip_authorization Authorization Header
 *
 * The Authorization header consists of credentials containing the
 * authentication information of the user agent for the realm of the
 * resource being requested. Its syntax is defined in @RFC2617 and @RFC3261
 * as follows:
 *
 * @code
 *    Authorization     =  "Authorization" HCOLON credentials
 *    credentials       =  ("Digest" LWS digest-response)
 *                         / other-response
 *    digest-response   =  dig-resp *(COMMA dig-resp)
 *    dig-resp          =  username / realm / nonce / digest-uri
 *                          / dresponse / algorithm / cnonce
 *                          / opaque / message-qop
 *                          / nonce-count / auth-param
 *    username          =  "username" EQUAL username-value
 *    username-value    =  quoted-string
 *    digest-uri        =  "uri" EQUAL LDQUOT digest-uri-value RDQUOT
 *    digest-uri-value  =  rquest-uri ; Equal to request-uri as specified
 *                         by HTTP/1.1
 *    message-qop       =  "qop" EQUAL qop-value
 *    cnonce            =  "cnonce" EQUAL cnonce-value
 *    cnonce-value      =  nonce-value
 *    nonce-count       =  "nc" EQUAL nc-value
 *    nc-value          =  8LHEX
 *    dresponse         =  "response" EQUAL request-digest
 *    request-digest    =  LDQUOT 32LHEX RDQUOT
 *    auth-param        =  auth-param-name EQUAL
 *                         ( token / quoted-string )
 *    auth-param-name   =  token
 *    other-response    =  auth-scheme LWS auth-param
 *                         *(COMMA auth-param)
 *    auth-scheme       =  token
 * @endcode
 *
 * The parsed Authorization header
 * is stored in #sip_authorization_t structure.
 *
 * @sa @RFC2617, auth_mod_verify(), auth_mod_check(), auth_get_params(),
 * auth_digest_response_get().
 */

/**@ingroup sip_authorization
 * @typedef typedef struct sip_authorization_s sip_authorization_t;
 *
 * The structure #sip_authorization_t contains representation of SIP
 * @Authorization header.
 *
 * The #sip_authorization_t is defined as follows:
 * @code
 * typedef struct msg_auth_s {
 *   msg_common_t       au_common[1];  // Common fragment info
 *   msg_auth_t        *au_next;       // Link to next header
 *   char const        *au_scheme;     // Auth-scheme like "Basic" or "Digest"
 *   msg_param_t const *au_params;     // Comma-separated parameters
 * } sip_authorization_t;
 * @endcode
 *
 */

msg_hclass_t sip_authorization_class[] =
SIP_HEADER_CLASS_AUTH(authorization, "Authorization", append);

issize_t sip_authorization_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return msg_auth_d(home, h, s, slen);
}

issize_t sip_authorization_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  assert(sip_is_authorization(h));
  return msg_auth_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@SIP_HEADER sip_proxy_authenticate Proxy-Authenticate Header
 *
 * The Proxy-Authenticate header consists of a challenge that indicates the
 * authentication scheme and parameters applicable to the proxy.  Its syntax
 * is defined in [H14.33, S10.31] as follows:
 *
 * @code
 *    Proxy-Authenticate  =  "Proxy-Authenticate" HCOLON challenge
 *    challenge           =  ("Digest" LWS digest-cln *(COMMA digest-cln))
 *                           / other-challenge
 *    other-challenge     =  auth-scheme LWS auth-param
 *                           *(COMMA auth-param)
 *    digest-cln          =  realm / domain / nonce
 *                            / opaque / stale / algorithm
 *                            / qop-options / auth-param
 *    realm               =  "realm" EQUAL realm-value
 *    realm-value         =  quoted-string
 *    domain              =  "domain" EQUAL LDQUOT URI
 *                           *( 1*SP URI ) RDQUOT
 *    URI                 =  absoluteURI / abs-path
 *    nonce               =  "nonce" EQUAL nonce-value
 *    nonce-value         =  quoted-string
 *    opaque              =  "opaque" EQUAL quoted-string
 *    stale               =  "stale" EQUAL ( "true" / "false" )
 *    algorithm           =  "algorithm" EQUAL ( "MD5" / "MD5-sess"
 *                           / token )
 *    qop-options         =  "qop" EQUAL LDQUOT qop-value
 *                           *("," qop-value) RDQUOT
 *    qop-value           =  "auth" / "auth-int" / token
 * @endcode
 *
 *
 * The parsed Proxy-Authenticate header
 * is stored in #sip_proxy_authenticate_t structure.
 */

/**@ingroup sip_proxy_authenticate
 * @typedef typedef struct sip_proxy_authenticate_s sip_proxy_authenticate_t;
 *
 * The structure #sip_proxy_authenticate_t contains representation of SIP
 * @ProxyAuthenticate header.
 *
 * The #sip_proxy_authenticate_t is defined as follows:
 * @code
 * typedef struct msg_auth_s {
 *   msg_common_t       au_common[1];  // Common fragment info
 *   msg_auth_t        *au_next;       // Link to next header
 *   char const        *au_scheme;     // Auth-scheme like "Basic" or "Digest"
 *   msg_param_t const *au_params;     // Comma-separated parameters
 * } sip_proxy_authenticate_t;
 * @endcode
 *
 */

msg_hclass_t sip_proxy_authenticate_class[] =
SIP_HEADER_CLASS_AUTH(proxy_authenticate, "Proxy-Authenticate", append);

issize_t sip_proxy_authenticate_d(su_home_t *home, sip_header_t *h,
				  char *s, isize_t slen)
{
  return msg_auth_d(home, h, s, slen);
}

issize_t sip_proxy_authenticate_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  assert(sip_is_proxy_authenticate(h));
  return msg_auth_e(b, bsiz, h, f);
}


/* ====================================================================== */

/**@SIP_HEADER sip_proxy_authorization Proxy-Authorization Header
 *
 * The Proxy-Authorization header consists of credentials containing the
 * authentication information of the user agent for the proxy and/or realm
 * of the resource being requested.  Its syntax is defined in @RFC3261
 * as follows:
 *
 * @code
 *    Proxy-Authorization  = "Proxy-Authorization" ":" credentials
 *    credentials          =  ("Digest" LWS digest-response)
 *                            / other-response
 * @endcode
 *
 * @sa auth_mod_verify(), auth_mod_check(), auth_get_params(),
 * auth_digest_response_get().
 *
 * The parsed Proxy-Authorization header
 * is stored in #sip_proxy_authorization_t structure.
 */

/**@ingroup sip_proxy_authorization
 * @typedef typedef struct sip_proxy_authorization_s sip_proxy_authorization_t;
 *
 * The structure #sip_proxy_authorization_t contains representation of SIP
 * @ProxyAuthorization header.
 *
 * The #sip_proxy_authorization_t is defined as follows:
 * @code
 * typedef struct msg_auth_s {
 *   msg_common_t       au_common[1];  // Common fragment info
 *   msg_auth_t        *au_next;       // Link to next header
 *   char const        *au_scheme;     // Auth-scheme like "Basic" or "Digest"
 *   msg_param_t const *au_params;     // Comma-separated parameters
 * } sip_proxy_authorization_t;
 * @endcode
 *
 */

msg_hclass_t sip_proxy_authorization_class[] =
SIP_HEADER_CLASS_AUTH(proxy_authorization, "Proxy-Authorization", append);

issize_t sip_proxy_authorization_d(su_home_t *home, sip_header_t *h,
				   char *s, isize_t slen)
{
  return msg_auth_d(home, h, s, slen);
}

issize_t sip_proxy_authorization_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  assert(sip_is_proxy_authorization(h));
  return msg_auth_e(b, bsiz, h, f);
}

/* ====================================================================== */

/**@SIP_HEADER sip_www_authenticate WWW-Authenticate Header
 *
 * The WWW-Authenticate header consists of at least one challenge that
 * indicates the authentication scheme(s) and parameters applicable to the
 * Request-URI.  Its syntax is defined in @RFC3261 as
 * follows:
 *
 * @code
 *    WWW-Authenticate  = "WWW-Authenticate" HCOLON challenge
 *    challenge         =  ("Digest" LWS digest-cln *(COMMA digest-cln))
 *                      / other-challenge
 *    other-challenge   =  auth-scheme LWS auth-param *(COMMA auth-param)
 * @endcode
 *
 * See @ProxyAuthenticate for the definition of \<digest-cln\>.
 *
 * The parsed WWW-Authenticate header
 * is stored in #sip_www_authenticate_t structure.
 */

/**@ingroup sip_www_authenticate
 * @typedef typedef struct sip_www_authenticate_s sip_www_authenticate_t;
 *
 * The structure #sip_www_authenticate_t contains representation of SIP
 * @WWWAuthenticate header.
 *
 * The #sip_www_authenticate_t is defined as follows:
 * @code
 * typedef struct msg_auth_s {
 *   msg_common_t       au_common[1];  // Common fragment info
 *   msg_auth_t        *au_next;       // Link to next header
 *   char const        *au_scheme;     // Auth-scheme like "Basic" or "Digest"
 *   msg_param_t const *au_params;     // Comma-separated parameters
 * } sip_www_authenticate_t;
 * @endcode
 *
 */

msg_hclass_t sip_www_authenticate_class[] =
SIP_HEADER_CLASS_AUTH(www_authenticate, "WWW-Authenticate", append);

issize_t sip_www_authenticate_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return msg_auth_d(home, h, s, slen);
}

issize_t sip_www_authenticate_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  assert(sip_is_www_authenticate(h));
  return msg_auth_e(b, bsiz, h, f);
}

/**@SIP_HEADER sip_authentication_info Authentication-Info Header
 *
 * The @b Authentication-Info header contains either a next-nonce used by
 * next request and/or authentication from server used in mutual
 * authentication. The syntax of @b Authentication-Info header is defined in
 * @RFC2617 and @RFC3261 as follows:
 *
 * @code
 *   Authentication-Info  = "Authentication-Info" HCOLON ainfo
 *                           *(COMMA ainfo)
 *   ainfo                =  nextnonce / message-qop
 *                            / response-auth / cnonce
 *                            / nonce-count
 *   nextnonce            =  "nextnonce" EQUAL nonce-value
 *   response-auth        =  "rspauth" EQUAL response-digest
 *   response-digest      =  LDQUOT *LHEX RDQUOT
 * @endcode
 *
 * The parsed Authentication-Info header
 * is stored in #sip_authentication_info_t structure.
 */

/**@ingroup sip_authentication_info
 * @typedef typedef struct sip_authentication_info_s sip_authentication_info_t;
 *
 * The structure #sip_authentication_info_t contains representation of SIP
 * @AuthenticationInfo header.
 *
 * The #sip_authentication_info_t is defined as follows:
 * @code
 * typedef struct msg_auth_info_s
 * {
 *   msg_common_t       ai_common[1];  // Common fragment info
 *   msg_error_t       *ai_next;       // Dummy link to next header
 *   msg_param_t       *ai_items;      // List of ainfo
 * } sip_authentication_info_t;
 * @endcode
 */

#define sip_authentication_info_dup_xtra msg_list_dup_xtra
#define sip_authentication_info_dup_one msg_list_dup_one
#define sip_authentication_info_update NULL

msg_hclass_t sip_authentication_info_class[] =
  SIP_HEADER_CLASS(authentication_info, "Authentication-Info", "",
		   ai_params, append, authentication_info);

issize_t sip_authentication_info_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return msg_list_d(home, (msg_header_t *)h, s, slen);
}


issize_t sip_authentication_info_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  assert(sip_is_authentication_info(h));
  return msg_list_e(b, bsiz, h, f);
}


/* ====================================================================== */

/**@SIP_HEADER sip_proxy_authentication_info Proxy-Authentication-Info Header
 *
 * The @b Proxy-Authentication-Info header contains either a next-nonce used
 * by next request and/or authentication from proxy used in mutual
 * authentication. The syntax of @b Proxy-Authentication-Info header is defined
 * in @RFC2617 as follows:
 *
 * @code
 *   Proxy-Authentication-Info  = "Proxy-Authentication-Info" HCOLON ainfo
 *                           *(COMMA ainfo)
 *   ainfo                =  nextnonce / message-qop
 *                            / response-auth / cnonce
 *                            / nonce-count
 *   nextnonce            =  "nextnonce" EQUAL nonce-value
 *   response-auth        =  "rspauth" EQUAL response-digest
 *   response-digest      =  LDQUOT *LHEX RDQUOT
 * @endcode
 *
 * @note @b Proxy-Authentication-Info is not specified @RFC3261 and it is
 * mentioned by @RFC2617 but in passage.
 *
 * The parsed Proxy-Authentication-Info header
 * is stored in #sip_proxy_authentication_info_t structure.
 */

/**@ingroup sip_proxy_authentication_info
 * @typedef typedef struct msg_authentication_info_s sip_proxy_authentication_info_t;
 *
 * The structure #sip_proxy_authentication_info_t contains representation of SIP
 * @ProxyAuthenticationInfo header.
 *
 * The #sip_proxy_authentication_info_t is defined as follows:
 * @code
 * typedef struct msg_auth_info_s
 * {
 *   msg_common_t       ai_common[1];  // Common fragment info
 *   msg_error_t       *ai_next;       // Dummy link to next header
 *   msg_param_t       *ai_items;      // List of ainfo
 * } sip_proxy_authentication_info_t;
 * @endcode
 *
 */

#define sip_proxy_authentication_info_dup_xtra msg_list_dup_xtra
#define sip_proxy_authentication_info_dup_one msg_list_dup_one
#define sip_proxy_authentication_info_update NULL

msg_hclass_t sip_proxy_authentication_info_class[] =
  SIP_HEADER_CLASS(proxy_authentication_info, "Proxy-Authentication-Info", "",
		   ai_params, append, proxy_authentication_info);

issize_t sip_proxy_authentication_info_d(su_home_t *home, sip_header_t *h,
					 char *s, isize_t slen)
{
  return msg_list_d(home, (msg_header_t *)h, s, slen);
}

issize_t sip_proxy_authentication_info_e(char b[], isize_t bsiz,
					 sip_header_t const *h, int f)
{
  assert(sip_is_proxy_authentication_info(h)); /* This is soo popular */
  return msg_list_e(b, bsiz, h, f);
}

/* ====================================================================== */

/* Functions parsing @RFC3329 SIP Security Agreement headers */

typedef struct sip_security_agree_s sip_security_agree_t;
#define sh_security_agree sh_security_client

static
issize_t sip_security_agree_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_security_agree_t *sa = (sip_security_agree_t *)h;

  isize_t n;

  while (*s == ',')   /* Ignore empty entries (comma-whitespace) */
    *s = '\0', s += span_lws(s + 1) + 1;

  if ((n = span_token(s)) == 0)
    return -1;
  sa->sa_mec = s; s += n; while (IS_LWS(*s)) *s++ = '\0';
  if (*s == ';' && msg_params_d(home, &s, &sa->sa_params) < 0)
    return -1;

  return msg_parse_next_field(home, h, s, slen);
}

static
issize_t sip_security_agree_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  char *end = b + bsiz, *b0 = b;
  sip_security_agree_t const *sa = (sip_security_agree_t const *)h;

  MSG_STRING_E(b, end, sa->sa_mec);
  MSG_PARAMS_E(b, end, sa->sa_params, flags);

  return b - b0;
}

static
isize_t sip_security_agree_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_security_agree_t const *sa = h->sh_security_agree;

  MSG_PARAMS_SIZE(offset, sa->sa_params);
  offset += MSG_STRING_SIZE(sa->sa_mec);

  return offset;
}

/** Duplicate one sip_security_agree_t object */
static
char *sip_security_agree_dup_one(sip_header_t *dst, sip_header_t const *src,
				 char *b, isize_t xtra)
{
  sip_security_agree_t *sa_dst = dst->sh_security_agree;
  sip_security_agree_t const *sa_src = src->sh_security_agree;

  char *end = b + xtra;
  b = msg_params_dup(&sa_dst->sa_params, sa_src->sa_params, b, xtra);
  MSG_STRING_DUP(b, sa_dst->sa_mec, sa_src->sa_mec);
  assert(b <= end); (void)end;

  return b;
}

static int sip_security_agree_update(msg_common_t *h,
				     char const *name, isize_t namelen,
				     char const *value)
{
  sip_security_agree_t *sa = (sip_security_agree_t *)h;

  if (name == NULL) {
    sa->sa_q = NULL;
    sa->sa_d_alg = NULL;
    sa->sa_d_qop = NULL;
    sa->sa_d_ver = NULL;
  }
#define MATCH(s) (namelen == strlen(#s) && su_casenmatch(name, #s, strlen(#s)))

  else if (MATCH(q)) {
    sa->sa_q = value;
  }
  else if (MATCH(d-alg)) {
    sa->sa_d_alg = value;
  }
  else if (MATCH(d-qop)) {
    sa->sa_d_qop = value;
  }
  else if (MATCH(d-ver)) {
    sa->sa_d_ver = value;
  }

#undef MATCH

  return 0;
}


/**@SIP_HEADER sip_security_client Security-Client Header
 *
 * The Security-Client header is defined by @RFC3329, "Security Mechanism
 * Agreement for the Session Initiation Protocol (SIP)".
 *
 * @code
 *    security-client  = "Security-Client" HCOLON
 *                       sec-mechanism *(COMMA sec-mechanism)
 *    security-server  = "Security-Server" HCOLON
 *                       sec-mechanism *(COMMA sec-mechanism)
 *    security-verify  = "Security-Verify" HCOLON
 *                       sec-mechanism *(COMMA sec-mechanism)
 *    sec-mechanism    = mechanism-name *(SEMI mech-parameters)
 *    mechanism-name   = ( "digest" / "tls" / "ipsec-ike" /
 *                        "ipsec-man" / token )
 *    mech-parameters  = ( preference / digest-algorithm /
 *                         digest-qop / digest-verify / extension )
 *    preference       = "q" EQUAL qvalue
 *    qvalue           = ( "0" [ "." 0*3DIGIT ] )
 *                        / ( "1" [ "." 0*3("0") ] )
 *    digest-algorithm = "d-alg" EQUAL token
 *    digest-qop       = "d-qop" EQUAL token
 *    digest-verify    = "d-ver" EQUAL LDQUOT 32LHEX RDQUOT
 *    extension        = generic-param
 * @endcode
 *
 * @sa @SecurityServer, @SecurityVerify, sip_security_verify_compare(),
 * sip_security_client_select(), @RFC3329
 *
 * The parsed Security-Client header
 * is stored in #sip_security_client_t structure.
 */

/**@ingroup sip_security_client
 * @typedef typedef struct sip_security_client_s sip_security_client_t;
 *
 * The structure #sip_security_client_t contains representation of SIP
 * @SecurityClient header.
 *
 * The #sip_security_client_t is defined as follows:
 * @code
 * typedef struct sip_security_agree_s
 * {
 *   sip_common_t        sa_common[1]; // Common fragment info
 *   sip_security_client_t *sa_next;   // Link to next mechanism
 *   char const         *sa_mec;       // Security mechanism
 *   msg_param_t const  *sa_params;    // List of mechanism parameters
 *   char const         *sa_q;         // Value of q (preference) parameter
 *   char const         *sa_d_alg;     // Value of d-alg parameter
 *   char const         *sa_d_qop;     // Value of d-qop parameter
 *   char const         *sa_d_ver;     // Value of d-ver parameter
 * } sip_security_client_t;
 * @endcode
 */

msg_hclass_t sip_security_client_class[] =
SIP_HEADER_CLASS(security_client, "Security-Client", "",
		 sa_params, append, security_agree);

issize_t sip_security_client_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return sip_security_agree_d(home, h, s, slen);
}

issize_t sip_security_client_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  return sip_security_agree_e(b, bsiz, h, f);
}


/**@SIP_HEADER sip_security_server Security-Server Header
 *
 * The Security-Server header is defined by @RFC3329, "Security Mechanism
 * Agreement for the Session Initiation Protocol (SIP)".
 *
 * @sa @SecurityClient, @SecurityVerify, sip_security_verify_compare(),
 * sip_security_client_select(), @RFC3329.
 *
 * The parsed Security-Server header
 * is stored in #sip_security_server_t structure.
 */

/**@ingroup sip_security_server
 * @typedef typedef struct sip_security_server_s sip_security_server_t;
 *
 * The structure #sip_security_server_t contains representation of SIP
 * @SecurityServer header.
 *
 * The #sip_security_server_t is defined as follows:
 * @code
 * typedef struct sip_security_agree_s
 * {
 *   sip_common_t        sa_common[1]; // Common fragment info
 *   sip_security_server_t *sa_next;   // Link to next mechanism
 *   char const         *sa_mec;       // Security mechanism
 *   msg_param_t const  *sa_params;    // List of mechanism parameters
 *   char const         *sa_q;         // Value of q (preference) parameter
 *   char const         *sa_d_alg;     // Value of d-alg parameter
 *   char const         *sa_d_qop;     // Value of d-qop parameter
 *   char const         *sa_d_ver;     // Value of d-ver parameter
 * } sip_security_server_t;
 * @endcode
 */

msg_hclass_t sip_security_server_class[] =
SIP_HEADER_CLASS(security_server, "Security-Server", "",
		 sa_params, append, security_agree);

issize_t sip_security_server_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return sip_security_agree_d(home, h, s, slen);
}

issize_t sip_security_server_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  return sip_security_agree_e(b, bsiz, h, f);
}


/**@SIP_HEADER sip_security_verify Security-Verify Header
 *
 * The Security-Verify header is defined by @RFC3329, "Security Mechanism
 * Agreement for the Session Initiation Protocol (SIP)".
 *
 * @sa @SecurityClient, @SecurityServer, sip_security_verify_compare(),
 * sip_security_client_select(), @RFC3329.
 *
 * The parsed Security-Verify header
 * is stored in #sip_security_verify_t structure.
 */

/**@ingroup sip_security_verify
 * @typedef typedef struct sip_security_verify_s sip_security_verify_t;
 *
 * The structure #sip_security_verify_t contains representation of SIP
 * @SecurityVerify header.
 *
 * The #sip_security_verify_t is defined as follows:
 * @code
 * typedef struct sip_security_agree_s
 * {
 *   sip_common_t        sa_common[1]; // Common fragment info
 *   sip_security_verify_t *sa_next;   // Link to next mechanism
 *   char const         *sa_mec;       // Security mechanism
 *   msg_param_t const  *sa_params;    // List of mechanism parameters
 *   char const         *sa_q;         // Value of q (preference) parameter
 *   char const         *sa_d_alg;     // Value of d-alg parameter
 *   char const         *sa_d_qop;     // Value of d-qop parameter
 *   char const         *sa_d_ver;     // Value of d-ver parameter
 * } sip_security_verify_t;
 * @endcode
 */


msg_hclass_t sip_security_verify_class[] =
SIP_HEADER_CLASS(security_verify, "Security-Verify", "",
		 sa_params, append, security_agree);

issize_t sip_security_verify_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  return sip_security_agree_d(home, h, s, slen);
}

issize_t sip_security_verify_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  return sip_security_agree_e(b, bsiz, h, f);
}

/* ====================================================================== */
/* RFC 3323 */

/**@SIP_HEADER sip_privacy Privacy Header
 *
 * The Privacy header is used by User-Agent to request privacy services from
 * the network. Its syntax is defined in @RFC3323 as follows:
 *
 * @code
 *    Privacy-hdr  =  "Privacy" HCOLON priv-value *(";" priv-value)
 *    priv-value   =   "header" / "session" / "user" / "none" / "critical"
 *                     / token
 * @endcode
 *
 * The parsed Privacy header is stored in #sip_privacy_t structure.
 */

/**@ingroup sip_privacy
 * @typedef typedef struct sip_privacy_s sip_privacy_t;
 *
 * The structure #sip_privacy_t contains representation of a SIP @Privacy
 * header.
 *
 * The #sip_privacy_t is defined as follows:
 * @code
 * typedef struct sip_privacy_s {
 *   sip_common_t       priv_common[1];	// Common fragment info
 *   sip_error_t       *priv_next;     	// Dummy link
 *   msg_param_t const *priv_values;   	// List of privacy values
 * } sip_privacy_t;
 * @endcode
 */

msg_xtra_f sip_privacy_dup_xtra;
msg_dup_f sip_privacy_dup_one;

#define sip_privacy_update NULL

msg_hclass_t sip_privacy_class[] =
SIP_HEADER_CLASS(privacy, "Privacy", "", priv_values, single, privacy);

static
issize_t sip_privacy_token_scan(char *start)
{
  char *s = start;
  skip_token(&s);

  if (s == start)
    return -1;

  if (IS_LWS(*s))
    *s++ = '\0';
  skip_lws(&s);

  return s - start;
}

issize_t sip_privacy_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  sip_privacy_t *priv = (sip_privacy_t *)h;

  while (*s == ';' || *s == ',') {
    s++;
    skip_lws(&s);
  }

  for (;;) {
    if (msg_any_list_d(home, &s, (msg_param_t **)&priv->priv_values,
		       sip_privacy_token_scan, ';') < 0)
      return -1;

    if (*s == '\0')
      return 0;			/* Success! */

    if (*s == ',')
      *s++ = '\0';		/* We accept comma-separated list, too */
    else if (IS_TOKEN(*s))
      ;				/* LWS separated list...  */
    else
      return -1;
  }
}

issize_t sip_privacy_e(char b[], isize_t bsiz, sip_header_t const *h, int f)
{
  sip_privacy_t const *priv = h->sh_privacy;
  char *b0 = b, *end = b + bsiz;
  size_t i;

  if (priv->priv_values) {
    for (i = 0; priv->priv_values[i]; i++) {
      if (i > 0) MSG_CHAR_E(b, end, ';');
      MSG_STRING_E(b, end, priv->priv_values[i]);
    }
  }

  MSG_TERM_E(b, end);

  return b - b0;
}

isize_t sip_privacy_dup_xtra(sip_header_t const *h, isize_t offset)
{
  sip_privacy_t const *priv = h->sh_privacy;

  MSG_PARAMS_SIZE(offset, priv->priv_values);

  return offset;
}

char *sip_privacy_dup_one(sip_header_t *dst,
			  sip_header_t const *src,
			  char *b,
			  isize_t xtra)
{
  sip_privacy_t *priv = dst->sh_privacy;
  sip_privacy_t const *o = src->sh_privacy;
  char *end = b + xtra;

  b = msg_params_dup(&priv->priv_values, o->priv_values, b, xtra);

  assert(b <= end); (void)end;

  return b;
}

