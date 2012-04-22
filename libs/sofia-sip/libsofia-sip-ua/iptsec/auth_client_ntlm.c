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

/**@CFILE auth_client_ntlm.c  NTLM authenticator for SIP client
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed May 24 21:44:39 EEST 2006
 */

#include "config.h"

#include <sofia-sip/su.h>
#include <sofia-sip/su_md5.h>

#include "sofia-sip/auth_ntlm.h"
#include "sofia-sip/auth_client.h"
#include "sofia-sip/auth_client_plugin.h"

#include <sofia-sip/msg_header.h>

#include <sofia-sip/auth_digest.h>

#include <sofia-sip/base64.h>
#include <sofia-sip/su_uniqueid.h>
#include <sofia-sip/su_string.h>

#include <sofia-sip/su_debug.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

typedef struct auth_ntlm_client_s
{
  auth_client_t ntlm_client;

  int           ntlm_ncount;
  char const   *ntlm_cnonce;
  auth_challenge_t ntlm_ac[1];
} auth_ntlm_client_t;

static int auc_ntlm_challenge(auth_client_t *ca,
				msg_auth_t const *ch);
static int auc_ntlm_authorization(auth_client_t *ca,
				    su_home_t *h,
				    char const *method,
				    url_t const *url,
				    msg_payload_t const *body,
				    msg_header_t **);

auth_client_plugin_t const _ntlm_client_plugin =
{
  sizeof ca_ntlm_plugin,
  sizeof (auth_ntlm_client_t),
  "NTLM",
  auc_ntlm_challenge,
  auc_ntlm_authorization
};

auth_client_plugin_t const * const ntlm_client_plugin = _ntlm_client_plugin;

/** Store a ntlm authorization challenge.
 */
static int auc_ntlm_challenge(auth_client_t *ca, msg_auth_t const *ch)
{
  su_home_t *home = ca->ca_home;
  auth_ntlm_client_t *ntlm = (auth_ntlm_client_t *)ca;
  auth_challenge_t ac[1] = {{ sizeof ac }};
  int stale;

  if (auth_ntlm_challenge_get(home, ac, ch->au_params) < 0)
    goto error;

  /* Check that we can handle the challenge */
  if (!ac->ac_md5 && !ac->ac_md5sess)
    goto error;
  if (ac->ac_qop && !ac->ac_auth && !ac->ac_auth_int)
    goto error;

  stale = ac->ac_stale || str0cmp(ac->ac_nonce, ntlm->ntlm_ac->ac_nonce);

  if (ac->ac_qop && (ntlm->ntlm_cnonce == NULL || ac->ac_stale)) {
    su_guid_t guid[1];
    char *cnonce;
    if (ntlm->ntlm_cnonce != NULL)
      /* Free the old one if we are updating after stale=true */
      su_free(home, (void *)ntlm->ntlm_cnonce);
    su_guid_generate(guid);
    ntlm->ntlm_cnonce = cnonce = su_alloc(home, BASE64_SIZE(sizeof(guid)) + 1);
    base64_e(cnonce, BASE64_SIZE(sizeof(guid)) + 1, guid, sizeof(guid));
    ntlm->ntlm_ncount = 0;
  }

  auth_ntlm_challenge_free_params(home, ntlm->ntlm_ac);

  *ntlm->ntlm_ac = *ac;

  return stale ? 2 : 1;

 error:
  auth_ntlm_challenge_free_params(home, ac);
  return -1;
}


/**Create a NTLM authorization header.
 *
 * Creates a ntlm authorization header from username @a user and password
 * @a pass, client nonce @a cnonce, client nonce count @a nc, request method
 * @a method, request URI @a uri and message body @a data. The authorization
 * header type is determined by @a hc - it can be either
 * sip_authorization_class or sip_proxy_authorization_class, as well as
 * http_authorization_class or http_proxy_authorization_class.
 *
 * @param home 	  memory home used to allocate memory for the new header
 * @param hc   	  header class for the header to be created
 * @param user 	  user name
 * @param pass 	  password
 * @param ac      challenge structure
 * @param cnonce  client nonce
 * @param nc      client nonce count
 * @param method  request method
 * @param uri     request uri
 * @param data    message body
 * @param dlen    length of message body
 *
 * @return
 * Returns a pointer to newly created authorization header, or NULL upon an
 * error.
 */
int auc_ntlm_authorization(auth_client_t *ca,
			     su_home_t *home,
			     char const *method,
			     url_t const *url,
			     msg_payload_t const *body,
			     msg_header_t **return_headers)
{
  auth_ntlm_client_t *ntlm = (auth_ntlm_client_t *)ca;
  msg_hclass_t *hc = ca->ca_credential_class;
  char const *user = ca->ca_user;
  char const *pass = ca->ca_pass;
  auth_challenge_t const *ac = ntlm->ntlm_ac;
  char const *cnonce = ntlm->ntlm_cnonce;
  unsigned nc = ++ntlm->ntlm_ncount;
  char *uri = url_as_string(home, url);
  void const *data = body ? body->pl_data : "";
  int dlen = body ? body->pl_len : 0;

  msg_header_t *h;
  auth_hexmd5_t sessionkey, response;
  auth_response_t ar[1] = {{ 0 }};
  char ncount[17];

  ar->ar_size = sizeof(ar);
  ar->ar_username = user;
  ar->ar_realm = ac->ac_realm;
  ar->ar_nonce = ac->ac_nonce;
  ar->ar_algorithm = NULL;
  ar->ar_md5 = ac->ac_md5;
  ar->ar_md5sess = ac->ac_md5sess;
  ar->ar_opaque = ac->ac_opaque;
  ar->ar_qop = NULL;
  ar->ar_auth = ac->ac_auth;
  ar->ar_auth_int = ac->ac_auth_int;
  ar->ar_uri = uri;

  /* If there is no qop, we MUST NOT include cnonce or nc */
  if (!ar->ar_auth && !ar->ar_auth_int)
    cnonce = NULL;

  if (cnonce) {
    snprintf(ncount, sizeof(ncount), "%08x", nc);
    ar->ar_cnonce = cnonce;
    ar->ar_nc = ncount;
  }

  auth_ntlm_sessionkey(ar, sessionkey, pass);
  auth_ntlm_response(ar, response, sessionkey, method, data, dlen);

  h = msg_header_format(home, hc,
			"NTLM "
			"username=\"%s\", "
			"realm=\"%s\", "
			"nonce=\"%s"
			"%s%s"
			"%s%s"
			"%s%s, "
			"uri=\"%s\", "
			"response=\"%s\""
			"%s%s"
			"%s%s",
			ar->ar_username,
			ar->ar_realm,
			ar->ar_nonce,
			cnonce ? "\",  cnonce=\"" : "",
			cnonce ? cnonce : "",
			ar->ar_opaque ? "\",  opaque=\"" : "",
			ar->ar_opaque ? ar->ar_opaque : "",
			ar->ar_algorithm ? "\", algorithm=" : "",
			ar->ar_algorithm ? ar->ar_algorithm : "",
			ar->ar_uri,
			response,
			ar->ar_auth || ar->ar_auth_int ? ", qop=" : "",
			ar->ar_auth_int ? "auth-int" :
			(ar->ar_auth ? "auth" : ""),
			cnonce ? ", nc=" : "",
			cnonce ? ncount : "");

  su_free(home, uri);

  if (!h)
    return -1;
  *return_headers = h;
  return 0;
}
