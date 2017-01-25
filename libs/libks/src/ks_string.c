/*
 * Copyright (c) 2007-2014, Anthony Minessale II
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ks.h>

#define ESCAPE_META '\\'

/* Written by Marc Espie, public domain */
#define KS_CTYPE_NUM_CHARS       256

const short _ks_C_toupper_[1 + KS_CTYPE_NUM_CHARS] = {
	EOF,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

const short *_ks_toupper_tab_ = _ks_C_toupper_;

KS_DECLARE(int) ks_toupper(int c)
{
	if ((unsigned int) c > 255)
		return (c);
	if (c < -1)
		return EOF;
	return ((_ks_toupper_tab_ + 1)[c]);
}

const short _ks_C_tolower_[1 + KS_CTYPE_NUM_CHARS] = {
	EOF,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

const short *_ks_tolower_tab_ = _ks_C_tolower_;

KS_DECLARE(int) ks_tolower(int c)
{
	if ((unsigned int) c > 255)
		return (c);
	if (c < -1)
		return EOF;
	return ((_ks_tolower_tab_ + 1)[c]);
}

KS_DECLARE(const char *) ks_stristr(const char *instr, const char *str)
{
/*
** Rev History:  16/07/97  Greg Thayer		Optimized
**               07/04/95  Bob Stout		ANSI-fy
**               02/03/94  Fred Cole		Original
**               09/01/03  Bob Stout		Bug fix (lines 40-41) per Fred Bulback
**
** Hereby donated to public domain.
*/
	const char *pptr, *sptr, *start;

	if (!str || !instr)
		return NULL;

	for (start = str; *start; start++) {
		/* find start of pattern in string */
		for (; ((*start) && (ks_toupper(*start) != ks_toupper(*instr))); start++);

		if (!*start)
			return NULL;

		pptr = instr;
		sptr = start;

		while (ks_toupper(*sptr) == ks_toupper(*pptr)) {
			sptr++;
			pptr++;

			/* if end of pattern then pattern was found */
			if (!*pptr)
				return (start);

			if (!*sptr)
				return NULL;
		}
	}
	return NULL;
}

#ifdef WIN32
#ifndef vsnprintf
#define vsnprintf _vsnprintf
#endif
#endif


int vasprintf(char **ret, const char *format, va_list ap);

KS_DECLARE(int) ks_vasprintf(char **ret, const char *fmt, va_list ap)
{
#if !defined(WIN32) && !defined(__sun)
	return vasprintf(ret, fmt, ap);
#else
	char *buf;
	int len;
	size_t buflen;
	va_list ap2;
	char *tmp = NULL;

#ifdef _MSC_VER
#if _MSC_VER >= 1500
	/* hack for incorrect assumption in msvc header files for code analysis */
	__analysis_assume(tmp);
#endif
	ap2 = ap;
#else
	va_copy(ap2, ap);
#endif

	len = vsnprintf(tmp, 0, fmt, ap2);

	if (len > 0 && (buf = malloc((buflen = (size_t) (len + 1)))) != NULL) {
		len = vsnprintf(buf, buflen, fmt, ap);
		*ret = buf;
	} else {
		*ret = NULL;
		len = -1;
	}

	va_end(ap2);
	return len;
#endif
}


KS_DECLARE(int) ks_snprintf(char *buffer, size_t count, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vsnprintf(buffer, count - 1, fmt, ap);
	if (ret < 0)
		buffer[count - 1] = '\0';
	va_end(ap);
	return ret;
}


/* Helper function used when separating strings to unescape a character. The
   supported characters are:

   \n  linefeed
   \r  carriage return
   \t  tab
   \s  space

   Any other character is returned as it was received. */
static char unescape_char(char escaped)
{
	char unescaped;

	switch (escaped) {
	case 'n':
		unescaped = '\n';
		break;
	case 'r':
		unescaped = '\r';
		break;
	case 't':
		unescaped = '\t';
		break;
	case 's':
		unescaped = ' ';
		break;
	default:
		unescaped = escaped;
	}
	return unescaped;
}


/* Helper function used when separating strings to remove quotes, leading /
   trailing spaces, and to convert escaped characters. */
static char *cleanup_separated_string(char *str, char delim)
{
	char *ptr;
	char *dest;
	char *start;
	char *end = NULL;
	int inside_quotes = 0;

	/* Skip initial whitespace */
	for (ptr = str; *ptr == ' '; ++ptr) {
	}

	for (start = dest = ptr; *ptr; ++ptr) {
		char e;
		int esc = 0;

		if (*ptr == ESCAPE_META) {
			e = *(ptr + 1);
			if (e == '\'' || e == '"' || (delim && e == delim) || e == ESCAPE_META || (e = unescape_char(*(ptr + 1))) != *(ptr + 1)) {
				++ptr;
				*dest++ = e;
				end = dest;
				esc++;
			}
		}
		if (!esc) {
			if (*ptr == '\'' && (inside_quotes || ((ptr+1) && strchr(ptr+1, '\'')))) {
				if ((inside_quotes = (1 - inside_quotes))) {
					end = dest;
				}
			} else {
				*dest++ = *ptr;
				if (*ptr != ' ' || inside_quotes) {
					end = dest;
				}
			}
		}
	}
	if (end) {
		*end = '\0';
	}

	return start;
}

KS_DECLARE(unsigned int) ks_separate_string_string(char *buf, const char *delim, char **array, unsigned int arraylen)
{
	unsigned int count = 0;
	char *d;
	size_t dlen = strlen(delim);

	array[count++] = buf;

	while (count < arraylen && array[count - 1]) {
		if ((d = strstr(array[count - 1], delim))) {
			*d = '\0';
			d += dlen;
			array[count++] = d;
		} else
			break;
	}

	return count;
}

KS_DECLARE(char *) ks_copy_string(char *from_str, const char *to_str, ks_size_t from_str_len)
{
	char *p, *e;

	if (!from_str)
		return NULL;
	if (!to_str) {
		*from_str = '\0';
		return from_str;
	}

	e = from_str + from_str_len - 1;

	for (p = from_str; p < e; ++p, ++to_str) {
		if (!(*p = *to_str)) {
			return p;
		}
	}

	*p = '\0';

	return p;
}


/* Separate a string using a delimiter that is not a space */
static unsigned int separate_string_char_delim(char *buf, char delim, char **array, unsigned int arraylen)
{
	enum tokenizer_state {
		START,
		FIND_DELIM
	} state = START;

	unsigned int count = 0;
	char *ptr = buf;
	int inside_quotes = 0;
	unsigned int i;

	while (*ptr && count < arraylen) {
		switch (state) {
		case START:
			array[count++] = ptr;
			state = FIND_DELIM;
			break;

		case FIND_DELIM:
			/* escaped characters are copied verbatim to the destination string */
			if (*ptr == ESCAPE_META) {
				++ptr;
			} else if (*ptr == '\'' && (inside_quotes || ((ptr+1) && strchr(ptr+1, '\'')))) {
				inside_quotes = (1 - inside_quotes);
			} else if (*ptr == delim && !inside_quotes) {
				*ptr = '\0';
				state = START;
			}
			++ptr;
			break;
		}
	}
	/* strip quotes, escaped chars and leading / trailing spaces */

	for (i = 0; i < count; ++i) {
		array[i] = cleanup_separated_string(array[i], delim);
	}

	return count;
}

/* Separate a string using a delimiter that is a space */
static unsigned int separate_string_blank_delim(char *buf, char **array, unsigned int arraylen)
{
	enum tokenizer_state {
		START,
		SKIP_INITIAL_SPACE,
		FIND_DELIM,
		SKIP_ENDING_SPACE
	} state = START;

	unsigned int count = 0;
	char *ptr = buf;
	int inside_quotes = 0;
	unsigned int i;

	while (*ptr && count < arraylen) {
		switch (state) {
		case START:
			array[count++] = ptr;
			state = SKIP_INITIAL_SPACE;
			break;

		case SKIP_INITIAL_SPACE:
			if (*ptr == ' ') {
				++ptr;
			} else {
				state = FIND_DELIM;
			}
			break;

		case FIND_DELIM:
			if (*ptr == ESCAPE_META) {
				++ptr;
			} else if (*ptr == '\'') {
				inside_quotes = (1 - inside_quotes);
			} else if (*ptr == ' ' && !inside_quotes) {
				*ptr = '\0';
				state = SKIP_ENDING_SPACE;
			}
			++ptr;
			break;

		case SKIP_ENDING_SPACE:
			if (*ptr == ' ') {
				++ptr;
			} else {
				state = START;
			}
			break;
		}
	}
	/* strip quotes, escaped chars and leading / trailing spaces */

	for (i = 0; i < count; ++i) {
		array[i] = cleanup_separated_string(array[i], 0);
	}
	
	return count;
}

KS_DECLARE(unsigned int) ks_separate_string(char *buf, char delim, char **array, unsigned int arraylen)
{
	if (!buf || !array || !arraylen) {
		return 0;
	}


	if (*buf == '^' && *(buf+1) == '^') {
		char *p = buf + 2;
		
		if (p && *p && *(p+1)) {
			buf = p;
			delim = *buf++;
		}
	}


	memset(array, 0, arraylen * sizeof(*array));

	return (delim == ' ' ? separate_string_blank_delim(buf, array, arraylen) : separate_string_char_delim(buf, delim, array, arraylen));
}
