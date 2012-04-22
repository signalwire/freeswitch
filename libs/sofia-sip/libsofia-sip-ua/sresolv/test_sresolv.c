/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2006 Nokia Corporation.
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
 *
 * @CFILE test_sresolv.c Test module for sresolv
 *
 * @author Mikko Haataja
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 */

#include "config.h"

#if HAVE_STDINT_H
#include <stdint.h>
#elif HAVE_INTTYPES_H
#include <inttypes.h>
#else
#if defined(_WIN32)
typedef unsigned _int8 uint8_t;
typedef unsigned _int16 uint16_t;
typedef unsigned _int32 uint32_t;
#endif
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if HAVE_NETINET_IN_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#if HAVE_WINSOCK2_H
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <sofia-resolv/sres.h>
#include <sofia-resolv/sres_async.h>
#include <sofia-resolv/sres_record.h>

#include <sofia-sip/su_alloc.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define TSTFLAGS tstflags
#include <sofia-sip/tstdef.h>

#if HAVE_POLL
#include <poll.h>
#elif HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#if HAVE_ALARM
#include <unistd.h>
#include <signal.h>
#endif

#include <sofia-sip/su_log.h>

extern su_log_t sresolv_log[];

char const name[] = "test_sresolv";

struct sres_context_s
{
  su_home_t        home[1];
  sres_resolver_t *resolver;
  sres_query_t    *query;
  sres_record_t  **result;

  int              timeout;
  sres_socket_t    sink;
  int              sinkidx;
  char const      *sinkconf;

  int              ready;
  int              n_sockets;
  sres_socket_t    sockets[SRES_MAX_NAMESERVERS];
#if HAVE_POLL
  struct pollfd    fds[SRES_MAX_NAMESERVERS];
#endif
};

static void test_answer(sres_context_t *ctx, sres_query_t *query,
			sres_record_t **answer);
static void test_answer_multi(sres_context_t *ctx, sres_query_t *query,
			      sres_record_t **answer);

static int tstflags = 0;

#if HAVE_WINSOCK2_H

/* Posix send() */
su_inline
ssize_t sres_send(sres_socket_t s, void *b, size_t length, int flags)
{
  if (length > INT_MAX)
    length = INT_MAX;
  return (ssize_t)send(s, b, (int)length, flags);
}

su_inline
ssize_t sres_sendto(sres_socket_t s, void *b, size_t length, int flags,
		    struct sockaddr const *sa, socklen_t salen)
{
  if (length > INT_MAX)
    length = INT_MAX;
  return (ssize_t)sendto(s, b, (int)length, flags, (void *)sa, (int)salen);
}

/* Posix recvfrom() */
su_inline
ssize_t sres_recvfrom(sres_socket_t s, void *buffer, size_t length, int flags,
		      struct sockaddr *from, socklen_t *fromlen)
{
  int retval, ilen;

  if (fromlen)
    ilen = *fromlen;

  if (length > INT_MAX)
    length = INT_MAX;

  retval = recvfrom(s, buffer, (int)length, flags,
		    (void *)from, fromlen ? &ilen : NULL);

  if (fromlen)
    *fromlen = ilen;

  return (ssize_t)retval;
}

static sres_socket_t sres_socket(int af, int socktype, int protocol)
{
  return socket(af, socktype, protocol);
}

su_inline
int sres_close(sres_socket_t s)
{
  return closesocket(s);
}

#if !defined(IPPROTO_IPV6)
#if HAVE_SIN6
#include <tpipv6.h>
#else
#if !defined(__MINGW32__)
struct sockaddr_storage {
    short ss_family;
    char ss_pad[126];
};
#endif
#endif
#endif

SOFIAPUBFUN int su_inet_pton(int af, char const *src, void *dst);
SOFIAPUBFUN const char *su_inet_ntop(int af, void const *src,
				  char *dst, size_t size);

#else

#define sres_send(s,b,len,flags) send((s),(b),(len),(flags))
#define sres_sendto(s,b,len,flags,a,alen) \
  sendto((s),(b),(len),(flags),(a),(alen))
#define sres_recvfrom(s,b,len,flags,a,alen) \
  recvfrom((s),(b),(len),(flags),(a),(alen))
#define sres_close(s) close((s))
#define SOCKET_ERROR   (-1)
#define INVALID_SOCKET ((sres_socket_t)-1)
#define sres_socket(x,y,z) socket((x),(y),(z))

#define su_inet_pton inet_pton
#define su_inet_ntop inet_ntop

#endif

#if 1

#if HAVE_POLL && 0

static
int setblocking(sres_socket_t s, int blocking)
{
  unsigned mode = fcntl(s, F_GETFL, 0);

  if (mode < 0)
     return -1;

  if (blocking)
    mode &= ~(O_NDELAY | O_NONBLOCK);
  else
    mode |= O_NDELAY | O_NONBLOCK;

  return fcntl(s, F_SETFL, mode);
}

/** Test few assumptions about sockets */
static
int test_socket(sres_context_t *ctx)
{
  int af;
  sres_socket_t s1, s2, s3, s4;
  struct sockaddr_storage a[1];
  struct sockaddr_storage a1[1], a2[1], a3[1], a4[1];
  struct sockaddr_in *sin = (void *)a;
  struct sockaddr_in *sin3 = (void *)a3, *sin4 = (void *)a4;
  struct sockaddr *sa = (void *)a;
  struct sockaddr *sa3 = (void *)a3, *sa4 = (void *)a4;
  socklen_t alen, a1len, a2len, a3len, a4len;
  char buf[16];

  BEGIN();

  af = AF_INET;

  for (;;) {
    TEST_1((s1 = sres_socket(af, SOCK_DGRAM, 0)) != INVALID_SOCKET);
    TEST_1((s2 = sres_socket(af, SOCK_DGRAM, 0)) != INVALID_SOCKET);
    TEST_1((s3 = sres_socket(af, SOCK_DGRAM, 0)) != INVALID_SOCKET);
    TEST_1((s4 = sres_socket(af, SOCK_DGRAM, 0)) != INVALID_SOCKET);

    TEST_1(setblocking(s1, 0) == 0);
    TEST_1(setblocking(s2, 0) == 0);
    TEST_1(setblocking(s3, 0) == 0);
    TEST_1(setblocking(s4, 0) == 0);

    memset(a, 0, sizeof a);
    memset(a1, 0, sizeof a1);
    memset(a2, 0, sizeof a2);
    memset(a3, 0, sizeof a3);
    memset(a4, 0, sizeof a4);

#if HAVE_SA_LEN
    a1->ss_len = a2->ss_len = a3->ss_len = a4->ss_len = sizeof a1;
#endif
    a1->ss_family = a2->ss_family = a3->ss_family = a4->ss_family = af;

    if (af == AF_INET)
      a1len = a2len = a3len = a4len = sizeof (struct sockaddr_in);
    else
      a1len = a2len = a3len = a4len = sizeof (struct sockaddr_in6);

    if (af == AF_INET) {
      TEST_1(su_inet_pton(af, "127.0.0.1", &sin3->sin_addr) > 0);
      TEST_1(su_inet_pton(af, "127.0.0.1", &sin4->sin_addr) > 0);
    } else {
    }

    TEST(bind(s3, (struct sockaddr *)a3, a3len), 0);
    TEST(bind(s4, (struct sockaddr *)a4, a4len), 0);

    alen = sizeof a;
    TEST(getsockname(s3, (struct sockaddr *)a, &alen), 0);
    sin3->sin_port = sin->sin_port;
    memset(sin->sin_zero, 0, sizeof sin->sin_zero);
    TEST(alen, a3len); TEST_M(a, a3, a3len);

    alen = sizeof a;
    TEST(getsockname(s4, (struct sockaddr *)a, &alen), 0);
    sin4->sin_port = sin->sin_port;
    memset(sin->sin_zero, 0, sizeof sin->sin_zero);
    TEST(alen, a4len); TEST_M(a, a4, a4len);

    TEST(connect(s1, sa3, a3len), 0);
    TEST(getsockname(s1, (struct sockaddr *)a1, &a1len), 0);
    TEST(connect(s2, sa4, a4len), 0);
    TEST(getsockname(s2, (struct sockaddr *)a2, &a2len), 0);

    TEST(sres_sendto(s1, "foo", 3, 0, sa4, a4len), 3);
    TEST(sres_recvfrom(s4, buf, sizeof buf, 0, sa, &alen), 3);
    TEST(sres_sendto(s4, "bar", 3, 0, sa, alen), 3);
    TEST(sres_recvfrom(s2, buf, sizeof buf, 0, sa, &alen), -1);
    TEST(sres_recvfrom(s1, buf, sizeof buf, 0, sa, &alen), 3);

    sres_close(s1), sres_close(s2), sres_close(s3), sres_close(s4);

    break;
  }

  END();
}

#endif

static unsigned offset;

#define TEST_RUN(ctx) \
  { sres_free_answers(ctx->resolver, ctx->result); ctx->result = NULL;	\
    ctx->query = NULL; run(ctx); TEST_1(ctx->query); }


static
int poll_sockets(sres_context_t *ctx)
{
  int i, n, events;

  n = ctx->n_sockets;

  events = poll(ctx->fds, ctx->n_sockets, ctx->timeout ? 50 : 500);

  if (!events)
    return events;

  for (i = 0; i < n; i++) {
    if (ctx->fds[i].revents & POLLERR)
      sres_resolver_error(ctx->resolver, ctx->fds[i].fd);
    if (ctx->fds[i].revents & POLLIN)
      sres_resolver_receive(ctx->resolver, ctx->fds[i].fd);
  }

  return events;
}

#define BREAK(ctx) (ctx->ready = 1)
static void run(sres_context_t *ctx)
{
  for (ctx->ready = 0; !ctx->ready; ) {
    poll_sockets(ctx);

    if (ctx->timeout) {
      ctx->timeout <<= 1;
      offset += ctx->timeout;
    }

    /* No harm is done (except wasted CPU) if timer is called more often */
    sres_resolver_timer(ctx->resolver, -1);
  }
}

int test_soa(sres_context_t *ctx)
{
  sres_resolver_t *res = ctx->resolver;
  sres_record_t **result;
  const sres_soa_record_t *rr_soa;

  char const *domain = "example.com";

  BEGIN();

  TEST_1(sres_query(res, test_answer, ctx, sres_type_soa, domain));
  TEST_RUN(ctx);

  TEST_1(sres_query(res, test_answer, ctx, sres_type_soa, domain));
  TEST_RUN(ctx);

  TEST_1(result = sres_cached_answers(res, sres_type_soa, domain));

  TEST_1(result != NULL);
  TEST_1(result[0] != NULL);

  rr_soa = result[0]->sr_soa;
  TEST(rr_soa->soa_record->r_type, sres_type_soa);
  TEST(rr_soa->soa_record->r_class, sres_class_in);

  TEST_S(rr_soa->soa_mname, "ns.example.com.");
  TEST_S(rr_soa->soa_rname, "root.example.com.");
  TEST(rr_soa->soa_serial, 2002042901);
  TEST(rr_soa->soa_refresh, 7200);
  TEST(rr_soa->soa_retry, 600);
  TEST(rr_soa->soa_expire, 36000000);
  TEST(rr_soa->soa_minimum, 60);

  sres_free_answers(res, result);

  END();
}

int test_naptr(sres_context_t *ctx)
{
  sres_resolver_t *res = ctx->resolver;
  sres_record_t **result;
  const sres_naptr_record_t *rr;
  char const *domain = "example.com";
  int i;

  BEGIN();

  TEST_1(sres_query(res, test_answer, ctx, sres_type_naptr, domain));
  TEST_RUN(ctx);

  TEST_1(result = ctx->result);
  TEST_1(result[0]);

  for (i = 0; result[i] != NULL; i++) {
    rr = (sres_naptr_record_t *) result[i]->sr_naptr;

    switch(rr->na_order) {
    case 20:
      TEST(rr->na_record->r_type, sres_type_naptr);
      TEST(rr->na_record->r_class, sres_class_in);
      TEST(rr->na_record->r_ttl, 60);
      TEST(rr->na_order, 20);
      TEST(rr->na_prefer, 50);
      TEST_S(rr->na_flags, "s");
      TEST_S(rr->na_services, "SIPS+D2T");
      TEST_S(rr->na_regexp, "");
      TEST_S(rr->na_replace, "_sips._tcp.example.com.");
      break;

    case 40:
      TEST(rr->na_record->r_type, sres_type_naptr);
      TEST(rr->na_record->r_class, sres_class_in);
      TEST(rr->na_record->r_ttl, 60);
      TEST(rr->na_order, 40);
      TEST(rr->na_prefer, 15);
      TEST_S(rr->na_flags, "s");
      TEST_S(rr->na_services, "SIP+D2U");
      TEST_S(rr->na_regexp, "");
      TEST_S(rr->na_replace, "_sip._udp.example.com.");
      break;

    case 50:
      TEST(rr->na_record->r_type, sres_type_naptr);
      TEST(rr->na_record->r_class, sres_class_in);
      TEST(rr->na_record->r_ttl, 60);
      TEST(rr->na_order, 50);
      TEST(rr->na_prefer, 15);
      TEST_S(rr->na_flags, "u");
      TEST_S(rr->na_services, "TEST+D2U");
      TEST_S(rr->na_regexp, "/(tst:([^@]+@))?example.com$/\\1operator.com/i");
      TEST_S(rr->na_replace, ".");
      break;

    case 80:
      TEST(rr->na_record->r_type, sres_type_naptr);
      TEST(rr->na_record->r_class, sres_class_in);
      TEST(rr->na_record->r_ttl, 60);
      TEST(rr->na_order, 80);
      TEST(rr->na_prefer, 25);
      TEST_S(rr->na_flags, "s");
      TEST_S(rr->na_services, "SIP+D2T");
      TEST_S(rr->na_regexp, "");
      TEST_S(rr->na_replace, "_sip._tcp.example.com.");
      break;

    default:
      TEST_1(0);
    }
  }

  sres_free_answers(res, ctx->result), ctx->result = NULL;

  END();
}

char const longname[1026] =
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa."
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa."
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa."
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa."

  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa."
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa."
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa."
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa."

  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa."
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa."
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa."
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa."

  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa."
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa."
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa."
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.";

char name2048[2049] =
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"
  "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
  "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
  "gggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg"
  "hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"

  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"
  "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
  "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
  "gggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg"
  "hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"

  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"
  "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
  "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
  "gggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg"
  "hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"

  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"
  "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
  "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
  "gggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg"
  "hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh";

int test_a(sres_context_t *ctx)
{
  sres_resolver_t *res = ctx->resolver;
  sres_query_t *query;
  sres_record_t **result;
  const sres_a_record_t *rr_a;
  char const *domain = "sip00.example.com";
  char name1025[1026] =
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"

		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"

		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"

		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"

		  ".";

  BEGIN();

  TEST_1(!sres_query(res, test_answer, ctx, sres_type_a, name1025));

  name1025[1024] = '\0';

  TEST_1(!sres_query(res, test_answer, ctx, sres_type_a, name1025));

  name1025[1023] = '\0';

  TEST_1(!sres_query(res, test_answer, ctx, sres_type_a, name1025));

  TEST_1(!sres_query(res, test_answer, ctx, sres_type_a, name2048));

  query = sres_query(res, test_answer, ctx, sres_type_a, longname);
  TEST_1(query);
  sres_query_bind(query, NULL, NULL);

  TEST_1(sres_query(res, test_answer, ctx, sres_type_a, domain));

  TEST_RUN(ctx);

  TEST_1(result = ctx->result);
  TEST_1(result[0]);
  TEST(result[0]->sr_record->r_status, 0);
  TEST_1(rr_a = result[0]->sr_a);
  TEST(rr_a->a_record->r_type, sres_type_a);
  TEST(rr_a->a_record->r_class, sres_class_in);
  TEST(rr_a->a_record->r_ttl, 60);

  TEST_S(inet_ntoa(rr_a->a_addr), "194.2.188.133");

  sres_free_answers(res, ctx->result), ctx->result = NULL;

  TEST_1(result = sres_cached_answers(res, sres_type_a, domain));
  TEST(result[0]->sr_record->r_status, 0);
  TEST_1(rr_a = result[0]->sr_a);
  TEST(rr_a->a_record->r_type, sres_type_a);
  TEST(rr_a->a_record->r_class, sres_class_in);
  TEST(rr_a->a_record->r_ttl, 60);
  TEST_S(inet_ntoa(rr_a->a_addr), "194.2.188.133");

  sres_free_answers(res, result);

  /* Try sub-queries */
  TEST_1(sres_search(res, test_answer, ctx, sres_type_a, "sip00"));

  TEST_RUN(ctx);

  TEST_1(result = ctx->result);

  for (;*result; result++)
    if (result[0]->sr_a->a_record->r_type == sres_type_a)
      break;

  TEST(result[0]->sr_record->r_status, 0);
  TEST_1(rr_a = result[0]->sr_a);
  TEST(rr_a->a_record->r_type, sres_type_a);
  TEST(rr_a->a_record->r_class, sres_class_in);
  TEST(rr_a->a_record->r_ttl, 60);
  TEST_S(inet_ntoa(rr_a->a_addr), "194.2.188.133");

  sres_free_answers(res, ctx->result), ctx->result = NULL;

  /* Try missing domain */
  TEST_1(sres_query(res, test_answer, ctx, sres_type_a,
			 "no-sip.example.com"));

  TEST_RUN(ctx);

  TEST_1(result = ctx->result);
  TEST(result[0]->sr_record->r_status, SRES_NAME_ERR);
  TEST_1(rr_a = result[0]->sr_a);
  TEST(rr_a->a_record->r_type, sres_type_a);
  TEST(rr_a->a_record->r_class, sres_class_in);
  /* Error gets TTL from example.com SOA record minimum time */
  TEST(rr_a->a_record->r_ttl, 60);

  sres_free_answers(res, ctx->result), ctx->result = NULL;

  /* Try domain without A record =>
     we should get a record with SRES_RECORD_ERR */
  TEST_1(sres_query(res, test_answer, ctx, sres_type_a,
			 "aaaa.example.com"));

  TEST_RUN(ctx);

  TEST_1(result = ctx->result);
  TEST(result[0]->sr_record->r_status, SRES_RECORD_ERR);
  TEST_1(rr_a = result[0]->sr_a);
  TEST(rr_a->a_record->r_type, sres_type_a);
  TEST(rr_a->a_record->r_class, sres_class_in);
  /* Error gets TTL from example.com SOA record minimum time */
  TEST(rr_a->a_record->r_ttl, 60);

  sres_free_answers(res, ctx->result), ctx->result = NULL;

  /* Cached search */
  TEST_1(result = sres_search_cached_answers(res, sres_type_a, "sip00"));
  TEST_1(rr_a = result[0]->sr_a);
  TEST(rr_a->a_record->r_status, 0);
  TEST_S(rr_a->a_record->r_name, "sip00.example.com.");
  TEST(rr_a->a_record->r_type, sres_type_a);
  TEST(rr_a->a_record->r_class, sres_class_in);
  TEST(rr_a->a_record->r_ttl, 60);
  TEST_S(inet_ntoa(rr_a->a_addr), "194.2.188.133");

  if (result[1]) {
    TEST(result[1]->sr_a->a_record->r_type, sres_type_a);
  }

  sres_free_answers(res, result), result = NULL;

  /* Cached search */
  TEST_1(result = sres_cached_answers(res, sres_type_a, "no-sip.example.com"));
  TEST_1(rr_a = result[0]->sr_a);
  TEST(rr_a->a_record->r_status, SRES_NAME_ERR);
  TEST(rr_a->a_record->r_type, sres_type_a);
  TEST(rr_a->a_record->r_class, sres_class_in);

  sres_free_answers(res, result), result = NULL;

  END();
}

#if HAVE_SIN6
int test_a6(sres_context_t *ctx)
{
  sres_resolver_t *res = ctx->resolver;
  sres_record_t **result;
  const sres_a6_record_t *rr_a6;
  char buf[50] = {0};
  char const *domain = "oldns.example.com";

  BEGIN();

  TEST_1(sres_query(res, test_answer, ctx, sres_type_a6, domain));
  TEST_RUN(ctx);

  TEST_1(result = ctx->result);
  TEST_1(result[0]);

  rr_a6 = result[0]->sr_a6;
  TEST(rr_a6->a6_record->r_type, sres_type_a6);
  TEST(rr_a6->a6_record->r_class, sres_class_in);
  TEST(rr_a6->a6_record->r_ttl, 60);
  TEST(rr_a6->a6_prelen, 0);

  TEST_S(su_inet_ntop(AF_INET6, &rr_a6->a6_suffix, buf, sizeof(buf)),
	 "3ffe:1200:3012:c000:210:a4ff:fe8d:6a46");

  TEST_P(rr_a6->a6_prename, NULL);

  sres_free_answers(res, ctx->result), ctx->result = NULL;

  TEST_1(result = sres_cached_answers(res, sres_type_a6, domain));
  TEST_1(rr_a6 = result[0]->sr_a6);
  TEST(rr_a6->a6_record->r_type, sres_type_a6);
  TEST(rr_a6->a6_record->r_class, sres_class_in);
  TEST(rr_a6->a6_record->r_ttl, 60);
  TEST(rr_a6->a6_prelen, 0);

  TEST_S(su_inet_ntop(AF_INET6, &rr_a6->a6_suffix, buf, sizeof(buf)),
	 "3ffe:1200:3012:c000:210:a4ff:fe8d:6a46");

  TEST_P(rr_a6->a6_prename, NULL);

  sres_free_answers(res, result), result = NULL;

  END();
}

int test_a6_prefix(sres_context_t *ctx)
{
  sres_resolver_t *res = ctx->resolver;
  sres_record_t **result;
  const sres_a6_record_t *rr_a6;
  char buf[50] = {0};
  char const *domain = "a6.example.com";

  BEGIN();

  TEST_1(sres_query(res, test_answer, ctx, sres_type_a6, domain));
  TEST_RUN(ctx);

  TEST_1(result = ctx->result);
  TEST_1(rr_a6 = result[0]->sr_a6);
  TEST(rr_a6->a6_record->r_type, sres_type_a6);
  TEST(rr_a6->a6_record->r_class, sres_class_in);
  TEST(rr_a6->a6_record->r_ttl, 60);
  TEST(rr_a6->a6_prelen, 64);

  TEST_S(su_inet_ntop(AF_INET6, &rr_a6->a6_suffix, buf, sizeof(buf)),
	 "::a08:20ff:fe7d:e7ac");

  TEST_S(rr_a6->a6_prename, "mynet.example.com.");

  sres_free_answers(res, ctx->result), ctx->result = NULL;

  /* Check parsing of special case: no prefix */
  TEST_1(sres_query(res, test_answer, ctx, sres_type_a6, "full.example.com"));
  TEST_RUN(ctx);

  TEST_1(result = ctx->result);
  TEST_1(rr_a6 = result[0]->sr_a6);
  TEST(rr_a6->a6_record->r_type, sres_type_a6);
  TEST(rr_a6->a6_record->r_class, sres_class_in);
  TEST(rr_a6->a6_record->r_ttl, 60);
  TEST(rr_a6->a6_prelen, 0);
  TEST_S(su_inet_ntop(AF_INET6, &rr_a6->a6_suffix, buf, sizeof(buf)),
	 "3ff0:12:3012:c006:a08:20ff:fe7d:e7ac");
  TEST_P(rr_a6->a6_prename, NULL);

  sres_free_answers(res, ctx->result), ctx->result = NULL;

  /* Check parsing of special case: no suffix */
  TEST_1(sres_query(res, test_answer, ctx, sres_type_a6, "alias6.example.com"));
  TEST_RUN(ctx);

  TEST_1(result = ctx->result);
  TEST_1(rr_a6 = result[0]->sr_a6);
  TEST(rr_a6->a6_record->r_type, sres_type_a6);
  TEST(rr_a6->a6_record->r_class, sres_class_in);
  TEST(rr_a6->a6_record->r_ttl, 60);
  TEST(rr_a6->a6_prelen, 128);
  TEST_S(su_inet_ntop(AF_INET6, &rr_a6->a6_suffix, buf, sizeof(buf)), "::");
  TEST_S(rr_a6->a6_prename, "a6.example.com.");

  sres_free_answers(res, ctx->result), ctx->result = NULL;

  TEST_1(result = sres_cached_answers(res, sres_type_a6, domain));

  TEST_1(rr_a6 = result[0]->sr_a6);
  TEST(rr_a6->a6_record->r_type, sres_type_a6);
  TEST(rr_a6->a6_record->r_class, sres_class_in);
  TEST(rr_a6->a6_record->r_ttl, 60);
  TEST(rr_a6->a6_prelen, 64);

  TEST_S(su_inet_ntop(AF_INET6, &rr_a6->a6_suffix, buf, sizeof(buf)),
	 "::a08:20ff:fe7d:e7ac");

  TEST_S(rr_a6->a6_prename, "mynet.example.com.");

  sres_free_answers(res, result), result = NULL;

  END();
}

int test_aaaa(sres_context_t *ctx)
{
  sres_resolver_t *res = ctx->resolver;
  sres_record_t **result;
  const sres_aaaa_record_t *rr_aaaa;
  char buf[50] = {0};
  char const *domain = "sip03.example.com";

  BEGIN();

  TEST_1(sres_query(res, test_answer, ctx, sres_type_aaaa, domain));
  TEST_RUN(ctx);

  TEST_1(result = ctx->result);
  TEST_1(rr_aaaa = result[0]->sr_aaaa);
  TEST(rr_aaaa->aaaa_record->r_type, sres_type_aaaa);
  TEST(rr_aaaa->aaaa_record->r_class, sres_class_in);
  TEST(rr_aaaa->aaaa_record->r_ttl, 60);

  TEST_S(su_inet_ntop(AF_INET6, &rr_aaaa->aaaa_addr, buf, sizeof(buf)),
	 "3ffe:1200:3012:c000:206:5bff:fe55:4630");

  sres_free_answers(res, ctx->result), ctx->result = NULL;

  TEST_1(result = sres_cached_answers(res, sres_type_aaaa, domain));

  TEST_1(rr_aaaa = result[0]->sr_aaaa);
  TEST(rr_aaaa->aaaa_record->r_type, sres_type_aaaa);
  TEST(rr_aaaa->aaaa_record->r_class, sres_class_in);
  TEST(rr_aaaa->aaaa_record->r_ttl, 60);

  TEST_S(su_inet_ntop(AF_INET6, &rr_aaaa->aaaa_addr, buf, sizeof(buf)),
	 "3ffe:1200:3012:c000:206:5bff:fe55:4630");

  sres_free_answers(res, result), result = NULL;

  END();
}
#endif /* HAVE_SIN6 */

int test_srv(sres_context_t *ctx)
{
  sres_resolver_t *res = ctx->resolver;
  sres_record_t **result;
  const sres_srv_record_t *rr;
  char const *domain = "_sips._tcp.example.com";
  int i;

  BEGIN();

  TEST_1(sres_query(res, test_answer, ctx, sres_type_srv, domain));
  TEST_RUN(ctx);

  TEST_1(result = ctx->result);

  for (i = 0; result[i] != NULL; i++) {
    TEST_1(rr = (sres_srv_record_t *) result[i]->sr_srv);

    switch(rr->srv_priority) {
    case 3:
      TEST(rr->srv_record->r_type, sres_type_srv);
      TEST(rr->srv_record->r_class, sres_class_in);
      TEST(rr->srv_record->r_ttl, 60);
      TEST(rr->srv_weight, 100);
      TEST(rr->srv_port, 5061);
      TEST_S(rr->srv_target, "sip00.example.com.");
      break;

    case 4:
      TEST(rr->srv_record->r_type, sres_type_srv);
      TEST(rr->srv_record->r_class, sres_class_in);
      TEST(rr->srv_record->r_ttl, 60);
      TEST(rr->srv_weight, 50);
      TEST(rr->srv_port, 5050);
      TEST_S(rr->srv_target, "sip02.example.com.");
      break;

    case 5:
      TEST(rr->srv_record->r_type, sres_type_srv);
      TEST(rr->srv_record->r_class, sres_class_in);
      TEST(rr->srv_record->r_ttl, 60);
      TEST(rr->srv_weight, 10);
      TEST(rr->srv_port, 5060);
      TEST_S(rr->srv_target, "sip01.example.com.");
      break;

    default:
      TEST_1(0);
    }
  }

  sres_free_answers(res, ctx->result), ctx->result = NULL;

  TEST_1(result = sres_cached_answers(res, sres_type_srv, domain));

  for (i = 0; result[i] != NULL; i++) {
    TEST_1(rr = (sres_srv_record_t *) result[i]->sr_srv);

    switch(rr->srv_priority) {
    case 3:
      TEST(rr->srv_record->r_type, sres_type_srv);
      TEST(rr->srv_record->r_class, sres_class_in);
      TEST(rr->srv_record->r_ttl, 60);
      TEST(rr->srv_weight, 100);
      TEST(rr->srv_port, 5061);
      TEST_S(rr->srv_target, "sip00.example.com.");
      break;

    case 4:
      TEST(rr->srv_record->r_type, sres_type_srv);
      TEST(rr->srv_record->r_class, sres_class_in);
      TEST(rr->srv_record->r_ttl, 60);
      TEST(rr->srv_weight, 50);
      TEST(rr->srv_port, 5050);
      TEST_S(rr->srv_target, "sip02.example.com.");
      break;

    case 5:
      TEST(rr->srv_record->r_type, sres_type_srv);
      TEST(rr->srv_record->r_class, sres_class_in);
      TEST(rr->srv_record->r_ttl, 60);
      TEST(rr->srv_weight, 10);
      TEST(rr->srv_port, 5060);
      TEST_S(rr->srv_target, "sip01.example.com.");
      break;

    default:
      TEST_1(0);
    }
  }

  sres_free_answers(res, result), result = NULL;

  END();
}

int test_cname(sres_context_t *ctx)
{
  sres_resolver_t *res = ctx->resolver;
  sres_record_t **result, *sr;
  const sres_cname_record_t *cn;
  char const *domain = "sip.example.com";

  BEGIN();

  TEST_1(sres_query(res, test_answer, ctx, sres_type_naptr, domain));
  TEST_RUN(ctx);

  TEST_1(result = ctx->result);
  TEST_1(sr = result[0]);
  TEST_1(sr->sr_record->r_status == SRES_RECORD_ERR);

  sres_free_answers(res, ctx->result), ctx->result = NULL;

  TEST_1(result = sres_cached_answers(res, sres_type_naptr, domain));
  TEST_1(sr = result[0]);
  TEST_1(sr->sr_record->r_status == SRES_RECORD_ERR);

  sres_free_answers(res, result), ctx->result = NULL;

  TEST_1(result = sres_cached_answers(res, sres_qtype_any, domain));
  TEST_1(cn = result[0]->sr_cname);
  TEST(cn->cn_record->r_class, sres_class_in);
  TEST(cn->cn_record->r_type, sres_type_cname);
  TEST(cn->cn_record->r_ttl, 60);
  TEST_S(cn->cn_cname, "sip00.example.com.");
  /* We might have A record, or then not */

  sres_free_answers(res, result), ctx->result = NULL;

  TEST_1(sres_query(res, test_answer, ctx, sres_type_a, domain));
  TEST_RUN(ctx);

  TEST_1(result = ctx->result);
  TEST_1(sr = result[0]);
  TEST(sr->sr_record->r_class, sres_class_in);
  TEST(sr->sr_record->r_type, sres_type_a);

  sres_free_answers(res, ctx->result), ctx->result = NULL;

  TEST_1(result = sres_cached_answers(res, sres_type_a, domain));
  TEST_1(sr = result[0]);
  TEST(sr->sr_record->r_class, sres_class_in);
  TEST(sr->sr_record->r_type, sres_type_a);

  sres_free_answers(res, result);

  domain = "cloop.example.com";

  TEST_1(sres_query(res, test_answer, ctx, sres_type_a, domain));
  TEST_RUN(ctx);

  TEST_1(result = ctx->result);
  TEST_1(sr = result[0]);
  TEST_1(sr->sr_record->r_status == SRES_RECORD_ERR);

  sres_free_answers(res, ctx->result), ctx->result = NULL;

  TEST_1(result = sres_cached_answers(res, sres_type_a, domain));
  TEST_1(sr = result[0]);
  TEST_1(sr->sr_record->r_status == SRES_RECORD_ERR);

  sres_free_answers(res, result);

  END();
}

int test_ptr_ipv4(sres_context_t *ctx)
{
  sres_resolver_t *res = ctx->resolver;
  sres_record_t **result;
  const sres_ptr_record_t *rr;
  char const *domain = "1.0.0.127.in-addr.arpa";

  BEGIN();

  TEST_1(sres_query(res, test_answer, ctx, sres_type_ptr, domain));
  TEST_RUN(ctx);

  result = ctx->result;
  TEST_1(result != NULL);
  TEST_1(result[0] != NULL);

  rr = result[0]->sr_ptr;
  TEST_1(rr != NULL);
  TEST(rr->ptr_record->r_class, sres_class_in);
  TEST(rr->ptr_record->r_type, sres_type_ptr);
  TEST_S(rr->ptr_domain, "localhost.");

  END();
}

int test_ptr_ipv4_sockaddr(sres_context_t *ctx)
{
  sres_resolver_t *res = ctx->resolver;
  sres_record_t **result;
  sres_query_t *query;
  const sres_ptr_record_t *rr;
  struct sockaddr_in sin = {0};

  su_inet_pton(AF_INET, "127.0.0.1", &sin.sin_addr);
  sin.sin_family = AF_INET;

  BEGIN();

  query = sres_query_sockaddr(res, test_answer, ctx,
				   sres_qtype_any, (struct sockaddr*)&sin);
  TEST_1(query != NULL);

  TEST_RUN(ctx);

  result = ctx->result;

  TEST_1(result != NULL);
  TEST_1(result[0] != NULL);

  rr = result[0]->sr_ptr;
  TEST_1(rr != NULL);
  TEST(rr->ptr_record->r_type, sres_type_ptr);
  TEST(rr->ptr_record->r_class, sres_class_in);
  TEST_S(rr->ptr_domain, "localhost.");

  END();
}

#if HAVE_SIN6
int test_ptr_ipv6(sres_context_t *ctx)
{
  sres_resolver_t *res = ctx->resolver;
  sres_record_t **result;
  const sres_ptr_record_t *rr;
  char const *domain =
    "c.a.7.e.d.7.e.f.f.f.0.2.8.0.a.0.0.0.0.c.2.1.0.3.0.0.2.1.e.f.f.3.ip6.int";

  BEGIN();

  TEST_1(sres_query(res, test_answer, ctx, sres_type_ptr, domain));
  TEST_RUN(ctx);

  result = ctx->result;

  TEST_1(result != NULL);
  TEST_1(result[0] != NULL);

  rr = result[0]->sr_ptr;
  TEST_1(rr != NULL);
  TEST(rr->ptr_record->r_type, sres_type_ptr);
  TEST(rr->ptr_record->r_class, sres_class_in);
  TEST_S(rr->ptr_domain, "sip01.example.com.");

  END();
}

int test_ptr_ipv6_sockaddr(sres_context_t *ctx)
{
  sres_resolver_t *res = ctx->resolver;
  sres_record_t **result;
  const sres_ptr_record_t *rr;
  struct sockaddr_in6 sin6 = {0};

  BEGIN();

  su_inet_pton(AF_INET6, "3ffe:1200:3012:c000:0a08:20ff:fe7d:e7ac",
	       &sin6.sin6_addr);

  sin6.sin6_family = AF_INET6;

  ctx->query =
    sres_query_sockaddr(res, test_answer, ctx,
			     sres_type_ptr, (struct sockaddr*)&sin6);
  TEST_1(ctx->query != NULL);

  TEST_RUN(ctx);

  result = ctx->result;

  TEST_1(result != NULL);
  TEST_1(result[0] != NULL);

  rr = result[0]->sr_ptr;
  TEST_1(rr != NULL);
  TEST(rr->ptr_record->r_type, sres_type_ptr);
  TEST(rr->ptr_record->r_class, sres_class_in);
  TEST_S(rr->ptr_domain, "sip01.example.com.");

  END();
}
#endif /* HAVE_SIN6 */

/* Test sres_cached_answers(), sres_sort_answers(), sres_free_answers() */
int test_cache(sres_context_t *ctx)
{
  sres_resolver_t *res = ctx->resolver;
  int ok = 0;
  sres_record_t *sort_array[3];
  sres_record_t **result;
  sres_query_t *query;
  const sres_record_t *rr = NULL;
  const sres_a6_record_t *rr_a6;
  const sres_aaaa_record_t *rr_aaaa;
  const sres_cname_record_t *rr_cname;
  const sres_ptr_record_t *rr_ptr;
#if HAVE_SIN6
  struct sockaddr_in6 sin6 = {0};
#endif
  char const *domain;
  char buf[50] = {0};
  int i, j;

  BEGIN();

  sres_query(res, test_answer_multi, ctx,
		  sres_qtype_any, "example.com");

  sres_query(res, test_answer_multi, ctx,
		  sres_qtype_any, "_sips._tcp.example.com");

  sres_query(res, test_answer_multi, ctx,
		  sres_qtype_any, "sip.example.com");

  sres_query(res, test_answer_multi, ctx,
		  sres_qtype_any, "subnet.example.com");

#if HAVE_SIN6
  sres_query(res, test_answer_multi, ctx,
	     sres_type_aaaa, "mgw02.example.com");

  su_inet_pton(AF_INET6,
	       "3ffe:1200:3012:c000:0a08:20ff:fe7d:e7ac",
	       &sin6.sin6_addr);

  sin6.sin6_family = AF_INET6;

  query = sres_query_sockaddr(res, test_answer_multi, ctx,
			      sres_qtype_any, (struct sockaddr *)&sin6);

  TEST_1(query != NULL);
#endif

  TEST_RUN(ctx);

  /* For a chance, a fully qualified domain name with final "." */
  domain = "example.com.";

  result = sres_cached_answers(res,
			       sres_qtype_any,
			       domain);

  TEST_1(result != NULL);

  for (i = 0; result[i] != NULL; i++) {
    rr = result[i];

    if (rr->sr_record->r_type == sres_type_naptr) {
      const sres_naptr_record_t *rr_naptr = rr->sr_naptr;
      switch(rr_naptr->na_order) {
      case 20:
        TEST(rr_naptr->na_record->r_type, sres_type_naptr);
        TEST(rr_naptr->na_record->r_class, sres_class_in);
        TEST(rr_naptr->na_record->r_ttl, 60);
        TEST(rr_naptr->na_order, 20);
        TEST(rr_naptr->na_prefer, 50);
        TEST_S(rr_naptr->na_flags, "s");
        TEST_S(rr_naptr->na_services, "SIPS+D2T");
        TEST_S(rr_naptr->na_regexp, "");
        TEST_S(rr_naptr->na_replace, "_sips._tcp.example.com.");
        ok |= 1;
        break;

      case 40:
        TEST(rr_naptr->na_record->r_type, sres_type_naptr);
        TEST(rr_naptr->na_record->r_class, sres_class_in);
        TEST(rr_naptr->na_record->r_ttl, 60);
        TEST(rr_naptr->na_order, 40);
        TEST(rr_naptr->na_prefer, 15);
        TEST_S(rr_naptr->na_flags, "s");
        TEST_S(rr_naptr->na_services, "SIP+D2U");
        TEST_S(rr_naptr->na_regexp, "");
        TEST_S(rr_naptr->na_replace, "_sip._udp.example.com.");
        ok |= 2;
        break;

      case 50:
        TEST(rr_naptr->na_record->r_type, sres_type_naptr);
        TEST(rr_naptr->na_record->r_class, sres_class_in);
        TEST(rr_naptr->na_record->r_ttl, 60);
        TEST(rr_naptr->na_order, 50);
        TEST(rr_naptr->na_prefer, 15);
        TEST_S(rr_naptr->na_flags, "u");
        TEST_S(rr_naptr->na_services, "TEST+D2U");

        TEST_S(rr_naptr->na_regexp,
	       "/(tst:([^@]+@))?example.com$/\\1operator.com/i");

        TEST_S(rr_naptr->na_replace, ".");
        break;

      case 80:
        TEST(rr_naptr->na_record->r_type, sres_type_naptr);
        TEST(rr_naptr->na_record->r_class, sres_class_in);
        TEST(rr_naptr->na_record->r_ttl, 60);
        TEST(rr_naptr->na_order, 80);
        TEST(rr_naptr->na_prefer, 25);
        TEST_S(rr_naptr->na_flags, "s");
        TEST_S(rr_naptr->na_services, "SIP+D2T");
        TEST_S(rr_naptr->na_regexp, "");
        TEST_S(rr_naptr->na_replace, "_sip._tcp.example.com.");
        ok |= 4;
        break;

      default:
        TEST_1(0);
      }
    }
  }

  TEST(ok, 7);

  /* Reverse order before sorting */
  for (j = 0; j < --i; j++) {
    sres_record_t *swap = result[j];
    result[j] = result[i];
    result[i] = swap;
  }

  /* Test sorting */
  sres_sort_answers(res, result);

  /* Sort all records with themselves */
  for (i = 0; result[i]; i++) {
    sort_array[0] = result[i], sort_array[1] = result[i], sort_array[2] = NULL;
    sres_sort_answers(res, sort_array);
  }

  /* Test free */
  for (i = 0; result[i]; i++) {
    sres_free_answer(res, result[i]);
    result[i] = NULL;
  }

  /* Test sres_free_answers() */
  sres_free_answers(res, result);

  result = sres_cached_answers(res,
			       sres_qtype_any,
			       "_sips._tcp.example.com");

  TEST_1(result != NULL);

  ok = 0;

  for (i = 0; result[i] != NULL; i++) {
    sres_srv_record_t *rr_srv = result[i]->sr_srv;

    TEST(rr_srv->srv_record->r_type, sres_type_srv);
    switch(rr_srv->srv_priority) {
    case 3:
      TEST(rr_srv->srv_record->r_type, sres_type_srv);
      TEST(rr_srv->srv_record->r_class, sres_class_in);
      TEST(rr_srv->srv_record->r_ttl, 60);
      TEST(rr_srv->srv_weight, 100);
      TEST(rr_srv->srv_port, 5061);
      TEST_S(rr_srv->srv_target, "sip00.example.com.");
      ok |= 1;
      break;

    case 4:
      TEST(rr_srv->srv_record->r_type, sres_type_srv);
      TEST(rr_srv->srv_record->r_class, sres_class_in);
      TEST(rr_srv->srv_record->r_ttl, 60);
      TEST(rr_srv->srv_weight, 50);
      TEST(rr_srv->srv_port, 5050);
      TEST_S(rr_srv->srv_target, "sip02.example.com.");
      ok |= 2;
      break;

    case 5:
      TEST(rr_srv->srv_record->r_type, sres_type_srv);
      TEST(rr_srv->srv_record->r_class, sres_class_in);
      TEST(rr_srv->srv_record->r_ttl, 60);
      TEST(rr_srv->srv_weight, 10);
      TEST(rr_srv->srv_port, 5060);
      TEST_S(rr_srv->srv_target, "sip01.example.com.");
      ok |= 4;
      break;

    default:
      TEST_1(0);
    }
  }

  TEST(ok, 7);

  /* Reverse order before sorting */
  for (j = 0; j < --i; j++) {
    sres_record_t *swap = result[j];
    result[j] = result[i];
    result[i] = swap;
  }

  sres_sort_answers(res, result);
  sres_free_answers(res, result);

  domain = "sip.example.com";
  result = sres_cached_answers(res,
			       sres_qtype_any,
			       domain);

  TEST_1(result != NULL);
  TEST_1(result[0] != NULL);

  rr_cname = result[0]->sr_cname;
  TEST(rr_cname->cn_record->r_type, sres_type_cname);
  TEST(rr_cname->cn_record->r_class, sres_class_in);
  TEST(rr_cname->cn_record->r_ttl, 60);
  TEST_S(rr_cname->cn_cname, "sip00.example.com.");

  sres_free_answers(res, result);

#if HAVE_SIN6
  domain = "subnet.example.com";
  result = sres_cached_answers(res,
			       sres_qtype_any,
			       domain);

  TEST_1(result != NULL);
  TEST_1(result[0] != NULL);

  rr_a6 = result[0]->sr_a6;
  TEST(rr_a6->a6_record->r_type, sres_type_a6);
  TEST(rr_a6->a6_record->r_class, sres_class_in);
  TEST(rr_a6->a6_record->r_ttl, 60);
  TEST(rr_a6->a6_prelen, 0);

  TEST_S(su_inet_ntop(AF_INET6, &rr_a6->a6_suffix, buf, sizeof(buf)), "3ff0::");

  TEST_P(rr_a6->a6_prename, NULL);

  sres_free_answers(res, result);

  domain = "mgw02.example.com";
  TEST_1(result = sres_cached_answers(res, sres_type_aaaa, domain));
  TEST_1(rr_aaaa = result[0]->sr_aaaa);
  TEST(rr_aaaa->aaaa_record->r_type, sres_type_aaaa);
  TEST(rr_aaaa->aaaa_record->r_class, sres_class_in);
  TEST(rr_aaaa->aaaa_record->r_ttl, 60);
  TEST_S(su_inet_ntop(AF_INET6, &rr_aaaa->aaaa_addr, buf, sizeof(buf)),
	 "3ffe:1200:3012:c000:206:5bff:fe55:462f");
  sres_free_answers(res, result);

  result = sres_cached_answers_sockaddr(res,
                                        sres_type_ptr,
                                        (struct sockaddr *)&sin6);

  TEST_1(result != NULL);

  rr_ptr = result[0]->sr_ptr;
  TEST_1(rr_ptr != NULL);

  TEST(rr_ptr->ptr_record->r_type, sres_type_ptr);
  TEST(rr_ptr->ptr_record->r_class, sres_class_in);
  TEST_S(rr_ptr->ptr_domain, "sip01.example.com.");

  sres_free_answers(res, result);
#endif /* HAVE_SIN6 */

  END();
}

#if HAVE_SIN6
int test_query_one_type(sres_context_t *ctx)
{
  sres_resolver_t *res = ctx->resolver;
  sres_record_t **result;
  const sres_aaaa_record_t *rr_aaaa;
  char buf[50] = {0};

  BEGIN();

  TEST_1(sres_query(res, test_answer, ctx,
			 sres_type_aaaa, "mgw02.example.com"));
  TEST_RUN(ctx);
  TEST_1(result = ctx->result);
  TEST_1(result[0] != NULL);

  TEST_1(rr_aaaa = result[0]->sr_aaaa);
  TEST(rr_aaaa->aaaa_record->r_type, sres_type_aaaa);
  TEST(rr_aaaa->aaaa_record->r_class, sres_class_in);
  TEST(rr_aaaa->aaaa_record->r_ttl, 60);
  TEST_S(su_inet_ntop(AF_INET6, &rr_aaaa->aaaa_addr, buf, sizeof(buf)),
	 "3ffe:1200:3012:c000:206:5bff:fe55:462f");

  TEST_P(result[1], NULL);

  END();
}
#endif /* HAVE_SIN6*/

#ifdef _WIN32
#include <fcntl.h>
#endif

int sink_make(sres_context_t *ctx)
{
  char *tmpdir = getenv("TMPDIR");
  char *template;
  int fd;
  sres_socket_t sink;
  struct sockaddr_in sin[1];
  socklen_t sinsize = sizeof sin;
  FILE *f;

  BEGIN();

  sink = socket(AF_INET, SOCK_DGRAM, 0); TEST_1(sink != INVALID_SOCKET);
  TEST(getsockname(sink, (struct sockaddr *)sin, &sinsize), 0);
  sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  TEST(bind(sink, (struct sockaddr *)sin, sinsize), 0);
  TEST(getsockname(sink, (struct sockaddr *)sin, &sinsize), 0);

  ctx->sink = sink;

  template = su_sprintf(ctx->home, "%s/test_sresolv.XXXXXX",
			tmpdir ? tmpdir : "/tmp");
  TEST_1(template);

#ifndef _WIN32
  fd = mkstemp(template); TEST_1(fd != -1);
#else
  fd = open(template, O_WRONLY); TEST_1(fd != -1);
#endif

  f = fdopen(fd, "w"); TEST_1(f);
  fprintf(f,
	  "domain example.com\n"
	  "search foo.bar.com\n"
	  "port %u\n",
	  ntohs(sin->sin_port));
  fclose(f);

  ctx->sinkconf = template;

  END();
}

#if 0
int recv_sink(su_root_magic_t *rm, su_wait_t *w, sres_context_t *ctx)
{
  union {
    char bytes[8192];
    unsigned short shorts[4096];
  } buffer[1];

  su_wait_events(w, ctx->sink);
  recv(ctx->sink, buffer->bytes, sizeof buffer, 0);

  return 0;
}

int sink_init(sres_context_t *ctx)
{
  su_wait_t w[1];
  BEGIN();

  TEST(su_wait_create(w, ctx->sink, SU_WAIT_IN), 0);
  ctx->sinkidx = su_root_register(ctx->root, w, recv_sink, ctx, 0);
  TEST_1(ctx->sinkidx != 0);

  END();
}

int sink_deinit(sres_context_t *ctx)
{
  BEGIN();

  if (ctx->sinkidx)
    su_root_deregister(ctx->root, ctx->sinkidx);
  ctx->sinkidx = 0;
  sres_close(ctx->sink), ctx->sink = INVALID_SOCKET;

  END();
}
#endif

int test_timeout(sres_context_t *ctx)
{
  sres_resolver_t *res = ctx->resolver;
  sres_record_t **result;
  /* const sres_soa_record_t *rr_soa; */
  char const *domain = "test";

  BEGIN();

  sres_query(res, test_answer, ctx, sres_type_a, domain);

  ctx->timeout = 1;
  TEST_RUN(ctx);
  ctx->timeout = 0;

  TEST_P(ctx->result, NULL);

  result = sres_cached_answers(res, sres_type_a, domain);

#if 0  /* Currently, we do not create error records */
  TEST_1(result); TEST_1(result[0] != NULL);

  rr_soa = result[0]->sr_soa;
  TEST(rr_soa->soa_record->r_type, sres_type_soa);
  TEST(rr_soa->soa_record->r_class, sres_class_in);
  TEST_S(rr_soa->soa_record->r_name, "example.com.");
  TEST(rr_soa->soa_record->r_status, SRES_TIMEOUT_ERR);

  sres_free_answers(res, result);
#else
  TEST_1(result == NULL);
#endif

  END();
}

static void test_answer(sres_context_t *ctx,
		 sres_query_t *q,
		 sres_record_t **answer)
{
  ctx->query = q;
  if (ctx->result)
    sres_free_answers(ctx->resolver, ctx->result);
  ctx->result = answer;
  BREAK(ctx);
}

static void test_answer_multi(sres_context_t *ctx,
		       sres_query_t *q,
		       sres_record_t **answer)
{
  static int count = 0;

  ctx->query = q;

  count++;

  sres_free_answers(ctx->resolver, answer);

  if (count == 6)
    BREAK(ctx);
}

#include <sys/time.h>

/* Fake time() implementation, used by sresolv library */
time_t time(time_t *tp)
{
  struct timeval tv[1];

#ifndef _WIN32
  gettimeofday(tv, NULL);
#else
  return 0;
#endif

  if (tp)
    *tp = tv->tv_sec + offset;

  return tv->tv_sec + offset;
}

int test_expiration(sres_context_t *ctx)
{
  sres_resolver_t *res = ctx->resolver;
  sres_record_t **result;
  char const *domain = "example.com";

  BEGIN();

  offset = 3600;		/* Time suddenly proceeds by an hour.. */

  sres_resolver_timer(res, -1);

  result = sres_cached_answers(res, sres_qtype_any, domain);

  TEST_P(result, NULL);  /* the cache should be empty after 15 secs */

  END();
}

#define is_hexdigit(c) ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))
#define hex(c) ((c >= '0' && c <= '9') ? (c - '0') : (c - 'a' + 10))

/* Convert lowercase hex to binary */
static
void *hex2bin(char const *test_name,
	      char const *hex1, char const *hex2, size_t *binsize)
{
  char output[2048];
  char *bin;
  char const *b;
  size_t j;

  if (hex1 == NULL || binsize == NULL)
    return NULL;

  for (b = hex1, j = 0; b;) {
    while (b[0]) {
      if (is_hexdigit(b[0])) {
	if (!is_hexdigit(b[1])) {
	  fprintf(stderr, "%s: hex2bin: invalid hex '%c'\n", test_name, b[1]);
	  exit(2);
	}

	output[j++] = (hex(b[0]) << 4) | hex(b[1]);
	if (j == sizeof(output)) {
	  fprintf(stderr, "%s:%u: hex2bin: buffer too small\n",
		  __FILE__, __LINE__);
	  exit(2);
	}
	b++;
      } else if (b[0] != ' ') {
	fprintf(stderr, "%s: hex2bin: invalid nonhex '%c'\n", test_name,
		b[0]);
	exit(2);
      }
      b++;
    }
    b = hex2, hex2 = NULL;
  }

  bin = malloc(j);
  if (bin == NULL)
    perror("malloc"), exit(2);

  return memcpy(bin, output, *binsize = j);
}

static char const hextest[] =
  "                        34 44 85 80 00 01 00 04 "
  "00 01 00 08 07 65 78 61 6d 70 6c 65 03 63 6f 6d "
  "00 00 23 00 01 c0 0c 00 23 00 01 00 00 00 3c 00 "
  "26 00 28 00 0f 01 73 07 53 49 50 2b 44 32 55 00 "
  "04 5f 73 69 70 04 5f 75 64 70 07 65 78 61 6d 70 "
  "6c 65 03 63 6f 6d 00 c0 42 00 23 00 01 00 00 00 "
  "3c 00 3e 00 32 00 0f 01 75 08 54 45 53 54 2b 44 "
  "32 55 2d 2f 28 74 73 74 3a 28 5b 5e 40 5d 2b 40 "
  "29 29 3f 65 78 61 6d 70 6c 65 2e 63 6f 6d 24 2f "
  "5c 31 6f 70 65 72 61 74 6f 72 2e 63 6f 6d 2f 69 "
  "00 c0 42 00 23 00 01 00 00 00 3c 00 26 00 50 00 "
  "19 01 73 07 53 49 50 2b 44 32 54 00 04 5f 73 69 "
  "70 04 5f 74 63 70 07 65 78 61 6d 70 6c 65 03 63 "
  "6f 6d 00 c0 be 00 23 00 01 00 00 00 3c 00 28 00 "
  "14 00 32 01 73 08 53 49 50 53 2b 44 32 54 00 05 "
  "5f 73 69 70 73 04 5f 74 63 70 07 65 78 61 6d 70 "
  "6c 65 03 63 6f 6d 00 c0 f2 00 02 00 01 00 00 00 "
  "3c 00 05 02 6e 73 c0 f2 05 73 69 70 30 30 c0 f2 "
  "00 01 00 01 00 00 00 3c 00 04 c2 02 bc 85 c1 10 "
  "00 1c 00 01 00 00 00 3c 00 10 3f f0 00 10 30 12 "
  "c0 00 02 c0 95 ff fe e2 4b 78 05 73 69 70 30 32 "
  "c0 f2 00 01 00 01 00 00 00 3c 00 04 c2 02 bc 87 "
  "c1 42 00 1c 00 01 00 00 00 3c 00 10 3f fe 12 00 "
  "30 12 c0 06 02 06 5b ff fe 55 46 2f 05 73 69 70 "
  "30 31 c0 f2 00 01 00 01 00 00 00 3c 00 04 c2 02 "
  "bc 86 c1 74 00 1c 00 01 00 00 00 3c 00 10 3f f0 "
  "00 12 30 12 c0 06 0a 08 20 ff fe 7d e7 ac c1 0b "
  "00 01 00 01 00 00 00 3c 00 04 c2 02 bc 85 c1 0b "
  "00 26 00 01 00 00 00 3c 00 11 00 3f fe 12 00 30 "
  "12 c0 00 02 10 a4 ff fe 8d 6a 46 ";

int test_net(sres_context_t *ctx)
{
  sres_resolver_t *res = ctx->resolver;
  sres_query_t *q = NULL;
  sres_socket_t c = ctx->sink;
  struct sockaddr_storage ss[1];
  struct sockaddr *sa = (void *)ss;
  socklen_t salen = sizeof ss;
  char *bin;
  size_t i, binlen;
  ssize_t n;
  char const *domain = "example.com";
  char query[512];

  BEGIN();

  TEST_1(ctx->sink != INVALID_SOCKET && ctx->sink != (sres_socket_t)0);

  /* Prepare for test_answer() callback */
  sres_free_answers(ctx->resolver, ctx->result);
  ctx->result = NULL;
  ctx->query = NULL;

  /* Get canned response */
  TEST_1(bin = hex2bin(__func__, hextest, NULL, &binlen));

  /* Send responses with one erroneus byte */
  for (i = 1; i < binlen; i++) {
    if (!q) {
      /* We got an error => make new query */
      TEST_1(q = sres_query(res, test_answer, ctx, /* Send query */
				 sres_type_naptr, domain));
      TEST_1((n = sres_recvfrom(c, query, sizeof query, 0, sa, &salen)) != -1);
      memcpy(bin, query, 2); /* Copy ID */
    }
    if (i != 1)
      bin[i] ^= 0xff;
    else
      bin[3] ^= SRES_FORMAT_ERR; /* format error -> EDNS0 failure */
    n = sres_sendto(c, bin, binlen, 0, sa, salen);
    if (i != 1)
      bin[i] ^= 0xff;
    else
      bin[3] ^= SRES_FORMAT_ERR;
    if (n == -1)
      perror("sendto");

    while (!poll_sockets(ctx))
      ;

    if (ctx->query)
      q = NULL;
  }

  /* Send runt responses */
  for (i = 1; i <= binlen; i++) {
    if (!q) {
      /* We got an error => make new query */
      TEST_1(q = sres_query(res, test_answer, ctx, /* Send query */
				 sres_type_naptr, domain));
      TEST_1((n = sres_recvfrom(c, query, sizeof query, 0, sa, &salen)) != -1);
      memcpy(bin, query, 2); /* Copy ID */
    }
    n = sres_sendto(c, bin, i, 0, sa, salen);
    if (n == -1)
      perror("sendto");
    while (!poll_sockets(ctx))
      ;

    if (ctx->query)
      q = NULL;
  }

  free(bin);

  END();
}

static
int test_init(sres_context_t *ctx, char const *conf_file)
{
  BEGIN();

  sres_resolver_t *res;
  int i, n;

  ctx->query = NULL, ctx->result = NULL;

  TEST_1(ctx->resolver = res = sres_resolver_new(conf_file));
  TEST(su_home_threadsafe((su_home_t *)ctx->resolver), 0);
  n = sres_resolver_sockets(res, NULL, 0);
  ctx->n_sockets = n;
  TEST_1(n < SRES_MAX_NAMESERVERS);
  TEST(sres_resolver_sockets(res, ctx->sockets, n), n);

  for (i = 0; i < n; i++) {
    ctx->fds[i].fd = ctx->sockets[i];
    ctx->fds[i].events = POLLIN | POLLERR;
  }

  TEST_P(sres_resolver_ref(ctx->resolver), ctx->resolver);
  sres_resolver_unref(ctx->resolver);

  END();
}

static
int test_deinit(sres_context_t *ctx)
{
  offset += 2 * 36000000;

  sres_resolver_timer(ctx->resolver, -1); /* Zap everything */

  sres_free_answers(ctx->resolver, ctx->result); ctx->result = NULL;

  su_free(ctx->home, (void *)ctx->sinkconf); ctx->sinkconf = NULL;

  sres_resolver_unref(ctx->resolver); ctx->resolver = NULL;

  offset = 0;
  memset(ctx, 0, sizeof ctx);
  ctx->home->suh_size = sizeof ctx;

  return 0;
}

static
int test_conf_errors(sres_context_t *ctx, char const *conf_file)
{
  sres_resolver_t *res;
  sres_socket_t socket;
  int n;

  BEGIN();

  TEST_1(res = sres_resolver_new(conf_file));
  n = sres_resolver_sockets(res, NULL, 0);
  TEST_1(n > 0);

  TEST(sres_resolver_sockets(res, &socket, 1), n);

#if !__linux
  /* We fail this test in most systems */
  /* conf_file looks like this:
--8<--8<--8<--8<--8<--8<--8<--8<--8<--8<--8<--8<--
nameserver 0.0.0.2
nameserver 1.1.1.1.1
search example.com
port $port
-->8-->8-->8-->8-->8-->8-->8-->8-->8-->8-->8-->8--
  */
  printf("%s:%u: %s test should be updated\n",
	 __FILE__, __LINE__, __func__);
#else
  TEST_P(sres_query(res, test_answer, ctx, sres_type_a, "example.com"), NULL);
#endif

  sres_resolver_unref(res);

  END();
}

void
fill_stack(void)
{
  int i,array[32768];

  for (i = 0; i < 32768; i++)
    array[i] = i ^ 0xdeadbeef;
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
	  "usage: %s OPTIONS [-] [conf-file] [error-conf-file]\n"
	  "\twhere OPTIONS are\n"
	  "\t    -v be verbose\n"
	  "\t    -a abort on error\n"
	  "\t    -l level\n",
	  name);
  exit(exitcode);
}

int main(int argc, char **argv)
{
  int i;
  int error = 0;
  int o_attach = 0, o_alarm = 1;
  sres_context_t ctx[1] = {{{SU_HOME_INIT(ctx)}}};

  for (i = 1; argv[i]; i++) {
    if (argv[i][0] != '-')
      break;
    else if (strcmp(argv[i], "-") == 0) {
      i++; break;
    }
    else if (strcmp(argv[i], "-v") == 0)
      tstflags |= tst_verbatim;
    else if (strcmp(argv[i], "-a") == 0)
      tstflags |= tst_abort;
    else if (strcmp(argv[i], "--no-alarm") == 0) {
      o_alarm = 0;
    }
    else if (strcmp(argv[i], "--attach") == 0) {
      o_attach = 1;
    }
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

      su_log_set_level(sresolv_log, level);
    } else
      usage(1);
  }

  if (o_attach) {
    char buf[8], *line;

    fprintf(stderr, "test_sresolv: started with pid %u"
	    " (press enter to continue)\n", getpid());

    line = fgets(buf, sizeof buf, stdin); (void) line;
  }
#if HAVE_ALARM
  else if (o_alarm) {
    alarm(60);
    signal(SIGALRM, sig_alarm);
  }
#endif

  if (!(TSTFLAGS & tst_verbatim)) {
    su_log_soft_set_level(sresolv_log, 0);
  }

#if 0
  if (sink_make(ctx) == 0)
  {
    error |= test_init(ctx, ctx->sinkconf);
    error |= sink_init(ctx);
    error |= test_net(ctx);
    error |= test_timeout(ctx);
    error |= sink_deinit(ctx);
    error |= test_deinit(ctx);
  }
#endif

  offset = 0;

  if (argv[i]) {
    /* These tests are run with (local) nameserver,
     */
    int initerror = test_init(ctx, argv[i]);
    if (!initerror) {
      error |= test_a(ctx);
      error |= test_soa(ctx);
      error |= test_naptr(ctx);
#if HAVE_SIN6
      error |= test_a6(ctx);
      error |= test_a6_prefix(ctx);
      error |= test_aaaa(ctx);
#endif
      error |= test_srv(ctx);
      error |= test_cname(ctx);
      error |= test_ptr_ipv4(ctx);
      error |= test_ptr_ipv4_sockaddr(ctx);
#if HAVE_SIN6
      error |= test_ptr_ipv6(ctx);
      error |= test_ptr_ipv6_sockaddr(ctx);
#endif
      error |= test_cache(ctx);
#if HAVE_SIN6
      error |= test_query_one_type(ctx);
#endif
      error |= test_expiration(ctx);
    }
    error |= test_deinit(ctx) | initerror;

    if (argv[i + 1]) {
      error |= test_conf_errors(ctx, argv[i + 1]);
    }
  }

  return error;
}


#else /* HAVE_POLL */

int main(int argc, char **argv)
{
  printf("*** Test not supported without POLL API ***\n");
  return 0;
}
#endif /* HAVE_POLL */
