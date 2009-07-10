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

/**@file smoothsort.c
 * @brief Smoothsort implementation
 *
 * Smoothsort is a in-place sorting algorithm with performance of O(NlogN)
 * in worst case and O(n) in best case.
 *
 * @sa <a href="http://www.enterag.ch/hartwig/order/smoothsort.pdf">
 * "Smoothsort, an alternative for sorting in-situ", E.D. Dijkstra, EWD796a</a>,
 * &lt;http://www.enterag.ch/hartwig/order/smoothsort.pdf&gt;.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 */
#include <stdlib.h>
#include <sofia-sip/su_config.h>
#include "config.h"

#include <sofia-sip/heap.h>

#include <assert.h>
#include <stdio.h>
#include <sys/types.h>

/** Description of current stretch */
typedef struct {
  size_t b, c;		/**< Leonardo numbers */
  unsigned longlong p;		/**< Concatenation codification */
} stretch;

/** Description of array */
typedef struct
{
  void *m;
  int (*less)(void *m, size_t a, size_t b);
  void (*swap)(void *m, size_t a, size_t b);
} array;

static inline size_t stretch_up(stretch s[1])
{
  size_t next;

  s->p >>= 1;

  next = s->b + s->c + 1, s->c = s->b, s->b = next;

  return next;
}

static inline size_t stretch_down(stretch s[1], unsigned bit)
{
  size_t next;

  s->p <<= 1, s->p |= bit;

  next = s->c, s->c = s->b - s->c - 1, s->b = next;

  return next;
}

#if DEBUG_SMOOTHSORT
static char const *binary(unsigned long long p)
{
  static char binary[65];
  int i;

  if (p == 0)
    return "0";

  binary[64] = 0;

  for (i = 64; p; p >>= 1)
    binary[--i] = "01"[p & 1];

  return binary + i;
}
#else
#define DEBUG(x) ((void)0)
#endif

/**
 * Sift the root of the stretch.
 *
 * The low values are sifted up (towards index 0) from root.
 *
 * @param array   description of array to sort
 * @param r       root of the stretch
 * @param s       description of current stretch
 */
static void sift(array const *array, size_t r, stretch s)
{
  while (s.b >= 3) {
    size_t r2 = r - s.b + s.c;

    if (!array->less(array->m, r - 1, r2)) {
      r2 = r - 1;
      stretch_down(&s, 0);
    }

    if (array->less(array->m, r2, r))
      break;

    DEBUG(("\tswap(%p @%zu <=> @%zu)\n", array, r, r2));

    array->swap(array->m, r, r2); r = r2;

    stretch_down(&s, 0);
  }
}

/** Trinkle the roots of the given stretches
 *
 * @param array   description of array to sort
 * @param r       root of the stretch
 * @param s       description of stretches to concatenate
 */
static void trinkle(array const *array, size_t r, stretch s)
{
  DEBUG(("trinkle(%p, %zu, (%u, %s))\n", array, r, s.b, binary(s.p)));

  while (s.p != 0) {
    size_t r2, r3;

    while ((s.p & 1) == 0)
      stretch_up(&s);

    if (s.p == 1)
      break;

    r3 = r - s.b;

    if (array->less(array->m, r3, r))
      break;

    s.p--;

    if (s.b < 3) {
      DEBUG(("\tswap(%p @%zu <=> @%zu b=%u)\n", array,  r, r3, s.b));
      array->swap(array->m, r, r3); r = r3;
      continue;
    }

    r2 = r - s.b + s.c;

    if (array->less(array->m, r2, r - 1)) {
      r2 = r - 1;
      stretch_down(&s, 0);
    }

    if (array->less(array->m, r2, r3)) {
      DEBUG(("swap(%p [%zu]=[%zu])\n", array, r, r3));
      array->swap(array->m, r, r3); r = r3;
      continue;
    }

    DEBUG(("\tswap(%p @%zu <=> @%zu b=%u)\n", array, r, r2, s.b));
    array->swap(array->m, r, r2); r = r2;
    stretch_down(&s, 0);
    break;
  }

  sift(array, r, s);
}

/** Trinkles the stretches when the adjacent stretches are already trusty.
 *
 * @param array   description of array to sort
 * @param r       root of the stretch
 * @param stretch description of stretches to trinkle
 */
static void semitrinkle(array const *array, size_t r, stretch s)
{
  size_t r1 = r - s.c;

  DEBUG(("semitrinkle(%p, %zu, (%u, %s))\n", array, r, s.b, binary(s.p)));

  if (array->less(array->m, r, r1)) {
    DEBUG(("\tswap(%p @%zu <=> @%zu b=%u)\n", array, r, r1, s.b));
    array->swap(array->m, r, r1);
    trinkle(array, r1, s);
  }
}

/** Sort array using smoothsort.
 *
 * Sort @a N elements from array @a base starting with index @a r with smoothsort.
 *
 * @param base  pointer to array
 * @param r     lowest index to sort
 * @param N     number of elements to sort
 * @param less  comparison function returning nonzero if m[a] < m[b]
 * @param swap  swapper function exchanging elements m[a] and m[b]
 */
void su_smoothsort(void *base, size_t r, size_t N,
		   int (*less)(void *m, size_t a, size_t b),
		   void (*swap)(void *m, size_t a, size_t b))
{
  stretch s = { 1, 1, 1 };
  size_t q;

  array array_i;
  array* const array = &array_i;
  array->less = less;
  array->swap = swap;
  array->m = base;

  assert(less && swap);

  if (base == NULL || N <= 1 || less == NULL || swap == NULL)
    return;

  DEBUG(("\nsmoothsort(%p, %zu)\n", array, nmemb));

  for (q = 1; q != N; q++, r++, s.p++) {
    DEBUG(("loop0 q=%zu, b=%u, p=%s \n", q, s.b, binary(s.p)));

    if ((s.p & 7) == 3) {
      sift(array, r, s), stretch_up(&s), stretch_up(&s);
    }
    else /* if ((s.p & 3) == 1) */ { assert((s.p & 3) == 1);
      if (q + s.c < N)
	sift(array, r, s);
      else
	trinkle(array, r, s);

      while (stretch_down(&s, 0) > 1)
	;
    }
  }

  trinkle(array, r, s);

  for (; q > 1; q--) {
    s.p--;

    DEBUG(("loop1 q=%zu: b=%u p=%s\n", q, s.b, binary(s.p)));

    if (s.b <= 1) {
      while ((s.p & 1) == 0)
	stretch_up(&s);
      --r;
    }
    else /* if b >= 3 */ {
      if (s.p) semitrinkle(array, r - (s.b - s.c), s);
      stretch_down(&s, 1);
      semitrinkle(array, --r, s);
      stretch_down(&s, 1);
    }
  }
}
