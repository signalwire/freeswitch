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


#ifndef _ZAP_THREADMUTEX_H
#define _ZAP_THREADMUTEX_H

#include "openzap.h"

typedef struct zap_mutex zap_mutex_t;
typedef struct zap_thread zap_thread_t;
typedef void *(*zap_thread_function_t) (zap_thread_t *, void *);

OZ_DECLARE(zap_status_t) zap_thread_create_detached(zap_thread_function_t func, void *data);
OZ_DECLARE(zap_status_t) zap_thread_create_detached_ex(zap_thread_function_t func, void *data, zap_size_t stack_size);
OZ_DECLARE(void) zap_thread_override_default_stacksize(zap_size_t size);
OZ_DECLARE(zap_status_t) zap_mutex_create(zap_mutex_t **mutex);
OZ_DECLARE(zap_status_t) zap_mutex_destroy(zap_mutex_t **mutex);
OZ_DECLARE(zap_status_t) _zap_mutex_lock(zap_mutex_t *mutex);
OZ_DECLARE(zap_status_t) _zap_mutex_trylock(zap_mutex_t *mutex);
OZ_DECLARE(zap_status_t) _zap_mutex_unlock(zap_mutex_t *mutex);

OZ_DECLARE(zap_status_t) zap_interrupt_create(zap_interrupt_t **ininterrupt, zap_socket_t device);
OZ_DECLARE(zap_status_t) zap_interrupt_wait(zap_interrupt_t *interrupt, int ms);
OZ_DECLARE(zap_status_t) zap_interrupt_signal(zap_interrupt_t *interrupt);
OZ_DECLARE(zap_status_t) zap_interrupt_destroy(zap_interrupt_t **ininterrupt);
OZ_DECLARE(zap_status_t) zap_interrupt_multiple_wait(zap_interrupt_t *interrupts[], zap_size_t size, int ms);
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

