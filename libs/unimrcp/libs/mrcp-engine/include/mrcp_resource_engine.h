/*
 * Copyright 2008 Arsen Chaloyan
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

#ifndef __MRCP_RESOURCE_ENGINE_H__
#define __MRCP_RESOURCE_ENGINE_H__

/**
 * @file mrcp_resource_engine.h
 * @brief MRCP Resource Engine Interface
 */ 

#include "mrcp_types.h"
#include "mpf_termination.h"
#include "mpf_stream.h"
#include "mrcp_resource_plugin.h"

APT_BEGIN_EXTERN_C

/** MRCP resource engine vtable declaration */
typedef struct mrcp_engine_method_vtable_t mrcp_engine_method_vtable_t;
/** MRCP engine channel declaration */
typedef struct mrcp_engine_channel_t mrcp_engine_channel_t;
/** MRCP engine channel virtual method table declaration */
typedef struct mrcp_engine_channel_method_vtable_t mrcp_engine_channel_method_vtable_t;
/** MRCP engine channel virtual event table declaration */
typedef struct mrcp_engine_channel_event_vtable_t mrcp_engine_channel_event_vtable_t;

/** Table of channel virtual methods */
struct mrcp_engine_channel_method_vtable_t {
	/** Virtual destroy */
	apt_bool_t (*destroy)(mrcp_engine_channel_t *channel);
	/** Virtual open */
	apt_bool_t (*open)(mrcp_engine_channel_t *channel);
	/** Virtual close */
	apt_bool_t (*close)(mrcp_engine_channel_t *channel);
	/** Virtual process_request */
	apt_bool_t (*process_request)(mrcp_engine_channel_t *channel, mrcp_message_t *request);
};

/** Table of channel virtual event handlers */
struct mrcp_engine_channel_event_vtable_t {
	/** Open event handler */
	apt_bool_t (*on_open)(mrcp_engine_channel_t *channel, apt_bool_t status);
	/** Close event handler */
	apt_bool_t (*on_close)(mrcp_engine_channel_t *channel);
	/** Message event handler */
	apt_bool_t (*on_message)(mrcp_engine_channel_t *channel, mrcp_message_t *message);
};

/** MRCP engine channel declaration */
struct mrcp_engine_channel_t {
	/** Table of virtual methods */
	const mrcp_engine_channel_method_vtable_t *method_vtable;
	/** External object used with virtual methods */
	void                                      *method_obj;
	/** Table of virtual event handlers */
	const mrcp_engine_channel_event_vtable_t  *event_vtable;
	/** External object used with event handlers */
	void                                      *event_obj;
	/** Media termination */
	mpf_termination_t                         *termination;
	/** Back pointer to resource engine */
	mrcp_resource_engine_t                    *engine;
	/** Unique identifier (useful for traces) */
	apt_str_t                                  id;
	/** Pool to allocate memory from */
	apr_pool_t                                *pool;
};

/** Table of MRCP engine virtual methods */
struct mrcp_engine_method_vtable_t {
	/** Virtual destroy */
	apt_bool_t (*destroy)(mrcp_resource_engine_t *engine);
	/** Virtual open */
	apt_bool_t (*open)(mrcp_resource_engine_t *engine);
	/** Virtual close */
	apt_bool_t (*close)(mrcp_resource_engine_t *engine);
	/** Virtual channel create */
	mrcp_engine_channel_t* (*create_channel)(mrcp_resource_engine_t *engine, apr_pool_t *pool);
};

/** MRCP resource engine */
struct mrcp_resource_engine_t {
	/** Plugin version */
	mrcp_plugin_version_t              plugin_version;
	/** Resource identifier */
	mrcp_resource_id                   resource_id;
	/** External object associated with engine */
	void                              *obj;
	/** Table of virtual methods */
	const mrcp_engine_method_vtable_t *method_vtable;
	/** Codec manager */
	const mpf_codec_manager_t         *codec_manager;
	/** Dir layout structure */
	const apt_dir_layout_t            *dir_layout;
	/** Pool to allocate memory from */
	apr_pool_t                        *pool;
};

/** Create resource engine */
mrcp_resource_engine_t* mrcp_resource_engine_create(
								mrcp_resource_id resource_id,
								void *obj, 
								const mrcp_engine_method_vtable_t *vtable,
								apr_pool_t *pool);

/** Destroy resource engine */
static APR_INLINE apt_bool_t mrcp_resource_engine_destroy(mrcp_resource_engine_t *engine)
{
	return engine->method_vtable->destroy(engine);
}

/** Open resource engine */
static APR_INLINE apt_bool_t mrcp_resource_engine_open(mrcp_resource_engine_t *engine)
{
	return engine->method_vtable->open(engine);
}

/** Close resource engine */
static APR_INLINE apt_bool_t mrcp_resource_engine_close(mrcp_resource_engine_t *engine)
{
	return engine->method_vtable->close(engine);
}

/** Create engine channel */
mrcp_engine_channel_t* mrcp_engine_channel_create(
								mrcp_resource_engine_t *engine,
								const mrcp_engine_channel_method_vtable_t *method_vtable,
								void *method_obj,
								mpf_termination_t *termination,
								apr_pool_t *pool);

/** Create engine channel and source media termination */
mrcp_engine_channel_t* mrcp_engine_source_channel_create(
								mrcp_resource_engine_t *engine,
								const mrcp_engine_channel_method_vtable_t *channel_vtable,
								const mpf_audio_stream_vtable_t *stream_vtable,
								void *method_obj,
								mpf_codec_descriptor_t *codec_descriptor,
								apr_pool_t *pool);

/** Create engine channel and sink media termination */
mrcp_engine_channel_t* mrcp_engine_sink_channel_create(
								mrcp_resource_engine_t *engine,
								const mrcp_engine_channel_method_vtable_t *channel_vtable,
								const mpf_audio_stream_vtable_t *stream_vtable,
								void *method_obj,
								mpf_codec_descriptor_t *codec_descriptor,
								apr_pool_t *pool);

/** Destroy engine channel */
static APR_INLINE apt_bool_t mrcp_engine_channel_destroy(mrcp_engine_channel_t *channel)
{
	return channel->method_vtable->destroy(channel);
}

/** Open engine channel */
static APR_INLINE apt_bool_t mrcp_engine_channel_open(mrcp_engine_channel_t *channel)
{
	return channel->method_vtable->open(channel);
}

/** Close engine channel */
static APR_INLINE apt_bool_t mrcp_engine_channel_close(mrcp_engine_channel_t *channel)
{
	return channel->method_vtable->close(channel);
}

/** Process request */
static APR_INLINE apt_bool_t mrcp_engine_channel_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *message)
{
	return channel->method_vtable->process_request(channel,message);
}

/** Send channel open response */
static APR_INLINE apt_bool_t mrcp_engine_channel_open_respond(mrcp_engine_channel_t *channel, apt_bool_t status)
{
	return channel->event_vtable->on_open(channel,status);
}

/** Send channel close response */
static APR_INLINE apt_bool_t mrcp_engine_channel_close_respond(mrcp_engine_channel_t *channel)
{
	return channel->event_vtable->on_close(channel);
}

/** Send response/event message */
static APR_INLINE apt_bool_t mrcp_engine_channel_message_send(mrcp_engine_channel_t *channel, mrcp_message_t *message)
{
	return channel->event_vtable->on_message(channel,message);
}

/** Get codec of the audio source stream */
mpf_codec_t* mrcp_engine_source_stream_codec_get(mrcp_engine_channel_t *channel);

/** Get codec of the audio sink stream */
mpf_codec_t* mrcp_engine_sink_stream_codec_get(mrcp_engine_channel_t *channel);


APT_END_EXTERN_C

#endif /*__MRCP_RESOURCE_ENGINE_H__*/
