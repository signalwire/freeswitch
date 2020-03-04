#pragma once

#include <switch.h>

#ifdef __cplusplus
#define KZ_BEGIN_EXTERN_C       extern "C" {
#define KZ_END_EXTERN_C         }
#else
#define KZ_BEGIN_EXTERN_C
#define KZ_END_EXTERN_C
#endif

KZ_BEGIN_EXTERN_C

void kz_check_set_profile_var(switch_channel_t *channel, char* var, char *val);

SWITCH_DECLARE(switch_status_t) kz_switch_core_merge_variables(switch_event_t *event);

SWITCH_DECLARE(switch_status_t) kz_switch_core_base_headers_for_expand(switch_event_t **event);

SWITCH_DECLARE(switch_status_t) kz_expand_api_execute(const char *cmd, const char *arg, switch_core_session_t *session, switch_stream_handle_t *stream);

SWITCH_DECLARE(char *) kz_event_expand_headers_check(switch_event_t *event, const char *in, switch_event_t *var_list, switch_event_t *api_list, uint32_t recur);

SWITCH_DECLARE(char *) kz_event_expand_headers(switch_event_t *event, const char *in);

SWITCH_DECLARE(char *) kz_event_expand_headers_pool(switch_memory_pool_t *pool, switch_event_t *event, char *val);

SWITCH_DECLARE(char *) kz_expand(const char *in, const char *uuid);

SWITCH_DECLARE(char *) kz_expand_pool(switch_memory_pool_t *pool, const char *in);

char* kz_switch_event_get_first_of(switch_event_t *event, const char *list[]);

SWITCH_DECLARE(switch_status_t) kz_switch_event_add_variable_name_printf(switch_event_t *event, switch_stack_t stack, const char *val, const char *fmt, ...);

SWITCH_DECLARE(switch_xml_t) kz_xml_child(switch_xml_t xml, const char *name);

void kz_xml_process(switch_xml_t cfg);
void kz_event_decode(switch_event_t *event);

char * kz_expand_vars(char *xml_str);
void kz_expand_headers(switch_event_t *resolver, switch_event_t *event);
void kz_expand_headers_self(switch_event_t *event);

char * kz_expand_vars_pool(char *xml_str, switch_memory_pool_t *pool);
switch_status_t kz_json_api(const char * command, cJSON *args, cJSON **res);

SWITCH_DECLARE(switch_status_t) kz_expand_json_to_event(cJSON *json, switch_event_t *event, char * prefix);

KZ_END_EXTERN_C
