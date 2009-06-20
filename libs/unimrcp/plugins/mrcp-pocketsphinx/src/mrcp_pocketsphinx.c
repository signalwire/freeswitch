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

/* 
 * Some mandatory rules for plugin implementation.
 * 1. Each plugin MUST contain the following function as an entry point of the plugin
 *        MRCP_PLUGIN_DECLARE(mrcp_resource_engine_t*) mrcp_plugin_create(apr_pool_t *pool)
 * 2. One and only one response MUST be sent back to the received request.
 * 3. Methods (callbacks) of the MRCP engine channel MUST not block.
 *   (asynch response can be sent from the context of other thread)
 * 4. Methods (callbacks) of the MPF engine stream MUST not block.
 */

#include "mrcp_resource_engine.h"
#include "mrcp_recog_resource.h"
#include "mrcp_recog_header.h"
#include "mrcp_generic_header.h"
#include "mrcp_message.h"
#include "mpf_activity_detector.h"
#include "apt_log.h"


typedef struct pocketsphinx_engine_t pocketsphinx_engine_t;
typedef struct pocketsphinx_recognizer_t pocketsphinx_recognizer_t;

/** Declaration of recognizer engine methods */
static apt_bool_t pocketsphinx_engine_destroy(mrcp_resource_engine_t *engine);
static apt_bool_t pocketsphinx_engine_open(mrcp_resource_engine_t *engine);
static apt_bool_t pocketsphinx_engine_close(mrcp_resource_engine_t *engine);
static mrcp_engine_channel_t* pocketsphinx_recognizer_create(mrcp_resource_engine_t *engine, apr_pool_t *pool);

static const struct mrcp_engine_method_vtable_t engine_vtable = {
	pocketsphinx_engine_destroy,
	pocketsphinx_engine_open,
	pocketsphinx_engine_close,
	pocketsphinx_recognizer_create
};


/** Declaration of recognizer channel methods */
static apt_bool_t pocketsphinx_recognizer_destroy(mrcp_engine_channel_t *channel);
static apt_bool_t pocketsphinx_recognizer_open(mrcp_engine_channel_t *channel);
static apt_bool_t pocketsphinx_recognizer_close(mrcp_engine_channel_t *channel);
static apt_bool_t pocketsphinx_recognizer_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request);

static const struct mrcp_engine_channel_method_vtable_t channel_vtable = {
	pocketsphinx_recognizer_destroy,
	pocketsphinx_recognizer_open,
	pocketsphinx_recognizer_close,
	pocketsphinx_recognizer_request_process
};

/** Declaration of recognizer audio stream methods */
static apt_bool_t pocketsphinx_stream_destroy(mpf_audio_stream_t *stream);
static apt_bool_t pocketsphinx_stream_open(mpf_audio_stream_t *stream);
static apt_bool_t pocketsphinx_stream_close(mpf_audio_stream_t *stream);
static apt_bool_t pocketsphinx_stream_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame);

static const mpf_audio_stream_vtable_t audio_stream_vtable = {
	pocketsphinx_stream_destroy,
	NULL,
	NULL,
	NULL,
	pocketsphinx_stream_open,
	pocketsphinx_stream_close,
	pocketsphinx_stream_write
};

/** Declaration of pocketsphinx engine */
struct pocketsphinx_engine_t {
	mrcp_resource_engine_t *base;
};

/** Declaration of pocketsphinx recognizer */
struct pocketsphinx_recognizer_t {
	/** Back pointer to engine */
	pocketsphinx_engine_t   *engine;
	/** Engine channel base */
	mrcp_engine_channel_t   *channel;
};

/** Declare this macro to use log routine of the server, plugin is loaded from */
MRCP_PLUGIN_LOGGER_IMPLEMENT


/** Create pocketsphinx engine (engine is an aggregation of recognizers) */
MRCP_PLUGIN_DECLARE(mrcp_resource_engine_t*) mrcp_plugin_create(apr_pool_t *pool)
{
	pocketsphinx_engine_t *engine = apr_palloc(pool,sizeof(pocketsphinx_engine_t));
	
	/* create resource engine base */
	engine->base = mrcp_resource_engine_create(
					MRCP_RECOGNIZER_RESOURCE,  /* MRCP resource identifier */
					engine,                    /* object to associate */
					&engine_vtable,            /* virtual methods table of resource engine */
					pool);                     /* pool to allocate memory from */
	return engine->base;
}

static apt_bool_t pocketsphinx_engine_destroy(mrcp_resource_engine_t *engine)
{
	return TRUE;
}

static apt_bool_t pocketsphinx_engine_open(mrcp_resource_engine_t *engine)
{
	return TRUE;
}

static apt_bool_t pocketsphinx_engine_close(mrcp_resource_engine_t *engine)
{
	return TRUE;
}

/** Create pocketsphinx recognizer */
static mrcp_engine_channel_t* pocketsphinx_recognizer_create(mrcp_resource_engine_t *engine, apr_pool_t *pool)
{
	mrcp_engine_channel_t *channel;
	pocketsphinx_recognizer_t *recognizer = apr_palloc(pool,sizeof(pocketsphinx_recognizer_t));

	/* create engine channel base */
	channel = mrcp_engine_sink_channel_create(
			engine,               /* resource engine */
			&channel_vtable,      /* virtual methods table of engine channel */
			&audio_stream_vtable, /* virtual methods table of audio stream */
			recognizer,           /* object to associate */
			NULL,                 /* codec descriptor might be NULL by default */
			pool);                /* pool to allocate memory from */
	
	recognizer->channel = channel;
	return channel;
}

/** Destroy engine channel */
static apt_bool_t pocketsphinx_recognizer_destroy(mrcp_engine_channel_t *channel)
{
	return TRUE;
}

/** Open engine channel (asynchronous response MUST be sent)*/
static apt_bool_t pocketsphinx_recognizer_open(mrcp_engine_channel_t *channel)
{
	return mrcp_engine_channel_open_respond(channel,TRUE);
}

/** Close engine channel (asynchronous response MUST be sent)*/
static apt_bool_t pocketsphinx_recognizer_close(mrcp_engine_channel_t *channel)
{
	return mrcp_engine_channel_close_respond(channel);
}


/** Process RECOGNIZE request */
static apt_bool_t pocketsphinx_recognize(pocketsphinx_recognizer_t *recognizer, mrcp_message_t *request, mrcp_message_t *response)
{
	return TRUE;
}

/** Process STOP request */
static apt_bool_t pocketsphinx_stop(pocketsphinx_recognizer_t *recognizer, mrcp_message_t *request, mrcp_message_t *response)
{
	return TRUE;
}

static apt_bool_t pocketsphinx_recognizer_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	pocketsphinx_recognizer_t *recognizer = channel->method_obj;
	apt_bool_t processed = FALSE;
	mrcp_message_t *response = mrcp_response_create(request,request->pool);
	switch(request->start_line.method_id) {
		case RECOGNIZER_SET_PARAMS:
			break;
		case RECOGNIZER_GET_PARAMS:
			break;
		case RECOGNIZER_DEFINE_GRAMMAR:
			break;
		case RECOGNIZER_RECOGNIZE:
			processed = pocketsphinx_recognize(recognizer,request,response);
			break;
		case RECOGNIZER_GET_RESULT:
			break;
		case RECOGNIZER_START_INPUT_TIMERS:
			break;
		case RECOGNIZER_STOP:
			processed = pocketsphinx_stop(recognizer,request,response);
			break;
		default:
			break;
	}
	if(processed == FALSE) {
		/* send asynchronous response for not handled request */
		mrcp_engine_channel_message_send(channel,response);
	}
	return TRUE;
}





static apt_bool_t pocketsphinx_stream_destroy(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t pocketsphinx_stream_open(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t pocketsphinx_stream_close(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t pocketsphinx_stream_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame)
{
	return TRUE;
}
