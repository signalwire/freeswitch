/*
 * Copyright 2008-2014 Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * $Id: mrcp_connection.h 2170 2014-09-09 05:19:48Z achaloyan@gmail.com $
 */

#ifndef MRCP_CONNECTION_H
#define MRCP_CONNECTION_H

/**
 * @file mrcp_connection.h
 * @brief MRCP Connection
 */ 

#include <apr_poll.h>
#include <apr_hash.h>
#ifdef WIN32
#pragma warning(disable: 4127)
#endif
#include <apr_ring.h>
#include "mrcp_connection_types.h"
#include "mrcp_stream.h"

APT_BEGIN_EXTERN_C

/** Size of the buffer used for MRCP rx/tx stream */
#define MRCP_STREAM_BUFFER_SIZE 1024

/** MRCPv2 connection */
struct mrcp_connection_t {
	/** Ring entry */
	APR_RING_ENTRY(mrcp_connection_t) link;

	/** Memory pool */
	apr_pool_t       *pool;

	/** Accepted/Connected socket */
	apr_socket_t     *sock;
	/** Socket poll descriptor */
	apr_pollfd_t      sock_pfd;
	/** Local sockaddr */
	apr_sockaddr_t   *l_sockaddr;
	/** Remote sockaddr */
	apr_sockaddr_t   *r_sockaddr;
	/** Remote IP */
	apt_str_t         remote_ip;
	/** String identifier used for traces */
	const char       *id;
	/** Transparently dump whatever received/sent on transport layer, 
	if verbose is set to TRUE (default) */
	apt_bool_t        verbose;

	/** Reference count */
	apr_size_t        access_count;
	/** Opaque agent */
	void             *agent;

	/** Table of control channels */
	apr_hash_t       *channel_table;

	/** Rx buffer */
	char             *rx_buffer;
	/** Rx buffer size */
	apr_size_t        rx_buffer_size;
	/** Rx stream */
	apt_text_stream_t rx_stream;
	/** MRCP parser to parser MRCP messages out of rx stream */
	mrcp_parser_t    *parser;

	/** Tx buffer */
	char             *tx_buffer;
	/** Tx buffer size */
	apr_size_t        tx_buffer_size;
	/** MRCP generator to generate MRCP messages into tx stream */
	mrcp_generator_t *generator;
};

/** Create MRCP connection. */
mrcp_connection_t* mrcp_connection_create(void);

/** Destroy MRCP connection. */
void mrcp_connection_destroy(mrcp_connection_t *connection);

/** Add Control Channel to MRCP connection. */
apt_bool_t mrcp_connection_channel_add(mrcp_connection_t *connection, mrcp_control_channel_t *channel);

/** Find Control Channel by Channel Identifier. */
mrcp_control_channel_t* mrcp_connection_channel_find(const mrcp_connection_t *connection, const apt_str_t *identifier);

/** Remove Control Channel from MRCP connection. */
apt_bool_t mrcp_connection_channel_remove(mrcp_connection_t *connection, mrcp_control_channel_t *channel);

/** Raise disconnect event for each channel from the specified connection. */
apt_bool_t mrcp_connection_disconnect_raise(mrcp_connection_t *connection, const mrcp_connection_event_vtable_t *vtable);

APT_END_EXTERN_C

#endif /* MRCP_CONNECTION_H */
