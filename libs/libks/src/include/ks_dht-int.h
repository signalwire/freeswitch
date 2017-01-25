#ifndef KS_DHT_INT_H
#define KS_DHT_INT_H

#include "ks.h"

KS_BEGIN_EXTERN_C

/**
 * Determines the appropriate endpoint to reach a remote address.
 * If an endpoint is provided, nothing more needs to be done.
 * If no endpoint is provided, first it will check for an active endpoint it can route though.
 * If no active endpoint is available and autorouting is enabled it will attempt to bind a usable endpoint.
 * @param dht pointer to the dht instance
 * @param raddr pointer to the remote address
 * @param endpoint dereferenced in/out pointer to the endpoint, if populated then returns immediately
 * @return The ks_status_t result: KS_STATUS_SUCCESS, ...
 * @see ks_ip_route
 * @see ks_hash_read_unlock
 * @see ks_addr_set
 * @see ks_dht_bind
 */
KS_DECLARE(ks_status_t) ks_dht_autoroute_check(ks_dht_t *dht, const ks_sockaddr_t *raddr, ks_dht_endpoint_t **endpoint);

/**
 * Called internally to receive datagrams from endpoints.
 * Handles datagrams by dispatching each through a threadpool.
 * @param dht pointer to the dht instance
 * @param timeout time in ms to wait for an incoming datagram on any endpoint
 */
KS_DECLARE(void) ks_dht_pulse_endpoints(ks_dht_t *dht, int32_t timeout);

/**
 * Called internally to expire or reannounce storage item data.
 * Handles reannouncing and purging of expiring storage items.
 * @param dht pointer to the dht instance
 */
KS_DECLARE(void) ks_dht_pulse_storageitems(ks_dht_t *dht);

/**
 * Called internally to process job state machine.
 * Handles completing and purging of finished jobs.
 * @param dht pointer to the dht instance
 */
KS_DECLARE(void) ks_dht_pulse_jobs(ks_dht_t *dht);

/**
 * Called internally to send queued messages.
 * Handles throttling of message sending to ensure system buffers are not overloaded and messages are not dropped.
 * @param dht pointer to the dht instance
 */
KS_DECLARE(void) ks_dht_pulse_send(ks_dht_t *dht);

/**
 * Called internally to expire transactions.
 * Handles purging of expired and finished transactions.
 * @param dht pointer to the dht instance
 */
KS_DECLARE(void) ks_dht_pulse_transactions(ks_dht_t *dht);

/**
 * Called internally to expire and cycle tokens.
 * Handles cycling new secret entropy for token generation.
 * @param dht pointer to the dht instance
 */
KS_DECLARE(void) ks_dht_pulse_tokens(ks_dht_t *dht);

/**
 * Converts a ks_dht_nodeid_t into it's hex string representation.
 * @param id pointer to the nodeid
 * @param buffer pointer to the buffer able to contain at least (KS_DHT_NODEID_SIZE * 2) + 1 characters
 * @return The pointer to the front of the populated string buffer
 */
KS_DECLARE(char *) ks_dht_hex(const uint8_t *data, char *buffer, ks_size_t len);

/**
 * Compacts address information as per the DHT specifications.
 * @param address pointer to the address being compacted from
 * @param buffer pointer to the buffer containing compacted data
 * @param buffer_length pointer to the buffer length consumed
 * @param buffer_size max size of the buffer
 * @return The ks_status_t result: KS_STATUS_SUCCESS, KS_STATUS_NO_MEM
 */
KS_DECLARE(ks_status_t) ks_dht_utility_compact_addressinfo(const ks_sockaddr_t *address,
														   uint8_t *buffer,
														   ks_size_t *buffer_length,
														   ks_size_t buffer_size);

/**
 * Expands address information as per the DHT specifications.
 * @param buffer pointer to the buffer containing compacted data
 * @param buffer_length pointer to the buffer length consumed
 * @param buffer_size max size of the buffer
 * @param address pointer to the address being expanded into
 * @return The ks_status_t result: KS_STATUS_SUCCESS, KS_STATUS_NO_MEM, ...
 * @see ks_addr_set_raw
 */
KS_DECLARE(ks_status_t) ks_dht_utility_expand_addressinfo(const uint8_t *buffer,
														  ks_size_t *buffer_length,
														  ks_size_t buffer_size,
														  ks_sockaddr_t *address);

/**
 * Compacts node information as per the DHT specifications.
 * Compacts address information after the nodeid.
 * @param nodeid pointer to the nodeid being compacted from
 * @param address pointer to the address being compacted from
 * @param buffer pointer to the buffer containing compacted data
 * @param buffer_length pointer to the buffer length consumed
 * @param buffer_size max size of the buffer
 * @return The ks_status_t result: KS_STATUS_SUCCESS, KS_STATUS_NO_MEM, ...
 * @see ks_dht_utility_compact_addressinfo
 */
KS_DECLARE(ks_status_t) ks_dht_utility_compact_nodeinfo(const ks_dht_nodeid_t *nodeid,
														const ks_sockaddr_t *address,
														uint8_t *buffer,
														ks_size_t *buffer_length,
														ks_size_t buffer_size);

/**
 * Expands address information as per the DHT specifications.
 * Expands compacted address information after the nodeid.
 * @param buffer pointer to the buffer containing compacted data
 * @param buffer_length pointer to the buffer length consumed
 * @param buffer_size max size of the buffer
 * @param address pointer to the address being expanded into
 * @param nodeid pointer to the nodeid being expanded into
 * @return The ks_status_t result: KS_STATUS_SUCCESS, KS_STATUS_NO_MEM, ...
 * @see ks_dht_utility_expand_addressinfo
 */
KS_DECLARE(ks_status_t) ks_dht_utility_expand_nodeinfo(const uint8_t *buffer,
													   ks_size_t *buffer_length,
													   ks_size_t buffer_size,
													   ks_dht_nodeid_t *nodeid,
													   ks_sockaddr_t *address);

/**
 * Extracts a ks_dht_nodeid_t from a bencode dictionary given a string key.
 * @param args pointer to the bencode dictionary
 * @param key string key in the bencode dictionary to extract the value from
 * @param nodeid dereferenced out pointer to the nodeid
 * @return The ks_status_t result: KS_STATUS_SUCCESS, KS_STATUS_ARG_INVALID
 */
KS_DECLARE(ks_status_t) ks_dht_utility_extract_nodeid(struct bencode *args, const char *key, ks_dht_nodeid_t **nodeid);

/**
 * Extracts a ks_dht_token_t from a bencode dictionary given a string key.
 * @param args pointer to the bencode dictionary
 * @param key string key in the bencode dictionary to extract the value from
 * @param nodeid dereferenced out pointer to the token
 * @return The ks_status_t result: KS_STATUS_SUCCESS, KS_STATUS_ARG_INVALID
 */
KS_DECLARE(ks_status_t) ks_dht_utility_extract_token(struct bencode *args, const char *key, ks_dht_token_t **token);

/**
 * Generates an opaque write token based on a shifting secret value, the remote address and target nodeid of interest.
 * This token ensures that future operations can be verified to the remote peer and target id requested.
 * @param secret rotating secret portion of the token hash
 * @param raddr pointer to the remote address used for the ip and port in the token hash
 * @param target pointer to the nodeid of the target used for the token hash
 * @param token pointer to the output token being generated
 * @return The ks_status_t result: KS_STATUS_SUCCESS, KS_STATUS_FAIL
 */
KS_DECLARE(ks_status_t) ks_dht_token_generate(uint32_t secret, const ks_sockaddr_t *raddr, ks_dht_nodeid_t *target, ks_dht_token_t *token);

/**
 * Verify an opaque write token matches the provided remote address and target nodeid.
 * Handles checking against the last two secret values for the token hash.
 * @param dht pointer to the dht instance
 * @param raddr pointer to the remote address used for the ip and port in the token hash
 * @param target pointer to the nodeid of the target used for the token hash
 * @param token pointer to the input token being compared
 * @return Either KS_TRUE if verification passes, otherwise KS_FALSE
 */
KS_DECLARE(ks_bool_t) ks_dht_token_verify(ks_dht_t *dht, const ks_sockaddr_t *raddr, ks_dht_nodeid_t *target, ks_dht_token_t *token);

/**
 * Encodes a message for transmission as a UDP datagram and sends it.
 * Uses the internally tracked local endpoint and remote address to route the UDP datagram.
 * @param dht pointer to the dht instance
 * @param message pointer to the message being sent
 * @return The ks_status_t result: KS_STATUS_SUCCESS, ...
 * @see ks_socket_sendto
 */
KS_DECLARE(ks_status_t) ks_dht_send(ks_dht_t *dht, ks_dht_message_t *message);

/**
 * Sets up the common parts of a query message.
 * Determines the local endpoint aware of autorouting, assigns the remote address, generates a transaction, and queues a callback.
 * @param dht pointer to the dht instance
 * @param job pointer to the job
 * @param query string value of the query type, for example "ping"
 * @param callback callback to be called when response to transaction is received
 * @param transaction dereferenced out pointer to the allocated transaction, may be NULL to ignore output
 * @param message dereferenced out pointer to the allocated message
 * @param args dereferenced out pointer to the allocated bencode args, may be NULL to ignore output
 * @return The ks_status_t result: KS_STATUS_SUCCESS, KS_STATUS_FAIL, ...
 * @see ks_dht_autoroute_check
 * @see ks_dht_transaction_alloc
 * @see ks_dht_transaction_init
 * @see ks_dht_message_alloc
 * @see ks_dht_message_init
 * @see ks_dht_message_query
 * @see ks_hash_insert
 */
KS_DECLARE(ks_status_t) ks_dht_query_setup(ks_dht_t *dht,
										   ks_dht_job_t *job,
										   const char *query,
										   ks_dht_job_callback_t callback,
										   ks_dht_transaction_t **transaction,
										   ks_dht_message_t **message,
										   struct bencode **args);

/**
 * Sets up the common parts of a response message.
 * Determines the local endpoint aware of autorouting, assigns the remote address, and assigns the transaction.
 * @param dht pointer to the dht instance
 * @param ep pointer to the endpoint, may be NULL to find an endpoint or autoroute one
 * @param raddr pointer to the remote address
 * @param transactionid pointer to the buffer containing the transactionid, may be of variable size depending on the querying node
 * @param transactionid_length length of the transactionid buffer
 * @param message dereferenced out pointer to the allocated message
 * @param args dereferenced out pointer to the allocated bencode args, may be NULL to ignore output
 * @return The ks_status_t result: KS_STATUS_SUCCESS, ...
 * @see ks_dht_autoroute_check
 * @see ks_dht_message_alloc
 * @see ks_dht_message_init
 * @see ks_dht_message_response
 */
KS_DECLARE(ks_status_t) ks_dht_response_setup(ks_dht_t *dht,
											  ks_dht_endpoint_t *ep,
											  const ks_sockaddr_t *raddr,
											  uint8_t *transactionid,
											  ks_size_t transactionid_length,
											  ks_dht_message_t **message,
											  struct bencode **args);
												
						
KS_DECLARE(ks_status_t) ks_dht_error(ks_dht_t *dht,
									 ks_dht_endpoint_t *ep,
									 const ks_sockaddr_t *raddr,
									 uint8_t *transactionid,
									 ks_size_t transactionid_length,
									 long long errorcode,
									 const char *errorstr);

KS_DECLARE(void) ks_dht_pulse_jobs(ks_dht_t *dht);
KS_DECLARE(ks_status_t) ks_dht_query_ping(ks_dht_t *dht, ks_dht_job_t *job);
KS_DECLARE(ks_status_t) ks_dht_query_findnode(ks_dht_t *dht, ks_dht_job_t *job);
KS_DECLARE(ks_status_t) ks_dht_query_get(ks_dht_t *dht, ks_dht_job_t *job);
KS_DECLARE(ks_status_t) ks_dht_query_put(ks_dht_t *dht, ks_dht_job_t *job);

KS_DECLARE(void *)ks_dht_process(ks_thread_t *thread, void *data);

KS_DECLARE(ks_status_t) ks_dht_process_query(ks_dht_t *dht, ks_dht_message_t *message);
KS_DECLARE(ks_status_t) ks_dht_process_response(ks_dht_t *dht, ks_dht_message_t *message);
KS_DECLARE(ks_status_t) ks_dht_process_error(ks_dht_t *dht, ks_dht_message_t *message);

KS_DECLARE(ks_status_t) ks_dht_process_query_ping(ks_dht_t *dht, ks_dht_message_t *message);
KS_DECLARE(ks_status_t) ks_dht_process_response_ping(ks_dht_t *dht, ks_dht_job_t *job);

KS_DECLARE(ks_status_t) ks_dht_process_query_findnode(ks_dht_t *dht, ks_dht_message_t *message);
KS_DECLARE(ks_status_t) ks_dht_process_response_findnode(ks_dht_t *dht, ks_dht_job_t *job);

KS_DECLARE(ks_status_t) ks_dht_process_query_get(ks_dht_t *dht, ks_dht_message_t *message);
KS_DECLARE(ks_status_t) ks_dht_process_response_get(ks_dht_t *dht, ks_dht_job_t *job);

KS_DECLARE(ks_status_t) ks_dht_process_query_put(ks_dht_t *dht, ks_dht_message_t *message);
KS_DECLARE(ks_status_t) ks_dht_process_response_put(ks_dht_t *dht, ks_dht_job_t *job);


/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_datagram_create(ks_dht_datagram_t **datagram,
											   ks_pool_t *pool,
											   ks_dht_t *dht,
											   ks_dht_endpoint_t *endpoint,
											   const ks_sockaddr_t *raddr);
KS_DECLARE(void) ks_dht_datagram_destroy(ks_dht_datagram_t **datagram);

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_job_create(ks_dht_job_t **job,
										  ks_pool_t *pool,
										  const ks_sockaddr_t *raddr,
										  int32_t attempts,
										  void *data);
KS_DECLARE(void) ks_dht_job_build_ping(ks_dht_job_t *job, ks_dht_job_callback_t query_callback, ks_dht_job_callback_t finish_callback);
KS_DECLARE(void) ks_dht_job_build_findnode(ks_dht_job_t *job,
										   ks_dht_job_callback_t query_callback,
										   ks_dht_job_callback_t finish_callback,
										   ks_dht_nodeid_t *target);
KS_DECLARE(void) ks_dht_job_build_get(ks_dht_job_t *job,
									  ks_dht_job_callback_t query_callback,
									  ks_dht_job_callback_t finish_callback,
									  ks_dht_nodeid_t *target,
									  const uint8_t *salt,
									  ks_size_t salt_length);
KS_DECLARE(void) ks_dht_job_build_put(ks_dht_job_t *job,
									  ks_dht_job_callback_t query_callback,
									  ks_dht_job_callback_t finish_callback,
									  ks_dht_token_t *token,
									  int64_t cas,
									  ks_dht_storageitem_t *item);
KS_DECLARE(void) ks_dht_job_build_search(ks_dht_job_t *job,
										 ks_dht_job_callback_t query_callback,
										 ks_dht_job_callback_t finish_callback);
KS_DECLARE(void) ks_dht_job_build_search_findnode(ks_dht_job_t *job,
									ks_dht_nodeid_t *target,
									uint32_t family,
									ks_dht_job_callback_t query_callback,
									ks_dht_job_callback_t finish_callback);
KS_DECLARE(void) ks_dht_job_destroy(ks_dht_job_t **job);


/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_endpoint_create(ks_dht_endpoint_t **endpoint,
											   ks_pool_t *pool,
											   const ks_sockaddr_t *addr,
											   ks_socket_t sock);
KS_DECLARE(void) ks_dht_endpoint_destroy(ks_dht_endpoint_t **endpoint);

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_message_create(ks_dht_message_t **message,
											  ks_pool_t *pool,
											  ks_dht_endpoint_t *endpoint,
											  const ks_sockaddr_t *raddr,
											  ks_bool_t alloc_data);
/**
 *
 */
KS_DECLARE(void) ks_dht_message_destroy(ks_dht_message_t **message);

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_message_parse(ks_dht_message_t *message, const uint8_t *buffer, ks_size_t buffer_length);

																								
/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_search_create(ks_dht_search_t **search,
											 ks_pool_t *pool,
											 ks_dhtrt_routetable_t *table,
											 const ks_dht_nodeid_t *target,
											 ks_dht_job_callback_t callback,
											 void *data);
KS_DECLARE(void) ks_dht_search_destroy(ks_dht_search_t **search);

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_publish_create(ks_dht_publish_t **publish,
											  ks_pool_t *pool,
											  ks_dht_job_callback_t callback,
											  void *data,
											  int64_t cas,
											  ks_dht_storageitem_t *item);
KS_DECLARE(void) ks_dht_publish_destroy(ks_dht_publish_t **publish);

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_distribute_create(ks_dht_distribute_t **distribute,
												 ks_pool_t *pool,
												 ks_dht_storageitem_callback_t callback,
												 void *data,
												 int64_t cas,
												 ks_dht_storageitem_t *item);
KS_DECLARE(void) ks_dht_distribute_destroy(ks_dht_distribute_t **distribute);

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_storageitem_target_immutable_internal(struct bencode *value, ks_dht_nodeid_t *target);

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_storageitem_target_mutable_internal(ks_dht_storageitem_pkey_t *pk, struct bencode *salt, ks_dht_nodeid_t *target);

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_storageitem_create_immutable_internal(ks_dht_storageitem_t **item,
																	 ks_pool_t *pool,
																	 ks_dht_nodeid_t *target,
																	 struct bencode *v,
																	 ks_bool_t clone_v);
KS_DECLARE(ks_status_t) ks_dht_storageitem_create_immutable(ks_dht_storageitem_t **item,
															ks_pool_t *pool,
															ks_dht_nodeid_t *target,
															const uint8_t *value,
															ks_size_t value_length);
KS_DECLARE(ks_status_t) ks_dht_storageitem_create_mutable_internal(ks_dht_storageitem_t **item,
																   ks_pool_t *pool,
																   ks_dht_nodeid_t *target,
																   struct bencode *v,
																   ks_bool_t clone_v,
																   ks_dht_storageitem_pkey_t *pk,
																   struct bencode *salt,
																   ks_bool_t clone_salt,
																   int64_t sequence,
																   ks_dht_storageitem_signature_t *signature);
KS_DECLARE(ks_status_t) ks_dht_storageitem_create_mutable(ks_dht_storageitem_t **item,
														  ks_pool_t *pool,
														  ks_dht_nodeid_t *target,
														  const uint8_t *value,
														  ks_size_t value_length,
														  ks_dht_storageitem_pkey_t *pk,
														  const uint8_t *salt,
														  ks_size_t salt_length,
														  int64_t sequence,
														  ks_dht_storageitem_signature_t *signature);
KS_DECLARE(void) ks_dht_storageitem_update_mutable(ks_dht_storageitem_t *item, struct bencode *v, int64_t sequence, ks_dht_storageitem_signature_t *signature);
KS_DECLARE(void) ks_dht_storageitem_destroy(ks_dht_storageitem_t **item);

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_transaction_create(ks_dht_transaction_t **transaction,
												  ks_pool_t *pool,
												  ks_dht_job_t *job,
												  uint32_t transactionid,
												  ks_dht_job_callback_t callback);
KS_DECLARE(void) ks_dht_transaction_destroy(ks_dht_transaction_t **transaction);

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
