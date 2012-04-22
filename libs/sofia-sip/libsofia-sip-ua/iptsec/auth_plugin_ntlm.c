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

/**@CFILE auth_plugin_ntlm.c
 *
 * @brief Plugin for delayed authentication.
 *
 * This authentication plugin provides authentication operation that is
 * intentionally delayed. It serves as an example of server-side
 * authentication plugins.
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

#include <sofia-sip/su_debug.h>
#include <sofia-sip/su_wait.h>

#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_tagarg.h>

#include "sofia-sip/auth_module.h"
#include "sofia-sip/auth_plugin.h"
#include "sofia-sip/auth_ntlm.h"

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
static char const __func__[] = "auth_plugin_ntml";
#endif

/* ====================================================================== */
/* NTLM authentication scheme */

static int auth_init_ntlm(auth_mod_t *am,
			  auth_scheme_t *base,
			  su_root_t *root,
			  tag_type_t tag, tag_value_t value, ...);

static void auth_method_ntlm_x(auth_mod_t *am,
			       auth_status_t *as,
			       msg_auth_t *au,
			       auth_challenger_t const *ach);

auth_scheme_t auth_scheme_ntlm[1] =
  {{
      "NTLM",			/* asch_method */
      sizeof (auth_mod_t),	/* asch_size */
      auth_init_default,	/* asch_init */
      auth_method_ntlm_x,	/* asch_check */
      auth_challenge_ntlm,	/* asch_challenge */
      auth_cancel_default,	/* asch_cancel */
      auth_destroy_default	/* asch_destroy */
  }};

#define AUTH_NTLM_NONCE_LEN (BASE64_SIZE(sizeof (struct nonce)) + 1)

static int auth_init_ntlm(auth_mod_t *am,
			     auth_scheme_t *base,
			     su_root_t *root,
			     tag_type_t tag, tag_value_t value, ...)
{
  auth_plugin_t *ap = AUTH_PLUGIN(am);
  int retval = -1;
  ta_list ta;

  ta_start(ta, tag, value);

  if (auth_init_default(am, NULL, root, ta_tags(ta)) != -1) {
    retval = 0;
  }

  ta_end(ta);

  return retval;
}


/** Authenticate a request with @b NTLM authentication scheme.
 *
 * This function reads user database before authentication, if needed.
 */
static
void auth_method_ntlm_x(auth_mod_t *am,
			auth_status_t *as,
			msg_auth_t *au,
			auth_challenger_t const *ach)
{
  if (am) {
    auth_readdb_if_needed(am);
    auth_method_ntlm(am, as, au, ach);
  }
}

/** Authenticate a request with @b Ntlm authentication scheme.
 */
void auth_method_ntlm(auth_mod_t *am,
		      auth_status_t *as,
		      msg_auth_t *au,
		      auth_challenger_t const *ach)
{
  as->as_allow = as->as_allow || auth_allow_check(am, as) == 0;

  if (as->as_realm)
    au = auth_ntlm_credentials(au, as->as_realm, am->am_opaque,
			       am->am_gssapi_data, am->am_targetname);

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
    auth_ntlm_response_get(as->as_home, ar, au->au_params);
    as->as_match = (msg_header_t *)au;
    auth_check_ntlm(am, as, ar, ach);
  }
  else {
    /* There was no matching credentials, send challenge */
    SU_DEBUG_5(("%s: no credentials matched\n", __func__));
    auth_challenge_ntlm(am, as, ach);
  }
}


/** Find a NTLM credential header with matching realm and opaque. */
msg_auth_t *auth_ntlm_credentials(msg_auth_t *auth,
				  char const *realm,
				  char const *opaque,
				  char const *gssapidata,
				  char const *targetname)
{
  char const *agssapidata, *atargetname;

  for (;auth; auth = auth_mod_credentials(auth->au_next)) {
    if (!su_casematch(auth->au_scheme, "NTLM"))
      continue;

    if (gssapidata) {
      agssapidata = msg_header_find_param(auth->au_common, "gssapi-data=");
      if (!agssapidata || auth_strcmp(agssapidata, gssapidata))
	continue;
    }

    if (targetname) {
      atargetname = msg_header_find_param(auth->au_common, "targetname=");
      if (!atargetname || auth_strcmp(atargetname, targetname))
	continue;
    }

    return auth;
  }

  return NULL;
}


/** Check ntlm authentication */
void auth_check_ntlm(auth_mod_t *am,
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
    SU_DEBUG_5(("auth_method_ntlm: 400 %s\n", phrase));
    as->as_status = 400, as->as_phrase = phrase;
    as->as_response = NULL;
    return;
  }

  /* XXX - replace */
#if 0
  if (as->as_nonce_issued == 0 /* Already validated nonce */ &&
      auth_validate_ntlm_nonce(am, as, ar, now) < 0) {
#else
  if (as->as_nonce_issued == 0 /* Already validated nonce */ &&
      auth_validate_digest_nonce(am, as, ar, now) < 0) {
#endif
    as->as_blacklist = am->am_blacklist;
    auth_challenge_ntlm(am, as, ach);
    return;
  }

  if (as->as_stale) {
    auth_challenge_ntlm(am, as, ach);
    return;
  }

  apw = auth_mod_getpass(am, ar->ar_username, ar->ar_realm);

#if 0
  if (apw && apw->apw_hash)
    a1 = apw->apw_hash;
  else if (apw && apw->apw_pass)
    auth_ntlm_a1(ar, a1buf, apw->apw_pass), a1 = a1buf;
  else
    auth_ntlm_a1(ar, a1buf, "xyzzy"), a1 = a1buf, apw = NULL;

  if (ar->ar_md5sess)
    auth_ntlm_a1sess(ar, a1buf, a1), a1 = a1buf;
#else
  if (apw && apw->apw_hash)
    a1 = apw->apw_hash;
  else if (apw && apw->apw_pass)
    auth_digest_a1(ar, a1buf, apw->apw_pass), a1 = a1buf;
  else
    auth_digest_a1(ar, a1buf, "xyzzy"), a1 = a1buf, apw = NULL;

  if (ar->ar_md5sess)
    auth_digest_a1sess(ar, a1buf, a1), a1 = a1buf;
#endif

  /* XXX - replace with auth_ntlm_response */
#if 0
  auth_ntlm_response(ar, response, a1,
		     as->as_method, as->as_body, as->as_bodylen);
#else
  auth_digest_response(ar, response, a1,
		       as->as_method, as->as_body, as->as_bodylen);
#endif

  if (!apw || strcmp(response, ar->ar_response)) {
    if (am->am_forbidden) {
      as->as_status = 403, as->as_phrase = "Forbidden";
      as->as_blacklist = am->am_blacklist;
      as->as_response = NULL;
    }
    else {
      auth_challenge_ntlm(am, as, ach);
      as->as_blacklist = am->am_blacklist;
    }
    SU_DEBUG_5(("auth_method_ntlm: response did not match\n"));

    return;
  }

  assert(apw);

  as->as_user = apw->apw_user;
  as->as_anonymous = apw == am->am_anon_user;

  if (am->am_nextnonce || am->am_mutual)
    auth_info_ntlm(am, as, ach);

  if (am->am_challenge)
    auth_challenge_ntlm(am, as, ach);

  SU_DEBUG_7(("auth_method_ntlm: successful authentication\n"));

  as->as_status = 0;	/* Successful authentication! */
  as->as_phrase = "";
}

/** Construct a challenge header for @b Ntlm authentication scheme. */
void auth_challenge_ntlm(auth_mod_t *am,
			   auth_status_t *as,
			   auth_challenger_t const *ach)
{
  char const *u, *d;
  char nonce[AUTH_NTLM_NONCE_LEN];

#if 0
  auth_generate_ntlm_nonce(am, nonce, sizeof nonce, 0, msg_now());
#else
  auth_generate_digest_nonce(am, nonce, sizeof nonce, 0, msg_now());
#endif

  u = as->as_uri;
  d = as->as_pdomain;

  as->as_response =
    msg_header_format(as->as_home, ach->ach_header,
		      "Ntlm"
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

/** Construct a info header for @b Ntlm authentication scheme. */
void auth_info_ntlm(auth_mod_t *am,
		      auth_status_t *as,
		      auth_challenger_t const *ach)
{
  if (!ach->ach_info)
    return;

  if (am->am_nextnonce) {
    char nonce[AUTH_NTLM_NONCE_LEN];

    /* XXX - replace */
#if 0
    auth_generate_ntlm_nonce(am, nonce, sizeof nonce, 1, msg_now());
#else
    auth_generate_digest_nonce(am, nonce, sizeof nonce, 1, msg_now());
#endif

    as->as_info =
      msg_header_format(as->as_home, ach->ach_info, "nextnonce=\"%s\"", nonce);
  }
}
