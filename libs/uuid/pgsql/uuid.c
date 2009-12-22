/*
**  OSSP uuid - Universally Unique Identifier
**  Copyright (c) 2004-2007 Ralf S. Engelschall <rse@engelschall.com>
**  Copyright (c) 2004-2007 The OSSP Project <http://www.ossp.org/>
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
**  uuid.c: PostgreSQL Binding (C part)
*/

/*  own headers */
#include "uuid.h"

/*  PostgreSQL (part 1/2) headers */
#include "postgres.h"

/*  system headers */
#include <string.h>

/*  PostgreSQL (part 2/2) headers */
#include "fmgr.h"
#include "lib/stringinfo.h"
#include "access/hash.h"

/*  PostgreSQL module magic cookie
    (PostgreSQL >= 8.2 only) */
#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

/* internal UUID datum data structure */
typedef struct {
    unsigned char uuid_bin[UUID_LEN_BIN];
} uuid_datum_t;

/* forward declarations */
Datum pg_uuid_in     (PG_FUNCTION_ARGS);
Datum pg_uuid_out    (PG_FUNCTION_ARGS);
Datum pg_uuid_recv   (PG_FUNCTION_ARGS);
Datum pg_uuid_send   (PG_FUNCTION_ARGS);
Datum pg_uuid_hash   (PG_FUNCTION_ARGS);
Datum pg_uuid_make   (PG_FUNCTION_ARGS);
Datum pg_uuid_eq     (PG_FUNCTION_ARGS);
Datum pg_uuid_ne     (PG_FUNCTION_ARGS);
Datum pg_uuid_lt     (PG_FUNCTION_ARGS);
Datum pg_uuid_gt     (PG_FUNCTION_ARGS);
Datum pg_uuid_le     (PG_FUNCTION_ARGS);
Datum pg_uuid_ge     (PG_FUNCTION_ARGS);
Datum pg_uuid_cmp    (PG_FUNCTION_ARGS);

/* API function: uuid_in */
PG_FUNCTION_INFO_V1(pg_uuid_in);
Datum pg_uuid_in(PG_FUNCTION_ARGS)
{
    char *uuid_str;
    uuid_datum_t *uuid_datum;
    uuid_rc_t rc;
    uuid_t *uuid;
    void *vp;
    size_t len;

    /* sanity check input argument */
    if ((uuid_str = PG_GETARG_CSTRING(0)) == NULL)
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("invalid UUID string")));
    if ((len = strlen(uuid_str)) != UUID_LEN_STR)
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("invalid UUID string length %d (expected %d)", (int)len, UUID_LEN_STR)));

    /* import as string representation */
    if ((rc = uuid_create(&uuid)) != UUID_RC_OK)
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("failed to create UUID object: %s", uuid_error(rc))));
    if ((rc = uuid_import(uuid, UUID_FMT_STR, uuid_str, len)) != UUID_RC_OK) {
        uuid_destroy(uuid);
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("failed to import UUID string representation: %s", uuid_error(rc))));
    }

    /* export as binary representation */
    if ((uuid_datum = (uuid_datum_t *)palloc(sizeof(uuid_datum_t))) == NULL) {
        uuid_destroy(uuid);
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("failed to allocate UUID datum")));
    }
    vp = &(uuid_datum->uuid_bin);
    len = sizeof(uuid_datum->uuid_bin);
    if ((rc = uuid_export(uuid, UUID_FMT_BIN, &vp, &len)) != UUID_RC_OK) {
        uuid_destroy(uuid);
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("failed to export UUID binary representation: %s", uuid_error(rc))));
    }
    uuid_destroy(uuid);

    /* return UUID datum */
    PG_RETURN_POINTER(uuid_datum);
}

/* API function: uuid_out */
PG_FUNCTION_INFO_V1(pg_uuid_out);
Datum pg_uuid_out(PG_FUNCTION_ARGS)
{
    uuid_datum_t *uuid_datum;
    uuid_rc_t rc;
    uuid_t *uuid;
    char *uuid_str;
    void *vp;
    size_t len;

    /* sanity check input argument */
    if ((uuid_datum = (uuid_datum_t *)PG_GETARG_POINTER(0)) == NULL)
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("invalid UUID datum")));

    /* import as binary representation */
    if ((rc = uuid_create(&uuid)) != UUID_RC_OK)
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("failed to create UUID object: %s", uuid_error(rc))));
    if ((rc = uuid_import(uuid, UUID_FMT_BIN, uuid_datum->uuid_bin, sizeof(uuid_datum->uuid_bin))) != UUID_RC_OK) {
        uuid_destroy(uuid);
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("failed to import UUID binary representation: %s", uuid_error(rc))));
    }

    /* export as string representation */
    len = UUID_LEN_STR+1;
    if ((vp = uuid_str = (char *)palloc(len)) == NULL) {
        uuid_destroy(uuid);
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("failed to allocate UUID string")));
    }
    if ((rc = uuid_export(uuid, UUID_FMT_STR, &vp, &len)) != UUID_RC_OK) {
        uuid_destroy(uuid);
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("failed to export UUID string representation: %s", uuid_error(rc))));
    }
    uuid_destroy(uuid);

    /* return UUID string */
    PG_RETURN_CSTRING(uuid_str);
}

/* API function: uuid_recv */
PG_FUNCTION_INFO_V1(pg_uuid_recv);
Datum pg_uuid_recv(PG_FUNCTION_ARGS)
{
    StringInfo uuid_internal;
    uuid_datum_t *uuid_datum;

    /* sanity check input argument */
    if ((uuid_internal = (StringInfo)PG_GETARG_POINTER(0)) == NULL)
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("invalid UUID StringInfo object")));
    if (uuid_internal->len != UUID_LEN_BIN)
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("invalid UUID binary length %d (expected %d)", uuid_internal->len, UUID_LEN_BIN)));

    /* import as binary representation */
    if ((uuid_datum = (uuid_datum_t *)palloc(sizeof(uuid_datum_t))) == NULL)
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("failed to allocate UUID datum")));
    memcpy(uuid_datum->uuid_bin, uuid_internal->data, uuid_internal->len);

    /* return UUID datum */
    PG_RETURN_POINTER(uuid_datum);
}

/* API function: uuid_send */
PG_FUNCTION_INFO_V1(pg_uuid_send);
Datum pg_uuid_send(PG_FUNCTION_ARGS)
{
    uuid_datum_t *uuid_datum;
    bytea *uuid_bytea;

    /* sanity check input argument */
    if ((uuid_datum = (uuid_datum_t *)PG_GETARG_POINTER(0)) == NULL)
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("invalid UUID datum")));

    /* export as binary representation */
    if ((uuid_bytea = (bytea *)palloc(VARHDRSZ + UUID_LEN_BIN)) == NULL)
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("failed to allocate UUID bytea")));
#if defined(SET_VARSIZE) /* PostgreSQL >= 8.3 */
    SET_VARSIZE(uuid_bytea, VARHDRSZ + UUID_LEN_BIN);
#else
    uuid_bytea->vl_len = VARHDRSZ + UUID_LEN_BIN;
#endif
    memcpy(uuid_bytea->vl_dat, uuid_datum->uuid_bin, UUID_LEN_BIN);

    /* return UUID bytea */
    PG_RETURN_BYTEA_P(uuid_bytea);
}

/* API function: uuid_make */
PG_FUNCTION_INFO_V1(pg_uuid_make);
Datum pg_uuid_make(PG_FUNCTION_ARGS)
{
    uuid_t *uuid;
    uuid_t *uuid_ns;
    uuid_rc_t rc;
    int version;
    unsigned int mode = 0;
    uuid_datum_t *uuid_datum;
    char *str_ns;
    char *str_name;
    void *vp;
    size_t len;

    /* sanity check input argument */
    version = (int)PG_GETARG_INT32(0);
    switch (version) {
        case 1: mode = UUID_MAKE_V1; break;
        case 3: mode = UUID_MAKE_V3; break;
        case 4: mode = UUID_MAKE_V4; break;
        case 5: mode = UUID_MAKE_V5; break;
        default:
            ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                    errmsg("invalid UUID version %d (expected 1, 3, 4 or 5)", version)));
    }
    if (   ((mode & (UUID_MAKE_V1|UUID_MAKE_V4)) && PG_NARGS() != 1)
        || ((mode & (UUID_MAKE_V3|UUID_MAKE_V5)) && PG_NARGS() != 3))
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("invalid number (%d) of arguments", PG_NARGS())));

    /* make a new UUID */
    if ((rc = uuid_create(&uuid)) != UUID_RC_OK)
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("failed to create UUID object: %s", uuid_error(rc))));
    if (version == 3 || version == 5) {
        if ((str_ns = PG_GETARG_CSTRING(1)) == NULL)
            ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                    errmsg("invalid namespace UUID string")));
        if ((str_name = PG_GETARG_CSTRING(2)) == NULL)
            ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                    errmsg("invalid name string")));
        if ((rc = uuid_create(&uuid_ns)) != UUID_RC_OK)
            ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                    errmsg("failed to create UUID namespace object: %s", uuid_error(rc))));
        if ((rc = uuid_load(uuid_ns, str_ns)) != UUID_RC_OK) {
            if ((rc = uuid_import(uuid_ns, UUID_FMT_STR, str_ns, strlen(str_ns))) != UUID_RC_OK)
                ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                        errmsg("failed to import UUID namespace: %s", uuid_error(rc))));
        }
        if ((rc = uuid_make(uuid, mode, uuid_ns, str_name)) != UUID_RC_OK) {
            uuid_destroy(uuid);
            ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                    errmsg("failed to make v%d UUID: %s", version, uuid_error(rc))));
        }
        uuid_destroy(uuid_ns);
    }
    else {
        if ((rc = uuid_make(uuid, mode)) != UUID_RC_OK) {
            uuid_destroy(uuid);
            ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                    errmsg("failed to make v%d UUID: %s", version, uuid_error(rc))));
        }
    }

    /* export as binary representation */
    if ((uuid_datum = (uuid_datum_t *)palloc(sizeof(uuid_datum_t))) == NULL) {
        uuid_destroy(uuid);
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("failed to allocate UUID datum")));
    }
    vp = &(uuid_datum->uuid_bin);
    len = sizeof(uuid_datum->uuid_bin);
    if ((rc = uuid_export(uuid, UUID_FMT_BIN, &vp, &len)) != UUID_RC_OK) {
        uuid_destroy(uuid);
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("failed to export UUID binary representation: %s", uuid_error(rc))));
    }
    uuid_destroy(uuid);
    PG_RETURN_POINTER(uuid_datum);
}

/* API function: uuid_hash */
PG_FUNCTION_INFO_V1(pg_uuid_hash);
Datum pg_uuid_hash(PG_FUNCTION_ARGS)
{
    uuid_datum_t *uuid_datum;

    /* sanity check input argument */
    if ((uuid_datum = (uuid_datum_t *)PG_GETARG_POINTER(0)) == NULL)
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("invalid UUID datum argument")));

    /* return hash value of the UUID */
    PG_RETURN_INT32(hash_any(uuid_datum->uuid_bin, sizeof(uuid_datum->uuid_bin)));
}

/* INTERNAL function: _uuid_cmp */
static int _uuid_cmp(PG_FUNCTION_ARGS)
{
    uuid_datum_t *uuid_datum1;
    uuid_datum_t *uuid_datum2;
    uuid_t *uuid1;
    uuid_t *uuid2;
    uuid_rc_t rc;
    int result;

    /* sanity check input argument */
    if ((uuid_datum1 = (uuid_datum_t *)PG_GETARG_POINTER(0)) == NULL)
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("invalid first UUID datum argument")));
    if ((uuid_datum2 = (uuid_datum_t *)PG_GETARG_POINTER(1)) == NULL)
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("invalid second UUID datum argument")));

    /* load both UUIDs */
    if ((rc = uuid_create(&uuid1)) != UUID_RC_OK)
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("failed to create UUID object: %s", uuid_error(rc))));
    if ((rc = uuid_create(&uuid2)) != UUID_RC_OK) {
        uuid_destroy(uuid1);
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("failed to create UUID object: %s", uuid_error(rc))));
    }
    if ((rc = uuid_import(uuid1, UUID_FMT_BIN, uuid_datum1->uuid_bin, sizeof(uuid_datum1->uuid_bin))) != UUID_RC_OK) {
        uuid_destroy(uuid1);
        uuid_destroy(uuid2);
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("failed to import UUID: %s", uuid_error(rc))));
    }
    if ((rc = uuid_import(uuid2, UUID_FMT_BIN, uuid_datum2->uuid_bin, sizeof(uuid_datum2->uuid_bin))) != UUID_RC_OK) {
        uuid_destroy(uuid1);
        uuid_destroy(uuid2);
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("failed to import UUID: %s", uuid_error(rc))));
    }

    /* compare UUIDs */
    if ((rc = uuid_compare(uuid1, uuid2, &result)) != UUID_RC_OK) {
        uuid_destroy(uuid1);
        uuid_destroy(uuid2);
        ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                errmsg("failed to compare UUID objects: %s", uuid_error(rc))));
    }

    /* cleanup */
    uuid_destroy(uuid1);
    uuid_destroy(uuid2);

    /* return result */
    return result;
}

/* API function: uuid_eq */
PG_FUNCTION_INFO_V1(pg_uuid_eq);
Datum pg_uuid_eq(PG_FUNCTION_ARGS)
{
    int rc;

    rc = _uuid_cmp(fcinfo);
    PG_RETURN_BOOL(rc == 0);
}

/* API function: uuid_ne */
PG_FUNCTION_INFO_V1(pg_uuid_ne);
Datum pg_uuid_ne(PG_FUNCTION_ARGS)
{
    int rc;

    rc = _uuid_cmp(fcinfo);
    PG_RETURN_BOOL(rc != 0);
}

/* API function: uuid_lt */
PG_FUNCTION_INFO_V1(pg_uuid_lt);
Datum pg_uuid_lt(PG_FUNCTION_ARGS)
{
    int rc;

    rc = _uuid_cmp(fcinfo);
    PG_RETURN_BOOL(rc == -1);
}

/* API function: uuid_gt */
PG_FUNCTION_INFO_V1(pg_uuid_gt);
Datum pg_uuid_gt(PG_FUNCTION_ARGS)
{
    int rc;

    rc = _uuid_cmp(fcinfo);
    PG_RETURN_BOOL(rc == 1);
}

/* API function: uuid_le */
PG_FUNCTION_INFO_V1(pg_uuid_le);
Datum pg_uuid_le(PG_FUNCTION_ARGS)
{
    int rc;

    rc = _uuid_cmp(fcinfo);
    PG_RETURN_BOOL(rc < 1);
}

/* API function: uuid_ge */
PG_FUNCTION_INFO_V1(pg_uuid_ge);
Datum pg_uuid_ge(PG_FUNCTION_ARGS)
{
    int rc;

    rc = _uuid_cmp(fcinfo);
    PG_RETURN_BOOL(rc > -1);
}

/* API function: uuid_cmp */
PG_FUNCTION_INFO_V1(pg_uuid_cmp);
Datum pg_uuid_cmp(PG_FUNCTION_ARGS)
{
    int rc;

    rc = _uuid_cmp(fcinfo);
    PG_RETURN_INT32(rc);
}

