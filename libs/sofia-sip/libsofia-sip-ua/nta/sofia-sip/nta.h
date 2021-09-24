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

#ifndef NTA_H
/** Defined when <sofia-sip/nta.h> has been included. */
#define NTA_H

/**@file sofia-sip/nta.h  @brief  Nokia Transaction API for SIP
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Jul 18 09:18:32 2000 ppessi
 */

#ifndef SU_WAIT_H
#include <sofia-sip/su_wait.h>
#endif

#ifndef SIP_H
#include <sofia-sip/sip.h>
#endif

#ifndef NTA_TAG_H
#include <sofia-sip/nta_tag.h>
#endif

SOFIA_BEGIN_DECLS

/* ----------------------------------------------------------------------
 * 1) Types
 */

/** NTA agent */
typedef struct nta_agent_s      nta_agent_t;
/** NTA call leg */
typedef struct nta_leg_s        nta_leg_t;
/** NTA outgoing request */
typedef struct nta_outgoing_s   nta_outgoing_t;
/** NTA incoming request */
typedef struct nta_incoming_s   nta_incoming_t;

#ifndef NTA_AGENT_MAGIC_T
/** Default type of application context for NTA agents.
 * Application may define this to appropriate type before including
 * <sofia-sip/nta.h>. */
#define NTA_AGENT_MAGIC_T struct nta_agent_magic_s
#endif
#ifndef NTA_LEG_MAGIC_T
/** Default type of application context for NTA call legs.
 * Application may define this to appropriate type before including
 * <sofia-sip/nta.h>. */
#define NTA_LEG_MAGIC_T struct nta_leg_magic_s
#endif
#ifndef NTA_OUTGOING_MAGIC_T
/** Default type of application context for outgoing NTA requests.
 * Application may define this to appropriate type before including
 * <sofia-sip/nta.h>. */
#define NTA_OUTGOING_MAGIC_T struct nta_outgoing_magic_s
#endif
#ifndef NTA_INCOMING_MAGIC_T
/** Default type of application context for incoming NTA requests.
 * Application may define this to appropriate type before including
 * <sofia-sip/nta.h>. */
#define NTA_INCOMING_MAGIC_T struct nta_incoming_magic_s
#endif

/** Application context for NTA agents */
typedef NTA_AGENT_MAGIC_T     nta_agent_magic_t;
/** Application context for NTA call legs */
typedef NTA_LEG_MAGIC_T       nta_leg_magic_t;
/** Application context for outgoing NTA requests */
typedef NTA_OUTGOING_MAGIC_T  nta_outgoing_magic_t;
/** Application context for incoming NTA requests */
typedef NTA_INCOMING_MAGIC_T  nta_incoming_magic_t;

/* ----------------------------------------------------------------------
 * 2) Constants
 */

/** NTA API version number */
#define NTA_VERSION "2.0"

/** NTA module version */
SOFIAPUBVAR char const nta_version[];

enum {
  /* Stack parameters */
  NTA_SIP_T1 = 500,		/**< SIP T1, 500 milliseconds. */
  NTA_SIP_T2 = 4000,		/**< SIP T2, 4 seconds. */
  NTA_SIP_T4 = 5000,		/**< SIP T4, 5 seconds. */
  NTA_TIME_MAX = 15 * 24 * 3600 * 1000
				/**< Maximum value for timers. */
};

/* ----------------------------------------------------------------------
 * 3) Agent-level prototypes
 */

typedef int nta_message_f(nta_agent_magic_t *context,
			  nta_agent_t *agent,
			  msg_t *msg,
			  sip_t *sip);

SOFIAPUBFUN
nta_agent_t *nta_agent_create(su_root_t *root,
			      url_string_t const *name,
			      nta_message_f *callback,
			      nta_agent_magic_t *magic,
			      tag_type_t tag, tag_value_t value, ...);

SOFIAPUBFUN void nta_agent_destroy(nta_agent_t *agent);

SOFIAPUBFUN char const *nta_agent_version(nta_agent_t const *a);
SOFIAPUBFUN nta_agent_magic_t *nta_agent_magic(nta_agent_t const *a);

SOFIAPUBFUN
int nta_agent_add_tport(nta_agent_t *agent,
			url_string_t const *url,
			tag_type_t tag, tag_value_t value, ...);

SOFIAPUBFUN int nta_agent_close_tports(nta_agent_t *agent);

SOFIAPUBFUN sip_contact_t *nta_agent_contact(nta_agent_t const *a);
SOFIAPUBFUN sip_via_t *nta_agent_via(nta_agent_t const *a);
SOFIAPUBFUN sip_via_t *nta_agent_public_via(nta_agent_t const *a);

SOFIAPUBFUN char const *nta_agent_newtag(su_home_t *,
					 char const *fmt, nta_agent_t *);

SOFIAPUBFUN int nta_agent_set_params(nta_agent_t *agent,
				     tag_type_t tag, tag_value_t value, ...);
SOFIAPUBFUN int nta_agent_get_params(nta_agent_t *agent,
				     tag_type_t tag, tag_value_t value, ...);

SOFIAPUBFUN int nta_agent_get_stats(nta_agent_t *agent,
				    tag_type_t tag, tag_value_t value, ...);

/* ----------------------------------------------------------------------
 * 4) Message-level prototypes
 */

SOFIAPUBFUN msg_t *nta_msg_create(nta_agent_t *self, int flags);

SOFIAPUBFUN int nta_msg_complete(msg_t *msg);

SOFIAPUBFUN int nta_msg_request_complete(msg_t *msg,
					 nta_leg_t *leg,
					 sip_method_t method,
					 char const *method_name,
					 url_string_t const *req_url);

SOFIAPUBFUN int nta_msg_is_internal(msg_t const *msg);
SOFIAPUBFUN int nta_sip_is_internal(sip_t const *sip);

/* ----------------------------------------------------------------------
 * 5) Leg-level prototypes
 */
typedef int nta_request_f(nta_leg_magic_t *lmagic,
			  nta_leg_t *leg,
			  nta_incoming_t *irq,
			  sip_t const *sip);

SOFIAPUBFUN
nta_leg_t *nta_leg_tcreate(nta_agent_t *agent,
			   nta_request_f *req_callback,
			   nta_leg_magic_t *magic,
			   tag_type_t tag, tag_value_t value, ...);

SOFIAPUBFUN void nta_leg_destroy(nta_leg_t *leg);

SOFIAPUBFUN nta_leg_t *nta_default_leg(nta_agent_t const *agent);

SOFIAPUBFUN nta_leg_magic_t *nta_leg_magic(nta_leg_t const *leg,
					   nta_request_f *callback);

SOFIAPUBFUN void nta_leg_bind(nta_leg_t *leg,
			      nta_request_f *callback,
			      nta_leg_magic_t *);

/** Add local tag. */
SOFIAPUBFUN char const *nta_leg_tag(nta_leg_t *leg, char const *tag);

/** Get local tag. */
SOFIAPUBFUN char const *nta_leg_get_tag(nta_leg_t const *leg);

/** Add remote tag. */
SOFIAPUBFUN char const *nta_leg_rtag(nta_leg_t *leg, char const *tag);

/** Get remote tag. */
SOFIAPUBFUN char const *nta_leg_get_rtag(nta_leg_t const *leg);

/** Get local request sequence number. @NEW_1_12_9 */
SOFIAPUBFUN uint32_t nta_leg_get_seq(nta_leg_t const *leg);

/** Get remote request sequence number. @NEW_1_12_9 */
SOFIAPUBFUN uint32_t nta_leg_get_rseq(nta_leg_t const *leg);

SOFIAPUBFUN int nta_leg_client_route(nta_leg_t *leg,
				     sip_record_route_t const *route,
				     sip_contact_t const *contact);

SOFIAPUBFUN int nta_leg_client_reroute(nta_leg_t *leg,
				       sip_record_route_t const *route,
				       sip_contact_t const *contact,
				       int initial);

SOFIAPUBFUN int nta_leg_server_route(nta_leg_t *leg,
				     sip_record_route_t const *route,
				     sip_contact_t const *contact);

/** Get route */
SOFIAPUBFUN int nta_leg_get_route(nta_leg_t *leg,
				  sip_route_t const **return_route,
				  sip_contact_t const **return_target);

/** Get leg by destination */
SOFIAPUBFUN nta_leg_t *nta_leg_by_uri(nta_agent_t const *,
				      url_string_t const *);

/** Get leg by dialog */
SOFIAPUBFUN
nta_leg_t *nta_leg_by_dialog(nta_agent_t const *agent,
			     url_t const *request_uri,
			     sip_call_id_t const *call_id,
			     char const *from_tag,
			     url_t const *from_url,
			     char const *to_tag,
			     url_t const *to_url);

/** Generate Replaces header */
SOFIAPUBFUN sip_replaces_t *nta_leg_make_replaces(nta_leg_t *leg,
						  su_home_t *home,
						  int early_only);
/** Get dialog leg by Replaces header */
SOFIAPUBFUN
nta_leg_t *nta_leg_by_replaces(nta_agent_t *, sip_replaces_t const *);

/** Get dialog leg by CallID */
SOFIAPUBFUN
nta_leg_t *nta_leg_by_call_id(nta_agent_t *sa, const char *call_id);

/* ----------------------------------------------------------------------
 * 6) Prototypes for incoming transactions
 */

SOFIAPUBFUN
nta_incoming_t *nta_incoming_create(nta_agent_t *agent,
				    nta_leg_t *leg,
				    msg_t *msg,
				    sip_t *sip,
				    tag_type_t tag, tag_value_t value, ...);

SOFIAPUBFUN nta_incoming_t *nta_incoming_default(nta_agent_t *agent);

typedef int nta_ack_cancel_f(nta_incoming_magic_t *imagic,
			     nta_incoming_t *irq,
			     sip_t const *sip);

SOFIAPUBFUN void nta_incoming_bind(nta_incoming_t *irq,
				   nta_ack_cancel_f *callback,
				   nta_incoming_magic_t *imagic);

SOFIAPUBFUN
nta_incoming_magic_t *nta_incoming_magic(nta_incoming_t *irq,
					 nta_ack_cancel_f *callback);

SOFIAPUBFUN
nta_incoming_t *nta_incoming_find(nta_agent_t const *agent,
				  sip_t const *sip,
				  sip_via_t const *v);

SOFIAPUBFUN char const *nta_incoming_tag(nta_incoming_t *irq, char const *tag);
SOFIAPUBFUN char const *nta_incoming_gettag(nta_incoming_t const *irq);

SOFIAPUBFUN int nta_incoming_status(nta_incoming_t const *irq);
SOFIAPUBFUN sip_method_t nta_incoming_method(nta_incoming_t const *irq);
SOFIAPUBFUN char const *nta_incoming_method_name(nta_incoming_t const *irq);
SOFIAPUBFUN url_t const *nta_incoming_url(nta_incoming_t const *irq);
SOFIAPUBFUN uint32_t nta_incoming_cseq(nta_incoming_t const *irq);
SOFIAPUBFUN sip_time_t nta_incoming_received(nta_incoming_t *irq, su_nanotime_t *nano);

SOFIAPUBFUN int nta_incoming_set_params(nta_incoming_t *irq,
					tag_type_t tag, tag_value_t value, ...);

SOFIAPUBFUN msg_t *nta_incoming_getrequest(nta_incoming_t *irq);
SOFIAPUBFUN msg_t *nta_incoming_getrequest_ackcancel(nta_incoming_t *irq);
SOFIAPUBFUN msg_t *nta_incoming_getresponse(nta_incoming_t *irq);

SOFIAPUBFUN
int nta_incoming_complete_response(nta_incoming_t *irq,
				   msg_t *msg,
				   int status,
				   char const *phrase,
				   tag_type_t tag, tag_value_t value, ...);

SOFIAPUBFUN
msg_t *nta_incoming_create_response(nta_incoming_t *irq, int status, char const *phrase);

SOFIAPUBFUN
int nta_incoming_treply(nta_incoming_t *ireq,
			int status, char const *phrase,
			tag_type_t tag, tag_value_t value, ...);

SOFIAPUBFUN int nta_incoming_mreply(nta_incoming_t *irq, msg_t *msg);

SOFIAPUBFUN void nta_incoming_destroy(nta_incoming_t *irq);

/* Functions for feature, method, mime, session-timer negotation */

SOFIAPUBFUN
int nta_check_required(nta_incoming_t *irq,
		       sip_t const *sip,
		       sip_supported_t const *supported,
		       tag_type_t tag, tag_value_t value, ...);
SOFIAPUBFUN
int nta_check_supported(nta_incoming_t *irq,
			sip_t const *sip,
			sip_require_t *require,
			tag_type_t tag, tag_value_t value, ...);
SOFIAPUBFUN
int nta_check_method(nta_incoming_t *irq,
		     sip_t const *sip,
		     sip_allow_t const *allow,
		     tag_type_t tag, tag_value_t value, ...);
SOFIAPUBFUN
int nta_check_session_content(nta_incoming_t *irq, sip_t const *sip,
			      sip_accept_t const *session_accepts,
			      tag_type_t tag, tag_value_t value, ...);
SOFIAPUBFUN
int nta_check_accept(nta_incoming_t *irq,
		     sip_t const *sip,
		     sip_accept_t const *acceptable,
		     sip_accept_t const **return_acceptable,
		     tag_type_t tag, tag_value_t value, ...);

SOFIAPUBFUN
int nta_check_session_expires(nta_incoming_t *irq,
			      sip_t const *sip,
			      sip_time_t my_min_se,
			      tag_type_t tag, tag_value_t value, ...);

/* ----------------------------------------------------------------------
 * 7) Prototypes for outgoing transactions
 */
typedef int nta_response_f(nta_outgoing_magic_t *magic,
			   nta_outgoing_t *request,
			   sip_t const *sip);

SOFIAPUBFUN
nta_outgoing_t *nta_outgoing_tcreate(nta_leg_t *leg,
				     nta_response_f *callback,
				     nta_outgoing_magic_t *magic,
				     url_string_t const *route_url,
				     sip_method_t method,
				     char const *method_name,
				     url_string_t const *request_uri,
				     tag_type_t tag, tag_value_t value, ...);

SOFIAPUBFUN
nta_outgoing_t *nta_outgoing_mcreate(nta_agent_t *agent,
				     nta_response_f *callback,
				     nta_outgoing_magic_t *magic,
				     url_string_t const *route_url,
				     msg_t *msg,
				     tag_type_t tag, tag_value_t value, ...);

SOFIAPUBFUN
nta_outgoing_t *nta_outgoing_default(nta_agent_t *agent,
				     nta_response_f *callback,
				     nta_outgoing_magic_t *magic);

SOFIAPUBFUN int nta_outgoing_bind(nta_outgoing_t *orq,
				  nta_response_f *callback,
				  nta_outgoing_magic_t *magic);
SOFIAPUBFUN nta_outgoing_magic_t *nta_outgoing_magic(nta_outgoing_t const *orq,
						     nta_response_f *callback);
SOFIAPUBFUN int nta_outgoing_status(nta_outgoing_t const *orq);
SOFIAPUBFUN sip_method_t nta_outgoing_method(nta_outgoing_t const *orq);
SOFIAPUBFUN char const *nta_outgoing_method_name(nta_outgoing_t const *orq);
SOFIAPUBFUN uint32_t nta_outgoing_cseq(nta_outgoing_t const *orq);
SOFIAPUBFUN char const *nta_outgoing_branch(nta_outgoing_t const *orq);

SOFIAPUBFUN unsigned nta_outgoing_delay(nta_outgoing_t const *orq);

SOFIAPUBFUN url_t const *nta_outgoing_request_uri(nta_outgoing_t const *orq);
SOFIAPUBFUN url_t const *nta_outgoing_route_uri(nta_outgoing_t const *orq);

SOFIAPUBFUN msg_t *nta_outgoing_getresponse(nta_outgoing_t *orq);
SOFIAPUBFUN msg_t *nta_outgoing_getrequest(nta_outgoing_t *orq);

SOFIAPUBFUN
nta_outgoing_t *nta_outgoing_tagged(nta_outgoing_t *orq,
				    nta_response_f *callback,
				    nta_outgoing_magic_t *magic,
				    char const *to_tag,
				    sip_rseq_t const *rseq);

SOFIAPUBFUN int nta_outgoing_cancel(nta_outgoing_t *);

SOFIAPUBFUN
nta_outgoing_t *nta_outgoing_tcancel(nta_outgoing_t *orq,
				     nta_response_f *callback,
				     nta_outgoing_magic_t *magic,
				     tag_type_t, tag_value_t, ...);

SOFIAPUBFUN void nta_outgoing_destroy(nta_outgoing_t *);

SOFIAPUBFUN
nta_outgoing_t *nta_outgoing_find(nta_agent_t const *sa,
				  msg_t const *msg,
				  sip_t const *sip,
				  sip_via_t const *v);

SOFIAPUBFUN int nta_tport_keepalive(nta_outgoing_t *orq);

/* ----------------------------------------------------------------------
 * 8) Reliable provisional responses (100rel)
 */

/* UAC side */

SOFIAPUBFUN
nta_outgoing_t *nta_outgoing_prack(nta_leg_t *leg,
				   nta_outgoing_t *oorq,
				   nta_response_f *callback,
				   nta_outgoing_magic_t *magic,
				   url_string_t const *route_url,
				   sip_t const *response_to_prack,
				   tag_type_t, tag_value_t, ...);

SOFIAPUBFUN uint32_t nta_outgoing_rseq(nta_outgoing_t const *orq);
SOFIAPUBFUN int nta_outgoing_setrseq(nta_outgoing_t *orq, uint32_t rseq);

/* UAS side */

/** NTA reliable response */
typedef struct nta_reliable_s   nta_reliable_t;

#ifndef NTA_RELIABLE_MAGIC_T
/** Default type of application context for reliable preliminary responses.
 * Application may define this to appropriate type before including
 * <sofia-sip/nta.h>. */
#define NTA_RELIABLE_MAGIC_T struct nta_reliable_magic_s
#endif

/** Application context for reliable preliminary responses. */
typedef NTA_RELIABLE_MAGIC_T  nta_reliable_magic_t;

typedef int nta_prack_f(nta_reliable_magic_t *rmagic,
			nta_reliable_t *rel,
			nta_incoming_t *prack,
			sip_t const *sip);

SOFIAPUBFUN
nta_reliable_t *nta_reliable_treply(nta_incoming_t *ireq,
				    nta_prack_f *callback,
				    nta_reliable_magic_t *rmagic,
				    int status, char const *phrase,
				    tag_type_t tag,
				    tag_value_t value, ...);

SOFIAPUBFUN
nta_reliable_t *nta_reliable_mreply(nta_incoming_t *irq,
				    nta_prack_f *callback,
				    nta_reliable_magic_t *rmagic,
				    msg_t *msg);

SOFIAPUBFUN void nta_reliable_destroy(nta_reliable_t *);

/* ----------------------------------------------------------------------
 * Backward-compatibility stuff - going away soon
 */

#define nta_outgoing_tmcreate nta_outgoing_mcreate
#define nta_msg_response_complete(msg, irq, status, phrase) \
  nta_incoming_complete_response((irq), (msg), (status), (phrase), TAG_END())

SOFIAPUBFUN void nta_msg_discard(nta_agent_t *agent, msg_t *msg);

SOFIAPUBFUN int nta_is_internal_msg(msg_t const *msg);

SOFIA_END_DECLS

#endif
