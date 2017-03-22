#include "ks_dht.h"
#include "ks_dht-int.h"
#include "sodium.h"

KS_DECLARE(ks_status_t) ks_dht_storageitem_create_immutable_internal(ks_dht_storageitem_t **item,
																	 ks_pool_t *pool,
																	 ks_dht_nodeid_t *target,
																	 struct bencode *v,
																	 ks_bool_t clone_v)
{
	ks_dht_storageitem_t *si;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(item);
	ks_assert(pool);
	ks_assert(v);
	ks_assert(SHA_DIGEST_LENGTH == KS_DHT_NODEID_SIZE);

	*item = si = ks_pool_alloc(pool, sizeof(ks_dht_storageitem_t));
	ks_assert(si);

	si->pool = pool;
	si->id = *target;
	si->mutable = KS_FALSE;
	si->expiration = ks_time_now() + ((ks_time_t)KS_DHT_STORAGEITEM_EXPIRATION * KS_USEC_PER_SEC);
	si->keepalive = ks_time_now() + ((ks_time_t)KS_DHT_STORAGEITEM_KEEPALIVE * KS_USEC_PER_SEC);
	si->v = clone_v ? ben_clone(v) : v;
	ks_assert(si->v);

	si->refc = 1;

	// done:
	if (ret != KS_STATUS_SUCCESS) {
		if (si) ks_dht_storageitem_destroy(item);
	}
	return ret;
}

KS_DECLARE(ks_status_t) ks_dht_storageitem_create_immutable(ks_dht_storageitem_t **item,
															ks_pool_t *pool,
															ks_dht_nodeid_t *target,
															const uint8_t *value,
															ks_size_t value_length)
{
	struct bencode *v = NULL;

	ks_assert(item);
	ks_assert(pool);
	ks_assert(value);
	ks_assert(value_length > 0);
	ks_assert(SHA_DIGEST_LENGTH == KS_DHT_NODEID_SIZE);

	v = ben_blob(value, value_length);
	ks_assert(v);
	
	return ks_dht_storageitem_create_immutable_internal(item, pool, target, v, KS_FALSE);
}

KS_DECLARE(ks_status_t) ks_dht_storageitem_create_mutable_internal(ks_dht_storageitem_t **item,
																   ks_pool_t *pool,
																   ks_dht_nodeid_t *target,
																   struct bencode *v,
																   ks_bool_t clone_v,
																   ks_dht_storageitem_pkey_t *pk,
																   struct bencode *salt,
																   ks_bool_t clone_salt,
																   int64_t sequence,
																   ks_dht_storageitem_signature_t *signature)
{
	ks_dht_storageitem_t *si;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(item);
	ks_assert(pool);
	ks_assert(v);
	ks_assert(SHA_DIGEST_LENGTH == KS_DHT_NODEID_SIZE);
	ks_assert(pk);
	ks_assert(signature);

	*item = si = ks_pool_alloc(pool, sizeof(ks_dht_storageitem_t));
	ks_assert(si);

	si->pool = pool;
	si->id = *target;
	si->mutable = KS_TRUE;
	si->expiration = ks_time_now() + ((ks_time_t)KS_DHT_STORAGEITEM_EXPIRATION * KS_USEC_PER_SEC);
	si->keepalive = ks_time_now() + ((ks_time_t)KS_DHT_STORAGEITEM_KEEPALIVE * KS_USEC_PER_SEC);
	si->v = clone_v ? ben_clone(v) : v;
	ks_assert(si->v);

	si->refc = 1;

	ks_mutex_create(&si->mutex, KS_MUTEX_FLAG_DEFAULT, si->pool);
	ks_assert(si->mutex);
	
	si->pk = *pk;
	if (salt) {
		si->salt = clone_salt ? ben_clone(salt) : salt;
		ks_assert(si->salt);
	}
	si->seq = sequence;
	si->sig = *signature;

	// done:
	if (ret != KS_STATUS_SUCCESS) {
		if (si) ks_dht_storageitem_destroy(item);
	}
	return ret;
}

KS_DECLARE(ks_status_t) ks_dht_storageitem_create_mutable(ks_dht_storageitem_t **item,
														  ks_pool_t *pool,
														  ks_dht_nodeid_t *target,
														  const uint8_t *value,
														  ks_size_t value_length,
														  ks_dht_storageitem_pkey_t *pk,
														  const uint8_t *salt,
														  ks_size_t salt_length,
														  int64_t sequence,
														  ks_dht_storageitem_signature_t *signature)
{
	struct bencode *v = NULL;
	struct bencode *s = NULL;

	ks_assert(item);
	ks_assert(pool);
	ks_assert(value);
	ks_assert(value_length > 0);
	ks_assert(SHA_DIGEST_LENGTH == KS_DHT_NODEID_SIZE);
	ks_assert(pk);
	ks_assert(signature);

	v = ben_blob(value, value_length);
	if (salt && salt_length > 0) s = ben_blob(salt, salt_length);
	return ks_dht_storageitem_create_mutable_internal(item, pool, target, v, KS_FALSE, pk, s, KS_FALSE, sequence, signature);
}

KS_DECLARE(void) ks_dht_storageitem_update_mutable(ks_dht_storageitem_t *item, struct bencode *v, int64_t sequence, ks_dht_storageitem_signature_t *signature)
{
	ks_assert(item);
	ks_assert(v);
	ks_assert(sequence);
	ks_assert(signature);

	ks_mutex_lock(item->mutex);
	ben_free(item->v);
	item->v = ben_clone(v);
	item->seq = sequence;
	item->sig = *signature;
	ks_mutex_unlock(item->mutex);
}

/**
 *
 */
KS_DECLARE(void) ks_dht_storageitem_destroy(ks_dht_storageitem_t **item)
{
	ks_dht_storageitem_t *si;
	
	ks_assert(item);
	ks_assert(*item);

	si = *item;

	if (si->v) {
		ben_free(si->v);
		si->v = NULL;
	}
	if (si->mutex) ks_mutex_destroy(&si->mutex);
	if (si->salt) {
		ben_free(si->salt);
		si->salt = NULL;
	}

	ks_pool_free(si->pool, item);
}

KS_DECLARE(void) ks_dht_storageitem_reference(ks_dht_storageitem_t *item)
{
	ks_assert(item);

	ks_mutex_lock(item->mutex);
	item->refc++;
	ks_mutex_unlock(item->mutex);
}

KS_DECLARE(void) ks_dht_storageitem_dereference(ks_dht_storageitem_t *item)
{
	ks_assert(item);

	ks_mutex_lock(item->mutex);
	item->refc--;
	ks_mutex_unlock(item->mutex);

	ks_assert(item->refc >= 0);
}

KS_DECLARE(void) ks_dht_storageitem_callback(ks_dht_storageitem_t *item, ks_dht_storageitem_callback_t callback)
{
	ks_assert(item);

	item->callback = callback;
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
