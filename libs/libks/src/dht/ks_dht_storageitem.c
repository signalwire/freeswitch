#include "ks_dht.h"
#include "ks_dht-int.h"
#include "sodium.h"

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_storageitem_alloc(ks_dht_storageitem_t **item, ks_pool_t *pool)
{
	ks_dht_storageitem_t *si;

	ks_assert(item);
	ks_assert(pool);

	*item = si = ks_pool_alloc(pool, sizeof(ks_dht_storageitem_t));
	si->pool = pool;

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_storageitem_prealloc(ks_dht_storageitem_t *item, ks_pool_t *pool)
{
	ks_assert(item);
	ks_assert(pool);

	memset(item, 0, sizeof(ks_dht_storageitem_t));

	item->pool = pool;

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_storageitem_free(ks_dht_storageitem_t **item)
{
	ks_assert(item);
	ks_assert(*item);

	ks_dht_storageitem_deinit(*item);
	ks_pool_free((*item)->pool, *item);

	*item = NULL;

	return KS_STATUS_SUCCESS;
}


/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_storageitem_init(ks_dht_storageitem_t *item, struct bencode *v)
{
	ks_assert(item);
	ks_assert(item->pool);
	ks_assert(v);
	ks_assert(SHA_DIGEST_LENGTH == KS_DHT_NODEID_SIZE);

	item->v = ben_clone(v);

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_storageitem_deinit(ks_dht_storageitem_t *item)
{
	ks_assert(item);

	if (item->v) {
		ben_free(item->v);
		item->v = NULL;
	}

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_storageitem_create(ks_dht_storageitem_t *item, ks_bool_t mutable)
{
	SHA_CTX sha;

	ks_assert(item);
	ks_assert(item->pool);
	ks_assert(item->v);
	
	item->mutable = mutable;

	if (!mutable) {
		size_t enc_len = 0;
		uint8_t *enc = ben_encode(&enc_len, item->v);
		SHA1_Init(&sha);
		SHA1_Update(&sha, enc, enc_len);
		SHA1_Final(item->id.id, &sha);
		free(enc);
	} else {
		size_t enc_len = 0;
		uint8_t *enc = NULL;
		struct bencode *sig = ben_dict();
		
		crypto_sign_keypair(item->pk.key, item->sk.key);
		randombytes_buf(item->salt, KS_DHT_STORAGEITEM_SALT_MAX_SIZE);
		item->salt_length = KS_DHT_STORAGEITEM_SALT_MAX_SIZE;
		item->seq = 1;

		ben_dict_set(sig, ben_blob("salt", 4), ben_blob(item->salt, item->salt_length));
		ben_dict_set(sig, ben_blob("seq", 3), ben_int(item->seq));
		ben_dict_set(sig, ben_blob("v", 1), ben_clone(item->v));
		enc = ben_encode(&enc_len, sig);
		ben_free(sig);

		SHA1_Init(&sha);
		SHA1_Update(&sha, enc, enc_len);
		SHA1_Final(item->sig.sig, &sha);

		free(enc);

		SHA1_Init(&sha);
		SHA1_Update(&sha, item->pk.key, KS_DHT_STORAGEITEM_KEY_SIZE);
		SHA1_Update(&sha, item->salt, item->salt_length);
		SHA1_Final(item->id.id, &sha);
	}

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_storageitem_immutable(ks_dht_storageitem_t *item)
{
	SHA_CTX sha;
	size_t enc_len = 0;
	uint8_t *enc = NULL;
	
	ks_assert(item);
	ks_assert(item->v);

	item->mutable = KS_FALSE;
	
	enc = ben_encode(&enc_len, item->v);
	SHA1_Init(&sha);
	SHA1_Update(&sha, enc, enc_len);
	SHA1_Final(item->id.id, &sha);
	free(enc);

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_storageitem_mutable(ks_dht_storageitem_t *item,
												   ks_dht_storageitem_key_t *k,
												   uint8_t *salt,
												   ks_size_t salt_length,
												   int64_t sequence,
												   ks_dht_storageitem_signature_t *signature)
{
	SHA_CTX sha;
	
	ks_assert(item);
	ks_assert(item->v);
	ks_assert(k);
	ks_assert(!(!salt && salt_length > 0));
	ks_assert(salt_length > KS_DHT_STORAGEITEM_SIGNATURE_SIZE);
	ks_assert(signature);

	item->mutable = KS_TRUE;

	memcpy(item->pk.key, k->key, KS_DHT_STORAGEITEM_KEY_SIZE);
	if (salt && salt_length > 0) {
		memcpy(item->salt, salt, salt_length);
		item->salt_length = salt_length;
	}
	item->seq = sequence;
	memcpy(item->sig.sig, signature->sig, KS_DHT_STORAGEITEM_SIGNATURE_SIZE);

	SHA1_Init(&sha);
	SHA1_Update(&sha, item->pk.key, KS_DHT_STORAGEITEM_KEY_SIZE);
	if (item->salt && item->salt_length > 0) SHA1_Update(&sha, item->salt, item->salt_length);
	SHA1_Final(item->id.id, &sha);

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
