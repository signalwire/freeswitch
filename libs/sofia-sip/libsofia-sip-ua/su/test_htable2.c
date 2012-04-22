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

/**@ingroup su_htable
 *
 * @CFILE test_htable.c
 *
 * Test functions for the @b su library hash table implementation.
 *
 * @internal
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

#include "sofia-sip/su_alloc.h"
#include "sofia-sip/htable2.h"

#define TSTFLAGS flags
#include <sofia-sip/tstdef.h>

char const name[] = "test_htable2";

void usage(int exitcode)
{
  fprintf(stderr, "usage: %s [-v|--verbatim] [-a|--abort]\n", name);
  exit(exitcode);
}

static int table2_test(int flags);

int main(int argc, char *argv[])
{
  int flags = 0;

  int retval = 0;
  int i;

  for (i = 1; argv[i]; i++) {
    if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbatim") == 0)
      flags |= tst_verbatim;
    else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--abort") == 0)
      flags |= tst_abort;
    else
      usage(1);
  }

  retval |= table2_test(flags); fflush(stdout);

  return retval;
}

typedef struct hentry_s entry_t;

struct hentry_s
{
  hash_value_t e_hash;
  unsigned long e_n;
};

HTABLE2_DECLARE2(htable2_t, htable2_s, ht2_, entry_t, size_t);
HTABLE2_PROTOS2(htable2_t, htable2, ht2_, entry_t, size_t);

#define HENTRY_HASH(e) ((e).e_hash)
#define HENTRY_IS_USED(e) ((e).e_n != 0)
#define HENTRY_REMOVE(e) ((e)->e_n = 0, (e)->e_hash = 0)
#define HENTRY_IS_EQUAL(a, b) ((a).e_n == (b).e_n)
#define HALLOC(home, old, newsize) (su_realloc(home, old, (isize_t)newsize))

HTABLE2_BODIES2(htable2_t, htable2, ht2_, entry_t, size_t,
	       HENTRY_HASH, HENTRY_IS_USED, HENTRY_REMOVE, HENTRY_IS_EQUAL,
	       HALLOC);

typedef struct context_s
{
  su_home_t c_home[1];
  htable2_t c_hash[1];
} context_t;

context_t *context_create(void)
{
  context_t *c = su_home_clone(NULL, sizeof(*c));

  if (c)
    htable2_resize(c->c_home, c->c_hash, 0);

  return c;
}

static
entry_t *search(context_t *c, unsigned long value, unsigned long n, int add)
{
  htable2_t *ht = c->c_hash;
  entry_t *e;
  unsigned long hash = value;

  /* Search for entry in hash table */
  for (e = htable2_hash(ht, hash);
       e->e_n != 0;
       e = htable2_next(ht, e)) {
    if (e->e_hash == value && e->e_n == n)
      return e;
  }

  if (add) {
    entry_t entry;

    /* Resize hash table */
    if (htable2_is_full(ht)) {
      htable2_resize(c->c_home, ht, 0);
      fprintf(stderr, "htable: resized to "MOD_ZU" with "MOD_ZU" entries\n",
	      ht->ht2_size, ht->ht2_used);
    }

    /* Add an entry */
    e = &entry, e->e_hash = value, e->e_n = n;

    return htable2_append(ht, *e);
  }

  return NULL;
}

static int add(context_t *c, unsigned long value, unsigned long n)
{
  return search(c, value, n, 1) != NULL;
}

static
void zap(context_t *c, entry_t e)
{
  htable2_remove(c->c_hash, e);
}

/*
 * Check that all n entries with hash h are in hash table
 * and they are stored in same order
 * in which they were added to the hash
 */
static unsigned long count(context_t *c, hash_value_t h)
{
  entry_t *e;
  unsigned long n;
  unsigned long expect = 1;

  for (e = htable2_hash(c->c_hash, h), n = 0;
       e->e_n != 0;
       e = htable2_next(c->c_hash, e)) {
    if (e->e_hash != h)
      continue;
    if (e->e_n == expect)
      n++, expect++;
  }

  return n;
}

int table2_test(int flags)
{
  context_t *c;
  entry_t *e, e0;

  BEGIN();

  TEST_1(c = context_create());
  TEST_1(add(c, 0, 1)); TEST_1(c->c_hash->ht2_table[0].e_n == 1);
  TEST_1(add(c, 1, 2)); TEST_1(c->c_hash->ht2_table[1].e_n == 2);
  TEST_1(add(c, 2, 3)); TEST_1(c->c_hash->ht2_table[2].e_n == 3);
  TEST_1(add(c, 0, 4)); TEST_1(c->c_hash->ht2_table[3].e_n == 4);
  TEST_1(add(c, 2, 5)); TEST_1(c->c_hash->ht2_table[4].e_n == 5);

  TEST_1(e = search(c, 1, 2, 0));
  e0 = *e;
  TEST(htable2_remove(c->c_hash, e0), 0);
  TEST(htable2_remove(c->c_hash, e0), -1);

  /* after remove , 4 is mode to [1], 5 to [4] */
  TEST(c->c_hash->ht2_table[0].e_n, 1);
  TEST(c->c_hash->ht2_table[1].e_n, 4);
  TEST(c->c_hash->ht2_table[2].e_n, 3);
  TEST(c->c_hash->ht2_table[3].e_n, 5);
  TEST(c->c_hash->ht2_table[4].e_n, 0);

  zap(c, c->c_hash->ht2_table[0]);

  /* after remove , 4 is mode to [1], 5 to [4] */
  TEST(c->c_hash->ht2_table[0].e_n, 4);
  TEST(c->c_hash->ht2_table[1].e_n, 0);
  TEST(c->c_hash->ht2_table[2].e_n, 3);
  TEST(c->c_hash->ht2_table[3].e_n, 5);
  TEST(c->c_hash->ht2_table[4].e_n, 0);

  TEST_1(add(c, 0, 6)); TEST(c->c_hash->ht2_table[1].e_n, 6);
  TEST_1(add(c, 1, 7)); TEST(c->c_hash->ht2_table[4].e_n, 7);

  /* Test that zapping entry 0 does not move 2 and 3 */
  zap(c, c->c_hash->ht2_table[0]);
  TEST(c->c_hash->ht2_table[4].e_n, 0);

  {
    /* Insert entries at the end of hash, then resize and check
       for correct ordering */
    size_t size = c->c_hash->ht2_size;
    hash_value_t h = (hash_value_t)size - 1;

    TEST_1(add(c, h, 1)); TEST_1(add(c, h, 2)); TEST_1(add(c, h, 3));
    TEST_1(add(c, h, 4)); TEST_1(add(c, h, 5)); TEST_1(add(c, h, 6));
    TEST_1(add(c, h, 7)); TEST_1(add(c, h, 8)); TEST_1(add(c, h, 9));

    TEST(count(c, h), 9);

    TEST(htable2_resize(c->c_home, c->c_hash, ++size), 0);
    TEST(count(c, h), 9);
    TEST(htable2_resize(c->c_home, c->c_hash, ++size), 0);
    TEST(count(c, h), 9);
    TEST(htable2_resize(c->c_home, c->c_hash, ++size), 0);
    TEST(count(c, h), 9);
    TEST(htable2_resize(c->c_home, c->c_hash, ++size), 0);
    TEST(count(c, h), 9);
    TEST(htable2_resize(c->c_home, c->c_hash, ++size), 0);
    TEST(count(c, h), 9);
    TEST(htable2_resize(c->c_home, c->c_hash, ++size), 0);
    TEST(count(c, h), 9);
    TEST(htable2_resize(c->c_home, c->c_hash, ++size), 0);
    TEST(count(c, h), 9);
    TEST(htable2_resize(c->c_home, c->c_hash, ++size), 0);
    TEST(count(c, h), 9);
    TEST(htable2_resize(c->c_home, c->c_hash, ++size), 0);
    TEST(count(c, h), 9);
  }

  TEST_VOID(su_home_unref(c->c_home));

  END();
}

