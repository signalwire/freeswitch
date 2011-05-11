#ifndef MOD_MONGO_H
#define MOD_MONGO_H


#include <client/dbclient.h>
#include <client/connpool.h>
#include <db/json.h>
#include <bson/bson.h>

using namespace mongo;

typedef struct {
  char *host;

  switch_size_t min_connections;
  switch_size_t max_connections;
  switch_size_t size;  
  switch_queue_t *connections;
  switch_mutex_t *mutex;
  switch_memory_pool_t *pool;

} mongo_connection_pool_t;


switch_status_t mongo_connection_pool_create(mongo_connection_pool_t **conn_pool, switch_size_t min_connections, switch_size_t max_connections,
					     const char *host);
void mongo_connection_pool_destroy(mongo_connection_pool_t **conn_pool);


DBClientConnection *mongo_connection_pool_get(mongo_connection_pool_t *conn_pool);
switch_status_t mongo_connection_pool_put(mongo_connection_pool_t *conn_pool, DBClientConnection *conn);


#endif

