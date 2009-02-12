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

/**@CFILE torture_url.c Test functions for url parser
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Aug 21 15:18:26 2001 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <assert.h>

#include "sofia-sip/url.h"
#include "sofia-sip/url_tag.h"

static int tstflags = 0;

#define TSTFLAGS tstflags

#include <sofia-sip/tstdef.h>

char const name[] = "torture_url";

unsigned char hash1[16], hash2[16];

/* test unquoting and canonizing */
int test_quote(void)
{
  su_home_t home[1] = { SU_HOME_INIT(home) };
  url_t *u;
  char s[] = "%73ip:q%74est%01:%01%02%00@host%2enokia.com;%70aram=%01%02";
  char c[] = "sip:qtest%01:%01%02%00@host.nokia.com;param=%01%02";
  char *d;

#define RESERVED        ";/?:@&=+$,"
#define DELIMS          "<>#%\""
#define UNWISE		"{}|\\^[]`"
#define EXCLUDED	RESERVED DELIMS UNWISE

  char escaped[1 + 3 * 23 + 1];

#define UNRESERVED    "ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
                      "abcdefghijklmnopqrstuvwxyz" \
                      "0123456789" \
                      "-_.!~*'()"

  char unreserved[26 + 26 + 10 + 9 + 1];

  BEGIN();

  d = url_as_string(home, (url_t *)"sip:joe@example.com");
  TEST_S(d, "sip:joe@example.com");

  TEST(strlen(EXCLUDED), 23);
  TEST(strlen(UNRESERVED), 71);

  TEST_1(!url_reserved_p("foo"));
  TEST_1(!url_reserved_p(""));
  TEST_1(url_reserved_p("foobar:bar"));

  TEST_SIZE(url_esclen("a" EXCLUDED, ""),
	    1 + strlen(RESERVED) + 3 * strlen(DELIMS UNWISE));
  TEST_SIZE(url_esclen("a" EXCLUDED, DELIMS UNWISE),
	    1 + strlen(RESERVED) + 3 * strlen(DELIMS UNWISE));
  TEST_SIZE(url_esclen("a" EXCLUDED, EXCLUDED), 1 + 3 * strlen(EXCLUDED));
  TEST_SIZE(url_esclen("a" EXCLUDED, NULL), 1 + 3 * strlen(EXCLUDED));

  TEST_S(url_escape(escaped, "a" EXCLUDED, NULL),
	 "a%3B%2F%3F%3A%40%26%3D%2B%24%2C"
	 "%3C%3E%23%25%22"
	 "%7B%7D%7C%5C%5E%5B%5D%60");
  TEST_S(url_unescape(escaped, escaped), "a" EXCLUDED);

  TEST_SIZE(url_esclen(UNRESERVED, NULL), strlen(UNRESERVED));
  TEST_S(url_escape(unreserved, UNRESERVED, NULL), UNRESERVED);
  TEST_S(url_unescape(unreserved, UNRESERVED), UNRESERVED);

  d = "%53ip:%75@%48";		/* Sip:u@H */
  u = url_hdup(home, (url_t *)d); TEST_1(u);
  url_digest(hash1, sizeof(hash1), u, NULL);
  url_digest(hash2, sizeof(hash2), (url_t const *)d, NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);

  d = "sip:u@h";
  u = url_hdup(home, (url_t *)d); TEST_1(u);
  url_digest(hash1, sizeof(hash1), u, NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);
  url_digest(hash2, sizeof(hash2), (url_t const *)d, NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);

  u = url_hdup(home, (url_t *)s); TEST_1(u);
  d = url_as_string(home, u); TEST_1(d);
  TEST_S(d, c);

  d = "sip:&=+$,;?/:&=+$,@[::1]:56001;param=+$,/:@&;another=@%40%2F"
    "?header=" RESERVED "&%3b%2f%3f%3a%40%26%3d%2b%24%2c";
  u = url_hdup(home, (url_t *)d); TEST_1(u);
  TEST_S(u->url_user, "&=+$,;?/");
  TEST_S(u->url_host, "[::1]");
  TEST_S(u->url_params, "param=+$,/:@&;another=@%40/");
  TEST_S(u->url_headers, "header=" RESERVED "&%3B%2F%3F%3A%40%26%3D%2B%24%2C");
  url_digest(hash1, sizeof(hash1), u, NULL);
  url_digest(hash2, sizeof(hash2), (url_t const *)d, NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);

  u = url_hdup(home, (url_t *)s); TEST_1(u);
  d = url_as_string(home, u); TEST_1(d);
  TEST_S(d, c);

  d = "http://&=+$,;:&=+$,;@host:8080/foo%2F%3B%3D"
    ";param=+$,%2f%3b%3d/bar;param=:@&;another=@"
    "?query=" RESERVED;
  u = url_hdup(home, (url_t *)d); TEST_1(u);
  TEST_S(u->url_user, "&=+$,;"); TEST_S(u->url_password, "&=+$,;");
  TEST_S(u->url_path, "foo%2F%3B%3D;param=+$,%2F%3B%3D/bar;param=:@&;another=@");
  url_digest(hash1, sizeof(hash1), u, NULL);
  url_digest(hash2, sizeof(hash2), (url_t const *)d, NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);

  u = url_hdup(home, (url_t *)s); TEST_1(u);
  d = url_as_string(home, u); TEST_1(d);
  TEST_S(d, c);

  url_digest(hash1, sizeof(hash1), u, NULL);
  url_digest(hash2, sizeof(hash2), (url_t const *)s, NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);

  url_digest(hash2, sizeof(hash2), (url_t const *)c, NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);

  END();
}


int test_any(void)
{
  /* Test any (*) urls */
  url_t any[1] = { URL_INIT_AS(any) };
  su_home_t home[1] = { SU_HOME_INIT(home) };
  url_t *u, url[1];
  char *tst;

  BEGIN();

  TEST_S(url_scheme(url_any), "*");
  TEST_S(url_scheme(url_mailto), "mailto");
  TEST_S(url_scheme(url_im), "im");
  TEST_S(url_scheme(url_cid), "cid");
  TEST_S(url_scheme(url_msrp), "msrp");
  TEST_S(url_scheme(url_msrps), "msrps");

  TEST_1(tst = su_strdup(home, "*"));
  TEST(url_d(url, tst), 0);
  TEST(url_cmp(any, url), 0);
  TEST(url->url_type, url_any);
  TEST_1(u = url_hdup(home, url));
  TEST(u->url_type, url_any);
  TEST(url_cmp(any, u), 0);

  url_digest(hash1, sizeof(hash1), url, NULL);
  url_digest(hash2, sizeof(hash2), (url_t *)"*", NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);

  {
    char buf[6];

    TEST_1(u = url_hdup(home, (void *)"error"));
    TEST_SIZE(url_xtra(u), 6);
    TEST_SIZE(url_dup(buf, 6, url, u), 6);
    TEST_S(buf, "error");
  }

  {
    TEST_1(u = url_hdup(home, (void *)"scheme:test"));
    TEST(u->url_type, url_unknown);
  }

  {
    TEST_1(u = url_hdup(home, (void *)"*;param=foo?query=bar"));
    TEST(u->url_type, url_unknown);
    TEST_S(u->url_host, "*");
    TEST_S(u->url_params, "param=foo");
    TEST_S(u->url_headers, "query=bar");
  }

  {
    TEST_1(u = url_hdup(home, (void *)"#foo"));
    TEST(u->url_type, url_unknown);
    TEST_S(u->url_fragment, "foo");
  }

  {
    url_t u[1];
    char b2[6] = "";

    memset(u, 0xff, sizeof u);
    TEST(url_d(u, b2), 0);
    TEST(u->url_type, url_unknown);
  }

  su_home_deinit(home);

  END();
}

int test_sip(void)
{
  /* sip urls */
  su_home_t home[1] = { SU_HOME_INIT(home) };
  url_t sip[1] = { URL_INIT_AS(sip) };
  url_t *u, url[1];
  char *tst, *s;
  char sipurl0[] =
    "sip:pekka%2Epessi@nokia%2Ecom;method=%4D%45%53%53%41%47%45"
    "?body=CANNED%20MSG";
  char sipurl[] =
    "sip:user:pass@host:32;param=1"
    "?From=foo@bar&To=bar@baz#unf";
  char sip2url[] =
    "sip:user/path;tel-param:pass@host:32;param=1%3d%3d1"
    "?From=foo@bar&To=bar@baz#unf";
  char sip2[sizeof(sipurl) + 32];
  char sipsurl[] =
    "sips:user:pass@host:32;param=1"
    "?From=foo@bar&To=bar@baz#unf";
  size_t i, j;
  url_t *a, *b;

  BEGIN();

  TEST_S(url_scheme(url_sip), "sip");
  TEST_S(url_scheme(url_sips), "sips");

  memset(url, 255, sizeof url);

  TEST(url_d(url, sipurl0), 0);

  TEST(url->url_type, url_sip);
  TEST(url->url_root, 0);
  TEST_S(url->url_scheme, "sip");
  TEST_S(url->url_user, "pekka.pessi");
  TEST_P(url->url_password, NULL);
  TEST_S(url->url_host, "nokia.com");
  TEST_P(url->url_port, NULL);
  TEST_P(url->url_path, NULL);
  TEST_S(url->url_params, "method=MESSAGE");
  TEST_S(url->url_headers, "body=CANNED%20MSG");
  TEST_P(url->url_fragment, NULL);

  TEST_S(url_query_as_header_string(home, url->url_headers),
	 "\n\nCANNED MSG");

  sip->url_user = "user";
  sip->url_password = "pass";
  sip->url_host = "host";
  sip->url_port = "32";
  sip->url_params = "param=1";
  sip->url_headers = "From=foo@bar&To=bar@baz";
  sip->url_fragment = "unf";

  memset(url, 255, sizeof url);

  TEST_1(tst = su_strdup(home, sipurl));
  TEST_1(url_d(url, tst) == 0);
  TEST_1(url_cmp(sip, url) == 0);
  TEST(url->url_type, url_sip);
  TEST_1(u = url_hdup(home, url));
  TEST(u->url_type, url_sip);
  TEST_1(url_cmp(sip, u) == 0);
  TEST(url_e(sip2, sizeof(sip2), u), strlen(sipurl));
  TEST_1(strcmp(sip2, sipurl) == 0);
  TEST_SIZE(snprintf(sip2, sizeof(sip2), URL_PRINT_FORMAT,
		     URL_PRINT_ARGS(sip)), strlen(sipurl));
  TEST_1(strcmp(sip2, sipurl) == 0);

  url_digest(hash1, sizeof(hash1), url, NULL);
  url_digest(hash2, sizeof(hash2), (url_t const *)sipurl, NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);

  TEST_1(tst = su_strdup(home, sip2url));
  TEST_1(url_d(url, tst) == 0);
  TEST_S(url->url_user, "user/path;tel-param");
  TEST_S(url->url_params, "param=1%3D%3D1");

  TEST_S(url_query_as_header_string(home, url->url_headers),
	 "From:foo@bar\nTo:bar@baz");

  url_digest(hash1, sizeof(hash1), url, NULL);
  url_digest(hash2, sizeof(hash2), (url_t *)sip2url, NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);

  sip->url_type = url_sips; sip->url_scheme = "sips";

  TEST_1(tst = su_strdup(home, sipsurl));
  TEST_1(url_d(url, tst) == 0);
  TEST_1(url_cmp(sip, url) == 0);
  TEST(url->url_type, url_sips);

  /* Test url_dup() */
  for (i = 0; i <= sizeof(sipsurl); i++) {
    char buf[sizeof(sipsurl) + 1];
    url_t dst[1];

    buf[i] = '\377';
    TEST_SIZE(url_dup(buf, i, dst, url), sizeof(sipsurl) - 1 - strlen("sips"));
    TEST(buf[i], '\377');
  }

  url_digest(hash1, sizeof(hash1), url, NULL);
  url_digest(hash2, sizeof(hash2), (url_t *)sipsurl, NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);

  u = url_hdup(home, (url_t*)"SIP:test@127.0.0.1:55"); TEST_1(u);
  TEST(u->url_type, url_sip);

  u = url_hdup(home, (url_t*)"SIP:test@127.0.0.1:"); TEST_1(u);
  TEST(u->url_type, url_sip);

  TEST_P(url_hdup(home, (url_t*)"sip:test@127.0.0.1::55"), NULL);
  TEST_P(url_hdup(home, (url_t*)"sip:test@127.0.0.1:55:"), NULL);
  TEST_P(url_hdup(home, (url_t*)"sip:test@127.0.0.1:sip"), NULL);

  u = url_hdup(home, (url_t*)"SIP:#**00**#;foo=/bar@127.0.0.1"); TEST_1(u);
  TEST(u->url_type, url_sip);
  TEST_S(u->url_user, "#**00**#;foo=/bar");

  TEST_1(!url_hdup(home, (url_t*)"SIP:#**00**#;foo=/bar@#127.0.0.1"));
  TEST_1(!url_hdup(home, (url_t*)"SIP:#**00**#;foo=/bar;127.0.0.1"));

  for (i = 32; i <= 256; i++) {
    char pu[512];
    char param[512];

    for (j = 0; j < i; j++)
      param[j] = 'x';
    param[j] = '\0';
    memcpy(param, "x=", 2);

    snprintf(pu, sizeof(pu), "sip:test@host;%s", param);
    u = url_hdup(home, (url_t*)pu); TEST_1(u);
    s = url_as_string(home, u);
    TEST_S(pu, s);
  }

  s = su_strdup(home, "ttl;transport=tcp;ttl=15;ttl=;method=INVITE;ttl");
  TEST_1(s);
  s = url_strip_param_string(s, "ttl");
  TEST_S(s, "transport=tcp;method=INVITE");

  u = url_hdup(home, (void*)"sip:u:p@host:5060;maddr=127.0.0.1;transport=tcp");
  TEST_1(u);
  TEST_1(url_have_transport(u));
  TEST_1(url_strip_transport(u));
  TEST_P(u->url_params, NULL);
  TEST_1(!url_have_transport(u));

  u = url_hdup(home, (void*)"sip:u:p@host:5060;user=phone;ttl=1;isfocus");
  TEST_1(u);
  TEST_1(url_have_transport(u));
  TEST_1(url_strip_transport(u));
  TEST_S(u->url_params, "user=phone;isfocus");
  TEST_1(!url_have_transport(u));

  u = url_hdup(home, (void*)"sip:u:p@host:5060;maddr=127.0.0.1;user=phone");
  TEST_1(u);
  TEST_1(url_have_transport(u));
  TEST_1(url_strip_transport(u));
  TEST_S(u->url_params, "user=phone");
  TEST_1(!url_have_transport(u));

  u = url_hdup(home, (void*)"sip:u:p@host:5060;user=phone;transport=tcp");
  TEST_1(u);
  TEST_1(url_have_transport(u));
  TEST_1(url_strip_transport(u));
  TEST_S(u->url_params, "user=phone");
  TEST_1(!url_have_transport(u));

  u = url_hdup(home, (void*)"sip:u:p@host;user=phone;;");
  TEST_1(u);
  /* We don't have transport params */
  TEST_1(!url_have_transport(u));
  /* ...but we still strip empty params */
  TEST_1(url_strip_transport(u));
  TEST_S(u->url_params, "user=phone");
  TEST_1(!url_have_transport(u));

  u = url_hdup(home, (void*)"sip:u:p@host:5060;ttl=1;isfocus;transport=udp;");
  TEST_1(u);
  TEST_1(url_have_transport(u));
  TEST_1(url_strip_transport(u));
  TEST_S(u->url_params, "isfocus");
  TEST_1(!url_have_transport(u));

  u = url_hdup(home, (void *)"sip:%22foo%22@172.21.55.55:5060");
  TEST_1(u);
  TEST_S(u->url_user, "%22foo%22");

  a = url_hdup(home, (void *)"sip:172.21.55.55:5060");
  b = url_hdup(home, (void *)"sip:172.21.55.55");
  TEST_1(a); TEST_1(b);
  TEST_1(url_cmp(a, b) == 0);
  TEST(url_cmp_all(a, b), 0);

  a = url_hdup(home, (void *)"sips:172.21.55.55:5060");
  b = url_hdup(home, (void *)"sips:172.21.55.55");
  TEST_1(a); TEST_1(b);
  TEST_1(url_cmp(a, b) != 0);
  TEST_1(url_cmp_all(a, b) < 0);

  a = url_hdup(home, (void *)"sips:172.21.55.55:5061");
  b = url_hdup(home, (void *)"sips:172.21.55.55");
  TEST_1(a); TEST_1(b);
  TEST_1(url_cmp(a, b) == 0);
  TEST(url_cmp_all(a, b), 0);

  a = url_hdup(home, (void *)"sip:my.domain:5060");
  b = url_hdup(home, (void *)"sip:my.domain");
  TEST_1(a); TEST_1(b);
  TEST_1(url_cmp(a, b) > 0);
  TEST_1(url_cmp_all(a, b) > 0);

  a = url_hdup(home, (void *)"sips:my.domain:5061");
  b = url_hdup(home, (void *)"sips:my.domain");
  TEST_1(a); TEST_1(b);
  TEST_1(url_cmp(a, b) > 0);
  TEST_1(url_cmp_all(a, b) > 0);

  a = url_hdup(home, (void *)"sip:my.domain");
  b = url_hdup(home, (void *)"SIP:MY.DOMAIN");
  TEST_1(a); TEST_1(b);
  TEST_1(url_cmp(a, b) == 0);
  TEST_1(url_cmp_all(a, b) == 0);

  su_home_deinit(home);

  END();
}

int test_wv(void)
{
  /* wv urls */
  su_home_t home[1] = { SU_HOME_INIT(home) };
  url_t wv[1] = { URL_INIT_AS(wv) };
  url_t *u, url[1];
  char *tst;
  char wvurl[] = "wv:+12345678@imps.com";
  char wv2[sizeof(wvurl) + 32];

  BEGIN();

  TEST_S(url_scheme(url_wv), "wv");

  wv->url_user = "+12345678";
  wv->url_host = "imps.com";

  TEST_1(tst = su_strdup(home, wvurl));
  TEST_1(url_d(url, tst) == 0);
  TEST_1(url_cmp(wv, url) == 0);
  TEST_1(url_cmp(url_hdup(home, (void *)"wv:+12345678@imps.com"), url) == 0);
  TEST(url->url_type, url_wv);
  TEST_1(u = url_hdup(home, url));
  TEST(u->url_type, url_wv);
  TEST_1(url_cmp(wv, u) == 0);
  TEST_SIZE(url_e(wv2, sizeof(wv2), u), strlen(wvurl));
  TEST_1(strcmp(wv2, wvurl) == 0);
  TEST_SIZE(snprintf(wv2, sizeof(wv2), URL_PRINT_FORMAT,
		     URL_PRINT_ARGS(wv)), strlen(wvurl));
  TEST_1(strcmp(wv2, wvurl) == 0);

  url_digest(hash1, sizeof(hash1), url, NULL);
  url_digest(hash2, sizeof(hash2), (url_t *)wvurl, NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);

  TEST_1(u = url_hdup(home, (void*)"wv:/managers@imps.com"));
  TEST_S(u->url_user, "/managers");

  su_home_deinit(home);

  END();
}


int test_tel(void)
{
  /* tel urls: RFC 3906 */
  su_home_t home[1] = { SU_HOME_INIT(home) };
  url_t tel[1] = { URL_INIT_AS(tel) };
  url_t *u, url[1];
  char *tst;
  char telurl[] =
    "tel:+12345678"
    ";param=1;param=2"
    "?From=foo@bar&To=bar@baz#unf";
  char tel2[sizeof(telurl) + 32];
  url_t *a, *b;

  BEGIN();

  TEST_S(url_scheme(url_tel), "tel");

  tel->url_user = "+12345678";
  tel->url_params = "param=1;param=2";
  tel->url_headers = "From=foo@bar&To=bar@baz";
  tel->url_fragment = "unf";

  TEST_1(tst = su_strdup(home, telurl));
  TEST_1(url_d(url, tst) == 0);
  TEST_1(url_cmp(tel, url) == 0);
  TEST_1(url_cmp(url_hdup(home, (url_t const *)"tel:+12345678"
			  ";param=1;param=2"
			  "?From=foo@bar&To=bar@baz#unf"), url) == 0);
  TEST(url->url_type, url_tel);
  TEST_1(u = url_hdup(home, url));
  TEST(u->url_type, url_tel);
  TEST_1(url_cmp(tel, u) == 0);
  TEST_SIZE(url_e(tel2, sizeof(tel2), u), strlen(telurl));
  TEST_1(strcmp(tel2, telurl) == 0);
  TEST_SIZE(snprintf(tel2, sizeof(tel2), URL_PRINT_FORMAT,
		     URL_PRINT_ARGS(tel)), strlen(telurl));
  TEST_1(strcmp(tel2, telurl) == 0);

  url_digest(hash1, sizeof(hash1), url, NULL);
  url_digest(hash2, sizeof(hash2), (url_t *)telurl, NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);

  a = url_hdup(home, (void *)"tel:+1.245.62357");
  b = url_hdup(home, (void *)"tel:+(1).245.62357");
  TEST_1(a); TEST_1(b);
  TEST_1(url_cmp(a, b) == 0);
  TEST_1(url_cmp_all(a, b) == 0);

  su_home_deinit(home);

  END();
}

int test_fax(void)
{
  /* fax urls */
  su_home_t home[1] = { SU_HOME_INIT(home) };
  url_t fax[1] = { URL_INIT_AS(fax) };
  url_t *u, url[1];
  char *tst;
  char faxurl[] =
    "fax:+12345678"
    ";param=1;param=2"
    "?From=foo@bar&To=bar@baz#unf";
  char fax2[sizeof(faxurl) + 32];

  BEGIN();

  TEST_S(url_scheme(url_fax), "fax");

  fax->url_user = "+12345678";
  fax->url_params = "param=1;param=2";
  fax->url_headers = "From=foo@bar&To=bar@baz";
  fax->url_fragment = "unf";

  TEST_1(tst = su_strdup(home, faxurl));
  TEST_1(url_d(url, tst) == 0);
  TEST_1(url_cmp(fax, url) == 0);
  TEST(url->url_type, url_fax);
  TEST_1(u = url_hdup(home, url));
  TEST(u->url_type, url_fax);
  TEST_1(url_cmp(fax, u) == 0);
  TEST_SIZE(url_e(fax2, sizeof(fax2), u), strlen(faxurl));
  TEST_1(strcmp(fax2, faxurl) == 0);
  TEST_SIZE(snprintf(fax2, sizeof(fax2), URL_PRINT_FORMAT,
		     URL_PRINT_ARGS(fax)), strlen(faxurl));
  TEST_1(strcmp(fax2, faxurl) == 0);


  url_digest(hash1, sizeof(hash1), url, NULL);
  url_digest(hash2, sizeof(hash2), (url_t *)faxurl, NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);

  su_home_deinit(home);

  END();
}

int test_modem(void)
{
  /* modem urls */
  su_home_t home[1] = { SU_HOME_INIT(home) };
  url_t modem[1] = { URL_INIT_AS(modem) };
  url_t *u, url[1];
  char *tst;
  char modemurl[] =
    "modem:+12345678"
    ";param=1;param=2"
    "?From=foo@bar&To=bar@baz#unf";
  char modem2[sizeof(modemurl) + 32];

  BEGIN();

  TEST_S(url_scheme(url_modem), "modem");

  modem->url_user = "+12345678";
  modem->url_params = "param=1;param=2";
  modem->url_headers = "From=foo@bar&To=bar@baz";
  modem->url_fragment = "unf";

  TEST_1(tst = su_strdup(home, modemurl));
  TEST_1(url_d(url, tst) == 0);
  TEST_1(url_cmp(modem, url) == 0);
  TEST(url->url_type, url_modem);
  TEST_1(u = url_hdup(home, url));
  TEST(u->url_type, url_modem);
  TEST_1(url_cmp(modem, u) == 0);
  TEST_SIZE(url_e(modem2, sizeof(modem2), u), strlen(modemurl));
  TEST_1(strcmp(modem2, modemurl) == 0);
  TEST_SIZE(snprintf(modem2, sizeof(modem2), URL_PRINT_FORMAT,
		     URL_PRINT_ARGS(modem)), strlen(modemurl));
  TEST_1(strcmp(modem2, modemurl) == 0);

  url_digest(hash1, sizeof(hash1), url, NULL);
  url_digest(hash2, sizeof(hash2), (url_t *)modemurl, NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);

  su_home_deinit(home);

  END();
}

int test_file(void)
{
  /* Test a url with path like file:/foo/bar  */
  char fileurl[] = "file:///foo/bar";
  url_t file[1] = { URL_INIT_AS(file) };
  su_home_t home[1] = { SU_HOME_INIT(home) };
  char *tst;
  url_t *u, url[1];
  char buf1[sizeof(fileurl) + 32];
  char buf2[sizeof(fileurl) + 32];

  BEGIN();

  TEST_S(url_scheme(url_file), "file");

  TEST_1(tst = su_strdup(home, fileurl));
  TEST(url_d(url, tst), 0);
  TEST_S(url->url_host, "");
  file->url_root = '/';
  file->url_host = "";
  file->url_path = "foo/bar";
  TEST(url_cmp(file, url), 0);
  TEST(url->url_type, url_file);
  TEST_1(u = url_hdup(home, url));
  TEST(u->url_type, url_file);
  TEST(url_cmp(file, u), 0);
  TEST_SIZE(url_e(buf1, sizeof(buf1), u), strlen(fileurl));
  TEST_S(buf1, fileurl);
  TEST_SIZE(snprintf(buf2, sizeof(buf2), URL_PRINT_FORMAT, URL_PRINT_ARGS(u)),
	    strlen(fileurl));
  TEST_S(buf2, fileurl);

  url_digest(hash1, sizeof(hash1), url, NULL);
  url_digest(hash2, sizeof(hash2), (url_t *)fileurl, NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);

  su_home_deinit(home);

  END();
}

int test_ldap(void)
{
  /* Test a LDAP url  */
  char ldapurl[] = "ldap://cn=Manager,o=nokia:secret@localhost:389/ou=devices,o=nokia";
  url_t ldap[1] = { URL_INIT_AS(file) };
  su_home_t home[1] = { SU_HOME_INIT(home) };
  char *tst;
  url_t *u, url[1];
  char buf1[sizeof(ldapurl) + 32];
  char buf2[sizeof(ldapurl) + 32];

  BEGIN();

  ldap->url_type = url_unknown;
  ldap->url_scheme = "ldap";

  /* TEST_S(url_scheme(url_ldap), "ldap"); */

  TEST_1(tst = su_strdup(home, ldapurl));
  TEST(url_d(url, tst), 0);

  TEST_S(url->url_user, "cn=Manager,o=nokia:secret");
  /* TEST_S(url->url_password, "secret"); */
  TEST_S(url->url_host, "localhost");
  TEST_S(url->url_port, "389");
  TEST_S(url->url_path, "ou=devices,o=nokia");

  ldap->url_user = "cn=Manager,o=nokia";
  ldap->url_password = "secret";
  ldap->url_user = "cn=Manager,o=nokia:secret", ldap->url_password = NULL;
  ldap->url_host = "localhost";
  ldap->url_port = "389";
  ldap->url_path = "ou=devices,o=nokia";

  TEST(url_cmp(ldap, url), 0);
  TEST(url->url_type, url_unknown);
  TEST_S(url->url_scheme, "ldap");
  TEST_1(u = url_hdup(home, url));
  TEST(u->url_type, url_unknown);
  TEST_S(u->url_scheme, "ldap");
  TEST(url_cmp(ldap, u), 0);
  TEST_SIZE(url_e(buf1, sizeof(buf1), u), strlen(ldapurl));
  TEST_S(buf1, ldapurl);
  TEST_SIZE(snprintf(buf2, sizeof(buf2), URL_PRINT_FORMAT, URL_PRINT_ARGS(u)),
	    strlen(ldapurl));
  TEST_S(buf2, ldapurl);

  url_digest(hash1, sizeof(hash1), url, NULL);
  url_digest(hash2, sizeof(hash2), (url_t *)ldapurl, NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);

  su_home_deinit(home);

  END();
}

int test_rtsp(void)
{
  /* RTSP urls */
  su_home_t home[1] = { SU_HOME_INIT(home) };
  url_t rtsp[1] = { URL_INIT_AS(rtsp) };
  url_t *u, url[1];
  char *tst;
  char rtspurl[] = "rtsp://example.com:42/barfoo.rm";
  char rtspuurl[] = "rtspu://example.com:42/barfoo.rm";
  char rtsp2[sizeof(rtspurl) + 32];

  BEGIN();

  TEST_S(url_scheme(url_rtsp), "rtsp");
  TEST_S(url_scheme(url_rtspu), "rtspu");

  rtsp->url_root = 1;
  rtsp->url_host = "example.com";
  rtsp->url_port = "42";
  rtsp->url_path = "barfoo.rm";

  TEST_1(tst = su_strdup(home, rtspurl));
  TEST_1(url_d(url, tst) == 0);
  TEST_1(url_cmp(rtsp, url) == 0);
  TEST(url->url_type, url_rtsp);
  TEST_1(u = url_hdup(home, url));
  TEST(u->url_type, url_rtsp);
  TEST_1(url_cmp(rtsp, u) == 0);
  TEST(url_e(rtsp2, sizeof(rtsp2), u), strlen(rtspurl));
  TEST_1(strcmp(rtsp2, rtspurl) == 0);
  TEST_SIZE(snprintf(rtsp2, sizeof(rtsp2), URL_PRINT_FORMAT,
		     URL_PRINT_ARGS(rtsp)), strlen(rtspurl));
  TEST_1(strcmp(rtsp2, rtspurl) == 0);

  url_digest(hash1, sizeof(hash1), url, NULL);
  url_digest(hash2, sizeof(hash2), (url_t *)rtspurl, NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);

  su_home_deinit(home);
  rtsp->url_type = url_rtspu, rtsp->url_scheme = "rtspu";

  TEST_1(tst = su_strdup(home, rtspuurl));
  TEST_1(url_d(url, tst) == 0);
  TEST_1(url_cmp(rtsp, url) == 0);
  TEST(url->url_type, url_rtspu);

  url_digest(hash1, sizeof(hash1), url, NULL);
  url_digest(hash2, sizeof(hash2), (url_t *)rtspuurl, NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);

  su_home_deinit(home);

  END();
}

int test_http(void)
{
  /* http urls */
  su_home_t home[1] = { SU_HOME_INIT(home) };
  url_t http[1] = { URL_INIT_AS(http) };
  url_t *u, url[1];
  char *tst;
  char httpurl[] =
    "http://user:pass@host:32/foo;param=1/bar;param=3"
    "?From=foo@bar&To=bar@baz#unf";
  char http2[sizeof(httpurl) + 32];

  char queryonly[] =
    "http://some.host?query";

  BEGIN();

  TEST_S(url_scheme(url_http), "http");
  TEST_S(url_scheme(url_https), "https");

  http->url_root = '/';
  http->url_user = "user";
  http->url_password = "pass";
  http->url_host = "host";
  http->url_port = "32";
  http->url_path = "foo;param=1/bar;param=3";
  http->url_headers = "From=foo@bar&To=bar@baz";
  http->url_fragment = "unf";

  TEST_1(tst = su_strdup(home, httpurl));
  TEST_1(url_d(url, tst) == 0);
  TEST_1(url_cmp(http, url) == 0);
  TEST(url->url_type, url_http);
  TEST_1(u = url_hdup(home, url));
  TEST(u->url_type, url_http);
  TEST_1(url_cmp(http, u) == 0);
  TEST_SIZE(url_e(http2, sizeof(http2), u), strlen(httpurl));
  TEST_1(strcmp(http2, httpurl) == 0);
  TEST_SIZE(snprintf(http2, sizeof(http2), URL_PRINT_FORMAT,
		     URL_PRINT_ARGS(http)), strlen(httpurl));
  TEST_1(strcmp(http2, httpurl) == 0);

  url_digest(hash1, sizeof(hash1), http, NULL);
  url_digest(hash2, sizeof(hash2), (url_t *)httpurl, NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);

  memset(url, 0, sizeof url);
  TEST_1(tst = su_strdup(home, queryonly));
  TEST(url_d(url, tst), 0);
  TEST_S(url->url_host, "some.host");
  TEST_S(url->url_headers, "query");
  TEST_S(url->url_params, NULL);

  TEST_1(u = url_hdup(home, (void *)"http://[::1]/test;ing?here"));
  TEST_S(u->url_host, "[::1]");
  TEST_S(u->url_path, "test;ing");
  TEST_S(u->url_headers, "here");

  url_digest(hash1, sizeof(hash1), u, NULL);
  url_digest(hash2, sizeof(hash2), (url_t *)"http://[::1]/test;ing?here",
	     NULL);
  TEST(memcmp(hash1, hash2, sizeof(hash1)), 0);

  su_home_deinit(home);

  END();
}

int test_sanitizing(void)
{
  url_t url[1];
  char www[] = "www.hut.fi";
  char ftp[] = "ftp.hut.fi";
  char www2[] = "iptel.hut.fi/humppa";
  char sip[] = "test.host";
  char buf[64];

  BEGIN();

  TEST_1(url_d(url, www) == 0);
  TEST_1(url_sanitize(url) == 0);
  TEST(url->url_type, url_http);
  snprintf(buf, sizeof(buf), URL_PRINT_FORMAT, URL_PRINT_ARGS(url));
  TEST_S(buf, "http://www.hut.fi");

  TEST_1(url_d(url, ftp) == 0);
  TEST_1(url_sanitize(url) == 0);
  TEST(url->url_type, url_ftp);
  snprintf(buf, sizeof(buf), URL_PRINT_FORMAT, URL_PRINT_ARGS(url));
  TEST_S(buf, "ftp://ftp.hut.fi");

  TEST_1(url_d(url, www2) == 0);
  TEST_1(url_sanitize(url) == 0);
  TEST(url->url_type, url_http);
  snprintf(buf, sizeof(buf), URL_PRINT_FORMAT, URL_PRINT_ARGS(url));
  TEST_S(buf, "http://iptel.hut.fi/humppa");

  TEST_1(url_d(url, sip) == 0);
  TEST_1(url_sanitize(url) == 0);
  TEST(url->url_type, url_sip);
  snprintf(buf, sizeof(buf), URL_PRINT_FORMAT, URL_PRINT_ARGS(url));
  TEST_S(buf, "sip:test.host");

  END();
}


int test_tags(void)
{
  url_t u0[1];
  url_t *u1 = NULL;
  url_t const *u2 = (void *)-1;
  url_t const u3[1] = { URL_INIT_AS(sip) };
  char c0[] = "http://www.nokia.com";
  char const *c1 = "http://goodfeel.nokia.com";
  char *c2 = "http://forum.nokia.com";
  char const c3[] = "http://www.research.nokia.com";
  url_string_t *us0 = NULL;

  tagi_t *lst, *dup;

  tag_value_t value;
  char *s;
  su_home_t home[1] = { SU_HOME_INIT(home) };

  BEGIN();

  TEST(t_scan(urltag_url, home, c0, &value), 0);
  TEST_S(s = url_as_string(home, (url_t *)value), c0);

  TEST(t_scan(urltag_url, home, c3, &value), 0);
  TEST_S(s = url_as_string(home, (url_t *)value), c3);

  TEST_1(url_d(u0, c0) == 0);

  lst = tl_list(URLTAG_URL(u0),
		URLTAG_URL(u1),
		URLTAG_URL(u2),
		URLTAG_URL(u3),
		URLTAG_URL(c0),
		URLTAG_URL(c1),
		URLTAG_URL(c2),
		URLTAG_URL(c3),
		URLTAG_URL(us0),
		TAG_NULL());

  TEST_1(lst);

  dup = tl_adup(home, lst);

  tl_vfree(lst);

  su_free(home, dup);

  su_home_deinit(home);

  END();
}

#include <sofia-sip/su_tag_class.h>

int test_tag_filter(void)
{
  BEGIN();

#undef TAG_NAMESPACE
#define TAG_NAMESPACE "test"
  tag_typedef_t tag_a = STRTAG_TYPEDEF(a);
#define TAG_A(s)      tag_a, tag_str_v((s))
  tag_typedef_t tag_b = STRTAG_TYPEDEF(b);
#define TAG_B(s)      tag_b, tag_str_v((s))

  tagi_t filter[2] = {{ URLTAG_ANY() }, { TAG_END() }};

  tagi_t *lst, *result;

  lst = tl_list(TAG_A("X"),
		TAG_SKIP(2),
		URLTAG_URL((void *)"urn:foo"),
		TAG_B("Y"),
		URLTAG_URL((void *)"urn:bar"),
		TAG_NULL());

  TEST_1(lst);

  result = tl_afilter(NULL, filter, lst);

  TEST_1(result);
  TEST_P(result[0].t_tag, urltag_url);
  TEST_P(result[1].t_tag, urltag_url);

  tl_vfree(lst);
  free(result);

  END();
}

#if 0
/* This is just a spike. How we can get
 *   register_printf_function('U', printf_url, printf_url_info)
 * run while initializing?
 */
#include <printf.h>

int printf_url(FILE *fp,
	       const struct printf_info *info,
	       const void * const *args)
{
  url_t const *url = *(url_t **)args[0];

  return fprintf(fp, URL_PRINT_FORMAT, URL_PRINT_ARGS(url));
}

/* This is the appropriate argument information function for `printf_url'.  */
int printf_url_info(const struct printf_info *info,
		    size_t n, int *argtypes)
{
  if (n > 0) {
    argtypes[0] = PA_POINTER;
    return 1;
  }
  return 0;
}
#endif

int test_print(void)
{
#if 0
  url_t u0[1];
  url_t *u1 = NULL;
  url_t const *u2 = (void *)-1;
  url_t const u3[1] = { URL_INIT_AS(sip) };
  char c0[] = "http://www.nokia.com/";
  char c1[] = "http://goodfeel.nokia.com/test";
  char c2[] = "/test2/";
  char c3[] = "///file/";

  tagi_t *lst;

  BEGIN();

  TEST(register_printf_function('U', printf_url, printf_url_info), 0);

  TEST(url_d(u0, c0), 0);
  printf("URL is %U\n", u0);

  TEST(url_d(u0, c1), 0);
  printf("URL is %U\n", u0);

  TEST(url_d(u0, c2), 0);
  printf("URL is %U\n", u0);

  TEST(url_d(u0, c3), 0);
  printf("URL is %U\n", u0);
#else
  BEGIN();
#endif

  END();
}

void usage(int exitcode)
{
  fprintf(stderr, "usage: %s [-v] [-a]\n", name);
  exit(exitcode);
}

int main(int argc, char *argv[])
{
  int retval = 0;
  int i;

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

  retval |= test_quote(); fflush(stdout);
  retval |= test_any(); fflush(stdout);
  retval |= test_sip(); fflush(stdout);
  retval |= test_wv(); fflush(stdout);
  retval |= test_tel(); fflush(stdout);
  retval |= test_fax(); fflush(stdout);
  retval |= test_modem(); fflush(stdout);
  retval |= test_file(); fflush(stdout);
  retval |= test_ldap(); fflush(stdout);
  retval |= test_rtsp(); fflush(stdout);
  retval |= test_http(); fflush(stdout);
  retval |= test_sanitizing(); fflush(stdout);
  retval |= test_tags(); fflush(stdout);
  retval |= test_print(); fflush(stdout);
  retval |= test_tag_filter(); fflush(stdout);

  return retval;
}

