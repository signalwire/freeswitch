#ifndef KS_DHT_H
#define KS_DHT_H

#include "ks.h"
#include "ks_bencode.h"
#include "sodium.h"

KS_BEGIN_EXTERN_C


#define KS_DHT_DEFAULT_PORT 5309

#define KS_DHT_TPOOL_MIN 2
#define KS_DHT_TPOOL_MAX 8
#define KS_DHT_TPOOL_STACK (1024 * 256)
#define KS_DHT_TPOOL_IDLE 10

#define KS_DHT_DATAGRAM_BUFFER_SIZE 1000

//#define KS_DHT_RECV_BUFFER_SIZE 0xFFFF

#define KS_DHT_NODEID_SIZE 20

#define KS_DHT_RESPONSE_NODES_MAX_SIZE 8

#define KS_DHT_MESSAGE_TRANSACTIONID_MAX_SIZE 20
#define KS_DHT_MESSAGE_TYPE_MAX_SIZE 20
#define KS_DHT_MESSAGE_QUERY_MAX_SIZE 20
#define KS_DHT_MESSAGE_ERROR_MAX_SIZE 256

#define KS_DHT_TRANSACTION_EXPIRATION 10
#define KS_DHT_TRANSACTIONS_PULSE 1

#define KS_DHT_SEARCH_RESULTS_MAX_SIZE 8 // @todo replace with KS_DHTRT_BUCKET_SIZE

#define KS_DHT_STORAGEITEM_PKEY_SIZE crypto_sign_PUBLICKEYBYTES
#define KS_DHT_STORAGEITEM_SKEY_SIZE crypto_sign_SECRETKEYBYTES
#define KS_DHT_STORAGEITEM_SALT_MAX_SIZE 64
#define KS_DHT_STORAGEITEM_SIGNATURE_SIZE crypto_sign_BYTES
#define KS_DHT_STORAGEITEM_EXPIRATION 7200
#define KS_DHT_STORAGEITEM_KEEPALIVE 300
#define KS_DHT_STORAGEITEMS_PULSE 10

#define KS_DHT_TOKEN_SIZE SHA_DIGEST_LENGTH
#define KS_DHT_TOKEN_EXPIRATION 300
#define KS_DHT_TOKENS_PULSE 1

#define  KS_DHTRT_MAXQUERYSIZE 20

typedef struct ks_dht_s ks_dht_t;
typedef struct ks_dht_datagram_s ks_dht_datagram_t;
typedef struct ks_dht_job_s ks_dht_job_t;
typedef struct ks_dht_nodeid_s ks_dht_nodeid_t;
typedef struct ks_dht_token_s ks_dht_token_t;
typedef struct ks_dht_storageitem_pkey_s ks_dht_storageitem_pkey_t;
typedef struct ks_dht_storageitem_skey_s ks_dht_storageitem_skey_t;
typedef struct ks_dht_storageitem_signature_s ks_dht_storageitem_signature_t;
typedef struct ks_dht_message_s ks_dht_message_t;
typedef struct ks_dht_endpoint_s ks_dht_endpoint_t;
typedef struct ks_dht_transaction_s ks_dht_transaction_t;
typedef struct ks_dht_search_s ks_dht_search_t;
typedef struct ks_dht_publish_s ks_dht_publish_t;
typedef struct ks_dht_distribute_s ks_dht_distribute_t;
typedef struct ks_dht_node_s ks_dht_node_t;
typedef struct ks_dhtrt_routetable_s ks_dhtrt_routetable_t;
typedef struct ks_dhtrt_querynodes_s ks_dhtrt_querynodes_t;
typedef struct ks_dht_storageitem_s ks_dht_storageitem_t;


typedef ks_status_t (*ks_dht_job_callback_t)(ks_dht_t *dht, ks_dht_job_t *job);
typedef ks_status_t (*ks_dht_message_callback_t)(ks_dht_t *dht, ks_dht_message_t *message);
//typedef ks_status_t (*ks_dht_search_callback_t)(ks_dht_t *dht, ks_dht_search_t *search);
typedef ks_status_t (*ks_dht_storageitem_callback_t)(ks_dht_t *dht, ks_dht_storageitem_t *item);


struct ks_dht_datagram_s {
	ks_pool_t *pool;
	ks_dht_t *dht;
	ks_dht_endpoint_t *endpoint;
	ks_sockaddr_t raddr;
	uint8_t buffer[KS_DHT_DATAGRAM_BUFFER_SIZE];
	ks_size_t buffer_length;
};

/**
 * Note: This must remain a structure for casting from raw data
 */
struct ks_dht_nodeid_s {
	uint8_t id[KS_DHT_NODEID_SIZE];
};

enum ks_afflags_t { ifv4=AF_INET, ifv6=AF_INET6, ifboth=AF_INET+AF_INET6};
enum ks_dht_nodetype_t { KS_DHT_REMOTE=0x01, 
						 KS_DHT_LOCAL=0x02, 
						 KS_DHT_BOTH=KS_DHT_REMOTE+KS_DHT_LOCAL };

enum ks_create_node_flags_t { 
						 KS_DHTRT_CREATE_DEFAULT=0,
						 KS_DHTRT_CREATE_PING,
						 KS_DHTRT_CREATE_TOUCH 
};		
				
struct ks_dht_node_s {
    ks_dht_nodeid_t  nodeid;
    ks_sockaddr_t    addr;
    enum ks_dht_nodetype_t type;               /* local or remote */
    ks_dhtrt_routetable_t* table;
    ks_rwl_t        *reflock;          
};

struct ks_dht_token_s {
	uint8_t token[KS_DHT_TOKEN_SIZE];
};

enum ks_dht_job_state_t {
	KS_DHT_JOB_STATE_QUERYING,
	KS_DHT_JOB_STATE_RESPONDING,
	KS_DHT_JOB_STATE_EXPIRING,
	KS_DHT_JOB_STATE_COMPLETING,
};

enum ks_dht_job_result_t {
	KS_DHT_JOB_RESULT_SUCCESS = 0,
	KS_DHT_JOB_RESULT_EXPIRED,
	KS_DHT_JOB_RESULT_ERROR,
	KS_DHT_JOB_RESULT_FAILURE,
};

struct ks_dht_job_s {
	ks_pool_t *pool;
	ks_dht_t *dht;
	ks_dht_job_t *next;

	enum ks_dht_job_state_t state;
	enum ks_dht_job_result_t result;

	ks_sockaddr_t raddr; // will obtain local endpoint node id when creating message using raddr
	int32_t attempts;

	//enum ks_dht_job_type_t type;
	ks_dht_job_callback_t query_callback;
	ks_dht_job_callback_t finish_callback;

	void *data;
	ks_dht_message_t *response;

	// job specific query parameters
	ks_dht_nodeid_t query_target;
	struct bencode *query_salt;
	int64_t query_cas;
	ks_dht_token_t query_token;
	ks_dht_storageitem_t *query_storageitem;

	// error response parameters
	int64_t error_code;
	struct bencode *error_description;
	
	// job specific response parameters
	ks_dht_node_t *response_id;
	ks_dht_node_t *response_nodes[KS_DHT_RESPONSE_NODES_MAX_SIZE];
	ks_size_t response_nodes_count;
	ks_dht_node_t *response_nodes6[KS_DHT_RESPONSE_NODES_MAX_SIZE];
	ks_size_t response_nodes6_count;
	
	ks_dht_token_t response_token;
	int64_t response_seq;
	ks_bool_t response_hasitem;
	ks_dht_storageitem_t *response_storageitem;
};

struct ks_dhtrt_routetable_s {
    void*       internal;                       
    ks_pool_t*  pool;                           
    ks_logger_t logger;
};

struct ks_dhtrt_querynodes_s {
    ks_dht_nodeid_t nodeid;                   /* in: id to query                   */
    enum ks_afflags_t family;                 /* in: AF_INET or AF_INET6 or both   */
    enum ks_dht_nodetype_t type;              /* remote, local, or  both           */
    uint8_t        max;                       /* in: maximum to return             */
    uint8_t        count;                     /* out: number returned              */
    ks_dht_node_t* nodes[ KS_DHTRT_MAXQUERYSIZE ]; /* out: array of peers (ks_dht_node_t* nodes[incount]) */
};

struct ks_dht_storageitem_pkey_s {
	uint8_t key[KS_DHT_STORAGEITEM_PKEY_SIZE];
};

struct ks_dht_storageitem_skey_s {
	uint8_t key[KS_DHT_STORAGEITEM_SKEY_SIZE];
};

struct ks_dht_storageitem_signature_s {
	uint8_t sig[KS_DHT_STORAGEITEM_SIGNATURE_SIZE];
};

struct ks_dht_message_s {
	ks_pool_t *pool;
	ks_dht_endpoint_t *endpoint;
	ks_sockaddr_t raddr;
	struct bencode *data;
	uint8_t transactionid[KS_DHT_MESSAGE_TRANSACTIONID_MAX_SIZE];
	ks_size_t transactionid_length;
	ks_dht_transaction_t *transaction;
	char type[KS_DHT_MESSAGE_TYPE_MAX_SIZE];
	struct bencode *args;
	ks_dht_nodeid_t args_id;
};

struct ks_dht_endpoint_s {
	ks_pool_t *pool;
	ks_sockaddr_t addr;
	ks_socket_t sock;
};

struct ks_dht_transaction_s {
	ks_pool_t *pool;
	ks_dht_job_t *job;
	uint32_t transactionid;
	//ks_dht_nodeid_t target; // @todo look at moving this into job now
	ks_dht_job_callback_t callback;
	ks_time_t expiration;
	ks_bool_t finished;
};

struct ks_dht_search_s {
	ks_pool_t *pool;
	ks_dhtrt_routetable_t *table;
	ks_dht_nodeid_t target;
	ks_dht_job_callback_t callback;
	void *data;
	ks_mutex_t *mutex;
	ks_hash_t *searched;
	int32_t searching;
	ks_dht_node_t *results[KS_DHT_SEARCH_RESULTS_MAX_SIZE];
	ks_dht_nodeid_t distances[KS_DHT_SEARCH_RESULTS_MAX_SIZE];
	ks_size_t results_length;
};

struct ks_dht_publish_s {
	ks_pool_t *pool;
	ks_dht_job_callback_t callback;
	void *data;
	int64_t cas;
	ks_dht_storageitem_t *item;
};

struct ks_dht_distribute_s {
	ks_pool_t *pool;
	ks_dht_storageitem_callback_t callback;
	void *data;
	ks_mutex_t *mutex;
	int32_t publishing;
	int64_t cas;
	ks_dht_storageitem_t *item;
};

struct ks_dht_storageitem_s {
	ks_pool_t *pool;
	ks_dht_nodeid_t id;
	ks_time_t expiration;
	ks_time_t keepalive;
	struct bencode *v;

	ks_mutex_t *mutex;
	volatile int32_t refc;
	ks_dht_storageitem_callback_t callback;

	ks_bool_t mutable;
	ks_dht_storageitem_pkey_t pk;
	ks_dht_storageitem_skey_t sk;
	struct bencode *salt;
	int64_t seq;
	ks_dht_storageitem_signature_t sig;
};

struct ks_dht_s {
	ks_pool_t *pool;
	ks_bool_t pool_alloc;

	ks_thread_pool_t *tpool;
	ks_bool_t tpool_alloc;

	ks_dht_nodeid_t nodeid;
	// @todo make sure this node is unlocked, and never gets destroyed, should also never use local nodes in search results as they can be internal
	// network addresses, not what others have contacted through
	ks_dht_node_t *node;
	
	ks_bool_t autoroute;
	ks_port_t autoroute_port;
	
	ks_hash_t *registry_type;
	ks_hash_t *registry_query;
	ks_hash_t *registry_error;

	ks_dht_endpoint_t **endpoints;
	int32_t endpoints_length;
	int32_t endpoints_size;
	ks_hash_t *endpoints_hash;
	struct pollfd *endpoints_poll;

	ks_q_t *send_q;
	ks_dht_message_t *send_q_unsent;
	uint8_t recv_buffer[KS_DHT_DATAGRAM_BUFFER_SIZE + 1]; // Add 1, if we receive it then overflow error
	ks_size_t recv_buffer_length;

	ks_mutex_t *jobs_mutex;
	ks_dht_job_t *jobs_first;
	ks_dht_job_t *jobs_last;

	ks_time_t transactions_pulse;
	ks_mutex_t *transactionid_mutex;
	volatile uint32_t transactionid_next;
	ks_hash_t *transactions_hash;

	ks_dhtrt_routetable_t *rt_ipv4;
	ks_dhtrt_routetable_t *rt_ipv6;

	ks_time_t tokens_pulse;
	volatile uint32_t token_secret_current;
	volatile uint32_t token_secret_previous;
	ks_time_t token_secret_expiration;

	ks_time_t storageitems_pulse;
	ks_hash_t *storageitems_hash;
};

/**
 * Constructor function for ks_dht_t.
 * Will allocate and initialize internal state including registration of message handlers.
 * @param dht dereferenced out pointer to the allocated dht instance
 * @param pool pointer to the memory pool used by the dht instance, may be NULL to create a new memory pool internally
 * @param tpool pointer to a thread pool used by the dht instance, may be NULL to create a new thread pool internally
 * @param nodeid pointer to the nodeid for this dht instance
 * @return The ks_status_t result: KS_STATUS_SUCCESS, KS_STATUS_NO_MEM
 */
KS_DECLARE(ks_status_t) ks_dht_create(ks_dht_t **dht, ks_pool_t *pool, ks_thread_pool_t *tpool, ks_dht_nodeid_t *nodeid);
						
/**
 * Destructor function for ks_dht_t.
 * Will deinitialize and deallocate internal state.
 * @param dht dereferenced in/out pointer to the dht instance, NULL upon return
 */
KS_DECLARE(void) ks_dht_destroy(ks_dht_t **dht);

/**
 * Enable or disable (default) autorouting support.
 * When enabled, autorouting will allow sending to remote addresses on interfaces which are not yet bound.
 * The address will be bound with the provided autoroute port when this occurs.
 * @param dht pointer to the dht instance
 * @param autoroute enable or disable autorouting
 * @param port when enabling autorouting this port will be used to bind new addresses, may be 0 to use the default DHT port
 */
KS_DECLARE(void) ks_dht_autoroute(ks_dht_t *dht, ks_bool_t autoroute, ks_port_t port);

/**
 * Register a callback for a specific message type.
 * Will overwrite any duplicate handlers.
 * @param dht pointer to the dht instance
 * @param value string of the type text under the 'y' key of a message
 * @param callback the callback to be called when a message matches
 */
KS_DECLARE(void) ks_dht_register_type(ks_dht_t *dht, const char *value, ks_dht_message_callback_t callback);

/**
 * Register a callback for a specific message query.
 * Will overwrite any duplicate handlers.
 * @param dht pointer to the dht instance
 * @param value string of the type text under the 'q' key of a message
 * @param callback the callback to be called when a message matches
 */
KS_DECLARE(void) ks_dht_register_query(ks_dht_t *dht, const char *value, ks_dht_message_callback_t callback);

/**
 * Register a callback for a specific message error.
 * Will overwrite any duplicate handlers.
 * @param dht pointer to the dht instance
 * @param value string of the errorcode under the first item of the 'e' key of a message
 * @param callback the callback to be called when a message matches
 */
KS_DECLARE(void) ks_dht_register_error(ks_dht_t *dht, const char *value, ks_dht_message_callback_t callback);

/**
 * Bind a local address and port for receiving UDP datagrams.
 * @param dht pointer to the dht instance
 * @param addr pointer to the local address information
 * @param endpoint dereferenced out pointer to the allocated endpoint, may be NULL to ignore endpoint output
 * @return The ks_status_t result: KS_STATUS_SUCCESS, KS_STATUS_FAIL, ...
 * @see ks_socket_option
 * @see ks_addr_bind
 * @see ks_dht_endpoint_alloc
 * @see ks_dht_endpoint_init
 * @see ks_hash_insert
 * @see ks_dhtrt_initroute
 * @see ks_dhtrt_create_node
 */
KS_DECLARE(ks_status_t) ks_dht_bind(ks_dht_t *dht, const ks_sockaddr_t *addr, ks_dht_endpoint_t **endpoint);

/**
 * Pulse the internals of dht.
 * Handles receiving UDP datagrams, dispatching processing, handles expirations, throttled message sending, route table pulsing, etc.
 * @param dht pointer to the dht instance
 * @param timeout timeout value used when polling sockets for new UDP datagrams
 */
KS_DECLARE(void) ks_dht_pulse(ks_dht_t *dht, int32_t timeout);


KS_DECLARE(char *) ks_dht_hex(const uint8_t *data, char *buffer, ks_size_t len);
KS_DECLARE(uint8_t *) ks_dht_dehex(uint8_t *data, const char *buffer, ks_size_t len);
						

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_storageitem_target_immutable(const uint8_t *value, ks_size_t value_length, ks_dht_nodeid_t *target);

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_storageitem_target_mutable(ks_dht_storageitem_pkey_t *pk, const uint8_t *salt, ks_size_t salt_length, ks_dht_nodeid_t *target);

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_storageitem_signature_generate(ks_dht_storageitem_signature_t *sig,
															  ks_dht_storageitem_skey_t *sk,
															  const uint8_t *salt,
															  ks_size_t salt_length,
															  int64_t sequence,
															  const uint8_t *value,
															  ks_size_t value_length);

/**
 *
 */
KS_DECLARE(void) ks_dht_storageitem_reference(ks_dht_storageitem_t *item);

/**
 *
 */
KS_DECLARE(void) ks_dht_storageitem_dereference(ks_dht_storageitem_t *item);

/**
 *
 */
KS_DECLARE(void) ks_dht_storageitem_callback(ks_dht_storageitem_t *item, ks_dht_storageitem_callback_t callback);
						
/**
 *
 */
KS_DECLARE(void) ks_dht_storageitems_read_lock(ks_dht_t *dht);

/**
 *
 */
KS_DECLARE(void) ks_dht_storageitems_read_unlock(ks_dht_t *dht);

/**
 *
 */
KS_DECLARE(void) ks_dht_storageitems_write_lock(ks_dht_t *dht);

/**
 *
 */
KS_DECLARE(void) ks_dht_storageitems_write_unlock(ks_dht_t *dht);

/**
 *
 */
KS_DECLARE(ks_dht_storageitem_t *) ks_dht_storageitems_find(ks_dht_t *dht, ks_dht_nodeid_t *target);

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_storageitems_insert(ks_dht_t *dht, ks_dht_storageitem_t *item);
						
/**
 *
 */
KS_DECLARE(void) ks_dht_ping(ks_dht_t *dht, const ks_sockaddr_t *raddr, ks_dht_job_callback_t callback, void *data);

/**
 *
 */
KS_DECLARE(void) ks_dht_findnode(ks_dht_t *dht,
								 const ks_sockaddr_t *raddr,
								 ks_dht_job_callback_t callback,
								 void *data,
								 ks_dht_nodeid_t *target);

/**
 *
 */
KS_DECLARE(void) ks_dht_get(ks_dht_t *dht,
							const ks_sockaddr_t *raddr,
							ks_dht_job_callback_t callback,
							void *data,
							ks_dht_nodeid_t *target,
							const uint8_t *salt,
							ks_size_t salt_length);

/**
 *
 */
KS_DECLARE(void) ks_dht_put(ks_dht_t *dht,
							const ks_sockaddr_t *raddr,
							ks_dht_job_callback_t callback,
							void *data,
							ks_dht_token_t *token,
							int64_t cas,
							ks_dht_storageitem_t *item);
						
/**
 * Create a network search of the closest nodes to a target.
 * @param dht pointer to the dht instance
 * @param family either AF_INET or AF_INET6 for the appropriate network to search
 * @param target pointer to the nodeid for the target to be searched
 * @param callback an optional callback to add to the search when it is finished
 * @param search dereferenced out pointer to the allocated search, may be NULL to ignore search output
 * @see ks_dht_search_create
 * @see ks_hash_insert
 * @see ks_dht_findnode
 */
KS_DECLARE(void) ks_dht_search(ks_dht_t *dht,
							   ks_dht_job_callback_t callback,
							   void *data,
							   ks_dhtrt_routetable_t *table,
							   ks_dht_nodeid_t *target);

KS_DECLARE(void) ks_dht_publish(ks_dht_t *dht,
								const ks_sockaddr_t *raddr,
								ks_dht_job_callback_t callback,
								void *data,
								int64_t cas,
								ks_dht_storageitem_t *item);

KS_DECLARE(void) ks_dht_distribute(ks_dht_t *dht,
								   ks_dht_storageitem_callback_t callback,
								   void *data,
								   ks_dhtrt_routetable_t *table,
								   int64_t cas,
								   ks_dht_storageitem_t *item);

/**
 * route table methods
 *
 */
KS_DECLARE(ks_status_t) ks_dhtrt_initroute(ks_dhtrt_routetable_t **tableP, 
											ks_dht_t *dht, 
											ks_pool_t *pool); 
KS_DECLARE(void) ks_dhtrt_deinitroute(ks_dhtrt_routetable_t **table);

KS_DECLARE(ks_status_t)        ks_dhtrt_create_node(ks_dhtrt_routetable_t* table,
													ks_dht_nodeid_t nodeid,
													enum ks_dht_nodetype_t type,
													char* ip, unsigned short port,
                                                    enum ks_create_node_flags_t flags,
													ks_dht_node_t** node);

KS_DECLARE(ks_status_t)        ks_dhtrt_delete_node(ks_dhtrt_routetable_t* table, ks_dht_node_t* node);

KS_DECLARE(ks_status_t)        ks_dhtrt_touch_node(ks_dhtrt_routetable_t* table,  ks_dht_nodeid_t nodeid);
KS_DECLARE(ks_status_t)        ks_dhtrt_expire_node(ks_dhtrt_routetable_t* table,  ks_dht_nodeid_t nodeid);

KS_DECLARE(uint8_t)            ks_dhtrt_findclosest_nodes(ks_dhtrt_routetable_t* table, ks_dhtrt_querynodes_t* query);
KS_DECLARE(ks_dht_node_t*)     ks_dhtrt_find_node(ks_dhtrt_routetable_t* table, ks_dht_nodeid_t id);

KS_DECLARE(ks_status_t)        ks_dhtrt_sharelock_node(ks_dht_node_t* node);
KS_DECLARE(ks_status_t)        ks_dhtrt_release_node(ks_dht_node_t* node);
KS_DECLARE(ks_status_t)        ks_dhtrt_release_querynodes(ks_dhtrt_querynodes_t* query);

KS_DECLARE(void)               ks_dhtrt_process_table(ks_dhtrt_routetable_t* table);

KS_DECLARE(uint32_t)           ks_dhtrt_serialize(ks_dhtrt_routetable_t* table, void** ptr);
KS_DECLARE(ks_status_t)        ks_dhtrt_deserialize(ks_dhtrt_routetable_t* table, void* ptr);
																							 
/* debugging aids */
KS_DECLARE(void)               ks_dhtrt_dump(ks_dhtrt_routetable_t* table, int level);

																																				
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
