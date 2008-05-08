/* 
 * Simple Mutex abstraction
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


#ifndef _SIMPLE_ABSTRACT_MUTEX_H
#define _SIMPLE_ABSTRACT_MUTEX_H

typedef struct mutex mutex_t;

typedef enum mutex_status {
	MUTEX_SUCCESS,
	MUTEX_FAILURE
} mutex_status_t;

mutex_status_t iax_mutex_create(mutex_t **mutex);
mutex_status_t iax_mutex_destroy(mutex_t *mutex);
mutex_status_t iax_mutex_lock(mutex_t *mutex);
mutex_status_t iax_mutex_trylock(mutex_t *mutex);
mutex_status_t iax_mutex_unlock(mutex_t *mutex);

#endif
