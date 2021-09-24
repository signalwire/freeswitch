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

/**@CFILE sres_cache.c
 * @brief Cache for Sofia DNS Resolver.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Teemu Jalava <Teemu.Jalava@nokia.com>
 * @author Mikko Haataja
 *
 * @todo The resolver should allow handling arbitrary records, too.
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

#if HAVE_NETINET_IN_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#if HAVE_WINSOCK2_H
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <time.h>

#include "sofia-resolv/sres_cache.h"
#include "sofia-resolv/sres_record.h"

#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_strlst.h>
#include <sofia-sip/su_string.h>
#include <sofia-sip/htable.h>
#include <sofia-sip/heap.h>

#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <limits.h>

#include <assert.h>

#define SU_LOG sresolv_log

#include <sofia-sip/su_debug.h>

typedef struct sres_rr_hash_entry_s sres_rr_hash_entry_t;

HTABLE_DECLARE_WITH(sres_htable, ht, sres_rr_hash_entry_t, unsigned, size_t);

typedef HEAP_TYPE sres_heap_t;

HEAP_DECLARE(static inline, sres_heap_t, sres_heap_, sres_rr_hash_entry_t *);

struct sres_rr_hash_entry_s {
  sres_record_t *rr;
  size_t         rr_heap_index;
  time_t         rr_expires;
  unsigned int   rr_hash_key;
};

#define SRES_HENTRY_HASH(e) ((e)->rr_hash_key)

/* ---------------------------------------------------------------------- */
/* Heap */

struct sres_cache
{
  su_home_t           cache_home[1];
  time_t              cache_cleaned;
  sres_htable_t       cache_hash[1];
  sres_heap_t         cache_heap;
};

#define sr_refcount sr_record->r_refcount
#define sr_name     sr_record->r_name
#define sr_status   sr_record->r_status
#define sr_size     sr_record->r_size
#define sr_type     sr_record->r_type
#define sr_class    sr_record->r_class
#define sr_ttl      sr_record->r_ttl
#define sr_rdlen    sr_record->r_rdlen

/* ---------------------------------------------------------------------- */
/* Internal prototypes */

#define LOCK(cache) (su_home_mutex_lock((cache)->cache_home) == 0)
#define UNLOCK(cache) (su_home_mutex_unlock((cache)->cache_home))

su_inline
void _sres_cache_free_one(sres_cache_t *cache, sres_record_t *answer);
su_inline
void _sres_cache_free_answers(sres_cache_t *cache, sres_record_t **answers);
su_inline sres_record_t **_sres_cache_copy_answers(
  sres_cache_t *, sres_record_t **);

static unsigned sres_hash_key(const char *string);

HTABLE_PROTOS_WITH(sres_htable, ht, sres_rr_hash_entry_t, unsigned, size_t);

/* ---------------------------------------------------------------------- */
/* Public functions */

/** Create a resolver cache object.
 *
 * @param n initial size of cache
 */
sres_cache_t *sres_cache_new(int n)
{
  sres_cache_t *cache = su_home_new(sizeof *cache);

  if (cache) {
    su_home_threadsafe(cache->cache_home);
    if (sres_htable_resize(cache->cache_home, cache->cache_hash, n) < 0 ||
	sres_heap_resize(cache->cache_home, &cache->cache_heap, 0) < 0)
      su_home_unref(cache->cache_home), cache = NULL;
  }

  return cache;
}

/** Increase reference count on a resolver cache object. */
sres_cache_t *sres_cache_ref(sres_cache_t *cache)
{
  return su_home_ref(cache->cache_home);
}

/** Decrease the reference count on a resolver cache object. */
void sres_cache_unref(sres_cache_t *cache)
{
  su_home_unref(cache->cache_home);
}

struct frame {
  struct frame *previous;
  char const *domain;
};

/** Count or get matching records from cache */
static int
sres_cache_get0(sres_htable_t *htable,
		sres_rr_hash_entry_t **iter,
		uint16_t type,
		char const *domain,
		time_t now,
		sres_record_t **cached,
		int len,
		struct frame *previous)
{
  sres_cname_record_t *cname = NULL;
  int dcount = 0, derrorcount = 0, ccount = 0;

  for (; iter && *iter; iter = sres_htable_next(htable, iter)) {
    sres_record_t *rr = (*iter)->rr;

    if (rr == NULL)
      continue;
    if (now > (*iter)->rr_expires)
      continue;
    if (rr->sr_name == NULL)
      continue;
    if (!su_casematch(rr->sr_name, domain))
      continue;

    if (rr->sr_type == type || type == sres_qtype_any) {
      if (rr->sr_status == SRES_RECORD_ERR && type == sres_qtype_any)
	continue;
      if (cached) {
	if (dcount >= len)
	  return -1;
	cached[dcount] = rr, rr->sr_refcount++;
      }
      dcount++;
      if (rr->sr_status)
	derrorcount++;
    }

    if (type != sres_type_cname && rr->sr_type == sres_type_cname) {
      if (rr->sr_status == 0)
	cname = rr->sr_cname;
    }
  }

  if (cname && dcount == derrorcount) {
    /* Nothing found, trace CNAMEs */
    unsigned hash;
    struct frame *f, frame;
    frame.previous = previous;
    frame.domain = domain;

    hash = sres_hash_key(domain = cname->cn_cname);

    /* Check for cname loops */
    for (f = previous; f; f = f->previous) {
      if (su_casematch(domain, f->domain))
	break;
    }

    if (f == NULL) {
      ccount = sres_cache_get0(htable, sres_htable_hash(htable, hash),
			       type, domain, now,
			       cached ? cached + dcount : NULL,
			       cached ? len - dcount : 0,
			       &frame);
    }
    if (ccount < 0)
      return ccount;
  }

  return dcount + ccount;
}

/** Get a list of matching records from cache. */
int sres_cache_get(sres_cache_t *cache,
		   uint16_t type,
		   char const *domain,
		   sres_record_t ***return_cached)
{
  sres_record_t **result = NULL;
  sres_rr_hash_entry_t **slot;
  int result_size, i, j;
  unsigned hash;
  time_t now;
  char b[8];

  if (!domain || !return_cached)
    return -1;

  *return_cached = NULL;

  SU_DEBUG_9(("%s(%p, %s, \"%s\") called\n", "sres_cache_get",
	      (void *)cache, sres_record_type(type, b), domain));

  hash = sres_hash_key(domain);

  if (!LOCK(cache))
    return -1;

  time(&now);

  /* First pass: just count the number of rr:s for array allocation */
  slot = sres_htable_hash(cache->cache_hash, hash);

  i = sres_cache_get0(cache->cache_hash, slot, type, domain, now,
		      NULL, 0, NULL);
  if (i <= 0) {
    UNLOCK(cache);
    return 0;
  }

  result_size = (sizeof *result) * (i + 1);
  result = su_zalloc(cache->cache_home, result_size);
  if (result == NULL) {
    UNLOCK(cache);
    return -1;
  }

  /* Second pass: add the rr pointers to the allocated array */
  j = sres_cache_get0(cache->cache_hash, slot, type, domain, now,
		      result, i, NULL);
  if (i != j) {
    /* Uh-oh. */
    SU_DEBUG_9(("%s(%p, %s, \"%s\") got %d != %d\n", "sres_cache_get",
		(void *)cache, sres_record_type(type, b), domain, i, j));
    for (i = 0; i < result_size; i++) {
      if (result[i])
	result[i]->sr_refcount--;
    }
    su_free(cache->cache_home, result);
    return 0;
  }

  result[i] = NULL;

  UNLOCK(cache);

  SU_DEBUG_9(("%s(%p, %s, \"%s\") returned %d entries\n", "sres_cache_get",
	      (void *)cache, sres_record_type(type, b), domain, i));

  *return_cached = result;

  return i;
}

sres_record_t *
sres_cache_alloc_record(sres_cache_t *cache,
			sres_record_t const *template,
			size_t extra)
{
  sres_record_t *sr;
  size_t size, name_length;

  size = template->sr_size;

  assert(size >= sizeof(sres_common_t));
  assert(template->sr_name != NULL);

  name_length = strlen(template->sr_name);

  sr = su_alloc(cache->cache_home, size + extra + name_length + 1);

  if (sr) {
    char *s = (char *)sr + size + extra;
    sr->sr_refcount = 0;
    sr->sr_name = memcpy(s, template->sr_name, name_length);
    sr->sr_name[name_length] = '\0';
    memcpy(&sr->sr_status, &template->sr_status,
	   size - offsetof(sres_common_t, r_status));
  }

  return sr;
}

/** Free a record that has not been stored. */
void sres_cache_free_record(sres_cache_t *cache, void *_sr)
{
  sres_record_t *sr = _sr;

  if (sr) {
    assert(sr->sr_refcount == 0);
    su_free(cache->cache_home, sr);
  }
}

/** Store record to cache */
void
sres_cache_store(sres_cache_t *cache, sres_record_t *rr, time_t now)
{
  sres_rr_hash_entry_t **rr_iter, *rr_hash_entry;
  unsigned hash;

  if (rr == NULL)
    return;

  hash = sres_hash_key(rr->sr_name);

  if (!LOCK(cache))
    return;

  if (sres_htable_is_full(cache->cache_hash))
    sres_htable_resize(cache->cache_home, cache->cache_hash, 0);

  if (sres_heap_is_full(cache->cache_heap))
    if (sres_heap_resize(cache->cache_home, &cache->cache_heap, 0) < 0) {
      UNLOCK(cache);
      return;
    }

  for (rr_iter = sres_htable_hash(cache->cache_hash, hash);
       (rr_hash_entry = *rr_iter);
       rr_iter = sres_htable_next(cache->cache_hash, rr_iter)) {
    sres_record_t *or = rr_hash_entry->rr;

    if (or == NULL)
      continue;
    if (rr_hash_entry->rr_hash_key != hash)
      continue;
    if (or->sr_type != rr->sr_type)
      continue;
    if (!!or->sr_name != !!rr->sr_name)
      continue;
    if (or->sr_name != rr->sr_name &&
	!su_casematch(or->sr_name, rr->sr_name))
      continue;
    if (rr->sr_type != sres_type_soa /* There can be only one */
	&& sres_record_compare(or, rr))
      continue;

    /* There was an old entry in the cache.. Zap it, replace this with it */
    sres_heap_remove(cache->cache_heap, rr_hash_entry->rr_heap_index);
    rr_hash_entry->rr_expires = now + rr->sr_ttl;
    rr_hash_entry->rr = rr;
    rr->sr_refcount++;
    sres_heap_add(cache->cache_heap, rr_hash_entry);

    _sres_cache_free_one(cache, or);

    UNLOCK(cache);

    return;
  }

  rr_hash_entry = su_zalloc(cache->cache_home, sizeof(*rr_hash_entry));

  if (rr_hash_entry) {
    rr_hash_entry->rr_hash_key = hash;
    rr_hash_entry->rr_expires = now + rr->sr_ttl;
    rr_hash_entry->rr = rr;
    rr->sr_refcount++;

    sres_heap_add(cache->cache_heap, rr_hash_entry);

    cache->cache_hash->ht_used++;

    *rr_iter = rr_hash_entry;
  }

  UNLOCK(cache);
}

/** Free the list records. */
void sres_cache_free_answers(sres_cache_t *cache, sres_record_t **answers)
{
  if (answers && LOCK(cache)) {
      _sres_cache_free_answers(cache, answers);
    UNLOCK(cache);
  }
}

/** Free and zero one record. */
void sres_cache_free_one(sres_cache_t *cache, sres_record_t *answer)
{
  if (LOCK(cache)) {
    _sres_cache_free_one(cache, answer);
    UNLOCK(cache);
  }
}

/** Copy the list of records. */
sres_record_t **
sres_cache_copy_answers(sres_cache_t *cache, sres_record_t **answers)
{
  sres_record_t **copy = NULL;

  if (answers && LOCK(cache)) {
    copy = _sres_cache_copy_answers(cache, answers);
    UNLOCK(cache);
  }

  return copy;
}

/* ---------------------------------------------------------------------- */
/* Private functions */

su_inline
void _sres_cache_free_answers(sres_cache_t *cache, sres_record_t **answers)
{
  int i;

  for (i = 0; answers[i] != NULL; i++) {
    if (answers[i]->sr_refcount <= 1)
      su_free(cache->cache_home, answers[i]);
    else
      answers[i]->sr_refcount--;
    answers[i] = NULL;
  }

  su_free(cache->cache_home, answers);
}

su_inline
void _sres_cache_free_one(sres_cache_t *cache, sres_record_t *answer)
{
  if (answer) {
    if (answer->sr_refcount <= 1)
      su_free(cache->cache_home, answer);
    else
      answer->sr_refcount--;
  }
}

su_inline sres_record_t **
_sres_cache_copy_answers(sres_cache_t *cache, sres_record_t **answers)
{
  int i, n;
  sres_record_t **copy;

  for (n = 0; answers[n] != NULL; n++)
    ;

  copy = su_alloc(cache->cache_home, (n + 1) * (sizeof *copy));
  if (copy == NULL)
    return NULL;

  for (i = 0; i < n; i++) {
    copy[i] = answers[i];
    copy[i]->sr_refcount++;
  }

  copy[i] = NULL;

  return copy;
}

/** Calculate a hash key for a string */
static
unsigned
sres_hash_key(const char *string)
{
  unsigned int result = 0;

  while (string && *string)
    result = result * 797 + (unsigned char) * (string++);

  if (result == 0)
    result--;

  return result;
}

/** Remove old records from cache.
 *
 * Remove entries older than @a now from the cache.
 *
 * @param cache    pointer to DNS cache object
 * @param now      remove older than this time
 */
void sres_cache_clean(sres_cache_t *cache, time_t now)
{
  size_t i;

  if (now < cache->cache_cleaned + SRES_CACHE_TIMER_INTERVAL)
    return;

  /* Clean cache from old entries */

  for (;;) {
    if (!LOCK(cache))
      return;

    cache->cache_cleaned = now;

    for (i = 0; i < 100; i++) {
      sres_rr_hash_entry_t *e = sres_heap_get(cache->cache_heap, 1);

      if (e == NULL || e->rr_expires >= now) {
	UNLOCK(cache);
	return;
      }

      sres_heap_remove(cache->cache_heap, 1);
      sres_htable_remove(cache->cache_hash, e);
      _sres_cache_free_one(cache, e->rr);
      su_free(cache->cache_home, e);
    }

    UNLOCK(cache);
  }
}

/** Set the priority of the matching cached SRV record.
 *
 * The SRV records with the domain name, target and port are matched and
 * their priority value is adjusted. This function is used to implement
 * greylisting of SIP servers.
 *
 * @param cache    pointer to DNS cache object
 * @param domain   domain name of the SRV record(s) to modify
 *                 (including final dot)
 * @param target   SRV target of the SRV record(s) to modify
 * @param port     port number of SRV record(s) to modify
 *                 (in host byte order)
 * @param ttl      new ttl
 * @param priority new priority value (0=highest, 65535=lowest)
 *
 * @sa sres_set_cached_srv_priority()
 *
 * @NEW_1_12_8
 */
int sres_cache_set_srv_priority(sres_cache_t *cache,
				char const *domain,
				char const *target,
				uint16_t port,
				uint32_t ttl,
				uint16_t priority)
{
  int ret = 0;
  unsigned hash;
  sres_rr_hash_entry_t **iter;
  time_t expires;

  if (cache == NULL || domain == NULL || target == NULL)
    return -1;

  hash = sres_hash_key(domain);

  if (!LOCK(cache))
    return -1;

  time(&expires);
  expires += ttl;

  for (iter = sres_htable_hash(cache->cache_hash, hash);
       iter && *iter;
       iter = sres_htable_next(cache->cache_hash, iter)) {
    sres_record_t *rr = (*iter)->rr;

    if (rr && rr->sr_name &&
	sres_type_srv == rr->sr_type &&
	su_casematch(rr->sr_name, domain)) {

      (*iter)->rr_expires = expires;

      if ((port == 0 || rr->sr_srv->srv_port == port) &&
	  rr->sr_srv->srv_target &&
	  su_casematch(rr->sr_srv->srv_target, target)) {
	/* record found --> change priority of server */
	rr->sr_srv->srv_priority = priority;
	ret++;
      }
    }
  }

  UNLOCK(cache);

  /** @return number of modified entries or -1 upon an error. */
  return ret;
}

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif

HTABLE_BODIES_WITH(sres_htable, ht, sres_rr_hash_entry_t, SRES_HENTRY_HASH,
		   unsigned, size_t);

#ifdef __clang__
#pragma clang diagnostic pop
#endif

static inline
int sres_heap_earlier_entry(sres_rr_hash_entry_t const *a,
			    sres_rr_hash_entry_t const *b)
{
  return a->rr_expires < b->rr_expires;
}

static inline
void sres_heap_set_entry(sres_rr_hash_entry_t **heap,
			 size_t index,
			 sres_rr_hash_entry_t *entry)
{
  entry->rr_heap_index = index;
  heap[index] = entry;
}

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif

HEAP_BODIES(static inline,
	    sres_heap_t,
	    sres_heap_,
	    sres_rr_hash_entry_t *,
	    sres_heap_earlier_entry,
	    sres_heap_set_entry,
	    su_realloc,
	    NULL);

#ifdef __clang__
#pragma clang diagnostic pop
#endif
