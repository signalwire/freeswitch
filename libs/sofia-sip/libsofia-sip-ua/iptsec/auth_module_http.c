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
 * @file auth_module_http.c
 * @brief Authenticate HTTP request
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Jari Urpalainen <Jari.Urpalainen@nokia.com>
 *
 * @date Created: Thu Jan 15 17:23:21 2004 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <string.h>

#include <sofia-sip/http.h>
#include <sofia-sip/http_header.h>
#include <sofia-sip/http_status.h>

#include <sofia-sip/auth_module.h>

static auth_challenger_t http_server_challenger[] =
  {{ HTTP_401_UNAUTHORIZED, http_www_authenticate_class }};

static auth_challenger_t http_proxy_challenger[] =
  {{ HTTP_407_PROXY_AUTH, http_proxy_authenticate_class }};

const char *auth_mod_check_http(auth_mod_t *am,
	 	                auth_status_t *as,
	                        http_t const *http,
		                auth_kind_t proxy)
{
  msg_auth_t *credentials =
    proxy ? http->http_proxy_authorization : http->http_authorization;
  auth_challenger_t const *challenger =
    proxy ? http_proxy_challenger : http_server_challenger;

  if (http->http_request) {
    if (!as->as_method)
      as->as_method = http->http_request->rq_method_name;
#if 0
    if (!as->as_uri)
      as->as_uri = http->http_request->rq_url;
#endif
  }

  if (http->http_payload && !as->as_body)
    as->as_body = http->http_payload->pl_data,
      as->as_bodylen = http->http_payload->pl_len;

  /* Call real authentication method */
  auth_mod_check_client(am, as, credentials, challenger);

  if (as->as_status)
    return NULL;
  else
    return as->as_user;
}
