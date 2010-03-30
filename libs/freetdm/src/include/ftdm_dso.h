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


#ifndef _FTDM_DSO_H
#define _FTDM_DSO_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ftdm_func_ptr_t) (void);
typedef void * ftdm_dso_lib_t;

FT_DECLARE(void) ftdm_dso_destroy(ftdm_dso_lib_t *lib);
FT_DECLARE(ftdm_dso_lib_t) ftdm_dso_open(const char *path, char **err);
FT_DECLARE(void *) ftdm_dso_func_sym(ftdm_dso_lib_t lib, const char *sym, char **err);

#ifdef __cplusplus
}
#endif

#endif

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

