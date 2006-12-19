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

#if APU_HAVE_SQLITE3

#include <ctype.h>
#include <stdlib.h>

#include <sqlite3.h>

#include "apr_strings.h"
#include "apr_time.h"

#include "apr_dbd_internal.h"

#define MAX_RETRY_COUNT 15
#define MAX_RETRY_SLEEP 100000

struct apr_dbd_transaction_t {
    int errnum;
    apr_dbd_t *handle;
};

struct apr_dbd_t {
    sqlite3 *conn;
    apr_dbd_transaction_t *trans;
#if APR_HAS_THREADS
    apr_thread_mutex_t *mutex;
#endif
    apr_pool_t *pool;
    apr_dbd_prepared_t *prep;
};

typedef struct {
    char *name;
    char *value;
    int size;
    int type;
} apr_dbd_column_t;

struct apr_dbd_row_t {
    apr_dbd_results_t *res;
    apr_dbd_column_t **columns;
    apr_dbd_row_t *next_row;
    int columnCount;
    int rownum;
};

struct apr_dbd_results_t {
    int random;
    sqlite3 *handle;
    sqlite3_stmt *stmt;
    apr_dbd_row_t *next_row;
    size_t sz;
    int tuples;
    char **col_names;
};

struct apr_dbd_prepared_t {
    sqlite3_stmt *stmt;
    apr_dbd_prepared_t *next;
};

#define dbd_sqlite3_is_success(x) (((x) == SQLITE_DONE ) \
		|| ((x) == SQLITE_OK ))

static int dbd_sqlite3_select(apr_pool_t * pool, apr_dbd_t * sql, apr_dbd_results_t ** results, const char *query, int seek)
{
    sqlite3_stmt *stmt = NULL;
    const char *tail = NULL;
    int i, ret, retry_count = 0;
    size_t num_tuples = 0;
    int increment = 0;
    apr_dbd_row_t *row = NULL;
    apr_dbd_row_t *lastrow = NULL;
    apr_dbd_column_t *column;
    char *hold = NULL;

    if (sql->trans && sql->trans->errnum) {
        return sql->trans->errnum;
    }

#if APR_HAS_THREADS
    apr_thread_mutex_lock(sql->mutex);
#endif

    ret = sqlite3_prepare(sql->conn, query, strlen(query), &stmt, &tail);
    if (!dbd_sqlite3_is_success(ret)) {
#if APR_HAS_THREADS
        apr_thread_mutex_unlock(sql->mutex);
#endif
        return ret;
    } else {
        int column_count;
        column_count = sqlite3_column_count(stmt);
        if (!*results) {
            *results = apr_pcalloc(pool, sizeof(apr_dbd_results_t));
        }
        (*results)->stmt = stmt;
        (*results)->sz = column_count;
        (*results)->random = seek;
        (*results)->next_row = 0;
        (*results)->tuples = 0;
        (*results)->col_names = apr_pcalloc(pool,
                                            column_count * sizeof(char *));
        do {
            ret = sqlite3_step(stmt);
            if (ret == SQLITE_BUSY) {
                if (retry_count++ > MAX_RETRY_COUNT) {
                    ret = SQLITE_ERROR;
                } else {
#if APR_HAS_THREADS
                    apr_thread_mutex_unlock(sql->mutex);
#endif
                    apr_sleep(MAX_RETRY_SLEEP);
#if APR_HAS_THREADS
                    apr_thread_mutex_lock(sql->mutex);
#endif
                }
            } else if (ret == SQLITE_ROW) {
                int length;
                apr_dbd_column_t *col;
                row = apr_palloc(pool, sizeof(apr_dbd_row_t));
                row->res = *results;
                increment = sizeof(apr_dbd_column_t *);
                length = increment * (*results)->sz;
                row->columns = apr_palloc(pool, length);
                row->columnCount = column_count;
                for (i = 0; i < (*results)->sz; i++) {
                    column = apr_palloc(pool, sizeof(apr_dbd_column_t));
                    row->columns[i] = column;
                    /* copy column name once only */
                    if ((*results)->col_names[i] == NULL) {
                      (*results)->col_names[i] =
                          apr_pstrdup(pool, sqlite3_column_name(stmt, i));
                    }
                    column->name = (*results)->col_names[i];
                    column->size = sqlite3_column_bytes(stmt, i);
                    column->type = sqlite3_column_type(stmt, i);
                    column->value = NULL;
                    switch (column->type) {
                    case SQLITE_FLOAT:
                    case SQLITE_INTEGER:
                    case SQLITE_TEXT:
                        hold = NULL;
                        hold = (char *) sqlite3_column_text(stmt, i);
                        if (hold) {
                            column->value = apr_palloc(pool, column->size + 1);
                            strncpy(column->value, hold, column->size + 1);
                        }
                        break;
                    case SQLITE_BLOB:
                        break;
                    case SQLITE_NULL:
                        break;
                    }
                    col = row->columns[i];
                }
                row->rownum = num_tuples++;
                row->next_row = 0;
                (*results)->tuples = num_tuples;
                if ((*results)->next_row == 0) {
                    (*results)->next_row = row;
                }
                if (lastrow != 0) {
                    lastrow->next_row = row;
                }
                lastrow = row;
            } else if (ret == SQLITE_DONE) {
                ret = SQLITE_OK;
            }
        } while (ret == SQLITE_ROW || ret == SQLITE_BUSY);
    }
    ret = sqlite3_finalize(stmt);
#if APR_HAS_THREADS
    apr_thread_mutex_unlock(sql->mutex);
#endif

    if (sql->trans) {
        sql->trans->errnum = ret;
    }
    return ret;
}

static int dbd_sqlite3_get_row(apr_pool_t *pool, apr_dbd_results_t *res,
                               apr_dbd_row_t **rowp, int rownum)
{
    int i = 0;

    if (rownum == -1) {
        *rowp = res->next_row;
        if (*rowp == 0)
            return -1;
        res->next_row = (*rowp)->next_row;
        return 0;
    }
    if (rownum > res->tuples) {
        return -1;
    }
    rownum--;
    *rowp = res->next_row;
    for (; *rowp != 0; i++, *rowp = (*rowp)->next_row) {
        if (i == rownum) {
            return 0;
        }
    }

    return -1;

}

static const char *dbd_sqlite3_get_entry(const apr_dbd_row_t *row, int n)
{
    apr_dbd_column_t *column;
    const char *value;
    if ((n < 0) || (n >= row->columnCount)) {
        return NULL;
    }
    column = row->columns[n];
    value = column->value;
    return value;
}

static const char *dbd_sqlite3_error(apr_dbd_t *sql, int n)
{
    return sqlite3_errmsg(sql->conn);
}

static int dbd_sqlite3_query(apr_dbd_t *sql, int *nrows, const char *query)
{
    sqlite3_stmt *stmt = NULL;
    const char *tail = NULL;
    int ret = -1, length = 0;

    if (sql->trans && sql->trans->errnum) {
        return sql->trans->errnum;
    }

    length = strlen(query);
#if APR_HAS_THREADS
    apr_thread_mutex_lock(sql->mutex);
#endif

    do {
        int retry_count = 0;

        ret = sqlite3_prepare(sql->conn, query, length, &stmt, &tail);
        if (ret != SQLITE_OK) {
            sqlite3_finalize(stmt);
            break;
        }

        while(retry_count++ <= MAX_RETRY_COUNT) {
            ret = sqlite3_step(stmt);
            if (ret != SQLITE_BUSY)
                break;

#if APR_HAS_THREADS
            apr_thread_mutex_unlock(sql->mutex);
#endif
            apr_sleep(MAX_RETRY_SLEEP);
#if APR_HAS_THREADS
            apr_thread_mutex_lock(sql->mutex);
#endif
        }

        *nrows = sqlite3_changes(sql->conn);
        sqlite3_finalize(stmt);
        length -= (tail - query);
        query = tail;
    } while (length > 0);

    if (dbd_sqlite3_is_success(ret)) {
        ret = 0;
    }
#if APR_HAS_THREADS
    apr_thread_mutex_unlock(sql->mutex);
#endif
    if (sql->trans) {
        sql->trans->errnum = ret;
    }
    return ret;
}

static apr_status_t free_mem(void *data)
{
    sqlite3_free(data);
    return APR_SUCCESS;
}

static const char *dbd_sqlite3_escape(apr_pool_t *pool, const char *arg,
                                      apr_dbd_t *sql)
{
    char *ret = sqlite3_mprintf("%q", arg);
    apr_pool_cleanup_register(pool, ret, free_mem,
                              apr_pool_cleanup_null);
    return ret;
}

static int dbd_sqlite3_prepare(apr_pool_t *pool, apr_dbd_t *sql,
                               const char *query, const char *label,
                               apr_dbd_prepared_t **statement)
{
    sqlite3_stmt *stmt;
    char *p, *slquery = apr_pstrdup(pool, query);
    const char *tail = NULL, *q;
    int ret;

    for (p = slquery, q = query; *q; ++q) {
        if (q[0] == '%') {
            if (isalpha(q[1])) {
                *p++ = '?';
                ++q;
            }
            else if (q[1] == '%') {
                /* reduce %% to % */
                *p++ = *q++;
            }
            else {
                *p++ = *q;
            }
        }
        else {
            *p++ = *q;
        }
    }
    *p = 0;

#if APR_HAS_THREADS
    apr_thread_mutex_lock(sql->mutex);
#endif

    ret = sqlite3_prepare(sql->conn, slquery, strlen(query), &stmt, &tail);
    if (ret == SQLITE_OK) {
        apr_dbd_prepared_t *prep; 

        prep = apr_pcalloc(sql->pool, sizeof(*prep));
        prep->stmt = stmt;
        prep->next = sql->prep;

        /* link new statement to the handle */
        sql->prep = prep;

        *statement = prep;
    } else {
        sqlite3_finalize(stmt);
    }
   
#if APR_HAS_THREADS
    apr_thread_mutex_unlock(sql->mutex);
#endif

    return ret;
}

static int dbd_sqlite3_pquery(apr_pool_t *pool, apr_dbd_t *sql,
                              int *nrows, apr_dbd_prepared_t *statement,
                              int nargs, const char **values)
{
    sqlite3_stmt *stmt = statement->stmt;
    int ret = -1, retry_count = 0, i;

    if (sql->trans && sql->trans->errnum) {
        return sql->trans->errnum;
    }

#if APR_HAS_THREADS
    apr_thread_mutex_lock(sql->mutex);
#endif

    ret = sqlite3_reset(stmt);
    if (ret == SQLITE_OK) {
        for (i=0; i < nargs; i++) {
            sqlite3_bind_text(stmt, i + 1, values[i], strlen(values[i]),
                              SQLITE_STATIC);
        }

        while(retry_count++ <= MAX_RETRY_COUNT) {
            ret = sqlite3_step(stmt);
            if (ret != SQLITE_BUSY)
                break;

#if APR_HAS_THREADS
            apr_thread_mutex_unlock(sql->mutex);
#endif
            apr_sleep(MAX_RETRY_SLEEP);
#if APR_HAS_THREADS
            apr_thread_mutex_lock(sql->mutex);
#endif
        }

        *nrows = sqlite3_changes(sql->conn);

        sqlite3_reset(stmt);
    }

    if (dbd_sqlite3_is_success(ret)) {
        ret = 0;
    }
#if APR_HAS_THREADS
    apr_thread_mutex_unlock(sql->mutex);
#endif
    if (sql->trans) {
        sql->trans->errnum = ret;
    }

    return ret;
}

static int dbd_sqlite3_pvquery(apr_pool_t *pool, apr_dbd_t *sql, int *nrows,
                               apr_dbd_prepared_t *statement, va_list args)
{
    const char **values;
    int i, nargs;

    if (sql->trans && sql->trans->errnum) {
        return sql->trans->errnum;
    }

    nargs = sqlite3_bind_parameter_count(statement->stmt);
    values = apr_palloc(pool, sizeof(*values) * nargs);

    for (i = 0; i < nargs; i++) {
        values[i] = apr_pstrdup(pool, va_arg(args, const char*));
    }

    return dbd_sqlite3_pquery(pool, sql, nrows, statement, nargs, values);
}

static int dbd_sqlite3_pselect(apr_pool_t *pool, apr_dbd_t *sql,
                               apr_dbd_results_t **results,
                               apr_dbd_prepared_t *statement, int seek,
                               int nargs, const char **values)
{
    sqlite3_stmt *stmt = statement->stmt;
    int i, ret, retry_count = 0;
    size_t num_tuples = 0;
    int increment = 0;
    apr_dbd_row_t *row = NULL;
    apr_dbd_row_t *lastrow = NULL;
    apr_dbd_column_t *column;
    char *hold = NULL;

    if (sql->trans && sql->trans->errnum) {
        return sql->trans->errnum;
    }

#if APR_HAS_THREADS
    apr_thread_mutex_lock(sql->mutex);
#endif

    ret = sqlite3_reset(stmt);
    if (ret == SQLITE_OK) {
        int column_count;

        for (i=0; i < nargs; i++) {
            sqlite3_bind_text(stmt, i + 1, values[i], strlen(values[i]),
                              SQLITE_STATIC);
        }

        column_count = sqlite3_column_count(stmt);
        if (!*results) {
            *results = apr_pcalloc(pool, sizeof(apr_dbd_results_t));
        }
        (*results)->stmt = stmt;
        (*results)->sz = column_count;
        (*results)->random = seek;
        (*results)->next_row = 0;
        (*results)->tuples = 0;
        (*results)->col_names = apr_pcalloc(pool,
                                            column_count * sizeof(char *));
        do {
            ret = sqlite3_step(stmt);
            if (ret == SQLITE_BUSY) {
                if (retry_count++ > MAX_RETRY_COUNT) {
                    ret = SQLITE_ERROR;
                } else {
#if APR_HAS_THREADS
                    apr_thread_mutex_unlock(sql->mutex);
#endif
                    apr_sleep(MAX_RETRY_SLEEP);
#if APR_HAS_THREADS
                    apr_thread_mutex_lock(sql->mutex);
#endif
                }
            } else if (ret == SQLITE_ROW) {
                int length;
                apr_dbd_column_t *col;
                row = apr_palloc(pool, sizeof(apr_dbd_row_t));
                row->res = *results;
                increment = sizeof(apr_dbd_column_t *);
                length = increment * (*results)->sz;
                row->columns = apr_palloc(pool, length);
                row->columnCount = column_count;
                for (i = 0; i < (*results)->sz; i++) {
                    column = apr_palloc(pool, sizeof(apr_dbd_column_t));
                    row->columns[i] = column;
                    /* copy column name once only */
                    if ((*results)->col_names[i] == NULL) {
                      (*results)->col_names[i] =
                          apr_pstrdup(pool, sqlite3_column_name(stmt, i));
                    }
                    column->name = (*results)->col_names[i];
                    column->size = sqlite3_column_bytes(stmt, i);
                    column->type = sqlite3_column_type(stmt, i);
                    column->value = NULL;
                    switch (column->type) {
                    case SQLITE_FLOAT:
                    case SQLITE_INTEGER:
                    case SQLITE_TEXT:
                        hold = NULL;
                        hold = (char *) sqlite3_column_text(stmt, i);
                        if (hold) {
                            column->value = apr_palloc(pool, column->size + 1);
                            strncpy(column->value, hold, column->size + 1);
                        }
                        break;
                    case SQLITE_BLOB:
                        break;
                    case SQLITE_NULL:
                        break;
                    }
                    col = row->columns[i];
                }
                row->rownum = num_tuples++;
                row->next_row = 0;
                (*results)->tuples = num_tuples;
                if ((*results)->next_row == 0) {
                    (*results)->next_row = row;
                }
                if (lastrow != 0) {
                    lastrow->next_row = row;
                }
                lastrow = row;
            } else if (ret == SQLITE_DONE) {
                ret = SQLITE_OK;
            }
        } while (ret == SQLITE_ROW || ret == SQLITE_BUSY);

        sqlite3_reset(stmt);
    }
#if APR_HAS_THREADS
    apr_thread_mutex_unlock(sql->mutex);
#endif

    if (sql->trans) {
        sql->trans->errnum = ret;
    }
    return ret;
}

static int dbd_sqlite3_pvselect(apr_pool_t *pool, apr_dbd_t *sql,
                                apr_dbd_results_t **results,
                                apr_dbd_prepared_t *statement, int seek,
                                va_list args)
{
    const char **values;
    int i, nargs;

    if (sql->trans && sql->trans->errnum) {
        return sql->trans->errnum;
    }

    nargs = sqlite3_bind_parameter_count(statement->stmt);
    values = apr_palloc(pool, sizeof(*values) * nargs);

    for (i = 0; i < nargs; i++) {
        values[i] = apr_pstrdup(pool, va_arg(args, const char*));
    }

    return dbd_sqlite3_pselect(pool, sql, results, statement,
                               seek, nargs, values);
}

static int dbd_sqlite3_start_transaction(apr_pool_t *pool,
                                         apr_dbd_t *handle,
                                         apr_dbd_transaction_t **trans)
{
    int ret = 0;
    int nrows = 0;

    ret = dbd_sqlite3_query(handle, &nrows, "BEGIN");
    if (!*trans) {
        *trans = apr_pcalloc(pool, sizeof(apr_dbd_transaction_t));
        (*trans)->handle = handle;
        handle->trans = *trans;
    }

    return ret;
}

static int dbd_sqlite3_end_transaction(apr_dbd_transaction_t *trans)
{
    int ret = -1; /* ending transaction that was never started is an error */
    int nrows = 0;

    if (trans) {
        if (trans->errnum) {
            trans->errnum = 0;
            ret = dbd_sqlite3_query(trans->handle, &nrows, "ROLLBACK");
        } else {
            ret = dbd_sqlite3_query(trans->handle, &nrows, "COMMIT");
        }
        trans->handle->trans = NULL;
    }

    return ret;
}

static apr_dbd_t *dbd_sqlite3_open(apr_pool_t *pool, const char *params)
{
    apr_dbd_t *sql = NULL;
    sqlite3 *conn = NULL;
    apr_status_t res;
    int sqlres;
    if (!params)
        return NULL;
    sqlres = sqlite3_open(params, &conn);
    if (sqlres != SQLITE_OK) {
        sqlite3_close(conn);
        return NULL;
    }
    /* should we register rand or power functions to the sqlite VM? */
    sql = apr_pcalloc(pool, sizeof(*sql));
    sql->conn = conn;
    sql->pool = pool;
    sql->trans = NULL;
#if APR_HAS_THREADS
    /* Create a mutex */
    res = apr_thread_mutex_create(&sql->mutex, APR_THREAD_MUTEX_DEFAULT,
                                  pool);
    if (res != APR_SUCCESS) {
        return NULL;
    }
#endif

    return sql;
}

static apr_status_t dbd_sqlite3_close(apr_dbd_t *handle)
{
    apr_dbd_prepared_t *prep = handle->prep;

    /* finalize all prepared statements, or we'll get SQLITE_BUSY on close */
    while (prep) {
        sqlite3_finalize(prep->stmt);
        prep = prep->next;
    }

    sqlite3_close(handle->conn);
#if APR_HAS_THREADS
    apr_thread_mutex_destroy(handle->mutex);
#endif
    return APR_SUCCESS;
}

static apr_status_t dbd_sqlite3_check_conn(apr_pool_t *pool,
                                           apr_dbd_t *handle)
{
    return (handle->conn != NULL) ? APR_SUCCESS : APR_EGENERAL;
}

static int dbd_sqlite3_select_db(apr_pool_t *pool, apr_dbd_t *handle,
                                 const char *name)
{
    return APR_ENOTIMPL;
}

static void *dbd_sqlite3_native(apr_dbd_t *handle)
{
    return handle->conn;
}

static int dbd_sqlite3_num_cols(apr_dbd_results_t *res)
{
    return res->sz;
}

static int dbd_sqlite3_num_tuples(apr_dbd_results_t *res)
{
    return res->tuples;
}

APU_DECLARE_DATA const apr_dbd_driver_t apr_dbd_sqlite3_driver = {
    "sqlite3",
    NULL,
    dbd_sqlite3_native,
    dbd_sqlite3_open,
    dbd_sqlite3_check_conn,
    dbd_sqlite3_close,
    dbd_sqlite3_select_db,
    dbd_sqlite3_start_transaction,
    dbd_sqlite3_end_transaction,
    dbd_sqlite3_query,
    dbd_sqlite3_select,
    dbd_sqlite3_num_cols,
    dbd_sqlite3_num_tuples,
    dbd_sqlite3_get_row,
    dbd_sqlite3_get_entry,
    dbd_sqlite3_error,
    dbd_sqlite3_escape,
    dbd_sqlite3_prepare,
    dbd_sqlite3_pvquery,
    dbd_sqlite3_pvselect,
    dbd_sqlite3_pquery,
    dbd_sqlite3_pselect,
};
#endif
