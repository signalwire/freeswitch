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

#ifndef MSG_BUFFER_H
/** Defined when <sofia-sip/msg_buffer.h> has been included. */
#define MSG_BUFFER_H

/**@file sofia-sip/msg_buffer.h
 * @brief Internal buffer management functions.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri Nov  8 12:23:00 2002 ppessi
 *
 */

#ifndef MSG_TYPES_H
#include <sofia-sip/msg_types.h>
#endif

SOFIA_BEGIN_DECLS

SOFIAPUBFUN void *msg_buf_alloc(msg_t *msg, usize_t size);
SOFIAPUBFUN void *msg_buf_exact(msg_t *msg, usize_t size);
SOFIAPUBFUN usize_t msg_buf_commit(msg_t *msg, usize_t size, int eos);
SOFIAPUBFUN usize_t msg_buf_committed(msg_t const *msg);
SOFIAPUBFUN void *msg_buf_committed_data(msg_t const *msg);
SOFIAPUBFUN usize_t msg_buf_size(msg_t const *msg);

SOFIAPUBFUN void *msg_buf_move(msg_t *dst, msg_t const *src);

SOFIAPUBFUN void msg_buf_set(msg_t *msg, void *b, usize_t size);

SOFIA_END_DECLS

#endif /* !defined MSG_BUFFER_H */
