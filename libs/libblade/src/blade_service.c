/*
 * Copyright (c) 2007-2014, Anthony Minessale II
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

#define BLADE_SERVICE_WEBSOCKETS_ENDPOINTS_MULTIHOME_MAX 16

struct blade_service_s {
	ks_pool_t *pool;
	ks_thread_pool_t *tpool;
	blade_handle_t *handle;
	blade_service_peer_state_callback_t peer_state_callback;

	ks_sockaddr_t config_websockets_endpoints_ipv4[BLADE_SERVICE_WEBSOCKETS_ENDPOINTS_MULTIHOME_MAX];
	ks_sockaddr_t config_websockets_endpoints_ipv6[BLADE_SERVICE_WEBSOCKETS_ENDPOINTS_MULTIHOME_MAX];
	int32_t config_websockets_endpoints_ipv4_length;
	int32_t config_websockets_endpoints_ipv6_length;
	int32_t config_websockets_endpoints_backlog;

	ks_bool_t shutdown;
	
	struct pollfd *listeners_poll;
	int32_t listeners_size;
	int32_t listeners_length;
	ks_thread_t *listeners_thread;
	
	list_t connected;
};


void *blade_service_listeners_thread(ks_thread_t *thread, void *data);
ks_status_t blade_service_listen(blade_service_t *bs, ks_sockaddr_t *addr);


KS_DECLARE(ks_status_t) blade_service_destroy(blade_service_t **bsP)
{
	blade_service_t *bs = NULL;

	ks_assert(bsP);

	bs = *bsP;
	*bsP = NULL;

	ks_assert(bs);

	blade_service_shutdown(bs);
	
	list_destroy(&bs->connected);

	ks_pool_free(bs->pool, &bs);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_service_create(blade_service_t **bsP,
											 ks_pool_t *pool,
											 ks_thread_pool_t *tpool,
											 blade_handle_t *handle,
											 blade_service_peer_state_callback_t peer_state_callback)
{
	blade_service_t *bs = NULL;

	ks_assert(bsP);
	ks_assert(pool);
	ks_assert(tpool);
	ks_assert(handle);

	bs = ks_pool_alloc(pool, sizeof(*bs));
	bs->pool = pool;
	bs->tpool = tpool;
	bs->handle = handle;
	bs->peer_state_callback = peer_state_callback;
	list_init(&bs->connected);
	*bsP = bs;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_handle_t *) blade_service_handle(blade_service_t *bs)
{
	ks_assert(bs);
	return bs->handle;
}

ks_status_t blade_service_config(blade_service_t *bs, config_setting_t *config)
{
	config_setting_t *websockets = NULL;
	config_setting_t *websockets_endpoints = NULL;
	config_setting_t *websockets_endpoints_ipv4 = NULL;
	config_setting_t *websockets_endpoints_ipv6 = NULL;
	config_setting_t *websockets_ssl = NULL;
    config_setting_t *element;
	config_setting_t *tmp1;
	config_setting_t *tmp2;
	ks_sockaddr_t config_websockets_endpoints_ipv4[BLADE_SERVICE_WEBSOCKETS_ENDPOINTS_MULTIHOME_MAX];
	ks_sockaddr_t config_websockets_endpoints_ipv6[BLADE_SERVICE_WEBSOCKETS_ENDPOINTS_MULTIHOME_MAX];
	int32_t config_websockets_endpoints_ipv4_length = 0;
	int32_t config_websockets_endpoints_ipv6_length = 0;
	int32_t config_websockets_endpoints_backlog = 8;

	ks_assert(bs);

	if (!config) return KS_STATUS_FAIL;
	if (!config_setting_is_group(config)) return KS_STATUS_FAIL;

	websockets = config_setting_get_member(config, "websockets");
	if (!websockets) return KS_STATUS_FAIL;
	websockets_endpoints = config_setting_get_member(config, "endpoints");
	if (!websockets_endpoints) return KS_STATUS_FAIL;
	websockets_endpoints_ipv4 = config_lookup_from(websockets_endpoints, "ipv4");
	websockets_endpoints_ipv6 = config_lookup_from(websockets_endpoints, "ipv6");
	if (websockets_endpoints_ipv4) {
		if (config_setting_type(websockets_endpoints_ipv4) != CONFIG_TYPE_LIST) return KS_STATUS_FAIL;
		if ((config_websockets_endpoints_ipv4_length = config_setting_length(websockets_endpoints_ipv4)) > BLADE_SERVICE_WEBSOCKETS_ENDPOINTS_MULTIHOME_MAX)
			return KS_STATUS_FAIL;
		
		for (int32_t index = 0; index < config_websockets_endpoints_ipv4_length; ++index) {
			element = config_setting_get_elem(websockets_endpoints_ipv4, index);
            tmp1 = config_lookup_from(element, "address");
            tmp2 = config_lookup_from(element, "port");
			if (!tmp1 || !tmp2) return KS_STATUS_FAIL;
			if (config_setting_type(tmp1) != CONFIG_TYPE_STRING) return KS_STATUS_FAIL;
			if (config_setting_type(tmp2) != CONFIG_TYPE_INT) return KS_STATUS_FAIL;
			
			if (ks_addr_set(&config_websockets_endpoints_ipv4[index],
							config_setting_get_string(tmp1),
							config_setting_get_int(tmp2),
							AF_INET) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
		}
	}
	if (websockets_endpoints_ipv6) {
		if (config_setting_type(websockets_endpoints_ipv6) != CONFIG_TYPE_LIST) return KS_STATUS_FAIL;
		if ((config_websockets_endpoints_ipv6_length = config_setting_length(websockets_endpoints_ipv6)) > BLADE_SERVICE_WEBSOCKETS_ENDPOINTS_MULTIHOME_MAX)
			return KS_STATUS_FAIL;
		
		for (int32_t index = 0; index < config_websockets_endpoints_ipv6_length; ++index) {
			element = config_setting_get_elem(websockets_endpoints_ipv6, index);
            tmp1 = config_lookup_from(element, "address");
            tmp2 = config_lookup_from(element, "port");
			if (!tmp1 || !tmp2) return KS_STATUS_FAIL;
			if (config_setting_type(tmp1) != CONFIG_TYPE_STRING) return KS_STATUS_FAIL;
			if (config_setting_type(tmp2) != CONFIG_TYPE_INT) return KS_STATUS_FAIL;
			
			if (ks_addr_set(&config_websockets_endpoints_ipv6[index],
							config_setting_get_string(tmp1),
							config_setting_get_int(tmp2),
							AF_INET6) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
		}
	}
	if (config_websockets_endpoints_ipv4_length + config_websockets_endpoints_ipv6_length <= 0) return KS_STATUS_FAIL;
	tmp1 = config_lookup_from(websockets_endpoints, "backlog");
	if (tmp1) {
		if (config_setting_type(tmp1) != CONFIG_TYPE_INT) return KS_STATUS_FAIL;
		config_websockets_endpoints_backlog = config_setting_get_int(tmp1);
	}
	websockets_ssl = config_setting_get_member(websockets, "ssl");
	if (websockets_ssl) {
		// @todo: SSL stuffs from websockets_ssl into config_websockets_ssl envelope
	}


	// Configuration is valid, now assign it to the variables that are used
	// If the configuration was invalid, then this does not get changed from the current config when reloading a new config
	for (int32_t index = 0; index < config_websockets_endpoints_ipv4_length; ++index)
		bs->config_websockets_endpoints_ipv4[index] = config_websockets_endpoints_ipv4[index];
	for (int32_t index = 0; index < config_websockets_endpoints_ipv6_length; ++index)
		bs->config_websockets_endpoints_ipv6[index] = config_websockets_endpoints_ipv6[index];
	bs->config_websockets_endpoints_ipv4_length = config_websockets_endpoints_ipv4_length;
	bs->config_websockets_endpoints_ipv6_length = config_websockets_endpoints_ipv6_length;
	bs->config_websockets_endpoints_backlog = config_websockets_endpoints_backlog;
	//bs->config_websockets_ssl = config_websockets_ssl;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_service_startup(blade_service_t *bs, config_setting_t *config)
{
	ks_assert(bs);

	blade_service_shutdown(bs);

	// @todo: If the configuration is invalid, and this is a case of reloading a new config, then the service shutdown shouldn't occur
	// but the service may use configuration that changes before we shutdown if it is read successfully, may require a config reader/writer mutex?
	
    if (blade_service_config(bs, config) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;

	for (int32_t index = 0; index < bs->config_websockets_endpoints_ipv4_length; ++index) {
		if (blade_service_listen(bs, &bs->config_websockets_endpoints_ipv4[index]) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
	}
	for (int32_t index = 0; index < bs->config_websockets_endpoints_ipv6_length; ++index) {
		if (blade_service_listen(bs, &bs->config_websockets_endpoints_ipv6[index]) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
	}

	if (ks_thread_create_ex(&bs->listeners_thread,
							blade_service_listeners_thread,
							bs,
							KS_THREAD_FLAG_DEFAULT,
							KS_THREAD_DEFAULT_STACK,
							KS_PRI_NORMAL,
							bs->pool) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
	
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_service_shutdown(blade_service_t *bs)
{
	ks_assert(bs);

	// @todo 1 more callback for blade_service_state_callback_t? providing event up the stack on service startup, shutdown, and service errors?
	
	bs->shutdown = KS_TRUE;
	
	if (bs->listeners_thread) {
		ks_thread_join(bs->listeners_thread);
		ks_pool_free(bs->pool, &bs->listeners_thread);
	}

	for (int32_t index = 0; index < bs->listeners_length; ++index) {
		ks_socket_t sock = bs->listeners_poll[index].fd;
		ks_socket_shutdown(sock, SHUT_RDWR);
		ks_socket_close(&sock);
	}
	bs->listeners_length = 0;

	list_iterator_start(&bs->connected);
	while (list_iterator_hasnext(&bs->connected)) {
		blade_peer_t *peer = (blade_peer_t *)list_iterator_next(&bs->connected);
		blade_peer_destroy(&peer); // @todo determine if NOT receiving the DISCONNECTING event callback for these will matter, as service is being shutdown
	}
	list_iterator_stop(&bs->connected);
	list_clear(&bs->connected);

	bs->shutdown = KS_FALSE;
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(void) blade_service_peer_state_callback(blade_service_t *bs, blade_peer_t *bp, blade_peerstate_t state)
{
	ks_assert(bs);
	ks_assert(bp);

	if (bs->peer_state_callback) bs->peer_state_callback(bs, bp, state);
}

ks_status_t blade_service_listen(blade_service_t *bs, ks_sockaddr_t *addr)
{
	ks_socket_t listener = KS_SOCK_INVALID;
	int32_t listener_index = -1;
	ks_status_t ret = KS_STATUS_SUCCESS;
	
	ks_assert(bs);
	ks_assert(addr);

	if ((listener = socket(addr->family, SOCK_STREAM, IPPROTO_TCP)) == KS_SOCK_INVALID) {
		ret = KS_STATUS_FAIL;
		goto done;
	}

	ks_socket_option(listener, SO_REUSEADDR, KS_TRUE);
	ks_socket_option(listener, TCP_NODELAY, KS_TRUE);
	// @todo make sure v6 does not automatically map to a v4 using socket option IPV6_V6ONLY?

	if (ks_addr_bind(listener, addr) != KS_STATUS_SUCCESS) {
		ret = KS_STATUS_FAIL;
		goto done;
	}

	if (listen(listener, bs->config_websockets_endpoints_backlog) != 0) {
		ret = KS_STATUS_FAIL;
		goto done;
	}

	listener_index = bs->listeners_length++;
	if (bs->listeners_length > bs->listeners_size) {
		bs->listeners_size = bs->listeners_length;
		bs->listeners_poll = (struct pollfd *)ks_pool_resize(bs->pool, bs->listeners_poll, sizeof(struct pollfd) * bs->listeners_size);
		ks_assert(bs->listeners_poll);
	}
	bs->listeners_poll[listener_index].fd = listener;
	bs->listeners_poll[listener_index].events = POLLIN | POLLERR;

 done:
	if (ret != KS_STATUS_SUCCESS) {
		if (listener != KS_SOCK_INVALID) {
			ks_socket_shutdown(listener, SHUT_RDWR);
			ks_socket_close(&listener);
		}
	}
	return ret;
}

void *blade_service_listeners_thread(ks_thread_t *thread, void *data)
{
	blade_service_t *service = NULL;

	ks_assert(thread);
	ks_assert(data);

	service = (blade_service_t *)data;
	
	// @todo 1 more callback for blade_service_state_callback_t? providing event up the stack on service startup, shutdown, and service errors?
	
	while (!service->shutdown) {
		// @todo take exact timeout from a setting in config_service_endpoints
		if (ks_poll(service->listeners_poll, service->listeners_length, 100) > 0) {
			for (int32_t index = 0; index < service->listeners_length; ++index) {
				ks_socket_t sock = KS_SOCK_INVALID;
				blade_peer_t *peer = NULL;

				if (!(service->listeners_poll[index].revents & POLLIN)) continue;
				if (service->listeners_poll[index].revents & POLLERR) {
					// @todo: error handling, just skip the listener for now, it might recover, could skip X sanity times before closing?
					continue;
				}

				if ((sock = accept(service->listeners_poll[index].fd, NULL, NULL)) == KS_SOCK_INVALID) {
					// @todo: error handling, just skip the socket for now as most causes are because the remote side suddenly became unreachable
					continue;
				}

				// @todo consider a recycle queue of peers per service, and only have to call startup when one is already available
				// blade_service_peer_claim(service, &peer);
				blade_peer_create(&peer, service->pool, service->tpool, service);
				ks_assert(peer);

				// @todo call state callback with connecting enum state
				blade_service_peer_state_callback(service, peer, BLADE_PEERSTATE_CONNECTING);
				
				blade_peer_startup(peer, sock);
				
				list_append(&service->connected, peer);
			}
		}

		list_iterator_start(&service->connected);
		while (list_iterator_hasnext(&service->connected)) {
			blade_peer_t *peer = (blade_peer_t *)list_iterator_next(&service->connected);
			// @todo expose accessor for disconnecting, after changing it into the state callback enum
			// ensure that every way kws_close might occur leads back to disconnecting = KS_TRUE for this to universally process disconnects
			if (blade_peer_state(peer) == BLADE_PEERSTATE_DISCONNECTING) {
				// @todo check if there is an iterator based remove function, or indexed iteration to use list_delete_at()
				list_delete(&service->connected, peer);
				blade_service_peer_state_callback(service, peer, BLADE_PEERSTATE_DISCONNECTING);

				// @todo switch to blade_peer_shutdown(&peer) and blade_peer_discard(&peer) after introducing recycling of peers
				blade_peer_destroy(&peer);
			}
		}
		list_iterator_stop(&service->connected);
	}
	
	return NULL;
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
