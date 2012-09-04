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

/**@CFILE nua_server.c
 * @brief Server transaction handling
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Feb  3 16:10:45 EET 2009
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <assert.h>

#include <sofia-sip/su_string.h>
#include <sofia-sip/su_tagarg.h>

#include <sofia-sip/sip_protos.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sip_util.h>

#define NUA_SAVED_EVENT_T su_msg_t *
#define NUA_SAVED_SIGNAL_T su_msg_t *

#include <sofia-sip/su_wait.h>

#include "nua_stack.h"
#include "nua_dialog.h"
#include "nua_server.h"
#include "nua_params.h"

/* ======================================================================== */
/*
 * Process incoming requests
 */

nua_server_methods_t const *nua_server_methods[] = {
  /* These must be in same order as in sip_method_t */
  &nua_extension_server_methods,
  &nua_invite_server_methods,	/**< INVITE */
  NULL,				/**< ACK */
  NULL,				/**< CANCEL */
  &nua_bye_server_methods,	/**< BYE */
  &nua_options_server_methods,	/**< OPTIONS */
  &nua_register_server_methods,	/**< REGISTER */
  &nua_info_server_methods,	/**< INFO */
  &nua_prack_server_methods,	/**< PRACK */
  &nua_update_server_methods,	/**< UPDATE */
  &nua_message_server_methods,	/**< MESSAGE */
  &nua_subscribe_server_methods,/**< SUBSCRIBE */
  &nua_notify_server_methods,	/**< NOTIFY */
  &nua_refer_server_methods,	/**< REFER */
  &nua_publish_server_methods,	/**< PUBLISH */
  NULL
};


int nua_stack_process_request(nua_handle_t *nh,
			      nta_leg_t *leg,
			      nta_incoming_t *irq,
			      sip_t const *sip)
{
  nua_t *nua = nh->nh_nua;
  sip_method_t method = sip->sip_request->rq_method;
  char const *name = sip->sip_request->rq_method_name;
  nua_server_methods_t const *sm;
  nua_server_request_t *sr, sr0[1];
  int status, initial = 1;
  int create_dialog;

  char const *user_agent = NH_PGET(nh, user_agent);
  sip_supported_t const *supported = NH_PGET(nh, supported);
  sip_allow_t const *allow = NH_PGET(nh, allow);

  enter;

  nta_incoming_tag(irq, NULL);

  if (method == sip_method_cancel)
    return 481;

  /* Hook to outbound */
  if (method == sip_method_options) {
    status = nua_registration_process_request(nua->nua_registrations,
					      irq, sip);
    if (status)
      return status;
  }

  if (nta_check_method(irq, sip, allow,
		       SIPTAG_SUPPORTED(supported),
		       SIPTAG_USER_AGENT_STR(user_agent),
		       TAG_END()))
    return 405;

  switch (sip->sip_request->rq_url->url_type) {
  case url_sip:
  case url_sips:
  case url_im:
  case url_pres:
  case url_tel:
    break;
  default:
    nta_incoming_treply(irq, status = SIP_416_UNSUPPORTED_URI,
			SIPTAG_ALLOW(allow),
			SIPTAG_SUPPORTED(supported),
			SIPTAG_USER_AGENT_STR(user_agent),
			TAG_END());
    return status;
  }

  if (nta_check_required(irq, sip, supported,
			 SIPTAG_ALLOW(allow),
			 SIPTAG_USER_AGENT_STR(user_agent),
			 TAG_END()))
    return 420;

  if (method > sip_method_unknown && method <= sip_method_publish)
    sm = nua_server_methods[method];
  else
    sm = nua_server_methods[0];

  initial = nh == nua->nua_dhandle;

  if (sm == NULL) {
    SU_DEBUG_1(("nua(%p): strange %s from <" URL_PRINT_FORMAT ">\n",
		(void *)nh, sip->sip_request->rq_method_name,
		URL_PRINT_ARGS(sip->sip_from->a_url)));
  }
  else if (initial && sm->sm_flags.in_dialog) {
    /* These must be in-dialog */
    sm = NULL;
  }
  else if (initial && sip->sip_to->a_tag && method != sip_method_subscribe) {
    /* RFC 3261 section 12.2.2:

       If the UAS wishes to reject the request because it does not wish to
       recreate the dialog, it MUST respond to the request with a 481
       (Call/Transaction Does Not Exist) status code and pass that to the
       server transaction.
    */ /* we allow this on subscribes because we have disabled the built-in notify server and we need those messages in the application layer */
	  
    if (method == sip_method_info)
      /* accept out-of-dialog info */; else
    if (method != sip_method_message || !NH_PGET(nh, win_messenger_enable))
      sm = NULL;
  }

  if (!sm) {
    nta_incoming_treply(irq,
			status = 481, "Call Does Not Exist",
			SIPTAG_ALLOW(allow),
			SIPTAG_SUPPORTED(supported),
			SIPTAG_USER_AGENT_STR(user_agent),
			TAG_END());
    return 481;
  }

  create_dialog = sm->sm_flags.create_dialog;
  if (method == sip_method_message && NH_PGET(nh, win_messenger_enable))
    create_dialog = 1;
  sr = memset(sr0, 0, (sizeof sr0));

  sr->sr_methods = sm;
  sr->sr_method = method = sip->sip_request->rq_method;
  sr->sr_add_contact = sm->sm_flags.add_contact;
  sr->sr_target_refresh = sm->sm_flags.target_refresh;

  sr->sr_owner = nh;
  sr->sr_initial = initial;

  sr->sr_irq = irq;

  SR_STATUS1(sr, SIP_100_TRYING);

  sr->sr_request.msg = nta_incoming_getrequest(irq);
  sr->sr_request.sip = sip;
  assert(sr->sr_request.msg);

  sr->sr_response.msg = nta_incoming_create_response(irq, 0, NULL);
  sr->sr_response.sip = sip_object(sr->sr_response.msg);

  if (sr->sr_response.msg == NULL) {
    SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);
  }
  else if (sm->sm_init && sm->sm_init(sr)) {
    if (sr->sr_status < 200)    /* Init may have set response status */
      SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);
  }
  /* Create handle if request does not fail */
  else if (initial && sr->sr_status < 300) {
    if ((nh = nua_stack_incoming_handle(nua, irq, sip, create_dialog)))
      sr->sr_owner = nh;
    else
      SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);
  }

  if (sr->sr_status < 300 && sm->sm_preprocess && sm->sm_preprocess(sr)) {
    if (sr->sr_status < 200)    /* Set response status if preprocess did not */
      SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);
  }

  if (sr->sr_status < 300) {
    if (sr->sr_target_refresh)
      nua_dialog_uas_route(nh, nh->nh_ds, sip, 1); /* Set route and tags */
    nua_dialog_store_peer_info(nh, nh->nh_ds, sip);
  }

  if (sr->sr_status == 100 && method != sip_method_unknown &&
      !sip_is_allowed(NH_PGET(sr->sr_owner, appl_method), method, name)) {
    if (method == sip_method_refer || method == sip_method_subscribe)
      SR_STATUS1(sr, SIP_202_ACCEPTED);
    else
      SR_STATUS1(sr, SIP_200_OK);
  }

  /* INVITE server request is not finalized after 2XX response */
  if (sr->sr_status < (method == sip_method_invite ? 300 : 200)) {
    sr = su_alloc(nh->nh_home, (sizeof *sr));

    if (sr) {
      *sr = *sr0;

      if ((sr->sr_next = nh->nh_ds->ds_sr))
	*(sr->sr_prev = sr->sr_next->sr_prev) = sr,
	  sr->sr_next->sr_prev = &sr->sr_next;
      else
	*(sr->sr_prev = &nh->nh_ds->ds_sr) = sr;
    }
    else {
      sr = sr0;
      SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);
    }
  }

  if (sr->sr_status <= 100) {
	  	  SR_STATUS1(sr, SIP_100_TRYING);
    if (method == sip_method_invite || sip->sip_timestamp) {
		nta_incoming_treply(irq, SIP_100_TRYING,
							SIPTAG_USER_AGENT_STR(user_agent),
							TAG_END());

    }
  }
  else {
    /* Note that this may change the sr->sr_status */
    nua_server_respond(sr, NULL);
  }

  if (nua_server_report(sr) == 0)
    return 0;

  return 501;
}

#undef nua_base_server_init
#undef nua_base_server_preprocess

int nua_base_server_init(nua_server_request_t *sr)
{
  return 0;
}

int nua_base_server_preprocess(nua_server_request_t *sr)
{
  return 0;
}

void nua_server_request_destroy(nua_server_request_t *sr)
{
  nua_server_request_t *sr0 = NULL;

  if (sr == NULL)
    return;

  if (SR_HAS_SAVED_SIGNAL(sr))
    nua_destroy_signal(sr->sr_signal);

  if (sr->sr_prev) {
    /* Allocated from heap */
    if ((*sr->sr_prev = sr->sr_next))
      sr->sr_next->sr_prev = sr->sr_prev;
	sr0 = sr;
  }

  if (sr->sr_irq) {
	nta_incoming_t *irq = sr->sr_irq;
    if (sr->sr_method == sip_method_bye && sr->sr_status < 200) {
      nta_incoming_treply(sr->sr_irq, SIP_200_OK, TAG_END());
    }
	sr->sr_irq = NULL;
    nta_incoming_destroy(irq);
  }

  if (sr->sr_request.msg) {
	msg_t *msg = sr->sr_request.msg;
	sr->sr_request.msg = NULL;
    msg_destroy(msg);
  }

  if (sr->sr_response.msg) {
	msg_t *msg = sr->sr_response.msg;
	sr->sr_response.msg = NULL;
    msg_destroy(msg); 
  }

  if (sr0) su_free(sr->sr_owner->nh_home, sr0);
}

/**@fn void nua_respond(nua_handle_t *nh, int status, char const *phrase, tag_type_t tag, tag_value_t value, ...);
 *
 * Respond to a request with given status code and phrase.
 *
 * The stack returns a SIP response message with given status code and
 * phrase to the client. The tagged parameter list can specify extra headers
 * to include with the response message and other stack parameters. The SIP
 * session or other protocol state associated with the handle is updated
 * accordingly (for instance, if an initial INVITE is responded with 200, a
 * SIP session is established.)
 *
 * When responding to an incoming INVITE request, the nua_respond() can be
 * called without NUTAG_WITH() (or NUTAG_WITH_CURRENT() or
 * NUTAG_WITH_SAVED()). Otherwise, NUTAG_WITH() will contain an indication
 * of the request being responded.
 *
 * @param nh              Pointer to operation handle
 * @param status          SIP response status code (see RFCs of SIP)
 * @param phrase          free text (default response phrase is used if NULL)
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *    nothing
 *
 * @par Responses by Protocol Engine
 *
 * When nua protocol engine receives an incoming SIP request, it can either
 * respond to the request automatically or let application to respond to the
 * request. The automatic response is returned to the client if the request
 * fails syntax check, or the method, SIP extension or content negotiation
 * fails.
 *
 * When the @ref nua_handlingevents "request event" is delivered to the
 * application, the application should examine the @a status parameter. The
 * @a status parameter is 200 or greater if the request has been already
 * responded automatically by the stack.
 *
 * The application can add methods that it likes to handle by itself with
 * NUTAG_APPL_METHOD(). The default set of NUTAG_APPL_METHOD() includes
 * INVITE, PUBLISH, REGISTER and SUBSCRIBE. Note that unless the method is
 * also included in the set of allowed methods with NUTAG_ALLOW(), the stack
 * will respond to the incoming methods with <i>405 Not Allowed</i>.
 *
 * In order to simplify the simple applications, most requests are responded
 * automatically. The BYE and CANCEL requests are always responded by the
 * stack. Likewise, the NOTIFY requests associated with an event
 * subscription are responded by the stack.
 *
 * Note that certain methods are rejected outside a SIP session (created
 * with INVITE transaction). They include BYE, UPDATE, PRACK and INFO. Also
 * the auxiliary methods ACK and CANCEL are rejected by the stack if there
 * is no ongoing INVITE transaction corresponding to them.
 *
 * @par Related Tags:
 *    NUTAG_WITH(), NUTAG_WITH_THIS(), NUTAG_WITH_SAVED() \n
 *    NUTAG_EARLY_ANSWER() \n
 *    SOATAG_ADDRESS() \n
 *    SOATAG_AF() \n
 *    SOATAG_HOLD() \n
 *    Tags used with nua_set_hparams() \n
 *    Header tags defined in <sofia-sip/sip_tag.h>.
 *
 * @par Events:
 *    #nua_i_state \n
 *    #nua_i_media_error \n
 *    #nua_i_error \n
 *    #nua_i_active \n
 *    #nua_i_terminated \n
 *
 * @sa #nua_i_invite, #nua_i_register, #nua_i_subscribe, #nua_i_publish
 */

void
nua_stack_respond(nua_t *nua, nua_handle_t *nh,
		  int status, char const *phrase, tagi_t const *tags)
{
  nua_server_request_t *sr;
  tagi_t const *t;
  msg_t const *request = NULL;

  t = tl_find_last(tags, nutag_with);

  if (t)
    request = (msg_t const *)t->t_value;

  for (sr = nh->nh_ds->ds_sr; sr; sr = sr->sr_next) {
    if (request && sr->sr_request.msg == request)
      break;
    /* nua_respond() to INVITE can be used without NUTAG_WITH() */
    if (!t && sr->sr_method == sip_method_invite)
      break;
  }

  if (sr == NULL) {
    nua_stack_event(nua, nh, NULL, nua_i_error,
		    500, "Responding to a Non-Existing Request", NULL);
    return;
  }
  else if (!nua_server_request_is_pending(sr)) {
    nua_stack_event(nua, nh, NULL, nua_i_error,
		    500, "Already Sent Final Response", NULL);
    return;
  }
  else if (sr->sr_100rel && !sr->sr_pracked && 200 <= status && status < 300) {
    /* Save signal until we have received PRACK */
    if (tags && nua_stack_set_params(nua, nh, nua_i_none, tags) < 0) {
      sr->sr_application = status;
      SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);
    }
    else {
      su_msg_save(sr->sr_signal, nh->nh_nua->nua_signal);
      return;
    }
  }
  else {
    sr->sr_application = status;
    if (tags && nua_stack_set_params(nua, nh, nua_i_none, tags) < 0)
      SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);
    else {
      sr->sr_status = status, sr->sr_phrase = phrase;
    }
  }

  nua_server_params(sr, tags);
  nua_server_respond(sr, tags);
  nua_server_report(sr);
}

int nua_server_params(nua_server_request_t *sr, tagi_t const *tags)
{
  if (sr->sr_methods->sm_params)
    return sr->sr_methods->sm_params(sr, tags);
  return 0;
}

#undef nua_base_server_params

int nua_base_server_params(nua_server_request_t *sr, tagi_t const *tags)
{
  return 0;
}

/** Return the response to the client.
 *
 * @retval 0 when successfully sent
 * @retval -1 upon an error
 */
int nua_server_trespond(nua_server_request_t *sr,
			tag_type_t tag, tag_value_t value, ...)
{
  int retval;
  ta_list ta;
  ta_start(ta, tag, value);
  retval = nua_server_respond(sr, ta_args(ta));
  ta_end(ta);
  return retval;
}

/** Return the response to the client.
 *
 * @retval 0 when successfully sent
 * @retval -1 upon an error
 */
int nua_server_respond(nua_server_request_t *sr, tagi_t const *tags)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_dialog_state_t *ds = nh->nh_ds;
  sip_method_t method = sr->sr_method;
  struct { msg_t *msg; sip_t *sip; } next = { NULL, NULL };
  int retval, user_contact = 1;
#if HAVE_OPEN_C
  /* Nice. And old arm symbian compiler; see below. */
  tagi_t next_tags[2];
#else
  tagi_t next_tags[2] = {{ SIPTAG_END() }, { TAG_NEXT(tags) }};
#endif

  msg_t *msg = sr->sr_response.msg;
  sip_t *sip = sr->sr_response.sip;
  sip_contact_t *m = sr->sr_request.sip->sip_contact;

#if HAVE_OPEN_C
  next_tags[0].t_tag   = siptag_end;
  next_tags[0].t_value = (tag_value_t)0;
  next_tags[1].t_tag   = tag_next;
  next_tags[1].t_value = (tag_value_t)(tags);
#endif

  if (sr->sr_response.msg == NULL) {
	  //assert(sr->sr_status == 500);
	  SU_DEBUG_0(("sr without msg, sr_status=%u", sr->sr_status));
    goto internal_error;
  }

  if (sr->sr_status < 200) {
    next.msg = nta_incoming_create_response(sr->sr_irq, 0, NULL);
    next.sip = sip_object(next.msg);
    if (next.sip == NULL)
      SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);
  }

  if (nta_incoming_complete_response(sr->sr_irq, msg,
				     sr->sr_status,
				     sr->sr_phrase,
				     TAG_NEXT(tags)) < 0)
    ;
  else if (!sip->sip_supported && NH_PGET(nh, supported) &&
	   sip_add_dup(msg, sip, (sip_header_t *)NH_PGET(nh, supported)) < 0)
    ;
  else if (!sip->sip_user_agent && NH_PGET(nh, user_agent) &&
	   sip_add_make(msg, sip, sip_user_agent_class,
			NH_PGET(nh, user_agent)) < 0)
    ;
  else if (!sip->sip_organization && NH_PGET(nh, organization) &&
	   sip_add_make(msg, sip, sip_organization_class,
			NH_PGET(nh, organization)) < 0)
    ;
  else if (!sip->sip_via && NH_PGET(nh, via) &&
	   sip_add_make(msg, sip, sip_via_class,
			NH_PGET(nh, via)) < 0)
    ;
  else if (!sip->sip_allow && NH_PGET(nh, allow) &&
	   sip_add_dup(msg, sip, (void *)NH_PGET(nh, allow)) < 0)
    ;
  else if (!sip->sip_allow_events &&
	   NH_PGET(nh, allow_events) &&
	   (method == sip_method_publish || method == sip_method_subscribe ||
	    method == sip_method_options || method == sip_method_refer ||
	    (sr->sr_initial &&
	     (method == sip_method_invite ||
	      method == sip_method_notify))) &&
	   sip_add_dup(msg, sip, (void *)NH_PGET(nh, allow_events)) < 0)
    ;
  else if (!sip->sip_contact && sr->sr_status < 300 && sr->sr_add_contact &&
	   (user_contact = 0,
	    ds->ds_ltarget
	    ? sip_add_dup(msg, sip, (sip_header_t *)ds->ds_ltarget)
	    : nua_registration_add_contact_to_response(nh, msg, sip, NULL, m))
	   < 0)
    ;
  else {
    int term;
    sip_contact_t *ltarget = NULL;

    term = sip_response_terminates_dialog(sr->sr_status, sr->sr_method, NULL);

    sr->sr_terminating = (term < 0) ? -1 : (term > 0 || sr->sr_terminating);

    if (sr->sr_target_refresh && sr->sr_status < 300 && !sr->sr_terminating &&
	user_contact && sip->sip_contact) {
      /* Save Contact given by application */
      ltarget = sip_contact_dup(nh->nh_home, sip->sip_contact);
    }

    retval = sr->sr_methods->sm_respond(sr, next_tags);

    if (sr->sr_status < 200)
      sr->sr_response.msg = next.msg, sr->sr_response.sip = next.sip;
    else if (next.msg)
      msg_destroy(next.msg);

    assert(sr->sr_status >= 200 || sr->sr_response.msg);

    if (ltarget) {
      if (sr->sr_status < 300) {
	nua_dialog_state_t *ds = nh->nh_ds;
	msg_header_free(nh->nh_home, (msg_header_t *)ds->ds_ltarget);
	ds->ds_ltarget = ltarget;
      }
      else
	msg_header_free(nh->nh_home, (msg_header_t *)ltarget);
    }

    return retval;
  }

  if (next.msg)
    msg_destroy(next.msg);

  SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);

  msg_destroy(msg);

 internal_error:
  sr->sr_response.msg = NULL, sr->sr_response.sip = NULL;
  nta_incoming_treply(sr->sr_irq, sr->sr_status, sr->sr_phrase, TAG_END());

  return 0;
}

/** Return the response to the client.
 *
 * @retval 0 when successfully sent
 * @retval -1 upon an error
 */
int nua_base_server_respond(nua_server_request_t *sr, tagi_t const *tags)
{
  msg_t *response = sr->sr_response.msg;
  sip_t *sip = sr->sr_response.sip;

  sr->sr_response.msg = NULL, sr->sr_response.sip = NULL;

  if (sr->sr_status != sip->sip_status->st_status) {
    msg_header_remove(response, (msg_pub_t *)sip,
		      (msg_header_t *)sip->sip_status);
    nta_incoming_complete_response(sr->sr_irq, response,
				   sr->sr_status,
				   sr->sr_phrase,
				   TAG_END());
  }

  if (sr->sr_status != sip->sip_status->st_status) {
    msg_destroy(response);
    SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);
    nta_incoming_treply(sr->sr_irq, sr->sr_status, sr->sr_phrase, TAG_END());
    return 0;
  }

  return nta_incoming_mreply(sr->sr_irq, response);
}

int nua_server_report(nua_server_request_t *sr)
{
  if (sr)
    return sr->sr_methods->sm_report(sr, NULL);
  else
    return 1;
}

int nua_base_server_treport(nua_server_request_t *sr,
			    tag_type_t tag, tag_value_t value,
			    ...)
{
  int retval;
  ta_list ta;
  ta_start(ta, tag, value);
  retval = nua_base_server_report(sr, ta_args(ta));
  ta_end(ta);
  return retval;
}

/**Report request event to the application.
 *
 * @retval 0 request lives
 * @retval 1 request was destroyed
 * @retval 2 request and its usage was destroyed
 * @retval 3 request, all usages and dialog was destroyed
 * @retval 4 request, all usages, dialog, and handle was destroyed
 */
int nua_base_server_report(nua_server_request_t *sr, tagi_t const *tags)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_t *nua = nh->nh_nua;
  nua_dialog_usage_t *usage = sr->sr_usage;
  int initial = sr->sr_initial;
  int status = sr->sr_status;
  char const *phrase = sr->sr_phrase;

  int terminated;
  int handle_can_be_terminated = initial && !sr->sr_event;

  if (sr->sr_application) {
    /* There was an error sending response */
    if (sr->sr_application != sr->sr_status)
      nua_stack_event(nua, nh, NULL, nua_i_error, status, phrase, tags);
    sr->sr_application = 0;
  }
  else if (status < 300 && !sr->sr_event) {
    msg_t *msg = msg_ref_create(sr->sr_request.msg);
    nua_event_t e = (enum nua_event_e)sr->sr_methods->sm_event;
    sr->sr_event = 1;
    nua_stack_event(nua, nh, msg, e, status, phrase, tags);
  }

  if (status < 200)
    return 0;			/* sr lives on until final response is sent */

  if (sr->sr_method == sip_method_invite && status < 300)
    return 0;			/* INVITE lives on until ACK is received */

  if (initial && 300 <= status)
    terminated = 1;
  else if (sr->sr_terminating && status < 300)
    terminated = 1;
  else
    terminated = sip_response_terminates_dialog(status, sr->sr_method, NULL);

  if (usage && terminated)
    nua_dialog_usage_remove(nh, nh->nh_ds, usage, NULL, sr);

  nua_server_request_destroy(sr);

  if (!terminated)
    return 1;

  if (!initial) {
    if (terminated > 0)
      return 2;

    /* Remove all usages of the dialog */
    nua_dialog_deinit(nh, nh->nh_ds);

    return 3;
  }
  else if (!handle_can_be_terminated) {
    return 3;
  }
  else {
    if (nh != nh->nh_nua->nua_dhandle)
      nh_destroy(nh->nh_nua, nh);

    return 4;
  }
}
