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
**  uuid_prng.h: PRNG API definition
*/

#ifndef __PRNG_H___
#define __PRNG_H___

#include <string.h> /* size_t */

#define PRNG_PREFIX uuid_

/* embedding support */
#ifdef PRNG_PREFIX
#if defined(__STDC__) || defined(__cplusplus)
#define __PRNG_CONCAT(x,y) x ## y
#define PRNG_CONCAT(x,y) __PRNG_CONCAT(x,y)
#else
#define __PRNG_CONCAT(x) x
#define PRNG_CONCAT(x,y) __PRNG_CONCAT(x)y
#endif
#define prng_st      PRNG_CONCAT(PRNG_PREFIX,prng_st)
#define prng_t       PRNG_CONCAT(PRNG_PREFIX,prng_t)
#define prng_create  PRNG_CONCAT(PRNG_PREFIX,prng_create)
#define prng_data    PRNG_CONCAT(PRNG_PREFIX,prng_data)
#define prng_destroy PRNG_CONCAT(PRNG_PREFIX,prng_destroy)
#endif

struct prng_st;
typedef struct prng_st prng_t;

typedef enum {
    PRNG_RC_OK  = 0,
    PRNG_RC_ARG = 1,
    PRNG_RC_MEM = 2,
    PRNG_RC_INT = 3
} prng_rc_t;

extern prng_rc_t prng_create  (prng_t **prng);
extern prng_rc_t prng_data    (prng_t  *prng, void *data_ptr, size_t data_len);
extern prng_rc_t prng_destroy (prng_t  *prng);

#endif /* __PRNG_H___ */

