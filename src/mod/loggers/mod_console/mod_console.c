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

static switch_loadable_module_interface_t console_module_interface = {
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

static switch_memory_pool_t *module_pool = NULL;
static switch_hash_t *log_hash = NULL;
static switch_hash_t *name_hash = NULL;
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

static switch_status_t config_logger(void)
{
	char *cf = "console.conf";
	switch_xml_t cfg, xml, settings, param;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	switch_core_hash_init(&log_hash, module_pool);
	switch_core_hash_init(&name_hash, module_pool);
	
	if ((settings = switch_xml_child(cfg, "mappings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			add_mapping(var, val);
		}
	}

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_console_logger(const switch_log_node_t *node, switch_log_level_t level)
{
	FILE *handle;

	if ((handle = switch_core_data_channel(SWITCH_CHANNEL_ID_LOG))) {
		uint8_t *lookup = NULL;
		switch_log_level_t level = SWITCH_LOG_DEBUG;		

		if (log_hash) {
			lookup = switch_core_hash_find(log_hash, node->file);
			
			if (!lookup) {
				lookup = switch_core_hash_find(log_hash, node->func);
			}
		}

		if (lookup) {
			level = (switch_log_level_t) *lookup;
		} else if (all_level > -1) {
			level = (switch_log_level_t) all_level;
		} 

		if (!log_hash || (((all_level > - 1) || lookup) && level >= node->level)) {
			fprintf(handle, "%s", node->data);
		}
	} else {
		fprintf(stderr, "HELP I HAVE NO CONSOLE TO LOG TO!\n");
		fflush(stderr);
	}
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{
	if (switch_core_new_memory_pool(&module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}


	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &console_module_interface;

	/* setup my logger function */
	switch_log_bind_logger(switch_console_logger, SWITCH_LOG_DEBUG);
	
	config_logger();

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
