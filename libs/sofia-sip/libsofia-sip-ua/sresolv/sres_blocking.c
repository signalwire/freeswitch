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

/**@CFILE sres_blocking.c
 * @brief Blocking interface for Sofia DNS Resolver implementation.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @date Created: Fri Mar 24 15:23:08 EET 2006 ppessi
 */

#include "config.h"

#if HAVE_STDINT_H
#include <stdint.h>
#elif HAVE_INTTYPES_H
#include <inttypes.h>
#else
#if defined(_WIN32)
typedef _int8 int8_t;
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
#define HAVE_SELECT 1
#else
#define SOCKET_ERROR   (-1)
#define INVALID_SOCKET ((sres_socket_t)-1)
#endif

typedef struct sres_blocking_s sres_blocking_t;
typedef struct sres_blocking_context_s sres_blocking_context_t;

#define SRES_CONTEXT_T struct sres_blocking_context_s
#define SRES_ASYNC_T struct sres_blocking_s

#include "sofia-resolv/sres.h"
#include "sofia-resolv/sres_async.h"
#include <sofia-sip/su_errno.h>

#if HAVE_POLL
#include <poll.h>
#elif HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <stdlib.h>
#include <errno.h>

struct sres_blocking_s
{
  int              n_sockets;
#if HAVE_POLL
  struct pollfd    fds[SRES_MAX_NAMESERVERS];
#elif HAVE_SELECT
  struct { sres_socket_t fd; } fds[SRES_MAX_NAMESERVERS];
#else
#warning No guaranteed wait mechanism!
/* typedef struct os_specific su_wait_t; */
struct _pollfd {
  sres_socket_t fd;   /* file descriptor */
  short events;     /* requested events */
  short revents;    /* returned events */
} fds[SRES_MAX_NAMESERVERS];
#endif
  sres_record_t ***return_records;
};

struct sres_blocking_context_s
{
  int          ready;
  sres_resolver_t *resolver;
  sres_blocking_t *block;
  sres_query_t *query;
  sres_record_t ***return_records;
};

static
int sres_blocking_update(sres_blocking_t *b,
			 sres_socket_t new_socket,
			 sres_socket_t old_socket)
{
  int i, N;

  if (b == NULL)
    return -1;

  if (old_socket == new_socket) {
    if (old_socket == INVALID_SOCKET) {
      free(b);      /* Destroy us */
    }
    return 0;
  }

  N = b->n_sockets;

  if (old_socket != INVALID_SOCKET) {
    for (i = 0; i < N; i++) {
      if (b->fds[i].fd == old_socket)
	break;
    }
    if (i == N)
      return -1;

    N--;
    b->fds[i].fd = b->fds[N].fd;
    b->fds[N].fd = INVALID_SOCKET;
#if HAVE_POLL
    b->fds[i].events = b->fds[N].events;
    b->fds[N].events = 0;
#endif

    b->n_sockets = N;
  }

  if (new_socket != INVALID_SOCKET) {
    if (N == SRES_MAX_NAMESERVERS)
      return -1;
    b->fds[N].fd = new_socket;
#if HAVE_POLL
    b->fds[N].events = POLLIN;
#endif
    b->n_sockets = N + 1;
  }

  return 0;
}

static
int sres_blocking_complete(sres_blocking_context_t *c)
{
  while (!c->ready) {
    int n, i;
#if HAVE_POLL
    n = poll(c->block->fds, c->block->n_sockets, 500);
    if (n < 0) {
      c->ready = n;
    }
    else if (n == 0) {
      sres_resolver_timer(c->resolver, -1);
    }
    else for (i = 0; i < c->block->n_sockets; i++) {
      if (c->block->fds[i].revents | POLLERR)
	sres_resolver_error(c->resolver, c->block->fds[i].fd);
      if (c->block->fds[i].revents | POLLIN)
	sres_resolver_receive(c->resolver, c->block->fds[i].fd);
    }
#elif HAVE_SELECT
    fd_set readfds[1], errorfds[1];
    struct timeval timeval[1];

    FD_ZERO(readfds);
    FD_ZERO(errorfds);

    timeval->tv_sec = 0;
    timeval->tv_usec = 500000;

    for (i = 0, n = 0; i < c->block->n_sockets; i++) {
      FD_SET(c->block->fds[i].fd, readfds);
      FD_SET(c->block->fds[i].fd, errorfds);
      if (c->block->fds[i].fd >= n)
	n = c->block->fds[i].fd + 1;
    }

    n = select(n, readfds, NULL, errorfds, timeval);

    if (n <= 0)
      sres_resolver_timer(c->resolver, -1);
    else for (i = 0; n > 0 && i < c->block->n_sockets; i++) {
      if (FD_ISSET(c->block->fds[i].fd, errorfds))
        sres_resolver_error(c->resolver, c->block->fds[i].fd);
      else if (FD_ISSET(c->block->fds[i].fd, readfds))
	sres_resolver_receive(c->resolver, c->block->fds[i].fd);
      else
	continue;
      n--;
    }
#endif
  }

  return c->ready;
}

static
void sres_blocking_callback(sres_blocking_context_t *c,
			    sres_query_t *query,
			    sres_record_t **answers)
{
  c->ready = 1;
  *c->return_records = answers;
}

static
sres_blocking_t *sres_set_blocking(sres_resolver_t *res)
{
  sres_blocking_t *b;
  int i;

  b = sres_resolver_get_async(res, sres_blocking_update);
  if (b)
    return b;

  /* Check if resolver is already in asynchronous mode */
  if (sres_resolver_get_async(res, NULL))
    return NULL;

  /* Create a synchronous (blocking) interface towards resolver */
  b = calloc(1, sizeof *b);

  if (b) {
    for (i = 0; i < SRES_MAX_NAMESERVERS; i++)
      b->fds[i].fd = INVALID_SOCKET;

    if (!sres_resolver_set_async(res, sres_blocking_update, b, 0)) {
      free(b), b = NULL;
    }
  }

  return b;
}

/** Return true (and set resolver in blocking mode) if resolver can block. */
int sres_is_blocking(sres_resolver_t *res)
{
  if (res == NULL)
    return 0;
  return sres_set_blocking(res) != NULL;
}

/**Send a DNS query, wait for response, return results.
 *
 * Sends a DNS query with specified @a type and @a domain to the DNS server,
 * if @a ignore_cache is not given or no records are found from cache.
 * Function returns an error record with nonzero status if no response is
 * received from DNS server.
 *
 * @param res pointer to resolver object
 * @param type record type to search (or sres_qtype_any for any record)
 * @param domain domain name to query
 * @param ignore_cache ignore cached answers if nonzero
 * @param return_records return-value parameter for dns records
 *
 * @retval >0 if query was responded
 * @retval 0 if result was found from cache
 * @retval -1 upon error
 *
 * @ERRORS
 * @ERROR EFAULT @a res or @a domain point outside the address space
 * @ERROR ENAMETOOLONG @a domain is longer than SRES_MAXDNAME
 * @ERROR ENETDOWN no DNS servers configured
 * @ERROR ENOMEM memory exhausted
 * @ERROR EOPNOTSUPP  resolver @a res is in asynchronous mode
 *
 * @sa sres_query(), sres_blocking_search()
 *
 * @note A blocking query converts a resolver object permanently into
 * blocking mode. If you need to make blocking and non-blocking queries, use
 * sres_resolver_copy() to make a separate resolver object for blocking
 * queries.
 */
int sres_blocking_query(sres_resolver_t *res,
			uint16_t type,
			char const *domain,
			int ignore_cache,
			sres_record_t ***return_records)
{
  sres_blocking_context_t c[1];
  sres_record_t **cached;

  if (return_records == NULL)
    return su_seterrno(EFAULT);

  *return_records = NULL;

  c->block = sres_set_blocking(res);
  if (c->block == NULL)
    return su_seterrno(EOPNOTSUPP); /* Resolver in asynchronous mode */

  if (!ignore_cache) {
    cached = sres_cached_answers(res, type, domain);
    if (cached) {
      *return_records = cached;
      return 0;
    }
  }

  c->ready = 0;
  c->resolver = res;
  c->return_records = return_records;
  c->query = sres_query(res, sres_blocking_callback, c, type, domain);

  return sres_blocking_complete(c);
}

/** Search DNS, return results.
 *
 * Search for @a name with specified @a type and @a name from the DNS server.
 * If the @a name does not contain enought dots, the search domains are
 * appended to the name and resulting domain name are also queried.
 *
 * @param res pointer to resolver object
 * @param type record type to search (or sres_qtype_any for any record)
 * @param name host or domain name to search from DNS
 * @param ignore_cache ignore cached answers if nonzero
 * @param return_records return-value parameter for dns records
 *
 * @retval >0 if query was responded
 * @retval 0 if result was found from cache
 * @retval -1 upon error
 *
 * @ERRORS
 * @ERROR EFAULT @a res or @a domain point outside the address space
 * @ERROR ENAMETOOLONG @a domain is longer than SRES_MAXDNAME
 * @ERROR ENETDOWN no DNS servers configured
 * @ERROR ENOMEM memory exhausted
 * @ERROR EOPNOTSUPP  resolver @a res is in asynchronous mode
 *
 * @sa sres_blocking_query(), sres_search()
 *
 * @note A blocking query converts a resolver object permanently into
 * blocking mode. If you need to make blocking and non-blocking queries, use
 * sres_resolver_copy() to make a separate resolver object for blocking
 * queries.
 */
int sres_blocking_search(sres_resolver_t *res,
			 uint16_t type,
			 char const *name,
			 int ignore_cache,
			 sres_record_t ***return_records)
{
  sres_blocking_context_t c[1];
  sres_record_t **cached;

  if (return_records == NULL)
    return su_seterrno(EFAULT);

  *return_records = NULL;

  c->block = sres_set_blocking(res);
  if (c->block == NULL)
    return su_seterrno(EOPNOTSUPP); /* Resolver in asynchronous mode */

  if (!ignore_cache) {
    cached = sres_search_cached_answers(res, type, name);
    if (cached) {
      *return_records = cached;
      return 0;
    }
  }

  c->ready = 0;
  c->resolver = res;
  c->return_records = return_records;
  c->query = sres_search(res, sres_blocking_callback, c, type, name);

  return sres_blocking_complete(c);
}

/** Send a a reverse DNS query, return results.
 *
 * Sends a reverse DNS query with specified @a type and @a domain to the DNS
 * server if @a ignore_cache is not given or no cached records are found from
 * the cache. Function returns an error record with nonzero status if no
 * response is received from DNS server.
 *
 * @retval >0 if query was responded
 * @retval 0 if result was found from cache
 * @retval -1 upon error
 *
 * @ERRORS
 * @ERROR EFAULT @a res or @a addr point outside the address space
 * @ERROR ENOMEM memory exhausted
 * @ERROR ENETDOWN no DNS servers configured
 * @ERROR EOPNOTSUPP  resolver @a res is in asynchronous mode
 *
 * @sa sres_blocking_query(), sres_query_sockaddr(), sres_cached_answers_sockaddr()
 *
 * @note A blocking query converts a resolver object permanently into
 * blocking mode. If you need to make blocking and non-blocking queries, use
 * sres_resolver_copy() to make a separate resolver object for blocking
 * queries.
 */
int sres_blocking_query_sockaddr(sres_resolver_t *res,
				 uint16_t type,
				 struct sockaddr const *addr,
				 int ignore_cache,
				 sres_record_t ***return_records)
{
  sres_blocking_context_t c[1];
  sres_record_t **cached;

  if (return_records == NULL)
    return errno = EFAULT, -1;

  *return_records = NULL;

  c->block = sres_set_blocking(res);
  if (c->block == NULL)
    return su_seterrno(EOPNOTSUPP); /* Resolver in asynchronous mode */

  if (!ignore_cache) {
    cached = sres_cached_answers_sockaddr(res, type, addr);
    if (cached) {
      *return_records = cached;
      return 0;
    }
  }

  c->ready = 0;
  c->resolver = res;
  c->return_records = return_records;
  c->query = sres_query_sockaddr(res, sres_blocking_callback, c, type, addr);

  return sres_blocking_complete(c);
}
