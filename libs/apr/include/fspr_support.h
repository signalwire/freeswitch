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

#ifndef APR_SUPPORT_H
#define APR_SUPPORT_H

/**
 * @file fspr_support.h
 * @brief APR Support functions
 */

#include "fspr.h"
#include "fspr_network_io.h"
#include "fspr_file_io.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @defgroup fspr_support Internal APR support functions
 * @ingroup APR 
 * @{
 */

/**
 * Wait for IO to occur or timeout.
 *
 * Uses POOL for temporary allocations.
 */
fspr_status_t fspr_wait_for_io_or_timeout(fspr_file_t *f, fspr_socket_t *s,
                                        int for_read);

/** @} */

#ifdef __cplusplus
}
#endif

#endif  /* ! APR_SUPPORT_H */
