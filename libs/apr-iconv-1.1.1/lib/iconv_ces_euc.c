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
#include <string.h>

#define ICONV_INTERNAL
#include "iconv.h"

#define	CESTOSTATE(ces)		((iconv_ces_euc_state_t *)(ces)->data)
#define	MODTOCCS(mod)		((struct iconv_ccs_desc *)(mod)->im_desc->imd_data)


typedef struct {
	int nccs;
	const struct iconv_module *ccs[1];
} iconv_ces_euc_state_t;

API_DECLARE_NONSTD(apr_status_t)
apr_iconv_euc_open(struct iconv_ces *ces, apr_pool_t *ctx)
{
	struct iconv_module *depmod = ces->mod->im_deplist;
	iconv_ces_euc_state_t *state;
	apr_size_t stsz;
	int i;

	stsz = sizeof(iconv_ces_euc_state_t) +
	    sizeof(struct iconv_module *) * (ces->mod->im_depcnt - 1);
	state = (iconv_ces_euc_state_t *)malloc(stsz);
	if (state == NULL)
		return APR_ENOMEM;
	memset(state, 0, stsz);
	state->nccs = ces->mod->im_depcnt;
	for (i = ces->mod->im_depcnt; i; i--, depmod = depmod->im_next)
		state->ccs[i - 1] = depmod;
	CESTOSTATE(ces) = state;
	return APR_SUCCESS;
}

API_DECLARE_NONSTD(apr_status_t)
apr_iconv_euc_close(struct iconv_ces *ces)
{
	free(CESTOSTATE(ces));
	return APR_SUCCESS;
}

#define is_7_14bit(data) ((data)->nbits & 7)
#define is_7bit(data) ((data)->nbits & 1)

API_DECLARE_NONSTD(apr_ssize_t)
apr_iconv_euc_convert_from_ucs(struct iconv_ces *ces, ucs_t in,
	unsigned char **outbuf, apr_size_t *outbytesleft)
{
	iconv_ces_euc_state_t *euc_state = CESTOSTATE(ces);
	const iconv_ces_euc_ccs_t *ccsattr;
	const struct iconv_ccs_desc *ccs;
	ucs_t res;
	apr_size_t bytes;
	int i;

	if (in == UCS_CHAR_NONE)
		return 1;	/* No state reinitialization for table charsets */
	if (iconv_char32bit(in))
		return -1;

	for (i = 0; i < euc_state->nccs; i++) {
		ccs = MODTOCCS(euc_state->ccs[i]);
		res = ICONV_CCS_CONVERT_FROM_UCS(ccs, in);
		if (res == UCS_CHAR_INVALID)
			continue;
		ccsattr = euc_state->ccs[i]->im_depdata;
		if (i) {
			if (is_7_14bit(ccs))
				res |= is_7bit(ccs) ? 0x80 : 0x8080;
			else if (!(res & 0x8080))
				continue;
		} else if (res & 0x8080)
			continue;
		bytes = (res & 0xFF00 ? 2 : 1) + ccsattr->prefixlen;
		if (*outbytesleft < bytes)
			return 0;	/* No space in the output buffer */
		if (ccsattr->prefixlen) {
			memcpy(*outbuf, ccsattr->prefix, ccsattr->prefixlen);
			(*outbuf) += ccsattr->prefixlen;
		}
		if (res & 0xFF00)
			*(*outbuf)++ = (unsigned char)(res >> 8);
		*(*outbuf)++ = (unsigned char)res;
		*outbytesleft -= bytes;
		return 1;
	}
	return -1;	/* No character in output charset */
}

static ucs_t
cvt2ucs(const struct iconv_ccs_desc *ccs, const unsigned char *inbuf,
	apr_size_t inbytesleft, int hi_plane, const unsigned char **bufptr)
{
	apr_size_t bytes = ccs->nbits > 8 ? 2 : 1;
	ucs_t ch = *(const unsigned char *)inbuf++;

	if (inbytesleft < bytes)
		return UCS_CHAR_NONE;	/* Not enough bytes in the input buffer */
	if (bytes == 2)
		ch = (ch << 8) | *(const unsigned char *)inbuf++;
	*bufptr = inbuf;
	if (hi_plane) {
		if (!(ch & 0x8080))
			return UCS_CHAR_INVALID;
		if (is_7_14bit(ccs))
			ch &= 0x7F7F;
	} else if (ch & 0x8080)
		return UCS_CHAR_INVALID;
	return ICONV_CCS_CONVERT_TO_UCS(ccs, ch);
}

API_DECLARE_NONSTD(ucs_t)
apr_iconv_euc_convert_to_ucs(struct iconv_ces *ces,
	const unsigned char **inbuf, apr_size_t *inbytesleft)
{
	iconv_ces_euc_state_t *euc_state = CESTOSTATE(ces);
	const iconv_ces_euc_ccs_t *ccsattr;
	const struct iconv_module *ccsmod;
	ucs_t res = UCS_CHAR_INVALID;
	const unsigned char *ptr;
        int i;

	if (**inbuf & 0x80) {
		for (i = 1; i < euc_state->nccs; i++) {
			ccsmod = euc_state->ccs[i];
			ccsattr = ccsmod->im_depdata;
			if (ccsattr->prefixlen + 1 > *inbytesleft)
				return UCS_CHAR_NONE;
			if (ccsattr->prefixlen &&
			    memcmp(*inbuf, ccsattr->prefix, ccsattr->prefixlen) != 0)
				continue;
			res = cvt2ucs(MODTOCCS(ccsmod), *inbuf + ccsattr->prefixlen,
			    *inbytesleft - ccsattr->prefixlen, 1, &ptr);
			if (res != UCS_CHAR_INVALID)
				break;
		}
		if (res == UCS_CHAR_INVALID)
			ptr = *inbuf + 1;
	} else
		res = cvt2ucs(MODTOCCS(euc_state->ccs[0]), *inbuf, *inbytesleft, 0, &ptr);
	if (res == UCS_CHAR_NONE)
		return res;	/* Not enough bytes in the input buffer */
	*inbytesleft -= ptr - *inbuf;
	*inbuf = ptr;
	return res;
}
