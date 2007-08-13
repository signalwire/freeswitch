/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Traun Leyden <tleyden@branchcut.com>
 *
 * Module which acts as an MRCP client to an MRCP speech recognition
 * server.  In other words it bridges freeswitch to an external speech
 * recognition system.  Documentation on how to install and configure
 * the module is here: http://wiki.freeswitch.org/wiki/Mod_openmrcp
 *
 * Uses OpenMrcp (http://wiki.freeswitch.org/wiki/OpenMRCP) as the 
 * the client library.  
 *
 * TODO
 * =======
 *
 * - MAJOR DESIGN ISSUE!!  It is way too expensive to be calling malloc on every audio frame,
 * this needs to send the packet directly.  OpenMrcp will need a way to disable their own
 * timer so that the fs timer is the only one driving the media.
 * 
 * - There are two memory pools in use.  One in asr_session which is managed
 * by this module, and one in the switch_asr_handle_t, which is managed by freeswitch.
 * These need to be consolidated into one.  (basically throw away the one in asr_session)
 * 
 * - fs status codes (eg, SWITCH_STATUS_GENERR) and mrcp status codes (MRCP_STATUS_FAILURE)
 * are intermixed badly.  this needs cleanup
 * 
 * - openmrcp_flush_tts, openmrcp_text_param_tts, openmrcp_numeric_param_tts, 
 * openmrcp_float_param_tts need to have functionality added
 *
 * - fix audio problem with TTS, convert from using queue to using a switch_buffer
 * (in progress)
 *
 * - use a regex for extracting xml from raw result received from mrcp recognition
 * server
 *
 */

#ifdef __ICC
#pragma warning (disable:188)
#endif


#include "openmrcp_client.h"
#include "mrcp_client_context.h"
#include "mrcp_recognizer.h"
#include "mrcp_synthesizer.h"
#include "mrcp_generic_header.h"
#include "rtp_session.h"
#include "mrcp_client.h"
#include "mrcp_client_context.h"
#include "mrcp_client_defs.h"
#include "mrcp_client_session.h"
#include "mrcp_client_resource.h"
#include "mrcp_client_signaling_agent.h"
#include "mrcp_media_agent.h"
#include "mrcp_resource.h"
#include "mrcp_consumer_task.h"
#include <apr_general.h>
#include <apr_file_io.h>
#include <apr_thread_proc.h>
#include <apr_thread_cond.h>
#include <apr_strings.h>

#include <switch.h>
	
#define OPENMRCP_WAIT_TIMEOUT 5000
#define MY_BUF_LEN 1024 * 128
#define MY_BLOCK_SIZE MY_BUF_LEN

SWITCH_MODULE_LOAD_FUNCTION(mod_openmrcp_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_openmrcp_shutdown);
SWITCH_MODULE_DEFINITION(mod_openmrcp, mod_openmrcp_load, 
						 mod_openmrcp_shutdown, NULL);

static struct {
	char *asr_client_ip;
	char *asr_server_ip;
	uint32_t asr_proto_version;
	uint32_t asr_client_port;
	uint32_t asr_server_port;
	char *tts_client_ip;
	char *tts_server_ip;
	uint32_t tts_proto_version;
	uint32_t tts_client_port;
	uint32_t tts_server_port;
	uint32_t rtp_port_min;
	uint32_t rtp_port_max;
} globals;


typedef enum {
	OPENMRCP_EVENT_NONE,
	OPENMRCP_EVENT_SESSION_INITIATE,
	OPENMRCP_EVENT_SESSION_TERMINATE,
	OPENMRCP_EVENT_CHANNEL_CREATE,
	OPENMRCP_EVENT_CHANNEL_DESTROY,
	OPENMRCP_EVENT_CHANNEL_MODIFY
} openmrcp_event_t;

static const char *openmrcp_event_names[] = {
	"NONE",
	"SESSION_INITIATE",
	"SESSION_TERMINATE",
	"CHANNEL_CREATE",
	"CHANNEL_DESTROY",
	"CHANNEL_MODIFY",
};

typedef struct asr_session_t asr_session_t;
struct asr_session_t {
	mrcp_session_t        *client_session;
	mrcp_client_channel_t *channel;
	switch_queue_t        *audio_queue;
	switch_queue_t        *event_queue;
	mrcp_message_t        *mrcp_message_last_rcvd;
	audio_source_t        *source;
	apr_pool_t            *pool;
	uint32_t 			   flags;
	switch_mutex_t        *flag_mutex;
};

typedef struct tts_session_t tts_session_t;
struct tts_session_t {
	mrcp_session_t        *client_session;
	mrcp_client_channel_t *channel;
	switch_queue_t        *audio_queue;  // TO BE REMOVED
	switch_queue_t        *event_queue;
	switch_mutex_t        *audio_lock;
	switch_buffer_t       *audio_buffer;
	audio_sink_t          *sink;
	apr_pool_t            *pool;
	switch_speech_flag_t  flags;
	switch_mutex_t        *flag_mutex;
};

static apr_status_t openmrcp_recognizer_read_frame(audio_source_t *source, media_frame_t *frame);
static apr_status_t openmrcp_tts_write_frame(audio_sink_t *sink, media_frame_t *frame);

static const audio_source_method_set_t audio_source_method_set = {
	NULL,
	NULL,
	NULL,
	openmrcp_recognizer_read_frame
};

static const audio_sink_method_set_t audio_sink_method_set = {
	NULL,
	NULL,
	NULL,
	openmrcp_tts_write_frame
};

typedef enum {
	FLAG_HAS_TEXT = (1 << 0),
	FLAG_BARGE = (1 << 1),
	FLAG_READY = (1 << 2),
	FLAG_SPEAK_COMPLETE = (1 << 3)
} mrcp_flag_t;

typedef struct {
	mrcp_client_t *asr_client;
	mrcp_client_context_t *asr_client_context;
	mrcp_client_t *tts_client;
	mrcp_client_context_t *tts_client_context;
} openmrcp_module_t;

static openmrcp_module_t openmrcp_module;

static asr_session_t* asr_session_create()
{
	asr_session_t *asr_session;
	apr_pool_t *session_pool;

	if(apr_pool_create(&session_pool,NULL) != APR_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to create session_pool\n");
		return NULL;
	}

	asr_session = apr_palloc(session_pool,sizeof(asr_session_t));
	asr_session->pool = session_pool;
	asr_session->client_session = NULL;
	asr_session->channel = NULL;
	asr_session->audio_queue = NULL;
	asr_session->event_queue = NULL;


	/* create an event queue */
	if (switch_queue_create(&asr_session->event_queue, 1000, asr_session->pool)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,  "event queue creation failed\n");
	}

	return asr_session;
}


static tts_session_t* tts_session_create()
{
	tts_session_t *tts_session;
	apr_pool_t *session_pool;

	if(apr_pool_create(&session_pool,NULL) != APR_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to create session_pool\n");
		return NULL;
	}

	tts_session = apr_palloc(session_pool,sizeof(tts_session_t));
	tts_session->pool = session_pool;
	tts_session->client_session = NULL;
	tts_session->channel = NULL;
	tts_session->audio_queue = NULL;
	tts_session->event_queue = NULL;

	/* create an event queue */
	if (switch_queue_create(&tts_session->event_queue, 1000, tts_session->pool)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,  "event queue creation failed\n");
	}

	return tts_session;
}


static mrcp_status_t asr_session_destroy(asr_session_t *asr_session)
{
	if(!asr_session) {
		return MRCP_STATUS_FAILURE;
	}

	if(asr_session->pool) {
		apr_pool_destroy(asr_session->pool);
		asr_session->pool = NULL;
	}
	return MRCP_STATUS_SUCCESS;
}


static mrcp_status_t tts_session_destroy(tts_session_t *tts_session)
{
	if(!tts_session) {
		return MRCP_STATUS_FAILURE;
	}

	switch_buffer_destroy(&tts_session->audio_buffer);

	if(tts_session->pool) {
		apr_pool_destroy(tts_session->pool);
		tts_session->pool = NULL;
	}
	return MRCP_STATUS_SUCCESS;
}


static mrcp_status_t wait_for_event(switch_queue_t *event_queue, openmrcp_event_t openmrcp_event, size_t timeout)
{

	openmrcp_event_t *popped_event = NULL;
	size_t sleep_ms = 100;

	if(event_queue) {
		if (switch_queue_trypop(event_queue, (void *) &popped_event)) {
			// most likely this failed because queue is empty.  sleep for timeout seconds and try again?
			if (timeout > 0) {
				if (timeout < sleep_ms) {
					switch_sleep(timeout * 1000);  // ms->us
					timeout = 0;  
				}
				else {
					switch_sleep(sleep_ms * 1000);  // ms->us
					timeout -= sleep_ms;  
				}

				// call recursively
				// TODO: This is going to end up in a lot of recursion and
				// maybe blow the stack .. rethink this approach
				return wait_for_event(event_queue, openmrcp_event, timeout);

			}
			else {
				// nothing in queue, no timeout left, return failure
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "nothing in queue, no timeout left, return failure\n");
				return MRCP_STATUS_FAILURE;
			}

		}
		else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "popped event\n");
			// check if popped event matches the event we are looking for
			if (*popped_event == openmrcp_event) {
				// just what we were waiting for!  
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "just what we were waiting for! rturn success\n");
				return MRCP_STATUS_SUCCESS;
			}
			else {
				// nothing popped, but maybe there's other things in queue, or something will arrive
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "popped unexpected\n");
				if (!popped_event) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "popped NULL!!\n");
				}
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "popped: %d\n", *popped_event);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "popped name: %s\n", openmrcp_event_names[*popped_event]);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "popped [%s] but was expecting [%s], but maybe there's other things in queue, or something will arrive\n", openmrcp_event_names[*popped_event], openmrcp_event_names[openmrcp_event]);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "calling recursively\n");
				return wait_for_event(event_queue, openmrcp_event, timeout);
			}
			
		}
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "event queue is null\n");
		return MRCP_STATUS_FAILURE;
	}

}


static mrcp_status_t asr_session_signal_event(asr_session_t *asr_session, openmrcp_event_t openmrcp_event)
{
	mrcp_status_t status = MRCP_STATUS_SUCCESS;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Got event: %s\n", openmrcp_event_names[openmrcp_event]);


	// allocate memory for event
	openmrcp_event_t *event2queue = (openmrcp_event_t *) switch_core_alloc(asr_session->pool, sizeof(openmrcp_event_t));
	*event2queue = openmrcp_event;

	// add it to queue
	if (switch_queue_trypush(asr_session->event_queue, (void *) event2queue)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "could not push event to queue\n");
		status = SWITCH_STATUS_GENERR;
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "pushed event to queue: %s\n", openmrcp_event_names[*event2queue]);
	}

	return status;
}


static mrcp_status_t asr_on_session_initiate(mrcp_client_context_t *context, mrcp_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_on_session_initiate called\n");
	asr_session_t *asr_session = mrcp_client_context_session_object_get(session);
	if(!asr_session) {
		return MRCP_STATUS_FAILURE;
	}
	return asr_session_signal_event(asr_session,OPENMRCP_EVENT_SESSION_INITIATE);
}

static mrcp_status_t asr_on_session_terminate(mrcp_client_context_t *context, mrcp_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_on_session_terminate called\n");
	asr_session_t *asr_session = mrcp_client_context_session_object_get(session);
	if(!asr_session) {
		return MRCP_STATUS_FAILURE;
	}
	return asr_session_signal_event(asr_session,OPENMRCP_EVENT_SESSION_TERMINATE);
}

static mrcp_status_t asr_on_channel_add(mrcp_client_context_t *context, mrcp_session_t *session, mrcp_client_channel_t *channel)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_on_channel_add called\n");
	asr_session_t *asr_session = mrcp_client_context_session_object_get(session);
	if(!asr_session) {
		return MRCP_STATUS_FAILURE;
	}
	return asr_session_signal_event(asr_session,OPENMRCP_EVENT_CHANNEL_CREATE);
}

static mrcp_status_t asr_on_channel_remove(mrcp_client_context_t *context, mrcp_session_t *session, mrcp_client_channel_t *channel)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "asr_on_channel_remove called\n");
	asr_session_t *asr_session = mrcp_client_context_session_object_get(session);
	if(!asr_session) {
		return MRCP_STATUS_FAILURE;
	}
	return asr_session_signal_event(asr_session,OPENMRCP_EVENT_CHANNEL_DESTROY);
}

/** this is called by the mrcp core whenever an mrcp message is received from
    the other side. */
static mrcp_status_t asr_on_channel_modify(mrcp_client_context_t *context, mrcp_session_t *session, mrcp_message_t *mrcp_message)
{

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "asr_on_channel_modify called\n");
	asr_session_t *asr_session = mrcp_client_context_session_object_get(session);
	if(!asr_session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "!asr_session\n");
		return MRCP_STATUS_FAILURE;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "mrcp msg body: %s\n", mrcp_message->body);

	if (!strcmp(mrcp_message->start_line.method_name,"RECOGNITION-COMPLETE")) {
		asr_session->mrcp_message_last_rcvd = mrcp_message;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "setting FLAG_HAS_TEXT\n");
		switch_set_flag_locked(asr_session, FLAG_HAS_TEXT);
	}
	else if (!strcmp(mrcp_message->start_line.method_name,"START-OF-SPEECH")) {
		asr_session->mrcp_message_last_rcvd = mrcp_message;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "setting FLAG_BARGE\n");
		switch_set_flag_locked(asr_session, FLAG_BARGE);
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "ignoring method: %s\n", mrcp_message->start_line.method_name);
	}
		
	return asr_session_signal_event(asr_session,OPENMRCP_EVENT_CHANNEL_MODIFY);
}



static mrcp_status_t tts_session_signal_event(tts_session_t *tts_session, openmrcp_event_t openmrcp_event)
{
	mrcp_status_t status = MRCP_STATUS_SUCCESS;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Got event: %s\n", openmrcp_event_names[openmrcp_event]);

	// allocate memory for event
	openmrcp_event_t *event2queue = (openmrcp_event_t *) switch_core_alloc(tts_session->pool, sizeof(openmrcp_event_t));
	*event2queue = openmrcp_event;

	// add it to queue
	if (switch_queue_trypush(tts_session->event_queue, (void *) event2queue)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "could not push event to queue\n");
		status = SWITCH_STATUS_GENERR;
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "pushed event to queue: %s\n", openmrcp_event_names[*event2queue]);
	}

	return status;
}

static mrcp_status_t tts_on_session_initiate(mrcp_client_context_t *context, mrcp_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "tts_on_session_initiate called\n");
	tts_session_t *tts_session = mrcp_client_context_session_object_get(session);
	if(!tts_session) {
		return MRCP_STATUS_FAILURE;
	}
	return tts_session_signal_event(tts_session,OPENMRCP_EVENT_SESSION_INITIATE);
}

static mrcp_status_t tts_on_session_terminate(mrcp_client_context_t *context, mrcp_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "tts_on_session_terminate called\n");
	tts_session_t *tts_session = mrcp_client_context_session_object_get(session);
	if(!tts_session) {
		return MRCP_STATUS_FAILURE;
	}
	return tts_session_signal_event(tts_session,OPENMRCP_EVENT_SESSION_TERMINATE);
}

static mrcp_status_t tts_on_channel_add(mrcp_client_context_t *context, mrcp_session_t *session, mrcp_client_channel_t *channel)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "tts_on_channel_add called\n");
	tts_session_t *tts_session = mrcp_client_context_session_object_get(session);
	if(!tts_session) {
		return MRCP_STATUS_FAILURE;
	}
	return tts_session_signal_event(tts_session,OPENMRCP_EVENT_CHANNEL_CREATE);
}

static mrcp_status_t tts_on_channel_remove(mrcp_client_context_t *context, mrcp_session_t *session, mrcp_client_channel_t *channel)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "tts_on_channel_remove called\n");
	tts_session_t *tts_session = mrcp_client_context_session_object_get(session);
	if(!tts_session) {
		return MRCP_STATUS_FAILURE;
	}
	return tts_session_signal_event(tts_session,OPENMRCP_EVENT_CHANNEL_DESTROY);
}

/** this is called by the mrcp core whenever an mrcp message is received from
    the other side. */
static mrcp_status_t tts_on_channel_modify(mrcp_client_context_t *context, mrcp_session_t *session, mrcp_message_t *mrcp_message)
{

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "tts_on_channel_modify called\n");
	tts_session_t *tts_session = mrcp_client_context_session_object_get(session);
	if(!tts_session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "!tts_session\n");
		return MRCP_STATUS_FAILURE;
	}

	if (mrcp_message->start_line.method_name) {
		if (!strcmp(mrcp_message->start_line.method_name,"SPEAK-COMPLETE")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "setting FLAG_SPEAK_COMPLETE\n");
			switch_set_flag_locked(tts_session, FLAG_SPEAK_COMPLETE);
		}
		else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "ignoring method: %s\n", mrcp_message->start_line.method_name);
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "mrcp msg body: %s\n", mrcp_message->body);
		
	

	return tts_session_signal_event(tts_session,OPENMRCP_EVENT_CHANNEL_MODIFY);
}



static apr_status_t set_default_asr_options(openmrcp_client_options_t *options)
{
	mrcp_logger.priority = MRCP_PRIO_INFO;
	options->proto_version = globals.asr_proto_version; 
	options->client_ip = globals.asr_client_ip;
	options->server_ip = globals.asr_server_ip;
	options->client_port = globals.asr_client_port;
	options->server_port = globals.asr_server_port;
	options->rtp_port_min = globals.rtp_port_min;
	options->rtp_port_max = globals.rtp_port_max;
	return APR_SUCCESS;
}


static apr_status_t set_default_tts_options(openmrcp_client_options_t *options)
{
	mrcp_logger.priority = MRCP_PRIO_INFO;
	options->proto_version = globals.tts_proto_version; 
	options->client_ip = globals.tts_client_ip;
	options->server_ip = globals.tts_server_ip;
	options->client_port = globals.tts_client_port;
	options->server_port = globals.tts_server_port;
	options->rtp_port_min = globals.rtp_port_min;
	options->rtp_port_max = globals.rtp_port_max;
	return APR_SUCCESS;
}


/**
 * Called back by openmrcp client thread every time it receives audio 
 * from the TTS server we are connected to.  Puts audio in a queueu
 * and it will be pulled out from read_tts
 */
static apr_status_t openmrcp_tts_write_frame(audio_sink_t *sink, media_frame_t *frame)
{
	tts_session_t *tts_session = sink->object;	
	media_frame_t *media_frame;
	switch_byte_t *buffer;
	size_t len;

	len = frame->codec_frame.size;

	/* create new media frame */
	media_frame = (media_frame_t *) switch_core_alloc(tts_session->pool, sizeof(media_frame_t));
	if (!media_frame) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "media_frame creation failed\n");
		return SWITCH_STATUS_MEMERR;
	}
	
	/**
	 * since *frame might get freed by caller (true or false?), allocate a
	 * new buffer and copy *data into it.
	 **/
	buffer = (switch_byte_t *) switch_core_alloc(tts_session->pool, sizeof(switch_byte_t)*len);
	if (!buffer) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate buffer\n");
		return SWITCH_STATUS_MEMERR;
	}
	buffer = memcpy(buffer, frame->codec_frame.buffer, len);
	media_frame->codec_frame.buffer = buffer;
	media_frame->codec_frame.size = len;  
	media_frame->type = MEDIA_FRAME_TYPE_AUDIO;


	/* push audio to queue */
	if (switch_queue_trypush(tts_session->audio_queue, (void *) media_frame)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "could not push audio to queue\n");
		return MRCP_STATUS_FAILURE;
	}

	return MRCP_STATUS_SUCCESS;

}


/** 
 * Called back by openmcp client thread every time its ready for more audio to send
 * the recognition server we are connected to.  Reads data that was put into a 
 * shared fifo queue upon receiving audio frames from asr_feed()
 */
static apr_status_t openmrcp_recognizer_read_frame(audio_source_t *source, media_frame_t *frame)
{
	asr_session_t *asr_session = source->object;
	frame->type = MEDIA_FRAME_TYPE_NONE;
	apr_status_t result;
	media_frame_t *queue_frame = NULL;

	/* pop next media frame data from incoming queue into frame */
	if(asr_session->audio_queue) {
		if (switch_queue_trypop(asr_session->audio_queue, (void *) &queue_frame)) {
			// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "could not pop from queue\n");
			result = MRCP_STATUS_FAILURE;
		}
		else {
			frame->codec_frame.size = queue_frame->codec_frame.size; 
			frame->codec_frame.buffer = queue_frame->codec_frame.buffer;
			frame->type = MEDIA_FRAME_TYPE_AUDIO;
			result = APR_SUCCESS;
		}
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "no audio queue\n");
		result = MRCP_STATUS_FAILURE;
	}

	return result;
}

/** Read in the grammar and construct an MRCP Recognize message that has
    The grammar attached as the payload */
static mrcp_status_t openmrcp_recog_start(mrcp_client_context_t *context, asr_session_t *asr_session, char *path)
{

	mrcp_generic_header_t *generic_header;
	apr_status_t rv;
	apr_file_t *fp;
	apr_pool_t *mp;
	apr_finfo_t finfo;
	char *buf1;
	apr_size_t bytes2read = 0;
	
	mrcp_message_t *mrcp_message = mrcp_client_context_message_get(context, asr_session->client_session, asr_session->channel, RECOGNIZER_RECOGNIZE);

	if(!mrcp_message) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not create mrcp msg\n");
		return MRCP_STATUS_FAILURE;
	}

	/* open the file with the grammar and read into char* buffer */
	mp = mrcp_message->pool;
	if ((rv = apr_file_open(&fp, path, APR_READ, APR_OS_DEFAULT, mp)) != APR_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not read grammar\n");
	    return -1;
	}
	rv = apr_file_info_get(&finfo, APR_FINFO_NORM, fp);
	
	buf1 = apr_palloc(mp, finfo.size);
	bytes2read = finfo.size;
	rv = apr_file_read(fp, buf1, &bytes2read);
	generic_header = mrcp_generic_header_prepare(mrcp_message);
	if(!generic_header) {
	    return MRCP_STATUS_FAILURE;
	}

	generic_header->content_type = mrcp_str_pdup(mrcp_message->pool,"application/srgs+xml");
	mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_CONTENT_TYPE);
	mrcp_message->body = mrcp_str_pdup(mrcp_message->pool,buf1);

	/* send the MRCP RECOGNIZE message to MRCP server */
	return mrcp_client_context_channel_modify(context, asr_session->client_session, mrcp_message);

}


/**
 * Freeswitch calls this from switch_ivr_detect_speech() and then adds a media
 * bug to tap into the channel's audio, which will result in all data getting
 * passed to asr_feed() and calls to asr_check_results() on each recevied frame.
 * 
 * This code expects certain one-time initialization of the openmrcp client
 * engine/systeme to have already taken place.function to open the asr interface 
 */
static switch_status_t openmrcp_asr_open(switch_asr_handle_t *ah, char *codec, int rate, char *dest, switch_asr_flag_t *flags) 
{
	mrcp_client_context_t *asr_client_context = openmrcp_module.asr_client_context ;
	asr_session_t *asr_session;
	audio_source_t *source;
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "asr_open called, codec: %s, rate: %d\n", codec, rate);

	/*! 
	  NOTE: According to the current FS media bugs design, the media bug can only feed audio
	  data in SLIN (L16) format.  So we dont need to worry about other codecs.

	  NOTE: forcing MRCP to use 20 as the CODEC_FRAME_TIME_BASE effectively ensures
	  that it matches with 16-bit audio at 8kz with 320 byte frames.  in testing, leaving
	  CODEC_FRAME_TIME_BASE at 10 and using pop (instead of trypop) in 
	  openmrcp_recognizer_read_frame() actually produces clean audio, however it causes 
      other problems as the full channel/session cleanup never completes in openmrcp, most 
	  likely due to a thread being blocked on a pop call from audio queue.  but with trypop 
	  (to avoid the cleanup problem), it only produces clean audio when CODEC_FRAME_TIME_BASE 
	  is set to 20. 
	 */
	if (strcmp(codec,"L16")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Sorry, only L16 codec supported\n");
		return SWITCH_STATUS_GENERR;		
	}
	if (rate != 8000) {
		// TODO: look into supporting other sample rates
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Sorry, only 8kz supported\n");
		return SWITCH_STATUS_GENERR;		
	}
	if (CODEC_FRAME_TIME_BASE != 20) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "You must recompile openmrcp with #define CODEC_FRAME_TIME_BASE 20\n");
		return SWITCH_STATUS_GENERR;				
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CODEC_FRAME_TIME_BASE: %d\n", CODEC_FRAME_TIME_BASE);
	}

	/* create session */
	asr_session = asr_session_create();
	if (!asr_session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "asr_session creation FAILED\n");
		return SWITCH_STATUS_GENERR;
	}
	asr_session->client_session = mrcp_client_context_session_create(asr_client_context,asr_session);
	if (!asr_session->client_session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "asr_session creation FAILED\n");
		return SWITCH_STATUS_GENERR;
	}

	/* create audio source */
	source = mrcp_palloc(asr_session->pool,sizeof(audio_source_t)); 
	source->method_set = &audio_source_method_set;
	source->object = asr_session;
	asr_session->source = source;
	
	/**
	 * create a new fifo queue.  incoming audio received from freeswitch
	 * will be put into this queue, and it will be later pulled out by 
	 * the openmrcp client thread.
	 */
	if (switch_queue_create(&asr_session->audio_queue, 10000, asr_session->pool)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "audio queue creation failed\n");
		return SWITCH_STATUS_MEMERR;
	}
	
	asr_session->flags = *flags;
	switch_mutex_init(&asr_session->flag_mutex, SWITCH_MUTEX_NESTED, asr_session->pool);

	ah->private_info = asr_session;

	return SWITCH_STATUS_SUCCESS;

}

/* function to load a grammar to the asr interface */
static switch_status_t openmrcp_asr_load_grammar(switch_asr_handle_t *ah, char *grammar, char *path)
{
	/** Read grammar from path and create and send and MRCP RECOGNIZE msg
	    that has the grammar attached to body.   
	
	    TODO: - how does DEFINE-GRAMMAR fit into the picture here?  (if at all) 
	*/
	
	asr_session_t *asr_session = (asr_session_t *) ah->private_info;
	mrcp_client_context_t *asr_client_context = openmrcp_module.asr_client_context;
	audio_source_t *source = asr_session->source;
		
	/* create recognizer channel, also starts outgoing rtp media */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Loading grammar\n");

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Create Recognizer Channel\n");
	asr_session->channel = mrcp_client_recognizer_channel_create(asr_client_context, asr_session->client_session, source);

	if (!asr_session->channel) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create recognizer channel\n");
		return SWITCH_STATUS_FALSE;
	}
	
	/* wait for recognizer channel creation */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "WAITING FOR CHAN CREATE\n");
	if(wait_for_event(asr_session->event_queue, OPENMRCP_EVENT_CHANNEL_CREATE, OPENMRCP_WAIT_TIMEOUT) == MRCP_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Got channel creation event\n");
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "WAITING FOR NONE\n");
		if (wait_for_event(asr_session->event_queue,OPENMRCP_EVENT_NONE,200) == MRCP_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "GOT NONE EVENT\n");
		}
		else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "NEVER GOT NONE EVENT\n");
		}
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Start Recognizer\n");
		openmrcp_recog_start(asr_client_context, asr_session, path);
		
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "never got channel creation event\n");
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Finished loading grammar\n");
	return SWITCH_STATUS_SUCCESS;
}


/*! function to feed audio to the ASR*/
static switch_status_t openmrcp_asr_feed(switch_asr_handle_t *ah, void *data, unsigned int len, switch_asr_flag_t *flags)
{

	asr_session_t *asr_session = (asr_session_t *) ah->private_info;
	media_frame_t *media_frame;
	switch_byte_t *buffer;
	
	/* create new media frame */
	media_frame = (media_frame_t *) switch_core_alloc(asr_session->pool, sizeof(media_frame_t));
	if (!media_frame) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "media_frame creation failed\n");
		return SWITCH_STATUS_MEMERR;
	}
	
	/**
	 * since *data buffer might get freed by caller (true or false?), allocate a
	 * new buffer and copy *data into it.
	 *
	 * MAJOR DESIGN ISSUE!!  It is way too expensive to be calling malloc on every audio frame,
	 * this needs to send the packet directly.  OpenMrcp will need a way to disable their own
	 * timer so that the fs timer is the only one driving the media.
	 **/
	buffer = (switch_byte_t *) switch_core_alloc(asr_session->pool, sizeof(switch_byte_t)*len);
	if (!buffer) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate buffer\n");
		return SWITCH_STATUS_MEMERR;
	}
	buffer = memcpy(buffer, data, len);
	
	media_frame->codec_frame.buffer = buffer;
	media_frame->codec_frame.size = len;  
	media_frame->type = MEDIA_FRAME_TYPE_AUDIO;
	
	/* push audio to queue */
	if (switch_queue_trypush(asr_session->audio_queue, (void *) media_frame)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "could not push audio to queue\n");
		return SWITCH_STATUS_GENERR;
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t openmrcp_asr_pause(switch_asr_handle_t *ah)
{

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "openmrcp_asr_pause called\n");

	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t openmrcp_asr_resume(switch_asr_handle_t *ah)
{

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "openmrcp_asr_resume called\n");

	return SWITCH_STATUS_SUCCESS;
}


/*! function to unload a grammar to the asr interface */
static switch_status_t openmrcp_asr_unload_grammar(switch_asr_handle_t *ah, char *grammar)
{

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Freeswitch calls this whenever the channel is hungup or the
 * speech detection is stopped via a call to switch_ivr_stop_detect_speech()
 */
static switch_status_t openmrcp_asr_close(switch_asr_handle_t *ah, switch_asr_flag_t *flags)
{
	asr_session_t *asr_session = (asr_session_t *) ah->private_info;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "openmrcp_asr_close()\n");

	mrcp_client_context_t *context = openmrcp_module.asr_client_context;

	// TODO!! should we do a switch_pool_clear(switch_memory_pool_t *p) on the pool held
	// by asr_session?

	// destroy channel
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Going to DESTROY CHANNEL\n");
	mrcp_client_context_channel_destroy(context, asr_session->client_session, asr_session->channel);
	if (wait_for_event(asr_session->event_queue,OPENMRCP_EVENT_CHANNEL_DESTROY,10000) == MRCP_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "OPENMRCP_EVENT_CHANNEL_DESTROY received\n");
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "timed out waiting for OPENMRCP_EVENT_CHANNEL_DESTROY\n");
	}

	// terminate client session
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Going to TERMINATE SESSION\n");
	mrcp_client_context_session_terminate(context, asr_session->client_session);
	if (wait_for_event(asr_session->event_queue,OPENMRCP_EVENT_SESSION_TERMINATE,10000) == MRCP_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "OPENMRCP_EVENT_SESSION_TERMINATE recevied\n");
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "timed out waiting for OPENMRCP_EVENT_SESSION_TERMINATE\n");
	}
	
	// destroy client session (NOTE: this sends a BYE to the other side)
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Going to DESTROY SESSION\n");
	mrcp_client_context_session_destroy(context, asr_session->client_session);

	// destroys the asr_session struct
	asr_session_destroy(asr_session);

	switch_set_flag(ah, SWITCH_ASR_FLAG_CLOSED);

	return SWITCH_STATUS_SUCCESS;
}


/**
 * Freeswitch calls this method from the speech_thread() thread body method
 * in switch_ivr_async.c every time a new frame is received by the media bug
 * attached to the audio channel.  If this method returns SWITCH_STATUS_SUCCESS,
 * then Freeswitch will call openmrcp_asr_get_results() to get the result value.
 */
static switch_status_t openmrcp_asr_check_results(switch_asr_handle_t *ah, switch_asr_flag_t *flags)
{
	asr_session_t *asr_session = (asr_session_t *) ah->private_info;
	
	switch_status_t rv = (switch_test_flag(asr_session, FLAG_HAS_TEXT) || switch_test_flag(asr_session, FLAG_BARGE)) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
	
	return rv;
}

/*! This will be called after asr_check_results returns SUCCESS */
static switch_status_t openmrcp_asr_get_results(switch_asr_handle_t *ah, char **xmlstr, switch_asr_flag_t *flags)
{
	asr_session_t *asr_session = (asr_session_t *) ah->private_info;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "openmrcp_asr_get_results called\n");
	switch_status_t ret = SWITCH_STATUS_SUCCESS;

	if (switch_test_flag(asr_session, FLAG_BARGE)) {
		switch_clear_flag_locked(asr_session, FLAG_BARGE);
		ret = SWITCH_STATUS_BREAK;
	}
	
	if (switch_test_flag(asr_session, FLAG_HAS_TEXT)) {
		/*! 
		   we have to extract the XML but stripping off the <?xml version="1.0"?>
		   header.  the body looks like:
		
		   Completion-Cause:001 no-match
		   Content-Type: application/nlsml+xml
		   Content-Length: 260
		 
		  <?xml version="1.0"?>
          <result xmlns="http://www.ietf.org/xml/ns/mrcpv2" xmlns:ex="http://www.example.com/example" score="100" grammar="session:request1@form-level.store">
            <interpretation>             <input mode="speech">open a</input>
            </interpretation>
          </result>
		*/

		char *marker = "?>";  // FIXME -- lame and brittle way of doing this.  use regex or better.
		char *position = strstr(asr_session->mrcp_message_last_rcvd->body, marker);
		if (!position) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Bad result received from mrcp server: %s", asr_session->mrcp_message_last_rcvd->body);
			ret = SWITCH_STATUS_FALSE;
		}
		else {
			position += strlen(marker);
			*xmlstr = strdup(position);
		}

		// since we are returning our result here, future calls to check_results
		// should return False
		switch_clear_flag_locked(asr_session, FLAG_HAS_TEXT);
		ret = SWITCH_STATUS_SUCCESS;	
	}
	return ret;
}


static mrcp_status_t synth_speak(mrcp_client_context_t *context, tts_session_t *tts_session, char *text)
{

	//buffer = (switch_byte_t *) switch_core_alloc(asr_session->pool, sizeof(switch_byte_t)*len);
	char *text2speak;
	const char xml_head[] = 
		"<?xml version=\"1.0\"?>\r\n"
		"<speak>\r\n"
		"<paragraph>\r\n"
		"    <sentence>";

	const char xml_tail[] = "</sentence>\r\n"
		"</paragraph>\r\n"
		"</speak>\r\n";

	size_t len = sizeof(xml_head) + sizeof(text) + sizeof(xml_tail);
	text2speak = (char *) switch_core_alloc(tts_session->pool, len);
	strcat(text2speak, xml_head);
	strcat(text2speak, text);
	strcat(text2speak, xml_tail);

	mrcp_generic_header_t *generic_header;
	mrcp_message_t *mrcp_message = mrcp_client_context_message_get(context,tts_session->client_session,tts_session->channel,SYNTHESIZER_SPEAK);
	if(!mrcp_message) {
		return MRCP_STATUS_FAILURE;
	}

	generic_header = mrcp_generic_header_prepare(mrcp_message);
	if(!generic_header) {
		return MRCP_STATUS_FAILURE;
	}

	generic_header->content_type = mrcp_str_pdup(mrcp_message->pool,"application/synthesis+ssml");
	mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_CONTENT_TYPE);
	mrcp_message->body = mrcp_str_pdup(mrcp_message->pool,text2speak);

	return mrcp_client_context_channel_modify(context,tts_session->client_session,mrcp_message);
}

static mrcp_status_t synth_stop(mrcp_client_context_t *context, tts_session_t *tts_session)
{
	mrcp_message_t *mrcp_message = mrcp_client_context_message_get(context,tts_session->client_session,tts_session->channel,SYNTHESIZER_STOP);
	if(!mrcp_message) {
		return MRCP_STATUS_FAILURE;
	}

	return mrcp_client_context_channel_modify(context,tts_session->client_session,mrcp_message);
}


static switch_status_t openmrcp_tts_open(switch_speech_handle_t *sh, char *voice_name, int rate, switch_speech_flag_t *flags) 
{

	tts_session_t *tts_session;
	audio_sink_t *sink;
	mrcp_client_context_t *tts_client_context = openmrcp_module.tts_client_context ;

	/* create session */
	tts_session = tts_session_create();
	if (!tts_session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "tts_session creation FAILED\n");
		return SWITCH_STATUS_GENERR;
	}
	tts_session->client_session = mrcp_client_context_session_create(tts_client_context,tts_session);
	if (!tts_session->client_session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "tts_session creation FAILED\n");
		return SWITCH_STATUS_GENERR;
	}


	/* create audio sink */
	sink = mrcp_palloc(tts_session->pool,sizeof(audio_sink_t));
	sink->method_set = &audio_sink_method_set;
	sink->object = tts_session;
	tts_session->sink = sink;
	

	/**
	 * create a new fifo queue.  
	 */
	if (switch_queue_create(&tts_session->audio_queue, 10000, tts_session->pool)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "audio queue creation failed\n");
		return SWITCH_STATUS_MEMERR;
	}

	/* create mutex that will be used to lock audio buffer */
	switch_mutex_init(&tts_session->audio_lock, SWITCH_MUTEX_NESTED, sh->memory_pool);

	/* create audio buffer */
	if (switch_buffer_create_dynamic(&tts_session->audio_buffer, MY_BLOCK_SIZE, MY_BUF_LEN, 0) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Write Buffer Creation Failed!\n");
		return SWITCH_STATUS_MEMERR;
	}

	
	tts_session->flags = *flags;
	switch_mutex_init(&tts_session->flag_mutex, SWITCH_MUTEX_NESTED, tts_session->pool);

	sh->private_info = tts_session;

	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t openmrcp_tts_close(switch_speech_handle_t *sh, switch_speech_flag_t *flags)
{
	tts_session_t *tts_session = (tts_session_t *) sh->private_info;
	mrcp_client_context_t *tts_client_context = openmrcp_module.tts_client_context ;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "synth_stop\n");
	synth_stop(tts_client_context,tts_session); // TODO
	wait_for_event(tts_session->event_queue,OPENMRCP_EVENT_CHANNEL_MODIFY,5000);

	

	/* terminate tts session */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Terminate tts_session\n");
	mrcp_client_context_session_terminate(tts_client_context,tts_session->client_session);
	/* wait for tts session termination */
	wait_for_event(tts_session->event_queue,OPENMRCP_EVENT_SESSION_TERMINATE,10000);

	/* destroy demo session */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "destroy tts_session\n");
	mrcp_client_context_session_destroy(tts_client_context,tts_session->client_session);
	tts_session_destroy(tts_session);

	return SWITCH_STATUS_SUCCESS;	
}

static switch_status_t openmrcp_feed_tts(switch_speech_handle_t *sh, char *text, switch_speech_flag_t *flags)
{

	tts_session_t *tts_session = (tts_session_t *) sh->private_info;
	mrcp_client_context_t *tts_client_context = openmrcp_module.tts_client_context ;
	audio_sink_t *sink = tts_session->sink;

	/* create synthesizer channel */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Create Synthesizer Channel\n");
	tts_session->channel = mrcp_client_synthesizer_channel_create(tts_client_context,tts_session->client_session,sink);
	/* wait for synthesizer channel creation */
	if(wait_for_event(tts_session->event_queue,OPENMRCP_EVENT_CHANNEL_CREATE,5000) == MRCP_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Got channel create event\n");
		wait_for_event(tts_session->event_queue,OPENMRCP_EVENT_NONE,1000);  // XXX: what are we waiting for??
		/* speak */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Going to speak\n");
		synth_speak(tts_client_context,tts_session, text); 

	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Never got channel create event.\n");
		return SWITCH_STATUS_FALSE;	
	}

	return SWITCH_STATUS_SUCCESS;	
}

/**
 * Freeswitch calls this when its ready to read datalen bytes of data.
 * 
 * TODO: check the blocking flag passed in flags and act accordingly  
 *       (see mod_cepstral.c)
 */
static switch_status_t openmrcp_read_tts(switch_speech_handle_t *sh, void *data, size_t *datalen, uint32_t *rate, switch_speech_flag_t *flags)
{
	media_frame_t *queue_frame = NULL;
	tts_session_t *tts_session = (tts_session_t *) sh->private_info;
	size_t return_len=0;
	size_t amt2copy=0;
	size_t desired = *datalen;
	switch_byte_t *audiodata = (switch_byte_t *) data;

	while(return_len < desired) {

		if (switch_test_flag(tts_session, FLAG_SPEAK_COMPLETE)) {
			// tell fs we are done
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "FLAG_SPEAK_COMPLETE\n");
			return SWITCH_STATUS_BREAK;
		}

		if (switch_queue_pop(tts_session->audio_queue, (void *) &queue_frame)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "could not pop from queue\n");
			if (switch_test_flag(tts_session, FLAG_SPEAK_COMPLETE)) {
				// tell fs we are done
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "FLAG_SPEAK_COMPLETE\n");
				return SWITCH_STATUS_BREAK;
			}
			break;
		}
		else {
			if (switch_test_flag(tts_session, FLAG_SPEAK_COMPLETE)) {
				// tell fs we are done
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "FLAG_SPEAK_COMPLETE\n");
				return SWITCH_STATUS_BREAK;
			}

			if (queue_frame->codec_frame.size >= desired) {
				amt2copy = desired;
			}
			else {
				// limit the amt we copy to audiodata to be LTE datalen
				// if the queue frame has _more_, just ignore it (TODO: fix this!)
				amt2copy = queue_frame->codec_frame.size;
			}
			memcpy(audiodata, queue_frame->codec_frame.buffer, amt2copy);
			return_len += amt2copy;
			*datalen = return_len;
			audiodata += amt2copy;  // move pointer forward
			*rate = 8000;
		}

	}

	// double check we actually read something
	if (*datalen == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "no data read from buffer\n");
		return SWITCH_STATUS_FALSE;	
	}
	else if (*datalen < desired) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "return_len: (%d) < desired: (%d)\n", return_len, desired);
	}

	return SWITCH_STATUS_SUCCESS;	
}


static void openmrcp_flush_tts(switch_speech_handle_t *sh)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "flush_tts called\n");
}

static void openmrcp_text_param_tts(switch_speech_handle_t *sh, char *param, char *val)
{

}

static void openmrcp_numeric_param_tts(switch_speech_handle_t *sh, char *param, int val)
{

}

static void openmrcp_float_param_tts(switch_speech_handle_t *sh, char *param, double val)
{

}

static switch_asr_interface_t openmrcp_asr_interface = {
	/*.interface_name*/			"openmrcp",
	/*.asr_open*/				openmrcp_asr_open,
	/*.asr_load_grammar*/		openmrcp_asr_load_grammar,
	/*.asr_unload_grammar*/		openmrcp_asr_unload_grammar,
	/*.asr_close*/				openmrcp_asr_close,
	/*.asr_feed*/				openmrcp_asr_feed,
	/*.asr_resume*/				openmrcp_asr_resume,
	/*.asr_pause*/				openmrcp_asr_pause,
	/*.asr_check_results*/		openmrcp_asr_check_results,
	/*.asr_get_results*/		openmrcp_asr_get_results
};

static switch_speech_interface_t openmrcp_tts_interface = {
	/*.interface_name*/			"openmrcp",
	/*.speech_open*/ openmrcp_tts_open,
	/*.speech_close*/ openmrcp_tts_close,
	/*.speech_feed_tts*/ openmrcp_feed_tts,
	/*.speech_read_tts*/ openmrcp_read_tts,
	/*.speech_flush_tts*/ openmrcp_flush_tts,
	/*.speech_text_param_tts*/ openmrcp_text_param_tts,
	/*.speech_numeric_param_tts*/ openmrcp_numeric_param_tts,
	/*.speech_float_param_tts*/	openmrcp_float_param_tts,

};

static switch_loadable_module_interface_t openmrcp_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL,
	/*.api_interface */ NULL,
	/*.file_interface */ NULL,
	/*.speech_interface */ &openmrcp_tts_interface,
	/*.directory_interface */ NULL,
	/*.chat_interface */ NULL,
	/*.say_interface */ NULL,
	/*.asr_interface */ &openmrcp_asr_interface
};


static switch_status_t do_config(void)
{
	char *cf = "mod_openmrcp.conf";
	switch_xml_t cfg, xml, settings, param;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	memset(&globals,0,sizeof(globals));

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "asr_client_ip")) {
				globals.asr_client_ip = val;
			} else if (!strcasecmp(var, "asr_server_ip")) {
				globals.asr_server_ip = val;
			} else if (!strcasecmp(var, "asr_proto_version")) {
				globals.asr_proto_version =(uint32_t) atoi(val);
			} else if (!strcasecmp(var, "asr_client_port")) {
				globals.asr_client_port = (uint32_t) atoi(val);
			} else if (!strcasecmp(var, "asr_server_port")) {
				globals.asr_server_port = (uint32_t) atoi(val);
			} else if (!strcasecmp(var, "tts_client_ip")) {
				globals.tts_client_ip = val;
			} else if (!strcasecmp(var, "tts_server_ip")) {
				globals.tts_server_ip = val;
			} else if (!strcasecmp(var, "tts_proto_version")) {
				globals.tts_proto_version =(uint32_t) atoi(val);
			} else if (!strcasecmp(var, "tts_client_port")) {
				globals.tts_client_port = (uint32_t) atoi(val);
			} else if (!strcasecmp(var, "tts_server_port")) {
				globals.tts_server_port = (uint32_t) atoi(val);
			} else if (!strcasecmp(var, "rtp_port_min")) {
				globals.rtp_port_min = (uint32_t) atoi(val);
			} else if (!strcasecmp(var, "rtp_port_max")) {
				globals.rtp_port_max = (uint32_t) atoi(val);
			}

		}
	}

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t mrcp_init()
{
	/*!
	Perform one-time initialization of asr client library
	*/
	
	mrcp_mem_pool_t *pool;
	mrcp_client_event_handler_t *asr_event_handler;
	mrcp_client_t *asr_client;
	mrcp_client_context_t *asr_client_context;
	openmrcp_client_options_t *asr_options;
	
	pool = mrcp_global_pool_get();
	asr_options = mrcp_palloc(pool,sizeof(openmrcp_client_options_t));
	asr_event_handler = mrcp_palloc(pool,sizeof(mrcp_client_event_handler_t));
	set_default_asr_options(asr_options);
	
	asr_event_handler->on_session_initiate = asr_on_session_initiate;
	asr_event_handler->on_session_terminate = asr_on_session_terminate;
	asr_event_handler->on_channel_add = asr_on_channel_add;
	asr_event_handler->on_channel_remove = asr_on_channel_remove;
	asr_event_handler->on_channel_modify = asr_on_channel_modify;

	// create asr client context, which to must be passed to client engine 
	asr_client_context = mrcp_client_context_create(&openmrcp_module,asr_event_handler);
	if(!asr_client_context) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "asr_client_context creation failed\n");
		return SWITCH_STATUS_GENERR;
	}
	openmrcp_module.asr_client_context = asr_client_context;

	// this basically starts a thread that pulls events from the event queue
	// and handles them 
	asr_client = openmrcp_client_start(asr_options,asr_client_context);
	if(!asr_client) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "openasr_client_start FAILED\n");
		mrcp_client_context_destroy(asr_client_context);
		return SWITCH_STATUS_GENERR;
	}

	openmrcp_module.asr_client = asr_client;


	/*!
	Perform one-time initialization of tts client library
	*/
	
	mrcp_client_event_handler_t *tts_event_handler;
	mrcp_client_t *tts_client;
	mrcp_client_context_t *tts_client_context;
	openmrcp_client_options_t *tts_options;
	
	pool = mrcp_global_pool_get();
	tts_options = mrcp_palloc(pool,sizeof(openmrcp_client_options_t));
	tts_event_handler = mrcp_palloc(pool,sizeof(mrcp_client_event_handler_t));
	set_default_tts_options(tts_options);
	
	tts_event_handler->on_session_initiate = tts_on_session_initiate;
	tts_event_handler->on_session_terminate = tts_on_session_terminate;
	tts_event_handler->on_channel_add = tts_on_channel_add;
	tts_event_handler->on_channel_remove = tts_on_channel_remove;
	tts_event_handler->on_channel_modify = tts_on_channel_modify;

	// create tts client context, which to must be passed to client engine 
	tts_client_context = mrcp_client_context_create(&openmrcp_module,tts_event_handler);
	if(!tts_client_context) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "tts_client_context creation failed\n");
		return SWITCH_STATUS_GENERR;
	}
	openmrcp_module.tts_client_context = tts_client_context;

	// this basically starts a thread that pulls events from the event queue
	// and handles them 
	tts_client = openmrcp_client_start(tts_options,tts_client_context);
	if(!tts_client) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "opentts_client_start FAILED\n");
		mrcp_client_context_destroy(tts_client_context);
		return SWITCH_STATUS_GENERR;
	}

	openmrcp_module.tts_client = tts_client;



	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_openmrcp_load)
{

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &openmrcp_module_interface;

	/* read config */
	do_config();
	
	/* initialize openmrcp */
	mrcp_global_init();
	mrcp_init();

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_openmrcp_shutdown)
{
	return SWITCH_STATUS_UNLOAD;
}
