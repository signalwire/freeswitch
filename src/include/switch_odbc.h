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
 * switch_odbc.h -- ODBC
 *
 */

#ifndef SWITCH_ODBC_H
#define SWITCH_ODBC_H

#include <switch.h>

#define DEFAULT_ODBC_RETRIES 120

SWITCH_BEGIN_EXTERN_C struct switch_odbc_handle;
typedef void *switch_odbc_statement_handle_t;

typedef enum {
	SWITCH_ODBC_STATE_INIT,
	SWITCH_ODBC_STATE_DOWN,
	SWITCH_ODBC_STATE_CONNECTED,
	SWITCH_ODBC_STATE_ERROR
} switch_odbc_state_t;

typedef enum {
	SWITCH_ODBC_SUCCESS = 0,
	SWITCH_ODBC_FAIL = -1
} switch_odbc_status_t;

SWITCH_DECLARE(switch_odbc_handle_t *) switch_odbc_handle_new(const char *dsn, const char *username, const char *password);
SWITCH_DECLARE(void) switch_odbc_set_num_retries(switch_odbc_handle_t *handle, int num_retries);
SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_handle_disconnect(switch_odbc_handle_t *handle);
SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_handle_connect(switch_odbc_handle_t *handle);
SWITCH_DECLARE(void) switch_odbc_handle_destroy(switch_odbc_handle_t **handlep);
SWITCH_DECLARE(switch_odbc_state_t) switch_odbc_handle_get_state(switch_odbc_handle_t *handle);
SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_handle_exec(switch_odbc_handle_t *handle, const char *sql, switch_odbc_statement_handle_t *rstmt,
															 char **err);
SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_handle_exec_string(switch_odbc_handle_t *handle, const char *sql, char *resbuf, size_t len, char **err);
SWITCH_DECLARE(switch_bool_t) switch_odbc_available(void);
SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_SQLSetAutoCommitAttr(switch_odbc_handle_t *handle, switch_bool_t on);
SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_SQLEndTran(switch_odbc_handle_t *handle, switch_bool_t commit);
SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_statement_handle_free(switch_odbc_statement_handle_t *stmt);

/*!
  \brief Execute the sql query and issue a callback for each row returned
  \param file the file from which this function is called
  \param func the function from which this function is called
  \param line the line from which this function is called
  \param handle the ODBC handle
  \param sql the sql string to execute
  \param callback the callback function to execute
  \param pdata the state data passed on each callback invocation
  \return SWITCH_STATUS_SUCCESS if the operation was successful
  \note none
*/
SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_handle_callback_exec_detailed(const char *file, const char *func, int line, switch_odbc_handle_t *handle,
																			   const char *sql, switch_core_db_callback_func_t callback, void *pdata,
																			   char **err);
/*!
  \brief Execute the sql query and issue a callback for each row returned
  \param handle the ODBC handle
  \param sql the sql string to execute
  \param callback the callback function to execute
  \param pdata the state data passed on each callback invocation
  \return SWITCH_STATUS_SUCCESS if the operation was successful
  \note none
*/
#define switch_odbc_handle_callback_exec(handle,  sql,  callback, pdata, err) \
		switch_odbc_handle_callback_exec_detailed(__FILE__, (char * )__SWITCH_FUNC__, __LINE__, \
												  handle, sql, callback, pdata, err)


SWITCH_DECLARE(char *) switch_odbc_handle_get_error(switch_odbc_handle_t *handle, switch_odbc_statement_handle_t stmt);

SWITCH_DECLARE(int) switch_odbc_handle_affected_rows(switch_odbc_handle_t *handle);

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
