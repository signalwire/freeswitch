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

/**@CFILE torture_bnf.c
 *
 * Torture tests for BNF functions.
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

#include "sofia-sip/bnf.h"

static int test_flags = 0;
#define TSTFLAGS test_flags

#include <sofia-sip/tstdef.h>

char const name[] = "torture_bnf";

static int count_bnf(int bnf_flags)
{
  int i, n;

  for (i = 0, n = 0; i < 128; i++)
    if (_bnf_table[i] & bnf_flags)
      n++;

  return n;
}

static int bnf_test(void)
{
  BEGIN();
  TEST_1(IS_TOKEN('a'));
  TEST_1(IS_TOKEN('b'));
  /* Fixed for 1.12.2: lws = [*wsp crlf] 1*wsp */
  TEST_SIZE(span_lws("  \r\n \r\nLoppuu"), 5);
  TEST_SIZE(span_lws("  \r\r \nLoppuu"), 2);
  TEST_SIZE(span_lws("  \n\r \nLoppuu"), 2);
  TEST_SIZE(span_lws("  \r \nLoppuu"), 4);
  TEST_SIZE(span_lws("  \n\t \nLoppuu"), 5);
  TEST_SIZE(span_lws("  \r\n\t \nLoppuu"), 6);
  TEST_SIZE(span_token(SIP_TOKEN), strlen(SIP_TOKEN));
  TEST_SIZE(count_bnf(bnf_token), strlen(SIP_TOKEN "$"));
  #define SIP_PARAM SIP_TOKEN "[:]/"
  TEST_SIZE(span_param(SIP_PARAM), strlen(SIP_PARAM));
  TEST_SIZE(count_bnf(bnf_param), strlen(SIP_PARAM "$"));

  TEST_SIZE(span_unreserved(URL_UNRESERVED URL_ESCAPED),
	    strlen(URL_UNRESERVED URL_ESCAPED));

  TEST_SIZE(count_bnf(bnf_unreserved),
	    strlen(URL_UNRESERVED URL_ESCAPED));

  {
    char word[] = ALPHA DIGIT "-.!%*_+`'~()<>:\\\"/[]?{}";
    TEST_SIZE(span_word(word), strlen(word));
  }

  END();
}

static int ip_test(void)
{
  BEGIN();
  char *s;

  TEST(span_ip4_address("127.255.249.000,"), 15);
  TEST(span_ip4_address("0.00.000.000:,"), 12);

  /* Test error detection */
  TEST(span_ip4_address("256.00.000.000:,"), 0);
  TEST(span_ip4_address("255.00.000.0000,"), 0);
  TEST(span_ip4_address("255.00.000.199."), 0);

  {
    char ip0[] = "010.250.020.000,";
    char ip1[] = "0.00.000.000:,";
    char ip2[] = "256.00.000.000:,";
    char ip3[] = "255.00.000.0000,";
    char ip4[] = "255.00.000.199.";

    s = ip0;
    TEST(scan_ip4_address(&s), 15); TEST_S(s, ","); TEST_S(ip0, "10.250.20.0");

    s = ip1;
    TEST(scan_ip4_address(&s), 12); TEST_S(s, ":,"); TEST_S(ip1, "0.0.0.0");

    /* Test error detection */
    s = ip2; TEST(scan_ip4_address(&s), -1);
    s = ip3; TEST(scan_ip4_address(&s), -1);
    s = ip4; TEST(scan_ip4_address(&s), -1);
  }

  TEST(span_ip6_address("dead:beef:feed:ded:0:1:2:3"), 26);
  TEST(span_ip6_address("::beef:feed:ded:0:1:2:3"), 23);
  TEST(span_ip6_address("::255.0.0.0,"), 11);
  TEST(span_ip6_address("::,"), 2);

  TEST(span_ip_address("[dead:beef:feed:ded:0:1:2:3]"), 28);
  TEST(span_ip_address("::255.0.0.0,"), 11);
  TEST(span_ip_address("[::255.0.0.0]:"), 13);

  TEST(span_ip6_address("dead:beef:feed::0ded:0:1:2:3"), 28);
  TEST(span_ip6_address("dead:beef:feed::0ded:0000:0001:0002:0003"), 40);

  TEST(span_ip6_address("dead:beef:feed::0ded::0000:0001:0002:0003"), 0);
  TEST(span_ip6_address("::dead:beef:feed::0ded:0000:0001:0002:0003"), 0);
  TEST(span_ip6_address("dead:beef:feed:ded:0:1:2:3:4"), 0);
  TEST(span_ip6_address("dead:beef:feed:00ded:0:1:2:3"), 0);
  TEST(span_ip6_address("dead:beef:feed:ded:0:1:2:127.0.0.1"), 0);
  TEST(span_ip6_address(":255.0.0.0,"), 0);
  TEST(span_ip6_address("255.0.0.0,"), 0);

  /* Accept colon after IP4-quad */
  TEST(span_ip_address("::255.0.0.0:5060"), 11);

  /* This is a reference */
  TEST(span_ip6_address("[dead:beef:feed:ded:0:1:2:3]"), 0);
  TEST(span_ip6_reference("[dead:beef:feed:ded:0:1:2:3]:1"), 28);
  TEST(span_ip_address("[dead:beef:feed:ded:0:1:2:3]:1"), 28);
  TEST(span_ip_address("[127.0.0.1]:1"), 0);

  {
    char ip0[] = "dead:beef:feed:ded:0:1:2:3,";
    char ip1[] = "::beef:feed:ded:0:1:2:3;";
    char ip1b[] = "::beef:feed:ded:0:0:2:3;";
    char ip2[] = "::255.00.0.0,";
    char ip3[] = "::,";
    char ip4[] = "0:0:0:0:0:0:0:0,";
    char ip4b[] = "0:0:0:0:0:0:0.0.0.0,";
    char ip4c[] = "0:0:0:0:0:0:0.0.0.1,";
    char ip5[] = "dead:beef:feed::0ded:0:1:2:3";
    char ip6[] = "dead:beef:feed::0ded:0000:0001:0002:0003+";
    char ip7[] = "1:0:0:2:0:0:0:3,";
    char ip8[] = "1:0:0:2:0:0:3:4,";
    char ip9[] = "1::2:0:0:0:3,";

    s = ip0; TEST(scan_ip6_address(&s), 26); TEST_S(s, ",");
    TEST_S(ip0, "dead:beef:feed:ded::1:2:3");
    s = ip1; TEST(scan_ip6_address(&s), 23); TEST_S(s, ";");
    TEST_S(ip1, "::beef:feed:ded:0:1:2:3;");
    s = ip1b; TEST(scan_ip6_address(&s), 23); TEST_S(s, ";");
    TEST_S(ip1b, "0:beef:feed:ded::2:3");
    s = ip2; TEST(scan_ip6_address(&s), 12); TEST_S(s, ",");
    TEST_S(ip2, "::255.0.0.0");
    s = ip3; TEST(scan_ip6_address(&s), 2); TEST_S(s, ",");
    TEST_S(ip3, "::,");
    s = ip4; TEST(scan_ip6_address(&s), 15); TEST_S(s, ",");
    TEST_S(ip4, "::");
    s = ip4b; TEST(scan_ip6_address(&s), 19); TEST_S(s, ",");
    TEST_S(ip4b, "::");
    s = ip4c; TEST(scan_ip6_address(&s), 19); TEST_S(s, ",");
    TEST_S(ip4c, "::1");
    TEST_S(ip5, "dead:beef:feed::0ded:0:1:2:3");
    s = ip5; TEST(scan_ip6_address(&s), 28); TEST_S(s, "");
    TEST_S(ip5, "dead:beef:feed:ded::1:2:3");
    s = ip6; TEST(scan_ip6_address(&s), 40); TEST_S(s, "+");
    TEST_S(ip6, "dead:beef:feed:ded::1:2:3");
    s = ip7; TEST(scan_ip6_address(&s), 15); TEST_S(s, ",");
    TEST_S(ip7, "1:0:0:2::3");
    s = ip8; TEST(scan_ip6_address(&s), 15); TEST_S(s, ",");
    TEST_S(ip8, "1::2:0:0:3:4");
    s = ip9; TEST(scan_ip6_address(&s), 12); TEST_S(s, ",");
    TEST_S(ip9, "1:0:0:2::3");
  }

  {
    char err0[] = "dead:beef:feed::0ded::0000:0001:0002:0003";
    char err1[] = "::dead:beef:feed::0ded:0000:0001:0002:0003";
    char err2[] = "dead:beef:feed:ded:0:1:2:3:4";
    char err3[] = "dead:beef:feed:00ded:0:1:2:3";
    char err4[] = "dead:beef:feed:ded:0:1:2:127.0.0.1";
    char err5[] = ":255.0.0.0,";
    char err6[] = "255.0.0.0,";
    char err7[] = "dead:beef:feed:ded:0:1:2:3:4:5,";

    TEST(scan_ip6_address((s = err0, &s)), -1);
    TEST(scan_ip6_address((s = err1, &s)), -1);
    TEST(scan_ip6_address((s = err2, &s)), -1);
    TEST(scan_ip6_address((s = err3, &s)), -1);
    TEST(scan_ip6_address((s = err4, &s)), -1);
    TEST(scan_ip6_address((s = err5, &s)), -1);
    TEST(scan_ip6_address((s = err6, &s)), -1);
    TEST(scan_ip6_address((s = err7, &s)), -1);
  }

  {
    char err0[] = "[dead:beef:feed::0ded::0000:0001:0002:0003]:";
    char err1[] = "[::dead:beef:feed::0ded:0000:0001:0002:0003]+";
    char err2[] = "[dead:beef:feed:ded:0:1:2:3:4]+";
    char err3[] = "[dead:beef:feed:00ded:0:1:2:3]+";
    char err4[] = "[dead:beef:feed:ded:0:1:2:127.0.0.1]";
    char err5[] = "[:255.0.0.0],";
    char err6[] = "[255.0.0.0],";
    char err7[] = "[dead:beef:feed:ded:0:1:2:]";

    TEST(scan_ip6_reference((s = err0, &s)), -1);
    TEST(scan_ip6_reference((s = err1, &s)), -1);
    TEST(scan_ip6_reference((s = err2, &s)), -1);
    TEST(scan_ip6_reference((s = err3, &s)), -1);
    TEST(scan_ip6_reference((s = err4, &s)), -1);
    TEST(scan_ip6_reference((s = err5, &s)), -1);
    TEST(scan_ip6_reference((s = err6, &s)), -1);
    TEST(scan_ip6_reference((s = err7, &s)), -1);
  }

  END();
}

#define TEST_SCAN(scanner, input, canonic, output)			\
  do { char s0[] = input; char *s = s0;					\
    size_t n = sizeof(input) - sizeof(output);				\
    TEST_SIZE(scanner(&s), n);						\
    TEST_S(s, output);							\
    TEST_S(s0, canonic); } while(0)

#include <sofia-sip/hostdomain.h>

static int host_test(void)
{
  BEGIN();

  TEST(host_is_ip4_address(NULL), 0);
  TEST(host_is_ip6_address(NULL), 0);
  TEST(host_ip6_reference(NULL), 0);
  TEST(host_is_ip_address(NULL), 0);
  TEST(host_is_domain(NULL), 0);
  TEST(host_is_valid(NULL), 0);
  TEST(host_has_domain_invalid(NULL), 0);

  TEST_SIZE(span_host("rama"), 4);
  TEST_SIZE(span_host("ra-ma.1-2.3-4.a4-9."), 19);
  TEST_SIZE(span_host("a.1.b"), 5);
  TEST_SIZE(span_host("127.255.249.000.a,"), 17);
  TEST_SIZE(span_host("127.255.249.000,"), 15);
  TEST_SIZE(span_host("0.00.000.000:,"), 12);
  TEST_SIZE(span_host("127.255.249.000,"), 15);
  TEST_SIZE(span_host("[dead:beef:feed:ded:0:1:2:3]:1"), 28);
  TEST_SIZE(span_host("[dead:beef:feed:ded::1:2:3]:1"), 27);
  TEST_SIZE(span_host("[::127.0.0.1]:1"), 13);

  TEST_SCAN(scan_host, "rama", "rama", "");
  TEST_SCAN(scan_host, "rama.", "rama.", "");
  TEST_SCAN(scan_ip4_address, "127.255.249.000,", "127.255.249.0", ",");
  TEST_SCAN(scan_host, "127.255.249.000,", "127.255.249.0", ",");
  TEST_SCAN(scan_host, "a.1.b.", "a.1.b", "");
  TEST_SCAN(scan_host, "ra-ma.1-2.3-4.a4-9.", "ra-ma.1-2.3-4.a4-9", "");
  TEST_SCAN(scan_host, "127.255.249.000.a,", "127.255.249.000.a,", ",");
  TEST_SCAN(scan_host, "0.00.000.000:,", "0.0.0.0", ":,");
  TEST_SCAN(scan_host, "127.255.249.000,", "127.255.249.0", ",");
  TEST_SCAN(scan_host, "[dead:beef:feed:ded:0:1:2:3]:1",
                       "[dead:beef:feed:ded::1:2:3]", ":1");
  TEST_SCAN(scan_host, "[::127.0.0.1]:1", "[::127.0.0.1]:1", ":1");

  /* Test error detection */
  TEST_SIZE(span_host("256.00.000.000:,"), 0);
  TEST_SIZE(span_host("255.00.000.0000,"), 0);
  TEST_SIZE(span_host("255.00.000.199."), 0);
  TEST_SIZE(span_host("[127.0.0.1]:1"), 0);
  TEST_SIZE(span_domain("rama.1"), 0);
  TEST_SIZE(span_domain("-ma.1-2.3-4.a4-9."), 0);
  TEST_SIZE(span_domain("a..b"), 0);
  TEST_SIZE(span_domain("a.b.-"), 0);
  TEST_SIZE(span_domain("a.b-"), 0);

  TEST(host_is_local("126.0.0.0"), 0);
  TEST(host_is_local("127.0.0.0"), 1);
  TEST(host_is_local("127.0.0.2"), 1);
  TEST(host_is_local("0.0.0.0"), 0);
  TEST(host_is_local("::1"), 1);
  TEST(host_is_local("::1"), 1);
  TEST(host_is_local("::1:3fff"), 0);
  TEST(host_is_local("[::1]"), 1);
  TEST(host_is_local("localdomain.domain.org"), 0);
  TEST(host_is_local("localhost.domain.org"), 0);
  TEST(host_is_local("localhost"), 1);
  TEST(host_is_local("localhost.localdomain"), 1);
  TEST(host_is_local("localhost."), 1);
  TEST(host_is_local("localhost.localdomain."), 1);
  TEST(host_is_local("localhost.localdomain.org"), 0);

  TEST(host_has_domain_invalid("invalid"), 1);
  TEST(host_has_domain_invalid("invalid."), 1);
  TEST(host_has_domain_invalid("1.invalid"), 1);
  TEST(host_has_domain_invalid("1.invalid."), 1);
  TEST(host_has_domain_invalid("1invalid"), 0);
  TEST(host_has_domain_invalid("valid."), 0);
  TEST(host_has_domain_invalid("1-.invalid."), 0);

  TEST(host_is_domain("127.0.0.1"), 0);
  TEST(host_is_domain("3.com"), 1);
  TEST(host_is_domain("127.0.0.com"), 1);
  TEST(host_is_domain("actra.0.1"), 0);

  /* Invalid IP4 address (extra leading zeros) */
  TEST_1(!host_cmp("127.0.0.1", "127.0.0.01"));
  /* Invalid reference (extra leading zeros) */
  TEST_1(host_cmp("[0ffff:0ffff:0ffff:0ffff:0ffff:0ffff:255.255.255.255]",
		  "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"));
#if SU_HAVE_IN6
  TEST_1(!host_cmp("[ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255]",
		  "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"));
  TEST_1(!host_cmp("::1", "::001"));
  TEST_1(!host_cmp("[::1]", "::1"));
  TEST_1(!host_cmp("[::1]", "::0.0.0.1"));
  TEST_1(!host_cmp("::ffff:127.0.0.1", "127.0.0.1"));
  TEST_1(!host_cmp("::ffff:127.0.0.1", "::ffff:7f00:1"));
  TEST_1(!host_cmp("::ffff:127.0.0.1", "::ffff:7f00:1"));
  TEST_1(!host_cmp("[::ffff:127.0.0.1]", "[::7f00:1]"));
#endif
  TEST_1(host_cmp("::", "0.0.0.0"));
  TEST_1(host_cmp("::1", "0.0.0.1"));

  END();
}

static void usage(int exitcode)
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

  retval |= bnf_test(); fflush(stdout);
  retval |= ip_test(); fflush(stdout);
  retval |= host_test(); fflush(stdout);

  return retval;
}

