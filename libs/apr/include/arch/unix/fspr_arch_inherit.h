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

#ifndef INHERIT_H
#define INHERIT_H

#include "fspr_inherit.h"

#define APR_INHERIT (1 << 24)    /* Must not conflict with other bits */

#define APR_IMPLEMENT_INHERIT_SET(name, flag, pool, cleanup)        \
fspr_status_t fspr_##name##_inherit_set(fspr_##name##_t *the##name)    \
{                                                                   \
    if (the##name->flag & APR_FILE_NOCLEANUP)                       \
        return APR_EINVAL;                                          \
    if (!(the##name->flag & APR_INHERIT)) {                         \
        the##name->flag |= APR_INHERIT;                             \
        fspr_pool_child_cleanup_set(the##name->pool,                 \
                                   (void *)the##name,               \
                                   cleanup, fspr_pool_cleanup_null); \
    }                                                               \
    return APR_SUCCESS;                                             \
}

#define APR_IMPLEMENT_INHERIT_UNSET(name, flag, pool, cleanup)      \
fspr_status_t fspr_##name##_inherit_unset(fspr_##name##_t *the##name)  \
{                                                                   \
    if (the##name->flag & APR_FILE_NOCLEANUP)                       \
        return APR_EINVAL;                                          \
    if (the##name->flag & APR_INHERIT) {                            \
        the##name->flag &= ~APR_INHERIT;                            \
        fspr_pool_child_cleanup_set(the##name->pool,                 \
                                   (void *)the##name,               \
                                   cleanup, cleanup);               \
    }                                                               \
    return APR_SUCCESS;                                             \
}

#endif	/* ! INHERIT_H */
