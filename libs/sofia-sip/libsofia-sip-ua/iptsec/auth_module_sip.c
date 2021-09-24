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

/**@internal
 * @file auth_module_sip.c
 * @brief Authenticate SIP request
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Jari Urpalainen <Jari.Urpalainen@nokia.com>
 *
 * @date Created: Thu Jan 15 17:23:21 2004 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <string.h>

#include <sofia-sip/sip.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_status.h>

#include <sofia-sip/nta.h>

#include <sofia-sip/auth_module.h>

static auth_challenger_t sip_server_challenger[] =
  {{ SIP_401_UNAUTHORIZED, sip_www_authenticate_class,
     sip_authentication_info_class
    }};

static auth_challenger_t sip_proxy_challenger[] =
  {{ SIP_407_PROXY_AUTH_REQUIRED, sip_proxy_authenticate_class }};

/** Authenticate an incoming SIP request.
 *
 * The function auth_mod_check() completes the @a as structure and calls the
 * scheme-specific authentication method performing the actual
 * authentication.
 *
 * A successful authentication is indicated by setting @a as->as_status to
 * 0.  The authentication module sets @a as->as_match as the matching
 * credential header.
 */
void auth_mod_check(auth_mod_t *am,
		    auth_status_t *as,
		    sip_t const *sip,
		    auth_kind_t proxy)
{
  msg_auth_t *credentials;
  auth_challenger_t const *challenger;

  if (as == NULL || sip == NULL)
    return;

  if (am == NULL) {
    as->as_status = 0;
    return;
  }

  credentials = proxy ? sip->sip_proxy_authorization : sip->sip_authorization;
  challenger = proxy ? sip_proxy_challenger : sip_server_challenger;

  if (sip->sip_request)
    as->as_method = sip->sip_request->rq_method_name;

  if (sip->sip_payload)
    as->as_body = sip->sip_payload->pl_data,
      as->as_bodylen = sip->sip_payload->pl_len;

  auth_mod_method(am, as, credentials, challenger);
}
