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

/**@ingroup test_msg
 *
 * @CFILE test_msg.c
 *
 * Torture tests for message parser.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Aug 21 15:18:26 2001 ppessi
 */

#include "config.h"

#include "test_class.h"
#include "test_protos.h"
#include "sofia-sip/msg.h"
#include "sofia-sip/msg_addr.h"
#include "sofia-sip/msg_date.h"
#include "sofia-sip/msg_parser.h"
#include "sofia-sip/bnf.h"
#include "sofia-sip/msg_mclass.h"
#include "sofia-sip/msg_mclass_hash.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <assert.h>

static int test_flags = 0;
#define TSTFLAGS test_flags

#include <sofia-sip/tstdef.h>

char const name[] = "test_msg";

static int msg_time_test(void)
{
  char buf[32];
  char const *s;
  char date1900[] = "Mon,  1 Jan 1900 00:00:00 GMT";
  char date1900_1[] = "Mon, 01 Jan 1900 00:00:01 GMT";
  char date822[] = "Thursday, 01-Jan-70 00:00:01 GMT";
  char date822b[] = "Wednesday, 09-Nov-99 23:12:40 GMT";
  char date822c[] = "Wednesday, 01-Sep-04 23:12:40 GMT";
  char date2822[] = "Monday, 01-Jan-1900 00:00:01 GMT";
  char dateasc[] = "Mon Jan  1 00:00:01 1900";
  msg_time_t now = msg_now(), date = now;

  BEGIN();
  s = date1900;
  TEST_1(msg_date_d(&s, &date) == 0);
  TEST(date, 0);
  TEST_SIZE(msg_date_e(buf, sizeof(buf), date), strlen(date1900));
  TEST_SIZE(msg_date_e(buf, sizeof(buf), 1), strlen(date1900_1));
  TEST_S(buf, date1900_1);

  s = date822;
  TEST_1(msg_date_d(&s, &date) == 0);
  TEST(date, 2208988801U);

  s = date822b;
  TEST_1(msg_date_d(&s, &date) == 0);
  TEST(date, 3151177960U);

  s = date822c;
  TEST_1(msg_date_d(&s, &date) == 0);
  TEST(date, 3303069160U);

  s = date2822;
  TEST_1(msg_date_d(&s, &date) == 0);
  TEST(date, 1);

  s = dateasc;
  TEST_1(msg_date_d(&s, &date) == 0);
  TEST(date, 1);

  {
    char error1[] = "Mo";
    char error2[] = "Mon,  1 Jan 19100 00:00:00 GMT";
    char error3[] = "Mon,  1 Jan 1900 00:00:";
    char error4[] = "Mon,  1 Jan 1900 25:00:00 GMT";
    char noerror5[] = "Mon,  1 Jan 1899 24:00:00 GMT";
    char error6[] = "Mon, 30 Feb 1896 23:59:59 GMT";
    char noerror7[] = "Mon, 29 Feb 1896 23:59:59 GMT";
    char error8[] = "Mon, 32 Jan 1900 24:00:00 GMT";
    char error9[] = "Mon, 27 Fev 1900 24:00:00 GMT";

    s = error1; TEST_1(msg_date_d(&s, &date) < 0);
    s = error2; TEST_1(msg_date_d(&s, &date) < 0);
    s = error3; TEST_1(msg_date_d(&s, &date) < 0);
    s = error4; TEST_1(msg_date_d(&s, &date) < 0);
    s = noerror5; TEST_1(msg_date_d(&s, &date) == 0); TEST(date, 0);
    s = error6; TEST_1(msg_date_d(&s, &date) < 0);
    s = noerror7; TEST_1(msg_date_d(&s, &date) == 0); TEST(date, 0);
    s = error8; TEST_1(msg_date_d(&s, &date) < 0);
    s = error9; TEST_1(msg_date_d(&s, &date) < 0);
  }

  {
    char error1[] = "4294967297";
    char *s;
    msg_numeric_t x[1];

    memset(x, 0, sizeof (x)); x->x_common->h_class = test_numeric_class;

    TEST_1(msg_numeric_d(NULL, (msg_header_t *)x, s = error1, strlen(error1)) < 0);
  }

  END();
}

static int addr_test(void)
{
  BEGIN();

  /* It *will* fail. */
  /* TEST(sizeof(socklen_t), sizeof(msg_addrlen(NULL))); */

  END();
}

int test_header_parsing(void)
{
  BEGIN();

  {
    /* Test quoting/unquoting */
    su_home_t home[1] = { SU_HOME_INIT(home) };
    char *quoted = "\"foo \\b\\a\\r\\\"\\\\\"extra";
    char *unquoted;

    TEST_1(unquoted = msg_unquote_dup(home, quoted));
    TEST_S(unquoted, "foo bar\"\\");

    su_home_deinit(home);
  }

  {
    /* Test parameter list */
    su_home_t home[1] = { SU_HOME_INIT(home) };
    msg_param_t const *params = NULL;
    char str[] = ";uffe;duffe = \"entten\" ; doo = [::1]  ", *s = str;
    char const canonic[] = ";uffe;duffe=\"entten\";doo=[::1]";
    char *end = str + strlen(str);
    char b[sizeof(canonic) + 8];

    TEST_1(msg_params_d(home, &s, &params) >= 0);
    TEST_1(params != 0);
    TEST_P(s, end);
    TEST_S(params[0], "uffe");
    TEST_S(params[1], "duffe=\042entten\042");
    TEST_S(params[2], "doo=[::1]");
    TEST_1(params[3] == NULL);
    TEST_SIZE(msg_params_e(NULL, 0, params), strlen(canonic));
    TEST_SIZE(msg_params_e(b, sizeof(b), params), strlen(canonic));
    TEST_S(b, canonic);

    TEST_S(msg_params_find(params, "uffe"), "");
    TEST_S(msg_params_find(params, "uffe="), "");
    TEST_S(msg_params_find(params, "duffe"), "\"entten\"");
    TEST_S(msg_params_find(params, "duffe="), "\"entten\"");
    TEST_S(msg_params_find(params, "doo"), "[::1]");
    TEST_S(msg_params_find(params, "doo="), "[::1]");

    TEST(msg_params_remove((msg_param_t *)params, "uffe"), 1);
    TEST_S(params[0], "duffe=\042entten\042");
    TEST_S(params[1], "doo=[::1]");
    TEST_1(params[2] == NULL);

    TEST(msg_params_remove((msg_param_t *)params, "doo"), 1);
    TEST_S(params[0], "duffe=\042entten\042");
    TEST_1(params[1] == NULL);

    su_home_deinit(home);
  }

  {
    /* Test that parameter list of length MSG_PARAMS_N is handled correctly */
    su_home_t home[1] = { SU_HOME_INIT(home) };
    msg_param_t const *params = NULL;
    char list1[] = ";one;two;three;four;five;six;seven;eight", *s = list1;
    char list2[] = ";one;two;three;four;five;six;seven";
    char list3[] = ";one;two;three;four;five;six;seven, humppaa";
    char *end3 = strchr(list3, ',');
    char list4[] = ";one;two;three;four;five;six;seven;eight;nine;ten"
      ";eleven;twelve;thirteen;fourteen;fiveteen;sixteen";
    char list5[] = ";one;two;three;four;five;six;seven;eight;nine;ten"
      ";eleven;twelve;thirteen;fourteen;fiveteen";
    char list6[] = ";one;two;three;four;five;six;seven;eight;nine;ten"
      ";eleven;twelve;thirteen;fourteen;fiveteen;sixteen;seventeen";
    int i;

    TEST_1(msg_params_d(home, &s, &params) >= 0);
    TEST_1(params);
    for (i = 0; i < 8; i++)
      TEST_1(params[i]);
    TEST_1(params[8] == NULL);

    s = list2, params = NULL;
    TEST_1(msg_params_d(home, &s, &params) >= 0);
    TEST_1(params);
    for (i = 0; i < 7; i++)
      TEST_1(params[i]);
    TEST_1(params[7] == NULL);

    s = list3; params = NULL;
    TEST_1(msg_params_d(home, &s, &params) >= 0);
    TEST_S(s, end3);
    TEST_1(params);
    for (i = 0; i < 7; i++)
      TEST_1(params[i]);
    TEST_1(params[7] == NULL);

    s = list4; params = NULL;
    TEST_1(msg_params_d(home, &s, &params) >= 0);
    TEST_1(params);
    for (i = 0; i < 16; i++)
      TEST_1(params[i]);
    TEST_1(params[16] == NULL);

    s = list5; params = NULL;
    TEST_1(msg_params_d(home, &s, &params) >= 0);
    TEST_1(params);
    for (i = 0; i < 15; i++)
      TEST_1(params[i]);
    TEST_1(params[15] == NULL);

    s = list6 ; params = NULL;
    TEST_1(msg_params_d(home, &s, &params) >= 0);
    TEST_1(params);
    for (i = 0; i < 17; i++)
      TEST_1(params[i]);
    TEST_1(params[17] == NULL);

    su_home_deinit(home);
  }

  {
    /* Test parameter lists */
    su_home_t home[1] = { SU_HOME_INIT(home) };
    unsigned i, j;

    msg_param_t const *p = NULL;
    char *master = ";0", *list, *end;

    for (i = 1; i < 256; i++) {
      master = su_sprintf(home, "%s; %u", master, i); TEST_1(master);
      list = end = su_strdup(home, master);
      TEST_1(msg_params_d(home, &end, &p) >= 0);
      TEST_S(end, "");
      TEST_1(p);
      for (j = 0; j <= i; j++) {
	char number[10];
	snprintf(number, sizeof number, "%u", j);
	TEST_S(p[j], number);
      }
      TEST_1(p[i + 1] == NULL);
      su_free(home, list);
      su_free(home, (void *)p), p = NULL;
    }

    master = ";0";

    for (i = 1; i < 256; i++) {
      master = su_sprintf(home, "%s; %u", master, i); TEST_1(master);
      list = end = su_strdup(home, master);
      TEST_1(msg_params_d(NULL, &end, &p) >= 0);
      TEST_S(end, "");
      TEST_1(p);
      for (j = 0; j <= i; j++) {
	char number[10];
	snprintf(number, sizeof number, "%u", j);
	TEST_S(p[j], number);
      }
      TEST_1(p[i + 1] == NULL);
      su_free(home, list);
      su_free(NULL, (void *)p), p = NULL;
    }

    su_home_deinit(home);
  }

  {
    /* Test comma-separated list */
    su_home_t home[1] = { SU_HOME_INIT(home) };

    msg_list_t k1[1] = {{{{ 0 }}}};
    char list1[] = "foo, bar, baz  zi  \"baz\"";

    TEST_1(msg_list_d(home, (msg_header_t *)k1, list1, strlen(list1)) >= 0);
    TEST_1(k1->k_items);
    TEST_S(k1->k_items[0], "foo");
    TEST_S(k1->k_items[1], "bar");
    TEST_S(k1->k_items[2], "baz zi\042baz\042");
    TEST_1(!k1->k_items[3]);

    su_home_deinit(home);
  }

  {
    /* Test that list of length MSG_PARAMS_N is handled correctly */
    su_home_t home[1] = { SU_HOME_INIT(home) };
    msg_list_t k2[1] = {{{{ 0 }}}};
    char list2[] = "one, two, three, four, five, six, seven, eight";

    TEST_1(
	  msg_list_d(home, (msg_header_t *)k2, list2, strlen(list2)) >= 0);
    TEST_1(k2->k_items);
    TEST_1(k2->k_items[7]);
    TEST_1(k2->k_items[8] == NULL);

    su_home_deinit(home);
  }

  {
    /* Test that list longer than MSG_PARAMS_N is handled correctly */
    su_home_t home[1] = { SU_HOME_INIT(home) };
    msg_list_t k3[1] = {{{{ 0 }}}};
    char list3[] = "one, two, three, four, five, six, seven, eight, nine";

    TEST_1(
	  msg_list_d(home, (msg_header_t *)k3, list3, strlen(list3)) >= 0);
    TEST_1(k3->k_items);
    TEST_1(k3->k_items[7]);
    TEST_1(k3->k_items[8]);
    TEST_1(k3->k_items[9] == NULL);

    su_home_deinit(home);
  }

  {
    /* Test that long lists are handled correctly */
    su_home_t home[1] = { SU_HOME_INIT(home) };

    msg_param_t *k = NULL;
    char *s;
    char list1[] = "one, two, three, four, five, six, seven, eight";
    char list2[] = "one, two, three, four, five, six, seven, eight";
    char list3[] = "one, two, three, four, five, six, seven, eight";
    char list4[] = "one, two, three, four, five, six, seven, eight, nine";

    s = list1; TEST_1(msg_commalist_d(home, &s, &k, msg_token_scan) >= 0);
    TEST_1(k);
    TEST_1(k[7]);
    TEST_1(k[8] == NULL);

    s = list2; TEST_1(msg_commalist_d(home, &s, &k, msg_token_scan) >= 0);
    s = list3; TEST_1(msg_commalist_d(home, &s, &k, msg_token_scan) >= 0);
    s = list4; TEST_1(msg_commalist_d(home, &s, &k, msg_token_scan) >= 0);

    su_home_deinit(home);
  }

  {
    /* Test parameter lists */
    su_home_t home[1] = { SU_HOME_INIT(home) };
    unsigned i, j;

    msg_param_t *p = NULL;
    char *master = "0", *list, *end;

    for (i = 1; i < 256; i++) {
      master = su_sprintf(home, "%s, %u", master, i); TEST_1(master);
      list = end = su_strdup(home, master);
      TEST_1(msg_commalist_d(home, &end, &p, msg_token_scan) >= 0);
      TEST_S(end, "");
      TEST_1(p);
      for (j = 0; j <= i; j++) {
	char number[10];
	snprintf(number, sizeof number, "%u", j);
	TEST_S(p[j], number);
      }
      TEST_1(p[i + 1] == NULL);
      su_free(home, list);
      su_free(home, (void *)p), p = NULL;
    }

    su_home_deinit(home);
  }

  {
    /* Test that errors in lists are handled correctly */
    su_home_t home[1] = { SU_HOME_INIT(home) };

    msg_param_t *k = NULL;
    char *s;
    char list1[] = "one, two, three, four, five, six, seven, foo=\"eight";
    char list2[] = "one, two, three,,@,$ four, five, six, seven, eight";

    s = list1; TEST_1(msg_commalist_d(home, &s, &k, NULL) < 0);
    TEST_1(k == NULL);

    s = list2; TEST_1(msg_commalist_d(home, &s, &k, msg_token_scan) < 0);

    su_home_deinit(home);
  }

  {
    /* Test empty parameter list */
    su_home_t home[1] = { SU_HOME_INIT(home) };

    msg_list_t k4[1] = {{{{ 0 }}}};
    char list4[] = ", ,\t,\r\n\t,  ,   ";

    TEST_1(
	  msg_list_d(home, (msg_header_t *)k4, list4, strlen(list4)) >= 0);
    TEST_1(k4->k_items == NULL);

    su_home_deinit(home);
  }

  {
    /* Test authentication headers */
    su_home_t home[1] = { SU_HOME_INIT(home) };
    msg_auth_t au[1] = {{{{ 0 }}}};
    char s[] = "Basic foo = \"bar==\" ,, bar=baari,"
      "baz=\"bof,\\\\ \\\" baff\", base\t64/ - is== ,,";

    TEST_1(msg_auth_d(home, (msg_header_t *)au, s, strlen(s)) >= 0);
    TEST_S(au->au_scheme, "Basic");
    TEST_1(au->au_params);
    TEST_S(au->au_params[0], "foo=\042bar==\042");
    TEST_S(au->au_params[1], "bar=baari");
    TEST_S(au->au_params[2], "baz=\042bof,\\\\ \\\042 baff\042");
    TEST_S(au->au_params[3], "base 64/- is==");
    TEST_1(!au->au_params[4]);

    su_home_deinit(home);
  }

  /* Test that msg_*_format() works */
  {
    su_home_t home[1] = { SU_HOME_INIT(home) };

    msg_content_type_t *c;

    c = msg_content_type_format(home, "%s/%s;%s;%s;%s;%s;%s;%s",
				"text", "plain",
				"charset=iso-8859-15",
				"format=flowed",
				"q=0.999",
				"msg-size=782572564",
				"name-space-url=\"http://www.nokia.com/foo\"",
				"foo=bar");

    su_home_deinit(home);
  }

  {
    /* Test parameter handling */
    su_home_t home[1] = { SU_HOME_INIT(home) };
    msg_content_encoding_t *ce;

    ce = msg_content_encoding_make(home, "zip, zap, zup, lz, zl, zz, ll");
    TEST_1(ce);
    TEST_S(msg_header_find_param(ce->k_common, "zz"), "");
    TEST_S(msg_header_find_item(ce->k_common, "zz"), "zz");
    TEST_P(msg_header_find_param(ce->k_common, "k"), NULL);
    TEST(msg_header_add_param(home, ce->k_common, "zip"), 0);
    TEST(msg_header_remove_param(ce->k_common, "zip"), 1);
    TEST_S(msg_header_find_param(ce->k_common, "zip"), "");
    TEST(msg_header_remove_param(ce->k_common, "zip"), 1);
    TEST_P(msg_header_find_param(ce->k_common, "zip"), NULL);
    TEST(msg_header_remove_param(ce->k_common, "zip"), 0);
    TEST(msg_header_replace_param(home, ce->k_common, "zip=zap"), 0);
    TEST_S(msg_header_find_param(ce->k_common, "zip=1"), "zap");
    TEST(msg_header_replace_param(home, ce->k_common, "zip=zup"), 1);
    TEST_S(msg_header_find_param(ce->k_common, "zip"), "zup");

    su_home_deinit(home);
  }

  {
    char b[8];
    TEST(msg_unquoted_e(NULL, 0, "\"\""), 6);
    TEST(msg_unquoted_e(b, 0, "\"\""), 6);
    TEST(msg_unquoted_e(b, 4, "\"\""), 6);
    TEST(msg_unquoted_e(b, 6, "\"\""), 6);
    TEST(memcmp(b, "\"\\\"\\\"\"", 6), 0);
    TEST(msg_unquoted_e(b, 4, "\""), 4);
    memset(b, 0, sizeof b);
    TEST(msg_unquoted_e(b, 1, "\"kuik"), 8);
    TEST(memcmp(b, "\"\0", 2), 0);
    TEST(msg_unquoted_e(b, 3, "\"kuik"), 8);
    TEST(memcmp(b, "\"\\\"\0", 4), 0);
    TEST(msg_unquoted_e(b, 7, "\"kuik"), 8);
    TEST(memcmp(b, "\"\\\"kuik\0", 8), 0);
  }

  END();
}

int hash_test(void)
{
  int i, j, hash = 0;
  msg_mclass_t const *mc = msg_test_mclass;
  msg_hclass_t *hc;

  BEGIN();

  for (i = 0; i < mc->mc_hash_size; i++) {
    hc = mc->mc_hash[i].hr_class;
    if (hc == NULL)
      continue;

    hash = msg_header_name_hash(hc->hc_name, NULL);
    TEST(hash, hc->hc_hash);

    /* Cross-check hashes */
    for (j = i + 1; j < mc->mc_hash_size; j++) {
      if (mc->mc_hash[j].hr_class == NULL)
	continue;
      if (hc->hc_hash == mc->mc_hash[j].hr_class->hc_hash)
	fprintf(stderr, "\t%s and %s have same hash\n",
		hc->hc_name, mc->mc_hash[j].hr_class->hc_name);
      TEST_1(hc->hc_hash != mc->mc_hash[j].hr_class->hc_hash);
    }
  }

  END();
}

msg_t *read_msg(char const buffer[])
{
  return msg_make(msg_test_mclass, MSG_DO_EXTRACT_COPY, buffer, -1);
}

/**Check if header chain contains any loops.
 *
 * @return
 * Return 0 if no loop, -1 otherwise.
 */
static
int msg_chain_loop(msg_header_t const *h)
{
  msg_header_t const *h2;

  if (!h) return 0;

  for (h2 = h->sh_succ; h && h2 && h2->sh_succ; h = h->sh_succ) {
    if (h == h2 || h == h2->sh_succ)
      return 1;

    h2 = h2->sh_succ->sh_succ;

    if (h == h2)
      return 1;
  }

  return 0;
}

/** Check header chain consistency.
 *
 * @return
 * Return 0 if consistent, number of errors otherwise.
 */
static
int msg_chain_errors(msg_header_t const *h)
{
  if (msg_chain_loop(h))
    return -1;

  for (; h; h = h->sh_succ) {
    if (h->sh_succ && h->sh_succ->sh_prev != &h->sh_succ)
      return -1;
    if (h->sh_prev && h != (*h->sh_prev))
      return -1;
  }

  return 0;
}

int test_msg_parsing(void)
{
  msg_t *msg, *orig;
  su_home_t *home;
  msg_test_t *tst, *otst;
  msg_request_t *request;
  msg_status_t *status;
  msg_content_location_t *location;
  msg_content_language_t *language;
  msg_accept_language_t *se;
  msg_separator_t *separator;
  msg_payload_t *payload;

  BEGIN();

  msg = read_msg("GET a-life HTTP/1.1" CRLF
		 "Content-Length: 6" CRLF
		 "Accept-Language: en;q=0.8, fi, se ; q = 0.6" CRLF
		 "Foo: bar" CRLF
		 CRLF
		 "test" CRLF);

  home = msg_home(msg);
  tst = msg_test_public(msg);

  TEST_1(msg);
  TEST_1(home);
  TEST_1(tst);

  TEST_P(tst->msg_error, NULL);

  TEST_1(tst->msg_accept_language);

  TEST_1(status = msg_status_make(home, "HTTP/1.1 200 Ok"));
  TEST(msg_header_insert(msg, (msg_pub_t *)tst, (msg_header_t *)status), 0);
  TEST_P(tst->msg_status, status); TEST_P(tst->msg_request, NULL);

  TEST_1(request = msg_request_make(home, "GET a-wife HTTP/1.0"));
  TEST(msg_header_insert(msg, (msg_pub_t *)tst, (msg_header_t *)request), 0);
  TEST_P(tst->msg_request, request); TEST_P(tst->msg_status, NULL);

  TEST_1(separator = msg_separator_make(home, "\r\n"));
  TEST(msg_header_insert(msg, (msg_pub_t *)tst, (msg_header_t *)separator), 0);
  TEST_P(tst->msg_separator, separator);
  TEST_P(separator->sep_common->h_succ, tst->msg_payload);

  /* Try to add a new payload */
  TEST_1(payload = msg_payload_make(home, "foofaa\r\n"));
  TEST(msg_header_insert(msg, (msg_pub_t *)tst, (msg_header_t *)payload), 0);
  /* It is appended */
  TEST_P(tst->msg_payload->pl_next, payload);
  TEST_P(tst->msg_payload->pl_common->h_succ, payload);

  {
    msg_param_t vs;
    int vi = 0;
    msg_param_t foo = "foo=bar";

    vs = NULL;
    MSG_PARAM_MATCH(vs, foo, "foo");
    TEST_S(vs, "bar");
    vs = NULL;
    MSG_PARAM_MATCH(vs, foo, "fo");
    TEST_P(vs, NULL);
    vi = 0;
    MSG_PARAM_MATCH_P(vi, foo, "foo");
    TEST(vi, 1);
    MSG_PARAM_MATCH_P(vi, foo, "fo");
    TEST(vi, 1);
    vi = 0;
    MSG_PARAM_MATCH_P(vi, foo, "fo");
    TEST(vi, 0);
  }

  msg_destroy(msg);

  /* Bug #2624: */
  msg = read_msg("GET /replaces HTTP/1.1" CRLF
		 "Accept-Encoding: gzip" CRLF
		 "Accept-Encoding: bzip2" CRLF
		 "Accept-Encoding: deflate" CRLF
		 "Accept-Language: en;q=0.8, fi, se ; q = 0.6" CRLF
		 );
  TEST_1(msg);
  tst = msg_test_public(msg);
  TEST_1(tst);

  {
    msg_accept_encoding_t *gzip, *bzip2, *deflate;
    msg_accept_encoding_t *lzss;
    msg_accept_language_t *en, *fi, *se;
    msg_accept_language_t *de, *sv, *sv_fi;

    TEST_1(gzip = tst->msg_accept_encoding);
    TEST_1(bzip2 = gzip->aa_next);
    TEST_1(deflate = bzip2->aa_next);

    TEST_1(gzip->aa_common->h_data);
    TEST_1(lzss = msg_accept_encoding_make(msg_home(msg), "lzss"));
    TEST(msg_header_replace(msg, (msg_pub_t *)tst, (void *)bzip2, (void *)lzss), 0);
    TEST_1(gzip->aa_common->h_data);

    TEST_1(en = tst->msg_accept_language);
    TEST_1(fi = en->aa_next);
    TEST_1(se = fi->aa_next);

    TEST_S(en->aa_value, "en");
    TEST_M(en->aa_common->h_data,
	   "Accept-Language: en;q=0.8, fi, se ; q = 0.6" CRLF,
	   en->aa_common->h_len);

    TEST_P((char *)en->aa_common->h_data + en->aa_common->h_len,
	   fi->aa_common->h_data);
    TEST(fi->aa_common->h_len, 0);
    TEST_P((char *)en->aa_common->h_data + en->aa_common->h_len,
	   se->aa_common->h_data);
    TEST(se->aa_common->h_len, 0);

    TEST_1(de = msg_accept_language_make(msg_home(msg), "de;q=0.3"));

    TEST(msg_header_replace(msg, (msg_pub_t *)tst, (void *)se, (void *)de), 0);
    TEST_P(en->aa_common->h_data, NULL);
    TEST_P(en->aa_next, fi);
    TEST_P(fi->aa_next, de);
    TEST_P(de->aa_next, NULL);

    TEST_P(en->aa_common->h_succ, fi);
    TEST_P(en->aa_common->h_prev, &deflate->aa_common->h_succ);
    TEST_P(fi->aa_common->h_succ, de);
    TEST_P(fi->aa_common->h_prev, &en->aa_common->h_succ);
    TEST_P(de->aa_common->h_succ, NULL);
    TEST_P(de->aa_common->h_prev, &fi->aa_common->h_succ);

    TEST_P(se->aa_next, NULL);
    TEST_P(se->aa_common->h_succ, NULL);
    TEST_P(se->aa_common->h_prev, NULL);

    TEST_1(sv = msg_accept_language_make(msg_home(msg),
					 "sv;q=0.6,sv_FI;q=0.7"));
    TEST_1(sv_fi = sv->aa_next);

    TEST(msg_header_replace(msg, (msg_pub_t *)tst, (void *)fi, (void *)sv), 0);

    TEST_P(en->aa_next, sv);
    TEST_P(sv->aa_next->aa_next, de);
    TEST_P(de->aa_next, NULL);

    TEST_P(en->aa_common->h_succ, sv);
    TEST_P(en->aa_common->h_prev, &deflate->aa_common->h_succ);
    TEST_P(sv->aa_common->h_succ, sv_fi);
    TEST_P(sv->aa_common->h_prev, &en->aa_common->h_succ);
    TEST_P(sv_fi->aa_common->h_succ, de);
    TEST_P(sv_fi->aa_common->h_prev, &sv->aa_common->h_succ);
    TEST_P(de->aa_common->h_succ, NULL);
    TEST_P(de->aa_common->h_prev, &sv_fi->aa_common->h_succ);

    TEST(msg_serialize(msg, (msg_pub_t *)tst), 0);
  }

  msg_destroy(msg);

  /* Bug #2429 */
  orig = read_msg("GET a-life HTTP/1.1" CRLF
		 "Foo: bar" CRLF
		 "Content-Length: 6" CRLF
		 CRLF
		 "test" CRLF
		 "extra stuff" CRLF);
  TEST_1(orig);
  otst = msg_test_public(orig);
  TEST_1(otst);

  msg = msg_copy(orig);
  msg_destroy(orig);
  tst = msg_test_public(msg);
  TEST_1(tst);

  home = msg_home(msg);

  TEST_1(request = msg_request_make(home, "GET a-wife HTTP/1.1"));

  TEST(msg_header_insert(msg, (msg_pub_t *)tst, (void *)request), 0);

  TEST_1(location =
	 msg_content_location_make(home, "http://localhost:8080/wife"));

  TEST(msg_header_insert(msg, (msg_pub_t *)tst, (void *)location), 0);

  TEST(msg_serialize(msg, (msg_pub_t *)tst), 0);
  TEST_1(msg_prepare(msg) > 0);

  TEST_1(language =
	 msg_content_language_make(home, "se-FI, fi-FI, sv-FI"));
  TEST(msg_header_insert(msg, (msg_pub_t *)tst, (void *)language), 0);

  TEST_1(se = msg_accept_language_make(home, "se, fi, sv"));
  TEST_1(se->aa_next);  TEST_1(se->aa_next->aa_next);
  TEST(msg_header_insert(msg, (msg_pub_t *)tst, (void *)se), 0);

  TEST(msg_serialize(msg, (msg_pub_t *)tst), 0);
  TEST_1(msg_prepare(msg) > 0);

  {
    char const encoded[] =
      "GET a-wife HTTP/1.1\r\n";

    TEST_SIZE(request->rq_common->h_len, strlen(encoded));
    TEST_M(request->rq_common->h_data, encoded, request->rq_common->h_len);
  }

  {
    char const encoded[] =
      "Content-Location: http://localhost:8080/wife\r\n";

    TEST_SIZE(location->g_common->h_len, strlen(encoded));
    TEST_M(location->g_common->h_data, encoded, location->g_common->h_len);
  }

  {
    char const encoded[] =
      "Content-Language: se-FI, fi-FI, sv-FI\r\n";
    TEST_SIZE(language->k_common->h_len, strlen(encoded));
    TEST_M(language->k_common->h_data, encoded, language->k_common->h_len);
  }

  {
    char const encoded[] = "Accept-Language: se, fi, sv\r\n";
    TEST_SIZE(se->aa_common->h_len, strlen(encoded));
    TEST_M(se->aa_common->h_data, encoded, se->aa_common->h_len);
    TEST_P((char *)se->aa_common->h_data + se->aa_common->h_len,
	   se->aa_next->aa_common->h_data);
    TEST_P((char *)se->aa_common->h_data + se->aa_common->h_len,
	   se->aa_next->aa_next->aa_common->h_data);
  }

  {
    size_t size = SIZE_MAX;
    char *s;
    char body[66 * 15 + 1];
    int i;
    msg_payload_t *pl;

    /* Bug #1726034 */
    for (i = 0; i < 15; i++)
      strcpy(body + i * 66,
	     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\r\n");
    pl = msg_payload_make(msg_home(msg), body);

    TEST(msg_header_insert(msg, (msg_pub_t *)tst, (void *)pl), 0);

    s = msg_as_string(msg_home(msg), msg, NULL, 0, &size);
    TEST_S(s,
"GET a-wife HTTP/1.1" CRLF
"Foo: bar" CRLF
"Content-Length: 6" CRLF
"Content-Location: http://localhost:8080/wife\r\n"
"Content-Language: se-FI, fi-FI, sv-FI\r\n"
"Accept-Language: se, fi, sv\r\n"
CRLF
"test" CRLF
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" CRLF
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" CRLF
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" CRLF
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" CRLF
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" CRLF
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" CRLF
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" CRLF
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" CRLF
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" CRLF
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" CRLF
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" CRLF
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" CRLF
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" CRLF
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" CRLF
"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" CRLF
);
  }

  msg_destroy(msg);

  END();
}

static int test_warning(void)
{
  msg_warning_t *w;
  su_home_t *home;
  char buf[64];

  BEGIN();

  TEST_1(home = su_home_new(sizeof *home));

  TEST_1((w = msg_warning_make(home,
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

  TEST_1(msg_warning_e(buf, sizeof buf, (msg_header_t *)w, 0) > 0);

  TEST_S(buf, "399 [::1]:39999 \"foo\\\" bar\"");

  su_home_unref(home);

  END();
}


/* Test error handling */
int test_msg_error(void)
{
  msg_t *msg;
  su_home_t *home;
  msg_test_t *tst;

  BEGIN();

  msg = read_msg("GET a-life HTTP/1.1" CRLF
		 "Content-Length: 6" CRLF
		 "Content-Language: fi" CRLF
		 "Content-Language: <se>" CRLF
		 "Accept-Language: en;q=0.8, fi, \"\", se ; q = 0.6" CRLF
		 "Foo bar baf: bar" CRLF
		 CRLF
		 "test" CRLF);

  home = msg_home(msg);
  tst = msg_test_public(msg);

  TEST_1(msg);
  TEST_1(home);
  TEST_1(tst);

  TEST_1(tst->msg_error);

  msg_destroy(msg);

  END();
}

int test_mclass(void)
{
  msg_t *msg;
  su_home_t *home;
  msg_test_t *tst;
  msg_request_t *request;
  msg_status_t *status;
  msg_separator_t *separator;
  msg_payload_t *payload;
  msg_content_length_t *l;
  msg_content_language_t *la;
  msg_content_encoding_t *k0, *k;
  msg_unknown_t *foo;

  BEGIN();

  /* Test that critical errors are signaled */
  msg = read_msg("GET a-life HTTP/1.1" CRLF
		 "Content-Length: 6bytes" CRLF
		 "Content-Type: *" CRLF
		 "Foo: bar" CRLF
		 "Content-Encoding: identity" CRLF
		 "Content-Language: en" CRLF
		 "Content-Language: en-us" CRLF
		 CRLF
		 "test" CRLF);

  tst = msg_test_public(msg);
  TEST_1(msg);
  TEST_1(tst);
  TEST_1(MSG_HAS_ERROR(tst->msg_flags)); /* Content-Length is critical */
  msg_destroy(msg);

  msg = read_msg("GET a-life HTTP/1.1" CRLF
		 "Content-Length: 6" CRLF
		 "Content-Type: *" CRLF
		 "Foo: bar" CRLF
		 "Content-Encoding: " CRLF /* XXX */
		 "Content-Language: en" CRLF
		 "Content-Language: en-us" CRLF
		 CRLF
		 "test" CRLF);

  home = msg_home(msg);
  tst = msg_test_public(msg);

  TEST_1(msg);
  TEST_1(home);
  TEST_1(tst);

  TEST_SIZE(msg_iovec(msg, NULL, 0), 1);

  TEST_1(tst->msg_unknown);	/* Foo */
  TEST_1(tst->msg_content_type == NULL);
  TEST_1(tst->msg_error);	/* Content-Type */
  TEST_1(tst->msg_error->er_next == NULL);

  TEST_1(!MSG_HAS_ERROR(tst->msg_flags)); /* Content-type is not critical */

  TEST_1(la = tst->msg_content_language);
  TEST_1(la->k_common->h_data);
  TEST_1(la->k_items);
  TEST_S(la->k_items[0], "en");
  TEST_S(la->k_items[1], "en-us");
  TEST_P(la->k_items[2], NULL);
  TEST_1(la->k_next);
  TEST_1(la->k_next->k_common->h_data);
  TEST_1(la->k_next->k_items == NULL);

  TEST(msg_header_add_make(msg, (msg_pub_t *)tst,
			   msg_content_language_class,
			   "en-gb"), 0);
  TEST_P(la, tst->msg_content_language);
  TEST_P(la->k_common->h_data, NULL);
  TEST_S(la->k_items[2], "en-gb");
  TEST_P(la->k_next, NULL);

  TEST_1(status = msg_status_make(home, "HTTP/1.1 200 Ok"));
  TEST(msg_header_insert(msg, (msg_pub_t *)tst, (msg_header_t *)status), 0);
  TEST_P(tst->msg_status, status); TEST_P(tst->msg_request, NULL);

  TEST_1(request = msg_request_make(home, "GET a-wife HTTP/1.0"));
  TEST(msg_header_insert(msg, (msg_pub_t *)tst, (msg_header_t *)request), 0);
  TEST_P(tst->msg_request, request); TEST_P(tst->msg_status, NULL);

  TEST_1(separator = msg_separator_make(home, "\r\n"));
  TEST(msg_header_insert(msg, (msg_pub_t *)tst, (msg_header_t *)separator), 0);
  TEST_P(tst->msg_separator, separator);
  TEST_P(separator->sep_common->h_succ, tst->msg_payload);

  /* Try to add a new payload */
  TEST_1(payload = msg_payload_make(home, "foofaa\r\n"));
  TEST(msg_header_insert(msg, (msg_pub_t *)tst, (msg_header_t *)payload), 0);
  /* The new payload should be appended */
  TEST_P(tst->msg_payload->pl_next, payload);
  TEST_P(tst->msg_payload->pl_common->h_succ, payload);

  /* Try to add a new header */
  TEST_1(l = msg_content_length_create(home,
				       tst->msg_payload->pl_len +
				       payload->pl_len));
  TEST(msg_header_insert(msg, (msg_pub_t *)tst, (msg_header_t *)l), 0);
  /* The new header should be last before separator */
  TEST_P(l->l_common->h_succ, separator);

  TEST_1(foo = tst->msg_unknown);
  TEST_S(foo->un_name, "Foo");
  TEST_S(foo->un_value, "bar");
  foo->un_value = "baz";
  TEST_1(foo = msg_unknown_dup(home, foo));
  TEST(msg_header_insert(msg, (msg_pub_t *)tst, (msg_header_t *)foo), 0);
  TEST_P(tst->msg_unknown->un_next, foo);

  TEST_1(k = msg_content_encoding_make(home, "gzip, compress"));
  k0 = tst->msg_content_encoding;
  TEST(msg_header_add_dup(msg, (msg_pub_t *)tst, (msg_header_t *)k), 0);
  TEST_P(k0, tst->msg_content_encoding);
  TEST_1(k0->k_items);
  TEST_S(k0->k_items[0], "gzip");
  TEST_S(k0->k_items[1], "compress");
  TEST_P(k0->k_items[2], NULL);

  TEST_1(k = msg_content_encoding_make(home, "gzip, deflate, compress"));
  TEST(msg_header_add_dup(msg, (msg_pub_t *)tst, (msg_header_t *)k), 0);
  TEST_P(k0, tst->msg_content_encoding);
  TEST_1(k0->k_items);
  TEST_S(k0->k_items[0], "gzip");
  TEST_S(k0->k_items[1], "compress");
  TEST_S(k0->k_items[2], "deflate");
  TEST_P(k0->k_items[3], NULL);

  msg_destroy(msg);

  END();
}

int test_copy(void)
{
  msg_t *msg0, *msg, *msg1, *msg2;
  su_home_t *home;
  msg_test_t *tst0, *tst, *copy, *dup;
  msg_request_t *request;
  msg_common_t *h, *h_succ;
  msg_iovec_t iovec[8];

  char const s[] =
    "GET /a-life HTTP/1.1" CRLF
    "Content-Length: 6" CRLF
    "Content-Type: *" CRLF
    "Foo: bar" CRLF
    "Content-Language: " CRLF
    CRLF
    "test" CRLF;

  BEGIN();

  msg0 = read_msg(s);

  TEST_1(msg0);
  TEST_1(tst0 = msg_test_public(msg0));

  TEST_SIZE(msg_iovec(msg0, iovec, 8), 1);

  TEST_1(msg = msg_copy(msg0));
  TEST_1(copy = msg_test_public(msg));
  TEST_1(copy->msg_request);
  TEST_1(tst0->msg_request);
  TEST_S(copy->msg_request->rq_url->url_path,
	 tst0->msg_request->rq_url->url_path);
  TEST_S(copy->msg_request->rq_url->url_path, "a-life");
  TEST_P(copy->msg_request->rq_url->url_path,
	 tst0->msg_request->rq_url->url_path);

  msg_destroy(msg);

  TEST_1(msg = msg_dup(msg0));
  TEST_1(dup = msg_test_public(msg));
  TEST_1(dup->msg_request);
  TEST_1(tst0->msg_request);
  TEST_S(dup->msg_request->rq_url->url_path,
	 tst0->msg_request->rq_url->url_path);
  TEST_S(dup->msg_request->rq_url->url_path, "a-life");
  TEST_1(dup->msg_request->rq_url->url_path !=
	 tst0->msg_request->rq_url->url_path);

  msg_destroy(msg);

  TEST_1(msg = msg_copy(msg0)); msg_destroy(msg0);

  TEST_1(home = msg_home(msg));
  TEST_1(tst = msg_test_public(msg));

  TEST_1(tst->msg_unknown);	/* Foo */
  TEST_1(tst->msg_content_type == NULL);
  TEST_1(tst->msg_error); /* Content-Type */

  TEST_1(!MSG_HAS_ERROR(tst->msg_flags)); /* Flags are not copied */

  TEST_1(tst0->msg_request);
  TEST_1(request = tst->msg_request);
  TEST_P(tst0->msg_request->rq_url->url_path, request->rq_url->url_path);

  TEST_SIZE(msg_iovec(msg, iovec, 8), 1);

  TEST_S(iovec->siv_base, s);

  TEST_1(msg1 = msg_dup(msg));
  TEST_1(tst = msg_test_public(msg1));
  TEST_1(tst->msg_request);

  for (h = tst->msg_request->rq_common; h; h = h_succ) {
    if (h->h_prev)
      *h->h_prev = NULL;
    h_succ = (msg_common_t*)h->h_succ;
    h->h_succ = NULL;
  }

  TEST_1(msg2 = msg_copy(msg1));
  msg_destroy(msg2);

  TEST_1(msg2 = msg_dup(msg1));
  msg_destroy(msg2);

  msg_destroy(msg1);
  msg_destroy(msg);

  END();
}

int test_mime(void)
{
  msg_t *msg;
  su_home_t *home;
  int n;
  msg_test_t *tst;
  msg_header_t *h, *h_succ, *head;
  void *removed;
  msg_accept_t *ac, *ac0;
  msg_accept_charset_t *aa;
  msg_multipart_t *mp, *mp0, *mpX, *mpnew;
  msg_payload_t *pl;
  msg_content_type_t *c;
  msg_content_id_t *cid;
  msg_content_transfer_encoding_t *cte;

  char const s[] =
    "GET /a-life HTTP/1.1" CRLF
    "Accept: text/html;level=4;q=1" CRLF
    "Accept: text / plain;q=0.9" CRLF
    "Accept-Charset: *;q=0.1, iso-latin-1, utf-8;q=0.9" CRLF
    "Accept-Encoding: gzip;q=0.9, deflate" CRLF
    "Accept-Encoding: , identity ," CRLF
    "Accept: */*;q=0.2" CRLF
    "Accept-Language: en;q=0.5, es;q=0.2, fr;q=0.9, fi, x-pig-latin" CRLF
    "Content-Language: fi, se" CRLF
    "Content-Language: en, de" CRLF
    "Content-Disposition: render; required" CRLF
    "Content-Encoding: gzip, deflate" CRLF
    "Content-Base: http://localhost/foo" CRLF
    "MIME-Version: 1.0" CRLF
    "Content-Type: multipart/alternative ; boundary=\"LaGqGt4BI6Ho\"" CRLF
    /* "Content-Length: 305" CRLF */
    "Content-MD5: LLO7gLaGqGt4BI6HouiWng==" CRLF
    CRLF
    "test" CRLF
    CRLF			/* 1 */
    "--LaGqGt4BI6Ho" "  " CRLF
    CRLF			/* 2 */
    "part 1" CRLF		/* 3 */
    CRLF			/* 4 */
    "--LaGqGt4BI6Ho" CRLF
    "Content-Type: text/plain ; charset = iso-8859-1" CRLF /* 5 */
    "Content-ID: <m7ZvEEm49xdTT0WCDUgnww@localhost>" CRLF /* 6 */
    "Content-Transfer-Encoding: quoted-unreadable" CRLF	/* 7 */
    CRLF			/* 8 */
    "part 2"			/* 9 */
    CRLF "--LaGqGt4BI6Ho"	/* 10 */
    "Content-Type: text/html" CRLF /* 11 */
    "Content-ID: <4SP77aQZ9z6Top2dvLqKPQ@localhost>" CRLF /* 12 */
    CRLF			/* 13 */
#define BODY3 "<html><body>part 3</body></html>" CRLF
    BODY3			/* 14 */
    CRLF "--LaGqGt4BI6Ho"	/* 15 */
    "c: text/html" CRLF		/* 16 */
    "l: 9" CRLF			/* 17 */
    "e: identity" CRLF		/* 18 */
    CRLF			/* 19 */
#define BODY4 "<html/>" CRLF
    BODY4			  /* 20 */
    CRLF "--LaGqGt4BI6Ho--"	  /* 21 */
    CRLF;

  BEGIN();

  msg = read_msg(s);
  home = msg_home(msg);
  tst = msg_test_public(msg);

  TEST_1(msg);
  TEST_1(home);
  TEST_1(tst);

  TEST_1(tst->msg_error == NULL);
  TEST_1((tst->msg_flags & MSG_FLG_ERROR) == 0);

  TEST_1(ac = tst->msg_accept);
  TEST_1(ac = ac->ac_next);
  TEST_S(ac->ac_type, "text/plain");
  TEST_S(ac->ac_q, "0.9");
  TEST_1(ac = msg_accept_dup(home, ac));
  TEST_S(ac->ac_type, "text/plain");
  TEST_S(ac->ac_q, "0.9");
  TEST_S(tst->msg_accept->ac_next->ac_q, "0.9");
  TEST_1(ac->ac_q != tst->msg_accept->ac_next->ac_q);

  TEST_1(ac = msg_accept_make(home, "text / plain"));
  ac->ac_q = "1.0";
  TEST_1(ac = msg_accept_dup(home, ac0 = ac));
  TEST_1(ac->ac_q != ac0->ac_q);

  for (h = (msg_header_t *)tst->msg_request; h; h = h->sh_succ) {
    TEST_1(h->sh_data);
    if (h->sh_succ)
      TEST_P((char*)h->sh_data + h->sh_len, h->sh_succ->sh_data);
  }

  TEST_1(aa = tst->msg_accept_charset);
  TEST_S(aa->aa_value, "*");   TEST_S(aa->aa_q, "0.1");

  mp = msg_multipart_parse(home, tst->msg_content_type, tst->msg_payload);

  TEST_1(mp0 = mp);

  TEST_1(mp->mp_data);
  TEST(memcmp(mp->mp_data, CRLF "--" "LaGqGt4BI6Ho" CRLF, mp->mp_len), 0);
  TEST_1(mp->mp_common->h_data);
  TEST_M(mp->mp_common->h_data, CRLF "--" "LaGqGt4BI6Ho" "  " CRLF,
	 mp->mp_common->h_len);

  TEST_1(pl = mp->mp_payload); TEST_1(pl->pl_data);
  TEST_SIZE(strlen("part 1" CRLF), pl->pl_len);
  TEST(memcmp(pl->pl_data, "part 1" CRLF, pl->pl_len), 0);

  TEST_1(mp = mp->mp_next);

  TEST_1(mp->mp_data);
  TEST(memcmp(mp->mp_data, CRLF "--" "LaGqGt4BI6Ho" CR LF, mp->mp_len), 0);

  TEST_1(c = mp->mp_content_type);
  TEST_S(c->c_type, "text/plain"); TEST_S(c->c_subtype, "plain");
  TEST_1(c->c_params);   TEST_1(c->c_params[0]);
  TEST_S(c->c_params[0], "charset=iso-8859-1");

  TEST_1(cid = mp->mp_content_id);

  TEST_1(cte = mp->mp_content_transfer_encoding);
  TEST_S(cte->g_string, "quoted-unreadable");

  TEST_1(pl = mp->mp_payload); TEST_1(pl->pl_data);
  TEST_SIZE(strlen("part 2"), pl->pl_len);
  TEST(memcmp(pl->pl_data, "part 2", pl->pl_len), 0);

  TEST_1(mp = mp->mp_next);

  TEST_1(mp->mp_data);
  TEST_M(mp->mp_data, CRLF "--" "LaGqGt4BI6Ho" CRLF, mp->mp_len);

  TEST_1(pl = mp->mp_payload); TEST_1(pl->pl_data);
  TEST_SIZE(strlen(BODY3), pl->pl_len);
  TEST(memcmp(pl->pl_data, BODY3, pl->pl_len), 0);

  TEST_1(mp = mp->mp_next);

  TEST_1(mp->mp_data);
  TEST_M(mp->mp_data, CRLF "--" "LaGqGt4BI6Ho" CRLF, mp->mp_len);

  TEST_1(mp->mp_content_encoding);
  TEST_1(mp->mp_content_type);

  TEST_1(pl = mp->mp_payload); TEST_1(pl->pl_data);
  TEST_SIZE(strlen(BODY4), pl->pl_len);
  TEST(memcmp(pl->pl_data, BODY4, pl->pl_len), 0);

  mpX = mp;

  TEST_1(!(mp = mp->mp_next));

  /* Test serialization */
  head = NULL;
  TEST_1(h = msg_multipart_serialize(&head, mp0));
  TEST_P((void *)h, mpX->mp_close_delim);
  TEST_1(!msg_chain_errors((msg_header_t *)mp0));

  /* Remove chain */
  for (h = (msg_header_t *)mp0, n = 0; h; h = h_succ, n++) {
    h_succ = h->sh_succ;
    if (h->sh_prev) *h->sh_prev = NULL;
    h->sh_prev = NULL;
    h->sh_succ = NULL;
  }

  TEST(n, 21);

  head = NULL;
  TEST_1(h = msg_multipart_serialize(&head, mp0));
  TEST_P(h, mpX->mp_close_delim);
  TEST_1(!msg_chain_errors((msg_header_t *)mp0));

  for (h = (msg_header_t *)mp0, n = 0; h; h = h_succ, n++) {
    h_succ = h->sh_succ;
  }

  TEST(n, 21);

  /* Add a new part to multipart */
  mpnew = su_zalloc(home, sizeof(*mpnew)); TEST_1(mpnew);
  removed = mpX->mp_close_delim;
  mpX->mp_next = mpnew; mpX = mpnew;
  mpnew->mp_content_type = msg_content_type_make(home, "multipart/mixed");
  TEST_1(mpnew->mp_content_type);
  TEST(msg_multipart_complete(msg_home(msg), tst->msg_content_type, mp0), 0);

  head = NULL;
  TEST_1(h = msg_multipart_serialize(&head, mp0));
  TEST_P((void *)h, mpX->mp_close_delim);
  TEST_1(!msg_chain_errors((msg_header_t *)mp0));

  for (h = (msg_header_t *)mp0, n = 0; h; h = h_succ, n++) {
    h_succ = h->sh_succ;
    TEST_1(h != removed);
  }

  TEST(n, 21 + 4);

#define remove(h) \
  (((*((msg_header_t*)(h))->sh_prev = ((msg_header_t*)(h))->sh_succ) ? \
   (((msg_header_t*)(h))->sh_succ->sh_prev = ((msg_header_t*)(h))->sh_prev) \
   : NULL), \
   ((msg_header_t*)(h))->sh_succ = NULL, \
   ((msg_header_t*)(h))->sh_prev = NULL)

  remove(mp0->mp_separator);
  remove(mp0->mp_next->mp_payload);
  remove(mp0->mp_next->mp_next->mp_content_type);
  remove(mp0->mp_next->mp_next->mp_next->mp_next->mp_close_delim);

  TEST_1(!msg_chain_errors((msg_header_t *)mp0));

  head = NULL;
  TEST_1(h = msg_multipart_serialize(&head, mp0));
  TEST_P(h, mpX->mp_close_delim);
  TEST_1(!msg_chain_errors((msg_header_t *)mp0));

  for (h = (msg_header_t *)mp0, n = 0; h; h = h_succ, n++) {
    h_succ = h->sh_succ;
    if (h_succ == NULL)
      TEST_P(h, mpX->mp_close_delim);
    TEST_1(h != removed);
  }

  TEST(n, 21 + 4);

  /* Add an recursive multipart */
  mpnew = su_zalloc(home, sizeof(*mpnew)); TEST_1(mpnew);
  mpX->mp_multipart = mpnew;
  mpnew->mp_content_type = msg_content_type_make(home, "text/plain");
  TEST_1(mpnew->mp_content_type);
  TEST(msg_multipart_complete(msg_home(msg), tst->msg_content_type, mp0), 0);
  TEST_1(mpnew->mp_close_delim);

  head = NULL;
  TEST_1(h = msg_multipart_serialize(&head, mp0));
  TEST_P(h, mpX->mp_close_delim);

  TEST_1(!msg_chain_errors((msg_header_t *)mp0));

  for (h = (msg_header_t *)mp0, n = 0; h; h = h_succ, n++)
    h_succ = h->sh_succ;

  TEST(n, 21 + 9);

  su_home_check(home);
  su_home_zap(home);

  END();
}

/** Test MIME encoding */
int test_mime2(void)
{
  msg_t *msg;
  su_home_t *home;
  int n, m, len;
  msg_test_t *tst;
  msg_header_t *h;
  msg_accept_charset_t *aa;
  msg_multipart_t *mp;
  msg_content_type_t *c;
  msg_payload_t *pl;
  char const *end;

  char const s[] =
    "GET /a-life HTTP/1.1" CRLF
    "Accept: text/html;level=4;q=1" CRLF
    "Accept: text / plain;q=0.9" CRLF
    "Accept-Charset: *;q=0.1, iso-latin-1, utf-8;q=0.9" CRLF
    "Accept-Encoding: gzip;q=0.9, deflate" CRLF
    "Accept-Encoding: , identity ," CRLF
    "Accept: */*;q=0.2" CRLF
    "Accept-Language: en;q=0.5, es;q=0.2, fr;q=0.9, fi, x-pig-latin" CRLF
    "Content-Language: fi, se" CRLF
    "Content-Language: en, de" CRLF
    "Content-Disposition: render; required" CRLF
    "Content-Encoding: gzip, deflate" CRLF
    "Content-Base: http://localhost/foo" CRLF
    "MIME-Version: 1.0" CRLF
    "Content-Type: multipart/alternative ; boundary=\"LaGqGt4BI6Ho\"" CRLF
    "Content-MD5: LLO7gLaGqGt4BI6HouiWng==" CRLF
    CRLF
    "test" CRLF
    CRLF			/* 1 */
    "--LaGqGt4BI6Ho" "  " CRLF
    CRLF			/* 2 */
    "part 1" CRLF		/* 3 */
    CRLF			/* 4 */
    "--LaGqGt4BI6Ho" CRLF
    "Content-Type: text/plain;charset=iso-8859-1" CRLF /* 5 */
    "Content-ID: <m7ZvEEm49xdTT0WCDUgnww@localhost>" CRLF /* 6 */
    "Content-Transfer-Encoding: quoted-unreadable" CRLF	/* 7 */
    CRLF			/* 8 */
    "part 2"			/* 9 */
    CRLF "--LaGqGt4BI6Ho"	/* 10 */
    "Content-Type: text/html" CRLF /* 11 */
    "Content-ID: <4SP77aQZ9z6Top2dvLqKPQ@localhost>" CRLF /* 12 */
    CRLF			/* 13 */
#define BODY3 "<html><body>part 3</body></html>" CRLF
    BODY3			/* 14 */
    CRLF			/* 15 */
    "--LaGqGt4BI6Ho--" CRLF;

  char const part1[] = "This is text\n";
  char const part2[] = "<html><body>This is html</body></html>";
  char const part3[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

  BEGIN();

  msg = read_msg(s);
  home = msg_home(msg);
  tst = msg_test_public(msg);

  TEST_1(msg);
  TEST_1(home);
  TEST_1(tst);

  TEST_1(tst->msg_error == NULL);
  TEST_1((tst->msg_flags & MSG_FLG_ERROR) == 0);

  for (h = (msg_header_t *)tst->msg_request; h; h = h->sh_succ) {
    TEST_1(h->sh_data);
    if (h->sh_succ)
      TEST_P((char*)h->sh_data + h->sh_len, h->sh_succ->sh_data);
  }

  TEST_1(aa = tst->msg_accept_charset);
  TEST_S(aa->aa_value, "*");   TEST_S(aa->aa_q, "0.1");

  TEST_1(c = tst->msg_content_type);
  TEST_1(tst->msg_payload);

  {
    su_home_t h0[1] = { SU_HOME_INIT(h0) };

    pl = msg_payload_dup(h0, tst->msg_payload); TEST_1(pl);

    mp = msg_multipart_parse(home, c, pl); TEST_1(mp);

    for (n = 0, h = (msg_header_t *)mp; h; h = h->sh_succ, n++)
      h->sh_data = NULL, h->sh_len = 0;
    TEST(n, 15);

    n = msg_multipart_prepare(msg, mp, 0);

    TEST_1(end = strstr(s, "--LaGqGt4BI6Ho  "));
    len = strlen(end);
    TEST(len, n);

    TEST_1(mp = msg_multipart_dup(h0, mp));

    su_home_check(h0);
    su_home_deinit(h0);
  }

  /* Test parsing without explicit boundary */
  {
    su_home_t h0[1] = { SU_HOME_INIT(h0) };

    pl = msg_payload_dup(h0, tst->msg_payload); TEST_1(pl);

    mp = msg_multipart_parse(h0, NULL, pl);
    TEST_1(mp);

    for (n = 0, h = (msg_header_t *)mp; h; h = h->sh_succ, n++)
      h->sh_data = NULL, h->sh_len = 0;
    TEST(n, 15);

    n = msg_multipart_prepare(msg, mp, 0);

    TEST_1(end = strstr(s, "--LaGqGt4BI6Ho  "));
    len = strlen(end);
    TEST(len, n);

    TEST_1(mp = msg_multipart_dup(h0, mp));

    su_home_check(h0);
    su_home_deinit(h0);
  }

  /* Test parsing without preamble and explicit boundary */
  {
    su_home_t h0[1] = { SU_HOME_INIT(h0) };

    pl = msg_payload_dup(h0, tst->msg_payload); TEST_1(pl);

    n = strstr(pl->pl_data, "--LaGqGt4BI6Ho") - (char *)pl->pl_data;
    pl->pl_data = n + (char *)pl->pl_data; pl->pl_len -= n;

    len = pl->pl_len;

    mp = msg_multipart_parse(h0, NULL, pl); TEST_1(mp);

    for (n = 0, h = (msg_header_t *)mp; h; h = h->sh_succ, n++)
      h->sh_data = NULL, h->sh_len = 0;
    TEST(n, 15);

    n = msg_multipart_prepare(msg, mp, 0);

    TEST(len, n);

    TEST_1(mp = msg_multipart_dup(h0, mp));

    su_home_check(h0);
    su_home_deinit(h0);
  }

  /* Test parsing without CR's */
  {
    su_home_t h0[1] = { SU_HOME_INIT(h0) };
    char *b;

    pl = msg_payload_dup(h0, tst->msg_payload); TEST_1(pl);

    /* Remove CRs */
    b = pl->pl_data, len = pl->pl_len;
    for (n = m = 0; n < len; n++) {
      if ((b[m] = b[n]) != '\r')
	m++;
    }
    pl->pl_len = m;

    mp = msg_multipart_parse(h0, NULL, pl);
    TEST_1(mp);

    for (n = 0, h = (msg_header_t *)mp; h; h = h->sh_succ, n++)
      h->sh_data = NULL, h->sh_len = 0;
    TEST(n, 15);

    n = msg_multipart_prepare(msg, mp, 0);
    TEST_1(n > 0);

    TEST_1(mp = msg_multipart_dup(h0, mp));

    su_home_check(h0);
    su_home_deinit(h0);
  }

  /* Create a new multipart from three parts */
  TEST_1(c = msg_content_type_make(home, "multipart/related"));

  TEST_1(mp = msg_multipart_create(home, "text/plain", part1, strlen(part1)));
  TEST_1(mp->mp_next =
	 msg_multipart_create(home, "text/html", part2, strlen(part2)));
  TEST_1(mp->mp_next->mp_next =
	 msg_multipart_create(home, "application/octet-stream",
			      part3, sizeof part3));

  TEST(msg_multipart_complete(home, c, mp), 0);

  h = NULL;
  TEST_P(msg_multipart_serialize(&h, mp), mp->mp_next->mp_next->mp_close_delim);

  TEST_1(msg_multipart_prepare(msg, mp, 0));

  TEST_1(mp = msg_multipart_dup(home, mp));

  su_home_check(home);
  su_home_zap(home);

  END();
}


/* Test serialization */
int test_serialize(void)
{
  msg_t *msg;
  su_home_t *home;
  msg_test_t *tst;
  msg_mime_version_t *mime;
  msg_separator_t *sep;
  msg_payload_t *pl;
  msg_accept_encoding_t *aen;
  msg_accept_language_t *ala;

  char const s[] =
    "GET /a-life HTTP/1.1" CRLF
    "Accept-Language: fi" CRLF
    "Accept-Encoding: z0" CRLF
    "Accept-Language: se, de" CRLF
    "Accept-Encoding: z1, z2" CRLF
    "Accept-Language: en, sv" CRLF
    "Accept-Encoding: z3, z4" CRLF
    "Content-Length: 6" CRLF
    CRLF
    "test" CRLF;

  BEGIN();

  msg = read_msg(s);

  TEST_1(msg); home = msg_home(msg);
  TEST_1(tst = msg_test_public(msg));
  TEST(msg_chain_errors((msg_header_t *)tst->msg_request), 0);

  TEST_1(ala = tst->msg_accept_language->aa_next->aa_next);
  TEST(msg_header_remove(msg, (msg_pub_t *)tst, (msg_header_t *)ala), 0);
  TEST_S(ala->aa_value, "de");

  TEST_1(ala = tst->msg_accept_language);
  TEST_1(ala = ala->aa_next); TEST_S(ala->aa_value, "se");
  /* Make sure that cached encoding of se is reset */
  TEST_1(ala->aa_common->h_data == NULL);
  TEST_1(ala->aa_common->h_len == 0);
  TEST_1(ala = ala->aa_next); TEST_S(ala->aa_value, "en");
  /* Make sure that cached encoding of en is kept intact */
  TEST_1(ala->aa_common->h_data != NULL);
  TEST_1(ala->aa_common->h_len != 0);

  TEST_1(aen = tst->msg_accept_encoding->aa_next->aa_next);
  TEST(msg_header_remove_all(msg, (msg_pub_t *)tst, (msg_header_t *)aen), 0);

  TEST_1(aen = tst->msg_accept_encoding);
  TEST_1(aen = aen->aa_next); TEST_S(aen->aa_value, "z1");
  /* Make sure that cached encoding of z1 is reset */
  TEST_1(aen->aa_common->h_data == NULL);
  TEST_1(aen->aa_common->h_len == 0);
  TEST_1(aen->aa_next == NULL);

  TEST_1(aen->aa_common->h_succ == (void *)ala);
  TEST_1(ala->aa_next->aa_common);
  TEST_1(ala->aa_next->aa_common->h_succ == (void *)tst->msg_content_length);

  TEST_1(mime = msg_mime_version_make(home, "1.0"));
  tst->msg_mime_version = mime;

  TEST(msg_serialize(msg, (msg_pub_t *)tst), 0);
  TEST(msg_chain_errors((msg_header_t *)tst->msg_request), 0);
  TEST_P(tst->msg_content_length->l_common->h_succ, mime);
  TEST_P(mime->g_common->h_succ, tst->msg_separator);

  msg_header_remove(msg, (msg_pub_t *)tst, (msg_header_t *)tst->msg_separator);
  TEST_1(sep = msg_separator_make(home, CRLF));
  tst->msg_separator = sep;
  TEST(msg_serialize(msg, (msg_pub_t *)tst), 0);
  TEST(msg_chain_errors((msg_header_t *)tst->msg_request), 0);
  TEST_P(mime->g_common->h_succ, sep);
  TEST_P(sep->sep_common->h_succ, tst->msg_payload);

  msg_header_remove(msg, (msg_pub_t *)tst, (msg_header_t *)tst->msg_payload);
  TEST_1(pl = msg_payload_make(home, "foobar" CRLF));
  pl->pl_next = tst->msg_payload;
  tst->msg_payload = pl;
  TEST(msg_serialize(msg, (msg_pub_t *)tst), 0);
  TEST(msg_chain_errors((msg_header_t *)tst->msg_request), 0);
  TEST_P(mime->g_common->h_succ, sep);
  TEST_P(sep->sep_common->h_succ, pl);
  TEST_P(pl->pl_common->h_succ, pl->pl_next);

  msg_destroy(msg);

  END();
}

static int random_test(void)
{
  struct { uint64_t low, mid, hi; } seed = { 0, 0, 0 };
  uint8_t zeros[24] = { 0 };
  uint8_t ones[24];

  char token[33];

  BEGIN();

  memset(ones, 255, sizeof ones);

  TEST_SIZE(msg_random_token(token, 32, (void *)&seed, sizeof(seed)), 32);
  TEST_S(token, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

  TEST_SIZE(msg_random_token(token, 32, zeros, 4), 7);
  TEST_S(token, "aaaaaaa");
  TEST_SIZE(msg_random_token(token, 32, ones, 4), 7);
  /* Last char may vary.. */
  token[6] = 0;  TEST_S(token, "999999");
  TEST_SIZE(msg_random_token(token, 32, zeros, 8), 13);
  TEST_S(token, "aaaaaaaaaaaaa");
  TEST_SIZE(msg_random_token(token, 32, zeros, 12), 20);
  TEST_S(token, "aaaaaaaaaaaaaaaaaaaa");

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
      test_flags |= tst_verbatim;
    else if (strcmp(argv[i], "-a") == 0)
      test_flags |= tst_abort;
    else
      usage(1);
  }

#if HAVE_OPEN_C
  test_flags |= tst_verbatim;
#endif

  retval |= msg_time_test(); fflush(stdout);
  retval |= addr_test(); fflush(stdout);
  retval |= hash_test(); fflush(stdout);
  retval |= random_test(); fflush(stdout);
  retval |= test_header_parsing(); fflush(stdout);
  retval |= test_msg_parsing(); fflush(stdout);
  retval |= test_warning(); fflush(stdout);
  retval |= test_msg_error(); fflush(stdout);
  retval |= test_mclass(); fflush(stdout);
  retval |= test_copy(); fflush(stdout);
  retval |= test_mime(); fflush(stdout);
  retval |= test_mime2(); fflush(stdout);
  retval |= test_serialize(); fflush(stdout);

  return retval;
}
