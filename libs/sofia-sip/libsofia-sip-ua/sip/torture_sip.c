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

/**@ingroup sip_test @internal
 *
 * @CFILE torture_sip.c
 *
 * Unit-testing functions for SIP.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Tue Mar  6 18:33:42 2001 ppessi
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

/* Avoid casting sip_t to msg_pub_t and sip_header_t to msg_header_t */
#define MSG_PUB_T       struct sip_s
#define MSG_HDR_T       union sip_header_u

#include <sofia-sip/su_types.h>

#include <sofia-sip/su_tag.h>
#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_tag_io.h>

#include "sofia-sip/sip_parser.h"
#include <sofia-sip/sip_util.h>
#include <sofia-sip/sip_status.h>

#include <sofia-sip/sip_tag.h>
#include <sofia-sip/url_tag.h>
#include <sofia-sip/msg_addr.h>
#include <sofia-sip/msg_mclass.h>
#include <sofia-sip/msg_mclass_hash.h>

#include <sofia-sip/sip_extra.h>

int tstflags;

#define TSTFLAGS tstflags

#include <sofia-sip/tstdef.h>

char const *name = "torture_sip.c";

msg_mclass_t *test_mclass = NULL;

static msg_t *read_message(int flags, char const string[]);

static int test_identity(void)
{
  su_home_t *home;
  sip_alert_info_t *ai;
  sip_reply_to_t *rplyto;
  sip_remote_party_id_t *rpid;
  sip_p_asserted_identity_t *paid;
  sip_p_preferred_identity_t *ppid;

  msg_t *msg;
  sip_t *sip;

  BEGIN();

  msg_href_t const *href;
  msg_mclass_t const *def0, *def1, *ext;

  def0 = sip_default_mclass();

  /* Check that Refer-Sub has been added to our parser */
  TEST_1(href = msg_find_hclass(def0, "Refer-Sub", NULL));
  TEST_P(href->hr_class, sip_refer_sub_class);
  /* Check that Reply-To is not there */
  TEST_P(msg_find_hclass(def0, "Reply-To", NULL), def0->mc_unknown);

  TEST_1(ext = sip_extend_mclass(NULL));
  /* Update default parser */
  TEST_1(sip_update_default_mclass(ext) == 0);
  def1 = sip_default_mclass();
  TEST_1(def0 != def1);
  TEST_1(ext == def1);

  TEST_1(href = msg_find_hclass(def1, "Reply-To", NULL));
  TEST_P(href->hr_class, sip_reply_to_class);

  TEST_1(test_mclass = msg_mclass_clone(def0, 0, 0));

  msg = read_message(MSG_DO_EXTRACT_COPY,
    "BYE sip:foo@bar SIP/2.0\r\n"
    "To: <sip:foo@bar>;tag=deadbeef\r\n"
    "From: <sip:bar@foo>;\r\n"
    "Call-ID: 0ha0isndaksdj@10.1.2.3\r\n"
    "CSeq: 8 SUBSCRIBE\r\n"
    "Via: SIP/2.0/UDP 135.180.130.133\r\n"
    "Alert-Info: <http://test.com/tune>;walz, <http://test.com/buzz>\r\n"
    "Reply-To: Arska <sip:arska@gov.ca.us>;humppa\r\n"
    "P-Asserted-Identity: <sip:test@test.domain.com>\r\n"
    "P-Preferred-Identity: <sip:test@test.domain.com>, <tel:+358708008000>\r\n"
    "Remote-Party-ID: <sip:test2@test.domain.com>\r\n"
    "Content-Length: 0\r\n"
    "\r\n");

  sip = sip_object(msg);

  TEST_1(sip);
  TEST_1(!sip_alert_info(sip));
  TEST_1(!sip_reply_to(sip));
  TEST_1(!sip_p_asserted_identity(sip));
  TEST_1(!sip_p_preferred_identity(sip));
  TEST_1(!sip_remote_party_id(sip));

  msg_destroy(msg);

  TEST_1(msg_mclass_insert_header(test_mclass,
				  sip_p_asserted_identity_class, 0) > 0);
  TEST_1(msg_mclass_insert_header(test_mclass,
				  sip_p_preferred_identity_class, 0) > 0);

  msg = read_message(MSG_DO_EXTRACT_COPY,
    "BYE sip:foo@bar SIP/2.0\r\n"
    "To: <sip:foo@bar>;tag=deadbeef\r\n"
    "From: <sip:bar@foo>;\r\n"
    "Call-ID: 0ha0isndaksdj@10.1.2.3\r\n"
    "CSeq: 8 SUBSCRIBE\r\n"
    "Via: SIP/2.0/UDP 135.180.130.133\r\n"
    "Alert-Info: <http://test.com/tune>;walz, <http://test.com/buzz>\r\n"
    "Reply-To: Arska <sip:arska@gov.ca.us>;humppa\r\n"
    "P-Asserted-Identity: <sip:test@test.domain.com>\r\n"
    "P-Preferred-Identity: <sip:test@test.domain.com>, <tel:+358708008000>\r\n"
    "Remote-Party-ID: <sip:test2@test.domain.com>\r\n"
    "Content-Length: 0\r\n"
    "\r\n");

  sip = sip_object(msg);

  TEST_1(!sip_alert_info(sip));
  TEST_1(!sip_reply_to(sip));
  TEST_1(sip_p_asserted_identity(sip));
  TEST_1(sip_p_preferred_identity(sip));
  TEST_1(!sip_remote_party_id(sip));

  TEST_1(home = msg_home(msg));

  TEST_1((paid = sip_p_asserted_identity_make(home, "sip:joe@example.com")));
  TEST_1((paid = sip_p_asserted_identity_make
	  (home, "Jaska <sip:joe@example.com>, Helmi <tel:+3587808000>")));
  TEST_1(paid->paid_next);
  TEST_1((ppid = sip_p_preferred_identity_make(home, "sip:joe@example.com")));
  TEST_1((ppid = sip_p_preferred_identity_make
	  (home, "Jaska <sip:joe@example.com>, Helmi <tel:+3587808000>")));

  msg_destroy(msg);

  /* Now with extensions */
  TEST_1(test_mclass = msg_mclass_clone(def1, 0, 0));

  msg = read_message(MSG_DO_EXTRACT_COPY,
    "BYE sip:foo@bar SIP/2.0\r\n"
    "To: <sip:foo@bar>;tag=deadbeef\r\n"
    "From: <sip:bar@foo>;\r\n"
    "Call-ID: 0ha0isndaksdj@10.1.2.3\r\n"
    "CSeq: 8 SUBSCRIBE\r\n"
    "Via: SIP/2.0/UDP 135.180.130.133\r\n"
    "Alert-Info: <http://test.com/tune>;walz, <http://test.com/buzz>\r\n"
    "Reply-To: Arska <sip:arska@gov.ca.us>;humppa\r\n"
    "P-Asserted-Identity: <sip:test@test.domain.com>\r\n"
    "P-Preferred-Identity: <sip:test@test.domain.com>, <tel:+358708008000>\r\n"
    "Remote-Party-ID: <sip:test2@test.domain.com>\r\n"
    "Content-Length: 0\r\n"
    "\r\n");

  sip = sip_object(msg);

  TEST_1(ai = sip_alert_info(sip));
  TEST_S(ai->ai_url->url_host, "test.com");
  TEST_1(rplyto = sip_reply_to(sip));
  TEST_S(rplyto->rplyto_url->url_host, "gov.ca.us");
  TEST_1(paid = sip_p_asserted_identity(sip));
  TEST_1(ppid = sip_p_preferred_identity(sip));
  TEST_1(rpid = sip_remote_party_id(sip));
  TEST_S(rpid->rpid_url->url_host, "test.domain.com");

  msg_destroy(msg);

  {
    su_home_t *home = su_home_clone(NULL, sizeof *home);

    char *s;
    char const canonic[] =
      "\"Jaska Jokunen\" <sip:jaska.jokunen@example.com>;"
      "screen=yes;party=called;id-type=user;privacy=\"name,uri-network\"";
    char const canonic2[] =
      "Jaska Jokunen <sip:jaska.jokunen@example.com>;"
      "screen=yes;party=called;id-type=user;privacy=\"name,uri-network\"";

    sip_remote_party_id_t *rpid, *d;

    TEST_1(rpid = sip_remote_party_id_make(home, canonic));
    TEST_S(rpid->rpid_display, "\"Jaska Jokunen\"");
    TEST_S(rpid->rpid_url->url_user, "jaska.jokunen");
    TEST_S(rpid->rpid_params[0], "screen=yes");
    TEST_S(rpid->rpid_screen, "yes");
    TEST_S(rpid->rpid_party, "called");
    TEST_S(rpid->rpid_id_type, "user");
    TEST_S(rpid->rpid_privacy, "\"name,uri-network\"");
    TEST_1(s = sip_header_as_string(home, (void*)rpid));
    TEST_S(s, canonic);
    TEST_1(d = sip_remote_party_id_dup(home, rpid));
    TEST_S(d->rpid_display, rpid->rpid_display);
    TEST_S(d->rpid_params[0], rpid->rpid_params[0]);

    TEST_1(rpid = sip_remote_party_id_make(home, canonic2));
    TEST_S(rpid->rpid_display, "Jaska Jokunen");
    TEST_1(s = sip_header_as_string(home, (void*)rpid));
    TEST_S(s, canonic2);
    TEST_1(d = sip_remote_party_id_dup(home, rpid));
    TEST_S(d->rpid_display, rpid->rpid_display);

    su_home_check(home);

    su_home_zap(home);
  }

  END();
}

int test_url_headers(void)
{
  BEGIN();
  su_home_t *home;
  char *s, *d;
  tagi_t *t;
  url_t *url;
  sip_from_t const *f;
  sip_accept_t const *ac;
  sip_payload_t const *body;
  sip_refer_sub_t rs[1];

  TEST_1(home = su_home_new(sizeof *home));

  sip_refer_sub_init(rs)->rs_value = "false";

  s = sip_headers_as_url_query
    (home,
     SIPTAG_SUBJECT_STR("kuik"),
     SIPTAG_REFER_SUB(rs),
     TAG_END());

  TEST_1(s);
  TEST_S(s, "subject=kuik&refer-sub=false");

  s = sip_headers_as_url_query
    (home,
     SIPTAG_TO_STR("\"Joe\" <sip:joe@example.com>;tag=foofaa"),
     SIPTAG_SUBJECT_STR("foo"),
     TAG_END());

  TEST_1(s);
  TEST_S(s, "to=%22Joe%22%20%3Csip%3Ajoe@example.com%3E%3Btag%3Dfoofaa"
	 "&subject=foo");

  url = url_format(home, "sip:test@example.net?%s", s); TEST_1(url);

  TEST_S(url->url_headers, s);

  s = sip_headers_as_url_query
    (home,
     SIPTAG_FROM_STR("<sip:joe@example.com;user=ip>"),
     SIPTAG_ACCEPT_STR(""),
     SIPTAG_PAYLOAD_STR("hello"),
     SIPTAG_ACCEPT_STR(""),
     TAG_END());

  TEST_S(s, "from=%3Csip%3Ajoe@example.com%3Buser%3Dip%3E"
	 "&accept="
	 "&body=hello"
	 "&accept=");

  d = url_query_as_header_string(home, s);
  TEST_S(d, "from:<sip:joe@example.com;user=ip>\n"
	 "accept:\n"
	 "accept:\n"
	 "\n"
	 "hello");

  t = sip_url_query_as_taglist(home, s, NULL); TEST_1(t);

  TEST_P(t[0].t_tag, siptag_from);    TEST_1(f = (void *)t[0].t_value);
  TEST_P(t[1].t_tag, siptag_accept);  TEST_1(ac = (void *)t[1].t_value);
  TEST_P(t[2].t_tag, siptag_payload); TEST_1(body = (void *)t[2].t_value);
  TEST_P(t[3].t_tag, siptag_accept);

  s = "xyzzy=foo";

  t = sip_url_query_as_taglist(home, s, NULL); TEST_1(t);

  TEST_P(t[0].t_tag, siptag_header_str);
  TEST_1(d = (void *)t[0].t_value);
  TEST_S(d, "foo");

  TEST_1(!sip_headers_as_url_query(home, SIPTAG_SEPARATOR_STR(""), TAG_END()));

  TEST_VOID(su_home_unref(home));

  END();
}

int test_manipulation(void)
{
  BEGIN();

  sip_content_length_t *l;
  sip_payload_t *pl;
  msg_t *msg, *msg0;
  sip_t *sip;

  msg0 = read_message(MSG_DO_EXTRACT_COPY,
    "MESSAGE sip:foo@bar SIP/2.0\r\n"
    "To: Joe User <sip:foo@bar>\r\n"
    "From: \"Bar Owner\" <sip:bar@foo>;tag=foobar\r\n"
    "Call-ID: 0ha0isndaksdj@10.1.2.3\r\n"
    "CSeq: 8 MESSAGE\r\n"
    "Via: SIP/2.0/UDP 135.180.130.133\r\n"
    "Content-Length: 7\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n"
    "Heippa!");
  TEST_1(msg0);
  TEST_1(msg = msg_copy(msg0));
  TEST_1(sip = sip_object(msg));

  TEST_1(l = sip_content_length_make(msg_home(msg), "6"));
  TEST_1(pl = sip_payload_make(msg_home(msg), "hello!"));

  TEST_1(msg_header_replace(msg, NULL,
			    (void *)sip->sip_content_length,
			    (void *)l) >= 0);
  TEST_1(msg_header_replace(msg, NULL,
			    (void *)sip->sip_payload,
			    (void *)pl) >= 0);

  TEST(msg_serialize(msg, NULL), 0);
  TEST_1(msg_prepare(msg) > 0);

  msg_destroy(msg);
  msg_destroy(msg0);

  END();
}

int test_methods(void)
{
  int i;
  char name[32];

  BEGIN();

  for (i = 1; sip_method_names[i]; i++) {
    TEST_S(sip_method_names[i], sip_method_name(i, "foo"));
  }

  {
    char version[] = "protocol /  version  ";
    char *end = version + strlen(version);
    char *s = version;
    char const *result = NULL;

    TEST(sip_version_d(&s, &result), 0);
    TEST_P(s, end);
    TEST_S(result, "protocol/version");
  }

  {
    char udp[] = "SIP/ 2.0  /  udp";
    char tcp[] = "SIP / 2.0  /  tcp";
    char tls[] = "SIP / 2.0  /  tls";
    char sctp[] = "SIP / 2.0  /  scTp";
    char dtls[] = "SIP/2.0/TLS-UDP";
    char tls_sctp[] = "SIP/2.0/TLS-SCTP";
    char *s, *end;
    char const *result = NULL;

    s = udp; end = s + strlen(s);
    TEST_SIZE(sip_transport_d(&s, &result), 0); TEST_P(s, end);
    TEST_S(result, sip_transport_udp);

    s = tcp; end = s + strlen(s);
    TEST_SIZE(sip_transport_d(&s, &result), 0); TEST_P(s, end);
    TEST_S(result, sip_transport_tcp);

    s = tls; end = s + strlen(s);
    TEST_SIZE(sip_transport_d(&s, &result), 0); TEST_P(s, end);
    TEST_S(result, sip_transport_tls);

    s = sctp; end = s + strlen(s);
    TEST_SIZE(sip_transport_d(&s, &result), 0); TEST_P(s, end);
    TEST_S(result, sip_transport_sctp);

    s = dtls; end = s + strlen(s);
    TEST_SIZE(sip_transport_d(&s, &result), 0); TEST_P(s, end);
    TEST_S(result, "SIP/2.0/TLS-UDP");

    s = tls_sctp; end = s + strlen(s);
    TEST_SIZE(sip_transport_d(&s, &result), 0); TEST_P(s, end);
    TEST_S(result, "SIP/2.0/TLS-SCTP");
  }
  END();
}

/* Test <sip_basic.c> functions. */
int test_basic(void)
{
  su_home_t *home = su_home_new(sizeof *home);

  BEGIN();

  TEST_1(home);

  {
    sip_request_t *rq, *rq1;

    rq = sip_request_make(home, "INVITE sip:joe@example.com SIP/2.1");
    TEST_1(rq);
    TEST(rq->rq_method, sip_method_invite);
    TEST_S(rq->rq_method_name, "INVITE");
    TEST_1(rq1 = sip_request_dup(home, rq));

    su_free(home, rq);
    su_free(home, rq1);

    rq = sip_request_make(home, "invite sip:joe@example.com SIP/2.0");
    TEST_1(rq);
    TEST(rq->rq_method, sip_method_unknown);
    TEST_S(rq->rq_method_name, "invite");

    TEST_1(rq1 = sip_request_dup(home, rq));

    su_free(home, rq);
    su_free(home, rq1);

    TEST_1(!sip_request_create(home, sip_method_unknown, NULL,
			       (void *)"sip:joe@example.com", NULL));
    TEST_1(rq = sip_request_create(home, sip_method_unknown, "invite",
				   (void *)"sip:joe@example.com", NULL));
    TEST(rq->rq_method, sip_method_unknown);
    TEST_S(rq->rq_method_name, "invite");
    su_free(home, rq);

    TEST_1(rq = sip_request_create(home, sip_method_unknown, "INVITE",
				   (void *)"sip:joe@example.com", NULL));
    TEST(rq->rq_method, sip_method_invite);
    TEST_S(rq->rq_method_name, "INVITE");

    su_free(home, rq);

    TEST_1(rq = sip_request_create(home, sip_method_invite, "foobar",
				   (void *)"sip:joe@example.com", NULL));
    TEST(rq->rq_method, sip_method_invite);
    TEST_S(rq->rq_method_name, "INVITE");

    su_free(home, rq);
  }

  {
    sip_status_t *st;

    TEST_1(st = sip_status_make(home, "SIP/2.0 200 OK"));
    su_free(home, st);

    TEST_1(st = sip_status_make(home, "SIP/2.0 200"));
    su_free(home, st);

    TEST_1(!sip_status_make(home, "SIP2.0 200 OK"));
    TEST_1(!sip_status_create(home, 99, NULL, "SIP/2.1"));
    TEST_1(!sip_status_create(home, 700, NULL, "SIP/2.1"));
    TEST_1(st = sip_status_create(home, 200, "Ok", "SIP/2.2"));
    su_free(home, st);

    TEST_1(st = sip_status_create(home, 200, NULL, "SIP/2.0"));
    su_free(home, st);
    TEST_1(st = sip_status_create(home, 200, NULL, NULL));
    su_free(home, st);
    TEST_1(st = sip_status_create(home, 699, NULL, NULL));
    su_free(home, st);
  }

  {
    sip_payload_t *pl;

    TEST_1(pl = sip_payload_create(home, "foo", 3));
    su_free(home, pl);

    TEST_1(pl = sip_payload_create(home, NULL, 3));
    su_free(home, pl);
  }

  {
    sip_separator_t *sep;

    TEST_1(!sip_separator_make(home, "foo"));
    TEST_1(sep = sip_separator_create(home));
    su_free(home, sep);
  }


  /* Test name-addr things */
  {
    su_home_t home[1] = { SU_HOME_INIT(home) };
    char const *display;
    url_t url[1];
    msg_param_t const *params;
    char const *comment;
    char const na[] = "Raaka Arska <tel:+358501970>;param=1;humppa (test) ";
    char const na2[] = "tel:+358501970;param=1;humppa (test) ";
    char *s, buf[sizeof(na)], ebuf[sizeof(na) + 32];

    s = strcpy(buf, na);

    TEST_1(sip_name_addr_d(home, &s, &display, url, &params, &comment) >= 0);
    TEST_P(s, buf + strlen(na));
    TEST_1(display);
    TEST(url->url_type, url_tel);
    TEST_1(params);
    TEST_1(comment);

    TEST_SIZE(sip_name_addr_e(ebuf, sizeof(ebuf), 0, display, 0, url,
			      params, comment),
	      strlen(na) - 1);
    TEST_1(strncmp(na, ebuf, strlen(na) - 1) == 0);

    s = strcpy(buf, na2);

    TEST_1(sip_name_addr_d(home, &s, &display, url, &params, &comment) >= 0);
    TEST_S(s, "");
    TEST_P(s, buf + strlen(na2));
    TEST_1(!display);
    TEST(url->url_type, url_tel);
    TEST_1(params);
    TEST_1(comment);

    su_home_deinit(home);
  }

  {
    sip_from_t *f; sip_to_t *t, *t2;

    TEST_1(f = sip_from_create(home, (void *)"sip:joe@bar"));
    TEST_1(sip_from_add_param(home, f, NULL) == -1);
    TEST_1(sip_from_add_param(home, f, "tag=tagged") == 0);
    TEST_S(f->a_tag, "tagged");
    TEST_1(sip_from_tag(home, f, "jxahudsf") == -1);
    while (f->a_params && f->a_params[0])
      msg_header_remove_param(f->a_common, f->a_params[0]);
    TEST_P(f->a_tag, NULL);
    TEST_1(sip_from_add_param(home, f, "test=1") == 0);
    TEST_1(sip_from_tag(home, f, "jxahudsf") == 0);
    TEST_S(f->a_tag, "jxahudsf");
    su_free(home, f);

    TEST_1(!sip_from_create(home, (void *)"sip:joe@[baa"));

    TEST_1(!sip_from_make(home, (void *)"tester <>;tag=fasjfuios"));

    TEST_1(f = sip_from_make(home, (void *)"sip:joe@bar (foo)"));
    su_free(home, f);

    TEST_1(f = sip_from_make(home, (void *)"<sip:joe@bar;tag=bar> (joe)"));
    TEST_1(sip_from_tag(home, f, "tag=jxahudsf") == 0);
    su_free(home, f);

    TEST_1(f = sip_from_create(home, (void *)"<sip:joe@bar;tag=bar> (joe)"));
    TEST_1(sip_is_from((sip_header_t*)f));
    su_free(home, f);

    TEST_1(t = sip_to_create(home, (void *)"<sip:joe@bar;tag=bar> (joe)"));
    TEST_1(sip_is_to((sip_header_t*)t));
    TEST_1(sip_to_tag(home, t, "tag=jxahudsf") == 0);
    TEST_S(t->a_tag, "jxahudsf");
    TEST(msg_header_replace_param(home, t->a_common, "tag=bar"), 1);
    TEST_S(t->a_tag, "bar");

    TEST_1(t2 = sip_to_dup(home, t));
    TEST_S(t2->a_tag, "bar");

    TEST(msg_header_remove_param(t->a_common, "tag"), 1);
    TEST_P(t->a_tag, NULL);
    TEST_1(sip_to_add_param(home, t, "tst=1") == 0);
    TEST_P(t->a_tag, NULL);

    su_free(home, t);
  }

  {
    sip_call_id_t *i, *i0;
    TEST_1(i = sip_call_id_create(home, "example.com"));
    i->i_hash = 0;
    TEST_1(i0 = sip_call_id_dup(home, i));
    su_free(home, i);
    TEST_1(i = sip_call_id_make(home, i0->i_id));
    TEST(i->i_hash, i0->i_hash);
    su_free(home, i);
    su_free(home, i0);
  }

  {
    sip_cseq_t *cs, *cs0;

    TEST_1(cs = sip_cseq_create(home, 123456789, sip_method_invite, "1nvite"));
    TEST(cs->cs_seq, 123456789);
    TEST(cs->cs_method, sip_method_invite);
    TEST_S(cs->cs_method_name, "INVITE");

    su_free(home, cs);

    TEST_1(cs = sip_cseq_create(home, 123456789, sip_method_invite, NULL));
    TEST(cs->cs_seq, 123456789);
    TEST(cs->cs_method, sip_method_invite);
    TEST_S(cs->cs_method_name, "INVITE");
    TEST_1(cs0 = sip_cseq_dup(home, cs));

    su_free(home, cs);
    su_free(home, cs0);

    TEST_1(!sip_cseq_create(home, 123456789, sip_method_unknown, NULL));

    TEST_1(cs = sip_cseq_create(home, 123456789, sip_method_unknown,
				"invite"));
    TEST(cs->cs_seq, 123456789);
    TEST(cs->cs_method, sip_method_unknown);
    TEST_S(cs->cs_method_name, "invite");
    TEST_1(cs0 = sip_cseq_dup(home, cs));

    su_free(home, cs);
    su_free(home, cs0);
  }

  {
    sip_contact_t *m, *m0;

    TEST_1(!sip_contact_make(home, ",,"));

    TEST_1(m = sip_contact_create(home, (void *)"sip:joe@bar",
				  "q=0.2",
				  "+message",
				  NULL));
    TEST_S(m->m_q, "0.2");

    TEST_1(m0 = sip_contact_dup(home, m));

    TEST_1(sip_contact_add_param(home, m, "q=0.5") >= 0);
    TEST_1(sip_contact_add_param(home, m, "video=FALSE") >= 0);
    TEST_1(sip_contact_add_param(home, m, NULL) == -1);
    TEST_1(sip_contact_add_param(home, NULL, "video=FALSE") == -1);
    TEST_1(sip_contact_add_param(home, m, "audio=FALSE") == 0);
    TEST_1(sip_contact_add_param(home, m, "expires=0") == 0);

    TEST_S(m->m_q, "0.5");
    TEST_S(m->m_expires, "0");

    TEST_1(!sip_contact_create(home, (void *)"sip:joe@[baa",
			       "audio", "video", NULL));

    TEST_1(sip_header_format(home, sip_contact_class, "*"));

    su_free(home, m);
    su_free(home, m0);
  }

  {
    sip_via_t *v;
    char *s;

    v = sip_via_make(home, "SIP/2.0/UDP domain.invalid:5060"); TEST_1(v);
    s = sip_contact_string_from_via(home, v, NULL, v->v_protocol);
    TEST_S(s, "<sip:domain.invalid;transport=udp>");
    su_free(home, v), su_free(home, s);

    TEST_1(sip_transport_has_tls("SIP/2.0/TLS-SCTP"));
    TEST_1(sip_transport_has_tls("TLS-UDP"));

    v = sip_via_make(home, "SIP/2.0/TLS-SCTP domain.invalid"); TEST_1(v);
    s = sip_contact_string_from_via(home, v, NULL, v->v_protocol);
    TEST_S(s, "<sips:domain.invalid;transport=tls-sctp>");
    su_free(home, v), su_free(home, s);
  }

  {
    char *input;
    char const *output = NULL;
    char udp[] = "sip/2.0/udp";
    char tcp[] = "sip/2.0/tCp ";
    char sctp[] = "sip/2.0/sctp\t";
    char tls[] = "sip/2.0/tls\r";

    input = udp;
    TEST(sip_transport_d(&input, &output), 0);
    TEST_S(output, "SIP/2.0/UDP");

    input = tcp;
    TEST(sip_transport_d(&input, &output), 0);
    TEST_S(output, "SIP/2.0/TCP");

    input = sctp;
    TEST(sip_transport_d(&input, &output), 0);
    TEST_S(output, "SIP/2.0/SCTP");

    input = tls;
    TEST(sip_transport_d(&input, &output), 0);
    TEST_S(output, "SIP/2.0/TLS");
  }

  {
    sip_expires_t *ex;

    TEST_1(!sip_expires_make(home, "-12+1"));

    TEST_1(ex = sip_expires_make(home, "4294967297")); /* XXX */
    su_free(home, ex);

    TEST_1(ex = sip_expires_make(home, "Wed, 25 Mar 2004 14:49:29 GMT"));
    su_free(home, ex);

    TEST_1(ex = sip_expires_create(home, 3600));
    su_free(home, ex);
  }

  {
    sip_retry_after_t *ra;
    char const *s;

    TEST_1(!(ra = sip_retry_after_make(home, "50 (foo")));
    TEST_1(ra = sip_retry_after_make(home, "50 (foo) ; duration = 13"));
    TEST_S(ra->af_duration, "13");
    TEST_S(ra->af_comment, "foo");
    TEST(msg_header_remove_param(ra->af_common, "duration"), 1);
    TEST_P(ra->af_duration, NULL);

    s = sip_header_as_string(home, (void*)ra);
    TEST_S(s, "50 (foo)");

    TEST(msg_header_add_param(home, ra->af_common, "x=z"), 0);
    s = sip_header_as_string(home, (void*)ra);
    TEST_S(s, "50 (foo) ;x=z");

    su_free(home, ra);
  }

  {
    sip_date_t *d;

    TEST_1(!(d = sip_date_make(home, "Mon, 30 Feb 1896 23:59:59 GMT")));
    su_free(home, d);

    TEST_1(d = sip_date_create(home, (1<<30)));
    su_free(home, d);

    TEST_1(d = sip_date_create(home, 0));
    TEST_1(d->d_time != 0);
    su_free(home, d);
  }

  {
    sip_route_t *r, *r0;

    TEST_1(!sip_route_make(home, "<sip:foo@[bar:50>;lr"));
    TEST_1(r = sip_route_make(home, "<sip:foo@[baa::1]:5060>;lr"));
    TEST_1(r0 = sip_route_dup(home, r));

    TEST_1(sip_route_fix(r));
    TEST_1(url_has_param(r->r_url, "lr"));

    su_free(home, r);
    TEST_1(r = sip_route_create(home, r0->r_url, r0->r_url));

    su_free(home, r); su_free(home, r0);
  }

  {
    sip_record_route_t *r, *r0;

    TEST_1(!sip_record_route_make(home, "<sip:foo@[bar:50>;lr"));
    TEST_1(!sip_record_route_make(home, "<sip:foo@[baa::1]>;lr bar, sip:foo"));
    TEST_1(r = sip_record_route_make(home, "<sip:foo@[baa::1]:5060>;lr"));
    TEST_1(r0 = sip_record_route_dup(home, r));
    su_free(home, r);

    TEST_1(r = sip_route_create(home, r0->r_url, r0->r_url));

    su_free(home, r), su_free(home, r0);
  }

  {
    sip_via_t *v, *v0;

    TEST_1(!sip_via_make(home, ",,"));
    TEST_1(!sip_via_make(home, "SIP// host:5060 (foo),"));
    TEST_1(!sip_via_make(home, "SIP/2.0/TCP host:5060 (foo) bar,"));
    TEST_1(!sip_via_make(home, "SIP/2.0/TCP [3ffe::1:5060 (foo),"));

    TEST_1(v = sip_via_create(home, "bar.com",
			      "50600",
			      "SIP/2.0/UDP",
			      "hidden",
			      "rport=50601",
			      "comp=sigcomp",
			      "branch=1",
			      "q=0.2",
			      NULL));
    TEST_S(v->v_branch, "1");
    TEST_S(v->v_rport, "50601");
    TEST_S(v->v_comp, "sigcomp");

    TEST_1(v = sip_via_make(home, "SIP/2.0/UDP bar.com:50600"
			    " ;hidden;rport=50601;comp=sigcomp;branch=1;ttl=15"
			    " ; maddr=[::227.0.0.1]"
			    " (This is a comment) "));
    TEST_S(v->v_ttl, "15");
    TEST_S(v->v_maddr, "[::227.0.0.1]");
    TEST_S(v->v_branch, "1");
    TEST_S(v->v_rport, "50601");
    TEST_S(v->v_comp, "sigcomp");

    TEST_1(v0 = sip_via_dup(home, v));

    TEST(msg_header_add_param(home, v->v_common, "rport"), 0);
    TEST_S(v->v_rport, "");
    TEST(msg_header_remove_param(v->v_common, "comp"), 1);
    TEST_P(v->v_comp, NULL);
    TEST(msg_header_remove_param(v->v_common, "ttl"), 1);
    TEST_P(v->v_ttl, NULL);
    TEST(msg_header_remove_param(v->v_common, "maddr"), 1);
    TEST_P(v->v_maddr, NULL);
    TEST(msg_header_remove_param(v->v_common, "rport"), 1);
    TEST_P(v->v_rport, NULL);
    TEST(msg_header_remove_param(v->v_common, "branch"), 1);
    TEST_P(v->v_branch, NULL);

    TEST_1(sip_via_add_param(home, v, "video=FALSE") == 0);
    TEST_1(sip_via_add_param(home, v, NULL) == -1);
    TEST_1(sip_via_add_param(home, NULL, "video=FALSE") == -1);
    TEST_1(sip_via_add_param(home, v, "audio=FALSE") == 0);
    TEST_1(sip_via_add_param(home, v, "branch=0") == 0);

    su_free(home, v);
    su_free(home, v0);

    TEST_1(v = sip_via_create(home, "bar.com",
			      "50600",
			      NULL,
			      "rport=50601",
			      "branch=1",
			      "q=0.2",
			      NULL));
    TEST_S(v->v_protocol, "SIP/2.0/UDP");
    su_free(home, v);

  }

  {
    sip_call_info_t *ci, *ci0;

    TEST_1(ci = sip_call_info_make(home,
				   "<http://www.nokia.com>;purpose=info"));
    TEST_S(ci->ci_purpose, "info");
    TEST_1(ci0 = sip_call_info_dup(home, ci));
    TEST_S(ci0->ci_purpose, "info");
    TEST_1(ci->ci_purpose != ci0->ci_purpose);

    TEST(msg_header_remove_param(ci->ci_common, "purpose"), 1);
    TEST_P(ci->ci_purpose, NULL);

    su_free(home, ci);
    su_free(home, ci0);
  }

  {
    sip_alert_info_t *ai, *ai0;

    TEST_1(ai = sip_alert_info_make(home, "<http://www.nokia.com/ringtone.mp3>;x-format=mp3"));
    TEST_1(ai0 = sip_alert_info_dup(home, ai));

    TEST(msg_header_remove_param(ai->ai_common, "x-format"), 1);
    TEST(msg_header_remove_param(ai0->ai_common, "x-format"), 1);

    su_free(home, ai);
    su_free(home, ai0);
  }

  {
    sip_reply_to_t *rplyto, *rplyto0;

    TEST_1(rplyto = sip_reply_to_make(home, "sip:joe@bar"));
    TEST_1(msg_header_add_param(home, (msg_common_t *)rplyto, "x-extra=extra") == 0);
    while (rplyto->rplyto_params && rplyto->rplyto_params[0])
      msg_header_remove_param(rplyto->rplyto_common, rplyto->rplyto_params[0]);
    su_free(home, rplyto);

    TEST_1(!sip_reply_to_make(home, (void *)"sip:joe@[baa"));

    TEST_1(rplyto = sip_reply_to_make(home, (void *)"sip:joe@bar"));
    su_free(home, rplyto);

    TEST_1(rplyto = sip_reply_to_make(home, (void *)"Joe <sip:joe@bar;user=ip>;x-extra=extra"));
    TEST_1(rplyto0 = sip_reply_to_dup(home, rplyto));
    su_free(home, rplyto);
    su_free(home, rplyto0);
  }


  su_home_check(home);
  su_home_zap(home);

  END();
}

int test_sip_msg_class(msg_mclass_t const *mc)
{
  int i, j, N;
  msg_hclass_t *hc;

  BEGIN();

  N = mc->mc_hash_size;

  /* check hashes */
  for (i = 0; i < N; i++) {
    if (!(hc = mc->mc_hash[i].hr_class))
      continue;
    for (j = i + 1; j < N; j++) {
      if (!mc->mc_hash[j].hr_class)
	continue;
      if (hc->hc_hash == mc->mc_hash[j].hr_class->hc_hash) {
	fprintf(stderr, "\t%s and %s have same hash\n",
		hc->hc_name, mc->mc_hash[j].hr_class->hc_name);
	return 1;
      }
    }
  }

  /* Check parser table sanity */
  for (i = 0; i < N; i++) {
    /* Verify each header entry */
    hc = mc->mc_hash[i].hr_class;

    if (hc == NULL)
      continue;

    /* Short form */
    if (hc->hc_short[0])
      TEST_P(mc->mc_short[hc->hc_short[0] - 'a'].hr_class, hc);

    /* Long form */
    j = msg_header_name_hash(hc->hc_name, NULL);
    TEST(j, hc->hc_hash);

    for (j = MC_HASH(hc->hc_name, N); j != i; j = (j + 1) % N)
      TEST_1(mc->mc_hash[j].hr_class);

  }

  END();
}

msg_t *read_message(int flags, char const buffer[])
{
  size_t n;
  int m;
  msg_t *msg;
  msg_iovec_t iovec[2];

  n = strlen(buffer);
  if (n == 0)
    return NULL;

  msg = msg_create(test_mclass, flags);
  if (msg_recv_iovec(msg, iovec, 2, n, 1) < 0) {
    perror("msg_recv_iovec");
  }
  memcpy(iovec->mv_base, buffer, n);
  msg_recv_commit(msg, n, 1);

  m = msg_extract(msg);

  return msg;
}

static int test_encoding(void)
{
  msg_header_t *h, *h1;
  msg_common_t *c;
  msg_t *msg;
  sip_t *sip;
  su_home_t *home;

  BEGIN();

  TEST_1(home = su_home_new(sizeof *home));

  msg = read_message(MSG_DO_EXTRACT_COPY,
    "SUBSCRIBE sip:foo@bar SIP/2.0\r\n"
    "To: Joe User <sip:foo@bar>\r\n"
    "From: \"Bar Owner\" <sip:bar@foo>;tag=foobar\r\n"
    "P-Asserted-Identity: <sip:bar@foo>\r\n"
    "P-Preferred-Identity: <sip:bar-owner@foo>\r\n"
    "Call-ID: 0ha0isndaksdj@10.1.2.3\r\n"
    "CSeq: 8 SUBSCRIBE\r\n"
    "Via: SIP/2.0/UDP 135.180.130.133\r\n"
    "Extension-Header: extended, more\r\n"
    "Reason: Q.850;cause=16;text=\"Terminated\"\r\n"
    "Contact: <sip:bar@pc.foo:5060>\r\n"
    "Date: Wed, 25 Mar 2004 14:49:29 GMT\r\n"
    "Max-Forwards: 80\r\n"
    "Min-Expires: 30\r\n"
    "Retry-After: 48 (this is a comment) ;duration=321\r\n"
    "Route: <sip:proxy.bar;maddr=172.21.40.40>\r\n"
    "Request-Disposition: proxy\r\n"
    "Accept-Contact: *;audio\r\n"
    "Reject-Contact: *;video\r\n"
    "Expires: 1200\r\n"
    "Event: presence;id=1\r\n"
    "In-Reply-To: {0h!a0i\"sndaksdj}@[kjsafi3], {0h!a0i\"snj}@[kjsfi3]\r\n"
    "Organization: Nuoret Banaani-Kotkat y.r.\r\n"
    "Priority: urgent\r\n"
    "Subject: ynk\r\n"
    "Timestamp: 3289129810.798259\r\n"
    "SIP-If-Match: foobar\r\n"
    "Proxy-Requires: prefs\r\n"
    "Supported: vnd.nokia\r\n"
    "User-Agent: Unknown Subscriber (1.0) Tonto (2.0)\r\n"
    "Accept: application/pidf+xml;version=1.0\r\n"
    "Accept-Encoding: gzip\r\n"
     /* Test loop below cannot encode multiple Accept-Language on one line */
    "Accept-Language: "/* "fi, "*/"en;q=0.2\r\n"
    "RAck: 421413 214214 INVITE\r\n"
    "Referred-By: <sips:bob@biloxi.example.com>\r\n"
    "Replaces: 12345601@atlanta.example.com;from-tag=314159;to-tag=1234567\r\n"
    "Authorization: Digest realm=\"foo\"\r\n"
    "Proxy-Authorization: Digest realm=\"foo\"\r\n"
    "Security-Client: tls\r\n"
    "Security-Verify: tls;q=0.2\r\n"
    "Privacy: none\r\n"
    "Content-Length: 7\r\n"
    "Content-Encoding: gzip, deflate, identity\r\n"
    "Content-Disposition: filter\r\n"
    "Content-Language: fi\r\n"
    "MIME-Version: 1.0\r\n"
    "Min-SE: 123\r\n"
    "Session-Expires: 1200\r\n"
    "Content-Type: text/plain\r\n"
    "Refer-Sub: true\r\n"
    "Suppress-Body-If-Match: humppa\r\n"
    "Suppress-Notify-If-Match: zumppa\r\n"
    "\r\n"
    "Heippa!");
  sip = sip_object(msg);

  TEST_1(msg); TEST_1(sip); TEST_1(!sip->sip_error);

  for (h = (msg_header_t *)sip->sip_request; h; h = h->sh_succ) {
    char b[80];
    size_t n;

    if (h == (msg_header_t*)sip->sip_payload)
      break;

    TEST_1(h1 = msg_header_dup(home, h));
    n = msg_header_e(b, sizeof b, h1, 0);
    TEST_SIZE(n, h->sh_len);
    TEST_M(b, h->sh_data, n);
    su_free(home, h1);
  }

  msg_destroy(msg), msg = NULL;

  /* Note: this should be canonic! */
  msg = read_message(MSG_DO_EXTRACT_COPY,
    "SIP/2.0 200 Ok\r\n"
    "To: Joe User <sip:foo@bar>;tag=deadbeef\r\n"
    "From: sip:bar@foo;tag=foobar\r\n"
    "Call-ID: {0h!a0i\"sndaksdj}@[kjsafi3]\r\n"
    "CSeq: 8912734 SUBSCRIBE\r\n"
    "Via: SIP/2.0/UDP 135.180.130.133\r\n"
    "Extension-Header: extended, more\r\n"
    "Reason: SIP;cause=400;text=\"Bad Message\"\r\n"
    "Contact: <sip:bar@pc.foo:5060>;audio\r\n"
    "Date: Wed, 25 Mar 2004 14:49:29 GMT\r\n"
    "Max-Forwards: 80\r\n"
    "Min-Expires: 30\r\n"
    "Expires: Wed, 25 Mar 2004 15:49:29 GMT\r\n"
    "Retry-After: 48;duration=321\r\n"
    "Record-Route: <sip:record-route@proxy.bar;maddr=172.21.40.40>\r\n"
    "Event: presence;id=1\r\n"
    "Allow-Events: presence, presence.winfo\r\n"
    "Subscription-State: active;expires=1800\r\n"
    "Call-Info: <http://www.bar.com/xcap/joe/>;purpose=xcap\r\n"
    "Error-Info: <http://www.bar.com/xcap/joe/errors>;param=xcap\r\n"
    "Server: None\r\n"
    "Timestamp: 3289129810.798259 0.084054\r\n"
    "SIP-ETag: foobar\r\n"
    "SIP-If-Match: foobar\r\n"
    "Requires: vnd.nokia\r\n"
    "Unsupported: vnd.nokia.pic\r\n"
    "Accept-Disposition: filter\r\n"
    "Warning: 399 presence.bar:5060 \"Unimplemented filter\"\r\n"
    "RSeq: 421414\r\n"
    "Refer-To: <sip:hsdf@cdwf.xcfw.com?Subject=test&Organization=Bar>\r\n"
    "Alert-Info: <http://alert.example.org/test.mp3>\r\n"
    "Reply-To: Bob <sip:bob@example.com>\r\n"
    "WWW-Authenticate: Digest realm=\"foo\"\r\n"
    "Proxy-Authenticate: Digest realm=\"foo\"\r\n"
    "Security-Server: tls;q=0.2\r\n"
    "Session-Expires: 1200;refresher=uac\r\n"
    "Content-Length: 7\r\n"
    "Content-Type: text/plain;charset=iso8859-1\r\n"
    "\r\n"
    "Heippa!");
  sip = sip_object(msg);

  TEST_1(msg); TEST_1(sip); TEST_1(!sip->sip_error);

  for (h = (msg_header_t *)sip->sip_status; h; h = h->sh_succ) {
    char b[80];
    size_t n;

    if (h == (sip_header_t*)sip->sip_payload)
      break;

    TEST_1(h1 = sip_header_dup(home, h));
    n = sip_header_e(b, sizeof b, h1, 0);
    TEST_SIZE(n, h->sh_len);
    TEST_M(b, h->sh_data, n);
    su_free(home, h1);
  }

  TEST_1(sip->sip_etag);
  TEST_S(sip->sip_etag->g_value, "foobar");
  TEST_1(sip->sip_if_match);

  msg_destroy(msg), msg = NULL;

  su_home_check(home);
  su_home_zap(home);

  msg = read_message(0,
		     "SIP/2.0 200 Ok\r\n"
		     "Via: SIP/2.0/UDP 135.180.130.133\r\n"
		     "Via: SIP/2.0/UDP 135.180.130.130:5060\r\n"
		     "To: Joe User <sip:foo@bar>;tag=deadbeef\r\n"
		     "From: sip:bar@foo;tag=foobar\r\n"
		     "Call-ID: {0h!a0i\"sndaksdj}@[kjsafi3]\r\n"
		     "CSeq: 8912734 SUBSCRIBE\r\n"
		     "Record-Route: <sip:135.180.130.133;lr>\r\n"
		     "Record-Route: <sip:135.180.130.130;lr>\r\n"
		     "Content-Length: 0\r\n"
		     "\r\n");

  sip = sip_object(msg);

  TEST_1(msg); TEST_1(sip); TEST_1(!sip->sip_error);

  sip->sip_flags |= MSG_FLG_COMPACT;

  TEST_1(msg_prepare(msg) != 0);

  TEST_1(c = sip->sip_status->st_common);
  TEST_M(c->h_data, "SIP/2.0 200 Ok\r\n", c->h_len);

  TEST_1(c = sip->sip_to->a_common);
  TEST_M(c->h_data, "t:Joe User<sip:foo@bar>;tag=deadbeef\r\n", c->h_len);

  TEST_1(c = sip->sip_from->a_common);
  TEST_M(c->h_data, "f:sip:bar@foo;tag=foobar\r\n", c->h_len);

  TEST_1(c = sip->sip_call_id->i_common);
  TEST_M(c->h_data, "i:{0h!a0i\"sndaksdj}@[kjsafi3]\r\n", c->h_len);

  TEST_1(c = sip->sip_cseq->cs_common);
  TEST_M(c->h_data, "CSeq:8912734 SUBSCRIBE\r\n", c->h_len);

  TEST_1(c = sip->sip_via->v_common);
  TEST_M(c->h_data, "v:SIP/2.0/UDP 135.180.130.133,SIP/2.0/UDP 135.180.130.130:5060\r\n", c->h_len);

  TEST_1(c = sip->sip_via->v_next->v_common);
  TEST_SIZE(c->h_len, 0); TEST_1(c->h_data);

  TEST_1(c = sip->sip_record_route->r_common);
  TEST_M(c->h_data, "Record-Route:<sip:135.180.130.133;lr>,<sip:135.180.130.130;lr>\r\n", c->h_len);

  TEST_1(c = sip->sip_record_route->r_next->r_common);
  TEST_SIZE(c->h_len, 0); TEST_1(c->h_data);

  TEST_1(c = sip->sip_content_length->l_common);
  TEST_M(c->h_data, "l:0\r\n", c->h_len);

  END();
}

#define XTRA(xtra, h) SU_ALIGN(xtra) + sip_header_size((sip_header_t*)h)

/** Test header filtering and duplicating */
int tag_test(void)
{
  su_home_t *home = su_home_new(sizeof(*home));
  sip_request_t *request =
    sip_request_make(home, "INVITE sip:joe@example.com SIP/2.0");
  sip_to_t *to = sip_to_make(home,
			     "Joe User <sip:joe.user@example.com;param=1>"
			     ";tag=12345678");
  sip_via_t *via = sip_via_make(home,
				"SIP/2.0/UDP sip.example.com"
				";maddr=128.12.9.254"
				";branch=289412978y641.321312");
  url_t *url = url_hdup(home,
    (url_t *)"sip:test:pass@example.com;baz=1?foo&bar");

  tagi_t *lst, *dup;
  size_t xtra;
  tag_value_t v;

  BEGIN();

  su_home_check(home);

  TEST_1(home && request && to && via);

  lst = tl_list(SIPTAG_REQUEST(request),
		SIPTAG_TO(to),
		SIPTAG_VIA(via),
		URLTAG_URL(url),
		TAG_NULL());

  xtra = 0;
  xtra += XTRA(xtra, request);
  xtra += XTRA(xtra, to);
  xtra += XTRA(xtra, via);
  xtra += SU_ALIGN(xtra) + sizeof(*url) + url_xtra(url);

  TEST_SIZE(tl_len(lst), 5 * sizeof(tagi_t));
  TEST_SIZE(tl_xtra(lst, 0), xtra);

  dup = tl_adup(NULL, lst);

  TEST(dup != NULL, 1);
  TEST_SIZE(tl_len(dup), 5 * sizeof(tagi_t));
  TEST_SIZE(tl_xtra(dup, 0), xtra);

  if (tstflags & tst_verbatim)
    tl_print(stdout, "dup:\n", dup);

  su_free(NULL, dup);
  tl_vfree(lst);

  TEST_1(t_scan(siptag_request, home, "INVITE sip:example.org SIP/2.0", &v));
  TEST_1(request = (void *)v);
  TEST_1(request->rq_common->h_class == sip_request_class);
  TEST_S(request->rq_method_name, "INVITE");
  TEST_S(request->rq_version, "SIP/2.0");

  TEST_1(t_scan(siptag_to, home, "Example <sip:example.org>;tag=foo", &v));
  TEST_1(to = (void *)v);
  TEST_1(to->a_common->h_class == sip_to_class);
  TEST_S(to->a_display, "Example");
  TEST_S(to->a_tag, "foo");

  su_home_check(home);
  su_home_zap(home);

  END();
}

/** Test advanced tag features */
static int parser_tag_test(void)
{
  tagi_t *lst, *dup, *filter1, *filter2, *filter3, *filter4;
  tagi_t *b1, *b2, *b3, *b4;

  msg_t *msg;
  sip_t *sip;
  su_home_t *home;
  size_t xtra;

  BEGIN();

  home = su_home_new(sizeof *home);

  msg = read_message(MSG_DO_EXTRACT_COPY,
"SIP/2.0 401 Unauthorized\r\n"
"Via: SIP/2.0/UDP srlab.sr.ntc.nokia.com:5060;maddr=192.168.102.5\r\n"
"Via: SIP/2.0/UDP 172.21.9.155\r\n"
"Record-Route: <sip:garage.sr.ntc.nokia.com:5060;maddr=srlab.sr.ntc.nokia.com>\r\n"
"From: sip:digest@garage.sr.ntc.nokia.com\r\n"
"To: sip:digest@garage.sr.ntc.nokia.com\r\n"
"Call-ID: 982773899-reg@172.21.9.155\r\n"
"CSeq: 1 REGISTER\r\n"
"WWW-Authenticate: Digest realm=\"garage.sr.ntc.nokia.com\",\r\n"
"  nonce=\"MjAwMS0wMS0yMSAxNTowODo1OA==\", algorithm=MD5, qop=\"auth\"\r\n"
"Proxy-Authenticate: Digest realm=\"IndigoSw\", domain=\"sip:indigosw.com\", "
"nonce=\"V2VkIEF1ZyAxNSAxODoxMzozMiBCU1QgMjAwMVtCQDJkYjE5ZA==\", "
"opaque=\"NzA3ZjJhYzU4MGY3MzU0MQ==\", stale=false, "
"algorithm=md5, algorithm=sha1, qop=\"auth\"\r\n"
/* , qop=\"auth, auth-int\"\r */
"\r\n");

  sip = sip_object(msg);

  TEST_1(home && msg && sip);
  TEST_1(sip->sip_size >= sizeof *sip);

  TEST_1(sip_is_status((sip_header_t *)sip->sip_status));
  TEST_1(sip_is_via((sip_header_t *)sip->sip_via));
  TEST_1(sip_is_via((sip_header_t *)sip->sip_via->v_next));
  TEST_1(sip_is_record_route((sip_header_t *)sip->sip_record_route));
  TEST_1(sip_is_from((sip_header_t *)sip->sip_from));
  TEST_1(sip_is_to((sip_header_t *)sip->sip_to));
  TEST_1(sip_is_call_id((sip_header_t *)sip->sip_call_id));
  TEST_1(sip_is_cseq((sip_header_t *)sip->sip_cseq));
  TEST_1(sip_is_www_authenticate(
    (sip_header_t *)sip->sip_www_authenticate));

  TEST_1(sip_complete_message(msg) == 0);

  TEST_1(sip_is_content_length((sip_header_t *)sip->sip_content_length));

  TEST_P(sip->sip_content_length->l_common->h_succ, sip->sip_separator);

  lst = tl_list(SIPTAG_VIA(sip->sip_via),
		SIPTAG_RECORD_ROUTE(sip->sip_record_route),
		TAG_SKIP(2),
		SIPTAG_CSEQ(sip->sip_cseq),
		SIPTAG_PAYLOAD(sip->sip_payload),
		TAG_NULL());
  filter1 = tl_list(SIPTAG_VIA(0),
		    TAG_NULL());
  filter2 = tl_list(SIPTAG_CALL_ID(0),
		    SIPTAG_FROM(0),
		    SIPTAG_ROUTE(0),
		    SIPTAG_CSEQ(0),
		    TAG_NULL());
  filter3 = tl_list(SIPTAG_CSEQ(0),
		    SIPTAG_CONTENT_LENGTH(0),
		    TAG_NULL());
  filter4 = tl_list(SIPTAG_STATUS(0),
		    SIPTAG_VIA(0),
		    SIPTAG_RECORD_ROUTE(0),
		    SIPTAG_FROM(0),
		    SIPTAG_TO(0),
		    SIPTAG_CALL_ID(0),
		    SIPTAG_CSEQ(0),
		    SIPTAG_WWW_AUTHENTICATE(0),
		    SIPTAG_PROXY_AUTHENTICATE(0),
		    SIPTAG_CONTENT_LENGTH(0),
		    TAG_NULL());

  TEST_1(lst && filter1 && filter2 && filter3 && filter4);

  b1 = tl_afilter(home, filter1, lst);
  TEST_SIZE(tl_len(b1), 2 * sizeof(tagi_t));
  TEST_1(((sip_via_t *)b1->t_value)->v_next);
  xtra = sip_header_size((sip_header_t *)sip->sip_via);
  xtra += SU_ALIGN(xtra);
  xtra += sip_header_size((sip_header_t *)sip->sip_via->v_next);
  TEST_SIZE(tl_xtra(b1, 0), xtra);

  dup = tl_adup(home, lst);

  TEST_SIZE(tl_len(dup), tl_len(lst));
  TEST_SIZE(tl_xtra(dup, 0), tl_xtra(lst, 0));

  tl_vfree(lst);

  lst = tl_list(SIPTAG_SIP(sip), TAG_NULL());

  b2 = tl_afilter(home, filter2, lst);
  TEST_SIZE(tl_len(b2), 4 * sizeof(tagi_t));
  xtra = 0;
  xtra += XTRA(xtra, sip->sip_call_id);
  xtra += XTRA(xtra, sip->sip_from);
  xtra += XTRA(xtra, sip->sip_cseq);
  TEST_SIZE(tl_xtra(b2, 0), xtra);

  b3 = tl_afilter(home, filter3, lst);

  TEST_SIZE(tl_len(b3), 3 * sizeof(tagi_t));
  TEST_SIZE(tl_xtra(b3, 0),
	    sizeof(sip_content_length_t) + sizeof(sip_cseq_t));

  b4 = tl_afilter(home, filter4, lst);
  TEST_SIZE(tl_len(b4), 11 * sizeof(tagi_t));
  xtra = 0;
  xtra += XTRA(xtra, sip->sip_status);
  xtra += XTRA(xtra, sip->sip_via);
  xtra += XTRA(xtra, sip->sip_via->v_next);
  xtra += XTRA(xtra, sip->sip_record_route);
  xtra += XTRA(xtra, sip->sip_from);
  xtra += XTRA(xtra, sip->sip_to);
  xtra += XTRA(xtra, sip->sip_call_id);
  xtra += XTRA(xtra, sip->sip_cseq);
  xtra += XTRA(xtra, sip->sip_www_authenticate);
  xtra += XTRA(xtra, sip->sip_proxy_authenticate);
  xtra += XTRA(xtra, sip->sip_content_length);
  TEST_SIZE(tl_xtra(b4, 0), xtra);

  tl_vfree(filter1); tl_vfree(filter2); tl_vfree(filter3); tl_vfree(filter4);
  tl_vfree(lst);

  su_home_check(home);

  su_free(home, b4);
  su_free(home, b3);
  su_free(home, b2);
  su_free(home, dup);
  su_free(home, b1);

  su_home_check(home);

  su_home_unref(home);

  msg_destroy(msg);

  END();
}

/** Test error messages */
static int response_phrase_test(void)
{
  BEGIN();
  {
    struct { int status; char const *phrase; } const errors[] =
      {
	{ SIP_100_TRYING },
	{ SIP_180_RINGING },
	{ SIP_181_CALL_IS_BEING_FORWARDED },
       	{ SIP_182_QUEUED },
       	{ SIP_183_SESSION_PROGRESS },
       	{ SIP_200_OK },
       	{ SIP_202_ACCEPTED },
       	{ SIP_300_MULTIPLE_CHOICES },
       	{ SIP_301_MOVED_PERMANENTLY },
       	{ SIP_302_MOVED_TEMPORARILY },
       	{ SIP_305_USE_PROXY },
       	{ SIP_380_ALTERNATIVE_SERVICE },
       	{ SIP_400_BAD_REQUEST },
       	{ SIP_401_UNAUTHORIZED },
       	{ SIP_402_PAYMENT_REQUIRED },
       	{ SIP_403_FORBIDDEN },
       	{ SIP_404_NOT_FOUND },
       	{ SIP_405_METHOD_NOT_ALLOWED },
       	{ SIP_406_NOT_ACCEPTABLE },
       	{ SIP_407_PROXY_AUTH_REQUIRED },
       	{ SIP_408_REQUEST_TIMEOUT },
       	{ SIP_409_CONFLICT },
       	{ SIP_410_GONE },
       	{ SIP_411_LENGTH_REQUIRED },
       	{ SIP_413_REQUEST_TOO_LARGE },
       	{ SIP_414_REQUEST_URI_TOO_LONG },
       	{ SIP_415_UNSUPPORTED_MEDIA },
       	{ SIP_416_UNSUPPORTED_URI },
       	{ SIP_420_BAD_EXTENSION },
       	{ SIP_421_EXTENSION_REQUIRED },
       	{ SIP_422_SESSION_TIMER_TOO_SMALL },
       	{ SIP_423_INTERVAL_TOO_BRIEF },
       	{ SIP_423_REGISTRATION_TOO_BRIEF },
       	{ SIP_480_TEMPORARILY_UNAVAILABLE },
       	{ SIP_481_NO_TRANSACTION },
       	{ SIP_481_NO_CALL },
       	{ SIP_482_LOOP_DETECTED },
       	{ SIP_483_TOO_MANY_HOPS },
       	{ SIP_484_ADDRESS_INCOMPLETE },
       	{ SIP_485_AMBIGUOUS },
       	{ SIP_486_BUSY_HERE },
       	{ SIP_487_REQUEST_TERMINATED },
       	{ SIP_487_REQUEST_CANCELLED },
       	{ SIP_488_NOT_ACCEPTABLE },
       	{ SIP_489_BAD_EVENT },
       	{ SIP_491_REQUEST_PENDING },
       	{ SIP_493_UNDECIPHERABLE },
       	{ SIP_500_INTERNAL_SERVER_ERROR },
       	{ SIP_501_NOT_IMPLEMENTED },
       	{ SIP_502_BAD_GATEWAY },
       	{ SIP_503_SERVICE_UNAVAILABLE },
       	{ SIP_504_GATEWAY_TIME_OUT },
       	{ SIP_505_VERSION_NOT_SUPPORTED },
       	{ SIP_513_MESSAGE_TOO_LARGE },
       	{ SIP_600_BUSY_EVERYWHERE },
       	{ SIP_603_DECLINE },
       	{ SIP_604_DOES_NOT_EXIST_ANYWHERE },
       	{ SIP_606_NOT_ACCEPTABLE },
	{ SIP_607_UNWANTED },
	{ 0, NULL }
      };
    int i;

    for (i = 0; errors[i].status; i++)
      TEST_S(errors[i].phrase, sip_status_phrase(errors[i].status));
  }
  END();
}

/** Test parser and header manipulation */
static int parser_test(void)
{
  msg_t *msg;
  sip_t *sip;
  su_home_t *home;

  sip_route_t *r;

  sip_request_t        sip_request[1] = { SIP_REQUEST_INIT() };
  sip_status_t         sip_status[1]  = { SIP_STATUS_INIT() };
  sip_header_t         sip_unknown[1]  = { SIP_UNKNOWN_INIT() };
  sip_separator_t      sip_separator[1] = { SIP_SEPARATOR_INIT() };
  sip_payload_t        sip_payload[1] = { SIP_PAYLOAD_INIT() };
  sip_via_t            sip_via[1] = { SIP_VIA_INIT() };
  sip_route_t          sip_route[1] = { SIP_ROUTE_INIT() };
  sip_record_route_t   sip_record_route[1] = { SIP_RECORD_ROUTE_INIT() };
  sip_max_forwards_t   sip_max_forwards[1] = { SIP_MAX_FORWARDS_INIT() };
  sip_from_t           sip_from[1] = { SIP_FROM_INIT() };
  sip_to_t             sip_to[1] = { SIP_TO_INIT() };
  sip_call_id_t        sip_call_id[1] = { SIP_CALL_ID_INIT() };
  sip_cseq_t           sip_cseq[1] = { SIP_CSEQ_INIT() };
  sip_contact_t        sip_contact[1] = { SIP_CONTACT_INIT() };

  sip_expires_t        sip_expires[1] = { SIP_EXPIRES_INIT() };
  sip_date_t           sip_date[1] = { SIP_DATE_INIT() };
  sip_retry_after_t    sip_retry_after[1] = { SIP_RETRY_AFTER_INIT() };
  sip_timestamp_t      sip_timestamp[1] = { SIP_TIMESTAMP_INIT() };
  sip_subject_t        sip_subject[1] = { SIP_SUBJECT_INIT() };
  sip_priority_t       sip_priority[1] = { SIP_PRIORITY_INIT() };

  sip_call_info_t      sip_call_info[1] = { SIP_CALL_INFO_INIT() };
  sip_organization_t   sip_organization[1] = { SIP_ORGANIZATION_INIT() };
  sip_server_t         sip_server[1] = { SIP_SERVER_INIT() };
  sip_user_agent_t     sip_user_agent[1] = { SIP_USER_AGENT_INIT() };
  sip_in_reply_to_t    sip_in_reply_to[1] = { SIP_IN_REPLY_TO_INIT() };

  sip_accept_t         sip_accept[1] = { SIP_ACCEPT_INIT() };
  sip_accept_encoding_t sip_accept_encoding[1] = { SIP_ACCEPT_ENCODING_INIT() };
  sip_accept_language_t sip_accept_language[1] = { SIP_ACCEPT_LANGUAGE_INIT() };

  sip_session_expires_t sip_session_expires[1] = { SIP_SESSION_EXPIRES_INIT() };
  sip_min_se_t sip_min_se[1] = { SIP_MIN_SE_INIT() };

  sip_allow_t          sip_allow[1] = { SIP_ALLOW_INIT() };
  sip_require_t        sip_require[1] = { SIP_REQUIRE_INIT() };
  sip_proxy_require_t  sip_proxy_require[1] = { SIP_PROXY_REQUIRE_INIT() };
  sip_supported_t      sip_supported[1] = { SIP_SUPPORTED_INIT() };
  sip_unsupported_t    sip_unsupported[1] = { SIP_UNSUPPORTED_INIT() };
#if SIP_HAVE_ENCRYPTION
  sip_encryption_t     sip_encryption[1] = { SIP_ENCRYPTION_INIT() };
#endif
#if SIP_HAVE_RESPONSE_KEY
  sip_response_key_t   sip_response_key[1] = { SIP_RESPONSE_KEY_INIT() };
#endif

  sip_proxy_authenticate_t  sip_proxy_authenticate[1] = { SIP_PROXY_AUTHENTICATE_INIT() };
  sip_proxy_authorization_t sip_proxy_authorization[1] = { SIP_PROXY_AUTHORIZATION_INIT() };
  sip_authorization_t  sip_authorization[1] = { SIP_AUTHORIZATION_INIT() };
  sip_www_authenticate_t sip_www_authenticate[1] = { SIP_WWW_AUTHENTICATE_INIT() };
  sip_error_info_t     sip_error_info[1] = { SIP_ERROR_INFO_INIT() };
  sip_warning_t        sip_warning[1] = { SIP_WARNING_INIT() };

  sip_mime_version_t   sip_mime_version[1] = { SIP_MIME_VERSION_INIT() };
  sip_content_type_t   sip_content_type[1] = { SIP_CONTENT_TYPE_INIT() };
  sip_content_encoding_t sip_content_encoding[1] = { SIP_CONTENT_ENCODING_INIT() };
  sip_content_disposition_t sip_content_disposition[1] = { SIP_CONTENT_DISPOSITION_INIT() };
  sip_content_length_t sip_content_length[1] = { SIP_CONTENT_LENGTH_INIT() };

  BEGIN();

  home = su_home_new(sizeof *home);

  msg = read_message(MSG_DO_EXTRACT_COPY,
    "INVITE sip:John_Smith@tct.hut.fi SIP/2.0\r\n"
    "To: John Smith <sip:John_Smith@tct.hut.fi:5066;user=ip;maddr=131.228.16.2>\r\n"
    "  ; tag = deadbeef\r\n"
    "From: http://www.cs.columbia.edu\r\n"
    "Call-ID: 0ha0isndaksdj@10.1.2.3\r\n"
    "CSeq : 8 INVITE\r\n"
    "Via: SIP/2.0/UDP 135.180.130.133\r\n"
    "Route: <sip:1@a;lr>, sip:2@b;lr=2, <sip:3@c;lr=3>\r\n"
    "Route: <sip:1@d;lr=4>\r\n"
    "Route: sip:2@e;lr=5, <sip:3@f;lr=6>\r\n"
    "Route: <sip:1@g;lr=7>, <sip:2@h>;lr=8\r\n"
    "Content-Type: application/sdp\r\n"
    "Contact: Joe Bob Briggs <urn:ipaddr:122.1.2.3> ; bar=\"foo baa\", sip:kuik@foo.invalid\r\n"
    "Via: SIP/2.0/UDP [aa:bb::1]:5061\r\n"
    "\r\n"
    "v=0\r\n"
    "o=mhandley 29739 7272939 IN IP4 126.5.4.3\r\n"
    "c=IN IP4 135.180.130.88\r\n"
    "m=audio 492170 RTP/AVP 0 12\r\n"
    "m=video 3227 RTP/AVP 31\r\n"
    "a=rtpmap:31 LPC\r\n");

  sip = sip_object(msg);

  TEST_1(home && msg && sip);

  TEST_1(sip_is_request((sip_header_t *)sip->sip_request));
  TEST_1(sip->sip_via); TEST_1(sip->sip_via->v_next);
  TEST_1(sip->sip_via->v_next->v_next == NULL);
  TEST_1(sip_sanity_check(sip) == 0);

  TEST_1(r = sip->sip_route); TEST_1(r->r_common->h_data);
  TEST_1(r = r->r_next); TEST_1(r->r_common->h_data);
  TEST_1(r = r->r_next); TEST_1(r->r_common->h_data);
  TEST_1(r = r->r_next); TEST_1(r->r_common->h_data);
  TEST_1(r = r->r_next); TEST_1(r->r_common->h_data);
  TEST_1(r = r->r_next); TEST_1(r->r_common->h_data);
  TEST_1(r = r->r_next); TEST_1(r->r_common->h_data);
  TEST_1(r = r->r_next); TEST_1(r->r_common->h_data);
  TEST_1(!r->r_next);

  TEST_1(r = sip_route_fix(sip->sip_route)); TEST_1(!r->r_common->h_data);
  TEST_1(r = r->r_next); TEST_1(!r->r_common->h_data);
  TEST_1(r = r->r_next); TEST_1(!r->r_common->h_data);
  TEST_1(r = r->r_next); TEST_1(r->r_common->h_data);
  TEST_1(r = r->r_next); TEST_1(!r->r_common->h_data);
  TEST_1(r = r->r_next); TEST_1(!r->r_common->h_data);
  TEST_1(r = r->r_next); TEST_1(!r->r_common->h_data);
  TEST_1(r = r->r_next); TEST_1(!r->r_common->h_data);
  TEST_1(!r->r_next);

  /* Quiet lots of warnings */
  #define _msg_header_offset msg_header_offset
  #define msg_header_offset(msg, sip, h) \
    _msg_header_offset(msg, (msg_pub_t *)sip, (msg_header_t *)h)

  TEST_P(msg_header_offset(msg, sip, sip_request), &sip->sip_request);
  TEST_P(msg_header_offset(msg, sip, sip_status), &sip->sip_status);
  TEST_P(msg_header_offset(msg, sip, sip_unknown), &sip->sip_unknown);
  TEST_P(msg_header_offset(msg, sip, sip_separator), &sip->sip_separator);
  TEST_P(msg_header_offset(msg, sip, sip_payload), &sip->sip_payload);
  TEST_P(msg_header_offset(msg, sip, sip_via), &sip->sip_via);
  TEST_P(msg_header_offset(msg, sip, sip_route), &sip->sip_route);
  TEST_P(msg_header_offset(msg, sip, sip_record_route),
	 &sip->sip_record_route);
  TEST_P(msg_header_offset(msg, sip, sip_max_forwards),
	 &sip->sip_max_forwards);
  TEST_P(msg_header_offset(msg, sip, sip_from), &sip->sip_from);
  TEST_P(msg_header_offset(msg, sip, sip_to), &sip->sip_to);
  TEST_P(msg_header_offset(msg, sip, sip_call_id), &sip->sip_call_id);
  TEST_P(msg_header_offset(msg, sip, sip_cseq), &sip->sip_cseq);
  TEST_P(msg_header_offset(msg, sip, sip_contact), &sip->sip_contact);

  TEST_P(msg_header_offset(msg, sip, sip_expires), &sip->sip_expires);
  TEST_P(msg_header_offset(msg, sip, sip_date), &sip->sip_date);
  TEST_P(msg_header_offset(msg, sip, sip_retry_after), &sip->sip_retry_after);
  TEST_P(msg_header_offset(msg, sip, sip_timestamp), &sip->sip_timestamp);
  TEST_P(msg_header_offset(msg, sip, sip_subject), &sip->sip_subject);
  TEST_P(msg_header_offset(msg, sip, sip_priority), &sip->sip_priority);

  TEST_P(msg_header_offset(msg, sip, sip_call_info), &sip->sip_call_info);
  TEST_P(msg_header_offset(msg, sip, sip_organization),
	 &sip->sip_organization);
  TEST_P(msg_header_offset(msg, sip, sip_server), &sip->sip_server);
  TEST_P(msg_header_offset(msg, sip, sip_user_agent), &sip->sip_user_agent);
  TEST_P(msg_header_offset(msg, sip, sip_in_reply_to), &sip->sip_in_reply_to);

  TEST_P(msg_header_offset(msg, sip, sip_accept), &sip->sip_accept);
  TEST_P(msg_header_offset(msg, sip, sip_accept_encoding),
	 &sip->sip_accept_encoding);
  TEST_P(msg_header_offset(msg, sip, sip_accept_language),
	 &sip->sip_accept_language);

  TEST_P(msg_header_offset(msg, sip, sip_session_expires),
	 &sip->sip_session_expires);
  TEST_P(msg_header_offset(msg, sip, sip_min_se), &sip->sip_min_se);

  TEST_P(msg_header_offset(msg, sip, sip_allow), &sip->sip_allow);
  TEST_P(msg_header_offset(msg, sip, sip_require), &sip->sip_require);
  TEST_P(msg_header_offset(msg, sip, sip_proxy_require),
	 &sip->sip_proxy_require);
  TEST_P(msg_header_offset(msg, sip, sip_supported), &sip->sip_supported);
  TEST_P(msg_header_offset(msg, sip, sip_unsupported), &sip->sip_unsupported);
#if SIP_HAVE_ENCRYPTION
  TEST(msg_header_offset(msg, sip, sip_encryption), &sip->sip_encryption);
#endif
#if SIP_HAVE_RESPONSE_KEY
  TEST(msg_header_offset(msg, sip, sip_response_key), &sip->sip_response_key);
#endif

  TEST_P(msg_header_offset(msg, sip, sip_proxy_authenticate),
	 &sip->sip_proxy_authenticate);
  TEST_P(msg_header_offset(msg, sip, sip_proxy_authorization),
	 &sip->sip_proxy_authorization);
  TEST_P(msg_header_offset(msg, sip, sip_authorization),
	 &sip->sip_authorization);
  TEST_P(msg_header_offset(msg, sip, sip_www_authenticate),
	 &sip->sip_www_authenticate);
  TEST_P(msg_header_offset(msg, sip, sip_error_info), &sip->sip_error_info);
  TEST_P(msg_header_offset(msg, sip, sip_warning), &sip->sip_warning);

  TEST_P(msg_header_offset(msg, sip, sip_mime_version), &sip->sip_mime_version);
  TEST_P(msg_header_offset(msg, sip, sip_content_type), &sip->sip_content_type);
  TEST_P(msg_header_offset(msg, sip, sip_content_encoding),
	 &sip->sip_content_encoding);
  TEST_P(msg_header_offset(msg, sip, sip_content_disposition),
	 &sip->sip_content_disposition);
  TEST_P(msg_header_offset(msg, sip, sip_content_length),
	 &sip->sip_content_length);

  TEST_SIZE(sip_request_class->hc_params, 0);
  TEST_SIZE(sip_status_class->hc_params, 0);
  TEST_SIZE(sip_unknown_class->hc_params, 0);
  TEST_SIZE(sip_separator_class->hc_params, 0);
  TEST_SIZE(sip_payload_class->hc_params, 0);
  TEST_SIZE(sip_via_class->hc_params, offsetof(sip_via_t, v_params));
  TEST_SIZE(sip_route_class->hc_params, offsetof(sip_route_t, r_params));
  TEST_SIZE(sip_record_route_class->hc_params,
	    offsetof(sip_record_route_t, r_params));

  TEST_SIZE(sip_max_forwards_class->hc_params, 0);
  TEST_SIZE(sip_from_class->hc_params, offsetof(sip_from_t, a_params));
  TEST_SIZE(sip_to_class->hc_params, offsetof(sip_to_t, a_params));
  TEST_SIZE(sip_call_id_class->hc_params, 0);
  TEST_SIZE(sip_cseq_class->hc_params, 0);
  TEST_SIZE(sip_contact_class->hc_params, offsetof(sip_contact_t, m_params));

  TEST_SIZE(sip_expires_class->hc_params, 0);
  TEST_SIZE(sip_date_class->hc_params, 0);
  TEST_SIZE(sip_retry_after_class->hc_params,
	    offsetof(sip_retry_after_t, af_params));
  TEST_SIZE(sip_timestamp_class->hc_params, 0);
  TEST_SIZE(sip_subject_class->hc_params, 0);
  TEST_SIZE(sip_priority_class->hc_params, 0);

  TEST_SIZE(sip_call_info_class->hc_params,
	    offsetof(sip_call_info_t, ci_params));
  TEST_SIZE(sip_organization_class->hc_params, 0);
  TEST_SIZE(sip_server_class->hc_params, 0);
  TEST_SIZE(sip_user_agent_class->hc_params, 0);

  TEST_SIZE(sip_in_reply_to_class->hc_params,
	    offsetof(sip_in_reply_to_t, k_items));
  TEST_SIZE(sip_accept_class->hc_params, offsetof(sip_accept_t, ac_params));
  TEST_SIZE(sip_accept_encoding_class->hc_params,
	    offsetof(sip_accept_encoding_t, aa_params));
  TEST_SIZE(sip_accept_language_class->hc_params,
	    offsetof(sip_accept_language_t, aa_params));

  TEST_SIZE(sip_session_expires_class->hc_params,
	    offsetof(sip_session_expires_t, x_params));
  TEST_SIZE(sip_min_se_class->hc_params, offsetof(sip_min_se_t, min_params));

  TEST_SIZE(sip_allow_class->hc_params, offsetof(sip_allow_t, k_items));
  TEST_SIZE(sip_require_class->hc_params, offsetof(sip_require_t, k_items));
  TEST_SIZE(sip_proxy_require_class->hc_params,
	    offsetof(sip_proxy_require_t, k_items));
  TEST_SIZE(sip_supported_class->hc_params,
	    offsetof(sip_supported_t, k_items));
  TEST_SIZE(sip_unsupported_class->hc_params,
	    offsetof(sip_unsupported_t, k_items));

#if SIP_HAVE_ENCRYPTION
  TEST_SIZE(sip_encryption_class->hc_params,
	    offsetof(sip_encryption_t, au_params));
#endif
#if SIP_HAVE_RESPONSE_KEY
  TEST_SIZE(sip_response_key_class->hc_params,
	    offsetof(sip_response_key_t, au_params));
#endif
  TEST_SIZE(sip_proxy_authenticate_class->hc_params,
	    offsetof(sip_proxy_authenticate_t, au_params));
  TEST_SIZE(sip_proxy_authorization_class->hc_params,
	    offsetof(sip_proxy_authorization_t, au_params));
  TEST_SIZE(sip_authorization_class->hc_params,
	    offsetof(sip_authorization_t, au_params));
  TEST_SIZE(sip_www_authenticate_class->hc_params,
	    offsetof(sip_www_authenticate_t, au_params));

  TEST_SIZE(sip_error_info_class->hc_params,
	    offsetof(sip_error_info_t, ei_params));
  TEST_SIZE(sip_alert_info_class->hc_params,
	    offsetof(sip_alert_info_t, ai_params));
  TEST_SIZE(sip_reply_to_class->hc_params,
	    offsetof(sip_reply_to_t, rplyto_params));
  TEST_SIZE(sip_warning_class->hc_params, 0);

  TEST_SIZE(sip_mime_version_class->hc_params, 0);
  TEST_SIZE(sip_content_type_class->hc_params,
	    offsetof(sip_content_type_t, c_params));
  TEST_SIZE(sip_content_encoding_class->hc_params,
	    offsetof(sip_content_encoding_t, k_items));
  TEST_SIZE(sip_content_disposition_class->hc_params,
	    offsetof(sip_content_disposition_t, cd_params));
  TEST_SIZE(sip_content_length_class->hc_params, 0);

  msg_destroy(msg);

  su_home_unref(home);

  END();
}

static int count(sip_common_t *h)
{
  sip_header_t *sh = (sip_header_t *)h;
  unsigned n;

  for (n = 0; sh; sh = sh->sh_next)
    n++;

  return n;
}

static int len(sip_common_t *h)
{
  sip_header_t *sh = (sip_header_t *)h;
  unsigned n;

  for (n = 0; sh; sh = sh->sh_next) {
    if (n) n +=2;
    n += sip_header_field_e(NULL, 0, sh, 0);
  }

  return n;
}

static int sip_header_test(void)
{
  msg_t *msg;
  sip_t *sip;
  su_home_t *home;
  void const *x;
  sip_via_t *v, *v0;
  tagi_t const *tl;
  tagi_t *tl0;

  BEGIN();

  home = su_home_new(sizeof *home);

  TEST_1(msg = read_message(MSG_DO_EXTRACT_COPY,
    "MESSAGE sip:John_Smith@tct.hut.fi SIP/2.0\r\n"
    "To: John Smith <sip:John_Smith@tct.hut.fi:5066;user=ip;maddr=131.228.16.2>\r\n"
    "  ; tag = deadbeef\r\n"
    "From:h<http://www.cs.columbia.edu>\r\n"
    "Call-ID: 0ha0isndaksdj@10.1.2.3\r\n"
    "CSeq : 8 MESSAGE\r\n"
    "Via: SIP/2.0/UDP 135.180.130.133;received=defa:daf::00:12\r\n"
    "Via: SIP/2.0/TCP 135.180.130.131;branch=deadbeef.barf;ttl=3;hidden,,"
    "SIP/2.0/UDP\r\n 135.180.130.131:5061;received=[defa::00:12]\r\n"
    "Contact: Joe Bob Briggs <urn:ipaddr:122.1.2.3> ; bar=\042foo baa\042, <sip:kuik@foo.invalid>, sip:barf\r\n"
    "Via: SIP/2.0/UDP [aa:bb::1]:5061\r\n"
    "Record-Route: Test Element <sip:[defa::00:12]:5061>;param=12+1\r\n"
    "Record-Route: sip:135.180.130.133,<sip:135.180.130.131;transport=tcp>,\r\n"
    "\t,Test Element <sip:[defa::00:12]:5061>;param=12+1\r\n"
    "Path: Test <sip:[defa::00:12]:5061>\r\n"
    "Service-Route: Test <sip:[defa::00:12]:5061>\r\n"
    "Route: ,\r\n"
    "Unknown-Extension: hip\r\n"
    "Hide: hop\r\n"
    "Max-Forwards: 12\r\n"
    "Min-Expires: 150\r\n"
    "Timestamp: 10.010 0.000100\r\n"
    "Suppress-Body-If-Match: humppa \t\r\n"
    "Suppress-Notify-If-Match: zumppa\r\n"
    " \r\n"
    "Content-Type: application/sdp\r\n"
    "\r\n"
    "v=0\r\n"
    "o=mhandley 29739 7272939 IN IP4 126.5.4.3\r\n"
    "c=IN IP4 135.180.130.88\r\n"
    "m=audio 492170 RTP/AVP 0 12\r\n"
    "m=video 3227 RTP/AVP 31\r\n"
    "a=rtpmap:31 LPC\r\n"));

  TEST_1(sip = sip_object(msg));

  TEST(count(sip->sip_request->rq_common), 1);
  TEST(count(sip->sip_to->a_common), 1);
  TEST(count(sip->sip_from->a_common), 1);
  TEST(count(sip->sip_cseq->cs_common), 1);
  TEST(count(sip->sip_call_id->i_common), 1);
  TEST(count(sip->sip_via->v_common), 4);
  TEST(count(sip->sip_contact->m_common), 3);
  TEST(count(sip->sip_content_type->c_common), 1);
  TEST(count(sip->sip_route->r_common), 0);
  TEST(count(sip->sip_record_route->r_common), 4);
#if SU_HAVE_EXPERIMENTAL
  TEST(count(sip->sip_unknown->un_common), 2);
#else
  TEST(count(sip->sip_unknown->un_common), 4);
#endif
  TEST(count(sip->sip_error->er_common), 1);
  TEST(count(sip->sip_max_forwards->mf_common), 1);
  TEST(count(sip->sip_min_expires->me_common), 1);
  TEST(count(sip->sip_timestamp->ts_common), 1);

  TEST_S(sip->sip_contact->m_display, "Joe Bob Briggs");
  TEST_1(sip->sip_contact->m_next->m_display != NULL);
  TEST_S(sip->sip_contact->m_next->m_display, "");
  TEST_1(sip->sip_contact->m_next->m_next->m_display == NULL);

  TEST(sip->sip_max_forwards->mf_count, 12);
  TEST(sip->sip_min_expires->me_delta, 150);

#if SU_HAVE_EXPERIMENTAL
  {
    sip_suppress_body_if_match_t *sbim;
    sip_suppress_notify_if_match_t *snim;

    TEST_1(sbim = sip_suppress_body_if_match(sip));
    TEST_S(sbim->sbim_tag, "humppa");

    TEST_SIZE(offsetof(msg_generic_t, g_value),
	      offsetof(sip_suppress_body_if_match_t, sbim_tag));

    TEST_1(snim = sip_suppress_notify_if_match(sip));
    TEST_S(snim->snim_tag, "zumppa");

    TEST_SIZE(offsetof(msg_generic_t, g_value),
	      offsetof(sip_suppress_notify_if_match_t, snim_tag));
  }
#endif

  TEST_1(sip->sip_from->a_display);
  TEST_S(sip->sip_from->a_display, "h");

  v0 = sip->sip_via;

  TEST_1(v = sip_via_copy(home, v0));
  TEST(len(v->v_common), len(v0->v_common));
  for (; v && v0; v = v->v_next, v0 = v0->v_next) {
    if (v->v_params)
      TEST_1(v->v_params != v0->v_params);
    if (v->v_branch)
      TEST_1(v->v_branch == v0->v_branch);
  }
  TEST_1(v == NULL && v0 == NULL);

  v0 = sip->sip_via;

  TEST_1(v = sip_via_dup(home, v0));
  TEST(len(v->v_common), len(v0->v_common));
  for (; v && v0; v = v->v_next, v0 = v0->v_next) {
    if (v->v_params)
      TEST_1(v->v_params != v0->v_params);
    if (v->v_branch)
      TEST_1(v->v_branch != v0->v_branch);
  }
  TEST_1(v == NULL && v0 == NULL);

  TEST(sip_add_dup(msg, sip, (sip_header_t *)sip->sip_max_forwards), 0);
  /* Max-Forwards is last header? */
  TEST_P(sip->sip_max_forwards, sip->sip_content_type->c_common->h_succ);

  TEST(sip_to_tag(home, sip->sip_to, sip->sip_to->a_tag), 0);
  TEST(sip_to_tag(home, sip->sip_to, "tag=deadbeef"), 0);
  TEST(sip_to_tag(home, sip->sip_to, "foofaa"), -1);

  msg_header_remove(msg, (msg_pub_t *)sip, (msg_header_t *)sip->sip_payload);

  TEST(sip_add_tl(msg, sip,
		  SIPTAG_FROM(SIP_NONE),
		  SIPTAG_VIA(SIP_NONE),
		  SIPTAG_VIA_STR("SIP/2.0/SCTP foo.bar.com:5060;branch=foo"),
		  SIPTAG_TO_STR("<sip:foo@bar>"),
		  SIPTAG_HEADER_STR("Authorization: Basic foobar\n"
				    "Priority:\n urgent"),
		  SIPTAG_HEADER_STR("Accept: foo/bar\n"
				    "\n"
				    "test payload\n"),
		  SIPTAG_TIMESTAMP(sip->sip_timestamp),
		  SIPTAG_END(),
		  SIPTAG_REFER_TO_STR("<sip:foo@bar>"),
		  TAG_END()), 0);
  TEST_1(sip->sip_from == NULL);
  TEST_1(sip->sip_via); TEST_1(sip->sip_via->v_next == NULL);
  TEST_S(sip->sip_via->v_protocol, "SIP/2.0/SCTP");
  TEST_1(sip->sip_authorization);
  TEST_1(sip->sip_priority);
  TEST_1(sip->sip_payload);
  TEST_S(sip->sip_payload->pl_data, "test payload\n");
  TEST_1(sip->sip_timestamp);
  TEST_S(sip->sip_timestamp->ts_stamp, "10.010");
  TEST_S(sip->sip_timestamp->ts_delay, "0.000100");
  TEST_1(!sip->sip_refer_to);

  TEST_1(tl = tl0 = tl_list(SIPTAG_TO_STR("<sip:foo@bar>"),
			    SIPTAG_END(),
			    SIPTAG_REFER_TO_STR("<sip:foo@bar>"),
			    TAG_END()));
  /* sip_add_tagis should stop after SIPTAG_END() */
  TEST(sip_add_tagis(msg, sip, &tl), 0);
  TEST_P(tl, tl0 + 2);

  tl_free(tl0);

  TEST_P(sip_timestamp_make(home, "+1"), NULL);
  TEST_P(sip_timestamp_make(home, "1.0e6 13.0"), NULL);
  TEST_1(sip_timestamp_make(home, "1.0 .001"));
  TEST_P(sip_timestamp_make(home, ".0001 13.0"), NULL);

  TEST_1(x = sip->sip_path);
  TEST_1(sip_add_make(msg, sip, sip_path_class, "<sip:135.180.130.133>") == 0);
  TEST_P(x, sip->sip_path->r_next);

  TEST_1(x = sip->sip_service_route);
  TEST_1(sip_add_make(msg, sip, sip_service_route_class,
		      "<sip:135.180.130.133>") == 0);
  TEST_P(x, sip->sip_service_route);
  TEST_1(sip->sip_service_route->r_next);

  /* Detect parsing errors */
  TEST_1(!sip_cseq_make(home, "21874624876976 INVITE"));
  TEST_1(!sip_cseq_make(home, "218746INVITE"));

  msg_destroy(msg), msg = NULL;

  su_home_unref(home), home = NULL;

  END();
}

static int test_bad_packet(void)
{
  msg_t *msg;
  sip_t *sip;
  su_home_t *home;

  BEGIN();

  home = su_home_new(sizeof *home);

  TEST_1(msg = read_message(MSG_DO_EXTRACT_COPY,
    "MESSAGE <sip:John_Smith@tct.hut.fi> SIP/2.0\r\n"
    "To: John Smith <sip:John_Smith@tct.hut.fi:5066;user=ip;maddr=131.228.16.2>\r\n"
    "  ; tag = deadbeef\r\n"
    "From:h<http://www.cs.columbia.edu>\r\n"
    "Call-ID: 0ha0isndaksdj@10.1.2.3\r\n"
    "CSeq : 8 MESSAGE\r\n"
    "Via: SIP/2.0/UDP 135.180.130.133;received=defa:daf::00:12\r\n"
    "Via: SIP/2.0/TCP 135.180.130.131;branch=deadbeef.barf;ttl=3;hidden,,"
    "SIP/2.0/UDP\r\n 135.180.130.131:5061;received=[defa::00:12]\r\n"
    "Contact: Joe Bob Briggs <urn:ipaddr:122.1.2.3> ; bar=\042foo baa\042, sip:kuik@foo.invalid\r\n"
    "Via: SIP/2.0/UDP [aa:bb::1]:5061\0\0"));

  TEST_1(sip = sip_object(msg));

  TEST(count(sip->sip_request->rq_common), 1);
  TEST(count(sip->sip_to->a_common), 1);
  TEST(count(sip->sip_from->a_common), 1);
  TEST(count(sip->sip_cseq->cs_common), 1);
  TEST(count(sip->sip_call_id->i_common), 1);
  TEST(count(sip->sip_via->v_common), 4);
  TEST(count(sip->sip_route->r_common), 0);

  TEST(sip->sip_request->rq_url->url_type, url_invalid);

  su_home_unref(home), home = NULL;

  msg_destroy(msg), msg = NULL;

  END();
}

static int test_sip_list_header(void)
{
  msg_t *msg;
  sip_t *sip;
  su_home_t *home;
  sip_allow_t *a;

  BEGIN();

  home = su_home_new(sizeof *home);

  TEST_1(msg = read_message(0,
    "MESSAGE sip:John_Smith@tct.hut.fi SIP/2.0\r\n"
    "To: John Smith <sip:John_Smith@tct.hut.fi:5066;user=ip;maddr=131.228.16.2>\r\n"
    "From: <sip:joe@doe.org>;tag=foobar\r\n"
    "Call-ID: 0ha0isndaksdj@10.1.2.3\r\n"
    "CSeq : 8 MESSAGE\r\n"
    "Via: SIP/2.0/UDP 135.180.130.133;received=defa:daf::00:12\r\n"
    "Via: SIP/2.0/TCP 135.180.130.131;branch=deadbeef.barf;ttl=3;hidden,,"
    "SIP/2.0/UDP\r\n 135.180.130.131:5061;received=[defa::00:12]\r\n"
    "Contact: Joe Bob Briggs <urn:ipaddr:122.1.2.3> ; bar=\042foo baa\042, <sip:kuik@foo.invalid>, sip:barf\r\n"
    "Allow: INVITE\r\n"
    "Allow: ACK\r\n"
    "Allow: CANCEL\r\n"
    "Allow: BYE\r\n"
    "Allow: OPTIONS\r\n"
    "Allow: MESSAGE\r\n"
    "Allow: KUIK\r\n"
    "Max-Forwards: 12\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n"
    "hello\r\n"));

  TEST_1(sip = sip_object(msg));
  TEST_1(a = sip->sip_allow);
  TEST_1(a->k_items);
  TEST_1(a->k_next == NULL);

  TEST_1(sip_is_allowed(a, SIP_METHOD_INVITE));
  TEST_1(!sip_is_allowed(a, SIP_METHOD_PUBLISH));
  TEST_1(sip_is_allowed(a, SIP_METHOD(KUIK)));
  TEST_1(!sip_is_allowed(a, SIP_METHOD(kuik)));

  TEST_1(a = sip_allow_make(home, ""));
  TEST_S(sip_header_as_string(home, (void *)a), "");

  TEST_1(a = sip_allow_make(home, "INVITE, PUBLISH"));

  TEST_1(sip_is_allowed(a, SIP_METHOD_INVITE));

  /* Test with list header */
  TEST_1(msg_header_add_dup(msg, NULL, (msg_header_t *)a) == 0);

  TEST_1(a = sip_allow_make(home, "MESSAGE, SUBSCRIBE"));

  TEST_1(msg_header_add_dup(msg, NULL, (msg_header_t *)a) == 0);

  TEST_1(msg_header_add_make(msg, NULL, sip_allow_class, "kuik") == 0);

  TEST_1(a = sip->sip_allow);
  TEST_1(a->k_items);
  TEST_S(a->k_items[0], "INVITE");
  TEST_S(a->k_items[1], "ACK");
  TEST_S(a->k_items[2], "CANCEL");
  TEST_S(a->k_items[3], "BYE");
  TEST_S(a->k_items[4], "OPTIONS");
  TEST_S(a->k_items[5], "MESSAGE");
  TEST_S(a->k_items[6], "KUIK");
  TEST_S(a->k_items[7], "PUBLISH");
  TEST_S(a->k_items[8], "SUBSCRIBE");
  TEST_S(a->k_items[9], "kuik");
  TEST_P(a->k_items[10], NULL);

  msg_destroy(msg), msg = NULL;

  su_home_unref(home), home = NULL;

  END();
}

static int test_prack(void)
{
  /* Test RAck and RSeq */
  su_home_t *home;
  sip_rack_t *rack, *rack0;
  sip_rseq_t *rseq, *rseq0;

  BEGIN();

  TEST_1(home = su_home_create());
  TEST_1(rack = sip_rack_make(home, "1 2 INVITE"));
  TEST(rack->ra_response, 1);
  TEST(rack->ra_cseq, 2);
  TEST(rack->ra_method, sip_method_invite);
  TEST_S(rack->ra_method_name, "INVITE");
  TEST_1(rseq = sip_rseq_make(home, "3"));
  TEST(rseq->rs_response, 3);

  TEST_1(rack0 = sip_rack_dup(home, rack));
  TEST_P(rack0->ra_method_name, rack->ra_method_name);
  TEST_1(rseq0 = sip_rseq_dup(home, rseq));

  TEST_1(rack = sip_rack_make(home, "4\r\n\t5\r\n\tEXTRA"));
  TEST(rack->ra_response, 4);
  TEST(rack->ra_cseq, 5);
  TEST(rack->ra_method, sip_method_unknown);
  TEST_S(rack->ra_method_name, "EXTRA");
  TEST_1(rseq = sip_rseq_make(home, "  6  "));
  TEST(rseq->rs_response, 6);

  TEST_1(rack0 = sip_rack_dup(home, rack));
  TEST_1(rack0->ra_method_name != rack->ra_method_name);
  TEST_1(rseq0 = sip_rseq_dup(home, rseq));

  su_home_unref(home);

  END();
}

/* Test MIME headers */
static int test_accept(void)
{
  /* Test Accept header */
  sip_accept_t *ac, *ac0;
  sip_accept_encoding_t *aa;
  su_home_t *home;

  BEGIN();

  TEST_1(home = su_home_create());
  TEST_1(ac = ac0 = sip_accept_make(home, "image / jpeg ; q = 0.6,, image/png, image/*, */*  "));
  TEST_S(ac->ac_type, "image/jpeg");
  TEST_S(ac->ac_subtype, "jpeg");
  TEST_1(ac->ac_params && ac->ac_params[0]);
  TEST_S(ac->ac_params[0], "q=0.6");
  TEST_S(ac->ac_q, "0.6");

  TEST_1(ac = ac->ac_next);
  TEST_S(ac->ac_type, "image/png");
  TEST_S(ac->ac_subtype, "png");

  TEST_1(ac = ac->ac_next);
  TEST_S(ac->ac_type, "image/*");
  TEST_S(ac->ac_subtype, "*");

  TEST_1(aa = sip_accept_encoding_make(home, "gzip"));
  TEST_1(aa = sip_accept_encoding_make(home, "gzip;q=1.0,deflate;q=1.0"));
  TEST_S(aa->aa_value, "gzip"); TEST_S(aa->aa_q, "1.0");
  TEST_1(aa->aa_next);
  TEST_S(aa->aa_next->aa_value, "deflate");
  TEST_1(aa = sip_accept_encoding_make(home, ","));
  TEST_S(aa->aa_value, ""); TEST_1(!aa->aa_next);
  TEST_1(aa = sip_accept_encoding_make(home, ""));
  TEST_S(aa->aa_value, ""); TEST_1(!aa->aa_next);

  TEST_1(aa = sip_accept_language_make(home, "fi"));
  TEST_1(aa = sip_accept_language_make(home, "fi;q=1.0,sv;q=1.0"));
  TEST_S(aa->aa_value, "fi"); TEST_S(aa->aa_q, "1.0");
  TEST_1(aa->aa_next);
  TEST_S(aa->aa_next->aa_value, "sv");
  TEST_1(aa = sip_accept_language_make(home, ","));
  TEST_S(aa->aa_value, ""); TEST_1(!aa->aa_next);
  TEST_1(aa = sip_accept_language_make(home, ""));
  TEST_S(aa->aa_value, ""); TEST_1(!aa->aa_next);

  su_home_unref(home);

  END();
}

static int test_content_disposition(void)
{
  sip_content_disposition_t *cd, *cd0;
  su_home_t *home;

  BEGIN();

  TEST_1(home = su_home_create());
  TEST_1(cd = cd0 = sip_content_disposition_make(home, "sip-cgi ; action = store;handling=required  "));
  TEST_S(cd->cd_type, "sip-cgi");
  TEST_1(cd->cd_params && cd->cd_params[0] && cd->cd_params[1] && !cd->cd_params[2]);
  TEST_S(cd->cd_params[0], "action=store");
  TEST_S(cd->cd_params[1], "handling=required");
  TEST_S(cd->cd_handling, "required");
  TEST_1(cd->cd_required);
  TEST_1(!cd->cd_optional);

  su_home_unref(home);
  END();
}


static int test_content_type(void)
{
  sip_content_type_t *c;
  sip_content_type_t c0[1];
  su_home_t *home;

  BEGIN();

  TEST_1(home = su_home_create());
  TEST_1(c = sip_content_type_make(home, "application/sdp ; charset = utf-8"));
  TEST_S(c->c_type, "application/sdp");
  TEST_S(c->c_subtype, "sdp");
  TEST_1(c->c_params && c->c_params[0] && !c->c_params[1]);
  TEST_S(c->c_params[0], "charset=utf-8");
  TEST_P(c->c_params[1], NULL);

  sip_content_type_init(c0);
  c = sip_content_type_dup(home, c0);
  TEST_P(c->c_type, NULL);
  TEST_P(c->c_subtype, NULL);

  c0->c_type = "text";
  c = sip_content_type_dup(home, c0);
  TEST_S(c->c_type, "text");
  TEST_P(c->c_subtype, NULL);

  su_home_unref(home);
  END();
}


static int test_www_authenticate(void)
{
  sip_www_authenticate_t *www;
  su_home_t *home;
  char const *s;
  msg_t *msg; sip_t *sip;

  BEGIN();

  TEST_1(home = su_home_create());
  TEST_1(www = sip_www_authenticate_make
	 (home, "Digest realm=\"Registered_Subscribers\",\n"
	  "domain=\"sip:206.229.26.61\",\n"
	  "nonce=\"20dfb7e5a77abee7a02dbe53efe42cdd\", "
	  "opaque=\"423767123y723742376423762376423784623782a794e58\",\n"
	  "stale=FALSE,algorithm=MD5"));
  TEST_S(www->au_scheme, "Digest");
  TEST_1(www->au_params && www->au_params[0] && www->au_params[1] && www->au_params[2] &&
	 www->au_params[3] && www->au_params[4] && www->au_params[5] &&
	 !www->au_params[6]);
  TEST_1(s = sip_header_as_string(home, (sip_header_t *)www));
  TEST_1(strlen(s) >= 128);

  su_home_unref(home);

  TEST_1(
    msg = read_message(
      MSG_DO_EXTRACT_COPY,
      "SIP/2.0 401 Unauthorized" "\r\n"
      "Date: Wed, 07 Jan 2009 22:24:39 GMT" "\r\n"
      "WWW-Authenticate: Kerberos realm=\"SIP Communications Service\", targetname=\"sip/OCS1.flux.local\", version=3" "\r\n"
      "WWW-Authenticate: NTLM realm=\"SIP Communications Service\", targetname=\"OCS1.flux.local\", version=3" "\r\n"
      "From: <sip:192.168.43.1:5069>;epid=1234567890;tag=48BXgr379e85j" "\r\n"
      "To: <sip:1234@192.168.43.20:5061>;transport=tls;tag=B57737091022903031FF204696B79CC4" "\r\n"
      "Call-ID: cf12f708-57ac-122c-ad90-6f23d7babf4f" "\r\n"
      "CSeq: 109565202 REGISTER" "\r\n"
      "Via: SIP/2.0/TLS 192.168.43.1:5069;branch=z9hG4bK47ZUrFK0v5eQa;received=192.168.43.1;ms-received-port=54059;ms-received-cid=E500" "\r\n"
      "Content-Length: 0" "\r\n"));
  TEST_1(sip = sip_object(msg));

  TEST_1(www = sip->sip_www_authenticate);
  TEST_S(www->au_scheme, "Kerberos");
  TEST_1(www = www->au_next);
  TEST_S(www->au_scheme, "NTLM");
 
  msg_destroy(msg);

  END();
}

int test_retry_after(void)
{
  /* Test Session-Expires header */
  sip_retry_after_t *af, *af0;
  su_home_t *home;
  char buf[64];

  BEGIN();

  TEST_1(home = su_home_create());
  TEST_1(af = sip_retry_after_make(home, "1800"));
  TEST(af->af_delta, 1800);
  TEST_1(af = sip_retry_after_make(home, "1800(foo); duration = 3600"));
  TEST_1(af->af_params && af->af_params[0]);
  TEST_S(af->af_comment, "foo");
  TEST_S(af->af_params[0], "duration=3600");
  TEST_S(af->af_duration, "3600");

  TEST_1(af0 = sip_retry_after_dup(home, af));
  TEST_1(af0->af_params && af0->af_params[0]);
  TEST_S(af0->af_comment, "foo");
  TEST_S(af0->af_params[0], "duration=3600");
  TEST_S(af0->af_duration, "3600");

  TEST_1(sip_retry_after_e(buf, sizeof(buf), (sip_header_t *)af0, 0));

  TEST_S(buf, "1800 (foo) ;duration=3600");

  su_home_unref(home);

  END();
}

int test_session_expires(void)
{
  /* Test Session-Expires header */
  sip_session_expires_t *x, *x0;
  su_home_t *home;

  BEGIN();

  TEST_1(home = su_home_create());
  TEST_1(x = x0 = sip_session_expires_make(home, "1800"));
  TEST(x->x_delta, 1800);
  TEST_1(x = x0 = sip_session_expires_make(home, "1800 ; refresher = uas"));
  TEST_1(x->x_params && x->x_params[0]);
  TEST_S(x->x_params[0], "refresher=uas");
  TEST_S(x->x_refresher, "uas");

  su_home_unref(home);

  END();
}

int test_min_se(void)
{
  /* Test Min-SE header */
  sip_min_se_t *min, *min0;
  su_home_t *home;

  BEGIN();

  TEST_1(home = su_home_create());
  TEST_1(min = min0 = sip_min_se_make(home, "1800"));
  TEST(min->min_delta, 1800);
  TEST_1(min = sip_min_se_dup(home, min0));
  TEST(min->min_delta, 1800);
  TEST_1(min = sip_min_se_copy(home, min0));
  TEST(min->min_delta, 1800);

  TEST_1(min = sip_min_se_make(home, "1999 ; foo = bar"));
  TEST(min->min_delta, 1999);
  TEST_1(min->min_params);
  TEST_S(min->min_params[0], "foo=bar");

  TEST_1(min0 = sip_min_se_dup(home, min));
  TEST(min0->min_delta, 1999);
  TEST_1(min0->min_params);
  TEST_S(min0->min_params[0], "foo=bar");

  su_home_unref(home);

  END();
}

int test_refer(void)
{
  sip_refer_to_t *r, *r0;
  sip_referred_by_t *b, *b0;
  sip_replaces_t *rp, *rp0;
  char const *s0;

  su_home_t *home;

  BEGIN();

  char const m[] =
    "REFER sip:10.3.3.104 SIP/2.0\r\n"
    "Via: SIP/2.0/UDP 10.3.3.8;branch=z9hG4bKb8389b4c1BA8899\r\n"
    "From: \"Anthony Minessale\" <sip:polycom500@10.3.3.104>;tag=5AA04E0-66CFC37F\r\n"
    "To: <sip:3001@10.3.3.104>;user=phone;tag=j6Fg9y7t8KNrF\r\n"
    "CSeq: 4 REFER\r\n"
    "Call-ID: a14822a4-5932e3ea-d7f37191@10.3.3.8\r\n"
    "Contact: <sip:polycom500@10.3.3.8>\r\n"
    "User-Agent: PolycomSoundPointIP-SPIP_500-UA/1.4.1\r\n"
    "Refer-To: <sip:2000@10.3.3.104?Replaces=7d84c014-321368da-efa90f41%40"
      "10.3.3.8%3Bto-tag%3DpaNKgBB9vQe3D%3Bfrom-tag%3D93AC8D50-7CF6DAAF>\r\n"
    "Referred-By: \"Anthony Minessale\" <sip:polycom500@10.3.3.104>\r\n"
    "Refer-Sub: true\r\n"
    "Max-Forwards: 70\r\n"
    "Content-Length: 0\r\n"
    "\r\n";
  msg_t *msg;
  sip_t *sip;
  msg_iovec_t *iovec;
  isize_t veclen, i, size;
  char *back;
  sip_refer_sub_t *rs;

  TEST_1(home = su_home_create());

  /* Check that Refer-Sub has now been added to our parser */
  TEST_1(msg_mclass_insert_with_mask(test_mclass, sip_refer_sub_class,
				     0, 0) == -1);

  msg = read_message(0, m); TEST_1(msg); TEST_1(sip = sip_object(msg));
  TEST_1(sip->sip_refer_to);
  TEST_S(sip->sip_refer_to->r_url->url_headers,
	 "Replaces=7d84c014-321368da-efa90f41%40"
	 "10.3.3.8%3Bto-tag%3DpaNKgBB9vQe3D%3Bfrom-tag%3D93AC8D50-7CF6DAAF");

  TEST_1(rs = sip_refer_sub(sip));
  TEST_S(rs->rs_value, "true");

  TEST_SIZE(msg_prepare(msg), strlen(m));
  TEST_1(veclen = msg_iovec(msg, NULL, ISIZE_MAX));
  TEST_1(iovec = su_zalloc(msg_home(home), veclen * (sizeof iovec[0])));
  TEST_SIZE(msg_iovec(msg, iovec, veclen), veclen);

  for (i = 0, size = 0; i < veclen; i++)
    size += iovec[i].mv_len;

  TEST_1(back = su_zalloc(msg_home(msg), size + 1));

  for (i = 0, size = 0; i < veclen; i++) {
    memcpy(back + size, iovec[i].mv_base, iovec[i].mv_len);
    size += iovec[i].mv_len;
  }
  back[size] = '\0';

  TEST_S(back, m);

  TEST_1(r = r0 = sip_refer_to_make(home, "http://example.com;foo=bar"));
  TEST(r->r_url->url_type, url_http);
  TEST_1(r->r_params);
  TEST_S(r->r_params[0], "foo=bar");
  r = sip_refer_to_dup(home, r0);
  TEST(r->r_url->url_type, url_http);
  TEST_1(r->r_params);
  TEST_S(r->r_params[0], "foo=bar");

  TEST_1(r = r0 = sip_refer_to_make(home, s0 = "<http://example.com>"));
  TEST_S(r->r_display, "");
  TEST(r->r_url->url_type, url_http);
  TEST_P(r->r_params, NULL);
  r = sip_refer_to_dup(home, r0);
  TEST_S(r->r_display, "");
  TEST(r->r_url->url_type, url_http);
  TEST_P(r->r_params, NULL);
  TEST_S(sip_header_as_string(home, (sip_header_t*)r), s0);

  TEST_1(r = r0 = sip_refer_to_make(home,
				    "Web Site <http://example.com>;foo=bar"));
  TEST_S(r->r_display, "Web Site");
  TEST(r->r_url->url_type, url_http);
  TEST_1(r->r_params);
  TEST_S(r->r_params[0], "foo=bar");
  TEST_P(r->r_params[1], NULL);
  r = sip_refer_to_dup(home, r0);
  TEST(r->r_url->url_type, url_http);
  TEST_1(r->r_params);
  TEST_S(r->r_params[0], "foo=bar");
  TEST_P(r->r_params[1], NULL);

  /* Test bad replaces without <> */
  {
    char const s[] =
      "sip:2000@10.3.3.104?Replaces=7d84c014-321368da-efa90f41%4010.3.3.8"
      "%3Bto-tag%3DpaNKgBB9vQe3D%3Bfrom-tag%3D93AC8D50-7CF6DAAF" "\r\n";
    char *str;

    TEST_1(r = r0 = sip_refer_to_make(home, s));
    msg_fragment_clear(r->r_common);
    TEST_1(str = sip_header_as_string(home, (void *)r));
    TEST_S(str,
	   "<"
	   "sip:2000@10.3.3.104?Replaces=7d84c014-321368da-efa90f41%4010.3.3.8"
	   "%3Bto-tag%3DpaNKgBB9vQe3D%3Bfrom-tag%3D93AC8D50-7CF6DAAF"
	   ">");
  }

  su_home_unref(home);

  TEST_1(home = su_home_create());
  TEST_1(b = b0 = sip_referred_by_make(home,
				      "sip:joe@example.edu;param=value"));
  TEST_P(b->b_display, NULL);
  TEST_1(b->b_params);
  TEST_P(b->b_cid, NULL);

  TEST_1(b = sip_referred_by_make(home,
				  "John Doe <sip:joe@example.edu>"
				  ";cid=\"foo@bar\""));
  TEST_S(b->b_display, "John Doe");
  TEST_1(b->b_params);
  TEST_1(b->b_cid);
  TEST_S(b->b_params[0] + 4, b->b_cid);

  b = sip_referred_by_dup(home, b0 = b);

  TEST_1(b);
  TEST_S(b->b_display, "John Doe");
  TEST_1(b->b_cid);
  TEST_S(b->b_params[0] + 4, b->b_cid);
  TEST_S(b->b_cid, b0->b_cid);

  TEST(msg_header_replace_param(home, b->b_common, "cid=cid:8u432658725"), 1);
  TEST_S(b->b_cid, "cid:8u432658725");
  TEST(msg_header_remove_param(b->b_common, "cid"), 1);
  TEST_P(b->b_cid, NULL);

  /* XXX */
#define WORD ALPHA DIGIT "-.!%*_+`'~()<>:\\\"/[]?{}"
  rp = sip_replaces_make(home, WORD "@" WORD ";to-tag=foo;from-tag=bar"
			 ";early-only = yes-please   ");

  TEST_1(rp);
  TEST_S(rp->rp_call_id, WORD "@" WORD);
  TEST_S(rp->rp_to_tag, "foo");
  TEST_S(rp->rp_from_tag, "bar");
  TEST(rp->rp_early_only, 1);

  rp = sip_replaces_dup(home, rp0 = rp);

  TEST_1(rp);
  TEST_S(rp->rp_call_id, WORD "@" WORD);
  TEST_S(rp->rp_to_tag, "foo");
  TEST_S(rp->rp_from_tag, "bar");
  TEST(rp->rp_early_only, 1);

  TEST(msg_header_replace_param(home, rp->rp_common, "early-only"), 1);
  TEST(rp->rp_early_only, 1);
  TEST(msg_header_remove_param(rp->rp_common, "from-tag"), 1);
  TEST_P(rp->rp_from_tag, NULL);
  TEST(msg_header_remove_param(rp->rp_common, "to-tag"), 1);
  TEST_P(rp->rp_to_tag, NULL);

  su_home_unref(home);

  END();
}

static int test_features(void)
{
  /* Test Proxy-Required, Require, Supported, and Unsupported headers */
  sip_proxy_require_t *pr;
  sip_require_t *r;
  sip_supported_t *s;
  sip_unsupported_t *u, *u1;
  su_home_t *home;

  BEGIN();

  TEST_1(home = su_home_create());
  TEST_1(pr = sip_proxy_require_make(home, "foo, bar, baz, dig, dug"));
  TEST_1(r = sip_require_make(home, "dig, dug"));
  TEST_1(s = sip_supported_make(home, "foo, baz, dug"));

  TEST_1(pr->k_items); TEST_S(pr->k_items[0], "foo");
  TEST_1(r->k_items); TEST_S(r->k_items[0], "dig");
  TEST_1(s->k_items); TEST_S(s->k_items[0], "foo");

  TEST_1(u = sip_has_unsupported(home, s, pr));
  TEST_1(u->k_items);
  TEST_S(u->k_items[0], "bar");
  TEST_S(u->k_items[1], "dig");
  TEST_P(u->k_items[2], NULL);

  TEST_1(u1 = sip_has_unsupported(home, s, r));
  TEST_1(u1->k_items); TEST_S(u1->k_items[0], "dig"); TEST_1(!u1->k_items[1]);

  TEST_1(sip_has_supported(s, "foo"));
  TEST_1(sip_has_supported(s, "baz"));
  TEST_1(sip_has_supported(s, "dug"));
  TEST_1(!sip_has_supported(s, "dig"));
  TEST_1(!sip_has_supported(s, "dag.2"));
  TEST_1(sip_has_supported(s, NULL));
  TEST_1(sip_has_supported(NULL, NULL));
  TEST_1(!sip_has_supported(NULL, "foo"));

  su_home_unref(home);
  END();
}

#if 0
static int sip_time_test(void)
{
  sip_contact_t *m;
  sip_expires_t *ex;
  sip_date_t *date = NULL;
  sip_time_t default = 3600;
  BEGIN();

  sip_time_t sip_contact_expires(sip_contact_t const *m,
				 sip_expires_t const *ex,
				 sip_date_t const *date,
				 sip_time_t def,
				 sip_time_t now);


  END();
}
#endif

static int test_events(void)
{
  sip_event_t *o;
  sip_allow_events_t *ae;
  sip_subscription_state_t *ss;
  su_home_t *home;
  msg_t *msg;
  sip_t *sip;

  BEGIN();

  TEST_1(home = su_home_create());

  TEST_1((o = sip_event_make(home, "presence;id=1")));
  TEST_S(o->o_type, "presence");
  TEST_S(o->o_id, "1");

  TEST(msg_header_remove_param(o->o_common, "ix=0"), 0);
  TEST_S(o->o_id, "1");
  TEST(msg_header_remove_param(o->o_common, "id"), 1);
  TEST_P(o->o_id, NULL);
  TEST(msg_header_replace_param(home, o->o_common, "id=32"), 0);
  TEST_S(o->o_id, "32");

  TEST_1((ae = sip_allow_events_make(home, "presence, presence.winfo, foo")));
  TEST_1(ae->k_items);
  TEST_S(ae->k_items[0], "presence");
  TEST_S(ae->k_items[1], "presence.winfo");
  TEST_S(ae->k_items[2], "foo");
  TEST_P(ae->k_items[3], 0);
  TEST(sip_allow_events_add(home, ae, "event3"), 0);
  TEST_S(ae->k_items[3], "event3");
  TEST(sip_allow_events_add(home, ae, "event4"), 0);
  TEST_S(ae->k_items[4], "event4");
  TEST(sip_allow_events_add(home, ae, "event5"), 0);
  TEST_S(ae->k_items[5], "event5");
  TEST(sip_allow_events_add(home, ae, "event6"), 0);
  TEST_S(ae->k_items[6], "event6");
  TEST(sip_allow_events_add(home, ae, "event7"), 0);
  TEST_S(ae->k_items[7], "event7");
  TEST(sip_allow_events_add(home, ae, "event8"), 0);
  TEST_S(ae->k_items[8], "event8");

  TEST_1((ss =
	 sip_subscription_state_make(home, "terminated ; reason=timeout")));
  TEST_S(ss->ss_substate, "terminated");
  TEST_S(ss->ss_reason, "timeout");

  TEST(msg_header_replace_param(home, ss->ss_common, "reason=TimeOut"), 1);
  TEST_S(ss->ss_reason, "TimeOut");
  TEST(msg_header_remove_param(ss->ss_common, "reasom"), 0);
  TEST_S(ss->ss_reason, "TimeOut");
  TEST(msg_header_remove_param(ss->ss_common, "reason"), 1);
  TEST_P(ss->ss_reason, NULL);
  TEST(msg_header_replace_param(home, ss->ss_common, "expires=200"), 0);
  TEST(msg_header_replace_param(home, ss->ss_common, "retry-after=10"), 0);
  TEST_S(ss->ss_expires, "200");
  TEST_S(ss->ss_retry_after, "10");

  TEST_1((ss =
	 sip_subscription_state_make(home, "active;expires=2")));
  TEST_S(ss->ss_substate, "active");
  TEST_S(ss->ss_expires, "2");

  TEST_1((ss =
	  sip_subscription_state_make(home, "terminated;retry-after=3600")));
  TEST_S(ss->ss_substate, "terminated");
  TEST_P(ss->ss_expires, NULL);
  TEST_S(ss->ss_retry_after, "3600");

  TEST_1((ss = sip_subscription_state_dup(home, ss)));
  TEST_S(ss->ss_substate, "terminated");
  TEST_P(ss->ss_expires, NULL);
  TEST_S(ss->ss_retry_after, "3600");

  msg = read_message(MSG_DO_EXTRACT_COPY,
    "SIP/2.0 202 Accepted\r\n"
    "To: <sip:foo@bar>;tag=deadbeef\r\n"
    "From: <sip:bar@foo>;\r\n"
    "Call-ID: 0ha0isndaksdj@10.1.2.3\r\n"
    "CSeq: 8 SUBSCRIBE\r\n"
    "Via: SIP/2.0/UDP 135.180.130.133\r\n"
    "Event: foo;id=1\r\n"
    "Allow-Events: bar, foo, zap\r\n"
    "Subscription-State: terminated;reason=probation;retry-after=100000\r\n"
    "Content-Length: 0\r\n"
    "\r\n");

  sip = sip_object(msg);

  TEST_1(msg);
  TEST_1(sip);
  TEST_1(sip->sip_event);
  TEST_1(sip->sip_allow_events);
  TEST_1(sip->sip_event->o_type);
  TEST_S(sip->sip_event->o_type, "foo");
  TEST_1(sip->sip_event->o_id);
  TEST_S(sip->sip_event->o_id, "1");
  TEST_1(sip->sip_allow_events);

  su_home_unref(home);
  msg_destroy(msg), msg = NULL;

  END();
}

static int test_route(void)
{
  sip_record_route_t *r0, *r1;
  sip_record_route_t *rr;
  sip_path_t *p, *p0;
  sip_service_route_t *sr, *sr0;

  su_home_t *home;

  BEGIN();

  TEST_1(home = su_home_create());

  TEST_1((rr = sip_record_route_make(home, "sip:foo.bar;lr")));
  TEST_1(rr->r_params);

  TEST_1((r0 = sip_record_route_make(home, "<sip:0@foo.bar:555;lr>")));
  TEST_P(r0->r_params, NULL);
  TEST_1(r0->r_url->url_params);

  TEST_1((r1 = sip_record_route_make(home, "<sip:1@foo.bar:666"
				    ";maddr=127.0.0.1>")));
  TEST_P(r1->r_params, NULL);
  TEST_1(r1->r_url->url_params);

  TEST_1((rr = sip_record_route_create(home, r0->r_url, r1->r_url)));
  TEST_S(rr->r_url->url_user, "0");
  TEST_S(rr->r_url->url_port, "666");
  TEST_S(rr->r_url->url_params, "maddr=127.0.0.1");

  TEST_1((rr = sip_record_route_create(home, r1->r_url, r0->r_url)));
  TEST_S(rr->r_url->url_user, "1");
  TEST_S(rr->r_url->url_port, "555");
  TEST_S(rr->r_url->url_params, "lr;maddr=foo.bar");

  TEST_1(!sip_path_make(home, "<sip:foo@[bar:50>;lr"));
  TEST_1(p = sip_path_make(home, "<sip:foo@[baa::1]:5060>;lr"));
  TEST_1(p0 = sip_path_dup(home, p));

  su_free(home, p);
  su_free(home, p0);

  TEST_1(!sip_service_route_make(home, "<sip:foo@[bar:50>;lr"));
  TEST_1(!sip_service_route_make(home,
				 "<sip:foo@[baa::1]>;lr bar, sip:foo"));

  TEST_1(sr = sip_service_route_make(home, "<sip:foo@[baa::1]:5060>;lr"));
  TEST_1(sr0 = sip_service_route_dup(home, sr));
  su_free(home, sr);

  TEST_1(sr = sip_service_route_make(home, "sip:foo@[baa::1]:5060;lr"));

  su_free(home, sr);
  su_free(home, sr0);

  su_home_unref(home);

  END();
}

/* Test Request-Disposition header */
int test_request_disposition(void)
{
  sip_request_disposition_t *rd;
  su_home_t *home;

  BEGIN();

  TEST_1(home = su_home_create());

  TEST_1(rd = sip_request_disposition_make(home, "proxy, recurse, parallel"));
  TEST_S(rd->rd_items[1], "recurse");

  su_home_unref(home);

  END();
}

#include <float.h>
#include <math.h>

int test_caller_prefs(void)
{
  sip_accept_contact_t *ac;
  sip_accept_contact_t *cp;
  sip_reject_contact_t *rejc;
  su_home_t *home;
  char const *s;
  int negate, error;
  unsigned S, N;
  union sip_pref sp[1], a[1];
  sip_contact_t *m, *m0, *m1, *m2;

  BEGIN();

  TEST_1(home = su_home_new(sizeof *home));

  TEST_1(!sip_is_callerpref("attendant"));
  TEST_1(sip_is_callerpref("audio"));
  TEST_1(sip_is_callerpref("automata"));
  TEST_1(sip_is_callerpref("class"));
  TEST_1(sip_is_callerpref("duplex"));
  TEST_1(sip_is_callerpref("data"));
  TEST_1(sip_is_callerpref("control"));
  TEST_1(sip_is_callerpref("mobility"));
  TEST_1(sip_is_callerpref("description"));
  TEST_1(sip_is_callerpref("events"));
  TEST_1(sip_is_callerpref("priority"));
  TEST_1(sip_is_callerpref("methods"));
  TEST_1(sip_is_callerpref("schemes"));
  TEST_1(sip_is_callerpref("application"));
  TEST_1(sip_is_callerpref("video"));
  TEST_1(sip_is_callerpref("actor"));
  TEST_1(!sip_is_callerpref("+actor"));
  TEST_1(!sip_is_callerpref("msgserver"));
  TEST_1(sip_is_callerpref("language"));
  TEST_1(sip_is_callerpref("isfocus"));
  TEST_1(sip_is_callerpref("type"));
  TEST_1(!sip_is_callerpref("uri-user"));
  TEST_1(!sip_is_callerpref("uri-domain"));
  TEST_1(!sip_is_callerpref(NULL));
  TEST_1(sip_is_callerpref("+"));
  TEST_1(sip_is_callerpref("+foo"));

  /* Booleans (treated as literals) */
  s = "TRUE";
  negate = 2; memset(sp, 0, sizeof sp);
  TEST_1(sip_prefs_parse(sp, &s, &negate));
  TEST(sp->sp_type, sp_literal);
  TEST_S(sp->sp_literal.spl_value, "TRUE"); TEST_1(!negate);
  TEST_1(sip_prefs_match(sp, sp));
  TEST_1(!sip_prefs_parse(sp, &s, &negate));
  TEST(sp->sp_type, sp_init);

  s = "FALSE";
  negate = 2; memset(sp, 0, sizeof sp);
  TEST_1(sip_prefs_parse(sp, &s, &negate));
  TEST(sp->sp_type, sp_literal);
  TEST_S(sp->sp_literal.spl_value, "FALSE"); TEST_1(!negate);
  TEST_1(sip_prefs_match(sp, sp));
  TEST_1(!sip_prefs_parse(sp, &s, &negate));
  TEST(sp->sp_type, sp_init);

  s = "\"!TRUE,!FALSE\""; negate = 0;
  TEST_1(sip_prefs_parse(sp, &s, &negate));
  TEST(sp->sp_type, sp_literal); TEST_1(negate);

  /* Literal */
  s = "\" !oukki , doukki  \""; negate = 0; memset(sp, 0, sizeof sp);

  TEST_1(sip_prefs_parse(sp, &s, &negate));
  TEST(sp->sp_type, sp_literal);
  TEST_SIZE(sp->sp_literal.spl_length, 5);
  TEST_M(sp->sp_literal.spl_value, "oukki", 5); TEST_1(negate);

  TEST_1(sip_prefs_parse(sp, &s, &negate));
  TEST(sp->sp_type, sp_literal);
  TEST_SIZE(sp->sp_literal.spl_length, 6);
  TEST_M(sp->sp_literal.spl_value, "doukki", 6); TEST_1(!negate);

  TEST_1(!sip_prefs_parse(sp, &s, &negate));
  TEST(sp->sp_type, sp_init);

  /* Strings */
  s = "\" !<oukki> , <douK\\\"ki  >\"";
  negate = 0; memset(sp, 0, sizeof sp);

  TEST_1(sip_prefs_parse(sp, &s, &negate));
  TEST(sp->sp_type, sp_string);
  TEST_SIZE(sp->sp_string.sps_length, 5);
  TEST_M(sp->sp_string.sps_value, "oukki", 5); TEST_1(negate);

  TEST_1(sip_prefs_parse(sp, &s, &negate));
  TEST(sp->sp_type, sp_string);
  TEST_SIZE(sp->sp_string.sps_length, 10);
  TEST_M(sp->sp_string.sps_value, "douK\\\"ki  ", 10); TEST_1(!negate);

  TEST_1(!sip_prefs_parse(sp, &s, &negate));
  TEST(sp->sp_type, sp_init);

  /* Numeric */
  s = "\" !#=6, #<=3, #>=6, !#<=6, #1:6.5\"";
  negate = 0; memset(sp, 0, sizeof sp);

  TEST_1(sip_prefs_parse(sp, &s, &negate));
  TEST(sp->sp_type, sp_range);
  TEST_D(sp->sp_range.spr_lower, 6.0);
  TEST_D(sp->sp_range.spr_upper, 6.0);
  TEST_1(sip_prefs_match(sp, sp));
  TEST_1(negate);

  *a = *sp;

  TEST_1(sip_prefs_parse(sp, &s, &negate));
  TEST(sp->sp_type, sp_range);
  TEST_D(sp->sp_range.spr_lower, -DBL_MAX);
  TEST_D(sp->sp_range.spr_upper, 3.0);
  TEST_1(sip_prefs_match(sp, sp));
  TEST_1(!negate);

  TEST_1(!sip_prefs_match(a, sp));

  TEST_1(sip_prefs_parse(sp, &s, &negate));
  TEST(sp->sp_type, sp_range);
  TEST_D(sp->sp_range.spr_lower, 6.0);
  TEST_D(sp->sp_range.spr_upper, DBL_MAX);
  TEST_1(sip_prefs_match(sp, sp));
  TEST_1(!negate);

  TEST_1(sip_prefs_match(a, sp));

  TEST_1(sip_prefs_parse(sp, &s, &negate));
  TEST(sp->sp_type, sp_range);
  TEST_D(sp->sp_range.spr_lower, -DBL_MAX);
  TEST_D(sp->sp_range.spr_upper, 6.0);
  TEST_1(sip_prefs_match(sp, sp));
  TEST_1(negate);

  TEST_1(sip_prefs_match(a, sp));

  TEST_1(sip_prefs_parse(sp, &s, &negate));
  TEST(sp->sp_type, sp_range);
  TEST_D(sp->sp_range.spr_lower, 1.0);
  TEST_D(sp->sp_range.spr_upper, 6.5);
  TEST_1(sip_prefs_match(sp, sp));
  TEST_1(!negate);

  TEST_1(sip_prefs_match(a, sp));

  TEST_1(!sip_prefs_parse(sp, &s, &negate));
  TEST(sp->sp_type, sp_init);

  /* Numeric */
  s = "\" !#="
    "1111111111111111111111111111111111111111"
    "1111111111111111111111111111111111111111"
    "1111111111111111111111111111111111111111"
    "1111111111111111111111111111111111111111"

    "1111111111111111111111111111111111111111"
    "1111111111111111111111111111111111111111"
    "1111111111111111111111111111111111111111"
    "1111111111111111111111111111111111111111."

    "1111111111111111111111111111111111111111"
    "1111111111111111111111111111111111111111"
    "1111111111111111111111111111111111111111"
    "1111111111111111111111111111111111111111"

    "1111111111111111111111111111111111111111"
    "1111111111111111111111111111111111111111"
    "1111111111111111111111111111111111111111"
    "1111111111111111111111111111111111111111,"
    " #<=-16"
    "\"";

  negate = 0; memset(sp, 0, sizeof sp);

  TEST_1(sip_prefs_parse(sp, &s, &negate));
  TEST(sp->sp_type, sp_range);
  TEST_D(sp->sp_range.spr_lower, DBL_MAX);
  TEST_D(sp->sp_range.spr_upper, DBL_MAX);
  TEST_1(sip_prefs_match(sp, sp));
  TEST_1(negate);

  TEST_1(sip_prefs_parse(sp, &s, &negate));
  TEST(sp->sp_type, sp_range);
  TEST_D(sp->sp_range.spr_lower, -DBL_MAX);
  TEST_D(sp->sp_range.spr_upper, -16.0);
  TEST_1(sip_prefs_match(sp, sp));
  TEST_1(!negate);

  error = 12;

  TEST_1(sip_prefs_matching("\"INVITE,MESSAGE,SUBSCRIBE\"",
			    "\"INVITE\"", &error));
  TEST_1(!sip_prefs_matching("\"INVITE,MESSAGE,SUBSCRIBE\"",
			     "\"BYE\"", &error));
  TEST(error, 12);
  TEST_1(sip_prefs_matching("\"INVITE,MESSAGE,SUBSCRIBE\"",
			     "\"invite\"", &error));
  TEST_1(sip_prefs_matching("\"!INVITE,MESSAGE,SUBSCRIBE\"",
			     "\"foo\"", &error));
  TEST_1(sip_prefs_matching("TRUE", "", &error));
  TEST_1(sip_prefs_matching("", "", &error));
  TEST_1(!sip_prefs_matching("FALSE", "", &error));
  TEST(error, 12);
  TEST_1(sip_prefs_matching("FALSE", "FALSE", &error));

  /* Lax when receiving... */
  TEST_1(sip_prefs_matching("\"FALSE\"", "FALSE", &error));  /* XXX */
  TEST_1(sip_prefs_matching("\"TRUE\"", "TRUE", &error));  /* XXX */

  TEST_1(!sip_prefs_matching("\"!INVITE\"", "\"INVITE\"", &error));
  TEST_1(!sip_prefs_matching("\"!INVITE\"", "\"invite\"", &error));
  TEST_1(sip_prefs_matching("\"!<INVITE>\"", "\"<invite>\"", &error));
  TEST_1(!sip_prefs_matching("\"INVITE\"", "\"!INVITE\"", &error));
  TEST_1(sip_prefs_matching("\"!INVITE\"", "\"INVITE,MESSAGE\"", &error));
  TEST_1(sip_prefs_matching("\"!INVITE,!MESSAGE\"",
			     "\"INVITE,MESSAGE\"", &error));
  TEST_1(sip_prefs_matching("\"!MESSAGE\"", "\"INVITE,MESSAGE\"", &error));
  TEST_1(!sip_prefs_matching("\"<foo>,<bar>\"",
			     "\"<FOO>,<BAR>\"", &error));
  TEST_1(!sip_prefs_matching("\"<FOO>,<BAR>\"", "\"foo,bar\"", &error));
  TEST_1(sip_prefs_matching("\"#=1\"", "\"#<=2\"", &error));
  TEST_1(sip_prefs_matching("\"#1:2\"", "\"#<=2\"", &error));
  TEST_1(!sip_prefs_matching("\"#1:2\"", "\"!#>=1,!#<=2\"", &error));
  TEST_1(!sip_prefs_matching("\"#=0,#=1\"", "\"<FOO>,<BAR>\"", &error));
  TEST(error, 12);

  error = 12;
  TEST_1(!sip_prefs_matching("\"<foo>,#=1\"", "\"<FOO>,<BAR>\"", &error));
  TEST(error, -1);

  error = 12;
  TEST_1(!sip_prefs_matching("\"<FOO>,<BAR>\"", "\"<foo>,#=1\"", &error));
  TEST(error, -1);

  error = 12;
  TEST_1(!sip_prefs_matching("\"<foo>,bar\"", "\"<FOO>,<BAR>\"", &error));
  TEST(error, -1);

  error = 12;
  TEST_1(!sip_prefs_matching("\"<FOO>,<BAR>\"", "\"<foo>,#12:12\"", &error));
  TEST(error, -1);

  {
    char const *params[] = {
      "methods=\"INVITE,MESSAGE,SUBSCRIBE\"",
      "events=\"presence,presence.winfo\"",
      "description=\"<PC>\"",
      "language=\"!en,de\"",
      "schemes=\"sip\"",
      "+res-x=\"#=640\"",
      "+res-y=\"#=480\"",
      NULL
    };

    TEST_1(sip_is_callerpref(params[0]));
    TEST_1(sip_is_callerpref(params[1]));
    TEST_1(sip_is_callerpref(params[2]));
    TEST_1(sip_is_callerpref(params[3]));
    TEST_1(sip_is_callerpref(params[4]));
    TEST_1(sip_is_callerpref(params[5]));
    TEST_1(sip_is_callerpref(params[6]));
    TEST_1(!sip_is_callerpref(params[7])); /* NULL */
    TEST_1(!sip_is_callerpref("method=\"foo\""));
    TEST_1(!sip_is_callerpref("+methods=\"foo\""));
  }

  TEST_1(m = sip_contact_make(home,
			      "<sip:1@domain>;video;audio;type=\"<video/H263>\","
			      "<sip:2@domain>;video=FALSE;audio;text,"
			      "<sip:3@domain>;text;audio=FALSE"));

  m0 = m, m1 = m->m_next, m2 = m->m_next->m_next;

  TEST_1(ac = sip_accept_contact_make(home, "*;type=\"<video/H263>\";video"));
  TEST_S(ac->cp_params[0], "type=\"<video/H263>\"");
  TEST_S(ac->cp_params[1], "video");
  TEST_P(ac->cp_params[2], NULL);

  TEST_1(sip_contact_accept(m0, ac, &S, &N, &error)); TEST(S, 2); TEST(N, 2);
  TEST_1(!sip_contact_accept(m1, ac, &S, &N, &error));
  TEST_1(sip_contact_accept(m2, ac, &S, &N, &error)); TEST(S, 0); TEST(N, 2);

  TEST_1(ac = sip_accept_contact_make(home, "sip:127.0.0.1:5060;video;audio"));
  TEST_S(ac->cp_params[0], "video");
  TEST_S(ac->cp_params[1], "audio");
  TEST_P(ac->cp_params[2], NULL);

  TEST_1(sip_contact_accept(m0, ac, &S, &N, &error)); TEST(S, 2); TEST(N, 2);
  TEST_1(!sip_contact_accept(m1, ac, &S, &N, &error));
  TEST_1(!sip_contact_accept(m2, ac, &S, &N, &error));

  TEST_1(ac = sip_accept_contact_make(home,
				      "Joe the Luser "
				      "<sip:127.0.0.1:5060>;video;audio"));
  TEST_S(ac->cp_params[0], "video");
  TEST_S(ac->cp_params[1], "audio");
  TEST_P(ac->cp_params[2], NULL);

  TEST_1(ac = sip_accept_contact_make(home,
				      "video;audio;explicit"));
  TEST_S(ac->cp_params[0], "video");
  TEST_S(ac->cp_params[1], "audio");
  TEST_S(ac->cp_params[2], "explicit");
  TEST_P(ac->cp_params[3], NULL);

  TEST_1(ac = sip_accept_contact_make(home,
				      "video = foo ;audio;explicit"));
  TEST_S(ac->cp_params[0], "video=foo");
  TEST_S(ac->cp_params[1], "audio");
  TEST_S(ac->cp_params[2], "explicit");
  TEST_P(ac->cp_params[3], NULL);

  TEST_1(ac = sip_accept_contact_make(home,
				      "video = \"bar\" ;audio;explicit"));
  TEST_S(ac->cp_params[0], "video=\"bar\"");
  TEST_S(ac->cp_params[1], "audio");
  TEST_S(ac->cp_params[2], "explicit");
  TEST_P(ac->cp_params[3], NULL);

  TEST_1(cp = sip_accept_contact_make(home,
				     "*;audio;video;require;explicit;q=1.0,"
				     "*;audio;require;q=0.8"
				     ));

  TEST_1(ac = sip_accept_contact_dup(home, cp));

  TEST_S(ac->cp_params[0], "audio");
  TEST_S(ac->cp_params[1], "video");
  TEST_S(ac->cp_params[2], "require");
  TEST_S(ac->cp_params[3], "explicit");
  TEST_S(ac->cp_params[4], "q=1.0");
  TEST_P(ac->cp_params[5], NULL);

  /* TEST_S(ac->cp_q, "1.0"); */
  TEST(ac->cp_require, 1);
  TEST(ac->cp_explicit, 1);

  TEST_1(sip_contact_accept(m0, ac, &S, &N, &error)); TEST(S, 2); TEST(N, 2);
  /* Explicit has short-circuit evaluation */
  TEST_1(!sip_contact_accept(m1, ac, &S, &N, &error));
  TEST_1(!sip_contact_accept(m2, ac, &S, &N, &error));

  TEST_1(ac = ac->cp_next);

  TEST_S(ac->cp_params[0], "audio");
  TEST_S(ac->cp_params[1], "require");
  TEST_S(ac->cp_params[2], "q=0.8");
  TEST_P(ac->cp_params[3], NULL);

  TEST(ac->cp_explicit, 0);

  TEST_1(sip_contact_accept(m0, ac, &S, &N, &error)); TEST(S, 1); TEST(N, 1);
  TEST_1(sip_contact_accept(m1, ac, &S, &N, &error)); TEST(S, 1); TEST(N, 1);
  TEST_1(!sip_contact_accept(m2, ac, &S, &N, &error));

  TEST_P(ac->cp_next, NULL);

  TEST_1(rejc = sip_reject_contact_make(home,
					"*;type=\"<video/H263>\";video=TRUE"));
  TEST_S(rejc->cp_params[0], "type=\"<video/H263>\"");
  TEST_S(rejc->cp_params[1], "video=TRUE");

  TEST_1(sip_contact_reject(m0, rejc));
  TEST_1(!sip_contact_reject(m1, rejc));
  TEST_1(!sip_contact_reject(m2, rejc));

  TEST_1(!sip_contact_immune(m0));
  m0 = sip_contact_immunize(home, m0); TEST_1(m0);
  TEST_1(sip_contact_immune(m0));

  TEST_1(!sip_contact_immune(m1));
  m1 = sip_contact_immunize(home, m1); TEST_1(m1);
  TEST_1(sip_contact_immune(m1));

  TEST_1(!sip_contact_immune(m2));
  m2 = sip_contact_immunize(home, m2); TEST_1(m2);
  TEST_1(sip_contact_immune(m2));

  m = sip_contact_make(home, "<sip:test.domain>;+test;+audio");
  TEST_1(!sip_contact_immune(m));
  m1 = sip_contact_immunize(home, m); TEST_1(m1);
  TEST_1(sip_contact_immune(m1));

  TEST_S(m->m_params[0], "+test");
  TEST_S(m->m_params[1], "+audio");
  TEST_P(m->m_params[2], NULL);

  TEST_S(m1->m_params[0], "+audio");
  TEST_P(m1->m_params[1], NULL);

  TEST_1(ac = sip_accept_contact_make(home, "*;q=0.9;require;explicit"));
  /* TEST_S(ac->cp_q, "0.9"); */
  TEST_1(ac->cp_require);
  TEST_1(ac->cp_explicit);

  TEST(msg_header_remove_param(ac->cp_common, "Q"), 1);
  /* TEST(ac->cp_q, NULL); */
  TEST(msg_header_remove_param(ac->cp_common, "require="), 1);
  TEST(ac->cp_require, 0);
  TEST(msg_header_remove_param(ac->cp_common, "require="), 0);
  TEST(ac->cp_require, 0);
  TEST(msg_header_remove_param(ac->cp_common, "explicit=true"), 1);
  TEST(ac->cp_explicit, 0);
  TEST(msg_header_replace_param(home, ac->cp_common, "explicit=true"), 0);
  TEST(ac->cp_explicit, 1);

  su_home_zap(home);

  END();
}


static int test_callerpref_scoring(void)
{
  sip_accept_contact_t *ac;
  sip_reject_contact_t *rejc;
  su_home_t *home;
  int S;
  sip_contact_t *m, *m1, *m2, *m3, *m4, *m5;

  char const contact[] =
    "sip:u1@h.example.com;audio;video;methods=\"INVITE,BYE\";q=0.2,"
    "sip:u2@h.example.com;audio=\"FALSE\";"
    "methods=\"INVITE\";actor=\"msg-taker\";q=0.2,"
    "sip:u3@h.example.com;audio;actor=\"msg-taker\";"
    "methods=\"INVITE\";video;q=0.3,"
    "sip:u4@h.example.com;audio;methods=\"INVITE,OPTIONS\";q=0.2,"
    "sip:u5@h.example.com;q=0.5";

  char const reject[] =
    "*;actor=\"msg-taker\";video";

  char const accept[] =
    "*;audio;require,"
    "*;video;explicit,"
    "*;methods=\"BYE\";class=\"business\";q=1.0";

  BEGIN();

  TEST_1(home = su_home_new(sizeof *home));

  TEST_1(m = sip_contact_make(home, contact));
  m1 = m, m2 = m->m_next, m3 = m->m_next->m_next, m4 = m->m_next->m_next->m_next,
    m5 = m->m_next->m_next->m_next->m_next;
  TEST_1(rejc = sip_reject_contact_make(home, reject));
  TEST_1(ac = sip_accept_contact_make(home, accept));

  S = sip_contact_score(m1, ac, rejc); TEST(S, 1000 * 5 / 6 + 1);
  S = sip_contact_score(m2, ac, rejc); TEST(S, 0);
  S = sip_contact_score(m3, ac, rejc); TEST_1(S < 0);
  S = sip_contact_score(m4, ac, rejc); TEST(S, 1000 / 2);
  S = sip_contact_score(m5, ac, rejc); TEST(S, 1000);

  su_home_zap(home);

  END();
}

static int test_reason(void)
{
  sip_reason_t *re;
  su_home_t *home;
  msg_t *msg;
  sip_t *sip;

  BEGIN();

  TEST_1(home = su_home_create());

  TEST_1((re = sip_reason_make(home, "SIP;cause=200;text=\"Ok\"")));
  TEST_S(re->re_protocol, "SIP");
  TEST_S(re->re_cause, "200");
  TEST_S(re->re_text, "\"Ok\"");

  msg = read_message(MSG_DO_EXTRACT_COPY,
    "BYE sip:foo@bar SIP/2.0\r\n"
    "To: <sip:foo@bar>;tag=deadbeef\r\n"
    "From: <sip:bar@foo>;\r\n"
    "Call-ID: 0ha0isndaksdj@10.1.2.3\r\n"
    "CSeq: 8 SUBSCRIBE\r\n"
    "Via: SIP/2.0/UDP 135.180.130.133\r\n"
    "Reason: SIP ;cause=200 ;text=\"Call completed elsewhere\"\r\n"
    "Reason: Q.850 ;cause=16 ;text=\"Terminated\"\r\n ,,"
    "SIP ; cause = 600 ;text\t\r\n =  \"Busy Everywhere\" \t \r\n"
    "Reason: SIP ;cause=580 ;text=\"Precondition Failure\"\r\n"
    "Content-Length: 0\r\n"
    "\r\n");

  sip = sip_object(msg);

  TEST_1(msg);
  TEST_1(sip);
  TEST_1(re = sip->sip_reason);
  TEST_S(re->re_protocol, "SIP");
  TEST_S(re->re_cause, "200");
  TEST_S(re->re_text, "\"Call completed elsewhere\"");
  TEST_1(re = re->re_next);
  TEST_S(re->re_protocol, "Q.850");
  TEST_S(re->re_cause, "16");
  TEST_S(re->re_text, "\"Terminated\"");
  TEST_1(re = re->re_next);
  TEST_S(re->re_protocol, "SIP");
  TEST_S(re->re_cause, "600");
  TEST_S(re->re_text, "\"Busy Everywhere\"");
  TEST_1(re = re->re_next);
  TEST_S(re->re_protocol, "SIP");
  TEST_S(re->re_cause, "580");
  TEST_S(re->re_text, "\"Precondition Failure\"");
  TEST_1(!(re = re->re_next));

  TEST_1(re = sip->sip_reason);
  TEST(msg_header_replace_param(home, re->re_common, "cause=202"), 1);
  TEST_S(re->re_cause, "202");
  TEST(msg_header_replace_param(home, re->re_common, "text=\"foo\""), 1);
  TEST_S(re->re_text, "\"foo\"");
  TEST(msg_header_remove_param(re->re_common, "cause=444"), 1);
  TEST_P(re->re_cause, NULL);
  TEST(msg_header_remove_param(re->re_common, "text=\"bar\""), 1);
  TEST_P(re->re_text, NULL);
  TEST(msg_header_remove_param(re->re_common, "cause=444"), 0);
  TEST(msg_header_remove_param(re->re_common, "text=\"bar\""), 0);

  /* Not a token */
  TEST_1(!sip_reason_make(home, "\"nSIP\";cause=200;text=\"Ok\""));
  /* Empty list */
  TEST_1(!sip_reason_make(home, ","));
  /* no protocol token */
  TEST_1(!sip_reason_make(home, "cause=16;text=\"call cleared\""));

  /* Empty parameter */
  TEST_1(!sip_reason_make(home, "SIP;cause=200;;text=\"Ok\""));
  /* no semicolon after token */
  TEST_1(!sip_reason_make(home, "SIP cause=200;text=\"Ok\""));
  /* extra semicolon after parameters */
  TEST_1(!sip_reason_make(home, "SIP ;cause=200;text=\"Ok\";"));

  su_home_unref(home);
  msg_destroy(msg), msg = NULL;

  END();
}

static int test_warning(void)
{
  sip_warning_t *w;
  su_home_t *home;

  BEGIN();

  TEST_1(home = su_home_create());

  TEST_1((w = sip_warning_make(home,
			       "399 host:5060 \"Ok\", "
			       "399 [::1]:39999 \"foo\\\" bar\"")));
  TEST(w->w_code, 399);
  TEST_S(w->w_host, "host");
  TEST_S(w->w_port, "5060");
  TEST_S(w->w_text, "Ok");
  TEST_1(w = w->w_next);

  TEST(w->w_code, 399);
  TEST_S(w->w_host, "[::1]");
  TEST_S(w->w_port, "39999");
  TEST_S(w->w_text, "foo\" bar");
  TEST_1(w->w_next == NULL);

  TEST_S(sip_header_as_string(home, (sip_header_t *)w),
	 "399 [::1]:39999 \"foo\\\" bar\"");

  su_home_unref(home);

  END();
}


static int test_sec_ext(void)
{
  su_home_t *home;

  sip_security_client_t *sac;
  sip_security_server_t *sas;
  sip_security_verify_t *sav;
  sip_privacy_t *priv;

  msg_t *msg;
  sip_t *sip;

  BEGIN();

  msg = read_message(MSG_DO_EXTRACT_COPY,
    "BYE sip:foo@bar SIP/2.0\r\n"
    "To: <sip:foo@bar>;tag=deadbeef\r\n"
    "From: <sip:bar@foo>;\r\n"
    "Call-ID: 0ha0isndaksdj@10.1.2.3\r\n"
    "CSeq: 8 SUBSCRIBE\r\n"
    "Via: SIP/2.0/UDP 135.180.130.133\r\n"
    "Content-Length: 0\r\n"
    "\r\n");

  sip = sip_object(msg);

  TEST_1(home = msg_home(msg));

  TEST_1(sac = sip_security_client_make(home, "digest;q=0.5,ipsec-3gpp"));
  TEST_S(sac->sa_mec, "digest");
  TEST_S(sac->sa_q, "0.5");
  TEST_1(sac = sac->sa_next);
  TEST_S(sac->sa_mec, "ipsec-3gpp");

  TEST_1((sas = sip_security_server_make(home, "digest;q=0.5")));
  TEST_S(sas->sa_mec, "digest");
  TEST_S(sas->sa_q, "0.5");

  TEST_1((sav = sip_security_verify_make(home, "digest;q=0.5")));
  TEST_S(sav->sa_mec, "digest");
  TEST_S(sav->sa_q, "0.5");

  TEST_1((sav = sip_security_verify_make
	  (home, "digest;q=0.1;d-alg=SHA1;d-qop=auth;"
	   "d-ver=\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"")));
  TEST_S(sav->sa_mec, "digest");
  TEST_S(sav->sa_q, "0.1");
  TEST_S(sav->sa_d_alg, "SHA1");
  TEST_S(sav->sa_d_qop, "auth");
  TEST_S(sav->sa_d_ver, "\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"");

  /* Test for accepting liberally.. */
  TEST_1(priv = sip_privacy_make(home, "header,media"));
  TEST_1(priv = sip_privacy_make(home, ";header;media"));

  TEST_1(!(priv = sip_privacy_make(home, "none explicit")));

  TEST_1((priv = sip_privacy_make(home, "header;media")));
  TEST_1(priv->priv_values);
  TEST_S(priv->priv_values[0], "header");
  TEST_S(priv->priv_values[1], "media");

  msg_destroy(msg);

  END();
}


static int test_utils(void)
{
  sip_from_t *f;
  su_home_t *home;
  sip_security_server_t *secs;
  sip_security_verify_t *secv;
  msg_param_t d_ver = NULL;

  BEGIN();

  TEST_1(home = su_home_new(sizeof *home));
  TEST_1(f = sip_from_make(home, "<sip:u:p@h.com:5555"
			   ";ttl=1;user=IP;maddr=::1;lr=TRUE;transport=TCP"
			   ";test=1?accept-contact=*;audio;video;explicit>"));
  TEST_1(sip_aor_strip(f->a_url) == 0);
  TEST_1(f->a_url->url_port == NULL);
  TEST_S(f->a_url->url_params, "user=IP;test=1");
  TEST_1(f->a_url->url_headers == NULL);

  TEST_1(f = sip_from_make(home, "<sip:u:p@h.com:5555;ttl=1>"));
  TEST_1(sip_aor_strip(f->a_url) == 0);
  TEST_1(f->a_url->url_params == NULL);

  TEST_1(f = sip_from_make(home, "<sip:u:p@h.com:5555;test=1;ttl=1>"));
  TEST_1(sip_aor_strip(f->a_url) == 0);
  TEST_S(f->a_url->url_params, "test=1");

  TEST_1(f = sip_from_make(home, "<sip:u:p@h.com:5555;;test=1;>"));
  TEST_1(sip_aor_strip(f->a_url) == 0);
  TEST_S(f->a_url->url_params, "test=1");

  TEST_1(secs = sip_security_server_make(home, "Digest"));
  TEST_1(secv =
	 sip_security_verify_make(home,
				  "Digest;"
				  "d-ver=\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\""));

  TEST_1(sip_security_verify_compare(secs, secv, &d_ver) == 0);
  TEST_S(d_ver, "\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"");

  TEST_1(secs =
	 sip_security_server_make(home,
				  "TLS;q=1,"
				  "Digest;q=0.1;"
				  "d-alg=MD5;d-qop=\"auth,auth-int\","
				  "Digest;d-alg=AKA-MD5;q=0.9"
				  ));
  TEST_1(secv =
	 sip_security_verify_make(home,
				  "TLS;q=1,"
				  "Digest ; q = 0.1;"
				  "d-ver=\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\";"
				  "d-alg = MD5 ; d-qop = \"auth,auth-int\","
				  "Digest ; d-alg=AKA-MD5;q=0.9"));

  TEST_1(sip_security_verify_compare(secs, secv, &d_ver) == 0);
  TEST_S(d_ver, "\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"");

  TEST_1(sip_security_verify_compare(secs->sa_next, secv, &d_ver) != 0);
  TEST_1(sip_security_verify_compare(secs, secv->sa_next, &d_ver) != 0);
  d_ver = "kuik";
  TEST_1(sip_security_verify_compare(secs->sa_next, secv->sa_next, &d_ver)
	 == 0);
  TEST_S(d_ver, "\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"");
  d_ver = "kuik";
  TEST_1(sip_security_verify_compare(secs->sa_next->sa_next,
				     secv->sa_next->sa_next, &d_ver)
	 == 0);
  TEST_1(d_ver == NULL);

  END();
}

void usage(int exitcode)
{
  fprintf(stderr,
	  "usage: %s [-v] [-a]\n",
	  name);
  exit(exitcode);
}

char *lastpart(char *path)
{
  if (strchr(path, '/'))
    return strrchr(path, '/') + 1;
  else
    return path;
}

int main(int argc, char *argv[])
{
  int retval = 0;
  int i;

  name = lastpart(argv[0]);  /* Set our name */

  for (i = 1; argv[i]; i++) {
    if (strcmp(argv[i], "-v") == 0)
      tstflags |= tst_verbatim;
    else if (strcmp(argv[i], "-a") == 0)
      tstflags |= tst_abort;
    else
      usage(1);
  }

#if HAVE_OPEN_C
  tstflags |= tst_verbatim;
#endif

  retval |= test_identity(); fflush(stdout);

  if (test_mclass == NULL)
    test_mclass = msg_mclass_clone(sip_default_mclass(), 0, 0);

  retval |= parser_test(); fflush(stdout);

  retval |= test_url_headers(); fflush(stdout);
  retval |= test_manipulation(); fflush(stdout);
  retval |= test_methods(); fflush(stdout);
  retval |= test_basic(); fflush(stdout);
  retval |= test_sip_msg_class(test_mclass); fflush(stdout);
  retval |= test_encoding(); fflush(stdout);
  retval |= test_events(); fflush(stdout);
  retval |= test_reason(); fflush(stdout);
  retval |= tag_test(); fflush(stdout);
  retval |= parser_tag_test(); fflush(stdout);
  retval |= response_phrase_test(); fflush(stdout);

  retval |= sip_header_test(); fflush(stdout);
  retval |= test_bad_packet(); fflush(stdout);
  retval |= test_sip_list_header(); fflush(stdout);
  retval |= test_prack(); fflush(stdout);
  retval |= test_accept(); fflush(stdout);
  retval |= test_content_disposition(); fflush(stdout);
  retval |= test_features(); fflush(stdout);
  retval |= test_retry_after(); fflush(stdout);
  retval |= test_session_expires(); fflush(stdout);
  retval |= test_min_se(); fflush(stdout);
  retval |= test_refer(); fflush(stdout);
  retval |= test_route(); fflush(stdout);
  retval |= test_request_disposition(); fflush(stdout);
  retval |= test_content_type(); fflush(stdout);
  retval |= test_caller_prefs(); fflush(stdout);
  retval |= test_callerpref_scoring(); fflush(stdout);
  retval |= test_warning(); fflush(stdout);
  retval |= test_www_authenticate(); fflush(stdout);
  retval |= test_sec_ext(); fflush(stdout);

  retval |= test_utils(); fflush(stdout);

#if HAVE_OPEN_C
  sleep(5);
#endif

  return retval;
}

