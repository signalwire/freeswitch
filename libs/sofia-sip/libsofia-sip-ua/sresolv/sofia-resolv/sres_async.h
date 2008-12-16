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

#ifndef SOFIA_RESOLV_SRES_ASYNC_H
/** Defined when <sofia-resolv/sres_async.h> has been included. */
#define SOFIA_RESOLV_SRES_ASYNC_H

/**
 * @file sofia-resolv/sres_async.h
 *
 * Asynchronous interface for Sofia DNS Resolver.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @par Include Context
 * @code
 * #include <sys/types.h>
 * #include <sys/socket.h>
 * #include <netinet/in.h>
 * #include <sofia-resolv/sres.h>
 * #include <sofia-resolv/sres_async.h>
 * @endcode
 *
 */

#include "sofia-resolv/sres_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SRES_ASYNC_T
#define SRES_ASYNC_T struct sres_async_s
#endif
/** Application-defined type for context used by asynchronous operation. */
typedef SRES_ASYNC_T sres_async_t;

/** Prototype for update function.
 *
 * This kind of function is called when the nameserver configuration has
 * been updated.
 *
 * If the old_socket is not -1, it indicates that old_socket will be closed
 * and it should be removed from poll() or select() set.
 *
 * If the new_socket is not -1, it indicates that resolver has created new
 * socket that should be added to the poll() or select() set.
 *
 * @sa sres_resolver_set_async(), sres_resolver_get_async()
 */
typedef int sres_update_f(sres_async_t *async,
			  sres_socket_t new_socket,
			  sres_socket_t old_socket);

/** Set asynchronous operation data. */
SRESPUBFUN
sres_async_t *sres_resolver_set_async(sres_resolver_t *res,
				      sres_update_f *update,
				      sres_async_t *async,
				      int update_all);

/** Get async operation data. */
SRESPUBFUN
sres_async_t *sres_resolver_get_async(sres_resolver_t const *res,
				      sres_update_f *update);

/** Create sockets for resolver. */
SRESPUBFUN int sres_resolver_sockets(sres_resolver_t *,
				     sres_socket_t *sockets,
				     int n);

/** Resolver timer function. */
SRESPUBFUN void sres_resolver_timer(sres_resolver_t *, int dummy);

/** Prototype for scheduler function.
 *
 * This function is called when a timer callback is to be scheduled.
 *
 * @param async asynchronous object (registered with sres_resolver_set_async())
 * @param interval interval in milliseconds
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 */
 typedef int sres_schedule_f(sres_async_t *async, unsigned long interval);

/** Register resolver timer callback. */
SRESPUBFUN int sres_resolver_set_timer_cb(sres_resolver_t *res,
					  sres_schedule_f *callback,
					  sres_async_t *async);

/** Receive DNS response from socket. */
SRESPUBFUN int sres_resolver_receive(sres_resolver_t *, int socket);

/** Receive error message from socket. */
SRESPUBFUN int sres_resolver_error(sres_resolver_t *, int socket);

#ifdef __cplusplus
}
#endif

#endif /* SOFIA_RESOLV_SRES_ASYNC_H */
