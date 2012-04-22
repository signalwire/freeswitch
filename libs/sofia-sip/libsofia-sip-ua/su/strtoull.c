/*
 * strtoull.c --
 *
 *	Source code for the "strtoull" library procedure.
 *
 * Copyright (c) 1988 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
The following license.terms for information on usage and redistribution
of this individual file, and for a DISCLAIMER OF ALL WARRANTIES.

This software is copyrighted by the Regents of the University of
California, Sun Microsystems, Inc., Scriptics Corporation, ActiveState
Corporation and other parties.  The following terms apply to all files
associated with the software unless explicitly disclaimed in
individual files.

The authors hereby grant permission to use, copy, modify, distribute,
and license this software and its documentation for any purpose, provided
that existing copyright notices are retained in all copies and that this
notice is included verbatim in any distributions. No written agreement,
license, or royalty fee is required for any of the authorized uses.
Modifications to this software may be copyrighted by their authors
and need not follow the licensing terms described here, provided that
the new terms are clearly indicated on the first page of each file where
they apply.

IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
MODIFICATIONS.

GOVERNMENT USE: If you are acquiring this software on behalf of the
U.S. government, the Government shall have only "Restricted Rights"
in the software and related documentation as defined in the Federal
Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
are acquiring the software on behalf of the Department of Defense, the
software shall be classified as "Commercial Computer Software" and the
Government shall have only "Restricted Rights" as defined in Clause
252.227-7013 (c) (1) of DFARs.  Notwithstanding the foregoing, the
authors grant the U.S. Government and others acting in its behalf
permission to use and distribute the software in accordance with the
terms specified in this license.
 */

#include "config.h"

#include <errno.h>
#include <ctype.h>

/*
 * The table below is used to convert from ASCII digits to a
 * numerical equivalent.  It maps from '0' through 'z' to integers
 * (100 for non-digit characters).
 */

static char cvtIn[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,		/* '0' - '9' */
    100, 100, 100, 100, 100, 100, 100,		/* punctuation */
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,	/* 'A' - 'Z' */
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35,
    100, 100, 100, 100, 100, 100,		/* punctuation */
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,	/* 'a' - 'z' */
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35};


/**Convert an ASCII string into an unsigned long long integer.
 *
 * @param[in]  string  String of ASCII digits, possibly preceded by white
 *                     space. For bases greater than 10, either lower- or
 *                     upper-case digits may be used.
 *
 * @param[out] endPtr  Where to store address of terminating character, or
 *                     NULL.
 *
 * @param[in] base     Base for conversion. Must be less than 37. If 0, then
 *                     the base is chosen from the leading characters of
 *                     string: "0x" means hex, "0" means octal, anything
 *                     else means decimal.
 *
 * @return
 * The integer equivalent of string. If @a endPtr is non-NULL, then pointer
 * to the character after the last one that was part of the integer is
 * stored to @a *endPtr.
 *
 * If string doesn't contain a valid integer value, then zero is
 * returned and *endPtr is set to original value of @a string.
 */

unsigned longlong
strtoull(const char *string, char **endPtr, int base)
{
    register const char *p;
    register unsigned longlong result = 0;
    register unsigned digit;
    register unsigned longlong shifted;
    int anyDigits = 0, negative = 0;

    /*
     * Skip any leading blanks.
     */

    p = string;
    while (isspace(*p)) {	/* INTL: locale-dependent */
	p += 1;
    }

    /*
     * Check for a sign.
     */

    if (*p == '-') {
	p += 1;
	negative = 1;
    } else {
	if (*p == '+') {
	    p += 1;
	}
    }

    /*
     * If no base was provided, pick one from the leading characters
     * of the string.
     */

    if (base == 0) {
	if (*p == '0') {
	    p += 1;
	    if (*p == 'x' || *p == 'X') {
		p += 1;
		base = 16;
	    } else {

		/*
		 * Must set anyDigits here, otherwise "0" produces a
		 * "no digits" error.
		 */

		anyDigits = 1;
		base = 8;
	    }
	} else {
	    base = 10;
	}
    } else if (base == 16) {

	/*
	 * Skip a leading "0x" from hex numbers.
	 */

	if ((p[0] == '0') && (p[1] == 'x' || *p == 'X')) {
	    p += 2;
	}
    }

    /*
     * Sorry this code is so messy, but speed seems important.  Do
     * different things for base 8, 10, 16, and other.
     */

    if (base == 8) {
	for ( ; ; p += 1) {
	    digit = *p - '0';
	    if (digit > 7) {
		break;
	    }
	    shifted = result << 3;
	    if ((shifted >> 3) != result) {
		goto overflow;
	    }
	    result = shifted + digit;
	    if ( result < shifted ) {
		goto overflow;
	    }
	    anyDigits = 1;
	}
    } else if (base == 10) {
	for ( ; ; p += 1) {
	    digit = *p - '0';
	    if (digit > 9) {
		break;
	    }
	    shifted = 10 * result;
	    if ((shifted / 10) != result) {
		goto overflow;
	    }
	    result = shifted + digit;
	    if ( result < shifted ) {
		goto overflow;
	    }
	    anyDigits = 1;
	}
    } else if (base == 16) {
	for ( ; ; p += 1) {
	    digit = *p - '0';
	    if (digit > ('z' - '0')) {
		break;
	    }
	    digit = cvtIn[digit];
	    if (digit > 15) {
		break;
	    }
	    shifted = result << 4;
	    if ((shifted >> 4) != result) {
		goto overflow;
	    }
	    result = shifted + digit;
	    if ( result < shifted ) {
		goto overflow;
	    }
	    anyDigits = 1;
	}
    } else if ( base >= 2 && base <= 36 ) {
	for ( ; ; p += 1) {
	    digit = *p - '0';
	    if (digit > ('z' - '0')) {
		break;
	    }
	    digit = cvtIn[digit];
	    if (digit >= (unsigned) base) {
		break;
	    }
	    shifted = result * base;
	    if ((shifted/base) != result) {
		goto overflow;
	    }
	    result = shifted + digit;
	    if ( result < shifted ) {
		goto overflow;
	    }
	    anyDigits = 1;
	}
    }

    /*
     * Negate if we found a '-' earlier.
     */

    if (negative) {
		result = (unsigned longlong)(-((longlong)result));
    }

    if (endPtr != 0) {
    /*
     * See if there were any digits at all.
     */
        if (!anyDigits) {
    	    p = string;
        }
	*endPtr = (char *) p;
    }

    return result;

    /*
     * On overflow generate the right output
     */

 overflow:
    errno = ERANGE;
    if (endPtr != 0) {
	for ( ; ; p += 1) {
	    digit = *p - '0';
	    if (digit > ('z' - '0')) {
		break;
	    }
	    digit = cvtIn[digit];
	    if (digit >= (unsigned) base) {
		break;
	    }
	}
	*endPtr = (char *) p;
    }

    return (unsigned longlong)-1;
}
