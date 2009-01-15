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
 * Ken Rice <krice at suspicious dot org
 * Mathieu Rene <mathieu.rene@gmail.com>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 *
 * mod_limit.c -- Resource Limit Module
 *
 */

#include <switch.h>
#ifdef SWITCH_HAVE_ODBC
#include <switch_odbc.h>
#endif

SWITCH_MODULE_LOAD_FUNCTION(mod_limit_load);
SWITCH_MODULE_DEFINITION(mod_limit, mod_limit_load, NULL, NULL);

static struct {
	switch_memory_pool_t *pool;
	char hostname[256];
	char *dbname;
	char *odbc_dsn;
	switch_mutex_t *mutex;
	switch_mutex_t *limit_hash_mutex;
	switch_hash_t *limit_hash;	
	switch_mutex_t *db_hash_mutex;
	switch_hash_t *db_hash;	
#ifdef SWITCH_HAVE_ODBC
	switch_odbc_handle_t *master_odbc;
#else
	void *filler1;
#endif
} globals;

struct limit_hash_item  {
	uint32_t total_usage;
	uint32_t rate_usage;
	time_t last_check;
};
typedef struct limit_hash_item limit_hash_item_t;

 
static char limit_sql[] =
	"CREATE TABLE limit_data (\n"
	"   hostname   VARCHAR(255),\n" "   realm      VARCHAR(255),\n" "   id         VARCHAR(255),\n" "   uuid       VARCHAR(255)\n" ");\n";

static char db_sql[] =
	"CREATE TABLE db_data (\n"
	"   hostname   VARCHAR(255),\n" "   realm      VARCHAR(255),\n" "   data_key   VARCHAR(255),\n" "   data       VARCHAR(255)\n" ");\n";

static char group_sql[] =
	"CREATE TABLE group_data (\n" "   hostname   VARCHAR(255),\n" "   groupname  VARCHAR(255),\n" "   url        VARCHAR(255)\n" ");\n";

static switch_status_t limit_execute_sql(char *sql, switch_mutex_t *mutex)
{
	switch_core_db_t *db;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (mutex) {
		switch_mutex_lock(mutex);
	}
#ifdef SWITCH_HAVE_ODBC
	if (globals.odbc_dsn) {
		SQLHSTMT stmt;
		if (switch_odbc_handle_exec(globals.master_odbc, sql, &stmt) != SWITCH_ODBC_SUCCESS) {
			char *err_str;
			err_str = switch_odbc_handle_get_error(globals.master_odbc, stmt);
			if (!switch_strlen_zero(err_str)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERR: [%s]\n[%s]\n", sql, switch_str_nil(err_str));
			}
			switch_safe_free(err_str);
			status = SWITCH_STATUS_FALSE;
		}
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	} else {
#endif
		if (!(db = switch_core_db_open_file(globals.dbname))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", globals.dbname);
			status = SWITCH_STATUS_FALSE;
			goto end;
		}

		status = switch_core_db_persistant_execute(db, sql, 1);
		switch_core_db_close(db);

#ifdef SWITCH_HAVE_ODBC
	}
#endif

  end:
	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return status;
}

static switch_bool_t limit_execute_sql_callback(switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	switch_bool_t ret = SWITCH_FALSE;
	switch_core_db_t *db;
	char *errmsg = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

#ifdef SWITCH_HAVE_ODBC
	if (globals.odbc_dsn) {
		switch_odbc_handle_callback_exec(globals.master_odbc, sql, callback, pdata);
	} else {
#endif
		if (!(db = switch_core_db_open_file(globals.dbname))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", globals.dbname);
			goto end;
		}

		switch_core_db_exec(db, sql, callback, pdata, &errmsg);

		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s] %s\n", sql, errmsg);
			free(errmsg);
		}

		if (db) {
			switch_core_db_close(db);
		}
#ifdef SWITCH_HAVE_ODBC
	}
#endif

  end:
	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return ret;
}

static switch_status_t do_config()
{
	char *cf = "limit.conf";
	switch_xml_t cfg, xml, settings, param;
	switch_core_db_t *db;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *odbc_user = NULL;
	char *odbc_pass = NULL;
	char *sql = NULL;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = NULL;
			char *val = NULL;

			var = (char *) switch_xml_attr_soft(param, "name");
			val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "odbc-dsn") && !switch_strlen_zero(val)) {
#ifdef SWITCH_HAVE_ODBC
				globals.odbc_dsn = switch_core_strdup(globals.pool, val);
				if ((odbc_user = strchr(globals.odbc_dsn, ':'))) {
					*odbc_user++ = '\0';
					if ((odbc_pass = strchr(odbc_user, ':'))) {
						*odbc_pass++ = '\0';
					}
				}
#else
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ODBC IS NOT AVAILABLE!\n");
#endif
			}
		}
	}

	if (switch_strlen_zero(globals.odbc_dsn) || switch_strlen_zero(odbc_user) || switch_strlen_zero(odbc_pass)) {
		globals.dbname = "call_limit";
	}

#ifdef SWITCH_HAVE_ODBC
	if (globals.odbc_dsn) {
		if (!(globals.master_odbc = switch_odbc_handle_new(globals.odbc_dsn, odbc_user, odbc_pass))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open ODBC Database!\n");
			status = SWITCH_STATUS_FALSE;
			goto done;
		}
		if (switch_odbc_handle_connect(globals.master_odbc) != SWITCH_ODBC_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open ODBC Database!\n");
			status = SWITCH_STATUS_FALSE;
			goto done;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Connected ODBC DSN: %s\n", globals.odbc_dsn);
		if (switch_odbc_handle_exec(globals.master_odbc, "select count(*) from limit_data", NULL) != SWITCH_STATUS_SUCCESS) {
			if (switch_odbc_handle_exec(globals.master_odbc, limit_sql, NULL) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Create SQL Database!\n");
			}
		}
		if (switch_odbc_handle_exec(globals.master_odbc, "select count(*) from db_data", NULL) != SWITCH_STATUS_SUCCESS) {
			if (switch_odbc_handle_exec(globals.master_odbc, db_sql, NULL) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Create SQL Database!\n");
			}
		}
		if (switch_odbc_handle_exec(globals.master_odbc, "select count(*) from group_data", NULL) != SWITCH_STATUS_SUCCESS) {
			if (switch_odbc_handle_exec(globals.master_odbc, group_sql, NULL) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Create SQL Database!\n");
			}
		}
	} else {
#endif
		if ((db = switch_core_db_open_file(globals.dbname))) {
			switch_core_db_test_reactive(db, "select * from limit_data", NULL, limit_sql);
			switch_core_db_test_reactive(db, "select * from db_data", NULL, db_sql);
			switch_core_db_test_reactive(db, "select * from group_data", NULL, group_sql);
			
			switch_core_db_exec(db, "create index if not exists ld_hostname on limit_data (hostname)", NULL, NULL, NULL);
			switch_core_db_exec(db, "create index if not exists ld_uuid on limit_data (uuid)", NULL, NULL, NULL);
			switch_core_db_exec(db, "create index if not exists ld_realm on limit_data (realm)", NULL, NULL, NULL);
			switch_core_db_exec(db, "create index if not exists ld_id on limit_data (id)", NULL, NULL, NULL);

			switch_core_db_exec(db, "create index if not exists dd_realm on db_data (realm)", NULL, NULL, NULL);
			switch_core_db_exec(db, "create index if not exists dd_data_key on db_data (data_key)", NULL, NULL, NULL);

			switch_core_db_exec(db, "create index if not exists gd_groupname on group_data (groupname)", NULL, NULL, NULL);
			switch_core_db_exec(db, "create index if not exists gd_url on group_data (url)", NULL, NULL, NULL);

		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open SQL Database!\n");
			status = SWITCH_STATUS_FALSE;
			goto done;
		}
		switch_core_db_close(db);
#ifdef SWITCH_HAVE_ODBC
	}
#endif

  done:

	sql = switch_mprintf("delete from limit_data where hostname='%q';", globals.hostname);
	limit_execute_sql(sql, globals.mutex);
	switch_safe_free(sql);

	switch_xml_free(xml);

	return status;
}

static switch_status_t db_state_handler(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t state = switch_channel_get_state(channel);
	char *sql = NULL;

	if (state == CS_HANGUP || state == CS_ROUTING) {
		sql = switch_mprintf("delete from limit_data where uuid='%q';",
							 switch_core_session_get_uuid(session));
		limit_execute_sql(sql, globals.mutex);
		switch_safe_free(sql);
		switch_core_event_hook_remove_state_change(session, db_state_handler);
		/* Remove limit_realm variable so we register another hook if limit is called again */
		switch_channel_set_variable(channel, "limit_realm", "");
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t hash_state_handler(switch_core_session_t *session) 
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t state = switch_channel_get_state(channel);
	switch_hash_t *channel_hash = switch_channel_get_private(channel, "limit_hash");
	
	/* The call is either hung up, or is going back into the dialplan, decrement appropriate couters */
	if (state == CS_HANGUP || state == CS_ROUTING) {	
		switch_hash_index_t *hi;
		switch_mutex_lock(globals.limit_hash_mutex);

		/* Loop through the channel's hashtable which contains mapping to all the limit_hash_item_t referenced by that channel */
		for(hi = switch_hash_first(NULL, channel_hash); hi; hi = switch_hash_next(hi))
		{
			void *val = NULL;
			const void *key;
			switch_ssize_t keylen;
			limit_hash_item_t *item = NULL;
			
			switch_hash_this(hi, &key, &keylen, &val);
			
			item = (limit_hash_item_t*)val;
			
			/* We keep the structure even though the count is 0 so we do not allocate too often */
			item->total_usage--;	
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Usage for %s is now %d\n", (const char*)key, item->total_usage);	
		}
		switch_core_event_hook_remove_state_change(session, hash_state_handler);
		switch_mutex_unlock(globals.limit_hash_mutex);
	}
	
	return SWITCH_STATUS_SUCCESS;
}

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

static int group_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	callback_t *cbt = (callback_t *) pArg;
	switch_snprintf(cbt->buf + strlen(cbt->buf), cbt->len - strlen(cbt->buf), "%s%c", argv[0], *argv[1]);
	cbt->matches++;
	return 0;
}

SWITCH_STANDARD_API(db_api_function)
{
	int argc = 0;
	char *argv[4] = { 0 };
	char *mydata = NULL;
	char *sql;

	switch_mutex_lock(globals.mutex);

	if (!switch_strlen_zero(cmd)) {
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
		assert(sql);
		limit_execute_sql(sql, NULL);
		switch_safe_free(sql);
		sql =
			switch_mprintf("insert into db_data (hostname, realm, data_key, data) values('%q','%q','%q','%q');", globals.hostname, argv[1], argv[2],
						   argv[3]);
		assert(sql);
		limit_execute_sql(sql, NULL);
		switch_safe_free(sql);
		stream->write_function(stream, "+OK");
		goto done;
	} else if (!strcasecmp(argv[0], "delete")) {
		if (argc < 2) {
			goto error;
		}
		sql = switch_mprintf("delete from db_data where realm='%q' and data_key='%q'", argv[1], argv[2]);
		assert(sql);
		limit_execute_sql(sql, NULL);
		switch_safe_free(sql);
		stream->write_function(stream, "+OK");
		goto done;
	} else if (!strcasecmp(argv[0], "select")) {
		char buf[256] = "";
		callback_t cbt = { 0 };
		if (argc < 3) {
			goto error;
		}
		cbt.buf = buf;
		cbt.len = sizeof(buf);
		sql = switch_mprintf("select data from db_data where realm='%q' and data_key='%q'", argv[1], argv[2]);
		limit_execute_sql_callback(NULL, sql, sql2str_callback, &cbt);
		stream->write_function(stream, "%s", buf);
		goto done;
	}

  error:
	stream->write_function(stream, "!err!");

  done:

	switch_mutex_unlock(globals.mutex);
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

	if (!switch_strlen_zero(data)) {
		mydata = switch_core_session_strdup(session, data);
		argc = switch_separate_string(mydata, '/', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 3 || !argv[0]) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "USAGE: db %s\n", DB_USAGE);
		return;
	}

	if (!strcasecmp(argv[0], "insert")) {
		if (argc < 4) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "USAGE: db %s\n", DB_USAGE);
			return;
		}
		sql = switch_mprintf("delete from db_data where realm='%q' and data_key='%q'", argv[1], argv[2]);
		switch_assert(sql);
		limit_execute_sql(sql, globals.mutex);
		switch_safe_free(sql);

		sql =
			switch_mprintf("insert into db_data (hostname, realm, data_key, data) values('%q','%q','%q','%q');", globals.hostname, argv[1], argv[2],
						   argv[3]);
	} else if (!strcasecmp(argv[0], "delete")) {
		sql = switch_mprintf("delete from db_data where realm='%q' and data_key='%q'", argv[1], argv[2]);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "USAGE: db %s\n", DB_USAGE);
		return;
	}

	switch_assert(sql);
	limit_execute_sql(sql, globals.mutex);
	switch_safe_free(sql);
}

#define HASH_USAGE "[insert|delete]/<realm>/<key>/<val>"
#define HASH_DESC "save data"

SWITCH_STANDARD_APP(hash_function)
{
	int argc = 0;
	char *argv[4] = { 0 };
	char *mydata = NULL;
	char *hash_key = NULL;
	char *value = NULL;
	
	if (!switch_strlen_zero(data)) {
		mydata = strdup(data);
		switch_assert(mydata);
		argc = switch_separate_string(mydata, '/', argv, (sizeof(argv) / sizeof(argv[0])));
	}
	
	if (argc < 3 || !argv[0]) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "USAGE: hash %s\n", HASH_USAGE);
		goto end;
	}
	
	hash_key = switch_mprintf("%s_%s", argv[1], argv[2]);
	
	switch_mutex_lock(globals.db_hash_mutex);
	if (!strcasecmp(argv[0], "insert")) {
		if ((value = switch_core_hash_find(globals.db_hash, hash_key))) {
			free(value);
			switch_core_hash_delete(globals.db_hash, hash_key);
		}
		value = strdup(argv[3]);
		switch_assert(value);
		switch_core_hash_insert(globals.db_hash, hash_key, value);
	} else if (!strcasecmp(argv[0], "delete")) {
		if ((value = switch_core_hash_find(globals.db_hash, hash_key))) {
			switch_safe_free(value);
			switch_core_hash_delete(globals.db_hash, hash_key);
		}
	}
	switch_mutex_unlock(globals.db_hash_mutex);
	
end:
	switch_safe_free(mydata);
	switch_safe_free(hash_key);
}

#define HASH_API_USAGE "insert|select|delete/realm/key[/value]"
SWITCH_STANDARD_API(hash_api_function)
{
	int argc = 0;
	char *argv[4] = { 0 };
	char *mydata = NULL;
	char *value = NULL;
	char *hash_key = NULL;

	switch_mutex_lock(globals.db_hash_mutex);

	if (!switch_strlen_zero(cmd)) {
		mydata = strdup(cmd);
		switch_assert(mydata);
		argc = switch_separate_string(mydata, '/', argv, (sizeof(argv) / sizeof(argv[0])));
	}
	
	if (argc < 3 || !argv[0]) {
		stream->write_function(stream, "-ERR Usage: hash %s\n", HASH_API_USAGE);
		goto end;
	}
	
	hash_key = switch_mprintf("%s_%s", argv[1], argv[2]);
	
	if (!strcasecmp(argv[0], "insert")) {
		if (argc < 4) {
			stream->write_function(stream, "-ERR Usage: hash %s\n", HASH_API_USAGE);
			goto end;
		}
		if ((value = switch_core_hash_find(globals.db_hash, hash_key))) {
			switch_safe_free(value);
			switch_core_hash_delete(globals.db_hash, hash_key);
		}
		value = strdup(argv[3]);
		switch_assert(value);
		switch_core_hash_insert(globals.db_hash, hash_key, value);
		stream->write_function(stream, "+OK\n");
	} else if (!strcasecmp(argv[0], "delete")) {
		if ((value = switch_core_hash_find(globals.db_hash, hash_key))) {
			switch_safe_free(value);
			switch_core_hash_delete(globals.db_hash, hash_key);
		}
		stream->write_function(stream, "+OK\n");
	} else if (!strcasecmp(argv[0], "select")) {
		if ((value = switch_core_hash_find(globals.db_hash, hash_key))) {
			stream->write_function(stream, "%s", value);
		}
	}
	
end:
	switch_mutex_unlock(globals.db_hash_mutex);
	switch_safe_free(mydata);
	switch_safe_free(hash_key);
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(group_api_function)
{
	int argc = 0;
	char *argv[4] = { 0 };
	char *mydata = NULL;
	char *sql;

	switch_mutex_lock(globals.mutex);

	if (!switch_strlen_zero(cmd)) {
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
		assert(sql);

		limit_execute_sql(sql, NULL);
		switch_safe_free(sql);
		sql = switch_mprintf("insert into group_data (hostname, groupname, url) values('%q','%q','%q');", globals.hostname, argv[1], argv[2]);
		assert(sql);
		limit_execute_sql(sql, NULL);
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
		assert(sql);
		limit_execute_sql(sql, NULL);
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
		assert(sql);

		limit_execute_sql_callback(NULL, sql, group_callback, &cbt);
		*(buf + (strlen(buf) - 1)) = '\0';
		stream->write_function(stream, "%s", buf);
		goto done;
	}

  error:
	stream->write_function(stream, "!err!");

  done:

	switch_mutex_unlock(globals.mutex);
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

	if (!switch_strlen_zero(data)) {
		mydata = switch_core_session_strdup(session, data);
		argc = switch_separate_string(mydata, ':', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 3 || !argv[0]) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "USAGE: group %s\n", DB_USAGE);
		return;
	}

	if (!strcasecmp(argv[0], "insert")) {
		sql = switch_mprintf("insert into group_data (hostname, groupname, url) values('%q','%q','%q');", globals.hostname, argv[1], argv[2]);
		assert(sql);
		limit_execute_sql(sql, globals.mutex);
		switch_safe_free(sql);
	} else if (!strcasecmp(argv[0], "delete")) {
		sql = switch_mprintf("delete from group_data where groupname='%q' and url='%q';", argv[1], argv[2]);
		assert(sql);
		limit_execute_sql(sql, globals.mutex);
		switch_safe_free(sql);
	}
}

#define LIMIT_USAGE "<realm> <id> <max> [number  [dialplan [context]]]"
#define LIMIT_DESC "limit access to a resource and transfer to an extension if the limit is exceeded"
static char *limit_def_xfer_exten = "limit_exceeded";

SWITCH_STANDARD_APP(limit_function)
{
	int argc = 0;
	char *argv[6] = { 0 };
	char *mydata = NULL;
	char *sql = NULL;
	char *realm = NULL;
	char *id = NULL;
	char *xfer_exten = NULL;
	int max = 0, got = 0;
	char buf[80] = "";
	callback_t cbt = { 0 };
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_bool_t new_channel = SWITCH_FALSE;

	if (!switch_strlen_zero(data)) {
		mydata = switch_core_session_strdup(session, data);
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 3) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "USAGE: limit %s\n", LIMIT_USAGE);
		return;
	}

	switch_mutex_lock(globals.mutex);

	realm = argv[0];
	id = argv[1];
	max = atoi(argv[2]);

	if (argc >= 4) {
		xfer_exten = argv[3];
	} else {
		xfer_exten = limit_def_xfer_exten;
	}

	if (max < 0) {
		max = 0;
	}
	
	new_channel = !switch_channel_get_variable(channel, "limit_realm");
	switch_channel_set_variable(channel, "limit_realm", realm);
	switch_channel_set_variable(channel, "limit_id", id);
	switch_channel_set_variable(channel, "limit_max", argv[2]);

	cbt.buf = buf;
	cbt.len = sizeof(buf);
	sql = switch_mprintf("select count(hostname) from limit_data where realm='%q' and id like '%q'", realm, id);
	limit_execute_sql_callback(NULL, sql, sql2str_callback, &cbt);
	got = atoi(buf);

	if (got + 1 > max) {
		switch_ivr_session_transfer(session, xfer_exten, argv[4], argv[5]);
		goto done;
	}

	if (new_channel) {
		switch_core_event_hook_add_state_change(session, db_state_handler);
	}
	sql =
		switch_mprintf("insert into limit_data (hostname, realm, id, uuid) values('%q','%q','%q','%q');", globals.hostname, realm, id,
					   switch_core_session_get_uuid(session));
	limit_execute_sql(sql, NULL);
	switch_safe_free(sql);

  done:
	switch_mutex_unlock(globals.mutex);
}


#define LIMIT_USAGE_USAGE "<realm> <id>"
SWITCH_STANDARD_API(limit_usage_function)
{
	int argc = 0;
	char *argv[6] = { 0 };
	char *mydata = NULL;
	char *sql = NULL;
	char *realm = NULL;
	char *id = NULL;
	char buf[80] = "";
	callback_t cbt = { 0 };


	if (!switch_strlen_zero(cmd)) {
		mydata = strdup(cmd);
		switch_assert(mydata);
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 2) {
		stream->write_function(stream, "USAGE: limit_usage %s\n", LIMIT_USAGE_USAGE);
		goto end;
	}


	realm = argv[0];
	id = argv[1];

	cbt.buf = buf;
	cbt.len = sizeof(buf);
	sql = switch_mprintf("select count(hostname) from limit_data where realm='%q' and id like '%q'", realm, id);
	limit_execute_sql_callback(NULL, sql, sql2str_callback, &cbt);
	switch_safe_free(sql);

	stream->write_function(stream, "%s", buf);

end:
	switch_safe_free(mydata);
	return SWITCH_STATUS_SUCCESS;
}

#define LIMITHASH_USAGE "<realm> <id> [<max>[/interval]] [number [dialplan [context]]]"
#define LIMITHASH_DESC "limit access to a resource and transfer to an extension if the limit is exceeded"
SWITCH_STANDARD_APP(limit_hash_function)
{
	int argc = 0;
	char *argv[6] = { 0 };
	char *mydata = NULL;
	char *realm = NULL;
	char *id = NULL;
	char *hashkey = NULL;
	char *xfer_exten = NULL;
	int max = 0;
	int interval = 0;
	char *szinterval = NULL;
	limit_hash_item_t *item = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	time_t now = switch_timestamp(NULL);
	switch_hash_t *channel_hash = NULL;
	uint8_t increment = 1;
	uint8_t new_channel = 0;
	
	/* Parse application data  */
	if (!switch_strlen_zero(data)) {
		mydata = switch_core_session_strdup(session, data);
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}
	
	if (argc < 3) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "USAGE: limit_hash %s\n", LIMITHASH_USAGE);
		return;
	}
	
	realm = argv[0];
	id = argv[1];
	
	/* If max is omitted, only act as a counter and skip maximum checks */
	if (argc > 2) {
		if ((szinterval = strchr(argv[2], '/')))
		{
			*szinterval++ = '\0';
			interval = atoi(szinterval);
		}
		
		max = atoi(argv[2]);
		
		if (max < 0) {
			max = 0;
		}
	}

	if (argc > 3) {
		xfer_exten = argv[3];
	} else {
		xfer_exten = limit_def_xfer_exten;
	}
	
	hashkey = switch_core_session_sprintf(session, "%s_%s", realm, id);
	
	switch_mutex_lock(globals.limit_hash_mutex);
	/* Check if that realm+id has ever been checked */
	if (!(item = (limit_hash_item_t*)switch_core_hash_find(globals.limit_hash, hashkey))) {
		/* No, create an empty structure and add it, then continue like as if it existed */
		item = (limit_hash_item_t*)switch_core_alloc(globals.pool, sizeof(limit_hash_item_t));
		memset(item, 0, sizeof(limit_hash_item_t));
		switch_core_hash_insert(globals.limit_hash, hashkey, item);
	}
	
	/* Did we already run on this channel before? */
	if ((channel_hash = switch_channel_get_private(channel, "limit_hash")))
	{
		/* Yes, but check if we did that realm+id
		   If we didnt, allow incrementing the counter.
		   If we did, dont touch it but do the validation anyways
		 */
		increment = !switch_core_hash_find(channel_hash, hashkey);
	} else {
		/* This is the first limit check on this channel, create a hashtable, set our prviate data and add a state handler */
		new_channel = 1;
	}

	if (interval > 0) {
		if (item->last_check <= (now - interval)) {
			item->rate_usage = 1;
			item->last_check = now;
		} else {
			/* Always increment rate when its checked as it doesnt depend on the channel */
			item->rate_usage++;
			
			if ((max >= 0) && (item->rate_usage > (uint32_t)max)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage for %s exceeds maximum rate of %d/%ds, now at %d\n", hashkey, max, interval, item->rate_usage);
				switch_ivr_session_transfer(session, xfer_exten, argv[4], argv[5]);
				goto end;
			}
		}
	} else if ((max >= 0) && (item->total_usage + increment > (uint32_t)max)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Usage for %s is already at max value (%d)\n", hashkey, item->total_usage);
		switch_ivr_session_transfer(session, xfer_exten, argv[4], argv[5]);
		goto end;
	}

	if (increment) {
		item->total_usage++;
		
		if (!new_channel)  {
			switch_core_hash_insert(channel_hash, hashkey, item);
		}
		
		if (max == -1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Usage for %s is now %d\n", hashkey, item->total_usage);
		} else if (interval == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Usage for %s is now %d/%d\n", hashkey, item->total_usage, max);	
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Usage for %s is now %d/%d for the last %d seconds\n", hashkey, item->rate_usage, max, interval);
		}
	}
	
	/* Save current usage & rate into channel variables so it can be used later in the dialplan, or added to CDR records */
	switch_channel_set_variable(channel, "limit_usage", switch_core_session_sprintf(session, "%d", item->total_usage));
	switch_channel_set_variable(channel, "limit_rate", switch_core_session_sprintf(session, "%d", item->rate_usage));

	if (new_channel) {
		switch_core_hash_init(&channel_hash, switch_core_session_get_pool(session));
		switch_core_hash_insert(channel_hash, hashkey, item);
		switch_channel_set_private(channel, "limit_hash", channel_hash);
		switch_core_event_hook_add_state_change(session, hash_state_handler);
	}
	
end:	
	switch_mutex_unlock(globals.limit_hash_mutex);
}

#define LIMIT_HASH_USAGE_USAGE "<realm> <id>"
SWITCH_STANDARD_API(limit_hash_usage_function)
{
	int argc = 0;
	char *argv[3] = { 0 };
	char *mydata = NULL;
	char *hash_key = NULL;
	limit_hash_item_t *item = NULL;
	uint32_t count = 0;

	switch_mutex_lock(globals.limit_hash_mutex);
	
	if (!switch_strlen_zero(cmd)) {
		mydata = strdup(cmd);
		switch_assert(mydata);
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}
	
	if (argc < 2) {
		stream->write_function(stream, "USAGE: limit_hash_usage %s\n", LIMIT_HASH_USAGE_USAGE);
		goto end;
	}
	
	hash_key = switch_mprintf("%s_%s", argv[0], argv[1]);
	
	if ((item = switch_core_hash_find(globals.limit_hash, hash_key))) {
		count = item->total_usage;
	} 
	
	stream->write_function(stream, "%d", count);
	
end:
	switch_safe_free(mydata);
	switch_safe_free(hash_key);
	switch_mutex_unlock(globals.limit_hash_mutex);
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_limit_load)
{
	switch_status_t status;
	switch_application_interface_t *app_interface;
	switch_api_interface_t *commands_api_interface;

	memset(&globals, 0, sizeof(&globals));
	gethostname(globals.hostname, sizeof(globals.hostname));
	globals.pool = pool;

	if ((status = do_config() != SWITCH_STATUS_SUCCESS)) {
		return status;
	}

	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);
	switch_mutex_init(&globals.limit_hash_mutex, SWITCH_MUTEX_NESTED, globals.pool);
	switch_mutex_init(&globals.db_hash_mutex, SWITCH_MUTEX_NESTED, globals.pool);
	switch_core_hash_init(&globals.limit_hash, pool);
	switch_core_hash_init(&globals.db_hash, pool);

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "limit", "Limit", LIMIT_DESC, limit_function, LIMIT_USAGE, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "limit_hash", "Limit (hash)", LIMITHASH_DESC, limit_hash_function, LIMITHASH_USAGE, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "db", "Insert to the db", DB_DESC, db_function, DB_USAGE, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "hash", "Insert into the hashtable", HASH_DESC, hash_function, HASH_USAGE, SAF_SUPPORT_NOMEDIA)
	SWITCH_ADD_APP(app_interface, "group", "Manage a group", GROUP_DESC, group_function, GROUP_USAGE, SAF_SUPPORT_NOMEDIA);

	SWITCH_ADD_API(commands_api_interface, "limit_hash_usage", "Gets the usage count of a limited resource", limit_hash_usage_function,  LIMIT_HASH_USAGE_USAGE);
	SWITCH_ADD_API(commands_api_interface, "limit_usage", "Gets the usage count of a limited resource", limit_usage_function, "<realm> <id>");
	SWITCH_ADD_API(commands_api_interface, "db", "db get/set", db_api_function, "[insert|delete|select]/<realm>/<key>/<value>");
	switch_console_set_complete("add db insert");
	switch_console_set_complete("add db delete");
	switch_console_set_complete("add db select");
	SWITCH_ADD_API(commands_api_interface, "hash", "hash get/set", hash_api_function, "[insert|delete|select]/<realm>/<key>/<value>");
	switch_console_set_complete("add hash insert");
	switch_console_set_complete("add hash delete");
	switch_console_set_complete("add hash select");
	SWITCH_ADD_API(commands_api_interface, "group", "group [insert|delete|call]", group_api_function, "[insert|delete|call]:<group name>:<url>");
	switch_console_set_complete("add group insert");
	switch_console_set_complete("add group delete");
	switch_console_set_complete("add group call");

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
