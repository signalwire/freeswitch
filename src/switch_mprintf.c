/*
** The "printf" code that follows dates from the 1980's.  It is in
** the public domain.  The original comments are included here for
** completeness.  They are very out-of-date but might be useful as
** an historical reference.  Most of the "enhancements" have been backed
** out so that the functionality is now the same as standard printf().
**
**************************************************************************
**
** The following modules is an enhanced replacement for the "printf" subroutines
** found in the standard C library.  The following enhancements are
** supported:
**
**      +  Additional functions.  The standard set of "printf" functions
**         includes printf, fprintf, sprintf, vprintf, vfprintf, and
**         vsprintf.  This module adds the following:
**
**           *  snprintf -- Works like sprintf, but has an extra argument
**                          which is the size of the buffer written to.
**
**           *  mprintf --  Similar to sprintf.  Writes output to memory
**                          obtained from malloc.
**
**           *  xprintf --  Calls a function to dispose of output.
**
**           *  nprintf --  No output, but returns the number of characters
**                          that would have been output by printf.
**
**           *  A v- version (ex: vsnprintf) of every function is also
**              supplied.
**
**      +  A few extensions to the formatting notation are supported:
**
**           *  The "=" flag (similar to "-") causes the output to be
**              be centered in the appropriately sized field.
**
**           *  The %b field outputs an integer in binary notation.
**
**           *  The %c field now accepts a precision.  The character output
**              is repeated by the number of times the precision specifies.
**
**           *  The %' field works like %c, but takes as its character the
**              next character of the format string, instead of the next
**              argument.  For example,  printf("%.78'-")  prints 78 minus
**              signs, the same as  printf("%.78c",'-').
**
**      +  When compiled using GCC on a SPARC, this version of printf is
**         faster than the library printf for SUN OS 4.1.
**
**      +  All functions are fully reentrant.
**
*/
/*
 * 20090210 (stkn):
 *    Taken from sqlite-3.3.x,
 *    renamed SQLITE_ -> SWITCH_,
 *    renamed visible functions to switch_*
 *    disabled functions without extra conversion specifiers
 */

#include <switch.h>

#define LONGDOUBLE_TYPE	long double

/*
** Conversion types fall into various categories as defined by the
** following enumeration.
*/
#define etRADIX       1			/* Integer types.  %d, %x, %o, and so forth */
#define etFLOAT       2			/* Floating point.  %f */
#define etEXP         3			/* Exponentional notation. %e and %E */
#define etGENERIC     4			/* Floating or exponential, depending on exponent. %g */
#define etSIZE        5			/* Return number of characters processed so far. %n */
#define etSTRING      6			/* Strings. %s */
#define etDYNSTRING   7			/* Dynamically allocated strings. %z */
#define etPERCENT     8			/* Percent symbol. %% */
#define etCHARX       9			/* Characters. %c */
/* The rest are extensions, not normally found in printf() */
#define etCHARLIT    10			/* Literal characters.  %' */
#define etSQLESCAPE  11			/* Strings with '\'' doubled.  %q */
#define etSQLESCAPE2 12			/* Strings with '\'' doubled and enclosed in '',
								   NULL pointers replaced by SQL NULL.  %Q */
#ifdef __UNSUPPORTED__
#define etTOKEN      13			/* a pointer to a Token structure */
#define etSRCLIST    14			/* a pointer to a SrcList */
#endif
#define etPOINTER    15			/* The %p conversion */
#define etSQLESCAPE3 16
#define etSQLESCAPE4 17

/*
** An "etByte" is an 8-bit unsigned value.
*/
typedef unsigned char etByte;

/*
** Each builtin conversion character (ex: the 'd' in "%d") is described
** by an instance of the following structure
*/
typedef struct et_info {		/* Information about each format field */
	char fmttype;				/* The format field code letter */
	etByte base;				/* The base for radix conversion */
	etByte flags;				/* One or more of FLAG_ constants below */
	etByte type;				/* Conversion paradigm */
	etByte charset;				/* Offset into aDigits[] of the digits string */
	etByte prefix;				/* Offset into aPrefix[] of the prefix string */
} et_info;

/*
** Allowed values for et_info.flags
*/
#define FLAG_SIGNED  1			/* True if the value to convert is signed */
#define FLAG_INTERN  2			/* True if for internal use only */
#define FLAG_STRING  4			/* Allow infinity precision */


/*
** The following table is searched linearly, so it is good to put the
** most frequently used conversion types first.
*/
static const char aDigits[] = "0123456789ABCDEF0123456789abcdef";
static const char aPrefix[] = "-x0\000X0";
static const et_info fmtinfo[] = {
	{'d', 10, 1, etRADIX, 0, 0},
	{'s', 0, 4, etSTRING, 0, 0},
	{'g', 0, 1, etGENERIC, 30, 0},
	{'z', 0, 6, etDYNSTRING, 0, 0},
	{'q', 0, 4, etSQLESCAPE, 0, 0},
	{'Q', 0, 4, etSQLESCAPE2, 0, 0},
	{'w', 0, 4, etSQLESCAPE3, 0, 0},
	{'y', 0, 4, etSQLESCAPE4, 0, 0},
	{'c', 0, 0, etCHARX, 0, 0},
	{'o', 8, 0, etRADIX, 0, 2},
	{'u', 10, 0, etRADIX, 0, 0},
	{'x', 16, 0, etRADIX, 16, 1},
	{'X', 16, 0, etRADIX, 0, 4},
#ifndef SWITCH_OMIT_FLOATING_POINT
	{'f', 0, 1, etFLOAT, 0, 0},
	{'e', 0, 1, etEXP, 30, 0},
	{'E', 0, 1, etEXP, 14, 0},
	{'G', 0, 1, etGENERIC, 14, 0},
#endif
	{'i', 10, 1, etRADIX, 0, 0},
	{'n', 0, 0, etSIZE, 0, 0},
	{'%', 0, 0, etPERCENT, 0, 0},
	{'p', 16, 0, etPOINTER, 0, 1},
#ifdef __UNSUPPORTED__
	{'T', 0, 2, etTOKEN, 0, 0},
	{'S', 0, 2, etSRCLIST, 0, 0},
#endif
};

#define etNINFO  (sizeof(fmtinfo)/sizeof(fmtinfo[0]))

/*
** If SWITCH_OMIT_FLOATING_POINT is defined, then none of the floating point
** conversions will work.
*/
#ifndef SWITCH_OMIT_FLOATING_POINT
/*
** "*val" is a double such that 0.1 <= *val < 10.0
** Return the ascii code for the leading digit of *val, then
** multiply "*val" by 10.0 to renormalize.
**
** Example:
**     input:     *val = 3.14159
**     output:    *val = 1.4159    function return = '3'
**
** The counter *cnt is incremented each time.  After counter exceeds
** 16 (the number of significant digits in a 64-bit float) '0' is
** always returned.
*/
static int et_getdigit(LONGDOUBLE_TYPE * val, int *cnt)
{
	int digit;
	LONGDOUBLE_TYPE d;
	if ((*cnt)++ >= 16)
		return '0';
	digit = (int) *val;
	d = digit;
	digit += '0';
	*val = (*val - d) * 10.0;
	return digit;
}
#endif /* SWITCH_OMIT_FLOATING_POINT */

/*
** On machines with a small stack size, you can redefine the
** SWITCH_PRINT_BUF_SIZE to be less than 350.  But beware - for
** smaller values some %f conversions may go into an infinite loop.
*/
#ifndef SWITCH_PRINT_BUF_SIZE
# define SWITCH_PRINT_BUF_SIZE 350
#endif
#define etBUFSIZE SWITCH_PRINT_BUF_SIZE	/* Size of the output buffer */

/*
** The root program.  All variations call this core.
**
** INPUTS:
**   func   This is a pointer to a function taking three arguments
**            1. A pointer to anything.  Same as the "arg" parameter.
**            2. A pointer to the list of characters to be output
**               (Note, this list is NOT null terminated.)
**            3. An integer number of characters to be output.
**               (Note: This number might be zero.)
**
**   arg    This is the pointer to anything which will be passed as the
**          first argument to "func".  Use it for whatever you like.
**
**   fmt    This is the format string, as in the usual print.
**
**   ap     This is a pointer to a list of arguments.  Same as in
**          vfprint.
**
** OUTPUTS:
**          The return value is the total number of characters sent to
**          the function "func".  Returns -1 on a error.
**
** Note that the order in which automatic variables are declared below
** seems to make a big difference in determining how fast this beast
** will run.
*/
static int vxprintf(void (*func) (void *, const char *, int),	/* Consumer of text */
					void *arg,	/* First argument to the consumer */
					int useExtended,	/* Allow extended %-conversions */
					const char *fmt,	/* Format string */
					va_list ap	/* arguments */
	)
{
	int c;						/* Next character in the format string */
	char *bufpt;				/* Pointer to the conversion buffer */
	int precision;				/* Precision of the current field */
	int length;					/* Length of the field */
	int idx;					/* A general purpose loop counter */
	int count;					/* Total number of characters output */
	int width;					/* Width of the current field */
	etByte flag_leftjustify;	/* True if "-" flag is present */
	etByte flag_plussign;		/* True if "+" flag is present */
	etByte flag_blanksign;		/* True if " " flag is present */
	etByte flag_alternateform;	/* True if "#" flag is present */
	etByte flag_altform2;		/* True if "!" flag is present */
	etByte flag_zeropad;		/* True if field width constant starts with zero */
	etByte flag_long;			/* True if "l" flag is present */
	etByte flag_longlong;		/* True if the "ll" flag is present */
	etByte done;				/* Loop termination flag */
	uint64_t longvalue;			/* Value for integer types */
	LONGDOUBLE_TYPE realvalue;	/* Value for real types */
	const et_info *infop;		/* Pointer to the appropriate info structure */
	char buf[etBUFSIZE];		/* Conversion buffer */
	char prefix;				/* Prefix character.  "+" or "-" or " " or '\0'. */
	etByte errorflag = 0;		/* True if an error is encountered */
	etByte xtype = 0;			/* Conversion paradigm */
	char *zExtra;				/* Extra memory used for etTCLESCAPE conversions */
	static const char spaces[] = "                                                                         ";
#define etSPACESIZE (sizeof(spaces)-1)
#ifndef SWITCH_OMIT_FLOATING_POINT
	int exp, e2;				/* exponent of real numbers */
	double rounder;				/* Used for rounding floating point values */
	etByte flag_dp;				/* True if decimal point should be shown */
	etByte flag_rtz;			/* True if trailing zeros should be removed */
	etByte flag_exp;			/* True to force display of the exponent */
	int nsd;					/* Number of significant digits returned */
#endif

	func(arg, "", 0);
	count = length = 0;
	bufpt = 0;
	for (; (c = (*fmt)) != 0; ++fmt) {
		if (c != '%') {
			int amt;
			bufpt = (char *) fmt;
			amt = 1;
			while ((c = (*++fmt)) != '%' && c != 0)
				amt++;
			(*func) (arg, bufpt, amt);
			count += amt;
			if (c == 0)
				break;
		}
		if ((c = (*++fmt)) == 0) {
			errorflag = 1;
			(*func) (arg, "%", 1);
			count++;
			break;
		}
		/* Find out what flags are present */
		flag_leftjustify = flag_plussign = flag_blanksign = flag_alternateform = flag_altform2 = flag_zeropad = 0;
		done = 0;
		do {
			switch (c) {
			case '-':
				flag_leftjustify = 1;
				break;
			case '+':
				flag_plussign = 1;
				break;
			case ' ':
				flag_blanksign = 1;
				break;
			case '#':
				flag_alternateform = 1;
				break;
			case '!':
				flag_altform2 = 1;
				break;
			case '0':
				flag_zeropad = 1;
				break;
			default:
				done = 1;
				break;
			}
		} while (!done && (c = (*++fmt)) != 0);
		/* Get the field width */
		width = 0;
		if (c == '*') {
			width = va_arg(ap, int);
			if (width < 0) {
				flag_leftjustify = 1;
				width = -width;
			}
			c = *++fmt;
		} else {
			while (c >= '0' && c <= '9') {
				width = width * 10 + c - '0';
				c = *++fmt;
			}
		}
		if (width > etBUFSIZE - 10) {
			width = etBUFSIZE - 10;
		}
		/* Get the precision */
		if (c == '.') {
			precision = 0;
			c = *++fmt;
			if (c == '*') {
				precision = va_arg(ap, int);
				if (precision < 0)
					precision = -precision;
				c = *++fmt;
			} else {
				while (c >= '0' && c <= '9') {
					precision = precision * 10 + c - '0';
					c = *++fmt;
				}
			}
		} else {
			precision = -1;
		}
		/* Get the conversion type modifier */
		if (c == 'l') {
			flag_long = 1;
			c = *++fmt;
			if (c == 'l') {
				flag_longlong = 1;
				c = *++fmt;
			} else {
				flag_longlong = 0;
			}
		} else {
			flag_long = flag_longlong = 0;
		}
		/* Fetch the info entry for the field */
		infop = 0;
		for (idx = 0; idx < etNINFO; idx++) {
			if (c == fmtinfo[idx].fmttype) {
				infop = &fmtinfo[idx];
				if (useExtended || (infop->flags & FLAG_INTERN) == 0) {
					xtype = infop->type;
				} else {
					return -1;
				}
				break;
			}
		}
		zExtra = 0;
		if (infop == 0) {
			return -1;
		}


		/* Limit the precision to prevent overflowing buf[] during conversion */
		if (precision > etBUFSIZE - 40 && (infop->flags & FLAG_STRING) == 0) {
			precision = etBUFSIZE - 40;
		}

		/*
		 ** At this point, variables are initialized as follows:
		 **
		 **   flag_alternateform          TRUE if a '#' is present.
		 **   flag_altform2               TRUE if a '!' is present.
		 **   flag_plussign               TRUE if a '+' is present.
		 **   flag_leftjustify            TRUE if a '-' is present or if the
		 **                               field width was negative.
		 **   flag_zeropad                TRUE if the width began with 0.
		 **   flag_long                   TRUE if the letter 'l' (ell) prefixed
		 **                               the conversion character.
		 **   flag_longlong               TRUE if the letter 'll' (ell ell) prefixed
		 **                               the conversion character.
		 **   flag_blanksign              TRUE if a ' ' is present.
		 **   width                       The specified field width.  This is
		 **                               always non-negative.  Zero is the default.
		 **   precision                   The specified precision.  The default
		 **                               is -1.
		 **   xtype                       The class of the conversion.
		 **   infop                       Pointer to the appropriate info struct.
		 */
		switch (xtype) {
		case etPOINTER:
			flag_longlong = sizeof(char *) == sizeof(int64_t);
			flag_long = sizeof(char *) == sizeof(long int);
			/* Fall through into the next case */
		case etRADIX:
			if (infop->flags & FLAG_SIGNED) {
				int64_t v;
				if (flag_longlong)
					v = va_arg(ap, int64_t);
				else if (flag_long)
					v = va_arg(ap, long int);
				else
					v = va_arg(ap, int);
				if (v < 0) {
					longvalue = -v;
					prefix = '-';
				} else {
					longvalue = v;
					if (flag_plussign)
						prefix = '+';
					else if (flag_blanksign)
						prefix = ' ';
					else
						prefix = 0;
				}
			} else {
				if (flag_longlong)
					longvalue = va_arg(ap, uint64_t);
				else if (flag_long)
					longvalue = va_arg(ap, unsigned long int);
				else
					longvalue = va_arg(ap, unsigned int);
				prefix = 0;
			}
			if (longvalue == 0)
				flag_alternateform = 0;
			if (flag_zeropad && precision < width - (prefix != 0)) {
				precision = width - (prefix != 0);
			}
			bufpt = &buf[etBUFSIZE - 1];
			{
				register const char *cset;	/* Use registers for speed */
				register int base;
				cset = &aDigits[infop->charset];
				base = infop->base;
				do {			/* Convert to ascii */
					*(--bufpt) = cset[longvalue % base];
					longvalue = longvalue / base;
				} while (longvalue > 0);
			}
			length = (int)(&buf[etBUFSIZE - 1] - bufpt);
			for (idx = precision - length; idx > 0; idx--) {
				*(--bufpt) = '0';	/* Zero pad */
			}
			if (prefix)
				*(--bufpt) = prefix;	/* Add sign */
			if (flag_alternateform && infop->prefix) {	/* Add "0" or "0x" */
				const char *pre;
				char x;
				pre = &aPrefix[infop->prefix];
				if (*bufpt != pre[0]) {
					for (; (x = (*pre)) != 0; pre++)
						*(--bufpt) = x;
				}
			}
			length = (int)(&buf[etBUFSIZE - 1] - bufpt);
			break;
		case etFLOAT:
		case etEXP:
		case etGENERIC:
			realvalue = va_arg(ap, double);
#ifndef SWITCH_OMIT_FLOATING_POINT
			if (precision < 0)
				precision = 6;	/* Set default precision */
			if (precision > etBUFSIZE / 2 - 10)
				precision = etBUFSIZE / 2 - 10;
			if (realvalue < 0.0) {
				realvalue = -realvalue;
				prefix = '-';
			} else {
				if (flag_plussign)
					prefix = '+';
				else if (flag_blanksign)
					prefix = ' ';
				else
					prefix = 0;
			}
			if (xtype == etGENERIC && precision > 0)
				precision--;
#if 0
			/* Rounding works like BSD when the constant 0.4999 is used.  Wierd! */
			for (idx = precision, rounder = 0.4999; idx > 0; idx--, rounder *= 0.1);
#else
			/* It makes more sense to use 0.5 */
			for (idx = precision, rounder = 0.5; idx > 0; idx--, rounder *= 0.1) {
			}
#endif
			if (xtype == etFLOAT)
				realvalue += rounder;
			/* Normalize realvalue to within 10.0 > realvalue >= 1.0 */
			exp = 0;
			if (realvalue > 0.0) {
				while (realvalue >= 1e32 && exp <= 350) {
					realvalue *= 1e-32;
					exp += 32;
				}
				while (realvalue >= 1e8 && exp <= 350) {
					realvalue *= 1e-8;
					exp += 8;
				}
				while (realvalue >= 10.0 && exp <= 350) {
					realvalue *= 0.1;
					exp++;
				}
				while (realvalue < 1e-8 && exp >= -350) {
					realvalue *= 1e8;
					exp -= 8;
				}
				while (realvalue < 1.0 && exp >= -350) {
					realvalue *= 10.0;
					exp--;
				}
				if (exp > 350 || exp < -350) {
					bufpt = "NaN";
					length = 3;
					break;
				}
			}
			bufpt = buf;
			/*
			 ** If the field type is etGENERIC, then convert to either etEXP
			 ** or etFLOAT, as appropriate.
			 */
			flag_exp = xtype == etEXP;
			if (xtype != etFLOAT) {
				realvalue += rounder;
				if (realvalue >= 10.0) {
					realvalue *= 0.1;
					exp++;
				}
			}
			if (xtype == etGENERIC) {
				flag_rtz = !flag_alternateform;
				if (exp < -4 || exp > precision) {
					xtype = etEXP;
				} else {
					precision = precision - exp;
					xtype = etFLOAT;
				}
			} else {
				flag_rtz = 0;
			}
			if (xtype == etEXP) {
				e2 = 0;
			} else {
				e2 = exp;
			}
			nsd = 0;
			flag_dp = (precision > 0) | flag_alternateform | flag_altform2;
			/* The sign in front of the number */
			if (prefix) {
				*(bufpt++) = prefix;
			}
			/* Digits prior to the decimal point */
			if (e2 < 0) {
				*(bufpt++) = '0';
			} else {
				for (; e2 >= 0; e2--) {
					*(bufpt++) = (char) et_getdigit(&realvalue, &nsd);
				}
			}
			/* The decimal point */
			if (flag_dp) {
				*(bufpt++) = '.';
			}
			/* "0" digits after the decimal point but before the first
			 ** significant digit of the number */
			for (e2++; e2 < 0 && precision > 0; precision--, e2++) {
				*(bufpt++) = '0';
			}
			/* Significant digits after the decimal point */
			while ((precision--) > 0) {
				*(bufpt++) = (char) et_getdigit(&realvalue, &nsd);
			}
			/* Remove trailing zeros and the "." if no digits follow the "." */
			if (flag_rtz && flag_dp) {
				while (bufpt[-1] == '0')
					*(--bufpt) = 0;
				assert(bufpt > buf);
				if (bufpt[-1] == '.') {
					if (flag_altform2) {
						*(bufpt++) = '0';
					} else {
						*(--bufpt) = 0;
					}
				}
			}
			/* Add the "eNNN" suffix */
			if (flag_exp || (xtype == etEXP && exp)) {
				*(bufpt++) = aDigits[infop->charset];
				if (exp < 0) {
					*(bufpt++) = '-';
					exp = -exp;
				} else {
					*(bufpt++) = '+';
				}
				if (exp >= 100) {
					*(bufpt++) = (char) (exp / 100) + '0';	/* 100's digit */
					exp %= 100;
				}
				*(bufpt++) = (char) exp / 10 + '0';	/* 10's digit */
				*(bufpt++) = exp % 10 + '0';	/* 1's digit */
			}
			*bufpt = 0;

			/* The converted number is in buf[] and zero terminated. Output it.
			 ** Note that the number is in the usual order, not reversed as with
			 ** integer conversions. */
			length = (int)(bufpt - buf);
			bufpt = buf;

			/* Special case:  Add leading zeros if the flag_zeropad flag is
			 ** set and we are not left justified */
			if (flag_zeropad && !flag_leftjustify && length < width) {
				int i;
				int nPad = width - length;
				for (i = width; i >= nPad; i--) {
					bufpt[i] = bufpt[i - nPad];
				}
				i = prefix != 0;
				while (nPad--)
					bufpt[i++] = '0';
				length = width;
			}
#endif
			break;
		case etSIZE:
			*(va_arg(ap, int *)) = count;
			length = width = 0;
			break;
		case etPERCENT:
			buf[0] = '%';
			bufpt = buf;
			length = 1;
			break;
		case etCHARLIT:
		case etCHARX:
			c = buf[0] = (char) (xtype == etCHARX ? va_arg(ap, int) : *++fmt);
			if (precision >= 0) {
				for (idx = 1; idx < precision; idx++)
					buf[idx] = (char) c;
				length = precision;
			} else {
				length = 1;
			}
			bufpt = buf;
			break;
		case etSTRING:
		case etDYNSTRING:
			bufpt = va_arg(ap, char *);
			if (bufpt == 0) {
				bufpt = "";
			} else if (xtype == etDYNSTRING) {
				zExtra = bufpt;
			}
			length = (int)strlen(bufpt);
			if (precision >= 0 && precision < length)
				length = precision;
			break;
		case etSQLESCAPE:
		case etSQLESCAPE2:
		case etSQLESCAPE4:
		case etSQLESCAPE3:{
				size_t i, j, n, ch;
				int needQuote, isnull;
				char *escarg = va_arg(ap, char *);
				isnull = escarg == 0;
				if (isnull)
					escarg = (xtype == etSQLESCAPE2 ? "NULL" : "(NULL)");
				for (i = n = 0; (ch = escarg[i]) != 0; i++) {
					if (ch == '\'' || (xtype == etSQLESCAPE3 && ch == '\\'))
						n++;
				}
				needQuote = !isnull && xtype == etSQLESCAPE2;
				n += i + 1 + needQuote * 2;
				if (n > etBUFSIZE) {
					bufpt = zExtra = malloc(n);
					if (bufpt == 0)
						return -1;
				} else {
					bufpt = buf;
				}
				j = 0;
				if (needQuote)
					bufpt[j++] = '\'';
				for (i = 0; (ch = escarg[i]) != 0; i++) {
					bufpt[j++] = (char) ch;
					if (xtype == etSQLESCAPE4) {
						if (ch == '\'' || (xtype == etSQLESCAPE3 && ch == '\\')) {
							bufpt[j] = (char) ch;
							bufpt[j-1] = (char) '\\';
							j++;
						}
					} else {
						if (ch == '\'' || (xtype == etSQLESCAPE3 && ch == '\\'))
							bufpt[j++] = (char) ch;
					}
				}
				if (needQuote)
					bufpt[j++] = '\'';
				bufpt[j] = 0;
				length = j;
				/* The precision is ignored on %q and %Q */
				/* if ( precision>=0 && precision<length ) length = precision; */
				break;
			}
#ifdef __UNSUPPORTED__
		case etTOKEN:{
				Token *pToken = va_arg(ap, Token *);
				if (pToken && pToken->z) {
					(*func) (arg, (char *) pToken->z, pToken->n);
				}
				length = width = 0;
				break;
			}
		case etSRCLIST:{
				SrcList *pSrc = va_arg(ap, SrcList *);
				int k = va_arg(ap, int);
				struct SrcList_item *pItem = &pSrc->a[k];
				assert(k >= 0 && k < pSrc->nSrc);
				if (pItem->zDatabase && pItem->zDatabase[0]) {
					(*func) (arg, pItem->zDatabase, strlen(pItem->zDatabase));
					(*func) (arg, ".", 1);
				}
				(*func) (arg, pItem->zName, strlen(pItem->zName));
				length = width = 0;
				break;
			}
#endif
		}						/* End switch over the format type */
		/*
		 ** The text of the conversion is pointed to by "bufpt" and is
		 ** "length" characters long.  The field width is "width".  Do
		 ** the output.
		 */
		if (!flag_leftjustify) {
			register int nspace;
			nspace = width - length;
			if (nspace > 0) {
				count += nspace;
				while (nspace >= etSPACESIZE) {
					(*func) (arg, spaces, etSPACESIZE);
					nspace -= etSPACESIZE;
				}
				if (nspace > 0)
					(*func) (arg, spaces, nspace);
			}
		}
		if (length > 0) {
			(*func) (arg, bufpt, length);
			count += length;
		}
		if (flag_leftjustify) {
			register int nspace;
			nspace = width - length;
			if (nspace > 0) {
				count += nspace;
				while (nspace >= etSPACESIZE) {
					(*func) (arg, spaces, etSPACESIZE);
					nspace -= etSPACESIZE;
				}
				if (nspace > 0)
					(*func) (arg, spaces, nspace);
			}
		}
		if (zExtra) {
			free(zExtra);
		}
	}							/* End for loop over the format string */
	return errorflag ? -1 : count;
}								/* End of function */


/* This structure is used to store state information about the
** write to memory that is currently in progress.
*/
struct sgMprintf {
	char *zBase;				/* A base allocation */
	char *zText;				/* The string collected so far */
	int nChar;					/* Length of the string so far */
	int nTotal;					/* Output size if unconstrained */
	int nAlloc;					/* Amount of space allocated in zText */
	void *(*xRealloc) (void *, int);	/* Function used to realloc memory */
};

/*
** This function implements the callback from vxprintf.
**
** This routine add nNewChar characters of text in zNewText to
** the sgMprintf structure pointed to by "arg".
*/
static void mout(void *arg, const char *zNewText, int nNewChar)
{
	struct sgMprintf *pM = (struct sgMprintf *) arg;
	pM->nTotal += nNewChar;
	if (pM->nChar + nNewChar + 1 > pM->nAlloc) {
		if (pM->xRealloc == 0) {
			nNewChar = pM->nAlloc - pM->nChar - 1;
		} else {
			pM->nAlloc = pM->nChar + nNewChar * 2 + 1;
			if (pM->zText == pM->zBase) {
				pM->zText = pM->xRealloc(0, pM->nAlloc);
				if (pM->zText && pM->nChar) {
					memcpy(pM->zText, pM->zBase, pM->nChar);
				}
			} else {
				char *zNew;
				zNew = pM->xRealloc(pM->zText, pM->nAlloc);
				if (zNew) {
					pM->zText = zNew;
				}
			}
		}
	}
	if (pM->zText) {
		if (nNewChar > 0) {
			memcpy(&pM->zText[pM->nChar], zNewText, nNewChar);
			pM->nChar += nNewChar;
		}
		pM->zText[pM->nChar] = 0;
	}
}

/*
** This routine is a wrapper around xprintf() that invokes mout() as
** the consumer.
*/
static char *base_vprintf(void *(*xRealloc) (void *, int),	/* Routine to realloc memory. May be NULL */
						  int useInternal,	/* Use internal %-conversions if true */
						  char *zInitBuf,	/* Initially write here, before mallocing */
						  int nInitBuf,	/* Size of zInitBuf[] */
						  const char *zFormat,	/* format string */
						  va_list ap	/* arguments */
	)
{
	struct sgMprintf sM;
	sM.zBase = sM.zText = zInitBuf;
	sM.nChar = sM.nTotal = 0;
	sM.nAlloc = nInitBuf;
	sM.xRealloc = xRealloc;
	vxprintf(mout, &sM, useInternal, zFormat, ap);
	if (xRealloc) {
		if (sM.zText == sM.zBase) {
			sM.zText = xRealloc(0, sM.nChar + 1);
			if (sM.zText) {
				memcpy(sM.zText, sM.zBase, sM.nChar + 1);
			}
		} else if (sM.nAlloc > sM.nChar + 10) {
			char *zNew = xRealloc(sM.zText, sM.nChar + 1);
			if (zNew) {
				sM.zText = zNew;
			}
		}
	}
	return sM.zText;
}

/*
** Realloc that is a real function, not a macro.
*/
static void *printf_realloc(void *old, int size)
{
	return realloc(old, size);
}

/*
** Print into memory. Omit the internal %-conversion extensions.
*/
SWITCH_DECLARE(char *) switch_vmprintf(const char *zFormat, va_list ap)
{
	char zBase[SWITCH_PRINT_BUF_SIZE];
	return base_vprintf(printf_realloc, 0, zBase, sizeof(zBase), zFormat, ap);
}

/*
** Print into memory.  Omit the internal %-conversion extensions.
*/
SWITCH_DECLARE(char *) switch_mprintf(const char *zFormat, ...)
{
	va_list ap;
	char *z;
	char zBase[SWITCH_PRINT_BUF_SIZE];
	va_start(ap, zFormat);
	z = base_vprintf(printf_realloc, 0, zBase, sizeof(zBase), zFormat, ap);
	va_end(ap);
	return z;
}

/*
** sqlite3_snprintf() works like snprintf() except that it ignores the
** current locale settings.  This is important for SQLite because we
** are not able to use a "," as the decimal point in place of "." as
** specified by some locales.
*/
SWITCH_DECLARE(char *) switch_snprintfv(char *zBuf, int n, const char *zFormat, ...)
{
	char *z;
	va_list ap;

	va_start(ap, zFormat);
	z = base_vprintf(0, 0, zBuf, n, zFormat, ap);
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
