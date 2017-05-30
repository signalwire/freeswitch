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

	ks_hash_t *jsonrpcs; // registered blade_jsonrpc_t, for locally processing messages, keyed by the rpc method
	ks_hash_t *requests; // outgoing jsonrpc requests waiting for a response, keyed by the message id

	ks_hash_t *connections; // active connections keyed by connection id

	ks_hash_t *sessions; // active sessions keyed by session id (which comes from the nodeid of the downstream side of the session, thus an upstream session is keyed under the local_nodeid)

	ks_hash_t *session_state_callbacks;

	// @note everything below this point is exclusively for the master node

	// @todo need to track the details from blade.publish, a protocol may be published under multiple realms, and each protocol published to a realm may have multiple target providers
	// @todo how does "exclusive" play into the providers, does "exclusive" mean only one provider can exist for a given protocol and realm?
	// for now, ignore exclusive and multiple providers, key by "protocol" in a hash, and use a blade_protocol_t to represent a protocol in the context of being published so it can be located by other nodes
	// each blade_protocol_t will contain the "protocol", common method/namespace/schema data, and a hash keyed by the "realm", with a value of an object of type blade_protocol_realm_t
	// each blade_protocol_realm_t will contain the "realm" and a list of publisher nodeid's, any of which can be chosen at random to use the protocol within the given realm (does "exclusive" only limit this to 1 provider per realm?)
	// @todo protocols must be cleaned up when routes are removed due to session terminations, should incorporate a faster way to lookup which protocols are tied to a given nodeid for efficient removal
	// create blade_protocol_method_t to represent a method that is executed with blade.execute, and is part of a protocol made available through blade.publish, registered locally by the protocol and method name (protocol.methodname?),
	// with a callback handler which should also have the realm available when executed so a single provider can easily provide a protocol for multiple realms with the same method callbacks
	ks_hash_t *protocols; // master only: protocols that have been published with blade.publish, and the details to locate a protocol provider with blade.locate
	ks_hash_t *protocols_cleanup; // master only: keyed by the nodeid, each value should be a list_t* of which contains string values matching the "protocol@realm" keys to remove each nodeid from as a provider during cleanup

};


ks_bool_t blade_protocol_publish_request_handler(blade_jsonrpc_request_t *breq, void *data);
ks_bool_t blade_protocol_publish_response_handler(blade_jsonrpc_response_t *bres);


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
		while ((it = ks_hash_first(bh->jsonrpcs, KS_UNLOCKED)) != NULL) {
			void *key = NULL;
			blade_jsonrpc_t *value = NULL;

			ks_hash_this(it, (const void **)&key, NULL, (void **)&value);
			ks_hash_remove(bh->jsonrpcs, key);

			blade_jsonrpc_destroy(&value); // must call destroy to close the jsonrpc pool, using FREE_VALUE on the hash would attempt to free the jsonrpc from the wrong pool
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

	ks_hash_create(&bh->jsonrpcs, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, bh->pool);
	ks_assert(bh->jsonrpcs);

	ks_hash_create(&bh->requests, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, bh->pool);
	ks_assert(bh->requests);

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
	blade_jsonrpc_t *bjsonrpc = NULL;
	blade_transport_t *bt = NULL;
	ks_hash_iterator_t *it = NULL;

	ks_assert(bh);

    if (blade_handle_config(bh, config) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "blade_handle_config failed\n");
		return KS_STATUS_FAIL;
	}

	// register internals
	blade_jsonrpc_create(&bjsonrpc, bh, "blade.publish", blade_protocol_publish_request_handler, NULL);
	blade_handle_jsonrpc_register(bjsonrpc);

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

	if (bh->master_nodeid) ks_pool_free(bh->pool, &bh->local_nodeid);
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

	// @todo when a route is added, upstream needs to be notified that the identity can be found through the session to the
	// upstream router, and likewise up the chain to the Master Router Node, to create a complete route from anywhere else
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_route_remove(blade_handle_t *bh, const char *nodeid)
{
	ks_hash_t *protocols = NULL;

	ks_assert(bh);
	ks_assert(nodeid);

	ks_log(KS_LOG_DEBUG, "Route Removed: %s\n", nodeid);

	ks_hash_remove(bh->routes, (void *)nodeid);

	// @todo when a route is removed, upstream needs to be notified, for whatever reason the node is no longer available through
	// this node so the routes leading here need to be cleared by passing a "blade.route" upstream to remove the routes, this
	// should actually happen only for local sessions, and blade.route should be always passed upstream AND processed locally, so
	// we don't want to duplicate blade.route calls already being passed up if this route is not a local session

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


KS_DECLARE(ks_status_t) blade_handle_jsonrpc_register(blade_jsonrpc_t *bjsonrpc)
{
	blade_handle_t *bh = NULL;
	char *key = NULL;

	ks_assert(bjsonrpc);

	bh = blade_jsonrpc_handle_get(bjsonrpc);
	ks_assert(bh);

	key = ks_pstrdup(bh->pool, blade_jsonrpc_method_get(bjsonrpc));
	ks_assert(key);

	ks_hash_insert(bh->jsonrpcs, (void *)key, bjsonrpc);

	ks_log(KS_LOG_DEBUG, "JSONRPC Registered: %s\n", key);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_jsonrpc_unregister(blade_jsonrpc_t *bjsonrpc)
{
	blade_handle_t *bh = NULL;
	const char *method = NULL;

	ks_assert(bjsonrpc);

	bh = blade_jsonrpc_handle_get(bjsonrpc);
	ks_assert(bh);

	method = blade_jsonrpc_method_get(bjsonrpc);
	ks_assert(method);

	ks_log(KS_LOG_DEBUG, "JSONRPC Unregistered: %s\n", method);

	ks_hash_remove(bh->jsonrpcs, (void *)method);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_jsonrpc_t *) blade_handle_jsonrpc_lookup(blade_handle_t *bh, const char *method)
{
	blade_jsonrpc_t *bjsonrpc = NULL;

	ks_assert(bh);
	ks_assert(method);

	bjsonrpc = ks_hash_search(bh->jsonrpcs, (void *)method, KS_READLOCKED);
	ks_hash_read_unlock(bh->jsonrpcs);

	return bjsonrpc;
}


KS_DECLARE(ks_status_t) blade_handle_requests_add(blade_jsonrpc_request_t *bjsonrpcreq)
{
	blade_handle_t *bh = NULL;
	const char *key = NULL;

	ks_assert(bjsonrpcreq);

	bh = blade_jsonrpc_request_handle_get(bjsonrpcreq);
	ks_assert(bh);

	key = ks_pstrdup(bh->pool, blade_jsonrpc_request_messageid_get(bjsonrpcreq));
	ks_assert(key);

	ks_hash_insert(bh->requests, (void *)key, bjsonrpcreq);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_requests_remove(blade_jsonrpc_request_t *bjsonrpcreq)
{
	blade_handle_t *bh = NULL;
	const char *id = NULL;

	ks_assert(bjsonrpcreq);

	bh = blade_jsonrpc_request_handle_get(bjsonrpcreq);
	ks_assert(bh);

	id = blade_jsonrpc_request_messageid_get(bjsonrpcreq);
	ks_assert(id);

	ks_hash_remove(bh->requests, (void *)id);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_jsonrpc_request_t *) blade_handle_requests_lookup(blade_handle_t *bh, const char *id)
{
	blade_jsonrpc_request_t *bjsonrpcreq = NULL;

	ks_assert(bh);
	ks_assert(id);

	bjsonrpcreq = ks_hash_search(bh->requests, (void *)id, KS_READLOCKED);
	ks_hash_read_unlock(bh->requests);

	return bjsonrpcreq;
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
	const char *id = NULL;
	ks_hash_iterator_t *it = NULL;
	ks_bool_t upstream = KS_FALSE;

	ks_assert(bs);

	bh = blade_session_handle_get(bs);
	ks_assert(bh);

	blade_session_write_lock(bs, KS_TRUE);

	id = blade_session_id_get(bs);
	ks_assert(id);

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

	// @todo consider using blade_session_t via reference counting, rather than locking a mutex to simulate a reference count to halt cleanups while in use
	// using actual reference counting would mean that mutexes would not need to be held locked when looking up a session by id just to prevent cleanup,
	// instead cleanup would automatically occur when the last reference is actually removed (which SHOULD be at the end of the state machine thread),
	// which is safer than another thread potentially waiting on the write lock to release while it's being destroyed, or external code forgetting to unlock
	// then use short lived mutex or rwl for accessing the content of the session while it is referenced
	// this approach should also be used for blade_connection_t, which has a similar threaded state machine

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
	bs = blade_handle_sessions_lookup(bh, bh->local_nodeid);
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
		blade_session_send(bs, json, NULL);
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
// This is where the real work happens for the blade protocol, where routing is done based on the specific intent of the given message, these exist here to simplify
// access to the internals of the blade_handle_t where all the relevant data is stored
// Each jsonrpc method for the blade protocol will require 3 functions: a request generator, a request handler, and a response handler
// Responses can be generated internally and are not required for an isolated entry point, in the case of further external layers like blade.execute, they will be
// handled within the blade protocol handlers to dispatch further execution callbacks, however each jsonrpc exposed to support the blade protocols may deal with
// routing in their own ways as they have different requirements for different blade layer messages.


// blade.publish notes
// This jsonrpc is used to notify the master of a new protocol being made available, the purpose of which is to make such protocols able to be located by other nodes with
// only minimal information about the protocol, particularly it's registered name, which is most often the main/only namespace for the protocols methods, however it is
// possible that additional namespaces could be included in this publish as well if the namespaces are defined separately from the protocol name, and the protocol name could
// result in an implicitly created namespace in addition to any others provided.
// Routing Notes:
// When routing a publish request, it only needs to travel upstream to the master node for processing, however in order to receive a publish response the original request
// and response must carry a nodeid for the requesting node (requester-nodeid), technically the master does not need to be provided, but for posterity and consistency
// the master nodeid can be provided in whatever is used for the responder of a request (responder-nodeid).
// By using requester-nodeid and responder-nodeid, these do not need to be swapped in the response, they can simply be copied over, and the routing looks at the
// appropriate field depending on whether it is handling a request or a response to determine the appropriate downstream nodeid


// blade.publish request generator
// @todo add additional async callback to be called upon a publish response to inform caller of the result?
KS_DECLARE(ks_status_t) blade_protocol_publish(blade_handle_t *bh, const char *name, const char *realm)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	cJSON *req = NULL;
	cJSON *req_params = NULL;
	blade_session_t *bs = NULL;

	ks_assert(bh);
	ks_assert(name);
	ks_assert(realm);

	if (!(bs = blade_handle_sessions_upstream(bh))) {
		ret = KS_STATUS_DISCONNECTED;
		goto done;
	}

	blade_jsonrpc_request_raw_create(blade_handle_pool_get(bh), &req, &req_params, NULL, "blade.publish");

	// fill in the req_params
	cJSON_AddStringToObject(req_params, "protocol", name);
	cJSON_AddStringToObject(req_params, "realm", realm);

	ks_rwl_read_lock(bh->local_nodeid_rwl);
	cJSON_AddStringToObject(req_params, "requester-nodeid", bh->local_nodeid);
	ks_rwl_read_unlock(bh->local_nodeid_rwl);

	ks_rwl_read_lock(bh->master_nodeid_rwl);
	cJSON_AddStringToObject(req_params, "responder-nodeid", bh->master_nodeid);
	ks_rwl_read_unlock(bh->master_nodeid_rwl);

	ks_log(KS_LOG_DEBUG, "Session (%s) publish request started\n", blade_session_id_get(bs));
	ret = blade_session_send(bs, req, blade_protocol_publish_response_handler);

done:
	if (req) cJSON_Delete(req);
	if (bs) blade_session_read_unlock(bs);

	return ret;
}

// blade.publish request handler
ks_bool_t blade_protocol_publish_request_handler(blade_jsonrpc_request_t *breq, void *data)
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

	ks_assert(breq);

	bh = blade_jsonrpc_request_handle_get(breq);
	ks_assert(bh);

	bs = blade_handle_sessions_lookup(bh, blade_jsonrpc_request_sessionid_get(breq));
	ks_assert(bs);

	req = blade_jsonrpc_request_message_get(breq);
	ks_assert(req);

	req_params = cJSON_GetObjectItem(req, "params");
	if (!req_params) {
		ks_log(KS_LOG_DEBUG, "Session (%s) publish request missing 'params' object\n", blade_session_id_get(bs));
		blade_jsonrpc_error_raw_create(&res, NULL, blade_jsonrpc_request_messageid_get(breq), -32602, "Missing params object");
		blade_session_send(bs, res, NULL);
		goto done;
	}

	req_params_protocol = cJSON_GetObjectCstr(req_params, "protocol");
	if (!req_params_protocol) {
		ks_log(KS_LOG_DEBUG, "Session (%s) publish request missing 'protocol'\n", blade_session_id_get(bs));
		blade_jsonrpc_error_raw_create(&res, NULL, blade_jsonrpc_request_messageid_get(breq), -32602, "Missing params protocol");
		blade_session_send(bs, res, NULL);
		goto done;
	}

	req_params_realm = cJSON_GetObjectCstr(req_params, "realm");
	if (!req_params_realm) {
		ks_log(KS_LOG_DEBUG, "Session (%s) publish request missing 'realm'\n", blade_session_id_get(bs));
		blade_jsonrpc_error_raw_create(&res, NULL, blade_jsonrpc_request_messageid_get(breq), -32602, "Missing params realm");
		blade_session_send(bs, res, NULL);
		goto done;
	}

	// @todo confirm the realm is permitted for the session, this gets complicated with subdomains, skipping for now

	req_params_requester_nodeid = cJSON_GetObjectCstr(req_params, "requester-nodeid");
	if (!req_params_requester_nodeid) {
		ks_log(KS_LOG_DEBUG, "Session (%s) publish request missing 'requester-nodeid'\n", blade_session_id_get(bs));
		blade_jsonrpc_error_raw_create(&res, NULL, blade_jsonrpc_request_messageid_get(breq), -32602, "Missing params requester-nodeid");
		blade_session_send(bs, res, NULL);
		goto done;
	}

	req_params_responder_nodeid = cJSON_GetObjectCstr(req_params, "responder-nodeid");
	if (!req_params_responder_nodeid) {
		ks_log(KS_LOG_DEBUG, "Session (%s) publish request missing 'responder-nodeid'\n", blade_session_id_get(bs));
		blade_jsonrpc_error_raw_create(&res, NULL, blade_jsonrpc_request_messageid_get(breq), -32602, "Missing params responder-nodeid");
		blade_session_send(bs, res, NULL);
		goto done;
	}

	if (!blade_handle_master_nodeid_compare(bh, req_params_responder_nodeid)) {
		ks_log(KS_LOG_DEBUG, "Session (%s) publish request invalid 'responder-nodeid' (%s)\n", blade_session_id_get(bs), req_params_responder_nodeid);
		blade_jsonrpc_error_raw_create(&res, NULL, blade_jsonrpc_request_messageid_get(breq), -32602, "Invalid params responder-nodeid");
		blade_session_send(bs, res, NULL);
		goto done;
	}

	// errors sent above this point are meant to be handled by the first node which receives the request, should not occur after the first node validates
	// errors (and the response) sent after this point must include the requester-nodeid and responder-nodeid for proper routing

	if (!blade_handle_local_nodeid_compare(bh, req_params_responder_nodeid)) {
		// not meant for local processing, continue with routing which on a publish request, it always goes upstream to the master node
		blade_session_t *bsu = blade_handle_sessions_upstream(bh);
		if (!bsu) {
			cJSON *res_error = NULL;

			ks_log(KS_LOG_DEBUG, "Session (%s) publish request (%s to %s) but upstream session unavailable\n", blade_session_id_get(bs), req_params_requester_nodeid, req_params_responder_nodeid);
			blade_jsonrpc_error_raw_create(&res, &res_error, blade_jsonrpc_request_messageid_get(breq), -32603, "Upstream session unavailable");

			// needed in case this error must propagate further than the session which sent it
			cJSON_AddStringToObject(res_error, "requester-nodeid", req_params_requester_nodeid);
			cJSON_AddStringToObject(res_error, "responder-nodeid", req_params_responder_nodeid); // @todo responder-nodeid should become the local_nodeid to inform of which node actually responded

			blade_session_send(bs, res, NULL);
			goto done;
		}

		// @todo this creates a new request that is tracked locally, in order to receive the response in a callback to route it correctly, this could be simplified
		// by using a couple special fields to indicate common routing approaches based on a routing block in common for every message, thus being able to bypass this
		// and still be able to properly route responses without a specific response handler on every intermediate router, in which case messages that are only being
		// routed would not enter into these handlers and would not leave a footprint passing through routers
		ks_log(KS_LOG_DEBUG, "Session (%s) publish request (%s to %s) routing upstream (%s)\n", blade_session_id_get(bs), req_params_requester_nodeid, req_params_responder_nodeid, blade_session_id_get(bsu));
		blade_session_send(bsu, req, blade_protocol_publish_response_handler);
		blade_session_read_unlock(bsu);

		goto done;
	}

	// this local node must be responder-nodeid for the request, so process the request
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
	blade_jsonrpc_response_raw_create(&res, &res_result, blade_jsonrpc_request_messageid_get(breq));

	cJSON_AddStringToObject(res_result, "protocol", req_params_protocol);
	cJSON_AddStringToObject(res_result, "requester-nodeid", req_params_requester_nodeid);
	cJSON_AddStringToObject(res_result, "responder-nodeid", req_params_responder_nodeid);

	// request was just received on a session that is already read locked, so we can assume the response goes back on the same session without further lookup
	blade_session_send(bs, res, NULL);

done:

	if (res) cJSON_Delete(res);
	if (bs) blade_session_read_unlock(bs);

	return KS_FALSE;
}

// blade.publish response handler
ks_bool_t blade_protocol_publish_response_handler(blade_jsonrpc_response_t *bres)
{
	blade_handle_t *bh = NULL;
	blade_session_t *bs = NULL;
	cJSON *res = NULL;
	cJSON *res_error = NULL;
	cJSON *res_result = NULL;
	cJSON *res_object = NULL;
	const char *requester_nodeid = NULL;
	const char *responder_nodeid = NULL;

	ks_assert(bres);

	bh = blade_jsonrpc_response_handle_get(bres);
	ks_assert(bh);

	bs = blade_handle_sessions_lookup(bh, blade_jsonrpc_response_sessionid_get(bres));
	ks_assert(bs);

	res = blade_jsonrpc_response_message_get(bres);
	ks_assert(res);

	res_error = cJSON_GetObjectItem(res, "error");
	res_result = cJSON_GetObjectItem(res, "result");

	if (!res_error && !res_result) {
		ks_log(KS_LOG_DEBUG, "Session (%s) publish response missing 'error' or 'result' object\n", blade_session_id_get(bs));
		goto done;
	}
	res_object = res_error ? res_error : res_result;

	requester_nodeid = cJSON_GetObjectCstr(res_object, "requester-nodeid");
	responder_nodeid = cJSON_GetObjectCstr(res_object, "responder-nodeid");

	if (requester_nodeid && responder_nodeid && !blade_handle_local_nodeid_compare(bh, requester_nodeid)) {
		blade_session_t *bsd = blade_handle_sessions_lookup(bh, requester_nodeid);
		if (!bsd) {
			ks_log(KS_LOG_DEBUG, "Session (%s) publish response (%s to %s) but downstream session unavailable\n", blade_session_id_get(bs), requester_nodeid, responder_nodeid);
			goto done;
		}

		ks_log(KS_LOG_DEBUG, "Session (%s) publish response (%s to %s) routing downstream (%s)\n", blade_session_id_get(bs), requester_nodeid, responder_nodeid, blade_session_id_get(bsd));
		blade_session_send(bsd, res, NULL);
		blade_session_read_unlock(bsd);

		goto done;
	}

	// this local node must be requester-nodeid for the response, or the response lacks routing nodeids, so process the response
	ks_log(KS_LOG_DEBUG, "Session (%s) publish response processing\n", blade_session_id_get(bs));

	if (res_error) {
		// @todo process error response
		ks_log(KS_LOG_DEBUG, "Session (%s) publish response error... add details\n", blade_session_id_get(bs));
		goto done;
	}

	// @todo process result response

done:
	if (bs) blade_session_read_unlock(bs);

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
