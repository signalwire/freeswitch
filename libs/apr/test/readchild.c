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

#include <stdlib.h>

#include "fspr_file_io.h"

int main(int argc, char *argv[])
{
    fspr_file_t *in, *out;
    fspr_size_t nbytes, total_bytes;
    fspr_pool_t *p;
    char buf[128];
    fspr_status_t rv;
    
    fspr_initialize();
    atexit(fspr_terminate);
    fspr_pool_create(&p, NULL);

    fspr_file_open_stdin(&in, p);
    fspr_file_open_stdout(&out, p);

    total_bytes = 0;
    nbytes = sizeof(buf);
    while ((rv = fspr_file_read(in, buf, &nbytes)) == APR_SUCCESS) {
        total_bytes += nbytes;
        nbytes = sizeof(buf);
    }

    fspr_file_printf(out, "%" APR_SIZE_T_FMT " bytes were read\n",
                    total_bytes);
    return 0;
}
