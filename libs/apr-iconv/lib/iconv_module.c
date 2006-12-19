/*-
 * Copyright (c) 2000, Boris Popov.  All rights reserved.
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
 *	This product includes software developed by Boris Popov
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
 */

#define ICONV_INTERNAL

#include "iconv.h"
#include "apr_file_io.h"
#include "apr_file_info.h"
#include "apr_pools.h"
#include "apr_dso.h"
#include "apr_env.h"
#include "apr_strings.h"
#include "apr_tables.h"
#include "apr_lib.h"
#include "api_version.h"

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef API_USE_BUILTIN_ALIASES
#include "charset_alias.h"
#endif

#define APR_ICONV_PATH "APR_ICONV" API_STRINGIFY(API_MAJOR_VERSION) "_PATH"

static apr_status_t
iconv_getpathname(char *buffer, const char *dir, const char *name, apr_pool_t *ctx)
{
        apr_status_t rv;
	apr_finfo_t sb;

	apr_snprintf(buffer, APR_PATH_MAX, "%s/%s.so", dir, name);
        rv = apr_stat(&sb, buffer, APR_FINFO_TYPE, ctx);
#ifdef API_HAVE_CHARSET_ALIAS_TABLE
        /* If we didn't find the file, try again after looking in
           the charset alias mapping table. */
        if (rv || sb.filetype != APR_REG) {
            const char *alias = charset_alias_find(name);
            if (alias) {
                apr_snprintf(buffer, APR_PATH_MAX, "%s/%s.so", dir, alias);
                rv = apr_stat(&sb, buffer, APR_FINFO_TYPE, ctx);
            }
        }
#endif /* API_HAVE_CHARSET_ALIAS_TABLE */
        if (!rv && sb.filetype != APR_REG)
            rv = APR_EINVAL;

        return rv;
}

static apr_status_t
iconv_getpath(char *buf, const char *name, apr_pool_t *ctx)
{
	char buffer[APR_PATH_MAX];
        apr_array_header_t *pathelts;
        apr_pool_t *subpool;
        apr_status_t status;
	char *ptr;

        status = apr_pool_create(&subpool, ctx);
        if (status)
            return status;

	if (apr_tolower(name[0]) == 'x' && name[1] == '-')
		name += 2;
        ptr = buffer;
        while (0 != (*ptr++ = apr_tolower(*name++)))
            ;

        /* Fall back on APR_ICONV_PATH if APR_ICONVn_PATH isn't set...
         * TODO: Drop support for "APR_ICONV_PATH" in apr-iconv 2.0
         */
        if ((!apr_env_get(&ptr, APR_ICONV_PATH, subpool)
             || (!apr_env_get(&ptr, "APR_ICONV_PATH", subpool)))
            && !apr_filepath_list_split(&pathelts, ptr, subpool))
        {
            int i;
            char **elts = (char **)pathelts->elts;
            for (i = 0; i < pathelts->nelts; ++i)
            {
                if (iconv_getpathname(buf, elts[i], buffer, subpool) == 0)
                {
                    apr_pool_destroy(subpool);
                    return APR_SUCCESS;
                }
            }
        }

        status = iconv_getpathname(buf, ICONV_DEFAULT_PATH, buffer, subpool);
        apr_pool_destroy(subpool);
        return status;
}

static int
iconv_dlopen(const char *name, const char *symbol, void **hpp, void **dpp, apr_pool_t *ctx)
{
	apr_dso_handle_t *handle;
	void *data;

	/* dlopen */
	if (apr_dso_load(&handle, name, ctx) != APR_SUCCESS) {
		return EINVAL;
	}
	/* dlsym */
	if ( apr_dso_sym(&data, handle, symbol) == APR_SUCCESS) {
		*hpp = handle;
		*dpp = data;
		return 0;
	}
	apr_dso_unload(handle);
	return EINVAL;
}

API_DECLARE_NONSTD(int)
apr_iconv_mod_load(const char *modname, int modtype, const void *args,
	struct iconv_module **modpp, apr_pool_t *ctx)
{
	struct iconv_module_desc *mdesc;
	struct iconv_module *mod, *depmod;
	const struct iconv_module_depend *depend;
	char buffer[APR_PATH_MAX];
	void *handle;
	int error;

	if (iconv_getpath(buffer, modname, ctx) != 0)
		return EINVAL;

	error = iconv_dlopen(buffer, "iconv_module", &handle, (void**)&mdesc, ctx);
	if (error)
		return error;
	if (modtype != ICMOD_ANY && mdesc->imd_type != modtype) {
		apr_dso_unload(handle);
		return APR_EFTYPE;
	}
	mod = malloc(sizeof(*mod));
	if (mod == NULL) {
		apr_dso_unload(handle);
		return ENOMEM;
	}
	memset(mod, 0, sizeof(*mod));
	mod->im_handle = handle;
	mod->im_desc = mdesc;
	mod->im_args = args;
	depend = mdesc->imd_depend;
	if (depend) {
		while (depend->md_name) {
			error = apr_iconv_mod_load(depend->md_name, 
			    depend->md_type, NULL, &depmod, ctx);
			if (error)
				goto bad;
			depmod->im_depdata = depend->md_data;
			depmod->im_next = mod->im_deplist;
			mod->im_deplist = depmod;
			depend++;
		}
	}
	error = ICONV_MOD_DYN_LOAD(mod,ctx);
	if (error)
		goto bad;
	depmod = mod->im_deplist;
	while (depmod) {
		mod->im_depcnt++;
		depmod = depmod->im_next;
	}
	error = ICONV_MOD_LOAD(mod,ctx);
	if (error)
		goto bad;
	mod->im_flags |= ICMODF_LOADED;
	*modpp = mod;
	return 0;
bad:
	apr_iconv_mod_unload(mod,ctx);
	return error;
}

API_DECLARE_NONSTD(int)
apr_iconv_mod_unload(struct iconv_module *mod, apr_pool_t *ctx)
{
	struct iconv_module *deplist, *tmp;
	int error = 0;

	if (mod == NULL)
		return -1;
	if (mod->im_flags & ICMODF_LOADED)
		error = ICONV_MOD_UNLOAD(mod,ctx);
	error = ICONV_MOD_DYN_UNLOAD(mod,ctx);
	deplist = mod->im_deplist;
	while (deplist) {
		tmp = deplist->im_next;
		apr_iconv_mod_unload(deplist,ctx);
		deplist = tmp;
	}
	if (mod->im_handle != NULL)
		if (apr_dso_unload(mod->im_handle) != APR_SUCCESS)
			error = APR_EINVAL;
	free(mod);
	return error;
}

API_DECLARE_NONSTD(int)
apr_iconv_mod_noevent(struct iconv_module *mod, int event, apr_pool_t *ctx)
{
	switch (event) {
	    case ICMODEV_LOAD:
	    case ICMODEV_UNLOAD:
	    case ICMODEV_DYN_LOAD:
	    case ICMODEV_DYN_UNLOAD:
		break;
	    default:
		return APR_EINVAL;
	}
	return 0;
}
