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

/**@CFILE test_ops.c
 * @brief High-level test framework for Sofia SIP User Agent Engine
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti Mela@nokia.com>
 *
 * @date Created: Wed Aug 17 12:12:12 EEST 2005 ppessi
 */

#include "config.h"

#include "test_nua.h"

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
#define __func__ name
#endif

int save_events(CONDITION_PARAMS)
{
  return save_event_in_list(ctx, event, ep, call) == event_is_normal;
}

int until_final_response(CONDITION_PARAMS)
{
  return status >= 200;
}

int save_until_final_response(CONDITION_PARAMS)
{
  save_event_in_list(ctx, event, ep, call);
  return
    nua_r_set_params <= event && event < nua_i_network_changed
    && status >= 200;
}

/** Save events.
 *
 * Terminate when a event is saved.
 */
int save_until_received(CONDITION_PARAMS)
{
  return save_event_in_list(ctx, event, ep, call) == event_is_normal;
}

/** Save events until nua_i_outbound is received.  */
int save_until_special(CONDITION_PARAMS)
{
  return save_event_in_list(ctx, event, ep, call) == event_is_special;
}

/* Return call state from event tag list */
int callstate(tagi_t const *tags)
{
  tagi_t const *ti = tl_find(tags, nutag_callstate);
  return ti ? ti->t_value : -1;
}

/* Return true if offer is sent */
int is_offer_sent(tagi_t const *tags)
{
  tagi_t const *ti = tl_find(tags, nutag_offer_sent);
  return ti ? ti->t_value : 0;
}

/* Return true if answer is sent */
int is_answer_sent(tagi_t const *tags)
{
  tagi_t const *ti = tl_find(tags, nutag_answer_sent);
  return ti ? ti->t_value : 0;
}

/* Return true if offer is recv */
int is_offer_recv(tagi_t const *tags)
{
  tagi_t const *ti = tl_find(tags, nutag_offer_recv);
  return ti ? ti->t_value : 0;
}

/* Return true if answer is recv */
int is_answer_recv(tagi_t const *tags)
{
  tagi_t const *ti = tl_find(tags, nutag_answer_recv);
  return ti ? ti->t_value : 0;
}

/* Return true if offer/answer is sent/recv */
int is_offer_answer_done(tagi_t const *tags)
{
  tagi_t const *ti;

  return
    ((ti = tl_find(tags, nutag_answer_recv)) && ti->t_value) ||
    ((ti = tl_find(tags, nutag_offer_sent)) && ti->t_value) ||
    ((ti = tl_find(tags, nutag_offer_recv)) && ti->t_value) ||
    ((ti = tl_find(tags, nutag_answer_sent)) && ti->t_value);
}

/* Return audio state from event tag list */
int audio_activity(tagi_t const *tags)
{
  tagi_t const *ti = tl_find(tags, soatag_active_audio);
  return ti ? ti->t_value : -1;
}

/* Return video state from event tag list */
int video_activity(tagi_t const *tags)
{
  tagi_t const *ti = tl_find(tags, soatag_active_video);
  return ti ? ti->t_value : -1;
}

void print_event(nua_event_t event,
		 char const *operation,
		 int status, char const *phrase,
		 nua_t *nua, struct context *ctx,
		 struct endpoint *ep,
		 nua_handle_t *nh, struct call *call,
		 sip_t const *sip,
		 tagi_t tags[])
{
  tagi_t const *t;
  static su_nanotime_t started = 0;
  su_nanotime_t now;
  char timestamp[32];

  su_nanotime(&now);

  if (started == 0) started = now;

  now -= started; now /= 1000000;

  snprintf(timestamp, sizeof timestamp, "%03u.%03u",
	   (unsigned)(now / 1000), (unsigned)(now % 1000));

  if (event == nua_i_state) {
    fprintf(stderr, "%s %s.nua(%p): event %s %s\n", timestamp,
	    ep->name, (void *)nh, nua_event_name(event),
	    nua_callstate_name(callstate(tags)));
  }
  else if ((int)event >= nua_r_set_params) {
    t = tl_find(tags, nutag_substate);
    if (t) {
      fprintf(stderr, "%s %s.nua(%p): event %s status %u %s (%s)\n", timestamp,
	      ep->name, (void*)nh, nua_event_name(event), status, phrase,
	      nua_substate_name(t->t_value));
    }
    else {
      fprintf(stderr, "%s %s.nua(%p): event %s status %u %s\n", timestamp,
	      ep->name, (void *)nh, nua_event_name(event), status, phrase);
    }
  }
  else if (event == nua_i_notify) {
    t = tl_find(tags, nutag_substate);
    fprintf(stderr, "%s %s.nua(%p): event %s %s (%s)\n", timestamp,
	    ep->name, (void *)nh, nua_event_name(event), phrase,
	    nua_substate_name(t ? t->t_value : 0));
  }
  else if ((int)event >= nua_i_bye ||
	   event == nua_i_invite || event == nua_i_cancel ||
	   event == nua_i_ack) {
    fprintf(stderr, "%s %s.nua(%p): event %s %03d %s\n", timestamp,
	    ep->name, (void *)nh, nua_event_name(event), status, phrase);
  }
  else if ((int)event >= 0) {
    fprintf(stderr, "%s %s.nua(%p): event %s %s\n", timestamp,
	    ep->name, (void *)nh, nua_event_name(event), phrase);
  }
  else if (status > 0) {
    fprintf(stderr, "%s %s.nua(%p): call %s() with status %u %s\n", timestamp,
	    ep->name, (void *)nh, operation, status, phrase);
  }
  else {
    t = tl_find(tags, siptag_subject_str);
    if (t && t->t_value) {
      char const *subject = (char const *)t->t_value;
      fprintf(stderr, "%s %s.nua(%p): call %s() \"%s\"\n", timestamp,
	      ep->name, (void *)nh, operation, subject);
    }
    else
      fprintf(stderr, "%s %s.nua(%p): call %s()\n", timestamp,
	      ep->name, (void *)nh, operation);
  }

  if (tags &&
      ((tstflags & tst_verbatim) || ctx->print_tags || ep->print_tags))
    tl_print(stderr, "", tags);
}

void ep_callback(nua_event_t event,
		 int status, char const *phrase,
		 nua_t *nua, struct context *ctx,
		 struct endpoint *ep,
		 nua_handle_t *nh, struct call *call,
		 sip_t const *sip,
		 tagi_t tags[])
{
  if (ep->printer)
    ep->printer(event, "", status, phrase, nua, ctx, ep, nh, call, sip, tags);

  if (call == NULL && nh) {
    for (call = ep->call; call; call = call->next) {
      if (!call->nh)
	break;
      if (nh == call->nh)
	break;
    }

    if (call && call->nh == NULL) {
      call->nh = nh;
      nua_handle_bind(nh, call);
    }
  }

  if ((ep->next_condition == NULL ||
       ep->next_condition(event, status, phrase,
			  nua, ctx, ep, nh, call, sip, tags))
      &&
      (ep->next_event == -1 || ep->next_event == event))
    ep->running = 0;

  ep->last_event = event;

  if (call == NULL && nh)
    nua_handle_destroy(nh);
}

void a_callback(nua_event_t event,
		int status, char const *phrase,
		nua_t *nua, struct context *ctx,
		nua_handle_t *nh, struct call *call,
		sip_t const *sip,
		tagi_t tags[])
{
  ep_callback(event, status, phrase, nua, ctx, &ctx->a, nh, call, sip, tags);
}

void b_callback(nua_event_t event,
		int status, char const *phrase,
		nua_t *nua, struct context *ctx,
		nua_handle_t *nh, struct call *call,
		sip_t const *sip,
		tagi_t tags[])
{
  ep_callback(event, status, phrase, nua, ctx, &ctx->b, nh, call, sip, tags);
}

void c_callback(nua_event_t event,
		int status, char const *phrase,
		nua_t *nua, struct context *ctx,
		nua_handle_t *nh, struct call *call,
		sip_t const *sip,
		tagi_t tags[])
{
  ep_callback(event, status, phrase, nua, ctx, &ctx->c, nh, call, sip, tags);
}

void run_abc_until(struct context *ctx,
		   nua_event_t a_event, condition_function *a_condition,
		   nua_event_t b_event, condition_function *b_condition,
		   nua_event_t c_event, condition_function *c_condition)
{
  struct endpoint *a = &ctx->a, *b = &ctx->b, *c = &ctx->c;

  a->next_event = a_event;
  a->next_condition = a_condition;
  a->last_event = -1;
  a->running = a_condition != NULL && a_condition != save_events;
  a->running |= a_event != -1;
  memset(&a->flags, 0, sizeof a->flags);

  b->next_event = b_event;
  b->next_condition = b_condition;
  b->last_event = -1;
  b->running = b_condition != NULL && b_condition != save_events;
  b->running |= b_event != -1;
  memset(&b->flags, 0, sizeof b->flags);

  c->next_event = c_event;
  c->next_condition = c_condition;
  c->last_event = -1;
  c->running = c_condition != NULL && c_condition != save_events;
  c->running |= c_event != -1;
  memset(&c->flags, 0, sizeof c->flags);

  for (; a->running || b->running || c->running;) {
    su_root_step(ctx->root, 100);
  }
}

void run_ab_until(struct context *ctx,
		  nua_event_t a_event, condition_function *a_condition,
		  nua_event_t b_event, condition_function *b_condition)
{
  run_abc_until(ctx, a_event, a_condition, b_event, b_condition, -1, NULL);
}

void run_bc_until(struct context *ctx,
		  nua_event_t b_event, condition_function *b_condition,
		  nua_event_t c_event, condition_function *c_condition)
{
  run_abc_until(ctx, -1, NULL, b_event, b_condition, c_event, c_condition);
}

int run_a_until(struct context *ctx,
		nua_event_t a_event,
		condition_function *a_condition)
{
  run_abc_until(ctx, a_event, a_condition, -1, NULL, -1, NULL);
  return ctx->a.last_event;
}

int run_b_until(struct context *ctx,
		nua_event_t b_event,
		condition_function *b_condition)
{
  run_abc_until(ctx, -1, NULL, b_event, b_condition, -1, NULL);
  return ctx->b.last_event;
}

int run_c_until(struct context *ctx,
		nua_event_t event,
		condition_function *condition)
{
  run_abc_until(ctx, -1, NULL, -1, NULL, event, condition);
  return ctx->c.last_event;
}

#define OPERATION(X, x)	   \
int X(struct endpoint *ep, \
      struct call *call, nua_handle_t *nh, \
      tag_type_t tag, tag_value_t value, \
      ...) \
{ \
  ta_list ta; \
  ta_start(ta, tag, value); \
\
  if (ep->printer) \
    ep->printer(-1, "nua_" #x, 0, "", ep->nua, ep->ctx, ep, \
		nh, call, NULL, ta_args(ta)); \
\
  nua_##x(nh, ta_tags(ta)); \
\
  ta_end(ta); \
  return 0; \
} extern int dummy

OPERATION(INVITE, invite);
OPERATION(ACK, ack);
OPERATION(BYE, bye);
OPERATION(CANCEL, cancel);
OPERATION(AUTHENTICATE, authenticate);
OPERATION(UPDATE, update);
OPERATION(INFO, info);
OPERATION(PRACK, prack);
OPERATION(REFER, refer);
OPERATION(MESSAGE, message);
OPERATION(METHOD, method);
OPERATION(OPTIONS, options);
OPERATION(PUBLISH, publish);
OPERATION(UNPUBLISH, unpublish);
OPERATION(REGISTER, register);
OPERATION(UNREGISTER, unregister);
OPERATION(SUBSCRIBE, subscribe);
OPERATION(UNSUBSCRIBE, unsubscribe);
OPERATION(NOTIFY, notify);
OPERATION(NOTIFIER, notifier);
OPERATION(TERMINATE, terminate);
OPERATION(AUTHORIZE, authorize);

/* Respond via endpoint and handle */
int RESPOND(struct endpoint *ep,
	    struct call *call,
	    nua_handle_t *nh,
	    int status, char const *phrase,
	    tag_type_t tag, tag_value_t value,
	    ...)
{
  ta_list ta;

  ta_start(ta, tag, value);

  if (ep->printer)
    ep->printer(-1, "nua_respond", status, phrase, ep->nua, ep->ctx, ep,
		nh, call, NULL, ta_args(ta));

  nua_respond(nh, status, phrase, ta_tags(ta));
  ta_end(ta);

  return 0;
}

/* Destroy a handle */
int DESTROY(struct endpoint *ep,
	    struct call *call,
	    nua_handle_t *nh)
{
  if (ep->printer)
    ep->printer(-1, "nua_handle_destroy", 0, "", ep->nua, ep->ctx, ep,
		nh, call, NULL, NULL);

  nua_handle_destroy(nh);

  if (call->nh == nh)
    call->nh = NULL;

  return 0;
}


/* Reject all but currently used handles */
struct call *check_handle(struct endpoint *ep,
			  struct call *call,
			  nua_handle_t *nh,
			  int status, char const *phrase)
{
  if (call)
    return call;

  if (status)
    RESPOND(ep, call, nh, status, phrase, TAG_END());

  nua_handle_destroy(nh);
  return NULL;
}

/* Save nua event in call-specific list */
int save_event_in_list(struct context *ctx,
		       nua_event_t nevent,
		       struct endpoint *ep,
		       struct call *call)

{
  struct eventlist *list;
  struct event *e;
  int action = ep->is_special(nevent);

  if (action == event_is_extra)
    return 0;
  else if (action == event_is_special || call == NULL)
    list = ep->specials;
  else if (call->events)
    list = call->events;
  else
    list = ep->events;

  e = su_zalloc(ctx->home, sizeof *e);

  if (!e) { perror("su_zalloc"), abort(); }

  if (!nua_save_event(ep->nua, e->saved_event)) {
    su_free(ctx->home, e);
    return -1;
  }

  *(e->prev = list->tail) = e; list->tail = &e->next;

  e->call = call;
  e->data = nua_event_data(e->saved_event);

  return action;
}

/* Free nua events from endpoint list */
void free_events_in_list(struct context *ctx,
			 struct eventlist *list)
{
  struct event *e;

  while ((e = list->head)) {
    if ((*e->prev = e->next))
      e->next->prev = e->prev;
    nua_destroy_event(e->saved_event);
    su_free(ctx->home, e);
  }

  list->tail = &list->head;
}

void free_event_in_list(struct context *ctx,
			struct eventlist *list,
			struct event *e)
{
  if (e) {
    if ((*e->prev = e->next))
      e->next->prev = e->prev;
    nua_destroy_event(e->saved_event);
    su_free(ctx->home, e);

    if (list->head == NULL)
      list->tail = &list->head;
  }
}

struct event *event_by_type(struct event *e, nua_event_t etype)
{
  for (; e; e = e->next) {
    if (e->data->e_event == etype)
      break;
  }

  return e;
}

size_t count_events(struct event const *e)
{
  size_t n;

  for (n = 0; e; e = e->next)
    n++;

  return n;
}


int is_special(nua_event_t e)
{
  if (e == nua_i_active || e == nua_i_terminated)
    return event_is_extra;
  if (e == nua_i_outbound)
    return event_is_special;

  return event_is_normal;
}

void
endpoint_init(struct context *ctx, struct endpoint *e, char id)
{
  e->name[0] = id;
  e->ctx = ctx;

  e->is_special = is_special;

  call_init(e->call);
  call_init(e->reg);
  eventlist_init(e->events);
  eventlist_init(e->specials);
}

void nolog(void *stream, char const *fmt, va_list ap) {}
