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
#include "mrcp_synth_resource.h"
#include "mrcp_synth_header.h"
#include "mrcp_generic_header.h"
#include "mrcp_message.h"
#include "mpf_buffer.h"
#include "apr_time.h"
#include "apt_consumer_task.h"
#include "apt_log.h"
#include "flite.h"

typedef struct flite_synth_engine_t flite_synth_engine_t;
typedef struct flite_synth_channel_t flite_synth_channel_t;

/** Declaration of synthesizer engine methods */
static apt_bool_t flite_synth_engine_destroy(mrcp_resource_engine_t *engine);
static apt_bool_t flite_synth_engine_open(mrcp_resource_engine_t *engine);
static apt_bool_t flite_synth_engine_close(mrcp_resource_engine_t *engine);
static mrcp_engine_channel_t* flite_synth_engine_channel_create(mrcp_resource_engine_t *engine, apr_pool_t *pool);

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

static apt_bool_t flite_synth_channel_close_t(mrcp_engine_channel_t *channel); // wait for speak thread

/** flite channel methods for processing MRCP channel request **/
static apt_bool_t flite_synth_channel_speak(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response);
static apt_bool_t flite_synth_channel_stop(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response);
static apt_bool_t flite_synth_channel_pause(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response);
static apt_bool_t flite_synth_channel_resume(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response);
// static apt_bool_t flite_synth_channel_setparams(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response);
// static apt_bool_t flite_synth_channel_getparams(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response);

static const struct mrcp_engine_channel_method_vtable_t channel_vtable = {
	flite_synth_channel_destroy,
	flite_synth_channel_open,
	flite_synth_channel_close,
	flite_synth_channel_request_process
};

/** Declaration of synthesizer audio stream methods */
static apt_bool_t flite_synth_stream_destroy(mpf_audio_stream_t *stream);
static apt_bool_t flite_synth_stream_open(mpf_audio_stream_t *stream);
static apt_bool_t flite_synth_stream_close(mpf_audio_stream_t *stream);
static apt_bool_t flite_synth_stream_read(mpf_audio_stream_t *stream, mpf_frame_t *frame);

static const mpf_audio_stream_vtable_t audio_stream_vtable = {
	flite_synth_stream_destroy,
	flite_synth_stream_open,
	flite_synth_stream_close,
	flite_synth_stream_read,
	NULL,
	NULL,
	NULL
};

/** Declaration of flite synthesizer engine */
struct flite_synth_engine_t {
	int					 iChannels;
	struct {
		cst_voice *awb;
		cst_voice *kal;
		cst_voice *rms;
		cst_voice *slt;
	} voices;
};

/** declarations for flite voices **/
cst_voice *register_cmu_us_awb(void);
cst_voice *register_cmu_us_kal(void);
cst_voice *register_cmu_us_rms(void);
cst_voice *register_cmu_us_slt(void);
void unregister_cmu_us_awb(cst_voice * v);
void unregister_cmu_us_kal(cst_voice * v);
void unregister_cmu_us_rms(cst_voice * v);
void unregister_cmu_us_slt(cst_voice * v);


/** Declaration of flite synthesizer channel */
struct flite_synth_channel_t {
	flite_synth_engine_t    *flite_engine;	// Back pointer to engine 
	mrcp_engine_channel_t	*channel;		// Engine channel base 
	mrcp_message_t			*speak_request;	// Active (in-progress) speak request
	mrcp_message_t			*stop_response;	// Pending stop response
	apt_bool_t				 paused;		// Is paused
	mpf_buffer_t			*audio_buffer;	// Audio buffer
	int						 iId;			// Synth channel simultaneous reference count
	cst_voice				*voice;
	apr_pool_t				*pool;
	apt_consumer_task_t     *task;
	apr_thread_mutex_t	    *channel_guard;
};

/** Declaration of flite synthesizer task message */
struct flite_speak_msg_t {
	flite_synth_channel_t *channel; 
	mrcp_message_t        *request;
};

typedef struct flite_speak_msg_t flite_speak_msg_t;

// we have a special task for the actual synthesis - 
// the task is created when a mrcp speak message is received
static apt_bool_t flite_speak(apt_task_t *task, apt_task_msg_t *msg);

/** Declare this macro to use log routine of the server where the plugin is loaded from */
MRCP_PLUGIN_LOGGER_IMPLEMENT

/** Create flite synthesizer engine */
MRCP_PLUGIN_DECLARE(mrcp_resource_engine_t*) mrcp_plugin_create(apr_pool_t *pool)
{
	/* create flite engine */
	flite_synth_engine_t *flite_engine = (flite_synth_engine_t *) apr_palloc(pool,sizeof(flite_synth_engine_t));

	flite_engine->iChannels = 0;

	/* create resource engine base */
	return mrcp_resource_engine_create(
					MRCP_SYNTHESIZER_RESOURCE, /* MRCP resource identifier */
					flite_engine,              /* object to associate */
					&engine_vtable,            /* virtual methods table of resource engine */
					pool);                     /* pool to allocate memory from */
}

/** Destroy synthesizer engine */
static apt_bool_t flite_synth_engine_destroy(mrcp_resource_engine_t *engine)
{
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_engine_destroy");
	return TRUE;
}

/** Open synthesizer engine */
static apt_bool_t flite_synth_engine_open(mrcp_resource_engine_t *engine)
{
	flite_synth_engine_t *flite_engine = (flite_synth_engine_t *) engine->obj;
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_engine_open");

	flite_init();
	flite_engine->voices.awb = register_cmu_us_awb();
	flite_engine->voices.kal = register_cmu_us_kal();
	flite_engine->voices.rms = register_cmu_us_rms();
	flite_engine->voices.slt = register_cmu_us_slt();

	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite init success");

	return TRUE;
}

/** Close synthesizer engine */
static apt_bool_t flite_synth_engine_close(mrcp_resource_engine_t *engine)
{
	flite_synth_engine_t *flite_engine = (flite_synth_engine_t *) engine->obj;
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_engine_close");

	unregister_cmu_us_awb(flite_engine->voices.awb);
	unregister_cmu_us_kal(flite_engine->voices.kal);
	unregister_cmu_us_rms(flite_engine->voices.rms);
	unregister_cmu_us_slt(flite_engine->voices.slt);

	return TRUE;
}

/** Create flite synthesizer channel derived from engine channel base */
static mrcp_engine_channel_t* flite_synth_engine_channel_create(mrcp_resource_engine_t *engine, apr_pool_t *pool)
{
	/* create flite synth channel */
	flite_synth_channel_t *synth_channel = (flite_synth_channel_t *) apr_palloc(pool,sizeof(flite_synth_channel_t));
	mpf_codec_descriptor_t *codec_descriptor = NULL;

	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_engine_channel_create");

//	codec_descriptor = (mpf_codec_descriptor_t *) apr_palloc(pool,sizeof(mpf_codec_descriptor_t));
//	mpf_codec_descriptor_init(codec_descriptor);
//	codec_descriptor->channel_count = 1;
//	codec_descriptor->payload_type = 96;
//	apt_string_set(&codec_descriptor->name,"LPCM");
//	codec_descriptor->sampling_rate = 16000;

	synth_channel->flite_engine = (flite_synth_engine_t *) engine->obj;
	synth_channel->speak_request = NULL; // no active speak request in progress
	synth_channel->stop_response = NULL;
	synth_channel->paused = FALSE;
	synth_channel->pool = pool;
	synth_channel->audio_buffer = NULL;
	synth_channel->voice = NULL;
	synth_channel->iId = 0;

	if (apr_thread_mutex_create(&synth_channel->channel_guard,APR_THREAD_MUTEX_DEFAULT,pool) != APR_SUCCESS) 
	{
		apt_log(APT_LOG_MARK, APT_PRIO_ERROR, "Failed to create channel guard");
		return NULL;
	}


	/* create engine channel base */
	synth_channel->channel = mrcp_engine_source_channel_create(
			engine,               /* resource engine */
			&channel_vtable,      /* virtual methods table of engine channel */
			&audio_stream_vtable, /* virtual methods table of audio stream */
			synth_channel,        /* object to associate */
			codec_descriptor,     /* codec descriptor might be NULL by default */
			pool);                /* pool to allocate memory from */
	
	if (!synth_channel->channel) 
	{
		apt_log(APT_LOG_MARK, APT_PRIO_WARNING, "flite_synth_engine_channel_create failed");
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
	if(synth_channel->task)
	{
		apt_task_t *task = apt_consumer_task_base_get(synth_channel->task);
		if (!task || !apt_task_destroy(task))
		{
			apt_log(APT_LOG_MARK, APT_PRIO_WARNING, "Speak task destroy failed - channel %d", synth_channel->iId);
		}
		else
		{
			apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "Speak task destroyed - channel %d", synth_channel->iId);
		}
	}
	
	synth_channel->task = NULL;
	synth_channel->flite_engine->iChannels--;

	apr_thread_mutex_destroy(synth_channel->channel_guard);

	return TRUE;
}

/** Open engine channel (asynchronous response MUST be sent)*/
static apt_bool_t flite_synth_channel_open(mrcp_engine_channel_t *channel)
{
	flite_synth_channel_t *synth_channel = (flite_synth_channel_t *) channel->method_obj;
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_channel_open - channel %d", synth_channel->iId);

	synth_channel->voice = synth_channel->flite_engine->voices.awb;
	return mrcp_engine_channel_open_respond(channel,TRUE);
}

/** Close engine channel (asynchronous response MUST be sent)*/
static apt_bool_t flite_synth_channel_close(mrcp_engine_channel_t *channel)
{
	flite_synth_channel_t *synth_channel = (flite_synth_channel_t *) channel->method_obj;
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_channel_close - channel %d", synth_channel->iId);
	// create thread to wait for speak thread to terminate
	flite_synth_channel_close_t(channel);
	return TRUE; 
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
				// TODO set voices
				//	if (!strcasecmp(voice_name, "awb")) {
				//		synth_channel->voice = voices.awb;
				//	} else if (!strcasecmp(voice_name, "kal")) {
				//		synth_channel->voice = voices.kal;
				//	} else if (!strcasecmp(voice_name, "rms")) {
				//		synth_channel->voice = voices.rms;
				//	} else if (!strcasecmp(voice_name, "slt")) {
				//		synth_channel->voice = voices.slt;
				//	} else {
				//		apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "Valid voice names are awb, kal, rms or slt");
				//	}
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

static apt_bool_t flite_synth_channel_close_t(mrcp_engine_channel_t *channel)
{
	flite_synth_channel_t *synth_channel = (flite_synth_channel_t *) channel->method_obj;
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_channel_close_t - channel %d", synth_channel->iId);
	
	if (synth_channel->task) 
	{
		apt_task_t *task = apt_consumer_task_base_get(synth_channel->task);
		if (!apt_task_terminate(task,TRUE))
		{
			apt_log(APT_LOG_MARK, APT_PRIO_WARNING, "Speak task terminate failed - channel %d", synth_channel->iId);
		}
		else
		{
			apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "Speak task terminated - channel %d", synth_channel->iId);
			apt_task_destroy(task);
			synth_channel->task = 0;
		}
	}
	mrcp_engine_channel_close_respond(channel);
	return TRUE;
}


/** Process SPEAK request */
static apt_bool_t flite_synth_channel_speak(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response)
{
	flite_synth_channel_t *synth_channel = (flite_synth_channel_t *) channel->method_obj;
	apt_log(APT_LOG_MARK, APT_PRIO_INFO, "flite_synth_channel_speak - channel %d", synth_channel->iId);

	if (!synth_channel->speak_request)
	{
		apt_task_msg_pool_t *msg_pool = apt_task_msg_pool_create_dynamic( sizeof(flite_speak_msg_t),synth_channel->pool);
		apt_task_vtable_t	*task_vtable = 0;
		apt_task_t * task = 0;
		apt_task_msg_t *msg = 0;
		flite_speak_msg_t *flite_msg = 0;

		/* create task/thread to run flite so this function is not blocking */
		apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "Create flite speak task - channel: %d", synth_channel->iId);
	    synth_channel->task = apt_consumer_task_create(synth_channel, msg_pool, synth_channel->pool);
		if (!synth_channel->task) 
		{
			apt_log(APT_LOG_MARK,APT_PRIO_ERROR, "flite_synth_channel_speak failed to create flite speak task - channel:%d", synth_channel->iId);
			return FALSE;
		}

		task_vtable = apt_consumer_task_vtable_get(synth_channel->task);
		if (!task_vtable) 
		{
			apt_log(APT_LOG_MARK,APT_PRIO_ERROR, "flite_synth_channel_speak cannot use flite speak task vtable - channel:%d", synth_channel->iId);
			return FALSE;
		}

		task_vtable->process_msg = flite_speak;
		synth_channel->speak_request = request;
		
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG, "Start task - channel %d", synth_channel->iId);
		task = apt_consumer_task_base_get(synth_channel->task);
		if (apt_task_start(task) == FALSE) 
		{
			apt_log(APT_LOG_MARK,APT_PRIO_ERROR, "flite_synth_channel_speak failed to start task - channel: %d", synth_channel->iId);
			apt_task_destroy(task);
			return FALSE;
		}

		msg = apt_task_msg_acquire(msg_pool);
		msg->type = TASK_MSG_USER;
		flite_msg = (flite_speak_msg_t*) msg->data;
		flite_msg->channel = synth_channel;
		flite_msg->request = request;
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG, "Send signal to start speech synthesis - channel:%d", synth_channel->iId);
		if (apt_task_msg_signal(task,msg))
		{
			response->start_line.request_state = MRCP_REQUEST_STATE_INPROGRESS;
			mrcp_engine_channel_message_send(channel,response);
		}
		else
		{
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING, "Failed to send signal to start speech synthesis - channel:%d", synth_channel->iId);
			apt_task_destroy(task);
			return FALSE;
		}
	}
	return TRUE;
}

static apt_bool_t flite_speak(apt_task_t *task, apt_task_msg_t *msg)
{
	flite_speak_msg_t *flite_msg = (flite_speak_msg_t*)msg->data;
	flite_synth_channel_t *synth_channel = (flite_synth_channel_t *) flite_msg->channel;
	apr_time_t start = 0;
	apr_time_t elapsed = 0;
	apr_time_t stamp = 0;

	mrcp_message_t *stop_response = 0;
	mpf_codec_t * codec = mrcp_engine_source_stream_codec_get(synth_channel->channel);
	apr_uint16_t rate = codec->descriptor->sampling_rate;			

	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "< flite_speak_msg_process speak - channel %d", synth_channel->iId);
	
	// just sequential stuff
	start = apr_time_now();	// in microsec
	if (synth_channel->speak_request->body.length)
	{
		// TODO 
		// create small units of text from synth_channel->speak_request->body.buf ( , . ? ! but ...
		// synthesize small unit and store in audio_buffer
		// check for stop 
		// pause resume state could improve performance 
		// you can "pause" generating new speech from a unit of text 
		// by checking the (decreasing) size of the audio_buffer
		// no need to generate more speech samples than can be listened to...
		cst_wave *wave = 0;
		wave = flite_text_to_wave(synth_channel->speak_request->body.buf, synth_channel->voice);
		if (wave && cst_wave_num_samples(wave)) 
		{
			int generated = (cst_wave_num_samples(wave)/cst_wave_sample_rate(wave)*1000);
			stamp = apr_time_now();
			elapsed = (stamp - start)/1000;
			apt_log(APT_LOG_MARK, APT_PRIO_INFO, "TTS (chan %d) took %"APR_TIME_T_FMT" to generate %d of speech (in millisec)", synth_channel->iId, elapsed, generated);

			if (rate != 16000)
			{
				cst_wave_resample(wave, rate);
				elapsed = (apr_time_now() - stamp)/1000;
				apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "TTS resampling to %d on (chan %d) took %"APR_TIME_T_FMT" millisec", rate, synth_channel->iId, elapsed);
			}
			mpf_buffer_audio_write(synth_channel->audio_buffer, cst_wave_samples(wave), cst_wave_num_samples(wave) * 2);
			delete_wave(wave);
		}
	}

	apr_thread_mutex_lock(synth_channel->channel_guard);
	stop_response = synth_channel->stop_response; 
	apr_thread_mutex_unlock(synth_channel->channel_guard);

	if (!stop_response)
	{
		// this will notify the callback that feeds the client that synthesis is complete
		mpf_buffer_event_write(synth_channel->audio_buffer, MEDIA_FRAME_TYPE_EVENT);
		apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "> flite_speak_msg_process speak - end of TTS - %d", synth_channel->iId);
	}
	else
	{
		apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "> flite_speak_msg_process speak - channel %d", synth_channel->iId);
	}
	return TRUE;
}

/** Process STOP request */
static apt_bool_t flite_synth_channel_stop(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response)
{
	flite_synth_channel_t *synth_channel = (flite_synth_channel_t *) channel->method_obj;
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_channel_stop - channel %d", synth_channel->iId);
	/* store the request, make sure there is no more activity and only then send the response */

	apr_thread_mutex_lock(synth_channel->channel_guard);
	synth_channel->stop_response = response;
	apr_thread_mutex_unlock(synth_channel->channel_guard);

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

/** Callback is called from MPF engine context to destroy any additional data associated with audio stream */
static apt_bool_t flite_synth_stream_destroy(mpf_audio_stream_t *stream)
{
	apt_log(APT_LOG_MARK, APT_PRIO_INFO, "flite_synth_stream_destroy");
	return TRUE;
}

/** Callback is called from MPF engine context to perform any action before open */
static apt_bool_t flite_synth_stream_open(mpf_audio_stream_t *stream)
{
//	flite_synth_channel_t *synth_channel = (flite_synth_channel_t *) stream->obj;
//	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_stream_open - channel %d", synth_channel->iId);
	return TRUE;
}

/** Callback is called from MPF engine context to perform any action after close */
static apt_bool_t flite_synth_stream_close(mpf_audio_stream_t *stream)
{
//	flite_synth_channel_t *synth_channel = (flite_synth_channel_t *) stream->obj;
//	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_stream_close - channel %d", synth_channel->iId);
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
	if (synth_channel->stop_response && synth_channel->speak_request)
	{
		/* send asynchronous response to STOP request */
		mrcp_engine_channel_message_send(synth_channel->channel, synth_channel->stop_response);
		synth_channel->speak_request = NULL;
		synth_channel->paused = FALSE;
		return TRUE;
	}

	/* check if there is active SPEAK request and it isn't in paused state */
	if (synth_channel->speak_request && synth_channel->paused == FALSE) 
	{
		/* normal processing */
		mpf_buffer_frame_read(synth_channel->audio_buffer,frame);
//		apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "flite_synth_stream_read - channel %d - size %d", synth_channel->iId, mpf_buffer_get_size(synth_channel->audio_buffer));

		if((frame->type & MEDIA_FRAME_TYPE_EVENT) == MEDIA_FRAME_TYPE_EVENT) 
		{
			flite_synth_speak_complete_raise(synth_channel);
		}
	}
	return TRUE;
}