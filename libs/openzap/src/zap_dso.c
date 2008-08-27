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

#include "zap_dso.h"
#include <stdlib.h>
#include <string.h>

/*
** {========================================================================
** This is an implementation of loadlib based on the dlfcn interface.
** The dlfcn interface is available in Linux, SunOS, Solaris, IRIX, FreeBSD,
** NetBSD, AIX 4.2, HPUX 11, and  probably most other Unix flavors, at least
** as an emulation layer on top of native functions.
** =========================================================================
*/


#include <dlfcn.h>

void zap_dso_destroy(zap_dso_lib_t *lib) {
	if (lib) {
		dlclose(lib);
		lib = NULL;
	}
}

zap_dso_lib_t zap_dso_open(const char *path, const char **err) {
	void *lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (lib == NULL) {
		*err = strdup(dlerror());
	}
	return lib;
}

zap_func_ptr_t zap_dso_func_sym(zap_dso_lib_t lib, const char *sym, const char **err) {
	zap_dso_lib_t func = (zap_dso_lib_t)dlsym(lib, sym);
	if (!func) {
		*err = strdup(dlerror());
	}
	return func;
}

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
