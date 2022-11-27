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
APR_DECLARE(fspr_status_t) fspr_##name##_inherit_set(fspr_##name##_t *the##name) \
{                                                                   \
    IF_WIN_OS_IS_UNICODE                                            \
    {                                                               \
        if (!SetHandleInformation(the##name->filehand,              \
                                  HANDLE_FLAG_INHERIT,              \
                                  HANDLE_FLAG_INHERIT))             \
            return fspr_get_os_error();                              \
    }                                                               \
    ELSE_WIN_OS_IS_ANSI                                             \
    {                                                               \
        HANDLE temp, hproc = GetCurrentProcess();                   \
        if (!DuplicateHandle(hproc, the##name->filehand,            \
                             hproc, &temp, 0, TRUE,                 \
                             DUPLICATE_SAME_ACCESS))                \
            return fspr_get_os_error();                              \
        CloseHandle(the##name->filehand);                           \
        the##name->filehand = temp;                                 \
    }                                                               \
    return APR_SUCCESS;                                             \
}

#define APR_IMPLEMENT_INHERIT_UNSET(name, flag, pool, cleanup)      \
APR_DECLARE(fspr_status_t) fspr_##name##_inherit_unset(fspr_##name##_t *the##name)\
{                                                                   \
    IF_WIN_OS_IS_UNICODE                                            \
    {                                                               \
        if (!SetHandleInformation(the##name->filehand,              \
                                  HANDLE_FLAG_INHERIT, 0))          \
            return fspr_get_os_error();                              \
    }                                                               \
    ELSE_WIN_OS_IS_ANSI                                             \
    {                                                               \
        HANDLE temp, hproc = GetCurrentProcess();                   \
        if (!DuplicateHandle(hproc, the##name->filehand,            \
                             hproc, &temp, 0, FALSE,                \
                             DUPLICATE_SAME_ACCESS))                \
            return fspr_get_os_error();                              \
        CloseHandle(the##name->filehand);                           \
        the##name->filehand = temp;                                 \
    }                                                               \
    return APR_SUCCESS;                                             \
}

#endif	/* ! INHERIT_H */
