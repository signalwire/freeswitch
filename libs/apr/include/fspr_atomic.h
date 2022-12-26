/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef APR_ATOMIC_H
#define APR_ATOMIC_H

/**
 * @file fspr_atomic.h
 * @brief APR Atomic Operations
 */

#include "fspr.h"
#include "fspr_pools.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup fspr_atomic Atomic Operations
 * @ingroup APR 
 * @{
 */

/**
 * this function is required on some platforms to initialize the
 * atomic operation's internal structures
 * @param p pool
 * @return APR_SUCCESS on successful completion
 */
APR_DECLARE(fspr_status_t) fspr_atomic_init(fspr_pool_t *p);

/*
 * Atomic operations on 32-bit values
 * Note: Each of these functions internally implements a memory barrier
 * on platforms that require it
 */

/**
 * atomically read an fspr_uint32_t from memory
 * @param mem the pointer
 */
APR_DECLARE(fspr_uint32_t) fspr_atomic_read32(volatile fspr_uint32_t *mem);

/**
 * atomically set an fspr_uint32_t in memory
 * @param mem pointer to the object
 * @param val value that the object will assume
 */
APR_DECLARE(void) fspr_atomic_set32(volatile fspr_uint32_t *mem, fspr_uint32_t val);

/**
 * atomically add 'val' to an fspr_uint32_t
 * @param mem pointer to the object
 * @param val amount to add
 * @return old value pointed to by mem
 */
APR_DECLARE(fspr_uint32_t) fspr_atomic_add32(volatile fspr_uint32_t *mem, fspr_uint32_t val);

/**
 * atomically subtract 'val' from an fspr_uint32_t
 * @param mem pointer to the object
 * @param val amount to subtract
 */
APR_DECLARE(void) fspr_atomic_sub32(volatile fspr_uint32_t *mem, fspr_uint32_t val);

/**
 * atomically increment an fspr_uint32_t by 1
 * @param mem pointer to the object
 * @return old value pointed to by mem
 */
APR_DECLARE(fspr_uint32_t) fspr_atomic_inc32(volatile fspr_uint32_t *mem);

/**
 * atomically decrement an fspr_uint32_t by 1
 * @param mem pointer to the atomic value
 * @return zero if the value becomes zero on decrement, otherwise non-zero
 */
APR_DECLARE(int) fspr_atomic_dec32(volatile fspr_uint32_t *mem);

/**
 * compare an fspr_uint32_t's value with 'cmp'.
 * If they are the same swap the value with 'with'
 * @param mem pointer to the value
 * @param with what to swap it with
 * @param cmp the value to compare it to
 * @return the old value of *mem
 */
APR_DECLARE(fspr_uint32_t) fspr_atomic_cas32(volatile fspr_uint32_t *mem, fspr_uint32_t with,
                              fspr_uint32_t cmp);

/**
 * exchange an fspr_uint32_t's value with 'val'.
 * @param mem pointer to the value
 * @param val what to swap it with
 * @return the old value of *mem
 */
APR_DECLARE(fspr_uint32_t) fspr_atomic_xchg32(volatile fspr_uint32_t *mem, fspr_uint32_t val);

/**
 * compare the pointer's value with cmp.
 * If they are the same swap the value with 'with'
 * @param mem pointer to the pointer
 * @param with what to swap it with
 * @param cmp the value to compare it to
 * @return the old value of the pointer
 */
APR_DECLARE(void*) fspr_atomic_casptr(volatile void **mem, void *with, const void *cmp);

/** @} */

#ifdef __cplusplus
}
#endif

#endif	/* !APR_ATOMIC_H */
