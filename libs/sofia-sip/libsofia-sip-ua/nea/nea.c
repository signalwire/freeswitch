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

/**@CFILE nea.c  Nokia Event Client API agent implementation.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Feb 14 18:32:58 2001 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_string.h>

#include <sofia-sip/sip.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_util.h>
#include <sofia-sip/sip_status.h>

#define SU_TIMER_ARG_T       struct nea_s
#define NTA_LEG_MAGIC_T      struct nea_s
#define NTA_OUTGOING_MAGIC_T struct nea_s

#define NEA_TIMER_DELTA 2 /* time to resubscribe without expiration */
#define EXPIRES_DEFAULT       3600

#include <sofia-sip/su_wait.h>

#include "sofia-sip/nea.h"

struct nea_s {
  su_home_t         nea_home[1];
  su_timer_t       *nea_timer;

  nta_agent_t      *nea_agent;
  nta_leg_t        *nea_leg;
  nta_outgoing_t   *nea_oreq;		/**< Outstanding request */
  sip_to_t         *nea_to;		/**< The other end of subscription :) */
  nea_notify_f      nea_callback;	/**< Notify callback  */
  nea_magic_t      *nea_context;	/**< Application context */

  sip_contact_t    *nea_contact;	/**< */
  sip_expires_t    *nea_expires;	/**< Proposed expiration time */

  nea_state_t       nea_state;	        /**< State of our subscription */
  sip_time_t        nea_deadline;	/**< When our subscription expires */
  tagi_t           *nea_args;

  unsigned          nea_dialog : 1;     /**< Dialog has been established */
  unsigned          nea_notify_received : 1;
  unsigned          nea_terminating : 1;
  unsigned          nea_strict_3265 : 1;       /**< Strict mode */
};

int details = 0;

static int process_nea_request(nea_t *nea,
			       nta_leg_t *leg,
			       nta_incoming_t *ireq,
			       sip_t const *sip);

static int handle_notify(nta_leg_magic_t *lmagic,
			 nta_leg_t *leg,
			 nta_incoming_t *ireq,
			 sip_t const *sip);

static int response_to_subscribe(nea_t *nea,
				 nta_outgoing_t *req,
				 sip_t const *sip);

static int response_to_unsubscribe(nea_t *nea,
				   nta_outgoing_t *req,
				   sip_t const *sip);

static void nea_expires_renew(su_root_magic_t *magic,
		       su_timer_t *timer,
		       nea_t *nea);

/* ---------------------------------------------------------- */

/** Create a event watcher object.
 *
 */
nea_t *nea_create(nta_agent_t *agent,
		  su_root_t *root,
		  nea_notify_f no_callback,
		  nea_magic_t *context,
		  tag_type_t tag, tag_value_t value, ...)
{
  nea_t *nea = NULL;
  ta_list ta;
  int have_from, have_to, have_contact;
  sip_expires_t const *expires = NULL;
  char const *expires_str = NULL;
  sip_method_t method = sip_method_subscribe;
  char const *SUBSCRIBE = "SUBSCRIBE";
  char const *method_name = SUBSCRIBE;

  ta_start(ta, tag, value);

  have_to =
    tl_find(ta_args(ta), siptag_to) || tl_find(ta_args(ta), siptag_to_str);
  have_from =
    tl_find(ta_args(ta), siptag_from) || tl_find(ta_args(ta), siptag_from_str);
  have_contact =
    tl_find(ta_args(ta), siptag_contact) ||
    tl_find(ta_args(ta), siptag_contact_str);

  if (have_to && (nea = su_home_new(sizeof(nea_t)))) {
    su_home_t      *home = nea->nea_home;
    sip_contact_t  *m = nta_agent_contact(agent);
    sip_from_t     *from;
    sip_to_t const *to;
    int strict = 0;

    nea->nea_agent = agent;
    nea->nea_callback = no_callback;
    nea->nea_context = context;

    if (!have_from)
      from = sip_from_create(home, (url_string_t*)m->m_url);
    else
      from = NULL;

    nea->nea_args = tl_tlist(home,
			     TAG_IF(!have_contact, SIPTAG_CONTACT(m)),
			     ta_tags(ta));

    /* Get and remove Expires header from tag list */
    tl_gets(nea->nea_args,
	    SIPTAG_EXPIRES_REF(expires),
	    SIPTAG_EXPIRES_STR_REF(expires_str),
	    SIPTAG_TO_REF(to),
	    NEATAG_STRICT_3265_REF(strict),
	    NTATAG_METHOD_REF(method_name),
	    TAG_END());

    nea->nea_strict_3265 = strict;

    if (to)
      nea->nea_to = sip_to_dup(home, to);

    if (expires)
      nea->nea_expires = sip_expires_dup(home, expires);
    else if (expires_str)
      nea->nea_expires = sip_expires_make(home, expires_str);
    else
      nea->nea_expires = sip_expires_create(home, EXPIRES_DEFAULT);

    tl_tremove(nea->nea_args,
	       SIPTAG_EXPIRES(0),
	       SIPTAG_EXPIRES_STR(0),
	       TAG_END());

    if (method_name != SUBSCRIBE)
      method = sip_method_code(method_name);

    if (method != sip_method_invalid)
      /* Create the timer object */
      nea->nea_timer = su_timer_create(su_root_task(root), 0L);

    if (nea->nea_timer) {
      /* Create leg for NOTIFY requests */
      nea->nea_leg = nta_leg_tcreate(nea->nea_agent,
				     process_nea_request, nea,
				     TAG_IF(!have_from, SIPTAG_FROM(from)),
				     TAG_NEXT(nea->nea_args));

      if (nea->nea_leg) {
	nta_leg_tag(nea->nea_leg, NULL);
	nea->nea_oreq = nta_outgoing_tcreate(nea->nea_leg,
					     response_to_subscribe, nea,
					     NULL,
					     method, method_name,
					     NULL,
					     SIPTAG_EXPIRES(nea->nea_expires),
					     TAG_NEXT(nea->nea_args));
      }
    }

    if (!nea->nea_leg ||
	!nea->nea_oreq ||
	!nea->nea_timer)
      nea_destroy(nea), nea = NULL;
  }

  ta_end(ta);
  return nea;
}


int nea_update(nea_t *nea,
	       tag_type_t tag,
	       tag_value_t value,
	       ...)
{
  ta_list ta;
  sip_expires_t const *expires = NULL;
  sip_payload_t const *pl = NULL;
  sip_content_type_t const *ct = NULL;
  char const *cts = NULL;

  /* char const *expires_str = NULL; */
  su_home_t *home = nea->nea_home;

  /* XXX - hack, previous request still waiting for response */
  if (!nea->nea_leg || nea->nea_oreq)
    return -1;

  ta_start(ta, tag, value);

  tl_gets(ta_args(ta),
	  SIPTAG_CONTENT_TYPE_REF(ct),
	  SIPTAG_CONTENT_TYPE_STR_REF(cts),
	  SIPTAG_PAYLOAD_REF(pl),
	  SIPTAG_EXPIRES_REF(expires),
	  TAG_NULL());

  if (!pl || (!ct && !cts)) {
    ta_end(ta);
    return -1;
  }

  tl_tremove(nea->nea_args,
	     SIPTAG_CONTENT_TYPE(0),
	     SIPTAG_CONTENT_TYPE_STR(0),
	     SIPTAG_PAYLOAD(0),
	     SIPTAG_PAYLOAD_STR(0),
	     TAG_END());

  su_free(home, nea->nea_expires);

  if (expires)
    nea->nea_expires = sip_expires_dup(home, expires);
  else
    nea->nea_expires = sip_expires_create(home, EXPIRES_DEFAULT);

  /* nta_leg_tag(nea->nea_leg, NULL); */
  nea->nea_oreq = nta_outgoing_tcreate(nea->nea_leg,
				       response_to_subscribe, nea,
				       NULL,
				       SIP_METHOD_SUBSCRIBE,
				       NULL,
				       SIPTAG_TO(nea->nea_to),
				       SIPTAG_PAYLOAD(pl),
				       TAG_IF(ct, SIPTAG_CONTENT_TYPE(ct)),
				       TAG_IF(cts, SIPTAG_CONTENT_TYPE_STR(cts)),
				       SIPTAG_EXPIRES(nea->nea_expires),
				       TAG_NEXT(nea->nea_args));

  ta_end(ta);

  if (!nea->nea_oreq)
    return -1;

  return 0;
}


/** Unsubscribe the agent. */
void nea_end(nea_t *nea)
{
  if (nea == NULL)
    return;

  nea->nea_terminating = 1;

  su_timer_destroy(nea->nea_timer), nea->nea_timer = NULL;

  if (nea->nea_leg && nea->nea_deadline) {
    nea->nea_oreq =
      nta_outgoing_tcreate(nea->nea_leg,
			   response_to_unsubscribe,
			   nea,
			   NULL,
			   SIP_METHOD_SUBSCRIBE,
			   NULL,
			   SIPTAG_EXPIRES_STR("0"),
			   TAG_NEXT(nea->nea_args));
  }
}

void nea_destroy(nea_t *nea)
{
  if (nea == NULL)
    return;

  if (nea->nea_oreq)
    nta_outgoing_destroy(nea->nea_oreq), nea->nea_oreq = NULL;

  if (nea->nea_leg)
    nta_leg_destroy(nea->nea_leg), nea->nea_leg = NULL;

  if (nea->nea_timer) {
    su_timer_reset(nea->nea_timer);
    su_timer_destroy(nea->nea_timer), nea->nea_timer = NULL;
  }

  su_free(NULL, nea);
}


/* Function called by NTA to handle incoming requests belonging to the leg */
int process_nea_request(nea_t *nea,
			nta_leg_t *leg,
			nta_incoming_t *ireq,
			sip_t const *sip)
{

  switch (sip->sip_request->rq_method) {
  case sip_method_notify:
    return handle_notify(nea, leg, ireq, sip);
  case sip_method_ack:
    return 400;
  default:
    nta_incoming_treply(ireq, SIP_405_METHOD_NOT_ALLOWED,
			SIPTAG_ALLOW_STR("NOTIFY"), TAG_END());
    return 405;
  }
}


/* Callback function to handle subscription requests */
int response_to_subscribe(nea_t *nea,
			  nta_outgoing_t *oreq,
			  sip_t const *sip)
{
  int status = sip->sip_status->st_status;
  int error = status >= 300;

  if (status >= 200 && oreq == nea->nea_oreq)
    nea->nea_oreq = NULL;

  nea->nea_callback(nea, nea->nea_context, sip);

  if (status < 200)
    return 0;

  nea->nea_oreq = NULL;

  if (status < 300) {
    sip_time_t now = sip_now();
    if (!nea->nea_notify_received) {
      nea->nea_deadline = now +
	sip_contact_expires(NULL, sip->sip_expires, sip->sip_date,
			    EXPIRES_DEFAULT, now);
      if (sip->sip_to->a_tag && !nea->nea_dialog) {
	nea->nea_dialog = 1;
	nta_leg_rtag(nea->nea_leg, sip->sip_to->a_tag);
	nta_leg_client_route(nea->nea_leg,
			     sip->sip_record_route, sip->sip_contact);
      }
    }
  }
  else {
    nea->nea_deadline = 0;
    nea->nea_state = nea_terminated;
    if (status == 301 || status == 302 || status == 305) {
      sip_contact_t *m;

      for (m = sip->sip_contact; m; m = m->m_next) {
	if (m->m_url->url_type == url_sip ||
	    m->m_url->url_type == url_sips)
	  break;
      }

      if (m) {
	url_string_t const *proxy, *url;
	if (status == 305)
	  url = NULL, proxy = (url_string_t *)m->m_url;
	else
	  url = (url_string_t *)m->m_url, proxy = NULL;

	nea->nea_oreq =
	  nta_outgoing_tcreate(nea->nea_leg,
			       response_to_subscribe,
			       nea,
			       proxy,
			       SIP_METHOD_SUBSCRIBE,
			       url,
			       SIPTAG_EXPIRES(nea->nea_expires),
			       TAG_NEXT(nea->nea_args));
      }
    } else if (status == 423 && sip->sip_min_expires) {
      unsigned value = sip->sip_min_expires->me_delta;
      su_free(nea->nea_home, nea->nea_expires);
      nea->nea_expires = sip_expires_format(nea->nea_home, "%u", value);

      nea->nea_oreq =
	nta_outgoing_tcreate(nea->nea_leg,
			     response_to_subscribe,
			     nea,
			     NULL,
			     SIP_METHOD_SUBSCRIBE,
			     NULL,
			     SIPTAG_EXPIRES(nea->nea_expires),
			     TAG_NEXT(nea->nea_args));
    }
  }

  if (status >= 200)
    nta_outgoing_destroy(oreq);

  if (nea->nea_oreq || !error) {
    su_time_t now = su_now();
    now.tv_sec = nea->nea_deadline;
    su_timer_set_at(nea->nea_timer,
		    nea_expires_renew,
		    nea,
		    now);
  }
  else
    nea->nea_callback(nea, nea->nea_context, NULL);

  return 0;
}


int response_to_unsubscribe(nea_t *nea,
			    nta_outgoing_t *orq,
			    sip_t const *sip)
{
  int status = sip->sip_status->st_status;

  nea->nea_callback(nea, nea->nea_context, sip);

  if (status >= 200)
    nta_outgoing_destroy(orq), nea->nea_oreq = NULL;
  if (status >= 300)
    nea->nea_callback(nea, nea->nea_context, NULL);

  return 0;
}

/** handle notifications */
int handle_notify(nea_t *nea,
		  nta_leg_t *leg,
		  nta_incoming_t *irq,
		  sip_t const *sip)
{
  sip_subscription_state_t *ss = sip->sip_subscription_state;
  sip_subscription_state_t ss0[1];
  char expires[32];

  if (nea->nea_strict_3265) {
    char const *phrase = NULL;

    if (ss == NULL)
      phrase = "NOTIFY Has No Subscription-State Header";
    else if (sip->sip_event == NULL)
      phrase = "Event Header Missing";

    if (phrase) {
      nta_incoming_treply(irq, 400, phrase, TAG_END());
      nta_incoming_destroy(irq);
      nta_leg_destroy(nea->nea_leg), nea->nea_leg = NULL;
      nea->nea_state = nea_terminated;
      nea->nea_callback(nea, nea->nea_context, NULL);
      return 0;
    }
  }

  if (ss == NULL) {
    /* Do some compatibility stuff here */
    unsigned long delta = 3600;

    sip_subscription_state_init(ss = ss0);

    if (sip->sip_expires)
      delta = sip->sip_expires->ex_delta;

    if (delta == 0)
      ss->ss_substate = "terminated";
    else
      ss->ss_substate = "active";

    if (delta > 0) {
      snprintf(expires, sizeof expires, "%lu", delta);
      ss->ss_expires = expires;
    }
  }

  if (!nea->nea_dialog) {
    nea->nea_dialog = 1;
    nta_leg_rtag(nea->nea_leg, sip->sip_from->a_tag);
    nta_leg_server_route(nea->nea_leg,
			 sip->sip_record_route, sip->sip_contact);
  }

  nea->nea_notify_received = 1;
  nea->nea_callback(nea, nea->nea_context, sip);

  if (su_casematch(ss->ss_substate, "terminated")) {
    nta_leg_destroy(nea->nea_leg), nea->nea_leg = NULL;
    nea->nea_state = nea_terminated;

    if (su_casematch(ss->ss_reason, "deactivated")) {
      nea->nea_state = nea_embryonic;
      nea->nea_deadline = sip_now();
    } else if (su_casematch(ss->ss_reason, "probation")) {
      sip_time_t retry = sip_now() + NEA_TIMER_DELTA;

      if (ss->ss_retry_after)
	retry += strtoul(ss->ss_retry_after, NULL, 10);
      else
	retry += NEA_TIMER_DELTA;

      nea->nea_state = nea_embryonic;
      nea->nea_deadline = retry;
    } else {
      nea->nea_deadline = 0;
      nea->nea_callback(nea, nea->nea_context, NULL);
      return 200;
    }
  }
  else if (su_casematch(ss->ss_substate, "pending"))
    nea->nea_state = nea_pending;
  else if (su_casematch(ss->ss_substate, "active"))
    nea->nea_state = nea_active;
  else
    nea->nea_state = nea_extended;

  if (nea->nea_state != nea_embryonic && ss->ss_expires) {
    unsigned retry = strtoul(ss->ss_expires, NULL, 10);
    if (retry > 60) retry -= 30; else retry /= 2;
    nea->nea_deadline = sip_now() + retry;
  }

  {
    su_time_t now = su_now();
    now.tv_sec = nea->nea_deadline;
    su_timer_set_at(nea->nea_timer,
		    nea_expires_renew,
		    nea,
		    now);
  }

  return 200;
}

void nea_expires_renew(su_root_magic_t *magic,
		       su_timer_t *timer,
		       nea_t *nea)
{
  sip_time_t now = sip_now();

  /* re-subscribe if expires soon */
  if (nea->nea_state == nea_terminated ||
      nea->nea_deadline == 0 ||
      nea->nea_deadline > now + NEA_TIMER_DELTA)
    return;

  if (!nea->nea_notify_received)	/* Hmph. */
    return;

  nea->nea_notify_received = 0;

  nea->nea_oreq =
    nta_outgoing_tcreate(nea->nea_leg,
			 response_to_subscribe,
			 nea,
			 NULL,
			 SIP_METHOD_SUBSCRIBE,
			 NULL,
			 SIPTAG_EXPIRES(nea->nea_expires),
			 TAG_NEXT(nea->nea_args));

  return;
}
