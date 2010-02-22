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

#include "mrcp_recorder_engine.h"
#include "mpf_activity_detector.h"
#include "apt_log.h"

#define RECORDER_ENGINE_TASK_NAME "Recorder Engine"

typedef struct recorder_channel_t recorder_channel_t;

/** Declaration of recorder engine methods */
static apt_bool_t recorder_engine_destroy(mrcp_engine_t *engine);
static apt_bool_t recorder_engine_open(mrcp_engine_t *engine);
static apt_bool_t recorder_engine_close(mrcp_engine_t *engine);
static mrcp_engine_channel_t* recorder_engine_channel_create(mrcp_engine_t *engine, apr_pool_t *pool);

static const struct mrcp_engine_method_vtable_t engine_vtable = {
	recorder_engine_destroy,
	recorder_engine_open,
	recorder_engine_close,
	recorder_engine_channel_create
};


/** Declaration of recorder channel methods */
static apt_bool_t recorder_channel_destroy(mrcp_engine_channel_t *channel);
static apt_bool_t recorder_channel_open(mrcp_engine_channel_t *channel);
static apt_bool_t recorder_channel_close(mrcp_engine_channel_t *channel);
static apt_bool_t recorder_channel_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request);

static const struct mrcp_engine_channel_method_vtable_t channel_vtable = {
	recorder_channel_destroy,
	recorder_channel_open,
	recorder_channel_close,
	recorder_channel_request_process
};

/** Declaration of recorder audio stream methods */
static apt_bool_t recorder_stream_destroy(mpf_audio_stream_t *stream);
static apt_bool_t recorder_stream_open(mpf_audio_stream_t *stream, mpf_codec_t *codec);
static apt_bool_t recorder_stream_close(mpf_audio_stream_t *stream);
static apt_bool_t recorder_stream_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame);

static const mpf_audio_stream_vtable_t audio_stream_vtable = {
	recorder_stream_destroy,
	NULL,
	NULL,
	NULL,
	recorder_stream_open,
	recorder_stream_close,
	recorder_stream_write
};

/** Declaration of recorder channel */
struct recorder_channel_t {
	/** Engine channel base */
	mrcp_engine_channel_t   *channel;

	/** Active (in-progress) record request */
	mrcp_message_t          *record_request;
	/** Pending stop response */
	mrcp_message_t          *stop_response;
	/** Indicates whether input timers are started */
	apt_bool_t               timers_started;
	/** Voice activity detector */
	mpf_activity_detector_t *detector;
	/** Max length of the recording in msec */
	apr_size_t               max_time;
	/** Elapsed time of the recording in msec */
	apr_size_t               cur_time;
	/** Written size of the recording in bytes */
	apr_size_t               cur_size;
	/** File name of the recording */
	const char              *file_name;
	/** File to write to */
	FILE                    *audio_out;
};


/** Declare this macro to set plugin version */
MRCP_PLUGIN_VERSION_DECLARE

/** Declare this macro to use log routine of the server, plugin is loaded from */
MRCP_PLUGIN_LOGGER_IMPLEMENT

/** Create recorder engine */
MRCP_PLUGIN_DECLARE(mrcp_engine_t*) mrcp_plugin_create(apr_pool_t *pool)
{
	/* create engine base */
	return mrcp_engine_create(
				MRCP_RECORDER_RESOURCE,    /* MRCP resource identifier */
				NULL,                      /* object to associate */
				&engine_vtable,            /* virtual methods table of engine */
				pool);                     /* pool to allocate memory from */
}

/** Destroy recorder engine */
static apt_bool_t recorder_engine_destroy(mrcp_engine_t *engine)
{
	return TRUE;
}

/** Open recorder engine */
static apt_bool_t recorder_engine_open(mrcp_engine_t *engine)
{
	return TRUE;
}

/** Close recorder engine */
static apt_bool_t recorder_engine_close(mrcp_engine_t *engine)
{
	return TRUE;
}

static mrcp_engine_channel_t* recorder_engine_channel_create(mrcp_engine_t *engine, apr_pool_t *pool)
{
	mpf_stream_capabilities_t *capabilities;
	mpf_termination_t *termination; 

	/* create recorder channel */
	recorder_channel_t *recorder_channel = apr_palloc(pool,sizeof(recorder_channel_t));
	recorder_channel->record_request = NULL;
	recorder_channel->stop_response = NULL;
	recorder_channel->detector = mpf_activity_detector_create(pool);
	recorder_channel->max_time = 0;
	recorder_channel->cur_time = 0;
	recorder_channel->cur_size = 0;
	recorder_channel->file_name = NULL;
	recorder_channel->audio_out = NULL;

	capabilities = mpf_sink_stream_capabilities_create(pool);
	mpf_codec_capabilities_add(
			&capabilities->codecs,
			MPF_SAMPLE_RATE_8000 | MPF_SAMPLE_RATE_16000,
			"LPCM");

	/* create media termination */
	termination = mrcp_engine_audio_termination_create(
			recorder_channel,     /* object to associate */
			&audio_stream_vtable, /* virtual methods table of audio stream */
			capabilities,         /* stream capabilities */
			pool);                /* pool to allocate memory from */

	/* create engine channel base */
	recorder_channel->channel = mrcp_engine_channel_create(
			engine,               /* engine */
			&channel_vtable,      /* virtual methods table of engine channel */
			recorder_channel,     /* object to associate */
			termination,          /* associated media termination */
			pool);                /* pool to allocate memory from */

	return recorder_channel->channel;
}

/** Destroy engine channel */
static apt_bool_t recorder_channel_destroy(mrcp_engine_channel_t *channel)
{
	/* nothing to destrtoy */
	return TRUE;
}

/** Open engine channel (asynchronous response MUST be sent)*/
static apt_bool_t recorder_channel_open(mrcp_engine_channel_t *channel)
{
	/* open channel and send asynch response */
	return mrcp_engine_channel_open_respond(channel,TRUE);
}

/** Close engine channel (asynchronous response MUST be sent)*/
static apt_bool_t recorder_channel_close(mrcp_engine_channel_t *channel)
{
	/* close channel, make sure there is no activity and send asynch response */
	return mrcp_engine_channel_close_respond(channel);
}

/** Open file to record */
static apt_bool_t recorder_file_open(recorder_channel_t *recorder_channel, mrcp_message_t *request)
{
	mrcp_engine_channel_t *channel = recorder_channel->channel;
	const apt_dir_layout_t *dir_layout = channel->engine->dir_layout;
	const mpf_codec_descriptor_t *descriptor = mrcp_engine_sink_stream_codec_get(channel);
	char *file_name = apr_psprintf(channel->pool,"rec-%dkHz-%s-%"MRCP_REQUEST_ID_FMT".pcm",
		descriptor ? descriptor->sampling_rate/1000 : 8,
		request->channel_id.session_id.buf,
		request->start_line.request_id);
	char *file_path = apt_datadir_filepath_get(dir_layout,file_name,channel->pool);
	if(!file_path) {
		return FALSE;
	}

	if(recorder_channel->audio_out) {
		fclose(recorder_channel->audio_out);
		recorder_channel->audio_out = NULL;
	}

	recorder_channel->audio_out = fopen(file_path,"wb");
	if(!recorder_channel->audio_out) {
		return FALSE;
	}

	recorder_channel->file_name = file_name;
	return TRUE;
}

/** Set Record-URI header field */
static apt_bool_t recorder_channel_uri_set(recorder_channel_t *recorder_channel, mrcp_message_t *message)
{
	char *record_uri;
	/* get/allocate recorder header */
	mrcp_recorder_header_t *recorder_header = mrcp_resource_header_prepare(message);
	if(!recorder_header) {
		return FALSE;
	}
	
	record_uri = apr_psprintf(
		message->pool,
		"<file://mediaserver/data/%s>;size=%"APR_SIZE_T_FMT";duration=%"APR_SIZE_T_FMT,
		recorder_channel->file_name,
		recorder_channel->cur_size,
		recorder_channel->cur_time);

	apt_string_set(&recorder_header->record_uri,record_uri);
	mrcp_resource_header_property_add(message,RECORDER_HEADER_RECORD_URI);
	return TRUE;
}

/** Process RECORD request */
static apt_bool_t recorder_channel_record(recorder_channel_t *recorder_channel, mrcp_message_t *request, mrcp_message_t *response)
{
	/* process RECORD request */
	mrcp_recorder_header_t *recorder_header;
	recorder_channel->timers_started = TRUE;

	/* get recorder header */
	recorder_header = mrcp_resource_header_get(request);
	if(recorder_header) {
		if(mrcp_resource_header_property_check(request,RECORDER_HEADER_START_INPUT_TIMERS) == TRUE) {
			recorder_channel->timers_started = recorder_header->start_input_timers;
		}
		if(mrcp_resource_header_property_check(request,RECORDER_HEADER_NO_INPUT_TIMEOUT) == TRUE) {
			mpf_activity_detector_noinput_timeout_set(recorder_channel->detector,recorder_header->no_input_timeout);
		}
		if(mrcp_resource_header_property_check(request,RECORDER_HEADER_FINAL_SILENCE) == TRUE) {
			mpf_activity_detector_silence_timeout_set(recorder_channel->detector,recorder_header->final_silence);
		}
		if(mrcp_resource_header_property_check(request,RECORDER_HEADER_MAX_TIME) == TRUE) {
			recorder_channel->max_time = recorder_header->max_time;
		}
	}

	/* open file to record */
	if(recorder_file_open(recorder_channel,request) == FALSE) {
		response->start_line.request_state = MRCP_REQUEST_STATE_COMPLETE;
		response->start_line.status_code = MRCP_STATUS_CODE_METHOD_FAILED;
		/* send asynchronous response */
		mrcp_engine_channel_message_send(recorder_channel->channel,response);
		return TRUE;
	}

	recorder_channel->cur_time = 0;
	recorder_channel->cur_size = 0;
	response->start_line.request_state = MRCP_REQUEST_STATE_INPROGRESS;
	/* send asynchronous response */
	mrcp_engine_channel_message_send(recorder_channel->channel,response);
	recorder_channel->record_request = request;
	return TRUE;
}

/** Process STOP request */
static apt_bool_t recorder_channel_stop(recorder_channel_t *recorder_channel, mrcp_message_t *request, mrcp_message_t *response)
{
	/* store STOP request, make sure there is no more activity and only then send the response */
	recorder_channel->stop_response = response;
	return TRUE;
}

/** Process START-INPUT-TIMERS request */
static apt_bool_t recorder_channel_timers_start(recorder_channel_t *recorder_channel, mrcp_message_t *request, mrcp_message_t *response)
{
	recorder_channel->timers_started = TRUE;
	return mrcp_engine_channel_message_send(recorder_channel->channel,response);
}

/** Process MRCP channel request (asynchronous response MUST be sent)*/
static apt_bool_t recorder_channel_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	apt_bool_t processed = FALSE;
	recorder_channel_t *recorder_channel = channel->method_obj;
	mrcp_message_t *response = mrcp_response_create(request,request->pool);
	switch(request->start_line.method_id) {
		case RECORDER_SET_PARAMS:
			break;
		case RECORDER_GET_PARAMS:
			break;
		case RECORDER_RECORD:
			processed = recorder_channel_record(recorder_channel,request,response);
			break;
		case RECORDER_STOP:
			processed = recorder_channel_stop(recorder_channel,request,response);
			break;
		case RECORDER_START_INPUT_TIMERS:
			processed = recorder_channel_timers_start(recorder_channel,request,response);
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

/* Raise START-OF-INPUT event */
static apt_bool_t recorder_start_of_input(recorder_channel_t *recorder_channel)
{
	/* create START-OF-INPUT event */
	mrcp_message_t *message = mrcp_event_create(
						recorder_channel->record_request,
						RECORDER_START_OF_INPUT,
						recorder_channel->record_request->pool);
	if(!message) {
		return FALSE;
	}

	/* set request state */
	message->start_line.request_state = MRCP_REQUEST_STATE_INPROGRESS;
	/* send asynch event */
	return mrcp_engine_channel_message_send(recorder_channel->channel,message);
}

/* Raise RECORD-COMPLETE event */
static apt_bool_t recorder_record_complete(recorder_channel_t *recorder_channel, mrcp_recorder_completion_cause_e cause)
{
	mrcp_recorder_header_t *recorder_header;
	/* create RECORD-COMPLETE event */
	mrcp_message_t *message = mrcp_event_create(
						recorder_channel->record_request,
						RECORDER_RECORD_COMPLETE,
						recorder_channel->record_request->pool);
	if(!message) {
		return FALSE;
	}

	if(recorder_channel->audio_out) {
		fclose(recorder_channel->audio_out);
		recorder_channel->audio_out = NULL;
	}

	/* get/allocate recorder header */
	recorder_header = mrcp_resource_header_prepare(message);
	if(recorder_header) {
		/* set completion cause */
		recorder_header->completion_cause = cause;
		mrcp_resource_header_property_add(message,RECORDER_HEADER_COMPLETION_CAUSE);
	}
	/* set record-uri */
	recorder_channel_uri_set(recorder_channel,message);
	/* set request state */
	message->start_line.request_state = MRCP_REQUEST_STATE_COMPLETE;

	recorder_channel->record_request = NULL;
	/* send asynch event */
	return mrcp_engine_channel_message_send(recorder_channel->channel,message);
}

/** Callback is called from MPF engine context to destroy any additional data associated with audio stream */
static apt_bool_t recorder_stream_destroy(mpf_audio_stream_t *stream)
{
	return TRUE;
}

/** Callback is called from MPF engine context to perform any action before open */
static apt_bool_t recorder_stream_open(mpf_audio_stream_t *stream, mpf_codec_t *codec)
{
	return TRUE;
}

/** Callback is called from MPF engine context to perform any action after close */
static apt_bool_t recorder_stream_close(mpf_audio_stream_t *stream)
{
	return TRUE;
}

/** Callback is called from MPF engine context to write/send new frame */
static apt_bool_t recorder_stream_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame)
{
	recorder_channel_t *recorder_channel = stream->obj;
	if(recorder_channel->stop_response) {
		if(recorder_channel->audio_out) {
			fclose(recorder_channel->audio_out);
			recorder_channel->audio_out = NULL;
		}
		
		if(recorder_channel->record_request){
			/* set record-uri */
			recorder_channel_uri_set(recorder_channel,recorder_channel->stop_response);
		}
		/* send asynchronous response to STOP request */
		mrcp_engine_channel_message_send(recorder_channel->channel,recorder_channel->stop_response);
		recorder_channel->stop_response = NULL;
		recorder_channel->record_request = NULL;
		return TRUE;
	}

	if(recorder_channel->record_request) {
		mpf_detector_event_e det_event = mpf_activity_detector_process(recorder_channel->detector,frame);
		switch(det_event) {
			case MPF_DETECTOR_EVENT_ACTIVITY:
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Detected Voice Activity");
				recorder_start_of_input(recorder_channel);
				break;
			case MPF_DETECTOR_EVENT_INACTIVITY:
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Detected Voice Inactivity");
				recorder_record_complete(recorder_channel,RECORDER_COMPLETION_CAUSE_SUCCESS_SILENCE);
				break;
			case MPF_DETECTOR_EVENT_NOINPUT:
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Detected Noinput");
				if(recorder_channel->timers_started == TRUE) {
					recorder_record_complete(recorder_channel,RECORDER_COMPLETION_CAUSE_NO_INPUT_TIMEOUT);
				}
				break;
			default:
				break;
		}

		if(recorder_channel->audio_out) {
			fwrite(frame->codec_frame.buffer,1,frame->codec_frame.size,recorder_channel->audio_out);
			
			recorder_channel->cur_size += frame->codec_frame.size;
			recorder_channel->cur_time += CODEC_FRAME_TIME_BASE;
			if(recorder_channel->max_time && recorder_channel->cur_time >= recorder_channel->max_time) {
				recorder_record_complete(recorder_channel,RECORDER_COMPLETION_CAUSE_SUCCESS_MAXTIME);
			}
		}
	}
	return TRUE;
}
