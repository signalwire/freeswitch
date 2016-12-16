#include "ks_dht.h"
#include "ks_dht-int.h"
#include "sodium.h"

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_endpoint_create(ks_dht_endpoint_t **endpoint,
											   ks_pool_t *pool,
											   const ks_dht_nodeid_t *nodeid,
											   const ks_sockaddr_t *addr,
											   ks_socket_t sock)
{
	ks_dht_endpoint_t *ep;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(endpoint);
	ks_assert(pool);
	ks_assert(addr);
	ks_assert(addr->family == AF_INET || addr->family == AF_INET6);

	*endpoint = ep = ks_pool_alloc(pool, sizeof(ks_dht_endpoint_t));
	ks_assert(ep);

	ep->pool = pool;
    if (!nodeid) randombytes_buf(ep->nodeid.id, KS_DHT_NODEID_SIZE);
	else memcpy(ep->nodeid.id, nodeid->id, KS_DHT_NODEID_SIZE);
	ep->addr = *addr;
	ep->sock = sock;

	// done:
	if (ret != KS_STATUS_SUCCESS) {
		if (ep) ks_dht_endpoint_destroy(&ep);
		*endpoint = NULL;
	}
	return ret;
}

/**
 *
 */
KS_DECLARE(void) ks_dht_endpoint_destroy(ks_dht_endpoint_t **endpoint)
{
	ks_dht_endpoint_t *ep;

	ks_assert(endpoint);
	ks_assert(*endpoint);

	ep = *endpoint;

	if (ep->sock != KS_SOCK_INVALID) ks_socket_close(&ep->sock);
	ks_pool_free(ep->pool, &ep);

	*endpoint = NULL;
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
