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
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
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

	if (!sh || !flags || switch_strlen_zero(module_name)) {
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
			return status;
		}
		switch_set_flag(sh, SWITCH_SPEECH_FLAG_FREE_POOL);
	}

	sh->engine = switch_core_strdup(sh->memory_pool, module_name);
	if (param) {
		sh->param = switch_core_strdup(sh->memory_pool, param);
	}

	sh->rate = rate;
	sh->name = switch_core_strdup(pool, module_name);
	sh->samples = switch_samples_per_packet(rate, interval);

	return sh->speech_interface->speech_open(sh, voice_name, rate, flags);
}

SWITCH_DECLARE(switch_status_t) switch_core_speech_feed_tts(switch_speech_handle_t *sh, char *text, switch_speech_flag_t *flags)
{
	switch_assert(sh != NULL);

	return sh->speech_interface->speech_feed_tts(sh, text, flags);
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

SWITCH_DECLARE(switch_status_t) switch_core_speech_read_tts(switch_speech_handle_t *sh,
															void *data, switch_size_t *datalen, uint32_t *rate, switch_speech_flag_t *flags)
{
	switch_assert(sh != NULL);

	return sh->speech_interface->speech_read_tts(sh, data, datalen, rate, flags);
}


SWITCH_DECLARE(switch_status_t) switch_core_speech_close(switch_speech_handle_t *sh, switch_speech_flag_t *flags)
{
	switch_status_t status = sh->speech_interface->speech_close(sh, flags);

	if (switch_test_flag(sh, SWITCH_SPEECH_FLAG_FREE_POOL)) {
		switch_core_destroy_memory_pool(&sh->memory_pool);
	}

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
