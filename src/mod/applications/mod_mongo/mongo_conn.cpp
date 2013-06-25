/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Tamas Cseke <cstomi.levlist@gmail.com>
 *
 * mongo_conn.cpp -- MongoDB connection pool
 *
 */
#include <switch.h>
#include "mod_mongo.h"

  /*
    we could use the driver's connection pool,
    if we could set the max connections (PoolForHost::setMaxPerHost)

    ScopedDbConnection scoped_conn("host");
    DBClientConnection *conn = dynamic_cast< DBClientConnection* >(&scoped_conn.conn());
    scoped_conn.done();
  */

switch_status_t mongo_connection_create(DBClientBase **connection, const char *conn_str)
{
  DBClientBase *conn = NULL;
  string conn_string(conn_str), err_msg;
  ConnectionString cs = ConnectionString::parse(conn_string, err_msg);
  switch_status_t status = SWITCH_STATUS_FALSE;
 
  if (!cs.isValid()) {
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't parse url: %s\n", err_msg.c_str());
    return status;
  }

  try {
    conn = cs.connect(err_msg);
  } catch (DBException &e) {
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't connect to mongo [%s]: %s\n", conn_str, err_msg.c_str());
    return status;
  }

  if (conn) {
    *connection = conn;
    status = SWITCH_STATUS_SUCCESS;
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Connected to mongo [%s]\n", conn_str);
  }

  return status;
}

void mongo_connection_destroy(DBClientBase **conn) 
{
  switch_assert(*conn != NULL);
  delete *conn;

  *conn = NULL;
}

switch_status_t mongo_connection_pool_create(mongo_connection_pool_t **conn_pool, switch_size_t min_connections, switch_size_t max_connections,
					     const char *conn_str)
{
  switch_memory_pool_t *pool = NULL;
  switch_status_t status = SWITCH_STATUS_SUCCESS;
  mongo_connection_pool_t *cpool = NULL;
  DBClientBase *conn = NULL;

  if ((status = switch_core_new_memory_pool(&pool)) != SWITCH_STATUS_SUCCESS) {
    return status;
  }

  if (!(cpool = (mongo_connection_pool_t *)switch_core_alloc(pool, sizeof(mongo_connection_pool_t)))) {
    switch_goto_status(SWITCH_STATUS_MEMERR, done);
  }

  if ((status = switch_mutex_init(&cpool->mutex, SWITCH_MUTEX_NESTED, pool)) != SWITCH_STATUS_SUCCESS) {
    goto done;
  } 

  if ((status = switch_queue_create(&cpool->connections, max_connections, pool)) != SWITCH_STATUS_SUCCESS) {
    goto done;
  }

  cpool->min_connections = min_connections;
  cpool->max_connections = max_connections;
  cpool->conn_str = switch_core_strdup(pool, conn_str);
  
  cpool->pool = pool;

  for (cpool->size = 0; cpool->size < min_connections; cpool->size++) {

    if (mongo_connection_create(&conn, conn_str) == SWITCH_STATUS_SUCCESS) {
      mongo_connection_pool_put(cpool, conn, SWITCH_FALSE);
    } else {
      break;
    }
  }

 done:

  if (status == SWITCH_STATUS_SUCCESS) {
    *conn_pool = cpool;
  } else {
    switch_core_destroy_memory_pool(&pool);
  }

  return status;
}

void mongo_connection_pool_destroy(mongo_connection_pool_t **conn_pool)
{
  mongo_connection_pool_t *cpool = *conn_pool;
  void *data = NULL;

  switch_assert(cpool != NULL);

  while (switch_queue_trypop(cpool->connections, &data) == SWITCH_STATUS_SUCCESS) {
    mongo_connection_destroy((DBClientBase **)&data);
  }

  switch_mutex_destroy(cpool->mutex);
  switch_core_destroy_memory_pool(&cpool->pool);

  *conn_pool = NULL;
}


DBClientBase *mongo_connection_pool_get(mongo_connection_pool_t *conn_pool)
{
  DBClientBase *conn = NULL;
  void *data = NULL;

  switch_assert(conn_pool != NULL);

  switch_mutex_lock(conn_pool->mutex);

  if (switch_queue_trypop(conn_pool->connections, &data) == SWITCH_STATUS_SUCCESS) {
    conn = (DBClientBase *) data;
  } else if (mongo_connection_create(&conn, conn_pool->conn_str) == SWITCH_STATUS_SUCCESS) {
    if (++conn_pool->size > conn_pool->max_connections) {
      switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Connection pool is empty. You may want to increase 'max-connections'\n");
    }
  }

  switch_mutex_unlock(conn_pool->mutex);

#ifdef MONGO_POOL_DEBUG
  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "POOL get: size %d conn: %p\n", (int) switch_queue_size(conn_pool->connections), conn);
#endif

  return conn;
}

switch_status_t mongo_connection_pool_put(mongo_connection_pool_t *conn_pool, DBClientBase *conn, switch_bool_t destroy)
{
  switch_status_t status = SWITCH_STATUS_SUCCESS;

  switch_assert(conn_pool != NULL);
  switch_assert(conn != NULL);

  switch_mutex_lock(conn_pool->mutex);
  if (destroy || conn_pool->size > conn_pool->max_connections) {
#ifdef MONGO_POOL_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "POOL: Destroy connection %p\n", conn);
#endif
    mongo_connection_destroy(&conn);
    conn_pool->size--;
  } else {
#ifdef MONGO_POOL_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "POOL: push connection %p\n", conn);
#endif
    status = switch_queue_push(conn_pool->connections, conn);
  }

  switch_mutex_unlock(conn_pool->mutex);

#ifdef MONGO_POOL_DEBUG
  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "POOL: put size %d conn: %p\n", (int) switch_queue_size(conn_pool->connections), conn);
#endif

  return status;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
