#ifndef KS_DHT_ENDPOINT_INT_H
#define KS_DHT_ENDPOINT_INT_H

#include "ks.h"

KS_BEGIN_EXTERN_C

KS_DECLARE(ks_status_t) ks_dht2_endpoint_alloc(ks_dht2_endpoint_t **endpoint, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht2_endpoint_prealloc(ks_dht2_endpoint_t *endpoint, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht2_endpoint_free(ks_dht2_endpoint_t *endpoint);

KS_DECLARE(ks_status_t) ks_dht2_endpoint_init(ks_dht2_endpoint_t *endpoint, const ks_sockaddr_t *addr, ks_socket_t sock);
KS_DECLARE(ks_status_t) ks_dht2_endpoint_deinit(ks_dht2_endpoint_t *endpoint);
						
KS_END_EXTERN_C

#endif /* KS_DHT_ENDPOINT_H */


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
