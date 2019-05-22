
#ifndef MOD_HIREDIS_H
#define MOD_HIREDIS_H

#include <switch.h>
#include <hiredis/hiredis.h>

#define MOD_HIREDIS_MAX_ARGS 64

typedef struct mod_hiredis_global_s {
	switch_memory_pool_t *pool;
	switch_hash_t *profiles;
	switch_mutex_t *limit_pvt_mutex;
} mod_hiredis_global_t;

extern mod_hiredis_global_t mod_hiredis_globals;

typedef struct hiredis_request_s {
	char *request;
	char **response;
	int done;
	int do_eval;
	int num_keys;
	char *keys;
	char *session_uuid;
	switch_status_t status;
	switch_mutex_t *mutex;
	switch_thread_cond_t *cond;
	struct hiredis_request_s *next;
	size_t argc;
	char *argv[MOD_HIREDIS_MAX_ARGS];
} hiredis_request_t;

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
	uint8_t ignore_error;
	hiredis_connection_t *conn_head;

	switch_thread_rwlock_t *pipeline_lock;
	switch_queue_t *request_pool;
	switch_queue_t *active_requests;
	int pipeline_running;
	int max_pipelined_requests;

	int delete_when_zero;
} hiredis_profile_t;

typedef struct hiredis_limit_pvt_node_s {
	char *realm;
	char *resource;
	char *limit_key;
	int inc;
	int interval;
	struct hiredis_limit_pvt_node_s *next;
} hiredis_limit_pvt_node_t;

typedef struct hiredis_limit_pvt_s {
	switch_mutex_t *mutex;
	struct hiredis_limit_pvt_node_s *first;
} hiredis_limit_pvt_t;

switch_status_t mod_hiredis_do_config(void);
switch_status_t hiredis_profile_create(hiredis_profile_t **new_profile, char *name, uint8_t ignore_connect_fail, uint8_t ignore_error, int max_pipelined_requests, int delete_when_zero);
switch_status_t hiredis_profile_destroy(hiredis_profile_t **old_profile);
switch_status_t hiredis_profile_connection_add(hiredis_profile_t *profile, char *host, char *password, uint32_t port, uint32_t timeout_ms, uint32_t max_connections);
switch_status_t hiredis_profile_execute_requests(hiredis_profile_t *profile, switch_core_session_t *session, hiredis_request_t *requests);
switch_status_t hiredis_profile_execute_sync(hiredis_profile_t *profile, switch_core_session_t *session, char **response, const char *data);
switch_status_t hiredis_profile_execute_sync_printf(hiredis_profile_t *profile, switch_core_session_t *session, char **response, const char *data_format_string, ...);

void hiredis_pipeline_thread_start(hiredis_profile_t *profile);
void hiredis_pipeline_threads_stop(hiredis_profile_t *profile);
switch_status_t hiredis_profile_execute_pipeline_printf(hiredis_profile_t *profile, switch_core_session_t *session, char **response, const char *data_format_string, ...);
switch_status_t hiredis_profile_eval_pipeline(hiredis_profile_t *profile, switch_core_session_t *session, char **response, const char *script, int num_keys, const char *keys);

#endif /* MOD_HIREDIS_H */
