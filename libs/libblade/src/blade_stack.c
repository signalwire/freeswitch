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

typedef enum {
	BH_NONE = 0,
} bhpvt_flag_t;

struct blade_handle_s {
	bhpvt_flag_t flags;
	ks_pool_t *pool;
	ks_thread_pool_t *tpool;

	// local nodeid, can also be used to get the upstream session, and is provided by upstream session "blade.connect" response
	const char *local_nodeid;
	ks_rwl_t *local_nodeid_rwl;

	// master router nodeid, provided by upstream session "blade.connect" response
	const char *master_nodeid;
	ks_rwl_t *master_nodeid_rwl;

	// realms for new nodes, these originate from the master, and are provided by upstream session "blade.connect" response
	ks_hash_t *realms;

	// routes to reach any downstream node, keyed by nodeid of the target node with a value of the nodeid for the locally connected session
	// which is the next step from this node to the target node
	ks_hash_t *routes;

	ks_hash_t *transports; // registered blade_transport_t
	blade_transport_t *default_transport; // default wss transport

	ks_hash_t *corerpcs; // registered blade_rpc_t, for locally processing core blade.xxxx messages, keyed by the rpc method
	ks_hash_t *requests; // outgoing corerpc requests waiting for a response, keyed by the message id

	ks_hash_t *protocolrpcs; // registered blade_rpc_t, for locally processing protocol messages, keyed by the rpc method

	ks_hash_t *subscriptions; // registered blade_subscription_t, subscribers may include the local node
	ks_hash_t *subscriptions_cleanup; // cleanup for subscriptions, keyed by the downstream subscriber nodeid, each value is a hash_t* of which contains string keys matching the "protocol@realm/event" keys to remove each nodeid from as a subscriber during cleanup

	ks_hash_t *connections; // active connections keyed by connection id

	ks_hash_t *sessions; // active sessions keyed by session id (which comes from the nodeid of the downstream side of the session, thus an upstream session is keyed under the local_nodeid)

	ks_hash_t *session_state_callbacks;

	// @note everything below this point is exclusively for the master node

	// @todo how does "exclusive" play into the providers, does "exclusive" mean only one provider can exist for a given protocol and realm?
	ks_hash_t *protocols; // master only: protocols that have been published with blade.publish, and the details to locate a protocol provider with blade.locate
	ks_hash_t *protocols_cleanup; // master only: keyed by the nodeid, each value is a hash_t* of which contains string keys matching the "protocol@realm" keys to remove each nodeid from as a provider during cleanup

};


ks_bool_t blade_protocol_register_request_handler(blade_rpc_request_t *brpcreq, void *data);
ks_bool_t blade_protocol_publish_request_handler(blade_rpc_request_t *brpcreq, void *data);
ks_bool_t blade_protocol_locate_request_handler(blade_rpc_request_t *brpcreq, void *data);
ks_bool_t blade_protocol_execute_request_handler(blade_rpc_request_t *brpcreq, void *data);
ks_bool_t blade_protocol_subscribe_request_handler(blade_rpc_request_t *brpcreq, void *data);
ks_bool_t blade_protocol_broadcast_request_handler(blade_rpc_request_t *brpcreq, void *data);


typedef struct blade_handle_session_state_callback_registration_s blade_handle_session_state_callback_registration_t;
struct blade_handle_session_state_callback_registration_s {
	ks_pool_t *pool;

	const char *id;
	void *data;
	blade_session_state_callback_t callback;
};

static void blade_handle_session_state_callback_registration_cleanup(ks_pool_t *pool, void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_handle_session_state_callback_registration_t *bhsscr = (blade_handle_session_state_callback_registration_t *)ptr;

	ks_assert(bhsscr);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		ks_pool_free(bhsscr->pool, &bhsscr->id);
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

ks_status_t blade_handle_session_state_callback_registration_create(blade_handle_session_state_callback_registration_t **bhsscrP,
																	ks_pool_t *pool,
																	void *data,
																	blade_session_state_callback_t callback)
{
	blade_handle_session_state_callback_registration_t *bhsscr = NULL;
	uuid_t uuid;

	ks_assert(bhsscrP);
	ks_assert(pool);
	ks_assert(callback);

	ks_uuid(&uuid);

	bhsscr = ks_pool_alloc(pool, sizeof(blade_handle_session_state_callback_registration_t));
	bhsscr->pool = pool;
	bhsscr->id = ks_uuid_str(pool, &uuid);
	bhsscr->data = data;
	bhsscr->callback = callback;

	ks_pool_set_cleanup(pool, bhsscr, NULL, blade_handle_session_state_callback_registration_cleanup);

	*bhsscrP = bhsscr;

	return KS_STATUS_SUCCESS;
}

static void blade_handle_cleanup(ks_pool_t *pool, void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_handle_t *bh = (blade_handle_t *)ptr;
	ks_hash_iterator_t *it = NULL;

	ks_assert(bh);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		while ((it = ks_hash_first(bh->transports, KS_UNLOCKED)) != NULL) {
			void *key = NULL;
			blade_transport_t *value = NULL;

			ks_hash_this(it, (const void **)&key, NULL, (void **)&value);
			ks_hash_remove(bh->transports, key);

			blade_transport_destroy(&value); // must call destroy to close the transport pool, using FREE_VALUE on the hash would attempt to free the transport from the wrong pool
		}
		while ((it = ks_hash_first(bh->corerpcs, KS_UNLOCKED)) != NULL) {
			void *key = NULL;
			blade_rpc_t *value = NULL;

			ks_hash_this(it, (const void **)&key, NULL, (void **)&value);
			ks_hash_remove(bh->corerpcs, key);

			blade_rpc_destroy(&value); // must call destroy to close the rpc pool, using FREE_VALUE on the hash would attempt to free the rpc from the wrong pool
		}
		while ((it = ks_hash_first(bh->protocolrpcs, KS_UNLOCKED)) != NULL) {
			void *key = NULL;
			blade_rpc_t *value = NULL;

			ks_hash_this(it, (const void **)&key, NULL, (void **)&value);
			ks_hash_remove(bh->protocolrpcs, key);

			blade_rpc_destroy(&value); // must call destroy to close the rpc pool, using FREE_VALUE on the hash would attempt to free the rpc from the wrong pool
		}

		ks_thread_pool_destroy(&bh->tpool);
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_handle_create(blade_handle_t **bhP)
{
	bhpvt_flag_t newflags = BH_NONE;
	blade_handle_t *bh = NULL;
	ks_pool_t *pool = NULL;
	ks_thread_pool_t *tpool = NULL;

	ks_assert(bhP);

	ks_pool_open(&pool);
	ks_assert(pool);

	ks_thread_pool_create(&tpool, BLADE_HANDLE_TPOOL_MIN, BLADE_HANDLE_TPOOL_MAX, BLADE_HANDLE_TPOOL_STACK, KS_PRI_NORMAL, BLADE_HANDLE_TPOOL_IDLE);
	ks_assert(tpool);

	bh = ks_pool_alloc(pool, sizeof(blade_handle_t));
	bh->flags = newflags;
	bh->pool = pool;
	bh->tpool = tpool;

	ks_rwl_create(&bh->local_nodeid_rwl, bh->pool);
	ks_assert(bh->local_nodeid_rwl);

	ks_rwl_create(&bh->master_nodeid_rwl, bh->pool);
	ks_assert(bh->master_nodeid_rwl);

	ks_hash_create(&bh->realms, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, bh->pool);
	ks_assert(bh->realms);

	// @note can let removes free keys and values for routes, both are strings and allocated from the same pool as the hash itself
	ks_hash_create(&bh->routes, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, bh->pool);
	ks_assert(bh->routes);

	ks_hash_create(&bh->transports, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, bh->pool);
	ks_assert(bh->transports);

	ks_hash_create(&bh->corerpcs, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, bh->pool);
	ks_assert(bh->corerpcs);

	ks_hash_create(&bh->requests, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, bh->pool);
	ks_assert(bh->requests);

	ks_hash_create(&bh->protocolrpcs, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, bh->pool);
	ks_assert(bh->protocolrpcs);

	ks_hash_create(&bh->subscriptions, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, bh->pool);
	ks_assert(bh->subscriptions);

	ks_hash_create(&bh->subscriptions_cleanup, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, bh->pool);
	ks_assert(bh->subscriptions_cleanup);

	ks_hash_create(&bh->connections, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK, bh->pool);
	ks_assert(bh->connections);

	ks_hash_create(&bh->sessions, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK, bh->pool);
	ks_assert(bh->sessions);

	ks_hash_create(&bh->session_state_callbacks, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_VALUE, bh->pool);
	ks_assert(bh->session_state_callbacks);

	ks_hash_create(&bh->protocols, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, bh->pool);
	ks_assert(bh->protocols);

	ks_hash_create(&bh->protocols_cleanup, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, bh->pool);
	ks_assert(bh->protocols_cleanup);

	ks_pool_set_cleanup(pool, bh, NULL, blade_handle_cleanup);

	*bhP = bh;

	ks_log(KS_LOG_DEBUG, "Created\n");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_destroy(blade_handle_t **bhP)
{
	blade_handle_t *bh = NULL;
	ks_pool_t *pool;

	ks_assert(bhP);

	bh = *bhP;
	*bhP = NULL;

	ks_assert(bh);

	pool = bh->pool;

	// shutdown cannot happen inside of the cleanup callback because it'll lock a mutex for the pool during cleanup callbacks which connections and sessions need to finish their cleanup
	// and more importantly, memory needs to remain intact until shutdown is completed to avoid various things hitting teardown before shutdown runs
	blade_handle_shutdown(bh);

	ks_pool_close(&pool);

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_handle_config(blade_handle_t *bh, config_setting_t *config)
{
	config_setting_t *master = NULL;
	config_setting_t *master_nodeid = NULL;
	config_setting_t *master_realms = NULL;
	const char *nodeid = NULL;
	int32_t realms_length = 0;

	ks_assert(bh);

	if (!config) return KS_STATUS_FAIL;
	if (!config_setting_is_group(config)) {
		ks_log(KS_LOG_DEBUG, "!config_setting_is_group(config)\n");
		return KS_STATUS_FAIL;
	}

	master = config_setting_get_member(config, "master");
	if (master) {
		master_nodeid = config_lookup_from(master, "nodeid");
		if (master_nodeid) {
			if (config_setting_type(master_nodeid) != CONFIG_TYPE_STRING) return KS_STATUS_FAIL;
			nodeid = config_setting_get_string(master_nodeid);

			blade_handle_local_nodeid_set(bh, nodeid);
			blade_handle_master_nodeid_set(bh, nodeid);
		}
		master_realms = config_lookup_from(master, "realms");
		if (master_realms) {
			if (config_setting_type(master_realms) != CONFIG_TYPE_LIST) return KS_STATUS_FAIL;
			realms_length = config_setting_length(master_realms);
			if (realms_length > 0) {
				for (int32_t index = 0; index < realms_length; ++index) {
					const char *realm = config_setting_get_string_elem(master_realms, index);
					if (!realm) return KS_STATUS_FAIL;
					blade_handle_realm_register(bh, realm);
				}
			}
		}
	}

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_startup(blade_handle_t *bh, config_setting_t *config)
{
	blade_rpc_t *brpc = NULL;
	blade_transport_t *bt = NULL;
	ks_hash_iterator_t *it = NULL;

	ks_assert(bh);

    if (blade_handle_config(bh, config) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "blade_handle_config failed\n");
		return KS_STATUS_FAIL;
	}

	// register internal core rpcs for blade.xxx
	blade_rpc_create(&brpc, bh, "blade.register", NULL, NULL, blade_protocol_register_request_handler, NULL);
	blade_handle_corerpc_register(brpc);

	blade_rpc_create(&brpc, bh, "blade.publish", NULL, NULL, blade_protocol_publish_request_handler, NULL);
	blade_handle_corerpc_register(brpc);

	blade_rpc_create(&brpc, bh, "blade.locate", NULL, NULL, blade_protocol_locate_request_handler, NULL);
	blade_handle_corerpc_register(brpc);

	blade_rpc_create(&brpc, bh, "blade.execute", NULL, NULL, blade_protocol_execute_request_handler, NULL);
	blade_handle_corerpc_register(brpc);

	blade_rpc_create(&brpc, bh, "blade.subscribe", NULL, NULL, blade_protocol_subscribe_request_handler, NULL);
	blade_handle_corerpc_register(brpc);

	blade_rpc_create(&brpc, bh, "blade.broadcast", NULL, NULL, blade_protocol_broadcast_request_handler, NULL);
	blade_handle_corerpc_register(brpc);

	// register internal transport for secure websockets
	blade_transport_wss_create(&bt, bh);
	ks_assert(bt);
	bh->default_transport = bt;
	blade_handle_transport_register(bt);

	for (it = ks_hash_first(bh->transports, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		void *key = NULL;
		blade_transport_t *value = NULL;
		blade_transport_callbacks_t *callbacks = NULL;

		ks_hash_this(it, (const void **)&key, NULL, (void **)&value);

		callbacks = blade_transport_callbacks_get(value);
		ks_assert(callbacks);

		if (callbacks->onstartup) callbacks->onstartup(value, config);
	}

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_shutdown(blade_handle_t *bh)
{
	ks_hash_iterator_t *it = NULL;

	ks_assert(bh);

	ks_hash_read_lock(bh->transports);
	for (it = ks_hash_first(bh->transports, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		void *key = NULL;
		blade_transport_t *value = NULL;
		blade_transport_callbacks_t *callbacks = NULL;

		ks_hash_this(it, (const void **)&key, NULL, (void **)&value);

		callbacks = blade_transport_callbacks_get(value);
		ks_assert(callbacks);

		if (callbacks->onshutdown) callbacks->onshutdown(value);
	}
	ks_hash_read_unlock(bh->transports);

	ks_hash_read_lock(bh->connections);
	for (it = ks_hash_first(bh->connections, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		void *key = NULL;
		blade_connection_t *value = NULL;

		ks_hash_this(it, (const void **)&key, NULL, (void **)&value);

		blade_connection_disconnect(value);
	}
	ks_hash_read_unlock(bh->connections);
	while (ks_hash_count(bh->connections) > 0) ks_sleep_ms(100);

	ks_hash_read_lock(bh->sessions);
	for (it = ks_hash_first(bh->sessions, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		void *key = NULL;
		blade_session_t *value = NULL;

		ks_hash_this(it, (const void **)&key, NULL, (void **)&value);

		blade_session_hangup(value);
	}
	ks_hash_read_unlock(bh->sessions);
	while (ks_hash_count(bh->sessions) > 0) ks_sleep_ms(100);


	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_pool_t *) blade_handle_pool_get(blade_handle_t *bh)
{
	ks_assert(bh);
	return bh->pool;
}

KS_DECLARE(ks_thread_pool_t *) blade_handle_tpool_get(blade_handle_t *bh)
{
	ks_assert(bh);
	return bh->tpool;
}

KS_DECLARE(ks_status_t) blade_handle_local_nodeid_set(blade_handle_t *bh, const char *nodeid)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(bh);

	ks_rwl_write_lock(bh->local_nodeid_rwl);
	if (bh->local_nodeid && nodeid) {
		ret = KS_STATUS_NOT_ALLOWED;
		goto done;
	}
	if (!bh->local_nodeid && !nodeid) {
		ret = KS_STATUS_DISCONNECTED;
		goto done;
	}

	if (bh->local_nodeid) ks_pool_free(bh->pool, &bh->local_nodeid);
	if (nodeid) bh->local_nodeid = ks_pstrdup(bh->pool, nodeid);

	ks_log(KS_LOG_DEBUG, "Local NodeID: %s\n", nodeid);

done:
	ks_rwl_write_unlock(bh->local_nodeid_rwl);
	return ret;
}

KS_DECLARE(ks_bool_t) blade_handle_local_nodeid_compare(blade_handle_t *bh, const char *nodeid)
{
	ks_bool_t ret = KS_FALSE;

	ks_assert(bh);
	ks_assert(nodeid);

	ks_rwl_read_lock(bh->local_nodeid_rwl);
	ret = ks_safe_strcasecmp(bh->local_nodeid, nodeid) == 0;
	ks_rwl_read_unlock(bh->local_nodeid_rwl);

	return ret;
}

KS_DECLARE(const char *) blade_handle_master_nodeid_copy(blade_handle_t *bh, ks_pool_t *pool)
{
	const char *nodeid = NULL;

	ks_assert(bh);
	ks_assert(pool);

	ks_rwl_write_lock(bh->master_nodeid_rwl);
	if (bh->master_nodeid) nodeid = ks_pstrdup(pool, bh->master_nodeid);
	ks_rwl_write_unlock(bh->master_nodeid_rwl);

	return nodeid;
}

KS_DECLARE(ks_status_t) blade_handle_master_nodeid_set(blade_handle_t *bh, const char *nodeid)
{
	ks_assert(bh);

	ks_rwl_write_lock(bh->master_nodeid_rwl);
	if (bh->master_nodeid) ks_pool_free(bh->pool, &bh->master_nodeid);
	if (nodeid) bh->master_nodeid = ks_pstrdup(bh->pool, nodeid);
	ks_rwl_write_unlock(bh->master_nodeid_rwl);

	ks_log(KS_LOG_DEBUG, "Master NodeID: %s\n", nodeid);

return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_bool_t) blade_handle_master_nodeid_compare(blade_handle_t *bh, const char *nodeid)
{
	ks_bool_t ret = KS_FALSE;

	ks_assert(bh);
	ks_assert(nodeid);

	ks_rwl_read_lock(bh->master_nodeid_rwl);
	ret = ks_safe_strcasecmp(bh->master_nodeid, nodeid) == 0;
	ks_rwl_read_unlock(bh->master_nodeid_rwl);

	return ret;
}

KS_DECLARE(ks_status_t) blade_handle_realm_register(blade_handle_t *bh, const char *realm)
{
	char *key = NULL;

	ks_assert(bh);
	ks_assert(realm);

	key = ks_pstrdup(bh->pool, realm);
	ks_hash_insert(bh->realms, (void *)key, (void *)KS_TRUE);

	ks_log(KS_LOG_DEBUG, "Realm Registered: %s\n", key);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_realm_unregister(blade_handle_t *bh, const char *realm)
{
	ks_assert(bh);
	ks_assert(realm);

	ks_log(KS_LOG_DEBUG, "Realm Unregistered: %s\n", realm);

	ks_hash_remove(bh->realms, (void *)realm);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_hash_t *) blade_handle_realms_get(blade_handle_t *bh)
{
	ks_assert(bh);
	return bh->realms;
}

KS_DECLARE(ks_status_t) blade_handle_route_add(blade_handle_t *bh, const char *nodeid, const char *sessionid)
{
	char *key = NULL;
	char *value = NULL;

	ks_assert(bh);
	ks_assert(nodeid);
	ks_assert(sessionid);

	key = ks_pstrdup(bh->pool, nodeid);
	value = ks_pstrdup(bh->pool, sessionid);

	ks_hash_insert(bh->routes, (void *)key, (void *)value);

	ks_log(KS_LOG_DEBUG, "Route Added: %s through %s\n", key, value);

	blade_protocol_register(bh, nodeid, KS_FALSE, NULL, NULL);
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_route_remove(blade_handle_t *bh, const char *nodeid)
{
	ks_hash_t *protocols = NULL;

	ks_assert(bh);
	ks_assert(nodeid);

	ks_log(KS_LOG_DEBUG, "Route Removed: %s\n", nodeid);

	ks_hash_remove(bh->routes, (void *)nodeid);

	blade_protocol_register(bh, nodeid, KS_TRUE, NULL, NULL);

	// @note everything below here is for master-only cleanup when a node is no longer routable

	// @note protocols are cleaned up here because routes can be removed that are not locally connected with a session but still
	// have protocols published to the master node from further downstream, in which case if a route is announced upstream to be
	// removed, a master node is still able to catch that here even when there is no direct session, but is also hit when there
	// is a direct session being terminated
	ks_hash_write_lock(bh->protocols);
	protocols = (ks_hash_t *)ks_hash_search(bh->protocols_cleanup, (void *)nodeid, KS_UNLOCKED);
	if (protocols) {
		for (ks_hash_iterator_t *it = ks_hash_first(protocols, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
			void *key = NULL;
			void *value = NULL;
			blade_protocol_t *bp = NULL;
			ks_hash_t *providers = NULL;

			ks_hash_this(it, (const void **)&key, NULL, &value);

			bp = (blade_protocol_t *)ks_hash_search(bh->protocols, key, KS_UNLOCKED);
			ks_assert(bp); // should not happen when a cleanup still has a provider tracked for a protocol

			ks_log(KS_LOG_DEBUG, "Protocol (%s) provider (%s) removed\n", key, nodeid);
			blade_protocol_providers_remove(bp, nodeid);

			providers = blade_protocol_providers_get(bp);
			if (ks_hash_count(providers) == 0) {
				// @note this depends on locking something outside of the protocol that won't be destroyed, like the top level
				// protocols hash, but assumes then that any reader keeps the top level hash read locked while using the protocol
				// so it cannot be deleted
				ks_log(KS_LOG_DEBUG, "Protocol (%s) removed\n", key);
				ks_hash_remove(bh->protocols, key);
			}
		}
		ks_hash_remove(bh->protocols_cleanup, (void *)nodeid);
	}
	ks_hash_write_unlock(bh->protocols);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_session_t *) blade_handle_route_lookup(blade_handle_t *bh, const char *nodeid)
{
	blade_session_t *bs = NULL;
	const char *sessionid = NULL;

	ks_assert(bh);
	ks_assert(nodeid);

	sessionid = ks_hash_search(bh->routes, (void *)nodeid, KS_READLOCKED);
	if (sessionid) bs = blade_handle_sessions_lookup(bh, sessionid);
	ks_hash_read_unlock(bh->routes);

	return bs;
}

KS_DECLARE(ks_status_t) blade_handle_transport_register(blade_transport_t *bt)
{
	blade_handle_t *bh = NULL;
	char *key = NULL;

	ks_assert(bt);

	bh = blade_transport_handle_get(bt);
	ks_assert(bh);

	key = ks_pstrdup(bh->pool, blade_transport_name_get(bt));
	ks_assert(key);

	ks_hash_insert(bh->transports, (void *)key, bt);

	ks_log(KS_LOG_DEBUG, "Transport Registered: %s\n", key);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_transport_unregister(blade_transport_t *bt)
{
	blade_handle_t *bh = NULL;
	const char *name = NULL;

	ks_assert(bt);

	bh = blade_transport_handle_get(bt);
	ks_assert(bh);

	name = blade_transport_name_get(bt);
	ks_assert(name);

	ks_log(KS_LOG_DEBUG, "Transport Unregistered: %s\n", name);

	ks_hash_remove(bh->transports, (void *)name);

	return KS_STATUS_SUCCESS;
}


KS_DECLARE(ks_status_t) blade_handle_corerpc_register(blade_rpc_t *brpc)
{
	blade_handle_t *bh = NULL;
	char *key = NULL;

	ks_assert(brpc);

	bh = blade_rpc_handle_get(brpc);
	ks_assert(bh);

	key = ks_pstrdup(bh->pool, blade_rpc_method_get(brpc));
	ks_assert(key);

	ks_hash_insert(bh->corerpcs, (void *)key, brpc);

	ks_log(KS_LOG_DEBUG, "CoreRPC Registered: %s\n", key);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_corerpc_unregister(blade_rpc_t *brpc)
{
	blade_handle_t *bh = NULL;
	const char *method = NULL;

	ks_assert(brpc);

	bh = blade_rpc_handle_get(brpc);
	ks_assert(bh);

	method = blade_rpc_method_get(brpc);
	ks_assert(method);

	ks_log(KS_LOG_DEBUG, "CoreRPC Unregistered: %s\n", method);

	ks_hash_remove(bh->corerpcs, (void *)method);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_rpc_t *) blade_handle_corerpc_lookup(blade_handle_t *bh, const char *method)
{
	blade_rpc_t *brpc = NULL;

	ks_assert(bh);
	ks_assert(method);

	brpc = ks_hash_search(bh->corerpcs, (void *)method, KS_READLOCKED);
	ks_hash_read_unlock(bh->corerpcs);

	return brpc;
}


KS_DECLARE(ks_status_t) blade_handle_requests_add(blade_rpc_request_t *brpcreq)
{
	blade_handle_t *bh = NULL;
	const char *key = NULL;

	ks_assert(brpcreq);

	bh = blade_rpc_request_handle_get(brpcreq);
	ks_assert(bh);

	key = ks_pstrdup(bh->pool, blade_rpc_request_messageid_get(brpcreq));
	ks_assert(key);

	ks_hash_insert(bh->requests, (void *)key, brpcreq);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_requests_remove(blade_rpc_request_t *brpcreq)
{
	blade_handle_t *bh = NULL;
	const char *id = NULL;

	ks_assert(brpcreq);

	bh = blade_rpc_request_handle_get(brpcreq);
	ks_assert(bh);

	id = blade_rpc_request_messageid_get(brpcreq);
	ks_assert(id);

	ks_hash_remove(bh->requests, (void *)id);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_rpc_request_t *) blade_handle_requests_lookup(blade_handle_t *bh, const char *id)
{
	blade_rpc_request_t *brpcreq = NULL;

	ks_assert(bh);
	ks_assert(id);

	brpcreq = ks_hash_search(bh->requests, (void *)id, KS_READLOCKED);
	ks_hash_read_unlock(bh->requests);

	return brpcreq;
}


KS_DECLARE(ks_status_t) blade_handle_protocolrpc_register(blade_rpc_t *brpc)
{
	blade_handle_t *bh = NULL;
	const char *method = NULL;
	const char *protocol = NULL;
	const char *realm = NULL;
	char *key = NULL;

	ks_assert(brpc);

	bh = blade_rpc_handle_get(brpc);
	ks_assert(bh);

	method = blade_rpc_method_get(brpc);
	ks_assert(method);

	protocol = blade_rpc_protocol_get(brpc);
	ks_assert(protocol);

	realm = blade_rpc_realm_get(brpc);
	ks_assert(realm);

	key = ks_psprintf(bh->pool, "%s@%s/%s", protocol, realm, method);
	ks_assert(key);

	ks_hash_insert(bh->protocolrpcs, (void *)key, brpc);

	ks_log(KS_LOG_DEBUG, "ProtocolRPC Registered: %s\n", key);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_protocolrpc_unregister(blade_rpc_t *brpc)
{
	blade_handle_t *bh = NULL;
	const char *method = NULL;
	const char *protocol = NULL;
	const char *realm = NULL;
	char *key = NULL;

	ks_assert(brpc);

	bh = blade_rpc_handle_get(brpc);
	ks_assert(bh);

	method = blade_rpc_method_get(brpc);
	ks_assert(method);

	protocol = blade_rpc_protocol_get(brpc);
	ks_assert(protocol);

	realm = blade_rpc_realm_get(brpc);
	ks_assert(realm);

	key = ks_psprintf(bh->pool, "%s@%s/%s", protocol, realm, method);
	ks_assert(key);

	ks_log(KS_LOG_DEBUG, "ProtocolRPC Unregistered: %s\n", key);

	ks_hash_remove(bh->protocolrpcs, (void *)key);

	ks_pool_free(bh->pool, &key);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_rpc_t *) blade_handle_protocolrpc_lookup(blade_handle_t *bh, const char *method, const char *protocol, const char *realm)
{
	blade_rpc_t *brpc = NULL;
	char *key = NULL;

	ks_assert(bh);
	ks_assert(method);
	ks_assert(protocol);
	ks_assert(realm);

	key = ks_psprintf(bh->pool, "%s@%s/%s", protocol, realm, method);
	brpc = ks_hash_search(bh->protocolrpcs, (void *)key, KS_READLOCKED);
	ks_hash_read_unlock(bh->protocolrpcs);

	ks_pool_free(bh->pool, &key);

	return brpc;
}

KS_DECLARE(ks_bool_t) blade_handle_subscriber_add(blade_handle_t *bh, blade_subscription_t **bsubP, const char *event, const char *protocol, const char *realm, const char *nodeid)
{
	char *key = NULL;
	blade_subscription_t *bsub = NULL;
	ks_hash_t *bsub_cleanup = NULL;
	ks_bool_t propagate = KS_FALSE;

	ks_assert(bh);
	ks_assert(event);
	ks_assert(protocol);
	ks_assert(realm);
	ks_assert(nodeid);

	key = ks_psprintf(bh->pool, "%s@%s/%s", protocol, realm, event);

	ks_hash_write_lock(bh->subscriptions);

	bsub = (blade_subscription_t *)ks_hash_search(bh->subscriptions, (void *)key, KS_UNLOCKED);

	if (!bsub) {
		blade_subscription_create(&bsub, bh->pool, event, protocol, realm);
		ks_assert(bsub);

		ks_hash_insert(bh->subscriptions, (void *)ks_pstrdup(bh->pool, key), bsub);
		propagate = KS_TRUE;
	}

	bsub_cleanup = (ks_hash_t *)ks_hash_search(bh->subscriptions_cleanup, (void *)nodeid, KS_UNLOCKED);
	if (!bsub_cleanup) {
		ks_hash_create(&bsub_cleanup, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, bh->pool);
		ks_assert(bsub_cleanup);

		ks_log(KS_LOG_DEBUG, "Subscription (%s) added\n", key);
		ks_hash_insert(bh->subscriptions_cleanup, (void *)ks_pstrdup(bh->pool, nodeid), bsub_cleanup);
	}
	ks_hash_insert(bsub_cleanup, (void *)ks_pstrdup(bh->pool, key), (void *)KS_TRUE);

	blade_subscription_subscribers_add(bsub, nodeid);

	ks_hash_write_unlock(bh->subscriptions);

	ks_log(KS_LOG_DEBUG, "Subscription (%s) subscriber (%s) added\n", key, nodeid);

	ks_pool_free(bh->pool, &key);

	if (bsubP) *bsubP = bsub;

	return propagate;
}

KS_DECLARE(ks_bool_t) blade_handle_subscriber_remove(blade_handle_t *bh, blade_subscription_t **bsubP, const char *event, const char *protocol, const char *realm, const char *nodeid)
{
	char *key = NULL;
	blade_subscription_t *bsub = NULL;
	ks_hash_t *bsub_cleanup = NULL;
	ks_bool_t propagate = KS_FALSE;

	ks_assert(bh);
	ks_assert(event);
	ks_assert(protocol);
	ks_assert(realm);
	ks_assert(nodeid);

	key = ks_psprintf(bh->pool, "%s@%s/%s", protocol, realm, event);

	ks_hash_write_lock(bh->subscriptions);

	bsub = (blade_subscription_t *)ks_hash_search(bh->subscriptions, (void *)key, KS_UNLOCKED);

	if (bsub) {
		bsub_cleanup = (ks_hash_t *)ks_hash_search(bh->subscriptions_cleanup, (void *)nodeid, KS_UNLOCKED);
		ks_assert(bsub_cleanup);
		ks_hash_remove(bsub_cleanup, key);

		if (ks_hash_count(bsub_cleanup) == 0) {
			ks_hash_remove(bh->subscriptions_cleanup, (void *)nodeid);
		}

		ks_log(KS_LOG_DEBUG, "Subscription (%s) subscriber (%s) removed\n", key, nodeid);
		blade_subscription_subscribers_remove(bsub, nodeid);

		if (ks_hash_count(blade_subscription_subscribers_get(bsub)) == 0) {
			ks_log(KS_LOG_DEBUG, "Subscription (%s) removed\n", key);
			ks_hash_remove(bh->subscriptions, (void *)key);
			propagate = KS_TRUE;
		}
	}

	ks_hash_write_unlock(bh->subscriptions);

	ks_pool_free(bh->pool, &key);

	if (bsubP) *bsubP = bsub;

	return propagate;
}



KS_DECLARE(ks_status_t) blade_handle_connect(blade_handle_t *bh, blade_connection_t **bcP, blade_identity_t *target, const char *session_id)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_transport_t *bt = NULL;
	blade_transport_callbacks_t *callbacks = NULL;
	const char *tname = NULL;

	ks_assert(bh);
	ks_assert(target);

	// @todo better locking here
	if (bh->local_nodeid) return KS_STATUS_DUPLICATE_OPERATION;

	ks_hash_read_lock(bh->transports);

	tname = blade_identity_parameter_get(target, "transport");
	if (tname) {
		bt = ks_hash_search(bh->transports, (void *)tname, KS_UNLOCKED);
	}
	ks_hash_read_unlock(bh->transports);

	if (!bt) bt = bh->default_transport;

	callbacks = blade_transport_callbacks_get(bt);

	if (callbacks->onconnect) ret = callbacks->onconnect(bcP, bt, target, session_id);

	return ret;
}


KS_DECLARE(ks_status_t) blade_handle_connections_add(blade_connection_t *bc)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_handle_t *bh = NULL;

	ks_assert(bc);

	bh = blade_connection_handle_get(bc);
	ks_assert(bh);

	ks_hash_write_lock(bh->connections);
	ret = ks_hash_insert(bh->connections, (void *)blade_connection_id_get(bc), bc);
	ks_hash_write_unlock(bh->connections);

	return ret;
}

KS_DECLARE(ks_status_t) blade_handle_connections_remove(blade_connection_t *bc)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_handle_t *bh = NULL;

	ks_assert(bc);

	bh = blade_connection_handle_get(bc);
	ks_assert(bh);

	blade_connection_write_lock(bc, KS_TRUE);

	ks_hash_write_lock(bh->connections);
	if (ks_hash_remove(bh->connections, (void *)blade_connection_id_get(bc)) == NULL) ret = KS_STATUS_FAIL;
	ks_hash_write_unlock(bh->connections);

	blade_connection_write_unlock(bc);

	return ret;
}

KS_DECLARE(blade_connection_t *) blade_handle_connections_lookup(blade_handle_t *bh, const char *id)
{
	blade_connection_t *bc = NULL;

	ks_assert(bh);
	ks_assert(id);

	ks_hash_read_lock(bh->connections);
	bc = ks_hash_search(bh->connections, (void *)id, KS_UNLOCKED);
	if (bc && blade_connection_read_lock(bc, KS_FALSE) != KS_STATUS_SUCCESS) bc = NULL;
	ks_hash_read_unlock(bh->connections);

	return bc;
}


KS_DECLARE(ks_status_t) blade_handle_sessions_add(blade_session_t *bs)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_handle_t *bh = NULL;

	ks_assert(bs);

	bh = blade_session_handle_get(bs);
	ks_assert(bh);

	ks_hash_write_lock(bh->sessions);
	ret = ks_hash_insert(bh->sessions, (void *)blade_session_id_get(bs), bs);
	ks_hash_write_unlock(bh->sessions);

	return ret;
}

KS_DECLARE(ks_status_t) blade_handle_sessions_remove(blade_session_t *bs)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_handle_t *bh = NULL;
	ks_pool_t *pool = NULL;
	const char *id = NULL;
	ks_hash_iterator_t *it = NULL;
	ks_bool_t upstream = KS_FALSE;
	ks_bool_t unsubbed = KS_FALSE;

	ks_assert(bs);

	bh = blade_session_handle_get(bs);
	ks_assert(bh);

	pool = blade_handle_pool_get(bh);
	ks_assert(pool);

	blade_session_write_lock(bs, KS_TRUE);

	id = blade_session_id_get(bs);
	ks_assert(id);

	// @todo this cleanup is a bit messy, move to using the combined key rather than passing around all 3 parts would make this cleaner
	while (!unsubbed) {
		ks_hash_t *subscriptions = NULL;
		const char *event = NULL;
		const char *protocol = NULL;
		const char *realm = NULL;

		ks_hash_read_lock(bh->subscriptions);
		subscriptions = (ks_hash_t *)ks_hash_search(bh->subscriptions_cleanup, (void *)id, KS_UNLOCKED);
		if (!subscriptions) unsubbed = KS_TRUE;
		else {
			void *key = NULL;
			void *value = NULL;
			blade_subscription_t *bsub = NULL;

			it = ks_hash_first(subscriptions, KS_UNLOCKED);
			ks_assert(it);

			ks_hash_this(it, (const void **)&key, NULL, &value);

			bsub = (blade_subscription_t *)ks_hash_search(bh->subscriptions, key, KS_UNLOCKED);
			ks_assert(bsub);

			// @note allocate these to avoid lifecycle issues when the last subscriber is removed causing the subscription to be removed
			event = ks_pstrdup(bh->pool, blade_subscription_event_get(bsub));
			protocol = ks_pstrdup(bh->pool, blade_subscription_protocol_get(bsub));
			realm = ks_pstrdup(bh->pool, blade_subscription_realm_get(bsub));
		}
		ks_hash_read_unlock(bh->subscriptions);

		if (!unsubbed) {
			if (blade_handle_subscriber_remove(bh, NULL, event, protocol, realm, id)) {
				blade_protocol_subscribe_raw(bh, event, protocol, realm, KS_TRUE, NULL, NULL);
			}
			ks_pool_free(bh->pool, &event);
			ks_pool_free(bh->pool, &protocol);
			ks_pool_free(bh->pool, &realm);
		}
	}

	ks_hash_write_lock(bh->sessions);
	if (ks_hash_remove(bh->sessions, (void *)id) == NULL) ret = KS_STATUS_FAIL;

	ks_rwl_read_lock(bh->local_nodeid_rwl);
	upstream = bh->local_nodeid && !ks_safe_strcasecmp(bh->local_nodeid, id);
	ks_rwl_read_unlock(bh->local_nodeid_rwl);

	if (upstream) {
		blade_handle_local_nodeid_set(bh, NULL);
		blade_handle_master_nodeid_set(bh, NULL);
		while ((it = ks_hash_first(bh->realms, KS_UNLOCKED))) {
			void *key = NULL;
			void *value = NULL;
			ks_hash_this(it, (const void **)&key, NULL, &value);
			ks_hash_remove(bh->realms, key);
		}
	}

	ks_hash_write_unlock(bh->sessions);

	blade_session_write_unlock(bs);

	return ret;
}

KS_DECLARE(blade_session_t *) blade_handle_sessions_lookup(blade_handle_t *bh, const char *id)
{
	blade_session_t *bs = NULL;

	ks_assert(bh);
	ks_assert(id);

	ks_hash_read_lock(bh->sessions);
	bs = ks_hash_search(bh->sessions, (void *)id, KS_UNLOCKED);
	if (bs && blade_session_read_lock(bs, KS_FALSE) != KS_STATUS_SUCCESS) bs = NULL;
	ks_hash_read_unlock(bh->sessions);

	return bs;
}

KS_DECLARE(blade_session_t *) blade_handle_sessions_upstream(blade_handle_t *bh)
{
	blade_session_t *bs = NULL;

	ks_assert(bh);

	ks_rwl_read_lock(bh->local_nodeid_rwl);
	if (bh->local_nodeid) bs = blade_handle_sessions_lookup(bh, bh->local_nodeid);
	ks_rwl_read_unlock(bh->local_nodeid_rwl);

	return bs;
}

KS_DECLARE(void) blade_handle_sessions_send(blade_handle_t *bh, ks_list_t *sessions, const char *exclude, cJSON *json)
{
	blade_session_t *bs = NULL;

	ks_assert(bh);
	ks_assert(sessions);
	ks_assert(json);

	ks_list_iterator_start(sessions);
	while (ks_list_iterator_hasnext(sessions)) {
		const char *sessionid = ks_list_iterator_next(sessions);
		if (exclude && !strcmp(exclude, sessionid)) continue;
		bs = blade_handle_sessions_lookup(bh, sessionid);
		if (!bs) {
			ks_log(KS_LOG_DEBUG, "This should not happen\n");
			continue;
		}
		blade_session_send(bs, json, NULL, NULL);
		blade_session_read_unlock(bs);
	}
	ks_list_iterator_stop(sessions);
}

KS_DECLARE(ks_status_t) blade_handle_session_state_callback_register(blade_handle_t *bh, void *data, blade_session_state_callback_t callback, const char **id)
{
	blade_handle_session_state_callback_registration_t *bhsscr = NULL;

	ks_assert(bh);
	ks_assert(callback);
	ks_assert(id);

	blade_handle_session_state_callback_registration_create(&bhsscr, blade_handle_pool_get(bh), data, callback);
	ks_assert(bhsscr);

	ks_hash_insert(bh->session_state_callbacks, (void *)bhsscr->id, bhsscr);

	*id = bhsscr->id;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_session_state_callback_unregister(blade_handle_t *bh, const char *id)
{
	ks_assert(bh);
	ks_assert(id);

	ks_hash_remove(bh->session_state_callbacks, (void *)id);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(void) blade_handle_session_state_callbacks_execute(blade_session_t *bs, blade_session_state_condition_t condition)
{
	blade_handle_t *bh = NULL;
	ks_hash_iterator_t *it = NULL;

	ks_assert(bs);

	if (blade_session_state_get(bs) == BLADE_SESSION_STATE_NONE) return;

	bh = blade_session_handle_get(bs);
	ks_assert(bh);

	ks_hash_read_lock(bh->session_state_callbacks);
	for (it = ks_hash_first(bh->session_state_callbacks, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		void *key = NULL;
		blade_handle_session_state_callback_registration_t *value = NULL;

		ks_hash_this(it, (const void **)&key, NULL, (void **)&value);

		value->callback(bs, condition, value->data);
	}
	ks_hash_read_unlock(bh->session_state_callbacks);
}


// BLADE PROTOCOL HANDLERS

// @todo revisit all error sending. JSONRPC "error" should only be used for json parsing errors, change the rest to internal errors for each of the corerpcs
// @todo all higher level errors should be handled by each of the calls internally so that a normal result response can be sent with an error block inside the result
// which is important for implementation of blade.execute where errors can be relayed back to the requester properly

// blade.register request generator
KS_DECLARE(ks_status_t) blade_protocol_register(blade_handle_t *bh, const char *nodeid, ks_bool_t remove, blade_rpc_response_callback_t callback, void *data)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_session_t *bs = NULL;
	ks_pool_t *pool = NULL;
	cJSON *req = NULL;
	cJSON *req_params = NULL;

	ks_assert(bh);
	ks_assert(nodeid);

	if (!(bs = blade_handle_sessions_upstream(bh))) {
		ret = KS_STATUS_DISCONNECTED;
		goto done;
	}

	pool = blade_handle_pool_get(bh);
	ks_assert(pool);

	blade_rpc_request_raw_create(pool, &req, &req_params, NULL, "blade.register");

	// fill in the req_params
	cJSON_AddStringToObject(req_params, "nodeid", nodeid);
	if (remove) cJSON_AddTrueToObject(req_params, "remove");

	ks_log(KS_LOG_DEBUG, "Session (%s) register request (%s %s) started\n", blade_session_id_get(bs), remove ? "removing" : "adding", nodeid);

	ret = blade_session_send(bs, req, callback, data);

done:
	if (req) cJSON_Delete(req);
	if (bs) blade_session_read_unlock(bs);

	return ret;
}

// blade.register request handler
ks_bool_t blade_protocol_register_request_handler(blade_rpc_request_t *brpcreq, void *data)
{
	blade_handle_t *bh = NULL;
	blade_session_t *bs = NULL;
	cJSON *req = NULL;
	cJSON *req_params = NULL;
	const char *req_params_nodeid = NULL;
	cJSON *req_params_remove = NULL;
	ks_bool_t remove = KS_FALSE;
	cJSON *res = NULL;
	cJSON *res_result = NULL;

	ks_assert(brpcreq);

	bh = blade_rpc_request_handle_get(brpcreq);
	ks_assert(bh);

	bs = blade_handle_sessions_lookup(bh, blade_rpc_request_sessionid_get(brpcreq));
	ks_assert(bs);

	req = blade_rpc_request_message_get(brpcreq);
	ks_assert(req);

	req_params = cJSON_GetObjectItem(req, "params");
	if (!req_params) {
		ks_log(KS_LOG_DEBUG, "Session (%s) register request missing 'params' object\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params object");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	req_params_nodeid = cJSON_GetObjectCstr(req_params, "nodeid");
	if (!req_params_nodeid) {
		ks_log(KS_LOG_DEBUG, "Session (%s) register request missing 'nodeid'\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params nodeid");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}
	req_params_remove = cJSON_GetObjectItem(req_params, "remove");
	remove = req_params_remove && req_params_remove->type == cJSON_True;

	ks_log(KS_LOG_DEBUG, "Session (%s) register request (%s %s) processing\n", blade_session_id_get(bs), remove ? "removing" : "adding", req_params_nodeid);

	if (remove) {
		blade_session_route_remove(bs, req_params_nodeid);
		blade_handle_route_remove(bh, req_params_nodeid);
	} else {
		blade_session_route_add(bs, req_params_nodeid);
		blade_handle_route_add(bh, req_params_nodeid, blade_session_id_get(bs));
	}

	blade_rpc_response_raw_create(&res, &res_result, blade_rpc_request_messageid_get(brpcreq));
	blade_session_send(bs, res, NULL, NULL);

done:

	if (res) cJSON_Delete(res);
	if (bs) blade_session_read_unlock(bs);

	return KS_FALSE;
}


// blade.publish request generator
KS_DECLARE(ks_status_t) blade_protocol_publish(blade_handle_t *bh, const char *name, const char *realm, blade_rpc_response_callback_t callback, void *data)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_session_t *bs = NULL;
	ks_pool_t *pool = NULL;
	cJSON *req = NULL;
	cJSON *req_params = NULL;

	ks_assert(bh);
	ks_assert(name);
	ks_assert(realm);

	if (!(bs = blade_handle_sessions_upstream(bh))) {
		ret = KS_STATUS_DISCONNECTED;
		goto done;
	}

	pool = blade_handle_pool_get(bh);
	ks_assert(pool);

	blade_rpc_request_raw_create(pool, &req, &req_params, NULL, "blade.publish");

	// fill in the req_params
	cJSON_AddStringToObject(req_params, "protocol", name);
	cJSON_AddStringToObject(req_params, "realm", realm);

	ks_rwl_read_lock(bh->local_nodeid_rwl);
	cJSON_AddStringToObject(req_params, "requester-nodeid", bh->local_nodeid);
	ks_rwl_read_unlock(bh->local_nodeid_rwl);

	ks_rwl_read_lock(bh->master_nodeid_rwl);
	cJSON_AddStringToObject(req_params, "responder-nodeid", bh->master_nodeid);
	ks_rwl_read_unlock(bh->master_nodeid_rwl);

	// @todo add a parameter containing a block of json for schema definitions for each of the methods being published

	ks_log(KS_LOG_DEBUG, "Session (%s) publish request started\n", blade_session_id_get(bs));

	ret = blade_session_send(bs, req, callback, data);

done:
	if (req) cJSON_Delete(req);
	if (bs) blade_session_read_unlock(bs);

	return ret;
}

// blade.publish request handler
ks_bool_t blade_protocol_publish_request_handler(blade_rpc_request_t *brpcreq, void *data)
{
	blade_handle_t *bh = NULL;
	blade_session_t *bs = NULL;
	cJSON *req = NULL;
	cJSON *req_params = NULL;
	const char *req_params_protocol = NULL;
	const char *req_params_realm = NULL;
	const char *req_params_requester_nodeid = NULL;
	const char *req_params_responder_nodeid = NULL;
	cJSON *res = NULL;
	cJSON *res_result = NULL;
	blade_protocol_t *bp = NULL;
	const char *bp_key = NULL;
	ks_hash_t *bp_cleanup = NULL;

	ks_assert(brpcreq);

	bh = blade_rpc_request_handle_get(brpcreq);
	ks_assert(bh);

	bs = blade_handle_sessions_lookup(bh, blade_rpc_request_sessionid_get(brpcreq));
	ks_assert(bs);

	req = blade_rpc_request_message_get(brpcreq);
	ks_assert(req);

	req_params = cJSON_GetObjectItem(req, "params");
	if (!req_params) {
		ks_log(KS_LOG_DEBUG, "Session (%s) publish request missing 'params' object\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params object");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	req_params_protocol = cJSON_GetObjectCstr(req_params, "protocol");
	if (!req_params_protocol) {
		ks_log(KS_LOG_DEBUG, "Session (%s) publish request missing 'protocol'\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params protocol");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	req_params_realm = cJSON_GetObjectCstr(req_params, "realm");
	if (!req_params_realm) {
		ks_log(KS_LOG_DEBUG, "Session (%s) publish request missing 'realm'\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params realm");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	// @todo confirm the realm is permitted for the session, this gets complicated with subdomains, skipping for now

	req_params_requester_nodeid = cJSON_GetObjectCstr(req_params, "requester-nodeid");
	if (!req_params_requester_nodeid) {
		ks_log(KS_LOG_DEBUG, "Session (%s) publish request missing 'requester-nodeid'\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params requester-nodeid");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	req_params_responder_nodeid = cJSON_GetObjectCstr(req_params, "responder-nodeid");
	if (!req_params_responder_nodeid) {
		ks_log(KS_LOG_DEBUG, "Session (%s) publish request missing 'responder-nodeid'\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params responder-nodeid");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	if (!blade_handle_master_nodeid_compare(bh, req_params_responder_nodeid)) {
		ks_log(KS_LOG_DEBUG, "Session (%s) publish request invalid 'responder-nodeid' (%s)\n", blade_session_id_get(bs), req_params_responder_nodeid);
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Invalid params responder-nodeid");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	ks_log(KS_LOG_DEBUG, "Session (%s) publish request (%s to %s) processing\n", blade_session_id_get(bs), req_params_requester_nodeid, req_params_responder_nodeid);

	bp_key = ks_psprintf(bh->pool, "%s@%s", req_params_protocol, req_params_realm);

	ks_hash_write_lock(bh->protocols);

	bp = (blade_protocol_t *)ks_hash_search(bh->protocols, (void *)bp_key, KS_UNLOCKED);
	if (bp) {
		// @todo deal with exclusive stuff when the protocol is already registered
	}

	if (!bp) {
		blade_protocol_create(&bp, bh->pool, req_params_protocol, req_params_realm);
		ks_assert(bp);

		ks_log(KS_LOG_DEBUG, "Protocol (%s) added\n", bp_key);
		ks_hash_insert(bh->protocols, (void *)ks_pstrdup(bh->pool, bp_key), bp);
	}

	bp_cleanup = (ks_hash_t *)ks_hash_search(bh->protocols_cleanup, (void *)req_params_requester_nodeid, KS_UNLOCKED);
	if (!bp_cleanup) {
		ks_hash_create(&bp_cleanup, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, bh->pool);
		ks_assert(bp_cleanup);

		ks_hash_insert(bh->protocols_cleanup, (void *)ks_pstrdup(bh->pool, req_params_requester_nodeid), bp_cleanup);
	}
	ks_hash_insert(bp_cleanup, (void *)ks_pstrdup(bh->pool, bp_key), (void *)KS_TRUE);
	blade_protocol_providers_add(bp, req_params_requester_nodeid);
	ks_log(KS_LOG_DEBUG, "Protocol (%s) provider (%s) added\n", bp_key, req_params_requester_nodeid);

	ks_hash_write_unlock(bh->protocols);


	// build the actual response finally
	blade_rpc_response_raw_create(&res, &res_result, blade_rpc_request_messageid_get(brpcreq));

	cJSON_AddStringToObject(res_result, "protocol", req_params_protocol);
	cJSON_AddStringToObject(res_result, "realm", req_params_realm);
	cJSON_AddStringToObject(res_result, "requester-nodeid", req_params_requester_nodeid);
	cJSON_AddStringToObject(res_result, "responder-nodeid", req_params_responder_nodeid);

	// request was just received on a session that is already read locked, so we can assume the response goes back on the same session without further lookup
	blade_session_send(bs, res, NULL, NULL);

done:

	if (res) cJSON_Delete(res);
	if (bs) blade_session_read_unlock(bs);

	return KS_FALSE;
}


// blade.locate request generator
KS_DECLARE(ks_status_t) blade_protocol_locate(blade_handle_t *bh, const char *name, const char *realm, blade_rpc_response_callback_t callback, void *data)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_session_t *bs = NULL;
	ks_pool_t *pool = NULL;
	cJSON *req = NULL;
	cJSON *req_params = NULL;

	ks_assert(bh);
	ks_assert(name);
	ks_assert(realm);

	if (!(bs = blade_handle_sessions_upstream(bh))) {
		ret = KS_STATUS_DISCONNECTED;
		goto done;
	}

	pool = blade_handle_pool_get(bh);
	ks_assert(pool);

	blade_rpc_request_raw_create(pool, &req, &req_params, NULL, "blade.locate");

	// fill in the req_params
	cJSON_AddStringToObject(req_params, "protocol", name);
	cJSON_AddStringToObject(req_params, "realm", realm);

	ks_rwl_read_lock(bh->local_nodeid_rwl);
	cJSON_AddStringToObject(req_params, "requester-nodeid", bh->local_nodeid);
	ks_rwl_read_unlock(bh->local_nodeid_rwl);

	ks_rwl_read_lock(bh->master_nodeid_rwl);
	cJSON_AddStringToObject(req_params, "responder-nodeid", bh->master_nodeid);
	ks_rwl_read_unlock(bh->master_nodeid_rwl);

	ks_log(KS_LOG_DEBUG, "Session (%s) locate request started\n", blade_session_id_get(bs));

	ret = blade_session_send(bs, req, callback, data);

done:
	if (req) cJSON_Delete(req);
	if (bs) blade_session_read_unlock(bs);

	return ret;
}

// blade.locate request handler
ks_bool_t blade_protocol_locate_request_handler(blade_rpc_request_t *brpcreq, void *data)
{
	blade_handle_t *bh = NULL;
	blade_session_t *bs = NULL;
	cJSON *req = NULL;
	cJSON *req_params = NULL;
	const char *req_params_protocol = NULL;
	const char *req_params_realm = NULL;
	const char *req_params_requester_nodeid = NULL;
	const char *req_params_responder_nodeid = NULL;
	cJSON *res = NULL;
	cJSON *res_result = NULL;
	cJSON *res_result_providers;
	blade_protocol_t *bp = NULL;
	const char *bp_key = NULL;

	ks_assert(brpcreq);

	bh = blade_rpc_request_handle_get(brpcreq);
	ks_assert(bh);

	bs = blade_handle_sessions_lookup(bh, blade_rpc_request_sessionid_get(brpcreq));
	ks_assert(bs);

	req = blade_rpc_request_message_get(brpcreq);
	ks_assert(req);

	req_params = cJSON_GetObjectItem(req, "params");
	if (!req_params) {
		ks_log(KS_LOG_DEBUG, "Session (%s) locate request missing 'params' object\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params object");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	req_params_protocol = cJSON_GetObjectCstr(req_params, "protocol");
	if (!req_params_protocol) {
		ks_log(KS_LOG_DEBUG, "Session (%s) locate request missing 'protocol'\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params protocol");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	req_params_realm = cJSON_GetObjectCstr(req_params, "realm");
	if (!req_params_realm) {
		ks_log(KS_LOG_DEBUG, "Session (%s) locate request missing 'realm'\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params realm");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	// @todo confirm the realm is permitted for the session, this gets complicated with subdomains, skipping for now

	req_params_requester_nodeid = cJSON_GetObjectCstr(req_params, "requester-nodeid");
	if (!req_params_requester_nodeid) {
		ks_log(KS_LOG_DEBUG, "Session (%s) locate request missing 'requester-nodeid'\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params requester-nodeid");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	req_params_responder_nodeid = cJSON_GetObjectCstr(req_params, "responder-nodeid");
	if (!req_params_responder_nodeid) {
		ks_log(KS_LOG_DEBUG, "Session (%s) locate request missing 'responder-nodeid'\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params responder-nodeid");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	if (!blade_handle_master_nodeid_compare(bh, req_params_responder_nodeid)) {
		ks_log(KS_LOG_DEBUG, "Session (%s) locate request invalid 'responder-nodeid' (%s)\n", blade_session_id_get(bs), req_params_responder_nodeid);
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Invalid params responder-nodeid");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	ks_log(KS_LOG_DEBUG, "Session (%s) locate request (%s to %s) processing\n", blade_session_id_get(bs), req_params_requester_nodeid, req_params_responder_nodeid);

	res_result_providers = cJSON_CreateObject();

	bp_key = ks_psprintf(bh->pool, "%s@%s", req_params_protocol, req_params_realm);

	ks_hash_read_lock(bh->protocols);

	bp = (blade_protocol_t *)ks_hash_search(bh->protocols, (void *)bp_key, KS_UNLOCKED);
	if (bp) {
		ks_hash_t *providers = blade_protocol_providers_get(bp);
		for (ks_hash_iterator_t *it = ks_hash_first(providers, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
			const char *key = NULL;
			void *value = NULL;

			ks_hash_this(it, (const void **)&key, NULL, &value);

			cJSON_AddItemToArray(res_result_providers, cJSON_CreateString(key));
		}
	}

	ks_hash_read_unlock(bh->protocols);


	// build the actual response finally
	blade_rpc_response_raw_create(&res, &res_result, blade_rpc_request_messageid_get(brpcreq));

	cJSON_AddStringToObject(res_result, "protocol", req_params_protocol);
	cJSON_AddStringToObject(res_result, "realm", req_params_realm);
	cJSON_AddStringToObject(res_result, "requester-nodeid", req_params_requester_nodeid);
	cJSON_AddStringToObject(res_result, "responder-nodeid", req_params_responder_nodeid);
	cJSON_AddItemToObject(res_result, "providers", res_result_providers);

	// request was just received on a session that is already read locked, so we can assume the response goes back on the same session without further lookup
	blade_session_send(bs, res, NULL, NULL);

done:

	if (res) cJSON_Delete(res);
	if (bs) blade_session_read_unlock(bs);

	return KS_FALSE;
}


// blade.execute request generator
KS_DECLARE(ks_status_t) blade_protocol_execute(blade_handle_t *bh, const char *nodeid, const char *method, const char *protocol, const char *realm, cJSON *params, blade_rpc_response_callback_t callback, void *data)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_session_t *bs = NULL;
	ks_pool_t *pool = NULL;
	cJSON *req = NULL;
	cJSON *req_params = NULL;

	ks_assert(bh);
	ks_assert(nodeid);
	ks_assert(method);
	ks_assert(protocol);
	ks_assert(realm);

	if (!(bs = blade_handle_route_lookup(bh, nodeid))) {
		if (!(bs = blade_handle_sessions_upstream(bh))) {
			ret = KS_STATUS_DISCONNECTED;
			goto done;
		}
	}

	pool = blade_handle_pool_get(bh);
	ks_assert(pool);

	blade_rpc_request_raw_create(pool, &req, &req_params, NULL, "blade.execute");

	// fill in the req_params
	cJSON_AddStringToObject(req_params, "method", method);
	cJSON_AddStringToObject(req_params, "protocol", protocol);
	cJSON_AddStringToObject(req_params, "realm", realm);

	ks_rwl_read_lock(bh->local_nodeid_rwl);
	cJSON_AddStringToObject(req_params, "requester-nodeid", bh->local_nodeid);
	ks_rwl_read_unlock(bh->local_nodeid_rwl);

	ks_rwl_read_lock(bh->master_nodeid_rwl);
	cJSON_AddStringToObject(req_params, "responder-nodeid", nodeid);
	ks_rwl_read_unlock(bh->master_nodeid_rwl);

	if (params) cJSON_AddItemToObject(req_params, "params", cJSON_Duplicate(params, 1));

	ks_log(KS_LOG_DEBUG, "Session (%s) execute request started\n", blade_session_id_get(bs));

	ret = blade_session_send(bs, req, callback, data);

done:
	if (req) cJSON_Delete(req);
	if (bs) blade_session_read_unlock(bs);

	return ret;
}

// blade.execute request handler
ks_bool_t blade_protocol_execute_request_handler(blade_rpc_request_t *brpcreq, void *data)
{
	ks_bool_t ret = KS_FALSE;
	blade_handle_t *bh = NULL;
	blade_session_t *bs = NULL;
	cJSON *req = NULL;
	cJSON *req_params = NULL;
	const char *req_params_method = NULL;
	const char *req_params_protocol = NULL;
	const char *req_params_realm = NULL;
	const char *req_params_requester_nodeid = NULL;
	const char *req_params_responder_nodeid = NULL;
	blade_rpc_t *brpc = NULL;
	blade_rpc_request_callback_t callback = NULL;
	cJSON *res = NULL;

	ks_assert(brpcreq);

	bh = blade_rpc_request_handle_get(brpcreq);
	ks_assert(bh);

	bs = blade_handle_sessions_lookup(bh, blade_rpc_request_sessionid_get(brpcreq));
	ks_assert(bs);

	req = blade_rpc_request_message_get(brpcreq);
	ks_assert(req);

	req_params = cJSON_GetObjectItem(req, "params");
	if (!req_params) {
		ks_log(KS_LOG_DEBUG, "Session (%s) execute request missing 'params' object\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params object");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	req_params_method = cJSON_GetObjectCstr(req_params, "method");
	if (!req_params_method) {
		ks_log(KS_LOG_DEBUG, "Session (%s) execute request missing 'method'\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params method");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	req_params_protocol = cJSON_GetObjectCstr(req_params, "protocol");
	if (!req_params_protocol) {
		ks_log(KS_LOG_DEBUG, "Session (%s) execute request missing 'protocol'\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params protocol");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	req_params_realm = cJSON_GetObjectCstr(req_params, "realm");
	if (!req_params_realm) {
		ks_log(KS_LOG_DEBUG, "Session (%s) execute request missing 'realm'\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params realm");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	// @todo confirm the realm is permitted for the session, this gets complicated with subdomains, skipping for now

	req_params_requester_nodeid = cJSON_GetObjectCstr(req_params, "requester-nodeid");
	if (!req_params_requester_nodeid) {
		ks_log(KS_LOG_DEBUG, "Session (%s) execute request missing 'requester-nodeid'\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params requester-nodeid");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	req_params_responder_nodeid = cJSON_GetObjectCstr(req_params, "responder-nodeid");
	if (!req_params_responder_nodeid) {
		ks_log(KS_LOG_DEBUG, "Session (%s) execute request missing 'responder-nodeid'\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params responder-nodeid");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	ks_log(KS_LOG_DEBUG, "Session (%s) execute request (%s to %s) processing\n", blade_session_id_get(bs), req_params_requester_nodeid, req_params_responder_nodeid);

	// @todo pull out nested params block if it exists and check against schema later, so blade_rpc_t should be able to carry a schema with it, even though blade.xxx may not associate one

	brpc = blade_handle_protocolrpc_lookup(bh, req_params_method, req_params_protocol, req_params_realm);
	if (!brpc) {
		ks_log(KS_LOG_DEBUG, "Session (%s) execute request unknown method\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Unknown params method");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	callback = blade_rpc_callback_get(brpc);
	if (callback) ret = callback(brpcreq, blade_rpc_callback_data_get(brpc));

done:

	if (res) cJSON_Delete(res);
	if (bs) blade_session_read_unlock(bs);

	return ret;
}

KS_DECLARE(cJSON *) blade_protocol_execute_request_params_get(blade_rpc_request_t *brpcreq)
{
	cJSON *req = NULL;
	cJSON *req_params = NULL;
	cJSON *req_params_params = NULL;

	ks_assert(brpcreq);

	req = blade_rpc_request_message_get(brpcreq);
	ks_assert(req);

	req_params = cJSON_GetObjectItem(req, "params");
	if (req_params) req_params_params = cJSON_GetObjectItem(req_params, "params");

	return req_params_params;
}

KS_DECLARE(cJSON *) blade_protocol_execute_response_result_get(blade_rpc_response_t *brpcres)
{
	cJSON *res = NULL;
	cJSON *res_result = NULL;
	cJSON *res_result_result = NULL;

	ks_assert(brpcres);

	res = blade_rpc_response_message_get(brpcres);
	ks_assert(res);

	res_result = cJSON_GetObjectItem(res, "result");
	if (res_result) res_result_result = cJSON_GetObjectItem(res_result, "result");

	return res_result_result;
}

// @note added blade_rpc_request_duplicate() to support async responding where the callbacks return immediately and the blade_rpc_request_t will be destroyed,
// in such cases duplicate the request to retain a copy for passing to blade_protocol_execute_response_send when sending the response as it contains everything
// needed to produce a response except the inner result block for blade.execute and call blade_rpc_request_destroy() to clean up the duplicate when finished
KS_DECLARE(void) blade_protocol_execute_response_send(blade_rpc_request_t *brpcreq, cJSON *result)
{
	blade_handle_t *bh = NULL;
	blade_session_t *bs = NULL;
	cJSON *req = NULL;
	cJSON *req_params = NULL;
	//const char *req_params_method = NULL;
	const char *req_params_protocol = NULL;
	const char *req_params_realm = NULL;
	const char *req_params_requester_nodeid = NULL;
	const char *req_params_responder_nodeid = NULL;
	cJSON *res = NULL;
	cJSON *res_result = NULL;

	ks_assert(brpcreq);

	bh = blade_rpc_request_handle_get(brpcreq);
	ks_assert(bh);

	bs = blade_handle_sessions_lookup(bh, blade_rpc_request_sessionid_get(brpcreq));
	ks_assert(bs);

	req = blade_rpc_request_message_get(brpcreq);
	ks_assert(req);

	req_params = cJSON_GetObjectItem(req, "params");
	ks_assert(req_params);

	req_params_protocol = cJSON_GetObjectCstr(req_params, "protocol");
	ks_assert(req_params_protocol);

	req_params_realm = cJSON_GetObjectCstr(req_params, "realm");
	ks_assert(req_params_realm);

	req_params_requester_nodeid = cJSON_GetObjectCstr(req_params, "requester-nodeid");
	ks_assert(req_params_requester_nodeid);

	req_params_responder_nodeid = cJSON_GetObjectCstr(req_params, "responder-nodeid");
	ks_assert(req_params_responder_nodeid);

	// build the actual response finally, wrap this into blade_protocol_execute_response_send()
	blade_rpc_response_raw_create(&res, &res_result, blade_rpc_request_messageid_get(brpcreq));

	cJSON_AddStringToObject(res_result, "protocol", req_params_protocol);
	cJSON_AddStringToObject(res_result, "realm", req_params_realm);
	cJSON_AddStringToObject(res_result, "requester-nodeid", req_params_requester_nodeid);
	cJSON_AddStringToObject(res_result, "responder-nodeid", req_params_responder_nodeid);
	if (result) cJSON_AddItemToObject(res_result, "result", cJSON_Duplicate(result, 1));

	// request was just received on a session that is already read locked, so we can assume the response goes back on the same session without further lookup
	blade_session_send(bs, res, NULL, NULL);

	cJSON_Delete(res);

	blade_session_read_unlock(bs);
}


// blade.subscribe request generator
KS_DECLARE(ks_status_t) blade_protocol_subscribe(blade_handle_t *bh, const char *event, const char *protocol, const char *realm, ks_bool_t remove, blade_rpc_response_callback_t callback, void *data, blade_rpc_request_callback_t event_callback, void *event_data)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_session_t *bs = NULL;
	ks_bool_t propagate = KS_FALSE;
	blade_subscription_t *bsub = NULL;

	ks_assert(bh);
	ks_assert(event);
	ks_assert(protocol);
	ks_assert(realm);

	if (!(bs = blade_handle_sessions_upstream(bh))) {
		ret = KS_STATUS_DISCONNECTED;
		goto done;
	}

	if (remove) {
		propagate = blade_handle_subscriber_remove(bh, &bsub, event, protocol, realm, bh->local_nodeid);
	} else {
		propagate = blade_handle_subscriber_add(bh, &bsub, event, protocol, realm, bh->local_nodeid);
		ks_assert(event_callback);
	}
	if (bsub) {
		blade_subscription_callback_set(bsub, event_callback);
		blade_subscription_callback_data_set(bsub, event_data);
	}

	if (propagate) ret = blade_protocol_subscribe_raw(bh, event, protocol, realm, remove, callback, data);

done:
	if (bs) blade_session_read_unlock(bs);

	return ret;
}

KS_DECLARE(ks_status_t) blade_protocol_subscribe_raw(blade_handle_t *bh, const char *event, const char *protocol, const char *realm, ks_bool_t remove, blade_rpc_response_callback_t callback, void *data)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_session_t *bs = NULL;
	ks_pool_t *pool = NULL;
	cJSON *req = NULL;
	cJSON *req_params = NULL;

	ks_assert(bh);
	ks_assert(event);
	ks_assert(protocol);
	ks_assert(realm);

	if (!(bs = blade_handle_sessions_upstream(bh))) {
		ret = KS_STATUS_DISCONNECTED;
		goto done;
	}

	pool = blade_handle_pool_get(bh);
	ks_assert(pool);

	blade_rpc_request_raw_create(pool, &req, &req_params, NULL, "blade.subscribe");

	cJSON_AddStringToObject(req_params, "event", event);
	cJSON_AddStringToObject(req_params, "protocol", protocol);
	cJSON_AddStringToObject(req_params, "realm", realm);
	if (remove) cJSON_AddTrueToObject(req_params, "remove");

	ks_log(KS_LOG_DEBUG, "Session (%s) subscribe request started\n", blade_session_id_get(bs));

	ret = blade_session_send(bs, req, callback, data);

done:
	if (req) cJSON_Delete(req);
	if (bs) blade_session_read_unlock(bs);

	return ret;
}

// blade.subscribe request handler
ks_bool_t blade_protocol_subscribe_request_handler(blade_rpc_request_t *brpcreq, void *data)
{
	blade_handle_t *bh = NULL;
	blade_session_t *bs = NULL;
	ks_pool_t *pool = NULL;
	cJSON *req = NULL;
	cJSON *req_params = NULL;
	const char *req_params_event = NULL;
	const char *req_params_protocol = NULL;
	const char *req_params_realm = NULL;
	cJSON *req_params_remove = NULL;
	ks_bool_t remove = KS_FALSE;
	cJSON *res = NULL;
	cJSON *res_result = NULL;
	ks_bool_t propagate = KS_FALSE;

	ks_assert(brpcreq);

	bh = blade_rpc_request_handle_get(brpcreq);
	ks_assert(bh);

	pool = blade_handle_pool_get(bh);
	ks_assert(pool);

	bs = blade_handle_sessions_lookup(bh, blade_rpc_request_sessionid_get(brpcreq));
	ks_assert(bs);

	req = blade_rpc_request_message_get(brpcreq);
	ks_assert(req);

	req_params = cJSON_GetObjectItem(req, "params");
	if (!req_params) {
		ks_log(KS_LOG_DEBUG, "Session (%s) subscribe request missing 'params' object\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params object");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	req_params_event = cJSON_GetObjectCstr(req_params, "event");
	if (!req_params_event) {
		ks_log(KS_LOG_DEBUG, "Session (%s) subscribe request missing 'event'\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params event");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	req_params_protocol = cJSON_GetObjectCstr(req_params, "protocol");
	if (!req_params_protocol) {
		ks_log(KS_LOG_DEBUG, "Session (%s) subscribe request missing 'protocol'\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params protocol");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	req_params_realm = cJSON_GetObjectCstr(req_params, "realm");
	if (!req_params_realm) {
		ks_log(KS_LOG_DEBUG, "Session (%s) subscribe request missing 'realm'\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params realm");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	req_params_remove = cJSON_GetObjectItem(req_params, "remove");
	remove = req_params_remove && req_params_remove->type == cJSON_True;

	// @todo confirm the realm is permitted for the session, this gets complicated with subdomains, skipping for now

	ks_log(KS_LOG_DEBUG, "Session (%s) subscribe request processing\n", blade_session_id_get(bs));

	if (remove) {
		propagate = blade_handle_subscriber_remove(bh, NULL, req_params_event, req_params_protocol, req_params_realm, blade_session_id_get(bs));
	} else {
		propagate = blade_handle_subscriber_add(bh, NULL, req_params_event, req_params_protocol, req_params_realm, blade_session_id_get(bs));
	}

	if (propagate) blade_protocol_subscribe_raw(bh, req_params_event, req_params_protocol, req_params_realm, remove, NULL, NULL);

	// build the actual response finally
	blade_rpc_response_raw_create(&res, &res_result, blade_rpc_request_messageid_get(brpcreq));

	cJSON_AddStringToObject(res_result, "event", req_params_event);
	cJSON_AddStringToObject(res_result, "protocol", req_params_protocol);
	cJSON_AddStringToObject(res_result, "realm", req_params_realm);

	// request was just received on a session that is already read locked, so we can assume the response goes back on the same session without further lookup
	blade_session_send(bs, res, NULL, NULL);

done:

	if (res) cJSON_Delete(res);
	if (bs) blade_session_read_unlock(bs);

	return KS_FALSE;
}


// blade.broadcast request generator
KS_DECLARE(ks_status_t) blade_protocol_broadcast(blade_handle_t *bh, const char *event, const char *protocol, const char *realm, cJSON *params, blade_rpc_response_callback_t callback, void *data)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(bh);
	ks_assert(event);
	ks_assert(protocol);
	ks_assert(realm);

	// this will ensure any downstream subscriber sessions, and upstream session if available will be broadcasted to
	ret = blade_protocol_broadcast_raw(bh, NULL, event, protocol, realm, params, callback, data);

	// @todo must check if the local node is also subscribed to receive the event, this is a special edge case which has some extra considerations
	// if the local node is subscribed to receive the event, it should be received here as a special case, otherwise the broadcast request handler
	// is where this normally occurs

	return ret;
}

KS_DECLARE(ks_status_t) blade_protocol_broadcast_raw(blade_handle_t *bh, const char *excluded_nodeid, const char *event, const char *protocol, const char *realm, cJSON *params, blade_rpc_response_callback_t callback, void *data)
{
	const char *bsub_key = NULL;
	blade_subscription_t *bsub = NULL;
	blade_session_t *bs = NULL;

	ks_assert(bh);
	ks_assert(event);
	ks_assert(protocol);
	ks_assert(realm);

	bsub_key = ks_psprintf(bh->pool, "%s@%s/%s", protocol, realm, event);

	ks_hash_read_lock(bh->subscriptions);

	bsub = (blade_subscription_t *)ks_hash_search(bh->subscriptions, (void *)bsub_key, KS_UNLOCKED);
	if (bsub) {
		ks_hash_t *subscribers = blade_subscription_subscribers_get(bsub);

		ks_assert(subscribers);

		for (ks_hash_iterator_t *it = ks_hash_first(subscribers, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
			void *key = NULL;
			void *value = NULL;
			cJSON *req = NULL;
			cJSON *req_params = NULL;

			ks_hash_this(it, (const void **)&key, NULL, &value);

			if (excluded_nodeid && !ks_safe_strcasecmp(excluded_nodeid, (const char *)key)) continue;

			if (blade_handle_local_nodeid_compare(bh, (const char *)key)) continue;

			bs = blade_handle_sessions_lookup(bh, (const char *)key);
			if (bs) {
				ks_log(KS_LOG_DEBUG, "Session (%s) broadcast request started\n", blade_session_id_get(bs));

				blade_rpc_request_raw_create(bh->pool, &req, &req_params, NULL, "blade.broadcast");

				cJSON_AddStringToObject(req_params, "event", event);
				cJSON_AddStringToObject(req_params, "protocol", protocol);
				cJSON_AddStringToObject(req_params, "realm", realm);

				if (params) cJSON_AddItemToObject(req_params, "params", cJSON_Duplicate(params, 1));

				blade_session_send(bs, req, callback, data);

				cJSON_Delete(req);

				blade_session_read_unlock(bs);
			}
		}
	}

	ks_hash_read_unlock(bh->subscriptions);

	ks_pool_free(bh->pool, &bsub_key);

	bs = blade_handle_sessions_upstream(bh);
	if (bs) {
		if (!excluded_nodeid || ks_safe_strcasecmp(blade_session_id_get(bs), excluded_nodeid)) {
			cJSON *req = NULL;
			cJSON *req_params = NULL;

			ks_log(KS_LOG_DEBUG, "Session (%s) broadcast request started\n", blade_session_id_get(bs));

			blade_rpc_request_raw_create(bh->pool, &req, &req_params, NULL, "blade.broadcast");

			cJSON_AddStringToObject(req_params, "event", event);
			cJSON_AddStringToObject(req_params, "protocol", protocol);
			cJSON_AddStringToObject(req_params, "realm", realm);

			if (params) cJSON_AddItemToObject(req_params, "params", cJSON_Duplicate(params, 1));

			blade_session_send(bs, req, callback, data);

			cJSON_Delete(req);
		}

		blade_session_read_unlock(bs);
	}
	return KS_STATUS_SUCCESS;
}

// blade.broadcast request handler
ks_bool_t blade_protocol_broadcast_request_handler(blade_rpc_request_t *brpcreq, void *data)
{
	ks_bool_t ret = KS_FALSE;
	blade_handle_t *bh = NULL;
	blade_session_t *bs = NULL;
	cJSON *req = NULL;
	cJSON *req_params = NULL;
	const char *req_params_event = NULL;
	const char *req_params_protocol = NULL;
	const char *req_params_realm = NULL;
	cJSON *req_params_params = NULL;
	const char *bsub_key = NULL;
	blade_subscription_t *bsub = NULL;
	blade_rpc_request_callback_t callback = NULL;
	cJSON *res = NULL;
	cJSON *res_result = NULL;

	ks_assert(brpcreq);

	bh = blade_rpc_request_handle_get(brpcreq);
	ks_assert(bh);

	bs = blade_handle_sessions_lookup(bh, blade_rpc_request_sessionid_get(brpcreq));
	ks_assert(bs);

	req = blade_rpc_request_message_get(brpcreq);
	ks_assert(req);

	req_params = cJSON_GetObjectItem(req, "params");
	if (!req_params) {
		ks_log(KS_LOG_DEBUG, "Session (%s) broadcast request missing 'params' object\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params object");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	req_params_event = cJSON_GetObjectCstr(req_params, "event");
	if (!req_params_event) {
		ks_log(KS_LOG_DEBUG, "Session (%s) broadcast request missing 'event'\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params event");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	req_params_protocol = cJSON_GetObjectCstr(req_params, "protocol");
	if (!req_params_protocol) {
		ks_log(KS_LOG_DEBUG, "Session (%s) broadcast request missing 'protocol'\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params protocol");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	req_params_realm = cJSON_GetObjectCstr(req_params, "realm");
	if (!req_params_realm) {
		ks_log(KS_LOG_DEBUG, "Session (%s) broadcast request missing 'realm'\n", blade_session_id_get(bs));
		blade_rpc_error_raw_create(&res, NULL, blade_rpc_request_messageid_get(brpcreq), -32602, "Missing params realm");
		blade_session_send(bs, res, NULL, NULL);
		goto done;
	}

	req_params_params = cJSON_GetObjectItem(req_params, "params");


	blade_protocol_broadcast_raw(bh, blade_session_id_get(bs), req_params_event, req_params_protocol, req_params_realm, req_params_params, NULL, NULL);


	bsub_key = ks_psprintf(bh->pool, "%s@%s/%s", req_params_protocol, req_params_realm, req_params_event);

	ks_hash_read_lock(bh->subscriptions);

	bsub = (blade_subscription_t *)ks_hash_search(bh->subscriptions, (void *)bsub_key, KS_UNLOCKED);
	if (bsub) {
		ks_rwl_read_lock(bh->local_nodeid_rwl);
		if (ks_hash_search(blade_subscription_subscribers_get(bsub), (void *)bh->local_nodeid, KS_UNLOCKED)) {
			callback = blade_subscription_callback_get(bsub);
			if (callback) ret = callback(brpcreq, blade_subscription_callback_data_get(bsub));
		}
		ks_rwl_read_unlock(bh->local_nodeid_rwl);
	}

	ks_hash_read_unlock(bh->subscriptions);

	ks_pool_free(bh->pool, &bsub_key);

	// build the actual response finally
	blade_rpc_response_raw_create(&res, &res_result, blade_rpc_request_messageid_get(brpcreq));

	cJSON_AddStringToObject(res_result, "event", req_params_event);
	cJSON_AddStringToObject(res_result, "protocol", req_params_protocol);
	cJSON_AddStringToObject(res_result, "realm", req_params_realm);

	// request was just received on a session that is already read locked, so we can assume the response goes back on the same session without further lookup
	blade_session_send(bs, res, NULL, NULL);


done:

	if (res) cJSON_Delete(res);
	if (bs) blade_session_read_unlock(bs);

	return ret;
}

KS_DECLARE(cJSON *) blade_protocol_broadcast_request_params_get(blade_rpc_request_t *brpcreq)
{
	cJSON *req = NULL;
	cJSON *req_params = NULL;
	cJSON *req_params_params = NULL;

	ks_assert(brpcreq);

	req = blade_rpc_request_message_get(brpcreq);
	ks_assert(req);

	req_params = cJSON_GetObjectItem(req, "params");
	if (req_params) req_params_params = cJSON_GetObjectItem(req_params, "params");

	return req_params_params;
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
