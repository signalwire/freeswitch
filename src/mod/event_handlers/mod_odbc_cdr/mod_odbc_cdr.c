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
 *
 * Contributor(s):
 *
 * Emmanuel Schmidbauer <e.schmidbauer@gmail.com>
 *
 * mod_odbc_cdr.c
 *
 */

#include "switch.h"

#define ODBC_CDR_SQLITE_DB_NAME "odbc_cdr"

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_odbc_cdr_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_odbc_cdr_load);
SWITCH_MODULE_DEFINITION(mod_odbc_cdr, mod_odbc_cdr_load, mod_odbc_cdr_shutdown, NULL);

static const char *global_cf = "odbc_cdr.conf";

typedef enum {
	ODBC_CDR_LOG_A,
	ODBC_CDR_LOG_B,
	ODBC_CDR_LOG_BOTH
} odbc_cdr_log_leg_t;

typedef enum {
	ODBC_CDR_CSV_ALWAYS,
	ODBC_CDR_CSV_NEVER,
	ODBC_CDR_CSV_ON_FAIL
} odbc_cdr_write_csv_t;

static struct {
	char *odbc_dsn;
	char *dbname;
	char *csv_path;
	char *csv_fail_path;
	odbc_cdr_log_leg_t log_leg;
	odbc_cdr_write_csv_t write_csv;
	switch_bool_t debug_sql;
	switch_hash_t *table_hash;
	uint32_t running;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
} globals;

struct table_profile {
	char *name;
	odbc_cdr_log_leg_t log_leg;
	switch_hash_t *field_hash;
	uint32_t flags;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
};
typedef struct table_profile table_profile_t;

static table_profile_t *load_table(const char *table_name)
{
	table_profile_t *table = NULL;
	switch_xml_t x_tables, cfg, xml, x_table, x_field;

	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf);
		return table;
	}

	if (!(x_tables = switch_xml_child(cfg, "tables"))) {
		goto end;
	}

	if ((x_table = switch_xml_find_child(x_tables, "table", "name", table_name))) {
		switch_memory_pool_t *pool;
		char *table_log_leg = (char *) switch_xml_attr_soft(x_table, "log-leg");

		if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
			goto end;
		}

		if (!(table = switch_core_alloc(pool, sizeof(table_profile_t)))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Alloc Failure\n");
			switch_core_destroy_memory_pool(&pool);
			goto end;
		}

		table->pool = pool;

		switch_mutex_init(&table->mutex, SWITCH_MUTEX_NESTED, table->pool);

		table->name = switch_core_strdup(pool, table_name);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Found table [%s]\n", table->name);

		if (!strcasecmp(table_log_leg, "a-leg")) {
			table->log_leg = ODBC_CDR_LOG_A;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set table [%s] to log A-legs only\n", table->name);
		} else if (!strcasecmp(table_log_leg, "b-leg")) {
			table->log_leg = ODBC_CDR_LOG_B;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set table [%s] to log B-legs only\n", table->name);
		} else {
			table->log_leg = ODBC_CDR_LOG_BOTH;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set table [%s] to log both legs\n", table->name);
		}

		switch_core_hash_init(&table->field_hash);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Adding fields to table [%s]\n", table->name);

		for (x_field = switch_xml_child(x_table, "field"); x_field; x_field = x_field->next) {
			char *var = (char *) switch_xml_attr_soft(x_field, "name");
			char *val = (char *) switch_xml_attr_soft(x_field, "chan-var-name");
			char *value = NULL;
			if (zstr(var) || zstr(val)) {
				continue; // Ignore empty entries
			}
			value = switch_core_strdup(pool, val);
			switch_core_hash_insert_locked(table->field_hash, var, value, table->mutex);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Field [%s] (%s) added to [%s]\n", var, val, table->name);
		}

		switch_core_hash_insert(globals.table_hash, table->name, table);
	}

end:

	if (xml) {
		switch_xml_free(xml);
	}

	return table;
}

switch_cache_db_handle_t *get_db_handle(void)
{
	switch_cache_db_handle_t *dbh = NULL;
	char *dsn;
	if (!zstr(globals.odbc_dsn)) {
		dsn = globals.odbc_dsn;
	} else {
		dsn = globals.dbname;
	}
	if (switch_cache_db_get_db_handle_dsn(&dbh, dsn) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");
		dbh = NULL;
	}
	return dbh;
}

static switch_status_t odbc_cdr_execute_sql_no_callback(char *sql)
{
	switch_cache_db_handle_t *dbh = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (!(dbh = get_db_handle())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");
		goto end;
	}

	status = switch_cache_db_execute_sql(dbh, sql, NULL);

end:

	switch_cache_db_release_db_handle(&dbh);

	return status;
}

static void write_cdr(const char *path, const char *log_line)
{
	int fd = -1;
#ifdef _MSC_VER
	if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
#else
	if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) > -1) {
#endif
		int wrote;
		wrote = write(fd, log_line, (unsigned) strlen(log_line));
		wrote += write(fd, "\n", 1);
		wrote++;
		close(fd);
		fd = -1;
	}
}

static switch_status_t odbc_cdr_reporting(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_memory_pool_t *pool = switch_core_session_get_pool(session);
	switch_caller_profile_t *caller_profile = switch_channel_get_caller_profile(channel);
	switch_hash_index_t *hi;
	const void *var;
	void *val;
	switch_console_callback_match_t *matches = NULL;
	switch_console_callback_match_node_t *m;
	const char *uuid = NULL;

	if (globals.log_leg == ODBC_CDR_LOG_A && caller_profile->direction == SWITCH_CALL_DIRECTION_OUTBOUND) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Only logging A-Leg, ignoring B-leg\n");
		return SWITCH_STATUS_SUCCESS;
	} else if (globals.log_leg == ODBC_CDR_LOG_B && caller_profile->direction == SWITCH_CALL_DIRECTION_INBOUND) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Only logging B-Leg, ignoring A-leg\n");
		return SWITCH_STATUS_SUCCESS;
	} else {
		const char *tmp = NULL;
		if ((tmp = switch_channel_get_variable(channel, "odbc-cdr-ignore-leg")) && switch_true(tmp)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "odbc-cdr-ignore-leg set to true, ignoring leg\n");
			return SWITCH_STATUS_SUCCESS;
		}
	}

	if (!(uuid = switch_channel_get_variable(channel, "uuid"))) {
		uuid = switch_core_strdup(pool, caller_profile->uuid);
	}

	// copy all table names from global hash
	switch_mutex_lock(globals.mutex);
	for (hi = switch_core_hash_first(globals.table_hash); hi; hi = switch_core_hash_next(&hi)) {
		switch_core_hash_this(hi, &var, NULL, &val);
		switch_console_push_match(&matches, (const char *) var);
	}
	switch_mutex_unlock(globals.mutex);

	if (matches) {
		table_profile_t *table = NULL;

		// loop through table names
		for (m = matches->head; m; m = m->next) {
			char *table_name = m->val;
			switch_bool_t started = SWITCH_FALSE;
			switch_bool_t skip_leg = SWITCH_FALSE;

			switch_mutex_lock(globals.mutex);
			table = switch_core_hash_find(globals.table_hash, table_name);
			switch_mutex_unlock(globals.mutex);

			if (!table) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Table [%s] not found, ignoring leg\n", table_name);
				skip_leg = SWITCH_TRUE;
			}

			if (table->log_leg == ODBC_CDR_LOG_A && caller_profile->direction == SWITCH_CALL_DIRECTION_OUTBOUND) {
				skip_leg = SWITCH_TRUE;
			}

			if (table->log_leg == ODBC_CDR_LOG_B && caller_profile->direction == SWITCH_CALL_DIRECTION_INBOUND) {
				skip_leg = SWITCH_TRUE;
			}

			if (skip_leg == SWITCH_FALSE) {
				switch_hash_index_t *i_hi = NULL;
				const void *i_var;
				void *i_val;
				char *field_hash_key;
				char *field_hash_val;
				char *sql = NULL;
				char *full_path = NULL;
				switch_stream_handle_t stream_field = { 0 };
				switch_stream_handle_t stream_value = { 0 };
				switch_bool_t insert_fail = SWITCH_FALSE;				

				SWITCH_STANDARD_STREAM(stream_field);
				SWITCH_STANDARD_STREAM(stream_value);

				for (i_hi = switch_core_hash_first_iter( table->field_hash, i_hi); i_hi; i_hi = switch_core_hash_next(&i_hi)) {
					const char *tmp;
					switch_core_hash_this(i_hi, &i_var, NULL, &i_val);
					field_hash_key = (char *) i_var;
					field_hash_val = (char *) i_val;

					if ((tmp = switch_channel_get_variable(channel, field_hash_val))) {
						if (started == SWITCH_FALSE) {
							stream_field.write_function(&stream_field, "%s", field_hash_key);
							stream_value.write_function(&stream_value, "'%s'", tmp);
						} else {
							stream_field.write_function(&stream_field, ", %s", field_hash_key);
							stream_value.write_function(&stream_value, ", '%s'", tmp);
						}
						started = SWITCH_TRUE;
					}

				}
				switch_safe_free(i_hi);

				sql = switch_mprintf("INSERT INTO %s (%s) VALUES (%s)", table_name, stream_field.data, stream_value.data);
				if (globals.debug_sql == SWITCH_TRUE) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "sql %s\n", sql);
				}
				if (odbc_cdr_execute_sql_no_callback(sql) == SWITCH_STATUS_FALSE) {
					insert_fail = SWITCH_TRUE;
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error executing query %s\n", sql);
				}

				if (globals.write_csv == ODBC_CDR_CSV_ALWAYS) {
					if (insert_fail == SWITCH_TRUE) {
						full_path = switch_mprintf("%s%s%s.csv", globals.csv_fail_path, SWITCH_PATH_SEPARATOR, uuid);
					} else {
						full_path = switch_mprintf("%s%s%s.csv", globals.csv_path, SWITCH_PATH_SEPARATOR, uuid);
					}
					assert(full_path);
					write_cdr(full_path, stream_value.data);
					switch_safe_free(full_path);
				} else if (globals.write_csv == ODBC_CDR_CSV_ON_FAIL && insert_fail == SWITCH_TRUE) {
					full_path = switch_mprintf("%s%s%s.csv", globals.csv_fail_path, SWITCH_PATH_SEPARATOR, uuid);
					assert(full_path);
					write_cdr(full_path, stream_value.data);
					switch_safe_free(full_path);
				}

				switch_safe_free(sql);

				switch_safe_free(stream_field.data);
				switch_safe_free(stream_value.data);

			}

		}

		switch_console_free_matches(&matches);
	}

	switch_safe_free(hi);

	return SWITCH_STATUS_SUCCESS;
}


switch_state_handler_table_t odbc_cdr_state_handlers = {
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
	/*.on_reporting */ odbc_cdr_reporting,
	/*.on_destroy */ NULL
};

static switch_status_t odbc_cdr_load_config(void)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_xml_t cfg, xml, settings, param, tables, table;
	switch_cache_db_handle_t *dbh = NULL;

	switch_mutex_lock(globals.mutex);

	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf);
		status = SWITCH_STATUS_TERM;
		goto end;
	}

	globals.debug_sql = SWITCH_FALSE;
	globals.log_leg = ODBC_CDR_LOG_BOTH;
	globals.write_csv = ODBC_CDR_CSV_NEVER;

	if ((settings = switch_xml_child(cfg, "settings")) != NULL) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (zstr(var) || zstr(val)) {
				continue; // Ignore empty entries
			}
			if (!strcasecmp(var, "dbname")) {
				globals.dbname = strdup(val);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set dbname [%s]\n", globals.dbname);
			} else if (!strcasecmp(var, "odbc-dsn")) {
				globals.odbc_dsn = strdup(val);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set odbc-dsn [%s]\n", globals.odbc_dsn);
			} else if (!strcasecmp(var, "log-leg")) {
				if (!strcasecmp(val, "a-leg")) {
					globals.log_leg = ODBC_CDR_LOG_A;
				} else if (!strcasecmp(val, "b-leg")) {
					globals.log_leg = ODBC_CDR_LOG_B;
				}
			} else if (!strcasecmp(var, "debug-sql") && switch_true(val)) {
				globals.debug_sql = SWITCH_TRUE;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set debug-sql [true]\n");
			} else if (!strcasecmp(var, "write-csv") && !zstr(val)) {
				if (!strcasecmp(val, "always")) {
					globals.write_csv = ODBC_CDR_CSV_ALWAYS;
				} else if (!strcasecmp(val, "on-db-fail")) {
					globals.write_csv = ODBC_CDR_CSV_ON_FAIL;
				}
			} else if (!strcasecmp(var, "csv-path") && !zstr(val)) {
				globals.csv_path = switch_mprintf("%s%s", val, SWITCH_PATH_SEPARATOR);
			} else if (!strcasecmp(var, "csv-path-on-fail") && !zstr(val)) {
				globals.csv_fail_path = switch_mprintf("%s%s", val, SWITCH_PATH_SEPARATOR);
			}
		}
	}

	if (globals.log_leg == ODBC_CDR_LOG_A) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set log-leg [a-leg]\n");
	} else if (globals.log_leg == ODBC_CDR_LOG_B) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set log-leg [b-leg]\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set log-leg [both]\n");
	}

	if (!globals.csv_path) {
		globals.csv_path = switch_mprintf("%s%sodbc-cdr", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
	}

	if (!globals.csv_fail_path) {
		globals.csv_fail_path = switch_mprintf("%s%sodbc-cdr-failed", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set csv-path [%s]\n", globals.csv_path);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set csv-path-on-fail [%s]\n", globals.csv_fail_path);

	if ((tables = switch_xml_child(cfg, "tables"))) {
		for (table = switch_xml_child(tables, "table"); table; table = table->next) {
			load_table(switch_xml_attr_soft(table, "name"));
		}
	}

	if (!globals.dbname) {
		globals.dbname = strdup(ODBC_CDR_SQLITE_DB_NAME);
	}

	// Initialize database
	if (!(dbh = get_db_handle())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot open DB!\n");
		status = SWITCH_STATUS_TERM;
		goto end;
	}

	switch_cache_db_release_db_handle(&dbh);

end:
	switch_mutex_unlock(globals.mutex);

	if (xml) {
		switch_xml_free(xml);
	}

	return status;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_odbc_cdr_load)
{
	switch_status_t status;

	memset(&globals, 0, sizeof(globals));
	switch_core_hash_init(&globals.table_hash);
	if (switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to initialize mutex\n");
	}
	globals.pool = pool;

	if ((status = odbc_cdr_load_config()) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	if (globals.write_csv != ODBC_CDR_CSV_NEVER) {
		if ((status = switch_dir_make_recursive(globals.csv_path, SWITCH_DEFAULT_DIR_PERMS, pool)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error creating %s\n", globals.csv_path);
			return status;
		}
		if (strcasecmp(globals.csv_path, globals.csv_fail_path)) {
			if ((status = switch_dir_make_recursive(globals.csv_fail_path, SWITCH_DEFAULT_DIR_PERMS, pool)) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error creating %s\n", globals.csv_path);
				return status;
			}
		}
	}

	switch_mutex_lock(globals.mutex);
	globals.running = 1;
	switch_mutex_unlock(globals.mutex);

	switch_core_add_state_handler(&odbc_cdr_state_handlers);
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_odbc_cdr_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_odbc_cdr_shutdown)
{
	switch_hash_index_t *hi = NULL;
	table_profile_t *table;
	void *val = NULL;
	const void *key;
	switch_ssize_t keylen;

	switch_mutex_lock(globals.mutex);
	if (globals.running == 1) {
		globals.running = 0;
	}

	while ((hi = switch_core_hash_first_iter(globals.table_hash, hi))) {
		switch_hash_index_t *field_hi = NULL;
		void *field_val = NULL;
		const void *field_key;
		switch_ssize_t field_keylen;

		switch_core_hash_this(hi, &key, &keylen, &val);
		table = (table_profile_t *) val;

		while ((field_hi = switch_core_hash_first_iter(table->field_hash, field_hi))) {
			switch_core_hash_this(field_hi, &field_key, &field_keylen, &field_val);
			switch_core_hash_delete_locked(table->field_hash, field_key, table->mutex);
		}
		switch_core_hash_destroy(&table->field_hash);
		switch_safe_free(field_hi);

		switch_core_hash_delete(globals.table_hash, table->name);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Destroying table %s\n", table->name);

		switch_core_destroy_memory_pool(&table->pool);
		table = NULL;
	}
	switch_core_hash_destroy(&globals.table_hash);
	switch_safe_free(hi);

	switch_safe_free(globals.csv_path)
	switch_safe_free(globals.csv_fail_path)
	switch_safe_free(globals.odbc_dsn);
	switch_safe_free(globals.dbname);

	switch_mutex_unlock(globals.mutex);
	switch_mutex_destroy(globals.mutex);

	switch_core_remove_state_handler(&odbc_cdr_state_handlers);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4
 */
