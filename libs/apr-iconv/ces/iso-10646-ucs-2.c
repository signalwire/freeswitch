/*-
 * Copyright (c) 1999,2000
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

#include <stdlib.h>	/* free, malloc */

#define ICONV_INTERNAL
#include <iconv.h>

static const char * const names[] = {
	"iso-10646-ucs-2", "csUnicode", "ucs-2", NULL
};

static const char * const *
ucs2_names(struct iconv_ces *ces)
{
	return names;
}

static apr_ssize_t
convert_from_ucs(struct iconv_ces *ces, ucs_t in,
	unsigned char **outbuf, apr_size_t *outbytesleft)
{
	apr_size_t bytes;
	int *state = ces->data;

	if (in == UCS_CHAR_NONE)
    		return 1;	/* No state reinitialization for table charsets */
	if (iconv_char32bit(in))
		return -1;	/* No such character in UCS-2 */
	bytes = *state ? 2 : 4;
	if (*outbytesleft < bytes)
		return 0;	/* No space in the output buffer */
	if (*state) {
		*(*outbuf)++ = 0xFE;
		*(*outbuf)++ = 0xFF;
		*state = 1;
	}
	*(*outbuf)++ = (in >> 8) & 0xFF;
	*(*outbuf)++ = in & 0xFF;
	*outbytesleft -= bytes;
	return 1;
}

static APR_INLINE ucs_t
msb(const unsigned char *buf)
{
	return (buf[0] << 8) | buf[1];
}

static ucs_t 
convert_to_ucs(struct iconv_ces *ces, const unsigned char **inbuf,
	apr_size_t *inbytesleft)
{
	ucs_t res;
	int *state = ces->data;
	int mark;

	if (*inbytesleft < 2)
		return UCS_CHAR_NONE;	/* Not enough bytes in the input buffer */
	res = msb((const unsigned char*)*inbuf);
	switch (res) {
	    case UCS_CHAR_ZERO_WIDTH_NBSP:
		*state = 1;
		mark = 1;
		break;
	    case UCS_CHAR_INVALID:
		*state = 2;
		mark = 1;
		break;
	    default:
		mark = 0;
	}
	if (mark) {
		if (*inbytesleft < 4)
			return UCS_CHAR_NONE;	/* Not enough bytes in the input buffer */
		*inbytesleft -= 2;
		res = msb(*inbuf += 2);
	}
	if (*state == 2) {
		res = (unsigned char)(*(*inbuf) ++);
		res |= (unsigned char)(*(*inbuf) ++) << 8;
	} else
		*inbuf += 2;
	*inbytesleft -= 2;
	return res;
}

static const struct iconv_ces_desc iconv_ces_desc = {
	apr_iconv_ces_open_func,
	apr_iconv_ces_close_func,
	apr_iconv_ces_reset_func,
	ucs2_names,
	apr_iconv_ces_nbits8,
	apr_iconv_ces_zero,
	convert_from_ucs,
	convert_to_ucs,
	NULL
};

struct iconv_module_desc iconv_module = {
	ICMOD_UC_CES,
	apr_iconv_mod_noevent,
	NULL,
	&iconv_ces_desc
};
