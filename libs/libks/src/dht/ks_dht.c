#include "ks_dht.h"
#include "ks_dht-int.h"
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
KS_DECLARE(ks_status_t) ks_dht2_prealloc(ks_dht2_t *dht, ks_pool_t *pool)
{
	ks_assert(dht);
	ks_assert(pool);

	dht->pool = pool;
	dht->pool_alloc = KS_FALSE;

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_free(ks_dht2_t *dht)
{
	ks_pool_t *pool = dht->pool;
	ks_bool_t pool_alloc = dht->pool_alloc;

	ks_dht2_deinit(dht);
	ks_pool_free(pool, dht);
	if (pool_alloc) {
		ks_pool_close(&pool);
	}

	return KS_STATUS_SUCCESS;
}
												

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_init(ks_dht2_t *dht, const ks_dht2_nodeid_raw_t *nodeid)
{
	ks_assert(dht);
	ks_assert(dht->pool);

	dht->autoroute = KS_FALSE;
	dht->autoroute_port = 0;
	
	if (ks_dht2_nodeid_prealloc(&dht->nodeid, dht->pool) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}
	
	if (ks_dht2_nodeid_init(&dht->nodeid, nodeid) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

	ks_hash_create(&dht->registry_type, KS_HASH_MODE_DEFAULT, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK, dht->pool);
	ks_dht2_register_type(dht, "q", ks_dht2_process_query);
	ks_dht2_register_type(dht, "r", ks_dht2_process_response);
	ks_dht2_register_type(dht, "e", ks_dht2_process_error);

	ks_hash_create(&dht->registry_query, KS_HASH_MODE_DEFAULT, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK, dht->pool);
	ks_dht2_register_query(dht, "ping", ks_dht2_process_query_ping);
	ks_dht2_register_query(dht, "find_node", ks_dht2_process_query_findnode);

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
	
	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_deinit(ks_dht2_t *dht)
{
	ks_assert(dht);

	dht->transactionid_next = 0;
	if (dht->transactions_hash) {
		ks_hash_destroy(&dht->transactions_hash);
		dht->transactions_hash = NULL;
	}
	dht->recv_buffer_length = 0;
	if (dht->send_q) {
		ks_dht2_message_t *msg;
		while (ks_q_pop_timeout(dht->send_q, (void **)&msg, 1) == KS_STATUS_SUCCESS && msg) {
			ks_dht2_message_deinit(msg);
			ks_dht2_message_free(msg);
		}
		ks_q_destroy(&dht->send_q);
		dht->send_q = NULL;
	}
	if (dht->send_q_unsent) {
		ks_dht2_message_deinit(dht->send_q_unsent);
		ks_dht2_message_free(dht->send_q_unsent);
		dht->send_q_unsent = NULL;
	}
	for (int32_t i = 0; i < dht->endpoints_size; ++i) {
		ks_dht2_endpoint_t *ep = dht->endpoints[i];
		ks_dht2_endpoint_deinit(ep);
		ks_dht2_endpoint_free(ep);
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

	ks_dht2_nodeid_deinit(&dht->nodeid);

	dht->autoroute = KS_FALSE;
	dht->autoroute_port = 0;
	
	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_autoroute(ks_dht2_t *dht, ks_bool_t autoroute, ks_port_t port)
{
	ks_assert(dht);

	if (!autoroute) {
		port = 0;
	} else if (port == 0) {
		return KS_STATUS_FAIL;
	}
	
	dht->autoroute = autoroute;
	dht->autoroute_port = port;
	
	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_register_type(ks_dht2_t *dht, const char *value, ks_dht2_message_callback_t callback)
{
	ks_assert(dht);
	ks_assert(value);
	ks_assert(callback);

	return ks_hash_insert(dht->registry_type, (void *)value, (void *)(intptr_t)callback) ? KS_STATUS_SUCCESS : KS_STATUS_FAIL;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_register_query(ks_dht2_t *dht, const char *value, ks_dht2_message_callback_t callback)
{
	ks_assert(dht);
	ks_assert(value);
	ks_assert(callback);

	return ks_hash_insert(dht->registry_query, (void *)value, (void *)(intptr_t)callback) ? KS_STATUS_SUCCESS : KS_STATUS_FAIL;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_register_error(ks_dht2_t *dht, const char *value, ks_dht2_message_callback_t callback)
{
	ks_assert(dht);
	ks_assert(value);
	ks_assert(callback);

	return ks_hash_insert(dht->registry_error, (void *)value, (void *)(intptr_t)callback) ? KS_STATUS_SUCCESS : KS_STATUS_FAIL;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_bind(ks_dht2_t *dht, const ks_sockaddr_t *addr, ks_dht2_endpoint_t **endpoint)
{
	ks_dht2_endpoint_t *ep;
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

	if (endpoint) {
		*endpoint = ep;
	}

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(void) ks_dht2_pulse(ks_dht2_t *dht, int32_t timeout)
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
					ks_dht2_process(dht, &raddr);
				}
			}
		}
	}

	ks_dht2_idle(dht);
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_maketid(ks_dht2_t *dht)
{
	ks_assert(dht);

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(void) ks_dht2_idle(ks_dht2_t *dht)
{
	ks_assert(dht);

	ks_dht2_idle_expirations(dht);

	ks_dht2_idle_send(dht);
}

/**
 *
 */
KS_DECLARE(void) ks_dht2_idle_expirations(ks_dht2_t *dht)
{
	ks_hash_iterator_t *it = NULL;
	ks_time_t now = ks_time_now_sec();
	
	ks_assert(dht);

	// @todo add delay between checking expirations, every 10 seconds?

	ks_hash_write_lock(dht->transactions_hash);
	for (it = ks_hash_first(dht->transactions_hash, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		const void *key = NULL;
		ks_dht2_transaction_t *value = NULL;
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
}

/**
 *
 */
KS_DECLARE(void) ks_dht2_idle_send(ks_dht2_t *dht)
{
	ks_dht2_message_t *message;
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
			bail = (ret = ks_dht2_send(dht, message)) != KS_STATUS_SUCCESS;
			if (ret == KS_STATUS_BREAK) {
				dht->send_q_unsent = message;
			} else if (ret == KS_STATUS_SUCCESS) {
				ks_dht2_message_deinit(message);
				ks_dht2_message_free(message);
			}
		}
	}
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_send(ks_dht2_t *dht, ks_dht2_message_t *message)
{
	// @todo lookup standard def for IPV6 max size
	char ip[48];
	ks_dht2_endpoint_t *ep;
	// @todo calculate max IPV6 payload size?
	char buf[1000];
	ks_size_t buf_len;
	
	ks_assert(dht);
	ks_assert(message);
	ks_assert(message->data);

	// @todo blacklist check

	ks_ip_route(ip, sizeof(ip), message->raddr.host);

	if (!(ep = ks_hash_search(dht->endpoints_hash, ip, KS_UNLOCKED)) && dht->autoroute) {
		ks_sockaddr_t addr;
		ks_addr_set(&addr, ip, dht->autoroute_port, message->raddr.family);
		if (ks_dht2_bind(dht, &addr, &ep) != KS_STATUS_SUCCESS) {
			return KS_STATUS_FAIL;
		}
	}

	if (!ep) {
		ks_log(KS_LOG_DEBUG, "No route available to %s\n", message->raddr.host);
		return KS_STATUS_FAIL;
	}

	buf_len = ben_encode2(buf, sizeof(buf), message->data);

	ks_log(KS_LOG_DEBUG, "Sending message to %s %d\n", message->raddr.host, message->raddr.port);
	ks_log(KS_LOG_DEBUG, "%s\n", ben_print(message->data));

	return ks_socket_sendto(ep->sock, (void *)buf, &buf_len, &message->raddr);
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_send_error(ks_dht2_t *dht,
										   ks_sockaddr_t *raddr,
										   uint8_t *transactionid,
										   ks_size_t transactionid_length,
										   long long errorcode,
										   const char *errorstr)
{
	ks_dht2_message_t *error = NULL;
	struct bencode *e = NULL;
	ks_status_t ret = KS_STATUS_FAIL;

	ks_assert(dht);
	ks_assert(raddr);
	ks_assert(transactionid);
	ks_assert(errorstr);

	if (ks_dht2_message_alloc(&error, dht->pool) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

	if (ks_dht2_message_init(error, raddr, KS_TRUE) != KS_STATUS_SUCCESS) {
		goto done;
	}

	if (ks_dht2_message_error(error, transactionid, transactionid_length, &e) != KS_STATUS_SUCCESS) {
		goto done;
	}

	ben_list_append(e, ben_int(errorcode));
	ben_list_append(e, ben_blob(errorstr, strlen(errorstr)));

	ks_log(KS_LOG_DEBUG, "Sending message error %d\n", errorcode);
	ks_q_push(dht->send_q, (void *)error);

	ret = KS_STATUS_SUCCESS;

 done:
	if (ret != KS_STATUS_SUCCESS && error) {
		ks_dht2_message_deinit(error);
		ks_dht2_message_free(error);
	}
	return ret;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_setup_query(ks_dht2_t *dht,
											ks_sockaddr_t *raddr,
											const char *query,
											ks_dht2_message_callback_t callback,
											ks_dht2_message_t **message,
											struct bencode **args)
{
	uint32_t transactionid;
	ks_dht2_transaction_t *trans = NULL;
	ks_dht2_message_t *msg = NULL;
	ks_status_t ret = KS_STATUS_FAIL;

	ks_assert(dht);
	ks_assert(raddr);
	ks_assert(query);
	ks_assert(callback);
	ks_assert(message);

	*message = NULL;
	
    // @todo atomic increment or mutex
	transactionid = dht->transactionid_next++;

	if (ks_dht2_transaction_alloc(&trans, dht->pool) != KS_STATUS_SUCCESS) {
		goto done;
	}

	if (ks_dht2_transaction_init(trans, raddr, transactionid, callback) != KS_STATUS_SUCCESS) {
		goto done;
	}

	if (ks_dht2_message_alloc(&msg, dht->pool) != KS_STATUS_SUCCESS) {
		goto done;
	}

	if (ks_dht2_message_init(msg, raddr, KS_TRUE) != KS_STATUS_SUCCESS) {
	    goto done;
	}

	if (ks_dht2_message_query(msg, transactionid, query, args) != KS_STATUS_SUCCESS) {
		goto done;
	}

	*message = msg;

	ks_hash_insert(dht->transactions_hash, (void *)&trans->transactionid, trans);

	ret = KS_STATUS_SUCCESS;

 done:
	if (ret != KS_STATUS_SUCCESS) {
		if (trans) {
			ks_dht2_transaction_deinit(trans);
			ks_dht2_transaction_free(trans);
		}
		if (msg) {
			ks_dht2_message_deinit(msg);
			ks_dht2_message_free(msg);
		}
		*message = NULL;
	}
	return ret;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_send_ping(ks_dht2_t *dht, ks_sockaddr_t *raddr)
{
	ks_dht2_message_t *message = NULL;
	struct bencode *a = NULL;
	
	ks_assert(dht);
	ks_assert(raddr);

	if (ks_dht2_setup_query(dht, raddr, "ping", ks_dht2_process_response_ping, &message, &a) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}
	
	ben_dict_set(a, ben_blob("id", 2), ben_blob(dht->nodeid.id, KS_DHT_NODEID_LENGTH));

	ks_log(KS_LOG_DEBUG, "Sending message query ping\n");
	ks_q_push(dht->send_q, (void *)message);

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_send_findnode(ks_dht2_t *dht, ks_sockaddr_t *raddr, ks_dht2_nodeid_raw_t *targetid)
{
	ks_dht2_message_t *message = NULL;
	struct bencode *a = NULL;
	
	ks_assert(dht);
	ks_assert(raddr);
	ks_assert(targetid);

	if (ks_dht2_setup_query(dht, raddr, "find_node", ks_dht2_process_response_findnode, &message, &a) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}
	
	ben_dict_set(a, ben_blob("id", 2), ben_blob(dht->nodeid.id, KS_DHT_NODEID_LENGTH));
	ben_dict_set(a, ben_blob("target", 6), ben_blob(targetid->id, KS_DHT_NODEID_LENGTH));

	ks_log(KS_LOG_DEBUG, "Sending message query find_node\n");
	ks_q_push(dht->send_q, (void *)message);
	//ks_dht2_send(dht, raddr, message);

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_process(ks_dht2_t *dht, ks_sockaddr_t *raddr)
{
	ks_dht2_message_t message;
	ks_dht2_message_callback_t callback;
	ks_status_t ret = KS_STATUS_FAIL;

	ks_assert(dht);
	ks_assert(raddr);

	ks_log(KS_LOG_DEBUG, "Received message from %s %d\n", raddr->host, raddr->port);
	if (raddr->family != AF_INET && raddr->family != AF_INET6) {
		ks_log(KS_LOG_DEBUG, "Message from unsupported address family\n");
		return KS_STATUS_FAIL;
	}

	// @todo blacklist check for bad actor nodes
	
	if (ks_dht2_message_prealloc(&message, dht->pool) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

	if (ks_dht2_message_init(&message, raddr, KS_FALSE) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}

	if (ks_dht2_message_parse(&message, dht->recv_buffer, dht->recv_buffer_length) != KS_STATUS_SUCCESS) {
		goto done;
	}
	
	if (!(callback = (ks_dht2_message_callback_t)(intptr_t)ks_hash_search(dht->registry_type, message.type, KS_UNLOCKED))) {
		ks_log(KS_LOG_DEBUG, "Message type '%s' is not registered\n", message.type);
	} else {
		ret = callback(dht, &message);
	}

 done:
	ks_dht2_message_deinit(&message);
	
	return ret;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_process_query(ks_dht2_t *dht, ks_dht2_message_t *message)
{
	struct bencode *q;
	struct bencode *a;
	const char *qv;
	ks_size_t qv_len;
	char query[KS_DHT_MESSAGE_QUERY_MAX_SIZE];
	ks_dht2_message_callback_t callback;
	ks_status_t ret = KS_STATUS_FAIL;

	ks_assert(dht);
	ks_assert(message);

	// @todo start of ks_dht2_message_parse_query
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
	// @todo end of ks_dht2_message_parse_query

	message->args = a;

	if (!(callback = (ks_dht2_message_callback_t)(intptr_t)ks_hash_search(dht->registry_query, query, KS_UNLOCKED))) {
		ks_log(KS_LOG_DEBUG, "Message query '%s' is not registered\n", query);
	} else {
		ret = callback(dht, message);
	}

	return ret;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_process_response(ks_dht2_t *dht, ks_dht2_message_t *message)
{
	struct bencode *r;
	ks_dht2_transaction_t *transaction;
	uint32_t *tid;
	uint32_t transactionid;
	ks_status_t ret = KS_STATUS_FAIL;

	ks_assert(dht);
	ks_assert(message);

	// @todo start of ks_dht2_message_parse_response
	r = ben_dict_get_by_str(message->data, "r");
	if (!r) {
		ks_log(KS_LOG_DEBUG, "Message response missing required key 'r'\n");
		return KS_STATUS_FAIL;
	}
	// todo end of ks_dht2_message_parse_response

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
		// @todo mark transaction for later removal
		transaction->finished = KS_TRUE;
		ret = transaction->callback(dht, message);
	}

	return ret;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_process_error(ks_dht2_t *dht, ks_dht2_message_t *message)
{
	struct bencode *e;
	struct bencode *ec;
	struct bencode *es;
	const char *et;
	ks_size_t es_len;
	long long errorcode;
	char error[KS_DHT_MESSAGE_ERROR_MAX_SIZE];
	ks_dht2_transaction_t *transaction;
	uint32_t *tid;
	uint32_t transactionid;
	ks_status_t ret = KS_STATUS_FAIL;

	ks_assert(dht);
	ks_assert(message);

	// @todo start of ks_dht2_message_parse_error
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
	// todo end of ks_dht2_message_parse_error

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
		// @todo mark transaction for later removal
		ks_dht2_message_callback_t callback;
		transaction->finished = KS_TRUE;

		if ((callback = (ks_dht2_message_callback_t)(intptr_t)ks_hash_search(dht->registry_error, error, KS_UNLOCKED))) {
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
KS_DECLARE(ks_status_t) ks_dht2_process_query_ping(ks_dht2_t *dht, ks_dht2_message_t *message)
{
	struct bencode *id;
	//const char *idv;
	ks_size_t idv_len;
	ks_dht2_message_t *response = NULL;
	struct bencode *r = NULL;
	ks_status_t ret = KS_STATUS_FAIL;

	ks_assert(dht);
	ks_assert(message);
	ks_assert(message->args);

    id = ben_dict_get_by_str(message->args, "id");
    if (!id) {
		ks_log(KS_LOG_DEBUG, "Message args missing required key 'id'\n");
		return KS_STATUS_FAIL;
	}
	
    //idv = ben_str_val(id);
	idv_len = ben_str_len(id);
    if (idv_len != KS_DHT_NODEID_LENGTH) {
		ks_log(KS_LOG_DEBUG, "Message args 'id' value has an unexpected size of %d\n", idv_len);
		return KS_STATUS_FAIL;
	}

	// @todo add/touch bucket entry for remote node

	ks_log(KS_LOG_DEBUG, "Message query ping is valid\n");

	
	if (ks_dht2_message_alloc(&response, dht->pool) != KS_STATUS_SUCCESS) {
		goto done;
	}

	if (ks_dht2_message_init(response, &message->raddr, KS_TRUE) != KS_STATUS_SUCCESS) {
		goto done;
	}

	if (ks_dht2_message_response(response, message->transactionid, message->transactionid_length, &r) != KS_STATUS_SUCCESS) {
		goto done;
	}
	
	ben_dict_set(r, ben_blob("id", 2), ben_blob(dht->nodeid.id, KS_DHT_NODEID_LENGTH));

	ks_log(KS_LOG_DEBUG, "Sending message response ping\n");
	ks_q_push(dht->send_q, (void *)response);

	ret = KS_STATUS_SUCCESS;

 done:
	if (ret != KS_STATUS_SUCCESS && response) {
		ks_dht2_message_deinit(response);
		ks_dht2_message_free(response);
	}
	return ret;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_process_query_findnode(ks_dht2_t *dht, ks_dht2_message_t *message)
{
	struct bencode *id;
	struct bencode *target;
	//const char *idv;
	//const char *targetv;
	ks_size_t idv_len;
	ks_size_t targetv_len;
	ks_dht2_message_t *response = NULL;
	struct bencode *r = NULL;
	ks_status_t ret = KS_STATUS_FAIL;

	ks_assert(dht);
	ks_assert(message);
	ks_assert(message->args);

    id = ben_dict_get_by_str(message->args, "id");
    if (!id) {
		ks_log(KS_LOG_DEBUG, "Message args missing required key 'id'\n");
		return KS_STATUS_FAIL;
	}
	
    //idv = ben_str_val(id);
	idv_len = ben_str_len(id);
    if (idv_len != KS_DHT_NODEID_LENGTH) {
		ks_log(KS_LOG_DEBUG, "Message args 'id' value has an unexpected size of %d\n", idv_len);
		return KS_STATUS_FAIL;
	}

	target = ben_dict_get_by_str(message->args, "target");
    if (!target) {
		ks_log(KS_LOG_DEBUG, "Message args missing required key 'target'\n");
		return KS_STATUS_FAIL;
	}
	
    //targetv = ben_str_val(target);
    targetv_len = ben_str_len(target);
    if (targetv_len != KS_DHT_NODEID_LENGTH) {
		ks_log(KS_LOG_DEBUG, "Message args 'target' value has an unexpected size of %d\n", targetv_len);
		return KS_STATUS_FAIL;
	}

	
	ks_log(KS_LOG_DEBUG, "Message query find_node is valid\n");

	
	if (ks_dht2_message_alloc(&response, dht->pool) != KS_STATUS_SUCCESS) {
		goto done;
	}

	if (ks_dht2_message_init(response, &message->raddr, KS_TRUE) != KS_STATUS_SUCCESS) {
		goto done;
	}

	if (ks_dht2_message_response(response, message->transactionid, message->transactionid_length, &r) != KS_STATUS_SUCCESS) {
		goto done;
	}
	
	ben_dict_set(r, ben_blob("id", 2), ben_blob(dht->nodeid.id, KS_DHT_NODEID_LENGTH));

	ks_log(KS_LOG_DEBUG, "Sending message response find_node\n");
	ks_q_push(dht->send_q, (void *)response);

	ret = KS_STATUS_SUCCESS;

 done:
	if (ret != KS_STATUS_SUCCESS && response) {
		ks_dht2_message_deinit(response);
		ks_dht2_message_free(response);
	}
	return ret;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_process_response_ping(ks_dht2_t *dht, ks_dht2_message_t *message)
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
KS_DECLARE(ks_status_t) ks_dht2_process_response_findnode(ks_dht2_t *dht, ks_dht2_message_t *message)
{
	ks_assert(dht);
	ks_assert(message);

	// @todo add/touch bucket entry for remote node and other nodes returned

	ks_log(KS_LOG_DEBUG, "Message response find_node is reached\n");

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
