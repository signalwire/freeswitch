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
#ifndef KS_PRINTF_H
#define KS_PRINTF_H

#include "ks.h"

KS_BEGIN_EXTERN_C

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
 *      char *z = ks_mprintf("INSERT INTO TABLES('%q')", zText);
 *      ks_core_db_exec(db, z, callback1, 0, 0);
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
KS_DECLARE(char *) ks_mprintf(const char *zFormat, ...);
KS_DECLARE(char *) ks_vmprintf(const char *zFormat, va_list ap);
KS_DECLARE(char *) ks_snprintfv(char *zBuf, int n, const char *zFormat, ...);
KS_DECLARE(char *) ks_vsnprintf(char *zbuf, int n, const char *zFormat, va_list ap);
KS_DECLARE(char *) ks_vpprintf(ks_pool_t *pool, const char *zFormat, va_list ap);
KS_DECLARE(char *) ks_pprintf(ks_pool_t *pool, const char *zFormat, ...);

KS_END_EXTERN_C

#endif /* KS_PRINTF_H */

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
