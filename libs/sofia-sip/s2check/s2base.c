/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2008 Nokia Corporation.
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

/**@CFILE s2base.c
 * @brief Common check-based tester for Sofia SIP modules
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Apr 30 12:48:27 EEST 2008 ppessi
 */

#include "config.h"

#undef NDEBUG

#define TP_MAGIC_T struct tp_magic_s

#include "s2base.h"

#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/msg_addr.h>
#include <sofia-sip/su_log.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_string.h>
#include <sofia-sip/sresolv.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <time.h>

#if HAVE_SYS_TIME_H
#include <sys/time.h> /* Get struct timeval */
#endif

#if HAVE_CLOCK_GETTIME
static double
now(void)
{
  struct timespec tv;

#if CLOCK_MONOTONIC
  if (clock_gettime(CLOCK_MONOTONIC, &tv) != 0)
#endif
    clock_gettime(CLOCK_REALTIME, &tv);

  return tv.tv_sec * 1.0 + tv.tv_nsec * 1e-9;
}
#elif HAVE_GETTIMEOFDAY
static double
now(void)
{
  struct timeval tv;

  gettimeofday(&tv, NULL);

  return tv.tv_sec * 1.0 + tv.tv_usec * 1e-6;
}

#else
#error no gettimeofday for test timing
#endif

/* -- Module globals ---------------------------------------------------- */

struct s2base *s2base;

char const *s2_tester = "s2_tester";
int s2_start_stop;

char const *_s2_suite = "X";
char const *_s2_case = "0.0";

static struct {
  double setup, start, done, toredown;
} stamps;

void s2_suite(char const *name)
{
  _s2_suite = name;
}

/** Basic setup for test cases */
void s2_setup(char const *label)
{
  assert(s2base == NULL);

  stamps.setup = now();

  if (s2_start_stop > 1) {
    printf("%s - setup %s test case\n", s2_tester, label ? label : "next");
  }

  su_init();

  s2base = su_home_new(sizeof *s2base);
  assert(s2base != NULL);

  s2base->root = su_root_create(s2base);
  assert(s2base->root != NULL);
}

void s2_case(char const *number,
	     char const *title,
	     char const *description,
	     char const *function)
{
  stamps.start = now();

  _s2_case = number;
  if (s2_start_stop)
    printf("%s - starting %s (%s/%s %s)\n", s2_tester, function,
	   _s2_suite, _s2_case, title);
}

void s2_step(void)
{
  su_root_step(s2base->root, 10);
}

static char const *s2_teardown_label = NULL;

void
s2_teardown_started(char const *label)
{
  stamps.done = now();

  if (!s2_teardown_label) {
    s2_teardown_label = label;
    if (s2_start_stop > 1) {
      double ms = (stamps.done - stamps.start) * 1000.0;
      printf("%s - tearing down %s test case (%g ms)\n", s2_tester, label, ms);
    }
  }
}

void
s2_teardown(void)
{
  struct s2base *_zap = s2base;

  s2base = NULL;

  su_root_destroy(_zap->root);
  su_deinit();

  stamps.toredown = now();

  if (s2_start_stop > 1) {
    double ms = (stamps.toredown - stamps.setup) * 1000.0;
    printf("%s - %s test case tore down (total %g ms)\n", s2_tester,
	   s2_teardown_label ? s2_teardown_label : "previous", ms);
  }

  s2_teardown_label = NULL;
}
