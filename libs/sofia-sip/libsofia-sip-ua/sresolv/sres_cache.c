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
#include <sofia-sip/htable.h>

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

struct sres_rr_hash_entry_s {
  unsigned int   rr_hash_key;
  time_t         rr_received;
  sres_record_t *rr;
};

#define SRES_HENTRY_HASH(e) ((e)->rr_hash_key)

struct sres_cache
{
  su_home_t           cache_home[1];
  time_t              cache_cleaned;
  sres_htable_t       cache_hash[1];
};

#define sr_refcount sr_record->r_refcount
#define sr_name     sr_record->r_name
#define sr_status   sr_record->r_status
#define sr_size     sr_record->r_size
#define sr_type     sr_record->r_type
#define sr_class    sr_record->r_class
#define sr_ttl      sr_record->r_ttl
#define sr_rdlen    sr_record->r_rdlen
#define sr_rdata    sr_generic->g_data

/* ---------------------------------------------------------------------- */
/* Internal prototypes */

#define LOCK(cache) (su_home_mutex_lock((cache)->cache_home) == 0)
#define UNLOCK(cache) (su_home_mutex_unlock((cache)->cache_home))

static inline
void _sres_cache_free_one(sres_cache_t *cache, sres_record_t *answer);
static inline
void _sres_cache_free_answers(sres_cache_t *cache, sres_record_t **answers);

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
    if (sres_htable_resize(cache->cache_home, cache->cache_hash, n) < 0)
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

/** Get a list of matching records from cache. */
int sres_cache_get(sres_cache_t *cache,
		   uint16_t type,
		   char const *domain,
		   sres_record_t ***return_cached)
{
  sres_record_t **result = NULL, *rr = NULL;
  sres_rr_hash_entry_t **rr_iter, **rr_iter2;
  int result_size, rr_count = 0;
  unsigned hash;
  time_t now;
  char b[8];

  if (!domain || !return_cached)
    return -1;

  *return_cached = NULL;

  SU_DEBUG_9(("%s(%p, %s, \"%s\") called\n", "sres_cache_get",
	      cache, sres_record_type(type, b), domain));

  hash = sres_hash_key(domain);

  if (!LOCK(cache))
    return -1;

  time(&now);

  /* First pass: just count the number of rr:s for array allocation */
  rr_iter2 = sres_htable_hash(cache->cache_hash, hash);

  /* Find the domain records from the hash table */
  for (rr_iter = rr_iter2; 
       rr_iter && *rr_iter; 
       rr_iter = sres_htable_next(cache->cache_hash, rr_iter)) {
    rr = (*rr_iter)->rr;

    if (rr != NULL &&
	(uint32_t)(now - (*rr_iter)->rr_received) <= rr->sr_ttl &&
        (type == sres_qtype_any || rr->sr_type == type) &&
        rr->sr_name != NULL &&
        strcasecmp(rr->sr_name, domain) == 0) 
      rr_count++;
  }

  if (rr_count == 0) {
    UNLOCK(cache);
    return 0;
  }

  result_size = (sizeof *result) * (rr_count + 1);
  result = su_zalloc(cache->cache_home, result_size);
  if (result == NULL) {
    UNLOCK(cache);
    return -1;
  }

  /* Second pass: add the rr pointers to the allocated array */

  for (rr_iter = rr_iter2, rr_count = 0; 
       rr_iter && *rr_iter; 
       rr_iter = sres_htable_next(cache->cache_hash, rr_iter)) {
    rr = (*rr_iter)->rr;

    if (rr != NULL &&
	(uint32_t)(now - (*rr_iter)->rr_received) <= rr->sr_ttl &&
        (type == sres_qtype_any || rr->sr_type == type) &&
        rr->sr_name != NULL &&
        strcasecmp(rr->sr_name, domain) == 0) {
      SU_DEBUG_9(("rr found in cache: %s %02d\n", 
		  rr->sr_name, rr->sr_type));

      result[rr_count++] = rr;
      rr->sr_refcount++;
    }
  }

  result[rr_count] = NULL;

  UNLOCK(cache);

  SU_DEBUG_9(("%s(%p, %s, \"%s\") returned %d entries\n", "sres_cache_get", 
	      cache, sres_record_type(type, b), domain, rr_count));

  *return_cached = result;

  return rr_count;
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
void sres_cache_free_record(sres_cache_t *cache, void *rr)
{
  sres_record_t *sr = rr;

  if (sr) {
    assert(sr->sr_refcount == 0);
    su_free(cache->cache_home, rr);
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
	strcasecmp(or->sr_name, rr->sr_name) != 0)
      continue;
    if (rr->sr_type != sres_type_soa /* There can be only one */
	&& sres_record_compare(or, rr))
      continue;
    
    /* There was an old entry in the cache.. Zap it, replace this with it */
    rr_hash_entry->rr_received = now;
    rr_hash_entry->rr = rr;
    rr->sr_refcount++;
    
    _sres_cache_free_one(cache, or);

    UNLOCK(cache);

    return;
  }
  
  rr_hash_entry = su_zalloc(cache->cache_home, sizeof(*rr_hash_entry));
  if (rr_hash_entry) {
    rr_hash_entry->rr_hash_key = hash;
    rr_hash_entry->rr_received = now;
    rr_hash_entry->rr = rr;
    rr->sr_refcount++;

    cache->cache_hash->ht_used++;
  }
  
  *rr_iter = rr_hash_entry;

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

/* ---------------------------------------------------------------------- */
/* Private functions */

static inline
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

static inline
void _sres_cache_free_one(sres_cache_t *cache, sres_record_t *answer)
{
  if (answer) {
    if (answer->sr_refcount <= 1)
      su_free(cache->cache_home, answer);
    else 
      answer->sr_refcount--;
  }
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

void sres_cache_clean(sres_cache_t *cache, time_t now)
{
  size_t i;

  if (now < cache->cache_cleaned + SRES_CACHE_TIMER_INTERVAL)
    return;

  if (!LOCK(cache))
    return;

  /* Clean cache from old entries */
  cache->cache_cleaned = now;

  for (i = 0; i < cache->cache_hash->ht_size; i++) {
    sres_rr_hash_entry_t *e;
      
    while ((e = cache->cache_hash->ht_table[i]) != NULL) {
      if ((uint32_t)(now - e->rr_received) <= e->rr->sr_ttl)
	break;
	
      sres_htable_remove(cache->cache_hash, e);
      
      _sres_cache_free_one(cache, e->rr);
    }
  }

  UNLOCK(cache);
}

HTABLE_BODIES_WITH(sres_htable, ht, sres_rr_hash_entry_t, SRES_HENTRY_HASH,
		   unsigned, size_t);
