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
 * switch_odbc.h -- ODBC
 *
 */

#ifndef SWITCH_ODBC_H
#define SWITCH_ODBC_H

#include <switch.h>
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

SWITCH_BEGIN_EXTERN_C struct switch_odbc_handle;

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

SWITCH_DECLARE(switch_odbc_handle_t *) switch_odbc_handle_new(char *dsn, char *username, char *password);
SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_handle_disconnect(switch_odbc_handle_t *handle);
SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_handle_connect(switch_odbc_handle_t *handle);
SWITCH_DECLARE(void) switch_odbc_handle_destroy(switch_odbc_handle_t **handlep);
SWITCH_DECLARE(switch_odbc_state_t) switch_odbc_handle_get_state(switch_odbc_handle_t *handle);
SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_handle_exec(switch_odbc_handle_t *handle, char *sql, SQLHSTMT * rstmt);
SWITCH_DECLARE(switch_odbc_status_t) switch_odbc_handle_callback_exec(switch_odbc_handle_t *handle,
																	  char *sql, switch_core_db_callback_func_t callback, void *pdata);
SWITCH_DECLARE(char *) switch_odbc_handle_get_error(switch_odbc_handle_t *handle, SQLHSTMT stmt);
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
