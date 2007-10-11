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

#ifndef HTABLE2_H
/** Defined when <sofia-sip/htable2.h> has been included. */
#define HTABLE2_H 

/**@file sofia-sip/htable2.h
 *
 * Hash tables templates, take 2.
 *
 * Note: this version can handle structures as entries, and it can be used
 * without <sofia-sip/su_alloc.h>.
 * 
 * This file contain a hash table template for C.  The hash tables are
 * resizeable, and they usually contain pointers to entries.  The
 * declaration for template datatypes is instantiated with macro
 * HTABLE2_DECLARE().  The prototypes for hashing functions are instantiated
 * with macro HTABLE2_PROTOS().  The implementation is instantiated with
 * macro HTABLE2_BODIES().
 *
 * The hash table template is most efficient when the hash value is
 * precalculated and stored in each entry.  The hash "function" given to the
 * HTABLE2_BODIES() would then be something like macro
 * @code
 * #define HTABLE2_ENTRY_HASH(e) ((e).e_hash_value)
 * @endcode
 *
 * When a entry with new identical hash key is added to the table, it can be
 * either @e inserted (before any other entry with same key value) or
 * @e appended.
 * 
 * Example code can be found from <htable_test.c>.
 * 
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Tue Sep 25 17:42:40 2001 ppessi
 *
 */

typedef unsigned long hash_value_t;

/** Minimum size of hash table */
#define HTABLE2_MIN_SIZE 31

/** Declare hash table structure type.
 * 
 * The macro HTABLE2_DECLARE() expands to a declaration for hash table
 * structure.  The its typedef will be <em>prefix</em><code>_t</code>, the
 * field names start with @a pr.  The entry type is @a entrytype.
 *
 * @param sname     name of struct
 * @param prefix    hash table type and function prefix
 * @param pr        hash table field prefix
 * @param entrytype entry type
 */
#define HTABLE2_DECLARE(sname, prefix, pr, entrytype)	\
struct sname { \
  unsigned pr##size; \
  unsigned pr##used; \
  entrytype *pr##table; /**< Hash table itself */ \
}

#ifndef HTABLE2_SCOPE
/** Default scope for hash table functions. */
#define HTABLE2_SCOPE su_inline
#endif

/** Prototypes for hash table
 *
 * The macro HTABLE2_PROTOS() expands to the prototypes of hash table
 * functions.  The function and type names start with @a prefix, the field
 * names start with @a pr.  The entry type is @a entrytype.

 * @param type      hash table typedef
 * @param prefix    function prefix
 * @param pr        hash table field prefix
 * @param entrytype entry type
 */
#define HTABLE2_PROTOS(type, prefix, pr, entrytype)			\
HTABLE2_SCOPE int prefix##_resize(void *a, type pr[1], unsigned); \
HTABLE2_SCOPE int prefix##_is_full(type const *); \
HTABLE2_SCOPE entrytype *prefix##_hash(type const *, hash_value_t hv); \
HTABLE2_SCOPE entrytype *prefix##_next(type const *, entrytype *ee); \
HTABLE2_SCOPE void prefix##_append(type *pr, entrytype e); \
HTABLE2_SCOPE void prefix##_insert(type *pr, entrytype e); \
HTABLE2_SCOPE int prefix##_remove(type *, entrytype const e)

/** Hash table implementation.
 *
 * The macro HTABLE2_BODIES() expands the hash table functions.  The function
 * and type names start with @a prefix, the field names start with @a pr.
 * The entry type is @a entrytype.  The function (or macro) name returning
 * hash value of each entry is given as @a hfun.
 *
 * @param type      hash table type
 * @param prefix    function prefix for hash table 
 * @param pr        field prefix for hash table 
 * @param entrytype type of entry element
 * @param hfun      function or macro returning hash value of entry
 * @param is_used   function or macro returning true if entry is occupied
 * @param reclaim   function or macro zeroing entry
 * @param is_equal  equality test
 * @param halloc    function allocating or freeing memory
 */
#define HTABLE2_BODIES(type, prefix, pr, entrytype,			\
		       hfun, is_used, reclaim, is_equal, halloc)	\
/** Reallocate new hash table */ \
HTABLE2_SCOPE \
int prefix##_resize(void *realloc_arg, \
                    type pr[1], \
		    unsigned new_size) \
{ \
  entrytype *new_hash; \
  entrytype *old_hash = pr->pr##table; \
  unsigned old_size; \
  unsigned i, j, i0; \
  unsigned again = 0, used = 0, collisions = 0; \
\
  (void)realloc_arg; \
\
  if (new_size == 0) \
    new_size = 2 * pr->pr##size + 1; \
  if (new_size < HTABLE2_MIN_SIZE) \
    new_size = HTABLE2_MIN_SIZE; \
\
  if (!(new_hash = halloc(realloc_arg, NULL, sizeof(*new_hash) * new_size))) \
    return -1; \
\
  memset(new_hash, 0, sizeof(*new_hash) * new_size); \
  old_size = pr->pr##size; \
\
  do for (j = 0; j < old_size; j++) { \
    if (!is_used(old_hash[j])) \
      continue; \
\
    if (again < 2 && hfun(old_hash[j]) % old_size > j) { \
      /* Wrapped, leave entry for second pass */ \
      again = 1; continue; \
    } \
\
    i0 = hfun(old_hash[j]) % new_size; \
\
    for (i = i0; is_used(new_hash[i]); \
         i = (i + 1) % new_size, assert(i != i0)) \
      collisions++; \
\
    new_hash[i] = old_hash[j]; reclaim(&old_hash[j]); \
    used++; \
  } \
  while (again++ == 1); \
\
  pr->pr##table = new_hash, pr->pr##size = new_size; \
\
  if (old_hash) old_hash = halloc(realloc_arg, old_hash, 0);	\
\
  assert(pr->pr##used == used);\
\
  return 0; \
} \
\
HTABLE2_SCOPE \
int prefix##_is_full(type const *pr) \
{ \
  return pr->pr##table == NULL || 3 * pr->pr##used > 2 * pr->pr##size; \
} \
\
HTABLE2_SCOPE \
entrytype *prefix##_hash(type const *pr, hash_value_t hv) \
{ \
  return pr->pr##table + hv % pr->pr##size; \
} \
\
HTABLE2_SCOPE \
entrytype *prefix##_next(type const *pr, entrytype *ee) \
{ \
  if (++ee < pr->pr##table + pr->pr##size && ee >= pr->pr##table) \
    return ee; \
  else \
    return pr->pr##table; \
}  \
\
HTABLE2_SCOPE \
void prefix##_append(type *pr, entrytype e) \
{ \
  entrytype *ee; \
\
  pr->pr##used++; \
  for (ee = prefix##_hash(pr, hfun(e)); \
       is_used(*ee); \
       ee = prefix##_next(pr, ee)) \
   ; \
  *ee = e; \
} \
\
HTABLE2_SCOPE \
void prefix##_insert(type *pr, entrytype e) \
{ \
  entrytype e0; \
  entrytype *ee; \
\
  pr->pr##used++; \
  /* Insert entry into hash table (before other entries with same hash) */ \
  for (ee = prefix##_hash(pr, hfun(e));  \
       is_used((*ee)); \
       ee = prefix##_next(pr, ee)) \
    *ee = e, e = e0; \
  *ee = e; \
} \
\
HTABLE2_SCOPE \
int prefix##_remove(type *pr, entrytype const e) \
{ \
  unsigned i, j, k, size = pr->pr##size; \
  entrytype *htable = pr->pr##table; \
\
  /* Search for entry */ \
  for (i = hfun(e) % size; is_used(htable[i]); i = (i + 1) % size) \
    if (is_equal(e, htable[i])) \
      break; \
\
  if (!is_used(htable[i])) return -1; \
\
  /* Move table entries towards their primary place  */ \
  for (j = (i + 1) % size; is_used(htable[j]); j = (j + 1) % size) { \
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
  pr->pr##used--; \
\
  reclaim(&htable[i]); \
\
  return 0; \
} \
extern int const prefix##_dummy

#endif /** !defined(HTABLE2_H) */
