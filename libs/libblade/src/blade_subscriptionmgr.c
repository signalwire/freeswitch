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

struct blade_subscriptionmgr_s {
	blade_handle_t *handle;

	ks_hash_t *subscriptions; // key, blade_subscription_t*
	ks_hash_t *subscriptions_cleanup; // target, ks_hash_t*

};


static void blade_subscriptionmgr_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	//blade_routemgr_t *brmgr = (blade_routemgr_t *)ptr;

	//ks_assert(brmgr);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_subscriptionmgr_create(blade_subscriptionmgr_t **bsmgrP, blade_handle_t *bh)
{
	ks_pool_t *pool = NULL;
	blade_subscriptionmgr_t *bsmgr = NULL;

	ks_assert(bsmgrP);

	ks_pool_open(&pool);
	ks_assert(pool);

	bsmgr = ks_pool_alloc(pool, sizeof(blade_subscriptionmgr_t));
	bsmgr->handle = bh;

	// @note can let removes free keys and values for subscriptions, both are allocated from the same pool as the hash itself
	ks_hash_create(&bsmgr->subscriptions, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, pool);
	ks_assert(bsmgr->subscriptions);

	ks_hash_create(&bsmgr->subscriptions_cleanup, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, pool);
	ks_assert(bsmgr->subscriptions_cleanup);

	ks_pool_set_cleanup(bsmgr, NULL, blade_subscriptionmgr_cleanup);

	*bsmgrP = bsmgr;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_subscriptionmgr_destroy(blade_subscriptionmgr_t **bsmgrP)
{
	blade_subscriptionmgr_t *bsmgr = NULL;
	ks_pool_t *pool;

	ks_assert(bsmgrP);
	ks_assert(*bsmgrP);

	bsmgr = *bsmgrP;
	*bsmgrP = NULL;

	pool = ks_pool_get(bsmgr);

	ks_pool_close(&pool);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_handle_t *) blade_subscriptionmgr_handle_get(blade_subscriptionmgr_t *bsmgr)
{
	ks_assert(bsmgr);
	return bsmgr->handle;
}

//KS_DECLARE(blade_session_t *) blade_subscriptionmgr_route_lookup(blade_routemgr_t *brmgr, const char *target)
//{
//	blade_session_t *bs = NULL;
//	const char *router = NULL;
//
//	ks_assert(brmgr);
//	ks_assert(target);
//
//	router = (const char *)ks_hash_search(brmgr->routes, (void *)target, KS_READLOCKED);
//	if (router) bs = blade_handle_sessions_lookup(brmgr->handle, router);
//	ks_hash_read_unlock(brmgr->routes);
//
//	return bs;
//}

KS_DECLARE(blade_subscription_t *) blade_subscriptionmgr_subscription_lookup(blade_subscriptionmgr_t *bsmgr, const char *protocol, const char *channel)
{
	blade_subscription_t *bsub = NULL;
	char *key = NULL;

	ks_assert(bsmgr);
	ks_assert(protocol);
	ks_assert(channel);

	key = ks_psprintf(ks_pool_get(bsmgr), "%s:%s", protocol, channel);

	bsub = (blade_subscription_t *)ks_hash_search(bsmgr->subscriptions, (void *)key, KS_READLOCKED);
	// @todo if (bsub) blade_subscription_read_lock(bsub);
	ks_hash_read_unlock(bsmgr->subscriptions);

	ks_pool_free(&key);

	return bsub;
}

KS_DECLARE(ks_status_t) blade_subscriptionmgr_subscription_remove(blade_subscriptionmgr_t *bsmgr, const char *protocol, const char *channel)
{
	ks_pool_t *pool = NULL;
	char *bsub_key = NULL;
	blade_subscription_t *bsub = NULL;
	ks_hash_t *subscribers = NULL;
	ks_hash_t *subscriptions = NULL;

	ks_assert(bsmgr);
	ks_assert(protocol);
	ks_assert(channel);

	pool = ks_pool_get(bsmgr);

	bsub_key = ks_psprintf(pool, "%s:%s", protocol, channel);

	ks_hash_write_lock(bsmgr->subscriptions);

	bsub = (blade_subscription_t *)ks_hash_search(bsmgr->subscriptions, (void *)bsub_key, KS_UNLOCKED);

	subscribers = blade_subscription_subscribers_get(bsub);
	ks_assert(subscribers);

	for (ks_hash_iterator_t *it = ks_hash_first(subscribers, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		void *key = NULL;
		void *value = NULL;

		ks_hash_this(it, (const void **)&key, NULL, &value);

		subscriptions = (ks_hash_t *)ks_hash_search(bsmgr->subscriptions_cleanup, key, KS_UNLOCKED);

		ks_log(KS_LOG_DEBUG, "Subscriber Removed: %s from protocol %s, channel %s\n", key, protocol, channel);
		ks_hash_remove(subscriptions, bsub_key);

		if (ks_hash_count(subscriptions) == 0) {
			ks_hash_remove(bsmgr->subscriptions_cleanup, key);
		}
	}

	ks_log(KS_LOG_DEBUG, "Subscription Removed: %s from %s\n", channel, protocol);
	ks_hash_remove(bsmgr->subscriptions, (void *)bsub_key);

	ks_hash_write_unlock(bsmgr->subscriptions);

	ks_pool_free(&bsub_key);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_bool_t) blade_subscriptionmgr_subscriber_add(blade_subscriptionmgr_t *bsmgr, blade_subscription_t **bsubP, const char *protocol, const char *channel, const char *subscriber)
{
	ks_pool_t *pool = NULL;
	char *key = NULL;
	blade_subscription_t *bsub = NULL;
	ks_hash_t *bsub_cleanup = NULL;
	ks_bool_t propagate = KS_FALSE;

	ks_assert(bsmgr);
	ks_assert(protocol);
	ks_assert(channel);
	ks_assert(subscriber);

	pool = ks_pool_get(bsmgr);

	key = ks_psprintf(pool, "%s:%s", protocol, channel);

	ks_hash_write_lock(bsmgr->subscriptions);

	bsub = (blade_subscription_t *)ks_hash_search(bsmgr->subscriptions, (void *)key, KS_UNLOCKED);

	if (!bsub) {
		blade_subscription_create(&bsub, pool, protocol, channel);
		ks_assert(bsub);

		ks_hash_insert(bsmgr->subscriptions, (void *)ks_pstrdup(pool, key), (void *)bsub);
		propagate = KS_TRUE;
	}

	bsub_cleanup = (ks_hash_t *)ks_hash_search(bsmgr->subscriptions_cleanup, (void *)subscriber, KS_UNLOCKED);
	if (!bsub_cleanup) {
		ks_hash_create(&bsub_cleanup, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, pool);
		ks_assert(bsub_cleanup);

		ks_log(KS_LOG_DEBUG, "Subscription Added: %s to %s\n", channel, protocol);
		ks_hash_insert(bsmgr->subscriptions_cleanup, (void *)ks_pstrdup(pool, subscriber), (void *)bsub_cleanup);
	}
	ks_hash_insert(bsub_cleanup, (void *)ks_pstrdup(pool, key), (void *)KS_TRUE);

	blade_subscription_subscribers_add(bsub, subscriber);

	ks_hash_write_unlock(bsmgr->subscriptions);

	ks_log(KS_LOG_DEBUG, "Subscriber Added: %s to protocol %s, channel %s\n", subscriber, protocol, channel);

	ks_pool_free(&key);

	if (bsubP) *bsubP = bsub;

	return propagate;
}

KS_DECLARE(ks_bool_t) blade_subscriptionmgr_subscriber_remove(blade_subscriptionmgr_t *bsmgr, blade_subscription_t **bsubP, const char *protocol, const char *channel, const char *subscriber)
{
	char *key = NULL;
	blade_subscription_t *bsub = NULL;
	ks_hash_t *bsub_cleanup = NULL;
	ks_bool_t propagate = KS_FALSE;

	ks_assert(bsmgr);
	ks_assert(protocol);
	ks_assert(channel);
	ks_assert(subscriber);

	key = ks_psprintf(ks_pool_get(bsmgr), "%s:%s", protocol, channel);

	ks_hash_write_lock(bsmgr->subscriptions);

	bsub = (blade_subscription_t *)ks_hash_search(bsmgr->subscriptions, (void *)key, KS_UNLOCKED);

	if (bsub) {
		bsub_cleanup = (ks_hash_t *)ks_hash_search(bsmgr->subscriptions_cleanup, (void *)subscriber, KS_UNLOCKED);
		if (bsub_cleanup) {
			ks_hash_remove(bsub_cleanup, key);

			if (ks_hash_count(bsub_cleanup) == 0) {
				ks_hash_remove(bsmgr->subscriptions_cleanup, (void *)subscriber);
			}

			ks_log(KS_LOG_DEBUG, "Subscriber Removed: %s from protocol %s, channel %s\n", subscriber, protocol, channel);
			blade_subscription_subscribers_remove(bsub, subscriber);

			if (ks_hash_count(blade_subscription_subscribers_get(bsub)) == 0) {
				ks_log(KS_LOG_DEBUG, "Subscription Removed: %s from %s\n", channel, protocol);
				ks_hash_remove(bsmgr->subscriptions, (void *)key);
				propagate = KS_TRUE;
			}
		}
	}

	ks_hash_write_unlock(bsmgr->subscriptions);

	ks_pool_free(&key);

	if (bsubP) *bsubP = bsub;

	return propagate;
}

KS_DECLARE(void) blade_subscriptionmgr_purge(blade_subscriptionmgr_t *bsmgr, const char *target)
{
	ks_pool_t *pool = NULL;
	ks_bool_t unsubbed = KS_FALSE;

	ks_assert(bsmgr);
	ks_assert(target);

	pool = ks_pool_get(bsmgr);

	while (!unsubbed) {
		ks_hash_t *subscriptions = NULL;
		const char *protocol = NULL;
		const char *channel = NULL;

		ks_hash_read_lock(bsmgr->subscriptions);
		subscriptions = (ks_hash_t *)ks_hash_search(bsmgr->subscriptions_cleanup, (void *)target, KS_UNLOCKED);
		if (!subscriptions) unsubbed = KS_TRUE;
		else {
			void *key = NULL;
			void *value = NULL;
			blade_subscription_t *bsub = NULL;

			ks_hash_iterator_t *it = ks_hash_first(subscriptions, KS_UNLOCKED);
			ks_assert(it);

			ks_hash_this(it, (const void **)&key, NULL, &value);

			bsub = (blade_subscription_t *)ks_hash_search(bsmgr->subscriptions, key, KS_UNLOCKED);
			ks_assert(bsub);

			// @note allocate these to avoid lifecycle issues when the last subscriber is removed causing the subscription to be removed
			protocol = ks_pstrdup(pool, blade_subscription_protocol_get(bsub));
			channel = ks_pstrdup(pool, blade_subscription_channel_get(bsub));
		}
		ks_hash_read_unlock(bsmgr->subscriptions);

		if (!unsubbed) {
			blade_subscriptionmgr_subscriber_remove(bsmgr, NULL, protocol, channel, target);

			ks_pool_free(&protocol);
			ks_pool_free(&channel);
		}
	}
}

KS_DECLARE(ks_status_t) blade_subscriptionmgr_broadcast(blade_subscriptionmgr_t *bsmgr, blade_rpcbroadcast_command_t command, const char *excluded_sessionid, const char *protocol, const char *channel, const char *event, cJSON *params, blade_rpc_response_callback_t callback, void *data)
{
	ks_pool_t *pool = NULL;
	const char *bsub_key = NULL;
	blade_subscription_t *bsub = NULL;
	blade_session_t *bs = NULL;
	cJSON *req = NULL;
	cJSON *req_params = NULL;
	ks_hash_t *routers = NULL;
	ks_hash_t *channels = NULL;

	ks_assert(bsmgr);
	ks_assert(protocol);

	pool = ks_pool_get(bsmgr);

	switch (command) {
	case BLADE_RPCBROADCAST_COMMAND_EVENT:
		ks_assert(channel);
		ks_assert(event);
	case BLADE_RPCBROADCAST_COMMAND_CHANNEL_REMOVE:
		ks_assert(channel);
		bsub_key = ks_psprintf(pool, "%s:%s", protocol, channel);

		ks_hash_read_lock(bsmgr->subscriptions);

		bsub = (blade_subscription_t *)ks_hash_search(bsmgr->subscriptions, (void *)bsub_key, KS_UNLOCKED);
		if (bsub) {
			ks_hash_t *subscribers = blade_subscription_subscribers_get(bsub);

			ks_assert(subscribers);

			for (ks_hash_iterator_t *it = ks_hash_first(subscribers, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
				void *key = NULL;
				void *value = NULL;

				ks_hash_this(it, (const void **)&key, NULL, &value);

				//if (excluded_nodeid && !ks_safe_strcasecmp(excluded_nodeid, (const char *)key)) continue;

				//if (blade_routemgr_local_check(blade_handle_routemgr_get(bsmgr->handle), (const char *)key)) continue;

				bs = blade_routemgr_route_lookup(blade_handle_routemgr_get(bsmgr->handle), (const char *)key);
				if (bs) {
					if (excluded_sessionid && !ks_safe_strcasecmp(excluded_sessionid, blade_session_id_get(bs))) continue;
					if (!routers) ks_hash_create(&routers, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_NOLOCK | KS_HASH_FLAG_DUP_CHECK, pool);
					if (!ks_hash_search(routers, (void *)blade_session_id_get(bs), KS_UNLOCKED)) ks_hash_insert(routers, (void *)blade_session_id_get(bs), (void *)bs);
					else blade_session_read_unlock(bs);
				}
			}
			if (command == BLADE_RPCBROADCAST_COMMAND_CHANNEL_REMOVE) {
				if (!channels) ks_hash_create(&channels, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_NOLOCK | KS_HASH_FLAG_DUP_CHECK, pool);
				ks_hash_insert(channels, (void *)channel, (void *)KS_TRUE);
			}
		}

		ks_hash_read_unlock(bsmgr->subscriptions);
		break;
	case BLADE_RPCBROADCAST_COMMAND_PROTOCOL_REMOVE:
		bsub_key = ks_pstrdup(pool, protocol);

		ks_hash_read_lock(bsmgr->subscriptions);

		for (ks_hash_iterator_t *it = ks_hash_first(bsmgr->subscriptions, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
			void *key = NULL;
			void *value = NULL;

			ks_hash_this(it, (const void **)&key, NULL, &value);

			bsub = (blade_subscription_t *)value;

			if (ks_stristr(bsub_key, (const char *)key) == (const char *)key) {
				ks_hash_t *subscribers = blade_subscription_subscribers_get(bsub);

				ks_assert(subscribers);

				for (ks_hash_iterator_t *it2 = ks_hash_first(subscribers, KS_UNLOCKED); it2; it2 = ks_hash_next(&it2)) {
					void *key2 = NULL;
					void *value2 = NULL;

					ks_hash_this(it2, (const void **)&key2, NULL, &value2);

					//if (excluded_nodeid && !ks_safe_strcasecmp(excluded_nodeid, (const char *)key2)) continue;

					//if (blade_routemgr_local_check(blade_handle_routemgr_get(bsmgr->handle), (const char *)key2)) continue;

					bs = blade_routemgr_route_lookup(blade_handle_routemgr_get(bsmgr->handle), (const char *)key2);
					if (bs) {
						if (excluded_sessionid && !ks_safe_strcasecmp(excluded_sessionid, blade_session_id_get(bs))) continue;
						if (!routers) ks_hash_create(&routers, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_NOLOCK | KS_HASH_FLAG_DUP_CHECK, pool);
						if (!ks_hash_search(routers, (void *)blade_session_id_get(bs), KS_UNLOCKED)) ks_hash_insert(routers, (void *)blade_session_id_get(bs), (void *)bs);
						else blade_session_read_unlock(bs);
					}
				}

				if (!channels) ks_hash_create(&channels, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_NOLOCK | KS_HASH_FLAG_DUP_CHECK, pool);
				ks_hash_insert(channels, (void *)blade_subscription_channel_get(bsub), (void *)KS_TRUE);
			}
		}

		ks_hash_read_unlock(bsmgr->subscriptions);
		break;
	default: return KS_STATUS_ARG_INVALID;
	}


	bs = blade_sessionmgr_upstream_lookup(blade_handle_sessionmgr_get(bsmgr->handle));
	if (bs) {
		if (!excluded_sessionid || ks_safe_strcasecmp(excluded_sessionid, blade_session_id_get(bs))) {
			if (!routers) ks_hash_create(&routers, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_NOLOCK | KS_HASH_FLAG_DUP_CHECK, pool);
			ks_hash_insert(routers, (void *)blade_session_id_get(bs), (void *)bs);
		}
		else blade_session_read_unlock(bs);
	}

	if (channels) {
		for (ks_hash_iterator_t *it = ks_hash_first(channels, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
			void *key = NULL;
			void *value = NULL;

			ks_hash_this(it, (const void **)&key, NULL, &value);

			blade_subscriptionmgr_subscription_remove(bsmgr, protocol, (const char *)key);
		}
		ks_hash_destroy(&channels);
	}

	if (routers) {
		for (ks_hash_iterator_t *it = ks_hash_first(routers, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
			void *key = NULL;
			void *value = NULL;

			ks_hash_this(it, (const void **)&key, NULL, &value);

			bs = (blade_session_t *)value;

			blade_rpc_request_raw_create(pool, &req, &req_params, NULL, "blade.broadcast");
			cJSON_AddNumberToObject(req_params, "command", command);
			cJSON_AddStringToObject(req_params, "protocol", protocol);
			if (channel) cJSON_AddStringToObject(req_params, "channel", channel);
			if (event) cJSON_AddStringToObject(req_params, "event", event);
			if (params) cJSON_AddItemToObject(req_params, "params", cJSON_Duplicate(params, 1));

			ks_log(KS_LOG_DEBUG, "Broadcasting: protocol %s, channel %s through %s\n", protocol, channel, blade_session_id_get(bs));

			blade_session_send(bs, req, callback, data);

			cJSON_Delete(req);

			blade_session_read_unlock(bs);
		}
		ks_hash_destroy(&routers);
	}

	ks_pool_free(&bsub_key);

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
