#include "ks_dht_nodeid.h"
#include "sodium.h"

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_nodeid_alloc(ks_dht2_nodeid_t **nodeid, ks_pool_t *pool)
{
	ks_dht2_nodeid_t *nid;

	ks_assert(nodeid);
	ks_assert(pool);
	
	*nodeid = nid = ks_pool_alloc(pool, sizeof(ks_dht2_nodeid_t));
	nid->pool = pool;

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_nodeid_free(ks_dht2_nodeid_t *nodeid)
{
	ks_assert(nodeid);
	
	ks_pool_free(nodeid->pool, nodeid);

	return KS_STATUS_SUCCESS;
}
												

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_nodeid_init(ks_dht2_nodeid_t *nodeid, const uint8_t *id)
{
	ks_assert(nodeid);

	if (!id) {
		randombytes_buf(nodeid->id, KS_DHT_NODEID_LENGTH);
	} else {
		memcpy(nodeid->id, id, KS_DHT_NODEID_LENGTH);
	}
	
	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_nodeid_deinit(ks_dht2_nodeid_t *nodeid)
{
	ks_assert(nodeid);

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
