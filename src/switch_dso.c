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

#include <switch.h>
#include "switch_dso.h"
#include <stdlib.h>
#include <string.h>

#ifdef WIN32

SWITCH_DECLARE(void) switch_dso_destroy(switch_dso_lib_t *lib)
{
	if (lib && *lib) {
		FreeLibrary(*lib);
		*lib = NULL;
	}
}

SWITCH_DECLARE(switch_dso_lib_t) switch_dso_open(const char *path, int global, char **err)
{
	HINSTANCE lib;

	lib = LoadLibraryEx(path, NULL, 0);

	if (!lib) {
		LoadLibraryEx(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
	}

	if (!lib) {
		DWORD error = GetLastError();
		*err = switch_mprintf("dll open error [%ul]\n", error);
	}

	return lib;
}

SWITCH_DECLARE(switch_dso_func_t) switch_dso_func_sym(switch_dso_lib_t lib, const char *sym, char **err)
{
	FARPROC func = GetProcAddress(lib, sym);
	if (!func) {
		DWORD error = GetLastError();
		*err = switch_mprintf("dll sym error [%ul]\n", error);
	}
	return (switch_dso_func_t) func;
}

SWITCH_DECLARE(void *) switch_dso_data_sym(switch_dso_lib_t lib, const char *sym, char **err)
{
	FARPROC addr = GetProcAddress(lib, sym);
	if (!addr) {
		DWORD error = GetLastError();
		*err = switch_mprintf("dll sym error [%ul]\n", error);
	}
	return (void *) (intptr_t) addr;
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

void switch_dso_destroy(switch_dso_lib_t *lib)
{
	if (lib && *lib) {
		dlclose(*lib);
		*lib = NULL;
	}
}

switch_dso_lib_t switch_dso_open(const char *path, int global, char **err)
{
	void *lib;

	if (global) {
		lib = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
	} else {
		lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	}

	if (lib == NULL) {
		const char *dlerr = dlerror();
		/* Work around broken uclibc returning NULL on both dlopen() and dlerror() */
		if (dlerr) {
			*err = strdup(dlerr);
		} else {
			*err = strdup("Unknown error");
		}
	}
	return lib;
}

switch_dso_func_t switch_dso_func_sym(switch_dso_lib_t lib, const char *sym, char **err)
{
	void *func = dlsym(lib, sym);
	if (!func) {
		*err = strdup(dlerror());
	}
	return (switch_dso_func_t) (intptr_t) func;
}

void *switch_dso_data_sym(switch_dso_lib_t lib, const char *sym, char **err)
{
	void *addr = dlsym(lib, sym);
	if (!addr) {
		*err = strdup(dlerror());
	}
	return addr;
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
