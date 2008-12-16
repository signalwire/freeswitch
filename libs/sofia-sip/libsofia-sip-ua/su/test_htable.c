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
#include "sofia-sip/htable.h"

#define TSTFLAGS flags
#include <sofia-sip/tstdef.h>

char const name[] = "htable_test";

void usage(int exitcode)
{
  fprintf(stderr, "usage: %s [-v|--verbatim] [-a|--abort]\n", name);
  exit(exitcode);
}

static int table_test(int flags);

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

  retval |= table_test(flags); fflush(stdout);

  return retval;
}

typedef struct hentry_s entry_t;
HTABLE_DECLARE(htable, ht, entry_t);
HTABLE_PROTOS(htable, ht, entry_t);

struct hentry_s
{
  unsigned long e_value;
  unsigned long e_n;
};

#define HENTRY_HASH(e) ((e)->e_value)

HTABLE_BODIES(htable, ht, entry_t, HENTRY_HASH);

typedef struct context_s
{
  su_home_t c_home[1];
  htable_t c_hash[1];
} context_t;

context_t *context_create(void)
{
  context_t *c = su_home_clone(NULL, sizeof(*c));

  if (c)
    htable_resize(c->c_home, c->c_hash, 0);

  return c;
}

static
entry_t *search(context_t *c, unsigned long value, unsigned long n, int add)
{
  htable_t *ht = c->c_hash;
  entry_t *e, **ee;
  unsigned long hash = value;

  /* Search for entry in hash table */
  for (ee = htable_hash(ht, hash);
       (e = *ee);
       ee = htable_next(ht, ee)) {
    if (e->e_value == value &&
	e->e_n == n)
      return e;
  }

  if (add) {
    /* Resize hash table */
    if (htable_is_full(ht)) {
      htable_resize(c->c_home, ht, 0);
      fprintf(stderr, "htable: resized to %u with %u entries\n",
	      ht->ht_size, ht->ht_used);
    }

    /* Add an entry */
    e = su_zalloc(c->c_home, sizeof(*e)); assert(e);
    e->e_value = value, e->e_n = n;

    htable_append(ht, e);
  }

  return e;
}

static int add(context_t *c, unsigned long value, unsigned long n)
{
  return search(c, value, n, 1) != NULL;
}

static
void zap(context_t *c, entry_t *e)
{
  htable_remove(c->c_hash, e);
  su_free(c->c_home, e);
}

/*
 * Check that all n entries with hash h are in hash table
 * and they are stored in same order
 * in which they were added to the hash
 */
static unsigned long count(context_t *c, hash_value_t h)
{
  entry_t *e, **ee;
  unsigned long n, expect = 1;

  for (ee = htable_hash(c->c_hash, h), n = 0;
       (e = *ee);
       ee = htable_next(c->c_hash, ee)) {
    if (e->e_value != h)
      continue;
    if (e->e_n == expect)
      n++, expect++;
  }

  return n;
}

int table_test(int flags)
{
  context_t *c;
  entry_t *e;

  BEGIN();

  TEST_1(c = context_create());
  TEST_1(add(c, 0, 1)); TEST_1(c->c_hash->ht_table[0]);
  TEST_1(add(c, 1, 2)); TEST_1(c->c_hash->ht_table[1]);
  TEST_1(add(c, 2, 3)); TEST_1(c->c_hash->ht_table[2]);
  TEST_1(add(c, 0, 4)); TEST_1(c->c_hash->ht_table[3]);
  TEST_1(add(c, 2, 5)); TEST_1(c->c_hash->ht_table[4]);

  TEST_1(e = search(c, 1, 2, 0));
  TEST(htable_remove(c->c_hash, e), 0);
  TEST(htable_remove(c->c_hash, e), -1);
  su_free(c->c_home, e);
  TEST_1(c->c_hash->ht_table[0]);
  TEST_1(c->c_hash->ht_table[1]);
  TEST_1(c->c_hash->ht_table[2]);
  TEST_1(c->c_hash->ht_table[3]);
  TEST_1(c->c_hash->ht_table[4] == NULL);

  zap(c, c->c_hash->ht_table[0]);

  TEST_1(c->c_hash->ht_table[0]);
  TEST_1(c->c_hash->ht_table[1] == NULL);
  TEST_1(c->c_hash->ht_table[2]);
  TEST_1(c->c_hash->ht_table[3]);
  TEST_1(c->c_hash->ht_table[4] == NULL);

  TEST_1(add(c, 0, 6)); TEST_1(c->c_hash->ht_table[1]);
  TEST_1(add(c, 1, 7)); TEST_1(c->c_hash->ht_table[4]);

  /* Test that zapping entry 0 does not move 2 and 3 */
  zap(c, c->c_hash->ht_table[0]);
  TEST_1(c->c_hash->ht_table[4] == NULL);

  {
    /* Insert entries at the end of hash, then resize and check
       for correct ordering */
    hash_value_t size = c->c_hash->ht_size, h = size - 1;

    TEST_1(add(c, h, 1)); TEST_1(add(c, h, 2)); TEST_1(add(c, h, 3));
    TEST_1(add(c, h, 4)); TEST_1(add(c, h, 5)); TEST_1(add(c, h, 6));
    TEST_1(add(c, h, 7)); TEST_1(add(c, h, 8)); TEST_1(add(c, h, 9));

    TEST(count(c, h), 9);

    TEST(htable_resize(c->c_home, c->c_hash, ++size), 0);
    TEST(count(c, h), 9);
    TEST(htable_resize(c->c_home, c->c_hash, ++size), 0);
    TEST(count(c, h), 9);
    TEST(htable_resize(c->c_home, c->c_hash, ++size), 0);
    TEST(count(c, h), 9);
    TEST(htable_resize(c->c_home, c->c_hash, ++size), 0);
    TEST(count(c, h), 9);
    TEST(htable_resize(c->c_home, c->c_hash, ++size), 0);
    TEST(count(c, h), 9);
    TEST(htable_resize(c->c_home, c->c_hash, ++size), 0);
    TEST(count(c, h), 9);
    TEST(htable_resize(c->c_home, c->c_hash, ++size), 0);
    TEST(count(c, h), 9);
    TEST(htable_resize(c->c_home, c->c_hash, ++size), 0);
    TEST(count(c, h), 9);
    TEST(htable_resize(c->c_home, c->c_hash, ++size), 0);
    TEST(count(c, h), 9);
  }

  TEST_VOID(su_home_unref(c->c_home));

  END();
}

