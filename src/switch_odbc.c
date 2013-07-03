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
 *
 * switch_odbc.c -- ODBC
 *
 */

#include <switch.h>

#ifdef SWITCH_HAVE_ODBC
#include <sql.h>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4201)
#include <sqlext.h>
#pragma warning(pop)
#else
#include <sqlext.h>
#endif
#include <sqltypes.h>

#if (ODBCVER < 0x0300)
#define SQL_NO_DATA SQL_SUCCESS
#endif

struct switch_odbc_handle {
	char *dsn;
	char *username;
	char *password;
	SQLHENV env;
	SQLHDBC con;
	switch_odbc_state_t state;
	char odbc_driver[256];
	BOOL is_firebird;
	int affected_rows;
	int num_retries;
};
#endif

SWITCH_DECLARE(switch_odbc_handle_t *) switch_odbc_handle_new(const char *dsn, const char *username, const char *password)
{
#ifdef SWITCH_HAVE_ODBC
	switch_odbc_handle_t *new_handle;

	if (!(new_handle = malloc(sizeof(*new_handle)))) {
		goto err;
	}

	memset(new_handle, 0, sizeof(*new_handle));

	if (!(new_handle->dsn = strdup(dsn))) {
		goto err;
	}

	if (username) {
		if (!(new_handle->username = strdup(username))) {
			goto err;
		}
	}

	if (password) {
		if (!(new_handle->password = strdup(password))) {
			goto err;
		}
	}

	new_handle->env = SQL_NULL_HANDLE;
	new_handle->state = SWITCH_ODBC_STATE_INIT;
	new_handle->affected_rows = 0;
	new_handle->num_retries = DEFAULT_ODBC_RETRIES;

	return new_handle;

  err:
	if (new_handle) {
		switch_safe_free(new_handle->dsn);
		switch_safe_free(new_handle->username);
		switch_safe_free(new_handle->password);
		switch_safe_free(new_handle);
	}
#endif
	return NULL;
}

SWITCH_DECLARE(void) switch_odbc_set_num_retries(switch_odbc_handle_t *handle, int num_retries)
{
#ifdef SWITCH_HAVE_ODBC
	if (handle) {
		handle->num_retries = num_retries;
	}
#endif
}

SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_handle_disconnect(switch_odbc_handle_t *handle)
{
#ifdef SWITCH_HAVE_ODBC

	int result;

	if (!handle) {
		return SWITCH_ODBC_FAIL;
	}

	if (handle->state == SWITCH_ODBC_STATE_CONNECTED) {
		result = SQLDisconnect(handle->con);
		if (result == SWITCH_ODBC_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Disconnected %d from [%s]\n", result, handle->dsn);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Disconnecting [%s]\n", handle->dsn);
		}
	}

	handle->state = SWITCH_ODBC_STATE_DOWN;

	return SWITCH_ODBC_SUCCESS;
#else
	return SWITCH_ODBC_FAIL;
#endif
}


#ifdef SWITCH_HAVE_ODBC
static switch_odbc_status_t init_odbc_handles(switch_odbc_handle_t *handle, switch_bool_t do_reinit)
{
	int result;

	if (!handle) {
		return SWITCH_ODBC_FAIL;
	}

	/* if handle is already initialized, and we're supposed to reinit - free old handle first */
	if (do_reinit == SWITCH_TRUE && handle->env != SQL_NULL_HANDLE) {
		SQLFreeHandle(SQL_HANDLE_DBC, handle->con);
		SQLFreeHandle(SQL_HANDLE_ENV, handle->env);
		handle->env = SQL_NULL_HANDLE;
	}

	if (handle->env == SQL_NULL_HANDLE) {
		result = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &handle->env);

		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error AllocHandle\n");
			handle->env = SQL_NULL_HANDLE; /* Reset handle value, just in case */
			return SWITCH_ODBC_FAIL;
		}

		result = SQLSetEnvAttr(handle->env, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);

		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error SetEnv\n");
			SQLFreeHandle(SQL_HANDLE_ENV, handle->env);
			handle->env = SQL_NULL_HANDLE; /* Reset handle value after it's freed */
			return SWITCH_ODBC_FAIL;
		}

		result = SQLAllocHandle(SQL_HANDLE_DBC, handle->env, &handle->con);

		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error AllocHDB %d\n", result);
			SQLFreeHandle(SQL_HANDLE_ENV, handle->env);
			handle->env = SQL_NULL_HANDLE; /* Reset handle value after it's freed */
			return SWITCH_ODBC_FAIL;
		}
		SQLSetConnectAttr(handle->con, SQL_LOGIN_TIMEOUT, (SQLPOINTER *) 10, 0);
	}

	return SWITCH_ODBC_SUCCESS;
}

static int db_is_up(switch_odbc_handle_t *handle)
{
	int ret = 0;
	SQLHSTMT stmt = NULL;
	SQLLEN m = 0;
	int result;
	switch_event_t *event;
	switch_odbc_status_t recon = 0;
	char *err_str = NULL;
	SQLCHAR sql[255] = "";
	int max_tries = DEFAULT_ODBC_RETRIES;
	int code = 0;
	SQLRETURN rc;
	SQLSMALLINT nresultcols;


	if (handle) {
		max_tries = handle->num_retries;
		if (max_tries < 1)
			max_tries = DEFAULT_ODBC_RETRIES;
	}

  top:

	if (!handle) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "No DB Handle\n");
		goto done;
	}

	if (handle->is_firebird) {
		strcpy((char *) sql, "select first 1 * from RDB$RELATIONS");
	} else {
		strcpy((char *) sql, "select 1");
	}

	if (SQLAllocHandle(SQL_HANDLE_STMT, handle->con, &stmt) != SQL_SUCCESS) {
		code = __LINE__;
		goto error;
	}

	SQLSetStmtAttr(stmt, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER)30, 0);

	if (SQLPrepare(stmt, sql, SQL_NTS) != SQL_SUCCESS) {
		code = __LINE__;
		goto error;
	}

	result = SQLExecute(stmt);

	if (result != SQL_SUCCESS && result != SQL_SUCCESS_WITH_INFO) {
		code = __LINE__;
		goto error;
	}

	SQLRowCount(stmt, &m);
	rc = SQLNumResultCols(stmt, &nresultcols);
	if (rc != SQL_SUCCESS) {
		code = __LINE__;
		goto error;
	}
	ret = (int) nresultcols;
	/* determine statement type */
	if (nresultcols <= 0) {
		/* statement is not a select statement */
		code = __LINE__;
		goto error;
	}

	goto done;

  error:
	err_str = switch_odbc_handle_get_error(handle, stmt);

	/* Make sure to free the handle before we try to reconnect */
	if (stmt) {
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		stmt = NULL;
	}

	recon = switch_odbc_handle_connect(handle);

	max_tries--;

	if (switch_event_create(&event, SWITCH_EVENT_TRAP) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Failure-Message", "The sql server is not responding for DSN %s [%s][%d]",
								switch_str_nil(handle->dsn), switch_str_nil(err_str), code);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "The sql server is not responding for DSN %s [%s][%d]\n",
						  switch_str_nil(handle->dsn), switch_str_nil(err_str), code);

		if (recon == SWITCH_ODBC_SUCCESS) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Additional-Info", "The connection has been re-established");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "The connection has been re-established\n");
		} else {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Additional-Info", "The connection could not be re-established");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "The connection could not be re-established\n");
		}
		if (!max_tries) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Additional-Info", "Giving up!");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Giving up!\n");
		}

		switch_event_fire(&event);
	}

	if (!max_tries) {
		goto done;
	}

	switch_safe_free(err_str);
	switch_yield(1000000);
	goto top;

  done:

	switch_safe_free(err_str);

	if (stmt) {
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	}

	return ret;
}
#endif

SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_statement_handle_free(switch_odbc_statement_handle_t *stmt)
{
	if (!stmt || !*stmt) {
		return SWITCH_ODBC_FAIL;
	}
#ifdef SWITCH_HAVE_ODBC
	SQLFreeHandle(SQL_HANDLE_STMT, *stmt);
	*stmt = NULL;
	return SWITCH_ODBC_SUCCESS;
#else
	return SWITCH_ODBC_FAIL;
#endif
}


SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_handle_connect(switch_odbc_handle_t *handle)
{
#ifdef SWITCH_HAVE_ODBC
	int result;
	SQLINTEGER err;
	int16_t mlen;
	unsigned char msg[200] = "", stat[10] = "";
	SQLSMALLINT valueLength = 0;
	int i = 0;

	init_odbc_handles(handle, SWITCH_FALSE); /* Init ODBC handles, if they are already initialized, don't do it again */

	if (handle->state == SWITCH_ODBC_STATE_CONNECTED) {
		switch_odbc_handle_disconnect(handle);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Re-connecting %s\n", handle->dsn);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Connecting %s\n", handle->dsn);

	if (!strstr(handle->dsn, "DRIVER")) {
		result = SQLConnect(handle->con, (SQLCHAR *) handle->dsn, SQL_NTS, (SQLCHAR *) handle->username, SQL_NTS, (SQLCHAR *) handle->password, SQL_NTS);
	} else {
		SQLCHAR outstr[1024] = { 0 };
		SQLSMALLINT outstrlen = 0;
		result =
			SQLDriverConnect(handle->con, NULL, (SQLCHAR *) handle->dsn, (SQLSMALLINT) strlen(handle->dsn), outstr, sizeof(outstr), &outstrlen,
							 SQL_DRIVER_NOPROMPT);
	}

	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO)) {
		char *err_str;
		if ((err_str = switch_odbc_handle_get_error(handle, NULL))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n", err_str);
			free(err_str);
		} else {
			SQLGetDiagRec(SQL_HANDLE_DBC, handle->con, 1, stat, &err, msg, sizeof(msg), &mlen);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error SQLConnect=%d errno=%d [%s]\n", result, (int) err, msg);
		}

		/* Deallocate handles again, more chanses to succeed when reconnecting */
		init_odbc_handles(handle, SWITCH_TRUE); /* Reinit ODBC handles */
		return SWITCH_ODBC_FAIL;
	}

	result = SQLGetInfo(handle->con, SQL_DRIVER_NAME, (SQLCHAR *) handle->odbc_driver, 255, &valueLength);
	if (result == SQL_SUCCESS || result == SQL_SUCCESS_WITH_INFO) {
		for (i = 0; i < valueLength; ++i)
			handle->odbc_driver[i] = (char) toupper(handle->odbc_driver[i]);
	}

	if (strstr(handle->odbc_driver, "FIREBIRD") != 0 || strstr(handle->odbc_driver, "FB32") != 0 || strstr(handle->odbc_driver, "FB64") != 0) {
		handle->is_firebird = TRUE;
	} else {
		handle->is_firebird = FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Connected to [%s]\n", handle->dsn);
	handle->state = SWITCH_ODBC_STATE_CONNECTED;
	return SWITCH_ODBC_SUCCESS;
#else
	return SWITCH_ODBC_FAIL;
#endif
}

SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_handle_exec_string(switch_odbc_handle_t *handle, const char *sql, char *resbuf, size_t len, char **err)
{
#ifdef SWITCH_HAVE_ODBC
	switch_odbc_status_t sstatus = SWITCH_ODBC_FAIL;
	switch_odbc_statement_handle_t stmt = NULL;
	SQLCHAR name[1024];
	SQLLEN m = 0;

	handle->affected_rows = 0;

	if (switch_odbc_handle_exec(handle, sql, &stmt, err) == SWITCH_ODBC_SUCCESS) {
		SQLSMALLINT NameLength, DataType, DecimalDigits, Nullable;
		SQLULEN ColumnSize;
		int result;

		SQLRowCount(stmt, &m);
		handle->affected_rows = (int) m;

		if (m == 0) {
			goto done;
		}

		result = SQLFetch(stmt);

		if (result != SQL_SUCCESS && result != SQL_SUCCESS_WITH_INFO && result != SQL_NO_DATA) {
			goto done;
		}

		SQLDescribeCol(stmt, 1, name, sizeof(name), &NameLength, &DataType, &ColumnSize, &DecimalDigits, &Nullable);
		SQLGetData(stmt, 1, SQL_C_CHAR, (SQLCHAR *) resbuf, (SQLLEN) len, NULL);

		sstatus = SWITCH_ODBC_SUCCESS;
	}

	done:

	switch_odbc_statement_handle_free(&stmt);

	return sstatus;
#else
	return SWITCH_ODBC_FAIL;
#endif
}

SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_handle_exec(switch_odbc_handle_t *handle, const char *sql, switch_odbc_statement_handle_t *rstmt,
															 char **err)
{
#ifdef SWITCH_HAVE_ODBC
	SQLHSTMT stmt = NULL;
	int result;
	char *err_str = NULL, *err2 = NULL;
	SQLLEN m = 0;

	handle->affected_rows = 0;

	if (!db_is_up(handle)) {
		goto error;
	}

	if (SQLAllocHandle(SQL_HANDLE_STMT, handle->con, &stmt) != SQL_SUCCESS) {
		err2 = "SQLAllocHandle failed.";
		goto error;
	}

	if (SQLPrepare(stmt, (unsigned char *) sql, SQL_NTS) != SQL_SUCCESS) {
		err2 = "SQLPrepare failed.";
		goto error;
	}

	result = SQLExecute(stmt);

	switch (result) {
	case SQL_SUCCESS:
	case SQL_SUCCESS_WITH_INFO:
	case SQL_NO_DATA:
		break;
	case SQL_ERROR:
		err2 = "SQLExecute returned SQL_ERROR.";
		goto error;
		break;
	case SQL_NEED_DATA:
		err2 = "SQLExecute returned SQL_NEED_DATA.";
		goto error;
		break;
	default:
		err2 = "SQLExecute returned unknown result code.";
		goto error;
	}

	SQLRowCount(stmt, &m);
	handle->affected_rows = (int) m;

	if (rstmt) {
		*rstmt = stmt;
	} else {
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	}

	return SWITCH_ODBC_SUCCESS;

  error:


	if (stmt) {
		err_str = switch_odbc_handle_get_error(handle, stmt);
	}

	if (zstr(err_str)) {
		if (err2) {
			err_str = strdup(err2);
		} else {
			err_str = strdup((char *)"SQL ERROR!");
		}
	}
	
	if (err_str) {
		if (!switch_stristr("already exists", err_str) && !switch_stristr("duplicate key name", err_str)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERR: [%s]\n[%s]\n", sql, switch_str_nil(err_str));
		}
		if (err) {
			*err = err_str;
		} else {
			free(err_str);
		}
	}

	if (rstmt) {
		*rstmt = stmt;
	} else if (stmt) {
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	}
#endif
	return SWITCH_ODBC_FAIL;
}

SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_handle_callback_exec_detailed(const char *file, const char *func, int line,
																			   switch_odbc_handle_t *handle,
																			   const char *sql, switch_core_db_callback_func_t callback, void *pdata,
																			   char **err)
{
#ifdef SWITCH_HAVE_ODBC
	SQLHSTMT stmt = NULL;
	SQLSMALLINT c = 0, x = 0;
	SQLLEN m = 0;
	char *x_err = NULL, *err_str = NULL;
	int result;
	int err_cnt = 0;
	int done = 0;

	handle->affected_rows = 0;

	switch_assert(callback != NULL);

	if (!db_is_up(handle)) {
		x_err = "DB is not up!";
		goto error;
	}

	if (SQLAllocHandle(SQL_HANDLE_STMT, handle->con, &stmt) != SQL_SUCCESS) {
		x_err = "Unable to SQL allocate handle!";
		goto error;
	}

	if (SQLPrepare(stmt, (unsigned char *) sql, SQL_NTS) != SQL_SUCCESS) {
		x_err = "Unable to prepare SQL statement!";
		goto error;
	}

	result = SQLExecute(stmt);

	if (result != SQL_SUCCESS && result != SQL_SUCCESS_WITH_INFO && result != SQL_NO_DATA) {
		x_err = "execute error!";
		goto error;
	}

	SQLNumResultCols(stmt, &c);
	SQLRowCount(stmt, &m);
	handle->affected_rows = (int) m;


	while (!done) {
		int name_len = 256;
		char **names;
		char **vals;
		int y = 0;

		result = SQLFetch(stmt);

		if (result != SQL_SUCCESS) {
			if (result != SQL_NO_DATA) {
				err_cnt++;
			}
			break;
		}

		names = calloc(c, sizeof(*names));
		vals = calloc(c, sizeof(*vals));

		switch_assert(names && vals);

		for (x = 1; x <= c; x++) {
			SQLSMALLINT NameLength = 0, DataType = 0, DecimalDigits = 0, Nullable = 0;
			SQLULEN ColumnSize = 0;
			names[y] = malloc(name_len);
			memset(names[y], 0, name_len);

			SQLDescribeCol(stmt, x, (SQLCHAR *) names[y], (SQLSMALLINT) name_len, &NameLength, &DataType, &ColumnSize, &DecimalDigits, &Nullable);

			if (!ColumnSize) {
				ColumnSize = 255;
			}
			ColumnSize++;

			vals[y] = malloc(ColumnSize);
			memset(vals[y], 0, ColumnSize);
			SQLGetData(stmt, x, SQL_C_CHAR, (SQLCHAR *) vals[y], ColumnSize, NULL);
			y++;
		}

		if (callback(pdata, y, vals, names)) {
			done = 1;
		}

		for (x = 0; x < y; x++) {
			free(names[x]);
			free(vals[x]);
		}
		free(names);
		free(vals);
	}

	SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	stmt = NULL; /* Make sure we don't try to free this handle again */
	
	if (!err_cnt) {
		return SWITCH_ODBC_SUCCESS;
	}

  error:

	if (stmt) {
		err_str = switch_odbc_handle_get_error(handle, stmt);
	}

	if (zstr(err_str) && !zstr(x_err)) {
		err_str = strdup(x_err);
	}

	if (err_str) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "ERR: [%s]\n[%s]\n", sql, switch_str_nil(err_str));
		if (err) {
			*err = err_str;
		} else {
			free(err_str);
		}
	}

	if (stmt) {
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	}


#endif
	return SWITCH_ODBC_FAIL;
}

SWITCH_DECLARE(void) switch_odbc_handle_destroy(switch_odbc_handle_t **handlep)
{
#ifdef SWITCH_HAVE_ODBC

	switch_odbc_handle_t *handle = NULL;

	if (!handlep) {
		return;
	}
	handle = *handlep;

	if (handle) {
		switch_odbc_handle_disconnect(handle);

		if (handle->env != SQL_NULL_HANDLE) {
			SQLFreeHandle(SQL_HANDLE_DBC, handle->con);
			SQLFreeHandle(SQL_HANDLE_ENV, handle->env);
		}
		switch_safe_free(handle->dsn);
		switch_safe_free(handle->username);
		switch_safe_free(handle->password);
		free(handle);
	}
	*handlep = NULL;
#else
	return;
#endif
}

SWITCH_DECLARE(switch_odbc_state_t) switch_odbc_handle_get_state(switch_odbc_handle_t *handle)
{
#ifdef SWITCH_HAVE_ODBC
	return handle ? handle->state : SWITCH_ODBC_STATE_INIT;
#else
	return SWITCH_ODBC_STATE_ERROR;
#endif
}

SWITCH_DECLARE(char *) switch_odbc_handle_get_error(switch_odbc_handle_t *handle, switch_odbc_statement_handle_t stmt)
{
#ifdef SWITCH_HAVE_ODBC

	char buffer[SQL_MAX_MESSAGE_LENGTH + 1] = "";
	char sqlstate[SQL_SQLSTATE_SIZE + 1] = "";
	SQLINTEGER sqlcode;
	SQLSMALLINT length;
	char *ret = NULL;

	if (SQLError(handle->env, handle->con, stmt, (SQLCHAR *) sqlstate, &sqlcode, (SQLCHAR *) buffer, sizeof(buffer), &length) == SQL_SUCCESS) {
		ret = switch_mprintf("STATE: %s CODE %ld ERROR: %s\n", sqlstate, sqlcode, buffer);
	};

	return ret;
#else
	return NULL;
#endif
}

SWITCH_DECLARE(int) switch_odbc_handle_affected_rows(switch_odbc_handle_t *handle)
{
#ifdef SWITCH_HAVE_ODBC
	return handle->affected_rows;
#else
	return 0;
#endif
}

SWITCH_DECLARE(switch_bool_t) switch_odbc_available(void)
{
#ifdef SWITCH_HAVE_ODBC
	return SWITCH_TRUE;
#else
	return SWITCH_FALSE;
#endif
}

SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_SQLSetAutoCommitAttr(switch_odbc_handle_t *handle, switch_bool_t on)
{
#ifdef SWITCH_HAVE_ODBC
	if (on) {
		return SQLSetConnectAttr(handle->con, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER *) SQL_AUTOCOMMIT_ON, 0 );
	} else {
		return SQLSetConnectAttr(handle->con, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER *) SQL_AUTOCOMMIT_OFF, 0 );
	}
#else
	return (switch_odbc_status_t) SWITCH_FALSE;
#endif
}

SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_SQLEndTran(switch_odbc_handle_t *handle, switch_bool_t commit)
{
#ifdef SWITCH_HAVE_ODBC
	if (commit) {
		return SQLEndTran(SQL_HANDLE_DBC, handle->con, SQL_COMMIT);
	} else {
		return SQLEndTran(SQL_HANDLE_DBC, handle->con, SQL_ROLLBACK);
	}
#else
	return (switch_odbc_status_t) SWITCH_FALSE;
#endif
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
