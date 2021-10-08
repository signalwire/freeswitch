/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2007 Nokia Corporation.
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

/**@CFILE suite_for_nua.c
 *
 * @brief Check-driven tester for Sofia SIP User Agent library
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @copyright (C) 2007 Nokia Corporation.
 */

#include "config.h"

#include "test_nua.h"
#include "check_sofia.h"

int print_headings = 0;
struct context ctx[1] = {{{ SU_HOME_INIT(ctx) }}};

SOFIAPUBVAR su_log_t nua_log[];
SOFIAPUBVAR su_log_t soa_log[];
SOFIAPUBVAR su_log_t nea_log[];
SOFIAPUBVAR su_log_t nta_log[];
SOFIAPUBVAR su_log_t tport_log[];
SOFIAPUBVAR su_log_t su_log_default[];

static void init_context(void)
{
  int level = 1;

  su_init();
  memset(ctx, 0, sizeof ctx);
  su_home_init(ctx->home);
  endpoint_init(ctx, &ctx->a, 'a');
  endpoint_init(ctx, &ctx->b, 'b');
  endpoint_init(ctx, &ctx->c, 'c');

  su_log_soft_set_level(nua_log, level);
  su_log_soft_set_level(soa_log, level);
  su_log_soft_set_level(su_log_default, level);
  su_log_soft_set_level(nea_log, level);
  su_log_soft_set_level(nta_log, level);
  su_log_soft_set_level(tport_log, level);
}

static void clean_context(void)
{
  test_deinit(ctx);
  su_home_deinit(ctx->home);
  memset(ctx, 0, sizeof ctx);
  su_deinit();
}

/* Each testcase is run in different process */

START_TEST(check_api) { fail_if(test_nua_api_errors(ctx)); } END_TEST
START_TEST(check_tag_filter) { fail_if(test_tag_filter()); } END_TEST
START_TEST(check_params) { fail_if(test_nua_params(ctx)); } END_TEST
START_TEST(check_destroy) { fail_if(test_nua_destroy(ctx)); } END_TEST
START_TEST(check_stack_errors) { fail_if(test_stack_errors(ctx)); } END_TEST

START_TEST(without_proxy)
{
  fail_if(test_nua_init(ctx, 0, NULL, 0, TAG_END()));
  fail_if(test_register(ctx));
  fail_if(test_connectivity(ctx));
  fail_if(test_basic_call(ctx));
  fail_if(test_rejects(ctx));
  fail_if(test_call_cancel(ctx));
  fail_if(test_call_destroy(ctx));
  fail_if(test_early_bye(ctx));
  fail_if(test_offer_answer(ctx));
  fail_if(test_reinvites(ctx));
  fail_if(test_session_timer(ctx));
  fail_if(test_refer(ctx));
  fail_if(test_100rel(ctx));
  fail_if(test_simple(ctx));
  fail_if(test_events(ctx));
  fail_if(test_extension(ctx));
  fail_if(test_unregister(ctx));
  fail_if(test_deinit(ctx));
}
END_TEST

START_TEST(with_proxy)
{
  fail_if(test_nua_init(ctx, 1, NULL, 0, TAG_END()));
  fail_if(test_register(ctx));
  fail_if(test_connectivity(ctx));
  fail_if(test_basic_call(ctx));
  fail_if(test_rejects(ctx));
  fail_if(test_call_cancel(ctx));
  fail_if(test_call_destroy(ctx));
  fail_if(test_early_bye(ctx));
  fail_if(test_offer_answer(ctx));
  fail_if(test_reinvites(ctx));
  fail_if(test_session_timer(ctx));
  fail_if(test_refer(ctx));
  fail_if(test_100rel(ctx));
  fail_if(test_simple(ctx));
  fail_if(test_events(ctx));
  fail_if(test_extension(ctx));
  fail_if(test_unregister(ctx));
  fail_if(test_deinit(ctx));
}
END_TEST

START_TEST(with_proxy_and_nat)
{
  fail_if(test_nua_init(ctx, 1, NULL, 1, TAG_END()));
  fail_if(test_register(ctx));
  fail_if(test_connectivity(ctx));
  fail_if(test_nat_timeout(ctx));
  fail_if(test_basic_call(ctx));
  fail_if(test_rejects(ctx));
  fail_if(test_call_cancel(ctx));
  fail_if(test_call_destroy(ctx));
  fail_if(test_early_bye(ctx));
  fail_if(test_offer_answer(ctx));
  fail_if(test_reinvites(ctx));
  fail_if(test_session_timer(ctx));
  fail_if(test_refer(ctx));
  fail_if(test_100rel(ctx));
  fail_if(test_simple(ctx));
  fail_if(test_events(ctx));
  fail_if(test_extension(ctx));
  fail_if(test_unregister(ctx));
  fail_if(test_deinit(ctx));
}
END_TEST

Suite *suite_for_nua(void)
{
  Suite *suite = suite_create("nua");
  TCase *tc;

  tc = tcase_create("api");
  tcase_add_unchecked_fixture(tc, init_context, clean_context);
  tcase_add_test(tc, check_api);
  tcase_add_test(tc, check_tag_filter);
  tcase_add_test(tc, check_params);
  tcase_add_test(tc, check_destroy);
  tcase_add_test(tc, check_stack_errors);
  tcase_set_timeout(tc, 5);
  suite_add_tcase(suite, tc);

  tc = tcase_create("without-proxy");
  tcase_add_unchecked_fixture(tc, init_context, clean_context);
  tcase_add_test(tc, without_proxy);
  tcase_set_timeout(tc, 60);
  suite_add_tcase(suite, tc);

  tc = tcase_create("with-proxy");
  tcase_add_unchecked_fixture(tc, init_context, clean_context);
  tcase_add_test(tc, with_proxy);
  tcase_set_timeout(tc, 120);
  suite_add_tcase(suite, tc);

  tc = tcase_create("with-proxy-and-nat");
  tcase_add_unchecked_fixture(tc, init_context, clean_context);
  tcase_add_test(tc, with_proxy_and_nat);
  tcase_set_timeout(tc, 120);
  suite_add_tcase(suite, tc);

  return suite;
}
