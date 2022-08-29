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

#include "mrcp_engine_impl.h"
#include "mpf_termination_factory.h"

/** Create engine */
mrcp_engine_t* mrcp_engine_create(
					mrcp_resource_id resource_id,
					void *obj, 
					const mrcp_engine_method_vtable_t *vtable,
					apr_pool_t *pool)
{
	mrcp_engine_t *engine = apr_palloc(pool,sizeof(mrcp_engine_t));
	engine->id = NULL;
	engine->resource_id = resource_id;
	engine->obj = obj;
	engine->method_vtable =vtable;
	engine->event_vtable = NULL;
	engine->event_obj = NULL;
	engine->config = NULL;
	engine->codec_manager = NULL;
	engine->dir_layout = NULL;
	engine->cur_channel_count = 0;
	engine->is_open = FALSE;
	engine->pool = pool;
	engine->create_state_machine = NULL;
	return engine;
}

/** Get engine config */
const mrcp_engine_config_t* mrcp_engine_config_get(const mrcp_engine_t *engine)
{
	return engine->config;
}

/** Get engine param by name */
const char* mrcp_engine_param_get(const mrcp_engine_t *engine, const char *name)
{
	if(!engine->config || !engine->config->params) {
		return NULL;
	}
	return apr_table_get(engine->config->params,name);
}

/** Create engine channel */
mrcp_engine_channel_t* mrcp_engine_channel_create(
							mrcp_engine_t *engine, 
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
	channel->is_open = FALSE;
	channel->pool = pool;
	channel->attribs = NULL;
	apt_string_reset(&channel->id);
	return channel;
}

/** Create audio termination */
mpf_termination_t* mrcp_engine_audio_termination_create(
								void *obj,
								const mpf_audio_stream_vtable_t *stream_vtable,
								mpf_stream_capabilities_t *capabilities,
								apr_pool_t *pool)
{
	mpf_audio_stream_t *audio_stream;
	if(!capabilities) {
		return NULL;
	}

	if(mpf_codec_capabilities_validate(&capabilities->codecs) == FALSE) {
		return NULL;
	}

	/* create audio stream */
	audio_stream = mpf_audio_stream_create(
			obj,                  /* object to associate */
			stream_vtable,        /* virtual methods table of audio stream */
			capabilities,         /* stream capabilities */
			pool);                /* pool to allocate memory from */

	if(!audio_stream) {
		return NULL;
	}

	/* create media termination */
	return mpf_raw_termination_create(
			NULL,                 /* no object to associate */
			audio_stream,         /* audio stream */
			NULL,                 /* no video stream */
			pool);                /* pool to allocate memory from */
}


/** Create engine channel and source media termination */
mrcp_engine_channel_t* mrcp_engine_source_channel_create(
							mrcp_engine_t *engine,
							const mrcp_engine_channel_method_vtable_t *channel_vtable,
							const mpf_audio_stream_vtable_t *stream_vtable,
							void *method_obj,
							mpf_codec_descriptor_t *codec_descriptor,
							apr_pool_t *pool)
{
	mpf_stream_capabilities_t *capabilities;
	mpf_audio_stream_t *audio_stream;
	mpf_termination_t *termination;

	capabilities = mpf_source_stream_capabilities_create(pool);
	if(codec_descriptor) {
		mpf_codec_capabilities_add(
						&capabilities->codecs,
						mpf_sample_rate_mask_get(codec_descriptor->sampling_rate),
						codec_descriptor->name.buf);
	}
	else {
		mpf_codec_default_capabilities_add(&capabilities->codecs);
	}

	/* create audio stream */
	audio_stream = mpf_audio_stream_create(
			method_obj,           /* object to associate */
			stream_vtable,        /* virtual methods table of audio stream */
			capabilities,         /* stream capabilities */
			pool);                /* pool to allocate memory from */

	if(!audio_stream) {
		return NULL;
	}

	audio_stream->rx_descriptor = codec_descriptor;
	
	/* create media termination */
	termination = mpf_raw_termination_create(
			NULL,                 /* no object to associate */
			audio_stream,         /* audio stream */
			NULL,                 /* no video stream */
			pool);                /* pool to allocate memory from */

	/* create engine channel base */
	return mrcp_engine_channel_create(
			engine,          /* engine */
			channel_vtable,  /* virtual methods table of engine channel */
			method_obj,      /* object to associate */
			termination,     /* media termination, used to terminate audio stream */
			pool);           /* pool to allocate memory from */
}

/** Create engine channel and sink media termination */
mrcp_engine_channel_t* mrcp_engine_sink_channel_create(
							mrcp_engine_t *engine,
							const mrcp_engine_channel_method_vtable_t *channel_vtable,
							const mpf_audio_stream_vtable_t *stream_vtable,
							void *method_obj,
							mpf_codec_descriptor_t *codec_descriptor,
							apr_pool_t *pool)
{
	mpf_stream_capabilities_t *capabilities;
	mpf_audio_stream_t *audio_stream;
	mpf_termination_t *termination;

	capabilities = mpf_sink_stream_capabilities_create(pool);
	if(codec_descriptor) {
		mpf_codec_capabilities_add(
						&capabilities->codecs,
						mpf_sample_rate_mask_get(codec_descriptor->sampling_rate),
						codec_descriptor->name.buf);
	}
	else {
		mpf_codec_default_capabilities_add(&capabilities->codecs);
	}

	/* create audio stream */
	audio_stream = mpf_audio_stream_create(
			method_obj,             /* object to associate */
			stream_vtable,          /* virtual methods table of audio stream */
			capabilities,           /* stream capabilities */
			pool);                  /* pool to allocate memory from */

	if(!audio_stream) {
		return NULL;
	}

	audio_stream->tx_descriptor = codec_descriptor;
	
	/* create media termination */
	termination = mpf_raw_termination_create(
			NULL,            /* no object to associate */
			audio_stream,    /* audio stream */
			NULL,            /* no video stream */
			pool);           /* pool to allocate memory from */

	/* create engine channel base */
	return mrcp_engine_channel_create(
			engine,          /* engine */
			channel_vtable,  /* virtual methods table of engine channel */
			method_obj,      /* object to associate */
			termination,     /* media termination, used to terminate audio stream */
			pool);           /* pool to allocate memory from */
}

/** Get codec descriptor of the audio source stream */
const mpf_codec_descriptor_t* mrcp_engine_source_stream_codec_get(const mrcp_engine_channel_t *channel)
{
	if(channel && channel->termination) {
		mpf_audio_stream_t *audio_stream = mpf_termination_audio_stream_get(channel->termination);
		if(audio_stream) {
			return audio_stream->rx_descriptor;
		}
	}
	return NULL;
}

/** Get codec descriptor of the audio sink stream */
const mpf_codec_descriptor_t* mrcp_engine_sink_stream_codec_get(const mrcp_engine_channel_t *channel)
{
	if(channel && channel->termination) {
		mpf_audio_stream_t *audio_stream = mpf_termination_audio_stream_get(channel->termination);
		if(audio_stream) {
			return audio_stream->tx_descriptor;
		}
	}
	return NULL;
}
