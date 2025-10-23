#ifndef SWITCH_TELNYX_KV_H
#define SWITCH_TELNYX_KV_H

#include "switch.h"

SWITCH_BEGIN_EXTERN_C

typedef switch_status_t (*switch_telnyx_kv_get_func)(const char* /*key*/, const char* /*namespace*/, char** /*value*/, void* /*user_data*/);
typedef switch_status_t (*switch_telnyx_kv_set_func)(const char* /*key*/, const char* /*value*/, const char* /*namespace*/, void* /*user_data*/);
typedef switch_status_t (*switch_telnyx_kv_set_ttl_func)(const char* /*key*/, const char* /*value*/, const char* /*namespace*/, uint32_t /*ttl_seconds*/, void* /*user_data*/);
typedef switch_status_t (*switch_telnyx_kv_delete_func)(const char* /*key*/, const char* /*namespace*/, void* /*user_data*/);

typedef struct {
	switch_telnyx_kv_get_func get;
    switch_telnyx_kv_set_func set;
    switch_telnyx_kv_set_ttl_func set_ttl;
    switch_telnyx_kv_delete_func del;
} switch_telnyx_kv_callbacks_t;

// KV API Methods
SWITCH_DECLARE(switch_status_t) switch_telnyx_kv_get(const char* key, const char* ns, char** value);
SWITCH_DECLARE(switch_status_t) switch_telnyx_kv_set(const char* key, const char* value, const char* ns);
SWITCH_DECLARE(switch_status_t) switch_telnyx_kv_set_ttl(const char* key, const char* value, const char* ns, uint32_t ttl_seconds);
SWITCH_DECLARE(switch_status_t) switch_telnyx_kv_delete(const char* key, const char* ns);

// KV API Methods with module selection
SWITCH_DECLARE(switch_status_t) switch_telnyx_kv_get_module(const char* module_name, const char* key, const char* ns, char** value);
SWITCH_DECLARE(switch_status_t) switch_telnyx_kv_set_module(const char* module_name, const char* key, const char* value, const char* ns);
SWITCH_DECLARE(switch_status_t) switch_telnyx_kv_set_ttl_module(const char* module_name, const char* key, const char* value, const char* ns, uint32_t ttl_seconds);
SWITCH_DECLARE(switch_status_t) switch_telnyx_kv_delete_module(const char* module_name, const char* key, const char* ns);

// Register KV Callbacks
SWITCH_DECLARE(void) switch_telnyx_kv_init(switch_memory_pool_t *pool);
SWITCH_DECLARE(void) switch_telnyx_kv_deinit(void);
SWITCH_DECLARE(void) switch_telnyx_kv_register_callbacks(const char* module_name, switch_telnyx_kv_callbacks_t* callbacks, void* user_data);
SWITCH_DECLARE(void) switch_telnyx_kv_unregister_callbacks(const char* module_name);
SWITCH_DECLARE(switch_bool_t) switch_telnyx_kv_module_exists(const char* module_name);

SWITCH_END_EXTERN_C

#endif /* SWITCH_TELNYX_KV_H */

