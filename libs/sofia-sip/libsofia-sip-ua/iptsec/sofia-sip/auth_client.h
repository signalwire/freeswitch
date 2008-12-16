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

#ifndef AUTH_CLIENT_H
/** Defined when <sofia-sip/auth_client.h> has been included. */
#define AUTH_CLIENT_H

/**@file sofia-sip/auth_client.h
 *
 * @brief Client-side authenticator library.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Feb 14 17:09:44 2001 ppessi
 */

#ifndef MSG_TYPES_H
#include <sofia-sip/msg_types.h>
#endif

#ifndef URL_H
#include <sofia-sip/url.h>
#endif

SOFIA_BEGIN_DECLS

/** Authenticator object. */
typedef struct auth_client_s auth_client_t;

SOFIAPUBFUN
int auc_challenge(auth_client_t **auc, su_home_t *home,
		  msg_auth_t const *auth,
		  msg_hclass_t *crcl);

SOFIAPUBFUN
int auc_credentials(auth_client_t **auc, su_home_t *home, char const *data);

SOFIAPUBFUN
int auc_info(auth_client_t **auc_list,
	     msg_auth_info_t const *ai,
	     msg_hclass_t *credential_class);

SOFIAPUBFUN
int auc_all_credentials(auth_client_t **auc_list,
			char const *scheme,
			char const *realm,
			char const *user,
			char const *pass);

SOFIAPUBFUN
int auc_clear_credentials(auth_client_t **auc_list,
			  char const *scheme,
			  char const *realm);

SOFIAPUBFUN
int auc_copy_credentials(auth_client_t **dst, auth_client_t const *src);

SOFIAPUBFUN
int auc_has_authorization(auth_client_t **auc_list);

SOFIAPUBFUN
int auc_authorization(auth_client_t **auc_list, msg_t *msg, msg_pub_t *pub,
		      char const *method,
		      url_t const *url,
		      msg_payload_t const *body);

SOFIAPUBFUN
int auc_authorization_headers(auth_client_t **auc_list,
			      su_home_t *home,
			      char const *method,
			      url_t const *url,
			      msg_payload_t const *body,
			      msg_header_t **return_headers);

struct sip_s;

SOFIAPUBFUN
int auc_authorize(auth_client_t **auc, msg_t *msg, struct sip_s *sip);

typedef struct auth_client_plugin auth_client_plugin_t;

SOFIAPUBFUN
int auc_register_plugin(auth_client_plugin_t const *plugin);

SOFIA_END_DECLS

#endif
