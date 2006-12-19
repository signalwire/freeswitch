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

/*#define ICONV_DEBUG*/
#define ICONV_INTERNAL
#include <iconv.h>

static const char * const names[] = {
    "utf-8", "iso-10646-utf-8", "utf8", NULL
};

static const char * const *
utf8_names(struct iconv_ces *ces)
{
	return names;
}

#define cont_byte(b) (((b) & 0x3F) | 0x80)

static apr_ssize_t
convert_from_ucs(struct iconv_ces *ces, ucs_t in,
	unsigned char **outbuf, apr_size_t *outbytesleft)
{
	unsigned char *cp;
	int n;
	if (in == UCS_CHAR_NONE)
		return 1;	/* No state reinitialization for table charsets */
	if (in < 0x80) {
		n = 1;
	} else if (in < 0x800) {
		n = 2;
	} else if (in < 0x10000) {
		n = 3;
	} else if (in < 0x110000) {
		n = 4;
	} else
		return -1;
	if (*outbytesleft < n)
		return 0;
	cp = *outbuf;
	switch (n) {
	    case 1:
		*cp = (unsigned char)in;
		break;
	    case 2:
		*cp++ = (unsigned char)((in >> 6) | 0xC0);
		*cp++ = (unsigned char)cont_byte(in);
		break;
	    case 3:
		*cp++ = (unsigned char)((in >> 12) | 0xE0);
		*cp++ = (unsigned char)cont_byte(in >> 6);
		*cp++ = (unsigned char)cont_byte(in);
		break;
	    case 4:
		*cp++ = (unsigned char)((in >> 18) | 0xF0);
		*cp++ = (unsigned char)cont_byte(in >> 12);
		*cp++ = (unsigned char)cont_byte(in >> 6);
		*cp++ = (unsigned char)cont_byte(in);
		break;
	}
	(*outbytesleft) -= n;
	(*outbuf) += n;
	return 1;
}

static ucs_t
convert_to_ucs(struct iconv_ces *ces,
	const unsigned char **inbuf, apr_size_t *inbytesleft)
{
	const unsigned char *in = *inbuf;
	unsigned char byte = *in++;
	ucs_t res = byte;

	if (byte >= 0xC0) {
		if (byte < 0xE0) {
			if (*inbytesleft < 2)
				return UCS_CHAR_NONE;
			res = (*in & 0xC0) == 0x80 ?
			    ((byte & 0x1F) << 6) | (*in++ & 0x3F) :
			    UCS_CHAR_INVALID;
		} else if (byte < 0xF0) {
			if (*inbytesleft < 3)
				return UCS_CHAR_NONE;
			if (((in[0] & 0xC0) == 0x80) && ((in[1] & 0xC0) == 0x80)) {
				res = ((byte & 0x0F) << 12) | ((in[0] & 0x3F) << 6)
                                            | (in[1] & 0x3F);
				in += 2;
    			} else
				res = UCS_CHAR_INVALID;
		} else if (byte <= 0xF4) {
			if (*inbytesleft < 4)
				return UCS_CHAR_NONE;
			if (((byte == 0xF4 && ((in[0] & 0xF0) == 0x80))
			    || ((in[0] & 0xC0) == 0x80))
				&& ((in[1] & 0xC0) == 0x80)
				&& ((in[2] & 0xC0) == 0x80)) {
				res = ((byte & 0x7) << 18) | ((in[0] & 0x3F) << 12)
				    | ((in[1] & 0x3F) << 6) | (in[2] & 0x3F);
				in += 3;
			} else
				res = UCS_CHAR_INVALID;
		} else
			res = UCS_CHAR_INVALID;
	} else if (byte & 0x80)
		res = UCS_CHAR_INVALID;

	(*inbytesleft) -= (in - *inbuf);
	*inbuf = in;
	return res;
}

static const struct iconv_ces_desc iconv_ces_desc = {
	apr_iconv_ces_open_zero,
	apr_iconv_ces_zero,
	apr_iconv_ces_no_func,
	utf8_names,
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
