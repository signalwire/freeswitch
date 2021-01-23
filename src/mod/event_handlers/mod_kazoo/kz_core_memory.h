#pragma once

#include <switch.h>

#ifdef __cplusplus
#define KZ_BEGIN_EXTERN_C       extern "C" {
#define KZ_END_EXTERN_C         }
#else
#define KZ_BEGIN_EXTERN_C
#define KZ_END_EXTERN_C
#endif

typedef int switch_apr_status;
#define switch_core_destroy_memory_pool_now(p) switch_core_perform_destroy_memory_pool_now(p, __FILE__, __SWITCH_FUNC__, __LINE__);

KZ_BEGIN_EXTERN_C

SWITCH_DECLARE(void) switch_pool_register_cleanup(switch_memory_pool_t *pool, const void *data,
                                                  switch_apr_status (*plain_cleanup_fn)(void *data),
                                                  switch_apr_status (*child_cleanup_fn)(void *data));

SWITCH_DECLARE(switch_status_t) switch_core_perform_destroy_memory_pool_now(switch_memory_pool_t **pool, const char *file, const char *func, int line);

KZ_END_EXTERN_C
