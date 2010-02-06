/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 *
 * mod_cepstral.c -- Cepstral Interface
 * 
 * Contains some material derived from the Cepstral Swift SDK, by  
 * permission.  You are free to copy and modify the source under the  
 * terms of FreeSWITCH itself, without additional permission from  
 * Cepstral
 * 
 *
 */
#ifdef __ICC
#pragma warning (disable:188)
#endif
#include <swift.h>
#include <switch.h>

#define MY_BUF_LEN 1024 * 32
#define MY_BLOCK_SIZE MY_BUF_LEN

#undef SWIFT_FAILED
#define SWIFT_FAILED(r) ((void *)(r) < (void *)0)

SWITCH_MODULE_LOAD_FUNCTION(mod_cepstral_load);
SWITCH_MODULE_DEFINITION_EX(mod_cepstral, mod_cepstral_load, NULL, NULL, SMODF_GLOBAL_SYMBOLS);

static swift_engine *engine;


typedef struct {
	swift_background_t tts_stream;
	swift_port *port;
	swift_params *params;
	swift_voice *voice;
	switch_mutex_t *audio_lock;
	switch_buffer_t *audio_buffer;
	int done;
	int done_gen;
} cepstral_t;


/* This callback caches the audio in the buffer */
static swift_result_t write_audio(swift_event * event, swift_event_t type, void *udata)
{
	cepstral_t *cepstral;
	swift_event_t rv = SWIFT_SUCCESS;
	void *buf = NULL;
	int len = 0, i = 0;

	cepstral = udata;
	assert(cepstral != NULL);

	if (!cepstral->port || cepstral->done || cepstral->done_gen) {
		return SWIFT_UNKNOWN_ERROR;
	}

	/* Only proceed when we have success */
	if (!SWIFT_FAILED((rv = swift_event_get_audio(event, &buf, &len)))) {
		while (!cepstral->done) {
			switch_mutex_lock(cepstral->audio_lock);
			if (switch_buffer_write(cepstral->audio_buffer, buf, len) > 0) {
				switch_mutex_unlock(cepstral->audio_lock);
				break;
			}
			switch_mutex_unlock(cepstral->audio_lock);
			if (!cepstral->done) {
				for (i = 0; i < 10; i++) {
					switch_yield(10000);
					if (cepstral->done) {
						break;
					}
				}
			}

		}
	} else {
		cepstral->done = 1;
	}

	if (cepstral->done) {
		rv = SWIFT_UNKNOWN_ERROR;
	}

	return rv;
}

static switch_status_t cepstral_speech_open(switch_speech_handle_t *sh, const char *voice_name, int rate, switch_speech_flag_t *flags)
{
	cepstral_t *cepstral = switch_core_alloc(sh->memory_pool, sizeof(*cepstral));
	char srate[25];

	if (!cepstral) {
		return SWITCH_STATUS_MEMERR;
	}

	if (switch_buffer_create_dynamic(&cepstral->audio_buffer, MY_BLOCK_SIZE, MY_BUF_LEN, 0) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Write Buffer Failed!\n");
		return SWITCH_STATUS_MEMERR;
	}


	switch_mutex_init(&cepstral->audio_lock, SWITCH_MUTEX_NESTED, sh->memory_pool);


	cepstral->params = swift_params_new(NULL);
	swift_params_set_string(cepstral->params, "audio/encoding", "pcm16");
	switch_snprintf(srate, sizeof(srate), "%d", rate);
	swift_params_set_string(cepstral->params, "audio/sampling-rate", srate);


	/* Open a Swift Port through which to make TTS calls */
	if (!(cepstral->port = swift_port_open(engine, cepstral->params))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to open Swift Port.\n");
		goto all_done;
	}


	if (voice_name && SWIFT_FAILED(swift_port_set_voice_by_name(cepstral->port, voice_name))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid voice %s!\n", voice_name);
		voice_name = NULL;
	}

	if (zstr(voice_name)) {
		/* Find the first voice on the system */
		if ((cepstral->voice = swift_port_find_first_voice(cepstral->port, NULL, NULL)) == NULL) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to find any voices!\n");
			goto all_done;
		}

		/* Set the voice found by find_first_voice() as the port's current voice */
		if (SWIFT_FAILED(swift_port_set_voice(cepstral->port, cepstral->voice))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to set voice.\n");
			goto all_done;
		}

		voice_name = (char *) swift_voice_get_attribute(cepstral->voice, "name");
	}

	if (voice_name) {
		switch_copy_string(sh->voice, voice_name, sizeof(sh->voice));
	}

	swift_port_set_callback(cepstral->port, &write_audio, SWIFT_EVENT_AUDIO, cepstral);

	sh->private_info = cepstral;
	return SWITCH_STATUS_SUCCESS;

  all_done:
	return SWITCH_STATUS_FALSE;
}

static switch_status_t cepstral_speech_close(switch_speech_handle_t *sh, switch_speech_flag_t *flags)
{
	cepstral_t *cepstral;

	assert(sh != NULL);
	cepstral = sh->private_info;
	assert(cepstral != NULL);


	cepstral->done = 1;
	cepstral->done_gen = 1;
	swift_port_stop(cepstral->port, SWIFT_ASYNC_ANY, SWIFT_EVENT_NOW);
	/* Close the Swift Port and Engine */
	if (NULL != cepstral->port)
		swift_port_close(cepstral->port);
	//if (NULL != cepstral->engine) swift_engine_close(cepstral->engine);

	cepstral->port = NULL;
	//cepstral->engine = NULL;

	switch_buffer_destroy(&cepstral->audio_buffer);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t cepstral_speech_feed_tts(switch_speech_handle_t *sh, char *text, switch_speech_flag_t *flags)
{
	cepstral_t *cepstral;
	const char *fp = "file:";
	int len = (int) strlen(fp);

	assert(sh != NULL);
	cepstral = sh->private_info;
	assert(cepstral != NULL);

	cepstral->done_gen = 0;
	cepstral->done = 0;

	cepstral->tts_stream = NULL;

	if (zstr(text)) {
		return SWITCH_STATUS_FALSE;
	}
	if (!strncasecmp(text, fp, len)) {
		text += len;
		if (zstr(text)) {
			return SWITCH_STATUS_FALSE;
		}
		swift_port_speak_file(cepstral->port, text, NULL, &cepstral->tts_stream, NULL);
	} else {
		char *to_say;
		if (zstr(text)) {
			return SWITCH_STATUS_FALSE;
		}

		if ((to_say = switch_mprintf("<break time=\"1000ms\"/> %s <break time=\"1000ms\"/>", text))) {
			swift_port_speak_text(cepstral->port, to_say, 0, NULL, &cepstral->tts_stream, NULL);
			switch_safe_free(to_say);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static void cepstral_speech_flush_tts(switch_speech_handle_t *sh)
{
	cepstral_t *cepstral;

	cepstral = sh->private_info;
	assert(cepstral != NULL);

	cepstral->done_gen = 1;
	cepstral->done = 1;
	if (cepstral->audio_buffer) {
		switch_mutex_lock(cepstral->audio_lock);
		switch_buffer_zero(cepstral->audio_buffer);
		switch_mutex_unlock(cepstral->audio_lock);
	}
	swift_port_stop(cepstral->port, SWIFT_ASYNC_ANY, SWIFT_EVENT_NOW);
}

static switch_status_t cepstral_speech_read_tts(switch_speech_handle_t *sh, void *data, size_t *datalen, switch_speech_flag_t *flags)
{
	cepstral_t *cepstral;
	size_t desired = *datalen;
	switch_status_t status = SWITCH_STATUS_FALSE;
	size_t used, padding = 0;

	assert(sh != NULL);
	cepstral = sh->private_info;
	assert(cepstral != NULL);

	while (!cepstral->done) {
		if (!cepstral->done_gen) {
			int check = swift_port_status(cepstral->port, cepstral->tts_stream);

			if (!check == SWIFT_STATUS_RUNNING) {
				cepstral->done_gen = 1;
			}
		}

		switch_mutex_lock(cepstral->audio_lock);
		used = switch_buffer_inuse(cepstral->audio_buffer);
		switch_mutex_unlock(cepstral->audio_lock);


		if (!used && cepstral->done_gen) {

			status = SWITCH_STATUS_BREAK;
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
			switch_cond_next();
			continue;
		}

		/* There is enough, read it and return */
		switch_mutex_lock(cepstral->audio_lock);
		*datalen = switch_buffer_read(cepstral->audio_buffer, data, desired);
		if (padding) {
			size_t x = 0;
			unsigned char *p = data;

			for (x = 0; x < padding; x++) {
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

static void cepstral_text_param_tts(switch_speech_handle_t *sh, char *param, const char *val)
{
	cepstral_t *cepstral;

	cepstral = sh->private_info;
	assert(cepstral != NULL);

	if (!strcasecmp(param, "voice")) {
		const char *voice_name = val;
		if (!strcasecmp(voice_name, "next")) {
			if ((cepstral->voice = swift_port_find_next_voice(cepstral->port))) {
				if (SWIFT_FAILED(swift_port_set_voice(cepstral->port, cepstral->voice))) {
					cepstral->done = cepstral->done_gen = 1;
					return;
				}
				voice_name = swift_voice_get_attribute(cepstral->voice, "name");
			} else {
				voice_name = NULL;
			}
		} else {
			if (voice_name && SWIFT_FAILED(swift_port_set_voice_by_name(cepstral->port, voice_name))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid voice %s!\n", voice_name);
				voice_name = NULL;
			}
		}

		if (!voice_name) {
			/* Find the first voice on the system */
			if ((cepstral->voice = swift_port_find_first_voice(cepstral->port, NULL, NULL)) == NULL) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to find any voices!\n");
				cepstral->done = cepstral->done_gen = 1;
				return;
			}

			/* Set the voice found by find_first_voice() as the port's current voice */
			if (SWIFT_FAILED(swift_port_set_voice(cepstral->port, cepstral->voice))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to set voice.\n");
				cepstral->done = cepstral->done_gen = 1;
				return;
			}

			voice_name = swift_voice_get_attribute(cepstral->voice, "name");
		}

		if (voice_name) {
			switch_copy_string(sh->voice, voice_name, sizeof(sh->voice));
		}

		return;
	}

	swift_port_set_param_string(cepstral->port, param, val, NULL);
}

static void cepstral_numeric_param_tts(switch_speech_handle_t *sh, char *param, int val)
{
	cepstral_t *cepstral;

	cepstral = sh->private_info;
	assert(cepstral != NULL);

	swift_port_set_param_int(cepstral->port, param, val, NULL);

}


static void cepstral_float_param_tts(switch_speech_handle_t *sh, char *param, double val)
{
	cepstral_t *cepstral;

	cepstral = sh->private_info;
	assert(cepstral != NULL);

	swift_port_set_param_float(cepstral->port, param, val, NULL);

}

SWITCH_MODULE_LOAD_FUNCTION(mod_cepstral_load)
{
	switch_speech_interface_t *speech_interface;

	/* Open the Swift TTS Engine */
	if (!(engine = swift_engine_open(NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to open Swift Engine.");
		return SWITCH_STATUS_GENERR;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	speech_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_SPEECH_INTERFACE);
	speech_interface->interface_name = "cepstral";
	speech_interface->speech_open = cepstral_speech_open;
	speech_interface->speech_close = cepstral_speech_close;
	speech_interface->speech_feed_tts = cepstral_speech_feed_tts;
	speech_interface->speech_read_tts = cepstral_speech_read_tts;
	speech_interface->speech_flush_tts = cepstral_speech_flush_tts;
	speech_interface->speech_text_param_tts = cepstral_text_param_tts;
	speech_interface->speech_numeric_param_tts = cepstral_numeric_param_tts;
	speech_interface->speech_float_param_tts = cepstral_float_param_tts;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
