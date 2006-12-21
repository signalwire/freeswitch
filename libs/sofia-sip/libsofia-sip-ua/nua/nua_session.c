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

#include <sofia-sip/string0.h>
#include <sofia-sip/sip_protos.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sip_util.h>
#include <sofia-sip/su_uniqueid.h>

#define NTA_LEG_MAGIC_T      struct nua_handle_s
#define NTA_OUTGOING_MAGIC_T struct nua_handle_s
#define NTA_INCOMING_MAGIC_T struct nua_server_request
#define NTA_RELIABLE_MAGIC_T struct nua_handle_s

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

/** Session-related state */
typedef struct nua_session_usage
{
  /* enum nua_callstate */
  unsigned        ss_state:4;		/**< Session status (enum nua_callstate) */
  
  unsigned        ss_100rel:1;	        /**< Use 100rel, send 183 */
  unsigned        ss_alerting:1;	/**< 180 is sent/received */
  
  unsigned        ss_update_needed:2;	/**< Send an UPDATE (do O/A if > 1) */

  unsigned        ss_precondition:1;	/**< Precondition required */

  unsigned        ss_timer_set:1;       /**< We have active session timer. */
  unsigned        : 0;
  
  unsigned        ss_session_timer;	/**< Value of Session-Expires (delta) */
  unsigned        ss_min_se;		/**< Minimum session expires */
  enum nua_session_refresher ss_refresher; /**< none, local or remote */

  char const     *ss_ack_needed;	/**< If non-null, need to send an ACK
					 * (do O/A, if "offer" or "answer")
					 */

  nua_client_request_t ss_crequest[1];  /* Outgoing invite */
} nua_session_usage_t;

static char const *nua_session_usage_name(nua_dialog_usage_t const *du);
static int nua_session_usage_add(nua_handle_t *nh,
				 nua_dialog_state_t *ds,
				 nua_dialog_usage_t *du);
static void nua_session_usage_remove(nua_handle_t *nh,
				     nua_dialog_state_t *ds,
				     nua_dialog_usage_t *du);
static void nua_session_usage_refresh(nua_owner_t *,
				      nua_dialog_state_t *,
				      nua_dialog_usage_t *,
				      sip_time_t now);
static int nua_session_usage_shutdown(nua_owner_t *,
				      nua_dialog_state_t *,
				      nua_dialog_usage_t *);

static nua_usage_class const nua_session_usage[1] = {
  {
    sizeof (nua_session_usage_t),
    sizeof nua_session_usage,
    nua_session_usage_add,
    nua_session_usage_remove,
    nua_session_usage_name,
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
  nua_session_usage_t *ss = nua_dialog_usage_private(du);

  if (ds->ds_has_session)
    return -1;
  ds->ds_has_session = 1;

  nh->nh_ds->ds_cr->cr_next = ss->ss_crequest;
 
  return 0;
}

static
void nua_session_usage_remove(nua_handle_t *nh,
			       nua_dialog_state_t *ds,
			       nua_dialog_usage_t *du)
{
  nua_session_usage_t *ss = nua_dialog_usage_private(du);

  ds->ds_has_session = 0;

  if (ss->ss_crequest)
    nua_creq_deinit(ss->ss_crequest, NULL);

  ds->ds_cr->cr_next = NULL;
}

static
nua_session_usage_t *nua_session_usage_get(nua_dialog_state_t const *ds)
{
  nua_dialog_usage_t *du;

  if (ds == ((nua_handle_t *)NULL)->nh_ds)
    return NULL;

  du = nua_dialog_usage_get(ds, nua_session_usage, NULL);

  return (nua_session_usage_t *)nua_dialog_usage_private(du);
}

/* ======================================================================== */
/* INVITE and call (session) processing */

static int nua_stack_invite2(nua_t *, nua_handle_t *, nua_event_t e,
			     int restarted, tagi_t const *tags);
static int process_response_to_invite(nua_handle_t *nh,
				      nta_outgoing_t *orq,
				      sip_t const *sip);
static void
  session_timeout(nua_handle_t *nh, nua_dialog_usage_t *du, sip_time_t now);

static void restart_invite(nua_handle_t *nh, tagi_t *tags);

static int process_100rel(nua_handle_t *nh,
			  nua_session_usage_t *ss,
			  nta_outgoing_t *orq,
			  sip_t const *sip);

int nua_stack_prack(nua_t *nua, nua_handle_t *nh, nua_event_t e,
		    tagi_t const *tags);

static int process_response_to_prack(nua_handle_t *nh,
				     nta_outgoing_t *orq,
				     sip_t const *sip);

static void nua_session_usage_destroy(nua_handle_t *, nua_session_usage_t *);

static void session_timer_preferences(nua_session_usage_t *ss,
				      unsigned expires,
				      unsigned min_se,
				      enum nua_session_refresher refresher);
static int session_timer_is_supported(nua_handle_t const *nh);
static int prefer_session_timer(nua_handle_t const *nh);

static int use_session_timer(nua_session_usage_t *ss, int uas, int always,
			     msg_t *msg, sip_t *);
static int init_session_timer(nua_session_usage_t *ss, sip_t const *, int refresher);
static void set_session_timer(nua_session_usage_t *ss);

static int
check_session_timer_restart(nua_handle_t *nh,
			    nua_session_usage_t *ss,
			    nua_client_request_t *cr,
			    nta_outgoing_t *orq,
			    sip_t const *sip,
			    nua_creq_restart_f *restart_function);

static int nh_referral_check(nua_handle_t *nh, tagi_t const *tags);
static void nh_referral_respond(nua_handle_t *,
				int status, char const *phrase);

static void signal_call_state_change(nua_handle_t *nh,
				     nua_session_usage_t *ss,
				     int status, char const *phrase,
				     enum nua_callstate next_state,
				     char const *oa_recv,
				     char const *oa_sent);

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
int session_process_response(nua_handle_t *nh,
			     nua_client_request_t *cr,
			     nta_outgoing_t *orq,
			     sip_t const *sip,
			     char const **return_received);

static
int respond_with_retry_after(nua_handle_t *nh, nta_incoming_t *irq,
			     int status, char const *phrase,
			     int min, int max);

/**@fn void nua_invite(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...);
 *
 * Place a call using SIP INVITE method. 
 *
 * Incomplete call can be hung-up with nua_cancel(). Complete or incomplete
 * calls can be hung-up with nua_bye().
 *
 * Optionally 
 * - uses early media if NUTAG_EARLY_MEDIA() tag is used with non zero-value
 * - media parameters can be set by SOA tags
 * - nua_invite() can be used to change status of an existing call: 
 *   - #SOATAG_HOLD tag can be used to list the media that will be put on hold,
 *     the value "*" sets all the media beloginging to the session on hold
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return 
 *    nothing
 *
 * @par Related Tags:
 *    NUTAG_URL() \n
 *    Tags of nua_set_hparams() \n
 *    NUTAG_INCLUDE_EXTRA_SDP() \n
 *    SOATAG_HOLD(), SOATAG_AF(), SOATAG_ADDRESS(),
 *    SOATAG_RTP_SELECT(), SOATAG_RTP_SORT(), SOATAG_RTP_MISMATCH(), 
 *    SOATAG_AUDIO_AUX(), \n
 *    SOATAG_USER_SDP() or SOATAG_USER_SDP_STR() \n
 *    See use of tags in <sip_tag.h> below
 *
 * @par Events:
 *    #nua_r_invite \n
 *    #nua_i_state (#nua_i_active, #nua_i_terminated) \n
 *    #nua_i_media_error \n
 *    #nua_i_fork \n
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
 * are generated, if they do not exist, and tag is added to @From header.
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
 * Next, the stack generates a @Contact header for the request (Unless the
 * application already gave a @Contact header or it does not want to use
 * @Contact and indicates that by including SIPTAG_CONTACT(NULL) or
 * SIPTAG_CONTACT(SIP_NONE) in the tagged parameters.) If the application
 * has registered the URI in @From header, the @Contact header used with
 * registration is used. Otherwise, the @Contact header is generated from the
 * local IP address and port number.
 *
 * @par
 * For the initial INVITE requests, @ServiceRoute set received from
 * the registrar is also added to the request message.
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
 * @par SDP Handling
 * The initial nua_invite() creates a @ref soa_session_t "soa media session"
 * unless NUTAG_MEDIA_ENABLE(0) has been given. The SDP description of the
 * @ref soa_session_t "soa media session" is included in the INVITE request
 * as message body. 
 *
 * @par
 * The SDP in a 1XX or 2XX response message is interpreted as an answer,
 * given to the @ref soa_session_t "soa media session" object for
 * processing.
 *
 * @bug If the INVITE request already contains a message body, SDP is not
 * added. Also, if the response contains a multipart body, it is not parsed.
 *
 * @par Authentication
 * The INVITE request may need authentication. Each proxy or server
 * requiring authentication can respond with 401 or 407 response. The
 * nua_authenticate() operation stores authentication information (username
 * and password) to the handle, and stack tries to authenticate all the rest
 * of the requests (e.g., PRACK, ACK, UPDATE, re-INVITE, BYE) using same
 * username and password.
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
int
nua_stack_invite(nua_t *nua, nua_handle_t *nh, nua_event_t e,
		 tagi_t const *tags)
{
  char const *what;

  if (nh_is_special(nh) || 
      nua_stack_set_handle_special(nh, nh_has_invite, nua_i_error))
    what = "Invalid handle for INVITE";
  else if (nh_referral_check(nh, tags) < 0) {
    what = "Invalid referral";
  }
  else if (nua_stack_init_handle(nua, nh, TAG_NEXT(tags)) < 0) {
    what = "Handle initialization failed";
  }
  else
    return nua_stack_invite2(nua, nh, e, 0, tags);

  UA_EVENT2(e, 900, what);

  signal_call_state_change(nh, NULL, 900, what, nua_callstate_init, 0, 0);

  return e;
}

static int
nua_stack_invite2(nua_t *nua, nua_handle_t *nh, nua_event_t e,
		  int restarted,
		  tagi_t const *tags)
{
  nua_dialog_usage_t *du;
  nua_session_usage_t *ss;
  nua_client_request_t *cr;
  int offer_sent = 0;

  msg_t *msg = NULL;
  sip_t *sip = NULL;

  char const *what;

  du = nua_dialog_usage_add(nh, nh->nh_ds, nua_session_usage, NULL);
  ss = nua_dialog_usage_private(du);
  cr = ss->ss_crequest;
  what = nua_internal_error;		/* Internal error */

  if (du == NULL)
    goto failure;

  if (cr->cr_orq) {
    what = "INVITE request already in progress";
    goto failure;
  }

  if (ss->ss_state == nua_callstate_terminated)
    ss->ss_state = nua_callstate_init;

  if (!restarted) {
    session_timer_preferences(ss, 
			      NH_PGET(nh, session_timer),
			      NH_PGET(nh, min_se),
			      NH_PGET(nh, refresher));
  }

  if (restarted && !cr->cr_msg) {
    if (du->du_msg)
      cr->cr_msg = msg_dup(du->du_msg);
    else
      restarted = 0;
  }

  msg = nua_creq_msg(nua, nh, cr, restarted,
		     SIP_METHOD_INVITE,
		     NUTAG_USE_DIALOG(1),
		     NUTAG_ADD_CONTACT(1),
		     TAG_NEXT(tags));
  sip = sip_object(msg);

  if (!sip) {
    what = "Cannot Initialize Request";
    goto failure;
  }

  if (!restarted) {
    msg_destroy(du->du_msg), du->du_msg = msg_dup(msg);
  }

  if (nh->nh_soa) {
    soa_init_offer_answer(nh->nh_soa);

    if (sip->sip_payload)
      offer_sent = 0;
    else if (soa_generate_offer(nh->nh_soa, 0, NULL) < 0)
      offer_sent = -1;
    else
      offer_sent = 1;
  }

  if (offer_sent >= 0) {
    sip_time_t invite_timeout = NH_PGET(nh, invite_timeout);
    if (invite_timeout == 0)
      invite_timeout = UINT_MAX;
    /* Cancel if we don't get response within timeout*/
    nua_dialog_usage_set_expires(du, invite_timeout);
    nua_dialog_usage_set_refresh(du, 0);

    /* Add session timer headers */
    if (session_timer_is_supported(nh))
      use_session_timer(ss, 0, prefer_session_timer(nh), msg, sip);

    ss->ss_100rel = NH_PGET(nh, early_media);
    ss->ss_precondition = sip_has_feature(sip->sip_require, "precondition");

    if (ss->ss_precondition)
      ss->ss_update_needed = ss->ss_100rel = 1;

    if (offer_sent > 0 &&
	session_include_description(nh->nh_soa, 1, msg, sip) < 0) {
      what = "Internal media error"; goto failure;
    }

    if (nh->nh_soa &&
	NH_PGET(nh, media_features) && !nua_dialog_is_established(nh->nh_ds) &&
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

    if (nh->nh_auth) {
      if (auc_authorize(&nh->nh_auth, msg, sip) < 0) {
	what = "Internal authentication error"; goto failure;
      }
    }

      cr->cr_orq = nta_outgoing_mcreate(nua->nua_nta,
					process_response_to_invite, nh, NULL,
					msg,
					NTATAG_REL100(ss->ss_100rel),
					SIPTAG_END(), TAG_NEXT(tags));

    if (cr->cr_orq) {
      cr->cr_offer_sent = offer_sent;
      cr->cr_usage = du;
      du->du_refresh = 0;
      signal_call_state_change(nh, ss, 0, "INVITE sent",
			       nua_callstate_calling, 0,
			       offer_sent ? "offer" : 0);
      return cr->cr_event = e;
    }
  }

 failure:

  msg_destroy(msg);
  if (du && !du->du_ready)
    nua_dialog_usage_remove(nh, nh->nh_ds, du), ss = NULL;

  UA_EVENT2(e, 900, what);
  signal_call_state_change(nh, ss, 900, what, nua_callstate_init, 0, 0);

  return e;
}

/** @NUA_EVENT nua_r_invite
 *
 * Answer to outgoing INVITE.
 *
 * The INVITE may be sent explicitly by nua_invite() or
 * implicitly by NUA state machine.
 *
 * @param status response status code
 *               (if the request is retried, @a status is 100, the @a
 *               sip->sip_status->st_status contain the real status code
 *               from the response message, e.g., 302, 401, or 407)
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the call
 * @param hmagic application context associated with the call
 * @param sip    response message to INVITE or NULL upon an error
 *               (status code is in @a status and 
 *                descriptive message in @a phrase parameters)
 * @param tags   empty
 *
 * @sa nua_invite(), @ref nua_call_model, #nua_i_state, #nua_i_invite, 
 * nua_ack(), NUTAG_AUTOACK()
 * 
 * @END_NUA_EVENT
 */

static int process_response_to_invite(nua_handle_t *nh,
				      nta_outgoing_t *orq,
				      sip_t const *sip)
{
  nua_t *nua = nh->nh_nua;
  nua_client_request_t *cr;
  nua_dialog_usage_t *du;
  nua_session_usage_t *ss;
  int status = sip->sip_status->st_status;
  char const *phrase = sip->sip_status->st_phrase;
  int terminated = 0;
  int gracefully = 1;
  char const *received = NULL;

  cr = nua_client_request_by_orq(nh->nh_ds->ds_cr, orq);
  du = cr ? cr->cr_usage : NULL;
  ss = nua_dialog_usage_private(du);
  
  assert(cr && du && ss);

  if (ss->ss_state == nua_callstate_terminating && 200 <= status) {
    /*
     * If the call is being terminated but re-INVITE was responded with 2XX
     * re-send the BYE, otherwise terminate the call.
     */
    gracefully = status < 300, terminated = !gracefully;
  }
  else if (status >= 300) {
    if (sip->sip_retry_after)
      gracefully = 0;

    terminated = sip_response_terminates_dialog(status, sip_method_invite,
						&gracefully);

    if (!terminated) {
      if (check_session_timer_restart(nh, ss, cr, orq, sip, restart_invite))
	return 0;

      if (ss->ss_state < nua_callstate_ready)
	terminated = 1;
    }
  }
  else if (status >= 200) {
    du->du_ready = 1;
    cr->cr_usage = NULL;

    /* XXX - check remote tag, handle forks */
    /* Set route, contact, nh_ds->ds_remote_tag */
    nua_dialog_uac_route(nh, nh->nh_ds, sip, 1);
    nua_dialog_store_peer_info(nh, nh->nh_ds, sip);

    init_session_timer(ss, sip, NH_PGET(nh, refresher));
    set_session_timer(ss);

    /* signal_call_state_change */
    if (session_process_response(nh, cr, orq, sip, &received) >= 0) {
      ss->ss_ack_needed = received ? received : "";

      if (NH_PGET(nh, auto_ack) ||
	  /* Auto-ACK response to re-INVITE unless auto_ack is set to 0 */
	  (ss->ss_state == nua_callstate_ready &&
	   !NH_PISSET(nh, auto_ack)))
	nua_stack_ack(nua, nh, nua_r_ack, NULL);
      else
	signal_call_state_change(nh, ss, status, phrase,
				 nua_callstate_completing, received, 0);
      nh_referral_respond(nh, SIP_200_OK);
      return 0;
    }

    status = 500, phrase = "Malformed Session in Response";

    nua_stack_ack(nua, nh, nua_r_ack, NULL);
    gracefully = 1;
  }
  else if (sip->sip_rseq) {
    /* Reliable provisional response */
    nh_referral_respond(nh, status, phrase);

    return process_100rel(nh, ss, orq, sip); /* signal_call_state_change */
  }
  else {
    /* Provisional response */
    nh_referral_respond(nh, status, phrase);
    session_process_response(nh, cr, orq, sip, &received);
    signal_call_state_change(nh, ss, status, phrase,
			     nua_callstate_proceeding, received, 0);
    return 0;
  }

  cr->cr_usage = NULL;

  nh_referral_respond(nh, status, phrase);
  nua_stack_process_response(nh, cr, orq, sip, TAG_END());

  if (terminated)
    signal_call_state_change(nh, ss, status, phrase,
			     nua_callstate_terminated, 0, 0);

  if (terminated < 0) {
    nua_dialog_terminated(nh, nh->nh_ds, status, phrase);
  }
  else if (terminated > 0) {
    nua_dialog_usage_remove(nh, nh->nh_ds, du);
  }
  else if (gracefully) {
    char *reason =
      su_sprintf(NULL, "SIP;cause=%u;text=\"%s\"", 
		 status > 699 ? 500 : status, phrase);

    signal_call_state_change(nh, ss, status, phrase,
			     nua_callstate_terminating, 0, 0);

    nua_stack_post_signal(nh, nua_r_bye,
			  SIPTAG_REASON_STR(reason),
			  TAG_END());

    su_free(NULL, reason);
  }

  return 0;
}

/**@fn void nua_ack(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...);
 *
 * Acknowledge a succesful response to INVITE request.
 *
 * Acknowledge a successful response (200..299) to INVITE request with the
 * SIP ACK request message. This function is need only if NUTAG_AUTOACK()
 * parameter has been cleared.
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return 
 *    nothing
 *
 * @par Related Tags:
 *    Tags in <sip_tag.h>
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
  nua_session_usage_t *ss;
  nua_client_request_t *cr;
  nta_outgoing_t *ack = NULL;
  msg_t *msg;
  sip_t *sip;
  int status = 200;
  char const *phrase = "OK", *reason = NULL, *sent = NULL;
  char const *received;

  ss = nua_session_usage_get(nh->nh_ds);
  cr = ss->ss_crequest;

  received = ss ? ss->ss_ack_needed : NULL;

  if (!received)
    return UA_EVENT2(nua_i_error, 900, "No response to ACK");

  ss->ss_ack_needed = 0;

  if (!received[0])
    received = NULL;

  if (tags)
    nua_stack_set_params(nua, nh, nua_i_error, tags);

  msg = nua_creq_msg(nua, nh, cr, 0,
		     SIP_METHOD_ACK,
		     /* NUTAG_COPY(0), */
		     TAG_NEXT(tags));
  sip = sip_object(msg);

  if (sip && nh->nh_soa) {
    if (tags)
      soa_set_params(nh->nh_soa, TAG_NEXT(tags));

    if (cr->cr_offer_recv && !cr->cr_answer_sent) {
      if (soa_generate_answer(nh->nh_soa, NULL) < 0 ||
	  session_include_description(nh->nh_soa, 1, msg, sip) < 0) {
	reason = soa_error_as_sip_reason(nh->nh_soa);
	status = 900, phrase = "Internal media error";
	reason = "SIP;cause=500;text=\"Internal media error\"";
      }
      else {
	cr->cr_answer_sent = 1;
	soa_activate(nh->nh_soa, NULL);

	/* signal that O/A round is complete */
	sent = "answer";
      }
    }

    if (!reason &&
	/* ss->ss_offer_sent && !ss->ss_answer_recv */
	!soa_is_complete(nh->nh_soa)) {
      /* No SDP answer in 2XX response -> terminate call */
      status = 988, phrase = "Incomplete offer/answer";
      reason = "SIP;cause=488;text=\"Incomplete offer/answer\"";
    }
  }

  if (sip) {
    msg_t *imsg = nta_outgoing_getrequest(cr->cr_orq);
    sip_t const *isip = sip_object(imsg);
    if (isip->sip_proxy_authorization)
      sip_add_dup(msg, sip, (void *)isip->sip_proxy_authorization);
    if (isip->sip_authorization)
      sip_add_dup(msg, sip, (void *)isip->sip_authorization);
    msg_destroy(imsg);
  }

  if (sip)
    ack = nta_outgoing_mcreate(nua->nua_nta, NULL, NULL, NULL, msg,
			       SIPTAG_END(), TAG_NEXT(tags));

  if (!ack) {
    if (!reason) {
      status = 900, phrase = "Cannot send ACK";
      reason = "SIP;cause=500;text=\"Internal Error\"";
    }
    msg_destroy(msg);
  }

  nua_creq_deinit(cr, NULL);	/* Destroy INVITE transaction */
  nta_outgoing_destroy(ack);	/* TR engine keeps this around for T2 */

  if (status < 300) {
    signal_call_state_change(nh, ss, status, phrase, nua_callstate_ready,
			     received, sent);
  }
  else {
    signal_call_state_change(nh, ss, status, phrase, nua_callstate_terminating,
			     0, 0);
    nua_stack_post_signal(nh, nua_r_bye,
			  SIPTAG_REASON_STR(reason),
			  TAG_END());
  }

  return 0;
}


/* Process reliable provisional response */
static int
process_100rel(nua_handle_t *nh,
	       nua_session_usage_t *ss,
	       nta_outgoing_t *orq,
	       sip_t const *sip)
{
  nua_client_request_t *cr_invite = ss->ss_crequest;
  nua_client_request_t *cr_prack = nh->nh_ds->ds_cr;
  
  sip_rseq_t *rseq;
  char const *recv = NULL;
  int status; char const *phrase;

  if (cr_prack->cr_orq) {
    /* XXX - better luck next time */
    SU_DEBUG_3(("nua(%p): cannot send PRACK because %s is pending\n", nh,
		nta_outgoing_method_name(cr_prack->cr_orq)));
    return 0; /* Wait until this response is re-transmitted */
  }

  if (!nua_dialog_is_established(nh->nh_ds)) {
    /* Establish early dialog */
    nua_dialog_uac_route(nh, nh->nh_ds, sip, 1);
    nua_dialog_store_peer_info(nh, nh->nh_ds, sip);
    
    /* Tag the INVITE request */
    cr_invite->cr_orq =
      nta_outgoing_tagged(orq, process_response_to_invite, nh,
			  sip->sip_to->a_tag, sip->sip_rseq);
    nta_outgoing_destroy(orq);
    orq = cr_invite->cr_orq;
  }
  
  assert(sip);

  status = sip->sip_status->st_status, phrase = sip->sip_status->st_phrase;
  rseq = sip->sip_rseq;

  if (!rseq) {
    SU_DEBUG_5(("nua(%p): 100rel missing RSeq\n", nh));
  }
  else if (rseq->rs_response <= nta_outgoing_rseq(orq)) {
    SU_DEBUG_5(("nua(%p): 100rel bad RSeq %u (got %u)\n", nh, 
		(unsigned)rseq->rs_response,
		nta_outgoing_rseq(orq)));
    /* XXX - send nua_r_invite event or not? */
    return 0;
  }
  else if (nta_outgoing_setrseq(orq, rseq->rs_response) < 0) {
    SU_DEBUG_1(("nua(%p): cannot set RSeq %u\n", nh, 
		(unsigned)rseq->rs_response));
  }
  else if (session_process_response(nh, cr_invite, orq, sip, &recv) < 0) {
    assert(nh->nh_soa);
    status = soa_error_as_sip_response(nh->nh_soa, &phrase);
    nua_stack_event(nh->nh_nua, nh, NULL,
		    nua_i_media_error, status, phrase, TAG_END());
  }
  /* Here we could let application PRACK and just send state event */
  else {
    sip_rack_t rack[1];
    tagi_t tags[] = {
      { TAG_SKIP(nua_stack_prack) }, /* this is autoprack */
      { NUTAG_STATUS(status), },
      { NUTAG_PHRASE(phrase), },
      { NUTAG_PHRASE(recv), },
      { SIPTAG_RACK(rack) }, 
      { TAG_END() }
    };

    sip_rack_init(rack);

    rack->ra_response    = sip->sip_rseq->rs_response;
    rack->ra_cseq        = sip->sip_cseq->cs_seq;
    rack->ra_method      = sip->sip_cseq->cs_method;
    rack->ra_method_name = sip->sip_cseq->cs_method_name;

    nua_stack_prack(nh->nh_nua, nh, nua_r_prack, tags);

    return 0;
  }

  /* XXX - CANCEL INVITE or BYE this session? */
  /* Because we don't do forking very well we just cancel INVITE */
  nua_stack_cancel(nh->nh_nua, nh, nua_r_cancel, NULL);

  return 0;
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


int nua_stack_prack(nua_t *nua, nua_handle_t *nh, nua_event_t e,
		    tagi_t const *tags)
{
  nua_session_usage_t *ss;
  nua_client_request_t *cr;
  msg_t *msg;
  sip_t *sip;
  int offer_sent_in_prack = 0, answer_sent_in_prack = 0;

  int status = 0; char const *phrase = "PRACK sent";
  char const *recv = NULL, *sent = NULL;

  int autoprack =		/* XXX - should have common indication */
    tags && tags->t_tag == tag_skip && 
    tags->t_value == (tag_value_t)nua_stack_prack;

  if (autoprack) {
    status = (int)tags[1].t_value; 
    phrase = (char const *)tags[2].t_value;
    recv = (char const *)tags[3].t_value;
    tags += 4;
  }

  ss = nua_session_usage_get(nh->nh_ds);

  if (!ss || !ss->ss_crequest || !nta_outgoing_rseq(ss->ss_crequest->cr_orq))
    return UA_EVENT2(e, 900, "Nothing to PRACK");
  else if (nh->nh_ds->ds_cr->cr_orq)
    return UA_EVENT2(e, 900, "Request already in progress");

  nua_stack_init_handle(nua, nh, TAG_NEXT(tags));

  cr = nh->nh_ds->ds_cr;

  msg = nua_creq_msg(nua, nh, cr, cr->cr_retry_count,
		     SIP_METHOD_PRACK,
		     NUTAG_USE_DIALOG(1),
		     NUTAG_ADD_CONTACT(1),
		     TAG_NEXT(tags));

  sip = sip_object(msg);

  if (sip) {
    nua_client_request_t *cri = ss->ss_crequest;
    if (nh->nh_soa == NULL)
      /* It is up to application to handle SDP */;
    else if (sip->sip_payload)
      /* XXX - we should just do MIME in session_include_description() */;
    else if (cri->cr_offer_recv && !cri->cr_answer_sent) {

      if (soa_generate_answer(nh->nh_soa, NULL) < 0 ||
	  session_include_description(nh->nh_soa, 1, msg, sip) < 0) {

	status = soa_error_as_sip_response(nh->nh_soa, &phrase);
	SU_DEBUG_3(("nua(%p): PRACK answer: %d %s\n", nh, status, phrase));
	nua_stack_event(nh->nh_nua, nh, NULL,
			nua_i_media_error, status, phrase, TAG_END());

	goto error;
      }
      else {
	answer_sent_in_prack = 1, sent = "answer";
	soa_activate(nh->nh_soa, NULL);
      }
    }
    /* When 100rel response status was 183 fake support for preconditions */
    else if (autoprack && status == 183 && ss->ss_precondition) {

      if (soa_generate_offer(nh->nh_soa, 0, NULL) < 0 ||
	  session_include_description(nh->nh_soa, 1, msg, sip) < 0) {

	status = soa_error_as_sip_response(nh->nh_soa, &phrase);
	SU_DEBUG_3(("nua(%p): PRACK offer: %d %s\n", nh, status, phrase));
	nua_stack_event(nh->nh_nua, nh, NULL,
			nua_i_media_error, status, phrase, TAG_END());
	goto error;
      }
      else {
	offer_sent_in_prack = 1, sent = "offer";
      }
    }

    if (nh->nh_auth) {
      if (auc_authorize(&nh->nh_auth, msg, sip) < 0)
	/* xyzzy */;
    }

    cr->cr_orq = nta_outgoing_mcreate(nua->nua_nta,
				      process_response_to_prack, nh, NULL,
				      msg,
				      SIPTAG_END(), TAG_NEXT(tags));
    if (cr->cr_orq) {
      cr->cr_usage = nua_dialog_usage_public(ss);
      cr->cr_event = nua_r_prack;

      if (answer_sent_in_prack)
	cri->cr_answer_sent = 1;
      else if (offer_sent_in_prack)
	cr->cr_offer_sent = 1;

      if (autoprack) 
	signal_call_state_change(nh, ss, status, phrase,
				 nua_callstate_proceeding, recv, sent);
      else
	signal_call_state_change(nh, ss, 0, "PRACK sent",
				 nua_callstate_proceeding, NULL, sent);
	

      return cr->cr_event = e;
    }
  }

 error:
  msg_destroy(msg);
  return UA_EVENT1(e, NUA_INTERNAL_ERROR);
}

void restart_prack(nua_handle_t *nh, tagi_t *tags)
{
  nua_creq_restart(nh, nh->nh_ds->ds_cr, process_response_to_prack, tags);
}


static int
process_response_to_prack(nua_handle_t *nh,
			  nta_outgoing_t *orq,
			  sip_t const *sip)
{
  nua_client_request_t *cr = nh->nh_ds->ds_cr;
  nua_session_usage_t *ss = nua_dialog_usage_private(cr->cr_usage);
  int status;
  char const *phrase = "OK", *reason = NULL, *recv = NULL;

  assert(cr->cr_usage && cr->cr_usage->du_class == nua_session_usage);

  if (sip)
    status = sip->sip_status->st_status, phrase = sip->sip_status->st_phrase;
  else
    status = 408, phrase = sip_408_Request_timeout;

  SU_DEBUG_5(("nua: process_response_to_prack: %u %s\n", status, phrase));

  if (nua_creq_check_restart(nh, cr, orq, sip, restart_prack))
    return 0;

  if (status < 200)
    return 0;

  cr->cr_usage = NULL;

  if (status < 300) {
    if (session_process_response(nh, cr, orq, sip, &recv) < 0) {
      status = 900, phrase = "Malformed Session in Response";
      reason = "SIP;status=400;phrase=\"Malformed Session in Response\"";
    }
  }
  else
    nua_stack_process_response(nh, cr, orq, sip, TAG_END());

  if (recv)
    signal_call_state_change(nh, ss, status, phrase,
			     nua_callstate_proceeding, recv, NULL);

  if (status < 300 && ss->ss_update_needed)
    nua_stack_update(nh->nh_nua, nh, nua_r_update, NULL);

  return 0;
}

/** Refresh session usage */
static void nua_session_usage_refresh(nua_handle_t *nh,
				      nua_dialog_state_t *ds,
				      nua_dialog_usage_t *du,
				      sip_time_t now)
{
  tagi_t const timer_tags[2] = {
    { SIPTAG_SUBJECT_STR("Session refresh") }, 
    { TAG_END() }
  };
  tagi_t const refresh_tags[2] = {
    { SIPTAG_SUBJECT_STR("Dialog refresh") }, 
    { TAG_END() }
  };

  nua_session_usage_t const *ss = nua_dialog_usage_private(du);
  nua_client_request_t const *cri = ss->ss_crequest, *cro = ds->ds_cr;
  nua_server_request_t const *sr;

  for (sr = ds->ds_sr; sr; sr = sr->sr_next)
    if (sr->sr_usage == du && 
	(sr->sr_method == sip_method_invite || 
	 sr->sr_method == sip_method_update))
      break;

  /* INVITE or UPDATE in progress or being authenticated */
  if ((cri && cri->cr_orq) || sr)	
    return;
  if (ss->ss_state >= nua_callstate_terminating)
    return;

  if (!ss->ss_refresher) {
    if (now >= du->du_expires)
      session_timeout(nh, du, now);
    else
      /* Refreshing contact & route set */
      nua_stack_invite2(nh->nh_nua, nh, nua_r_invite, 1, refresh_tags);
  }
  else if (NH_PGET(nh, update_refresh)) {
    if (!cro->cr_orq)
      nua_stack_update(nh->nh_nua, nh, nua_r_update, timer_tags);
    else
      nua_dialog_usage_refresh_range(du, 5, 15);
  }
  else {
    nua_stack_invite2(nh->nh_nua, nh, nua_r_invite, 1, timer_tags);
  }
}

static
char const reason_timeout[] = "SIP;cause=408;text=\"Session timeout\"";

static void
session_timeout(nua_handle_t *nh, nua_dialog_usage_t *du, sip_time_t now)
{
  if (now > 1) {
    nua_session_usage_t *ss = nua_dialog_usage_private(du);

    signal_call_state_change(nh, ss, 408, "Session Timeout",
			     nua_callstate_terminating, NULL, NULL);

    nua_stack_post_signal(nh, nua_r_bye,
			  SIPTAG_REASON_STR(reason_timeout),
			  TAG_END());
  }
}

/** Terminate usage/dialog/handle/agent gracefully */
static int nua_session_usage_shutdown(nua_handle_t *nh,
				      nua_dialog_state_t *ds,
				      nua_dialog_usage_t *du)
{
  nua_session_usage_t *ss = nua_dialog_usage_private(du);
  nua_client_request_t *cr;
  nua_server_request_t *sr, *sr_next;
  int status;

  /* Zap client-side invite transaction */
  if (ss->ss_crequest->cr_orq) {
    cr = ss->ss_crequest;
    status = nta_outgoing_status(cr->cr_orq);

    if (status < 200) 
      nta_outgoing_tcancel(cr->cr_orq, NULL, NULL, TAG_END());

    if (ss->ss_ack_needed) {
      msg_t *ack = nua_creq_msg(nh->nh_nua, nh, cr, 0,
				SIP_METHOD_ACK,
				TAG_END());
      nta_outgoing_mcreate(nh->nh_nua->nua_nta, NULL, NULL, NULL, 
			   ack, TAG_END());
    }

    nua_creq_deinit(cr, NULL);
  }

  /* Zap server-side transactions */
  for (sr = ds->ds_sr; sr; sr = sr_next) {
    sr_next = sr->sr_next;
    if (sr->sr_usage == du) {
      assert(sr->sr_usage == du);
      sr->sr_usage = NULL;
      if (sr->sr_respond) 
	nua_server_respond(sr, SIP_480_TEMPORARILY_UNAVAILABLE, TAG_END());
      else
	nua_server_request_destroy(sr);
    }
  }

  assert(ss == nua_session_usage_get(nh->nh_ds));

  switch (ss->ss_state) {
  case nua_callstate_completing:
  case nua_callstate_ready:
  case nua_callstate_completed:
    {
      msg_t *bye;

      cr = ds->ds_cr;
      nua_creq_deinit(cr, NULL);
      bye = nua_creq_msg(nh->nh_nua, nh, ds->ds_cr, 0, 
			 SIP_METHOD_BYE,
			 TAG_END());
      cr->cr_orq = nta_outgoing_mcreate(nh->nh_nua->nua_nta,
					NULL, NULL, NULL,
					bye,
					TAG_END());
      nua_creq_deinit(cr, NULL);
    }
  }

  nua_dialog_usage_remove(nh, ds, du);

  return 0;
}

/** Restart invite (e.g., after 302 or 407) */
void
restart_invite(nua_handle_t *nh, tagi_t *tags)
{
  nua_stack_invite2(nh->nh_nua, nh, nua_r_invite, 1, tags);
}

static int process_response_to_cancel(nua_handle_t *nh,
				      nta_outgoing_t *orq,
				      sip_t const *sip);

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
 *    Tags in <sip_tag.h>
 *
 * @par Events:
 *    #nua_r_cancel, #nua_i_state  (#nua_i_active, #nua_i_terminated)
 *
 * @sa @ref nua_call_model, nua_invite(), #nua_i_cancel
 */

int
nua_stack_cancel(nua_t *nua, nua_handle_t *nh, nua_event_t e,
		 tagi_t const *tags)
{
  nua_session_usage_t *ss;
  nua_client_request_t *cri, *crc;

  ss = nua_session_usage_get(nh->nh_ds);

  if (!nh || !ss || !ss->ss_crequest->cr_usage ||
      nta_outgoing_status(ss->ss_crequest->cr_orq) >= 200) {
    return UA_EVENT2(e, 481, "No transaction to CANCEL");
  }

  cri = ss->ss_crequest;
  crc = nh->nh_ds->ds_cr;

  if (tags)
    nua_stack_set_params(nua, nh, nua_i_error, tags);

  if (nh && cri->cr_orq && cri->cr_usage) {
    nta_outgoing_t *orq;

    /* nh_referral_respond(nh, SIP_487_REQUEST_TERMINATED); */

    if (e)
      orq = nta_outgoing_tcancel(cri->cr_orq, process_response_to_cancel, nh,
				 TAG_NEXT(tags));
    else
      orq = nta_outgoing_tcancel(cri->cr_orq, NULL, NULL, TAG_NEXT(tags));

    if (orq == NULL)
      return nua_stack_event(nua, nh, NULL, e, 400, "Internal error",
			     TAG_END());

    if (e && crc->cr_orq == NULL)
      crc->cr_orq = orq, crc->cr_event = e;
  }

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



static int process_response_to_cancel(nua_handle_t *nh,
				      nta_outgoing_t *orq,
				      sip_t const *sip)
{
  return nua_stack_process_response(nh, nh->nh_ds->ds_cr, orq, sip, TAG_END());
}

/* ---------------------------------------------------------------------- */
/* UAS side of INVITE */

static int respond_to_invite(nua_server_request_t *sr, tagi_t const *tags);

static int
  preprocess_invite(nua_t *, nua_handle_t *, nua_server_request_t **, sip_t *),
  session_check_request(nua_t *nua,
			nua_handle_t *nh,
			nta_incoming_t *irq,
			sip_t const *sip),
  process_invite(nua_t *, nua_handle_t *, nua_server_request_t *, sip_t *),
  process_prack(nua_handle_t *, nta_reliable_t *, nta_incoming_t *,
		sip_t const *);

static int
  process_ack_or_cancel(nua_server_request_t *, nta_incoming_t *, 
			sip_t const *),
  process_ack(nua_server_request_t *, nta_incoming_t *, sip_t const *),
  process_cancel(nua_server_request_t *, nta_incoming_t *, sip_t const *),
  process_timeout(nua_server_request_t *, nta_incoming_t *);

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
 * included in message
 *
 * @par Preliminary Responses and 100rel
 *
 * Call progress can be signaled with preliminary responses (with status
 * code in the range 101..199). It is possible to conclude the SDP
 * Offer-Answer negotiation using preliminary responses, too. If
 * SOATAG_USER_SDP() or SOATAG_USER_SDP_STR() parameter is included with in
 * a preliminary nua_response(), the SDP answer is generated and sent with
 * the preliminary responses, too.
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
 * @RFC3262, NUTAG_EARLY_MEDIA(), NUTAG_ONLY183_100REL(), 
 * NUTAG_INCLUDE_EXTRA_SDP(),
 * #nua_i_prack, #nua_i_update, nua_update(),
 * nua_invite(), #nua_r_invite
 *
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

/** @internal Process incoming INVITE. */
int nua_stack_process_invite(nua_t *nua,
			     nua_handle_t *nh,
			     nta_incoming_t *irq,
			     sip_t const *sip)
{
  nua_server_request_t *sr, sr0[1];
  int status;
  
  sr = SR_INIT(sr0);
  sr->sr_irq = irq;

  status = preprocess_invite(nua, nh, &sr, (sip_t *)sip);

  if (status) {
    if (sr->sr_status > 100) 
      nta_incoming_treply(irq, sr->sr_status, sr->sr_phrase,
			  SIPTAG_USER_AGENT_STR(NUA_PGET(nua, nh, user_agent)),
			  TAG_END());
    nua_server_request_destroy(sr);
    /* if something has failed, respond with 500 Internal Server Error */
    return 500; 
  }

  assert(sr != sr0);

  return process_invite(nua, sr->sr_owner, sr, (sip_t *)sip);
}

/** @internal Preprocess incoming invite - sure we have a valid request. 
 * 
 * @return 0 if request is valid, or error statuscode when request has been 
 * responded.
 */
static
int preprocess_invite(nua_t *nua,
		      nua_handle_t *nh,
		      nua_server_request_t **inout_sr,
		      sip_t *sip)
{
  nua_dialog_state_t *ds;
  nua_server_request_t *sr = *inout_sr;
  nua_server_request_t const *sr0;
  nua_dialog_usage_t *du;
  nua_session_usage_t *ss;
  int have_sdp;
  char const *sdp;
  size_t len;

  if (nh) {
    ds = nh->nh_ds;
    du = nua_dialog_usage_get(ds, nua_session_usage, NULL);
    ss = nua_dialog_usage_private(du);
  }
  else {
    nh = nua->nua_dhandle, ds = NULL, du = NULL, ss = NULL;
  }

  sr->sr_usage = du;

  if (!NUA_PGET(nua, nh, invite_enable))
    return SR_STATUS1(sr, SIP_403_FORBIDDEN);

  if (session_check_request(nua, nh, sr->sr_irq, sip))
    return 500;

  have_sdp = session_get_description(sip, &sdp, &len);

  if (ss) {
    /* Existing session */ 

    for (sr0 = ds->ds_sr; sr0; sr0 = sr0->sr_next) {
      /* Final response have not been sent to previous INVITE */
      if (sr0->sr_method == sip_method_invite && sr0->sr_respond)
	break;
      /* Or we have sent offer but have not received answer */
      if (have_sdp && sr0->sr_offer_sent && !sr0->sr_answer_recv)
	break;
      /* Or we have received request with offer but not sent answer */
      if (have_sdp && sr0->sr_offer_recv && !sr0->sr_answer_sent)
	break;
    }
    
    if (sr0)
      /* Overlapping invites - RFC 3261 14.2 */
      return respond_with_retry_after(nh, sr->sr_irq, 
				      500, "Overlapping Requests",
				      0, 10);

    if ((ss->ss_crequest && ss->ss_crequest->cr_orq) ||
	(have_sdp && ds && ds->ds_cr->cr_orq && ds->ds_cr->cr_offer_sent)) {
      /* Glare - RFC 3261 14.2 and RFC 3311 section 5.2 */
      return SR_STATUS1(sr, SIP_491_REQUEST_PENDING);
    }
  }

  /* Create handle and server request structure when needed */
  sr = nua_server_request(nua, nh, sr->sr_irq, sip, sr, sizeof *sr,
			  respond_to_invite, create_dialog);
  *inout_sr = sr;

  if (sr->sr_status > 100)
    return sr->sr_status;

  nh = sr->sr_owner; assert(nh != nua->nua_dhandle);
  ds = nh->nh_ds;

  if (nh->nh_soa) {
    soa_init_offer_answer(nh->nh_soa);

    if (have_sdp) {
      if (soa_set_remote_sdp(nh->nh_soa, NULL, sdp, len) < 0) {
	SU_DEBUG_5(("nua(%p): error parsing SDP in INVITE\n", nh));
	return SR_STATUS(sr, 400, "Bad Session Description");
      }
      else
	sr->sr_offer_recv = 1;
    }
  }

  /* Add the session usage */
  if (du == NULL)
    du = nua_dialog_usage_add(nh, nh->nh_ds, nua_session_usage, NULL);

  if (!du)
    return SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);

  sr->sr_usage = du;

  return 0;
}

static int
session_check_request(nua_t *nua,
		      nua_handle_t *nh,
		      nta_incoming_t *irq,
		      sip_t const *sip)
{
  char const *user_agent = NUA_PGET(nua, nh, user_agent);

  if (nh->nh_soa) {
    /* Make sure caller uses application/sdp without compression */
    if (nta_check_session_content(irq, sip,
				  nua->nua_invite_accept,
				  SIPTAG_USER_AGENT_STR(user_agent),
				  SIPTAG_ACCEPT_ENCODING_STR(""),
				  TAG_END()))
      return 415;

    /* Make sure caller accepts application/sdp */
    if (nta_check_accept(irq, sip,
			 nua->nua_invite_accept,
			 NULL,
			 SIPTAG_USER_AGENT_STR(user_agent),
			 SIPTAG_ACCEPT_ENCODING_STR(""),
			 TAG_END()))
      return 406;
  }

  if (sip->sip_session_expires) {
    unsigned min_se = NH_PGET(nh, min_se);
    if (sip->sip_min_se && min_se < sip->sip_min_se->min_delta)
      min_se = sip->sip_min_se->min_delta;
    if (nta_check_session_expires(irq, sip,
				  min_se,
				  SIPTAG_USER_AGENT_STR(user_agent),
				  TAG_END()))
      return 422;
  }

  return 0;
}

/** @internal Process incoming invite - initiate media, etc. */
static
int process_invite(nua_t *nua,
		   nua_handle_t *nh,
		   nua_server_request_t *sr,
		   sip_t *sip)
{
  nua_session_usage_t *ss = nua_dialog_usage_private(sr->sr_usage);
  int status = sr->sr_status; char const *phrase = sr->sr_phrase;

  assert(ss); assert(status == 100);

  ss->ss_100rel = NH_PGET(nh, early_media);
  ss->ss_precondition = sip_has_feature(sip->sip_require, "precondition");
  if (ss->ss_precondition)
    ss->ss_100rel = 1;

  session_timer_preferences(ss, 
			    NH_PGET(nh, session_timer),
			    NH_PGET(nh, min_se),
			    NH_PGET(nh, refresher));

  /* Session Timer negotiation */
  if (sip_has_supported(NH_PGET(nh, supported), "timer"))
    init_session_timer(ss, sip, ss->ss_refresher);

  nua_dialog_uas_route(nh, nh->nh_ds, sip, 1);	/* Set route and tags */

  nta_incoming_bind(sr->sr_irq, process_ack_or_cancel, sr);

  assert(ss->ss_state >= nua_callstate_ready ||
	 ss->ss_state == nua_callstate_init);

  if (NH_PGET(nh, auto_answer) ||
      /* Auto-answer to re-INVITE unless auto_answer is set to 0 on handle */
      (ss->ss_state == nua_callstate_ready &&
       /* Auto-answer requires enabled media (soa). 
	* XXX - if the re-INVITE modifies the media we should not auto-answer.
	*/
       nh->nh_soa &&
       !NH_PISSET(nh, auto_answer))) {
    SET_STATUS1(SIP_200_OK);
  }
  else if (NH_PGET(nh, auto_alert)) {
    if (ss->ss_100rel &&
	(sip_has_feature(nh->nh_ds->ds_remote_ua->nr_supported, "100rel") ||
	 sip_has_feature(nh->nh_ds->ds_remote_ua->nr_require, "100rel"))) {
      SET_STATUS1(SIP_183_SESSION_PROGRESS);
    }
    else {
      SET_STATUS1(SIP_180_RINGING);
    }
  }

  /* Magical value indicating autoanswer within respond_to_invite() */
#define AUTOANSWER ((void*)-1)

  if (status > 100) {
    sr->sr_auto = 1;
    nua_server_respond(sr, status, phrase, TAG_END());
    sr->sr_auto = 0;
    return 0;
  }

  nta_incoming_treply(sr->sr_irq, SIP_100_TRYING, 
		      SIPTAG_USER_AGENT_STR(NUA_PGET(nua, nh, user_agent)),
		      TAG_END());

  nua_stack_event(nh->nh_nua, nh, 
		  sr->sr_msg = nta_incoming_getrequest(sr->sr_irq),
		  nua_i_invite, SIP_100_TRYING,
		  NH_ACTIVE_MEDIA_TAGS(1, nh->nh_soa),
		  TAG_END());

  signal_call_state_change(nh, ss, SIP_100_TRYING,
			   nua_callstate_received,
			   sr->sr_offer_recv ? "offer" : 0, 0);

  return 0;
}

/** @internal Respond to an INVITE request.
 *
 * XXX - use tags to indicate when to use reliable responses.
 * XXX - change prototype.
 */
static
int respond_to_invite(nua_server_request_t *sr, tagi_t const *tags)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_t *nua = nh->nh_nua;
  nua_dialog_state_t *ds = nh->nh_ds;
  nua_dialog_usage_t *du;
  nua_session_usage_t *ss;
  msg_t *msg;
  sip_t *sip;
  int reliable;
  int status = sr->sr_status; char const *phrase = sr->sr_phrase;
  sip_warning_t *warning = NULL;

  int offer = 0, answer = 0, early_answer = 0;

  enter;

  du = sr->sr_usage, ss = nua_dialog_usage_private(du);

  if (du == NULL)
    return nua_default_respond(sr, tags);

  assert(ss == nua_session_usage_get(nh->nh_ds));

  if (tags) {
    nua_stack_set_params(nua, nh, nua_i_error, tags);

    if (!NHP_ISSET(nh->nh_prefs, early_answer)
	&& 100 < status && status < 200) {
      sdp_session_t const *user_sdp = NULL;
      char const *user_sdp_str = NULL;

      tl_gets(tags,
	      SOATAG_USER_SDP_REF(user_sdp),
	      SOATAG_USER_SDP_STR_REF(user_sdp_str),
	      TAG_END());

      early_answer = user_sdp || user_sdp_str;
    }
    else
      early_answer = NH_PGET(nh, early_answer);
  }

  msg = nua_server_response(sr,
			    status, phrase,
			    TAG_IF(status < 300, NUTAG_ADD_CONTACT(1)),
			    SIPTAG_SUPPORTED(NH_PGET(nh, supported)),
			    TAG_NEXT(tags));
  sip = sip_object(msg);

  if (!sip) {
    SET_STATUS1(SIP_500_INTERNAL_SERVER_ERROR), reliable = 0;
    goto send_response;
  }

  reliable =
    (status >= 200)
    || (status > 100 && sip->sip_require &&
	sip_has_feature(sip->sip_require, "100rel"))
    || (status > 100 &&
	ds->ds_remote_ua->nr_require &&
	sip_has_feature(ds->ds_remote_ua->nr_require, "100rel"))
    || (status > 100 && !NH_PGET(nh, only183_100rel) &&
	(NH_PGET(nh, early_media) ||
	 (ds->ds_remote_ua->nr_require &&
	  sip_has_feature(ds->ds_remote_ua->nr_require, "precondition"))) &&
	ds->ds_remote_ua->nr_supported &&
	sip_has_feature(ds->ds_remote_ua->nr_supported, "100rel"))
    || (status == 183 &&
	ds->ds_remote_ua->nr_supported &&
	sip_has_feature(ds->ds_remote_ua->nr_supported, "100rel"))
    || (status == 183 &&
	ds->ds_remote_ua->nr_require &&
	sip_has_feature(ds->ds_remote_ua->nr_require, "precondition"))
    || (status > 100 &&
	ds->ds_remote_ua->nr_require &&
	sip_has_feature(ds->ds_remote_ua->nr_require, "precondition") &&
	sr->sr_offer_recv && !sr->sr_answer_sent);

  if (!nh->nh_soa)
    /* Xyzzy */;
  else if (status >= 300) {
    soa_clear_remote_sdp(nh->nh_soa);
  }
  else {
    int extra = 0;

    if (sr->sr_offer_sent && !sr->sr_answer_recv)
      /* Wait for answer */;
    else if (sr->sr_offer_recv && sr->sr_answer_sent > 1) {
      /* We have sent answer */
      /* ...  but we may want to send it again */
      tagi_t const *t = tl_find_last(tags, nutag_include_extra_sdp);
      extra = t && t->t_value;
    }
    else if (sr->sr_offer_recv && !sr->sr_answer_sent && 
	     (reliable || early_answer)) {
      /* Generate answer */ 
      if (soa_generate_answer(nh->nh_soa, NULL) >= 0) {
	answer = 1;
	soa_activate(nh->nh_soa, NULL);
	/* signal that O/A answer sent (answer to invite) */
      }
      else if (status >= 200) {
	int wcode;
	char const *text;
	char const *host = "invalid.";
	status = soa_error_as_sip_response(nh->nh_soa, &phrase);

	wcode = soa_get_warning(nh->nh_soa, &text);
	if (wcode) {
	  if (sip->sip_contact)
	    host = sip->sip_contact->m_url->url_host;
	  warning = sip_warning_format(msg_home(msg), "%u %s \"%s\"",
				       wcode, host, text);
	}
      }
      else {
	/* 1xx - we don't have to send answer */
      }
    }
    else if (sr->sr_offer_recv && sr->sr_answer_sent == 1 && 
	     (reliable || early_answer)) {
      /* The answer was sent unreliably, keep sending it */
      answer = 1;
    }
    else if (!sr->sr_offer_recv && !sr->sr_offer_sent && reliable) {
      /* Generate offer */
      if (soa_generate_offer(nh->nh_soa, 0, NULL) < 0)
	status = soa_error_as_sip_response(nh->nh_soa, &phrase);
      else
	offer = 1;
    }

    if (offer || answer || extra) {
      if (session_include_description(nh->nh_soa, 1, msg, sip) < 0)
	SET_STATUS1(SIP_500_INTERNAL_SERVER_ERROR);
    }
  }

  if (ss->ss_refresher && 200 <= status && status < 300)
    if (session_timer_is_supported(nh))
      use_session_timer(ss, 1, 1, msg, sip);

  if (reliable && status < 200) {
    nta_reliable_t *rel;
    rel = nta_reliable_mreply(sr->sr_irq,
			      process_prack, nh, msg);
    if (!rel)
      SET_STATUS1(SIP_500_INTERNAL_SERVER_ERROR);
  }

 send_response:

  if (reliable && status < 200)
    /* we are done */;
  else if (status != sr->sr_status) {    /* Error responding */
    assert(status >= 200);
    sr->sr_respond = NULL;
    nta_incoming_treply(sr->sr_irq,
			status, phrase,
			SIPTAG_WARNING(warning),
			SIPTAG_USER_AGENT_STR(NH_PGET(nh, user_agent)),
			TAG_END());
    msg_destroy(msg), msg = NULL;
  }
  else {
    if (status >= 200)
      sr->sr_respond = NULL;
    nta_incoming_mreply(sr->sr_irq, msg);
  }

  if (sr->sr_auto) {
    msg_t *request = nta_incoming_getrequest(sr->sr_irq);
    if (status < 200)
      sr->sr_msg = request;
    nua_stack_event(nh->nh_nua, nh, request,
		    nua_i_invite, status, phrase,
		    NH_ACTIVE_MEDIA_TAGS(1, nh->nh_soa),
		    TAG_END());
  }
  else if (status != sr->sr_status)
    nua_stack_event(nua, nh, NULL, nua_i_error, status, phrase, TAG_END());

  sr->sr_status = status, sr->sr_phrase = phrase;

  if (status >= 300)
    offer = 0, answer = 0;

  if (offer)
    sr->sr_offer_sent = 1;
  else if (answer)
    sr->sr_answer_sent = 1 + reliable;

  /* Update session state */
  assert(ss->ss_state != nua_callstate_calling);
  assert(ss->ss_state != nua_callstate_proceeding);

  signal_call_state_change(nh, ss, status, phrase,
			   status >= 300
			   ? nua_callstate_init
			   : status >= 200
			   ? nua_callstate_completed
			   : nua_callstate_early,
			   sr->sr_auto && sr->sr_offer_recv ? "offer" : 0,
			   offer ? "offer" : answer ? "answer" : 0);

  if (status == 180)
    ss->ss_alerting = 1;
  else if (status >= 200)
    ss->ss_alerting = 0;

  if (status >= 200 && status < 300) {
    du->du_ready = 1;
  }
  else if (status >= 300) {
    sr->sr_usage = NULL;
    if (nh->nh_soa)
      soa_init_offer_answer(nh->nh_soa);
  }

  if (ss->ss_state == nua_callstate_init) {
    assert(status >= 300);
    nua_session_usage_destroy(nh, ss);
  }

  return status >= 300 ? status : 0;
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
 * @param sip    incoming INFO request
 * @param tags   empty
 *
 * @sa nua_prack(), #nua_r_prack, @RFC3262, NUTAG_EARLY_MEDIA()
 * 
 * @END_NUA_EVENT
 */

/** @internal Process PRACK or (timeout from 100rel) */
static
int process_prack(nua_handle_t *nh,
		  nta_reliable_t *rel,
		  nta_incoming_t *irq,
		  sip_t const *sip)
{
  nua_dialog_state_t *ds = nh->nh_ds;
  nua_dialog_usage_t *du;
  nua_session_usage_t *ss;
  nua_server_request_t *sri;
  int status = 200; char const *phrase = sip_200_OK;
  char const *recv = NULL, *sent = NULL;

  nta_reliable_destroy(rel);

  ss = nua_session_usage_get(ds); du = nua_dialog_usage_public(ss);

  for (sri = ds->ds_sr; sri; sri = sri->sr_next) {
    if (sri->sr_method == sip_method_invite && sri->sr_usage == du)
      break;
  }
                     
  if (!sri || !sri->sr_respond) /* XXX */
    return 481;

  if (sip)
    /* received PRACK */;
  else if (!sri || irq == NULL) { /* Final response interrupted 100rel */
    /* Ignore */
    return 200;
  }
  else if (sip == NULL) {
    SET_STATUS(504, "Reliable Response Timeout");

    nua_stack_event(nh->nh_nua, nh, NULL,
		    nua_i_error, status, phrase,
		    TAG_END());

    nua_server_respond(sri, status, phrase, TAG_END());

    return status;
  }

  if (nh->nh_soa) {
    msg_t *msg = nta_incoming_getrequest(irq);
    char const *sdp;
    size_t len;

    if (session_get_description(sip, &sdp, &len)) {
      su_home_t home[1] = { SU_HOME_INIT(home) };

      sip_content_disposition_t *cd = NULL;
      sip_content_type_t *ct = NULL;
      sip_payload_t *pl = NULL;

      if (soa_set_remote_sdp(nh->nh_soa, NULL, sdp, len) < 0) {
	SU_DEBUG_5(("nua(%p): error parsing SDP in INVITE\n", nh));
	msg_destroy(msg);
	status = 400, phrase = "Bad Session Description";
      }

      /* Respond to PRACK */

      if (status >= 300)
	;
      else if (sri->sr_offer_sent) {
	recv = "answer";
	sri->sr_answer_recv = 1;
	if (soa_process_answer(nh->nh_soa, NULL) < 0)
	  status = soa_error_as_sip_response(nh->nh_soa, &phrase);
      }
      else {
	recv = "offer";
	if (soa_generate_answer(nh->nh_soa, NULL) < 0) {
	  status = soa_error_as_sip_response(nh->nh_soa, &phrase);
	}
	else {
	  if (session_make_description(home, nh->nh_soa, 1, &cd, &ct, &pl) > 0)
	    sent = "answer";
	}
      }

      if (nta_incoming_treply(irq, status, phrase,
			      SIPTAG_CONTENT_DISPOSITION(cd),
			      SIPTAG_CONTENT_TYPE(ct),
			      SIPTAG_PAYLOAD(pl),
			      TAG_END()) < 0)
	/* Respond with 500 if nta_incoming_treply() failed */
	SET_STATUS1(SIP_500_INTERNAL_SERVER_ERROR);

      su_home_deinit(home);
    }

    msg_destroy(msg);
  }

  nua_stack_event(nh->nh_nua, nh, nta_incoming_getrequest(irq),
		  nua_i_prack, status, phrase, TAG_END());

  if (status >= 300)
    return status;

  if (recv || sent) {
    soa_activate(nh->nh_soa, NULL);
    signal_call_state_change(nh, ss, status, phrase,
			     nua_callstate_early, recv, sent);
  }

  if (NH_PGET(nh, auto_alert) && !ss->ss_alerting && !ss->ss_precondition)
    nua_server_respond(sri, SIP_180_RINGING, TAG_END());

  return status;
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

int process_ack(nua_server_request_t *sr,
		nta_incoming_t *irq,
		sip_t const *sip)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_session_usage_t *ss = nua_dialog_usage_private(sr->sr_usage);
  msg_t *msg = nta_incoming_getrequest_ackcancel(irq);
  char const *recv = NULL;

  if (ss == NULL)
    return 0;

  if (nh->nh_soa && sr->sr_offer_sent && !sr->sr_answer_recv) {
    char const *sdp;
    size_t len;

    if (!session_get_description(sip, &sdp, &len) ||
	!(recv = "answer") ||
	soa_set_remote_sdp(nh->nh_soa, NULL, sdp, len) < 0 ||
	soa_process_answer(nh->nh_soa, NULL) < 0 ||
	soa_activate(nh->nh_soa, NULL)) {
      int status; char const *phrase, *reason;

      status = soa_error_as_sip_response(nh->nh_soa, &phrase);
      reason = soa_error_as_sip_reason(nh->nh_soa);

      nua_stack_event(nh->nh_nua, nh, msg,
	       nua_i_ack, status, phrase, TAG_END());
      nua_stack_event(nh->nh_nua, nh, NULL,
	       nua_i_media_error, status, phrase, TAG_END());

      signal_call_state_change(nh, ss, 488, "Offer-Answer Error",
			       nua_callstate_terminating, recv, 0);
      nua_stack_post_signal(nh, nua_r_bye,
			    SIPTAG_REASON_STR(reason),
			    TAG_END());

      return 0;
    }
  }

  soa_clear_remote_sdp(nh->nh_soa);
  nua_stack_event(nh->nh_nua, nh, msg, nua_i_ack, SIP_200_OK, TAG_END());
  signal_call_state_change(nh, ss, 200, "OK", nua_callstate_ready, recv, 0);
  set_session_timer(ss);

  nua_server_request_destroy(sr);

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

  assert(nta_incoming_status(irq) < 200);  assert(sr->sr_respond);
  assert(ss); assert(ss == nua_session_usage_get(nh->nh_ds)); (void)ss;

  nua_stack_event(nh->nh_nua, nh, cancel, nua_i_cancel, SIP_200_OK, TAG_END());

  nua_server_respond(sr, SIP_487_REQUEST_TERMINATED, TAG_END());

  return 0;
}

/* Timeout (no ACK or PRACK received) */
static
int process_timeout(nua_server_request_t *sr,
		    nta_incoming_t *irq)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_session_usage_t *ss = nua_dialog_usage_private(sr->sr_usage);

  assert(ss); assert(ss == nua_session_usage_get(nh->nh_ds));

  nua_stack_event(nh->nh_nua, nh, 0, nua_i_error,
		  408, "Response timeout",
		  TAG_END());

  if (sr->sr_respond) {
    /* PRACK timeout */
    nua_server_respond(sr, SIP_504_GATEWAY_TIME_OUT,
		       SIPTAG_REASON_STR("SIP;cause=504;"
					 "text=\"PRACK Timeout\""),
		       TAG_END());
    ss = nua_session_usage_get(nh->nh_ds);
    sr = NULL;
  }

  if (ss) {
    /* send BYE, too if 200 OK (or 183 to re-INVITE) timeouts  */
    signal_call_state_change(nh, ss, 0, "Timeout",
			     nua_callstate_terminating, 0, 0);
    nua_stack_post_signal(nh, nua_r_bye,
			  SIPTAG_REASON_STR("SIP;cause=408;text=\"ACK Timeout\""),
			  TAG_END());
  }

  if (sr)
    nua_server_request_destroy(sr);

  return 0;
}


/* ---------------------------------------------------------------------- */
/* Session timer - RFC 4028 */

static int session_timer_is_supported(nua_handle_t const *nh)
{
  /* Is timer feature supported? */
  return sip_has_supported(NH_PGET(nh, supported), "timer");
}

static int prefer_session_timer(nua_handle_t const *nh)
{
  return 
    NH_PGET(nh, refresher) != nua_no_refresher || 
    NH_PGET(nh, session_timer) != 0;
}

/* Initialize session timer */ 
static
void session_timer_preferences(nua_session_usage_t *ss,
			       unsigned expires,
			       unsigned min_se,
			       enum nua_session_refresher refresher)
{
  if (expires < min_se)
    expires = min_se;
  if (refresher && expires == 0)
    expires = 3600;

  ss->ss_min_se = min_se;
  ss->ss_session_timer = expires;
  ss->ss_refresher = refresher;
}


/** Add timer featuretag and Session-Expires/Min-SE headers */
static int
use_session_timer(nua_session_usage_t *ss, int uas, int always,
		  msg_t *msg, sip_t *sip)
{
  sip_min_se_t min_se[1];
  sip_session_expires_t session_expires[1];

  static sip_param_t const x_params_uac[] = {"refresher=uac", NULL};
  static sip_param_t const x_params_uas[] = {"refresher=uas", NULL};

  /* Session-Expires timer */
  if (ss->ss_refresher == nua_no_refresher && !always)
    return 0;

  sip_min_se_init(min_se)->min_delta = ss->ss_min_se;
  sip_session_expires_init(session_expires)->x_delta = ss->ss_session_timer;

  if (ss->ss_refresher == nua_remote_refresher)
    session_expires->x_params = uas ? x_params_uac : x_params_uas;
  else if (ss->ss_refresher == nua_local_refresher)
    session_expires->x_params = uas ? x_params_uas : x_params_uac;

  sip_add_tl(msg, sip,
	     TAG_IF(ss->ss_session_timer,
		    SIPTAG_SESSION_EXPIRES(session_expires)),
	     TAG_IF(ss->ss_min_se != 0
		    /* Min-SE: 0 is optional with initial INVITE */
		    || ss->ss_state != nua_callstate_init,
		    SIPTAG_MIN_SE(min_se)),
	     TAG_IF(ss->ss_refresher == nua_remote_refresher,
		    SIPTAG_REQUIRE_STR("timer")),
	     TAG_END());

  return 1;
}

static int
init_session_timer(nua_session_usage_t *ss,
		   sip_t const *sip,
		   int refresher)
{
  int server;

  /* Session timer is not needed */
  if (!sip->sip_session_expires) {
    if (!sip_has_supported(sip->sip_supported, "timer"))
      ss->ss_refresher = nua_local_refresher;
    return 0;
  }

  ss->ss_refresher = nua_no_refresher;
  ss->ss_session_timer = sip->sip_session_expires->x_delta;

  if (sip->sip_min_se != NULL
      && sip->sip_min_se->min_delta > ss->ss_min_se)
    ss->ss_min_se = sip->sip_min_se->min_delta;

  server = sip->sip_request != NULL;

  if (!sip_has_supported(sip->sip_supported, "timer"))
    ss->ss_refresher = nua_local_refresher;
  else if (!str0casecmp("uac", sip->sip_session_expires->x_refresher))
    ss->ss_refresher = server ? nua_remote_refresher : nua_local_refresher;
  else if (!str0casecmp("uas", sip->sip_session_expires->x_refresher))
    ss->ss_refresher = server ? nua_local_refresher : nua_remote_refresher;
  else if (!server)
    return 0;			/* XXX */
  /* User preferences */
  else if (refresher == nua_local_refresher)
    ss->ss_refresher = nua_local_refresher;
  else
    ss->ss_refresher = nua_remote_refresher;

  SU_DEBUG_7(("nua session: session expires in %u refreshed by %s (%s %s)\n",
	      ss->ss_session_timer,
	      ss->ss_refresher == nua_local_refresher ? "local" : "remote",
	      server ? sip->sip_request->rq_method_name : "response to",
	      server ? "request" : sip->sip_cseq->cs_method_name));

  return 1;
}

static void
set_session_timer(nua_session_usage_t *ss)
{
  nua_dialog_usage_t *du = nua_dialog_usage_public(ss);

  if (ss == NULL)
    return;

  if (ss->ss_refresher == nua_local_refresher) {
    ss->ss_timer_set = 1;
    nua_dialog_usage_set_expires(du, ss->ss_session_timer);
  }
  else if (ss->ss_refresher == nua_remote_refresher) {
    ss->ss_timer_set = 1;
    nua_dialog_usage_set_expires(du, ss->ss_session_timer + 32);
    nua_dialog_usage_reset_refresh(du);
  }
  else {
    ss->ss_timer_set = 0;
    nua_dialog_usage_set_expires(du, UINT_MAX);
    nua_dialog_usage_reset_refresh(du);
  }
}

static int
check_session_timer_restart(nua_handle_t *nh,
			    nua_session_usage_t *ss,
			    nua_client_request_t *cr,
			    nta_outgoing_t *orq,
			    sip_t const *sip,
			    nua_creq_restart_f *restart_function)
{
  if (ss && sip && sip->sip_status->st_status == 422) {
    if (sip->sip_min_se && ss->ss_min_se < sip->sip_min_se->min_delta)
      ss->ss_min_se = sip->sip_min_se->min_delta;
    if (ss->ss_min_se > ss->ss_session_timer)
      ss->ss_session_timer = ss->ss_min_se;
  
    if (orq == cr->cr_orq)
      cr->cr_orq = NULL;

    return nua_creq_restart_with(nh, cr, orq,
				 100, "Re-Negotiating Session Timer",
				 restart_function, TAG_END());
  }

  return nua_creq_check_restart(nh, cr, orq, sip, restart_function);
}

static inline int
is_session_timer_set(nua_session_usage_t *ss)
{
  return ss->ss_timer_set;
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
		    ref->ref_handle));
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


/** Zap the session associated with the handle */
static
void nua_session_usage_destroy(nua_handle_t *nh,
			       nua_session_usage_t *ss)
{
  nh->nh_has_invite = 0;
  nh->nh_active_call = 0;
  nh->nh_hold_remote = 0;

  if (nh->nh_soa)
    soa_destroy(nh->nh_soa), nh->nh_soa = NULL;

  /* Remove usage */
  nua_dialog_usage_remove(nh, nh->nh_ds, nua_dialog_usage_public(ss));

  SU_DEBUG_5(("nua: terminated session %p\n", nh));
}


/* ======================================================================== */
/* INFO */

static int process_response_to_info(nua_handle_t *nh,
				       nta_outgoing_t *orq,
				       sip_t const *sip);

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
 *    Tags in <sip_tag.h>.
 *
 * @par Events:
 *    #nua_r_info
 *
 * @sa #nua_i_info
 */

int
nua_stack_info(nua_t *nua, nua_handle_t *nh, nua_event_t e, tagi_t const *tags)
{
  nua_client_request_t *cr = nh->nh_ds->ds_cr;
  msg_t *msg;

  if (nh_is_special(nh)) {
    return UA_EVENT2(e, 900, "Invalid handle for INFO");
  }
  else if (cr->cr_orq) {
    return UA_EVENT2(e, 900, "Request already in progress");
  }

  nua_stack_init_handle(nua, nh, TAG_NEXT(tags));

  msg = nua_creq_msg(nua, nh, cr, cr->cr_retry_count,
			 SIP_METHOD_INFO ,
			 NUTAG_ADD_CONTACT(1),
			 TAG_NEXT(tags));

  cr->cr_orq = nta_outgoing_mcreate(nua->nua_nta,
				    process_response_to_info, nh, NULL,
				    msg,
				    SIPTAG_END(), TAG_NEXT(tags));
  if (!cr->cr_orq) {
    msg_destroy(msg);
    return UA_EVENT1(e, NUA_INTERNAL_ERROR);
  }

  return cr->cr_event = e;
}

void restart_info(nua_handle_t *nh, tagi_t *tags)
{
  nua_creq_restart(nh, nh->nh_ds->ds_cr, process_response_to_info, tags);
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

static int process_response_to_info(nua_handle_t *nh,
				    nta_outgoing_t *orq,
				    sip_t const *sip)
{
  if (nua_creq_check_restart(nh, nh->nh_ds->ds_cr, orq, sip, restart_info))
    return 0;
  return nua_stack_process_response(nh, nh->nh_ds->ds_cr, orq, sip, TAG_END());
}

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

int nua_stack_process_info(nua_t *nua,
			   nua_handle_t *nh,
			   nta_incoming_t *irq,
			   sip_t const *sip)
{
  nua_stack_event(nh->nh_nua, nh, nta_incoming_getrequest(irq),
		  nua_i_info, SIP_200_OK, TAG_END());

  return 200;		/* Respond automatically with 200 Ok */
}


/* ======================================================================== */
/* UPDATE */

static int process_response_to_update(nua_handle_t *nh,
				       nta_outgoing_t *orq,
				       sip_t const *sip);

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

int nua_stack_update(nua_t *nua, nua_handle_t *nh, nua_event_t e,
		     tagi_t const *tags)
{
  nua_dialog_state_t *ds = nh->nh_ds;
  nua_session_usage_t *ss;
  nua_client_request_t *cr;
  msg_t *msg;
  sip_t *sip;
  char const *offer_sent = 0;

  ss = nua_session_usage_get(ds);
  cr = ds->ds_cr;

  if (!ss)
    return UA_EVENT2(e, 900, "Invalid handle for UPDATE");
  else if (cr->cr_orq)
    return UA_EVENT2(e, 900, "Request already in progress");

  nua_stack_init_handle(nua, nh, TAG_NEXT(tags));

  msg = nua_creq_msg(nua, nh, cr, cr->cr_retry_count,
		     SIP_METHOD_UPDATE,
		     NUTAG_USE_DIALOG(1),
		     NUTAG_ADD_CONTACT(1),
		     TAG_NEXT(tags));

  sip = sip_object(msg);

  if (sip) {
    nua_client_request_t *cri = ss->ss_crequest;
    nua_server_request_t *sr;

    for (sr = ds->ds_sr; sr; sr = sr->sr_next)
      if ((sr->sr_offer_sent && !sr->sr_answer_recv) ||
	  (sr->sr_offer_recv && !sr->sr_answer_sent))
	break;
    
    if (nh->nh_soa && !sip->sip_payload && 
	!sr &&
	!(cri && cri->cr_offer_sent && !cri->cr_answer_recv) &&
	!(cri && cri->cr_offer_recv && !cri->cr_answer_sent)) {
      soa_init_offer_answer(nh->nh_soa);

      if (soa_generate_offer(nh->nh_soa, 0, NULL) < 0 ||
	  session_include_description(nh->nh_soa, 1, msg, sip) < 0) {
	if (ss->ss_state < nua_callstate_ready) {
	  /* XXX */
	}
	msg_destroy(msg);
	return UA_EVENT2(e, 900, "Local media failed");
      }

      offer_sent = "offer";
    }

    /* Add session timer headers */
    if (session_timer_is_supported(nh))
      use_session_timer(ss, 0, prefer_session_timer(nh), msg, sip);

    if (nh->nh_auth) {
      if (auc_authorize(&nh->nh_auth, msg, sip) < 0)
	/* xyzzy */;
    }

    cr->cr_orq = nta_outgoing_mcreate(nua->nua_nta,
				      process_response_to_update, nh, NULL,
				      msg,
				      SIPTAG_END(), TAG_NEXT(tags));
    if (cr->cr_orq) {
      if (offer_sent)
	cr->cr_offer_sent = 1;
      ss->ss_update_needed = 0;
      signal_call_state_change(nh, ss, 0, "UPDATE sent",
			       ss->ss_state, 0, offer_sent);
      return cr->cr_event = e;
    }
  }

  msg_destroy(msg);
  return UA_EVENT1(e, NUA_INTERNAL_ERROR);
}

void restart_update(nua_handle_t *nh, tagi_t *tags)
{
  nua_creq_restart(nh, nh->nh_ds->ds_cr, process_response_to_update, tags);
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

static int process_response_to_update(nua_handle_t *nh,
				       nta_outgoing_t *orq,
				       sip_t const *sip)
{
  nua_t *nua = nh->nh_nua;
  nua_session_usage_t *ss;
  nua_client_request_t *cr = nh->nh_ds->ds_cr;

  int status = sip->sip_status->st_status;
  char const *phrase = sip->sip_status->st_phrase;
  char const *recv = NULL;
  int terminate = 0, gracefully = 1;

  ss = nua_session_usage_get(nh->nh_ds); assert(ss);

  if (status >= 300) {
    if (sip->sip_retry_after)
      gracefully = 0;

    terminate = sip_response_terminates_dialog(status, sip_method_update,
					       &gracefully);

    if (!terminate &&
	check_session_timer_restart(nh, ss, cr, orq, sip, restart_update)) {
      return 0;
    }
    /* XXX - if we have a concurrent INVITE, what we do with it? */
  }
  else if (status >= 200) {
    /* XXX - check remote tag, handle forks */
    /* Set (route), contact, (remote tag) */
    nua_dialog_uac_route(nh, nh->nh_ds, sip, 1);
    nua_dialog_store_peer_info(nh, nh->nh_ds, sip);

    if (is_session_timer_set(ss)) {
      init_session_timer(ss, sip, NH_PGET(nh, refresher));
      set_session_timer(ss);
    }

    if (session_process_response(nh, cr, orq, sip, &recv) < 0) {
      nua_stack_event(nua, nh, NULL, nua_i_error,
	       400, "Bad Session Description", TAG_END());
    }

    signal_call_state_change(nh, ss, status, phrase, ss->ss_state, recv, 0);

    return 0;
  }
  else
    gracefully = 0;

  nua_stack_process_response(nh, cr, orq, sip, TAG_END());

  if (!terminate && !gracefully)
    return 0;

  nh_referral_respond(nh, status, phrase);
  
  if (ss == NULL) {

  } 
  else if (terminate || 
      (ss->ss_state < nua_callstate_completed &&
       ss->ss_state != nua_callstate_completing)) {
    signal_call_state_change(nh, ss, status, phrase,
			     nua_callstate_terminated, recv, 0);
    nua_session_usage_destroy(nh, ss);
  }
  else /* if (gracefully) */ {
    signal_call_state_change(nh, ss, status, phrase,
			     nua_callstate_terminating, recv, 0);
#if 0
    if (nh->nh_ss->ss_crequest->cr_orq)
      nua_stack_post_signal(nh, nua_r_cancel, TAG_END());
    else
#endif
      nua_stack_post_signal(nh, nua_r_bye, TAG_END());
  }

  return 0;
}

int nua_stack_process_update(nua_t *nua,
			     nua_handle_t *nh,
			     nta_incoming_t *irq,
			     sip_t const *sip)
{
  nua_dialog_state_t *ds = nh->nh_ds;
  nua_session_usage_t *ss;
  nua_dialog_usage_t *du;
  msg_t *msg = nta_incoming_getrequest(irq);

  char const *sdp;
  size_t len;

  int original_status = 200, status = 200;
  char const *phrase = sip_200_OK;

  char const *offer_recv = NULL, *answer_sent = NULL;
  int use_timer = 0;

  msg_t *rmsg;
  sip_t *rsip;

  ss = nua_session_usage_get(ds); du = nua_dialog_usage_public(ss);
  if (!ss) {
    /* RFC 3261 section 12.2.2:
       If the UAS wishes to reject the request because it does not wish to
       recreate the dialog, it MUST respond to the request with a 481
       (Call/Transaction Does Not Exist) status code and pass that to the
       server transaction.
    */
    return 481;
  }

  if (session_check_request(nua, nh, irq, sip))
    return 501;

  /* Do session timer negotiation */
  if (sip->sip_session_expires) {
    use_timer = 1;
    init_session_timer(ss, sip, NH_PGET(nh, refresher));
  }

  if (status < 300 && nh->nh_soa &&
      session_get_description(sip, &sdp, &len)) {
    nua_client_request_t *cr;
    nua_server_request_t *sr;
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
    for (cr = ds->ds_cr; cr && !overlap; cr = cr->cr_next)
      overlap = cr->cr_offer_sent && !cr->cr_answer_recv;
    for (sr = ds->ds_sr; sr && !overlap; sr = sr->sr_next)
      overlap = (sr->sr_offer_recv && !sr->sr_answer_sent) ||
	(sr->sr_method == sip_method_update && sr->sr_respond);

    if (overlap)
      return respond_with_retry_after(nh, irq, 
				      500, "Overlapping Offer/Answer",
				      0, 10);

    offer_recv = "offer";

    if (soa_set_remote_sdp(nh->nh_soa, NULL, sdp, len) < 0) {
      SU_DEBUG_5(("nua(%p): error parsing SDP in UPDATE\n", nh));
      msg_destroy(msg);
      status = soa_error_as_sip_response(nh->nh_soa, &phrase);
      offer_recv = NULL;
    }
    /* Respond to UPDATE */
    else if (soa_generate_answer(nh->nh_soa, NULL) < 0) {
      SU_DEBUG_5(("nua(%p): error processing SDP in UPDATE\n", nh));
      msg_destroy(msg);
      status = soa_error_as_sip_response(nh->nh_soa, &phrase);
    }
    else if (soa_activate(nh->nh_soa, NULL) < 0) {
      SU_DEBUG_5(("nua(%p): error activating media after %s\n",
		  nh, "UPDATE"));
      /* XXX */
    }
    else {
      answer_sent = "answer";
    }
  }

  rmsg = nh_make_response(nua, nh, irq,
			  status, phrase,
			  TAG_IF(status < 300, NUTAG_ADD_CONTACT(1)),
			  SIPTAG_SUPPORTED(NH_PGET(nh, supported)),
			  TAG_NEXT(NULL));
  rsip = sip_object(rmsg);
  assert(sip);			/* XXX */

  if (answer_sent && 
      session_include_description(nh->nh_soa, 1, rmsg, rsip) < 0) {
    status = 500, phrase = sip_500_Internal_server_error;
    answer_sent = NULL;
  }

  if (200 <= status && status < 300 && session_timer_is_supported(nh)) {
    use_session_timer(ss, 1, use_timer, rmsg, rsip);
    set_session_timer(ss);
  }

  if (status == original_status) {
    if (nta_incoming_mreply(irq, rmsg) < 0)
      status = 500, phrase = sip_500_Internal_server_error;
  }

  if (status != original_status) {
    nua_stack_event(nua, nh, NULL, nua_i_error, status, phrase, TAG_END());
    nta_incoming_treply(irq, status, phrase, TAG_END());
    msg_destroy(rmsg), rmsg = NULL;
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

  nua_stack_event(nh->nh_nua, nh, msg, nua_i_update, status, phrase, TAG_END());

  if (offer_recv || answer_sent)
    /* signal offer received, answer sent */
    signal_call_state_change(nh, ss, 200, "OK", ss->ss_state,
			     offer_recv, answer_sent);

  if (NH_PGET(nh, auto_alert)
      && ss->ss_state < nua_callstate_ready
      && !ss->ss_alerting
      && ss->ss_precondition) {
    nua_server_request_t *sr;
    
    for (sr = ds->ds_sr; sr; sr = sr->sr_next)
      if (sr->sr_method == sip_method_invite && 
	  sr->sr_usage == du && sr->sr_respond)
	break;

    if (sr)
      nua_server_respond(sr, SIP_180_RINGING, TAG_END());
  }

  return status;
}


/* ======================================================================== */
/* BYE */

static int process_response_to_bye(nua_handle_t *nh,
				   nta_outgoing_t *orq,
				   sip_t const *sip);

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

int
nua_stack_bye(nua_t *nua, nua_handle_t *nh, nua_event_t e, tagi_t const *tags)
{
  nua_session_usage_t *ss;
  nua_client_request_t *cr = nh->nh_ds->ds_cr;
  msg_t *msg;
  nta_outgoing_t *orq;

  ss = nua_session_usage_get(nh->nh_ds);
  
  if (!ss || ss->ss_state >= nua_callstate_terminating)
    return UA_EVENT2(e, 900, "Invalid handle for BYE");

  nua_stack_init_handle(nua, nh, TAG_NEXT(tags));

  if (!nua_dialog_is_established(nh->nh_ds)) {
    nua_client_request_t *cri = ss->ss_crequest;

    if (cri->cr_orq == NULL)
      return UA_EVENT2(e, 900, "No session to BYE");

    /* No (early) dialog. BYE is invalid action, do CANCEL instead */
    orq = nta_outgoing_tcancel(cri->cr_orq,
			       process_response_to_cancel, nh,
			       TAG_NEXT(tags));
    if (!cr->cr_orq)
      cr->cr_orq = orq, cr->cr_event = e;

    return 0;
  }

  if (cr->cr_orq) {
    if (cr->cr_usage == nua_dialog_usage_public(ss)) {
      nua_creq_deinit(cr, cr->cr_orq);
    }
    else {
      cr = ss->ss_crequest;
      if (cr->cr_orq)
	nua_creq_deinit(cr, cr->cr_orq);
    }
  }

  assert(!cr->cr_orq);

  msg = nua_creq_msg(nua, nh, cr, 0, SIP_METHOD_BYE, TAG_NEXT(tags));

  cr->cr_orq = nta_outgoing_mcreate(nua->nua_nta,
				    process_response_to_bye, nh, NULL,
				    msg,
				    SIPTAG_END(), TAG_NEXT(tags));

  ss->ss_state = nua_callstate_terminating;
  if (nh->nh_soa)
    soa_terminate(nh->nh_soa, 0);

  if (cr->cr_orq) {
    cr->cr_event = e;
  }
  else {
    msg_destroy(msg);
    UA_EVENT2(e, 400, "Internal error");
    signal_call_state_change(nh, ss, 400, "Failure sending BYE",
			     nua_callstate_terminated, 0, 0);
    nua_session_usage_destroy(nh, ss);
  }

  return 0;
}


void restart_bye(nua_handle_t *nh, tagi_t *tags)
{
  nua_creq_restart(nh, nh->nh_ds->ds_cr, process_response_to_bye, tags);
}

/** @NUA_EVENT nua_r_bye
 *
 * Answer to outgoing BYE.
 *
 * The BYE may be sent explicitly by nua_bye() or
 * implicitly by NUA state machine.
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

static int process_response_to_bye(nua_handle_t *nh,
				   nta_outgoing_t *orq,
				   sip_t const *sip)
{
  nua_client_request_t *cr = NULL;
  nua_session_usage_t *ss;
  int status = sip ? sip->sip_status->st_status : 400;
  char const *phrase = sip ? sip->sip_status->st_phrase : "";

  cr = nua_client_request_by_orq(nh->nh_ds->ds_cr, orq); assert(cr);

  if (cr) {
    if (nua_creq_check_restart(nh, cr, orq, sip, restart_bye))
      return 0;
    nua_stack_process_response(nh, cr, orq, sip, TAG_END());
  }
  else {			/* No cr for BYE */
    msg_t *msg = nta_outgoing_getresponse(orq);
    nua_stack_event(nh->nh_nua, nh, msg, nua_r_bye, status, phrase, TAG_END());
    nta_outgoing_destroy(orq);
  }

  ss = nua_session_usage_get(nh->nh_ds);

  if (status >= 200 && ss) {
    if (ss->ss_crequest->cr_orq) {
      /* Do not destroy usage while INVITE is alive */
    }
    else {
      signal_call_state_change(nh, ss, status, "to BYE",
			       nua_callstate_terminated, 0, 0);
      nua_session_usage_destroy(nh, ss);
    }
  }

  return 0;
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

int nua_stack_process_bye(nua_t *nua,
			  nua_handle_t *nh,
			  nta_incoming_t *irq,
			  sip_t const *sip)
{
  nua_dialog_state_t *ds = nh->nh_ds;
  nua_session_usage_t *ss;
  nua_server_request_t *sr, *sr_next;
  int early = 0;

  ss = nua_session_usage_get(ds);
  if (!ss)
    return 481;

  assert(nh && ss);

  nua_stack_event(nh->nh_nua, nh, nta_incoming_getrequest(irq),
		  nua_i_bye, SIP_200_OK, TAG_END());
  nta_incoming_treply(irq, SIP_200_OK, TAG_END());
  nta_incoming_destroy(irq), irq = NULL;

  for (sr = ds->ds_sr; sr; sr = sr_next) {
    sr_next = sr->sr_next;
    if (sr->sr_respond && sr->sr_usage == nua_dialog_usage_public(ss)) {
      char const *phrase;
      early = ss->ss_state < nua_callstate_ready;
      phrase = early ? "Early Session Terminated" : "Session Terminated";
      sr->sr_usage = NULL;
      if (sr->sr_respond)
	nua_server_respond(sr, 487, phrase, TAG_END());
      else
	nua_server_request_destroy(sr);
    }
  }

  signal_call_state_change(nh, ss, 200,
			   early ? "Received early BYE" : "Received BYE",
			   nua_callstate_terminated, 0, 0);

  nua_session_usage_destroy(nh, ss);

  return 0;
}

/* ---------------------------------------------------------------------- */

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
				     enum nua_callstate next_state,
				     char const *oa_recv,
				     char const *oa_sent)
{
  enum nua_callstate ss_state;

  sdp_session_t const *remote_sdp = NULL;
  char const *remote_sdp_str = NULL;
  sdp_session_t const *local_sdp = NULL;
  char const *local_sdp_str = NULL;

  int offer_recv = 0, answer_recv = 0, offer_sent = 0, answer_sent = 0;

  ss_state = ss ? ss->ss_state : nua_callstate_init;

  if (ss_state < nua_callstate_ready || next_state > nua_callstate_ready)
    SU_DEBUG_5(("nua(%p): call state changed: %s -> %s%s%s%s%s\n",
		nh, nua_callstate_name(ss_state),
		nua_callstate_name(next_state),
		oa_recv ? ", received " : "", oa_recv ? oa_recv : "",
		oa_sent && oa_recv ? ", and sent " :
		oa_sent ? ", sent " : "", oa_sent ? oa_sent : ""));
  else
    SU_DEBUG_5(("nua(%p): ready call updated: %s%s%s%s%s\n",
		nh, nua_callstate_name(next_state),
		oa_recv ? " received " : "", oa_recv ? oa_recv : "",
		oa_sent && oa_recv ? ", sent " :
		oa_sent ? " sent " : "", oa_sent ? oa_sent : ""));

  if (oa_recv) {
    soa_get_remote_sdp(nh->nh_soa, &remote_sdp, &remote_sdp_str, 0);
    offer_recv = strcasecmp(oa_recv, "offer") == 0;
    answer_recv = strcasecmp(oa_recv, "answer") == 0;
  }

  if (oa_sent) {
    soa_get_local_sdp(nh->nh_soa, &local_sdp, &local_sdp_str, 0);
    offer_sent = strcasecmp(oa_sent, "offer") == 0;
    answer_sent = strcasecmp(oa_sent, "answer") == 0;
  }

  if (answer_recv || answer_sent) {
    /* Update nh_hold_remote */

    char const *held;

    soa_get_params(nh->nh_soa, SOATAG_HOLD_REF(held), TAG_END());

    nh->nh_hold_remote = held && strlen(held) > 0;
  }

  if (ss) {
    /* Update state variables */
    if (next_state > ss_state)
      ss->ss_state = next_state;
    else if (next_state == nua_callstate_init && ss_state < nua_callstate_ready)
      ss->ss_state = nua_callstate_init, next_state = nua_callstate_terminated;
  }

  if (ss && ss->ss_state == nua_callstate_ready)
    nh->nh_active_call = 1;
  else if (next_state == nua_callstate_terminated)
    nh->nh_active_call = 0;

  /* Send events */
  if (phrase == NULL)
    phrase = "Call state";

/** @NUA_EVENT nua_i_state
 *
 * @brief Call state has changed.
 *
 * This event will be sent whenever the call state changes. 
 *
 * In addition to basic changes of session status indicated with enum
 * ::nua_callstate, the @RFC3264 SDP Offer/Answer negotiation status is also
 * included if it is enabled (by default or with NUTAG_MEDIA_ENABLE(1)). The
 * received remote SDP is included in tag SOATAG_REMOTE_SDP(). The tags
 * NUTAG_OFFER_RECV() or NUTAG_ANSWER_RECV() indicate whether the remote SDP
 * was an offer or an answer. The SDP negotiation result is included in the
 * tags SOATAG_LOCAL_SDP() and SOATAG_LOCAL_SDP_STR() and tags
 * NUTAG_OFFER_SENT() or NUTAG_ANSWER_SENT() indicate whether the local SDP
 * was an offer or answer.
 *
 * SOATAG_ACTIVE_AUDIO() and SOATAG_ACTIVE_VIDEO() are informational tags
 * used to indicate what is the status of audio or video.
 *
 * Note that #nua_i_state also covers call establisment events
 * (#nua_i_active) and termination (#nua_i_terminated).
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
 * NUTAG_AUTOALERT(), NUTAG_AUTOANSWER(), NUTAG_EARLY_MEDIA(),
 * NUTAG_EARLY_ANSWER(), NUTAG_INCLUDE_EXTRA_SDP(),
 * nua_ack(), NUTAG_AUTOACK(), nua_bye(), #nua_r_bye, #nua_i_bye,
 * nua_cancel(), #nua_r_cancel, #nua_i_cancel,
 * nua_prack(), #nua_r_prack, #nua_i_prack,
 * nua_update(), #nua_r_update, #nua_i_update
 *
 * @END_NUA_EVENT
 */

  nua_stack_event(nh->nh_nua, nh, NULL, nua_i_state,
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

  if (next_state == nua_callstate_ready && ss_state <= nua_callstate_ready) {
    nua_stack_event(nh->nh_nua, nh, NULL, nua_i_active, status, "Call active",
	     NH_ACTIVE_MEDIA_TAGS(1, nh->nh_soa),
	     /* NUTAG_SOA_SESSION(nh->nh_soa), */
	     TAG_END());
  }

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

  else if (next_state == nua_callstate_terminated) {
    nua_stack_event(nh->nh_nua, nh, NULL, nua_i_terminated, status, phrase,
	     TAG_END());
  }
}

/* ======================================================================== */

static
int respond_with_retry_after(nua_handle_t *nh, nta_incoming_t *irq,
			     int status, char const *phrase,
			     int min, int max)
{
  sip_retry_after_t af[1];

  sip_retry_after_init(af);
  af->af_delta = (unsigned)su_randint(min, max);
  af->af_comment = phrase;

  nta_incoming_treply(irq, status, phrase,
		      SIPTAG_RETRY_AFTER(af),
		      SIPTAG_USER_AGENT_STR(NH_PGET(nh, user_agent)),
		      TAG_END());

  return 500;
}

/* ======================================================================== */

/** Get SDP from a SIP message */
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
  else if (strcasecmp(ct->c_type, SDP_MIME_TYPE)) {
    SU_DEBUG_5(("nua: unknown %s: %s\n", "Content-Type", ct->c_type));
    return 0;
  }
  else
    matching_content_type = 1;

  if (pl == NULL)
    return 0;

  if (!matching_content_type) {
    /* Make sure we got SDP */
    if (pl->pl_len < 3 || strncasecmp(pl->pl_data, "v=0", 3))
      return 0;
  }

  *return_sdp = pl->pl_data;
  *return_len = pl->pl_len;

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

    if (!*return_pl || !*return_cd)
      return -1;

    if (session && !*return_cd)
      return -1;
  }

  return retval;
}

/**
 * Stores and processes SDP from incoming response, then calls
 * nua_stack_process_response().
 *
 * @retval 1 if there was SDP to process.
 */
static
int session_process_response(nua_handle_t *nh,
			     nua_client_request_t *cr,
			     nta_outgoing_t *orq,
			     sip_t const *sip,
			     char const **return_received)
{
  char const *method = nta_outgoing_method_name(orq);
  msg_t *msg = nta_outgoing_getresponse(orq);
  int retval = 0;
  char const *sdp = NULL;
  size_t len;

  if (nh->nh_soa == NULL)
    /* Xyzzy */;
  else if (!session_get_description(sip, &sdp, &len))
    /* No SDP */;
  else if (cr->cr_answer_recv) {
    /* Ignore spurious answers after completing O/A */
    SU_DEBUG_3(("nua(%p): %s: ignoring duplicate SDP in %u %s\n",
		nh, method,
		sip->sip_status->st_status, sip->sip_status->st_phrase));
    sdp = NULL;
  }
  else if (!cr->cr_offer_sent &&
	   nta_outgoing_method(orq) != sip_method_invite) {
    /* If non-invite request did not have offer, ignore SDP in response */
    SU_DEBUG_3(("nua(%p): %s: ignoring extra SDP in %u %s\n",
		nh, method,
		sip->sip_status->st_status, sip->sip_status->st_phrase));
    sdp = NULL;
  }
  else {
    if (cr->cr_offer_sent) {
      cr->cr_answer_recv = sip->sip_status->st_status;
      *return_received = "answer";
    }
    else {
      cr->cr_offer_recv = 1, cr->cr_answer_sent = 0;
      *return_received = "offer";
    }

    if (soa_set_remote_sdp(nh->nh_soa, NULL, sdp, len) < 0) {
      SU_DEBUG_5(("nua(%p): %s: error parsing SDP in %u %s\n",
		  nh, method,
		  sip->sip_status->st_status,
		  sip->sip_status->st_phrase));
      retval = -1;
      sdp = NULL;
    }
    else if (cr->cr_offer_recv) {
      /* note: case 1: incoming offer */
      SU_DEBUG_5(("nua(%p): %s: get SDP %s in %u %s\n",
		  nh, method, "offer",
		  sip->sip_status->st_status,
		  sip->sip_status->st_phrase));
      retval = 1;
    }
    else if (soa_process_answer(nh->nh_soa, NULL) < 0) {
      SU_DEBUG_5(("nua(%p): %s: error processing SDP answer in %u %s\n",
		  nh, method,
		  sip->sip_status->st_status,
		  sip->sip_status->st_phrase));
      sdp = NULL;
    }
    else {
      /* note: case 2: answer to our offer */
      if (soa_activate(nh->nh_soa, NULL) < 0) {
	SU_DEBUG_3(("nua(%p): %s: error activating media after %u %s\n",
		    nh, method,
		    sip->sip_status->st_status,
		    sip->sip_status->st_phrase));
	/* XXX */
      }
      else {
	SU_DEBUG_5(("nua(%p): %s: processed SDP answer in %u %s\n",
		    nh, method,
		    sip->sip_status->st_status,
		    sip->sip_status->st_phrase));
      }

      assert(!cr->cr_offer_recv);
    }
  }

  msg_destroy(msg);		/* unref */

  nua_stack_process_response(nh, cr, orq, sip,
			     NH_REMOTE_MEDIA_TAGS(sdp != NULL, nh->nh_soa),
			     TAG_END());

  return retval;
}

#if 0
/** Parse and store SDP from incoming request */
static
int session_process_request(nua_handle_t *nh,
			    nta_incoming_t *irq,
			    sip_t const *sip)
{
  char const *sdp = NULL;
  isize_t len;

  if (nh->nh_soa) {
    msg_t *msg = nta_outgoing_getresponse(irq);

    if (session_get_description(msg, sip, &sdp, &len)) {
      if (soa_is_complete(nh->nh_soa)) {
	/* Ignore spurious answers after completing O/A */
	SU_DEBUG_5(("nua: ignoring duplicate SDP in %u %s\n",
		    sip->sip_status->st_status, sip->sip_status->st_phrase));
	sdp = NULL;
      }
      else if (soa_parse_sdp(nh->nh_soa, sdp, len) < 0) {
	SU_DEBUG_5(("nua: error parsing SDP in %u %s\n",
		    sip->sip_status->st_status,
		    sip->sip_status->st_phrase));
	sdp = NULL;
      }
    }

    msg_destroy(msg);
  }

  return
    nua_stack_process_response(nh, cr, orq, sip,
			       NH_REMOTE_MEDIA_TAGS(sdp != NULL, nh->nh_soa),
			       TAG_END());
}
#endif

static int respond_to_options(nua_server_request_t *sr, tagi_t const *tags);

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

int nua_stack_process_options(nua_t *nua,
			      nua_handle_t *nh,
			      nta_incoming_t *irq,
			      sip_t const *sip)
{
  nua_server_request_t *sr, sr0[1];
  int done;

  /* Hook to outbound */
  done = nua_registration_process_request(nua->nua_registrations, irq, sip);
  if (done)
    return done;

  sr = nua_server_request(nua, nh, irq, sip, SR_INIT(sr0), sizeof *sr,
			  respond_to_options, 0);

  SR_STATUS1(sr, SIP_200_OK);

  return nua_stack_server_event(nua, sr, nua_i_options, TAG_END());
}

/** @internal Respond to an OPTIONS request.
 *
 */
static int respond_to_options(nua_server_request_t *sr, tagi_t const *tags)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_t *nua = nh->nh_nua;
  msg_t *msg;
  int final;

  msg = nua_server_response(sr,
			    sr->sr_status, sr->sr_phrase,
			    SIPTAG_ALLOW(NH_PGET(nh, allow)),
			    SIPTAG_SUPPORTED(NH_PGET(nh, supported)),
			    TAG_IF(NH_PGET(nh, path_enable),
				   SIPTAG_SUPPORTED_STR("path")),
			    SIPTAG_ACCEPT_STR(SDP_MIME_TYPE),
			    TAG_NEXT(tags));

  final = sr->sr_status >= 200;

  if (msg) {
    sip_t *sip = sip_object(msg);

    if (!sip->sip_payload) {	/* XXX - do MIME multipart? */
      soa_session_t *soa = nh->nh_soa;

      if (soa == NULL)
	soa = nua->nua_dhandle->nh_soa;

      session_include_description(soa, 0, msg, sip);
    }

    if (nta_incoming_mreply(sr->sr_irq, msg) < 0)
      final = 1;
  }

  return final;
}
