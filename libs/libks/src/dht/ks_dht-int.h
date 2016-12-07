#ifndef KS_DHT_INT_H
#define KS_DHT_INT_H

#include "ks.h"

KS_BEGIN_EXTERN_C

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_utility_compact_address(ks_sockaddr_t *address,
														uint8_t *buffer,
														ks_size_t *buffer_length,
														ks_size_t buffer_size);
KS_DECLARE(ks_status_t) ks_dht2_utility_compact_node(ks_dht2_nodeid_t *nodeid,
													 ks_sockaddr_t *address,
													 uint8_t *buffer,
													 ks_size_t *buffer_length,
													 ks_size_t buffer_size);

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
KS_DECLARE(ks_status_t) ks_dht2_send_findnode(ks_dht2_t *dht, ks_sockaddr_t *raddr, ks_dht2_nodeid_t *targetid);

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

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_node_alloc(ks_dht2_node_t **node, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht2_node_prealloc(ks_dht2_node_t *node, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht2_node_free(ks_dht2_node_t *node);

KS_DECLARE(ks_status_t) ks_dht2_node_init(ks_dht2_node_t *node, const ks_dht2_nodeid_t *id, const ks_sockaddr_t *addr);
KS_DECLARE(ks_status_t) ks_dht2_node_deinit(ks_dht2_node_t *node);


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
