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

/**@CFILE nua_event_server.c
 * @brief Easy event server
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Mar  8 11:48:49 EET 2006 ppessi
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
#include <sofia-sip/su_tagarg.h>

#define NEA_SMAGIC_T         struct nua_handle_s
#define NEA_EMAGIC_T         struct nua_handle_s

#include "nua_stack.h"

/* ======================================================================== */
/* Event server */

static
nea_event_t *nh_notifier_event(nua_handle_t *nh,
			       su_home_t *home,
			       sip_event_t const *event,
			       tagi_t const *tags);

static
void authorize_watcher(nea_server_t *nes,
		       nua_handle_t *nh,
		       nea_event_t *ev,
		       nea_subnode_t *sn,
		       sip_t const *sip);

void
nua_stack_notifier(nua_t *nua, nua_handle_t *nh, nua_event_t e, tagi_t const *tags)
{
  su_home_t home[1] = { SU_HOME_INIT(home) };
  sip_event_t const *event = NULL;
  sip_content_type_t const *ct = NULL;
  sip_payload_t const *pl = NULL;
  url_string_t const *url = NULL;
  char const *event_s = NULL, *ct_s = NULL, *pl_s = NULL;
  nea_event_t *ev;
  int status = 900;
  char const *phrase = nua_internal_error;

  nua_stack_init_handle(nua, nh, tags);

  tl_gets(tags,
	  NUTAG_URL_REF(url),
	  SIPTAG_EVENT_REF(event),
	  SIPTAG_EVENT_STR_REF(event_s),
	  SIPTAG_CONTENT_TYPE_STR_REF(ct_s),
	  SIPTAG_PAYLOAD_REF(pl),
	  SIPTAG_PAYLOAD_STR_REF(pl_s),
	  TAG_END());

  if (!event && !event_s)
    status = 400, phrase = "Missing Event";

  else if (!ct && !ct_s)
    status = 400, phrase = "Missing Content-Type";

  else if (!nh->nh_notifier &&
	   !(nh->nh_notifier =
	     nea_server_create(nua->nua_nta, nua->nua_root,
			       url->us_url,
			       NH_PGET(nh, max_subscriptions),
			       NULL, nh,
			       TAG_NEXT(tags))))
    status = 900, phrase = nua_internal_error;

  else if (!event && !(event = sip_event_make(home, event_s)))
    status = 900, phrase = "Could not create an event header";

  else if (!(ev = nh_notifier_event(nh, home, event, tags)))
    status = 900, phrase = "Could not create an event view";

  else if (nea_server_update(nh->nh_notifier, ev,  TAG_NEXT(tags)) < 0)
    status = 900, phrase = "No content for event";

  else if (nea_server_notify(nh->nh_notifier, ev) < 0)
    status = 900, phrase = "Error when notifying watchers";

  else
    nua_stack_tevent(nua, nh, NULL, e, status = SIP_200_OK,
		     SIPTAG_EVENT(event),
		     SIPTAG_CONTENT_TYPE(ct),
		     TAG_END());

  if (status != 200)
    nua_stack_event(nua, nh, NULL, e, status, phrase, NULL);

  su_home_deinit(home);
}


/* Create a event view for notifier */
static
nea_event_t *nh_notifier_event(nua_handle_t *nh,
			       su_home_t *home,
			       sip_event_t const *event,
			       tagi_t const *tags)
{
  nea_event_t *ev = nea_event_get(nh->nh_notifier, event->o_type);
  sip_accept_t const *accept = NULL;
  char const  *accept_s = NULL;
  sip_content_type_t const *ct = NULL;
  char const *ct_s = NULL;

  if (ev == NULL) {
    char *o_type, *o_subtype;
    char *temp = NULL;

    o_type = su_strdup(home, event->o_type);
    if (o_type == NULL)
      return NULL;
    o_subtype = strchr(o_type, '.');
    if (o_subtype)
      *o_subtype++ = '\0';

    tl_gets(tags,
	    SIPTAG_ACCEPT_REF(accept),
	    SIPTAG_ACCEPT_STR_REF(accept_s),
	    SIPTAG_CONTENT_TYPE_REF(ct),
	    SIPTAG_CONTENT_TYPE_STR_REF(ct_s),
	    TAG_END());

    /*
     * XXX - We really should build accept header when we add new content
     * types
     */
    if (accept_s == NULL && accept)
      accept_s = temp = sip_header_as_string(home, (sip_header_t *)accept);
    if (accept_s == NULL && ct)
      accept_s = ct->c_type;
    if (accept_s == NULL && ct_s)
      accept_s = ct_s;

    ev = nea_event_create(nh->nh_notifier,
			  authorize_watcher, nh,
			  o_type, o_subtype,
			  ct ? ct->c_type : ct_s,
			  accept_s);

    su_free(home, temp);
    su_free(home, o_type);
  }

  return ev;
}

/* Callback from nea_server asking nua to authorize subscription */
static
void authorize_watcher(nea_server_t *nes,
		       nua_handle_t *nh,
		       nea_event_t *ev,
		       nea_subnode_t *sn,
		       sip_t const *sip)
{
  nua_t *nua = nh->nh_nua;
  msg_t *msg = NULL;
  nta_incoming_t *irq = NULL;
  int substate = sn->sn_state;
  int status; char const *phrase;

  SET_STATUS1(SIP_200_OK);

  /* OK. In nhp (nua_handle_preferences_t) structure we have the
     current default action (or state) for incoming
     subscriptions.
     Action can now be modified by the application with NUTAG_SUBSTATE().
  */
  irq = nea_sub_get_request(sn->sn_subscriber);
  msg = nta_incoming_getrequest(irq);

  if (sn->sn_state == nea_embryonic) {
    char const *what;

    substate = NH_PGET(nh, substate);

    if (substate == nua_substate_embryonic)
      substate = nua_substate_pending;

    if (substate == nua_substate_terminated) {
      what = "rejected"; SET_STATUS1(SIP_403_FORBIDDEN);
    }
    else if (substate == nua_substate_pending) {
      what = "pending"; SET_STATUS1(SIP_202_ACCEPTED);
    }
    else {
      what = "active";
    }

    SU_DEBUG_7(("nua(%p): authorize_watcher: %s\n", (void *)nh, what));
    nea_sub_auth(sn->sn_subscriber, (nea_state_t)substate,
		 TAG_IF(substate == nua_substate_pending,
			NEATAG_FAKE(1)),
		 TAG_IF(substate == nua_substate_terminated,
			NEATAG_REASON("rejected")),
		 TAG_END());
  }
  else if (sn->sn_state == nea_terminated || sn->sn_expires == 0) {
    substate = nua_substate_terminated;
    nea_server_flush(nes, NULL);
    SU_DEBUG_7(("nua(%p): authorize_watcher: %s\n",
		(void *)nh, "watcher is removed"));
  }

  nua_stack_tevent(nua, nh, msg, nua_i_subscription, status, phrase,
		   NUTAG_SUBSTATE(substate),
		   NEATAG_SUB(sn->sn_subscriber),
		   TAG_END());
}

/* ---------------------------------------------------------------------- */
/* Authorization of watchers by application */

void nua_stack_authorize(nua_t *nua,
			 nua_handle_t *nh,
			 nua_event_t e,
			 tagi_t const *tags)
{
  nea_sub_t *sub = NULL;
  int state = nea_extended;

  tl_gets(tags,
	  NEATAG_SUB_REF(sub),
	  NUTAG_SUBSTATE_REF(state),
	  TAG_END());

  if (sub && state > 0) {
    nea_sub_auth(sub, (nea_state_t)state, TAG_NEXT(tags));
    nua_stack_event(nua, nh, NULL, e, SIP_200_OK, NULL);
  }
  else {
    nua_stack_event(nua, nh, NULL, e, NUA_ERROR_AT(__FILE__, __LINE__), NULL);
  }
}

/** @internal Shutdown notifier object */
int nh_notifier_shutdown(nua_handle_t *nh,
			 nea_event_t *ev,
			 tag_type_t t,
			 tag_value_t v, ...)
{
  nea_server_t *nes = nh->nh_notifier;
  nea_subnode_t const **subs;
  int busy = 0;

  if (nes == NULL)
    return 0;

  subs = nea_server_get_subscribers(nes, ev);

  if (subs) {
    int i;
    ta_list ta;

    ta_start(ta, t, v);

    for (i = 0; subs[i]; i++)
      nea_sub_auth(subs[i]->sn_subscriber, nea_terminated, ta_tags(ta));

    ta_end(ta);

    busy++;
  }

  nea_server_free_subscribers(nes, subs);

  nea_server_flush(nh->nh_notifier, NULL);

  if (ev == NULL)
    nea_server_destroy(nh->nh_notifier), nh->nh_notifier = NULL;

  return busy;
}


/** @internal Terminate notifier. */
void nua_stack_terminate(nua_t *nua,
			 nua_handle_t *nh,
			 nua_event_t e,
			 tagi_t const *tags)
{
  sip_event_t const *event = NULL;
  sip_content_type_t const *ct = NULL;
  sip_payload_t const *pl = NULL;
  char const *event_s = NULL, *ct_s = NULL, *pl_s = NULL;
  nea_event_t *nev = NULL;

  if (nh->nh_notifier == NULL) {
    UA_EVENT2(e, 900, "No event server to terminate");
    return;
  }

  tl_gets(tags,
	  SIPTAG_EVENT_REF(event),
	  SIPTAG_EVENT_STR_REF(event_s),
	  SIPTAG_CONTENT_TYPE_REF(ct),
	  SIPTAG_CONTENT_TYPE_STR_REF(ct_s),
	  SIPTAG_PAYLOAD_REF(pl),
	  SIPTAG_PAYLOAD_STR_REF(pl_s),
	  TAG_END());

  nev = nea_event_get(nh->nh_notifier,
		      event ? event->o_type : event_s);

  if (nev && (pl || pl_s) && (ct || ct_s)) {
    nea_server_update(nh->nh_notifier, nev, TAG_NEXT(tags));
  }

  nh_notifier_shutdown(nh, NULL,
		       NEATAG_REASON("noresource"),
		       TAG_NEXT(tags));

  nua_stack_event(nua, nh, NULL, e, SIP_200_OK, NULL);
}
