#include "ks_dht.h"
#include "ks_dht-int.h"
#include "sodium.h"

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_alloc(ks_dht_t **dht, ks_pool_t *pool)
{
	ks_bool_t pool_alloc = !pool;
	ks_dht_t *d;

	ks_assert(dht);
	
	if (pool_alloc) ks_pool_open(&pool);
	*dht = d = ks_pool_alloc(pool, sizeof(ks_dht_t));

	d->pool = pool;
	d->pool_alloc = pool_alloc;

	d->autoroute = KS_FALSE;
	d->autoroute_port = 0;
	d->registry_type = NULL;
	d->registry_query = NULL;
	d->registry_error = NULL;
	d->bind_ipv4 = KS_FALSE;
	d->bind_ipv6 = KS_FALSE;
	d->endpoints = NULL;
	d->endpoints_size = 0;
	d->endpoints_hash = NULL;
	d->endpoints_poll = NULL;
	d->send_q = NULL;
	d->send_q_unsent = NULL;
	d->recv_buffer_length = 0;
	d->transactionid_next = 0;
	d->transactions_hash = NULL;
	d->rt_ipv4 = NULL;
	d->rt_ipv6 = NULL;
	d->token_secret_current = 0;
	d->token_secret_previous = 0;
	d->token_secret_expiration = 0;
	d->storage_hash = NULL;

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_prealloc(ks_dht_t *dht, ks_pool_t *pool)
{
	ks_assert(dht);
	ks_assert(pool);

	dht->pool = pool;
	dht->pool_alloc = KS_FALSE;

	dht->autoroute = KS_FALSE;
	dht->autoroute_port = 0;
	dht->registry_type = NULL;
	dht->registry_query = NULL;
	dht->registry_error = NULL;
	dht->bind_ipv4 = KS_FALSE;
	dht->bind_ipv6 = KS_FALSE;
	dht->endpoints = NULL;
	dht->endpoints_size = 0;
	dht->endpoints_hash = NULL;
	dht->endpoints_poll = NULL;
	dht->send_q = NULL;
	dht->send_q_unsent = NULL;
	dht->recv_buffer_length = 0;
	dht->transactionid_next = 0;
	dht->transactions_hash = NULL;
	dht->rt_ipv4 = NULL;
	dht->rt_ipv6 = NULL;
	dht->token_secret_current = 0;
	dht->token_secret_previous = 0;
	dht->token_secret_expiration = 0;
	dht->storage_hash = NULL;
	
	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_free(ks_dht_t *dht)
{
	ks_pool_t *pool = dht->pool;
	ks_bool_t pool_alloc = dht->pool_alloc;

	ks_dht_deinit(dht);
	ks_pool_free(pool, dht);
	if (pool_alloc) {
		ks_pool_close(&pool);
	}

	return KS_STATUS_SUCCESS;
}
												

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_init(ks_dht_t *dht)
{
	ks_assert(dht);
	ks_assert(dht->pool);

	dht->autoroute = KS_FALSE;
	dht->autoroute_port = 0;
	
	ks_hash_create(&dht->registry_type, KS_HASH_MODE_DEFAULT, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK, dht->pool);
	ks_dht_register_type(dht, "q", ks_dht_process_query);
	ks_dht_register_type(dht, "r", ks_dht_process_response);
	ks_dht_register_type(dht, "e", ks_dht_process_error);

	ks_hash_create(&dht->registry_query, KS_HASH_MODE_DEFAULT, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK, dht->pool);
	ks_dht_register_query(dht, "ping", ks_dht_process_query_ping);
	ks_dht_register_query(dht, "find_node", ks_dht_process_query_findnode);
	ks_dht_register_query(dht, "get", ks_dht_process_query_get);
	ks_dht_register_query(dht, "put", ks_dht_process_query_put);

	ks_hash_create(&dht->registry_error, KS_HASH_MODE_DEFAULT, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK, dht->pool);
	// @todo register 301 error for internal get/put CAS hash mismatch retry handler
	
    dht->bind_ipv4 = KS_FALSE;
	dht->bind_ipv6 = KS_FALSE;
	
	dht->endpoints = NULL;
	dht->endpoints_size = 0;
	ks_hash_create(&dht->endpoints_hash, KS_HASH_MODE_DEFAULT, KS_HASH_FLAG_RWLOCK, dht->pool);
	dht->endpoints_poll = NULL;

	ks_q_create(&dht->send_q, dht->pool, 0);
	dht->send_q_unsent = NULL;
	dht->recv_buffer_length = 0;

	dht->transactionid_next = 1; //rand();
	ks_hash_create(&dht->transactions_hash, KS_HASH_MODE_INT, KS_HASH_FLAG_RWLOCK, dht->pool);

	dht->rt_ipv4 = NULL;
	dht->rt_ipv6 = NULL;

	dht->token_secret_current = dht->token_secret_previous = rand();
	dht->token_secret_expiration = ks_time_now_sec() + KS_DHT_TOKENSECRET_EXPIRATION;

	ks_hash_create(&dht->storage_hash, KS_HASH_MODE_ARBITRARY, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK, dht->pool);
	ks_hash_set_keysize(dht->storage_hash, KS_DHT_NODEID_SIZE);
	
	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_deinit(ks_dht_t *dht)
{
	ks_assert(dht);

	// @todo free storage_hash entries
	if (dht->storage_hash) {
		ks_hash_destroy(&dht->storage_hash);
		dht->storage_hash = NULL;
	}
	dht->token_secret_current = 0;
	dht->token_secret_previous = 0;
	dht->token_secret_expiration = 0;
	if (dht->rt_ipv4) {
		ks_dhtrt_deinitroute(dht->rt_ipv4);
		dht->rt_ipv4 = NULL;
	}
	if (dht->rt_ipv6) {
		ks_dhtrt_deinitroute(dht->rt_ipv6);
		dht->rt_ipv6 = NULL;
	}
	dht->transactionid_next = 0;
	if (dht->transactions_hash) {
		ks_hash_destroy(&dht->transactions_hash);
		dht->transactions_hash = NULL;
	}
	dht->recv_buffer_length = 0;
	if (dht->send_q) {
		ks_dht_message_t *msg;
		while (ks_q_pop_timeout(dht->send_q, (void **)&msg, 1) == KS_STATUS_SUCCESS && msg) {
			ks_dht_message_deinit(msg);
			ks_dht_message_free(msg);
		}
		ks_q_destroy(&dht->send_q);
		dht->send_q = NULL;
	}
	if (dht->send_q_unsent) {
		ks_dht_message_deinit(dht->send_q_unsent);
		ks_dht_message_free(dht->send_q_unsent);
		dht->send_q_unsent = NULL;
	}
	for (int32_t i = 0; i < dht->endpoints_size; ++i) {
		ks_dht_endpoint_t *ep = dht->endpoints[i];
		ks_dht_endpoint_deinit(ep);
		ks_dht_endpoint_free(ep);
	}
	dht->endpoints_size = 0;
	if (dht->endpoints) {
		ks_pool_free(dht->pool, dht->endpoints);
		dht->endpoints = NULL;
	}
	if (dht->endpoints_poll) {
		ks_pool_free(dht->pool, dht->endpoints_poll);
		dht->endpoints_poll = NULL;
	}
	if (dht->endpoints_hash) {
		ks_hash_destroy(&dht->endpoints_hash);
		dht->endpoints_hash = NULL;
	}
	dht->bind_ipv4 = KS_FALSE;
	dht->bind_ipv6 = KS_FALSE;

	if (dht->registry_type) {
		ks_hash_destroy(&dht->registry_type);
		dht->registry_type = NULL;
	}
	if (dht->registry_query) {
		ks_hash_destroy(&dht->registry_query);
		dht->registry_query = NULL;
	}
	if (dht->registry_error) {
		ks_hash_destroy(&dht->registry_error);
		dht->registry_error = NULL;
	}

	dht->autoroute = KS_FALSE;
	dht->autoroute_port = 0;
	
	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_autoroute(ks_dht_t *dht, ks_bool_t autoroute, ks_port_t port)
{
	ks_assert(dht);

	if (!autoroute) {
		port = 0;
	} else if (port <= 0) {
		port = KS_DHT_DEFAULT_PORT;
	}
	
	dht->autoroute = autoroute;
	dht->autoroute_port = port;
	
	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_autoroute_check(ks_dht_t *dht, ks_sockaddr_t *raddr, ks_dht_endpoint_t **endpoint)
{
	// @todo lookup standard def for IPV6 max size
	char ip[48];
	ks_dht_endpoint_t *ep = NULL;

	ks_assert(dht);
	ks_assert(raddr);
	ks_assert(endpoint);

	*endpoint = NULL;
	
    ks_ip_route(ip, sizeof(ip), raddr->host);

	// @todo readlock hash
	if (!(ep = ks_hash_search(dht->endpoints_hash, ip, KS_UNLOCKED)) && dht->autoroute) {
		ks_sockaddr_t addr;
		ks_addr_set(&addr, ip, dht->autoroute_port, raddr->family);
		if (ks_dht_bind(dht, NULL, &addr, &ep) != KS_STATUS_SUCCESS) {
			return KS_STATUS_FAIL;
		}
	}

	if (!ep) {
		ks_log(KS_LOG_DEBUG, "No route available to %s\n", raddr->host);
		return KS_STATUS_FAIL;
	}
	
	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_register_type(ks_dht_t *dht, const char *value, ks_dht_message_callback_t callback)
{
	ks_assert(dht);
	ks_assert(value);
	ks_assert(callback);
	// @todo writelock registry
	return ks_hash_insert(dht->registry_type, (void *)value, (void *)(intptr_t)callback) ? KS_STATUS_SUCCESS : KS_STATUS_FAIL;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_register_query(ks_dht_t *dht, const char *value, ks_dht_message_callback_t callback)
{
	ks_assert(dht);
	ks_assert(value);
	ks_assert(callback);
	// @todo writelock registry
	return ks_hash_insert(dht->registry_query, (void *)value, (void *)(intptr_t)callback) ? KS_STATUS_SUCCESS : KS_STATUS_FAIL;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_register_error(ks_dht_t *dht, const char *value, ks_dht_message_callback_t callback)
{
	ks_assert(dht);
	ks_assert(value);
	ks_assert(callback);
	// @todo writelock registry
	return ks_hash_insert(dht->registry_error, (void *)value, (void *)(intptr_t)callback) ? KS_STATUS_SUCCESS : KS_STATUS_FAIL;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_bind(ks_dht_t *dht, const ks_dht_nodeid_t *nodeid, const ks_sockaddr_t *addr, ks_dht_endpoint_t **endpoint)
{
	ks_dht_endpoint_t *ep;
	ks_socket_t sock;
	int32_t epindex;
	
	ks_assert(dht);
	ks_assert(addr);
	ks_assert(addr->family == AF_INET || addr->family == AF_INET6);
	ks_assert(addr->port);

	if (endpoint) {
		*endpoint = NULL;
	}

	dht->bind_ipv4 |= addr->family == AF_INET;
	dht->bind_ipv6 |= addr->family == AF_INET6;

	if ((sock = socket(addr->family, SOCK_DGRAM, IPPROTO_UDP)) == KS_SOCK_INVALID) {
		return KS_STATUS_FAIL;
	}

	// @todo shouldn't ks_addr_bind take a const addr *?
	if (ks_addr_bind(sock, (ks_sockaddr_t *)addr) != KS_STATUS_SUCCESS) {
		ks_socket_close(&sock);
		return KS_STATUS_FAIL;
	}
	
	if (ks_dht_endpoint_alloc(&ep, dht->pool) != KS_STATUS_SUCCESS) {
		ks_socket_close(&sock);
		return KS_STATUS_FAIL;
	}
	
	if (ks_dht_endpoint_init(ep, nodeid, addr, sock) != KS_STATUS_SUCCESS) {
		ks_dht_endpoint_free(ep);
		ks_socket_close(&sock);
		return KS_STATUS_FAIL;
	}

	ks_socket_option(ep->sock, SO_REUSEADDR, KS_TRUE);
	ks_socket_option(ep->sock, KS_SO_NONBLOCK, KS_TRUE);

	
	epindex = dht->endpoints_size++;
	dht->endpoints = (ks_dht_endpoint_t **)ks_pool_resize(dht->pool,
														   (void *)dht->endpoints,
														   sizeof(ks_dht_endpoint_t *) * dht->endpoints_size);
	dht->endpoints[epindex] = ep;
	ks_hash_insert(dht->endpoints_hash, ep->addr.host, ep);
	
	dht->endpoints_poll = (struct pollfd *)ks_pool_resize(dht->pool,
														  (void *)dht->endpoints_poll,
														  sizeof(struct pollfd) * dht->endpoints_size);
	dht->endpoints_poll[epindex].fd = ep->sock;
	dht->endpoints_poll[epindex].events = POLLIN | POLLERR;

	// @todo initialize or add local nodeid to appropriate route table
	if (ep->addr.family == AF_INET) {
		if (!dht->rt_ipv4) {
			//ks_dhtrt_initroute(&dht->rt_ipv4, dht->pool, &ep->nodeid);
		}
	} else {
		if (!dht->rt_ipv6) {
			//ks_dhtrt_initroute(&dht->rt_ipv6, dht->pool, &ep->nodeid);
		}
	}
	
	if (endpoint) {
		*endpoint = ep;
	}

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(void) ks_dht_pulse(ks_dht_t *dht, int32_t timeout)
{
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
				dht->recv_buffer_length = KS_DHT_RECV_BUFFER_SIZE;
			
				raddr.family = dht->endpoints[i]->addr.family;
				if (ks_socket_recvfrom(dht->endpoints_poll[i].fd, dht->recv_buffer, &dht->recv_buffer_length, &raddr) == KS_STATUS_SUCCESS) {
					// @todo copy data to a ks_dht_frame then create job to call ks_dht_process from threadpool
					ks_dht_process(dht, dht->endpoints[i], &raddr);
				}
			}
		}
	}

	ks_dht_idle(dht);
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_utility_compact_address(ks_sockaddr_t *address,
													   uint8_t *buffer,
													   ks_size_t *buffer_length,
													   ks_size_t buffer_size)
{
	ks_size_t required = sizeof(uint16_t);
	uint16_t port = 0;
	
	ks_assert(address);
	ks_assert(buffer);
	ks_assert(buffer_length);
	ks_assert(buffer_size);
	ks_assert(address->family == AF_INET || address->family == AF_INET6);

	if (address->family == AF_INET) {
		required += sizeof(uint32_t);
	} else {
		required += 8 * sizeof(uint16_t);
	}

	if (*buffer_length + required > buffer_size) {
		ks_log(KS_LOG_DEBUG, "Insufficient space remaining for compacting\n");
		return KS_STATUS_FAIL;
	}

	if (address->family == AF_INET) {
		uint32_t *paddr = (uint32_t *)&address->v.v4.sin_addr;
		uint32_t addr = htonl(*paddr);
		port = htons(address->v.v4.sin_port);

		memcpy(buffer + (*buffer_length), (void *)&addr, sizeof(uint32_t));
		*buffer_length += sizeof(uint32_t);
	} else {
		uint16_t *paddr = (uint16_t *)&address->v.v6.sin6_addr;
		port = htons(address->v.v6.sin6_port);

		for (int32_t i = 0; i < 8; ++i) {
			uint16_t addr = htons(paddr[i]);
			memcpy(buffer + (*buffer_length), (void *)&addr, sizeof(uint16_t));
			*buffer_length += sizeof(uint16_t);
		}
	}

	memcpy(buffer + (*buffer_length), (void *)&port, sizeof(uint16_t));
	*buffer_length += sizeof(uint16_t);
	
	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_utility_compact_node(ks_dht_nodeid_t *nodeid,
													ks_sockaddr_t *address,
													uint8_t *buffer,
													ks_size_t *buffer_length,
													ks_size_t buffer_size)
{
	ks_assert(address);
	ks_assert(buffer);
	ks_assert(buffer_length);
	ks_assert(buffer_size);
	ks_assert(address->family == AF_INET || address->family == AF_INET6);

	if (*buffer_length + KS_DHT_NODEID_SIZE > buffer_size) {
		ks_log(KS_LOG_DEBUG, "Insufficient space remaining for compacting\n");
		return KS_STATUS_FAIL;
	}

	memcpy(buffer + (*buffer_length), (void *)nodeid, KS_DHT_NODEID_SIZE);
	*buffer_length += KS_DHT_NODEID_SIZE;

	return ks_dht_utility_compact_address(address, buffer, buffer_length, buffer_size);
}

/**
 *
 */
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
		return KS_STATUS_FAIL;
	}
	
    idv = ben_str_val(id);
	idv_len = ben_str_len(id);
    if (idv_len != KS_DHT_NODEID_SIZE) {
		ks_log(KS_LOG_DEBUG, "Message args '%s' value has an unexpected size of %d\n", key, idv_len);
		return KS_STATUS_FAIL;
	}

	*nodeid = (ks_dht_nodeid_t *)idv;

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
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
		return KS_STATUS_FAIL;
	}

    tokv = ben_str_val(tok);
	tokv_len = ben_str_len(tok);
    if (tokv_len != KS_DHT_TOKEN_SIZE) {
		ks_log(KS_LOG_DEBUG, "Message args '%s' value has an unexpected size of %d\n", key, tokv_len);
		return KS_STATUS_FAIL;
	}

	*token = (ks_dht_token_t *)tokv;

	return KS_STATUS_SUCCESS;
}


/**
 *
 */
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
	
	SHA1_Init(&sha);
	SHA1_Update(&sha, &secret, sizeof(uint32_t));
	SHA1_Update(&sha, raddr->host, strlen(raddr->host));
	SHA1_Update(&sha, &port, sizeof(uint16_t));
	SHA1_Update(&sha, target->id, KS_DHT_NODEID_SIZE);
	SHA1_Final(token->token, &sha);

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_bool_t) ks_dht_token_verify(ks_dht_t *dht, ks_sockaddr_t *raddr, ks_dht_nodeid_t *target, ks_dht_token_t *token)
{
	ks_dht_token_t tok;

	ks_dht_token_generate(dht->token_secret_current, raddr, target, &tok);

	if (!memcmp(tok.token, token->token, KS_DHT_TOKEN_SIZE)) {
		return KS_TRUE;
	}

	ks_dht_token_generate(dht->token_secret_previous, raddr, target, &tok);

	return memcmp(tok.token, token->token, KS_DHT_TOKEN_SIZE) == 0;
}

/**
 *
 */
KS_DECLARE(void) ks_dht_idle(ks_dht_t *dht)
{
	ks_assert(dht);

	ks_dht_idle_expirations(dht);

	ks_dht_idle_send(dht);
}

/**
 *
 */
KS_DECLARE(void) ks_dht_idle_expirations(ks_dht_t *dht)
{
	ks_hash_iterator_t *it = NULL;
	ks_time_t now = ks_time_now_sec();
	
	ks_assert(dht);

	// @todo add delay between checking expirations, every 10 seconds?

	ks_hash_write_lock(dht->transactions_hash);
	for (it = ks_hash_first(dht->transactions_hash, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		const void *key = NULL;
		ks_dht_transaction_t *value = NULL;
		ks_bool_t remove = KS_FALSE;

		ks_hash_this(it, &key, NULL, (void **)&value);
		if (value->finished) {
			remove = KS_TRUE;
		} else if (value->expiration <= now) {
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

/**
 *
 */
KS_DECLARE(void) ks_dht_idle_send(ks_dht_t *dht)
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
		if (!message) {
			bail = ks_q_pop_timeout(dht->send_q, (void **)&message, 1) != KS_STATUS_SUCCESS || !message;
		}
		if (!bail) {
			bail = (ret = ks_dht_send(dht, message)) != KS_STATUS_SUCCESS;
			if (ret == KS_STATUS_BREAK) {
				dht->send_q_unsent = message;
			} else if (ret == KS_STATUS_SUCCESS) {
				ks_dht_message_deinit(message);
				ks_dht_message_free(message);
			}
		}
	}
}

/**
 *
 */
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

	buf_len = ben_encode2(buf, sizeof(buf), message->data);

	ks_log(KS_LOG_DEBUG, "Sending message to %s %d\n", message->raddr.host, message->raddr.port);
	ks_log(KS_LOG_DEBUG, "%s\n", ben_print(message->data));

	return ks_socket_sendto(message->endpoint->sock, (void *)buf, &buf_len, &message->raddr);
}

/**
 *
 */
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

	if (!ep && ks_dht_autoroute_check(dht, raddr, &ep) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

	if (ks_dht_message_alloc(&error, dht->pool) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

	if (ks_dht_message_init(error, ep, raddr, KS_TRUE) != KS_STATUS_SUCCESS) {
		goto done;
	}

	if (ks_dht_message_error(error, transactionid, transactionid_length, &e) != KS_STATUS_SUCCESS) {
		goto done;
	}

	ben_list_append(e, ben_int(errorcode));
	ben_list_append(e, ben_blob(errorstr, strlen(errorstr)));

	ks_log(KS_LOG_DEBUG, "Sending message error %d\n", errorcode);
	ks_q_push(dht->send_q, (void *)error);

	ret = KS_STATUS_SUCCESS;

 done:
	if (ret != KS_STATUS_SUCCESS && error) {
		ks_dht_message_deinit(error);
		ks_dht_message_free(error);
	}
	return ret;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_setup_query(ks_dht_t *dht,
										   ks_dht_endpoint_t *ep,
										   ks_sockaddr_t *raddr,
										   const char *query,
										   ks_dht_message_callback_t callback,
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

	*message = NULL;

	if (!ep && ks_dht_autoroute_check(dht, raddr, &ep) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

    // @todo atomic increment or mutex
	transactionid = dht->transactionid_next++;

	if (ks_dht_transaction_alloc(&trans, dht->pool) != KS_STATUS_SUCCESS) {
		goto done;
	}

	if (ks_dht_transaction_init(trans, raddr, transactionid, callback) != KS_STATUS_SUCCESS) {
		goto done;
	}

	if (ks_dht_message_alloc(&msg, dht->pool) != KS_STATUS_SUCCESS) {
		goto done;
	}

	if (ks_dht_message_init(msg, ep, raddr, KS_TRUE) != KS_STATUS_SUCCESS) {
	    goto done;
	}

	if (ks_dht_message_query(msg, transactionid, query, args) != KS_STATUS_SUCCESS) {
		goto done;
	}

	*message = msg;

	ks_hash_insert(dht->transactions_hash, (void *)&trans->transactionid, trans);

	ret = KS_STATUS_SUCCESS;

 done:
	if (ret != KS_STATUS_SUCCESS) {
		if (trans) {
			ks_dht_transaction_deinit(trans);
			ks_dht_transaction_free(trans);
		}
		if (msg) {
			ks_dht_message_deinit(msg);
			ks_dht_message_free(msg);
		}
		*message = NULL;
	}
	return ret;
}

/**
 *
 */
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

	if (!ep && ks_dht_autoroute_check(dht, raddr, &ep) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

	if (ks_dht_message_alloc(&msg, dht->pool) != KS_STATUS_SUCCESS) {
		goto done;
	}

	if (ks_dht_message_init(msg, ep, raddr, KS_TRUE) != KS_STATUS_SUCCESS) {
		goto done;
	}

	if (ks_dht_message_response(msg, transactionid, transactionid_length, args) != KS_STATUS_SUCCESS) {
		goto done;
	}
	
	*message = msg;

	ret = KS_STATUS_SUCCESS;

 done:
	if (ret != KS_STATUS_SUCCESS && msg) {
		ks_dht_message_deinit(msg);
		ks_dht_message_free(msg);
		*message = NULL;
	}
	return ret;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_send_ping(ks_dht_t *dht, ks_dht_endpoint_t *ep, ks_sockaddr_t *raddr)
{
	ks_dht_message_t *message = NULL;
	struct bencode *a = NULL;

	ks_assert(dht);
	ks_assert(raddr);

	if (ks_dht_setup_query(dht, ep, raddr, "ping", ks_dht_process_response_ping, &message, &a) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

	ben_dict_set(a, ben_blob("id", 2), ben_blob(message->endpoint->nodeid.id, KS_DHT_NODEID_SIZE));

	ks_log(KS_LOG_DEBUG, "Sending message query ping\n");
	ks_q_push(dht->send_q, (void *)message);

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_send_findnode(ks_dht_t *dht, ks_dht_endpoint_t *ep, ks_sockaddr_t *raddr, ks_dht_nodeid_t *targetid)
{
	ks_dht_message_t *message = NULL;
	struct bencode *a = NULL;
	
	ks_assert(dht);
	ks_assert(raddr);
	ks_assert(targetid);

	if (ks_dht_setup_query(dht, ep, raddr, "find_node", ks_dht_process_response_findnode, &message, &a) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}
	
	ben_dict_set(a, ben_blob("id", 2), ben_blob(message->endpoint->nodeid.id, KS_DHT_NODEID_SIZE));
	ben_dict_set(a, ben_blob("target", 6), ben_blob(targetid->id, KS_DHT_NODEID_SIZE));

	ks_log(KS_LOG_DEBUG, "Sending message query find_node\n");
	ks_q_push(dht->send_q, (void *)message);

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_send_get(ks_dht_t *dht, ks_dht_endpoint_t *ep, ks_sockaddr_t *raddr, ks_dht_nodeid_t *targetid)
{
	ks_dht_message_t *message = NULL;
	struct bencode *a = NULL;
	
	ks_assert(dht);
	ks_assert(raddr);
	ks_assert(targetid);

	if (ks_dht_setup_query(dht, ep, raddr, "get", ks_dht_process_response_get, &message, &a) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

	ben_dict_set(a, ben_blob("id", 2), ben_blob(message->endpoint->nodeid.id, KS_DHT_NODEID_SIZE));
	// @todo check for target item locally, set seq to item seq to prevent getting back what we already have if a newer seq is not available
	ben_dict_set(a, ben_blob("target", 6), ben_blob(targetid->id, KS_DHT_NODEID_SIZE));

	ks_log(KS_LOG_DEBUG, "Sending message query get\n");
	ks_q_push(dht->send_q, (void *)message);

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_process(ks_dht_t *dht, ks_dht_endpoint_t *ep, ks_sockaddr_t *raddr)
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
	
	if (ks_dht_message_prealloc(&message, dht->pool) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

	if (ks_dht_message_init(&message, ep, raddr, KS_FALSE) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

	if (ks_dht_message_parse(&message, dht->recv_buffer, dht->recv_buffer_length) != KS_STATUS_SUCCESS) {
		goto done;
	}

	// @todo readlocking registry for calling from threadpool
	if (!(callback = (ks_dht_message_callback_t)(intptr_t)ks_hash_search(dht->registry_type, message.type, KS_UNLOCKED))) {
		ks_log(KS_LOG_DEBUG, "Message type '%s' is not registered\n", message.type);
	} else {
		ret = callback(dht, &message);
	}

 done:
	ks_dht_message_deinit(&message);
	
	return ret;
}

/**
 *
 */
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

	// @todo readlocking registry for calling from threadpool
	if (!(callback = (ks_dht_message_callback_t)(intptr_t)ks_hash_search(dht->registry_query, query, KS_UNLOCKED))) {
		ks_log(KS_LOG_DEBUG, "Message query '%s' is not registered\n", query);
	} else {
		ret = callback(dht, message);
	}

	return ret;
}

/**
 *
 */
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
	
	if (!transaction) {
		ks_log(KS_LOG_DEBUG, "Message response rejected with unknown transaction id %d\n", transactionid);
	} else if (!ks_addr_cmp(&message->raddr, &transaction->raddr)) {
		ks_log(KS_LOG_DEBUG,
			   "Message response rejected due to spoofing from %s %d, expected %s %d\n",
			   message->raddr.host,
			   message->raddr.port,
			   transaction->raddr.host,
			   transaction->raddr.port);
	} else {
		transaction->finished = KS_TRUE;
		ret = transaction->callback(dht, message);
	}

	return ret;
}

/**
 *
 */
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

		// @todo readlock on registry
		if ((callback = (ks_dht_message_callback_t)(intptr_t)ks_hash_search(dht->registry_error, error, KS_UNLOCKED))) {
			ret = callback(dht, message);
		} else {
			ks_log(KS_LOG_DEBUG, "Message error received for transaction id %d, error %d: %s\n", transactionid, errorcode, error);
			ret = KS_STATUS_SUCCESS;
		}
	}

	return ret;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_process_query_ping(ks_dht_t *dht, ks_dht_message_t *message)
{
	ks_dht_nodeid_t *id;
	ks_dht_message_t *response = NULL;
	struct bencode *r = NULL;

	ks_assert(dht);
	ks_assert(message);
	ks_assert(message->args);

	if (ks_dht_utility_extract_nodeid(message->args, "id", &id) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

	// @todo add/touch bucket entry for remote node

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

/**
 *
 */
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

	ks_assert(dht);
	ks_assert(message);
	ks_assert(message->args);

	if (ks_dht_utility_extract_nodeid(message->args, "id", &id) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

	if (ks_dht_utility_extract_nodeid(message->args, "target", &target) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

	want = ben_dict_get_by_str(message->args, "want");
	if (want) {
		size_t want_len = ben_list_len(want);
		for (size_t i = 0; i < want_len; ++i) {
			struct bencode *iv = ben_list_get(want, i);
			if (!ben_cmp_with_str(iv, "n4")) {
				want4 = KS_TRUE;
			}
			if (!ben_cmp_with_str(iv, "n6")) {
				want6 = KS_TRUE;
			}
		}
	}

	if (!want4 && !want6) {
		want4 = message->raddr.family == AF_INET;
		want6 = message->raddr.family == AF_INET6;
	}

	// @todo add/touch bucket entry for remote node
	
	ks_log(KS_LOG_DEBUG, "Message query find_node is valid\n");


	if (want4) {
		// @todo get closest nodes to target from ipv4 route table
		// @todo compact nodes into buffer4
	}
	if (want6) {
		// @todo get closest nodes to target from ipv6 route table
		// @todo compact nodes into buffer6
	}

	// @todo remove this, testing only
	if (ks_dht_utility_compact_node(id,
									&message->raddr,
									message->raddr.family == AF_INET ? buffer4 : buffer6,
									message->raddr.family == AF_INET ? &buffer4_length : &buffer6_length,
									message->raddr.family == AF_INET ? sizeof(buffer4) : sizeof(buffer6)) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

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
	if (want4) {
		ben_dict_set(r, ben_blob("nodes", 5), ben_blob(buffer4, buffer4_length));
	}
	if (want6) {
		ben_dict_set(r, ben_blob("nodes6", 6), ben_blob(buffer6, buffer6_length));
	}

	ks_log(KS_LOG_DEBUG, "Sending message response find_node\n");
	ks_q_push(dht->send_q, (void *)response);

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
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

	ks_assert(dht);
	ks_assert(message);
	ks_assert(message->args);

	if (ks_dht_utility_extract_nodeid(message->args, "id", &id) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

	if (ks_dht_utility_extract_nodeid(message->args, "target", &target) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}
	
	seq = ben_dict_get_by_str(message->args, "seq");
	if (seq) {
		sequence = ben_int_val(seq);
	}

	// @todo add/touch bucket entry for remote node

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
		if (!sequence_snuffed) {
			ben_dict_set(r, ben_blob("v", 1), ben_clone(item->v));
		}
	}
	// @todo nodes, nodes6

	ks_log(KS_LOG_DEBUG, "Sending message response get\n");
	ks_q_push(dht->send_q, (void *)response);

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_process_query_put(ks_dht_t *dht, ks_dht_message_t *message)
{
	ks_dht_message_t *response = NULL;
	struct bencode *r = NULL;

	ks_assert(dht);
	ks_assert(message);
	ks_assert(message->args);

	// @todo add/touch bucket entry for remote node

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


/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_process_response_ping(ks_dht_t *dht, ks_dht_message_t *message)
{
	ks_assert(dht);
	ks_assert(message);

	// @todo add/touch bucket entry for remote node
	
	ks_log(KS_LOG_DEBUG, "Message response ping is reached\n");

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_process_response_findnode(ks_dht_t *dht, ks_dht_message_t *message)
{
	ks_assert(dht);
	ks_assert(message);

	// @todo add/touch bucket entry for remote node and other nodes returned

	ks_log(KS_LOG_DEBUG, "Message response find_node is reached\n");

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_process_response_get(ks_dht_t *dht, ks_dht_message_t *message)
{
	ks_dht_nodeid_t *id;
	ks_dht_token_t *token;
	
	ks_assert(dht);
	ks_assert(message);

	// @todo use ks_dht_storageitem_mutable or ks_dht_storageitem_immutable if v is provided
	if (ks_dht_utility_extract_nodeid(message->args, "id", &id) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}
	
	if (ks_dht_utility_extract_token(message->args, "token", &token) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

	// @todo add extract function for mutable ks_dht_storageitem_key_t
	// @todo add extract function for mutable ks_dht_storageitem_signature_t
	
	// @todo add/touch bucket entry for remote node and other nodes returned

	ks_log(KS_LOG_DEBUG, "Message response get is reached\n");

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
