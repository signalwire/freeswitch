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
    SU_DEBUG_1(("nua: initializing SIP stack failed\n" VA_NONE));
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
      char const *name = nua_event_name((enum nua_event_e)e->e_event) + 4;
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

    nua->nua_callback((enum nua_event_e)e->e_event, e->e_status, e->e_phrase,
		      nua, nua->nua_magic,
		      nh, nh ? nh->nh_magic : NULL,
		      e->e_msg ? sip_object(e->e_msg) : NULL,
		      e->e_tags);

	if (su_msg_is_non_null(frame->nf_saved)) {
		su_msg_destroy(frame->nf_saved);
	}
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


su_msg_t *nua_current_msg(nua_t const *nua, int clear)
{
	if (nua && nua->nua_current && su_msg_is_non_null(nua->nua_current->nf_saved)) {
		su_msg_t *r = nua->nua_current->nf_saved[0];
		if (clear) {
			nua->nua_current->nf_saved[0] = NULL;
		}
		return r;
		//return su_msg_data(nua->nua_current->nf_saved)->ee_data->e_msg;
		
	}

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
    char const *name = nua_event_name((enum nua_event_e)e->e_event);

    if (e->e_status == 0)
      SU_DEBUG_5(("nua(%p): %s signal %s\n", (void *)nh, "recv", name + 4));
    else
      SU_DEBUG_5(("nua(%p): recv signal %s %u %s\n",
		  (void *)nh, name + 4,
		  e->e_status, e->e_phrase ? e->e_phrase : ""));
  }

  su_msg_save(nua->nua_signal, msg);

  event = (enum nua_event_e)e->e_event;

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
	  if (!nh->nh_destroyed) {
		  nua_stack_destroy_handle(nua, nh, tags);
		  su_msg_destroy(nua->nua_signal);
	  }
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
  if (saved && saved[0]) su_msg_destroy(saved);
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
      while (nh->nh_ds->ds_usage) {
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
  if (nh->nh_destroyed) {
	  return;
  }

  if (nh->nh_notifier)
    nua_stack_terminate(nua, nh, (enum nua_event_e)0, NULL);

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

  if (nh->nh_destroyed) {
	  return;
  }

  nh->nh_destroyed = 1;

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
  if (nh && nh->nh_ds->ds_leg)
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
