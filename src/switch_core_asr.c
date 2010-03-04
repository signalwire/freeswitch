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
 * switch_core_asr.c -- Main Core Library (Speech Detection Interface)
 *
 */

#include <switch.h>
#include "private/switch_core_pvt.h"

SWITCH_DECLARE(switch_status_t) switch_core_asr_open(switch_asr_handle_t *ah,
													 const char *module_name,
													 const char *codec, int rate, const char *dest, switch_asr_flag_t *flags, switch_memory_pool_t *pool)
{
	switch_status_t status;
	char buf[256] = "";
	char *param = NULL;

	if (strchr(module_name, ':')) {
		switch_set_string(buf, module_name);
		if ((param = strchr(buf, ':'))) {
			*param++ = '\0';
			module_name = buf;
		}
	}

	switch_assert(ah != NULL);

	if ((ah->asr_interface = switch_loadable_module_get_asr_interface(module_name)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid ASR module [%s]!\n", module_name);
		return SWITCH_STATUS_GENERR;
	}

	ah->flags = *flags;

	if (pool) {
		ah->memory_pool = pool;
	} else {
		if ((status = switch_core_new_memory_pool(&ah->memory_pool)) != SWITCH_STATUS_SUCCESS) {
			UNPROTECT_INTERFACE(ah->asr_interface);
			return status;
		}
		switch_set_flag(ah, SWITCH_ASR_FLAG_FREE_POOL);
	}

	if (param) {
		ah->param = switch_core_strdup(ah->memory_pool, param);
	}
	ah->rate = rate;
	ah->name = switch_core_strdup(ah->memory_pool, module_name);

	status = ah->asr_interface->asr_open(ah, codec, rate, dest, flags);

	if (status != SWITCH_STATUS_SUCCESS) {
		UNPROTECT_INTERFACE(ah->asr_interface);
	}

	return status;

}

SWITCH_DECLARE(switch_status_t) switch_core_asr_load_grammar(switch_asr_handle_t *ah, const char *grammar, const char *name)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *param_string = NULL;
	char *data = NULL;
	char *lgrammar = NULL;

	switch_assert(ah != NULL);

	if (zstr(grammar)) {
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	/* Set ASR parameters from params in the grammar string
	 * Params are defined as follows {name1=val1,name2=val2,name3=val3}grammar text
	 */
	lgrammar = strdup(grammar);
	data = lgrammar;

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

	/* set ASR params */
	if (!zstr(param_string)) {
		char *param[256] = { 0 };
		int i;
		int argc = switch_separate_string(param_string, ',', param, (sizeof(param) / sizeof(param[0])));
		for (i = 0; i < argc && param[i]; ++i) {
			char *param_pair[2] = { 0 };
			if (switch_separate_string(param[i], '=', param_pair, (sizeof(param_pair) / sizeof(param_pair[0]))) == 2) {
				switch_core_asr_text_param(ah, param_pair[0], param_pair[1]);
			}
		}
	}

	status = ah->asr_interface->asr_load_grammar(ah, data, name);

  done:

	switch_safe_free(lgrammar);
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_asr_unload_grammar(switch_asr_handle_t *ah, const char *name)
{
	switch_status_t status;

	switch_assert(ah != NULL);
	status = ah->asr_interface->asr_unload_grammar(ah, name);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_asr_pause(switch_asr_handle_t *ah)
{
	switch_assert(ah != NULL);

	return ah->asr_interface->asr_pause(ah);
}

SWITCH_DECLARE(switch_status_t) switch_core_asr_resume(switch_asr_handle_t *ah)
{
	switch_assert(ah != NULL);

	return ah->asr_interface->asr_resume(ah);
}

SWITCH_DECLARE(switch_status_t) switch_core_asr_close(switch_asr_handle_t *ah, switch_asr_flag_t *flags)
{
	switch_status_t status;

	switch_assert(ah != NULL);

	status = ah->asr_interface->asr_close(ah, flags);
	switch_set_flag(ah, SWITCH_ASR_FLAG_CLOSED);

	UNPROTECT_INTERFACE(ah->asr_interface);

	if (switch_test_flag(ah, SWITCH_ASR_FLAG_FREE_POOL)) {
		switch_core_destroy_memory_pool(&ah->memory_pool);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_asr_feed(switch_asr_handle_t *ah, void *data, unsigned int len, switch_asr_flag_t *flags)
{
	switch_assert(ah != NULL);

	return ah->asr_interface->asr_feed(ah, data, len, flags);
}

SWITCH_DECLARE(switch_status_t) switch_core_asr_check_results(switch_asr_handle_t *ah, switch_asr_flag_t *flags)
{
	switch_assert(ah != NULL);

	return ah->asr_interface->asr_check_results(ah, flags);
}

SWITCH_DECLARE(switch_status_t) switch_core_asr_get_results(switch_asr_handle_t *ah, char **xmlstr, switch_asr_flag_t *flags)
{
	switch_assert(ah != NULL);

	return ah->asr_interface->asr_get_results(ah, xmlstr, flags);
}

SWITCH_DECLARE(switch_status_t) switch_core_asr_start_input_timers(switch_asr_handle_t *ah)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_assert(ah != NULL);

	if (ah->asr_interface->asr_start_input_timers) {
		status = ah->asr_interface->asr_start_input_timers(ah);
	}

	return status;
}

SWITCH_DECLARE(void) switch_core_asr_text_param(switch_asr_handle_t *ah, char *param, const char *val)
{
	switch_assert(ah != NULL);

	if (ah->asr_interface->asr_text_param) {
		ah->asr_interface->asr_text_param(ah, param, val);
	}
}

SWITCH_DECLARE(void) switch_core_asr_numeric_param(switch_asr_handle_t *ah, char *param, int val)
{
	switch_assert(ah != NULL);

	if (ah->asr_interface->asr_numeric_param) {
		ah->asr_interface->asr_numeric_param(ah, param, val);
	}
}

SWITCH_DECLARE(void) switch_core_asr_float_param(switch_asr_handle_t *ah, char *param, double val)
{
	switch_assert(ah != NULL);

	if (ah->asr_interface->asr_float_param) {
		ah->asr_interface->asr_float_param(ah, param, val);
	}
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
