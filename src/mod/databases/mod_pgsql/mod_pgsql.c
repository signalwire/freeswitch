/*
* mod_pgsql for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2019, Anthony Minessale II <anthm@freeswitch.org>
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
* Anthony Minessale II <anthm@freeswitch.org>
* Eliot Gable <egable@gmail.com>
* Seven Du <dujinfang@gmail.com>
* Andrey Volk <andywolk@gmail.com>
*
* mod_pgsql.c -- PostgreSQL FreeSWITCH module
*
*/

#define SWITCH_PGSQL_H

#include <switch.h>

#include <libpq-fe.h>

#ifndef _WIN32
#include <poll.h>
#else
#include <WinSock2.h>
#endif

switch_loadable_module_interface_t *MODULE_INTERFACE;
static char *supported_prefixes[4] = { 0 };

SWITCH_MODULE_LOAD_FUNCTION(mod_pgsql_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_pgsql_shutdown);
SWITCH_MODULE_DEFINITION(mod_pgsql, mod_pgsql_load, mod_pgsql_shutdown, NULL);

#define DEFAULT_PGSQL_RETRIES 120

typedef enum {
	SWITCH_PGSQL_STATE_INIT,
	SWITCH_PGSQL_STATE_DOWN,
	SWITCH_PGSQL_STATE_CONNECTED,
	SWITCH_PGSQL_STATE_ERROR
} switch_pgsql_state_t;

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

typedef struct switch_pgsql_handle switch_pgsql_handle_t;
typedef struct switch_pgsql_result switch_pgsql_result_t;

switch_status_t pgsql_handle_connect(switch_pgsql_handle_t *handle);
switch_status_t pgsql_handle_destroy(switch_database_interface_handle_t **dih);
switch_status_t pgsql_cancel_real(const char *file, const char *func, int line, switch_pgsql_handle_t *handle);
switch_status_t pgsql_next_result_timed(switch_pgsql_handle_t *handle, switch_pgsql_result_t **result_out, int msec);

#define pgsql_handle_exec_base(handle, sql, err) pgsql_handle_exec_base_detailed(__FILE__, (char * )__SWITCH_FUNC__, __LINE__, handle, sql, err)
#define pgsql_next_result(h, r) pgsql_next_result_timed(h, r, 10000)
#define pgsql_finish_results(handle) pgsql_finish_results_real(__FILE__, (char * )__SWITCH_FUNC__, __LINE__, handle)
#define pgsql_cancel(handle) pgsql_cancel_real(__FILE__, (char * )__SWITCH_FUNC__, __LINE__, handle)

char * pgsql_handle_get_error(switch_pgsql_handle_t *handle)
{
	char * err_str;

	if (!handle) {
		return NULL;
	}

	switch_strdup(err_str, PQerrorMessage(handle->con));

	return err_str;
}

static int db_is_up(switch_pgsql_handle_t *handle)
{
	int ret = 0;
	switch_event_t *event;
	char *err_str = NULL;
	int max_tries = DEFAULT_PGSQL_RETRIES;
	int code = 0;
	int recon = 0;
	switch_byte_t sanity = 255;

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
	while (--sanity > 0)
	{
		if (PQisBusy(handle->con)) {
			PQconsumeInput(handle->con);
			switch_yield(1);
			continue;
		}
		break;
	}

	if (!sanity) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Can not check DB Connection status: sanity = 0. Reconnecting...\n");
		goto reset;
	}

	if (PQstatus(handle->con) == CONNECTION_BAD) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "PQstatus returned bad connection; reconnecting...\n");
reset:
		handle->state = SWITCH_PGSQL_STATE_ERROR;
		PQreset(handle->con);
		if (PQstatus(handle->con) == CONNECTION_BAD) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "PQstatus returned bad connection -- reconnection failed!\n");
			goto error;
		}
		handle->state = SWITCH_PGSQL_STATE_CONNECTED;
		handle->sock = PQsocket(handle->con);
	}

	ret = 1;
	goto done;

error:
	err_str = pgsql_handle_get_error(handle);

	if (PQstatus(handle->con) == CONNECTION_BAD) {
		handle->state = SWITCH_PGSQL_STATE_ERROR;
		PQreset(handle->con);
		if (PQstatus(handle->con) == CONNECTION_OK) {
			handle->state = SWITCH_PGSQL_STATE_CONNECTED;
			recon = SWITCH_STATUS_SUCCESS;
			handle->sock = PQsocket(handle->con);
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

void pgsql_free_result(switch_pgsql_result_t **result)
{
	if (!*result) {
		return;
	}

	if ((*result)->result) {
		PQclear((*result)->result);
	}
	free(*result);
	*result = NULL;
}

switch_status_t pgsql_finish_results_real(const char* file, const char* func, int line, switch_pgsql_handle_t *handle)
{
	switch_pgsql_result_t *res = NULL;
	switch_status_t final_status = SWITCH_STATUS_SUCCESS;
	int done = 0;

	do {
		pgsql_next_result(handle, &res);
		if (res && res->err && !switch_stristr("already exists", res->err) && !switch_stristr("duplicate key name", res->err)) {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "Error executing query:\n%s\n", res->err);
			final_status = SWITCH_STATUS_FALSE;
		}

		if (!res) {
			done = 1;
		} else if (res->result) {
			char *affected_rows = PQcmdTuples(res->result);

			if (!zstr(affected_rows)) {
				handle->affected_rows = atoi(affected_rows);
			}
		}

		pgsql_free_result(&res);
	} while (!done);

	return final_status;
}

switch_status_t pgsql_handle_affected_rows(switch_database_interface_handle_t *dih, int *affected_rows)
{
	switch_pgsql_handle_t *handle = NULL;

	if (!dih) {
		return SWITCH_STATUS_FALSE;
	}

	handle = dih->handle;

	if (!handle)
		return SWITCH_STATUS_FALSE;

	*affected_rows = handle->affected_rows;

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t pgsql_handle_new(switch_cache_db_database_interface_options_t database_interface_options, switch_database_interface_handle_t **dih)
{
	switch_pgsql_handle_t *new_handle = NULL;	
	
	if (!(*dih = malloc(sizeof(**dih)))) {
		goto err;
	}

	if (!(new_handle = malloc(sizeof(*new_handle)))) {
		goto err;
	}

	memset(new_handle, 0, sizeof(*new_handle));

	if (!strcasecmp(database_interface_options.prefix, "postgresql") || !strcasecmp(database_interface_options.prefix, "postgres")) {
		new_handle->dsn = strdup(database_interface_options.original_dsn);
	} else if (!strcasecmp(database_interface_options.prefix, "pgsql")) {
		new_handle->dsn = strdup(database_interface_options.connection_string);
	}

	if (!new_handle->dsn) {
		goto err;
	}

	new_handle->sock = 0;
	new_handle->state = SWITCH_PGSQL_STATE_INIT;
	new_handle->con = NULL;
	new_handle->affected_rows = 0;
	new_handle->num_retries = DEFAULT_PGSQL_RETRIES;
	new_handle->auto_commit = SWITCH_TRUE;
	new_handle->in_txn = SWITCH_FALSE;

	(*dih)->handle = new_handle;

	if (pgsql_handle_connect(new_handle) != SWITCH_STATUS_SUCCESS) {
		if (pgsql_handle_destroy(dih) != SWITCH_STATUS_SUCCESS) {
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

switch_status_t pgsql_handle_disconnect(switch_pgsql_handle_t *handle)
{
	if (!handle) {
		return SWITCH_STATUS_FALSE;
	}

	if (handle->state == SWITCH_PGSQL_STATE_CONNECTED) {
		PQfinish(handle->con);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Disconnected from [%s]\n", handle->dsn);
	}
	switch_safe_free(handle->sql);
	handle->state = SWITCH_PGSQL_STATE_DOWN;

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t pgsql_handle_connect(switch_pgsql_handle_t *handle)
{	
	if (!handle) {
		return SWITCH_STATUS_FALSE;
	}

	if (handle->state == SWITCH_PGSQL_STATE_CONNECTED) {
		pgsql_handle_disconnect(handle);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Re-connecting %s\n", handle->dsn);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Connecting %s\n", handle->dsn);
	PQinitSSL(0);
	
	handle->con = PQconnectdb(handle->dsn);
	if (PQstatus(handle->con) != CONNECTION_OK) {
		char *err_str;

		if ((err_str = pgsql_handle_get_error(handle))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n", err_str);
			switch_safe_free(err_str);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to connect to the database [%s]\n", handle->dsn);
			pgsql_handle_disconnect(handle);
		}

		return SWITCH_STATUS_FALSE;
	}

	if (PQsetnonblocking(handle->con, 1) == -1) {
		char *err_str;

		if ((err_str = pgsql_handle_get_error(handle))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n", err_str);
			switch_safe_free(err_str);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to setup socket for the database [%s]\n", handle->dsn);
			pgsql_handle_disconnect(handle);
		}

		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Connected to [%s]\n", handle->dsn);
	handle->state = SWITCH_PGSQL_STATE_CONNECTED;
	handle->sock = PQsocket(handle->con);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t pgsql_handle_destroy(switch_database_interface_handle_t **dih)
{
	switch_pgsql_handle_t *handle = NULL;

	if (!dih) {
		return SWITCH_STATUS_FALSE;
	}

	handle = (*dih)->handle;

	if (handle) {
		pgsql_handle_disconnect(handle);

		switch_safe_free(handle->dsn);
		free(handle);
	}

	switch_safe_free(*dih);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t pgsql_flush(switch_pgsql_handle_t *handle)
{
	PGresult *tmp = NULL;
	int x = 0;

	if (!handle) {
		return SWITCH_STATUS_FALSE;
	}

	/* Make sure the query is fully cleared */
	while ((tmp = PQgetResult(handle->con)) != NULL) {
		PQclear(tmp);
		x++;
	}

	if (x) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Flushing %d results\n", x);
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t database_flush(switch_database_interface_handle_t *dih)
{
	switch_pgsql_handle_t *handle;

	if (!dih) {
		return SWITCH_STATUS_FALSE;
	}

	handle = dih->handle;

	return pgsql_flush(handle);
}

switch_status_t pgsql_send_query(switch_pgsql_handle_t *handle, const char* sql)
{
	char *err_str;

	switch_safe_free(handle->sql);
	handle->sql = strdup(sql);
	if (!PQsendQuery(handle->con, sql)) {
		err_str = pgsql_handle_get_error(handle);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to send query (%s) to database: %s\n", sql, err_str);
		switch_safe_free(err_str);
		pgsql_finish_results(handle);
		goto error;
	}

	return SWITCH_STATUS_SUCCESS;
error:
	return SWITCH_STATUS_FALSE;
}

switch_status_t pgsql_handle_exec_base_detailed(const char *file, const char *func, int line,
	switch_pgsql_handle_t *handle, const char *sql, char **err)
{
	char *err_str = NULL;
	char *er = NULL;

	pgsql_flush(handle);
	handle->affected_rows = 0;

	if (!db_is_up(handle)) {
		er = strdup("Database is not up!");
		goto error;
	}

	if (handle->auto_commit == SWITCH_FALSE && handle->in_txn == SWITCH_FALSE) {
		if (pgsql_send_query(handle, "BEGIN") != SWITCH_STATUS_SUCCESS) {
			er = strdup("Error sending BEGIN!");
			if (pgsql_finish_results(handle) != SWITCH_STATUS_SUCCESS) {
				db_is_up(handle); /* If finish_results failed, maybe the db went dead */
			}
			goto error;
		}

		if (pgsql_finish_results(handle) != SWITCH_STATUS_SUCCESS) {
			db_is_up(handle);
			er = strdup("Error sending BEGIN!");
			goto error;
		}
		handle->in_txn = SWITCH_TRUE;
	}

	if (pgsql_send_query(handle, sql) != SWITCH_STATUS_SUCCESS) {
		er = strdup("Error sending query!");
		if (pgsql_finish_results(handle) != SWITCH_STATUS_SUCCESS) {
			db_is_up(handle);
		}
		goto error;
	}

	return SWITCH_STATUS_SUCCESS;

error:
	err_str = pgsql_handle_get_error(handle);

	if (zstr(err_str)) {
		switch_safe_free(err_str);
		if (!er) {
			err_str = strdup((char *)"SQL ERROR!");
		} else {
			err_str = er;
		}
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


switch_status_t pgsql_handle_exec_detailed(const char *file, const char *func, int line,
	switch_pgsql_handle_t *handle, const char *sql, char **err)
{
	if (pgsql_handle_exec_base_detailed(file, func, line, handle, sql, err) == SWITCH_STATUS_FALSE) {
		goto error;
	}

	return pgsql_finish_results(handle);
error:
	return SWITCH_STATUS_FALSE;
}

switch_status_t database_handle_exec_detailed(const char *file, const char *func, int line, 
	switch_database_interface_handle_t *dih, const char *sql, char **err)
{
	switch_pgsql_handle_t *handle;

	if (!dih) {
		return SWITCH_STATUS_FALSE;
	}

	handle = dih->handle;

	return pgsql_handle_exec_detailed(file, func, line, handle, sql, err);
}

switch_status_t database_handle_exec_string(switch_database_interface_handle_t *dih, const char *sql, char *resbuf, size_t len, char **err)
{
	switch_pgsql_handle_t *handle;
	switch_status_t sstatus = SWITCH_STATUS_SUCCESS;
	char *val = NULL;
	switch_pgsql_result_t *result = NULL;

	if (!dih) {
		return SWITCH_STATUS_FALSE;
	}

	handle = dih->handle;

	if (!handle)
		return SWITCH_STATUS_FALSE;

	handle->affected_rows = 0;

	if (pgsql_handle_exec_base(handle, sql, err) == SWITCH_STATUS_FALSE) {
		goto error;
	}

	if (pgsql_next_result(handle, &result) == SWITCH_STATUS_FALSE) {
		goto error;
	}

	if (!result) {
		goto done;
	} else {
		switch (result->status) {
#if POSTGRESQL_MAJOR_VERSION >= 9 && POSTGRESQL_MINOR_VERSION >= 2
		case PGRES_SINGLE_TUPLE:
			/* Added in PostgreSQL 9.2 */
#endif
		case PGRES_COMMAND_OK:
		case PGRES_TUPLES_OK:
			break;
		default:
			sstatus = SWITCH_STATUS_FALSE;
			goto done;
		}
	}

	if (handle->affected_rows <= 0) {
		goto done;
	}

	val = PQgetvalue(result->result, 0, 0);
	strncpy(resbuf, val, len);

done:

	pgsql_free_result(&result);
	if (pgsql_finish_results(handle) != SWITCH_STATUS_SUCCESS) {
		sstatus = SWITCH_STATUS_FALSE;
	}

	return sstatus;

error:

	return SWITCH_STATUS_FALSE;
}

switch_status_t pgsql_next_result_timed(switch_pgsql_handle_t *handle, switch_pgsql_result_t **result_out, int msec)
{
	char *affected_rows = NULL;
	switch_pgsql_result_t *res;
	switch_time_t start;
	switch_time_t ctime;
	unsigned int usec = msec * 1000;
	char *err_str;
#ifndef _WIN32
	struct pollfd fds[2] = { { 0 } };
#else
	fd_set rs, es;
#endif
	int poll_res = 0;

	if (!handle) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "**BUG** Null handle passed to pgsql_next_result.\n");
		return SWITCH_STATUS_FALSE;
	}

	if (PQisBusy(handle->con)) {
		/* Try to consume input that might be waiting right away */
		if (PQconsumeInput(handle->con)) {
			/* And check to see if we have a full result ready for reading */
			if (PQisBusy(handle->con)) {

				/* Wait for a result to become available, up to msec milliseconds */
				start = switch_micro_time_now();
				while ((ctime = switch_micro_time_now()) - start <= usec) {
					switch_time_t wait_time = (usec - (ctime - start)) / 1000;
					/* Wait for the PostgreSQL socket to be ready for data reads. */
#ifndef _WIN32
					fds[0].fd = handle->sock;
					fds[0].events |= POLLIN;
					fds[0].events |= POLLERR;
					fds[0].events |= POLLNVAL;
					fds[0].events |= POLLHUP;
					fds[0].events |= POLLPRI;
					fds[0].events |= POLLRDNORM;
					fds[0].events |= POLLRDBAND;

					poll_res = poll(&fds[0], 1, wait_time);
#else
					struct timeval wait = { (long)wait_time * 1000, 0 };
					FD_ZERO(&rs);
					FD_SET(handle->sock, &rs);
					FD_ZERO(&es);
					FD_SET(handle->sock, &es);
					poll_res = select(0, &rs, 0, &es, &wait);
#endif
					if (poll_res > 0) {
#ifndef _WIN32
						if (fds[0].revents & POLLHUP || fds[0].revents & POLLNVAL) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "PGSQL socket closed or invalid while waiting for result for query (%s)\n", handle->sql);
							goto error;
						} else if (fds[0].revents & POLLERR) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Poll error trying to read PGSQL socket for query (%s)\n", handle->sql);
							goto error;
						} else if (fds[0].revents & POLLIN || fds[0].revents & POLLPRI || fds[0].revents & POLLRDNORM || fds[0].revents & POLLRDBAND) {
#else
						if (FD_ISSET(handle->sock, &rs)) {
#endif						
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
								err_str = pgsql_handle_get_error(handle);
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "An error occurred trying to consume input for query (%s): %s\n", handle->sql, err_str);
								switch_safe_free(err_str);
								pgsql_cancel(handle);
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
					pgsql_cancel(handle);
					goto error;
				}
			}
		} else {
			/* If we had an error trying to consume input, report it and cancel the query. */
			err_str = pgsql_handle_get_error(handle);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "An error occurred trying to consume input for query (%s): %s\n", handle->sql, err_str);
			switch_safe_free(err_str);
			/* pgsql_cancel(handle); */
			goto error;
		}
	}

	/* At this point, we know we can read a full result without blocking. */
	if (!(res = malloc(sizeof(switch_pgsql_result_t)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Malloc failed!\n");
		goto error;
	}

	memset(res, 0, sizeof(switch_pgsql_result_t));

	res->result = PQgetResult(handle->con);
	if (res->result) {
		affected_rows = PQcmdTuples(res->result);
		if (!zstr(affected_rows)) {
			handle->affected_rows = atoi(affected_rows);
		}

		*result_out = res;
		res->status = PQresultStatus(res->result);
		switch (res->status) {
//#if (POSTGRESQL_MAJOR_VERSION == 9 && POSTGRESQL_MINOR_VERSION >= 2) || POSTGRESQL_MAJOR_VERSION > 9
		case PGRES_SINGLE_TUPLE:
			/* Added in PostgreSQL 9.2 */
//#endif
		case PGRES_TUPLES_OK:
		{
			res->rows = PQntuples(res->result);
			res->cols = PQnfields(res->result);
		}
		break;
//#if (POSTGRESQL_MAJOR_VERSION == 9 && POSTGRESQL_MINOR_VERSION >= 1) || POSTGRESQL_MAJOR_VERSION > 9
		case PGRES_COPY_BOTH:
			/* Added in PostgreSQL 9.1 */
//#endif
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

	return SWITCH_STATUS_SUCCESS;
error:

	/* Make sure the failed connection does not have any transactions marked as in progress */
	pgsql_flush(handle);

	/* Try to reconnect to the DB if we were dropped */
	db_is_up(handle);

	return SWITCH_STATUS_FALSE;
}

switch_status_t pgsql_cancel_real(const char *file, const char *func, int line, switch_pgsql_handle_t *handle)
{
	switch_status_t ret = SWITCH_STATUS_SUCCESS;
	char err_buf[256];
	PGcancel *cancel = NULL;

	memset(err_buf, 0, 256);
	cancel = PQgetCancel(handle->con);

	if (!PQcancel(cancel, err_buf, 256)) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CRIT, "Failed to cancel long-running query (%s): %s\n", handle->sql, err_buf);
		ret = SWITCH_STATUS_FALSE;
	}

	PQfreeCancel(cancel);
	pgsql_flush(handle);

	return ret;
}

switch_status_t pgsql_SQLSetAutoCommitAttr(switch_database_interface_handle_t *dih, switch_bool_t on)
{
	switch_pgsql_handle_t *handle;

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

switch_status_t pgsql_SQLEndTran(switch_pgsql_handle_t *handle, switch_bool_t commit)
{
	char * err_str = NULL;

	if (!handle) {
		return SWITCH_STATUS_FALSE;
	}

	if (commit) {
		if (!PQsendQuery(handle->con, "COMMIT")) {
			err_str = pgsql_handle_get_error(handle);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not commit transaction: %s\n", err_str);
			switch_safe_free(err_str);
			return SWITCH_STATUS_FALSE;
		}
	} else {
		if (!PQsendQuery(handle->con, "ROLLBACK")) {
			err_str = pgsql_handle_get_error(handle);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not rollback transaction: %s\n", err_str);
			switch_safe_free(err_str);
			return SWITCH_STATUS_FALSE;
		}
	}
	handle->in_txn = SWITCH_FALSE;

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t database_commit(switch_database_interface_handle_t *dih)
{
	switch_status_t result;

	switch_pgsql_handle_t *handle;

	if (!dih) {
		return SWITCH_STATUS_FALSE;
	}

	handle = dih->handle;

	if (!handle)
		return SWITCH_STATUS_FALSE;

	result = pgsql_SQLEndTran(handle, SWITCH_TRUE);
	result = pgsql_SQLSetAutoCommitAttr(dih, SWITCH_TRUE) && result;
	result = pgsql_finish_results(handle) && result;

	return result;
}

switch_status_t database_rollback(switch_database_interface_handle_t *dih)
{
	switch_pgsql_handle_t *handle;
	switch_status_t result;

	if (!dih) {
		return SWITCH_STATUS_FALSE;
	}

	handle = dih->handle;

	if (!handle)
		return SWITCH_STATUS_FALSE;

	result = pgsql_SQLEndTran(handle, SWITCH_FALSE);
	result = pgsql_SQLSetAutoCommitAttr(dih, SWITCH_TRUE) && result;
	result = pgsql_finish_results(handle) && result;

	return result;
}

switch_status_t pgsql_handle_callback_exec_detailed(const char *file, const char *func, int line,
	switch_database_interface_handle_t *dih, const char *sql, switch_core_db_callback_func_t callback, void *pdata, char **err)
{
	char *err_str = NULL;
	int row = 0, col = 0, err_cnt = 0;
	switch_pgsql_result_t *result = NULL;

	switch_pgsql_handle_t *handle;

	if (!dih) {
		return SWITCH_STATUS_FALSE;
	}

	handle = dih->handle;

	if (!handle) {
		return SWITCH_STATUS_FALSE;
	}

	handle->affected_rows = 0;

	switch_assert(callback != NULL);

	if (pgsql_handle_exec_base(handle, sql, err) == SWITCH_STATUS_FALSE) {
		goto error;
	}

	if (pgsql_next_result(handle, &result) == SWITCH_STATUS_FALSE) {
		err_cnt++;
		err_str = pgsql_handle_get_error(handle);

		if (result && !zstr(result->err)) {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "ERR: [%s]\n[%s]\n", sql, switch_str_nil(result->err));
		}

		if (!zstr(err_str)) {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "ERR: [%s]\n[%s]\n", sql, switch_str_nil(err_str));
		}

		switch_safe_free(err_str);
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
				size_t len;

				tmp = PQfname(result->result, col);
				if (tmp) {
					len = strlen(tmp);
					names[col] = malloc(len + 1);
					snprintf(names[col], len + 1, "%s", tmp);

					len = PQgetlength(result->result, row, col);
					vals[col] = malloc(len + 1);
					tmp = PQgetvalue(result->result, row, col);
					snprintf(vals[col], len + 1, "%s", tmp);
					/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Processing result row %d, col %d: %s => %s\n", row, col, names[col], vals[col]);*/
				} else {
					/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Processing result row %d, col %d.\n", row, col);*/
					switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "ERR: Column number %d out of range\n", col);
				}
			}

			/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Executing callback for row %d...\n", row);*/
			if (callback(pdata, result->cols, vals, names)) {
				pgsql_finish_results(handle); /* Makes sure next call to switch_pgsql_next_result will return NULL */
				row = result->rows;                  /* Makes us exit the for loop */
			}

			for (col = 0; col < result->cols; ++col) {
				free(names[col]);
				free(vals[col]);
			}

			free(names);
			free(vals);
		}

		pgsql_free_result(&result);

		if (pgsql_next_result(handle, &result) == SWITCH_STATUS_FALSE) {
			err_cnt++;
			err_str = pgsql_handle_get_error(handle);

			if (result && !zstr(result->err)) {
				switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "ERR: [%s]\n[%s]\n", sql, switch_str_nil(result->err));
			}

			if (!zstr(err_str)) {
				switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "ERR: [%s]\n[%s]\n", sql, switch_str_nil(err_str));
			}
			switch_safe_free(err_str);
		}
	}

	if (err_cnt) {
		goto error;
	}

	return SWITCH_STATUS_SUCCESS;
error:

	return SWITCH_STATUS_FALSE;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_pgsql_load)
{
	switch_database_interface_t *database_interface;

	supported_prefixes[0] = (char *)"pgsql";
	supported_prefixes[1] = (char *)"postgres";
	supported_prefixes[2] = (char *)"postgresql";

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	MODULE_INTERFACE = *module_interface;

	database_interface = (switch_database_interface_t *)switch_loadable_module_create_interface(*module_interface, SWITCH_DATABASE_INTERFACE);
	database_interface->flags = 0;
	database_interface->interface_name = modname;
	database_interface->prefixes = supported_prefixes;
	database_interface->handle_new = pgsql_handle_new;
	database_interface->handle_destroy = pgsql_handle_destroy;
	database_interface->flush = database_flush;
	database_interface->exec_detailed = database_handle_exec_detailed;
	database_interface->exec_string = database_handle_exec_string;
	database_interface->affected_rows = pgsql_handle_affected_rows;
	database_interface->sql_set_auto_commit_attr = pgsql_SQLSetAutoCommitAttr;
	database_interface->commit = database_commit;
	database_interface->rollback = database_rollback;
	database_interface->callback_exec_detailed = pgsql_handle_callback_exec_detailed;
	
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_pgsql_shutdown)
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
