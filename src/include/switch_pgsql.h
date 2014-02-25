/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * switch_pgsql.h -- PGSQL Driver
 *
 */

#ifndef SWITCH_PGSQL_H
#define SWITCH_PGSQL_H

#include <switch.h>

#define DEFAULT_PGSQL_RETRIES 120

SWITCH_BEGIN_EXTERN_C 

struct switch_pgsql_handle;
struct switch_pgsql_result;


typedef enum {
	SWITCH_PGSQL_STATE_INIT,
	SWITCH_PGSQL_STATE_DOWN,
	SWITCH_PGSQL_STATE_CONNECTED,
	SWITCH_PGSQL_STATE_ERROR
} switch_pgsql_state_t;

typedef enum {
	SWITCH_PGSQL_SUCCESS = 0,
	SWITCH_PGSQL_FAIL = -1
} switch_pgsql_status_t;

/*!
  \brief Create a new handle for the PGSQL connection.
  \param dsn The DSN of the database to connect to. See documentation for PQconnectdb() at 
             http://www.postgresql.org/docs/9.0/static/libpq-connect.html. The DSN *MUST* be
			 prefixed with 'pgsql;' to use the switch_cache_db* functionality. However, the DSN
			 passed to this function directly *MUST NOT* be prefixed with 'pgsql;'.
  \return Returns a pointer to a newly allocated switch_pgsql_handle_t type or NULL on failure.
 */
SWITCH_DECLARE(switch_pgsql_handle_t *) switch_pgsql_handle_new(const char *dsn);

/*!
  \brief Sets the number of retries if the PGSQL connection fails.
  \param handle A fully allocated switch_pgsql_handle_t returned from a call to switch_pgsql_handle_new().
  \param num_retries How many times to retry connecting to the database if this connection fails.
 */
SWITCH_DECLARE(void) switch_pgsql_set_num_retries(switch_pgsql_handle_t *handle, int num_retries);

/*!
  \brief Disconnects a PGSQL connection from the database.
  \param handle The PGSQL database handle to disconnect.
  \return Returns SWITCH_PGSQL_SUCCESS or SWITCH_PGSQL_FAIL.
 */
SWITCH_DECLARE(switch_pgsql_status_t ) switch_pgsql_handle_disconnect(switch_pgsql_handle_t *handle);
#if 0
									   ) /* Emacs formatting issue */
#endif
/*!
  \brief Connect to the database specified by the DSN passed to the switch_pgsql_handle_new() call which 
         initialized this handle.
  \param The database handle to connect to the database.
  \return Returns SWITCH_PGSQL_SUCCESS or SWITCH_PGSQL_FAIL.
 */
SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_handle_connect(switch_pgsql_handle_t *handle);

/*!
 */
SWITCH_DECLARE(void) switch_pgsql_handle_destroy(switch_pgsql_handle_t **handlep);

SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_send_query(switch_pgsql_handle_t *handle, const char* sql);

SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_cancel_real(const char *file, const char *func, int line, switch_pgsql_handle_t *handle);
#define switch_pgsql_cancel(handle) switch_pgsql_cancel_real(__FILE__, (char * )__SWITCH_FUNC__, __LINE__, handle)

SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_next_result_timed(switch_pgsql_handle_t *handle, switch_pgsql_result_t **result_out, int seconds);
#define switch_pgsql_next_result(h, r) switch_pgsql_next_result_timed(h, r, 10000)

SWITCH_DECLARE(void) switch_pgsql_free_result(switch_pgsql_result_t **result);

SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_finish_results_real(const char* file, const char *func, int line, switch_pgsql_handle_t *handle);
#define switch_pgsql_finish_results(handle) switch_pgsql_finish_results_real(__FILE__, (char * )__SWITCH_FUNC__, __LINE__, handle)

SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_handle_exec_base_detailed(const char *file, const char *func, int line,
																			 switch_pgsql_handle_t *handle, const char *sql, char **err);
#define switch_pgsql_handle_exec_base(handle, sql, err) switch_pgsql_handle_exec_base_detailed(__FILE__, (char * )__SWITCH_FUNC__, __LINE__, handle, sql, err)

SWITCH_DECLARE(switch_pgsql_state_t) switch_pgsql_handle_get_state(switch_pgsql_handle_t *handle);

SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_handle_exec_detailed(const char *file, const char *func, int line,
															   switch_pgsql_handle_t *handle, const char *sql, char **err);
#define switch_pgsql_handle_exec(handle, sql, err) switch_pgsql_handle_exec_detailed(__FILE__, (char * )__SWITCH_FUNC__, __LINE__, handle, sql, err)

SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_handle_exec_string_detailed(const char *file, const char *func, int line,
																			   switch_pgsql_handle_t *handle, const char *sql, char *resbuf, size_t len, char **err);
#define switch_pgsql_handle_exec_string(handle, sql, resbuf, len, err) switch_pgsql_handle_exec_string_detailed(__FILE__, (char * )__SWITCH_FUNC__, __LINE__, handle, sql, resbuf, len, err)

SWITCH_DECLARE(switch_bool_t) switch_pgsql_available(void);
SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_SQLSetAutoCommitAttr(switch_pgsql_handle_t *handle, switch_bool_t on);
SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_SQLEndTran(switch_pgsql_handle_t *handle, switch_bool_t commit);

/*!
  \brief Execute the sql query and issue a callback for each row returned
  \param file the file from which this function is called
  \param func the function from which this function is called
  \param line the line from which this function is called
  \param handle the PGSQL handle
  \param sql the sql string to execute
  \param callback the callback function to execute
  \param pdata the state data passed on each callback invocation
  \return SWITCH_STATUS_SUCCESS if the operation was successful
  \note none
*/
SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_handle_callback_exec_detailed(const char *file, const char *func, int line, switch_pgsql_handle_t *handle,
																			   const char *sql, switch_core_db_callback_func_t callback, void *pdata,
																			   char **err);
/*!
  \brief Execute the sql query and issue a callback for each row returned
  \param handle the PGSQL handle
  \param sql the sql string to execute
  \param callback the callback function to execute
  \param pdata the state data passed on each callback invocation
  \return SWITCH_STATUS_SUCCESS if the operation was successful
  \note none
*/
#define switch_pgsql_handle_callback_exec(handle,  sql,  callback, pdata, err) \
		switch_pgsql_handle_callback_exec_detailed(__FILE__, (char * )__SWITCH_FUNC__, __LINE__, \
												  handle, sql, callback, pdata, err)


SWITCH_DECLARE(char *) switch_pgsql_handle_get_error(switch_pgsql_handle_t *handle);

SWITCH_DECLARE(int) switch_pgsql_handle_affected_rows(switch_pgsql_handle_t *handle);
SWITCH_DECLARE(switch_pgsql_status_t) switch_pgsql_flush(switch_pgsql_handle_t *handle);


SWITCH_END_EXTERN_C
#endif
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
