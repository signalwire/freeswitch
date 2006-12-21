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

/**@CFILE nua_notifier.c
 * @brief SUBSCRIBE server, NOTIFY client and REFER server
 *
 * Simpler event server. See nua_event_server.c for more complex event
 * server.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Mar  8 15:10:08 EET 2006 ppessi
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

#include "nua_stack.h"

/* ---------------------------------------------------------------------- */
/* Notifier event usage */

struct notifier_usage
{
  enum nua_substate  nu_substate;	/**< Subscription state */
  sip_time_t         nu_expires;
};

static char const *nua_notify_usage_name(nua_dialog_usage_t const *du);
static int nua_notify_usage_add(nua_handle_t *nh, 
				   nua_dialog_state_t *ds,
				   nua_dialog_usage_t *du);
static void nua_notify_usage_remove(nua_handle_t *nh, 
				       nua_dialog_state_t *ds,
				       nua_dialog_usage_t *du);
static void nua_notify_usage_refresh(nua_handle_t *nh,
				     nua_dialog_state_t *ds,
				     nua_dialog_usage_t *du,
				     sip_time_t now);
static int nua_notify_usage_shutdown(nua_handle_t *nh,
				     nua_dialog_state_t *ds,
				     nua_dialog_usage_t *du);

static nua_usage_class const nua_notify_usage[1] = {
  {
    sizeof (struct notifier_usage), (sizeof nua_notify_usage),
    nua_notify_usage_add,
    nua_notify_usage_remove,
    nua_notify_usage_name,
    NULL,
    nua_notify_usage_refresh,
    nua_notify_usage_shutdown,
  }};

static char const *nua_notify_usage_name(nua_dialog_usage_t const *du)
{
  return "notify";
}

static 
int nua_notify_usage_add(nua_handle_t *nh, 
			   nua_dialog_state_t *ds,
			   nua_dialog_usage_t *du)
{
  ds->ds_has_events++;
  ds->ds_has_notifys++;
  return 0;
}

static 
void nua_notify_usage_remove(nua_handle_t *nh, 
			       nua_dialog_state_t *ds,
			       nua_dialog_usage_t *du)
{
  ds->ds_has_events--;	
  ds->ds_has_notifys--;	
}

/* ====================================================================== */
/* SUBSCRIBE server */

static int respond_to_subscribe(nua_server_request_t *sr, tagi_t const *tags);

/** @NUA_EVENT nua_i_subscribe
 *
 * Incoming @b SUBSCRIBE request.
 *
 * @b SUBSCRIBE request is used to query SIP event state or establish a SIP
 * event subscription.
 *
 * Initial SUBSCRIBE requests are dropped with <i>489 Bad Event</i>
 * response, unless the application has explicitly included the @Event in
 * the list of allowed events with nua_set_params() tag NUTAG_ALLOW_EVENTS()
 * (or SIPTAG_ALLOW_EVENTS() or SIPTAG_ALLOW_EVENTS_STR()). The application
 * can decide whether to accept the SUBSCRIBE request or reject it. The
 * nua_response() call responding to a SUBSCRIBE request must have
 * NUTAG_WITH() (or NUTAG_WITH_CURRENT()/NUTAG_WITH_SAVED()) tag.
 *
 * If the application accepts the SUBSCRIBE request, it must immediately
 * send an initial NOTIFY establishing the dialog. This is because the
 * response to the SUBSCRIBE request may be lost because the SUBSCRIBE
 * request was forked by an intermediate proxy. 
 *
 * SUBSCRIBE requests modifying (usually refreshing or terminating) an
 * existing event subscription are accepted by default and a <i>200 OK</i>
 * response along with a copy of previously sent NOTIFY is sent
 * automatically.
 *
 * By default, only event subscriptions accepted are those created
 * implicitly by REFER request. See #nua_i_refer how the application must
 * handle the REFER requests.
 *
 * @param status status code of response sent automatically by stack
 * @param phrase response phrase sent automatically by stack
 * @param nh     operation handle associated with the incoming request
 * @param hmagic application context associated with the handle
 *               (NULL when handle is created by the stack)
 * @param sip    SUBSCRIBE request headers
 * @param tags   NUTAG_SUBSTATE()
 *
 * @sa @RFC3265, nua_notify(), NUTAG_SUBSTATE(), @SubscriptionState,
 * @Event, nua_subscribe(), #nua_r_subscribe, #nua_i_refer, nua_refer()
 *
 * @END_NUA_EVENT
 */


/** @internal Process incoming SUBSCRIBE. */
int nua_stack_process_subscribe(nua_t *nua,
				nua_handle_t *nh,
				nta_incoming_t *irq,
				sip_t const *sip)
{
  nua_server_request_t *sr, sr0[1];
  nua_dialog_state_t *ds;
  nua_dialog_usage_t *du = NULL;
  sip_event_t *o = sip->sip_event;
  char const *event = o ? o->o_type : NULL;
  
  enum nua_substate substate = nua_substate_terminated;

  enter;

  if (nh)
    du = nua_dialog_usage_get(ds = nh->nh_ds, nua_notify_usage, o);

  sr = SR_INIT(sr0);
  
  if (nh == NULL || du == NULL) {
    sip_allow_events_t *allow_events = NUA_PGET(nua, nh, allow_events);

    if (event && str0cmp(event, "refer") == 0)
      /* refer event subscription should be initiated with REFER */
      SR_STATUS1(sr, SIP_403_FORBIDDEN);
    else if (!event || !msg_header_find_param(allow_events->k_common, event))
      SR_STATUS1(sr, SIP_489_BAD_EVENT);
    else
      substate = nua_substate_embryonic;
  }
  else {
    /* Refresh existing subscription */
    struct notifier_usage *nu = nua_dialog_usage_private(du);
    unsigned long expires;

    assert(nh && du && nu);

    expires = str0cmp(event, "refer") ? 3600 : NH_PGET(nh, refer_expires);

    if (sip->sip_expires && sip->sip_expires->ex_delta < expires)
      expires = sip->sip_expires->ex_delta;

    if (expires == 0)
      nu->nu_substate = nua_substate_terminated;

    nu->nu_expires = sip_now() + expires;
    substate = nu->nu_substate;

    /* XXX - send notify */

    SR_STATUS1(sr, SIP_200_OK);
  }

  sr = nua_server_request(nua, nh, irq, sip, sr, sizeof *sr,
			  respond_to_subscribe, 1);

  if (!du && substate == nua_substate_embryonic && sr->sr_status < 300) {
    nh = sr->sr_owner; assert(nh && nh != nua->nua_dhandle);
    du = nua_dialog_usage_add(nh, nh->nh_ds, nua_notify_usage, sip->sip_event);
    if (du) {
      struct notifier_usage *nu = nua_dialog_usage_private(du);
      unsigned long expires = 3600; /* XXX */
      
      if (sip->sip_expires && sip->sip_expires->ex_delta < expires)
	expires = sip->sip_expires->ex_delta;

      nu->nu_expires = sip_now() + expires;
      nu->nu_substate = substate;
    }
    else 
      SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);
  }

  if (substate == nua_substate_embryonic && sr->sr_status >= 300)
    substate = nua_substate_terminated;

  sr->sr_usage = du;

  return nua_stack_server_event(nua, sr, nua_i_subscribe,
				NUTAG_SUBSTATE(substate), TAG_END());
}

/** @internal Respond to an SUBSCRIBE request.
 *
 */
static
int respond_to_subscribe(nua_server_request_t *sr, tagi_t const *tags)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_dialog_state_t *ds = nh->nh_ds;
  nua_t *nua = nh->nh_nua;
  struct notifier_usage *nu;
  sip_allow_events_t *allow_events = NUA_PGET(nua, nh, allow_events);
  sip_expires_t ex[1]; 
  sip_time_t now = sip_now();
  msg_t *msg;

  sip_expires_init(ex);

  nu = nua_dialog_usage_private(sr->sr_usage);
  if (nu && nu->nu_expires > now)
    ex->ex_delta = nu->nu_expires - now;

  msg = nua_server_response(sr,
			    sr->sr_status, sr->sr_phrase,
			    NUTAG_ADD_CONTACT(sr->sr_status < 300),
			    TAG_IF(nu, SIPTAG_EXPIRES(ex)),
			    SIPTAG_SUPPORTED(NH_PGET(nh, supported)),
			    SIPTAG_ALLOW_EVENTS(allow_events),
			    TAG_NEXT(tags));

  if (msg) {
    sip_t *sip = sip_object(msg);

    if (nu && sip->sip_expires && sr->sr_status < 300)
      nu->nu_expires = now + sip->sip_expires->ex_delta;

    nta_incoming_mreply(sr->sr_irq, msg);

    if (nu && nu->nu_substate != nua_substate_embryonic)
      /* Send NOTIFY (and terminate subscription, when needed) */
      nua_dialog_usage_refresh(nh, ds, sr->sr_usage, sip_now());
  }
  else {
    /* XXX - send nua_i_error */
    SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);
    nta_incoming_treply(sr->sr_irq, sr->sr_status, sr->sr_phrase, TAG_END());
  }
  
  return sr->sr_status >= 200 ? sr->sr_status : 0;
}

/* ======================================================================== */
/* NOTIFY */

static int process_response_to_notify(nua_handle_t *nh,
				      nta_outgoing_t *orq,
				      sip_t const *sip);

static int nua_stack_notify2(nua_t *, nua_handle_t *, nua_event_t, 
			     nua_dialog_usage_t *du,
			     tagi_t const *tags);


/**@fn void nua_notify(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...);
 *
 * Send a SIP NOTIFY request message.
 *
 * This function is used when the application implements itself the
 * notifier. The application must provide valid @SubscriptionState and
 * @Event headers using SIP tags. If there is no @SubscriptionState header,
 * the subscription state can be modified with NUTAG_SUBSTATE().
 *
 * @bug If the @Event is not given by application, stack uses the @Event
 * header from the first subscription usage on handle.
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return 
 *    nothing
 *
 * @par Related Tags:
 *    NUTAG_SUBSTATE() \n
 *    Tags of nua_set_hparams() \n
 *    Tags in <sip_tag.h>
 *
 * @par Events:
 *    #nua_r_notify
 *
 * @sa @RFC3265, #nua_i_subscribe, #nua_i_refer, NUTAG_ALLOW_EVENTS()
 */

/**@internal Send NOTIFY. */
int nua_stack_notify(nua_t *nua,
		     nua_handle_t *nh,
		     nua_event_t e,
		     tagi_t const *tags)
{
  return nua_stack_notify2(nua, nh, e, NULL, tags);
}


int nua_stack_notify2(nua_t *nua,
		      nua_handle_t *nh,
		      nua_event_t e,
		      nua_dialog_usage_t *du,
		      tagi_t const *tags)
{
  nua_client_request_t *cr = nh->nh_ds->ds_cr;
  struct notifier_usage *nu;
  msg_t *msg;
  sip_t *sip;
  sip_event_t const *o;
  sip_time_t now;
  int refresh = du != NULL;

  if (cr->cr_orq) {
    return UA_EVENT2(e, 900, "Request already in progress");
  }

  nua_stack_init_handle(nua, nh, TAG_NEXT(tags));

  if (refresh) {
    assert(!cr->cr_msg);
    if (cr->cr_msg)
      msg_destroy(cr->cr_msg);
    cr->cr_msg = msg_copy(du->du_msg);
  }

  msg = nua_creq_msg(nua, nh, cr, cr->cr_retry_count || refresh,
		     SIP_METHOD_NOTIFY,
		     NUTAG_ADD_CONTACT(1),
		     TAG_NEXT(tags));
  sip = sip_object(msg);
  if (!sip)
    return UA_EVENT1(e, NUA_INTERNAL_ERROR);

  if (nh->nh_ds->ds_has_notifys == 1 && !sip->sip_event)
    o = NONE;
  else
    o = sip->sip_event;

  du = nua_dialog_usage_get(nh->nh_ds, nua_notify_usage, o);
  nu = nua_dialog_usage_private(du);

  if (du && du->du_event && !sip->sip_event)
    sip_add_dup(msg, sip, (sip_header_t *)du->du_event);

  now = sip_now();

  if (!du)
    ;
  else if (sip->sip_subscription_state) {
    /* SIPTAG_SUBSCRIPTION_STATE() overrides NUTAG_SUBSTATE() */
    char const *ss_substate = sip->sip_subscription_state->ss_substate;

    if (strcasecmp(ss_substate, "terminated") == 0)
      nu->nu_substate = nua_substate_terminated;
    else if (strcasecmp(ss_substate, "pending") == 0)
      nu->nu_substate = nua_substate_pending;
    else /* if (strcasecmp(subs->ss_substate, "active") == 0) */ 
      nu->nu_substate = nua_substate_active;

    if (sip->sip_subscription_state->ss_expires) {
      unsigned long expires;
      expires = strtoul(sip->sip_subscription_state->ss_expires, NULL, 10);
      if (expires > 3600)
        expires = 3600;
      nu->nu_expires = now + expires;
    }
    else if (nu->nu_substate != nua_substate_terminated) {
      sip_subscription_state_t *ss = sip->sip_subscription_state;
      char *param;

      if (now < nu->nu_expires)
        param = su_sprintf(msg_home(msg), "expires=%lu", nu->nu_expires - now);
      else
        param = "expires=0";

      msg_header_add_param(msg_home(msg), ss->ss_common, param);
    }
  }
  else {
    sip_subscription_state_t *ss;
    enum nua_substate substate;
    char const *name;

    substate = nu->nu_substate;

    if (nu->nu_expires <= now)
      substate = nua_substate_terminated;

    if (substate != nua_substate_terminated) {
      tagi_t const *t = tl_find_last(tags, nutag_substate);
      if (t)
	substate = (enum nua_substate)t->t_value;
    }

    switch (substate) {
    case nua_substate_embryonic:
      /*FALLTHROUGH*/
    case nua_substate_pending:
      name = "pending";
      nu->nu_substate = nua_substate_pending;
      break;
    case nua_substate_active:
    default:
      name = "active";
      nu->nu_substate = nua_substate_active;
      break;
    case nua_substate_terminated:
      name = "terminated";
      nu->nu_substate = nua_substate_terminated;
      break;
    }

    if (nu->nu_substate != nua_substate_terminated) {
      unsigned long expires = nu->nu_expires - now;
      ss = sip_subscription_state_format(msg_home(msg), "%s;expires=%lu",
					 name, expires);
    }
    else {
      ss = sip_subscription_state_make(msg_home(msg), "terminated; "
				       "reason=noresource");
    }

    msg_header_insert(msg, (void *)sip, (void *)ss);
  }

  if (du) {
    if (nu->nu_substate == nua_substate_terminated)
      du->du_terminating = 1;

    if (!du->du_terminating && !refresh) {
      /* Save template */
      if (du->du_msg)
        msg_destroy(du->du_msg);
      du->du_msg = msg_ref_create(cr->cr_msg);
    }
  }

  /* NOTIFY outside a dialog */
  cr->cr_orq = nta_outgoing_mcreate(nua->nua_nta,
				    process_response_to_notify, nh, NULL,
				    msg,
				    SIPTAG_END(), TAG_NEXT(tags));

  if (!cr->cr_orq) {
    msg_destroy(msg);
    return UA_EVENT1(e, NUA_INTERNAL_ERROR);
  }

  cr->cr_usage = du;

  return cr->cr_event = e;
}

static
void restart_notify(nua_handle_t *nh, tagi_t *tags)
{
  nua_creq_restart(nh, nh->nh_ds->ds_cr, process_response_to_notify, tags);
}

/** @NUA_EVENT nua_r_notify
 *
 * Response to an outgoing @b NOTIFY request.
 *
 * The @b NOTIFY may be sent explicitly by nua_notify() or implicitly by NUA
 * state machine. Implicit @b NOTIFY is sent when an established dialog is
 * refreshed by client or it is terminated (either by client or because of a
 * timeout)
 *
 * @param status response status code
 *               (if the request is retried, @a status is 100, the @a
 *               sip->sip_status->st_status contain the real status code
 *               from the response message, e.g., 302, 401, or 407)
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the subscription
 * @param hmagic application context associated with the handle
 * @param sip    response to @b NOTIFY request or NULL upon an error 
 *               (status code is in @a status and 
 *               descriptive message in @a phrase parameters)
 * @param tags   NUTAG_SUBSTATE() indicating subscription state
 *
 * @sa nua_notify(), @RFC3265, #nua_i_subscribe, #nua_i_refer
 *
 * @END_NUA_EVENT
 */

static int process_response_to_notify(nua_handle_t *nh,
				      nta_outgoing_t *orq,
				      sip_t const *sip)
{
  enum nua_substate substate = nua_substate_terminated;

  if (nua_creq_check_restart(nh, nh->nh_ds->ds_cr, orq, sip, restart_notify))
    return 0;

  if (nh->nh_ds->ds_cr->cr_usage) {
    struct notifier_usage *nu = nua_dialog_usage_private(nh->nh_ds->ds_cr->cr_usage);
    substate = nu->nu_substate;
    assert(substate != nua_substate_embryonic);
  }

  return nua_stack_process_response(nh, nh->nh_ds->ds_cr, orq, sip, 
				    NUTAG_SUBSTATE(substate),
				    TAG_END());
}


static void nua_notify_usage_refresh(nua_handle_t *nh,
				     nua_dialog_state_t *ds,
				     nua_dialog_usage_t *du,
				     sip_time_t now)
{
  struct notifier_usage *nu = nua_dialog_usage_private(du);

  if (nh->nh_ds->ds_cr->cr_usage == du) /* Already notifying. */
    return;

  if (now >= nu->nu_expires) {
    sip_subscription_state_t ss[1];
    char const *params[] = { NULL, NULL };
    tagi_t tags[2] = {
      { SIPTAG_SUBSCRIPTION_STATE(ss) }, { TAG_END() }
    };

    sip_subscription_state_init(ss);

    ss->ss_substate = "terminated";
    ss->ss_params = params;
    params[0] = "reason=timeout";
    ss->ss_reason = "timeout";

    nua_stack_notify2(nh->nh_nua, nh, nua_r_notify, du, tags);
  }
  else {
    nua_stack_notify2(nh->nh_nua, nh, nua_r_notify, du, NULL);
  }
}

/** @interal Shut down NOTIFY usage. 
 *
 * @retval >0  shutdown done
 * @retval 0   shutdown in progress
 * @retval <0  try again later
 */
static int nua_notify_usage_shutdown(nua_handle_t *nh,
				     nua_dialog_state_t *ds,
				     nua_dialog_usage_t *du)
{
  nua_client_request_t *cr = nh->nh_ds->ds_cr;

  if (!cr->cr_usage) {
    /* Unnotify */
    nua_stack_notify2(nh->nh_nua, nh, nua_r_destroy, du, NULL);
    return cr->cr_usage != du;
  }

  if (!du->du_ready && !cr->cr_orq)
    return 1;			/* Unauthenticated NOTIFY? */

  return -1;  /* Request in progress */
}


/* ======================================================================== */
/* REFER */
/* RFC 3515 */

/** @NUA_EVENT nua_i_refer
 *
 * Incoming @b REFER request used to transfer calls.
 *
 * @param status status code of response sent automatically by stack
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the incoming request
 * @param hmagic application context associated with the handle
 *               (NULL if outside of an already established session)
 * @param sip    incoming REFER request
 * @param tags   NUTAG_REFER_EVENT() \n
 *               SIPTAG_REFERRED_BY()
 *  
 * @sa nua_refer(), #nua_r_refer, @ReferTo, NUTAG_REFER_EVENT(), 
 * SIPTAG_REFERRED_BY(), @ReferredBy, NUTAG_NOTIFY_REFER(),
 * NUTAG_REFER_WITH_ID(), @RFC3515.
 *
 * @END_NUA_EVENT
 */

/** @internal Process incoming REFER. */
int nua_stack_process_refer(nua_t *nua,
			    nua_handle_t *nh,
			    nta_incoming_t *irq,
			    sip_t const *sip)
{
  nua_dialog_usage_t *du = NULL;
  struct notifier_usage *nu;
  sip_event_t *event;
  sip_referred_by_t *by = NULL, default_by[1];
  msg_t *response;
  sip_time_t expires;
  int created = 0;

  if (nh == NULL) {
    if (!(nh = nua_stack_incoming_handle(nua, irq, sip, 1)))
      return 500;
    created = 1;
  }

  if (nh->nh_ds->ds_has_referrals || NH_PGET(nh, refer_with_id))
    event = sip_event_format(nh->nh_home, "refer;id=%u", sip->sip_cseq->cs_seq);
  else
    event = sip_event_make(nh->nh_home, "refer");

  if (event)
    du = nua_dialog_usage_add(nh, nh->nh_ds, nua_notify_usage, event);

  if (!du || du->du_ready) {
    if (du->du_ready) {
      SU_DEBUG_1(("nua(%p): REFER with existing refer;id=%u\n", nh,
		  sip->sip_cseq->cs_seq));
    }
    if (created) 
      nh_destroy(nua, nh);
    return 500;
  }

  nu = nua_dialog_usage_private(du);
  du->du_ready = 1;
  nh->nh_ds->ds_has_referrals = 1;

  nua_dialog_uas_route(nh, nh->nh_ds, sip, 1);	/* Set route and tags */

  if (!sip->sip_referred_by) {
    sip_from_t *a = sip->sip_from;

    sip_referred_by_init(by = default_by);

    *by->b_url = *a->a_url;
    by->b_display = a->a_display;
  }

  response = nh_make_response(nua, nh, irq, 
			      SIP_202_ACCEPTED, 
			      NUTAG_ADD_CONTACT(1),
			      TAG_END());

  nta_incoming_mreply(irq, response);

  expires = NH_PGET(nh, refer_expires);

  if (sip->sip_expires && sip->sip_expires->ex_delta < expires)
    expires = sip->sip_expires->ex_delta;
  nu->nu_substate = nua_substate_pending;
  nu->nu_expires = sip_now() + expires;

  /* Immediate notify in order to establish the dialog */
  if (!sip->sip_to->a_tag)
    nua_stack_post_signal(nh,
			  nua_r_notify,
			  SIPTAG_EVENT(event),
			  SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
			  SIPTAG_PAYLOAD_STR("SIP/2.0 100 Trying\r\n"),
			  TAG_END());
  
  nua_stack_event(nh->nh_nua, nh, nta_incoming_getrequest(irq),
		  nua_i_refer, SIP_202_ACCEPTED, 
		  NUTAG_REFER_EVENT(event),
		  TAG_IF(by, SIPTAG_REFERRED_BY(by)),
		  TAG_END());
  
  su_free(nh->nh_home, event);

  return 500;   
}
