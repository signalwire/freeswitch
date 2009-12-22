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
**  uuid_dce.h: DCE 1.1 compatibility API definition
*/

#ifndef __UUID_DCE_H___
#define __UUID_DCE_H___

/* sanity check usage */
#ifdef __UUID_H__
#error the regular OSSP uuid API and the DCE 1.1 backward compatibility API are mutually exclusive -- you cannot use them at the same time.
#endif

/* resolve namespace conflicts (at linking level) with regular OSSP uuid API */
#define uuid_create      uuid_dce_create
#define uuid_create_nil  uuid_dce_create_nil
#define uuid_is_nil      uuid_dce_is_nil
#define uuid_compare     uuid_dce_compare
#define uuid_equal       uuid_dce_equal
#define uuid_from_string uuid_dce_from_string
#define uuid_to_string   uuid_dce_to_string
#define uuid_hash        uuid_dce_hash

/* DCE 1.1 uuid_t type */
typedef struct {
#if 0
    /* stricter but unportable version */
    uuid_uint32_t   time_low;
    uuid_uint16_t   time_mid;
    uuid_uint16_t   time_hi_and_version;
    uuid_uint8_t    clock_seq_hi_and_reserved;
    uuid_uint8_t    clock_seq_low;
    uuid_uint8_t    node[6];
#else
    /* sufficient and portable version */
    unsigned char   data[16];
#endif
} uuid_t;
typedef uuid_t *uuid_p_t;

/* DCE 1.1 uuid_vector_t type */
typedef struct {
    unsigned int    count;
    uuid_t         *uuid[1];
} uuid_vector_t;

/* DCE 1.1 UUID API status codes */
enum {
    uuid_s_ok = 0,     /* standardized */
    uuid_s_error = 1   /* implementation specific */
};

/* DCE 1.1 UUID API functions */
extern void          uuid_create      (uuid_t *,               int *);
extern void          uuid_create_nil  (uuid_t *,               int *);
extern int           uuid_is_nil      (uuid_t *,               int *);
extern int           uuid_compare     (uuid_t *, uuid_t *,     int *);
extern int           uuid_equal       (uuid_t *, uuid_t *,     int *);
extern void          uuid_from_string (const char *, uuid_t *, int *);
extern void          uuid_to_string   (uuid_t *,     char **,  int *);
extern unsigned int  uuid_hash        (uuid_t *,               int *);

#endif /* __UUID_DCE_H___ */

