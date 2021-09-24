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

#ifndef AUTH_PLUGIN_H
/** Defined when <sofia-sip/auth_plugin.h> has been included. */
#define AUTH_PLUGIN_H

/**@file sofia-sip/auth_plugin.h
 * @brief Plugin interface for authentication verification modules.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Apr 27 15:22:07 2004 ppessi
 */

#ifndef AUTH_MODULE_H
#include "sofia-sip/auth_module.h"
#endif

#ifndef AUTH_DIGEST_H
#include "sofia-sip/auth_digest.h"
#endif

#ifndef AUTH_COMMON_H
#include "sofia-sip/auth_common.h"
#endif

#ifndef MSG_DATE_H
#include <sofia-sip/msg_date.h>
#endif

#ifndef SU_MD5_H
#include <sofia-sip/su_md5.h>
#endif

#include <sofia-sip/htable.h>

SOFIA_BEGIN_DECLS

/* ====================================================================== */
/* Plugin interface for authentication */

/** Authentication scheme */
struct auth_scheme
{
  /** Name */
  char const *asch_method;

  /** Size of module object */
  usize_t asch_size;

  /** Initialize module. Invoked by auth_mod_create(). */
  int (*asch_init)(auth_mod_t *am,
		   auth_scheme_t *base,
		   su_root_t *root,
		   tag_type_t tag, tag_value_t value, ...);

  /** Check authentication. Invoked by auth_mod_method(). */
  void (*asch_check)(auth_mod_t *am,
		     auth_status_t *as,
		     msg_auth_t *auth,
		     auth_challenger_t const *ch);

  /** Create a challenge. Invoked by auth_mod_challenge(). */
  void (*asch_challenge)(auth_mod_t *am,
			 auth_status_t *as,
			 auth_challenger_t const *ch);

  /** Cancel an asynchronous authentication request.
   * Invoked by auth_mod_cancel().
   */
  void (*asch_cancel)(auth_mod_t *am,
		      auth_status_t *as);

  /** Reclaim resources an authentication module.
   *
   * Invoked by auth_mod_destroy()/auth_mod_unref().
   */
  void (*asch_destroy)(auth_mod_t *am);

};

/** User data structure */
typedef struct
{
  unsigned        apw_index;	/**< Key to hash table */
  void const     *apw_type;	/**< Magic identifier */

  char const   	 *apw_user;	/**< Username */
  char const     *apw_realm;	/**< Realm */
  char const   	 *apw_pass;	/**< Password */
  char const     *apw_hash;	/**< MD5 of the username, realm and pass */
  char const     *apw_ident;	/**< Identity information */
  auth_uplugin_t *apw_extended;	/**< Method-specific extension */
} auth_passwd_t;


HTABLE_DECLARE_WITH(auth_htable, aht, auth_passwd_t, usize_t, unsigned);

struct stat;

/** Common data for authentication module */
struct auth_mod_t
{
  su_home_t      am_home[1];
  unsigned       _am_refcount;	/**< Not used */

  /* User database / cache */
  char const    *am_db;		/**< User database file name */
  struct stat   *am_stat;	/**< State of user file when read */
  auth_htable_t  am_users[1];	/**< Table of users */

  void          *am_buffer;	/**< Buffer for database */
  auth_passwd_t *am_locals;	/**< Entries from local user file */
  size_t         am_local_count; /**< Number of entries from local user file */

  auth_passwd_t *am_anon_user;	/**< Special entry for anonymous user */

  /* Attributes */
  url_t         *am_remote;	/**< Remote authenticator */
  char const    *am_realm;	/**< Our realm */
  char const    *am_opaque;	/**< Opaque identification data */
  char const    *am_gssapi_data; /**< NTLM data */
  char const    *am_targetname; /**< NTLM target name */
  auth_scheme_t *am_scheme;	/**< Authentication scheme (Digest, Basic). */
  char const   **am_allow;	/**< Methods to allow without authentication */
  msg_param_t    am_algorithm;	/**< Defauilt algorithm */
  msg_param_t    am_qop;	/**< Default qop (quality-of-protection) */
  unsigned       am_expires;	/**< Nonce lifetime */
  unsigned       am_next_exp;	/**< Next nonce lifetime */
  unsigned       am_blacklist;	/**< Extra delay if bad credentials. */
  unsigned       am_forbidden:1;/**< Respond with 403 if bad credentials */
  unsigned       am_anonymous:1;/**< Allow anonymous access */
  unsigned       am_challenge:1;/**< Challenge even if successful */
  unsigned       am_nextnonce:1;/**< Send next nonce in responses */
  unsigned       am_mutual:1;   /**< Mutual authentication */
  unsigned       am_fake:1;	/**< Fake authentication */

  unsigned :0;			/**< Pad */
  unsigned       am_count;	/**< Nonce counter */

  uint8_t        am_master_key[16]; /**< Private master key */

  su_md5_t       am_hmac_ipad;	/**< MD5 with inner pad */
  su_md5_t       am_hmac_opad;	/**< MD5 with outer pad */

  unsigned       am_max_ncount:1; /**< If nonzero, challenge with new nonce after ncount */
};

SOFIAPUBFUN
auth_passwd_t *auth_mod_getpass(auth_mod_t *am,
				char const *user,
				char const *realm);

SOFIAPUBFUN
auth_passwd_t *auth_mod_addpass(auth_mod_t *am,
				char const *user,
				char const *realm);

SOFIAPUBFUN int auth_readdb_if_needed(auth_mod_t *am);

SOFIAPUBFUN int auth_readdb(auth_mod_t *am);

SOFIAPUBFUN msg_auth_t *auth_mod_credentials(msg_auth_t *auth,
					     char const *scheme,
					     char const *realm);

SOFIAPUBFUN auth_mod_t *auth_mod_alloc(auth_scheme_t *scheme,
				       tag_type_t, tag_value_t, ...);

#define AUTH_PLUGIN(am) (auth_plugin_t *)((am) + 1)

SOFIAPUBFUN
int auth_init_default(auth_mod_t *am,
		      auth_scheme_t *base,
		      su_root_t *root,
		      tag_type_t tag, tag_value_t value, ...);

/** Default cancel method */
SOFIAPUBFUN void auth_cancel_default(auth_mod_t *am, auth_status_t *as);

/** Default destroy method */
SOFIAPUBFUN void auth_destroy_default(auth_mod_t *am);

/** Basic scheme */
SOFIAPUBFUN
void auth_method_basic(auth_mod_t *am,
		       auth_status_t *as,
		       msg_auth_t *auth,
		       auth_challenger_t const *ach);

SOFIAPUBFUN
void auth_challenge_basic(auth_mod_t *am,
			  auth_status_t *as,
			  auth_challenger_t const *ach);

/** Digest scheme */
SOFIAPUBFUN
msg_auth_t *auth_digest_credentials(msg_auth_t *auth,
				    char const *realm,
				    char const *opaque);

SOFIAPUBFUN
void auth_method_digest(auth_mod_t *am,
			auth_status_t *as,
			msg_auth_t *au,
			auth_challenger_t const *ach);

SOFIAPUBFUN
void auth_info_digest(auth_mod_t *am,
		      auth_status_t *as,
		      auth_challenger_t const *ach);

SOFIAPUBFUN
void auth_check_digest(auth_mod_t *am,
		       auth_status_t *as,
		       auth_response_t *ar,
		       auth_challenger_t const *ach);

SOFIAPUBFUN
void auth_challenge_digest(auth_mod_t *am,
			   auth_status_t *as,
			   auth_challenger_t const *ach);

SOFIAPUBFUN
isize_t auth_generate_digest_nonce(auth_mod_t *am,
				   char buffer[],
				   size_t buffer_len,
				   int nextnonce,
				   msg_time_t now);

SOFIAPUBFUN
int auth_validate_digest_nonce(auth_mod_t *am,
			       auth_status_t *as,
			       auth_response_t *ar,
			       msg_time_t now);

SOFIAPUBFUN int auth_allow_check(auth_mod_t *am, auth_status_t *as);

/** Init md5 for MD5-based HMAC */
SOFIAPUBFUN void auth_md5_hmac_init(auth_mod_t *am, su_md5_t *md5);
SOFIAPUBFUN void auth_md5_hmac_digest(auth_mod_t *am, su_md5_t *md5,
				      void *hmac, size_t size);

SOFIA_END_DECLS

#endif /* !defined AUTH_PLUGIN_H */
