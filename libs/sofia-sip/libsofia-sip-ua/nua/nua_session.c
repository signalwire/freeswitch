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

/**@CFILE nua_session.c
 * @brief SIP session handling
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Mar  8 16:17:27 EET 2006 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <assert.h>

#include <sofia-sip/su_string.h>
#include <sofia-sip/sip_protos.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sip_util.h>
#include <sofia-sip/su_uniqueid.h>
#include <sofia-sip/msg_mime_protos.h>

#define NTA_INCOMING_MAGIC_T struct nua_server_request
#define NTA_OUTGOING_MAGIC_T struct nua_client_request
#define NTA_RELIABLE_MAGIC_T struct nua_server_request

#include "nua_stack.h"
#include <sofia-sip/soa.h>

#ifndef SDP_H
typedef struct sdp_session_s sdp_session_t;
#endif

/* ---------------------------------------------------------------------- */

/** @enum nua_callstate

The states for SIP session established with INVITE.

Initially the call states follow the state of the INVITE transaction. If the
initial INVITE transaction fails, the call is terminated. The status codes
401 and 407 are an exception: if the client (on the left side in the diagram
below) receives them, it enters in #nua_callstate_authenticating state.

If a re-INVITE transaction fails, the result depends on the status code in
failure. The call can return to the ready state, be terminated immediately,
or be terminated gracefully. The proper action to take is determined with
sip_response_terminates_dialog().

@sa @ref nua_call_model, #nua_i_state, nua_invite(), #nua_i_invite

@par Session State Diagram

@code
  			 +----------+
  			 |          |---------------------+
  			 |   Init   |                     |
  			 |          |----------+          |
  			 +----------+          |          |
  			  |        |           |          |
                 --/INVITE|        |INVITE/100 |          |
                          V        V           |          |
     		+----------+      +----------+ |          |
       +--------|          |      |          | |          |
       |  18X +-| Calling  |      | Received | |INVITE/   |
       |   /- | |          |      |          | |  /18X    |
       |      V +----------+      +----------+ V          |
       |   +----------+ |           |     |  +----------+ |
       |---|          | |2XX     -/ |  -/ |  |          | |
       |   | Proceed- | | /-     2XX|  18X|  |  Early   | |INVITE/
       |   |   ing    | |           |     +->|          | |  /200
       |   +----------+ V           V        +----------+ |
       |     |  +----------+      +----------+   | -/     |
       |  2XX|  |          |      |          |<--+ 2XX    |
       |   /-|  | Complet- |      | Complete |<-----------+
       |     +->|   ing    |      |          |------+
       |        +----------+      +----------+      |
       |                  |        |      |         |
       |401,407/     -/ACK|        |ACK/- |timeout/ |
       | /ACK             V        V      | /BYE    |
       |                 +----------+     |         |
       |                 |          |     |         |
       |              +--|  Ready   |     |         |
       |              |  |          |     |         |
       |              |  +----------+     |         |
       |              |       |           |         |
       |         BYE/ |       |-/BYE      |         |BYE/
       V         /200 |       V           |         |/200
  +----------+        |  +----------+     |         |
  |          |        |  |          |     |         |
  |Authentic-|        |  | Terminat-|<----+         |
  |  ating   |        |  |   ing    |               |
  +----------+        |  +----------+               |
                      |       |                     |
                      |       |[23456]XX/-          |
                      |       V                     |
                      |  +----------+               |
                      |  |          |               |
                      +->|Terminated|<--------------+
                         |          |
                         +----------+
                              |
                              V
                         +----------+
        		 |          |
                         |   Init   |
			 |          |
          		 +----------+
@endcode
*/

/* ---------------------------------------------------------------------- */
/* Session event usage */

/** @internal @brief Session-related state. */
typedef struct nua_session_usage
{
  enum nua_callstate ss_state;		/**< Session status (enum nua_callstate) */

  unsigned        ss_100rel:1;	        /**< Use 100rel, send 183 */
  unsigned        ss_alerting:1;	/**< 180 is sent/received */

  unsigned        ss_update_needed:2;	/**< Send an UPDATE (do O/A if > 1) */

  unsigned        ss_precondition:1;	/**< Precondition required */

  unsigned        ss_reporting:1;       /**< True if reporting state */
  unsigned        : 0;

  struct session_timer {
    unsigned  interval;		/**< Negotiated expiration time */
    enum nua_session_refresher refresher; /**< Our Negotiated role */

    struct {
      unsigned expires, defaults; /**< Value of Session-Expires (delta) */
      unsigned min_se;	/**< Minimum session expires */
      /** none, local or remote */
      enum nua_session_refresher refresher;
      unsigned    supported:1, require:1, :0;
    } local, remote;

    unsigned      timer_set:1;  /**< We have active session timer. */
  } ss_timer[1];

  char const     *ss_reason;	        /**< Reason for termination. */

  /* Offer-Answer status */
  char const     *ss_oa_recv, *ss_oa_sent;

  /**< Version of user SDP from latest successful O/A */
  int ss_sdp_version;
} nua_session_usage_t;

static char const Offer[] = "offer", Answer[] = "answer";

static char const *nua_session_usage_name(nua_dialog_usage_t const *du);
static int nua_session_usage_add(nua_handle_t *nh,
				 nua_dialog_state_t *ds,
				 nua_dialog_usage_t *du);
static void nua_session_usage_remove(nua_handle_t *nh,
				     nua_dialog_state_t *ds,
				     nua_dialog_usage_t *du,
				     nua_client_request_t *cr,
				     nua_server_request_t *sr);
static void nua_session_usage_refresh(nua_owner_t *,
				      nua_dialog_state_t *,
				      nua_dialog_usage_t *,
				      sip_time_t now);
static int nua_session_usage_shutdown(nua_owner_t *,
				      nua_dialog_state_t *,
				      nua_dialog_usage_t *);

static void signal_call_state_change(nua_handle_t *nh,
				      nua_session_usage_t *ss,
				      int status, char const *phrase,
				      enum nua_callstate next_state);

static int nua_invite_client_should_ack(nua_client_request_t const *cr);
static int nua_invite_client_ack(nua_client_request_t *cr, tagi_t const *tags);
static int nua_invite_client_complete(nua_client_request_t *cr);

static nua_usage_class const nua_session_usage[1] = {
  {
    sizeof (nua_session_usage_t),
    sizeof nua_session_usage,
    nua_session_usage_add,
    nua_session_usage_remove,
    nua_session_usage_name,
    nua_base_usage_update_params,
    NULL,
    nua_session_usage_refresh,
    nua_session_usage_shutdown
  }};

static char const *nua_session_usage_name(nua_dialog_usage_t const *du)
{
  return "session";
}

static
int nua_session_usage_add(nua_handle_t *nh,
			   nua_dialog_state_t *ds,
			   nua_dialog_usage_t *du)
{
  nua_session_usage_t *ss = NUA_DIALOG_USAGE_PRIVATE(du);

  if (ds->ds_has_session)
    return -1;
  ds->ds_has_session = 1;
  ds->ds_got_session = 1;

  ss->ss_timer->local.refresher = nua_any_refresher;
  ss->ss_timer->remote.refresher = nua_any_refresher;

  return 0;
}

static
void nua_session_usage_remove(nua_handle_t *nh,
			      nua_dialog_state_t *ds,
			      nua_dialog_usage_t *du,
			      nua_client_request_t *cr0,
			      nua_server_request_t *sr0)
{
  nua_session_usage_t *ss = NUA_DIALOG_USAGE_PRIVATE(du);
  nua_client_request_t *cr, *cr_next;
  nua_server_request_t *sr;

  /* Destroy queued INVITE transactions */
  for (cr = ds->ds_cr; cr; cr = cr_next) {
    cr_next = cr->cr_next;

    if (cr->cr_method != sip_method_invite)
      continue;

    if (cr == cr0)
      continue;

    nua_client_request_ref(cr);

    if (nua_invite_client_should_ack(cr)) {
      ss->ss_reporting = 1;
      nua_invite_client_ack(cr, NULL);
      ss->ss_reporting = 0;
    }

    if (cr == du->du_cr && cr->cr_orq)
      continue;

    if (cr->cr_status < 200) {
      nua_stack_event(nh->nh_nua, nh,
		      NULL,
		      (enum nua_event_e)cr->cr_event,
		      SIP_481_NO_TRANSACTION,
		      NULL);
    }

    nua_client_request_remove(cr);

    nua_client_request_unref(cr);

    cr_next = ds->ds_cr;
  }

  if (ss->ss_state != nua_callstate_terminated &&
      ss->ss_state != nua_callstate_init &&
      !ss->ss_reporting) {
    int status = 0; char const *phrase = "Terminated";

    if (cr0)
      status = cr0->cr_status, phrase = cr0->cr_phrase ? cr0->cr_phrase : phrase;
    else if (sr0)
      status = sr0->sr_status, phrase = sr0->sr_phrase;

    signal_call_state_change(nh, ss, status, phrase, nua_callstate_terminated);
  }

  /* Application can respond to BYE after the session usage has terminated */
  for (sr = ds->ds_sr; sr; sr = sr->sr_next) {
    if (sr->sr_usage == du && sr->sr_method == sip_method_bye)
      sr->sr_usage = NULL;
  }

  ds->ds_has_session = 0;
  nh->nh_has_invite = 0;
  nh->nh_active_call = 0;
  nh->nh_hold_remote = 0;

  if (nh->nh_soa)
    soa_destroy(nh->nh_soa), nh->nh_soa = NULL;
}

static
nua_dialog_usage_t *nua_dialog_usage_for_session(nua_dialog_state_t const *ds)
{
  if (ds == ((nua_handle_t *)NULL)->nh_ds)
    return NULL;

  return nua_dialog_usage_get(ds, nua_session_usage, NULL);
}

static
nua_session_usage_t *nua_session_usage_for_dialog(nua_dialog_state_t const *ds)
{
  nua_dialog_usage_t *du;

  if (ds == ((nua_handle_t *)NULL)->nh_ds)
    return NULL;

  du = nua_dialog_usage_get(ds, nua_session_usage, NULL);

  return (nua_session_usage_t *)nua_dialog_usage_private(du);
}

/** Zap the session associated with the handle */
static
void nua_session_usage_destroy(nua_handle_t *nh,
			       nua_session_usage_t *ss)
{
  /* Remove usage */
  nua_dialog_usage_remove(nh, nh->nh_ds, nua_dialog_usage_public(ss), NULL, NULL);

  SU_DEBUG_5(("nua: terminated session %p\n", (void *)nh));
}

/* ======================================================================== */
/* INVITE and call (session) processing */

static int session_timer_is_supported(struct session_timer const *t);

static void session_timer_preferences(struct session_timer *t,
				      sip_t const *sip,
				      sip_supported_t const *supported,
				      unsigned expires, int isset,
				      enum nua_session_refresher refresher,
				      unsigned min_se);

static void session_timer_store(struct session_timer *t,
				sip_t const *sip);

static int session_timer_check_min_se(msg_t *msg, sip_t *sip,
				      sip_t const *request,
				      unsigned long min_se);

static int session_timer_add_headers(struct session_timer *t,
				     int initial,
					 msg_t *msg, sip_t *sip,
					 nua_handle_t *nh);

static void session_timer_negotiate(struct session_timer *t, int uas);

static void session_timer_set(nua_session_usage_t *ss, int uas);

static int session_timer_check_restart(nua_client_request_t *cr,
				       int status, char const *phrase,
				       sip_t const *sip);

static int nh_referral_check(nua_handle_t *nh, tagi_t const *tags);
static void nh_referral_respond(nua_handle_t *,
				int status, char const *phrase);

static
int session_get_description(sip_t const *sip,
			    char const **return_sdp,
			    size_t *return_len);

static
int session_include_description(soa_session_t *soa,
				int session,
				msg_t *msg,
				sip_t *sip);

static
int session_make_description(su_home_t *home,
			     soa_session_t *soa,
			     int session,
			     sip_content_disposition_t **return_cd,
			     sip_content_type_t **return_ct,
			     sip_payload_t **return_pl);

static
int nua_server_retry_after(nua_server_request_t *sr,
			   int status, char const *phrase,
			   int min, int max);

/**@fn void nua_invite(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...);
 *
 * Place a call using SIP @b INVITE method.
 *
 * The INVITE method is used to initiate a call between two parties. The
 * call is also known as <i>SIP session</i>.
 *
 * At SIP level the session is represented as @e Dialog, which is a
 * peer-to-peer association between two SIP User-Agents. The dialog is
 * established by a successful 2XX response to the INVITE. The dialog is
 * terminated by BYE transaction, which application can initiate with
 * nua_bye() call.
 *
 * An @e early @e dialog is established by an preliminary response
 * (101..199), such as <i>180 Ringing</i>. An early dialog is terminated
 * with an error response with response code in range 300...699.
 *
 * The media session belonging to the SIP session is usually represented by
 * SDP, Session Description Protocol. The media session it is usually
 * established during the call set-up with procedure known as SDP
 * Offer/Answer exchange, defined by @RFC3264. See <b>Media Session
 * Handling</b> below for details.
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *    nothing
 *
 * @par Events:
 *    #nua_r_invite \n
 *    #nua_i_state (#nua_i_active, #nua_i_terminated) \n
 *    #nua_i_media_error \n
 *    #nua_i_fork \n
 *
 * @par Tags:
 *   NUTAG_AUTH_CACHE() \n
 *   NUTAG_AUTOACK() \n
 *   NUTAG_AUTOANSWER() \n
 *   NUTAG_EARLY_MEDIA() \n
 *   NUTAG_ENABLEINVITE() \n
 *   NUTAG_INITIAL_ROUTE(), NUTAG_INITIAL_ROUTE_STR() \n
 *   NUTAG_INVITE_TIMER() \n
 *   NUTAG_MEDIA_ENABLE() \n
 *   NUTAG_MEDIA_FEATURES() \n
 *   NUTAG_MIN_SE() \n
 *   NUTAG_RETRY_COUNT() \n
 *   NUTAG_SERVICE_ROUTE_ENABLE() \n
 *   NUTAG_SESSION_REFRESHER() \n
 *   NUTAG_SESSION_TIMER() \n
 *   NUTAG_SOA_NAME() \n
 *   NUTAG_UPDATE_REFRESH() \n
 *
 * @par Populating SIP Request Message with Tagged Arguments
 * The tagged arguments can be used to pass values for any SIP headers to
 * the stack. When the INVITE message (or any other SIP message) is created,
 * the tagged values saved with nua_handle() are used first, next the tagged
 * values given with the operation (nua_invite()) are added.
 *
 * @par
 * When multiple tags for the same header are specified, the behaviour
 * depends on the header type. If only a single header field can be included
 * in a SIP message, the latest non-NULL value is used, e.g., @Subject.
 * However, if the SIP header can consist of multiple lines or header fields
 * separated by comma, e.g., @Accept, all the tagged
 * values are concatenated.
 *
 * @par
 * However, if a tag value is #SIP_NONE (-1 casted as a void pointer), the
 * values from previous tags are ignored.
 *
 * @par
 * Next, values previously set with nua_set_params() or nua_set_hparams()
 * are used: @Allow, @Supported, @Organization, and @UserAgent headers are
 * added to the request if they are not already set.
 *
 * @par
 * Now, the target URI for the request needs to be determined.
 *
 * @par
 * For initial INVITE requests, values from tags are used. If NUTAG_URL() is
 * given, it is used as target URI. Otherwise, if SIPTAG_TO() is given, it
 * is used as target URI. If neither is given, the complete request line
 * already specified using SIPTAG_REQUEST() or SIPTAG_REQUEST_STR() is used.
 * If none of the tags above are given, an internal error is returned to the
 * application. At this point, the target URI is stored in the request line,
 * together with method name ("INVITE") and protocol version ("SIP/2.0").
 * The initial dialog information is also created: @CallID, @CSeq headers
 * are generated, if they do not exist, and an unique tag is added to @From
 * header.
 *
 * @par
 * For the initial INVITE requests, the @Route headers specified by
 * SIPTAG_ROUTE()/SIPTAG_ROUTER_STR() tags in nua_handle() and nua_invite()
 * calls are inserted to the request. Next the initial route set specified
 * by NUTAG_INITIAL_ROUTE()/NUTAG_INITIAL_ROUTE_STR() tags is prepended to
 * the route. Finally (unless NUTAG_SERVICE_ROUTE_ENABLE(0) is used) the
 * @ServiceRoute set received from the registrar is also appended to the
 * route set of the initial request message.
 *
 * @par
 * Next, the stack generates a @Contact header for the request (Unless the
 * application already gave a @Contact header or it does not want to use
 * @Contact and indicates that by including SIPTAG_CONTACT(NULL) or
 * SIPTAG_CONTACT(SIP_NONE) in the tagged parameters.) If the application
 * has a registration active, the @Contact header used with registration is
 * used. Otherwise, the @Contact header is generated from the local IP
 * address and port number, taking also the values from NUTAG_M_DISPLAY(),
 * NUTAG_M_FEATURES(), NUTAG_M_PARAMS(), and NUTAG_M_USERNAME().
 *
 * @par
 * For in-dialog INVITE (re-INVITE), the request URI is taken from the
 * @Contact header received from the remote party during the dialog
 * establishment. Also, the @CallID and @CSeq headers and @From and @To tags
 * are generated based on the dialog information and added to the request.
 * If the dialog has a route (set by @RecordRoute headers), it is added to
 * the request, too.
 *
 * @par
 * @MaxForwards header (with default value set by NTATAG_MAX_FORWARDS()) is
 * also added now, if it does not exist.
 *
 * @par
 * The INVITE request message created by nua_invite() operation is saved as
 * a template for automatic re-INVITE requests sent by the session timer
 * ("timer") feature (see NUTAG_SESSION_TIMER() for more details). Please
 * note that the template message is not used when ACK, PRACK, UPDATE or
 * INFO requests are created (however, these requests will include
 * dialog-specific headers like @To, @From, and @CallID as well as
 * preference headers @Allow, @Supported, @UserAgent, @Organization).
 *
 * @par Tags Related to SIP Headers and Request-URI
 *    NUTAG_URL(), SIPTAG_REQUEST(), SIPTAG_REQUEST_STR() \n
 *    NUTAG_INITIAL_ROUTE(), NUTAG_INITIAL_ROUTE_STR(),
 *    SIPTAG_ROUTE(), SIPTAG_ROUTE_STR(),
 *    NUTAG_SERVICE_ROUTE_ENABLE() \n
 *    SIPTAG_MAX_FORWARDS(), SIPTAG_MAX_FORWARDS_STR() \n
 *    SIPTAG_PROXY_REQUIRE(), SIPTAG_PROXY_REQUIRE_STR() \n
 *    SIPTAG_FROM(), SIPTAG_FROM_STR() \n
 *    SIPTAG_TO(), SIPTAG_TO_STR() \n
 *    SIPTAG_CALL_ID(), SIPTAG_CALL_ID_STR() \n
 *    SIPTAG_CSEQ(), SIPTAG_CSEQ_STR()
 *    (note that @CSeq value is incremented if request gets retried)\n
 *    SIPTAG_CONTACT(), SIPTAG_CONTACT_STR() \n
 *    SIPTAG_REQUEST_DISPOSITION(), SIPTAG_REQUEST_DISPOSITION_STR() \n
 *    SIPTAG_ACCEPT_CONTACT(), SIPTAG_ACCEPT_CONTACT_STR() \n
 *    SIPTAG_REJECT_CONTACT(), SIPTAG_REJECT_CONTACT_STR() \n
 *    SIPTAG_EXPIRES(), SIPTAG_EXPIRES_STR() \n
 *    SIPTAG_DATE(), SIPTAG_DATE_STR() \n
 *    SIPTAG_TIMESTAMP(), SIPTAG_TIMESTAMP_STR() \n
 *    SIPTAG_SUBJECT(), SIPTAG_SUBJECT_STR() \n
 *    SIPTAG_PRIORITY(), SIPTAG_PRIORITY_STR() \n
 *    SIPTAG_CALL_INFO(), SIPTAG_CALL_INFO_STR() \n
 *    SIPTAG_ORGANIZATION(), SIPTAG_ORGANIZATION_STR() \n
 *    NUTAG_USER_AGENT(), SIPTAG_USER_AGENT() and SIPTAG_USER_AGENT_STR() \n
 *    SIPTAG_IN_REPLY_TO(), SIPTAG_IN_REPLY_TO_STR() \n
 *    SIPTAG_ACCEPT(), SIPTAG_ACCEPT_STR() \n
 *    SIPTAG_ACCEPT_ENCODING(), SIPTAG_ACCEPT_ENCODING_STR() \n
 *    SIPTAG_ACCEPT_LANGUAGE(), SIPTAG_ACCEPT_LANGUAGE_STR() \n
 *    NUTAG_ALLOW(), SIPTAG_ALLOW(), and SIPTAG_ALLOW_STR() \n
 *    NUTAG_EARLY_MEDIA(), SIPTAG_REQUIRE(), and SIPTAG_REQUIRE_STR() \n
 *    NUTAG_SUPPORTED(), SIPTAG_SUPPORTED(), and SIPTAG_SUPPORTED_STR() \n
 *    SIPTAG_ALLOW_EVENTS(), SIPTAG_ALLOW_EVENTS_STR() \n
 *    SIPTAG_PROXY_AUTHORIZATION(), SIPTAG_PROXY_AUTHORIZATION_STR() \n
 *    SIPTAG_AUTHORIZATION(), SIPTAG_AUTHORIZATION_STR() \n
 *    SIPTAG_REFERRED_BY(), SIPTAG_REFERRED_BY_STR() \n
 *    SIPTAG_REPLACES(), SIPTAG_REPLACES_STR() \n
 *    NUTAG_SESSION_TIMER(), NUTAG_SESSION_REFRESHER(),
 *    SIPTAG_SESSION_EXPIRES(), SIPTAG_SESSION_EXPIRES_STR() \n
 *    NUTAG_MIN_SE(), SIPTAG_MIN_SE(), SIPTAG_MIN_SE_STR() \n
 *    SIPTAG_SECURITY_CLIENT(), SIPTAG_SECURITY_CLIENT_STR() \n
 *    SIPTAG_SECURITY_VERIFY(), SIPTAG_SECURITY_VERIFY_STR() \n
 *    SIPTAG_PRIVACY(), SIPTAG_PRIVACY_STR() \n
 *    SIPTAG_MIME_VERSION(), SIPTAG_MIME_VERSION_STR() \n
 *    SIPTAG_CONTENT_TYPE(), SIPTAG_CONTENT_TYPE_STR() \n
 *    SIPTAG_CONTENT_ENCODING(), SIPTAG_CONTENT_ENCODING_STR() \n
 *    SIPTAG_CONTENT_LANGUAGE(), SIPTAG_CONTENT_LANGUAGE_STR() \n
 *    SIPTAG_CONTENT_DISPOSITION(), SIPTAG_CONTENT_DISPOSITION_STR() \n
 *    SIPTAG_HEADER(), SIPTAG_HEADER_STR() \n
 *    SIPTAG_PAYLOAD(), SIPTAG_PAYLOAD_STR() \n
 *
 * @par SDP Handling
 * By default the nua_invite() uses an @ref soa_session_t "SOA media
 * session" object to take care of the Offer/Answer exchange. The SOA can
 * be disabled with tag NUTAG_MEDIA_ENABLE(0).
 *
 * @par
 * The SDP description of the
 * @ref soa_session_t "soa media session" is included in the INVITE request
 * as a message body.
 * The SDP in the message body of the 1XX or 2XX response message is
 * interpreted as an answer, given to the @ref soa_session_t "soa media
 * session" object for processing.
 *
 * @bug If the INVITE request already contains a message body, SDP is not
 * added.  Also, if the response contains a multipart body, it is not parsed.
 *
 * @par Tags Related to SDP Management and Offer/Answer Model:
 *    NUTAG_MEDIA_ENABLE(), \n
 *    NUTAG_INCLUDE_EXTRA_SDP(), \n
 *    SOATAG_HOLD(), SOATAG_AF(), SOATAG_ADDRESS(),
 *    SOATAG_ORDERED_USER(), SOATAG_REUSE_REJECTED(),
 *    SOATAG_RTP_SELECT(), SOATAG_RTP_SORT(), SOATAG_RTP_MISMATCH(),
 *    SOATAG_AUDIO_AUX(), \n
 *    SOATAG_USER_SDP() or SOATAG_USER_SDP_STR() \n
 *
 * @par Alternative Call Models
 * In addition to the basic SIP call model described in @RFC3261 and
 * @RFC3264, the early media model described in @RFC3262 is available. The
 * use of 100rel and early media can be use can be forced with
 * NUTAG_EARLY_MEDIA(1).
 *
 * Also, the "precondition" call model described in @RFC3312 is supported at
 * SIP level, that is, the SIP PRACK and UPDATE requests are sent if
 * "precondition" is added to the @Require header in the INVITE request.
 *
 * Optionally
 * - uses early media if NUTAG_EARLY_MEDIA() tag is used with non zero-value
 * - media parameters can be set by SOA tags
 * - nua_invite() can be used to change status of an existing call:
 *   - #SOATAG_HOLD tag can be used to list the media that will be put on hold,
 *     the value "*" sets all the media beloginging to the session on hold
 *
 * @par Authentication
 * The INVITE request may need authentication. Each proxy or server
 * requiring authentication can respond with 401 or 407 response. The
 * nua_authenticate() operation stores authentication information (username
 * and password) to the handle, and stack tries to authenticate all the rest
 * of the requests (e.g., PRACK, ACK, UPDATE, re-INVITE, BYE) using the
 * stored username and password.
 *
 * @sa @ref nua_call_model, #nua_r_invite, #nua_i_state, \n
 *     nua_handle_has_active_call() \n
 *     nua_handle_has_call_on_hold()\n
 *     nua_handle_has_invite() \n
 *     nua_authenticate() \n
 *     nua_prack() \n
 *     nua_update() \n
 *     nua_info() \n
 *     nua_cancel() \n
 *     nua_bye() \n
 *     #nua_i_invite, nua_respond()
 */

/* Tags not implemented
 *    NUTAG_REFER_PAUSE() \n
 */

static int nua_invite_client_init(nua_client_request_t *cr,
				  msg_t *msg, sip_t *sip,
				  tagi_t const *tags);
static int nua_invite_client_request(nua_client_request_t *cr,
				     msg_t *msg, sip_t *sip,
				     tagi_t const *tags);
static int nua_invite_client_preliminary(nua_client_request_t *cr,
					 int status, char const *phrase,
					 sip_t const *sip);
static int nua_invite_client_response(nua_client_request_t *cr,
				      int status, char const *phrase,
				      sip_t const *sip);
static int nua_session_client_response(nua_client_request_t *cr,
				       int status, char const *phrase,
				       sip_t const *sip);
static int nua_invite_client_report(nua_client_request_t *cr,
				    int status, char const *phrase,
				    sip_t const *sip,
				    nta_outgoing_t *orq,
				    tagi_t const *tags);

nua_client_methods_t const nua_invite_client_methods = {
  SIP_METHOD_INVITE,		/* crm_method, crm_method_name */
  0,				/* crm_extra */
  {				/* crm_flags */
    /* create_dialog */ 1,
    /* in_dialog */ 1,
    /* target refresh */ 1
  },
  NULL,				/* crm_template */
  nua_invite_client_init,	/* crm_init */
  nua_invite_client_request,	/* crm_send */
  session_timer_check_restart,	/* crm_check_restart */
  nua_invite_client_response,	/* crm_recv */
  nua_invite_client_preliminary, /* crm_preliminary */
  nua_invite_client_report,	/* crm_report */
  nua_invite_client_complete,	/* crm_complete */
};

extern nua_client_methods_t const nua_bye_client_methods;
extern nua_client_methods_t const nua_cancel_client_methods;
extern nua_client_methods_t const nua_update_client_methods;
extern nua_client_methods_t const nua_prack_client_methods;

int nua_stack_invite(nua_t *nua, nua_handle_t *nh, nua_event_t e,
		     tagi_t const *tags)
{
  return nua_client_create(nh, e, &nua_invite_client_methods, tags);
}

static int nua_invite_client_init(nua_client_request_t *cr,
				  msg_t *msg, sip_t *sip,
				  tagi_t const *tags)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du;
  nua_session_usage_t *ss;

  cr->cr_usage = du = nua_dialog_usage_for_session(nh->nh_ds);
  /* Errors returned by nua_invite_client_init()
     do not change the session state */
  cr->cr_neutral = 1;

  if (nh_is_special(nh) ||
      nua_stack_set_handle_special(nh, nh_has_invite, nua_i_error))
    return nua_client_return(cr, 900, "Invalid handle for INVITE", msg);
  else if (nh_referral_check(nh, tags) < 0)
    return nua_client_return(cr, 900, "Invalid referral", msg);

  if (du) {
    nua_server_request_t *sr;
    for (sr = nh->nh_ds->ds_sr; sr; sr = sr->sr_next)
      /* INVITE in progress? */
      if (sr->sr_usage == du && sr->sr_method == sip_method_invite &&
	  nua_server_request_is_pending(sr))
	return nua_client_return(cr, SIP_491_REQUEST_PENDING, msg);
    cr->cr_initial = 0;
  }
  else {
    du = nua_dialog_usage_add(nh, nh->nh_ds, nua_session_usage, NULL);
    cr->cr_initial = 1;
  }

  if (!du)
    return -1;

  ss = nua_dialog_usage_private(du);

  if (ss->ss_state >= nua_callstate_terminating)
    return nua_client_return(cr, 900, "Session is terminating", msg);

  if (nua_client_bind(cr, du) < 0)
    return nua_client_return(cr, 900, "INVITE already in progress", msg);

  cr->cr_neutral = 0;

  session_timer_preferences(ss->ss_timer,
			    sip,
			    NH_PGET(nh, supported),
			    NH_PGET(nh, session_timer),
			    NUA_PISSET(nh->nh_nua, nh, session_timer),
			    NH_PGET(nh, refresher),
			    NH_PGET(nh, min_se));

  return 0;
}

static int nua_invite_client_request(nua_client_request_t *cr,
				     msg_t *msg, sip_t *sip,
				     tagi_t const *tags)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du = cr->cr_usage;
  nua_session_usage_t *ss;
  int offer_sent = 0, retval;
  sip_time_t invite_timeout;

  if (du == NULL)		/* Call terminated */
    return nua_client_return(cr, SIP_481_NO_TRANSACTION, msg);

  ss = NUA_DIALOG_USAGE_PRIVATE(du);

  if (ss->ss_state >= nua_callstate_terminating)
    return nua_client_return(cr, 900, "Session is terminating", msg);

  invite_timeout = NH_PGET(nh, invite_timeout);
  if (invite_timeout == 0)
    invite_timeout = UINT_MAX;
  /* Send CANCEL if we don't get response within timeout*/
  /* nua_dialog_usage_set_expires(du, invite_timeout); Xyzzy */
  nua_dialog_usage_reset_refresh(du);

  /* Add session timer headers */
  if (session_timer_is_supported(ss->ss_timer))
    session_timer_add_headers(ss->ss_timer, ss->ss_state == nua_callstate_init,
				  msg, sip, nh);

  ss->ss_100rel = NH_PGET(nh, early_media);
  ss->ss_precondition = sip_has_feature(sip->sip_require, "precondition");
  if (ss->ss_precondition)
    ss->ss_update_needed = ss->ss_100rel = 1;

  if (nh->nh_soa) {
    soa_init_offer_answer(nh->nh_soa);

    if (sip->sip_payload)
      offer_sent = 0;		/* XXX - kludge */
    else if (soa_generate_offer(nh->nh_soa, 0, NULL) < 0)
      return -1;
    else
      offer_sent = 1;

    if (offer_sent > 0 &&
	session_include_description(nh->nh_soa, 1, msg, sip) < 0)
      return nua_client_return(cr, 900, "Internal media error", msg);

    if (NH_PGET(nh, media_features) &&
	!nua_dialog_is_established(nh->nh_ds) &&
	!sip->sip_accept_contact && !sip->sip_reject_contact) {
      sip_accept_contact_t ac[1];
      sip_accept_contact_init(ac);

      ac->cp_params = (msg_param_t *)
	soa_media_features(nh->nh_soa, 1, msg_home(msg));

      if (ac->cp_params) {
	msg_header_replace_param(msg_home(msg), ac->cp_common, "explicit");
	sip_add_dup(msg, sip, (sip_header_t *)ac);
      }
    }
  }
  else {
    offer_sent = session_get_description(sip, NULL, NULL);
  }

  retval = nua_base_client_trequest(cr, msg, sip,
				    NTATAG_REL100(ss->ss_100rel),
				    TAG_NEXT(tags));
  if (retval == 0) {
    if ((cr->cr_offer_sent = offer_sent))
      ss->ss_oa_sent = Offer;

    if (!cr->cr_restarting) /* Restart logic calls nua_invite_client_report */
      signal_call_state_change(nh, ss, 0, "INVITE sent",
			       nua_callstate_calling);
  }

  return retval;
}

static int nua_invite_client_response(nua_client_request_t *cr,
				      int status, char const *phrase,
				      sip_t const *sip)
{
  nua_dialog_usage_t *du = cr->cr_usage;
  nua_session_usage_t *ss = nua_dialog_usage_private(du);
  int uas;

  if (ss == NULL || sip == NULL) {
    /* Xyzzy */
  }
  else if (status < 300) {
    du->du_ready = 1;

    if (session_timer_is_supported(ss->ss_timer))
      session_timer_store(ss->ss_timer, sip);

    session_timer_set(ss, uas = 0);
  }

  return nua_session_client_response(cr, status, phrase, sip);
}

static int nua_invite_client_preliminary(nua_client_request_t *cr,
					 int status, char const *phrase,
					 sip_t const *sip)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du = cr->cr_usage;
  nua_session_usage_t *ss = nua_dialog_usage_private(du);

  assert(sip);

  if (ss && sip && sip->sip_rseq) {
    /* Handle 100rel responses */
    sip_rseq_t *rseq = sip->sip_rseq;

    /* Establish early dialog - we should fork here */
    if (!nua_dialog_is_established(nh->nh_ds)) {
      nta_outgoing_t *tagged;

      nua_dialog_uac_route(nh, nh->nh_ds, sip, 1, 1);
      nua_dialog_store_peer_info(nh, nh->nh_ds, sip);

      /* Tag the INVITE request */
      tagged = nta_outgoing_tagged(cr->cr_orq,
				   nua_client_orq_response, cr,
				   sip->sip_to->a_tag, sip->sip_rseq);
      if (tagged) {
	nta_outgoing_destroy(cr->cr_orq), cr->cr_orq = tagged;
      }
      else {
	cr->cr_graceful = 1;
	ss->ss_reason = "SIP;cause=500;text=\"Cannot Create Early Dialog\"";
      }
    }

    if (!rseq) {
      SU_DEBUG_5(("nua(%p): 100rel missing RSeq\n", (void *)nh));
    }
    else if (nta_outgoing_rseq(cr->cr_orq) > rseq->rs_response) {
      SU_DEBUG_5(("nua(%p): 100rel bad RSeq %u (got %u)\n", (void *)nh,
		  (unsigned)rseq->rs_response,
		  nta_outgoing_rseq(cr->cr_orq)));
      return 1;    /* Do not send event */
    }
    else if (nta_outgoing_setrseq(cr->cr_orq, rseq->rs_response) < 0) {
      SU_DEBUG_1(("nua(%p): cannot set RSeq %u\n", (void *)nh,
		  (unsigned)rseq->rs_response));
      cr->cr_graceful = 1;
      ss->ss_reason = "SIP;cause=400;text=\"Bad RSeq\"";
    }
  }

  return nua_session_client_response(cr, status, phrase, sip);
}

/** Process response to a session request (INVITE, PRACK, UPDATE) */
static int nua_session_client_response(nua_client_request_t *cr,
				       int status, char const *phrase,
				       sip_t const *sip)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du = cr->cr_usage;
  nua_session_usage_t *ss = nua_dialog_usage_private(du);

  char const *sdp = NULL;
  size_t len;
  char const *received = NULL;

#define LOG3(m) \
  SU_DEBUG_3(("nua(%p): %s: %s %s in %u %s\n", \
	      (void *)nh, cr->cr_method_name, (m), \
	      received ? received : "SDP", status, phrase))
#define LOG5(m) \
  SU_DEBUG_5(("nua(%p): %s: %s %s in %u %s\n", \
	      (void *)nh, cr->cr_method_name, (m), received, status, phrase))

 retry:

  if (!ss || !sip || 300 <= status)
    /* Xyzzy */;
  else if (!session_get_description(sip, &sdp, &len))
    /* No SDP */;
  else if (cr->cr_answer_recv) {
    /* Ignore spurious answers after completing O/A */
	  //LOG3("ignoring duplicate");
	  //sdp = NULL;
	  // we need to make sure its *actually* a dup, so we can't assume for now.
	  cr->cr_answer_recv = 0;
	  goto retry;
  }
  else if (cr->cr_offer_sent) {
    /* case 1: answer to our offer */
    cr->cr_answer_recv = status;
    received = Answer;

    if (nh->nh_soa == NULL)
      LOG5("got SDP");
    else if (soa_set_remote_sdp(nh->nh_soa, NULL, sdp, len) < 0) {
      LOG3("error parsing SDP");
      sdp = NULL;
      cr->cr_graceful = 1;
      ss->ss_reason = "SIP;cause=400;text=\"Malformed Session Description\"";
    }
    else if (soa_process_answer(nh->nh_soa, NULL) < 0) {
      LOG5("error processing SDP");
      /* XXX */
      sdp = NULL;
    }
    else if (soa_activate(nh->nh_soa, NULL) < 0) {
      /* XXX - what about errors? */
      LOG3("error activating media after");
    }
    else {
      ss->ss_sdp_version = soa_get_user_version(nh->nh_soa);
      LOG5("processed SDP");
    }
  }
  else if (cr->cr_method != sip_method_invite) {
    /* If non-invite request did not have offer, ignore SDP in response */
    LOG3("ignoring extra");
    sdp = NULL;
  }
  else {
    /* case 2: new offer */
    cr->cr_offer_recv = 1, cr->cr_answer_sent = 0;
    received = Offer;

    if (nh->nh_soa && soa_set_remote_sdp(nh->nh_soa, NULL, sdp, len) < 0) {
      LOG3("error parsing SDP");
      sdp = NULL;
      cr->cr_graceful = 1;
      ss->ss_reason = "SIP;cause=400;text=\"Malformed Session Description\"";
    }
    else
      LOG5("got SDP");
  }

  if (ss && received)
    ss->ss_oa_recv = received;

  if (sdp && nh->nh_soa)
    return nua_base_client_tresponse(cr, status, phrase, sip,
				     NH_REMOTE_MEDIA_TAGS(1, nh->nh_soa),
				     TAG_END());
  else
    return nua_base_client_response(cr, status, phrase, sip, NULL);
}

static int nua_invite_client_report(nua_client_request_t *cr,
				    int status, char const *phrase,
				    sip_t const *sip,
				    nta_outgoing_t *orq,
				    tagi_t const *tags)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_state_t *ds = nh->nh_ds;
  nua_dialog_usage_t *du = cr->cr_usage;
  nua_session_usage_t *ss = nua_dialog_usage_private(du);
  msg_t *response = nta_outgoing_getresponse(orq);
  unsigned next_state;
  int error;

  nh_referral_respond(nh, status, phrase); /* XXX - restarting after 401/407 */

  nua_stack_event(nh->nh_nua, nh,
		  response,
		  (enum nua_event_e)cr->cr_event,
		  status, phrase,
		  tags);

  if (cr->cr_waiting)
    /* Do not report call state change if waiting for restart */
    return 1;

  if (ss == NULL) {
    signal_call_state_change(nh, ss, status, phrase, nua_callstate_terminated);
    return 1;
  }

  ss->ss_reporting = 1;

  if (cr->cr_neutral) {
    signal_call_state_change(nh, ss, status, phrase, ss->ss_state);
    ss->ss_reporting = 0;
    return 1;
  }

  response = msg_ref_create(response); /* Keep reference to contents of sip */

  if (orq != cr->cr_orq && cr->cr_orq) {	/* Being restarted */
    next_state = nua_callstate_calling;
  }
  else if (status == 100) {
    next_state = nua_callstate_calling;
  }
  else if (status < 300 && cr->cr_graceful) {
    next_state = nua_callstate_terminating;
    if (200 <= status) {
      nua_invite_client_ack(cr, NULL);
    }
  }
  else if (status < 200) {
    next_state = nua_callstate_proceeding;

    if (sip && sip->sip_rseq &&
	!SIP_IS_ALLOWED(NH_PGET(nh, appl_method), sip_method_prack)) {
      sip_rack_t rack[1];

      sip_rack_init(rack);
      rack->ra_response    = sip->sip_rseq->rs_response;
      rack->ra_cseq        = sip->sip_cseq->cs_seq;
      rack->ra_method      = sip->sip_cseq->cs_method;
      rack->ra_method_name = sip->sip_cseq->cs_method_name;

      error = nua_client_tcreate(nh, nua_r_prack, &nua_prack_client_methods,
				 SIPTAG_RACK(rack),
				 TAG_END());
      if (error < 0) {
	cr->cr_graceful = 1;
	next_state = nua_callstate_terminating;
      }
    }
  }
  else if (status < 300) {
    next_state = nua_callstate_completing;
  }
  else if (cr->cr_terminated) {
    next_state = nua_callstate_terminated;
  }
  else if (cr->cr_graceful && ss->ss_state >= nua_callstate_completing) {
    next_state = nua_callstate_terminating;
  }
  else {
    next_state = nua_callstate_init;
  }

  if (next_state == nua_callstate_calling) {
    if (sip && sip->sip_status && sip->sip_status->st_status == 100) {
      ss->ss_reporting = 0;
      return 1;
    }
  }

  if (next_state == nua_callstate_completing) {
    if (NH_PGET(nh, auto_ack) ||
	/* Auto-ACK response to re-INVITE when media is enabled
	   and auto_ack is not set to 0 on handle */
	(ss->ss_state == nua_callstate_ready && nh->nh_soa &&
	 !NH_PISSET(nh, auto_ack))) {
      nua_client_request_t *cru;

      for (cru = ds->ds_cr; cru; cru = cru->cr_next) {
	if (cr != cru && cru->cr_offer_sent && !cru->cr_answer_recv)
	  break;
      }

      if (cru)
	/* A final response to UPDATE or PRACK with answer on its way? */;
      else if (nua_invite_client_ack(cr, NULL) > 0)
	next_state = nua_callstate_ready;
      else
	next_state = nua_callstate_terminating;
    }
  }

  if (next_state == nua_callstate_terminating) {
    /* Send BYE or CANCEL */
    /* XXX - Forking - send BYE to early dialog?? */
    if (ss->ss_state > nua_callstate_proceeding || status >= 200)
      error = nua_client_create(nh, nua_r_bye, &nua_bye_client_methods, NULL);
    else
      error = nua_client_create(nh, nua_r_cancel,
				&nua_cancel_client_methods, tags);

    if (error) {
      next_state = nua_callstate_terminated;
      cr->cr_terminated = 1;
    }
    cr->cr_graceful = 0;
  }

  ss->ss_reporting = 0;

  signal_call_state_change(nh, ss, status, phrase, (enum nua_callstate)next_state);

  msg_destroy(response);

  return 1;
}

/**@fn void nua_ack(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...);
 *
 * Acknowledge a succesful response to INVITE request.
 *
 * Acknowledge a successful response (200..299) to INVITE request with the
 * SIP ACK request message. This function is needed only if NUTAG_AUTOACK()
 * parameter has been cleared.
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *    nothing
 *
 * @par Related Tags:
 *    Header tags defined in <sofia-sip/sip_tag.h>
 *
 * @par Events:
 *    #nua_i_media_error \n
 *    #nua_i_state  (#nua_i_active, #nua_i_terminated)
 *
 * @sa NUTAG_AUTOACK(), @ref nua_call_model, #nua_i_state
 */

int nua_stack_ack(nua_t *nua, nua_handle_t *nh, nua_event_t e,
		  tagi_t const *tags)
{
  nua_dialog_usage_t *du = nua_dialog_usage_for_session(nh->nh_ds);
  nua_session_usage_t *ss = nua_dialog_usage_private(du);
  nua_client_request_t *cr = du ? du->du_cr : NULL;
  int error;

  if (!cr || cr->cr_orq == NULL || cr->cr_status < 200) {
    UA_EVENT2(nua_i_error, 900, "No response to ACK");
    return 1;
  }

  if (tags)
    nua_stack_set_params(nua, nh, nua_i_error, tags);

  nua_client_request_ref(cr);
  error = nua_invite_client_ack(cr, tags);

  if (error < 0) {
    if (ss->ss_reason == NULL)
      ss->ss_reason = "SIP;cause=500;text=\"Internal Error\"";
    ss->ss_reporting = 1;	/* We report terminated state here if BYE fails */
    error = nua_client_create(nh, nua_r_bye, &nua_bye_client_methods, NULL);
    ss->ss_reporting = 0;
    signal_call_state_change(nh, ss, 500, "Internal Error",
			     error
			     ? nua_callstate_terminated
			     : nua_callstate_terminating);
  }
  else if (ss)
    signal_call_state_change(nh, ss, 200, "ACK sent", nua_callstate_ready);

  nua_client_request_unref(cr);

  return 0;
}

/** Send ACK, destroy INVITE transaction.
 *
 *  @retval 1 if successful
 *  @retval < 0 if an error occurred
 */
static
int nua_invite_client_ack(nua_client_request_t *cr, tagi_t const *tags)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_state_t *ds = nh->nh_ds;
  nua_session_usage_t *ss = nua_dialog_usage_private(cr->cr_usage);

  msg_t *msg;
  sip_t *sip;
  int error = -1;
  sip_authorization_t *wa;
  sip_proxy_authorization_t *pa;
  sip_cseq_t *cseq;
  int proxy_is_set;
  url_string_t *proxy;
  nta_outgoing_t *ack;
  int status = 200;
  char const *phrase = "OK", *reason = NULL;
  char const *invite_branch;

  assert(cr->cr_orq);
  assert(cr->cr_method == sip_method_invite);

  cr->cr_initial = 0;

  if (!ds->ds_leg) {
    /* XXX - fix nua_dialog_usage_remove_at() instead! */
    goto error;
  }

  assert(ds->ds_leg);

  msg = nta_outgoing_getrequest(cr->cr_orq);
  sip = sip_object(msg);
  if (!msg)
    goto error;
  invite_branch = nta_outgoing_branch(cr->cr_orq);

  wa = sip_authorization(sip);
  pa = sip_proxy_authorization(sip);

  msg_destroy(msg);

  msg = nta_msg_create(nh->nh_nua->nua_nta, 0);
  sip = sip_object(msg);
  if (!msg)
    goto error;

  cseq = sip_cseq_create(msg_home(msg), cr->cr_seq, SIP_METHOD_ACK);

  if (!cseq)
    ;
  else if (nh->nh_tags && sip_add_tl(msg, sip, TAG_NEXT(nh->nh_tags)) < 0)
    ;
  else if (tags && sip_add_tl(msg, sip, TAG_NEXT(tags)) < 0)
    ;
  else if (wa && sip_add_dup(msg, sip, (sip_header_t *)wa) < 0)
    ;
  else if (pa && sip_add_dup(msg, sip, (sip_header_t *)pa) < 0)
    ;
  else if (sip_header_insert(msg, sip, (sip_header_t *)cseq) < 0)
    ;
  else if (nta_msg_request_complete(msg, ds->ds_leg, SIP_METHOD_ACK, NULL) < 0)
    ;
  else {
    /* Remove extra headers */
    while (sip->sip_allow)
      sip_header_remove(msg, sip, (sip_header_t*)sip->sip_allow);
    while (sip->sip_priority)
      sip_header_remove(msg, sip, (sip_header_t*)sip->sip_priority);
    while (sip->sip_proxy_require)
      sip_header_remove(msg, sip, (sip_header_t*)sip->sip_proxy_require);
    while (sip->sip_require)
      sip_header_remove(msg, sip, (sip_header_t*)sip->sip_require);
    while (sip->sip_subject)
      sip_header_remove(msg, sip, (sip_header_t*)sip->sip_subject);
    while (sip->sip_supported)
      sip_header_remove(msg, sip, (sip_header_t*)sip->sip_supported);

    if (ss == NULL || ss->ss_state > nua_callstate_ready)
      ;
    else if (cr->cr_offer_recv && !cr->cr_answer_sent) {
      if (nh->nh_soa == NULL) {
	if (session_get_description(sip, NULL, NULL))
	  cr->cr_answer_sent = 1, ss->ss_oa_sent = Answer;
      }
      else if (soa_generate_answer(nh->nh_soa, NULL) < 0 ||
	  session_include_description(nh->nh_soa, 1, msg, sip) < 0) {
	status = 900, phrase = "Internal media error";
	reason = "SIP;cause=500;text=\"Internal media error\"";
	/* reason = soa_error_as_sip_reason(nh->nh_soa); */
      }
      else {
	cr->cr_answer_sent = 1, ss->ss_oa_sent = Answer;
      }
    }

    if (ss == NULL || ss->ss_state > nua_callstate_ready || reason)
      ;
    else if (nh->nh_soa && soa_is_complete(nh->nh_soa)) {
      /* signal SOA that O/A round(s) is (are) complete */
      if (soa_activate(nh->nh_soa, NULL) >= 0) {
	ss->ss_sdp_version = soa_get_user_version(nh->nh_soa);
      }
    }
    else if (nh->nh_soa == NULL
	     /* NUA does not necessarily know dirty details */
	     /* && !(cr->cr_offer_sent && !cr->cr_answer_recv) */) {
      ;
    }
    else {
      nua_client_request_t *cru;

      /* Final response to UPDATE or PRACK may be on its way ... */
      for (cru = ds->ds_cr; cru; cru = cru->cr_next) {
	if (cr != cru && cru->cr_offer_sent && !cru->cr_answer_recv)
	  break;
      }

      if (cru == NULL) {
	/* No SDP answer -> terminate call */
	status = 988, phrase = "Incomplete offer/answer";
	reason = "SIP;cause=488;text=\"Incomplete offer/answer\"";
      }
    }

    proxy_is_set = NH_PISSET(nh, proxy);
    proxy = NH_PGET(nh, proxy);

    if ((ack = nta_outgoing_mcreate(nh->nh_nua->nua_nta, NULL, NULL, NULL,
				    msg,
				    NTATAG_ACK_BRANCH(invite_branch),
				    TAG_IF(proxy_is_set,
					   NTATAG_DEFAULT_PROXY(proxy)),
				    SIPTAG_END(),
				    TAG_NEXT(tags)))) {
      /* TR engine keeps this around for T2 so it catches all 2XX retransmissions  */
      nta_outgoing_destroy(ack);

      if (nh->nh_soa && reason && ss && ss->ss_state <= nua_callstate_ready)
	nua_stack_event(nh->nh_nua, nh, NULL,
			nua_i_media_error, status, phrase,
			NULL);
    }
    else if (!reason) {
      status = 900, phrase = "Cannot send ACK";
      reason = "SIP;cause=500;text=\"Internal Error\"";
    }

    if (ss && reason)
      ss->ss_reason = reason;

    if (status < 300)
      error = 1;
    else
      error = -2;
  }

  if (error == -1)
    msg_destroy(msg);

 error:
  cr->cr_acked = 1;		/* ... or we have at least tried */

  nua_client_request_remove(cr);
  nua_client_request_clean(cr);

  return error;
}

static int
nua_invite_client_should_ack(nua_client_request_t const *cr)
{
  return
    cr && cr->cr_orq && !cr->cr_acked &&
    200 <= cr->cr_status && cr->cr_status < 300;
}

/** Complete client request */
static int nua_invite_client_complete(nua_client_request_t *cr)
{
  if (cr->cr_orq == NULL)
    /* Xyzzy */;
  else if (cr->cr_status < 200)
    nta_outgoing_cancel(cr->cr_orq);
  else if (cr->cr_status < 300 && !cr->cr_acked)
    nua_invite_client_ack(cr, NULL);

  return 0;
}

/**@fn void nua_cancel(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...);
 *
 * Cancel an INVITE operation
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *    nothing
 *
 * @par Related Tags:
 *    Header tags defined in <sofia-sip/sip_tag.h>
 *
 * @par Events:
 *    #nua_r_cancel, #nua_i_state  (#nua_i_active, #nua_i_terminated)
 *
 * @sa @ref nua_call_model, nua_invite(), #nua_i_cancel
 */

static int nua_cancel_client_request(nua_client_request_t *cr,
				     msg_t *msg, sip_t *sip,
				     tagi_t const *tags);
static int nua_cancel_client_check_restart(nua_client_request_t *cr,
					   int status,
					   char const *phrase,
					   sip_t const *sip);

nua_client_methods_t const nua_cancel_client_methods = {
  SIP_METHOD_CANCEL,		/* crm_method, crm_method_name */
  0,				/* crm_extra */
  {				/* crm_flags */
    /* create_dialog */ 0,
    /* in_dialog */ 1,
    /* target refresh */ 0
  },
  NULL,				/* crm_template */
  NULL,				/* crm_init */
  nua_cancel_client_request,	/* .. not really crm_send */
  nua_cancel_client_check_restart, /* crm_check_restart */
  NULL,				/* crm_recv */
  NULL,				/* crm_preliminary */
  NULL,				/* crm_report */
  NULL,				/* crm_complete */
};

int nua_stack_cancel(nua_t *nua, nua_handle_t *nh, nua_event_t e,
		     tagi_t const *tags)
{
  return nua_client_create(nh, e, &nua_cancel_client_methods, tags);
}

static int nua_cancel_client_request(nua_client_request_t *cr,
				     msg_t *msg, sip_t *sip,
				     tagi_t const *tags)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du = nua_dialog_usage_for_session(nh->nh_ds);

  if (!du || !du->du_cr || !du->du_cr->cr_orq ||
      nta_outgoing_status(du->du_cr->cr_orq) >= 200) {
    return nua_client_return(cr, 481, "No transaction to CANCEL", msg);
  }

  assert(cr->cr_orq == NULL);

  cr->cr_orq = nta_outgoing_tcancel(du->du_cr->cr_orq,
				    nua_client_orq_response,
				    nua_client_request_ref(cr),
				    TAG_NEXT(tags));

  if (cr->cr_orq == NULL) {
    nua_client_request_unref(cr);
    return -1;
  }

  return 0;
}

static int
nua_cancel_client_check_restart(nua_client_request_t *cr,
				int status,
				char const *phrase,
				sip_t const *sip)
{
  /* We cannot really restart CANCEL */
  return 0;
}

/** @NUA_EVENT nua_r_cancel
 *
 * Answer to outgoing CANCEL.
 *
 * The CANCEL may be sent explicitly by nua_cancel() or implicitly by NUA
 * state machine.
 *
 * @param status response status code
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the call
 * @param hmagic application context associated with the call
 * @param sip    response to CANCEL request or NULL upon an error
 *               (status code is in @a status and
 *                descriptive message in @a phrase parameters)
 * @param tags   empty
 *
 * @sa nua_cancel(), @ref nua_uac_call_model, #nua_r_invite, nua_invite(),
 * #nua_i_state
 *
 * @END_NUA_EVENT
 */

static void nua_session_usage_refresh(nua_handle_t *nh,
				      nua_dialog_state_t *ds,
				      nua_dialog_usage_t *du,
				      sip_time_t now)
{
  nua_session_usage_t *ss = NUA_DIALOG_USAGE_PRIVATE(du);
  nua_client_request_t const *cr = du->du_cr;
  nua_server_request_t const *sr;

  if (ss->ss_state >= nua_callstate_terminating ||
      /* INVITE is in progress or being authenticated */
      nua_client_request_in_progress(cr))
    return;

  /* UPDATE has been queued */
  for (cr = ds->ds_cr; cr; cr = cr->cr_next)
    if (cr->cr_method == sip_method_update)
      return;

  /* INVITE or UPDATE in progress on server side */
  for (sr = ds->ds_sr; sr; sr = sr->sr_next)
    if (sr->sr_usage == du &&
	(sr->sr_method == sip_method_invite ||
	 sr->sr_method == sip_method_update))
      return;

  if (ss->ss_timer->refresher == nua_remote_refresher) {
    SU_DEBUG_3(("nua(%p): session almost expired, sending BYE before timeout.\n", (void *)nh));
    ss->ss_reason = "SIP;cause=408;text=\"Session timeout\"";
    nua_stack_bye(nh->nh_nua, nh, nua_r_bye, NULL);
    return;
  }
  else if (NH_PGET(nh, update_refresh)) {
    nua_stack_update(nh->nh_nua, nh, nua_r_update, NULL);
  }
  else if (du->du_cr) {
    nua_client_resend_request(du->du_cr, 0);
  }
  else {
    nua_stack_invite(nh->nh_nua, nh, nua_r_invite, NULL);
  }
}

/** @interal Shut down session usage.
 *
 * @retval >0  shutdown done
 * @retval 0   shutdown in progress
 * @retval <0  try again later
 */
static int nua_session_usage_shutdown(nua_handle_t *nh,
				      nua_dialog_state_t *ds,
				      nua_dialog_usage_t *du)
{
  nua_session_usage_t *ss = NUA_DIALOG_USAGE_PRIVATE(du);
  nua_server_request_t *sr, *sr_next;
  nua_client_request_t *cri;

  assert(ss == nua_session_usage_for_dialog(nh->nh_ds));

  /* Zap server-side transactions */
  for (sr = ds->ds_sr; sr; sr = sr_next) {
    sr_next = sr->sr_next;
    if (sr->sr_usage == du) {
      assert(sr->sr_usage == du);
      sr->sr_usage = NULL;

      if (nua_server_request_is_pending(sr)) {
	SR_STATUS1(sr, SIP_480_TEMPORARILY_UNAVAILABLE);
	nua_server_respond(sr, NULL);
	if (nua_server_report(sr) >= 2)
	  return 480;
      }
      else
	nua_server_request_destroy(sr);
    }
  }

  cri = du->du_cr;

  switch (ss->ss_state) {
  case nua_callstate_calling:
  case nua_callstate_proceeding:
    return nua_client_create(nh, nua_r_cancel, &nua_cancel_client_methods, NULL);

  case nua_callstate_completing:
  case nua_callstate_completed:
  case nua_callstate_ready:
    if (cri && cri->cr_orq) {
      if (cri->cr_status < 200) {
	nua_client_create(nh, nua_r_cancel, &nua_cancel_client_methods, NULL);
      }
      else if (cri->cr_status < 300 && !cri->cr_acked) {
	nua_invite_client_ack(cri, NULL);
      }
    }
    if (nua_client_create(nh, nua_r_bye, &nua_bye_client_methods, NULL) != 0)
      break;

    signal_call_state_change(nh, ss, 487, "BYE sent",
			     nua_callstate_terminating);
    return 0;

  case nua_callstate_terminating:
  case nua_callstate_terminated: /* XXX */
    return 0;

  default:
    break;
  }

  nua_dialog_usage_remove(nh, ds, du, NULL, NULL);

  return 200;
}

/**@fn void nua_prack(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...);
 * Send a PRACK request.
 *
 * PRACK is used to acknowledge receipt of 100rel responses. See @RFC3262.
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *    nothing
 *
 * @par Related Tags:
 *    Tags in <sofia-sip/soa_tag.h>, <sofia-sip/sip_tag.h>.
 *
 * @par Events:
 *    #nua_r_prack
 */

/** @NUA_EVENT nua_r_prack
 *
 * Response to an outgoing @b PRACK request. PRACK request is used to
 * acknowledge reliable preliminary responses and it is usually sent
 * automatically by the nua stack.
 *
 * @param status response status code
 *               (if the request is retried, @a status is 100, the @a
 *               sip->sip_status->st_status contain the real status code
 *               from the response message, e.g., 302, 401, or 407)
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the call
 * @param hmagic application context associated with the call
 * @param sip    response to @b PRACK or NULL upon an error
 *               (status code is in @a status and
 *                descriptive message in @a phrase parameters)
 * @param tags   empty
 *
 * @sa nua_prack(), #nua_i_prack, @RFC3262
 *
 * @END_NUA_EVENT
 */

static int nua_prack_client_init(nua_client_request_t *cr,
				 msg_t *msg, sip_t *sip,
				 tagi_t const *tags);
static int nua_prack_client_request(nua_client_request_t *cr,
				    msg_t *msg, sip_t *sip,
				    tagi_t const *tags);
static int nua_prack_client_response(nua_client_request_t *cr,
				     int status, char const *phrase,
				     sip_t const *sip);
static int nua_prack_client_report(nua_client_request_t *cr,
				   int status, char const *phrase,
				   sip_t const *sip,
				   nta_outgoing_t *orq,
				   tagi_t const *tags);

nua_client_methods_t const nua_prack_client_methods = {
  SIP_METHOD_PRACK,		/* crm_method, crm_method_name */
  0,				/* crm_extra */
  {				/* crm_flags */
    /* create_dialog */ 0,
    /* in_dialog */ 1,
    /* target refresh */ 0
  },
  NULL,				/* crm_template */
  nua_prack_client_init,	/* crm_init */
  nua_prack_client_request,	/* crm_send */
  NULL,				/* crm_check_restart */
  nua_prack_client_response,	/* crm_recv */
  NULL,				/* crm_preliminary */
  nua_prack_client_report,	/* crm_report */
  NULL,				/* crm_complete */
};

int nua_stack_prack(nua_t *nua, nua_handle_t *nh, nua_event_t e,
		     tagi_t const *tags)
{
  return nua_client_create(nh, e, &nua_prack_client_methods, tags);
}

static int nua_prack_client_init(nua_client_request_t *cr,
				 msg_t *msg, sip_t *sip,
				 tagi_t const *tags)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du = nua_dialog_usage_for_session(nh->nh_ds);

  cr->cr_usage = du;

  return 0;
}

static int nua_prack_client_request(nua_client_request_t *cr,
				    msg_t *msg, sip_t *sip,
				    tagi_t const *tags)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du = cr->cr_usage;
  nua_session_usage_t *ss;
  nua_client_request_t *cri;
  int offer_sent = 0, answer_sent = 0, retval;
  int status = 0; char const *phrase = "PRACK Sent";
  //uint32_t rseq = 0;

  if (du == NULL)		/* Call terminated */
    return nua_client_return(cr, SIP_481_NO_TRANSACTION, msg);

  ss = NUA_DIALOG_USAGE_PRIVATE(du);
  if (ss->ss_state >= nua_callstate_terminating)
    return nua_client_return(cr, 900, "Session is terminating", msg);

  cri = du->du_cr;

//  if (sip->sip_rack)
//    rseq = sip->sip_rack->ra_response;

  if (cri->cr_offer_recv && !cri->cr_answer_sent) {
    if (nh->nh_soa == NULL)
      /* It is up to application to handle SDP */
      answer_sent = session_get_description(sip, NULL, NULL);
    else if (sip->sip_payload)
      /* XXX - we should just do MIME in session_include_description() */;
    else if (soa_generate_answer(nh->nh_soa, NULL) < 0 ||
	     session_include_description(nh->nh_soa, 1, msg, sip) < 0) {
      status = soa_error_as_sip_response(nh->nh_soa, &phrase);
      SU_DEBUG_3(("nua(%p): local response to PRACK: %d %s\n",
		  (void *)nh, status, phrase));
      nua_stack_event(nh->nh_nua, nh, NULL,
		      nua_i_media_error, status, phrase,
		      NULL);
      return nua_client_return(cr, status, phrase, msg);
    }
    else {
      answer_sent = 1;
      if (soa_activate(nh->nh_soa, NULL) >= 0) {
	ss->ss_sdp_version = soa_get_user_version(nh->nh_soa);
      }
    }
  }
  else if (nh->nh_soa == NULL) {
    offer_sent = session_get_description(sip, NULL, NULL);
  }
  else {
    /* When 100rel response status was 183 do support for preconditions */
    int send_offer = ss->ss_precondition &&
      cri->cr_status == 183 && cri->cr_offer_sent && cri->cr_answer_recv;

    if (!send_offer) {
      tagi_t const *t = tl_find_last(tags, nutag_include_extra_sdp);
      send_offer = t && t->t_value;
    }

    if (!send_offer) {
    }
    else if (soa_generate_offer(nh->nh_soa, 0, NULL) >= 0 &&
	     session_include_description(nh->nh_soa, 1, msg, sip) >= 0) {
      offer_sent = 1;
    }
    else {
      status = soa_error_as_sip_response(nh->nh_soa, &phrase);
      SU_DEBUG_3(("nua(%p): PRACK offer: %d %s\n", (void *)nh,
		  status, phrase));
      nua_stack_event(nh->nh_nua, nh, NULL,
		      nua_i_media_error, status, phrase, NULL);
      return nua_client_return(cr, status, phrase, msg);
    }
  }

  retval = nua_base_client_request(cr, msg, sip, NULL);

  if (retval == 0) {
    cr->cr_offer_sent = offer_sent;
    cr->cr_answer_sent = answer_sent;

    if (offer_sent)
      ss->ss_oa_sent = Offer;
    else if (answer_sent)
      ss->ss_oa_sent = Answer;

    if (cr->cr_restarting)
      /* Restart logic calls nua_prack_client_report */;
    else if (!cr->cr_auto && (!offer_sent || !answer_sent))
      /* Suppose application know it called nua_prack() */;
    else
      signal_call_state_change(nh, ss, status, phrase, ss->ss_state);
  }

  return retval;
}

static int nua_prack_client_response(nua_client_request_t *cr,
				     int status, char const *phrase,
				     sip_t const *sip)
{
  /* XXX - fatal error cases? */

  return nua_session_client_response(cr, status, phrase, sip);
}

static int nua_prack_client_report(nua_client_request_t *cr,
				   int status, char const *phrase,
				   sip_t const *sip,
				   nta_outgoing_t *orq,
				   tagi_t const *tags)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du = cr->cr_usage;
  nua_session_usage_t *ss = nua_dialog_usage_private(du);
  int acked = 0;

  nua_stack_event(nh->nh_nua, nh,
		  nta_outgoing_getresponse(orq),
		  (enum nua_event_e)cr->cr_event,
		  status, phrase,
		  tags);

  if (!ss || cr->cr_terminated || cr->cr_graceful || cr->cr_waiting)
    return 1;

  if (cr->cr_offer_sent || cr->cr_answer_sent) {
    unsigned next_state = ss->ss_state;

    if (status < 200)
      ;
    else if (nua_invite_client_should_ack(du->du_cr)) {
      /* There is an un-ACK-ed INVITE there */
      assert(du->du_cr->cr_method == sip_method_invite);
      if (NH_PGET(nh, auto_ack) ||
	  /* Auto-ACK response to re-INVITE when media is enabled
	     and auto_ack is not set to 0 on handle */
	  (ss->ss_state == nua_callstate_ready && nh->nh_soa &&
	   !NH_PISSET(nh, auto_ack))) {
	/* There should be no UPDATE with offer/answer
	   if PRACK with offer/answer was ongoing! */
	if (nua_invite_client_ack(du->du_cr, NULL) > 0)
	  next_state = nua_callstate_ready;
	else
	  next_state = nua_callstate_terminating;

	acked = 1;
      }
    }

    signal_call_state_change(nh, ss, status, phrase, (enum nua_callstate)next_state);
  }

  if (acked &&
      nua_client_is_queued(du->du_cr) &&
      du->du_cr->cr_method == sip_method_invite) {
    /* New INVITE was queued - do not send UPDATE */
  }
  else if (ss->ss_update_needed && 200 <= status && status < 300 &&
      !SIP_IS_ALLOWED(NH_PGET(nh, appl_method), sip_method_update))
    nua_client_create(nh, nua_r_update, &nua_update_client_methods, NULL);

  return 1;
}

/* ---------------------------------------------------------------------- */
/* UAS side of INVITE */

/** @NUA_EVENT nua_i_invite
 *
 * Indication of incoming call or re-INVITE request.
 *
 * @param status statuscode of response sent automatically by stack
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with this call
 *               (maybe created for this call)
 * @param hmagic application context associated with this call
 *               (maybe NULL if call handle was created for this call)
 * @param sip    incoming INVITE request
 * @param tags   SOATAG_ACTIVE_AUDIO(), SOATAG_ACTIVE_VIDEO()
 *
 * @par
 * @par Responding to INVITE with nua_respond()
 *
 * If @a status in #nua_i_invite event is below 200, the application should
 * accept or reject the call with nua_respond(). See the @ref nua_call_model
 * for the detailed explanation of various options in call processing at
 * server end.
 *
 * The @b INVITE request takes care of session setup using SDP Offer-Answer
 * negotiation as specified in @RFC3264 (updated in @RFC3262 section 5,
 * @RFC3311, and @RFC3312). The Offer-Answer can be taken care by
 * application (if NUTAG_MEDIA_ENABLE(0) parameter has been set) or by the
 * built-in SDP Offer/Answer engine @soa (by default and when
 * NUTAG_MEDIA_ENABLE(1) parameter has been set). When @soa is enabled, it
 * will take care of parsing the SDP, negotiating the media and codecs, and
 * including the SDP in the SIP message bodies as required by the
 * Offer-Answer model.
 *
 * When @soa is enabled, the SDP in the incoming INVITE is parsed and feed
 * to a #soa_session_t object. The #nua_i_state event sent to the
 * application immediately after #nua_i_invite will contain the parsing
 * results in SOATAG_REMOTE_SDP() and SOATAG_REMOTE_SDP_STR() tags.
 *
 * Note that currently the parser within @nua does not handle MIME
 * multipart. The SDP Offer/Answer engine can get confused if the SDP offer
 * is included in a MIME multipart, therefore such an @b INVITE is rejected
 * with <i>415 Unsupported Media Type</i> error response: the client is
 * expected to retry the INVITE without MIME multipart content.
 *
 * If the call is to be accepted, the application should include the SDP in
 * the 2XX response. If @soa is not disabled with NUTAG_MEDIA_ENABLE(0), the
 * SDP should be included in the SOATAG_USER_SDP() or SOATAG_USER_SDP_STR()
 * parameter given to nua_respond(). If it is disabled, the SDP should be
 * included in the response message using SIPTAG_PAYLOAD() or
 * SIPTAG_PAYLOAD_STR(). Also, the @ContentType should be set using
 * SIPTAG_CONTENT_TYPE() or SIPTAG_CONTENT_TYPE_STR().
 *
 * @par Preliminary Responses and 100rel
 *
 * Call progress can be signaled with preliminary responses (with status
 * code in the range 101..199). It is possible to conclude the SDP
 * Offer-Answer negotiation using preliminary responses, too. If
 * NUTAG_EARLY_ANSWER(1), SOATAG_USER_SDP() or SOATAG_USER_SDP_STR()
 * parameter is included with in a preliminary nua_response(), the SDP
 * answer is generated and sent with the preliminary responses, too.
 *
 * The preliminary responses are sent reliably if feature tag "100rel" is
 * included in the @Require header of the response or if
 * NUTAG_EARLY_MEDIA(1) parameter has been given. The reliably delivery of
 * preliminary responses mean that a sequence number is included in the
 * @RSeq header in the response message and the response message is resent
 * until the client responds with a @b PRACK request with matching sequence
 * number in @RAck header.
 *
 * Note that only the "183" response is sent reliably if the
 * NUTAG_ONLY183_100REL(1) parameter has been given. The reliable
 * preliminary responses are acknowledged with @b PRACK request sent by the
 * client.
 *
 * Note if the SDP offer-answer is completed with the reliable preliminary
 * responses, the is no need to include SDP in 200 OK response (or other 2XX
 * response). However, it the tag NUTAG_INCLUDE_EXTRA_SDP(1) is included
 * with nua_respond(), a copy of the SDP answer generated earlier by @soa is
 * included as the message body.
 *
 * @sa nua_respond(), @ref nua_uas_call_model, #nua_i_state,
 * NUTAG_MEDIA_ENABLE(), SOATAG_USER_SDP(), SOATAG_USER_SDP_STR(),
 * @RFC3262, NUTAG_EARLY_ANSWER(), NUTAG_EARLY_MEDIA(),
 * NUTAG_ONLY183_100REL(),
 * NUTAG_INCLUDE_EXTRA_SDP(),
 * #nua_i_prack, #nua_i_update, nua_update(),
 * nua_invite(), #nua_r_invite
 *
 * @par
 * @par Third Party Call Control
 *
 * When so called 2rd party call control is used, the initial @b INVITE may
 * not contain SDP offer. In that case, the offer is sent by the recipient
 * of the @b INVITE request (User-Agent Server, UAS). The SDP sent in 2XX
 * response (or in a preliminary reliable response) is considered as an
 * offer, and the answer will be included in the @b ACK request sent by the
 * UAC (or @b PRACK in case of preliminary reliable response).
 *
 * @sa @ref nua_3pcc_call_model
 *
 * @END_NUA_EVENT
 */

static int nua_invite_server_init(nua_server_request_t *sr);
static int nua_session_server_init(nua_server_request_t *sr);
static int nua_invite_server_preprocess(nua_server_request_t *sr);
static int nua_invite_server_respond(nua_server_request_t *sr, tagi_t const *);
static int nua_invite_server_is_100rel(nua_server_request_t *, tagi_t const *);
static int nua_invite_server_report(nua_server_request_t *sr, tagi_t const *);

static int
  process_ack_or_cancel(nua_server_request_t *, nta_incoming_t *,
			sip_t const *),
  process_ack(nua_server_request_t *, nta_incoming_t *, sip_t const *),
  process_ack_error(nua_server_request_t *sr, msg_t *ackmsg,
		    int status, char const *phrase, char const *reason),
  process_cancel(nua_server_request_t *, nta_incoming_t *, sip_t const *),
  process_timeout(nua_server_request_t *, nta_incoming_t *),
  process_prack(nua_server_request_t *,
		nta_reliable_t *rel,
		nta_incoming_t *irq,
		sip_t const *sip);

nua_server_methods_t const nua_invite_server_methods =
  {
    SIP_METHOD_INVITE,
    nua_i_invite,		/* Event */
    {
      1,			/* Create dialog */
      0,			/* Initial request */
      1,			/* Target refresh request  */
      1,			/* Add Contact */
    },
    nua_invite_server_init,
    nua_invite_server_preprocess,
    nua_base_server_params,
    nua_invite_server_respond,
    nua_invite_server_report,
  };


/** @internal Preprocess incoming invite - sure we have a valid request.
 *
 * @return 0 if request is valid, or error statuscode otherwise
 */
static int
nua_invite_server_init(nua_server_request_t *sr)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_t *nua = nh->nh_nua;
  nua_session_usage_t *ss;

  sr->sr_neutral = 1;

  if (!NUA_PGET(nua, nh, invite_enable))
    return SR_STATUS1(sr, SIP_403_FORBIDDEN);

  if (nua_session_server_init(sr))
    return sr->sr_status;

  if (sr->sr_usage) {
    /* Existing session - check for overlap and glare */

    nua_server_request_t const *sr0;
    nua_client_request_t const *cr;

    for (sr0 = nh->nh_ds->ds_sr; sr0; sr0 = sr0->sr_next) {
      /* Previous INVITE has not been ACKed */
      if (sr0->sr_method == sip_method_invite)
	break;
      /* Or we have sent offer but have not received an answer */
      if (sr->sr_sdp && sr0->sr_offer_sent && !sr0->sr_answer_recv)
	break;
      /* Or we have received request with offer but not sent an answer */
      if (sr->sr_sdp && sr0->sr_offer_recv && !sr0->sr_answer_sent)
	break;
    }

    if (sr0) {
      /* Overlapping invites - RFC 3261 14.2 */
      return nua_server_retry_after(sr, 500, "Overlapping Requests", 0, 10);
    }

    for (cr = nh->nh_ds->ds_cr; cr; cr = cr->cr_next) {
      if (cr->cr_usage == sr->sr_usage && cr->cr_orq && cr->cr_offer_sent)
	/* Glare - RFC 3261 14.2 and RFC 3311 section 5.2 */
	return SR_STATUS1(sr, SIP_491_REQUEST_PENDING);
    }

    ss = nua_dialog_usage_private(sr->sr_usage);

    if (ss->ss_state < nua_callstate_ready &&
	ss->ss_state != nua_callstate_init) {
      return nua_server_retry_after(sr, 500, "Overlapping Requests 2", 0, 10);
    }
  }

  sr->sr_neutral = 0;

  return 0;
}

/** Initialize session server request.
 *
 * Ensure that the request is valid.
 */
static int
nua_session_server_init(nua_server_request_t *sr)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_t *nua = nh->nh_nua;

  msg_t *msg = sr->sr_response.msg;
  sip_t *sip = sr->sr_response.sip;

  sip_t *request = (sip_t *) sr->sr_request.sip;

  if (!sr->sr_initial)
    sr->sr_usage = nua_dialog_usage_get(nh->nh_ds, nua_session_usage, NULL);

  if (sr->sr_method != sip_method_invite && sr->sr_usage == NULL) {
    /* UPDATE/PRACK sent within an existing dialog? */
    return SR_STATUS(sr, 481, "Call Does Not Exist");
  }
  else if (sr->sr_usage) {
    nua_session_usage_t *ss = nua_dialog_usage_private(sr->sr_usage);
    if (ss->ss_state >= nua_callstate_terminating)
      return SR_STATUS(sr, 481, "Call is being terminated");
  }

  if (nh->nh_soa) {
    sip_accept_t *a = nua->nua_invite_accept;

    /* XXX - soa should know what it supports */
    sip_add_dup(msg, sip, (sip_header_t *)a);

	/* if we see there is a multipart content-type, 
	   parse it into the sip structre and find the SDP and replace it 
	   into the request as the requested content */
	if (request->sip_content_type &&
        su_casenmatch(request->sip_content_type->c_type, "multipart/", 10)) {
		msg_multipart_t *mp, *mpp;

		if (request->sip_multipart) {
			mp = request->sip_multipart;
		} else {
			mp = msg_multipart_parse(nua_handle_home(nh),
									 request->sip_content_type,
									 (sip_payload_t *)request->sip_payload);
			request->sip_multipart = mp;
		}

		if (mp) {
			int sdp = 0;
			
			/* extract the SDP and set the primary content-type and payload to that SDP as if it was the only content so SOA will work */
			for(mpp = mp; mpp; mpp = mpp->mp_next) {
				if (mpp->mp_content_type && mpp->mp_content_type->c_type && 
					mpp->mp_payload && mpp->mp_payload->pl_data && 
					su_casenmatch(mpp->mp_content_type->c_type, "application/sdp", 15)) {

					request->sip_content_type = msg_content_type_dup(nua_handle_home(nh), mpp->mp_content_type);
					
					if (request->sip_content_length) {
						request->sip_content_length->l_length = mpp->mp_payload->pl_len;
					}
					
					request->sip_payload->pl_data = su_strdup(nua_handle_home(nh), mpp->mp_payload->pl_data);
					request->sip_payload->pl_len = mpp->mp_payload->pl_len;

					sdp++;

					break;
				}
			}

			/* insist on the existance of a SDP in the content or refuse the request */
			if (!sdp) {
				return SR_STATUS1(sr, SIP_406_NOT_ACCEPTABLE);
			}
		}
	}


    /* Make sure caller uses application/sdp without compression */
    if (nta_check_session_content(NULL, request, a, TAG_END())) {
      sip_add_make(msg, sip, sip_accept_encoding_class, "");
      return SR_STATUS1(sr, SIP_415_UNSUPPORTED_MEDIA);
    }

    /* Make sure caller accepts application/sdp */
    if (nta_check_accept(NULL, request, a, NULL, TAG_END())) {
      sip_add_make(msg, sip, sip_accept_encoding_class, "");
      return SR_STATUS1(sr, SIP_406_NOT_ACCEPTABLE);
    }
  }

  if (request->sip_session_expires &&
      sip_has_feature(NH_PGET(nh, supported), "timer") &&
      session_timer_check_min_se(msg, sip, request, NH_PGET(nh, min_se))) {
    if (sip->sip_min_se)
      return SR_STATUS1(sr, SIP_422_SESSION_TIMER_TOO_SMALL);
    else
      return SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);
  }

  session_get_description(request, &sr->sr_sdp, &sr->sr_sdp_len);

  return 0;
}

/** Preprocess INVITE.
 *
 * This is called after a handle has been created for an incoming INVITE.
 */
int nua_invite_server_preprocess(nua_server_request_t *sr)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_dialog_state_t *ds = nh->nh_ds;
  nua_session_usage_t *ss;

  sip_t const *request = sr->sr_request.sip;

  assert(sr->sr_status == 100);
  assert(nh != nh->nh_nua->nua_dhandle);

  if (sr->sr_status > 100)
    return sr->sr_status;

  if (nh->nh_soa)
    soa_init_offer_answer(nh->nh_soa);

  if (sr->sr_sdp) {
    if (nh->nh_soa &&
	soa_set_remote_sdp(nh->nh_soa, NULL, sr->sr_sdp, sr->sr_sdp_len) < 0) {
      SU_DEBUG_5(("nua(%p): %s server: error parsing SDP\n", (void *)nh,
		  "INVITE"));
      return SR_STATUS(sr, 400, "Bad Session Description");
    }
    else
      sr->sr_offer_recv = 1;
  }

  /* Add the session usage */
  if (sr->sr_usage == NULL) {
    sr->sr_usage = nua_dialog_usage_add(nh, ds, nua_session_usage, NULL);
    if (sr->sr_usage == NULL)
      return SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);
  }

  ss = nua_dialog_usage_private(sr->sr_usage);

  if (sr->sr_offer_recv)
    ss->ss_oa_recv = Offer;

  ss->ss_100rel = NH_PGET(nh, early_media);
  ss->ss_precondition = sip_has_feature(request->sip_require, "precondition");
  if (ss->ss_precondition)
    ss->ss_100rel = 1;

  session_timer_store(ss->ss_timer, request);

#if 0 /* The glare and overlap tests should take care of this. */
  assert(ss->ss_state >= nua_callstate_completed ||
	 ss->ss_state == nua_callstate_init);
#endif

  if (NH_PGET(nh, auto_answer) ||
      /* Auto-answer to re-INVITE unless auto_answer is set to 0 on handle */
      (ss->ss_state == nua_callstate_ready &&
       /* Auto-answer requires enabled media (soa).
	* XXX - if the re-INVITE modifies the media we should not auto-answer.
	*/
       nh->nh_soa &&
       !NH_PISSET(nh, auto_answer))) {
    SR_STATUS1(sr, SIP_200_OK);
  }
  else if (NH_PGET(nh, auto_alert)) {
    if (ss->ss_100rel &&
	(sip_has_feature(request->sip_supported, "100rel") ||
	 sip_has_feature(request->sip_require, "100rel"))) {
      SR_STATUS1(sr, SIP_183_SESSION_PROGRESS);
    }
    else {
      SR_STATUS1(sr, SIP_180_RINGING);
    }
  }

  return 0;
}


/** @internal Respond to an INVITE request.
 *
 */
static
int nua_invite_server_respond(nua_server_request_t *sr, tagi_t const *tags)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_dialog_usage_t *du = sr->sr_usage;
  nua_session_usage_t *ss = nua_dialog_usage_private(du);
  msg_t *msg = sr->sr_response.msg;
  sip_t *sip = sr->sr_response.sip;

  int reliable = 0, maybe_answer = 0, offer = 0, answer = 0, extra = 0;

  enter;

  if (du == NULL) {
    if (sr->sr_status < 300)
      sr_status(sr, SIP_500_INTERNAL_SERVER_ERROR);
    return nua_base_server_respond(sr, tags);
  }

  if (200 <= sr->sr_status && sr->sr_status < 300) {
    reliable = 1, maybe_answer = 1;
  }
  else if (nua_invite_server_is_100rel(sr, tags)) {
    reliable = 1, maybe_answer = 1;
  }
  else if (!nh->nh_soa || sr->sr_status >= 300) {
    if (sr->sr_neutral)
      return nua_base_server_respond(sr, tags);
  }
  else if (tags && 100 < sr->sr_status && sr->sr_status < 200 &&
	   !NHP_ISSET(nh->nh_prefs, early_answer)) {
    sdp_session_t const *user_sdp = NULL;
    char const *user_sdp_str = NULL;

    tl_gets(tags,
	    SOATAG_USER_SDP_REF(user_sdp),
	    SOATAG_USER_SDP_STR_REF(user_sdp_str),
	    TAG_END());

    maybe_answer = user_sdp || user_sdp_str;
  }
  else {
    maybe_answer = NH_PGET(nh, early_answer);
  }

  if (!nh->nh_soa) {
    if (session_get_description(sip, NULL, NULL)) {
      if (sr->sr_offer_recv)
	answer = 1;
      else if (sr->sr_offer_sent < 2)
	offer = 1;
    }
  }
  else if (sr->sr_status >= 300) {
    soa_clear_remote_sdp(nh->nh_soa);
  }
  else if (sr->sr_offer_sent && !sr->sr_answer_recv)
    /* Wait for answer */;
  else if (sr->sr_offer_recv && sr->sr_answer_sent > 1) {
    /* We have sent answer */
    /* ...  but we may want to send it again */
    tagi_t const *t = tl_find_last(tags, nutag_include_extra_sdp);
    extra = t && t->t_value;
  }
  else if (sr->sr_offer_recv && !sr->sr_answer_sent && maybe_answer) {
    /* Generate answer */
    if (soa_generate_answer(nh->nh_soa, NULL) >= 0 &&
	soa_activate(nh->nh_soa, NULL) >= 0) {
      answer = 1;      /* signal that O/A answer sent (answer to invite) */
      /* ss_sdp_version is updated only after answer is sent reliably */
    }
    /* We have an error! */
    else if (sr->sr_status >= 200) {
      sip_warning_t *warning = NULL;
      int wcode;
      char const *text;
      char const *host = "invalid.";

      sr->sr_status = soa_error_as_sip_response(nh->nh_soa, &sr->sr_phrase);

      wcode = soa_get_warning(nh->nh_soa, &text);

      if (wcode) {
	if (sip->sip_contact)
	  host = sip->sip_contact->m_url->url_host;
	warning = sip_warning_format(msg_home(msg), "%u %s \"%s\"",
				     wcode, host, text);
	sip_header_insert(msg, sip, (sip_header_t *)warning);
      }
    }
    else {
      /* 1xx - we don't have to send answer */
    }
  }
  else if (sr->sr_offer_recv && sr->sr_answer_sent == 1 && maybe_answer) {
    /* The answer was sent unreliably, keep sending it */
    answer = 1;
  }
  else if (!sr->sr_offer_recv && !sr->sr_offer_sent && reliable) {
    if (200 <= sr->sr_status && nua_callstate_ready <= ss->ss_state &&
	NH_PGET(nh, refresh_without_sdp))
      /* This is a re-INVITE without SDP - do not try to send offer in 200 */;
    else
      /* Generate offer */
    if (soa_generate_offer(nh->nh_soa, 0, NULL) < 0)
      sr->sr_status = soa_error_as_sip_response(nh->nh_soa, &sr->sr_phrase);
    else
      offer = 1;
  }

  if (sr->sr_status < 300 && (offer || answer || extra)) {
    if (nh->nh_soa && session_include_description(nh->nh_soa, 1, msg, sip) < 0)
      SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);
    else if (offer)
      sr->sr_offer_sent = 1 + reliable, ss->ss_oa_sent = Offer;
    else if (answer)
      sr->sr_answer_sent = 1 + reliable, ss->ss_oa_sent = Answer;

    if (answer && reliable && nh->nh_soa) {
      ss->ss_sdp_version = soa_get_user_version(nh->nh_soa);
    }
  }

  if (reliable && sr->sr_status < 200) {
    sr->sr_response.msg = NULL, sr->sr_response.sip = NULL;
    if (nta_reliable_mreply(sr->sr_irq, process_prack, sr, msg) == NULL)
      return -1;
    sr->sr_100rel = 1;
    return 0;
  }

  if (200 <= sr->sr_status && sr->sr_status < 300) {
    session_timer_preferences(ss->ss_timer,
			      sip,
			      NH_PGET(nh, supported),
			      NH_PGET(nh, session_timer),
			      NUA_PISSET(nh->nh_nua, nh, session_timer),
			      NH_PGET(nh, refresher),
			      NH_PGET(nh, min_se));

    if (session_timer_is_supported(ss->ss_timer))
	  session_timer_add_headers(ss->ss_timer, 0, msg, sip, nh);
  }

  return nua_base_server_respond(sr, tags);
}

/** Check if the response should be sent reliably.
 * XXX - use tags to indicate when to use reliable responses ???
 */
static
int nua_invite_server_is_100rel(nua_server_request_t *sr, tagi_t const *tags)
{
  nua_handle_t *nh = sr->sr_owner;
  sip_require_t *require = sr->sr_request.sip->sip_require;
  sip_supported_t *supported = sr->sr_request.sip->sip_supported;

  if (sr->sr_status >= 200)
    return 0;
  else if (sr->sr_status == 100)
    return 0;

  if (sip_has_feature(sr->sr_response.sip->sip_require, "100rel"))
    return 1;

  if (require == NULL && supported == NULL)
    return 0;

  if (!sip_has_feature(NH_PGET(nh, supported), "100rel"))
    return 0;
  if (sip_has_feature(require, "100rel"))
    return 1;
  if (!sip_has_feature(supported, "100rel"))
    return 0;
  if (sr->sr_status == 183)
    return 1;

  if (NH_PGET(nh, early_media) && !NH_PGET(nh, only183_100rel))
    return 1;

  if (sip_has_feature(require, "precondition")) {
    if (!NH_PGET(nh, only183_100rel))
      return 1;
    if (sr->sr_offer_recv && !sr->sr_answer_sent)
      return 1;
  }

  return 0;
}


int nua_invite_server_report(nua_server_request_t *sr, tagi_t const *tags)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_dialog_usage_t *du = sr->sr_usage;
  nua_session_usage_t *ss = nua_dialog_usage_private(sr->sr_usage);
  int initial = sr->sr_initial && !sr->sr_event;
  int neutral = sr->sr_neutral;
  int application = sr->sr_application;
  int status = sr->sr_status; char const *phrase = sr->sr_phrase;
  int retval;

  if (!sr->sr_event && status < 300) {	/* Not reported yet */
    nta_incoming_bind(sr->sr_irq, process_ack_or_cancel, sr);
  }

  retval = nua_base_server_report(sr, tags), sr = NULL; /* destroys sr */

  if (retval >= 2 || ss == NULL) {
    /* Session has been terminated. */
    if (!initial && !neutral) {
#if 0
      signal_call_state_change(nh, NULL, status, phrase,
			       nua_callstate_terminated);
#endif
    }
    return retval;
  }

  /* Update session state */
  if (status < 300 || application != 0) {
    assert(ss->ss_state != nua_callstate_calling);
    assert(ss->ss_state != nua_callstate_proceeding);
    signal_call_state_change(nh, ss, status, phrase,
			     status >= 300
			     ? nua_callstate_init
			     : status >= 200
			     ? nua_callstate_completed
			     : status > 100
			     ? nua_callstate_early
			     : nua_callstate_received);
  }

  if (status == 180)
    ss->ss_alerting = 1;
  else if (status >= 200)
    ss->ss_alerting = 0;

  if (200 <= status && status < 300) {
     du->du_ready = 1;
  }
  else if (300 <= status && !neutral) {
    if (nh->nh_soa)
      soa_init_offer_answer(nh->nh_soa);
  }

  if (ss->ss_state == nua_callstate_init) {
    assert(status >= 300);
    nua_session_usage_destroy(nh, ss);
  }

  return retval;
}

/** @internal Process ACK or CANCEL or timeout (no ACK) for incoming INVITE */
static
int process_ack_or_cancel(nua_server_request_t *sr,
			  nta_incoming_t *irq,
			  sip_t const *sip)
{
  enter;

  assert(sr->sr_usage);
  assert(sr->sr_usage->du_class == nua_session_usage);

  if (sip && sip->sip_request->rq_method == sip_method_ack)
    return process_ack(sr, irq, sip);
  else if (sip && sip->sip_request->rq_method == sip_method_cancel)
    return process_cancel(sr, irq, sip);
  else
    return process_timeout(sr, irq);
}

/** @NUA_EVENT nua_i_ack
 *
 * Final response to INVITE has been acknowledged by UAC with ACK.
 *
 * @note This event is only sent after 2XX response.
 *
 * @param nh     operation handle associated with the call
 * @param hmagic application context associated with the call
 * @param sip    incoming ACK request
 * @param tags   empty
 *
 * @sa #nua_i_invite, #nua_i_state, @ref nua_uas_call_model, nua_ack()
 *
 * @END_NUA_EVENT
 */
static
int process_ack(nua_server_request_t *sr,
		nta_incoming_t *irq,
		sip_t const *sip)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_session_usage_t *ss = nua_dialog_usage_private(sr->sr_usage);
  msg_t *msg = nta_incoming_getrequest_ackcancel(irq);
  char const *recv = NULL;
  int uas;

  if (ss == NULL)
    return 0;

  if (sr->sr_offer_sent && !sr->sr_answer_recv) {
    char const *sdp;
    size_t len;

    if (session_get_description(sip, &sdp, &len))
      recv = Answer;

    if (recv) {
      assert(ss->ss_oa_recv == NULL);
      ss->ss_oa_recv = recv;
    }

    if (nh->nh_soa == NULL)
      ;
    else if (recv == NULL ) {
      if (ss->ss_state >= nua_callstate_ready &&
	  soa_get_user_version(nh->nh_soa) == ss->ss_sdp_version &&
	  soa_process_reject(nh->nh_soa, NULL) >= 0) {
	url_t const *m;

	/* The re-INVITE was a refresh and re-INVITEr ignored our offer */
	ss->ss_oa_sent = NULL;

	if (sr->sr_request.sip->sip_contact)
	  m = sr->sr_request.sip->sip_contact->m_url;
	else
	  m = sr->sr_request.sip->sip_from->a_url;

	SU_DEBUG_3(("nua(%p): re-INVITEr ignored offer in our %u response "
		    "(Contact: <" URL_PRINT_FORMAT  ">)\n",
		    (void *)nh, sr->sr_status, URL_PRINT_ARGS(m)));
	if (sr->sr_request.sip->sip_user_agent)
	  SU_DEBUG_3(("nua(%p): re-INVITE: \"User-Agent: %s\"\n", (void *)nh,
		      sr->sr_request.sip->sip_user_agent->g_string));
      }
      else
	return process_ack_error(sr, msg, 488, "Offer-Answer error",
				 "SIP;cause=488;text=\"No answer to offer\"");
    }
    else if (soa_set_remote_sdp(nh->nh_soa, NULL, sdp, len) >= 0 &&
	     soa_process_answer(nh->nh_soa, NULL) >= 0 &&
	     soa_activate(nh->nh_soa, NULL) >= 0) {
      ss->ss_sdp_version = soa_get_user_version(nh->nh_soa);
    }
    else {
      int status; char const *phrase, *reason;

      status = soa_error_as_sip_response(nh->nh_soa, &phrase);
      reason = soa_error_as_sip_reason(nh->nh_soa);

      return process_ack_error(sr, msg, status, phrase, reason);
    }
  }

  if (nh->nh_soa)
    soa_clear_remote_sdp(nh->nh_soa);

  nua_stack_event(nh->nh_nua, nh, msg, nua_i_ack, SIP_200_OK, NULL);
  signal_call_state_change(nh, ss, 200, "OK", nua_callstate_ready);
  session_timer_set(ss, uas = 1);

  nua_server_request_destroy(sr);

  return 0;
}

static int
process_ack_error(nua_server_request_t *sr,
		  msg_t *ackmsg,
		  int status,
		  char const *phrase,
		  char const *reason)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_session_usage_t *ss = nua_dialog_usage_private(sr->sr_usage);
  int error;

  nua_stack_event(nh->nh_nua, nh, ackmsg,
		  nua_i_ack, status, phrase, NULL);
  nua_stack_event(nh->nh_nua, nh, NULL,
		  nua_i_media_error, status, phrase, NULL);

  if (reason) ss->ss_reason = reason;
  ss->ss_reporting = 1;
  error = nua_client_create(nh, nua_r_bye, &nua_bye_client_methods, NULL);
  ss->ss_reporting = 0;

  signal_call_state_change(nh, ss,
			   488, "Offer-Answer Error",
			   /* We report terminated state if BYE failed */
			   error
			   ? nua_callstate_terminated
			   : nua_callstate_terminating);

  return 0;
}


/** @NUA_EVENT nua_i_cancel
 *
 * Incoming INVITE has been cancelled by the client.
 *
 * @param status status code of response to CANCEL sent automatically by stack
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the call
 * @param hmagic application context associated with the call
 * @param sip    incoming CANCEL request
 * @param tags   empty
 *
 * @sa @ref nua_uas_call_model, nua_cancel(), #nua_i_invite, #nua_i_state
 *
 * @END_NUA_EVENT
 */

/* CANCEL  */
static
int process_cancel(nua_server_request_t *sr,
		   nta_incoming_t *irq,
		   sip_t const *sip)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_session_usage_t *ss = nua_dialog_usage_private(sr->sr_usage);
  msg_t *cancel = nta_incoming_getrequest_ackcancel(irq);

  assert(ss); assert(ss == nua_session_usage_for_dialog(nh->nh_ds)); (void)ss;

  assert(nta_incoming_status(irq) < 200);

  nua_stack_event(nh->nh_nua, nh, cancel, nua_i_cancel, SIP_200_OK, NULL);
  sr->sr_application = SR_STATUS1(sr, SIP_487_REQUEST_TERMINATED);
  nua_server_respond(sr, NULL);
  nua_server_report(sr);

  return 0;
}

/* Timeout (no ACK or PRACK received) */
static
int process_timeout(nua_server_request_t *sr,
		    nta_incoming_t *irq)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_session_usage_t *ss = nua_dialog_usage_private(sr->sr_usage);
  char const *phrase = "ACK Timeout";
  char const *reason = "SIP;cause=408;text=\"ACK Timeout\"";
  int error;

  assert(ss); assert(ss == nua_session_usage_for_dialog(nh->nh_ds));

  if (nua_server_request_is_pending(sr)) {
    phrase = "PRACK Timeout";
    reason = "SIP;cause=504;text=\"PRACK Timeout\"";
  }

  nua_stack_event(nh->nh_nua, nh, 0, nua_i_error, 408, phrase, NULL);

  if (nua_server_request_is_pending(sr)) {
    /* PRACK timeout */
    SR_STATUS1(sr, SIP_504_GATEWAY_TIME_OUT);
    nua_server_trespond(sr,
			SIPTAG_REASON_STR(reason),
			TAG_END());
    if (nua_server_report(sr) >= 2)
      return 0;			/* Done */
    sr = NULL;
  }

  /* send BYE, too, if 200 OK (or 183 to re-INVITE) timeouts  */
  ss->ss_reason = reason;

  ss->ss_reporting = 1;		/* We report terminated state here if BYE fails */
  error = nua_client_create(nh, nua_r_bye, &nua_bye_client_methods, NULL);
  ss->ss_reporting = 0;

  signal_call_state_change(nh, ss, 0, phrase,
			   error
			   ? nua_callstate_terminated
			   : nua_callstate_terminating);

  if (sr)
    nua_server_request_destroy(sr);

  return 0;
}


/** @NUA_EVENT nua_i_prack
 *
 * Incoming PRACK request. PRACK request is used to acknowledge reliable
 * preliminary responses and it is usually sent automatically by the nua
 * stack.
 *
 * @param status status code of response sent automatically by stack
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the call
 * @param hmagic application context associated with the call
 * @param sip    incoming PRACK request
 * @param tags   empty
 *
 * @sa nua_prack(), #nua_r_prack, @RFC3262, NUTAG_EARLY_MEDIA()
 *
 * @END_NUA_EVENT
 */

int nua_prack_server_init(nua_server_request_t *sr);
int nua_prack_server_respond(nua_server_request_t *sr, tagi_t const *tags);
int nua_prack_server_report(nua_server_request_t *sr, tagi_t const *tags);

nua_server_methods_t const nua_prack_server_methods =
  {
    SIP_METHOD_PRACK,
    nua_i_prack,		/* Event */
    {
      0,			/* Do not create dialog */
      1,			/* In-dialog request */
      1,			/* Target refresh request  */
      1,			/* Add Contact */
    },
    nua_prack_server_init,
    nua_base_server_preprocess,
    nua_base_server_params,
    nua_prack_server_respond,
    nua_prack_server_report,
  };

/** @internal Process reliable response PRACK or (timeout from 100rel) */
static int process_prack(nua_server_request_t *sr,
			 nta_reliable_t *rel,
			 nta_incoming_t *irq,
			 sip_t const *sip)
{
  nua_handle_t *nh;

  nta_reliable_destroy(rel);

  if (irq == NULL)
    /* Final response interrupted 100rel, we did not actually receive PRACK */
    return 200;

  sr->sr_pracked = 1;

  if (!nua_server_request_is_pending(sr)) /* There is no INVITE anymore */
    return 481;

  nh = sr->sr_owner;

  if (nh->nh_ds->ds_leg == NULL)
    return 500;

  if (sip == NULL) {
    /* 100rel timeout */
    SR_STATUS(sr, 504, "Reliable Response Timeout");
    nua_stack_event(nh->nh_nua, nh, NULL, nua_i_error,
		    sr->sr_status, sr->sr_phrase,
		    NULL);
    nua_server_trespond(sr,
			SIPTAG_REASON_STR("SIP;cause=504;"
					  "text=\"PRACK Timeout\""),
			TAG_END());
    nua_server_report(sr);
    return 504;
  }

  nta_incoming_bind(irq, NULL, (void *)sr);

  return nua_stack_process_request(nh, nh->nh_ds->ds_leg, irq, sip);
}


int nua_prack_server_init(nua_server_request_t *sr)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_server_request_t *sri = nta_incoming_magic(sr->sr_irq, NULL);

  if (sri == NULL)
    return SR_STATUS(sr, 481, "No Such Preliminary Response");

  if (nua_session_server_init(sr))
    return sr->sr_status;

  if (sr->sr_sdp) {
    nua_session_usage_t *ss = NUA_DIALOG_USAGE_PRIVATE(sr->sr_usage);
    char const *offeranswer;

    /* XXX - check for overlap? */

    if (sri->sr_offer_sent && !sri->sr_answer_recv)
      sr->sr_answer_recv = 1, sri->sr_answer_recv = 1, offeranswer = Answer;
    else
      sr->sr_offer_recv = 1, offeranswer = Offer;

    ss->ss_oa_recv = offeranswer;

    if (nh->nh_soa &&
	soa_set_remote_sdp(nh->nh_soa, NULL, sr->sr_sdp, sr->sr_sdp_len) < 0) {
      SU_DEBUG_5(("nua(%p): %s server: error parsing %s\n", (void *)nh,
		  "PRACK", offeranswer));
      return
	sr->sr_status = soa_error_as_sip_response(nh->nh_soa, &sr->sr_phrase);
    }
  }

  return 0;
}

int nua_prack_server_respond(nua_server_request_t *sr, tagi_t const *tags)
{
  nua_handle_t *nh = sr->sr_owner;

  if (sr->sr_status < 200 || 300 <= sr->sr_status)
    return nua_base_server_respond(sr, tags);

  if (sr->sr_sdp) {
    nua_session_usage_t *ss = nua_dialog_usage_private(sr->sr_usage);
    msg_t *msg = sr->sr_response.msg;
    sip_t *sip = sr->sr_response.sip;

    if (nh->nh_soa == NULL) {
      if (sr->sr_offer_recv && session_get_description(sip, NULL, NULL))
	sr->sr_answer_sent = 1, ss ? ss->ss_oa_sent = Answer : Answer;
    }
    else if ((sr->sr_offer_recv && soa_generate_answer(nh->nh_soa, NULL) < 0) ||
	     (sr->sr_answer_recv && soa_process_answer(nh->nh_soa, NULL) < 0)) {
      SU_DEBUG_5(("nua(%p): %s server: %s %s\n",
		  (void *)nh, "PRACK",
		  "error processing",
		  sr->sr_offer_recv ? Offer : Answer));
      sr->sr_status = soa_error_as_sip_response(nh->nh_soa, &sr->sr_phrase);
    }
    else if (sr->sr_offer_recv) {
      if (session_include_description(nh->nh_soa, 1, msg, sip) < 0)
	sr_status(sr, SIP_500_INTERNAL_SERVER_ERROR);
      else
	sr->sr_answer_sent = 1, ss ? ss->ss_oa_sent = Answer : Answer;
    }
  }

  return nua_base_server_respond(sr, tags);
}

int nua_prack_server_report(nua_server_request_t *sr, tagi_t const *tags)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_session_usage_t *ss = nua_dialog_usage_private(sr->sr_usage);
  nua_server_request_t *sri = nta_incoming_magic(sr->sr_irq, NULL);
  int status = sr->sr_status; char const *phrase = sr->sr_phrase;
  int offer_recv_or_answer_sent = sr->sr_offer_recv || sr->sr_answer_sent;
  int retval;

  retval = nua_base_server_report(sr, tags), sr = NULL; /* destroys sr */

  if (retval >= 2 || ss == NULL) {
#if 0
    signal_call_state_change(nh, NULL,
			     status, phrase,
			     nua_callstate_terminated);
#endif
    return retval;
  }

  if (offer_recv_or_answer_sent) {
    /* signal offer received, answer sent */
    signal_call_state_change(nh, ss,
			     status, phrase,
			     ss->ss_state);
    if (nh->nh_soa) {
      soa_activate(nh->nh_soa, NULL);
      ss->ss_sdp_version = soa_get_user_version(nh->nh_soa);
    }
  }

  if (status < 200 || 300 <= status)
    return retval;

  assert(sri);

  if (sri == NULL) {

  }
  else if (SR_HAS_SAVED_SIGNAL(sri)) {
    nua_signal_data_t const *e;

    e = nua_signal_data(sri->sr_signal);

    sri->sr_application = SR_STATUS(sri, e->e_status, e->e_phrase);

    nua_server_params(sri, e->e_tags);
    nua_server_respond(sri, e->e_tags);
    nua_server_report(sri);
  }
  else if (ss->ss_state < nua_callstate_ready
	   && !ss->ss_alerting
	   && !ss->ss_precondition
	   && NH_PGET(nh, auto_alert))  {
    SR_STATUS1(sri, SIP_180_RINGING);
    nua_server_respond(sri, NULL);
    nua_server_report(sri);
  }

  return retval;
}

/* ---------------------------------------------------------------------- */
/* Automatic notifications from a referral */

static int
nh_referral_check(nua_handle_t *nh, tagi_t const *tags)
{
  sip_event_t const *event = NULL;
  int pause = 1;
  struct nua_referral *ref = nh->nh_referral;
  nua_handle_t *ref_handle = ref->ref_handle;

  if (!ref_handle
      &&
      tl_gets(tags,
	      NUTAG_NOTIFY_REFER_REF(ref_handle),
	      NUTAG_REFER_EVENT_REF(event),
	      NUTAG_REFER_PAUSE_REF(pause),
	      TAG_END()) == 0
      &&
      tl_gets(nh->nh_tags,
	      NUTAG_NOTIFY_REFER_REF(ref_handle),
	      NUTAG_REFER_EVENT_REF(event),
	      NUTAG_REFER_PAUSE_REF(pause),
	      TAG_END()) == 0)
    return 0;

  if (!ref_handle)
    return 0;

  /* Remove nh_referral and nh_notevent */
  tl_tremove(nh->nh_tags,
	     NUTAG_NOTIFY_REFER(ref_handle),
	     TAG_IF(event, NUTAG_REFER_EVENT(event)),
	     TAG_END());

  if (event)
    ref->ref_event = sip_event_dup(nh->nh_home, event);

  if (!nh_validate(nh->nh_nua, ref_handle)) {
    SU_DEBUG_3(("nua: invalid NOTIFY_REFER handle\n"));
    return -1;
  }
  else if (!ref->ref_event) {
    SU_DEBUG_3(("nua: NOTIFY event missing\n"));
    return -1;
  }

  if (ref_handle != ref->ref_handle) {
    if (ref->ref_handle)
      nua_handle_unref(ref->ref_handle);
    ref->ref_handle = nua_handle_ref(ref_handle);
  }

#if 0
  if (pause) {
    /* Pause media on REFER handle */
    nmedia_pause(nua, ref_handle->nh_nm, NULL);
  }
#endif

  return 0;
}

static void
nh_referral_respond(nua_handle_t *nh, int status, char const *phrase)
{
  char payload[128];
  char const *substate;
  struct nua_referral *ref = nh->nh_referral;

  if (!nh_validate(nh->nh_nua, ref->ref_handle)) {
    if (ref) {
      if (ref->ref_handle)
	SU_DEBUG_1(("nh_handle_referral: stale referral handle %p\n",
		    (void *)ref->ref_handle));
      ref->ref_handle = NULL;
    }
    return;
  }

  /* XXX - we should have a policy here whether to send 101..199 */

  assert(ref->ref_event);

  if (status >= 300)
    status = 503, phrase = sip_503_Service_unavailable;

  snprintf(payload, sizeof(payload), "SIP/2.0 %03u %s\r\n", status, phrase);

  if (status < 200)
    substate = "active";
  else
    substate = "terminated ;reason=noresource";

  nua_stack_post_signal(ref->ref_handle,
			nua_r_notify,
			SIPTAG_EVENT(ref->ref_event),
			SIPTAG_SUBSCRIPTION_STATE_STR(substate),
			SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
			SIPTAG_PAYLOAD_STR(payload),
			TAG_END());

  if (status < 200)
    return;

  su_free(nh->nh_home, ref->ref_event), ref->ref_event = NULL;

  nua_handle_unref(ref->ref_handle), ref->ref_handle = NULL;
}

/* ======================================================================== */
/* INFO */

/**@fn void nua_info(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...);
 *
 * Send an INFO request.
 *
 * INFO is used to send call related information like DTMF
 * digit input events. See @RFC2976.
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *    nothing
 *
 * @par Related Tags:
 *    Header tags defined in <sofia-sip/sip_tag.h>.
 *
 * @par Events:
 *    #nua_r_info
 *
 * @sa #nua_i_info
 */

nua_client_methods_t const nua_info_client_methods = {
  SIP_METHOD_INFO,		/* crm_method, crm_method_name */
  0,				/* crm_extra */
  {				/* crm_flags */
    /* create_dialog */ 0,
    /* in_dialog */ 1,
    /* target refresh */ 0
  },
  NULL,				/* crm_template */
  NULL,		/* crm_init */
  NULL,	/* crm_send */
  NULL,				/* crm_check_restart */
  NULL,				/* crm_recv */
  NULL,				/* crm_preliminary */
  NULL,				/* crm_report */
  NULL,				/* crm_complete */
};

int
nua_stack_info(nua_t *nua, nua_handle_t *nh, nua_event_t e, tagi_t const *tags)
{
  return nua_client_create(nh, e, &nua_info_client_methods, tags);
}

/** @NUA_EVENT nua_r_info
 *
 * Response to an outgoing @b INFO request.
 *
 * @param status response status code
 *               (if the request is retried, @a status is 100, the @a
 *               sip->sip_status->st_status contain the real status code
 *               from the response message, e.g., 302, 401, or 407)
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the call
 * @param hmagic application context associated with the call
 * @param sip    response to @b INFO or NULL upon an error
 *               (status code is in @a status and
 *                descriptive message in @a phrase parameters)
 * @param tags   empty
 *
 * @sa nua_info(), #nua_i_info, @RFC2976
 *
 * @END_NUA_EVENT
 */

/** @NUA_EVENT nua_i_info
 *
 * Incoming session INFO request.
 *
 * @param status statuscode of response sent automatically by stack
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the call
 * @param hmagic application context associated with the call
 * @param sip    incoming INFO request
 * @param tags   empty
 *
 * @sa nua_info(), #nua_r_info, @RFC2976
 *
 * @END_NUA_EVENT
 */

nua_server_methods_t const nua_info_server_methods =
  {
    SIP_METHOD_INFO,
    nua_i_info,			/* Event */
    {
      0,			/* Do not create dialog */
      0,			/* Allow outside dialog, too */
      0,			/* Not a target refresh request  */
      0,			/* Do not add Contact */
    },
    nua_base_server_init,
    nua_base_server_preprocess,
    nua_base_server_params,
    nua_base_server_respond,
    nua_base_server_report,
  };

/* ======================================================================== */
/* UPDATE */

/**@fn void nua_update(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...);
 *
 * Update a session.
 *
 * Update a session using SIP UPDATE method. See @RFC3311.
 *
 * Update method can be used when the session has been established with
 * INVITE. It's mainly used during the session establishment when
 * preconditions are used (@RFC3312). It can be also used during the call if
 * no user input is needed for offer/answer negotiation.
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *    nothing
 *
 * @par Related Tags:
 *    same as nua_invite()
 *
 * @par Events:
 *    #nua_r_update \n
 *    #nua_i_state (#nua_i_active, #nua_i_terminated)\n
 *    #nua_i_media_error \n
 *
 * @sa @ref nua_call_model, @RFC3311, nua_update(), #nua_i_update
 */

static int nua_update_client_init(nua_client_request_t *cr,
				  msg_t *msg, sip_t *sip,
				  tagi_t const *tags);
static int nua_update_client_request(nua_client_request_t *cr,
				     msg_t *msg, sip_t *sip,
				     tagi_t const *tags);
static int nua_update_client_response(nua_client_request_t *cr,
				      int status, char const *phrase,
				      sip_t const *sip);
static int nua_update_client_report(nua_client_request_t *cr,
				    int status, char const *phrase,
				    sip_t const *sip,
				    nta_outgoing_t *orq,
				    tagi_t const *tags);

nua_client_methods_t const nua_update_client_methods = {
  SIP_METHOD_UPDATE,		/* crm_method, crm_method_name */
  0,				/* crm_extrasize of private data */
  {				/* crm_flags */
    /* create_dialog */ 0,
    /* in_dialog */ 1,
    /* target refresh */ 1
  },
  NULL,				/* crm_template */
  nua_update_client_init,	/* crm_init */
  nua_update_client_request,	/* crm_send */
  session_timer_check_restart,	/* crm_check_restart */
  nua_update_client_response,	/* crm_recv */
  NULL,				/* crm_preliminary */
  nua_update_client_report,	/* crm_report */
  NULL,				/* crm_complete */
};

int nua_stack_update(nua_t *nua, nua_handle_t *nh, nua_event_t e,
		     tagi_t const *tags)
{
  return nua_client_create(nh, e, &nua_update_client_methods, tags);
}

static int nua_update_client_init(nua_client_request_t *cr,
				  msg_t *msg, sip_t *sip,
				  tagi_t const *tags)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du = nua_dialog_usage_for_session(nh->nh_ds);

  cr->cr_usage = du;

  return 0;
}

static int nua_update_client_request(nua_client_request_t *cr,
				     msg_t *msg, sip_t *sip,
				     tagi_t const *tags)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du = cr->cr_usage;
  nua_session_usage_t *ss;
  nua_server_request_t *sr;
  nua_client_request_t *cri;
  int offer_sent = 0, retval;

  if (du == NULL)		/* Call terminated */
    return nua_client_return(cr, SIP_481_NO_TRANSACTION, msg);

  ss = NUA_DIALOG_USAGE_PRIVATE(du);
  if (ss->ss_state >= nua_callstate_terminating)
    return nua_client_return(cr, 900, "Session is terminating", msg);

  cri = du->du_cr;

  for (sr = nh->nh_ds->ds_sr; sr; sr = sr->sr_next)
    if ((sr->sr_offer_sent && !sr->sr_answer_recv) ||
	(sr->sr_offer_recv && !sr->sr_answer_sent))
      break;

  if (nh->nh_soa == NULL) {
    offer_sent = session_get_description(sip, NULL, NULL);
  }
  else if (sr ||
	   (cri && cri->cr_offer_sent && !cri->cr_answer_recv) ||
	   (cri && cri->cr_offer_recv && !cri->cr_answer_sent)) {
   if (session_get_description(sip, NULL, NULL))
     return nua_client_return(cr, 500, "Overlapping Offer/Answer", msg);
  }
  else if (!sip->sip_payload) {
    soa_init_offer_answer(nh->nh_soa);

    if (soa_generate_offer(nh->nh_soa, 0, NULL) < 0 ||
	session_include_description(nh->nh_soa, 1, msg, sip) < 0) {
      if (ss->ss_state < nua_callstate_ready) {
	/* XXX - use soa_error_as_sip_reason(nh->nh_soa) */
	cr->cr_graceful = 1;
	ss->ss_reason = "SIP;cause=400;text=\"Local media failure\"";
      }
      return nua_client_return(cr, 900, "Local media failed", msg);
    }
    offer_sent = 1;
  }

  /* Add session timer headers */
  session_timer_preferences(ss->ss_timer,
			    sip,
			    NH_PGET(nh, supported),
			    NH_PGET(nh, session_timer),
			    NUA_PISSET(nh->nh_nua, nh, session_timer),
			    NH_PGET(nh, refresher),
			    NH_PGET(nh, min_se));

  if (session_timer_is_supported(ss->ss_timer))
    session_timer_add_headers(ss->ss_timer, ss->ss_state < nua_callstate_ready,
				  msg, sip, nh);

  retval = nua_base_client_request(cr, msg, sip, NULL);

  if (retval == 0) {
    enum nua_callstate state = ss->ss_state;
    cr->cr_offer_sent = offer_sent;
    ss->ss_update_needed = 0;

    if (state == nua_callstate_ready)
      state = nua_callstate_calling; /* XXX */

    if (offer_sent)
      ss->ss_oa_sent = Offer;

    if (!cr->cr_restarting)	/* Restart logic calls nua_update_client_report */
      signal_call_state_change(nh, ss, 0, "UPDATE sent", state);
  }

  return retval;
}

static int nua_update_client_response(nua_client_request_t *cr,
				      int status, char const *phrase,
				      sip_t const *sip)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du = cr->cr_usage;
  nua_session_usage_t *ss = nua_dialog_usage_private(du);
  int uas;

  assert(200 <= status);

  if (ss && sip && status < 300) {
    if (session_timer_is_supported(ss->ss_timer)) {
      nua_server_request_t *sr;

      for (sr = nh->nh_ds->ds_sr; sr; sr = sr->sr_next)
	if (sr->sr_method == sip_method_invite ||
	    sr->sr_method == sip_method_update)
	  break;

      if (!sr && (!du->du_cr || !du->du_cr->cr_orq)) {
	session_timer_store(ss->ss_timer, sip);
	session_timer_set(ss, uas = 0);
      }
    }
  }

  return nua_session_client_response(cr, status, phrase, sip);
}

/** @NUA_EVENT nua_r_update
 *
 * Answer to outgoing UPDATE.
 *
 * The UPDATE may be sent explicitly by nua_update() or
 * implicitly by NUA state machine.
 *
 * @param status response status code
 *               (if the request is retried, @a status is 100, the @a
 *               sip->sip_status->st_status contain the real status code
 *               from the response message, e.g., 302, 401, or 407)
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the call
 * @param hmagic application context associated with the call
 * @param sip    response to UPDATE request or NULL upon an error
 *               (status code is in @a status and
 *                descriptive message in @a phrase parameters)
 * @param tags   empty
 *
 * @sa @ref nua_call_model, @RFC3311, nua_update(), #nua_i_update
 *
 * @END_NUA_EVENT
 */

static int nua_update_client_report(nua_client_request_t *cr,
				    int status, char const *phrase,
				    sip_t const *sip,
				    nta_outgoing_t *orq,
				    tagi_t const *tags)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du = cr->cr_usage;
  nua_session_usage_t *ss = nua_dialog_usage_private(du);

  nua_stack_event(nh->nh_nua, nh,
		  nta_outgoing_getresponse(orq),
		  (enum nua_event_e)cr->cr_event,
		  status, phrase,
		  tags);

  if (!ss || cr->cr_terminated || cr->cr_graceful || cr->cr_waiting)
    return 1;

  if (cr->cr_offer_sent) {
    unsigned next_state = ss->ss_state;

    if (status < 200)
      ;
    else if (nua_invite_client_should_ack(du->du_cr)) {
      /* There is an un-ACK-ed INVITE there */
      assert(du->du_cr->cr_method == sip_method_invite);

      if (NH_PGET(nh, auto_ack) ||
	  /* Auto-ACK response to re-INVITE when media is enabled
	     and auto_ack is not set to 0 on handle */
	  (ss->ss_state == nua_callstate_ready && nh->nh_soa &&
	   !NH_PISSET(nh, auto_ack))) {
	if (nua_invite_client_ack(du->du_cr, NULL) > 0)
	  next_state = nua_callstate_ready;
	else
	  next_state = nua_callstate_terminating;
      }
    }

    signal_call_state_change(nh, ss, status, phrase, (enum nua_callstate)next_state);
  }

  return 1;
}

/* ---------------------------------------------------------------------- */
/* UPDATE server */

int nua_update_server_init(nua_server_request_t *sr);
int nua_update_server_respond(nua_server_request_t *sr, tagi_t const *tags);
int nua_update_server_report(nua_server_request_t *, tagi_t const *);

nua_server_methods_t const nua_update_server_methods =
  {
    SIP_METHOD_UPDATE,
    nua_i_update,		/* Event */
    {
      0,			/* Do not create dialog */
      1,			/* In-dialog request */
      1,			/* Target refresh request  */
      1,			/* Add Contact */
    },
    nua_update_server_init,
    nua_base_server_preprocess,
    nua_base_server_params,
    nua_update_server_respond,
    nua_update_server_report,
  };

int nua_update_server_init(nua_server_request_t *sr)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_session_usage_t *ss;

  sip_t const *request = sr->sr_request.sip;

  if (nua_session_server_init(sr))
    return sr->sr_status;

  ss = nua_dialog_usage_private(sr->sr_usage);

  /* Do session timer negotiation */
  if (request->sip_session_expires)
    session_timer_store(ss->ss_timer, request);

  if (sr->sr_sdp) {		/* Check for overlap */
    nua_client_request_t *cr;
    nua_server_request_t *sr0;
    int overlap = 0;

    /*
      A UAS that receives an UPDATE before it has generated a final
      response to a previous UPDATE on the same dialog MUST return a 500
      response to the new UPDATE, and MUST include a Retry-After header
      field with a randomly chosen value between 0 and 10 seconds.

      If an UPDATE is received that contains an offer, and the UAS has
      generated an offer (in an UPDATE, PRACK or INVITE) to which it has
      not yet received an answer, the UAS MUST reject the UPDATE with a 491
      response.  Similarly, if an UPDATE is received that contains an
      offer, and the UAS has received an offer (in an UPDATE, PRACK, or
      INVITE) to which it has not yet generated an answer, the UAS MUST
      reject the UPDATE with a 500 response, and MUST include a Retry-After
      header field with a randomly chosen value between 0 and 10 seconds.
    */
    for (cr = nh->nh_ds->ds_cr; cr; cr = cr->cr_next)
      if ((overlap = cr->cr_offer_sent && !cr->cr_answer_recv))
	break;

    if (!overlap)
      for (sr0 = nh->nh_ds->ds_sr; sr0; sr0 = sr0->sr_next)
	if ((overlap = sr0->sr_offer_recv && !sr0->sr_answer_sent))
	  break;

    if (nh->nh_soa && overlap) {
		return nua_server_retry_after(sr, 500, "Overlapping Offer/Answer", 1, 9);
	}

    if (nh->nh_soa &&
	soa_set_remote_sdp(nh->nh_soa, NULL, sr->sr_sdp, sr->sr_sdp_len) < 0) {
      SU_DEBUG_5(("nua(%p): %s server: error parsing %s\n", (void *)nh,
		  "UPDATE", Offer));
      return
	sr->sr_status = soa_error_as_sip_response(nh->nh_soa, &sr->sr_phrase);
    }

    sr->sr_offer_recv = 1;
    ss ? ss->ss_oa_recv = Offer : Offer;
  }

  return 0;
}

/** @internal Respond to an UPDATE request.
 *
 */
int nua_update_server_respond(nua_server_request_t *sr, tagi_t const *tags)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_session_usage_t *ss = nua_dialog_usage_private(sr->sr_usage);
  msg_t *msg = sr->sr_response.msg;
  sip_t *sip = sr->sr_response.sip;

  if (200 <= sr->sr_status && sr->sr_status < 300 && sr->sr_sdp) {
    if (nh->nh_soa == NULL) {
      sr->sr_answer_sent = 1, ss ? ss->ss_oa_sent = Answer : Answer;
    }
    else if (soa_generate_answer(nh->nh_soa, NULL) < 0) {
      SU_DEBUG_5(("nua(%p): %s server: %s %s\n",
		  (void *)nh, "UPDATE", "error processing", Offer));
      sr->sr_status = soa_error_as_sip_response(nh->nh_soa, &sr->sr_phrase);
    }
    else if (soa_activate(nh->nh_soa, NULL) < 0) {
      SU_DEBUG_5(("nua(%p): %s server: error activating media\n",
		  (void *)nh, "UPDATE"));
      /* XXX */
    }
    else if (session_include_description(nh->nh_soa, 1, msg, sip) < 0) {
      sr_status(sr, SIP_500_INTERNAL_SERVER_ERROR);
    }
    else {
      sr->sr_answer_sent = 1, ss->ss_oa_sent = Answer;
      ss->ss_sdp_version = soa_get_user_version(nh->nh_soa);
    }
  }

  if (ss && 200 <= sr->sr_status && sr->sr_status < 300) {
    session_timer_preferences(ss->ss_timer,
			      sip,
			      NH_PGET(nh, supported),
			      NH_PGET(nh, session_timer),
			      NUA_PISSET(nh->nh_nua, nh, session_timer),
			      NH_PGET(nh, refresher),
			      NH_PGET(nh, min_se));

    if (session_timer_is_supported(ss->ss_timer)) {
      nua_server_request_t *sr0;
      int uas;

      session_timer_add_headers(ss->ss_timer, 0, msg, sip, nh);

      for (sr0 = nh->nh_ds->ds_sr; sr0; sr0 = sr0->sr_next)
	if (sr0->sr_method == sip_method_invite)
	  break;

      if (!sr0 && (!sr->sr_usage->du_cr || !sr->sr_usage->du_cr->cr_orq))
	session_timer_set(ss, uas = 1);
    }
  }

  return nua_base_server_respond(sr, tags);
}

/** @NUA_EVENT nua_i_update
 *
 * @brief Incoming session UPDATE request.
 *
 * @param status statuscode of response sent automatically by stack
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the call
 * @param hmagic application context associated with the call
 * @param sip    incoming UPDATE request
 * @param tags   empty
 *
 * @sa nua_update(), #nua_r_update, #nua_i_state
 *
 * @END_NUA_EVENT
 */

int nua_update_server_report(nua_server_request_t *sr, tagi_t const *tags)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_dialog_usage_t *du = sr->sr_usage;
  nua_session_usage_t *ss = nua_dialog_usage_private(du);
  int status = sr->sr_status; char const *phrase = sr->sr_phrase;
  int offer_recv_or_answer_sent = sr->sr_offer_recv || sr->sr_answer_sent;
  int retval;

  retval = nua_base_server_report(sr, tags), sr = NULL; /* destroys sr */

  if (retval >= 2 || ss == NULL) {
#if 0
    signal_call_state_change(nh, NULL, status, phrase,
			     nua_callstate_terminated);
#endif
    return retval;
  }

  if (offer_recv_or_answer_sent) {
    /* signal offer received, answer sent */
    enum nua_callstate state = ss->ss_state;

    if (state == nua_callstate_ready && status < 200)
      state = nua_callstate_received;

    signal_call_state_change(nh, ss, status, phrase, state);
  }

  if (200 <= status && status < 300
      && ss->ss_state < nua_callstate_ready
      && ss->ss_precondition
      && !ss->ss_alerting
      && NH_PGET(nh, auto_alert))  {
    nua_server_request_t *sri;

    for (sri = nh->nh_ds->ds_sr; sri; sri = sri->sr_next)
      if (sri->sr_method == sip_method_invite &&
	  nua_server_request_is_pending(sri))
	break;

    if (sri) {
      SR_STATUS1(sri, SIP_180_RINGING);
      nua_server_respond(sri, NULL);
      nua_server_report(sri);
    }
  }

  return retval;
}

/* ======================================================================== */
/* BYE */

/**@fn void nua_bye(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...);
 *
 * Hangdown a call.
 *
 * Hangdown a call using SIP BYE method. Also the media session
 * associated with the call is terminated.
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *    nothing
 *
 * @par Related Tags:
 *    none
 *
 * @par Events:
 *    #nua_r_bye \n
 *    #nua_i_media_error
 */

static int nua_bye_client_init(nua_client_request_t *cr,
			       msg_t *msg, sip_t *sip,
			       tagi_t const *tags);
static int nua_bye_client_request(nua_client_request_t *cr,
				  msg_t *msg, sip_t *sip,
				  tagi_t const *tags);
static int nua_bye_client_response(nua_client_request_t *cr,
				      int status, char const *phrase,
				      sip_t const *sip);
static int nua_bye_client_report(nua_client_request_t *cr,
				 int status, char const *phrase,
				 sip_t const *sip,
				 nta_outgoing_t *orq,
				 tagi_t const *tags);

nua_client_methods_t const nua_bye_client_methods = {
  SIP_METHOD_BYE,		/* crm_method, crm_method_name */
  0,				/* crm_extrasize */
  {				/* crm_flags */
    /* create_dialog */ 0,
    /* in_dialog */ 1,
    /* target refresh */ 0
  },
  NULL,				/* crm_template */
  nua_bye_client_init,		/* crm_init */
  nua_bye_client_request,	/* crm_send */
  NULL,				/* crm_check_restart */
  nua_bye_client_response,	/* crm_recv */
  NULL,				/* crm_preliminary */
  nua_bye_client_report,	/* crm_report */
  NULL,				/* crm_complete */
};

int
nua_stack_bye(nua_t *nua, nua_handle_t *nh, nua_event_t e, tagi_t const *tags)
{
  nua_session_usage_t *ss = nua_session_usage_for_dialog(nh->nh_ds);

  if (ss &&
      nua_callstate_calling <= ss->ss_state &&
      ss->ss_state <= nua_callstate_proceeding)
    return nua_client_create(nh, e, &nua_cancel_client_methods, tags);
  else
    return nua_client_create(nh, e, &nua_bye_client_methods, tags);
}

static int nua_bye_client_init(nua_client_request_t *cr,
			       msg_t *msg, sip_t *sip,
			       tagi_t const *tags)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du = nua_dialog_usage_for_session(nh->nh_ds);
  nua_session_usage_t *ss = nua_dialog_usage_private(du);

  if (!ss || (ss->ss_state >= nua_callstate_terminating && !cr->cr_auto))
    return nua_client_return(cr, 900, "Invalid handle for BYE", msg);

  if (!cr->cr_auto)
    /* Implicit state transition by nua_bye() */
    ss->ss_state = nua_callstate_terminating;

  if (nh->nh_soa)
    soa_terminate(nh->nh_soa, 0);

  nua_client_bind(cr, du);

  return 0;
}

static int nua_bye_client_request(nua_client_request_t *cr,
				  msg_t *msg, sip_t *sip,
				  tagi_t const *tags)
{
  nua_dialog_usage_t *du = cr->cr_usage;
  nua_session_usage_t *ss;
  char const *reason = NULL;

  int error;
  nua_server_request_t *sr;

  if (du == NULL)
    return nua_client_return(cr, SIP_481_NO_TRANSACTION, msg);

  ss = nua_dialog_usage_private(du);
  reason = ss->ss_reason;

  error = nua_base_client_trequest(cr, msg, sip,
				    SIPTAG_REASON_STR(reason),
				    TAG_NEXT(tags));

  if (error == 0) {
    nua_dialog_usage_reset_refresh(du);
    ss->ss_timer->timer_set = 0;

    /* Terminate server transactions associated with session, too. */
    for (sr = du->du_dialog->ds_sr; sr; sr = sr->sr_next) {
      if (sr->sr_usage == du && nua_server_request_is_pending(sr) &&
	  sr->sr_method != sip_method_bye) {
	sr_status(sr, SIP_486_BUSY_HERE);
	nua_server_respond(sr, 0);
      }
    }
  }

  return error;
}
static int nua_bye_client_response(nua_client_request_t *cr,
				      int status, char const *phrase,
				      sip_t const *sip) {

  nua_dialog_usage_t *du = cr->cr_usage;
  nua_session_usage_t *ss = nua_dialog_usage_private(du);

  if (ss && ss->ss_reporting && status >= 900)
    return 1;

  return nua_base_client_response(cr, status, phrase, sip, NULL);
}

/** @NUA_EVENT nua_r_bye
 *
 * Answer to outgoing BYE.
 *
 * The BYE may be sent explicitly by nua_bye() or implicitly by NUA state
 * machine.
 *
 * @param status response status code
 *               (if the request is retried, @a status is 100, the @a
 *               sip->sip_status->st_status contain the real status code
 *               from the response message, e.g., 302, 401, or 407)
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the call
 * @param hmagic application context associated with the call
 * @param sip    response to BYE request or NULL upon an error
 *               (status code is in @a status and
 *                descriptive message in @a phrase parameters)
 * @param tags   empty
 *
 * @sa nua_bye(), @ref nua_call_model, #nua_i_state, #nua_r_invite()
 *
 * @END_NUA_EVENT
 */

static int nua_bye_client_report(nua_client_request_t *cr,
				 int status, char const *phrase,
				 sip_t const *sip,
				 nta_outgoing_t *orq,
				 tagi_t const *tags)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du = cr->cr_usage;

  nua_stack_event(nh->nh_nua, nh,
		  nta_outgoing_getresponse(orq),
		  (enum nua_event_e)cr->cr_event,
		  status, phrase,
		  tags);

  if (du == NULL) {
    /* No more session */
  }
  else if (status < 200) {
    /* Preliminary */
  }
  else {
    nua_session_usage_t *ss = nua_dialog_usage_private(du);
    nua_client_request_t *cri;

    if (ss->ss_reporting) {
      return 1;			/* Somebody else's problem */
    }
    else if (cr->cr_waiting) {
      return 1; /* Application problem */
    }

    nua_client_bind(cr, NULL);

    signal_call_state_change(nh, ss, status, "to BYE",
			     nua_callstate_terminated);

    for (cri = du->du_dialog->ds_cr; cri; cri = cri->cr_next) {
      if (cri->cr_method == sip_method_invite)
	break;
    }

    if (!cri || cri->cr_status >= 200) {
      /* INVITE is completed, we can zap the session... */;
      nua_session_usage_destroy(nh, ss);
    }
  }

  return 1;
}

/** @NUA_EVENT nua_i_bye
 *
 * Incoming BYE request, call hangup.
 *
 * @param status statuscode of response sent automatically by stack
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the call
 * @param hmagic application context associated with the call
 * @param sip    pointer to BYE request
 * @param tags   empty
 *
 * @sa @ref nua_call_model, #nua_i_state, nua_bye(), nua_bye(), #nua_r_cancel
 *
 * @END_NUA_EVENT
 */

int nua_bye_server_init(nua_server_request_t *sr);
int nua_bye_server_report(nua_server_request_t *sr, tagi_t const *tags);

nua_server_methods_t const nua_bye_server_methods =
  {
    SIP_METHOD_BYE,
    nua_i_bye,			/* Event */
    {
      0,			/* Do not create dialog */
      1,			/* In-dialog request */
      0,			/* Not a target refresh request  */
      0,			/* Do not add Contact */
    },
    nua_bye_server_init,
    nua_base_server_preprocess,
    nua_base_server_params,
    nua_base_server_respond,
    nua_bye_server_report,
  };


int nua_bye_server_init(nua_server_request_t *sr)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_dialog_usage_t *du = nua_dialog_usage_for_session(nh->nh_ds);

  sr->sr_terminating = 1;

  if (du)
    sr->sr_usage = du;
  else
    return SR_STATUS(sr, 481, "No Such Call");

  return 0;
}

int nua_bye_server_report(nua_server_request_t *sr, tagi_t const *tags)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_dialog_usage_t *du = sr->sr_usage;
  nua_session_usage_t *ss = nua_dialog_usage_private(du);
  int early = 0, retval;

  if (sr->sr_status < 200)
    return nua_base_server_report(sr, tags);

  if (ss) {
    nua_server_request_t *sr0 = NULL, *sr_next;
    char const *phrase;

    early = ss->ss_state < nua_callstate_ready;
    phrase = early ? "Early Session Terminated" : "Session Terminated";

#if 0
    sr->sr_usage = NULL;
#endif

    for (sr0 = nh->nh_ds->ds_sr; sr0; sr0 = sr_next) {
      sr_next = sr0->sr_next;

      if (sr == sr0 || sr0->sr_usage != sr->sr_usage)
	continue;

      if (nua_server_request_is_pending(sr0)) {
	SR_STATUS(sr0, 487, phrase);
	nua_server_respond(sr0, NULL);
      }
      nua_server_request_destroy(sr0);
    }

    sr->sr_phrase = phrase;
  }

  retval = nua_base_server_report(sr, tags);

  //assert(2 <= retval && retval < 4);

#if 0
  if (ss) {
    signal_call_state_change(nh, ss, 200,
			     early ? "Received early BYE" : "Received BYE",
			     nua_callstate_terminated);
    nua_dialog_usage_remove(nh, nh->nh_ds, du);
  }
#endif

  return retval;
}

/* ---------------------------------------------------------------------- */

/** @NUA_EVENT nua_i_state
 *
 * @brief Call state has changed.
 *
 * This event will be sent whenever the call state changes.
 *
 * In addition to basic changes of session status indicated with enum
 * ::nua_callstate, the @RFC3264 SDP Offer/Answer negotiation status is also
 * included. The tags NUTAG_OFFER_RECV() or NUTAG_ANSWER_RECV() indicate
 * whether the remote SDP that was received was considered as an offer or an
 * answer. Tags NUTAG_OFFER_SENT() or NUTAG_ANSWER_SENT() indicate whether
 * the local SDP which was sent was considered as an offer or answer.
 *
 * If the @b soa SDP negotiation is enabled (by default or with
 * NUTAG_MEDIA_ENABLE(1)), the received remote SDP is included in tags
 * SOATAG_REMOTE_SDP() and SOATAG_REMOTE_SDP_STR(). The SDP negotiation
 * result from @b soa is included in the tags SOATAG_LOCAL_SDP() and
 * SOATAG_LOCAL_SDP_STR().
 *
 * SOATAG_ACTIVE_AUDIO() and SOATAG_ACTIVE_VIDEO() are informational tags
 * used to indicate what is the status of audio or video.
 *
 * Note that #nua_i_state also covers the information relayed in call
 * establisment (#nua_i_active) and termination (#nua_i_terminated) events.
 *
 * @param status protocol status code \n
 *               (always present)
 * @param phrase short description of status code \n
 *               (always present)
 * @param nh     operation handle associated with the call
 * @param hmagic application context associated with the call
 * @param sip    NULL
 * @param tags   NUTAG_CALLSTATE(),
 *               SOATAG_LOCAL_SDP(), SOATAG_LOCAL_SDP_STR(),
 *               NUTAG_OFFER_SENT(), NUTAG_ANSWER_SENT(),
 *               SOATAG_REMOTE_SDP(), SOATAG_REMOTE_SDP_STR(),
 *               NUTAG_OFFER_RECV(), NUTAG_ANSWER_RECV(),
 *               SOATAG_ACTIVE_AUDIO(), SOATAG_ACTIVE_VIDEO(),
 *               SOATAG_ACTIVE_IMAGE(), SOATAG_ACTIVE_CHAT().
 *
 * @sa @ref nua_call_model, #nua_i_active, #nua_i_terminated,
 * nua_invite(), #nua_r_invite, #nua_i_invite, nua_respond(),
 * NUTAG_MEDIA_ENABLE(),
 * NUTAG_AUTOALERT(), NUTAG_AUTOANSWER(), NUTAG_EARLY_MEDIA(),
 * NUTAG_EARLY_ANSWER(), NUTAG_INCLUDE_EXTRA_SDP(),
 * nua_ack(), NUTAG_AUTOACK(), nua_bye(), #nua_r_bye, #nua_i_bye,
 * nua_cancel(), #nua_r_cancel, #nua_i_cancel,
 * nua_prack(), #nua_r_prack, #nua_i_prack,
 * nua_update(), #nua_r_update, #nua_i_update
 *
 * @par History
 * Prior @VERSION_1_12_6 the tags NUTAG_OFFER_RECV(), NUTAG_ANSWER_RECV(),
 * NUTAG_ANSWER_SENT(), NUTAG_OFFER_SENT() were not included with
 * nua_i_state eventif media was disabled.
 *
 * @END_NUA_EVENT
 */

/**
 * Delivers call state changed event to the nua client. @internal
 *
 * @param nh call handle
 * @param status status code
 * @param tr_event SIP transaction event triggering this change
 * @param oa_recv Received SDP
 * @param oa_sent Sent SDP
 */

static void signal_call_state_change(nua_handle_t *nh,
				     nua_session_usage_t *ss,
				     int status, char const *phrase,
				     enum nua_callstate next_state)
{
  enum nua_callstate ss_state = nua_callstate_init;
  enum nua_callstate invite_state = next_state;

  char const *oa_recv = NULL;
  char const *oa_sent = NULL;

  int offer_recv = 0, answer_recv = 0, offer_sent = 0, answer_sent = 0;

  if (ss) {
    if (ss->ss_reporting)
      return;

    ss_state = ss->ss_state;
    oa_recv = ss->ss_oa_recv, ss->ss_oa_recv = NULL;
    oa_sent = ss->ss_oa_sent, ss->ss_oa_sent = NULL;

    assert(oa_sent == Offer || oa_sent == Answer || oa_sent == NULL);
    assert(oa_recv == Offer || oa_recv == Answer || oa_recv == NULL);

    if (oa_recv) {
      offer_recv = oa_recv == Offer;
      answer_recv = oa_recv == Answer;
    }

    if (oa_sent) {
      offer_sent = oa_sent == Offer;
      answer_sent = oa_sent == Answer;
    }
  }

  if (ss_state < nua_callstate_ready || next_state > nua_callstate_ready)
    SU_DEBUG_5(("nua(%p): call state changed: %s -> %s%s%s%s%s\n",
		(void *)nh, nua_callstate_name(ss_state),
		nua_callstate_name(next_state),
		oa_recv ? ", received " : "", oa_recv ? oa_recv : "",
		oa_sent && oa_recv ? ", and sent " :
		oa_sent ? ", sent " : "", oa_sent ? oa_sent : ""));
  else
    SU_DEBUG_5(("nua(%p): ready call updated: %s%s%s%s%s\n",
		(void *)nh, nua_callstate_name(next_state),
		oa_recv ? " received " : "", oa_recv ? oa_recv : "",
		oa_sent && oa_recv ? ", sent " :
		oa_sent ? " sent " : "", oa_sent ? oa_sent : ""));

  if (next_state == nua_callstate_terminating &&
      ss_state >= nua_callstate_terminating)
    return;

  if (ss) {
    /* Update state variables */
    if (next_state == nua_callstate_init) {
      if (ss_state < nua_callstate_ready)
	ss->ss_state = next_state;
      else if (ss->ss_state == nua_callstate_ready)
	next_state = ss->ss_state;
      else if (ss->ss_state == nua_callstate_terminating)
	return;
      else
	ss->ss_state = next_state = nua_callstate_terminated;
    }
    else if (next_state > ss_state)
      ss->ss_state = next_state;
  }

  if (next_state == nua_callstate_init)
    next_state = nua_callstate_terminated;

  if (ss && ss->ss_state == nua_callstate_ready)
    nh->nh_active_call = 1;
  else if (next_state == nua_callstate_terminated)
    nh->nh_active_call = 0;

  /* Send events */
  if (phrase == NULL)
    phrase = "Call state";

  {
    sdp_session_t const *remote_sdp = NULL;
    char const *remote_sdp_str = NULL;
    sdp_session_t const *local_sdp = NULL;
    char const *local_sdp_str = NULL;

    if (nh->nh_soa) {
      if (oa_recv)
	soa_get_remote_sdp(nh->nh_soa, &remote_sdp, &remote_sdp_str, 0);
      if (oa_sent)
	soa_get_local_sdp(nh->nh_soa, &local_sdp, &local_sdp_str, 0);

      if (answer_recv || answer_sent) {      /* Update nh_hold_remote */
	char const *held = NULL;
	soa_get_params(nh->nh_soa, SOATAG_HOLD_REF(held), TAG_END());
	nh->nh_hold_remote = held && strlen(held) > 0;
      }
    }
    else
      oa_recv = NULL, oa_sent = NULL;

    nua_stack_tevent(nh->nh_nua, nh, NULL, nua_i_state,
		     status, phrase,
		     NUTAG_CALLSTATE(next_state),
		     NH_ACTIVE_MEDIA_TAGS(1, nh->nh_soa),
		     /* NUTAG_SOA_SESSION(nh->nh_soa), */
		     TAG_IF(offer_recv, NUTAG_OFFER_RECV(offer_recv)),
		     TAG_IF(answer_recv, NUTAG_ANSWER_RECV(answer_recv)),
		     TAG_IF(offer_sent, NUTAG_OFFER_SENT(offer_sent)),
		     TAG_IF(answer_sent, NUTAG_ANSWER_SENT(answer_sent)),
		     TAG_IF(oa_recv, SOATAG_REMOTE_SDP(remote_sdp)),
		     TAG_IF(oa_recv, SOATAG_REMOTE_SDP_STR(remote_sdp_str)),
		     TAG_IF(oa_sent, SOATAG_LOCAL_SDP(local_sdp)),
		     TAG_IF(oa_sent, SOATAG_LOCAL_SDP_STR(local_sdp_str)),
		     TAG_END());
  }

  if (next_state == nua_callstate_ready && ss_state <= nua_callstate_ready) {
    nua_stack_tevent(nh->nh_nua, nh, NULL, nua_i_active, status, "Call active",
		     NH_ACTIVE_MEDIA_TAGS(1, nh->nh_soa),
		     /* NUTAG_SOA_SESSION(nh->nh_soa), */
		     TAG_END());
  }

  else if (next_state == nua_callstate_terminated) {
    nua_stack_event(nh->nh_nua, nh, NULL,
		    nua_i_terminated, status, phrase,
		    NULL);
  }

  if (invite_state == nua_callstate_ready) {
    /* Start next INVITE request, if queued */
    nua_client_next_request(nh->nh_ds->ds_cr, 1);
  }
}

/** @NUA_EVENT nua_i_active
 *
 * A call has been activated.
 *
 * This event will be sent after a succesful response to the initial
 * INVITE has been received and the media has been activated.
 *
 * @param nh     operation handle associated with the call
 * @param hmagic application context associated with the call
 * @param sip    NULL
 * @param tags   SOATAG_ACTIVE_AUDIO(), SOATAG_ACTIVE_VIDEO(),
 *               SOATAG_ACTIVE_IMAGE(), SOATAG_ACTIVE_CHAT().
 *
 * @deprecated Use #nua_i_state instead.
 *
 * @sa @ref nua_call_model, #nua_i_state, #nua_i_terminated,
 * #nua_i_invite
 *
 * @END_NUA_EVENT
 */

/** @NUA_EVENT nua_i_terminated
 *
 * A call has been terminated.
 *
 * This event will be sent after a call has been terminated. A call is
 * terminated, when
 * 1) an error response (300..599) is sent to an incoming initial INVITE
 * 2) a reliable response (200..299 or reliable preliminary response) to
 *    an incoming initial INVITE is not acknowledged with ACK or PRACK
 * 3) BYE is received or sent
 *
 * @param nh     operation handle associated with the call
 * @param hmagic application context associated with the call
 * @param sip    NULL
 * @param tags   empty
 *
 * @deprecated Use #nua_i_state instead.
 *
 * @sa @ref nua_call_model, #nua_i_state, #nua_i_active, #nua_i_bye,
 * #nua_i_invite
 *
 * @END_NUA_EVENT
 */


/* ======================================================================== */

static
int nua_server_retry_after(nua_server_request_t *sr,
			   int status, char const *phrase,
			   int min, int max)
{
  sip_retry_after_t af[1];

  sip_retry_after_init(af);
  af->af_delta = (unsigned)su_randint(min, max);
  af->af_comment = phrase;

  sip_add_dup(sr->sr_response.msg, sr->sr_response.sip, (sip_header_t *)af);

  return sr_status(sr, status, phrase);
}

/* ======================================================================== */
/* Session timer - RFC 4028 */

static int session_timer_is_supported(struct session_timer const *t)
{
  return t->local.supported;
}

/** Set session timer preferences  */
static
void session_timer_preferences(struct session_timer *t,
			       sip_t const *sip,
			       sip_supported_t const *supported,
			       unsigned expires,
			       int isset,
			       enum nua_session_refresher refresher,
			       unsigned min_se)
{
  memset(&t->local, 0, sizeof t->local);

  t->local.require = sip_has_feature(sip->sip_require, "timer");
  t->local.supported =
    sip_has_feature(supported, "timer") ||
    sip_has_feature(sip->sip_supported, "timer");
  if (isset || refresher != nua_no_refresher)
    t->local.expires = expires;
  else
    t->local.defaults = expires;
  t->local.min_se = min_se;
  t->local.refresher = refresher;
}

static int session_timer_check_restart(nua_client_request_t *cr,
				       int status, char const *phrase,
				       sip_t const *sip)
{
  if (status == 422) {
    nua_session_usage_t *ss = nua_dialog_usage_private(cr->cr_usage);

    if (ss && session_timer_is_supported(ss->ss_timer)) {
      struct session_timer *t = ss->ss_timer;

      if (sip->sip_min_se && t->local.min_se < sip->sip_min_se->min_delta)
	t->local.min_se = sip->sip_min_se->min_delta;
      if (t->local.expires != 0 && t->local.min_se > t->local.expires)
	t->local.expires = t->local.min_se;

      return nua_client_restart(cr, 100, "Re-Negotiating Session Timer");
    }
  }

  return nua_base_client_check_restart(cr, status, phrase, sip);
}

/** Check that received Session-Expires is longer than Min-SE */
static
int session_timer_check_min_se(msg_t *msg,
			       sip_t *sip,
			       sip_t const *request,
			       unsigned long min)
{
  if (min == 0)
    min = 1;

  /*
   If an incoming request contains a Supported header field with a value
   'timer' and a Session Expires header field, the UAS MAY reject the
   INVITE request with a 422 (Session Interval Too Small) response if
   the session interval in the Session-Expires header field is smaller
   than the minimum interval defined by the UAS' local policy.  When
   sending the 422 response, the UAS MUST include a Min-SE header field
   with the value of its minimum interval.  This minimum interval MUST
   NOT be lower than 90 seconds.
  */
  if (request->sip_session_expires &&
      sip_has_feature(request->sip_supported, "timer") &&
      request->sip_session_expires->x_delta < min) {
    sip_min_se_t min_se[1];

    if (min < 90)
      min = 90;

    sip_min_se_init(min_se)->min_delta = min;

    /* Include extension parameters, if any */
    if (request->sip_min_se)
      min_se->min_params = request->sip_min_se->min_params;

    sip_add_dup(msg, sip, (sip_header_t *)min_se);

    return 422;
  }

  return 0;
}

/** Store session timer parameters in request from uac / response from uas */
static
void session_timer_store(struct session_timer *t,
			 sip_t const *sip)
{
  sip_require_t const *require = sip->sip_require;
  sip_supported_t const *supported = sip->sip_supported;
  sip_session_expires_t const *x = sip->sip_session_expires;

  t->remote.require = require && sip_has_feature(require, "timer");
  t->remote.supported =
    t->remote.supported || (supported && sip_has_feature(supported, "timer"));

  t->remote.expires = 0;
  t->remote.refresher = nua_any_refresher;
  t->remote.min_se = 0;

  if (x) {
    t->remote.expires = x->x_delta;

    if (x->x_refresher) {
      int uas = sip->sip_request != NULL;

      if (su_casenmatch(x->x_refresher, "uac", (sizeof "uac")))
	t->remote.refresher = uas ? nua_remote_refresher : nua_local_refresher;
      else if (su_casenmatch(x->x_refresher, "uas", (sizeof "uas")))
	t->remote.refresher = uas ? nua_local_refresher : nua_remote_refresher;
    }
    else if (t->remote.require) {
      /* Require: timer but no refresher parameter in Session-Expires header */
      t->remote.refresher = nua_local_refresher;
    }
  }

  if (sip->sip_min_se)
    t->remote.min_se = sip->sip_min_se->min_delta;
}

/** Add timer feature and Session-Expires/Min-SE headers to request/response
 *
 */
static int
session_timer_add_headers(struct session_timer *t,
			  int initial,
			  msg_t *msg,
			  sip_t *sip,
			  nua_handle_t *nh)
{
  unsigned long expires, min;
  sip_min_se_t min_se[1];
  sip_session_expires_t x[1];
  int uas;
  int autorequire = 1;

  enum nua_session_refresher refresher = nua_any_refresher;

  static sip_param_t const x_params_uac[] = {"refresher=uac", NULL};
  static sip_param_t const x_params_uas[] = {"refresher=uas", NULL};

  if ( !NH_PGET(nh, timer_autorequire) && NH_PISSET(nh, timer_autorequire)) {
    autorequire = 0;
  }

  if (!t->local.supported)
    return 0;

  uas = sip->sip_status != NULL;

  min = t->local.min_se;
  if (min < t->remote.min_se)
    min = t->remote.min_se;

  if (uas) {
    session_timer_negotiate(t, uas = 1);

    refresher = t->refresher;
    expires = t->interval;
  }
  else {
    /* RFC 4028:
     * The UAC MAY include the refresher parameter with value 'uac' if it
     * wants to perform the refreshes.  However, it is RECOMMENDED that the
     * parameter be omitted so that it can be selected by the negotiation
     * mechanisms described below.
     */
    if (t->local.refresher == nua_local_refresher)
      refresher = nua_local_refresher;
    else if (!initial)
      refresher = t->refresher;

    expires = t->local.expires;
    if (expires != 0 && expires < min)
      expires = min;

    if (expires == 0 && !initial && t->interval)
      expires = t->interval;
  }

  sip_min_se_init(min_se)->min_delta = min;

  sip_session_expires_init(x)->x_delta = expires;
  if (refresher == nua_remote_refresher)
    x->x_params = uas ? x_params_uac : x_params_uas;
  else if (refresher == nua_local_refresher)
    x->x_params = uas ? x_params_uas : x_params_uac;

  if (expires == 0 && t->remote.min_se == 0)
    /* Session timer is not used, do not add headers */
    return 1;

  sip_add_tl(msg, sip,
			 TAG_IF(expires != 0, SIPTAG_SESSION_EXPIRES(x)),
			 TAG_IF(min != 0
					/* Min-SE: 0 is optional with initial INVITE */
					|| !initial,
					SIPTAG_MIN_SE(min_se)),
			 TAG_IF(autorequire && refresher == nua_remote_refresher && expires != 0, SIPTAG_REQUIRE_STR("timer")),
			 TAG_END());

  return 1;
}

static void
session_timer_negotiate(struct session_timer *t, int uas)
{
  if (!t->local.supported)
    t->refresher = nua_no_refresher;
  else if (!t->remote.supported)
    t->refresher = nua_local_refresher;
  else if (t->remote.refresher == nua_local_refresher)
    t->refresher = nua_local_refresher;
  else if (t->remote.refresher == nua_remote_refresher)
    t->refresher = nua_remote_refresher;
  else if (uas)
    /* UAS defaults UAC to refreshing */
    t->refresher = nua_remote_refresher;
  else
    /* UAC refreshes by itself */
    t->refresher = nua_local_refresher;

  t->interval = t->remote.expires;
  if (t->interval == 0)
    t->interval = t->local.expires;
  if (t->local.expires != 0 && t->interval > t->local.expires)
    t->interval = t->local.expires;
  if (t->local.defaults != 0 && t->interval > t->local.defaults)
    t->interval = t->local.defaults;

  if (t->interval != 0) {
    if (t->interval < t->local.min_se)
      t->interval = t->local.min_se;
    if (t->interval < t->remote.min_se)
      t->interval = t->remote.min_se;
  }

  if (t->interval == 0)
    t->refresher = nua_no_refresher;
}

static void
session_timer_set(nua_session_usage_t *ss, int uas)
{
  nua_dialog_usage_t *du = nua_dialog_usage_public(ss);
  struct session_timer *t;

  if (ss == NULL)
    return;

  t = ss->ss_timer;

  session_timer_negotiate(t, uas);

  if (t->refresher == nua_local_refresher) {
    unsigned low = t->interval / 2, high = t->interval / 2;

    if (t->interval >= 90)
      low -=5, high += 5;

    nua_dialog_usage_set_refresh_range(du, low, high);
    t->timer_set = 1;
  }
  else if (t->refresher == nua_remote_refresher) {
    /* RFC 4028 10.3 and 10.4: Send BYE before the session expires.
       Increased interval from 2/3 to 9/10 of session expiration delay
       because some endpoints won't UPDATE early enough with very short
       sessions (e.g. 120). */

    unsigned interval = t->interval;

    interval -= 32 > interval / 10 ? interval / 10 : 32;

    nua_dialog_usage_set_refresh_range(du, interval, interval);
    t->timer_set = 1;
  }
  else {
    nua_dialog_usage_reset_refresh(du);
    t->timer_set = 0;
  }
}

su_inline int
session_timer_has_been_set(struct session_timer const *t)
{
  return t->timer_set;
}

/* ======================================================================== */

/** Get SDP from a SIP message.
 *
 * @retval 1 if message contains SDP
 * @retval 0 if message does not contain valid SDP
 */
static
int session_get_description(sip_t const *sip,
			    char const **return_sdp,
			    size_t *return_len)
{
  sip_payload_t const *pl = sip->sip_payload;
  sip_content_type_t const *ct = sip->sip_content_type;
  int matching_content_type = 0;

  if (pl == NULL)
    return 0;
  else if (pl->pl_len == 0 || pl->pl_data == NULL)
    return 0;
  else if (ct == NULL)
    /* Be bug-compatible with our old gateways */
    SU_DEBUG_3(("nua: no %s, assuming %s\n",
		"Content-Type", SDP_MIME_TYPE));
  else if (ct->c_type == NULL)
    SU_DEBUG_3(("nua: empty %s, assuming %s\n",
		"Content-Type", SDP_MIME_TYPE));
  else if (!su_casematch(ct->c_type, SDP_MIME_TYPE)) {
    SU_DEBUG_5(("nua: unknown %s: %s\n", "Content-Type", ct->c_type));
    return 0;
  }
  else
    matching_content_type = 1;

  if (pl == NULL)
    return 0;

  if (!matching_content_type) {
    /* Make sure we got SDP */
    if (pl->pl_len < 3 || !su_casenmatch(pl->pl_data, "v=0", 3))
      return 0;
  }

  if (return_sdp && return_len) {
    *return_sdp = pl->pl_data;
    *return_len = pl->pl_len;
  }

  return 1;
}

/** Insert SDP into SIP message */
static
int session_include_description(soa_session_t *soa,
				int session,
				msg_t *msg,
				sip_t *sip)
{
  su_home_t *home = msg_home(msg);

  sip_content_disposition_t *cd = NULL;
  sip_content_type_t *ct = NULL;
  sip_payload_t *pl = NULL;

  int retval;

  if (!soa)
    return 0;

  retval = session_make_description(home, soa, session, &cd, &ct, &pl);

  if (retval <= 0)
    return retval;

  if ((cd && sip_header_insert(msg, sip, (sip_header_t *)cd) < 0) ||
      sip_header_insert(msg, sip, (sip_header_t *)ct) < 0 ||
      sip_header_insert(msg, sip, (sip_header_t *)pl) < 0)
    return -1;

  return retval;
}

/** Generate SDP headers */
static
int session_make_description(su_home_t *home,
			     soa_session_t *soa,
			     int session,
			     sip_content_disposition_t **return_cd,
			     sip_content_type_t **return_ct,
			     sip_payload_t **return_pl)
{
  char const *sdp;
  isize_t len;
  int retval;

  if (!soa)
    return 0;

  if (session)
    retval = soa_get_local_sdp(soa, 0, &sdp, &len);
  else
    retval = soa_get_capability_sdp(soa, 0, &sdp, &len);

  if (retval > 0) {
    *return_pl = sip_payload_create(home, sdp, len);
    *return_ct = sip_content_type_make(home, SDP_MIME_TYPE);
    if (session)
      *return_cd = sip_content_disposition_make(home, "session");
    else
      *return_cd = NULL;

    if (!*return_pl || !*return_ct)
      return -1;

    if (session && !*return_cd)
      return -1;
  }

  return retval;
}

/* ====================================================================== */

/** @NUA_EVENT nua_i_options
 *
 * Incoming OPTIONS request. The user-agent should respond to an OPTIONS
 * request with the same statuscode as it would respond to an INVITE
 * request.
 *
 * Stack responds automatically to OPTIONS request unless OPTIONS is
 * included in the set of application methods, set by NUTAG_APPL_METHOD().
 *
 * The OPTIONS request does not create a dialog. Currently the processing
 * of incoming OPTIONS creates a new handle for each incoming request which
 * is not assiciated with an existing dialog. If the handle @a nh is not
 * bound, you should probably destroy it after responding to the OPTIONS
 * request.
 *
 * @param status status code of response sent automatically by stack
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the OPTIONS request
 * @param hmagic application context associated with the call
 *               (NULL if outside session)
 * @param sip    incoming OPTIONS request
 * @param tags   empty
 *
 * @sa nua_respond(), nua_options(), #nua_r_options, @RFC3261 section 11.2
 *
 * @END_NUA_EVENT
 */

int nua_options_server_respond(nua_server_request_t *sr, tagi_t const *tags);

nua_server_methods_t const nua_options_server_methods =
  {
    SIP_METHOD_OPTIONS,
    nua_i_options,		/* Event */
    {
      0,			/* Do not create dialog */
      0,			/* Initial request */
      0,			/* Not a target refresh request  */
      1,			/* Add Contact */
    },
    nua_base_server_init,
    nua_base_server_preprocess,
    nua_base_server_params,
    nua_options_server_respond,
    nua_base_server_report,
  };

/** @internal Respond to an OPTIONS request.
 *
 */
int nua_options_server_respond(nua_server_request_t *sr, tagi_t const *tags)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_t *nua = nh->nh_nua;

  if (200 <= sr->sr_status && sr->sr_status < 300) {
    msg_t *msg = sr->sr_response.msg;
    sip_t *sip = sr->sr_response.sip;

    sip_add_tl(msg, sip, SIPTAG_ACCEPT(nua->nua_invite_accept), TAG_END());

    if (!sip->sip_payload) {	/* XXX - do MIME multipart? */
      soa_session_t *soa = nh->nh_soa;

      if (soa == NULL)
	soa = nua->nua_dhandle->nh_soa;

      session_include_description(soa, 0, msg, sip);
    }
  }

  return nua_base_server_respond(sr, tags);
}
