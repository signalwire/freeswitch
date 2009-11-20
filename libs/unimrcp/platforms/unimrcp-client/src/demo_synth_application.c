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
 * Demo synthesizer scenario.
 * C -> S: SIP INVITE or RTPS SETUP   (add synthesizer channel)
 * S -> C: SIP OK or RTPS OK
 * C -> S: MRCP SPEAK
 * S -> C: MRCP IN-PROGRESS
 * S -> C: RTP Start Transmission
 * S -> C: MRCP SPEAK-COMPLETE
 * S -> C: RTP Stop Transmission
 * C -> S: SIP INVITE or RTPS SETUP   (optionally remove synthesizer channel)
 * S -> C: SIP OK or RTPS OK
 * C -> S: SIP BYE or RTPS TEARDOWN
 * S -> C: SIP OK or RTPS OK
 */

#include "demo_application.h"
#include "demo_util.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_synth_header.h"
#include "mrcp_synth_resource.h"

typedef struct synth_app_channel_t synth_app_channel_t;

/** Declaration of synthesizer application channel */
struct synth_app_channel_t {
	/** MRCP control channel */
	mrcp_channel_t *channel;
	/** File to write audio stream to */
	FILE           *audio_out;
};

/** Declaration of synthesizer application methods */
static apt_bool_t synth_application_run(demo_application_t *demo_application, const char *profile);
static apt_bool_t synth_application_handler(demo_application_t *application, const mrcp_app_message_t *app_message);

/** Declaration of application message handlers */
static apt_bool_t synth_application_on_session_update(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status);
static apt_bool_t synth_application_on_session_terminate(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status);
static apt_bool_t synth_application_on_channel_add(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status);
static apt_bool_t synth_application_on_channel_remove(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status);
static apt_bool_t synth_application_on_message_receive(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message);

static const mrcp_app_message_dispatcher_t synth_application_dispatcher = {
	synth_application_on_session_update,
	synth_application_on_session_terminate,
	synth_application_on_channel_add,
	synth_application_on_channel_remove,
	synth_application_on_message_receive
};

/** Declaration of synthesizer audio stream methods */
static apt_bool_t synth_app_stream_destroy(mpf_audio_stream_t *stream);
static apt_bool_t synth_app_stream_open(mpf_audio_stream_t *stream, mpf_codec_t *codec);
static apt_bool_t synth_app_stream_close(mpf_audio_stream_t *stream);
static apt_bool_t synth_app_stream_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame);

static const mpf_audio_stream_vtable_t audio_stream_vtable = {
	synth_app_stream_destroy,
	NULL,
	NULL,
	NULL,
	synth_app_stream_open,
	synth_app_stream_close,
	synth_app_stream_write
};


/** Create demo synthesizer application */
demo_application_t* demo_synth_application_create(apr_pool_t *pool)
{
	demo_application_t *synth_application = apr_palloc(pool,sizeof(demo_application_t));
	synth_application->application = NULL;
	synth_application->framework = NULL;
	synth_application->handler = synth_application_handler;
	synth_application->run = synth_application_run;
	return synth_application;
}

/** Create demo synthesizer channel */
static mrcp_channel_t* synth_application_channel_create(mrcp_session_t *session)
{
	mrcp_channel_t *channel;
	mpf_termination_t *termination;
	mpf_stream_capabilities_t *capabilities;
	apr_pool_t *pool = mrcp_application_session_pool_get(session);

	/* create channel */
	synth_app_channel_t *synth_channel = apr_palloc(pool,sizeof(synth_app_channel_t));
	synth_channel->audio_out = NULL;

	/* create sink stream capabilities */
	capabilities = mpf_sink_stream_capabilities_create(pool);

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
			synth_channel);            /* object to associate */
	
	channel = mrcp_application_channel_create(
			session,                   /* session, channel belongs to */
			MRCP_SYNTHESIZER_RESOURCE, /* MRCP resource identifier */
			termination,               /* media termination, used to terminate audio stream */
			NULL,                      /* RTP descriptor, used to create RTP termination (NULL by default) */
			synth_channel);            /* object to associate */
	return channel;
}


/** Run demo synthesizer scenario */
static apt_bool_t synth_application_run(demo_application_t *demo_application, const char *profile)
{
	mrcp_channel_t *channel;
	/* create session */
	mrcp_session_t *session = mrcp_application_session_create(demo_application->application,profile,NULL);
	if(!session) {
		return FALSE;
	}
	
	/* create channel and associate all the required data */
	channel = synth_application_channel_create(session);
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
static apt_bool_t synth_application_handler(demo_application_t *application, const mrcp_app_message_t *app_message)
{
	/* app_message should be dispatched now,
	*  the default dispatcher is used in demo. */
	return mrcp_application_message_dispatch(&synth_application_dispatcher,app_message);
}

/** Handle the responses sent to session update requests */
static apt_bool_t synth_application_on_session_update(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status)
{
	/* not used in demo */
	return TRUE;
}

/** Handle the responses sent to session terminate requests */
static apt_bool_t synth_application_on_session_terminate(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status)
{
	/* received response to session termination request,
	now it's safe to destroy no more referenced session */
	mrcp_application_session_destroy(session);
	return TRUE;
}

/** Handle the responses sent to channel add requests */
static apt_bool_t synth_application_on_channel_add(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status)
{
	synth_app_channel_t *synth_channel = mrcp_application_channel_object_get(channel);
	apr_pool_t *pool = mrcp_application_session_pool_get(session);
	if(status == MRCP_SIG_STATUS_CODE_SUCCESS) {
		mrcp_message_t *mrcp_message;
		const apt_dir_layout_t *dir_layout = mrcp_application_dir_layout_get(application);
		/* create and send SPEAK request */
		mrcp_message = demo_speak_message_create(session,channel,dir_layout);
		if(mrcp_message) {
			mrcp_application_message_send(session,channel,mrcp_message);
		}

		if(synth_channel && session) {
			const apt_str_t *id = mrcp_application_session_id_get(session);
			const mpf_codec_descriptor_t *descriptor = mrcp_application_sink_descriptor_get(channel);
			char *file_name = apr_psprintf(pool,"synth-%dkHz-%s.pcm",
				descriptor ? descriptor->sampling_rate/1000 : 8,
				id->buf);
			char *file_path = apt_datadir_filepath_get(dir_layout,file_name,pool);
			if(file_path) {
				synth_channel->audio_out = fopen(file_path,"wb");
			}
		}
	}
	else {
		/* error case, just terminate the demo */
		mrcp_application_session_terminate(session);
	}
	return TRUE;
}

/** Handle the responses sent to channel remove requests */
static apt_bool_t synth_application_on_channel_remove(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status)
{
	synth_app_channel_t *synth_channel = mrcp_application_channel_object_get(channel);

	/* terminate the demo */
	mrcp_application_session_terminate(session);

	if(synth_channel) {
		FILE *audio_out = synth_channel->audio_out;
		if(audio_out) {
			synth_channel->audio_out = NULL;
			fclose(audio_out);
		}
	}
	return TRUE;
}

/** Handle the MRCP responses/events */
static apt_bool_t synth_application_on_message_receive(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message)
{
	if(message->start_line.message_type == MRCP_MESSAGE_TYPE_RESPONSE) {
		/* received MRCP response */
		if(message->start_line.method_id == SYNTHESIZER_SPEAK) {
			/* received the response to SPEAK request */
			if(message->start_line.request_state == MRCP_REQUEST_STATE_INPROGRESS) {
				/* waiting for SPEAK-COMPLETE event */
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
		/* received MRCP event */
		if(message->start_line.method_id == SYNTHESIZER_SPEAK_COMPLETE) {
			/* received SPEAK-COMPLETE event, remove channel */
			mrcp_application_channel_remove(session,channel);
		}
	}
	return TRUE;
}

/** Callback is called from MPF engine context to destroy any additional data associated with audio stream */
static apt_bool_t synth_app_stream_destroy(mpf_audio_stream_t *stream)
{
	/* nothing to destroy in demo */
	return TRUE;
}

/** Callback is called from MPF engine context to perform application stream specific action before open */
static apt_bool_t synth_app_stream_open(mpf_audio_stream_t *stream, mpf_codec_t *codec)
{
	return TRUE;
}

/** Callback is called from MPF engine context to perform application stream specific action after close */
static apt_bool_t synth_app_stream_close(mpf_audio_stream_t *stream)
{
	return TRUE;
}

/** Callback is called from MPF engine context to make new frame available to write/send */
static apt_bool_t synth_app_stream_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame)
{
	synth_app_channel_t *synth_channel = stream->obj;
	if(synth_channel && synth_channel->audio_out) {
		fwrite(frame->codec_frame.buffer,1,frame->codec_frame.size,synth_channel->audio_out);
	}
	return TRUE;
}
