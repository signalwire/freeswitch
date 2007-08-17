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
 * Arsen Chaloyan <achaloyan@yahoo.com>
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

#include <apr_hash.h>
#include <switch.h>
	
#define OPENMRCP_WAIT_TIMEOUT 5000
#define MY_BUF_LEN 1024 * 128
#define MY_BLOCK_SIZE MY_BUF_LEN

SWITCH_MODULE_LOAD_FUNCTION(mod_openmrcp_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_openmrcp_shutdown);
SWITCH_MODULE_DEFINITION(mod_openmrcp, mod_openmrcp_load, 
						 mod_openmrcp_shutdown, NULL);

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

typedef struct {
	char                      *name;
	openmrcp_client_options_t *mrcp_options;
	mrcp_client_t             *mrcp_client;
	mrcp_client_context_t     *mrcp_context;
} openmrcp_profile_t;

typedef struct {
	openmrcp_profile_t    *profile;
	mrcp_session_t        *client_session;
	mrcp_client_channel_t *tts_channel;
	mrcp_client_channel_t *asr_channel;
	mrcp_audio_channel_t  *audio_channel;
	switch_queue_t        *event_queue;
	mrcp_message_t        *mrcp_message_last_rcvd;
	apr_pool_t            *pool;
	switch_speech_flag_t   flags;
	switch_mutex_t        *flag_mutex;
} openmrcp_session_t;

typedef enum {
	FLAG_HAS_TEXT = (1 << 0),
	FLAG_BARGE = (1 << 1),
	FLAG_READY = (1 << 2),
	FLAG_SPEAK_COMPLETE = (1 << 3)
} mrcp_flag_t;

typedef struct {
	switch_memory_pool_t *pool;
	switch_hash_t        *profile_hash;

	openmrcp_profile_t   *asr_profile;
	openmrcp_profile_t   *tts_profile;
} openmrcp_module_t;

static openmrcp_module_t openmrcp_module;


static openmrcp_session_t* openmrcp_session_create(openmrcp_profile_t *profile)
{
	openmrcp_session_t *openmrcp_session;
	apr_pool_t *session_pool;

	if(!profile) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "no profile specified\n");
		return NULL;
	}

	if(apr_pool_create(&session_pool,NULL) != APR_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to create session_pool\n");
		return NULL;
	}

	openmrcp_session = apr_palloc(session_pool,sizeof(openmrcp_session_t));
	openmrcp_session->pool = session_pool;
	openmrcp_session->profile = profile;
	openmrcp_session->client_session = NULL;
	openmrcp_session->asr_channel = NULL;
	openmrcp_session->tts_channel = NULL;
	openmrcp_session->audio_channel = NULL;
	openmrcp_session->mrcp_message_last_rcvd = NULL;
	openmrcp_session->event_queue = NULL;

	/* create an event queue */
	if (switch_queue_create(&openmrcp_session->event_queue, 1000, openmrcp_session->pool)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,  "event queue creation failed\n");
	}

	openmrcp_session->client_session = mrcp_client_context_session_create(openmrcp_session->profile->mrcp_context,openmrcp_session);
	if (!openmrcp_session->client_session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "session creation FAILED\n");
		apr_pool_destroy(session_pool);
		return NULL;
	}

	return openmrcp_session;
}

static void openmrcp_session_destroy(openmrcp_session_t *openmrcp_session)
{
	if(openmrcp_session && openmrcp_session->pool) {
		apr_pool_destroy(openmrcp_session->pool);
	}
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


static mrcp_status_t openmrcp_session_signal_event(openmrcp_session_t *openmrcp_session, openmrcp_event_t openmrcp_event)
{
	mrcp_status_t status = MRCP_STATUS_SUCCESS;

	// allocate memory for event
	openmrcp_event_t *event2queue = (openmrcp_event_t *) switch_core_alloc(openmrcp_session->pool, sizeof(openmrcp_event_t));
	*event2queue = openmrcp_event;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Got event: %s\n", openmrcp_event_names[openmrcp_event]);

	// add it to queue
	if (switch_queue_trypush(openmrcp_session->event_queue, (void *) event2queue)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "could not push event to queue\n");
		status = SWITCH_STATUS_GENERR;
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "pushed event to queue: %s\n", openmrcp_event_names[*event2queue]);
	}

	return status;
}


static mrcp_status_t openmrcp_on_session_initiate(mrcp_client_context_t *context, mrcp_session_t *session)
{
	openmrcp_session_t *openmrcp_session = mrcp_client_context_session_object_get(session);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "on_session_initiate called\n");
	if(!openmrcp_session) {
		return MRCP_STATUS_FAILURE;
	}
	return openmrcp_session_signal_event(openmrcp_session,OPENMRCP_EVENT_SESSION_INITIATE);
}

static mrcp_status_t openmrcp_on_session_terminate(mrcp_client_context_t *context, mrcp_session_t *session)
{
	openmrcp_session_t *openmrcp_session = mrcp_client_context_session_object_get(session);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "on_session_terminate called\n");
	if(!openmrcp_session) {
		return MRCP_STATUS_FAILURE;
	}
	return openmrcp_session_signal_event(openmrcp_session,OPENMRCP_EVENT_SESSION_TERMINATE);
}

static mrcp_status_t openmrcp_on_channel_add(mrcp_client_context_t *context, mrcp_session_t *session, mrcp_client_channel_t *control_channel, mrcp_audio_channel_t *audio_channel)
{
	openmrcp_session_t *openmrcp_session = mrcp_client_context_session_object_get(session);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "on_channel_add called\n");
	if(!openmrcp_session) {
		return MRCP_STATUS_FAILURE;
	}
	openmrcp_session->audio_channel = audio_channel;
	return openmrcp_session_signal_event(openmrcp_session,OPENMRCP_EVENT_CHANNEL_CREATE);
}

static mrcp_status_t openmrcp_on_channel_remove(mrcp_client_context_t *context, mrcp_session_t *session, mrcp_client_channel_t *control_channel)
{
	openmrcp_session_t *openmrcp_session = mrcp_client_context_session_object_get(session);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "on_channel_remove called\n");
	if(!openmrcp_session) {
		return MRCP_STATUS_FAILURE;
	}
	return openmrcp_session_signal_event(openmrcp_session,OPENMRCP_EVENT_CHANNEL_DESTROY);
}

/** this is called by the mrcp core whenever an mrcp message is received from
    the other side. */
static mrcp_status_t openmrcp_on_channel_modify(mrcp_client_context_t *context, mrcp_session_t *session, mrcp_message_t *mrcp_message)
{
	openmrcp_session_t *openmrcp_session = mrcp_client_context_session_object_get(session);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_channel_modify called\n");
	if(!openmrcp_session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "!openmrcp_session\n");
		return MRCP_STATUS_FAILURE;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "mrcp msg body: %s\n", mrcp_message->body);

	if(openmrcp_session->asr_channel) {
		if (!strcmp(mrcp_message->start_line.method_name,"RECOGNITION-COMPLETE")) {
			openmrcp_session->mrcp_message_last_rcvd = mrcp_message;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "setting FLAG_HAS_TEXT\n");
			switch_set_flag_locked(openmrcp_session, FLAG_HAS_TEXT);
		}
		else if (!strcmp(mrcp_message->start_line.method_name,"START-OF-SPEECH")) {
			openmrcp_session->mrcp_message_last_rcvd = mrcp_message;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "setting FLAG_BARGE\n");
			switch_set_flag_locked(openmrcp_session, FLAG_BARGE);
		}
		else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "ignoring method: %s\n", mrcp_message->start_line.method_name);
		}
	}
	else if(openmrcp_session->tts_channel) {
		if (mrcp_message->start_line.method_name) {
			if (!strcmp(mrcp_message->start_line.method_name,"SPEAK-COMPLETE")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "setting FLAG_SPEAK_COMPLETE\n");
				switch_set_flag_locked(openmrcp_session, FLAG_SPEAK_COMPLETE);
			}
			else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "ignoring method: %s\n", mrcp_message->start_line.method_name);
			}
		}
	}
		
	return openmrcp_session_signal_event(openmrcp_session,OPENMRCP_EVENT_CHANNEL_MODIFY);
}

/** Read in the grammar and construct an MRCP Recognize message that has
    The grammar attached as the payload */
static mrcp_status_t openmrcp_recog_start(mrcp_client_context_t *context, openmrcp_session_t *asr_session, char *path)
{
	mrcp_generic_header_t *generic_header;
	apr_status_t rv;
	apr_file_t *fp;
	apr_pool_t *mp;
	apr_finfo_t finfo;
	char *buf1;
	apr_size_t bytes2read = 0;
	
	mrcp_message_t *mrcp_message = mrcp_client_context_message_get(context, asr_session->client_session, asr_session->asr_channel, RECOGNIZER_RECOGNIZE);

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
	
	buf1 = apr_palloc(mp, (apr_size_t)finfo.size);
	bytes2read = (apr_size_t)finfo.size;
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
	openmrcp_session_t *asr_session;
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "asr_open called, codec: %s, rate: %d\n", codec, rate);

	if (strcmp(codec,"L16")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Sorry, only L16 codec supported\n");
		return SWITCH_STATUS_GENERR;		
	}
	if (rate != 8000) {
		// TODO: look into supporting other sample rates
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Sorry, only 8kz supported\n");
		return SWITCH_STATUS_GENERR;		
	}
	/* create session */
	asr_session = openmrcp_session_create(openmrcp_module.asr_profile);
	if (!asr_session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "asr_session creation FAILED\n");
		return SWITCH_STATUS_GENERR;
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
	
	openmrcp_session_t *asr_session = (openmrcp_session_t *) ah->private_info;
	mrcp_client_context_t *context = asr_session->profile->mrcp_context;
		
	/* create recognizer channel, also starts outgoing rtp media */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Loading grammar\n");

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Create Recognizer Channel\n");
	asr_session->asr_channel = mrcp_client_recognizer_channel_create(context, asr_session->client_session, NULL);
	if (!asr_session->asr_channel) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create recognizer channel\n");
		return SWITCH_STATUS_FALSE;
	}

	mrcp_client_context_channel_add(context, asr_session->client_session, asr_session->asr_channel, NULL);
	
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
		openmrcp_recog_start(context, asr_session, path);
		
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
	openmrcp_session_t *asr_session = (openmrcp_session_t *) ah->private_info;
	media_frame_t media_frame;
	audio_sink_t *audio_sink = mrcp_client_audio_sink_get(asr_session->audio_channel);

	media_frame.type = MEDIA_FRAME_TYPE_AUDIO;
	/* sampling rate and frame size should be retrieved from audio sink */
	media_frame.codec_frame.size = 160;
	media_frame.codec_frame.buffer = data;
	while(len >= media_frame.codec_frame.size) {
		if (!audio_sink) {
			return SWITCH_STATUS_GENERR;
		}
		audio_sink->method_set->write_frame(audio_sink,&media_frame);
		
		len -= (unsigned int)media_frame.codec_frame.size;
		media_frame.codec_frame.buffer = (char*)media_frame.codec_frame.buffer + media_frame.codec_frame.size;
	}
	if(len > 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Not framed alligned data len [%d]\n",len);
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
	openmrcp_session_t *asr_session = (openmrcp_session_t *) ah->private_info;
	mrcp_client_context_t *context = asr_session->profile->mrcp_context;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "openmrcp_asr_close()\n");

	// TODO!! should we do a switch_pool_clear(switch_memory_pool_t *p) on the pool held
	// by asr_session?

	// destroy channel
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Going to DESTROY CHANNEL\n");
	mrcp_client_context_channel_destroy(context, asr_session->client_session, asr_session->asr_channel);
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
	openmrcp_session_destroy(asr_session);

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
	openmrcp_session_t *asr_session = (openmrcp_session_t *) ah->private_info;
	
	switch_status_t rv = (switch_test_flag(asr_session, FLAG_HAS_TEXT) || switch_test_flag(asr_session, FLAG_BARGE)) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
	
	return rv;
}

/*! This will be called after asr_check_results returns SUCCESS */
static switch_status_t openmrcp_asr_get_results(switch_asr_handle_t *ah, char **xmlstr, switch_asr_flag_t *flags)
{
	openmrcp_session_t *asr_session = (openmrcp_session_t *) ah->private_info;
	switch_status_t ret = SWITCH_STATUS_SUCCESS;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "openmrcp_asr_get_results called\n");

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


static mrcp_status_t synth_speak(mrcp_client_context_t *context, openmrcp_session_t *tts_session, char *text)
{
	mrcp_generic_header_t *generic_header;
	mrcp_message_t *mrcp_message;

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

	mrcp_message = mrcp_client_context_message_get(context,tts_session->client_session,tts_session->tts_channel,SYNTHESIZER_SPEAK);
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

static mrcp_status_t synth_stop(mrcp_client_context_t *context, openmrcp_session_t *tts_session)
{
	mrcp_message_t *mrcp_message = mrcp_client_context_message_get(context,tts_session->client_session,tts_session->tts_channel,SYNTHESIZER_STOP);
	if(!mrcp_message) {
		return MRCP_STATUS_FAILURE;
	}

	return mrcp_client_context_channel_modify(context,tts_session->client_session,mrcp_message);
}


static switch_status_t openmrcp_tts_open(switch_speech_handle_t *sh, char *voice_name, int rate, switch_speech_flag_t *flags) 
{
	openmrcp_session_t *tts_session;

	/* create session */
	tts_session = openmrcp_session_create(openmrcp_module.tts_profile);
	if (!tts_session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "tts_session creation FAILED\n");
		return SWITCH_STATUS_GENERR;
	}
	tts_session->flags = *flags;
	switch_mutex_init(&tts_session->flag_mutex, SWITCH_MUTEX_NESTED, tts_session->pool);
	
	sh->private_info = tts_session;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t openmrcp_tts_close(switch_speech_handle_t *sh, switch_speech_flag_t *flags)
{
	openmrcp_session_t *tts_session = (openmrcp_session_t *) sh->private_info;
	mrcp_client_context_t *context = tts_session->profile->mrcp_context;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "synth_stop\n");
	synth_stop(context,tts_session); // TODO
	wait_for_event(tts_session->event_queue,OPENMRCP_EVENT_CHANNEL_MODIFY,5000);

	/* terminate tts session */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Terminate tts_session\n");
	mrcp_client_context_session_terminate(context,tts_session->client_session);
	/* wait for tts session termination */
	wait_for_event(tts_session->event_queue,OPENMRCP_EVENT_SESSION_TERMINATE,10000);

	/* destroy demo session */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "destroy tts_session\n");
	mrcp_client_context_session_destroy(context,tts_session->client_session);
	openmrcp_session_destroy(tts_session);

	return SWITCH_STATUS_SUCCESS;	
}

static switch_status_t openmrcp_feed_tts(switch_speech_handle_t *sh, char *text, switch_speech_flag_t *flags)
{
	openmrcp_session_t *tts_session = (openmrcp_session_t *) sh->private_info;
	mrcp_client_context_t *context = tts_session->profile->mrcp_context;

	/* create synthesizer channel */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Create Synthesizer Channel\n");
	tts_session->tts_channel = mrcp_client_synthesizer_channel_create(context,tts_session->client_session,NULL);
	if (!tts_session->tts_channel) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create synthesizer channel\n");
		return SWITCH_STATUS_FALSE;
	}

	mrcp_client_context_channel_add(context, tts_session->client_session, tts_session->tts_channel, NULL);
	
	/* wait for synthesizer channel creation */
	if(wait_for_event(tts_session->event_queue,OPENMRCP_EVENT_CHANNEL_CREATE,5000) == MRCP_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Got channel create event\n");
		wait_for_event(tts_session->event_queue,OPENMRCP_EVENT_NONE,1000);  // XXX: what are we waiting for??
		/* speak */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Going to speak\n");
		synth_speak(context, tts_session, text); 

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
	openmrcp_session_t *tts_session = (openmrcp_session_t *) sh->private_info;
	size_t return_len=0;
	media_frame_t media_frame;
	audio_source_t *audio_source;

	if (switch_test_flag(tts_session, FLAG_SPEAK_COMPLETE)) {
		/* tell fs we are done */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "FLAG_SPEAK_COMPLETE\n");
		return SWITCH_STATUS_BREAK;
	}

	audio_source = mrcp_client_audio_source_get(tts_session->audio_channel);
	if(!audio_source) {
		return SWITCH_STATUS_BREAK;
	}

	/* sampling rate and frame size should be retrieved from audio source */
	*rate = 8000;
	media_frame.codec_frame.size = 160;
	while(return_len < *datalen) {
		media_frame.codec_frame.buffer = (char*)data + return_len;
		audio_source->method_set->read_frame(audio_source,&media_frame);
		if(media_frame.type != MEDIA_FRAME_TYPE_AUDIO) {
			memset(media_frame.codec_frame.buffer,0,media_frame.codec_frame.size);
		}
		return_len += media_frame.codec_frame.size;
	}
	*datalen = return_len;
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


static switch_status_t do_config()
{
	char *cf = "mod_openmrcp.conf";
	const char *asr_profile_name = NULL;
	const char *tts_profile_name = NULL;
	switch_xml_t cfg, xml, settings, profiles, xprofile, param;
	openmrcp_profile_t *mrcp_profile;
	openmrcp_client_options_t *mrcp_options;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			const char *var = switch_xml_attr_soft(param, "name");
			const char *val = switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "asr_default_profile")) {
				asr_profile_name = val;
			} else if (!strcasecmp(var, "tts_default_profile")) {
				tts_profile_name = val;
			}
		}
	}
	else {
		goto error;
	}
	if ((profiles = switch_xml_child(cfg, "profiles"))) {
		for (xprofile = switch_xml_child(profiles, "profile"); xprofile; xprofile = xprofile->next) {
			const char *profile_name = switch_xml_attr_soft(xprofile, "name");
			mrcp_profile = switch_core_alloc(openmrcp_module.pool,sizeof(openmrcp_profile_t));
			mrcp_profile->mrcp_client = NULL;
			mrcp_profile->mrcp_context = NULL;
			mrcp_profile->name = "noname";
			if(profile_name) {
				mrcp_profile->name = switch_core_strdup(openmrcp_module.pool,profile_name);
			}

			mrcp_options = openmrcp_client_options_create(openmrcp_module.pool);
			for (param = switch_xml_child(xprofile, "param"); param; param = param->next) {
				const char *var = switch_xml_attr_soft(param, "name");
				const char *val = switch_xml_attr_soft(param, "value");

				if (!strcasecmp(var, "proto_version")) {
					mrcp_options->proto_version =(mrcp_version_t) atoi(val);
				}
				else if (!strcasecmp(var, "client_ip")) {
					mrcp_options->client_ip = switch_core_strdup(openmrcp_module.pool,val);
				} else if (!strcasecmp(var, "server_ip")) {
					mrcp_options->server_ip = switch_core_strdup(openmrcp_module.pool,val);
				} else if (!strcasecmp(var, "client_port")) {
					mrcp_options->client_port = (apr_port_t) atoi(val);
				} else if (!strcasecmp(var, "server_port")) {
					mrcp_options->server_port = (apr_port_t) atoi(val);
				} else if (!strcasecmp(var, "rtp_port_min")) {
					mrcp_options->rtp_port_min = (apr_port_t) atoi(val);
				} else if (!strcasecmp(var, "rtp_port_max")) {
					mrcp_options->rtp_port_max = (apr_port_t) atoi(val);
				}
			}
			mrcp_profile->mrcp_options = mrcp_options;

			/* add profile */
			if (!switch_core_hash_find(openmrcp_module.profile_hash, mrcp_profile->name)) {
				switch_core_hash_insert(openmrcp_module.profile_hash, mrcp_profile->name, mrcp_profile);

				/* try to set default asr profile */
				if (!openmrcp_module.asr_profile) {
					if (asr_profile_name) {
						if (!strcasecmp(mrcp_profile->name,asr_profile_name)) {
							openmrcp_module.asr_profile = mrcp_profile;
						}
					}
					else {
						openmrcp_module.asr_profile = mrcp_profile;
					}
				}
				/* try to set default tts profile */
				if (!openmrcp_module.tts_profile) {
					if (tts_profile_name) {
						if (!strcasecmp(mrcp_profile->name,tts_profile_name)) {
							openmrcp_module.tts_profile = mrcp_profile;
						}
					}
					else {
						openmrcp_module.tts_profile = mrcp_profile;
					}
				}
			}
		}
	}
	else {
		goto error;
	}

	switch_xml_free(xml);
	return SWITCH_STATUS_SUCCESS;

 error:
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to load module configuration\n");
	switch_xml_free(xml);
	return SWITCH_STATUS_TERM;

}

static switch_status_t openmrcp_profile_run(openmrcp_profile_t *profile)
{
	mrcp_client_event_handler_t *mrcp_event_handler;
	mrcp_client_t *mrcp_client;
	mrcp_client_context_t *mrcp_context;
	
	/*!
	Perform one-time initialization of mrcp client library
	*/
	mrcp_event_handler = switch_core_alloc(openmrcp_module.pool,sizeof(mrcp_client_event_handler_t));
	mrcp_event_handler->on_session_initiate = openmrcp_on_session_initiate;
	mrcp_event_handler->on_session_terminate = openmrcp_on_session_terminate;
	mrcp_event_handler->on_channel_add = openmrcp_on_channel_add;
	mrcp_event_handler->on_channel_remove = openmrcp_on_channel_remove;
	mrcp_event_handler->on_channel_modify = openmrcp_on_channel_modify;

	// create client context, which must be passed to client engine 
	mrcp_context = mrcp_client_context_create(&openmrcp_module,mrcp_event_handler);
	if(!mrcp_context) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mrcp_client_context creation failed\n");
		return SWITCH_STATUS_GENERR;
	}
	profile->mrcp_context = mrcp_context;

	// this basically starts a thread that pulls events from the event queue
	// and handles them 
	mrcp_client = openmrcp_client_start(profile->mrcp_options,mrcp_context);
	if(!mrcp_client) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "openmrcp_client_start FAILED\n");
		mrcp_client_context_destroy(mrcp_context);
		return SWITCH_STATUS_GENERR;
	}
	profile->mrcp_client = mrcp_client;
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t openmrcp_init()
{
	openmrcp_module.pool = mrcp_global_pool_get();
	openmrcp_module.asr_profile = NULL;
	openmrcp_module.tts_profile = NULL;

	switch_core_hash_init(&openmrcp_module.profile_hash,openmrcp_module.pool);

	/* read config */
	if (do_config() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	/* run default asr/tts profiles */
	if(openmrcp_module.asr_profile) {
		openmrcp_profile_run(openmrcp_module.asr_profile);
	}
	if(openmrcp_module.tts_profile && openmrcp_module.tts_profile != openmrcp_module.asr_profile) {
		openmrcp_profile_run(openmrcp_module.tts_profile);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_openmrcp_load)
{
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &openmrcp_module_interface;

	mrcp_global_init();
	
	/* initialize openmrcp */
	if (openmrcp_init() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;		
	}

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_openmrcp_shutdown)
{
	return SWITCH_STATUS_UNLOAD;
}
