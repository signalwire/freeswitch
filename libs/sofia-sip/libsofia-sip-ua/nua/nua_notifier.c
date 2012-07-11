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

#include <sofia-sip/su_string.h>
#include <sofia-sip/sip_protos.h>
#include <sofia-sip/sip_extra.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sip_util.h>
#include <sofia-sip/su_uniqueid.h>
#include <sofia-sip/su_md5.h>
#include <sofia-sip/token64.h>

#include "nua_stack.h"

/* ---------------------------------------------------------------------- */
/* Notifier event usage */

struct notifier_usage
{
  enum nua_substate  nu_substate;	/**< Subscription state */
  sip_time_t         nu_expires; 	/**< Expiration time */
  sip_time_t         nu_requested;      /**< Requested expiration time */
#if SU_HAVE_EXPERIMENTAL
  char              *nu_tag;	        /**< @ETag in last NOTIFY */
  unsigned           nu_etags:1;	/**< Subscriber supports etags */
  unsigned           nu_appl_etags:1;   /**< Application generates etags */
  unsigned           nu_no_body:1;      /**< Suppress body */
#endif
};

static char const *nua_notify_usage_name(nua_dialog_usage_t const *du);
static int nua_notify_usage_add(nua_handle_t *nh,
				   nua_dialog_state_t *ds,
				   nua_dialog_usage_t *du);
static void nua_notify_usage_remove(nua_handle_t *nh,
				    nua_dialog_state_t *ds,
				    nua_dialog_usage_t *du,
				    nua_client_request_t *cr,
				    nua_server_request_t *sr);
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
    nua_base_usage_update_params,
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
			     nua_dialog_usage_t *du,
			     nua_client_request_t *cr,
			     nua_server_request_t *sr)
{
  ds->ds_has_events--;
  ds->ds_has_notifys--;
}

/* ====================================================================== */
/* SUBSCRIBE server */

/** @NUA_EVENT nua_i_subscribe
 *
 * Incoming @b SUBSCRIBE request.
 *
 * @b SUBSCRIBE request is used to query SIP event state or establish a SIP
 * event subscription.
 *
 * @param status status code of response sent automatically by stack
 * @param phrase response phrase sent automatically by stack
 * @param nh     operation handle associated with the incoming request
 * @param hmagic application context associated with the handle
 *               (NULL when handle is created by the stack)
 * @param sip    SUBSCRIBE request headers
 * @param tags   NUTAG_SUBSTATE()
 *
 * Initial SUBSCRIBE requests are dropped with <i>489 Bad Event</i>
 * response, unless the application has explicitly included the @Event in
 * the list of allowed events with nua_set_params() tag NUTAG_ALLOW_EVENTS()
 * (or SIPTAG_ALLOW_EVENTS() or SIPTAG_ALLOW_EVENTS_STR()).
 *
 * If the event has been allowed the application
 * can decide whether to accept the SUBSCRIBE request or reject it. The
 * nua_response() call responding to a SUBSCRIBE request must have
 * NUTAG_WITH() (or NUTAG_WITH_THIS()/NUTAG_WITH_SAVED()) tag.
 *
 * If the application accepts the SUBSCRIBE request, it must immediately
 * send an initial NOTIFY establishing the dialog. This is because the
 * response to the SUBSCRIBE request may be lost by an intermediate proxy
 * because it had forked the SUBSCRIBE request.
 *
 * SUBSCRIBE requests modifying (usually refreshing or terminating) an
 * existing event subscription are accepted by default and a <i>200 OK</i>
 * response along with a copy of previously sent NOTIFY is sent
 * automatically to the subscriber.
 *
 * By default, only event subscriptions accepted are those created
 * implicitly by REFER request. See #nua_i_refer how the application must
 * handle the REFER requests.
 *
 * @par Subscription Lifetime and Terminating Subscriptions
 *
 * Accepting the SUBSCRIBE request creates a dialog with a <i>notifier
 * dialog usage</i> on the handle. The dialog usage is active, until the
 * subscriber terminates the subscription, it times out or the application
 * terminates the usage with nua_notify() call containing the tag
 * NUTAG_SUBSTATE(nua_substate_terminated) or @SubscriptionState header with
 * state "terminated" and/or expiration time 0.
 *
 * When the subscriber terminates the subscription, the application is
 * notified of an termination by a #nua_i_subscribe event with
 * NUTAG_SUBSTATE(nua_substate_terminated) tag. When the subscription times
 * out, nua automatically initiates a NOTIFY transaction. When it is
 * terminated, the application is sent a #nua_r_notify event with
 * NUTAG_SUBSTATE(nua_substate_terminated) tag.
 *
 * @sa @RFC3265, nua_notify(), NUTAG_SUBSTATE(), @SubscriptionState,
 * @Event, nua_subscribe(), #nua_r_subscribe, #nua_i_refer, nua_refer()
 *
 * @END_NUA_EVENT
 */

static int nua_subscribe_server_init(nua_server_request_t *sr);
static int nua_subscribe_server_preprocess(nua_server_request_t *sr);
static int nua_subscribe_server_respond(nua_server_request_t*, tagi_t const *);
static int nua_subscribe_server_report(nua_server_request_t*, tagi_t const *);

nua_server_methods_t const nua_subscribe_server_methods =
  {
    SIP_METHOD_SUBSCRIBE,
    nua_i_subscribe,		/* Event */
    {
      1,			/* Create dialog */
      0,			/* Initial request */
      1,			/* Target refresh request  */
      1,			/* Add Contact */
    },
    nua_subscribe_server_init,
    nua_subscribe_server_preprocess,
    nua_base_server_params,
    nua_subscribe_server_respond,
    nua_subscribe_server_report,
  };

int nua_subscribe_server_init(nua_server_request_t *sr)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_dialog_state_t *ds = nh->nh_ds;
  sip_allow_events_t const *allow_events = NH_PGET(nh, allow_events);
  sip_t const *sip = sr->sr_request.sip;
  sip_event_t *o = sip->sip_event;
  char const *event = o ? o->o_type : NULL;

  if (sr->sr_initial || !nua_dialog_usage_get(ds, nua_notify_usage, o)) {
    if (su_strmatch(event, "refer"))
      /* refer event subscription should be initiated with REFER */
      return SR_STATUS1(sr, SIP_403_FORBIDDEN);

    /* XXX - event is case-sensitive, should use msg_header_find_item() */
    if (!event || !msg_header_find_param(allow_events->k_common, event))
      return SR_STATUS1(sr, SIP_489_BAD_EVENT);
  }

  return 0;
}

int nua_subscribe_server_preprocess(nua_server_request_t *sr)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_dialog_state_t *ds = nh->nh_ds;
  nua_dialog_usage_t *du;
  struct notifier_usage *nu;
  sip_t const *sip = sr->sr_request.sip;
  sip_event_t *o = sip->sip_event;
  char const *event = o ? o->o_type : NULL;
  /* Maximum expiration time */
  unsigned long expires = sip->sip_expires ? sip->sip_expires->ex_delta : 3600;
  sip_time_t now = sip_now();

  assert(nh && nh->nh_nua->nua_dhandle != nh);

  du = nua_dialog_usage_get(ds, nua_notify_usage, o);

  if (du == NULL) {
    /* Create a new subscription */
    du = nua_dialog_usage_add(nh, ds, nua_notify_usage, o);
    if (du == NULL)
      return SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);
  }
  else {
    /* Refresh existing subscription */
	  if (su_strmatch(event, "refer")){
      expires = NH_PGET(nh, refer_expires);

	  SR_STATUS1(sr, SIP_200_OK);}
  }

  nu = nua_dialog_usage_private(du);

  if (now + expires >= now)
    nu->nu_requested = now + expires;
  else
    nu->nu_requested = SIP_TIME_MAX - 1;

#if SU_HAVE_EXPERIMENTAL
  nu->nu_etags =
    sip_suppress_body_if_match(sip) ||
    sip_suppress_notify_if_match(sip) ||
    sip_has_feature(sr->sr_request.sip->sip_supported, "etags");
#endif

  sr->sr_usage = du;

  return sr->sr_status <= 100 ? 0 : sr->sr_status;
}

/** @internal Respond to a SUBSCRIBE request.
 *
 */
static
int nua_subscribe_server_respond(nua_server_request_t *sr, tagi_t const *tags)
{
  struct notifier_usage *nu = nua_dialog_usage_private(sr->sr_usage);

  msg_t *msg = sr->sr_response.msg;
  sip_t *sip = sr->sr_response.sip;

  if (200 <= sr->sr_status && sr->sr_status < 300) {
    sip_expires_t ex[1];

    sip_expires_init(ex);

    if (nu) {
      sip_time_t now = sip_now();

      if (nu->nu_requested) {
	if (sip->sip_expires) {
	  /* Expires in response can only shorten the expiration time */
	  if (nu->nu_requested > now + sip->sip_expires->ex_delta)
	    nu->nu_requested = now + sip->sip_expires->ex_delta;
	}
	else {
	  unsigned sub_expires = NH_PGET(sr->sr_owner, sub_expires);
	  if (nu->nu_requested > now + sub_expires)
	    nu->nu_requested = now + sub_expires;
	}

	if (nu->nu_requested >= now)
	  nu->nu_expires = nu->nu_requested;
	else
	  nu->nu_expires = now;

	if (nu->nu_expires <= now)
	  nu->nu_substate = nua_substate_terminated;
      }

      if (nu->nu_expires > now)
	ex->ex_delta = nu->nu_expires - now;
    }
    else {
      /* Always add header Expires: 0 */
    }

    if (!sip->sip_expires || sip->sip_expires->ex_delta > ex->ex_delta)
      sip_add_dup(msg, sip, (sip_header_t *)ex);
  }

  return nua_base_server_respond(sr, tags);
}

static
int nua_subscribe_server_report(nua_server_request_t *sr, tagi_t const *tags)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_dialog_state_t *ds = nh->nh_ds;
  nua_dialog_usage_t *du = sr->sr_usage;
  struct notifier_usage *nu = nua_dialog_usage_private(du);
  enum nua_substate substate = nua_substate_terminated;
  int notify = 0;
  int retval;

  if (nu && !sr->sr_terminating) {
    substate = nu->nu_substate;
  }

  /* nu_requested is set by SUBSCRIBE and cleared when NOTIFY is sent */
  if (nu && nu->nu_requested && substate != nua_substate_embryonic) {
#if SU_HAVE_EXPERIMENTAL
    sip_t const *sip = sr->sr_request.sip;
    sip_suppress_notify_if_match_t *snim = sip_suppress_notify_if_match(sip);
    sip_suppress_body_if_match_t *sbim = sip_suppress_body_if_match(sip);

    if (!nu->nu_tag)
      notify = 1;
    else if (snim && su_casematch(snim->snim_tag, nu->nu_tag))
      notify = 0;
    else if (sbim && su_casematch(snim->snim_tag, nu->nu_tag))
      notify = 1, nu->nu_no_body = 1;
    else
#endif
      notify = 1;

    notify = notify && du->du_cr != NULL;
  }

  retval = nua_base_server_treport(sr, NUTAG_SUBSTATE(substate), TAG_END());

  if (retval >= 2 || du == NULL)
    return retval;

  if (notify) {
    /* Send NOTIFY (and terminate subscription, when needed) */
    nua_dialog_usage_refresh(nh, ds, du, sip_now());
  }

  return retval;
}

/* ======================================================================== */
/* NOTIFY client */

/**@fn void nua_notify(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...);
 *
 * Send a SIP NOTIFY request message.
 *
 * This function is used when the application implements itself the
 * notifier. The application must provide valid @SubscriptionState and
 * @Event headers using SIP tags. The subscription state can be modified
 * with NUTAG_SUBSTATE(), however, its effect is overriden by
 * @SubscriptionState header included in the nua_notify() tags.
 *
 * @bug If the @Event is not given by application, stack uses the @Event
 * header from the first subscription usage on handle.
 *
 * If there is no active <i>notifier dialog usage</i> or no notifier dialog
 * usage matches the @Event header given by the application the nua_notify()
 * request is rejected locally by the stack with status code 481. The local
 * rejection can be bypassed if NUTAG_NEWSUB(1) is included in tags.
 *
 * Please note that including NUTAG_NEWSUB(1) in nua_notify() tags if there
 * is a valid subscription may lead to an extra NOTIFY sent to subscriber if
 * the subscription had been terminated by the subscriber or by a timeout
 * before the nua_notify() is processed.
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *    nothing
 *
 * @par Related Tags:
 *    NUTAG_SUBSTATE() \n
 *    NUTAG_NEWSUB() \n
 *    Tags of nua_set_hparams() \n
 *    Header tags defined in <sofia-sip/sip_tag.h>
 *
 * @par Events:
 *    #nua_r_notify
 *
 * @sa @RFC3265, #nua_i_subscribe, #nua_i_refer, NUTAG_ALLOW_EVENTS()
 */

#if 0
static int nua_notify_client_init(nua_client_request_t *cr,
				  msg_t *, sip_t *,
				  tagi_t const *tags);


static int nua_notify_client_init_etag(nua_client_request_t *cr,
				       msg_t *msg, sip_t *sip,
				       tagi_t const *tags);

static int nua_notify_client_request(nua_client_request_t *cr,
				     msg_t *, sip_t *,
				     tagi_t const *tags);
static int nua_notify_client_report(nua_client_request_t *cr,
				    int status, char const *phrase,
				    sip_t const *sip,
				    nta_outgoing_t *orq,
				    tagi_t const *tags);
#endif
#if 0
static nua_client_methods_t const nua_notify_client_methods = {
  SIP_METHOD_NOTIFY,		/* crm_method, crm_method_name */
  0,				/* crm_extra */
  {				/* crm_flags */
    /* create_dialog */ 1,
    /* in_dialog */ 1,
    /* target refresh */ 1
  },
  NULL,				/* crm_template */
  nua_notify_client_init,	/* crm_init */
  nua_notify_client_request,	/* crm_send */
  NULL,				/* crm_check_restart */
  NULL,				/* crm_recv */
  NULL,				/* crm_preliminary */
  nua_notify_client_report,	/* crm_report */
  NULL,				/* crm_complete */
};
#endif

nua_client_methods_t const nua_notify_client_methods = {
  SIP_METHOD_NOTIFY,		/* crm_method, crm_method_name */
  0,				/* crm_extra */
  {				/* crm_flags */
    /* create_dialog */ 1,
    /* in_dialog */ 1,
    /* target refresh */ 1
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

/**@internal Send NOTIFY. */
int nua_stack_notify(nua_t *nua,
		     nua_handle_t *nh,
		     nua_event_t e,
		     tagi_t const *tags)
{
  return nua_client_create(nh, e, &nua_notify_client_methods, tags);
}
#if 0
static int nua_notify_client_init(nua_client_request_t *cr,
				  msg_t *msg, sip_t *sip,
				  tagi_t const *tags)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du;
  struct notifier_usage *nu;
  sip_event_t const *o = sip->sip_event;
  sip_subscription_state_t *ss = sip->sip_subscription_state;
  sip_time_t now = sip_now();

  if (o == NULL && nh->nh_ds->ds_has_notifys == 1)
    o = NONE;

  du = nua_dialog_usage_get(nh->nh_ds, nua_notify_usage, o);

  if (!du) {
    tagi_t const *newsub = tl_find_last(tags, nutag_newsub);

    if (!newsub || !newsub->t_value)
      return 0; /* Rejected eventually by nua_notify_client_request() */

    /* Create new notifier */
    du = nua_dialog_usage_add(nh, nh->nh_ds, nua_notify_usage, o);
    if (du == NULL)
      return -1;

    nu = nua_dialog_usage_private(du);
    nu->nu_expires = now;
  }
  else
    nu = nua_dialog_usage_private(du);


  if (nu->nu_substate == nua_substate_terminated) {
    /*Xyzzy*/;
  }
  else if (ss != NULL) {
    /* SIPTAG_SUBSCRIPTION_STATE() overrides NUTAG_SUBSTATE() */
    nu->nu_substate = nua_substate_make(ss->ss_substate);

    if (ss->ss_expires) {
      unsigned long expires = strtoul(ss->ss_expires, NULL, 10);
      if (now + expires < now)
	expires = SIP_TIME_MAX - now - 1;

      /* We can change the lifetime of unsolicited subscription at will */
      if (nu->nu_requested == 0)
	nu->nu_expires = nu->nu_requested = now + expires;
      /* Notifier can only shorten the subscribed time */
      else if (nu->nu_requested >= now + expires)
	nu->nu_expires = nu->nu_requested = now + expires;
    }
    else {
      if (nu->nu_requested >= nu->nu_expires)
	nu->nu_expires = nu->nu_requested;
    }

  }
  else {
    enum nua_substate substate = nu->nu_substate;

    if (nu->nu_requested >= nu->nu_expires)
      nu->nu_expires = nu->nu_requested;

    if (nu->nu_expires > now) {
      tagi_t const *t = tl_find_last(tags, nutag_substate);
      if (t)
        substate = (enum nua_substate)t->t_value;
    }
    else
      substate = nua_substate_terminated;

    switch (substate) {
    case nua_substate_embryonic:
      /*FALLTHROUGH*/
    case nua_substate_pending:
      nu->nu_substate = nua_substate_pending;
      break;
    case nua_substate_active:
    default:
      nu->nu_substate = nua_substate_active;
      break;
    case nua_substate_terminated:
      nu->nu_substate = nua_substate_terminated;
      break;
    }
  }

  cr->cr_usage = du;

  return nua_notify_client_init_etag(cr, msg, sip, tags);
}

static int nua_notify_client_init_etag(nua_client_request_t *cr,
				       msg_t *msg, sip_t *sip,
				       tagi_t const *tags)
{
#if SU_HAVE_EXPERIMENTAL
  nua_handle_t *nh = cr->cr_owner;
  struct notifier_usage *nu = nua_dialog_usage_private(cr->cr_usage);
  nua_server_request_t *sr;

  if (nu->nu_tag)
    su_free(nh->nh_home, nu->nu_tag), nu->nu_tag = NULL;
    nu->nu_no_body = 0;

  if (sip->sip_etag) {
    nu->nu_appl_etags = 1;
    nu->nu_tag = su_strdup(nh->nh_home, sip->sip_etag->g_string);
  }
  else if (!nu->nu_appl_etags && nu->nu_etags) {
    su_md5_t md5[1];
    unsigned char digest[SU_MD5_DIGEST_SIZE];
    sip_payload_t pl[1] = { SIP_PAYLOAD_INIT() };
    char token[2 * 16];

    su_md5_init(md5);

    if (sip->sip_payload) *pl = *sip->sip_payload;

    if (pl->pl_len)
      su_md5_update(md5, pl->pl_data, pl->pl_len);
    su_md5_update(md5, &pl->pl_len, sizeof(pl->pl_len));

    if (sip->sip_content_type)
      su_md5_striupdate(md5, sip->sip_content_type->c_type);

    su_md5_digest(md5, digest);
    token64_e(token, sizeof token, digest, sizeof digest);
    token[(sizeof token) - 1] = '\0';
    nu->nu_tag = su_strdup(nh->nh_home, token);
  }

  if (!nu->nu_requested || !nu->nu_tag)
    return 0;

  /* Check if SUBSCRIBE had matching suppression */
  for (sr = nh->nh_ds->ds_sr; sr; sr = sr->sr_next)
    if (sr->sr_usage == cr->cr_usage && sr->sr_method == sip_method_subscribe)
      break;

  if (sr) {
    sip_t const *sip = sr->sr_request.sip;

    sip_suppress_body_if_match_t *sbim;
    sip_suppress_notify_if_match_t *snim;

    if (cr->cr_usage->du_ready) {
      snim = sip_suppress_notify_if_match(sip);

      if (snim && su_casematch(snim->snim_tag, nu->nu_tag)) {
	if (nu->nu_requested > nu->nu_expires)
	  nu->nu_expires = nu->nu_requested;
	nu->nu_requested = 0;
	return nua_client_return(cr, 202, "NOTIFY Suppressed", msg);
      }
    }

    sbim = sip_suppress_body_if_match(sip);
    if (sbim && su_casematch(sbim->sbim_tag, nu->nu_tag))
      nu->nu_no_body = 1;
  }
#endif

  return 0;
}

static
int nua_notify_client_request(nua_client_request_t *cr,
			      msg_t *msg, sip_t *sip,
			      tagi_t const *tags)
{
  nua_dialog_usage_t *du = cr->cr_usage;
  struct notifier_usage *nu = nua_dialog_usage_private(du);
  su_home_t *home = msg_home(msg);
  sip_time_t now = sip_now();
  sip_subscription_state_t *ss = sip->sip_subscription_state;
  char const *expires;

  if (du == NULL)		/* Subscription has been terminated */
    return nua_client_return(cr, SIP_481_NO_TRANSACTION, msg);

  assert(du && nu);

  if (du && nua_client_bind(cr, du) < 0)
    return -1;

  if (nu->nu_requested)
    nu->nu_expires = nu->nu_requested;
  nu->nu_requested = 0;

  if (nu->nu_expires <= now || du->du_shutdown) {
    nu->nu_substate = nua_substate_terminated;
    expires = "expires=0";
  }
  else {
    expires = su_sprintf(home, "expires=%lu", nu->nu_expires - now);
  }

  if (ss == NULL || nua_substate_make(ss->ss_substate) != nu->nu_substate) {
    if (nu->nu_substate == nua_substate_terminated)
      expires = nu->nu_expires > now ? "reason=noresource" : "reason=timeout";

    ss = sip_subscription_state_format(home, "%s;%s",
				       nua_substate_name(nu->nu_substate),
				       expires);

    msg_header_insert(msg, (void *)sip, (void *)ss);
  }
  else if (nu->nu_substate != nua_substate_terminated) {
    msg_header_replace_param(home, ss->ss_common, expires);
  }

#if SU_HAVE_EXPERIMENTAL
  if (nu->nu_tag && !sip->sip_etag)
    msg_header_add_make(msg, (void *)sip, sip_etag_class, nu->nu_tag);

  if (nu->nu_no_body) {
    nu->nu_no_body = 0;
    msg_header_remove(msg, (void *)sip, (void *)sip->sip_payload);
    msg_header_remove(msg, (void *)sip, (void *)sip->sip_content_length);
  }
#endif

  if (nu->nu_substate == nua_substate_terminated)
    nua_client_set_terminating(cr, 1);

  if (cr->cr_terminating) {
    nua_server_request_t *sr;
    for (sr = du->du_dialog->ds_sr; sr; sr = sr->sr_next) {
      if (sr->sr_usage == du) {
	/* If subscribe has not been responded, don't terminate usage by NOTIFY */
	sr->sr_terminating = 1;
	// nua_client_set_terminating(cr, 0);
	break;
      }
    }
  }

  if (du->du_event && !sip->sip_event)
    sip_add_dup(cr->cr_msg, sip, (sip_header_t *)du->du_event);

  return nua_base_client_request(cr, msg, sip, tags);
}
#endif
/** @NUA_EVENT nua_r_notify
 *
 * Response to an outgoing @b NOTIFY request.
 *
 * The @b NOTIFY may be sent explicitly by nua_notify() or implicitly by NUA
 * state machine. Implicit @b NOTIFY is sent when an established dialog is
 * refreshed by client or it is terminated (either by client or because of a
 * timeout).
 *
 * The current subscription state is included in NUTAG_SUBSTATE() tag. The
 * nua_substate_terminated indicates that the subscription is terminated,
 * the notifier usage has been removed and when there was no other usages of
 * the dialog the dialog state is also removed.
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
 *               SIPTAG_EVENT() indicating subscription event
 *
 * @sa nua_notify(), @RFC3265, #nua_i_subscribe, #nua_i_refer, NUTAG_SUBSTATE()
 *
 * @END_NUA_EVENT
 */
#if 0
static int nua_notify_client_report(nua_client_request_t *cr,
				    int status, char const *phrase,
				    sip_t const *sip,
				    nta_outgoing_t *orq,
				    tagi_t const *tags)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du = cr->cr_usage;
  struct notifier_usage *nu = nua_dialog_usage_private(du);
  enum nua_substate substate = nua_substate_terminated;

  if (nu && !cr->cr_terminated)
    substate = nu->nu_substate;

  nua_stack_tevent(nh->nh_nua, nh,
		   nta_outgoing_getresponse(orq),
		   (enum nua_event_e)cr->cr_event,
		   status, phrase,
		   NUTAG_SUBSTATE(substate),
		   SIPTAG_EVENT(du ? du->du_event : NULL),
		   TAG_NEXT(tags));

  if (du && du->du_cr == cr && !cr->cr_terminated) {
    if (nu->nu_requested) {
      /* Re-SUBSCRIBEd while NOTIFY was in progress, resend NOTIFY */
      nua_client_resend_request(cr, 0);
    }
    else if (nu->nu_expires) {
      nua_dialog_usage_set_refresh_at(du, nu->nu_expires);
    }
  }

  return 0;
}
#endif

static void nua_notify_usage_refresh(nua_handle_t *nh,
				     nua_dialog_state_t *ds,
				     nua_dialog_usage_t *du,
				     sip_time_t now)
{
  struct notifier_usage *nu = nua_dialog_usage_private(du);
  nua_client_request_t *cr = du->du_cr;
  nua_event_t e = nua_r_notify;

  if (cr) {
    int terminating = 0;

    if (nu->nu_expires && nu->nu_expires <= now)
      terminating = 1;
    else if (nu->nu_requested && nu->nu_requested <= now)
      terminating = 1;

    if (nua_client_resend_request(cr, terminating) >= 0)
      return;
  }
  else {
    if (nua_client_create(nh, e, &nua_notify_client_methods, NULL) >= 0)
      return;
  }

  nua_stack_tevent(nh->nh_nua, nh, NULL, e, NUA_ERROR_AT(__FILE__, __LINE__),
		   NUTAG_SUBSTATE(nua_substate_terminated),
		   TAG_END());

  nua_dialog_usage_remove(nh, ds, du, NULL, NULL);
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
  struct notifier_usage *nu = nua_dialog_usage_private(du);
  //nua_client_request_t *cr = du->du_cr;

  nu->nu_substate = nua_substate_terminated;
#if 0
  if (cr) {
    SU_DEBUG_5(("%s(%p, %p, %p): using existing cr=%p\n",
		"nua_notify_usage_shutdown",
		(void *)nh, (void *)ds, (void *)du, (void *)cr));

    if (nua_client_resend_request(cr, 1) >= 0)
      return 0;
  }
  else {
    SU_DEBUG_5(("%s(%p, %p, %p): new NOTIFY cr for %s\n",
		"nua_notify_usage_shutdown",
		(void *)nh, (void *)ds, (void *)du,
		du->du_event ? du->du_event->o_type : "<implicit>"));

    if (nua_client_tcreate(nh, nua_r_notify,
			   &nua_notify_client_methods,
			   SIPTAG_EVENT(du->du_event),
			   NUTAG_SUBSTATE(nua_substate_terminated),
			   TAG_END()) >= 0)
      return 0;
  }
#endif
  nua_dialog_usage_remove(nh, ds, du, NULL, NULL);
  return 200;
}

/* ======================================================================== */
/* REFER */
/* RFC 3515 */

static int nua_refer_server_init(nua_server_request_t *sr);
static int nua_refer_server_preprocess(nua_server_request_t *sr);
static int nua_refer_server_respond(nua_server_request_t*, tagi_t const *);
static int nua_refer_server_report(nua_server_request_t*, tagi_t const *);

nua_server_methods_t const nua_refer_server_methods =
  {
    SIP_METHOD_REFER,
    nua_i_refer,		/* Event */
    {
      1,			/* Create dialog */
      0,			/* Initial request */
      1,			/* Target refresh request  */
      1,			/* Add Contact */
    },
    nua_refer_server_init,
    nua_refer_server_preprocess,
    nua_base_server_params,
    nua_refer_server_respond,
    nua_refer_server_report,
  };

static int nua_refer_server_init(nua_server_request_t *sr)
{
  return 0;
}

static int nua_refer_server_preprocess(nua_server_request_t *sr)
{
  nua_handle_t *nh = sr->sr_owner;
  sip_t const *sip = sr->sr_request.sip;
  struct notifier_usage *nu;
  sip_event_t *o;

  if (nh->nh_ds->ds_got_referrals || NH_PGET(nh, refer_with_id))
    o = sip_event_format(nh->nh_home, "refer;id=%u", sip->sip_cseq->cs_seq);
  else
    o = sip_event_make(nh->nh_home, "refer");

  if (o) {
    sr->sr_usage = nua_dialog_usage_add(nh, nh->nh_ds, nua_notify_usage, o);
    msg_header_free(nh->nh_home, (msg_header_t *)o);
  }

  if (!sr->sr_usage)
    return SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);

  nu = nua_dialog_usage_private(sr->sr_usage);
  nu->nu_requested = sip_now() + NH_PGET(nh, refer_expires);

  return 0;
}

static
int nua_refer_server_respond(nua_server_request_t *sr, tagi_t const *tags)
{
  nua_handle_t *nh = sr->sr_owner;
  struct notifier_usage *nu = nua_dialog_usage_private(sr->sr_usage);
  sip_refer_sub_t const *rs = sip_refer_sub(sr->sr_response.sip);

  if (sr->sr_status < 200 || nu == NULL) {
  }
  else if (sr->sr_status < 300 &&
	   /* No subscription if Refer-Sub: false in response */
	   (rs == NULL || !su_casematch(rs->rs_value, "false"))) {
    sr->sr_usage->du_ready = 1;

    nu->nu_expires = sip_now() + NH_PGET(nh, refer_expires);

    if (sr->sr_application)	/* Application responded to REFER */
      nu->nu_substate = nua_substate_active;
  }
  else {
    /* Destroy the implicit subscription usage */
    sr->sr_terminating = 1;
  }

  return nua_base_server_respond(sr, tags);
}


/** @NUA_EVENT nua_i_refer
 *
 * Incoming @b REFER request used to transfer calls. The tag list will
 * contain tag NUTAG_REFER_EVENT() with the @Event header constructed from
 * the REFER request. It will also contain the SIPTAG_REFERRED_BY() tag with
 * the @ReferredBy header containing the identity of the party sending the
 * REFER. The @ReferredBy structure contained in the tag is constructed from
 * the @From header if the @ReferredBy header was not present in the REFER
 * request.
 *
 * The application can let the nua to send NOTIFYs from the call it
 * initiates with nua_invite() if it includes in the nua_invite() arguments
 * both the NUTAG_NOTIFY_REFER() with the handle with which nua_i_refer was
 * received and the NUTAG_REFER_EVENT() from #nua_i_refer event tags.
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

static
int nua_refer_server_report(nua_server_request_t *sr, tagi_t const *tags)
{
	//nua_handle_t *nh = sr->sr_owner;
  struct notifier_usage *nu = nua_dialog_usage_private(sr->sr_usage);
  sip_t const *sip = sr->sr_request.sip;
  sip_referred_by_t *by = sip->sip_referred_by, default_by[1];
  sip_event_t const *o = sr->sr_usage->du_event;
  enum nua_substate substate = nua_substate_terminated;
  //int initial = sr->sr_initial, retval;
  int retval;

  if (nu) {
    if (!sr->sr_terminating)
      substate = nu->nu_substate;
  }

  if (by == NULL) {
     by = sip_referred_by_init(default_by);

    by->b_display = sip->sip_from->a_display;
    *by->b_url = *sip->sip_from->a_url;
  }

  retval = nua_base_server_treport(sr,
				   NUTAG_SUBSTATE(substate),
				   NUTAG_REFER_EVENT(o),
				   TAG_IF(by, SIPTAG_REFERRED_BY(by)),
				   TAG_END());

  if (retval >= 2 || nu == NULL)
    return retval;

#if 0
  if (initial)
    nua_stack_post_signal(nh,
			  nua_r_notify,
			  SIPTAG_EVENT(o),
			  SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
			  SIPTAG_PAYLOAD_STR("SIP/2.0 100 Trying\r\n"),
		  TAG_END());
#endif
  return retval;
}
