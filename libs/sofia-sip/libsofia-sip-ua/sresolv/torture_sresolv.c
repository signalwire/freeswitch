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
 * @CFILE torture_sresolv.c Torture tests for Sofia resolver.
 *
 * @author Mikko Haataja
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
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
#include <sofia-resolv/sres_cache.h>

#include <sofia-sip/su_alloc.h>

#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#if HAVE_ALARM
#include <signal.h>
#endif

#define TSTFLAGS tstflags
int tstflags, o_timing;

#include <sofia-sip/tstdef.h>

char const name[] = "torture_sresolv";

struct sres_context_s
{
  su_home_t        home[1];
};

static void test_answer(sres_context_t *ctx,
			sres_query_t *q,
			sres_record_t **answer)
{
}

static char name2048[2049] =
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

/* Test API function argument validation */
static
int test_api_errors(void)
{
  sres_context_t ctx[1];
  sres_resolver_t *res;
  int s, fd;
  int sockets[20];
  struct sockaddr sa[1] = {{ 0 }};
  char *template = NULL;
  FILE *f;

  BEGIN();

  memset(ctx, 0, sizeof ctx);

  template = su_sprintf(ctx->home, ".torture_sresolv_api.conf.XXXXXX");
  TEST_1(template);

  TEST_1(res = sres_resolver_new(NULL));
  TEST(su_home_threadsafe((su_home_t *)res), 0);
  TEST_VOID(sres_resolver_unref(res));

#ifndef _WIN32
  fd = mkstemp(template); TEST_1(fd != -1);
#else
  fd = open(template, O_WRONLY); TEST_1(fd != -1);
#endif

  f = fdopen(fd, "w"); TEST_1(f);
  fprintf(f, "domain example.com\n");
  fclose(f);

  /* Test also LOCALDOMAIN handling */
  putenv("LOCALDOMAIN=localdomain");

  TEST_1(res = sres_resolver_new(template));
  TEST(su_home_threadsafe((su_home_t *)res), 0);

  unlink(template);

  s = sockets[0];

  TEST_P(sres_resolver_ref(NULL), NULL);
  TEST(errno, EFAULT);
  sres_resolver_unref(NULL);

  TEST_P(sres_resolver_set_userdata(NULL, NULL), NULL);
  TEST(errno, EFAULT);

  TEST_P(sres_resolver_get_userdata(NULL), NULL);

  TEST_P(sres_resolver_get_userdata(res), NULL);
  TEST_P(sres_resolver_set_userdata(res, sa), NULL);
  TEST_P(sres_resolver_get_userdata(res), sa);
  TEST_P(sres_resolver_set_userdata(res, NULL), sa);
  TEST_P(sres_resolver_get_userdata(res), NULL);

  errno = 0;
  TEST_P(sres_query(NULL, test_answer, ctx, sres_type_a, "com"), NULL);
  TEST(errno, EFAULT); errno = 0;
  TEST_P(sres_query(res, test_answer, ctx, sres_type_a, NULL), NULL);
  TEST(errno, EFAULT); errno = 0;
  TEST_P(sres_query_sockaddr(res, test_answer, ctx,
			     sres_qtype_any, sa), NULL);
  TEST(errno, EAFNOSUPPORT); errno = 0;

  TEST_P(sres_cached_answers(NULL, sres_qtype_any, "example.com"), NULL);
  TEST(errno, EFAULT); errno = 0;
  TEST_P(sres_cached_answers(res, sres_qtype_any, NULL), NULL);
  TEST(errno, EFAULT); errno = 0;
  TEST_P(sres_cached_answers(res, sres_qtype_any, name2048), NULL);
  TEST(errno, ENAMETOOLONG); errno = 0;
  TEST_P(sres_cached_answers_sockaddr(res, sres_qtype_any, sa), NULL);
  TEST(errno, EAFNOSUPPORT); errno = 0;

  sres_free_answer(res, NULL);
  sres_free_answers(res, NULL);
  sres_sort_answers(res, NULL);

  sres_free_answer(NULL, NULL);
  sres_free_answers(NULL, NULL);
  sres_sort_answers(NULL, NULL);

  sres_resolver_unref(res);

  END();
}

extern void sres_cache_clean(sres_cache_t *cache, time_t now);

#ifndef CLOCK_PROCESS_CPUTIME_ID
#define CLOCK_PROCESS_CPUTIME_ID CLOCK_REALTIME
#endif

static
int test_cache(void)
{
  BEGIN();

  sres_a_record_t *a, a0[1], **all;
  sres_record_t **copy;
  char host[128];
  sres_cache_t *cache;
  time_t now, base;
  struct timespec t0, t1, t2;

  size_t i, N, N1 = 1000, N3 = 1000000;

  time(&base);

  cache = sres_cache_new(N1);
  TEST_1(cache);

  all = calloc(N3, sizeof *all); if (!all) perror("calloc"), exit(2);

  memset(a0, 0, sizeof a0);

  a0->a_record->r_refcount = 1;
  a0->a_record->r_size = sizeof *a;
  a0->a_record->r_type = sres_type_a;
  a0->a_record->r_class = sres_class_in;
  a0->a_record->r_ttl = 3600;
  a0->a_record->r_rdlen = sizeof a->a_addr;
  a0->a_record->r_parsed = 1;

  for (i = 0, N = N3; i < N; i++) {
    a0->a_record->r_name = host;

    snprintf(host, sizeof host, "%u.example.com.", (unsigned)i);

    a = (sres_a_record_t *)
      sres_cache_alloc_record(cache, (sres_record_t *)a0, 0);

    if (!a)
      perror("sres_cache_alloc_record"), exit(2);

    all[i] = a, a->a_record->r_refcount = 1;
  }

  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t0);

  for (i = 0, N = N3; i < N; i++) {
    now = base + (3600 * i + N / 2) / N;
    a->a_record->r_ttl = 60 + (i * 60) % 3600;
    sres_cache_store(cache, (sres_record_t *)all[i], now);
  }

  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t1);
  if (o_timing) {
    t2.tv_sec = t1.tv_sec - t0.tv_sec, t2.tv_nsec = t1.tv_nsec - t0.tv_nsec;
    if (t1.tv_nsec < t0.tv_nsec)
      t2.tv_sec--, t2.tv_nsec += 1000000000;
    printf("sres_cache: stored %u entries: %lu.%09lu sec\n",
	   (unsigned)N, (long unsigned)t2.tv_sec, t2.tv_nsec);
  }

  for (i = 0, N; i < N; i++)
    TEST(all[i]->a_record->r_refcount, 2);

  TEST_1(copy = sres_cache_copy_answers(cache, (sres_record_t **)all));

  for (i = 0, N; i < N; i++)
    TEST(all[i]->a_record->r_refcount, 3);

  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t0);

  for (now = base; now <= base + 3660; now += 30)
    sres_cache_clean(cache, now + 3600);

  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t1);
  if (o_timing) {
    t2.tv_sec = t1.tv_sec - t0.tv_sec, t2.tv_nsec = t1.tv_nsec - t0.tv_nsec;
    if (t1.tv_nsec < t0.tv_nsec)
      t2.tv_sec--, t2.tv_nsec += 1000000000;
    printf("sres_cache: cleaned %u entries: %lu.%09lu sec\n",
	   (unsigned)N, (long unsigned)t2.tv_sec, t2.tv_nsec);
  }

  for (i = 0, N; i < N; i++)
    TEST(all[i]->a_record->r_refcount, 2);

  sres_cache_free_answers(cache, copy), copy = NULL;

  for (i = 0, N; i < N; i++)
    TEST(all[i]->a_record->r_refcount, 1);

  base += 24 * 3600;

  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t0);

  for (i = 0, N; i < N; i++) {
    now = base + (3600 * i + N / 2) / N;
    a->a_record->r_ttl = 60 + (i * 60) % 3600;
    sres_cache_store(cache, (sres_record_t *)all[i], now);
  }

  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t1);
  if (o_timing) {
    t2.tv_sec = t1.tv_sec - t0.tv_sec, t2.tv_nsec = t1.tv_nsec - t0.tv_nsec;
    if (t1.tv_nsec < t0.tv_nsec)
      t2.tv_sec--, t2.tv_nsec += 1000000000;
    printf("sres_cache: stored %u entries: %lu.%09lu sec\n",
	   (unsigned)N, (long unsigned)t2.tv_sec, t2.tv_nsec);
  }

  for (i = 0, N; i < N; i++)
    TEST(all[i]->a_record->r_refcount, 2);

  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t0);

  for (now = base; now <= base + 3660; now += 1)
    sres_cache_clean(cache, now + 3600);

  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t1);

  if (o_timing) {
    t2.tv_sec = t1.tv_sec - t0.tv_sec, t2.tv_nsec = t1.tv_nsec - t0.tv_nsec;
    if (t1.tv_nsec < t0.tv_nsec)
      t2.tv_sec--, t2.tv_nsec += 1000000000;
    printf("sres_cache: cleaned %u entries: %lu.%09lu sec\n",
	   (unsigned)N, (long unsigned)t2.tv_sec, t2.tv_nsec);
  }

  for (i = 0, N; i < N; i++) {
    TEST(all[i]->a_record->r_refcount, 1);
    sres_cache_free_one(cache, (sres_record_t *)all[i]);
  }

  sres_cache_unref(cache);

  free(all);

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
	  "usage: %s OPTIONS [-]\n"
	  "\twhere OPTIONS are\n"
	  "\t    -v be verbose\n"
	  "\t    -a abort on error\n"
	  "\t    -t show timing\n"
	  "\r    --no-alarm  disable timeout\n"
	  "\t    -llevel set debugging level\n",
	  name);
  exit(exitcode);
}

#include <sofia-sip/su_log.h>

extern su_log_t sresolv_log[];

int main(int argc, char **argv)
{
  int i;
  int error = 0;
  int o_alarm = 1;

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
    else if (strcmp(argv[i], "-t") == 0)
      o_timing = 1;
    else if (strcmp(argv[i], "--no-alarm") == 0) {
      o_alarm = 0;
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
    }
    else
      usage(1);
  }

#if HAVE_ALARM
  if (o_alarm) {
    alarm(60);
    signal(SIGALRM, sig_alarm);
  }
#endif

  if (!(TSTFLAGS & tst_verbatim)) {
    su_log_soft_set_level(sresolv_log, 0);
  }

  error |= test_api_errors();
  error |= test_cache();

  return error;
}
