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


#ifndef _SWITCH_DSO_H
#define _SWITCH_DSO_H

typedef void (*switch_func_ptr_t) (void);
#ifdef WIN32
typedef HINSTANCE switch_dso_lib_t;
#else
typedef void * switch_dso_lib_t;
#endif
#ifdef WIN32
typedef FARPROC switch_dso_func_t;
#else
typedef void * switch_dso_func_t;
#endif

void switch_dso_destroy(switch_dso_lib_t *lib);
switch_dso_lib_t switch_dso_open(const char *path, int global, char **err);
switch_dso_func_t switch_dso_func_sym(switch_dso_lib_t lib, const char *sym, char **err);


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

