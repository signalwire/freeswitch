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

#ifndef AUTH_MODULE_H
/** Defined when <sofia-sip/auth_module.h> has been included. */
#define AUTH_MODULE_H

/**@file sofia-sip/auth_module.h
 * @brief Authentication verification interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Mon Jul 23 19:21:24 2001 ppessi
 */

#ifndef SU_TAG_H
#include <sofia-sip/su_tag.h>
#endif
#ifndef SU_WAIT_H
#include <sofia-sip/su_wait.h>
#endif
#ifndef MSG_TYPES_H
#include <sofia-sip/msg_types.h>
#endif
#ifndef URL_H
#include <sofia-sip/url.h>
#endif
#ifndef URL_TAG_H
#include <sofia-sip/url_tag.h>
#endif

SOFIA_BEGIN_DECLS

typedef struct auth_mod_t auth_mod_t;
/** Authentication operation. */
typedef struct auth_status_t auth_status_t;

#ifdef  AUTH_MAGIC_T
typedef AUTH_MAGIC_T auth_magic_t;
#else
typedef void auth_magic_t;
#endif

/** Virtual table for authentication plugin. */
typedef struct auth_scheme const auth_scheme_t;

/** Opaque data used by authentication plugin module. */
typedef struct auth_plugin_t  auth_plugin_t;
/** Opaque user data used by plugin module. */
typedef struct auth_splugin_t auth_splugin_t;
/** Opaque authentication operation data used by plugin module. */
typedef struct auth_uplugin_t auth_uplugin_t;

/** Callback from completeted asynchronous authentication operation. */
typedef void auth_callback_t(auth_magic_t *, auth_status_t *);

/**Authentication operation result.
 *
 * The auth_status_t structure is used to store the status of the
 * authentication operation and all the related data. The application
 * verifying the authentication fills the auth_status_t structure, then
 * calls auth_mod_method() (or auth_mod_challenge()). The operation result
 * is stored in the structure.
 *
 * If the operation is asynchronous, only a preliminary result is stored in
 * the auth_status_t structure when the call to auth_mod_method() returns.
 * In that case, the application @b must assign a callback function to the
 * structure. The callback function is invoked when the authentication
 * operation is completed.
 *
 * It is recommended that the auth_status_t structure is allocated with
 * auth_status_new() or initialized with auth_status_init() or
 * auth_status_init_with() functions.
 */
struct auth_status_t
{
  su_home_t       as_home[1];	/**< Memory home for authentication */

  int          	  as_status;	/**< Return authorization status [out] */
  char const   	 *as_phrase;	/**< Return response phrase [out] */
  char const   	 *as_user;	/**< Authenticated username [in/out] */
  char const   	 *as_display;	/**< Return user's real name [in/out] */

  url_t const    *as_user_uri;	/* Return user's identity [in/out] */
  char const     *as_ident;	/**< Identities [out] */
  unsigned        as_profile;	/**< User profile (group) [out] */

  su_addrinfo_t  *as_source;	/**< Source address [in] */

  char const   	 *as_realm;	/**< Authentication realm [in] */
  char const  	 *as_domain;	/**< Hostname [in] */
  char const  	 *as_uri;	/**< Request-URI [in] */
  char const     *as_pdomain;	/**< Domain parameter [in] (ignored). */
  char const   	 *as_method;	/**< Method name to authenticate [in] */

  void const   	 *as_body;	/**< Message body to protect [in] */
  isize_t      	  as_bodylen;	/**< Length of message body [in] */

  msg_time_t      as_nonce_issued; /**< Nonce issue time [out] */
  unsigned   	  as_blacklist; /**< Blacklist time [out] */
  unsigned        as_anonymous:1;/**< Return true if user is anonymous [out] */
  unsigned        as_stale:1;	/**< Credentials were stale [out] */
  unsigned        as_allow:1;	/**< Method cannot be challenged [out] */
  unsigned        as_nextnonce:1; /**< Client used nextnonce [out] */
  unsigned :0;

  msg_header_t 	 *as_response;	/**< Authentication challenge [out] */
  msg_header_t   *as_info;	/**< Authentication-Info [out] */
  msg_header_t 	 *as_match;	/**< Used authentication header [out] */

  /** @defgroup Callback information for asynchronous operation.  */
  /** @{ */
  auth_magic_t   *as_magic;	/**< Application data [in] */
  auth_callback_t*as_callback;	/**< Completion callback [in] */
  /** @} */

  /** Pointer to extended state, used exclusively by plugin modules. */
  auth_splugin_t *as_plugin;
};

/** Authentication challenge.
 *
 * This structure defines what kind of response and challenge header is
 * returned to the user. For example, a server authentication is implemented
 * with 401 response code and phrase along with header class for
 * @b WWW-Authenticate header in the @a ach structure.
 */
typedef struct auth_challenger
{
  int           ach_status;	/**< Response status for challenge response */
  char const   *ach_phrase;	/**< Response phrase for challenge response */
  msg_hclass_t *ach_header;	/**< Header class for challenge header */
  msg_hclass_t *ach_info;
} auth_challenger_t;

SOFIAPUBVAR char const auth_internal_server_error[];

#define AUTH_STATUS_INIT \
  {{ SU_HOME_INIT(auth_status_t) }, 500, auth_internal_server_error, NULL }

#define AUTH_STATUS_DEINIT(as) \
  su_home_deinit(as->as_home)

#define AUTH_RESPONSE_INIT(as) AUTH_STATUS_INIT
#define AUTH_RESPONSE_DEINIT(as) AUTH_STATUS_DEINIT(as)

SOFIAPUBFUN int auth_mod_register_plugin(auth_scheme_t *asch);

SOFIAPUBFUN auth_mod_t *auth_mod_create(su_root_t *root,
					tag_type_t, tag_value_t, ...);
SOFIAPUBFUN void auth_mod_destroy(auth_mod_t *);

SOFIAPUBFUN auth_mod_t *auth_mod_ref(auth_mod_t *am);
SOFIAPUBFUN void auth_mod_unref(auth_mod_t *am);

SOFIAPUBFUN char const *auth_mod_name(auth_mod_t *am);

SOFIAPUBFUN auth_status_t *auth_status_init(void *, isize_t size);
SOFIAPUBFUN auth_status_t *auth_status_init_with(void *, isize_t size,
						 int status,
						 char const *phrase);

SOFIAPUBFUN auth_status_t *auth_status_new(su_home_t *);

SOFIAPUBFUN auth_status_t *auth_status_ref(auth_status_t *as);

SOFIAPUBFUN void auth_status_unref(auth_status_t *as);

SOFIAPUBFUN void auth_mod_verify(auth_mod_t *am,
				 auth_status_t *as,
				 msg_auth_t *credentials,
				 auth_challenger_t const *ach);

SOFIAPUBFUN void auth_mod_challenge(auth_mod_t *am,
				    auth_status_t *as,
				    auth_challenger_t const *ach);

SOFIAPUBFUN void auth_mod_authorize(auth_mod_t *am,
				    auth_status_t *as,
				    auth_challenger_t const *ach);

SOFIAPUBFUN void auth_mod_cancel(auth_mod_t *am, auth_status_t *as);

/* ====================================================================== */
/* Deprecated functions */

typedef enum {
  auth_server,
  auth_proxy,
  auth_proxy_consume,
  auth_consume
} auth_kind_t;

SOFIAPUBFUN void auth_mod_method(auth_mod_t *am,
				 auth_status_t *as,
				 msg_auth_t *credentials,
				 auth_challenger_t const *ach);

SOFIAPUBFUN void auth_mod_check_client(auth_mod_t *am,
				       auth_status_t *as,
				       msg_auth_t *credentials,
				       auth_challenger_t const *ach);

SOFIAPUBFUN void auth_mod_challenge_client(auth_mod_t *am,
					   auth_status_t *as,
					   auth_challenger_t const *ach);

#ifdef SIP_H
SOFIAPUBFUN void auth_mod_check(auth_mod_t *am,
				auth_status_t *as,
				sip_t const *sip,
				auth_kind_t proxy);
#endif

#ifdef HTTP_H
SOFIAPUBFUN const char *auth_mod_check_http(auth_mod_t *am,
					    auth_status_t *as,
					    http_t const *http,
					    auth_kind_t proxy);
#endif

/* ====================================================================== */
/* Tags */

#define AUTHTAG_ANY()         authtag_any, ((tag_value_t)0)
SOFIAPUBVAR tag_typedef_t authtag_any;

/** Pointer to an authentication server (auth_mod_t). */
#define AUTHTAG_MODULE(x)	authtag_module, authtag_module_v((x))
SOFIAPUBVAR tag_typedef_t authtag_module;

#define AUTHTAG_MODULE_REF(x)	authtag_module_ref, authtag_module_vr((&x))
SOFIAPUBVAR tag_typedef_t authtag_module_ref;

#if SU_INLINE_TAG_CAST
su_inline tag_value_t authtag_module_v(auth_mod_t *v) {
  return (tag_value_t)v;
}
su_inline tag_value_t authtag_module_vr(auth_mod_t **vp) {
  return (tag_value_t)vp;
}
#else
#define authtag_module_v(v)   ((tag_value_t)(v))
#define authtag_module_vr(v)  ((tag_value_t)(v))
#endif

/** Authentication scheme used by authentication module. */
#define AUTHTAG_METHOD(x)	authtag_method, tag_str_v((x))
SOFIAPUBVAR tag_typedef_t authtag_method;

#define AUTHTAG_METHOD_REF(x)	authtag_method_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t authtag_method_ref;

/** Authentication realm used by authentication server. */
#define AUTHTAG_REALM(x)	authtag_realm, tag_str_v((x))
SOFIAPUBVAR tag_typedef_t authtag_realm;

#define AUTHTAG_REALM_REF(x)	authtag_realm_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t authtag_realm_ref;

/** Opaque authentication data always included in challenge. */
#define AUTHTAG_OPAQUE(x)	authtag_opaque, tag_str_v((x))
SOFIAPUBVAR tag_typedef_t authtag_opaque;

#define AUTHTAG_OPAQUE_REF(x)	authtag_opaque_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t authtag_opaque_ref;

/** Name of authentication database used by authentication server. */
#define AUTHTAG_DB(x)		authtag_db, tag_str_v((x))
SOFIAPUBVAR tag_typedef_t authtag_db;

#define AUTHTAG_DB_REF(x)		authtag_db_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t authtag_db_ref;

/** Quality-of-protection used by digest authentication. */
#define AUTHTAG_QOP(x)	        authtag_qop, tag_str_v((x))
SOFIAPUBVAR tag_typedef_t authtag_qop;

#define AUTHTAG_QOP_REF(x)	        authtag_qop_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t authtag_qop_ref;

/** Algorithm used by digest authentication. */
#define AUTHTAG_ALGORITHM(x)    authtag_algorithm, tag_str_v((x))
SOFIAPUBVAR tag_typedef_t authtag_algorithm;

#define AUTHTAG_ALGORITHM_REF(x)    authtag_algorithm_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t authtag_algorithm_ref;

/** Nonce lifetime. */
#define AUTHTAG_EXPIRES(x)    authtag_expires, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t authtag_expires;

#define AUTHTAG_EXPIRES_REF(x)    authtag_expires_ref, tag_uint_vr((&x))
SOFIAPUBVAR tag_typedef_t authtag_expires_ref;

/** Lifetime for nextnonce, 0 disables nextnonce. */
#define AUTHTAG_NEXT_EXPIRES(x)    authtag_next_expires, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t authtag_next_expires;

#define AUTHTAG_NEXT_EXPIRES_REF(x)  \
  authtag_next_expires_ref, tag_uint_vr((&x))
SOFIAPUBVAR tag_typedef_t authtag_next_expires_ref;

/** Maximum nonce count allowed. */
#define AUTHTAG_MAX_NCOUNT(x)    authtag_max_ncount, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t authtag_max_ncount;

#define AUTHTAG_MAX_NCOUNT_REF(x)    authtag_max_ncount_ref, tag_uint_vr((&x))
SOFIAPUBVAR tag_typedef_t authtag_max_ncount_ref;

/** Extra delay when responding if provided invalid credentials or nonce. */
#define AUTHTAG_BLACKLIST(x)    authtag_blacklist, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t authtag_blacklist;

#define AUTHTAG_BLACKLIST_REF(x)    authtag_blacklist_ref, tag_uint_vr((&x))
SOFIAPUBVAR tag_typedef_t authtag_blacklist_ref;

/** Respond with 403 Forbidden if given invalid credentials. */
#define AUTHTAG_FORBIDDEN(x)    authtag_forbidden, tag_bool_v((x))
SOFIAPUBVAR tag_typedef_t authtag_forbidden;

#define AUTHTAG_FORBIDDEN_REF(x)    authtag_forbidden_ref, tag_bool_vr((&x))
SOFIAPUBVAR tag_typedef_t authtag_forbidden_ref;

/** Allow anonymous access. */
#define AUTHTAG_ANONYMOUS(x)    authtag_anonymous, tag_bool_v((x))
SOFIAPUBVAR tag_typedef_t authtag_anonymous;

#define AUTHTAG_ANONYMOUS_REF(x)    authtag_anonymous_ref, tag_bool_vr((&x))
SOFIAPUBVAR tag_typedef_t authtag_anonymous_ref;

/** HSS client structure. */
#define AUTHTAG_HSS(x)        authtag_hss, tag_ptr_v((x))
SOFIAPUBVAR tag_typedef_t authtag_hss;

#define AUTHTAG_HSS_REF(x)    authtag_hss_ref, tag_ptr_vr((&x), (x))
SOFIAPUBVAR tag_typedef_t authtag_hss_ref;

/** Remote authenticator URL. */
#define AUTHTAG_REMOTE(x)     authtag_remote, urltag_url_v((x))
SOFIAPUBVAR tag_typedef_t authtag_remote;

#define AUTHTAG_REMOTE_REF(x) authtag_remote_ref, urltag_url_vr((&x))
SOFIAPUBVAR tag_typedef_t authtag_remote_ref;

/** Comma-separated list of methods never challenged. */
#define AUTHTAG_ALLOW(x)      authtag_allow, tag_str_v((x))
SOFIAPUBVAR tag_typedef_t authtag_allow;

#define AUTHTAG_ALLOW_REF(x)  authtag_allow_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t authtag_allow_ref;

/** Check that user exists, don't do authentication. */
#define AUTHTAG_FAKE(x)	authtag_fake, tag_bool_v((x))
SOFIAPUBVAR tag_typedef_t authtag_fake;

#define AUTHTAG_FAKE_REF(x) authtag_fake_ref, tag_bool_vr((&x))
SOFIAPUBVAR tag_typedef_t authtag_fake_ref;

/** Master key in base64 for the authentication module. */
#define AUTHTAG_MASTER_KEY(x)	authtag_master_key, tag_str_v((x))
SOFIAPUBVAR tag_typedef_t authtag_master_key;

#define AUTHTAG_MASTER_KEY_REF(x) authtag_master_key_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t authtag_master_key_ref;

/** Cache time for authentication data. */
#define AUTHTAG_CACHE_USERS(x)	authtag_cache_users, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t authtag_cache_users;

#define AUTHTAG_CACHE_USERS_REF(x) authtag_cache_users_ref, tag_uint_vr((&x))
SOFIAPUBVAR tag_typedef_t authtag_cache_users_ref;

/** Cache time for errors. */
#define AUTHTAG_CACHE_ERRORS(x)	authtag_cache_errors, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t authtag_cache_errors;

#define AUTHTAG_CACHE_ERRORS_REF(x) authtag_cache_errors_ref, tag_uint_vr((&x))
SOFIAPUBVAR tag_typedef_t authtag_cache_errors_ref;

SOFIA_END_DECLS

#endif
