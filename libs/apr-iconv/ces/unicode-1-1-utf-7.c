/*-
 * Copyright (c) 2000
 *	Konstantin Chuguev.  All rights reserved.
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
 *	This product includes software developed by Konstantin Chuguev
 *	and its contributors.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	iconv (Charset Conversion Library) v1.0
 */

#include <stdlib.h>

#define ICONV_INTERNAL
#include <iconv.h>

static const char * const names[] = {
	"unicode-1-1-utf-7",
	"utf-7",
	NULL
};

static const char * const *
utf7_names(struct iconv_ces *ces)
{
	return names;
}

static APR_INLINE int
lackofbytes(int bytes, apr_size_t *bytesleft)
{
	if (bytes > *bytesleft)
	    return 1;
	(*bytesleft) -= bytes;
	return 0;
}

static const char *base64_str = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                "abcdefghijklmnopqrstuvwxyz"
                                "0123456789+/";

#define base64(ch) (base64_str[(ch) & 0x3F])

static APR_INLINE int output(char ch, unsigned char **outbuf)
{
	*(*outbuf)++ = ch;
	return 1;
}

static int
encode(char *state, ucs_t ch, unsigned char **outbuf)
{
	switch (state[0]) {
	    case 2:
		output(base64(state[1] | (ch >> 14)), outbuf);
		output(base64(ch >> 8), outbuf);
		output(base64(ch >> 2), outbuf);
		state[1] = ch << 4;
		state[0] = 3;
		break;
	    case 3:
		output(base64(state[1] | (ch >> 12)), outbuf);
		output(base64(ch >> 6), outbuf);
		output(base64(ch), outbuf);
		state[0] = 1;
		break;
	    default:
		output(base64(ch >> 10), outbuf);
		output(base64(ch >> 4), outbuf);
		state[1] = ch << 2;
		state[0] = 2;
	}
	return 1;
}

enum { utf7_printable, utf7_base64, utf7_encoded, utf7_shift_in,
       utf7_shift_out, utf7_end };

#define between(ch, min, max) ((min) <= (ch) && (ch) <= (max))

static int char_type(ucs_t ch)
{
	switch (ch) {
	    case UCS_CHAR_NONE:
		return utf7_end;
	    case '-':
		return utf7_shift_out;
	    case '+':
		return utf7_shift_in;
	    case ':':
	    case '?':
		return utf7_printable;
	}
	return (between(ch, '/', '9') || between(ch, 'A', 'Z') ||
                between(ch, 'a', 'z')) ?
		    utf7_base64 :
		    (ch <= ' ' || (between(ch, '\'', '.') && ch != '*')) ?
			utf7_printable :
			utf7_encoded;
}

static apr_ssize_t
convert_from_ucs(struct iconv_ces *module, ucs_t in,
                 unsigned char **outbuf, apr_size_t *outbytesleft)
{
#define utf7_state ((char *)(module->data))
    int ch = char_type(in), needbytes = 3;
    if (iconv_char32bit(in))
        return -1;
    if (utf7_state[0]) {
        needbytes = utf7_state[0] > 1 ? 1 : 0;
        switch (ch) {
        case utf7_encoded:
        case utf7_shift_in:
            return lackofbytes(needbytes + 2, outbytesleft)
                   ? 0 : encode(utf7_state, in, outbuf);
        case utf7_base64:
        case utf7_shift_out:
            needbytes ++;
        case utf7_printable:
            needbytes ++;
            break;
        default:
            if (needbytes) {
                output(base64(utf7_state[1]), outbuf);
                (*outbytesleft) --;
            }
            return 1;
        }
        if (lackofbytes(needbytes, outbytesleft))
            return 0;
        if (utf7_state[0] > 1)
            output(base64(utf7_state[1]), outbuf);
        if (ch != utf7_printable)
            output('-', outbuf);
        utf7_state[0] = 0;
        return output((unsigned char)in, outbuf);
    }
    switch (ch) {
    case utf7_end:
        return 1;
    case utf7_base64:
    case utf7_printable:
    case utf7_shift_out:
        (*outbytesleft) --;
        return output((unsigned char)in, outbuf);
    case utf7_shift_in:
        needbytes = 2;
    }
    if (lackofbytes(needbytes, outbytesleft))
        return 0;
    output('+', outbuf);
    return ch == utf7_shift_in ? output('-', outbuf)
                               : encode(utf7_state, in, outbuf);
#undef utf7_state
}

static ucs_t base64_input(const unsigned char **inbuf, int *error)
{
    unsigned char ch = *(*inbuf)++;
    if (between(ch, 'A', 'Z'))
        return ch - 'A';
    else if (between(ch, 'a', 'z'))
        return ch - 'a' + 26;
    else if (between(ch, '0', '9'))
        return ch - '0' + 52;
    else if (ch == '+')
        return 62;
    else if (ch == '/')
        return 63;
    *error = 1;
    return UCS_CHAR_INVALID;
}

static ucs_t decode(char *state, const unsigned char **inbuf)
{
    int errflag = 0;
    ucs_t res, ch;
    switch (state[0]) {
    case 2:
        res = ((unsigned)(state[1]) << 14)
              | (base64_input(inbuf, &errflag) << 8)
              | (base64_input(inbuf, &errflag) << 2)
              | ((ch = base64_input(inbuf, &errflag)) >> 4);
        if (errflag)
            return UCS_CHAR_INVALID;
        state[1] = ch;
        state[0] = 3;
        break;
    case 3:
        res = ((unsigned)(state[1]) << 12)
              | (base64_input(inbuf, &errflag) << 6)
              | base64_input(inbuf, &errflag);
        if (errflag)
            return UCS_CHAR_INVALID;
        state[0] = 1;
        break;
    default:
        res = (base64_input(inbuf, &errflag) << 10)
              | (base64_input(inbuf, &errflag) << 4)
              | ((ch = base64_input(inbuf, &errflag)) >> 2);
        if (errflag)
            return UCS_CHAR_INVALID;
        state[1] = ch;
        state[0] = 2;
    }
    return res & 0xFFFF;
}

static ucs_t convert_to_ucs(struct iconv_ces *module,
                            const unsigned char **inbuf, apr_size_t *inbytesleft)
{
#define utf7_state ((char *)(module->data))
    int ch = char_type(*(unsigned char *)*inbuf), needbytes = 0;
    if (ch == utf7_encoded)
        return lackofbytes(1, inbytesleft) ? UCS_CHAR_NONE
                                           : UCS_CHAR_INVALID;
    if (utf7_state[0]) {
        switch (ch) {
        case utf7_shift_out:
            if (*inbytesleft < 2)
                return UCS_CHAR_NONE;
            needbytes = 1;
            ch = char_type(*(++((unsigned char *)*inbuf)));
            (*inbytesleft) --;
        case utf7_printable:
            utf7_state[0] = 0;
            break;
        default:
            return lackofbytes(utf7_state[0] > 2 ? 2 : 3, inbytesleft)
                   ? UCS_CHAR_NONE : decode(utf7_state, inbuf);
        }
    }
    if (ch == utf7_shift_in) {
        if (*inbytesleft < 2) {
            (*inbuf) -= needbytes;
            (*inbytesleft) += needbytes;
            return UCS_CHAR_NONE;
        }
        switch (char_type(*(++(unsigned char *)*inbuf))) {
        case utf7_shift_out:
            (*inbuf) ++;
            (*inbytesleft) -= 2;
            return '+';
        case utf7_base64:
        case utf7_shift_in:
            if (lackofbytes(4, inbytesleft)) {
                (*inbuf) -= (++needbytes);
                (*inbytesleft) += needbytes;
                return UCS_CHAR_NONE;
            }
            return decode(utf7_state, inbuf);
        }
        (*inbytesleft) --;
        return UCS_CHAR_INVALID;
    }
    (*inbytesleft) --;
    return *((unsigned char *)*inbuf) ++;
#undef utf7_state
}

static const struct iconv_ces_desc iconv_ces_desc = {
	apr_iconv_ces_open_func,
	apr_iconv_ces_close_func,
	apr_iconv_ces_reset_func,
	utf7_names,
	apr_iconv_ces_nbits7,
	apr_iconv_ces_zero,
	convert_from_ucs,
	convert_to_ucs, NULL
};

struct iconv_module_desc iconv_module = {
	ICMOD_UC_CES,
	apr_iconv_mod_noevent,
	NULL,
	&iconv_ces_desc
};
