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
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 * Portions created by Seventh Signal Ltd. & Co. KG and its employees are Copyright (C)
 * Seventh Signal Ltd. & Co. KG, All Rights Reserverd.
 *
 * Contributor(s):
 * Michal Bielicki <michal.bielicki@halokwadrat.de>
 * Daniel Swarbrick <daniel.swarbrick@gmail.com>
 * Evgeney Bugorov <evgeney.bugrov@halokwadrat.ru>
 * Sponsored by Halo Kwadrat Sp. z o.o. & Seventh Signal Ltd. & CO. KG
 *
 * mod_cdr_pg_csv.c -- Asterisk Compatible CDR Module with PostgreSQL interface
 * derived from:
 * mod_cdr_csv.c -- Asterisk Compatible CDR Module
 *
 */

#include <switch.h>
#include <sys/stat.h>
#include <libpq-fe.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_cdr_pg_csv_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cdr_pg_csv_shutdown);
SWITCH_MODULE_DEFINITION(mod_cdr_pg_csv, mod_cdr_pg_csv_load, mod_cdr_pg_csv_shutdown, NULL);


typedef enum {
	CDR_LEG_A = (1 << 0),
	CDR_LEG_B = (1 << 1)
} cdr_leg_t;

typedef enum {
	SPOOL_FORMAT_CSV,
	SPOOL_FORMAT_SQL
} spool_format_t;

typedef struct {
	int fd;
	char *path;
	int64_t bytes;
	switch_mutex_t *mutex;
} cdr_fd_t;

typedef struct {
	char *col_name;
	char *var_name;
	switch_bool_t quote;
	switch_bool_t not_null;
} cdr_field_t;

typedef struct {
	char *columns;
	cdr_field_t fields[1];
} db_schema_t;

static struct {
	switch_memory_pool_t *pool;
	switch_hash_t *fd_hash;
	int shutdown;
	char *db_info;
	char *db_table;
	db_schema_t *db_schema;
	PGconn *db_connection;
	switch_mutex_t *db_mutex;
	int db_online;
	cdr_leg_t legs;
	char *spool_dir;
	spool_format_t spool_format;
	int rotate;
	int debug;
} globals;

static switch_xml_config_enum_item_t config_opt_cdr_leg_enum[] = {
        {"a", CDR_LEG_A},
        {"b", CDR_LEG_B},
        {"ab", CDR_LEG_A | CDR_LEG_B},
        {NULL, 0}
};

static switch_xml_config_enum_item_t config_opt_spool_format_enum[] = {
        {"csv", SPOOL_FORMAT_CSV},
        {"sql", SPOOL_FORMAT_SQL},
        {NULL, 0}
};

static switch_status_t config_validate_spool_dir(switch_xml_config_item_t *item, const char *newvalue, switch_config_callback_type_t callback_type, switch_bool_t changed)
{
	if ((callback_type == CONFIG_LOAD || callback_type == CONFIG_RELOAD)) {
		if (zstr(newvalue)) {
			globals.spool_dir = switch_core_sprintf(globals.pool, "%s%scdr-pg-csv", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_xml_config_item_t config_settings[] = {
	/* key, type, flags, ptr, default_value, data, syntax, helptext */
	SWITCH_CONFIG_ITEM_STRING_STRDUP("db-info", CONFIG_RELOADABLE, &globals.db_info, "dbname=cdr", NULL, NULL),
	SWITCH_CONFIG_ITEM_STRING_STRDUP("db-table", CONFIG_RELOADABLE, &globals.db_table, "cdr", NULL, NULL),
	SWITCH_CONFIG_ITEM("legs", SWITCH_CONFIG_ENUM, CONFIG_RELOADABLE, &globals.legs, (void *) CDR_LEG_A, &config_opt_cdr_leg_enum, "a|b|ab", NULL),
	SWITCH_CONFIG_ITEM("spool-format", SWITCH_CONFIG_ENUM, CONFIG_RELOADABLE, &globals.spool_format, (void *) SPOOL_FORMAT_CSV, &config_opt_spool_format_enum, "csv|sql", "Disk spool format to use if SQL insert fails."),
	SWITCH_CONFIG_ITEM("rotate-on-hup", SWITCH_CONFIG_BOOL, CONFIG_RELOADABLE, &globals.rotate, SWITCH_FALSE, NULL, NULL, NULL),
	SWITCH_CONFIG_ITEM("debug", SWITCH_CONFIG_BOOL, CONFIG_RELOADABLE, &globals.debug, SWITCH_FALSE, NULL, NULL, NULL),

	/* key, type, flags, ptr, defaultvalue, function, functiondata, syntax, helptext */
	SWITCH_CONFIG_ITEM_CALLBACK("spool-dir", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &globals.spool_dir, NULL, config_validate_spool_dir, NULL, NULL, NULL),
	SWITCH_CONFIG_ITEM_END()
};


static off_t fd_size(int fd)
{
	struct stat s = { 0 };
	fstat(fd, &s);
	return s.st_size;
}

static void do_reopen(cdr_fd_t *fd)
{
	int x = 0;

	if (fd->fd > -1) {
		close(fd->fd);
		fd->fd = -1;
	}

	for (x = 0; x < 10; x++) {
		if ((fd->fd = open(fd->path, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR)) > -1) {
			fd->bytes = fd_size(fd->fd);
			break;
		}
		switch_yield(100000);
	}
}

static void do_rotate(cdr_fd_t *fd)
{
	switch_time_exp_t tm;
	char date[80] = "";
	switch_size_t retsize;
	char *p;

	close(fd->fd);
	fd->fd = -1;

	if (globals.rotate) {
		switch_time_exp_lt(&tm, switch_micro_time_now());
		switch_strftime_nocheck(date, &retsize, sizeof(date), "%Y-%m-%d-%H-%M-%S", &tm);

		p = switch_mprintf("%s.%s", fd->path, date);
		assert(p);
		switch_file_rename(fd->path, p, globals.pool);
		switch_safe_free(p);
	}

	do_reopen(fd);

	if (fd->fd < 0) {
		switch_event_t *event;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error opening %s\n", fd->path);
		if (switch_event_create(&event, SWITCH_EVENT_TRAP) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Critical-Error", "Error opening cdr file %s\n", fd->path);
			switch_event_fire(&event);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "%s CDR logfile %s\n", globals.rotate ? "Rotated" : "Re-opened", fd->path);
	}

}

static void spool_cdr(const char *path, const char *log_line)
{
	cdr_fd_t *fd = NULL;
	char *log_line_lf = NULL;
	unsigned int bytes_in, bytes_out;
	int loops = 0;

	if (!(fd = switch_core_hash_find(globals.fd_hash, path))) {
		fd = switch_core_alloc(globals.pool, sizeof(*fd));
		switch_assert(fd);
		memset(fd, 0, sizeof(*fd));
		fd->fd = -1;
		switch_mutex_init(&fd->mutex, SWITCH_MUTEX_NESTED, globals.pool);
		fd->path = switch_core_strdup(globals.pool, path);
		switch_core_hash_insert(globals.fd_hash, path, fd);
	}

	if (end_of(log_line) != '\n') {
		log_line_lf = switch_mprintf("%s\n", log_line);
	} else {
		switch_strdup(log_line_lf, log_line);
	}
	assert(log_line_lf);

	switch_mutex_lock(fd->mutex);
	bytes_out = (unsigned) strlen(log_line_lf);

	if (fd->fd < 0) {
		do_reopen(fd);
		if (fd->fd < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening %s\n", path);
			goto end;
		}
	}

	if (fd->bytes + bytes_out > UINT_MAX) {
		do_rotate(fd);
	}

	while ((bytes_in = write(fd->fd, log_line_lf, bytes_out)) != bytes_out && ++loops < 10) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Write error to file %s %d/%d\n", path, (int) bytes_in, (int) bytes_out);
		do_rotate(fd);
		switch_yield(250000);
	}

	if (bytes_in > 0) {
		fd->bytes += bytes_in;
	}

  end:

	switch_mutex_unlock(fd->mutex);
	switch_safe_free(log_line_lf);
}

static switch_status_t insert_cdr(const char *values)
{
	char *sql = NULL, *path = NULL;
	PGresult *res;

	sql = switch_mprintf("INSERT INTO %s (%s) VALUES (%s);", globals.db_table, globals.db_schema->columns, values);
	assert(sql);

	if (globals.debug) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Query: \"%s\"\n", sql);
	}

	switch_mutex_lock(globals.db_mutex);

	if (!globals.db_online || PQstatus(globals.db_connection) != CONNECTION_OK) {
		globals.db_connection = PQconnectdb(globals.db_info);
	}

	if (PQstatus(globals.db_connection) == CONNECTION_OK) {
		globals.db_online = 1;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Connection to database failed: %s", PQerrorMessage(globals.db_connection));
		goto error;
	}

	res = PQexec(globals.db_connection, sql);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "INSERT command failed: %s", PQresultErrorMessage(res));
		PQclear(res);
		goto error;
	}

	PQclear(res);
	switch_safe_free(sql);

	switch_mutex_unlock(globals.db_mutex);

	return SWITCH_STATUS_SUCCESS;


  error:

	PQfinish(globals.db_connection);
	globals.db_online = 0;
	switch_mutex_unlock(globals.db_mutex);

	/* SQL INSERT failed for whatever reason. Spool the attempted query to disk */
	if (globals.spool_format == SPOOL_FORMAT_SQL) {
		path = switch_mprintf("%s%scdr-spool.sql", globals.spool_dir, SWITCH_PATH_SEPARATOR);
		assert(path);
		spool_cdr(path, sql);
	} else {
		path = switch_mprintf("%s%scdr-spool.csv", globals.spool_dir, SWITCH_PATH_SEPARATOR);
		assert(path);
		spool_cdr(path, values);
	}

	switch_safe_free(path);
	switch_safe_free(sql);

	return SWITCH_STATUS_FALSE;
}

static switch_status_t my_on_reporting(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *values = NULL, *tmp = NULL, *pq_var = NULL;
	const char *var = NULL;
	cdr_field_t *cdr_field = NULL;
	switch_size_t len, offset;

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

	if (switch_dir_make_recursive(globals.spool_dir, SWITCH_DEFAULT_DIR_PERMS, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error creating %s\n", globals.spool_dir);
		return SWITCH_STATUS_FALSE;
	}

	if (globals.debug) {
		switch_event_t *event;
		if (switch_event_create(&event, SWITCH_EVENT_COMMAND) == SWITCH_STATUS_SUCCESS) {
			char *buf;
			switch_channel_event_set_data(channel, event);
			switch_event_serialize(event, &buf, SWITCH_FALSE);
			switch_assert(buf);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "CHANNEL_DATA:\n%s\n", buf);
			switch_event_destroy(&event);
			switch_safe_free(buf);
		}
	}

	switch_zmalloc(values, 1);
	offset = 0;

	for (cdr_field = globals.db_schema->fields; cdr_field->var_name; cdr_field++) {
		if ((var = switch_channel_get_variable(channel, cdr_field->var_name))) {
			/* Allocate sufficient buffer for PQescapeString */
			len = strlen(var);
			tmp = switch_core_session_alloc(session, len * 2 + 1);
			PQescapeString(tmp, var, len);
			var = tmp;
		}

		if (cdr_field->quote) {
			if ((cdr_field->not_null == SWITCH_FALSE) && zstr(var)) {
				pq_var = switch_mprintf("null,", var);
			} else {
				pq_var = switch_mprintf("'%s',", var);
			}
		} else {
			pq_var = switch_mprintf("%s,", var);
		}

		/* Resize values buffer to accomodate next var */
		len = strlen(pq_var);
		tmp = realloc(values, offset + len);
		values = tmp;
		memcpy(values + offset, pq_var, len);
		switch_safe_free(pq_var);
		offset += len;
	}
	*(values + --offset) = '\0';

	insert_cdr(values);
	switch_safe_free(values);

	return status;
}


static void event_handler(switch_event_t *event)
{
	const char *sig = switch_event_get_header(event, "Trapped-Signal");
	switch_hash_index_t *hi;
	void *val;
	cdr_fd_t *fd;

	if (globals.shutdown) {
		return;
	}

	if (sig && !strcmp(sig, "HUP")) {
		for (hi = switch_core_hash_first(globals.fd_hash); hi; hi = switch_core_hash_next(&hi)) {
			switch_core_hash_this(hi, NULL, NULL, &val);
			fd = (cdr_fd_t *) val;
			switch_mutex_lock(fd->mutex);
			do_rotate(fd);
			switch_mutex_unlock(fd->mutex);
		}
		if (globals.db_online) {
			PQfinish(globals.db_connection);
			globals.db_online = 0;
		}
	}
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
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *cf = "cdr_pg_csv.conf", *ptr;
	switch_xml_t cfg, xml, schema, field;
	const char *attr;
	int num_fields = 0;
	switch_size_t len = 0;
	cdr_field_t *cdr_field;

	if (globals.db_online) {
		PQfinish(globals.db_connection);
		switch_mutex_destroy(globals.db_mutex);
		globals.db_online = 0;
	}

	memset(&globals, 0, sizeof(globals));
	switch_core_hash_init(&globals.fd_hash);
	switch_mutex_init(&globals.db_mutex, SWITCH_MUTEX_NESTED, pool);

	globals.pool = pool;

	if (switch_xml_config_parse_module_settings(cf, SWITCH_FALSE, config_settings) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	if ((xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		if ((schema = switch_xml_child(cfg, "schema"))) {
			/* Count fields in schema so we can calculate required buffer size */
			for (field = switch_xml_child(schema, "field"); field; field = field->next) {
				if (switch_xml_attr(field, "var")) {
					num_fields++;
				}
			}

			globals.db_schema = switch_core_alloc(pool, (num_fields + 1) * sizeof(cdr_field_t));
			cdr_field = globals.db_schema->fields;

			for (field = switch_xml_child(schema, "field"); field; field = field->next) {
				if ((attr = switch_xml_attr(field, "var"))) {
					cdr_field->var_name = switch_core_strdup(pool, attr);

					/* Assume SQL column name is the same as FreeSWITCH channel var name, unless specified otherwise */
					if ((attr = switch_xml_attr(field, "column"))) {
						cdr_field->col_name = switch_core_strdup(pool, attr);
					} else {
						cdr_field->col_name = switch_core_strdup(pool, cdr_field->var_name);
					}

					/* Assume all fields should be quoted (treated as strings), unless specified otherwise */
					if ((attr = switch_xml_attr(field, "quote")) && !strncmp(attr, "false", 5)) {
						cdr_field->quote = SWITCH_FALSE;
					} else {
						cdr_field->quote = SWITCH_TRUE;
					}

					/* Assume all fields allow SQL nulls, unless specified otherwise */
					if ((attr = switch_xml_attr(field, "not-null")) && !strncmp(attr, "true", 4)) {
						cdr_field->not_null = SWITCH_TRUE;
					} else {
						cdr_field->not_null = SWITCH_FALSE;
					}

					len += strlen(cdr_field->col_name) + 1;
					cdr_field++;
				}
			}
			cdr_field->var_name = 0;

			globals.db_schema->columns = switch_core_alloc(pool, len);
			ptr = globals.db_schema->columns;
			for (cdr_field = globals.db_schema->fields; cdr_field->col_name; cdr_field++) {
				len = strlen(cdr_field->col_name);
				memcpy(ptr, cdr_field->col_name, len);
				ptr += len;
				*ptr = ',';
				ptr++;
			}
			*--ptr = '\0';
		}

		switch_xml_free(xml);
	}

	return status;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_cdr_pg_csv_load)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	load_config(pool);

	if ((status = switch_dir_make_recursive(globals.spool_dir, SWITCH_DEFAULT_DIR_PERMS, pool)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error creating %s\n", globals.spool_dir);
		return status;
	}

	if ((status = switch_event_bind(modname, SWITCH_EVENT_TRAP, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return status;
	}

	switch_core_add_state_handler(&state_handlers);
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	return status;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cdr_pg_csv_shutdown)
{

	globals.shutdown = 1;

	if (globals.db_online) {
		PQfinish(globals.db_connection);
		globals.db_online = 0;
	}

	switch_event_unbind_callback(event_handler);
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
