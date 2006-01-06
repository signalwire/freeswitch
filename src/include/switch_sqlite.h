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
 * switch_sqlite.h -- Sqlite Header
 *
 */
/*! \file switch_sqlite.h
    \brief Sqlite Header
*/
#ifndef SWITCH_SQLITE_H
#define SWITCH_SQLITE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sqlite3.h>

/**
 * @defgroup switch_sqlite_top Brought To You By SQLite
 * @ingroup FREESWITCH 
 * @{
 */

/**
 * @defgroup switch_sqlite Database Routines
 * @ingroup switch_sqlite_top 
 * @{
 */

/**
 * Each open sqlite database is represented by an instance of the
 * following opaque structure.
*/
typedef sqlite3 switch_core_db;

/**
** Aggregate functions use the following routine to allocate
** a structure for storing their state.  The first time this routine
** is called for a particular aggregate, a new structure of size nBytes
** is allocated, zeroed, and returned.  On subsequent calls (for the
** same aggregate instance) the same buffer is returned.  The implementation
** of the aggregate can use the returned buffer to accumulate data.
**
** The buffer allocated is freed automatically by SQLite.
*/
DoxyDefine(void *switch_core_db_aggregate_context(sqlite3_context*, int nBytes);)
#define switch_core_db_aggregate_context sqlite3_aggregate_context

/**
** /return the number of calls to xStep for a particular
** aggregate function instance.  The current call to xStep counts so this
** routine always returns at least 1.
*/
DoxyDefine(int switch_core_db_aggregate_count(sqlite3_context*);)
#define switch_core_db_aggregate_count sqlite3_aggregate_count

/**
** In the SQL strings input to sqlite3_prepare() and sqlite3_prepare16(),
** one or more literals can be replace by parameters "?" or ":AAA" or
** "$VVV" where AAA is an identifer and VVV is a variable name according
** to the syntax rules of the TCL programming language.
** The value of these parameters (also called "host parameter names") can
** be set using the routines listed below.
**
** In every case, the first parameter is a pointer to the sqlite3_stmt
** structure returned from sqlite3_prepare().  The second parameter is the
** index of the parameter.  The first parameter as an index of 1.  For
** named parameters (":AAA" or "$VVV") you can use 
** sqlite3_bind_parameter_index() to get the correct index value given
** the parameters name.  If the same named parameter occurs more than
** once, it is assigned the same index each time.
**
** The fifth parameter to sqlite3_bind_blob(), sqlite3_bind_text(), and
** sqlite3_bind_text16() is a destructor used to dispose of the BLOB or
** text after SQLite has finished with it.  If the fifth argument is the
** special value SQLITE_STATIC, then the library assumes that the information
** is in static, unmanaged space and does not need to be freed.  If the
** fifth argument has the value SQLITE_TRANSIENT, then SQLite makes its
** own private copy of the data.
**
** The sqlite3_bind_* routine must be called before sqlite3_step() after
** an sqlite3_prepare() or sqlite3_reset().  Unbound parameterss are
** interpreted as NULL.
*/

DoxyDefine(int switch_core_db_bind_blob(sqlite3_stmt*, int, const void*, int n, void(*)(void*));)
#define switch_core_db_bind_blob sqlite3_bind_blob
DoxyDefine(int switch_core_db_bind_double(sqlite3_stmt*, int, double);)
#define switch_core_db_bind_double sqlite3_bind_double
DoxyDefine(int switch_core_db_bind_int(sqlite3_stmt*, int, int);)
#define switch_core_db_bind_int sqlite3_bind_int
DoxyDefine(int switch_core_db_bind_int64(sqlite3_stmt*, int, sqlite_int64);)
#define switch_core_db_bind_int64 sqlite3_bind_int64
DoxyDefine(int switch_core_db_bind_null(sqlite3_stmt*, int);)
#define switch_core_db_bind_null sqlite3_bind_null
DoxyDefine(int switch_core_db_bind_text(sqlite3_stmt*, int, const char*, int n, void(*)(void*));)
#define switch_core_db_bind_text sqlite3_bind_text
DoxyDefine(int switch_core_db_bind_text16(sqlite3_stmt*, int, const void*, int, void(*)(void*));)
#define switch_core_db_bind_text16 sqlite3_bind_text16
DoxyDefine(int switch_core_db_bind_value(sqlite3_stmt*, int, const sqlite3_value*);)
#define switch_core_db_bind_value sqlite3_bind_value

/**
** Return the number of parameters in a compiled SQL statement.  This
** routine was added to support DBD::SQLite.
*/
DoxyDefine(int switch_core_db_bind_parameter_count(sqlite3_stmt*);)
#define switch_core_db_bind_parameter_count sqlite3_bind_parameter_count

/**
** Return the index of a parameter with the given name.  The name
** must match exactly.  If no parameter with the given name is found,
** return 0.
*/
DoxyDefine(int switch_core_db_bind_parameter_index(sqlite3_stmt*, const char *zName);)
#define switch_core_db_bind_parameter_index sqlite3_bind_parameter_index

/**
** Return the name of the i-th parameter.  Ordinary parameters "?" are
** nameless and a NULL is returned.  For parameters of the form :AAA or
** $VVV the complete text of the parameter name is returned, including
** the initial ":" or "$".  NULL is returned if the index is out of range.
*/
DoxyDefine(const char *switch_core_db_bind_parameter_name(sqlite3_stmt*, int);)
#define switch_core_db_bind_parameter_name sqlite3_bind_parameter_name

/**
** This routine identifies a callback function that is invoked
** whenever an attempt is made to open a database table that is
** currently locked by another process or thread.  If the busy callback
** is NULL, then sqlite3_exec() returns SQLITE_BUSY immediately if
** it finds a locked table.  If the busy callback is not NULL, then
** sqlite3_exec() invokes the callback with three arguments.  The
** second argument is the name of the locked table and the third
** argument is the number of times the table has been busy.  If the
** busy callback returns 0, then sqlite3_exec() immediately returns
** SQLITE_BUSY.  If the callback returns non-zero, then sqlite3_exec()
** tries to open the table again and the cycle repeats.
**
** The default busy callback is NULL.
**
** Sqlite is re-entrant, so the busy handler may start a new query. 
** (It is not clear why anyone would every want to do this, but it
** is allowed, in theory.)  But the busy handler may not close the
** database.  Closing the database from a busy handler will delete 
** data structures out from under the executing query and will 
** probably result in a coredump.
*/
DoxyDefine(int switch_core_db_busy_handler(switch_core_db*, int(*)(void*,int), void*);)
#define switch_core_db_busy_handler sqlite3_busy_handler

/**
** This routine sets a busy handler that sleeps for a while when a
** table is locked.  The handler will sleep multiple times until 
** at least "ms" milleseconds of sleeping have been done.  After
** "ms" milleseconds of sleeping, the handler returns 0 which
** causes sqlite3_exec() to return SQLITE_BUSY.
**
** Calling this routine with an argument less than or equal to zero
** turns off all busy handlers.
*/
DoxyDefine(int switch_core_db_busy_timeout(switch_core_db*, int ms);)
#define switch_core_db_busy_timeout sqlite3_busy_timeout

/**
** This function returns the number of database rows that were changed
** (or inserted or deleted) by the most recent called sqlite3_exec().
**
** All changes are counted, even if they were later undone by a
** ROLLBACK or ABORT.  Except, changes associated with creating and
** dropping tables are not counted.
**
** If a callback invokes sqlite3_exec() recursively, then the changes
** in the inner, recursive call are counted together with the changes
** in the outer call.
**
** SQLite implements the command "DELETE FROM table" without a WHERE clause
** by dropping and recreating the table.  (This is much faster than going
** through and deleting individual elements form the table.)  Because of
** this optimization, the change count for "DELETE FROM table" will be
** zero regardless of the number of elements that were originally in the
** table. To get an accurate count of the number of rows deleted, use
** "DELETE FROM table WHERE 1" instead.
*/
DoxyDefine(int switch_core_db_changes(switch_core_db*);)
#define switch_core_db_changes sqlite3_changes

/**
** A function to close the database.
**
** Call this function with a pointer to a structure that was previously
** returned from sqlite3_open() and the corresponding database will by closed.
**
** All SQL statements prepared using sqlite3_prepare() or
** sqlite3_prepare16() must be deallocated using sqlite3_finalize() before
** this routine is called. Otherwise, SQLITE_BUSY is returned and the
** database connection remains open.
*/
DoxyDefine(int switch_core_db_close(switch_core_db *);)
#define switch_core_db_close sqlite3_close

/**
** To avoid having to register all collation sequences before a database
** can be used, a single callback function may be registered with the
** database handle to be called whenever an undefined collation sequence is
** required.
**
** If the function is registered using the sqlite3_collation_needed() API,
** then it is passed the names of undefined collation sequences as strings
** encoded in UTF-8. If sqlite3_collation_needed16() is used, the names
** are passed as UTF-16 in machine native byte order. A call to either
** function replaces any existing callback.
**
** When the user-function is invoked, the first argument passed is a copy
** of the second argument to sqlite3_collation_needed() or
** sqlite3_collation_needed16(). The second argument is the database
** handle. The third argument is one of SQLITE_UTF8, SQLITE_UTF16BE or
** SQLITE_UTF16LE, indicating the most desirable form of the collation
** sequence function required. The fourth parameter is the name of the
** required collation sequence.
**
** The collation sequence is returned to SQLite by a collation-needed
** callback using the sqlite3_create_collation() or
** sqlite3_create_collation16() APIs, described above.
*/
DoxyDefine(int switch_core_db_collation_needed(
  switch_core_db*, 
  void*, 
  void(*)(void*,switch_core_db*,int eTextRep,const char*)
);)
#define switch_core_db_collation_needed sqlite3_collation_needed
DoxyDefine(int switch_core_db_collation_needed16(
  switch_core_db*, 
  void*,
  void(*)(void*,switch_core_db*,int eTextRep,const void*)
);)
#define switch_core_db_collation_needed16 sqlite3_collation_needed16

/**
** The next group of routines returns information about the information
** in a single column of the current result row of a query.  In every
** case the first parameter is a pointer to the SQL statement that is being
** executed (the sqlite_stmt* that was returned from sqlite3_prepare()) and
** the second argument is the index of the column for which information 
** should be returned.  iCol is zero-indexed.  The left-most column as an
** index of 0.
**
** If the SQL statement is not currently point to a valid row, or if the
** the colulmn index is out of range, the result is undefined.
**
** These routines attempt to convert the value where appropriate.  For
** example, if the internal representation is FLOAT and a text result
** is requested, sprintf() is used internally to do the conversion
** automatically.  The following table details the conversions that
** are applied:
**
**    Internal Type    Requested Type     Conversion
**    -------------    --------------    --------------------------
**       NULL             INTEGER         Result is 0
**       NULL             FLOAT           Result is 0.0
**       NULL             TEXT            Result is an empty string
**       NULL             BLOB            Result is a zero-length BLOB
**       INTEGER          FLOAT           Convert from integer to float
**       INTEGER          TEXT            ASCII rendering of the integer
**       INTEGER          BLOB            Same as for INTEGER->TEXT
**       FLOAT            INTEGER         Convert from float to integer
**       FLOAT            TEXT            ASCII rendering of the float
**       FLOAT            BLOB            Same as FLOAT->TEXT
**       TEXT             INTEGER         Use atoi()
**       TEXT             FLOAT           Use atof()
**       TEXT             BLOB            No change
**       BLOB             INTEGER         Convert to TEXT then use atoi()
**       BLOB             FLOAT           Convert to TEXT then use atof()
**       BLOB             TEXT            Add a \000 terminator if needed
**
** The following access routines are provided:
**
** _type()     Return the datatype of the result.  This is one of
**             SQLITE_INTEGER, SQLITE_FLOAT, SQLITE_TEXT, SQLITE_BLOB,
**             or SQLITE_NULL.
** _blob()     Return the value of a BLOB.
** _bytes()    Return the number of bytes in a BLOB value or the number
**             of bytes in a TEXT value represented as UTF-8.  The \000
**             terminator is included in the byte count for TEXT values.
** _bytes16()  Return the number of bytes in a BLOB value or the number
**             of bytes in a TEXT value represented as UTF-16.  The \u0000
**             terminator is included in the byte count for TEXT values.
** _double()   Return a FLOAT value.
** _int()      Return an INTEGER value in the host computer's native
**             integer representation.  This might be either a 32- or 64-bit
**             integer depending on the host.
** _int64()    Return an INTEGER value as a 64-bit signed integer.
** _text()     Return the value as UTF-8 text.
** _text16()   Return the value as UTF-16 text.
*/
DoxyDefine(const void *switch_core_db_column_blob(sqlite3_stmt*, int iCol);)
#define switch_core_db_column_blob sqlite3_column_blob
DoxyDefine(int switch_core_db_column_bytes(sqlite3_stmt*, int iCol);)
#define switch_core_db_column_bytes sqlite3_column_bytes
DoxyDefine(int switch_core_db_column_bytes16(sqlite3_stmt*, int iCol);)
#define switch_core_db_column_bytes16 sqlite3_column_bytes16
DoxyDefine(double switch_core_db_column_double(sqlite3_stmt*, int iCol);)
#define switch_core_db_column_double sqlite3_column_double
DoxyDefine(int switch_core_db_column_int(sqlite3_stmt*, int iCol);)
#define switch_core_db_column_int sqlite3_column_int
DoxyDefine(sqlite_int64 switch_core_db_column_int64(sqlite3_stmt*, int iCol);)
#define switch_core_db_column_int64 sqlite3_column_int64
DoxyDefine(const unsigned char *switch_core_db_column_text(sqlite3_stmt*, int iCol);)
#define switch_core_db_column_text sqlite3_column_text
DoxyDefine(const void *switch_core_db_column_text16(sqlite3_stmt*, int iCol);)
#define switch_core_db_column_text16 sqlite3_column_text16
DoxyDefine(int switch_core_db_column_type(sqlite3_stmt*, int iCol);)
#define switch_core_db_column_type sqlite3_column_type

/**
** The first parameter is a compiled SQL statement. This function returns
** the column heading for the Nth column of that statement, where N is the
** second function parameter.  The string returned is UTF-8 for
** sqlite3_column_name() and UTF-16 for sqlite3_column_name16().
*/
DoxyDefine(const char *switch_core_db_column_name(sqlite3_stmt*,int);)
#define switch_core_db_column_name sqlite3_column_name
DoxyDefine(const void *switch_core_db_column_name16(sqlite3_stmt*,int);)
#define switch_core_db_column_name16 sqlite3_column_name16

/**
** Return the number of columns in the result set returned by the compiled
** SQL statement. This routine returns 0 if pStmt is an SQL statement
** that does not return data (for example an UPDATE).
*/
DoxyDefine(int switch_core_db_column_count(sqlite3_stmt *pStmt);)
#define switch_core_db_column_count sqlite3_column_count

/**
** The first parameter is a compiled SQL statement. If this statement
** is a SELECT statement, the Nth column of the returned result set 
** of the SELECT is a table column then the declared type of the table
** column is returned. If the Nth column of the result set is not at table
** column, then a NULL pointer is returned. The returned string is always
** UTF-8 encoded. For example, in the database schema:
**
** CREATE TABLE t1(c1 VARIANT);
**
** And the following statement compiled:
**
** SELECT c1 + 1, 0 FROM t1;
**
** Then this routine would return the string "VARIANT" for the second
** result column (i==1), and a NULL pointer for the first result column
** (i==0).
*/
DoxyDefine(const char *switch_core_db_column_decltype(sqlite3_stmt *, int i);)
#define switch_core_db_column_decltype sqlite3_column_decltype

/**
** The first parameter is a compiled SQL statement. If this statement
** is a SELECT statement, the Nth column of the returned result set 
** of the SELECT is a table column then the declared type of the table
** column is returned. If the Nth column of the result set is not at table
** column, then a NULL pointer is returned. The returned string is always
** UTF-16 encoded. For example, in the database schema:
**
** CREATE TABLE t1(c1 INTEGER);
**
** And the following statement compiled:
**
** SELECT c1 + 1, 0 FROM t1;
**
** Then this routine would return the string "INTEGER" for the second
** result column (i==1), and a NULL pointer for the first result column
** (i==0).
*/
DoxyDefine(const void *switch_core_db_column_decltype16(sqlite3_stmt*,int);)
#define switch_core_db_column_decltype16 sqlite3_column_decltype16

#define switch_core_db_commit_hook sqlite3_commit_hook
#define switch_core_db_complete sqlite3_complete
#define switch_core_db_complete16 sqlite3_complete16
#define switch_core_db_create_collation sqlite3_create_collation
#define switch_core_db_create_collation16 sqlite3_create_collation16
#define switch_core_db_create_function sqlite3_create_function
#define switch_core_db_create_function16 sqlite3_create_function16
#define switch_core_db_data_count sqlite3_data_count
#define switch_core_db_db_handle sqlite3_db_handle
#define switch_core_db_errcode sqlite3_errcode
#define switch_core_db_errmsg sqlite3_errmsg
#define switch_core_db_errmsg16 sqlite3_errmsg16
#define switch_core_db_exec sqlite3_exec
#define switch_core_db_expired sqlite3_expired
#define switch_core_db_finalize sqlite3_finalize
#define switch_core_db_free sqlite3_free
#define switch_core_db_free_table sqlite3_free_table
#define switch_core_db_get_autocommit sqlite3_get_autocommit
#define switch_core_db_get_auxdata sqlite3_get_auxdata
#define switch_core_db_get_table sqlite3_get_table
#define switch_core_db_get_table_cb sqlite3_get_table_cb
#define switch_core_db_global_recover sqlite3_global_recover
#define switch_core_db_interrupt sqlite3_interrupt
#define switch_core_db_interrupt_count sqlite3_interrupt_count
#define switch_core_db_last_insert_rowid sqlite3_last_insert_rowid
#define switch_core_db_libversion sqlite3_libversion
#define switch_core_db_libversion_number sqlite3_libversion_number
#define switch_core_db_malloc_failed sqlite3_malloc_failed
#define switch_core_db_mprintf sqlite3_mprintf
#define switch_core_db_open sqlite3_open
#define switch_core_db_open16 sqlite3_open16
#define switch_core_db_opentemp_count sqlite3_opentemp_count
#define switch_core_db_os_trace sqlite3_os_trace
#define switch_core_db_prepare sqlite3_prepare
#define switch_core_db_prepare16 sqlite3_prepare16
#define switch_core_db_profile sqlite3_profile
#define switch_core_db_progress_handler sqlite3_progress_handler
#define switch_core_db_reset sqlite3_reset
#define switch_core_db_result_blob sqlite3_result_blob
#define switch_core_db_result_double sqlite3_result_double
#define switch_core_db_result_error sqlite3_result_error
#define switch_core_db_result_error16 sqlite3_result_error16
#define switch_core_db_result_int sqlite3_result_int
#define switch_core_db_result_int64 sqlite3_result_int64
#define switch_core_db_result_null sqlite3_result_null
#define switch_core_db_result_text sqlite3_result_text
#define switch_core_db_result_text16 sqlite3_result_text16
#define switch_core_db_result_text16be sqlite3_result_text16be
#define switch_core_db_result_text16le sqlite3_result_text16le
#define switch_core_db_result_value sqlite3_result_value
#define switch_core_db_search_count sqlite3_search_count
#define switch_core_db_set_authorizer sqlite3_set_authorizer
#define switch_core_db_set_auxdata sqlite3_set_auxdata
#define switch_core_db_snprintf sqlite3_snprintf
#define switch_core_db_sort_count sqlite3_sort_count
#define switch_core_db_step sqlite3_step
#define switch_core_db_temp_directory sqlite3_temp_directory
#define switch_core_db_total_changes sqlite3_total_changes
#define switch_core_db_trace sqlite3_trace
#define switch_core_db_transfer_bindings sqlite3_transfer_bindings
#define switch_core_db_user_data sqlite3_user_data
#define switch_core_db_value_blob sqlite3_value_blob
#define switch_core_db_value_bytes sqlite3_value_bytes
#define switch_core_db_value_bytes16 sqlite3_value_bytes16
#define switch_core_db_value_double sqlite3_value_double
#define switch_core_db_value_int sqlite3_value_int
#define switch_core_db_value_int64 sqlite3_value_int64
#define switch_core_db_value_text sqlite3_value_text
#define switch_core_db_value_text16 sqlite3_value_text16
#define switch_core_db_value_text16be sqlite3_value_text16be
#define switch_core_db_value_text16le sqlite3_value_text16le
#define switch_core_db_value_type sqlite3_value_type
#define switch_core_db_version sqlite3_version
#define switch_core_db_vmprintf sqlite3_vmprintf

/** @} */
/** @} */



#ifdef __cplusplus
}
#endif

#endif
