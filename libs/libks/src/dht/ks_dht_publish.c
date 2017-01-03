#include "ks_dht.h"
#include "ks_dht-int.h"
#include "sodium.h"

KS_DECLARE(ks_status_t) ks_dht_publish_create(ks_dht_publish_t **publish,
											  ks_pool_t *pool,
											  ks_dht_job_callback_t callback,
											  void *data,
											  int64_t cas,
											  ks_dht_storageitem_t *item)
{
	ks_dht_publish_t *p;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(publish);
	ks_assert(pool);
	ks_assert(cas >= 0);
	ks_assert(item);

	*publish = p = ks_pool_alloc(pool, sizeof(ks_dht_publish_t));
	ks_assert(p);

	p->pool = pool;

	p->callback = callback;
	p->data = data;
	p->cas = cas;
	p->item = item;
	
	ks_dht_storageitem_reference(p->item);

	// done:
	if (ret != KS_STATUS_SUCCESS) {
		if (p) ks_dht_publish_destroy(publish);
	}
	return ret;
}

KS_DECLARE(void) ks_dht_publish_destroy(ks_dht_publish_t **publish)
{
	ks_dht_publish_t *p;

	ks_assert(publish);
	ks_assert(*publish);

	p = *publish;

	ks_dht_storageitem_dereference(p->item);
	
	ks_pool_free(p->pool, publish);
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
