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
#include <string.h>	/* memset, memcmp, memcpy */

#define ICONV_INTERNAL
#include "iconv.h"

typedef struct {
	const char	*sequence;
	apr_size_t	length;
	int		prefix_type;
} iconv_ces_iso2022_shift;

enum { ICONV_PREFIX_STATE = 0, ICONV_PREFIX_LINE, ICONV_PREFIX_CHAR };

static const iconv_ces_iso2022_shift iso_shift[] = {
    { "\x0f",  1, ICONV_PREFIX_STATE },
    { "\x0e",  1, ICONV_PREFIX_LINE },
    { "\x1bN", 2, ICONV_PREFIX_CHAR },
    { "\x1bO", 2, ICONV_PREFIX_CHAR }
};

#define shift_num (sizeof(iso_shift) / sizeof(iconv_ces_iso2022_shift))
#define SHIFT_LEN (sizeof(int) * shift_num)
#define	CESTOSTATE(ces)		((iconv_ces_iso2022_state_t *)ces->data)
#define	MODTOCCS(mod)		((struct iconv_ccs_desc *)(mod)->im_desc->imd_data)

typedef struct iconv_ces_iso2022_state {
	int	*shift_tab;
	int	shift_index;
	ucs_t	previous_char;
	char	prefix[128];
	int	nccs;
	const int *org_shift_tab;
	const struct iconv_module *ccsmod[1];
} iconv_ces_iso2022_state_t;

API_DECLARE_NONSTD(apr_status_t)
apr_iconv_iso2022_open(struct iconv_ces *ces, apr_pool_t *ctx)
{
	const iconv_ces_iso2022_ccs_t *ccsattr;
	const struct iconv_ccs_desc *ccs;
	iconv_ces_iso2022_state_t *state;
	struct iconv_module *depmod;
	apr_size_t stsz, shiftsz;
	int i;

	shiftsz = SHIFT_LEN;
	stsz = sizeof(iconv_ces_iso2022_state_t) +
	    sizeof(struct iconv_module *) * (ces->mod->im_depcnt - 1);
	state = (iconv_ces_iso2022_state_t *)malloc(stsz + shiftsz);
	if (state == NULL)
		return APR_ENOMEM;
	memset(state, 0, stsz + shiftsz);
	ces->data = state;
	state->shift_tab = (int*)((char*)state + stsz);
	state->org_shift_tab = ces->desc->data;
	apr_iconv_iso2022_reset(ces);
	state->nccs = ces->mod->im_depcnt;
	depmod = ces->mod->im_deplist;
	for (i = ces->mod->im_depcnt; i; i--, depmod = depmod->im_next) {
		state->ccsmod[i - 1] = depmod;
		ccs = MODTOCCS(depmod);
		ccsattr = depmod->im_depdata;
		if (ccsattr->designatorlen)
			state->prefix[(int)ccsattr->designator[0]] = 1;
		if (ccsattr->shift >= 0)
			state->prefix[(int)(iso_shift[ccsattr->shift].sequence[0])] = 1;
	}
	return APR_SUCCESS;
}

API_DECLARE_NONSTD(int)
apr_iconv_iso2022_close(struct iconv_ces *ces)
{
	free(ces->data);
	return 0;
}

API_DECLARE_NONSTD(void)
apr_iconv_iso2022_reset(struct iconv_ces *ces)
{
	struct iconv_ces_iso2022_state *state = CESTOSTATE(ces);

	memcpy(state->shift_tab, ces->desc->data, SHIFT_LEN);
	state->shift_index = 0;
	state->previous_char = UCS_CHAR_NONE;
}

static void
update_shift_state(const struct iconv_ces *ces, ucs_t ch)
{
	struct iconv_ces_iso2022_state *iso_state = CESTOSTATE(ces);
	apr_size_t i;

	if (ch == '\n' && iso_state->previous_char == '\r') {
		for (i = 0; i < shift_num; i ++) {
			if (iso_shift[i].prefix_type != ICONV_PREFIX_STATE)
				iso_state->shift_tab[i] = iso_state->org_shift_tab[i];
		}
        }
	iso_state->previous_char = ch;
}

#define is_7_14bit(ccs) ((ccs)->nbits & 7)

static apr_ssize_t
cvt_ucs2iso(const struct iconv_ces *ces, ucs_t in,
	unsigned char **outbuf, apr_size_t *outbytesleft, int cs)
{
	struct iconv_ces_iso2022_state *iso_state = CESTOSTATE(ces);
	const struct iconv_ces_iso2022_ccs *ccsattr;
	const struct iconv_ccs_desc *ccs;
	ucs_t res;
	apr_size_t len = 0;
	int need_designator, need_shift;

	ccs = MODTOCCS(iso_state->ccsmod[cs]);
	res = (in == UCS_CHAR_NONE) ?
	    in : ICONV_CCS_CONVERT_FROM_UCS(ccs, in);
	if (in != UCS_CHAR_NONE) {
		if (iso_shift[cs].prefix_type == ICONV_PREFIX_CHAR &&
		    !is_7_14bit(ccs)) {
			if ((res & 0x8080) == 0)
				return -1;
		    res &= 0x7F7F;
		} else if (res & 0x8080)
			return -1; /* Invalid/missing character in the output charset */
	}
	ccsattr = iso_state->ccsmod[cs]->im_depdata;
	if ((need_shift = (ccsattr->shift != iso_state->shift_index)))
		len += iso_shift[ccsattr->shift].length;
	if ((need_designator = (cs != iso_state->shift_tab[ccsattr->shift])))
		len += ccsattr->designatorlen;
	if (in != UCS_CHAR_NONE)
		len += res & 0xFF00 ? 2 : 1;
	if (len > *outbytesleft)
		return 0;	/* No space in output buffer */
	if (need_designator && (len = ccsattr->designatorlen)) {
		memcpy(*outbuf, ccsattr->designator, len);
		(*outbuf) += len;
		(*outbytesleft) -= len;
		iso_state->shift_tab[ccsattr->shift] = cs;
	}
	if (need_shift && (len = iso_shift[ccsattr->shift].length)) {
		memcpy(*outbuf, iso_shift[ccsattr->shift].sequence, len);
		(*outbuf) += len;
		(*outbytesleft) -= len;
		if (iso_shift[ccsattr->shift].prefix_type != ICONV_PREFIX_CHAR)
			iso_state->shift_index = ccsattr->shift;
	}
	if (in == UCS_CHAR_NONE)
		return 1;
	if (res & 0xFF00) {
		*(unsigned char *)(*outbuf) ++ = res >> 8;
		(*outbytesleft)--;
	}
	*(unsigned char *)(*outbuf) ++ = res;
	(*outbytesleft) --;
	update_shift_state(ces, res);
	return 1;
}

API_DECLARE_NONSTD(apr_ssize_t)
apr_iconv_iso2022_convert_from_ucs(struct iconv_ces *ces,
	ucs_t in, unsigned char **outbuf, apr_size_t *outbytesleft)
{
	struct iconv_ces_iso2022_state *iso_state = CESTOSTATE(ces);
	apr_ssize_t res;
	int cs, i;

	if (in == UCS_CHAR_NONE)
		return cvt_ucs2iso(ces, in, outbuf, outbytesleft, 0);
	if (iconv_char32bit(in))
		return -1;
	cs = iso_state->shift_tab[iso_state->shift_index];
	if ((res = cvt_ucs2iso(ces, in, outbuf, outbytesleft, cs)) >= 0)
		return res;
	for (i = 0; i < iso_state->nccs; i++) {
		if (i == cs)
			continue;
		if ((res = cvt_ucs2iso(ces, in, outbuf, outbytesleft, i)) >= 0)
			return res;
	}
	(*outbuf) ++;
	(*outbytesleft) --;
	return -1;	/* No character in output charset */
}

static ucs_t
cvt_iso2ucs(const struct iconv_ccs_desc *ccs, const unsigned char **inbuf,
	apr_size_t *inbytesleft, int prefix_type)
{
	apr_size_t bytes = ccs->nbits > 8 ? 2 : 1;
	ucs_t ch = **inbuf;

	if (*inbytesleft < bytes)
		return UCS_CHAR_NONE;	/* Not enough bytes in the input buffer */
	if (bytes == 2)
		ch = (ch << 8) | *(++(*inbuf));
	(*inbuf)++;
	(*inbytesleft) -= bytes;
	if (ch & 0x8080)
		return UCS_CHAR_INVALID;
	if (prefix_type == ICONV_PREFIX_CHAR && !is_7_14bit(ccs))
		ch |= (bytes == 2) ? 0x8080 : 0x80;
	return ICONV_CCS_CONVERT_TO_UCS(ccs, ch);
}

API_DECLARE_NONSTD(ucs_t)
apr_iconv_iso2022_convert_to_ucs(struct iconv_ces *ces,
	const unsigned char **inbuf, apr_size_t *inbytesleft)
{
	struct iconv_ces_iso2022_state *iso_state = CESTOSTATE(ces);
	const struct iconv_ces_iso2022_ccs *ccsattr;
	ucs_t res;
	const unsigned char *ptr = *inbuf;
	unsigned char byte;
	apr_size_t len, left = *inbytesleft;
	int i;

	while (left) {
		byte = *ptr;
		if (byte & 0x80) {
			(*inbuf)++;
			(*inbytesleft) --;
			return UCS_CHAR_INVALID;
		}
		if (!iso_state->prefix[byte])
			break;
		for (i = 0; i < iso_state->nccs; i++) {
			ccsattr = iso_state->ccsmod[i]->im_depdata;
			len = ccsattr->designatorlen;
			if (len) {
				if (len + 1 > left)
					return UCS_CHAR_NONE;
				if (memcmp(ptr, ccsattr->designator, len) == 0) {
					iso_state->shift_tab[ccsattr->shift] = i;
					ptr += len;
					left -= len;
					break;
				}
			}
			len = iso_shift[ccsattr->shift].length;
			if (len) {
				if (len + 1 > left)
					return UCS_CHAR_NONE;
				if (memcmp(ptr,
				    iso_shift[ccsattr->shift].sequence, len) == 0) {
					if (iso_shift[ccsattr->shift].prefix_type != ICONV_PREFIX_CHAR)
						iso_state->shift_index = ccsattr->shift;
					ptr += len;
					left -= len;
					break;
				}
			}
		}
	}
	i = iso_state->shift_tab[iso_state->shift_index];
	if (i < 0) {
		(*inbuf) ++;
		(*inbytesleft) --;
		return UCS_CHAR_INVALID;
	}
	res = cvt_iso2ucs(MODTOCCS(iso_state->ccsmod[i]), &ptr, &left, iso_shift[i].prefix_type);
	if (res != UCS_CHAR_NONE) {
		*inbuf = (const char*)ptr;
		*inbytesleft = left;
		update_shift_state(ces, res);
	}
	return res;
}
