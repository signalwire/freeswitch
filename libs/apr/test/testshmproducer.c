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
static void msgput(int boxnum, char *msg)
{
    fspr_cpystrn(boxes[boxnum].msg, msg, strlen(msg) + 1);
    boxes[boxnum].msgavail = 1;
}

int main(void)
{
    fspr_status_t rv;
    fspr_pool_t *pool;
    fspr_shm_t *shm;
    int i;
    int sent = 0;

    fspr_initialize();
    
    if (fspr_pool_create(&pool, NULL) != APR_SUCCESS) {
        exit(-1);
    }

    rv = fspr_shm_attach(&shm, SHARED_FILENAME, pool);
    if (rv != APR_SUCCESS) {
        exit(-2);
    }

    boxes = fspr_shm_baseaddr_get(shm);

    /* produce messages on all of the boxes, in descending order,
     * Yes, we could just return N_BOXES, but I want to have a double-check
     * in this code.  The original code actually sent N_BOXES - 1 messages,
     * so rather than rely on possibly buggy code, this way we know that we
     * are returning the right number.
     */
    for (i = N_BOXES - 1, sent = 0; i >= 0; i--, sent++) {
        msgput(i, MSG);
        fspr_sleep(fspr_time_from_sec(1));
    }

    rv = fspr_shm_detach(shm);
    if (rv != APR_SUCCESS) {
        exit(-3);
    }

    return sent;
}

#else /* APR_HAS_SHARED_MEMORY */

int main(void)
{
    /* Just return, this program will never be launched, so there is no
     * reason to print a message.
     */
    return 0;
}

#endif /* APR_HAS_SHARED_MEMORY */

