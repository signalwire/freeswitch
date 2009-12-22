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
**  uuid++.cc: library C++ API implementation
*/

#include <string.h>
#include <stdarg.h>

#include "uuid++.hh"

/*  standard constructor */
uuid::uuid()
{
    uuid_rc_t rc;
    if ((rc = uuid_create(&ctx)) != UUID_RC_OK)
        throw uuid_error_t(rc);
}

/*  copy constructor */
uuid::uuid(const uuid &obj)
{
    /* Notice: the copy constructor is the same as the assignment
       operator (with the object as the argument) below, except that
       (1) no check for self-assignment is required, (2) no existing
       internals have to be destroyed and (3) no return value is given back. */
    uuid_rc_t rc;
    if ((rc = uuid_clone(obj.ctx, &ctx)) != UUID_RC_OK)
        throw uuid_error_t(rc);
    return;
}

/*  extra constructor via C API object */
uuid::uuid(const uuid_t *obj)
{
    uuid_rc_t rc;
    if (obj == NULL)
        throw uuid_error_t(UUID_RC_ARG);
    if ((rc = uuid_clone(obj, &ctx)) != UUID_RC_OK)
        throw uuid_error_t(rc);
    return;
}

/*  extra constructor via binary representation */
uuid::uuid(const void *bin)
{
    uuid_rc_t rc;
    if (bin == NULL)
        throw uuid_error_t(UUID_RC_ARG);
    if ((rc = uuid_create(&ctx)) != UUID_RC_OK)
        throw uuid_error_t(rc);
    import(bin);
    return;
}

/*  extra constructor via string representation */
uuid::uuid(const char *str)
{
    uuid_rc_t rc;
    if (str == NULL)
        throw uuid_error_t(UUID_RC_ARG);
    if ((rc = uuid_create(&ctx)) != UUID_RC_OK)
        throw uuid_error_t(rc);
    import(str);
    return;
}

/*  standard destructor */
uuid::~uuid()
{
    uuid_destroy(ctx);
    return;
}

/*  assignment operator: import of other C++ API object */
uuid &uuid::operator=(const uuid &obj)
{
    uuid_rc_t rc;
    if (this == &obj)
        return *this;
    if ((rc = uuid_destroy(ctx)) != UUID_RC_OK)
        throw uuid_error_t(rc);
    if ((rc = uuid_clone(obj.ctx, &ctx)) != UUID_RC_OK)
        throw uuid_error_t(rc);
    return *this;
}

/*  assignment operator: import of other C API object */
uuid &uuid::operator=(const uuid_t *obj)
{
    uuid_rc_t rc;
    if (obj == NULL)
        throw uuid_error_t(UUID_RC_ARG);
    if ((rc = uuid_clone(obj, &ctx)) != UUID_RC_OK)
        throw uuid_error_t(rc);
    return *this;
}

/*  assignment operator: import of binary representation */
uuid &uuid::operator=(const void *bin)
{
    if (bin == NULL)
        throw uuid_error_t(UUID_RC_ARG);
    import(bin);
    return *this;
}

/*  assignment operator: import of string representation */
uuid &uuid::operator=(const char *str)
{
    if (str == NULL)
        throw uuid_error_t(UUID_RC_ARG);
    import(str);
    return *this;
}

/*  method: clone object */
uuid uuid::clone(void)
{
    return new uuid(this);
}

/*  method: loading existing UUID by name */
void uuid::load(const char *name)
{
    uuid_rc_t rc;
    if (name == NULL)
        throw uuid_error_t(UUID_RC_ARG);
    if ((rc = uuid_load(ctx, name)) != UUID_RC_OK)
        throw uuid_error_t(rc);
    return;
}

/*  method: making new UUID one from scratch */
void uuid::make(unsigned int mode, ...)
{
    uuid_rc_t rc;
    va_list ap;

    va_start(ap, mode);
    if ((mode & UUID_MAKE_V3) || (mode & UUID_MAKE_V5)) {
        const uuid *ns = (const uuid *)va_arg(ap, const uuid *);
        const char *name = (const char *)va_arg(ap, char *);
        if (ns == NULL || name == NULL)
            throw uuid_error_t(UUID_RC_ARG);
        rc = uuid_make(ctx, mode, ns->ctx, name);
    }
    else
        rc = uuid_make(ctx, mode);
    va_end(ap);
    if (rc != UUID_RC_OK)
        throw uuid_error_t(rc);
    return;
}

/*  method: comparison for Nil UUID */
int uuid::isnil(void)
{
    uuid_rc_t rc;
    int rv;

    if ((rc = uuid_isnil(ctx, &rv)) != UUID_RC_OK)
        throw uuid_error_t(rc);
    return rv;
}

/*  method: comparison against other object */
int uuid::compare(const uuid &obj)
{
    uuid_rc_t rc;
    int rv;

    if ((rc = uuid_compare(ctx, obj.ctx, &rv)) != UUID_RC_OK)
        throw uuid_error_t(rc);
    return rv;
}

/*  method: comparison for equality */
int uuid::operator==(const uuid &obj)
{
    return (compare(obj) == 0);
}

/*  method: comparison for inequality */
int uuid::operator!=(const uuid &obj)
{
    return (compare(obj) != 0);
}

/*  method: comparison for lower-than */
int uuid::operator<(const uuid &obj)
{
    return (compare(obj) < 0);
}

/*  method: comparison for lower-than-or-equal */
int uuid::operator<=(const uuid &obj)
{
    return (compare(obj) <= 0);
}

/*  method: comparison for greater-than */
int uuid::operator>(const uuid &obj)
{
    return (compare(obj) > 0);
}

/*  method: comparison for greater-than-or-equal */
int uuid::operator>=(const uuid &obj)
{
    return (compare(obj) >= 0);
}

/*  method: import binary representation */
void uuid::import(const void *bin)
{
    uuid_rc_t rc;
    if ((rc = uuid_import(ctx, UUID_FMT_BIN, bin, UUID_LEN_BIN)) != UUID_RC_OK)
        throw uuid_error_t(rc);
    return;
}

/*  method: import string or single integer value representation */
void uuid::import(const char *str)
{
    uuid_rc_t rc;
    if ((rc = uuid_import(ctx, UUID_FMT_STR, str, UUID_LEN_STR)) != UUID_RC_OK)
        if ((rc = uuid_import(ctx, UUID_FMT_SIV, str, UUID_LEN_SIV)) != UUID_RC_OK)
            throw uuid_error_t(rc);
    return;
}

/*  method: export binary representation */
void *uuid::binary(void)
{
    uuid_rc_t rc;
    void *bin = NULL;
    if ((rc = uuid_export(ctx, UUID_FMT_BIN, &bin, NULL)) != UUID_RC_OK)
        throw uuid_error_t(rc);
    return bin;
}

/*  method: export string representation */
char *uuid::string(void)
{
    uuid_rc_t rc;
    char *str = NULL;
    if ((rc = uuid_export(ctx, UUID_FMT_STR, (void **)&str, NULL)) != UUID_RC_OK)
        throw uuid_error_t(rc);
    return str;
}

/*  method: export single integer value representation */
char *uuid::integer(void)
{
    uuid_rc_t rc;
    char *str = NULL;
    if ((rc = uuid_export(ctx, UUID_FMT_SIV, (void **)&str, NULL)) != UUID_RC_OK)
        throw uuid_error_t(rc);
    return str;
}

/*  method: export textual summary representation */
char *uuid::summary(void)
{
    uuid_rc_t rc;
    char *txt = NULL;
    if ((rc = uuid_export(ctx, UUID_FMT_TXT, (void **)&txt, NULL)) != UUID_RC_OK)
        throw uuid_error_t(rc);
    return txt;
}

/*  method: return library version */
unsigned long uuid::version(void)
{
    return uuid_version();
}

