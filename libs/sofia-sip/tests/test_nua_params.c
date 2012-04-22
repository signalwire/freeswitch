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

/**@CFILE test_nua_params.c
 * @brief Test NUA parameter handling.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti Mela@nokia.com>
 *
 * @date Created: Wed Aug 17 12:12:12 EEST 2005 ppessi
 */

#include "config.h"

#include "test_nua.h"
#include <sofia-sip/su_tag_class.h>

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
#define __func__ "test_nua_params"
#endif

sip_route_t *GLOBAL_ROUTE;

int test_tag_filter(void)
{
  BEGIN();

#undef TAG_NAMESPACE
#define TAG_NAMESPACE "test"
  tag_typedef_t tag_a = STRTAG_TYPEDEF(a);
#define TAG_A(s)      tag_a, tag_str_v((s))
  tag_typedef_t tag_b = STRTAG_TYPEDEF(b);
#define TAG_B(s)      tag_b, tag_str_v((s))

  tagi_t filter[2] = {{ NUTAG_ANY() }, { TAG_END() }};

  tagi_t *lst, *result;

  lst = tl_list(TAG_A("X"),
		TAG_SKIP(2),
		NUTAG_URL((void *)"urn:foo"),
		TAG_B("Y"),
		NUTAG_URL((void *)"urn:bar"),
		TAG_NULL());

  TEST_1(lst);

  result = tl_afilter(NULL, filter, lst);

  TEST_1(result);
  TEST_P(result[0].t_tag, nutag_url);
  TEST_P(result[1].t_tag, nutag_url);

  tl_vfree(lst);
  free(result);

  END();
}

int test_nua_params(struct context *ctx)
{
  BEGIN();

  char const Alice[] = "Alice <sip:a@wonderland.org>";
  sip_from_t const *from;
  su_home_t tmphome[SU_HOME_AUTO_SIZE(16384)];
  nua_handle_t *nh;
  struct event *e;
  tagi_t const *t;
  int n;

  su_home_auto(tmphome, sizeof(tmphome));

  if (print_headings)
    printf("TEST NUA-1.1: PARAMETERS\n");

#if SU_HAVE_OSX_CF_API
  if (ctx->osx_runloop)
    ctx->root = su_root_osx_runloop_create(NULL);
  else
#endif
  ctx->root = su_root_create(NULL);
  TEST_1(ctx->root);

  su_root_threading(ctx->root, ctx->threading);

  ctx->a.nua = nua_create(ctx->root, a_callback, ctx,
			  SIPTAG_FROM_STR("sip:alice@example.com"),
			  NUTAG_URL("sip:0.0.0.0:*;transport=udp"),
			  TAG_END());

  TEST_1(ctx->a.nua);

  nua_get_params(ctx->a.nua, TAG_ANY(), TAG_END());
  run_a_until(ctx, nua_r_get_params, save_until_final_response);

  TEST_1(e = ctx->a.specials->head);
  TEST_E(e->data->e_event, nua_r_get_params);
  for (n = 0, t = e->data->e_tags; t; n++, t = tl_next(t))
    ;
  TEST_1(n > 32);
  free_events_in_list(ctx, ctx->a.specials);

  nh = nua_handle(ctx->a.nua, NULL, TAG_END()); TEST_1(nh);
  nua_handle_unref(nh);

  nh = nua_handle(ctx->a.nua, NULL, TAG_END()); TEST_1(nh);
  nua_handle_destroy(nh);

  from = sip_from_make(tmphome, Alice);

  nh = nua_handle(ctx->a.nua, NULL, TAG_END());

  nua_set_hparams(nh, NUTAG_INVITE_TIMER(90), TAG_END());
  run_a_until(ctx, nua_r_set_params, until_final_response);

  /* Modify all pointer values */
  nua_set_params(ctx->a.nua,
		 SIPTAG_FROM_STR(Alice),

		 NUTAG_MEDIA_ENABLE(0),
		 NUTAG_SOA_NAME("test"),

		 NUTAG_REGISTRAR("sip:openlaboratory.net"),

		 SIPTAG_SUPPORTED_STR("test"),
		 SIPTAG_ALLOW_STR("DWIM, OPTIONS, INFO"),
		 NUTAG_APPL_METHOD(NULL),
		 NUTAG_APPL_METHOD("INVITE, REGISTER, PUBLISH, SUBSCRIBE"),
		 SIPTAG_ALLOW_EVENTS_STR("reg"),
		 SIPTAG_USER_AGENT_STR("test_nua/1.0"),

		 SIPTAG_ORGANIZATION_STR("Open Laboratory"),

		 NUTAG_M_DISPLAY("XXX"),
		 NUTAG_M_USERNAME("xxx"),
		 NUTAG_M_PARAMS("user=ip"),
		 NUTAG_M_FEATURES("language=\"fi\""),
		 NUTAG_INSTANCE("urn:uuid:3eb007b1-6d7f-472e-8b64-29e482795da8"),
		 NUTAG_OUTBOUND("bar"),

		 NUTAG_INITIAL_ROUTE(NULL),
		 NUTAG_INITIAL_ROUTE(sip_route_make(tmphome, "<sip:tst@example.net;lr>")),
		 NUTAG_INITIAL_ROUTE_STR("<sip:str1@example.net;lr>"),
		 NUTAG_INITIAL_ROUTE_STR("sip:str2@example.net;lr=foo"),
		 NUTAG_INITIAL_ROUTE_STR(NULL),

		 TAG_END());

  run_a_until(ctx, nua_r_set_params, until_final_response);

  /* Modify everything from their default value */
  nua_set_params(ctx->a.nua,
		 SIPTAG_FROM(from),
		 NUTAG_RETRY_COUNT(9),
		 NUTAG_MAX_SUBSCRIPTIONS(6),

		 NUTAG_ENABLEINVITE(0),
		 NUTAG_AUTOALERT(1),
		 NUTAG_EARLY_MEDIA(1),
		 NUTAG_AUTOANSWER(1),
		 NUTAG_AUTOACK(0),
		 NUTAG_INVITE_TIMER(60),

		 NUTAG_SESSION_TIMER(600),
		 NUTAG_MIN_SE(35),
		 NUTAG_SESSION_REFRESHER(nua_remote_refresher),
		 NUTAG_UPDATE_REFRESH(1),

		 NUTAG_ENABLEMESSAGE(0),
		 NUTAG_ENABLEMESSENGER(1),
		 /* NUTAG_MESSAGE_AUTOANSWER(0), */

		 NUTAG_CALLEE_CAPS(1),
		 NUTAG_MEDIA_FEATURES(1),
		 NUTAG_SERVICE_ROUTE_ENABLE(0),
		 NUTAG_PATH_ENABLE(0),
		 NUTAG_AUTH_CACHE(nua_auth_cache_challenged),
		 NUTAG_REFER_EXPIRES(333),
		 NUTAG_REFER_WITH_ID(0),
		 NUTAG_SUBSTATE(nua_substate_pending),
		 NUTAG_SUB_EXPIRES(3700),

		 NUTAG_KEEPALIVE(66),
		 NUTAG_KEEPALIVE_STREAM(33),

		 NUTAG_INSTANCE("urn:uuid:97701ad9-39df-1229-1083-dbc0a85f029c"),
		 NUTAG_M_DISPLAY("Joe"),
		 NUTAG_M_USERNAME("joe"),
		 NUTAG_M_PARAMS("user=phone"),
		 NUTAG_M_FEATURES("language=\"en\""),
		 NUTAG_OUTBOUND("foo"),
		 SIPTAG_SUPPORTED(sip_supported_make(tmphome, "foo")),
		 NUTAG_SUPPORTED("foo, bar"),
		 SIPTAG_SUPPORTED_STR(",baz,"),

		 SIPTAG_ALLOW_STR("OPTIONS"),
		 SIPTAG_ALLOW(sip_allow_make(tmphome, "INFO")),
		 NUTAG_ALLOW("ACK, INFO"),

		 NUTAG_APPL_METHOD("NOTIFY"),

		 SIPTAG_ALLOW_EVENTS_STR("reg"),
		 SIPTAG_ALLOW_EVENTS(sip_allow_events_make(tmphome, "presence")),
		 NUTAG_ALLOW_EVENTS("presence.winfo"),

		 NUTAG_INITIAL_ROUTE(NULL),
		 NUTAG_INITIAL_ROUTE(sip_route_make(nua_handle_home(nh), "<sip:1@example.com;lr>")),
		 NUTAG_INITIAL_ROUTE_STR("<sip:2@example.com;lr>"),
		 /* Check for sip_route_fix() */
		 NUTAG_INITIAL_ROUTE_STR("sip:3@example.com;lr=foo"),
		 NUTAG_INITIAL_ROUTE_STR(NULL),

		 SIPTAG_USER_AGENT(sip_user_agent_make(tmphome, "test_nua")),

		 SIPTAG_ORGANIZATION(sip_organization_make(tmphome, "Pussy Galore's Flying Circus")),

		 NUTAG_MEDIA_ENABLE(0),
		 NUTAG_REGISTRAR(url_hdup(tmphome, (url_t *)"sip:sip.wonderland.org")),

		 TAG_END());

  run_a_until(ctx, nua_r_set_params, until_final_response);

  /* Modify something... */
  nua_set_params(ctx->a.nua,
		 NUTAG_RETRY_COUNT(5),
		 TAG_END());
  run_a_until(ctx, nua_r_set_params, until_final_response);

  {
    sip_from_t const *from = NONE;
    char const *from_str = "NONE";

    unsigned retry_count = (unsigned)-1;
    unsigned max_subscriptions = (unsigned)-1;

    char const *soa_name = "NONE";
    int media_enable = -1;
    int invite_enable = -1;
    int auto_alert = -1;
    int early_media = -1;
    int only183_100rel = -1;
    int auto_answer = -1;
    int auto_ack = -1;
    unsigned invite_timeout = (unsigned)-1;

    unsigned session_timer = (unsigned)-1;
    unsigned min_se = (unsigned)-1;
    int refresher = -1;
    int update_refresh = -1;

    int message_enable = -1;
    int win_messenger_enable = -1;
    int message_auto_respond = -1;

    int callee_caps = -1;
    int media_features = -1;
    int service_route_enable = -1;
    int path_enable = -1;
    int auth_cache = -1;
    unsigned refer_expires = (unsigned)-1;
    int refer_with_id = -1;
    unsigned sub_expires = (unsigned)-1;
    int substate = -1;

    sip_allow_t const *allow = NONE;
    char const *allow_str = "NONE";
    char const *appl_method = "NONE";
    sip_allow_events_t const *allow_events = NONE;
    char const *allow_events_str = "NONE";
    sip_supported_t const *supported = NONE;
    char const *supported_str = "NONE";
    sip_user_agent_t const *user_agent = NONE;
    char const *user_agent_str = "NONE";
    char const *ua_name = "NONE";
    sip_organization_t const *organization = NONE;
    char const *organization_str = "NONE";

    sip_route_t const *initial_route = NONE;
    char const *initial_route_str = NONE;

    char const *outbound = "NONE";
    char const *m_display = "NONE";
    char const *m_username = "NONE";
    char const *m_params = "NONE";
    char const *m_features = "NONE";
    char const *instance = "NONE";

    url_string_t const *registrar = NONE;
    unsigned keepalive = (unsigned)-1, keepalive_stream = (unsigned)-1;

    nua_get_params(ctx->a.nua, TAG_ANY(), TAG_END());
    run_a_until(ctx, nua_r_get_params, save_until_final_response);

    TEST_1(e = ctx->a.specials->head);
    TEST_E(e->data->e_event, nua_r_get_params);

    n = tl_gets(e->data->e_tags,
	       	SIPTAG_FROM_REF(from),
	       	SIPTAG_FROM_STR_REF(from_str),

	       	NUTAG_RETRY_COUNT_REF(retry_count),
	       	NUTAG_MAX_SUBSCRIPTIONS_REF(max_subscriptions),

		NUTAG_SOA_NAME_REF(soa_name),
		NUTAG_MEDIA_ENABLE_REF(media_enable),
	       	NUTAG_ENABLEINVITE_REF(invite_enable),
	       	NUTAG_AUTOALERT_REF(auto_alert),
	       	NUTAG_EARLY_MEDIA_REF(early_media),
		NUTAG_ONLY183_100REL_REF(only183_100rel),
	       	NUTAG_AUTOANSWER_REF(auto_answer),
	       	NUTAG_AUTOACK_REF(auto_ack),
	       	NUTAG_INVITE_TIMER_REF(invite_timeout),

	       	NUTAG_SESSION_TIMER_REF(session_timer),
	       	NUTAG_MIN_SE_REF(min_se),
	       	NUTAG_SESSION_REFRESHER_REF(refresher),
	       	NUTAG_UPDATE_REFRESH_REF(update_refresh),

	       	NUTAG_ENABLEMESSAGE_REF(message_enable),
	       	NUTAG_ENABLEMESSENGER_REF(win_messenger_enable),
	       	/* NUTAG_MESSAGE_AUTOANSWER(message_auto_respond), */

	       	NUTAG_CALLEE_CAPS_REF(callee_caps),
	       	NUTAG_MEDIA_FEATURES_REF(media_features),
	       	NUTAG_SERVICE_ROUTE_ENABLE_REF(service_route_enable),
	       	NUTAG_PATH_ENABLE_REF(path_enable),
	       	NUTAG_AUTH_CACHE_REF(auth_cache),
	       	NUTAG_REFER_EXPIRES_REF(refer_expires),
	       	NUTAG_REFER_WITH_ID_REF(refer_with_id),
	       	NUTAG_SUBSTATE_REF(substate),
		NUTAG_SUB_EXPIRES_REF(sub_expires),

	       	SIPTAG_SUPPORTED_REF(supported),
	       	SIPTAG_SUPPORTED_STR_REF(supported_str),
	       	SIPTAG_ALLOW_REF(allow),
	       	SIPTAG_ALLOW_STR_REF(allow_str),
		NUTAG_APPL_METHOD_REF(appl_method),
		SIPTAG_ALLOW_EVENTS_REF(allow_events),
		SIPTAG_ALLOW_EVENTS_STR_REF(allow_events_str),
	       	SIPTAG_USER_AGENT_REF(user_agent),
	       	SIPTAG_USER_AGENT_STR_REF(user_agent_str),
		NUTAG_USER_AGENT_REF(ua_name),

	       	SIPTAG_ORGANIZATION_REF(organization),
	       	SIPTAG_ORGANIZATION_STR_REF(organization_str),

		NUTAG_INITIAL_ROUTE_REF(initial_route),
		NUTAG_INITIAL_ROUTE_STR_REF(initial_route_str),

	       	NUTAG_REGISTRAR_REF(registrar),
		NUTAG_KEEPALIVE_REF(keepalive),
		NUTAG_KEEPALIVE_STREAM_REF(keepalive_stream),

		NUTAG_OUTBOUND_REF(outbound),
		NUTAG_M_DISPLAY_REF(m_display),
		NUTAG_M_USERNAME_REF(m_username),
		NUTAG_M_PARAMS_REF(m_params),
		NUTAG_M_FEATURES_REF(m_features),
		NUTAG_INSTANCE_REF(instance),

		TAG_END());
    TEST(n, 51);

    TEST_S(sip_header_as_string(tmphome, (void *)from), Alice);
    TEST_S(from_str, Alice);

    TEST(retry_count, 5);
    TEST(max_subscriptions, 6);

    TEST_S(soa_name, "test");
    TEST(media_enable, 0);
    TEST(invite_enable, 0);
    TEST(auto_alert, 1);
    TEST(early_media, 1);
    TEST(auto_answer, 1);
    TEST(auto_ack, 0);
    TEST(invite_timeout, 60);

    TEST(session_timer, 600);
    TEST(min_se, 35);
    TEST(refresher, nua_remote_refresher);
    TEST(update_refresh, 1);

    TEST(message_enable, 0);
    TEST(win_messenger_enable, 1);
    TEST(message_auto_respond, -1); /* XXX */

    TEST(callee_caps, 1);
    TEST(media_features, 1);
    TEST(service_route_enable, 0);
    TEST(path_enable, 0);
    TEST(auth_cache, nua_auth_cache_challenged);
    TEST(refer_expires, 333);
    TEST(refer_with_id, 0);
    TEST(substate, nua_substate_pending);
    TEST(sub_expires, 3700);

    TEST_S(sip_header_as_string(tmphome, (void *)allow), "OPTIONS, INFO, ACK");
    TEST_S(allow_str, "OPTIONS, INFO, ACK");
    TEST_S(appl_method, "INVITE, REGISTER, PUBLISH, SUBSCRIBE, NOTIFY");
    TEST_S(sip_header_as_string(tmphome, (void *)allow_events),
	   "reg, presence, presence.winfo");
    TEST_S(allow_events_str, "reg, presence, presence.winfo");
    TEST_S(sip_header_as_string(tmphome, (void *)supported),
	   "foo, bar, baz");
    TEST_S(supported_str, "foo, bar, baz");
    TEST_S(sip_header_as_string(tmphome, (void *)user_agent), "test_nua");
    TEST_S(user_agent_str, "test_nua");
    TEST_S(sip_header_as_string(tmphome, (void *)organization),
	   "Pussy Galore's Flying Circus");
    TEST_S(organization_str, "Pussy Galore's Flying Circus");

    TEST_1(initial_route); TEST_1(initial_route != (void *)-1);
    TEST_S(initial_route->r_url->url_user, "1");
    TEST_1(url_has_param(initial_route->r_url, "lr"));
    TEST_1(initial_route->r_next);
    TEST_S(initial_route->r_next->r_url->url_user, "2");
    TEST_1(url_has_param(initial_route->r_next->r_url, "lr"));
    TEST_1(initial_route->r_next->r_next);
    TEST_S(initial_route->r_next->r_next->r_url->url_user, "3");
    TEST_1(url_has_param(initial_route->r_next->r_next->r_url, "lr"));
    TEST_1(!initial_route->r_next->r_next->r_next);

    TEST_S(url_as_string(tmphome, registrar->us_url),
	   "sip:sip.wonderland.org");
    TEST(keepalive, 66);
    TEST(keepalive_stream, 33);

    TEST_S(instance, "urn:uuid:97701ad9-39df-1229-1083-dbc0a85f029c");
    TEST_S(m_display, "Joe");
    TEST_S(m_username, "joe");
    TEST_S(m_params, "user=phone");
    { char const *expect_m_features = "language=\"en\"";
    TEST_S(m_features, expect_m_features); }
    TEST_S(outbound, "foo");

    free_events_in_list(ctx, ctx->a.specials);
  }

  /* Test that only those tags that have been set per handle are returned by nua_get_hparams() */

  {
    sip_from_t const *from = NONE;
    char const *from_str = "NONE";

    unsigned retry_count = (unsigned)-1;
    unsigned max_subscriptions = (unsigned)-1;

    int invite_enable = -1;
    int auto_alert = -1;
    int early_media = -1;
    int auto_answer = -1;
    int auto_ack = -1;
    unsigned invite_timeout = (unsigned)-1;

    unsigned session_timer = (unsigned)-1;
    unsigned min_se = (unsigned)-1;
    int refresher = -1;
    int update_refresh = -1;

    int message_enable = -1;
    int win_messenger_enable = -1;
    int message_auto_respond = -1;

    int callee_caps = -1;
    int media_features = -1;
    int service_route_enable = -1;
    int path_enable = -1;
    int auth_cache = -1;
    unsigned refer_expires = (unsigned)-1;
    int refer_with_id = -1;
    int substate = -1;
    unsigned sub_expires = (unsigned)-1;

    sip_allow_t const *allow = NONE;
    char const   *allow_str = "NONE";
    sip_supported_t const *supported = NONE;
    char const *supported_str = "NONE";
    sip_user_agent_t const *user_agent = NONE;
    char const *user_agent_str = "NONE";
    sip_organization_t const *organization = NONE;
    char const *organization_str = "NONE";

    sip_route_t *initial_route = NONE;
    char const *initial_route_str = "NONE";

    url_string_t const *registrar = NONE;

    char const *outbound = "NONE";
    char const *m_display = "NONE";
    char const *m_username = "NONE";
    char const *m_params = "NONE";
    char const *m_features = "NONE";
    char const *instance = "NONE";

    int n;
    struct event *e;

    nua_get_hparams(nh, TAG_ANY(), TAG_END());
    run_a_until(ctx, nua_r_get_params, save_until_final_response);

    TEST_1(e = ctx->a.events->head);
    TEST_E(e->data->e_event, nua_r_get_params);

    n = tl_gets(e->data->e_tags,
	       	SIPTAG_FROM_REF(from),
	       	SIPTAG_FROM_STR_REF(from_str),

	       	NUTAG_RETRY_COUNT_REF(retry_count),
	       	NUTAG_MAX_SUBSCRIPTIONS_REF(max_subscriptions),

	       	NUTAG_ENABLEINVITE_REF(invite_enable),
	       	NUTAG_AUTOALERT_REF(auto_alert),
	       	NUTAG_EARLY_MEDIA_REF(early_media),
	       	NUTAG_AUTOANSWER_REF(auto_answer),
	       	NUTAG_AUTOACK_REF(auto_ack),
	       	NUTAG_INVITE_TIMER_REF(invite_timeout),

	       	NUTAG_SESSION_TIMER_REF(session_timer),
	       	NUTAG_MIN_SE_REF(min_se),
	       	NUTAG_SESSION_REFRESHER_REF(refresher),
	       	NUTAG_UPDATE_REFRESH_REF(update_refresh),

	       	NUTAG_ENABLEMESSAGE_REF(message_enable),
	       	NUTAG_ENABLEMESSENGER_REF(win_messenger_enable),
	       	/* NUTAG_MESSAGE_AUTOANSWER(message_auto_respond), */

	       	NUTAG_CALLEE_CAPS_REF(callee_caps),
	       	NUTAG_MEDIA_FEATURES_REF(media_features),
	       	NUTAG_SERVICE_ROUTE_ENABLE_REF(service_route_enable),
	       	NUTAG_PATH_ENABLE_REF(path_enable),
		NUTAG_AUTH_CACHE_REF(auth_cache),
	       	NUTAG_SUBSTATE_REF(substate),
	       	NUTAG_SUB_EXPIRES_REF(sub_expires),

	       	SIPTAG_SUPPORTED_REF(supported),
	       	SIPTAG_SUPPORTED_STR_REF(supported_str),
	       	SIPTAG_ALLOW_REF(allow),
	       	SIPTAG_ALLOW_STR_REF(allow_str),
	       	SIPTAG_USER_AGENT_REF(user_agent),
	       	SIPTAG_USER_AGENT_STR_REF(user_agent_str),

	       	SIPTAG_ORGANIZATION_REF(organization),
	       	SIPTAG_ORGANIZATION_STR_REF(organization_str),

		NUTAG_OUTBOUND_REF(outbound),
		NUTAG_M_DISPLAY_REF(m_display),
		NUTAG_M_USERNAME_REF(m_username),
		NUTAG_M_PARAMS_REF(m_params),
		NUTAG_M_FEATURES_REF(m_features),
		NUTAG_INSTANCE_REF(instance),

	       	NUTAG_REGISTRAR_REF(registrar),

		TAG_END());
    TEST(n, 3);

    TEST(invite_timeout, 90);

    TEST_1(from != NULL && from != NONE);
    TEST_1(strcmp(from_str, "NONE"));

    /* Nothing else should be set */
    TEST(retry_count, (unsigned)-1);
    TEST(max_subscriptions, (unsigned)-1);

    TEST(invite_enable, -1);
    TEST(auto_alert, -1);
    TEST(early_media, -1);
    TEST(auto_answer, -1);
    TEST(auto_ack, -1);

    TEST(session_timer, (unsigned)-1);
    TEST(min_se, (unsigned)-1);
    TEST(refresher, -1);
    TEST(update_refresh, -1);

    TEST(message_enable, -1);
    TEST(win_messenger_enable, -1);
    TEST(message_auto_respond, -1); /* XXX */

    TEST(callee_caps, -1);
    TEST(media_features, -1);
    TEST(service_route_enable, -1);
    TEST(path_enable, -1);
    TEST(auth_cache, -1);
    TEST(refer_expires, (unsigned)-1);
    TEST(refer_with_id, -1);
    TEST(substate, -1);
    TEST(sub_expires, (unsigned)-1);

    TEST_P(allow, NONE);
    TEST_S(allow_str, "NONE");
    TEST_P(supported, NONE);
    TEST_S(supported_str, "NONE");
    TEST_P(user_agent, NONE);
    TEST_S(user_agent_str, "NONE");
    TEST_P(organization, NONE);
    TEST_S(organization_str, "NONE");

    TEST_1(initial_route == (void *)-1);
    TEST_S(initial_route_str, "NONE");

    TEST_S(outbound, "NONE");
    TEST_S(m_display, "NONE");
    TEST_S(m_username, "NONE");
    TEST_S(m_params, "NONE");
    TEST_S(m_features, "NONE");
    TEST_S(instance, "NONE");

    TEST_P(registrar->us_url, NONE);

    free_events_in_list(ctx, ctx->a.events);
  }

  nua_handle_destroy(nh);

  nua_shutdown(ctx->a.nua);
  run_a_until(ctx, nua_r_shutdown, until_final_response);
  nua_destroy(ctx->a.nua), ctx->a.nua = NULL;

  su_root_destroy(ctx->root), ctx->root = NULL;

  su_home_deinit(tmphome);

  if (print_headings)
    printf("TEST NUA-1.1: PASSED\n");

  END();
}
