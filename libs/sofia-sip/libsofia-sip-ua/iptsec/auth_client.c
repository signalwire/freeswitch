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

/**@CFILE auth_client.c  Authenticators for SIP client
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Feb 14 18:32:58 2001 ppessi
 */

#include "config.h"

#define SOFIA_EXTEND_AUTH_CLIENT 1

#include <sofia-sip/su.h>
#include <sofia-sip/su_md5.h>

#include "sofia-sip/auth_common.h"
#include "sofia-sip/auth_client.h"
#include "sofia-sip/auth_client_plugin.h"

#include <sofia-sip/msg_types.h>
#include <sofia-sip/msg_header.h>

#include <sofia-sip/auth_digest.h>

#include <sofia-sip/base64.h>
#include <sofia-sip/bnf.h>
#include <sofia-sip/su_uniqueid.h>
#include <sofia-sip/su_string.h>

#include <sofia-sip/su_debug.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

static auth_client_t *ca_create(su_home_t *home,
				char const *scheme,
				char const *realm);

static void ca_destroy(su_home_t *home, auth_client_t *ca);

static int ca_challenge(auth_client_t *ca,
			msg_auth_t const *auth,
			msg_hclass_t *credential_class,
			char const *scheme,
			char const *realm);

static int ca_info(auth_client_t *ca,
		   msg_auth_info_t const *ai,
		   msg_hclass_t *credential_class);

static int ca_credentials(auth_client_t *ca,
			  char const *scheme,
			  char const *realm,
			  char const *user,
			  char const *pass);

static int ca_clear_credentials(auth_client_t *ca);

static int ca_has_authorization(auth_client_t const *ca);


/** Initialize authenticators.
 *
 * The function auc_challenge() merges the challenge @a ch to the list of
 * authenticators @a auc_list.
 *
 * @param[in,out] auc_list  list of authenticators to be updated
 * @param[in,out] home      memory home used for allocating authenticators
 * @param[in] ch        challenge to be processed
 * @param[in] crcl      credential class
 *
 * @retval 1 when at least one challenge was updated
 * @retval 0 when there was no new challenges
 * @retval -1 upon an error
 */
int auc_challenge(auth_client_t **auc_list,
		  su_home_t *home,
		  msg_auth_t const *ch,
		  msg_hclass_t *crcl)
{
  auth_client_t **cca;
  int retval = 0;

  /* Go through each challenge in Authenticate or Proxy-Authenticate headers */
  for (; ch; ch = ch->au_next) {
    char const *scheme = ch->au_scheme;
    char const *realm = msg_header_find_param(ch->au_common, "realm=");
    int matched = 0, updated;

    if (!scheme || !realm)
      continue;

    /* Update matching authenticator */
    for (cca = auc_list; (*cca); cca = &(*cca)->ca_next) {
      updated = ca_challenge((*cca), ch, crcl, scheme, realm);
      if (updated < 0)
	return -1;
      if (updated == 0)
	continue;		/* No match, next */
      matched = 1;
      if (updated > 1)
	retval = 1;		/* Updated authenticator */
    }

    if (!matched) {
      /* There was no matching authenticator, create a new one */
      *cca = ca_create(home, scheme, realm);

      if (*cca == NULL) {
	return -1;
      }
      else if (ca_challenge((*cca), ch, crcl, scheme, realm) < 0) {
	ca_destroy(home, *cca), *cca = NULL;
	return -1;
      }
      /* XXX - case w/ unknown authentication scheme */
      else
	retval = 1;		/* Updated authenticator */
    }
  }

  return retval;
}

/** Update authentication client.
 *
 * @retval -1 upon an error
 * @retval 0 when challenge did not match
 * @retval 1 when challenge did match but was not updated
 * @retval 2 when challenge did match and updated client
 */
static
int ca_challenge(auth_client_t *ca,
		 msg_auth_t const *ch,
		 msg_hclass_t *credential_class,
		 char const *scheme,
		 char const *realm)
{
  int stale = 0;

  assert(ca); assert(ch);

  if (!ca || !ch)
    return -1;

  if (!su_casematch(ca->ca_scheme, scheme))
    return 0;
  if (!su_strmatch(ca->ca_realm, realm))
    return 0;

  if (ca->ca_credential_class &&
      ca->ca_credential_class != credential_class)
    return 0;

  if (!ca->ca_auc) {
    ca->ca_credential_class = credential_class;
    return 1;
  }

  if (ca->ca_auc->auc_challenge)
    stale = ca->ca_auc->auc_challenge(ca, ch);
  if (stale < 0)
    return -1;

  if (!ca->ca_credential_class)
    stale = 2, ca->ca_credential_class = credential_class;

  return stale > 1 ? 2 : 1;
}

/** Store authentication info to authenticators.
 *
 * The function auc_info() feeds the authentication data from the @b
 * Authentication-Info header @a info to the list of authenticators @a
 * auc_list.
 *
 * @param[in,out] auc_list  list of authenticators to be updated
 * @param[in] info        info header to be processed
 * @param[in] credential_class      corresponding credential class
 *
 * The authentication info can be in either @AuthenticationInfo or in
 * @ProxyAuthenticationInfo headers. If the header is @AuthenticationInfo,
 * the @a credential_class should be #sip_authorization_class or
 * #http_authorization_class. Likewise, If the header is
 * @ProxyAuthenticationInfo, the @a credential_class should be
 * #sip_proxy_authorization_class or #http_proxy_authorization_class.

 * The authentication into header usually contains next nonce or mutual
 * authentication information. Currently, only the nextnonce parameter is
 * processed.
 *
 * @bug
 * In principle, SIP allows more than one challenge for a single request.
 * For example, there can be multiple proxies that each challenge the
 * client. The result of storing authentication info can be quite unexpected
 * if there are more than one authenticator with the given type (specified
 * by @a credential_class).
 *
 * @retval number of challenges to updated
 * @retval 0 when there was no challenge to update
 * @retval -1 upon an error
 *
 * @NEW_1_12_5.
 */
int auc_info(auth_client_t **auc_list,
	     msg_auth_info_t const *info,
	     msg_hclass_t *credential_class)
{
  auth_client_t *ca;
  int retval = 0;

  /* Go through each challenge in Authenticate or Proxy-Authenticate headers */

  /* Update matching authenticator */
  for (ca = *auc_list; ca; ca = ca->ca_next) {
    int updated = ca_info(ca, info, credential_class);
    if (updated < 0)
      return -1;
    if (updated >= 1)
      retval = 1;		/* Updated authenticator */
  }

  return retval;
}

/** Update authentication client with authentication info.
 *
 * @retval -1 upon an error
 * @retval 0 when challenge did not match
 * @retval 1 when challenge did match but was not updated
 * @retval 2 when challenge did match and updated client
 */
static
int ca_info(auth_client_t *ca,
	    msg_auth_info_t const *info,
	    msg_hclass_t *credential_class)
{
  assert(ca); assert(info);

  if (!ca || !info)
    return -1;

  if (!ca->ca_credential_class)
    return 0;

  if (ca->ca_credential_class != credential_class)
    return 0;

  if (!ca->ca_auc
      || (size_t)ca->ca_auc->auc_plugin_size <=
         offsetof(auth_client_plugin_t, auc_info)
      || !ca->ca_auc->auc_info)
    return 0;

  return ca->ca_auc->auc_info(ca, info);
}


/**Feed authentication data to the authenticator.
 *
 * The function auc_credentials() is used to provide the authenticators in
 * with authentication data (user name, secret).  The authentication data
 * has format as follows:
 *
 * scheme:"realm":user:pass
 *
 * For instance, @c Basic:"nokia-proxy":ppessi:verysecret
 *
 * @todo The authentication data format sucks.
 *
 * @param[in,out] auc_list  list of authenticators
 * @param[in,out] home      memory home used for allocations
 * @param[in]     data      colon-separated authentication data
 *
 * @retval >0 when successful
 * @retval 0 if not authenticator matched with @a data
 * @retval -1 upon an error
 */
int auc_credentials(auth_client_t **auc_list, su_home_t *home,
		    char const *data)
{
  int retval = 0, match;
  char *s0, *s;
  char *scheme = NULL, *user = NULL, *pass = NULL, *realm = NULL;

  s0 = s = su_strdup(NULL, data);

  /* Parse authentication data */
  /* Data is string like "Basic:\"agni\":user1:secret"
     or "Basic:\"[fe80::204:23ff:fea7:d60a]\":user1:secret" (IPv6)
     or "Basic:\"Use \\\"interesting\\\" username and password here:\":user1:secret"
  */
  if (s && (s = strchr(scheme = s, ':')))
    *s++ = 0;
  if (s) {
    if (*s == '"') {
      realm = s;
      s += span_quoted(s);
      if (*s == ':')
	*s++ = 0;
      else
	realm = NULL, s = NULL;
    }
    else
      s = NULL;
  }
  if (s && (s = strchr(user = s, ':')))
    *s++ = 0;
  if (s && (s = strchr(pass = s, ':')))
    *s++ = 0;

  if (scheme && realm && user && pass) {
    for (; *auc_list; auc_list = &(*auc_list)->ca_next) {
      match = ca_credentials(*auc_list, scheme, realm, user, pass);
      if (match < 0) {
	retval = -1;
	break;
      }
      if (match)
	retval++;
    }
  }

  su_free(NULL, s0);

  return retval;
}

/**Feed authentication data to the authenticator.
 *
 * The function auc_credentials() is used to provide the authenticators in
 * with authentication tuple (scheme, realm, user name, secret).
 *
 * scheme:"realm":user:pass
 *
 * @param[in,out] auc_list  list of authenticators
 * @param[in] scheme        scheme to use (NULL, if any)
 * @param[in] realm         realm to use (NULL, if any)
 * @param[in] user          username
 * @param[in] pass          password
 *
 * @retval >0 or number of updated clients when successful
 * @retval 0 when no client was updated
 * @retval -1 upon an error
 */
int auc_all_credentials(auth_client_t **auc_list,
			char const *scheme,
			char const *realm,
			char const *user,
			char const *pass)
{
  int retval = 0, match;

#if HAVE_SC_CRED_H
  /* XXX: add */
#endif

  if (user && pass) {
    for (; *auc_list; auc_list = &(*auc_list)->ca_next) {
      match = ca_credentials(*auc_list, scheme, realm, user, pass);
      if (match < 0)
	return -1;
      if (match)
	retval++;
    }
  }

  return retval;
}

int ca_credentials(auth_client_t *ca,
		   char const *scheme,
		   char const *realm,
		   char const *user,
		   char const *pass)
{
  char *new_user, *new_pass;
  char *old_user, *old_pass;

  assert(ca);

  if (!ca || !ca->ca_scheme || !ca->ca_realm)
    return -1;

  if ((scheme != NULL && !su_casematch(scheme, ca->ca_scheme)) ||
      (realm != NULL && !su_strmatch(realm, ca->ca_realm)))
    return 0;

  old_user = ca->ca_user, old_pass = ca->ca_pass;

  if (su_strmatch(user, old_user) && su_strmatch(pass, old_pass))
    return 0;

  new_user = su_strdup(ca->ca_home, user);
  new_pass = su_strdup(ca->ca_home, pass);

  if (!new_user || !new_pass)
    return -1;

  ca->ca_user = new_user, ca->ca_pass = new_pass;
  if (AUTH_CLIENT_IS_EXTENDED(ca))
    ca->ca_clear = 0;

  su_free(ca->ca_home, old_user);
  su_free(ca->ca_home, old_pass);

  return 1;
}


/** Copy authentication data from @a src to @a dst.
 *
 * @retval >0 if credentials were copied
 * @retval 0 if there was no credentials to copy
 * @retval <0 if an error occurred.
 */
int auc_copy_credentials(auth_client_t **dst,
			 auth_client_t const *src)
{
  int retval = 0;

  if (!dst)
    return -1;

  for (;*dst; dst = &(*dst)->ca_next) {
    auth_client_t *d = *dst;
    auth_client_t const *ca;

    for (ca = src; ca; ca = ca->ca_next) {
      char *u, *p;
      if (!ca->ca_user || !ca->ca_pass)
	continue;
      if (AUTH_CLIENT_IS_EXTENDED(ca) && ca->ca_clear)
	continue;
      if (!ca->ca_scheme[0] || !su_casematch(ca->ca_scheme, d->ca_scheme))
	continue;
      if (!ca->ca_realm || !su_strmatch(ca->ca_realm, d->ca_realm))
	continue;

      if (!(AUTH_CLIENT_IS_EXTENDED(d) && d->ca_clear) &&
	  su_strmatch(d->ca_user, ca->ca_user) &&
	  su_strmatch(d->ca_pass, ca->ca_pass)) {
	retval++;
	break;
      }

      u = su_strdup(d->ca_home, ca->ca_user);
      p = su_strdup(d->ca_home, ca->ca_pass);
      if (!u || !p)
	return -1;

      if (d->ca_user) su_free(d->ca_home, (void *)d->ca_user);
      if (d->ca_pass) su_free(d->ca_home, (void *)d->ca_pass);
      d->ca_user = u, d->ca_pass = p;
      if (AUTH_CLIENT_IS_EXTENDED(d))
	d->ca_clear = 0;

      retval++;
      break;
    }
  }

  return retval;
}


/**Clear authentication data from the authenticator.
 *
 * The function auc_clear_credentials() is used to remove the credentials
 * from the authenticators.
 *
 * @param[in,out] auc_list  list of authenticators
 * @param[in] scheme    scheme (if non-null, remove only matching credentials)
 * @param[in] realm     realm (if non-null, remove only matching credentials)
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 */
int auc_clear_credentials(auth_client_t **auc_list,
			  char const *scheme,
			  char const *realm)
{
  int retval = 0;
  int match;

  for (; *auc_list; auc_list = &(*auc_list)->ca_next) {
    auth_client_t *ca = *auc_list;

    if (!AUTH_CLIENT_IS_EXTENDED(ca))
      continue;

    if ((scheme != NULL && !su_casematch(scheme, ca->ca_scheme)) ||
	(realm != NULL && !su_strmatch(realm, ca->ca_realm)))
      continue;

    match = ca->ca_auc->auc_clear(*auc_list);

    if (match < 0) {
      retval = -1;
      break;
    }
    if (match)
      retval++;
  }

  return retval;
}

static
int ca_clear_credentials(auth_client_t *ca)
{
  assert(ca); assert(ca->ca_home->suh_size >= (int)(sizeof *ca));

  if (!ca)
    return -1;

  ca->ca_clear = 1;

  return 1;
}

/** Check if there are credentials for all challenges.
 *
 * @retval 1 when authorization can proceed
 * @retval 0 when there is not enough credentials
 *
 * @NEW_1_12_5.
 */
int auc_has_authorization(auth_client_t **auc_list)
{
  auth_client_t const *ca, *other;

  if (auc_list == NULL)
    return 0;

  for (ca = *auc_list; ca; ca = ca->ca_next) {
    if (!ca_has_authorization(ca)) {
      /*
       * Check if we have another challenge with same realm but different
       * scheme
       */
      for (other = *auc_list; other; other = other->ca_next) {
		  if (ca == other) {
			  continue;
		  }

	if (ca->ca_credential_class == other->ca_credential_class &&
	    su_strcmp(ca->ca_realm, other->ca_realm) == 0 &&
	    ca_has_authorization(other))
	  break;
      }

      if (!other)
	return 0;
    }
  }

  return 1;
}

static int
ca_has_authorization(auth_client_t const *ca)
{
  return ca->ca_credential_class &&
    ca->ca_auc &&
    ca->ca_user && ca->ca_pass &&
    !(AUTH_CLIENT_IS_EXTENDED(ca) && ca->ca_clear);
}

/**Authorize a request.
 *
 * The function auc_authorization() is used to add correct authentication
 * headers to a request. The authentication headers will contain the
 * credentials generated by the list of authenticators.
 *
 * @param[in,out] auc_list  list of authenticators
 * @param[out] msg          message to be authenticated
 * @param[out] pub          headers of the message
 * @param[in] method        request method
 * @param[in] url           request URI
 * @param[in] body          message body (NULL if empty)
 *
 * @retval 1 when successful
 * @retval 0 when there is not enough credentials
 * @retval -1 upon an error
 */
int auc_authorization(auth_client_t **auc_list, msg_t *msg, msg_pub_t *pub,
		      char const *method,
		      url_t const *url,
		      msg_payload_t const *body)
{
  auth_client_t *ca;
  msg_mclass_t const *mc = msg_mclass(msg);

  if (auc_list == NULL || msg == NULL)
    return -1;

  if (!auc_has_authorization(auc_list))
    return 0;

  if (pub == NULL)
    pub = msg_object(msg);

  /* Remove existing credential headers */
  for (ca = *auc_list; ca; ca = ca->ca_next) {
    msg_header_t **hh = msg_hclass_offset(mc, pub, ca->ca_credential_class);

    while (hh && *hh)
      msg_header_remove(msg, pub, *hh);
  }

  /* Insert new credential headers */
  for (; *auc_list; auc_list = &(*auc_list)->ca_next) {
    su_home_t *home = msg_home(msg);
    msg_header_t *h = NULL;

    ca = *auc_list;

    if (!ca->ca_auc)
      continue;
    if (!ca_has_authorization(ca))
      continue;

    if (ca->ca_auc->auc_authorize(ca, home, method, url, body, &h) < 0)
      return -1;
    if (h == NULL)
      continue;
    if (msg_header_insert(msg, pub, h) < 0)
      return -1;
  }

  return 1;
}

/**Generate headers authorizing a request.
 *
 * The function auc_authorization_headers() is used to generate
 * authentication headers for a request. The list of authentication headers
 * will contain the credentials generated by the list of authenticators.
 *
 * @param[in] auc_list      list of authenticators
 * @param[in] home          memory home used to allocate headers
 * @param[in] method        request method
 * @param[in] url           request URI
 * @param[in] body          message body (NULL if empty)
 * @param[out] return_headers  authorization headers return value
 *
 * @retval 1 when successful
 * @retval 0 when there is not enough credentials
 * @retval -1 upon an error
 */
int auc_authorization_headers(auth_client_t **auc_list,
			      su_home_t *home,
			      char const *method,
			      url_t const *url,
			      msg_payload_t const *body,
			      msg_header_t **return_headers)
{
  auth_client_t *ca;

  /* Make sure every challenge has credentials */
  if (!auc_has_authorization(auc_list))
    return 0;

  /* Create new credential headers */
  for (; *auc_list; auc_list = &(*auc_list)->ca_next) {
    msg_header_t *h = NULL;

    ca = *auc_list;

    if (!ca->ca_auc)
      continue;
    if (!ca_has_authorization(ca))
      continue;

    if (ca->ca_auc->auc_authorize(ca, home, method, url, body, &h) < 0)
      return -1;

    *return_headers = h;

    while (*return_headers)
      return_headers = &(*return_headers)->sh_next;
  }

  return 1;
}

/* ---------------------------------------------------------------------- */
/* Basic scheme */

static int auc_basic_authorization(auth_client_t *ca,
				   su_home_t *h,
				   char const *method,
				   url_t const *url,
				   msg_payload_t const *body,
				   msg_header_t **);

static const auth_client_plugin_t ca_basic_plugin =
{
  /* auc_plugin_size: */ sizeof ca_basic_plugin,
  /* auc_size: */        sizeof (auth_client_t),
  /* auc_name: */       "Basic",
  /* auc_challenge: */   NULL,
  /* auc_authorize: */   auc_basic_authorization,
  /* auc_info: */        NULL,
  /* auc_clear: */       ca_clear_credentials
};

/**Create a basic authorization header.
 *
 * The function auc_basic_authorization() creates a basic authorization
 * header from username @a user and password @a pass. The authorization
 * header type is determined by @a hc - it can be sip_authorization_class,
 * sip_proxy_authorization_class, http_authorization_class, or
 * http_proxy_authorization_class, for instance.
 *
 * @param home memory home used to allocate memory for the new header
 * @param hc   header class for the header to be created
 * @param user user name
 * @param pass password
 *
 * @return
 * The function auc_basic_authorization() returns a pointer to newly created
 * authorization header, or NULL upon an error.
 */
int auc_basic_authorization(auth_client_t *ca,
			    su_home_t *home,
			    char const *method,
			    url_t const *url,
			    msg_payload_t const *body,
			    msg_header_t **return_headers)
{
  msg_hclass_t *hc = ca->ca_credential_class;
  char const *user = ca->ca_user;
  char const *pass = ca->ca_pass;
  size_t ulen, plen, uplen, b64len, basiclen;
  char *basic, *base64, *userpass;
  char buffer[71];

  if (user == NULL || pass == NULL)
    return -1;

  if (AUTH_CLIENT_IS_EXTENDED(ca) && ca->ca_clear)
    return 0;

  ulen = strlen(user), plen = strlen(pass), uplen = ulen + 1 + plen;
  b64len = BASE64_SIZE(uplen);
  basiclen = strlen("Basic ") + b64len;

  if (sizeof(buffer) > basiclen + 1)
    basic = buffer;
  else
    basic = malloc(basiclen + 1);

  if (basic == NULL)
    return -1;

  /*
   * Basic authentication consists of username and password separated by
   * colon and then base64 encoded.
   */
  strcpy(basic, "Basic ");
  base64 = basic + strlen("Basic ");
  userpass = base64 + b64len - uplen;
  memcpy(userpass, user, ulen);
  userpass[ulen] = ':';
  memcpy(userpass + ulen + 1, pass, plen);
  userpass[uplen] = '\0';

  base64_e(base64, b64len + 1, userpass, uplen);

  base64[b64len] = '\0';

  *return_headers = msg_header_make(home, hc, basic);

  if (buffer != basic)
    free(basic);

  return *return_headers ? 0 : -1;
}

/* ---------------------------------------------------------------------- */
/* Digest scheme */

typedef struct auth_digest_client_s
{
  auth_client_t cda_client;

  int           cda_ncount;
  char const   *cda_cnonce;
  auth_challenge_t cda_ac[1];
} auth_digest_client_t;

static int auc_digest_challenge(auth_client_t *ca,
				msg_auth_t const *ch);
static int auc_digest_authorization(auth_client_t *ca,
				    su_home_t *h,
				    char const *method,
				    url_t const *url,
				    msg_payload_t const *body,
				    msg_header_t **);
static int auc_digest_info(auth_client_t *ca,
			   msg_auth_info_t const *info);

static const auth_client_plugin_t ca_digest_plugin =
{
  /* auc_plugin_size: */ sizeof ca_digest_plugin,
  /* auc_size: */        sizeof (auth_digest_client_t),
  /* auc_name: */       "Digest",
  /* auc_challenge: */   auc_digest_challenge,
  /* auc_authorize: */   auc_digest_authorization,
  /* auc_info: */        auc_digest_info,
  /* auc_clear: */       ca_clear_credentials
};

/** Store a digest authorization challenge.
 *
 * @retval 2 if credentials need to be (re)sent
 * @retval 1 if challenge was updated
 * @retval -1 upon an error
 */
static int auc_digest_challenge(auth_client_t *ca, msg_auth_t const *ch)
{
  su_home_t *home = ca->ca_home;
  auth_digest_client_t *cda = (auth_digest_client_t *)ca;
  auth_challenge_t ac[1] = {{ sizeof ac }};
  int stale;

  if (auth_digest_challenge_get(home, ac, ch->au_params) < 0)
    goto error;

  /* Check that we can handle the challenge */
  if (!ac->ac_md5 && !ac->ac_md5sess)
    goto error;
  if (ac->ac_qop && !ac->ac_auth && !ac->ac_auth_int)
    goto error;

  stale = ac->ac_stale || cda->cda_ac->ac_nonce == NULL;

  if (ac->ac_qop && (cda->cda_cnonce == NULL || ac->ac_stale)) {
    su_guid_t guid[1];
    char *cnonce;
    size_t b64len = BASE64_MINSIZE(sizeof(guid)) + 1;
    if (cda->cda_cnonce != NULL)
      /* Free the old one if we are updating after stale=true */
      su_free(home, (void *)cda->cda_cnonce);
    su_guid_generate(guid);
    cda->cda_cnonce = cnonce = su_alloc(home, b64len);
    base64_e(cnonce, b64len, guid, sizeof(guid));
    cda->cda_ncount = 0;
  }

  auth_digest_challenge_free_params(home, cda->cda_ac);

  *cda->cda_ac = *ac;

  return stale ? 2 : 1;

 error:
  auth_digest_challenge_free_params(home, ac);
  return -1;
}

static int auc_digest_info(auth_client_t *ca,
			   msg_auth_info_t const *info)
{
  auth_digest_client_t *cda = (auth_digest_client_t *)ca;
  su_home_t *home = ca->ca_home;
  char const *nextnonce = NULL;
  issize_t n;

  n = auth_get_params(home, info->ai_params,
		      "nextnonce=", &nextnonce,
		      NULL);

  if (n <= 0)
    return n;

  cda->cda_ac->ac_nonce = nextnonce;

  return 1;
}

/**Create a digest authorization header.
 *
 * Creates a digest authorization header from username @a user and password
 * @a pass, client nonce @a cnonce, client nonce count @a nc, request method
 * @a method, request URI @a uri and message body @a data. The authorization
 * header type is determined by @a hc - it can be either
 * sip_authorization_class or sip_proxy_authorization_class, as well as
 * http_authorization_class or http_proxy_authorization_class.
 *
 * @retval 1 when authorization headers has been created
 * @retval 0 when there is no credentials
 * @retval -1 upon an error
 */
static
int auc_digest_authorization(auth_client_t *ca,
			     su_home_t *home,
			     char const *method,
			     url_t const *url,
			     msg_payload_t const *body,
			     msg_header_t **return_headers)
{
  auth_digest_client_t *cda = (auth_digest_client_t *)ca;
  msg_hclass_t *hc = ca->ca_credential_class;
  char const *user = ca->ca_user;
  char const *pass = ca->ca_pass;
  auth_challenge_t const *ac = cda->cda_ac;
  char const *cnonce = cda->cda_cnonce;
  unsigned nc = ++cda->cda_ncount;
  void const *data = body ? body->pl_data : "";
  usize_t dlen = body ? body->pl_len : 0;
  char *uri;

  msg_header_t *h;
  auth_hexmd5_t sessionkey, response;
  auth_response_t ar[1] = {{ 0 }};
  char ncount[17];

  if (!user || !pass || (AUTH_CLIENT_IS_EXTENDED(ca) && ca->ca_clear))
    return 0;

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
  ar->ar_uri = uri = url_as_string(home, url);

  if (ar->ar_uri == NULL)
    return -1;

  /* If there is no qop, we MUST NOT include cnonce or nc */
  if (!ar->ar_auth && !ar->ar_auth_int)
    cnonce = NULL;

  if (cnonce) {
    snprintf(ncount, sizeof(ncount), "%08x", nc);
    ar->ar_cnonce = cnonce;
    ar->ar_nc = ncount;
  }

  auth_digest_sessionkey(ar, sessionkey, pass);
  auth_digest_response(ar, response, sessionkey, method, data, dlen);

  h = msg_header_format(home, hc,
			"Digest "
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


/* ---------------------------------------------------------------------- */

#define MAX_AUC 20

static auth_client_plugin_t const *ca_plugins[MAX_AUC] =
{
  &ca_digest_plugin, &ca_basic_plugin, NULL
};

/** Register an authentication client plugin */
int auc_register_plugin(auth_client_plugin_t const *plugin)
{
  int i;

  if (plugin == NULL ||
      plugin->auc_name == NULL ||
      plugin->auc_authorize == NULL)
    return errno = EFAULT, -1;

  if (plugin->auc_size < (int) sizeof (auth_client_t))
    return errno = EINVAL, -1;

  for (i = 0; i < MAX_AUC; i++) {
    if (ca_plugins[i] == NULL ||
	su_strmatch(plugin->auc_name, ca_plugins[i]->auc_name) == 0) {
      ca_plugins[i] = plugin;
      return 0;
    }
  }

  return errno = ENOMEM, -1;
}

/** Allocate an (possibly extended) auth_client_t structure. */
static
auth_client_t *ca_create(su_home_t *home,
			 char const *scheme,
			 char const *realm)
{
  auth_client_plugin_t const *auc = NULL;
  auth_client_t *ca;
  size_t aucsize = (sizeof *ca), realmlen, size;
  char *s;
  int i;

  if (scheme == NULL || realm == NULL)
    return (void)(errno = EFAULT), NULL;

  realmlen = strlen(realm) + 1;

  for (i = 0; i < MAX_AUC; i++) {
    auc = ca_plugins[i];
    if (!auc || su_casematch(auc->auc_name, scheme))
      break;
  }

  /* XXX - should report error if the auth scheme is not known? */

  aucsize = auc ? (size_t)auc->auc_size : (sizeof *ca);
  size = aucsize + realmlen;
  if (!auc)
    size += strlen(scheme) + 1;

  ca = su_home_clone(home, (isize_t)size);
  if (!ca)
    return ca;

  s = (char *)ca + aucsize;
  ca->ca_auc = auc;
  ca->ca_realm = strcpy(s, realm);
  ca->ca_scheme = auc ? auc->auc_name : strcpy(s + realmlen, scheme);

  return ca;
}

void ca_destroy(su_home_t *home, auth_client_t *ca)
{
  su_free(home, ca);
}


#if HAVE_SOFIA_SIP
#include <sofia-sip/sip.h>

/**Authorize a SIP request.
 *
 * The function auc_authorize() is used to add correct authentication
 * headers to a SIP request. The authentication headers will contain the
 * credentials generated by the list of authenticators.
 *
 * @param[in,out] auc_list  list of authenticators
 * @param[in,out] msg       message to be authenticated
 * @param[in,out] sip       sip headers of the message
 *
 * @retval 1 when successful
 * @retval 0 when there is not enough credentials
 * @retval -1 upon an error
 */
int auc_authorize(auth_client_t **auc_list, msg_t *msg, sip_t *sip)
{
  sip_request_t *rq = sip ? sip->sip_request : NULL;

  if (!rq)
    return 0;

  return auc_authorization(auc_list, msg, (msg_pub_t *)sip,
			   rq->rq_method_name,
			   /*
			     RFC 3261 defines the protection domain based
			     only on realm, so we do not bother get a
			     correct URI to auth module.
			   */
			   rq->rq_url,
			   sip->sip_payload);
}
#endif
