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
 *
 * switch_odbc.c -- ODBC
 *
 */

#include <switch.h>
#include <switch_odbc.h>

struct switch_odbc_handle {
	char *dsn;
	char *username;
	char *password;
	SQLHENV env;
	SQLHDBC con;
	switch_odbc_state_t state;
	char odbc_driver[256];
	BOOL is_firebird;
};

SWITCH_DECLARE(switch_odbc_handle_t *) switch_odbc_handle_new(char *dsn, char *username, char *password)
{
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

	return new_handle;

  err:
	if (new_handle) {
		switch_safe_free(new_handle->dsn);
		switch_safe_free(new_handle->username);
		switch_safe_free(new_handle->password);
		switch_safe_free(new_handle);
	}

	return NULL;
}

SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_handle_disconnect(switch_odbc_handle_t *handle)
{
	int result;

	if (handle->state == SWITCH_ODBC_STATE_CONNECTED) {
		result = SQLDisconnect(handle->con);
		if (result == SWITCH_ODBC_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Disconnected %d from [%s]\n", result, handle->dsn);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Disconnectiong [%s]\n", handle->dsn);
		}
	} 

	handle->state = SWITCH_ODBC_STATE_DOWN;

	return SWITCH_ODBC_SUCCESS;
}

SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_handle_connect(switch_odbc_handle_t *handle)
{
	int result;
	SQLINTEGER err;
	int16_t mlen;
	unsigned char msg[200], stat[10];
	SQLSMALLINT valueLength = 0;
	int i = 0;

	if (handle->env == SQL_NULL_HANDLE) {
		result = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &handle->env);

		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error AllocHandle\n");
			return SWITCH_ODBC_FAIL;
		}

		result = SQLSetEnvAttr(handle->env, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);

		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error SetEnv\n");
			SQLFreeHandle(SQL_HANDLE_ENV, handle->env);
			return SWITCH_ODBC_FAIL;
		}

		result = SQLAllocHandle(SQL_HANDLE_DBC, handle->env, &handle->con);

		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error AllocHDB %d\n", result);
			SQLFreeHandle(SQL_HANDLE_ENV, handle->env);
			return SWITCH_ODBC_FAIL;
		}
		SQLSetConnectAttr(handle->con, SQL_LOGIN_TIMEOUT, (SQLPOINTER *) 10, 0);
	}
	if (handle->state == SWITCH_ODBC_STATE_CONNECTED) {
		switch_odbc_handle_disconnect(handle);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Re-connecting %s\n", handle->dsn);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connecting %s\n", handle->dsn);
	
	if(!strstr(handle->dsn, "DRIVER")) { 
		result = SQLConnect(handle->con, (SQLCHAR *) handle->dsn, SQL_NTS, (SQLCHAR *) handle->username, SQL_NTS, (SQLCHAR *) handle->password, SQL_NTS); 
	} else { 
		SQLCHAR outstr[1024] = {0}; 
		SQLSMALLINT outstrlen = 0; 
		result = SQLDriverConnect(handle->con, NULL, (SQLCHAR *) handle->dsn, (SQLSMALLINT)strlen(handle->dsn), outstr, sizeof(outstr), &outstrlen, SQL_DRIVER_NOPROMPT); 
	} 

	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO)) {
		char *err_str;
		if ((err_str = switch_odbc_handle_get_error(handle, NULL))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n",err_str);
			free(err_str);
		} else {
			SQLGetDiagRec(SQL_HANDLE_DBC, handle->con, 1, stat, &err, msg, 100, &mlen);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error SQLConnect=%d errno=%d %s\n", result, (int) err, msg);
		}
		SQLFreeHandle(SQL_HANDLE_ENV, handle->env);
		return SWITCH_ODBC_FAIL;
	}

	result = SQLGetInfo(handle->con, SQL_DRIVER_NAME, (SQLCHAR*)handle->odbc_driver, 255, &valueLength);
	if ( result == SQL_SUCCESS || result == SQL_SUCCESS_WITH_INFO) {
		for (i = 0; i < valueLength; ++i)
			handle->odbc_driver[i] = (char)toupper(handle->odbc_driver[i]);
	}

	if (strstr(handle->odbc_driver, "FIREBIRD") != 0 || strstr(handle->odbc_driver, "FB32") != 0 || strstr(handle->odbc_driver, "FB64") != 0) {
		handle->is_firebird = TRUE;
	} else {
		handle->is_firebird = FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connected to [%s]\n", handle->dsn);
	handle->state = SWITCH_ODBC_STATE_CONNECTED;
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
	int max_tries = 120;

 top:
	
	if (!handle) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "No DB Handle\n");
		goto done;
	}

	if (handle->is_firebird) {
		strcpy((char*)sql, "select first 1 * from RDB$RELATIONS");
	} else {
		strcpy((char*)sql, "select 1");
	}

    if (SQLAllocHandle(SQL_HANDLE_STMT, handle->con, &stmt) != SQL_SUCCESS) {
        goto error;
    }
	
    if (SQLPrepare(stmt, sql, SQL_NTS) != SQL_SUCCESS) {
        goto error;
    }
	
    result = SQLExecute(stmt);

	SQLRowCount(stmt, &m);
	ret = (int) m;

	if (result < 0 || m < 0) {
		goto error;
	}

	goto done;

 error:
	err_str = switch_odbc_handle_get_error(handle, stmt);
	recon = switch_odbc_handle_connect(handle);

	max_tries--;

	if (switch_event_create(&event, SWITCH_EVENT_TRAP) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Failure-Message", "The sql server is not responding for DSN %s [%s]", 
								switch_str_nil(handle->dsn), switch_str_nil(err_str));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "The sql server is not responding for DSN %s [%s]\n",
						  switch_str_nil(handle->dsn), switch_str_nil(err_str));

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

SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_handle_exec(switch_odbc_handle_t *handle, char *sql, SQLHSTMT *rstmt)
{
	SQLHSTMT stmt = NULL;
	int result;

	if (!db_is_up(handle)) {
		goto error;
	}
	
	if (SQLAllocHandle(SQL_HANDLE_STMT, handle->con, &stmt) != SQL_SUCCESS) {
		goto error;
	}

	if (SQLPrepare(stmt, (unsigned char *) sql, SQL_NTS) != SQL_SUCCESS) {
		goto error;
	}

	result = SQLExecute(stmt);

	if (result != SQL_SUCCESS && result != SQL_SUCCESS_WITH_INFO) {
		goto error;
	}

	if (rstmt) {
		*rstmt = stmt;
	} else {
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	}

	return SWITCH_ODBC_SUCCESS;

 error:
	if (rstmt) {
        *rstmt = stmt;
    } else if (stmt) {
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	}
	return SWITCH_ODBC_FAIL;	
}

SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_handle_callback_exec(switch_odbc_handle_t *handle,
																	  char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	SQLHSTMT stmt = NULL;
	SQLSMALLINT c = 0, x = 0;
	SQLLEN m = 0, t = 0;
	int result;

	switch_assert(callback != NULL);

	if (!db_is_up(handle)) {
		goto error;
	}

	if (SQLAllocHandle(SQL_HANDLE_STMT, handle->con, &stmt) != SQL_SUCCESS) {
		goto error;
	}

	if (SQLPrepare(stmt, (unsigned char *) sql, SQL_NTS) != SQL_SUCCESS) {
		goto error;
	}

	result = SQLExecute(stmt);

	if (result != SQL_SUCCESS && result != SQL_SUCCESS_WITH_INFO) {
		goto error;
	}

	SQLNumResultCols(stmt, &c);
	SQLRowCount(stmt, &m);

	if (m > 0) {
		for (t = 0; t < m; t++) {
			int name_len = 256;
			char **names;
			char **vals;
			int y = 0;
		
			if (!(result = SQLFetch(stmt)) == SQL_SUCCESS) {
				goto error;
			}
		
			names = calloc(c, sizeof(*names));
			vals = calloc(c, sizeof(*vals));
		
			switch_assert(names && vals);

			for (x = 1; x <= c; x++) {
				SQLSMALLINT NameLength, DataType, DecimalDigits, Nullable;
				SQLULEN ColumnSize;
				names[y] = malloc(name_len);
				memset(names[y], 0, name_len);

				SQLDescribeCol(stmt, x, (SQLCHAR *) names[y], (SQLSMALLINT)name_len, &NameLength, &DataType, &ColumnSize, &DecimalDigits, &Nullable);
				ColumnSize++;

				vals[y] = malloc(ColumnSize);
				memset(vals[y], 0, ColumnSize);
				SQLGetData(stmt, x, SQL_C_CHAR, (SQLCHAR *) vals[y], ColumnSize, NULL);
				y++;
			}
		
			if (callback(pdata, y, vals, names)) {
				break;
			}

			for (x = 0; x < y; x++) {
				free(names[x]);
				free(vals[x]);
			}
			free(names);
			free(vals);
		}
	}

	SQLFreeHandle(SQL_HANDLE_STMT, stmt);

	return SWITCH_ODBC_SUCCESS;

  error:

	if (stmt) {
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	}

	return SWITCH_ODBC_FAIL;
}

SWITCH_DECLARE(void) switch_odbc_handle_destroy(switch_odbc_handle_t **handlep)
{
	switch_odbc_handle_t *handle = NULL;

	if (!handlep) {
		return;
	}
	handle = *handlep;

	if (handle) {
		switch_odbc_handle_disconnect(handle);

		SQLFreeHandle(SQL_HANDLE_DBC, handle->con);
		SQLFreeHandle(SQL_HANDLE_ENV, handle->env);
		switch_safe_free(handle->dsn);
		switch_safe_free(handle->username);
		switch_safe_free(handle->password);
		free(handle);
	}
	*handlep = NULL;
}

SWITCH_DECLARE(switch_odbc_state_t) switch_odbc_handle_get_state(switch_odbc_handle_t *handle)
{
	return handle ? handle->state : SWITCH_ODBC_STATE_INIT;
}

SWITCH_DECLARE(char *) switch_odbc_handle_get_error(switch_odbc_handle_t *handle, SQLHSTMT stmt)
{
	char buffer[SQL_MAX_MESSAGE_LENGTH + 1] = "";
	char sqlstate[SQL_SQLSTATE_SIZE + 1] = "";
	SQLINTEGER sqlcode;
	SQLSMALLINT length;
	char *ret = NULL;

	if (SQLError(handle->env, handle->con, stmt, (SQLCHAR *)sqlstate, &sqlcode, (SQLCHAR *)buffer, sizeof(buffer), &length) == SQL_SUCCESS) {
		ret = switch_mprintf("STATE: %s CODE %ld ERROR: %s\n", sqlstate,  sqlcode, buffer);
	};

	return ret;
}


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
