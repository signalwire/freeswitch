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

#include "mrcp_verifier_engine.h"
#include "mpf_activity_detector.h"
#include "apt_consumer_task.h"
#include "apt_log.h"

#define VERIFIER_ENGINE_TASK_NAME "Demo Verifier Engine"

typedef struct demo_verifier_engine_t demo_verifier_engine_t;
typedef struct demo_verifier_channel_t demo_verifier_channel_t;
typedef struct demo_verifier_msg_t demo_verifier_msg_t;

/** Declaration of verification engine methods */
static apt_bool_t demo_verifier_engine_destroy(mrcp_engine_t *engine);
static apt_bool_t demo_verifier_engine_open(mrcp_engine_t *engine);
static apt_bool_t demo_verifier_engine_close(mrcp_engine_t *engine);
static mrcp_engine_channel_t* demo_verifier_engine_channel_create(mrcp_engine_t *engine, apr_pool_t *pool);

static const struct mrcp_engine_method_vtable_t engine_vtable = {
	demo_verifier_engine_destroy,
	demo_verifier_engine_open,
	demo_verifier_engine_close,
	demo_verifier_engine_channel_create
};


/** Declaration of verification channel methods */
static apt_bool_t demo_verifier_channel_destroy(mrcp_engine_channel_t *channel);
static apt_bool_t demo_verifier_channel_open(mrcp_engine_channel_t *channel);
static apt_bool_t demo_verifier_channel_close(mrcp_engine_channel_t *channel);
static apt_bool_t demo_verifier_channel_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request);

static const struct mrcp_engine_channel_method_vtable_t channel_vtable = {
	demo_verifier_channel_destroy,
	demo_verifier_channel_open,
	demo_verifier_channel_close,
	demo_verifier_channel_request_process
};

/** Declaration of verification audio stream methods */
static apt_bool_t demo_verifier_stream_destroy(mpf_audio_stream_t *stream);
static apt_bool_t demo_verifier_stream_open(mpf_audio_stream_t *stream, mpf_codec_t *codec);
static apt_bool_t demo_verifier_stream_close(mpf_audio_stream_t *stream);
static apt_bool_t demo_verifier_stream_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame);

static const mpf_audio_stream_vtable_t audio_stream_vtable = {
	demo_verifier_stream_destroy,
	NULL,
	NULL,
	NULL,
	demo_verifier_stream_open,
	demo_verifier_stream_close,
	demo_verifier_stream_write,
	NULL
};

/** Declaration of demo verification engine */
struct demo_verifier_engine_t {
	apt_consumer_task_t    *task;
};

/** Declaration of demo verification channel */
struct demo_verifier_channel_t {
	/** Back pointer to engine */
	demo_verifier_engine_t     *demo_engine;
	/** Engine channel base */
	mrcp_engine_channel_t   *channel;

	/** Active (in-progress) verification request */
	mrcp_message_t          *verifier_request;
	/** Pending stop response */
	mrcp_message_t          *stop_response;
	/** Indicates whether input timers are started */
	apt_bool_t               timers_started;
	/** Voice activity detector */
	mpf_activity_detector_t *detector;
	/** File to write voiceprint to */
	FILE                    *audio_out;
};

typedef enum {
	DEMO_VERIF_MSG_OPEN_CHANNEL,
	DEMO_VERIF_MSG_CLOSE_CHANNEL,
	DEMO_VERIF_MSG_REQUEST_PROCESS
} demo_verifier_msg_type_e;

/** Declaration of demo verification task message */
struct demo_verifier_msg_t {
	demo_verifier_msg_type_e  type;
	mrcp_engine_channel_t *channel; 
	mrcp_message_t        *request;
};

static apt_bool_t demo_verifier_msg_signal(demo_verifier_msg_type_e type, mrcp_engine_channel_t *channel, mrcp_message_t *request);
static apt_bool_t demo_verifier_msg_process(apt_task_t *task, apt_task_msg_t *msg);

static apt_bool_t demo_verifier_result_load(demo_verifier_channel_t *verifier_channel, mrcp_message_t *message);

/** Declare this macro to set plugin version */
MRCP_PLUGIN_VERSION_DECLARE

/**
 * Declare this macro to use log routine of the server, plugin is loaded from.
 * Enable/add the corresponding entry in logger.xml to set a cutsom log source priority.
 *    <source name="VERIF-PLUGIN" priority="DEBUG" masking="NONE"/>
 */
MRCP_PLUGIN_LOG_SOURCE_IMPLEMENT(VERIF_PLUGIN,"VERIF-PLUGIN")

/** Use custom log source mark */
#define VERIF_LOG_MARK   APT_LOG_MARK_DECLARE(VERIF_PLUGIN)

/** Create demo verification engine */
MRCP_PLUGIN_DECLARE(mrcp_engine_t*) mrcp_plugin_create(apr_pool_t *pool)
{
	demo_verifier_engine_t *demo_engine = apr_palloc(pool,sizeof(demo_verifier_engine_t));
	apt_task_t *task;
	apt_task_vtable_t *vtable;
	apt_task_msg_pool_t *msg_pool;

	msg_pool = apt_task_msg_pool_create_dynamic(sizeof(demo_verifier_msg_t),pool);
	demo_engine->task = apt_consumer_task_create(demo_engine,msg_pool,pool);
	if(!demo_engine->task) {
		return NULL;
	}
	task = apt_consumer_task_base_get(demo_engine->task);
	apt_task_name_set(task,VERIFIER_ENGINE_TASK_NAME);
	vtable = apt_task_vtable_get(task);
	if(vtable) {
		vtable->process_msg = demo_verifier_msg_process;
	}

	/* create engine base */
	return mrcp_engine_create(
				MRCP_VERIFIER_RESOURCE,    /* MRCP resource identifier */
				demo_engine,               /* object to associate */
				&engine_vtable,            /* virtual methods table of engine */
				pool);                     /* pool to allocate memory from */
}

/** Destroy verification engine */
static apt_bool_t demo_verifier_engine_destroy(mrcp_engine_t *engine)
{
	demo_verifier_engine_t *demo_engine = engine->obj;
	if(demo_engine->task) {
		apt_task_t *task = apt_consumer_task_base_get(demo_engine->task);
		apt_task_destroy(task);
		demo_engine->task = NULL;
	}
	return TRUE;
}

/** Open verification engine */
static apt_bool_t demo_verifier_engine_open(mrcp_engine_t *engine)
{
	demo_verifier_engine_t *demo_engine = engine->obj;
	if(demo_engine->task) {
		apt_task_t *task = apt_consumer_task_base_get(demo_engine->task);
		apt_task_start(task);
	}
	return mrcp_engine_open_respond(engine,TRUE);
}

/** Close verification engine */
static apt_bool_t demo_verifier_engine_close(mrcp_engine_t *engine)
{
	demo_verifier_engine_t *demo_engine = engine->obj;
	if(demo_engine->task) {
		apt_task_t *task = apt_consumer_task_base_get(demo_engine->task);
		apt_task_terminate(task,TRUE);
	}
	return mrcp_engine_close_respond(engine);
}

static mrcp_engine_channel_t* demo_verifier_engine_channel_create(mrcp_engine_t *engine, apr_pool_t *pool)
{
	mpf_stream_capabilities_t *capabilities;
	mpf_termination_t *termination; 

	/* create demo verification channel */
	demo_verifier_channel_t *verifier_channel = apr_palloc(pool,sizeof(demo_verifier_channel_t));
	verifier_channel->demo_engine = engine->obj;
	verifier_channel->verifier_request = NULL;
	verifier_channel->stop_response = NULL;
	verifier_channel->detector = mpf_activity_detector_create(pool);
	verifier_channel->audio_out = NULL;

	capabilities = mpf_sink_stream_capabilities_create(pool);
	mpf_codec_capabilities_add(
			&capabilities->codecs,
			MPF_SAMPLE_RATE_8000 | MPF_SAMPLE_RATE_16000,
			"LPCM");

	/* create media termination */
	termination = mrcp_engine_audio_termination_create(
			verifier_channel,     /* object to associate */
			&audio_stream_vtable, /* virtual methods table of audio stream */
			capabilities,         /* stream capabilities */
			pool);                /* pool to allocate memory from */

	/* create engine channel base */
	verifier_channel->channel = mrcp_engine_channel_create(
			engine,               /* engine */
			&channel_vtable,      /* virtual methods table of engine channel */
			verifier_channel,        /* object to associate */
			termination,          /* associated media termination */
			pool);                /* pool to allocate memory from */

	return verifier_channel->channel;
}

/** Destroy engine channel */
static apt_bool_t demo_verifier_channel_destroy(mrcp_engine_channel_t *channel)
{
	/* nothing to destrtoy */
	return TRUE;
}

/** Open engine channel (asynchronous response MUST be sent)*/
static apt_bool_t demo_verifier_channel_open(mrcp_engine_channel_t *channel)
{
	return demo_verifier_msg_signal(DEMO_VERIF_MSG_OPEN_CHANNEL,channel,NULL);
}

/** Close engine channel (asynchronous response MUST be sent)*/
static apt_bool_t demo_verifier_channel_close(mrcp_engine_channel_t *channel)
{
	return demo_verifier_msg_signal(DEMO_VERIF_MSG_CLOSE_CHANNEL,channel,NULL);
}

/** Process MRCP channel request (asynchronous response MUST be sent)*/
static apt_bool_t demo_verifier_channel_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	return demo_verifier_msg_signal(DEMO_VERIF_MSG_REQUEST_PROCESS,channel,request);
}

/** Process VERIFY request */
static apt_bool_t demo_verifier_channel_verify(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response)
{
	/* process verify request */
	mrcp_verifier_header_t *verifier_header;
	demo_verifier_channel_t *verifier_channel = channel->method_obj;
	const mpf_codec_descriptor_t *descriptor = mrcp_engine_sink_stream_codec_get(channel);

	if(!descriptor) {
		apt_log(VERIF_LOG_MARK,APT_PRIO_WARNING,"Failed to Get Codec Descriptor " APT_SIDRES_FMT, MRCP_MESSAGE_SIDRES(request));
		response->start_line.status_code = MRCP_STATUS_CODE_METHOD_FAILED;
		return FALSE;
	}

	verifier_channel->timers_started = TRUE;

	/* get verifier header */
	verifier_header = mrcp_resource_header_get(request);
	if(verifier_header) {
		if(mrcp_resource_header_property_check(request,VERIFIER_HEADER_START_INPUT_TIMERS) == TRUE) {
			verifier_channel->timers_started = verifier_header->start_input_timers;
		}
		if(mrcp_resource_header_property_check(request,VERIFIER_HEADER_NO_INPUT_TIMEOUT) == TRUE) {
			mpf_activity_detector_noinput_timeout_set(verifier_channel->detector,verifier_header->no_input_timeout);
		}
		if(mrcp_resource_header_property_check(request,VERIFIER_HEADER_SPEECH_COMPLETE_TIMEOUT) == TRUE) {
			mpf_activity_detector_silence_timeout_set(verifier_channel->detector,verifier_header->speech_complete_timeout);
		}
	}

	if(!verifier_channel->audio_out) {
		const apt_dir_layout_t *dir_layout = channel->engine->dir_layout;
		char *file_name = apr_psprintf(channel->pool,"voiceprint-%dkHz-%s.pcm",
							descriptor->sampling_rate/1000,
							request->channel_id.session_id.buf);
		char *file_path = apt_vardir_filepath_get(dir_layout,file_name,channel->pool);
		if(file_path) {
			apt_log(VERIF_LOG_MARK,APT_PRIO_INFO,"Open Utterance Output File [%s] for Writing",file_path);
			verifier_channel->audio_out = fopen(file_path,"wb");
			if(!verifier_channel->audio_out) {
				apt_log(VERIF_LOG_MARK,APT_PRIO_WARNING,"Failed to Open Utterance Output File [%s] for Writing",file_path);
			}
		}
	}

	response->start_line.request_state = MRCP_REQUEST_STATE_INPROGRESS;
	/* send asynchronous response */
	mrcp_engine_channel_message_send(channel,response);
	verifier_channel->verifier_request = request;
	return TRUE;
}

/** Process STOP request */
static apt_bool_t demo_verifier_channel_stop(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response)
{
	/* process STOP request */
	demo_verifier_channel_t *verifier_channel = channel->method_obj;
	/* store STOP request, make sure there is no more activity and only then send the response */
	verifier_channel->stop_response = response;
	return TRUE;
}

/** Process START-INPUT-TIMERS request */
static apt_bool_t demo_verifier_channel_timers_start(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response)
{
	demo_verifier_channel_t *verifier_channel = channel->method_obj;
	verifier_channel->timers_started = TRUE;
	return mrcp_engine_channel_message_send(channel,response);
}

/** Process GET-INTERMEDIATE-RESULT request */
static apt_bool_t demo_verifier_channel_get_result(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response)
{
	demo_verifier_channel_t *verifier_channel = channel->method_obj;
	demo_verifier_result_load(verifier_channel,response);
	return mrcp_engine_channel_message_send(channel,response);
}


/** Dispatch MRCP request */
static apt_bool_t demo_verifier_channel_request_dispatch(mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	apt_bool_t processed = FALSE;
	mrcp_message_t *response = mrcp_response_create(request,request->pool);
	switch(request->start_line.method_id) {
		case VERIFIER_SET_PARAMS:
			break;
		case VERIFIER_GET_PARAMS:
			break;
		case VERIFIER_START_SESSION:
			break;
		case VERIFIER_END_SESSION:
			break;
		case VERIFIER_QUERY_VOICEPRINT:
			break;
		case VERIFIER_DELETE_VOICEPRINT:
			break;
		case VERIFIER_VERIFY:
			processed = demo_verifier_channel_verify(channel,request,response);
			break;
		case VERIFIER_VERIFY_FROM_BUFFER:
			break;
		case VERIFIER_VERIFY_ROLLBACK:
			break;
		case VERIFIER_STOP:
			processed = demo_verifier_channel_stop(channel,request,response);
			break;
		case VERIFIER_CLEAR_BUFFER:
			break;
		case VERIFIER_START_INPUT_TIMERS:
			processed = demo_verifier_channel_timers_start(channel,request,response);
			break;
		case VERIFIER_GET_INTERMIDIATE_RESULT:
			processed = demo_verifier_channel_get_result(channel,request,response);
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

/** Callback is called from MPF engine context to destroy any additional data associated with audio stream */
static apt_bool_t demo_verifier_stream_destroy(mpf_audio_stream_t *stream)
{
	return TRUE;
}

/** Callback is called from MPF engine context to perform any action before open */
static apt_bool_t demo_verifier_stream_open(mpf_audio_stream_t *stream, mpf_codec_t *codec)
{
	return TRUE;
}

/** Callback is called from MPF engine context to perform any action after close */
static apt_bool_t demo_verifier_stream_close(mpf_audio_stream_t *stream)
{
	return TRUE;
}

/* Raise demo START-OF-INPUT event */
static apt_bool_t demo_verifier_start_of_input(demo_verifier_channel_t *verifier_channel)
{
	/* create START-OF-INPUT event */
	mrcp_message_t *message = mrcp_event_create(
						verifier_channel->verifier_request,
						VERIFIER_START_OF_INPUT,
						verifier_channel->verifier_request->pool);
	if(!message) {
		return FALSE;
	}

	/* set request state */
	message->start_line.request_state = MRCP_REQUEST_STATE_INPROGRESS;
	/* send asynch event */
	return mrcp_engine_channel_message_send(verifier_channel->channel,message);
}

/* Load demo verification result */
static apt_bool_t demo_verifier_result_load(demo_verifier_channel_t *verifier_channel, mrcp_message_t *message)
{
	FILE *file;
	mrcp_engine_channel_t *channel = verifier_channel->channel;
	const apt_dir_layout_t *dir_layout = channel->engine->dir_layout;
	char *file_path = apt_datadir_filepath_get(dir_layout,"result-verification.xml",message->pool);
	if(!file_path) {
		return FALSE;
	}
	
	/* read the demo result from file */
	file = fopen(file_path,"r");
	if(file) {
		mrcp_generic_header_t *generic_header;
		char text[1024];
		apr_size_t size;
		size = fread(text,1,sizeof(text),file);
		apt_string_assign_n(&message->body,text,size,message->pool);
		fclose(file);

		/* get/allocate generic header */
		generic_header = mrcp_generic_header_prepare(message);
		if(generic_header) {
			/* set content types */
			apt_string_assign(&generic_header->content_type,"application/nlsml+xml",message->pool);
			mrcp_generic_header_property_add(message,GENERIC_HEADER_CONTENT_TYPE);
		}
	}
	return TRUE;
}

/* Raise demo VERIFICATION-COMPLETE event */
static apt_bool_t demo_verifier_verification_complete(demo_verifier_channel_t *verifier_channel, mrcp_verifier_completion_cause_e cause)
{
	mrcp_verifier_header_t *verifier_header;
	/* create VERIFICATION-COMPLETE event */
	mrcp_message_t *message = mrcp_event_create(
						verifier_channel->verifier_request,
						VERIFIER_VERIFICATION_COMPLETE,
						verifier_channel->verifier_request->pool);
	if(!message) {
		return FALSE;
	}

	/* get/allocate verifier header */
	verifier_header = mrcp_resource_header_prepare(message);
	if(verifier_header) {
		/* set completion cause */
		verifier_header->completion_cause = cause;
		mrcp_resource_header_property_add(message,VERIFIER_HEADER_COMPLETION_CAUSE);
	}
	/* set request state */
	message->start_line.request_state = MRCP_REQUEST_STATE_COMPLETE;

	if(cause == VERIFIER_COMPLETION_CAUSE_SUCCESS) {
		demo_verifier_result_load(verifier_channel,message);
	}

	verifier_channel->verifier_request = NULL;
	/* send asynch event */
	return mrcp_engine_channel_message_send(verifier_channel->channel,message);
}

/** Callback is called from MPF engine context to write/send new frame */
static apt_bool_t demo_verifier_stream_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame)
{
	demo_verifier_channel_t *verifier_channel = stream->obj;
	if(verifier_channel->stop_response) {
		/* send asynchronous response to STOP request */
		mrcp_engine_channel_message_send(verifier_channel->channel,verifier_channel->stop_response);
		verifier_channel->stop_response = NULL;
		verifier_channel->verifier_request = NULL;
		return TRUE;
	}

	if(verifier_channel->verifier_request) {
		mpf_detector_event_e det_event = mpf_activity_detector_process(verifier_channel->detector,frame);
		switch(det_event) {
			case MPF_DETECTOR_EVENT_ACTIVITY:
				apt_log(VERIF_LOG_MARK,APT_PRIO_INFO,"Detected Voice Activity " APT_SIDRES_FMT,
					MRCP_MESSAGE_SIDRES(verifier_channel->verifier_request));
				demo_verifier_start_of_input(verifier_channel);
				break;
			case MPF_DETECTOR_EVENT_INACTIVITY:
				apt_log(VERIF_LOG_MARK,APT_PRIO_INFO,"Detected Voice Inactivity " APT_SIDRES_FMT,
					MRCP_MESSAGE_SIDRES(verifier_channel->verifier_request));
				demo_verifier_verification_complete(verifier_channel,VERIFIER_COMPLETION_CAUSE_SUCCESS);
				break;
			case MPF_DETECTOR_EVENT_NOINPUT:
				apt_log(VERIF_LOG_MARK,APT_PRIO_INFO,"Detected Noinput " APT_SIDRES_FMT,
					MRCP_MESSAGE_SIDRES(verifier_channel->verifier_request));
				if(verifier_channel->timers_started == TRUE) {
					demo_verifier_verification_complete(verifier_channel,VERIFIER_COMPLETION_CAUSE_NO_INPUT_TIMEOUT);
				}
				break;
			default:
				break;
		}

		if(verifier_channel->verifier_request) {
			if((frame->type & MEDIA_FRAME_TYPE_EVENT) == MEDIA_FRAME_TYPE_EVENT) {
				if(frame->marker == MPF_MARKER_START_OF_EVENT) {
					apt_log(VERIF_LOG_MARK,APT_PRIO_INFO,"Detected Start of Event " APT_SIDRES_FMT " id:%d",
						MRCP_MESSAGE_SIDRES(verifier_channel->verifier_request),
						frame->event_frame.event_id);
				}
				else if(frame->marker == MPF_MARKER_END_OF_EVENT) {
					apt_log(VERIF_LOG_MARK,APT_PRIO_INFO,"Detected End of Event " APT_SIDRES_FMT " id:%d duration:%d ts",
						MRCP_MESSAGE_SIDRES(verifier_channel->verifier_request),
						frame->event_frame.event_id,
						frame->event_frame.duration);
				}
			}
		}

		if(verifier_channel->audio_out) {
			fwrite(frame->codec_frame.buffer,1,frame->codec_frame.size,verifier_channel->audio_out);
		}
	}
	return TRUE;
}

static apt_bool_t demo_verifier_msg_signal(demo_verifier_msg_type_e type, mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	apt_bool_t status = FALSE;
	demo_verifier_channel_t *demo_channel = channel->method_obj;
	demo_verifier_engine_t *demo_engine = demo_channel->demo_engine;
	apt_task_t *task = apt_consumer_task_base_get(demo_engine->task);
	apt_task_msg_t *msg = apt_task_msg_get(task);
	if(msg) {
		demo_verifier_msg_t *demo_msg;
		msg->type = TASK_MSG_USER;
		demo_msg = (demo_verifier_msg_t*) msg->data;

		demo_msg->type = type;
		demo_msg->channel = channel;
		demo_msg->request = request;
		status = apt_task_msg_signal(task,msg);
	}
	return status;
}

static apt_bool_t demo_verifier_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	demo_verifier_msg_t *demo_msg = (demo_verifier_msg_t*)msg->data;
	switch(demo_msg->type) {
		case DEMO_VERIF_MSG_OPEN_CHANNEL:
			/* open channel and send asynch response */
			mrcp_engine_channel_open_respond(demo_msg->channel,TRUE);
			break;
		case DEMO_VERIF_MSG_CLOSE_CHANNEL:
		{
			/* close channel, make sure there is no activity and send asynch response */
			demo_verifier_channel_t *verifier_channel = demo_msg->channel->method_obj;
			if(verifier_channel->audio_out) {
				fclose(verifier_channel->audio_out);
				verifier_channel->audio_out = NULL;
			}

			mrcp_engine_channel_close_respond(demo_msg->channel);
			break;
		}
		case DEMO_VERIF_MSG_REQUEST_PROCESS:
			demo_verifier_channel_request_dispatch(demo_msg->channel,demo_msg->request);
			break;
		default:
			break;
	}
	return TRUE;
}
