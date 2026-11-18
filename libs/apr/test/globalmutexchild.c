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

#include "testglobalmutex.h"
#include "fspr_pools.h"
#include "fspr_file_io.h"
#include "fspr_general.h"
#include "fspr_global_mutex.h"
#include "fspr_strings.h"
#include "fspr.h"

#if APR_HAVE_STDLIB_H
#include <stdlib.h>
#endif


int main(int argc, const char * const argv[])
{
    fspr_pool_t *p;
    int i = 0;
    fspr_lockmech_e mech;
    fspr_global_mutex_t *global_lock;
    fspr_status_t rv;

    fspr_initialize();
    atexit(fspr_terminate);
    
    fspr_pool_create(&p, NULL);
    if (argc >= 2) {
        mech = (fspr_lockmech_e)fspr_strtoi64(argv[1], NULL, 0);
    }
    else {
        mech = APR_LOCK_DEFAULT;
    }
    rv = fspr_global_mutex_create(&global_lock, LOCKNAME, mech, p);
    if (rv != APR_SUCCESS) {
        exit(-rv);
    }
    fspr_global_mutex_child_init(&global_lock, LOCKNAME, p);
    
    while (1) {
        fspr_global_mutex_lock(global_lock);
        if (i == MAX_ITER) {
            fspr_global_mutex_unlock(global_lock);
            exit(i);
        }
        i++;
        fspr_global_mutex_unlock(global_lock);
    }
    exit(0);
}
