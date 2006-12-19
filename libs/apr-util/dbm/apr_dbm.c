/* Copyright 2000-2005 The Apache Software Foundation or its licensors, as
 * applicable.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "apr.h"
#include "apr_errno.h"
#include "apr_pools.h"
#include "apr_strings.h"
#define APR_WANT_MEMFUNC
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "apr_general.h"

#include "apu.h"
#include "apu_select_dbm.h"
#include "apr_dbm.h"
#include "apr_dbm_private.h"

/* ### note: the setting of DBM_VTABLE will go away once we have multiple
   ### DBMs in here. 
   ### Well, that day is here.  So, do we remove DBM_VTABLE and the old
   ### API entirely?  Oh, what to do.  We need an APU_DEFAULT_DBM #define.
   ### Sounds like a job for autoconf. */

#if APU_USE_SDBM
#define DBM_VTABLE apr_dbm_type_sdbm
#elif APU_USE_GDBM
#define DBM_VTABLE apr_dbm_type_gdbm
#elif APU_USE_DB
#define DBM_VTABLE apr_dbm_type_db
#elif APU_USE_NDBM
#define DBM_VTABLE apr_dbm_type_ndbm
#else /* Not in the USE_xDBM list above */
#error a DBM implementation was not specified
#endif

APU_DECLARE(apr_status_t) apr_dbm_open_ex(apr_dbm_t **pdb, const char*type, 
                                       const char *pathname, 
                                       apr_int32_t mode, apr_fileperms_t perm,
                                       apr_pool_t *pool)
{
#if APU_HAVE_GDBM
    if (!strcasecmp(type, "GDBM")) {
        return (*apr_dbm_type_gdbm.open)(pdb, pathname, mode, perm, pool);
    }
#endif
#if APU_HAVE_SDBM
    if (!strcasecmp(type, "SDBM")) {
        return (*apr_dbm_type_sdbm.open)(pdb, pathname, mode, perm, pool);
    }
#endif
#if APU_HAVE_DB
    if (!strcasecmp(type, "DB")) {
        return (*apr_dbm_type_db.open)(pdb, pathname, mode, perm, pool);
    }
#endif
#if APU_HAVE_NDBM
    if (!strcasecmp(type, "NDBM")) {
        return (*apr_dbm_type_ndbm.open)(pdb, pathname, mode, perm, pool);
    }
#endif

    if (!strcasecmp(type, "default")) {
        return (*DBM_VTABLE.open)(pdb, pathname, mode, perm, pool);
    }

    return APR_ENOTIMPL;
} 

APU_DECLARE(apr_status_t) apr_dbm_open(apr_dbm_t **pdb, const char *pathname, 
                                       apr_int32_t mode, apr_fileperms_t perm,
                                       apr_pool_t *pool)
{
    return (*DBM_VTABLE.open)(pdb, pathname, mode, perm, pool);
}

APU_DECLARE(void) apr_dbm_close(apr_dbm_t *dbm)
{
    (*dbm->type->close)(dbm);
}

APU_DECLARE(apr_status_t) apr_dbm_fetch(apr_dbm_t *dbm, apr_datum_t key,
                                        apr_datum_t *pvalue)
{
    return (*dbm->type->fetch)(dbm, key, pvalue);
}

APU_DECLARE(apr_status_t) apr_dbm_store(apr_dbm_t *dbm, apr_datum_t key,
                                        apr_datum_t value)
{
    return (*dbm->type->store)(dbm, key, value);
}

APU_DECLARE(apr_status_t) apr_dbm_delete(apr_dbm_t *dbm, apr_datum_t key)
{
    return (*dbm->type->del)(dbm, key);
}

APU_DECLARE(int) apr_dbm_exists(apr_dbm_t *dbm, apr_datum_t key)
{
    return (*dbm->type->exists)(dbm, key);
}

APU_DECLARE(apr_status_t) apr_dbm_firstkey(apr_dbm_t *dbm, apr_datum_t *pkey)
{
    return (*dbm->type->firstkey)(dbm, pkey);
}

APU_DECLARE(apr_status_t) apr_dbm_nextkey(apr_dbm_t *dbm, apr_datum_t *pkey)
{
    return (*dbm->type->nextkey)(dbm, pkey);
}

APU_DECLARE(void) apr_dbm_freedatum(apr_dbm_t *dbm, apr_datum_t data)
{
    (*dbm->type->freedatum)(dbm, data);
}

APU_DECLARE(char *) apr_dbm_geterror(apr_dbm_t *dbm, int *errcode,
                                     char *errbuf, apr_size_t errbufsize)
{
    if (errcode != NULL)
        *errcode = dbm->errcode;

    /* assert: errbufsize > 0 */

    if (dbm->errmsg == NULL)
        *errbuf = '\0';
    else
        (void) apr_cpystrn(errbuf, dbm->errmsg, errbufsize);
    return errbuf;
}

APU_DECLARE(apr_status_t) apr_dbm_get_usednames_ex(apr_pool_t *p, 
                                                   const char *type, 
                                                   const char *pathname,
                                                   const char **used1,
                                                   const char **used2)
{
#if APU_HAVE_GDBM
    if (!strcasecmp(type, "GDBM")) {
        (*apr_dbm_type_gdbm.getusednames)(p,pathname,used1,used2);
        return APR_SUCCESS;
    }
#endif
#if APU_HAVE_SDBM
    if (!strcasecmp(type, "SDBM")) {
        (*apr_dbm_type_sdbm.getusednames)(p,pathname,used1,used2);
        return APR_SUCCESS;
    }
#endif
#if APU_HAVE_DB
    if (!strcasecmp(type, "DB")) {
        (*apr_dbm_type_db.getusednames)(p,pathname,used1,used2);
        return APR_SUCCESS;
    }
#endif
#if APU_HAVE_NDBM
    if (!strcasecmp(type, "NDBM")) {
        (*apr_dbm_type_ndbm.getusednames)(p,pathname,used1,used2);
        return APR_SUCCESS;
    }
#endif

    if (!strcasecmp(type, "default")) {
        (*DBM_VTABLE.getusednames)(p, pathname, used1, used2);
        return APR_SUCCESS;
    }

    return APR_ENOTIMPL;
} 

APU_DECLARE(void) apr_dbm_get_usednames(apr_pool_t *p,
                                        const char *pathname,
                                        const char **used1,
                                        const char **used2)
{
    /* ### one day, a DBM type name will be passed and we'll need to look it
       ### up. for now, it is constant. */

    (*DBM_VTABLE.getusednames)(p, pathname, used1, used2);
}

/* Most DBM libraries take a POSIX mode for creating files.  Don't trust
 * the mode_t type, some platforms may not support it, int is safe.
 */
APU_DECLARE(int) apr_posix_perms2mode(apr_fileperms_t perm)
{
    int mode = 0;

    mode |= 0700 & (perm >> 2); /* User  is off-by-2 bits */
    mode |= 0070 & (perm >> 1); /* Group is off-by-1 bit */
    mode |= 0007 & (perm);      /* World maps 1 for 1 */
    return mode;
}
