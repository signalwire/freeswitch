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

#include "ks.h"

#ifndef _KS_DSO_H
#define _KS_DSO_H

KS_BEGIN_EXTERN_C

typedef void (*ks_func_ptr_t) (void);
typedef void * ks_dso_lib_t;

KS_DECLARE(ks_status_t) ks_dso_destroy(ks_dso_lib_t *lib);
KS_DECLARE(ks_dso_lib_t) ks_dso_open(const char *path, char **err);
KS_DECLARE(void *) ks_dso_func_sym(ks_dso_lib_t lib, const char *sym, char **err);
KS_DECLARE(char *) ks_build_dso_path(const char *name, char *path, ks_size_t len);

KS_END_EXTERN_C

#endif

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */

