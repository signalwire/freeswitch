/* 
 * Cross Platform dso/dll load abstraction
 * Copyright(C) 2008 Michael Jerris
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so.
 *
 * This work is provided under this license on an "as is" basis, without warranty of any kind,
 * either expressed or implied, including, without limitation, warranties that the covered code
 * is free of defects, merchantable, fit for a particular purpose or non-infringing. The entire
 * risk as to the quality and performance of the covered code is with you. Should any covered
 * code prove defective in any respect, you (not the initial developer or any other contributor)
 * assume the cost of any necessary servicing, repair or correction. This disclaimer of warranty
 * constitutes an essential part of this license. No use of any covered code is authorized hereunder
 * except under this disclaimer. 
 *
 */

#include "freetdm.h"
#include "ftdm_dso.h"
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>
#include <stdio.h>


FT_DECLARE(void) ftdm_dso_destroy(ftdm_dso_lib_t *lib) {
	if (lib && *lib) {
		FreeLibrary(*lib);
		*lib = NULL;
	}
}

FT_DECLARE(ftdm_dso_lib_t) ftdm_dso_open(const char *path, char **err) {
    HINSTANCE lib;
	
	lib = LoadLibraryEx(path, NULL, 0);

	if (!lib) {
		LoadLibraryEx(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
	}

	if (!lib) {
		DWORD error = GetLastError();
		char tmp[80];
		sprintf(tmp, "dll open error [%ul]\n", error);
		*err = ftdm_strdup(tmp);
	}

	return lib;
}

FT_DECLARE(void*) ftdm_dso_func_sym(ftdm_dso_lib_t lib, const char *sym, char **err) {
	FARPROC func = GetProcAddress(lib, sym);
	if (!func) {
		DWORD error = GetLastError();
		char tmp[80];
		sprintf(tmp, "dll sym error [%ul]\n", error);
		*err = ftdm_strdup(tmp);
	}
	return (void *)(intptr_t)func; // this should really be addr - ftdm_dso_func_data
}

#else

/*
** {========================================================================
** This is an implementation of loadlib based on the dlfcn interface.
** The dlfcn interface is available in Linux, SunOS, Solaris, IRIX, FreeBSD,
** NetBSD, AIX 4.2, HPUX 11, and  probably most other Unix flavors, at least
** as an emulation layer on top of native functions.
** =========================================================================
*/


#include <dlfcn.h>

FT_DECLARE(void) ftdm_dso_destroy(ftdm_dso_lib_t *lib) {
	if (lib && *lib) {
		dlclose(*lib);
		*lib = NULL;
	}
}

FT_DECLARE(ftdm_dso_lib_t) ftdm_dso_open(const char *path, char **err) {
	void *lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (lib == NULL) {
		*err = ftdm_strdup(dlerror());
	}
	return lib;
}

FT_DECLARE(void*) ftdm_dso_func_sym(ftdm_dso_lib_t lib, const char *sym, char **err) {
	void *func = dlsym(lib, sym);
	if (!func) {
		*err = ftdm_strdup(dlerror());
	}
	return func;
}
#endif

/* }====================================================== */

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4
 */
