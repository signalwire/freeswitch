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
/*
 * FILE:        sha2.h
 * AUTHOR:      Aaron D. Gifford <me@aarongifford.com>
 * 
 * A licence was granted to the ASF by Aaron on 4 November 2003.
 */

#ifndef __SHA2_H__
#define __SHA2_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "fspr.h"

/*** SHA-256/384/512 Various Length Definitions ***********************/
#define SHA256_BLOCK_LENGTH             64
#define SHA256_DIGEST_LENGTH            32
#define SHA256_DIGEST_STRING_LENGTH     (SHA256_DIGEST_LENGTH * 2 + 1)
#define SHA384_BLOCK_LENGTH             128
#define SHA384_DIGEST_LENGTH            48
#define SHA384_DIGEST_STRING_LENGTH     (SHA384_DIGEST_LENGTH * 2 + 1)
#define SHA512_BLOCK_LENGTH             128
#define SHA512_DIGEST_LENGTH            64
#define SHA512_DIGEST_STRING_LENGTH     (SHA512_DIGEST_LENGTH * 2 + 1)


/*** SHA-256/384/512 Context Structures *******************************/
typedef struct _SHA256_CTX {
        fspr_uint32_t    state[8];
        fspr_uint64_t    bitcount;
        fspr_byte_t      buffer[SHA256_BLOCK_LENGTH];
} SHA256_CTX;
typedef struct _SHA512_CTX {
        fspr_uint64_t    state[8];
        fspr_uint64_t    bitcount[2];
        fspr_byte_t      buffer[SHA512_BLOCK_LENGTH];
} SHA512_CTX;

typedef SHA512_CTX SHA384_CTX;


/*** SHA-256/384/512 Function Prototypes ******************************/
void fspr__SHA256_Init(SHA256_CTX *);
void fspr__SHA256_Update(SHA256_CTX *, const fspr_byte_t *, size_t);
void fspr__SHA256_Final(fspr_byte_t [SHA256_DIGEST_LENGTH], SHA256_CTX *);
char* fspr__SHA256_End(SHA256_CTX *, char [SHA256_DIGEST_STRING_LENGTH]);
char* fspr__SHA256_Data(const fspr_byte_t *, size_t,
                  char [SHA256_DIGEST_STRING_LENGTH]);

void fspr__SHA384_Init(SHA384_CTX *);
void fspr__SHA384_Update(SHA384_CTX *, const fspr_byte_t *, size_t);
void fspr__SHA384_Final(fspr_byte_t [SHA384_DIGEST_LENGTH], SHA384_CTX *);
char* fspr__SHA384_End(SHA384_CTX *, char [SHA384_DIGEST_STRING_LENGTH]);
char* fspr__SHA384_Data(const fspr_byte_t *, size_t,
                  char [SHA384_DIGEST_STRING_LENGTH]);

void fspr__SHA512_Init(SHA512_CTX *);
void fspr__SHA512_Update(SHA512_CTX *, const fspr_byte_t *, size_t);
void fspr__SHA512_Final(fspr_byte_t [SHA512_DIGEST_LENGTH], SHA512_CTX *);
char* fspr__SHA512_End(SHA512_CTX *, char [SHA512_DIGEST_STRING_LENGTH]);
char* fspr__SHA512_Data(const fspr_byte_t *, size_t,
                  char [SHA512_DIGEST_STRING_LENGTH]);

#ifdef  __cplusplus
}
#endif /* __cplusplus */

#endif /* __SHA2_H__ */

