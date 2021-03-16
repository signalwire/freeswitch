#ifndef PROMETHEUS_METRICS_H
#define PROMETHEUS_METRICS_H

#include <switch.h>

SWITCH_BEGIN_EXTERN_C

void prometheus_init(switch_loadable_module_interface_t **module_interface, switch_api_interface_t *api_interface, switch_memory_pool_t* pool);
void prometheus_destroy();

void prometheus_increment_download_duration(unsigned int duration);
void prometheus_increment_download_fail_count();

SWITCH_END_EXTERN_C
#endif /* PROMETHEUS_METRICS_H */

