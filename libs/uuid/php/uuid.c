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
**  uuid.c: PHP/Zend API (language: C)
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "uuid.h"
#include "php.h"
#include "ext/standard/info.h"

/* context structure */
typedef struct {
    uuid_t *uuid;
} ctx_t;

/* context implicit destruction */
static void ctx_destructor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    ctx_t *ctx = (ctx_t *)rsrc->ptr;

    if (ctx != NULL) {
        if (ctx->uuid != NULL) {
            uuid_destroy(ctx->uuid);
            ctx->uuid = NULL;
        }
        free(ctx);
    }
    return;
}

/* context resource identification */
static int ctx_id;               /* internal number */
#define ctx_name "UUID context"  /* external name   */

/* module initialization */
PHP_MINIT_FUNCTION(uuid)
{
    /* register resource identifier */
    ctx_id = zend_register_list_destructors_ex(
        ctx_destructor, NULL, ctx_name, module_number);

    /* register API constants */
    REGISTER_LONG_CONSTANT("UUID_VERSION", UUID_VERSION, CONST_CS|CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("UUID_LEN_BIN", UUID_LEN_BIN, CONST_CS|CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("UUID_LEN_STR", UUID_LEN_STR, CONST_CS|CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("UUID_LEN_SIV", UUID_LEN_SIV, CONST_CS|CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("UUID_RC_OK",   UUID_RC_OK,   CONST_CS|CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("UUID_RC_ARG",  UUID_RC_ARG,  CONST_CS|CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("UUID_RC_MEM",  UUID_RC_MEM,  CONST_CS|CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("UUID_RC_SYS",  UUID_RC_SYS,  CONST_CS|CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("UUID_RC_INT",  UUID_RC_INT,  CONST_CS|CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("UUID_RC_IMP",  UUID_RC_IMP,  CONST_CS|CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("UUID_MAKE_V1", UUID_MAKE_V1, CONST_CS|CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("UUID_MAKE_V3", UUID_MAKE_V3, CONST_CS|CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("UUID_MAKE_V4", UUID_MAKE_V4, CONST_CS|CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("UUID_MAKE_V5", UUID_MAKE_V5, CONST_CS|CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("UUID_MAKE_MC", UUID_MAKE_MC, CONST_CS|CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("UUID_FMT_BIN", UUID_FMT_BIN, CONST_CS|CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("UUID_FMT_STR", UUID_FMT_STR, CONST_CS|CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("UUID_FMT_SIV", UUID_FMT_SIV, CONST_CS|CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("UUID_FMT_TXT", UUID_FMT_TXT, CONST_CS|CONST_PERSISTENT);

    return SUCCESS;
}

/* module shutdown */
PHP_MSHUTDOWN_FUNCTION(uuid)
{
    return SUCCESS;
}

/* module information */
PHP_MINFO_FUNCTION(uuid)
{
    char version[32];

    /* provide PHP module information */
    sprintf(version, "%lx", uuid_version());
    php_info_print_table_start();
    php_info_print_table_row(2, "UUID (Universally Unique Identifier) Support", "enabled");
    php_info_print_table_row(2, "UUID Library Version", version);
    php_info_print_table_end();

    return;
}

/* API FUNCTION:
   proto rc uuid_create(ctx)
   $rc = uuid_create(&$uuid);
   create UUID context */
PHP_FUNCTION(uuid_create)
{
    zval *z_ctx;
    ctx_t *ctx;
    uuid_rc_t rc;

    /* parse parameters */
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &z_ctx) == FAILURE)
        RETURN_LONG((long)UUID_RC_ARG);

    /* post-process and sanity check parameters */
    if (!PZVAL_IS_REF(z_ctx)) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_create: parameter wasn't passed by reference");
        RETURN_LONG((long)UUID_RC_ARG);
    }

    /* perform operation */
    if ((ctx = (ctx_t *)malloc(sizeof(ctx_t))) == NULL)
        RETURN_LONG((long)UUID_RC_MEM);
    if ((rc = uuid_create(&ctx->uuid)) != UUID_RC_OK) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_create: %s", uuid_error(rc));
        RETURN_LONG((long)rc);
    }
    ZEND_REGISTER_RESOURCE(z_ctx, ctx, ctx_id);

    RETURN_LONG((long)rc);
}

/* API FUNCTION:
   proto rc uuid_destroy(ctx)
   $rc = uuid_destroy($uuid);
   destroy UUID context */
PHP_FUNCTION(uuid_destroy)
{
    zval *z_ctx;
    ctx_t *ctx;
    uuid_rc_t rc;

    /* parse parameters */
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &z_ctx) == FAILURE)
        RETURN_LONG((long)UUID_RC_ARG);

    /* post-process and sanity check parameters */
    ZEND_FETCH_RESOURCE(ctx, ctx_t *, &z_ctx, -1, ctx_name, ctx_id);
    if (ctx == NULL || ctx->uuid == NULL) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_destroy: invalid context");
        RETURN_LONG((long)UUID_RC_ARG);
    }

    /* perform operation */
    if ((rc = uuid_destroy(ctx->uuid)) != UUID_RC_OK) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_destroy: %s", uuid_error(rc));
        RETURN_LONG((long)rc);
    }
    ctx->uuid = NULL;

    RETURN_LONG((long)rc);
}

/* API FUNCTION:
   proto rc uuid_clone(ctx, &ctx2)
   $rc = uuid_clone($uuid, &$uuid);
   clone UUID context */
PHP_FUNCTION(uuid_clone)
{
    zval *z_ctx;
    ctx_t *ctx;
    zval *z_clone;
    ctx_t *clone;
    uuid_rc_t rc;

    /* parse parameters */
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rz", &z_ctx, &z_clone) == FAILURE)
        RETURN_LONG((long)UUID_RC_ARG);

    /* post-process and sanity check parameters */
    ZEND_FETCH_RESOURCE(ctx, ctx_t *, &z_ctx, -1, ctx_name, ctx_id);
    if (ctx == NULL || ctx->uuid == NULL) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_clone: invalid context");
        RETURN_LONG((long)UUID_RC_ARG);
    }
    if (!PZVAL_IS_REF(z_clone)) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_clone: clone parameter wasn't passed by reference");
        RETURN_LONG((long)UUID_RC_ARG);
    }

    /* perform operation */
    if ((clone = (ctx_t *)malloc(sizeof(ctx_t))) == NULL)
        RETURN_LONG((long)UUID_RC_MEM);
    if ((rc = uuid_clone(ctx->uuid, &clone->uuid)) != UUID_RC_OK) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_clone: %s", uuid_error(rc));
        RETURN_LONG((long)rc);
    }
    ZEND_REGISTER_RESOURCE(z_clone, clone, ctx_id);

    RETURN_LONG((long)rc);
}

/* API FUNCTION:
   proto rc uuid_load(ctx, name)
   $rc = uuid_name($uuid, $name);
   load an existing UUID */
PHP_FUNCTION(uuid_load)
{
    zval *z_ctx;
    ctx_t *ctx;
    char *name;
    size_t name_len;
    uuid_rc_t rc;

    /* parse parameters */
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &z_ctx, &name, &name_len) == FAILURE)
        RETURN_LONG((long)UUID_RC_ARG);

    /* post-process and sanity check parameters */
    ZEND_FETCH_RESOURCE(ctx, ctx_t *, &z_ctx, -1, ctx_name, ctx_id);
    if (ctx == NULL || ctx->uuid == NULL) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_load: invalid context");
        RETURN_LONG((long)UUID_RC_ARG);
    }

    /* perform operation */
    if ((rc = uuid_load(ctx->uuid, name)) != UUID_RC_OK) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_load: %s", uuid_error(rc));
        RETURN_LONG((long)rc);
    }

    RETURN_LONG((long)rc);
}

/* API FUNCTION:
   proto rc uuid_make(ctx, mode[, ..., ...])
   $rc = uuid_make($uuid, $mode[, ..., ...]);
   make a new UUID */
PHP_FUNCTION(uuid_make)
{
    zval *z_ctx;
    ctx_t *ctx;
    uuid_rc_t rc;
    long z_mode;
    unsigned long mode;
    zval *z_ctx_ns;
    ctx_t *ctx_ns;
    char *url;
    size_t url_len;

    /* parse parameters */
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rl|rs", &z_ctx, &z_mode, &z_ctx_ns, &url, &url_len) == FAILURE)
        RETURN_LONG((long)UUID_RC_ARG);

    /* post-process and sanity check parameters */
    ZEND_FETCH_RESOURCE(ctx, ctx_t *, &z_ctx, -1, ctx_name, ctx_id);
    if (ctx == NULL || ctx->uuid == NULL) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_make: invalid context");
        RETURN_LONG((long)UUID_RC_ARG);
    }
    mode = (unsigned long)z_mode;

    /* perform operation */
    if (ZEND_NUM_ARGS() == 2 && ((mode & UUID_MAKE_V1) || (mode & UUID_MAKE_V4))) {
        if ((rc = uuid_make(ctx->uuid, mode)) != UUID_RC_OK) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_make: %s", uuid_error(rc));
            RETURN_LONG((long)rc);
        }
    }
    else if (ZEND_NUM_ARGS() == 4 && ((mode & UUID_MAKE_V3) || (mode & UUID_MAKE_V5))) {
        ZEND_FETCH_RESOURCE(ctx_ns, ctx_t *, &z_ctx_ns, -1, ctx_name, ctx_id);
        if (ctx_ns == NULL) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_make: invalid namespace context");
            RETURN_LONG((long)UUID_RC_ARG);
        }
        if (url == NULL) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_make: invalid URL");
            RETURN_LONG((long)UUID_RC_ARG);
        }
        if ((rc = uuid_make(ctx->uuid, mode, ctx_ns->uuid, url)) != UUID_RC_OK) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_make: %s", uuid_error(rc));
            RETURN_LONG((long)rc);
        }
    }
    else {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_make: invalid mode");
        RETURN_LONG((long)UUID_RC_ARG);
    }

    RETURN_LONG((long)rc);
}

/* API FUNCTION:
   proto rc uuid_isnil(ctx, result)
   $rc = uuid_isnil($uuid, &$result);
   compare UUID for being Nil UUID */
PHP_FUNCTION(uuid_isnil)
{
    zval *z_ctx;
    ctx_t *ctx;
    uuid_rc_t rc;
    zval *z_result;
    int result;

    /* parse parameters */
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rz", &z_ctx, &z_result) == FAILURE)
        RETURN_LONG((long)UUID_RC_ARG);

    /* post-process and sanity check parameters */
    ZEND_FETCH_RESOURCE(ctx, ctx_t *, &z_ctx, -1, ctx_name, ctx_id);
    if (ctx == NULL || ctx->uuid == NULL) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_isnil: invalid context");
        RETURN_LONG((long)UUID_RC_ARG);
    }
    if (!PZVAL_IS_REF(z_result)) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_isnil: result parameter wasn't passed by reference");
        RETURN_LONG((long)UUID_RC_ARG);
    }

    /* perform operation */
    if ((rc = uuid_isnil(ctx->uuid, &result)) != UUID_RC_OK) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_isnil: %s", uuid_error(rc));
        RETURN_LONG((long)rc);
    }
    ZVAL_LONG(z_result, (long)result);

    RETURN_LONG((long)rc);
}

/* API FUNCTION:
   proto rc uuid_compare(ctx, ctx2, result)
   $rc = uuid_compare($uuid, $uuid2, &$result);
   compare two UUIDs */
PHP_FUNCTION(uuid_compare)
{
    zval *z_ctx;
    ctx_t *ctx;
    zval *z_ctx2;
    ctx_t *ctx2;
    uuid_rc_t rc;
    zval *z_result;
    int result;

    /* parse parameters */
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rrz", &z_ctx, &z_ctx2, &z_result) == FAILURE)
        RETURN_LONG((long)UUID_RC_ARG);

    /* post-process and sanity check parameters */
    ZEND_FETCH_RESOURCE(ctx, ctx_t *, &z_ctx, -1, ctx_name, ctx_id);
    if (ctx == NULL || ctx->uuid == NULL) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_compare: invalid context");
        RETURN_LONG((long)UUID_RC_ARG);
    }
    ZEND_FETCH_RESOURCE(ctx2, ctx_t *, &z_ctx2, -1, ctx_name, ctx_id);
    if (ctx2 == NULL || ctx2->uuid) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_compare: invalid context");
        RETURN_LONG((long)UUID_RC_ARG);
    }
    if (!PZVAL_IS_REF(z_result)) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_compare: result parameter wasn't passed by reference");
        RETURN_LONG((long)UUID_RC_ARG);
    }

    /* perform operation */
    if ((rc = uuid_compare(ctx->uuid, ctx2->uuid, &result)) != UUID_RC_OK) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_compare: %s", uuid_error(rc));
        RETURN_LONG((long)rc);
    }
    ZVAL_LONG(z_result, (long)result);

    RETURN_LONG((long)rc);
}

/* API FUNCTION:
   proto rc uuid_import(ctx, fmt, data)
   $rc = uuid_import($ctx, $fmt, $data);
   import UUID from variable */
PHP_FUNCTION(uuid_import)
{
    zval *z_ctx;
    ctx_t *ctx;
    long z_fmt;
    unsigned long fmt;
    zval *z_data;
    uuid_rc_t rc;
    void *data_ptr;
    size_t data_len;

    /* parse parameters */
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rls", &z_ctx, &z_fmt, &data_ptr, &data_len) == FAILURE)
        RETURN_LONG((long)UUID_RC_ARG);

    /* post-process and sanity check parameters */
    ZEND_FETCH_RESOURCE(ctx, ctx_t *, &z_ctx, -1, ctx_name, ctx_id);
    if (ctx == NULL || ctx->uuid == NULL) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_import: invalid context");
        RETURN_LONG((long)UUID_RC_ARG);
    }
    fmt = (unsigned long)z_fmt;

    /* perform operation */
    if ((rc = uuid_import(ctx->uuid, fmt, data_ptr, data_len)) != UUID_RC_OK) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_import: %s", uuid_error(rc));
        RETURN_LONG((long)rc);
    }

    RETURN_LONG((long)rc);
}

/* API FUNCTION:
   proto rc uuid_export(ctx, fmt, data)
   $rc = uuid_error($ctx, $fmt, &$data);
   export UUID into variable */
PHP_FUNCTION(uuid_export)
{
    zval *z_ctx;
    ctx_t *ctx;
    long z_fmt;
    unsigned long fmt;
    zval *z_data;
    uuid_rc_t rc;
    void *data_ptr;
    size_t data_len;

    /* parse parameters */
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rlz", &z_ctx, &z_fmt, &z_data) == FAILURE)
        RETURN_LONG((long)UUID_RC_ARG);

    /* post-process and sanity check parameters */
    ZEND_FETCH_RESOURCE(ctx, ctx_t *, &z_ctx, -1, ctx_name, ctx_id);
    if (ctx == NULL || ctx->uuid == NULL) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_export: invalid context");
        RETURN_LONG((long)UUID_RC_ARG);
    }
    fmt = (unsigned long)z_fmt;
    if (!PZVAL_IS_REF(z_data)) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_export: data parameter wasn't passed by reference");
        RETURN_LONG((long)UUID_RC_ARG);
    }

    /* perform operation */
    data_ptr = NULL;
    data_len = 0;
    if ((rc = uuid_export(ctx->uuid, fmt, &data_ptr, &data_len)) != UUID_RC_OK) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "uuid_export: %s", uuid_error(rc));
        RETURN_LONG((long)rc);
    }
    if (fmt == UUID_FMT_SIV)
        data_len = strlen((char *)data_ptr);
    else if (fmt == UUID_FMT_STR || fmt == UUID_FMT_TXT)
        data_len--; /* PHP doesn't wish NUL-termination on strings */
    ZVAL_STRINGL(z_data, data_ptr, data_len, 1);
    free(data_ptr);

    RETURN_LONG((long)rc);
}

/* API FUNCTION:
   proto rc uuid_error(ctx)
   $error = uuid_error($rc);
   return error string corresponding to error return code */
PHP_FUNCTION(uuid_error)
{
    int z_rc;
    uuid_rc_t rc;
    char *error;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &z_rc) == FAILURE)
        RETURN_NULL();
    rc = (uuid_rc_t)z_rc;
    if ((error = uuid_error(rc)) == NULL)
        RETURN_NULL();
    RETURN_STRING(error, 1);
}

/* API FUNCTION:
   proto int uuid_version()
   $version = uuid_version();
   return library version number */
PHP_FUNCTION(uuid_version)
{
    RETURN_LONG((long)uuid_version());
}

/* module function table */
static function_entry uuid_functions[] = {
    PHP_FE(uuid_create,  NULL)
    PHP_FE(uuid_destroy, NULL)
    PHP_FE(uuid_clone,   NULL)
    PHP_FE(uuid_load,    NULL)
    PHP_FE(uuid_make,    NULL)
    PHP_FE(uuid_isnil,   NULL)
    PHP_FE(uuid_compare, NULL)
    PHP_FE(uuid_import,  NULL)
    PHP_FE(uuid_export,  NULL)
    PHP_FE(uuid_error,   NULL)
    PHP_FE(uuid_version, NULL)
    { NULL, NULL, NULL }
};

/* module entry table */
zend_module_entry uuid_module_entry = {
    STANDARD_MODULE_HEADER,
    "uuid",
    uuid_functions,
    PHP_MINIT(uuid),
    PHP_MSHUTDOWN(uuid),
    NULL,
    NULL,
    PHP_MINFO(uuid),
    NO_VERSION_YET,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_UUID
ZEND_GET_MODULE(uuid)
#endif

