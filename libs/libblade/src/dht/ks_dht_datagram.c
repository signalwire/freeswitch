#include "ks_dht.h"
#include "ks_dht-int.h"
#include "sodium.h"

KS_DECLARE(ks_status_t) ks_dht_datagram_create(ks_dht_datagram_t **datagram,
											   ks_pool_t *pool,
											   ks_dht_t *dht,
											   ks_dht_endpoint_t *endpoint,
											   const ks_sockaddr_t *raddr)
{
	ks_dht_datagram_t *dg;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(datagram);
	ks_assert(pool);
	ks_assert(dht);
	ks_assert(endpoint);
	ks_assert(raddr);
	ks_assert(raddr->family == AF_INET || raddr->family == AF_INET6);

	*datagram = dg = ks_pool_alloc(pool, sizeof(ks_dht_datagram_t));
	ks_assert(dg);

	dg->pool = pool;
	dg->dht = dht;
	dg->endpoint = endpoint;
	dg->raddr = *raddr;

	memcpy(dg->buffer, dht->recv_buffer, dht->recv_buffer_length);
	dg->buffer_length = dht->recv_buffer_length;

	// done:
	if (ret != KS_STATUS_SUCCESS) {
		ks_dht_datagram_destroy(datagram);
	}
	return ret;
}

KS_DECLARE(void) ks_dht_datagram_destroy(ks_dht_datagram_t **datagram)
{
	ks_dht_datagram_t *dg;

	ks_assert(datagram);
	ks_assert(*datagram);

	dg = *datagram;

	ks_pool_free(dg->pool, datagram);
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
