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

/**@CFILE nua_register.c
 * @brief REGISTER and registrations
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 * @author Kai Vehmanen <Kai.Vehmanen@nokia.com>
 *
 * @date Created: Wed Mar  8 11:48:49 EET 2006 ppessi
 */

#include "config.h"

/** @internal SU network changed detector argument pointer type */
#define SU_NETWORK_CHANGED_MAGIC_T struct nua_s

#define TP_CLIENT_T          struct register_usage

#include <sofia-sip/su_string.h>
#include <sofia-sip/su_strlst.h>
#include <sofia-sip/su_uniqueid.h>
#include <sofia-sip/su_tagarg.h>

#include <sofia-sip/sip_protos.h>
#include <sofia-sip/sip_util.h>
#include <sofia-sip/sip_status.h>

#define NTA_UPDATE_MAGIC_T   struct nua_s
#define NTA_ERROR_MAGIC_T   struct nua_s

#include "nua_stack.h"

#include <sofia-sip/hostdomain.h>
#include <sofia-sip/nta_tport.h>
#include <sofia-sip/tport.h>
#include <sofia-sip/tport_tag.h>

#define OUTBOUND_OWNER_T struct nua_handle_s

#include "outbound.h"

#if HAVE_SIGCOMP
#include <sigcomp.h>
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <assert.h>

/* ======================================================================== */
/* Registrations and contacts */

int nua_registration_from_via(nua_registration_t **list,
			      nua_handle_t *nh,
			      sip_via_t const *via,
			      int public);

int nua_registration_add(nua_registration_t **list, nua_registration_t *nr);

void nua_registration_remove(nua_registration_t *nr);

int nua_registration_set_aor(su_home_t *, nua_registration_t *nr,
			     sip_from_t const *aor);

int nua_registration_set_contact(nua_handle_t *,
				 nua_registration_t *nr,
				 sip_contact_t const *m,
				 int terminating);

void nua_registration_set_ready(nua_registration_t *nr, int ready);

/* ====================================================================== */
/* REGISTER usage */

static char const *nua_register_usage_name(nua_dialog_usage_t const *du);

static int nua_register_usage_add(nua_handle_t *nh,
				  nua_dialog_state_t *ds,
				  nua_dialog_usage_t *du);
static void nua_register_usage_remove(nua_handle_t *nh,
				      nua_dialog_state_t *ds,
				      nua_dialog_usage_t *du,
				      nua_client_request_t *cr,
				      nua_server_request_t *sr);
static void nua_register_usage_update_params(nua_dialog_usage_t const *du,
					     nua_handle_preferences_t const *,
					     nua_handle_preferences_t const *,
					     nua_handle_preferences_t const *);
static void nua_register_usage_peer_info(nua_dialog_usage_t *du,
					 nua_dialog_state_t const *ds,
					 sip_t const *sip);
static void nua_register_usage_refresh(nua_handle_t *,
				       nua_dialog_state_t *,
				       nua_dialog_usage_t *,
				       sip_time_t);
static int nua_register_usage_shutdown(nua_handle_t *,
				       nua_dialog_state_t *,
				       nua_dialog_usage_t *);

/** @internal @brief REGISTER usage, aka nua_registration_t. */
struct register_usage {
  nua_registration_t *nr_next, **nr_prev, **nr_list; /* Doubly linked list and its head */
  sip_from_t *nr_aor;		/**< AoR for this registration, NULL if none */
  sip_contact_t *nr_contact;	/**< Our Contact */
  sip_contact_t nr_dcontact[1];	/**< Contact in dialog */
  sip_via_t *nr_via;		/**< Corresponding Via headers */

  unsigned long nr_min_expires;	/**< Value from 423 negotiation */

  /** Status of registration */
  unsigned nr_ready:1;

  /** Kind of registration.
   *
   * If nr_default is true, this is not a real registration but placeholder
   * for Contact header derived from a transport address.
   *
   * If nr_secure is true, this registration supports SIPS/TLS.
   *
   * If nr_public is true, transport should have public address.
   */
  unsigned nr_default:1, nr_secure:1, nr_public:1, nr_ip4:1, nr_ip6:1;

  /** Stack-generated contact */
  unsigned nr_by_stack:1;

  unsigned:0;

  int nr_error_report_id;	/**< ID used to ask for error reports from tport */

  sip_route_t *nr_route;	/**< Outgoing Service-Route */
  sip_path_t *nr_path;		/**< Incoming Path */

  tport_t *nr_tport;		/**< Transport to be used when registered */
  nua_dialog_state_t *nr_dialogs; /**< List of our dialogs */

#if HAVE_SIGCOMP
  struct sigcomp_compartment *nr_compartment;
#endif

  outbound_t *nr_ob;	/**< Outbound connection */
};

nua_usage_class const nua_register_usage[1] = {
  {
    sizeof (struct register_usage),
    (sizeof nua_register_usage),
    nua_register_usage_add,
    nua_register_usage_remove,
    nua_register_usage_name,
    nua_register_usage_update_params,
    nua_register_usage_peer_info,
    nua_register_usage_refresh,
    nua_register_usage_shutdown
  }};

static char const *nua_register_usage_name(nua_dialog_usage_t const *du)
{
  return "register";
}

static int nua_register_usage_add(nua_handle_t *nh,
				  nua_dialog_state_t *ds,
				  nua_dialog_usage_t *du)
{
  nua_registration_t *nr = NUA_DIALOG_USAGE_PRIVATE(du);

  if (ds->ds_has_register)
    return -1;			/* There can be only one usage */

  ds->ds_has_register = 1;

  nr->nr_public = 1;		/* */

  return 0;
}


static void nua_register_usage_remove(nua_handle_t *nh,
				      nua_dialog_state_t *ds,
				      nua_dialog_usage_t *du,
				      nua_client_request_t *cr,
				      nua_server_request_t *sr)
{
  nua_registration_t *nr = NUA_DIALOG_USAGE_PRIVATE(du);

  if (nr->nr_list)
    nua_registration_remove(nr);	/* Remove from list of registrations */

  if (nr->nr_ob)
    outbound_unref(nr->nr_ob);

#if HAVE_SIGCOMP
  if (nr->nr_compartment)
    sigcomp_compartment_unref(nr->nr_compartment);
  nr->nr_compartment = NULL;
#endif

  if (nr->nr_error_report_id)
    tport_release(nr->nr_tport, nr->nr_error_report_id, NULL, NULL, nr, 0);

  if (nr->nr_tport)
    tport_unref(nr->nr_tport), nr->nr_tport = NULL;

  ds->ds_has_register = 0;	/* There can be only one */
}


/** @internal Store information about registrar. */
static void nua_register_usage_peer_info(nua_dialog_usage_t *du,
					 nua_dialog_state_t const *ds,
					 sip_t const *sip)
{
  nua_registration_t *nr = NUA_DIALOG_USAGE_PRIVATE(du);
  if (nr->nr_ob)
    outbound_peer_info(nr->nr_ob, sip);
}

/* ======================================================================== */
/* REGISTER */

static void nua_register_connection_closed(tp_stack_t *sip_stack,
					   nua_registration_t *nr,
					   tport_t *tport,
					   msg_t *msg,
					   int error);

/* Interface towards outbound_t */
sip_contact_t *nua_handle_contact_by_via(nua_handle_t *nh,
					 su_home_t *home,
					 int in_dialog,
					 sip_via_t const *v,
					 char const *transport,
					 char const *m_param,
					 ...);

static int nua_stack_outbound_refresh(nua_handle_t *,
				      outbound_t *ob);

static int nua_stack_outbound_status(nua_handle_t *,
				     outbound_t *ob,
				     int status, char const *phrase,
				     tag_type_t tag, tag_value_t value, ...);

static int nua_stack_outbound_failed(nua_handle_t *,
				     outbound_t *ob,
				     int status, char const *phrase,
				     tag_type_t tag, tag_value_t value, ...);

static int nua_stack_outbound_credentials(nua_handle_t *, auth_client_t **auc);

outbound_owner_vtable nua_stack_outbound_callbacks = {
    sizeof nua_stack_outbound_callbacks,
    /* oo_contact */ nua_handle_contact_by_via,
    /* oo_refresh */ nua_stack_outbound_refresh,
    /* oo_status */  nua_stack_outbound_status,
    /* oo_probe_error */     nua_stack_outbound_failed,
    /* oo_keepalive_error */ nua_stack_outbound_failed,
    /* oo_credentials */     nua_stack_outbound_credentials
  };

/**@fn void nua_register(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...);
 *
 * Send SIP REGISTER request to the registrar.
 *
 * Request status will be delivered to the application using #nua_r_register
 * event. When successful the registration will be updated periodically.
 *
 * The handle used for registration cannot be used for any other purposes.
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *     nothing
 *
 * @par Related tags:
 *     NUTAG_REGISTRAR(), NUTAG_INSTANCE(), NUTAG_OUTBOUND(),
 *     NUTAG_KEEPALIVE(), NUTAG_KEEPALIVE_STREAM(), NUTAG_M_USERNAME(),
 *     NUTAG_M_DISPLAY(), NUTAG_M_PARAMS(), NUTAG_M_FEATURES()
 *
 * @par Events:
 *     #nua_r_register, #nua_i_outbound
 *
 * @par Generating Contact Header
 *
 * If the application did not specify the Contact header in the tags,
 * nua_register() will generate one. It will obtain the schema, IP address
 * for the host and port number for the Contact URI from the transport
 * socket. The diplay name is taken from NUTAG_M_DISPLAY(), URL username
 * part is taken from NUTAG_M_USERNAME(), URI parameters from
 * NUTAG_M_PARAMS(), and Contact header parameters from NUTAG_M_FEATURES().
 * If NUTAG_CALLEE_CAPS(1) is specified, additional Contact header
 * parameters are generated based on SDP capabilities and SIP @Allow header.
 *
 * Note that @b nua may append a identifier of its own to the @Contact URI
 * username. Such nua-generated identifier trailer always starts with "="
 * (equal sign), rest of the nua-generated identifier may contain any
 * url-unreserved characters except "=".
 *
 * Likewise, nua may add transport parameters (such as "transport=tcp" or
 * "maddr") to the @Contact URI. It can add addtional header parameters, like
 * "+sip.instance" or "reg-id", too.
 *
 * For instance, if application uses tags like
 * @code
 *   nua_register(nh,
 *                NUTAG_M_DISPLAY("1"),
 *                NUTAG_M_USERNAME("line-1"),
 *                NUTAG_M_PARAMS("user=phone"),
 *                NUTAG_M_FEATURES("audio"),
 *                NUTAG_CALLEE_CAPS(0),
 *                TAG_END())
 * @endcode
 * @b nua can generate a Contact header like
 * @code
 * Contact: 1 <sip:line-1=SSQAIbjv@192.168.1.200;transport=tcp;user=phone>
 *   ;audio;reg-id=1
 *   ;+sip.instance=urn:uuid:97701ad9-39df-1229-1083-dbc0a85f029c
 * @endcode
 *
 * The incoming request from the proxy should contain the registered contact
 * URI as the request URI. The application can use the username prefix set
 * by NUTAG_M_USERNAME() and the non-transport parameters of the request URI
 * set by NUTAG_M_PARAMS() when determining to which registration the
 * incoming request belongs.
 *
 * For example, a request line correspoding to the @Contact in above example
 * may look like:
 * @code
 * INVITE sip:line-1=SSQAIbjv@192.168.1.200;user=phone SIP/2.0
 * @endcode
 *
 * @sa NUTAG_M_DISPLAY(), NUTAG_M_USERNAME(), NUTAG_M_PARAMS(),
 * NUTAG_M_FEATURES(), NUTAG_CALLEE_CAPS().
 *
 * @par NAT, Firewall and Outbound Support
 *
 * Normally, @b nua will start start a protocol engine for outbound
 * connections used for NAT and firewall traversal and connectivity checks
 * when registering.
 *
 * @note If the application provides @b nua with a
 * @Contact header of its own (or includes a SIPTAG_CONTACT(NULL) tag in
 * nua_register() tags), the outbound protocol engine is not started. It is
 * assumed that the application knows better what it is doing when it sets
 * the @Contact, or it is using experimental CPL upload as specified in
 * <a href="http://www.ietf.org/internet-drafts/draft-lennox-sip-reg-payload-01.txt">
 * draft-lennox-sip-reg-payload-01.txt</a>.
 *
 * First, outbound engine will probe for NATs in between UA and registrar.
 * It will send a REGISTER request as usual. Upon receiving the response it
 * checks for the presence of "received" and "rport" parameters in the Via
 * header returned by registrar. The presence of NAT is determined from the
 * "received" parameter in a Via header. When a REGISTER request was sent,
 * the stack inserted the actual source IP address in the Via header: if
 * that is different from the source IP address seen by the registrar, the
 * registrar inserts the source IP address it sees into the "received"
 * parameter.
 *
 * Please note that an ALG (application-level gateway) modifying the Via
 * headers in outbound requests and again in incoming responses will make
 * the above-described NAT check to fail.
 *
 * The response to the initial REGISTER should also include option tags
 * indicating whether registrar supports various SIP extension options: @e
 * outbound, @e pref, @e path, @e gruu.
 *
 * Basically, @e outbound means that instead of registering its contact URI
 * with a particular address-of-record URI, the user-agent registers a
 * transport-level connection. Such a connection is identified on the
 * Contact header field with an instance identifier, application-provided
 * @ref NUTAG_INSTANCE() "unique string" identifying the user-agent instance
 * and a stack-generated numeric index identifying the transport-level
 * connection.
 *
 * If the @e outbound extension is supported, NUTAG_OUTBOUND() contains
 * option string "outbound" and the application has provided an instance
 * identifer to the stack with NUTAG_INSTANCE(), the nua_register() will try
 * to use outbound.
 *
 * If @e outbound is not supported, nua_register() has to generate a URI
 * that can be used to reach it from outside. It will check for public
 * transport addresses detected by underlying stack with, e.g., STUN, UPnP
 * or SOCKS. If there are public addresses, nua_register() will use them. If
 * there is no public address, it will try to generate a Contact URI from
 * the "received" and "rport" parameters found in the Via header of the
 * response message.
 *
 * @todo Actually generate public addresses.
 *
 * You can disable this kind of NAT traversal by setting "no-natify" into
 * NUTAG_OUTBOUND() options string.
 *
 * @par GRUU and Service-Route
 *
 * After a successful response to the REGISTER request has been received,
 * nua_register() will update the information about the registration based
 * on it. If there is a "gruu" parameter included in the response,
 * nua_register() will save it and use the gruu URI in the Contact header
 * fields of dialog-establishing messages, such as INVITE or SUBSCRIBE.
 * Also, if the registrar has included a Service-Route header in the
 * response, and the service route feature has not been disabled using
 * NUTAG_SERVICE_ROUTE_ENABLE(), the route URIs from the Service-Route
 * header will be used for initial non-REGISTER requests.
 *
 * The #nua_r_register message will include the contact header and route
 * used in with the registration.
 *
 * @par Registration Keep-Alive
 *
 * After the registration has successfully completed the nua_register() will
 * validate the registration and initiate the keepalive mechanism, too. The
 * user-agent validates the registration by sending a OPTIONS requests to
 * itself. If there is an error, nua_register() will indicate that to the
 * application using #nua_i_outbound event, and start unregistration
 * procedure (unless that has been explicitly disabled).
 *
 * You can disable validation by inserting "no-validate" into
 * NUTAG_OUTBOUND() string.
 *
 * The keepalive mechanism depends on the network features detected earlier.
 * If @a outbound extension is used, the STUN keepalives will be used.
 * Otherwise, NUA stack will repeatedly send OPTIONS requests to itself. In
 * order to save bandwidth, it will include Max-Forwards: 0 in the
 * keep-alive requests, however. The keepalive interval is determined by
 * NUTAG_KEEPALIVE() parameter. If the interval is 0, no keepalive messages
 * is sent.
 *
 * You can disable keepalive OPTIONS by inserting "no-options-keepalive"
 * into NUTAG_OUTBOUND() string. Currently there are no other keepalive
 * mechanisms available.
 *
 * The value of NUTAG_KEEPALIVE_STREAM(), if specified, is used to indicate
 * the desired transport-layer keepalive interval for stream-based
 * transports like TLS and TCP.
 *
 * As alternative to OPTIONS/STUN keepalives, the client can propose
 * a more frequent registration refresh interval with
 * NUTAG_M_FEATURES() (e.g. NUTAG_M_FEATURES("expires=120") given as
 * parameter to nua_register()).
 *
 * @sa #nua_r_register, nua_unregister(), #nua_r_unregister,
 * #nua_i_register,
 * @RFC3261 section 10,
 * @Expires, @Contact, @CallID, @CSeq,
 * @Path, @RFC3327, @ServiceRoute, @RFC3608, @RFC3680,
 *     NUTAG_REGISTRAR(), NUTAG_INSTANCE(), NUTAG_OUTBOUND(),
 *     NUTAG_KEEPALIVE(), NUTAG_KEEPALIVE_STREAM(),
 *     SIPTAG_CONTACT(), SIPTAG_CONTACT_STR(), NUTAG_M_USERNAME(),
 *     NUTAG_M_DISPLAY(), NUTAG_M_PARAMS(), NUTAG_M_FEATURES(),
 */

/** @NUA_EVENT nua_r_register
 *
 * Response to an outgoing REGISTER.
 *
 * The REGISTER may be sent explicitly by nua_register() or implicitly by
 * NUA state machines.
 *
 * When REGISTER request has been restarted the @a status may be 100 even
 * while the real response status returned is different.
 *
 * @param status response status code
 *               (if the request is retried, @a status is 100, the @a
 *               sip->sip_status->st_status contain the real status code
 *               from the response message, e.g., 302, 401, or 407)
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the registration
 * @param hmagic application context associated with the registration
 * @param sip    response message to REGISTER request or NULL upon an error
 *               (status code is in @a status and
 *                descriptive message in @a phrase parameters)
 * @param tags   empty
 *
 * @sa nua_register(), nua_unregister(), #nua_r_unregister,
 * @Contact, @CallID, @CSeq, @RFC3261 section 10,
 * @Path, @RFC3327, @ServiceRoute, @RFC3608, @RFC3680
 *
 * @END_NUA_EVENT
 */

/**@fn void nua_unregister(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...);
 * Unregister.
 *
 * Send a REGISTER request with expiration time 0. This removes the
 * registration from the registrar. If the handle was earlier used
 * with nua_register() the periodic updates will be terminated.
 *
 * If a SIPTAG_CONTACT_STR() with argument "*" is used, all the
 * registrations will be removed from the registrar otherwise only the
 * contact address belonging to the NUA stack is removed.
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *     nothing
 *
 * @par Related tags:
 *     NUTAG_REGISTRAR() \n
 *     Header tags defined in <sofia-sip/sip_tag.h> except SIPTAG_EXPIRES() or SIPTAG_EXPIRES_STR()
 *
 * @par Events:
 *     #nua_r_unregister
 *
 * @sa nua_register(), #nua_r_register, nua_handle_destroy(), nua_shutdown(),
 * #nua_i_register,
 * @Expires, @Contact, @CallID, @CSeq, @RFC3261 section 10,
 * @Path, @RFC3327, @ServiceRoute, @RFC3608, @RFC3680,
 *     NUTAG_REGISTRAR(), NUTAG_INSTANCE(), NUTAG_OUTBOUND(),
 *     NUTAG_KEEPALIVE(), NUTAG_KEEPALIVE_STREAM(),
 *     SIPTAG_CONTACT(), SIPTAG_CONTACT_STR(), NUTAG_M_USERNAME(),
 *     NUTAG_M_DISPLAY(), NUTAG_M_PARAMS(), NUTAG_M_FEATURES(),
 */

/** @NUA_EVENT nua_r_unregister
 *
 * Answer to outgoing un-REGISTER.
 *
 * @param status response status code
 *               (if the request is retried, @a status is 100, the @a
 *               sip->sip_status->st_status contain the real status code
 *               from the response message, e.g., 302, 401, or 407)
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the registration
 * @param hmagic application context associated with the registration
 * @param sip    response message to REGISTER request or NULL upon an error
 *               (status code is in @a status and
 *                descriptive message in @a phrase parameters)
 * @param tags   empty
 *
 * @sa nua_unregister(), nua_register(), #nua_r_register,
 * @Contact, @CallID, @CSeq, @RFC3261 section 10,
 * @Path, @RFC3327, @ServiceRoute, @RFC3608, @RFC3680
 *
 * @END_NUA_EVENT
 */

static int nua_register_client_template(nua_client_request_t *cr,
					msg_t **return_msg,
					tagi_t const *tags);
static int nua_register_client_init(nua_client_request_t *cr,
				    msg_t *, sip_t *,
				    tagi_t const *tags);
static int nua_register_client_request(nua_client_request_t *cr,
				       msg_t *, sip_t *,
				       tagi_t const *tags);
static int nua_register_client_check_restart(nua_client_request_t *cr,
					     int status, char const *phrase,
					     sip_t const *sip);
static int nua_register_client_response(nua_client_request_t *cr,
					int status, char const *phrase,
					sip_t const *sip);

static nua_client_methods_t const nua_register_client_methods = {
  SIP_METHOD_REGISTER,		/* crm_method, crm_method_name */
  0,				/* crm_extra */
  {				/* crm_flags */
    /* create_dialog */ 1,
    /* in_dialog */ 0,
    /* target refresh */ 0
  },
  nua_register_client_template,	/* crm_template */
  nua_register_client_init,	/* crm_init */
  nua_register_client_request,	/* crm_send */
  nua_register_client_check_restart, /* crm_check_restart */
  nua_register_client_response,	/* crm_recv */
  NULL,				/* crm_preliminary */
  NULL,				/* crm_report */
  NULL,				/* crm_complete */
};

/**@internal Send REGISTER. */
int nua_stack_register(nua_t *nua,
		       nua_handle_t *nh,
		       nua_event_t e,
		       tagi_t const *tags)
{
  return nua_client_create(nh, e, &nua_register_client_methods, tags);
}

static int nua_register_client_template(nua_client_request_t *cr,
					msg_t **return_msg,
					tagi_t const *tags)
{
  nua_dialog_usage_t *du;

  if (cr->cr_event == nua_r_register)
    return 0;

  /* Use a copy of REGISTER message as the template for un-REGISTER */
  du = nua_dialog_usage_get(cr->cr_owner->nh_ds, nua_register_usage, NULL);
  if (du && du->du_cr) {
    if (nua_client_set_target(cr, du->du_cr->cr_target) < 0)
      return -1;
    *return_msg = msg_copy(du->du_cr->cr_msg);
    return 1;
  }

  return 0;
}

static int nua_register_client_init(nua_client_request_t *cr,
				    msg_t *msg, sip_t *sip,
				    tagi_t const *tags)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du;
  nua_registration_t *nr;
  sip_to_t const *aor = sip->sip_to;

  int unreg;

  /* Explicit empty (NULL) contact - used for CPL store/remove? */
  if (!sip->sip_contact && cr->cr_has_contact)
    /* Do not create any usage */
    return 0;

  unreg = cr->cr_event != nua_r_register ||
    (sip->sip_expires && sip->sip_expires->ex_delta == 0);
  if (unreg)
    nua_client_set_terminating(cr, 1);

  du = nua_dialog_usage_add(nh, nh->nh_ds, nua_register_usage, NULL);
  if (du == NULL)
    return -1;
  nr = nua_dialog_usage_private(du);

  if (nua_client_bind(cr, du) < 0)
    return -1;

  if (!nr->nr_list) {
    nua_registration_add(&nh->nh_nua->nua_registrations, nr);

    if (aor == NULL)
      aor = sip->sip_from;
    if (aor == NULL)
      aor = nh->nh_nua->nua_from;

    if (nua_registration_set_aor(nh->nh_home, nr, aor) < 0)
      return -1;
  }

  if (nua_registration_set_contact(nh, nr, sip->sip_contact, unreg) < 0)
    return -1;

  if (!nr->nr_ob && (NH_PGET(nh, outbound) || NH_PGET(nh, instance))) {
    nr->nr_ob = outbound_new(nh, &nua_stack_outbound_callbacks,
			     nh->nh_nua->nua_root,
			     nh->nh_nua->nua_nta,
			     NH_PGET(nh, instance));
    if (!nr->nr_ob)
      return nua_client_return(cr, 900, "Cannot create outbound", msg);

    nua_register_usage_update_params(du,
				     NULL,
				     nh->nh_prefs,
				     nh->nh_dprefs);
  }

  if (nr->nr_ob) {
    outbound_t *ob = nr->nr_ob;
    sip_contact_t *m;

    if (!unreg && sip->sip_contact) {
      for (m = sip->sip_contact; m; m = m->m_next)
	if (!m->m_expires || strtoul(m->m_expires, NULL, 10) != 0)
	  break;

      if (m == NULL)
	unreg = 1;	/* All contacts have expires=0 */
    }

    if (outbound_set_contact(ob, sip->sip_contact, nr->nr_via, unreg) < 0)
      return nua_client_return(cr, 900, "Cannot set outbound contact", msg);
  }

  return 0;
}

static
int nua_register_client_request(nua_client_request_t *cr,
				msg_t *msg, sip_t *sip,
				tagi_t const *tags)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du = cr->cr_usage;
  nua_registration_t *nr;
  sip_contact_t *m, *contacts = sip->sip_contact;
  char const *min_expires = NULL;
  int unreg;
  tport_t *tport = NULL;

  (void)nh;

  /* Explicit empty (NULL) contact - used for CPL store/remove? */
  if (!contacts && cr->cr_has_contact)
    return nua_base_client_request(cr, msg, sip, tags);

  if ((du && du->du_shutdown) ||
      (sip->sip_expires && sip->sip_expires->ex_delta == 0))
    nua_client_set_terminating(cr, 1);

  if (contacts) {
    if (!cr->cr_terminating) {
      for (m = contacts; m; m = m->m_next)
	if (!m->m_expires || strtoul(m->m_expires, NULL, 10) != 0)
	  break;
      /* All contacts have expires=0 */
      if (m == NULL)
	nua_client_set_terminating(cr, 1);
    }
  }

  unreg = cr->cr_terminating;

  nr = nua_dialog_usage_private(du);

  if (nr) {
    if (nr->nr_ob) {
      outbound_stop_keepalive(nr->nr_ob);
      outbound_start_registering(nr->nr_ob);
    }

    if (nr->nr_by_stack) {
      sip_contact_t *m = nr->nr_contact, *previous = NULL;

      outbound_get_contacts(nr->nr_ob, &m, &previous);

      sip_add_dup(msg, sip, (sip_header_t *)m);
      /* previous is an outdated contact generated by stack
       * and it is now unregistered */
      if (previous)
	sip_add_dup(msg, sip, (sip_header_t *)previous);
    }

    tport = nr->nr_tport;
  }

  for (m = sip->sip_contact; m; m = m->m_next) {
    if (m->m_url->url_type == url_any) {
      /* If there is a '*' in contact list, remove everything else */
      while (m != sip->sip_contact)
	sip_header_remove(msg, sip, (sip_header_t *)sip->sip_contact);
      while (m->m_next)
	sip_header_remove(msg, sip, (sip_header_t *)m->m_next);
      contacts = m;
      break;
    }

    if (!m->m_expires)
      continue;
    if (unreg) {
      /* Remove the expire parameters from contacts */
      msg_header_remove_param(m->m_common, "expires");
    }
    else if (nr && nr->nr_min_expires &&
	     strtoul(m->m_expires, 0, 10) < nr->nr_min_expires) {
      if (min_expires == NULL)
	min_expires = su_sprintf(msg_home(msg), "expires=%lu",
				 nr->nr_min_expires);
      msg_header_replace_param(msg_home(msg), m->m_common, min_expires);
    }
  }

  return nua_base_client_trequest(cr, msg, sip,
				  TAG_IF(unreg, SIPTAG_EXPIRES_STR("0")),
#if 0
				  TAG_IF(unreg, NTATAG_SIGCOMP_CLOSE(1)),
				  TAG_IF(!unreg, NTATAG_COMP("sigcomp")),
#endif
				  NTATAG_TPORT(tport),
				  TAG_NEXT(tags));
}

static int nua_register_client_check_restart(nua_client_request_t *cr,
					     int status, char const *phrase,
					     sip_t const *sip)
{
  nua_registration_t *nr = nua_dialog_usage_private(cr->cr_usage);
  unsigned short retry_count = cr->cr_retry_count;
  int restart = 0, retry;

  if (nr && nr->nr_ob) {
    msg_t *_reqmsg = nta_outgoing_getrequest(cr->cr_orq);
    sip_t *req = sip_object(_reqmsg); msg_destroy(_reqmsg);

    retry = outbound_register_response(nr->nr_ob, cr->cr_terminating,
				       req, sip);

    restart = retry >= ob_reregister_now;

    if (retry == ob_reregister)
      /* outbound restarts REGISTER later */;

    if (retry < 0)
      /* XXX - report an error? */;
  }

  if (nr && status == 423) {
    if (sip->sip_min_expires)
      nr->nr_min_expires = sip->sip_min_expires->me_delta;
  }

  /* Check for status-specific reasons to retry */
  if (nua_base_client_check_restart(cr, status, phrase, sip))
    return 1;

  /* Restart only if nua_base_client_check_restart() did not try to restart */
  if (restart && retry_count == cr->cr_retry_count)
    return nua_client_restart(cr, 100, "Outbound NAT Detected");

  return 0;
}

static int nua_register_client_response(nua_client_request_t *cr,
					int status, char const *phrase,
					sip_t const *sip)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du = cr->cr_usage;
  nua_registration_t *nr = nua_dialog_usage_private(du);
  int ready;

  ready = du && !cr->cr_terminated && status < 300;

  if (ready) {
    sip_time_t mindelta = 0;
    sip_time_t now = sip_now(), delta, reqdelta, mdelta;

    sip_contact_t const *m, *sent;

    msg_t *_reqmsg = nta_outgoing_getrequest(cr->cr_orq);
    sip_t *req = sip_object(_reqmsg);

    tport_t *tport;

    msg_destroy(_reqmsg);

    assert(nr); assert(sip); assert(req);

#if HAVE_SIGCOMP
    {
      struct sigcomp_compartment *cc;
      cc = nta_outgoing_compartment(cr->cr_orq);
      sigcomp_compartment_unref(nr->nr_compartment);
      nr->nr_compartment = cc;
    }
#endif

    /* XXX - if store/remove, remove
       Content-Disposition
       Content-Type
       body
    */

    /** Search for lowest delta of SIP contacts we tried to register */
    mindelta = SIP_TIME_MAX;

    reqdelta = req->sip_expires ? req->sip_expires->ex_delta : 0;

    for (m = sip->sip_contact; m; m = m->m_next) {
      if (m->m_url->url_type != url_sip &&
	  m->m_url->url_type != url_sips)
	continue;

      for (sent = req->sip_contact; sent; sent = sent->m_next) {
	if (url_cmp(m->m_url, sent->m_url))
	  continue;

	if (sent->m_expires)
	  mdelta = strtoul(sent->m_expires, NULL, 10);
	else
	  mdelta = reqdelta;

	if (mdelta == 0)
	  mdelta = 3600;

	delta = sip_contact_expires(m, sip->sip_expires, sip->sip_date,
				    mdelta, now);
	if (delta > 0 && delta < mindelta)
	  mindelta = delta;

	if (url_cmp_all(m->m_url, sent->m_url) == 0)
	  break;
      }
    }

    if (mindelta == SIP_TIME_MAX)
      mindelta = 3600;

    nua_dialog_usage_set_refresh(du, mindelta);

  /*  RFC 3608 Section 6.1 Procedures at the UA

   The UA performs a registration as usual.  The REGISTER response may
   contain a Service-Route header field.  If so, the UA MAY store the
   value of the Service-Route header field in an association with the
   address-of-record for which the REGISTER transaction had registered a
   contact.  If the UA supports multiple addresses-of-record, it may be
   able to store multiple service routes, one per address-of-record.  If
   the UA refreshes the registration, the stored value of the Service-
   Route is updated according to the Service-Route header field of the
   latest 200 class response.  If there is no Service-Route header field
   in the response, the UA clears any service route for that address-
   of-record previously stored by the UA.  If the re-registration
   request is refused or if an existing registration expires and the UA
   chooses not to re-register, the UA SHOULD discard any stored service
   route for that address-of-record.

  */
    su_free(nh->nh_home, nr->nr_route);
    nr->nr_route = sip_route_dup(nh->nh_home, sip->sip_service_route);

    {
      /* RFC 3327 */
      /* Store last URI in Path header */
      sip_path_t *path = sip->sip_path;

      while (path && path->r_next)
	path = path->r_next;

      if (!nr->nr_path || !path ||
	  url_cmp_all(nr->nr_path->r_url, path->r_url)) {
	su_free(nh->nh_home, nr->nr_path);
	nr->nr_path = sip_path_dup(nh->nh_home, path);
      }
    }

    if (sip->sip_to->a_url->url_type == url_sips)
      nr->nr_secure = 1;

    if (nr->nr_ob) {
      outbound_gruuize(nr->nr_ob, sip);
      outbound_start_keepalive(nr->nr_ob, cr->cr_orq);
    }

    tport = nta_outgoing_transport (cr->cr_orq);

    /* cache persistant connection for registration */
    if (tport && tport != nr->nr_tport) {
      if (nr->nr_error_report_id) {
	if (tport_release(nr->nr_tport, nr->nr_error_report_id, NULL, NULL, nr, 0) < 0)
	  SU_DEBUG_1(("nua_register: tport_release() failed\n" VA_NONE));
	nr->nr_error_report_id = 0;
      }
      tport_unref(nr->nr_tport);
      nr->nr_tport = tport;

      if (tport_is_secondary(tport)) {
	tport_set_params(tport, TPTAG_SDWN_ERROR(1), TAG_END());
	nr->nr_error_report_id =
	  tport_pend(tport, NULL, nua_register_connection_closed, nr);
      }
    }
    else
      tport_unref(tport);    /* note: nta_outgoing_transport() makes a ref */

    nua_registration_set_ready(nr, 1);
  }
  else if (du) {
    nua_dialog_usage_reset_refresh(du);

    su_free(nh->nh_home, nr->nr_route);
    nr->nr_route = NULL;

    outbound_stop_keepalive(nr->nr_ob);

    /* release the persistant transport for registration */
    if (nr->nr_tport) {
      if (nr->nr_error_report_id) {
	if (tport_release(nr->nr_tport, nr->nr_error_report_id, NULL, NULL, nr, 0) < 0)
	  SU_DEBUG_1(("nua_register: tport_release() failed\n" VA_NONE));
	nr->nr_error_report_id = 0;
      }

      tport_unref(nr->nr_tport), nr->nr_tport = NULL;
    }
    nua_registration_set_ready(nr, 0);
  }


  return nua_base_client_response(cr, status, phrase, sip, NULL);
}

static
void nua_register_connection_closed(tp_stack_t *sip_stack,
				    nua_registration_t *nr,
				    tport_t *tport,
				    msg_t *msg,
				    int error)
{
  nua_dialog_usage_t *du;
  tp_name_t const *tpn;
  int pending;

  assert(nr && tport == nr->nr_tport);
  if (nr == NULL || tport != nr->nr_tport)
    return;

  du = NUA_DIALOG_USAGE_PUBLIC(nr);
  pending = nr->nr_error_report_id;

  if (tport_release(tport, pending, NULL, NULL, nr, 0) < 0)
    SU_DEBUG_1(("nua_register: tport_release() failed\n" VA_NONE));
  nr->nr_error_report_id = 0;

  tpn = tport_name(nr->nr_tport);

  SU_DEBUG_5(("nua_register(%p): tport to %s/%s:%s%s%s closed %s\n",
		  (void *)du->du_dialog->ds_owner,
	      tpn->tpn_proto, tpn->tpn_host, tpn->tpn_port,
	      tpn->tpn_comp ? ";comp=" : "",
	      tpn->tpn_comp ? tpn->tpn_comp : "",
	      error != 0 ? su_strerror(error) : ""));

  tport_unref(nr->nr_tport), nr->nr_tport = NULL;

  /* Schedule re-REGISTER immediately */
  nua_dialog_usage_set_refresh_range(du, 0, 0);
}

static void
nua_register_usage_update_params(nua_dialog_usage_t const *du,
				 nua_handle_preferences_t const *changed,
				 nua_handle_preferences_t const *nhp,
				 nua_handle_preferences_t const *dnhp)
{
  nua_registration_t *nr = nua_dialog_usage_private(du);
  outbound_t *ob = nr->nr_ob;

  if (!ob)
    return;

  if (!changed ||
      NHP_ISSET(changed, outbound) ||
      NHP_ISSET(changed, keepalive) ||
      NHP_ISSET(changed, keepalive_stream)) {
    char const *outbound =
      NHP_ISSET(nhp, outbound) ? nhp->nhp_outbound
      : dnhp->nhp_outbound;
    unsigned keepalive =
      NHP_ISSET(nhp, keepalive) ? nhp->nhp_keepalive
      : dnhp->nhp_keepalive;
    unsigned keepalive_stream =
      NHP_ISSET(nhp, keepalive_stream) ? nhp->nhp_keepalive_stream
      : NHP_ISSET(dnhp, keepalive_stream) ? nhp->nhp_keepalive_stream
      : keepalive;

    outbound_set_options(ob, outbound, keepalive, keepalive_stream);
  }

  if (!changed || NHP_ISSET(changed, proxy)) {
    if (NHP_ISSET(nhp, proxy))
      outbound_set_proxy(ob, nhp->nhp_proxy);
  }
}


static void nua_register_usage_refresh(nua_handle_t *nh,
				       nua_dialog_state_t *ds,
				       nua_dialog_usage_t *du,
				       sip_time_t now)
{
  nua_t *nua = nh->nh_nua;
  nua_client_request_t *cr = du->du_cr;

  if (cr) {
    if (nua_client_resend_request(cr, 0) >= 0)
      return;
  }

  /* Report that we have de-registered */
  nua_stack_event(nua, nh, NULL, nua_r_register, NUA_ERROR_AT(__FILE__, __LINE__), NULL);
  nua_dialog_usage_remove(nh, ds, du, NULL, NULL);
}

/** @interal Shut down REGISTER usage.
 *
 * @retval >0  shutdown done
 * @retval 0   shutdown in progress
 * @retval <0  try again later
 */
static int nua_register_usage_shutdown(nua_handle_t *nh,
				       nua_dialog_state_t *ds,
				       nua_dialog_usage_t *du)
{
  nua_client_request_t *cr = du->du_cr;
  nua_registration_t *nr = NUA_DIALOG_USAGE_PRIVATE(du);

  if (cr) {
    if (nua_client_is_queued(cr)) /* Already registering. */
      return -1;
    cr->cr_event = nua_r_unregister;
    if (nua_client_resend_request(cr, 1) >= 0)
      return 0;
  }

  /* release the persistant transport for registration */
  if (nr->nr_tport)
    tport_decref(&nr->nr_tport), nr->nr_tport = NULL;

  nua_dialog_usage_remove(nh, ds, du, NULL, NULL);
  return 200;
}

/* ---------------------------------------------------------------------- */
/* nua_registration_t interface */

#if HAVE_SOFIA_STUN
#include <sofia-sip/stun.h>
#endif

static void nua_stack_tport_update(nua_t *nua, nta_agent_t *nta);
static void nua_stack_tport_error(nua_t *nua, nta_agent_t *nta, tport_t *tport);
static int nua_registration_add_contact_and_route(nua_handle_t *nh,
						  nua_registration_t *nr,
						  msg_t *msg,
						  sip_t *sip,
						  int add_contact,
						  int add_service_route);

int
nua_stack_init_transport(nua_t *nua, tagi_t const *tags)
{
  url_string_t const *contact1 = NULL, *contact2 = NULL;
  url_string_t const *contact3 = NULL, *contact4 = NULL;
  char const *name1 = "sip", *name2 = "sip";
  char const *name3 = "sip", *name4 = "sip";
  char const *certificate_dir = NULL;

  tl_gets(tags,
          NUTAG_URL_REF(contact1),
          NUTAG_SIPS_URL_REF(contact2),
          NUTAG_WS_URL_REF(contact3),
          NUTAG_WSS_URL_REF(contact4),
          NUTAG_CERTIFICATE_DIR_REF(certificate_dir),
          TAG_END());

  if (!contact1 && contact2)
    contact1 = contact2, contact2 = NULL;

  if (contact1 &&
      (url_is_string(contact1)
       ? su_casenmatch(contact1->us_str, "sips:", 5)
       : contact1->us_url->url_type == url_sips))
    name1 = "sips";

  if (contact2 &&
      (url_is_string(contact2)
       ? su_casenmatch(contact2->us_str, "sips:", 5)
       : contact2->us_url->url_type == url_sips))
    name2 = "sips";

  if (contact3 &&
      (url_is_string(contact3)
       ? su_casenmatch(contact3->us_str, "sips:", 5)
       : contact3->us_url->url_type == url_sips))
    name3 = "sips";

  if (contact4 &&
      (url_is_string(contact4)
       ? su_casenmatch(contact4->us_str, "sips:", 5)
       : contact4->us_url->url_type == url_sips))
    name4 = "sips";

  if (!contact1 /* && !contact2 */) {
    if (nta_agent_add_tport(nua->nua_nta, NULL,
			    TPTAG_IDENT("sip"),
			    TPTAG_CERTIFICATE(certificate_dir),
			    TAG_NEXT(nua->nua_args)) < 0 &&
        nta_agent_add_tport(nua->nua_nta, URL_STRING_MAKE("sip:*:*"),
			    TPTAG_IDENT("sip"),
			    TPTAG_CERTIFICATE(certificate_dir),
			    TAG_NEXT(nua->nua_args)) < 0)
      return -1;
#if HAVE_SOFIA_STUN
    if (stun_is_requested(TAG_NEXT(nua->nua_args)) &&
	nta_agent_add_tport(nua->nua_nta, URL_STRING_MAKE("sip:0.0.0.0:*"),
			    TPTAG_IDENT("stun"),
			    TPTAG_PUBLIC(tport_type_stun), /* use stun */
			    TPTAG_CERTIFICATE(certificate_dir),
			    TAG_NEXT(nua->nua_args)) < 0) {
      SU_DEBUG_0(("nua: error initializing STUN transport\n" VA_NONE));
    }
#endif
  }
  else {
    if (nta_agent_add_tport(nua->nua_nta, contact1,
			    TPTAG_IDENT(name1),
			    TPTAG_CERTIFICATE(certificate_dir),
			    TAG_NEXT(nua->nua_args)) < 0)
      return -1;

    if (contact2 &&
	nta_agent_add_tport(nua->nua_nta, contact2,
			    TPTAG_IDENT(name2),
			    TPTAG_CERTIFICATE(certificate_dir),
			    TAG_NEXT(nua->nua_args)) < 0)
      return -1;

    if (contact3 &&
	nta_agent_add_tport(nua->nua_nta, contact3,
			    TPTAG_IDENT(name3),
			    TPTAG_CERTIFICATE(certificate_dir),
			    TAG_NEXT(nua->nua_args)) < 0)
      return -1;

    if (contact4 &&
	nta_agent_add_tport(nua->nua_nta, contact4,
			    TPTAG_IDENT(name4),
			    TPTAG_CERTIFICATE(certificate_dir),
			    TAG_NEXT(nua->nua_args)) < 0)
      return -1;
  }


  if (nua_stack_init_registrations(nua) < 0)
    return -1;

  return 0;
}

#if 0
  /* Store network detector param value */
  if (agent->sa_nw_updates == 0)
    agent->sa_nw_updates = nw_updates;
 	      NTATAG_DETECT_NETWORK_UPDATES_REF(nw_updates),
  unsigned nw_updates = 0;
  unsigned nw_updates = 0;

  su_network_changed_t *sa_nw_changed;

#endif

static
void nua_network_changed_cb(nua_t *nua, su_root_t *root)
{

  uint32_t nw_updates;

  nw_updates = nua->nua_prefs->ngp_detect_network_updates;

  switch (nw_updates) {
  case NUA_NW_DETECT_ONLY_INFO:
    nua_stack_event(nua, NULL, NULL, nua_i_network_changed, SIP_200_OK, NULL);
    break;

  case NUA_NW_DETECT_TRY_FULL:

    /* 1) Shutdown all tports */
    nta_agent_close_tports(nua->nua_nta);

    /* 2) Create new tports */
    if (nua_stack_init_transport(nua, nua->nua_args) < 0)
      /* We are hosed */
      nua_stack_event(nua, NULL, NULL, nua_i_network_changed,
		      900, "Internal Error", NULL);
    else
      nua_stack_event(nua, NULL, NULL, nua_i_network_changed,
		      SIP_200_OK, NULL);

    break;

  default:
    break;
  }

  return;
}

int nua_stack_launch_network_change_detector(nua_t *nua)
{
  su_network_changed_t *snc = NULL;

  snc = su_root_add_network_changed(nua->nua_home,
				    nua->nua_root,
				    nua_network_changed_cb,
				    nua);

  if (!snc)
    return -1;

  nua->nua_nw_changed = snc;

  return 0;
}


int
nua_stack_init_registrations(nua_t *nua)
{
  /* Create initial identities: peer-to-peer, public, sips */
  nua_registration_t **nr_list = &nua->nua_registrations, **nr_next;
  nua_handle_t **nh_list;
  nua_handle_t *dnh = nua->nua_dhandle;
  sip_via_t const *v;

  /* Remove existing, local address based registrations and count the
     rest */
  while (nr_list && *nr_list) {
    nr_next = &(*nr_list)->nr_next;
    if ((*nr_list)->nr_default == 1) {
      nua_registration_remove(*nr_list);
      /* memset(*nr_list, 170, sizeof(**nr_list)); */
      /* XXX - free, too */
    }
    nr_list = nr_next;
  }
  nr_list = &nua->nua_registrations;

  v = nta_agent_public_via(nua->nua_nta);
  if (v) {
    nua_registration_from_via(nr_list, dnh, v, 1);
  }

  v = nta_agent_via(nua->nua_nta);
  if (v) {
    nua_registration_from_via(nr_list, dnh, v, 0);
  }
  else {
    sip_via_t v[2];

    sip_via_init(v)->v_next = v + 1;
    v[0].v_protocol = sip_transport_udp;
    v[0].v_host = "addr.is.invalid.";
    sip_via_init(v + 1);
    v[1].v_protocol = sip_transport_tcp;
    v[1].v_host = "addr.is.invalid.";

    nua_registration_from_via(nr_list, dnh, v, 0);
  }

  /* Go through all the registrations and set to refresh almost
     immediately */
  nh_list = &nua->nua_handles;
  for (; *nh_list; nh_list = &(*nh_list)->nh_next) {
    nua_dialog_state_t *ds;
    nua_dialog_usage_t *du;

    ds = (*nh_list)->nh_ds;
    du = ds->ds_usage;

    if (ds->ds_has_register == 1 && du->du_class->usage_refresh) {
      nua_dialog_usage_refresh(*nh_list, ds, du, 1);
    }
  }

  nta_agent_bind_tport_update(nua->nua_nta, (nta_update_magic_t *)nua, nua_stack_tport_update);
  nta_agent_bind_tport_error(nua->nua_nta, (nta_error_magic_t *)nua, nua_stack_tport_error);

  return 0;
}

int nua_registration_from_via(nua_registration_t **list,
			      nua_handle_t *nh,
			      sip_via_t const *via,
			      int public)
{
  su_home_t *home = nh->nh_home;
  sip_via_t *v, *pair, /* v2[2], */ *vias, **vv, **prev;
  nua_registration_t *nr = NULL, **next;
  su_home_t autohome[SU_HOME_AUTO_SIZE(1024)];
  int nr_items = 0;

  vias = sip_via_copy(su_home_auto(autohome, sizeof autohome), via);

  for (; *list; list = &(*list)->nr_next)
    ++nr_items;

  next = list;

  for (vv = &vias; (v = *vv);) {
    char const *protocol;
    sip_contact_t *contact;
    sip_via_t v2[2];

    *vv = v->v_next, v->v_next = NULL, pair = NULL;

    if (v->v_protocol == sip_transport_tcp)
      protocol = sip_transport_udp;
    else if (v->v_protocol == sip_transport_udp)
      protocol = sip_transport_tcp;
    else
      protocol = NULL;

    if (protocol) {
      /* Try to pair vias if we have both udp and tcp */
      for (prev = vv; *prev; prev = &(*prev)->v_next) {
        if (!su_casematch(protocol, (*prev)->v_protocol))
          continue;
        if (!su_casematch(v->v_host, (*prev)->v_host))
          continue;
        if (!su_strmatch(v->v_port, (*prev)->v_port))
          continue;
        break;
      }

      if (*prev) {
        pair = *prev; *prev = pair->v_next; pair->v_next = NULL;
      }
    }

    /* if more than one candidate, ignore local entries */
    if (v && (*vv || nr_items > 0) &&
	host_is_local(v->v_host)) {
      SU_DEBUG_9(("nua_register: ignoring contact candidate %s:%s.\n",
		  v->v_host, v->v_port ? v->v_port : ""));
      continue;
    }

    nr = su_zalloc(home, sizeof *nr);
    if (!nr)
      break;

    v2[0] = *v;

    if (pair)
      /* Don't use protocol if we have both udp and tcp */
      protocol = NULL, v2[0].v_next = &v2[1], v2[1] = *pair;
    else
      protocol = via->v_protocol, v2[0].v_next = NULL;

    v2[1].v_next = NULL;

    contact = nua_handle_contact_by_via(nh, home, 0, v2, protocol, NULL);

    v = sip_via_dup(home, v2);

    if (!contact || !v) {
      su_free(home, nr);
      break;
    }

    nr->nr_ready = 1, nr->nr_default = 1, nr->nr_public = public;
    nr->nr_secure = contact->m_url->url_type == url_sips;
    nr->nr_contact = contact;
    *nr->nr_dcontact = *contact, nr->nr_dcontact->m_params = NULL;
    nr->nr_via = v;
    nr->nr_ip4 = host_is_ip4_address(contact->m_url->url_host);
    nr->nr_ip6 = !nr->nr_ip4 && host_is_ip6_reference(contact->m_url->url_host);

    SU_DEBUG_9(("nua_register: Adding contact URL '%s' to list.\n", contact->m_url->url_host));

    ++nr_items;
    nr->nr_next = *next, nr->nr_prev = next; *next = nr, next = &nr->nr_next;
    nr->nr_list = list;
  }

  su_home_deinit(autohome);

  return 0;
}

static
void nua_stack_tport_error(nua_t *nua, nta_agent_t *nta, tport_t *tport)
{
  return;
}

static
void nua_stack_tport_update(nua_t *nua, nta_agent_t *nta)
{
#if 0
  nua_registration_t *default_oc;
  nua_registration_t const *defaults = nua->nua_registrations;
  sip_via_t *via = nta_agent_via(nta);

  default_oc = outbound_by_aor(defaults, NULL, 1);

  if (default_oc) {
    assert(default_oc->nr_via);

    outbound_contacts_from_via(default_oc,
				       via,
				       via->v_next);

    /* refresh_register(nua_handle_t *nh, nua_dialog_usage_t *du, sip_time_t now); */
  }
#endif
  return;
}

nua_registration_t *nua_registration_by_aor(nua_registration_t const *list,
					    sip_from_t const *aor,
					    url_t const *remote_uri,
					    int only_default)
{
  sip_from_t *alt_aor = NULL, _alt_aor[1];
  int sips_aor = aor && aor->a_url->url_type == url_sips;
  int sips_uri = remote_uri && remote_uri->url_type == url_sips;

  nua_registration_t const *nr, *public = NULL, *any = NULL;
  nua_registration_t const *registered = NULL;
  nua_registration_t const *namewise = NULL, *sipswise = NULL;

  int ip4 = remote_uri && host_is_ip4_address(remote_uri->url_host);
  int ip6 = remote_uri && host_is_ip6_reference(remote_uri->url_host);

  if (only_default || aor == NULL) {
    /* Ignore AoR, select only by remote_uri */
    for (nr = list; nr; nr = nr->nr_next) {
      if (!nr->nr_ready)
	continue;
      if (only_default && !nr->nr_default)
	continue;
      if (nr->nr_ip4 && ip6)
	continue;
      if (nr->nr_ip6 && ip4)
	continue;
      if (sips_uri ? nr->nr_secure : !nr->nr_secure)
	return (nua_registration_t *)nr;
      if (!registered && nr->nr_aor)
	registered = nr;
      if (!public && nr->nr_public)
	public = nr;
      if (!any)
	any = nr;
    }
    if (registered)
      return (nua_registration_t *)registered;
    if (public)
      return (nua_registration_t *)public;
    if (any)
      return (nua_registration_t *)any;
    return NULL;
  }

  if (!sips_aor && aor) {
    alt_aor = memcpy(_alt_aor, aor, sizeof _alt_aor);
    alt_aor->a_url->url_type = url_sips;
    alt_aor->a_url->url_scheme = "sips";
  }

  for (nr = list; nr; nr = nr->nr_next) {
    if (!nr->nr_ready || !nr->nr_contact)
      continue;
    if (nr->nr_aor) {
      if (aor && url_cmp(nr->nr_aor->a_url, aor->a_url) == 0)
	return (nua_registration_t *)nr;
      if (!namewise && alt_aor && url_cmp(nr->nr_aor->a_url, aor->a_url) == 0)
	namewise = nr;
    }

    if (!sipswise && ((sips_aor || sips_uri) ?
		      nr->nr_secure : !nr->nr_secure))
      sipswise = nr;
    if (!registered)
      registered = nr;
    if (!public && nr->nr_public)
      public = nr;
    if (!any)
      any = nr;
  }

  if (namewise)
    return (nua_registration_t *)namewise;
  if (sipswise)
    return (nua_registration_t *)sipswise;
  if (registered)
    return (nua_registration_t *)registered;

  /* XXX -
     should we do some policing whether sips_aor or sips_uri can be used
     with sip contact?
  */
  if (public)
    return (nua_registration_t *)public;
  if (any)
    return (nua_registration_t *)any;

  return NULL;
}


nua_registration_t *
nua_registration_for_request(nua_registration_t const *list, sip_t const *sip)
{
  sip_from_t const *aor;
  url_t *uri;

  aor = sip->sip_from;
  uri = sip->sip_request->rq_url;

  return nua_registration_by_aor(list, aor, uri, 0);
}

nua_registration_t *
nua_registration_for_response(nua_registration_t const *list,
			      sip_t const *sip,
			      sip_record_route_t const *record_route,
			      sip_contact_t const *remote_contact)
{
  nua_registration_t *nr;
  sip_to_t const *aor = NULL;
  url_t const *uri = NULL;

  if (sip)
    aor = sip->sip_to;

  if (record_route)
    uri = record_route->r_url;
  else if (sip && sip->sip_record_route)
    uri = sip->sip_record_route->r_url;
  else if (remote_contact)
    uri = remote_contact->m_url;
  else if (sip && sip->sip_from)
    uri = sip->sip_from->a_url;

  nr = nua_registration_by_aor(list, aor, uri, 0);

  return nr;
}


/** Return Contact usable in dialogs */
sip_contact_t const *nua_registration_contact(nua_registration_t const *nr)
{
  if (nr->nr_by_stack && nr->nr_ob) {
    sip_contact_t const *m = outbound_dialog_contact(nr->nr_ob);
    if (m)
      return m;
  }

  if (nr->nr_contact)
    return nr->nr_dcontact;
  else
    return NULL;
}

/** Return initial route. */
sip_route_t const *nua_registration_route(nua_registration_t const *nr)
{
  return nr ? nr->nr_route : NULL;
}

sip_contact_t const *nua_stack_get_contact(nua_registration_t const *nr)
{
  nr = nua_registration_by_aor(nr, NULL, NULL, 1);
  return nr && nr->nr_contact ? nr->nr_dcontact : NULL;
}

/** Add a Contact (and Route) header to request */
int nua_registration_add_contact_to_request(nua_handle_t *nh,
					    msg_t *msg,
					    sip_t *sip,
					    int add_contact,
					    int add_service_route)
{
  nua_registration_t *nr = NULL;

  if (!add_contact && !add_service_route)
    return 0;

  if (nh == NULL || msg == NULL)
    return -1;

  if (sip == NULL)
    sip = sip_object(msg);

  if (nr == NULL)
    nr = nua_registration_for_request(nh->nh_nua->nua_registrations, sip);

  return nua_registration_add_contact_and_route(nh, nr, msg, sip,
						add_contact,
						add_service_route);
}

/** Add a Contact header to response.
 *
 * @param nh
 * @param msg response message
 * @param sip headers in response message
 * @param record_route record-route from request
 * @param remote_contact Contact from request
 */
int nua_registration_add_contact_to_response(nua_handle_t *nh,
					     msg_t *msg,
					     sip_t *sip,
					     sip_record_route_t const *record_route,
					     sip_contact_t const *remote_contact)
{
  nua_registration_t *nr = NULL;

  if (sip == NULL)
    sip = sip_object(msg);

  if (nh == NULL || msg == NULL || sip == NULL)
    return -1;

  if (nr == NULL)
    nr = nua_registration_for_response(nh->nh_nua->nua_registrations, sip,
				       record_route, remote_contact);

  return nua_registration_add_contact_and_route(nh, nr, msg, sip,
						1,
						0);
}

/** Add a Contact (and Route) header to request */
static
int nua_registration_add_contact_and_route(nua_handle_t *nh,
					   nua_registration_t *nr,
					   msg_t *msg,
					   sip_t *sip,
					   int add_contact,
					   int add_service_route)
{
  if (nr == NULL)
    return -1;

  if (add_contact) {
    sip_contact_t const *m = NULL;
    char const *m_display;
    char const *m_username;
    char const *m_params;
    url_t const *u;

    if (nr->nr_by_stack && nr->nr_ob) {
      m = outbound_dialog_gruu(nr->nr_ob);

      if (m)
	return msg_header_add_dup(msg, (msg_pub_t *)sip, (void const *)m);

      m = outbound_dialog_contact(nr->nr_ob);
    }

    if (m == NULL)
      m = nr->nr_contact;

    if (!m)
      return -1;

    u = m->m_url;

    if (NH_PISSET(nh, m_display))
      m_display = NH_PGET(nh, m_display);
    else
      m_display = m->m_display;

    if (NH_PISSET(nh, m_username))
      m_username = NH_PGET(nh, m_username);
    else
      m_username = m->m_url->url_user;

    if (NH_PISSET(nh, m_params)) {
      m_params = NH_PGET(nh, m_params);

      if (u->url_params && m_params && strstr(u->url_params, m_params) == 0)
	m_params = NULL;
    }
    else
      m_params = NULL;

    m = sip_contact_format(msg_home(msg),
			   "%s<%s:%s%s%s%s%s%s%s%s%s>",
			   m_display ? m_display : "",
			   u->url_scheme,
			   m_username ? m_username : "",
			   m_username ? "@" : "",
			   u->url_host,
			   u->url_port ? ":" : "",
			   u->url_port ? u->url_port : "",
			   u->url_params ? ";" : "",
			   u->url_params ? u->url_params : "",
			   m_params ? ";" : "",
			   m_params ? m_params : "");

    if (msg_header_insert(msg, (msg_pub_t *)sip, (void *)m) < 0)
      return -1;
  }

  if (add_service_route && !sip->sip_status) {
    sip_route_t const *sr = nua_registration_route(nr);
    if (msg_header_add_dup(msg, (msg_pub_t *)sip, (void const *)sr) < 0)
      return -1;
  }

  return 0;
}


/** Add a registration to list of contacts */
int nua_registration_add(nua_registration_t **list,
			 nua_registration_t *nr)
{
  assert(list && nr);

  if (nr->nr_list == NULL) {
    nua_registration_t *next = *list;
    if (next)
      next->nr_prev = &nr->nr_next;
    nr->nr_next = next, nr->nr_prev = list, nr->nr_list = list;
    *list = nr;
  }

  return 0;
}

/** Remove from list of registrations */
void nua_registration_remove(nua_registration_t *nr)
{
  if ((*nr->nr_prev = nr->nr_next))
    nr->nr_next->nr_prev = nr->nr_prev;
  nr->nr_next = NULL, nr->nr_prev = NULL, nr->nr_list = NULL;
}

/** Set address-of-record. */
int nua_registration_set_aor(su_home_t *home,
			     nua_registration_t *nr,
			     sip_from_t const *aor)
{
  sip_from_t *new_aor, *old_aor;

  if (!home || !nr || !aor)
    return -1;

  new_aor = sip_from_dup(home, aor);
  if (!new_aor)
    return -1;

  old_aor = nr->nr_aor;
  nr->nr_aor = new_aor;
  msg_header_free(home, (void *)old_aor);

  return 0;
}

/** Set contact. */
int nua_registration_set_contact(nua_handle_t *nh,
				 nua_registration_t *nr,
				 sip_contact_t const *application_contact,
				 int terminating)
{
  sip_contact_t *m = NULL, *previous;
  url_t *uri;

  if (!nh || !nr)
    return -1;

  uri = nr->nr_aor ? nr->nr_aor->a_url : NULL;

  previous = nr->nr_contact;

  if (application_contact) {
    m = sip_contact_dup(nh->nh_home, application_contact);
  }
  else if (terminating && nr->nr_contact) {
    return 0;
  }
  else {
    nua_registration_t *nr0;

    nr0 = nua_registration_by_aor(*nr->nr_list, NULL, uri, 1);

    if (nr0 && nr0->nr_via) {
      char const *tport = nr0->nr_via->v_next ? NULL : nr0->nr_via->v_protocol;
      m = nua_handle_contact_by_via(nh, nh->nh_home, 0,
				    nr0->nr_via, tport, NULL);
    }
  }

  if (!m)
    return -1;

  nr->nr_contact = m;
  *nr->nr_dcontact = *m, nr->nr_dcontact->m_params = NULL;
  nr->nr_ip4 = host_is_ip4_address(m->m_url->url_host);
  nr->nr_ip6 = !nr->nr_ip4 && host_is_ip6_reference(m->m_url->url_host);
  nr->nr_by_stack = !application_contact;

  msg_header_free(nh->nh_home, (void *)previous);

  return 0;
}

/** Mark registration as ready */
void nua_registration_set_ready(nua_registration_t *nr, int ready)
{
  if (nr) {
    assert(!ready || nr->nr_contact);
    nr->nr_ready = ready;
  }
}

/** @internal Hook for processing incoming request by registration.
 *
 * This is used for keepalive/validate OPTIONS.
 */
int nua_registration_process_request(nua_registration_t *list,
				     nta_incoming_t *irq,
				     sip_t const *sip)
{
  //sip_call_id_t *i;
  nua_registration_t *nr;

  if (!outbound_targeted_request(sip))
    return 0;

  /* Process by outbound... */
  //i = sip->sip_call_id;

  for (nr = list; nr; nr = nr->nr_next) {
    outbound_t *ob = nr->nr_ob;
    if (ob)
      if (outbound_process_request(ob, irq, sip))
	return 501;		/* Just in case  */
  }

  return 481;			/* Call/Transaction does not exist */
}

/** Outbound requests us to refresh registration */
static int nua_stack_outbound_refresh(nua_handle_t *nh,
				      outbound_t *ob)
{
  nua_dialog_state_t *ds = nh->nh_ds;
  nua_dialog_usage_t *du;

  du = nua_dialog_usage_get(ds, nua_register_usage, NULL);

  if (du)
    nua_dialog_usage_refresh(nh, ds, du, 1);

  return 0;
}

/** @NUA_EVENT nua_i_outbound
 *
 * Status from outbound engine.
 *
 * @param status SIP status code or NUA status code (>= 900)
 *               describing the outbound state
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the outbound engine
 * @param hmagic application context associated with the handle
 * @param sip    NULL or response message to an keepalive message or
 *               registration probe
 *               (error code and message are in status an phrase parameters)
 * @param tags   empty
 *
 * @sa NUTAG_OUTBOUND(), NUTAG_KEEPALIVE(), NUTAG_KEEPALIVE_STREAM(),
 * nua_register(), #nua_r_register, nua_unregister(), #nua_r_unregister
 *
 * @END_NUA_EVENT
 */

/** @internal Callback from outbound_t */
static int nua_stack_outbound_status(nua_handle_t *nh, outbound_t *ob,
				     int status, char const *phrase,
				     tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;

  ta_start(ta, tag, value);

  nua_stack_event(nh->nh_nua, nh, NULL,
		  nua_i_outbound, status, phrase,
		  ta_args(ta));

  ta_end(ta);

  return 0;
}

/** @internal Callback from outbound_t */
static int nua_stack_outbound_failed(nua_handle_t *nh, outbound_t *ob,
				     int status, char const *phrase,
				     tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  ta_start(ta, tag, value);

  nua_stack_event(nh->nh_nua, nh, NULL,
		  nua_i_outbound, status, phrase,
		  ta_args(ta));

  ta_end(ta);

  return 0;
}

/** @internal Callback for obtaining credentials for keepalive */
static int nua_stack_outbound_credentials(nua_handle_t *nh,
					  auth_client_t **auc)
{
  return auc_copy_credentials(auc, nh->nh_auth);
}

#include <ctype.h>
#include <sofia-sip/bnf.h>

/** @internal Generate a @Contact header. */
sip_contact_t *nua_handle_contact_by_via(nua_handle_t *nh,
					 su_home_t *home,
					 int in_dialog,
					 sip_via_t const *v,
					 char const *transport,
					 char const *m_param,
					 ...)
{
  su_strlst_t *l;
  char const *s;
  char const *host, *port, *maddr, *comp;
  int one = 1;
  char _transport[16];
  va_list va;
  sip_contact_t *m;
  url_t url;

  url_init(&url, url_sip);

  if (!v) return NULL;

  host = v->v_host;
  if (v->v_received)
    host = v->v_received;
  port = sip_via_port(v, &one);
  maddr = v->v_maddr;
  comp = v->v_comp;

  if (host == NULL)
    return NULL;

  if (sip_transport_has_tls(v->v_protocol) ||
      sip_transport_has_tls(transport)) {
    url.url_type = url_sips;
    url.url_scheme = url_scheme(url_sips);
    if (port && strcmp(port, SIPS_DEFAULT_SERV) == 0)
      port = NULL;
    if (port || host_is_ip_address(host))
      transport = NULL;
  }
  else if (port && host_is_ip_address(host) &&
	   strcmp(port, SIP_DEFAULT_SERV) == 0) {
    port = NULL;
  }

  if (transport) {
    if (su_casenmatch(transport, "SIP/2.0/", 8))
      transport += 8;

    /* Make transport parameter lowercase */
    if (strlen(transport) < (sizeof _transport)) {
      char *s = strcpy(_transport, transport);
      short c;

      for (s = _transport; (c = *s) && c != ';'; s++)
	if (isupper(c))
	  *s = tolower(c);

      transport = _transport;
    }
  }

  s = NH_PGET(nh, m_username);
  if (s)
    url.url_user = s;
  url.url_host = host;
  url.url_port = port;
  url.url_params = su_strdup(home, NH_PGET(nh, m_params));
  if (transport) {
    url.url_params = url_strip_param_string((char*)url.url_params, "transport");
    url_param_add(home, &url, su_sprintf(home, "transport=%s", transport));
  }
  if (maddr) {
    url.url_params = url_strip_param_string((char*)url.url_params, "maddr");
    url_param_add(home, &url, su_sprintf(home, "maddr=%s", maddr));
  }
  if (comp) {
    url.url_params = url_strip_param_string((char*)url.url_params, "comp");
    url_param_add(home, &url, su_sprintf(home, "comp=%s", comp));
  }

  l = su_strlst_create(NULL);

  s = NH_PGET(nh, m_display);
  if (s) {
    int quote = s[span_token_lws(s)] != '\0';

    su_strlst_append(l, quote ? "\"" : "");
    su_strlst_append(l, s);
    su_strlst_append(l, quote ? "\" " : " ");
  }

  su_strlst_append(l, "<");
  su_strlst_append(l, url_as_string(home, &url));
  su_strlst_append(l, ">");

  va_start(va, m_param);

  for (s = m_param; s; s = va_arg(va, char *)) {
    if (strlen(s) == 0)
      continue;
    su_strlst_append(l, s[0] == ';' ? "" : ";");
    su_strlst_append(l, s);
  }

  va_end(va);

  if (!in_dialog) {
    s = NH_PGET(nh, m_features);
    if (s)
      s[0] == ';' ? "" : su_strlst_append(l, ";"), su_strlst_append(l, s);

    if (NH_PGET(nh, callee_caps)) {
      sip_allow_t const *allow = NH_PGET(nh, allow);

      if (allow) {
	su_strlst_append(l, ";methods=\"");
	if (allow->k_items) {
	  size_t i;
	  for (i = 0; allow->k_items[i]; i++) {
	    su_strlst_append(l, allow->k_items[i]);
	    if (allow->k_items[i + 1])
	      su_strlst_append(l, ",");
	  }
	}
	su_strlst_append(l, "\"");
      }

      if (nh->nh_soa) {
	char **media = soa_media_features(nh->nh_soa, 0, home);

	while (*media) {
	  if (su_strlst_len(l))
	    su_strlst_append(l, ";");
	  su_strlst_append(l, *media++);
	}
      }
    }
  }

  m = sip_contact_make(home, su_strlst_join(l, su_strlst_home(l), ""));

  su_strlst_destroy(l);

  return m;
}
