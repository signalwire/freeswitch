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
 * Michael Jerris <mike@jerris.com>
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
typedef sqlite3_stmt switch_core_db_stmt;
/**
 * Aggregate functions use the following routine to allocate
 * a structure for storing their state.  The first time this routine
 * is called for a particular aggregate, a new structure of size nBytes
 * is allocated, zeroed, and returned.  On subsequent calls (for the
 * same aggregate instance) the same buffer is returned.  The implementation
 * of the aggregate can use the returned buffer to accumulate data.
 *
 * The buffer allocated is freed automatically by SQLite.
 */
DoxyDefine(void *switch_core_db_aggregate_context(sqlite3_context*, int nBytes);)
#define switch_core_db_aggregate_context sqlite3_aggregate_context

/**
 * /return the number of calls to xStep for a particular
 * aggregate function instance.  The current call to xStep counts so this
 * routine always returns at least 1.
 */
DoxyDefine(int switch_core_db_aggregate_count(sqlite3_context*);)
#define switch_core_db_aggregate_count sqlite3_aggregate_count

/**
 * In the SQL strings input to switch_core_db_prepare(),
 * one or more literals can be replace by parameters "?" or ":AAA" or
 * "$VVV" where AAA is an identifer and VVV is a variable name according
 * to the syntax rules of the TCL programming language.
 * The value of these parameters (also called "host parameter names") can
 * be set using the routines listed below.
 *
 * In every case, the first parameter is a pointer to the sqlite3_stmt
 * structure returned from switch_core_db_prepare().  The second parameter is the
 * index of the parameter.  The first parameter as an index of 1.  For
 * named parameters (":AAA" or "$VVV") you can use 
 * sqlite3_bind_parameter_index() to get the correct index value given
 * the parameters name.  If the same named parameter occurs more than
 * once, it is assigned the same index each time.
 *
 * The fifth parameter to sqlite3_bind_blob(), sqlite3_bind_text(), and
 * sqlite3_bind_text16() is a destructor used to dispose of the BLOB or
 * text after SQLite has finished with it.  If the fifth argument is the
 * special value SQLITE_STATIC, then the library assumes that the information
 * is in static, unmanaged space and does not need to be freed.  If the
 * fifth argument has the value SQLITE_TRANSIENT, then SQLite makes its
 * own private copy of the data.
 *
 * The sqlite3_bind_* routine must be called before switch_core_db_step() after
 * an switch_core_db_prepare() or sqlite3_reset().  Unbound parameterss are
 * interpreted as NULL.
 */
DoxyDefine(int switch_core_db_bind_blob(sqlite3_stmt*, int, const void*, int n, void(*)(void*));)
#define switch_core_db_bind_blob sqlite3_bind_blob

/**
 * In the SQL strings input to switch_core_db_prepare(),
 * one or more literals can be replace by parameters "?" or ":AAA" or
 * "$VVV" where AAA is an identifer and VVV is a variable name according
 * to the syntax rules of the TCL programming language.
 * The value of these parameters (also called "host parameter names") can
 * be set using the routines listed below.
 *
 * In every case, the first parameter is a pointer to the sqlite3_stmt
 * structure returned from switch_core_db_prepare().  The second parameter is the
 * index of the parameter.  The first parameter as an index of 1.  For
 * named parameters (":AAA" or "$VVV") you can use 
 * sqlite3_bind_parameter_index() to get the correct index value given
 * the parameters name.  If the same named parameter occurs more than
 * once, it is assigned the same index each time.
 *
 * The sqlite3_bind_* routine must be called before switch_core_db_step() after
 * an switch_core_db_prepare() or sqlite3_reset().  Unbound parameterss are
 * interpreted as NULL.
 */
DoxyDefine(int switch_core_db_bind_double(sqlite3_stmt*, int, double);)
#define switch_core_db_bind_double sqlite3_bind_double

/**
 * In the SQL strings input to switch_core_db_prepare(),
 * one or more literals can be replace by parameters "?" or ":AAA" or
 * "$VVV" where AAA is an identifer and VVV is a variable name according
 * to the syntax rules of the TCL programming language.
 * The value of these parameters (also called "host parameter names") can
 * be set using the routines listed below.
 *
 * In every case, the first parameter is a pointer to the sqlite3_stmt
 * structure returned from switch_core_db_prepare().  The second parameter is the
 * index of the parameter.  The first parameter as an index of 1.  For
 * named parameters (":AAA" or "$VVV") you can use 
 * sqlite3_bind_parameter_index() to get the correct index value given
 * the parameters name.  If the same named parameter occurs more than
 * once, it is assigned the same index each time.
 *
 * The sqlite3_bind_* routine must be called before switch_core_db_step() after
 * an switch_core_db_prepare() or sqlite3_reset().  Unbound parameterss are
 * interpreted as NULL.
 */
DoxyDefine(int switch_core_db_bind_int(sqlite3_stmt*, int, int);)
#define switch_core_db_bind_int sqlite3_bind_int

/**
 * In the SQL strings input to switch_core_db_prepare(),
 * one or more literals can be replace by parameters "?" or ":AAA" or
 * "$VVV" where AAA is an identifer and VVV is a variable name according
 * to the syntax rules of the TCL programming language.
 * The value of these parameters (also called "host parameter names") can
 * be set using the routines listed below.
 *
 * In every case, the first parameter is a pointer to the sqlite3_stmt
 * structure returned from switch_core_db_prepare().  The second parameter is the
 * index of the parameter.  The first parameter as an index of 1.  For
 * named parameters (":AAA" or "$VVV") you can use 
 * sqlite3_bind_parameter_index() to get the correct index value given
 * the parameters name.  If the same named parameter occurs more than
 * once, it is assigned the same index each time.
 *
 * The sqlite3_bind_* routine must be called before switch_core_db_step() after
 * an switch_core_db_prepare() or sqlite3_reset().  Unbound parameterss are
 * interpreted as NULL.
 */
DoxyDefine(int switch_core_db_bind_int64(sqlite3_stmt*, int, sqlite_int64);)
#define switch_core_db_bind_int64 sqlite3_bind_int64

/**
 * In the SQL strings input to switch_core_db_prepare(),
 * one or more literals can be replace by parameters "?" or ":AAA" or
 * "$VVV" where AAA is an identifer and VVV is a variable name according
 * to the syntax rules of the TCL programming language.
 * The value of these parameters (also called "host parameter names") can
 * be set using the routines listed below.
 *
 * In every case, the first parameter is a pointer to the sqlite3_stmt
 * structure returned from switch_core_db_prepare().  The second parameter is the
 * index of the parameter.  The first parameter as an index of 1.  For
 * named parameters (":AAA" or "$VVV") you can use 
 * sqlite3_bind_parameter_index() to get the correct index value given
 * the parameters name.  If the same named parameter occurs more than
 * once, it is assigned the same index each time.
 *
 * The sqlite3_bind_* routine must be called before switch_core_db_step() after
 * an switch_core_db_prepare() or sqlite3_reset().  Unbound parameterss are
 * interpreted as NULL.
 */
DoxyDefine(int switch_core_db_bind_null(sqlite3_stmt*, int);)
#define switch_core_db_bind_null sqlite3_bind_null

/**
 * In the SQL strings input to switch_core_db_prepare(),
 * one or more literals can be replace by parameters "?" or ":AAA" or
 * "$VVV" where AAA is an identifer and VVV is a variable name according
 * to the syntax rules of the TCL programming language.
 * The value of these parameters (also called "host parameter names") can
 * be set using the routines listed below.
 *
 * In every case, the first parameter is a pointer to the sqlite3_stmt
 * structure returned from switch_core_db_prepare().  The second parameter is the
 * index of the parameter.  The first parameter as an index of 1.  For
 * named parameters (":AAA" or "$VVV") you can use 
 * sqlite3_bind_parameter_index() to get the correct index value given
 * the parameters name.  If the same named parameter occurs more than
 * once, it is assigned the same index each time.
 *
 * The fifth parameter to sqlite3_bind_blob(), sqlite3_bind_text(), and
 * sqlite3_bind_text16() is a destructor used to dispose of the BLOB or
 * text after SQLite has finished with it.  If the fifth argument is the
 * special value SQLITE_STATIC, then the library assumes that the information
 * is in static, unmanaged space and does not need to be freed.  If the
 * fifth argument has the value SQLITE_TRANSIENT, then SQLite makes its
 * own private copy of the data.
 *
 * The sqlite3_bind_* routine must be called before switch_core_db_step() after
 * an switch_core_db_prepare() or sqlite3_reset().  Unbound parameterss are
 * interpreted as NULL.
 */
DoxyDefine(int switch_core_db_bind_text(sqlite3_stmt*, int, const char*, int n, void(*)(void*));)
#define switch_core_db_bind_text sqlite3_bind_text

/**
 * In the SQL strings input to switch_core_db_prepare(),
 * one or more literals can be replace by parameters "?" or ":AAA" or
 * "$VVV" where AAA is an identifer and VVV is a variable name according
 * to the syntax rules of the TCL programming language.
 * The value of these parameters (also called "host parameter names") can
 * be set using the routines listed below.
 *
 * In every case, the first parameter is a pointer to the sqlite3_stmt
 * structure returned from switch_core_db_prepare().  The second parameter is the
 * index of the parameter.  The first parameter as an index of 1.  For
 * named parameters (":AAA" or "$VVV") you can use 
 * sqlite3_bind_parameter_index() to get the correct index value given
 * the parameters name.  If the same named parameter occurs more than
 * once, it is assigned the same index each time.
 *
 * The fifth parameter to sqlite3_bind_blob(), sqlite3_bind_text(), and
 * sqlite3_bind_text16() is a destructor used to dispose of the BLOB or
 * text after SQLite has finished with it.  If the fifth argument is the
 * special value SQLITE_STATIC, then the library assumes that the information
 * is in static, unmanaged space and does not need to be freed.  If the
 * fifth argument has the value SQLITE_TRANSIENT, then SQLite makes its
 * own private copy of the data.
 *
 * The sqlite3_bind_* routine must be called before switch_core_db_step() after
 * an switch_core_db_prepare() or sqlite3_reset().  Unbound parameterss are
 * interpreted as NULL.
 */
DoxyDefine(int switch_core_db_bind_text16(sqlite3_stmt*, int, const void*, int, void(*)(void*));)
#define switch_core_db_bind_text16 sqlite3_bind_text16

/**
 * In the SQL strings input to switch_core_db_prepare(),
 * one or more literals can be replace by parameters "?" or ":AAA" or
 * "$VVV" where AAA is an identifer and VVV is a variable name according
 * to the syntax rules of the TCL programming language.
 * The value of these parameters (also called "host parameter names") can
 * be set using the routines listed below.
 *
 * In every case, the first parameter is a pointer to the sqlite3_stmt
 * structure returned from switch_core_db_prepare().  The second parameter is the
 * index of the parameter.  The first parameter as an index of 1.  For
 * named parameters (":AAA" or "$VVV") you can use 
 * sqlite3_bind_parameter_index() to get the correct index value given
 * the parameters name.  If the same named parameter occurs more than
 * once, it is assigned the same index each time.
 *
 * The sqlite3_bind_* routine must be called before switch_core_db_step() after
 * an switch_core_db_prepare() or sqlite3_reset().  Unbound parameterss are
 * interpreted as NULL.
 */
DoxyDefine(int switch_core_db_bind_value(sqlite3_stmt*, int, const sqlite3_value*);)
#define switch_core_db_bind_value sqlite3_bind_value

/**
 * @return The number of parameters in a compiled SQL statement.
 * @remark This routine was added to support DBD::SQLite.
 */
DoxyDefine(int switch_core_db_bind_parameter_count(sqlite3_stmt*);)
#define switch_core_db_bind_parameter_count sqlite3_bind_parameter_count

/**
 * @return the index of a parameter with the given name.  If no parameter with the 
 * given name is found, return 0.
 * @remark The name must match exactly.
 */
DoxyDefine(int switch_core_db_bind_parameter_index(sqlite3_stmt*, const char *zName);)
#define switch_core_db_bind_parameter_index sqlite3_bind_parameter_index

/**
 * @return the name of the i-th parameter.  
 * @remark Ordinary parameters "?" are
 * nameless and a NULL is returned.  For parameters of the form :AAA or
 * $VVV the complete text of the parameter name is returned, including
 * the initial ":" or "$".  NULL is returned if the index is out of range.
 */
DoxyDefine(const char *switch_core_db_bind_parameter_name(sqlite3_stmt*, int);)
#define switch_core_db_bind_parameter_name sqlite3_bind_parameter_name

/**
 * This routine identifies a callback function that is invoked
 * whenever an attempt is made to open a database table that is
 * currently locked by another process or thread.  If the busy callback
 * is NULL, then sqlite3_exec() returns SQLITE_BUSY immediately if
 * it finds a locked table.  If the busy callback is not NULL, then
 * sqlite3_exec() invokes the callback with three arguments.  The
 * second argument is the name of the locked table and the third
 * argument is the number of times the table has been busy.  If the
 * busy callback returns 0, then sqlite3_exec() immediately returns
 * SQLITE_BUSY.  If the callback returns non-zero, then sqlite3_exec()
 * tries to open the table again and the cycle repeats.
 *
 * The default busy callback is NULL.
 *
 * Sqlite is re-entrant, so the busy handler may start a new query. 
 * (It is not clear why anyone would every want to do this, but it
 * is allowed, in theory.)  But the busy handler may not close the
 * database.  Closing the database from a busy handler will delete 
 * data structures out from under the executing query and will 
 * probably result in a coredump.
 */
DoxyDefine(int switch_core_db_busy_handler(switch_core_db*, int(*)(void*,int), void*);)
#define switch_core_db_busy_handler sqlite3_busy_handler

/**
 * This routine sets a busy handler that sleeps for a while when a
 * table is locked.  The handler will sleep multiple times until 
 * at least "ms" milleseconds of sleeping have been done.  After
 * "ms" milleseconds of sleeping, the handler returns 0 which
 * causes sqlite3_exec() to return SQLITE_BUSY.
 *
 * Calling this routine with an argument less than or equal to zero
 * turns off all busy handlers.
 */
DoxyDefine(int switch_core_db_busy_timeout(switch_core_db*, int ms);)
#define switch_core_db_busy_timeout sqlite3_busy_timeout

/**
 * This function returns the number of database rows that were changed
 * (or inserted or deleted) by the most recent called sqlite3_exec().
 *
 * All changes are counted, even if they were later undone by a
 * ROLLBACK or ABORT.  Except, changes associated with creating and
 * dropping tables are not counted.
 *
 * If a callback invokes sqlite3_exec() recursively, then the changes
 * in the inner, recursive call are counted together with the changes
 * in the outer call.
 *
 * SQLite implements the command "DELETE FROM table" without a WHERE clause
 * by dropping and recreating the table.  (This is much faster than going
 * through and deleting individual elements form the table.)  Because of
 * this optimization, the change count for "DELETE FROM table" will be
 * zero regardless of the number of elements that were originally in the
 * table. To get an accurate count of the number of rows deleted, use
 * "DELETE FROM table WHERE 1" instead.
 */
DoxyDefine(int switch_core_db_changes(switch_core_db*);)
#define switch_core_db_changes sqlite3_changes

/**
 * A function to close the database.
 *
 * Call this function with a pointer to a structure that was previously
 * returned from sqlite3_open() and the corresponding database will by closed.
 *
 * All SQL statements prepared using switch_core_db_prepare()
 * must be deallocated using sqlite3_finalize() before
 * this routine is called. Otherwise, SQLITE_BUSY is returned and the
 * database connection remains open.
 */
DoxyDefine(int switch_core_db_close(switch_core_db *);)
#define switch_core_db_close sqlite3_close

/**
 * To avoid having to register all collation sequences before a database
 * can be used, a single callback function may be registered with the
 * database handle to be called whenever an undefined collation sequence is
 * required.
 *
 * The function is passed the names of undefined collation sequences as 
 * strings encoded in UTF-8. A call to the function replaces any existing callback.
 *
 * When the user-function is invoked, the first argument passed is a copy
 * of the second argument to sqlite3_collation_needed().  The second argument is the database
 * handle. The third argument is one of SQLITE_UTF8, SQLITE_UTF16BE or
 * SQLITE_UTF16LE, indicating the most desirable form of the collation
 * sequence function required. The fourth parameter is the name of the
 * required collation sequence.
 *
 * The collation sequence is returned to SQLite by a collation-needed
 * callback using the sqlite3_create_collation() API, described above.
 */
DoxyDefine(int switch_core_db_collation_needed(
  switch_core_db*, 
  void*, 
  void(*)(void*,switch_core_db*,int eTextRep,const char*)
);)
#define switch_core_db_collation_needed sqlite3_collation_needed

/**
 * The next group of routines returns information about the information
 * in a single column of the current result row of a query.
 *
 * @param stmt a pointer to the SQL statement that is being
 * executed (the sqlite_stmt* that was returned from switch_core_db_prepare())
 *
 * @param iCol the index of the column for which information 
 * should be returned.  iCol is zero-indexed.  The left-most column as an
 * index of 0.
 *
 * @remark If the SQL statement is not currently point to a valid row, or if the
 * the colulmn index is out of range, the result is undefined.
 *
 * These routines attempt to convert the value where appropriate.  For
 * example, if the internal representation is FLOAT and a text result
 * is requested, sprintf() is used internally to do the conversion
 * automatically.  The following table details the conversions that
 * are applied:
 *
 *    Internal Type    Requested Type     Conversion
 *    -------------    --------------    --------------------------
 *       NULL             INTEGER         Result is 0
 *       NULL             FLOAT           Result is 0.0
 *       NULL             TEXT            Result is an empty string
 *       NULL             BLOB            Result is a zero-length BLOB
 *       INTEGER          FLOAT           Convert from integer to float
 *       INTEGER          TEXT            ASCII rendering of the integer
 *       INTEGER          BLOB            Same as for INTEGER->TEXT
 *       FLOAT            INTEGER         Convert from float to integer
 *       FLOAT            TEXT            ASCII rendering of the float
 *       FLOAT            BLOB            Same as FLOAT->TEXT
 *       TEXT             INTEGER         Use atoi()
 *       TEXT             FLOAT           Use atof()
 *       TEXT             BLOB            No change
 *       BLOB             INTEGER         Convert to TEXT then use atoi()
 *       BLOB             FLOAT           Convert to TEXT then use atof()
 *       BLOB             TEXT            Add a "\000" terminator if needed
 *
 * @return the value of a BLOB.
 */
DoxyDefine(const void *switch_core_db_column_blob(sqlite3_stmt *stmt, int iCol);)
#define switch_core_db_column_blob sqlite3_column_blob

/**
 * The next group of routines returns information about the information
 * in a single column of the current result row of a query.  In every
 * case the first parameter is a pointer to the SQL statement that is being
 * executed (the sqlite_stmt* that was returned from switch_core_db_prepare()) and
 * the second argument is the index of the column for which information 
 * should be returned.  iCol is zero-indexed.  The left-most column as an
 * index of 0.
 *
 * If the SQL statement is not currently point to a valid row, or if the
 * the colulmn index is out of range, the result is undefined.
 *
 * These routines attempt to convert the value where appropriate.  For
 * example, if the internal representation is FLOAT and a text result
 * is requested, sprintf() is used internally to do the conversion
 * automatically.  The following table details the conversions that
 * are applied:
 *
 *    Internal Type    Requested Type     Conversion
 *    -------------    --------------    --------------------------
 *       NULL             INTEGER         Result is 0
 *       NULL             FLOAT           Result is 0.0
 *       NULL             TEXT            Result is an empty string
 *       NULL             BLOB            Result is a zero-length BLOB
 *       INTEGER          FLOAT           Convert from integer to float
 *       INTEGER          TEXT            ASCII rendering of the integer
 *       INTEGER          BLOB            Same as for INTEGER->TEXT
 *       FLOAT            INTEGER         Convert from float to integer
 *       FLOAT            TEXT            ASCII rendering of the float
 *       FLOAT            BLOB            Same as FLOAT->TEXT
 *       TEXT             INTEGER         Use atoi()
 *       TEXT             FLOAT           Use atof()
 *       TEXT             BLOB            No change
 *       BLOB             INTEGER         Convert to TEXT then use atoi()
 *       BLOB             FLOAT           Convert to TEXT then use atof()
 *       BLOB             TEXT            Add a "\000" terminator if needed
 *
 * Return the number of bytes in a BLOB value or the number of bytes in a 
 * TEXT value represented as UTF-8.  The "\000" terminator is included in the 
 * byte count for TEXT values.
 */
DoxyDefine(int switch_core_db_column_bytes(sqlite3_stmt*, int iCol);)
#define switch_core_db_column_bytes sqlite3_column_bytes

/**
 * The next group of routines returns information about the information
 * in a single column of the current result row of a query.  In every
 * case the first parameter is a pointer to the SQL statement that is being
 * executed (the sqlite_stmt* that was returned from switch_core_db_prepare()) and
 * the second argument is the index of the column for which information 
 * should be returned.  iCol is zero-indexed.  The left-most column as an
 * index of 0.
 *
 * If the SQL statement is not currently point to a valid row, or if the
 * the colulmn index is out of range, the result is undefined.
 *
 * These routines attempt to convert the value where appropriate.  For
 * example, if the internal representation is FLOAT and a text result
 * is requested, sprintf() is used internally to do the conversion
 * automatically.  The following table details the conversions that
 * are applied:
 *
 *    Internal Type    Requested Type     Conversion
 *    -------------    --------------    --------------------------
 *       NULL             INTEGER         Result is 0
 *       NULL             FLOAT           Result is 0.0
 *       NULL             TEXT            Result is an empty string
 *       NULL             BLOB            Result is a zero-length BLOB
 *       INTEGER          FLOAT           Convert from integer to float
 *       INTEGER          TEXT            ASCII rendering of the integer
 *       INTEGER          BLOB            Same as for INTEGER->TEXT
 *       FLOAT            INTEGER         Convert from float to integer
 *       FLOAT            TEXT            ASCII rendering of the float
 *       FLOAT            BLOB            Same as FLOAT->TEXT
 *       TEXT             INTEGER         Use atoi()
 *       TEXT             FLOAT           Use atof()
 *       TEXT             BLOB            No change
 *       BLOB             INTEGER         Convert to TEXT then use atoi()
 *       BLOB             FLOAT           Convert to TEXT then use atof()
 *       BLOB             TEXT            Add a "\000" terminator if needed
 *
 * Return the number of bytes in a BLOB value or the number of bytes in a 
 * TEXT value represented as UTF-16.  The "\u0000" terminator is included in 
 * the byte count for TEXT values.
 */
DoxyDefine(int switch_core_db_column_bytes16(sqlite3_stmt*, int iCol);)
#define switch_core_db_column_bytes16 sqlite3_column_bytes16

/**
 * The next group of routines returns information about the information
 * in a single column of the current result row of a query.  In every
 * case the first parameter is a pointer to the SQL statement that is being
 * executed (the sqlite_stmt* that was returned from switch_core_db_prepare()) and
 * the second argument is the index of the column for which information 
 * should be returned.  iCol is zero-indexed.  The left-most column as an
 * index of 0.
 *
 * If the SQL statement is not currently point to a valid row, or if the
 * the colulmn index is out of range, the result is undefined.
 *
 * These routines attempt to convert the value where appropriate.  For
 * example, if the internal representation is FLOAT and a text result
 * is requested, sprintf() is used internally to do the conversion
 * automatically.  The following table details the conversions that
 * are applied:
 *
 *    Internal Type    Requested Type     Conversion
 *    -------------    --------------    --------------------------
 *       NULL             INTEGER         Result is 0
 *       NULL             FLOAT           Result is 0.0
 *       NULL             TEXT            Result is an empty string
 *       NULL             BLOB            Result is a zero-length BLOB
 *       INTEGER          FLOAT           Convert from integer to float
 *       INTEGER          TEXT            ASCII rendering of the integer
 *       INTEGER          BLOB            Same as for INTEGER->TEXT
 *       FLOAT            INTEGER         Convert from float to integer
 *       FLOAT            TEXT            ASCII rendering of the float
 *       FLOAT            BLOB            Same as FLOAT->TEXT
 *       TEXT             INTEGER         Use atoi()
 *       TEXT             FLOAT           Use atof()
 *       TEXT             BLOB            No change
 *       BLOB             INTEGER         Convert to TEXT then use atoi()
 *       BLOB             FLOAT           Convert to TEXT then use atof()
 *       BLOB             TEXT            Add a "\000" terminator if needed
 *
 *  Return a FLOAT value.
 */
DoxyDefine(double switch_core_db_column_double(sqlite3_stmt*, int iCol);)
#define switch_core_db_column_double sqlite3_column_double

/**
 * The next group of routines returns information about the information
 * in a single column of the current result row of a query.  In every
 * case the first parameter is a pointer to the SQL statement that is being
 * executed (the sqlite_stmt* that was returned from switch_core_db_prepare()) and
 * the second argument is the index of the column for which information 
 * should be returned.  iCol is zero-indexed.  The left-most column as an
 * index of 0.
 *
 * If the SQL statement is not currently point to a valid row, or if the
 * the colulmn index is out of range, the result is undefined.
 *
 * These routines attempt to convert the value where appropriate.  For
 * example, if the internal representation is FLOAT and a text result
 * is requested, sprintf() is used internally to do the conversion
 * automatically.  The following table details the conversions that
 * are applied:
 *
 *    Internal Type    Requested Type     Conversion
 *    -------------    --------------    --------------------------
 *       NULL             INTEGER         Result is 0
 *       NULL             FLOAT           Result is 0.0
 *       NULL             TEXT            Result is an empty string
 *       NULL             BLOB            Result is a zero-length BLOB
 *       INTEGER          FLOAT           Convert from integer to float
 *       INTEGER          TEXT            ASCII rendering of the integer
 *       INTEGER          BLOB            Same as for INTEGER->TEXT
 *       FLOAT            INTEGER         Convert from float to integer
 *       FLOAT            TEXT            ASCII rendering of the float
 *       FLOAT            BLOB            Same as FLOAT->TEXT
 *       TEXT             INTEGER         Use atoi()
 *       TEXT             FLOAT           Use atof()
 *       TEXT             BLOB            No change
 *       BLOB             INTEGER         Convert to TEXT then use atoi()
 *       BLOB             FLOAT           Convert to TEXT then use atof()
 *       BLOB             TEXT            Add a "\000" terminator if needed
 *
 *  Return an INTEGER value in the host computer's native integer representation.  
 *  This might be either a 32- or 64-bit integer depending on the host.
 */
DoxyDefine(int switch_core_db_column_int(sqlite3_stmt*, int iCol);)
#define switch_core_db_column_int sqlite3_column_int

/**
 * The next group of routines returns information about the information
 * in a single column of the current result row of a query.  In every
 * case the first parameter is a pointer to the SQL statement that is being
 * executed (the sqlite_stmt* that was returned from switch_core_db_prepare()) and
 * the second argument is the index of the column for which information 
 * should be returned.  iCol is zero-indexed.  The left-most column as an
 * index of 0.
 *
 * If the SQL statement is not currently point to a valid row, or if the
 * the colulmn index is out of range, the result is undefined.
 *
 * These routines attempt to convert the value where appropriate.  For
 * example, if the internal representation is FLOAT and a text result
 * is requested, sprintf() is used internally to do the conversion
 * automatically.  The following table details the conversions that
 * are applied:
 *
 *    Internal Type    Requested Type     Conversion
 *    -------------    --------------    --------------------------
 *       NULL             INTEGER         Result is 0
 *       NULL             FLOAT           Result is 0.0
 *       NULL             TEXT            Result is an empty string
 *       NULL             BLOB            Result is a zero-length BLOB
 *       INTEGER          FLOAT           Convert from integer to float
 *       INTEGER          TEXT            ASCII rendering of the integer
 *       INTEGER          BLOB            Same as for INTEGER->TEXT
 *       FLOAT            INTEGER         Convert from float to integer
 *       FLOAT            TEXT            ASCII rendering of the float
 *       FLOAT            BLOB            Same as FLOAT->TEXT
 *       TEXT             INTEGER         Use atoi()
 *       TEXT             FLOAT           Use atof()
 *       TEXT             BLOB            No change
 *       BLOB             INTEGER         Convert to TEXT then use atoi()
 *       BLOB             FLOAT           Convert to TEXT then use atof()
 *       BLOB             TEXT            Add a "\000" terminator if needed
 *
 * Return an INTEGER value as a 64-bit signed integer.
 */
DoxyDefine(sqlite_int64 switch_core_db_column_int64(sqlite3_stmt*, int iCol);)
#define switch_core_db_column_int64 sqlite3_column_int64

/**
 * The next group of routines returns information about the information
 * in a single column of the current result row of a query.  In every
 * case the first parameter is a pointer to the SQL statement that is being
 * executed (the sqlite_stmt* that was returned from switch_core_db_prepare()) and
 * the second argument is the index of the column for which information 
 * should be returned.  iCol is zero-indexed.  The left-most column as an
 * index of 0.
 *
 * If the SQL statement is not currently point to a valid row, or if the
 * the colulmn index is out of range, the result is undefined.
 *
 * These routines attempt to convert the value where appropriate.  For
 * example, if the internal representation is FLOAT and a text result
 * is requested, sprintf() is used internally to do the conversion
 * automatically.  The following table details the conversions that
 * are applied:
 *
 *    Internal Type    Requested Type     Conversion
 *    -------------    --------------    --------------------------
 *       NULL             INTEGER         Result is 0
 *       NULL             FLOAT           Result is 0.0
 *       NULL             TEXT            Result is an empty string
 *       NULL             BLOB            Result is a zero-length BLOB
 *       INTEGER          FLOAT           Convert from integer to float
 *       INTEGER          TEXT            ASCII rendering of the integer
 *       INTEGER          BLOB            Same as for INTEGER->TEXT
 *       FLOAT            INTEGER         Convert from float to integer
 *       FLOAT            TEXT            ASCII rendering of the float
 *       FLOAT            BLOB            Same as FLOAT->TEXT
 *       TEXT             INTEGER         Use atoi()
 *       TEXT             FLOAT           Use atof()
 *       TEXT             BLOB            No change
 *       BLOB             INTEGER         Convert to TEXT then use atoi()
 *       BLOB             FLOAT           Convert to TEXT then use atof()
 *       BLOB             TEXT            Add a "\000" terminator if needed
 *
 *  Return the value as UTF-8 text.
 */
DoxyDefine(const unsigned char *switch_core_db_column_text(sqlite3_stmt*, int iCol);)
#define switch_core_db_column_text sqlite3_column_text

/**
 * The next group of routines returns information about the information
 * in a single column of the current result row of a query.  In every
 * case the first parameter is a pointer to the SQL statement that is being
 * executed (the sqlite_stmt* that was returned from switch_core_db_prepare()) and
 * the second argument is the index of the column for which information 
 * should be returned.  iCol is zero-indexed.  The left-most column as an
 * index of 0.
 *
 * If the SQL statement is not currently point to a valid row, or if the
 * the colulmn index is out of range, the result is undefined.
 *
 * These routines attempt to convert the value where appropriate.  For
 * example, if the internal representation is FLOAT and a text result
 * is requested, sprintf() is used internally to do the conversion
 * automatically.  The following table details the conversions that
 * are applied:
 *
 *    Internal Type    Requested Type     Conversion
 *    -------------    --------------    --------------------------
 *       NULL             INTEGER         Result is 0
 *       NULL             FLOAT           Result is 0.0
 *       NULL             TEXT            Result is an empty string
 *       NULL             BLOB            Result is a zero-length BLOB
 *       INTEGER          FLOAT           Convert from integer to float
 *       INTEGER          TEXT            ASCII rendering of the integer
 *       INTEGER          BLOB            Same as for INTEGER->TEXT
 *       FLOAT            INTEGER         Convert from float to integer
 *       FLOAT            TEXT            ASCII rendering of the float
 *       FLOAT            BLOB            Same as FLOAT->TEXT
 *       TEXT             INTEGER         Use atoi()
 *       TEXT             FLOAT           Use atof()
 *       TEXT             BLOB            No change
 *       BLOB             INTEGER         Convert to TEXT then use atoi()
 *       BLOB             FLOAT           Convert to TEXT then use atof()
 *       BLOB             TEXT            Add a "\000" terminator if needed
 *
 * Return the value as UTF-16 text.
 */
DoxyDefine(const void *switch_core_db_column_text16(sqlite3_stmt*, int iCol);)
#define switch_core_db_column_text16 sqlite3_column_text16

/**
 * The next group of routines returns information about the information
 * in a single column of the current result row of a query.  In every
 * case the first parameter is a pointer to the SQL statement that is being
 * executed (the sqlite_stmt* that was returned from switch_core_db_prepare()) and
 * the second argument is the index of the column for which information 
 * should be returned.  iCol is zero-indexed.  The left-most column as an
 * index of 0.
 *
 * If the SQL statement is not currently point to a valid row, or if the
 * the colulmn index is out of range, the result is undefined.
 *
 * These routines attempt to convert the value where appropriate.  For
 * example, if the internal representation is FLOAT and a text result
 * is requested, sprintf() is used internally to do the conversion
 * automatically.  The following table details the conversions that
 * are applied:
 *
 *    Internal Type    Requested Type     Conversion
 *    -------------    --------------    --------------------------
 *       NULL             INTEGER         Result is 0
 *       NULL             FLOAT           Result is 0.0
 *       NULL             TEXT            Result is an empty string
 *       NULL             BLOB            Result is a zero-length BLOB
 *       INTEGER          FLOAT           Convert from integer to float
 *       INTEGER          TEXT            ASCII rendering of the integer
 *       INTEGER          BLOB            Same as for INTEGER->TEXT
 *       FLOAT            INTEGER         Convert from float to integer
 *       FLOAT            TEXT            ASCII rendering of the float
 *       FLOAT            BLOB            Same as FLOAT->TEXT
 *       TEXT             INTEGER         Use atoi()
 *       TEXT             FLOAT           Use atof()
 *       TEXT             BLOB            No change
 *       BLOB             INTEGER         Convert to TEXT then use atoi()
 *       BLOB             FLOAT           Convert to TEXT then use atof()
 *       BLOB             TEXT            Add a "\000" terminator if needed
 *
 * ReturnS the datatype of the result.  This is one of
 * SQLITE_INTEGER, SQLITE_FLOAT, SQLITE_TEXT, SQLITE_BLOB, or SQLITE_NULL.
 */
DoxyDefine(int switch_core_db_column_type(sqlite3_stmt*, int iCol);)
#define switch_core_db_column_type sqlite3_column_type

/**
 * The first parameter is a compiled SQL statement. This function returns
 * the column heading for the Nth column of that statement, where N is the
 * second function parameter.  The string returned is UTF-8.
 */
DoxyDefine(const char *switch_core_db_column_name(sqlite3_stmt*,int);)
#define switch_core_db_column_name sqlite3_column_name

/**
 * Return the number of columns in the result set returned by the compiled
 * SQL statement. This routine returns 0 if pStmt is an SQL statement
 * that does not return data (for example an UPDATE).
 */
DoxyDefine(int switch_core_db_column_count(sqlite3_stmt *pStmt);)
#define switch_core_db_column_count sqlite3_column_count

/**
 * The first parameter is a compiled SQL statement. If this statement
 * is a SELECT statement, the Nth column of the returned result set 
 * of the SELECT is a table column then the declared type of the table
 * column is returned. If the Nth column of the result set is not at table
 * column, then a NULL pointer is returned. The returned string is always
 * UTF-8 encoded. For example, in the database schema:
 *
 * CREATE TABLE t1(c1 VARIANT);
 *
 * And the following statement compiled:
 *
 * SELECT c1 + 1, 0 FROM t1;
 *
 * Then this routine would return the string "VARIANT" for the second
 * result column (i==1), and a NULL pointer for the first result column
 * (i==0).
 */
DoxyDefine(const char *switch_core_db_column_decltype(sqlite3_stmt *, int i);)
#define switch_core_db_column_decltype sqlite3_column_decltype

/**
 * The first parameter is a compiled SQL statement. If this statement
 * is a SELECT statement, the Nth column of the returned result set 
 * of the SELECT is a table column then the declared type of the table
 * column is returned. If the Nth column of the result set is not at table
 * column, then a NULL pointer is returned. The returned string is always
 * UTF-16 encoded. For example, in the database schema:
 *
 * CREATE TABLE t1(c1 INTEGER);
 *
 * And the following statement compiled:
 *
 * SELECT c1 + 1, 0 FROM t1;
 *
 * Then this routine would return the string "INTEGER" for the second
 * result column (i==1), and a NULL pointer for the first result column
 * (i==0).
 */
DoxyDefine(const void *switch_core_db_column_decltype16(sqlite3_stmt*,int);)
#define switch_core_db_column_decltype16 sqlite3_column_decltype16

/**
 * Register a callback function to be invoked whenever a new transaction
 * is committed.  The pArg argument is passed through to the callback.
 * callback.  If the callback function returns non-zero, then the commit
 * is converted into a rollback.
 *
 * If another function was previously registered, its pArg value is returned.
 * Otherwise NULL is returned.
 *
 * Registering a NULL function disables the callback.
 *
 ****** THIS IS AN EXPERIMENTAL API AND IS SUBJECT TO CHANGE ******
 */
DoxyDefine(void *switch_core_db_commit_hook(switch_core_db*, int(*)(void*), void*);)
#define switch_core_db_commit_hook sqlite3_commit_hook

/**
 * This functions return true if the given input string comprises
 * one or more complete SQL statements. The parameter must be a nul-terminated 
 * UTF-8 string. 
 *
 * The algorithm is simple.  If the last token other than spaces
 * and comments is a semicolon, then return true.  otherwise return
 * false.
 */
DoxyDefine(int switch_core_db_complete(const char *sql);)
#define switch_core_db_complete sqlite3_complete

/**
 * This function is used to add new collation sequences to the
 * sqlite3 handle specified as the first argument. 
 *
 * The name of the new collation sequence is specified as a UTF-8 string
 * and the name is passed as the second function argument.
 *
 * The third argument must be one of the constants SQLITE_UTF8,
 * SQLITE_UTF16LE or SQLITE_UTF16BE, indicating that the user-supplied
 * routine expects to be passed pointers to strings encoded using UTF-8,
 * UTF-16 little-endian or UTF-16 big-endian respectively.
 *
 * A pointer to the user supplied routine must be passed as the fifth
 * argument. If it is NULL, this is the same as deleting the collation
 * sequence (so that SQLite cannot call it anymore). Each time the user
 * supplied function is invoked, it is passed a copy of the void* passed as
 * the fourth argument.
 *
 * The remaining arguments to the user-supplied routine are two strings,
 * each represented by a [length, data] pair and encoded in the encoding
 * that was passed as the third argument when the collation sequence was
 * registered. The user routine should return negative, zero or positive if
 * the first string is less than, equal to, or greater than the second
 * string. i.e. (STRING1 - STRING2).
 */
DoxyDefine(int switch_core_db_create_collation(
  switch_core_db*, 
  const char *zName, 
  int eTextRep, 
  void*,
  int(*xCompare)(void*,int,const void*,int,const void*)
);)
#define switch_core_db_create_collation sqlite3_create_collation

/**
 * The following function is used to add user functions or aggregates
 * implemented in C to the SQL langauge interpreted by SQLite. The
 * name of the (scalar) function or aggregate, is encoded in UTF-8.
 *
 * The first argument is the database handle that the new function or
 * aggregate is to be added to. If a single program uses more than one
 * database handle internally, then user functions or aggregates must 
 * be added individually to each database handle with which they will be
 * used.
 *
 * The third parameter is the number of arguments that the function or
 * aggregate takes. If this parameter is negative, then the function or
 * aggregate may take any number of arguments.
 *
 * The fourth parameter is one of SQLITE_UTF* values defined below,
 * indicating the encoding that the function is most likely to handle
 * values in.  This does not change the behaviour of the programming
 * interface. However, if two versions of the same function are registered
 * with different encoding values, SQLite invokes the version likely to
 * minimize conversions between text encodings.
 *
 * The seventh, eighth and ninth parameters, xFunc, xStep and xFinal, are
 * pointers to user implemented C functions that implement the user
 * function or aggregate. A scalar function requires an implementation of
 * the xFunc callback only, NULL pointers should be passed as the xStep
 * and xFinal parameters. An aggregate function requires an implementation
 * of xStep and xFinal, but NULL should be passed for xFunc. To delete an
 * existing user function or aggregate, pass NULL for all three function
 * callback. Specifying an inconstent set of callback values, such as an
 * xFunc and an xFinal, or an xStep but no xFinal, SQLITE_ERROR is
 * returned.
 */
DoxyDefine(int switch_core_db_create_function(
  switch_core_db *,
  const char *zFunctionName,
  int nArg,
  int eTextRep,
  void*,
  void (*xFunc)(sqlite3_context*,int,sqlite3_value**),
  void (*xStep)(sqlite3_context*,int,sqlite3_value**),
  void (*xFinal)(sqlite3_context*)
);)
#define switch_core_db_create_function sqlite3_create_function

/**
 * Return the number of values in the current row of the result set.
 *
 * After a call to switch_core_db_step() that returns SQLITE_ROW, this routine
 * will return the same value as the switch_core_db_column_count() function.
 * After switch_core_db_step() has returned an SQLITE_DONE, SQLITE_BUSY or
 * error code, or before switch_core_db_step() has been called on a 
 * compiled SQL statement, this routine returns zero.
 */
DoxyDefine(int switch_core_db_data_count(sqlite3_stmt *pStmt);)
#define switch_core_db_data_count sqlite3_data_count

/**
 * Return the sqlite3* database handle to which the prepared statement given
 * in the argument belongs.  This is the same database handle that was
 * the first argument to the switch_core_db_prepare() that was used to create
 * the statement in the first place.
 */
DoxyDefine(switch_core_db *switch_core_db_db_handle(sqlite3_stmt*);)
#define switch_core_db_db_handle sqlite3_db_handle

/**
** Return the error code for the most recent switch_core_db_* API call associated
** with switch_core_db handle 'db'. SQLITE_OK is returned if the most recent 
** API call was successful.
**
** Calls to many switch_core_db_* functions set the error code and string returned
** by switch_core_db_errcode(), and switch_core_db_errmsg()
** (overwriting the previous values). Note that calls to switch_core_db_errcode(),
** and switch_core_db_errmsg() themselves do not affect the
** results of future invocations.
**
** Assuming no other intervening switch_core_db_* API calls are made, the error
** code returned by this function is associated with the same error as
** the strings  returned by sqlite3_errmsg() and sqlite3_errmsg16().
*/
DoxyDefine(int switch_core_db_errcode(switch_core_db *db);)
#define switch_core_db_errcode sqlite3_errcode

/**
 * Return a pointer to a UTF-8 encoded string describing in english the
 * error condition for the most recent sqlite3_* API call. The returned
 * string is always terminated by an 0x00 byte.
 *
 * The string "not an error" is returned when the most recent API call was
 * successful.
 */
DoxyDefine(const char *switch_core_db_errmsg(switch_core_db*);)
#define switch_core_db_errmsg sqlite3_errmsg

/**
 * A function to executes one or more statements of SQL.
 *
 * If one or more of the SQL statements are queries, then
 * the callback function specified by the 3rd parameter is
 * invoked once for each row of the query result.  This callback
 * should normally return 0.  If the callback returns a non-zero
 * value then the query is aborted, all subsequent SQL statements
 * are skipped and the switch_core_db_exec() function returns the SQLITE_ABORT.
 *
 * The 4th parameter is an arbitrary pointer that is passed
 * to the callback function as its first parameter.
 *
 * The 2nd parameter to the callback function is the number of
 * columns in the query result.  The 3rd parameter to the callback
 * is an array of strings holding the values for each column.
 * The 4th parameter to the callback is an array of strings holding
 * the names of each column.
 *
 * The callback function may be NULL, even for queries.  A NULL
 * callback is not an error.  It just means that no callback
 * will be invoked.
 *
 * If an error occurs while parsing or evaluating the SQL (but
 * not while executing the callback) then an appropriate error
 * message is written into memory obtained from malloc() and
 * *errmsg is made to point to that message.  The calling function
 * is responsible for freeing the memory that holds the error
 * message.   Use switch_core_db_free() for this.  If errmsg==NULL,
 * then no error message is ever written.
 *
 * The return value is is SQLITE_OK if there are no errors and
 * some other return code if there is an error.  The particular
 * return value depends on the type of error. 
 *
 * If the query could not be executed because a database file is
 * locked or busy, then this function returns SQLITE_BUSY.  (This
 * behavior can be modified somewhat using the sswitch_core_db_busy_handler()
 * and switch_core_db_busy_timeout() functions below.)
 */
DoxyDefine(int switch_core_db_exec(
  switch_core_db*,                     /* An open database */
  const char *sql,              /* SQL to be executed */
  sqlite3_callback,             /* Callback function */
  void *,                       /* 1st argument to callback function */
  char **errmsg                 /* Error msg written here */
);)
#define switch_core_db_exec sqlite3_exec

/**
 * Return TRUE (non-zero) if the statement supplied as an argument needs
 * to be recompiled.  A statement needs to be recompiled whenever the
 * execution environment changes in a way that would alter the program
 * that switch_core_db_prepare() generates.  For example, if new functions or
 * collating sequences are registered or if an authorizer function is
 * added or changed.
 *
 */
DoxyDefine(int switch_core_db_expired(sqlite3_stmt*);)
#define switch_core_db_expired sqlite3_expired

/**
 * This function is called to delete a compiled
 * SQL statement obtained by a previous call to switch_core_db_prepare().
 * If the statement was executed successfully, or
 * not executed at all, then SQLITE_OK is returned. If execution of the
 * statement failed then an error code is returned. 
 *
 * This routine can be called at any point during the execution of the
 * virtual machine.  If the virtual machine has not completed execution
 * when this routine is called, that is like encountering an error or
 * an interrupt.  (See switch_core_db_interrupt().)  Incomplete updates may be
 * rolled back and transactions cancelled,  depending on the circumstances,
 * and the result code returned will be SQLITE_ABORT.
 */
DoxyDefine(int switch_core_db_finalize(sqlite3_stmt *pStmt);)
#define switch_core_db_finalize sqlite3_finalize

/**
 * Call this routine to free the memory that sqlite3_get_table() allocated.
 */
DoxyDefine(void switch_core_db_free_table(char **result);)
#define switch_core_db_free_table sqlite3_free_table

/**
 * Test to see whether or not the database connection is in autocommit
 * mode.  Return TRUE if it is and FALSE if not.  Autocommit mode is on
 * by default.  Autocommit is disabled by a BEGIN statement and reenabled
 * by the next COMMIT or ROLLBACK.
 */
DoxyDefine(int switch_core_db_get_autocommit(switch_core_db*);)
#define switch_core_db_get_autocommit sqlite3_get_autocommit

/**
 * The following function may be used by scalar user functions to
 * associate meta-data with argument values. If the same value is passed to
 * multiple invocations of the user-function during query execution, under
 * some circumstances the associated meta-data may be preserved. This may
 * be used, for example, to add a regular-expression matching scalar
 * function. The compiled version of the regular expression is stored as
 * meta-data associated with the SQL value passed as the regular expression
 * pattern.
 *
 * returns a pointer to the meta data
 * associated with the Nth argument value to the current user function
 * call, where N is the second parameter. If no meta-data has been set for
 * that value, then a NULL pointer is returned.
 *
 * In practice, meta-data is preserved between function calls for
 * expressions that are constant at compile time. This includes literal
 * values and SQL variables.
 */
DoxyDefine(void *switch_core_db_get_auxdata(sqlite3_context*, int);)
#define switch_core_db_get_auxdata sqlite3_get_auxdata

/**
 * The following function may be used by scalar user functions to
 * associate meta-data with argument values. If the same value is passed to
 * multiple invocations of the user-function during query execution, under
 * some circumstances the associated meta-data may be preserved. This may
 * be used, for example, to add a regular-expression matching scalar
 * function. The compiled version of the regular expression is stored as
 * meta-data associated with the SQL value passed as the regular expression
 * pattern.
 *
 * This function is used to associate meta data with a user
 * function argument. The third parameter is a pointer to the meta data
 * to be associated with the Nth user function argument value. The fourth
 * parameter specifies a 'delete function' that will be called on the meta
 * data pointer to release it when it is no longer required. If the delete
 * function pointer is NULL, it is not invoked.
 *
 * In practice, meta-data is preserved between function calls for
 * expressions that are constant at compile time. This includes literal
 * values and SQL variables.
 */
DoxyDefine(void switch_core_db_set_auxdata(sqlite3_context*, int, void*, void (*)(void*));)
#define switch_core_db_set_auxdata sqlite3_set_auxdata

/**
 * This next routine is really just a wrapper around sqlite3_exec().
 * Instead of invoking a user-supplied callback for each row of the
 * result, this routine remembers each row of the result in memory
 * obtained from malloc(), then returns all of the result after the
 * query has finished. 
 *
 * As an example, suppose the query result where this table:
 *
 *        Name        | Age
 *        -----------------------
 *        Alice       | 43
 *        Bob         | 28
 *        Cindy       | 21
 *
 * If the 3rd argument were &azResult then after the function returns
 * azResult will contain the following data:
 *
 *        azResult[0] = "Name";
 *        azResult[1] = "Age";
 *        azResult[2] = "Alice";
 *        azResult[3] = "43";
 *        azResult[4] = "Bob";
 *        azResult[5] = "28";
 *        azResult[6] = "Cindy";
 *        azResult[7] = "21";
 *
 * Notice that there is an extra row of data containing the column
 * headers.  But the *nrow return value is still 3.  *ncolumn is
 * set to 2.  In general, the number of values inserted into azResult
 * will be ((*nrow) + 1)*(*ncolumn).
 *
 * After the calling function has finished using the result, it should 
 * pass the result data pointer to switch_core_db_free_table() in order to 
 * release the memory that was malloc-ed.  Because of the way the 
 * malloc() happens, the calling function must not try to call 
 * free() directly.  Only switch_core_db_free_table() is able to release 
 * the memory properly and safely.
 *
 * The return value of this routine is the same as from switch_core_db_exec().
 */
DoxyDefine(int switch_core_db_get_table(
  switch_core_db*,       /* An open database */
  const char *sql,       /* SQL to be executed */
  char ***resultp,       /* Result written to a char *[]  that this points to */
  int *nrow,             /* Number of result rows written here */
  int *ncolumn,          /* Number of result columns written here */
  char **errmsg          /* Error msg written here */
);)
#define switch_core_db_get_table sqlite3_get_table

/**
 * This function is called to recover from a malloc() failure that occured
 * within the SQLite library. Normally, after a single malloc() fails the 
 * library refuses to function (all major calls return SQLITE_NOMEM).
 * This function restores the library state so that it can be used again.
 *
 * All existing statements (sqlite3_stmt pointers) must be finalized or
 * reset before this call is made. Otherwise, SQLITE_BUSY is returned.
 * If any in-memory databases are in use, either as a main or TEMP
 * database, SQLITE_ERROR is returned. In either of these cases, the 
 * library is not reset and remains unusable.
 *
 * This function is *not* threadsafe. Calling this from within a threaded
 * application when threads other than the caller have used SQLite is
 * dangerous and will almost certainly result in malfunctions.
 *
 * This functionality can be omitted from a build by defining the 
 * SQLITE_OMIT_GLOBALRECOVER at compile time.
 */
DoxyDefine(int switch_core_db_global_recover();)
#define switch_core_db_global_recover sqlite3_global_recover

/** This function causes any pending database operation to abort and
 * return at its earliest opportunity.  This routine is typically
 * called in response to a user action such as pressing "Cancel"
 * or Ctrl-C where the user wants a long query operation to halt
 * immediately.
 */
DoxyDefine(void switch_core_db_interrupt(switch_core_db*);)
#define switch_core_db_interrupt sqlite3_interrupt

/**
 * Each entry in an SQLite table has a unique integer key.  (The key is
 * the value of the INTEGER PRIMARY KEY column if there is such a column,
 * otherwise the key is generated at random.  The unique key is always
 * available as the ROWID, OID, or _ROWID_ column.)  The following routine
 * returns the integer key of the most recent insert in the database.
 *
 * This function is similar to the mysql_insert_id() function from MySQL.
 */
DoxyDefine(sqlite_int64 switch_core_db_last_insert_rowid(switch_core_db*);)
#define switch_core_db_last_insert_rowid sqlite3_last_insert_rowid

/**
 * Open the sqlite database file "filename".  The "filename" is UTF-8
 * encoded.  An sqlite3* handle is returned in *ppDb, even
 * if an error occurs. If the database is opened (or created) successfully,
 * then SQLITE_OK is returned. Otherwise an error code is returned. The
 * switch_core_db_errmsg() routine can be used to obtain
 * an English language description of the error.
 *
 * If the database file does not exist, then a new database is created.
 * The encoding for the database is UTF-8.
 *
 * Whether or not an error occurs when it is opened, resources associated
 * with the switch_core_db* handle should be released by passing it to
 * switch_core_db_close() when it is no longer required.
 */
DoxyDefine(int switch_core_db_open(
  const char *filename,   /* Database filename (UTF-8) */
  switch_core_db **ppDb          /* OUT: SQLite db handle */
);)
#define switch_core_db_open sqlite3_open

/**
 * To execute an SQL query, it must first be compiled into a byte-code
 * program using the following routine.
 *
 * The first parameter "db" is an SQLite database handle. The second
 * parameter "zSql" is the statement to be compiled, encoded as
 * UTF-8. If the next parameter, "nBytes", is less
 * than zero, then zSql is read up to the first nul terminator.  If
 * "nBytes" is not less than zero, then it is the length of the string zSql
 * in bytes (not characters).
 *
 * *pzTail is made to point to the first byte past the end of the first
 * SQL statement in zSql.  This routine only compiles the first statement
 * in zSql, so *pzTail is left pointing to what remains uncompiled.
 *
 * *ppStmt is left pointing to a compiled SQL statement that can be
 * executed using switch_core_db_step().  Or if there is an error, *ppStmt may be
 * set to NULL.  If the input text contained no SQL (if the input is and
 * empty string or a comment) then *ppStmt is set to NULL.
 *
 * On success, SQLITE_OK is returned.  Otherwise an error code is returned.
 */
DoxyDefine(int switch_core_db_prepare(
  sqlite3 *db,            /* Database handle */
  const char *zSql,       /* SQL statement, UTF-8 encoded */
  int nBytes,             /* Length of zSql in bytes. */
  sqlite3_stmt **ppStmt,  /* OUT: Statement handle */
  const char **pzTail     /* OUT: Pointer to unused portion of zSql */
);)
#define switch_core_db_prepare sqlite3_prepare

/**
 * Register a function for tracing SQL command evaluation.  The function registered by
 * switch_core_db_profile() runs at the end of each SQL statement and includes
 * information on how long that statement ran.
 *
 * The sqlite3_profile() API is currently considered experimental and
 * is subject to change.
 */
DoxyDefine(void *switch_core_db_profile(switch_core_db*,
   void(*xProfile)(void*,const char*,sqlite_uint64), void*);)
#define switch_core_db_profile sqlite3_profile

/**
 * This routine configures a callback function - the progress callback - that
 * is invoked periodically during long running calls to switch_core_db_exec(),
 * switch_core_db_step() and switch_core_db_get_table(). An example use for this API is to 
 * keep a GUI updated during a large query.
 *
 * The progress callback is invoked once for every N virtual machine opcodes,
 * where N is the second argument to this function. The progress callback
 * itself is identified by the third argument to this function. The fourth
 * argument to this function is a void pointer passed to the progress callback
 * function each time it is invoked.
 *
 * If a call to switch_core_db_exec(), switch_core_db_step() or switch_core_db_get_table() results 
 * in less than N opcodes being executed, then the progress callback is not
 * invoked.
 * 
 * To remove the progress callback altogether, pass NULL as the third
 * argument to this function.
 *
 * If the progress callback returns a result other than 0, then the current 
 * query is immediately terminated and any database changes rolled back. If the
 * query was part of a larger transaction, then the transaction is not rolled
 * back and remains active. The switch_core_db_exec() call returns SQLITE_ABORT. 
 *
 ****** THIS IS AN EXPERIMENTAL API AND IS SUBJECT TO CHANGE ******
 */
DoxyDefine(void switch_core_db_progress_handler(switch_core_db*, int, int(*)(void*), void*);)
#define switch_core_db_progress_handler sqlite3_progress_handler

/**
 * The switch_core_db_reset() function is called to reset a compiled SQL
 * statement obtained by a previous call to switch_core_db_prepare()
 * back to it's initial state, ready to be re-executed.
 * Any SQL statement variables that had values bound to them using
 * the switch_core_db_bind_*() API retain their values.
 */
DoxyDefine(int switch_core_db_reset(sqlite3_stmt *pStmt);)
#define switch_core_db_reset sqlite3_reset

/**
 * User-defined functions invoke this routine in order to
 * set their return value.
 */
DoxyDefine(void switch_core_db_result_blob(sqlite3_context*, const void*, int, void(*)(void*));)
#define switch_core_db_result_blob sqlite3_result_blob

/**
 * User-defined functions invoke this routine in order to
 * set their return value.
 */
DoxyDefine(void switch_core_db_result_double(sqlite3_context*, double);)
#define switch_core_db_result_double sqlite3_result_double

/**
 * User-defined functions invoke this routine in order to
 * set their return value.
 */
DoxyDefine(void switch_core_db_result_error(sqlite3_context*, const char*, int);)
#define switch_core_db_result_error sqlite3_result_error

/**
 * User-defined functions invoke this routine in order to
 * set their return value.
 */
DoxyDefine(void switch_core_db_result_error16(sqlite3_context*, const void*, int);)
#define switch_core_db_result_error16 sqlite3_result_error16

/**
 * User-defined functions invoke this routine in order to
 * set their return value.
 */
DoxyDefine(void switch_core_db_result_int(sqlite3_context*, int);)
#define switch_core_db_result_int sqlite3_result_int

/**
 * User-defined functions invoke this routine in order to
 * set their return value.
 */
DoxyDefine(void switch_core_db_result_int64(sqlite3_context*, sqlite_int64);)
#define switch_core_db_result_int64 sqlite3_result_int64

/**
 * User-defined functions invoke this routine in order to
 * set their return value.
 */
DoxyDefine(void switch_core_db_result_null(sqlite3_context*);)
#define switch_core_db_result_null sqlite3_result_null

/**
 * User-defined functions invoke this routine in order to
 * set their return value.
 */
DoxyDefine(void switch_core_db_result_text(sqlite3_context*, const char*, int, void(*)(void*));)
#define switch_core_db_result_text sqlite3_result_text

/**
 * User-defined functions invoke this routine in order to
 * set their return value.
 */
DoxyDefine(void switch_core_db_result_text16(sqlite3_context*, const void*, int, void(*)(void*));)
#define switch_core_db_result_text16 sqlite3_result_text16

/**
 * User-defined functions invoke this routine in order to
 * set their return value.
 */
DoxyDefine(void switch_core_db_result_text16le(sqlite3_context*, const void*, int,void(*)(void*));)
#define switch_core_db_result_text16le sqlite3_result_text16le

/**
 * User-defined functions invoke this routine in order to
 * set their return value.
 */
DoxyDefine(void switch_core_db_result_text16be(sqlite3_context*, const void*, int,void(*)(void*));)
#define switch_core_db_result_text16be sqlite3_result_text16be

/**
 * User-defined functions invoke this routine in order to
 * set their return value.
 */
DoxyDefine(void switch_core_db_result_value(sqlite3_context*, sqlite3_value*);)
#define switch_core_db_result_value sqlite3_result_value

/**
 * This routine registers a callback with the SQLite library.  The
 * callback is invoked (at compile-time, not at run-time) for each
 * attempt to access a column of a table in the database.  The callback
 * returns SQLITE_OK if access is allowed, SQLITE_DENY if the entire
 * SQL statement should be aborted with an error and SQLITE_IGNORE
 * if the column should be treated as a NULL value.
 */
DoxyDefine(int switch_core_db_set_authorizer(
  switch_core_db*,
  int (*xAuth)(void*,int,const char*,const char*,const char*,const char*),
  void *pUserData
);)
#define switch_core_db_set_authorizer sqlite3_set_authorizer

/** 
 * After an SQL query has been compiled with a call to either
 * switch_core_db_prepare(), then this function must be
 * called one or more times to execute the statement.
 *
 * The return value will be either SQLITE_BUSY, SQLITE_DONE, 
 * SQLITE_ROW, SQLITE_ERROR, or SQLITE_MISUSE.
 *
 * SQLITE_BUSY means that the database engine attempted to open
 * a locked database and there is no busy callback registered.
 * Call switch_core_db_step() again to retry the open.
 *
 * SQLITE_DONE means that the statement has finished executing
 * successfully.  switch_core_db_step() should not be called again on this virtual
 * machine.
 *
 * If the SQL statement being executed returns any data, then 
 * SQLITE_ROW is returned each time a new row of data is ready
 * for processing by the caller. The values may be accessed using
 * the switch_core_db_column_*() functions described below. switch_core_db_step()
 * is called again to retrieve the next row of data.
 * 
 * SQLITE_ERROR means that a run-time error (such as a constraint
 * violation) has occurred.  switch_core_db_step() should not be called again on
 * the VM. More information may be found by calling switch_core_db_errmsg().
 *
 * SQLITE_MISUSE means that the this routine was called inappropriately.
 * Perhaps it was called on a virtual machine that had already been
 * finalized or on one that had previously returned SQLITE_ERROR or
 * SQLITE_DONE.  Or it could be the case the the same database connection
 * is being used simulataneously by two or more threads.
 */
DoxyDefine(int switch_core_db_step(sqlite3_stmt*);)
#define switch_core_db_step sqlite3_step

/**
 * If the following global variable is made to point to a
 * string which is the name of a directory, then all temporary files
 * created by SQLite will be placed in that directory.  If this variable
 * is NULL pointer, then SQLite does a search for an appropriate temporary
 * file directory.
 *
 * Once switch_core_db_open() has been called, changing this variable will invalidate
 * the current temporary database, if any.
 */
DoxyDefine(extern char *switch_core_db_temp_directory;)
#define switch_core_db_temp_directory sqlite3_temp_directory

/**
 * This function returns the number of database rows that have been
 * modified by INSERT, UPDATE or DELETE statements since the database handle
 * was opened. This includes UPDATE, INSERT and DELETE statements executed
 * as part of trigger programs. All changes are counted as soon as the
 * statement that makes them is completed (when the statement handle is
 * passed to switch_core_db_reset() or switch_core_db_finalise()).
 *
 * SQLite implements the command "DELETE FROM table" without a WHERE clause
 * by dropping and recreating the table.  (This is much faster than going
 * through and deleting individual elements form the table.)  Because of
 * this optimization, the change count for "DELETE FROM table" will be
 * zero regardless of the number of elements that were originally in the
 * table. To get an accurate count of the number of rows deleted, use
 * "DELETE FROM table WHERE 1" instead.
 */
DoxyDefine(int switch_core_db_total_changes(switch_core_db*);)
#define switch_core_db_total_changes sqlite3_total_changes

/**
 * Register a function for tracing SQL command evaluation.  The function
 * registered is invoked at the first switch_core_db_step()
 * for the evaluation of an SQL statement.
 */
DoxyDefine(void *switch_core_db_trace(switch_core_db*, void(*xTrace)(void*,const char*), void*);)
#define switch_core_db_trace sqlite3_trace

/**
 * Move all bindings from the first prepared statement over to the second.
 * This routine is useful, for example, if the first prepared statement
 * fails with an SQLITE_SCHEMA error.  The same SQL can be prepared into
 * the second prepared statement then all of the bindings transfered over
 * to the second statement before the first statement is finalized.
 */
DoxyDefine(int switch_core_db_transfer_bindings(sqlite3_stmt*, sqlite3_stmt*);)
#define switch_core_db_transfer_bindings sqlite3_transfer_bindings

/**
 * The pUserData parameter to the switch_core_db_create_function()
 * routine used to register user functions is available to
 * the implementation of the function using this call.
 */
DoxyDefine(void *switch_core_db_user_data(sqlite3_context*);)
#define switch_core_db_user_data sqlite3_user_data

/**
 * returns information about parameters to
 * a user-defined function.  Function implementations use this routines
 * to access their parameters.  This routine is the same as the
 * switch_core_db_column_* routines except that this routine takes a single
 * sqlite3_value* pointer instead of an sqlite3_stmt* and an integer
 * column number.
 */
DoxyDefine(const void *switch_core_db_value_blob(sqlite3_value*);)
#define switch_core_db_value_blob sqlite3_value_blob

/**
 * returns information about parameters to
 * a user-defined function.  Function implementations use this routines
 * to access their parameters.  This routine is the same as the
 * switch_core_db_column_* routines except that this routine takes a single
 * sqlite3_value* pointer instead of an sqlite3_stmt* and an integer
 * column number.
 */
DoxyDefine(int switch_core_db_value_bytes(sqlite3_value*);)
#define switch_core_db_value_bytes sqlite3_value_bytes

/**
 * returns information about parameters to
 * a user-defined function.  Function implementations use this routines
 * to access their parameters.  This routine is the same as the
 * switch_core_db_column_* routines except that this routine takes a single
 * sqlite3_value* pointer instead of an sqlite3_stmt* and an integer
 * column number.
 */
DoxyDefine(int switch_core_db_value_bytes16(sqlite3_value*);)
#define switch_core_db_value_bytes16 sqlite3_value_bytes16

/**
 * returns information about parameters to
 * a user-defined function.  Function implementations use this routines
 * to access their parameters.  This routine is the same as the
 * switch_core_db_column_* routines except that this routine takes a single
 * sqlite3_value* pointer instead of an sqlite3_stmt* and an integer
 * column number.
 */
DoxyDefine(double switch_core_db_value_double(sqlite3_value*);)
#define switch_core_db_value_double sqlite3_value_double

/**
 * returns information about parameters to
 * a user-defined function.  Function implementations use this routines
 * to access their parameters.  This routine is the same as the
 * switch_core_db_column_* routines except that this routine takes a single
 * sqlite3_value* pointer instead of an sqlite3_stmt* and an integer
 * column number.
 */
DoxyDefine(int switch_core_db_value_int(sqlite3_value*);)
#define switch_core_db_value_int sqlite3_value_int

/**
 * returns information about parameters to
 * a user-defined function.  Function implementations use this routines
 * to access their parameters.  This routine is the same as the
 * switch_core_db_column_* routines except that this routine takes a single
 * sqlite3_value* pointer instead of an sqlite3_stmt* and an integer
 * column number.
 */
DoxyDefine(sqlite_int64 switch_core_db_value_int64(sqlite3_value*);)
#define switch_core_db_value_int64 sqlite3_value_int64

/**
 * returns information about parameters to
 * a user-defined function.  Function implementations use this routines
 * to access their parameters.  This routine is the same as the
 * switch_core_db_column_* routines except that this routine takes a single
 * sqlite3_value* pointer instead of an sqlite3_stmt* and an integer
 * column number.
 */
DoxyDefine(const unsigned char *switch_core_db_value_text(sqlite3_value*);)
#define switch_core_db_value_text sqlite3_value_text

/**
 * returns information about parameters to
 * a user-defined function.  Function implementations use this routines
 * to access their parameters.  This routine is the same as the
 * switch_core_db_column_* routines except that this routine takes a single
 * sqlite3_value* pointer instead of an sqlite3_stmt* and an integer
 * column number.
 */
DoxyDefine(const void *switch_core_db_value_text16(sqlite3_value*);)
#define switch_core_db_value_text16 sqlite3_value_text16

/**
 * returns information about parameters to
 * a user-defined function.  Function implementations use this routines
 * to access their parameters.  This routine is the same as the
 * switch_core_db_column_* routines except that this routine takes a single
 * sqlite3_value* pointer instead of an sqlite3_stmt* and an integer
 * column number.
 */
DoxyDefine(const void *switch_core_db_value_text16be(sqlite3_value*);)
#define switch_core_db_value_text16be sqlite3_value_text16be

/**
 * returns information about parameters to
 * a user-defined function.  Function implementations use this routines
 * to access their parameters.  This routine is the same as the
 * switch_core_db_column_* routines except that this routine takes a single
 * sqlite3_value* pointer instead of an sqlite3_stmt* and an integer
 * column number.
 */
DoxyDefine(const void *switch_core_db_value_text16le(sqlite3_value*);)
#define switch_core_db_value_text16le sqlite3_value_text16le

/**
 * returns information about parameters to
 * a user-defined function.  Function implementations use this routines
 * to access their parameters.  This routine is the same as the
 * switch_core_db_column_* routines except that this routine takes a single
 * sqlite3_value* pointer instead of an sqlite3_stmt* and an integer
 * column number.
 */
DoxyDefine(int switch_core_db_value_type(sqlite3_value*);)
#define switch_core_db_value_type sqlite3_value_type


/**
 * This routine is a variant of the "sprintf()" from the
 * standard C library.  The resulting string is written into memory
 * obtained from malloc() so that there is never a possiblity of buffer
 * overflow.  This routine also implement some additional formatting
 * options that are useful for constructing SQL statements.
 *
 * The strings returned by this routine should be freed by calling
 * switch_core_db_free().
 *
 * All of the usual printf formatting options apply.  In addition, there
 * is a "%q" option.  %q works like %s in that it substitutes a null-terminated
 * string from the argument list.  But %q also doubles every '\'' character.
 * %q is designed for use inside a string literal.  By doubling each '\''
 * character it escapes that character and allows it to be inserted into
 * the string.
 *
 * For example, so some string variable contains text as follows:
 *
 *      char *zText = "It's a happy day!";
 *
 * We can use this text in an SQL statement as follows:
 *
 *      char *z = switch_core_db_mprintf("INSERT INTO TABLES('%q')", zText);
 *      switch_core_db_exec(db, z, callback1, 0, 0);
 *      switch_core_db_free(z);
 *
 * Because the %q format string is used, the '\'' character in zText
 * is escaped and the SQL generated is as follows:
 *
 *      INSERT INTO table1 VALUES('It''s a happy day!')
 *
 * This is correct.  Had we used %s instead of %q, the generated SQL
 * would have looked like this:
 *
 *      INSERT INTO table1 VALUES('It's a happy day!');
 *
 * This second example is an SQL syntax error.  As a general rule you
 * should always use %q instead of %s when inserting text into a string 
 * literal.
 */
DoxyDefine(char *switch_core_db_mprintf(const char*,...);)
#define switch_core_db_mprintf sqlite3_mprintf

/**
 * This routine is a variant of the "sprintf()" from the
 * standard C library.  The resulting string is written into memory
 * obtained from malloc() so that there is never a possiblity of buffer
 * overflow.  This routine also implement some additional formatting
 * options that are useful for constructing SQL statements.
 *
 * The strings returned by this routine should be freed by calling
 * switch_core_db_free().
 *
 * All of the usual printf formatting options apply.  In addition, there
 * is a "%q" option.  %q works like %s in that it substitutes a null-terminated
 * string from the argument list.  But %q also doubles every '\'' character.
 * %q is designed for use inside a string literal.  By doubling each '\''
 * character it escapes that character and allows it to be inserted into
 * the string.
 *
 * For example, so some string variable contains text as follows:
 *
 *      char *zText = "It's a happy day!";
 *
 * We can use this text in an SQL statement as follows:
 *
 *      char *z = switch_core_db_mprintf("INSERT INTO TABLES('%q')", zText);
 *      switch_core_db_exec(db, z, callback1, 0, 0);
 *      switch_core_db_free(z);
 *
 * Because the %q format string is used, the '\'' character in zText
 * is escaped and the SQL generated is as follows:
 *
 *      INSERT INTO table1 VALUES('It''s a happy day!')
 *
 * This is correct.  Had we used %s instead of %q, the generated SQL
 * would have looked like this:
 *
 *      INSERT INTO table1 VALUES('It's a happy day!');
 *
 * This second example is an SQL syntax error.  As a general rule you
 * should always use %q instead of %s when inserting text into a string 
 * literal.
 */
DoxyDefine(char *switch_core_db_vmprintf(const char*, va_list);)
#define switch_core_db_vmprintf sqlite3_vmprintf

/**
 * This routine is a variant of the "sprintf()" from the
 * standard C library.  The resulting string is written into memory
 * obtained from malloc() so that there is never a possiblity of buffer
 * overflow.  This routine also implement some additional formatting
 * options that are useful for constructing SQL statements.
 *
 * The strings returned by this routine should be freed by calling
 * switch_core_db_free().
 *
 * All of the usual printf formatting options apply.  In addition, there
 * is a "%q" option.  %q works like %s in that it substitutes a null-terminated
 * string from the argument list.  But %q also doubles every '\'' character.
 * %q is designed for use inside a string literal.  By doubling each '\''
 * character it escapes that character and allows it to be inserted into
 * the string.
 *
 * For example, so some string variable contains text as follows:
 *
 *      char *zText = "It's a happy day!";
 *
 * We can use this text in an SQL statement as follows:
 *
 *      char *z = switch_core_db_mprintf("INSERT INTO TABLES('%q')", zText);
 *      switch_core_db_exec(db, z, callback1, 0, 0);
 *      switch_core_db_free(z);
 *
 * Because the %q format string is used, the '\'' character in zText
 * is escaped and the SQL generated is as follows:
 *
 *      INSERT INTO table1 VALUES('It''s a happy day!')
 *
 * This is correct.  Had we used %s instead of %q, the generated SQL
 * would have looked like this:
 *
 *      INSERT INTO table1 VALUES('It's a happy day!');
 *
 * This second example is an SQL syntax error.  As a general rule you
 * should always use %q instead of %s when inserting text into a string 
 * literal.
 */
DoxyDefine(char *switch_core_db_snprintf(int,char*,const char*, ...);)
#define switch_core_db_snprintf sqlite3_snprintf

/**
 * call this routine to free memory malloced by a call to switch_core_db_mprintf, switch_core_db_vmprintf, or switch_core_db_snprintf
 */
DoxyDefine(void switch_core_db_free(char *z);)
#define switch_core_db_free sqlite3_free

/** @} */
/** @} */



#ifdef __cplusplus
}
#endif

#endif
