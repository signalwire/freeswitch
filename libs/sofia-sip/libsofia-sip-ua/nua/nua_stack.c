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
#include <sofia-sip/su_tag_inline.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_strlst.h>
#include <sofia-sip/su_uniqueid.h>

#include <sofia-sip/su_tag_io.h>

#define SU_ROOT_MAGIC_T   struct nua_s
#define SU_MSG_ARG_T      struct nua_ee_data
#define SU_TIMER_ARG_T    struct nua_client_request

#define NUA_SAVED_EVENT_T su_msg_t *
#define NUA_SAVED_SIGNAL_T su_msg_t *

#define NTA_AGENT_MAGIC_T    struct nua_s
#define NTA_LEG_MAGIC_T      struct nua_handle_s
#define NTA_OUTGOING_MAGIC_T struct nua_client_request

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

/* ---------------------------------------------------------------------- */
/* Internal types */

/** Extended event data. */
typedef struct nua_ee_data {
  nua_t *ee_nua;
  nua_event_data_t ee_data[1];
} nua_ee_data_t;

/** @internal Linked stack frames from nua event callback */
struct nua_event_frame_s {
  nua_event_frame_t *nf_next;
  nua_saved_event_t nf_saved[1];
};


static void nua_event_deinit(nua_ee_data_t *ee);
static void nua_application_event(nua_t *, su_msg_r, nua_ee_data_t *ee);
static void nua_stack_signal(nua_t *nua, su_msg_r, nua_ee_data_t *ee);

nua_handle_t *nh_create(nua_t *nua, tag_type_t t, tag_value_t v, ...);
static void nh_append(nua_t *nua, nua_handle_t *nh);
static void nh_remove(nua_t *nua, nua_handle_t *nh);

static int nh_authorize(nua_handle_t *nh,
			tag_type_t tag, tag_value_t value, ...);

static void nua_stack_timer(nua_t *nua, su_timer_t *t, su_timer_arg_t *a);

static int nua_client_request_complete(nua_client_request_t *cr);

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
  dnh->nh_valid = nua_valid_handle_cookie;
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

  if (nua->nua_prefs->ngp_detect_network_updates)
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

int nua_stack_tevent(nua_t *nua, nua_handle_t *nh, msg_t *msg,
		     nua_event_t event, int status, char const *phrase,
		     tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  int retval;
  ta_start(ta, tag, value);
  retval = nua_stack_event(nua, nh, msg, event, status, phrase, ta_args(ta));
  ta_end(ta);
  return retval;
}

/** @internal Send an event to the application. */
int nua_stack_event(nua_t *nua, nua_handle_t *nh, msg_t *msg,
		    nua_event_t event, int status, char const *phrase,
		    tagi_t const *tags)
{
  su_msg_r sumsg = SU_MSG_R_INIT;
  size_t e_len, len, xtra, p_len;

  if (event == nua_r_ack || event == nua_i_none)
    return event;

  if (nh == nua->nua_dhandle)
    nh = NULL;

  if (nua_log->log_level >= 5) {
    char const *name = nua_event_name(event) + 4;
    char const *p = phrase ? phrase : "";

    if (status == 0)
      SU_DEBUG_5(("nua(%p): event %s %s\n", (void *)nh, name, p));
    else
      SU_DEBUG_5(("nua(%p): event %s %u %s\n", (void *)nh, name, status, p));
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
      || (nua->nua_shutdown && event != nua_r_shutdown &&
	  !nua->nua_prefs->ngp_shutdown_events)) {
    if (msg)
      msg_destroy(msg);
    return event;
  }

  if (tags) {
    e_len = offsetof(nua_ee_data_t, ee_data[0].e_tags);
    len = tl_len(tags);
    xtra = tl_xtra(tags, len);
  }
  else {
    e_len = sizeof(nua_ee_data_t), len = 0, xtra = 0;
  }
  p_len = phrase ? strlen(phrase) + 1 : 1;

  if (su_msg_new(sumsg, e_len + len + xtra + p_len) == 0) {
    nua_ee_data_t *ee = su_msg_data(sumsg);
    nua_event_data_t *e = ee->ee_data;
    void *p;

    if (tags) {
      tagi_t *t = e->e_tags, *t_end = (tagi_t *)((char *)t + len);
      void *b = t_end, *end = (char *)b + xtra;

      t = tl_dup(t, tags, &b); p = b;
      assert(t == t_end); assert(b == end); (void)end;
    }
    else
      p = e + 1;

    ee->ee_nua = nua_stack_ref(nua);
    e->e_event = event;
    e->e_nh = nh ? nua_handle_ref(nh) : NULL;
    e->e_status = status;
    e->e_phrase = strcpy(p, phrase ? phrase : "");
    if (msg)
      e->e_msg = msg, su_home_threadsafe(msg_home(msg));

    su_msg_deinitializer(sumsg, nua_event_deinit);

    su_msg_send_to(sumsg, nua->nua_client, nua_application_event);
  }

  return event;
}

static
void nua_event_deinit(nua_ee_data_t *ee)
{
  nua_t *nua = ee->ee_nua;
  nua_event_data_t *e = ee->ee_data;
  nua_handle_t *nh = e->e_nh;

  if (e->e_msg)
    msg_destroy(e->e_msg), e->e_msg = NULL;

  if (nh)
    nua_handle_unref(nh), e->e_nh = NULL;

  if (nua)
    nua_stack_unref(nua), ee->ee_nua = NULL;
}

/*# Receive event from protocol machine and hand it over to application */
static
void nua_application_event(nua_t *dummy, su_msg_r sumsg, nua_ee_data_t *ee)
{
  nua_t *nua = ee->ee_nua;
  nua_event_data_t *e = ee->ee_data;
  nua_handle_t *nh = e->e_nh;

  enter;

  ee->ee_nua = NULL;
  e->e_nh = NULL;

  if (nh == NULL) {
    /* Xyzzy */
  }
  else if (nh->nh_valid) {
    if (!nh->nh_ref_by_user) {
      /* Application must now call nua_handle_destroy() */
      nh->nh_ref_by_user = 1;
      nua_handle_ref(nh);
    }
  }
  else if (!nh->nh_valid) {	/* Handle has been destroyed */
    if (nua_log->log_level >= 7) {
      char const *name = nua_event_name(e->e_event) + 4;
      SU_DEBUG_7(("nua(%p): event %s dropped\n", (void *)nh, name));
    }
    nua_handle_unref(nh);
    nua_stack_unref(nua);
    return;
  }

  if (e->e_event == nua_r_shutdown && e->e_status >= 200)
    nua->nua_shutdown_final = 1;

  if (nua->nua_callback) {
    nua_event_frame_t frame[1];

    su_msg_save(frame->nf_saved, sumsg);
    frame->nf_next = nua->nua_current, nua->nua_current = frame;

    nua->nua_callback(e->e_event, e->e_status, e->e_phrase,
		      nua, nua->nua_magic,
		      nh, nh ? nh->nh_magic : NULL,
		      e->e_msg ? sip_object(e->e_msg) : NULL,
		      e->e_tags);

    su_msg_destroy(frame->nf_saved);
    nua->nua_current = frame->nf_next;
  }

  nua_handle_unref(nh);
  nua_stack_unref(nua);
}

/** Get current request message. @NEW_1_12_4.
 *
 * @note A response message is returned when processing response message.
 *
 * @sa #nua_event_e, nua_respond(), NUTAG_WITH_CURRENT()
 */
msg_t *nua_current_request(nua_t const *nua)
{
  if (nua && nua->nua_current && su_msg_is_non_null(nua->nua_current->nf_saved))
    return su_msg_data(nua->nua_current->nf_saved)->ee_data->e_msg;
  return NULL;
}

/** Get request message from saved nua event. @NEW_1_12_4.
 *
 * @sa nua_save_event(), nua_respond(), NUTAG_WITH_SAVED(),
 */
msg_t *nua_saved_event_request(nua_saved_event_t const *saved)
{
  return saved && saved[0] ? su_msg_data(saved)->ee_data->e_msg : NULL;
}

/** Save nua event and its arguments.
 *
 * @sa #nua_event_e, nua_event_data() nua_saved_event_request(), nua_destroy_event()
 */
int nua_save_event(nua_t *nua, nua_saved_event_t return_saved[1])
{
  if (return_saved) {
    if (nua && nua->nua_current) {
      su_msg_save(return_saved, nua->nua_current->nf_saved);
      return su_msg_is_non_null(return_saved);
    }
    else
      *return_saved = NULL;
  }

  return 0;
}

/* ---------------------------------------------------------------------- */

/** @internal
 * Post signal to stack itself
 */
void nua_stack_post_signal(nua_handle_t *nh, nua_event_t event,
			   tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  ta_start(ta, tag, value);
  nua_signal((nh)->nh_nua, nh, NULL, event, 0, NULL, ta_tags(ta));
  ta_end(ta);
}


/*# Send a request to the protocol thread */
int nua_signal(nua_t *nua, nua_handle_t *nh, msg_t *msg,
	       nua_event_t event,
	       int status, char const *phrase,
	       tag_type_t tag, tag_value_t value, ...)
{
  su_msg_r sumsg = SU_MSG_R_INIT;
  size_t len, xtra, ee_len, l_len = 0, l_xtra = 0;
  ta_list ta;
  int retval = -1;

  if (nua == NULL)
    return -1;

  if (nua->nua_shutdown_started && event != nua_r_shutdown)
    return -1;

  ta_start(ta, tag, value);

  ee_len = offsetof(nua_ee_data_t, ee_data[0].e_tags);
  len = tl_len(ta_args(ta));
  xtra = tl_xtra(ta_args(ta), len);

  if (su_msg_new(sumsg, ee_len + len + l_len + xtra + l_xtra) == 0) {
    nua_ee_data_t *ee = su_msg_data(sumsg);
    nua_event_data_t *e = ee->ee_data;
    tagi_t *t = e->e_tags;
    void *b = (char *)t + len + l_len;

    tagi_t *tend = (tagi_t *)b;
    char *bend = (char *)b + xtra + l_xtra;

    t = tl_dup(t, ta_args(ta), &b);

    assert(tend == t); (void)tend; assert(b == bend); (void)bend;

    e->e_always = event == nua_r_destroy || event == nua_r_shutdown;
    e->e_event = event;
    e->e_nh = nh ? nua_handle_ref(nh) : NULL;
    e->e_status = status;
    e->e_phrase = phrase;

    su_msg_deinitializer(sumsg, nua_event_deinit);

    retval = su_msg_send_to(sumsg, nua->nua_server, nua_stack_signal);

    if (retval == 0){
      SU_DEBUG_7(("nua(%p): %s signal %s\n", (void *)nh,
		  "sent", nua_event_name(event) + 4));
    }
    else {
      SU_DEBUG_0(("nua(%p): %s signal %s\n", (void *)nh,
		  "FAILED TO SEND", nua_event_name(event) + 4));

    }
  }

  ta_end(ta);

  return retval;
}

/* ----------------------------------------------------------------------
 * Receiving events from client
 */
static
void nua_stack_signal(nua_t *nua, su_msg_r msg, nua_ee_data_t *ee)
{
  nua_event_data_t *e = ee->ee_data;
  nua_handle_t *nh = e->e_nh;
  tagi_t *tags = e->e_tags;
  nua_event_t event;
  int error = 0;

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
      SU_DEBUG_5(("nua(%p): %s signal %s\n", (void *)nh, "recv", name + 4));
    else
      SU_DEBUG_5(("nua(%p): recv signal %s %u %s\n",
		  (void *)nh, name + 4,
		  e->e_status, e->e_phrase ? e->e_phrase : ""));
  }

  su_msg_save(nua->nua_signal, msg);

  event = e->e_event;

  if (nua->nua_shutdown && !e->e_always) {
    /* Shutting down */
    nua_stack_event(nua, nh, NULL, event,
		    901, "Stack is going down",
		    NULL);
  }
  else switch (event) {
  case nua_r_get_params:
    nua_stack_get_params(nua, nh ? nh : nua->nua_dhandle, event, tags);
    break;
  case nua_r_set_params:
    nua_stack_set_params(nua, nh ? nh : nua->nua_dhandle, event, tags);
    break;
  case nua_r_shutdown:
    nua_stack_shutdown(nua);
    break;
  case nua_r_register:
  case nua_r_unregister:
    nua_stack_register(nua, nh, event, tags);
    break;
  case nua_r_invite:
    error = nua_stack_invite(nua, nh, event, tags);
    break;
  case nua_r_cancel:
    error = nua_stack_cancel(nua, nh, event, tags);
    break;
  case nua_r_bye:
    error = nua_stack_bye(nua, nh, event, tags);
    break;
  case nua_r_options:
    error = nua_stack_options(nua, nh, event, tags);
    break;
  case nua_r_refer:
    error = nua_stack_refer(nua, nh, event, tags);
    break;
  case nua_r_publish:
  case nua_r_unpublish:
    error = nua_stack_publish(nua, nh, event, tags);
    break;
  case nua_r_info:
    error = nua_stack_info(nua, nh, event, tags);
    break;
  case nua_r_prack:
    error = nua_stack_prack(nua, nh, event, tags);
    break;
  case nua_r_update:
    error = nua_stack_update(nua, nh, event, tags);
    break;
  case nua_r_message:
    error = nua_stack_message(nua, nh, event, tags);
    break;
  case nua_r_subscribe:
  case nua_r_unsubscribe:
    error = nua_stack_subscribe(nua, nh, event, tags);
    break;
  case nua_r_notify:
    error = nua_stack_notify(nua, nh, event, tags);
    break;
  case nua_r_notifier:
    nua_stack_notifier(nua, nh, event, tags);
    break;
  case nua_r_terminate:
    nua_stack_terminate(nua, nh, event, tags);
    break;
  case nua_r_method:
    error = nua_stack_method(nua, nh, event, tags);
    break;
  case nua_r_authenticate:
    nua_stack_authenticate(nua, nh, event, tags);
    break;
  case nua_r_authorize:
    nua_stack_authorize(nua, nh, event, tags);
    break;
  case nua_r_ack:
    error = nua_stack_ack(nua, nh, event, tags);
    break;
  case nua_r_respond:
    nua_stack_respond(nua, nh, e->e_status, e->e_phrase, tags);
    break;
  case nua_r_destroy:
    nua_stack_destroy_handle(nua, nh, tags);
    su_msg_destroy(nua->nua_signal);
    return;
  default:
    break;
  }

  if (error < 0) {
    nua_stack_event(nh->nh_nua, nh, NULL, event,
		    NUA_ERROR_AT(__FILE__, __LINE__), NULL);
  }

  su_msg_destroy(nua->nua_signal);
}

/* ====================================================================== */
/* Signal and event handling */

/** Get event data.
 *
 * @sa #nua_event_e, nua_event_save(), nua_saved_event_request(), nua_destroy_event().
 */
nua_event_data_t const *nua_event_data(nua_saved_event_t const saved[1])
{
  return saved && saved[0] ? su_msg_data(saved)->ee_data : NULL;
}

/** Destroy saved event.
 *
 * @sa #nua_event_e, nua_event_save(), nua_event_data(), nua_saved_event_request().
 */
void nua_destroy_event(nua_saved_event_t saved[1])
{
  if (saved) su_msg_destroy(saved);
}

/** @internal Move signal. */
void nua_move_signal(nua_saved_signal_t a[1], nua_saved_signal_t b[1])
{
  su_msg_save(a, b);
}

void nua_destroy_signal(nua_saved_signal_t saved[1])
{
  if (saved) su_msg_destroy(saved);
}

nua_signal_data_t const *nua_signal_data(nua_saved_signal_t const saved[1])
{
  return nua_event_data(saved);
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

    nh_next = nh->nh_next;

    busy += nua_dialog_repeat_shutdown(nh, ds);

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
	nua_dialog_usage_remove(nh, nh->nh_ds, nh->nh_ds->ds_usage, NULL, NULL);
      }
    }
    su_timer_destroy(nua->nua_timer), nua->nua_timer = NULL;
    nta_agent_destroy(nua->nua_nta), nua->nua_nta = NULL;
  }

  nua_stack_event(nua, NULL, NULL, nua_r_shutdown, status, phrase, NULL);
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

/** @internal Append a handle to the list of handles */
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
  if (nh->nh_notifier)
    nua_stack_terminate(nua, nh, 0, NULL);

  nua_dialog_shutdown(nh, nh->nh_ds);

  if (nh->nh_ref_by_user) {
    nh->nh_ref_by_user = 0;
    nua_handle_unref(nh);
  }

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

  if (nh->nh_notifier)
    nea_server_destroy(nh->nh_notifier), nh->nh_notifier = NULL;

  while (nh->nh_ds->ds_cr)
    nua_client_request_complete(nh->nh_ds->ds_cr);

  while (nh->nh_ds->ds_sr)
    nua_server_request_destroy(nh->nh_ds->ds_sr);

  nua_dialog_deinit(nh, nh->nh_ds);

  if (nh->nh_soa)
    soa_destroy(nh->nh_soa), nh->nh_soa = NULL;

  if (nh_is_inserted(nh))
    nh_remove(nua, nh);

  nua_handle_unref(nh);		/* Remove stack reference */
}

/* ======================================================================== */

/**@internal
 * Save handle parameters and initial authentication info.
 *
 * @retval -1 upon an error
 * @retval 0 when successful
 */
int nua_stack_init_handle(nua_t *nua, nua_handle_t *nh, tagi_t const *tags)
{
  int retval = 0;

  if (nh == NULL)
    return -1;

  assert(nh != nua->nua_dhandle);

  if (nua_stack_set_params(nua, nh, nua_i_error, tags) < 0)
    retval = -1;

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

  if (nh && nua_stack_init_handle(nua, nh, NULL) < 0)
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

nua_handle_t *nua_stack_handle_by_call_id(nua_t *nua, const char *call_id)
{
  if (nua) {
    nta_leg_t *leg = nta_leg_by_call_id(nua->nua_nta, call_id);
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

su_inline
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
  nua_client_request_t *cr = nh->nh_ds->ds_cr;
  int status = nh_authorize(nh, TAG_NEXT(tags));

  if (status > 0) {
    if (cr && cr->cr_wait_for_cred) {
      cr->cr_waiting = cr->cr_wait_for_cred = 0;
      nua_client_restart_request(cr, cr->cr_terminating, tags);
    }
    else {
      nua_stack_event(nua, nh, NULL, e,
		      202, "No operation to restart",
		      NULL);
    }
  }
  else if (cr && cr->cr_wait_for_cred) {
    cr->cr_waiting = cr->cr_wait_for_cred = 0;

    if (status < 0)
      nua_client_response(cr, 900, "Operation cannot add credentials", NULL);
    else
      nua_client_response(cr, 904, "Operation has no matching challenge ", NULL);
  }
  else if (status < 0) {
    nua_stack_event(nua, nh, NULL, e, 900, "Cannot add credentials", NULL);
  }
  else {
    nua_stack_event(nua, nh, NULL, e, 904, "No matching challenge", NULL);
  }
}


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
  else if (initial && sip->sip_to->a_tag) {
    /* RFC 3261 section 12.2.2:

       If the UAS wishes to reject the request because it does not wish to
       recreate the dialog, it MUST respond to the request with a 481
       (Call/Transaction Does Not Exist) status code and pass that to the
       server transaction.
    */
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
  if (sr == NULL)
    return;

  if (SR_HAS_SAVED_SIGNAL(sr))
    nua_destroy_signal(sr->sr_signal);

  if (sr->sr_irq)
    nta_incoming_destroy(sr->sr_irq), sr->sr_irq = NULL;

  if (sr->sr_request.msg)
    msg_destroy(sr->sr_request.msg), sr->sr_request.msg = NULL;

  if (sr->sr_response.msg)
    msg_destroy(sr->sr_response.msg), sr->sr_response.msg = NULL;

  if (sr->sr_prev) {
    /* Allocated from heap */
    if ((*sr->sr_prev = sr->sr_next))
      sr->sr_next->sr_prev = sr->sr_prev;
    su_free(sr->sr_owner->nh_home, sr);
  }
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
    assert(sr->sr_status == 500);
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
    nua_event_t e = sr->sr_methods->sm_event;
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

/* ---------------------------------------------------------------------- */

/**@internal
 *
 * @class nua_client_request
 *
 * Each handle has a queue of client-side requests; if a request is pending,
 * a new request from API is added to the queue. After the request is
 * complete, it is removed from the queue and destroyed by the default. The
 * exception is the client requests bound to a dialog usage: they are saved
 * and re-used when the dialog usage is refreshed (and sometimes when the
 * usage is terminated).
 *
 * The client request is subclassed and its behaviour modified using virtual
 * function table in #nua_client_methods_t.
 *
 * The first three methods (crm_template(), crm_init(), crm_send()) are
 * called when the request is sent first time.
 *
 * The crm_template() is called if a template request message is needed (for
 * example, in case of unregister, unsubscribe and unpublish, the template
 * message is taken from the request establishing the usage).
 *
 * The crm_init() is called when the template message and dialog leg has
 * been created and populated by the tags procided by the application. Its
 * parameters msg and sip are pointer to the template request message that
 * is saved in the nua_client_request::cr_msg field.
 *
 * The crm_send() is called with a copy of the template message that has
 * been populated with all the fields included in the request, including
 * @CSeq and @MaxForwards. The crm_send() function, such as
 * nua_publish_client_request(), usually calls nua_base_client_trequest() that
 * then creates the nta-level transaction.
 *
 * The response to the request is processed by crm_check_restart(), which
 * modifies and restarts the request when needed (e.g., when negotiating
 * expiration time). After the request has been suitably modified, e.g., the
 * expiration time has been increased, the restart function calls
 * nua_client_restart(), which restarts the request and relays the
 * intermediate response to the application with nua_client_restart() and
 * crm_report().
 *
 * The final responses are processed by crm_recv() and and preliminary ones
 * by crm_preliminary(). All virtual functions should call
 * nua_base_client_response() beside method-specific processing.
 *
 * The nua_base_client_response() relays the response to the application with
 * nua_client_restart() and crm_report().
 *
 * @par Terminating Dialog Usages and Dialogs
 *
 * The response is marked as terminating with nua_client_set_terminating().
 * When a terminating request completes the dialog usage is removed and the
 * dialog is destroyed (unless there is an another active usage).
 */
static void nua_client_request_destroy(nua_client_request_t *cr);
static int nua_client_init_request0(nua_client_request_t *cr);
static int nua_client_request_try(nua_client_request_t *cr);
static int nua_client_request_sendmsg(nua_client_request_t *cr,
  				      msg_t *msg, sip_t *sip);
static void nua_client_restart_after(su_root_magic_t *magic,
				     su_timer_t *timer,
				     nua_client_request_t *cr);

/**Create a client request.
 *
 * @retval 0 if request is pending
 * @retval > 0 if error event has been sent
 * @retval < 0 upon an error
 */
int nua_client_create(nua_handle_t *nh,
		      int event,
		      nua_client_methods_t const *methods,
		      tagi_t const * const tags)
{
  su_home_t *home = nh->nh_home;
  nua_client_request_t *cr;
  sip_method_t method;
  char const *name;

  method = methods->crm_method, name = methods->crm_method_name;
  if (!name) {
    tagi_t const *t = tl_find_last(tags, nutag_method);
    if (t)
      name = (char const *)t->t_value;
  }

  cr = su_zalloc(home, sizeof *cr + methods->crm_extra);
  if (!cr) {
    return nua_stack_event(nh->nh_nua, nh,
			   NULL,
			   event,
			   NUA_ERROR_AT(__FILE__, __LINE__),
			   NULL);
  }

  cr->cr_methods = methods;
  cr->cr_event = event;
  cr->cr_method = method;
  cr->cr_method_name = name;
  cr->cr_contactize = methods->crm_flags.target_refresh;
  cr->cr_dialog = methods->crm_flags.create_dialog;
  cr->cr_auto = 1;

  if (su_msg_is_non_null(nh->nh_nua->nua_signal)) {
    nua_event_data_t *e = su_msg_data(nh->nh_nua->nua_signal)->ee_data;

    if (tags == e->e_tags && event == e->e_event) {
      cr->cr_auto = 0;

      if (tags) {
	nua_move_signal(cr->cr_signal, nh->nh_nua->nua_signal);
	if (cr->cr_signal) {
	  /* Steal reference from signal */
	  cr->cr_owner = e->e_nh, e->e_nh = NULL;
	  cr->cr_tags = tags;
	}
      }
    }
  }

  if (cr->cr_owner == NULL)
    cr->cr_owner = nua_handle_ref(nh);

  if (tags && cr->cr_tags == NULL)
    cr->cr_tags = tl_tlist(nh->nh_home, TAG_NEXT(tags));

#if HAVE_MEMLEAK_LOG
  SU_DEBUG_0(("%p %s() for %s\n", cr, __func__, cr->cr_methods->crm_method_name));
#endif

  if (nua_client_request_queue(cr))
    return 0;

  return nua_client_init_request(cr);
}

int nua_client_tcreate(nua_handle_t *nh,
		       int event,
		       nua_client_methods_t const *methods,
		       tag_type_t tag, tag_value_t value, ...)
{
  int retval;
  ta_list ta;
  ta_start(ta, tag, value);
  retval = nua_client_create(nh, event, methods, ta_args(ta));
  ta_end(ta);
  return retval;
}

#if HAVE_MEMLEAK_LOG
nua_client_request_t *
nua_client_request_ref_by(nua_client_request_t *cr,
			  char const *where, unsigned line, char const *who)
{
  SU_DEBUG_0(("%p ref %s to %u by %s:%u: %s()\n",
	      cr, cr->cr_methods->crm_method_name,
	      ++(cr->cr_refs), where, line, who));
  return cr;
}

int nua_client_request_unref_by(nua_client_request_t *cr,
				char const *where, unsigned line, char const *who)
{
  SU_DEBUG_0(("%p unref %s to %u by %s:%u: %s()\n",
	      cr, cr->cr_methods->crm_method_name,
	      cr->cr_refs - 1, where, line, who));

  if (cr->cr_refs > 1) {
    cr->cr_refs--;
    return 0;
  }
  else {
    cr->cr_refs = 0;
    nua_client_request_destroy(cr);
    return 1;
  }
}
#else
nua_client_request_t *nua_client_request_ref(nua_client_request_t *cr)
{
  cr->cr_refs++;
  return cr;
}

int nua_client_request_unref(nua_client_request_t *cr)
{
  if (cr->cr_refs > 1) {
    cr->cr_refs--;
    return 0;
  }
  else {
    cr->cr_refs = 0;
    nua_client_request_destroy(cr);
    return 1;
  }
}
#endif

int nua_client_request_queue(nua_client_request_t *cr)
{
  int queued = 0;
  nua_client_request_t **queue = &cr->cr_owner->nh_ds->ds_cr;

  assert(cr->cr_prev == NULL && cr->cr_next == NULL);

  cr->cr_status = 0;

  nua_client_request_ref(cr);

  if (cr->cr_method != sip_method_invite &&
      cr->cr_method != sip_method_cancel) {
    while (*queue) {
      if ((*queue)->cr_method == sip_method_invite ||
	  (*queue)->cr_method == sip_method_cancel)
	break;
      queue = &(*queue)->cr_next;
      queued = 1;
    }
  }
  else {
    while (*queue) {
      queue = &(*queue)->cr_next;
      if (cr->cr_method == sip_method_invite)
	queued = 1;
    }
  }

  if ((cr->cr_next = *queue))
    cr->cr_next->cr_prev = &cr->cr_next;

  cr->cr_prev = queue, *queue = cr;

  return queued;
}

int
nua_client_request_remove(nua_client_request_t *cr)
{
  int retval = 0;
  int in_queue = cr->cr_prev != NULL;

  if (in_queue) {
    if ((*cr->cr_prev = cr->cr_next))
      cr->cr_next->cr_prev = cr->cr_prev;
  }
  cr->cr_prev = NULL, cr->cr_next = NULL;

  if (cr->cr_timer) {
    su_timer_destroy(cr->cr_timer), cr->cr_timer = NULL;
    retval = nua_client_request_unref(cr);
  }

  if (!in_queue)
    return retval;

  return nua_client_request_unref(cr);
}

int
nua_client_request_clean(nua_client_request_t *cr)
{
  if (cr->cr_orq) {
    nta_outgoing_destroy(cr->cr_orq), cr->cr_orq = NULL, cr->cr_acked = 0;
    return nua_client_request_unref(cr);
  }
  return 0;
}

static int
nua_client_request_complete(nua_client_request_t *cr)
{
  if (cr->cr_orq) {
    nua_client_request_ref(cr);
    if (cr && cr->cr_methods->crm_complete)
      cr->cr_methods->crm_complete(cr);
    nua_client_request_clean(cr);
    if (nua_client_request_unref(cr))
      return 1;
  }

  return nua_client_request_remove(cr);
}

static void
nua_client_request_destroy(nua_client_request_t *cr)
{
  nua_handle_t *nh;

  if (cr == NULL)
    return;

  /* Possible references: */
  assert(cr->cr_prev == NULL);	/* queue */
  assert(cr->cr_orq == NULL);	/* transaction callback */
  assert(cr->cr_timer == NULL);	/* timer callback */

  nh = cr->cr_owner;

  nua_destroy_signal(cr->cr_signal);

  nua_client_bind(cr, NULL);

#if HAVE_MEMLEAK_LOG
  SU_DEBUG_0(("%p %s for %s\n", cr, __func__, cr->cr_methods->crm_method_name));
#endif

  if (cr->cr_msg)
    msg_destroy(cr->cr_msg);
  cr->cr_msg = NULL, cr->cr_sip = NULL;

  if (cr->cr_orq)
    nta_outgoing_destroy(cr->cr_orq), cr->cr_orq = NULL;

  if (cr->cr_target)
    su_free(nh->nh_home, cr->cr_target);

  su_free(nh->nh_home, cr);

  nua_handle_unref(nh);
}

/** Bind client request to a dialog usage */
int nua_client_bind(nua_client_request_t *cr, nua_dialog_usage_t *du)
{
  assert(cr);
  if (cr == NULL)
    return -1;

  if (du == NULL) {
    du = cr->cr_usage;
    cr->cr_usage = NULL;
    if (du && du->du_cr == cr) {
      du->du_cr = NULL;
      nua_client_request_unref(cr);
    }
    return 0;
  }

  if (du->du_cr && cr == du->du_cr)
    return 0;

  if (du->du_cr) {
    nua_client_bind(du->du_cr, NULL);
  }

  du->du_cr = nua_client_request_ref(cr), cr->cr_usage = du;

  return 0;
}

/**Initialize client request for sending.
 *
 * This function is called when the request is taken from queue and sent.
 *
 * @retval 0 if request is pending
 * @retval >=1 if error event has been sent
 */
int nua_client_init_request(nua_client_request_t *cr)
{
  int retval;
  nua_client_request_ref(cr);
  retval = nua_client_init_request0(cr);
  nua_client_request_unref(cr);
  return retval;
}

/**Initialize client request for sending.
 *
 * This function is called when the request is taken from queue and sent.
 *
 * @retval 0 if request is pending
 * @retval >=1 if error event has been sent
 */
static
int nua_client_init_request0(nua_client_request_t *cr)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_t *nua = nh->nh_nua;
  nua_dialog_state_t *ds = nh->nh_ds;
  msg_t *msg = NULL;
  sip_t *sip;
  url_string_t const *url = NULL;
  tagi_t const *t;
  int has_contact = 0;
  int error = 0;

  if (!cr->cr_method_name)
    return nua_client_return(cr, NUA_ERROR_AT(__FILE__, __LINE__), NULL);

  if (cr->cr_msg)
    return nua_client_request_try(cr);

  cr->cr_answer_recv = 0, cr->cr_offer_sent = 0;
  cr->cr_offer_recv = 0, cr->cr_answer_sent = 0;
  cr->cr_terminated = 0, cr->cr_graceful = 0;

  nua_stack_init_handle(nua, nh, cr->cr_tags);

  if (cr->cr_method == sip_method_cancel) {
    if (cr->cr_methods->crm_init) {
      error = cr->cr_methods->crm_init(cr, NULL, NULL, cr->cr_tags);
      if (error)
	return error;
    }

    if (cr->cr_methods->crm_send)
      return cr->cr_methods->crm_send(cr, NULL, NULL, cr->cr_tags);
    else
      return nua_base_client_request(cr, NULL, NULL, cr->cr_tags);
  }

  if (!cr->cr_methods->crm_template ||
      cr->cr_methods->crm_template(cr, &msg, cr->cr_tags) == 0)
    msg = nua_client_request_template(cr);

  sip = sip_object(msg);
  if (!sip)
    return nua_client_return(cr, NUA_ERROR_AT(__FILE__, __LINE__), msg);

  if (nh->nh_tags) {
    for (t = nh->nh_tags; t; t = t_next(t)) {
      if (t->t_tag == siptag_contact ||
	  t->t_tag == siptag_contact_str)
	has_contact = 1;
      else if (t->t_tag == nutag_url)
	url = (url_string_t const *)t->t_value;
    }
  }

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
  for (t = cr->cr_tags; t; t = t_next(t)) {
    if (t->t_tag == siptag_contact ||
	t->t_tag == siptag_contact_str)
      has_contact = 1;
    else if (t->t_tag == nutag_url)
      url = (url_string_t const *)t->t_value;
    else if (t->t_tag == nutag_dialog) {
      cr->cr_dialog = t->t_value > 1;
      cr->cr_contactize = t->t_value >= 1;
    }
    else if (t->t_tag == nutag_auth && t->t_value) {
      /* XXX ignoring errors */
      if (nh->nh_auth)
	auc_credentials(&nh->nh_auth, nh->nh_home, (char *)t->t_value);
    }
  }

  if (cr->cr_method == sip_method_register && url == NULL)
    url = (url_string_t const *)NH_PGET(nh, registrar);

  if ((t = cr->cr_tags)) {
    if (sip_add_tagis(msg, sip, &t) < 0)
      return nua_client_return(cr, NUA_ERROR_AT(__FILE__, __LINE__), msg);
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
    if (ds->ds_remote_tag && ds->ds_remote_tag[0] &&
	sip_to_tag(nh->nh_home, sip->sip_to, ds->ds_remote_tag) < 0)
      return nua_client_return(cr, NUA_ERROR_AT(__FILE__, __LINE__), msg);

    if (sip->sip_from == NULL &&
	sip_add_dup(msg, sip, (sip_header_t *)nua->nua_from) < 0)
      return nua_client_return(cr, NUA_ERROR_AT(__FILE__, __LINE__), msg);

    if (sip->sip_to == NULL && cr->cr_method == sip_method_register &&
      sip_add_dup_as(msg, sip, sip_to_class,
		     (sip_header_t *)sip->sip_from) < 0) {
      return nua_client_return(cr, NUA_ERROR_AT(__FILE__, __LINE__), msg);
    }

    if (cr->cr_dialog) {
      ds->ds_leg = nta_leg_tcreate(nua->nua_nta,
				   nua_stack_process_request, nh,
				   SIPTAG_CALL_ID(sip->sip_call_id),
				   SIPTAG_FROM(sip->sip_from),
				   SIPTAG_TO(sip->sip_to),
				   SIPTAG_CSEQ(sip->sip_cseq),
				   TAG_END());
      if (!ds->ds_leg)
	return nua_client_return(cr, NUA_ERROR_AT(__FILE__, __LINE__), msg);

      if (!sip->sip_from->a_tag &&
	  sip_from_tag(msg_home(msg), sip->sip_from,
		       nta_leg_tag(ds->ds_leg, NULL)) < 0)
	return nua_client_return(cr, NUA_ERROR_AT(__FILE__, __LINE__), msg);
    }
  }
  else {
    if (ds->ds_route)
      url = NULL;
  }

  if (url && nua_client_set_target(cr, (url_t *)url) < 0)
    return nua_client_return(cr, NUA_ERROR_AT(__FILE__, __LINE__), msg);

  cr->cr_has_contact = has_contact;

  if (cr->cr_methods->crm_init) {
    error = cr->cr_methods->crm_init(cr, msg, sip, cr->cr_tags);
    if (error < -1)
      msg = NULL;
    if (error < 0)
      return nua_client_return(cr, NUA_ERROR_AT(__FILE__, __LINE__), msg);
    if (error != 0)
      return error;
  }

  cr->cr_msg = msg;
  cr->cr_sip = sip;

  return nua_client_request_try(cr);
}

msg_t *nua_client_request_template(nua_client_request_t *cr)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_t *nua = nh->nh_nua;
  nua_dialog_state_t *ds = nh->nh_ds;

  msg_t *msg = nta_msg_create(nua->nua_nta, 0);
  sip_t *sip = sip_object(msg);

  if (!sip)
    return NULL;

  if (nh->nh_tags) {
    tagi_t const *t = nh->nh_tags;

    /* Use the From header from the dialog.
       If From is set, it is always first tag in the handle */
    if (ds->ds_leg && t->t_tag == siptag_from)
      t++;

    /* When the INVITE message (or any other SIP message) is
     * created, the tagged values saved with nua_handle() are used first. */
    sip_add_tagis(msg, sip, &t);
  }

  return msg;
}


/** Restart the request message.
 *
 * A restarted request has not been completed successfully.
 *
 * @retval 0 if request is pending
 * @retval >=1 if error event has been sent
 */
int nua_client_restart_request(nua_client_request_t *cr,
			      int terminating,
			      tagi_t const *tags)
{
  if (cr) {
    assert(nua_client_is_queued(cr));

    if (tags && cr->cr_msg)
      if (sip_add_tagis(cr->cr_msg, NULL, &tags) < 0)
	/* XXX */;

    nua_client_set_terminating(cr, terminating);

    return nua_client_request_try(cr);
  }

  return 0;
}

/** Resend the request message.
 *
 * A resent request has completed once successfully - restarted has not.
 *
 * @retval 0 if request is pending
 * @retval >=1 if error event has been sent
 */
int nua_client_resend_request(nua_client_request_t *cr,
			      int terminating)
{
  if (cr) {
    cr->cr_retry_count = 0;
    cr->cr_challenged = 0;

    if (nua_client_is_queued(cr)) {
      if (terminating)
	cr->cr_graceful = 1;
      return 0;
    }

    if (terminating)
      nua_client_set_terminating(cr, terminating);

    if (nua_client_request_queue(cr))
      return 0;
    if (nua_dialog_is_reporting(cr->cr_owner->nh_ds))
      return 0;
    return nua_client_request_try(cr);
  }
  return 0;
}


/** Create a request message and send it.
 *
 * If an error occurs, send error event to the application.
 *
 * @retval 0 if request is pending
 * @retval >=1 if error event has been sent
 */
static
int nua_client_request_try(nua_client_request_t *cr)
{
  int error = -1;
  msg_t *msg = msg_copy(cr->cr_msg);
  sip_t *sip = sip_object(msg);

  cr->cr_answer_recv = 0, cr->cr_offer_sent = 0;
  cr->cr_offer_recv = 0, cr->cr_answer_sent = 0;

  if (msg && sip) {
    error = nua_client_request_sendmsg(cr, msg, sip);
    if (!error)
      return 0;

    if (error == -1)
      msg_destroy(msg);
  }

  if (error < 0)
    error = nua_client_response(cr, NUA_ERROR_AT(__FILE__, __LINE__), NULL);

  assert(error > 0);
  return error;
}

/**Send a request message.
 *
 * @retval 0 if request is pending
 * @retval >=1 if error event has been sent
 * @retval -1 if error occurred but event has not been sent,
               and @a msg has not been destroyed
 * @retval -2 if error occurred, event has not been sent,
 *            but @a msg has been destroyed
 */
static
int nua_client_request_sendmsg(nua_client_request_t *cr, msg_t *msg, sip_t *sip)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_state_t *ds = nh->nh_ds;
  sip_method_t method = cr->cr_method;
  char const *name = cr->cr_method_name;
  url_string_t const *url = (url_string_t *)cr->cr_target;
  nta_leg_t *leg;

  assert(cr->cr_orq == NULL);

  cr->cr_retry_count++;

  if (ds->ds_leg)
    leg = ds->ds_leg;
  else
    leg = nh->nh_nua->nua_dhandle->nh_ds->ds_leg; /* Default leg */

  if (nua_dialog_is_established(ds)) {
    while (sip->sip_route)
      sip_route_remove(msg, sip);
  }
  else if (!ds->ds_route) {
    sip_route_t *initial_route = NH_PGET(nh, initial_route);

    if (initial_route) {
      initial_route = sip_route_dup(msg_home(msg), initial_route);
      if (!initial_route) return -1;
      msg_header_prepend(msg, (msg_pub_t*)sip,
			 /* This should be
			    (msg_header_t **)&sip->sip_route
			  * but directly casting pointer &sip->sip_route gives
			  * spurious type-punning warning */
			 (msg_header_t **)((char *)sip + offsetof(sip_t, sip_route)),
			 (msg_header_t *)initial_route);
    }
  }


  /**
   * For in-dialog requests, the request URI is taken from the @Contact
   * header received from the remote party during dialog establishment,
   * and the NUTAG_URL() is ignored.
   *
   * Also, the @CallID and @CSeq headers and @From and @To tags are
   * generated based on the dialog information and added to the request.
   * If the dialog has a route, it is added to the request, too.
   */
  if (nta_msg_request_complete(msg, leg, method, name, url) < 0)
    return -1;

  /**@MaxForwards header (with default value set by NTATAG_MAX_FORWARDS()) is
   * also added now, if it does not exist.
   */

  if (!ds->ds_remote)
    ds->ds_remote = sip_to_dup(nh->nh_home, sip->sip_to);
  if (!ds->ds_local)
    ds->ds_local = sip_from_dup(nh->nh_home, sip->sip_from);

  /**
   * Next, values previously set with nua_set_params() or nua_set_hparams()
   * are used: @Allow, @Supported, @Organization, @UserAgent and
   * @AllowEvents headers are added to the request if they are not already
   * set.
   */
  if (!sip->sip_allow)
    sip_add_dup(msg, sip, (sip_header_t*)NH_PGET(nh, allow));

  if (!sip->sip_supported && NH_PGET(nh, supported))
    sip_add_dup(msg, sip, (sip_header_t *)NH_PGET(nh, supported));

  if (method == sip_method_register && NH_PGET(nh, path_enable) &&
      !sip_has_feature(sip->sip_supported, "path") &&
      !sip_has_feature(sip->sip_require, "path"))
    sip_add_make(msg, sip, sip_supported_class, "path");

  if (!sip->sip_organization && NH_PGET(nh, organization))
    sip_add_make(msg, sip, sip_organization_class, NH_PGET(nh, organization));

  if (!sip->sip_user_agent && NH_PGET(nh, user_agent))
    sip_add_make(msg, sip, sip_user_agent_class, NH_PGET(nh, user_agent));

  /** Any node implementing one or more event packages SHOULD include an
   * appropriate @AllowEvents header indicating all supported events in
   * all methods which initiate dialogs and their responses (such as
   * INVITE) and OPTIONS responses.
   */
  if (!sip->sip_allow_events &&
      NH_PGET(nh, allow_events) &&
      (method == sip_method_notify || /* Always in NOTIFY */
       (!ds->ds_remote_tag &&	      /* And in initial requests */
	(method == sip_method_subscribe || method == sip_method_refer ||
	 method == sip_method_options ||
	 method == sip_method_invite))))
    sip_add_dup(msg, sip, (void *)NH_PGET(nh, allow_events));

  /**
   * Next, the stack generates a @Contact header for the request (unless
   * the application already gave a @Contact header or it does not want to
   * use @Contact and indicates that by including SIPTAG_CONTACT(NULL) or
   * SIPTAG_CONTACT(SIP_NONE) in the tagged parameters.) If the
   * application has registered the URI in @From header, the @Contact
   * header used with registration is used. Otherwise, the @Contact header
   * is generated from the local IP address and port number.
   */

  /**For the initial requests, @ServiceRoute set that was received from the
   * registrar is also added to the request message.
   */
  if (cr->cr_method != sip_method_register) {
    if (cr->cr_contactize && cr->cr_has_contact) {
      sip_contact_t *ltarget = sip_contact_dup(nh->nh_home, sip->sip_contact);
      if (ds->ds_ltarget)
	msg_header_free(nh->nh_home, (msg_header_t *)ds->ds_ltarget);
      ds->ds_ltarget = ltarget;
    }

    if (ds->ds_ltarget && !cr->cr_has_contact)
      sip_add_dup(msg, sip, (sip_header_t *)ds->ds_ltarget);

    if (nua_registration_add_contact_to_request(nh, msg, sip,
						cr->cr_contactize &&
						!cr->cr_has_contact &&
						!ds->ds_ltarget,
						!ds->ds_route) < 0)
      return -1;
  }

  cr->cr_wait_for_cred = 0;

  if (cr->cr_methods->crm_send)
    return cr->cr_methods->crm_send(cr, msg, sip, NULL);

  return nua_base_client_request(cr, msg, sip, NULL);
}

/**Add tags to request message and send it,
 *
 * @retval 0 success
 * @retval -1 if error occurred, but event has not been sent
 * @retval -2 if error occurred, event has not been sent,
 *            and @a msg has been destroyed
 * @retval >=1 if error event has been sent
 */
int nua_base_client_trequest(nua_client_request_t *cr,
			     msg_t *msg, sip_t *sip,
			     tag_type_t tag, tag_value_t value, ...)
{
  int retval;
  ta_list ta;
  ta_start(ta, tag, value);
  retval = nua_base_client_request(cr, msg, sip, ta_args(ta));
  ta_end(ta);
  return retval;
}

/** Send request.
 *
 * @retval 0 success
 * @retval -1 if error occurred, but event has not been sent
 * @retval -2 if error occurred, event has not been sent,
 *            and @a msg has been destroyed
 * @retval >=1 if error event has been sent
 */
int nua_base_client_request(nua_client_request_t *cr, msg_t *msg, sip_t *sip,
			    tagi_t const *tags)
{
  nua_handle_t *nh = cr->cr_owner;
  int proxy_is_set = NH_PISSET(nh, proxy);
  url_string_t * proxy = NH_PGET(nh, proxy);

  if (nh->nh_auth) {
    if (cr->cr_challenged ||
	NH_PGET(nh, auth_cache) == nua_auth_cache_dialog) {
      if (auc_authorize(&nh->nh_auth, msg, sip) < 0)
	return nua_client_return(cr, 900, "Cannot add credentials", msg);
    }
  }

  cr->cr_seq = sip->sip_cseq->cs_seq; /* Save last sequence number */

  assert(cr->cr_orq == NULL);

  cr->cr_orq = nta_outgoing_mcreate(nh->nh_nua->nua_nta,
				    nua_client_orq_response,
				    nua_client_request_ref(cr),
				    NULL,
				    msg,
				    TAG_IF(proxy_is_set,
					   NTATAG_DEFAULT_PROXY(proxy)),
				    TAG_NEXT(tags));

  if (cr->cr_orq == NULL) {
    nua_client_request_unref(cr);
    return -1;
  }

  return 0;
}

/** Callback for nta client transaction */
int nua_client_orq_response(nua_client_request_t *cr,
			    nta_outgoing_t *orq,
			    sip_t const *sip)
{
  int status;
  char const *phrase;

  if (sip && sip->sip_status) {
    status = sip->sip_status->st_status;
    phrase = sip->sip_status->st_phrase;
  }
  else {
    status = nta_outgoing_status(orq);
    phrase = "";
  }

  nua_client_response(cr, status, phrase, sip);

  return 0;
}

/**Return response to the client request.
 *
 * Return a response generated by the stack. This function is used to return
 * a error response within @a nua_client_methods_t#crm_init or @a
 * nua_client_methods_t#crm_send functions. It takes care of disposing the @a
 * to_be_destroyed that cannot be sent.
 *
 * @retval 0 if response event was preliminary
 * @retval 1 if response event was final
 * @retval 2 if response event destroyed the handle, too.
 */
int nua_client_return(nua_client_request_t *cr,
		      int status,
		      char const *phrase,
		      msg_t *to_be_destroyed)
{
  if (to_be_destroyed)
    msg_destroy(to_be_destroyed);
  nua_client_response(cr, status, phrase, NULL);
  return 1;
}

/** Process response to the client request.
 *
 * The response can be generated by the stack (@a sip is NULL) or
 * returned by the remote server.
 *
 * @retval 0 if response event was preliminary
 * @retval 1 if response event was final
 * @retval 2 if response event destroyed the handle, too.
 */
int nua_client_response(nua_client_request_t *cr,
			int status,
			char const *phrase,
			sip_t const *sip)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du = cr->cr_usage;
  int retval = 0;

  if (cr->cr_restarting)
    return 0;

  nua_client_request_ref(cr);

  cr->cr_status = status;
  cr->cr_phrase = phrase;

  if (status < 200) {
    /* Xyzzy */
  }
  else if (sip && nua_client_check_restart(cr, status, phrase, sip)) {
    nua_client_request_unref(cr);
    return 0;
  }
  else if (status < 300) {
    if (cr->cr_terminating) {
      cr->cr_terminated = 1;
    }
    else {
      if (sip) {
	if (cr->cr_contactize)
	  nua_dialog_uac_route(nh, nh->nh_ds, sip, 1);
	nua_dialog_store_peer_info(nh, nh->nh_ds, sip);
      }

      if (du && du->du_cr == cr)
	du->du_ready = 1;
    }
  }
  else {
    sip_method_t method = cr->cr_method;
    int terminated, graceful = 1;

    if (status < 700)
      terminated = sip_response_terminates_dialog(status, method, &graceful);
    else
      /* XXX - terminate usage by all internal error responses */
      terminated = 0, graceful = 1;

    if (terminated < 0)
      cr->cr_terminated = terminated;
    else if (cr->cr_terminating || terminated)
      cr->cr_terminated = 1;
    else if (graceful)
      cr->cr_graceful = 1;
  }

  if (status < 200) {
    if (cr->cr_methods->crm_preliminary)
      cr->cr_methods->crm_preliminary(cr, status, phrase, sip);
    else
      nua_base_client_response(cr, status, phrase, sip, NULL);
    cr->cr_phrase = NULL;
  }
  else {
    if (cr->cr_methods->crm_recv)
      retval = cr->cr_methods->crm_recv(cr, status, phrase, sip);
    else
      retval = nua_base_client_response(cr, status, phrase, sip, NULL);
  }

  nua_client_request_unref(cr);

  return retval;
}

/** Check if request should be restarted.
 *
 * @retval 1 if restarted or waiting for restart
 * @retval 0 otherwise
 */
int nua_client_check_restart(nua_client_request_t *cr,
			     int status,
			     char const *phrase,
			     sip_t const *sip)
{
  nua_handle_t *nh;

  assert(cr && status >= 200 && phrase && sip);

  nh = cr->cr_owner;

  if (cr->cr_retry_count > NH_PGET(nh, retry_count))
    return 0;

  if (cr->cr_methods->crm_check_restart)
    return cr->cr_methods->crm_check_restart(cr, status, phrase, sip);
  else
    return nua_base_client_check_restart(cr, status, phrase, sip);
}

int nua_base_client_check_restart(nua_client_request_t *cr,
				  int status,
				  char const *phrase,
				  sip_t const *sip)
{
  nua_handle_t *nh = cr->cr_owner;
  nta_outgoing_t *orq;

  if (status == 302 || status == 305) {
    sip_route_t r[1];

    if (!can_redirect(sip->sip_contact, cr->cr_method))
      return 0;

    switch (status) {
    case 302:
      if (nua_dialog_zap(nh, nh->nh_ds) == 0 &&
	  nua_client_set_target(cr, sip->sip_contact->m_url) >= 0)
	return nua_client_restart(cr, 100, "Redirected");
      break;

    case 305:
      sip_route_init(r);
      *r->r_url = *sip->sip_contact->m_url;
      if (nua_dialog_zap(nh, nh->nh_ds) == 0 &&
	  sip_add_dup(cr->cr_msg, cr->cr_sip, (sip_header_t *)r) >= 0)
	return nua_client_restart(cr, 100, "Redirected via a proxy");
      break;

    default:
      break;
    }
  }


  if (status == 423) {
    unsigned my_expires = 0;

    if (cr->cr_sip->sip_expires)
      my_expires = cr->cr_sip->sip_expires->ex_delta;

    if (sip->sip_min_expires &&
	sip->sip_min_expires->me_delta > my_expires) {
      sip_expires_t ex[1];
      sip_expires_init(ex);
      ex->ex_delta = sip->sip_min_expires->me_delta;

      if (sip_add_dup(cr->cr_msg, NULL, (sip_header_t *)ex) < 0)
	return 0;

      return nua_client_restart(cr, 100, "Re-Negotiating Expiration");
    }
  }

  if ((status == 401 && sip->sip_www_authenticate) ||
      (status == 407 && sip->sip_proxy_authenticate)) {
    int server = 0, proxy = 0;

    if (sip->sip_www_authenticate)
      server = auc_challenge(&nh->nh_auth, nh->nh_home,
			     sip->sip_www_authenticate,
			     sip_authorization_class);

    if (sip->sip_proxy_authenticate)
      proxy = auc_challenge(&nh->nh_auth, nh->nh_home,
			    sip->sip_proxy_authenticate,
			    sip_proxy_authorization_class);

    if (server >= 0 && proxy >= 0) {
      int invalid = cr->cr_challenged && server + proxy == 0;

      cr->cr_challenged = 1;

      if (invalid) {
	/* Bad username/password */
	SU_DEBUG_7(("nua(%p): bad credentials, clearing them\n", (void *)nh));
	auc_clear_credentials(&nh->nh_auth, NULL, NULL);
      }
      else if (auc_has_authorization(&nh->nh_auth))
	return nua_client_restart(cr, 100, "Request Authorized by Cache");

      orq = cr->cr_orq, cr->cr_orq = NULL;

      cr->cr_waiting = cr->cr_wait_for_cred = 1;
      nua_client_report(cr, status, phrase, NULL, orq, NULL);
      nta_outgoing_destroy(orq);
      cr->cr_status = 0, cr->cr_phrase = NULL;
      nua_client_request_unref(cr);

      return 1;
    }
  }

  if (500 <= status && status < 600 &&
      sip->sip_retry_after &&
      sip->sip_retry_after->af_delta < 32) {
    su_timer_t *timer;
    char phrase[18];		/* Retry After XXXX\0 */

    timer = su_timer_create(su_root_task(nh->nh_nua->nua_root), 0);

    if (su_timer_set_interval(timer, nua_client_restart_after, cr,
			      sip->sip_retry_after->af_delta * 1000) < 0) {
      su_timer_destroy(timer);
      return 0; /* Too bad */
    }

    cr->cr_timer = timer;	/* This takes over reference from orq */

    snprintf(phrase, sizeof phrase, "Retry After %u",
	     (unsigned)sip->sip_retry_after->af_delta);

    orq = cr->cr_orq, cr->cr_orq = NULL;
    cr->cr_waiting = 1;
    nua_client_report(cr, 100, phrase, NULL, orq, NULL);
    nta_outgoing_destroy(orq);
    cr->cr_status = 0, cr->cr_phrase = NULL;
    return 1;
  }

  return 0;  /* This was a final response that cannot be restarted. */
}

/** Request restarted by timer */
static
void nua_client_restart_after(su_root_magic_t *magic,
			      su_timer_t *timer,
			      nua_client_request_t *cr)
{
  cr->cr_waiting = 0;
  su_timer_destroy(cr->cr_timer), cr->cr_timer = NULL;
  nua_client_restart_request(cr, cr->cr_terminating, NULL);
  nua_client_request_unref(cr);
}

/** Restart request.
 *
 * @retval 1 if restarted
 * @retval 0 otherwise
 */
int nua_client_restart(nua_client_request_t *cr,
		       int status, char const *phrase)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_state_t *ds = nh->nh_ds;
  nta_outgoing_t *orq;
  int error = -1, terminated, graceful;
  msg_t *msg = NULL;
  sip_t *sip = NULL;

  if (cr->cr_retry_count > NH_PGET(nh, retry_count))
    return 0;

  orq = cr->cr_orq, cr->cr_orq = NULL;  assert(orq);
  terminated = cr->cr_terminated, cr->cr_terminated = 0;
  graceful = cr->cr_graceful, cr->cr_graceful = 0;
  cr->cr_offer_sent = cr->cr_answer_recv = 0;
  cr->cr_offer_recv = cr->cr_answer_sent = 0;

  if (!ds->ds_leg && cr->cr_dialog) {
    sip_t const *sip = cr->cr_sip;
    ds->ds_leg = nta_leg_tcreate(nh->nh_nua->nua_nta,
				 nua_stack_process_request, nh,
				 SIPTAG_CALL_ID(sip->sip_call_id),
				 SIPTAG_FROM(sip->sip_from),
				 SIPTAG_TO(sip->sip_to),
				 SIPTAG_CSEQ(sip->sip_cseq),
				 TAG_END());
  }

  if (ds->ds_leg || !cr->cr_dialog) {
    msg = msg_copy(cr->cr_msg);
    sip = sip_object(msg);
  }

  if (msg && sip) {
    cr->cr_restarting = 1;
    error = nua_client_request_sendmsg(cr, msg, sip);
    cr->cr_restarting = 0;
    if (error !=0 && error != -2)
      msg_destroy(msg);
  }

  if (error) {
    cr->cr_graceful = graceful;
    cr->cr_terminated = terminated;
    assert(cr->cr_orq == NULL);
    cr->cr_orq = orq;
    return 0;
  }

  nua_client_report(cr, status, phrase, NULL, orq, NULL);

  nta_outgoing_destroy(orq);
  nua_client_request_unref(cr);	/* ... reference used by old orq */

  return 1;
}

int nua_client_set_target(nua_client_request_t *cr, url_t const *target)
{
  url_t *new_target, *old_target = cr->cr_target;

  if (!target || target == old_target)
    return 0;

  new_target = url_hdup(cr->cr_owner->nh_home, (url_t *)target);
  if (!new_target)
    return -1;
  cr->cr_target = new_target;
  if (old_target)
    su_free(cr->cr_owner->nh_home, old_target);

  return 0;
}

/**@internal
 * Relay response event to the application.
 *
 * @todo
 * If handle has already been marked as destroyed by nua_handle_destroy(),
 * release the handle with nh_destroy().
 *
 * @retval 0 if event was preliminary
 * @retval 1 if event was final
 * @retval 2 if event destroyed the handle, too.
 */
int nua_base_client_tresponse(nua_client_request_t *cr,
			      int status, char const *phrase,
			      sip_t const *sip,
			      tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  int retval;

  if (cr->cr_event == nua_r_destroy)
    return nua_base_client_response(cr, status, phrase, sip, NULL);

  ta_start(ta, tag, value);
  retval = nua_base_client_response(cr, status, phrase, sip, ta_args(ta));
  ta_end(ta);

  return retval;
}

/**@internal
 * Relay response event to the application.
 *
 * @todo
 * If handle has already been marked as destroyed by nua_handle_destroy(),
 * release the handle with nh_destroy().
 *
 * @retval 0 if event was preliminary
 * @retval 1 if event was final
 * @retval 2 if event destroyed the handle, too.
 */
int nua_base_client_response(nua_client_request_t *cr,
			     int status, char const *phrase,
			     sip_t const *sip,
			     tagi_t const *tags)
{
  nua_handle_t *nh = cr->cr_owner;
  sip_method_t method = cr->cr_method;
  nua_dialog_usage_t *du;

  cr->cr_reporting = 1, nh->nh_ds->ds_reporting = 1;

  if (nh->nh_auth && sip &&
      (sip->sip_authentication_info || sip->sip_proxy_authentication_info)) {
    /* Collect nextnonce */
    if (sip->sip_authentication_info)
      auc_info(&nh->nh_auth,
	       sip->sip_authentication_info,
	       sip_authorization_class);
    if (sip->sip_proxy_authentication_info)
      auc_info(&nh->nh_auth,
	       sip->sip_proxy_authentication_info,
	       sip_proxy_authorization_class);
  }

  if ((method != sip_method_invite && status >= 200) || status >= 300)
    nua_client_request_remove(cr);

  nua_client_report(cr, status, phrase, sip, cr->cr_orq, tags);

  if (status < 200 ||
      /* Un-ACKed 2XX response to INVITE */
      (method == sip_method_invite && status < 300 && !cr->cr_acked)) {
    cr->cr_reporting = 0, nh->nh_ds->ds_reporting = 0;
    return 1;
  }

  nua_client_request_clean(cr);

  du = cr->cr_usage;

  if (cr->cr_terminated < 0) {
    /* XXX - dialog has been terminated */;
    nua_dialog_deinit(nh, nh->nh_ds), cr->cr_usage = NULL;
  }
  else if (du) {
    if (cr->cr_terminated ||
	(!du->du_ready && status >= 300 && nua_client_is_bound(cr))) {
      /* Usage has been destroyed */
      nua_dialog_usage_remove(nh, nh->nh_ds, du, cr, NULL), cr->cr_usage = NULL;
    }
    else if (cr->cr_graceful) {
      /* Terminate usage gracefully */
      if (nua_dialog_usage_shutdown(nh, nh->nh_ds, du) > 0)
	cr->cr_usage = NULL;
    }
  }
  else if (cr->cr_terminated) {
    if (nh->nh_ds->ds_usage == NULL)
      nua_dialog_remove(nh, nh->nh_ds, NULL), cr->cr_usage = NULL;
  }

  cr->cr_phrase = NULL;
  cr->cr_reporting = 0, nh->nh_ds->ds_reporting = 0;

  if (method == sip_method_cancel)
    return 1;

  return nua_client_next_request(nh->nh_ds->ds_cr, method == sip_method_invite);
}

/** Send event, zap transaction but leave cr in list */
int nua_client_report(nua_client_request_t *cr,
		      int status, char const *phrase,
		      sip_t const *sip,
		      nta_outgoing_t *orq,
		      tagi_t const *tags)
{
  nua_handle_t *nh;

  if (cr->cr_event == nua_r_destroy)
    return 1;

  if (cr->cr_methods->crm_report)
    return cr->cr_methods->crm_report(cr, status, phrase, sip, orq, tags);

  nh = cr->cr_owner;

  nua_stack_event(nh->nh_nua, nh,
		  nta_outgoing_getresponse(orq),
		  cr->cr_event,
		  status, phrase,
		  tags);
  return 1;
}

int nua_client_treport(nua_client_request_t *cr,
		       int status, char const *phrase,
		       sip_t const *sip,
		       nta_outgoing_t *orq,
		       tag_type_t tag, tag_value_t value, ...)
{
  int retval;
  ta_list ta;
  ta_start(ta, tag, value);
  retval = nua_client_report(cr, status, phrase, sip, orq, ta_args(ta));
  ta_end(ta);
  return retval;
}

int nua_client_next_request(nua_client_request_t *cr, int invite)
{
  for (; cr; cr = cr->cr_next) {
    if (cr->cr_method == sip_method_cancel)
      continue;

    if (invite
	? cr->cr_method == sip_method_invite
	: cr->cr_method != sip_method_invite)
      break;
  }

  if (cr && cr->cr_orq == NULL) {
    nua_client_init_request(cr);
  }

  return 1;
}

nua_client_request_t *
nua_client_request_pending(nua_client_request_t const *cr)
{
  for (;cr;cr = cr->cr_next)
    if (cr->cr_orq)
      return (nua_client_request_t *)cr;

  return NULL;
}
