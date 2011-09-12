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
 
  if (!cs.isValid()) {
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't parse url: %s\n", err_msg.c_str());
    return SWITCH_STATUS_GENERR;
  }

  try {
    conn = cs.connect(err_msg);
  } catch (DBException &e) {
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't connect to mongo [%s]: %s\n", conn_str, err_msg.c_str());
    return SWITCH_STATUS_GENERR;
  }

  *connection = conn;
  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Connected to mongo [%s]\n", conn_str);

  return SWITCH_STATUS_SUCCESS;
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
      mongo_connection_pool_put(cpool, conn);
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

switch_status_t mongo_connection_pool_put(mongo_connection_pool_t *conn_pool, DBClientBase *conn)
{
  switch_status_t status = SWITCH_STATUS_SUCCESS;

  switch_assert(conn_pool != NULL);
  switch_assert(conn != NULL);

  switch_mutex_lock(conn_pool->mutex);
  if (conn_pool->size > conn_pool->max_connections) {
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
