#include "kz_core_memory.h"

#define APR_DECLARE(type)            type
typedef int apr_status_t;

APR_DECLARE(void) apr_pool_cleanup_register(switch_memory_pool_t *p, const void *data,
                      apr_status_t (*plain_cleanup_fn)(void *data),
                      apr_status_t (*child_cleanup_fn)(void *data));

APR_DECLARE(void) apr_pool_destroy(switch_memory_pool_t *pool);

SWITCH_DECLARE(void) switch_pool_register_cleanup(switch_memory_pool_t *pool, const void *data,
                                                  switch_apr_status (*plain_cleanup_fn)(void *data),
                                                  switch_apr_status (*child_cleanup_fn)(void *data))
{
	apr_pool_cleanup_register(pool, data, plain_cleanup_fn, child_cleanup_fn);
}



SWITCH_DECLARE(switch_status_t) switch_core_perform_destroy_memory_pool_now(switch_memory_pool_t **pool, const char *file, const char *func, int line)
{
	switch_assert(pool != NULL);
	apr_pool_destroy(*pool);
	*pool = NULL;

	return SWITCH_STATUS_SUCCESS;
}
