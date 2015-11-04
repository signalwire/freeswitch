
#ifndef MOD_HIREDIS_H
#define MOD_HIREDIS_H

#include <switch.h>
#include <hiredis/hiredis.h>

typedef struct mod_hiredis_global_s {
  switch_memory_pool_t *pool;
  switch_hash_t *profiles;
  uint8_t debug;
} mod_hiredis_global_t;

extern mod_hiredis_global_t mod_hiredis_globals;

typedef struct hiredis_connection_s {
  char *host;
  char *password;
  uint32_t port;
  redisContext *context;
  struct timeval timeout;

  struct hiredis_connection_s *next;
} hiredis_connection_t;

typedef struct hiredis_profile_s {
  switch_memory_pool_t *pool;
  char *name;
  int debug;

  hiredis_connection_t *conn;
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

switch_status_t mod_hiredis_do_config();
switch_status_t hiredis_profile_create(hiredis_profile_t **new_profile, char *name, uint8_t port);
switch_status_t hiredis_profile_destroy(hiredis_profile_t **old_profile);
switch_status_t hiredis_profile_connection_add(hiredis_profile_t *profile, char *host, char *password, uint32_t port, uint32_t timeout_ms);

switch_status_t hiredis_profile_execute_sync(hiredis_profile_t *profile, const char *data, char **response);

#endif /* MOD_HIREDIS_H */
