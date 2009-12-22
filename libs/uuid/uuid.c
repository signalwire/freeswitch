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
**  uuid.c: library API implementation
*/

/* own headers (part 1/2) */
#include "uuid.h"
#include "uuid_ac.h"

/* system headers */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

/* own headers (part 2/2) */
#include "uuid_vers.h"
#include "uuid_md5.h"
#include "uuid_sha1.h"
#include "uuid_prng.h"
#include "uuid_mac.h"
#include "uuid_time.h"
#include "uuid_ui64.h"
#include "uuid_ui128.h"
#include "uuid_str.h"
#include "uuid_bm.h"
#include "uuid_ac.h"

/* maximum number of 100ns ticks of the actual resolution of system clock
   (which in our case is 1us (= 1000ns) because we use gettimeofday(2) */
#define UUIDS_PER_TICK 10

/* time offset between UUID and Unix Epoch time according to standards.
   (UUID UTC base time is October 15, 1582
    Unix UTC base time is January  1, 1970) */
#define UUID_TIMEOFFSET "01B21DD213814000"

/* IEEE 802 MAC address encoding/decoding bit fields */
#define IEEE_MAC_MCBIT BM_OCTET(0,0,0,0,0,0,0,1)
#define IEEE_MAC_LOBIT BM_OCTET(0,0,0,0,0,0,1,0)

/* IEEE 802 MAC address octet length */
#define IEEE_MAC_OCTETS 6

/* UUID binary representation according to UUID standards */
typedef struct {
    uuid_uint32_t  time_low;                  /* bits  0-31 of time field */
    uuid_uint16_t  time_mid;                  /* bits 32-47 of time field */
    uuid_uint16_t  time_hi_and_version;       /* bits 48-59 of time field plus 4 bit version */
    uuid_uint8_t   clock_seq_hi_and_reserved; /* bits  8-13 of clock sequence field plus 2 bit variant */
    uuid_uint8_t   clock_seq_low;             /* bits  0-7  of clock sequence field */
    uuid_uint8_t   node[IEEE_MAC_OCTETS];     /* bits  0-47 of node MAC address */
} uuid_obj_t;

/* abstract data type (ADT) of API */
struct uuid_st {
    uuid_obj_t     obj;                       /* inlined UUID object */
    prng_t        *prng;                      /* RPNG sub-object */
    md5_t         *md5;                       /* MD5 sub-object */
    sha1_t        *sha1;                      /* SHA-1 sub-object */
    uuid_uint8_t   mac[IEEE_MAC_OCTETS];      /* pre-determined MAC address */
    struct timeval time_last;                 /* last retrieved timestamp */
    unsigned long  time_seq;                  /* last timestamp sequence counter */
};

/* create UUID object */
uuid_rc_t uuid_create(uuid_t **uuid)
{
    uuid_t *obj;

    /* argument sanity check */
    if (uuid == NULL)
        return UUID_RC_ARG;

    /* allocate UUID object */
    if ((obj = (uuid_t *)malloc(sizeof(uuid_t))) == NULL)
        return UUID_RC_MEM;

    /* create PRNG, MD5 and SHA1 sub-objects */
    if (prng_create(&obj->prng) != PRNG_RC_OK) {
        free(obj);
        return UUID_RC_INT;
    }
    if (md5_create(&obj->md5) != MD5_RC_OK) {
        (void)prng_destroy(obj->prng);
        free(obj);
        return UUID_RC_INT;
    }
    if (sha1_create(&obj->sha1) != SHA1_RC_OK) {
        (void)md5_destroy(obj->md5);
        (void)prng_destroy(obj->prng);
        free(obj);
        return UUID_RC_INT;
    }

    /* set UUID object initially to "Nil UUID" */
    if (uuid_load(obj, "nil") != UUID_RC_OK) {
        (void)sha1_destroy(obj->sha1);
        (void)md5_destroy(obj->md5);
        (void)prng_destroy(obj->prng);
        free(obj);
        return UUID_RC_INT;
    }

    /* resolve MAC address for insertion into node field of UUIDs */
    if (!mac_address((unsigned char *)(obj->mac), sizeof(obj->mac))) {
        memset(obj->mac, 0, sizeof(obj->mac));
        obj->mac[0] = BM_OCTET(1,0,0,0,0,0,0,0);
    }

    /* initialize time attributes */
    obj->time_last.tv_sec  = 0;
    obj->time_last.tv_usec = 0;
    obj->time_seq = 0;

    /* store result object */
    *uuid = obj;

    return UUID_RC_OK;
}

/* destroy UUID object */
uuid_rc_t uuid_destroy(uuid_t *uuid)
{
    /* argument sanity check */
    if (uuid == NULL)
        return UUID_RC_ARG;

    /* destroy PRNG, MD5 and SHA-1 sub-objects */
    (void)prng_destroy(uuid->prng);
    (void)md5_destroy(uuid->md5);
    (void)sha1_destroy(uuid->sha1);

    /* free UUID object */
    free(uuid);

    return UUID_RC_OK;
}

/* clone UUID object */
uuid_rc_t uuid_clone(const uuid_t *uuid, uuid_t **clone)
{
    uuid_t *obj;

    /* argument sanity check */
    if (uuid == NULL || uuid_clone == NULL)
        return UUID_RC_ARG;

    /* allocate UUID object */
    if ((obj = (uuid_t *)malloc(sizeof(uuid_t))) == NULL)
        return UUID_RC_MEM;

    /* clone entire internal state */
    memcpy(obj, uuid, sizeof(uuid_t));

    /* re-initialize with new PRNG, MD5 and SHA1 sub-objects */
    if (prng_create(&obj->prng) != PRNG_RC_OK) {
        free(obj);
        return UUID_RC_INT;
    }
    if (md5_create(&obj->md5) != MD5_RC_OK) {
        (void)prng_destroy(obj->prng);
        free(obj);
        return UUID_RC_INT;
    }
    if (sha1_create(&obj->sha1) != SHA1_RC_OK) {
        (void)md5_destroy(obj->md5);
        (void)prng_destroy(obj->prng);
        free(obj);
        return UUID_RC_INT;
    }

    /* store result object */
    *clone = obj;

    return UUID_RC_OK;
}

/* check whether UUID object represents "Nil UUID" */
uuid_rc_t uuid_isnil(const uuid_t *uuid, int *result)
{
    const unsigned char *ucp;
    int i;

    /* sanity check argument(s) */
    if (uuid == NULL || result == NULL)
        return UUID_RC_ARG;

    /* a "Nil UUID" is defined as all octets zero, so check for this case */
    *result = UUID_TRUE;
    for (i = 0, ucp = (unsigned char *)&(uuid->obj); i < UUID_LEN_BIN; i++) {
        if (*ucp++ != (unsigned char)'\0') {
            *result = UUID_FALSE;
            break;
        }
    }

    return UUID_RC_OK;
}

/* compare UUID objects */
uuid_rc_t uuid_compare(const uuid_t *uuid1, const uuid_t *uuid2, int *result)
{
    int r;

    /* argument sanity check */
    if (result == NULL)
        return UUID_RC_ARG;

    /* convenience macro for setting result */
#define RESULT(r) \
    /*lint -save -e801 -e717*/ \
    do { \
        *result = (r); \
        goto result_exit; \
    } while (0) \
    /*lint -restore*/

    /* special cases: NULL or equal UUIDs */
    if (uuid1 == uuid2)
        RESULT(0);
    if (uuid1 == NULL && uuid2 == NULL)
        RESULT(0);
    if (uuid1 == NULL)
        RESULT((uuid_isnil(uuid2, &r) == UUID_RC_OK ? r : 0) ? 0 : -1);
    if (uuid2 == NULL)
        RESULT((uuid_isnil(uuid1, &r) == UUID_RC_OK ? r : 0) ? 0 : 1);

    /* standard cases: regular different UUIDs */
    if (uuid1->obj.time_low != uuid2->obj.time_low)
        RESULT((uuid1->obj.time_low < uuid2->obj.time_low) ? -1 : 1);
    if ((r = (int)uuid1->obj.time_mid
           - (int)uuid2->obj.time_mid) != 0)
        RESULT((r < 0) ? -1 : 1);
    if ((r = (int)uuid1->obj.time_hi_and_version
           - (int)uuid2->obj.time_hi_and_version) != 0)
        RESULT((r < 0) ? -1 : 1);
    if ((r = (int)uuid1->obj.clock_seq_hi_and_reserved
           - (int)uuid2->obj.clock_seq_hi_and_reserved) != 0)
        RESULT((r < 0) ? -1 : 1);
    if ((r = (int)uuid1->obj.clock_seq_low
           - (int)uuid2->obj.clock_seq_low) != 0)
        RESULT((r < 0) ? -1 : 1);
    if ((r = memcmp(uuid1->obj.node, uuid2->obj.node, sizeof(uuid1->obj.node))) != 0)
        RESULT((r < 0) ? -1 : 1);

    /* default case: the keys are equal */
    *result = 0;

    result_exit:
    return UUID_RC_OK;
}

/* INTERNAL: unpack UUID binary presentation into UUID object
   (allows in-place operation for internal efficiency!) */
static uuid_rc_t uuid_import_bin(uuid_t *uuid, const void *data_ptr, size_t data_len)
{
    const uuid_uint8_t *in;
    uuid_uint32_t tmp32;
    uuid_uint16_t tmp16;
    unsigned int i;

    /* sanity check argument(s) */
    if (uuid == NULL || data_ptr == NULL || data_len < UUID_LEN_BIN)
        return UUID_RC_ARG;

    /* treat input data buffer as octet stream */
    in = (const uuid_uint8_t *)data_ptr;

    /* unpack "time_low" field */
    tmp32 = (uuid_uint32_t)(*in++);
    tmp32 = (tmp32 << 8) | (uuid_uint32_t)(*in++);
    tmp32 = (tmp32 << 8) | (uuid_uint32_t)(*in++);
    tmp32 = (tmp32 << 8) | (uuid_uint32_t)(*in++);
    uuid->obj.time_low = tmp32;

    /* unpack "time_mid" field */
    tmp16 = (uuid_uint16_t)(*in++);
    tmp16 = (uuid_uint16_t)(tmp16 << 8) | (uuid_uint16_t)(*in++);
    uuid->obj.time_mid = tmp16;

    /* unpack "time_hi_and_version" field */
    tmp16 = (uuid_uint16_t)*in++;
    tmp16 = (uuid_uint16_t)(tmp16 << 8) | (uuid_uint16_t)(*in++);
    uuid->obj.time_hi_and_version = tmp16;

    /* unpack "clock_seq_hi_and_reserved" field */
    uuid->obj.clock_seq_hi_and_reserved = *in++;

    /* unpack "clock_seq_low" field */
    uuid->obj.clock_seq_low = *in++;

    /* unpack "node" field */
    for (i = 0; i < (unsigned int)sizeof(uuid->obj.node); i++)
        uuid->obj.node[i] = *in++;

    return UUID_RC_OK;
}

/* INTERNAL: pack UUID object into binary representation
   (allows in-place operation for internal efficiency!) */
static uuid_rc_t uuid_export_bin(const uuid_t *uuid, void *_data_ptr, size_t *data_len)
{
    uuid_uint8_t **data_ptr;
    uuid_uint8_t *out;
    uuid_uint32_t tmp32;
    uuid_uint16_t tmp16;
    unsigned int i;

    /* cast generic data pointer to particular pointer to pointer type */
    data_ptr = (uuid_uint8_t **)_data_ptr;

    /* sanity check argument(s) */
    if (uuid == NULL || data_ptr == NULL)
        return UUID_RC_ARG;

    /* optionally allocate octet data buffer */
    if (*data_ptr == NULL) {
        if ((*data_ptr = (uuid_uint8_t *)malloc(sizeof(uuid_t))) == NULL)
            return UUID_RC_MEM;
        if (data_len != NULL)
            *data_len = UUID_LEN_BIN;
    }
    else {
        if (data_len == NULL)
            return UUID_RC_ARG;
        if (*data_len < UUID_LEN_BIN)
            return UUID_RC_MEM;
        *data_len = UUID_LEN_BIN;
    }

    /* treat output data buffer as octet stream */
    out = *data_ptr;

    /* pack "time_low" field */
    tmp32 = uuid->obj.time_low;
    out[3] = (uuid_uint8_t)(tmp32 & 0xff); tmp32 >>= 8;
    out[2] = (uuid_uint8_t)(tmp32 & 0xff); tmp32 >>= 8;
    out[1] = (uuid_uint8_t)(tmp32 & 0xff); tmp32 >>= 8;
    out[0] = (uuid_uint8_t)(tmp32 & 0xff);

    /* pack "time_mid" field */
    tmp16 = uuid->obj.time_mid;
    out[5] = (uuid_uint8_t)(tmp16 & 0xff); tmp16 >>= 8;
    out[4] = (uuid_uint8_t)(tmp16 & 0xff);

    /* pack "time_hi_and_version" field */
    tmp16 = uuid->obj.time_hi_and_version;
    out[7] = (uuid_uint8_t)(tmp16 & 0xff); tmp16 >>= 8;
    out[6] = (uuid_uint8_t)(tmp16 & 0xff);

    /* pack "clock_seq_hi_and_reserved" field */
    out[8] = uuid->obj.clock_seq_hi_and_reserved;

    /* pack "clock_seq_low" field */
    out[9] = uuid->obj.clock_seq_low;

    /* pack "node" field */
    for (i = 0; i < (unsigned int)sizeof(uuid->obj.node); i++)
        out[10+i] = uuid->obj.node[i];

    return UUID_RC_OK;
}

/* INTERNAL: check for valid UUID string representation syntax */
static int uuid_isstr(const char *str, size_t str_len)
{
    int i;
    const char *cp;

    /* example reference:
       f81d4fae-7dec-11d0-a765-00a0c91e6bf6
       012345678901234567890123456789012345
       0         1         2         3       */
    if (str == NULL)
        return UUID_FALSE;
    if (str_len == 0)
        str_len = strlen(str);
    if (str_len < UUID_LEN_STR)
        return UUID_FALSE;
    for (i = 0, cp = str; i < UUID_LEN_STR; i++, cp++) {
        if ((i == 8) || (i == 13) || (i == 18) || (i == 23)) {
            if (*cp == '-')
                continue;
            else
                return UUID_FALSE;
        }
        if (!isxdigit((int)(*cp)))
            return UUID_FALSE;
    }
    return UUID_TRUE;
}

/* INTERNAL: import UUID object from string representation */
static uuid_rc_t uuid_import_str(uuid_t *uuid, const void *data_ptr, size_t data_len)
{
    uuid_uint16_t tmp16;
    const char *cp;
    char hexbuf[3];
    const char *str;
    unsigned int i;

    /* sanity check argument(s) */
    if (uuid == NULL || data_ptr == NULL || data_len < UUID_LEN_STR)
        return UUID_RC_ARG;

    /* check for correct UUID string representation syntax */
    str = (const char *)data_ptr;
    if (!uuid_isstr(str, 0))
        return UUID_RC_ARG;

    /* parse hex values of "time" parts */
    uuid->obj.time_low            = (uuid_uint32_t)strtoul(str,    NULL, 16);
    uuid->obj.time_mid            = (uuid_uint16_t)strtoul(str+9,  NULL, 16);
    uuid->obj.time_hi_and_version = (uuid_uint16_t)strtoul(str+14, NULL, 16);

    /* parse hex values of "clock" parts */
    tmp16 = (uuid_uint16_t)strtoul(str+19, NULL, 16);
    uuid->obj.clock_seq_low             = (uuid_uint8_t)(tmp16 & 0xff); tmp16 >>= 8;
    uuid->obj.clock_seq_hi_and_reserved = (uuid_uint8_t)(tmp16 & 0xff);

    /* parse hex values of "node" part */
    cp = str+24;
    hexbuf[2] = '\0';
    for (i = 0; i < (unsigned int)sizeof(uuid->obj.node); i++) {
        hexbuf[0] = *cp++;
        hexbuf[1] = *cp++;
        uuid->obj.node[i] = (uuid_uint8_t)strtoul(hexbuf, NULL, 16);
    }

    return UUID_RC_OK;
}

/* INTERNAL: import UUID object from single integer value representation */
static uuid_rc_t uuid_import_siv(uuid_t *uuid, const void *data_ptr, size_t data_len)
{
    const char *str;
    uuid_uint8_t tmp_bin[UUID_LEN_BIN];
    ui128_t ui, ui2;
    uuid_rc_t rc;
    int i;

    /* sanity check argument(s) */
    if (uuid == NULL || data_ptr == NULL || data_len < 1)
        return UUID_RC_ARG;

    /* check for correct UUID single integer value syntax */
    str = (const char *)data_ptr;
    for (i = 0; i < (int)data_len; i++)
        if (!isdigit((int)str[i]))
            return UUID_RC_ARG;

    /* parse single integer value representation (SIV) */
    ui = ui128_s2i(str, NULL, 10);

    /* import octets into UUID binary representation */
    for (i = 0; i < UUID_LEN_BIN; i++) {
        ui = ui128_rol(ui, 8, &ui2);
        tmp_bin[i] = (uuid_uint8_t)(ui128_i2n(ui2) & 0xff);
    }

    /* import into internal UUID representation */
    if ((rc = uuid_import(uuid, UUID_FMT_BIN, (void *)&tmp_bin, UUID_LEN_BIN)) != UUID_RC_OK)
        return rc;

    return UUID_RC_OK;
}

/* INTERNAL: export UUID object to string representation */
static uuid_rc_t uuid_export_str(const uuid_t *uuid, void *_data_ptr, size_t *data_len)
{
    char **data_ptr;
    char *data_buf;

    /* cast generic data pointer to particular pointer to pointer type */
    data_ptr = (char **)_data_ptr;

    /* sanity check argument(s) */
    if (uuid == NULL || data_ptr == NULL)
        return UUID_RC_ARG;

    /* determine output buffer */
    if (*data_ptr == NULL) {
        if ((data_buf = (char *)malloc(UUID_LEN_STR+1)) == NULL)
            return UUID_RC_MEM;
        if (data_len != NULL)
            *data_len = UUID_LEN_STR+1;
    }
    else {
        data_buf = (char *)(*data_ptr);
        if (data_len == NULL)
            return UUID_RC_ARG;
        if (*data_len < UUID_LEN_STR+1)
            return UUID_RC_MEM;
        *data_len = UUID_LEN_STR+1;
    }

    /* format UUID into string representation */
    if (str_snprintf(data_buf, UUID_LEN_STR+1,
        "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        (unsigned long)uuid->obj.time_low,
        (unsigned int)uuid->obj.time_mid,
        (unsigned int)uuid->obj.time_hi_and_version,
        (unsigned int)uuid->obj.clock_seq_hi_and_reserved,
        (unsigned int)uuid->obj.clock_seq_low,
        (unsigned int)uuid->obj.node[0],
        (unsigned int)uuid->obj.node[1],
        (unsigned int)uuid->obj.node[2],
        (unsigned int)uuid->obj.node[3],
        (unsigned int)uuid->obj.node[4],
        (unsigned int)uuid->obj.node[5]) != UUID_LEN_STR) {
        if (*data_ptr == NULL)
            free(data_buf);
        return UUID_RC_INT;
    }

    /* pass back new buffer if locally allocated */
    if (*data_ptr == NULL)
        *data_ptr = data_buf;

    return UUID_RC_OK;
}

/* INTERNAL: export UUID object to single integer value representation */
static uuid_rc_t uuid_export_siv(const uuid_t *uuid, void *_data_ptr, size_t *data_len)
{
    char **data_ptr;
    char *data_buf;
    void *tmp_ptr;
    size_t tmp_len;
    uuid_uint8_t tmp_bin[UUID_LEN_BIN];
    ui128_t ui, ui2;
    uuid_rc_t rc;
    int i;

    /* cast generic data pointer to particular pointer to pointer type */
    data_ptr = (char **)_data_ptr;

    /* sanity check argument(s) */
    if (uuid == NULL || data_ptr == NULL)
        return UUID_RC_ARG;

    /* determine output buffer */
    if (*data_ptr == NULL) {
        if ((data_buf = (char *)malloc(UUID_LEN_SIV+1)) == NULL)
            return UUID_RC_MEM;
        if (data_len != NULL)
            *data_len = UUID_LEN_SIV+1;
    }
    else {
        data_buf = (char *)(*data_ptr);
        if (data_len == NULL)
            return UUID_RC_ARG;
        if (*data_len < UUID_LEN_SIV+1)
            return UUID_RC_MEM;
        *data_len = UUID_LEN_SIV+1;
    }

    /* export into UUID binary representation */
    tmp_ptr = (void *)&tmp_bin;
    tmp_len = sizeof(tmp_bin);
    if ((rc = uuid_export(uuid, UUID_FMT_BIN, &tmp_ptr, &tmp_len)) != UUID_RC_OK) {
        if (*data_ptr == NULL)
            free(data_buf);
        return rc;
    }

    /* import from UUID binary representation */
    ui = ui128_zero();
    for (i = 0; i < UUID_LEN_BIN; i++) {
        ui2 = ui128_n2i((unsigned long)tmp_bin[i]);
        ui = ui128_rol(ui, 8, NULL);
        ui = ui128_or(ui, ui2);
    }

    /* format into single integer value representation */
    (void)ui128_i2s(ui, data_buf, UUID_LEN_SIV+1, 10);

    /* pass back new buffer if locally allocated */
    if (*data_ptr == NULL)
        *data_ptr = data_buf;

    return UUID_RC_OK;
}

/* decoding tables */
static struct {
    uuid_uint8_t num;
    const char *desc;
} uuid_dectab_variant[] = {
    { (uuid_uint8_t)BM_OCTET(0,0,0,0,0,0,0,0), "reserved (NCS backward compatible)" },
    { (uuid_uint8_t)BM_OCTET(1,0,0,0,0,0,0,0), "DCE 1.1, ISO/IEC 11578:1996" },
    { (uuid_uint8_t)BM_OCTET(1,1,0,0,0,0,0,0), "reserved (Microsoft GUID)" },
    { (uuid_uint8_t)BM_OCTET(1,1,1,0,0,0,0,0), "reserved (future use)" }
};
static struct {
    int num;
    const char *desc;
} uuid_dectab_version[] = {
    { 1, "time and node based" },
    { 3, "name based, MD5" },
    { 4, "random data based" },
    { 5, "name based, SHA-1" }
};

/* INTERNAL: dump UUID object as descriptive text */
static uuid_rc_t uuid_export_txt(const uuid_t *uuid, void *_data_ptr, size_t *data_len)
{
    char **data_ptr;
    uuid_rc_t rc;
    char **out;
    char *out_ptr;
    size_t out_len;
    const char *version;
    const char *variant;
    char *content;
    int isnil;
    uuid_uint8_t tmp8;
    uuid_uint16_t tmp16;
    uuid_uint32_t tmp32;
    uuid_uint8_t tmp_bin[UUID_LEN_BIN];
    char tmp_str[UUID_LEN_STR+1];
    char tmp_siv[UUID_LEN_SIV+1];
    void *tmp_ptr;
    size_t tmp_len;
    ui64_t t;
    ui64_t t_offset;
    int t_nsec;
    int t_usec;
    time_t t_sec;
    char t_buf[19+1]; /* YYYY-MM-DD HH:MM:SS */
    struct tm *tm;
    int i;

    /* cast generic data pointer to particular pointer to pointer type */
    data_ptr = (char **)_data_ptr;

    /* sanity check argument(s) */
    if (uuid == NULL || data_ptr == NULL)
        return UUID_RC_ARG;

    /* initialize output buffer */
    out_ptr = NULL;
    out = &out_ptr;

    /* check for special case of "Nil UUID" */
    if ((rc = uuid_isnil(uuid, &isnil)) != UUID_RC_OK)
        return rc;

    /* decode into various representations */
    tmp_ptr = (void *)&tmp_str;
    tmp_len = sizeof(tmp_str);
    if ((rc = uuid_export(uuid, UUID_FMT_STR, &tmp_ptr, &tmp_len)) != UUID_RC_OK)
        return rc;
    tmp_ptr = (void *)&tmp_siv;
    tmp_len = sizeof(tmp_siv);
    if ((rc = uuid_export(uuid, UUID_FMT_SIV, &tmp_ptr, &tmp_len)) != UUID_RC_OK)
        return rc;
    (void)str_rsprintf(out, "encode: STR:     %s\n", tmp_str);
    (void)str_rsprintf(out, "        SIV:     %s\n", tmp_siv);

    /* decode UUID variant */
    tmp8 = uuid->obj.clock_seq_hi_and_reserved;
    if (isnil)
        variant = "n.a.";
    else {
        variant = "unknown";
        for (i = 7; i >= 0; i--) {
            if ((tmp8 & (uuid_uint8_t)BM_BIT(i,1)) == 0) {
                tmp8 &= ~(uuid_uint8_t)BM_MASK(i,0);
                break;
            }
        }
        for (i = 0; i < (int)(sizeof(uuid_dectab_variant)/sizeof(uuid_dectab_variant[0])); i++) {
            if (uuid_dectab_variant[i].num == tmp8) {
                variant = uuid_dectab_variant[i].desc;
                break;
            }
        }
    }
    (void)str_rsprintf(out, "decode: variant: %s\n", variant);

    /* decode UUID version */
    tmp16 = (BM_SHR(uuid->obj.time_hi_and_version, 12) & (uuid_uint16_t)BM_MASK(3,0));
    if (isnil)
        version = "n.a.";
    else {
        version = "unknown";
        for (i = 0; i < (int)(sizeof(uuid_dectab_version)/sizeof(uuid_dectab_version[0])); i++) {
            if (uuid_dectab_version[i].num == (int)tmp16) {
                version = uuid_dectab_version[i].desc;
                break;
            }
        }
    }
    str_rsprintf(out, "        version: %d (%s)\n", (int)tmp16, version);

    /*
     * decode UUID content
     */

    if (tmp8 == BM_OCTET(1,0,0,0,0,0,0,0) && tmp16 == 1) {
        /* decode DCE 1.1 version 1 UUID */

        /* decode system time */
        t = ui64_rol(ui64_n2i((unsigned long)(uuid->obj.time_hi_and_version & BM_MASK(11,0))), 48, NULL),
        t = ui64_or(t, ui64_rol(ui64_n2i((unsigned long)(uuid->obj.time_mid)), 32, NULL));
        t = ui64_or(t, ui64_n2i((unsigned long)(uuid->obj.time_low)));
        t_offset = ui64_s2i(UUID_TIMEOFFSET, NULL, 16);
        t = ui64_sub(t, t_offset, NULL);
        t = ui64_divn(t, 10, &t_nsec);
        t = ui64_divn(t, 1000000, &t_usec);
        t_sec = (time_t)ui64_i2n(t);
        tm = gmtime(&t_sec);
        (void)strftime(t_buf, sizeof(t_buf), "%Y-%m-%d %H:%M:%S", tm);
        (void)str_rsprintf(out, "        content: time:  %s.%06d.%d UTC\n", t_buf, t_usec, t_nsec);

        /* decode clock sequence */
        tmp32 = ((uuid->obj.clock_seq_hi_and_reserved & BM_MASK(5,0)) << 8)
                + uuid->obj.clock_seq_low;
        (void)str_rsprintf(out, "                 clock: %ld (usually random)\n", (long)tmp32);

        /* decode node MAC address */
        (void)str_rsprintf(out, "                 node:  %02x:%02x:%02x:%02x:%02x:%02x (%s %s)\n",
            (unsigned int)uuid->obj.node[0],
            (unsigned int)uuid->obj.node[1],
            (unsigned int)uuid->obj.node[2],
            (unsigned int)uuid->obj.node[3],
            (unsigned int)uuid->obj.node[4],
            (unsigned int)uuid->obj.node[5],
            (uuid->obj.node[0] & IEEE_MAC_LOBIT ? "local" : "global"),
            (uuid->obj.node[0] & IEEE_MAC_MCBIT ? "multicast" : "unicast"));
    }
    else {
        /* decode anything else as hexadecimal byte-string only */

        /* determine annotational hint */
        content = "not decipherable: unknown UUID version";
        if (isnil)
            content = "special case: DCE 1.1 Nil UUID";
        else if (tmp16 == 3)
            content = "not decipherable: MD5 message digest only";
        else if (tmp16 == 4)
            content = "no semantics: random data only";
        else if (tmp16 == 5)
            content = "not decipherable: truncated SHA-1 message digest only";

        /* pack UUID into binary representation */
        tmp_ptr = (void *)&tmp_bin;
        tmp_len = sizeof(tmp_bin);
        if ((rc = uuid_export(uuid, UUID_FMT_BIN, &tmp_ptr, &tmp_len)) != UUID_RC_OK)
            return rc;

        /* mask out version and variant parts */
        tmp_bin[6] &= BM_MASK(3,0);
        tmp_bin[8] &= BM_MASK(5,0);

        /* dump as colon-seperated hexadecimal byte-string */
        (void)str_rsprintf(out,
            "        content: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n"
            "                 (%s)\n",
            (unsigned int)tmp_bin[0],  (unsigned int)tmp_bin[1],  (unsigned int)tmp_bin[2],
            (unsigned int)tmp_bin[3],  (unsigned int)tmp_bin[4],  (unsigned int)tmp_bin[5],
            (unsigned int)tmp_bin[6],  (unsigned int)tmp_bin[7],  (unsigned int)tmp_bin[8],
            (unsigned int)tmp_bin[9],  (unsigned int)tmp_bin[10], (unsigned int)tmp_bin[11],
            (unsigned int)tmp_bin[12], (unsigned int)tmp_bin[13], (unsigned int)tmp_bin[14],
            (unsigned int)tmp_bin[15], content);
    }

    /* provide result */
    out_len = strlen(out_ptr)+1;
    if (*data_ptr == NULL) {
        *data_ptr = (void *)out_ptr;
        if (data_len != NULL)
            *data_len = out_len;
    }
    else {
        if (data_len == NULL)
            return UUID_RC_ARG;
        if (*data_len < out_len)
            return UUID_RC_MEM;
        memcpy(*data_ptr, out_ptr, out_len);
    }

    return UUID_RC_OK;
}

/* UUID importing */
uuid_rc_t uuid_import(uuid_t *uuid, uuid_fmt_t fmt, const void *data_ptr, size_t data_len)
{
    uuid_rc_t rc;

    /* sanity check argument(s) */
    if (uuid == NULL || data_ptr == NULL)
        return UUID_RC_ARG;

    /* dispatch into format-specific functions */
    switch (fmt) {
        case UUID_FMT_BIN: rc = uuid_import_bin(uuid, data_ptr, data_len); break;
        case UUID_FMT_STR: rc = uuid_import_str(uuid, data_ptr, data_len); break;
        case UUID_FMT_SIV: rc = uuid_import_siv(uuid, data_ptr, data_len); break;
        case UUID_FMT_TXT: rc = UUID_RC_IMP; /* not implemented */ break;
        default:           rc = UUID_RC_ARG;
    }

    return rc;
}

/* UUID exporting */
uuid_rc_t uuid_export(const uuid_t *uuid, uuid_fmt_t fmt, void *data_ptr, size_t *data_len)
{
    uuid_rc_t rc;

    /* sanity check argument(s) */
    if (uuid == NULL || data_ptr == NULL)
        return UUID_RC_ARG;

    /* dispatch into format-specific functions */
    switch (fmt) {
        case UUID_FMT_BIN: rc = uuid_export_bin(uuid, data_ptr, data_len); break;
        case UUID_FMT_STR: rc = uuid_export_str(uuid, data_ptr, data_len); break;
        case UUID_FMT_SIV: rc = uuid_export_siv(uuid, data_ptr, data_len); break;
        case UUID_FMT_TXT: rc = uuid_export_txt(uuid, data_ptr, data_len); break;
        default:           rc = UUID_RC_ARG;
    }

    return rc;
}

/* INTERNAL: brand UUID with version and variant */
static void uuid_brand(uuid_t *uuid, unsigned int version)
{
    /* set version (as given) */
    uuid->obj.time_hi_and_version &= BM_MASK(11,0);
    uuid->obj.time_hi_and_version |= (uuid_uint16_t)BM_SHL(version, 12);

    /* set variant (always DCE 1.1 only) */
    uuid->obj.clock_seq_hi_and_reserved &= BM_MASK(5,0);
    uuid->obj.clock_seq_hi_and_reserved |= BM_SHL(0x02, 6);
    return;
}

/* INTERNAL: generate UUID version 1: time, clock and node based */
static uuid_rc_t uuid_make_v1(uuid_t *uuid, unsigned int mode, va_list ap)
{
    struct timeval time_now;
    ui64_t t;
    ui64_t offset;
    ui64_t ov;
    uuid_uint16_t clck;

    /*
     *  GENERATE TIME
     */

    /* determine current system time and sequence counter */
    for (;;) {
        /* determine current system time */
        if (time_gettimeofday(&time_now) == -1)
            return UUID_RC_SYS;

        /* check whether system time changed since last retrieve */
        if (!(   time_now.tv_sec  == uuid->time_last.tv_sec
              && time_now.tv_usec == uuid->time_last.tv_usec)) {
            /* reset time sequence counter and continue */
            uuid->time_seq = 0;
            break;
        }

        /* until we are out of UUIDs per tick, increment
           the time/tick sequence counter and continue */
        if (uuid->time_seq < UUIDS_PER_TICK) {
            uuid->time_seq++;
            break;
        }

        /* stall the UUID generation until the system clock (which
           has a gettimeofday(2) resolution of 1us) catches up */
        time_usleep(1);
    }

    /* convert from timeval (sec,usec) to OSSP ui64 (100*nsec) format */
    t = ui64_n2i((unsigned long)time_now.tv_sec);
    t = ui64_muln(t, 1000000, NULL);
    t = ui64_addn(t, (int)time_now.tv_usec, NULL);
    t = ui64_muln(t, 10, NULL);

    /* adjust for offset between UUID and Unix Epoch time */
    offset = ui64_s2i(UUID_TIMEOFFSET, NULL, 16);
    t = ui64_add(t, offset, NULL);

    /* compensate for low resolution system clock by adding
       the time/tick sequence counter */
    if (uuid->time_seq > 0)
        t = ui64_addn(t, (int)uuid->time_seq, NULL);

    /* store the 60 LSB of the time in the UUID */
    t = ui64_rol(t, 16, &ov);
    uuid->obj.time_hi_and_version =
        (uuid_uint16_t)(ui64_i2n(ov) & 0x00000fff); /* 12 of 16 bit only! */
    t = ui64_rol(t, 16, &ov);
    uuid->obj.time_mid =
        (uuid_uint16_t)(ui64_i2n(ov) & 0x0000ffff); /* all 16 bit */
    t = ui64_rol(t, 32, &ov);
    uuid->obj.time_low =
        (uuid_uint32_t)(ui64_i2n(ov) & 0xffffffff); /* all 32 bit */

    /*
     *  GENERATE CLOCK
     */

    /* retrieve current clock sequence */
    clck = ((uuid->obj.clock_seq_hi_and_reserved & BM_MASK(5,0)) << 8)
           + uuid->obj.clock_seq_low;

    /* generate new random clock sequence (initially or if the
       time has stepped backwards) or else just increase it */
    if (   clck == 0
        || (   time_now.tv_sec < uuid->time_last.tv_sec
            || (   time_now.tv_sec == uuid->time_last.tv_sec
                && time_now.tv_usec < uuid->time_last.tv_usec))) {
        if (prng_data(uuid->prng, (void *)&clck, sizeof(clck)) != PRNG_RC_OK)
            return UUID_RC_INT;
    }
    else
        clck++;
    clck %= BM_POW2(14);

    /* store back new clock sequence */
    uuid->obj.clock_seq_hi_and_reserved =
        (uuid->obj.clock_seq_hi_and_reserved & BM_MASK(7,6))
        | (uuid_uint8_t)((clck >> 8) & 0xff);
    uuid->obj.clock_seq_low =
        (uuid_uint8_t)(clck & 0xff);

    /*
     *  GENERATE NODE
     */

    if ((mode & UUID_MAKE_MC) || (uuid->mac[0] & BM_OCTET(1,0,0,0,0,0,0,0))) {
        /* generate random IEEE 802 local multicast MAC address */
        if (prng_data(uuid->prng, (void *)&(uuid->obj.node), sizeof(uuid->obj.node)) != PRNG_RC_OK)
            return UUID_RC_INT;
        uuid->obj.node[0] |= IEEE_MAC_MCBIT;
        uuid->obj.node[0] |= IEEE_MAC_LOBIT;
    }
    else {
        /* use real regular MAC address */
        memcpy(uuid->obj.node, uuid->mac, sizeof(uuid->mac));
    }

    /*
     *  FINISH
     */

    /* remember current system time for next iteration */
    uuid->time_last.tv_sec  = time_now.tv_sec;
    uuid->time_last.tv_usec = time_now.tv_usec;

    /* brand with version and variant */
    uuid_brand(uuid, 1);

    return UUID_RC_OK;
}

/* INTERNAL: pre-defined UUID values.
   (defined as network byte ordered octet stream) */
#define UUID_MAKE(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16) \
    { (uuid_uint8_t)(a1),  (uuid_uint8_t)(a2),  (uuid_uint8_t)(a3),  (uuid_uint8_t)(a4),  \
      (uuid_uint8_t)(a5),  (uuid_uint8_t)(a6),  (uuid_uint8_t)(a7),  (uuid_uint8_t)(a8),  \
      (uuid_uint8_t)(a9),  (uuid_uint8_t)(a10), (uuid_uint8_t)(a11), (uuid_uint8_t)(a12), \
      (uuid_uint8_t)(a13), (uuid_uint8_t)(a14), (uuid_uint8_t)(a15), (uuid_uint8_t)(a16) }
static struct {
    char *name;
    uuid_uint8_t uuid[UUID_LEN_BIN];
} uuid_value_table[] = {
    { "nil",     /* 00000000-0000-0000-0000-000000000000 ("Nil UUID") */
      UUID_MAKE(0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00) },
    { "ns:DNS",  /* 6ba7b810-9dad-11d1-80b4-00c04fd430c8 (see RFC 4122) */
      UUID_MAKE(0x6b,0xa7,0xb8,0x10,0x9d,0xad,0x11,0xd1,0x80,0xb4,0x00,0xc0,0x4f,0xd4,0x30,0xc8) },
    { "ns:URL",  /* 6ba7b811-9dad-11d1-80b4-00c04fd430c8 (see RFC 4122) */
      UUID_MAKE(0x6b,0xa7,0xb8,0x11,0x9d,0xad,0x11,0xd1,0x80,0xb4,0x00,0xc0,0x4f,0xd4,0x30,0xc8) },
    { "ns:OID",  /* 6ba7b812-9dad-11d1-80b4-00c04fd430c8 (see RFC 4122) */
      UUID_MAKE(0x6b,0xa7,0xb8,0x12,0x9d,0xad,0x11,0xd1,0x80,0xb4,0x00,0xc0,0x4f,0xd4,0x30,0xc8) },
    { "ns:X500", /* 6ba7b814-9dad-11d1-80b4-00c04fd430c8 (see RFC 4122) */
      UUID_MAKE(0x6b,0xa7,0xb8,0x14,0x9d,0xad,0x11,0xd1,0x80,0xb4,0x00,0xc0,0x4f,0xd4,0x30,0xc8) }
};

/* load UUID object with pre-defined value */
uuid_rc_t uuid_load(uuid_t *uuid, const char *name)
{
    uuid_uint8_t *uuid_octets;
    uuid_rc_t rc;
    unsigned int i;

    /* sanity check argument(s) */
    if (uuid == NULL || name == NULL)
        return UUID_RC_ARG;

    /* search for UUID in table */
    uuid_octets = NULL;
    for (i = 0; i < (unsigned int)sizeof(uuid_value_table)/sizeof(uuid_value_table[0]); i++) {
         if (strcmp(uuid_value_table[i].name, name) == 0) {
             uuid_octets = uuid_value_table[i].uuid;
             break;
         }
    }
    if (uuid_octets == NULL)
        return UUID_RC_ARG;

    /* import value into UUID object */
    if ((rc = uuid_import(uuid, UUID_FMT_BIN, uuid_octets, UUID_LEN_BIN)) != UUID_RC_OK)
        return rc;

    return UUID_RC_OK;
}

/* INTERNAL: generate UUID version 3: name based with MD5 */
static uuid_rc_t uuid_make_v3(uuid_t *uuid, unsigned int mode, va_list ap)
{
    char *str;
    uuid_t *uuid_ns;
    uuid_uint8_t uuid_buf[UUID_LEN_BIN];
    void *uuid_ptr;
    size_t uuid_len;
    uuid_rc_t rc;

    /* determine namespace UUID and name string arguments */
    if ((uuid_ns = (uuid_t *)va_arg(ap, void *)) == NULL)
        return UUID_RC_ARG;
    if ((str = (char *)va_arg(ap, char *)) == NULL)
        return UUID_RC_ARG;

    /* initialize MD5 context */
    if (md5_init(uuid->md5) != MD5_RC_OK)
        return UUID_RC_MEM;

    /* load the namespace UUID into MD5 context */
    uuid_ptr = (void *)&uuid_buf;
    uuid_len = sizeof(uuid_buf);
    if ((rc = uuid_export(uuid_ns, UUID_FMT_BIN, &uuid_ptr, &uuid_len)) != UUID_RC_OK)
        return rc;
    if (md5_update(uuid->md5, uuid_buf, uuid_len) != MD5_RC_OK)
        return UUID_RC_INT;

    /* load the argument name string into MD5 context */
    if (md5_update(uuid->md5, str, strlen(str)) != MD5_RC_OK)
        return UUID_RC_INT;

    /* store MD5 result into UUID
       (requires MD5_LEN_BIN space, UUID_LEN_BIN space is available,
       and both are equal in size, so we are safe!) */
    uuid_ptr = (void *)&(uuid->obj);
    if (md5_store(uuid->md5, &uuid_ptr, NULL) != MD5_RC_OK)
        return UUID_RC_INT;

    /* fulfill requirement of standard and convert UUID data into
       local/host byte order (this uses fact that uuid_import_bin() is
       able to operate in-place!) */
    if ((rc = uuid_import(uuid, UUID_FMT_BIN, (void *)&(uuid->obj), UUID_LEN_BIN)) != UUID_RC_OK)
        return rc;

    /* brand UUID with version and variant */
    uuid_brand(uuid, 3);

    return UUID_RC_OK;
}

/* INTERNAL: generate UUID version 4: random number based */
static uuid_rc_t uuid_make_v4(uuid_t *uuid, unsigned int mode, va_list ap)
{
    /* fill UUID with random data */
    if (prng_data(uuid->prng, (void *)&(uuid->obj), sizeof(uuid->obj)) != PRNG_RC_OK)
        return UUID_RC_INT;

    /* brand UUID with version and variant */
    uuid_brand(uuid, 4);

    return UUID_RC_OK;
}

/* INTERNAL: generate UUID version 5: name based with SHA-1 */
static uuid_rc_t uuid_make_v5(uuid_t *uuid, unsigned int mode, va_list ap)
{
    char *str;
    uuid_t *uuid_ns;
    uuid_uint8_t uuid_buf[UUID_LEN_BIN];
    void *uuid_ptr;
    size_t uuid_len;
    uuid_uint8_t sha1_buf[SHA1_LEN_BIN];
    void *sha1_ptr;
    uuid_rc_t rc;

    /* determine namespace UUID and name string arguments */
    if ((uuid_ns = (uuid_t *)va_arg(ap, void *)) == NULL)
        return UUID_RC_ARG;
    if ((str = (char *)va_arg(ap, char *)) == NULL)
        return UUID_RC_ARG;

    /* initialize SHA-1 context */
    if (sha1_init(uuid->sha1) != SHA1_RC_OK)
        return UUID_RC_INT;

    /* load the namespace UUID into SHA-1 context */
    uuid_ptr = (void *)&uuid_buf;
    uuid_len = sizeof(uuid_buf);
    if ((rc = uuid_export(uuid_ns, UUID_FMT_BIN, &uuid_ptr, &uuid_len)) != UUID_RC_OK)
        return rc;
    if (sha1_update(uuid->sha1, uuid_buf, uuid_len) != SHA1_RC_OK)
        return UUID_RC_INT;

    /* load the argument name string into SHA-1 context */
    if (sha1_update(uuid->sha1, str, strlen(str)) != SHA1_RC_OK)
        return UUID_RC_INT;

    /* store SHA-1 result into UUID
       (requires SHA1_LEN_BIN space, but UUID_LEN_BIN space is available
       only, so use a temporary buffer to store SHA-1 results and then
       use lower part only according to standard */
    sha1_ptr = (void *)sha1_buf;
    if (sha1_store(uuid->sha1, &sha1_ptr, NULL) != SHA1_RC_OK)
        return UUID_RC_INT;
    uuid_ptr = (void *)&(uuid->obj);
    memcpy(uuid_ptr, sha1_ptr, UUID_LEN_BIN);

    /* fulfill requirement of standard and convert UUID data into
       local/host byte order (this uses fact that uuid_import_bin() is
       able to operate in-place!) */
    if ((rc = uuid_import(uuid, UUID_FMT_BIN, (void *)&(uuid->obj), UUID_LEN_BIN)) != UUID_RC_OK)
        return rc;

    /* brand UUID with version and variant */
    uuid_brand(uuid, 5);

    return UUID_RC_OK;
}

/* generate UUID */
uuid_rc_t uuid_make(uuid_t *uuid, unsigned int mode, ...)
{
    va_list ap;
    uuid_rc_t rc;

    /* sanity check argument(s) */
    if (uuid == NULL)
        return UUID_RC_ARG;

    /* dispatch into version dependent generation functions */
    va_start(ap, mode);
    if (mode & UUID_MAKE_V1)
        rc = uuid_make_v1(uuid, mode, ap);
    else if (mode & UUID_MAKE_V3)
        rc = uuid_make_v3(uuid, mode, ap);
    else if (mode & UUID_MAKE_V4)
        rc = uuid_make_v4(uuid, mode, ap);
    else if (mode & UUID_MAKE_V5)
        rc = uuid_make_v5(uuid, mode, ap);
    else
        rc = UUID_RC_ARG;
    va_end(ap);

    return rc;
}

/* translate UUID API error code into corresponding error string */
char *uuid_error(uuid_rc_t rc)
{
    char *str;

    switch (rc) {
        case UUID_RC_OK:  str = "everything ok";    break;
        case UUID_RC_ARG: str = "invalid argument"; break;
        case UUID_RC_MEM: str = "out of memory";    break;
        case UUID_RC_SYS: str = "system error";     break;
        case UUID_RC_INT: str = "internal error";   break;
        case UUID_RC_IMP: str = "not implemented";  break;
        default:          str = NULL;               break;
    }
    return str;
}

/* OSSP uuid version (link-time information) */
unsigned long uuid_version(void)
{
    return (unsigned long)(_UUID_VERSION);
}

