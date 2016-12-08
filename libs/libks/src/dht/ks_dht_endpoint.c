#include "ks_dht.h"
#include "ks_dht-int.h"
#include "sodium.h"

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_endpoint_alloc(ks_dht_endpoint_t **endpoint, ks_pool_t *pool)
{
	ks_dht_endpoint_t *ep;

	ks_assert(endpoint);
	ks_assert(pool);
	
	*endpoint = ep = ks_pool_alloc(pool, sizeof(ks_dht_endpoint_t));
	ep->pool = pool;
	ep->sock = KS_SOCK_INVALID;

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_endpoint_prealloc(ks_dht_endpoint_t *endpoint, ks_pool_t *pool)
{
	ks_assert(endpoint);
	ks_assert(pool);

	endpoint->pool = pool;
	endpoint->sock = KS_SOCK_INVALID;

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_endpoint_free(ks_dht_endpoint_t *endpoint)
{
	ks_assert(endpoint);

	ks_dht_endpoint_deinit(endpoint);
	ks_pool_free(endpoint->pool, endpoint);

	return KS_STATUS_SUCCESS;
}


/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_endpoint_init(ks_dht_endpoint_t *endpoint, const ks_dht_nodeid_t *nodeid, const ks_sockaddr_t *addr, ks_socket_t sock)
{
	ks_assert(endpoint);
	ks_assert(endpoint->pool);
	ks_assert(addr);
	ks_assert(addr->family == AF_INET || addr->family == AF_INET6);
	
    if (!nodeid) {
		randombytes_buf(endpoint->nodeid.id, KS_DHT_NODEID_SIZE);
	} else {
		memcpy(endpoint->nodeid.id, nodeid->id, KS_DHT_NODEID_SIZE);
	}

	endpoint->addr = *addr;
	endpoint->sock = sock;

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_endpoint_deinit(ks_dht_endpoint_t *endpoint)
{
	ks_assert(endpoint);

	if (endpoint->sock != KS_SOCK_INVALID) {
		ks_socket_close(&endpoint->sock);
		endpoint->sock = KS_SOCK_INVALID;
	}

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
