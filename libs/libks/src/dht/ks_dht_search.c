#include "ks_dht.h"
#include "ks_dht-int.h"
#include "sodium.h"

KS_DECLARE(ks_status_t) ks_dht_search_create(ks_dht_search_t **search, ks_pool_t *pool, const ks_dht_nodeid_t *target, ks_dht_search_callback_t callback)
{
	ks_dht_search_t *s;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(search);
	ks_assert(pool);
	ks_assert(target);

	*search = s = ks_pool_alloc(pool, sizeof(ks_dht_search_t));
	ks_assert(s);

	s->pool = pool;

	ks_mutex_create(&s->mutex, KS_MUTEX_FLAG_DEFAULT, s->pool);
	ks_assert(s->mutex);

	memcpy(s->target.id, target->id, KS_DHT_NODEID_SIZE);

	s->callback = callback;

	ks_hash_create(&s->searched, KS_HASH_MODE_ARBITRARY, KS_HASH_FLAG_NOLOCK | KS_HASH_FLAG_DUP_CHECK, s->pool);
	ks_assert(s->searched);
	ks_hash_set_keysize(s->searched, KS_DHT_NODEID_SIZE);

	ks_hash_create(&s->searching, KS_HASH_MODE_ARBITRARY, KS_HASH_FLAG_NOLOCK | KS_HASH_FLAG_DUP_CHECK, s->pool);
	ks_assert(s->searching);
	ks_hash_set_keysize(s->searching, KS_DHT_NODEID_SIZE);

	// done:
	if (ret != KS_STATUS_SUCCESS) {
		if (s) ks_dht_search_destroy(search);
	}
	return ret;
}

KS_DECLARE(void) ks_dht_search_destroy(ks_dht_search_t **search)
{
	ks_dht_search_t *s;

	ks_assert(search);
	ks_assert(*search);

	s = *search;

	if (s->searching) ks_hash_destroy(&s->searching);
	if (s->searched) ks_hash_destroy(&s->searched);
	if (s->mutex) ks_mutex_destroy(&s->mutex);

	ks_pool_free(s->pool, search);
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
