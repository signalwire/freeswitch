/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2013, Anthony Minessale II <anthm@freeswitch.org>
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
 * Brian West <brian@freeswitch.org>
 * Christopher Rienzo <chris.rienzo@grasshopper.com>
 * Nickolay V. Shmyrev <nshmyrev@alphacephei.com>
 *
 * mod_vosk - Speech recognition using Vosk server
 *
 *
 */

#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_vosk_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_vosk_shutdown);
SWITCH_MODULE_DEFINITION(mod_vosk, mod_vosk_load, mod_vosk_shutdown, NULL);

static switch_mutex_t *MUTEX = NULL;
static switch_event_node_t *NODE = NULL;

static struct {
	char *remote_url;
	int auto_reload;
	switch_memory_pool_t *pool;
} globals;


typedef struct {
	switch_mutex_t *flag_mutex;
} vosk_t;

/*! function to open the asr interface */
static switch_status_t vosk_asr_open(switch_asr_handle_t *ah, const char *codec, int rate, const char *dest, switch_asr_flag_t *flags)
{
	vosk_t *vosk;

	if (!(vosk = (vosk_t *) switch_core_alloc(ah->memory_pool, sizeof(*vosk)))) {
		return SWITCH_STATUS_MEMERR;
	}

	switch_mutex_init(&vosk->flag_mutex, SWITCH_MUTEX_NESTED, ah->memory_pool);
	ah->private_info = vosk;

	codec = "L16";
	ah->codec = switch_core_strdup(ah->memory_pool, codec);

	return SWITCH_STATUS_SUCCESS;
}

/*! function to close the asr interface */
static switch_status_t vosk_asr_close(switch_asr_handle_t *ah, switch_asr_flag_t *flags)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Port Closed.\n");
	switch_set_flag(ah, SWITCH_ASR_FLAG_CLOSED);
	return SWITCH_STATUS_SUCCESS;
}

/*! function to feed audio to the ASR */
static switch_status_t vosk_asr_feed(switch_asr_handle_t *ah, void *data, unsigned int len, switch_asr_flag_t *flags)
{
	//vosk_t *vosk = (vosk_t *) ah->private_info;

	if (switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED))
		return SWITCH_STATUS_BREAK;

	return SWITCH_STATUS_SUCCESS;
}

/*! function to pause recognizer */
static switch_status_t vosk_asr_pause(switch_asr_handle_t *ah)
{
	//vosk_t *vosk = (vosk_t *) ah->private_info;
	switch_status_t status = SWITCH_STATUS_FALSE;

	return status;
}

/*! function to resume recognizer */
static switch_status_t vosk_asr_resume(switch_asr_handle_t *ah)
{
	//vosk_t *vosk = (vosk_t *) ah->private_info;
	switch_status_t status = SWITCH_STATUS_FALSE;

	return status;
}

/**
 * Process asr_load_grammar request from FreeSWITCH.
 *
 * FreeSWITCH sends this request to load a grammar
 * @param ah the FreeSWITCH speech recognition handle
 * @param grammar the grammar data.  This can be an absolute file path, a URI, or the grammar text.
 * @param name used to reference grammar for unloading or for recognition requests
 */
static switch_status_t vosk_asr_load_grammar(switch_asr_handle_t *ah, const char *grammar, const char *name)
{
	return SWITCH_STATUS_SUCCESS;
}

/**
 * Process asr_unload_grammar request from FreeSWITCH.
 *
 * FreeSWITCH sends this request to stop recognition on this grammar.
 * @param ah the FreeSWITCH speech recognition handle
 * @param name the grammar name.
 */
static switch_status_t vosk_asr_unload_grammar(switch_asr_handle_t *ah, const char *name)
{
	return SWITCH_STATUS_SUCCESS;
}


/*! function to read results from the ASR*/
static switch_status_t vosk_asr_check_results(switch_asr_handle_t *ah, switch_asr_flag_t *flags)
{
	//vosk_t *vosk = (vosk_t *) ah->private_info;

	return SWITCH_STATUS_SUCCESS;
}

/*! function to read results from the ASR */
static switch_status_t vosk_asr_get_results(switch_asr_handle_t *ah, char **xmlstr, switch_asr_flag_t *flags)
{
	//vosk_t *vosk = (vosk_t *) ah->private_info;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	return status;
}

/*! function to start input timeouts */
static switch_status_t vosk_asr_start_input_timers(switch_asr_handle_t *ah)
{
	//vosk_t *vosk = (vosk_t *) ah->private_info;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t load_config(void)
{
	char *cf = "vosk.conf";
	switch_xml_t cfg, xml = NULL, param, settings;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			if (!strcasecmp(var, "remote_url")) {
				globals.remote_url = switch_core_strdup(globals.pool, val);
			}
		}
	}

  done:
	if (xml) {
		switch_xml_free(xml);
	}

	return status;
}

static void do_load(void)
{
	switch_mutex_lock(MUTEX);
	load_config();
	switch_mutex_unlock(MUTEX);
}

static void event_handler(switch_event_t *event)
{
	if (globals.auto_reload) {
		do_load();
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "vosk Reloaded\n");
	}
}

SWITCH_MODULE_LOAD_FUNCTION(mod_vosk_load)
{
	switch_asr_interface_t *asr_interface;

	switch_mutex_init(&MUTEX, SWITCH_MUTEX_NESTED, pool);

	globals.pool = pool;

	if ((switch_event_bind_removable(modname, SWITCH_EVENT_RELOADXML, NULL, event_handler, NULL, &NODE) != SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
	}

	do_load();

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	asr_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ASR_INTERFACE);
	asr_interface->interface_name = "vosk";
	asr_interface->asr_open = vosk_asr_open;
	asr_interface->asr_close = vosk_asr_close;
	asr_interface->asr_load_grammar = vosk_asr_load_grammar;
	asr_interface->asr_unload_grammar = vosk_asr_unload_grammar;
	asr_interface->asr_resume = vosk_asr_resume;
	asr_interface->asr_pause = vosk_asr_pause;
	asr_interface->asr_feed = vosk_asr_feed;
	asr_interface->asr_check_results = vosk_asr_check_results;
	asr_interface->asr_get_results = vosk_asr_get_results;
	asr_interface->asr_start_input_timers = vosk_asr_start_input_timers;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_vosk_shutdown)
{
	switch_event_unbind(&NODE);
	return SWITCH_STATUS_UNLOAD;
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
