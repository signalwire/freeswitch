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

#include "apu.h"

#if APU_HAVE_PGSQL

#include "apu_config.h"

#include <ctype.h>
#include <stdlib.h>

#ifdef HAVE_LIBPQ_FE_H
#include <libpq-fe.h>
#elif defined(HAVE_POSTGRESQL_LIBPQ_FE_H)
#include <postgresql/libpq-fe.h>
#endif

#include "apr_strings.h"
#include "apr_time.h"

#include "apr_dbd_internal.h"

#define QUERY_MAX_ARGS 40

struct apr_dbd_transaction_t {
    int errnum;
    apr_dbd_t *handle;
};

struct apr_dbd_t {
    PGconn *conn;
    apr_dbd_transaction_t *trans;
};

struct apr_dbd_results_t {
    int random;
    PGconn *handle;
    PGresult *res;
    size_t ntuples;
    size_t sz;
    size_t index;
};

struct apr_dbd_row_t {
    int n;
    apr_dbd_results_t *res;
};

struct apr_dbd_prepared_t {
    const char *name;
    int prepared;
    int nargs;
};

#define dbd_pgsql_is_success(x) (((x) == PGRES_EMPTY_QUERY) \
                                 || ((x) == PGRES_COMMAND_OK) \
                                 || ((x) == PGRES_TUPLES_OK))

static apr_status_t clear_result(void *data)
{
    PQclear(data);
    return APR_SUCCESS;
}

static int dbd_pgsql_select(apr_pool_t *pool, apr_dbd_t *sql,
                            apr_dbd_results_t **results,
                            const char *query, int seek)
{
    PGresult *res;
    int ret;
    if ( sql->trans && sql->trans->errnum ) {
        return sql->trans->errnum;
    }
    if (seek) { /* synchronous query */
        res = PQexec(sql->conn, query);
        if (res) {
            ret = PQresultStatus(res);
            if (dbd_pgsql_is_success(ret)) {
                ret = 0;
            } else {
                PQclear(res);
            }
        } else {
            ret = PGRES_FATAL_ERROR;
        }
        if (ret != 0) {
            if (sql->trans) {
                sql->trans->errnum = ret;
            }
            return ret;
        }
        if (!*results) {
            *results = apr_pcalloc(pool, sizeof(apr_dbd_results_t));
        }
        (*results)->res = res;
        (*results)->ntuples = PQntuples(res);
        (*results)->sz = PQnfields(res);
        (*results)->random = seek;
        apr_pool_cleanup_register(pool, res, clear_result,
                                  apr_pool_cleanup_null);
    }
    else {
        if (PQsendQuery(sql->conn, query) == 0) {
            if (sql->trans) {
                sql->trans->errnum = 1;
            }
            return 1;
        }
        if (*results == NULL) {
            *results = apr_pcalloc(pool, sizeof(apr_dbd_results_t));
        }
        (*results)->random = seek;
        (*results)->handle = sql->conn;
    }
    return 0;
}

static int dbd_pgsql_get_row(apr_pool_t *pool, apr_dbd_results_t *res,
                             apr_dbd_row_t **rowp, int rownum)
{
    apr_dbd_row_t *row = *rowp;
    int sequential = ((rownum >= 0) && res->random) ? 0 : 1;

    if (row == NULL) {
        row = apr_palloc(pool, sizeof(apr_dbd_row_t));
        *rowp = row;
        row->res = res;
        row->n = sequential ? 0 : rownum;
    }
    else {
        if ( sequential ) {
            ++row->n;
        }
        else {
            row->n = rownum;
        }
    }

    if (res->random) {
        if (row->n >= res->ntuples) {
            *rowp = NULL;
            apr_pool_cleanup_run(pool, res->res, clear_result);
            res->res = NULL;
            return -1;
        }
    }
    else {
        if (row->n >= res->ntuples) {
            /* no data; we have to fetch some */
            row->n -= res->ntuples;
            if (res->res != NULL) {
                PQclear(res->res);
            }
            res->res = PQgetResult(res->handle);
            if (res->res) {
                res->ntuples = PQntuples(res->res);
                while (res->ntuples == 0) {
                    /* if we got an empty result, clear it, wait a mo, try
                     * again */
                    PQclear(res->res);
                    apr_sleep(100000);        /* 0.1 secs */
                    res->res = PQgetResult(res->handle);
                    if (res->res) {
                        res->ntuples = PQntuples(res->res);
                    }
                    else {
                        return -1;
                    }
                }
                if (res->sz == 0) {
                    res->sz = PQnfields(res->res);
                }
            }
            else {
                return -1;
            }
        }
    }
    return 0;
}

static const char *dbd_pgsql_get_entry(const apr_dbd_row_t *row, int n)
{
    return PQgetvalue(row->res->res, row->n, n);
}

static const char *dbd_pgsql_error(apr_dbd_t *sql, int n)
{
    return PQerrorMessage(sql->conn);
}

static int dbd_pgsql_query(apr_dbd_t *sql, int *nrows, const char *query)
{
    PGresult *res;
    int ret;
    if (sql->trans && sql->trans->errnum) {
        return sql->trans->errnum;
    }
    res = PQexec(sql->conn, query);
    if (res) {
        ret = PQresultStatus(res);
        if (dbd_pgsql_is_success(ret)) {
            /* ugh, making 0 return-success doesn't fit */
            ret = 0;
        }
        *nrows = atoi(PQcmdTuples(res));
        PQclear(res);
    }
    else {
        ret = PGRES_FATAL_ERROR;
    }
    if (sql->trans) {
        sql->trans->errnum = ret;
    }
    return ret;
}

static const char *dbd_pgsql_escape(apr_pool_t *pool, const char *arg,
                                    apr_dbd_t *sql)
{
    size_t len = strlen(arg);
    char *ret = apr_palloc(pool, 2*(len + 1));
    PQescapeString(ret, arg, len);
    return ret;
}

static int dbd_pgsql_prepare(apr_pool_t *pool, apr_dbd_t *sql,
                             const char *query, const char *label,
                             apr_dbd_prepared_t **statement)
{
    char *sqlcmd;
    char *sqlptr;
    size_t length;
    size_t i = 0;
    const char *args[QUERY_MAX_ARGS];
    size_t alen;
    int ret;
    PGresult *res;
    char *pgquery;
    char *pgptr;

    if (!*statement) {
        *statement = apr_palloc(pool, sizeof(apr_dbd_prepared_t));
    }
    (*statement)->nargs = 0;
    /* Translate from apr_dbd to native query format */
    for (sqlptr = (char*)query; *sqlptr; ++sqlptr) {
        if (sqlptr[0] == '%') {
            if (isalpha(sqlptr[1])) {
                ++(*statement)->nargs;
            }
            else if (sqlptr[1] == '%') {
                ++sqlptr;
            }
        }
    }
    length = strlen(query) + 1;
    if ((*statement)->nargs > 8) {
        length += (*statement)->nargs - 8;
    }
    pgptr = pgquery = apr_palloc(pool, length) ;

    for (sqlptr = (char*)query; *sqlptr; ++sqlptr) {
        if ((sqlptr[0] == '%') && isalpha(sqlptr[1])) {
            *pgptr++ = '$';
            if (i < 9) {
                *pgptr++ = '1' + i;
            }
            else {
                *pgptr++ = '0' + ((i+1)/10);
                *pgptr++ = '0' + ((i+1)%10);
            }
            switch (*++sqlptr) {
            case 'd':
                args[i] = "integer";
                break;
            case 's':
                args[i] = "varchar";
                break;
            default:
                args[i] = "varchar";
                break;
            }
            length += 1 + strlen(args[i]);
            ++i;
        }
        else if ((sqlptr[0] == '%') && (sqlptr[1] == '%')) {
            /* reduce %% to % */
            *pgptr++ = *sqlptr++;
        }
        else {
            *pgptr++ = *sqlptr;
        }
    }
    *pgptr = 0;

    if (!label) {
        /* don't really prepare; use in execParams instead */
        (*statement)->prepared = 0;
        (*statement)->name = apr_pstrdup(pool, pgquery);
        return 0;
    }
    (*statement)->name = apr_pstrdup(pool, label);

    /* length of SQL query that prepares this statement */
    length = 8 + strlen(label) + 2 + 4 + length + 1;
    sqlcmd = apr_palloc(pool, length);
    sqlptr = sqlcmd;
    memcpy(sqlptr, "PREPARE ", 8);
    sqlptr += 8;
    length = strlen(label);
    memcpy(sqlptr, label, length);
    sqlptr += length;
    if ((*statement)->nargs > 0) {
        memcpy(sqlptr, " (",2);
        sqlptr += 2;
        for (i=0; i < (*statement)->nargs; ++i) {
            alen = strlen(args[i]);
            memcpy(sqlptr, args[i], alen);
            sqlptr += alen;
            *sqlptr++ = ',';
        }
        sqlptr[-1] =  ')';
    }
    memcpy(sqlptr, " AS ", 4);
    sqlptr += 4;
    memcpy(sqlptr, pgquery, strlen(pgquery));
    sqlptr += strlen(pgquery);
    *sqlptr = 0;

    res = PQexec(sql->conn, sqlcmd);
    if ( res ) {
        ret = PQresultStatus(res);
        if (dbd_pgsql_is_success(ret)) {
            ret = 0;
        }
        /* Hmmm, do we do this here or register it on the pool? */
        PQclear(res);
    }
    else {
        ret = PGRES_FATAL_ERROR;
    }
    (*statement)->prepared = 1;

    return ret;
}

static int dbd_pgsql_pquery(apr_pool_t *pool, apr_dbd_t *sql,
                            int *nrows, apr_dbd_prepared_t *statement,
                            int nargs, const char **values)
{
    int ret;
    PGresult *res;

    if (sql->trans && sql->trans->errnum) {
        return sql->trans->errnum;
    }

    if (statement->prepared) {
        res = PQexecPrepared(sql->conn, statement->name, nargs, values, 0, 0,
                             0);
    }
    else {
        res = PQexecParams(sql->conn, statement->name, nargs, 0, values, 0, 0,
                           0);
    }
    if (res) {
        ret = PQresultStatus(res);
        if (dbd_pgsql_is_success(ret)) {
            ret = 0;
        }
	*nrows = atoi(PQcmdTuples(res));
        PQclear(res);
    }
    else {
        ret = PGRES_FATAL_ERROR;
    }

    if (sql->trans) {
        sql->trans->errnum = ret;
    }
    return ret;
}

static int dbd_pgsql_pvquery(apr_pool_t *pool, apr_dbd_t *sql,
                             int *nrows, apr_dbd_prepared_t *statement,
                             va_list args)
{
    const char **values;
    int i;

    if (sql->trans && sql->trans->errnum) {
        return sql->trans->errnum;
    }

    values = apr_palloc(pool, sizeof(*values) * statement->nargs);

    for (i = 0; i < statement->nargs; i++) {
        values[i] = apr_pstrdup(pool, va_arg(args, const char*));
    }

    return dbd_pgsql_pquery(pool, sql, nrows, statement,
                            statement->nargs, values);
}

static int dbd_pgsql_pselect(apr_pool_t *pool, apr_dbd_t *sql,
                             apr_dbd_results_t **results,
                             apr_dbd_prepared_t *statement,
                             int seek, int nargs, const char **values)
{
    PGresult *res;
    int rv;
    int ret = 0;

    if (sql->trans && sql->trans->errnum) {
        return sql->trans->errnum;
    }

    if (seek) { /* synchronous query */
        if (statement->prepared) {
            res = PQexecPrepared(sql->conn, statement->name, nargs, values, 0,
                                 0, 0);
        }
        else {
            res = PQexecParams(sql->conn, statement->name, nargs, 0, values, 0,
                               0, 0);
        }
        if (res) {
            ret = PQresultStatus(res);
            if (dbd_pgsql_is_success(ret)) {
                ret = 0;
            }
            else {
                PQclear(res);
            }
        }
        else {
            ret = PGRES_FATAL_ERROR;
        }
        if (ret != 0) {
            if (sql->trans) {
                sql->trans->errnum = ret;
            }
            return ret;
        }
        if (!*results) {
            *results = apr_pcalloc(pool, sizeof(apr_dbd_results_t));
        }
        (*results)->res = res;
        (*results)->ntuples = PQntuples(res);
        (*results)->sz = PQnfields(res);
        (*results)->random = seek;
        apr_pool_cleanup_register(pool, res, clear_result,
                                  apr_pool_cleanup_null);
    }
    else {
        if (statement->prepared) {
            rv = PQsendQueryPrepared(sql->conn, statement->name, nargs, values,
                                     0, 0, 0);
        }
        else {
            rv = PQsendQueryParams(sql->conn, statement->name, nargs, 0,
                                   values, 0, 0, 0);
        }
        if (rv == 0) {
            if (sql->trans) {
                sql->trans->errnum = 1;
            }
            return 1;
        }
        if (!*results) {
            *results = apr_pcalloc(pool, sizeof(apr_dbd_results_t));
        }
        (*results)->random = seek;
        (*results)->handle = sql->conn;
    }

    if (sql->trans) {
        sql->trans->errnum = ret;
    }
    return ret;
}

static int dbd_pgsql_pvselect(apr_pool_t *pool, apr_dbd_t *sql,
                              apr_dbd_results_t **results,
                              apr_dbd_prepared_t *statement,
                              int seek, va_list args)
{
    const char **values;
    int i;

    if (sql->trans && sql->trans->errnum) {
        return sql->trans->errnum;
    }

    values = apr_palloc(pool, sizeof(*values) * statement->nargs);

    for (i = 0; i < statement->nargs; i++) {
        values[i] = apr_pstrdup(pool, va_arg(args, const char*));
    }

    return dbd_pgsql_pselect(pool, sql, results, statement,
                             seek, statement->nargs, values) ;
}

static int dbd_pgsql_start_transaction(apr_pool_t *pool, apr_dbd_t *handle,
                                       apr_dbd_transaction_t **trans)
{
    int ret = 0;
    PGresult *res;

    /* XXX handle recursive transactions here */

    res = PQexec(handle->conn, "BEGIN TRANSACTION");
    if (res) {
        ret = PQresultStatus(res);
        if (dbd_pgsql_is_success(ret)) {
            ret = 0;
            if (!*trans) {
                *trans = apr_pcalloc(pool, sizeof(apr_dbd_transaction_t));
            }
        }
        PQclear(res);
        (*trans)->handle = handle;
        handle->trans = *trans;
    }
    else {
        ret = PGRES_FATAL_ERROR;
    }
    return ret;
}

static int dbd_pgsql_end_transaction(apr_dbd_transaction_t *trans)
{
    PGresult *res;
    int ret = -1;                /* no transaction is an error cond */
    if (trans) {
        if (trans->errnum) {
            trans->errnum = 0;
            res = PQexec(trans->handle->conn, "ROLLBACK");
        }
        else {
            res = PQexec(trans->handle->conn, "COMMIT");
        }
        if (res) {
            ret = PQresultStatus(res);
            if (dbd_pgsql_is_success(ret)) {
                ret = 0;
            }
            PQclear(res);
        }
        else {
            ret = PGRES_FATAL_ERROR;
        }
        trans->handle->trans = NULL;
    }
    return ret;
}

static apr_dbd_t *dbd_pgsql_open(apr_pool_t *pool, const char *params)
{
    apr_dbd_t *sql;
    
    PGconn *conn = PQconnectdb(params);

    /* if there's an error in the connect string or something we get
     * back a * bogus connection object, and things like PQreset are
     * liable to segfault, so just close it out now.  it would be nice
     * if we could give an indication of why we failed to connect... */
    if (PQstatus(conn) != CONNECTION_OK) {
        PQfinish(conn);
        return NULL;
    }

    sql = apr_pcalloc (pool, sizeof (*sql));

    sql->conn = conn;

    return sql;
}

static apr_status_t dbd_pgsql_close(apr_dbd_t *handle)
{
    PQfinish(handle->conn);
    return APR_SUCCESS;
}

static apr_status_t dbd_pgsql_check_conn(apr_pool_t *pool,
                                         apr_dbd_t *handle)
{
    if (PQstatus(handle->conn) != CONNECTION_OK) {
        PQreset(handle->conn);
        if (PQstatus(handle->conn) != CONNECTION_OK) {
            return APR_EGENERAL;
        }
    }
    return APR_SUCCESS;
}

static int dbd_pgsql_select_db(apr_pool_t *pool, apr_dbd_t *handle,
                               const char *name)
{
    return APR_ENOTIMPL;
}

static void *dbd_pgsql_native(apr_dbd_t *handle)
{
    return handle->conn;
}

static int dbd_pgsql_num_cols(apr_dbd_results_t* res)
{
    return res->sz;
}

static int dbd_pgsql_num_tuples(apr_dbd_results_t* res)
{
    if (res->random) {
        return res->ntuples;
    }
    else {
        return -1;
    }
}

APU_DECLARE_DATA const apr_dbd_driver_t apr_dbd_pgsql_driver = {
    "pgsql",
    NULL,
    dbd_pgsql_native,
    dbd_pgsql_open,
    dbd_pgsql_check_conn,
    dbd_pgsql_close,
    dbd_pgsql_select_db,
    dbd_pgsql_start_transaction,
    dbd_pgsql_end_transaction,
    dbd_pgsql_query,
    dbd_pgsql_select,
    dbd_pgsql_num_cols,
    dbd_pgsql_num_tuples,
    dbd_pgsql_get_row,
    dbd_pgsql_get_entry,
    dbd_pgsql_error,
    dbd_pgsql_escape,
    dbd_pgsql_prepare,
    dbd_pgsql_pvquery,
    dbd_pgsql_pvselect,
    dbd_pgsql_pquery,
    dbd_pgsql_pselect,
};
#endif
