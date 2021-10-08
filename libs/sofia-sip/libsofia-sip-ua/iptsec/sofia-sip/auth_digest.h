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

#ifndef AUTH_DIGEST_H
/** Defined when <sofia-sip/auth_digest.h> has been included. */
#define AUTH_DIGEST_H

/**@file sofia-sip/auth_digest.h
 * Datatypes and functions for Digest authentication.
 *
 * The structures and functions here follow the RFC 2617.
 *
 * @sa @RFC2617,
 * <i>"HTTP Authentication: Basic and Digest Access Authentication"</i>,
 * J. Franks et al,
 * June 1999.
 *
 * @sa @RFC3261 section 22
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Feb 22 12:25:55 2001 ppessi
 */

#ifndef SU_ALLOC_H
#include <sofia-sip/su_alloc.h>
#endif

SOFIA_BEGIN_DECLS

/** Parameters for digest-challenge.
 *
 * The digest-challenge is sent by server or proxy to client. It can be
 * included in, e.g, WWW-Authenticate or Proxy-Authenticate headers.
 *
 * @code
 *   challenge        =  "Digest" digest-challenge
 *   digest-challenge  = 1#( realm | [domain] | nonce |
 *                           [opaque] | [stale] | [algorithm] |
 *                           [qop-options] | [auth-param] )
 *   domain            = "domain" "=" <"> URI ( 1*SP URI ) <">
 *   URI               = absoluteURI | abs_path
 *   nonce             = "nonce" "=" nonce-value
 *   nonce-value       = quoted-string
 *   opaque            = "opaque" "=" quoted-string
 *   stale             = "stale" "=" ( "true" | "false" )
 *   algorithm         = "algorithm" "=" ( "MD5" | "MD5-sess" | token )
 *   qop-options       = "qop" "=" <"> 1#qop-value <">
 *   qop-value         = "auth" | "auth-int" | token
 * @endcode
 *
 * @sa @RFC2617
 */
typedef struct {
  int         ac_size;
  char const *ac_realm;		/**< realm */
  char const *ac_domain;	/**< domain */
  char const *ac_nonce;		/**< nonce */
  char const *ac_opaque;	/**< opaque */
  char const *ac_algorithm;	/**< algorithm */
  char const *ac_qop;		/**< qop */
  unsigned    ac_stale : 1;	/**< stale=true */
  unsigned    ac_md5 : 1;	/**< algorithm=MS5 (or missing) */
  unsigned    ac_md5sess : 1;	/**< algorithm=MD5-sess */
  unsigned    ac_sha1 : 1;	/**< algorithm=sha1 (SSA Hash) */
  unsigned    ac_auth : 1;	/**< qop=auth */
  unsigned    ac_auth_int : 1;	/**< qop=auth-int */
  unsigned : 0;
} auth_challenge_t;

/** Digest parameters for digest-response in Authorize.
 *
 * The digest-response is sent by the client to a server or a proxy. It can
 * be included in, e.g., Authorization or Proxy-Authorization headers.
 *
 * @code
 *   credentials      = "Digest" digest-response
 *   digest-response  = 1#( username | realm | nonce | digest-uri |
 *                          response | [ algorithm ] | [cnonce] | [opaque] |
 *                          [message-qop] | [nonce-count] | [auth-param] )
 *   username         = "username" "=" username-value
 *   username-value   = quoted-string
 *   digest-uri       = "uri" "=" digest-uri-value
 *   digest-uri-value = request-uri   ; As specified by HTTP/1.1
 *   message-qop      = "qop" "=" qop-value
 *   cnonce           = "cnonce" "=" cnonce-value
 *   cnonce-value     = nonce-value
 *   nonce-count      = "nc" "=" nc-value
 *   nc-value         = 8LHEX
 *   response         = "response" "=" request-digest
 *   request-digest   = <"> 32LHEX <">
 *   LHEX             =  "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" |
 *                       "8" | "9" | "a" | "b" | "c" | "d" | "e" | "f"
 * @endcode
 */
typedef struct {
  int         ar_size;
  char const *ar_username;
  char const *ar_realm;		/**< realm */
  char const *ar_nonce;		/**< nonce */
  char const *ar_uri;		/**< uri */
  char const *ar_response;	/**< response */
  char const *ar_algorithm;	/**< algorithm */
  char const *ar_cnonce;	/**< cnonce */
  char const *ar_opaque;	/**< opaque */
  char const *ar_qop;		/**< qop */
  char const *ar_nc;		/**< nonce count */
  unsigned    ar_md5 : 1;	/**< MS5 algorithm */
  unsigned    ar_md5sess : 1;	/**< MD5-sess algorithm */
  unsigned    ar_sha1 : 1;	/**< SHA1 algorithm */
  unsigned    ar_auth : 1;	/**< qop=auth */
  unsigned    ar_auth_int : 1;	/**< qop=auth-int */
  unsigned : 0;
} auth_response_t;

typedef char auth_hexmd5_t[33];

SOFIAPUBFUN issize_t auth_digest_challenge_get(su_home_t *, auth_challenge_t *,
					       char const * const params[]);
SOFIAPUBFUN void auth_digest_challenge_free_params(su_home_t *home,
						   auth_challenge_t *ac);
SOFIAPUBFUN issize_t auth_digest_response_get(su_home_t *, auth_response_t *,
					      char const * const params[]);

SOFIAPUBFUN int auth_digest_a1(auth_response_t *ar,
			       auth_hexmd5_t ha1,
			       char const *secret);

SOFIAPUBFUN int auth_digest_a1sess(auth_response_t *ar,
				   auth_hexmd5_t ha1sess,
				   char const *ha1);

SOFIAPUBFUN int auth_digest_sessionkey(auth_response_t *, auth_hexmd5_t ha1,
				       char const *secret);
SOFIAPUBFUN int auth_digest_response(auth_response_t *, auth_hexmd5_t response,
				     auth_hexmd5_t const ha1,
				     char const *method_name,
				     void const *data, isize_t dlen);

SOFIAPUBFUN int auth_struct_copy(void *dst, void const *src, isize_t s_size);

SOFIAPUBFUN int auth_strcmp(char const *quoted, char const *unquoted);

SOFIA_END_DECLS

#endif
