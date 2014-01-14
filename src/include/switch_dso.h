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


#ifndef FREESWITCH_DSO_H
#define FREESWITCH_DSO_H

SWITCH_BEGIN_EXTERN_C

typedef int (*switch_dso_func_t) (void);
#ifdef WIN32
typedef HINSTANCE switch_dso_lib_t;
#else
typedef void *switch_dso_lib_t;
#endif

typedef void *switch_dso_data_t;

SWITCH_DECLARE(void) switch_dso_destroy(switch_dso_lib_t *lib);
SWITCH_DECLARE(switch_dso_lib_t) switch_dso_open(const char *path, int global, char **err);
SWITCH_DECLARE(switch_dso_func_t) switch_dso_func_sym(switch_dso_lib_t lib, const char *sym, char **err);
SWITCH_DECLARE(void *) switch_dso_data_sym(switch_dso_lib_t lib, const char *sym, char **err);

SWITCH_END_EXTERN_C

#endif

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
