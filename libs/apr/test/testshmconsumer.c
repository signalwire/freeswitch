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

#include "fspr_shm.h"
#include "fspr_errno.h"
#include "fspr_general.h"
#include "fspr_lib.h"
#include "fspr_strings.h"
#include "fspr_time.h"
#include "testshm.h"
#include "fspr.h"

#if APR_HAVE_STDLIB_H
#include <stdlib.h>
#endif


#if APR_HAS_SHARED_MEMORY

static int msgwait(int sleep_sec, int first_box, int last_box)
{
    int i;
    int recvd = 0;
    fspr_time_t start = fspr_time_now();
    fspr_interval_time_t sleep_duration = fspr_time_from_sec(sleep_sec);
    while (fspr_time_now() - start < sleep_duration) {
        for (i = first_box; i < last_box; i++) {
            if (boxes[i].msgavail && !strcmp(boxes[i].msg, MSG)) {
                recvd++;
                boxes[i].msgavail = 0; /* reset back to 0 */
                memset(boxes[i].msg, 0, 1024);
            }
        }
        fspr_sleep(fspr_time_from_sec(1));
    }
    return recvd;
}

int main(void)
{
    fspr_status_t rv;
    fspr_pool_t *pool;
    fspr_shm_t *shm;
    int recvd;

    fspr_initialize();
    
    if (fspr_pool_create(&pool, NULL) != APR_SUCCESS) {
        exit(-1);
    }

    rv = fspr_shm_attach(&shm, SHARED_FILENAME, pool);
    if (rv != APR_SUCCESS) {
        exit(-2);
    }

    boxes = fspr_shm_baseaddr_get(shm);

    /* consume messages on all of the boxes */
    recvd = msgwait(30, 0, N_BOXES); /* wait for 30 seconds for messages */

    rv = fspr_shm_detach(shm);
    if (rv != APR_SUCCESS) {
        exit(-3);
    }

    return recvd;
}

#else /* APR_HAS_SHARED_MEMORY */

int main(void)
{
    /* Just return, this program will never be called, so we don't need
     * to print a message 
     */
    return 0;
}

#endif /* APR_HAS_SHARED_MEMORY */

