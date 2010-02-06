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
 * Michael Jerris <mike@jerris.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Michael Jerris <mike@jerris.com>
 *
 *
 * switch_core_db.c -- sqlite wrapper and extensions
 *
 */

#include <switch.h>
#include "private/switch_core_pvt.h"

#include <sqlite3.h>

static void db_pick_path(const char *dbname, char *buf, switch_size_t size)
{
	memset(buf, 0, size);
	if (switch_is_file_path(dbname)) {
		strncpy(buf, dbname, size);
	} else {
		switch_snprintf(buf, size, "%s%s%s.db", SWITCH_GLOBAL_dirs.db_dir, SWITCH_PATH_SEPARATOR, dbname);
	}
}

SWITCH_DECLARE(int) switch_core_db_open(const char *filename, switch_core_db_t **ppDb)
{
	return sqlite3_open(filename, ppDb);
}

SWITCH_DECLARE(int) switch_core_db_close(switch_core_db_t *db)
{
	return sqlite3_close(db);
}

SWITCH_DECLARE(const unsigned char *) switch_core_db_column_text(switch_core_db_stmt_t *stmt, int iCol)
{
	const unsigned char *txt = sqlite3_column_text(stmt, iCol);

	if (!strcasecmp((char *) stmt, "(null)")) {
		memset(stmt, 0, 1);
		txt = NULL;
	}

	return txt;

}

SWITCH_DECLARE(const char *) switch_core_db_column_name(switch_core_db_stmt_t *stmt, int N)
{
	return sqlite3_column_name(stmt, N);
}

SWITCH_DECLARE(int) switch_core_db_column_count(switch_core_db_stmt_t *pStmt)
{
	return sqlite3_column_count(pStmt);
}

SWITCH_DECLARE(const char *) switch_core_db_errmsg(switch_core_db_t *db)
{
	return sqlite3_errmsg(db);
}

SWITCH_DECLARE(int) switch_core_db_exec(switch_core_db_t *db, const char *sql, switch_core_db_callback_func_t callback, void *data, char **errmsg)
{
	int ret = 0;
	int sane = 300;
	char *err = NULL;

	while (--sane > 0) {
		ret = sqlite3_exec(db, sql, callback, data, &err);
		if (ret == SQLITE_BUSY || ret == SQLITE_LOCKED) {
			if (sane > 1) {
				switch_safe_free(err);
				switch_yield(100000);
				continue;
			}
		} else {
			break;
		}
	}

	if (errmsg) {
		*errmsg = err;
	} else if (err) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR [%s]\n", err);
		switch_core_db_free(err);
		err = NULL;
	}

	return ret;
}

SWITCH_DECLARE(int) switch_core_db_finalize(switch_core_db_stmt_t *pStmt)
{
	return sqlite3_finalize(pStmt);
}

SWITCH_DECLARE(int) switch_core_db_prepare(switch_core_db_t *db, const char *zSql, int nBytes, switch_core_db_stmt_t **ppStmt, const char **pzTail)
{
	return sqlite3_prepare(db, zSql, nBytes, ppStmt, pzTail);
}

SWITCH_DECLARE(int) switch_core_db_step(switch_core_db_stmt_t *stmt)
{
	return sqlite3_step(stmt);
}

SWITCH_DECLARE(int) switch_core_db_reset(switch_core_db_stmt_t *pStmt)
{
	return sqlite3_reset(pStmt);
}

SWITCH_DECLARE(int) switch_core_db_bind_int(switch_core_db_stmt_t *pStmt, int i, int iValue)
{
	return sqlite3_bind_int(pStmt, i, iValue);
}

SWITCH_DECLARE(int) switch_core_db_bind_int64(switch_core_db_stmt_t *pStmt, int i, int64_t iValue)
{
	return sqlite3_bind_int64(pStmt, i, iValue);
}

SWITCH_DECLARE(int) switch_core_db_bind_text(switch_core_db_stmt_t *pStmt, int i, const char *zData, int nData, switch_core_db_destructor_type_t xDel)
{
	return sqlite3_bind_text(pStmt, i, zData, nData, xDel);
}

SWITCH_DECLARE(int) switch_core_db_bind_double(switch_core_db_stmt_t *pStmt, int i, double dValue)
{
	return sqlite3_bind_double(pStmt, i, dValue);
}

SWITCH_DECLARE(int64_t) switch_core_db_last_insert_rowid(switch_core_db_t *db)
{
	return sqlite3_last_insert_rowid(db);
}

SWITCH_DECLARE(int) switch_core_db_get_table(switch_core_db_t *db, const char *sql, char ***resultp, int *nrow, int *ncolumn, char **errmsg)
{
	return sqlite3_get_table(db, sql, resultp, nrow, ncolumn, errmsg);
}

SWITCH_DECLARE(void) switch_core_db_free_table(char **result)
{
	sqlite3_free_table(result);
}

SWITCH_DECLARE(void) switch_core_db_free(char *z)
{
	sqlite3_free(z);
}

SWITCH_DECLARE(int) switch_core_db_changes(switch_core_db_t *db)
{
	return sqlite3_changes(db);
}

SWITCH_DECLARE(switch_core_db_t *) switch_core_db_open_file(const char *filename)
{
	switch_core_db_t *db;
	char path[1024];

	db_pick_path(filename, path, sizeof(path));
	if (switch_core_db_open(path, &db)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR [%s]\n", switch_core_db_errmsg(db));
		switch_core_db_close(db);
		db = NULL;
	}
	return db;
}

SWITCH_DECLARE(void) switch_core_db_test_reactive(switch_core_db_t *db, char *test_sql, char *drop_sql, char *reactive_sql)
{
	char *errmsg;

	if (db) {
		if (test_sql) {
			switch_core_db_exec(db, test_sql, NULL, NULL, &errmsg);

			if (errmsg) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SQL ERR [%s]\n[%s]\nAuto Generating Table!\n", errmsg, test_sql);
				switch_core_db_free(errmsg);
				errmsg = NULL;
				if (drop_sql) {
					switch_core_db_exec(db, drop_sql, NULL, NULL, &errmsg);
				}
				if (errmsg) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SQL ERR [%s]\n[%s]\n", errmsg, reactive_sql);
					switch_core_db_free(errmsg);
					errmsg = NULL;
				}
				switch_core_db_exec(db, reactive_sql, NULL, NULL, &errmsg);
				if (errmsg) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SQL ERR [%s]\n[%s]\n", errmsg, reactive_sql);
					switch_core_db_free(errmsg);
					errmsg = NULL;
				}
			}
		}
	}

}


SWITCH_DECLARE(switch_status_t) switch_core_db_persistant_execute_trans(switch_core_db_t *db, char *sql, uint32_t retries)
{
	char *errmsg;
	switch_status_t status = SWITCH_STATUS_FALSE;
	uint8_t forever = 0;
	unsigned begin_retries = 100;
	uint8_t again = 0;

	if (!retries) {
		forever = 1;
		retries = 1000;
	}

  again:

	while (begin_retries > 0) {
		again = 0;

		switch_core_db_exec(db, "BEGIN", NULL, NULL, &errmsg);

		if (errmsg) {
			begin_retries--;
			if (strstr(errmsg, "cannot start a transaction within a transaction")) {
				again = 1;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SQL Retry [%s]\n", errmsg);
			}
			switch_core_db_free(errmsg);
			errmsg = NULL;

			if (again) {
				switch_core_db_exec(db, "COMMIT", NULL, NULL, NULL);
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
		switch_core_db_exec(db, sql, NULL, NULL, &errmsg);
		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR [%s]\n", errmsg);
			switch_core_db_free(errmsg);
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

	switch_core_db_exec(db, "COMMIT", NULL, NULL, NULL);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_db_persistant_execute(switch_core_db_t *db, char *sql, uint32_t retries)
{
	char *errmsg;
	switch_status_t status = SWITCH_STATUS_FALSE;
	uint8_t forever = 0;

	if (!retries) {
		forever = 1;
		retries = 1000;
	}

	while (retries > 0) {
		switch_core_db_exec(db, sql, NULL, NULL, &errmsg);
		if (errmsg) {
			//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR [%s]\n", errmsg);
			switch_core_db_free(errmsg);
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
