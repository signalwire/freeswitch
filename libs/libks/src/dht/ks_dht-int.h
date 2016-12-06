#ifndef KS_DHT_INT_H
#define KS_DHT_INT_H

#include "ks.h"

KS_BEGIN_EXTERN_C

/**
 *
 */
KS_DECLARE(void) ks_dht2_idle(ks_dht2_t *dht);
KS_DECLARE(void) ks_dht2_idle_expirations(ks_dht2_t *dht);
KS_DECLARE(void) ks_dht2_idle_send(ks_dht2_t *dht);

KS_DECLARE(ks_status_t) ks_dht2_send(ks_dht2_t *dht, ks_dht2_message_t *message);
KS_DECLARE(ks_status_t) ks_dht2_send_error(ks_dht2_t *dht,
										   ks_sockaddr_t *raddr,
										   uint8_t *transactionid,
										   ks_size_t transactionid_length,
										   long long errorcode,
										   const char *errorstr);
KS_DECLARE(ks_status_t) ks_dht2_send_ping(ks_dht2_t *dht, ks_sockaddr_t *raddr);
KS_DECLARE(ks_status_t) ks_dht2_send_findnode(ks_dht2_t *dht, ks_sockaddr_t *raddr, ks_dht2_nodeid_raw_t *targetid);

KS_DECLARE(ks_status_t) ks_dht2_process(ks_dht2_t *dht, ks_sockaddr_t *raddr);

KS_DECLARE(ks_status_t) ks_dht2_process_query(ks_dht2_t *dht, ks_dht2_message_t *message);
KS_DECLARE(ks_status_t) ks_dht2_process_response(ks_dht2_t *dht, ks_dht2_message_t *message);
KS_DECLARE(ks_status_t) ks_dht2_process_error(ks_dht2_t *dht, ks_dht2_message_t *message);

KS_DECLARE(ks_status_t) ks_dht2_process_query_ping(ks_dht2_t *dht, ks_dht2_message_t *message);
KS_DECLARE(ks_status_t) ks_dht2_process_query_findnode(ks_dht2_t *dht, ks_dht2_message_t *message);

KS_DECLARE(ks_status_t) ks_dht2_process_response_ping(ks_dht2_t *dht, ks_dht2_message_t *message);
KS_DECLARE(ks_status_t) ks_dht2_process_response_findnode(ks_dht2_t *dht, ks_dht2_message_t *message);

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_endpoint_alloc(ks_dht2_endpoint_t **endpoint, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht2_endpoint_prealloc(ks_dht2_endpoint_t *endpoint, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht2_endpoint_free(ks_dht2_endpoint_t *endpoint);

KS_DECLARE(ks_status_t) ks_dht2_endpoint_init(ks_dht2_endpoint_t *endpoint, const ks_sockaddr_t *addr, ks_socket_t sock);
KS_DECLARE(ks_status_t) ks_dht2_endpoint_deinit(ks_dht2_endpoint_t *endpoint);


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
