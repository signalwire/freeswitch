/*
* mod_mariadb for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
* Copyright (C) 2019, Andrey Volk <andywolk@gmail.com>
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
* The Original Code is ported from FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
*
* The Initial Developer of the Original Code is
* Anthony Minessale II <anthm@freeswitch.org>
* Portions created by the Initial Developer are Copyright (C)
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
* Andrey Volk <andywolk@gmail.com>
*
* mod_mariadb.c -- MariaDB (MySQL) FreeSWITCH module
*
*/

#include <switch.h>

#ifndef _WIN32
#include <poll.h>
#else
#include <winsock2.h>
#endif

#include <mysql.h>

#include "mariadb_dsn.hpp"

switch_loadable_module_interface_t *MODULE_INTERFACE;

SWITCH_MODULE_LOAD_FUNCTION(mod_mariadb_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_mariadb_shutdown);
SWITCH_MODULE_DEFINITION(mod_mariadb, mod_mariadb_load, mod_mariadb_shutdown, NULL);

MYSQL *mariadb_dsn_connect(MYSQL *mysql, const char *host, const char *user, const char *passwd,
	const char *db, unsigned int port, const char *unix_socket, unsigned long clientflag);
void mariadb_dsn_close(MYSQL *mysql);
int mariadb_db_set_connection(MYSQL *mysql, enum enum_server_command command, const char *arg,
	size_t length, my_bool skipp_check, void *opt_arg);
my_bool mariadb_db_dsn_reconnect(MYSQL *mysql);

my_bool reconnect = 1;

#define DEFAULT_MARIADB_RETRIES 120

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

typedef enum {
	MARIADB_STATE_INIT,
	MARIADB_STATE_DOWN,
	MARIADB_STATE_CONNECTED,
	MARIADB_STATE_ERROR
} mariadb_state_t;

struct mariadb_handle {
	char *dsn;
	char *sql;
	MYSQL con;
	my_socket sock;
	mariadb_state_t state;
	int affected_rows;
	int num_retries;
	switch_bool_t auto_commit;
	switch_bool_t in_txn;
	int stored_results;
};

typedef struct mariadb_handle mariadb_handle_t;

struct mariadb_result {
	MYSQL_RES *result;
	char *err;
	int rows;
	int cols;
};

typedef struct mariadb_result mariadb_result_t;

switch_status_t mariadb_handle_destroy(switch_database_interface_handle_t **dih);
switch_status_t mariadb_flush(mariadb_handle_t *handle);
static int db_is_up(mariadb_handle_t *handle);
static char *mariadb_handle_get_error(mariadb_handle_t *handle);

#define mariadb_handle_exec_base(handle, sql, err) mariadb_handle_exec_base_detailed(__FILE__, (char * )__SWITCH_FUNC__, __LINE__, handle, sql, err)
#define mariadb_next_result(h, r) mariadb_next_result_timed(h, r, 10000)
#define mariadb_finish_results(handle) mariadb_finish_results_real(__FILE__, (char * )__SWITCH_FUNC__, __LINE__, handle)

static char *supported_prefixes[4] = { 0 };

static int wait_for_mysql(mariadb_handle_t *handle, int status, int msec)
{
	int res = -1;
#ifdef _WIN32
	/*
	On Windows, select() must be used due to a bug in WSAPoll()
	which is supposed to be identical to BSD's poll(), but it is not,
	"Windows 8 Bugs 309411 – WSAPoll does not report failed connections":
	https://social.msdn.microsoft.com/Forums/windowsdesktop/en-US/18769abd-fca0-4d3c-9884-1a38ce27ae90/wsapoll-and-nonblocking-connects-to-nonexistent-ports?forum=wsk
	*/

	fd_set rs, ws, es;
	struct timeval tv, *timeout = NULL;

	FD_ZERO(&rs);
	FD_ZERO(&ws);
	FD_ZERO(&es);

	if (status & MYSQL_WAIT_READ) FD_SET(handle->sock, &rs);
	if (status & MYSQL_WAIT_WRITE) FD_SET(handle->sock, &ws);
	if (status & MYSQL_WAIT_EXCEPT) FD_SET(handle->sock, &es);

	if (status & MYSQL_WAIT_TIMEOUT) {
		tv.tv_sec = MIN(mysql_get_timeout_value(&handle->con), (unsigned int)msec * 1000);
		tv.tv_usec = 0;
		timeout = &tv;
	}

	res = select(1, &rs, &ws, &es, timeout);
#else
	struct pollfd pfd = { 0 };
	int timeout = -1;

	pfd.fd = handle->sock;
	pfd.events |= (status & MYSQL_WAIT_READ ? POLLIN : 0);
	pfd.events |= (status & MYSQL_WAIT_WRITE ? POLLOUT : 0);
	pfd.events |= (status & MYSQL_WAIT_EXCEPT ? POLLPRI : 0);

	if (status & MYSQL_WAIT_TIMEOUT) {
		timeout = MIN(mysql_get_timeout_value(&handle->con), (unsigned int)msec * 1000);
	}

	do {
		res = poll(&pfd, 1, timeout);
	} while (res == -1 && errno == EINTR);
#endif

	if (res == 0) goto timeout;
	else if (res < 0) goto error;

	status = 0;

#ifdef _WIN32
	if (FD_ISSET(handle->sock, &rs)) status |= MYSQL_WAIT_READ;
	if (FD_ISSET(handle->sock, &ws)) status |= MYSQL_WAIT_WRITE;
	if (FD_ISSET(handle->sock, &es)) status |= MYSQL_WAIT_EXCEPT;
#else
	if (pfd.revents & POLLIN) status |= MYSQL_WAIT_READ;
	if (pfd.revents & POLLOUT) status |= MYSQL_WAIT_WRITE;
	if (pfd.revents & POLLPRI) status |= MYSQL_WAIT_EXCEPT;
#endif

	return status;

error:
	// en error
timeout:
	return MYSQL_WAIT_TIMEOUT;
}


static int db_is_up(mariadb_handle_t *handle)
{
	int ret = 0;
	switch_event_t *event;
	char *err_str = NULL;
	int max_tries = DEFAULT_MARIADB_RETRIES;
	int code = 0, recon = 0;

	if (handle) {
		max_tries = handle->num_retries;
		if (max_tries < 1)
			max_tries = DEFAULT_MARIADB_RETRIES;
	}

top:

	if (!handle) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "No DB Handle\n");
		goto done;
	}

	if (mysql_ping(&handle->con) != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "mysql_ping returned bad connection [Error: %s]; reconnecting...\n", mysql_error(&handle->con));
		handle->state = MARIADB_STATE_ERROR;
		if (mariadb_reconnect(&handle->con) != 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mariadb_reconnect returned bad connection -- reconnection failed! [Error: %s]\n", mysql_error(&handle->con));
			goto error;
		}
		handle->state = MARIADB_STATE_CONNECTED;
		handle->sock = mysql_get_socket(&handle->con);
	}

	ret = 1;
	goto done;

error:
	err_str = mariadb_handle_get_error(handle);

	if (mysql_ping(&handle->con) != 0) {
		handle->state = MARIADB_STATE_ERROR;
		if (mariadb_reconnect(&handle->con) == 0) {
			handle->state = MARIADB_STATE_CONNECTED;
			recon = SWITCH_STATUS_SUCCESS;
			handle->sock = mysql_get_socket(&handle->con);
		}
	}

	max_tries--;

	if (switch_event_create(&event, SWITCH_EVENT_TRAP) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Failure-Message", "The sql server is not responding for DSN %s [%s][%d]",
			switch_str_nil(handle->dsn), switch_str_nil(err_str), code);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "The sql server is not responding for DSN %s [%s][%d]\n",
			switch_str_nil(handle->dsn), switch_str_nil(err_str), code);

		if (recon == SWITCH_STATUS_SUCCESS) {
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

	return ret;
}

switch_status_t mariadb_SQLEndTran(mariadb_handle_t *handle, switch_bool_t commit)
{
	char * err_str = NULL;

	if (!handle)
		return SWITCH_STATUS_FALSE;

	if (commit) {
		char *sql = "COMMIT";
		handle->stored_results = 0;
		if (mysql_query(&handle->con, sql)) {
			err_str = mariadb_handle_get_error(handle);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not commit transaction: %s\n", err_str);
			switch_safe_free(err_str);
			return SWITCH_STATUS_FALSE;
		}
	} else {
		char *sql = "ROLLBACK";
		handle->stored_results = 0;
		if (mysql_query(&handle->con, sql)) {
			err_str = mariadb_handle_get_error(handle);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not rollback transaction: %s\n", err_str);
			switch_safe_free(err_str);
			return SWITCH_STATUS_FALSE;
		}
	}
	handle->in_txn = SWITCH_FALSE;

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t mariadb_next_result_timed(mariadb_handle_t *handle, mariadb_result_t **result_out, int msec)
{
	int status = 0;
	mariadb_result_t *res;
	switch_time_t start;
	switch_time_t ctime;
	unsigned int usec = msec * 1000;
	char *err_str;

	if (!handle) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "**BUG** Null handle passed to mariadb_next_result.\n");
		return SWITCH_STATUS_FALSE;
	}

	if (handle->stored_results)	{
		if ( (status = mysql_next_result(&handle->con)) ) {
			if (status > 0)	{
				err_str = mariadb_handle_get_error(handle);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "An error occurred trying to get next for query (%s): %s\n", handle->sql, err_str);
				switch_safe_free(err_str);

				goto error;
			}

			return SWITCH_STATUS_SUCCESS;
		}
	}

	if (!(res = malloc(sizeof(mariadb_result_t)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Malloc failed!\n");
		goto error;
	}

	memset(res, 0, sizeof(mariadb_result_t));

	if ( (status = mysql_store_result_start(&res->result, &handle->con)) ) {
		/* Wait for a result to become available, up to msec milliseconds */
		start = switch_micro_time_now();
		while ((ctime = switch_micro_time_now()) - start <= usec) {
			int wait_time = (usec - (unsigned int)(ctime - start)) / 1000;

			/* Wait for the mariadb socket to be ready for data reads. */
			status = wait_for_mysql(handle, status, wait_time);
			status = mysql_store_result_cont(&res->result, &handle->con, status);

			if (!status)
				break;
		}
	}

	/* At this point, we know we can read a full result without blocking. */

	if (res->result) {
		*result_out = res;

		res->rows = (int)mysql_num_rows(res->result);
		handle->affected_rows = res->rows;
		handle->stored_results++;
		res->cols = mysql_num_fields(res->result);		
	} else {
		if (mysql_field_count(&handle->con) != 0) {
			// mysql_store_result_() should have returned data

			err_str = mariadb_handle_get_error(handle);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "An error occurred trying to use result for query (%s): %s\n", handle->sql, err_str);
			switch_safe_free(err_str);
		}

		free(res);
		res = NULL;
		*result_out = NULL;
	}

	return SWITCH_STATUS_SUCCESS;

error:

	/* Make sure the failed connection does not have any transactions marked as in progress */
	mariadb_flush(handle);

	/* Try to reconnect to the DB if we were dropped */
	db_is_up(handle);

	return SWITCH_STATUS_FALSE;
}

void mariadb_free_result(mariadb_result_t **result)
{
	if (!*result) {
		return;
	}

	if ((*result)->result) {
		mysql_free_result((*result)->result);
	}

	free(*result);
	*result = NULL;
}

switch_status_t mariadb_finish_results_real(const char* file, const char* func, int line, mariadb_handle_t *handle)
{
	char *err_str;
	mariadb_result_t *res = NULL;
	switch_status_t final_status = SWITCH_STATUS_SUCCESS;
	int done = 0, status;

	do {
		mariadb_next_result(handle, &res);
		if (res && res->err && !switch_stristr("already exists", res->err) && !switch_stristr("duplicate key name", res->err)) {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "Error executing query:\n%s\n", res->err);
			final_status = SWITCH_STATUS_FALSE;
		}

		if (!res) {
			if (!mysql_more_results(&handle->con)) {
				done = 1;
			} else {
				if ((status = mysql_next_result(&handle->con))) {
					if (status > 0) {
						err_str = mariadb_handle_get_error(handle);
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "An error occurred trying to get next for query (%s): %s\n", handle->sql, err_str);
						switch_safe_free(err_str);

						break;
					}
				}
			}
		} else if (res->result) {
			handle->affected_rows = (int)mysql_affected_rows(&handle->con);
		}

		mariadb_free_result(&res);
	} while (!done);

	return final_status;
}

switch_status_t mariadb_handle_disconnect(mariadb_handle_t *handle)
{
	if (!handle) {
		return SWITCH_STATUS_FALSE;
	}

	if (handle->state == MARIADB_STATE_CONNECTED) {
		mysql_close(&handle->con);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Disconnected from [%s]\n", handle->dsn);
	}
	switch_safe_free(handle->sql);
	handle->state = MARIADB_STATE_DOWN;

	return SWITCH_STATUS_SUCCESS;
}

static char * mariadb_handle_get_error(mariadb_handle_t *handle)
{
	char *err_str = NULL;

	if (!handle) {
		return NULL;
	}

	switch_strdup(err_str, mysql_error(&handle->con));

	return err_str;
}

switch_status_t mariadb_handle_connect(mariadb_handle_t *handle)
{
	if (!handle) {
		return SWITCH_STATUS_FALSE;
	}

	if (handle->state == MARIADB_STATE_CONNECTED) {
		mariadb_handle_disconnect(handle);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Re-connecting %s\n", handle->dsn);
	}

	if (handle->state == MARIADB_STATE_CONNECTED) {
		mariadb_handle_disconnect(handle);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Re-connecting %s\n", handle->dsn);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Connecting %s\n", handle->dsn);
	mysql_init(&handle->con);

	/* Enable non-blocking operation */
	/* https://mariadb.com/kb/en/library/using-the-non-blocking-library */
	mysql_options(&handle->con, MYSQL_OPT_NONBLOCK, 0);

	/* Enable automatic reconnect with the mariadb_reconnect function, without this that function does not work */
	mysql_options(&handle->con, MYSQL_OPT_RECONNECT, &reconnect);

	/* set timeouts to 300 microseconds */
	/*int default_timeout = 3;
	mysql_options(&handle->con, MYSQL_OPT_READ_TIMEOUT, &default_timeout);
	mysql_options(&handle->con, MYSQL_OPT_CONNECT_TIMEOUT, &default_timeout);
	mysql_options(&handle->con, MYSQL_OPT_WRITE_TIMEOUT, &default_timeout);*/

	if (!mysql_dsn_connect(&handle->con, handle->dsn, CLIENT_MULTI_STATEMENTS)) {
		char *err_str;
		if ((err_str = mariadb_handle_get_error(handle))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n", err_str);
			switch_safe_free(err_str);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to connect to the database [%s]\n", handle->dsn);
			mariadb_handle_disconnect(handle);
		}
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Connected to [%s]\n", handle->dsn);
	handle->state = MARIADB_STATE_CONNECTED;
	handle->sock = mysql_get_socket(&handle->con);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t mariadb_handle_new(switch_cache_db_database_interface_options_t database_interface_options, switch_database_interface_handle_t **dih)
{
	mariadb_handle_t *new_handle = NULL;

	if (!(*dih = malloc(sizeof(**dih)))) {
		goto err;
	}

	if (!(new_handle = malloc(sizeof(*new_handle)))) {
		goto err;
	}

	memset(new_handle, 0, sizeof(*new_handle));

	if (!(new_handle->dsn = strdup(database_interface_options.connection_string))) {
		goto err;
	}

	new_handle->sock = 0;
	new_handle->state = MARIADB_STATE_INIT;	
	new_handle->affected_rows = 0;
	new_handle->num_retries = DEFAULT_MARIADB_RETRIES;
	new_handle->auto_commit = SWITCH_TRUE;
	new_handle->in_txn = SWITCH_FALSE;
	new_handle->stored_results = 0;

	(*dih)->handle = new_handle;

	if (mariadb_handle_connect(new_handle) != SWITCH_STATUS_SUCCESS) {
		if (mariadb_handle_destroy(dih) != SWITCH_STATUS_SUCCESS) {
			goto err;
		}
	}

	return SWITCH_STATUS_SUCCESS;

err:
	switch_safe_free(*dih);

	if (new_handle) {
		switch_safe_free(new_handle->dsn);
		switch_safe_free(new_handle);
	}

	return SWITCH_STATUS_FALSE;
}

switch_status_t mariadb_handle_destroy(switch_database_interface_handle_t **dih)
{
	mariadb_handle_t *handle = NULL;

	if (!dih) {
		return SWITCH_STATUS_FALSE;
	}

	handle = (*dih)->handle;

	if (handle) {
		mariadb_handle_disconnect(handle);

		switch_safe_free(handle->dsn);
		free(handle);
	}

	switch_safe_free(*dih);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t database_flush(switch_database_interface_handle_t *dih)
{
	mariadb_handle_t *handle;

	if (!dih) {
		return SWITCH_STATUS_FALSE;
	}

	handle = dih->handle;

	return mariadb_flush(handle);
}

switch_status_t mariadb_flush(mariadb_handle_t *handle)
{
	MYSQL_RES *tmp = NULL;
	int x = 0;

	if (!handle) {
		return SWITCH_STATUS_FALSE;
	}

	if (handle->stored_results) {
		if (mysql_next_result(&handle->con) != 0) {
			goto done;
		}
	}

	/* Make sure the query is fully cleared */
	while ((tmp = mysql_store_result(&handle->con)) != NULL) {
		mysql_free_result(tmp);
		x++;

		if (mysql_next_result(&handle->con) != 0)
			break;
	}

	if (x) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Flushing %d results\n", x);
	}

done:

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t mariadb_send_query(mariadb_handle_t *handle, const char* sql)
{
	char *err_str;
	int ret;

	switch_safe_free(handle->sql);
	handle->sql = strdup(sql);
	handle->stored_results = 0;
	ret = mysql_real_query(&handle->con, sql, (unsigned long)strlen(sql));	
	if (ret) {
		err_str = mariadb_handle_get_error(handle);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to send query (%s) to database: %s\n", sql, err_str);
		switch_safe_free(err_str);
		mariadb_finish_results(handle);
		goto error;
	}

	return SWITCH_STATUS_SUCCESS;

error:
	return SWITCH_STATUS_FALSE;
}

switch_status_t mariadb_handle_exec_base_detailed(const char *file, const char *func, int line,
	mariadb_handle_t *handle, const char *sql, char **err)
{
	char *err_str = NULL, *er = NULL;

	mariadb_flush(handle);
	handle->affected_rows = 0;

	if (!db_is_up(handle)) {
		er = strdup("Database is not up!");
		goto error;
	}

	if (handle->auto_commit == SWITCH_FALSE && handle->in_txn == SWITCH_FALSE) {
		if (mariadb_send_query(handle, "BEGIN") != SWITCH_STATUS_SUCCESS) {
			er = strdup("Error sending BEGIN!");
			if (mariadb_finish_results(handle) != SWITCH_STATUS_SUCCESS) {
				db_is_up(handle); /* If finish_results failed, maybe the db went dead */
			}
			goto error;
		}

		if (mariadb_finish_results(handle) != SWITCH_STATUS_SUCCESS) {
			db_is_up(handle);
			er = strdup("Error sending BEGIN!");
			goto error;
		}
		handle->in_txn = SWITCH_TRUE;
	}

	if (mariadb_send_query(handle, sql) != SWITCH_STATUS_SUCCESS) {
		er = strdup("Error sending query!");
		if (mariadb_finish_results(handle) != SWITCH_STATUS_SUCCESS) {
			db_is_up(handle);
		}
		goto error;
	}

	return SWITCH_STATUS_SUCCESS;

error:
	err_str = mariadb_handle_get_error(handle);

	if (zstr(err_str)) {
		switch_safe_free(err_str);
		err_str = (er) ? er : strdup((char *)"SQL ERROR!");
	} else {
		switch_safe_free(er);
	}

	if (err_str) {
		if (!switch_stristr("already exists", err_str) && !switch_stristr("duplicate key name", err_str)) {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "ERR: [%s]\n[%s]\n", sql, switch_str_nil(err_str));
		}

		if (err) {
			*err = err_str;
		} else {
			free(err_str);
		}
	}

	return SWITCH_STATUS_FALSE;
}

switch_status_t mariadb_handle_exec_detailed(const char *file, const char *func, int line,
	mariadb_handle_t *handle, const char *sql, char **err)
{
	if (mariadb_handle_exec_base_detailed(file, func, line, handle, sql, err) == SWITCH_STATUS_FALSE) {
		goto error;
	}

	return mariadb_finish_results(handle);

error:
	return SWITCH_STATUS_FALSE;
}

switch_status_t database_handle_exec_detailed(const char *file, const char *func, int line, 
	switch_database_interface_handle_t *dih, const char *sql, char **err)
{
	mariadb_handle_t *handle;

	if (!dih) {
		return SWITCH_STATUS_FALSE;
	}

	handle = dih->handle;

	return mariadb_handle_exec_detailed(file, func, line, handle, sql, err);
}

switch_status_t database_handle_exec_string(switch_database_interface_handle_t *dih, const char *sql, char *resbuf, size_t len, char **err)
{
	mariadb_handle_t *handle;
	switch_status_t sstatus = SWITCH_STATUS_SUCCESS;
	mariadb_result_t *result = NULL;
	MYSQL_ROW row;

	if (!dih) {
		return SWITCH_STATUS_FALSE;
	}

	handle = dih->handle;

	if (!handle)
		return SWITCH_STATUS_FALSE;

	handle->affected_rows = 0;

	if (mariadb_handle_exec_base(handle, sql, err) == SWITCH_STATUS_FALSE) {
		goto error;
	}

	if (mariadb_next_result(handle, &result) == SWITCH_STATUS_FALSE) {
		goto error;
	}

	if (!result) {
		sstatus = SWITCH_STATUS_FALSE;
		goto done;
	}

	if (!result->result) {
		sstatus = SWITCH_STATUS_FALSE;
		goto done;
	}

	if (handle->affected_rows <= 0) {
		goto done;
	}

	row = mysql_fetch_row(result->result);
	if (row) {
		strncpy(resbuf, row[0], len);
	} else {
		resbuf[0] = '\0';
	}

done:

	mariadb_free_result(&result);
	if (mariadb_finish_results(handle) != SWITCH_STATUS_SUCCESS) {
		sstatus = SWITCH_STATUS_FALSE;
	}

	return sstatus;

error:
	return SWITCH_STATUS_FALSE;
}

switch_status_t database_SQLSetAutoCommitAttr(switch_database_interface_handle_t *dih, switch_bool_t on)
{
	mariadb_handle_t *handle;

	if (!dih) {
		return SWITCH_STATUS_FALSE;
	}

	handle = dih->handle;

	if (!handle)
		return SWITCH_STATUS_FALSE;

	if (on) {
		handle->auto_commit = SWITCH_TRUE;
	} else {
		handle->auto_commit = SWITCH_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t mariadb_handle_affected_rows(switch_database_interface_handle_t *dih, int *affected_rows)
{
	mariadb_handle_t *handle = NULL;

	if (!dih) {
		return SWITCH_STATUS_FALSE;
	}

	handle = dih->handle;

	if (!handle)
		return SWITCH_STATUS_FALSE;

	*affected_rows = handle->affected_rows;

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t database_commit(switch_database_interface_handle_t *dih)
{
	switch_status_t result;

	mariadb_handle_t *handle;

	if (!dih) {
		return SWITCH_STATUS_FALSE;
	}

	handle = dih->handle;

	if (!handle)
		return SWITCH_STATUS_FALSE;

	result = mariadb_SQLEndTran(handle, SWITCH_TRUE);
	result = database_SQLSetAutoCommitAttr(dih, SWITCH_TRUE) && result;
	result = mariadb_finish_results(handle) && result;

	return result;
}

switch_status_t database_rollback(switch_database_interface_handle_t *dih)
{
	switch_status_t result;
	mariadb_handle_t *handle;

	if (!dih) {
		return SWITCH_STATUS_FALSE;
	}

	handle = dih->handle;

	if (!handle) {
		return SWITCH_STATUS_FALSE;
	}

	result = mariadb_SQLEndTran(handle, SWITCH_FALSE);
	result = database_SQLSetAutoCommitAttr(dih, SWITCH_TRUE) && result;
	result = mariadb_finish_results(handle) && result;

	return result;
}

switch_status_t mariadb_handle_callback_exec_detailed(const char *file, const char *func, int line,
	switch_database_interface_handle_t *dih, const char *sql, switch_core_db_callback_func_t callback, void *pdata, char **err)
{
	mariadb_handle_t *handle;
	char *err_str = NULL;
	int row = 0, col = 0, err_cnt = 0;
	mariadb_result_t *result = NULL;

	if (!dih) {
		return SWITCH_STATUS_FALSE;
	}

	handle = dih->handle;

	if (!handle) {
		return SWITCH_STATUS_FALSE;
	}

	handle->affected_rows = 0;

	switch_assert(callback != NULL);

	if (mariadb_handle_exec_base(handle, sql, err) == SWITCH_STATUS_FALSE) {
		goto error;
	}

	if (mariadb_next_result(handle, &result) == SWITCH_STATUS_FALSE) {
		err_cnt++;
		err_str = mariadb_handle_get_error(handle);
		if (result && !zstr(result->err)) {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "ERR: [%s]\n[%s]\n", sql, switch_str_nil(result->err));
		}
		if (!zstr(err_str)) {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "ERR: [%s]\n[%s]\n", sql, switch_str_nil(err_str));
		}
		switch_safe_free(err_str);
		err_str = NULL;
	}

	while (result != NULL) {
		for (row = 0; row < result->rows; ++row) {
			char **names;
			char **vals;
			MYSQL_ROW data_row;

			names = calloc(result->cols, sizeof(*names));
			vals = calloc(result->cols, sizeof(*vals));

			switch_assert(names && vals);

			data_row = mysql_fetch_row(result->result);

			for (col = 0; col < result->cols; ++col) {
				unsigned long *lengths;
				MYSQL_FIELD *field = mysql_fetch_field_direct(result->result, col);
				if (field) {
					names[col] = malloc(field->name_length +1);
					names[col][field->name_length] = '\0';
					strncpy(names[col], field->name, field->name_length);
										
					lengths = mysql_fetch_lengths(result->result);
					vals[col] = malloc(lengths[col] + 1);
					vals[col][lengths[col]] = '\0';

					if (data_row) {
						strncpy(vals[col], data_row[col], lengths[col]);
					} else {
						vals[col][0] = '\0';
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "ERR: Column number %d out of range\n", col);
				}
			}

			if (callback(pdata, result->cols, vals, names)) {
				mariadb_finish_results(handle);
				row = result->rows;
			}

			for (col = 0; col < result->cols; ++col) {
				free(names[col]);
				free(vals[col]);
			}

			free(names);
			free(vals);
		}

		mariadb_free_result(&result);
		if (mariadb_next_result(handle, &result) == SWITCH_STATUS_FALSE) {
			err_cnt++;
			err_str = mariadb_handle_get_error(handle);
			if (result && !zstr(result->err)) {
				switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "ERR: [%s]\n[%s]\n", sql, switch_str_nil(result->err));
			}
			if (!zstr(err_str)) {
				switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "ERR: [%s]\n[%s]\n", sql, switch_str_nil(err_str));
			}
			switch_safe_free(err_str);
			err_str = NULL;
		}
	}
	if (err_cnt) {
		goto error;
	}

	return SWITCH_STATUS_SUCCESS;

error:
	return SWITCH_STATUS_FALSE;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_mariadb_load)
{
	switch_database_interface_t *database_interface;

	supported_prefixes[0] = (char *)"mariadb";
	supported_prefixes[1] = (char *)"maria";
	supported_prefixes[2] = (char *)"mysql";

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	MODULE_INTERFACE = *module_interface;

	database_interface = (switch_database_interface_t *)switch_loadable_module_create_interface(*module_interface, SWITCH_DATABASE_INTERFACE);
	database_interface->interface_name = modname;
	/*
		MariaDB and MySQL both have row size limits
		https://mariadb.com/kb/en/library/innodb-limitations/#limitations-on-size
		https://dev.mysql.com/doc/refman/8.0/en/column-count-limit.html

		Setting database flag to SWITCH_DATABASE_FLAG_ROW_SIZE_LIMIT 
		will allow FreeSWITCH Core to properly create wide tables such as the channel table 
		which size exeeds the limit in the case of a multi-byte charset like utf8 (1-4 bytes per character).
	*/
	database_interface->flags = SWITCH_DATABASE_FLAG_ROW_SIZE_LIMIT;
	database_interface->prefixes = supported_prefixes;
	database_interface->handle_new = mariadb_handle_new;
	database_interface->handle_destroy = mariadb_handle_destroy;
	database_interface->flush = database_flush;
	database_interface->exec_detailed = database_handle_exec_detailed;
	database_interface->exec_string = database_handle_exec_string;
	database_interface->affected_rows = mariadb_handle_affected_rows;
	database_interface->sql_set_auto_commit_attr = database_SQLSetAutoCommitAttr;
	database_interface->commit = database_commit;
	database_interface->rollback = database_rollback;
	database_interface->callback_exec_detailed = mariadb_handle_callback_exec_detailed;
	
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_mariadb_shutdown)
{
	return SWITCH_STATUS_UNLOAD;
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
