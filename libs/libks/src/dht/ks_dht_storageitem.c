#include "ks_dht.h"
#include "ks_dht-int.h"
#include "sodium.h"

KS_DECLARE(ks_status_t) ks_dht_storageitem_create_immutable(ks_dht_storageitem_t **item, ks_pool_t *pool, ks_dht_nodeid_t *target, struct bencode *v)
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
	si->v = ben_clone(v);
	ks_assert(si->v);

	//enc = ben_encode(&enc_len, si->v);
	//ks_assert(enc);
	//SHA1_Init(&sha);
	//SHA1_Update(&sha, enc, enc_len);
	//SHA1_Final(si->id.id, &sha);
	//free(enc);

	// done:
	if (ret != KS_STATUS_SUCCESS) {
		if (si) ks_dht_storageitem_destroy(item);
	}
	return ret;
}

KS_DECLARE(ks_status_t) ks_dht_storageitem_create_mutable(ks_dht_storageitem_t **item,
														  ks_pool_t *pool,
														  ks_dht_nodeid_t *target,
														  struct bencode *v,
														  ks_dht_storageitem_key_t *k,
														  struct bencode *salt,
														  int64_t sequence,
														  ks_dht_storageitem_signature_t *signature)
{
	ks_dht_storageitem_t *si;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(item);
	ks_assert(pool);
	ks_assert(v);
	ks_assert(SHA_DIGEST_LENGTH == KS_DHT_NODEID_SIZE);
	ks_assert(k);
	ks_assert(signature);

	*item = si = ks_pool_alloc(pool, sizeof(ks_dht_storageitem_t));
	ks_assert(si);

	si->pool = pool;
	si->id = *target;
	si->mutable = KS_TRUE;
	si->expiration = ks_time_now() + ((ks_time_t)KS_DHT_STORAGEITEM_EXPIRATION * KS_USEC_PER_SEC);
	si->v = ben_clone(v);
	ks_assert(si->v);

	si->pk = *k;
	if (salt) {
		si->salt = ben_clone(salt);
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
	if (si->salt) {
		ben_free(si->salt);
		si->salt = NULL;
	}

	ks_pool_free(si->pool, item);
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
