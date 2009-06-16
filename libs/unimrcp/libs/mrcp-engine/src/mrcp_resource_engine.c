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

#include "mrcp_resource_engine.h"
#include "mpf_codec_manager.h"

/** Create resource engine */
mrcp_resource_engine_t* mrcp_resource_engine_create(
								mrcp_resource_id resource_id,
								void *obj, 
								const mrcp_engine_method_vtable_t *vtable,
								apr_pool_t *pool)
{
	mrcp_resource_engine_t *engine = apr_palloc(pool,sizeof(mrcp_resource_engine_t));
	mrcp_plugin_version_get(&engine->plugin_version);
	engine->resource_id = resource_id;
	engine->obj = obj;
	engine->method_vtable =vtable;
	engine->codec_manager = NULL;
	engine->dir_layout = NULL;
	engine->pool = pool;
	return engine;
}

/** Create engine channel */
mrcp_engine_channel_t* mrcp_engine_channel_create(
								mrcp_resource_engine_t *engine, 
								const mrcp_engine_channel_method_vtable_t *method_vtable,
								void *method_obj,
								mpf_termination_t *termination,
								apr_pool_t *pool)
{
	mrcp_engine_channel_t *channel = apr_palloc(pool,sizeof(mrcp_engine_channel_t));
	channel->method_vtable = method_vtable;
	channel->method_obj = method_obj;
	channel->event_vtable = NULL;
	channel->event_obj = NULL;
	channel->termination = termination;
	channel->engine = engine;
	channel->pool = pool;
	return channel;
}

/** Create engine channel and source media termination */
mrcp_engine_channel_t* mrcp_engine_source_channel_create(
								mrcp_resource_engine_t *engine,
								const mrcp_engine_channel_method_vtable_t *channel_vtable,
								const mpf_audio_stream_vtable_t *stream_vtable,
								void *method_obj,
								mpf_codec_descriptor_t *codec_descriptor,
								apr_pool_t *pool)
{
	mpf_audio_stream_t *audio_stream;
	mpf_termination_t *termination;
	/* create audio stream */
	audio_stream = mpf_audio_stream_create(
			method_obj,           /* object to associate */
			stream_vtable,        /* virtual methods table of audio stream */
			STREAM_MODE_RECEIVE,  /* stream mode/direction */
			pool);                /* pool to allocate memory from */

	if(engine->codec_manager) {
		audio_stream->rx_codec = mpf_codec_manager_codec_get(engine->codec_manager,codec_descriptor,pool);
	}
	
	/* create media termination */
	termination = mpf_raw_termination_create(
			NULL,                 /* no object to associate */
			audio_stream,         /* audio stream */
			NULL,                 /* no video stream */
			pool);                /* pool to allocate memory from */

	/* create engine channel base */
	return mrcp_engine_channel_create(
			engine,          /* resource engine */
			channel_vtable,  /* virtual methods table of engine channel */
			method_obj,      /* object to associate */
			termination,     /* media termination, used to terminate audio stream */
			pool);           /* pool to allocate memory from */
}

/** Create engine channel and sink media termination */
mrcp_engine_channel_t* mrcp_engine_sink_channel_create(
								mrcp_resource_engine_t *engine,
								const mrcp_engine_channel_method_vtable_t *channel_vtable,
								const mpf_audio_stream_vtable_t *stream_vtable,
								void *method_obj,
								mpf_codec_descriptor_t *codec_descriptor,
								apr_pool_t *pool)
{
	mpf_audio_stream_t *audio_stream;
	mpf_termination_t *termination;

	/* create audio stream */
	audio_stream = mpf_audio_stream_create(
			method_obj,             /* object to associate */
			stream_vtable,          /* virtual methods table of audio stream */
			STREAM_MODE_SEND,       /* stream mode/direction */
			pool);                  /* pool to allocate memory from */
	
	if(engine->codec_manager) {
		audio_stream->tx_codec = mpf_codec_manager_codec_get(engine->codec_manager,codec_descriptor,pool);
	}
	
	/* create media termination */
	termination = mpf_raw_termination_create(
			NULL,            /* no object to associate */
			audio_stream,    /* audio stream */
			NULL,            /* no video stream */
			pool);           /* pool to allocate memory from */

	/* create engine channel base */
	return mrcp_engine_channel_create(
			engine,          /* resource engine */
			channel_vtable,  /* virtual methods table of engine channel */
			method_obj,      /* object to associate */
			termination,     /* media termination, used to terminate audio stream */
			pool);           /* pool to allocate memory from */
}

/** Get codec of the audio source stream */
mpf_codec_t* mrcp_engine_source_stream_codec_get(mrcp_engine_channel_t *channel)
{
	if(channel && channel->termination && channel->termination->audio_stream) {
		return channel->termination->audio_stream->rx_codec;
	}
	return NULL;
}

/** Get codec of the audio sink stream */
mpf_codec_t* mrcp_engine_sink_stream_codec_get(mrcp_engine_channel_t *channel)
{
	if(channel && channel->termination && channel->termination->audio_stream) {
		return channel->termination->audio_stream->tx_codec;
	}
	return NULL;
}
