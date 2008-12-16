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

/**@CFILE auth_digest.c
 *
 * Implementation for digest authentication.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Feb 22 12:10:37 2001 ppessi
 */

#include "config.h"

#include <sofia-sip/su_md5.h>
#include "sofia-sip/auth_common.h"
#include "sofia-sip/auth_digest.h"
#include "sofia-sip/msg_header.h"
#include "sofia-sip/auth_common.h"

#include "iptsec_debug.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <assert.h>

/**Get digest-challenge parameters.
 *
 * The function digest_challenge_get() searches for the digest authentication
 * parameters in @a params.  The parameters are assigned to the appropriate
 * fields in @a ac structure.
 *
 * @return
 *
 * The function digest_challenge_get() returns number of parameters
 * found, or -1 upon an error.
 */
issize_t auth_digest_challenge_get(su_home_t *home,
				   auth_challenge_t *ac0,
				   char const * const params[])
{
  ssize_t n;
  auth_challenge_t ac[1] = {{ 0 }};
  char const *md5 = NULL, *md5sess = NULL, *sha1 = NULL,
    *stale = NULL,
    *qop_auth = NULL, *qop_auth_int = NULL;

  ac->ac_size = sizeof(ac);

  assert(ac0);
  assert(ac0->ac_size >= (int) sizeof(*ac));

  if (ac0 == NULL || params == NULL)
    return -1;

  n = auth_get_params(home, params,
		      "realm=", &ac->ac_realm,
		      "domain=", &ac->ac_domain,
		      "nonce=", &ac->ac_nonce,
		      "opaque=", &ac->ac_opaque,
		      "algorithm=", &ac->ac_algorithm,
		      "qop=", &ac->ac_qop,
		      "algorithm=md5", &md5,
		      "algorithm=md5-sess", &md5sess,
		      "algorithm=sha1", &sha1,
		      "stale=true", &stale,
		      "qop=auth", &qop_auth,
		      "qop=auth-int", &qop_auth_int,
		      NULL);
  if (n < 0)
    return n;

  ac->ac_stale = stale != NULL;
  ac->ac_md5 = md5 != NULL || ac->ac_algorithm == NULL;
  ac->ac_md5sess = md5sess != NULL;
  ac->ac_sha1 = sha1 != NULL;
  ac->ac_auth = qop_auth != NULL;
  ac->ac_auth_int = qop_auth_int != NULL;

  auth_struct_copy(ac0, ac, sizeof(ac));

  SU_DEBUG_5(("%s(): got "MOD_ZD"\n", "auth_digest_challenge_get", n));

  return n;
}

/** Free challenge parameters */
void auth_digest_challenge_free_params(su_home_t *home, auth_challenge_t *ac)
{
  if (ac->ac_realm)
    su_free(home, (void *)ac->ac_realm), ac->ac_realm = NULL;
  if (ac->ac_domain)
    su_free(home, (void *)ac->ac_domain), ac->ac_domain = NULL;
  if (ac->ac_nonce)
    su_free(home, (void *)ac->ac_nonce), ac->ac_nonce = NULL;
  if (ac->ac_opaque)
    su_free(home, (void *)ac->ac_opaque), ac->ac_opaque = NULL;
  if (ac->ac_algorithm)
    su_free(home, (void *)ac->ac_algorithm), ac->ac_algorithm = NULL;
  if (ac->ac_qop)
    su_free(home, (void *)ac->ac_qop), ac->ac_qop = NULL;
}

/**Get digest-response parameters.
 *
 * The function auth_response_get() searches for the digest authentication
 * parameters in @a params.  The parameters are assigned to the appropriate
 * fields in @a ar structure.
 *
 * @return
 *
 * The function auth_response_get() returns number of parameters
 * found, or -1 upon an error.
 */
issize_t auth_digest_response_get(su_home_t *home,
				  auth_response_t *ar0,
				  char const *const params[])
{
  ssize_t n;
  auth_response_t ar[1] = {{ 0 }};
  char const *md5 = NULL, *md5sess = NULL, *sha1 = NULL,
    *qop_auth = NULL, *qop_auth_int = NULL;

  ar->ar_size = sizeof(ar);

  assert(ar0); assert(params); assert(ar0->ar_size >= (int) sizeof(ar));

  if (ar0 == NULL || params == NULL)
    return -1;

  n = auth_get_params(home, params,
		      "username=", &ar->ar_username,
		      "realm=", &ar->ar_realm,
		      "nonce=", &ar->ar_nonce,
		      "uri=", &ar->ar_uri,
		      "response=", &ar->ar_response,
		      "algorithm=", &ar->ar_algorithm,
		      "opaque=", &ar->ar_opaque,
		      "cnonce=", &ar->ar_cnonce,
		      "qop=", &ar->ar_qop,
		      "nc=", &ar->ar_nc,
		      "algorithm=md5", &md5,
		      "algorithm=md5-sess", &md5sess,
		      "algorithm=sha1", &sha1,
		      "qop=auth", &qop_auth,
		      "qop=auth-int", &qop_auth_int,
		      NULL);
  if (n < 0)
    return n;

  ar->ar_md5      = md5 != NULL || ar->ar_algorithm == NULL;
  ar->ar_md5sess  = md5sess != NULL;
  ar->ar_sha1     = sha1 != NULL;
  ar->ar_auth     = qop_auth != NULL;
  ar->ar_auth_int = qop_auth_int != NULL;

  auth_struct_copy(ar0, ar, sizeof(ar));

  SU_DEBUG_7(("%s: "MOD_ZD"\n", "auth_digest_response_get", n));

  return n;
}

static void unquote_update(su_md5_t md5[1], char const *quoted)
{
  if (!quoted)
    /*xyzzy*/;
  else if (quoted[0] == '"') {
    char const *q;
    size_t n;

    for (q = quoted + 1; *q; q += n + 2) {
      n = strcspn(q, "\"\\");
      su_md5_update(md5, q, n);
      if (q[n] == '"' || q[n] == '\0')
	break;
      su_md5_update(md5, q + n + 1, 1);
    }
  }
  else
    su_md5_strupdate(md5, quoted);
}

/** Generate A1 hash for digest authentication.
 */
int auth_digest_a1(auth_response_t *ar,
		   auth_hexmd5_t ha1,
		   char const *secret)
{
  su_md5_t md5[1];

  /* Calculate A1 */
  su_md5_init(md5);
  su_md5_strupdate(md5, ar->ar_username);
  su_md5_update(md5, ":", 1);
  unquote_update(md5, ar->ar_realm);
  su_md5_update(md5, ":", 1);
  su_md5_strupdate(md5, secret);

  su_md5_hexdigest(md5, ha1);

  SU_DEBUG_5(("auth_digest_a1() has A1 = MD5(%s:%s:%s) = %s\n",
	      ar->ar_username, ar->ar_realm, secret, ha1));

  return 0;
}

int auth_digest_a1sess(auth_response_t *ar,
		       auth_hexmd5_t ha1sess,
		       char const *ha1)
{
  su_md5_t md5[1];

  su_md5_init(md5);
  su_md5_strupdate(md5, ha1);
  su_md5_update(md5, ":", 1);
  unquote_update(md5, ar->ar_nonce);
  su_md5_update(md5, ":", 1);
  unquote_update(md5, ar->ar_cnonce);

  su_md5_hexdigest(md5, ha1sess);

  SU_DEBUG_5(("auth_sessionkey has A1' = MD5(%s:%s:%s) = %s\n",
	      ha1, ar->ar_nonce, ar->ar_cnonce, ha1sess));

  return 0;
}

/** Generate MD5 session key for digest authentication.
 */
int auth_digest_sessionkey(auth_response_t *ar,
			   auth_hexmd5_t ha1,
			   char const *secret)
{
  if (ar->ar_md5sess)
    ar->ar_algorithm = "MD5-sess";
  else if (ar->ar_md5)
    ar->ar_algorithm = "MD5";
  else
    return -1;

  if (ar->ar_md5sess) {
    auth_hexmd5_t base_ha1;
    auth_digest_a1(ar, base_ha1, secret);
    auth_digest_a1sess(ar, ha1, base_ha1);
  } else {
    auth_digest_a1(ar, ha1, secret);
  }

  return 0;
}

/** Generate response for digest authentication.
 *
 */
int auth_digest_response(auth_response_t *ar,
			 auth_hexmd5_t response,
			 auth_hexmd5_t const ha1,
			 char const *method_name,
			 void const *data, isize_t dlen)
{
  su_md5_t md5[1];
  auth_hexmd5_t Hentity, HA2;

  if (ar->ar_auth_int)
    ar->ar_qop = "auth-int";
  else if (ar->ar_auth)
    ar->ar_qop = "auth";
  else
    ar->ar_qop = NULL;

  /* Calculate Hentity */
  if (ar->ar_auth_int) {
    if (data && dlen) {
      su_md5_init(md5);
      su_md5_update(md5, data, dlen);
      su_md5_hexdigest(md5, Hentity);
    } else {
      strcpy(Hentity, "d41d8cd98f00b204e9800998ecf8427e");
    }
  }

  /* Calculate A2 */
  su_md5_init(md5);
  su_md5_strupdate(md5, method_name);
  su_md5_update(md5, ":", 1);
  unquote_update(md5, ar->ar_uri);
  if (ar->ar_auth_int) {
    su_md5_update(md5, ":", 1);
    su_md5_update(md5, Hentity, sizeof(Hentity) - 1);
  }
  su_md5_hexdigest(md5, HA2);

  SU_DEBUG_5(("A2 = MD5(%s:%s%s%s)\n", method_name, ar->ar_uri,
	      ar->ar_auth_int ? ":" : "", ar->ar_auth_int ? Hentity : ""));

  /* Calculate response */
  su_md5_init(md5);
  su_md5_update(md5, ha1, 32);
  su_md5_update(md5, ":", 1);
  unquote_update(md5, ar->ar_nonce);

  if (ar->ar_auth || ar->ar_auth_int) {
    su_md5_update(md5, ":", 1);
    su_md5_strupdate(md5, ar->ar_nc);
    su_md5_update(md5, ":", 1);
    unquote_update(md5, ar->ar_cnonce);
    su_md5_update(md5, ":", 1);
    su_md5_strupdate(md5, ar->ar_qop);
  }

  su_md5_update(md5, ":", 1);
  su_md5_update(md5, HA2, 32);
  su_md5_hexdigest(md5, response);

  SU_DEBUG_5(("auth_response: %s = MD5(%s:%s%s%s%s%s%s%s:%s) (qop=%s)\n",
	      response, ha1, ar->ar_nonce,
	      ar->ar_auth ||  ar->ar_auth_int ? ":" : "",
	      ar->ar_auth ||  ar->ar_auth_int ? ar->ar_nc : "",
	      ar->ar_auth ||  ar->ar_auth_int ? ":" : "",
	      ar->ar_auth ||  ar->ar_auth_int ? ar->ar_cnonce : "",
	      ar->ar_auth ||  ar->ar_auth_int ? ":" : "",
	      ar->ar_auth ||  ar->ar_auth_int ? ar->ar_qop : "",
	      HA2,
	      ar->ar_qop ? ar->ar_qop : "NONE"));

  return 0;
}
