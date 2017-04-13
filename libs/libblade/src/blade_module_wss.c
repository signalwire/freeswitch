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

typedef struct blade_module_wss_s blade_module_wss_t;
typedef struct blade_transport_wss_s blade_transport_wss_t;
typedef struct blade_transport_wss_init_s blade_transport_wss_init_t;

struct blade_module_wss_s {
	blade_handle_t *handle;
	ks_pool_t *pool;
	ks_thread_pool_t *tpool;
	blade_module_t *module;
	blade_module_callbacks_t *module_callbacks;
	blade_transport_callbacks_t *transport_callbacks;

	ks_sockaddr_t config_wss_endpoints_ipv4[BLADE_MODULE_WSS_ENDPOINTS_MULTIHOME_MAX];
	ks_sockaddr_t config_wss_endpoints_ipv6[BLADE_MODULE_WSS_ENDPOINTS_MULTIHOME_MAX];
	int32_t config_wss_endpoints_ipv4_length;
	int32_t config_wss_endpoints_ipv6_length;
	int32_t config_wss_endpoints_backlog;

	ks_bool_t shutdown;

	ks_thread_t *listeners_thread;
	struct pollfd *listeners_poll;
	int32_t listeners_count;

	list_t connected; // @todo consider keeping this only as the list of connection id's, since the handle retains the pointer lookup
};

struct blade_transport_wss_s {
	blade_module_wss_t *module;
	ks_pool_t *pool;

	ks_socket_t sock;
	kws_t *kws;
};

struct blade_transport_wss_init_s {
	blade_module_wss_t *module;
	ks_pool_t *pool;

	ks_socket_t sock;
	const char *session_id;
};



ks_status_t blade_module_wss_create(blade_module_wss_t **bm_wssP, blade_handle_t *bh);
ks_status_t blade_module_wss_destroy(blade_module_wss_t **bm_wssP);

// @todo remove exporting this, it's only temporary until DSO loading is in place so wss module can be loaded
KS_DECLARE(ks_status_t) blade_module_wss_on_load(blade_module_t **bmP, blade_handle_t *bh);
KS_DECLARE(ks_status_t) blade_module_wss_on_unload(blade_module_t *bm);
KS_DECLARE(ks_status_t) blade_module_wss_on_startup(blade_module_t *bm, config_setting_t *config);
KS_DECLARE(ks_status_t) blade_module_wss_on_shutdown(blade_module_t *bm);

ks_status_t blade_module_wss_listen(blade_module_wss_t *bm, ks_sockaddr_t *addr);
void *blade_module_wss_listeners_thread(ks_thread_t *thread, void *data);



ks_status_t blade_transport_wss_create(blade_transport_wss_t **bt_wssP, blade_module_wss_t *bm_wss, ks_socket_t sock);
ks_status_t blade_transport_wss_destroy(blade_transport_wss_t **bt_wssP);

ks_status_t blade_transport_wss_on_connect(blade_connection_t **bcP, blade_module_t *bm, blade_identity_t *target, const char *session_id);
blade_connection_rank_t blade_transport_wss_on_rank(blade_connection_t *bc, blade_identity_t *target);

ks_status_t blade_transport_wss_on_send(blade_connection_t *bc, cJSON *json);
ks_status_t blade_transport_wss_on_receive(blade_connection_t *bc, cJSON **json);

blade_connection_state_hook_t blade_transport_wss_on_state_disconnect(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_on_state_new_inbound(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_on_state_new_outbound(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_on_state_connect_inbound(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_on_state_connect_outbound(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_on_state_attach_inbound(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_on_state_attach_outbound(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_on_state_detach_inbound(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_on_state_detach_outbound(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_on_state_ready_inbound(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_on_state_ready_outbound(blade_connection_t *bc, blade_connection_state_condition_t condition);



ks_status_t blade_transport_wss_init_create(blade_transport_wss_init_t **bt_wssiP, blade_module_wss_t *bm_wss, ks_socket_t sock, const char *session_id);
ks_status_t blade_transport_wss_init_destroy(blade_transport_wss_init_t **bt_wssiP);


ks_bool_t blade_test_echo_request_handler(blade_module_t *bm, blade_request_t *breq);
ks_bool_t blade_test_echo_response_handler(blade_response_t *bres);


static blade_module_callbacks_t g_module_wss_callbacks =
{
	blade_module_wss_on_load,
	blade_module_wss_on_unload,
	blade_module_wss_on_startup,
	blade_module_wss_on_shutdown,
};

static blade_transport_callbacks_t g_transport_wss_callbacks =
{
	blade_transport_wss_on_connect,
	blade_transport_wss_on_rank,
	blade_transport_wss_on_send,
	blade_transport_wss_on_receive,

	blade_transport_wss_on_state_disconnect,
	blade_transport_wss_on_state_disconnect,
	blade_transport_wss_on_state_new_inbound,
	blade_transport_wss_on_state_new_outbound,
	blade_transport_wss_on_state_connect_inbound,
	blade_transport_wss_on_state_connect_outbound,
	blade_transport_wss_on_state_attach_inbound,
	blade_transport_wss_on_state_attach_outbound,
	blade_transport_wss_on_state_detach_inbound,
	blade_transport_wss_on_state_detach_outbound,
	blade_transport_wss_on_state_ready_inbound,
	blade_transport_wss_on_state_ready_outbound,
};



ks_status_t blade_module_wss_create(blade_module_wss_t **bm_wssP, blade_handle_t *bh)
{
	blade_module_wss_t *bm_wss = NULL;
	ks_pool_t *pool = NULL;

	ks_assert(bm_wssP);
	ks_assert(bh);

	pool = blade_handle_pool_get(bh);

    bm_wss = ks_pool_alloc(pool, sizeof(blade_module_wss_t));
	bm_wss->handle = bh;
	bm_wss->pool = pool;
	bm_wss->tpool = blade_handle_tpool_get(bh);

	blade_module_create(&bm_wss->module, bh, bm_wss, &g_module_wss_callbacks);
	bm_wss->module_callbacks = &g_module_wss_callbacks;
	bm_wss->transport_callbacks = &g_transport_wss_callbacks;

	list_init(&bm_wss->connected);

	*bm_wssP = bm_wss;

	ks_log(KS_LOG_DEBUG, "Created\n");

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_module_wss_destroy(blade_module_wss_t **bm_wssP)
{
	blade_module_wss_t *bm_wss = NULL;

	ks_assert(bm_wssP);
	ks_assert(*bm_wssP);

	bm_wss = *bm_wssP;

	blade_module_wss_on_shutdown(bm_wss->module);

	blade_module_destroy(&bm_wss->module);

	list_destroy(&bm_wss->connected);

	ks_pool_free(bm_wss->pool, bm_wssP);

	ks_log(KS_LOG_DEBUG, "Destroyed\n");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_module_wss_on_load(blade_module_t **bmP, blade_handle_t *bh)
{
	blade_module_wss_t *bm_wss = NULL;

	ks_assert(bmP);
	ks_assert(bh);

	blade_module_wss_create(&bm_wss, bh);
	ks_assert(bm_wss);

	*bmP = bm_wss->module;

	ks_log(KS_LOG_DEBUG, "Loaded\n");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_module_wss_on_unload(blade_module_t *bm)
{
	blade_module_wss_t *bm_wss = NULL;

	ks_assert(bm);

	bm_wss = blade_module_data_get(bm);

	blade_module_wss_destroy(&bm_wss);

	ks_log(KS_LOG_DEBUG, "Unloaded\n");

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_transport_wss_init_create(blade_transport_wss_init_t **bt_wssiP, blade_module_wss_t *bm_wss, ks_socket_t sock, const char *session_id)
{
	blade_transport_wss_init_t *bt_wssi = NULL;

	ks_assert(bt_wssiP);
	ks_assert(bm_wss);
	ks_assert(sock != KS_SOCK_INVALID);

    bt_wssi = ks_pool_alloc(bm_wss->pool, sizeof(blade_transport_wss_init_t));
	bt_wssi->module = bm_wss;
	bt_wssi->pool = bm_wss->pool;
	bt_wssi->sock = sock;
	if (session_id) bt_wssi->session_id = ks_pstrdup(bt_wssi->pool, session_id);

	*bt_wssiP = bt_wssi;

	ks_log(KS_LOG_DEBUG, "Created\n");

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_transport_wss_init_destroy(blade_transport_wss_init_t **bt_wssiP)
{
	blade_transport_wss_init_t *bt_wssi = NULL;

	ks_assert(bt_wssiP);
	ks_assert(*bt_wssiP);

	bt_wssi = *bt_wssiP;

	if (bt_wssi->session_id) ks_pool_free(bt_wssi->pool, &bt_wssi->session_id);

	ks_pool_free(bt_wssi->pool, bt_wssiP);

	ks_log(KS_LOG_DEBUG, "Destroyed\n");

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_module_wss_config(blade_module_wss_t *bm_wss, config_setting_t *config)
{
	config_setting_t *wss = NULL;
	config_setting_t *wss_endpoints = NULL;
	config_setting_t *wss_endpoints_ipv4 = NULL;
	config_setting_t *wss_endpoints_ipv6 = NULL;
	config_setting_t *wss_ssl = NULL;
    config_setting_t *element;
	config_setting_t *tmp1;
	config_setting_t *tmp2;
	ks_sockaddr_t config_wss_endpoints_ipv4[BLADE_MODULE_WSS_ENDPOINTS_MULTIHOME_MAX];
	ks_sockaddr_t config_wss_endpoints_ipv6[BLADE_MODULE_WSS_ENDPOINTS_MULTIHOME_MAX];
	int32_t config_wss_endpoints_ipv4_length = 0;
	int32_t config_wss_endpoints_ipv6_length = 0;
	int32_t config_wss_endpoints_backlog = 8;

	ks_assert(bm_wss);
	ks_assert(config);

	if (!config_setting_is_group(config)) {
		ks_log(KS_LOG_DEBUG, "!config_setting_is_group(config)\n");
		return KS_STATUS_FAIL;
	}

	wss = config_setting_get_member(config, "wss");
	if (wss) {
		wss_endpoints = config_setting_get_member(wss, "endpoints");
		if (!wss_endpoints) {
			ks_log(KS_LOG_DEBUG, "!wss_endpoints\n");
			return KS_STATUS_FAIL;
		}
		wss_endpoints_ipv4 = config_lookup_from(wss_endpoints, "ipv4");
		wss_endpoints_ipv6 = config_lookup_from(wss_endpoints, "ipv6");
		if (wss_endpoints_ipv4) {
			if (config_setting_type(wss_endpoints_ipv4) != CONFIG_TYPE_LIST) return KS_STATUS_FAIL;
			if ((config_wss_endpoints_ipv4_length = config_setting_length(wss_endpoints_ipv4)) > BLADE_MODULE_WSS_ENDPOINTS_MULTIHOME_MAX)
				return KS_STATUS_FAIL;

			for (int32_t index = 0; index < config_wss_endpoints_ipv4_length; ++index) {
				element = config_setting_get_elem(wss_endpoints_ipv4, index);
				tmp1 = config_lookup_from(element, "address");
				tmp2 = config_lookup_from(element, "port");
				if (!tmp1 || !tmp2) return KS_STATUS_FAIL;
				if (config_setting_type(tmp1) != CONFIG_TYPE_STRING) return KS_STATUS_FAIL;
				if (config_setting_type(tmp2) != CONFIG_TYPE_INT) return KS_STATUS_FAIL;

				if (ks_addr_set(&config_wss_endpoints_ipv4[index],
								config_setting_get_string(tmp1),
								config_setting_get_int(tmp2),
								AF_INET) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
				ks_log(KS_LOG_DEBUG,
					   "Binding to IPV4 %s on port %d\n",
					   ks_addr_get_host(&config_wss_endpoints_ipv4[index]),
					   ks_addr_get_port(&config_wss_endpoints_ipv4[index]));
			}
		}
		if (wss_endpoints_ipv6) {
			if (config_setting_type(wss_endpoints_ipv6) != CONFIG_TYPE_LIST) return KS_STATUS_FAIL;
			if ((config_wss_endpoints_ipv6_length = config_setting_length(wss_endpoints_ipv6)) > BLADE_MODULE_WSS_ENDPOINTS_MULTIHOME_MAX)
				return KS_STATUS_FAIL;

			for (int32_t index = 0; index < config_wss_endpoints_ipv6_length; ++index) {
				element = config_setting_get_elem(wss_endpoints_ipv6, index);
				tmp1 = config_lookup_from(element, "address");
				tmp2 = config_lookup_from(element, "port");
				if (!tmp1 || !tmp2) return KS_STATUS_FAIL;
				if (config_setting_type(tmp1) != CONFIG_TYPE_STRING) return KS_STATUS_FAIL;
				if (config_setting_type(tmp2) != CONFIG_TYPE_INT) return KS_STATUS_FAIL;


				if (ks_addr_set(&config_wss_endpoints_ipv6[index],
								config_setting_get_string(tmp1),
								config_setting_get_int(tmp2),
								AF_INET6) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
				ks_log(KS_LOG_DEBUG,
					   "Binding to IPV6 %s on port %d\n",
					   ks_addr_get_host(&config_wss_endpoints_ipv6[index]),
					   ks_addr_get_port(&config_wss_endpoints_ipv6[index]));
			}
		}
		if (config_wss_endpoints_ipv4_length + config_wss_endpoints_ipv6_length <= 0) return KS_STATUS_FAIL;
		tmp1 = config_lookup_from(wss_endpoints, "backlog");
		if (tmp1) {
			if (config_setting_type(tmp1) != CONFIG_TYPE_INT) return KS_STATUS_FAIL;
			config_wss_endpoints_backlog = config_setting_get_int(tmp1);
		}
		wss_ssl = config_setting_get_member(wss, "ssl");
		if (wss_ssl) {
			// @todo: SSL stuffs from wss_ssl into config_wss_ssl envelope
		}
	}


	// Configuration is valid, now assign it to the variables that are used
	// If the configuration was invalid, then this does not get changed
	for (int32_t index = 0; index < config_wss_endpoints_ipv4_length; ++index)
		bm_wss->config_wss_endpoints_ipv4[index] = config_wss_endpoints_ipv4[index];
	for (int32_t index = 0; index < config_wss_endpoints_ipv6_length; ++index)
		bm_wss->config_wss_endpoints_ipv6[index] = config_wss_endpoints_ipv6[index];
	bm_wss->config_wss_endpoints_ipv4_length = config_wss_endpoints_ipv4_length;
	bm_wss->config_wss_endpoints_ipv6_length = config_wss_endpoints_ipv6_length;
	bm_wss->config_wss_endpoints_backlog = config_wss_endpoints_backlog;
	//bm_wss->config_wss_ssl = config_wss_ssl;

	ks_log(KS_LOG_DEBUG, "Configured\n");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_module_wss_on_startup(blade_module_t *bm, config_setting_t *config)
{
	blade_module_wss_t *bm_wss = NULL;
	blade_space_t *space = NULL;
	blade_method_t *method = NULL;

	ks_assert(bm);
	ks_assert(config);

	bm_wss = (blade_module_wss_t *)blade_module_data_get(bm);

    if (blade_module_wss_config(bm_wss, config) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "blade_module_wss_config failed\n");
		return KS_STATUS_FAIL;
	}

	for (int32_t index = 0; index < bm_wss->config_wss_endpoints_ipv4_length; ++index) {
		if (blade_module_wss_listen(bm_wss, &bm_wss->config_wss_endpoints_ipv4[index]) != KS_STATUS_SUCCESS) {
			ks_log(KS_LOG_DEBUG, "blade_module_wss_listen (v4) failed\n");
			return KS_STATUS_FAIL;
		}
	}
	for (int32_t index = 0; index < bm_wss->config_wss_endpoints_ipv6_length; ++index) {
		if (blade_module_wss_listen(bm_wss, &bm_wss->config_wss_endpoints_ipv6[index]) != KS_STATUS_SUCCESS) {
			ks_log(KS_LOG_DEBUG, "blade_module_wss_listen (v6) failed\n");
			return KS_STATUS_FAIL;
		}
	}

	if (ks_thread_create_ex(&bm_wss->listeners_thread,
							blade_module_wss_listeners_thread,
							bm_wss,
							KS_THREAD_FLAG_DEFAULT,
							KS_THREAD_DEFAULT_STACK,
							KS_PRI_NORMAL,
							bm_wss->pool) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	blade_handle_transport_register(bm_wss->handle, bm, BLADE_MODULE_WSS_TRANSPORT_NAME, bm_wss->transport_callbacks);


	blade_space_create(&space, bm_wss->handle, bm, "blade.test");
	ks_assert(space);

	blade_method_create(&method, space, "echo", blade_test_echo_request_handler);
	ks_assert(method);

	blade_space_methods_add(space, method);

	blade_handle_space_register(space);

	ks_log(KS_LOG_DEBUG, "Started\n");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_module_wss_on_shutdown(blade_module_t *bm)
{
	blade_module_wss_t *bm_wss = NULL;
	blade_connection_t *bc = NULL;
	ks_bool_t stopped = KS_FALSE;

	ks_assert(bm);

	bm_wss = (blade_module_wss_t *)blade_module_data_get(bm);

	blade_handle_transport_unregister(bm_wss->handle, BLADE_MODULE_WSS_TRANSPORT_NAME);

	if (bm_wss->listeners_thread) {
		bm_wss->shutdown = KS_TRUE;
		ks_thread_join(bm_wss->listeners_thread);
		ks_pool_free(bm_wss->pool, &bm_wss->listeners_thread);
		bm_wss->shutdown = KS_FALSE;
		stopped = KS_TRUE;
	}

	for (int32_t index = 0; index < bm_wss->listeners_count; ++index) {
		ks_socket_t sock = bm_wss->listeners_poll[index].fd;
		ks_socket_shutdown(sock, SHUT_RDWR);
		ks_socket_close(&sock);
	}
	bm_wss->listeners_count = 0;
	if (bm_wss->listeners_poll) ks_pool_free(bm_wss->pool, &bm_wss->listeners_poll);

	if (list_size(&bm_wss->connected) > 0) {
		// this approach to shutdown is cleaner, ensures connections will detach from sessions and be destroyed all in the same places
		list_iterator_start(&bm_wss->connected);
		while (list_iterator_hasnext(&bm_wss->connected)) {
			bc = (blade_connection_t *)list_iterator_next(&bm_wss->connected);
			blade_connection_disconnect(bc);
		}
		list_iterator_stop(&bm_wss->connected);
		while (list_size(&bm_wss->connected) > 0) ks_sleep_ms(100);
	}

	if (stopped) ks_log(KS_LOG_DEBUG, "Stopped\n");

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_module_wss_listen(blade_module_wss_t *bm_wss, ks_sockaddr_t *addr)
{
	ks_socket_t listener = KS_SOCK_INVALID;
	int32_t listener_index = -1;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(bm_wss);
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

	if (listen(listener, bm_wss->config_wss_endpoints_backlog) != 0) {
		ks_log(KS_LOG_DEBUG, "listen(listener, backlog) != 0\n");
		ret = KS_STATUS_FAIL;
		goto done;
	}

	listener_index = bm_wss->listeners_count++;
	bm_wss->listeners_poll = (struct pollfd *)ks_pool_resize(bm_wss->pool,
															 bm_wss->listeners_poll,
															 sizeof(struct pollfd) * bm_wss->listeners_count);
	ks_assert(bm_wss->listeners_poll);
	bm_wss->listeners_poll[listener_index].fd = listener;
	bm_wss->listeners_poll[listener_index].events = POLLIN; // | POLLERR;

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

void *blade_module_wss_listeners_thread(ks_thread_t *thread, void *data)
{
	blade_module_wss_t *bm_wss = NULL;
	blade_transport_wss_init_t *bt_wss_init = NULL;
	blade_connection_t *bc = NULL;

	ks_assert(thread);
	ks_assert(data);

	bm_wss = (blade_module_wss_t *)data;

	ks_log(KS_LOG_DEBUG, "Started\n");
	while (!bm_wss->shutdown) {
		// @todo take exact timeout from a setting in config_wss_endpoints
		if (ks_poll(bm_wss->listeners_poll, bm_wss->listeners_count, 100) > 0) {
			for (int32_t index = 0; index < bm_wss->listeners_count; ++index) {
				ks_socket_t sock = KS_SOCK_INVALID;

				if (bm_wss->listeners_poll[index].revents & POLLERR) {
					// @todo: error handling, just skip the listener for now, it might recover, could skip X times before closing?
					ks_log(KS_LOG_DEBUG, "POLLERR on index %d\n", index);
					continue;
				}
				if (!(bm_wss->listeners_poll[index].revents & POLLIN)) continue;

				if ((sock = accept(bm_wss->listeners_poll[index].fd, NULL, NULL)) == KS_SOCK_INVALID) {
					// @todo: error handling, just skip the socket for now as most causes are because remote side became unreachable
					ks_log(KS_LOG_DEBUG, "Accept failed on index %d\n", index);
					continue;
				}

				// @todo getsockname and getpeername (getpeername can be skipped if passing to accept instead)

				ks_log(KS_LOG_DEBUG, "Socket accepted\n", index);

				blade_transport_wss_init_create(&bt_wss_init, bm_wss, sock, NULL);
				ks_assert(bt_wss_init);

                blade_connection_create(&bc, bm_wss->handle, bt_wss_init, bm_wss->transport_callbacks);
				ks_assert(bc);

				blade_connection_read_lock(bc, KS_TRUE);

				if (blade_connection_startup(bc, BLADE_CONNECTION_DIRECTION_INBOUND) != KS_STATUS_SUCCESS) {
					ks_log(KS_LOG_DEBUG, "Connection (%s) startup failed\n", blade_connection_id_get(bc));
					blade_connection_destroy(&bc);
					blade_transport_wss_init_destroy(&bt_wss_init);
					ks_socket_close(&sock);
					continue;
				}
				ks_log(KS_LOG_DEBUG, "Connection (%s) started\n", blade_connection_id_get(bc));

				blade_handle_connections_add(bc);
				list_append(&bm_wss->connected, bc);
				blade_connection_state_set(bc, BLADE_CONNECTION_STATE_NEW);

				blade_connection_read_unlock(bc);
			}
		}
	}
	ks_log(KS_LOG_DEBUG, "Stopped\n");

    return NULL;
}



ks_status_t blade_transport_wss_create(blade_transport_wss_t **bt_wssP, blade_module_wss_t *bm_wss, ks_socket_t sock)
{
	blade_transport_wss_t *bt_wss = NULL;

	ks_assert(bt_wssP);
	ks_assert(bm_wss);
	ks_assert(sock != KS_SOCK_INVALID);

    bt_wss = ks_pool_alloc(bm_wss->pool, sizeof(blade_transport_wss_t));
	bt_wss->module = bm_wss;
	bt_wss->pool = bm_wss->pool;
	bt_wss->sock = sock;

	*bt_wssP = bt_wss;

	ks_log(KS_LOG_DEBUG, "Created\n");

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_transport_wss_destroy(blade_transport_wss_t **bt_wssP)
{
	blade_transport_wss_t *bt_wss = NULL;

	ks_assert(bt_wssP);
	ks_assert(*bt_wssP);

	bt_wss = *bt_wssP;

	if (bt_wss->kws) kws_destroy(&bt_wss->kws);
	else ks_socket_close(&bt_wss->sock);

	ks_pool_free(bt_wss->pool, bt_wssP);

	ks_log(KS_LOG_DEBUG, "Destroyed\n");

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_transport_wss_on_connect(blade_connection_t **bcP, blade_module_t *bm, blade_identity_t *target, const char *session_id)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_module_wss_t *bm_wss = NULL;
	ks_sockaddr_t addr;
	ks_socket_t sock = KS_SOCK_INVALID;
	int family = AF_INET;
	const char *ip = NULL;
	const char *portstr = NULL;
	ks_port_t port = 1234;
	blade_transport_wss_init_t *bt_wss_init = NULL;
	blade_connection_t *bc = NULL;

	ks_assert(bcP);
	ks_assert(bm);
	ks_assert(target);

	bm_wss = (blade_module_wss_t *)blade_module_data_get(bm);

	*bcP = NULL;

	ks_log(KS_LOG_DEBUG, "Connect Callback: %s\n", blade_identity_uri(target));

	// @todo completely rework all of this once more is known about connecting when an identity has no explicit transport details but this transport
	// has been choosen anyway
	ip = blade_identity_parameter_get(target, "host");
	portstr = blade_identity_parameter_get(target, "port");
	if (!ip) {
		// @todo: temporary, this should fall back on DNS SRV or whatever else can turn "a@b.com" into an ip (and port?) to connect to
		// also need to deal with hostname lookup, so identities with wss transport need to have a host parameter that is an IP for the moment
		ks_log(KS_LOG_DEBUG, "No host provided\n");
		ret = KS_STATUS_FAIL;
		goto done;
	}

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

	blade_transport_wss_init_create(&bt_wss_init, bm_wss, sock, session_id);
	ks_assert(bt_wss_init);

	blade_connection_create(&bc, bm_wss->handle, bt_wss_init, bm_wss->transport_callbacks);
	ks_assert(bc);

	if (blade_connection_startup(bc, BLADE_CONNECTION_DIRECTION_OUTBOUND) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "Connection (%s) startup failed\n", blade_connection_id_get(bc));
		blade_connection_destroy(&bc);
		blade_transport_wss_init_destroy(&bt_wss_init);
		ks_socket_close(&sock);
		ret = KS_STATUS_FAIL;
		goto done;
	}
	ks_log(KS_LOG_DEBUG, "Connection (%s) started\n", blade_connection_id_get(bc));
	// @todo make sure it's sensible to be mixing outbound and inbound connections in the same list, but this allows entering the destruction pipeline
	// for module shutdown, disconnects and errors without special considerations
	blade_handle_connections_add(bc);
	list_append(&bm_wss->connected, bc);

	blade_connection_state_set(bc, BLADE_CONNECTION_STATE_NEW);

	*bcP = bc;

 done:
	return ret;
}

blade_connection_rank_t blade_transport_wss_on_rank(blade_connection_t *bc, blade_identity_t *target)
{
	ks_assert(bc);
	ks_assert(target);

	return BLADE_CONNECTION_RANK_POOR;
}

ks_status_t blade_transport_wss_write(blade_transport_wss_t *bt_wss, cJSON *json)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	char *json_str = cJSON_PrintUnformatted(json);
	ks_size_t json_str_len = 0;
	if (!json_str) {
		ks_log(KS_LOG_DEBUG, "Failed to generate json string\n");
		ret = KS_STATUS_FAIL;
		goto done;
	}
	// @todo determine if WSOC_TEXT null terminates when read_frame is called, or if it's safe to include like this
	json_str_len = strlen(json_str) + 1;
	if ((ks_size_t)kws_write_frame(bt_wss->kws, WSOC_TEXT, json_str, json_str_len) != json_str_len) {
		ks_log(KS_LOG_DEBUG, "Failed to write frame\n");
		ret = KS_STATUS_FAIL;
		goto done;
	}
	ks_log(KS_LOG_DEBUG, "Frame written %d bytes\n", json_str_len);

 done:
	if (json_str) free(json_str);

	return ret;
}

ks_status_t blade_transport_wss_on_send(blade_connection_t *bc, cJSON *json)
{
	blade_transport_wss_t *bt_wss = NULL;

	ks_assert(bc);
	ks_assert(json);

	bt_wss = (blade_transport_wss_t *)blade_connection_transport_get(bc);

	return blade_transport_wss_write(bt_wss, json);
}

ks_status_t blade_transport_wss_read(blade_transport_wss_t *bt_wss, cJSON **json)
{
	// @todo get exact timeout from service config?
	int32_t poll_flags = ks_wait_sock(bt_wss->sock, 100, KS_POLL_READ); // | KS_POLL_ERROR);

	*json = NULL;

	if (poll_flags & KS_POLL_ERROR) {
		ks_log(KS_LOG_DEBUG, "POLLERR\n");
		return KS_STATUS_FAIL;
	}
	if (poll_flags & KS_POLL_READ) {
		kws_opcode_t opcode;
		uint8_t *frame_data = NULL;
		ks_ssize_t frame_data_len = kws_read_frame(bt_wss->kws, &opcode, &frame_data);

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

ks_status_t blade_transport_wss_on_receive(blade_connection_t *bc, cJSON **json)
{
	blade_transport_wss_t *bt_wss = NULL;

	ks_assert(bc);
	ks_assert(json);

	bt_wss = (blade_transport_wss_t *)blade_connection_transport_get(bc);

	return blade_transport_wss_read(bt_wss, json);
}

ks_status_t blade_transport_wss_rpc_error_send(blade_connection_t *bc, const char *id, int32_t code, const char *message)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_transport_wss_t *bt_wss = NULL;
	cJSON *json = NULL;

	ks_assert(bc);
	//ks_assert(id);
	ks_assert(message);

	bt_wss = (blade_transport_wss_t *)blade_connection_transport_get(bc);

	blade_rpc_error_create(blade_connection_pool_get(bc), &json, NULL, id, code, message);

    if (blade_transport_wss_write(bt_wss, json) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "Failed to write error message\n");
		ret = KS_STATUS_FAIL;
	}

	cJSON_Delete(json);
	return ret;
}

blade_connection_state_hook_t blade_transport_wss_on_state_disconnect(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	blade_transport_wss_t *bt_wss = NULL;
	blade_transport_wss_init_t *bt_wss_init = NULL;

	ks_assert(bc);

	ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);

	if (condition == BLADE_CONNECTION_STATE_CONDITION_PRE) return BLADE_CONNECTION_STATE_HOOK_SUCCESS;

	bt_wss = (blade_transport_wss_t *)blade_connection_transport_get(bc);
	bt_wss_init = (blade_transport_wss_init_t *)blade_connection_transport_init_get(bc);

	list_delete(&bt_wss->module->connected, bc);

	if (bt_wss_init) blade_transport_wss_init_destroy(&bt_wss_init);
	if (bt_wss) blade_transport_wss_destroy(&bt_wss); // @TODO: Scream at this very loudly until I feel better for it wasting 2 days to track down, and then fix the issue it's causing

	return BLADE_CONNECTION_STATE_HOOK_SUCCESS;
}

blade_connection_state_hook_t blade_transport_wss_on_state_new_inbound(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	blade_transport_wss_t *bt_wss = NULL;
	blade_transport_wss_init_t *bt_wss_init = NULL;

	ks_assert(bc);

	ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);

	if (condition == BLADE_CONNECTION_STATE_CONDITION_PRE) return BLADE_CONNECTION_STATE_HOOK_SUCCESS;

	bt_wss_init = (blade_transport_wss_init_t *)blade_connection_transport_init_get(bc);

	blade_transport_wss_create(&bt_wss, bt_wss_init->module, bt_wss_init->sock);
	ks_assert(bt_wss);

	blade_connection_transport_set(bc, bt_wss);

	return BLADE_CONNECTION_STATE_HOOK_SUCCESS;
}

blade_connection_state_hook_t blade_transport_wss_on_state_new_outbound(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	blade_transport_wss_t *bt_wss = NULL;
	blade_transport_wss_init_t *bt_wss_init = NULL;

	ks_assert(bc);

	ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);

	if (condition == BLADE_CONNECTION_STATE_CONDITION_PRE) return BLADE_CONNECTION_STATE_HOOK_SUCCESS;

	bt_wss_init = (blade_transport_wss_init_t *)blade_connection_transport_init_get(bc);

	blade_transport_wss_create(&bt_wss, bt_wss_init->module, bt_wss_init->sock);
	ks_assert(bt_wss);

	blade_connection_transport_set(bc, bt_wss);

	return BLADE_CONNECTION_STATE_HOOK_SUCCESS;
}

blade_connection_state_hook_t blade_transport_wss_on_state_connect_inbound(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	blade_transport_wss_t *bt_wss = NULL;

	ks_assert(bc);

	ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);

	if (condition == BLADE_CONNECTION_STATE_CONDITION_PRE) return BLADE_CONNECTION_STATE_HOOK_SUCCESS;

	bt_wss = (blade_transport_wss_t *)blade_connection_transport_get(bc);

	// @todo: SSL init stuffs based on data from config to pass into kws_init
	if (kws_init(&bt_wss->kws, bt_wss->sock, NULL, NULL, KWS_BLOCK, bt_wss->pool) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "Failed websocket init\n");
		return BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
	}

	return BLADE_CONNECTION_STATE_HOOK_SUCCESS;
}

blade_connection_state_hook_t blade_transport_wss_on_state_connect_outbound(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	blade_transport_wss_t *bt_wss = NULL;

	ks_assert(bc);

	ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);

	if (condition == BLADE_CONNECTION_STATE_CONDITION_PRE) return BLADE_CONNECTION_STATE_HOOK_SUCCESS;

	bt_wss = (blade_transport_wss_t *)blade_connection_transport_get(bc);

	// @todo: SSL init stuffs based on data from config to pass into kws_init
	if (kws_init(&bt_wss->kws, bt_wss->sock, NULL, "/blade:blade.invalid:blade", KWS_BLOCK, bt_wss->pool) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "Failed websocket init\n");
		return BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
	}

	return BLADE_CONNECTION_STATE_HOOK_SUCCESS;
}

blade_connection_state_hook_t blade_transport_wss_on_state_attach_inbound(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	blade_connection_state_hook_t ret = BLADE_CONNECTION_STATE_HOOK_SUCCESS;
	blade_transport_wss_t *bt_wss = NULL;
	ks_pool_t *pool = NULL;
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
	const char *sid = NULL;
	ks_time_t timeout;

	ks_assert(bc);

	bh = blade_connection_handle_get(bc);
	ks_assert(bh);

	ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);

	if (condition == BLADE_CONNECTION_STATE_CONDITION_PRE) return BLADE_CONNECTION_STATE_HOOK_SUCCESS;

	bt_wss = (blade_transport_wss_t *)blade_connection_transport_get(bc);

	pool = blade_connection_pool_get(bc);

	// @todo very temporary, really need monotonic clock and get timeout delay and sleep delay from config
	timeout = ks_time_now() + (5 * KS_USEC_PER_SEC);
	while (blade_transport_wss_read(bt_wss, &json_req) == KS_STATUS_SUCCESS) {
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

	// @todo validation wrapper for request and response/error to confirm jsonrpc and provide enum for output as to which it is
	jsonrpc = cJSON_GetObjectCstr(json_req, "jsonrpc"); // @todo check for definitions of these keys and fixed values
	if (!jsonrpc || strcmp(jsonrpc, "2.0")) {
		ks_log(KS_LOG_DEBUG, "Received message is not the expected protocol\n");
		blade_transport_wss_rpc_error_send(bc, NULL, -32600, "Invalid request, missing 'jsonrpc' field");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	id = cJSON_GetObjectCstr(json_req, "id"); // @todo switch to number if we are not using a uuid for message id
	if (!id) {
		ks_log(KS_LOG_DEBUG, "Received message is missing 'id'\n");
		blade_transport_wss_rpc_error_send(bc, NULL, -32600, "Invalid request, missing 'id' field");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	method = cJSON_GetObjectCstr(json_req, "method");
	if (!method || strcasecmp(method, "blade.session.attach")) {
		ks_log(KS_LOG_DEBUG, "Received message is missing 'method' or is an unexpected method\n");
		blade_transport_wss_rpc_error_send(bc, id, -32601, "Missing or unexpected 'method' field");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	json_params = cJSON_GetObjectItem(json_req, "params");
	if (json_params) {
		sid = cJSON_GetObjectCstr(json_params, "session-id");
		if (sid) {
			// @todo validate uuid format by parsing, not currently available in uuid functions, send -32602 (invalid params) if invalid
			ks_log(KS_LOG_DEBUG, "Session (%s) requested\n", sid);
		}
	}

	if (sid) {
		bs = blade_handle_sessions_get(bh, sid); // bs comes out read locked if not null to prevent it being cleaned up before we are done
		if (bs) {
			if (blade_session_terminating(bs)) {
				blade_session_read_unlock(bs);
				ks_log(KS_LOG_DEBUG, "Session (%s) terminating\n", blade_session_id_get(bs));
				bs = NULL;
			} else {
				ks_log(KS_LOG_DEBUG, "Session (%s) located\n", blade_session_id_get(bs));
			}
		}
	}

	if (!bs) {
		blade_session_create(&bs, bh, NULL);
		ks_assert(bs);

		ks_log(KS_LOG_DEBUG, "Session (%s) created\n", blade_session_id_get(bs));

		blade_session_read_lock(bs, KS_TRUE); // this will be done by blade_handle_sessions_get() otherwise

		if (blade_session_startup(bs) != KS_STATUS_SUCCESS) {
			ks_log(KS_LOG_DEBUG, "Session (%s) startup failed\n", blade_session_id_get(bs));
			blade_transport_wss_rpc_error_send(bc, id, -32603, "Internal error, session could not be started");
			blade_session_read_unlock(bs);
			blade_session_destroy(&bs);
			ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
			goto done;
		}
		ks_log(KS_LOG_DEBUG, "Session (%s) started\n", blade_session_id_get(bs));
		blade_handle_sessions_add(bs);
	}

	// @todo wrapper to generate request and response
	blade_rpc_response_create(pool, &json_res, &json_result, id);
	ks_assert(json_res);

	cJSON_AddStringToObject(json_result, "session-id", blade_session_id_get(bs));

	// @todo send response
	if (blade_transport_wss_write(bt_wss, json_res) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "Failed to write response message\n");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	blade_connection_session_set(bc, blade_session_id_get(bs));

 done:
	// @note the state machine expects if we return SUCCESS, that the session assigned to the connection will be read locked to ensure that the state
	// machine can finish attaching the session, if you BYPASS then you can handle everything here in the callback, but this should be fairly standard
	// behaviour to simply go as far as assigning a session to the connection and let the system handle the rest
	if (json_req) cJSON_Delete(json_req);
	if (json_res) cJSON_Delete(json_res);

	
	return ret;
}

blade_connection_state_hook_t blade_transport_wss_on_state_attach_outbound(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	blade_connection_state_hook_t ret = BLADE_CONNECTION_STATE_HOOK_SUCCESS;
	blade_handle_t *bh = NULL;
	blade_transport_wss_t *bt_wss = NULL;
	blade_transport_wss_init_t *bt_wss_init = NULL;
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
	const char *sid = NULL;
	blade_session_t *bs = NULL;

	ks_assert(bc);

	ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);

	if (condition == BLADE_CONNECTION_STATE_CONDITION_PRE) return BLADE_CONNECTION_STATE_HOOK_SUCCESS;

	bh = blade_connection_handle_get(bc);
	bt_wss = (blade_transport_wss_t *)blade_connection_transport_get(bc);
	bt_wss_init = (blade_transport_wss_init_t *)blade_connection_transport_init_get(bc);
	pool = blade_connection_pool_get(bc);


	blade_rpc_request_create(pool, &json_req, &json_params, &mid, "blade.session.attach");
	ks_assert(json_req);

	if (bt_wss_init->session_id) cJSON_AddStringToObject(json_params, "session-id", bt_wss_init->session_id);

	ks_log(KS_LOG_DEBUG, "Session (%s) requested\n", (bt_wss_init->session_id ? bt_wss_init->session_id : "none"));

	if (blade_transport_wss_write(bt_wss, json_req) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "Failed to write request message\n");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}


	timeout = ks_time_now() + (5 * KS_USEC_PER_SEC);
	while (blade_transport_wss_read(bt_wss, &json_res) == KS_STATUS_SUCCESS) {
		if (json_res) break;
		ks_sleep_ms(250);
		if (ks_time_now() >= timeout) break;
	}

	if (!json_res) {
		ks_log(KS_LOG_DEBUG, "Failed to receive message before timeout\n");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	// @todo validation wrapper for request and response/error to confirm jsonrpc and provide enum for output as to which it is
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

	sid = cJSON_GetObjectCstr(json_result, "session-id");
	if (!sid) {
		ks_log(KS_LOG_DEBUG, "Received message 'result' is missing 'session-id'\n");
		ret = BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
		goto done;
	}

	if (sid) {
		// @todo validate uuid format by parsing, not currently available in uuid functions
		bs = blade_handle_sessions_get(bh, sid); // bs comes out read locked if not null to prevent it being cleaned up before we are done
		if (bs) {
			ks_log(KS_LOG_DEBUG, "Session (%s) located\n", blade_session_id_get(bs));
		}
	}

	if (!bs) {
		blade_session_create(&bs, bh, sid);
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
		ks_log(KS_LOG_DEBUG, "Session (%s) started\n", blade_session_id_get(bs));
		blade_handle_sessions_add(bs);
	}

	blade_connection_session_set(bc, blade_session_id_get(bs));

 done:
	if (json_req) cJSON_Delete(json_req);
	if (json_res) cJSON_Delete(json_res);

	return ret;
}

blade_connection_state_hook_t blade_transport_wss_on_state_detach_inbound(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	ks_assert(bc);

	ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);

	return BLADE_CONNECTION_STATE_HOOK_SUCCESS;
}

blade_connection_state_hook_t blade_transport_wss_on_state_detach_outbound(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	ks_assert(bc);

	ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);

	return BLADE_CONNECTION_STATE_HOOK_SUCCESS;
}

blade_connection_state_hook_t blade_transport_wss_on_state_ready_inbound(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	ks_assert(bc);

	if (condition == BLADE_CONNECTION_STATE_CONDITION_PRE) {
		ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);
	}

	return BLADE_CONNECTION_STATE_HOOK_SUCCESS;
}

blade_connection_state_hook_t blade_transport_wss_on_state_ready_outbound(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	ks_assert(bc);

	if (condition == BLADE_CONNECTION_STATE_CONDITION_PRE) {
		blade_session_t *bs = NULL;
		//cJSON *req = NULL;

		ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);

		bs = blade_handle_sessions_get(blade_connection_handle_get(bc), blade_connection_session_get(bc));
		ks_assert(bs);

		//blade_rpc_request_create(blade_connection_pool_get(bc), &req, NULL, NULL, "blade.test.echo");
		//blade_session_send(bs, req, blade_test_echo_response_handler);

		blade_session_read_unlock(bs);
	}

	return BLADE_CONNECTION_STATE_HOOK_SUCCESS;
}



ks_bool_t blade_test_echo_request_handler(blade_module_t *bm, blade_request_t *breq)
{
	blade_session_t *bs = NULL;
	cJSON *res = NULL;

	ks_assert(bm);
	ks_assert(breq);

	ks_log(KS_LOG_DEBUG, "Request Received!\n");

	bs = blade_handle_sessions_get(breq->handle, breq->session_id);
	ks_assert(bs);

	blade_rpc_response_create(breq->pool, &res, NULL, breq->message_id);
	blade_session_send(bs, res, NULL);

	blade_session_read_unlock(bs);

	return KS_FALSE;
}

ks_bool_t blade_test_echo_response_handler(blade_response_t *bres)
{
	ks_assert(bres);

	ks_log(KS_LOG_DEBUG, "Response Received!\n");

	return KS_FALSE;
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
