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

/**@CFILE auth_module.c
 * @brief Authentication verification module
 *
 * The authentication module provides server or proxy-side authentication
 * verification for network elements like registrars, presence servers, and
 * proxies.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Wed Apr 11 15:14:03 2001 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sofia-sip/auth_digest.h>

#include "iptsec_debug.h"

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
static char const __func__[] = "auth_mod";
#endif

#include <sofia-sip/su_debug.h>

#include <sofia-sip/su_wait.h>
#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_string.h>
#include <sofia-sip/su_tagarg.h>

#include <sofia-sip/base64.h>
#include <sofia-sip/su_md5.h>

#include <sofia-sip/msg_parser.h>
#include <sofia-sip/msg_date.h>

#include "sofia-sip/auth_module.h"
#include "sofia-sip/auth_plugin.h"

#define APW_HASH(apw) ((apw)->apw_index)

char const auth_internal_server_error[] = "Internal server error";

static void auth_call_scheme_destructor(void *);
static void auth_md5_hmac_key(auth_mod_t *am);

HTABLE_PROTOS_WITH(auth_htable, aht, auth_passwd_t, usize_t, unsigned);
HTABLE_BODIES_WITH(auth_htable, aht, auth_passwd_t, APW_HASH,
		   usize_t, unsigned);

/**Allocate an authentication module instance.
 *
 * The function auth_mod_alloc() allocates an authentication module object.
 *
 */
auth_mod_t *auth_mod_alloc(auth_scheme_t *scheme,
			   tag_type_t tag, tag_value_t value, ...)
{
  auth_mod_t *am = NULL;

  if ((am = su_home_new(scheme->asch_size))) {
    am->am_scheme = scheme;
    su_home_destructor(am->am_home, auth_call_scheme_destructor);
  }

  return am;
}

/**Initialize an authentication module instance.
 *
 * The function auth_mod_init_default() initializes an authentication module
 * object used to authenticate the requests.
 *
 * @param am
 * @param base
 * @param root
 * @param tag,value,... tagged argument list
 *
 * @TAGS
 * AUTHTAG_REALM(), AUTHTAG_OPAQUE(), AUTHTAG_DB(), AUTHTAG_QOP(),
 * AUTHTAG_ALGORITHM(), AUTHTAG_EXPIRES(), AUTHTAG_NEXT_EXPIRES(),
 * AUTHTAG_BLACKLIST(), AUTHTAG_FORBIDDEN(), AUTHTAG_ANONYMOUS(),
 * AUTHTAG_FAKE(), AUTHTAG_ALLOW(), AUTHTAG_REMOTE(), and
 * AUTHTAG_MASTER_KEY().
 *
 * @return 0 if successful
 * @return -1 upon an error
 */
int auth_init_default(auth_mod_t *am,
		      auth_scheme_t *base,
		      su_root_t *root,
		      tag_type_t tag, tag_value_t value, ...)
{
  int retval = 0;

  ta_list ta;

  char const *realm = NULL, *opaque = NULL, *db = NULL, *allows = NULL;
  char const *qop = NULL, *algorithm = NULL;
  unsigned expires = 60 * 60, next_expires = 5 * 60;
  unsigned max_ncount = 0;
  unsigned blacklist = 5;
  int forbidden = 0;
  int anonymous = 0;
  int fake = 0;
  url_string_t const *remote = NULL;
  char const *master_key = "fish";
  char *s;

  ta_start(ta, tag, value);

  /* Authentication stuff */
  tl_gets(ta_args(ta),
	  AUTHTAG_REALM_REF(realm),
	  AUTHTAG_OPAQUE_REF(opaque),
	  AUTHTAG_DB_REF(db),
	  AUTHTAG_QOP_REF(qop),
	  AUTHTAG_ALGORITHM_REF(algorithm),
	  AUTHTAG_EXPIRES_REF(expires),
	  AUTHTAG_NEXT_EXPIRES_REF(next_expires),
	  AUTHTAG_MAX_NCOUNT_REF(max_ncount),
	  AUTHTAG_BLACKLIST_REF(blacklist),
	  AUTHTAG_FORBIDDEN_REF(forbidden),
	  AUTHTAG_ANONYMOUS_REF(anonymous),
	  AUTHTAG_FAKE_REF(fake),
	  AUTHTAG_ALLOW_REF(allows),
	  AUTHTAG_REMOTE_REF(remote),
	  AUTHTAG_MASTER_KEY_REF(master_key),
	  TAG_NULL());

  if (!realm) realm = "*";
  if (!allows) allows = "ACK, BYE, CANCEL";

  am->am_realm = su_strdup(am->am_home, realm);
  am->am_opaque = su_strdup(am->am_home, opaque);
  am->am_db = su_strdup(am->am_home, db);
  s = su_strdup(am->am_home, allows);
  if (s)
    msg_commalist_d(am->am_home, &s, &am->am_allow, NULL);
  am->am_expires = expires;
  am->am_next_exp = next_expires;
  am->am_max_ncount = max_ncount;
  am->am_blacklist = blacklist;
  am->am_forbidden = forbidden;
  am->am_anonymous = anonymous;
  am->am_fake = fake;
  am->am_remote = url_hdup(am->am_home, (url_t *)remote);
  am->am_algorithm = algorithm ? su_strdup(am->am_home, algorithm) : "MD5";
  am->am_nextnonce = !algorithm || su_casematch(algorithm, "MD5");
  if (next_expires == 0)
    am->am_nextnonce = 0;
  am->am_qop = su_strdup(am->am_home, qop);

  if (master_key) {
    su_md5_t md5[1];

    su_md5_init(md5);
    su_md5_strupdate(md5, master_key);
    su_md5_strupdate(md5, "70P 53KR37");
    su_md5_digest(md5, am->am_master_key);
  }

  auth_md5_hmac_key(am);

  /* Make sure that we have something
     that can be used to identify credentials */
  if (am->am_opaque && strcmp(am->am_opaque, "*") == 0) {
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif
    char hostname[HOST_NAME_MAX + 1];
    su_md5_t md5[1];
    uint8_t hmac[6];

    gethostname(hostname, sizeof hostname);

    auth_md5_hmac_init(am, md5);

    su_md5_strupdate(md5, hostname);
    su_md5_update(md5, ":", 1);
    if (am->am_remote)
      url_update(md5, am->am_remote);

    auth_md5_hmac_digest(am, md5, hmac, sizeof hmac);

    base64_e(hostname, sizeof hostname, hmac, sizeof hmac);

    am->am_opaque = su_strdup(am->am_home, hostname);

    if (!am->am_opaque) {
      retval = -1;
      SU_DEBUG_1(("%s: cannot create unique identifier\n", __func__));
    }
  }

  if (retval < 0)
    ;
  else if (db) {
    retval = auth_readdb(am);
    if (retval == -1) {
      int err = errno;
      SU_DEBUG_1(("auth-module: %s: %s\n", am->am_db, strerror(err)));
      errno = err;
    }
  }
  else {
    retval = auth_htable_resize(am->am_home, am->am_users, 0);
  }

  ta_end(ta);

  return retval;
}

/** Destroy (a reference to) an authentication module. */
void auth_mod_destroy(auth_mod_t *am)
{
  su_home_unref(am->am_home);
}

/** Call scheme-specific destructor function. */
static void auth_call_scheme_destructor(void *arg)
{
  auth_mod_t *am = arg;
  am->am_scheme->asch_destroy(am);
}

/** Do-nothing destroy function.
 *
 * The auth_destroy_default() is the default member function called by
 * auth_mod_destroy().
 */
void auth_destroy_default(auth_mod_t *am)
{
}

/** Create a new reference to authentication module. */
auth_mod_t *auth_mod_ref(auth_mod_t *am)
{
  return (auth_mod_t *)su_home_ref(am->am_home);
}

/** Destroy a reference to an authentication module. */
void auth_mod_unref(auth_mod_t *am)
{
  su_home_unref(am->am_home);
}

/** Get authenticatin module name. @NEW_1_12_4. */
char const *auth_mod_name(auth_mod_t *am)
{
  return am ? am->am_scheme->asch_method : "<nil>";
}

/** Initialize a auth_status_t stucture.
 *
 * @retval NULL upon an error
 * @relates auth_status_t
 */
auth_status_t *auth_status_init(void *p, isize_t size)
{
  return auth_status_init_with(p, size, 500, auth_internal_server_error);
}

/** Initialize a auth_status_t stucture.
 *
 * @retval NULL upon an error
 * @relates auth_status_t
 */
auth_status_t *auth_status_init_with(void *p,
				     isize_t size,
				     int status,
				     char const *phrase)
{
  auth_status_t *as;

  if (!p || size < (sizeof *as))
    return NULL;

  if (size > INT_MAX) size = INT_MAX;

  as = memset(p, 0, size);
  as->as_home->suh_size = (int)size;

  /* su_home_init(as->as_home); */

  as->as_status = status, as->as_phrase = phrase;

  return as;
}

/** Allocate a new auth_status_t structure. @relates auth_status_t */

auth_status_t *auth_status_new(su_home_t *home)
{
  auth_status_t *as = su_home_clone(home, (sizeof *as));
  if (as) {
    as->as_status = 500;
    as->as_phrase = auth_internal_server_error;
  }
  return as;
}

/** Create a new reference to an auth_status_t structure.
 * @relates auth_status_t
 */
auth_status_t *auth_status_ref(auth_status_t *as)
{
  return (auth_status_t *)su_home_ref(as->as_home);
}

/** Destroy (a reference to) an auth_status_t structure. @relates auth_status_t
 */
void auth_status_unref(auth_status_t *as)
{
  su_home_unref(as->as_home);
}

/** Authenticate user.
 *
 * The function auth_mod_method() invokes scheme-specific authentication
 * operation where the user's credentials are checked using scheme-specific
 * method. The authentication result along with an optional challenge header
 * is stored in the @a as structure.
 *
 * @param am           pointer to authentication module object [in]
 * @param as           pointer to authentication status structure [in/out]
 * @param credentials  pointer to a header with user's credentials [in]
 * @param ach          pointer to a structure describing challenge [in]
 *
 * The @a ach structure defines what kind of response and challenge header
 * is returned to the user. For example, a server authentication is
 * implemented with 401 response code and phrase along with WWW-Authenticate
 * header template in the @a ach structure.
 *
 * The auth_mod_method() returns the authentication result in the
 * #auth_mod_t @a as structure. The @a as->as_status describes the result
 * as follows:
 * - <i>as->as_status == 0</i> authentication is successful
 * - <i>as->as_status == 100</i> authentication is pending
 * - <i>as->as_status >= 400</i> authentication fails,
 *   return as_status as an error code to client
 *
 * When the authentication is left pending, the client must set the
 * as_callback pointer in @a as structure to an appropriate callback
 * function. The callback is invoked when the authentication is completed,
 * either successfully or with an error.
 *
 * Note that the authentication module may generate a new challenge each
 * time authentication is used (e.g., Digest using MD5 algorithm). Such a
 * challenge header is stored in the @a as->as_response return-value field.
 *
 * @note The authentication plugin may use the given reference to @a as, @a
 * credentials and @a ach structures until the asynchronous authentication
 * completes. Therefore, they should not be allocated from stack unless
 * application uses strictly synchronous authentication schemes only (Basic
 * and Digest).
 *
 * @note This function should be called auth_mod_check().
 */
void auth_mod_verify(auth_mod_t *am,
		     auth_status_t *as,
		     msg_auth_t *credentials,
		     auth_challenger_t const *ach)
{
  char const *wildcard, *host;

  if (!am || !as || !ach)
    return;

  wildcard = strchr(am->am_realm, '*');

  host = as->as_domain;

  /* Initialize per-request realm */
  if (as->as_realm)
    ;
  else if (!wildcard) {
    as->as_realm = am->am_realm;
  }
  else if (!host) {
    return;			/* Internal error */
  }
  else if (strcmp(am->am_realm, "*") == 0) {
    as->as_realm = host;
  }
  else {
    /* Replace * with hostpart */
    as->as_realm = su_sprintf(as->as_home, "%.*s%s%s",
			      (int)(wildcard - am->am_realm), am->am_realm,
			      host,
			      wildcard + 1);
  }

  am->am_scheme->asch_check(am, as, credentials, ach);
}

/** Make a challenge header.
 *
 * This function invokes plugin-specific member function generating a
 * challenge header. Client uses the challenge header contents when
 * prompting the user for a username and password then generates its
 * credential header using the parameters given in the challenge header.
 *
 * @param am pointer to authentication module object
 * @param as pointer to authentication status structure (return-value)
 * @param ach pointer to a structure describing challenge
 *
 * The auth_mod_challenge() returns the challenge header, appropriate
 * response code and reason phrase in the #auth_status_t structure. The
 * auth_mod_challenge() is currently always synchronous function.
 */
void auth_mod_challenge(auth_mod_t *am,
			auth_status_t *as,
			auth_challenger_t const *ach)
{
  if (am && as && ach)
    am->am_scheme->asch_challenge(am, as, ach);
}


/** Cancel asynchronous authentication.
 *
 * The auth_mod_cancel() function cancels a pending authentication.
 * Application can reclaim the authentication status, credential and
 * challenger objects by using auth_mod_cancel().
 */
void auth_mod_cancel(auth_mod_t *am, auth_status_t *as)
{
  if (am && as)
    am->am_scheme->asch_cancel(am, as);
}

/** Do-nothing cancel function.
 *
 * The auth_cancel_default() is the default member function called by
 * auth_mod_cancel().
 */
void auth_cancel_default(auth_mod_t *am, auth_status_t *as)
{
}

/* ====================================================================== */
/* Basic authentication scheme */

static void auth_method_basic_x(auth_mod_t *am,
				auth_status_t *as,
				msg_auth_t *au,
				auth_challenger_t const *ach);

auth_scheme_t auth_scheme_basic[1] =
  {{
      "Basic",			/* asch_method */
      sizeof (auth_mod_t),	/* asch_size */
      auth_init_default,	/* asch_init */
      auth_method_basic_x, 	/* asch_check */
      auth_challenge_basic,	/* asch_challenge */
      auth_cancel_default,	/* asch_cancel */
      auth_destroy_default	/* asch_destroy */
  }};

/**Authenticate a request with @b Basic authentication.
 *
 * This function reads user database before authentication, if needed.
 */
static
void auth_method_basic_x(auth_mod_t *am,
			 auth_status_t *as,
			 msg_auth_t *au,
			 auth_challenger_t const *ach)
{
  if (am) {
    auth_readdb_if_needed(am);
    auth_method_basic(am, as, au, ach);
  }
}

/** Authenticate a request with @b Basic authentication scheme.
 *
 */
void auth_method_basic(auth_mod_t *am,
		       auth_status_t *as,
		       msg_auth_t *au,
		       auth_challenger_t const *ach)
{
  char *userpass, buffer[128];
  size_t n, upsize;
  char *pass;
  auth_passwd_t *apw;

  if (!as->as_realm)
    return;

  userpass = buffer, upsize = sizeof buffer;

  for (au = auth_mod_credentials(au, "Basic", NULL);
       au;
       au = auth_mod_credentials(au->au_next, "Basic", NULL)) {
    if (!au->au_params)
      continue;
    n = base64_d(userpass, upsize - 1, au->au_params[0]);
    if (n < 0 || n >= INT_MAX)
      continue;
    if (n >= upsize) {
      void *b = realloc(userpass == buffer ? NULL : userpass, upsize = n + 1);
      if (b == NULL)
	break;
      base64_d(userpass = b, upsize - 1, au->au_params[0]);
    }
    userpass[n] = 0;
    if (!(pass = strchr(userpass, ':')))
      continue;
    *pass++ = '\0';
    SU_DEBUG_5(("auth_method_basic: %s => %s:%s\n",
		au->au_params[0], userpass, pass));

    if (!(apw = auth_mod_getpass(am, userpass, as->as_realm)))
      continue;
    if (strcmp(apw->apw_pass, pass))
      continue;

    as->as_user = apw->apw_user;
    as->as_anonymous = apw == am->am_anon_user;
    as->as_ident = apw->apw_ident;
    as->as_match = (msg_header_t *)au;
    as->as_status = 0;	/* Successful authentication! */

    break;
  }

  if (userpass != buffer)
    free(userpass);

  if (au)
    return;

  if (auth_allow_check(am, as))
    auth_challenge_basic(am, as, ach);
}

/** Construct a challenge header for @b Basic authentication scheme. */
void auth_challenge_basic(auth_mod_t *am,
			  auth_status_t *as,
			  auth_challenger_t const *ach)
{
  as->as_status = ach->ach_status;
  as->as_phrase = ach->ach_phrase;
  as->as_response = msg_header_format(as->as_home, ach->ach_header,
				      "Basic realm=\"%s\"", as->as_realm);
}

/* ====================================================================== */
/* Digest authentication scheme */

static void auth_method_digest_x(auth_mod_t *am,
				 auth_status_t *as,
				 msg_auth_t *au,
				 auth_challenger_t const *ach);

auth_scheme_t auth_scheme_digest[1] =
  {{
      "Digest",			/* asch_method */
      sizeof (auth_mod_t),	/* asch_size */
      auth_init_default,	/* asch_init */
      auth_method_digest_x,	/* asch_check */
      auth_challenge_digest,	/* asch_challenge */
      auth_cancel_default,	/* asch_cancel */
      auth_destroy_default	/* asch_destroy */
  }};

struct nonce {
  msg_time_t issued;
  uint32_t   count;
  uint16_t   nextnonce;
  uint8_t    digest[6];
};

#define AUTH_DIGEST_NONCE_LEN (BASE64_MINSIZE(sizeof (struct nonce)) + 1)

/** Authenticate a request with @b Digest authentication scheme.
 *
 * This function reads user database before authentication, if needed.
 */
static
void auth_method_digest_x(auth_mod_t *am,
			  auth_status_t *as,
			  msg_auth_t *au,
			  auth_challenger_t const *ach)
{
  if (am) {
    auth_readdb_if_needed(am);
    auth_method_digest(am, as, au, ach);
  }
}

/** Authenticate a request with @b Digest authentication scheme.
 */
void auth_method_digest(auth_mod_t *am,
			auth_status_t *as,
			msg_auth_t *au,
			auth_challenger_t const *ach)
{
  as->as_allow = as->as_allow || auth_allow_check(am, as) == 0;

  if (as->as_realm)
    au = auth_digest_credentials(au, as->as_realm, am->am_opaque);
  else
    au = NULL;

  if (as->as_allow) {
    SU_DEBUG_5(("%s: allow unauthenticated %s\n", __func__, as->as_method));
    as->as_status = 0, as->as_phrase = NULL;
    as->as_match = (msg_header_t *)au;
    return;
  }

  if (au) {
    auth_response_t ar[1] = {{ sizeof(ar) }};
    auth_digest_response_get(as->as_home, ar, au->au_params);
    as->as_match = (msg_header_t *)au;
    auth_check_digest(am, as, ar, ach);
  }
  else {
    /* There was no matching credentials, send challenge */
    SU_DEBUG_5(("%s: no credentials matched\n", __func__));
    auth_challenge_digest(am, as, ach);
  }
}

/** Verify digest authentication */
void auth_check_digest(auth_mod_t *am,
		       auth_status_t *as,
		       auth_response_t *ar,
		       auth_challenger_t const *ach)
{
  char const *a1;
  auth_hexmd5_t a1buf, response;
  auth_passwd_t *apw;
  char const *phrase;
  msg_time_t now = msg_now();

  if (am == NULL || as == NULL || ar == NULL || ach == NULL) {
    if (as) {
      as->as_status = 500, as->as_phrase = "Internal Server Error";
      as->as_response = NULL;
    }
    return;
  }

  phrase = "Bad authorization";

#define PA "Authorization missing "

  if ((!ar->ar_username && (phrase = PA "username")) ||
      (!ar->ar_nonce && (phrase = PA "nonce")) ||
      (!ar->ar_uri && (phrase = PA "URI")) ||
      (!ar->ar_response && (phrase = PA "response")) ||
      /* (!ar->ar_opaque && (phrase = PA "opaque")) || */
      /* Check for qop */
      (ar->ar_qop &&
       ((ar->ar_auth &&
	 !su_casematch(ar->ar_qop, "auth") &&
	 !su_casematch(ar->ar_qop, "\"auth\"")) ||
	(ar->ar_auth_int &&
	 !su_casematch(ar->ar_qop, "auth-int") &&
	 !su_casematch(ar->ar_qop, "\"auth-int\"")))
       && (phrase = PA "has invalid qop"))) {
    assert(phrase);
    SU_DEBUG_5(("auth_method_digest: 400 %s\n", phrase));
    as->as_status = 400, as->as_phrase = phrase;
    as->as_response = NULL;
    return;
  }

  if (as->as_nonce_issued == 0 /* Already validated nonce */ &&
      auth_validate_digest_nonce(am, as, ar, now) < 0) {
    as->as_blacklist = am->am_blacklist;
    auth_challenge_digest(am, as, ach);
    return;
  }

  if (as->as_stale) {
    auth_challenge_digest(am, as, ach);
    return;
  }

  apw = auth_mod_getpass(am, ar->ar_username, ar->ar_realm);

  if (apw && apw->apw_hash)
    a1 = apw->apw_hash;
  else if (apw && apw->apw_pass)
    auth_digest_a1(ar, a1buf, apw->apw_pass), a1 = a1buf;
  else
    auth_digest_a1(ar, a1buf, "xyzzy"), a1 = a1buf, apw = NULL;

  if (ar->ar_md5sess)
    auth_digest_a1sess(ar, a1buf, a1), a1 = a1buf;

  auth_digest_response(ar, response, a1,
		       as->as_method, as->as_body, as->as_bodylen);

  if (!apw || strcmp(response, ar->ar_response)) {

    if (am->am_forbidden) {
      as->as_status = 403, as->as_phrase = "Forbidden";
      as->as_response = NULL;
      as->as_blacklist = am->am_blacklist;
    }
    else {
      auth_challenge_digest(am, as, ach);
      as->as_blacklist = am->am_blacklist;
    }
    SU_DEBUG_5(("auth_method_digest: response did not match\n"));

    return;
  }

  assert(apw);

  as->as_user = apw->apw_user;
  as->as_anonymous = apw == am->am_anon_user;
  as->as_ident = apw->apw_ident;

  if (am->am_nextnonce || am->am_mutual)
    auth_info_digest(am, as, ach);

  if (am->am_challenge)
    auth_challenge_digest(am, as, ach);

  SU_DEBUG_7(("auth_method_digest: successful authentication\n"));

  as->as_status = 0;	/* Successful authentication! */
  as->as_phrase = "";
}

/** Construct a challenge header for @b Digest authentication scheme. */
void auth_challenge_digest(auth_mod_t *am,
			   auth_status_t *as,
			   auth_challenger_t const *ach)
{
  char const *u, *d;
  char nonce[AUTH_DIGEST_NONCE_LEN];

  auth_generate_digest_nonce(am, nonce, sizeof nonce, 0, msg_now());

  u = as->as_uri;
  d = as->as_pdomain;

  as->as_response =
    msg_header_format(as->as_home, ach->ach_header,
		      "Digest"
		      " realm=\"%s\","
		      "%s%s%s"
		      "%s%s%s"
		      " nonce=\"%s\","
		      "%s%s%s"
		      "%s"	/* stale */
		      " algorithm=%s"
		      "%s%s%s",
		      as->as_realm,
		      u ? " uri=\"" : "", u ? u : "", u ? "\"," : "",
		      d ? " domain=\"" : "", d ? d : "", d ? "\"," : "",
		      nonce,
		      am->am_opaque ? " opaque=\"" : "",
		      am->am_opaque ? am->am_opaque : "",
		      am->am_opaque ? "\"," : "",
		      as->as_stale ? " stale=true," : "",
		      am->am_algorithm,
		      am->am_qop ? ", qop=\"" : "",
		      am->am_qop ? am->am_qop : "",
		      am->am_qop ? "\"" : "");

  if (!as->as_response)
    as->as_status = 500, as->as_phrase = auth_internal_server_error;
  else
    as->as_status = ach->ach_status, as->as_phrase = ach->ach_phrase;
}

/** Construct a info header for @b Digest authentication scheme. */
void auth_info_digest(auth_mod_t *am,
		      auth_status_t *as,
		      auth_challenger_t const *ach)
{
  if (!ach->ach_info)
    return;

  if (am->am_nextnonce) {
    char nonce[AUTH_DIGEST_NONCE_LEN];

    auth_generate_digest_nonce(am, nonce, sizeof nonce, 1, msg_now());

    as->as_info =
      msg_header_format(as->as_home, ach->ach_info, "nextnonce=\"%s\"", nonce);
  }
}


/* ====================================================================== */
/* Password database */

su_inline void
auth_htable_append_local(auth_htable_t *pr, auth_passwd_t *apw);

/** Get an passwd entry for user. */
auth_passwd_t *auth_mod_getpass(auth_mod_t *am,
				char const *user,
				char const *realm)
{
  auth_passwd_t *apw, **slot;
  unsigned hash;

  if (am == NULL || user == NULL)
    return NULL;

  hash = msg_hash_string(user);

  for (slot = auth_htable_hash(am->am_users, hash);
       (apw = *slot);
       slot = auth_htable_next(am->am_users, slot)) {
    if (hash != apw->apw_index)
      continue;
    if (strcmp(user, apw->apw_user))
      continue;
    if (realm && apw->apw_realm[0] && strcmp(realm, apw->apw_realm))
      continue;
    break;			/* Found it */
  }

  return apw;
}

/** Add a password entry. */
auth_passwd_t *auth_mod_addpass(auth_mod_t *am,
				char const *user,
				char const *realm)
{
  auth_passwd_t *apw, **slot;
  unsigned index;

  if (am == NULL || user == NULL)
    return NULL;

  index = msg_hash_string(user);

  for (slot = auth_htable_hash(am->am_users, index);
       (apw = *slot);
       slot = auth_htable_next(am->am_users, slot)) {
    if (index != apw->apw_index)
      continue;
    if (strcmp(user, apw->apw_user))
      continue;
    if (realm && strcmp(realm, apw->apw_realm))
      continue;
    break;			/* Found it */
  }

  if (realm == NULL)
    realm = "";

  if (!apw) {
    size_t ulen = strlen(user) + 1, rlen = strlen(realm) + 1;
    size_t size = sizeof *apw + ulen + rlen;

    apw = su_alloc(am->am_home, size);

    if (apw) {
      memset(apw, 0, sizeof *apw);
      apw->apw_index = index;
      apw->apw_user = memcpy((char *)(apw + 1), user, ulen);
      apw->apw_realm = memcpy((char *)apw->apw_user + ulen, realm, rlen);

      if (!auth_htable_is_full(am->am_users)) {
	*slot = apw, am->am_users->aht_used++;
      } else {
	if (auth_htable_resize(am->am_home, am->am_users, 0) < 0)
	  su_free(am->am_home, apw), apw = NULL;
	else
	  auth_htable_append(am->am_users, apw);
      }
    }
  }

  return apw;
}

static ssize_t readfile(su_home_t *, FILE *,
			void **contents, int add_trailing_lf);
static int auth_readdb_internal(auth_mod_t *am, int always);

/** Read authentication database */
int auth_readdb(auth_mod_t *am)
{
  return auth_readdb_internal(am, 1);
}

/** Read authentication database only when needed */
int auth_readdb_if_needed(auth_mod_t *am)
{
  struct stat st[1];

  if (!am->am_stat || !am->am_db)
    return 0;

  if (stat(am->am_db, st) != -1 &&
      st->st_dev == am->am_stat->st_dev &&
      st->st_ino == am->am_stat->st_ino &&
      st->st_size == am->am_stat->st_size &&
      memcmp(&st->st_mtime, &am->am_stat->st_mtime,
	     (sizeof st->st_mtime)) == 0)
    /* Nothing has changed or passwd file is removed */
    return 0;

  return auth_readdb_internal(am, 0);
}

#if HAVE_FLOCK
#include <sys/file.h>
#endif

/* This is just a magic value */
#define auth_apw_local ((void *)(intptr_t)auth_readdb_internal)

/** Read authentication database */
static
int auth_readdb_internal(auth_mod_t *am, int always)
{
  FILE *f;
  char *data, *s;
  size_t len, i, n, N;
  auth_passwd_t *apw;

  if (!am->am_stat)
    am->am_stat = su_zalloc(am->am_home, sizeof (*am->am_stat));

  f = fopen(am->am_db, "rb");

  if (f) {
    void *buffer = NULL;
    auth_passwd_t *fresh = NULL;

#if HAVE_FLOCK
    int locked;

    /* Obtain shared lock on the database file */
    if (flock(fileno(f), LOCK_SH | (always ? 0 : LOCK_NB)) == 0) {
      locked = 1;
    } else {
      locked = 0;

      if (errno == ENOLCK) {
	;
      }
      else if (errno == EWOULDBLOCK) {
	SU_DEBUG_3(("auth(%s): user file \"%s\" is busy, trying again later\n",
		    am->am_scheme->asch_method, am->am_db));
	fclose(f);
	return always ? -1 : 0;
      }
      else {
	SU_DEBUG_3(("auth(%s): flock(\"%s\"): %s (%u)\n",
		    am->am_scheme->asch_method, am->am_db,
		    strerror(errno), errno));
	fclose(f);
	return always ? -1 : 0;
      }
    }
#endif
    if (am->am_stat)
      stat(am->am_db, am->am_stat); /* too bad if this fails */

    len = readfile(am->am_home, f, &buffer, 1);

#if HAVE_FLOCK
    /* Release shared lock on the database file */
    if (locked && flock(fileno(f), LOCK_UN) == -1) {
      SU_DEBUG_0(("auth(%s): un-flock(\"%s\"): %s (%u)\n",
		  am->am_scheme->asch_method, am->am_db, strerror(errno), errno));
      fclose(f);
      return -1;
    }
#endif

    fclose(f);

    if (len < 0)
      return -1;

    /* Count number of entries in new buffer */
    for (i = am->am_anonymous, s = data = buffer;
	 s < data + len;
	 s += n + strspn(s + n, "\r\n")) {
      n = strcspn(s, "\r\n");
      if (*s != '#' && *s != '\n' && *s != '\r')
	i++;
    }

    N = i, i = 0;

    if (N > 0) {
      size_t size = (N * 5 + 3) / 4;
      if (auth_htable_resize(am->am_home, am->am_users, size) < 0 ||
	  !(fresh = su_zalloc(am->am_home, sizeof(*fresh) * N))) {
	su_free(am->am_home, buffer);
	return -1;
      }
    }

    if (am->am_anonymous) {
      assert(i < N);

      apw = fresh + i++;

      apw->apw_index = msg_hash_string("anonymous");
      apw->apw_user = "anonymous";
      apw->apw_pass = "";
      apw->apw_realm = "";

      am->am_anon_user = apw;

      if (auth_htable_is_full(am->am_users))
	auth_htable_resize(am->am_home, am->am_users, 0);

      auth_htable_append_local(am->am_users, apw);
    }

    apw = NULL;

    for (data = buffer, s = data;
	 s < data + len && i < N;
	 s += n + strspn(s + n, "\r\n")) {
      char *user, *pass, *realm, *ident;

      n = strcspn(s, "\r\n");
      if (*s == '#')
	continue;

      user = s;
      s[n++] = '\0';
      if (!(pass = strchr(user, ':')))
	continue;

      *pass++ = '\0';
      if (!*pass || !*user)
	continue;

      if ((realm = strchr(pass, ':')))
	*realm++ = '\0';
      else
	realm = "";

      if ((ident = strchr(realm, ':')))
	*ident++ = '\0';
      else
	ident = "";

      apw = fresh + i++;

      apw->apw_index = msg_hash_string(user);
      apw->apw_user = user;
      apw->apw_ident = ident;

      /* Check for htdigest format */
      if (span_hexdigit(realm) == 32 && realm[32] == '\0') {
	apw->apw_realm = pass;
	apw->apw_hash = realm;
      } else {
	apw->apw_pass = pass;
	apw->apw_realm = realm;
      }

      if (auth_htable_is_full(am->am_users))
	auth_htable_resize(am->am_home, am->am_users, 0);

      auth_htable_append_local(am->am_users, apw);
    }

    assert(i <= N);
    N = i;

    /* Remove from hash those entries that were read from old passwd file */
    for (i = 0; i < am->am_local_count; i++) {
      if (am->am_locals[i].apw_type == auth_apw_local)
	auth_htable_remove(am->am_users, &am->am_locals[i]);
    }

    if (am->am_locals)
      su_free(am->am_home, am->am_locals); /* Free old entries */
    if (am->am_buffer)
      su_free(am->am_home, am->am_buffer); /* Free old passwd file contents */

    SU_DEBUG_5(("auth(%s): read %u entries from \"%s\"\n",
		am->am_scheme->asch_method, (unsigned)N, am->am_db));

    am->am_locals = fresh;
    am->am_local_count = N;
    am->am_buffer = buffer;

    return 0;
  }

  return -1;
}

/** Append to hash, remove existing local user */
su_inline void
auth_htable_append_local(auth_htable_t *aht, auth_passwd_t *apw)
{
  auth_passwd_t **slot;

  apw->apw_type = auth_apw_local;

  /* Append to hash */
  for (slot = auth_htable_hash(aht, apw->apw_index);
       *slot;
       slot = auth_htable_next(aht, slot)) {
    if (strcmp((*slot)->apw_user, apw->apw_user) == 0) {
      if ((*slot)->apw_type == auth_apw_local) {
	(*slot)->apw_type = NULL;
	assert(aht->aht_used > 0); aht->aht_used--;
	apw->apw_extended = (*slot)->apw_extended;
	*slot = NULL;
	break;
      }
      else {
	/* We insert local before external entry */
	auth_passwd_t *swap = apw;
	apw = *slot;
	*slot = swap;
      }
    }
  }

  aht->aht_used++; assert(aht->aht_used <= aht->aht_size);

  *slot = apw;
}

static
ssize_t readfile(su_home_t *home,
		 FILE *f,
		 void **contents,
		 int add_trailing_lf)
{
  /* Read in whole (binary!) file */
  char *buffer = NULL;
  long size;
  size_t len;

  /* Read whole file in */
  if (fseek(f, 0, SEEK_END) < 0 ||
      (size = ftell(f)) < 0 ||
      fseek(f, 0, SEEK_SET) < 0 ||
      (long)(len = (size_t)size) != size ||
      size + 2 > SSIZE_MAX) {
    SU_DEBUG_1(("%s: unable to determine file size (%s)\n",
		__func__, strerror(errno)));
    return -1;
  }

  if (!(buffer = su_alloc(home, len + 2)) ||
      fread(buffer, 1, len, f) != len) {
    SU_DEBUG_1(("%s: unable to read file (%s)\n", __func__, strerror(errno)));
    if (buffer)
      su_free(home, buffer);
    return -1;
  }

  if (add_trailing_lf) {
    /* Make sure that the buffer has trailing newline */
    if (len == 0 || buffer[len - 1] != '\n')
      buffer[len++] = '\n';
  }

  buffer[len] = '\0';
  *contents = buffer;

  return len;
}

/* ====================================================================== */
/* Helper functions */

/** Check if request method is on always-allowed list.
 *
 * @return 0 if allowed
 * @return 1 otherwise
 */
int auth_allow_check(auth_mod_t *am, auth_status_t *as)
{
  char const *method = as->as_method;
  int i;

  if (method && strcmp(method, "ACK") == 0) /* Hack */
    return as->as_status = 0;

  if (!method || !am->am_allow)
    return 1;

  if (am->am_allow[0] && strcmp(am->am_allow[0], "*") == 0)
    return as->as_status = 0;

  for (i = 0; am->am_allow[i]; i++)
    if (strcmp(am->am_allow[i], method) == 0)
      return as->as_status = 0;

  return 1;
}

/** Find a credential header with matching scheme and realm. */
msg_auth_t *auth_mod_credentials(msg_auth_t *auth,
				 char const *scheme,
				 char const *realm)
{
  char const *arealm;

  for (;auth; auth = auth->au_next) {
    if (!su_casematch(auth->au_scheme, scheme))
      continue;

    if (!realm)
      return auth;

    arealm = msg_header_find_param(auth->au_common, "realm=");

    if (!arealm)
      continue;

    if (arealm[0] == '"') {
      /* Compare quoted arealm with unquoted realm */
      int i, j;
      for (i = 1, j = 0; arealm[i] != 0; i++, j++) {
	if (arealm[i] == '"' && realm[j] == 0)
	  return auth;

	if (arealm[i] == '\\' && arealm[i + 1] != '\0')
	  i++;

	if (arealm[i] != realm[j])
	  break;
      }
    } else {
      if (strcmp(arealm, realm) == 0)
	return auth;
    }
  }

  return NULL;
}

/** Find a Digest credential header with matching realm and opaque. */
msg_auth_t *auth_digest_credentials(msg_auth_t *auth,
				    char const *realm,
				    char const *opaque)
{
  char const *arealm, *aopaque;

  for (;auth; auth = auth->au_next) {
    if (!su_casematch(auth->au_scheme, "Digest"))
      continue;

    if (realm) {
      int cmp = 1;

      arealm = msg_header_find_param(auth->au_common, "realm=");
      if (!arealm)
	continue;

      if (arealm[0] == '"') {
	/* Compare quoted arealm with unquoted realm */
	int i, j;
	for (i = 1, j = 0, cmp = 1; arealm[i] != 0; i++, j++) {
	  if (arealm[i] == '"' && realm[j] == 0) {
	    cmp = 0;
	    break;
	  }

	  if (arealm[i] == '\\' && arealm[i + 1] != '\0')
	    i++;

	  if (arealm[i] != realm[j])
	    break;
	}
      }
      else {
	cmp = strcmp(arealm, realm);
      }

      if (cmp)
	continue;
    }

    if (opaque) {
      int cmp = 1;

      aopaque = msg_header_find_param(auth->au_common, "opaque=");
      if (!aopaque)
	continue;

      if (aopaque[0] == '"') {
	/* Compare quoted aopaque with unquoted opaque */
	int i, j;
	for (i = 1, j = 0, cmp = 1; aopaque[i] != 0; i++, j++) {
	  if (aopaque[i] == '"' && opaque[j] == 0) {
	    cmp = 0;
	    break;
	  }

	  if (aopaque[i] == '\\' && aopaque[i + 1] != '\0')
	    i++;

	  if (aopaque[i] != opaque[j])
	    break;
	}
      } else {
	cmp = strcmp(aopaque, opaque);
      }

      if (cmp)
	continue;
    }

    return auth;
  }

  return NULL;
}

/** Generate nonce parameter.
 *
 * @param am pointer to authentication module object
 * @param buffer string buffer for nonce [OUT]
 * @param bsize size of buffer [IN]
 * @param nextnonce true if this is a "nextnonce" [IN]
 * @param now  current time [IN]
 */
isize_t auth_generate_digest_nonce(auth_mod_t *am,
				   char buffer[],
				   size_t bsize,
				   int nextnonce,
				   msg_time_t now)
{
  struct nonce nonce[1] = {{ 0 }};
  su_md5_t md5[1];

  am->am_count += 3730029547U;	/* 3730029547 is a prime */

  nonce->issued = now;
  nonce->count = am->am_count;
  nonce->nextnonce = nextnonce != 0;

  /* Calculate HMAC of nonce data */
  auth_md5_hmac_init(am, md5);
  su_md5_update(md5, nonce, offsetof(struct nonce, digest));
  auth_md5_hmac_digest(am, md5, nonce->digest, sizeof nonce->digest);

  return base64_e(buffer, bsize, nonce, sizeof(nonce));
}


/** Validate nonce parameter.
 *
 * @param am   pointer to authentication module object
 * @param as   authentication status structure [OUT]
 * @param ar   decoded authentication response from client [IN]
 * @param now  current time [IN]
 */
int auth_validate_digest_nonce(auth_mod_t *am,
			       auth_status_t *as,
			       auth_response_t *ar,
			       msg_time_t now)
{
  struct nonce nonce[1] = {{ 0 }};
  su_md5_t md5[1];
  uint8_t hmac[sizeof nonce->digest];
  unsigned expires;

  /* Check nonce */
  if (!ar->ar_nonce) {
    SU_DEBUG_5(("auth_method_digest: no nonce\n"));
    return -1;
  }
  if (base64_d((void*)nonce, (sizeof nonce), ar->ar_nonce) != (sizeof nonce)) {
    SU_DEBUG_5(("auth_method_digest: too short nonce\n"));
    return -1;
  }

  /* Calculate HMAC over decoded nonce data */
  auth_md5_hmac_init(am, md5);
  su_md5_update(md5, nonce, offsetof(struct nonce, digest));
  auth_md5_hmac_digest(am, md5, hmac, sizeof hmac);

  if (memcmp(nonce->digest, hmac, sizeof nonce->digest)) {
    SU_DEBUG_5(("auth_method_digest: bad nonce\n"));
    return -1;
  }

  as->as_nonce_issued = nonce->issued;
  as->as_nextnonce = nonce->nextnonce != 0;

  expires = nonce->nextnonce ? am->am_next_exp : am->am_expires;

  if (nonce->issued > now ||
      (expires && nonce->issued + expires < now)) {
    SU_DEBUG_5(("auth_method_digest: nonce expired %lu seconds ago "
		"(lifetime %u)\n",
		now - (nonce->issued + expires), expires));
    as->as_stale = 1;
  }

  if (am->am_max_ncount && ar->ar_nc) {
    unsigned long nc = strtoul(ar->ar_nc, NULL, 10);

    if (nc == 0 || nc > am->am_max_ncount) {
      SU_DEBUG_5(("auth_method_digest: nonce used %s times, max %u\n",
		  ar->ar_nc, am->am_max_ncount));
      as->as_stale = 1;
    }
  }

  /* We should also check cnonce, nc... */

  return 0;
}


/* ====================================================================== */
/* HMAC routines */
static
void auth_md5_hmac_key(auth_mod_t *am)
{
  size_t i;
  uint8_t ipad[SU_MD5_DIGEST_SIZE];
  uint8_t opad[SU_MD5_DIGEST_SIZE];

  assert(SU_MD5_DIGEST_SIZE == sizeof am->am_master_key);

  /* Derive HMAC ipad and opad from master key */
  for (i = 0; i < sizeof am->am_master_key; i++) {
    ipad[i] = am->am_master_key[i] ^ 0x36;
    opad[i] = am->am_master_key[i] ^ 0x5C;
  }

  /* Pre-calculate sum of ipad */
  su_md5_init(&am->am_hmac_ipad);
  su_md5_update(&am->am_hmac_ipad, ipad, sizeof ipad);

  /* Pre-calculate sum of opad */
  su_md5_init(&am->am_hmac_opad);
  su_md5_update(&am->am_hmac_opad, opad, sizeof opad);
}

void auth_md5_hmac_init(auth_mod_t *am, struct su_md5_t *imd5)
{
  *imd5 = am->am_hmac_ipad;
}

void auth_md5_hmac_digest(auth_mod_t *am, struct su_md5_t *imd5,
			  void *hmac, size_t size)
{
  uint8_t digest[SU_MD5_DIGEST_SIZE];
  su_md5_t omd5[1];

  /* inner sum */
  su_md5_digest(imd5, digest);

  *omd5 = am->am_hmac_opad;
  su_md5_update(omd5, digest, sizeof *digest);

  /* outer sum */
  if (size == sizeof digest) {
    su_md5_digest(omd5, hmac);
  }
  else {
    su_md5_digest(omd5, digest);

    if (size > sizeof digest) {
      memset((char *)hmac + (sizeof digest), 0, size - sizeof digest);
      size = sizeof digest;
    }

    memcpy(hmac, digest, size);
  }
}

/* ====================================================================== */
/* Compatibility interface */

void auth_mod_method(auth_mod_t *am,
		     auth_status_t *as,
		     msg_auth_t *credentials,
		     auth_challenger_t const *ach)
{
  auth_mod_verify(am, as, credentials, ach);
}

void auth_mod_check_client(auth_mod_t *am,
			   auth_status_t *as,
			   msg_auth_t *credentials,
			   auth_challenger_t const *ach)
{
  auth_mod_verify(am, as, credentials, ach);
}


void auth_mod_challenge_client(auth_mod_t *am,
			       auth_status_t *as,
			       auth_challenger_t const *ach)
{
  auth_mod_challenge(am, as, ach);
}
