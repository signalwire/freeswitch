#include "ks_dht.h"
#include "ks_dht-int.h"
#include "sodium.h"

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_search_alloc(ks_dht_search_t **search, ks_pool_t *pool)
{
	ks_dht_search_t *s;

	ks_assert(search);
	ks_assert(pool);
	
	*search = s = ks_pool_alloc(pool, sizeof(ks_dht_search_t));
	s->pool = pool;

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(void) ks_dht_search_prealloc(ks_dht_search_t *search, ks_pool_t *pool)
{
	ks_assert(search);
	ks_assert(pool);

	memset(search, 0, sizeof(ks_dht_search_t));
	
	search->pool = pool;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_search_free(ks_dht_search_t **search)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	
	ks_assert(search);
	ks_assert(*search);

	if ((ret = ks_dht_search_deinit(*search)) != KS_STATUS_SUCCESS) return ret;
	if ((ret = ks_pool_free((*search)->pool, *search)) != KS_STATUS_SUCCESS) return ret;

	*search = NULL;

	return KS_STATUS_SUCCESS;
}


/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_search_init(ks_dht_search_t *search, const ks_dht_nodeid_t *target)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(search);
	ks_assert(search->pool);
	ks_assert(target);

	if ((ret = ks_mutex_create(&search->mutex, KS_MUTEX_FLAG_DEFAULT, search->pool)) != KS_STATUS_SUCCESS) return ret;
	memcpy(search->target.id, target->id, KS_DHT_NODEID_SIZE);

	if ((ret = ks_hash_create(&search->pending,
							  KS_HASH_MODE_ARBITRARY,
							  KS_HASH_FLAG_RWLOCK,
							  search->pool)) != KS_STATUS_SUCCESS) return ret;
	ks_hash_set_keysize(search->pending, KS_DHT_NODEID_SIZE);

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_search_deinit(ks_dht_search_t *search)
{
	ks_hash_iterator_t *it;
	ks_status_t ret = KS_STATUS_SUCCESS;
	
	ks_assert(search);

	search->results_length = 0;
	if (search->pending) {
		for (it = ks_hash_first(search->pending, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
			const void *key;
			ks_dht_search_pending_t *val;
			
			ks_hash_this(it, &key, NULL, (void **)&val);
			if ((ret = ks_dht_search_pending_deinit(val)) != KS_STATUS_SUCCESS) return ret;
			if ((ret = ks_dht_search_pending_free(&val)) != KS_STATUS_SUCCESS) return ret;
		}
		ks_hash_destroy(&search->pending);
	}
	search->callbacks_size = 0;
	if (search->callbacks) {
		if ((ret = ks_pool_free(search->pool, search->callbacks)) != KS_STATUS_SUCCESS) return ret;
		search->callbacks = NULL;
	}
	if (search->mutex && (ret = ks_mutex_destroy(&search->mutex)) != KS_STATUS_SUCCESS) return ret;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_dht_search_callback_add(ks_dht_search_t *search, ks_dht_search_callback_t callback)
{
	ks_assert(search);

	if (callback) {
		int32_t index;
		// @todo lock mutex
		index = search->callbacks_size++;
		search->callbacks = (ks_dht_search_callback_t *)ks_pool_resize(search->pool,
																	   (void *)search->callbacks,
																	   sizeof(ks_dht_search_callback_t) * search->callbacks_size);
		search->callbacks[index] = callback;
		// @todo unlock mutex
	}
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_dht_search_pending_alloc(ks_dht_search_pending_t **pending, ks_pool_t *pool)
{
	ks_dht_search_pending_t *p;

	ks_assert(pending);
	ks_assert(pool);
	
	*pending = p = ks_pool_alloc(pool, sizeof(ks_dht_search_pending_t));
	p->pool = pool;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(void) ks_dht_search_pending_prealloc(ks_dht_search_pending_t *pending, ks_pool_t *pool)
{
	ks_assert(pending);
	ks_assert(pool);

	memset(pending, 0, sizeof(ks_dht_search_pending_t));
	
	pending->pool = pool;
}

KS_DECLARE(ks_status_t) ks_dht_search_pending_free(ks_dht_search_pending_t **pending)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	
	ks_assert(pending);
	ks_assert(*pending);

	if ((ret = ks_dht_search_pending_deinit(*pending)) != KS_STATUS_SUCCESS) return ret;
	if ((ret = ks_pool_free((*pending)->pool, *pending)) != KS_STATUS_SUCCESS) return ret;

	*pending = NULL;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_dht_search_pending_init(ks_dht_search_pending_t *pending, ks_dht_node_t *node)
{
	ks_assert(pending);
	ks_assert(pending->pool);
	ks_assert(node);

	pending->node = node;
	pending->expiration = ks_time_now_sec() + KS_DHT_SEARCH_EXPIRATION;
	pending->finished = KS_FALSE;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_dht_search_pending_deinit(ks_dht_search_pending_t *pending)
{
	ks_assert(pending);

	pending->node = NULL;
	pending->expiration = 0;
	pending->finished = KS_FALSE;
	
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
