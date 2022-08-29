/*
 * Copyright 2008-2015 Arsen Chaloyan
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
 */

#ifndef MRCP_CONNECTION_TYPES_H
#define MRCP_CONNECTION_TYPES_H

/**
 * @file mrcp_connection_types.h
 * @brief MRCP Connection Types Declaration
 */ 

#include <apr_network_io.h>
#include "apt_string.h"
#include "apt_timer_queue.h"
#include "mrcp_types.h"

APT_BEGIN_EXTERN_C

/** Opaque MRCPv2 control descriptor declaration */
typedef struct mrcp_control_descriptor_t mrcp_control_descriptor_t;

/** Opaque MRCPv2 connection declaration */
typedef struct mrcp_connection_t mrcp_connection_t;

/** Opaque MRCPv2 control channel declaration */
typedef struct mrcp_control_channel_t mrcp_control_channel_t;

/** Opaque MRCPv2 connection agent declaration */
typedef struct mrcp_connection_agent_t mrcp_connection_agent_t;

/** Opaque MRCPv2 connection agent factory declaration */
typedef struct mrcp_ca_factory_t mrcp_ca_factory_t;

/** MRCPv2 connection event vtable declaration */
typedef struct mrcp_connection_event_vtable_t mrcp_connection_event_vtable_t;

/** MRCPv2 connection event vtable */
struct mrcp_connection_event_vtable_t {
	/** Channel add event handler */
	apt_bool_t (*on_add)(mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor, apt_bool_t status);
	/** Channel modify event handler */
	apt_bool_t (*on_modify)(mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor, apt_bool_t status);
	/** Channel remove event handler */
	apt_bool_t (*on_remove)(mrcp_control_channel_t *channel, apt_bool_t status);
	/** Message receive event handler */
	apt_bool_t (*on_receive)(mrcp_control_channel_t *channel, mrcp_message_t *message);
	/** Disconnect event handler */
	apt_bool_t (*on_disconnect)(mrcp_control_channel_t *channel);
};

/** MRCPv2 control channel */
struct mrcp_control_channel_t {
	/** MRCPv2 Connection agent */
	mrcp_connection_agent_t *agent;
	/** MRCPv2 (shared) connection */
	mrcp_connection_t       *connection;
	/** Request sent to the server and waiting for a response */
	mrcp_message_t          *active_request;
	/** Timer used for request timeouts */
	apt_timer_t             *request_timer;
	/** Indicate removed connection (safe to destroy) */
	apt_bool_t               removed;
	/** External object associated with the channel */
	void                    *obj;
	/** External logger object associated with the channel */
	void                    *log_obj;
	/** Pool to allocate memory from */
	apr_pool_t              *pool;
	/** Channel identifier (id at resource) */
	apt_str_t                identifier;
};

/** Send channel add response */
static APR_INLINE apt_bool_t mrcp_control_channel_add_respond(
						const mrcp_connection_event_vtable_t *vtable, 
						mrcp_control_channel_t *channel, 
						mrcp_control_descriptor_t *descriptor,
						apt_bool_t status)
{
	if(vtable && vtable->on_add) {
		return vtable->on_add(channel,descriptor,status);
	}
	return FALSE;
}

/** Send channel modify response */
static APR_INLINE apt_bool_t mrcp_control_channel_modify_respond(
						const mrcp_connection_event_vtable_t *vtable, 
						mrcp_control_channel_t *channel, 
						mrcp_control_descriptor_t *descriptor,
						apt_bool_t status)
{
	if(vtable && vtable->on_modify) {
		return vtable->on_modify(channel,descriptor,status);
	}
	return FALSE;
}

/** Send channel remove response */
static APR_INLINE apt_bool_t mrcp_control_channel_remove_respond(
						const mrcp_connection_event_vtable_t *vtable, 
						mrcp_control_channel_t *channel,
						apt_bool_t status)
{
	if(vtable && vtable->on_remove) {
		return vtable->on_remove(channel,status);
	}
	return FALSE;
}

/** Send MRCP message receive event */
static APR_INLINE apt_bool_t mrcp_connection_message_receive(
						const mrcp_connection_event_vtable_t *vtable,
						mrcp_control_channel_t *channel, 
						mrcp_message_t *message)
{
	if(vtable && vtable->on_receive) {
		return vtable->on_receive(channel,message);
	}
	return FALSE;
}

APT_END_EXTERN_C

#endif /* MRCP_CONNECTION_TYPES_H */
