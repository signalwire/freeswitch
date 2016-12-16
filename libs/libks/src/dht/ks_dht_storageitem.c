#include "ks_dht.h"
#include "ks_dht-int.h"
#include "sodium.h"

KS_DECLARE(ks_status_t) ks_dht_storageitem_create_immutable(ks_dht_storageitem_t **item, ks_pool_t *pool, struct bencode *v)
{
	ks_dht_storageitem_t *si;
	SHA_CTX sha;
	size_t enc_len = 0;
	uint8_t *enc = NULL;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(item);
	ks_assert(pool);
	ks_assert(v);
	ks_assert(SHA_DIGEST_LENGTH == KS_DHT_NODEID_SIZE);

	*item = si = ks_pool_alloc(pool, sizeof(ks_dht_storageitem_t));
	ks_assert(si);

	si->pool = pool;
	si->mutable = KS_FALSE;
	si->v = ben_clone(v);
	ks_assert(si->v);
	
	enc = ben_encode(&enc_len, si->v);
	ks_assert(enc);
	SHA1_Init(&sha);
	SHA1_Update(&sha, enc, enc_len);
	SHA1_Final(si->id.id, &sha);
	free(enc);

	// done:
	if (ret != KS_STATUS_SUCCESS) {
		if (si) ks_dht_storageitem_destroy(&si);
		*item = NULL;
	}
	return ret;
}

KS_DECLARE(ks_status_t) ks_dht_storageitem_create_mutable(ks_dht_storageitem_t **item,
														  ks_pool_t *pool,
														  struct bencode *v,
														  ks_dht_storageitem_key_t *k,
														  uint8_t *salt,
														  ks_size_t salt_length,
														  int64_t sequence,
														  ks_dht_storageitem_signature_t *signature)
{
	ks_dht_storageitem_t *si;
	SHA_CTX sha;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(item);
	ks_assert(pool);
	ks_assert(v);
	ks_assert(SHA_DIGEST_LENGTH == KS_DHT_NODEID_SIZE);
	ks_assert(k);
	ks_assert(!(!salt && salt_length > 0));
	ks_assert(!(salt_length > KS_DHT_STORAGEITEM_SIGNATURE_SIZE));
	ks_assert(signature);

	*item = si = ks_pool_alloc(pool, sizeof(ks_dht_storageitem_t));
	ks_assert(si);

	si->pool = pool;
	si->mutable = KS_TRUE;
	si->v = ben_clone(v);
	ks_assert(si->v);

	memcpy(si->pk.key, k->key, KS_DHT_STORAGEITEM_KEY_SIZE);
	if (salt && salt_length > 0) {
		memcpy(si->salt, salt, salt_length);
		si->salt_length = salt_length;
	}
	si->seq = sequence;
	memcpy(si->sig.sig, signature->sig, KS_DHT_STORAGEITEM_SIGNATURE_SIZE);

	SHA1_Init(&sha);
	SHA1_Update(&sha, si->pk.key, KS_DHT_STORAGEITEM_KEY_SIZE);
	if (si->salt && si->salt_length > 0) SHA1_Update(&sha, si->salt, si->salt_length);
	SHA1_Final(si->id.id, &sha);

	// done:
	if (ret != KS_STATUS_SUCCESS) {
		if (si) ks_dht_storageitem_destroy(&si);
		*item = NULL;
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
	ks_pool_free(si->pool, si);

	*item = NULL;
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
