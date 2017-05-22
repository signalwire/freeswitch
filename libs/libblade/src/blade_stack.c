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

	// These are for the master identity, since it has no upstream, but the realm list will also propagate through other router nodes in "blade.connect" calls
	const char *master_user;
	const char **master_realms;
	int32_t master_realms_length;

	// local identities such as upstream-session-id@mydomain.com, messages with a destination matching a key in this hash will be received and processed locally
	// @todo currently the value is unused, but may find use for it later (could store a blade_identity_t, but these are becoming less useful in the core)
	ks_hash_t *identities;

	// realms for new identities, identities get created in all of these realms, these originate from the master and may be reduced down to a single realm by
	// the master router, or by each router as it sees fit
	ks_hash_t *realms;

	// The guts of routing messages, this maps a remote identity key to a local sessionid value, sessions must also track the identities coming through them to
	// allow for removing downstream identities from these routes when they are no longer available upon session termination
	// When any node registers an identity through this node, whether it is a locally connected session or downstream through another router node, the registered
	// identity will be added to this hash, with the sessionid of the session it came through as the value
	// Any future message received and destined for identities that are not our own (see identities hash above), will use this hash for downstream relays or will
	// otherwise attempt to send upstream if it did not come from upstream
	// Messages must never back-travel through a session they were received from, thus when recieved from a downstream session, that downstream session is excluded
	// for further downstream routing scenarios to avoid any possible circular routing, message routing must be checked through downstreams before passing upstream
	ks_hash_t *routes;

	ks_hash_t *transports; // registered blade_transport_t
	blade_transport_t *default_transport; // default wss transport

	ks_hash_t *jsonrpcs; // registered blade_jsonrpc_t, for locally processing messages, keyed by the rpc method
	ks_hash_t *requests; // outgoing jsonrpc requests waiting for a response, keyed by the message id

	ks_hash_t *connections; // active connections keyed by connection id

	ks_hash_t *sessions; // active sessions keyed by session id

	ks_mutex_t *upstream_mutex; // locked when messing with upstream_id
	const char *upstream_id; // session id of the currently active upstream session

	ks_hash_t *session_state_callbacks;
};



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

	ks_hash_create(&bh->identities, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, bh->pool);
	ks_assert(bh->identities);

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

	ks_mutex_create(&bh->upstream_mutex, KS_MUTEX_FLAG_DEFAULT, bh->pool);
	ks_assert(bh->upstream_mutex);

	ks_hash_create(&bh->session_state_callbacks, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_VALUE, bh->pool);
	ks_assert(bh->session_state_callbacks);

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
	config_setting_t *master_user = NULL;
	config_setting_t *master_realms = NULL;
	const char *user = NULL;
	const char **realms = NULL;
	int32_t realms_length = 0;

	ks_assert(bh);

	if (!config) return KS_STATUS_FAIL;
	if (!config_setting_is_group(config)) {
		ks_log(KS_LOG_DEBUG, "!config_setting_is_group(config)\n");
		return KS_STATUS_FAIL;
	}

	master = config_setting_get_member(config, "master");
	if (master) {
		master_user = config_lookup_from(master, "user");
		if (master_user) {
			if (config_setting_type(master_user) != CONFIG_TYPE_STRING) return KS_STATUS_FAIL;
			user = config_setting_get_string(master_user);
		}
		master_realms = config_lookup_from(master, "realms");
		if (master_realms) {
			if (config_setting_type(master_realms) != CONFIG_TYPE_LIST) return KS_STATUS_FAIL;
			realms_length = config_setting_length(master_realms);
			if (realms_length > 0) {
				realms = ks_pool_alloc(bh->pool, sizeof(const char *) * realms_length);
				for (int32_t index = 0; index < realms_length; ++index) {
					const char *realm = config_setting_get_string_elem(master_realms, index);
					if (!realm) return KS_STATUS_FAIL;
					realms[index] = ks_pstrdup(bh->pool, realm);
				}
			}
		}
	}

	// @todo in spirit of simple config, keep the list of routers you can attempt as a client at a root level config setting "routers" using identities with transport parameters if required

	if (user && realms_length > 0) {
		bh->master_user = ks_pstrdup(bh->pool, user);
		bh->master_realms = realms;
		bh->master_realms_length = realms_length;
	}

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_startup(blade_handle_t *bh, config_setting_t *config)
{
	blade_transport_t *bt = NULL;
	ks_hash_iterator_t *it = NULL;

	ks_assert(bh);

    if (blade_handle_config(bh, config) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "blade_handle_config failed\n");
		return KS_STATUS_FAIL;
	}

	// register internals
	blade_transport_wss_create(&bt, bh);
	ks_assert(bt);
	bh->default_transport = bt;
	blade_handle_transport_register(bt);

	for (int32_t index = 0; index < bh->master_realms_length; ++index) {
		const char *realm = bh->master_realms[index];
		//char *identity = ks_pstrcat(bh->pool, bh->master_user, "@", realm); // @todo this does not work... why?
		char *identity = ks_psprintf(bh->pool, "%s@%s", bh->master_user, realm);
		
		blade_handle_identity_register(bh, identity);
		blade_handle_realm_register(bh, realm);

		ks_pool_free(bh->pool, &identity);
	}

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

KS_DECLARE(ks_status_t) blade_handle_identity_register(blade_handle_t *bh, const char *identity)
{
	char *key = NULL;

	ks_assert(bh);
	ks_assert(identity);

	key = ks_pstrdup(bh->pool, identity);
	ks_hash_insert(bh->identities, (void *)key, (void *)KS_TRUE);
	
	ks_log(KS_LOG_DEBUG, "Identity Registered: %s\n", key);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_identity_unregister(blade_handle_t *bh, const char *identity)
{
	ks_assert(bh);
	ks_assert(identity);

	ks_log(KS_LOG_DEBUG, "Identity Unregistered: %s\n", identity);

	ks_hash_remove(bh->identities, (void *)identity);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_bool_t) blade_handle_identity_local(blade_handle_t *bh, const char *identity)
{
	void *exists = NULL;

	ks_assert(bh);
	ks_assert(identity);

	exists = ks_hash_search(bh->routes, (void *)identity, KS_READLOCKED);
	ks_hash_read_unlock(bh->routes);

	return (ks_bool_t)(uintptr_t)exists == KS_TRUE;
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

KS_DECLARE(ks_status_t) blade_handle_route_add(blade_handle_t *bh, const char *identity, const char *id)
{
	char *key = NULL;
	char *value = NULL;

	ks_assert(bh);
	ks_assert(identity);
	ks_assert(id);

	key = ks_pstrdup(bh->pool, identity);
	value = ks_pstrdup(bh->pool, id);

	ks_hash_insert(bh->identities, (void *)key, (void *)id);

	ks_log(KS_LOG_DEBUG, "Route Added: %s through %s\n", key, id);

	// @todo when a route is added, upstream needs to be notified that the identity can be found through the session to the
	// upstream router, and likewise up the chain to the Master Router Node, to create a complete route from anywhere else
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_route_remove(blade_handle_t *bh, const char *identity)
{
	ks_assert(bh);
	ks_assert(identity);

	ks_log(KS_LOG_DEBUG, "Route Removed: %s\n", identity);

	ks_hash_remove(bh->identities, (void *)identity);

	// @todo when a route is removed, upstream needs to be notified, for whatever reason the identity is no longer
	// available through this node so the routes leading here need to be cleared, the disconnected node cannot be informed
	// and does not need to change it's routes because upstream is not included in routes (and thus should never call to remove
	// a route if an upstream session is closed)

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_session_t *) blade_handle_route_lookup(blade_handle_t *bh, const char *identity)
{
	blade_session_t *bs = NULL;
	const char *id = NULL;

	ks_assert(bh);
	ks_assert(identity);

	id = ks_hash_search(bh->routes, (void *)identity, KS_READLOCKED);
	if (id) bs = blade_handle_sessions_lookup(bh, id);
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

	if (bh->upstream_id) return KS_STATUS_DUPLICATE_OPERATION;

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

	ks_assert(bs);

	bh = blade_session_handle_get(bs);
	ks_assert(bh);

	blade_session_write_lock(bs, KS_TRUE);

	id = blade_session_id_get(bs);
	ks_assert(id);

	ks_hash_write_lock(bh->sessions);
	if (ks_hash_remove(bh->sessions, (void *)id) == NULL) ret = KS_STATUS_FAIL;

	ks_mutex_lock(bh->upstream_mutex);
	if (bh->upstream_id && !ks_safe_strcasecmp(bh->upstream_id, id)) {
		// the session is the upstream being terminated, so clear out all of the local identities and realms from the handle,
		// @todo this complicates any remaining connected downstream sessions, because they are based on realms that may not
		// be available after a new upstream is registered, therefore all downstream sessions should be fully terminated when
		// this happens, and ignore inbound downstream sessions until the upstream is available again, and require new
		// downstream inbound sessions to be completely reestablished fresh
		while ((it = ks_hash_first(bh->identities, KS_UNLOCKED))) {
			void *key = NULL;
			void *value = NULL;
			ks_hash_this(it, &key, NULL, &value);
			ks_hash_remove(bh->identities, key);
		}
		while ((it = ks_hash_first(bh->realms, KS_UNLOCKED))) {
			void *key = NULL;
			void *value = NULL;
			ks_hash_this(it, &key, NULL, &value);
			ks_hash_remove(bh->realms, key);
		}
		ks_pool_free(bh->pool, &bh->upstream_id);
	}
	ks_mutex_unlock(bh->upstream_mutex);

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

KS_DECLARE(ks_status_t) blade_handle_upstream_set(blade_handle_t *bh, const char *id)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(bh);

	ks_mutex_lock(bh->upstream_mutex);

	if (bh->upstream_id) {
		ret = KS_STATUS_DUPLICATE_OPERATION;
		goto done;
	}

	bh->upstream_id = ks_pstrdup(bh->pool, id);

done:

	ks_mutex_unlock(bh->upstream_mutex);

	return ret;
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
