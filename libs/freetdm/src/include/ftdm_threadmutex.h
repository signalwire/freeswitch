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
 * Contributors: 
 *
 * Moises Silva <moy@sangoma.com>
 *
 */


#ifndef _FTDM_THREADMUTEX_H
#define _FTDM_THREADMUTEX_H

#include "freetdm.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct ftdm_mutex ftdm_mutex_t;
typedef struct ftdm_thread ftdm_thread_t;
typedef struct ftdm_interrupt ftdm_interrupt_t;
typedef void *(*ftdm_thread_function_t) (ftdm_thread_t *, void *);

FT_DECLARE(ftdm_status_t) ftdm_thread_create_detached(ftdm_thread_function_t func, void *data);
FT_DECLARE(ftdm_status_t) ftdm_thread_create_detached_ex(ftdm_thread_function_t func, void *data, ftdm_size_t stack_size);
FT_DECLARE(void) ftdm_thread_override_default_stacksize(ftdm_size_t size);
FT_DECLARE(ftdm_status_t) ftdm_mutex_create(ftdm_mutex_t **mutex);
FT_DECLARE(ftdm_status_t) ftdm_mutex_destroy(ftdm_mutex_t **mutex);
FT_DECLARE(ftdm_status_t) _ftdm_mutex_lock(ftdm_mutex_t *mutex);
FT_DECLARE(ftdm_status_t) _ftdm_mutex_trylock(ftdm_mutex_t *mutex);
FT_DECLARE(ftdm_status_t) _ftdm_mutex_unlock(ftdm_mutex_t *mutex);
FT_DECLARE(ftdm_status_t) ftdm_interrupt_create(ftdm_interrupt_t **cond, ftdm_socket_t device);
FT_DECLARE(ftdm_status_t) ftdm_interrupt_destroy(ftdm_interrupt_t **cond);
FT_DECLARE(ftdm_status_t) ftdm_interrupt_signal(ftdm_interrupt_t *cond);
FT_DECLARE(ftdm_status_t) ftdm_interrupt_wait(ftdm_interrupt_t *cond, int ms);
FT_DECLARE(ftdm_status_t) ftdm_interrupt_multiple_wait(ftdm_interrupt_t *interrupts[], ftdm_size_t size, int ms);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

