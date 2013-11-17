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
 * Eliot Gable <egable@gmail.com>
 *
 * switch_pgsql.c -- PGSQL Driver
 *
 */

#include <switch.h>

#ifndef WIN32
#include <switch_private.h>
#endif

#ifdef SWITCH_HAVE_PGSQL
#include <libpq-fe.h>
#include <poll.h>


struct switch_pgsql_handle {
	char *dsn;
	char *sql;
	PGconn* con;
	int sock;
	switch_pgsql_state_t state;
	int affected_rows;
	int num_retries;
	switch_bool_t auto_commit;
	switch_bool_t in_txn;
};

struct switch_pgsql_result {
	PGresult *result;
	ExecStatusType status;
	char *err;
	int rows;
	int cols;
};
#endif

SWITCH_DECLARE(switch_pgsql_handle_t *) switch_pgsql_handle_new(const char *dsn)
{
#ifdef SWITCH_HAVE_PGSQL
	switch_pgsql_handle_t *new_handle;

	if (!(new_handle = malloc(sizeof(*new_handle)))) {
		goto err;
	}

	memset(new_handle, 0, sizeof(*new_handle));

	if (!(new_handle->dsn = strdup(dsn))) {
		goto err;
	}

	new_handle->sock = 0;
	new_handle->state = SWITCH_PGSQL_STATE_INIT;
	new_handle->con = NULL;
	new_handle->affected_rows = 0;
	new_handle->num_retries = DEFAULT_PGSQL_RETRIES;
	new_handle->auto_commit = SWITCH_TRUE;
	new_handle->in_txn = SWITCH_FALSE;

	return new_handle;

  err:
	if (new_handle) {
		switch_safe_free(new_handle->dsn);
		switch_safe_free(new_handle);
	}
#endif
	return NULL;
}


#ifdef SWITCH_HAVE_PGSQL
static int db_is_up(switch_pgsql_handle_t *handle)
{
	int ret = 0;
	switch_event_t *event;
	char *err_str = NULL;
	int max_tries = DEFAULT_PGSQL_RETRIES;
	int code = 0, recon = 0;

	if (handle) {
		max_tries = handle->num_retries;
		if (max_tries < 1)
			max_tries = DEFAULT_PGSQL_RETRIES;
	}

  top:

	if (!handle) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "No DB Handle\n");
		goto done;
	}
	if (!handle->con) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "No DB Connection\n");
		goto done;
	}
  
    /* Try a non-blocking read on the connection to gobble up any EOF from a closed connection and mark the connection BAD if it is closed. */
    PQconsumeInput(handle->con);

	if (PQstatus(handle->con) == CONNECTION_BAD) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "PQstatus returned bad connection; reconnecting...\n");
		handle->state = SWITCH_PGSQL_STATE_ERROR;
		PQreset(handle->con);
		if (PQstatus(handle->con) == CONNECTION_BAD) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "PQstatus returned bad connection -- reconnection failed!\n");
			goto error;
		}
		handle->state = SWITCH_PGSQL_STATE_CONNECTED;
		handle->sock = PQsocket(handle->con);
	}

/*	if (!PQsendQuery(handle->con, "SELECT 1")) {
		code = __LINE__;
		goto error;
	}

	if(switch_pgsql_next_result(handle, &result) == SWITCH_PGSQL_FAIL) {
		code = __LINE__;
		goto error;
	}

	if (!result || result->status != PGRES_COMMAND_OK) {
		code = __LINE__;
		goto error;
	}

	switch_pgsql_free_result(&result);
	switch_pgsql_finish_results(handle);
*/
	ret = 1;
	goto done;

  error:
	err_str = switch_pgsql_handle_get_error(handle);

	if (PQstatus(handle->con) == CONNECTION_BAD) {
		handle->state = SWITCH_PGSQL_STATE_ERROR;
		PQreset(handle->con);
		if (PQstatus(handle->con) == CONNECTION_OK) {
			handle->state = SWITCH_PGSQL_STATE_CONNECTED;
			recon = SWITCH_PGSQL_SUCCESS;
			handle->sock = PQsocket(handle->con);
		}
	}

	max_tries--;

	if (switch_event_create(&event, SWITCH_EVENT_TRAP) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Failure-Message", "The sql server is not responding for DSN %s [%s][%d]",
								switch_str_nil(handle->dsn), switch_str_nil(err_str), code);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "The sql server is not responding for DSN %s [%s][%d]\n",
						  switch_str_nil(handle->dsn), switch_str_nil(err_str), code);

		if (recon == SWITCH_PGSQL_SUCCESS) {
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
#endif


SWITCH_DECLARE(void) switch_pgsql_set_num_retries(switch_pgsql_handle_t *handle, int num_retries)
{
#ifdef SWITCH_HAVE_PGSQL
	if (handle) {
		handle->num_retries = num_retries;
	}
#endif
}

SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_handle_disconnect(switch_pgsql_handle_t *handle)
{
#ifdef SWITCH_HAVE_PGSQL

	if (!handle) {
		return SWITCH_PGSQL_FAIL;
	}

	if (handle->state == SWITCH_PGSQL_STATE_CONNECTED) {
		PQfinish(handle->con);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Disconnected from [%s]\n", handle->dsn);
	}
	switch_safe_free(handle->sql);
	handle->state = SWITCH_PGSQL_STATE_DOWN;

	return SWITCH_PGSQL_SUCCESS;
#else
	return SWITCH_PGSQL_FAIL;
#endif
}

SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_send_query(switch_pgsql_handle_t *handle, const char* sql)
{
#ifdef SWITCH_HAVE_PGSQL
	char *err_str;

	switch_safe_free(handle->sql);
	handle->sql = strdup(sql);
	if (!PQsendQuery(handle->con, sql)) {
		err_str = switch_pgsql_handle_get_error(handle);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to send query (%s) to database: %s\n", sql, err_str);
		switch_pgsql_finish_results(handle);
		goto error;
	}

	return SWITCH_PGSQL_SUCCESS;
 error:
#endif
	return SWITCH_PGSQL_FAIL;
}

SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_cancel_real(const char *file, const char *func, int line, switch_pgsql_handle_t *handle)
{
	switch_pgsql_status_t ret = SWITCH_PGSQL_SUCCESS;
#ifdef SWITCH_HAVE_PGSQL
	char err_buf[256];
	PGcancel *cancel = NULL;

	memset(err_buf, 0, 256);
	cancel = PQgetCancel(handle->con);
	if(!PQcancel(cancel, err_buf, 256)) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CRIT, "Failed to cancel long-running query (%s): %s\n", handle->sql, err_buf);
		ret = SWITCH_PGSQL_FAIL;
	}
	PQfreeCancel(cancel);
	switch_pgsql_flush(handle);

#endif
	return ret;
}


SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_next_result_timed(switch_pgsql_handle_t *handle, switch_pgsql_result_t **result_out, int msec)
{
#ifdef SWITCH_HAVE_PGSQL
	switch_pgsql_result_t *res;
	switch_time_t start;
	switch_time_t ctime;
	unsigned int usec = msec * 1000;
	char *err_str;
	struct pollfd fds[2] = { {0} };
	int poll_res = 0;
	
	if(!handle) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "**BUG** Null handle passed to switch_pgsql_next_result.\n");
		return SWITCH_PGSQL_FAIL;
	}

	/* Try to consume input that might be waiting right away */
	if (PQconsumeInput(handle->con)) {
		/* And check to see if we have a full result ready for reading */
		if (PQisBusy(handle->con)) {

			/* Wait for a result to become available, up to msec milliseconds */
			start = switch_time_now();
			while((ctime = switch_micro_time_now()) - start <= usec) {
				int wait_time = (usec - (ctime - start)) / 1000;
				fds[0].fd = handle->sock;
				fds[0].events |= POLLIN;
				fds[0].events |= POLLERR;
				fds[0].events |= POLLNVAL;
				fds[0].events |= POLLHUP;
				fds[0].events |= POLLPRI;
				fds[0].events |= POLLRDNORM;
				fds[0].events |= POLLRDBAND;

				/* Wait for the PostgreSQL socket to be ready for data reads. */
				if ((poll_res = poll(&fds[0], 1, wait_time)) > 0 ) {
					if (fds[0].revents & POLLHUP || fds[0].revents & POLLNVAL) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "PGSQL socket closed or invalid while waiting for result for query (%s)\n", handle->sql);
						goto error;
					} else if (fds[0].revents & POLLERR) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Poll error trying to read PGSQL socket for query (%s)\n", handle->sql);
						goto error;
					} else if (fds[0].revents & POLLIN || fds[0].revents & POLLPRI || fds[0].revents & POLLRDNORM || fds[0].revents & POLLRDBAND) {
						/* Then try to consume any input waiting. */
						if (PQconsumeInput(handle->con)) {
							if (PQstatus(handle->con) == CONNECTION_BAD) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Connection terminated while waiting for result.\n");
								handle->state = SWITCH_PGSQL_STATE_ERROR;
								goto error;
							}

							/* And check to see if we have a full result ready for reading */
							if (!PQisBusy(handle->con)) {
								/* If we can pull a full result without blocking, then break this loop */
								break;
							}
						} else {
							/* If we had an error trying to consume input, report it and cancel the query. */
							err_str = switch_pgsql_handle_get_error(handle);
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "An error occurred trying to consume input for query (%s): %s\n", handle->sql, err_str);
							switch_safe_free(err_str);
							switch_pgsql_cancel(handle);
							goto error;
						}
					}
				} else if (poll_res == -1) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Poll failed trying to read PGSQL socket for query (%s)\n", handle->sql);
					goto error;
				}
			}

			/* If we broke the loop above because of a timeout, report that and cancel the query. */
			if (ctime - start > usec) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Query (%s) took too long to complete or database not responding.\n", handle->sql);
				switch_pgsql_cancel(handle);
				goto error;
			}

		}
	} else {
		/* If we had an error trying to consume input, report it and cancel the query. */
		err_str = switch_pgsql_handle_get_error(handle);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "An error occurred trying to consume input for query (%s): %s\n", handle->sql, err_str);
		switch_safe_free(err_str);
		/* switch_pgsql_cancel(handle); */
		goto error;
	}


	/* At this point, we know we can read a full result without blocking. */
	if(!(res = malloc(sizeof(switch_pgsql_result_t)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Malloc failed!\n");
		goto error;
	}
	memset(res, 0, sizeof(switch_pgsql_result_t));

	
	res->result = PQgetResult(handle->con);
	if (res->result) {
		*result_out = res;
		res->status = PQresultStatus(res->result);
		switch(res->status) {
#if POSTGRESQL_MAJOR_VERSION >= 9 && POSTGRESQL_MINOR_VERSION >= 2
		case PGRES_SINGLE_TUPLE:
			/* Added in PostgreSQL 9.2 */
#endif
		case PGRES_TUPLES_OK:
			{
				res->rows = PQntuples(res->result);
				handle->affected_rows = res->rows;
				res->cols = PQnfields(res->result);
			}
			break;
#if POSTGRESQL_MAJOR_VERSION >= 9 && POSTGRESQL_MINOR_VERSION >= 1
		case PGRES_COPY_BOTH:
			/* Added in PostgreSQL 9.1 */
#endif
		case PGRES_COPY_OUT:
		case PGRES_COPY_IN:
		case PGRES_COMMAND_OK:
			break;
		case PGRES_EMPTY_QUERY:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Query (%s) returned PGRES_EMPTY_QUERY\n", handle->sql);
		case PGRES_BAD_RESPONSE:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Query (%s) returned PGRES_BAD_RESPONSE\n", handle->sql);
		case PGRES_NONFATAL_ERROR:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Query (%s) returned PGRES_NONFATAL_ERROR\n", handle->sql);
		case PGRES_FATAL_ERROR:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Query (%s) returned PGRES_FATAL_ERROR\n", handle->sql);
			res->err = PQresultErrorMessage(res->result);
			goto error;
			break;
		}
	} else {
		free(res);
		res = NULL;
		*result_out = NULL;
	}

	return SWITCH_PGSQL_SUCCESS;
 error:

	/* Make sure the failed connection does not have any transactions marked as in progress */
	switch_pgsql_flush(handle);

	/* Try to reconnect to the DB if we were dropped */
	db_is_up(handle);

#endif
	return SWITCH_PGSQL_FAIL;
}

SWITCH_DECLARE(void) switch_pgsql_free_result(switch_pgsql_result_t **result)
{
#ifdef SWITCH_HAVE_PGSQL

	if (!*result) {
		return;
	}

	if ((*result)->result) {
		PQclear((*result)->result);
	}
	free(*result);
	*result = NULL;
#else
	return;
#endif
}

SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_finish_results_real(const char* file, const char* func, int line, switch_pgsql_handle_t *handle)
{
#ifdef SWITCH_HAVE_PGSQL
	switch_pgsql_result_t *res = NULL;
	switch_pgsql_status_t final_status = SWITCH_PGSQL_SUCCESS;
	int done = 0;
	do {
		switch_pgsql_next_result(handle, &res);
		if (res && res->err && !switch_stristr("already exists", res->err) && !switch_stristr("duplicate key name", res->err)) {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "Error executing query:\n%s\n", res->err);
			final_status = SWITCH_PGSQL_FAIL;
		}
		if (!res) done = 1;
		switch_pgsql_free_result(&res);
	} while (!done);
	return final_status;
#else
	return SWITCH_PGSQL_FAIL;
#endif
}


SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_handle_connect(switch_pgsql_handle_t *handle)
{
#ifdef SWITCH_HAVE_PGSQL
	if (handle->state == SWITCH_PGSQL_STATE_CONNECTED) {
		switch_pgsql_handle_disconnect(handle);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Re-connecting %s\n", handle->dsn);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Connecting %s\n", handle->dsn);

	handle->con = PQconnectdb(handle->dsn);
	if (PQstatus(handle->con) != CONNECTION_OK) {
		char *err_str;
		if ((err_str = switch_pgsql_handle_get_error(handle))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n", err_str);
			switch_safe_free(err_str);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to connect to the database [%s]\n", handle->dsn);
			switch_pgsql_handle_disconnect(handle);
		}
		return SWITCH_PGSQL_FAIL;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Connected to [%s]\n", handle->dsn);
	handle->state = SWITCH_PGSQL_STATE_CONNECTED;
	handle->sock = PQsocket(handle->con);
	return SWITCH_PGSQL_SUCCESS;
#else
	return SWITCH_PGSQL_FAIL;
#endif
}

SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_handle_exec_string_detailed(const char *file, const char *func, int line,
																			   switch_pgsql_handle_t *handle, const char *sql, char *resbuf, size_t len, char **err)
{
#ifdef SWITCH_HAVE_PGSQL
	switch_pgsql_status_t sstatus = SWITCH_PGSQL_SUCCESS;
	char *val = NULL;
	switch_pgsql_result_t *result = NULL;

	handle->affected_rows = 0;

	if (switch_pgsql_handle_exec_base_detailed(file, func, line, handle, sql, err) == SWITCH_PGSQL_FAIL) {
		goto error;
	}

	if(switch_pgsql_next_result(handle, &result) == SWITCH_PGSQL_FAIL) {
		goto error;
	}

	if (!result) {
		switch (result->status) {
#if POSTGRESQL_MAJOR_VERSION >= 9 && POSTGRESQL_MINOR_VERSION >= 2
		case PGRES_SINGLE_TUPLE:
			/* Added in PostgreSQL 9.2 */
#endif
		case PGRES_COMMAND_OK:
		case PGRES_TUPLES_OK:
			break;
		default:
			sstatus = SWITCH_PGSQL_FAIL;
			goto done;
		}
	}

	if (handle->affected_rows <= 0) {
		goto done;
	}

	val = PQgetvalue(result->result, 0, 0);
	strncpy(resbuf, val, len);

	done:

	switch_pgsql_free_result(&result);
	if (switch_pgsql_finish_results(handle) != SWITCH_PGSQL_SUCCESS) {
		sstatus = SWITCH_PGSQL_FAIL;
	}

	return sstatus;
 error:
	return SWITCH_PGSQL_FAIL;
#else
	return SWITCH_PGSQL_FAIL;
#endif
}

SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_handle_exec_base_detailed(const char *file, const char *func, int line,
																			 switch_pgsql_handle_t *handle, const char *sql, char **err)
{
#ifdef SWITCH_HAVE_PGSQL
	char *err_str = NULL, *er = NULL;



	switch_pgsql_flush(handle);
	handle->affected_rows = 0;

	if (!db_is_up(handle)) {
		er = strdup("Database is not up!");
		goto error;
	}

	if (handle->auto_commit == SWITCH_FALSE && handle->in_txn == SWITCH_FALSE) {
		if (switch_pgsql_send_query(handle, "BEGIN") != SWITCH_PGSQL_SUCCESS) {
			er = strdup("Error sending BEGIN!");
			if (switch_pgsql_finish_results(handle) != SWITCH_PGSQL_SUCCESS) {
				db_is_up(handle); /* If finish_results failed, maybe the db went dead */
			}
			goto error;
		}

		if (switch_pgsql_finish_results(handle) != SWITCH_PGSQL_SUCCESS) {
			db_is_up(handle);
			er = strdup("Error sending BEGIN!");
			goto error;
		}
		handle->in_txn = SWITCH_TRUE;
	}

	if (switch_pgsql_send_query(handle, sql) != SWITCH_PGSQL_SUCCESS) {
		er = strdup("Error sending query!");
		if (switch_pgsql_finish_results(handle) != SWITCH_PGSQL_SUCCESS) {
			db_is_up(handle);
		}
		goto error;
	}

	return SWITCH_PGSQL_SUCCESS;

  error:
	err_str = switch_pgsql_handle_get_error(handle);

	if (zstr(err_str)) {
		if (zstr(er)) {
			err_str = strdup((char *)"SQL ERROR!");
		} else {
			err_str = er;
		}
	} else {
		if (!zstr(er)) {
			free(er);
		}
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
#endif
	return SWITCH_PGSQL_FAIL;
}

SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_handle_exec_detailed(const char *file, const char *func, int line,
																		switch_pgsql_handle_t *handle, const char *sql, char **err)
{
#ifdef SWITCH_HAVE_PGSQL
	if (switch_pgsql_handle_exec_base_detailed(file, func, line, handle, sql, err) == SWITCH_PGSQL_FAIL) {
		goto error;
	}

	return switch_pgsql_finish_results(handle);
  error:
#endif
	return SWITCH_PGSQL_FAIL;
}

SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_handle_callback_exec_detailed(const char *file, const char *func, int line,
																			   switch_pgsql_handle_t *handle,
																			   const char *sql, switch_core_db_callback_func_t callback, void *pdata,
																			   char **err)
{
#ifdef SWITCH_HAVE_PGSQL
	char *err_str = NULL;
	int row = 0, col = 0, err_cnt = 0;
	switch_pgsql_result_t *result = NULL;

	handle->affected_rows = 0;

	switch_assert(callback != NULL);

	if (switch_pgsql_handle_exec_base(handle, sql, err) == SWITCH_PGSQL_FAIL) {
		goto error;
	}
	
	if (switch_pgsql_next_result(handle, &result) == SWITCH_PGSQL_FAIL) {
		err_cnt++;
		err_str = switch_pgsql_handle_get_error(handle);
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
		/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Processing result with %d rows and %d columns.\n", result->rows, result->cols);*/
		for (row = 0; row < result->rows; ++row) {
			char **names;
			char **vals;

			names = calloc(result->cols, sizeof(*names));
			vals = calloc(result->cols, sizeof(*vals));

			switch_assert(names && vals);

			for (col = 0; col < result->cols; ++col) {
				char * tmp;
				int len;

				tmp = PQfname(result->result, col);
				if (tmp) {
					len = strlen(tmp);
					names[col] = malloc(len+1);
					names[col][len] = '\0';
					strncpy(names[col], tmp, len);
					
					len = PQgetlength(result->result, row, col);
					vals[col] = malloc(len+1);
					vals[col][len] = '\0';
					tmp = PQgetvalue(result->result, row, col);
					strncpy(vals[col], tmp, len);
					/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Processing result row %d, col %d: %s => %s\n", row, col, names[col], vals[col]);*/
				} else {
					/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Processing result row %d, col %d.\n", row, col);*/
					switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "ERR: Column number %d out of range\n", col);
				}
			}

			/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Executing callback for row %d...\n", row);*/
			if (callback(pdata, result->cols, vals, names)) {
				switch_pgsql_finish_results(handle); /* Makes sure next call to switch_pgsql_next_result will return NULL */
				row = result->rows;                  /* Makes us exit the for loop */
			}

			for (col = 0; col < result->cols; ++col) {
				free(names[col]);
				free(vals[col]);
			}
			free(names);
			free(vals);
		}
		switch_pgsql_free_result(&result);
		if (switch_pgsql_next_result(handle, &result) == SWITCH_PGSQL_FAIL) {
			err_cnt++;
			err_str = switch_pgsql_handle_get_error(handle);
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

	return SWITCH_PGSQL_SUCCESS;
 error:
#endif
	return SWITCH_PGSQL_FAIL;
}

SWITCH_DECLARE(void) switch_pgsql_handle_destroy(switch_pgsql_handle_t **handlep)
{
#ifdef SWITCH_HAVE_PGSQL

	switch_pgsql_handle_t *handle = NULL;

	if (!handlep) {
		return;
	}
	handle = *handlep;

	if (handle) {
		switch_pgsql_handle_disconnect(handle);

		switch_safe_free(handle->dsn);
		free(handle);
	}
	*handlep = NULL;
#else
	return;
#endif
}

SWITCH_DECLARE(switch_pgsql_state_t) switch_pgsql_handle_get_state(switch_pgsql_handle_t *handle)
{
#ifdef SWITCH_HAVE_PGSQL
	return handle ? handle->state : SWITCH_PGSQL_STATE_INIT;
#else
	return SWITCH_PGSQL_STATE_ERROR;
#endif
}

SWITCH_DECLARE(char *) switch_pgsql_handle_get_error(switch_pgsql_handle_t *handle)
{
#ifdef SWITCH_HAVE_PGSQL
	char * err_str;
	if (!handle) {
		return NULL;
	};
	switch_strdup(err_str, PQerrorMessage(handle->con));
	return err_str;
#else
	return NULL;
#endif
}

SWITCH_DECLARE(int) switch_pgsql_handle_affected_rows(switch_pgsql_handle_t *handle)
{
#ifdef SWITCH_HAVE_PGSQL
	return handle->affected_rows;
#else
	return 0;
#endif
}

SWITCH_DECLARE(switch_bool_t) switch_pgsql_available(void)
{
#ifdef SWITCH_HAVE_PGSQL
	return SWITCH_TRUE;
#else
	return SWITCH_FALSE;
#endif
}

SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_SQLSetAutoCommitAttr(switch_pgsql_handle_t *handle, switch_bool_t on)
{
#ifdef SWITCH_HAVE_PGSQL
	if (on) {
		handle->auto_commit = SWITCH_TRUE;
	} else {
		handle->auto_commit = SWITCH_FALSE;
	}
	return SWITCH_PGSQL_SUCCESS;
#else
	return (switch_pgsql_status_t) SWITCH_FALSE;
#endif
}

SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_flush(switch_pgsql_handle_t *handle)
{
#ifdef SWITCH_HAVE_PGSQL

	PGresult *tmp = NULL;
	int x = 0;

	/* Make sure the query is fully cleared */
	while ((tmp = PQgetResult(handle->con)) != NULL) {
		PQclear(tmp);
		x++;
	}
	
	if (x) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Flushing %d results\n", x);
	}

	return SWITCH_PGSQL_SUCCESS;
#else
	return (switch_pgsql_status_t) SWITCH_FALSE;
#endif
}

SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_SQLEndTran(switch_pgsql_handle_t *handle, switch_bool_t commit)
{
#ifdef SWITCH_HAVE_PGSQL
	char * err_str = NULL;
	if (commit) {
		if(!PQsendQuery(handle->con, "COMMIT")) {
			err_str = switch_pgsql_handle_get_error(handle);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not commit transaction: %s\n", err_str);
			switch_safe_free(err_str);
			return SWITCH_PGSQL_FAIL;
		}
	} else {
		if(!PQsendQuery(handle->con, "ROLLBACK")) {
			err_str = switch_pgsql_handle_get_error(handle);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not rollback transaction: %s\n", err_str);
			switch_safe_free(err_str);
			return SWITCH_PGSQL_FAIL;
		}
	}
	handle->in_txn = SWITCH_FALSE;
	return SWITCH_PGSQL_SUCCESS;
#else
	return (switch_pgsql_status_t) SWITCH_FALSE;
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
