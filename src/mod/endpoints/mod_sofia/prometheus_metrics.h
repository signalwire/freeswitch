#ifndef PROMETHEUS_METRICS_H
#define PROMETHEUS_METRICS_H

#include <switch.h>

SWITCH_BEGIN_EXTERN_C

void prometheus_init(switch_loadable_module_interface_t **module_interface, switch_api_interface_t *api_interface, switch_memory_pool_t* pool);
void prometheus_destroy();

void prometheus_increment_call_counter();
void prometheus_increment_sip_terminated_counter(int status);
void prometheus_increment_dialplan_terminated_counter(int status);
void prometheus_increment_request_method(const char* method);
void prometheus_increment_outgoing_invite();
void prometheus_increment_incoming_new_invite();
void prometheus_increment_invite_retransmission();

SWITCH_END_EXTERN_C
#endif /* PROMETHEUS_METRICS_H */

