/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 * The Initial Developer of this module is
 * Ken Rice <krice@freeswitch.org>
 *
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Ken Rice <krice@freeswitch.org>
 *
 * mod_easyroute.c -- EasyRoute
 * Take Incoming DIDs and Lookup where to send them as well as retrieve
 * the number of channels they are allowed you use
 *
 * Big Thanks to CP and Joseph Watson (Powercode Systems, LLC) for funding this work.
 *
 */

#include <switch.h>

typedef struct easyroute_results {
	char limit[16];
	char dialstring[256];
	char group[16];
	char acctcode[17];
	char translated[61];
} easyroute_results_t;


typedef struct route_callback {
	char gateway[129];
	char group[129];
	char techprofile[129];
	char limit[129];
	char acctcode[129];
	char translated[61];
} route_callback_t;

static struct {
	char *db_username;
	char *db_password;
	char *db_dsn;
	char *default_techprofile;
	char *default_gateway;
	switch_mutex_t *mutex;
	char *custom_query;
	switch_odbc_handle_t *master_odbc;
	int odbc_num_retries;
} globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_easyroute_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_easyroute_shutdown);
SWITCH_MODULE_DEFINITION(mod_easyroute, mod_easyroute_load, mod_easyroute_shutdown, NULL);

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_db_username, globals.db_username);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_db_password, globals.db_password);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_db_dsn, globals.db_dsn);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_default_techprofile, globals.default_techprofile);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_default_gateway, globals.default_gateway);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_custom_query, globals.custom_query);

static int route_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	route_callback_t *cbt = (route_callback_t *) pArg;
	if (argc >= 6) {
		switch_copy_string(cbt->gateway, argv[0], 128);
		switch_copy_string(cbt->group, argv[1], 128);
		switch_copy_string(cbt->limit, argv[2], 128);
		switch_copy_string(cbt->techprofile, argv[3], 128);
		switch_copy_string(cbt->acctcode, argv[4], 128);
		switch_copy_string(cbt->translated, argv[5], 60);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "sql for route_callback only returning %d fields\n", argc);
	}
	return 0;
}

static switch_status_t load_config(void)
{
	char *cf = "easyroute.conf";
	switch_xml_t cfg, xml = NULL, param, settings;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			if (!strcasecmp(var, "db-username")) {
				set_global_db_username(val);
			} else if (!strcasecmp(var, "db-password")) {
				set_global_db_password(val);
			} else if (!strcasecmp(var, "db-dsn")) {
				set_global_db_dsn(val);
			} else if (!strcasecmp(var, "default-techprofile")) {
				set_global_default_techprofile(val);
			} else if (!strcasecmp(var, "default-gateway")) {
				set_global_default_gateway(val);
			} else if (!strcasecmp(var, "custom-query")) {
				set_global_custom_query(val);
			} else if (!strcasecmp(var, "odbc-retries")) {
				globals.odbc_num_retries = atoi(val);
			}
		}
	}

  done:
	if (zstr(globals.db_username)) {
		set_global_db_username("root");
	}
	if (zstr(globals.db_password)) {
		set_global_db_password("password");
	}
	if (zstr(globals.db_dsn)) {
		set_global_db_dsn("easyroute");
	}

	if (globals.db_dsn) {
		if (!(globals.master_odbc = switch_odbc_handle_new(globals.db_dsn, globals.db_username, globals.db_password))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open Database!\n");
			status = SWITCH_STATUS_FALSE;
			goto reallydone;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Opened Database!\n");
		}
		if (globals.odbc_num_retries) {
			switch_odbc_set_num_retries(globals.master_odbc, globals.odbc_num_retries);
		}
		if (switch_odbc_handle_connect(globals.master_odbc) != SWITCH_ODBC_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open Database!\n");
			status = SWITCH_STATUS_FALSE;
			goto reallydone;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Opened Database!\n");
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Connected ODBC DSN: %s\n", globals.db_dsn);
		if (!globals.custom_query) {
			if (switch_odbc_handle_exec(globals.master_odbc, "select count(*) from numbers", NULL, NULL) != SWITCH_ODBC_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot find  SQL Database! (Where\'s the numbers table\?\?)\n");
			}
			if (switch_odbc_handle_exec(globals.master_odbc, "select count(*) from gateways", NULL, NULL) != SWITCH_ODBC_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot find  SQL Database! (Where\'s the gateways table\?\?)\n");
			}
		}
	} else if (!globals.db_dsn) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open ODBC Connection (did you enable it?)\n");
	}

  reallydone:

	if (xml) {
		switch_xml_free(xml);
	}
	if (!globals.default_techprofile) {
		set_global_default_techprofile("sofia/default");
	}
	if (!globals.default_gateway) {
		set_global_default_gateway("192.168.1.1");
	}
	return status;
}

static char SQL_LOOKUP[] =
	"SELECT gateways.gateway_ip, gateways.group, gateways.limit, gateways.techprofile, numbers.acctcode, numbers.translated from gateways, numbers where numbers.number = '%q' and numbers.gateway_id = gateways.gateway_id limit 1;";

static switch_status_t route_lookup(char *dn, easyroute_results_t *results, int noat, char *separator)
{
	switch_status_t sstatus = SWITCH_STATUS_SUCCESS;
	char *sql = NULL;
	route_callback_t pdata;

	memset(&pdata, 0, sizeof(pdata));
	if (!globals.custom_query) {
		sql = switch_mprintf(SQL_LOOKUP, dn);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Doing static Query\n[%s]\n", sql);
	} else {
		sql = switch_mprintf(globals.custom_query, dn);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Doing custom Query\n[%s]\n", sql);
	}

	if (globals.mutex) {
		switch_mutex_lock(globals.mutex);
	}
	/* Do the Query */
	if (globals.master_odbc && switch_odbc_handle_callback_exec(globals.master_odbc, sql, route_callback, &pdata, NULL) == SWITCH_ODBC_SUCCESS) {
		char tmp_profile[129];
		char tmp_gateway[129];

		if (zstr(pdata.limit)) {
			switch_set_string(results->limit, "9999");
		} else {
			switch_set_string(results->limit, pdata.limit);
		}

		if (zstr(pdata.techprofile)) {
			switch_set_string(tmp_profile, globals.default_techprofile);
		} else {
			switch_set_string(tmp_profile, pdata.techprofile);
		}

		if (zstr(pdata.gateway)) {
			switch_set_string(tmp_gateway, globals.default_gateway);
		} else {
			switch_set_string(tmp_gateway, pdata.gateway);
		}

		if (zstr(pdata.translated)) {
			switch_set_string(results->translated, dn);
		} else {
			switch_set_string(results->translated, pdata.translated);
		}
		if (noat) {
			switch_snprintf(results->dialstring, 256, "%s/%s%s", tmp_profile, results->translated, tmp_gateway);
		} else if (separator) {
			switch_snprintf(results->dialstring, 256, "%s/%s%s%s", tmp_profile, results->translated, separator, tmp_gateway);
		} else {
			switch_snprintf(results->dialstring, 256, "%s/%s@%s", tmp_profile, results->translated, tmp_gateway);
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "THE ROUTE [%s]\n", results->dialstring);

		if (zstr(pdata.group)) {
			switch_set_string(results->group, "");
		} else {
			switch_set_string(results->group, pdata.group);
		}

		if (zstr(pdata.acctcode)) {
			switch_set_string(results->acctcode, "");
		} else {
			switch_set_string(results->acctcode, pdata.acctcode);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "DB Error Setting Default Route!\n");
		switch_set_string(results->limit, "9999");
		if (noat) {
			switch_snprintf(results->dialstring, 256, "%s/%s%s", globals.default_techprofile, dn, globals.default_gateway);
		} else if (separator) {
			switch_snprintf(results->dialstring, 256, "%s/%s%s%s", globals.default_techprofile, dn, separator, globals.default_gateway);
		} else {
			switch_snprintf(results->dialstring, 256, "%s/%s@%s", globals.default_techprofile, dn, globals.default_gateway);
		}
		switch_set_string(results->group, "");
		switch_set_string(results->acctcode, "");
	}

	switch_safe_free(sql);

	if (globals.mutex) {
		switch_mutex_unlock(globals.mutex);
	}
	return sstatus;
}

SWITCH_STANDARD_APP(easyroute_app_function)
{
	int argc = 0;
	char *argv[4] = { 0 };
	char *mydata = NULL;
	char *destnum = NULL;

	int noat = 0;
	char *separator = NULL;

	easyroute_results_t results;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (!channel) {
		return;
	}

	if (!(mydata = switch_core_session_strdup(session, data))) {
		return;
	}

	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		destnum = argv[0];
		if (argc >= 2) {
			if (!strcasecmp(argv[1], "noat")) {
				noat = 1;
			} else if (!strcasecmp(argv[1], "separator")) {
				if (argc == 3) {
					separator = argv[2];
				}
			}
		}
		route_lookup(destnum, &results, noat, separator);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "EASY ROUTE DEST: [%s]\n", results.dialstring);
		switch_channel_set_variable(channel, "easy_destnum", destnum);
		switch_channel_set_variable(channel, "easy_dialstring", results.dialstring);
		switch_channel_set_variable(channel, "easy_group", results.group);
		switch_channel_set_variable(channel, "easy_limit", results.limit);
		switch_channel_set_variable(channel, "easy_acctcode", results.acctcode);
	}
}

SWITCH_STANDARD_API(easyroute_function)
{
	int argc = 0;
	char *argv[4] = { 0 };
	char *mydata = NULL;
	char *destnum = NULL;
	char *separator = NULL;
	int noat = 0;
	easyroute_results_t results;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (session) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "This function cannot be called from the dialplan.\n");
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	if (!cmd || !(mydata = strdup(cmd))) {
		stream->write_function(stream, "Usage: easyroute <number>\n");
		status = SWITCH_STATUS_SUCCESS;
		goto done;
	}

	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		destnum = argv[0];
		if (argc < 1 || argc > 3) {
			stream->write_function(stream, "Invalid Input!\n");
			status = SWITCH_STATUS_SUCCESS;
			goto done;
		}
		if (argc >= 2) {
			if (!strcasecmp(argv[1], "noat")) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Entering noat.\n");
				noat = 1;
			} else if (!strcasecmp(argv[1], "separator")) {
				if (argc == 3) {
					separator = argv[2];
				}
			}
		}

		if (route_lookup(destnum, &results, noat, separator) != SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "No Match!\n");
			status = SWITCH_STATUS_SUCCESS;
			goto done;
		}
		if (argc != 2) {
			stream->write_function(stream, "Number    \tLimit     \tGroup    \tAcctCode  \tDialstring\n");
			stream->write_function(stream, "%-10s\t%-10s\t%-10s\t%-10s\t%s\n", destnum, results.limit, results.group, results.acctcode,
								   results.dialstring);
		} else {
			if (!strncasecmp(argv[1], "dialstring", 10)) {
				stream->write_function(stream, "%s", results.dialstring);
			} else if (!strncasecmp(argv[1], "translated", 10)) {
				stream->write_function(stream, "%s", results.translated);
			} else if (!strncasecmp(argv[1], "limit", 5)) {
				stream->write_function(stream, "%s", results.limit);
			} else if (!strncasecmp(argv[1], "group", 5)) {
				stream->write_function(stream, "%s", results.group);
			} else if (!strncasecmp(argv[1], "acctcode", 8)) {
				stream->write_function(stream, "%s", results.acctcode);
			} else {
				stream->write_function(stream, "Invalid Input!\n");
				status = SWITCH_STATUS_SUCCESS;
				goto done;
			}
		}
	} else {
		stream->write_function(stream, "Invalid Input!\n");
	}

  done:
	switch_safe_free(mydata);
	return status;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_easyroute_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;

	memset(&globals, 0, sizeof(globals));
	load_config();

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_API(api_interface, "easyroute", "EasyRoute", easyroute_function, "");
	SWITCH_ADD_APP(app_interface, "easyroute", "Perform an easyroute lookup", "Perform an easyroute lookup", easyroute_app_function, "<number>",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_easyroute_shutdown)
{
	if (globals.master_odbc) {
		switch_odbc_handle_disconnect(globals.master_odbc);
		switch_odbc_handle_destroy(&globals.master_odbc);
	}
	switch_safe_free(globals.db_username);
	switch_safe_free(globals.db_password);
	switch_safe_free(globals.db_dsn);
	switch_safe_free(globals.default_techprofile);
	switch_safe_free(globals.default_gateway);
	switch_safe_free(globals.custom_query);

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
