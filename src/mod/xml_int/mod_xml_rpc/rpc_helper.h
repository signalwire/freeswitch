#ifndef RPC_HELPER_H
#define RPC_HELPER_H

#include <switch.h>

SWITCH_BEGIN_EXTERN_C

switch_bool_t is_resource_available(const char* comman, const char* api_str);
void set_min_idle_cpu_watermark(const char* idle_cpu);
void set_throttled_api_calls(const char* api);
SWITCH_END_EXTERN_C

#endif /* RPC_HELPER_H */

