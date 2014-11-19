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
 * $Id: demo_bypass_application.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

/* 
 * Demo synthesizer scenario (client stack stays out of media path).
 * C -> S: SIP INVITE or RTPS SETUP   (add synthesizer channel)
 * S -> C: SIP OK or RTPS OK
 * C -> S: MRCP SPEAK
 * S -> C: MRCP IN-PROGRESS
 * S -> X: RTP Start Transmission     (RTP stream is sent directly to external endpoint bypassing client stack)
 * S -> C: MRCP SPEAK-COMPLETE
 * S -> X: RTP Stop Transmission
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
#include "apt_log.h"

typedef struct demo_app_channel_t demo_app_channel_t;

/** Declaration of synthesizer application channel */
struct demo_app_channel_t {
	/** MRCP control channel */
	mrcp_channel_t     *channel;
};

/** Declaration of demo application methods */
static apt_bool_t demo_application_run(demo_application_t *demo_application, const char *profile);
static apt_bool_t demo_application_handler(demo_application_t *application, const mrcp_app_message_t *app_message);

static apt_bool_t demo_application_on_session_update(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status);
static apt_bool_t demo_application_on_session_terminate(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status);
static apt_bool_t demo_application_on_channel_add(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status);
static apt_bool_t demo_application_on_channel_remove(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status);
static apt_bool_t demo_application_on_message_receive(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message);

static const mrcp_app_message_dispatcher_t demo_application_dispatcher = {
	demo_application_on_session_update,
	demo_application_on_session_terminate,
	demo_application_on_channel_add,
	demo_application_on_channel_remove,
	demo_application_on_message_receive,
	NULL /* demo_application_on_terminate_event */,
	NULL /* demo_application_on_resource_discover */
};


/** Create demo bypass media application */
demo_application_t* demo_bypass_application_create(apr_pool_t *pool)
{
	demo_application_t *demo_application = apr_palloc(pool,sizeof(demo_application_t));
	demo_application->application = NULL;
	demo_application->framework = NULL;
	demo_application->handler = demo_application_handler;
	demo_application->run = demo_application_run;
	return demo_application;
}

/** Create demo channel */
static mrcp_channel_t* demo_application_channel_create(mrcp_session_t *session)
{
	mrcp_channel_t *channel;
	apr_pool_t *pool = mrcp_application_session_pool_get(session);
	/* create channel */
	demo_app_channel_t *demo_channel = apr_palloc(pool,sizeof(demo_app_channel_t));
	mpf_rtp_termination_descriptor_t *rtp_descriptor = demo_rtp_descriptor_create(pool);
	channel = mrcp_application_channel_create(
			session,                     /* session, channel belongs to */
			MRCP_SYNTHESIZER_RESOURCE,   /* MRCP resource identifier */
			NULL,                        /* no termination (not to use internal media processing)  */
			rtp_descriptor,              /* RTP descriptor, used to create RTP termination */
			demo_channel);               /* object to associate */
	return channel;
}


/** Run demo scenario */
static apt_bool_t demo_application_run(demo_application_t *demo_application, const char *profile)
{
	mrcp_channel_t *channel;
	/* create session */
	mrcp_session_t *session = mrcp_application_session_create(demo_application->application,profile,NULL);
	if(!session) {
		return FALSE;
	}
	
	/* create channel and associate all the required data */
	channel = demo_application_channel_create(session);
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
static apt_bool_t demo_application_handler(demo_application_t *application, const mrcp_app_message_t *app_message)
{
	/* app_message should be dispatched now,
	*  the default dispatcher is used in demo. */
	return mrcp_application_message_dispatch(&demo_application_dispatcher,app_message);
}

/** Handle the responses sent to session update requests */
static apt_bool_t demo_application_on_session_update(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status)
{
	return TRUE;
}

/** Handle the responses sent to session terminate requests */
static apt_bool_t demo_application_on_session_terminate(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status)
{
	/* received response to session termination request,
	now it's safe to destroy no more referenced session */
	mrcp_application_session_destroy(session);
	return TRUE;
}

/** Handle the responses sent to channel add requests */
static apt_bool_t demo_application_on_channel_add(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status)
{
	if(status == MRCP_SIG_STATUS_CODE_SUCCESS) {
		mpf_rtp_termination_descriptor_t *rtp_descriptor;
		mrcp_message_t *mrcp_message;
		const apt_dir_layout_t *dir_layout = mrcp_application_dir_layout_get(application);
		/* create and send SPEAK request */
		mrcp_message = demo_speak_message_create(session,channel,dir_layout);
		if(mrcp_message) {
			mrcp_application_message_send(session,channel,mrcp_message);
		}
		rtp_descriptor = mrcp_application_rtp_descriptor_get(channel);
		if(rtp_descriptor) {
			mpf_rtp_media_descriptor_t *local_media = rtp_descriptor->audio.local;
			mpf_rtp_media_descriptor_t *remote_media = rtp_descriptor->audio.remote;
			if(local_media && remote_media) {
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Media Attributes: L[%s/%d] R[%s/%d]",
					local_media->ip.buf,
					local_media->port,
					remote_media->ip.buf,
					remote_media->port);
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
static apt_bool_t demo_application_on_channel_remove(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status)
{
	/* terminate the demo */
	mrcp_application_session_terminate(session);
	return TRUE;
}

/** Handle the MRCP responses/events */
static apt_bool_t demo_application_on_message_receive(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message)
{
	if(message->start_line.message_type == MRCP_MESSAGE_TYPE_RESPONSE) {
		/* received MRCP response */
		if(message->start_line.method_id == SYNTHESIZER_SPEAK) {
			/* received the response to SPEAK request, 
			waiting for SPEAK-COMPLETE event */
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
