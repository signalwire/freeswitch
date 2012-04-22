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

#ifndef NTA_STATELESS_H
/** Defined when <sofia-sip/nta_stateless.h> has been included. */
#define NTA_STATELESS_H

/**@file sofia-sip/nta_stateless.h
 * @brief NTA functions for stateless SIP processing.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Sep  4 15:54:57 2001 ppessi
 */

#ifndef NTA_H
#include <sofia-sip/nta.h>
#endif

SOFIA_BEGIN_DECLS

/**@typedef int nta_message_f(nta_agent_magic_t *context,
 *                            nta_agent_t *agent,
 *			      msg_t *msg,
 *			      sip_t *sip);
 *
 * Callback for incoming messages.
 *
 * The typedef nta_message_f() defines prototype for the callback functions
 * invoked by NTA when it has received an incoming message that will be
 * processed statelessly.
 *
 * The application can either discard the message by calling
 * nta_msg_discard(), forward it by calling nta_msg_tsend() or reply to the
 * message by calling nta_msg_treply(). When application wants to process a
 * request statefully, it passes the message to a leg with the function
 * nta_leg_stateful(). A new leg can be created by calling the function
 * nta_leg_tcreate().
 *
 * @par Prototype
 * @code
 * int message_callback(nta_agent_magic_t *context,
 *                      nta_agent_t *agent,
 *                      msg_t *msg,
 *                      sip_t *sip);
 * @endcode
 *
 * @param context agent context
 * @param agent   agent handle
 * @param msg     received message
 * @param sip     contents of message
 *
 * @return
 * This callback function should always return 0.
 */

/** Forward a request or response message. */
SOFIAPUBFUN
int nta_msg_tsend(nta_agent_t *agent, msg_t *msg, url_string_t const *u,
		  tag_type_t tag, tag_value_t value, ...);

/** Reply to a request message. */
SOFIAPUBFUN
int nta_msg_mreply(nta_agent_t *agent,
		   msg_t *reply, sip_t *sip,
		   int status, char const *phrase,
		   msg_t *req_msg,
		   tag_type_t tag, tag_value_t value, ...);

/** Reply to a request message. */
SOFIAPUBFUN
int nta_msg_treply(nta_agent_t *self,
		   msg_t *msg,
		   int status, char const *phrase,
		   tag_type_t tag, tag_value_t value, ...);

/** ACK and BYE an unknown 200 OK response to INVITE. */
SOFIAPUBFUN int nta_msg_ackbye(nta_agent_t *a, msg_t *msg);

SOFIA_END_DECLS

#endif /* !defined(NTA_STATELESS_H) */
