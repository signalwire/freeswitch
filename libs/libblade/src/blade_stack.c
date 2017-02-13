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
	BH_MYPOOL = (1 << 0),
	BH_MYTPOOL = (1 << 1)
} bhpvt_flag_t;

struct blade_handle_s {
	bhpvt_flag_t flags;
	ks_pool_t *pool;
	ks_thread_pool_t *tpool;
	
	config_setting_t *config_service;
	config_setting_t *config_datastore;

	ks_hash_t *transports;
	ks_q_t *messages_discarded;

	blade_datastore_t *datastore;
};


KS_DECLARE(ks_status_t) blade_handle_destroy(blade_handle_t **bhP)
{
	blade_handle_t *bh = NULL;
	bhpvt_flag_t flags;
	ks_pool_t *pool;

	ks_assert(bhP);

	bh = *bhP;
	*bhP = NULL;

	ks_assert(bh);

	flags = bh->flags;
	pool = bh->pool;

	blade_handle_shutdown(bh);

	if (bh->messages_discarded) {
		// @todo make sure messages are cleaned up
		ks_q_destroy(&bh->messages_discarded);
	}

	ks_hash_destroy(&bh->transports);

    if (bh->tpool && (flags & BH_MYTPOOL)) ks_thread_pool_destroy(&bh->tpool);

	ks_pool_free(bh->pool, &bh);

	if (pool && (flags & BH_MYPOOL)) {
		ks_pool_close(&pool);
	}

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_create(blade_handle_t **bhP, ks_pool_t *pool, ks_thread_pool_t *tpool)
{
	bhpvt_flag_t newflags = BH_NONE;
	blade_handle_t *bh = NULL;

	ks_assert(bhP);

	if (!pool) {
		newflags |= BH_MYPOOL;
		ks_pool_open(&pool);
	}
    if (!tpool) {
		newflags |= BH_MYTPOOL;
		ks_thread_pool_create(&tpool, BLADE_HANDLE_TPOOL_MIN, BLADE_HANDLE_TPOOL_MAX, BLADE_HANDLE_TPOOL_STACK, KS_PRI_NORMAL, BLADE_HANDLE_TPOOL_IDLE);
		ks_assert(tpool);
	}
	
	bh = ks_pool_alloc(pool, sizeof(*bh));
	bh->flags = newflags;
	bh->pool = pool;
	bh->tpool = tpool;

	ks_hash_create(&bh->transports, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_NOLOCK | KS_HASH_FLAG_DUP_CHECK, bh->pool);
	ks_assert(bh->transports);
	
	// @todo check thresholds from config, for now just ensure it doesn't grow out of control, allow 100 discarded messages
	ks_q_create(&bh->messages_discarded, bh->pool, 100);
	ks_assert(bh->messages_discarded);

	*bhP = bh;

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_handle_config(blade_handle_t *bh, config_setting_t *config)
{
	config_setting_t *service = NULL;
	config_setting_t *datastore = NULL;
	
	ks_assert(bh);

	if (!config) return KS_STATUS_FAIL;
    if (!config_setting_is_group(config)) return KS_STATUS_FAIL;

	// @todo config for messages_discarded threshold (ie, message count, message memory, etc)

    service = config_setting_get_member(config, "service");
	
    datastore = config_setting_get_member(config, "datastore");
    //if (datastore && !config_setting_is_group(datastore)) return KS_STATUS_FAIL;


	bh->config_service = service;
	bh->config_datastore = datastore;
	
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_startup(blade_handle_t *bh, config_setting_t *config)
{
	ks_assert(bh);

    if (blade_handle_config(bh, config) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "blade_handle_config failed\n");
		return KS_STATUS_FAIL;
	}

	if (bh->config_datastore && !blade_handle_datastore_available(bh)) {
		blade_datastore_create(&bh->datastore, bh->pool, bh->tpool);
		ks_assert(bh->datastore);
		if (blade_datastore_startup(bh->datastore, bh->config_datastore) != KS_STATUS_SUCCESS) {
			ks_log(KS_LOG_DEBUG, "blade_datastore_startup failed\n");
			return KS_STATUS_FAIL;
		}
	}
	
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_shutdown(blade_handle_t *bh)
{
	ks_assert(bh);

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

KS_DECLARE(ks_status_t) blade_handle_transport_register(blade_handle_t *bh, blade_transport_callbacks_t *callbacks)
{
	ks_assert(bh);
	ks_assert(callbacks);

	ks_hash_write_lock(bh->transports);
	ks_hash_insert(bh->transports, (void *)callbacks->name, callbacks);
	ks_hash_write_unlock(bh->transports);

	ks_log(KS_LOG_DEBUG, "Transport Registered: %s\n", callbacks->name);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_transport_unregister(blade_handle_t *bh, blade_transport_callbacks_t *callbacks)
{
	ks_assert(bh);
	ks_assert(callbacks);

	ks_hash_write_lock(bh->transports);
	ks_hash_remove(bh->transports, (void *)callbacks->name);
	ks_hash_write_unlock(bh->transports);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_connect(blade_handle_t *bh, blade_connection_t **bcP, blade_identity_t *target)
{
	ks_assert(bh);
	ks_assert(target);

	ks_hash_read_lock(bh->transports);
	// @todo find transport for target, check if target specifies explicit transport parameter first, otherwise use onrank and keep highest ranked callbacks
	ks_hash_read_unlock(bh->transports);

	// transport_callbacks->onconnect(bcP, target);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_message_claim(blade_handle_t *bh, blade_message_t **message, void *data, ks_size_t data_length)
{
	blade_message_t *msg = NULL;
	
	ks_assert(bh);
	ks_assert(message);
	ks_assert(data);

	*message = NULL;
	
	if (ks_q_trypop(bh->messages_discarded, (void **)&msg) != KS_STATUS_SUCCESS || !msg) blade_message_create(&msg, bh->pool, bh);
	ks_assert(msg);

	if (blade_message_set(msg, data, data_length) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
	
	*message = msg;
		
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_message_discard(blade_handle_t *bh, blade_message_t **message)
{
	ks_assert(bh);
	ks_assert(message);
	ks_assert(*message);

	// @todo check thresholds for discarded messages, if the queue is full just destroy the message for now (currently 100 messages)
	if (ks_q_push(bh->messages_discarded, *message) != KS_STATUS_SUCCESS) blade_message_destroy(message);

	*message = NULL;

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
