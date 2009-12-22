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
**  uuid_md5.h: MD5 API definition
*/

#ifndef __MD5_H___
#define __MD5_H___

#include <string.h> /* size_t */

#define MD5_PREFIX uuid_

/* embedding support */
#ifdef MD5_PREFIX
#if defined(__STDC__) || defined(__cplusplus)
#define __MD5_CONCAT(x,y) x ## y
#define MD5_CONCAT(x,y) __MD5_CONCAT(x,y)
#else
#define __MD5_CONCAT(x) x
#define MD5_CONCAT(x,y) __MD5_CONCAT(x)y
#endif
#define md5_st      MD5_CONCAT(MD5_PREFIX,md5_st)
#define md5_t       MD5_CONCAT(MD5_PREFIX,md5_t)
#define md5_create  MD5_CONCAT(MD5_PREFIX,md5_create)
#define md5_init    MD5_CONCAT(MD5_PREFIX,md5_init)
#define md5_update  MD5_CONCAT(MD5_PREFIX,md5_update)
#define md5_store   MD5_CONCAT(MD5_PREFIX,md5_store)
#define md5_format  MD5_CONCAT(MD5_PREFIX,md5_format)
#define md5_destroy MD5_CONCAT(MD5_PREFIX,md5_destroy)
#endif

struct md5_st;
typedef struct md5_st md5_t;

#define MD5_LEN_BIN 16
#define MD5_LEN_STR 32

typedef enum {
    MD5_RC_OK  = 0,
    MD5_RC_ARG = 1,
    MD5_RC_MEM = 2
} md5_rc_t;

extern md5_rc_t md5_create  (md5_t **md5);
extern md5_rc_t md5_init    (md5_t  *md5);
extern md5_rc_t md5_update  (md5_t  *md5, const void  *data_ptr, size_t  data_len);
extern md5_rc_t md5_store   (md5_t  *md5,       void **data_ptr, size_t *data_len);
extern md5_rc_t md5_format  (md5_t  *md5,       char **data_ptr, size_t *data_len);
extern md5_rc_t md5_destroy (md5_t  *md5);

#endif /* __MD5_H___ */

