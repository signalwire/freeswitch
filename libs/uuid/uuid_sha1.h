/*
**  OSSP uuid - Universally Unique Identifier
**  Copyright (c) 2004-2008 Ralf S. Engelschall <rse@engelschall.com>
**  Copyright (c) 2004-2008 The OSSP Project <http://www.ossp.org/>
**
**  This file is part of OSSP uuid, a library for the generation
**  of UUIDs which can found at http://www.ossp.org/pkg/lib/uuid/
**
**  Permission to use, copy, modify, and distribute this software for
**  any purpose with or without fee is hereby granted, provided that
**  the above copyright notice and this permission notice appear in all
**  copies.
**
**  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
**  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
**  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
**  IN NO EVENT SHALL THE AUTHORS AND COPYRIGHT HOLDERS AND THEIR
**  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
**  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
**  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
**  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
**  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
**  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
**  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
**  SUCH DAMAGE.
**
**  uuid_sha1.h: SHA-1 API definition
*/

#ifndef __SHA1_H___
#define __SHA1_H___

#include <string.h> /* size_t */

#define SHA1_PREFIX uuid_

/* embedding support */
#ifdef SHA1_PREFIX
#if defined(__STDC__) || defined(__cplusplus)
#define __SHA1_CONCAT(x,y) x ## y
#define SHA1_CONCAT(x,y) __SHA1_CONCAT(x,y)
#else
#define __SHA1_CONCAT(x) x
#define SHA1_CONCAT(x,y) __SHA1_CONCAT(x)y
#endif
#define sha1_st      SHA1_CONCAT(SHA1_PREFIX,sha1_st)
#define sha1_t       SHA1_CONCAT(SHA1_PREFIX,sha1_t)
#define sha1_create  SHA1_CONCAT(SHA1_PREFIX,sha1_create)
#define sha1_init    SHA1_CONCAT(SHA1_PREFIX,sha1_init)
#define sha1_update  SHA1_CONCAT(SHA1_PREFIX,sha1_update)
#define sha1_store   SHA1_CONCAT(SHA1_PREFIX,sha1_store)
#define sha1_format  SHA1_CONCAT(SHA1_PREFIX,sha1_format)
#define sha1_destroy SHA1_CONCAT(SHA1_PREFIX,sha1_destroy)
#endif

struct sha1_st;
typedef struct sha1_st sha1_t;

#define SHA1_LEN_BIN 20
#define SHA1_LEN_STR 40

typedef enum {
    SHA1_RC_OK  = 0,
    SHA1_RC_ARG = 1,
    SHA1_RC_MEM = 2,
    SHA1_RC_INT = 3
} sha1_rc_t;

extern sha1_rc_t sha1_create  (sha1_t **sha1);
extern sha1_rc_t sha1_init    (sha1_t  *sha1);
extern sha1_rc_t sha1_update  (sha1_t  *sha1, const void  *data_ptr, size_t  data_len);
extern sha1_rc_t sha1_store   (sha1_t  *sha1,       void **data_ptr, size_t *data_len);
extern sha1_rc_t sha1_format  (sha1_t  *sha1,       char **data_ptr, size_t *data_len);
extern sha1_rc_t sha1_destroy (sha1_t  *sha1);

#endif /* __SHA1_H___ */

