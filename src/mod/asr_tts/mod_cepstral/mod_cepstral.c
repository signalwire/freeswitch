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
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * mod_cepstral.c -- Cepstral Interface
 *
 */
#include <swift.h>
#include <switch.h>

static const char modname[] = "mod_cepstral";

static swift_engine *engine;


typedef struct {
	swift_background_t tts_stream;
	swift_port *port;
	swift_params *params;
	swift_voice *voice;
	switch_mutex_t *audio_lock;
	switch_buffer *audio_buffer;
	int done;
	int done_gen;
} cepstral_t;


/* This callback caches the audio in the buffer */
static swift_result_t write_audio(swift_event *event, swift_event_t type, void *udata)
{
	cepstral_t *cepstral;
    swift_event_t rv = SWIFT_SUCCESS;
    void *buf = NULL;
    int len = 0;
	int wrote;
	
	cepstral = udata;
	assert(cepstral != NULL);
	
	/* Only proceed when we have success */
    if (!SWIFT_FAILED((rv = swift_event_get_audio(event, &buf, &len)))) {
		switch_mutex_lock(cepstral->audio_lock);
		if ((wrote=switch_buffer_write(cepstral->audio_buffer, buf, len)) <= 0) {
			rv = SWIFT_UNKNOWN_ERROR;
		}
		switch_mutex_unlock(cepstral->audio_lock);
	} else {
		cepstral->done = 1;
	}

    return rv;
}

static switch_status cepstral_speech_open(switch_speech_handle *sh, char *voice_name, unsigned int rate, switch_speech_flag flags)
{
	if (flags & SWITCH_SPEECH_FLAG_ASR) {
		return SWITCH_STATUS_FALSE;
	}
	if (flags & SWITCH_SPEECH_FLAG_TTS) {
		cepstral_t *cepstral = switch_core_alloc(sh->memory_pool, sizeof(*cepstral));
		char srate[25];

		if (!cepstral) {
			return SWITCH_STATUS_MEMERR;
		}

		if (switch_buffer_create(sh->memory_pool, &cepstral->audio_buffer, SWITCH_RECCOMMENDED_BUFFER_SIZE) != SWITCH_STATUS_SUCCESS) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Write Buffer Failed!\n");
			return SWITCH_STATUS_MEMERR;
		}


		switch_mutex_init(&cepstral->audio_lock, SWITCH_MUTEX_NESTED, sh->memory_pool);


		cepstral->params = swift_params_new(NULL);
		swift_params_set_string(cepstral->params, "audio/encoding", "pcm16");
		snprintf(srate, sizeof(srate), "%d", rate);
		swift_params_set_string(cepstral->params, "audio/sampling-rate", srate);


		/* Open a Swift Port through which to make TTS calls */
		if (SWIFT_FAILED(cepstral->port = swift_port_open(engine, cepstral->params))) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Failed to open Swift Port.");
			goto all_done;
		}

		
		if (voice_name && SWIFT_FAILED(swift_port_set_voice_by_name(cepstral->port, voice_name))) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Invalid voice %s!\n", voice_name);
			voice_name = NULL;
		}

		if (!voice_name) {
			/* Find the first voice on the system */
			if ((cepstral->voice = swift_port_find_first_voice(cepstral->port, NULL, NULL)) == NULL) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Failed to find any voices!\n");
				goto all_done;
			}

			/* Set the voice found by find_first_voice() as the port's current voice */
			if ( SWIFT_FAILED(swift_port_set_voice(cepstral->port, cepstral->voice)) ) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Failed to set voice.\n");
				goto all_done;
			}
		}

		swift_port_set_callback(cepstral->port, &write_audio, SWIFT_EVENT_AUDIO, cepstral);

		sh->private_info = cepstral;
		return SWITCH_STATUS_SUCCESS;
	}

 all_done:
	return SWITCH_STATUS_FALSE;
}

static switch_status cepstral_speech_close(switch_speech_handle *sh, switch_speech_flag *flags)
{
	cepstral_t *cepstral;

	assert(sh != NULL);
	cepstral = sh->private_info;
	assert(cepstral != NULL);
	
	/* Close the Swift Port and Engine */
	if (NULL != cepstral->port) swift_port_close(cepstral->port);
	//if (NULL != cepstral->engine) swift_engine_close(cepstral->engine);

	cepstral->port = NULL;
	//cepstral->engine = NULL;
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status cepstral_speech_feed_tts(switch_speech_handle *sh, char *text, switch_speech_flag *flags)
{
	cepstral_t *cepstral;

	assert(sh != NULL);
	cepstral = sh->private_info;
	assert(cepstral != NULL);


	swift_port_speak_text(cepstral->port, text, 0, NULL, &cepstral->tts_stream, NULL); 
	//swift_port_speak_text(cepstral->port, text, 0, NULL, NULL, NULL); 

	
	return SWITCH_STATUS_FALSE;
}

static switch_status cepstral_speech_read_tts(switch_speech_handle *sh,
											  void *data,
											  unsigned int *datalen,
											  unsigned int *rate,
											  switch_speech_flag *flags) 
{
	cepstral_t *cepstral;
	size_t desired = *datalen;
	switch_status status = SWITCH_STATUS_FALSE;
	size_t used, padding = 0;

	assert(sh != NULL);
	cepstral = sh->private_info;
	assert(cepstral != NULL);

	while(!cepstral->done) {
		if (!cepstral->done_gen) {
			int check = (SWIFT_STATUS_RUNNING == swift_port_status(cepstral->port, cepstral->tts_stream));
			if (!check) {
				cepstral->done_gen = 1;
			}
		}

		used = switch_buffer_inuse(cepstral->audio_buffer);


		
		if (!used && cepstral->done_gen) {
			break;
		}
		

		/* wait for the right amount of data (unless there is no blocking flag) */
		if (used < desired) {
			if (cepstral->done_gen) {
				padding = desired - used;
				desired = used;
			}
			if (!(*flags & SWITCH_SPEECH_FLAG_BLOCKING)) {
				*datalen = 0;
				status = SWITCH_STATUS_SUCCESS;
				break;
			}
			switch_yield(1000);
			continue;
		}
		
		/* There is enough, read it and return */
		switch_mutex_lock(cepstral->audio_lock);
		*datalen = switch_buffer_read(cepstral->audio_buffer, data, desired);
		if (padding) {
			size_t x = 0;
			unsigned char *p = data;
			
			for(x = 0; x < padding; x++) {
				*(p + x) = 0;
				(*datalen)++;
			}
		}

		switch_mutex_unlock(cepstral->audio_lock);
		status = SWITCH_STATUS_SUCCESS;

		break;
	}
	
	return status;
}

const switch_speech_interface cepstral_speech_interface = {
	/*.interface_name*/			"cepstral",
	/*.speech_open*/			cepstral_speech_open,
	/*.speech_close*/			cepstral_speech_close,
	/*.speech_feed_asr*/		NULL,
	/*.speech_interpret_asr*/	NULL,
	/*.speech_feed_tts*/		cepstral_speech_feed_tts,
	/*.speech_read_tts*/		cepstral_speech_read_tts
	
};

static switch_loadable_module_interface cepstral_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL,
	/*.api_interface */ NULL,
	/*.file_interface */ NULL,
	/*.speech_interface */ &cepstral_speech_interface,
	/*.directory_interface */ NULL
};

switch_status switch_module_load(const switch_loadable_module_interface **interface, char *filename)
{

	/* Open the Swift TTS Engine */
	if ( SWIFT_FAILED(engine = swift_engine_open(NULL)) ) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Failed to open Swift Engine.");
		return SWITCH_STATUS_GENERR;
	}
	
	/* connect my internal structure to the blank pointer passed to me */
	*interface = &cepstral_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}
