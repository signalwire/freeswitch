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

/**@internal
 * @file torture_su.c
 *
 * Testing functions for su socket functions.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu May  2 18:17:46 2002 ppessi
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sofia-sip/su.h>
#include <sofia-sip/su_localinfo.h>

int tstflags;

#define TSTFLAGS tstflags
#include <sofia-sip/tstdef.h>

char const *name = "torture_su";

/**  */
static int test_sockaddr(void)
{
  su_localinfo_t hints[1] = {{ LI_CANONNAME }};
  su_localinfo_t *li, *res = NULL;
  int s;
  su_sockaddr_t  su[1], a[1], b[1];

  BEGIN();

  hints->li_family = AF_INET;

  TEST(su_getlocalinfo(hints, &res), 0);

  for (li = res; li; li = li->li_next) {
    if (li->li_addrlen != res->li_addrlen ||
	memcmp(li->li_addr, res->li_addr, li->li_addrlen) != 0)
      TEST_1(su_cmp_sockaddr(li->li_addr, res->li_addr) != 0);
    else
      TEST_1(su_cmp_sockaddr(li->li_addr, res->li_addr) == 0);
  }

  memset(su, 0, sizeof su);
  TEST(su_getlocalip(su), 0);

  if (res->li_family == AF_INET)
    TEST(su_cmp_sockaddr(res->li_addr, su), 0);

  TEST_1(su_gli_strerror(ELI_NOERROR));
  TEST_1(su_gli_strerror(ELI_NOADDRESS));
  TEST_1(su_gli_strerror(ELI_FAMILY));
  TEST_1(su_gli_strerror(ELI_MEMORY));
  TEST_1(su_gli_strerror(ELI_RESOLVER));
  TEST_1(su_gli_strerror(ELI_SYSTEM));
  TEST_1(su_gli_strerror(-100));

  li = su_copylocalinfo(res); TEST_1(li);
  su_freelocalinfo(li);

  s = su_socket(res->li_family, SOCK_DGRAM, 0); TEST_1(s != -1);
  TEST(su_setblocking(s, 0), 0);
  TEST(su_setblocking(s, 1), 0);
  TEST(su_close(s), 0);

  su_freelocalinfo(res), res = NULL;

#if SU_HAVE_IN6
  hints->li_family = AF_INET6;
  hints->li_flags &= ~LI_CANONNAME;
  hints->li_flags |= LI_V4MAPPED;

  TEST(su_getlocalinfo(hints, &res), 0);
  for (li = res; li; li = li->li_next)
    TEST(li->li_family, AF_INET6);

  su_freelocalinfo(res), res = NULL;
#endif

  hints->li_flags |= LI_NUMERIC;
  TEST(su_getlocalinfo(hints, &res), 0);
  su_freelocalinfo(res), res = NULL;

  res = NULL;
  hints->li_flags |= LI_NAMEREQD;
  su_getlocalinfo(hints, &res);
  su_freelocalinfo(res), res = NULL;

  memset(a, 0, sizeof *a);
  memset(b, 0, sizeof *b);

  TEST_1(su_match_sockaddr(a, b));
  b->su_family = AF_INET;
  TEST_1(su_match_sockaddr(a, b));
  a->su_port = htons(12);
  TEST_1(!su_match_sockaddr(a, b));
  b->su_port = htons(12);
  TEST_1(su_match_sockaddr(a, b));
  a->su_sin.sin_addr.s_addr = htonl(0x7f000001);
  TEST_1(su_match_sockaddr(a, b));
  a->su_family = AF_INET;
  TEST_1(!su_match_sockaddr(a, b));
  b->su_sin.sin_addr.s_addr = htonl(0x7f000001);
  TEST_1(su_match_sockaddr(a, b));
  a->su_sin.sin_addr.s_addr = 0;
  TEST_1(su_match_sockaddr(a, b));
#if SU_HAVE_IN6
  a->su_family = AF_INET6;
  TEST_1(!su_match_sockaddr(a, b));
  b->su_family = AF_INET6;
  TEST_1(su_match_sockaddr(a, b));
  b->su_sin6.sin6_addr.s6_addr[15] = 1;
  TEST_1(su_match_sockaddr(a, b));
  TEST_1(!su_match_sockaddr(b, a));
  a->su_sin6.sin6_addr.s6_addr[15] = 2;
  TEST_1(!su_match_sockaddr(a, b));
  a->su_family = 0;
  TEST_1(su_match_sockaddr(a, b));
  TEST_1(!su_match_sockaddr(b, a));
#endif
  END();
}

#include <sofia-sip/su_wait.h>

int test_sendrecv(void)
{
  su_socket_t s, l, a;
  ssize_t n;
  su_sockaddr_t su, csu;
  socklen_t sulen = sizeof su.su_sin, csulen = sizeof csu.su_sin;
  char b1[8], b2[8], b3[8];
  su_iovec_t sv[3], rv[3];

  sv[0].siv_base = "one!one!", sv[0].siv_len = 8;
  sv[1].siv_base = "two!two!", sv[1].siv_len = 8;
  sv[2].siv_base = "third!", sv[2].siv_len = 6;

  rv[0].siv_base = b1, rv[0].siv_len = 8;
  rv[1].siv_base = b2, rv[1].siv_len = 8;
  rv[2].siv_base = b3, rv[2].siv_len = 8;

  BEGIN();

  s = su_socket(AF_INET, SOCK_DGRAM, 0); TEST_1(s != -1);
  su_setblocking(s, 1);

  memset(&su, 0, sulen);
  su.su_len = sulen;
  su.su_family = AF_INET;
  TEST(su_inet_pton(AF_INET, "127.0.0.1", &su.su_sin.sin_addr), 1);

  TEST(bind(s, &su.su_sa, sulen), 0);
  TEST(getsockname(s, &su.su_sa, &sulen), 0);

  n = su_vsend(s, sv, 3, 0, &su, sulen); TEST(n, 8 + 8 + 6);
  n = su_vrecv(s, rv, 3, 0, &su, &sulen); TEST(n, 8 + 8 + 6);

  TEST_M(rv[0].siv_base, sv[0].siv_base, sv[0].siv_len);
  TEST_M(rv[1].siv_base, sv[1].siv_base, sv[1].siv_len);
  TEST_M(rv[2].siv_base, sv[2].siv_base, sv[2].siv_len);

  su_close(s);

  l = su_socket(AF_INET, SOCK_STREAM, 0); TEST_1(l != -1);
  s = su_socket(AF_INET, SOCK_STREAM, 0); TEST_1(s != -1);

  memset(&su, 0, sulen);
  su.su_len = sulen;
  su.su_family = AF_INET;
  TEST(su_inet_pton(AF_INET, "127.0.0.1", &su.su_sin.sin_addr), 1);

  TEST(bind(l, &su.su_sa, sulen), 0);
  TEST(bind(s, &su.su_sa, sulen), 0);

  TEST(getsockname(l, &su.su_sa, &sulen), 0);
  TEST(listen(l, 5), 0);

  TEST(su_setblocking(s, 1), 0);

  TEST(connect(s, &su.su_sa, sulen), 0);
  a = accept(l, &csu.su_sa, &csulen); TEST_1(a != -1);

  TEST(su_setblocking(a, 1), 0);

  n = su_vsend(s, sv, 3, 0, NULL, 0); TEST(n, 8 + 8 + 6);
  n = su_vrecv(a, rv, 3, 0, NULL, NULL); TEST(n, 8 + 8 + 6);

  TEST_M(rv[0].siv_base, sv[0].siv_base, sv[0].siv_len);
  TEST_M(rv[1].siv_base, sv[1].siv_base, sv[1].siv_len);
  TEST_M(rv[2].siv_base, sv[2].siv_base, sv[2].siv_len);

  n = send(a, "", 0, 0); TEST(n, 0);
  n = su_vsend(a, sv, 3, 0, NULL, 0); TEST(n, 8 + 8 + 6);

  {
    su_wait_t w[1] = { SU_WAIT_INIT };

    TEST(su_wait_create(w, s, SU_WAIT_IN | SU_WAIT_HUP), 0);

    TEST(su_wait(w, 0, 500), SU_WAIT_TIMEOUT);

    TEST(su_wait(w, 1, 500), 0);
    TEST(su_wait_events(w, s), SU_WAIT_IN);

    TEST_SIZE(su_getmsgsize(s), 8 + 8 + 6);
    n = su_vrecv(s, rv, 3, 0, NULL, NULL); TEST(n, 8 + 8 + 6);

    TEST(su_wait(w, 1, 100), SU_WAIT_TIMEOUT);

    shutdown(a, 2);

    TEST(su_wait(w, 1, 100), 0);
#if SU_HAVE_WINSOCK
    TEST_1(su_wait_events(w, s) & SU_WAIT_HUP);
#else
    TEST_1(su_wait_events(w, s));
    n = su_vrecv(s, rv, 3, 0, NULL, NULL); TEST(n, 0);
#endif

    su_wait_destroy(w);
  }

  su_close(a);

  su_close(l);
  su_close(s);

  END();
}

#if HAVE_SELECT

#if HAVE_WIN32
int test_select(void)
{
  return 0;
}
#else

#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#elif HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <sys/types.h>
#include <unistd.h>

#ifndef __NFDBITS
#define __NFDBITS (8 * sizeof (long int))
#endif

#undef howmany
/* Size of fd set in bytes. Sorry, octets. */
#define howmany(n) (((n) + __NFDBITS - 1) / __NFDBITS * (__NFDBITS / 8))

#define FD_ZERO_TO(maxfd, set) \
  memset((set), 0, howmany(maxfd))

/* Test assumptions in su_select_port implementation */
int test_select(void)
{
  su_socket_t s;
  su_sockaddr_t su;
  socklen_t sulen = sizeof su.su_sin;
  size_t bytes;
  fd_set *rset, *wset;
  struct timeval tv;

  BEGIN();

  s = su_socket(AF_INET, SOCK_DGRAM, 0); TEST_1(s != -1);

  memset(&su, 0, sulen);
  su.su_len = sulen;
  su.su_family = AF_INET;
  TEST(su_inet_pton(AF_INET, "127.0.0.1", &su.su_sin.sin_addr), 1);
  TEST(bind(s, &su.su_sa, sulen), 0);
  TEST(getsockname(s, &su.su_sa, &sulen), 0);

  tv.tv_sec = 0; tv.tv_usec = 1000;
  TEST(select(0, NULL, NULL, NULL, &tv), 0);

  bytes = howmany(s);
  TEST_1(rset = malloc(bytes));
  TEST_1(wset = malloc(bytes));

  FD_ZERO_TO(s, rset); FD_ZERO_TO(s, wset); FD_SET(s, wset);
  tv.tv_sec = 0, tv.tv_usec = 1000;
  TEST(select(s + 1, NULL, wset, NULL, &tv), 1);
  TEST_1(FD_ISSET(s, wset));

  FD_ZERO_TO(s, rset); FD_ZERO_TO(s, wset);
  FD_SET(s, rset); FD_SET(s, wset);
  tv.tv_sec = 0, tv.tv_usec = 1000;
  TEST(select(s + 1, rset, wset, NULL, &tv), 1);
  TEST_1(!FD_ISSET(s, rset));
  TEST_1(FD_ISSET(s, wset));

  FD_ZERO_TO(s, rset); FD_ZERO_TO(s, wset);
  FD_SET(s, rset); FD_SET(s, wset);
  tv.tv_sec = 0, tv.tv_usec = 1000;
  TEST(select(s + 1, rset, NULL, NULL, &tv), 0);
  TEST_1(!FD_ISSET(s, rset));

  FD_ZERO_TO(s, rset); FD_ZERO_TO(s, wset);
  FD_SET(s, rset); FD_CLR(s, wset);
  tv.tv_sec = 0, tv.tv_usec = 1000;
  TEST(select(s + 1, rset, wset, NULL, &tv), 0);
  TEST_1(!FD_ISSET(s, rset));
  TEST_1(!FD_ISSET(s, wset));

  TEST(su_sendto(s, "foo", 3, 0, &su, sulen), 3);

  FD_ZERO_TO(s, rset); FD_ZERO_TO(s, wset);
  FD_SET(s, rset); FD_CLR(s, wset);
  tv.tv_sec = 0, tv.tv_usec = 1000;
  TEST(select(s + 1, rset, wset, NULL, &tv), 1);
  TEST_1(FD_ISSET(s, rset));
  TEST_1(!FD_ISSET(s, wset));

  FD_ZERO_TO(s, rset); FD_ZERO_TO(s, wset);
  FD_SET(s, rset); FD_SET(s, wset);
  tv.tv_sec = 0, tv.tv_usec = 1000;
  TEST(select(s + 1, rset, wset, NULL, &tv), 2);
  TEST_1(FD_ISSET(s, rset));
  TEST_1(FD_ISSET(s, wset));

  su_close(s);

  free(wset);
  free(rset);

  END();
}
#endif

#else
int test_select(void)
{
  return 0;
}
#endif

#include <sofia-sip/su_md5.h>

int test_md5(void)
{
  BEGIN();

  su_md5_t md5[1], md5i[1];
  uint8_t digest[SU_MD5_DIGEST_SIZE];
  char hexdigest[2 * SU_MD5_DIGEST_SIZE + 1];

  struct { char *input; uint8_t digest[SU_MD5_DIGEST_SIZE]; } suite[] = {
    { (""),
	{ 0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04,
	  0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e } },
    { ("a"), { 0x0c, 0xc1, 0x75, 0xb9, 0xc0, 0xf1, 0xb6, 0xa8,
	       0x31, 0xc3, 0x99, 0xe2, 0x69, 0x77, 0x26, 0x61 } },
    { ("abc"), { 0x90, 0x01, 0x50, 0x98, 0x3c, 0xd2, 0x4f, 0xb0,
		 0xd6, 0x96, 0x3f, 0x7d, 0x28, 0xe1, 0x7f, 0x72 } },
    { ("message digest"),
      { 0xf9, 0x6b, 0x69, 0x7d, 0x7c, 0xb7, 0x93, 0x8d,
	0x52, 0x5a, 0x2f, 0x31, 0xaa, 0xf1, 0x61, 0xd0 } },
    { ("abcdefghijklmnopqrstuvwxyz"),
      { 0xc3, 0xfc, 0xd3, 0xd7, 0x61, 0x92, 0xe4, 0x00,
	0x7d, 0xfb, 0x49, 0x6c, 0xca, 0x67, 0xe1, 0x3b } },
    { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
      { 0xd1, 0x74, 0xab, 0x98, 0xd2, 0x77, 0xd9, 0xf5,
	0xa5, 0x61, 0x1c, 0x2c, 0x9f, 0x41, 0x9d, 0x9f } },

    { "1234567890123456789012345678901234567890"
      "1234567890123456789012345678901234567890",
      { 0x57, 0xed, 0xf4, 0xa2, 0x2b, 0xe3, 0xc9, 0x55,
	0xac, 0x49, 0xda, 0x2e, 0x21, 0x07, 0xb6, 0x7a } }};

  su_md5_init(md5);
  su_md5_update(md5, suite[0].input, 0);
  su_md5_digest(md5, digest);
  TEST_M(digest, suite[0].digest, SU_MD5_DIGEST_SIZE);
  su_md5_deinit(md5);

  su_md5_init(md5);
  su_md5_strupdate(md5, suite[1].input);
  su_md5_digest(md5, digest);
  TEST_M(digest, suite[1].digest, SU_MD5_DIGEST_SIZE);
  su_md5_deinit(md5);

  su_md5_init(md5);
  su_md5_iupdate(md5, suite[2].input, 3);
  su_md5_digest(md5, digest);
  TEST_M(digest, suite[2].digest, SU_MD5_DIGEST_SIZE);
  su_md5_deinit(md5);

  su_md5_init(md5);
  su_md5_striupdate(md5, suite[3].input);
  su_md5_digest(md5, digest);
  TEST_M(digest, suite[3].digest, SU_MD5_DIGEST_SIZE);
  su_md5_deinit(md5);

  su_md5_init(md5);
  su_md5_iupdate(md5, suite[4].input, 13);
  su_md5_striupdate(md5, suite[4].input + 13);
  su_md5_digest(md5, digest);
  TEST_M(digest, suite[4].digest, SU_MD5_DIGEST_SIZE);
  su_md5_deinit(md5);

  su_md5_init(md5);
  su_md5_update(md5, suite[5].input, 13);
  su_md5_strupdate(md5, suite[5].input + 13);
  su_md5_digest(md5, digest);
  TEST_M(digest, suite[5].digest, SU_MD5_DIGEST_SIZE);
  su_md5_deinit(md5);

  su_md5_init(md5);
  su_md5_update(md5, suite[6].input, 13);
  su_md5_strupdate(md5, suite[6].input + 13);
  su_md5_digest(md5, digest);
  TEST_M(digest, suite[6].digest, SU_MD5_DIGEST_SIZE);
  su_md5_deinit(md5);

  su_md5_init(md5);
  su_md5_str0update(md5, NULL);
  su_md5_hexdigest(md5, hexdigest);
  TEST_S(hexdigest, "93b885adfe0da089cdf634904fd59f71");
  su_md5_deinit(md5);

  su_md5_init(md5);
  su_md5_stri0update(md5, NULL);
  su_md5_stri0update(md5, "ABBADABBADOO");
  su_md5_hexdigest(md5, hexdigest);
  TEST_S(hexdigest, "101e6dd7cfabdb5c74f44b4c545c05cc");

  su_md5_init(md5);
  su_md5_update(md5, "\0abbadabbadoo\0", 14);
  su_md5_hexdigest(md5, hexdigest);
  TEST_S(hexdigest, "101e6dd7cfabdb5c74f44b4c545c05cc");
  su_md5_deinit(md5);

  /* Calculate md5 sum of 512 MB of zero */
  if (getenv("EXPENSIVE_CHECKS")) {
    char zerokilo[1024] = { '\0' };
    int i;

    su_md5_init(md5);
    su_md5_iupdate(md5, zerokilo, 19);
    for (i = 1; i < 512 * 1024; i++)
      su_md5_update(md5, zerokilo, 1024);
    *md5i = *md5;

    su_md5_update(md5, zerokilo, 1024 - 19);
    su_md5_hexdigest(md5, hexdigest);
    TEST_S(hexdigest, "aa559b4e3523a6c931f08f4df52d58f2");
    su_md5_deinit(md5);

    su_md5_iupdate(md5i, zerokilo, 1024 - 19);
    su_md5_hexdigest(md5i, hexdigest);
    TEST_S(hexdigest, "aa559b4e3523a6c931f08f4df52d58f2");
  }
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

  su_init();

  for (i = 1; argv[i]; i++) {
    if (strcmp(argv[i], "-v") == 0)
      tstflags |= tst_verbatim;
    else if (strcmp(argv[i], "-a") == 0)
      tstflags |= tst_abort;
    else
      usage(1);
  }

  retval |= test_sockaddr();
  retval |= test_sendrecv();
  retval |= test_select();
  retval |= test_md5(); fflush(stdout);

  su_deinit();

  return retval;
}
