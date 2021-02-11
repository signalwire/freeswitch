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

#ifndef MRCP_ENGINE_IMPL_H
#define MRCP_ENGINE_IMPL_H

/**
 * @file mrcp_engine_impl.h
 * @brief MRCP Engine Realization Interface (typically should be implemented in plugins)
 */ 

#include "mrcp_engine_types.h"
#include "mpf_stream.h"

APT_BEGIN_EXTERN_C

/** Create engine */
mrcp_engine_t* mrcp_engine_create(
					mrcp_resource_id resource_id,
					void *obj, 
					const mrcp_engine_method_vtable_t *vtable,
					apr_pool_t *pool);

/** Send engine open response */
static APR_INLINE apt_bool_t mrcp_engine_open_respond(mrcp_engine_t *engine, apt_bool_t status)
{
	return engine->event_vtable->on_open(engine,status);
}

/** Send engine close response */
static APR_INLINE apt_bool_t mrcp_engine_close_respond(mrcp_engine_t *engine)
{
	return engine->event_vtable->on_close(engine);
}


/** Get engine config */
const mrcp_engine_config_t* mrcp_engine_config_get(const mrcp_engine_t *engine);

/** Get engine param by name */
const char* mrcp_engine_param_get(const mrcp_engine_t *engine, const char *name);


/** Create engine channel */
mrcp_engine_channel_t* mrcp_engine_channel_create(
					mrcp_engine_t *engine,
					const mrcp_engine_channel_method_vtable_t *method_vtable,
					void *method_obj,
					mpf_termination_t *termination,
					apr_pool_t *pool);

/** Create audio termination */
mpf_termination_t* mrcp_engine_audio_termination_create(
								void *obj,
								const mpf_audio_stream_vtable_t *stream_vtable,
								mpf_stream_capabilities_t *capabilities,
								apr_pool_t *pool);

/** Create engine channel and source media termination 
 * @deprecated @see mrcp_engine_channel_create() and mrcp_engine_audio_termination_create()
 */
mrcp_engine_channel_t* mrcp_engine_source_channel_create(
								mrcp_engine_t *engine,
								const mrcp_engine_channel_method_vtable_t *channel_vtable,
								const mpf_audio_stream_vtable_t *stream_vtable,
								void *method_obj,
								mpf_codec_descriptor_t *codec_descriptor,
								apr_pool_t *pool);

/** Create engine channel and sink media termination 
 * @deprecated @see mrcp_engine_channel_create() and mrcp_engine_audio_termination_create()
 */
mrcp_engine_channel_t* mrcp_engine_sink_channel_create(
								mrcp_engine_t *engine,
								const mrcp_engine_channel_method_vtable_t *channel_vtable,
								const mpf_audio_stream_vtable_t *stream_vtable,
								void *method_obj,
								mpf_codec_descriptor_t *codec_descriptor,
								apr_pool_t *pool);

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

/** Get channel identifier */
static APR_INLINE const char* mrcp_engine_channel_id_get(mrcp_engine_channel_t *channel)
{
	return channel->id.buf;
}

/** Get MRCP version channel is created in the scope of */
static APR_INLINE mrcp_version_e mrcp_engine_channel_version_get(const mrcp_engine_channel_t *channel)
{
	return channel->mrcp_version;
}

/** Get codec descriptor of the audio source stream */
const mpf_codec_descriptor_t* mrcp_engine_source_stream_codec_get(const mrcp_engine_channel_t *channel);

/** Get codec descriptor of the audio sink stream */
const mpf_codec_descriptor_t* mrcp_engine_sink_stream_codec_get(const mrcp_engine_channel_t *channel);


APT_END_EXTERN_C

#endif /* MRCP_ENGINE_IMPL_H */
