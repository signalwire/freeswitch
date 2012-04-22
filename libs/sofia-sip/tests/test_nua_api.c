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

/**@CFILE test_nua_api.c
 * @brief NUA API tester.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti Mela@nokia.com>
 *
 * @date Created: Wed Aug 17 12:12:12 EEST 2005 ppessi
 */

#include "config.h"

#include "test_nua.h"
#include "sofia-sip/tport_tag.h"

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
#define __func__ "test_nua_api_errors"
#endif

/* ------------------------------------------------------------------------ */
/* API tests */

SOFIAPUBVAR su_log_t nua_log[];

int check_set_status(int status, char const *phrase)
{
  return status == 200 && strcmp(phrase, sip_200_OK) == 0;
}

int test_nua_api_errors(struct context *ctx)
{
  BEGIN();

  /* Invoke every API function with invalid arguments */

  int level;

  int status; char const *phrase;

  if (print_headings)
    printf("TEST NUA-1.0: test API\n");

  /* This is a nasty macro. Test it. */
#define SET_STATUS1(x) ((status = x), status), (phrase = ((void)x))
  TEST_1(check_set_status(SET_STATUS1(SIP_200_OK)));
  TEST(status, 200); TEST_S(phrase, sip_200_OK);

  su_log_init(nua_log);
  level = nua_log->log_level;
  if (!(tstflags & tst_verbatim))
    su_log_set_level(nua_log, 0);  /* Log at level 0 by default */

  TEST_1(!nua_create(NULL, NULL, NULL, TAG_END()));
  TEST_VOID(nua_shutdown(NULL));
  TEST_VOID(nua_destroy(NULL));
  TEST_VOID(nua_set_params(NULL, TAG_END()));
  TEST_VOID(nua_get_params(NULL, TAG_END()));
  TEST_1(!nua_default(NULL));
  TEST_1(!nua_handle(NULL, NULL, TAG_END()));
  TEST_VOID(nua_handle_destroy(NULL));
  TEST_VOID(nua_handle_bind(NULL, NULL));
  TEST_1(!nua_handle_has_invite(NULL));
  TEST_1(!nua_handle_has_subscribe(NULL));
  TEST_1(!nua_handle_has_register(NULL));
  TEST_1(!nua_handle_has_active_call(NULL));
  TEST_1(!nua_handle_has_call_on_hold(NULL));
  TEST_1(!nua_handle_has_events(NULL));
  TEST_1(!nua_handle_has_registrations(NULL));
  TEST_1(!nua_handle_remote(NULL));
  TEST_1(!nua_handle_local(NULL));
  TEST_S(nua_event_name(-111), "NUA_UNKNOWN");
  TEST_VOID(nua_register(NULL, TAG_END()));
  TEST_VOID(nua_unregister(NULL, TAG_END()));
  TEST_VOID(nua_invite(NULL, TAG_END()));
  TEST_VOID(nua_ack(NULL, TAG_END()));
  TEST_VOID(nua_prack(NULL, TAG_END()));
  TEST_VOID(nua_options(NULL, TAG_END()));
  TEST_VOID(nua_publish(NULL, TAG_END()));
  TEST_VOID(nua_message(NULL, TAG_END()));
  TEST_VOID(nua_chat(NULL, TAG_END()));
  TEST_VOID(nua_info(NULL, TAG_END()));
  TEST_VOID(nua_subscribe(NULL, TAG_END()));
  TEST_VOID(nua_unsubscribe(NULL, TAG_END()));
  TEST_VOID(nua_notify(NULL, TAG_END()));
  TEST_VOID(nua_notifier(NULL, TAG_END()));
  TEST_VOID(nua_terminate(NULL, TAG_END()));
  TEST_VOID(nua_refer(NULL, TAG_END()));
  TEST_VOID(nua_update(NULL, TAG_END()));
  TEST_VOID(nua_bye(NULL, TAG_END()));
  TEST_VOID(nua_cancel(NULL, TAG_END()));
  TEST_VOID(nua_authenticate(NULL, TAG_END()));
  TEST_VOID(nua_redirect(NULL, TAG_END()));
  TEST_VOID(nua_respond(NULL, 0, "", TAG_END()));

  TEST_1(!nua_handle_home(NULL));
  TEST_1(!nua_save_event(NULL, NULL));
  TEST_1(!nua_event_data(NULL));
  TEST_VOID(nua_destroy_event(NULL));

  {
    nua_saved_event_t event[1];

    memset(event, 0, sizeof event);

    TEST_1(!nua_save_event(NULL, event));
    TEST_1(!nua_event_data(event));
    TEST_VOID(nua_destroy_event(event));
  }

  su_log_set_level(nua_log, level);

  if (print_headings)
    printf("TEST NUA-1.0: PASSED\n");

  END();
}

/* ======================================================================== */

void destroy_callback(nua_event_t event,
		      int status, char const *phrase,
		      nua_t *nua, struct context *ctx,
		      nua_handle_t *nh, struct call *call,
		      sip_t const *sip,
		      tagi_t tags[])
{
  if (status >= 200) {
    nua_destroy(ctx->a.nua), ctx->a.nua = NULL;
    su_root_break(ctx->root);
  }
}

/* Test different nua_destroy() corner cases */
int test_nua_destroy(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a;

  TEST_1(ctx->root = su_root_create(NULL));

#if 0
  a->nua = nua_create(ctx->root, destroy_callback, ctx,
		      NUTAG_URL("sip:0.0.0.0:*"),
		      TAG_IF(ctx->a.logging, TPTAG_LOG(1)),
		      TAG_END());
  TEST_1(a->nua);

  nua_get_params(a->nua, TAG_ANY(), TAG_END());

  su_root_run(ctx->root);

  TEST_1(a->nua == NULL);
#endif

  a->nua = nua_create(ctx->root, destroy_callback, ctx,
		      NUTAG_URL("sip:0.0.0.0:*"),
		      TAG_IF(ctx->a.logging, TPTAG_LOG(1)),
		      TAG_END());
  TEST_1(a->nua);

  nua_shutdown(a->nua);

  su_root_run(ctx->root);

  TEST_1(a->nua == NULL);

  su_root_destroy(ctx->root), ctx->root = NULL;

  END();
}

/* ======================================================================== */

int test_byecancel_without_invite(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a, *b = &ctx->b;
  struct call *a_call = a->call;
  struct event *e;

  int internal_error = 900;

  if (print_headings)
    printf("TEST NUA-1.2.1: CANCEL without INVITE\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  CANCEL(a, a_call, a_call->nh, TAG_END());

  run_a_until(ctx, -1, save_until_final_response);

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_cancel);
  TEST(e->data->e_status, 481);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-1.2.1: PASSED\n");

  /* -BYE without INVITE--------------------------------------------------- */

  if (print_headings)
    printf("TEST NUA-1.2.2: BYE without INVITE\n");

  a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END());
  TEST_1(a_call->nh);

  BYE(a, a_call, a_call->nh, TAG_END());

  run_a_until(ctx, -1, save_until_final_response);

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_bye);
  TEST(e->data->e_status, internal_error);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-1.2.2: PASSED\n");

  END();
}


int test_unregister_without_register(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a, *b = &ctx->b;
  struct call *a_call = a->call;
  struct event *e;

  /* -Un-register without REGISTER--------------------------------------- */

  if (print_headings)
    printf("TEST NUA-1.2.3: unregister without register\n");

  a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(a->to), TAG_END());
  TEST_1(a_call->nh);

  UNREGISTER(a, a_call, a_call->nh, TAG_END());

  run_a_until(ctx, -1, save_until_final_response);

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_unregister);
  if (e->data->e_status == 401)
    TEST(e->data->e_status, 401);
  else
    TEST(e->data->e_status, 406);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-1.2.3: PASSED\n");

  /* -Un-publish without publish--------------------------------------- */

  if (print_headings)
    printf("TEST NUA-1.2.4: unpublish without publish\n");

  a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END());
  TEST_1(a_call->nh);

  UNPUBLISH(a, a_call, a_call->nh, TAG_END());

  run_a_until(ctx, -1, save_until_final_response);

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_unpublish);
  TEST(e->data->e_status, 404);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);
  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-1.2.4: PASSED\n");

  END();
}

/* -terminate without notifier--------------------------------------- */

int test_terminate_without_notifier(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a, *b = &ctx->b;
  struct call *a_call = a->call;
  struct event *e;

  int internal_error = 900;

  if (print_headings)
    printf("TEST NUA-1.2.5: terminate without notifier\n");

  TEST_1(a_call->nh = nua_handle(a->nua, a_call, SIPTAG_TO(b->to), TAG_END()));

  TERMINATE(a, a_call, a_call->nh, TAG_END());

  run_a_until(ctx, -1, save_until_final_response);

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_terminate);
  TEST(e->data->e_status, internal_error);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);

  AUTHORIZE(a, a_call, a_call->nh, TAG_END());

  run_a_until(ctx, -1, save_until_final_response);

  TEST_1(e = a->events->head); TEST_E(e->data->e_event, nua_r_authorize);
  TEST(e->data->e_status, internal_error);
  TEST_1(!e->next);

  free_events_in_list(ctx, a->events);

  nua_handle_destroy(a_call->nh), a_call->nh = NULL;

  if (print_headings)
    printf("TEST NUA-1.2.5: PASSED\n");

  END();
}

int destroy_on_503(CONDITION_PARAMS)
{
  save_event_in_list(ctx, event, ep, call);

  if (status == 503) {
    assert(nh == call->nh);
    nua_handle_destroy(call->nh), call->nh = NULL;
  }

  return
    nua_r_set_params <= event && event < nua_i_network_changed
    && status >= 200;
}


int test_register_503(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a;
  struct call *a_reg = a->reg;
  struct event *e;

/* REGISTER test

   A
   |------REGISTER--\
   |<-------503-----/
   |

*/

  if (print_headings)
    printf("TEST NUA-1.2.6: REGISTER with bad domain\n");

  TEST_1(a_reg->nh = nua_handle(a->nua, a_reg, TAG_END()));

  REGISTER(a, a_reg, a_reg->nh,
	   NUTAG_REGISTRAR(URL_STRING_MAKE("sip:bad.domain")),
	   SIPTAG_TO_STR("sip:lissu@bad.domain"),
	   TAG_END());
  run_a_until(ctx, -1, destroy_on_503);

  TEST_1(e = a->events->head);
  TEST_E(e->data->e_event, nua_r_register);
  TEST(e->data->e_status, 503);
  TEST_1(!e->next);
  free_events_in_list(ctx, a->events);

  TEST_1(a_reg->nh = nua_handle(a->nua, a_reg, TAG_END()));

  REGISTER(a, a_reg, a_reg->nh,
	   NUTAG_REGISTRAR(URL_STRING_MAKE("sip:bad.domain")),
	   SIPTAG_TO_STR("sip:lissu@bad.domain"),
	   TAG_END());
  nua_handle_destroy(a_reg->nh), a_reg->nh = NULL;

  if (print_headings)
    printf("TEST NUA-1.2.6: PASSED\n");

  END();
}

int test_stack_errors(struct context *ctx)
{
  BEGIN();

  struct endpoint *a = &ctx->a;

  if (print_headings)
    printf("TEST NUA-1.2: Stack error handling\n");

  TEST_1(ctx->root == NULL);
  TEST_1(ctx->root = su_root_create(NULL));

  a->nua = nua_create(ctx->root, a_callback, ctx,
		      NUTAG_URL("sip:0.0.0.0:*"),
		      TAG_IF(ctx->a.logging, TPTAG_LOG(1)),
		      TAG_END());
  TEST_1(a->nua);

  TEST(test_byecancel_without_invite(ctx), 0);
  if (ctx->proxy_tests)
    TEST(test_unregister_without_register(ctx), 0);
  TEST(test_terminate_without_notifier(ctx), 0);
  TEST(test_register_503(ctx), 0);
  TEST(test_register_503(ctx), 0);

  nua_shutdown(a->nua);

  run_a_until(ctx, -1, until_final_response);

  TEST_VOID(nua_destroy(a->nua));

  a->nua = NULL;

  su_root_destroy(ctx->root), ctx->root = NULL;

  if (print_headings)
    printf("TEST NUA-1.2: PASSED\n");

  END();
}

