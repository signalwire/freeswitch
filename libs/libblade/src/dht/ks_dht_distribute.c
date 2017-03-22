#include "ks_dht.h"
#include "ks_dht-int.h"
#include "sodium.h"

KS_DECLARE(ks_status_t) ks_dht_distribute_create(ks_dht_distribute_t **distribute,
												 ks_pool_t *pool,
												 ks_dht_storageitem_callback_t callback,
												 void *data,
												 int64_t cas,
												 ks_dht_storageitem_t *item)
{
	ks_dht_distribute_t *d;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(distribute);
	ks_assert(pool);
	ks_assert(cas >= 0);
	ks_assert(item);

	*distribute = d = ks_pool_alloc(pool, sizeof(ks_dht_distribute_t));
	ks_assert(d);

	d->pool = pool;

	d->callback = callback;
	d->data = data;
	ks_mutex_create(&d->mutex, KS_MUTEX_FLAG_DEFAULT, d->pool);
	ks_assert(d->mutex);
	d->cas = cas;
	d->item = item;
	
	ks_dht_storageitem_reference(d->item);

	// done:
	if (ret != KS_STATUS_SUCCESS) {
		if (d) ks_dht_distribute_destroy(distribute);
	}
	return ret;
}

KS_DECLARE(void) ks_dht_distribute_destroy(ks_dht_distribute_t **distribute)
{
	ks_dht_distribute_t *d;

	ks_assert(distribute);
	ks_assert(*distribute);

	d = *distribute;

	if (d->mutex) ks_mutex_destroy(&d->mutex);
	ks_dht_storageitem_dereference(d->item);
	
	ks_pool_free(d->pool, distribute);
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
