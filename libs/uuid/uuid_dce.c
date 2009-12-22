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
**  uuid_dce.c: DCE 1.1 compatibility API implementation
*/

/* include DCE 1.1 API */
#define uuid_t uuid_dce_t
#include "uuid_dce.h"
#undef uuid_t
#undef uuid_create
#undef uuid_create_nil
#undef uuid_is_nil
#undef uuid_compare
#undef uuid_equal
#undef uuid_from_string
#undef uuid_to_string
#undef uuid_hash

/* include regular API */
#include "uuid.h"

/* helper macro */
#define LEAVE /*lint -save -e801*/ goto leave /*lint -restore*/

/* create a UUID (v1 only) */
void uuid_dce_create(uuid_dce_t *uuid_dce, int *status)
{
    uuid_t *uuid;
    size_t len;
    void *vp;

    /* initialize status */
    if (status != NULL)
        *status = uuid_s_error;

    /* sanity check argument(s) */
    if (uuid_dce == NULL)
        return;

    /* create UUID and export as binary representation */
    if (uuid_create(&uuid) != UUID_RC_OK)
        return;
    if (uuid_make(uuid, UUID_MAKE_V1) != UUID_RC_OK) {
        uuid_destroy(uuid);
        return;
    }
    vp  = uuid_dce;
    len = UUID_LEN_BIN;
    if (uuid_export(uuid, UUID_FMT_BIN, &vp, &len) != UUID_RC_OK) {
        uuid_destroy(uuid);
        return;
    }
    uuid_destroy(uuid);

    /* return successfully */
    if (status != NULL)
        *status = uuid_s_ok;
    return;
}

/* create a Nil UUID */
void uuid_dce_create_nil(uuid_dce_t *uuid_dce, int *status)
{
    /* initialize status */
    if (status != NULL)
        *status = uuid_s_error;

    /* sanity check argument(s) */
    if (uuid_dce == NULL)
        return;

    /* short-circuit implementation, because Nil UUID is trivial to
       create, so no need to use regular OSSP uuid API */
    memset(uuid_dce, 0, UUID_LEN_BIN);

    /* return successfully */
    if (status != NULL)
        *status = uuid_s_ok;
    return;
}

/* check whether it is Nil UUID */
int uuid_dce_is_nil(uuid_dce_t *uuid_dce, int *status)
{
    int i;
    int result;
    unsigned char *ucp;

    /* initialize status */
    if (status != NULL)
        *status = uuid_s_error;

    /* sanity check argument(s) */
    if (uuid_dce == NULL)
        return 0;

    /* short-circuit implementation, because Nil UUID is trivial to
       check, so no need to use regular OSSP uuid API */
    result = 1;
    ucp = (unsigned char *)uuid_dce;
    for (i = 0; i < UUID_LEN_BIN; i++) {
        if (ucp[i] != '\0') {
            result = 0;
            break;
        }
    }

    /* return successfully with result */
    if (status != NULL)
        *status = uuid_s_ok;
    return result;
}

/* compare two UUIDs */
int uuid_dce_compare(uuid_dce_t *uuid_dce1, uuid_dce_t *uuid_dce2, int *status)
{
    uuid_t *uuid1 = NULL;
    uuid_t *uuid2 = NULL;
    int result = 0;

    /* initialize status */
    if (status != NULL)
        *status = uuid_s_error;

    /* sanity check argument(s) */
    if (uuid_dce1 == NULL || uuid_dce2 == NULL)
        return 0;

    /* import both UUID binary representations and compare them */
    if (uuid_create(&uuid1) != UUID_RC_OK)
        LEAVE;
    if (uuid_create(&uuid2) != UUID_RC_OK)
        LEAVE;
    if (uuid_import(uuid1, UUID_FMT_BIN, uuid_dce1, UUID_LEN_BIN) != UUID_RC_OK)
        LEAVE;
    if (uuid_import(uuid2, UUID_FMT_BIN, uuid_dce2, UUID_LEN_BIN) != UUID_RC_OK)
        LEAVE;
    if (uuid_compare(uuid1, uuid2, &result) != UUID_RC_OK)
        LEAVE;

    /* indicate successful operation */
    if (status != NULL)
        *status = uuid_s_ok;

    /* cleanup and return */
    leave:
    if (uuid1 != NULL)
        uuid_destroy(uuid1);
    if (uuid2 != NULL)
        uuid_destroy(uuid2);
    return result;
}

/* compare two UUIDs (equality only) */
int uuid_dce_equal(uuid_dce_t *uuid_dce1, uuid_dce_t *uuid_dce2, int *status)
{
    /* initialize status */
    if (status != NULL)
        *status = uuid_s_error;

    /* sanity check argument(s) */
    if (uuid_dce1 == NULL || uuid_dce2 == NULL)
        return 0;

    /* pass through to generic compare function */
    return (uuid_dce_compare(uuid_dce1, uuid_dce2, status) == 0 ? 1 : 0);
}

/* import UUID from string representation */
void uuid_dce_from_string(const char *str, uuid_dce_t *uuid_dce, int *status)
{
    uuid_t *uuid = NULL;
    size_t len;
    void *vp;

    /* initialize status */
    if (status != NULL)
        *status = uuid_s_error;

    /* sanity check argument(s) */
    if (str == NULL || uuid_dce == NULL)
        return;

    /* import string representation and export binary representation */
    if (uuid_create(&uuid) != UUID_RC_OK)
        LEAVE;
    if (uuid_import(uuid, UUID_FMT_STR, str, UUID_LEN_STR) != UUID_RC_OK)
        LEAVE;
    vp  = uuid_dce;
    len = UUID_LEN_BIN;
    if (uuid_export(uuid, UUID_FMT_BIN, &vp, &len) != UUID_RC_OK)
        LEAVE;

    /* indicate successful operation */
    if (status != NULL)
        *status = uuid_s_ok;

    /* cleanup and return */
    leave:
    if (uuid != NULL)
        uuid_destroy(uuid);
    return;
}

/* export UUID to string representation */
void uuid_dce_to_string(uuid_dce_t *uuid_dce, char **str, int *status)
{
    uuid_t *uuid = NULL;
    size_t len;
    void *vp;

    /* initialize status */
    if (status != NULL)
        *status = uuid_s_error;

    /* sanity check argument(s) */
    if (str == NULL || uuid_dce == NULL)
        return;

    /* import binary representation and export string representation */
    if (uuid_create(&uuid) != UUID_RC_OK)
        LEAVE;
    if (uuid_import(uuid, UUID_FMT_BIN, uuid_dce, UUID_LEN_BIN) != UUID_RC_OK)
        LEAVE;
    vp  = str;
    len = UUID_LEN_STR;
    if (uuid_export(uuid, UUID_FMT_STR, &vp, &len) != UUID_RC_OK)
        LEAVE;

    /* indicate successful operation */
    if (status != NULL)
        *status = uuid_s_ok;

    /* cleanup and return */
    leave:
    if (uuid != NULL)
        uuid_destroy(uuid);
    return;
}

/* export UUID into hash value */
unsigned int uuid_dce_hash(uuid_dce_t *uuid_dce, int *status)
{
    int i;
    unsigned char *ucp;
    unsigned int hash;

    /* initialize status */
    if (status != NULL)
        *status = uuid_s_error;

    /* sanity check argument(s) */
    if (uuid_dce == NULL)
        return 0;

    /* generate a hash value
       (DCE 1.1 actually requires 16-bit only) */
    hash = 0;
    ucp = (unsigned char *)uuid_dce;
    for (i = UUID_LEN_BIN-1; i >= 0; i--) {
        hash <<= 8;
        hash |= ucp[i];
    }

    /* return successfully */
    if (status != NULL)
        *status = uuid_s_ok;
    return hash;
}

