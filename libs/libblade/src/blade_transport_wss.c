/*
 * Copyright (c) 2017, Shane Bryldt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "blade.h"

#define BLADE_MODULE_WSS_TRANSPORT_NAME "wss"
#define BLADE_MODULE_WSS_ENDPOINTS_MULTIHOME_MAX 16

typedef struct blade_transport_wss_s blade_transport_wss_t;
typedef struct blade_transport_wss_link_s blade_transport_wss_link_t;

struct blade_transport_wss_s {
	blade_handle_t *handle;
	blade_transport_t *transport;
	blade_transport_callbacks_t *callbacks;

	const char *ssl_key;
	const char *ssl_cert;
	const char *ssl_chain;
	ks_sockaddr_t endpoints_ipv4[BLADE_MODULE_WSS_ENDPOINTS_MULTIHOME_MAX];
	ks_sockaddr_t endpoints_ipv6[BLADE_MODULE_WSS_ENDPOINTS_MULTIHOME_MAX];
	int32_t endpoints_ipv4_length;
	int32_t endpoints_ipv6_length;
	int32_t endpoints_backlog;
	const char *endpoints_ssl_key;
	const char *endpoints_ssl_cert;
	const char *endpoints_ssl_chain;

	volatile ks_bool_t shutdown;

	struct pollfd *listeners_poll;
	int32_t listeners_count;
};

struct blade_transport_wss_link_s {
	blade_transport_wss_t *transport;

	const char *session_id;
	ks_socket_t sock;
	kws_t *kws;
	SSL_CTX *ssl;
};



ks_status_t blade_transport_wss_listen(blade_transport_wss_t *btwss, ks_sockaddr_t *addr);
void *blade_transport_wss_listeners_thread(ks_thread_t *thread, void *data);


ks_status_t blade_transport_wss_link_create(blade_transport_wss_link_t **btwsslP, ks_pool_t *pool, blade_transport_wss_t *btwss, ks_socket_t sock, const char *session_id);

ks_status_t blade_transport_wss_onstartup(blade_transport_t *bt, config_setting_t *config);
ks_status_t blade_transport_wss_onshutdown(blade_transport_t *bt);

ks_status_t blade_transport_wss_onconnect(blade_connection_t **bcP, blade_transport_t *bt, blade_identity_t *target, const char *session_id);

ks_status_t blade_transport_wss_onsend(blade_connection_t *bc, cJSON *json);
ks_status_t blade_transport_wss_onreceive(blade_connection_t *bc, cJSON **json);

blade_connection_state_hook_t blade_transport_wss_onstate_startup_inbound(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_onstate_startup_outbound(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_onstate_shutdown(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_onstate_run_inbound(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_onstate_run_outbound(blade_connection_t *bc, blade_connection_state_condition_t condition);



static blade_transport_callbacks_t g_transport_wss_callbacks =
{
	blade_transport_wss_onstartup,
	blade_transport_wss_onshutdown,

	blade_transport_wss_onconnect,

	blade_transport_wss_onsend,
	blade_transport_wss_onreceive,

	blade_transport_wss_onstate_startup_inbound,
	blade_transport_wss_onstate_startup_outbound,
	blade_transport_wss_onstate_shutdown,
	blade_transport_wss_onstate_shutdown,
	blade_transport_wss_onstate_run_inbound,
	blade_transport_wss_onstate_run_outbound,
};


static void blade_transport_wss_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	//blade_transport_wss_t *btwss = (blade_transport_wss_t *)ptr;

	//ks_assert(btwss);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_transport_wss_create(blade_transport_t **btP, blade_handle_t *bh)
{
	blade_transport_wss_t *btwss = NULL;
	ks_pool_t *pool = NULL;

	ks_assert(btP);
	ks_assert(bh);

	ks_pool_open(&pool);
	ks_assert(pool);

    btwss = ks_pool_alloc(pool, sizeof(blade_transport_wss_t));
	btwss->handle = bh;

	blade_transport_create(&btwss->transport, bh, pool, BLADE_MODULE_WSS_TRANSPORT_NAME, btwss, &g_transport_wss_callbacks);
	btwss->callbacks = &g_transport_wss_callbacks;

	ks_pool_set_cleanup(btwss, NULL, blade_transport_wss_cleanup);

	ks_log(KS_LOG_DEBUG, "Created\n");

	*btP = btwss->transport;

	return KS_STATUS_SUCCESS;
}

static void blade_transport_wss_link_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_transport_wss_link_t *btwssl = (blade_transport_wss_link_t *)ptr;

	ks_assert(btwssl);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		if (btwssl->session_id) ks_pool_free(&btwssl->session_id);
		if (btwssl->kws) kws_destroy(&btwssl->kws);
		else ks_socket_close(&btwssl->sock);
		if (btwssl->ssl) SSL_CTX_free(btwssl->ssl);
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}


ks_status_t blade_transport_wss_link_create(blade_transport_wss_link_t **btwsslP, ks_pool_t *pool, blade_transport_wss_t *btwss, ks_socket_t sock, const char *session_id)
{
	blade_transport_wss_link_t *btwssl = NULL;

	ks_assert(btwsslP);
	ks_assert(btwss);
	ks_assert(sock != KS_SOCK_INVALID);

	btwssl = ks_pool_alloc(pool, sizeof(blade_transport_wss_link_t));
	btwssl->transport = btwss;
	btwssl->sock = sock;
	if (session_id) btwssl->session_id = ks_pstrdup(pool, session_id);

	ks_pool_set_cleanup(btwssl, NULL, blade_transport_wss_link_cleanup);

	ks_log(KS_LOG_DEBUG, "Created\n");

	*btwsslP = btwssl;

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_transport_wss_link_ssl_init(blade_transport_wss_link_t *btwssl, ks_bool_t server)
{
	const SSL_METHOD *method = NULL;
	const char *key = NULL;
	const char *cert = NULL;
	const char *chain = NULL;

	ks_assert(btwssl);

	method = server ? TLSv1_2_server_method() : TLSv1_2_client_method();
	key = server ? btwssl->transport->endpoints_ssl_key : btwssl->transport->ssl_key;
	cert = server ? btwssl->transport->endpoints_ssl_cert : btwssl->transport->ssl_cert;
	chain = server ? btwssl->transport->endpoints_ssl_chain : btwssl->transport->ssl_chain;

	// @todo should actually error out if there is no key/cert/chain available, as SSL/TLS is meant to be mandatory
	if (key && cert) {
		btwssl->ssl = SSL_CTX_new(method);

		// @todo probably manage this through configuration, but TLS 1.2 is preferred
		SSL_CTX_set_options(btwssl->ssl, SSL_OP_NO_SSLv2);
		SSL_CTX_set_options(btwssl->ssl, SSL_OP_NO_SSLv3);
		SSL_CTX_set_options(btwssl->ssl, SSL_OP_NO_TLSv1);
		SSL_CTX_set_options(btwssl->ssl, SSL_OP_NO_TLSv1_1);
		// @todo look into difference in debian vs windows OpenSSL, aparantly debian system package does not provide this definition
		//SSL_CTX_set_options(btwssl->ssl, SSL_OP_NO_DTLSv1);
		SSL_CTX_set_options(btwssl->ssl, SSL_OP_NO_COMPRESSION);
		if (server) SSL_CTX_set_verify(btwssl->ssl, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);

		if (chain) {
			if (!SSL_CTX_use_certificate_chain_file(btwssl->ssl, chain)) {
				ks_log(KS_LOG_DEBUG, "SSL Chain File Error\n");
				return KS_STATUS_FAIL;
			}
			if (!SSL_CTX_load_verify_locations(btwssl->ssl, chain, NULL)) {
				ks_log(KS_LOG_DEBUG, "SSL Verify File Error\n");
				return KS_STATUS_FAIL;
			}
		}

		if (!SSL_CTX_use_certificate_file(btwssl->ssl, cert, SSL_FILETYPE_PEM)) {
			ks_log(KS_LOG_DEBUG, "SSL Cert File Error\n");
			return KS_STATUS_FAIL;
		}

		if (!SSL_CTX_use_PrivateKey_file(btwssl->ssl, key, SSL_FILETYPE_PEM)) {
			ks_log(KS_LOG_DEBUG, "SSL Key File Error\n");
			return KS_STATUS_FAIL;
		}

		if (!SSL_CTX_check_private_key(btwssl->ssl)) {
			ks_log(KS_LOG_DEBUG, "SSL Key File Verification Error\n");
			return KS_STATUS_FAIL;
		}

		SSL_CTX_set_cipher_list(btwssl->ssl, "HIGH:!DSS:!aNULL@STRENGTH");
	}

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_transport_wss_config(blade_transport_wss_t *btwss, config_setting_t *config)
{
	ks_pool_t *pool = NULL;
	config_setting_t *transport = NULL;
	config_setting_t *transport_wss = NULL;
	config_setting_t *transport_wss_ssl = NULL;
	config_setting_t *transport_wss_endpoints = NULL;
	config_setting_t *transport_wss_endpoints_ipv4 = NULL;
	config_setting_t *transport_wss_endpoints_ipv6 = NULL;
	config_setting_t *transport_wss_endpoints_ssl = NULL;
	config_setting_t *element;
	config_setting_t *tmp1;
	config_setting_t *tmp2;
	const char *ssl_key = NULL;
	const char *ssl_cert = NULL;
	const char *ssl_chain = NULL;
	ks_sockaddr_t endpoints_ipv4[BLADE_MODULE_WSS_ENDPOINTS_MULTIHOME_MAX];
	ks_sockaddr_t endpoints_ipv6[BLADE_MODULE_WSS_ENDPOINTS_MULTIHOME_MAX];
	int32_t endpoints_ipv4_length = 0;
	int32_t endpoints_ipv6_length = 0;
	int32_t endpoints_backlog = 8;
	const char *endpoints_ssl_key = NULL;
	const char *endpoints_ssl_cert = NULL;
	const char *endpoints_ssl_chain = NULL;

	ks_assert(btwss);
	ks_assert(config);

	pool = ks_pool_get(btwss);

	if (!config_setting_is_group(config)) {
		ks_log(KS_LOG_DEBUG, "!config_setting_is_group(config)\n");
		return KS_STATUS_FAIL;
	}
	transport = config_setting_get_member(config, "transport");
	if (transport) {
		transport_wss = config_setting_get_member(transport, "wss");
		if (transport_wss) {
			transport_wss_ssl = config_setting_get_member(transport_wss, "ssl");
			if (transport_wss_ssl) {
				tmp1 = config_setting_get_member(transport_wss_ssl, "key");
				if (tmp1) ssl_key = config_setting_get_string(tmp1);
				tmp1 = config_setting_get_member(transport_wss_ssl, "cert");
				if (tmp1) ssl_cert = config_setting_get_string(tmp1);
				tmp1 = config_setting_get_member(transport_wss_ssl, "chain");
				if (tmp1) ssl_chain = config_setting_get_string(tmp1);
				if (!ssl_key || !ssl_cert || !ssl_chain) return KS_STATUS_FAIL;
				ks_log(KS_LOG_DEBUG,
					"Using SSL: %s, %s, %s\n",
					ssl_key,
					ssl_cert,
					ssl_chain);
			}

			transport_wss_endpoints = config_setting_get_member(transport_wss, "endpoints");
			if (transport_wss_endpoints) {
				transport_wss_endpoints_ipv4 = config_setting_get_member(transport_wss_endpoints, "ipv4");
				transport_wss_endpoints_ipv6 = config_setting_get_member(transport_wss_endpoints, "ipv6");
				if (transport_wss_endpoints_ipv4) {
					if (config_setting_type(transport_wss_endpoints_ipv4) != CONFIG_TYPE_LIST) return KS_STATUS_FAIL;
					if ((endpoints_ipv4_length = config_setting_length(transport_wss_endpoints_ipv4)) > BLADE_MODULE_WSS_ENDPOINTS_MULTIHOME_MAX)
						return KS_STATUS_FAIL;

					for (int32_t index = 0; index < endpoints_ipv4_length; ++index) {
						element = config_setting_get_elem(transport_wss_endpoints_ipv4, index);
						tmp1 = config_setting_get_member(element, "address");
						tmp2 = config_setting_get_member(element, "port");
						if (!tmp1 || !tmp2) return KS_STATUS_FAIL;
						if (config_setting_type(tmp1) != CONFIG_TYPE_STRING) return KS_STATUS_FAIL;
						if (config_setting_type(tmp2) != CONFIG_TYPE_INT) return KS_STATUS_FAIL;

						if (ks_addr_set(&endpoints_ipv4[index],
							config_setting_get_string(tmp1),
							config_setting_get_int(tmp2),
							AF_INET) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
						ks_log(KS_LOG_DEBUG,
							"Binding to IPV4 %s on port %d\n",
							ks_addr_get_host(&endpoints_ipv4[index]),
							ks_addr_get_port(&endpoints_ipv4[index]));
					}
				}
				if (transport_wss_endpoints_ipv6) {
					if (config_setting_type(transport_wss_endpoints_ipv6) != CONFIG_TYPE_LIST) return KS_STATUS_FAIL;
					if ((endpoints_ipv6_length = config_setting_length(transport_wss_endpoints_ipv6)) > BLADE_MODULE_WSS_ENDPOINTS_MULTIHOME_MAX)
						return KS_STATUS_FAIL;

					for (int32_t index = 0; index < endpoints_ipv6_length; ++index) {
						element = config_setting_get_elem(transport_wss_endpoints_ipv6, index);
						tmp1 = config_setting_get_member(element, "address");
						tmp2 = config_setting_get_member(element, "port");
						if (!tmp1 || !tmp2) return KS_STATUS_FAIL;
						if (config_setting_type(tmp1) != CONFIG_TYPE_STRING) return KS_STATUS_FAIL;
						if (config_setting_type(tmp2) != CONFIG_TYPE_INT) return KS_STATUS_FAIL;


						if (ks_addr_set(&endpoints_ipv6[index],
							config_setting_get_string(tmp1),
							config_setting_get_int(tmp2),
							AF_INET6) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
						ks_log(KS_LOG_DEBUG,
							"Binding to IPV6 %s on port %d\n",
							ks_addr_get_host(&endpoints_ipv6[index]),
							ks_addr_get_port(&endpoints_ipv6[index]));
					}
				}
				if (endpoints_ipv4_length + endpoints_ipv6_length <= 0) return KS_STATUS_FAIL;
				tmp1 = config_setting_get_member(transport_wss_endpoints, "backlog");
				if (tmp1) {
					if (config_setting_type(tmp1) != CONFIG_TYPE_INT) return KS_STATUS_FAIL;
					endpoints_backlog = config_setting_get_int(tmp1);
				}
				transport_wss_endpoints_ssl = config_setting_get_member(transport_wss_endpoints, "ssl");
				if (transport_wss_endpoints_ssl) {
					tmp1 = config_setting_get_member(transport_wss_endpoints_ssl, "key");
					if (tmp1) endpoints_ssl_key = config_setting_get_string(tmp1);
					tmp1 = config_setting_get_member(transport_wss_endpoints_ssl, "cert");
					if (tmp1) endpoints_ssl_cert = config_setting_get_string(tmp1);
					tmp1 = config_setting_get_member(transport_wss_endpoints_ssl, "chain");
					if (tmp1) endpoints_ssl_chain = config_setting_get_string(tmp1);
					if (!endpoints_ssl_key || !endpoints_ssl_cert || !endpoints_ssl_chain) return KS_STATUS_FAIL;
					ks_log(KS_LOG_DEBUG,
						"Using Endpoint SSL: %s, %s, %s\n",
						endpoints_ssl_key,
						endpoints_ssl_cert,
						endpoints_ssl_chain);
				}
			}
		}
	}


	// Configuration is valid, now assign it to the variables that are used
	// If the configuration was invalid, then this does not get changed
	if (ssl_key) {
		btwss->ssl_key = ks_pstrdup(pool, ssl_key);
		btwss->ssl_cert = ks_pstrdup(pool, ssl_cert);
		btwss->ssl_chain = ks_pstrdup(pool, ssl_chain);
	}

	for (int32_t index = 0; index < endpoints_ipv4_length; ++index)
		btwss->endpoints_ipv4[index] = endpoints_ipv4[index];
	for (int32_t index = 0; index < endpoints_ipv6_length; ++index)
		btwss->endpoints_ipv6[index] = endpoints_ipv6[index];
	btwss->endpoints_ipv4_length = endpoints_ipv4_length;
	btwss->endpoints_ipv6_length = endpoints_ipv6_length;
	btwss->endpoints_backlog = endpoints_backlog;
	if (endpoints_ssl_key) {
		btwss->endpoints_ssl_key = ks_pstrdup(pool, endpoints_ssl_key);
		btwss->endpoints_ssl_cert = ks_pstrdup(pool, endpoints_ssl_cert);
		btwss->endpoints_ssl_chain = ks_pstrdup(pool, endpoints_ssl_chain);
	}

	ks_log(KS_LOG_DEBUG, "Configured\n");

	return KS_STATUS_SUCCESS;
}


ks_status_t blade_transport_wss_onstartup(blade_transport_t *bt, config_setting_t *config)
{
	blade_transport_wss_t *btwss = NULL;

	ks_assert(bt);
	ks_assert(config);

	btwss = (blade_transport_wss_t *)blade_transport_data_get(bt);

    if (blade_transport_wss_config(btwss, config) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "blade_transport_wss_config failed\n");
		return KS_STATUS_FAIL;
	}

	for (int32_t index = 0; index < btwss->endpoints_ipv4_length; ++index) {
		if (blade_transport_wss_listen(btwss, &btwss->endpoints_ipv4[index]) != KS_STATUS_SUCCESS) {
			ks_log(KS_LOG_DEBUG, "blade_transport_wss_listen (v4) failed\n");
			return KS_STATUS_FAIL;
		}
	}
	for (int32_t index = 0; index < btwss->endpoints_ipv6_length; ++index) {
		if (blade_transport_wss_listen(btwss, &btwss->endpoints_ipv6[index]) != KS_STATUS_SUCCESS) {
			ks_log(KS_LOG_DEBUG, "blade_transport_wss_listen (v6) failed\n");
			return KS_STATUS_FAIL;
		}
	}


	if (btwss->listeners_count > 0 &&
		ks_thread_pool_add_job(blade_handle_tpool_get(btwss->handle), blade_transport_wss_listeners_thread, btwss) != KS_STATUS_SUCCESS) {
		// @todo error logging
		return KS_STATUS_FAIL;
	}

	ks_log(KS_LOG_DEBUG, "Started\n");

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_transport_wss_onshutdown(blade_transport_t *bt)
{
	blade_transport_wss_t *btwss = NULL;

	ks_assert(bt);

	btwss = (blade_transport_wss_t *)blade_transport_data_get(bt);

	if (btwss->listeners_count > 0) {
		btwss->shutdown = KS_TRUE;
		while (btwss->shutdown) ks_sleep_ms(1);
	}

	for (int32_t index = 0; index < btwss->listeners_count; ++index) {
		ks_socket_t sock = btwss->listeners_poll[index].fd;
		ks_socket_shutdown(sock, SHUT_RDWR);
		ks_socket_close(&sock);
	}

	ks_log(KS_LOG_DEBUG, "Stopped\n");

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_transport_wss_listen(blade_transport_wss_t *btwss, ks_sockaddr_t *addr)
{
	ks_socket_t listener = KS_SOCK_INVALID;
	int32_t listener_index = -1;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(btwss);
	ks_assert(addr);

	if ((listener = socket(addr->family, SOCK_STREAM, IPPROTO_TCP)) == KS_SOCK_INVALID) {
		ks_log(KS_LOG_DEBUG, "listener == KS_SOCK_INVALID\n");
		ret = KS_STATUS_FAIL;
		goto done;
	}

	ks_socket_option(listener, SO_REUSEADDR, KS_TRUE);
	ks_socket_option(listener, TCP_NODELAY, KS_TRUE);
	if (addr->family == AF_INET6) ks_socket_option(listener, IPV6_V6ONLY, KS_TRUE);

	if (ks_addr_bind(listener, addr) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "ks_addr_bind(listener, addr) != KS_STATUS_SUCCESS\n");
		ret = KS_STATUS_FAIL;
		goto done;
	}

	if (listen(listener, btwss->endpoints_backlog) != 0) {
		ks_log(KS_LOG_DEBUG, "listen(listener, backlog) != 0\n");
		ret = KS_STATUS_FAIL;
		goto done;
	}

	listener_index = btwss->listeners_count++;
	if (!btwss->listeners_poll) btwss->listeners_poll = (struct pollfd *)ks_pool_alloc(ks_pool_get(btwss), sizeof(struct pollfd) * btwss->listeners_count);
	else btwss->listeners_poll = (struct pollfd *)ks_pool_resize(btwss->listeners_poll, sizeof(struct pollfd) * btwss->listeners_count);
	ks_assert(btwss->listeners_poll);
	btwss->listeners_poll[listener_index].fd = listener;
	btwss->listeners_poll[listener_index].events = POLLIN; // | POLLERR;

	ks_log(KS_LOG_DEBUG, "Bound %s on port %d at index %d\n", ks_addr_get_host(addr), ks_addr_get_port(addr), listener_index);

done:
	if (ret != KS_STATUS_SUCCESS) {
		if (listener != KS_SOCK_INVALID) {
			ks_socket_shutdown(listener, SHUT_RDWR);
			ks_socket_close(&listener);
		}
	}
	return ret;
}

void *blade_transport_wss_listeners_thread(ks_thread_t *thread, void *data)
{
	blade_transport_wss_t *btwss = NULL;
	blade_transport_wss_link_t *btwssl = NULL;
	blade_connection_t *bc = NULL;

	ks_assert(thread);
	ks_assert(data);

	btwss = (blade_transport_wss_t *)data;

	ks_log(KS_LOG_DEBUG, "Started\n");
	while (!btwss->shutdown) {
		// @todo take exact timeout from a setting in config_wss_endpoints
		if (ks_poll(btwss->listeners_poll, btwss->listeners_count, 100) > 0) {
			for (int32_t index = 0; index < btwss->listeners_count; ++index) {
				ks_socket_t sock = KS_SOCK_INVALID;

				if (btwss->listeners_poll[index].revents & POLLERR) {
					// @todo: error handling, just skip the listener for now, it might recover, could skip X times before closing?
					ks_log(KS_LOG_DEBUG, "POLLERR on index %d\n", index);
					continue;
				}
				if (!(btwss->listeners_poll[index].revents & POLLIN)) continue;

				if ((sock = accept(btwss->listeners_poll[index].fd, NULL, NULL)) == KS_SOCK_INVALID) {
					// @todo: error handling, just skip the socket for now as most causes are because remote side became unreachable
					ks_log(KS_LOG_DEBUG, "Accept failed on index %d\n", index);
					continue;
				}

				// @todo getsockname and getpeername (getpeername can be skipped if passing to accept instead)

				ks_log(KS_LOG_DEBUG, "Socket accepted\n", index);

				// @todo make new function to wrap the following code all the way through assigning initial state to reuse in outbound connects
				blade_connection_create(&bc, btwss->handle);
				ks_assert(bc);

				blade_transport_wss_link_create(&btwssl, ks_pool_get(bc), btwss, sock, NULL);
				ks_assert(btwssl);

				blade_connection_transport_set(bc, btwssl, btwss->callbacks);

				blade_connection_read_lock(bc, KS_TRUE);

				if (blade_connection_startup(bc, BLADE_CONNECTION_DIRECTION_INBOUND) != KS_STATUS_SUCCESS) {
					ks_log(KS_LOG_DEBUG, "Connection (%s) startup failed\n", blade_connection_id_get(bc));
					blade_connection_read_unlock(bc);
					blade_connection_destroy(&bc);
					continue;
				}
				ks_log(KS_LOG_DEBUG, "Connection (%s) started\n", blade_connection_id_get(bc));

				blade_connectionmgr_connection_add(blade_handle_connectionmgr_get(btwss->handle), bc);

				blade_connection_state_set(bc, BLADE_CONNECTION_STATE_STARTUP);

				blade_connection_read_unlock(bc);
				// @todo end of reusable function, lock ensures it cannot be destroyed until this code finishes
			}
		}
	}
	ks_log(KS_LOG_DEBUG, "Stopped\n");

	btwss->shutdown = KS_FALSE;

	return NULL;
}

ks_status_t blade_transport_wss_onconnect(blade_connection_t **bcP, blade_transport_t *bt, blade_identity_t *target, const char *session_id)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_transport_wss_t *btwss = NULL;
	ks_sockaddr_t addr;
	ks_socket_t sock = KS_SOCK_INVALID;
	int family = AF_INET;
	const char *ip = NULL;
	const char *portstr = NULL;
	ks_port_t port = 2100;
	blade_transport_wss_link_t *btwssl = NULL;
	blade_connection_t *bc = NULL;

	ks_assert(bcP);
	ks_assert(bt);
	ks_assert(target);

	btwss = (blade_transport_wss_t *)blade_transport_data_get(bt);

	*bcP = NULL;

	ks_log(KS_LOG_DEBUG, "Connect Callback: %s\n", blade_identity_uri_get(target));

	// @todo completely rework all of this once more is known about connecting when an identity has no explicit transport details but this transport
	// has been choosen anyway
	ip = blade_identity_host_get(target);
	portstr = blade_identity_port_get(target);
	if (!ip) {
		ks_log(KS_LOG_DEBUG, "No host provided\n");
		ret = KS_STATUS_FAIL;
		goto done;
	}
	// @todo: this should detect IP's and fall back on DNS and/or SRV for hostname lookup, for the moment hosts must be IP's

	// @todo wrap this code to get address family from string IP between IPV4 and IPV6, and put it in libks somewhere
	{
		ks_size_t len = strlen(ip);

		if (len <= 3) {
			ks_log(KS_LOG_DEBUG, "Invalid host provided\n");
			ret = KS_STATUS_FAIL;
			goto done;
		}
		if (ip[1] == '.' || ip[2] == '.' || (len > 3 && ip[3] == '.')) family = AF_INET;
		else family = AF_INET6;
	}

	if (portstr) {
		int p = atoi(portstr);
		if (p > 0 && p <= UINT16_MAX) port = p;
	}

	ks_log(KS_LOG_DEBUG, "Connecting to %s on port %d\n", ip, port);

	ks_addr_set(&addr, ip, port, family);
	if ((sock = ks_socket_connect(SOCK_STREAM, IPPROTO_TCP, &addr)) == KS_SOCK_INVALID) {
		// @todo: error handling, just fail for now as most causes are because remote side became unreachable
		ks_log(KS_LOG_DEBUG, "Connect failed\n");
		ret = KS_STATUS_FAIL;
		goto done;
	}

	ks_log(KS_LOG_DEBUG, "Socket connected\n");

	// @todo see above listener code, make reusable function for the following code
	blade_connection_create(&bc, btwss->handle);
	ks_assert(bc);

	blade_transport_wss_link_create(&btwssl, ks_pool_get(bc), btwss, sock, session_id);
	ks_assert(btwssl);

	blade_connection_transport_set(bc, btwssl, btwss->callbacks);

	blade_connection_read_lock(bc, KS_TRUE);

	if (blade_connection_startup(bc, BLADE_CONNECTION_DIRECTION_OUTBOUND) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "Connection (%s) startup failed\n", blade_connection_id_get(bc));
		blade_connection_read_unlock(bc);
		blade_connection_destroy(&bc);
		ret = KS_STATUS_FAIL;
		goto done;
	}
	ks_log(KS_LOG_DEBUG, "Connection (%s) started\n", blade_connection_id_get(bc));

	blade_connectionmgr_connection_add(blade_handle_connectionmgr_get(btwss->handle), bc);

	blade_connection_state_set(bc, BLADE_CONNECTION_STATE_STARTUP);

	blade_connection_read_unlock(bc);

	// @todo consider ramification of unlocking above, while returning the new connection object back to the framework, thread might run and disconnect quickly
	// @todo have blade_handle_connect and blade_transport_wss_on_connect (and the abstracted callback) return a copy of the connection id (allocated from blade_handle_t's pool temporarily) rather than the connection pointer itself
	// which will then require getting the connection and thus relock it for any further use, if it disconnects during that time the connection will be locked preventing obtaining and then return NULL if removed
	*bcP = bc;

 done:
	return ret;
}

ks_status_t blade_transport_wss_link_write(blade_transport_wss_link_t *btwssl, cJSON *json)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	char *json_str = NULL;
	ks_size_t json_str_len = 0;

	ks_assert(btwssl);
	ks_assert(json);

	json_str = cJSON_PrintUnformatted(json);
	if (!json_str) {
		ks_log(KS_LOG_DEBUG, "Failed to generate json string\n");
		ret = KS_STATUS_FAIL;
		goto done;
	}
	// @todo determine if WSOC_TEXT null terminates when read_frame is called, or if it's safe to include like this
	json_str_len = strlen(json_str) + 1;
	if ((ks_size_t)kws_write_frame(btwssl->kws, WSOC_TEXT, json_str, json_str_len) != json_str_len) {
		ks_log(KS_LOG_DEBUG, "Failed to write frame\n");
		ret = KS_STATUS_FAIL;
		goto done;
	}
	ks_log(KS_LOG_DEBUG, "Frame written %d bytes\n", json_str_len);

 done:
	if (json_str) free(json_str);

	return ret;
}

ks_status_t blade_transport_wss_onsend(blade_connection_t *bc, cJSON *json)
{
	blade_transport_wss_link_t *btwssl = NULL;

	ks_assert(bc);
	ks_assert(json);

	btwssl = (blade_transport_wss_link_t *)blade_connection_transport_get(bc);

	return blade_transport_wss_link_write(btwssl, json);
}

ks_status_t blade_transport_wss_link_read(blade_transport_wss_link_t *btwssl, cJSON **json)
{
	// @todo get exact timeout from service config?
	int32_t poll_flags = 0;

	ks_assert(btwssl);
	ks_assert(json);

	poll_flags = ks_wait_sock(btwssl->sock, 1, KS_POLL_READ); // | KS_POLL_ERROR);

	*json = NULL;

	if (poll_flags & KS_POLL_ERROR) {
		ks_log(KS_LOG_DEBUG, "POLLERR\n");
		return KS_STATUS_FAIL;
	}
	if (poll_flags & KS_POLL_READ) {
		kws_opcode_t opcode;
		uint8_t *frame_data = NULL;
		ks_ssize_t frame_data_len = kws_read_frame(btwssl->kws, &opcode, &frame_data);

		if (frame_data_len <= 0) {
			// @todo error logging, strerror(ks_errno())
			// 0 means socket closed with WS_NONE, which closes websocket with no additional reason
			// -1 means socket closed with a general failure
			// -2 means nonblocking wait
			// other values are based on WS_XXX reasons
			// negative values are based on reasons, except for -1 is but -2 is nonblocking wait, and
			ks_log(KS_LOG_DEBUG, "Failed to read frame\n");
			return KS_STATUS_FAIL;
		}
		ks_log(KS_LOG_DEBUG, "Frame read %d bytes\n", frame_data_len);

		if (!(*json = cJSON_Parse((char *)frame_data))) {
			ks_log(KS_LOG_DEBUG, "Failed to parse frame\n");
			return KS_STATUS_FAIL;
		}
	}
	return KS_STATUS_SUCCESS;
}

ks_status_t blade_transport_wss_onreceive(blade_connection_t *bc, cJSON **json)
{
	blade_transport_wss_link_t *btwssl = NULL;

	ks_assert(bc);
	ks_assert(json);

	btwssl = (blade_transport_wss_link_t *)blade_connection_transport_get(bc);

	return blade_transport_wss_link_read(btwssl, json);
}

ks_status_t blade_transport_wss_rpc_error_send(blade_connection_t *bc, const char *id, int32_t code, const char *message)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_transport_wss_link_t *btwssl = NULL;
	cJSON *json = NULL;

	ks_assert(bc);
	//ks_assert(id);
	ks_assert(message);

	btwssl = (blade_transport_wss_link_t *)blade_connection_transport_get(bc);

	blade_rpc_error_raw_create(&json, NULL, id, code, message);

    if (blade_transport_wss_link_write(btwssl, json) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "Failed to write error message\n");
		ret = KS_STATUS_FAIL;
	}

	cJSON_Delete(json);
	return ret;
}


blade_connection_state_hook_t blade_transport_wss_onstate_startup_inbound(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	blade_connection_state_hook_t ret = BLADE_CONNECTION_STATE_HOOK_SUCCESS;
	blade_transport_wss_link_t *btwssl = NULL;
	cJSON *json_req = NULL;
	cJSON *json_res = NULL;
	cJSON *json_params = NULL;
	cJSON *json_result = NULL;
	//cJSON *error = NULL;
	blade_session_t *bs = NULL;
	blade_handle_t *bh = NULL;
	const char *jsonrpc = NULL;
	const char *id = NULL;
	const char *method = NULL;
	const char *sessionid = NULL;
	uuid_t uuid;
	const char *nodeid = NULL;
	ks_time_t timeout;

	ks_assert(bc);

	bh = blade_connection_handle_get(bc);
	ks_assert(bh);

	ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);

	if (condition == BLADE_CONNECTION_STATE_CONDITION_PRE) return BLADE_CONNECTION_STATE_HOOK_SUCCESS;

	btwssl = (blade_transport_wss_link_t *)blade_connection_transport_get(bc);

	if (blade_transport_wss_link_ssl_init(btwssl, KS_TRUE) != KS_STATUS_SUCCESS) {
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	if (kws_init(&btwssl->kws, btwssl->sock, btwssl->ssl, NULL, KWS_BLOCK, ks_pool_get(btwssl)) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "Failed websocket init\n");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	// @todo very temporary, really need monotonic clock and get timeout delay and sleep delay from config
	timeout = ks_time_now() + (5 * KS_USEC_PER_SEC);
	while (blade_transport_wss_link_read(btwssl, &json_req) == KS_STATUS_SUCCESS) {
		if (json_req) break;
		ks_sleep_ms(250);
		if (ks_time_now() >= timeout) break;
	}

	if (!json_req) {
		ks_log(KS_LOG_DEBUG, "Failed to receive message before timeout\n");
		blade_transport_wss_rpc_error_send(bc, NULL, -32600, "Timeout while expecting request");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	// @todo start here for a reusable handler for "blade.connect" request rpc method within transport implementations,
	// output 2 parameters for response and error, if an error occurs, send it, otherwise send the response

	jsonrpc = cJSON_GetObjectCstr(json_req, "jsonrpc"); // @todo check for definitions of these keys and fixed values
	if (!jsonrpc || strcmp(jsonrpc, "2.0")) {
		ks_log(KS_LOG_DEBUG, "Received message is not the expected protocol\n");
		blade_transport_wss_rpc_error_send(bc, NULL, -32600, "Invalid request, missing 'jsonrpc' field");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	id = cJSON_GetObjectCstr(json_req, "id");
	if (!id) {
		ks_log(KS_LOG_DEBUG, "Received message is missing 'id'\n");
		blade_transport_wss_rpc_error_send(bc, NULL, -32600, "Invalid request, missing 'id' field");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	method = cJSON_GetObjectCstr(json_req, "method");
	if (!method || strcasecmp(method, "blade.connect")) {
		ks_log(KS_LOG_DEBUG, "Received message is missing 'method' or is an unexpected method\n");
		blade_transport_wss_rpc_error_send(bc, id, -32601, "Missing or unexpected 'method' field");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	json_params = cJSON_GetObjectItem(json_req, "params");
	if (json_params) {
		sessionid = cJSON_GetObjectCstr(json_params, "sessionid");
		if (sessionid) {
			ks_log(KS_LOG_DEBUG, "Session (%s) requested\n", sessionid);
		}
	}

	ks_uuid(&uuid);
	nodeid = ks_uuid_str(ks_pool_get(bc), &uuid);

	if (sessionid) {
		bs = blade_sessionmgr_session_lookup(blade_handle_sessionmgr_get(bh), sessionid);
		if (bs) {
			if (blade_session_terminating(bs)) {
				blade_session_read_unlock(bs);
				ks_log(KS_LOG_DEBUG, "Session (%s) terminating\n", blade_session_id_get(bs));
				bs = NULL;
			} else {
				ks_log(KS_LOG_DEBUG, "Session (%s) located\n", blade_session_id_get(bs));
				// @todo validate against IP address or something to ensure reconnects are acceptable
			}
		}
	}

	if (!bs) {
		blade_session_create(&bs, bh, BLADE_SESSION_FLAGS_NONE, NULL);
		ks_assert(bs);

		sessionid = blade_session_id_get(bs);
		ks_log(KS_LOG_DEBUG, "Session (%s) created\n", sessionid);

		blade_session_read_lock(bs, KS_TRUE); // this will be done by blade_handle_sessions_get() otherwise

		if (blade_session_startup(bs) != KS_STATUS_SUCCESS) {
			ks_log(KS_LOG_DEBUG, "Session (%s) startup failed\n", sessionid);
			blade_transport_wss_rpc_error_send(bc, id, -32603, "Internal error, session could not be started");
			blade_session_read_unlock(bs);
			blade_session_destroy(&bs);
			ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
			goto done;
		}

		// This is an inbound connection, thus it is always creating a downstream session

		ks_log(KS_LOG_DEBUG, "Session (%s) started\n", sessionid);
		blade_sessionmgr_session_add(blade_handle_sessionmgr_get(bh), bs);

		// This is primarily to cleanup the routes added to the blade_handle for main routing when a session terminates, these don't have a lot of use otherwise but it will keep the main route table
		// from having long running write locks when a session cleans up
		blade_session_route_add(bs, nodeid);
		// This is the main routing entry to make an identity routable through a session when a message is received for a given identity in this table, these allow to efficiently determine which session
		// a message should pass through when it does not match the local node id from blade_routemgr_t, and must be matched with a call to blade_session_route_add() for cleanup, additionally when
		// a "blade.route" is received the identity it carries affects these routes along with the sessionid of the downstream session it came through, and "blade.route" would also
		// result in the new identities being added as routes however federation registration would require a special process to maintain proper routing
		blade_routemgr_route_add(blade_handle_routemgr_get(bh), nodeid, sessionid);
	}

	blade_rpc_response_raw_create(&json_res, &json_result, id);
	ks_assert(json_res);

	cJSON_AddStringToObject(json_result, "sessionid", sessionid);
	cJSON_AddStringToObject(json_result, "nodeid", nodeid);

	if (!blade_routemgr_master_pack(blade_handle_routemgr_get(bh), json_result, "master-nodeid")) {
		ks_log(KS_LOG_DEBUG, "Master nodeid unavailable, upstream is not established\n");
		blade_transport_wss_rpc_error_send(bc, id, -32602, "Master nodeid unavailable");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	// This starts the final process for associating the connection to the session, including for reconnecting to an existing session, this simply
	// associates the session to this connection, upon return the remainder of the association for the session to the connection is handled along
	// with making sure both this connection and the session state machines are in running states
	blade_connection_session_set(bc, sessionid);

	// @todo end of reusable handler for "blade.connect" request

	if (blade_transport_wss_link_write(btwssl, json_res) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "Failed to write response message\n");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

 done:
	// @note the state machine expects if we return SUCCESS, that the session assigned to the connection will be read locked to ensure that the state
	// machine can finish attaching the session, if you BYPASS then you can handle everything here in the callback, but this should be fairly standard
	// behaviour to simply go as far as assigning a session to the connection and let the system handle the rest
	if (json_req) cJSON_Delete(json_req);
	if (json_res) cJSON_Delete(json_res);

	return ret;
}

blade_connection_state_hook_t blade_transport_wss_onstate_startup_outbound(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	blade_connection_state_hook_t ret = BLADE_CONNECTION_STATE_HOOK_SUCCESS;
	blade_handle_t *bh = NULL;
	blade_transport_wss_link_t *btwssl = NULL;
	ks_pool_t *pool = NULL;
	cJSON *json_req = NULL;
	cJSON *json_params = NULL;
	cJSON *json_res = NULL;
	const char *mid = NULL;
	ks_time_t timeout;
	const char *jsonrpc = NULL;
	const char *id = NULL;
	cJSON *json_error = NULL;
	cJSON *json_result = NULL;
	const char *sessionid = NULL;
	const char *nodeid = NULL;
	const char *master_nodeid = NULL;
	blade_session_t *bs = NULL;

	ks_assert(bc);

	ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);

	if (condition == BLADE_CONNECTION_STATE_CONDITION_PRE) return BLADE_CONNECTION_STATE_HOOK_SUCCESS;

	bh = blade_connection_handle_get(bc);
	btwssl = (blade_transport_wss_link_t *)blade_connection_transport_get(bc);
	pool = ks_pool_get(bh);

	if (blade_transport_wss_link_ssl_init(btwssl, KS_FALSE) != KS_STATUS_SUCCESS) {
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	if (kws_init(&btwssl->kws, btwssl->sock, btwssl->ssl, "/blade:blade.invalid:blade", KWS_BLOCK, ks_pool_get(btwssl)) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "Failed websocket init\n");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	blade_rpc_request_raw_create(pool, &json_req, &json_params, &mid, "blade.connect");
	ks_assert(json_req);

	if (btwssl->session_id) cJSON_AddStringToObject(json_params, "sessionid", btwssl->session_id);

	ks_log(KS_LOG_DEBUG, "Session (%s) requested\n", (btwssl->session_id ? btwssl->session_id : "none"));

	if (blade_transport_wss_link_write(btwssl, json_req) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "Failed to write request message\n");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}


	timeout = ks_time_now() + (5 * KS_USEC_PER_SEC);
	while (blade_transport_wss_link_read(btwssl, &json_res) == KS_STATUS_SUCCESS) {
		if (json_res) break;
		ks_sleep_ms(250);
		if (ks_time_now() >= timeout) break;
	}

	if (!json_res) {
		ks_log(KS_LOG_DEBUG, "Failed to receive message before timeout\n");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	// @todo start here for a reusable handler for "blade.connect" response rpc method within transport implementations

	jsonrpc = cJSON_GetObjectCstr(json_res, "jsonrpc"); // @todo check for definitions of these keys and fixed values
	if (!jsonrpc || strcmp(jsonrpc, "2.0")) {
		ks_log(KS_LOG_DEBUG, "Received message is not the expected protocol\n");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	id = cJSON_GetObjectCstr(json_res, "id"); // @todo switch to number if we are not using a uuid for message id
	if (!id || strcasecmp(mid, id)) {
		ks_log(KS_LOG_DEBUG, "Received message has missing or unexpected 'id'\n");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	json_error = cJSON_GetObjectItem(json_res, "error");
	if (json_error) {
		ks_log(KS_LOG_DEBUG, "Error message ... add the details\n");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	json_result = cJSON_GetObjectItem(json_res, "result");
	if (!json_result) {
		ks_log(KS_LOG_DEBUG, "Received message is missing 'result'\n");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	sessionid = cJSON_GetObjectCstr(json_result, "sessionid");
	if (!sessionid) {
		ks_log(KS_LOG_DEBUG, "Received message 'result' is missing 'sessionid'\n");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	nodeid = cJSON_GetObjectCstr(json_result, "nodeid");
	if (!nodeid) {
		ks_log(KS_LOG_DEBUG, "Received message 'result' is missing 'nodeid'\n");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	master_nodeid = cJSON_GetObjectCstr(json_result, "master-nodeid");
	if (!master_nodeid) {
		ks_log(KS_LOG_DEBUG, "Received message 'result' is missing 'master-nodeid'\n");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	bs = blade_sessionmgr_upstream_lookup(blade_handle_sessionmgr_get(bh));
	if (bs) {
		if (ks_safe_strcasecmp(blade_session_id_get(bs), sessionid)) {
			ks_log(KS_LOG_DEBUG, "Already have upstream session with different sessionid, could not establish session\n");
			blade_session_read_unlock(bs);
			ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
			goto done;
		}
		ks_log(KS_LOG_DEBUG, "Session (%s) reestablishing\n", blade_session_id_get(bs));
	}

	if (!bs) {
		blade_session_create(&bs, bh, BLADE_SESSION_FLAGS_UPSTREAM, sessionid);
		ks_assert(bs);

		ks_log(KS_LOG_DEBUG, "Session (%s) created\n", blade_session_id_get(bs));

		blade_session_read_lock(bs, KS_TRUE); // this will be done by blade_handle_sessions_get() otherwise

		if (blade_session_startup(bs) != KS_STATUS_SUCCESS) {
			ks_log(KS_LOG_DEBUG, "Session (%s) startup failed\n", blade_session_id_get(bs));
			blade_session_read_unlock(bs);
			blade_session_destroy(&bs);
			ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
			goto done;
		}

		// This is an outbound connection, thus it is always creating an upstream session
		ks_log(KS_LOG_DEBUG, "Session (%s) started\n", blade_session_id_get(bs));
		blade_sessionmgr_session_add(blade_handle_sessionmgr_get(bh), bs);

		blade_routemgr_local_set(blade_handle_routemgr_get(bh), nodeid);
		blade_routemgr_master_set(blade_handle_routemgr_get(bh), master_nodeid);
	}

	blade_connection_session_set(bc, blade_session_id_get(bs));

	// @todo end of reusable handler for "blade.connect" response

 done:
	if (json_req) cJSON_Delete(json_req);
	if (json_res) cJSON_Delete(json_res);

	return ret;
}

blade_connection_state_hook_t blade_transport_wss_onstate_shutdown(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	ks_assert(bc);

	ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);

	if (condition == BLADE_CONNECTION_STATE_CONDITION_PRE) return BLADE_CONNECTION_STATE_HOOK_SUCCESS;

	return BLADE_CONNECTION_STATE_HOOK_SUCCESS;
}

blade_connection_state_hook_t blade_transport_wss_onstate_run_inbound(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	ks_assert(bc);

	if (condition == BLADE_CONNECTION_STATE_CONDITION_PRE) {
		ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);
	}

	return BLADE_CONNECTION_STATE_HOOK_SUCCESS;
}

blade_connection_state_hook_t blade_transport_wss_onstate_run_outbound(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	ks_assert(bc);

	if (condition == BLADE_CONNECTION_STATE_CONDITION_PRE) {
		ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);
	}

	return BLADE_CONNECTION_STATE_HOOK_SUCCESS;
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
