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

#ifndef TPORT_WS_H
/** Defined when <tport_ws.h> has been included. */
#define TPORT_WS_H
/**@internal
 * @file tport_ws.h
 * @brief Internal WS interface
 *
 * @author Mike Jerris <mike@jerris.com>
 *
 * Copyright 2013 Michael Jerris.  All rights reserved.
 *
 */

#ifndef SU_TYPES_H
#include <sofia-sip/su_types.h>
#endif

#include "tport_internal.h"
#include "ws.h"

SOFIA_BEGIN_DECLS

typedef enum {
    TPORT_WS_OPCODE_CONTINUATION = 0x0,
    TPORT_WS_OPCODE_TEXT = 0x1,
    TPORT_WS_OPCODE_BINARY = 0x2,
    TPORT_WS_OPCODE_CLOSE = 0x8,
    TPORT_WS_OPCODE_PING = 0x9,
    TPORT_WS_OPCODE_PONG = 0xA
} tport_ws_opcode_t;

typedef struct tport_ws_s {
  tport_t  wstp_tp[1];
  wsh_t    ws[1];
  char    *wstp_buffer;
  SU_S8_T  ws_initialized;
  unsigned ws_secure:1;
  unsigned:0;
} tport_ws_t;

typedef struct tport_ws_primary_s {
  tport_primary_t wspri_pri[1];
  SSL_CTX *ssl_ctx;
  const SSL_METHOD *ssl_method;
  unsigned ws_secure:1;
  unsigned :0;
} tport_ws_primary_t;

int tport_recv_stream_ws(tport_t *self);
ssize_t tport_send_stream_ws(tport_t const *self, msg_t *msg,
			  msg_iovec_t iov[], size_t iovused);

int tport_ws_ping(tport_t *self, su_time_t now);
int tport_ws_pong(tport_t *self);

int tport_ws_init_primary(tport_primary_t *,
 			  tp_name_t  tpn[1],
 			  su_addrinfo_t *, tagi_t const *,
 			  char const **return_culprit);
int tport_ws_init_client(tport_primary_t *,
 			 tp_name_t tpn[1],
 			 su_addrinfo_t *, tagi_t const *,
 			 char const **return_culprit);
int tport_ws_init_secondary(tport_t *self, int socket, int accepted,
			     char const **return_reason);

int tport_ws_next_timer(tport_t *self, su_time_t *, char const **);
void tport_ws_timer(tport_t *self, su_time_t);
static void tport_ws_deinit_secondary(tport_t *self);

SOFIA_END_DECLS

#endif
