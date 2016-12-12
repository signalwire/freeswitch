#ifndef KS_DHT_INT_H
#define KS_DHT_INT_H

#include "ks.h"

KS_BEGIN_EXTERN_C

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_utility_compact_addressinfo(const ks_sockaddr_t *address,
														   uint8_t *buffer,
														   ks_size_t *buffer_length,
														   ks_size_t buffer_size);
KS_DECLARE(ks_status_t) ks_dht_utility_expand_addressinfo(const uint8_t *buffer,
														  ks_size_t *buffer_length,
														  ks_size_t buffer_size,
														  ks_sockaddr_t *address);
KS_DECLARE(ks_status_t) ks_dht_utility_compact_nodeinfo(const ks_dht_nodeid_t *nodeid,
														const ks_sockaddr_t *address,
														uint8_t *buffer,
														ks_size_t *buffer_length,
														ks_size_t buffer_size);
KS_DECLARE(ks_status_t) ks_dht_utility_expand_nodeinfo(const uint8_t *buffer,
													   ks_size_t *buffer_length,
													   ks_size_t buffer_size,
													   ks_dht_nodeid_t *nodeid,
													   ks_sockaddr_t *address);

/**
 *
 */
KS_DECLARE(void) ks_dht_pulse_expirations(ks_dht_t *dht);
KS_DECLARE(void) ks_dht_pulse_send(ks_dht_t *dht);

KS_DECLARE(ks_status_t) ks_dht_send(ks_dht_t *dht, ks_dht_message_t *message);
KS_DECLARE(ks_status_t) ks_dht_send_error(ks_dht_t *dht,
										  ks_dht_endpoint_t *ep,
										  ks_sockaddr_t *raddr,
										  uint8_t *transactionid,
										  ks_size_t transactionid_length,
										  long long errorcode,
										  const char *errorstr);
KS_DECLARE(ks_status_t) ks_dht_send_ping(ks_dht_t *dht, ks_dht_endpoint_t *ep, ks_sockaddr_t *raddr);
KS_DECLARE(ks_status_t) ks_dht_send_findnode(ks_dht_t *dht, ks_dht_endpoint_t *ep, ks_sockaddr_t *raddr, ks_dht_nodeid_t *targetid);
KS_DECLARE(ks_status_t) ks_dht_send_get(ks_dht_t *dht, ks_dht_endpoint_t *ep, ks_sockaddr_t *raddr, ks_dht_nodeid_t *targetid);

KS_DECLARE(ks_status_t) ks_dht_process(ks_dht_t *dht, ks_dht_endpoint_t *ep, ks_sockaddr_t *raddr);

KS_DECLARE(ks_status_t) ks_dht_process_query(ks_dht_t *dht, ks_dht_message_t *message);
KS_DECLARE(ks_status_t) ks_dht_process_response(ks_dht_t *dht, ks_dht_message_t *message);
KS_DECLARE(ks_status_t) ks_dht_process_error(ks_dht_t *dht, ks_dht_message_t *message);

KS_DECLARE(ks_status_t) ks_dht_process_query_ping(ks_dht_t *dht, ks_dht_message_t *message);
KS_DECLARE(ks_status_t) ks_dht_process_response_ping(ks_dht_t *dht, ks_dht_message_t *message);

KS_DECLARE(ks_status_t) ks_dht_process_query_findnode(ks_dht_t *dht, ks_dht_message_t *message);
KS_DECLARE(ks_status_t) ks_dht_process_response_findnode(ks_dht_t *dht, ks_dht_message_t *message);

KS_DECLARE(ks_status_t) ks_dht_process_query_get(ks_dht_t *dht, ks_dht_message_t *message);
KS_DECLARE(ks_status_t) ks_dht_process_response_get(ks_dht_t *dht, ks_dht_message_t *message);

KS_DECLARE(ks_status_t) ks_dht_process_query_put(ks_dht_t *dht, ks_dht_message_t *message);
KS_DECLARE(ks_status_t) ks_dht_process_response_put(ks_dht_t *dht, ks_dht_message_t *message);

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_endpoint_alloc(ks_dht_endpoint_t **endpoint, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht_endpoint_prealloc(ks_dht_endpoint_t *endpoint, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht_endpoint_free(ks_dht_endpoint_t **endpoint);

KS_DECLARE(ks_status_t) ks_dht_endpoint_init(ks_dht_endpoint_t *endpoint,
											 const ks_dht_nodeid_t *nodeid,
											 const ks_sockaddr_t *addr,
											 ks_socket_t sock);
KS_DECLARE(ks_status_t) ks_dht_endpoint_deinit(ks_dht_endpoint_t *endpoint);

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_storageitem_alloc(ks_dht_storageitem_t **item, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht_storageitem_prealloc(ks_dht_storageitem_t *item, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht_storageitem_free(ks_dht_storageitem_t **item);

KS_DECLARE(ks_status_t) ks_dht_storageitem_init(ks_dht_storageitem_t *item, struct bencode *v);
KS_DECLARE(ks_status_t) ks_dht_storageitem_deinit(ks_dht_storageitem_t *item);

KS_DECLARE(ks_status_t) ks_dht_storageitem_create(ks_dht_storageitem_t *item, ks_bool_t mutable);
KS_DECLARE(ks_status_t) ks_dht_storageitem_immutable(ks_dht_storageitem_t *item);
KS_DECLARE(ks_status_t) ks_dht_storageitem_mutable(ks_dht_storageitem_t *item,
												   ks_dht_storageitem_key_t *k,
												   uint8_t *salt,
												   ks_size_t salt_length,
												   int64_t sequence,
												   ks_dht_storageitem_signature_t *signature);
/**
 *
 */
//KS_DECLARE(ks_status_t) ks_dht_node_alloc(ks_dht_node_t **node, ks_pool_t *pool);
//KS_DECLARE(ks_status_t) ks_dht_node_prealloc(ks_dht_node_t *node, ks_pool_t *pool);
//KS_DECLARE(ks_status_t) ks_dht_node_free(ks_dht_node_t *node);

//KS_DECLARE(ks_status_t) ks_dht_node_init(ks_dht_node_t *node, const ks_dht_nodeid_t *id, const ks_sockaddr_t *addr);
//KS_DECLARE(ks_status_t) ks_dht_node_deinit(ks_dht_node_t *node);

//KS_DECLARE(ks_status_t) ks_dht_node_address_check(ks_dht_node_t *node, const ks_sockaddr_t *addr);
//KS_DECLARE(ks_bool_t) ks_dht_node_address_exists(ks_dht_node_t *node, const ks_sockaddr_t *addr);
//KS_DECLARE(ks_status_t) ks_dht_node_address_add(ks_dht_node_t *node, const ks_sockaddr_t *addr);


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
