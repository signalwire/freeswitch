/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2006 Nokia Corporation.
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

#ifndef OUTBOUND_H
/** Defined when <outbound.h> has been included. */
#define OUTBOUND_H
/**@IFILE outbound.h
 *
 * @brief Interface to SIP NAT traversal and outbound
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed May 10 12:01:38 EEST 2006 ppessi
 */

#ifndef SU_CONFIG_H
#include <sofia-sip/su_config.h>
#endif
#ifndef NTA_H
#include <sofia-sip/nta.h>
#endif
#ifndef AUTH_CLIENT_H
#include <sofia-sip/auth_client.h>
#endif

SOFIA_BEGIN_DECLS

/* ====================================================================== */
/* Outbound connection */

#ifndef OUTBOUND_OWNER_T
#define OUTBOUND_OWNER_T struct nua_handle_s /* Just for now */
#endif

typedef struct outbound outbound_t;
typedef struct outbound_owner_vtable outbound_owner_vtable;
typedef OUTBOUND_OWNER_T outbound_owner_t;

outbound_t *outbound_new(outbound_owner_t *owner,
			 outbound_owner_vtable const *owner_methods,
			 su_root_t *root,
			 nta_agent_t *agent,
			 char const *instance);

void outbound_unref(outbound_t *ob);

int outbound_set_options(outbound_t *ob,
			 char const *options,
			 unsigned dgram_interval,
			 unsigned stream_interval);

int outbound_set_proxy(outbound_t *ob,
		       url_string_t *proxy);

int outbound_get_contacts(outbound_t *ob,
			  sip_contact_t **return_current_contact,
			  sip_contact_t **return_previous_contact);

int outbound_start_registering(outbound_t *ob);

int outbound_register_response(outbound_t *ob,
			       int terminating,
			       sip_t const *request,
			       sip_t const *response);

/** Return values for outbound_register_response(). */
enum {
  ob_register_error = -1,	/* Or anything below zero */
  ob_register_ok = 0,		/* No need to re-register */
  ob_reregister = 1,		/* Re-register when oo_refresh() is called */
  ob_reregister_now = 2		/* Re-register immediately */
};

int outbound_set_contact(outbound_t *ob,
			 sip_contact_t const *application_contact,
			 sip_via_t const *v,
			 int terminating);

sip_contact_t const *outbound_dialog_contact(outbound_t const *ob);

sip_contact_t const *outbound_dialog_gruu(outbound_t const *ob);

int outbound_gruuize(outbound_t *ob, sip_t const *sip);

void outbound_start_keepalive(outbound_t *ob,
			      nta_outgoing_t *register_trans);

void outbound_stop_keepalive(outbound_t *ob);

int outbound_targeted_request(sip_t const *sip);

int outbound_process_request(outbound_t *ob,
			     nta_incoming_t *irq,
			     sip_t const *sip);

void outbound_peer_info(outbound_t *ob, sip_t const *sip);

struct outbound_owner_vtable
{
  int oo_size;
  sip_contact_t *(*oo_contact)(outbound_owner_t *,
			       su_home_t *home,
			       int used_in_dialog,
			       sip_via_t const *v,
			       char const *transport,
			       char const *m_param,
			       ...);
  int (*oo_refresh)(outbound_owner_t *, outbound_t *ob);
  int (*oo_status)(outbound_owner_t *, outbound_t *ob,
		   int status, char const *phrase,
		   tag_type_t tag, tag_value_t value, ...);
  int (*oo_probe_error)(outbound_owner_t *, outbound_t *ob,
			int status, char const *phrase,
			tag_type_t tag, tag_value_t value, ...);
  int (*oo_keepalive_error)(outbound_owner_t *, outbound_t *ob,
			    int status, char const *phrase,
			    tag_type_t tag, tag_value_t value, ...);
  int (*oo_credentials)(outbound_owner_t *, auth_client_t **auc);
};

SOFIA_END_DECLS

#endif /* OUTBOUND_H */
