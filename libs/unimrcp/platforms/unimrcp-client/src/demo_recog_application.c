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
 * $Id: demo_recog_application.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

/*
 * Demo recognizer scenario.
 * C -> S: SIP INVITE or RTPS SETUP   (add recognizer channel)
 * S -> C: SIP OK or RTPS OK
 * C -> S: MRCP RECOGNIZE
 * S -> C: MRCP IN-PROGRESS
 * C -> S: RTP Start Transmission
 * S -> C: MRCP START-OF-INPUT
 * S -> C: MRCP RECOGNITION-COMPLETE
 * C -> S: RTP Stop Transmission
 * C -> S: SIP INVITE or RTPS SETUP   (optionally remove recognizer channel)
 * S -> C: SIP OK or RTPS OK
 * C -> S: SIP BYE or RTPS TEARDOWN
 * S -> C: SIP OK or RTPS OK
 */

#include "demo_application.h"
#include "demo_util.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_recog_header.h"
#include "mrcp_recog_resource.h"
#include "apt_nlsml_doc.h"
#include "apt_log.h"

typedef struct recog_app_channel_t recog_app_channel_t;

/** Declaration of recognizer application channel */
struct recog_app_channel_t {
	/** MRCP control channel */
	mrcp_channel_t *channel;

	/** Streaming is in-progress */
	apt_bool_t      streaming;
	/** File to read audio stream from */
	FILE           *audio_in;
	/** Estimated time to complete (used if no audio_in available) */
	apr_size_t      time_to_complete;
};

/** Declaration of recognizer application methods */
static apt_bool_t recog_application_run(demo_application_t *demo_application, const char *profile);
static apt_bool_t recog_application_handler(demo_application_t *application, const mrcp_app_message_t *app_message);

/** Declaration of application message handlers */
static apt_bool_t recog_application_on_session_update(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status);
static apt_bool_t recog_application_on_session_terminate(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status);
static apt_bool_t recog_application_on_channel_add(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status);
static apt_bool_t recog_application_on_channel_remove(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status);
static apt_bool_t recog_application_on_message_receive(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message);

static const mrcp_app_message_dispatcher_t recog_application_dispatcher = {
	recog_application_on_session_update,
	recog_application_on_session_terminate,
	recog_application_on_channel_add,
	recog_application_on_channel_remove,
	recog_application_on_message_receive,
	NULL /* recog_application_on_terminate_event */,
	NULL /* recog_application_on_resource_discover */
};

/** Declaration of recognizer audio stream methods */
static apt_bool_t recog_app_stream_destroy(mpf_audio_stream_t *stream);
static apt_bool_t recog_app_stream_open(mpf_audio_stream_t *stream, mpf_codec_t *codec);
static apt_bool_t recog_app_stream_close(mpf_audio_stream_t *stream);
static apt_bool_t recog_app_stream_read(mpf_audio_stream_t *stream, mpf_frame_t *frame);

static const mpf_audio_stream_vtable_t audio_stream_vtable = {
	recog_app_stream_destroy,
	recog_app_stream_open,
	recog_app_stream_close,
	recog_app_stream_read,
	NULL,
	NULL,
	NULL,
	NULL
};


/** Create demo recognizer application */
demo_application_t* demo_recog_application_create(apr_pool_t *pool)
{
	demo_application_t *recog_application = apr_palloc(pool,sizeof(demo_application_t));
	recog_application->application = NULL;
	recog_application->framework = NULL;
	recog_application->handler = recog_application_handler;
	recog_application->run = recog_application_run;
	return recog_application;
}

/** Create demo recognizer channel */
static mrcp_channel_t* recog_application_channel_create(mrcp_session_t *session)
{
	mrcp_channel_t *channel;
	mpf_termination_t *termination;
	mpf_stream_capabilities_t *capabilities;
	apr_pool_t *pool = mrcp_application_session_pool_get(session);

	/* create channel */
	recog_app_channel_t *recog_channel = apr_palloc(pool,sizeof(recog_app_channel_t));
	recog_channel->streaming = FALSE;
	recog_channel->audio_in = NULL;
	recog_channel->time_to_complete = 0;

	/* create source stream capabilities */
	capabilities = mpf_source_stream_capabilities_create(pool);

	/* add codec capabilities (Linear PCM) */
	mpf_codec_capabilities_add(
			&capabilities->codecs,
			MPF_SAMPLE_RATE_8000 | MPF_SAMPLE_RATE_16000,
			"LPCM");

#if 0
	/* more capabilities can be added or replaced */
	mpf_codec_capabilities_add(
			&capabilities->codecs,
			MPF_SAMPLE_RATE_8000 | MPF_SAMPLE_RATE_16000,
			"PCMU");
#endif

	termination = mrcp_application_audio_termination_create(
			session,                   /* session, termination belongs to */
			&audio_stream_vtable,      /* virtual methods table of audio stream */
			capabilities,              /* capabilities of audio stream */
			recog_channel);            /* object to associate */
	
	channel = mrcp_application_channel_create(
			session,                   /* session, channel belongs to */
			MRCP_RECOGNIZER_RESOURCE,  /* MRCP resource identifier */
			termination,               /* media termination, used to terminate audio stream */
			NULL,                      /* RTP descriptor, used to create RTP termination (NULL by default) */
			recog_channel);            /* object to associate */
	return channel;
}

/** Run demo recognizer scenario */
static apt_bool_t recog_application_run(demo_application_t *demo_application, const char *profile)
{
	mrcp_channel_t *channel;
	/* create session */
	mrcp_session_t *session = mrcp_application_session_create(demo_application->application,profile,NULL);
	if(!session) {
		return FALSE;
	}
	
	/* create channel and associate all the required data */
	channel = recog_application_channel_create(session);
	if(!channel) {
		mrcp_application_session_destroy(session);
		return FALSE;
	}

	/* add channel to session (send asynchronous request) */
	if(mrcp_application_channel_add(session,channel) != TRUE) {
		/* session and channel are still not referenced 
		and both are allocated from session pool and will
		be freed with session destroy call */
		mrcp_application_session_destroy(session);
		return FALSE;
	}

	return TRUE;
}

/** Handle the messages sent from the MRCP client stack */
static apt_bool_t recog_application_handler(demo_application_t *application, const mrcp_app_message_t *app_message)
{
	/* app_message should be dispatched now,
	*  the default dispatcher is used in demo. */
	return mrcp_application_message_dispatch(&recog_application_dispatcher,app_message);
}

/** Handle the responses sent to session update requests */
static apt_bool_t recog_application_on_session_update(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status)
{
	/* not used in demo */
	return TRUE;
}

/** Handle the responses sent to session terminate requests */
static apt_bool_t recog_application_on_session_terminate(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status)
{
	/* received response to session termination request,
	now it's safe to destroy no more referenced session */
	mrcp_application_session_destroy(session);
	return TRUE;
}

/** Handle the responses sent to channel add requests */
static apt_bool_t recog_application_on_channel_add(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status)
{
	if(status == MRCP_SIG_STATUS_CODE_SUCCESS) {
		mrcp_message_t *mrcp_message;
		const apt_dir_layout_t *dir_layout = mrcp_application_dir_layout_get(application);
		/* create and send DEFINE-GRAMMAR request */
		mrcp_message = demo_define_grammar_message_create(session,channel,dir_layout);
		if(mrcp_message) {
			mrcp_application_message_send(session,channel,mrcp_message);
		}
	}
	else {
		/* error case, just terminate the demo */
		mrcp_application_session_terminate(session);
	}
	return TRUE;
}

static apt_bool_t recog_application_on_channel_remove(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status)
{
	recog_app_channel_t *recog_channel = mrcp_application_channel_object_get(channel);

	/* terminate the demo */
	mrcp_application_session_terminate(session);

	if(recog_channel) {
		FILE *audio_in = recog_channel->audio_in;
		if(audio_in) {
			recog_channel->audio_in = NULL;
			fclose(audio_in);
		}
	}
	return TRUE;
}

/** Handle the DEFINE-GRAMMAR responses */
static apt_bool_t recog_application_on_define_grammar(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel)
{
	mrcp_message_t *mrcp_message;
	recog_app_channel_t *recog_channel = mrcp_application_channel_object_get(channel);
	const apt_dir_layout_t *dir_layout = mrcp_application_dir_layout_get(application);

	const mpf_codec_descriptor_t *descriptor = mrcp_application_source_descriptor_get(channel);
	if(!descriptor) {
		/* terminate the demo */
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Get Media Source Descriptor");
		return mrcp_application_session_terminate(session);
	}

	/* create and send RECOGNIZE request */
	mrcp_message = demo_recognize_message_create(session,channel,dir_layout);
	if(mrcp_message) {
		mrcp_application_message_send(session,channel,mrcp_message);
	}
	
	if(recog_channel) {
		apr_pool_t *pool = mrcp_application_session_pool_get(session);
		char *file_name = apr_psprintf(pool,"one-%dkHz.pcm",descriptor->sampling_rate/1000);
		char *file_path = apt_datadir_filepath_get(dir_layout,file_name,pool);
		if(file_path) {
			recog_channel->audio_in = fopen(file_path,"rb");
			if(recog_channel->audio_in) {
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set [%s] as Speech Source",file_path);
			}
			else {
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Cannot Find [%s]",file_path);
				/* set some estimated time to complete */
				recog_channel->time_to_complete = 5000; // 5 sec
			}
		}
	}
	return TRUE;
}

static apt_bool_t recog_application_on_message_receive(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message)
{
	recog_app_channel_t *recog_channel = mrcp_application_channel_object_get(channel);
	if(message->start_line.message_type == MRCP_MESSAGE_TYPE_RESPONSE) {
		/* received MRCP response */
		if(message->start_line.method_id == RECOGNIZER_DEFINE_GRAMMAR) {
			/* received the response to DEFINE-GRAMMAR request */
			if(message->start_line.request_state == MRCP_REQUEST_STATE_COMPLETE) {
				recog_application_on_define_grammar(application,session,channel);
			}
			else {
				/* received unexpected response, remove channel */
				mrcp_application_channel_remove(session,channel);
			}
		}
		else if(message->start_line.method_id == RECOGNIZER_RECOGNIZE) {
			/* received the response to RECOGNIZE request */
			if(message->start_line.request_state == MRCP_REQUEST_STATE_INPROGRESS) {
				/* start to stream the speech to recognize */
				if(recog_channel) {
					recog_channel->streaming = TRUE;
				}
			}
			else {
				/* received unexpected response, remove channel */
				mrcp_application_channel_remove(session,channel);
			}
		}
		else {
			/* received unexpected response */
		}
	}
	else if(message->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) {
		if(message->start_line.method_id == RECOGNIZER_RECOGNITION_COMPLETE) {
			nlsml_result_t *result = nlsml_result_parse(message->body.buf, message->body.length, message->pool);
			if(result) {
				nlsml_result_trace(result, message->pool);
			}
			
			if(recog_channel) {
				recog_channel->streaming = FALSE;
			}
			mrcp_application_channel_remove(session,channel);
		}
		else if(message->start_line.method_id == RECOGNIZER_START_OF_INPUT) {
			/* received start-of-input, do whatever you need here */
		}
	}
	return TRUE;
}

/** Callback is called from MPF engine context to destroy any additional data associated with audio stream */
static apt_bool_t recog_app_stream_destroy(mpf_audio_stream_t *stream)
{
	return TRUE;
}

/** Callback is called from MPF engine context to perform application stream specific action before open */
static apt_bool_t recog_app_stream_open(mpf_audio_stream_t *stream, mpf_codec_t *codec)
{
	return TRUE;
}

/** Callback is called from MPF engine context to perform application stream specific action after close */
static apt_bool_t recog_app_stream_close(mpf_audio_stream_t *stream)
{
	return TRUE;
}

/** Callback is called from MPF engine context to read new frame */
static apt_bool_t recog_app_stream_read(mpf_audio_stream_t *stream, mpf_frame_t *frame)
{
	recog_app_channel_t *recog_channel = stream->obj;
	if(recog_channel && recog_channel->streaming == TRUE) {
		if(recog_channel->audio_in) {
			if(fread(frame->codec_frame.buffer,1,frame->codec_frame.size,recog_channel->audio_in) == frame->codec_frame.size) {
				/* normal read */
				frame->type |= MEDIA_FRAME_TYPE_AUDIO;
			}
			else {
				/* file is over */
				recog_channel->streaming = FALSE;
			}
		}
		else {
			/* fill with silence in case no file available */
			if(recog_channel->time_to_complete >= CODEC_FRAME_TIME_BASE) {
				frame->type |= MEDIA_FRAME_TYPE_AUDIO;
				memset(frame->codec_frame.buffer,0,frame->codec_frame.size);
				recog_channel->time_to_complete -= CODEC_FRAME_TIME_BASE;
			}
			else {
				recog_channel->streaming = FALSE;
			}
		}
	}
	return TRUE;
}
