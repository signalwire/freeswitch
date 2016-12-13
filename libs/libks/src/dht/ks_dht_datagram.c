#include "ks_dht.h"
#include "ks_dht-int.h"
#include "sodium.h"

KS_DECLARE(ks_status_t) ks_dht_datagram_alloc(ks_dht_datagram_t **datagram, ks_pool_t *pool)
{
	ks_dht_datagram_t *dg;

	ks_assert(datagram);
	ks_assert(pool);
	
	*datagram = dg = ks_pool_alloc(pool, sizeof(ks_dht_datagram_t));
	dg->pool = pool;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(void) ks_dht_datagram_prealloc(ks_dht_datagram_t *datagram, ks_pool_t *pool)
{
	ks_assert(datagram);
	ks_assert(pool);

	memset(datagram, 0, sizeof(ks_dht_datagram_t));
	
	datagram->pool = pool;
}

KS_DECLARE(ks_status_t) ks_dht_datagram_free(ks_dht_datagram_t **datagram)
{
	ks_assert(datagram);
	ks_assert(*datagram);

	ks_dht_datagram_deinit(*datagram);
	ks_pool_free((*datagram)->pool, *datagram);

	*datagram = NULL;

	return KS_STATUS_SUCCESS;
}


KS_DECLARE(ks_status_t) ks_dht_datagram_init(ks_dht_datagram_t *datagram, ks_dht_t *dht, ks_dht_endpoint_t *endpoint, const ks_sockaddr_t *raddr)
{
	ks_assert(datagram);
	ks_assert(datagram->pool);
	ks_assert(dht);
	ks_assert(endpoint);
	ks_assert(raddr);
	ks_assert(raddr->family == AF_INET || raddr->family == AF_INET6);

	datagram->dht = dht;
	datagram->endpoint = endpoint;
	datagram->raddr = *raddr;

	memcpy(datagram->buffer, dht->recv_buffer, dht->recv_buffer_length);
	datagram->buffer_length = dht->recv_buffer_length;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_dht_datagram_deinit(ks_dht_datagram_t *datagram)
{
	ks_assert(datagram);

	datagram->buffer_length = 0;
	datagram->raddr = (const ks_sockaddr_t){ 0 };
	datagram->endpoint = NULL;
	datagram->dht = NULL;

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
