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
 * $Id: mrcp_client_connection.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef MRCP_CLIENT_CONNECTION_H
#define MRCP_CLIENT_CONNECTION_H

/**
 * @file mrcp_client_connection.h
 * @brief MRCPv2 Client Connection
 */ 

#include "apt_task.h"
#include "mrcp_connection_types.h"

APT_BEGIN_EXTERN_C

/**
 * Create connection agent.
 * @param id the identifier of the agent
 * @param max_connection_count the number of max MRCPv2 connections
 * @param offer_new_connection the connection establishment policy in o/a
 * @param pool the pool to allocate memory from
 */
MRCP_DECLARE(mrcp_connection_agent_t*) mrcp_client_connection_agent_create(
											const char *id,
											apr_size_t max_connection_count,
											apt_bool_t offer_new_connection,
											apr_pool_t *pool);

/**
 * Destroy connection agent.
 * @param agent the agent to destroy
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_connection_agent_destroy(mrcp_connection_agent_t *agent);

/**
 * Start connection agent and wait for incoming requests.
 * @param agent the agent to start
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_connection_agent_start(mrcp_connection_agent_t *agent);

/**
 * Terminate connection agent.
 * @param agent the agent to terminate
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_connection_agent_terminate(mrcp_connection_agent_t *agent);


/**
 * Set connection event handler.
 * @param agent the agent to set event hadler for
 * @param obj the external object to associate with the agent
 * @param vtable the event handler virtual methods
 */
MRCP_DECLARE(void) mrcp_client_connection_agent_handler_set(
								mrcp_connection_agent_t *agent, 
								void *obj, 
								const mrcp_connection_event_vtable_t *vtable);

/**
 * Set MRCP resource factory.
 * @param agent the agent to set resource factory for
 * @param resource_factory the MRCP resource factory to set
 */
MRCP_DECLARE(void) mrcp_client_connection_resource_factory_set(
								mrcp_connection_agent_t *agent,
								const mrcp_resource_factory_t *resource_factory);
/**
 * Set rx buffer size.
 * @param agent the agent to set buffer size for
 * @param size the size of rx buffer to set
 */
MRCP_DECLARE(void) mrcp_client_connection_rx_size_set(
								mrcp_connection_agent_t *agent,
								apr_size_t size);

/**
 * Set tx buffer size.
 * @param agent the agent to set buffer size for
 * @param size the size of the rx buffer to set
 */
MRCP_DECLARE(void) mrcp_client_connection_tx_size_set(
								mrcp_connection_agent_t *agent,
								apr_size_t size);
/**
 * Set request timeout.
 * @param agent the agent to set timeout for
 * @param timeout the timeout to set
 */
MRCP_DECLARE(void) mrcp_client_connection_timeout_set(
								mrcp_connection_agent_t *agent,
								apr_size_t timeout);

/**
 * Get task.
 * @param agent the agent to get task from
 */
MRCP_DECLARE(apt_task_t*) mrcp_client_connection_agent_task_get(const mrcp_connection_agent_t *agent);

/**
 * Get external object.
 * @param agent the agent to get object from
 */
MRCP_DECLARE(void*) mrcp_client_connection_agent_object_get(const mrcp_connection_agent_t *agent);

/**
 * Get string identifier.
 * @param agent the agent to get identifier of
 */
MRCP_DECLARE(const char*) mrcp_client_connection_agent_id_get(const mrcp_connection_agent_t *agent);


/**
 * Create control channel.
 * @param agent the agent to create channel for
 * @param obj the external object to associate with the control channel
 * @param pool the pool to allocate memory from
 */
MRCP_DECLARE(mrcp_control_channel_t*) mrcp_client_control_channel_create(
										mrcp_connection_agent_t *agent, 
										void *obj, 
										apr_pool_t *pool);

/**
 * Add MRCPv2 control channel.
 * @param channel the control channel to add
 * @param descriptor the control descriptor
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_control_channel_add(
										mrcp_control_channel_t *channel, 
										mrcp_control_descriptor_t *descriptor);

/**
 * Modify MRCPv2 control channel.
 * @param channel the control channel to modify
 * @param descriptor the control descriptor
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_control_channel_modify(
										mrcp_control_channel_t *channel, 
										mrcp_control_descriptor_t *descriptor);

/**
 * Remove MRCPv2 control channel.
 * @param channel the control channel to remove
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_control_channel_remove(mrcp_control_channel_t *channel);

/**
 * Destroy MRCPv2 control channel.
 * @param channel the control channel to destroy
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_control_channel_destroy(mrcp_control_channel_t *channel);

/**
 * Send MRCPv2 message.
 * @param channel the control channel to send message through
 * @param message the message to send
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_control_message_send(mrcp_control_channel_t *channel, mrcp_message_t *message);

/**
 * Set the logger object.
 * @param channel the control channel to set the object for
 * @param log_obj the object to set
 */
MRCP_DECLARE(void) mrcp_client_control_channel_log_obj_set(mrcp_control_channel_t *channel, void *log_obj);


APT_END_EXTERN_C

#endif /* MRCP_CLIENT_CONNECTION_H */
