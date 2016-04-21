
#ifndef MOD_HIREDIS_H
#define MOD_HIREDIS_H

#include <switch.h>
#include <hiredis/hiredis.h>

typedef struct mod_hiredis_global_s {
  switch_memory_pool_t *pool;
  switch_hash_t *profiles;
} mod_hiredis_global_t;

extern mod_hiredis_global_t mod_hiredis_globals;

typedef struct mod_hiredis_context_s {
  struct hiredis_connection_s *connection;
  redisContext *context;
} hiredis_context_t;

typedef struct hiredis_connection_s {
  char *host;
  char *password;
  uint32_t port;
  switch_interval_time_t timeout_us;
  struct timeval timeout;
  switch_memory_pool_t *pool;
  switch_queue_t *context_pool;

  struct hiredis_connection_s *next;
} hiredis_connection_t;

typedef struct hiredis_profile_s {
  switch_memory_pool_t *pool;
  char *name;
  uint8_t ignore_connect_fail;

  hiredis_connection_t *conn_head;
} hiredis_profile_t;

typedef struct hiredis_limit_pvt_s {
  char *realm;
  char *resource;
  char *limit_key;
  int inc;
  int interval;
  struct hiredis_limit_pvt_s *next;
} hiredis_limit_pvt_t;

switch_status_t mod_hiredis_do_config(void);
switch_status_t hiredis_profile_create(hiredis_profile_t **new_profile, char *name, uint8_t ignore_connect_fail);
switch_status_t hiredis_profile_destroy(hiredis_profile_t **old_profile);
switch_status_t hiredis_profile_connection_add(hiredis_profile_t *profile, char *host, char *password, uint32_t port, uint32_t timeout_ms, uint32_t max_connections);

switch_status_t hiredis_profile_execute_sync(hiredis_profile_t *profile, const char *data, char **response, switch_core_session_t *session);

#endif /* MOD_HIREDIS_H */
