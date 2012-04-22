/*
** 2001 September 22
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
*/
#ifndef SWITCH_MPRINTF_H
#define SWITCH_MPRINTF_H

SWITCH_BEGIN_EXTERN_C
/**
 * This routine is a variant of the "sprintf()" from the
 * standard C library.  The resulting string is written into memory
 * obtained from malloc() so that there is never a possiblity of buffer
 * overflow.  This routine also implement some additional formatting
 * options that are useful for constructing SQL statements.
 *
 * The strings returned by this routine should be freed by calling
 * free().
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
 *      char *z = switch_mprintf("INSERT INTO TABLES('%q')", zText);
 *      switch_core_db_exec(db, z, callback1, 0, 0);
 *      free(z);
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
SWITCH_DECLARE(char *) switch_mprintf(const char *zFormat, ...);
SWITCH_DECLARE(char *) switch_vmprintf(const char *zFormat, va_list ap);
SWITCH_DECLARE(char *) switch_snprintfv(char *zBuf, int n, const char *zFormat, ...);

SWITCH_END_EXTERN_C
#endif /* SWITCH_MPRINTF_H */
