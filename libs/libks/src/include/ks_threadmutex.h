/* 
 * Cross Platform Thread/Mutex abstraction
 * Copyright(C) 2007 Michael Jerris
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


#ifndef _KS_THREADMUTEX_H
#define _KS_THREADMUTEX_H

#include "ks.h"

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

typedef struct ks_mutex ks_mutex_t;
typedef struct ks_thread ks_thread_t;
typedef void *(*ks_thread_function_t) (ks_thread_t *, void *);

KS_DECLARE(ks_status_t) ks_thread_create_detached(ks_thread_function_t func, void *data);
ks_status_t ks_thread_create_detached_ex(ks_thread_function_t func, void *data, size_t stack_size);
void ks_thread_override_default_stacksize(size_t size);
KS_DECLARE(ks_status_t) ks_mutex_create(ks_mutex_t **mutex);
KS_DECLARE(ks_status_t) ks_mutex_destroy(ks_mutex_t **mutex);
KS_DECLARE(ks_status_t) ks_mutex_lock(ks_mutex_t *mutex);
KS_DECLARE(ks_status_t) ks_mutex_trylock(ks_mutex_t *mutex);
KS_DECLARE(ks_status_t) ks_mutex_unlock(ks_mutex_t *mutex);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* defined(_KS_THREADMUTEX_H) */

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
