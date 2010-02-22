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
 * Mandatory rules concerning plugin implementation.
 * 1. Each plugin MUST implement a plugin/engine creator function
 *    with the exact signature and name (the main entry point)
 *        MRCP_PLUGIN_DECLARE(mrcp_engine_t*) mrcp_plugin_create(apr_pool_t *pool)
 * 2. Each plugin MUST declare its version number
 *        MRCP_PLUGIN_VERSION_DECLARE
 * 3. One and only one response MUST be sent back to the received request.
 * 4. Methods (callbacks) of the MRCP engine channel MUST not block.
 *   (asynchronous response can be sent from the context of other thread)
 * 5. Methods (callbacks) of the MPF engine stream MUST not block.
 */

#include "flite_voices.h"
#include "mrcp_synth_engine.h"
#include "mpf_buffer.h"
#include "apr_time.h"
#include "apt_consumer_task.h"
#include "apt_log.h"

typedef struct flite_synth_engine_t flite_synth_engine_t;
typedef struct flite_synth_channel_t flite_synth_channel_t;

/** Declaration of synthesizer engine methods */
static apt_bool_t flite_synth_engine_destroy(mrcp_engine_t *engine);
static apt_bool_t flite_synth_engine_open(mrcp_engine_t *engine);
static apt_bool_t flite_synth_engine_close(mrcp_engine_t *engine);
static mrcp_engine_channel_t* flite_synth_engine_channel_create(mrcp_engine_t *engine, apr_pool_t *pool);

static const struct mrcp_engine_method_vtable_t engine_vtable = {
	flite_synth_engine_destroy,
	flite_synth_engine_open,
	flite_synth_engine_close,
	flite_synth_engine_channel_create
};

/** Declaration of synthesizer channel methods */
static apt_bool_t flite_synth_channel_destroy(mrcp_engine_channel_t *channel);
static apt_bool_t flite_synth_channel_open(mrcp_engine_channel_t *channel);
static apt_bool_t flite_synth_channel_close(mrcp_engine_channel_t *channel);
static apt_bool_t flite_synth_channel_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request);

/** flite channel methods for processing MRCP channel request **/
static apt_bool_t flite_synth_channel_speak(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response);
static apt_bool_t flite_synth_channel_stop(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response);
static apt_bool_t flite_synth_channel_pause(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response);
static apt_bool_t flite_synth_channel_resume(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response);

static const struct mrcp_engine_channel_method_vtable_t channel_vtable = {
	flite_synth_channel_destroy,
	flite_synth_channel_open,
	flite_synth_channel_close,
	flite_synth_channel_request_process
};

/** Declaration of synthesizer audio stream methods */
static apt_bool_t flite_synth_stream_read(mpf_audio_stream_t *stream, mpf_frame_t *frame);

static const mpf_audio_stream_vtable_t audio_stream_vtable = {
	NULL,
	NULL,
	NULL,
	flite_synth_stream_read,
	NULL,
	NULL,
	NULL
};

/** Declaration of flite synthesizer engine */
struct flite_synth_engine_t {
	/** Table of flite voices */
	flite_voices_t *voices;
	int             iChannels;
};

/** Declaration of flite synthesizer channel */
struct flite_synth_channel_t {
	flite_synth_engine_t  *flite_engine;  /* Back pointer to engine */
	mrcp_engine_channel_t *channel;		  /* Engine channel base */
	mrcp_message_t		  *speak_request; /* Active (in-progress) speak request */
	mrcp_message_t		  *speak_response;/* Pending speak response */
	mrcp_message_t        *stop_response; /* Pending stop response */
	apt_bool_t             synthesizing;  /* Is synthesizer task processing speak request */
	apt_bool_t             paused;        /* Is paused */
	mpf_buffer_t          *audio_buffer;  /* Audio buffer */
	int                    iId;           /* Synth channel simultaneous reference count */
	apr_pool_t            *pool;
	apt_task_t            *task;
	apt_task_msg_pool_t   *msg_pool;
};

/** Declaration of flite synthesizer task message */
struct flite_speak_msg_t {
	flite_synth_channel_t *channel; 
	mrcp_message_t        *request;
};

typedef struct flite_speak_msg_t flite_speak_msg_t;

/* we have a special task for the actual synthesis - 
   the task is created when a mrcp speak message is received */
static apt_bool_t flite_speak(apt_task_t *task, apt_task_msg_t *msg);
static void flite_on_start(apt_task_t *task);
static void flite_on_terminate(apt_task_t *task);

/** Declare this macro to set plugin version */
MRCP_PLUGIN_VERSION_DECLARE

/** Declare this macro to use log routine of the server where the plugin is loaded from */
MRCP_PLUGIN_LOGGER_IMPLEMENT

/** Create flite synthesizer engine */
MRCP_PLUGIN_DECLARE(mrcp_engine_t*) mrcp_plugin_create(apr_pool_t *pool)
{
	/* create flite engine */
	flite_synth_engine_t *flite_engine = (flite_synth_engine_t *) apr_palloc(pool,sizeof(flite_synth_engine_t));
	flite_engine->iChannels = 0;

	/* create engine base */
	return mrcp_engine_create(
				MRCP_SYNTHESIZER_RESOURCE, /* MRCP resource identifier */
				flite_engine,              /* object to associate */
				&engine_vtable,            /* virtual methods table of engine */
				pool);                     /* pool to allocate memory from */
}

/** Destroy synthesizer engine */
static apt_bool_t flite_synth_engine_destroy(mrcp_engine_t *engine)
{
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_engine_destroy");
	return TRUE;
}

/** Open synthesizer engine */
static apt_bool_t flite_synth_engine_open(mrcp_engine_t *engine)
{
	flite_synth_engine_t *flite_engine = (flite_synth_engine_t *) engine->obj;
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_engine_open");

	flite_init();

	flite_engine->voices = flite_voices_load(engine->pool);

	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite init success");
	return TRUE;
}

/** Close synthesizer engine */
static apt_bool_t flite_synth_engine_close(mrcp_engine_t *engine)
{
	flite_synth_engine_t *flite_engine = (flite_synth_engine_t *) engine->obj;
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_engine_close");

	flite_voices_unload(flite_engine->voices);

	return TRUE;
}

static apt_bool_t flite_synth_task_create(flite_synth_channel_t *synth_channel)
{
	apt_task_msg_pool_t *msg_pool = apt_task_msg_pool_create_dynamic( sizeof(flite_speak_msg_t),synth_channel->pool);
	apt_task_vtable_t *task_vtable = 0;
	apt_consumer_task_t *consumer_task = 0;

	/* create task/thread to run flite synthesizer in */
    consumer_task = apt_consumer_task_create(synth_channel, msg_pool, synth_channel->pool);
	if(!consumer_task) {
		apt_log(APT_LOG_MARK,APT_PRIO_ERROR, "flite_synth_channel_speak failed to create flite speak task - channel:%d", synth_channel->iId);
		return FALSE;
	}

	task_vtable = apt_consumer_task_vtable_get(consumer_task);
	if(task_vtable) {
		task_vtable->process_msg = flite_speak;
		task_vtable->on_pre_run = flite_on_start;
		task_vtable->on_post_run = flite_on_terminate;
	}
	synth_channel->msg_pool = msg_pool;
	synth_channel->task = apt_consumer_task_base_get(consumer_task);
	if(synth_channel->task) {
		apt_task_name_set(synth_channel->task,"Flite Task");
	}
	return TRUE;
}

/** Create flite synthesizer channel derived from engine channel base */
static mrcp_engine_channel_t* flite_synth_engine_channel_create(mrcp_engine_t *engine, apr_pool_t *pool)
{
	mpf_stream_capabilities_t *capabilities;
	mpf_termination_t *termination; 

	/* create flite synth channel */
	flite_synth_channel_t *synth_channel = (flite_synth_channel_t *) apr_palloc(pool,sizeof(flite_synth_channel_t));

	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_engine_channel_create");
	synth_channel->flite_engine = (flite_synth_engine_t *) engine->obj;
	synth_channel->speak_request = NULL; // no active speak request in progress
	synth_channel->speak_response = NULL;
	synth_channel->stop_response = NULL;
	synth_channel->synthesizing = FALSE;
	synth_channel->paused = FALSE;
	synth_channel->pool = pool;
	synth_channel->audio_buffer = NULL;
	synth_channel->iId = 0;
	synth_channel->task = NULL;
	synth_channel->msg_pool = NULL;
	if(flite_synth_task_create(synth_channel) != TRUE) {
		apt_log(APT_LOG_MARK, APT_PRIO_WARNING, "flite_synth_task_create failed");
		return NULL;
	}

	capabilities = mpf_source_stream_capabilities_create(pool);
	mpf_codec_capabilities_add(
			&capabilities->codecs,
			MPF_SAMPLE_RATE_8000 | MPF_SAMPLE_RATE_16000,
			"LPCM");
 
	/* create media termination */
	termination = mrcp_engine_audio_termination_create(
			synth_channel,        /* object to associate */
			&audio_stream_vtable, /* virtual methods table of audio stream */
			capabilities,         /* stream capabilities */
			pool);                /* pool to allocate memory from */

	/* create engine channel base */
	synth_channel->channel = mrcp_engine_channel_create(
 			engine,               /* engine */
 			&channel_vtable,      /* virtual methods table of engine channel */
 			synth_channel,        /* object to associate */
			termination,          /* associated media termination */
 			pool);                /* pool to allocate memory from */
 	
 	if(!synth_channel->channel) {
 		apt_log(APT_LOG_MARK, APT_PRIO_WARNING, "flite_synth_engine_channel_create failed");
 		apt_task_destroy(synth_channel->task);
 		return NULL;		
 	} 

	synth_channel->audio_buffer = mpf_buffer_create(pool);
	synth_channel->iId = ++synth_channel->flite_engine->iChannels;

	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_engine_channel_create created channel %d", synth_channel->iId);
	return synth_channel->channel;
}

/** Destroy engine channel */
static apt_bool_t flite_synth_channel_destroy(mrcp_engine_channel_t *channel)
{
	flite_synth_channel_t *synth_channel = (flite_synth_channel_t *) channel->method_obj;
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_channel_destroy - channel %d", synth_channel->iId);
	if(synth_channel->task) {
		apt_task_destroy(synth_channel->task);
		synth_channel->task = NULL;
	}
	
	synth_channel->flite_engine->iChannels--;
	return TRUE;
}

/** Open engine channel (asynchronous response MUST be sent)*/
static apt_bool_t flite_synth_channel_open(mrcp_engine_channel_t *channel)
{
	flite_synth_channel_t *synth_channel = (flite_synth_channel_t *) channel->method_obj;
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_channel_open - channel %d", synth_channel->iId);

	if(synth_channel->task) {
		if(apt_task_start(synth_channel->task) == TRUE) {
			/* async response will be sent */
			return TRUE;
		}
		else {
			apt_log(APT_LOG_MARK, APT_PRIO_WARNING, "Speak task start failed - channel %d", synth_channel->iId);
		}
	}

	return mrcp_engine_channel_open_respond(channel,FALSE);
}

/** Close engine channel (asynchronous response MUST be sent)*/
static apt_bool_t flite_synth_channel_close(mrcp_engine_channel_t *channel)
{
	flite_synth_channel_t *synth_channel = (flite_synth_channel_t *) channel->method_obj;
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_channel_close - channel %d", synth_channel->iId);

	if(synth_channel->task) {
		if(apt_task_terminate(synth_channel->task,FALSE) == TRUE) {
			/* async response will be sent */
			return TRUE;
		}
		else {
			apt_log(APT_LOG_MARK, APT_PRIO_WARNING, "Speak task terminate failed - channel %d", synth_channel->iId);
		}
	}
	return mrcp_engine_channel_close_respond(channel);
}

/** Process MRCP channel request (asynchronous response MUST be sent)*/
static apt_bool_t flite_synth_channel_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	flite_synth_channel_t *synth_channel = (flite_synth_channel_t *) channel->method_obj;
	mrcp_message_t *response = mrcp_response_create(request,request->pool);
	apt_bool_t processed = FALSE;

	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_channel_request_process - channel %d", synth_channel->iId);

	switch(request->start_line.method_id) {
		case SYNTHESIZER_SET_PARAMS:
			break;
		case SYNTHESIZER_GET_PARAMS:
			break;
		case SYNTHESIZER_SPEAK:
			processed = flite_synth_channel_speak(channel,request,response);
			break;
		case SYNTHESIZER_STOP:
			processed = flite_synth_channel_stop(channel,request,response);
			break;
		case SYNTHESIZER_PAUSE:
			processed = flite_synth_channel_pause(channel,request,response);
			break;
		case SYNTHESIZER_RESUME:
			processed = flite_synth_channel_resume(channel,request,response);
			break;
		case SYNTHESIZER_BARGE_IN_OCCURRED:
			processed = flite_synth_channel_stop(channel,request,response);
			break;
		case SYNTHESIZER_CONTROL:
			break;
		case SYNTHESIZER_DEFINE_LEXICON:
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

/** Process SPEAK request */
static apt_bool_t synth_response_construct(mrcp_message_t *response, mrcp_status_code_e status_code, mrcp_synth_completion_cause_e completion_cause)
{
	mrcp_synth_header_t *synth_header = mrcp_resource_header_prepare(response);
	if(!synth_header) {
		return FALSE;
	}

	response->start_line.status_code = status_code;
	synth_header->completion_cause = completion_cause;
	mrcp_resource_header_property_add(response,SYNTHESIZER_HEADER_COMPLETION_CAUSE);
	return TRUE;
}

/** Process SPEAK request */
static apt_bool_t flite_synth_channel_speak(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response)
{
	mrcp_generic_header_t *generic_header;
	const char *content_type = NULL;
	flite_synth_channel_t *synth_channel = (flite_synth_channel_t *) channel->method_obj;
	apt_task_msg_t *msg = 0;
	flite_speak_msg_t *flite_msg = 0;
	apt_log(APT_LOG_MARK, APT_PRIO_INFO, "flite_synth_channel_speak - channel %d", synth_channel->iId);

	generic_header = mrcp_generic_header_get(request);
	if(generic_header) {
		/* content-type must be specified */
		if(mrcp_generic_header_property_check(request,GENERIC_HEADER_CONTENT_TYPE) == TRUE) {
			content_type = generic_header->content_type.buf;
		}
	}
	if(!content_type) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Missing Content-Type");
		synth_response_construct(response,MRCP_STATUS_CODE_MISSING_PARAM,SYNTHESIZER_COMPLETION_CAUSE_ERROR);
		return FALSE;
	}

	/* Flite currently supports only text/plain (no SSML) */
	if(strstr(content_type,"text") == NULL) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Not Supported Content-Type [%s]",content_type);
		synth_response_construct(response,MRCP_STATUS_CODE_UNSUPPORTED_PARAM_VALUE,SYNTHESIZER_COMPLETION_CAUSE_ERROR);
		return FALSE;
	}

	synth_channel->speak_request = request;
	synth_channel->speak_response = response;

	msg = apt_task_msg_acquire(synth_channel->msg_pool);
	msg->type = TASK_MSG_USER;
	flite_msg = (flite_speak_msg_t*) msg->data;
	flite_msg->channel = synth_channel;
	flite_msg->request = request;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG, "Send signal to start speech synthesis - channel:%d", synth_channel->iId);
	if(apt_task_msg_signal(synth_channel->task,msg) != TRUE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING, "Failed to send signal to start speech synthesis - channel:%d", synth_channel->iId);
		synth_channel->speak_request = NULL;
		synth_channel->speak_response = NULL;
		synth_response_construct(response,MRCP_STATUS_CODE_METHOD_FAILED,SYNTHESIZER_COMPLETION_CAUSE_ERROR);
		return FALSE;
	}
	return TRUE;
}

static apt_bool_t flite_speak(apt_task_t *task, apt_task_msg_t *msg)
{
	flite_speak_msg_t *flite_msg = (flite_speak_msg_t*)msg->data;
	flite_synth_channel_t *synth_channel = (flite_synth_channel_t *) flite_msg->channel;
	cst_wave *wave = NULL;
	cst_voice *voice = NULL;
	apr_time_t start = 0;
	apr_time_t elapsed = 0;
	apr_time_t stamp = 0;
	apt_str_t *body;
	mrcp_message_t *response;

	apr_uint16_t rate = 8000;
	const mpf_codec_descriptor_t * descriptor = mrcp_engine_source_stream_codec_get(synth_channel->channel);
	if(descriptor) {
		rate = descriptor->sampling_rate;
	}
	body = &synth_channel->speak_request->body;

	response = synth_channel->speak_response;
	synth_channel->speak_response = NULL;

	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "< flite_speak_msg_process speak - channel %d", synth_channel->iId);
	
	/* just sequential stuff */
	start = apr_time_now();	/* in microsec */
	if(!body->length) {
		synth_channel->speak_request = NULL;
		synth_response_construct(response,MRCP_STATUS_CODE_MISSING_PARAM,SYNTHESIZER_COMPLETION_CAUSE_ERROR);
		mrcp_engine_channel_message_send(synth_channel->channel,response);
		return FALSE;
	}

	voice = flite_voices_best_match_get(
							synth_channel->flite_engine->voices,
							synth_channel->speak_request);
	if(!voice) {
		/* error case: no voice found, appropriate respond must be sent */
		synth_channel->speak_request = NULL;
		synth_response_construct(response,MRCP_STATUS_CODE_METHOD_FAILED,SYNTHESIZER_COMPLETION_CAUSE_ERROR);
		mrcp_engine_channel_message_send(synth_channel->channel,response);
		return FALSE;
	}

	/* 
	TODO 
	create small units of text from synth_channel->speak_request->body.buf ( , . ? ! but ...
	synthesize small unit and store in audio_buffer
	check for stop 
	pause resume state could improve performance 
	you can "pause" generating new speech from a unit of text 
	by checking the (decreasing) size of the audio_buffer
	no need to generate more speech samples than can be listened to...	
	*/

	/* send in-progress response and start synthesizing */
	response->start_line.request_state = MRCP_REQUEST_STATE_INPROGRESS;
	mrcp_engine_channel_message_send(synth_channel->channel,response);
	
	synth_channel->synthesizing = TRUE;
	wave = flite_text_to_wave(body->buf, voice);
	if(wave && cst_wave_num_samples(wave)) {
		int generated = (cst_wave_num_samples(wave)/cst_wave_sample_rate(wave)*1000);
		stamp = apr_time_now();
		elapsed = (stamp - start)/1000;
		apt_log(APT_LOG_MARK, APT_PRIO_INFO, "TTS (chan %d) took %"APR_TIME_T_FMT" to generate %d of speech (in millisec)", synth_channel->iId, elapsed, generated);

		if(rate != 16000) {
			cst_wave_resample(wave, rate);
			elapsed = (apr_time_now() - stamp)/1000;
			apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "TTS resampling to %d on (chan %d) took %"APR_TIME_T_FMT" millisec", rate, synth_channel->iId, elapsed);
		}
		mpf_buffer_audio_write(synth_channel->audio_buffer, cst_wave_samples(wave), cst_wave_num_samples(wave) * 2);
		delete_wave(wave);
	}

	/* this will notify the callback that feeds the client that synthesis is complete */
	mpf_buffer_event_write(synth_channel->audio_buffer, MEDIA_FRAME_TYPE_EVENT);
	synth_channel->synthesizing = FALSE;

	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "> flite_speak_msg_process speak - end of TTS - %d", synth_channel->iId);
	return TRUE;
}

static APR_INLINE flite_synth_channel_t* flite_synth_channel_get(apt_task_t *task)
{
	apt_consumer_task_t *consumer_task = apt_task_object_get(task);
	return apt_consumer_task_object_get(consumer_task);
}

static void flite_on_start(apt_task_t *task)
{
	flite_synth_channel_t *synth_channel = flite_synth_channel_get(task);
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "Speak task started - channel %d", synth_channel->iId);
	mrcp_engine_channel_open_respond(synth_channel->channel,TRUE);
}

static void flite_on_terminate(apt_task_t *task)
{
	flite_synth_channel_t *synth_channel = flite_synth_channel_get(task);
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "Speak task terminated - channel %d", synth_channel->iId);
	mrcp_engine_channel_close_respond(synth_channel->channel);
}

/** Process STOP request */
static apt_bool_t flite_synth_channel_stop(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response)
{
	flite_synth_channel_t *synth_channel = (flite_synth_channel_t *) channel->method_obj;
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_channel_stop - channel %d", synth_channel->iId);
	/* store the request, make sure there is no more activity and only then send the response */

	synth_channel->stop_response = response;
	return TRUE;
}

/** Process PAUSE request */
static apt_bool_t flite_synth_channel_pause(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response)
{
	flite_synth_channel_t *synth_channel = (flite_synth_channel_t *) channel->method_obj;
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_channel_pause - channel %d", synth_channel->iId);
	
	synth_channel->paused = TRUE;
	/* send asynchronous response */
	mrcp_engine_channel_message_send(channel,response);
	return TRUE;
}

/** Process RESUME request */
static apt_bool_t flite_synth_channel_resume(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response)
{
	flite_synth_channel_t *synth_channel = (flite_synth_channel_t *) channel->method_obj;
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_channel_resume - channel %d", synth_channel->iId);

	synth_channel->paused = FALSE;
	/* send asynchronous response */
	mrcp_engine_channel_message_send(channel,response);
	return TRUE;
}

/** Raise SPEAK-COMPLETE event */
static apt_bool_t flite_synth_speak_complete_raise(flite_synth_channel_t *synth_channel)
{
	mrcp_message_t *message = 0;
	mrcp_synth_header_t * synth_header = 0;
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_speak_complete_raise - channel %d", synth_channel->iId);

	if (!synth_channel->speak_request) {
		return FALSE;
	}

	message = mrcp_event_create(
						synth_channel->speak_request,
						SYNTHESIZER_SPEAK_COMPLETE,
						synth_channel->speak_request->pool);
	if (!message) {
		return FALSE;
	}

	/* get/allocate synthesizer header */
	synth_header = (mrcp_synth_header_t *) mrcp_resource_header_prepare(message);
	if (synth_header) {
		/* set completion cause */
		synth_header->completion_cause = SYNTHESIZER_COMPLETION_CAUSE_NORMAL;
		mrcp_resource_header_property_add(message,SYNTHESIZER_HEADER_COMPLETION_CAUSE);
	}
	/* set request state */
	message->start_line.request_state = MRCP_REQUEST_STATE_COMPLETE;

	synth_channel->speak_request = NULL;
	/* send asynch event */
	return mrcp_engine_channel_message_send(synth_channel->channel,message);
}

/** Callback is called from MPF engine context to read/get new frame */
static apt_bool_t flite_synth_stream_read(mpf_audio_stream_t *stream, mpf_frame_t *frame)
{
	flite_synth_channel_t *synth_channel = (flite_synth_channel_t *) stream->obj;
	if(synth_channel->stop_response && synth_channel->synthesizing == FALSE) {
		/* send asynchronous response to STOP request */
		mrcp_message_t *stop_response = synth_channel->stop_response;
		synth_channel->stop_response = NULL;
		synth_channel->speak_request = NULL;
		synth_channel->paused = FALSE;
		mrcp_engine_channel_message_send(synth_channel->channel,stop_response);
		return TRUE;
	}

	/* check if there is active SPEAK request and it isn't in paused state */
	if(synth_channel->speak_request && synth_channel->paused == FALSE) {
		/* normal processing */
		mpf_buffer_frame_read(synth_channel->audio_buffer,frame);
#if 0
		apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_stream_read - channel %d - size %d", synth_channel->iId, mpf_buffer_get_size(synth_channel->audio_buffer));
#endif

		if((frame->type & MEDIA_FRAME_TYPE_EVENT) == MEDIA_FRAME_TYPE_EVENT) {
			frame->type &= ~MEDIA_FRAME_TYPE_EVENT;
			flite_synth_speak_complete_raise(synth_channel);
		}
	}
	return TRUE;
}
