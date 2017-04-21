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

	ks_hash_t *modules; // registered modules
	ks_hash_t *transports; // registered transports exposed by modules, NOT active connections
	ks_hash_t *spaces; // registered method spaces exposed by modules

	// registered event callback registry
	// @todo should probably use a blade_handle_event_registration_t and contain optional userdata to pass from registration back into the callback, like
	// a blade_module_t to get at inner module data for events that service modules may need to subscribe to between each other, but this may evolve into
	// an implementation based on ESL
	ks_hash_t *events;

	//blade_identity_t *identity;
	blade_datastore_t *datastore;

	ks_hash_t *connections; // active connections keyed by connection id

	ks_hash_t *sessions; // active sessions keyed by session id
	ks_hash_t *session_state_callbacks;

	ks_hash_t *requests; // outgoing requests waiting for a response keyed by the message id
};

typedef struct blade_handle_transport_registration_s blade_handle_transport_registration_t;
struct blade_handle_transport_registration_s {
	ks_pool_t *pool;

	blade_module_t *module;
	blade_transport_callbacks_t *callbacks;
};


static void blade_handle_transport_registration_cleanup(ks_pool_t *pool, void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	//blade_handle_transport_registration_t *bhtr = (blade_handle_transport_registration_t *)ptr;

	//ks_assert(bhtr);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_handle_transport_registration_create(blade_handle_transport_registration_t **bhtrP,
																   ks_pool_t *pool,
																   blade_module_t *module,
																   blade_transport_callbacks_t *callbacks)
{
	blade_handle_transport_registration_t *bhtr = NULL;

	ks_assert(bhtrP);
	ks_assert(pool);
	ks_assert(module);
	ks_assert(callbacks);

	bhtr = ks_pool_alloc(pool, sizeof(blade_handle_transport_registration_t));
	bhtr->pool = pool;
	bhtr->module = module;
	bhtr->callbacks = callbacks;

	ks_pool_set_cleanup(pool, bhtr, NULL, blade_handle_transport_registration_cleanup);

	*bhtrP = bhtr;

	return KS_STATUS_SUCCESS;
}


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
		while ((it = ks_hash_first(bh->modules, KS_UNLOCKED)) != NULL) {
			void *key = NULL;
			blade_module_t *value = NULL;

			ks_hash_this(it, (const void **)&key, NULL, (void **)&value);
			ks_hash_remove(bh->modules, key);

			blade_module_destroy(&value); // must call destroy to close the module pool, FREE_VALUE would attempt to free the module from the main handle pool used for the modules hash
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

	ks_hash_create(&bh->modules, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK, bh->pool);
	ks_assert(bh->modules);

	ks_hash_create(&bh->transports, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, bh->pool);
	ks_assert(bh->transports);

	ks_hash_create(&bh->spaces, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK, bh->pool);
	ks_assert(bh->spaces);

	ks_hash_create(&bh->events, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, bh->pool);
	ks_assert(bh->events);

	ks_hash_create(&bh->connections, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK, bh->pool);
	ks_assert(bh->connections);

	ks_hash_create(&bh->sessions, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK, bh->pool);
	ks_assert(bh->sessions);
	ks_hash_create(&bh->session_state_callbacks, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_VALUE, bh->pool);
	ks_assert(bh->session_state_callbacks);

	ks_hash_create(&bh->requests, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK, bh->pool);
	ks_assert(bh->requests);

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
	blade_handle_shutdown(bh);

	ks_pool_close(&pool);

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_handle_config(blade_handle_t *bh, config_setting_t *config)
{
	ks_assert(bh);

	if (!config) return KS_STATUS_FAIL;
    if (!config_setting_is_group(config)) return KS_STATUS_FAIL;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_startup(blade_handle_t *bh, config_setting_t *config)
{
	blade_module_t *module = NULL;
	ks_hash_iterator_t *it = NULL;

	ks_assert(bh);

	// register internal modules
	blade_module_wss_create(&module, bh);
	ks_assert(module);
	blade_handle_module_register(module);


    if (blade_handle_config(bh, config) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "blade_handle_config failed\n");
		return KS_STATUS_FAIL;
	}


	for (it = ks_hash_first(bh->modules, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		void *key = NULL;
		blade_module_t *value = NULL;
		blade_module_callbacks_t *callbacks = NULL;

		ks_hash_this(it, (const void **)&key, NULL, (void **)&value);

		callbacks = blade_module_callbacks_get(value);
		ks_assert(callbacks);

		if (callbacks->onstartup) callbacks->onstartup(value, config);
	}

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_shutdown(blade_handle_t *bh)
{
	ks_hash_iterator_t *it = NULL;

	ks_assert(bh);

	ks_hash_read_lock(bh->modules);
	for (it = ks_hash_first(bh->modules, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		void *key = NULL;
		blade_module_t *value = NULL;
		blade_module_callbacks_t *callbacks = NULL;

		ks_hash_this(it, (const void **)&key, NULL, (void **)&value);

		callbacks = blade_module_callbacks_get(value);
		ks_assert(callbacks);

		if (callbacks->onshutdown) callbacks->onshutdown(value);
	}
	ks_hash_read_unlock(bh->modules);

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

	// @todo old code, datastore will be completely revamped under the new architecture
	if (blade_handle_datastore_available(bh)) blade_datastore_destroy(&bh->datastore);

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

KS_DECLARE(ks_status_t) blade_handle_module_register(blade_module_t *bm)
{
	blade_handle_t *bh = NULL;
	const char *id = NULL;

	ks_assert(bm);

	bh = blade_module_handle_get(bm);
	ks_assert(bh);

	id = blade_module_id_get(bm);
	ks_assert(id);

	ks_hash_insert(bh->modules, (void *)id, bm);

	ks_log(KS_LOG_DEBUG, "Module Registered\n");

	return KS_STATUS_SUCCESS;
}


KS_DECLARE(ks_status_t) blade_handle_transport_register(blade_handle_t *bh, blade_module_t *bm, const char *name, blade_transport_callbacks_t *callbacks)
{
	blade_handle_transport_registration_t *bhtr = NULL;
	char *key = NULL;

	ks_assert(bh);
	ks_assert(bm);
	ks_assert(name);
	ks_assert(callbacks);

	blade_handle_transport_registration_create(&bhtr, bh->pool, bm, callbacks);
	ks_assert(bhtr);

	key = ks_pstrdup(bh->pool, name);
	ks_assert(key);

	ks_hash_insert(bh->transports, (void *)key, bhtr);

	ks_log(KS_LOG_DEBUG, "Transport Registered: %s\n", name);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_transport_unregister(blade_handle_t *bh, const char *name)
{
	ks_assert(bh);
	ks_assert(name);

	ks_log(KS_LOG_DEBUG, "Transport Unregistered: %s\n", name);

	ks_hash_remove(bh->transports, (void *)name);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_space_register(blade_space_t *bs)
{
	blade_handle_t *bh = NULL;
	const char *path = NULL;

	ks_assert(bs);

	bh = blade_space_handle_get(bs);
	ks_assert(bh);

	path = blade_space_path_get(bs);
	ks_assert(path);

	ks_hash_insert(bh->spaces, (void *)path, bs);

	ks_log(KS_LOG_DEBUG, "Space Registered: %s\n", path);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_space_unregister(blade_space_t *bs)
{
	blade_handle_t *bh = NULL;
	const char *path = NULL;

	ks_assert(bs);

	bh = blade_space_handle_get(bs);
	ks_assert(bh);

	path = blade_space_path_get(bs);
	ks_assert(path);

	ks_log(KS_LOG_DEBUG, "Space Unregistered: %s\n", path);

	ks_hash_remove(bh->spaces, (void *)path);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_space_t *) blade_handle_space_lookup(blade_handle_t *bh, const char *path)
{
	blade_space_t *bs = NULL;

	ks_assert(bh);
	ks_assert(path);

	bs = ks_hash_search(bh->spaces, (void *)path, KS_READLOCKED);
	ks_hash_read_unlock(bh->spaces);

	return bs;
}

KS_DECLARE(ks_status_t) blade_handle_event_register(blade_handle_t *bh, const char *event, blade_event_callback_t callback)
{
	char *key = NULL;

	ks_assert(bh);
	ks_assert(event);
	ks_assert(callback);

	key = ks_pstrdup(bh->pool, event);
	ks_assert(key);

	ks_hash_insert(bh->events, (void *)key, (void *)(intptr_t)callback);

	ks_log(KS_LOG_DEBUG, "Event Registered: %s\n", event);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_event_unregister(blade_handle_t *bh, const char *event)
{
	ks_assert(bh);
	ks_assert(event);

	ks_log(KS_LOG_DEBUG, "Event Unregistered: %s\n", event);

	ks_hash_remove(bh->events, (void *)event);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_event_callback_t) blade_handle_event_lookup(blade_handle_t *bh, const char *event)
{
	blade_event_callback_t callback = NULL;

	ks_assert(bh);
	ks_assert(event);

	callback = (blade_event_callback_t)(intptr_t)ks_hash_search(bh->events, (void *)event, KS_READLOCKED);
	ks_hash_read_unlock(bh->events);

	return callback;
}

KS_DECLARE(ks_status_t) blade_handle_connect(blade_handle_t *bh, blade_connection_t **bcP, blade_identity_t *target, const char *session_id)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_handle_transport_registration_t *bhtr = NULL;
	const char *tname = NULL;

	ks_assert(bh);
	ks_assert(target);

	// @todo this should take a callback, and push this to a queue to be processed async from another thread on the handle
	// which will allow the onconnect callback to block while doing things like DNS lookups without having unknown
	// impact depending on the caller thread

	ks_hash_read_lock(bh->transports);

	tname = blade_identity_parameter_get(target, "transport");
	if (tname) {
		bhtr = ks_hash_search(bh->transports, (void *)tname, KS_UNLOCKED);
		if (!bhtr) {
			// @todo error logging, target has an explicit transport that is not available in the local transports registry
			// discuss later whether this scenario should still attempt other transports when target is explicit
			// @note discussions indicate that by default messages should favor relaying through a master service, unless
			// an existing direct connection already exists to the target (which if the target is the master node, then there is
			// no conflict of proper routing). This also applies to routing for identities which relate to groups, relaying should
			// most often occur through a master service, however there may be scenarios that exist where an existing session
			// exists dedicated to faster delivery for a group (IE, through an ampq cluster directly, such as master services
			// syncing with each other through a pub/sub).  There is also the potential that instead of a separate session, the
			// current session with a master service may be able to have another connection attached which represents access through
			// amqp, which in turn acts as a preferred router for only group identities
			// This information does not directly apply to connecting, but should be noted for the next level up where you simply
			// send a message which will not actually connect, only check for existing sessions for the target and master service
			// @note relaying by master services should take a slightly different path, when they receive something not for the
			// master service itself, it should relay this on to all other master services, which in turn all including original
			// receiver pass on to any sessions matching an identity that is part of the group, alternatively they can use a pub/sub
			// like amqp to relay between the master services more efficiently than using the websocket to send every master service
			// session the message individually
		}
	} else {
		for (ks_hash_iterator_t *it = ks_hash_first(bh->transports, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
			// @todo use onrank (or replace with whatever method is used for determining what transport to use) and keep highest ranked callbacks
		}
	}
	ks_hash_read_unlock(bh->transports);

	// @todo need to be able to get to the blade_module_t from the callbacks, may require envelope around registration of callbacks to include module
	// this is required because onconnect transport callback needs to be able to get back to the module data to create the connection being returned
	if (bhtr) ret = bhtr->callbacks->onconnect(bcP, bhtr->module, target, session_id);
	else ret = KS_STATUS_FAIL;

	return ret;
}


KS_DECLARE(blade_connection_t *) blade_handle_connections_get(blade_handle_t *bh, const char *cid)
{
	blade_connection_t *bc = NULL;

	ks_assert(bh);
	ks_assert(cid);

	ks_hash_read_lock(bh->connections);
	bc = ks_hash_search(bh->connections, (void *)cid, KS_UNLOCKED);
	if (bc && blade_connection_read_lock(bc, KS_FALSE) != KS_STATUS_SUCCESS) bc = NULL;
	ks_hash_read_unlock(bh->connections);

	return bc;
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

	// @todo call bh->connection_callbacks

	return ret;
}

KS_DECLARE(blade_session_t *) blade_handle_sessions_get(blade_handle_t *bh, const char *sid)
{
	blade_session_t *bs = NULL;

	ks_assert(bh);
	ks_assert(sid);

	// @todo consider using blade_session_t via reference counting, rather than locking a mutex to simulate a reference count to halt cleanups while in use
	// using actual reference counting would mean that mutexes would not need to be held locked when looking up a session by id just to prevent cleanup,
	// instead cleanup would automatically occur when the last reference is actually removed (which SHOULD be at the end of the state machine thread),
	// which is safer than another thread potentially waiting on the write lock to release while it's being destroyed, or external code forgetting to unlock
	// then use short lived mutex or rwl for accessing the content of the session while it is referenced
	// this approach should also be used for blade_connection_t, which has a similar threaded state machine

	ks_hash_read_lock(bh->sessions);
	bs = ks_hash_search(bh->sessions, (void *)sid, KS_UNLOCKED);
	if (bs && blade_session_read_lock(bs, KS_FALSE) != KS_STATUS_SUCCESS) bs = NULL;
	ks_hash_read_unlock(bh->sessions);

	return bs;
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

	ks_assert(bs);

	bh = blade_session_handle_get(bs);
	ks_assert(bh);

	blade_session_write_lock(bs, KS_TRUE);

	ks_hash_write_lock(bh->sessions);
	if (ks_hash_remove(bh->sessions, (void *)blade_session_id_get(bs)) == NULL) ret = KS_STATUS_FAIL;
	ks_hash_write_unlock(bh->sessions);

	blade_session_write_unlock(bs);

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
		bs = blade_handle_sessions_get(bh, sessionid);
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


KS_DECLARE(blade_request_t *) blade_handle_requests_get(blade_handle_t *bh, const char *mid)
{
	blade_request_t *breq = NULL;

	ks_assert(bh);
	ks_assert(mid);

	breq = ks_hash_search(bh->requests, (void *)mid, KS_READLOCKED);
	ks_hash_read_unlock(bh->requests);

	return breq;
}

KS_DECLARE(ks_status_t) blade_handle_requests_add(blade_request_t *br)
{
	blade_handle_t *bh = NULL;

	ks_assert(br);

	bh = br->handle;
	ks_assert(bh);

	ks_hash_insert(bh->requests, (void *)br->message_id, br);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_requests_remove(blade_request_t *br)
{
	blade_handle_t *bh = NULL;

	ks_assert(br);

	bh = br->handle;
	ks_assert(bh);

	ks_hash_remove(bh->requests, (void *)br->message_id);

	return KS_STATUS_SUCCESS;
}



KS_DECLARE(ks_bool_t) blade_handle_datastore_available(blade_handle_t *bh)
{
	ks_assert(bh);

	return bh->datastore != NULL;
}

KS_DECLARE(ks_status_t) blade_handle_datastore_store(blade_handle_t *bh, const void *key, int32_t key_length, const void *data, int64_t data_length)
{
	ks_assert(bh);
	ks_assert(key);
	ks_assert(key_length > 0);
	ks_assert(data);
	ks_assert(data_length > 0);

	if (!blade_handle_datastore_available(bh)) return KS_STATUS_INACTIVE;

	return blade_datastore_store(bh->datastore, key, key_length, data, data_length);
}

KS_DECLARE(ks_status_t) blade_handle_datastore_fetch(blade_handle_t *bh,
													 blade_datastore_fetch_callback_t callback,
													 const void *key,
													 int32_t key_length,
													 void *userdata)
{
	ks_assert(bh);
	ks_assert(callback);
	ks_assert(key);
	ks_assert(key_length > 0);

	if (!blade_handle_datastore_available(bh)) return KS_STATUS_INACTIVE;

	return blade_datastore_fetch(bh->datastore, callback, key, key_length, userdata);
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
