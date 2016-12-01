#ifndef KS_DHT_NODEID_H
#define KS_DHT_NODEID_H

#include "ks.h"

KS_BEGIN_EXTERN_C

#define KS_DHT_NODEID_LENGTH 20

typedef struct ks_dht2_nodeid_raw_s ks_dht2_nodeid_raw_t;
struct ks_dht2_nodeid_raw_s {
    uint8_t id[KS_DHT_NODEID_LENGTH];
};

typedef struct ks_dht2_nodeid_s ks_dht2_nodeid_t;
struct ks_dht2_nodeid_s {
	ks_pool_t *pool;
    uint8_t id[KS_DHT_NODEID_LENGTH];
};

KS_DECLARE(ks_status_t) ks_dht2_nodeid_alloc(ks_dht2_nodeid_t **nodeid, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht2_nodeid_prealloc(ks_dht2_nodeid_t *nodeid, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht2_nodeid_free(ks_dht2_nodeid_t *nodeid);

KS_DECLARE(ks_status_t) ks_dht2_nodeid_init(ks_dht2_nodeid_t *nodeid, const ks_dht2_nodeid_raw_t *id);
KS_DECLARE(ks_status_t) ks_dht2_nodeid_deinit(ks_dht2_nodeid_t *nodeid);

KS_END_EXTERN_C

#endif /* KS_DHT_NODEID_H */

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

