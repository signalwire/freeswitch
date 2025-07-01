#ifndef PROMETHEUS_METRICS_H
#define PROMETHEUS_METRICS_H

#include <switch.h>
SWITCH_BEGIN_EXTERN_C

void prometheus_init(switch_loadable_module_interface_t **module_interface, switch_api_interface_t *api_interface, switch_memory_pool_t* pool);
void prometheus_destroy(void);

void prometheus_increment_tx_fax_success(void);
void prometheus_increment_tx_fax_failure(void);
void prometheus_increment_rx_fax_success(void);
void prometheus_increment_rx_fax_failure(void);
void prometheus_increment_gateway_fax_failure(void);
void prometheus_increment_gateway_fax_success(void);

SWITCH_END_EXTERN_C
#endif /* PROMETHEUS_METRICS_H */

