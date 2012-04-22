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

#ifndef SOFIA_RESOLV_SRES_H
/** Defined when <sofia-resolv/sres.h> has been included. */
#define SOFIA_RESOLV_SRES_H
/**
 * @file sofia-resolv/sres.h Sofia DNS Resolver.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>,
 * @author Teemu Jalava <Teemu.Jalava@nokia.com>,
 * @author Mikko Haataja <ext-Mikko.A.Haataja@nokia.com>.
 *
 * @par Include Context
 * @code
 * #include <sys/types.h>
 * #include <sys/socket.h>
 * #include <netinet/in.h>
 * #include <sofia-resolv/sres.h>
 * @endcode
 *
 */

#include <stdarg.h>
#include "sofia-resolv/sres_config.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
  /** Resolver timer interval in milliseconds. */
  SRES_RETRANSMIT_INTERVAL = 500,
#define SRES_RETRANSMIT_INTERVAL  (SRES_RETRANSMIT_INTERVAL)

  /** Initial retry interval in seconds. */
  SRES_RETRY_INTERVAL = 1,
#define SRES_RETRY_INTERVAL  (SRES_RETRY_INTERVAL)

  /** Maximum number of retries sent. */
  SRES_MAX_RETRY_COUNT = 6,
#define SRES_MAX_RETRY_COUNT (SRES_MAX_RETRY_COUNT)

  /** Maximum number of search domains. */
  SRES_MAX_SEARCH = 6,
#define SRES_MAX_SEARCH (SRES_MAX_SEARCH)

  /** Maximum number of nameservers. */
  SRES_MAX_NAMESERVERS = 6,
#define SRES_MAX_NAMESERVERS (SRES_MAX_NAMESERVERS)

  /** Maximum length of domain name. */
  SRES_MAXDNAME = 1025,
#define SRES_MAXDNAME (SRES_MAXDNAME)

  /** Maximum length of sortlist */
  SRES_MAX_SORTLIST = 10
#define SRES_MAX_SORTLIST (SRES_MAX_SORTLIST)
};

#ifndef SRES_RECORD_T
#define SRES_RECORD_T
/** Type representing any DNS record. */
typedef union sres_record sres_record_t;
#endif

#ifndef SRES_CACHE_T
#define SRES_CACHE_T
/** Opaque type of DNS cache object. */
typedef struct sres_cache sres_cache_t;
#endif

/** Opaque type of DNS resolver object. */
typedef struct sres_resolver_s sres_resolver_t;

#ifndef SRES_CONTEXT_T
#define SRES_CONTEXT_T struct sres_context_s
#endif
/** Application-defined type for sres_query_t context. */
typedef SRES_CONTEXT_T sres_context_t;

/** Opaque type of DNS query object. */
typedef struct sres_query_s         sres_query_t;

struct sockaddr;

/** New resolver object. */
SRESPUBFUN sres_resolver_t *sres_resolver_new(char const *resolv_conf_path);

/** Copy a resolver. */
SRESPUBFUN sres_resolver_t *sres_resolver_copy(sres_resolver_t *);

/** New resolver object. */
SRESPUBFUN
sres_resolver_t *
sres_resolver_new_with_cache(char const *conf_file_path,
			     sres_cache_t *cache,
			     char const *options, ...);

/** New resolver object. */
SRESPUBFUN
sres_resolver_t *
sres_resolver_new_with_cache_va(char const *conf_file_path,
				sres_cache_t *cache,
				char const *options, va_list va);

/** Increase reference count on a resolver object. */
SRESPUBFUN sres_resolver_t *sres_resolver_ref(sres_resolver_t *res);

/** Decrease the reference count on a resolver object.  */
SRESPUBFUN void sres_resolver_unref(sres_resolver_t *res);

/** Re-read resolv.conf if needed */
SRESPUBFUN int sres_resolver_update(sres_resolver_t *res, int always);

/** Set userdata pointer. */
SRESPUBFUN
void *sres_resolver_set_userdata(sres_resolver_t *res, void *userdata);

/** Get userdata pointer. */
SRESPUBFUN
void *sres_resolver_get_userdata(sres_resolver_t const *res);

/** Prototype for callback function.
 *
 * This kind of function is called when a query is completed. The called
 * function is responsible for freeing the list of answers and it must
 * (eventually) call sres_free_answers().
 */
typedef void sres_answer_f(sres_context_t *context,
			   sres_query_t *query,
			   sres_record_t **answers);

/** Make a DNS query. */
SRESPUBFUN
sres_query_t *sres_query(sres_resolver_t *res,
                         sres_answer_f *callback,
                         sres_context_t *context,
                         uint16_t type,
                         char const *domain);

/** Search DNS. */
SRESPUBFUN
sres_query_t *sres_search(sres_resolver_t *res,
			  sres_answer_f *callback,
			  sres_context_t *context,
			  uint16_t type,
			  char const *name);

/** Make a reverse DNS query. */
SRESPUBFUN
sres_query_t *sres_query_sockaddr(sres_resolver_t *res,
                                  sres_answer_f *callback,
                                  sres_context_t *context,
                                  uint16_t type,
				  struct sockaddr const *addr);

/** Make a DNS query with socket. @deprecated */
SRESPUBFUN
sres_query_t *sres_query_make(sres_resolver_t *res,
			      sres_answer_f *callback,
			      sres_context_t *context,
			      int dummy,
			      uint16_t type,
			      char const *domain);

/** Make a reverse DNS query with socket. @deprecated */
SRESPUBFUN
sres_query_t *sres_query_make_sockaddr(sres_resolver_t *res,
				       sres_answer_f *callback,
				       sres_context_t *context,
				       int dummy,
				       uint16_t type,
				       struct sockaddr const *addr);

/** Rebind a DNS query. */
SRESPUBFUN
void sres_query_bind(sres_query_t *q,
                     sres_answer_f *callback,
                     sres_context_t *context);

/**Get a list of matching (type/domain) records from cache. */
SRESPUBFUN
sres_record_t **sres_cached_answers(sres_resolver_t *res,
				    uint16_t type,
				    char const *domain);

/**Search for a list of matching (type/name) records from cache. */
SRESPUBFUN
sres_record_t **sres_search_cached_answers(sres_resolver_t *res,
					   uint16_t type,
					   char const *name);

/**Get a list of matching (type/domain) records from cache. */
SRESPUBFUN
sres_record_t **sres_cached_answers_sockaddr(sres_resolver_t *res,
                                             uint16_t type,
					     struct sockaddr const *addr);

/**Modify the priority of the specified SRV records. */
SRESPUBFUN
int sres_set_cached_srv_priority(sres_resolver_t *res,
				 char const *domain,
				 char const *target,
				 uint16_t port,
				 uint32_t newttl,
				 uint16_t newprio);


/** Send a query, wait for answer, return results. */
SRESPUBFUN
int sres_blocking_query(sres_resolver_t *res,
			uint16_t type,
			char const *domain,
			int ignore_cache,
			sres_record_t ***return_records);

/** Search DNS, return results. */
SRESPUBFUN
int sres_blocking_search(sres_resolver_t *res,
			 uint16_t type,
			 char const *name,
			 int ignore_cache,
			 sres_record_t ***return_records);

/** Send a a reverse DNS query, wait for answer, return results. */
SRESPUBFUN
int sres_blocking_query_sockaddr(sres_resolver_t *res,
				 uint16_t type,
				 struct sockaddr const *addr,
				 int ignore_cache,
				 sres_record_t ***return_records);

/** Return true (and set resolver in blocking mode) if resolver can block. */
SRESPUBFUN int sres_is_blocking(sres_resolver_t *res);

/** Sort the list of records */
SRESPUBFUN int sres_sort_answers(sres_resolver_t *, sres_record_t **answers);

/** Filter and sort the list of records */
SRESPUBFUN
int sres_filter_answers(sres_resolver_t *res,
			sres_record_t **answers,
			uint16_t type);

/** Free the list records. */
SRESPUBFUN void sres_free_answers(sres_resolver_t *, sres_record_t **answers);

/** Free and zero one record. */
SRESPUBFUN void sres_free_answer(sres_resolver_t *res, sres_record_t *answer);

#ifdef __cplusplus
}
#endif

#endif /* SOFIA_RESOLV_SRES_H */
