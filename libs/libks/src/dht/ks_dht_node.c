#include "ks_dht.h"
#include "ks_dht-int.h"
#include "sodium.h"

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_node_alloc(ks_dht_node_t **node, ks_pool_t *pool)
{
	ks_dht_node_t *n;

	ks_assert(node);
	ks_assert(pool);

	*node = n = ks_pool_alloc(pool, sizeof(ks_dht_node_t));
	n->pool = pool;

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_node_prealloc(ks_dht_node_t *node, ks_pool_t *pool)
{
	ks_assert(node);
	ks_assert(pool);
	
	node->pool = pool;

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_node_free(ks_dht_node_t *node)
{
	ks_assert(node);

	ks_dht_node_deinit(node);
	ks_pool_free(node->pool, node);

	return KS_STATUS_SUCCESS;
}


/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_node_init(ks_dht_node_t *node, const ks_dht_nodeid_t *id, const ks_sockaddr_t *addr)
{
	ks_assert(node);
	ks_assert(node->pool);
	ks_assert(id);
	ks_assert(addr);

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_node_deinit(ks_dht_node_t *node)
{
	ks_assert(node);

	return KS_STATUS_SUCCESS;
}


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
