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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Ken Rice <krice@freeswitch.org>
 * Mathieu Rene <mathieu.rene@gmail.com>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Rupa Schomaker <rupa@rupa.com>
 *
 * mod_db.c -- Implements simple db API, group support, and limit db backend
 *
 */

#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_db_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_db_shutdown);
SWITCH_MODULE_DEFINITION(mod_db, mod_db_load, mod_db_shutdown, NULL);

static struct {
	switch_memory_pool_t *pool;
	char hostname[256];
	char *dbname;
	char *odbc_dsn;
	switch_mutex_t *mutex;
	switch_mutex_t *db_hash_mutex;
	switch_hash_t *db_hash;
} globals;

typedef struct {
	uint32_t total_usage;
	uint32_t rate_usage;
	time_t last_check;
} limit_hash_item_t;

struct callback {
	char *buf;
	size_t len;
	int matches;
};

typedef struct callback callback_t;

static int sql2str_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	callback_t *cbt = (callback_t *) pArg;

	switch_copy_string(cbt->buf, argv[0], cbt->len);
	cbt->matches++;
	return 0;
}

static char limit_sql[] =
	"CREATE TABLE limit_data (\n"
	"   hostname   VARCHAR(255),\n" "   realm      VARCHAR(255),\n" "   id         VARCHAR(255),\n" "   uuid       VARCHAR(255)\n" ");\n";

static char db_sql[] =
	"CREATE TABLE db_data (\n"
	"   hostname   VARCHAR(255),\n" "   realm      VARCHAR(255),\n" "   data_key   VARCHAR(255),\n" "   data       VARCHAR(255)\n" ");\n";

static char group_sql[] =
	"CREATE TABLE group_data (\n" "   hostname   VARCHAR(255),\n" "   groupname  VARCHAR(255),\n" "   url        VARCHAR(255)\n" ");\n";


switch_cache_db_handle_t *limit_get_db_handle(void)
{
	switch_cache_db_handle_t *dbh = NULL;
	char *dsn;
	
	if (!zstr(globals.odbc_dsn)) {
		dsn = globals.odbc_dsn;
	} else {
		dsn = globals.dbname;
	}

	if (switch_cache_db_get_db_handle_dsn(&dbh, dsn) != SWITCH_STATUS_SUCCESS) {
		dbh = NULL;
	}
	
	return dbh;

}


static switch_status_t limit_execute_sql(char *sql)
{
	switch_cache_db_handle_t *dbh = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (!(dbh = limit_get_db_handle())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");
		goto end;
	}

	status = switch_cache_db_execute_sql(dbh, sql, NULL);

  end:

	switch_cache_db_release_db_handle(&dbh);

	return status;
}


static switch_bool_t limit_execute_sql_callback(char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	switch_bool_t ret = SWITCH_FALSE;
	char *errmsg = NULL;
	switch_cache_db_handle_t *dbh = NULL;

	if (!(dbh = limit_get_db_handle())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");
		goto end;
	}

	switch_cache_db_execute_sql_callback(dbh, sql, callback, pdata, &errmsg);

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s] %s\n", sql, errmsg);
		free(errmsg);
	}

  end:

	switch_cache_db_release_db_handle(&dbh);

	return ret;
}

static char * limit_execute_sql2str(char *sql, char *str, size_t len)
{
	callback_t cbt = { 0 };

	cbt.buf = str;
	cbt.len = len;

	limit_execute_sql_callback(sql, sql2str_callback, &cbt);
	
	return cbt.buf;
}

/* \brief Enforces limit restrictions
 * \param session current session
 * \param realm limit realm
 * \param id limit id
 * \param max maximum count
 * \return SWITCH_STATUS_SUCCESS if the access is allowed
 */
SWITCH_LIMIT_INCR(limit_incr_db)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	int got = 0;
	char *sql = NULL;
	char gotstr[128];
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_mutex_lock(globals.mutex);

	switch_channel_set_variable(channel, "limit_realm", realm);
	switch_channel_set_variable(channel, "limit_id", resource);
	switch_channel_set_variable(channel, "limit_max", switch_core_session_sprintf(session, "%d", max));

	sql = switch_mprintf("select count(hostname) from limit_data where realm='%q' and id='%q';", realm, resource);
	limit_execute_sql2str(sql, gotstr, 128);
	switch_safe_free(sql);
	got = atoi(gotstr);

	if (max < 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Usage for %s_%s is now %d\n", realm, resource, got + 1);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Usage for %s_%s is now %d/%d\n", realm, resource, got + 1, max);
	}

	if (max >= 0 && got + 1 > max) {
		status = SWITCH_STATUS_GENERR;
		goto done;
	}

	sql =
		switch_mprintf("insert into limit_data (hostname, realm, id, uuid) values('%q','%q','%q','%q');", globals.hostname, realm, resource,
					   switch_core_session_get_uuid(session));
	limit_execute_sql(sql);
	switch_safe_free(sql);

	{
		const char *susage = switch_core_session_sprintf(session, "%d", ++got);

		switch_channel_set_variable(channel, "limit_usage", susage);
		switch_channel_set_variable(channel, switch_core_session_sprintf(session, "limit_usage_%s_%s", realm, resource), susage);
	}
	switch_limit_fire_event("db", realm, resource, got, 0, max, 0);

  done:
	switch_mutex_unlock(globals.mutex);
	return status;
}

SWITCH_LIMIT_RELEASE(limit_release_db)
{
	char *sql = NULL;

	if (realm == NULL && resource == NULL) {
		sql = switch_mprintf("delete from limit_data where uuid='%q'", switch_core_session_get_uuid(session));
	} else {
		sql = switch_mprintf("delete from limit_data where uuid='%q' and realm='%q' and id = '%q'", switch_core_session_get_uuid(session), realm, resource);
	}
	limit_execute_sql(sql);
	switch_safe_free(sql);
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_LIMIT_USAGE(limit_usage_db)
{
	char usagestr[128] = "";
	int usage = 0;
	char *sql = NULL;
	
	sql = switch_mprintf("select count(hostname) from limit_data where realm='%q' and id='%q'", realm, resource);
	limit_execute_sql2str(sql, usagestr, sizeof(usagestr));
	switch_safe_free(sql);
	usage = atoi(usagestr);
	
	return usage;
}

SWITCH_LIMIT_RESET(limit_reset_db)
{
	char *sql = NULL;
	sql = switch_mprintf("delete from limit_data where hostname='%q';", globals.hostname);
	limit_execute_sql(sql);
	switch_safe_free(sql);
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_LIMIT_STATUS(limit_status_db)
{
	char count[128] = "";
	char *ret = NULL;
	char *sql = NULL;
	
	sql = switch_mprintf("select count(hostname) from limit_data where hostname='%q'", globals.hostname);
	limit_execute_sql2str(sql, count, sizeof(count));
	switch_safe_free(sql);
	ret = switch_mprintf("Tracking %s resources for hostname %s.", count, globals.hostname);
	return ret;
}

/* INIT / Config */

static switch_xml_config_string_options_t limit_config_dsn = { NULL, 0, "^pgsql|^odbc|^sqlite|[^:]+:[^:]+:.+" };

static switch_xml_config_item_t config_settings[] = {
	SWITCH_CONFIG_ITEM("odbc-dsn", SWITCH_CONFIG_STRING, 0, &globals.odbc_dsn, NULL, &limit_config_dsn,
					   "dsn:username:password", "If set, the ODBC DSN used by the limit and db applications"),
	SWITCH_CONFIG_ITEM_END()
};

static switch_status_t do_config()
{
	switch_cache_db_handle_t *dbh = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *sql = NULL;

	limit_config_dsn.pool = globals.pool;

	if (switch_xml_config_parse_module_settings("db.conf", SWITCH_FALSE, config_settings) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No config file found, defaulting to sqlite\n");
	}

	if (globals.odbc_dsn) {
		if (!(dbh = limit_get_db_handle())) {
			globals.odbc_dsn = NULL;
		}
	}


	if (zstr(globals.odbc_dsn)) {
		globals.dbname = "call_limit";
		dbh = limit_get_db_handle();
	}


	if (dbh) {
		int x = 0;
		char *indexes[] = {
			"create index ld_hostname on limit_data (hostname)",
			"create index ld_uuid on limit_data (uuid)",
			"create index ld_realm on limit_data (realm)",
			"create index ld_id on limit_data (id)",
			"create index dd_realm on db_data (realm)",
			"create unique index dd_data_key_realm on db_data (data_key,realm)",
			"create index gd_groupname on group_data (groupname)",
			"create index gd_url on group_data (url)",
			NULL
		};



		switch_cache_db_test_reactive(dbh, "select * from limit_data", NULL, limit_sql);
		switch_cache_db_test_reactive(dbh, "select * from db_data", NULL, db_sql);
		switch_cache_db_test_reactive(dbh, "select * from group_data", NULL, group_sql);

		for (x = 0; indexes[x]; x++) {
			switch_cache_db_execute_sql(dbh, indexes[x], NULL);
		}

		switch_cache_db_release_db_handle(&dbh);

		sql = switch_mprintf("delete from limit_data where hostname='%q';", globals.hostname);
		limit_execute_sql(sql);
		switch_safe_free(sql);
	}

	return status;
}

/* APP/API STUFF */

/* CORE DB STUFF */

SWITCH_STANDARD_API(db_api_function)
{
	int argc = 0;
	char *argv[4] = { 0 };
	char *mydata = NULL;
	char *sql;

	if (!zstr(cmd)) {
		mydata = strdup(cmd);
		switch_assert(mydata);
		argc = switch_separate_string(mydata, '/', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 1 || !argv[0]) {
		goto error;
	}

	if (!strcasecmp(argv[0], "insert")) {
		if (argc < 4) {
			goto error;
		}
		sql = switch_mprintf("delete from db_data where realm='%q' and data_key='%q'", argv[1], argv[2]);
		switch_assert(sql);
		limit_execute_sql(sql);
		switch_safe_free(sql);
		sql =
			switch_mprintf("insert into db_data (hostname, realm, data_key, data) values('%q','%q','%q','%q');", globals.hostname, argv[1], argv[2],
						   argv[3]);
		switch_assert(sql);
		limit_execute_sql(sql);
		switch_safe_free(sql);
		stream->write_function(stream, "+OK");
		goto done;
	} else if (!strcasecmp(argv[0], "delete")) {
		if (argc < 2) {
			goto error;
		}
		sql = switch_mprintf("delete from db_data where realm='%q' and data_key='%q'", argv[1], argv[2]);
		switch_assert(sql);
		limit_execute_sql(sql);
		switch_safe_free(sql);
		stream->write_function(stream, "+OK");
		goto done;
	} else if (!strcasecmp(argv[0], "select")) {
		char buf[256] = "";
		if (argc < 3) {
			goto error;
		}
		sql = switch_mprintf("select data from db_data where realm='%q' and data_key='%q'", argv[1], argv[2]);
		limit_execute_sql2str(sql, buf, sizeof(buf));
		switch_safe_free(sql);
		stream->write_function(stream, "%s", buf);
		goto done;
	} else if (!strcasecmp(argv[0], "exists")) {
		char buf[256] = "";
		if (argc < 3) {
			goto error;
		}
		sql = switch_mprintf("select data from db_data where realm='%q' and data_key='%q'", argv[1], argv[2]);
		limit_execute_sql2str(sql, buf, sizeof(buf));
		switch_safe_free(sql);
		if ( !strcmp(buf, "") ) {
			stream->write_function(stream, "false");
		}
		else {
			stream->write_function(stream, "true");
		}
		goto done;
	}

  error:
	stream->write_function(stream, "!err!");

  done:

	switch_safe_free(mydata);
	return SWITCH_STATUS_SUCCESS;
}

#define DB_USAGE "[insert|delete]/<realm>/<key>/<val>"
#define DB_DESC "save data"

SWITCH_STANDARD_APP(db_function)
{
	int argc = 0;
	char *argv[4] = { 0 };
	char *mydata = NULL;
	char *sql = NULL;

	if (!zstr(data)) {
		mydata = switch_core_session_strdup(session, data);
		argc = switch_separate_string(mydata, '/', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 3 || !argv[0]) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "USAGE: db %s\n", DB_USAGE);
		return;
	}

	if (!strcasecmp(argv[0], "insert")) {
		if (argc < 4) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "USAGE: db %s\n", DB_USAGE);
			return;
		}
		sql = switch_mprintf("delete from db_data where realm='%q' and data_key='%q'", argv[1], argv[2]);
		switch_assert(sql);
		limit_execute_sql(sql);
		switch_safe_free(sql);

		sql =
			switch_mprintf("insert into db_data (hostname, realm, data_key, data) values('%q','%q','%q','%q');", globals.hostname, argv[1], argv[2],
						   argv[3]);
	} else if (!strcasecmp(argv[0], "delete")) {
		sql = switch_mprintf("delete from db_data where realm='%q' and data_key='%q'", argv[1], argv[2]);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "USAGE: db %s\n", DB_USAGE);
		return;
	}

	if (sql) {
		limit_execute_sql(sql);
		switch_safe_free(sql);
	}
}

/* GROUP STUFF */

static int group_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	callback_t *cbt = (callback_t *) pArg;
	switch_snprintf(cbt->buf + strlen(cbt->buf), cbt->len - strlen(cbt->buf), "%s%c", argv[0], *argv[1]);
	cbt->matches++;
	return 0;
}

SWITCH_STANDARD_API(group_api_function)
{
	int argc = 0;
	char *argv[4] = { 0 };
	char *mydata = NULL;
	char *sql;

	if (!zstr(cmd)) {
		mydata = strdup(cmd);
		argc = switch_separate_string(mydata, ':', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 2 || !argv[0]) {
		goto error;
	}

	if (!strcasecmp(argv[0], "insert")) {
		if (argc < 3) {
			goto error;
		}
		sql = switch_mprintf("delete from group_data where groupname='%q' and url='%q';", argv[1], argv[2]);
		switch_assert(sql);

		limit_execute_sql(sql);
		switch_safe_free(sql);
		sql = switch_mprintf("insert into group_data (hostname, groupname, url) values('%q','%q','%q');", globals.hostname, argv[1], argv[2]);
		switch_assert(sql);
		limit_execute_sql(sql);
		switch_safe_free(sql);
		stream->write_function(stream, "+OK");
		goto done;
	} else if (!strcasecmp(argv[0], "delete")) {
		if (argc < 3) {
			goto error;
		}
		if (!strcmp(argv[2], "*")) {
			sql = switch_mprintf("delete from group_data where groupname='%q';", argv[1]);
		} else {
			sql = switch_mprintf("delete from group_data where groupname='%q' and url='%q';", argv[1], argv[2]);
		}
		switch_assert(sql);
		limit_execute_sql(sql);
		switch_safe_free(sql);
		stream->write_function(stream, "+OK");
		goto done;
	} else if (!strcasecmp(argv[0], "call")) {
		char buf[4096] = "";
		char *how = ",";
		callback_t cbt = { 0 };
		cbt.buf = buf;
		cbt.len = sizeof(buf);

		if (argc > 2) {
			if (!strcasecmp(argv[2], "order")) {
				how = "|";
			}
		}

		sql = switch_mprintf("select url,'%q' from group_data where groupname='%q'", how, argv[1]);
		switch_assert(sql);

		limit_execute_sql_callback(sql, group_callback, &cbt);
		switch_safe_free(sql);

		if (!zstr(buf)) {
			*(buf + (strlen(buf) - 1)) = '\0';
    }

		stream->write_function(stream, "%s", buf);

		goto done;
	}

  error:
	stream->write_function(stream, "!err!");

  done:

	switch_safe_free(mydata);
	return SWITCH_STATUS_SUCCESS;
}

#define GROUP_USAGE "[insert|delete]:<group name>:<val>"
#define GROUP_DESC "save data"

SWITCH_STANDARD_APP(group_function)
{
	int argc = 0;
	char *argv[3] = { 0 };
	char *mydata = NULL;
	char *sql;

	if (!zstr(data)) {
		mydata = switch_core_session_strdup(session, data);
		argc = switch_separate_string(mydata, ':', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 3 || !argv[0]) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "USAGE: group %s\n", DB_USAGE);
		return;
	}

	if (!strcasecmp(argv[0], "insert")) {
		sql = switch_mprintf("insert into group_data (hostname, groupname, url) values('%q','%q','%q');", globals.hostname, argv[1], argv[2]);
		switch_assert(sql);
		limit_execute_sql(sql);
		switch_safe_free(sql);
	} else if (!strcasecmp(argv[0], "delete")) {
		sql = switch_mprintf("delete from group_data where groupname='%q' and url='%q';", argv[1], argv[2]);
		switch_assert(sql);
		limit_execute_sql(sql);
		switch_safe_free(sql);
	}
}

/* INIT/DEINIT STUFF */

SWITCH_MODULE_LOAD_FUNCTION(mod_db_load)
{
	switch_status_t status;
	switch_application_interface_t *app_interface;
	switch_api_interface_t *commands_api_interface;
	switch_limit_interface_t *limit_interface;

	memset(&globals, 0, sizeof(globals));
	strncpy(globals.hostname, switch_core_get_switchname(), sizeof(globals.hostname));
	globals.pool = pool;


	if ((status = do_config() != SWITCH_STATUS_SUCCESS)) {
		return status;
	}

	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);
	switch_mutex_init(&globals.db_hash_mutex, SWITCH_MUTEX_NESTED, globals.pool);
	switch_core_hash_init(&globals.db_hash, pool);

	status = switch_event_reserve_subclass(LIMIT_EVENT_USAGE);
	if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_INUSE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register event subclass \"%s\" (%d)\n", LIMIT_EVENT_USAGE, status);
		return SWITCH_STATUS_FALSE;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	/* register limit interfaces */
	SWITCH_ADD_LIMIT(limit_interface, "db", limit_incr_db, limit_release_db, limit_usage_db, limit_reset_db, limit_status_db, NULL);

	SWITCH_ADD_APP(app_interface, "db", "Insert to the db", DB_DESC, db_function, DB_USAGE, SAF_SUPPORT_NOMEDIA | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "group", "Manage a group", GROUP_DESC, group_function, GROUP_USAGE, SAF_SUPPORT_NOMEDIA | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_API(commands_api_interface, "db", "db get/set", db_api_function, "[insert|delete|select]/<realm>/<key>/<value>");
	switch_console_set_complete("add db insert");
	switch_console_set_complete("add db delete");
	switch_console_set_complete("add db select");
	switch_console_set_complete("add db exists");
	SWITCH_ADD_API(commands_api_interface, "group", "group [insert|delete|call]", group_api_function, "[insert|delete|call]:<group name>:<url>");
	switch_console_set_complete("add group insert");
	switch_console_set_complete("add group delete");
	switch_console_set_complete("add group call");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
	
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_db_shutdown)
{

	switch_xml_config_cleanup(config_settings);

	switch_mutex_destroy(globals.mutex);
	switch_mutex_destroy(globals.db_hash_mutex);

	switch_core_hash_destroy(&globals.db_hash);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
