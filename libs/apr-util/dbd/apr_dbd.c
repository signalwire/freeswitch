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

#include <stdio.h>

#include "apu.h"
#include "apr_pools.h"
#include "apr_dbd_internal.h"
#include "apr_dbd.h"
#include "apr_hash.h"
#include "apr_thread_mutex.h"
#include "apr_dso.h"
#include "apr_strings.h"

static apr_hash_t *drivers = NULL;

#define CLEANUP_CAST (apr_status_t (*)(void*))

/* Once the autofoo supports building it for dynamic load, we can use
 * #define APR_DSO_BUILD APR_HAS_DSO
 */

#if APR_DSO_BUILD
#if APR_HAS_THREADS
static apr_thread_mutex_t* mutex = NULL;
#endif
#else
#define DRIVER_LOAD(name,driver,pool) \
    {   \
        extern const apr_dbd_driver_t driver; \
        apr_hash_set(drivers,name,APR_HASH_KEY_STRING,&driver); \
        if (driver.init) {     \
            driver.init(pool); \
        }  \
    }
#endif

static apr_status_t apr_dbd_term(void *ptr)
{
    /* set drivers to NULL so init can work again */
    drivers = NULL;

    /* Everything else we need is handled by cleanups registered
     * when we created mutexes and loaded DSOs
     */
    return APR_SUCCESS;
}

APU_DECLARE(apr_status_t) apr_dbd_init(apr_pool_t *pool)
{
    apr_status_t ret = APR_SUCCESS;

    if (drivers != NULL) {
        return APR_SUCCESS;
    }
    drivers = apr_hash_make(pool);
    apr_pool_cleanup_register(pool, NULL, apr_dbd_term,
                              apr_pool_cleanup_null);

#if APR_DSO_BUILD

#if APR_HAS_THREADS
    ret = apr_thread_mutex_create(&mutex, APR_THREAD_MUTEX_DEFAULT, pool);
    /* This already registers a pool cleanup */
#endif

#else

#if APU_HAVE_MYSQL
    DRIVER_LOAD("mysql", apr_dbd_mysql_driver, pool);
#endif
#if APU_HAVE_PGSQL
    DRIVER_LOAD("pgsql", apr_dbd_pgsql_driver, pool);
#endif
#if APU_HAVE_SQLITE3
    DRIVER_LOAD("sqlite3", apr_dbd_sqlite3_driver, pool);
#endif
#if APU_HAVE_SQLITE2
    DRIVER_LOAD("sqlite2", apr_dbd_sqlite2_driver, pool);
#endif
#if APU_HAVE_SOME_OTHER_BACKEND
    DRIVER_LOAD("firebird", apr_dbd_other_driver, pool);
#endif
#endif
    return ret;
}
APU_DECLARE(apr_status_t) apr_dbd_get_driver(apr_pool_t *pool, const char *name,
                                             const apr_dbd_driver_t **driver)
{
#if APR_DSO_BUILD
    char path[80];
    apr_dso_handle_t *dlhandle = NULL;
#endif
    apr_status_t rv;

   *driver = apr_hash_get(drivers, name, APR_HASH_KEY_STRING);
    if (*driver) {
        return APR_SUCCESS;
    }

#if APR_DSO_BUILD

#if APR_HAS_THREADS
    rv = apr_thread_mutex_lock(mutex);
    if (rv != APR_SUCCESS) {
        goto unlock;
    }
    *driver = apr_hash_get(drivers, name, APR_HASH_KEY_STRING);
    if (*driver) {
        goto unlock;
    }
#endif

#ifdef WIN32
    sprintf(path, "apr_dbd_%s.dll", name);
#else
    sprintf(path, "apr_dbd_%s.so", name);
#endif
    rv = apr_dso_load(&dlhandle, path, pool);
    if (rv != APR_SUCCESS) { /* APR_EDSOOPEN */
        goto unlock;
    }
    sprintf(path, "apr_dbd_%s_driver", name);
    rv = apr_dso_sym((void*)driver, dlhandle, path);
    if (rv != APR_SUCCESS) { /* APR_ESYMNOTFOUND */
        apr_dso_unload(dlhandle);
        goto unlock;
    }
    if ((*driver)->init) {
        (*driver)->init(pool);
    }
    apr_hash_set(drivers, name, APR_HASH_KEY_STRING, *driver);

unlock:
#if APR_HAS_THREADS
    apr_thread_mutex_unlock(mutex);
#endif

#else	/* APR_DSO_BUILD - so if it wasn't already loaded, it's NOTIMPL */
    rv = APR_ENOTIMPL;
#endif

    return rv;
}
APU_DECLARE(apr_status_t) apr_dbd_open(const apr_dbd_driver_t *driver,
                                       apr_pool_t *pool, const char *params,
                                       apr_dbd_t **handle)
{
    apr_status_t rv;
    *handle = driver->open(pool, params);
    if (*handle == NULL) {
        return APR_EGENERAL;
    }
    rv = apr_dbd_check_conn(driver, pool, *handle);
    if ((rv != APR_SUCCESS) && (rv != APR_ENOTIMPL)) {
        apr_dbd_close(driver, *handle);
        return APR_EGENERAL;
    }
    return APR_SUCCESS;
}
APU_DECLARE(int) apr_dbd_transaction_start(const apr_dbd_driver_t *driver,
                                           apr_pool_t *pool, apr_dbd_t *handle,
                                           apr_dbd_transaction_t **trans)
{
    int ret = driver->start_transaction(pool, handle, trans);
    if (*trans) {
        apr_pool_cleanup_register(pool, *trans,
                                  CLEANUP_CAST driver->end_transaction,
                                  apr_pool_cleanup_null);
    }
    return ret;
}
APU_DECLARE(int) apr_dbd_transaction_end(const apr_dbd_driver_t *driver,
                                         apr_pool_t *pool,
                                         apr_dbd_transaction_t *trans)
{
    apr_pool_cleanup_kill(pool, trans, CLEANUP_CAST driver->end_transaction);
    return driver->end_transaction(trans);
}

APU_DECLARE(apr_status_t) apr_dbd_close(const apr_dbd_driver_t *driver,
                                        apr_dbd_t *handle)
{
    return driver->close(handle);
}
APU_DECLARE(const char*) apr_dbd_name(const apr_dbd_driver_t *driver)
{
    return driver->name;
}
APU_DECLARE(void*) apr_dbd_native_handle(const apr_dbd_driver_t *driver,
                                         apr_dbd_t *handle)
{
    return driver->native_handle(handle);
}
APU_DECLARE(int) apr_dbd_check_conn(const apr_dbd_driver_t *driver, apr_pool_t *pool,
                                    apr_dbd_t *handle)
{
    return driver->check_conn(pool, handle);
}
APU_DECLARE(int) apr_dbd_set_dbname(const apr_dbd_driver_t *driver, apr_pool_t *pool,
                                   apr_dbd_t *handle, const char *name)
{
    return driver->set_dbname(pool,handle,name);
}
APU_DECLARE(int) apr_dbd_query(const apr_dbd_driver_t *driver, apr_dbd_t *handle,
                               int *nrows, const char *statement)
{
    return driver->query(handle,nrows,statement);
}
APU_DECLARE(int) apr_dbd_select(const apr_dbd_driver_t *driver, apr_pool_t *pool,
                                apr_dbd_t *handle, apr_dbd_results_t **res,
                                const char *statement, int random)
{
    return driver->select(pool,handle,res,statement,random);
}
APU_DECLARE(int) apr_dbd_num_cols(const apr_dbd_driver_t *driver,
                                  apr_dbd_results_t *res)
{
    return driver->num_cols(res);
}
APU_DECLARE(int) apr_dbd_num_tuples(const apr_dbd_driver_t *driver,
                                    apr_dbd_results_t *res)
{
    return driver->num_tuples(res);
}
APU_DECLARE(int) apr_dbd_get_row(const apr_dbd_driver_t *driver, apr_pool_t *pool,
                                 apr_dbd_results_t *res, apr_dbd_row_t **row,
                                 int rownum)
{
    return driver->get_row(pool,res,row,rownum);
}
APU_DECLARE(const char*) apr_dbd_get_entry(const apr_dbd_driver_t *driver,
                                           apr_dbd_row_t *row, int col)
{
    return driver->get_entry(row,col);
}
APU_DECLARE(const char*) apr_dbd_error(const apr_dbd_driver_t *driver,
                                       apr_dbd_t *handle, int errnum)
{
    return driver->error(handle,errnum);
}
APU_DECLARE(const char*) apr_dbd_escape(const apr_dbd_driver_t *driver,
                                        apr_pool_t *pool, const char *string,
                                        apr_dbd_t *handle)
{
    return driver->escape(pool,string,handle);
}
APU_DECLARE(int) apr_dbd_prepare(const apr_dbd_driver_t *driver, apr_pool_t *pool,
                                 apr_dbd_t *handle, const char *query,
                                 const char *label,
                                 apr_dbd_prepared_t **statement)
{
    return driver->prepare(pool,handle,query,label,statement);
}
APU_DECLARE(int) apr_dbd_pquery(const apr_dbd_driver_t *driver, apr_pool_t *pool,
                                apr_dbd_t *handle, int *nrows,
                                apr_dbd_prepared_t *statement, int nargs,
                                const char **args)
{
    return driver->pquery(pool,handle,nrows,statement,nargs,args);
}
APU_DECLARE(int) apr_dbd_pselect(const apr_dbd_driver_t *driver, apr_pool_t *pool,
                                 apr_dbd_t *handle, apr_dbd_results_t **res,
                                 apr_dbd_prepared_t *statement, int random,
                                 int nargs, const char **args)
{
    return driver->pselect(pool,handle,res,statement,random,nargs,args);
}
APU_DECLARE(int) apr_dbd_pvquery(const apr_dbd_driver_t *driver, apr_pool_t *pool,
                                 apr_dbd_t *handle, int *nrows,
                                 apr_dbd_prepared_t *statement,...)
{
    int ret;
    va_list args;
    va_start(args, statement);
    ret = driver->pvquery(pool,handle,nrows,statement,args);
    va_end(args);
    return ret;
}
APU_DECLARE(int) apr_dbd_pvselect(const apr_dbd_driver_t *driver, apr_pool_t *pool,
                                  apr_dbd_t *handle, apr_dbd_results_t **res,
                                  apr_dbd_prepared_t *statement, int random,...)
{
    int ret;
    va_list args;
    va_start(args, random);
    ret = driver->pvselect(pool,handle,res,statement,random,args);
    va_end(args);
    return ret;
}
