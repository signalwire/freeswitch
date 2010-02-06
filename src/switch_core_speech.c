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
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Christopher M. Rienzo <chris@rienzo.net>
 *
 *
 * switch_core_speech.c -- Main Core Library (speech functions)
 *
 */

#include <switch.h>
#include "private/switch_core_pvt.h"

SWITCH_DECLARE(switch_status_t) switch_core_speech_open(switch_speech_handle_t *sh,
														const char *module_name,
														const char *voice_name,
														unsigned int rate, unsigned int interval, switch_speech_flag_t *flags, switch_memory_pool_t *pool)
{
	switch_status_t status;
	char buf[256] = "";
	char *param = NULL;

	if (!sh || !flags || zstr(module_name)) {
		return SWITCH_STATUS_FALSE;
	}

	if (strchr(module_name, ':')) {
		switch_set_string(buf, module_name);
		if ((param = strchr(buf, ':'))) {
			*param++ = '\0';
			module_name = buf;
		}
	}

	if ((sh->speech_interface = switch_loadable_module_get_speech_interface(module_name)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid speech module [%s]!\n", module_name);
		return SWITCH_STATUS_GENERR;
	}

	sh->flags = *flags;
	if (pool) {
		sh->memory_pool = pool;
	} else {
		if ((status = switch_core_new_memory_pool(&sh->memory_pool)) != SWITCH_STATUS_SUCCESS) {
			UNPROTECT_INTERFACE(sh->speech_interface);
			return status;
		}
		switch_set_flag(sh, SWITCH_SPEECH_FLAG_FREE_POOL);
	}

	sh->engine = switch_core_strdup(sh->memory_pool, module_name);
	if (param) {
		sh->param = switch_core_strdup(sh->memory_pool, param);
	}

	sh->rate = rate;
	sh->name = switch_core_strdup(sh->memory_pool, module_name);
	sh->samples = switch_samples_per_packet(rate, interval);
	sh->samplerate = rate;
	sh->native_rate = rate;

	if ((status = sh->speech_interface->speech_open(sh, voice_name, rate, flags)) == SWITCH_STATUS_SUCCESS) {
		switch_set_flag(sh, SWITCH_SPEECH_FLAG_OPEN);
	} else {
		UNPROTECT_INTERFACE(sh->speech_interface);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_speech_feed_tts(switch_speech_handle_t *sh, char *text, switch_speech_flag_t *flags)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *param_string = NULL;
	char *data = NULL;
	char *ltext = NULL;

	switch_assert(sh != NULL);

	if (zstr(text)) {
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	/* Set TTS parameters from params in the text string
	 * Params are defined as follows {name1=val1,name2=val2,name3=val3}text to speak
	 */
	ltext = strdup(text);
	data = ltext;

	/* strip leading spaces */
	while (data && *data == ' ') {
		data++;
	}
	if (zstr(data)) {
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	/* extract params */
	if (*data == '{') {
		param_string = data + 1;
		data = switch_find_end_paren(data, '{', '}');
		if (zstr(data)) {
			status = SWITCH_STATUS_FALSE;
			goto done;
		} else {
			*data = '\0';
			data++;
		}
	}

	/* set TTS params */
	if (!zstr(param_string)) {
		char *param[256] = { 0 };
		int i;
		int argc = switch_separate_string(param_string, ',', param, (sizeof(param) / sizeof(param[0])));
		for (i = 0; i < argc && param[i]; ++i) {
			char *param_pair[2] = { 0 };
			if (switch_separate_string(param[i], '=', param_pair, (sizeof(param_pair) / sizeof(param_pair[0]))) == 2) {
				switch_core_speech_text_param_tts(sh, param_pair[0], param_pair[1]);
			}
		}
	}

	status = sh->speech_interface->speech_feed_tts(sh, data, flags);

  done:

	switch_safe_free(ltext);
	return status;
}

SWITCH_DECLARE(void) switch_core_speech_flush_tts(switch_speech_handle_t *sh)
{
	switch_assert(sh != NULL);

	if (sh->speech_interface->speech_flush_tts) {
		sh->speech_interface->speech_flush_tts(sh);
	}
}

SWITCH_DECLARE(void) switch_core_speech_text_param_tts(switch_speech_handle_t *sh, char *param, const char *val)
{
	switch_assert(sh != NULL);

	if (sh->speech_interface->speech_text_param_tts) {
		sh->speech_interface->speech_text_param_tts(sh, param, val);
	}
}

SWITCH_DECLARE(void) switch_core_speech_numeric_param_tts(switch_speech_handle_t *sh, char *param, int val)
{
	switch_assert(sh != NULL);

	if (sh->speech_interface->speech_numeric_param_tts) {
		sh->speech_interface->speech_numeric_param_tts(sh, param, val);
	}
}

SWITCH_DECLARE(void) switch_core_speech_float_param_tts(switch_speech_handle_t *sh, char *param, double val)
{
	switch_assert(sh != NULL);

	if (sh->speech_interface->speech_float_param_tts) {
		sh->speech_interface->speech_float_param_tts(sh, param, val);
	}
}

SWITCH_DECLARE(switch_status_t) switch_core_speech_read_tts(switch_speech_handle_t *sh, void *data, switch_size_t *datalen, switch_speech_flag_t *flags)
{
	switch_status_t status;
	switch_size_t want, orig_len = *datalen;

	switch_assert(sh != NULL);

	want = *datalen;

  top:

	if (sh->buffer && (switch_buffer_inuse(sh->buffer) >= orig_len || switch_test_flag(sh, SWITCH_SPEECH_FLAG_DONE))) {
		if ((*datalen = switch_buffer_read(sh->buffer, data, orig_len))) {
			return SWITCH_STATUS_SUCCESS;
		}
	}

	if (switch_test_flag(sh, SWITCH_SPEECH_FLAG_DONE)) {
		switch_clear_flag(sh, SWITCH_SPEECH_FLAG_DONE);
		*datalen = 0;
		return SWITCH_STATUS_BREAK;
	}

  more:

	if ((status = sh->speech_interface->speech_read_tts(sh, data, datalen, flags)) != SWITCH_STATUS_SUCCESS) {
		switch_set_flag(sh, SWITCH_SPEECH_FLAG_DONE);
		goto top;
	}


	if (sh->native_rate && sh->samplerate && sh->native_rate != sh->samplerate) {
		if (!sh->resampler) {
			if (switch_resample_create(&sh->resampler,
									   sh->native_rate, sh->samplerate, (uint32_t) orig_len, SWITCH_RESAMPLE_QUALITY, 1) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Unable to create resampler!\n");
				return SWITCH_STATUS_GENERR;
			}
		}

		switch_resample_process(sh->resampler, data, *datalen / 2);
		if (sh->resampler->to_len < want / 2 || sh->resampler->to_len > orig_len / 2) {
			if (!sh->buffer) {
				int factor = sh->resampler->to_len * sh->samplerate / 1000;
				switch_buffer_create_dynamic(&sh->buffer, factor, factor, 0);
				switch_assert(sh->buffer);
			}
			if (!sh->dbuf || sh->dbuflen < sh->resampler->to_len * 2) {
				sh->dbuflen = sh->resampler->to_len * 2;
				sh->dbuf = switch_core_alloc(sh->memory_pool, sh->dbuflen);
			}
			switch_assert(sh->resampler->to_len <= sh->dbuflen);

			memcpy((int16_t *) sh->dbuf, sh->resampler->to, sh->resampler->to_len * 2);
			switch_buffer_write(sh->buffer, sh->dbuf, sh->resampler->to_len * 2);

			if (switch_buffer_inuse(sh->buffer) < want) {
				*datalen = want;
				goto more;
			}
			*datalen = switch_buffer_read(sh->buffer, data, orig_len);
			status = SWITCH_STATUS_SUCCESS;
		} else {
			memcpy(data, sh->resampler->to, sh->resampler->to_len * 2);
			*datalen = sh->resampler->to_len * 2;
			status = SWITCH_STATUS_SUCCESS;
		}
	}

	return status;

}


SWITCH_DECLARE(switch_status_t) switch_core_speech_close(switch_speech_handle_t *sh, switch_speech_flag_t *flags)
{
	switch_status_t status = sh->speech_interface->speech_close(sh, flags);

	if (!switch_test_flag(sh, SWITCH_SPEECH_FLAG_OPEN)) {
		return SWITCH_STATUS_FALSE;
	}

	if (sh->buffer) {
		switch_buffer_destroy(&sh->buffer);
	}

	switch_resample_destroy(&sh->resampler);

	UNPROTECT_INTERFACE(sh->speech_interface);

	if (switch_test_flag(sh, SWITCH_SPEECH_FLAG_FREE_POOL)) {
		switch_core_destroy_memory_pool(&sh->memory_pool);
	}

	switch_clear_flag(sh, SWITCH_SPEECH_FLAG_OPEN);

	return status;
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
