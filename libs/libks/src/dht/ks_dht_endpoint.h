#ifndef KS_DHT_ENDPOINT_H
#define KS_DHT_ENDPOINT_H

#include "ks.h"

KS_BEGIN_EXTERN_C

typedef struct ks_dht2_endpoint_s ks_dht2_endpoint_t;
struct ks_dht2_endpoint_s {
	ks_pool_t *pool;
	ks_sockaddr_t addr;
	ks_socket_t sock;
};

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
