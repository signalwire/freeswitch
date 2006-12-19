/*
 * Copyright (c) 1989, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Guido van Rossum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)fnmatch.c	8.2 (Berkeley) 4/16/94";
#endif /* LIBC_SCCS and not lint */

/*
 * Function fnmatch() as specified in POSIX 1003.2-1992, section B.6.
 * Compares a filename or pathname to a pattern.
 */
#ifndef WIN32
#include "apr_private.h"
#endif
#include "apr_file_info.h"
#include "apr_fnmatch.h"
#include "apr_tables.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include <string.h>
#if APR_HAVE_CTYPE_H
# include <ctype.h>
#endif

#define	EOS	'\0'

static const char *rangematch(const char *, int, int);

APR_DECLARE(apr_status_t) apr_fnmatch(const char *pattern, const char *string, int flags)
{
    const char *stringstart;
    char c, test;

    for (stringstart = string;;) {
	switch (c = *pattern++) {
	case EOS:
	    return (*string == EOS ? APR_SUCCESS : APR_FNM_NOMATCH);
	case '?':
	    if (*string == EOS) {
		return (APR_FNM_NOMATCH);
	    }
	    if (*string == '/' && (flags & APR_FNM_PATHNAME)) {
		return (APR_FNM_NOMATCH);
	    }
	    if (*string == '.' && (flags & APR_FNM_PERIOD) &&
		(string == stringstart ||
		 ((flags & APR_FNM_PATHNAME) && *(string - 1) == '/'))) {
		return (APR_FNM_NOMATCH);
	    }
	    ++string;
	    break;
	case '*':
	    c = *pattern;
	    /* Collapse multiple stars. */
	    while (c == '*') {
		c = *++pattern;
	    }

	    if (*string == '.' && (flags & APR_FNM_PERIOD) &&
		(string == stringstart ||
		 ((flags & APR_FNM_PATHNAME) && *(string - 1) == '/'))) {
		return (APR_FNM_NOMATCH);
	    }

	    /* Optimize for pattern with * at end or before /. */
	    if (c == EOS) {
		if (flags & APR_FNM_PATHNAME) {
		    return (strchr(string, '/') == NULL ? APR_SUCCESS : APR_FNM_NOMATCH);
		}
		else {
		    return (APR_SUCCESS);
		}
	    }
	    else if (c == '/' && flags & APR_FNM_PATHNAME) {
	        if ((string = strchr(string, '/')) == NULL) {
		    return (APR_FNM_NOMATCH);
		}
		break;
	    }

	    /* General case, use recursion. */
	    while ((test = *string) != EOS) {
	        if (!apr_fnmatch(pattern, string, flags & ~APR_FNM_PERIOD)) {
		    return (APR_SUCCESS);
		}
		if (test == '/' && flags & APR_FNM_PATHNAME) {
		    break;
		}
		++string;
	    }
	    return (APR_FNM_NOMATCH);
	case '[':
	    if (*string == EOS) {
		return (APR_FNM_NOMATCH);
	    }
	    if (*string == '/' && flags & APR_FNM_PATHNAME) {
		return (APR_FNM_NOMATCH);
	    }
	    if (*string == '.' && (flags & APR_FNM_PERIOD) &&
		(string == stringstart ||
		 ((flags & APR_FNM_PATHNAME) && *(string - 1) == '/'))) {
	        return (APR_FNM_NOMATCH);
	    }
	    if ((pattern = rangematch(pattern, *string, flags)) == NULL) {
		return (APR_FNM_NOMATCH);
	    }
	    ++string;
	    break;
	case '\\':
	    if (!(flags & APR_FNM_NOESCAPE)) {
		if ((c = *pattern++) == EOS) {
		    c = '\\';
		    --pattern;
		}
	    }
	    /* FALLTHROUGH */
	default:
	    if (flags & APR_FNM_CASE_BLIND) {
	        if (apr_tolower(c) != apr_tolower(*string)) {
		    return (APR_FNM_NOMATCH);
		}
	    }
	    else if (c != *string) {
	        return (APR_FNM_NOMATCH);
	    }
	    string++;
	    break;
	}
    /* NOTREACHED */
    }
}

static const char *rangematch(const char *pattern, int test, int flags)
{
    int negate, ok;
    char c, c2;

    /*
     * A bracket expression starting with an unquoted circumflex
     * character produces unspecified results (IEEE 1003.2-1992,
     * 3.13.2).  This implementation treats it like '!', for
     * consistency with the regular expression syntax.
     * J.T. Conklin (conklin@ngai.kaleida.com)
     */
    if ((negate = (*pattern == '!' || *pattern == '^'))) {
	++pattern;
    }

    for (ok = 0; (c = *pattern++) != ']';) {
        if (c == '\\' && !(flags & APR_FNM_NOESCAPE)) {
	    c = *pattern++;
	}
	if (c == EOS) {
	    return (NULL);
	}
	if (*pattern == '-' && (c2 = *(pattern + 1)) != EOS && c2 != ']') {
	    pattern += 2;
	    if (c2 == '\\' && !(flags & APR_FNM_NOESCAPE)) {
		c2 = *pattern++;
	    }
	    if (c2 == EOS) {
		return (NULL);
	    }
	    if ((c <= test && test <= c2)
		|| ((flags & APR_FNM_CASE_BLIND)
		    && ((apr_tolower(c) <= apr_tolower(test))
			&& (apr_tolower(test) <= apr_tolower(c2))))) {
		ok = 1;
	    }
	}
	else if ((c == test)
		 || ((flags & APR_FNM_CASE_BLIND)
		     && (apr_tolower(c) == apr_tolower(test)))) {
	    ok = 1;
	}
    }
    return (ok == negate ? NULL : pattern);
}


/* This function is an Apache addition */
/* return non-zero if pattern has any glob chars in it */
APR_DECLARE(int) apr_fnmatch_test(const char *pattern)
{
    int nesting;

    nesting = 0;
    while (*pattern) {
	switch (*pattern) {
	case '?':
	case '*':
	    return 1;

	case '\\':
	    if (*pattern++ == '\0') {
		return 0;
	    }
	    break;

	case '[':	/* '[' is only a glob if it has a matching ']' */
	    ++nesting;
	    break;

	case ']':
	    if (nesting) {
		return 1;
	    }
	    break;
	}
	++pattern;
    }
    return 0;
}

/* Find all files matching the specified pattern */
APR_DECLARE(apr_status_t) apr_match_glob(const char *pattern, 
                                         apr_array_header_t **result,
                                         apr_pool_t *p)
{
    apr_dir_t *dir;
    apr_finfo_t finfo;
    apr_status_t rv;
    char *path;

    /* XXX So, this is kind of bogus.  Basically, I need to strip any leading
     * directories off the pattern, but there is no portable way to do that.
     * So, for now we just find the last occurance of '/' and if that doesn't
     * return anything, then we look for '\'.  This means that we could
     * screw up on unix if the pattern is something like "foo\.*"  That '\'
     * isn't a directory delimiter, it is a part of the filename.  To fix this,
     * we really need apr_filepath_basename, which will be coming as soon as
     * I get to it.  rbb
     */
    char *idx = strrchr(pattern, '/');
    
    if (idx == NULL) {
        idx = strrchr(pattern, '\\');
    }
    if (idx == NULL) {
        path = ".";
    }
    else {
        path = apr_pstrndup(p, pattern, idx - pattern);
        pattern = idx + 1;
    }

    *result = apr_array_make(p, 0, sizeof(char *));
    rv = apr_dir_open(&dir, path, p);
    if (rv != APR_SUCCESS) {
        return rv;
    }

    while (apr_dir_read(&finfo, APR_FINFO_NAME, dir) == APR_SUCCESS) {
        if (apr_fnmatch(pattern, finfo.name, 0) == APR_SUCCESS) {
            *(const char **)apr_array_push(*result) = apr_pstrdup(p, finfo.name);
        }
    }
    apr_dir_close(dir);
    return APR_SUCCESS;
}
