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

#ifndef APR_RANDOM_H
#define APR_RANDOM_H

#include <fspr_pools.h>

typedef struct fspr_crypto_hash_t fspr_crypto_hash_t;

typedef void fspr_crypto_hash_init_t(fspr_crypto_hash_t *hash);
typedef void fspr_crypto_hash_add_t(fspr_crypto_hash_t *hash,const void *data,
                                   fspr_size_t bytes);
typedef void fspr_crypto_hash_finish_t(fspr_crypto_hash_t *hash,
                                      unsigned char *result);

/* FIXME: make this opaque */
struct fspr_crypto_hash_t {
    fspr_crypto_hash_init_t *init;
    fspr_crypto_hash_add_t *add;
    fspr_crypto_hash_finish_t *finish;
    fspr_size_t size;
    void *data;
};

APR_DECLARE(fspr_crypto_hash_t *) fspr_crypto_sha256_new(fspr_pool_t *p);

typedef struct fspr_random_t fspr_random_t;

APR_DECLARE(void) fspr_random_init(fspr_random_t *g,fspr_pool_t *p,
                                  fspr_crypto_hash_t *pool_hash,
                                  fspr_crypto_hash_t *key_hash,
                                  fspr_crypto_hash_t *prng_hash);
APR_DECLARE(fspr_random_t *) fspr_random_standard_new(fspr_pool_t *p);
APR_DECLARE(void) fspr_random_add_entropy(fspr_random_t *g,
                                         const void *entropy_,
                                         fspr_size_t bytes);
APR_DECLARE(fspr_status_t) fspr_random_insecure_bytes(fspr_random_t *g,
                                                    void *random,
                                                    fspr_size_t bytes);
APR_DECLARE(fspr_status_t) fspr_random_secure_bytes(fspr_random_t *g,
                                                  void *random,
                                                  fspr_size_t bytes);
APR_DECLARE(void) fspr_random_barrier(fspr_random_t *g);
APR_DECLARE(fspr_status_t) fspr_random_secure_ready(fspr_random_t *r);
APR_DECLARE(fspr_status_t) fspr_random_insecure_ready(fspr_random_t *r);

/* Call this in the child after forking to mix the randomness
   pools. Note that its generally a bad idea to fork a process with a
   real PRNG in it - better to have the PRNG externally and get the
   randomness from there. However, if you really must do it, then you
   should supply all your entropy to all the PRNGs - don't worry, they
   won't produce the same output.

   Note that fspr_proc_fork() calls this for you, so only weird
   applications need ever call it themselves.
*/
struct fspr_proc_t;
APR_DECLARE(void) fspr_random_after_fork(struct fspr_proc_t *proc);

#endif /* ndef APR_RANDOM_H */
