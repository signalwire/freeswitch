/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, James Martelletti <james@nerdc0re.com>
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
#define DEFAULT_FACILITY LOG_USER
#define DEFAULT_LEVEL    "warning"
#define DEFAULT_FORMAT   "[message]"
#define MAX_LENGTH       1024

SWITCH_MODULE_LOAD_FUNCTION(mod_syslog_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_syslog_shutdown);
SWITCH_MODULE_DEFINITION(mod_syslog, mod_syslog_load, mod_syslog_shutdown, NULL);

static switch_status_t load_config(void);
static switch_log_level_t log_level;

static struct {
	char *ident;
	char *format;
	int facility;
	switch_bool_t log_uuid;
} globals;

struct _facility_table_entry {
	char *description;
	int facility;
};

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_ident, globals.ident);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_format, globals.format);

switch_status_t set_global_facility(const char *facility)
{
	const struct _facility_table_entry facilities[] = {
		{"auth", LOG_AUTH},
#if !defined (__SVR4) && !defined (__sun)
		{"authpriv", LOG_AUTHPRIV},
		{"ftp", LOG_FTP},
#endif
		{"cron", LOG_CRON},
		{"daemon", LOG_DAEMON},
		{"kern", LOG_KERN},
		{"local0", LOG_LOCAL0},
		{"local1", LOG_LOCAL1},
		{"local2", LOG_LOCAL2},
		{"local3", LOG_LOCAL3},
		{"local4", LOG_LOCAL4},
		{"local5", LOG_LOCAL5},
		{"local6", LOG_LOCAL6},
		{"local7", LOG_LOCAL7},
		{"lpr", LOG_LPR},
		{"mail", LOG_MAIL},
		{"news", LOG_NEWS},
		{"syslog", LOG_SYSLOG},
		{"user", LOG_USER},
		{"uucp", LOG_UUCP},
		{NULL, 0}
	};
	const struct _facility_table_entry *entry = facilities;

	while (!zstr(entry->description)) {
		if (!strcasecmp(entry->description, facility)) {
			globals.facility = entry->facility;
			return SWITCH_STATUS_SUCCESS;
		}
		entry++;
	}

	return SWITCH_STATUS_FALSE;
}

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


static int find_unprintable(const char *s)
{
	const char *p;

	for(p = s; p && *p; p++) {
		if (*p < 10 || *p == 27) {
			return 1;
		}
	}

	return 0;
}

static switch_status_t mod_syslog_logger(const switch_log_node_t *node, switch_log_level_t level)
{
	int syslog_level;

	switch (level) {
	case SWITCH_LOG_DEBUG:
		syslog_level = LOG_DEBUG;
		break;
	case SWITCH_LOG_INFO:
		syslog_level = LOG_INFO;
		break;
	case SWITCH_LOG_NOTICE:
		syslog_level = LOG_NOTICE;
		break;
	case SWITCH_LOG_WARNING:
		syslog_level = LOG_WARNING;
		break;
	case SWITCH_LOG_ERROR:
		syslog_level = LOG_ERR;
		break;
	case SWITCH_LOG_CRIT:
		syslog_level = LOG_CRIT;
		break;
	case SWITCH_LOG_ALERT:
		syslog_level = LOG_ALERT;
		break;
	default:
		syslog_level = LOG_NOTICE;
		break;
	}

	/* don't log blank lines */
	if (!zstr(node->data) && (strspn(node->data, " \t\r\n") < strlen(node->data)) && !find_unprintable(node->data)) {
		if (globals.log_uuid && !zstr(node->userdata)) {
			syslog(syslog_level, "%s %s", node->userdata, node->data);
		} else {
			syslog(syslog_level, "%s", node->data);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t load_config(void)
{
	char *cf = "syslog.conf";
	switch_xml_t cfg, xml, settings, param;

	/* default log level */
	log_level = SWITCH_LOG_WARNING;

	/* default facility */
	globals.facility = DEFAULT_FACILITY;
	globals.log_uuid = SWITCH_TRUE;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
	} else {
		if ((settings = switch_xml_child(cfg, "settings"))) {
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (!strcmp(var, "ident")) {
					set_global_ident(val);
				} else if (!strcmp(var, "format")) {
					set_global_format(val);
				} else if (!strcmp(var, "facility")) {
					set_global_facility(val);
				} else if (!strcasecmp(var, "loglevel") && !zstr(val)) {
					log_level = switch_log_str2level(val);
					if (log_level == SWITCH_LOG_INVALID) {
						log_level = SWITCH_LOG_WARNING;
					}
				} else if (!strcasecmp(var, "uuid")) {
					globals.log_uuid = switch_true(val);
				}
			}
		}
		switch_xml_free(xml);
	}

	if (zstr(globals.ident)) {
		set_global_ident(DEFAULT_IDENT);
	}
	if (zstr(globals.format)) {
		set_global_format(DEFAULT_FORMAT);
	}
	return 0;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_syslog_load)
{
	switch_status_t status;
	*module_interface = &console_module_interface;

	memset(&globals, 0, sizeof(globals));

	if ((status = load_config()) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	openlog(globals.ident, LOG_PID, globals.facility);

	setlogmask(LOG_UPTO(LOG_DEBUG));
	switch_log_bind_logger(mod_syslog_logger, log_level, SWITCH_FALSE);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_syslog_shutdown)
{
	closelog();

	switch_safe_free(globals.ident);
	switch_safe_free(globals.format);

	switch_log_unbind_logger(mod_syslog_logger);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
