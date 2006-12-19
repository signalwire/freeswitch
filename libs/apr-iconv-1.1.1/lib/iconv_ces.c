/*-
 * Copyright (c) 1999, 2000
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

#include <limits.h>	/* PATH_MAX */
#include <stdlib.h>	/* free, malloc */
#include <string.h>

#define ICONV_INTERNAL
#include "iconv.h"	/* iconv_ccs_desc, iconv_ccs */

API_DECLARE_NONSTD(int)
apr_iconv_ces_open(const char *cesname, struct iconv_ces **cespp, apr_pool_t *ctx)
{
	struct iconv_module *mod;
	struct iconv_ces *ces;
	apr_status_t error;

	error = apr_iconv_mod_load(cesname, ICMOD_UC_CES, NULL, &mod, ctx);
	if (APR_STATUS_IS_EFTYPE(error))
		error = apr_iconv_mod_load("_tbl_simple", ICMOD_UC_CES, cesname, &mod, ctx);
	if (error != APR_SUCCESS)
		return (APR_STATUS_IS_EFTYPE(error)) ? APR_EINVAL : error;
	ces = malloc(sizeof(*ces));
	if (ces == NULL) {
		apr_iconv_mod_unload(mod, ctx);
		return APR_ENOMEM;
	}
	memset(ces,0, sizeof(*ces));
	ces->desc = (struct iconv_ces_desc*)mod->im_desc->imd_data;
	ces->data = mod->im_data;
	ces->mod = mod;
	error = ICONV_CES_OPEN(ces,ctx);
	if (error != APR_SUCCESS) {
		free(ces);
		apr_iconv_mod_unload(mod, ctx);
		return error;
	}
	*cespp = ces;
	return APR_SUCCESS;
}

API_DECLARE_NONSTD(int)
apr_iconv_ces_close(struct iconv_ces *ces, apr_pool_t *ctx)
{
	int res;

	if (ces == NULL)
		return -1;
	res = ICONV_CES_CLOSE(ces);
	if (ces->mod != NULL)
		apr_iconv_mod_unload(ces->mod, ctx);
	free(ces);
	return res;
}

API_DECLARE_NONSTD(int)
apr_iconv_ces_open_func(struct iconv_ces *ces, apr_pool_t *ctx)
{
	return iconv_malloc(sizeof(int), &ces->data);
}

API_DECLARE_NONSTD(int)
apr_iconv_ces_close_func(struct iconv_ces *ces)
{
	free(ces->data);
	return 0;
}

API_DECLARE_NONSTD(void)
apr_iconv_ces_reset_func(struct iconv_ces *ces)
{
	memset(ces->data, 0, sizeof(int));
}

/*ARGSUSED*/
API_DECLARE_NONSTD(void)
apr_iconv_ces_no_func(struct iconv_ces *ces)
{
}

API_DECLARE_NONSTD(int)
apr_iconv_ces_nbits7(struct iconv_ces *ces)
{
	return 7;
}

API_DECLARE_NONSTD(int)
apr_iconv_ces_nbits8(struct iconv_ces *ces)
{
	return 8;
}

API_DECLARE_NONSTD(int)
apr_iconv_ces_zero(struct iconv_ces *ces)
{
	return 0;
}
