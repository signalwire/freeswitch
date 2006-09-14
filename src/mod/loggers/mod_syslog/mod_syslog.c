/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, James Martelletti <james@nerdc0re.com>
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
 * James Martelletti <james@nerdc0re.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * James Martelletti <james@nerdc0re.com>
 *
 *
 * mod_syslog.c -- System Logging
 *
 */
#include <switch.h>
#include <stdlib.h>
#include <syslog.h>

#define DEFAULT_IDENT    "freeswitch"
#define DEFAULT_FACILITY "user"
#define DEFAULT_LEVEL    "warning"
#define DEFAULT_FORMAT   "[message]"
#define MAX_LENGTH       1024

static const char modname[] = "mod_syslog";
static switch_status_t load_config(void);

static struct {
	char *ident;
	char *facility;
	char *level;
	char *format;
} globals;

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_ident, globals.ident)
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_level, globals.level)
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_format, globals.format)
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_facility, globals.facility)

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

static switch_status_t mod_syslog_logger(const switch_log_node_t *node, switch_log_level_t level)
{
	char *message = NULL;
	char line_no[sizeof(int)*8+1];
	char date[80] = "";
	switch_time_exp_t time;
	switch_size_t retsize;

	message = (char *)malloc(strlen(globals.format)+2);

	switch_copy_string(message, globals.format, strlen(globals.format)+1);

	message = switch_string_replace(message, "${message}", node->content);
	
	if (switch_time_exp_lt(&time, node->timestamp) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}
	
	switch_strftime(date, &retsize, sizeof(date), "%Y-%m-%d %T", &time);
	message = switch_string_replace(message, "${time}", date);
	
	message = switch_string_replace(message, "${file}", node->file);
	message = switch_string_replace(message, "${func}", node->func);
	
	snprintf(line_no, sizeof(line_no), "%d", node->line);
	message = switch_string_replace(message, "${line}", line_no);
	
	if (!switch_strlen_zero(message)) {
		syslog(LOG_ERR, "%s", message);
	}

	free(message);
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t load_config(void)
{
	char *cf = "syslog.conf";
	switch_xml_t cfg, xml, settings, param;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
	} else {
		if ((settings = switch_xml_child(cfg, "settings"))) {
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (!strcmp(var, "ident")) {
					set_global_ident(val);
				} else if (!strcmp(var, "facility")) {
					set_global_facility(val);
				} else if (!strcmp(var, "format")) {
					set_global_format(val);
				} else if (!strcmp(var, "level")) {
					set_global_level(val);;
				}
	
			}
		}
		switch_xml_free(xml);
	}

	if (switch_strlen_zero(globals.ident)) {
		set_global_ident(DEFAULT_IDENT);
	}
	if (switch_strlen_zero(globals.facility)) {
		set_global_facility(DEFAULT_FACILITY);
	}
	if (switch_strlen_zero(globals.format)) {
		set_global_format(DEFAULT_FORMAT);
	}
	if (switch_strlen_zero(globals.level)) {
		set_global_level(DEFAULT_LEVEL);
	}


	return 0;
}

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **interface, char *filename)
{
	switch_status_t status;
	*interface = &console_module_interface;

	if ((status=load_config()) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	openlog(globals.ident, LOG_PID, LOG_USER);

	switch_log_bind_logger(mod_syslog_logger, SWITCH_LOG_DEBUG);
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MOD_DECLARE(switch_status_t) switch_module_unload(const switch_loadable_module_interface_t **interface)
{
	closelog();
	
	return SWITCH_STATUS_SUCCESS;
}

