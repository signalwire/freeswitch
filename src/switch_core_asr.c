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
 * switch_core_asr.c -- Main Core Library (Speech Detection Interface)
 *
 */

#include <switch.h>
#include "private/switch_core_pvt.h"

SWITCH_DECLARE(switch_status_t) switch_core_asr_open(switch_asr_handle_t *ah,
													 const char *module_name,
													 const char *codec,
													 int rate,
													 const char *dest,
													 switch_asr_flag_t *flags,
													 switch_memory_pool_t *pool)
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid asr module [%s]!\n", module_name);
		return SWITCH_STATUS_GENERR;
	}

	ah->flags = *flags;

	if (pool) {
		ah->memory_pool = pool;
	} else {
		if ((status = switch_core_new_memory_pool(&ah->memory_pool)) != SWITCH_STATUS_SUCCESS) {
			return status;
		}
		switch_set_flag(ah, SWITCH_ASR_FLAG_FREE_POOL);
	}

	if (param) {
		ah->param = switch_core_strdup(ah->memory_pool, param);
	}
	ah->rate = rate;
	ah->name = switch_core_strdup(ah->memory_pool, module_name);

	return ah->asr_interface->asr_open(ah, codec, rate, dest, flags);
}

SWITCH_DECLARE(switch_status_t) switch_core_asr_load_grammar(switch_asr_handle_t *ah, const char *grammar, const char *path)
{
	char *epath = NULL;
	switch_status_t status;

	switch_assert(ah != NULL);

	if (!switch_is_file_path(path)) {
		epath = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.grammar_dir, SWITCH_PATH_SEPARATOR, path);
		path = epath;
	}

	status = ah->asr_interface->asr_load_grammar(ah, grammar, path);
	switch_safe_free(epath);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_asr_unload_grammar(switch_asr_handle_t *ah, const char *grammar)
{
	switch_status_t status;

	switch_assert(ah != NULL);
	status = ah->asr_interface->asr_unload_grammar(ah, grammar);

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

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
