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
 * mod_console.c -- Console Logger
 *
 */
#include <switch.h>

static const char modname[] = "mod_console";
static const uint8_t STATIC_LEVELS[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};

static switch_loadable_module_interface console_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL,
	/*.api_interface */ NULL,
	/*.file_interface */ NULL,
	/*.speech_interface */ NULL,
	/*.directory_interface */ NULL
};

static switch_memory_pool *module_pool = NULL;
static switch_hash *log_hash = NULL;
static switch_hash *name_hash = NULL;
static int8_t all_level = -1;

static void del_mapping(char *var) {
	if (!strcasecmp(var, "all")) {
		all_level = -1;
		return;
	}
	switch_core_hash_insert(log_hash, var, NULL);
}

static void add_mapping(char *var, char *val)
{
	char *name;

	if (!strcasecmp(var, "all")) {
		all_level = (int8_t) switch_log_str2level(val);
		return;
	}

	if (!(name = switch_core_hash_find(name_hash, var))) {
		name = switch_core_strdup(module_pool, var);
		switch_core_hash_insert(name_hash, name, name);
	}

	del_mapping(name);
	switch_core_hash_insert(log_hash, name, (void *) &STATIC_LEVELS[(uint8_t)switch_log_str2level(val)]);
}

static switch_status config_logger(void)
{
	switch_config cfg;
	char *var, *val;
	char *cf = "console.conf";

	if (!switch_config_open_file(&cfg, cf)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_FALSE;
	}

	switch_core_hash_init(&log_hash, module_pool);
	switch_core_hash_init(&name_hash, module_pool);

	while (switch_config_next_pair(&cfg, &var, &val)) {
		if (!strcasecmp(cfg.category, "mappings")) {
			add_mapping(var, val);
		}
	}
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status switch_console_logger(const switch_log_node *node, switch_log_level level)
{
	FILE *handle;

	if ((handle = switch_core_data_channel(SWITCH_CHANNEL_ID_LOG))) {
		uint8_t *lookup = NULL;
		switch_log_level level = SWITCH_LOG_DEBUG;
		
		if (log_hash) {
			lookup = switch_core_hash_find(log_hash, node->file);
			
			if (!lookup) {
				lookup = switch_core_hash_find(log_hash, node->func);
			}
		}

		if (lookup) {
			level = (switch_log_level) *lookup;
		} else if (all_level > -1) {
			level = (switch_log_level) all_level;
		} 

		if (!log_hash || (((all_level > - 1) || lookup) && level >= node->level)) {
			fprintf(handle, node->data);
		}
	}
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename)
{
	if (switch_core_new_memory_pool(&module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}


	/* connect my internal structure to the blank pointer passed to me */
	*interface = &console_module_interface;

	/* setup my logger function */
	switch_log_bind_logger(switch_console_logger, SWITCH_LOG_DEBUG);
	
	config_logger();

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

