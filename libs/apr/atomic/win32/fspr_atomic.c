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

#include "fspr.h"
#include "fspr_atomic.h"
#include "fspr_thread_mutex.h"

APR_DECLARE(fspr_status_t) fspr_atomic_init(fspr_pool_t *p)
{
    return APR_SUCCESS;
}

/* 
 * Remapping function pointer type to accept fspr_uint32_t's type-safely
 * as the arguments for as our fspr_atomic_foo32 Functions
 */
#if (_MSC_VER < 1800)
typedef WINBASEAPI fspr_uint32_t (WINAPI * fspr_atomic_win32_ptr_fn)
    (fspr_uint32_t volatile *);
typedef WINBASEAPI fspr_uint32_t (WINAPI * fspr_atomic_win32_ptr_val_fn)
    (fspr_uint32_t volatile *, 
     fspr_uint32_t);
typedef WINBASEAPI fspr_uint32_t (WINAPI * fspr_atomic_win32_ptr_val_val_fn)
    (fspr_uint32_t volatile *, 
     fspr_uint32_t, fspr_uint32_t);
typedef WINBASEAPI void * (WINAPI * fspr_atomic_win32_ptr_ptr_ptr_fn)
    (volatile void **, 
     void *, const void *);
#endif

APR_DECLARE(fspr_uint32_t) fspr_atomic_add32(volatile fspr_uint32_t *mem, fspr_uint32_t val)
{
#if (defined(_M_IA64) || defined(_M_AMD64))
    return InterlockedExchangeAdd(mem, val);
#elif (_MSC_VER >= 1800)
	return InterlockedExchangeAdd(mem, val);
#else
    return ((fspr_atomic_win32_ptr_val_fn)InterlockedExchangeAdd)(mem, val);
#endif
}

/* Of course we want the 2's compliment of the unsigned value, val */
#pragma warning(disable: 4146)

APR_DECLARE(void) fspr_atomic_sub32(volatile fspr_uint32_t *mem, fspr_uint32_t val)
{
#if (defined(_M_IA64) || defined(_M_AMD64))
    InterlockedExchangeAdd(mem, -val);
#elif (_MSC_VER >= 1800)
	InterlockedExchangeAdd(mem, -val);
#else
    ((fspr_atomic_win32_ptr_val_fn)InterlockedExchangeAdd)(mem, -val);
#endif
}

APR_DECLARE(fspr_uint32_t) fspr_atomic_inc32(volatile fspr_uint32_t *mem)
{
    /* we return old value, win32 returns new value :( */
#if (defined(_M_IA64) || defined(_M_AMD64)) && !defined(RC_INVOKED)
    return InterlockedIncrement(mem) - 1;
#elif (_MSC_VER >= 1800)
	return InterlockedIncrement(mem) - 1;
#else
    return ((fspr_atomic_win32_ptr_fn)InterlockedIncrement)(mem) - 1;
#endif
}

APR_DECLARE(int) fspr_atomic_dec32(volatile fspr_uint32_t *mem)
{
#if (defined(_M_IA64) || defined(_M_AMD64)) && !defined(RC_INVOKED)
    return InterlockedDecrement(mem);
#elif (_MSC_VER >= 1800)
	return InterlockedDecrement(mem);
#else
    return ((fspr_atomic_win32_ptr_fn)InterlockedDecrement)(mem);
#endif
}

APR_DECLARE(void) fspr_atomic_set32(volatile fspr_uint32_t *mem, fspr_uint32_t val)
{
#if (defined(_M_IA64) || defined(_M_AMD64)) && !defined(RC_INVOKED)
    InterlockedExchange(mem, val);
#elif (_MSC_VER >= 1800)
	InterlockedExchange(mem, val);
#else
    ((fspr_atomic_win32_ptr_val_fn)InterlockedExchange)(mem, val);
#endif
}

APR_DECLARE(fspr_uint32_t) fspr_atomic_read32(volatile fspr_uint32_t *mem)
{
    return *mem;
}

APR_DECLARE(fspr_uint32_t) fspr_atomic_cas32(volatile fspr_uint32_t *mem, fspr_uint32_t with,
                                           fspr_uint32_t cmp)
{
#if (defined(_M_IA64) || defined(_M_AMD64)) && !defined(RC_INVOKED)
    return InterlockedCompareExchange(mem, with, cmp);
#elif (_MSC_VER >= 1800)
	return InterlockedCompareExchange(mem, with, cmp);
#else
    return ((fspr_atomic_win32_ptr_val_val_fn)InterlockedCompareExchange)(mem, with, cmp);
#endif
}

APR_DECLARE(void *) fspr_atomic_casptr(volatile void **mem, void *with, const void *cmp)
{
#if (defined(_M_IA64) || defined(_M_AMD64)) && !defined(RC_INVOKED)
    return InterlockedCompareExchangePointer(mem, with, cmp);
#elif (_MSC_VER >= 1800)
	return InterlockedCompareExchangePointer(mem, with, cmp);
#else
    /* Too many VC6 users have stale win32 API files, stub this */
    return ((fspr_atomic_win32_ptr_ptr_ptr_fn)InterlockedCompareExchange)(mem, with, cmp);
#endif
}

APR_DECLARE(fspr_uint32_t) fspr_atomic_xchg32(volatile fspr_uint32_t *mem, fspr_uint32_t val)
{
#if (defined(_M_IA64) || defined(_M_AMD64)) && !defined(RC_INVOKED)
    return InterlockedExchange(mem, val);
#elif (_MSC_VER >= 1800)
	return InterlockedExchange(mem, val);
#else
    return ((fspr_atomic_win32_ptr_val_fn)InterlockedExchange)(mem, val);
#endif
}
