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

/**@file sofia-sip/nth.h
 * @brief Transaction API for HTTP
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Wed Jun  5 19:25:18 2002 ppessi
 */

/* ----------------------------------------------------------------------
 * 1) Types
 */

#ifndef NTH_H_TYPES
#define NTH_H_TYPES

/** NTH engine */
typedef struct nth_engine_s   nth_engine_t;
/** NTH client request */
typedef struct nth_client_s   nth_client_t;

/** NTH (virtual) hosts or site(s) */
typedef struct nth_site_s   nth_site_t;
/** Server transaction  */
typedef struct nth_request_s nth_request_t;

#ifndef NTH_CLIENT_MAGIC_T
/** Default type of application context for client NTH requests.
 * Application may define this to appropriate type before including
 * <sofia-sip/nth.h>. */
#define NTH_CLIENT_MAGIC_T struct nth_client_magic_s
#endif

/** Application context for client requests */
typedef NTH_CLIENT_MAGIC_T  nth_client_magic_t;

#ifndef NTH_SITE_MAGIC_T
/** Default type of application context for NTH servers.
 * Application may define this to appropriate type before including
 * <sofia-sip/nth.h>. */
#define NTH_SITE_MAGIC_T struct nth_site_magic_s
#endif

/** Application context for NTH servers */
typedef NTH_SITE_MAGIC_T   nth_site_magic_t;

#endif

#ifndef NTH_H
/** Defined when <sofia-sip/nth.h> has been included. */
#define NTH_H

/* ----------------------------------------------------------------------
 * 2) Constants
 */

/** Version number */
#define NTH_VERSION "1.0"

#define NTH_CLIENT_VERSION NTH_VERSION
#define NTH_SERVER_VERSION NTH_VERSION

/* ----------------------------------------------------------------------
 * 3) Other include files
 */

#include <sofia-sip/su_wait.h>
#include <sofia-sip/su_tag.h>
#include <sofia-sip/http.h>
#include <sofia-sip/http_status.h>

#ifndef NTH_TAG_H
#include <sofia-sip/nth_tag.h>
#endif

/* ----------------------------------------------------------------------
 * 3) Engine prototypes
 */

SOFIA_BEGIN_DECLS

NTH_DLL char const *nth_engine_version(void);

NTH_DLL nth_engine_t *nth_engine_create(su_root_t *root,
					tag_type_t tag, tag_value_t value, ...);
NTH_DLL void nth_engine_destroy(nth_engine_t *engine);

NTH_DLL int nth_engine_set_params(nth_engine_t *engine,
				  tag_type_t tag, tag_value_t value, ...);
NTH_DLL int nth_engine_get_params(nth_engine_t const *engine,
				  tag_type_t tag, tag_value_t value, ...);
NTH_DLL int nth_engine_get_stats(nth_engine_t const *engine,
				 tag_type_t tag, tag_value_t value, ...);

NTH_DLL msg_t *nth_engine_msg_create(nth_engine_t *he, int flags);

/* ----------------------------------------------------------------------
 * 4) Prototypes for client transactions
 */
typedef int nth_response_f(nth_client_magic_t *magic,
			   nth_client_t *request,
			   http_t const *http);

NTH_DLL nth_client_t *nth_client_tcreate(nth_engine_t *engine,
					 nth_response_f *callback,
					 nth_client_magic_t *magic,
					 http_method_t method,
					 char const *method_name,
					 url_string_t const *request_uri,
					 tag_type_t tag, tag_value_t value,
					 ...);

NTH_DLL int nth_client_status(nth_client_t const *clnt);
NTH_DLL http_method_t nth_client_method(nth_client_t const *cnlt);
NTH_DLL int nth_client_is_streaming(nth_client_t const *hc);

NTH_DLL url_t const *nth_client_url(nth_client_t const *clnt);

NTH_DLL msg_t *nth_client_request(nth_client_t *clnt);
NTH_DLL msg_t *nth_client_response(nth_client_t const *clnt);
NTH_DLL void nth_client_destroy(nth_client_t *clnt);

/* ----------------------------------------------------------------------
 * 5) Server side prototypes
 */

typedef int nth_request_f(nth_site_magic_t *lmagic,
			  nth_site_t *server,
			  nth_request_t *req,
			  http_t const *http,
			  char const *path);

char const *nth_site_server_version(void);

NTH_DLL nth_site_t *nth_site_create(nth_site_t *parent,
				    nth_request_f *req_callback,
				    nth_site_magic_t *magic,
				    url_string_t const *address,
				    tag_type_t tag, tag_value_t value,
				    ...);

NTH_DLL void nth_site_destroy(nth_site_t *site);

NTH_DLL nth_site_magic_t *nth_site_magic(nth_site_t const *site);

NTH_DLL void nth_site_bind(nth_site_t *site,
			   nth_request_f *callback,
			   nth_site_magic_t *);

NTH_DLL su_time_t nth_site_access_time(nth_site_t const *site);

NTH_DLL int nth_site_set_params(nth_site_t *site,
				tag_type_t tag, tag_value_t value, ...);
NTH_DLL int nth_site_get_params(nth_site_t const *site,
				tag_type_t tag, tag_value_t value, ...);
NTH_DLL int nth_site_get_stats(nth_site_t const *site,
			       tag_type_t tag, tag_value_t value, ...);

NTH_DLL url_t const *nth_site_url(nth_site_t const *site);

/* ----------------------------------------------------------------------
 * 6) Prototypes for server transactions
 */

NTH_DLL int nth_request_status(nth_request_t const *req);
NTH_DLL http_method_t nth_request_method(nth_request_t const *req);
NTH_DLL msg_t *nth_request_message(nth_request_t *req);

NTH_DLL int nth_request_treply(nth_request_t *ireq,
			       int status, char const *phrase,
			       tag_type_t tag, tag_value_t value, ...);

NTH_DLL void nth_request_destroy(nth_request_t *req);

NTH_DLL struct auth_status_t *nth_request_auth(nth_request_t const *req);

SOFIA_END_DECLS

#endif
