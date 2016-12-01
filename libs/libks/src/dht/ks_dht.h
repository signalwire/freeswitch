#ifndef KS_DHT_H
#define KS_DHT_H

#include "ks.h"
#include "ks_bencode.h"

#include "ks_dht_endpoint.h"
#include "ks_dht_message.h"
#include "ks_dht_nodeid.h"

KS_BEGIN_EXTERN_C


#define KS_DHT_DEFAULT_PORT 5309
#define KS_DHT_RECV_BUFFER_SIZE 0xFFFF

typedef struct ks_dht2_s ks_dht2_t;
struct ks_dht2_s {
	ks_pool_t *pool;
	ks_bool_t pool_alloc;

	ks_dht2_nodeid_t nodeid;

	ks_hash_t *registry_type;
	ks_hash_t *registry_query;

	ks_bool_t bind_ipv4;
	ks_bool_t bind_ipv6;

	ks_dht2_endpoint_t **endpoints;
	int32_t endpoints_size;
	ks_hash_t *endpoints_hash;
	struct pollfd *endpoints_poll;

	uint8_t recv_buffer[KS_DHT_RECV_BUFFER_SIZE];
	ks_size_t recv_buffer_length;
};

typedef ks_status_t (*ks_dht2_registry_callback_t)(ks_dht2_t *dht, ks_sockaddr_t *raddr, ks_dht2_message_t *message);


KS_DECLARE(ks_status_t) ks_dht2_alloc(ks_dht2_t **dht, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht2_prealloc(ks_dht2_t *dht, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht2_free(ks_dht2_t *dht);


KS_DECLARE(ks_status_t) ks_dht2_init(ks_dht2_t *dht, const ks_dht2_nodeid_raw_t *nodeid);
KS_DECLARE(ks_status_t) ks_dht2_deinit(ks_dht2_t *dht);


KS_DECLARE(ks_status_t) ks_dht2_bind(ks_dht2_t *dht, const ks_sockaddr_t *addr);
KS_DECLARE(ks_status_t) ks_dht2_pulse(ks_dht2_t *dht, int32_t timeout);


KS_DECLARE(ks_status_t) ks_dht2_register_type(ks_dht2_t *dht, const char *value, ks_dht2_registry_callback_t callback);
KS_DECLARE(ks_status_t) ks_dht2_register_query(ks_dht2_t *dht, const char *value, ks_dht2_registry_callback_t callback);


KS_END_EXTERN_C

#endif /* KS_DHT_H */

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
