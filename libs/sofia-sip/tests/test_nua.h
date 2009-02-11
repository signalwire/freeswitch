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

/**@@internal
 * @file test_nua.h
 * @brief High-level tester framework for Sofia SIP User Agent Engine
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti Mela@nokia.com>
 *
 * @date Created: Wed Aug 17 12:12:12 EEST 2005 ppessi
 */

#ifndef TEST_NUA_H
#define TEST_NUA_H

struct context;
#define NUA_MAGIC_T struct context

struct call;
#define NUA_HMAGIC_T struct call

#include "sofia-sip/nua.h"
#include "sofia-sip/sip_status.h"

#include <sofia-sip/sdp.h>
#include <sofia-sip/sip_header.h>

#include <sofia-sip/su_log.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_tag_io.h>
#include <sofia-sip/su_string.h>
#include <sofia-sip/nua_tag.h>

#if __APPLE_CC__
#include <sofia-sip/su_osx_runloop.h>
#endif

#include "test_proxy.h"
#include "test_nat.h"
#include <sofia-sip/auth_module.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>
#include <unistd.h>

extern char const name[];

extern int print_headings;
extern int tstflags;
#define TSTFLAGS tstflags

#include <sofia-sip/tstdef.h>

#define TEST_E(a, b) TEST_S(nua_event_name(a), nua_event_name(b))

#define NONE ((void*)-1)

struct endpoint;

typedef
int condition_function(nua_event_t event,
		       int status, char const *phrase,
		       nua_t *nua, struct context *ctx,
		       struct endpoint *ep,
		       nua_handle_t *nh, struct call *call,
		       sip_t const *sip,
		       tagi_t tags[]);

typedef
void printer_function(nua_event_t event,
		      char const *operation,
		      int status, char const *phrase,
		      nua_t *nua, struct context *ctx,
		      struct endpoint *ep,
		      nua_handle_t *nh, struct call *call,
		      sip_t const *sip,
		      tagi_t tags[]);

struct proxy_transaction;
struct registration_entry;

enum { event_is_extra, event_is_normal, event_is_special };

struct eventlist
{
  nua_event_t kind;
  struct event *head, **tail;
};

struct event
{
  struct event *next, **prev;
  struct call *call;
  nua_saved_event_t saved_event[1];
  nua_event_data_t const *data;
};


struct context
{
  su_home_t home[1];
  su_root_t *root;

  int threading, proxy_tests, expensive, quit_on_single_failure, osx_runloop;
  int print_tags;

  url_t *external_proxy;

  int proxy_logging;

  struct endpoint {
    char name[4];
    struct context *ctx;	/* Backpointer */

    int logging;
    int print_tags;

    int running;

    struct domain *domain;
    condition_function *next_condition;
    nua_event_t next_event, last_event;
    nua_t *nua;
    sip_contact_t *contact;
    sip_from_t *to;

    sip_allow_t *allow;
    char const *appl_method;
    sip_supported_t *supported;

    printer_function *printer;

    char const *instance;

    /* Per-call stuff */
    struct call {
      struct call *next;
      nua_handle_t *nh;
      char const *sdp;
      struct eventlist *events;
    } call[1], reg[1];

    int (*is_special)(nua_event_t e);

    /* Normal events are saved here */
    struct eventlist events[1];
    /* Special events are saved here */
    struct eventlist specials[1];

    /* State flags for complex scenarios */
    struct {
      unsigned n;
      unsigned bit0:1, bit1:1, bit2:1, bit3:1;
      unsigned bit4:1, bit5:1, bit6:1, bit7:1;
      unsigned :0;
    } flags;
    /* Accross-run state information */
    struct {
      unsigned n;
    } state;
  } a, b, c;

  struct proxy *p;
  sip_route_t const *lr;
  struct nat *nat;
};

#define RETURN_ON_SINGLE_FAILURE(retval)			  \
  do {								  \
    fflush(stdout);						  \
    if (retval && ctx->quit_on_single_failure) { return retval; } \
  } while(0)


int save_event_in_list(struct context *,
		       nua_event_t nevent,
		       struct endpoint *,
		       struct call *);
void free_events_in_list(struct context *,
			 struct eventlist *);
void free_event_in_list(struct context *ctx,
			struct eventlist *list,
			struct event *e);

struct event *event_by_type(struct event *e, nua_event_t);
size_t count_events(struct event const *e);

#define CONDITION_PARAMS			\
  nua_event_t event,				\
  int status, char const *phrase,		\
  nua_t *nua, struct context *ctx,		\
  struct endpoint *ep,				\
  nua_handle_t *nh, struct call *call,		\
  sip_t const *sip,				\
  tagi_t tags[]

int save_events(CONDITION_PARAMS);
int until_final_response(CONDITION_PARAMS);
int save_until_final_response(CONDITION_PARAMS);
int save_until_received(CONDITION_PARAMS);
int save_until_special(CONDITION_PARAMS);

int until_terminated(CONDITION_PARAMS);
int until_ready(CONDITION_PARAMS);
int accept_call(CONDITION_PARAMS);
int cancel_when_ringing(CONDITION_PARAMS);

int accept_notify(CONDITION_PARAMS);

void a_callback(nua_event_t event,
		int status, char const *phrase,
		nua_t *nua, struct context *ctx,
		nua_handle_t *nh, struct call *call,
		sip_t const *sip,
		tagi_t tags[]);
void b_callback(nua_event_t event,
		int status, char const *phrase,
		nua_t *nua, struct context *ctx,
		nua_handle_t *nh, struct call *call,
		sip_t const *sip,
		tagi_t tags[]);
void c_callback(nua_event_t event,
		int status, char const *phrase,
		nua_t *nua, struct context *ctx,
		nua_handle_t *nh, struct call *call,
		sip_t const *sip,
		tagi_t tags[]);

void run_abc_until(struct context *ctx,
		   nua_event_t a_event, condition_function *a_condition,
		   nua_event_t b_event, condition_function *b_condition,
		   nua_event_t c_event, condition_function *c_condition);

void run_ab_until(struct context *ctx,
		  nua_event_t a_event, condition_function *a_condition,
		  nua_event_t b_event, condition_function *b_condition);

void run_bc_until(struct context *ctx,
		  nua_event_t b_event, condition_function *b_condition,
		  nua_event_t c_event, condition_function *c_condition);

int run_a_until(struct context *, nua_event_t, condition_function *);
int run_b_until(struct context *, nua_event_t, condition_function *);
int run_c_until(struct context *, nua_event_t, condition_function *);

typedef int operation_f(struct endpoint *ep, struct call *call,
			nua_handle_t *nh, tag_type_t tag, tag_value_t value,
			...);

operation_f INVITE, ACK, BYE, CANCEL, AUTHENTICATE, UPDATE, INFO, PRACK,
  REFER, MESSAGE, METHOD, OPTIONS, PUBLISH, UNPUBLISH, REGISTER, UNREGISTER,
  SUBSCRIBE, UNSUBSCRIBE, NOTIFY, NOTIFIER, TERMINATE, AUTHORIZE;

int RESPOND(struct endpoint *ep,
	    struct call *call,
	    nua_handle_t *nh,
	    int status, char const *phrase,
	    tag_type_t tag, tag_value_t value,
	    ...);

int DESTROY(struct endpoint *ep,
	    struct call *call,
	    nua_handle_t *nh);

struct call *check_handle(struct endpoint *ep,
			  struct call *call,
			  nua_handle_t *nh,
			  int status, char const *phrase);

int is_special(nua_event_t e);
int callstate(tagi_t const *tags);
int is_offer_sent(tagi_t const *tags);
int is_answer_sent(tagi_t const *tags);
int is_offer_recv(tagi_t const *tags);
int is_answer_recv(tagi_t const *tags);
int is_offer_answer_done(tagi_t const *tags);
int audio_activity(tagi_t const *tags);
int video_activity(tagi_t const *tags);

void print_event(nua_event_t event,
		 char const *operation,
		 int status, char const *phrase,
		 nua_t *nua, struct context *ctx,
		 struct endpoint *ep,
		 nua_handle_t *nh, struct call *call,
		 sip_t const *sip,
		 tagi_t tags[]);

su_inline
void eventlist_init(struct eventlist *list)
{
  list->tail = &list->head;
}

su_inline
void call_init(struct call *call)
{
}

void endpoint_init(struct context *ctx, struct endpoint *e, char id);

int test_nua_init(struct context *ctx,
		  int start_proxy,
		  url_t const *o_proxy,
		  int start_nat,
		  tag_type_t tag, tag_value_t value, ...);

int test_deinit(struct context *ctx);

int test_nua_api_errors(struct context *ctx);
int test_nua_destroy(struct context *ctx);
int test_stack_errors(struct context *ctx);
int test_tag_filter(void);
int test_nua_params(struct context *ctx);

int test_register(struct context *ctx);
int test_connectivity(struct context *ctx);
int test_nat_timeout(struct context *ctx);
int test_unregister(struct context *ctx);

int test_basic_call(struct context *ctx);
int test_offer_answer(struct context *ctx);
int test_rejects(struct context *ctx);
int test_mime_negotiation(struct context *ctx);
int test_call_timeouts(struct context *ctx);
int test_reject_401_aka(struct context *ctx);
int test_call_cancel(struct context *ctx);
int test_call_destroy(struct context *ctx);
int test_early_bye(struct context *ctx);
int test_call_hold(struct context *ctx);
int test_reinvites(struct context *ctx);
int test_session_timer(struct context *ctx);
int test_refer(struct context *ctx);
int test_100rel(struct context *ctx);
int test_simple(struct context *ctx);
int test_events(struct context *ctx);

int test_extension(struct context *ctx);

#endif
