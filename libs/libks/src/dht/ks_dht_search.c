#include "ks_dht.h"
#include "ks_dht-int.h"
#include "sodium.h"

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_search_create(ks_dht_search_t **search, ks_pool_t *pool, const ks_dht_nodeid_t *target)
{
	ks_dht_search_t *s;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(search);
	ks_assert(pool);
	ks_assert(target);

	*search = s = ks_pool_alloc(pool, sizeof(ks_dht_search_t));
	if (!s) {
		ret = KS_STATUS_NO_MEM;
		goto done;
	}
	s->pool = pool;

	if ((ret = ks_mutex_create(&s->mutex, KS_MUTEX_FLAG_DEFAULT, s->pool)) != KS_STATUS_SUCCESS) goto done;
	memcpy(s->target.id, target->id, KS_DHT_NODEID_SIZE);

	if ((ret = ks_hash_create(&s->pending,
							  KS_HASH_MODE_ARBITRARY,
							  KS_HASH_FLAG_RWLOCK,
							  s->pool)) != KS_STATUS_SUCCESS) goto done;
	ks_hash_set_keysize(s->pending, KS_DHT_NODEID_SIZE);

 done:
	if (ret != KS_STATUS_SUCCESS) {
		if (s) ks_dht_search_destroy(&s);
		*search = NULL;
	}
	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(void) ks_dht_search_destroy(ks_dht_search_t **search)
{
	ks_dht_search_t *s;
	ks_hash_iterator_t *it;

	ks_assert(search);
	ks_assert(*search);

	s = *search;

	if (s->pending) {
		for (it = ks_hash_first(s->pending, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
			ks_dht_search_pending_t *val;
			
			ks_hash_this_val(it, (void **)&val);
			ks_dht_search_pending_destroy(&val);
		}
		ks_hash_destroy(&s->pending);
	}
	if (s->callbacks) {
		ks_pool_free(s->pool, s->callbacks);
		s->callbacks = NULL;
	}
	if (s->mutex) ks_mutex_destroy(&s->mutex);

	ks_pool_free(s->pool, s);

	*search = NULL;
}

KS_DECLARE(ks_status_t) ks_dht_search_callback_add(ks_dht_search_t *search, ks_dht_search_callback_t callback)
{
	ks_assert(search);

	if (callback) {
		int32_t index;

		ks_mutex_lock(search->mutex);
		index = search->callbacks_size++;
		search->callbacks = (ks_dht_search_callback_t *)ks_pool_resize(search->pool,
																	   (void *)search->callbacks,
																	   sizeof(ks_dht_search_callback_t) * search->callbacks_size);
		if (!search->callbacks) return KS_STATUS_NO_MEM;
		search->callbacks[index] = callback;
		ks_mutex_unlock(search->mutex);
	}
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_dht_search_pending_create(ks_dht_search_pending_t **pending, ks_pool_t *pool, const ks_dht_nodeid_t *nodeid)
{
	ks_dht_search_pending_t *p;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(pending);
	ks_assert(pool);
	
	*pending = p = ks_pool_alloc(pool, sizeof(ks_dht_search_pending_t));
	if (!p) {
		ret = KS_STATUS_NO_MEM;
		goto done;
	}
	p->pool = pool;

	p->nodeid = *nodeid;
	p->expiration = ks_time_now_sec() + KS_DHT_SEARCH_EXPIRATION;
	p->finished = KS_FALSE;

 done:
	if (ret != KS_STATUS_SUCCESS) {
		if (p) ks_dht_search_pending_destroy(&p);
		*pending = NULL;
	}
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(void) ks_dht_search_pending_destroy(ks_dht_search_pending_t **pending)
{
	ks_dht_search_pending_t *p;

	ks_assert(pending);
	ks_assert(*pending);

	p = *pending;

	ks_pool_free(p->pool, p);

	*pending = NULL;
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
