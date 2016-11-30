#include "ks_dht.h"
#include "ks_dht-int.h"
#include "ks_dht_endpoint-int.h"
#include "sodium.h"

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_alloc(ks_dht2_t **dht, ks_pool_t *pool)
{
	ks_bool_t pool_alloc = !pool;
	ks_dht2_t *d;

	ks_assert(dht);
	
	if (pool_alloc) ks_pool_open(&pool);
	*dht = d = ks_pool_alloc(pool, sizeof(ks_dht2_t));

	d->pool = pool;
	d->pool_alloc = pool_alloc;

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_free(ks_dht2_t *dht)
{
	ks_pool_t *pool = dht->pool;
	ks_bool_t pool_alloc = dht->pool_alloc;

	ks_pool_free(pool, dht);
	if (pool_alloc) {
		ks_pool_close(&pool);
	}

	return KS_STATUS_SUCCESS;
}
												

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_init(ks_dht2_t *dht, const uint8_t *nodeid)
{
	ks_assert(dht);

	if (ks_dht2_nodeid_init(&dht->nodeid, nodeid) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}
	
    dht->bind_ipv4 = KS_FALSE;
	dht->bind_ipv6 = KS_FALSE;
	
	dht->endpoints = NULL;
	dht->endpoints_size = 0;
	ks_hash_create(&dht->endpoints_hash, KS_HASH_MODE_DEFAULT, KS_HASH_FLAG_RWLOCK, dht->pool);
	dht->endpoints_poll = NULL;
	
	dht->recv_buffer_length = 0;
	
	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_deinit(ks_dht2_t *dht)
{
	ks_assert(dht);

	dht->recv_buffer_length = 0;
	// @todo dht->endpoints_poll deinit
	// @todo dht->endpoints deinit
	ks_hash_destroy(&dht->endpoints_hash);
	dht->bind_ipv4 = KS_FALSE;
	dht->bind_ipv6 = KS_FALSE;
	ks_dht2_nodeid_deinit(&dht->nodeid);
	
	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_bind(ks_dht2_t *dht, const ks_sockaddr_t *addr)
{
	ks_dht2_endpoint_t *ep;
	ks_socket_t sock;
	int32_t epindex;
	
	ks_assert(dht);
	ks_assert(addr);
	ks_assert(addr->family == AF_INET || addr->family == AF_INET6);
	ks_assert(addr->port);
	
	//if (!addr->port) {
	//	addr->port = KS_DHT_DEFAULT_PORT;
	//}

	dht->bind_ipv4 |= addr->family == AF_INET;
	dht->bind_ipv6 |= addr->family == AF_INET6;

	// @todo start of ks_dht2_endpoint_bind
	if ((sock = socket(addr->family, SOCK_DGRAM, IPPROTO_UDP)) == KS_SOCK_INVALID) {
		return KS_STATUS_FAIL;
	}

	// @todo shouldn't ks_addr_bind take a const addr *?
	if (ks_addr_bind(sock, (ks_sockaddr_t *)addr) != KS_STATUS_SUCCESS) {
		ks_socket_close(&sock);
		return KS_STATUS_FAIL;
	}
	
	if (ks_dht2_endpoint_alloc(&ep, dht->pool) != KS_STATUS_SUCCESS) {
		ks_socket_close(&sock);
		return KS_STATUS_FAIL;
	}
	
	if (ks_dht2_endpoint_init(ep, addr, sock) != KS_STATUS_SUCCESS) {
		ks_dht2_endpoint_free(ep);
		ks_socket_close(&sock);
		return KS_STATUS_FAIL;
	}

	ks_socket_option(ep->sock, SO_REUSEADDR, KS_TRUE);
	ks_socket_option(ep->sock, KS_SO_NONBLOCK, KS_TRUE);

	// @todo end of ks_dht2_endpoint_bind
	
	epindex = dht->endpoints_size++;
	dht->endpoints = (ks_dht2_endpoint_t **)ks_pool_resize(dht->pool,
														   (void *)dht->endpoints,
														   sizeof(ks_dht2_endpoint_t *) * dht->endpoints_size);
	dht->endpoints[epindex] = ep;
	ks_hash_insert(dht->endpoints_hash, ep->addr.host, ep);
	
	dht->endpoints_poll = (struct pollfd *)ks_pool_resize(dht->pool,
														  (void *)dht->endpoints_poll,
														  sizeof(struct pollfd) * dht->endpoints_size);
	dht->endpoints_poll[epindex].fd = ep->sock;
	dht->endpoints_poll[epindex].events = POLLIN | POLLERR;
	
	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_pulse(ks_dht2_t *dht, int32_t timeout)
{
	int32_t result;
		
	ks_assert(dht);
	ks_assert (timeout >= 0);

	// @todo why was old DHT code checking for poll descriptor resizing here?

	if (timeout == 0) {
		// @todo deal with default timeout, should return quickly but not hog the CPU polling
	}
	
	result = ks_poll(dht->endpoints_poll, dht->endpoints_size, timeout);
	if (result < 0) {
		return KS_STATUS_FAIL;
	}

	if (result == 0) {
		ks_dht2_idle(dht);
		return KS_STATUS_TIMEOUT;
	}

	for (int32_t i = 0; i < dht->endpoints_size; ++i) {
		if (dht->endpoints_poll[i].revents & POLLIN) {
			ks_sockaddr_t raddr = KS_SA_INIT;
			dht->recv_buffer_length = KS_DHT_RECV_BUFFER_SIZE;
			
			raddr.family = dht->endpoints[i]->addr.family;
			if (ks_socket_recvfrom(dht->endpoints_poll[i].fd, dht->recv_buffer, &dht->recv_buffer_length, &raddr) == KS_STATUS_SUCCESS) {
				ks_dht2_process(dht, &raddr);
			}
		}
	}

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_idle(ks_dht2_t *dht)
{
	ks_assert(dht);

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_process(ks_dht2_t *dht, ks_sockaddr_t *raddr)
{
	ks_assert(dht);
	ks_assert(raddr);

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
