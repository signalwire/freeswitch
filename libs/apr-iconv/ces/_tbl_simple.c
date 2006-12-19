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

#include "apr.h"

#define ICONV_INTERNAL
#include "iconv.h"

static int
table_open(struct iconv_ces *ces, apr_pool_t *ctx)
{
        struct iconv_module *ccsmod = ces->mod->im_private;
        ces->data = (void *)(ccsmod->im_desc->imd_data);
	return 0;
}

static const char *const *
table_names(struct iconv_ces *ces)
{
	return ((struct iconv_ccs_desc *)(ces->data))->names;
}

static APR_INLINE int
table_nbits(struct iconv_ces *ces)
{
	return ((struct iconv_ccs_desc *)(ces->data))->nbits;
}

static int
ces_nbits(struct iconv_ces *ces)
{
	int res = table_nbits(ces);

	if (res > 8)
		res >>= 1;
	return res;
}

static int
ces_nbytes(struct iconv_ces *ces)
{
	int res = table_nbits(ces);

	return res == 16 ? 0 : (res > 8 ? 2 : 1);
}

static apr_ssize_t
convert_from_ucs(struct iconv_ces *ces, ucs_t in,
	unsigned char **outbuf, apr_size_t *outbytesleft)
{
	struct iconv_ccs_desc *ccsd = ces->data;
	ucs_t res;
	apr_size_t bytes;

	if (in == UCS_CHAR_NONE)
		return 1;	/* No state reinitialization for table charsets */
	if (iconv_char32bit(in))
		return -1;
        /* This cast to ucs2_t silences a MSVC argument conversion warning.
           It's safe because we've just checked that 'in' is a 16-bit
           (or shorter) character. */
	res = ICONV_CCS_CONVERT_FROM_UCS(ccsd, (ucs2_t)in);
	if (res == UCS_CHAR_INVALID)
		return -1;	/* No character in output charset */
	bytes = res & 0xFF00 ? 2 : 1;
	if (*outbytesleft < bytes)
		return 0;	/* No space in output buffer */
	if (bytes == 2)
		*(*outbuf)++ = (res >> 8) & 0xFF;
	*(*outbuf)++ = res & 0xFF;
	*outbytesleft -= bytes;
	return 1;
}

static ucs_t
convert_to_ucs(struct iconv_ces *ces, const unsigned char **inbuf,
	apr_size_t *inbytesleft)
{
	struct iconv_ccs_desc *ccsd = ces->data;
	unsigned char byte = *(*inbuf);
	ucs_t res = ICONV_CCS_CONVERT_TO_UCS(ccsd, byte);
	apr_size_t bytes = (res == UCS_CHAR_INVALID && table_nbits(ces) > 8) ? 2 : 1;

	if (*inbytesleft < bytes)
		return UCS_CHAR_NONE;	/* Not enough bytes in the input buffer */
        /* This cast to ucs2_t silences a MSVC argument conversion warning.
           It's safe because we're creating s 16-bit char from two bytes. */
	if (bytes == 2)
    		res = ICONV_CCS_CONVERT_TO_UCS(ccsd,
		    (ucs2_t)((byte << 8) | (* ++(*inbuf))));
	(*inbuf) ++;
	*inbytesleft -= bytes;
	return res;
}

static apr_status_t
table_load_ccs(struct iconv_module *mod, apr_pool_t *ctx)
{
	struct iconv_module *ccsmod;
	int error;

	if (mod->im_args == NULL)
		return APR_EINVAL;
        if (mod->im_private != NULL)
            return APR_EINVAL;
	error = apr_iconv_mod_load(mod->im_args, ICMOD_UC_CCS, NULL, &ccsmod, ctx);
	if (error)
		return error;
        mod->im_private = ccsmod;
	return APR_SUCCESS;
}

static apr_status_t
table_unload_ccs(struct iconv_module *mod, apr_pool_t *ctx)
{
    struct iconv_module *ccsmod = mod->im_private;
    if (ccsmod == NULL)
        return APR_EINVAL;
    mod->im_private = NULL;
    return apr_iconv_mod_unload(ccsmod, ctx);
}

static apr_status_t
table_event(struct iconv_module *mod, int event, apr_pool_t *ctx)
{
	switch (event) {
	    case ICMODEV_LOAD:
	    case ICMODEV_UNLOAD:
		break;
            case ICMODEV_DYN_LOAD:
		return table_load_ccs(mod,ctx);
            case ICMODEV_DYN_UNLOAD:
                return table_unload_ccs(mod,ctx);
	    default:
		return APR_EINVAL;
	}
	return APR_SUCCESS;
}

static const struct iconv_ces_desc iconv_ces_desc = {
	table_open,
	apr_iconv_ces_zero,
	apr_iconv_ces_no_func,
	table_names,
	ces_nbits,
	ces_nbytes,
	convert_from_ucs,
	convert_to_ucs,
	NULL
};

struct iconv_module_desc iconv_module = {
	ICMOD_UC_CES,
	table_event,
	NULL,
	&iconv_ces_desc
};
