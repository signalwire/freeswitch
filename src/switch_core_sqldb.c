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
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 *
 *
 * switch_core_sqldb.c -- Main Core Library (statistics tracker)
 *
 */

#include <switch.h>
#include "private/switch_core_pvt.h"

#define SWITCH_SQL_QUEUE_LEN 100000
#define SWITCH_SQL_QUEUE_PAUSE_LEN 90000

struct switch_cache_db_handle {
	char name[CACHE_DB_LEN];
	switch_cache_db_handle_type_t type;
	switch_cache_db_native_handle_t native_handle;
	time_t last_used;
	switch_mutex_t *mutex;
	switch_mutex_t *io_mutex;
	switch_memory_pool_t *pool;
	int32_t flags;
	unsigned long hash;
	unsigned long thread_hash;
	char creator[CACHE_DB_LEN];
	char last_user[CACHE_DB_LEN];
	uint32_t use_count;
	struct switch_cache_db_handle *next;
};

static struct {
	switch_memory_pool_t *memory_pool;
	switch_thread_t *db_thread;
	int db_thread_running;
	switch_bool_t manage;
	switch_mutex_t *io_mutex;
	switch_mutex_t *dbh_mutex;
	switch_mutex_t *ctl_mutex;
	switch_cache_db_handle_t *handle_pool;
	uint32_t total_handles;
	uint32_t total_used_handles;
	switch_cache_db_handle_t *dbh;
	switch_sql_queue_manager_t *qm;
	int paused;
} sql_manager;


static void switch_core_sqldb_start_thread(void);
static void switch_core_sqldb_stop_thread(void);

static switch_cache_db_handle_t *create_handle(switch_cache_db_handle_type_t type)
{
	switch_cache_db_handle_t *new_dbh = NULL;
	switch_memory_pool_t *pool = NULL;

	switch_core_new_memory_pool(&pool);
	new_dbh = switch_core_alloc(pool, sizeof(*new_dbh));
	new_dbh->pool = pool;
	new_dbh->type = type;
	switch_mutex_init(&new_dbh->mutex, SWITCH_MUTEX_NESTED, new_dbh->pool);

	return new_dbh;
}

static void add_handle(switch_cache_db_handle_t *dbh, const char *db_str, const char *db_callsite_str, const char *thread_str)
{
	switch_ssize_t hlen = -1;

	switch_mutex_lock(sql_manager.dbh_mutex);

	switch_set_string(dbh->creator, db_callsite_str);

	switch_set_string(dbh->name, db_str);
	dbh->hash = switch_ci_hashfunc_default(db_str, &hlen);
	dbh->thread_hash = switch_ci_hashfunc_default(thread_str, &hlen);

	dbh->use_count++;
	sql_manager.total_used_handles++;
	dbh->next = sql_manager.handle_pool;

	sql_manager.handle_pool = dbh;
	sql_manager.total_handles++;
	switch_mutex_lock(dbh->mutex);
	switch_mutex_unlock(sql_manager.dbh_mutex);
}

static void del_handle(switch_cache_db_handle_t *dbh)
{
	switch_cache_db_handle_t *dbh_ptr, *last = NULL;

	switch_mutex_lock(sql_manager.dbh_mutex);
	for (dbh_ptr = sql_manager.handle_pool; dbh_ptr; dbh_ptr = dbh_ptr->next) {
		if (dbh_ptr == dbh) {
			if (last) {
				last->next = dbh_ptr->next;
			} else {
				sql_manager.handle_pool = dbh_ptr->next;
			}
			sql_manager.total_handles--;
			break;
		}
		
		last = dbh_ptr;
	}
	switch_mutex_unlock(sql_manager.dbh_mutex);
}

static switch_cache_db_handle_t *get_handle(const char *db_str, const char *user_str, const char *thread_str)
{
	switch_ssize_t hlen = -1;
	unsigned long hash = 0, thread_hash = 0;
	switch_cache_db_handle_t *dbh_ptr, *r = NULL;

	hash = switch_ci_hashfunc_default(db_str, &hlen);
	thread_hash = switch_ci_hashfunc_default(thread_str, &hlen);
	
	switch_mutex_lock(sql_manager.dbh_mutex);

	for (dbh_ptr = sql_manager.handle_pool; dbh_ptr; dbh_ptr = dbh_ptr->next) {
		if (dbh_ptr->thread_hash == thread_hash && dbh_ptr->hash == hash && !dbh_ptr->use_count &&
			!switch_test_flag(dbh_ptr, CDF_PRUNE) && switch_mutex_trylock(dbh_ptr->mutex) == SWITCH_STATUS_SUCCESS) {
			r = dbh_ptr;
		}
	}
			
	if (!r) {
		for (dbh_ptr = sql_manager.handle_pool; dbh_ptr; dbh_ptr = dbh_ptr->next) {
			if (dbh_ptr->hash == hash && (dbh_ptr->type != SCDB_TYPE_PGSQL || !dbh_ptr->use_count) && !switch_test_flag(dbh_ptr, CDF_PRUNE) && 
				switch_mutex_trylock(dbh_ptr->mutex) == SWITCH_STATUS_SUCCESS) {
				r = dbh_ptr;
				break;
			}
		}	
	}
	
	if (r) {
		r->use_count++;
		sql_manager.total_used_handles++;
		r->hash = switch_ci_hashfunc_default(db_str, &hlen);
		r->thread_hash = thread_hash;
		switch_set_string(r->last_user, user_str);
	}

	switch_mutex_unlock(sql_manager.dbh_mutex);

	return r;
	
}

/*!
  \brief Open the default system database
*/
SWITCH_DECLARE(switch_status_t) _switch_core_db_handle(switch_cache_db_handle_t **dbh, const char *file, const char *func, int line)
{
	switch_status_t r;
	char *dsn;
	
	if (!sql_manager.manage) {
		return SWITCH_STATUS_FALSE;
	}

	if (!zstr(runtime.odbc_dsn)) {
		dsn = runtime.odbc_dsn;
	} else if (!zstr(runtime.dbname)) {
		dsn = runtime.dbname;
	} else {
		dsn = "core";
	}

	if ((r = _switch_cache_db_get_db_handle_dsn(dbh, dsn, file, func, line)) != SWITCH_STATUS_SUCCESS) {
		*dbh = NULL;
	}
	
	return r;
}

#define SQL_CACHE_TIMEOUT 30
#define SQL_REG_TIMEOUT 15


static void sql_close(time_t prune)
{
	switch_cache_db_handle_t *dbh = NULL;
	int locked = 0;
	int sanity = 10000;

	switch_mutex_lock(sql_manager.dbh_mutex);
 top:
	locked = 0;

	for (dbh = sql_manager.handle_pool; dbh; dbh = dbh->next) {
		time_t diff = 0;

		if (prune > 0 && prune > dbh->last_used) {
			diff = (time_t) prune - dbh->last_used;
		}

		if (prune > 0 && (dbh->use_count || (diff < SQL_CACHE_TIMEOUT && !switch_test_flag(dbh, CDF_PRUNE)))) {
			continue;
		}

		if (switch_mutex_trylock(dbh->mutex) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Dropping idle DB connection %s\n", dbh->name);

			switch (dbh->type) {
			case SCDB_TYPE_PGSQL:
				{
					switch_pgsql_handle_destroy(&dbh->native_handle.pgsql_dbh);
				}
				break;
			case SCDB_TYPE_ODBC:
				{
					switch_odbc_handle_destroy(&dbh->native_handle.odbc_dbh);
				}
				break;
			case SCDB_TYPE_CORE_DB:
				{
					switch_core_db_close(dbh->native_handle.core_db_dbh);
					dbh->native_handle.core_db_dbh = NULL;
				}
				break;
			}

			del_handle(dbh);
			switch_mutex_unlock(dbh->mutex);
			switch_core_destroy_memory_pool(&dbh->pool);
			goto top;

		} else {
			if (!prune) {
				if (!sanity) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "SANITY CHECK FAILED!  Handle %s (%s;%s) was not properly released.\n", 
									  dbh->name, dbh->creator, dbh->last_user);
				} else {
					locked++;
				}
			}
			continue;
		}
		
	}

	if (locked) {
		if (!prune) {
			switch_cond_next();
			if (sanity) sanity--;
		}
		goto top;
	}

	switch_mutex_unlock(sql_manager.dbh_mutex);
}


SWITCH_DECLARE(switch_cache_db_handle_type_t) switch_cache_db_get_type(switch_cache_db_handle_t *dbh)
{
	return dbh->type;
}

SWITCH_DECLARE(void) switch_cache_db_flush_handles(void)
{
	sql_close(switch_epoch_time_now(NULL) + SQL_CACHE_TIMEOUT + 1);
}


SWITCH_DECLARE(void) switch_cache_db_release_db_handle(switch_cache_db_handle_t **dbh)
{
	if (dbh && *dbh) {

		switch((*dbh)->type) {
		case SCDB_TYPE_PGSQL:
			{
				switch_pgsql_flush((*dbh)->native_handle.pgsql_dbh);
			}
			break;
		default:
			break;
		}

		switch_mutex_lock(sql_manager.dbh_mutex);
		(*dbh)->last_used = switch_epoch_time_now(NULL);

		(*dbh)->io_mutex = NULL;
		
		if ((*dbh)->use_count) {
			if (--(*dbh)->use_count == 0) {
				(*dbh)->thread_hash = 1;
			}
		}
		switch_mutex_unlock((*dbh)->mutex);
		sql_manager.total_used_handles--;
		*dbh = NULL;
		switch_mutex_unlock(sql_manager.dbh_mutex);
	}
}


SWITCH_DECLARE(void) switch_cache_db_dismiss_db_handle(switch_cache_db_handle_t **dbh)
{
	switch_cache_db_release_db_handle(dbh);
}


SWITCH_DECLARE(switch_status_t) _switch_cache_db_get_db_handle_dsn(switch_cache_db_handle_t **dbh, const char *dsn, 
																   const char *file, const char *func, int line)
{
	switch_cache_db_connection_options_t connection_options = { {0} };
	switch_cache_db_handle_type_t type;
	char tmp[256] = "";
	char *p;
	switch_status_t status = SWITCH_STATUS_FALSE;
	int i;

	if (!strncasecmp(dsn, "pgsql://", 8)) {
		type = SCDB_TYPE_PGSQL;
		connection_options.pgsql_options.dsn = (char *)(dsn + 8);
	} else if (!strncasecmp(dsn, "sqlite://", 9)) {
		type = SCDB_TYPE_CORE_DB;
		connection_options.core_db_options.db_path = (char *)(dsn + 9);
	} else if ((!(i = strncasecmp(dsn, "odbc://", 7))) || strchr(dsn, ':')) {
		type = SCDB_TYPE_ODBC;

		if (i) {
			switch_set_string(tmp, dsn);
		} else {
			switch_set_string(tmp, dsn+7);
		}
		
		connection_options.odbc_options.dsn = tmp;

		if ((p = strchr(tmp, ':'))) {
			*p++ = '\0';
			connection_options.odbc_options.user = p;
			
			if ((p = strchr(connection_options.odbc_options.user, ':'))) {
				*p++ = '\0';
				connection_options.odbc_options.pass = p;
			}
		}
	} else {
		type = SCDB_TYPE_CORE_DB;
		connection_options.core_db_options.db_path = (char *)dsn;
	}

	status = _switch_cache_db_get_db_handle(dbh, type, &connection_options, file, func, line);

	if (status != SWITCH_STATUS_SUCCESS) *dbh = NULL;

	return status;
}


SWITCH_DECLARE(switch_status_t) _switch_cache_db_get_db_handle(switch_cache_db_handle_t **dbh,
															   switch_cache_db_handle_type_t type,
															   switch_cache_db_connection_options_t *connection_options,
															   const char *file, const char *func, int line)
{
	switch_thread_id_t self = switch_thread_self();
	char thread_str[CACHE_DB_LEN] = "";
	char db_str[CACHE_DB_LEN] = "";
	char db_callsite_str[CACHE_DB_LEN] = "";
	switch_cache_db_handle_t *new_dbh = NULL;
	int waiting = 0;
	uint32_t yield_len = 100000, total_yield = 0;

	const char *db_name = NULL;
	const char *odbc_user = NULL;
	const char *odbc_pass = NULL;
	const char *db_type = NULL;

	while(runtime.max_db_handles && sql_manager.total_handles >= runtime.max_db_handles && sql_manager.total_used_handles >= sql_manager.total_handles) {
		if (!waiting++) {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_WARNING, "Max handles %u exceeded, blocking....\n", 
							  runtime.max_db_handles);
		}

		switch_yield(yield_len);
		total_yield += yield_len;
		
		if (runtime.db_handle_timeout && total_yield > runtime.db_handle_timeout) {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "Error connecting\n");
			*dbh = NULL;
			return SWITCH_STATUS_FALSE;
		}
	}

	switch (type) {
	case SCDB_TYPE_PGSQL:
		{
			db_name = connection_options->pgsql_options.dsn;
			odbc_user = NULL;
			odbc_pass = NULL;
			db_type = "pgsql";
		}
	case SCDB_TYPE_ODBC:
		{
			db_name = connection_options->odbc_options.dsn;
			odbc_user = connection_options->odbc_options.user;
			odbc_pass = connection_options->odbc_options.pass;
			db_type = "odbc";
		}
		break;
	case SCDB_TYPE_CORE_DB:
		{
			db_name = connection_options->core_db_options.db_path;
			odbc_user = NULL;
			odbc_pass = NULL;
			db_type = "core_db";
		}
		break;
	}

	if (!db_name) {
		return SWITCH_STATUS_FALSE;
	}

	if (odbc_user || odbc_pass) {
		snprintf(db_str, sizeof(db_str) - 1, "db=\"%s\";type=\"%s\"user=\"%s\";pass=\"%s\"", db_name, db_type, odbc_user, odbc_pass);
	} else {
		snprintf(db_str, sizeof(db_str) - 1, "db=\"%s\",type=\"%s\"", db_name, db_type);
	}
	snprintf(db_callsite_str, sizeof(db_callsite_str) - 1, "%s:%d", file, line);
	snprintf(thread_str, sizeof(thread_str) - 1, "thread=\"%lu\"",  (unsigned long) (intptr_t) self); 

	if ((new_dbh = get_handle(db_str, db_callsite_str, thread_str))) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_DEBUG10,
						  "Reuse Unused Cached DB handle %s [%s]\n", new_dbh->name, switch_cache_db_type_name(new_dbh->type));
	} else {
		switch_core_db_t *db = NULL;
		switch_odbc_handle_t *odbc_dbh = NULL;
		switch_pgsql_handle_t *pgsql_dbh = NULL;

		switch (type) {
		case SCDB_TYPE_PGSQL:
			{
				if (!switch_pgsql_available()) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failure! PGSQL NOT AVAILABLE! Can't connect to DSN %s\n", connection_options->pgsql_options.dsn);
					goto end;
				}

				if ((pgsql_dbh = switch_pgsql_handle_new(connection_options->pgsql_options.dsn))) {
					if (switch_pgsql_handle_connect(pgsql_dbh) != SWITCH_PGSQL_SUCCESS) {
						switch_pgsql_handle_destroy(&pgsql_dbh);
					}
				}
			}
			break;
		case SCDB_TYPE_ODBC:
			{

				if (!switch_odbc_available()) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failure! ODBC NOT AVAILABLE! Can't connect to DSN %s\n", connection_options->odbc_options.dsn);
					goto end;
				}

				if ((odbc_dbh = switch_odbc_handle_new(connection_options->odbc_options.dsn,
													   connection_options->odbc_options.user, connection_options->odbc_options.pass))) {
					if (switch_odbc_handle_connect(odbc_dbh) != SWITCH_ODBC_SUCCESS) {
						switch_odbc_handle_destroy(&odbc_dbh);
					}
				}


			}
			break;
		case SCDB_TYPE_CORE_DB:
			{
				db = switch_core_db_open_file(connection_options->core_db_options.db_path);
			}
			break;

		default:
			goto end;
		}

		if (!db && !odbc_dbh && !pgsql_dbh) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failure to connect to %s %s!\n", switch_cache_db_type_name(type), db_name);
			goto end;
		}

		new_dbh = create_handle(type);

		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_DEBUG10,
						  "Create Cached DB handle %s [%s] %s:%d\n", new_dbh->name, switch_cache_db_type_name(type), file, line);

		new_dbh = create_handle(type);

		if (db) {
			new_dbh->native_handle.core_db_dbh = db;
		} else if (odbc_dbh) {
			new_dbh->native_handle.odbc_dbh = odbc_dbh;
		} else {
			new_dbh->native_handle.pgsql_dbh = pgsql_dbh;
		}

		add_handle(new_dbh, db_str, db_callsite_str, thread_str);
	}

 end:

	if (new_dbh) {
		new_dbh->last_used = switch_epoch_time_now(NULL);
	}
	
	*dbh = new_dbh;

	return *dbh ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}


static switch_status_t switch_cache_db_execute_sql_real(switch_cache_db_handle_t *dbh, const char *sql, char **err)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *errmsg = NULL;
	char *tmp = NULL;
	char *type = NULL;
	switch_mutex_t *io_mutex = dbh->io_mutex;
	
	if (io_mutex) switch_mutex_lock(io_mutex);

	if (err) {
		*err = NULL;
	}

	switch (dbh->type) {
	case SCDB_TYPE_PGSQL:
		{
			type = "PGSQL";
			status = switch_pgsql_handle_exec(dbh->native_handle.pgsql_dbh, sql, &errmsg);
		}
		break;
	case SCDB_TYPE_ODBC:
		{
			type = "ODBC";
			status = switch_odbc_handle_exec(dbh->native_handle.odbc_dbh, sql, NULL, &errmsg);
		}
		break;
	case SCDB_TYPE_CORE_DB:
		{
			int ret = switch_core_db_exec(dbh->native_handle.core_db_dbh, sql, NULL, NULL, &errmsg);
			type = "NATIVE";

			if (ret == SWITCH_CORE_DB_OK) {
				status = SWITCH_STATUS_SUCCESS;
			}

			if (errmsg) {
				switch_strdup(tmp, errmsg);
				switch_core_db_free(errmsg);
				errmsg = tmp;
			}
		}
		break;
	}

	if (errmsg) {
		if (!switch_stristr("already exists", errmsg) && !switch_stristr("duplicate key name", errmsg)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s SQL ERR [%s]\n%s\n", (type ? type : "Unknown"), errmsg, sql);
		}
		if (err) {
			*err = errmsg;
		} else {
			free(errmsg);
		}
	}


	if (io_mutex) switch_mutex_unlock(io_mutex);

	return status;
}

/**
   OMFG you cruel bastards.  Who chooses 64k as a max buffer len for a sql statement, have you ever heard of transactions?
**/
static switch_status_t switch_cache_db_execute_sql_chunked(switch_cache_db_handle_t *dbh, char *sql, uint32_t chunk_size, char **err)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *p, *s, *e;
	int chunk_count;
	switch_size_t len;

	switch_assert(chunk_size);

	if (err)
		*err = NULL;

	len = strlen(sql);

	if (chunk_size > len) {
		return switch_cache_db_execute_sql_real(dbh, sql, err);
	}

	if (!(chunk_count = strlen(sql) / chunk_size)) {
		return SWITCH_STATUS_FALSE;
	}

	e = end_of_p(sql);
	s = sql;

	while (s && s < e) {
		p = s + chunk_size;

		if (p > e) {
			p = e;
		}

		while (p > s) {
			if (*p == '\n' && *(p - 1) == ';') {
				*p = '\0';
				*(p - 1) = '\0';
				p++;
				break;
			}

			p--;
		}

		if (p <= s)
			break;


		status = switch_cache_db_execute_sql_real(dbh, s, err);
		if (status != SWITCH_STATUS_SUCCESS || (err && *err)) {
			break;
		}

		s = p;

	}

	return status;

}


SWITCH_DECLARE(switch_status_t) switch_cache_db_execute_sql(switch_cache_db_handle_t *dbh, char *sql, char **err)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_mutex_t *io_mutex = dbh->io_mutex;

	if (io_mutex) switch_mutex_lock(io_mutex);

	switch (dbh->type) {
	default:
		{
			status = switch_cache_db_execute_sql_chunked(dbh, (char *) sql, 32768, err);
		}
		break;
	}

	if (io_mutex) switch_mutex_unlock(io_mutex);

	return status;

}


SWITCH_DECLARE(int) switch_cache_db_affected_rows(switch_cache_db_handle_t *dbh)
{
	switch (dbh->type) {
	case SCDB_TYPE_CORE_DB:
		{
			return switch_core_db_changes(dbh->native_handle.core_db_dbh);
		}
		break;
	case SCDB_TYPE_ODBC:
		{
			return switch_odbc_handle_affected_rows(dbh->native_handle.odbc_dbh);
		}
		break;
	case SCDB_TYPE_PGSQL:
		{
			return switch_pgsql_handle_affected_rows(dbh->native_handle.pgsql_dbh);
		}
		break;
	}
	return 0;
}

SWITCH_DECLARE(int) switch_cache_db_load_extension(switch_cache_db_handle_t *dbh, const char *extension)
{
	switch (dbh->type) {
	case SCDB_TYPE_CORE_DB:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "try to load extension [%s]!\n", extension);
			return switch_core_db_load_extension(dbh->native_handle.core_db_dbh, extension);
		}
		break;
	case SCDB_TYPE_ODBC:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "load extension not supported by type ODBC!\n");
		}
		break;
	case SCDB_TYPE_PGSQL:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "load extension not supported by type PGSQL!\n");
		}
		break;
	}
	return 0;
}


SWITCH_DECLARE(char *) switch_cache_db_execute_sql2str(switch_cache_db_handle_t *dbh, char *sql, char *str, size_t len, char **err)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_mutex_t *io_mutex = dbh->io_mutex;

	if (io_mutex) switch_mutex_lock(io_mutex);

	memset(str, 0, len);

	switch (dbh->type) {
	case SCDB_TYPE_CORE_DB:
		{
			switch_core_db_stmt_t *stmt;

			if (switch_core_db_prepare(dbh->native_handle.core_db_dbh, sql, -1, &stmt, 0)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Statement Error [%s]!\n", sql);
				goto end;
			} else {
				int running = 1;
				int colcount;

				while (running < 5000) {
					int result = switch_core_db_step(stmt);
					const unsigned char *txt;

					if (result == SWITCH_CORE_DB_ROW) {
						if ((colcount = switch_core_db_column_count(stmt)) > 0) {
							if ((txt = switch_core_db_column_text(stmt, 0))) {
								switch_copy_string(str, (char *) txt, len);
								status = SWITCH_STATUS_SUCCESS;
							} else {
								goto end;
							}
						}
						break;
					} else if (result == SWITCH_CORE_DB_BUSY) {
						running++;
						switch_cond_next();
						continue;
					}
					break;
				}

				switch_core_db_finalize(stmt);
			}
		}
		break;
	case SCDB_TYPE_ODBC:
		{
			status = switch_odbc_handle_exec_string(dbh->native_handle.odbc_dbh, sql, str, len, err);
		}
		break;
	case SCDB_TYPE_PGSQL:
		{
			status = switch_pgsql_handle_exec_string(dbh->native_handle.pgsql_dbh, sql, str, len, err);
		}
		break;
	}

 end:

	if (io_mutex) switch_mutex_unlock(io_mutex);

	return status == SWITCH_STATUS_SUCCESS ? str : NULL;

}

SWITCH_DECLARE(switch_status_t) switch_cache_db_persistant_execute(switch_cache_db_handle_t *dbh, const char *sql, uint32_t retries)
{
	char *errmsg = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	uint8_t forever = 0;
	switch_mutex_t *io_mutex = dbh->io_mutex;

	if (!retries) {
		forever = 1;
		retries = 1000;
	}

	while (retries > 0) {

		if (io_mutex) switch_mutex_lock(io_mutex);
		switch_cache_db_execute_sql_real(dbh, sql, &errmsg);
		if (io_mutex) switch_mutex_unlock(io_mutex);

		
		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR [%s]\n", errmsg);
			switch_safe_free(errmsg);
			switch_yield(100000);
			retries--;
			if (retries == 0 && forever) {
				retries = 1000;
				continue;
			}
		} else {
			status = SWITCH_STATUS_SUCCESS;
			break;
		}
	}
	
	return status;
}


SWITCH_DECLARE(switch_status_t) switch_cache_db_persistant_execute_trans_full(switch_cache_db_handle_t *dbh, 
																			  char *sql, uint32_t retries,
																			  const char *pre_trans_execute,
																			  const char *post_trans_execute,
																			  const char *inner_pre_trans_execute,
																			  const char *inner_post_trans_execute)
{
	char *errmsg = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	uint8_t forever = 0;
	unsigned begin_retries = 100;
	uint8_t again = 0;
	switch_mutex_t *io_mutex = dbh->io_mutex;

	if (!retries) {
		forever = 1;
		retries = 1000;
	}

	if (io_mutex) switch_mutex_lock(io_mutex);

	if (!zstr(pre_trans_execute)) {
		switch_cache_db_execute_sql_real(dbh, pre_trans_execute, &errmsg);
		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "SQL PRE TRANS EXEC %s [%s]\n", pre_trans_execute, errmsg);
			free(errmsg);
		}
	}

 again:

	while (begin_retries > 0) {
		again = 0;

		switch(dbh->type) {
		case SCDB_TYPE_CORE_DB:
			{
				switch_cache_db_execute_sql_real(dbh, "BEGIN EXCLUSIVE", &errmsg);
			}
			break;
		case SCDB_TYPE_ODBC:
			{
				switch_odbc_status_t result;
				
				if ((result = switch_odbc_SQLSetAutoCommitAttr(dbh->native_handle.odbc_dbh, 0)) != SWITCH_ODBC_SUCCESS) {
					char tmp[100];
					switch_snprintfv(tmp, sizeof(tmp), "%q-%i", "Unable to Set AutoCommit Off", result);
					errmsg = strdup(tmp);
				}
			}
			break;
		case SCDB_TYPE_PGSQL:
			{
				switch_pgsql_status_t result;
				
				if ((result = switch_pgsql_SQLSetAutoCommitAttr(dbh->native_handle.pgsql_dbh, 0)) != SWITCH_PGSQL_SUCCESS) {
					char tmp[100];
					switch_snprintfv(tmp, sizeof(tmp), "%q-%i", "Unable to Set AutoCommit Off", result);
					errmsg = strdup(tmp);
				}
			}
			break;
		}

		if (errmsg) {
			begin_retries--;
			if (strstr(errmsg, "cannot start a transaction within a transaction")) {
				again = 1;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SQL Retry [%s]\n", errmsg);
			}
			free(errmsg);
			errmsg = NULL;

			if (again) {
				switch(dbh->type) {
				case SCDB_TYPE_CORE_DB:
					{
						switch_cache_db_execute_sql_real(dbh, "COMMIT", NULL);
					}
					break;
				case SCDB_TYPE_ODBC:
					{
						switch_odbc_SQLEndTran(dbh->native_handle.odbc_dbh, 1);
						switch_odbc_SQLSetAutoCommitAttr(dbh->native_handle.odbc_dbh, 1);
					}
					break;
				case SCDB_TYPE_PGSQL:
					{
						switch_pgsql_SQLEndTran(dbh->native_handle.pgsql_dbh, 1);
						switch_pgsql_SQLSetAutoCommitAttr(dbh->native_handle.pgsql_dbh, 1);
						switch_pgsql_finish_results(dbh->native_handle.pgsql_dbh);
					}
					break;
				}

				goto again;
			}
			
			switch_yield(100000);

			if (begin_retries == 0) {
				goto done;
			}

			continue;
		}

		break;
	}


	if (!zstr(inner_pre_trans_execute)) {
		switch_cache_db_execute_sql_real(dbh, inner_pre_trans_execute, &errmsg);
		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "SQL PRE TRANS EXEC %s [%s]\n", inner_pre_trans_execute, errmsg);
			free(errmsg);
		}
	}

	while (retries > 0) {

		switch_cache_db_execute_sql(dbh, sql, &errmsg);

		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR [%s]\n", errmsg);
			free(errmsg);
			errmsg = NULL;
			switch_yield(100000);
			retries--;
			if (retries == 0 && forever) {
				retries = 1000;
				continue;
			}
		} else {
			status = SWITCH_STATUS_SUCCESS;
			break;
		}
	}

	if (!zstr(inner_post_trans_execute)) {
		switch_cache_db_execute_sql_real(dbh, inner_post_trans_execute, &errmsg);
		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "SQL POST TRANS EXEC %s [%s]\n", inner_post_trans_execute, errmsg);
			free(errmsg);
		}
	}

 done:

	switch(dbh->type) {
	case SCDB_TYPE_CORE_DB:
		{
			switch_cache_db_execute_sql_real(dbh, "COMMIT", NULL);
		}
		break;
	case SCDB_TYPE_ODBC:
		{
			switch_odbc_SQLEndTran(dbh->native_handle.odbc_dbh, 1);
			switch_odbc_SQLSetAutoCommitAttr(dbh->native_handle.odbc_dbh, 1);
		}
		break;
	case SCDB_TYPE_PGSQL:
		{
			switch_pgsql_SQLEndTran(dbh->native_handle.pgsql_dbh, 1);
			switch_pgsql_SQLSetAutoCommitAttr(dbh->native_handle.pgsql_dbh, 1);
			switch_pgsql_finish_results(dbh->native_handle.pgsql_dbh);
		}
		break;
	}

	if (!zstr(post_trans_execute)) {
		switch_cache_db_execute_sql_real(dbh, post_trans_execute, &errmsg);
		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "SQL POST TRANS EXEC %s [%s]\n", post_trans_execute, errmsg);
			free(errmsg);
		}
	}

	if (io_mutex) switch_mutex_unlock(io_mutex);

	return status;
}

struct helper {
	switch_core_db_event_callback_func_t callback;
	void *pdata;
};

static int helper_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct helper *h = (struct helper *) pArg;
	int r = 0;
	switch_event_t *event;

	switch_event_create_array_pair(&event, columnNames, argv, argc);

	r = h->callback(h->pdata, event);

	switch_event_destroy(&event);
	
	return r;
}

SWITCH_DECLARE(switch_status_t) switch_cache_db_execute_sql_event_callback(switch_cache_db_handle_t *dbh,
																	 const char *sql, switch_core_db_event_callback_func_t callback, void *pdata, char **err)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *errmsg = NULL;
	switch_mutex_t *io_mutex = dbh->io_mutex;
	struct helper h;


	if (err) {
		*err = NULL;
	}

	if (io_mutex) switch_mutex_lock(io_mutex);

	h.callback = callback;
	h.pdata = pdata;
	
	switch (dbh->type) {
	case SCDB_TYPE_PGSQL:
		{
			status = switch_pgsql_handle_callback_exec(dbh->native_handle.pgsql_dbh, sql, helper_callback, &h, err);
		}
		break;
	case SCDB_TYPE_ODBC:
		{
			status = switch_odbc_handle_callback_exec(dbh->native_handle.odbc_dbh, sql, helper_callback, &h, err);
		}
		break;
	case SCDB_TYPE_CORE_DB:
		{
			int ret = switch_core_db_exec(dbh->native_handle.core_db_dbh, sql, helper_callback, &h, &errmsg);

			if (ret == SWITCH_CORE_DB_OK || ret == SWITCH_CORE_DB_ABORT) {
				status = SWITCH_STATUS_SUCCESS;
			}

			if (errmsg) {
				dbh->last_used = switch_epoch_time_now(NULL) - (SQL_CACHE_TIMEOUT * 2);
				if (!strstr(errmsg, "query abort")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s] %s\n", sql, errmsg);
				}
				switch_core_db_free(errmsg);
			}
		}
		break;
	}

	if (io_mutex) switch_mutex_unlock(io_mutex);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_cache_db_execute_sql_callback(switch_cache_db_handle_t *dbh,
																	 const char *sql, switch_core_db_callback_func_t callback, void *pdata, char **err)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *errmsg = NULL;
	switch_mutex_t *io_mutex = dbh->io_mutex;

	if (err) {
		*err = NULL;
	}

	if (io_mutex) switch_mutex_lock(io_mutex);


	switch (dbh->type) {
	case SCDB_TYPE_PGSQL:
		{
			status = switch_pgsql_handle_callback_exec(dbh->native_handle.pgsql_dbh, sql, callback, pdata, err);
		}
		break;
	case SCDB_TYPE_ODBC:
		{
			status = switch_odbc_handle_callback_exec(dbh->native_handle.odbc_dbh, sql, callback, pdata, err);
		}
		break;
	case SCDB_TYPE_CORE_DB:
		{
			int ret = switch_core_db_exec(dbh->native_handle.core_db_dbh, sql, callback, pdata, &errmsg);

			if (ret == SWITCH_CORE_DB_OK || ret == SWITCH_CORE_DB_ABORT) {
				status = SWITCH_STATUS_SUCCESS;
			}

			if (errmsg) {
				dbh->last_used = switch_epoch_time_now(NULL) - (SQL_CACHE_TIMEOUT * 2);
				if (!strstr(errmsg, "query abort")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s] %s\n", sql, errmsg);
				}
				switch_core_db_free(errmsg);
			}
		}
		break;
	}

	if (io_mutex) switch_mutex_unlock(io_mutex);

	return status;
}

SWITCH_DECLARE(switch_bool_t) switch_cache_db_test_reactive(switch_cache_db_handle_t *dbh,
															const char *test_sql, const char *drop_sql, const char *reactive_sql)
{
	char *errmsg;
	switch_bool_t r = SWITCH_TRUE;
	switch_mutex_t *io_mutex = dbh->io_mutex;

	if (!switch_test_flag((&runtime), SCF_CLEAR_SQL)) {
		return SWITCH_TRUE;
	}

	if (!switch_test_flag((&runtime), SCF_AUTO_SCHEMAS)) {
		switch_cache_db_execute_sql(dbh, (char *)test_sql, NULL);
		return SWITCH_TRUE;
	}

	if (io_mutex) switch_mutex_lock(io_mutex);

	switch (dbh->type) {
	case SCDB_TYPE_PGSQL:
		{
			if (switch_pgsql_handle_exec(dbh->native_handle.pgsql_dbh, test_sql, NULL) != SWITCH_PGSQL_SUCCESS) {
				r = SWITCH_FALSE;
				if (drop_sql) {
					switch_pgsql_handle_exec(dbh->native_handle.pgsql_dbh, drop_sql, NULL);
				}
				switch_pgsql_handle_exec(dbh->native_handle.pgsql_dbh, reactive_sql, NULL);
			}
		}
		break;
	case SCDB_TYPE_ODBC:
		{
			if (switch_odbc_handle_exec(dbh->native_handle.odbc_dbh, test_sql, NULL, NULL) != SWITCH_ODBC_SUCCESS) {
				r = SWITCH_FALSE;
				if (drop_sql) {
					switch_odbc_handle_exec(dbh->native_handle.odbc_dbh, drop_sql, NULL, NULL);
				}
				switch_odbc_handle_exec(dbh->native_handle.odbc_dbh, reactive_sql, NULL, NULL);
			}
		}
		break;
	case SCDB_TYPE_CORE_DB:
		{
			if (test_sql) {
				switch_core_db_exec(dbh->native_handle.core_db_dbh, test_sql, NULL, NULL, &errmsg);

				if (errmsg) {
					r = SWITCH_FALSE;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SQL ERR [%s]\n[%s]\nAuto Generating Table!\n", errmsg, test_sql);
					switch_core_db_free(errmsg);
					errmsg = NULL;
					if (drop_sql) {
						switch_core_db_exec(dbh->native_handle.core_db_dbh, drop_sql, NULL, NULL, &errmsg);
					}
					if (errmsg) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SQL ERR [%s]\n[%s]\n", errmsg, drop_sql);
						switch_core_db_free(errmsg);
						errmsg = NULL;
					}
					switch_core_db_exec(dbh->native_handle.core_db_dbh, reactive_sql, NULL, NULL, &errmsg);
					if (errmsg) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SQL ERR [%s]\n[%s]\n", errmsg, reactive_sql);
						switch_core_db_free(errmsg);
						errmsg = NULL;
					}
				}
			}
		}
		break;
	}


	if (io_mutex) switch_mutex_unlock(io_mutex);

	return r;
}


static void *SWITCH_THREAD_FUNC switch_core_sql_db_thread(switch_thread_t *thread, void *obj)
{
	int sec = 0, reg_sec = 0;;

	sql_manager.db_thread_running = 1;

	while (sql_manager.db_thread_running == 1) {
		if (++sec == SQL_CACHE_TIMEOUT) {
			sql_close(switch_epoch_time_now(NULL));		
			sec = 0;
		}

		if (switch_test_flag((&runtime), SCF_USE_SQL) && ++reg_sec == SQL_REG_TIMEOUT) {
			switch_core_expire_registration(0);
			reg_sec = 0;
		}
		switch_yield(1000000);
	}


	return NULL;
}


static void *SWITCH_THREAD_FUNC switch_user_sql_thread(switch_thread_t *thread, void *obj);

struct switch_sql_queue_manager {
	const char *name;
	switch_cache_db_handle_t *event_db;
	switch_queue_t **sql_queue;
	uint32_t *pre_written;
	uint32_t *written;
	uint32_t numq;
	char *dsn;
	switch_thread_t *thread;
	int thread_running;
	switch_thread_cond_t *cond;
	switch_mutex_t *cond_mutex;
	switch_mutex_t *cond2_mutex;
	switch_mutex_t *mutex;
	char *pre_trans_execute;
	char *post_trans_execute;
	char *inner_pre_trans_execute;
	char *inner_post_trans_execute;
	switch_memory_pool_t *pool;
	uint32_t max_trans;
	uint32_t confirm;
};

static int qm_wake(switch_sql_queue_manager_t *qm)
{
	switch_status_t status;
	int tries = 0;

 top:
	
	status = switch_mutex_trylock(qm->cond_mutex);

	if (status == SWITCH_STATUS_SUCCESS) {
		switch_thread_cond_signal(qm->cond);
		switch_mutex_unlock(qm->cond_mutex);
		return 1;
	} else {
		if (switch_mutex_trylock(qm->cond2_mutex) == SWITCH_STATUS_SUCCESS) {
			switch_mutex_unlock(qm->cond2_mutex);
		} else {
			if (++tries < 10) {
				switch_cond_next();
				goto top;
			}
		}
	}

	return 0;
}

static uint32_t qm_ttl(switch_sql_queue_manager_t *qm)
{
	uint32_t ttl = 0;
	uint32_t i;

	for (i = 0; i < qm->numq; i++) {
		ttl += switch_queue_size(qm->sql_queue[i]);
	}

	return ttl;
}

struct db_job {
	switch_sql_queue_manager_t *qm;
	char *sql;
	switch_core_db_callback_func_t callback;
	switch_core_db_event_callback_func_t event_callback;
	void *pdata;
	int event;
	switch_memory_pool_t *pool;
};

static void *SWITCH_THREAD_FUNC sql_in_thread (switch_thread_t *thread, void *obj)
{
	struct db_job *job = (struct db_job *) obj;
	switch_memory_pool_t *pool = job->pool;
	char *err = NULL;
	switch_cache_db_handle_t *dbh;


	if (switch_cache_db_get_db_handle_dsn(&dbh, job->qm->dsn) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot connect DSN %s\n", job->qm->dsn);
		return NULL;
	}

	if (job->callback) {
		switch_cache_db_execute_sql_callback(dbh, job->sql, job->callback, job->pdata, &err);
	} else if (job->event_callback) {
		switch_cache_db_execute_sql_event_callback(dbh, job->sql, job->event_callback, job->pdata, &err);
	}

	if (err) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s] %s\n", job->sql, err);
		free(err);
	}
	
	switch_cache_db_release_db_handle(&dbh);
	
	if (pool) {
		switch_core_destroy_memory_pool(&pool);
	}

	return NULL;
}

static switch_thread_data_t *new_job(switch_sql_queue_manager_t *qm, const char *sql, 
									 switch_core_db_callback_func_t callback, switch_core_db_event_callback_func_t event_callback, void *pdata)
{
	switch_memory_pool_t *pool;
	switch_thread_data_t *td;
	struct db_job *job;
	switch_core_new_memory_pool(&pool);

	td = switch_core_alloc(pool, sizeof(*td));
	job = switch_core_alloc(pool, sizeof(*job));

	td->func = sql_in_thread;
	td->obj = job;

	job->sql = switch_core_strdup(pool, sql);
	job->qm = qm;

	if (callback) {
		job->callback = callback;
	} else if (event_callback) {
		job->event_callback = event_callback;
	}

	job->pdata = pdata;
	job->pool = pool;

	return td;
}


SWITCH_DECLARE(void) switch_sql_queue_manger_execute_sql_callback(switch_sql_queue_manager_t *qm, 
																   const char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	
	switch_thread_data_t *td;
	if ((td = new_job(qm, sql, callback, NULL, pdata))) {
		switch_thread_pool_launch_thread(&td);
	}
}

SWITCH_DECLARE(void) switch_sql_queue_manger_execute_sql_event_callback(switch_sql_queue_manager_t *qm, 
																		const char *sql, switch_core_db_event_callback_func_t callback, void *pdata)
{
	
	switch_thread_data_t *td;
	if ((td = new_job(qm, sql, NULL, callback, pdata))) {
		switch_thread_pool_launch_thread(&td);
	}
}

SWITCH_DECLARE(int) switch_sql_queue_manager_size(switch_sql_queue_manager_t *qm, uint32_t index)
{
	int size = 0;

	switch_mutex_lock(qm->mutex);
	if (index < qm->numq) {
		size = switch_queue_size(qm->sql_queue[index]);
	}
	switch_mutex_unlock(qm->mutex);

	return size;
}

SWITCH_DECLARE(switch_status_t) switch_sql_queue_manager_stop(switch_sql_queue_manager_t *qm)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	uint32_t i, sanity = 100;

	if (qm->thread_running == 1) {
		qm->thread_running = -1;

		while(--sanity && qm->thread_running == -1) {
			for(i = 0; i < qm->numq; i++) {
				switch_queue_push(qm->sql_queue[i], NULL);
				switch_queue_interrupt_all(qm->sql_queue[i]);
			}
			qm_wake(qm);

			if (qm->thread_running == -1) {
				switch_yield(100000);
			}
		}
		status = SWITCH_STATUS_SUCCESS;
	}

	if (qm->thread) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s Stopping SQL thread.\n", qm->name);
		qm_wake(qm);
		switch_thread_join(&status, qm->thread);
		qm->thread = NULL;
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_sql_queue_manager_start(switch_sql_queue_manager_t *qm)
{
	switch_threadattr_t *thd_attr;

	if (!qm->thread_running) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s Starting SQL thread.\n", qm->name);
		switch_threadattr_create(&thd_attr, qm->pool);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_threadattr_priority_set(thd_attr, SWITCH_PRI_NORMAL);
		switch_thread_create(&qm->thread, thd_attr, switch_user_sql_thread, qm, qm->pool);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}


static void do_flush(switch_sql_queue_manager_t *qm, int i, switch_cache_db_handle_t *dbh)
{
	void *pop = NULL;
	switch_queue_t *q = qm->sql_queue[i];

	switch_mutex_lock(qm->mutex);
	while (switch_queue_trypop(q, &pop) == SWITCH_STATUS_SUCCESS) {
		if (pop) {
			if (dbh) {
				switch_cache_db_execute_sql(dbh, (char *) pop, NULL);
			}
			free(pop);
		}
	}
	switch_mutex_unlock(qm->mutex);

}

SWITCH_DECLARE(switch_status_t) switch_sql_queue_manager_destroy(switch_sql_queue_manager_t **qmp)
{
	switch_sql_queue_manager_t *qm;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_memory_pool_t *pool;
	uint32_t i;

	switch_assert(qmp);
	qm = *qmp;
	*qmp = NULL;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s Destroying SQL queue.\n", qm->name);

	switch_sql_queue_manager_stop(qm);



	for(i = 0; i < qm->numq; i++) {
		do_flush(qm, i, NULL);
	}

	pool = qm->pool;
	switch_core_destroy_memory_pool(&pool);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_sql_queue_manager_push(switch_sql_queue_manager_t *qm, const char *sql, uint32_t pos, switch_bool_t dup)
{

	if (sql_manager.paused || qm->thread_running != 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "DROP [%s]\n", sql);
		if (!dup) free((char *)sql);
		qm_wake(qm);
		return SWITCH_STATUS_SUCCESS;
	}

	if (qm->thread_running != 1) {
		if (!dup) free((char *)sql);
		return SWITCH_STATUS_FALSE;
	}

	if (pos > qm->numq - 1) {
		pos = 0;
	}

	switch_mutex_lock(qm->mutex);
	switch_queue_push(qm->sql_queue[pos], dup ? strdup(sql) : (char *)sql);
	switch_mutex_unlock(qm->mutex);

	qm_wake(qm);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_sql_queue_manager_push_confirm(switch_sql_queue_manager_t *qm, const char *sql, uint32_t pos, switch_bool_t dup)
{
#define EXEC_NOW
#ifdef EXEC_NOW
	switch_cache_db_handle_t *dbh;

	if (sql_manager.paused || qm->thread_running != 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "DROP [%s]\n", sql);
		if (!dup) free((char *)sql);
		qm_wake(qm);
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_cache_db_get_db_handle_dsn(&dbh, qm->dsn) == SWITCH_STATUS_SUCCESS) {
		switch_cache_db_execute_sql(dbh, (char *)sql, NULL);		
		switch_cache_db_release_db_handle(&dbh);
	}

	if (!dup) free((char *)sql);

#else

	int size, x = 0, sanity = 0;
	uint32_t written, want;

	if (sql_manager.paused) {
		if (!dup) free((char *)sql);
		qm_wake(qm);
		return SWITCH_STATUS_SUCCESS;
	}

	if (qm->thread_running != 1) {
		if (!dup) free((char *)sql);
		return SWITCH_STATUS_FALSE;
	}

	if (pos > qm->numq - 1) {
		pos = 0;
	}

	switch_mutex_lock(qm->mutex);
	qm->confirm++;
	switch_queue_push(qm->sql_queue[pos], dup ? strdup(sql) : (char *)sql);
	written = qm->pre_written[pos];
	size = switch_sql_queue_manager_size(qm, pos);
	want = written + size;
	switch_mutex_unlock(qm->mutex);

	qm_wake(qm);

	while((qm->written[pos] < want) || (qm->written[pos] >= written && want < written && qm->written[pos] > want)) {
		switch_yield(5000);

		if (++x == 200) {
			qm_wake(qm);
			x = 0;
			if (++sanity == 20) {
				break;
			}
		}
	}

	switch_mutex_lock(qm->mutex);
	qm->confirm--;
	switch_mutex_unlock(qm->mutex);
#endif

	return SWITCH_STATUS_SUCCESS;
}





SWITCH_DECLARE(switch_status_t) switch_sql_queue_manager_init_name(const char *name,
																   switch_sql_queue_manager_t **qmp, 
																   uint32_t numq, const char *dsn, uint32_t max_trans,
																   const char *pre_trans_execute,
																   const char *post_trans_execute,
																   const char *inner_pre_trans_execute,
																   const char *inner_post_trans_execute)
{
	switch_memory_pool_t *pool;
	switch_sql_queue_manager_t *qm;
	uint32_t i;

	if (!numq) numq = 1;

	switch_core_new_memory_pool(&pool);
	qm = switch_core_alloc(pool, sizeof(*qm));

	qm->pool = pool;
	qm->numq = numq;
	qm->dsn = switch_core_strdup(qm->pool, dsn);
	qm->name = switch_core_strdup(qm->pool, name);
	qm->max_trans = max_trans;

	switch_mutex_init(&qm->cond_mutex, SWITCH_MUTEX_NESTED, qm->pool);
	switch_mutex_init(&qm->cond2_mutex, SWITCH_MUTEX_NESTED, qm->pool);
	switch_mutex_init(&qm->mutex, SWITCH_MUTEX_NESTED, qm->pool);
	switch_thread_cond_create(&qm->cond, qm->pool);
	
	qm->sql_queue = switch_core_alloc(qm->pool, sizeof(switch_queue_t *) * numq);
	qm->written = switch_core_alloc(qm->pool, sizeof(uint32_t) * numq);
	qm->pre_written = switch_core_alloc(qm->pool, sizeof(uint32_t) * numq);

	for (i = 0; i < qm->numq; i++) {
		switch_queue_create(&qm->sql_queue[i], SWITCH_SQL_QUEUE_LEN, qm->pool);
	}

	if (pre_trans_execute) {
		qm->pre_trans_execute = switch_core_strdup(qm->pool, pre_trans_execute);
	}
	if (post_trans_execute) {
		qm->post_trans_execute = switch_core_strdup(qm->pool, post_trans_execute);
	}
	if (inner_pre_trans_execute) {
		qm->inner_pre_trans_execute = switch_core_strdup(qm->pool, inner_pre_trans_execute);
	}
	if (inner_post_trans_execute) {
		qm->inner_post_trans_execute = switch_core_strdup(qm->pool, inner_post_trans_execute);
	}

	*qmp = qm;

	return SWITCH_STATUS_SUCCESS;

}

static uint32_t do_trans(switch_sql_queue_manager_t *qm)
{
	char *errmsg = NULL;
	void *pop;
	switch_status_t status;
	uint32_t ttl = 0;
	switch_mutex_t *io_mutex = qm->event_db->io_mutex;
	uint32_t i;

	if (io_mutex) switch_mutex_lock(io_mutex);

	if (!zstr(qm->pre_trans_execute)) {
		switch_cache_db_execute_sql_real(qm->event_db, qm->pre_trans_execute, &errmsg);
		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "SQL PRE TRANS EXEC %s [%s]\n", qm->pre_trans_execute, errmsg);
			free(errmsg); errmsg = NULL;
		}
	}

	switch(qm->event_db->type) {
	case SCDB_TYPE_CORE_DB:
		{
			switch_cache_db_execute_sql_real(qm->event_db, "BEGIN EXCLUSIVE", &errmsg);
		}
		break;
	case SCDB_TYPE_ODBC:
		{
			switch_odbc_status_t result;
			
			if ((result = switch_odbc_SQLSetAutoCommitAttr(qm->event_db->native_handle.odbc_dbh, 0)) != SWITCH_ODBC_SUCCESS) {
				char tmp[100];
				switch_snprintfv(tmp, sizeof(tmp), "%q-%i", "Unable to Set AutoCommit Off", result);
				errmsg = strdup(tmp);
			}
		}
		break;
	case SCDB_TYPE_PGSQL:
		{
			switch_pgsql_status_t result;
			
			if ((result = switch_pgsql_SQLSetAutoCommitAttr(qm->event_db->native_handle.pgsql_dbh, 0)) != SWITCH_PGSQL_SUCCESS) {
				char tmp[100];
				switch_snprintfv(tmp, sizeof(tmp), "%q-%i", "Unable to Set AutoCommit Off", result);
				errmsg = strdup(tmp);
			}
		}
		break;
	}

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "ERROR [%s]\n", errmsg);
		free(errmsg);
		goto end;
	}


	if (!zstr(qm->inner_pre_trans_execute)) {
		switch_cache_db_execute_sql_real(qm->event_db, qm->inner_pre_trans_execute, &errmsg);
		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "SQL PRE TRANS EXEC %s [%s]\n", qm->inner_pre_trans_execute, errmsg);
			free(errmsg);
		}
	}


	while(qm->max_trans == 0 || ttl <= qm->max_trans) {
		pop = NULL;

		for (i = 0; (qm->max_trans == 0 || ttl <= qm->max_trans) && (i < qm->numq); i++) {
			switch_mutex_lock(qm->mutex);
			switch_queue_trypop(qm->sql_queue[i], &pop);
			switch_mutex_unlock(qm->mutex);
			if (pop) break;
		}

		if (pop) {
			if ((status = switch_cache_db_execute_sql(qm->event_db, (char *) pop, NULL)) == SWITCH_STATUS_SUCCESS) {
				switch_mutex_lock(qm->mutex);
				qm->pre_written[i]++;
				switch_mutex_unlock(qm->mutex);
				ttl++;
			}
			free(pop);
			pop = NULL;
			if (status != SWITCH_STATUS_SUCCESS) break;
		} else {
			break;
		}
	}

	if (!zstr(qm->inner_post_trans_execute)) {
		switch_cache_db_execute_sql_real(qm->event_db, qm->inner_post_trans_execute, &errmsg);
		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "SQL POST TRANS EXEC %s [%s]\n", qm->inner_post_trans_execute, errmsg);
			free(errmsg);
		}
	}


 end:

	switch(qm->event_db->type) {
	case SCDB_TYPE_CORE_DB:
		{
			switch_cache_db_execute_sql_real(qm->event_db, "COMMIT", NULL);
		}
		break;
	case SCDB_TYPE_ODBC:
		{
			switch_odbc_SQLEndTran(qm->event_db->native_handle.odbc_dbh, 1);
			switch_odbc_SQLSetAutoCommitAttr(qm->event_db->native_handle.odbc_dbh, 1);
		}
		break;
	case SCDB_TYPE_PGSQL:
		{
			switch_pgsql_SQLEndTran(qm->event_db->native_handle.pgsql_dbh, 1);
			switch_pgsql_SQLSetAutoCommitAttr(qm->event_db->native_handle.pgsql_dbh, 1);
			switch_pgsql_finish_results(qm->event_db->native_handle.pgsql_dbh);
		}
		break;
	}


	if (!zstr(qm->post_trans_execute)) {
		switch_cache_db_execute_sql_real(qm->event_db, qm->post_trans_execute, &errmsg);
		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "SQL POST TRANS EXEC %s [%s]\n", qm->post_trans_execute, errmsg);
			free(errmsg);
		}
	}


	switch_mutex_lock(qm->mutex);
	for (i = 0; i < qm->numq; i++) {
		qm->written[i] = qm->pre_written[i];
	}
	switch_mutex_unlock(qm->mutex);


	if (io_mutex) switch_mutex_unlock(io_mutex);

	return ttl;
}

static void *SWITCH_THREAD_FUNC switch_user_sql_thread(switch_thread_t *thread, void *obj)
{

	uint32_t sanity = 120;
	switch_sql_queue_manager_t *qm = (switch_sql_queue_manager_t *) obj;
	uint32_t i;

	while (!qm->event_db) {
		if (switch_cache_db_get_db_handle_dsn(&qm->event_db, qm->dsn) == SWITCH_STATUS_SUCCESS && qm->event_db)
			break;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "%s Error getting db handle, Retrying\n", qm->name);
		switch_yield(500000);
		sanity--;
	}

	if (!qm->event_db) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s Error getting db handle\n", qm->name);
		return NULL;
	}

	qm->thread_running = 1;

	switch_mutex_lock(qm->cond_mutex);

	switch (qm->event_db->type) {
	case SCDB_TYPE_PGSQL:
		break;
	case SCDB_TYPE_ODBC:
		break;
	case SCDB_TYPE_CORE_DB:
		{
			switch_cache_db_execute_sql(qm->event_db, "PRAGMA synchronous=OFF;", NULL);
			switch_cache_db_execute_sql(qm->event_db, "PRAGMA count_changes=OFF;", NULL);
			switch_cache_db_execute_sql(qm->event_db, "PRAGMA temp_store=MEMORY;", NULL);
			switch_cache_db_execute_sql(qm->event_db, "PRAGMA journal_mode=OFF;", NULL);
		}
		break;
	}


	while (qm->thread_running == 1) {
		uint32_t i, lc;
		uint32_t written = 0, iterations = 0;

		if (sql_manager.paused) {
			for (i = 0; i < qm->numq; i++) {
				do_flush(qm, i, NULL);
			}
			goto check;
		}

		do {
			if (!qm_ttl(qm)) {
				goto check;
			}
			written = do_trans(qm);			
			iterations += written;
		} while(written == qm->max_trans);
		
		if (switch_test_flag((&runtime), SCF_DEBUG_SQL)) {
			char line[128] = "";
			int l;
			
			switch_snprintf(line, sizeof(line), "%s RUN QUEUE [", qm->name);
			
			for (i = 0; i < qm->numq; i++) {
				l = strlen(line);
				switch_snprintf(line + l, sizeof(line) - l, "%d%s", switch_queue_size(qm->sql_queue[i]), i == qm->numq - 1 ? "" : "|");
			}
			
			l = strlen(line);
			switch_snprintf(line + l, sizeof(line) - l, "]--[%d]\n", iterations);
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s", line);
			
		}

	check:

		if ((lc = qm_ttl(qm)) == 0) {
			switch_mutex_lock(qm->cond2_mutex);
			switch_thread_cond_wait(qm->cond, qm->cond_mutex);
			switch_mutex_unlock(qm->cond2_mutex);
		}

		i = 40;

		while (--i > 0 && (lc = qm_ttl(qm)) < 500) {
			switch_yield(5000);
		}


	}

	switch_mutex_unlock(qm->cond_mutex);

	for(i = 0; i < qm->numq; i++) {
		do_flush(qm, i, qm->event_db);
	}

	switch_cache_db_release_db_handle(&qm->event_db);

	qm->thread_running = 0;
	
	return NULL;
}


static char *parse_presence_data_cols(switch_event_t *event)
{
	char *cols[128] = { 0 };
	int col_count = 0;
	char *data_copy;
	switch_stream_handle_t stream = { 0 };
	int i;
	char *r;
	char col_name[128] = "";
	const char *data = switch_event_get_header(event, "presence-data-cols");

	if (zstr(data)) {
		return NULL;
	}

	data_copy = strdup(data);
	
	col_count = switch_split(data_copy, ':', cols);

	SWITCH_STANDARD_STREAM(stream);

	for (i = 0; i < col_count; i++) {
		const char *val = NULL;

		switch_snprintfv(col_name, sizeof(col_name), "PD-%q", cols[i]);
		val = switch_event_get_header_nil(event, col_name);
		if (zstr(val)) {
			stream.write_function(&stream, "%q=NULL,", cols[i]);
		} else {
			stream.write_function(&stream, "%q='%q',", cols[i], val);
		}
	}

	r = (char *) stream.data;

	if (end_of(r) == ',') {
		end_of(r) = '\0';
	}

	switch_safe_free(data_copy);
	
	return r;
	
}


#define MAX_SQL 5
#define new_sql()   switch_assert(sql_idx+1 < MAX_SQL); if (exists) sql[sql_idx++]
#define new_sql_a() switch_assert(sql_idx+1 < MAX_SQL); sql[sql_idx++]

static void core_event_handler(switch_event_t *event)
{
	char *sql[MAX_SQL] = { 0 };
	int sql_idx = 0;
	char *extra_cols;
	int exists = 1;
	char *uuid = NULL;

	switch_assert(event);

	switch (event->event_id) {
	case SWITCH_EVENT_CHANNEL_UUID:
	case SWITCH_EVENT_CHANNEL_CREATE:
	case SWITCH_EVENT_CHANNEL_ANSWER:
	case SWITCH_EVENT_CHANNEL_PROGRESS_MEDIA:
	case SWITCH_EVENT_CHANNEL_HOLD:
	case SWITCH_EVENT_CHANNEL_UNHOLD:
	case SWITCH_EVENT_CHANNEL_EXECUTE:
	case SWITCH_EVENT_CHANNEL_ORIGINATE:
	case SWITCH_EVENT_CALL_UPDATE:
	case SWITCH_EVENT_CHANNEL_CALLSTATE:
	case SWITCH_EVENT_CHANNEL_STATE:
	case SWITCH_EVENT_CHANNEL_BRIDGE:
	case SWITCH_EVENT_CHANNEL_UNBRIDGE:
	case SWITCH_EVENT_CALL_SECURE:
		{
			if ((uuid = switch_event_get_header(event, "unique-id"))) {
				exists = switch_ivr_uuid_exists(uuid);
			}
		}
		break;
	default:
		break;
	}

	switch (event->event_id) {
	case SWITCH_EVENT_ADD_SCHEDULE:
		{
			const char *id = switch_event_get_header(event, "task-id");
			const char *manager = switch_event_get_header(event, "task-sql_manager");

			if (id) {
				new_sql() = switch_mprintf("insert into tasks values(%q,'%q','%q',%q, '%q')",
										   id,
										   switch_event_get_header_nil(event, "task-desc"),
										   switch_event_get_header_nil(event, "task-group"), manager ? manager : "0", switch_core_get_hostname()
										   );
			}
		}
		break;
	case SWITCH_EVENT_DEL_SCHEDULE:
	case SWITCH_EVENT_EXE_SCHEDULE:
		new_sql() = switch_mprintf("delete from tasks where task_id=%q and hostname='%q'",
								   switch_event_get_header_nil(event, "task-id"), switch_core_get_hostname());
		break;
	case SWITCH_EVENT_RE_SCHEDULE:
		{
			const char *id = switch_event_get_header(event, "task-id");
			const char *manager = switch_event_get_header(event, "task-sql_manager");

			if (id) {
				new_sql() = switch_mprintf("update tasks set task_desc='%q',task_group='%q', task_sql_manager=%q where task_id=%q and hostname='%q'",
										   switch_event_get_header_nil(event, "task-desc"),
										   switch_event_get_header_nil(event, "task-group"), manager ? manager : "0", id,
										   switch_core_get_hostname());
			}
		}
		break;
	case SWITCH_EVENT_CHANNEL_DESTROY:
		{
			const char *uuid = switch_event_get_header(event, "unique-id");
			
			if (uuid) {
				new_sql() = switch_mprintf("delete from channels where uuid='%q'",
										   uuid);

				new_sql() = switch_mprintf("delete from calls where (caller_uuid='%q' or callee_uuid='%q')",
										   uuid, uuid);

			}
		}
		break;
	case SWITCH_EVENT_CHANNEL_UUID:
		{
			new_sql() = switch_mprintf("update channels set uuid='%q' where uuid='%q'",
									   switch_event_get_header_nil(event, "unique-id"),
									   switch_event_get_header_nil(event, "old-unique-id")
									   );

			new_sql() = switch_mprintf("update channels set call_uuid='%q' where call_uuid='%q'",
									   switch_event_get_header_nil(event, "unique-id"),
									   switch_event_get_header_nil(event, "old-unique-id")
									   );
			break;
		}
	case SWITCH_EVENT_CHANNEL_CREATE:
		new_sql() = switch_mprintf("insert into channels (uuid,direction,created,created_epoch, name,state,callstate,dialplan,context,hostname) "
								   "values('%q','%q','%q','%ld','%q','%q','%q','%q','%q','%q')",
								   switch_event_get_header_nil(event, "unique-id"),
								   switch_event_get_header_nil(event, "call-direction"),
								   switch_event_get_header_nil(event, "event-date-local"),
								   (long) switch_epoch_time_now(NULL),
								   switch_event_get_header_nil(event, "channel-name"),
								   switch_event_get_header_nil(event, "channel-state"),
								   switch_event_get_header_nil(event, "channel-call-state"),
								   switch_event_get_header_nil(event, "caller-dialplan"),
								   switch_event_get_header_nil(event, "caller-context"), switch_core_get_switchname()
								   );
		break;
	case SWITCH_EVENT_CHANNEL_ANSWER:
	case SWITCH_EVENT_CHANNEL_PROGRESS_MEDIA:
	case SWITCH_EVENT_CODEC:
		new_sql() =
			switch_mprintf
			("update channels set read_codec='%q',read_rate='%q',read_bit_rate='%q',write_codec='%q',write_rate='%q',write_bit_rate='%q' where uuid='%q'",
			 switch_event_get_header_nil(event, "channel-read-codec-name"),
			 switch_event_get_header_nil(event, "channel-read-codec-rate"),
			 switch_event_get_header_nil(event, "channel-read-codec-bit-rate"),
			 switch_event_get_header_nil(event, "channel-write-codec-name"),
			 switch_event_get_header_nil(event, "channel-write-codec-rate"),
			 switch_event_get_header_nil(event, "channel-write-codec-bit-rate"),
			 switch_event_get_header_nil(event, "unique-id"));
		break;
	case SWITCH_EVENT_CHANNEL_HOLD:
	case SWITCH_EVENT_CHANNEL_UNHOLD:
	case SWITCH_EVENT_CHANNEL_EXECUTE: {
		
		new_sql() = switch_mprintf("update channels set application='%q',application_data='%q',"
								   "presence_id='%q',presence_data='%q' where uuid='%q'",
								   switch_event_get_header_nil(event, "application"),
								   switch_event_get_header_nil(event, "application-data"),
								   switch_event_get_header_nil(event, "channel-presence-id"),
								   switch_event_get_header_nil(event, "channel-presence-data"),
								   switch_event_get_header_nil(event, "unique-id")
								   );

	}
		break;

	case SWITCH_EVENT_CHANNEL_ORIGINATE:
		{
			if ((extra_cols = parse_presence_data_cols(event))) {
				new_sql() = switch_mprintf("update channels set "
										   "presence_id='%q',presence_data='%q', call_uuid='%q',%s where uuid='%q'",
										   switch_event_get_header_nil(event, "channel-presence-id"),
										   switch_event_get_header_nil(event, "channel-presence-data"),
										   switch_event_get_header_nil(event, "channel-call-uuid"),
										   extra_cols,
										   switch_event_get_header_nil(event, "unique-id"));
				free(extra_cols);
			} else {
				new_sql() = switch_mprintf("update channels set "
										   "presence_id='%q',presence_data='%q', call_uuid='%q' where uuid='%q'",
										   switch_event_get_header_nil(event, "channel-presence-id"),
										   switch_event_get_header_nil(event, "channel-presence-data"),
										   switch_event_get_header_nil(event, "channel-call-uuid"),
										   switch_event_get_header_nil(event, "unique-id"));
			}

		}

		break;
	case SWITCH_EVENT_CALL_UPDATE:
		{
			new_sql() = switch_mprintf("update channels set callee_name='%q',callee_num='%q',sent_callee_name='%q',sent_callee_num='%q',callee_direction='%q',"
									   "cid_name='%q',cid_num='%q' where uuid='%s'",
									   switch_event_get_header_nil(event, "caller-callee-id-name"),
									   switch_event_get_header_nil(event, "caller-callee-id-number"),
									   switch_event_get_header_nil(event, "sent-callee-id-name"),
									   switch_event_get_header_nil(event, "sent-callee-id-number"),
									   switch_event_get_header_nil(event, "direction"),
									   switch_event_get_header_nil(event, "caller-caller-id-name"),
									   switch_event_get_header_nil(event, "caller-caller-id-number"),
									   switch_event_get_header_nil(event, "unique-id")
									   );
		}
		break;
	case SWITCH_EVENT_CHANNEL_CALLSTATE:
		{
			char *num = switch_event_get_header_nil(event, "channel-call-state-number");
			switch_channel_callstate_t callstate = CCS_DOWN;

			if (num) {
				callstate = atoi(num);
			}

			if (callstate != CCS_DOWN && callstate != CCS_HANGUP) {
				if ((extra_cols = parse_presence_data_cols(event))) {
					new_sql() = switch_mprintf("update channels set callstate='%q',%s where uuid='%q'",
											   switch_event_get_header_nil(event, "channel-call-state"),
											   extra_cols,
											   switch_event_get_header_nil(event, "unique-id"));
					free(extra_cols);
				} else {
					new_sql() = switch_mprintf("update channels set callstate='%q' where uuid='%q'",
											   switch_event_get_header_nil(event, "channel-call-state"),
											   switch_event_get_header_nil(event, "unique-id"));
				}
			}

		}
		break;
	case SWITCH_EVENT_CHANNEL_STATE:
		{
			char *state = switch_event_get_header_nil(event, "channel-state-number");
			switch_channel_state_t state_i = CS_DESTROY;

			if (!zstr(state)) {
				state_i = atoi(state);
			}

			switch (state_i) {
			case CS_NEW:
			case CS_DESTROY:
			case CS_REPORTING:
#ifndef SWITCH_DEPRECATED_CORE_DB
			case CS_HANGUP: /* marked for deprication */
#endif
			case CS_INIT:
				break;
#ifdef SWITCH_DEPRECATED_CORE_DB
			case CS_HANGUP: /* marked for deprication */
				new_sql_a() = switch_mprintf("update channels set state='%s' where uuid='%s'",
											 switch_event_get_header_nil(event, "channel-state"),
											 switch_event_get_header_nil(event, "unique-id"));
				break;
#endif
			case CS_EXECUTE:
				if ((extra_cols = parse_presence_data_cols(event))) {
					new_sql() = switch_mprintf("update channels set state='%s',%s where uuid='%q'",
											   switch_event_get_header_nil(event, "channel-state"),
											   extra_cols,
											   switch_event_get_header_nil(event, "unique-id"));
					free(extra_cols);
					
				} else {
					new_sql() = switch_mprintf("update channels set state='%s' where uuid='%s'",
											   switch_event_get_header_nil(event, "channel-state"),
											   switch_event_get_header_nil(event, "unique-id"));
				}
				break;
			case CS_ROUTING:
				if ((extra_cols = parse_presence_data_cols(event))) {
					new_sql() = switch_mprintf("update channels set state='%s',cid_name='%q',cid_num='%q',callee_name='%q',callee_num='%q',"
											   "sent_callee_name='%q',sent_callee_num='%q',"
											   "ip_addr='%s',dest='%q',dialplan='%q',context='%q',presence_id='%q',presence_data='%q',%s "
											   "where uuid='%s'",
											   switch_event_get_header_nil(event, "channel-state"),
											   switch_event_get_header_nil(event, "caller-caller-id-name"),
											   switch_event_get_header_nil(event, "caller-caller-id-number"),
											   switch_event_get_header_nil(event, "caller-callee-id-name"),
											   switch_event_get_header_nil(event, "caller-callee-id-number"),
											   switch_event_get_header_nil(event, "sent-callee-id-name"),
											   switch_event_get_header_nil(event, "sent-callee-id-number"),
											   switch_event_get_header_nil(event, "caller-network-addr"),
											   switch_event_get_header_nil(event, "caller-destination-number"),
											   switch_event_get_header_nil(event, "caller-dialplan"),
											   switch_event_get_header_nil(event, "caller-context"),
											   switch_event_get_header_nil(event, "channel-presence-id"),
											   switch_event_get_header_nil(event, "channel-presence-data"),
											   extra_cols,
											   switch_event_get_header_nil(event, "unique-id"));
					free(extra_cols);
				} else {
					new_sql() = switch_mprintf("update channels set state='%s',cid_name='%q',cid_num='%q',callee_name='%q',callee_num='%q',"
											   "sent_callee_name='%q',sent_callee_num='%q',"
											   "ip_addr='%s',dest='%q',dialplan='%q',context='%q',presence_id='%q',presence_data='%q' "
											   "where uuid='%s'",
											   switch_event_get_header_nil(event, "channel-state"),
											   switch_event_get_header_nil(event, "caller-caller-id-name"),
											   switch_event_get_header_nil(event, "caller-caller-id-number"),
											   switch_event_get_header_nil(event, "caller-callee-id-name"),
											   switch_event_get_header_nil(event, "caller-callee-id-number"),
											   switch_event_get_header_nil(event, "sent-callee-id-name"),
											   switch_event_get_header_nil(event, "sent-callee-id-number"),
											   switch_event_get_header_nil(event, "caller-network-addr"),
											   switch_event_get_header_nil(event, "caller-destination-number"),
											   switch_event_get_header_nil(event, "caller-dialplan"),
											   switch_event_get_header_nil(event, "caller-context"),
											   switch_event_get_header_nil(event, "channel-presence-id"),
											   switch_event_get_header_nil(event, "channel-presence-data"),
											   switch_event_get_header_nil(event, "unique-id"));
				}
				break;
			default:
				new_sql() = switch_mprintf("update channels set state='%s' where uuid='%s'",
										   switch_event_get_header_nil(event, "channel-state"),
										   switch_event_get_header_nil(event, "unique-id"));
				break;
			}

			break;


		}
	case SWITCH_EVENT_CHANNEL_BRIDGE:
		{
			const char *a_uuid, *b_uuid, *uuid;

			a_uuid = switch_event_get_header(event, "Bridge-A-Unique-ID");
			b_uuid = switch_event_get_header(event, "Bridge-B-Unique-ID");
			uuid = switch_event_get_header(event, "unique-id");

			if (zstr(a_uuid) || zstr(b_uuid)) {
				a_uuid = switch_event_get_header_nil(event, "caller-unique-id");
				b_uuid = switch_event_get_header_nil(event, "other-leg-unique-id");
			}

			if (uuid && (extra_cols = parse_presence_data_cols(event))) {
				new_sql() = switch_mprintf("update channels set %s where uuid='%s'", extra_cols, uuid);
				free(extra_cols);
			} 

			new_sql() = switch_mprintf("update channels set call_uuid='%q' where uuid='%s' or uuid='%s'",
									   switch_event_get_header_nil(event, "channel-call-uuid"), a_uuid, b_uuid);
			

			new_sql() = switch_mprintf("insert into calls (call_uuid,call_created,call_created_epoch,"
									   "caller_uuid,callee_uuid,hostname) "
									   "values ('%s','%s','%ld','%q','%q','%q')",
									   switch_event_get_header_nil(event, "channel-call-uuid"),
									   switch_event_get_header_nil(event, "event-date-local"),
									   (long) switch_epoch_time_now(NULL),
									   a_uuid,
									   b_uuid,
									   switch_core_get_switchname()
									   );
		}
		break;
	case SWITCH_EVENT_CHANNEL_UNBRIDGE:
		{
			char *cuuid = switch_event_get_header_nil(event, "caller-unique-id");
			char *uuid = switch_event_get_header(event, "unique-id");

			if (uuid && (extra_cols = parse_presence_data_cols(event))) {
				new_sql() = switch_mprintf("update channels set %s where uuid='%s'", extra_cols, uuid);
				free(extra_cols);
			} 

			new_sql() = switch_mprintf("update channels set call_uuid=uuid where call_uuid='%s'",
									   switch_event_get_header_nil(event, "channel-call-uuid"));
			
			new_sql() = switch_mprintf("delete from calls where (caller_uuid='%q' or callee_uuid='%q')",
									   cuuid, cuuid);
			break;
		}
	case SWITCH_EVENT_SHUTDOWN:
		new_sql() = switch_mprintf("delete from channels where hostname='%q';"
								   "delete from interfaces where hostname='%q';"
								   "delete from calls where hostname='%q'",
								   switch_core_get_switchname(), switch_core_get_hostname(), switch_core_get_switchname()
								   );
		break;
	case SWITCH_EVENT_LOG:
		return;
	case SWITCH_EVENT_MODULE_LOAD:
		{
			const char *type = switch_event_get_header_nil(event, "type");
			const char *name = switch_event_get_header_nil(event, "name");
			const char *description = switch_event_get_header_nil(event, "description");
			const char *syntax = switch_event_get_header_nil(event, "syntax");
			const char *key = switch_event_get_header_nil(event, "key");
			const char *filename = switch_event_get_header_nil(event, "filename");
			if (!zstr(type) && !zstr(name)) {
				new_sql() =
					switch_mprintf
					("insert into interfaces (type,name,description,syntax,ikey,filename,hostname) values('%q','%q','%q','%q','%q','%q','%q')", type, name,
					 switch_str_nil(description), switch_str_nil(syntax), switch_str_nil(key), switch_str_nil(filename),
					 switch_core_get_hostname()
					 );
			}
			break;
		}
	case SWITCH_EVENT_MODULE_UNLOAD:
		{
			const char *type = switch_event_get_header_nil(event, "type");
			const char *name = switch_event_get_header_nil(event, "name");
			if (!zstr(type) && !zstr(name)) {
				new_sql() = switch_mprintf("delete from interfaces where type='%q' and name='%q' and hostname='%q'", type, name,
										   switch_core_get_hostname());
			}
			break;
		}
	case SWITCH_EVENT_CALL_SECURE:
		{
			const char *type = switch_event_get_header_nil(event, "secure_type");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Secure Type: %s\n", type);
			if (zstr(type)) {
				break;
			}
			new_sql() = switch_mprintf("update channels set secure='%s' where uuid='%s'",
									   type, switch_event_get_header_nil(event, "caller-unique-id")
									   );
			break;
		}
	case SWITCH_EVENT_NAT:
		{
			const char *op = switch_event_get_header_nil(event, "op");
			switch_bool_t sticky = switch_true(switch_event_get_header_nil(event, "sticky"));
			if (!strcmp("add", op)) {
				new_sql() = switch_mprintf("insert into nat (port, proto, sticky, hostname) values (%s, %s, %d,'%q')",
										   switch_event_get_header_nil(event, "port"),
										   switch_event_get_header_nil(event, "proto"), sticky, switch_core_get_hostname()
										   );
			} else if (!strcmp("del", op)) {
				new_sql() = switch_mprintf("delete from nat where port=%s and proto=%s and hostname='%q'",
										   switch_event_get_header_nil(event, "port"),
										   switch_event_get_header_nil(event, "proto"), switch_core_get_hostname());
			} else if (!strcmp("status", op)) {
				/* call show nat api */
			} else if (!strcmp("status_response", op)) {
				/* ignore */
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown op for SWITCH_EVENT_NAT: %s\n", op);
			}
			break;
		}
	default:
		break;
	}

	if (sql_idx) {
		int i = 0;
		

		for (i = 0; i < sql_idx; i++) {
			if (switch_stristr("update channels", sql[i]) || switch_stristr("delete from channels", sql[i])) {
				switch_sql_queue_manager_push(sql_manager.qm, sql[i], 1, SWITCH_FALSE);
			} else {
				switch_sql_queue_manager_push(sql_manager.qm, sql[i], 0, SWITCH_FALSE);
			}
			sql[i] = NULL;
		}
	}
}


static char create_complete_sql[] =
	"CREATE TABLE complete (\n"
	"   sticky  INTEGER,\n"
	"   a1  VARCHAR(128),\n"
	"   a2  VARCHAR(128),\n"
	"   a3  VARCHAR(128),\n"
	"   a4  VARCHAR(128),\n"
	"   a5  VARCHAR(128),\n"
	"   a6  VARCHAR(128),\n"
	"   a7  VARCHAR(128),\n"
	"   a8  VARCHAR(128),\n"
	"   a9  VARCHAR(128),\n"
	"   a10 VARCHAR(128),\n"
	"   hostname VARCHAR(256)\n"
	");\n";

static char create_alias_sql[] =
	"CREATE TABLE aliases (\n"
	"   sticky  INTEGER,\n"
	"   alias  VARCHAR(128),\n"
	"   command  VARCHAR(4096),\n"
	"   hostname VARCHAR(256)\n"
	");\n";

static char create_channels_sql[] =
	"CREATE TABLE channels (\n"
	"   uuid  VARCHAR(256),\n"
	"   direction  VARCHAR(32),\n"
	"   created  VARCHAR(128),\n"
	"   created_epoch  INTEGER,\n"
	"   name  VARCHAR(1024),\n"
	"   state  VARCHAR(64),\n"
	"   cid_name  VARCHAR(1024),\n"
	"   cid_num  VARCHAR(256),\n"
	"   ip_addr  VARCHAR(256),\n"
	"   dest  VARCHAR(1024),\n"
	"   application  VARCHAR(128),\n"
	"   application_data  VARCHAR(4096),\n"
	"   dialplan VARCHAR(128),\n"
	"   context VARCHAR(128),\n"
	"   read_codec  VARCHAR(128),\n"
	"   read_rate  VARCHAR(32),\n"
	"   read_bit_rate  VARCHAR(32),\n"
	"   write_codec  VARCHAR(128),\n"
	"   write_rate  VARCHAR(32),\n"
	"   write_bit_rate  VARCHAR(32),\n"
	"   secure VARCHAR(32),\n"
	"   hostname VARCHAR(256),\n"
	"   presence_id VARCHAR(4096),\n"
	"   presence_data VARCHAR(4096),\n"
	"   callstate  VARCHAR(64),\n"
	"   callee_name  VARCHAR(1024),\n"
	"   callee_num  VARCHAR(256),\n"
	"   callee_direction  VARCHAR(5),\n"
	"   call_uuid  VARCHAR(256),\n"
	"   sent_callee_name  VARCHAR(1024),\n"
	"   sent_callee_num  VARCHAR(256)\n"
	");\n";

static char create_calls_sql[] =
	"CREATE TABLE calls (\n"
	"   call_uuid  VARCHAR(255),\n"
	"   call_created  VARCHAR(128),\n"
	"   call_created_epoch  INTEGER,\n"
	"   caller_uuid      VARCHAR(256),\n"
	"   callee_uuid      VARCHAR(256),\n"
	"   hostname VARCHAR(256)\n"
	");\n";

static char create_interfaces_sql[] =
	"CREATE TABLE interfaces (\n"
	"   type             VARCHAR(128),\n"
	"   name             VARCHAR(1024),\n"
	"   description      VARCHAR(4096),\n"
	"   ikey             VARCHAR(1024),\n"
	"   filename         VARCHAR(4096),\n"
	"   syntax           VARCHAR(4096),\n"
	"   hostname VARCHAR(256)\n"
	");\n";

static char create_tasks_sql[] =
	"CREATE TABLE tasks (\n"
	"   task_id             INTEGER,\n"
	"   task_desc           VARCHAR(4096),\n"
	"   task_group          VARCHAR(1024),\n"
	"   task_sql_manager    INTEGER,\n"
	"   hostname VARCHAR(256)\n"
	");\n";

static char create_nat_sql[] =
	"CREATE TABLE nat (\n"
	"   sticky  INTEGER,\n"
	"	port	INTEGER,\n"
	"	proto	INTEGER,\n"
	"   hostname VARCHAR(256)\n"
	");\n";


static char create_registrations_sql[] =
	"CREATE TABLE registrations (\n"
	"   reg_user      VARCHAR(256),\n"
	"   realm     VARCHAR(256),\n"
	"   token     VARCHAR(256),\n"
	/* If url is modified please check for code in switch_core_sqldb_start for dependencies for MSSQL" */
	"   url      TEXT,\n"
	"   expires  INTEGER,\n"
	"   network_ip VARCHAR(256),\n"
	"   network_port VARCHAR(256),\n"
	"   network_proto VARCHAR(256),\n"
	"   hostname VARCHAR(256),\n"
	"   metadata VARCHAR(256)\n"
	");\n";

	


static char detailed_calls_sql[] =
	"create view detailed_calls as select "
	"a.uuid as uuid,"
	"a.direction as direction,"
	"a.created as created,"
	"a.created_epoch as created_epoch,"
	"a.name as name,"
	"a.state as state,"
	"a.cid_name as cid_name,"
	"a.cid_num as cid_num,"
	"a.ip_addr as ip_addr,"
	"a.dest as dest,"
	"a.application as application,"
	"a.application_data as application_data,"
	"a.dialplan as dialplan,"
	"a.context as context,"
	"a.read_codec as read_codec,"
	"a.read_rate as read_rate,"
	"a.read_bit_rate as read_bit_rate,"
	"a.write_codec as write_codec,"
	"a.write_rate as write_rate,"
	"a.write_bit_rate as write_bit_rate,"
	"a.secure as secure,"
	"a.hostname as hostname,"
	"a.presence_id as presence_id,"
	"a.presence_data as presence_data,"
	"a.callstate as callstate,"
	"a.callee_name as callee_name,"
	"a.callee_num as callee_num,"
	"a.callee_direction as callee_direction,"
	"a.call_uuid as call_uuid,"
	"a.sent_callee_name as sent_callee_name,"
	"a.sent_callee_num as sent_callee_num,"
	"b.uuid as b_uuid,"
	"b.direction as b_direction,"
	"b.created as b_created,"
	"b.created_epoch as b_created_epoch,"
	"b.name as b_name,"
	"b.state as b_state,"
	"b.cid_name as b_cid_name,"
	"b.cid_num as b_cid_num,"
	"b.ip_addr as b_ip_addr,"
	"b.dest as b_dest,"
	"b.application as b_application,"
	"b.application_data as b_application_data,"
	"b.dialplan as b_dialplan,"
	"b.context as b_context,"
	"b.read_codec as b_read_codec,"
	"b.read_rate as b_read_rate,"
	"b.read_bit_rate as b_read_bit_rate,"
	"b.write_codec as b_write_codec,"
	"b.write_rate as b_write_rate,"
	"b.write_bit_rate as b_write_bit_rate,"
	"b.secure as b_secure,"
	"b.hostname as b_hostname,"
	"b.presence_id as b_presence_id,"
	"b.presence_data as b_presence_data,"
	"b.callstate as b_callstate,"
	"b.callee_name as b_callee_name,"
	"b.callee_num as b_callee_num,"
	"b.callee_direction as b_callee_direction,"
	"b.call_uuid as b_call_uuid,"
	"b.sent_callee_name as b_sent_callee_name,"
	"b.sent_callee_num as b_sent_callee_num,"
	"c.call_created_epoch as call_created_epoch "
	"from channels a "
	"left join calls c on a.uuid = c.caller_uuid and a.hostname = c.hostname "
	"left join channels b on b.uuid = c.callee_uuid and b.hostname = c.hostname "
	"where a.uuid = c.caller_uuid or a.uuid not in (select callee_uuid from calls)";


static char recovery_sql[] =
	"CREATE TABLE recovery (\n"
	"   runtime_uuid    VARCHAR(255),\n"
	"   technology      VARCHAR(255),\n"
	"   profile_name    VARCHAR(255),\n"
	"   hostname        VARCHAR(255),\n"
	"   uuid            VARCHAR(255),\n"
	"   metadata        text\n"
	");\n";

static char basic_calls_sql[] =
	"create view basic_calls as select "
	"a.uuid as uuid,"
	"a.direction as direction,"
	"a.created as created,"
	"a.created_epoch as created_epoch,"
	"a.name as name,"
	"a.state as state,"
	"a.cid_name as cid_name,"
	"a.cid_num as cid_num,"
	"a.ip_addr as ip_addr,"
	"a.dest as dest,"

	"a.presence_id as presence_id,"
	"a.presence_data as presence_data,"
	"a.callstate as callstate,"
	"a.callee_name as callee_name,"
	"a.callee_num as callee_num,"
	"a.callee_direction as callee_direction,"
	"a.call_uuid as call_uuid,"
	"a.hostname as hostname,"
	"a.sent_callee_name as sent_callee_name,"
	"a.sent_callee_num as sent_callee_num,"


	"b.uuid as b_uuid,"
	"b.direction as b_direction,"
	"b.created as b_created,"
	"b.created_epoch as b_created_epoch,"
	"b.name as b_name,"
	"b.state as b_state,"
	"b.cid_name as b_cid_name,"
	"b.cid_num as b_cid_num,"
	"b.ip_addr as b_ip_addr,"
	"b.dest as b_dest,"
	
	"b.presence_id as b_presence_id,"
	"b.presence_data as b_presence_data,"
	"b.callstate as b_callstate,"
	"b.callee_name as b_callee_name,"
	"b.callee_num as b_callee_num,"
	"b.callee_direction as b_callee_direction,"
	"b.sent_callee_name as b_sent_callee_name,"
	"b.sent_callee_num as b_sent_callee_num,"
	"c.call_created_epoch as call_created_epoch "

	"from channels a "
	"left join calls c on a.uuid = c.caller_uuid and a.hostname = c.hostname "
	"left join channels b on b.uuid = c.callee_uuid and b.hostname = c.hostname "
	"where a.uuid = c.caller_uuid or a.uuid not in (select callee_uuid from calls)";



SWITCH_DECLARE(void) switch_core_recovery_flush(const char *technology, const char *profile_name)
{
	char *sql = NULL;
	switch_cache_db_handle_t *dbh;

	if (switch_core_db_handle(&dbh) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB!\n");
		return;
	}

	if (zstr(technology)) {

		if (zstr(profile_name)) {
			sql = switch_mprintf("delete from recovery");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "INVALID\n");
		}

	} else {
		if (zstr(profile_name)) {
			sql = switch_mprintf("delete from recovery where technology='%q' ", technology);
		} else {
			sql = switch_mprintf("delete from recovery where technology='%q' and profile_name='%q'", technology, profile_name);
		}
	}

	if (sql) {
		switch_cache_db_execute_sql(dbh, sql, NULL);
		switch_safe_free(sql);
	}
	
	switch_cache_db_release_db_handle(&dbh);
}


static int recover_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	int *rp = (int *) pArg;
	switch_xml_t xml;
	switch_endpoint_interface_t *ep;
	switch_core_session_t *session;

	if (argc < 4) {
		return 0;
	}
	
	if (!(xml = switch_xml_parse_str_dynamic(argv[4], SWITCH_TRUE))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "XML ERROR\n");
		return 0;
	}

	if (!(ep = switch_loadable_module_get_endpoint_interface(argv[0]))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "EP ERROR\n");
		return 0;
	}

	if (!(session = switch_core_session_request_xml(ep, NULL, xml))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid cdr data, call not recovered\n");
		goto end;
	}

	if (ep->recover_callback) {
		switch_caller_extension_t *extension = NULL;


		if (ep->recover_callback(session) > 0) {
			switch_channel_t *channel = switch_core_session_get_channel(session);

			if (switch_channel_get_partner_uuid(channel)) {
				switch_channel_set_flag(channel, CF_RECOVERING_BRIDGE);
			} else {
				switch_xml_t callflow, param, x_extension;
				if ((extension = switch_caller_extension_new(session, "recovery", "recovery")) == 0) {
					abort();
				}

				if ((callflow = switch_xml_child(xml, "callflow")) && (x_extension = switch_xml_child(callflow, "extension"))) {
					for (param = switch_xml_child(x_extension, "application"); param; param = param->next) {
						const char *var = switch_xml_attr_soft(param, "app_name");
						const char *val = switch_xml_attr_soft(param, "app_data");
						/* skip announcement type apps */
						if (strcasecmp(var, "speak") && strcasecmp(var, "playback") && strcasecmp(var, "gentones") && strcasecmp(var, "say")) {
							switch_caller_extension_add_application(session, extension, var, val);
						}
					}
				}

				switch_channel_set_caller_extension(channel, extension);
			}

			switch_channel_set_state(channel, CS_INIT);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, 
							  "Resurrecting fallen channel %s\n", switch_channel_get_name(channel));
			switch_core_session_thread_launch(session);

			*rp = (*rp) + 1;
			
		}

	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Endpoint %s has no recovery function\n", argv[0]);
	}


 end:

	UNPROTECT_INTERFACE(ep);

	switch_xml_free(xml);

	return 0;
}

SWITCH_DECLARE(int) switch_core_recovery_recover(const char *technology, const char *profile_name)
												  
{
	char *sql = NULL;
	char *errmsg = NULL;
	switch_cache_db_handle_t *dbh;
	int r = 0;

	if (!sql_manager.manage) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "DATABASE NOT AVAIALBLE, REVCOVERY NOT POSSIBLE\n");
		return 0;
	}

	if (switch_core_db_handle(&dbh) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB!\n");
		return 0;
	}

	if (zstr(technology)) {
		
		if (zstr(profile_name)) {
			sql = switch_mprintf("select technology, profile_name, hostname, uuid, metadata "
								 "from recovery where runtime_uuid!='%q'", 
								 switch_core_get_uuid());
		} else {
			sql = switch_mprintf("select technology, profile_name, hostname, uuid, metadata "
								 "from recovery where runtime_uuid!='%q' and profile_name='%q'", 
								 switch_core_get_uuid(), profile_name);
		}

	} else {

		if (zstr(profile_name)) {
			sql = switch_mprintf("select technology, profile_name, hostname, uuid, metadata "
								 "from recovery where technology='%q' and runtime_uuid!='%q'", 
								 technology, switch_core_get_uuid());
		} else {
			sql = switch_mprintf("select technology, profile_name, hostname, uuid, metadata "
								 "from recovery where technology='%q' and runtime_uuid!='%q' and profile_name='%q'", 
								 technology, switch_core_get_uuid(), profile_name);
		}
	}


	switch_cache_db_execute_sql_callback(dbh, sql, recover_callback, &r, &errmsg);
	
	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s] %s\n", sql, errmsg);
		free(errmsg);
	}

	switch_safe_free(sql);

	if (zstr(technology)) {
		if (zstr(profile_name)) {
			sql = switch_mprintf("delete from recovery where runtime_uuid!='%q'", 
								 switch_core_get_uuid());
		} else {
			sql = switch_mprintf("delete from recovery where runtime_uuid!='%q' and profile_name='%q'", 
								 switch_core_get_uuid(), profile_name);
		}
	} else {
		if (zstr(profile_name)) {
			sql = switch_mprintf("delete from recovery where runtime_uuid!='%q' and technology='%q' ", 
								 switch_core_get_uuid(), technology);
		} else {
			sql = switch_mprintf("delete from recovery where runtime_uuid!='%q' and technology='%q' and profile_name='%q'", 
								 switch_core_get_uuid(), technology, profile_name);
		}
	}

	switch_cache_db_execute_sql(dbh, sql, NULL);
	switch_safe_free(sql);

	switch_cache_db_release_db_handle(&dbh);

	return r;

}

SWITCH_DECLARE(switch_cache_db_handle_type_t) switch_core_dbtype(void)
{
	switch_cache_db_handle_type_t type = SCDB_TYPE_CORE_DB;

	switch_mutex_lock(sql_manager.ctl_mutex);
	if (sql_manager.qm && sql_manager.qm->event_db) {
		type = sql_manager.qm->event_db->type;
	}
	switch_mutex_unlock(sql_manager.ctl_mutex);

	return type;
}

SWITCH_DECLARE(void) switch_core_sql_exec(const char *sql)
{
	if (!sql_manager.manage) {
		return;
	}

	if (!switch_test_flag((&runtime), SCF_USE_SQL)) {
		return;
	}


	switch_sql_queue_manager_push(sql_manager.qm, sql, 3, SWITCH_TRUE);
}

SWITCH_DECLARE(void) switch_core_recovery_untrack(switch_core_session_t *session, switch_bool_t force)
{
	char *sql = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (!sql_manager.manage) {
		return;
	}

	if (!switch_channel_test_flag(channel, CF_ANSWERED) || switch_channel_get_state(channel) < CS_SOFT_EXECUTE) {
		return;
	}

	if (!switch_channel_test_flag(channel, CF_TRACKABLE)) {
		return;
	}

	if ((switch_channel_test_flag(channel, CF_RECOVERING))) {
		return;
	}

	if (switch_channel_test_flag(channel, CF_TRACKED) || force) {

		if (force) {
			sql = switch_mprintf("delete from recovery where uuid='%q'", switch_core_session_get_uuid(session));
			
		} else {
			sql = switch_mprintf("delete from recovery where runtime_uuid='%q' and uuid='%q'",
								 switch_core_get_uuid(), switch_core_session_get_uuid(session));
		}

		switch_sql_queue_manager_push(sql_manager.qm, sql, 3, SWITCH_FALSE);
		
		switch_channel_clear_flag(channel, CF_TRACKED);
	}
	
}

SWITCH_DECLARE(void) switch_core_recovery_track(switch_core_session_t *session)
{
	switch_xml_t cdr = NULL;
	char *xml_cdr_text = NULL;
	char *sql = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *profile_name;
	const char *technology;

	if (!sql_manager.manage) {
		return;
	}

	if (!switch_channel_test_flag(channel, CF_ANSWERED) || switch_channel_get_state(channel) < CS_SOFT_EXECUTE) {
		return;
	}

	if (switch_channel_test_flag(channel, CF_RECOVERING) || !switch_channel_test_flag(channel, CF_TRACKABLE)) {
		return;
	}

	profile_name = switch_channel_get_variable_dup(channel, "recovery_profile_name", SWITCH_FALSE, -1);
	technology = session->endpoint_interface->interface_name;

	if (switch_ivr_generate_xml_cdr(session, &cdr) == SWITCH_STATUS_SUCCESS) {
		xml_cdr_text = switch_xml_toxml_nolock(cdr, SWITCH_FALSE);
		switch_xml_free(cdr);
	}

	if (xml_cdr_text) {
		if (switch_channel_test_flag(channel, CF_TRACKED)) {
			sql = switch_mprintf("update recovery set metadata='%q' where uuid='%q'",  xml_cdr_text, switch_core_session_get_uuid(session));
		} else {
			sql = switch_mprintf("insert into recovery (runtime_uuid, technology, profile_name, hostname, uuid, metadata) "
								 "values ('%q','%q','%q','%q','%q','%q')",
								 switch_core_get_uuid(), switch_str_nil(technology), 
								 switch_str_nil(profile_name), switch_core_get_switchname(), switch_core_session_get_uuid(session), xml_cdr_text);
		}

		switch_sql_queue_manager_push(sql_manager.qm, sql, 2, SWITCH_FALSE);

		free(xml_cdr_text);
		switch_channel_set_flag(channel, CF_TRACKED);
		
	}
	
}



SWITCH_DECLARE(switch_status_t) switch_core_add_registration(const char *user, const char *realm, const char *token, const char *url, uint32_t expires, 
															 const char *network_ip, const char *network_port, const char *network_proto,
															 const char *metadata)
{
	char *sql;

	if (!switch_test_flag((&runtime), SCF_USE_SQL)) {
		return SWITCH_STATUS_FALSE;
	}

	if (runtime.multiple_registrations) {
		sql = switch_mprintf("delete from registrations where hostname='%q' and (url='%q' or token='%q')", 
							 switch_core_get_switchname(), url, switch_str_nil(token));
	} else {
		sql = switch_mprintf("delete from registrations where reg_user='%q' and realm='%q' and hostname='%q'", 
							 user, realm, switch_core_get_switchname());
	}

	switch_sql_queue_manager_push(sql_manager.qm, sql, 0, SWITCH_FALSE);

	if ( !zstr(metadata) ) {
		sql = switch_mprintf("insert into registrations (reg_user,realm,token,url,expires,network_ip,network_port,network_proto,hostname,metadata) "
							 "values ('%q','%q','%q','%q',%ld,'%q','%q','%q','%q','%q')",
							 switch_str_nil(user),
							 switch_str_nil(realm),
							 switch_str_nil(token),
							 switch_str_nil(url),
							 expires,
							 switch_str_nil(network_ip),
							 switch_str_nil(network_port),
							 switch_str_nil(network_proto),
							 switch_core_get_switchname(),
							 metadata
							 );
	} else {
		sql = switch_mprintf("insert into registrations (reg_user,realm,token,url,expires,network_ip,network_port,network_proto,hostname) "
							 "values ('%q','%q','%q','%q',%ld,'%q','%q','%q','%q')",
							 switch_str_nil(user),
							 switch_str_nil(realm),
							 switch_str_nil(token),
							 switch_str_nil(url),
							 expires,
							 switch_str_nil(network_ip),
							 switch_str_nil(network_port),
							 switch_str_nil(network_proto),
							 switch_core_get_switchname()
							 );
	}

	
	switch_sql_queue_manager_push(sql_manager.qm, sql, 0, SWITCH_FALSE);
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_del_registration(const char *user, const char *realm, const char *token)
{

	char *sql;

	if (!switch_test_flag((&runtime), SCF_USE_SQL)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!zstr(token) && runtime.multiple_registrations) {
		sql = switch_mprintf("delete from registrations where reg_user='%q' and realm='%q' and hostname='%q' and token='%q'", user, realm, switch_core_get_switchname(), token);
	} else {
		sql = switch_mprintf("delete from registrations where reg_user='%q' and realm='%q' and hostname='%q'", user, realm, switch_core_get_switchname());
	}

	switch_sql_queue_manager_push(sql_manager.qm, sql, 0, SWITCH_FALSE);


	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_expire_registration(int force)
{
	
	char *sql;
	time_t now;

	if (!switch_test_flag((&runtime), SCF_USE_SQL)) {
		return SWITCH_STATUS_FALSE;
	}

	now = switch_epoch_time_now(NULL);

	if (force) {
		sql = switch_mprintf("delete from registrations where hostname='%q'", switch_core_get_switchname());
	} else {
		sql = switch_mprintf("delete from registrations where expires > 0 and expires <= %ld and hostname='%q'", now, switch_core_get_switchname());
	}

	switch_sql_queue_manager_push(sql_manager.qm, sql, 0, SWITCH_FALSE);

	return SWITCH_STATUS_SUCCESS;

}

switch_status_t switch_core_sqldb_start(switch_memory_pool_t *pool, switch_bool_t manage)
{
	switch_threadattr_t *thd_attr;

	sql_manager.memory_pool = pool;
	sql_manager.manage = manage;

	switch_mutex_init(&sql_manager.dbh_mutex, SWITCH_MUTEX_NESTED, sql_manager.memory_pool);
	switch_mutex_init(&sql_manager.io_mutex, SWITCH_MUTEX_NESTED, sql_manager.memory_pool);
	switch_mutex_init(&sql_manager.ctl_mutex, SWITCH_MUTEX_NESTED, sql_manager.memory_pool);

	if (!sql_manager.manage) goto skip;

 top:	

	/* Activate SQL database */
	if (switch_core_db_handle(&sql_manager.dbh) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB!\n");

		if (switch_test_flag((&runtime), SCF_CORE_NON_SQLITE_DB_REQ)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failure! ODBC IS REQUIRED!\n");
			return SWITCH_STATUS_FALSE;
		}

		if (runtime.odbc_dsn) {
			runtime.odbc_dsn = NULL;
			runtime.odbc_dbtype = DBTYPE_DEFAULT;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Falling back to core_db.\n");
			goto top;
		}


		switch_clear_flag((&runtime), SCF_USE_SQL);
		return SWITCH_STATUS_FALSE;
	}


	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Opening DB\n");

	switch (sql_manager.dbh->type) {
	case SCDB_TYPE_PGSQL:
	case SCDB_TYPE_ODBC:
		if (switch_test_flag((&runtime), SCF_CLEAR_SQL)) {
			char sql[512] = "";
			char *tables[] = { "channels", "calls", "tasks", NULL };
			int i;
			const char *hostname = switch_core_get_switchname();

			for (i = 0; tables[i]; i++) {
				switch_snprintfv(sql, sizeof(sql), "delete from %q where hostname='%q'", tables[i], hostname);
				switch_cache_db_execute_sql(sql_manager.dbh, sql, NULL);
			}
		}
		break;
	case SCDB_TYPE_CORE_DB:
		{
			switch_cache_db_execute_sql(sql_manager.dbh, "drop table channels", NULL);
			switch_cache_db_execute_sql(sql_manager.dbh, "drop table calls", NULL);
			switch_cache_db_execute_sql(sql_manager.dbh, "drop view detailed_calls", NULL);
			switch_cache_db_execute_sql(sql_manager.dbh, "drop view basic_calls", NULL);
			switch_cache_db_execute_sql(sql_manager.dbh, "drop table interfaces", NULL);
			switch_cache_db_execute_sql(sql_manager.dbh, "drop table tasks", NULL);
			switch_cache_db_execute_sql(sql_manager.dbh, "PRAGMA synchronous=OFF;", NULL);
			switch_cache_db_execute_sql(sql_manager.dbh, "PRAGMA count_changes=OFF;", NULL);
			switch_cache_db_execute_sql(sql_manager.dbh, "PRAGMA default_cache_size=8000", NULL);
			switch_cache_db_execute_sql(sql_manager.dbh, "PRAGMA temp_store=MEMORY;", NULL);
			switch_cache_db_execute_sql(sql_manager.dbh, "PRAGMA journal_mode=OFF;", NULL);
		}
		break;
	}

	switch_cache_db_test_reactive(sql_manager.dbh, "select hostname from aliases", "DROP TABLE aliases", create_alias_sql);
	switch_cache_db_test_reactive(sql_manager.dbh, "select hostname from complete", "DROP TABLE complete", create_complete_sql);
	switch_cache_db_test_reactive(sql_manager.dbh, "select hostname from nat", "DROP TABLE nat", create_nat_sql);
	switch_cache_db_test_reactive(sql_manager.dbh, "delete from registrations where reg_user='' or network_proto='tcp' or network_proto='tls'", 
								  "DROP TABLE registrations", create_registrations_sql);

	switch_cache_db_test_reactive(sql_manager.dbh, "select metadata from registrations", NULL, "ALTER TABLE registrations ADD COLUMN metadata VARCHAR(256)");


	switch_cache_db_test_reactive(sql_manager.dbh, "select hostname from recovery", "DROP TABLE recovery", recovery_sql);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index recovery1 on recovery(technology)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index recovery2 on recovery(profile_name)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index recovery3 on recovery(uuid)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index recovery3 on recovery(runtime_uuid)", NULL);




	switch (sql_manager.dbh->type) {
	case SCDB_TYPE_PGSQL:
	case SCDB_TYPE_ODBC:
		{
			char *err;
			int result = 0;

			switch_cache_db_test_reactive(sql_manager.dbh, "select call_uuid, read_bit_rate, sent_callee_name from channels", "DROP TABLE channels", create_channels_sql);
			switch_cache_db_test_reactive(sql_manager.dbh, "select * from detailed_calls where sent_callee_name=''", "DROP VIEW detailed_calls", detailed_calls_sql);
			switch_cache_db_test_reactive(sql_manager.dbh, "select * from basic_calls where sent_callee_name=''", "DROP VIEW basic_calls", basic_calls_sql);
			switch_cache_db_test_reactive(sql_manager.dbh, "select call_uuid from calls", "DROP TABLE calls", create_calls_sql);
			if (runtime.odbc_dbtype == DBTYPE_DEFAULT) {
				switch_cache_db_test_reactive(sql_manager.dbh, "delete from registrations where reg_user='' or network_proto='tcp' or network_proto='tls'", 
											  "DROP TABLE registrations", create_registrations_sql);
			} else {
				char *tmp = switch_string_replace(create_registrations_sql, "url      TEXT", "url      VARCHAR(max)");
				switch_cache_db_test_reactive(sql_manager.dbh, "delete from registrations where reg_user='' or network_proto='tcp' or network_proto='tls'", 
											  "DROP TABLE registrations", tmp);
				free(tmp);
			}
			switch_cache_db_test_reactive(sql_manager.dbh, "select ikey from interfaces", "DROP TABLE interfaces", create_interfaces_sql);
			switch_cache_db_test_reactive(sql_manager.dbh, "select hostname from tasks", "DROP TABLE tasks", create_tasks_sql);


			switch(sql_manager.dbh->type) {
			case SCDB_TYPE_CORE_DB:
				{
					switch_cache_db_execute_sql_real(sql_manager.dbh, "BEGIN EXCLUSIVE", &err);
				}
				break;
			case SCDB_TYPE_ODBC:
				{
					switch_odbc_status_t result;
					
					if ((result = switch_odbc_SQLSetAutoCommitAttr(sql_manager.dbh->native_handle.odbc_dbh, 0)) != SWITCH_ODBC_SUCCESS) {
						char tmp[100];
						switch_snprintfv(tmp, sizeof(tmp), "%q-%i", "Unable to Set AutoCommit Off", result);
						err = strdup(tmp);
					}
				}
				break;
			case SCDB_TYPE_PGSQL:
				{
					switch_pgsql_status_t result;
					
					if ((result = switch_pgsql_SQLSetAutoCommitAttr(sql_manager.dbh->native_handle.pgsql_dbh, 0)) != SWITCH_PGSQL_SUCCESS) {
						char tmp[100];
						switch_snprintfv(tmp, sizeof(tmp), "%q-%i", "Unable to Set AutoCommit Off", result);
						err = strdup(tmp);
					}
				}
				break;
			}
			
			switch_cache_db_execute_sql(sql_manager.dbh, "delete from channels where hostname=''", &err);
			if (!err) {
				switch_cache_db_execute_sql(sql_manager.dbh, "delete from channels where hostname=''", &err);

				switch(sql_manager.dbh->type) {
				case SCDB_TYPE_CORE_DB:
					{
						switch_cache_db_execute_sql_real(sql_manager.dbh, "COMMIT", &err);
					}
					break;
				case SCDB_TYPE_ODBC:
					{
						if (switch_odbc_SQLEndTran(sql_manager.dbh->native_handle.odbc_dbh, 1) != SWITCH_ODBC_SUCCESS ||
							switch_odbc_SQLSetAutoCommitAttr(sql_manager.dbh->native_handle.odbc_dbh, 1) != SWITCH_ODBC_SUCCESS) {
							char tmp[100];
							switch_snprintfv(tmp, sizeof(tmp), "%q-%i", "Unable to commit transaction.", result);
							err = strdup(tmp);
						}
					}
					break;
				case SCDB_TYPE_PGSQL:
					{
						if (switch_pgsql_SQLEndTran(sql_manager.dbh->native_handle.pgsql_dbh, 1) != SWITCH_PGSQL_SUCCESS ||
							switch_pgsql_SQLSetAutoCommitAttr(sql_manager.dbh->native_handle.pgsql_dbh, 1) != SWITCH_PGSQL_SUCCESS ||
							switch_pgsql_finish_results(sql_manager.dbh->native_handle.pgsql_dbh) != SWITCH_PGSQL_SUCCESS) {
							char tmp[100];
							switch_snprintfv(tmp, sizeof(tmp), "%q-%i", "Unable to commit transaction.", result);
							err = strdup(tmp);
						}
					}
					break;
				}
			}

			
			if (err) {
				//runtime.odbc_dsn = NULL;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database Error [%s]\n", err);
				//switch_cache_db_release_db_handle(&sql_manager.dbh);
                                if (switch_stristr("read-only", err)) { 
                                        free(err);
                                } else {
                                        free(err);
                                        goto top;
                                }
			}
		}
		break;
	case SCDB_TYPE_CORE_DB:
		{
			switch_cache_db_execute_sql(sql_manager.dbh, create_channels_sql, NULL);
			switch_cache_db_execute_sql(sql_manager.dbh, create_calls_sql, NULL);
			switch_cache_db_execute_sql(sql_manager.dbh, create_interfaces_sql, NULL);
			switch_cache_db_execute_sql(sql_manager.dbh, create_tasks_sql, NULL);
			switch_cache_db_execute_sql(sql_manager.dbh, detailed_calls_sql, NULL);
			switch_cache_db_execute_sql(sql_manager.dbh, basic_calls_sql, NULL);
		}
		break;
	}

	if (switch_test_flag((&runtime), SCF_CLEAR_SQL)) {
		char sql[512] = "";
		char *tables[] = { "complete", "interfaces", NULL };
		int i;
		const char *hostname = switch_core_get_hostname();

		for (i = 0; tables[i]; i++) {
			switch_snprintfv(sql, sizeof(sql), "delete from %q where hostname='%q'", tables[i], hostname);
			switch_cache_db_execute_sql(sql_manager.dbh, sql, NULL);
		}
	}

	switch_cache_db_execute_sql(sql_manager.dbh, "delete from aliases where sticky=0", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "delete from nat where sticky=0", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index alias1 on aliases (alias)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index tasks1 on tasks (hostname,task_id)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index complete1 on complete (a1,hostname)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index complete2 on complete (a2,hostname)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index complete3 on complete (a3,hostname)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index complete4 on complete (a4,hostname)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index complete5 on complete (a5,hostname)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index complete6 on complete (a6,hostname)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index complete7 on complete (a7,hostname)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index complete8 on complete (a8,hostname)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index complete9 on complete (a9,hostname)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index complete10 on complete (a10,hostname)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index complete11 on complete (a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,hostname)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index nat_map_port_proto on nat (port,proto,hostname)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index channels1 on channels(hostname)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index calls1 on calls(hostname)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index chidx1 on channels (hostname)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index uuindex on channels (uuid, hostname)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index uuindex2 on channels (call_uuid)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index callsidx1 on calls (hostname)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index eruuindex on calls (caller_uuid, hostname)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index eeuuindex on calls (callee_uuid)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index eeuuindex2 on calls (call_uuid)", NULL);
	switch_cache_db_execute_sql(sql_manager.dbh, "create index regindex1 on registrations (reg_user,realm,hostname)", NULL);


 skip:

	if (sql_manager.manage) {
#ifdef SWITCH_SQL_BIND_EVERY_EVENT
		switch_event_bind("core_db", SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
#else
		switch_event_bind("core_db", SWITCH_EVENT_ADD_SCHEDULE, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_DEL_SCHEDULE, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_EXE_SCHEDULE, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_RE_SCHEDULE, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_CHANNEL_DESTROY, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_CHANNEL_UUID, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_CHANNEL_CREATE, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_CHANNEL_ANSWER, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_CHANNEL_PROGRESS_MEDIA, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_CHANNEL_HOLD, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_CHANNEL_UNHOLD, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_CHANNEL_EXECUTE, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_CHANNEL_ORIGINATE, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_CALL_UPDATE, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_CHANNEL_CALLSTATE, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_CHANNEL_STATE, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_CHANNEL_BRIDGE, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_CHANNEL_UNBRIDGE, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_SHUTDOWN, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_LOG, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_MODULE_LOAD, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_MODULE_UNLOAD, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_CALL_SECURE, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_NAT, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
		switch_event_bind("core_db", SWITCH_EVENT_CODEC, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL);
#endif	

		switch_threadattr_create(&thd_attr, sql_manager.memory_pool);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_threadattr_priority_set(thd_attr, SWITCH_PRI_REALTIME);
		switch_core_sqldb_start_thread();
		switch_thread_create(&sql_manager.db_thread, thd_attr, switch_core_sql_db_thread, NULL, sql_manager.memory_pool);

	}

	switch_cache_db_release_db_handle(&sql_manager.dbh);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void) switch_core_sqldb_pause(void)
{
	if (sql_manager.paused) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "SQL is already paused.\n");
	}
	sql_manager.paused = 1;
}

SWITCH_DECLARE(void) switch_core_sqldb_resume(void)
{
	if (!sql_manager.paused) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "SQL is already running.\n");
	}
	sql_manager.paused = 0;
}


static void switch_core_sqldb_stop_thread(void)
{
	switch_mutex_lock(sql_manager.ctl_mutex);
	if (sql_manager.manage) {
		if (sql_manager.qm) {
			switch_sql_queue_manager_destroy(&sql_manager.qm);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL is not enabled\n");
	}
	
	switch_mutex_unlock(sql_manager.ctl_mutex);
}

static void switch_core_sqldb_start_thread(void)
{

	switch_mutex_lock(sql_manager.ctl_mutex);
	if (sql_manager.manage) {
		if (!sql_manager.qm) {
			char *dbname = runtime.odbc_dsn;

			if (zstr(dbname)) {
				dbname = runtime.dbname;
				if (zstr(dbname)) {
					dbname = "core";
				}
			}

			switch_sql_queue_manager_init_name("CORE",
											   &sql_manager.qm,
											   4,
											   dbname,
											   SWITCH_MAX_TRANS,
											   runtime.core_db_pre_trans_execute,
											   runtime.core_db_post_trans_execute,
											   runtime.core_db_inner_pre_trans_execute,
											   runtime.core_db_inner_post_trans_execute);

		}
		switch_sql_queue_manager_start(sql_manager.qm);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL is not enabled\n");
	}
	switch_mutex_unlock(sql_manager.ctl_mutex);
}

void switch_core_sqldb_stop(void)
{
	switch_status_t st;

	switch_event_unbind_callback(core_event_handler);

	if (sql_manager.db_thread && sql_manager.db_thread_running) {
		sql_manager.db_thread_running = -1;
		switch_thread_join(&st, sql_manager.db_thread);
	}

	switch_core_sqldb_stop_thread();

	switch_cache_db_flush_handles();
	sql_close(0);
}

SWITCH_DECLARE(void) switch_cache_db_status(switch_stream_handle_t *stream)
{
	/* return some status info suitable for the cli */
	switch_cache_db_handle_t *dbh = NULL;
	switch_bool_t locked = SWITCH_FALSE;
	time_t now = switch_epoch_time_now(NULL);
	char cleankey_str[CACHE_DB_LEN];
	char *pos1 = NULL;
	char *pos2 = NULL;
	int count = 0, used = 0;

	switch_mutex_lock(sql_manager.dbh_mutex);

	for (dbh = sql_manager.handle_pool; dbh; dbh = dbh->next) {
		char *needles[3];
		time_t diff = 0;
		int i = 0;

		needles[0] = "pass=\"";
		needles[1] = "password=";
		needles[2] = "password='";

		diff = now - dbh->last_used;

		if (switch_mutex_trylock(dbh->mutex) == SWITCH_STATUS_SUCCESS) {
			switch_mutex_unlock(dbh->mutex);
			locked = SWITCH_FALSE;
		} else {
			locked = SWITCH_TRUE;
		}

		/* sanitize password */
		memset(cleankey_str, 0, sizeof(cleankey_str));
		for (i = 0; i < 3; i++) {
			if((pos1 = strstr(dbh->name, needles[i]))) {
				pos1 += strlen(needles[i]);

				if (!(pos2 = strstr(pos1, "\""))) {
					if (!(pos2 = strstr(pos1, "'"))) {
						if (!(pos2 = strstr(pos1, " "))) {
							pos2 = pos1 + strlen(pos1);
						}
					}
				}
				strncpy(cleankey_str, dbh->name, pos1 - dbh->name);
				strcpy(&cleankey_str[pos1 - dbh->name], pos2);
				break;
			}
		}
		if (i == 3) {
			strncpy(cleankey_str, dbh->name, strlen(dbh->name));
		}

		count++;
		
		if (dbh->use_count) {
			used++;
		}
		
		stream->write_function(stream, "%s\n\tType: %s\n\tLast used: %d\n\tFlags: %s, %s(%d)\n"
							   "\tCreator: %s\n\tLast User: %s\n",
							   cleankey_str,
							   switch_cache_db_type_name(dbh->type),
							   diff,
							   locked ? "Locked" : "Unlocked",
							   dbh->use_count ? "Attached" : "Detached", dbh->use_count, dbh->creator, dbh->last_user);
	}

	stream->write_function(stream, "%d total. %d in use.\n", count, used);

	switch_mutex_unlock(sql_manager.dbh_mutex);
}

SWITCH_DECLARE(char*)switch_sql_concat(void)
{
	if(runtime.odbc_dbtype == DBTYPE_MSSQL)
		return "+";

	return "||";
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
