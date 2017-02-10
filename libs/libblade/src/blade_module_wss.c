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

	list_t connected;
	ks_q_t *disconnected;
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
};



ks_status_t blade_module_wss_create(blade_module_wss_t **bm_wssP, blade_handle_t *bh);
ks_status_t blade_module_wss_destroy(blade_module_wss_t **bm_wssP);

ks_status_t blade_module_wss_on_load(blade_module_t **bmP, blade_handle_t *bh);
ks_status_t blade_module_wss_on_unload(blade_module_t *bm);
ks_status_t blade_module_wss_on_startup(blade_module_t *bm, config_setting_t *config);
ks_status_t blade_module_wss_on_shutdown(blade_module_t *bm);

ks_status_t blade_module_wss_listen(blade_module_wss_t *bm, ks_sockaddr_t *addr);
void *blade_module_wss_listeners_thread(ks_thread_t *thread, void *data);



ks_status_t blade_transport_wss_create(blade_transport_wss_t **bt_wssP, blade_module_wss_t *bm_wss, ks_socket_t sock);
ks_status_t blade_transport_wss_destroy(blade_transport_wss_t **bt_wssP);

ks_status_t blade_transport_wss_on_connect(blade_connection_t **bcP, blade_module_t *bm, blade_identity_t *target);
blade_connection_rank_t blade_transport_wss_on_rank(blade_connection_t *bc, blade_identity_t *target);

ks_status_t blade_transport_wss_on_send(blade_connection_t *bc, blade_identity_t *target, cJSON *json);
ks_status_t blade_transport_wss_on_receive(blade_connection_t *bc, cJSON **json);

blade_connection_state_hook_t blade_transport_wss_on_state_disconnect(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_on_state_new_inbound(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_on_state_new_outbound(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_on_state_connect_inbound(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_on_state_connect_outbound(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_on_state_attach_inbound(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_on_state_attach_outbound(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_on_state_detach(blade_connection_t *bc, blade_connection_state_condition_t condition);
blade_connection_state_hook_t blade_transport_wss_on_state_ready(blade_connection_t *bc, blade_connection_state_condition_t condition);



ks_status_t blade_transport_wss_init_create(blade_transport_wss_init_t **bt_wssiP, blade_module_wss_t *bm_wss, ks_socket_t sock);
ks_status_t blade_transport_wss_init_destroy(blade_transport_wss_init_t **bt_wssiP);



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
	blade_transport_wss_on_state_detach,
	blade_transport_wss_on_state_detach,
	blade_transport_wss_on_state_ready,
	blade_transport_wss_on_state_ready,
};



ks_status_t blade_module_wss_create(blade_module_wss_t **bm_wssP, blade_handle_t *bh)
{
	blade_module_wss_t *bm_wss = NULL;
	
	ks_assert(bm_wssP);
	ks_assert(bh);

    bm_wss = ks_pool_alloc(bm_wss->pool, sizeof(blade_module_wss_t));
	bm_wss->handle = bh;
	bm_wss->pool = blade_handle_pool_get(bh);
	bm_wss->tpool = blade_handle_tpool_get(bh);

	blade_module_create(&bm_wss->module, bh, bm_wss, &g_module_wss_callbacks);
	bm_wss->module_callbacks = &g_module_wss_callbacks;
	bm_wss->transport_callbacks = &g_transport_wss_callbacks;
 
	list_init(&bm_wss->connected);
	ks_q_create(&bm_wss->disconnected, bm_wss->pool, 0);
	ks_assert(bm_wss->disconnected);

	*bm_wssP = bm_wss;

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
	ks_q_destroy(&bm_wss->disconnected);

	ks_pool_free(bm_wss->pool, bm_wssP);

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_module_wss_on_load(blade_module_t **bmP, blade_handle_t *bh)
{
	blade_module_wss_t *bm_wss = NULL;

	ks_assert(bmP);
	ks_assert(bh);

	blade_module_wss_create(&bm_wss, bh);
	ks_assert(bm_wss);

	*bmP = bm_wss->module;
	
	return KS_STATUS_SUCCESS;
}

ks_status_t blade_module_wss_on_unload(blade_module_t *bm)
{
	blade_module_wss_t *bm_wss = NULL;

	ks_assert(bm);

	bm_wss = blade_module_data_get(bm);
	
	blade_module_wss_destroy(&bm_wss);
	
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
	if (!wss) {
		ks_log(KS_LOG_DEBUG, "!wss\n");
		return KS_STATUS_FAIL;
	}
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

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_module_wss_on_startup(blade_module_t *bm, config_setting_t *config)
{
	blade_module_wss_t *bm_wss = NULL;
	
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
	
	return KS_STATUS_SUCCESS;
}

ks_status_t blade_module_wss_on_shutdown(blade_module_t *bm)
{
	blade_module_wss_t *bm_wss = NULL;
	blade_transport_wss_t *bt_wss = NULL;
	blade_connection_t *bc = NULL;
	
	ks_assert(bm);

	bm_wss = (blade_module_wss_t *)blade_module_data_get(bm);

	if (bm_wss->listeners_thread) {
		bm_wss->shutdown = KS_TRUE;
		ks_thread_join(bm_wss->listeners_thread);
		ks_pool_free(bm_wss->pool, &bm_wss->listeners_thread);
		bm_wss->shutdown = KS_FALSE;
	}

	for (int32_t index = 0; index < bm_wss->listeners_count; ++index) {
		ks_socket_t sock = bm_wss->listeners_poll[index].fd;
		ks_socket_shutdown(sock, SHUT_RDWR);
		ks_socket_close(&sock);
	}
	bm_wss->listeners_count = 0;
	if (bm_wss->listeners_poll) ks_pool_free(bm_wss->pool, &bm_wss->listeners_poll);

	while (ks_q_trypop(bm_wss->disconnected, (void **)&bc) == KS_STATUS_SUCCESS) ;
	list_iterator_start(&bm_wss->connected);
	while (list_iterator_hasnext(&bm_wss->connected)) {
		bc = (blade_connection_t *)list_iterator_next(&bm_wss->connected);
		bt_wss = (blade_transport_wss_t *)blade_connection_transport_get(bc);

		blade_connection_destroy(&bc);
		blade_transport_wss_destroy(&bt_wss);
	}
	list_iterator_stop(&bm_wss->connected);
	list_clear(&bm_wss->connected);

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
	bm_wss->listeners_poll[listener_index].events = POLLIN | POLLERR;

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
	blade_transport_wss_t *bt_wss = NULL;
	blade_connection_t *bc = NULL;

	ks_assert(thread);
	ks_assert(data);

	bm_wss = (blade_module_wss_t *)data;

	while (!bm_wss->shutdown) {
		// @todo take exact timeout from a setting in config_wss_endpoints
		if (ks_poll(bm_wss->listeners_poll, bm_wss->listeners_count, 100) > 0) {
			for (int32_t index = 0; index < bm_wss->listeners_count; ++index) {
				ks_socket_t sock = KS_SOCK_INVALID;

				if (!(bm_wss->listeners_poll[index].revents & POLLIN)) continue;
				if (bm_wss->listeners_poll[index].revents & POLLERR) {
					// @todo: error handling, just skip the listener for now, it might recover, could skip X times before closing?
					continue;
				}

				if ((sock = accept(bm_wss->listeners_poll[index].fd, NULL, NULL)) == KS_SOCK_INVALID) {
					// @todo: error handling, just skip the socket for now as most causes are because remote side became unreachable
					continue;
				}

				blade_transport_wss_init_create(&bt_wss_init, bm_wss, sock);
				ks_assert(bt_wss_init);
				
                blade_connection_create(&bc, bm_wss->handle, bt_wss_init, bm_wss->transport_callbacks);
				ks_assert(bc);

				if (blade_connection_startup(bc, BLADE_CONNECTION_DIRECTION_INBOUND) != KS_STATUS_SUCCESS) {
					blade_connection_destroy(&bc);
					blade_transport_wss_init_destroy(&bt_wss_init);
					ks_socket_close(&sock);
					continue;
				}
				list_append(&bm_wss->connected, bc);
				blade_connection_state_set(bc, BLADE_CONNECTION_STATE_NEW);
			}
		}

		while (ks_q_trypop(bm_wss->disconnected, (void **)&bc) == KS_STATUS_SUCCESS) {
			bt_wss_init = (blade_transport_wss_init_t *)blade_connection_transport_init_get(bc);
			bt_wss = (blade_transport_wss_t *)blade_connection_transport_get(bc);

			list_delete(&bm_wss->connected, bc);

			if (bt_wss_init) blade_transport_wss_init_destroy(&bt_wss_init);
			blade_connection_destroy(&bc);
			if (bt_wss) blade_transport_wss_destroy(&bt_wss);
		}
	}

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
	
	return KS_STATUS_SUCCESS;
}

ks_status_t blade_transport_wss_on_connect(blade_connection_t **bcP, blade_module_t *bm, blade_identity_t *target)
{
	ks_assert(bcP);
	ks_assert(bm);
	ks_assert(target);

	*bcP = NULL;

	// @todo connect-out equivilent of accept

	return KS_STATUS_SUCCESS;
}

blade_connection_rank_t blade_transport_wss_on_rank(blade_connection_t *bc, blade_identity_t *target)
{
	ks_assert(bc);
	ks_assert(target);
	
	return BLADE_CONNECTION_RANK_POOR;
}

ks_status_t blade_transport_wss_write(blade_transport_wss_t *bt_wss, cJSON *json)
{
	char *json_str = cJSON_PrintUnformatted(json);
	ks_size_t json_str_len = 0;
	if (!json_str) {
		// @todo error logging
		return KS_STATUS_FAIL;
	}
	json_str_len = strlen(json_str) + 1; // @todo determine if WSOC_TEXT null terminates when read_frame is called, or if it's safe to include like this
	kws_write_frame(bt_wss->kws, WSOC_TEXT, json_str, json_str_len);

	free(json_str);

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_transport_wss_on_send(blade_connection_t *bc, blade_identity_t *target, cJSON *json)
{
	blade_transport_wss_t *bt_wss = NULL;

	ks_assert(bc);
	ks_assert(json);

	ks_log(KS_LOG_DEBUG, "Send Callback\n");

	bt_wss = (blade_transport_wss_t *)blade_connection_transport_get(bc);

	return blade_transport_wss_write(bt_wss, json);
}

ks_status_t blade_transport_wss_read(blade_transport_wss_t *bt_wss, cJSON **json)
{
	// @todo get exact timeout from service config?
	int32_t poll_flags = ks_wait_sock(bt_wss->sock, 100, KS_POLL_READ | KS_POLL_ERROR);

	*json = NULL;

	if (poll_flags & KS_POLL_ERROR) {
		// @todo error logging
		return KS_STATUS_FAIL;
	}
	if (poll_flags & KS_POLL_READ) {
		kws_opcode_t opcode;
		uint8_t *frame_data = NULL;
		ks_size_t frame_data_len = kws_read_frame(bt_wss->kws, &opcode, &frame_data);

		if (frame_data_len <= 0) {
			// @todo error logging, strerror(ks_errno())
			// 0 means socket closed with WS_NONE, which closes websocket with no additional reason
			// -1 means socket closed with a general failure
			// -2 means nonblocking wait
			// other values are based on WS_XXX reasons
			// negative values are based on reasons, except for -1 is but -2 is nonblocking wait, and
			return KS_STATUS_FAIL;
		}

		if (!(*json = cJSON_Parse((char *)frame_data))) {
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

	ks_log(KS_LOG_DEBUG, "Receive Callback\n");

	bt_wss = (blade_transport_wss_t *)blade_connection_transport_get(bc);

	return blade_transport_wss_read(bt_wss, json);
}

blade_connection_state_hook_t blade_transport_wss_on_state_disconnect(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	blade_transport_wss_t *bt_wss = NULL;

	ks_assert(bc);

	ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);

	if (condition == BLADE_CONNECTION_STATE_CONDITION_PRE) return BLADE_CONNECTION_STATE_HOOK_SUCCESS;

	bt_wss = (blade_transport_wss_t *)blade_connection_transport_get(bc);

	ks_q_push(bt_wss->module->disconnected, bc);

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
	ks_assert(bc);

	ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);

	if (condition == BLADE_CONNECTION_STATE_CONDITION_PRE) return BLADE_CONNECTION_STATE_HOOK_SUCCESS;

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
		// @todo error logging
		return BLADE_CONNECTION_STATE_HOOK_DISCONNECT;
	}

	return BLADE_CONNECTION_STATE_HOOK_SUCCESS;
}

blade_connection_state_hook_t blade_transport_wss_on_state_connect_outbound(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	ks_assert(bc);

	ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);

	return BLADE_CONNECTION_STATE_HOOK_SUCCESS;
}

blade_connection_state_hook_t blade_transport_wss_on_state_attach_inbound(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	ks_assert(bc);

	ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);
	// @todo Establish sessid and discover existing session or create and register new session through BLADE commands
	// Set session state to CONNECT if its new or RECONNECT if existing
	// start session and its thread if its new
																								
	return BLADE_CONNECTION_STATE_HOOK_BYPASS;
}

blade_connection_state_hook_t blade_transport_wss_on_state_attach_outbound(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	ks_assert(bc);

	ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);

	return BLADE_CONNECTION_STATE_HOOK_SUCCESS;
}

blade_connection_state_hook_t blade_transport_wss_on_state_detach(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	ks_assert(bc);

	ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);

	return BLADE_CONNECTION_STATE_HOOK_SUCCESS;
}

blade_connection_state_hook_t blade_transport_wss_on_state_ready(blade_connection_t *bc, blade_connection_state_condition_t condition)
{
	ks_assert(bc);

	ks_log(KS_LOG_DEBUG, "State Callback: %d\n", (int32_t)condition);

	return BLADE_CONNECTION_STATE_HOOK_SUCCESS;
}

ks_status_t blade_transport_wss_init_create(blade_transport_wss_init_t **bt_wssiP, blade_module_wss_t *bm_wss, ks_socket_t sock)
{
	blade_transport_wss_init_t *bt_wssi = NULL;

	ks_assert(bt_wssiP);
	ks_assert(bm_wss);
	ks_assert(sock != KS_SOCK_INVALID);

    bt_wssi = ks_pool_alloc(bm_wss->pool, sizeof(blade_transport_wss_init_t));
	bt_wssi->module = bm_wss;
	bt_wssi->pool = bm_wss->pool;
	bt_wssi->sock = sock;

	*bt_wssiP = bt_wssi;
	
	return KS_STATUS_SUCCESS;
}

ks_status_t blade_transport_wss_init_destroy(blade_transport_wss_init_t **bt_wssiP)
{
	blade_transport_wss_init_t *bt_wssi = NULL;
	
	ks_assert(bt_wssiP);
	ks_assert(*bt_wssiP);

	bt_wssi = *bt_wssiP;

	ks_pool_free(bt_wssi->pool, bt_wssiP);
	
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
