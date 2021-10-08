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

#ifndef SOFIA_RESOLV_SRES_CACHE_H
/** Defined when <sofia-resolv/sres_cache.h> has been included. */
#define SOFIA_RESOLV_SRES_CACHE_H
/**
 * @file sofia-resolv/sres_cache.h Sofia DNS Resolver Cache.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>,
 *
 * @par Include Context
 * @code
 * #include <sys/types.h>
 * #include <sys/socket.h>
 * #include <netinet/in.h>
 * #include <sofia-resolv/sres_cache.h>
 * @endcode
 *
 */

#include "sofia-resolv/sres_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SRES_CACHE_T
#define SRES_CACHE_T
/** Opaque type of DNS cache object. */
typedef struct sres_cache sres_cache_t;
#endif

#ifndef SRES_RECORD_T
#define SRES_RECORD_T
/** Type representing any DNS record. */
typedef union sres_record sres_record_t;
#endif

enum {
  /** Cache cleanup interval in seconds. */
  SRES_CACHE_TIMER_INTERVAL = 5,
#define SRES_CACHE_TIMER_INTERVAL (SRES_CACHE_TIMER_INTERVAL)
};

/** Create a resolver cache object. */
SRESPUBFUN sres_cache_t *sres_cache_new(int n);

/** Increase reference count on a resolver cache object. */
SRESPUBFUN sres_cache_t *sres_cache_ref(sres_cache_t *);

/** Decrease the reference count on a resolver cache object. */
SRESPUBFUN void sres_cache_unref(sres_cache_t *);

/** Get a list of matching records from cache. */
SRESPUBFUN int sres_cache_get(sres_cache_t *cache,
			      uint16_t type,
			      char const *domain,
			      sres_record_t ***return_cached);

/** Free answers not matching with type */
SRESPUBFUN int sres_cache_filter(sres_cache_t *cache,
				 sres_record_t **answers,
				 uint16_t type);

/** Free the list records. */
SRESPUBFUN void sres_cache_free_answers(sres_cache_t *, sres_record_t **);

/** Free and zero one record. */
SRESPUBFUN void sres_cache_free_one(sres_cache_t *, sres_record_t *answer);

/** Copy list of records */
SRESPUBFUN
sres_record_t **sres_cache_copy_answers(sres_cache_t *, sres_record_t **);

/** Remove old records from cache.  */
SRESPUBFUN void sres_cache_clean(sres_cache_t *cache, time_t now);

/** Allocate a cache record */
SRESPUBFUN
sres_record_t *sres_cache_alloc_record(sres_cache_t *cache,
				       sres_record_t const *template,
				       size_t extra);

/** Free a record that has not been stored. */
SRESPUBFUN void sres_cache_free_record(sres_cache_t *cache, void *rr);

/** Store a record to cache */
SRESPUBFUN void sres_cache_store(sres_cache_t *, sres_record_t *, time_t now);

/** Modify the priority in the specified SRV record */
SRESPUBFUN int sres_cache_set_srv_priority(sres_cache_t *,
					   char const *domain,
					   char const *target,
					   uint16_t port,
					   uint32_t newttl,
					   uint16_t newprio);

#ifdef __cplusplus
}
#endif

#endif /* SOFIA_RESOLV_SRES_CACHED_H */
