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

#ifndef HTABLE_H
/** Defined when <sofia-sip/htable.h> has been included. */
#define HTABLE_H

/**@ingroup su_htable
 * @file sofia-sip/htable.h
 *
 * Hash tables templates.
 *
 * This file contain a hash table template for C.  The hash tables are
 * resizeable, and they usually contain pointers to entries.  The
 * declaration for template datatypes is instantiated with macro
 * HTABLE_DECLARE().  The prototypes for hashing functions are instantiated
 * with macro HTABLE_PROTOS().  The implementation is instantiated with
 * macro HTABLE_BODIES().
 *
 * The hash table template is most efficient when the hash value is
 * precalculated and stored in each entry.  The hash "function" given to the
 * HTABLE_BODIES() would then be something like macro
 * @code
 * #define HTABLE_ENTRY_HASH(e) ((e)->e_hash_value)
 * @endcode
 *
 * When a entry with new identical hash key is added to the table, it can be
 * either @e inserted (before any other entry with same key value) or
 * @e appended.
 *
 * Example code can be found from <htable_test.c>.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Sep 25 17:42:40 2001 ppessi
 */

typedef unsigned long hash_value_t;

/** Minimum size of hash table */
#define HTABLE_MIN_SIZE 31

/** Declare hash table structure type.
 *
 * The macro HTABLE_DECLARE() expands to a declaration for hash table
 * structure.  The its typedef will be <em>prefix</em><code>_t</code>, the
 * field names start withb @a pr.  The entry type is @a entry_t.
 *
 * @param prefix  hash table type and function prefix
 * @param pr      hash table field prefix
 * @param entry_t entry type
 */
#define HTABLE_DECLARE(prefix, pr, entry_t)		\
  HTABLE_DECLARE_WITH(prefix, pr, entry_t, unsigned, hash_value_t)

#define HTABLE_DECLARE_WITH(prefix, pr, entry_t, size_t, hash_value_t)	\
  typedef struct prefix##_s {						\
    size_t pr##_size;							\
    size_t pr##_used;							\
    entry_t**pr##_table; /**< Hash table itself */			\
  } prefix##_t

#ifndef HTABLE_SCOPE
/** Default scope for hash table functions. */
#define HTABLE_SCOPE su_inline
#endif

/** Prototypes for hash table
 *
 * The macro HTABLE_PROTOS() expands to the prototypes of hash table
 * functions.  The function and type names start with @a prefix, the field
 * names start with @a pr.  The entry type is @a entry_t.
 *
 * @param prefix  hash table type and function prefix
 * @param pr      hash table field prefix
 * @param entry_t entry type
 */
#define HTABLE_PROTOS(prefix, pr, entry_t) \
  HTABLE_PROTOS_WITH(prefix, pr, entry_t, unsigned, hash_value_t)

#define HTABLE_PROTOS_WITH(prefix, pr, entry_t, size_t, hash_value_t)	\
HTABLE_SCOPE int prefix##_resize(su_home_t *, prefix##_t pr[1], size_t); \
HTABLE_SCOPE int prefix##_is_full(prefix##_t const *); \
HTABLE_SCOPE entry_t **prefix##_hash(prefix##_t const *, hash_value_t hv); \
HTABLE_SCOPE entry_t **prefix##_next(prefix##_t const *, entry_t * const *ee); \
HTABLE_SCOPE void prefix##_append(prefix##_t *pr, entry_t const *e); \
HTABLE_SCOPE void prefix##_insert(prefix##_t *pr, entry_t const *e); \
HTABLE_SCOPE int prefix##_remove(prefix##_t *, entry_t const *e)

/** Hash table implementation.
 *
 * The macro HTABLE_BODIES() expands the hash table functions.  The function
 * and type names start with @a prefix, the field names start with @a pr.
 * The entry type is @a entry_t.  The function (or macro) name returning
 * hash value of each entry is given as @a hfun.
 *
 * @param prefix  hash table type and function prefix
 * @param pr      hash table field prefix
 * @param entry_t entry type
 * @param hfun    function or macro returning hash value of entry
 */
#define HTABLE_BODIES(prefix, pr, entry_t, hfun) \
  HTABLE_BODIES_WITH(prefix, pr, entry_t, hfun, unsigned, hash_value_t)

#define HTABLE_BODIES_WITH(prefix, pr, entry_t, hfun, size_t, hash_value_t) \
/** Reallocate new hash table */ \
HTABLE_SCOPE \
int prefix##_resize(su_home_t *home, \
                    prefix##_t pr[], \
		    size_t new_size) \
{ \
  entry_t **new_hash; \
  entry_t **old_hash = pr->pr##_table; \
  size_t old_size; \
  size_t i, j, i0; \
  unsigned again = 0; \
  size_t used = 0, collisions = 0; \
\
  if (new_size == 0) \
    new_size = 2 * pr->pr##_size + 1; \
  if (new_size < HTABLE_MIN_SIZE) \
    new_size = HTABLE_MIN_SIZE; \
  if (new_size < 5 * pr->pr##_used / 4) \
    new_size = 5 * pr->pr##_used / 4; \
\
  if (!(new_hash = su_zalloc(home, sizeof(*new_hash) * new_size))) \
    return -1; \
\
  old_size = pr->pr##_size; \
\
  do for (j = 0; j < old_size; j++) { \
    if (!old_hash[j]) \
      continue; \
\
    if (again < 2 && hfun(old_hash[j]) % old_size > j) { \
      /* Wrapped, leave entry for second pass */ \
      again = 1; continue; \
    } \
\
    i0 = hfun(old_hash[j]) % new_size; \
\
    for (i = i0; new_hash[i]; i = (i + 1) % new_size, assert(i != i0)) \
      collisions++; \
\
    new_hash[i] = old_hash[j], old_hash[j] = NULL; \
    used++; \
  } \
  while (again++ == 1); \
\
  pr->pr##_table = new_hash, pr->pr##_size = new_size; \
\
  assert(pr->pr##_used == used); \
\
  su_free(home, old_hash); \
\
  return 0; \
} \
\
HTABLE_SCOPE \
int prefix##_is_full(prefix##_t const *pr) \
{ \
  return pr->pr##_table == NULL || 3 * pr->pr##_used > 2 * pr->pr##_size; \
} \
\
HTABLE_SCOPE \
entry_t **prefix##_hash(prefix##_t const *pr, hash_value_t hv) \
{ \
  return pr->pr##_table + hv % pr->pr##_size; \
} \
\
HTABLE_SCOPE \
entry_t **prefix##_next(prefix##_t const *pr, entry_t * const *ee) \
{ \
  if (++ee < pr->pr##_table + pr->pr##_size && ee >= pr->pr##_table) \
    return (entry_t **)ee; \
  else \
    return pr->pr##_table; \
}  \
\
HTABLE_SCOPE \
void prefix##_append(prefix##_t *pr, entry_t const *e) \
{ \
  entry_t **ee; \
\
  pr->pr##_used++; \
  for (ee = prefix##_hash(pr, hfun(e)); *ee; ee = prefix##_next(pr, ee)) \
   ; \
  *ee = (entry_t *)e; \
} \
\
HTABLE_SCOPE \
void prefix##_insert(prefix##_t *pr, entry_t const *e) \
{ \
  entry_t *e0, **ee; \
\
  pr->pr##_used++; \
  /* Insert entry into hash table (before other entries with same hash) */ \
  for (ee = prefix##_hash(pr, hfun(e));  \
       (e0 = *ee); \
       ee = prefix##_next(pr, ee)) \
    *ee = (entry_t *)e, e = e0; \
  *ee = (entry_t *)e; \
} \
\
HTABLE_SCOPE \
int prefix##_remove(prefix##_t *pr, entry_t const *e) \
{ \
  size_t i, j, k; \
  size_t size = pr->pr##_size; \
  entry_t **htable = pr->pr##_table; \
\
  if (!e) return -1; \
\
  /* Search for entry */ \
  for (i = hfun(e) % size; htable[i]; i = (i + 1) % size) \
    if (e == htable[i]) \
      break; \
\
  /* Entry is not in table? */ \
  if (!htable[i]) return -1; \
\
  /* Move table entries towards their primary place  */ \
  for (j = (i + 1) % size; htable[j]; j = (j + 1) % size) { \
    /* k is primary place for entry */ \
    k = hfun(htable[j]) % size; \
    if (k == j)			/* entry is in its primary place? */ \
      continue; \
    /* primary place is between i and j - do not move this to i */ \
    if (j > i ? (i < k && k < j) : (i < k || k < j)) \
      continue; \
\
    htable[i] = htable[j], i = j; \
  } \
\
  pr->pr##_used--; \
\
  htable[i] = NULL; \
\
  return 0; \
} \
extern int prefix##_dummy

#endif /** !defined(HTABLE_H) */
