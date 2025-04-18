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

#include <stdlib.h>

fspr_status_t fspr_atomic_init(fspr_pool_t *p)
{
    return APR_SUCCESS;
}

fspr_uint32_t fspr_atomic_add32(volatile fspr_uint32_t *mem, fspr_uint32_t val)
{
    fspr_uint32_t old, new_val; 

    old = *mem;   /* old is automatically updated on cs failure */
    do {
        new_val = old + val;
    } while (__cs(&old, (cs_t *)mem, new_val));
    return old;
}

void fspr_atomic_sub32(volatile fspr_uint32_t *mem, fspr_uint32_t val)
{
     fspr_uint32_t old, new_val;

     old = *mem;   /* old is automatically updated on cs failure */
     do {
         new_val = old - val;
     } while (__cs(&old, (cs_t *)mem, new_val));
}

fspr_uint32_t fspr_atomic_inc32(volatile fspr_uint32_t *mem)
{
    return fspr_atomic_add32(mem, 1);
}

int fspr_atomic_dec32(volatile fspr_uint32_t *mem)
{
    fspr_uint32_t old, new_val; 

    old = *mem;   /* old is automatically updated on cs failure */
    do {
        new_val = old - 1;
    } while (__cs(&old, (cs_t *)mem, new_val)); 

    return new_val != 0;
}

fspr_uint32_t fspr_atomic_read32(volatile fspr_uint32_t *mem)
{
    return *mem;
}

void fspr_atomic_set32(volatile fspr_uint32_t *mem, fspr_uint32_t val)
{
    *mem = val;
}

fspr_uint32_t fspr_atomic_cas32(volatile fspr_uint32_t *mem, fspr_uint32_t swap, 
                              fspr_uint32_t cmp)
{
    fspr_uint32_t old = cmp;
    
    __cs(&old, (cs_t *)mem, swap);
    return old; /* old is automatically updated from mem on cs failure */
}

fspr_uint32_t fspr_atomic_xchg32(volatile fspr_uint32_t *mem, fspr_uint32_t val)
{
    fspr_uint32_t old, new_val; 

    old = *mem;   /* old is automatically updated on cs failure */
    do {
        new_val = val;
    } while (__cs(&old, (cs_t *)mem, new_val)); 

    return old;
}

