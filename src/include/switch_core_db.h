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
 * Michael Jerris <mike@jerris.com>
 *
 * switch_core_db.h -- Sqlite wrapper and extensions Header
 *
 */
/*! \file switch_core_db.h
    \brief Core DB Header
*/
#ifndef SWITCH_CORE_DB_H
#define SWITCH_CORE_DB_H

SWITCH_BEGIN_EXTERN_C
/**
 * @defgroup switch_sqlite_top Brought To You By SQLite
 * @ingroup FREESWITCH
 * @{
 */
/**
 * @defgroup switch_core_db Database Routines
 * @ingroup switch_sqlite_top 
 * @{
 */
/**
 * Each open database is represented by an instance of the
 * following opaque structure.
*/
	typedef struct sqlite3 switch_core_db_t;
typedef struct sqlite3_stmt switch_core_db_stmt_t;

typedef int (*switch_core_db_callback_func_t) (void *pArg, int argc, char **argv, char **columnNames);
typedef int (*switch_core_db_err_callback_func_t) (void *pArg, const char *errmsg);

/*
** These are special value for the destructor that is passed in as the
** final argument to routines like switch_core_db_result_blob().  If the destructor
** argument is SWITCH_CORE_DB_STATIC, it means that the content pointer is constant
** and will never change.  It does not need to be destroyed.  The 
** SWITCH_CORE_DB_TRANSIENT value means that the content will likely change in
** the near future and that the db should make its own private copy of
** the content before returning.
**
** The typedef is necessary to work around problems in certain
** C++ compilers.
*/
typedef void (*switch_core_db_destructor_type_t) (void *);
#define SWITCH_CORE_DB_STATIC      ((switch_core_db_destructor_type_t)0)
#define SWITCH_CORE_DB_TRANSIENT   ((switch_core_db_destructor_type_t)-1)

/**
 * A function to close the database.
 *
 * Call this function with a pointer to a structure that was previously
 * returned from switch_core_db_open() and the corresponding database will by closed.
 *
 * All SQL statements prepared using switch_core_db_prepare()
 * must be deallocated using switch_core_db_finalize() before
 * this routine is called. Otherwise, SWITCH_CORE_DB_BUSY is returned and the
 * database connection remains open.
 */
SWITCH_DECLARE(int) switch_core_db_close(switch_core_db_t *db);

/**
 * Open the database file "filename".  The "filename" is UTF-8
 * encoded.  A switch_core_db_t* handle is returned in *Db, even
 * if an error occurs. If the database is opened (or created) successfully,
 * then SWITCH_CORE_DB_OK is returned. Otherwise an error code is returned. The
 * switch_core_db_errmsg() routine can be used to obtain
 * an English language description of the error.
 *
 * If the database file does not exist, then a new database is created.
 * The encoding for the database is UTF-8.
 *
 * Whether or not an error occurs when it is opened, resources associated
 * with the switch_core_db_t* handle should be released by passing it to
 * switch_core_db_close() when it is no longer required.
 */
SWITCH_DECLARE(int) switch_core_db_open(const char *filename, switch_core_db_t **ppDb);

/**
 * The next group of routines returns information about the information
 * in a single column of the current result row of a query.  In every
 * case the first parameter is a pointer to the SQL statement that is being
 * executed (the switch_core_db_stmt_t* that was returned from switch_core_db_prepare()) and
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
SWITCH_DECLARE(const unsigned char *) switch_core_db_column_text(switch_core_db_stmt_t *stmt, int iCol);

/**
 * The first parameter is a compiled SQL statement. This function returns
 * the column heading for the Nth column of that statement, where N is the
 * second function parameter.  The string returned is UTF-8.
 */
SWITCH_DECLARE(const char *) switch_core_db_column_name(switch_core_db_stmt_t *stmt, int N);

/**
 * Return the number of columns in the result set returned by the compiled
 * SQL statement. This routine returns 0 if pStmt is an SQL statement
 * that does not return data (for example an UPDATE).
 */
SWITCH_DECLARE(int) switch_core_db_column_count(switch_core_db_stmt_t *pStmt);

/**
 * Return a pointer to a UTF-8 encoded string describing in english the
 * error condition for the most recent switch_core_db_* API call. The returned
 * string is always terminated by an 0x00 byte.
 *
 * The string "not an error" is returned when the most recent API call was
 * successful.
 */
SWITCH_DECLARE(const char *) switch_core_db_errmsg(switch_core_db_t *db);

/**
 * A function to executes one or more statements of SQL.
 *
 * If one or more of the SQL statements are queries, then
 * the callback function specified by the 3rd parameter is
 * invoked once for each row of the query result.  This callback
 * should normally return 0.  If the callback returns a non-zero
 * value then the query is aborted, all subsequent SQL statements
 * are skipped and the switch_core_db_exec() function returns the SWITCH_CORE_DB_ABORT.
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
 * The return value is is SWITCH_CORE_DB_OK if there are no errors and
 * some other return code if there is an error.  The particular
 * return value depends on the type of error. 
 *
 * If the query could not be executed because a database file is
 * locked or busy, then this function returns SWITCH_CORE_DB_BUSY.  (This
 * behavior can be modified somewhat using the sswitch_core_db_busy_handler()
 * and switch_core_db_busy_timeout() functions below.)
 */
SWITCH_DECLARE(int) switch_core_db_exec(switch_core_db_t *db, const char *sql, switch_core_db_callback_func_t callback, void *data, char **errmsg);

/**
 * This function is called to delete a compiled
 * SQL statement obtained by a previous call to switch_core_db_prepare().
 * If the statement was executed successfully, or
 * not executed at all, then SWITCH_CORE_DB_OK is returned. If execution of the
 * statement failed then an error code is returned. 
 *
 * This routine can be called at any point during the execution of the
 * virtual machine.  If the virtual machine has not completed execution
 * when this routine is called, that is like encountering an error or
 * an interrupt.  (See switch_core_db_interrupt().)  Incomplete updates may be
 * rolled back and transactions cancelled,  depending on the circumstances,
 * and the result code returned will be SWITCH_CORE_DB_ABORT.
 */
SWITCH_DECLARE(int) switch_core_db_finalize(switch_core_db_stmt_t *pStmt);

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
 * On success, SWITCH_CORE_DB_OK is returned.  Otherwise an error code is returned.
 */
SWITCH_DECLARE(int) switch_core_db_prepare(switch_core_db_t *db, const char *zSql, int nBytes, switch_core_db_stmt_t **ppStmt, const char **pzTail);

/** 
 * After an SQL query has been compiled with a call to either
 * switch_core_db_prepare(), then this function must be
 * called one or more times to execute the statement.
 *
 * The return value will be either SWITCH_CORE_DB_BUSY, SWITCH_CORE_DB_DONE, 
 * SWITCH_CORE_DB_ROW, SWITCH_CORE_DB_ERROR, or SWITCH_CORE_DB_MISUSE.
 *
 * SWITCH_CORE_DB_BUSY means that the database engine attempted to open
 * a locked database and there is no busy callback registered.
 * Call switch_core_db_step() again to retry the open.
 *
 * SWITCH_CORE_DB_DONE means that the statement has finished executing
 * successfully.  switch_core_db_step() should not be called again on this virtual
 * machine.
 *
 * If the SQL statement being executed returns any data, then 
 * SWITCH_CORE_DB_ROW is returned each time a new row of data is ready
 * for processing by the caller. The values may be accessed using
 * the switch_core_db_column_*() functions described below. switch_core_db_step()
 * is called again to retrieve the next row of data.
 * 
 * SWITCH_CORE_DB_ERROR means that a run-time error (such as a constraint
 * violation) has occurred.  switch_core_db_step() should not be called again on
 * the VM. More information may be found by calling switch_core_db_errmsg().
 *
 * SWITCH_CORE_DB_MISUSE means that the this routine was called inappropriately.
 * Perhaps it was called on a virtual machine that had already been
 * finalized or on one that had previously returned SWITCH_CORE_DB_ERROR or
 * SWITCH_CORE_DB_DONE.  Or it could be the case the the same database connection
 * is being used simulataneously by two or more threads.
 */
SWITCH_DECLARE(int) switch_core_db_step(switch_core_db_stmt_t *stmt);

/**
 * The switch_core_db_reset() function is called to reset a compiled SQL
 * statement obtained by a previous call to switch_core_db_prepare()
 * back to it's initial state, ready to be re-executed.
 * Any SQL statement variables that had values bound to them using
 * the switch_core_db_bind_*() API retain their values.
 */
SWITCH_DECLARE(int) switch_core_db_reset(switch_core_db_stmt_t *pStmt);

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
 * switch_core_db_bind_parameter_index() to get the correct index value given
 * the parameters name.  If the same named parameter occurs more than
 * once, it is assigned the same index each time.
 *
 * The switch_core_db_bind_* routine must be called before switch_core_db_step() after
 * an switch_core_db_prepare() or sqlite3_reset().  Unbound parameterss are
 * interpreted as NULL.
 */
SWITCH_DECLARE(int) switch_core_db_bind_int(switch_core_db_stmt_t *pStmt, int i, int iValue);

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
 * switch_core_db_bind_parameter_index() to get the correct index value given
 * the parameters name.  If the same named parameter occurs more than
 * once, it is assigned the same index each time.
 *
 * The switch_core_db_bind_* routine must be called before switch_core_db_step() after
 * an switch_core_db_prepare() or sqlite3_reset().  Unbound parameterss are
 * interpreted as NULL.
 */
SWITCH_DECLARE(int) switch_core_db_bind_int64(switch_core_db_stmt_t *pStmt, int i, int64_t iValue);

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
 * switch_core_db_bind_parameter_index() to get the correct index value given
 * the parameters name.  If the same named parameter occurs more than
 * once, it is assigned the same index each time.
 *
 * The fifth parameter to switch_core_db_bind_blob(), switch_core_db_bind_text(), and
 * switch_core_db_bind_text16() is a destructor used to dispose of the BLOB or
 * text after SQLite has finished with it.  If the fifth argument is the
 * special value SQLITE_STATIC, then the library assumes that the information
 * is in static, unmanaged space and does not need to be freed.  If the
 * fifth argument has the value SQLITE_TRANSIENT, then SQLite makes its
 * own private copy of the data.
 *
 * The switch_core_db_bind_* routine must be called before switch_core_db_step() after
 * an switch_core_db_prepare() or sqlite3_reset().  Unbound parameterss are
 * interpreted as NULL.
 */
SWITCH_DECLARE(int) switch_core_db_bind_text(switch_core_db_stmt_t *pStmt, int i, const char *zData, int nData, switch_core_db_destructor_type_t xDel);

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
 * an switch_core_db_prepare() or switch_core_db_reset().  Unbound parameterss are
 * interpreted as NULL.
 */
SWITCH_DECLARE(int) switch_core_db_bind_double(switch_core_db_stmt_t *pStmt, int i, double dValue);

/**
 * Each entry in a table has a unique integer key.  (The key is
 * the value of the INTEGER PRIMARY KEY column if there is such a column,
 * otherwise the key is generated at random.  The unique key is always
 * available as the ROWID, OID, or _ROWID_ column.)  The following routine
 * returns the integer key of the most recent insert in the database.
 *
 * This function is similar to the mysql_insert_id() function from MySQL.
 */
SWITCH_DECLARE(int64_t) switch_core_db_last_insert_rowid(switch_core_db_t *db);

/**
 * This next routine is really just a wrapper around switch_core_db_exec().
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
SWITCH_DECLARE(int) switch_core_db_get_table(switch_core_db_t *db,	/* An open database */
											 const char *sql,	/* SQL to be executed */
											 char ***resultp,	/* Result written to a char *[]  that this points to */
											 int *nrow,	/* Number of result rows written here */
											 int *ncolumn,	/* Number of result columns written here */
											 char **errmsg	/* Error msg written here */
	);

/**
 * Call this routine to free the memory that sqlite3_get_table() allocated.
 */
SWITCH_DECLARE(void) switch_core_db_free_table(char **result);

/**
 * Call this routine to free the memory that switch_core_db_get_table() allocated.
 */
SWITCH_DECLARE(void) switch_core_db_free(char *z);

/**
 * Call this routine to find the number of rows changed by the last statement.
 */
SWITCH_DECLARE(int) switch_core_db_changes(switch_core_db_t *db);

/**
 * Call this routine to load an external extension
 */
SWITCH_DECLARE(int) switch_core_db_load_extension(switch_core_db_t *db, const char *extension);

/** Return values for switch_core_db_exec() and switch_core_db_step()*/
#define SWITCH_CORE_DB_OK           0	/* Successful result */
/* beginning-of-error-codes */
#define SWITCH_CORE_DB_ERROR        1	/* SQL error or missing database */
#define SWITCH_CORE_DB_INTERNAL     2	/* NOT USED. Internal logic error in SQLite */
#define SWITCH_CORE_DB_PERM         3	/* Access permission denied */
#define SWITCH_CORE_DB_ABORT        4	/* Callback routine requested an abort */
#define SWITCH_CORE_DB_BUSY         5	/* The database file is locked */
#define SWITCH_CORE_DB_LOCKED       6	/* A table in the database is locked */
#define SWITCH_CORE_DB_NOMEM        7	/* A malloc() failed */
#define SWITCH_CORE_DB_READONLY     8	/* Attempt to write a readonly database */
#define SWITCH_CORE_DB_INTERRUPT    9	/* Operation terminated by switch_core_db_interrupt() */
#define SWITCH_CORE_DB_IOERR       10	/* Some kind of disk I/O error occurred */
#define SWITCH_CORE_DB_CORRUPT     11	/* The database disk image is malformed */
#define SWITCH_CORE_DB_NOTFOUND    12	/* NOT USED. Table or record not found */
#define SWITCH_CORE_DB_FULL        13	/* Insertion failed because database is full */
#define SWITCH_CORE_DB_CANTOPEN    14	/* Unable to open the database file */
#define SWITCH_CORE_DB_PROTOCOL    15	/* Database lock protocol error */
#define SWITCH_CORE_DB_EMPTY       16	/* Database is empty */
#define SWITCH_CORE_DB_SCHEMA      17	/* The database schema changed */
#define SWITCH_CORE_DB_TOOBIG      18	/* NOT USED. Too much data for one row */
#define SWITCH_CORE_DB_CONSTRAINT  19	/* Abort due to contraint violation */
#define SWITCH_CORE_DB_MISMATCH    20	/* Data type mismatch */
#define SWITCH_CORE_DB_MISUSE      21	/* Library used incorrectly */
#define SWITCH_CORE_DB_NOLFS       22	/* Uses OS features not supported on host */
#define SWITCH_CORE_DB_AUTH        23	/* Authorization denied */
#define SWITCH_CORE_DB_FORMAT      24	/* Auxiliary database format error */
#define SWITCH_CORE_DB_RANGE       25	/* 2nd parameter to switch_core_db_bind out of range */
#define SWITCH_CORE_DB_NOTADB      26	/* File opened that is not a database file */
#define SWITCH_CORE_DB_ROW         100	/* switch_core_db_step() has another row ready */
#define SWITCH_CORE_DB_DONE        101	/* switch_core_db_step() has finished executing */
/* end-of-error-codes */


/** @} */
/** @} */
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

SWITCH_DECLARE(char*)switch_sql_concat(void);

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
