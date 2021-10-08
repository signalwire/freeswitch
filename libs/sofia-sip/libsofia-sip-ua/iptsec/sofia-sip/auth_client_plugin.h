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

#ifndef AUTH_CLIENT_PLUGIN_H
/** Defined when <sofia-sip/auth_client_plugin.h> has been included. */
#define AUTH_CLIENT_PLUGIN_H

/**@file sofia-sip/auth_client_plugin.h
 * @brief Client-side plugin interface for authentication
 *
 * @note For extensions in 1.12.6 or later,
 * you have to define SOFIA_EXTEND_AUTH_CLIENT to 1
 * before including this file.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri May 19 16:18:21 EEST 2006
 */

#ifndef AUTH_CLIENT_H
#include "sofia-sip/auth_client.h"
#endif

#ifndef MSG_HEADER_H
#include <sofia-sip/msg_header.h>
#endif

SOFIA_BEGIN_DECLS

/* ====================================================================== */

struct auth_client_s {
  su_home_t     ca_home[1];
  auth_client_plugin_t const *ca_auc;

  auth_client_t *ca_next;

  char const   *ca_scheme;
  char const   *ca_realm;
  char         *ca_user;
  char         *ca_pass;

  msg_hclass_t *ca_credential_class;

#if SOFIA_EXTEND_AUTH_CLIENT
  int           ca_clear;
#endif
};

struct auth_client_plugin
{
  int auc_plugin_size;		/* Size of this structure */
  int auc_size;			/* Size of the client structure */

  char const *auc_name;		/* Name of the autentication scheme */

  /** Store challenge */
  int (*auc_challenge)(auth_client_t *ca,
		       msg_auth_t const *ch);

  /** Authorize request. */
  int (*auc_authorize)(auth_client_t *ca,
		       su_home_t *h,
		       char const *method,
		       url_t const *url,
		       msg_payload_t const *body,
		       msg_header_t **return_headers);

  /** Store nextnonce from Authentication-Info or Proxy-Authentication-Info. */
  int (*auc_info)(auth_client_t *ca, msg_auth_info_t const *ai);

#if SOFIA_EXTEND_AUTH_CLIENT
  /** Clear credentials (user/pass). @NEW_1_12_6. */
  int (*auc_clear)(auth_client_t *ca);
#endif
};

/** Check if authentication client has been extended. @NEW_1_12_6. */
#define AUTH_CLIENT_IS_EXTENDED(ca)					\
  ((ca)->ca_auc &&							\
   (ca)->ca_auc->auc_plugin_size >					\
   (int)offsetof(auth_client_plugin_t, auc_clear)			\
   && (ca)->ca_auc->auc_clear != NULL)

SOFIA_END_DECLS

#endif /* !defined AUTH_CLIENT_PLUGIN_H */
