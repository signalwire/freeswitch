/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2020, Anthony Minessale II <anthm@freeswitch.org>
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
 * Chris Rienzo <chris@signalwire.com>
 * Seven Du <dujinfang@gmail.com>
 *
 *
 * mod_test.c -- mock module interfaces for testing
 *
 */

#include <switch.h>


SWITCH_MODULE_LOAD_FUNCTION(mod_test_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_test_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_test_runtime);
SWITCH_MODULE_DEFINITION(mod_test, mod_test_load, mod_test_shutdown, mod_test_runtime);

typedef struct {
	char *text;
	int samples;
} test_tts_t;

typedef enum {
	ASRFLAG_READY = (1 << 0),
	ASRFLAG_INPUT_TIMERS = (1 << 1),
	ASRFLAG_START_OF_SPEECH = (1 << 2),
	ASRFLAG_RETURNED_START_OF_SPEECH = (1 << 3),
	ASRFLAG_NOINPUT_TIMEOUT = (1 << 4),
	ASRFLAG_RESULT = (1 << 5),
	ASRFLAG_RETURNED_RESULT = (1 << 6)
} test_asr_flag_t;

typedef struct {
	uint32_t flags;
	const char *result_text;
	double result_confidence;
	uint32_t thresh;
	uint32_t silence_ms;
	uint32_t voice_ms;
	int no_input_timeout;
	int speech_timeout;
	switch_bool_t start_input_timers;
	switch_time_t no_input_time;
	switch_time_t speech_time;
	char *grammar;
	char *channel_uuid;
	switch_vad_t *vad;
} test_asr_t;


static void test_asr_reset(test_asr_t *context)
{
	if (context->vad) {
		switch_vad_reset(context->vad);
	}
	context->flags = 0;
	context->result_text = "agent";
	context->result_confidence = 87.3;
	switch_set_flag(context, ASRFLAG_READY);
	context->no_input_time = switch_micro_time_now();
	if (context->start_input_timers) {
		switch_set_flag(context, ASRFLAG_INPUT_TIMERS);
	}
}

static switch_status_t test_asr_open(switch_asr_handle_t *ah, const char *codec, int rate, const char *dest, switch_asr_flag_t *flags)
{
	test_asr_t *context;

	if (switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "asr_open attempt on CLOSED asr handle\n");
		return SWITCH_STATUS_FALSE;
	}

	if (!(context = (test_asr_t *) switch_core_alloc(ah->memory_pool, sizeof(*context)))) {
		return SWITCH_STATUS_MEMERR;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "codec = %s, rate = %d, dest = %s\n", codec, rate, dest);

	ah->private_info = context;
	ah->codec = switch_core_strdup(ah->memory_pool, codec);

	if (rate > 16000) {
		ah->native_rate = 16000;
	}

	context->thresh = 400;
	context->silence_ms = 700;
	context->voice_ms = 60;
	context->start_input_timers = 1;
	context->no_input_timeout = 5000;
	context->speech_timeout = 10000;

	context->vad = switch_vad_init(ah->native_rate, 1);
	switch_vad_set_mode(context->vad, -1);
	switch_vad_set_param(context->vad, "thresh", context->thresh);
	switch_vad_set_param(context->vad, "silence_ms", context->silence_ms);
	switch_vad_set_param(context->vad, "voice_ms", context->voice_ms);
	switch_vad_set_param(context->vad, "debug", 0);

	test_asr_reset(context);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t test_asr_load_grammar(switch_asr_handle_t *ah, const char *grammar, const char *name)
{
	test_asr_t *context = (test_asr_t *)ah->private_info;

	if (switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "asr_open attempt on CLOSED asr handle\n");
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_INFO, "load grammar %s\n", grammar);
	context->grammar = switch_core_strdup(ah->memory_pool, grammar);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t test_asr_unload_grammar(switch_asr_handle_t *ah, const char *name)
{
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t test_asr_close(switch_asr_handle_t *ah, switch_asr_flag_t *flags)
{
	test_asr_t *context = (test_asr_t *)ah->private_info;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Double ASR close!\n");
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_NOTICE, "ASR closing ...\n");

	if (context->vad) {
		switch_vad_destroy(&context->vad);
	}

	switch_set_flag(ah, SWITCH_ASR_FLAG_CLOSED);
	return status;
}

static switch_status_t test_asr_feed(switch_asr_handle_t *ah, void *data, unsigned int len, switch_asr_flag_t *flags)
{
	test_asr_t *context = (test_asr_t *) ah->private_info;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_vad_state_t vad_state;

	if (switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED)) {
		return SWITCH_STATUS_BREAK;
	}

	if (switch_test_flag(context, ASRFLAG_RETURNED_RESULT) && switch_test_flag(ah, SWITCH_ASR_FLAG_AUTO_RESUME)) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "Auto Resuming\n");
		test_asr_reset(context);
	}

	if (switch_test_flag(context, ASRFLAG_READY)) {
		vad_state = switch_vad_process(context->vad, (int16_t *)data, len / sizeof(uint16_t));
		if (vad_state == SWITCH_VAD_STATE_STOP_TALKING) {
			switch_set_flag(context, ASRFLAG_RESULT);
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_INFO, "Talking stopped, have result.\n");
			switch_vad_reset(context->vad);
			switch_clear_flag(context, ASRFLAG_READY);
		} else if (vad_state == SWITCH_VAD_STATE_START_TALKING) {
			switch_set_flag(context, ASRFLAG_START_OF_SPEECH);
			context->speech_time = switch_micro_time_now();
		}
	}

	return status;
}

static switch_status_t test_asr_pause(switch_asr_handle_t *ah)
{
	test_asr_t *context = (test_asr_t *) ah->private_info;

	if (switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "asr_pause attempt on CLOSED asr handle\n");
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "Pausing\n");
	context->flags = 0;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t test_asr_resume(switch_asr_handle_t *ah)
{
	test_asr_t *context = (test_asr_t *) ah->private_info;

	if (switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "asr_resume attempt on CLOSED asr handle\n");
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "Resuming\n");
	test_asr_reset(context);
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t test_asr_check_results(switch_asr_handle_t *ah, switch_asr_flag_t *flags)
{
	test_asr_t *context = (test_asr_t *) ah->private_info;

	if (switch_test_flag(context, ASRFLAG_RETURNED_RESULT) || switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED)) {
		return SWITCH_STATUS_BREAK;
	}

	if (!switch_test_flag(context, ASRFLAG_RETURNED_START_OF_SPEECH) && switch_test_flag(context, ASRFLAG_START_OF_SPEECH)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if ((!switch_test_flag(context, ASRFLAG_RESULT)) && (!switch_test_flag(context, ASRFLAG_NOINPUT_TIMEOUT))) {
		if (switch_test_flag(context, ASRFLAG_INPUT_TIMERS) && !(switch_test_flag(context, ASRFLAG_START_OF_SPEECH)) &&
				context->no_input_timeout >= 0 &&
				(switch_micro_time_now() - context->no_input_time) / 1000 >= context->no_input_timeout) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "NO INPUT TIMEOUT %" SWITCH_TIME_T_FMT "ms\n", (switch_micro_time_now() - context->no_input_time) / 1000);
			switch_set_flag(context, ASRFLAG_NOINPUT_TIMEOUT);
		} else if (switch_test_flag(context, ASRFLAG_START_OF_SPEECH) && context->speech_timeout > 0 && (switch_micro_time_now() - context->speech_time) / 1000 >= context->speech_timeout) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "SPEECH TIMEOUT %" SWITCH_TIME_T_FMT "ms\n", (switch_micro_time_now() - context->speech_time) / 1000);
			if (switch_test_flag(context, ASRFLAG_START_OF_SPEECH)) {
				switch_set_flag(context, ASRFLAG_RESULT);
			} else {
				switch_set_flag(context, ASRFLAG_NOINPUT_TIMEOUT);
			}
		}
	}

	return switch_test_flag(context, ASRFLAG_RESULT) || switch_test_flag(context, ASRFLAG_NOINPUT_TIMEOUT) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_BREAK;
}

static switch_status_t test_asr_get_results(switch_asr_handle_t *ah, char **resultstr, switch_asr_flag_t *flags)
{
	test_asr_t *context = (test_asr_t *) ah->private_info;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (switch_test_flag(context, ASRFLAG_RETURNED_RESULT) || switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_test_flag(context, ASRFLAG_RESULT)) {

		*resultstr = switch_mprintf("{\"grammar\": \"%s\", \"text\": \"%s\", \"confidence\": %f}", context->grammar, context->result_text, context->result_confidence);

		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_ERROR, "Result: %s\n", *resultstr);

		status = SWITCH_STATUS_SUCCESS;
	} else if (switch_test_flag(context, ASRFLAG_NOINPUT_TIMEOUT)) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "Result: NO INPUT\n");

		*resultstr = switch_mprintf("{\"grammar\": \"%s\", \"text\": \"\", \"confidence\": 0, \"error\": \"no_input\"}", context->grammar);

		status = SWITCH_STATUS_SUCCESS;
	} else if (!switch_test_flag(context, ASRFLAG_RETURNED_START_OF_SPEECH) && switch_test_flag(context, ASRFLAG_START_OF_SPEECH)) {
		switch_set_flag(context, ASRFLAG_RETURNED_START_OF_SPEECH);
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "Result: START OF SPEECH\n");
		status = SWITCH_STATUS_BREAK;
	} else {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_ERROR, "Unexpected call to asr_get_results - no results to return!\n");
		status = SWITCH_STATUS_FALSE;
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		switch_set_flag(context, ASRFLAG_RETURNED_RESULT);
		switch_clear_flag(context, ASRFLAG_READY);
	}

	return status;
}

static switch_status_t test_asr_start_input_timers(switch_asr_handle_t *ah)
{
	test_asr_t *context = (test_asr_t *) ah->private_info;

	if (switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "asr_start_input_timers attempt on CLOSED asr handle\n");
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "start_input_timers\n");

	if (!switch_test_flag(context, ASRFLAG_INPUT_TIMERS)) {
		switch_set_flag(context, ASRFLAG_INPUT_TIMERS);
		context->no_input_time = switch_micro_time_now();
	} else {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_INFO, "Input timers already started\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

static void test_asr_text_param(switch_asr_handle_t *ah, char *param, const char *val)
{
	test_asr_t *context = (test_asr_t *) ah->private_info;

	if (!zstr(param) && !zstr(val)) {
		int nval = atoi(val);
		double fval = atof(val);

		if (!strcasecmp("no-input-timeout", param) && switch_is_number(val)) {
			context->no_input_timeout = nval;
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "no-input-timeout = %d\n", context->no_input_timeout);
		} else if (!strcasecmp("speech-timeout", param) && switch_is_number(val)) {
			context->speech_timeout = nval;
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "speech-timeout = %d\n", context->speech_timeout);
		} else if (!strcasecmp("start-input-timers", param)) {
			context->start_input_timers = switch_true(val);
			if (context->start_input_timers) {
				switch_set_flag(context, ASRFLAG_INPUT_TIMERS);
			} else {
				switch_clear_flag(context, ASRFLAG_INPUT_TIMERS);
			}
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "start-input-timers = %d\n", context->start_input_timers);
		} else if (!strcasecmp("vad-mode", param)) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "vad-mode = %s\n", val);
			if (context->vad) switch_vad_set_mode(context->vad, nval);
		} else if (!strcasecmp("vad-voice-ms", param) && nval > 0) {
			context->voice_ms = nval;
			switch_vad_set_param(context->vad, "voice_ms", nval);
		} else if (!strcasecmp("vad-silence-ms", param) && nval > 0) {
			context->silence_ms = nval;
			switch_vad_set_param(context->vad, "silence_ms", nval);
		} else if (!strcasecmp("vad-thresh", param) && nval > 0) {
			context->thresh = nval;
			switch_vad_set_param(context->vad, "thresh", nval);
		} else if (!strcasecmp("channel-uuid", param)) {
			context->channel_uuid = switch_core_strdup(ah->memory_pool, val);
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "channel-uuid = %s\n", val);
		} else if (!strcasecmp("result", param)) {
			context->result_text = switch_core_strdup(ah->memory_pool, val);
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "result = %s\n", val);
		} else if (!strcasecmp("confidence", param) && fval >= 0.0) {
			context->result_confidence = fval;
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(context->channel_uuid), SWITCH_LOG_DEBUG, "confidence = %f\n", fval);
		}
	}
}

static switch_status_t test_speech_open(switch_speech_handle_t *sh, const char *voice_name, int rate, int channels, switch_speech_flag_t *flags)
{
	test_tts_t *context = switch_core_alloc(sh->memory_pool, sizeof(test_tts_t));
	switch_assert(context);
	context->samples = sh->samplerate;
	sh->private_info = context;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t test_speech_close(switch_speech_handle_t *sh, switch_speech_flag_t *flags)
{
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t test_speech_feed_tts(switch_speech_handle_t *sh, char *text, switch_speech_flag_t *flags)
{
	test_tts_t *context = (test_tts_t *)sh->private_info;

	if (!zstr(text)) {
		char *p = strstr(text, "silence://");

		if (p) {
			p += strlen("silence://");
			context->samples = atoi(p) * sh->samplerate / 1000;
		}

		context->text = switch_core_strdup(sh->memory_pool, text);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t test_speech_read_tts(switch_speech_handle_t *sh, void *data, switch_size_t *datalen, switch_speech_flag_t *flags)
{
	test_tts_t *context = (test_tts_t *)sh->private_info;

	if (context->samples <= 0) {
		return SWITCH_STATUS_FALSE;
	}

	if (context->samples < *datalen / 2 / sh->channels) {
		*datalen = context->samples * 2 * sh->channels;
	}

	context->samples -= *datalen / 2 / sh->channels;
	memset(data, 0, *datalen);

	return SWITCH_STATUS_SUCCESS;
}

static void test_speech_flush_tts(switch_speech_handle_t *sh)
{
}

static void test_speech_text_param_tts(switch_speech_handle_t *sh, char *param, const char *val)
{
}

static void test_speech_numeric_param_tts(switch_speech_handle_t *sh, char *param, int val)
{
}

static void test_speech_float_param_tts(switch_speech_handle_t *sh, char *param, double val)
{
}

SWITCH_MODULE_LOAD_FUNCTION(mod_test_load)
{
	switch_asr_interface_t *asr_interface;
	switch_speech_interface_t *speech_interface = NULL;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	asr_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ASR_INTERFACE);
	asr_interface->interface_name = "test";
	asr_interface->asr_open = test_asr_open;
	asr_interface->asr_load_grammar = test_asr_load_grammar;
	asr_interface->asr_unload_grammar = test_asr_unload_grammar;
	asr_interface->asr_close = test_asr_close;
	asr_interface->asr_feed = test_asr_feed;
	asr_interface->asr_resume = test_asr_resume;
	asr_interface->asr_pause = test_asr_pause;
	asr_interface->asr_check_results = test_asr_check_results;
	asr_interface->asr_get_results = test_asr_get_results;
	asr_interface->asr_start_input_timers = test_asr_start_input_timers;
	asr_interface->asr_text_param = test_asr_text_param;

	speech_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_SPEECH_INTERFACE);
	speech_interface->interface_name = "test";
	speech_interface->speech_open = test_speech_open;
	speech_interface->speech_close = test_speech_close;
	speech_interface->speech_feed_tts = test_speech_feed_tts;
	speech_interface->speech_read_tts = test_speech_read_tts;
	speech_interface->speech_flush_tts = test_speech_flush_tts;
	speech_interface->speech_text_param_tts = test_speech_text_param_tts;
	speech_interface->speech_numeric_param_tts = test_speech_numeric_param_tts;
	speech_interface->speech_float_param_tts = test_speech_float_param_tts;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_test_shutdown)
{
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_test_runtime)
{
	return SWITCH_STATUS_TERM;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
