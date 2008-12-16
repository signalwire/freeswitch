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
 * @file torture_su_tag.c
 *
 * Testing functions for su_tag module.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Mar  6 18:33:42 2001 ppessi
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sofia-sip/su_tag.h>
#include <sofia-sip/su_tag_io.h>
#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_tagarg.h>

#ifndef WIN32
#define DEVNULL "/dev/null"
#else
#define DEVNULL "nul"
#endif

int tstflags = 0;

#define TSTFLAGS tstflags

#include <sofia-sip/tstdef.h>

#define TAG_A(s)      tag_a, tag_str_v((s))
#define TAG_A_REF(s)  tag_a_ref, tag_str_vr(&(s))
#define TAG_B(s)      tag_b, tag_str_v((s))
#define TAG_B_REF(s)  tag_b_ref, tag_str_vr(&(s))

#define TAG_I(i)      tag_i, tag_int_v((i))
#define TAG_I_REF(i)  tag_i_ref, tag_int_vr(&(i))
#define TAG_J(i)      tag_j, tag_int_v((i))
#define TAG_J_REF(i)  tag_j_ref, tag_int_vr(&(i))
#define TAG_K(i)      tag_k, tag_int_v((i))
#define TAG_K_REF(i)  tag_k_ref, tag_int_vr(&(i))

#define TAG_ANY_PQ()  tag_any_pq, (tag_value_t)0

#define TAG_P(i)      tag_p, tag_bool_v((i))
#define TAG_P_REF(i)  tag_p_ref, tag_bool_vr(&(i))
#define TAG_Q(i)      tag_q, tag_bool_v((i))
#define TAG_Q_REF(i)  tag_q_ref, tag_bool_vr(&(i))

char const *name = "su_tag_test";

#if HAVE_WIN32
typedef struct tag_type_s tag_typedef_win_t[1];

tag_typedef_win_t tag_a, tag_a_ref, tag_b, tag_b_ref;
tag_typedef_win_t tag_i, tag_i_ref, tag_j, tag_j_ref;
tag_typedef_win_t tag_k, tag_k_ref, tag_n, tag_n_ref;
tag_typedef_win_t tag_any_pq;
tag_typedef_win_t tag_p, tag_p_ref, tag_q, tag_q_ref;

#else
tag_typedef_t tag_a = STRTAG_TYPEDEF(a);
tag_typedef_t tag_a_ref = REFTAG_TYPEDEF(tag_a);

tag_typedef_t tag_b = STRTAG_TYPEDEF(b);
tag_typedef_t tag_b_ref = REFTAG_TYPEDEF(tag_b);

tag_typedef_t tag_i = INTTAG_TYPEDEF(i);
tag_typedef_t tag_i_ref = REFTAG_TYPEDEF(tag_i);

tag_typedef_t tag_j = INTTAG_TYPEDEF(j);
tag_typedef_t tag_j_ref = REFTAG_TYPEDEF(tag_j);

tag_typedef_t tag_k = INTTAG_TYPEDEF(k);
tag_typedef_t tag_k_ref = REFTAG_TYPEDEF(tag_k);

tag_typedef_t tag_n = INTTAG_TYPEDEF(n);
tag_typedef_t tag_n_ref = REFTAG_TYPEDEF(tag_n);

#undef TAG_NAMESPACE
#define TAG_NAMESPACE "pq"

tag_typedef_t tag_any_pq = NSTAG_TYPEDEF(*);

tag_typedef_t tag_p = BOOLTAG_TYPEDEF(p);
tag_typedef_t tag_p_ref = REFTAG_TYPEDEF(tag_p);

tag_typedef_t tag_q = BOOLTAG_TYPEDEF(q);
tag_typedef_t tag_q_ref = REFTAG_TYPEDEF(tag_q);

#endif

static void init_tags(void)
{
#if HAVE_WIN32
  /* Automatic initialization with pointers from DLL does not work in WIN32 */

  tag_typedef_t _tag_a = STRTAG_TYPEDEF(a);
  tag_typedef_t _tag_a_ref = REFTAG_TYPEDEF(tag_a);

  tag_typedef_t _tag_b = STRTAG_TYPEDEF(b);
  tag_typedef_t _tag_b_ref = REFTAG_TYPEDEF(tag_b);

  tag_typedef_t _tag_i = INTTAG_TYPEDEF(i);
  tag_typedef_t _tag_i_ref = REFTAG_TYPEDEF(tag_i);

  tag_typedef_t _tag_j = INTTAG_TYPEDEF(j);
  tag_typedef_t _tag_j_ref = REFTAG_TYPEDEF(tag_j);

  tag_typedef_t _tag_k = INTTAG_TYPEDEF(k);
  tag_typedef_t _tag_k_ref = REFTAG_TYPEDEF(tag_k);

  tag_typedef_t _tag_n = INTTAG_TYPEDEF(n);
  tag_typedef_t _tag_n_ref = REFTAG_TYPEDEF(tag_n);

#undef TAG_NAMESPACE
#define TAG_NAMESPACE "pq"

  tag_typedef_t _tag_any_pq = NSTAG_TYPEDEF(*);

  tag_typedef_t _tag_p = BOOLTAG_TYPEDEF(p);
  tag_typedef_t _tag_p_ref = REFTAG_TYPEDEF(tag_p);

  tag_typedef_t _tag_q = BOOLTAG_TYPEDEF(q);
  tag_typedef_t _tag_q_ref = REFTAG_TYPEDEF(tag_q);

  *(struct tag_type_s *)tag_a = *_tag_a;
  *(struct tag_type_s *)tag_a_ref = *_tag_a_ref;
  *(struct tag_type_s *)tag_b = *_tag_b;
  *(struct tag_type_s *)tag_b_ref = *_tag_b_ref;
  *(struct tag_type_s *)tag_i = *_tag_i;
  *(struct tag_type_s *)tag_i_ref = *_tag_i_ref;
  *(struct tag_type_s *)tag_j = *_tag_j;
  *(struct tag_type_s *)tag_j_ref = *_tag_j_ref;
  *(struct tag_type_s *)tag_k = *_tag_k;
  *(struct tag_type_s *)tag_k_ref = *_tag_k_ref;
  *(struct tag_type_s *)tag_n = *_tag_n;
  *(struct tag_type_s *)tag_n_ref = *_tag_n_ref;
  *(struct tag_type_s *)tag_any_pq = *_tag_any_pq;
  *(struct tag_type_s *)tag_p = *_tag_p;
  *(struct tag_type_s *)tag_p_ref = *_tag_p_ref;
  *(struct tag_type_s *)tag_q = *_tag_q;
  *(struct tag_type_s *)tag_q_ref = *_tag_q_ref;
#endif
}

static int test_assumptions(void)
{
  tagi_t tags[2], *tagi;

  BEGIN();

  TEST_1(sizeof (tag_value_t) >= sizeof (void *));

  /* First some pointer arithmetics - this is always true, right? */
  tagi = tags;
  TEST_P(tagi + 1, tags + 1);
  tagi = (tagi_t *)(sizeof(tagi_t) + (char *)tagi);
  TEST_P(tagi, tags + 1);

  END();
}

#if SU_HAVE_TAGSTACK

/* Automake thing:

###
### Test if we have stack suitable for handling tags directly
###
AC_TRY_RUN( [
#if HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#if HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdarg.h>

typedef void *tp;
typedef intptr_t tv;

int test1(tv l, tv h, ...)
{
  va_list ap;
  tv i, *p = &l;

  va_start(ap, h);

  if (*p++ != l || *p++ != h) return 1;

  for (i = l; i <= h; i++) {
    if (*p++ != i)
      return 1;
  }

  for (i = l; i <= h; i++) {
    if (va_arg(ap, tv) != i)
      return 1;
  }

  va_end(ap);

  return 0;
}

int main(int avc, char *av[])
{
  return test1((tv)1, (tv)10,
	       (tv)1, (tv)2, (tv)3, (tv)4, (tv)5,
	       (tv)6, (tv)7, (tv)8, (tv)9, (tv)10);
}
],
SAC_SU_DEFINE(SU_HAVE_TAGSTACK, 1, [
Define this as 1 if your compiler puts the variable argument list nicely in memory]),
)

*/

static int test_stackargs(int l, ...)
{
  va_list ap;
  int i, *p;

  BEGIN();

  va_start(ap, l);

  p = &l;

  for (i = l; i <= 10; i++) {
    TEST(*p++, i);
  }

  for (i = l + 1; i <= 10; i++) {
    TEST(va_arg(ap, int), i);
  }

  va_end(ap);

  END();
}
#else
static int test_stackargs(int l, ...) { return 0; }
#endif

/** Test tl_list, tl_llist and tl_dup */
static int test_dup(void)
{
  tagi_t *lst, *dup;
#if HAVE_OPEN_C
  tagi_t rest[2];
  rest[0].t_tag = tag_a;
  rest[0].t_value = tag_str_v("Foo");
  rest[1].t_tag = (tag_type_t)0;
  rest[0].t_value = (tag_value_t)0;
#else
  tagi_t const rest[] = {{ TAG_A("Foo") }, { TAG_NULL() }};
#endif

  BEGIN();

  lst = tl_list(TAG_A("Moro"),
		TAG_A("Vaan"),
		TAG_I(1),
		TAG_SKIP(2),
		TAG_NEXT(rest));

  TEST_P(lst[0].t_tag, tag_a);
  TEST_P(lst[1].t_tag, tag_a);
  TEST_P(lst[2].t_tag, tag_i);
  TEST_P(lst[3].t_tag, tag_skip);
  TEST_P(lst[4].t_tag, tag_next);

  TEST_SIZE(tl_len(lst), 5 * sizeof(tagi_t));
  TEST_SIZE(tl_xtra(lst, 0), strlen("Moro" "Vaan" "Foo") + 3);

  dup = tl_adup(NULL, lst);

  TEST_1(dup != NULL);
  TEST_SIZE(tl_len(dup), 5 * sizeof(tagi_t));
  TEST_SIZE(tl_xtra(dup, 0), strlen("Moro" "Vaan" "Foo") + 3);

  su_free(NULL, dup);

  dup = tl_llist(TAG_B("Moi"), TAG_NEXT(lst));

  TEST_P(dup[0].t_tag, tag_b);
  TEST_P(dup[1].t_tag, tag_a);
  TEST_P(dup[2].t_tag, tag_a);
  TEST_P(dup[3].t_tag, tag_i);
  TEST_P(dup[4].t_tag, tag_a);
  TEST_P(dup[5].t_tag, NULL);

  su_free(NULL, dup);

  dup = tl_llist(TAG_NEXT(NULL));
  TEST_P(dup[0].t_tag, 0);
  su_free(NULL, dup);

  dup = tl_llist(TAG_END());
  TEST_P(dup[0].t_tag, 0);
  su_free(NULL, dup);

  dup = tl_llist(TAG_SKIP(1), TAG_NEXT(lst + 4));
  TEST_P(dup[0].t_tag, tag_a);
  TEST_P(dup[1].t_tag, 0);
  su_free(NULL, dup);

  tl_vfree(lst);

  END();
}

/* filtering function */
int filter(tagi_t const *filter, tagi_t const *t)
{
  if (!t)
    return 0;

  /* Accept even TAG_I() */
  if (t->t_tag == tag_i)
    return (t->t_value & 1) == 0;

  /* TAG_Q(true) */
  if (t->t_tag == tag_q)
    return t->t_value != 0;

  /* TAG_P(false) */
  if (t->t_tag == tag_p)
    return t->t_value == 0;

  if (t->t_tag == tag_a) {
    char const *s = (char *)t->t_value;
    if (s && 'A' <= *s && *s <= 'Z')
      return 1;
  }

  return 0;
}

/* Test tl_afilter() */
static int test_filters(void)
{
  tagi_t *lst, *filter1, *filter2, *filter3, *filter4, *filter5, *filter6;
  tagi_t *b, *b1, *b2, *b3, *b4;

  tagi_t *nsfilter, *b5;
  su_home_t *home;

  BEGIN();

  home = su_home_new(sizeof *home); TEST_1(home);

  lst = tl_list(TAG_A("Moro"),
		TAG_I(2),
		TAG_Q(3),
		TAG_SKIP(2),
		TAG_A("vaan"),
		TAG_I(1),
		TAG_P(2),
		TAG_NULL());

  filter1 = tl_list(TAG_A(""), TAG_NULL());
  filter2 = tl_list(TAG_I(0), TAG_NULL());
  filter3 = tl_list(TAG_A(""), TAG_I(0), TAG_NULL());
  filter4 = tl_list(TAG_ANY(), TAG_NULL());
  filter5 = tl_list(TAG_FILTER(filter), TAG_NULL());
  filter6 = tl_list(TAG_NULL());

  TEST0(lst && filter1 && filter2 && filter3 && filter4 && filter5);

  b1 = tl_afilter(NULL, filter1, lst);

  TEST_SIZE(tl_len(b1), 3 * sizeof(tagi_t));
  TEST_SIZE(tl_xtra(b1, 0), strlen("Moro" "vaan") + 2);

  b2 = tl_afilter(NULL, filter2, lst);

  TEST_SIZE(tl_len(b2), 3 * sizeof(tagi_t));
  TEST_SIZE(tl_xtra(b2, 0), 0);

  b3 = tl_afilter(NULL, filter3, lst);

  TEST_SIZE(tl_len(b3), 5 * sizeof(tagi_t));
  TEST_SIZE(tl_xtra(b3, 0), strlen("Moro" "vaan") + 2);

  b4 = tl_afilter(NULL, filter4, lst);

  TEST_SIZE(tl_len(b4), 7 * sizeof(tagi_t));
  TEST_SIZE(tl_xtra(b4, 0), strlen("Moro" "vaan") + 2);

  su_free(NULL, b1); su_free(NULL, b2); su_free(NULL, b3); su_free(NULL, b4);

  nsfilter = tl_list(TAG_ANY_PQ(), TAG_END());

  b5 = tl_afilter(NULL, nsfilter, lst);
  TEST_SIZE(tl_len(b5), 3 * sizeof(tagi_t));
  TEST_SIZE(tl_xtra(b5, 0), 0);

  TEST_P(b5[0].t_tag, tag_q);
  TEST_P(b5[1].t_tag, tag_p);

  b = tl_afilter(home, filter5, lst); TEST_1(b);
  TEST_P(b[0].t_tag, tag_a);
  TEST_P(b[1].t_tag, tag_i);
  TEST_P(b[2].t_tag, tag_q);
  TEST_P(b[3].t_tag, 0);

  b = tl_afilter(home, filter6, lst); TEST_1(b);
  TEST_P(b[0].t_tag, 0);

  tl_vfree(filter1); tl_vfree(filter2); tl_vfree(filter3);
  tl_vfree(filter4); tl_vfree(filter5); tl_vfree(filter6);

  tl_vfree(lst);

  su_home_unref(home);

  END();
}

/* Test tl_print */
static int test_print(void)
{
  BEGIN();

  tagi_t *lst;
  FILE *out;

  lst = tl_list(TAG_A("Moro"),
		TAG_I(2),
		TAG_J(3),
		TAG_K(5),
		TAG_SKIP(2),
		TAG_A("Vaan"),
		TAG_B("b"),
		TAG_I(1),
		TAG_NULL());

  if (tstflags & tst_verbatim)
    out = stdout;
  else
    out = fopen(DEVNULL, "w");

  if (out) {
    tl_print(out, "test_print: ", lst);

    if ((tstflags & tst_verbatim) == 0)
      fclose(out);
  }

  END();
}

static int test_tagargs2(tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  tagi_t const *t;

  BEGIN();

  ta_start(ta, tag, value);

  t = ta_args(ta);
  TEST_P(t->t_tag, tag_a);
  TEST_S((char *)t->t_value, "a");

  t = tl_next(t);
  TEST_P(t->t_tag, tag_b);
  TEST_S((char *)t->t_value, "b");

  ta_end(ta);

  END();
}

/* Test tagargs) */
static int test_tagargs(void)
{
  return test_tagargs2(TAG_A("a"),
		       TAG_B("b"),
		       TAG_I(1),
		       TAG_J(2),
		       TAG_K(3),
		       TAG_P(0),
		       TAG_Q(-1),
		       TAG_END());
}

/* Test tl_tgets() and tl_gets() */
static int test_gets(void)
{
  tagi_t *lst, *t;
  char const *a = "B", *b = "A";
  int i = 1, j = 0, k = 0, p = -1;

  BEGIN();

  lst = tl_list(TAG_A("Moro"),
		TAG_I(2),
		TAG_J(3),
		TAG_K(5),
		TAG_P(3),
		TAG_SKIP(2),
		TAG_A("Vaan"),
		TAG_B("b"),
		TAG_I(1),
		TAG_NULL());

  TEST_1(lst);

  TEST_1(t = tl_find(lst, tag_i)); TEST(t->t_value, 2);
  TEST_1(t = tl_find_last(lst, tag_i)); TEST(t->t_value, 1);

  TEST(tl_gets(lst,
	       TAG_A_REF(a),
	       TAG_B_REF(b),
	       TAG_I_REF(i),
	       TAG_J_REF(j),
	       TAG_K_REF(k),
	       TAG_P_REF(p),
	       TAG_END()),
       6);

  /* tl_gets() semantics have changed */
#if 0
  TEST_S(a, "Moro");
  TEST(i, 2);
#else
  TEST_S(a, "Vaan");
  TEST(i, 1);
#endif
  TEST(j, 3);
  TEST(k, 5);
  TEST(p, 1);
  TEST_S(b, "b");

  lst = tl_list(TAG_A_REF(a),
		TAG_I_REF(i),
		TAG_NULL());

  TEST(tl_tgets(lst, TAG_A("Foo"), TAG_I(-1), TAG_END()), 2);

  TEST_S(a, "Foo");
  TEST(i, -1);

  END();
}

static int test_scan(void)
{
  tag_value_t v = 0;

  BEGIN();

  TEST(t_scan(tag_a, NULL, NULL, &v), -1); /* Invalid case */

  TEST(t_scan(tag_a, NULL, "foo", &v), 1);
  TEST_S((char *)v, "foo");
  su_free(NULL, (void *)v);

  TEST(t_scan(tag_i, NULL, "", &v), -1); /* Invalid case */
  TEST(t_scan(tag_i, NULL, "kukkuu-reset", &v), -1); /* Invalid case */

  TEST(t_scan(tag_i, NULL, "-1234", &v), 1);
  TEST(v, -1234);

  TEST(t_scan(tag_n, NULL, "", &v), -1); /* Invalid case */

  TEST(t_scan(tag_n, NULL, "3243432", &v), 1);
  TEST(v, 3243432);

  TEST(t_scan(tag_p, NULL, "kukkuu-reset", &v), -1); /* Invalid case */
  TEST(t_scan(tag_p, NULL, "", &v), -1); /* Invalid case */

  TEST(t_scan(tag_p, NULL, "true", &v), 1);
  TEST(v, 1);

  TEST(t_scan(tag_p, NULL, "true ", &v), 1);
  TEST(v, 1);

  TEST(t_scan(tag_p, NULL, "  134  ", &v), 1);
  TEST(v, 1);

  TEST(t_scan(tag_p, NULL, "false   \n\t", &v), 1);
  TEST(v, 0);

  TEST(t_scan(tag_p, NULL, "  000.000  ", &v), 1);
  TEST(v, 0);

  TEST(t_scan(tag_null, NULL, "NULL", &v), -2);
  TEST(t_scan(tag_skip, NULL, "SKIP", &v), -2);
  TEST(t_scan(tag_any, NULL, "any", &v), -2);

  TEST(t_scan(NULL, NULL, "-1234", &v), -1);
  TEST(t_scan(tag_a, NULL, NULL, &v), -1);
  TEST(t_scan(tag_a, NULL, "-1234", NULL), -1);

  END();
}

void usage(int exitcode)
{
  fprintf(stderr,
	  "usage: %s [-v] [-a]\n",
	  name);
  exit(exitcode);
}

int main(int argc, char *argv[])
{
  int retval = 0;
  int i;

  init_tags();

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

  retval |= test_assumptions();
  retval |= test_stackargs(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
  retval |= test_dup();
  retval |= test_filters();
  retval |= test_print();
  retval |= test_tagargs();
  retval |= test_gets();
  retval |= test_scan();

  return retval;
}
