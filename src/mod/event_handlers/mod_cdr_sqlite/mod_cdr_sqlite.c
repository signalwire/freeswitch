/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * Portions created by Seventh Signal Ltd. & Co. KG and its employees are Copyright (C)
 * Seventh Signal Ltd. & Co. KG, All Rights Reserverd.
 *
 * Contributor(s):
 * Daniel Swarbrick <daniel.swarbrick@gmail.com>
 * 
 * mod_cdr_sqlite.c -- SQLite CDR Module
 *
 * Derived from:
 * mod_cdr_csv.c -- Asterisk Compatible CDR Module
 *
 */
#include <switch.h>

typedef enum {
	CDR_LEG_A = (1 << 0),
	CDR_LEG_B = (1 << 1)
} cdr_leg_t;

static char default_create_sql[] =
	"CREATE TABLE %s (\n"
	"    caller_id_name VARCHAR,\n"
	"    caller_id_number VARCHAR,\n"
	"    destination_number VARCHAR,\n"
	"    context VARCHAR,\n"
	"    start_stamp DATETIME,\n"
	"    answer_stamp DATETIME,\n"
	"    end_stamp DATETIME,\n"
	"    duration INTEGER,\n"
	"    billsec INTEGER,\n"
	"    hangup_cause VARCHAR,\n"
	"    uuid VARCHAR,\n"
	"    bleg_uuid VARCHAR,\n"
	"    account_code VARCHAR\n"
	")\n";

const char *default_template =
	"\"${caller_id_name}\",\"${caller_id_number}\",\"${destination_number}\",\"${context}\","
	"\"${start_stamp}\",\"${answer_stamp}\",\"${end_stamp}\",${duration},${billsec},"
	"\"${hangup_cause}\",\"${uuid}\",\"${bleg_uuid}\",\"${accountcode}\"";

static struct {
	switch_memory_pool_t *pool;
	char *db_name;
	char *db_table;
	cdr_leg_t legs;
	int debug;
	switch_hash_t *template_hash;
	char *default_template;
	int shutdown;
} globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_cdr_sqlite_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cdr_sqlite_shutdown);
SWITCH_MODULE_DEFINITION(mod_cdr_sqlite, mod_cdr_sqlite_load, mod_cdr_sqlite_shutdown, NULL);


switch_cache_db_handle_t *cdr_get_db_handle(void)
{
	switch_cache_db_handle_t *dbh = NULL;

	if (switch_cache_db_get_db_handle_dsn(&dbh, globals.db_name) != SWITCH_STATUS_SUCCESS) {
		dbh = NULL;
	}

	return dbh;
}


static switch_status_t write_cdr(char *sql)
{
	switch_cache_db_handle_t *dbh = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (!(dbh = cdr_get_db_handle())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");
		goto end;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Writing SQL to DB: %s\n", sql);
	status = switch_cache_db_execute_sql(dbh, sql, NULL);

  end:

	switch_cache_db_release_db_handle(&dbh);

	return status;
}


static switch_status_t my_on_reporting(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	const char *template_str = NULL;
	char *expanded_vars = NULL, *sql = NULL;

	if (globals.shutdown) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (!((globals.legs & CDR_LEG_A) && (globals.legs & CDR_LEG_B))) {
		if ((globals.legs & CDR_LEG_A)) {
			if (switch_channel_get_originator_caller_profile(channel)) {
				return SWITCH_STATUS_SUCCESS;
			}
		} else {
			if (switch_channel_get_originatee_caller_profile(channel)) {
				return SWITCH_STATUS_SUCCESS;
			}
		}
	}

	if (globals.debug) {
		switch_event_t *event;
		if (switch_event_create_plain(&event, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
			char *buf;
			switch_channel_event_set_data(channel, event);
			switch_event_serialize(event, &buf, SWITCH_FALSE);
			switch_assert(buf);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "CHANNEL_DATA:\n%s\n", buf);
			switch_event_destroy(&event);
			switch_safe_free(buf);
		}
	}

	template_str = (const char *) switch_core_hash_find(globals.template_hash, globals.default_template);

	if (!template_str) {
		template_str = default_template;
	}

	expanded_vars = switch_channel_expand_variables(channel, template_str);

	if (!expanded_vars) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error expanding CDR variables.\n");
		return SWITCH_STATUS_FALSE;
	}

	sql = switch_mprintf("INSERT INTO %s VALUES (%s)", globals.db_table, expanded_vars);
	assert(sql);
	write_cdr(sql);
	switch_safe_free(sql);

	if (expanded_vars != template_str) {
		switch_safe_free(expanded_vars);
	}

	return status;
}


static switch_state_handler_table_t state_handlers = {
	/*.on_init */ NULL,
	/*.on_routing */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ NULL,
	/*.on_exchange_media */ NULL,
	/*.on_soft_execute */ NULL,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ NULL,
	/*.on_reset */ NULL,
	/*.on_park */ NULL,
	/*.on_reporting */ my_on_reporting
};


static switch_status_t load_config(switch_memory_pool_t *pool)
{
	char *cf = "cdr_sqlite.conf";
	switch_xml_t cfg, xml, settings, param;
	switch_cache_db_handle_t *dbh = NULL;
	char *select_sql = NULL, *create_sql = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	memset(&globals, 0, sizeof(globals));
	switch_core_hash_init(&globals.template_hash, pool);

	globals.pool = pool;

	switch_core_hash_insert(globals.template_hash, "default", default_template);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding default template.\n");
	globals.legs = CDR_LEG_A;

	if ((xml = switch_xml_open_cfg(cf, &cfg, NULL))) {

		if ((settings = switch_xml_child(cfg, "settings"))) {
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");
				if (!strcasecmp(var, "debug")) {
					globals.debug = switch_true(val);
				} else if (!strcasecmp(var, "db-name")) {
					globals.db_name = switch_core_strdup(pool, val);
				} else if (!strcasecmp(var, "db-table")) {
					globals.db_table = switch_core_strdup(pool, val);
				} else if (!strcasecmp(var, "legs")) {
					globals.legs = 0;

					if (strchr(val, 'a')) {
						globals.legs |= CDR_LEG_A;
					}

					if (strchr(val, 'b')) {
						globals.legs |= CDR_LEG_B;
					}
				} else if (!strcasecmp(var, "default-template")) {
					globals.default_template = switch_core_strdup(pool, val);
				}
			}
		}

		if ((settings = switch_xml_child(cfg, "templates"))) {
			for (param = switch_xml_child(settings, "template"); param; param = param->next) {
				char *var = (char *) switch_xml_attr(param, "name");
				if (var) {
					char *tpl;
					tpl = switch_core_strdup(pool, param->txt);

					switch_core_hash_insert(globals.template_hash, var, tpl);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding template %s.\n", var);
				}
			}
		}
		switch_xml_free(xml);
	}

	if (zstr(globals.db_name)) {
		globals.db_name = switch_core_strdup(pool, "cdr");
	}

	if (zstr(globals.db_table)) {
		globals.db_table = switch_core_strdup(pool, "cdr");
	}

	if (zstr(globals.default_template)) {
		globals.default_template = switch_core_strdup(pool, "default");
	}

	dbh = cdr_get_db_handle();

	if (dbh) {
		select_sql = switch_mprintf("SELECT * FROM %s LIMIT 1", globals.db_table);
		assert(select_sql);

		create_sql = switch_mprintf(default_create_sql, globals.db_table);
		assert(create_sql);

		/* Check if table exists (try SELECT FROM ...) and create table if query fails */
		switch_cache_db_test_reactive(dbh, select_sql, NULL, create_sql);
		switch_safe_free(select_sql);
		switch_safe_free(create_sql);

		switch_cache_db_release_db_handle(&dbh);
	}

	return status;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_cdr_sqlite_load)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	load_config(pool);

	switch_core_add_state_handler(&state_handlers);
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	return status;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cdr_sqlite_shutdown)
{
	globals.shutdown = 1;
	switch_core_remove_state_handler(&state_handlers);

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
