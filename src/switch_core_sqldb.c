/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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

static struct {
	switch_cache_db_handle_t *event_db;
	switch_queue_t *sql_queue[2];
	switch_memory_pool_t *memory_pool;
	switch_event_node_t *event_node;
	switch_thread_t *thread;
	int thread_running;
	switch_bool_t manage;
	switch_mutex_t *io_mutex;
	switch_mutex_t *dbh_mutex;
	switch_hash_t *dbh_hash;
} sql_manager;


#define SWITCH_CORE_DB "core"
/*!
  \brief Open the default system database
*/
SWITCH_DECLARE(switch_status_t) _switch_core_db_handle(switch_cache_db_handle_t ** dbh, const char *file, const char *func, int line)
{
	switch_cache_db_connection_options_t options = { {0} };
	switch_status_t r;

	if (!zstr(runtime.odbc_dsn)) {
		options.odbc_options.dsn = runtime.odbc_dsn;
		options.odbc_options.user = runtime.odbc_user;
		options.odbc_options.pass = runtime.odbc_pass;

		r = _switch_cache_db_get_db_handle(dbh, SCDB_TYPE_ODBC, &options, file, func, line);
	} else {
		options.core_db_options.db_path = SWITCH_CORE_DB;
		r = _switch_cache_db_get_db_handle(dbh, SCDB_TYPE_CORE_DB, &options, file, func, line);
	}

	if (r == SWITCH_STATUS_SUCCESS && !(*dbh)->io_mutex) {
		(*dbh)->io_mutex = sql_manager.io_mutex;
	}

	return r;
}


#define SQL_CACHE_TIMEOUT 300

static void sql_close(time_t prune)
{
	switch_hash_index_t *hi;
	const void *var;
	void *val;
	switch_cache_db_handle_t *dbh = NULL;
	int locked = 0;
	char *key;

	switch_mutex_lock(sql_manager.dbh_mutex);
  top:
	locked = 0;

	for (hi = switch_hash_first(NULL, sql_manager.dbh_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &var, NULL, &val);
		key = (char *) var;

		if ((dbh = (switch_cache_db_handle_t *) val)) {
			time_t diff = 0;

			if (prune > 0 && prune > dbh->last_used) {
				diff = (time_t) prune - dbh->last_used;
			}

			if (prune > 0 && diff < SQL_CACHE_TIMEOUT) {
				continue;
			}

			if (switch_mutex_trylock(dbh->mutex) == SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Dropping idle DB connection %s\n", key);

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

				switch_core_hash_delete(sql_manager.dbh_hash, key);
				switch_mutex_unlock(dbh->mutex);
				switch_core_destroy_memory_pool(&dbh->pool);
				goto top;

			} else {
				if (!prune)
					locked++;
				continue;
			}
		}
	}

	if (locked) {
		goto top;
	}

	switch_mutex_unlock(sql_manager.dbh_mutex);
}


SWITCH_DECLARE(void) switch_cache_db_flush_handles(void)
{
	sql_close(switch_epoch_time_now(NULL) + SQL_CACHE_TIMEOUT + 1);
}



SWITCH_DECLARE(void) switch_cache_db_release_db_handle(switch_cache_db_handle_t ** dbh)
{
	if (dbh && *dbh) {
		switch_mutex_unlock((*dbh)->mutex);
		*dbh = NULL;
	}
}


SWITCH_DECLARE(void) switch_cache_db_destroy_db_handle(switch_cache_db_handle_t ** dbh)
{
	if (dbh && *dbh) {
		switch_mutex_lock(sql_manager.dbh_mutex);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Deleting DB connection %s\n", (*dbh)->name);

		switch ((*dbh)->type) {
		case SCDB_TYPE_ODBC:
			{
				switch_odbc_handle_destroy(&(*dbh)->native_handle.odbc_dbh);
			}
			break;
		case SCDB_TYPE_CORE_DB:
			{
				switch_core_db_close((*dbh)->native_handle.core_db_dbh);
				(*dbh)->native_handle.core_db_dbh = NULL;
			}
			break;
		}


		switch_core_hash_delete(sql_manager.dbh_hash, (*dbh)->name);
		switch_mutex_unlock((*dbh)->mutex);
		switch_core_destroy_memory_pool(&(*dbh)->pool);
		*dbh = NULL;
		switch_mutex_unlock(sql_manager.dbh_mutex);
	}
}

SWITCH_DECLARE(void) switch_cache_db_detach(void)
{
	char thread_str[CACHE_DB_LEN] = "";
	switch_hash_index_t *hi;
	const void *var;
	void *val;
	char *key;
	switch_cache_db_handle_t *dbh = NULL;

	snprintf(thread_str, sizeof(thread_str) - 1, "%lu", (unsigned long) (intptr_t) switch_thread_self());
	switch_mutex_lock(sql_manager.dbh_mutex);

	for (hi = switch_hash_first(NULL, sql_manager.dbh_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &var, NULL, &val);
		key = (char *) var;
		if ((dbh = (switch_cache_db_handle_t *) val)) {
			if (switch_mutex_trylock(dbh->mutex) == SWITCH_STATUS_SUCCESS) {
				if (strstr(dbh->name, thread_str)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10,
									  "Detach cached DB handle %s [%s]\n", thread_str, switch_cache_db_type_name(dbh->type));
					switch_clear_flag(dbh, CDF_INUSE);
				}
				switch_mutex_unlock(dbh->mutex);
			}
		}
	}

	switch_mutex_unlock(sql_manager.dbh_mutex);
}

SWITCH_DECLARE(switch_status_t) _switch_cache_db_get_db_handle(switch_cache_db_handle_t ** dbh,
															   switch_cache_db_handle_type_t type,
															   switch_cache_db_connection_options_t *connection_options,
															   const char *file, const char *func, int line)
{
	switch_thread_id_t self = switch_thread_self();
	char thread_str[CACHE_DB_LEN] = "";
	char db_str[CACHE_DB_LEN] = "";
	char db_callsite_str[CACHE_DB_LEN] = "";
	switch_cache_db_handle_t *new_dbh = NULL;
	switch_ssize_t hlen = -1;

	const char *db_name = NULL;
	const char *db_user = NULL;
	const char *db_pass = NULL;

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
	snprintf(thread_str, sizeof(thread_str) - 1, "%s;thread=\"%lu\"", db_str, (unsigned long) (intptr_t) self);
	snprintf(db_callsite_str, sizeof(db_callsite_str) - 1, "%s:%d", file, line);

	switch_mutex_lock(sql_manager.dbh_mutex);
	if ((new_dbh = switch_core_hash_find(sql_manager.dbh_hash, thread_str))) {
		switch_set_string(new_dbh->last_user, db_callsite_str);
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_DEBUG10,
						  "Reuse Cached DB handle %s [%s]\n", thread_str, switch_cache_db_type_name(new_dbh->type));
	} else {
		switch_hash_index_t *hi;
		const void *var;
		void *val;
		char *key;
		unsigned long hash = 0;

		hash = switch_ci_hashfunc_default(db_str, &hlen);

		for (hi = switch_hash_first(NULL, sql_manager.dbh_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, &var, NULL, &val);
			key = (char *) var;

			if ((new_dbh = (switch_cache_db_handle_t *) val)) {
				if (hash == new_dbh->hash && !strncasecmp(new_dbh->name, db_str, strlen(db_str)) &&
					!switch_test_flag(new_dbh, CDF_INUSE) && switch_mutex_trylock(new_dbh->mutex) == SWITCH_STATUS_SUCCESS) {
					switch_set_flag(new_dbh, CDF_INUSE);
					switch_set_string(new_dbh->name, thread_str);
					new_dbh->hash = switch_ci_hashfunc_default(db_str, &hlen);
					switch_set_string(new_dbh->last_user, db_callsite_str);
					switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_DEBUG10,
									  "Reuse Unused Cached DB handle %s [%s]\n", thread_str, switch_cache_db_type_name(new_dbh->type));
					break;
				}
			}
			new_dbh = NULL;
		}
	}

	if (!new_dbh) {
		switch_memory_pool_t *pool = NULL;
		switch_core_db_t *db = NULL;
		switch_odbc_handle_t *odbc_dbh = NULL;

		switch (type) {
		case SCDB_TYPE_ODBC:
			{

				if (!switch_odbc_available()) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failure! OBDC NOT AVAILABLE!\n");
					goto end;
				}

				if ((odbc_dbh = switch_odbc_handle_new(connection_options->odbc_options.dsn,
													   connection_options->odbc_options.user, connection_options->odbc_options.pass))) {
					if (switch_odbc_handle_connect(odbc_dbh) != SWITCH_STATUS_SUCCESS) {
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
						  "Create Cached DB handle %s [%s]\n", thread_str, switch_cache_db_type_name(type));

		switch_core_new_memory_pool(&pool);
		new_dbh = switch_core_alloc(pool, sizeof(*new_dbh));
		new_dbh->pool = pool;
		new_dbh->type = type;
		switch_set_string(new_dbh->name, thread_str);
		switch_set_flag(new_dbh, CDF_INUSE);
		new_dbh->hash = switch_ci_hashfunc_default(db_str, &hlen);


		if (db)
			new_dbh->native_handle.core_db_dbh = db;
		else
			new_dbh->native_handle.odbc_dbh = odbc_dbh;
		switch_mutex_init(&new_dbh->mutex, SWITCH_MUTEX_UNNESTED, new_dbh->pool);
		switch_set_string(new_dbh->creator, db_callsite_str);
		switch_mutex_lock(new_dbh->mutex);

		switch_core_hash_insert(sql_manager.dbh_hash, new_dbh->name, new_dbh);
	}

  end:

	if (new_dbh)
		new_dbh->last_used = switch_epoch_time_now(NULL);

	switch_mutex_unlock(sql_manager.dbh_mutex);

	*dbh = new_dbh;

	return *dbh ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}


static switch_status_t switch_cache_db_execute_sql_real(switch_cache_db_handle_t *dbh, const char *sql, char **err)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *errmsg = NULL;

	if (dbh->io_mutex) {
		switch_mutex_lock(dbh->io_mutex);
	}

	if (err)
		*err = NULL;

	switch (dbh->type) {
	case SCDB_TYPE_ODBC:
		{
			switch_odbc_statement_handle_t stmt = NULL;
			if ((status = switch_odbc_handle_exec(dbh->native_handle.odbc_dbh, sql, &stmt, NULL)) != SWITCH_ODBC_SUCCESS) {
				errmsg = switch_odbc_handle_get_error(dbh->native_handle.odbc_dbh, stmt);
			}
			switch_odbc_statement_handle_free(&stmt);
		}
		break;
	case SCDB_TYPE_CORE_DB:
		{
			status = switch_core_db_exec(dbh->native_handle.core_db_dbh, sql, NULL, NULL, &errmsg);
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


	if (dbh->io_mutex) {
		switch_mutex_unlock(dbh->io_mutex);
	}

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

	if (dbh->io_mutex) {
		switch_mutex_lock(dbh->io_mutex);
	}

	switch (dbh->type) {
	default:
		{
			status = switch_cache_db_execute_sql_chunked(dbh, (char *) sql, 32768, err);
		}
		break;
	}

	if (dbh->io_mutex) {
		switch_mutex_unlock(dbh->io_mutex);
	}

	return status;

}


SWITCH_DECLARE(char *) switch_cache_db_execute_sql2str(switch_cache_db_handle_t *dbh, char *sql, char *str, size_t len, char **err)
{
	switch_status_t status = SWITCH_STATUS_FALSE;


	if (dbh->io_mutex) {
		switch_mutex_lock(dbh->io_mutex);
	}

	switch (dbh->type) {
	case SCDB_TYPE_CORE_DB:
		{
			switch_core_db_stmt_t *stmt;

			if (switch_core_db_prepare(dbh->native_handle.core_db_dbh, sql, -1, &stmt, 0)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Statement Error [%s]!\n", sql);
				return NULL;
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

	if (dbh->io_mutex) {
		switch_mutex_unlock(dbh->io_mutex);
	}

	return status == SWITCH_STATUS_SUCCESS ? str : NULL;

}

SWITCH_DECLARE(switch_status_t) switch_cache_db_persistant_execute(switch_cache_db_handle_t *dbh, const char *sql, uint32_t retries)
{
	char *errmsg = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	uint8_t forever = 0;

	if (!retries) {
		forever = 1;
		retries = 1000;
	}

	if (dbh->io_mutex) {
		switch_mutex_lock(dbh->io_mutex);
	}

	while (retries > 0) {
		switch_cache_db_execute_sql_real(dbh, sql, &errmsg);
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

	if (dbh->io_mutex) {
		switch_mutex_unlock(dbh->io_mutex);
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

	if (!retries) {
		forever = 1;
		retries = 1000;
	}

	if (dbh->io_mutex) {
		switch_mutex_lock(dbh->io_mutex);
	}

  again:

	while (begin_retries > 0) {
		again = 0;

		switch_cache_db_execute_sql_real(dbh, "BEGIN", &errmsg);

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
				switch_cache_db_execute_sql_real(dbh, "COMMIT", NULL);
				goto again;
			}

			switch_yield(100000);

			if (begin_retries == 0) {
				goto done;
			}
		} else {
			break;
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

  done:

	switch_cache_db_execute_sql_real(dbh, "COMMIT", NULL);

	if (dbh->io_mutex) {
		switch_mutex_unlock(dbh->io_mutex);
	}


	return status;
}

SWITCH_DECLARE(switch_status_t) switch_cache_db_execute_sql_callback(switch_cache_db_handle_t *dbh,
																	 const char *sql, switch_core_db_callback_func_t callback, void *pdata, char **err)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *errmsg = NULL;

	if (err)
		*err = NULL;

	if (dbh->io_mutex) {
		switch_mutex_lock(dbh->io_mutex);
	}


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
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s] %s\n", sql, errmsg);
				free(errmsg);
			}
		}
		break;
	}

	if (dbh->io_mutex) {
		switch_mutex_unlock(dbh->io_mutex);
	}

	return status;
}

SWITCH_DECLARE(switch_bool_t) switch_cache_db_test_reactive(switch_cache_db_handle_t *dbh,
															const char *test_sql, const char *drop_sql, const char *reactive_sql)
{
	char *errmsg;
	switch_bool_t r = SWITCH_TRUE;

	if (dbh->io_mutex) {
		switch_mutex_lock(dbh->io_mutex);
	}

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


	if (dbh->io_mutex) {
		switch_mutex_unlock(dbh->io_mutex);
	}

	return r;
}



#define SQLLEN 1024 * 1024
static void *SWITCH_THREAD_FUNC switch_core_sql_thread(switch_thread_t *thread, void *obj)
{
	void *pop;
	uint32_t itterations = 0;
	uint8_t trans = 0, nothing_in_queue = 0;
	uint32_t target = 100000;
	switch_size_t len = 0, sql_len = SQLLEN;
	char *tmp, *sqlbuf = (char *) malloc(sql_len);
	char *sql;
	switch_size_t newlen;
	int lc = 0;
	uint32_t loops = 0, sec = 0;
	uint32_t l1 = 1000;

	switch_assert(sqlbuf);

	if (!sql_manager.manage) {
		l1 = 10;
	}

	if (!sql_manager.event_db) {
		switch_core_db_handle(&sql_manager.event_db);
	}

	sql_manager.thread_running = 1;

	while (sql_manager.thread_running == 1) {
		if (++loops == l1) {
			if (++sec == SQL_CACHE_TIMEOUT) {
				sql_close(switch_epoch_time_now(NULL));
				sec = 0;
			}
			loops = 0;
		}

		if (!sql_manager.manage) {
			switch_yield(100000);
			continue;
		}

		if (switch_queue_trypop(sql_manager.sql_queue[0], &pop) == SWITCH_STATUS_SUCCESS ||
			switch_queue_trypop(sql_manager.sql_queue[1], &pop) == SWITCH_STATUS_SUCCESS) {
			sql = (char *) pop;

			if (sql) {
				newlen = strlen(sql) + 2;

				if (itterations == 0) {
					trans = 1;
				}

				/* ignore abnormally large strings sql strings as potential buffer overflow */
				if (newlen < SQLLEN) {
					itterations++;
					if (len + newlen > sql_len) {
						sql_len = len + SQLLEN;
						if (!(tmp = realloc(sqlbuf, sql_len))) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "SQL thread ending on mem err\n");
							abort();
							break;
						}
						sqlbuf = tmp;
					}
					sprintf(sqlbuf + len, "%s;\n", sql);
					len += newlen;

				}
				switch_core_db_free(sql);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "SQL thread ending\n");
				break;
			}
		} else {
			nothing_in_queue = 1;
		}


		if (trans && ((itterations == target) || (nothing_in_queue && ++lc >= 500))) {
			if (switch_cache_db_persistant_execute_trans(sql_manager.event_db, sqlbuf, 1) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "SQL thread unable to commit transaction, records lost!\n");
			}
			itterations = 0;
			trans = 0;
			nothing_in_queue = 0;
			len = 0;
			*sqlbuf = '\0';
			lc = 0;
		}

		if (nothing_in_queue) {
			switch_cond_next();
		}
	}

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

static void core_event_handler(switch_event_t *event)
{
	char *sql = NULL;

	switch_assert(event);

	switch (event->event_id) {
	case SWITCH_EVENT_ADD_SCHEDULE:
		{
			const char *id = switch_event_get_header(event, "task-id");
			const char *manager = switch_event_get_header(event, "task-sql_manager");

			if (id) {
				sql = switch_mprintf("insert into tasks values(%q,'%q','%q',%q, '%q')",
									 id,
									 switch_event_get_header_nil(event, "task-desc"),
									 switch_event_get_header_nil(event, "task-group"), manager ? manager : "0", switch_core_get_variable("hostname")
					);
			}
		}
		break;
	case SWITCH_EVENT_DEL_SCHEDULE:
	case SWITCH_EVENT_EXE_SCHEDULE:
		sql = switch_mprintf("delete from tasks where task_id=%q and hostname='%q'",
							 switch_event_get_header_nil(event, "task-id"), switch_core_get_variable("hostname"));
		break;
	case SWITCH_EVENT_RE_SCHEDULE:
		{
			const char *id = switch_event_get_header(event, "task-id");
			const char *manager = switch_event_get_header(event, "task-sql_manager");

			if (id) {
				sql = switch_mprintf("update tasks set task_desc='%q',task_group='%q', task_sql_manager=%q where task_id=%q and hostname='%q'",
									 switch_event_get_header_nil(event, "task-desc"),
									 switch_event_get_header_nil(event, "task-group"), manager ? manager : "0", id, switch_core_get_variable("hostname"));
			}
		}
		break;
	case SWITCH_EVENT_CHANNEL_DESTROY:
		sql = switch_mprintf("delete from channels where uuid='%q' and hostname='%q'",
							 switch_event_get_header_nil(event, "unique-id"), switch_core_get_variable("hostname"));
		break;
	case SWITCH_EVENT_CHANNEL_UUID:
		{
			sql = switch_mprintf("update channels set uuid='%q' where uuid='%q' and hostname='%q';"
								 "update calls set caller_uuid='%q' where caller_uuid='%q' and hostname='%q';"
								 "update calls set callee_uuid='%q' where callee_uuid='%q' and hostname='%q'",
								 switch_event_get_header_nil(event, "unique-id"),
								 switch_event_get_header_nil(event, "old-unique-id"),
								 switch_core_get_variable("hostname"),
								 switch_event_get_header_nil(event, "unique-id"),
								 switch_event_get_header_nil(event, "old-unique-id"),
								 switch_core_get_variable("hostname"),
								 switch_event_get_header_nil(event, "unique-id"),
								 switch_event_get_header_nil(event, "old-unique-id"), switch_core_get_variable("hostname")
				);
			break;
		}
	case SWITCH_EVENT_CHANNEL_CREATE:
		sql = switch_mprintf("insert into channels (uuid,direction,created,created_epoch, name,state,dialplan,context,hostname) "
							 "values('%q','%q','%q','%ld','%q','%q','%q','%q','%q')",
							 switch_event_get_header_nil(event, "unique-id"),
							 switch_event_get_header_nil(event, "call-direction"),
							 switch_event_get_header_nil(event, "event-date-local"),
							 (long) switch_epoch_time_now(NULL),
							 switch_event_get_header_nil(event, "channel-name"),
							 switch_event_get_header_nil(event, "channel-state"),
							 switch_event_get_header_nil(event, "caller-dialplan"),
							 switch_event_get_header_nil(event, "caller-context"), switch_core_get_variable("hostname")
			);
		break;
	case SWITCH_EVENT_CODEC:
		sql =
			switch_mprintf
			("update channels set read_codec='%q',read_rate='%q',write_codec='%q',write_rate='%q' where uuid='%q' and hostname='%q'",
			 switch_event_get_header_nil(event, "channel-read-codec-name"),
			 switch_event_get_header_nil(event, "channel-read-codec-rate"),
			 switch_event_get_header_nil(event, "channel-write-codec-name"),
			 switch_event_get_header_nil(event, "channel-write-codec-rate"),
			 switch_event_get_header_nil(event, "unique-id"), switch_core_get_variable("hostname"));
		break;
	case SWITCH_EVENT_CHANNEL_EXECUTE:
		sql = switch_mprintf("update channels set application='%q',application_data='%q',"
							 "presence_id='%q',presence_data='%q' where uuid='%q' and hostname='%q'",
							 switch_event_get_header_nil(event, "application"),
							 switch_event_get_header_nil(event, "application-data"),
							 switch_event_get_header_nil(event, "channel-presence-id"),
							 switch_event_get_header_nil(event, "channel-presence-data"),
							 switch_event_get_header_nil(event, "unique-id"), switch_core_get_variable("hostname")

			);

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
				sql = switch_mprintf("update channels set state='%s',cid_name='%q',cid_num='%q',"
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
									 switch_event_get_header_nil(event, "unique-id"), switch_core_get_variable("hostname"));
				break;
			default:
				sql = switch_mprintf("update channels set state='%s' where uuid='%s' and hostname='%q'",
									 switch_event_get_header_nil(event, "channel-state"),
									 switch_event_get_header_nil(event, "unique-id"), switch_core_get_variable("hostname"));
				break;
			}
			break;
		}
	case SWITCH_EVENT_CHANNEL_BRIDGE:
		sql = switch_mprintf("insert into calls values ('%s', '%ld', '%s','%q','%q','%q','%q','%s','%q','%q','%q','%q','%s','%q')",
							 switch_event_get_header_nil(event, "event-date-local"),
							 (long) switch_epoch_time_now(NULL),
							 switch_event_get_header_nil(event, "event-calling-function"),
							 switch_event_get_header_nil(event, "caller-caller-id-name"),
							 switch_event_get_header_nil(event, "caller-caller-id-number"),
							 switch_event_get_header_nil(event, "caller-destination-number"),
							 switch_event_get_header_nil(event, "caller-channel-name"),
							 switch_event_get_header_nil(event, "caller-unique-id"),
							 switch_event_get_header_nil(event, "Other-Leg-caller-id-name"),
							 switch_event_get_header_nil(event, "Other-Leg-caller-id-number"),
							 switch_event_get_header_nil(event, "Other-Leg-destination-number"),
							 switch_event_get_header_nil(event, "Other-Leg-channel-name"),
							 switch_event_get_header_nil(event, "Other-Leg-unique-id"), switch_core_get_variable("hostname")
			);
		break;
	case SWITCH_EVENT_CHANNEL_UNBRIDGE:
		sql = switch_mprintf("delete from calls where caller_uuid='%s' and hostname='%q'",
							 switch_event_get_header_nil(event, "caller-unique-id"), switch_core_get_variable("hostname"));
		break;
	case SWITCH_EVENT_SHUTDOWN:
		sql = switch_mprintf("delete from channels where hostname='%q';"
							 "delete from interfaces where hostname='%q';"
							 "delete from calls where hostname='%q'",
							 switch_core_get_variable("hostname"), switch_core_get_variable("hostname"), switch_core_get_variable("hostname")
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
				sql =
					switch_mprintf
					("insert into interfaces (type,name,description,syntax,ikey,filename,hostname) values('%q','%q','%q','%q','%q','%q','%q')", type, name,
					 switch_str_nil(description), switch_str_nil(syntax), switch_str_nil(key), switch_str_nil(filename),
					 switch_core_get_variable("hostname")
					);
			}
			break;
		}
	case SWITCH_EVENT_MODULE_UNLOAD:
		{
			const char *type = switch_event_get_header_nil(event, "type");
			const char *name = switch_event_get_header_nil(event, "name");
			if (!zstr(type) && !zstr(name)) {
				sql = switch_mprintf("delete from interfaces where type='%q' and name='%q' and hostname='%q'", type, name,
									 switch_core_get_variable("hostname"));
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
			sql = switch_mprintf("update channels set secure='%s' where uuid='%s' and hostname='%q'",
								 type, switch_event_get_header_nil(event, "caller-unique-id"), switch_core_get_variable("hostname")
				);
			break;
		}
	case SWITCH_EVENT_NAT:
		{
			const char *op = switch_event_get_header_nil(event, "op");
			switch_bool_t sticky = switch_true(switch_event_get_header_nil(event, "sticky"));
			if (!strcmp("add", op)) {
				sql = switch_mprintf("insert into nat (port, proto, sticky, hostname) values (%s, %s, %d,'%q')",
									 switch_event_get_header_nil(event, "port"),
									 switch_event_get_header_nil(event, "proto"), sticky, switch_core_get_variable("hostname")
					);
			} else if (!strcmp("del", op)) {
				sql = switch_mprintf("delete from nat where port=%s and proto=%s and hostname='%q'",
									 switch_event_get_header_nil(event, "port"),
									 switch_event_get_header_nil(event, "proto"), switch_core_get_variable("hostname"));
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

	if (sql) {
		if (switch_stristr("update channels", sql)) {
			switch_queue_push(sql_manager.sql_queue[1], sql);
		} else {
			switch_queue_push(sql_manager.sql_queue[0], sql);
		}
		sql = NULL;
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
	"   write_codec  VARCHAR(128),\n"
	"   write_rate  VARCHAR(32),\n"
	"   secure VARCHAR(32),\n"
	"   hostname VARCHAR(256),\n"
	"   presence_id VARCHAR(4096),\n"
	"   presence_data VARCHAR(4096)\n"
	");\ncreate index uuindex on channels (uuid,hostname);\n";

static char create_calls_sql[] =
	"CREATE TABLE calls (\n"
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
	");\n" "create index eruuindex on calls (caller_uuid,hostname);\n" "create index eeuuindex on calls (callee_uuid,hostname);\n";

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

switch_status_t switch_core_sqldb_start(switch_memory_pool_t *pool, switch_bool_t manage)
{
	switch_threadattr_t *thd_attr;
	switch_cache_db_handle_t *dbh;

	sql_manager.memory_pool = pool;
	sql_manager.manage = manage;

	switch_mutex_init(&sql_manager.dbh_mutex, SWITCH_MUTEX_NESTED, sql_manager.memory_pool);
	switch_mutex_init(&sql_manager.io_mutex, SWITCH_MUTEX_NESTED, sql_manager.memory_pool);

	switch_core_hash_init(&sql_manager.dbh_hash, sql_manager.memory_pool);

  top:

	/* Activate SQL database */
	if (switch_core_db_handle(&dbh) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB!\n");

		if (runtime.odbc_dsn) {
			runtime.odbc_dsn = NULL;
			runtime.odbc_user = NULL;
			runtime.odbc_pass = NULL;
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
			const char *hostname = switch_core_get_variable("hostname");

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


	switch (dbh->type) {
	case SCDB_TYPE_ODBC:
		{
			char *err;
			switch_cache_db_test_reactive(dbh, "select hostname from channels", "DROP TABLE channels", create_channels_sql);
			switch_cache_db_test_reactive(dbh, "select hostname from calls", "DROP TABLE calls", create_calls_sql);
			switch_cache_db_test_reactive(dbh, "select ikey from interfaces", "DROP TABLE interfaces", create_interfaces_sql);
			switch_cache_db_test_reactive(dbh, "select hostname from tasks", "DROP TABLE tasks", create_tasks_sql);

			switch_cache_db_execute_sql(dbh, "begin;delete from channels where hostname='';delete from channels where hostname='';commit;", &err);

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
	switch_cache_db_execute_sql(dbh, "create index nat_map_port_proto on nat (port,proto,hostname)", NULL);
	switch_cache_db_execute_sql(dbh, "create index channels1 on channels(hostname)", NULL);
	switch_cache_db_execute_sql(dbh, "create index calls1 on calls(hostname)", NULL);


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
	switch_thread_create(&sql_manager.thread, thd_attr, switch_core_sql_thread, NULL, sql_manager.memory_pool);

	while (!sql_manager.thread_running) {
		switch_yield(10000);
	}

	switch_cache_db_release_db_handle(&dbh);

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
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Waiting for unfinished SQL transactions\n");
		}

		sql_manager.thread_running = -1;
		switch_thread_join(&st, sql_manager.thread);
	}

	sql_close(0);

	switch_core_hash_destroy(&sql_manager.dbh_hash);

}

SWITCH_DECLARE(void) switch_cache_db_status(switch_stream_handle_t *stream)
{
	/* return some status info suitable for the cli */
	switch_hash_index_t *hi;
	switch_cache_db_handle_t *dbh = NULL;
	void *val;
	const void *var;
	char *key;
	switch_bool_t locked = SWITCH_FALSE;
	time_t now = switch_epoch_time_now(NULL);
	char cleankey_str[CACHE_DB_LEN];
	char *pos1 = NULL;
	char *pos2 = NULL;

	switch_mutex_lock(sql_manager.dbh_mutex);

	for (hi = switch_hash_first(NULL, sql_manager.dbh_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &var, NULL, &val);
		key = (char *) var;

		if ((dbh = (switch_cache_db_handle_t *) val)) {
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
			pos1 = strstr(key, needle) + strlen(needle);
			pos2 = strstr(pos1, "\"");
			strncpy(cleankey_str, key, pos1 - key);
			strcpy(&cleankey_str[pos1 - key], pos2);


			stream->write_function(stream, "%s\n\tType: %s\n\tLast used: %d\n\tFlags: %s, %s\n"
								   "\tCreator: %s\n\tLast User: %s\n",
								   cleankey_str,
								   switch_cache_db_type_name(dbh->type),
								   diff,
								   locked ? "Locked" : "Unlocked",
								   switch_test_flag(dbh, CDF_INUSE) ? "Attached" : "Detached", dbh->creator, dbh->last_user);
		}
	}
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
