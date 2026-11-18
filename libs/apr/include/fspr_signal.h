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

#ifndef APR_SIGNAL_H
#define APR_SIGNAL_H

/**
 * @file fspr_signal.h 
 * @brief APR Signal Handling
 */

#include "fspr.h"
#include "fspr_pools.h"

#if APR_HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @defgroup fspr_signal Handling
 * @ingroup APR 
 * @{
 */

#if APR_HAVE_SIGACTION || defined(DOXYGEN)

#if defined(DARWIN) && !defined(__cplusplus) && !defined(_ANSI_SOURCE)
/* work around Darwin header file bugs
 *   http://www.opensource.apple.com/bugs/X/BSD%20Kernel/2657228.html
 */
#undef SIG_DFL
#undef SIG_IGN
#undef SIG_ERR
#define SIG_DFL (void (*)(int))0
#define SIG_IGN (void (*)(int))1
#define SIG_ERR (void (*)(int))-1
#endif

/** Function prototype for signal handlers */
typedef void fspr_sigfunc_t(int);

/**
 * Set the signal handler function for a given signal
 * @param signo The signal (eg... SIGWINCH)
 * @param func the function to get called
 */
APR_DECLARE(fspr_sigfunc_t *) fspr_signal(int signo, fspr_sigfunc_t * func);

#if defined(SIG_IGN) && !defined(SIG_ERR)
#define SIG_ERR ((fspr_sigfunc_t *) -1)
#endif

#else /* !APR_HAVE_SIGACTION */
#define fspr_signal(a, b) signal(a, b)
#endif


/**
 * Get the description for a specific signal number
 * @param signum The signal number
 * @return The description of the signal
 */
APR_DECLARE(const char *) fspr_signal_description_get(int signum);

/**
 * APR-private function for initializing the signal package
 * @internal
 * @param pglobal The internal, global pool
 */
void fspr_signal_init(fspr_pool_t *pglobal);

/**
 * Block the delivery of a particular signal
 * @param signum The signal number
 * @return status
 */
APR_DECLARE(fspr_status_t) fspr_signal_block(int signum);

/**
 * Enable the delivery of a particular signal
 * @param signum The signal number
 * @return status
 */
APR_DECLARE(fspr_status_t) fspr_signal_unblock(int signum);

/** @} */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* APR_SIGNAL_H */
