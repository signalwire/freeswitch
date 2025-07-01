#ifndef PROMETHEUS_METRICS_H
#define PROMETHEUS_METRICS_H

#include <switch.h>

SWITCH_BEGIN_EXTERN_C

void prometheus_init(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t* pool);
void prometheus_destroy(void);

void prometheus_increment_cdr_counter(void);
void prometheus_increment_cdr_success(void);
void prometheus_increment_cdr_error(void);
void prometheus_increment_tmpcdr_move_success(void);
void prometheus_increment_tmpcdr_move_error(void);
void prometheus_increment_backup_cdr_success(void);
void prometheus_increment_backup_cdr_error(void);

SWITCH_END_EXTERN_C
#endif /* PROMETHEUS_METRICS_H */

