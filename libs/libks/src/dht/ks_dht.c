#include "ks_dht.h"
#include "ks_dht-int.h"
#include "sodium.h"

KS_DECLARE(ks_status_t) ks_dht_alloc(ks_dht_t **dht, ks_pool_t *pool)
{
	ks_bool_t pool_alloc = !pool;
	ks_dht_t *d;

	ks_assert(dht);

	/**
	 * Create a new internally managed pool if one wasn't provided, and returns KS_STATUS_NO_MEM if pool was not created.
	 */
	if (pool_alloc) ks_pool_open(&pool);
	if (!pool) return KS_STATUS_NO_MEM;
	
	/**
	 * Allocate the dht instance from the pool, and returns KS_STATUS_NO_MEM if the dht was not created.
	 */
	*dht = d = ks_pool_alloc(pool, sizeof(ks_dht_t));
	if (!d) return KS_STATUS_NO_MEM;

	/**
	 * Keep track of the pool used for future allocations and cleanup.
	 * Keep track of whether the pool was created internally or not.
	 */
	d->pool = pool;
	d->pool_alloc = pool_alloc;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(void) ks_dht_prealloc(ks_dht_t *dht, ks_pool_t *pool)
{
	ks_assert(dht);
	ks_assert(pool);

	/**
	 * Treat preallocate function like allocate, zero the memory like pool allocations do.
	 */
	memset(dht, 0, sizeof(ks_dht_t));

	/**
	 * Keep track of the pool used for future allocations, pool must
	 */
	dht->pool = pool;
	dht->pool_alloc = KS_FALSE;
}

KS_DECLARE(ks_status_t) ks_dht_free(ks_dht_t **dht)
{
	ks_pool_t *pool = NULL;
	ks_bool_t pool_alloc = KS_FALSE;
	ks_status_t ret = KS_STATUS_SUCCESS;
	
	ks_assert(dht);
	ks_assert(*dht);

	/**
	 * Call ks_dht_deinit to ensure everything has been cleaned up internally.
	 * The pool member variables must not be messed with in deinit, they are managed at the allocator layer.
	 */
	if ((ret = ks_dht_deinit(*dht)) != KS_STATUS_SUCCESS) return ret;
	
	/**
	 * Temporarily store the allocator level variables because freeing the dht instance will invalidate it.
	 */
	pool = (*dht)->pool;
	pool_alloc = (*dht)->pool_alloc;
	
	/**
	 * Free the dht instance from the pool, after this the dht instance memory is invalid.
	 */
	if ((ret = ks_pool_free((*dht)->pool, *dht)) != KS_STATUS_SUCCESS) return ret;

	/**
	 * At this point dht instance is invalidated so NULL the pointer.
	 */
	*dht = NULL;

	/**
	 * If the pool was allocated internally, destroy it using the temporary variables stored earlier.
	 * If this fails, something catastrophically bad happened like memory corruption.
	 */
	if (pool_alloc && (ret = ks_pool_close(&pool)) != KS_STATUS_SUCCESS) return ret;
	

	return KS_STATUS_SUCCESS;
}
												

KS_DECLARE(ks_status_t) ks_dht_init(ks_dht_t *dht, ks_thread_pool_t *tpool)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	
	ks_assert(dht);
	ks_assert(dht->pool);

	/**
	 * Create a new internally managed thread pool if one wasn't provided.
	 */
	if (!tpool) {
		if ((ret = ks_thread_pool_create(&tpool,
										 KS_DHT_TPOOL_MIN,
										 KS_DHT_TPOOL_MAX,
										 KS_DHT_TPOOL_STACK,
										 KS_PRI_NORMAL,
										 KS_DHT_TPOOL_IDLE)) != KS_STATUS_SUCCESS) return ret;
		dht->tpool_alloc = KS_TRUE;
	}
	dht->tpool = tpool;

	/**
	 * Default autorouting to disabled.
	 */
	dht->autoroute = KS_FALSE;
	dht->autoroute_port = 0;

	/**
	 * Create the message type registry.
	 */
	if ((ret = ks_hash_create(&dht->registry_type,
							  KS_HASH_MODE_DEFAULT,
							  KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK,
							  dht->pool)) != KS_STATUS_SUCCESS) return ret;

	/**
	 * Register the message type callbacks for query (q), response (r), and error (e)
	 */
	ks_dht_register_type(dht, "q", ks_dht_process_query);
	ks_dht_register_type(dht, "r", ks_dht_process_response);
	ks_dht_register_type(dht, "e", ks_dht_process_error);

	/**
	 * Create the message query registry.
	 */
	if ((ret = ks_hash_create(&dht->registry_query,
							  KS_HASH_MODE_DEFAULT,
							  KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK,
							  dht->pool)) != KS_STATUS_SUCCESS) return ret;

	/**
	 * Register the message query callbacks for ping, find_node, etc.
	 */
	ks_dht_register_query(dht, "ping", ks_dht_process_query_ping);
	ks_dht_register_query(dht, "find_node", ks_dht_process_query_findnode);
	ks_dht_register_query(dht, "get", ks_dht_process_query_get);
	ks_dht_register_query(dht, "put", ks_dht_process_query_put);

	/**
	 * Create the message error registry.
	 */
	if ((ret = ks_hash_create(&dht->registry_error,
							  KS_HASH_MODE_DEFAULT,
							  KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK,
							  dht->pool)) != KS_STATUS_SUCCESS) return ret;
	// @todo register 301 error for internal get/put CAS hash mismatch retry handler

	/**
	 * Default these to FALSE, binding will set them TRUE when a respective address is bound.
	 * @todo these may not be useful anymore they are from legacy code
	 */
    dht->bind_ipv4 = KS_FALSE;
	dht->bind_ipv6 = KS_FALSE;

	/**
	 * Initialize the data used to track endpoints to NULL, binding will handle latent allocations.
	 * The endpoints and endpoints_poll arrays are maintained in parallel to optimize polling.
	 */
	dht->endpoints = NULL;
	dht->endpoints_size = 0;
	dht->endpoints_poll = NULL;

	/**
	 * Create the endpoints hash for fast lookup, this is used to route externally provided remote addresses when the local endpoint is unknown.
	 * This also provides the basis for autorouting to find unbound interfaces and bind them at runtime.
	 * This hash uses the host ip string concatenated with a colon and the port, ie: "123.123.123.123:123" or ipv6 equivilent
	 */
	if ((ret = ks_hash_create(&dht->endpoints_hash,
							  KS_HASH_MODE_DEFAULT,
							  KS_HASH_FLAG_RWLOCK,
							  dht->pool)) != KS_STATUS_SUCCESS) return ret;

	/**
	 * Default expirations to not be checked for one pulse.
	 */
	dht->pulse_expirations = ks_time_now_sec() + KS_DHT_PULSE_EXPIRATIONS;

	/**
	 * Create the queue for outgoing messages, this ensures sending remains async and can be throttled when system buffers are full.
	 */
	if ((ret = ks_q_create(&dht->send_q, dht->pool, 0)) != KS_STATUS_SUCCESS) return ret;
	
	/**
	 * If a message is popped from the queue for sending but the system buffers are too full, this is used to temporarily store the message.
	 */
	dht->send_q_unsent = NULL;

	/**
	 * The dht uses a single internal large receive buffer for receiving all frames, this may change in the future to offload processing to a threadpool.
	 */
	dht->recv_buffer_length = 0;

	/**
	 * Initialize the first transaction id randomly, this doesn't really matter.
	 */
	dht->transactionid_next = 1; //rand();

	/**
	 * Create the hash to track pending transactions on queries that are pending responses.
	 * It should be impossible to receive a duplicate transaction id in the hash before it expires, but if it does an error is preferred.
	 */
	if ((ret = ks_hash_create(&dht->transactions_hash,
							  KS_HASH_MODE_INT,
							  KS_HASH_FLAG_RWLOCK,
							  dht->pool)) != KS_STATUS_SUCCESS) return ret;

	/**
	 * The internal route tables will be latent allocated when binding.
	 */
	dht->rt_ipv4 = NULL;
	dht->rt_ipv6 = NULL;

	/**
	 * The opaque write tokens require some entropy for generating which needs to change periodically but accept tokens using the last two secrets.
	 */
	dht->token_secret_current = dht->token_secret_previous = rand();
	dht->token_secret_expiration = ks_time_now_sec() + KS_DHT_TOKENSECRET_EXPIRATION;

	/**
	 * Create the hash to store arbitrary data for BEP44.
	 */
	if ((ret = ks_hash_create(&dht->storage_hash,
							  KS_HASH_MODE_ARBITRARY,
							  KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK,
							  dht->pool)) != KS_STATUS_SUCCESS) return ret;
	/**
	 * The storage hash uses arbitrary key size, which requires the key size be provided, they are the same size as nodeid's.
	 */
	ks_hash_set_keysize(dht->storage_hash, KS_DHT_NODEID_SIZE);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_dht_deinit(ks_dht_t *dht)
{
	ks_hash_iterator_t *it;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(dht);

	/**
	 * Cleanup the storage hash and it's contents if it is allocated.
	 */
	if (dht->storage_hash) {
		for (it = ks_hash_first(dht->storage_hash, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
			const void *key;
			ks_dht_storageitem_t *val;
			ks_hash_this(it, &key, NULL, (void **)&val);
			if ((ret = ks_dht_storageitem_deinit(val)) != KS_STATUS_SUCCESS) return ret;
			if ((ret = ks_dht_storageitem_free(&val)) != KS_STATUS_SUCCESS) return ret;
		}
		ks_hash_destroy(&dht->storage_hash);
	}

	/**
	 * Zero out the opaque write token variables.
	 */
	dht->token_secret_current = 0;
	dht->token_secret_previous = 0;
	dht->token_secret_expiration = 0;

	/**
	 * Cleanup the route tables if they are allocated.
	 */
	if (dht->rt_ipv4) ks_dhtrt_deinitroute(&dht->rt_ipv4);
	if (dht->rt_ipv6) ks_dhtrt_deinitroute(&dht->rt_ipv6);

	/**
	 * Cleanup the transactions hash if it is allocated.
	 */
	dht->transactionid_next = 0;
	if (dht->transactions_hash) ks_hash_destroy(&dht->transactions_hash);

	/**
	 * Probably don't need this, recv_buffer_length is temporary and may change
	 */
	dht->recv_buffer_length = 0;

	/**
	 * Cleanup the send queue and it's contents if it is allocated.
	 */
	if (dht->send_q) {
		ks_dht_message_t *msg;
		while (ks_q_pop_timeout(dht->send_q, (void **)&msg, 1) == KS_STATUS_SUCCESS && msg) {
			if ((ret = ks_dht_message_deinit(msg)) != KS_STATUS_SUCCESS) return ret;
			if ((ret = ks_dht_message_free(&msg)) != KS_STATUS_SUCCESS) return ret;
		}
		if ((ret = ks_q_destroy(&dht->send_q)) != KS_STATUS_SUCCESS) return ret;
	}
	
	/**
	 * Cleanup the cached popped message if it is set.
	 */
	if (dht->send_q_unsent) {
		if ((ret = ks_dht_message_deinit(dht->send_q_unsent)) != KS_STATUS_SUCCESS) return ret;
		if ((ret = ks_dht_message_free(&dht->send_q_unsent)) != KS_STATUS_SUCCESS) return ret;
	}

	/**
	 * Probably don't need this
	 */
	dht->pulse_expirations = 0;

	/**
	 * Cleanup any endpoints that have been allocated.
	 */
	for (int32_t i = 0; i < dht->endpoints_size; ++i) {
		ks_dht_endpoint_t *ep = dht->endpoints[i];
		if ((ret = ks_dht_endpoint_deinit(ep)) != KS_STATUS_SUCCESS) return ret;
		if ((ret = ks_dht_endpoint_free(&ep)) != KS_STATUS_SUCCESS) return ret;
	}
	dht->endpoints_size = 0;

	/**
	 * Cleanup the array of endpoint pointers if it is allocated.
	 */
	if (dht->endpoints) {
		if ((ret = ks_pool_free(dht->pool, dht->endpoints)) != KS_STATUS_SUCCESS) return ret;
		dht->endpoints = NULL;
	}

	/**
	 * Cleanup the array of endpoint polling data if it is allocated.
	 */
	if (dht->endpoints_poll) {
		if ((ret = ks_pool_free(dht->pool, dht->endpoints_poll)) != KS_STATUS_SUCCESS) return ret;
		dht->endpoints_poll = NULL;
	}

	/**
	 * Cleanup the endpoints hash if it is allocated.
	 */
	if (dht->endpoints_hash) ks_hash_destroy(&dht->endpoints_hash);

	/**
	 * Probably don't need this
	 */
	dht->bind_ipv4 = KS_FALSE;
	dht->bind_ipv6 = KS_FALSE;

	/**
	 * Cleanup the type, query, and error registries if they have been allocated.
	 */
	if (dht->registry_type) ks_hash_destroy(&dht->registry_type);
	if (dht->registry_query) ks_hash_destroy(&dht->registry_query);
	if (dht->registry_error) ks_hash_destroy(&dht->registry_error);

	/**
	 * Probably don't need this
	 */
	dht->autoroute = KS_FALSE;
	dht->autoroute_port = 0;

	/**
	 * If the thread pool was allocated internally, destroy it.
	 * If this fails, something catastrophically bad happened like memory corruption.
	 */
	if (dht->tpool_alloc && (ret = ks_thread_pool_destroy(&dht->tpool)) != KS_STATUS_SUCCESS) return ret;
	dht->tpool_alloc = KS_FALSE;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(void) ks_dht_autoroute(ks_dht_t *dht, ks_bool_t autoroute, ks_port_t port)
{
	ks_assert(dht);

	/**
	 * If autorouting is being disabled, port is always set to zero, otherwise if the port is zero use the DHT default port
	 */
	if (!autoroute) port = 0;
    else if (port <= 0) port = KS_DHT_DEFAULT_PORT;

	/**
	 * Set the autoroute state
	 */
	dht->autoroute = autoroute;
	dht->autoroute_port = port;
}

KS_DECLARE(ks_status_t) ks_dht_autoroute_check(ks_dht_t *dht, ks_sockaddr_t *raddr, ks_dht_endpoint_t **endpoint)
{
	// @todo lookup standard def for IPV6 max size
	char ip[48 + 1];
	ks_dht_endpoint_t *ep = NULL;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(dht);
	ks_assert(raddr);
	ks_assert(endpoint);

	/**
	 * If the endpoint is already provided just leave it alone and return successfully.
	 */
	if (*endpoint) return KS_STATUS_SUCCESS;

	/**
	 * Use the remote address to figure out what local address we should use to attempt contacting it.
	 */
	if ((ret = ks_ip_route(ip, sizeof(ip), raddr->host)) != KS_STATUS_SUCCESS) return ret;

	/**
	 * Check if the endpoint has already been bound for the address we want to route through.
	 */
	ep = ks_hash_search(dht->endpoints_hash, ip, KS_READLOCKED);
	if ((ret = ks_hash_read_unlock(dht->endpoints_hash)) != KS_STATUS_SUCCESS) return ret;

	/**
	 * If the endpoint has not been bound, and autorouting is enabled then try to bind the new address.
	 */
	if (!ep && dht->autoroute) {
		ks_sockaddr_t addr;
		if ((ret = ks_addr_set(&addr, ip, dht->autoroute_port, raddr->family)) != KS_STATUS_SUCCESS) return ret;
		if ((ret = ks_dht_bind(dht, NULL, &addr, &ep)) != KS_STATUS_SUCCESS) return ret;
	}

	/**
	 * If no endpoint can be found to route through then all hope is lost, bail out with a failure.
	 */
	if (!ep) {
		ks_log(KS_LOG_DEBUG, "No route available to %s\n", raddr->host);
		return KS_STATUS_FAIL;
	}

	/**
	 * Reaching here means an endpoint is available, assign it and return successfully.
	 */
	*endpoint = ep;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_dht_register_type(ks_dht_t *dht, const char *value, ks_dht_message_callback_t callback)
{
	ks_assert(dht);
	ks_assert(value);
	ks_assert(callback);

	return ks_hash_insert(dht->registry_type, (void *)value, (void *)(intptr_t)callback) ? KS_STATUS_SUCCESS : KS_STATUS_FAIL;
}

KS_DECLARE(ks_status_t) ks_dht_register_query(ks_dht_t *dht, const char *value, ks_dht_message_callback_t callback)
{
	ks_assert(dht);
	ks_assert(value);
	ks_assert(callback);

	return ks_hash_insert(dht->registry_query, (void *)value, (void *)(intptr_t)callback) ? KS_STATUS_SUCCESS : KS_STATUS_FAIL;
}

KS_DECLARE(ks_status_t) ks_dht_register_error(ks_dht_t *dht, const char *value, ks_dht_message_callback_t callback)
{
	ks_assert(dht);
	ks_assert(value);
	ks_assert(callback);

	return ks_hash_insert(dht->registry_error, (void *)value, (void *)(intptr_t)callback) ? KS_STATUS_SUCCESS : KS_STATUS_FAIL;
}


KS_DECLARE(ks_status_t) ks_dht_bind(ks_dht_t *dht, const ks_dht_nodeid_t *nodeid, const ks_sockaddr_t *addr, ks_dht_endpoint_t **endpoint)
{
	ks_dht_endpoint_t *ep = NULL;
	ks_socket_t sock = KS_SOCK_INVALID;
	int32_t epindex = 0;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(dht);
	ks_assert(addr);
	ks_assert(addr->family == AF_INET || addr->family == AF_INET6);
	ks_assert(addr->port);

	/**
	 * If capturing the endpoint output, make sure it is set NULL to start with.
	 */
	if (endpoint) *endpoint = NULL;

	ep = ks_hash_search(dht->endpoints_hash, (void *)addr->host, KS_READLOCKED);
	if ((ret = ks_hash_read_unlock(dht->endpoints_hash)) != KS_STATUS_SUCCESS) return ret;
	if (ep) {
		ks_log(KS_LOG_DEBUG, "Attempted to bind to %s more than once.\n", addr->host);
		return KS_STATUS_FAIL;
	}

	/**
	 * Legacy code, this can probably go away
	 */
	dht->bind_ipv4 |= addr->family == AF_INET;
	dht->bind_ipv6 |= addr->family == AF_INET6;

	/**
	 * Attempt to open a UDP datagram socket for the given address family.
	 */
	if ((sock = socket(addr->family, SOCK_DGRAM, IPPROTO_UDP)) == KS_SOCK_INVALID) return KS_STATUS_FAIL;

	/**
	 * Set some common socket options for non-blocking IO and forced binding when already in use
	 */
	if ((ret = ks_socket_option(sock, SO_REUSEADDR, KS_TRUE)) != KS_STATUS_SUCCESS) goto done;
	if ((ret = ks_socket_option(sock, KS_SO_NONBLOCK, KS_TRUE)) != KS_STATUS_SUCCESS) goto done;
	
	/**
	 * Attempt to bind the socket to the desired local address.
	 */
	// @todo shouldn't ks_addr_bind take a const addr *?
	if ((ret = ks_addr_bind(sock, (ks_sockaddr_t *)addr)) != KS_STATUS_SUCCESS) goto done;

	/**
	 * Allocate the endpoint to track the local socket.
	 */
	if ((ret = ks_dht_endpoint_alloc(&ep, dht->pool)) != KS_STATUS_SUCCESS) goto done;

	/**
	 * Initialize the node, may provide NULL nodeid to have one generated internally.
	 */
	if ((ret = ks_dht_endpoint_init(ep, nodeid, addr, sock)) != KS_STATUS_SUCCESS) goto done;

	/**
	 * Resize the endpoints array to take another endpoint pointer.
	 */
	epindex = dht->endpoints_size++;
	dht->endpoints = (ks_dht_endpoint_t **)ks_pool_resize(dht->pool,
														   (void *)dht->endpoints,
														   sizeof(ks_dht_endpoint_t *) * dht->endpoints_size);
	dht->endpoints[epindex] = ep;

	/**
	 * Add the new endpoint into the endpoints hash for quick lookups.
	 */
	if (!ks_hash_insert(dht->endpoints_hash, ep->addr.host, ep)) {
		ret = KS_STATUS_FAIL;
		goto done;
	}

	/**
	 * Resize the endpoints_poll array to keep in parallel with endpoints array, populate new entry with the right data.
	 */
	dht->endpoints_poll = (struct pollfd *)ks_pool_resize(dht->pool,
														  (void *)dht->endpoints_poll,
														  sizeof(struct pollfd) * dht->endpoints_size);
	dht->endpoints_poll[epindex].fd = ep->sock;
	dht->endpoints_poll[epindex].events = POLLIN | POLLERR;

	/**
	 * If the route table for the family doesn't exist yet, initialize a new route table and create a local node for the endpoint.
	 */
	if (ep->addr.family == AF_INET) {
		if (!dht->rt_ipv4 && (ret = ks_dhtrt_initroute(&dht->rt_ipv4, dht->pool)) != KS_STATUS_SUCCESS) goto done;
		if ((ret = ks_dhtrt_create_node(dht->rt_ipv4,
										ep->nodeid,
										KS_DHT_LOCAL,
										ep->addr.host,
										ep->addr.port,
										&ep->node)) != KS_STATUS_SUCCESS) goto done;
		/**
		 * Do not release the ep->node, keep it alive until cleanup
		 */
	} else {
		if (!dht->rt_ipv6 && (ret = ks_dhtrt_initroute(&dht->rt_ipv6, dht->pool)) != KS_STATUS_SUCCESS) goto done;
		if ((ret = ks_dhtrt_create_node(dht->rt_ipv6,
										ep->nodeid,
										KS_DHT_LOCAL,
										ep->addr.host,
										ep->addr.port,
										&ep->node)) != KS_STATUS_SUCCESS) goto done;
		/**
		 * Do not release the ep->node, keep it alive until cleanup
		 */
	}

	/**
	 * If the endpoint output is being captured, assign it and return successfully.
	 */
	if (endpoint) *endpoint = ep;

	ret = KS_STATUS_SUCCESS;

 done:
	if (ret != KS_STATUS_SUCCESS) {
		/**
		 * If any failures occur, we need to make sure the socket is properly closed.
		 * This will be done in ks_dht_endpoint_deinit only if the socket was assigned during a successful ks_dht_endpoint_init.
		 * Then return whatever failure condition resulted in landed here.
		 */
		if (sock != KS_SOCK_INVALID && ep && ep->sock == KS_SOCK_INVALID) ks_socket_close(&sock);
		if (ep) {
			ks_dht_endpoint_deinit(ep);
			ks_dht_endpoint_free(&ep);
		}
	}
	return ret;
}

KS_DECLARE(void) ks_dht_pulse(ks_dht_t *dht, int32_t timeout)
{
	ks_dht_datagram_t *datagram = NULL;
	int32_t result;

	ks_assert(dht);
	ks_assert (timeout >= 0);

	// @todo why was old DHT code checking for poll descriptor resizing here?

	if (timeout == 0) {
		// @todo deal with default timeout, should return quickly but not hog the CPU polling
	}

	result = ks_poll(dht->endpoints_poll, dht->endpoints_size, timeout);
	if (result > 0) {
		for (int32_t i = 0; i < dht->endpoints_size; ++i) {
			if (dht->endpoints_poll[i].revents & POLLIN) {
				ks_sockaddr_t raddr = KS_SA_INIT;
				dht->recv_buffer_length = sizeof(dht->recv_buffer);

				raddr.family = dht->endpoints[i]->addr.family;
				if (ks_socket_recvfrom(dht->endpoints_poll[i].fd, dht->recv_buffer, &dht->recv_buffer_length, &raddr) == KS_STATUS_SUCCESS) {
					if (dht->recv_buffer_length == sizeof(dht->recv_buffer)) {
						ks_log(KS_LOG_DEBUG, "Dropped oversize datagram from %s %d\n", raddr.host, raddr.port);
					} else {
						// @todo check for recycled datagrams
						if (ks_dht_datagram_alloc(&datagram, dht->pool) == KS_STATUS_SUCCESS) {
							if (ks_dht_datagram_init(datagram, dht, dht->endpoints[i], &raddr) != KS_STATUS_SUCCESS) {
								// @todo add to recycled datagrams
								ks_dht_datagram_free(&datagram);
							} else if (ks_thread_pool_add_job(dht->tpool, ks_dht_process, datagram) != KS_STATUS_SUCCESS) {
								// @todo add to recycled datagrams
								ks_dht_datagram_deinit(datagram);
								ks_dht_datagram_free(&datagram);
							}
						}
					}
				}
			}
		}
	}

	ks_dht_pulse_expirations(dht);

	ks_dht_pulse_send(dht);

	if (dht->rt_ipv4) ks_dhtrt_process_table(dht->rt_ipv4);
	if (dht->rt_ipv6) ks_dhtrt_process_table(dht->rt_ipv6);
}

KS_DECLARE(void) ks_dht_pulse_expirations(ks_dht_t *dht)
{
	ks_hash_iterator_t *it = NULL;
	ks_time_t now = ks_time_now_sec();

	ks_assert(dht);

	if (dht->pulse_expirations <= now) {
		dht->pulse_expirations = now + KS_DHT_PULSE_EXPIRATIONS;
	}

	ks_hash_write_lock(dht->transactions_hash);
	for (it = ks_hash_first(dht->transactions_hash, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		const void *key = NULL;
		ks_dht_transaction_t *value = NULL;
		ks_bool_t remove = KS_FALSE;

		ks_hash_this(it, &key, NULL, (void **)&value);
		if (value->finished) remove = KS_TRUE;
		else if (value->expiration <= now) {
			ks_log(KS_LOG_DEBUG, "Transaction has expired without response %d\n", value->transactionid);
			remove = KS_TRUE;
		}
		if (remove) {
			ks_hash_remove(dht->transactions_hash, (char *)key);
			ks_pool_free(value->pool, value);
		}
	}
	ks_hash_write_unlock(dht->transactions_hash);

	if (dht->token_secret_expiration && dht->token_secret_expiration <= now) {
		dht->token_secret_expiration = ks_time_now_sec() + KS_DHT_TOKENSECRET_EXPIRATION;
		dht->token_secret_previous = dht->token_secret_current;
		dht->token_secret_current = rand();
	}
}

KS_DECLARE(void) ks_dht_pulse_send(ks_dht_t *dht)
{
	ks_dht_message_t *message;
	ks_bool_t bail = KS_FALSE;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(dht);

	while (!bail) {
		message = NULL;
		if (dht->send_q_unsent) {
			message = dht->send_q_unsent;
			dht->send_q_unsent = NULL;
		}
		if (!message) bail = ks_q_pop_timeout(dht->send_q, (void **)&message, 1) != KS_STATUS_SUCCESS || !message;
		if (!bail) {
			bail = (ret = ks_dht_send(dht, message)) != KS_STATUS_SUCCESS;
			if (ret == KS_STATUS_BREAK) dht->send_q_unsent = message;
			else if (ret == KS_STATUS_SUCCESS) {
				ks_dht_message_deinit(message);
				ks_dht_message_free(&message);
			}
		}
	}
}

KS_DECLARE(char *) ks_dht_hexid(ks_dht_nodeid_t *id, char *buffer)
{
	char *t = buffer;

	ks_assert(id);
	ks_assert(buffer);

	memset(buffer, 0, KS_DHT_NODEID_SIZE * 2 + 1);

	for (int i = 0; i < KS_DHT_NODEID_SIZE; ++i, t += 2) sprintf(t, "%02X", id->id[i]);

	return buffer;
}

KS_DECLARE(ks_status_t) ks_dht_utility_compact_addressinfo(const ks_sockaddr_t *address,
														   uint8_t *buffer,
														   ks_size_t *buffer_length,
														   ks_size_t buffer_size)
{
	ks_size_t addr_len;
	const void *paddr = NULL;
	uint16_t port = 0;

	ks_assert(address);
	ks_assert(buffer);
	ks_assert(buffer_length);
	ks_assert(buffer_size);
	ks_assert(address->family == AF_INET || address->family == AF_INET6);

	// @todo change parameters to dereferenced pointer and forward buffer pointer directly

	addr_len = address->family == AF_INET ? sizeof(uint32_t) : (sizeof(uint16_t) * 8);
	
	if (*buffer_length + addr_len + sizeof(uint16_t) > buffer_size) {
		ks_log(KS_LOG_DEBUG, "Insufficient space remaining for compacting\n");
		return KS_STATUS_NO_MEM;
	}

	if (address->family == AF_INET) {
		paddr = &address->v.v4.sin_addr; // already network byte order
		port = address->v.v4.sin_port; // already network byte order
	} else {
		paddr = &address->v.v6.sin6_addr; // already network byte order
		port = address->v.v6.sin6_port; // already network byte order
	}
	memcpy(buffer + (*buffer_length), paddr, sizeof(uint32_t));
	*buffer_length += addr_len;

	memcpy(buffer + (*buffer_length), (const void *)&port, sizeof(uint16_t));
	*buffer_length += sizeof(uint16_t);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_dht_utility_expand_addressinfo(const uint8_t *buffer,
														  ks_size_t *buffer_length,
														  ks_size_t buffer_size,
														  ks_sockaddr_t *address)
{
	ks_size_t addr_len;
	const void *paddr = NULL;
	uint16_t port = 0;

	ks_assert(buffer);
	ks_assert(buffer_length);
	ks_assert(address);
	ks_assert(address->family == AF_INET ||address->family == AF_INET6);

	// @todo change parameters to dereferenced pointer and forward buffer pointer directly

	addr_len = address->family == AF_INET ? sizeof(uint32_t) : (sizeof(uint16_t) * 8);
	if (*buffer_length + addr_len + sizeof(uint16_t) > buffer_size) return KS_STATUS_NO_MEM;

	paddr = buffer + *buffer_length;
	*buffer_length += addr_len;
	port = *((uint16_t *)(buffer + *buffer_length));
	*buffer_length += sizeof(uint16_t);

	// @todo ks_addr_set_raw second parameter should be const?
	return ks_addr_set_raw(address, (void *)paddr, port, address->family);
}

KS_DECLARE(ks_status_t) ks_dht_utility_compact_nodeinfo(const ks_dht_nodeid_t *nodeid,
														const ks_sockaddr_t *address,
														uint8_t *buffer,
														ks_size_t *buffer_length,
														ks_size_t buffer_size)
{
	ks_assert(address);
	ks_assert(buffer);
	ks_assert(buffer_length);
	ks_assert(buffer_size);
	ks_assert(address->family == AF_INET || address->family == AF_INET6);

	// @todo change parameters to dereferenced pointer and forward buffer pointer directly

	if (*buffer_length + KS_DHT_NODEID_SIZE > buffer_size) {
		ks_log(KS_LOG_DEBUG, "Insufficient space remaining for compacting\n");
		return KS_STATUS_NO_MEM;
	}

	memcpy(buffer + (*buffer_length), (void *)nodeid, KS_DHT_NODEID_SIZE);
	*buffer_length += KS_DHT_NODEID_SIZE;

	return ks_dht_utility_compact_addressinfo(address, buffer, buffer_length, buffer_size);
}

KS_DECLARE(ks_status_t) ks_dht_utility_expand_nodeinfo(const uint8_t *buffer,
													   ks_size_t *buffer_length,
													   ks_size_t buffer_size,
													   ks_dht_nodeid_t *nodeid,
													   ks_sockaddr_t *address)
{
	ks_assert(buffer);
	ks_assert(buffer_length);
	ks_assert(nodeid);
	ks_assert(address);
	ks_assert(address->family == AF_INET ||address->family == AF_INET6);

	// @todo change parameters to dereferenced pointer and forward buffer pointer directly

	if (*buffer_length + KS_DHT_NODEID_SIZE > buffer_size) return KS_STATUS_NO_MEM;

	memcpy(nodeid->id, buffer + *buffer_length, KS_DHT_NODEID_SIZE);
	*buffer_length += KS_DHT_NODEID_SIZE;

	return ks_dht_utility_expand_addressinfo(buffer, buffer_length, buffer_size, address);
}

KS_DECLARE(ks_status_t) ks_dht_utility_extract_nodeid(struct bencode *args, const char *key, ks_dht_nodeid_t **nodeid)
{
	struct bencode *id;
	const char *idv;
	ks_size_t idv_len;

	ks_assert(args);
	ks_assert(key);
	ks_assert(nodeid);

	*nodeid = NULL;

    id = ben_dict_get_by_str(args, key);
    if (!id) {
		ks_log(KS_LOG_DEBUG, "Message args missing key '%s'\n", key);
		return KS_STATUS_ARG_INVALID;
	}

    idv = ben_str_val(id);
	idv_len = ben_str_len(id);
    if (idv_len != KS_DHT_NODEID_SIZE) {
		ks_log(KS_LOG_DEBUG, "Message args '%s' value has an unexpected size of %d\n", key, idv_len);
		return KS_STATUS_ARG_INVALID;
	}

	*nodeid = (ks_dht_nodeid_t *)idv;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_dht_utility_extract_token(struct bencode *args, const char *key, ks_dht_token_t **token)
{
	struct bencode *tok;
	const char *tokv;
	ks_size_t tokv_len;

	ks_assert(args);
	ks_assert(key);
	ks_assert(token);

	*token = NULL;

    tok = ben_dict_get_by_str(args, key);
    if (!tok) {
		ks_log(KS_LOG_DEBUG, "Message args missing key '%s'\n", key);
		return KS_STATUS_ARG_INVALID;
	}

    tokv = ben_str_val(tok);
	tokv_len = ben_str_len(tok);
    if (tokv_len != KS_DHT_TOKEN_SIZE) {
		ks_log(KS_LOG_DEBUG, "Message args '%s' value has an unexpected size of %d\n", key, tokv_len);
		return KS_STATUS_ARG_INVALID;
	}

	*token = (ks_dht_token_t *)tokv;

	return KS_STATUS_SUCCESS;
}


KS_DECLARE(ks_status_t) ks_dht_token_generate(uint32_t secret, ks_sockaddr_t *raddr, ks_dht_nodeid_t *target, ks_dht_token_t *token)
{
	SHA_CTX sha;
	uint16_t port = 0;

	ks_assert(raddr);
	ks_assert(raddr->family == AF_INET || raddr->family == AF_INET6);
	ks_assert(target);
	ks_assert(token);

	secret = htonl(secret);
	port = htons(raddr->port);

	if (!SHA1_Init(&sha) ||
		!SHA1_Update(&sha, &secret, sizeof(uint32_t)) ||
		!SHA1_Update(&sha, raddr->host, strlen(raddr->host)) ||
		!SHA1_Update(&sha, &port, sizeof(uint16_t)) ||
		!SHA1_Update(&sha, target->id, KS_DHT_NODEID_SIZE) ||
		!SHA1_Final(token->token, &sha)) return KS_STATUS_FAIL;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_bool_t) ks_dht_token_verify(ks_dht_t *dht, ks_sockaddr_t *raddr, ks_dht_nodeid_t *target, ks_dht_token_t *token)
{
	ks_dht_token_t tok;

	if (ks_dht_token_generate(dht->token_secret_current, raddr, target, &tok) != KS_STATUS_SUCCESS) return KS_FALSE;

	if (memcmp(tok.token, token->token, KS_DHT_TOKEN_SIZE) == 0) return KS_TRUE;

	if (ks_dht_token_generate(dht->token_secret_previous, raddr, target, &tok) != KS_STATUS_SUCCESS) return KS_FALSE;

	return memcmp(tok.token, token->token, KS_DHT_TOKEN_SIZE) == 0;
}


KS_DECLARE(ks_status_t) ks_dht_send(ks_dht_t *dht, ks_dht_message_t *message)
{
	// @todo calculate max IPV6 payload size?
	char buf[1000];
	ks_size_t buf_len;

	ks_assert(dht);
	ks_assert(message);
	ks_assert(message->endpoint);
	ks_assert(message->data);

	// @todo blacklist check

	// @todo use different encode function to check if all data was encoded, do not send large incomplete messages
	buf_len = ben_encode2(buf, sizeof(buf), message->data);

	ks_log(KS_LOG_DEBUG, "Sending message to %s %d\n", message->raddr.host, message->raddr.port);
	ks_log(KS_LOG_DEBUG, "%s\n", ben_print(message->data));

	return ks_socket_sendto(message->endpoint->sock, (void *)buf, &buf_len, &message->raddr);
}


KS_DECLARE(ks_status_t) ks_dht_setup_query(ks_dht_t *dht,
										   ks_dht_endpoint_t *ep,
										   ks_sockaddr_t *raddr,
										   const char *query,
										   ks_dht_message_callback_t callback,
										   ks_dht_transaction_t **transaction,
										   ks_dht_message_t **message,
										   struct bencode **args)
{
	uint32_t transactionid;
	ks_dht_transaction_t *trans = NULL;
	ks_dht_message_t *msg = NULL;
	ks_status_t ret = KS_STATUS_FAIL;

	ks_assert(dht);
	ks_assert(raddr);
	ks_assert(query);
	ks_assert(callback);
	ks_assert(message);

	if (transaction) *transaction = NULL;
	*message = NULL;

	if (!ep && (ret = ks_dht_autoroute_check(dht, raddr, &ep)) != KS_STATUS_SUCCESS) return ret;

    // @todo atomic increment or mutex
	transactionid = dht->transactionid_next++;

	if ((ret = ks_dht_transaction_alloc(&trans, dht->pool)) != KS_STATUS_SUCCESS) goto done;

	if ((ret = ks_dht_transaction_init(trans, raddr, transactionid, callback)) != KS_STATUS_SUCCESS) goto done;

	if ((ret = ks_dht_message_alloc(&msg, dht->pool)) != KS_STATUS_SUCCESS) goto done;

	if ((ret = ks_dht_message_init(msg, ep, raddr, KS_TRUE)) != KS_STATUS_SUCCESS) goto done;

	if ((ret = ks_dht_message_query(msg, transactionid, query, args)) != KS_STATUS_SUCCESS) goto done;

	*message = msg;

	if (!ks_hash_insert(dht->transactions_hash, (void *)&trans->transactionid, trans)) {
		ret = KS_STATUS_FAIL;
		goto done;
	}

	if (transaction) *transaction = trans;

	ret = KS_STATUS_SUCCESS;

 done:
	if (ret != KS_STATUS_SUCCESS) {
		if (trans) {
			ks_dht_transaction_deinit(trans);
			ks_dht_transaction_free(&trans);
		}
		if (msg) {
			ks_dht_message_deinit(msg);
			ks_dht_message_free(&msg);
		}
		*message = NULL;
	}
	return ret;
}

KS_DECLARE(ks_status_t) ks_dht_setup_response(ks_dht_t *dht,
											  ks_dht_endpoint_t *ep,
											  ks_sockaddr_t *raddr,
											  uint8_t *transactionid,
											  ks_size_t transactionid_length,
											  ks_dht_message_t **message,
											  struct bencode **args)
{
	ks_dht_message_t *msg = NULL;
	ks_status_t ret = KS_STATUS_FAIL;

	ks_assert(dht);
	ks_assert(raddr);
	ks_assert(transactionid);
	ks_assert(message);

	*message = NULL;

	if (!ep && (ret = ks_dht_autoroute_check(dht, raddr, &ep)) != KS_STATUS_SUCCESS) return ret;

	if ((ret = ks_dht_message_alloc(&msg, dht->pool)) != KS_STATUS_SUCCESS) goto done;

	if ((ret = ks_dht_message_init(msg, ep, raddr, KS_TRUE)) != KS_STATUS_SUCCESS) goto done;

	if ((ret = ks_dht_message_response(msg, transactionid, transactionid_length, args)) != KS_STATUS_SUCCESS) goto done;

	*message = msg;

	ret = KS_STATUS_SUCCESS;

 done:
	if (ret != KS_STATUS_SUCCESS && msg) {
		ks_dht_message_deinit(msg);
		ks_dht_message_free(&msg);
		*message = NULL;
	}
	return ret;
}


KS_DECLARE(void *) ks_dht_process(ks_thread_t *thread, void *data)
{
	ks_dht_datagram_t *datagram = (ks_dht_datagram_t *)data;
	ks_dht_message_t message;
	ks_dht_message_callback_t callback;

	ks_assert(thread);
	ks_assert(data);

	ks_log(KS_LOG_DEBUG, "Received message from %s %d\n", datagram->raddr.host, datagram->raddr.port);
	if (datagram->raddr.family != AF_INET && datagram->raddr.family != AF_INET6) {
		ks_log(KS_LOG_DEBUG, "Message from unsupported address family\n");
		return NULL;
	}

	// @todo blacklist check for bad actor nodes

	if (ks_dht_message_prealloc(&message, datagram->dht->pool) != KS_STATUS_SUCCESS) return NULL;

	if (ks_dht_message_init(&message, datagram->endpoint, &datagram->raddr, KS_FALSE) != KS_STATUS_SUCCESS) return NULL;

	if (ks_dht_message_parse(&message, datagram->buffer, datagram->buffer_length) != KS_STATUS_SUCCESS) goto done;

	callback = (ks_dht_message_callback_t)(intptr_t)ks_hash_search(datagram->dht->registry_type, message.type, KS_READLOCKED);
	ks_hash_read_unlock(datagram->dht->registry_type);

	if (!callback) ks_log(KS_LOG_DEBUG, "Message type '%s' is not registered\n", message.type);
	else callback(datagram->dht, &message);

 done:
	ks_dht_message_deinit(&message);
	
	// @todo recycle datagram
	ks_dht_datagram_deinit(datagram);
	ks_dht_datagram_free(&datagram);

	return NULL;
}

KS_DECLARE(ks_status_t) ks_dht_process_(ks_dht_t *dht, ks_dht_endpoint_t *ep, ks_sockaddr_t *raddr)
{
	ks_dht_message_t message;
	ks_dht_message_callback_t callback;
	ks_status_t ret = KS_STATUS_FAIL;

	ks_assert(dht);
	ks_assert(raddr);

	ks_log(KS_LOG_DEBUG, "Received message from %s %d\n", raddr->host, raddr->port);
	if (raddr->family != AF_INET && raddr->family != AF_INET6) {
		ks_log(KS_LOG_DEBUG, "Message from unsupported address family\n");
		return KS_STATUS_FAIL;
	}

	// @todo blacklist check for bad actor nodes

	if (ks_dht_message_prealloc(&message, dht->pool) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	if (ks_dht_message_init(&message, ep, raddr, KS_FALSE) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	if (ks_dht_message_parse(&message, dht->recv_buffer, dht->recv_buffer_length) != KS_STATUS_SUCCESS) goto done;

	callback = (ks_dht_message_callback_t)(intptr_t)ks_hash_search(dht->registry_type, message.type, KS_READLOCKED);
	ks_hash_read_unlock(dht->registry_type);

	if (!callback) ks_log(KS_LOG_DEBUG, "Message type '%s' is not registered\n", message.type);
	else ret = callback(dht, &message);

 done:
	ks_dht_message_deinit(&message);

	return ret;
}

KS_DECLARE(ks_status_t) ks_dht_process_query(ks_dht_t *dht, ks_dht_message_t *message)
{
	struct bencode *q;
	struct bencode *a;
	const char *qv;
	ks_size_t qv_len;
	char query[KS_DHT_MESSAGE_QUERY_MAX_SIZE];
	ks_dht_message_callback_t callback;
	ks_status_t ret = KS_STATUS_FAIL;

	ks_assert(dht);
	ks_assert(message);

	// @todo start of ks_dht_message_parse_query
    q = ben_dict_get_by_str(message->data, "q");
    if (!q) {
		ks_log(KS_LOG_DEBUG, "Message query missing required key 'q'\n");
		return KS_STATUS_FAIL;
	}

    qv = ben_str_val(q);
	qv_len = ben_str_len(q);
    if (qv_len >= KS_DHT_MESSAGE_QUERY_MAX_SIZE) {
		ks_log(KS_LOG_DEBUG, "Message query 'q' value has an unexpectedly large size of %d\n", qv_len);
		return KS_STATUS_FAIL;
	}

	memcpy(query, qv, qv_len);
	query[qv_len] = '\0';
	ks_log(KS_LOG_DEBUG, "Message query is '%s'\n", query);

	a = ben_dict_get_by_str(message->data, "a");
	if (!a) {
		ks_log(KS_LOG_DEBUG, "Message query missing required key 'a'\n");
		return KS_STATUS_FAIL;
	}
	// @todo end of ks_dht_message_parse_query

	message->args = a;

	callback = (ks_dht_message_callback_t)(intptr_t)ks_hash_search(dht->registry_query, query, KS_READLOCKED);
	ks_hash_read_unlock(dht->registry_query);

	if (!callback) ks_log(KS_LOG_DEBUG, "Message query '%s' is not registered\n", query);
	else ret = callback(dht, message);

	return ret;
}

KS_DECLARE(ks_status_t) ks_dht_process_response(ks_dht_t *dht, ks_dht_message_t *message)
{
	struct bencode *r;
	ks_dht_transaction_t *transaction;
	uint32_t *tid;
	uint32_t transactionid;
	ks_status_t ret = KS_STATUS_FAIL;

	ks_assert(dht);
	ks_assert(message);

	// @todo start of ks_dht_message_parse_response
	r = ben_dict_get_by_str(message->data, "r");
	if (!r) {
		ks_log(KS_LOG_DEBUG, "Message response missing required key 'r'\n");
		return KS_STATUS_FAIL;
	}
	// @todo end of ks_dht_message_parse_response

	message->args = r;

	tid = (uint32_t *)message->transactionid;
	transactionid = ntohl(*tid);

	transaction = ks_hash_search(dht->transactions_hash, (void *)&transactionid, KS_READLOCKED);
	ks_hash_read_unlock(dht->transactions_hash);

	if (!transaction) ks_log(KS_LOG_DEBUG, "Message response rejected with unknown transaction id %d\n", transactionid);
	else if (!ks_addr_cmp(&message->raddr, &transaction->raddr)) {
		ks_log(KS_LOG_DEBUG,
			   "Message response rejected due to spoofing from %s %d, expected %s %d\n",
			   message->raddr.host,
			   message->raddr.port,
			   transaction->raddr.host,
			   transaction->raddr.port);
	} else {
		ret = transaction->callback(dht, message);
		transaction->finished = KS_TRUE;
	}

	return ret;
}


KS_DECLARE(ks_status_t) ks_dht_search(ks_dht_t *dht,
									  int family,
									  ks_dht_nodeid_t *target,
									  ks_dht_search_callback_t callback,
									  ks_dht_search_t **search)
{
	ks_dht_search_t *s = NULL;
	ks_status_t ret = KS_STATUS_SUCCESS;
	ks_bool_t inserted = KS_FALSE;
	ks_bool_t allocated = KS_FALSE;
    ks_dhtrt_querynodes_t query;

	ks_assert(dht);
	ks_assert(family == AF_INET || family == AF_INET6);
	ks_assert(target);

	if (search) *search = NULL;

	// check hash for target to see if search already exists
	s = ks_hash_search(dht->search_hash, target->id, KS_READLOCKED);
	ks_hash_read_unlock(dht->search_hash);

	// if search does not exist, create new search and store in hash by target
	if (!s) {
		if ((ret = ks_dht_search_alloc(&s, dht->pool)) != KS_STATUS_SUCCESS) goto done;
		if ((ret = ks_dht_search_init(s, target)) != KS_STATUS_SUCCESS) goto done;
		allocated = KS_TRUE;
	} else inserted = KS_TRUE;

	// add callback regardless of whether the search is new or old
	if ((ret = ks_dht_search_callback_add(s, callback)) != KS_STATUS_SUCCESS) goto done;

	// if the search is old then bail out and return successfully
	if (!allocated) goto done;

	// find closest good nodes to target locally and store as the closest results
    query.nodeid = *target;
	query.type = KS_DHT_REMOTE;
	query.max = KS_DHT_SEARCH_RESULTS_MAX_SIZE;
	query.family = family;
	ks_dhtrt_findclosest_nodes(family == AF_INET ? dht->rt_ipv4 : dht->rt_ipv6, &query);
	for (int32_t i = 0; i < query.count; ++i) {
		ks_dht_node_t *n = query.nodes[i];
		ks_dht_search_pending_t *pending = NULL;
		s->results[i] = n;
		// add to pending with expiration
		if ((ret = ks_dht_search_pending_alloc(&pending, s->pool)) != KS_STATUS_SUCCESS) goto done;
		if ((ret = ks_dht_search_pending_init(pending, n)) != KS_STATUS_SUCCESS) {
			ks_dht_search_pending_free(&pending);
			goto done;
		}
		if (!ks_hash_insert(s->pending, n->nodeid.id, n)) {
			ks_dht_search_pending_deinit(pending);
			ks_dht_search_pending_free(&pending);
			goto done;
		}
		// @todo call send_findnode, but transactions need to track the target id from a find_node query since find_node response does not contain it
	}
	s->results_length = query.count;
	
	if (!ks_hash_insert(dht->search_hash, s->target.id, s)) {
		ret = KS_STATUS_FAIL;
		goto done;
	}
	inserted = KS_TRUE;

	if (search) *search = s;
	ret = KS_STATUS_SUCCESS;

 done:
	if (ret != KS_STATUS_SUCCESS && !inserted && s) {
		ks_dht_search_deinit(s);
		ks_dht_search_free(&s);
	}
	return ret;
}


KS_DECLARE(ks_status_t) ks_dht_send_error(ks_dht_t *dht,
										  ks_dht_endpoint_t *ep,
										  ks_sockaddr_t *raddr,
										  uint8_t *transactionid,
										  ks_size_t transactionid_length,
										  long long errorcode,
										  const char *errorstr)
{
	ks_dht_message_t *error = NULL;
	struct bencode *e = NULL;
	ks_status_t ret = KS_STATUS_FAIL;

	ks_assert(dht);
	ks_assert(raddr);
	ks_assert(transactionid);
	ks_assert(errorstr);

	if (!ep && ks_dht_autoroute_check(dht, raddr, &ep) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	if (ks_dht_message_alloc(&error, dht->pool) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	if (ks_dht_message_init(error, ep, raddr, KS_TRUE) != KS_STATUS_SUCCESS) goto done;

	if (ks_dht_message_error(error, transactionid, transactionid_length, &e) != KS_STATUS_SUCCESS) goto done;

	ben_list_append(e, ben_int(errorcode));
	ben_list_append(e, ben_blob(errorstr, strlen(errorstr)));

	ks_log(KS_LOG_DEBUG, "Sending message error %d\n", errorcode);
	ks_q_push(dht->send_q, (void *)error);

	ret = KS_STATUS_SUCCESS;

 done:
	if (ret != KS_STATUS_SUCCESS && error) {
		ks_dht_message_deinit(error);
		ks_dht_message_free(&error);
	}
	return ret;
}

KS_DECLARE(ks_status_t) ks_dht_process_error(ks_dht_t *dht, ks_dht_message_t *message)
{
	struct bencode *e;
	struct bencode *ec;
	struct bencode *es;
	const char *et;
	ks_size_t es_len;
	long long errorcode;
	char error[KS_DHT_MESSAGE_ERROR_MAX_SIZE];
	ks_dht_transaction_t *transaction;
	uint32_t *tid;
	uint32_t transactionid;
	ks_status_t ret = KS_STATUS_FAIL;

	ks_assert(dht);
	ks_assert(message);

	// @todo start of ks_dht_message_parse_error
	e = ben_dict_get_by_str(message->data, "e");
	if (!e) {
		ks_log(KS_LOG_DEBUG, "Message error missing required key 'e'\n");
		return KS_STATUS_FAIL;
	}
	ec = ben_list_get(e, 0);
	es = ben_list_get(e, 1);
	es_len = ben_str_len(es);
	if (es_len >= KS_DHT_MESSAGE_ERROR_MAX_SIZE) {
		ks_log(KS_LOG_DEBUG, "Message error value has an unexpectedly large size of %d\n", es_len);
		return KS_STATUS_FAIL;
	}
	errorcode = ben_int_val(ec);
	et = ben_str_val(es);

	memcpy(error, et, es_len);
	error[es_len] = '\0';
	// @todo end of ks_dht_message_parse_error

	message->args = e;

	tid = (uint32_t *)message->transactionid;
	transactionid = ntohl(*tid);

	transaction = ks_hash_search(dht->transactions_hash, (void *)&transactionid, KS_READLOCKED);
	ks_hash_read_unlock(dht->transactions_hash);

	if (!transaction) {
		ks_log(KS_LOG_DEBUG, "Message error rejected with unknown transaction id %d\n", transactionid);
	} else if (!ks_addr_cmp(&message->raddr, &transaction->raddr)) {
		ks_log(KS_LOG_DEBUG,
			   "Message error rejected due to spoofing from %s %d, expected %s %d\n",
			   message->raddr.host,
			   message->raddr.port,
			   transaction->raddr.host,
			   transaction->raddr.port);
	} else {
		ks_dht_message_callback_t callback;
		transaction->finished = KS_TRUE;

		callback = (ks_dht_message_callback_t)(intptr_t)ks_hash_search(dht->registry_error, error, KS_READLOCKED);
		ks_hash_read_unlock(dht->registry_error);

		if (callback) ret = callback(dht, message);
		else {
			ks_log(KS_LOG_DEBUG, "Message error received for transaction id %d, error %d: %s\n", transactionid, errorcode, error);
			ret = KS_STATUS_SUCCESS;
		}
	}

	return ret;
}


KS_DECLARE(ks_status_t) ks_dht_send_ping(ks_dht_t *dht, ks_dht_endpoint_t *ep, ks_sockaddr_t *raddr)
{
	ks_dht_message_t *message = NULL;
	struct bencode *a = NULL;

	ks_assert(dht);
	ks_assert(raddr);

	if (ks_dht_setup_query(dht,
						   ep,
						   raddr,
						   "ping",
						   ks_dht_process_response_ping,
						   NULL,
						   &message,
						   &a) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	ben_dict_set(a, ben_blob("id", 2), ben_blob(message->endpoint->nodeid.id, KS_DHT_NODEID_SIZE));

	ks_log(KS_LOG_DEBUG, "Sending message query ping\n");
	ks_q_push(dht->send_q, (void *)message);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_dht_process_query_ping(ks_dht_t *dht, ks_dht_message_t *message)
{
	ks_dht_nodeid_t *id;
	ks_dht_message_t *response = NULL;
	struct bencode *r = NULL;
	ks_dhtrt_routetable_t *routetable = NULL;
	ks_dht_node_t *node = NULL;
	char id_buf[KS_DHT_NODEID_SIZE * 2 + 1];

	ks_assert(dht);
	ks_assert(message);
	ks_assert(message->args);

	if (ks_dht_utility_extract_nodeid(message->args, "id", &id) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	routetable = message->endpoint->node->table;

	ks_log(KS_LOG_DEBUG, "Creating node %s\n", ks_dht_hexid(id, id_buf));
	if (ks_dhtrt_create_node(routetable, *id, KS_DHT_REMOTE, message->raddr.host, message->raddr.port, &node) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
	if (ks_dhtrt_release_node(node) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	ks_log(KS_LOG_DEBUG, "Message query ping is valid\n");

	if (ks_dht_setup_response(dht,
							  message->endpoint,
							  &message->raddr,
							  message->transactionid,
							  message->transactionid_length,
							  &response,
							  &r) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

	ben_dict_set(r, ben_blob("id", 2), ben_blob(response->endpoint->nodeid.id, KS_DHT_NODEID_SIZE));

	ks_log(KS_LOG_DEBUG, "Sending message response ping\n");
	ks_q_push(dht->send_q, (void *)response);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_dht_process_response_ping(ks_dht_t *dht, ks_dht_message_t *message)
{
	ks_dht_nodeid_t *id;
	ks_dhtrt_routetable_t *routetable = NULL;
	ks_dht_node_t *node = NULL;
	char id_buf[KS_DHT_NODEID_SIZE * 2 + 1];

	ks_assert(dht);
	ks_assert(message);

	if (ks_dht_utility_extract_nodeid(message->args, "id", &id) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	routetable = message->endpoint->node->table;

	ks_log(KS_LOG_DEBUG, "Creating node %s\n", ks_dht_hexid(id, id_buf));
	if (ks_dhtrt_create_node(routetable, *id, KS_DHT_REMOTE, message->raddr.host, message->raddr.port, &node) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
	if (ks_dhtrt_release_node(node) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
	
	ks_log(KS_LOG_DEBUG, "Touching node %s\n", ks_dht_hexid(id, id_buf));
	if (ks_dhtrt_touch_node(routetable, *id) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	ks_log(KS_LOG_DEBUG, "Message response ping is reached\n");

	return KS_STATUS_SUCCESS;
}


KS_DECLARE(ks_status_t) ks_dht_send_findnode(ks_dht_t *dht, ks_dht_endpoint_t *ep, ks_sockaddr_t *raddr, ks_dht_nodeid_t *targetid)
{
	ks_dht_transaction_t *transaction = NULL;
	ks_dht_message_t *message = NULL;
	struct bencode *a = NULL;

	ks_assert(dht);
	ks_assert(raddr);
	ks_assert(targetid);

	if (ks_dht_setup_query(dht,
						   ep,
						   raddr,
						   "find_node",
						   ks_dht_process_response_findnode,
						   &transaction,
						   &message,
						   &a) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	memcpy(transaction->target.id, targetid->id, KS_DHT_NODEID_SIZE);

	ben_dict_set(a, ben_blob("id", 2), ben_blob(message->endpoint->nodeid.id, KS_DHT_NODEID_SIZE));
	ben_dict_set(a, ben_blob("target", 6), ben_blob(targetid->id, KS_DHT_NODEID_SIZE));
	// @todo produce "want" value if both families are bound

	ks_log(KS_LOG_DEBUG, "Sending message query find_node\n");
	ks_q_push(dht->send_q, (void *)message);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_dht_process_query_findnode(ks_dht_t *dht, ks_dht_message_t *message)
{
	ks_dht_nodeid_t *id;
	ks_dht_nodeid_t *target;
	struct bencode *want;
	ks_bool_t want4 = KS_FALSE;
	ks_bool_t want6 = KS_FALSE;
	ks_dht_message_t *response = NULL;
	struct bencode *r = NULL;
	uint8_t buffer4[1000];
	uint8_t buffer6[1000];
	ks_size_t buffer4_length = 0;
	ks_size_t buffer6_length = 0;
	ks_dhtrt_routetable_t *routetable = NULL;
	ks_dht_node_t *node = NULL;
	ks_dhtrt_querynodes_t query;
	char id_buf[KS_DHT_NODEID_SIZE * 2 + 1];

	ks_assert(dht);
	ks_assert(message);
	ks_assert(message->args);

	if (ks_dht_utility_extract_nodeid(message->args, "id", &id) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	if (ks_dht_utility_extract_nodeid(message->args, "target", &target) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	want = ben_dict_get_by_str(message->args, "want");
	if (want) {
		// @todo use ben_list_for_each
		size_t want_len = ben_list_len(want);
		for (size_t i = 0; i < want_len; ++i) {
			struct bencode *iv = ben_list_get(want, i);
			if (!ben_cmp_with_str(iv, "n4") && dht->rt_ipv4) want4 = KS_TRUE;
			if (!ben_cmp_with_str(iv, "n6") && dht->rt_ipv6) want6 = KS_TRUE;
		}
	}

	if (!want4 && !want6) {
		want4 = message->raddr.family == AF_INET;
		want6 = message->raddr.family == AF_INET6;
	}

	routetable = message->endpoint->node->table;

	ks_log(KS_LOG_DEBUG, "Creating node %s\n", ks_dht_hexid(id, id_buf));
	if (ks_dhtrt_create_node(routetable, *id, KS_DHT_REMOTE, message->raddr.host, message->raddr.port, &node) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
	if (ks_dhtrt_release_node(node) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	ks_log(KS_LOG_DEBUG, "Message query find_node is valid\n");


	query.nodeid = *target;
	query.type = KS_DHT_REMOTE;
	query.max = 8; // should be like KS_DHTRT_BUCKET_SIZE
	if (want4) {
		query.family = AF_INET;
		ks_dhtrt_findclosest_nodes(routetable, &query);

		for (int32_t i = 0; i < query.count; ++i) {
			ks_dht_node_t *qn = query.nodes[i];

			if (ks_dht_utility_compact_nodeinfo(&qn->nodeid,
												&qn->addr,
												buffer4,
												&buffer4_length,
												sizeof(buffer4)) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

			ks_log(KS_LOG_DEBUG, "Compacted ipv4 nodeinfo for %s (%s %d)\n", ks_dht_hexid(&qn->nodeid, id_buf), qn->addr.host, qn->addr.port);
		}
	}
	if (want6) {
		query.family = AF_INET6;
		ks_dhtrt_findclosest_nodes(routetable, &query);

		for (int32_t i = 0; i < query.count; ++i) {
			ks_dht_node_t *qn = query.nodes[i];

			if (ks_dht_utility_compact_nodeinfo(&qn->nodeid,
												&qn->addr,
												buffer6,
												&buffer6_length,
												sizeof(buffer6)) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

			ks_log(KS_LOG_DEBUG, "Compacted ipv6 nodeinfo for %s (%s %d)\n", ks_dht_hexid(&qn->nodeid, id_buf), qn->addr.host, qn->addr.port);
		}
	}

	if (ks_dht_setup_response(dht,
							  message->endpoint,
							  &message->raddr,
							  message->transactionid,
							  message->transactionid_length,
							  &response,
							  &r) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	ben_dict_set(r, ben_blob("id", 2), ben_blob(response->endpoint->nodeid.id, KS_DHT_NODEID_SIZE));
	if (want4) ben_dict_set(r, ben_blob("nodes", 5), ben_blob(buffer4, buffer4_length));
	if (want6) ben_dict_set(r, ben_blob("nodes6", 6), ben_blob(buffer6, buffer6_length));

	ks_log(KS_LOG_DEBUG, "Sending message response find_node\n");
	ks_q_push(dht->send_q, (void *)response);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_dht_process_response_findnode(ks_dht_t *dht, ks_dht_message_t *message)
{
	ks_dht_nodeid_t *id;
	struct bencode *n;
	const uint8_t *nodes = NULL;
	const uint8_t *nodes6 = NULL;
	size_t nodes_size = 0;
	size_t nodes6_size = 0;
	size_t nodes_len = 0;
	size_t nodes6_len = 0;
	ks_dhtrt_routetable_t *routetable = NULL;
	ks_dht_node_t *node = NULL;
	char id_buf[KS_DHT_NODEID_SIZE * 2 + 1];

	ks_assert(dht);
	ks_assert(message);

	// @todo pass in the ks_dht_transaction_t from the original query, available one call higher, to get the target id for search updating
	// @todo make a utility function to produce a xor of two nodeid's for distance checks based on memcmp on the existing results and new response nodes
	// @todo lookup search by target from transaction, lookup responding node id in search pending hash, set entry to finished for purging
	// @todo check response nodes for closer nodes than results contain, skip duplicates, add pending and call send_findnode for new closer results

	if (ks_dht_utility_extract_nodeid(message->args, "id", &id) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	n = ben_dict_get_by_str(message->args, "nodes");
	if (n) {
		nodes = (const uint8_t *)ben_str_val(n);
		nodes_size = ben_str_len(n);
	}
	n = ben_dict_get_by_str(message->args, "nodes6");
	if (n) {
		nodes6 = (const uint8_t *)ben_str_val(n);
		nodes6_size = ben_str_len(n);
	}

	routetable = message->endpoint->node->table;

	ks_log(KS_LOG_DEBUG, "Creating node %s\n", ks_dht_hexid(id, id_buf));
	if (ks_dhtrt_create_node(routetable, *id, KS_DHT_REMOTE, message->raddr.host, message->raddr.port, &node) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
	if (ks_dhtrt_release_node(node) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	ks_log(KS_LOG_DEBUG, "Touching node %s\n", ks_dht_hexid(id, id_buf));
	if (ks_dhtrt_touch_node(routetable, *id) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	while (nodes_len < nodes_size) {
		ks_dht_nodeid_t nid;
		ks_sockaddr_t addr;

		addr.family = AF_INET;
		if (ks_dht_utility_expand_nodeinfo(nodes, &nodes_len, nodes_size, &nid, &addr) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

		ks_log(KS_LOG_DEBUG,
			   "Expanded ipv4 nodeinfo for %s (%s %d)\n",
			   ks_dht_hexid(&nid, id_buf),
			   addr.host,
			   addr.port);

		ks_log(KS_LOG_DEBUG, "Creating node %s\n", ks_dht_hexid(&nid, id_buf));
		ks_dhtrt_create_node(dht->rt_ipv4, nid, KS_DHT_REMOTE, addr.host, addr.port, &node);
		ks_dhtrt_release_node(node);
	}

	while (nodes6_len < nodes6_size) {
		ks_dht_nodeid_t nid;
		ks_sockaddr_t addr;

		addr.family = AF_INET6;
		if (ks_dht_utility_expand_nodeinfo(nodes6, &nodes6_len, nodes6_size, &nid, &addr) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

		ks_log(KS_LOG_DEBUG,
			   "Expanded ipv6 nodeinfo for %s (%s %d)\n",
			   ks_dht_hexid(&nid, id_buf),
			   addr.host,
			   addr.port);

		ks_log(KS_LOG_DEBUG, "Creating node %s\n", ks_dht_hexid(&nid, id_buf));
		ks_dhtrt_create_node(dht->rt_ipv6, nid, KS_DHT_REMOTE, addr.host, addr.port, &node);
		ks_dhtrt_release_node(node);
	}
	// @todo repeat above for ipv6 table

	ks_log(KS_LOG_DEBUG, "Message response find_node is reached\n");

	return KS_STATUS_SUCCESS;
}


KS_DECLARE(ks_status_t) ks_dht_send_get(ks_dht_t *dht, ks_dht_endpoint_t *ep, ks_sockaddr_t *raddr, ks_dht_nodeid_t *targetid)
{
	ks_dht_message_t *message = NULL;
	struct bencode *a = NULL;

	ks_assert(dht);
	ks_assert(raddr);
	ks_assert(targetid);

	if (ks_dht_setup_query(dht,
						   ep,
						   raddr,
						   "get",
						   ks_dht_process_response_get,
						   NULL,
						   &message,
						   &a) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	ben_dict_set(a, ben_blob("id", 2), ben_blob(message->endpoint->nodeid.id, KS_DHT_NODEID_SIZE));
	// @todo check for target item locally, set seq to item seq to prevent getting back what we already have if a newer seq is not available
	ben_dict_set(a, ben_blob("target", 6), ben_blob(targetid->id, KS_DHT_NODEID_SIZE));

	ks_log(KS_LOG_DEBUG, "Sending message query get\n");
	ks_q_push(dht->send_q, (void *)message);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_dht_process_query_get(ks_dht_t *dht, ks_dht_message_t *message)
{
	ks_dht_nodeid_t *id;
	ks_dht_nodeid_t *target;
	struct bencode *seq;
	int64_t sequence = -1;
	ks_bool_t sequence_snuffed = KS_FALSE;
	ks_dht_token_t token;
	ks_dht_storageitem_t *item = NULL;
	ks_dht_message_t *response = NULL;
	struct bencode *r = NULL;
	ks_dhtrt_routetable_t *routetable = NULL;
	ks_dht_node_t *node = NULL;
	char id_buf[KS_DHT_NODEID_SIZE * 2 + 1];

	ks_assert(dht);
	ks_assert(message);
	ks_assert(message->args);

	if (ks_dht_utility_extract_nodeid(message->args, "id", &id) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	if (ks_dht_utility_extract_nodeid(message->args, "target", &target) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	seq = ben_dict_get_by_str(message->args, "seq");
	if (seq) sequence = ben_int_val(seq);

	routetable = message->endpoint->node->table;

	ks_log(KS_LOG_DEBUG, "Creating node %s\n", ks_dht_hexid(id, id_buf));
	if (ks_dhtrt_create_node(routetable, *id, KS_DHT_REMOTE, message->raddr.host, message->raddr.port, &node) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
	if (ks_dhtrt_release_node(node) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	ks_log(KS_LOG_DEBUG, "Message query get is valid\n");

	ks_dht_token_generate(dht->token_secret_current, &message->raddr, target, &token);

	item = ks_hash_search(dht->storage_hash, (void *)target, KS_READLOCKED);
	ks_hash_read_unlock(dht->storage_hash);

	sequence_snuffed = item && sequence >= 0 && item->seq <= sequence;
	// @todo if sequence is provided then requester has the data so if the local sequence is lower, maybe create job to update local data from the requester?

	// @todo find closest ipv4 and ipv6 nodes to target

	// @todo compact ipv4 and ipv6 nodes into separate buffers

	if (ks_dht_setup_response(dht,
							  message->endpoint,
							  &message->raddr,
							  message->transactionid,
							  message->transactionid_length,
							  &response,
							  &r) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

	ben_dict_set(r, ben_blob("id", 2), ben_blob(response->endpoint->nodeid.id, KS_DHT_NODEID_SIZE));
	ben_dict_set(r, ben_blob("token", 5), ben_blob(token.token, KS_DHT_TOKEN_SIZE));
	if (item) {
		if (item->mutable) {
			if (!sequence_snuffed) {
				ben_dict_set(r, ben_blob("k", 1), ben_blob(item->pk.key, KS_DHT_STORAGEITEM_KEY_SIZE));
				ben_dict_set(r, ben_blob("sig", 3), ben_blob(item->sig.sig, KS_DHT_STORAGEITEM_SIGNATURE_SIZE));
			}
			ben_dict_set(r, ben_blob("seq", 3), ben_int(item->seq));
		}
		if (!sequence_snuffed) ben_dict_set(r, ben_blob("v", 1), ben_clone(item->v));
	}
	// @todo nodes, nodes6

	ks_log(KS_LOG_DEBUG, "Sending message response get\n");
	ks_q_push(dht->send_q, (void *)response);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_dht_process_response_get(ks_dht_t *dht, ks_dht_message_t *message)
{
	ks_dht_nodeid_t *id;
	ks_dht_token_t *token;
	ks_dhtrt_routetable_t *routetable = NULL;
	ks_dht_node_t *node = NULL;
	char id_buf[KS_DHT_NODEID_SIZE * 2 + 1];

	ks_assert(dht);
	ks_assert(message);

	// @todo use ks_dht_storageitem_mutable or ks_dht_storageitem_immutable if v is provided
	if (ks_dht_utility_extract_nodeid(message->args, "id", &id) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	if (ks_dht_utility_extract_token(message->args, "token", &token) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	// @todo add extract function for mutable ks_dht_storageitem_key_t
	// @todo add extract function for mutable ks_dht_storageitem_signature_t

	routetable = message->endpoint->node->table;

	ks_log(KS_LOG_DEBUG, "Creating node %s\n", ks_dht_hexid(id, id_buf));
	if (ks_dhtrt_create_node(routetable, *id, KS_DHT_REMOTE, message->raddr.host, message->raddr.port, &node) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
	if (ks_dhtrt_release_node(node) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	ks_log(KS_LOG_DEBUG, "Touching node %s\n", ks_dht_hexid(id, id_buf));
	if (ks_dhtrt_touch_node(routetable, *id) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
	// @todo add/touch bucket entries for other nodes/nodes6 returned

	ks_log(KS_LOG_DEBUG, "Message response get is reached\n");

	return KS_STATUS_SUCCESS;
}


// @todo ks_dht_send_put

KS_DECLARE(ks_status_t) ks_dht_process_query_put(ks_dht_t *dht, ks_dht_message_t *message)
{
	ks_dht_nodeid_t *id;
	ks_dht_message_t *response = NULL;
	struct bencode *r = NULL;
	ks_dhtrt_routetable_t *routetable = NULL;
	ks_dht_node_t *node = NULL;
	char id_buf[KS_DHT_NODEID_SIZE * 2 + 1];

	ks_assert(dht);
	ks_assert(message);
	ks_assert(message->args);

	if (ks_dht_utility_extract_nodeid(message->args, "id", &id) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	routetable = message->endpoint->node->table;

	ks_log(KS_LOG_DEBUG, "Creating node %s\n", ks_dht_hexid(id, id_buf));
	if (ks_dhtrt_create_node(routetable, *id, KS_DHT_REMOTE, message->raddr.host, message->raddr.port, &node) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
	if (ks_dhtrt_release_node(node) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	ks_log(KS_LOG_DEBUG, "Message query put is valid\n");

	if (ks_dht_setup_response(dht,
							  message->endpoint,
							  &message->raddr,
							  message->transactionid,
							  message->transactionid_length,
							  &response,
							  &r) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

	//ben_dict_set(r, ben_blob("id", 2), ben_blob(response->endpoint->nodeid.id, KS_DHT_NODEID_SIZE));

	ks_log(KS_LOG_DEBUG, "Sending message response put\n");
	ks_q_push(dht->send_q, (void *)response);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_dht_process_response_put(ks_dht_t *dht, ks_dht_message_t *message)
{
	ks_dht_nodeid_t *id;
	ks_dhtrt_routetable_t *routetable = NULL;
	ks_dht_node_t *node = NULL;
	char id_buf[KS_DHT_NODEID_SIZE * 2 + 1];

	ks_assert(dht);
	ks_assert(message);

	if (ks_dht_utility_extract_nodeid(message->args, "id", &id) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	routetable = message->endpoint->node->table;

	ks_log(KS_LOG_DEBUG, "Creating node %s\n", ks_dht_hexid(id, id_buf));
	if (ks_dhtrt_create_node(routetable, *id, KS_DHT_REMOTE, message->raddr.host, message->raddr.port, &node) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
	if (ks_dhtrt_release_node(node) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	ks_log(KS_LOG_DEBUG, "Touching node %s\n", ks_dht_hexid(id, id_buf));
	if (ks_dhtrt_touch_node(routetable, *id) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	ks_log(KS_LOG_DEBUG, "Message response put is reached\n");

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
