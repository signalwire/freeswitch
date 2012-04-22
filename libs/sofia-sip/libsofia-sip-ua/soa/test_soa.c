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

/**@CFILE test_soa.c
 * @brief High-level tester for Sofia SDP Offer/Answer Engine
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Aug 17 12:12:12 EEST 2005 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

#if HAVE_ALARM
#include <unistd.h>
#include <signal.h>
#endif

struct context;
#define SOA_MAGIC_T struct context

#include "sofia-sip/soa.h"
#include "sofia-sip/soa_tag.h"
#include "sofia-sip/soa_add.h"

#include <sofia-sip/sdp.h>

#include <sofia-sip/su_log.h>
#include <sofia-sip/sip_tag.h>

#include <s2_localinfo.h>

S2_LOCALINFO_STUBS();

extern su_log_t soa_log[];

char const name[] = "test_soa";
int tstflags = 0;
#define TSTFLAGS tstflags

#include <sofia-sip/tstdef.h>

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
#define __func__ name
#endif

#define NONE ((void*)-1)

static char const *test_ifaces1[] = {
  "eth0\0" "11.12.13.14\0" "2001:1508:1003::21a:a0ff:fe71:813\0" "fe80::21a:a0ff:fe71:813\0",
  "eth1\0" "12.13.14.15\0" "2001:1508:1004::21a:a0ff:fe71:814\0" "fe80::21a:a0ff:fe71:814\0",
  "eth2\0" "192.168.2.15\0" "fec0::21a:a0ff:fe71:815\0" "fe80::21a:a0ff:fe71:815\0",
  "lo0\0" "127.0.0.1\0" "::1\0",
  NULL
};

int test_localinfo_replacement(void)
{
  BEGIN();
  su_localinfo_t *res, *li, hints[1];
  int error, n;
  struct results {
    struct afresult { unsigned global, site, link, host; } ip6[1], ip4[1];
  } results[1];

  s2_localinfo_ifaces(test_ifaces1);

  error = su_getlocalinfo(NULL, &res);
  TEST(error, ELI_NOERROR);
  TEST_1(res != NULL);
  memset(results, 0, sizeof results);
  for (li = res, n = 0; li; li = li->li_next) {
    struct afresult *afr;
    TEST_1(li->li_family == AF_INET || li->li_family == AF_INET6);
    if (li->li_family == AF_INET)
      afr = results->ip4;
    else
      afr = results->ip6;

    if (li->li_scope == LI_SCOPE_GLOBAL)
      afr->global++;
    else if (li->li_scope == LI_SCOPE_SITE)
      afr->site++;
    else if (li->li_scope == LI_SCOPE_LINK)
      afr->link++;
    else if (li->li_scope == LI_SCOPE_HOST)
      afr->host++;
    n++;
  }
  TEST(n, 11);
  TEST(results->ip4->global, 2);
  TEST(results->ip4->site, 1);
  TEST(results->ip4->link, 0);
  TEST(results->ip4->host, 1);
#if SU_HAVE_IN6
  TEST(results->ip6->global, 2);
  TEST(results->ip6->site, 1);
  TEST(results->ip6->link, 3);
  TEST(results->ip6->host, 1);
#endif
  su_freelocalinfo(res);

  error = su_getlocalinfo(memset(hints, 0, sizeof hints), &res);
  TEST(error, ELI_NOERROR);
  TEST_1(res != NULL);
  for (li = res, n = 0; li; li = li->li_next)
    n++;
  TEST(n, 11);
  su_freelocalinfo(res);

  hints->li_flags = LI_CANONNAME;

  error = su_getlocalinfo(hints, &res);
  TEST(error, ELI_NOERROR);
  TEST_1(res != NULL);
  for (li = res, n = 0; li; li = li->li_next) {
    TEST_1(li->li_canonname != NULL);
    n++;
  }
  TEST(n, 11);
  su_freelocalinfo(res);

  hints->li_flags = LI_IFNAME | LI_CANONNAME;
  hints->li_ifname = "eth1";

  error = su_getlocalinfo(hints, &res);
  TEST(error, ELI_NOERROR);
  TEST_1(res != NULL);
  for (li = res, n = 0; li; li = li->li_next) {
    TEST_1(li->li_canonname != NULL);
    TEST_S(li->li_ifname, "eth1");
    n++;
  }
  TEST(n, 3);
  su_freelocalinfo(res);

  END();
}
/* ========================================================================= */

struct context
{
  su_home_t home[1];
  su_root_t *root;

  struct {
    soa_session_t *a;
    soa_session_t *b;
  } asynch;

  soa_session_t *a;
  soa_session_t *b;

  soa_session_t *completed;
};

int test_api_completed(struct context *arg, soa_session_t *session)
{
  return 0;
}

int test_api_errors(struct context *ctx)
{
  BEGIN();

  char const *phrase = NULL;
  char const *null = NULL;

  TEST_1(!soa_create("default", NULL, NULL));
  TEST_1(!soa_clone(NULL, NULL, NULL));
  TEST_VOID(soa_destroy(NULL));

  TEST_1(-1 == soa_set_params(NULL, TAG_END()));
  TEST_1(-1 == soa_get_params(NULL, TAG_END()));

  TEST_1(!soa_get_paramlist(NULL, TAG_END()));

  TEST(soa_error_as_sip_response(NULL, &phrase), 500);
  TEST_S(phrase, "Internal Server Error");

  TEST_1(soa_error_as_sip_reason(NULL));

  TEST_1(!soa_media_features(NULL, 0, NULL));

  TEST_1(!soa_sip_require(NULL));
  TEST_1(!soa_sip_supported(NULL));

  TEST_1(-1 == soa_remote_sip_features(NULL, &null, &null));

  TEST_1(soa_set_capability_sdp(NULL, NULL, NULL, -1) < 0);
  TEST_1(soa_set_remote_sdp(NULL, NULL, NULL, -1) < 0);
  TEST_1(soa_set_user_sdp(NULL, NULL, NULL, -1) < 0);

  TEST_1(soa_get_capability_sdp(NULL, NULL, NULL, NULL) < 0);
  TEST_1(soa_get_remote_sdp(NULL, NULL, NULL, NULL) < 0);
  TEST_1(soa_get_user_sdp(NULL, NULL, NULL, NULL) < 0);
  TEST_1(soa_get_local_sdp(NULL, NULL, NULL, NULL) < 0);

  TEST_1(-1 == soa_generate_offer(NULL, 0, test_api_completed));

  TEST_1(-1 == soa_generate_answer(NULL, test_api_completed));

  TEST_1(-1 == soa_process_answer(NULL, test_api_completed));

  TEST_1(-1 == soa_process_reject(NULL, test_api_completed));

  TEST(soa_activate(NULL, "both"), -1);
  TEST(soa_deactivate(NULL, "both"), -1);
  TEST_VOID(soa_terminate(NULL, "both"));

  TEST_1(!soa_is_complete(NULL));

  TEST_1(!soa_init_offer_answer(NULL));

  TEST(soa_is_audio_active(NULL), SOA_ACTIVE_DISABLED);
  TEST(soa_is_video_active(NULL), SOA_ACTIVE_DISABLED);
  TEST(soa_is_image_active(NULL), SOA_ACTIVE_DISABLED);
  TEST(soa_is_chat_active(NULL), SOA_ACTIVE_DISABLED);

  TEST(soa_is_remote_audio_active(NULL), SOA_ACTIVE_DISABLED);
  TEST(soa_is_remote_video_active(NULL), SOA_ACTIVE_DISABLED);
  TEST(soa_is_remote_image_active(NULL), SOA_ACTIVE_DISABLED);
  TEST(soa_is_remote_chat_active(NULL), SOA_ACTIVE_DISABLED);

  END();
}

int test_soa_tags(struct context *ctx)
{
  BEGIN();

  su_home_t home[1] = { SU_HOME_INIT(home) };
  tagi_t *t;

  tagi_t const soafilter[] = {
    { TAG_FILTER(soa_tag_filter) },
    { TAG_NULL() }
  };

  t = tl_filtered_tlist(home, soafilter,
			SIPTAG_FROM_STR("sip:my.domain"),
			SOATAG_USER_SDP_STR("v=0"),
			SOATAG_HOLD("*"),
			TAG_END());
  TEST_1(t);
  TEST_P(t[0].t_tag, soatag_user_sdp_str);
  TEST_P(t[1].t_tag, soatag_hold);
  TEST_1(t[2].t_tag == NULL || t[2].t_tag == tag_null);

  su_home_deinit(home);

  END();
}

int test_init(struct context *ctx, char *argv[])
{
  BEGIN();

  int n;

  ctx->root = su_root_create(ctx); TEST_1(ctx->root);

  ctx->asynch.a = soa_create("asynch", ctx->root, ctx);
  TEST_1(!ctx->asynch.a);

#if 0
  TEST_1(!soa_find("asynch"));
  TEST_1(soa_find("default"));

  n = soa_add("asynch", &soa_asynch_actions); TEST(n, 0);

  TEST_1(soa_find("asynch"));

  ctx->asynch.a = soa_create("asynch", ctx->root, ctx);
  TEST_1(ctx->asynch.a);

  ctx->asynch.b = soa_create("asynch", ctx->root, ctx);
  TEST_1(ctx->asynch.b);
#endif

  /* Create asynchronous endpoints */

  ctx->a = soa_create("static", ctx->root, ctx);
  TEST_1(!ctx->a);

  TEST_1(!soa_find("static"));
  TEST_1(soa_find("default"));

  n = soa_add("static", &soa_default_actions); TEST(n, 0);

  TEST_1(soa_find("static"));

  ctx->a = soa_create("static", ctx->root, ctx);
  TEST_1(ctx->a);

  ctx->b = soa_create("static", ctx->root, ctx);
  TEST_1(ctx->b);

  END();
}

int test_params(struct context *ctx)
{
  BEGIN();
  int n;
  int af;
  char const *address;
  char const *hold;
  int rtp_select, rtp_sort;
  int rtp_mismatch;
  int srtp_enable, srtp_confidentiality, srtp_integrity;
  soa_session_t *a = ctx->a, *b = ctx->b;

  n = soa_set_params(a, TAG_END()); TEST(n, 0);
  n = soa_set_params(b, TAG_END()); TEST(n, 0);

  af = -42;
  address = NONE;
  hold = NONE;

  rtp_select = -1, rtp_sort = -1, rtp_mismatch = -1;
  srtp_enable = -1, srtp_confidentiality = -1, srtp_integrity = -1;

  TEST(soa_get_params(a,
		      SOATAG_AF_REF(af),
		      SOATAG_ADDRESS_REF(address),
		      SOATAG_HOLD_REF(hold),

		      SOATAG_RTP_SELECT_REF(rtp_select),
		      SOATAG_RTP_SORT_REF(rtp_sort),
		      SOATAG_RTP_MISMATCH_REF(rtp_mismatch),

		      SOATAG_SRTP_ENABLE_REF(srtp_enable),
		      SOATAG_SRTP_CONFIDENTIALITY_REF(srtp_confidentiality),
		      SOATAG_SRTP_INTEGRITY_REF(srtp_integrity),
		      TAG_END()),
       9);
  TEST(af, SOA_AF_ANY);
  TEST_P(address, 0);
  TEST_P(hold, 0);
  TEST(rtp_select, SOA_RTP_SELECT_SINGLE);
  TEST(rtp_sort, SOA_RTP_SORT_DEFAULT);
  TEST(rtp_mismatch, 0);
  TEST(srtp_enable, 0);
  TEST(srtp_confidentiality, 0);
  TEST(srtp_integrity, 0);

  TEST(soa_set_params(a,
		      SOATAG_AF(SOA_AF_IP4_IP6),
		      SOATAG_ADDRESS("127.0.0.1"),
		      SOATAG_HOLD("audio"),

		      SOATAG_RTP_SELECT(SOA_RTP_SELECT_ALL),
		      SOATAG_RTP_SORT(SOA_RTP_SORT_LOCAL),
		      SOATAG_RTP_MISMATCH(1),

		      SOATAG_SRTP_ENABLE(1),
		      SOATAG_SRTP_CONFIDENTIALITY(1),
		      SOATAG_SRTP_INTEGRITY(1),

		      TAG_END()),
       9);
  TEST(soa_get_params(a,
		      SOATAG_AF_REF(af),
		      SOATAG_ADDRESS_REF(address),
		      SOATAG_HOLD_REF(hold),

		      SOATAG_RTP_SELECT_REF(rtp_select),
		      SOATAG_RTP_SORT_REF(rtp_sort),
		      SOATAG_RTP_MISMATCH_REF(rtp_mismatch),

		      SOATAG_SRTP_ENABLE_REF(srtp_enable),
		      SOATAG_SRTP_CONFIDENTIALITY_REF(srtp_confidentiality),
		      SOATAG_SRTP_INTEGRITY_REF(srtp_integrity),
		      TAG_END()),
       9);
  TEST(af, SOA_AF_IP4_IP6);
  TEST_S(address, "127.0.0.1");
  TEST_S(hold, "audio");
  TEST(rtp_select, SOA_RTP_SELECT_ALL);
  TEST(rtp_sort, SOA_RTP_SORT_LOCAL);
  TEST(rtp_mismatch, 1);
  TEST(srtp_enable, 1);
  TEST(srtp_confidentiality, 1);
  TEST(srtp_integrity, 1);

  /* Restore defaults */
  TEST(soa_set_params(a,
		      SOATAG_AF(SOA_AF_IP4_IP6),
		      SOATAG_ADDRESS(NULL),
		      SOATAG_HOLD(NULL),

		      SOATAG_RTP_SELECT(SOA_RTP_SELECT_SINGLE),
		      SOATAG_RTP_SORT(SOA_RTP_SORT_DEFAULT),
		      SOATAG_RTP_MISMATCH(0),

		      SOATAG_SRTP_ENABLE(0),
		      SOATAG_SRTP_CONFIDENTIALITY(0),
		      SOATAG_SRTP_INTEGRITY(0),

		      TAG_END()),
       9);

  END();
}

int test_completed(struct context *ctx, soa_session_t *session)
{
  ctx->completed = session;
  su_root_break(ctx->root);
  return 0;
}

int test_static_offer_answer(struct context *ctx)
{
  BEGIN();
  int n;

  soa_session_t *a, *b;

  char const *caps = NONE, *offer = NONE, *answer = NONE;
  isize_t capslen = (isize_t)-1;
  isize_t offerlen = (isize_t)-1;
  isize_t answerlen = (isize_t)-1;

  su_home_t home[1] = { SU_HOME_INIT(home) };

  char const a_caps[] =
    "v=0\r\n"
    "o=left 219498671 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=audio 0 RTP/AVP 0 8\r\n";

  char const b_caps[] =
    "m=audio 5004 RTP/AVP 96 8\n"
    "m=rtpmap:96 GSM/8000\n";

  TEST(soa_set_capability_sdp(ctx->a, 0, "m=audio 0 RTP/AVP 0 8", -1),
       1);
  TEST(soa_set_capability_sdp(ctx->a, 0, a_caps, strlen(a_caps)),
       1);
  TEST(soa_get_capability_sdp(ctx->a, NULL, &caps, &capslen), 1);

  TEST_1(caps != NULL && caps != NONE);
  TEST_1(capslen > 0);

  TEST(soa_set_user_sdp(ctx->b, 0, b_caps, strlen(b_caps)), 1);
  TEST(soa_get_capability_sdp(ctx->a, NULL, &caps, &capslen), 1);

  TEST_1(a = soa_clone(ctx->a, ctx->root, ctx));
  TEST_1(b = soa_clone(ctx->b, ctx->root, ctx));

  n = soa_get_local_sdp(a, NULL, &offer, &offerlen); TEST(n, 0);

  n = soa_set_user_sdp(a, 0, "m=audio 5004 RTP/AVP 0 8", -1); TEST(n, 1);

  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);

  n = soa_get_local_sdp(a, NULL, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);

  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);

  n = soa_get_local_sdp(b, NULL, &answer, &answerlen); TEST(n, 0);

  n = soa_set_params(b,
		     SOATAG_LOCAL_SDP_STR("m=audio 5004 RTP/AVP 8"),
		     SOATAG_AF(SOA_AF_IP4_ONLY),
		     SOATAG_ADDRESS("1.2.3.4"),
		     TAG_END());

  n = soa_generate_answer(b, test_completed); TEST(n, 0);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  n = soa_get_local_sdp(b, NULL, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  TEST_1(strstr(answer, "c=IN IP4 1.2.3.4"));

  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 1);

  n = soa_process_answer(a, test_completed); TEST(n, 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_video_active(a), SOA_ACTIVE_DISABLED);
  TEST(soa_is_image_active(a), SOA_ACTIVE_DISABLED);
  TEST(soa_is_chat_active(a), SOA_ACTIVE_DISABLED);

  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_video_active(a), SOA_ACTIVE_DISABLED);
  TEST(soa_is_remote_image_active(a), SOA_ACTIVE_DISABLED);
  TEST(soa_is_remote_chat_active(a), SOA_ACTIVE_DISABLED);

  /* 'A' will put call on hold */
  offer = NONE;
  TEST(soa_set_params(a, SOATAG_HOLD("*"), TAG_END()), 1);

  TEST(soa_generate_offer(a, 1, test_completed), 0);
  TEST(soa_get_local_sdp(a, NULL, &offer, &offerlen), 1);
  TEST_1(offer != NULL && offer != NONE);
  TEST_1(strstr(offer, "a=sendonly"));
  TEST(soa_set_remote_sdp(b, 0, offer, offerlen), 1);
  TEST(soa_generate_answer(b, test_completed), 0);
  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);
  TEST(soa_get_local_sdp(b, NULL, &answer, &answerlen), 1);
  TEST_1(answer != NULL && answer != NONE);
  TEST_1(strstr(answer, "a=recvonly"));
  TEST(soa_set_remote_sdp(a, 0, answer, -1), 1);
  TEST(soa_process_answer(a, test_completed), 0);
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDONLY);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDONLY);

  /* 'A' will put call inactive */
  offer = NONE;
  TEST(soa_set_params(a, SOATAG_HOLD("#"), TAG_END()), 1);

  TEST(soa_generate_offer(a, 1, test_completed), 0);
  TEST(soa_get_local_sdp(a, NULL, &offer, &offerlen), 1);
  TEST_1(offer != NULL && offer != NONE);
  TEST_1(strstr(offer, "a=inactive"));
  TEST(soa_set_remote_sdp(b, 0, offer, offerlen), 1);
  TEST(soa_generate_answer(b, test_completed), 0);
  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);
  TEST(soa_get_local_sdp(b, NULL, &answer, &answerlen), 1);
  TEST_1(answer != NULL && answer != NONE);
  TEST_1(strstr(answer, "a=inactive"));
  TEST(soa_set_remote_sdp(a, 0, answer, -1), 1);
  TEST(soa_process_answer(a, test_completed), 0);
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_INACTIVE);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_INACTIVE);

  /* B will send an offer to A, but there is no change in O/A status */
  TEST(soa_generate_offer(b, 1, test_completed), 0);
  TEST(soa_get_local_sdp(b, NULL, &offer, &offerlen), 1);
  TEST_1(offer != NULL && offer != NONE);
  TEST_1(!strstr(offer, "a=inactive"));
  /* printf("offer:\n%s", offer); */
  TEST(soa_set_remote_sdp(a, 0, offer, offerlen), 1);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_generate_answer(a, test_completed), 0);
  TEST(soa_is_audio_active(a), SOA_ACTIVE_INACTIVE);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_INACTIVE);
  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);
  TEST(soa_get_local_sdp(a, NULL, &answer, &answerlen), 1);
  TEST_1(answer != NULL && answer != NONE);
  TEST_1(strstr(answer, "a=inactive"));
  /* printf("answer:\n%s", answer); */
  TEST(soa_set_remote_sdp(b, 0, answer, -1), 1);
  TEST(soa_process_answer(b, test_completed), 0);
  TEST(soa_activate(b, NULL), 0);


  TEST(soa_is_audio_active(b), SOA_ACTIVE_INACTIVE);
  TEST(soa_is_remote_audio_active(b), SOA_ACTIVE_INACTIVE);

  /* 'A' will release hold. */
  TEST(soa_set_params(a, SOATAG_HOLD(NULL), TAG_END()), 1);

  TEST(soa_generate_offer(a, 1, test_completed), 0);
  TEST(soa_get_local_sdp(a, NULL, &offer, &offerlen), 1);
  TEST_1(offer != NULL && offer != NONE);
  TEST_1(!strstr(offer, "a=sendonly") && !strstr(offer, "a=inactive"));
  TEST(soa_set_remote_sdp(b, 0, offer, offerlen), 1);
  TEST(soa_generate_answer(b, test_completed), 0);
  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);
  TEST(soa_get_local_sdp(b, NULL, &answer, &answerlen), 1);
  TEST_1(answer != NULL && answer != NONE);
  TEST_1(!strstr(answer, "a=recvonly") && !strstr(answer, "a=inactive"));
  TEST(soa_set_remote_sdp(a, 0, answer, -1), 1);
  TEST(soa_process_answer(a, test_completed), 0);
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);

  /* 'A' will put B on hold but this time with c=IN IP4 0.0.0.0 */
  TEST(soa_set_params(a, SOATAG_HOLD("*"), TAG_END()), 1);
  TEST(soa_generate_offer(a, 1, test_completed), 0);

  {
    sdp_session_t const *o_sdp;
    sdp_session_t *sdp;
    sdp_printer_t *p;
    sdp_connection_t *c;

    TEST(soa_get_local_sdp(a, &o_sdp, NULL, NULL), 1);
    TEST_1(o_sdp != NULL && o_sdp != NONE);
    TEST_1(sdp = sdp_session_dup(home, o_sdp));

    /* Remove mode, change c=, encode offer */
    if (sdp->sdp_media->m_connections)
      c = sdp->sdp_media->m_connections;
    else
      c = sdp->sdp_connection;
    TEST_1(c);
    c->c_address = "0.0.0.0";

    TEST_1(p = sdp_print(home, sdp, NULL, 0, sdp_f_realloc));
    TEST_1(sdp_message(p));
    offer = sdp_message(p); offerlen = strlen(offer);
  }

  TEST(soa_set_remote_sdp(b, 0, offer, -1), 1);
  TEST(soa_generate_answer(b, test_completed), 0);
  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);
  TEST(soa_get_local_sdp(b, NULL, &answer, &answerlen), 1);
  TEST_1(answer != NULL && answer != NONE);
  TEST_1(strstr(answer, "a=recvonly"));
  TEST(soa_set_remote_sdp(a, 0, answer, -1), 1);
  TEST(soa_process_answer(a, test_completed), 0);
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDONLY);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDONLY);
  TEST(soa_is_audio_active(b), SOA_ACTIVE_RECVONLY);
  TEST(soa_is_remote_audio_active(b), SOA_ACTIVE_RECVONLY);

  /* 'A' will propose adding video. */
  /* 'B' will reject. */
  TEST(soa_set_params(a,
		      SOATAG_HOLD(NULL),  /* 'A' will release hold. */
		      SOATAG_USER_SDP_STR("m=audio 5008 RTP/AVP 0 8\r\ni=x\r\n"
					  "m=video 5006 RTP/AVP 34\r\n"),
		      TAG_END()), 2);

  TEST(soa_generate_offer(a, 1, test_completed), 0);
  TEST(soa_get_local_sdp(a, NULL, &offer, &offerlen), 1);
  TEST_1(offer != NULL && offer != NONE);
  TEST_1(!strstr(offer, "a=sendonly"));
  TEST_1(strstr(offer, "m=video"));
  TEST(soa_set_remote_sdp(b, 0, offer, offerlen), 1);
  TEST(soa_generate_answer(b, test_completed), 0);
  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);
  TEST(soa_get_local_sdp(b, NULL, &answer, &answerlen), 1);
  TEST_1(answer != NULL && answer != NONE);
  TEST_1(!strstr(answer, "a=recvonly"));
  TEST_1(strstr(answer, "m=video"));
  TEST(soa_set_remote_sdp(a, 0, answer, -1), 1);
  TEST(soa_process_answer(a, test_completed), 0);
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_video_active(a), SOA_ACTIVE_REJECTED);

  {
    /* Test tags */
    sdp_session_t const *l = NULL, *u = NULL, *r = NULL;
    sdp_media_t const *m;

    TEST(soa_get_params(b,
			SOATAG_LOCAL_SDP_REF(l),
			SOATAG_USER_SDP_REF(u),
			SOATAG_REMOTE_SDP_REF(r),
			TAG_END()), 3);

    TEST_1(l); TEST_1(u); TEST_1(r);
    TEST_1(m = l->sdp_media); TEST(m->m_type, sdp_media_audio);
    TEST_1(!m->m_rejected);
    TEST_1(m = m->m_next); TEST(m->m_type, sdp_media_video);
    TEST_1(m->m_rejected);
  }

  /* 'B' will now propose adding video. */
  /* 'A' will accept. */
  TEST(soa_set_params(b,
		      SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8\r\n"
					  "m=video 5006 RTP/AVP 34\r\n"),
		      TAG_END()), 1);

  TEST(soa_generate_offer(b, 1, test_completed), 0);
  TEST(soa_get_local_sdp(b, NULL, &offer, &offerlen), 1);
  TEST_1(offer != NULL && offer != NONE);
  TEST_1(!strstr(offer, "b=sendonly"));
  TEST_1(strstr(offer, "m=video"));
  TEST(soa_set_remote_sdp(a, 0, offer, offerlen), 1);
  TEST(soa_generate_answer(a, test_completed), 0);
  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);
  TEST(soa_get_local_sdp(a, NULL, &answer, &answerlen), 1);
  TEST_1(answer != NULL && answer != NONE);
  TEST_1(!strstr(answer, "b=recvonly"));
  TEST_1(strstr(answer, "m=video"));
  TEST(soa_set_remote_sdp(b, 0, answer, -1), 1);
  TEST(soa_process_answer(b, test_completed), 0);
  TEST(soa_activate(b, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_video_active(a), SOA_ACTIVE_SENDRECV);

  TEST_VOID(soa_terminate(a, NULL));

  TEST(soa_is_audio_active(a), SOA_ACTIVE_DISABLED);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_DISABLED);

  TEST_VOID(soa_terminate(b, NULL));

  TEST_VOID(soa_destroy(a));
  TEST_VOID(soa_destroy(b));

  su_home_deinit(home);

  END();
}

int test_codec_selection(struct context *ctx)
{
  BEGIN();
  int n;

  soa_session_t *a, *b;

  char const *offer = NONE, *answer = NONE;
  isize_t offerlen = (isize_t)-1, answerlen = (isize_t)-1;

  sdp_session_t const *a_sdp, *b_sdp;
  sdp_media_t const *m;
  sdp_rtpmap_t const *rm;

  char const a_caps[] =
    "v=0\r\n"
    "o=left 219498671 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=audio 5008 RTP/AVP 0 8 97\r\n"
    "a=rtpmap:97 GSM/8000\n"
    ;

  char const b_caps[] =
    "m=audio 5004 RTP/AVP 96 97\n"
    "a=rtpmap:96 G7231/8000\n"
    "a=rtpmap:97 G729/8000\n";

  TEST_1(a = soa_create("static", ctx->root, ctx));
  TEST_1(b = soa_create("static", ctx->root, ctx));

  TEST(soa_set_user_sdp(a, 0, a_caps, strlen(a_caps)), 1);
  TEST(soa_set_user_sdp(b, 0, b_caps, strlen(b_caps)), 1);

  TEST(soa_set_params(a, SOATAG_AUDIO_AUX("cn telephone-event"), TAG_END()), 1);

  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, NULL, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);
  n = soa_get_local_sdp(b, NULL, &answer, &answerlen); TEST(n, 0);
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 1);
  n = soa_process_answer(a, test_completed); TEST(n, 0);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_REJECTED);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_REJECTED);

  TEST_1(m = b_sdp->sdp_media); TEST_1(m->m_rejected);
  TEST_1(rm = m->m_rtpmaps); TEST(rm->rm_pt, 96);
  TEST_S(rm->rm_encoding, "G7231");
  /* Not reusing payload type 97 from offer */
  TEST_1(rm = rm->rm_next); TEST(rm->rm_pt, 98);
  TEST_S(rm->rm_encoding, "G729");
  TEST_1(!rm->rm_next);

  /* ---------------------------------------------------------------------- */

  /* Re-O/A: A generates new SDP */
  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 0);
  n = soa_process_answer(a, test_completed); TEST(n, 0);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  /* Re-O/A: no-one regenerates new SDP */
  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 0);
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 0);
  n = soa_process_answer(a, test_completed); TEST(n, 0);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  /* ---------------------------------------------------------------------- */

  /* Re-O/A: accept media without common codecs */

  /* Accept media without common codecs */
  TEST_1(soa_set_params(a, SOATAG_RTP_MISMATCH(1), TAG_END()));
  TEST_1(soa_set_params(b, SOATAG_RTP_MISMATCH(1), TAG_END()));

  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 1);
  n = soa_process_answer(a, test_completed); TEST(n, 0);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);

  TEST_1(m = b_sdp->sdp_media); TEST_1(!m->m_rejected);
  TEST_1(rm = m->m_rtpmaps); TEST(rm->rm_pt, 96);
  TEST_S(rm->rm_encoding, "G7231");
  /* Not using payload type 97 from offer */
  TEST_1(rm = rm->rm_next); TEST(rm->rm_pt, 98);
  TEST_S(rm->rm_encoding, "G729");
  TEST_1(!rm->rm_next);

  /* ---------------------------------------------------------------------- */

  /* Re-O/A: add a common codec */

  /* Accept media without common codecs */
  TEST_1(soa_set_params(a, SOATAG_RTP_MISMATCH(0),
			SOATAG_USER_SDP_STR(
    "v=0\r\n"
    "o=left 219498671 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=audio 5008 RTP/AVP 0 8 96 3 127\r\n"
    "a=rtpmap:96 G729/8000\n"
    "a=rtpmap:127 CN/8000\n"
    ),
			SOATAG_RTP_SORT(SOA_RTP_SORT_REMOTE),
			SOATAG_RTP_SELECT(SOA_RTP_SELECT_ALL),
			TAG_END()));
  TEST_1(soa_set_params(b, SOATAG_RTP_MISMATCH(0),
			SOATAG_USER_SDP_STR(
    "v=0\r\n"
    "o=left 219498671 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=audio 5004 RTP/AVP 96 3 97 111\r\n"
    "a=rtpmap:96 G7231/8000\n"
    "a=rtpmap:97 G729/8000\n"
    "a=rtpmap:111 telephone-event/8000\n"
    "a=fmtp:111 0-15\n"
    ),
			SOATAG_AUDIO_AUX("cn telephone-event"),
			SOATAG_RTP_SORT(SOA_RTP_SORT_LOCAL),
			SOATAG_RTP_SELECT(SOA_RTP_SELECT_COMMON),
			TAG_END()));

  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 1);
  n = soa_process_answer(a, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, NULL, NULL); TEST(n, 1);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);

  TEST_1(m = a_sdp->sdp_media); TEST_1(!m->m_rejected);
  TEST_1(rm = m->m_rtpmaps); TEST(rm->rm_pt, 3);
  TEST_S(rm->rm_encoding, "GSM");
  TEST_1(rm = rm->rm_next); TEST(rm->rm_pt, 96);
  TEST_S(rm->rm_encoding, "G729");
  TEST_1(rm = rm->rm_next); TEST(rm->rm_pt, 0);
  TEST_S(rm->rm_encoding, "PCMU");
  TEST_1(rm = rm->rm_next); TEST(rm->rm_pt, 8);
  TEST_S(rm->rm_encoding, "PCMA");
  TEST_1(rm = rm->rm_next); TEST(rm->rm_pt, 127);
  TEST_S(rm->rm_encoding, "CN");
  TEST_1(!rm->rm_next);

  TEST_1(m = b_sdp->sdp_media); TEST_1(!m->m_rejected);
  TEST_1(rm = m->m_rtpmaps); TEST(rm->rm_pt, 3);
  TEST_S(rm->rm_encoding, "GSM");
  /* Using payload type 96 from offer */
  TEST_1(rm = rm->rm_next); TEST(rm->rm_pt, 96);
  TEST_S(rm->rm_encoding, "G729");
  TEST_1(rm = rm->rm_next); TEST(rm->rm_pt, 111);
  TEST_S(rm->rm_encoding, "telephone-event");
  TEST_1(!rm->rm_next);

  /* ---------------------------------------------------------------------- */
  /* Re-O/A: prune down to single codec. */

  TEST_1(soa_set_params(a,
			SOATAG_USER_SDP_STR(
    "v=0\r\n"
    "o=left 219498671 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=audio 5008 RTP/AVP 0 8 97 96 127\r\n"
    "a=rtpmap:96 G729/8000\n"
    "a=rtpmap:97 GSM/8000\n"
    "a=rtpmap:127 CN/8000\n"
    ),
			SOATAG_RTP_MISMATCH(0),
			SOATAG_RTP_SELECT(SOA_RTP_SELECT_COMMON),
			TAG_END()));
  TEST_1(soa_set_params(b,
			SOATAG_RTP_MISMATCH(0),
			SOATAG_RTP_SORT(SOA_RTP_SORT_LOCAL),
			SOATAG_RTP_SELECT(SOA_RTP_SELECT_SINGLE),
			TAG_END()));

  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 1);
  n = soa_process_answer(a, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, &offer, &offerlen); TEST(n, 1);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);

  TEST_1(m = a_sdp->sdp_media); TEST_1(!m->m_rejected);
  TEST_1(rm = m->m_rtpmaps); TEST(rm->rm_pt, 97);
  TEST_S(rm->rm_encoding, "GSM");
  TEST_1(rm = rm->rm_next); TEST(rm->rm_pt, 127);
  TEST_S(rm->rm_encoding, "CN");
  TEST_1(!rm->rm_next);

  /* Answering end matches payload types
     then sorts by local preference,
     then select best codec => GSM with pt 97 */
  TEST_1(m = b_sdp->sdp_media); TEST_1(!m->m_rejected);
  TEST_1(rm = m->m_rtpmaps); TEST(rm->rm_pt, 97);
  TEST_S(rm->rm_encoding, "GSM");
  TEST_1(rm = rm->rm_next); TEST(rm->rm_pt, 111);
  TEST_S(rm->rm_encoding, "telephone-event");
  TEST_1(!rm->rm_next);

  /* ---------------------------------------------------------------------- */
  /* Re-O/A: A generates new SDP offer with single codec only */
  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  TEST_1(m = a_sdp->sdp_media); TEST_1(!m->m_rejected);
  TEST_1(rm = m->m_rtpmaps); TEST(rm->rm_pt, 97);
  TEST_S(rm->rm_encoding, "GSM");
  TEST_1(rm = rm->rm_next); TEST(rm->rm_pt, 127);
  TEST_S(rm->rm_encoding, "CN");
  TEST_1(!rm->rm_next);
  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);
  /* Answer from B is identical to previous one */
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 0);
  n = soa_process_answer(a, test_completed); TEST(n, 0);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  /* ---------------------------------------------------------------------- */

  /* Add new media - without common codecs */
  TEST_1(soa_set_params(a,
			SOATAG_RTP_MISMATCH(0),
			SOATAG_USER_SDP_STR(
    "v=0\r\n"
    "o=left 219498671 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=audio 5008 RTP/AVP 0 8 96 3 127\r\n"
    "a=rtpmap:96 G729/8000\n"
    "a=rtpmap:127 CN/8000\n"
    "m=video 5010 RTP/AVP 31\r\n"
    "m=audio 6008 RTP/SAVP 3\n"
    ),
			TAG_END()));
  TEST_1(soa_set_params(b,
			SOATAG_RTP_MISMATCH(0),
			SOATAG_USER_SDP_STR(
    "v=0\r\n"
    "o=left 219498671 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=audio 5004 RTP/AVP 96 3 97 111\r\n"
    "a=rtpmap:96 G7231/8000\n"
    "a=rtpmap:97 G729/8000\n"
    "a=rtpmap:111 telephone-event/8000\n"
    "a=fmtp:111 0-15\n"
    "m=audio 6004 RTP/SAVP 96\n"
    "a=rtpmap:96 G729/8000\n"
    "m=video 5006 RTP/AVP 34\n"
    ),
			TAG_END()));

  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  TEST_1(m = a_sdp->sdp_media); TEST_1(!m->m_rejected);
  TEST_1(m = m->m_next); TEST_1(!m->m_rejected);
  TEST_1(m = m->m_next); TEST_1(!m->m_rejected);
  TEST_1(!m->m_next);
  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  /* Answer from B rejects video */
  n = soa_get_local_sdp(b, &b_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 1);
  n = soa_process_answer(a, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, &offer, &offerlen); TEST(n, 1);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_video_active(a), SOA_ACTIVE_REJECTED);
  TEST(soa_is_remote_video_active(a), SOA_ACTIVE_REJECTED);

  TEST_1(m = a_sdp->sdp_media); TEST_1(!m->m_rejected);
  TEST_1(m = m->m_next); TEST_1(m->m_rejected);
  TEST_1(m = m->m_next); TEST_1(m->m_rejected);
  TEST_1(!m->m_next);

  TEST_1(m = b_sdp->sdp_media); TEST_1(!m->m_rejected);
  TEST_1(m = m->m_next); TEST_1(m->m_rejected);
  /* Rejected but tell what we support */
  TEST_1(rm = m->m_rtpmaps); TEST(rm->rm_pt, 34);
  TEST_S(rm->rm_encoding, "H263");
  TEST_1(m = m->m_next); TEST_1(m->m_rejected);
  TEST_1(!m->m_next);

  /* ---------------------------------------------------------------------- */
  /* A adds H.263 to video */
  TEST_1(soa_set_params(a,
			SOATAG_USER_SDP_STR(
    "v=0\r\n"
    "o=left 219498671 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=audio 5008 RTP/AVP 0 8 96 3 127\r\n"
    "a=rtpmap:96 G729/8000\n"
    "a=rtpmap:127 CN/8000\n"
    "m=video 5010 RTP/AVP 31 34\r\n"
    "m=audio 6008 RTP/SAVP 3\n"
    ),
			TAG_END()));

  /* B adds GSM to SRTP */
  TEST_1(soa_set_params(b,
			SOATAG_USER_SDP_STR(
    "v=0\r\n"
    "o=left 219498671 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=audio 5004 RTP/AVP 96 3 97 111\r\n"
    "a=rtpmap:96 G7231/8000\n"
    "a=rtpmap:97 G729/8000\n"
    "a=rtpmap:111 telephone-event/8000\n"
    "a=fmtp:111 0-15\n"
    "m=audio 6004 RTP/SAVP 96 3\n"
    "a=rtpmap:96 G729/8000\n"
    "m=video 5006 RTP/AVP 34\n"
    ),
			TAG_END()));

  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  TEST_1(m = a_sdp->sdp_media); TEST_1(!m->m_rejected);
  TEST_1(m = m->m_next); TEST_1(!m->m_rejected);
  TEST_1(m = m->m_next); TEST_1(!m->m_rejected);
  TEST_1(!m->m_next);
  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  /* Answer from B now accepts video */
  n = soa_get_local_sdp(b, &b_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 1);
  n = soa_process_answer(a, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, &offer, &offerlen); TEST(n, 1);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_video_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_video_active(a), SOA_ACTIVE_SENDRECV);

  TEST_1(m = a_sdp->sdp_media); TEST_1(!m->m_rejected);
  TEST_1(m = m->m_next); TEST_1(!m->m_rejected);
  TEST_1(rm = m->m_rtpmaps); TEST(rm->rm_pt, 34);
  TEST_S(rm->rm_encoding, "H263");
  TEST_1(m = m->m_next); TEST_1(!m->m_rejected);
  TEST_1(!m->m_next);

  TEST_1(m = b_sdp->sdp_media); TEST_1(!m->m_rejected);
  TEST_1(m = m->m_next); TEST_1(!m->m_rejected);
  TEST_1(rm = m->m_rtpmaps); TEST(rm->rm_pt, 34);
  TEST_S(rm->rm_encoding, "H263");
  TEST_1(m = m->m_next); TEST_1(!m->m_rejected);
  TEST_1(!m->m_next);

  /* ---------------------------------------------------------------------- */
  /* A drops GSM support */

  TEST_1(soa_set_params(a,
			SOATAG_USER_SDP_STR(
    "v=0\r\n"
    "o=left 219498671 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=audio 5008 RTP/AVP 0 8 96 127\r\n"
    "a=rtpmap:96 G729/8000\n"
    "a=rtpmap:127 CN/8000\n"
    "m=video 5010 RTP/AVP 31 34\r\n"
    "m=audio 6008 RTP/SAVP 3\n"
    ),
			TAG_END()));

  /* B adds GSM to SRTP, changes IP address */
  TEST_1(soa_set_params(b,
			SOATAG_USER_SDP_STR(
    "v=0\r\n"
    "o=left 219498671 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.3\r\n"
    "m=audio 5004 RTP/AVP 96 3 97 111\r\n"
    "a=rtpmap:96 G7231/8000\n"
    "a=rtpmap:97 G729/8000\n"
    "a=rtpmap:111 telephone-event/8000\n"
    "a=fmtp:111 0-15\n"
    "m=audio 6004 RTP/SAVP 96 3\n"
    "a=rtpmap:96 G729/8000\n"
    "m=video 5006 RTP/AVP 34\n"
    ),
			TAG_END()));

  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  TEST_1(m = a_sdp->sdp_media); TEST_1(!m->m_rejected);
  TEST_1(m = m->m_next); TEST_1(!m->m_rejected);
  TEST_1(m = m->m_next); TEST_1(!m->m_rejected);
  TEST_1(!m->m_next);
  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  /* Answer from B now accepts video */
  n = soa_get_local_sdp(b, &b_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  /* Check that updated c= line is propagated */
  TEST_1(b_sdp->sdp_connection);
  TEST_S(b_sdp->sdp_connection->c_address, "127.0.0.3");
  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 1);
  n = soa_process_answer(a, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, &offer, &offerlen); TEST(n, 1);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_video_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_video_active(a), SOA_ACTIVE_SENDRECV);

  TEST_1(m = a_sdp->sdp_media); TEST_1(!m->m_rejected);
  TEST_1(rm = m->m_rtpmaps); TEST(rm->rm_pt, 96);
  TEST_S(rm->rm_encoding, "G729");
  TEST_1(rm = rm->rm_next); TEST(rm->rm_pt, 127);
  TEST_S(rm->rm_encoding, "CN");
  TEST_1(m = m->m_next); TEST_1(!m->m_rejected);
  TEST_1(rm = m->m_rtpmaps); TEST(rm->rm_pt, 34);
  TEST_S(rm->rm_encoding, "H263");
  TEST_1(m = m->m_next); TEST_1(!m->m_rejected);
  TEST_1(!m->m_next);

  TEST_1(m = b_sdp->sdp_media); TEST_1(!m->m_rejected);
  TEST_1(rm = m->m_rtpmaps); TEST(rm->rm_pt, 96);
  TEST_S(rm->rm_encoding, "G729");
  TEST_1(rm = rm->rm_next); TEST(rm->rm_pt, 111);
  TEST_S(rm->rm_encoding, "telephone-event");
  TEST_1(m = m->m_next); TEST_1(!m->m_rejected);
  TEST_1(rm = m->m_rtpmaps); TEST(rm->rm_pt, 34);
  TEST_S(rm->rm_encoding, "H263");
  TEST_1(m = m->m_next); TEST_1(!m->m_rejected);
  TEST_1(!m->m_next);

  /* ---------------------------------------------------------------------- */

  TEST_VOID(soa_terminate(a, NULL));
  TEST_VOID(soa_terminate(b, NULL));

  TEST_VOID(soa_destroy(a));
  TEST_VOID(soa_destroy(b));

  END();
}

int test_media_mode(struct context *ctx)
{
  BEGIN();
  int n;

  soa_session_t *a, *b;

  char const *offer = NONE, *answer = NONE;
  isize_t offerlen = (isize_t)-1;
  isize_t answerlen = (isize_t)-1;

  su_home_t home[1] = { SU_HOME_INIT(home) };

  char const a_caps[] = "m=audio 5008 RTP/AVP 0 8\r\n";
  char const b_caps[] = "m=audio 5004 RTP/AVP 8 0\n";

  TEST_1(a = soa_clone(ctx->a, ctx->root, ctx));
  TEST_1(b = soa_clone(ctx->b, ctx->root, ctx));

  TEST(soa_set_user_sdp(a, 0, a_caps, -1), 1);
  TEST(soa_set_user_sdp(b, 0, b_caps, -1), 1);

  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, NULL, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);

  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  n = soa_get_local_sdp(b, NULL, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);

  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 1);
  n = soa_process_answer(a, test_completed); TEST(n, 0);
  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);

  /* 'A' will put call on hold */
  offer = NONE;
  TEST(soa_set_params(a, SOATAG_HOLD("*"), TAG_END()), 1);
  TEST(soa_generate_offer(a, 1, test_completed), 0);
  TEST(soa_get_local_sdp(a, NULL, &offer, &offerlen), 1);
  TEST_1(offer != NULL && offer != NONE);
  TEST_1(strstr(offer, "a=sendonly"));

  TEST(soa_set_user_sdp(b, 0,
			"m=audio 5004 RTP/AVP 8 0\n"
			"a=inactive\n", -1), 1);
  TEST(soa_set_remote_sdp(b, 0, offer, offerlen), 1);
  TEST(soa_generate_answer(b, test_completed), 0);
  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);
  TEST(soa_get_local_sdp(b, NULL, &answer, &answerlen), 1);
  TEST_1(answer != NULL && answer != NONE);
  TEST_1(strstr(answer, "a=inactive"));
  TEST(soa_set_remote_sdp(a, 0, answer, -1), 1);
  TEST(soa_process_answer(a, test_completed), 0);
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_INACTIVE);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_INACTIVE);

  /* 'A' will retrieve call, 'B' will keep call on hold */
  offer = NONE;
  TEST(soa_set_params(a, SOATAG_HOLD(NULL), TAG_END()), 1);
  TEST(soa_generate_offer(a, 1, test_completed), 0);
  TEST(soa_get_local_sdp(a, NULL, &offer, &offerlen), 1);
  TEST_1(offer != NULL && offer != NONE);
  TEST_1(!strstr(offer, "a=sendonly"));

  TEST(soa_set_user_sdp(b, 0,
			"m=audio 5004 RTP/AVP 8 0\n"
			"a=sendonly\n", -1), 1);
  TEST(soa_set_remote_sdp(b, 0, offer, offerlen), 1);
  TEST(soa_generate_answer(b, test_completed), 0);
  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);
  TEST(soa_get_local_sdp(b, NULL, &answer, &answerlen), 1);
  TEST_1(answer != NULL && answer != NONE);
  TEST_1(strstr(answer, "a=sendonly"));
  TEST(soa_set_remote_sdp(a, 0, answer, -1), 1);
  TEST(soa_process_answer(a, test_completed), 0);
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_RECVONLY);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_RECVONLY);

  TEST_VOID(soa_terminate(a, NULL));
  TEST_VOID(soa_terminate(b, NULL));

  TEST_VOID(soa_destroy(a));
  TEST_VOID(soa_destroy(b));

  su_home_deinit(home);

  END();
}

int test_media_replace(struct context *ctx)
{
  BEGIN();
  int n;

  soa_session_t *a, *b;

  char const *offer = NONE, *answer = NONE;
  isize_t offerlen = (isize_t)-1, answerlen = (isize_t)-1;

  sdp_session_t const *a_sdp, *b_sdp;
  sdp_media_t const *m;

  char const a_caps[] =
    "v=0\r\n"
    "o=left 219498671 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=audio 5008 RTP/AVP 0 8\r\n"
    ;

  char const b_caps[] =
    "m=audio 5004 RTP/AVP 0 8\n"
    "a=rtpmap:96 G7231/8000\n"
    "a=rtpmap:97 G729/8000\n"
    "m=image 5556 UDPTL t38\r\n"
    "a=T38FaxVersion:0\r\n"
    "a=T38MaxBitRate:9600\r\n"
    "a=T38FaxFillBitRemoval:0\r\n"
    "a=T38FaxTranscodingMMR:0\r\n"
    "a=T38FaxTranscodingJBIG:0\r\n"
    "a=T38FaxRateManagement:transferredTCF\r\n"
    "a=T38FaxMaxDatagram:400\r\n";

  TEST_1(a = soa_create("static", ctx->root, ctx));
  TEST_1(b = soa_create("static", ctx->root, ctx));

  TEST(soa_set_user_sdp(a, 0, a_caps, strlen(a_caps)), 1);
  TEST(soa_set_user_sdp(b, 0, b_caps, strlen(b_caps)), 1);

  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, NULL, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);
  n = soa_get_local_sdp(b, NULL, &answer, &answerlen); TEST(n, 0);
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 1);
  n = soa_process_answer(a, test_completed); TEST(n, 0);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);

  /* ---------------------------------------------------------------------- */

  /* Re-O/A: replace media stream */

  /* Accept media without common codecs */
  TEST_1(soa_set_params(a, SOATAG_RTP_MISMATCH(0),
			SOATAG_ORDERED_USER(1),
			SOATAG_USER_SDP_STR(
    "v=0\r\n"
    "o=left 219498671 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=image 16384 UDPTL t38\r\n"
    "a=T38FaxVersion:0\r\n"
    "a=T38MaxBitRate:9600\r\n"
    "a=T38FaxFillBitRemoval:0\r\n"
    "a=T38FaxTranscodingMMR:0\r\n"
    "a=T38FaxTranscodingJBIG:0\r\n"
    "a=T38FaxRateManagement:transferredTCF\r\n"
    "a=T38FaxMaxDatagram:400\r\n"
    ),
			TAG_END()));

  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 1);
  n = soa_process_answer(a, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, NULL, NULL); TEST(n, 1);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST_1(m = a_sdp->sdp_media); TEST_1(!m->m_rejected);
  TEST(m->m_type, sdp_media_image);
  TEST(m->m_proto, sdp_proto_udptl);
  TEST_1(m->m_format);
  TEST_S(m->m_format->l_text, "t38");

  TEST_1(m = b_sdp->sdp_media); TEST_1(!m->m_rejected);
  TEST(m->m_type, sdp_media_image);
  TEST(m->m_proto, sdp_proto_udptl);
  TEST_1(m->m_format);
  TEST_S(m->m_format->l_text, "t38");

  TEST(soa_is_audio_active(a), SOA_ACTIVE_DISABLED);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_DISABLED);

  TEST_VOID(soa_terminate(a, NULL));
  TEST_VOID(soa_terminate(b, NULL));

  TEST_VOID(soa_destroy(a));
  TEST_VOID(soa_destroy(b));

  END();
}


int test_media_removal(struct context *ctx)
{
  BEGIN();
  int n;

  soa_session_t *a, *b;

  char const *offer = NONE, *answer = NONE;
  isize_t offerlen = (isize_t)-1, answerlen = (isize_t)-1;

  sdp_session_t const *a_sdp, *b_sdp;
  sdp_media_t const *m;

  char const a_caps[] =
    "v=0\r\n"
    "o=left 219498671 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=audio 5008 RTP/AVP 0 8\r\n"
    ;

  char const b_caps[] =
    "v=0\n"
    "m=audio 5004 RTP/AVP 0 8\n"
    "a=rtpmap:96 G7231/8000\n"
    "a=rtpmap:97 G729/8000\n";

  TEST_1(a = soa_create("static", ctx->root, ctx));
  TEST_1(b = soa_create("static", ctx->root, ctx));

  TEST(soa_set_user_sdp(a, 0, a_caps, strlen(a_caps)), 1);
  TEST(soa_set_user_sdp(b, 0, b_caps, strlen(b_caps)), 1);

  TEST_1(soa_set_params(b, SOATAG_RTP_MISMATCH(0),
			SOATAG_ORDERED_USER(1),
			TAG_END()));

  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, NULL, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);
  n = soa_get_local_sdp(b, NULL, &answer, &answerlen); TEST(n, 0);
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 1);
  n = soa_process_answer(a, test_completed); TEST(n, 0);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);

  /* ---------------------------------------------------------------------- */
  /* Re-O/A: remove media stream */
  TEST(soa_set_user_sdp(b, 0, "v=0", -1), 1);

  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(m = b_sdp->sdp_media); TEST_1(m->m_rejected);
  TEST_1(answer != NULL && answer != NONE);
  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 1);
  n = soa_process_answer(a, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, NULL, NULL); TEST(n, 1);
  TEST_1(m = a_sdp->sdp_media); TEST_1(m->m_rejected);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_REJECTED);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_REJECTED);

  TEST_VOID(soa_terminate(a, NULL));
  TEST_VOID(soa_terminate(b, NULL));

  TEST_VOID(soa_destroy(a));
  TEST_VOID(soa_destroy(b));

  /* ---------------------------------------------------------------------- */

  TEST_1(a = soa_create("static", ctx->root, ctx));
  TEST_1(b = soa_create("static", ctx->root, ctx));

  TEST(soa_set_user_sdp(a, 0, a_caps, strlen(a_caps)), 1);
  TEST(soa_set_user_sdp(b, 0, b_caps, strlen(b_caps)), 1);

  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, NULL, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);
  n = soa_get_local_sdp(b, NULL, &answer, &answerlen); TEST(n, 0);
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 1);
  n = soa_process_answer(a, test_completed); TEST(n, 0);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);

  /* ---------------------------------------------------------------------- */
  /* Re-O/A: offerer remove media stream from user sdp */
  TEST(soa_set_user_sdp(a, 0, "v=0", -1), 1);

  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, &offer, &offerlen); TEST(n, 1);
  TEST_1(m = a_sdp->sdp_media); TEST_1(m->m_rejected);
  TEST_1(offer != NULL && offer != NONE);
  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(m = b_sdp->sdp_media); TEST_1(m->m_rejected);
  TEST_1(answer != NULL && answer != NONE);
  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 1);
  n = soa_process_answer(a, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, NULL, NULL); TEST(n, 1);
  TEST_1(m = a_sdp->sdp_media); TEST_1(m->m_rejected);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_REJECTED);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_REJECTED);

  TEST_VOID(soa_terminate(a, NULL));
  TEST_VOID(soa_terminate(b, NULL));

  TEST_VOID(soa_destroy(a));
  TEST_VOID(soa_destroy(b));

  END();
}


int test_media_reject(struct context *ctx)
{
  BEGIN();
  int n;

  soa_session_t *a, *b;

  char const *offer = NONE, *answer = NONE;
  isize_t offerlen = (isize_t)-1, answerlen = (isize_t)-1;

  sdp_session_t const *b_sdp;

  char const a_caps[] =
    "v=0\r\n"
    "o=left 219498671 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=audio 5008 RTP/AVP 0 8 97\r\n"
    "a=rtpmap:97 GSM/8000\n"
    ;

  char const b_caps[] =
    "m=audio 0 RTP/AVP 96 97\n"
    "a=rtpmap:96 G7231/8000\n"
    "a=rtpmap:97 G729/8000\n";

  TEST_1(a = soa_create("static", ctx->root, ctx));
  TEST_1(b = soa_create("static", ctx->root, ctx));

  TEST(soa_set_user_sdp(a, 0, a_caps, strlen(a_caps)), 1);
  TEST(soa_set_user_sdp(b, 0, b_caps, strlen(b_caps)), 1);

  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, NULL, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);
  n = soa_get_local_sdp(b, NULL, &answer, &answerlen); TEST(n, 0);
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 1);
  n = soa_process_answer(a, test_completed); TEST(n, 0);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_REJECTED);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_REJECTED);

  TEST_VOID(soa_terminate(a, NULL));
  TEST_VOID(soa_terminate(b, NULL));

  TEST_VOID(soa_destroy(a));
  TEST_VOID(soa_destroy(b));

  END();
}


int test_media_replace2(struct context *ctx)
{
  BEGIN();
  int n;

  soa_session_t *a, *b;

  char const *offer = NONE, *answer = NONE;
  isize_t offerlen = (isize_t)-1, answerlen = (isize_t)-1;

  sdp_session_t const *a_sdp, *b_sdp;
  sdp_media_t const *m;

  char const a_caps[] =
    "v=0\r\n"
    "o=a 432432423423 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=image 5556 UDPTL t38\r\n"
    "a=T38FaxVersion:0\r\n"
    "a=T38MaxBitRate:9600\r\n"
    "a=T38FaxFillBitRemoval:0\r\n"
    "a=T38FaxTranscodingMMR:0\r\n"
    "a=T38FaxTranscodingJBIG:0\r\n"
    "a=T38FaxRateManagement:transferredTCF\r\n"
    "a=T38FaxMaxDatagram:400\r\n"
    "m=audio 5004 RTP/AVP 0 8\n"
    "a=rtpmap:96 G7231/8000\n"
    "a=rtpmap:97 G729/8000\n";

  char const a_caps2[] =
    "v=0\r\n"
    "o=a 432432423423 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=image 0 UDPTL t38\r\n"
    "m=image 5004 UDPTL t38\r\n"
    "a=T38FaxVersion:0\r\n"
    "a=T38MaxBitRate:9600\r\n"
    "a=T38FaxFillBitRemoval:0\r\n"
    "a=T38FaxTranscodingMMR:0\r\n"
    "a=T38FaxTranscodingJBIG:0\r\n"
    "a=T38FaxRateManagement:transferredTCF\r\n"
    "a=T38FaxMaxDatagram:400\r\n";

  char const b_caps[] =
    "v=0\r\n"
    "o=left 219498671 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=audio 5008 RTP/AVP 0 8\r\n"
    ;

  char const b_caps2[] =
    "v=0\r\n"
    "o=left 219498671 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=image 5008 UDPTL t38\r\n"
    ;

  TEST_1(a = soa_create("static", ctx->root, ctx));
  TEST_1(b = soa_create("static", ctx->root, ctx));

  TEST_1(soa_set_params(a, SOATAG_ORDERED_USER(1),
			SOATAG_REUSE_REJECTED(1),
			TAG_END()) > 0);
  TEST_1(soa_set_params(b, SOATAG_ORDERED_USER(1),
			SOATAG_REUSE_REJECTED(1),
			TAG_END()) > 0);

  TEST(soa_set_user_sdp(a, 0, a_caps, strlen(a_caps)), 1);
  TEST(soa_set_user_sdp(b, 0, b_caps, strlen(b_caps)), 1);

  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, NULL, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);
  n = soa_get_local_sdp(b, NULL, &answer, &answerlen); TEST(n, 0);
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 1);
  n = soa_process_answer(a, test_completed); TEST(n, 0);

  n = soa_get_local_sdp(a, &a_sdp, NULL, NULL); TEST(n, 1);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);

  TEST_1(a_sdp->sdp_media);
  TEST_1(a_sdp->sdp_media->m_type == sdp_media_image);
  TEST_1(a_sdp->sdp_media->m_next);
  TEST_1(a_sdp->sdp_media->m_next->m_type == sdp_media_audio);

  /* ---------------------------------------------------------------------- */

  /* Re-O/A: replace media stream */

  /* Do not accept media without common codecs */
  TEST_1(soa_set_params(b, SOATAG_RTP_MISMATCH(0),
			SOATAG_USER_SDP_STR(b_caps2),
			TAG_END()) > 0);

  n = soa_generate_offer(b, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  n = soa_set_remote_sdp(a, 0, offer, offerlen); TEST(n, 1);
  TEST_1(soa_set_params(a, SOATAG_RTP_MISMATCH(0),
			SOATAG_USER_SDP_STR(a_caps2),
			TAG_END()) > 0);

  n = soa_generate_answer(a, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  n = soa_set_remote_sdp(b, 0, answer, -1); TEST(n, 1);
  n = soa_process_answer(b, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, NULL, NULL); TEST(n, 1);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(m = a_sdp->sdp_media); TEST_1(m->m_rejected);
  TEST_1(m = m->m_next);
  TEST(m->m_type, sdp_media_image);
  TEST(m->m_proto, sdp_proto_udptl);
  TEST_1(m->m_format);
  TEST_S(m->m_format->l_text, "t38");

  TEST_1(m = b_sdp->sdp_media); TEST_1(m->m_rejected);
  TEST_1(m = m->m_next);
  TEST(m->m_type, sdp_media_image);
  TEST(m->m_proto, sdp_proto_udptl);
  TEST_1(m->m_format);
  TEST_S(m->m_format->l_text, "t38");

  TEST(soa_is_audio_active(a), SOA_ACTIVE_DISABLED);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_DISABLED);

  TEST_VOID(soa_terminate(a, NULL));
  TEST_VOID(soa_terminate(b, NULL));

  TEST_VOID(soa_destroy(a));
  TEST_VOID(soa_destroy(b));

  END();
}


int test_large_sessions(struct context *ctx)
{
  BEGIN();
  int n;

  soa_session_t *a, *b;

  char const *offer = NONE, *answer = NONE;
  isize_t offerlen = (isize_t)-1, answerlen = (isize_t)-1;

  sdp_session_t const *a_sdp, *b_sdp;
  sdp_media_t const *m;

  char const a_caps[] =
    "v=0\r\n"
    "o=a 432432423423 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=image 5556 UDPTL t38\r\n"
    "m=audio 5004 RTP/AVP 0 8\n"
    ;

  char const a_caps2[] =
    "v=0\r\n"
    "o=a 432432423423 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=image 5556 UDPTL t38\r\n"
    "m=audio 5004 RTP/AVP 0\n"
    "m=audio1 5006 RTP/AVP 8\n"
    "m=audio2 5008 RTP/AVP 3\n"
    "m=audio3 5010 RTP/AVP 9\n"
    "m=audio4 5010 RTP/AVP 15\n"
    ;

  char const b_caps[] =
    "v=0\r\n"
    "o=left 219498671 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=audio 5008 RTP/AVP 0 8\r\n"
    ;

  char const b_caps2[] =
    "v=0\r\n"
    "o=left 219498671 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=audio 5008 RTP/AVP 0 8\r\n"
    "m=image 5008 UDPTL t38\r\n"
    "m=audio00 5008 RTP/AVP 0 8\r\n"
    "m=audio01 5006 RTP/AVP 8\n"
    "m=audio02 5008 RTP/AVP 3\n"
    "m=audio03 5010 RTP/AVP 9\n"
    "m=audio04 5010 RTP/AVP 15\n"
    ;

  TEST_1(a = soa_create("static", ctx->root, ctx));
  TEST_1(b = soa_create("static", ctx->root, ctx));

  TEST_1(soa_set_params(a, SOATAG_ORDERED_USER(1),
			TAG_END()) > 0);
  TEST_1(soa_set_params(b, SOATAG_ORDERED_USER(1),
			TAG_END()) > 0);

  TEST(soa_set_user_sdp(a, 0, a_caps, strlen(a_caps)), 1);
  TEST(soa_set_user_sdp(b, 0, b_caps, strlen(b_caps)), 1);

  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, NULL, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  /* printf("offer1: %s\n", offer); */
  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);
  n = soa_get_local_sdp(b, NULL, &answer, &answerlen); TEST(n, 0);
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  /* printf("answer1: %s\n", answer); */
  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 1);
  n = soa_process_answer(a, test_completed); TEST(n, 0);

  n = soa_get_local_sdp(a, &a_sdp, NULL, NULL); TEST(n, 1);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);

  TEST_1(a_sdp->sdp_media);
  TEST_1(a_sdp->sdp_media->m_type == sdp_media_image);
  TEST_1(a_sdp->sdp_media->m_next);
  TEST_1(a_sdp->sdp_media->m_next->m_type == sdp_media_audio);

  /* ---------------------------------------------------------------------- */
  /* Re-O/A - activate image, add incompatible m= lines */

  TEST_1(soa_set_params(b, SOATAG_RTP_MISMATCH(0),
			SOATAG_USER_SDP_STR(b_caps2),
			SOATAG_REUSE_REJECTED(1),
			TAG_END()) > 0);

  n = soa_generate_offer(b, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  n = soa_set_remote_sdp(a, 0, offer, offerlen); TEST(n, 1);
  /* printf("offer2: %s\n", offer); */

  n = soa_generate_answer(a, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, &a_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  /* printf("answer2: %s\n", answer); */
  n = soa_set_remote_sdp(b, 0, answer, -1); TEST(n, 1);
  n = soa_process_answer(b, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, NULL, NULL); TEST(n, 1);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(m = a_sdp->sdp_media);
  TEST(m->m_type, sdp_media_image); TEST(m->m_proto, sdp_proto_udptl);
  TEST_1(m->m_format); TEST_S(m->m_format->l_text, "t38");

  TEST_1(m = b_sdp->sdp_media);
  TEST(m->m_type, sdp_media_image); TEST(m->m_proto, sdp_proto_udptl);
  TEST_1(m->m_format); TEST_S(m->m_format->l_text, "t38");

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);

  /* 2nd re-offer */
  /* Add even more incompatible lines */
  TEST_1(soa_set_params(a, SOATAG_RTP_MISMATCH(0),
			SOATAG_USER_SDP_STR(a_caps2),
			TAG_END()) > 0);

  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(a, NULL, &offer, &offerlen); TEST(n, 1);
  TEST_1(offer != NULL && offer != NONE);
  /* printf("offer3: %s\n", offer); */
  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);
  n = soa_get_local_sdp(b, NULL, &answer, &answerlen); TEST(n, 1);
  n = soa_generate_answer(b, test_completed); TEST(n, 0);
  n = soa_get_local_sdp(b, &b_sdp, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  /* printf("answer3: %s\n", answer); */
  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 1);
  n = soa_process_answer(a, test_completed); TEST(n, 0);

  n = soa_get_local_sdp(a, &a_sdp, NULL, NULL); TEST(n, 1);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST_VOID(soa_terminate(a, NULL));
  TEST_VOID(soa_terminate(b, NULL));

  TEST_VOID(soa_destroy(a));
  TEST_VOID(soa_destroy(b));

  END();
}


int test_asynch_offer_answer(struct context *ctx)
{
  BEGIN();

#if 0				/* This has never been implemented */
  int n;

  char const *caps = NONE, *offer = NONE, *answer = NONE;
  isize_t capslen = -1, offerlen = -1, answerlen = -1;

  char const a[] =
    "v=0\r\n"
    "o=left 219498671 2 IN IP4 127.0.0.2\r\n"
    "c=IN IP4 127.0.0.2\r\n"
    "m=audio 5004 RTP/AVP 0 8\r\n";

  char const b[] =
    "v=0\n"
    "o=right 93298573265 321974 IN IP4 127.0.0.3\n"
    "c=IN IP4 127.0.0.3\n"
    "m=audio 5006 RTP/AVP 96\n"
    "m=rtpmap:96 GSM/8000\n";

  n = soa_set_capability_sdp(ctx->asynch.a, 0,
			     "m=audio 5004 RTP/AVP 0 8", -1);
  TEST(n, 1);

  n = soa_set_capability_sdp(ctx->asynch.a, 0, a, strlen(a)); TEST(n, 1);
  n = soa_get_capability_sdp(ctx->asynch.a, 0, &caps, &capslen); TEST(n, 1);

  TEST_1(caps != NULL && caps != NONE);
  TEST_1(capslen > 0);

  n = soa_set_capability_sdp(ctx->asynch.b, 0, b, strlen(b)); TEST(n, 1);

  n = soa_generate_offer(ctx->asynch.a, 1, test_completed); TEST(n, 1);

  su_root_run(ctx->root); TEST(ctx->completed, ctx->asynch.a);
  ctx->completed = NULL;

  n = soa_get_local_sdp(ctx->asynch.a, 0, &offer, &offerlen); TEST(n, 1);

  n = soa_set_remote_sdp(ctx->asynch.b, 0, offer, offerlen); TEST(n, 1);

  n = soa_generate_answer(ctx->asynch.b, test_completed); TEST(n, 1);

  su_root_run(ctx->root); TEST(ctx->completed, ctx->asynch.b);
  ctx->completed = NULL;

  TEST_1(soa_is_complete(ctx->asynch.b));
  TEST(soa_activate(ctx->asynch.b, NULL), 0);

  n = soa_get_local_sdp(ctx->asynch.b, 0, &answer, &answerlen); TEST(n, 1);

  n = soa_set_remote_sdp(ctx->asynch.a, 0, answer, answerlen); TEST(n, 1);

  n = soa_process_answer(ctx->asynch.a, test_completed); TEST(n, 1);

  su_root_run(ctx->root); TEST(ctx->completed, ctx->asynch.a);
  ctx->completed = NULL;

  TEST_1(soa_is_complete(ctx->asynch.a));
  TEST(soa_activate(ctx->asynch.a, NULL), 0);

  TEST(soa_is_audio_active(ctx->asynch.a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_video_active(ctx->asynch.a), SOA_ACTIVE_DISABLED);
  TEST(soa_is_image_active(ctx->asynch.a), SOA_ACTIVE_DISABLED);
  TEST(soa_is_chat_active(ctx->asynch.a), SOA_ACTIVE_DISABLED);

  TEST(soa_is_remote_audio_active(ctx->asynch.a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_video_active(ctx->asynch.a), SOA_ACTIVE_DISABLED);
  TEST(soa_is_remote_image_active(ctx->asynch.a), SOA_ACTIVE_DISABLED);
  TEST(soa_is_remote_chat_active(ctx->asynch.a), SOA_ACTIVE_DISABLED);

  TEST(soa_deactivate(ctx->asynch.a, NULL), 0);
  TEST(soa_deactivate(ctx->asynch.b, NULL), 0);

  TEST_VOID(soa_terminate(ctx->asynch.a, NULL));

  TEST(soa_is_audio_active(ctx->asynch.a), SOA_ACTIVE_DISABLED);
  TEST(soa_is_remote_audio_active(ctx->asynch.a), SOA_ACTIVE_DISABLED);

  TEST_VOID(soa_terminate(ctx->asynch.b, NULL));

#endif

  END();
}

#define TEST_OC_ADDRESS(s, address, ip)		\
  TEST(test_address_in_offer(s, address, sdp_addr_ ## ip, address, sdp_addr_ ## ip), 0);

static int
test_address_in_offer(soa_session_t *ss,
		      char const *o_address,
		      int o_addrtype,
		      char const *c_address,
		      int c_addrtype)
{
  sdp_session_t const *sdp = NULL;
  sdp_connection_t const *c;

  TEST(soa_get_local_sdp(ss, &sdp, NULL, NULL), 1);
  TEST_1(sdp != NULL);
  TEST_1(c = sdp->sdp_connection);
  TEST(c->c_nettype, sdp_net_in);
  if (c_addrtype) TEST(c->c_addrtype, c_addrtype);
  if (c_address) TEST_S(c->c_address, c_address);

  TEST_1(c = sdp->sdp_origin->o_address);
  TEST(c->c_nettype, sdp_net_in);
  if (o_addrtype) TEST(c->c_addrtype, o_addrtype);
  if (o_address) TEST_S(c->c_address, o_address);

  return 0;
}

/** This tests the IP address selection logic.
 *
 * The IP address is selected based on the SOATAG_AF() preference,
 * SOATAG_ADDRESS(), and locally obtained address list.
 */
int test_address_selection(struct context *ctx)
{
  BEGIN();
  int n;

  static char const *ifaces1[] = {
    "eth2\0" "192.168.2.15\0" "fec0::21a:a0ff:fe71:815\0" "fe80::21a:a0ff:fe71:815\0",
    "eth0\0" "11.12.13.14\0" "2001:1508:1003::21a:a0ff:fe71:813\0" "fe80::21a:a0ff:fe71:813\0",
    "eth1\0" "12.13.14.15\0" "2001:1508:1004::21a:a0ff:fe71:814\0" "fe80::21a:a0ff:fe71:814\0",
    "lo0\0" "127.0.0.1\0" "::1\0",
    NULL
  };

  static char const *ifaces_ip6only[] = {
    "eth2\0" "fec0::21a:a0ff:fe71:815\0" "fe80::21a:a0ff:fe71:815\0",
    "eth0\0" "2001:1508:1003::21a:a0ff:fe71:813\0" "fe80::21a:a0ff:fe71:813\0",
    "eth1\0" "2001:1508:1004::21a:a0ff:fe71:814\0" "fe80::21a:a0ff:fe71:814\0",
    "lo0\0" "127.0.0.1\0" "::1\0",
    NULL
  };

  static char const *ifaces_ip4only[] = {
    "eth2\0" "192.168.2.15\0" "fe80::21a:a0ff:fe71:815\0",
    "eth0\0" "11.12.13.14\0" "fe80::21a:a0ff:fe71:813\0",
    "eth1\0" "12.13.14.15\0" "fe80::21a:a0ff:fe71:814\0",
    "lo0\0" "127.0.0.1\0" "::1\0",
    NULL
  };

  soa_session_t *a, *b;
  sdp_origin_t *o;

  su_home_t home[1] = { SU_HOME_INIT(home) };

  s2_localinfo_ifaces(ifaces1);

  TEST_1(a = soa_clone(ctx->a, ctx->root, ctx));

  /* SOATAG_AF(SOA_AF_IP4_ONLY) => select IP4 address */
  n = soa_set_params(a, SOATAG_AF(SOA_AF_IP4_ONLY), TAG_END());
  n = soa_set_user_sdp(a, 0, "m=audio 5008 RTP/AVP 0 8", -1); TEST(n, 1);
  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  TEST_OC_ADDRESS(a, "11.12.13.14", ip4);
  /* Should flush the session */
  TEST_VOID(soa_process_reject(a, NULL));

  /* SOATAG_AF(SOA_AF_IP6_ONLY) => select IP6 address */
  n = soa_set_params(a, SOATAG_AF(SOA_AF_IP6_ONLY), TAG_END());
  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  TEST_OC_ADDRESS(a, "2001:1508:1003::21a:a0ff:fe71:813", ip6);
  TEST_VOID(soa_terminate(a, NULL));

  /* SOATAG_AF(SOA_AF_IP4_IP6) => select IP4 address */
  n = soa_set_params(a, SOATAG_AF(SOA_AF_IP4_IP6), TAG_END());
  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  TEST_OC_ADDRESS(a, "11.12.13.14", ip4);
  TEST_VOID(soa_process_reject(a, NULL));

  /* SOATAG_AF(SOA_AF_IP6_IP4) => select IP6 address */
  n = soa_set_params(a, SOATAG_AF(SOA_AF_IP6_IP4), TAG_END());
  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  TEST_OC_ADDRESS(a, "2001:1508:1003::21a:a0ff:fe71:813", ip6);
  TEST_VOID(soa_terminate(a, NULL));

  /* SOATAG_AF(SOA_AF_IP4_IP6) but session mentions IP6 => select IP6  */
  n = soa_set_params(a, SOATAG_AF(SOA_AF_IP4_IP6), TAG_END());
  n = soa_set_user_sdp(a, 0, "c=IN IP6 ::\r\nm=audio 5008 RTP/AVP 0 8", -1); TEST(n, 1);
  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  TEST_OC_ADDRESS(a, "2001:1508:1003::21a:a0ff:fe71:813", ip6);
  TEST_VOID(soa_terminate(a, NULL));

  /* SOATAG_AF(SOA_AF_IP4_IP6), o= mentions IP6 => select IP4  */
  n = soa_set_user_sdp(a, 0, "o=- 1 1 IN IP6 ::\r\n"
		       "m=audio 5008 RTP/AVP 0 8", -1); TEST(n, 1);
  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  TEST_OC_ADDRESS(a, "11.12.13.14", ip4);
  TEST_VOID(soa_process_reject(a, NULL));

  /* SOATAG_AF(SOA_AF_IP4_IP6), c= uses non-local IP6
     => select local IP6 on o=  */
  n = soa_set_user_sdp(a, 0,
		       "c=IN IP6 2001:1508:1004::21a:a0ff:fe71:819\r\n"
		       "m=audio 5008 RTP/AVP 0 8", -1);
  TEST(n, 1);
  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  TEST(test_address_in_offer(a,
			     /* o= has local address */
			     "2001:1508:1003::21a:a0ff:fe71:813", sdp_addr_ip6,
			     /* c= has sdp-provided address */
			     "2001:1508:1004::21a:a0ff:fe71:819", sdp_addr_ip6), 0);
  TEST_VOID(soa_terminate(a, NULL));

  /* SOATAG_AF(SOA_AF_IP4_ONLY), no IP4 addresses  */
  s2_localinfo_ifaces(ifaces_ip6only);
  n = soa_set_params(a, SOATAG_AF(SOA_AF_IP4_ONLY), TAG_END());
  n = soa_set_user_sdp(a, 0, "m=audio 5008 RTP/AVP 0 8", -1);
  TEST(soa_generate_offer(a, 1, test_completed), -1);

  /* Retry with IP6 enabled */
  n = soa_set_params(a, SOATAG_AF(SOA_AF_IP4_IP6), TAG_END());
  TEST(soa_generate_offer(a, 1, test_completed), 0);
  TEST_OC_ADDRESS(a, "2001:1508:1003::21a:a0ff:fe71:813", ip6);
  TEST_VOID(soa_terminate(a, NULL));

  /* SOATAG_AF(SOA_AF_IP6_ONLY), no IP6 addresses  */
  s2_localinfo_ifaces(ifaces_ip4only);
  n = soa_set_params(a, SOATAG_AF(SOA_AF_IP6_ONLY), TAG_END());
  TEST(soa_generate_offer(a, 1, test_completed), -1); /* should fail */
  TEST_VOID(soa_terminate(a, NULL));

  /* SOATAG_AF(SOA_AF_IP4_ONLY), no IP4 addresses  */
  s2_localinfo_ifaces(ifaces_ip6only);
  n = soa_set_params(a, SOATAG_AF(SOA_AF_IP4_ONLY), TAG_END());
  TEST(soa_generate_offer(a, 1, test_completed), -1); /* should fail */
  TEST_VOID(soa_terminate(a, NULL));

  /* Select locally available address from the SOATAG_ADDRESS() list */
  s2_localinfo_ifaces(ifaces1);
  n = soa_set_params(a, SOATAG_AF(SOA_AF_IP4_IP6),
		     SOATAG_ADDRESS("test.com 17.18.19.20 12.13.14.15"),
		     TAG_END());
  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  TEST_OC_ADDRESS(a, "12.13.14.15", ip4);
  TEST_VOID(soa_process_reject(a, NULL));

  /* Select locally available IP6 address from the SOATAG_ADDRESS() list */
  n = soa_set_params(a, SOATAG_AF(SOA_AF_IP6_IP4),
		     SOATAG_ADDRESS("test.com 12.13.14.15 fec0::21a:a0ff:fe71:815"),
		     TAG_END());
  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  TEST_OC_ADDRESS(a, "fec0::21a:a0ff:fe71:815", ip6);
  TEST_VOID(soa_process_reject(a, NULL));

  /* Select first available address from the SOATAG_ADDRESS() list */
  n = soa_set_params(a, SOATAG_AF(SOA_AF_ANY),
		     SOATAG_ADDRESS("test.com 12.13.14.15 fec0::21a:a0ff:fe71:815"),
		     TAG_END());
  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  TEST_OC_ADDRESS(a, "12.13.14.15", ip4);
  TEST_VOID(soa_process_reject(a, NULL));

  /* Select preferred address from the SOATAG_ADDRESS() list */
  s2_localinfo_ifaces(ifaces1);
  n = soa_set_params(a, SOATAG_AF(SOA_AF_IP4_IP6),
		     SOATAG_ADDRESS("test.com fec0::22a:a0ff:fe71:815 19.18.19.20"),
		     TAG_END());
  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  TEST_OC_ADDRESS(a, "19.18.19.20", ip4);
  TEST_VOID(soa_process_reject(a, NULL));

  /* Select preferred address from the SOATAG_ADDRESS() list */
  s2_localinfo_ifaces(ifaces1);
  n = soa_set_params(a, SOATAG_AF(SOA_AF_IP6_IP4),
		     SOATAG_ADDRESS("test.com 19.18.19.20 fec0::22a:a0ff:fe71:815 fec0::22a:a0ff:fe71:819"),
		     TAG_END());
  n = soa_generate_offer(a, 1, test_completed); TEST(n, 0);
  TEST_OC_ADDRESS(a, "fec0::22a:a0ff:fe71:815", ip6);
  TEST_VOID(soa_process_reject(a, NULL));

  TEST_VOID(soa_destroy(a));

  (void)b; (void)o;
#if 0
  TEST_1(b = soa_clone(ctx->b, ctx->root, ctx));

  n = soa_set_remote_sdp(b, 0, offer, offerlen); TEST(n, 1);

  n = soa_get_local_sdp(b, NULL, &answer, &answerlen); TEST(n, 0);

  n = soa_set_params(b,
		     SOATAG_LOCAL_SDP_STR("m=audio 5004 RTP/AVP 8"),
		     SOATAG_AF(SOA_AF_IP4_ONLY),
		     SOATAG_ADDRESS("1.2.3.4"),
		     TAG_END());

  n = soa_generate_answer(b, test_completed); TEST(n, 0);

  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);

  n = soa_get_local_sdp(b, NULL, &answer, &answerlen); TEST(n, 1);
  TEST_1(answer != NULL && answer != NONE);
  TEST_1(strstr(answer, "c=IN IP4 1.2.3.4"));

  n = soa_set_remote_sdp(a, 0, answer, -1); TEST(n, 1);

  n = soa_process_answer(a, test_completed); TEST(n, 0);

  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_video_active(a), SOA_ACTIVE_DISABLED);
  TEST(soa_is_image_active(a), SOA_ACTIVE_DISABLED);
  TEST(soa_is_chat_active(a), SOA_ACTIVE_DISABLED);

  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_video_active(a), SOA_ACTIVE_DISABLED);
  TEST(soa_is_remote_image_active(a), SOA_ACTIVE_DISABLED);
  TEST(soa_is_remote_chat_active(a), SOA_ACTIVE_DISABLED);

  /* 'A' will put call on hold */
  offer = NONE;
  TEST(soa_set_params(a, SOATAG_HOLD("*"), TAG_END()), 1);

  TEST(soa_generate_offer(a, 1, test_completed), 0);
  TEST(soa_get_local_sdp(a, NULL, &offer, &offerlen), 1);
  TEST_1(offer != NULL && offer != NONE);
  TEST_1(strstr(offer, "a=sendonly"));
  TEST(soa_set_remote_sdp(b, 0, offer, offerlen), 1);
  TEST(soa_generate_answer(b, test_completed), 0);
  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);
  TEST(soa_get_local_sdp(b, NULL, &answer, &answerlen), 1);
  TEST_1(answer != NULL && answer != NONE);
  TEST_1(strstr(answer, "a=recvonly"));
  TEST(soa_set_remote_sdp(a, 0, answer, -1), 1);
  TEST(soa_process_answer(a, test_completed), 0);
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDONLY);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDONLY);

  /* 'A' will put call inactive */
  offer = NONE;
  TEST(soa_set_params(a, SOATAG_HOLD("#"), TAG_END()), 1);

  TEST(soa_generate_offer(a, 1, test_completed), 0);
  TEST(soa_get_local_sdp(a, NULL, &offer, &offerlen), 1);
  TEST_1(offer != NULL && offer != NONE);
  TEST_1(strstr(offer, "a=inactive"));
  TEST(soa_set_remote_sdp(b, 0, offer, offerlen), 1);
  TEST(soa_generate_answer(b, test_completed), 0);
  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);
  TEST(soa_get_local_sdp(b, NULL, &answer, &answerlen), 1);
  TEST_1(answer != NULL && answer != NONE);
  TEST_1(strstr(answer, "a=inactive"));
  TEST(soa_set_remote_sdp(a, 0, answer, -1), 1);
  TEST(soa_process_answer(a, test_completed), 0);
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_INACTIVE);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_INACTIVE);

  /* B will send an offer to A, but there is no change in O/A status */
  TEST(soa_generate_offer(b, 1, test_completed), 0);
  TEST(soa_get_local_sdp(b, NULL, &offer, &offerlen), 1);
  TEST_1(offer != NULL && offer != NONE);
  TEST_1(!strstr(offer, "a=inactive"));
  /* printf("offer:\n%s", offer); */
  TEST(soa_set_remote_sdp(a, 0, offer, offerlen), 1);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_generate_answer(a, test_completed), 0);
  TEST(soa_is_audio_active(a), SOA_ACTIVE_INACTIVE);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_INACTIVE);
  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);
  TEST(soa_get_local_sdp(a, NULL, &answer, &answerlen), 1);
  TEST_1(answer != NULL && answer != NONE);
  TEST_1(strstr(answer, "a=inactive"));
  /* printf("answer:\n%s", answer); */
  TEST(soa_set_remote_sdp(b, 0, answer, -1), 1);
  TEST(soa_process_answer(b, test_completed), 0);
  TEST(soa_activate(b, NULL), 0);


  TEST(soa_is_audio_active(b), SOA_ACTIVE_INACTIVE);
  TEST(soa_is_remote_audio_active(b), SOA_ACTIVE_INACTIVE);

  /* 'A' will release hold. */
  TEST(soa_set_params(a, SOATAG_HOLD(NULL), TAG_END()), 1);

  TEST(soa_generate_offer(a, 1, test_completed), 0);
  TEST(soa_get_local_sdp(a, NULL, &offer, &offerlen), 1);
  TEST_1(offer != NULL && offer != NONE);
  TEST_1(!strstr(offer, "a=sendonly") && !strstr(offer, "a=inactive"));
  TEST(soa_set_remote_sdp(b, 0, offer, offerlen), 1);
  TEST(soa_generate_answer(b, test_completed), 0);
  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);
  TEST(soa_get_local_sdp(b, NULL, &answer, &answerlen), 1);
  TEST_1(answer != NULL && answer != NONE);
  TEST_1(!strstr(answer, "a=recvonly") && !strstr(answer, "a=inactive"));
  TEST(soa_set_remote_sdp(a, 0, answer, -1), 1);
  TEST(soa_process_answer(a, test_completed), 0);
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);

  /* 'A' will put B on hold but this time with c=IN IP4 0.0.0.0 */
  TEST(soa_set_params(a, SOATAG_HOLD("*"), TAG_END()), 1);
  TEST(soa_generate_offer(a, 1, test_completed), 0);

  {
    sdp_session_t const *o_sdp;
    sdp_session_t *sdp;
    sdp_printer_t *p;
    sdp_connection_t *c;

    TEST(soa_get_local_sdp(a, &o_sdp, NULL, NULL), 1);
    TEST_1(o_sdp != NULL && o_sdp != NONE);
    TEST_1(sdp = sdp_session_dup(home, o_sdp));

    /* Remove mode, change c=, encode offer */
    if (sdp->sdp_media->m_connections)
      c = sdp->sdp_media->m_connections;
    else
      c = sdp->sdp_connection;
    TEST_1(c);
    c->c_address = "0.0.0.0";

    TEST_1(p = sdp_print(home, sdp, NULL, 0, sdp_f_realloc));
    TEST_1(sdp_message(p));
    offer = sdp_message(p); offerlen = strlen(offer);
  }

  TEST(soa_set_remote_sdp(b, 0, offer, -1), 1);
  TEST(soa_generate_answer(b, test_completed), 0);
  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);
  TEST(soa_get_local_sdp(b, NULL, &answer, &answerlen), 1);
  TEST_1(answer != NULL && answer != NONE);
  TEST_1(strstr(answer, "a=recvonly"));
  TEST(soa_set_remote_sdp(a, 0, answer, -1), 1);
  TEST(soa_process_answer(a, test_completed), 0);
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDONLY);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDONLY);
  TEST(soa_is_audio_active(b), SOA_ACTIVE_RECVONLY);
  TEST(soa_is_remote_audio_active(b), SOA_ACTIVE_RECVONLY);

  /* 'A' will propose adding video. */
  /* 'B' will reject. */
  TEST(soa_set_params(a,
		      SOATAG_HOLD(NULL),  /* 'A' will release hold. */
		      SOATAG_USER_SDP_STR("m=audio 5008 RTP/AVP 0 8\r\ni=x\r\n"
					  "m=video 5006 RTP/AVP 34\r\n"),
		      TAG_END()), 2);

  TEST(soa_generate_offer(a, 1, test_completed), 0);
  TEST(soa_get_local_sdp(a, NULL, &offer, &offerlen), 1);
  TEST_1(offer != NULL && offer != NONE);
  TEST_1(!strstr(offer, "a=sendonly"));
  TEST_1(strstr(offer, "m=video"));
  TEST(soa_set_remote_sdp(b, 0, offer, offerlen), 1);
  TEST(soa_generate_answer(b, test_completed), 0);
  TEST_1(soa_is_complete(b));
  TEST(soa_activate(b, NULL), 0);
  TEST(soa_get_local_sdp(b, NULL, &answer, &answerlen), 1);
  TEST_1(answer != NULL && answer != NONE);
  TEST_1(!strstr(answer, "a=recvonly"));
  TEST_1(strstr(answer, "m=video"));
  TEST(soa_set_remote_sdp(a, 0, answer, -1), 1);
  TEST(soa_process_answer(a, test_completed), 0);
  TEST(soa_activate(a, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_video_active(a), SOA_ACTIVE_REJECTED);

  {
    /* Test tags */
    sdp_session_t const *l = NULL, *u = NULL, *r = NULL;
    sdp_media_t const *m;

    TEST(soa_get_params(b,
			SOATAG_LOCAL_SDP_REF(l),
			SOATAG_USER_SDP_REF(u),
			SOATAG_REMOTE_SDP_REF(r),
			TAG_END()), 3);

    TEST_1(l); TEST_1(u); TEST_1(r);
    TEST_1(m = l->sdp_media); TEST(m->m_type, sdp_media_audio);
    TEST_1(!m->m_rejected);
    TEST_1(m = m->m_next); TEST(m->m_type, sdp_media_video);
    TEST_1(m->m_rejected);
  }

  /* 'B' will now propose adding video. */
  /* 'A' will accept. */
  TEST(soa_set_params(b,
		      SOATAG_USER_SDP_STR("m=audio 5004 RTP/AVP 0 8\r\n"
					  "m=video 5006 RTP/AVP 34\r\n"),
		      TAG_END()), 1);

  TEST(soa_generate_offer(b, 1, test_completed), 0);
  TEST(soa_get_local_sdp(b, NULL, &offer, &offerlen), 1);
  TEST_1(offer != NULL && offer != NONE);
  TEST_1(!strstr(offer, "b=sendonly"));
  TEST_1(strstr(offer, "m=video"));
  TEST(soa_set_remote_sdp(a, 0, offer, offerlen), 1);
  TEST(soa_generate_answer(a, test_completed), 0);
  TEST_1(soa_is_complete(a));
  TEST(soa_activate(a, NULL), 0);
  TEST(soa_get_local_sdp(a, NULL, &answer, &answerlen), 1);
  TEST_1(answer != NULL && answer != NONE);
  TEST_1(!strstr(answer, "b=recvonly"));
  TEST_1(strstr(answer, "m=video"));
  TEST(soa_set_remote_sdp(b, 0, answer, -1), 1);
  TEST(soa_process_answer(b, test_completed), 0);
  TEST(soa_activate(b, NULL), 0);

  TEST(soa_is_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_SENDRECV);
  TEST(soa_is_video_active(a), SOA_ACTIVE_SENDRECV);

  TEST_VOID(soa_terminate(a, NULL));

  TEST(soa_is_audio_active(a), SOA_ACTIVE_DISABLED);
  TEST(soa_is_remote_audio_active(a), SOA_ACTIVE_DISABLED);

  TEST_VOID(soa_terminate(b, NULL));

  TEST_VOID(soa_destroy(a));
  TEST_VOID(soa_destroy(b));
#endif
  su_home_deinit(home);

  END();
}

int test_deinit(struct context *ctx)
{
  BEGIN();

  su_root_destroy(ctx->root), ctx->root = NULL;
  soa_destroy(ctx->a);
  soa_destroy(ctx->b);

  END();
}

#if HAVE_ALARM
static RETSIGTYPE sig_alarm(int s)
{
  fprintf(stderr, "%s: FAIL! test timeout!\n", name);
  exit(1);
}
#endif

void usage(int exitcode)
{
  fprintf(stderr,
	  "usage: %s [-v|-q] [-a] [-l level] [-p outbound-proxy-uri]\n",
	  name);
  exit(exitcode);
}

int main(int argc, char *argv[])
{
  int retval = 0, quit_on_single_failure = 0;
  int i, o_attach = 0, o_alarm = 1;

  struct context ctx[1] = {{{ SU_HOME_INIT(ctx) }}};

  for (i = 1; argv[i]; i++) {
    if (strcmp(argv[i], "-v") == 0)
      tstflags |= tst_verbatim;
    else if (strcmp(argv[i], "-a") == 0)
      tstflags |= tst_abort;
    else if (strcmp(argv[i], "-q") == 0)
      tstflags &= ~tst_verbatim;
    else if (strcmp(argv[i], "-1") == 0)
      quit_on_single_failure = 1;
    else if (strncmp(argv[i], "-l", 2) == 0) {
      int level = 3;
      char *rest = NULL;

      if (argv[i][2])
	level = strtol(argv[i] + 2, &rest, 10);
      else if (argv[i + 1])
	level = strtol(argv[i + 1], &rest, 10), i++;
      else
	level = 3, rest = "";

      if (rest == NULL || *rest)
	usage(1);

      su_log_set_level(soa_log, level);
    }
    else if (strcmp(argv[i], "--attach") == 0) {
      o_attach = 1;
    }
    else if (strcmp(argv[i], "--no-alarm") == 0) {
      o_alarm = 0;
    }
    else if (strcmp(argv[i], "-") == 0) {
      i++; break;
    }
    else if (argv[i][0] != '-') {
      break;
    }
    else
      usage(1);
  }

#if HAVE_OPEN_C
  tstflags |= tst_verbatim;
#endif

  if (o_attach) {
    char line[10], *cr;
    printf("%s: pid %u\n", name, getpid());
    printf("<Press RETURN to continue>\n");
    cr = fgets(line, sizeof line, stdin);
    (void)cr;
  }
#if HAVE_ALARM
  else if (o_alarm) {
    alarm(60);
    signal(SIGALRM, sig_alarm);
  }
#endif

  su_init();

  if (!(TSTFLAGS & tst_verbatim)) {
    su_log_soft_set_level(soa_log, 0);
  }

#define SINGLE_FAILURE_CHECK()						\
  do { fflush(stdout);							\
    if (retval && quit_on_single_failure) { su_deinit(); return retval; } \
  } while(0)

  retval |= test_localinfo_replacement(); SINGLE_FAILURE_CHECK();

  retval |= test_api_errors(ctx); SINGLE_FAILURE_CHECK();
  retval |= test_soa_tags(ctx); SINGLE_FAILURE_CHECK();
  retval |= test_init(ctx, argv + i); SINGLE_FAILURE_CHECK();

  if (retval == 0) {
    retval |= test_address_selection(ctx); SINGLE_FAILURE_CHECK();
    retval |= test_large_sessions(ctx); SINGLE_FAILURE_CHECK();
    retval |= test_params(ctx); SINGLE_FAILURE_CHECK();
    retval |= test_static_offer_answer(ctx); SINGLE_FAILURE_CHECK();
    retval |= test_codec_selection(ctx); SINGLE_FAILURE_CHECK();
    retval |= test_media_replace(ctx); SINGLE_FAILURE_CHECK();
    retval |= test_media_removal(ctx); SINGLE_FAILURE_CHECK();
    retval |= test_media_reject(ctx); SINGLE_FAILURE_CHECK();
    retval |= test_media_replace2(ctx); SINGLE_FAILURE_CHECK();
    retval |= test_media_mode(ctx); SINGLE_FAILURE_CHECK();

    retval |= test_asynch_offer_answer(ctx); SINGLE_FAILURE_CHECK();
  }
  retval |= test_deinit(ctx); SINGLE_FAILURE_CHECK();

  su_deinit();

#if HAVE_OPEN_C
  sleep(5);
#endif

  return retval;
}
