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

/**@CFILE nua_stack.c
 * @brief Sofia-SIP User Agent Engine implementation
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <Kai.Vehmanen@nokia.com>
 * @author Martti Mela <Martti Mela@nokia.com>
 * @author Remeres Jacobs <Remeres.Jacobs@nokia.com>
 * @author Tat Chan <Tat.Chan@nokia.com>
 *
 * @date Created: Wed Feb 14 18:32:58 2001 ppessi
 */

#include "config.h"

#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_strlst.h>
#include <sofia-sip/su_uniqueid.h>

#include <sofia-sip/su_tag_io.h>

#define SU_ROOT_MAGIC_T   struct nua_s
#define SU_MSG_ARG_T      struct event_s

#define NUA_SAVED_EVENT_T su_msg_t *

#define NTA_AGENT_MAGIC_T    struct nua_s
#define NTA_LEG_MAGIC_T      struct nua_handle_s
#define NTA_OUTGOING_MAGIC_T struct nua_handle_s
#define NTA_INCOMING_MAGIC_T struct nua_handle_s

#include <sofia-sip/sip.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sip_util.h>

#include <sofia-sip/tport_tag.h>
#include <sofia-sip/nta.h>
#include <sofia-sip/nta_tport.h>
#include <sofia-sip/auth_client.h>

#include <sofia-sip/soa.h>

#include "sofia-sip/nua.h"
#include "sofia-sip/nua_tag.h"
#include "nua_stack.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#include <assert.h>

/* ========================================================================
 *
 *                       Protocol stack side
 *
 * ======================================================================== */

nua_handle_t *nh_create(nua_t *nua, tag_type_t t, tag_value_t v, ...);
static void nh_append(nua_t *nua, nua_handle_t *nh);
static void nh_remove(nua_t *nua, nua_handle_t *nh);

static int nh_authorize(nua_handle_t *nh,
			tag_type_t tag, tag_value_t value, ...);

static int nh_challenge(nua_handle_t *nh, sip_t const *sip);

static void nua_stack_timer(nua_t *nua, su_timer_t *t, su_timer_arg_t *a);

/* ---------------------------------------------------------------------- */
/* Constant data */

/**@internal Default internal error. */
char const nua_internal_error[] = "Internal NUA Error";

char const nua_application_sdp[] = "application/sdp";

#define NUA_STACK_TIMER_INTERVAL (1000)

/* ----------------------------------------------------------------------
 * Initialization & deinitialization
 */

int nua_stack_init(su_root_t *root, nua_t *nua)
{
  su_home_t *home;
  nua_handle_t *dnh;

  static int initialized_logs = 0;

  enter;

  if (!initialized_logs) {
    extern su_log_t tport_log[];
    extern su_log_t nta_log[];
    extern su_log_t nea_log[];
    extern su_log_t iptsec_log[];

    su_log_init(tport_log);
    su_log_init(nta_log);
    su_log_init(nea_log);
    su_log_init(iptsec_log);

    initialized_logs = 1;
  }

  nua->nua_root = root;
  nua->nua_timer = su_timer_create(su_root_task(root),
				   NUA_STACK_TIMER_INTERVAL);
  if (!nua->nua_timer)
    return -1;

  home = nua->nua_home;
  nua->nua_handles_tail = &nua->nua_handles;
  sip_from_init(nua->nua_from);

  dnh = su_home_clone(nua->nua_home, sizeof (*dnh) + sizeof(*dnh->nh_prefs));
  if (!dnh)
    return -1;

  dnh->nh_prefs = (void *)(dnh + 1);
  dnh->nh_valid = nua_handle;
  dnh->nh_nua = nua;
  nua_handle_ref(dnh); dnh->nh_ref_by_stack = 1; 
  nua_handle_ref(dnh); dnh->nh_ref_by_user = 1;
  nh_append(nua, dnh);
  dnh->nh_identity = dnh;
  dnh->nh_ds->ds_local = nua->nua_from;
  dnh->nh_ds->ds_remote = nua->nua_from;

  if (nua_stack_set_defaults(dnh, dnh->nh_prefs) < 0)
    return -1;

  if (nua_stack_set_params(nua, dnh, nua_i_none, nua->nua_args) < 0)
    return -1;

  nua->nua_invite_accept = sip_accept_make(home, SDP_MIME_TYPE);

  nua->nua_nta = nta_agent_create(root, NONE, NULL, NULL,
				  NTATAG_MERGE_482(1),
				  NTATAG_CLIENT_RPORT(1),
				  NTATAG_UA(1),
#if HAVE_SOFIA_SMIME
				  NTATAG_SMIME(nua->sm),
#endif
				  TPTAG_STUN_SERVER(1),
				  TAG_NEXT(nua->nua_args));

  dnh->nh_ds->ds_leg = nta_leg_tcreate(nua->nua_nta,
				       nua_stack_process_request, dnh,
				       NTATAG_NO_DIALOG(1),
				       TAG_END());

  if (nua->nua_nta == NULL ||
      dnh->nh_ds->ds_leg == NULL || 
      nta_agent_set_params(nua->nua_nta, NTATAG_UA(1), TAG_END()) < 0 ||
      nua_stack_init_transport(nua, nua->nua_args) < 0) {
    SU_DEBUG_1(("nua: initializing SIP stack failed\n"));
    return -1;
  }

  if (nua_stack_set_from(nua, 1, nua->nua_args) < 0)
    return -1;

  if (NHP_ISSET(dnh->nh_prefs, detect_network_updates))
    nua_stack_launch_network_change_detector(nua);

  nua_stack_timer(nua, nua->nua_timer, NULL);

  return 0;
}

void nua_stack_deinit(su_root_t *root, nua_t *nua)
{
  enter;

  su_timer_destroy(nua->nua_timer), nua->nua_timer = NULL;
  nta_agent_destroy(nua->nua_nta), nua->nua_nta = NULL;
}

/* ----------------------------------------------------------------------
 * Sending events to client application
 */

static void nua_stack_shutdown(nua_t *);

void
  nua_stack_authenticate(nua_t *, nua_handle_t *, nua_event_t, tagi_t const *),
  nua_stack_respond(nua_t *, nua_handle_t *, int , char const *, tagi_t const *),
  nua_stack_destroy_handle(nua_t *, nua_handle_t *, tagi_t const *);

/* Notifier */
void
  nua_stack_authorize(nua_t *, nua_handle_t *, nua_event_t, tagi_t const *),
  nua_stack_notifier(nua_t *, nua_handle_t *, nua_event_t, tagi_t const *),
  nua_stack_terminate(nua_t *, nua_handle_t *, nua_event_t, tagi_t const *);

int nh_notifier_shutdown(nua_handle_t *nh, nea_event_t *ev,
			 tag_type_t t, tag_value_t v, ...);

/** @internal Send an event to the application. */
int nua_stack_event(nua_t *nua, nua_handle_t *nh, msg_t *msg,
		    nua_event_t event, int status, char const *phrase,
		    tag_type_t tag, tag_value_t value, ...)
{
  su_msg_r sumsg = SU_MSG_R_INIT;

  ta_list ta;
  size_t e_len, len, xtra, p_len;

  if (event == nua_r_ack || event == nua_i_none)
    return event;

  enter;

  if (nua_log->log_level >= 5) {
    char const *name = nua_event_name(event) + 4;
    char const *p = phrase ? phrase : "";

    if (status == 0)
      SU_DEBUG_5(("nua(%p): %s %s\n", nh, name, p));
    else
      SU_DEBUG_5(("nua(%p): %s %u %s\n", nh, name, status, p));
  }

  if (event == nua_r_destroy) {
    if (msg)
      msg_destroy(msg);
    if (status >= 200) {
      nh_destroy(nua, nh);
    }
    return event;
  }

  if ((event > nua_r_authenticate && event <= nua_r_ack)
      || event < nua_i_error
      || (nh && !nh->nh_valid)
      || (nua->nua_shutdown && event != nua_r_shutdown)) {
    if (msg)
      msg_destroy(msg);
    return event;
  }

  ta_start(ta, tag, value);

  e_len = offsetof(event_t, e_tags);
  len = tl_len(ta_args(ta));
  xtra = tl_xtra(ta_args(ta), len);
  p_len = phrase ? strlen(phrase) + 1 : 1;

  if (su_msg_create(sumsg, nua->nua_client, su_task_null,
		    nua_event, e_len + len + xtra + p_len) == 0) {
    event_t *e = su_msg_data(sumsg);

    tagi_t *t = e->e_tags, *t_end = (tagi_t *)((char *)t + len);
    void *b = t_end, *end = (char *)b + xtra;

    t = tl_dup(t, ta_args(ta), &b);
    assert(t == t_end); assert(b == end);

    e->e_event = event;
    e->e_nh = nh ? nua_handle_ref(nh) : nua->nua_dhandle;
    e->e_status = status;
    e->e_phrase = strcpy(end, phrase ? phrase : "");
    if (msg)
      e->e_msg = msg, su_home_threadsafe(msg_home(msg));

    if (su_msg_send(sumsg) != 0)
      nua_handle_unref(nh);
  }

  ta_end(ta);

  return event;
}

/* ----------------------------------------------------------------------
 * Post signal to stack itself
 */
void nua_stack_post_signal(nua_handle_t *nh, nua_event_t event,
			   tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  ta_start(ta, tag, value);
  nua_signal((nh)->nh_nua, nh, NULL, 1, event, 0, NULL, ta_tags(ta));
  ta_end(ta);
}


/* ----------------------------------------------------------------------
 * Receiving events from client
 */
void nua_stack_signal(nua_t *nua, su_msg_r msg, nua_event_data_t *e)
{
  nua_handle_t *nh = e->e_nh;
  tagi_t *tags = e->e_tags;

  assert(tags);

  if (nh) {
    if (!nh->nh_prev)
      nh_append(nua, nh);
    if (!nh->nh_ref_by_stack) {
      /* Mark handle as used by stack */
      nh->nh_ref_by_stack = 1;
      nua_handle_ref(nh);
    }
  }

  if (nua_log->log_level >= 5) {
    char const *name = nua_event_name(e->e_event);
    if (e->e_status == 0)
      SU_DEBUG_5(("nua(%p): signal %s\n", nh, name + 4));
    else
      SU_DEBUG_5(("nua(%p): signal %s %u %s\n",
		  nh, name + 4, e->e_status, e->e_phrase ? e->e_phrase : ""));
  }

  su_msg_save(nua->nua_signal, msg);

  if (nua->nua_shutdown && !e->e_always) {
    /* Shutting down */
    nua_stack_event(nua, nh, NULL, e->e_event,
		    901, "Stack is going down",
		    TAG_END());
  }

  else switch (e->e_event) {
  case nua_r_get_params:
    nua_stack_get_params(nua, nh ? nh : nua->nua_dhandle, e->e_event, tags);
    break;
  case nua_r_set_params:
    nua_stack_set_params(nua, nh ? nh : nua->nua_dhandle, e->e_event, tags);
    break;
  case nua_r_shutdown:
    nua_stack_shutdown(nua);
    break;
  case nua_r_register:
  case nua_r_unregister:
    nua_stack_register(nua, nh, e->e_event, tags);
    break;
  case nua_r_invite:
    nua_stack_invite(nua, nh, e->e_event, tags);
    break;
  case nua_r_cancel:
    nua_stack_cancel(nua, nh, e->e_event, tags);
    break;
  case nua_r_bye:
    nua_stack_bye(nua, nh, e->e_event, tags);
    break;
  case nua_r_options:
    nua_stack_options(nua, nh, e->e_event, tags);
    break;
  case nua_r_refer:
    nua_stack_refer(nua, nh, e->e_event, tags);
    break;
  case nua_r_publish:
  case nua_r_unpublish:
    nua_stack_publish(nua, nh, e->e_event, tags);
    break;
  case nua_r_info:
    nua_stack_info(nua, nh, e->e_event, tags);
    break;
  case nua_r_update:
    nua_stack_update(nua, nh, e->e_event, tags);
    break;
  case nua_r_message:
    nua_stack_message(nua, nh, e->e_event, tags);
    break;
  case nua_r_subscribe:
  case nua_r_unsubscribe:
    nua_stack_subscribe(nua, nh, e->e_event, tags);
    break;
  case nua_r_notify:
    nua_stack_notify(nua, nh, e->e_event, tags);
    break;
  case nua_r_notifier:
    nua_stack_notifier(nua, nh, e->e_event, tags);
    break;
  case nua_r_terminate:
    nua_stack_terminate(nua, nh, e->e_event, tags);
    break;
  case nua_r_method:
    nua_stack_method(nua, nh, e->e_event, tags);
    break;
  case nua_r_authenticate:
    nua_stack_authenticate(nua, nh, e->e_event, tags);
    break;
  case nua_r_authorize:
    nua_stack_authorize(nua, nh, e->e_event, tags);
    break;
  case nua_r_ack:
    nua_stack_ack(nua, nh, e->e_event, tags);
    break;
  case nua_r_respond:
    nua_stack_respond(nua, nh, e->e_status, e->e_phrase, tags);
    break;
  case nua_r_destroy:
    nua_stack_destroy_handle(nua, nh, tags);
    break;
  default:
    break;
  }

  if (su_msg_is_non_null(nua->nua_signal))
    su_msg_destroy(nua->nua_signal);

  if (nh != nua->nua_dhandle)
    nua_handle_unref(nh);
}

/* ====================================================================== */

static int nh_call_pending(nua_handle_t *nh, sip_time_t time);

/**@internal
 * Timer routine.
 *
 * Go through all active handles and execute pending tasks
 */
void nua_stack_timer(nua_t *nua, su_timer_t *t, su_timer_arg_t *a)
{
  nua_handle_t *nh, *nh_next;
  sip_time_t now = sip_now();
  su_root_t *root = su_timer_root(t);

  su_timer_set(t, nua_stack_timer, a);

  if (nua->nua_shutdown) {
    nua_stack_shutdown(nua);
    return;
  }

  for (nh = nua->nua_handles; nh; nh = nh_next) {
    nh_next = nh->nh_next;
    nh_call_pending(nh, now);
    su_root_yield(root);	/* Handle received packets */
  }
}


static
int nh_call_pending(nua_handle_t *nh, sip_time_t now)
{
  nua_dialog_state_t *ds = nh->nh_ds;
  nua_dialog_usage_t *du;
  sip_time_t next = now + NUA_STACK_TIMER_INTERVAL / 1000;

  for (du = ds->ds_usage; du; du = du->du_next) {
    if (now == 0)
      break;
    if (du->du_refresh && du->du_refresh < next)
      break;
  }

  if (du == NULL)
    return 0;

  nua_handle_ref(nh);

  while (du) {
    nua_dialog_usage_t *du_next = du->du_next;

    nua_dialog_usage_refresh(nh, ds, du, now);

    if (du_next == NULL)
      break;

    for (du = nh->nh_ds->ds_usage; du; du = du->du_next)
      if (du == du_next)
	break;

    for (; du; du = du->du_next) {
      if (now == 0)
	break;
      if (du->du_refresh && du->du_refresh < next)
	break;
    }
  }

  nua_handle_unref(nh);

  return 1;
}


/* ====================================================================== */

/**Shutdown a @nua stack.
 *
 * When the @nua stack is shutdown, ongoing calls are released,
 * registrations unregistered, publications un-PUBLISHed and subscriptions
 * terminated. If the stack cannot terminate everything within 30 seconds,
 * it sends the #nua_r_shutdown event with status 500.
 *
 * @param nua         Pointer to @nua stack object
 *
 * @return
 *     nothing
 *
 * @par Related tags:
 *     none
 *
 * @par Events:
 *     #nua_r_shutdown
 *
 * @sa #nua_r_shutdown, nua_destroy(), nua_create(), nua_bye(),
 * nua_unregister(), nua_unpublish(), nua_unsubscribe(), nua_notify(),
 * nua_handle_destroy(), nua_handle_unref()
 */

/** @NUA_EVENT nua_r_shutdown
 *
 * Answer to nua_shutdown().
 *
 * Status codes
 * - 100 shutdown started
 * - 101 shutdown in progress (sent when shutdown has been progressed)
 * - 200 shutdown was successful
 * - 500 shutdown timeout after 30 sec
 *
 * @param status shutdown status code
 * @param nh     NULL
 * @param hmagic NULL
 * @param sip    NULL
 * @param tags   empty
 *
 * @sa nua_shutdown(), nua_destroy()
 *
 * @END_NUA_EVENT
 */

/** @internal Shut down stack. */
void nua_stack_shutdown(nua_t *nua)
{
  nua_handle_t *nh, *nh_next;
  int busy = 0;
  sip_time_t now = sip_now();
  int status;
  char const *phrase;

  enter;

  if (!nua->nua_shutdown)
    nua->nua_shutdown = now;

  for (nh = nua->nua_handles; nh; nh = nh_next) {
    nua_dialog_state_t *ds = nh->nh_ds;
    nua_server_request_t *sr, *sr_next;

    nh_next = nh->nh_next;

    for (sr = ds->ds_sr; sr; sr = sr_next) {
      sr_next = sr->sr_next;

      if (sr->sr_respond) {
	SR_STATUS1(sr, SIP_410_GONE);
	sr->sr_usage = NULL;
	sr->sr_respond(sr, NULL);
	sr->sr_respond = NULL;
	busy++;
      }
	
      nua_server_request_destroy(sr);
    }

    busy += nh_call_pending(nh, 0);

    if (nh->nh_soa) {
      soa_destroy(nh->nh_soa), nh->nh_soa = NULL;
    }

    if (nua_client_request_pending(ds->ds_cr))
      busy++;

    if (nh_notifier_shutdown(nh, NULL, NEATAG_REASON("noresource"), TAG_END()))
      busy++;
  }

  if (!busy)
    SET_STATUS(200, "Shutdown successful");
  else if (now == nua->nua_shutdown)
    SET_STATUS(100, "Shutdown started");
  else if (now - nua->nua_shutdown < 30)
    SET_STATUS(101, "Shutdown in progress");
  else
    SET_STATUS(500, "Shutdown timeout");

  if (status >= 200) {
    for (nh = nua->nua_handles; nh; nh = nh_next) {
      nh_next = nh->nh_next;
      while (nh->nh_ds && nh->nh_ds->ds_usage) {
	nua_dialog_usage_remove(nh, nh->nh_ds, nh->nh_ds->ds_usage);
      }
    }
    su_timer_destroy(nua->nua_timer), nua->nua_timer = NULL;
    nta_agent_destroy(nua->nua_nta), nua->nua_nta = NULL;
  }

  nua_stack_event(nua, NULL, NULL, nua_r_shutdown, status, phrase, TAG_END());
}

/* ---------------------------------------------------------------------- */

/** @internal Create a handle */
nua_handle_t *nh_create(nua_t *nua, tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  nua_handle_t *nh;

  enter;

  ta_start(ta, tag, value);
  nh = nh_create_handle(nua, NULL, ta_args(ta));
  ta_end(ta);

  if (nh) {
    nh->nh_ref_by_stack = 1;
    nh_append(nua, nh);
  }

  return nh;
}

/** @internal Append an handle to the list of handles */
void nh_append(nua_t *nua, nua_handle_t *nh)
{
  nh->nh_next = NULL;
  nh->nh_prev = nua->nua_handles_tail;
  *nua->nua_handles_tail = nh;
  nua->nua_handles_tail = &nh->nh_next;
}

nua_handle_t *nh_validate(nua_t *nua, nua_handle_t *maybe)
{
  nua_handle_t *nh;

  if (maybe)
    for (nh = nua->nua_handles; nh; nh = nh->nh_next)
      if (nh == maybe)
	return nh;

  return NULL;
}

void nua_stack_destroy_handle(nua_t *nua, nua_handle_t *nh, tagi_t const *tags)
{
  nh_call_pending(nh, 0);	/* Call pending operations with 0 */

  if (nh->nh_notifier)
    nua_stack_terminate(nua, nh, 0, NULL);

#if 0
  if (nh->nh_ref_by_user) {
    nh->nh_ref_by_user = 0;
    nua_handle_unref(nh);
  }
#endif

  nh_destroy(nua, nh);
}

#define nh_is_inserted(nh) ((nh)->nh_prev != NULL)

/** @internal Remove a handle from list of handles */
static
void nh_remove(nua_t *nua, nua_handle_t *nh)
{
  assert(nh_is_inserted(nh)); assert(*nh->nh_prev == nh);

  if (nh->nh_next)
    nh->nh_next->nh_prev = nh->nh_prev;
  else
    nua->nua_handles_tail = nh->nh_prev;

  *nh->nh_prev = nh->nh_next;

  nh->nh_prev = NULL;
  nh->nh_next = NULL;
}


void nh_destroy(nua_t *nua, nua_handle_t *nh)
{
  assert(nh); assert(nh != nua->nua_dhandle);

  nh_enter;

  if (nh->nh_notifier)
    nea_server_destroy(nh->nh_notifier), nh->nh_notifier = NULL;

  nua_creq_deinit(nh->nh_ds->ds_cr, NULL);

  nua_dialog_deinit(nh, nh->nh_ds);

  if (nh->nh_soa)
    soa_destroy(nh->nh_soa), nh->nh_soa = NULL;

  if (nh_is_inserted(nh))
    nh_remove(nua, nh);

  nua_handle_unref(nh);		/* Remove stack reference */
}

/* ======================================================================== */

/**@internal
 * Initialize handle Allow and authentication info, save parameters.
 *
 * @retval -1 upon an error
 * @retval 0 when successful
 */
int nua_stack_init_handle(nua_t *nua, nua_handle_t *nh,
			  tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  int retval = 0;

  if (nh == NULL)
    return -1;

  assert(nh != nua->nua_dhandle);

  ta_start(ta, tag, value);

  if (nua_stack_set_params(nua, nh, nua_i_error, ta_args(ta)) < 0)
    retval = -1;

  if (!retval && nh->nh_soa)
    if (soa_set_params(nh->nh_soa, ta_tags(ta)) < 0)
      retval = -1;

  ta_end(ta);

  if (retval || nh->nh_init) /* Already initialized? */
    return retval;

  if (nh->nh_tags)
    nh_authorize(nh, TAG_NEXT(nh->nh_tags));

  nh->nh_init = 1;

  return 0;
}

/** @internal Create a handle for processing incoming request */
nua_handle_t *nua_stack_incoming_handle(nua_t *nua,
					nta_incoming_t *irq,
					sip_t const *sip,
					int create_dialog)
{
  nua_handle_t *nh;
  url_t const *url;
  sip_to_t to[1];
  sip_from_t from[1];

  assert(sip && sip->sip_from && sip->sip_to);

  if (sip->sip_contact)
    url = sip->sip_contact->m_url;
  else
    url = sip->sip_from->a_url;

  /* Strip away parameters */
  sip_from_init(from)->a_display = sip->sip_to->a_display;
  *from->a_url = *sip->sip_to->a_url;

  sip_to_init(to)->a_display = sip->sip_from->a_display;
  *to->a_url = *sip->sip_from->a_url;

  nh = nh_create(nua,
		 NUTAG_URL((url_string_t *)url), /* Remote target */
		 SIPTAG_TO(to), /* Local AoR */
		 SIPTAG_FROM(from), /* Remote AoR */
		 TAG_END());

  if (nua_stack_init_handle(nh->nh_nua, nh, TAG_END()) < 0)
    nh_destroy(nua, nh), nh = NULL;

  if (nh && create_dialog) {
    struct nua_dialog_state *ds = nh->nh_ds;

    nua_dialog_store_peer_info(nh, ds, sip);

    ds->ds_leg = nta_leg_tcreate(nua->nua_nta, nua_stack_process_request, nh,
				 SIPTAG_CALL_ID(sip->sip_call_id),
				 SIPTAG_FROM(sip->sip_to),
				 SIPTAG_TO(sip->sip_from),
				 NTATAG_REMOTE_CSEQ(sip->sip_cseq->cs_seq),
				 TAG_END());

    if (!ds->ds_leg || !nta_leg_tag(ds->ds_leg, nta_incoming_tag(irq, NULL)))
      nh_destroy(nua, nh), nh = NULL;
  }

  if (nh)
    nua_dialog_uas_route(nh, nh->nh_ds, sip, 1);

  return nh;
}


/** Set flags and special event on handle.
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 */
int nua_stack_set_handle_special(nua_handle_t *nh,
				 enum nh_kind kind,
				 nua_event_t special)
{
  if (nh == NULL)
    return -1;

  if (special && nh->nh_special && nh->nh_special != special)
    return -1;

  if (!nh_is_special(nh) && !nh->nh_has_invite) {
    switch (kind) {
    case nh_has_invite:    nh->nh_has_invite = 1;    break;
    case nh_has_subscribe: nh->nh_has_subscribe = 1; break;
    case nh_has_notify:    nh->nh_has_notify = 1;    break;
    case nh_has_register:  nh->nh_has_register = 1;  break;
    case nh_has_nothing:
    default:
      break;
    }

    if (special)
      nh->nh_special = special;
  }

  return 0;
}

sip_replaces_t *nua_stack_handle_make_replaces(nua_handle_t *nh, 
					       su_home_t *home,
					       int early_only)
{
  if (nh && nh->nh_ds && nh->nh_ds->ds_leg)
    return nta_leg_make_replaces(nh->nh_ds->ds_leg, home, early_only);
  else
    return NULL;
}

nua_handle_t *nua_stack_handle_by_replaces(nua_t *nua,
					   sip_replaces_t const *r)
{
  if (nua) {
    nta_leg_t *leg = nta_leg_by_replaces(nua->nua_nta, r);
    if (leg)
      return nta_leg_magic(leg, nua_stack_process_request);
  }
  return NULL;
}


/** @internal Add authorization data */
int nh_authorize(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  int retval = 0;
  tagi_t const *ti;
  ta_list ta;

  ta_start(ta, tag, value);

  for (ti = ta_args(ta); ti; ti = tl_next(ti)) {
    if (ti->t_tag == nutag_auth && ti->t_value) {
      char *data = (char *)ti->t_value;
      int rv = auc_credentials(&nh->nh_auth, nh->nh_home, data);

      if (rv > 0) {
	retval = 1;
      }
      else if (rv < 0) {
	retval = -1;
	break;
      }
    }
  }

  ta_end(ta);

  return retval;
}

/**@internal
 * Collect challenges from response.
 *
 * @return Number of updated challenges, 0 if no updates found.
 * @retval -1 upon error.
 */
static
int nh_challenge(nua_handle_t *nh, sip_t const *sip)
{
  int server = 0, proxy = 0;

  if (sip->sip_www_authenticate)
    server = auc_challenge(&nh->nh_auth, nh->nh_home,
			   sip->sip_www_authenticate,
			   sip_authorization_class);

  if (sip->sip_proxy_authenticate)
    proxy = auc_challenge(&nh->nh_auth, nh->nh_home,
			  sip->sip_proxy_authenticate,
			  sip_proxy_authorization_class);

  if (server < 0 || proxy < 0)
    return -1;

  return server + proxy;
}

#include <sofia-sip/su_tag_inline.h>

/** Check if tag list has contact */
int nua_tagis_have_contact_tag(tagi_t const *t)
{
  for (; t && t->t_tag; t = t_next(t))
    if (t->t_tag == siptag_contact ||
	t->t_tag == siptag_contact_str)
      return 1;
  return 0;
}

/**@internal
 * Create a request message.
 *
 * @param nua
 * @param nh
 * @param cr
 * @param restart
 * @param method
 * @param name
 * @param tag, value, ... list of tag-value pairs
 */
msg_t *nua_creq_msg(nua_t *nua,
		    nua_handle_t *nh,
		    nua_client_request_t *cr,
		    int restart,
		    sip_method_t method, char const *name,
		    tag_type_t tag, tag_value_t value, ...)
{
  struct nua_dialog_state *ds = nh->nh_ds;
  msg_t *msg = NULL;
  sip_t *sip;
  ta_list ta;
  url_string_t const *url = NULL;
  long seq = -1;
  int copy = 1;

  /* If restarting, use existing message */
  if (restart) {
    msg = cr->cr_msg; sip = sip_object(msg);

    /* Trying to restart different method? */
    if (sip && method && sip->sip_request->rq_method != method) {
      SU_DEBUG_3(("nua(%p): trying to %s "
		  "but there is already %s waiting to restart\n",
		  nh, name, sip->sip_request->rq_method_name));
      restart = 0, msg = NULL; sip = NULL;
    }

    /* Remove CSeq */
    if (sip && sip->sip_cseq)
      sip_header_remove(msg, sip, (sip_header_t *)sip->sip_cseq);
    if (sip && sip->sip_request)
      method = sip->sip_request->rq_method,
	name = sip->sip_request->rq_method_name;
  }

  if (!restart) {
    if (cr->cr_msg) {
      /* If method is ACK or CANCEL, use existing CSeq */
      if (method == sip_method_ack || method == sip_method_cancel) {
	sip_t *nh_sip = sip_object(cr->cr_msg);
	if (nh_sip && nh_sip->sip_cseq)
	  seq = nh_sip->sip_cseq->cs_seq;
	/* ACK/CANCEL cannot be restarted so we do not copy message */
	copy = 0;
      }
      else
	msg_destroy(cr->cr_msg), cr->cr_msg = NULL;
    }
    msg = nta_msg_create(nua->nua_nta, 0);

    /**@par Populating SIP Request Message with Tagged Arguments
     *
     * The tagged arguments can be used to pass values for any SIP headers
     * to the stack. When the INVITE message (or any other SIP message) is
     * created, the tagged values saved with nua_handle() are used first,
     * next the tagged values given with the operation (nua_invite()) are
     * added.
     *
     * When multiple tags for the same header are specified, the behaviour
     * depends on the header type. If only a single header field can be
     * included in a SIP message, the latest non-NULL value is used, e.g.,
     * @Subject. However, if the SIP header can consist of multiple lines or
     * header fields separated by comma, e.g., @Accept, all the tagged
     * values are concatenated.
     *
     * However, if a tag value is #SIP_NONE (-1 casted as a void pointer),
     * the values from previous tags are ignored.
     */
    tl_gets(nh->nh_tags, NUTAG_URL_REF(url), TAG_END());
    sip_add_tl(msg, sip_object(msg), TAG_NEXT(nh->nh_tags));
  }

  ta_start(ta, tag, value);

  sip = sip_object(msg);
  if (!sip)
    goto error;
  if (sip_add_tl(msg, sip, ta_tags(ta)) < 0)
    goto error;

  if (method != sip_method_ack) {
    /**
     * Next, values previously set with nua_set_params() or nua_set_hparams()
     * are used: @Allow, @Supported, @Organization, and @UserAgent headers are
     * added to the request if they are not already set. 
     */
    if (!sip->sip_allow && !ds->ds_remote_tag)
      sip_add_dup(msg, sip, (sip_header_t*)NH_PGET(nh, allow));

    if (!sip->sip_supported && NH_PGET(nh, supported))
      sip_add_dup(msg, sip, (sip_header_t *)NH_PGET(nh, supported));
    
    if (method == sip_method_register && NH_PGET(nh, path_enable) &&
	!sip_has_feature(sip->sip_supported, "path") &&
	!sip_has_feature(sip->sip_require, "path"))
      sip_add_make(msg, sip, sip_supported_class, "path");

    if (!sip->sip_organization && NH_PGET(nh, organization))
      sip_add_dup(msg, sip, (sip_header_t *)NH_PGET(nh, organization));

    if (!sip->sip_user_agent && NH_PGET(nh, user_agent))
      sip_add_make(msg, sip, sip_user_agent_class, NH_PGET(nh, user_agent));
  }
	  
  {
    int add_contact = 0, use_dialog = 0, add_service_route, has_contact = 0;
    tagi_t const *t;

    for (t = ta_args(ta); t; t = t_next(t)) {
      if (t->t_tag == siptag_contact ||
	  t->t_tag == siptag_contact_str)
	has_contact = 1;
      else if (t->t_tag == nutag_url)
	url = (url_string_t const *)t->t_value;
      else if (t->t_tag == nutag_method && method == sip_method_unknown)
	name = (char const *)t->t_value;
      else if (t->t_tag == nutag_use_dialog)
	use_dialog = t->t_value != 0;
      else if (t->t_tag == _nutag_add_contact)
	add_contact = t->t_value != 0;
    }

    if (!restart)
      cr->cr_has_contact = has_contact;

    if (has_contact) add_contact = 0;

    if (method == sip_method_register && url == NULL)
      url = (url_string_t const *)NH_PGET(nh, registrar);

    if (seq != -1) {
      sip_cseq_t *cseq;
     
      assert(method != sip_method_unknown || name || sip->sip_request);
      
      if (method || name)
	cseq = sip_cseq_create(msg_home(msg), seq, method, name);
      else 
	cseq = sip_cseq_create(msg_home(msg), seq, 
			       sip->sip_request->rq_method, 
			       sip->sip_request->rq_method_name);

      sip_header_insert(msg, sip, (sip_header_t *)cseq);
    }

    /**
     * Now, the target URI for the request needs to be determined.
     *
     * For initial requests, values from tags are used. If NUTAG_URL() is
     * given, it is used as target URI. Otherwise, if SIPTAG_TO() is given,
     * it is used as target URI. If neither is given, the complete request
     * line already specified using SIPTAG_REQUEST() or SIPTAG_REQUEST_STR()
     * is used. At this point, the target URI is stored in the request line,
     * together with method name and protocol version ("SIP/2.0"). The
     * initial dialog information is also created: @CallID, @CSeq headers
     * are generated, if they do not exist, and a tag is added to the @From
     * header.
     */
    if (!ds->ds_leg) {
      nta_leg_t *leg = nua->nua_dhandle->nh_ds->ds_leg;

      if ((ds->ds_remote_tag && ds->ds_remote_tag[0] && 
	   sip_to_tag(nh->nh_home, sip->sip_to, ds->ds_remote_tag) < 0)
	  || 
	  (sip->sip_from == NULL &&
	   sip_add_dup(msg, sip, (sip_header_t *)nua->nua_from) < 0))
	goto error;

      if (use_dialog) {
	ds->ds_leg = nta_leg_tcreate(nua->nua_nta,
				     nua_stack_process_request, nh,
				     SIPTAG_CALL_ID(sip->sip_call_id),
				     SIPTAG_FROM(sip->sip_from),
				     SIPTAG_TO(sip->sip_to),
				     SIPTAG_CSEQ(sip->sip_cseq),
				     TAG_END());
	if (!ds->ds_leg)
	  goto error;

	leg = ds->ds_leg;

	if (!sip->sip_from->a_tag &&
	    sip_from_tag(msg_home(msg), sip->sip_from,
			 nta_leg_tag(ds->ds_leg, NULL)) < 0)
	  goto error;
      }

      if (nta_msg_request_complete(msg, leg, method, name, url) < 0)
	goto error;

      add_service_route = !restart;
    }
    else {
      /**
       * For in-dialog requests, the request URI is taken from the @Contact
       * header received from the remote party during dialog establishment, 
       * and the NUTAG_URL() is ignored.
       */
      if (ds->ds_route)
	url = NULL;

      /**Also, the @CallID and @CSeq headers and @From and @To tags are
       * generated based on the dialog information and added to the request. 
       * If the dialog has a route, it is added to the request, too.
       */
      if (nta_msg_request_complete(msg, ds->ds_leg, method, name, url) < 0)
	goto error;
      add_service_route = 0;
    }
    /***
     * @MaxForwards header (with default value set by NTATAG_MAX_FORWARDS()) is
     * also added now, if it does not exist.
     */

    /**
     * Next, the stack generates a @Contact header for the request (unless
     * the application already gave a @Contact header or it does not want to
     * use @Contact and indicates that by including SIPTAG_CONTACT(NULL) or
     * SIPTAG_CONTACT(SIP_NONE) in the tagged parameters.) If the
     * application has registered the URI in @From header, the @Contact
     * header used with registration is used. Otherwise, the @Contact header
     * is generated from the local IP address and port number.
     */
    if (!add_contact ||
	sip->sip_contact ||
	nua_tagis_have_contact_tag(nh->nh_tags) ||
	nua_tagis_have_contact_tag(ta_args(ta)))
      add_contact = 0;

    /**For the initial requests, @ServiceRoute set received from the registrar
     * is also added to the request message.
     */
    if (add_contact || add_service_route) {
      if (nua_registration_add_contact_to_request(nh, msg, sip, 
						  add_contact, 
						  add_service_route) < 0)
	goto error;
    }

    if (method != sip_method_ack) {
      if (nh->nh_auth) {
	nh_authorize(nh, ta_tags(ta));

	if (method != sip_method_invite &&
	    method != sip_method_update &&
	    method != sip_method_prack &&
	    /* auc_authorize() removes existing authentication headers */
	    auc_authorize(&nh->nh_auth, msg, sip) < 0)
	  goto error;
      }
    }
    else /* ACK */ {
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
    }

    ta_end(ta);

    if (!ds->ds_remote)
      ds->ds_remote = sip_to_dup(nh->nh_home, sip->sip_to);
    if (!ds->ds_local)
      ds->ds_local = sip_from_dup(nh->nh_home, sip->sip_from);

    if (copy) {
      cr->cr_msg = msg;
      msg = msg_copy(msg);
    }
  }

  return msg;

 error:
  ta_end(ta);
  msg_destroy(msg);
  return NULL;
}

/* ---------------------------------------------------------------------- */
nua_client_request_t *
nua_client_request_pending(nua_client_request_t const *cr)
{
  for (;cr;cr = cr->cr_next)
    if (cr->cr_orq)
      return (nua_client_request_t *)cr;

  return NULL;
}

nua_client_request_t *
nua_client_request_restarting(nua_client_request_t const *cr)
{
  for (;cr;cr = cr->cr_next)
    if (cr->cr_restart)
      return (nua_client_request_t *)cr;

  return NULL;
}

nua_client_request_t *
nua_client_request_by_orq(nua_client_request_t const *cr,
			  nta_outgoing_t const *orq)
{
  for (;cr;cr = cr->cr_next)
    if (cr->cr_orq == orq)
      return (nua_client_request_t *)cr;

  return NULL;
}

void nua_creq_deinit(nua_client_request_t *cr, nta_outgoing_t *orq)
{
  if (orq == NULL || orq == cr->cr_orq) {
    cr->cr_retry_count = 0;
    cr->cr_offer_sent = cr->cr_answer_recv = 0;

    if (cr->cr_msg)
      msg_destroy(cr->cr_msg);
    cr->cr_msg = NULL;

    if (cr->cr_orq)
      nta_outgoing_destroy(cr->cr_orq);
    cr->cr_orq = NULL;
  }
  else {
    nta_outgoing_destroy(orq);
  }
}


/**@internal
 * Get remote contact header for @a irq */
static inline
sip_contact_t const *incoming_contact(nta_incoming_t *irq)
{
  sip_contact_t const *retval = NULL;
  msg_t *request;
  sip_t *sip;

  request = nta_incoming_getrequest(irq);
  sip = sip_object(request);
  if (sip)
    retval = sip->sip_contact;
  msg_destroy(request);

  return retval;
}

/**@internal
 * Create a response message.
 */
msg_t *nh_make_response(nua_t *nua,
			nua_handle_t *nh,
			nta_incoming_t *irq,
			int status, char const *phrase,
			tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  msg_t *msg = nta_msg_create(nua->nua_nta, 0);
  sip_t *sip = sip_object(msg);
  msg_t *retval = NULL;
  tagi_t const *t;

  ta_start(ta, tag, value);

  if (!msg)
    /* retval is NULL */;
  else if (nta_msg_response_complete(msg, irq, status, phrase) < 0)
    msg_destroy(msg);
  else if (sip_add_tl(msg, sip, ta_tags(ta)) < 0)
    msg_destroy(msg);
  else if (sip_complete_message(msg) < 0)
    msg_destroy(msg);
  else if (!sip->sip_supported && NH_PGET(nh, supported) &&
	   sip_add_dup(msg, sip, (sip_header_t *)NH_PGET(nh, supported)) < 0)
    msg_destroy(msg);
  else if (!sip->sip_user_agent && NH_PGET(nh, user_agent) &&
	   sip_add_make(msg, sip, sip_user_agent_class, 
			NH_PGET(nh, user_agent)) < 0)
    msg_destroy(msg);
  else if (!sip->sip_organization && NH_PGET(nh, organization) &&
	   sip_add_dup(msg, sip, (sip_header_t *)NH_PGET(nh, organization)) < 0)
    msg_destroy(msg);
  else if (!sip->sip_allow && NH_PGET(nh, allow) &&
	   sip_add_dup(msg, sip, (sip_header_t*)NH_PGET(nh, allow)) < 0)
    msg_destroy(msg);
  else if (!sip->sip_allow_events && 
	   (sip->sip_cseq && 
	    (sip->sip_cseq->cs_method == sip_method_publish ||
	     sip->sip_cseq->cs_method == sip_method_subscribe)) &&
	   NH_PGET(nh, allow_events) &&
	   sip_add_dup(msg, sip, (sip_header_t*)NH_PGET(nh, allow_events)) < 0)
    msg_destroy(msg);
  else if (!sip->sip_contact &&
	   (t = tl_find(ta_args(ta), _nutag_add_contact)) &&
	   t->t_value && 
	   nua_registration_add_contact_to_response(nh, msg, sip, NULL, 
						    incoming_contact(irq)) < 0)
    msg_destroy(msg);
  else
    retval = msg;

  ta_end(ta);

  return retval;
}


/* ======================================================================== */
/* Generic processing */

int nua_stack_process_unknown(nua_t *nua,
			      nua_handle_t *nh,
			      nta_incoming_t *irq,
			      sip_t const *sip)
{
  return 501;
}

/**@internal
 * Relay response message to the application.
 *
 * If handle has already been marked as destroyed by nua_handle_destroy(),
 * release the handle with nh_destroy().
 */
int nua_stack_process_response(nua_handle_t *nh,
			       nua_client_request_t *cr,
			       nta_outgoing_t *orq,
			       sip_t const *sip,
			       tag_type_t tag, tag_value_t value, ...)
{
  msg_t *msg = nta_outgoing_getresponse(orq);
  int status = sip->sip_status->st_status;
  char const *phrase = sip->sip_status->st_phrase;
  ta_list ta;
  int final;

  if (status >= 200 && status < 300)
    nh_challenge(nh, sip);  /* Collect nextnonce */

  if (nta_outgoing_method(orq) == sip_method_invite)
    final = status >= 300;
  else
    final = status >= 200;

  if (final && cr) {
    nua_creq_deinit(cr, orq);

    if (cr->cr_usage && nh->nh_ds->ds_cr == cr) {
      if ((status >= 300 && !cr->cr_usage->du_ready) ||
	  cr->cr_usage->du_terminating)
	nua_dialog_usage_remove(nh, nh->nh_ds, cr->cr_usage);
    }

    cr->cr_usage = NULL;
  }

  ta_start(ta, tag, value);

  nua_stack_event(nh->nh_nua, nh, msg, cr->cr_event, status, phrase,
		  ta_tags(ta));

  if (final)
    cr->cr_event = nua_i_error;

  ta_end(ta);

  return 0;
}

static inline
int can_redirect(sip_contact_t const *m, sip_method_t method)
{
  if (m && m->m_url->url_host) {
    enum url_type_e type = m->m_url->url_type;
    return
      type == url_sip ||
      type == url_sips ||
      (type == url_tel &&
       (method == sip_method_invite || method == sip_method_message)) ||
      (type == url_im && method == sip_method_message) ||
      (type == url_pres && method == sip_method_subscribe);
  }
  return 0;
}

int nua_creq_restart_with(nua_handle_t *nh,
			  nua_client_request_t *cr,
			  nta_outgoing_t *orq,
			  int status, char const *phrase,
			  nua_creq_restart_f *f,
			  TAG_LIST)
{
  ta_list ta;
  msg_t *msg = nta_outgoing_getresponse(orq);

  nua_stack_event(nh->nh_nua, nh, msg, cr->cr_event, status, phrase,
		  TAG_END());

  nta_outgoing_destroy(orq);

  if (f) {
    ta_start(ta, tag, value);
    f(nh, ta_args(ta));
    ta_end(ta);
  }

  return 1;
}


/** @internal Save operation until it can be restarted */
int nua_creq_save_restart(nua_handle_t *nh,
			  nua_client_request_t *cr,
			  nta_outgoing_t *orq,
			  int status, char const *phrase,
			  nua_creq_restart_f *restart_function)
{
  nua_dialog_usage_t *du = cr->cr_usage;
  msg_t *msg = nta_outgoing_getresponse(orq);

  nua_stack_event(nh->nh_nua, nh, msg, cr->cr_event,
		  status, phrase, 
		  TAG_END());
  nta_outgoing_destroy(orq);

  if (du)
    du->du_refresh = 0;

  cr->cr_restart = restart_function;
  return 1;
}


/**@internal
 * Check response, return true if we can restart the request.
 *
 */
int nua_creq_check_restart(nua_handle_t *nh,
			   nua_client_request_t *cr,
			   nta_outgoing_t *orq,
			   sip_t const *sip,
			   nua_creq_restart_f *restart_function)
{
  int status = sip->sip_status->st_status;
  sip_method_t method = nta_outgoing_method(orq);

  nua_dialog_usage_t *du = cr->cr_usage;

  assert(restart_function);

  if (orq != cr->cr_orq)
    return 0;

  cr->cr_orq = NULL;
  cr->cr_restart = NULL;

  if (cr->cr_msg == NULL || status < 200)
    ;
  else if (++cr->cr_retry_count > NH_PGET(nh, retry_count))
    ;
  else if (status == 302) {
    if (can_redirect(sip->sip_contact, method)) {
      return
	nua_creq_restart_with(nh, cr, orq, 100, "Redirected",
			      restart_function,
			      NUTAG_URL(sip->sip_contact->m_url),
			      TAG_END());
    }
  }
  else if (status == 423) {
    sip_t *req = sip_object(cr->cr_msg);
    unsigned my_expires = 0;

    if (req->sip_expires)
      my_expires = req->sip_expires->ex_delta;

    if (sip->sip_min_expires &&
	sip->sip_min_expires->me_delta > my_expires) {
      sip_expires_t ex[1];
      sip_expires_init(ex);
      ex->ex_delta = sip->sip_min_expires->me_delta;

      return
	nua_creq_restart_with(nh, cr, orq,
			      100, "Re-Negotiating Expiration",
			      restart_function, 
			      SIPTAG_EXPIRES(ex),
			      TAG_END());
    }
  }
  else if (method != sip_method_ack && method != sip_method_cancel &&
	   ((status == 401 && sip->sip_www_authenticate) ||
	    (status == 407 && sip->sip_proxy_authenticate)) &&
	   nh_challenge(nh, sip) > 0) {
    msg_t *request = nta_outgoing_getrequest(orq);
    sip_t *rsip = sip_object(request);
    int done;

    /* XXX - check for instant restart */
    done = auc_authorization(&nh->nh_auth, cr->cr_msg, (msg_pub_t*)NULL,
			     rsip->sip_request->rq_method_name,
			     rsip->sip_request->rq_url,
			     rsip->sip_payload);

    msg_destroy(request);

    if (done > 0) {
      return
	nua_creq_restart_with(nh, cr, orq,
			      100, "Request Authorized by Cache",
			      restart_function, TAG_END());
    }
    else if (done == 0) {
      /* Operation waits for application to call nua_authenticate() */
      return nua_creq_save_restart(nh, cr, orq, 
				   status, sip->sip_status->st_phrase, 
				   restart_function);
    }
    else {
      SU_DEBUG_5(("nua(%p): auc_authorization failed\n", nh));
    }
  }

  /* This was final response that cannot be restarted. */
  cr->cr_orq = orq;

  if (du)
    du->du_refresh = 0;
  cr->cr_retry_count = 0;

  if (cr->cr_msg)
    msg_destroy(cr->cr_msg), cr->cr_msg = NULL;

  return 0;
}

/** @internal Restart a request */
int nua_creq_restart(nua_handle_t *nh,
		     nua_client_request_t *cr,
		     nta_response_f *cb,
		     tagi_t *tags)
{
  msg_t *msg;

  cr->cr_restart = NULL;

  if (!cr->cr_msg)
    return 0;

  msg = nua_creq_msg(nh->nh_nua, nh, cr, 1, SIP_METHOD_UNKNOWN,
		     TAG_NEXT(tags));

  cr->cr_orq = nta_outgoing_mcreate(nh->nh_nua->nua_nta, cb, nh, NULL, msg,
				    SIPTAG_END(), TAG_NEXT(tags));

  if (!cr->cr_orq) {
    msg_destroy(msg);
    return 0;
  }

  return 1;
}

/* ======================================================================== */
/* Authentication */

/** @NUA_EVENT nua_r_authenticate
 *
 * Response to nua_authenticate(). Under normal operation, this event is
 * never sent but rather the unauthenticated operation is completed. 
 * However, if there is no operation to authentication or if there is an
 * authentication error the #nua_r_authenticate event is sent to the
 * application with the status code as follows:
 * - <i>202 No operation to restart</i>:\n
 *   The authenticator associated with the handle was updated, but there was
 *   no operation to retry with the new credentials.
 * - <i>900 Cannot add credentials</i>:\n
 *   There was internal problem updating authenticator.
 * - <i>904 No matching challenge</i>:\n
 *   There was no challenge matching with the credentials provided by
 *   nua_authenticate(), e.g., their realm did not match with the one 
 *   received with the challenge.
 * 
 * @param status status code from authentication 
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle authenticated
 * @param hmagic application context associated with the handle
 * @param sip    NULL
 * @param tags   empty
 * 
 * @sa nua_terminate(), nua_handle_destroy()
 *
 * @END_NUA_EVENT
 */

void
nua_stack_authenticate(nua_t *nua, nua_handle_t *nh, nua_event_t e,
		       tagi_t const *tags)
{
  int status = nh_authorize(nh, TAG_NEXT(tags));

  if (status > 0) {
    nua_client_request_t *cr;
    nua_creq_restart_f *restart = NULL;

    cr = nua_client_request_restarting(nh->nh_ds->ds_cr);

    if (cr) 
      restart = cr->cr_restart, cr->cr_restart = NULL;

    if (restart) {
      /* nua_stack_event(nua, nh, NULL, e, SIP_200_OK, TAG_END()); */
      restart(nh, (tagi_t *)tags);	/* Restart operation */
    }
    else {
      nua_stack_event(nua, nh, NULL, e, 
		      202, "No operation to restart",
		      TAG_END());
    }
  }
  else if (status < 0) {
    nua_stack_event(nua, nh, NULL, e, 900, "Cannot add credentials", TAG_END());
  }
  else {
    nua_stack_event(nua, nh, NULL, e, 904, "No matching challenge", TAG_END());
  }
}

/* ======================================================================== */
/*
 * Process incoming requests
 */

int nua_stack_process_request(nua_handle_t *nh,
			      nta_leg_t *leg,
			      nta_incoming_t *irq,
			      sip_t const *sip)
{
  nua_t *nua = nh->nh_nua;
  sip_method_t method = sip->sip_request->rq_method;
  char const *user_agent = NH_PGET(nh, user_agent);
  sip_supported_t const *supported = NH_PGET(nh, supported);
  sip_allow_t const *allow = NH_PGET(nh, allow);
  enter;

  nta_incoming_tag(irq, NULL);

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
    nta_incoming_treply(irq, SIP_416_UNSUPPORTED_URI,
			SIPTAG_ALLOW(allow),
			SIPTAG_SUPPORTED(supported),
			SIPTAG_USER_AGENT_STR(user_agent),
			TAG_END());
  }

  if (nta_check_required(irq, sip, supported,
			 SIPTAG_ALLOW(allow),
			 SIPTAG_USER_AGENT_STR(user_agent),
			 TAG_END()))
    return 420;

  if (nh == nua->nua_dhandle) {
    if (!sip->sip_to->a_tag)
      ;
    else if (method == sip_method_message && NH_PGET(nh, win_messenger_enable))
      ;
    else {
      nta_incoming_treply(irq, 481, "Initial transaction with a To tag",
			  TAG_END());
      return 481;
    }
    nh = NULL;
  }

  if (sip->sip_timestamp)
    nta_incoming_treply(irq, SIP_100_TRYING, TAG_END());

  switch (method) {
  case sip_method_invite:
    return nua_stack_process_invite(nua, nh, irq, sip);

  case sip_method_info:
    if (nh) return nua_stack_process_info(nua, nh, irq, sip);
    /*FALLTHROUGH*/

  case sip_method_update:
    if (nh) return nua_stack_process_update(nua, nh, irq, sip);
    /*FALLTHROUGH*/

  case sip_method_bye:
    if (nh) return nua_stack_process_bye(nua, nh, irq, sip);

    nta_incoming_treply(irq,
			481, "Call Does Not Exist",
			SIPTAG_ALLOW(allow),
			SIPTAG_SUPPORTED(supported),
			SIPTAG_USER_AGENT_STR(user_agent),
			TAG_END());
    return 481;

  case sip_method_message:
    return nua_stack_process_message(nua, nh, irq, sip);

  case sip_method_notify:
    return nua_stack_process_notify(nua, nh, irq, sip);

  case sip_method_subscribe:
    return nua_stack_process_subscribe(nua, nh, irq, sip);

  case sip_method_register:
    return nua_stack_process_register(nua, nh, irq, sip);

  case sip_method_options:
    return nua_stack_process_options(nua, nh, irq, sip);

  case sip_method_refer:
    return nua_stack_process_refer(nua, nh, irq, sip);

  case sip_method_publish:
    return nua_stack_process_publish(nua, nh, irq, sip);

  case sip_method_ack:
  case sip_method_cancel:
    SU_DEBUG_1(("nua(%p): strange %s from <" URL_PRINT_FORMAT ">\n", nh,
		sip->sip_request->rq_method_name,
		URL_PRINT_ARGS(sip->sip_from->a_url)));
    /* Send nua_i_error ? */
    return 481;

  case sip_method_unknown:
    return nua_stack_process_method(nua, nh, irq, sip);

  default:
    return nua_stack_process_unknown(nua, nh, irq, sip);
  }
}

nua_server_request_t *nua_server_request(nua_t *nua,
					 nua_handle_t *nh,
					 nta_incoming_t *irq,
					 sip_t const *sip,
					 nua_server_request_t *sr,
					 size_t size,
					 nua_server_respond_f *respond,
					 int create_dialog)
{
  int initial = 1, final = 200;

  assert(nua && irq && sip && sr);

  initial = nh == NULL || nh == nua->nua_dhandle;

  /* INVITE server request is not finalized after 2XX response */
  if (sip->sip_request->rq_method == sip_method_invite)
    final = 300;

  /* Create handle if request does not fail */
  if (sr->sr_status >= 300)
    ;
  else if (initial) {
    if (!(nh = nua_stack_incoming_handle(nua, irq, sip, create_dialog)))
      SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);
  }
  else if (create_dialog) {
    nua_dialog_store_peer_info(nh, nh->nh_ds, sip);
    nua_dialog_uas_route(nh, nh->nh_ds, sip, 1);
  }

  if (nh == NULL)
    nh = nua->nua_dhandle;

  if (sr->sr_status < final) {
    nua_server_request_t *sr0 = sr;

    if (size < (sizeof *sr))
      size = sizeof *sr;

    sr = su_zalloc(nh->nh_home, size);

    if (sr) {
      if ((sr->sr_next = nh->nh_ds->ds_sr))
	*(sr->sr_prev = sr->sr_next->sr_prev) = sr,
	  sr->sr_next->sr_prev = &sr->sr_next;
      else
	*(sr->sr_prev = &nh->nh_ds->ds_sr) = sr;
      SR_STATUS(sr, sr0->sr_status, sr0->sr_phrase);
    }
    else {
      sr = sr0;
      SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);
    }
  }

  sr->sr_owner = nh;
  sr->sr_method = sip->sip_request->rq_method;
  sr->sr_respond = respond;
  sr->sr_irq = irq;
  sr->sr_initial = initial;

  return sr;
}		 

void nua_server_request_destroy(nua_server_request_t *sr)
{
  if (sr->sr_irq)
    nta_incoming_destroy(sr->sr_irq), sr->sr_irq = NULL;

  sr->sr_msg = NULL;

  if (sr->sr_prev) {
    if ((*sr->sr_prev = sr->sr_next))
      sr->sr_next->sr_prev = sr->sr_prev;

    if (sr->sr_owner)
      su_free(sr->sr_owner->nh_home, sr);
  }
}

/** Send server event (nua_i_*) to the application. */
int nua_stack_server_event(nua_t *nua,
			   nua_server_request_t *sr,
			   nua_event_t event,
			   tag_type_t tag, tag_value_t value, ...)
{
  nua_handle_t *nh = sr->sr_owner; 
  int status, final = 0;

  if (nh == NULL) nh = nua->nua_dhandle;

  if (sr->sr_status > 100)
    /* Note that this may change the sr->sr_status */
    final = sr->sr_respond(sr, NULL);

  status = sr->sr_status;

  if (status >= 200)
    sr->sr_respond = NULL;
  
  if (status < 300 || !sr->sr_initial) {
    ta_list ta;
    msg_t *request;

    ta_start(ta, tag, value);

    assert(sr->sr_owner);
    request = nta_incoming_getrequest(sr->sr_irq);
    nua_stack_event(nua, sr->sr_owner, request, event, 
		    sr->sr_status, sr->sr_phrase, 
		    ta_tags(ta));
    ta_end(ta);

    if (final)
      nua_server_request_destroy(sr);
    else if (sr->sr_status < 200)
      sr->sr_msg = request;
  }
  else {
    nh = sr->sr_owner;

    nua_server_request_destroy(sr);

    if (nh && nh != nua->nua_dhandle)
      nh_destroy(nua, nh);
  }

  return 0;
}

/** Respond to a request. */
int nua_server_respond(nua_server_request_t *sr,
		       int status, char const *phrase,
		       tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  int final;

  assert(sr && sr->sr_respond);
  SR_STATUS(sr, status, phrase);

  ta_start(ta, tag, value);
  final = sr->sr_respond(sr, ta_args(ta));
  ta_end(ta);

  if (final) {
    nua_server_request_destroy(sr);    
    return final;
  }

  if (sr->sr_status >= 200)
    sr->sr_respond = NULL;

  return 0;
}

msg_t *nua_server_response(nua_server_request_t *sr,
			   int status, char const *phrase,
			   tag_type_t tag, tag_value_t value, ...)
{
  msg_t *msg;
  ta_list(ta);

  assert(sr && sr->sr_owner && sr->sr_owner->nh_nua);

  ta_start(ta, tag, value);

  msg = nh_make_response(sr->sr_owner->nh_nua, sr->sr_owner, sr->sr_irq,
			 sr->sr_status, sr->sr_phrase,
			 ta_tags(ta));
  
  ta_end(ta);

  return msg;
}

int nua_default_respond(nua_server_request_t *sr,
			tagi_t const *tags)
{
  msg_t *m;

  assert(sr && sr->sr_owner && sr->sr_owner->nh_nua);

  m = nh_make_response(sr->sr_owner->nh_nua, sr->sr_owner, 
		       sr->sr_irq,
		       sr->sr_status, sr->sr_phrase,
		       TAG_NEXT(tags));

  if (m) {
    if (nta_incoming_mreply(sr->sr_irq, m) < 0)
      SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);      
  }
  
  return sr->sr_status >= 200 ? sr->sr_status : 0;
}

/** Respond to an request with given status. 
 *
 * When nua protocol engine receives an incoming SIP request, it can either
 * respond to the request automatically or let it up to application to
 * respond to the request. The automatic answer is sent if the request fails
 * because of method, SIP extension or, in some times, MIME content
 * negotiation fails.
 *
 * When responding to an incoming INVITE request, the nua_respond() can be
 * called without NUTAG_WITH() (or NUTAG_WITH_CURRENT() or
 * NUTAG_WITH_SAVED()). Otherwise, NUTAG_WITH() will contain an indication
 * of the request being responded.
 *
 * In order to simplify the simple applications, most requests are responded
 * automatically. The set of requests always responded by the stack include
 * BYE, CANCEL and NOTIFY. The application can add methods that it likes to
 * handle by itself with NUTAG_APPL_METHOD(). The default set of
 * NUTAG_APPL_METHOD() includes INVITE, PUBLISH, REGISTER and SUBSCRIBE. 
 * Note that unless the method is also included in the set of allowed
 * methods with NUTAG_ALLOW(), the stack will respond to the incoming
 * methods with <i>405 Not Allowed</i>.
 *
 * Note that certain methods are rejected outside a SIP session (created
 * with INVITE transaction). They include BYE, UPDATE, PRACK and INFO. Also
 * the auxiliary methods ACK and CANCEL are rejected by stack if there is no
 * ongoing INVITE transaction corresponding to them.
 *
 * @param nh              Pointer to operation handle
 * @param status          SIP response status (see RFCs of SIP)
 * @param phrase          free text (default response phrase used if NULL)
 * @param tag, value, ... List of tagged parameters
 *
 * @return 
 *    nothing
 *
 * @par Related Tags:
 *    NUTAG_WITH(), NUTAG_WITH_CURRENT(), NUTAG_WITH_SAVED() \n
 *    NUTAG_EARLY_ANSWER() \n
 *    SOATAG_ADDRESS() \n
 *    SOATAG_AF() \n
 *    SOATAG_HOLD() \n
 *    Tags in <sip_tag.h>.
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
    if (request && sr->sr_msg == request)
      break;
    /* nua_respond() to INVITE can be used without NUTAG_WITH() */
    if (!t && sr->sr_method == sip_method_invite && sr->sr_respond)
      break;
  }
  
  if (sr && sr->sr_respond) {
    int final;
    SR_STATUS(sr, status, phrase);
    final = sr->sr_respond(sr, tags);
    if (final)
      nua_server_request_destroy(sr);    
    else if (sr->sr_status >= 200)
      sr->sr_respond = NULL;
    return;
  }
  else if (sr) {
    nua_stack_event(nua, nh, NULL, nua_i_error,
		    500, "Already Sent Final Response", TAG_END());
    return;
  }

  nua_stack_event(nua, nh, NULL, nua_i_error,
		  500, "Responding to a Non-Existing Request", TAG_END());
}
