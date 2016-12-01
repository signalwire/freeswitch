#ifndef KS_DHT_INT_H
#define KS_DHT_INT_H

#include "ks.h"

KS_BEGIN_EXTERN_C


KS_DECLARE(ks_status_t) ks_dht2_idle(ks_dht2_t *dht);
KS_DECLARE(ks_status_t) ks_dht2_process(ks_dht2_t *dht, ks_sockaddr_t *raddr);

						
KS_END_EXTERN_C

#endif /* KS_DHT_INT_H */

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
