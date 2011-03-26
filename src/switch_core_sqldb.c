/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2011, Anthony Minessale II <anthm@freeswitch.org>
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
//*#define DEBUG_SQL 1

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
	switch_cache_db_handle_t *event_db;
	switch_queue_t *sql_queue[2];
	switch_memory_pool_t *memory_pool;
	switch_event_node_t *event_node;
	switch_thread_t *thread;
	switch_thread_t *db_thread;
	int thread_running;
	int db_thread_running;
	switch_bool_t manage;
	switch_mutex_t *io_mutex;
	switch_mutex_t *dbh_mutex;
	switch_cache_db_handle_t *handle_pool;
	switch_thread_cond_t *cond;
	switch_mutex_t *cond_mutex;
	uint32_t total_handles;
	uint32_t total_used_handles;
} sql_manager;


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
		if (dbh_ptr->thread_hash == thread_hash && dbh_ptr->hash == hash &&
			!switch_test_flag(dbh_ptr, CDF_PRUNE) && switch_mutex_trylock(dbh_ptr->mutex) == SWITCH_STATUS_SUCCESS) {
			r = dbh_ptr;
		}
	}
			
	if (!r) {
		for (dbh_ptr = sql_manager.handle_pool; dbh_ptr; dbh_ptr = dbh_ptr->next) {
			if (dbh_ptr->hash == hash && !dbh_ptr->use_count && !switch_test_flag(dbh_ptr, CDF_PRUNE) && 
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


#define SWITCH_CORE_DB "core"
/*!
  \brief Open the default system database
*/
SWITCH_DECLARE(switch_status_t) _switch_core_db_handle(switch_cache_db_handle_t **dbh, const char *file, const char *func, int line)
{
	switch_cache_db_connection_options_t options = { {0} };
	switch_status_t r;
	
	if (!sql_manager.manage) {
		return SWITCH_STATUS_FALSE;
	}

	if (!zstr(runtime.odbc_dsn)) {
		options.odbc_options.dsn = runtime.odbc_dsn;
		options.odbc_options.user = runtime.odbc_user;
		options.odbc_options.pass = runtime.odbc_pass;

		r = _switch_cache_db_get_db_handle(dbh, SCDB_TYPE_ODBC, &options, file, func, line);
	} else {
		if (runtime.dbname) {
			options.core_db_options.db_path = runtime.dbname;
		} else {
			options.core_db_options.db_path = SWITCH_CORE_DB;
		}
		r = _switch_cache_db_get_db_handle(dbh, SCDB_TYPE_CORE_DB, &options, file, func, line);
	}

	if (r == SWITCH_STATUS_SUCCESS && !(*dbh)->io_mutex) {
		(*dbh)->io_mutex = sql_manager.io_mutex;
	}

	return r;
}


#define SQL_CACHE_TIMEOUT 120
#define SQL_REG_TIMEOUT 15


static void sql_close(time_t prune)
{
	switch_cache_db_handle_t *dbh = NULL;
	int locked = 0;

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
				locked++;
			}
			continue;
		}
		
	}

	if (locked) {
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
		switch_mutex_lock(sql_manager.dbh_mutex);
		(*dbh)->last_used = switch_epoch_time_now(NULL);

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
	const char *db_user = NULL;
	const char *db_pass = NULL;

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
	case SCDB_TYPE_ODBC:
		{
			db_name = connection_options->odbc_options.dsn;
			db_user = connection_options->odbc_options.user;
			db_pass = connection_options->odbc_options.pass;
		}
		break;
	case SCDB_TYPE_CORE_DB:
		{
			db_name = connection_options->core_db_options.db_path;
			db_user = "";
			db_pass = "";
		}
		break;
	}

	if (!db_name) {
		return SWITCH_STATUS_FALSE;
	}


	snprintf(db_str, sizeof(db_str) - 1, "db=\"%s\";user=\"%s\";pass=\"%s\"", db_name, db_user, db_pass);
	snprintf(db_callsite_str, sizeof(db_callsite_str) - 1, "%s:%d", file, line);
	snprintf(thread_str, sizeof(thread_str) - 1, "thread=\"%lu\"",  (unsigned long) (intptr_t) self); 

	if ((new_dbh = get_handle(db_str, db_callsite_str, thread_str))) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_DEBUG10,
						  "Reuse Unused Cached DB handle %s [%s]\n", new_dbh->name, switch_cache_db_type_name(new_dbh->type));
	} else {
		switch_core_db_t *db = NULL;
		switch_odbc_handle_t *odbc_dbh = NULL;

		switch (type) {
		case SCDB_TYPE_ODBC:
			{

				if (!switch_odbc_available()) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failure! ODBC NOT AVAILABLE!\n");
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

		if (!db && !odbc_dbh) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failure!\n");
			goto end;
		}

		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_DEBUG10,
						  "Create Cached DB handle %s [%s] %s:%d\n", new_dbh->name, switch_cache_db_type_name(type), file, line);

		new_dbh = create_handle(type);

		if (db) {
			new_dbh->native_handle.core_db_dbh = db;
		} else {
			new_dbh->native_handle.odbc_dbh = odbc_dbh;
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
	switch_mutex_t *io_mutex = dbh->io_mutex;
	
	if (io_mutex) switch_mutex_lock(io_mutex);

	if (err) {
		*err = NULL;
	}

	switch (dbh->type) {
	case SCDB_TYPE_ODBC:
		{
			status = switch_odbc_handle_exec(dbh->native_handle.odbc_dbh, sql, NULL, &errmsg);
		}
		break;
	case SCDB_TYPE_CORE_DB:
		{
			status = switch_core_db_exec(dbh->native_handle.core_db_dbh, sql, NULL, NULL, &errmsg);
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
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR [%s]\n%s\n", errmsg, sql);
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

static void wake_thread(int force)
{
	if (force) {
		switch_thread_cond_signal(sql_manager.cond);
		return;
	}

	if (switch_mutex_trylock(sql_manager.cond_mutex) == SWITCH_STATUS_SUCCESS) {
		switch_thread_cond_signal(sql_manager.cond);
		switch_mutex_unlock(sql_manager.cond_mutex);
	}
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


SWITCH_DECLARE(switch_status_t) switch_cache_db_persistant_execute_trans(switch_cache_db_handle_t *dbh, char *sql, uint32_t retries)
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

 again:

	while (begin_retries > 0) {
		again = 0;

		if (runtime.odbc_dbtype == DBTYPE_DEFAULT) {
			switch_cache_db_execute_sql_real(dbh, "BEGIN", &errmsg);
		} else {
			switch_odbc_status_t result;

			if ((result = switch_odbc_SQLSetAutoCommitAttr(dbh->native_handle.odbc_dbh, 0)) != SWITCH_ODBC_SUCCESS) {
				char tmp[100];
				switch_snprintf(tmp, sizeof(tmp), "%s-%i", "Unable to Set AutoCommit Off", result);
				errmsg = strdup(tmp);
			}
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
				if (runtime.odbc_dbtype == DBTYPE_DEFAULT) {
					switch_cache_db_execute_sql_real(dbh, "COMMIT", NULL);
				} else  {
					switch_odbc_SQLEndTran(dbh->native_handle.odbc_dbh, 1);
					switch_odbc_SQLSetAutoCommitAttr(dbh->native_handle.odbc_dbh, 1);
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

 done:

	if (runtime.odbc_dbtype == DBTYPE_DEFAULT) {
		switch_cache_db_execute_sql_real(dbh, "COMMIT", NULL);
	} else {
		switch_odbc_SQLEndTran(dbh->native_handle.odbc_dbh, 1);
		switch_odbc_SQLSetAutoCommitAttr(dbh->native_handle.odbc_dbh, 1);
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
	case SCDB_TYPE_ODBC:
		{
			status = switch_odbc_handle_callback_exec(dbh->native_handle.odbc_dbh, sql, callback, pdata, err);
		}
		break;
	case SCDB_TYPE_CORE_DB:
		{
			status = switch_core_db_exec(dbh->native_handle.core_db_dbh, sql, callback, pdata, &errmsg);

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

	if (!switch_test_flag((&runtime), SCF_AUTO_SCHEMAS)) {
		switch_cache_db_execute_sql(dbh, (char *)test_sql, NULL);
		return SWITCH_TRUE;
	}

	if (io_mutex) switch_mutex_lock(io_mutex);

	switch (dbh->type) {
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
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SQL ERR [%s]\n[%s]\n", errmsg, reactive_sql);
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
			wake_thread(0);
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

static void *SWITCH_THREAD_FUNC switch_core_sql_thread(switch_thread_t *thread, void *obj)
{
	void *pop = NULL;
	uint32_t iterations = 0;
	uint8_t trans = 0;
	uint32_t target = 20000;
	switch_size_t len = 0, sql_len = runtime.sql_buffer_len;
	char *tmp, *sqlbuf = (char *) malloc(sql_len);
	char *sql = NULL, *save_sql = NULL;
	switch_size_t newlen;
	int lc = 0, wrote = 0, do_sleep = 1;
	uint32_t sanity = 120;
	
	switch_assert(sqlbuf);

	while (!sql_manager.event_db) {
		if (switch_core_db_handle(&sql_manager.event_db) == SWITCH_STATUS_SUCCESS && sql_manager.event_db)
			break;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Error getting core db, Retrying\n");
		switch_yield(500000);
		sanity--;
	}

	if (!sql_manager.event_db) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error getting core db Disabling core sql functionality\n");
		return NULL;
	}

	sql_manager.thread_running = 1;

	switch_mutex_lock(sql_manager.cond_mutex);

	while (sql_manager.thread_running == 1) {
		if (save_sql || switch_queue_trypop(sql_manager.sql_queue[0], &pop) == SWITCH_STATUS_SUCCESS ||
			switch_queue_trypop(sql_manager.sql_queue[1], &pop) == SWITCH_STATUS_SUCCESS) {

			if (save_sql) {
				sql = save_sql;
				save_sql = NULL;
			} else if ((sql = (char *) pop)) {
				pop = NULL;
			}
			
			if (sql) {
				newlen = strlen(sql) + 2;

				if (iterations == 0) {
					trans = 1;
				}

				if (len + newlen > sql_len) {
					int new_mlen = len + newlen + 10240;
					
					if (new_mlen < runtime.max_sql_buffer_len) {
						sql_len = new_mlen;
#ifdef DEBUG_SQL
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, 
										  "REALLOC %ld %d %d\n", (long int)sql_len, switch_queue_size(sql_manager.sql_queue[0]), 
										  switch_queue_size(sql_manager.sql_queue[1]));
#endif
						if (!(tmp = realloc(sqlbuf, sql_len))) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "SQL thread ending on mem err\n");
							abort();
							break;
						}
						sqlbuf = tmp;
					} else {
#ifdef DEBUG_SQL
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, 
										  "SAVE %d %d\n", switch_queue_size(sql_manager.sql_queue[0]), switch_queue_size(sql_manager.sql_queue[1]));
#endif
						save_sql = sql;
						sql = NULL;
						lc = 0;
						goto skip;
					}
				}

				iterations++;				
				sprintf(sqlbuf + len, "%s;\n", sql);
				len += newlen;
				free(sql);
				sql = NULL;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "SQL thread ending\n");
				break;
			}
		}

		lc = switch_queue_size(sql_manager.sql_queue[0]) + switch_queue_size(sql_manager.sql_queue[1]);

	skip:
		
		wrote = 0;

		if (trans && iterations && (iterations > target || !lc)) {
#ifdef DEBUG_SQL
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, 
							  "RUN %d %d %d\n", switch_queue_size(sql_manager.sql_queue[0]), switch_queue_size(sql_manager.sql_queue[1]), iterations);
#endif
			if (switch_cache_db_persistant_execute_trans(sql_manager.event_db, sqlbuf, 1) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "SQL thread unable to commit transaction, records lost!\n");
			}
#ifdef DEBUG_SQL
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "DONE\n");
#endif
			iterations = 0;
			trans = 0;
			len = 0;
			*sqlbuf = '\0';
			lc = 0;
			if (do_sleep) {
				switch_yield(200000);
			}
			wrote = 1;
		}

		lc = switch_queue_size(sql_manager.sql_queue[0]) + switch_queue_size(sql_manager.sql_queue[1]);
		
		if (!lc) {
			switch_thread_cond_wait(sql_manager.cond, sql_manager.cond_mutex);
		} else if (wrote) {
			if (lc > 2000) {
				do_sleep = 0;
			} else {
				do_sleep = 1;
			}
		}
		
		
	}

	switch_mutex_unlock(sql_manager.cond_mutex);

	while (switch_queue_trypop(sql_manager.sql_queue[0], &pop) == SWITCH_STATUS_SUCCESS) {
		free(pop);
	}

	while (switch_queue_trypop(sql_manager.sql_queue[1], &pop) == SWITCH_STATUS_SUCCESS) {
		free(pop);
	}

	free(sqlbuf);

	sql_manager.thread_running = 0;

	switch_cache_db_release_db_handle(&sql_manager.event_db);

	return NULL;
}

static char *parse_presence_data_cols(switch_event_t *event)
{
	char *cols[25] = { 0 };
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
		switch_snprintf(col_name, sizeof(col_name), "variable_%s", cols[i]);
		stream.write_function(&stream, "%q='%q',", cols[i], switch_event_get_header_nil(event, col_name));
	}

	r = (char *) stream.data;

	if (end_of(r) == ',') {
		end_of(r) = '\0';
	}

	switch_safe_free(data_copy);
	
	return r;
	
}


#define MAX_SQL 5
#define new_sql() switch_assert(sql_idx+1 < MAX_SQL); sql[sql_idx++]

static void core_event_handler(switch_event_t *event)
{
	char *sql[MAX_SQL] = { 0 };
	int sql_idx = 0;
	char *extra_cols;

	switch_assert(event);

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
				new_sql() = switch_mprintf("delete from channels where uuid='%q' and hostname='%q'",
										   uuid, switch_core_get_hostname());

				new_sql() = switch_mprintf("delete from calls where (caller_uuid='%q' or callee_uuid='%q') and hostname='%q'",
										   uuid, uuid, switch_core_get_hostname());

			}
		}
		break;
	case SWITCH_EVENT_CHANNEL_UUID:
		{
			new_sql() = switch_mprintf("update channels set uuid='%q' where uuid='%q' and hostname='%q';"
									   "update calls set caller_uuid='%q' where caller_uuid='%q' and hostname='%q';"
									   "update calls set callee_uuid='%q' where callee_uuid='%q' and hostname='%q'",
									   switch_event_get_header_nil(event, "unique-id"),
									   switch_event_get_header_nil(event, "old-unique-id"),
									   switch_core_get_hostname(),
									   switch_event_get_header_nil(event, "unique-id"),
									   switch_event_get_header_nil(event, "old-unique-id"),
									   switch_core_get_hostname(),
									   switch_event_get_header_nil(event, "unique-id"),
									   switch_event_get_header_nil(event, "old-unique-id"), switch_core_get_hostname()
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
								   switch_event_get_header_nil(event, "caller-context"), switch_core_get_hostname()
								   );
		break;
	case SWITCH_EVENT_CODEC:
		new_sql() =
			switch_mprintf
			("update channels set read_codec='%q',read_rate='%q',read_bit_rate='%q',write_codec='%q',write_rate='%q',write_bit_rate='%q' where uuid='%q' and hostname='%q'",
			 switch_event_get_header_nil(event, "channel-read-codec-name"),
			 switch_event_get_header_nil(event, "channel-read-codec-rate"),
			 switch_event_get_header_nil(event, "channel-read-codec-bit-rate"),
			 switch_event_get_header_nil(event, "channel-write-codec-name"),
			 switch_event_get_header_nil(event, "channel-write-codec-rate"),
			 switch_event_get_header_nil(event, "channel-write-codec-bit-rate"),
			 switch_event_get_header_nil(event, "unique-id"), switch_core_get_hostname());
		break;
	case SWITCH_EVENT_CHANNEL_HOLD:
	case SWITCH_EVENT_CHANNEL_UNHOLD:
	case SWITCH_EVENT_CHANNEL_EXECUTE: {
		
		new_sql() = switch_mprintf("update channels set application='%q',application_data='%q',"
								   "presence_id='%q',presence_data='%q' where uuid='%q' and hostname='%q'",
								   switch_event_get_header_nil(event, "application"),
								   switch_event_get_header_nil(event, "application-data"),
								   switch_event_get_header_nil(event, "channel-presence-id"),
								   switch_event_get_header_nil(event, "channel-presence-data"),
								   switch_event_get_header_nil(event, "unique-id"), switch_core_get_hostname()
								   );

	}
		break;

	case SWITCH_EVENT_CHANNEL_ORIGINATE:
		{
			if ((extra_cols = parse_presence_data_cols(event))) {
				new_sql() = switch_mprintf("update channels set "
										   "presence_id='%q',presence_data='%q', call_uuid='%q',%s where uuid='%q' and hostname='%q'",
										   switch_event_get_header_nil(event, "channel-presence-id"),
										   switch_event_get_header_nil(event, "channel-presence-data"),
										   switch_event_get_header_nil(event, "channel-call-uuid"),
										   extra_cols,
										   switch_event_get_header_nil(event, "unique-id"), switch_core_get_hostname());
				free(extra_cols);
			} else {
				new_sql() = switch_mprintf("update channels set "
										   "presence_id='%q',presence_data='%q', call_uuid='%q' where uuid='%q' and hostname='%q'",
										   switch_event_get_header_nil(event, "channel-presence-id"),
										   switch_event_get_header_nil(event, "channel-presence-data"),
										   switch_event_get_header_nil(event, "channel-call-uuid"),
										   switch_event_get_header_nil(event, "unique-id"), switch_core_get_hostname());
			}

		}

		break;
	case SWITCH_EVENT_CALL_UPDATE:
		{
			const char *name = NULL, *number = NULL, *direction;
			int recv = 0;

			direction = switch_event_get_header(event, "direction");

			if (direction && strcasecmp(direction, "send")) {
				recv = 1;
				name = switch_event_get_header(event, "callee-name");
				number = switch_event_get_header(event, "callee-number");
			}
			
			if (!name) {
				name = switch_event_get_header(event, "caller-callee-id-name");
			}

			if (!number) {
				number = switch_event_get_header(event, "caller-callee-id-number");
			}

			if (!zstr(name) && !zstr(number)) {
				new_sql() = switch_mprintf("update channels set state='%s',callstate='%s',callee_name='%q',"
										   "callee_num='%q',callee_direction='%q' where uuid='%s' and hostname='%q'",
										   switch_event_get_header_nil(event, "channel-state"),
										   switch_event_get_header_nil(event, "channel-call-state"),
										   switch_str_nil(name),
										   switch_str_nil(number),
										   switch_event_get_header_nil(event, "direction"),
										   switch_event_get_header_nil(event, "unique-id"), switch_core_get_hostname());

				name = switch_event_get_header(event, "callee-name");
				number = switch_event_get_header(event, "callee-number");

				if (name && number && recv) {
					new_sql() = switch_mprintf("update calls set callee_cid_name='%q',callee_cid_num='%q' where caller_uuid='%q'",
											   name, number, switch_event_get_header_nil(event, "unique-id"));

				}
			}
		}
		break;
	case SWITCH_EVENT_CHANNEL_CALLSTATE:
		{
			new_sql() = switch_mprintf("update channels set callstate='%q' where uuid='%q' and hostname='%q'",
									   switch_event_get_header_nil(event, "channel-call-state"),
									   switch_event_get_header_nil(event, "unique-id"), switch_core_get_hostname());

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
			case CS_HANGUP:
			case CS_DESTROY:
				break;
			case CS_ROUTING:
				if ((extra_cols = parse_presence_data_cols(event))) {
					new_sql() = switch_mprintf("update channels set state='%s',cid_name='%q',cid_num='%q',"
											   "ip_addr='%s',dest='%q',dialplan='%q',context='%q',presence_id='%q',presence_data='%q',%s "
											   "where uuid='%s' and hostname='%q'",
											   switch_event_get_header_nil(event, "channel-state"),
											   switch_event_get_header_nil(event, "caller-caller-id-name"),
											   switch_event_get_header_nil(event, "caller-caller-id-number"),
											   switch_event_get_header_nil(event, "caller-network-addr"),
											   switch_event_get_header_nil(event, "caller-destination-number"),
											   switch_event_get_header_nil(event, "caller-dialplan"),
											   switch_event_get_header_nil(event, "caller-context"),
											   switch_event_get_header_nil(event, "channel-presence-id"),
											   switch_event_get_header_nil(event, "channel-presence-data"),
											   extra_cols,
											   switch_event_get_header_nil(event, "unique-id"), switch_core_get_hostname());
					free(extra_cols);
				} else {
					new_sql() = switch_mprintf("update channels set state='%s',cid_name='%q',cid_num='%q',"
											   "ip_addr='%s',dest='%q',dialplan='%q',context='%q',presence_id='%q',presence_data='%q' "
											   "where uuid='%s' and hostname='%q'",
											   switch_event_get_header_nil(event, "channel-state"),
											   switch_event_get_header_nil(event, "caller-caller-id-name"),
											   switch_event_get_header_nil(event, "caller-caller-id-number"),
											   switch_event_get_header_nil(event, "caller-network-addr"),
											   switch_event_get_header_nil(event, "caller-destination-number"),
											   switch_event_get_header_nil(event, "caller-dialplan"),
											   switch_event_get_header_nil(event, "caller-context"),
											   switch_event_get_header_nil(event, "channel-presence-id"),
											   switch_event_get_header_nil(event, "channel-presence-data"),
											   switch_event_get_header_nil(event, "unique-id"), switch_core_get_hostname());
				}
				break;
			default:
				new_sql() = switch_mprintf("update channels set state='%s' where uuid='%s' and hostname='%q'",
										   switch_event_get_header_nil(event, "channel-state"),
										   switch_event_get_header_nil(event, "unique-id"), switch_core_get_hostname());
				break;
			}

			break;


		}
	case SWITCH_EVENT_CHANNEL_BRIDGE:
		{
			const char *callee_cid_name, *callee_cid_num, *direction;
			char *func_name;

			direction = switch_event_get_header(event, "other-leg-direction");

			if (direction && !strcasecmp(direction, "outbound")) {
				callee_cid_name = switch_event_get_header_nil(event, "Other-Leg-callee-id-name");
				callee_cid_num = switch_event_get_header_nil(event, "Other-Leg-callee-id-number");
			} else {
				callee_cid_name = switch_event_get_header_nil(event, "Other-Leg-caller-id-name");
				callee_cid_num = switch_event_get_header_nil(event, "Other-Leg-caller-id-number");
			}


			new_sql() = switch_mprintf("update channels set call_uuid='%q' where uuid='%s' and hostname='%q'",
									   switch_event_get_header_nil(event, "channel-call-uuid"),
									   switch_event_get_header_nil(event, "unique-id"), switch_core_get_hostname());

			if (runtime.odbc_dbtype == DBTYPE_DEFAULT) {
				func_name = "function";
			}
			else {
				func_name = "call_function";
			}

			new_sql() = switch_mprintf("insert into calls (call_uuid,call_created,call_created_epoch,%s,caller_cid_name,"
									   "caller_cid_num,caller_dest_num,caller_chan_name,caller_uuid,callee_cid_name,"
									   "callee_cid_num,callee_dest_num,callee_chan_name,callee_uuid,hostname) "
									   "values ('%s', '%s', '%ld', '%s','%q','%q','%q','%q','%s','%q','%q','%q','%q','%s','%q')",
									   func_name,
									   switch_event_get_header_nil(event, "channel-call-uuid"),
									   switch_event_get_header_nil(event, "event-date-local"),
									   (long) switch_epoch_time_now(NULL),
									   switch_event_get_header_nil(event, "event-calling-function"),
									   switch_event_get_header_nil(event, "caller-caller-id-name"),
									   switch_event_get_header_nil(event, "caller-caller-id-number"),
									   switch_event_get_header_nil(event, "caller-destination-number"),
									   switch_event_get_header_nil(event, "caller-channel-name"),
									   switch_event_get_header_nil(event, "caller-unique-id"),
									   callee_cid_name,
									   callee_cid_num,
									   switch_event_get_header_nil(event, "Other-Leg-destination-number"),
									   switch_event_get_header_nil(event, "Other-Leg-channel-name"),
									   switch_event_get_header_nil(event, "Other-Leg-unique-id"), switch_core_get_hostname()
									   );
		}
		break;
	case SWITCH_EVENT_CHANNEL_UNBRIDGE:
		{
			char *uuid = switch_event_get_header_nil(event, "caller-unique-id");

			new_sql() = switch_mprintf("delete from calls where (caller_uuid='%q' or callee_uuid='%q') and hostname='%q'",
									   uuid, uuid, switch_core_get_hostname());
			break;
		}
	case SWITCH_EVENT_SHUTDOWN:
		new_sql() = switch_mprintf("delete from channels where hostname='%q';"
								   "delete from interfaces where hostname='%q';"
								   "delete from calls where hostname='%q'",
								   switch_core_get_hostname(), switch_core_get_hostname(), switch_core_get_hostname()
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
			new_sql() = switch_mprintf("update channels set secure='%s' where uuid='%s' and hostname='%q'",
									   type, switch_event_get_header_nil(event, "caller-unique-id"), switch_core_get_hostname()
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
				switch_queue_push(sql_manager.sql_queue[1], sql[i]);
			} else {
				switch_queue_push(sql_manager.sql_queue[0], sql[i]);
			}
			sql[i] = NULL;
			wake_thread(0);
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
	"   call_uuid  VARCHAR(256)\n"
	");\n"
	"create index uuindex on channels (uuid,hostname);\n"
	"create index uuindex2 on channels (call_uuid,hostname);\n";

static char create_calls_sql[] =
	"CREATE TABLE calls (\n"
	"   call_uuid  VARCHAR(255),\n"
	"   call_created  VARCHAR(128),\n"
	"   call_created_epoch  INTEGER,\n"
	"   function  VARCHAR(1024),\n"
	"   caller_cid_name  VARCHAR(1024),\n"
	"   caller_cid_num   VARCHAR(256),\n"
	"   caller_dest_num  VARCHAR(256),\n"
	"   caller_chan_name VARCHAR(1024),\n"
	"   caller_uuid      VARCHAR(256),\n"
	"   callee_cid_name  VARCHAR(1024),\n"
	"   callee_cid_num   VARCHAR(256),\n"
	"   callee_dest_num  VARCHAR(256),\n"
	"   callee_chan_name VARCHAR(1024),\n"
	"   callee_uuid      VARCHAR(256),\n"
	"   hostname VARCHAR(256)\n"
	");\n"
	"create index eruuindex on calls (caller_uuid,hostname);\n"
	"create index eeuuindex on calls (callee_uuid,hostname);\n"
	"create index eeuuindex2 on calls (call_uuid,hostname);\n";

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
	"   url      TEXT,\n"
	"   expires  INTEGER,\n"
	"   network_ip VARCHAR(256),\n"
	"   network_port VARCHAR(256),\n"
	"   network_proto VARCHAR(256),\n"
	"   hostname VARCHAR(256)\n"
	");\n"
	"create index regindex1 on registrations (reg_user,realm,hostname);\n";
	

SWITCH_DECLARE(switch_status_t) switch_core_add_registration(const char *user, const char *realm, const char *token, const char *url, uint32_t expires, 
															 const char *network_ip, const char *network_port, const char *network_proto)
{
	switch_cache_db_handle_t *dbh;
	char *sql;

	if (!switch_test_flag((&runtime), SCF_USE_SQL)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_core_db_handle(&dbh) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB!\n");
		return SWITCH_STATUS_FALSE;
	}

	if (runtime.multiple_registrations) {
		sql = switch_mprintf("delete from registrations where hostname='%q' and (url='%q' or token='%q')", 
							 switch_core_get_hostname(), url, switch_str_nil(token));
	} else {
		sql = switch_mprintf("delete from registrations where reg_user='%q' and realm='%q' and hostname='%q'", 
							 user, realm, switch_core_get_hostname());
	}

	switch_cache_db_execute_sql(dbh, sql, NULL);
	free(sql);

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
						 switch_core_get_hostname()
						 );
	
	switch_cache_db_execute_sql(dbh, sql, NULL);
	switch_cache_db_release_db_handle(&dbh);

	free(sql);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_del_registration(const char *user, const char *realm, const char *token)
{

	switch_cache_db_handle_t *dbh;
	char *sql;

	if (!switch_test_flag((&runtime), SCF_USE_SQL)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_core_db_handle(&dbh) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB!\n");
		return SWITCH_STATUS_FALSE;
	}

	if (!zstr(token) && runtime.multiple_registrations) {
		sql = switch_mprintf("delete from registrations where reg_user='%q' and realm='%q' and hostname='%q' and token='%q'", user, realm, switch_core_get_hostname(), token);
	} else {
		sql = switch_mprintf("delete from registrations where reg_user='%q' and realm='%q' and hostname='%q'", user, realm, switch_core_get_hostname());
	}

	switch_cache_db_execute_sql(dbh, sql, NULL);
	switch_cache_db_release_db_handle(&dbh);

	free(sql);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_expire_registration(int force)
{
	
	switch_cache_db_handle_t *dbh;
	char *sql;
	time_t now;

	if (!switch_test_flag((&runtime), SCF_USE_SQL)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_core_db_handle(&dbh) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB!\n");
		return SWITCH_STATUS_FALSE;
	}

	now = switch_epoch_time_now(NULL);

	if (force) {
		sql = switch_mprintf("delete from registrations where hostname='%q'", switch_core_get_hostname());
	} else {
		sql = switch_mprintf("delete from registrations where expires > 0 and expires <= %ld and hostname='%q'", now, switch_core_get_hostname());
	}

	switch_cache_db_execute_sql(dbh, sql, NULL);
	switch_cache_db_release_db_handle(&dbh);

	free(sql);

	return SWITCH_STATUS_SUCCESS;

}

switch_status_t switch_core_sqldb_start(switch_memory_pool_t *pool, switch_bool_t manage)
{
	switch_threadattr_t *thd_attr;
	switch_cache_db_handle_t *dbh;
	uint32_t sanity = 400;

	sql_manager.memory_pool = pool;
	sql_manager.manage = manage;

	switch_mutex_init(&sql_manager.dbh_mutex, SWITCH_MUTEX_NESTED, sql_manager.memory_pool);
	switch_mutex_init(&sql_manager.io_mutex, SWITCH_MUTEX_NESTED, sql_manager.memory_pool);
	switch_mutex_init(&sql_manager.cond_mutex, SWITCH_MUTEX_NESTED, sql_manager.memory_pool);

	switch_thread_cond_create(&sql_manager.cond, sql_manager.memory_pool);

 top:

	if (!sql_manager.manage) goto skip;

	/* Activate SQL database */
	if (switch_core_db_handle(&dbh) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB!\n");

		if (runtime.odbc_dsn) {
			runtime.odbc_dsn = NULL;
			runtime.odbc_user = NULL;
			runtime.odbc_pass = NULL;
			runtime.odbc_dbtype = DBTYPE_DEFAULT;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Falling back to core_db.\n");
			goto top;
		}


		switch_clear_flag((&runtime), SCF_USE_SQL);
		return SWITCH_STATUS_FALSE;
	}


	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Opening DB\n");

	switch (dbh->type) {
	case SCDB_TYPE_ODBC:
		{
			char sql[512] = "";
			char *tables[] = { "channels", "calls", "interfaces", "tasks", NULL };
			int i;
			const char *hostname = switch_core_get_hostname();

			for (i = 0; tables[i]; i++) {
				switch_snprintf(sql, sizeof(sql), "delete from %s where hostname='%s'", tables[i], hostname);
				switch_cache_db_execute_sql(dbh, sql, NULL);
			}
		}
		break;
	case SCDB_TYPE_CORE_DB:
		{
			switch_cache_db_execute_sql(dbh, "drop table channels", NULL);
			switch_cache_db_execute_sql(dbh, "drop table calls", NULL);
			switch_cache_db_execute_sql(dbh, "drop table interfaces", NULL);
			switch_cache_db_execute_sql(dbh, "drop table tasks", NULL);
			switch_cache_db_execute_sql(dbh, "PRAGMA synchronous=OFF;", NULL);
			switch_cache_db_execute_sql(dbh, "PRAGMA count_changes=OFF;", NULL);
			switch_cache_db_execute_sql(dbh, "PRAGMA cache_size=8000", NULL);
			switch_cache_db_execute_sql(dbh, "PRAGMA temp_store=MEMORY;", NULL);
		}
		break;
	}


	switch_cache_db_test_reactive(dbh, "select hostname from complete", "DROP TABLE complete", create_complete_sql);
	switch_cache_db_test_reactive(dbh, "select hostname from aliases", "DROP TABLE aliases", create_alias_sql);
	switch_cache_db_test_reactive(dbh, "select hostname from nat", "DROP TABLE nat", create_nat_sql);
	switch_cache_db_test_reactive(dbh, "delete from registrations where reg_user='' or network_proto='tcp' or network_proto='tls'", 
								  "DROP TABLE registrations", create_registrations_sql);


	switch (dbh->type) {
	case SCDB_TYPE_ODBC:
		{
			char *err;
			switch_cache_db_test_reactive(dbh, "select call_uuid, read_bit_rate from channels", "DROP TABLE channels", create_channels_sql);
			if (runtime.odbc_dbtype == DBTYPE_DEFAULT) {
				switch_cache_db_test_reactive(dbh, "select call_uuid from calls", "DROP TABLE calls", create_calls_sql);
			} else {
				char *tmp = switch_string_replace(create_calls_sql, "function", "call_function");
				switch_cache_db_test_reactive(dbh, "select call_uuid from calls", "DROP TABLE calls", tmp);
				free(tmp);
			}
			switch_cache_db_test_reactive(dbh, "select ikey from interfaces", "DROP TABLE interfaces", create_interfaces_sql);
			switch_cache_db_test_reactive(dbh, "select hostname from tasks", "DROP TABLE tasks", create_tasks_sql);
			switch_cache_db_test_reactive(dbh, "delete from registrations where reg_user='' or network_proto='tcp' or network_proto='tls'", 
										  "DROP TABLE registrations", create_registrations_sql);

			if (runtime.odbc_dbtype == DBTYPE_DEFAULT) {
				switch_cache_db_execute_sql(dbh, "begin;delete from channels where hostname='';delete from channels where hostname='';commit;", &err);
			} else {
				switch_cache_db_execute_sql(dbh, "delete from channels where hostname='';delete from channels where hostname='';", &err);
			}

			if (err) {
				runtime.odbc_dsn = NULL;
				runtime.odbc_user = NULL;
				runtime.odbc_pass = NULL;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Transactions not supported on your DB, disabling ODBC\n");
				switch_cache_db_release_db_handle(&dbh);
				free(err);
				goto top;
			}
		}
		break;
	case SCDB_TYPE_CORE_DB:
		{
			switch_cache_db_execute_sql(dbh, create_channels_sql, NULL);
			switch_cache_db_execute_sql(dbh, create_calls_sql, NULL);
			switch_cache_db_execute_sql(dbh, create_interfaces_sql, NULL);
			switch_cache_db_execute_sql(dbh, create_tasks_sql, NULL);
		}
		break;
	}


	switch_cache_db_execute_sql(dbh, "delete from complete where sticky=0", NULL);
	switch_cache_db_execute_sql(dbh, "delete from aliases where sticky=0", NULL);
	switch_cache_db_execute_sql(dbh, "delete from nat where sticky=0", NULL);
	switch_cache_db_execute_sql(dbh, "create index alias1 on aliases (alias)", NULL);
	switch_cache_db_execute_sql(dbh, "create index tasks1 on tasks (hostname,task_id)", NULL);
	switch_cache_db_execute_sql(dbh, "create index complete1 on complete (a1,hostname)", NULL);
	switch_cache_db_execute_sql(dbh, "create index complete2 on complete (a2,hostname)", NULL);
	switch_cache_db_execute_sql(dbh, "create index complete3 on complete (a3,hostname)", NULL);
	switch_cache_db_execute_sql(dbh, "create index complete4 on complete (a4,hostname)", NULL);
	switch_cache_db_execute_sql(dbh, "create index complete5 on complete (a5,hostname)", NULL);
	switch_cache_db_execute_sql(dbh, "create index complete6 on complete (a6,hostname)", NULL);
	switch_cache_db_execute_sql(dbh, "create index complete7 on complete (a7,hostname)", NULL);
	switch_cache_db_execute_sql(dbh, "create index complete8 on complete (a8,hostname)", NULL);
	switch_cache_db_execute_sql(dbh, "create index complete9 on complete (a9,hostname)", NULL);
	switch_cache_db_execute_sql(dbh, "create index complete10 on complete (a10,hostname)", NULL);
	switch_cache_db_execute_sql(dbh, "create index complete11 on complete (a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,hostname)", NULL);
	switch_cache_db_execute_sql(dbh, "create index nat_map_port_proto on nat (port,proto,hostname)", NULL);
	switch_cache_db_execute_sql(dbh, "create index channels1 on channels(hostname)", NULL);
	switch_cache_db_execute_sql(dbh, "create index calls1 on calls(hostname)", NULL);

 skip:

	if (sql_manager.manage) {
		if (switch_event_bind_removable("core_db", SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY,
										core_event_handler, NULL, &sql_manager.event_node) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind event handler!\n");
		}

		switch_queue_create(&sql_manager.sql_queue[0], SWITCH_SQL_QUEUE_LEN, sql_manager.memory_pool);
		switch_queue_create(&sql_manager.sql_queue[1], SWITCH_SQL_QUEUE_LEN, sql_manager.memory_pool);
	}

	switch_threadattr_create(&thd_attr, sql_manager.memory_pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	if (sql_manager.manage) {
		switch_thread_create(&sql_manager.thread, thd_attr, switch_core_sql_thread, NULL, sql_manager.memory_pool);
	}
	switch_thread_create(&sql_manager.db_thread, thd_attr, switch_core_sql_db_thread, NULL, sql_manager.memory_pool);

	while (sql_manager.manage && !sql_manager.thread_running && --sanity) {
		switch_yield(10000);
	}

	if (sql_manager.manage) switch_cache_db_release_db_handle(&dbh);

	return SWITCH_STATUS_SUCCESS;
}

void switch_core_sqldb_stop(void)
{
	switch_status_t st;

	switch_event_unbind(&sql_manager.event_node);

	if (sql_manager.thread && sql_manager.thread_running) {

		if (sql_manager.manage) {
			switch_queue_push(sql_manager.sql_queue[0], NULL);
			switch_queue_push(sql_manager.sql_queue[1], NULL);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Waiting for unfinished SQL transactions\n");
			wake_thread(0);
		}

		sql_manager.thread_running = -1;
		switch_thread_join(&st, sql_manager.thread);
	}


	if (sql_manager.thread && sql_manager.db_thread_running) {
		sql_manager.db_thread_running = -1;
		switch_thread_join(&st, sql_manager.db_thread);
	}

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
		char *needle = "pass=\"";
		time_t diff = 0;

		diff = now - dbh->last_used;

		if (switch_mutex_trylock(dbh->mutex) == SWITCH_STATUS_SUCCESS) {
			switch_mutex_unlock(dbh->mutex);
			locked = SWITCH_FALSE;
		} else {
			locked = SWITCH_TRUE;
		}

		/* sanitize password */
		memset(cleankey_str, 0, sizeof(cleankey_str));
		pos1 = strstr(dbh->name, needle) + strlen(needle);
		pos2 = strstr(pos1, "\"");
		strncpy(cleankey_str, dbh->name, pos1 - dbh->name);
		strcpy(&cleankey_str[pos1 - dbh->name], pos2);
		
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
