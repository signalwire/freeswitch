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
#include <sqlite3.h>

SWITCH_DECLARE(int) switch_core_db_open(const char *filename, switch_core_db_t **ppDb)
{
	return sqlite3_open(filename, ppDb);
}

SWITCH_DECLARE(int) switch_core_db_close(switch_core_db_t *db)
{
	return sqlite3_close(db);
}

SWITCH_DECLARE(const unsigned char *)switch_core_db_column_text(switch_core_db_stmt_t *stmt, int iCol)
{
	return sqlite3_column_text(stmt, iCol);
}

SWITCH_DECLARE(const char *)switch_core_db_column_name(switch_core_db_stmt_t *stmt, int N)
{
	return sqlite3_column_name(stmt, N);
}

SWITCH_DECLARE(int) switch_core_db_column_count(switch_core_db_stmt_t *pStmt)
{
	return sqlite3_column_count(pStmt);
}

SWITCH_DECLARE(const char *)switch_core_db_errmsg(switch_core_db_t *db)
{
	return sqlite3_errmsg(db);
}

SWITCH_DECLARE(int) switch_core_db_exec(switch_core_db_t *db,
										const char *sql,
										switch_core_db_callback_func_t callback,
										void *data,
										char **errmsg)
{
	return sqlite3_exec(db, sql, callback, data, errmsg);
}

SWITCH_DECLARE(int) switch_core_db_finalize(switch_core_db_stmt_t *pStmt)
{
	return sqlite3_finalize(pStmt);
}

SWITCH_DECLARE(int) switch_core_db_prepare(switch_core_db_t *db,
										   const char *zSql,
										   int nBytes,
										   switch_core_db_stmt_t **ppStmt,
										   const char **pzTail)
{
	return sqlite3_prepare(db, zSql, nBytes, ppStmt, pzTail);
}

SWITCH_DECLARE(int) switch_core_db_step(switch_core_db_stmt_t *stmt)
{
	return sqlite3_step(stmt);
}

SWITCH_DECLARE(void) switch_core_db_free(char *z)
{
	sqlite3_free(z);
}

SWITCH_DECLARE(char *)switch_mprintf(const char *zFormat,...)
{
  va_list ap;
  char *z;
  va_start(ap, zFormat);
  z = sqlite3_vmprintf(zFormat, ap);
  va_end(ap);
  return z;
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
