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

/**@CFILE nua_client.c
 * @brief Client transaction handling
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
#include <sofia-sip/su_tag_inline.h>

#include <sofia-sip/sip_util.h>
#include <sofia-sip/sip_protos.h>
#include <sofia-sip/sip_status.h>

#define SU_MSG_ARG_T   struct nua_ee_data
#define SU_TIMER_ARG_T struct nua_client_request

#define NUA_SAVED_EVENT_T su_msg_t *
#define NUA_SAVED_SIGNAL_T su_msg_t *

#define NTA_AGENT_MAGIC_T    struct nua_s
#define NTA_LEG_MAGIC_T      struct nua_handle_s
#define NTA_OUTGOING_MAGIC_T struct nua_client_request

#include "nua_stack.h"
#include "nua_dialog.h"
#include "nua_client.h"

#include <sofia-sip/su_wait.h>

#if 0
su_inline int can_redirect(sip_contact_t const *m, sip_method_t method);
#endif

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
static int nua_client_request_sendmsg(nua_client_request_t *cr);
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
			   (enum nua_event_e)event,
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
	if (cr->cr_signal[0]) {
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

int
nua_client_request_complete(nua_client_request_t *cr)
{
  if (cr->cr_orq) {
    nua_client_request_ref(cr);
    if (cr->cr_methods->crm_complete)
      /* Calls nua_invite_client_complete() */
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

/** Check if client request is in progress.
 *
 * A client request is in progress, if
 * 1) it has actual transaction going on
 * 2) it is waiting credentials from application
 * 3) it is waiting for Retry-After timer
 */
int
nua_client_request_in_progress(nua_client_request_t const *cr)
{
  return cr &&
    (cr->cr_orq || cr->cr_wait_for_cred || cr->cr_timer);
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


/** Send a request message.
 *
 * If an error occurs, send error event to the application.
 *
 * @retval 0 if request is pending
 * @retval >=1 if error event has been sent
 */
static
int nua_client_request_try(nua_client_request_t *cr)
{
  int error = nua_client_request_sendmsg(cr);

  if (error < 0)
    error = nua_client_response(cr, NUA_ERROR_AT(__FILE__, __LINE__), NULL);

  return error;
}

/**Send a request message.
 *
 * @retval 0 if request is pending
 * @retval >=1 if error event has been sent
 * @retval < 0 if no error event has been sent
 */
static
int nua_client_request_sendmsg(nua_client_request_t *cr)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_state_t *ds = nh->nh_ds;
  sip_method_t method = cr->cr_method;
  char const *name = cr->cr_method_name;
  url_string_t const *url = (url_string_t *)cr->cr_target;
  nta_leg_t *leg;
  msg_t *msg;
  sip_t *sip;
  int error;

  assert(cr->cr_orq == NULL);

  cr->cr_offer_sent = cr->cr_answer_recv = 0;
  cr->cr_offer_recv = cr->cr_answer_sent = 0;

  if (!ds->ds_leg && cr->cr_dialog) {
    ds->ds_leg = nta_leg_tcreate(nh->nh_nua->nua_nta,
				 nua_stack_process_request, nh,
				 SIPTAG_CALL_ID(cr->cr_sip->sip_call_id),
				 SIPTAG_FROM(cr->cr_sip->sip_from),
				 SIPTAG_TO(cr->cr_sip->sip_to),
				 SIPTAG_CSEQ(cr->cr_sip->sip_cseq),
				 TAG_END());
    if (!ds->ds_leg)
      return -1;
  }

  if (cr->cr_sip->sip_from && ds->ds_leg) {
    if (cr->cr_sip->sip_from->a_tag == NULL) {
      if (sip_from_tag(msg_home(cr->cr_msg), cr->cr_sip->sip_from,
		       nta_leg_tag(ds->ds_leg, NULL)) < 0) {
	return -1;
      }
    }
  }

  cr->cr_retry_count++;

  if (ds->ds_leg)
    leg = ds->ds_leg;
  else
    leg = nh->nh_nua->nua_dhandle->nh_ds->ds_leg; /* Default leg */

  msg = msg_copy(cr->cr_msg), sip = sip_object(msg);

  if (msg == NULL)
    return -1;

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
  if (nta_msg_request_complete(msg, leg, method, name, url) < 0) {
    msg_destroy(msg);
    return -1;
  }

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

  if (!sip->sip_via && NH_PGET(nh, via))
    sip_add_make(msg, sip, sip_via_class, NH_PGET(nh, via));

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
						!ds->ds_route) < 0) {
      msg_destroy(msg);
      return -1;
    }
  }

  cr->cr_wait_for_cred = 0;

  if (cr->cr_methods->crm_send)
    error = cr->cr_methods->crm_send(cr, msg, sip, NULL);
  else
    error = nua_base_client_request(cr, msg, sip, NULL);

  if (error == -1)
    msg_destroy(msg);

  return error;
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
 * @retval -1 if error occurred, but event has not been sent,
 *            and caller has to destroy request message @ msg
 * @retval -2 if error occurred, event has not been sent
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
int
nua_client_orq_response(nua_client_request_t *cr,
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
	  nua_dialog_uac_route(nh, nh->nh_ds, sip, 1, cr->cr_initial);
	nua_dialog_store_peer_info(nh, nh->nh_ds, sip);
      }

      if (du && du->du_cr == cr)
	du->du_ready = 1;
    }
  }
  else {
    sip_method_t method = cr->cr_method;
    int terminated, graceful = 1;

    if (status < 700) {
		terminated = sip_response_terminates_dialog(status, method, &graceful);
		if (terminated && !cr->cr_initial) {
			terminated = 0, graceful = 1;
		}
	} else {
		/* XXX - terminate usage by all internal error responses */
		terminated = 0, graceful = 1;
	}

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
#if 0
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
#endif

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

  if (status == 403) {
	  if (nh->nh_auth) {
		  /* Bad username/password */
		  SU_DEBUG_7(("nua(%p): bad credentials, clearing them\n", (void *)nh));
		  auc_clear_credentials(&nh->nh_auth, NULL, NULL);
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
      } else if (auc_has_authorization(&nh->nh_auth)) {
		  return nua_client_restart(cr, 100, "Request Authorized by Cache");
	  }

      orq = cr->cr_orq, cr->cr_orq = NULL;

      cr->cr_waiting = cr->cr_wait_for_cred = 1;
      nua_client_report(cr, status, phrase, NULL, orq, NULL);
      nta_outgoing_destroy(orq);
      cr->cr_status = 0, cr->cr_phrase = NULL;
      nua_client_request_unref(cr);

      return 1;
    }
  }
  /* GriGiu : RFC-3261 status supported Retry-After */
  if ( (status == 404 || status == 413 || status == 480 || status == 486 ||
	   status == 500 || status == 503 ||
	   status == 600 || status == 603) &&
      sip->sip_retry_after &&
	  NH_PGET(nh, retry_after_enable) &&
      sip->sip_retry_after->af_delta < 3200) {
    su_timer_t *timer;
    char phrase[18];		/* Retry After XXXX\0 */

    timer = su_timer_create(su_root_task(nh->nh_nua->nua_root), 0);

    if (su_timer_set_interval(timer, nua_client_restart_after, cr,
			      sip->sip_retry_after->af_delta * 1000) < 0) {
      su_timer_destroy(timer);
      return 0; /* Too bad */
    }

    cr->cr_timer = timer;	/* This takes over cr reference from orq */

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

#if 0
su_inline
int can_redirect(sip_contact_t const *m, sip_method_t method)
{
  if (m && m->m_url->url_host) {
    enum url_type_e type = (enum url_type_e)m->m_url->url_type;
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
#endif

/** @internal Add authorization data */
static int nh_authorize(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
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

/** Request restarted by timer */
static void
nua_client_restart_after(su_root_magic_t *magic,
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
  nta_outgoing_t *orq;
  int error = -1, terminated, graceful;

  if (cr->cr_retry_count > NH_PGET(nh, retry_count))
    return 0;

  orq = cr->cr_orq, cr->cr_orq = NULL;  assert(orq);
  terminated = cr->cr_terminated, cr->cr_terminated = 0;
  graceful = cr->cr_graceful, cr->cr_graceful = 0;

  cr->cr_restarting = 1;
  error = nua_client_request_sendmsg(cr);
  cr->cr_restarting = 0;

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
		  (enum nua_event_e)cr->cr_event,
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

  if (cr && !nua_client_request_in_progress(cr)) {
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
