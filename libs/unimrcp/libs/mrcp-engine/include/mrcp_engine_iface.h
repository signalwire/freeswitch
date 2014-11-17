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
 * $Id: mrcp_engine_iface.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef MRCP_ENGINE_IFACE_H
#define MRCP_ENGINE_IFACE_H

/**
 * @file mrcp_engine_iface.h
 * @brief MRCP Engine User Interface (typically user is an MRCP server)
 */ 

#include "mrcp_engine_types.h"

APT_BEGIN_EXTERN_C

/** Destroy engine */
apt_bool_t mrcp_engine_virtual_destroy(mrcp_engine_t *engine);

/** Open engine */
apt_bool_t mrcp_engine_virtual_open(mrcp_engine_t *engine);

/** Response to open engine request */
void mrcp_engine_on_open(mrcp_engine_t *engine, apt_bool_t status);

/** Close engine */
apt_bool_t mrcp_engine_virtual_close(mrcp_engine_t *engine);

/** Response to close engine request */
void mrcp_engine_on_close(mrcp_engine_t *engine);


/** Create engine channel */
mrcp_engine_channel_t* mrcp_engine_channel_virtual_create(mrcp_engine_t *engine, mrcp_version_e mrcp_version, apr_pool_t *pool);

/** Destroy engine channel */
apt_bool_t mrcp_engine_channel_virtual_destroy(mrcp_engine_channel_t *channel);

/** Open engine channel */
static APR_INLINE apt_bool_t mrcp_engine_channel_virtual_open(mrcp_engine_channel_t *channel)
{
	if(channel->is_open == FALSE) {
		channel->is_open = channel->method_vtable->open(channel);
		return channel->is_open;
	}
	return FALSE;
}

/** Close engine channel */
static APR_INLINE apt_bool_t mrcp_engine_channel_virtual_close(mrcp_engine_channel_t *channel)
{
	if(channel->is_open == TRUE) {
		channel->is_open = FALSE;
		return channel->method_vtable->close(channel);
	}
	return FALSE;
}

/** Process request */
static APR_INLINE apt_bool_t mrcp_engine_channel_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *message)
{
	return channel->method_vtable->process_request(channel,message);
}

/** Allocate engine config */
mrcp_engine_config_t* mrcp_engine_config_alloc(apr_pool_t *pool);


APT_END_EXTERN_C

#endif /* MRCP_ENGINE_IFACE_H */
