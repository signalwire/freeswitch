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

#ifndef MSG_ADDR_H
/** Defined when <sofia-sip/msg_addr.h> has been included. */
#define MSG_ADDR_H


/**@file sofia-sip/msg_addr.h
 * @brief Addressing and I/O interface for messages.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Jun  8 19:28:55 2000 ppessi
 */

#ifndef MSG_H
#include <sofia-sip/msg.h>
#endif
#ifndef SU_H
#include <sofia-sip/su.h>
#endif

SOFIA_BEGIN_DECLS

SOFIAPUBFUN void msg_addr_zero(msg_t *msg);
SOFIAPUBFUN su_addrinfo_t *msg_addrinfo(msg_t *msg);

SOFIAPUBFUN su_sockaddr_t *msg_addr(msg_t *msg);

SOFIAPUBFUN int msg_get_address(msg_t *msg, su_sockaddr_t *, socklen_t *);
SOFIAPUBFUN int msg_set_address(msg_t *msg, su_sockaddr_t const *, socklen_t);

SOFIAPUBFUN void msg_addr_copy(msg_t *dst, msg_t const *src);

SOFIAPUBFUN int msg_errno(msg_t const *msg);
SOFIAPUBFUN void msg_set_errno(msg_t *msg, int err);

enum {
  /** Minimum size of a message buffer */
  msg_min_size = 512,
  /** Minimum size of external buffer */
  msg_min_block = 8192,
  /** Number of external buffers */
  msg_n_fragments = 8
};

/** I/O vector type.
 * @sa msg_recv_iovec(), msg_iovec(), #su_iovec_s, su_vsend(), su_vrecv().
 */
typedef struct su_iovec_s msg_iovec_t;
#define mv_base siv_base
#define mv_len  siv_len

SOFIAPUBFUN isize_t msg_iovec(msg_t *msg, msg_iovec_t vec[], isize_t veclen);

SOFIAPUBFUN issize_t msg_recv_iovec(msg_t *msg,
				    msg_iovec_t vec[], isize_t veclen, usize_t n,
				    int exact);
SOFIAPUBFUN isize_t msg_recv_commit(msg_t *msg, usize_t n, int eos);

SOFIAPUBFUN issize_t msg_recv_buffer(msg_t *msg, void **return_buffer);

SOFIAPUBFUN msg_t *msg_next(msg_t *msg);

SOFIAPUBFUN int msg_set_next(msg_t *msg, msg_t *next);

SOFIAPUBFUN void msg_clear_committed(msg_t *msg);

SOFIAPUBFUN issize_t msg_buf_external(msg_t *msg,
				      usize_t N,
				      usize_t blocksize);

SOFIA_END_DECLS

#endif /* !defined(MSG_ADDR_H) */
