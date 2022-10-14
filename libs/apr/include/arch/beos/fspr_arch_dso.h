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

#ifndef DSO_H
#define DSO_H

#include "fspr_private.h"
#include "fspr_general.h"
#include "fspr_pools.h"
#include "fspr_errno.h"
#include "fspr_dso.h"
#include "fspr.h"
#include <kernel/image.h>
#include <string.h>

#if APR_HAS_DSO

struct fspr_dso_handle_t {
    image_id      handle;    /* Handle to the DSO loaded */
    fspr_pool_t   *pool;
    const char   *errormsg;  /* if the load fails, we have an error
                              * message here :)
                              */
};

#endif

#endif
