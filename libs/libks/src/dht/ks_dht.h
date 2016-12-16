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
#define KS_DHT_PULSE_EXPIRATIONS 10

#define KS_DHT_NODEID_SIZE 20

#define KS_DHT_MESSAGE_TRANSACTIONID_MAX_SIZE 20
#define KS_DHT_MESSAGE_TYPE_MAX_SIZE 20
#define KS_DHT_MESSAGE_QUERY_MAX_SIZE 20
#define KS_DHT_MESSAGE_ERROR_MAX_SIZE 256

#define KS_DHT_TRANSACTION_EXPIRATION 30
#define KS_DHT_SEARCH_EXPIRATION 10
#define KS_DHT_SEARCH_RESULTS_MAX_SIZE 8 // @todo replace with KS_DHTRT_BUCKET_SIZE

#define KS_DHT_STORAGEITEM_KEY_SIZE crypto_sign_PUBLICKEYBYTES
#define KS_DHT_STORAGEITEM_SALT_MAX_SIZE 64
#define KS_DHT_STORAGEITEM_SIGNATURE_SIZE crypto_sign_BYTES

#define KS_DHT_TOKEN_SIZE SHA_DIGEST_LENGTH
#define KS_DHT_TOKENSECRET_EXPIRATION 300

#define  KS_DHTRT_MAXQUERYSIZE 20

typedef struct ks_dht_s ks_dht_t;
typedef struct ks_dht_datagram_s ks_dht_datagram_t;
typedef struct ks_dht_nodeid_s ks_dht_nodeid_t;
typedef struct ks_dht_token_s ks_dht_token_t;
typedef struct ks_dht_storageitem_key_s ks_dht_storageitem_key_t;
typedef struct ks_dht_storageitem_signature_s ks_dht_storageitem_signature_t;
typedef struct ks_dht_message_s ks_dht_message_t;
typedef struct ks_dht_endpoint_s ks_dht_endpoint_t;
typedef struct ks_dht_transaction_s ks_dht_transaction_t;
typedef struct ks_dht_search_s ks_dht_search_t;
typedef struct ks_dht_search_pending_s ks_dht_search_pending_t;
typedef struct ks_dht_node_s ks_dht_node_t;
typedef struct ks_dhtrt_routetable_s ks_dhtrt_routetable_t;
typedef struct ks_dhtrt_querynodes_s ks_dhtrt_querynodes_t;
typedef struct ks_dht_storageitem_s ks_dht_storageitem_t;


typedef ks_status_t (*ks_dht_message_callback_t)(ks_dht_t *dht, ks_dht_message_t *message);
typedef ks_status_t (*ks_dht_search_callback_t)(ks_dht_t *dht, ks_dht_search_t *search);

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

struct ks_dht_node_s {
    ks_dht_nodeid_t  nodeid;
    ks_sockaddr_t    addr;
    enum ks_afflags_t family;                  /* AF_INET or AF_INET6 */
    enum ks_dht_nodetype_t type;               /* local or remote */
    ks_dhtrt_routetable_t* table;
    ks_rwl_t        *reflock;          
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

struct ks_dht_token_s {
	uint8_t token[KS_DHT_TOKEN_SIZE];
};

struct ks_dht_storageitem_key_s {
	uint8_t key[KS_DHT_STORAGEITEM_KEY_SIZE];
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
};

struct ks_dht_endpoint_s {
	ks_pool_t *pool;
	ks_dht_nodeid_t nodeid;
	ks_sockaddr_t addr;
	ks_socket_t sock;
	// @todo make sure this node is unlocked, and never gets destroyed, should also never use local nodes in search results as they can be internal
	// network addresses, not what others have contacted through
	ks_dht_node_t *node;
};

struct ks_dht_transaction_s {
	ks_pool_t *pool;
	ks_sockaddr_t raddr;
	uint32_t transactionid;
	ks_dht_nodeid_t target;
	ks_dht_message_callback_t callback;
	ks_time_t expiration;
	ks_bool_t finished;
};

// Check if search already exists for the target id, if so add another callback, must be a popular target id
// Otherwise create new search, set target id, add callback, and insert the search into the dht search_hash with target id key
// Get closest local nodes to target id, check against results, send_findnode for closer nodes and add to pending hash with queried node id
// Upon receiving find_node response, check target id against dht search_hash, check responding node id against pending hash, set finished for purging
// Update results if responding node id is closer than any current result, or the results are not full
// Check response nodes against results, send_findnode for closer nodes and add to pending hash with an expiration
// Pulse expirations purges expired and finished from pending hash, once hash is empty callbacks are called providing results array
// Note:
// During the lifetime of a search, the ks_dht_node_t's must be kept alive
// Do a query touch on nodes prior to being added to pending, this should reset timeout and keep the nodes alive long enough even if they are dubious
// Nodes which land in results are known good with recent response to find_nodes and should be around for a while before route table worries about cleanup
struct ks_dht_search_s {
	ks_pool_t *pool;
	ks_mutex_t *mutex;
	ks_dht_nodeid_t target;
	ks_dht_search_callback_t *callbacks;
	ks_size_t callbacks_size;
	ks_hash_t *pending;
	ks_dht_nodeid_t results[KS_DHT_SEARCH_RESULTS_MAX_SIZE];
	ks_dht_nodeid_t distances[KS_DHT_SEARCH_RESULTS_MAX_SIZE];
	ks_size_t results_length;
};

struct ks_dht_search_pending_s {
	ks_pool_t *pool;
	ks_dht_nodeid_t nodeid;
	ks_time_t expiration;
	ks_bool_t finished;
};

struct ks_dht_storageitem_s {
	ks_pool_t *pool;
	ks_dht_nodeid_t id;
	// @todo ks_time_t expiration;
	struct bencode *v;
	
	ks_bool_t mutable;
	ks_dht_storageitem_key_t pk;
	ks_dht_storageitem_key_t sk;
	uint8_t salt[KS_DHT_STORAGEITEM_SALT_MAX_SIZE];
	ks_size_t salt_length;
	int64_t seq;
	ks_dht_storageitem_signature_t sig;
};

struct ks_dht_s {
	ks_pool_t *pool;
	ks_bool_t pool_alloc;

	ks_thread_pool_t *tpool;
	ks_bool_t tpool_alloc;

	ks_bool_t autoroute;
	ks_port_t autoroute_port;
	
	ks_hash_t *registry_type;
	ks_hash_t *registry_query;
	ks_hash_t *registry_error;

	ks_bool_t bind_ipv4;
	ks_bool_t bind_ipv6;

	ks_dht_endpoint_t **endpoints;
	int32_t endpoints_size;
	ks_hash_t *endpoints_hash;
	struct pollfd *endpoints_poll;

	ks_time_t pulse_expirations;

	ks_q_t *send_q;
	ks_dht_message_t *send_q_unsent;
	uint8_t recv_buffer[KS_DHT_DATAGRAM_BUFFER_SIZE + 1]; // Add 1, if we receive it then overflow error
	ks_size_t recv_buffer_length;

	ks_mutex_t *tid_mutex;
	volatile uint32_t transactionid_next;
	ks_hash_t *transactions_hash;

	ks_dhtrt_routetable_t *rt_ipv4;
	ks_dhtrt_routetable_t *rt_ipv6;

	ks_hash_t *search_hash;

	volatile uint32_t token_secret_current;
	volatile uint32_t token_secret_previous;
	ks_time_t token_secret_expiration;
	ks_hash_t *storage_hash;
};

/**
 * Constructor function for ks_dht_t.
 * Will allocate and initialize internal state including registration of message handlers.
 * @param dht dereferenced out pointer to the allocated dht instance
 * @param pool pointer to the memory pool used by the dht instance, may be NULL to create a new memory pool internally
 * @param tpool pointer to a thread pool used by the dht instance, may be NULL to create a new thread pool internally
 * @return The ks_status_t result: KS_STATUS_SUCCESS, KS_STATUS_NO_MEM
 */
KS_DECLARE(ks_status_t) ks_dht_create(ks_dht_t **dht, ks_pool_t *pool, ks_thread_pool_t *tpool);
						
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
 * @return The ks_status_t result: KS_STATUS_SUCCESS, KS_STATUS_FAIL
 */
KS_DECLARE(ks_status_t) ks_dht_register_type(ks_dht_t *dht, const char *value, ks_dht_message_callback_t callback);

/**
 * Register a callback for a specific message query.
 * Will overwrite any duplicate handlers.
 * @param dht pointer to the dht instance
 * @param value string of the type text under the 'q' key of a message
 * @param callback the callback to be called when a message matches
 * @return The ks_status_t result: KS_STATUS_SUCCESS, KS_STATUS_FAIL
 */
KS_DECLARE(ks_status_t) ks_dht_register_query(ks_dht_t *dht, const char *value, ks_dht_message_callback_t callback);

/**
 * Register a callback for a specific message error.
 * Will overwrite any duplicate handlers.
 * @param dht pointer to the dht instance
 * @param value string of the errorcode under the first item of the 'e' key of a message
 * @param callback the callback to be called when a message matches
 * @return The ks_status_t result: KS_STATUS_SUCCESS, KS_STATUS_FAIL
 */
KS_DECLARE(ks_status_t) ks_dht_register_error(ks_dht_t *dht, const char *value, ks_dht_message_callback_t callback);

/**
 * Bind a local address and port for receiving UDP datagrams.
 * @param dht pointer to the dht instance
 * @param nodeid pointer to a nodeid for this endpoint, may be NULL to generate one randomly
 * @param addr pointer to the local address information
 * @param dereferenced out pointer to the allocated endpoint, may be NULL to ignore endpoint output
 * @return The ks_status_t result: KS_STATUS_SUCCESS, KS_STATUS_FAIL, ...
 * @see ks_socket_option
 * @see ks_addr_bind
 * @see ks_dht_endpoint_alloc
 * @see ks_dht_endpoint_init
 * @see ks_hash_insert
 * @see ks_dhtrt_initroute
 * @see ks_dhtrt_create_node
 */
KS_DECLARE(ks_status_t) ks_dht_bind(ks_dht_t *dht, const ks_dht_nodeid_t *nodeid, const ks_sockaddr_t *addr, ks_dht_endpoint_t **endpoint);

/**
 * Pulse the internals of dht.
 * Handles receiving UDP datagrams, dispatching processing, handles expirations, throttled message sending, route table pulsing, etc.
 * @param dht pointer to the dht instance
 * @param timeout timeout value used when polling sockets for new UDP datagrams
 */
KS_DECLARE(void) ks_dht_pulse(ks_dht_t *dht, int32_t timeout);


/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_message_create(ks_dht_message_t **message,
											  ks_pool_t *pool,
											  ks_dht_endpoint_t *endpoint,
											  ks_sockaddr_t *raddr,
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
KS_DECLARE(ks_status_t) ks_dht_message_query(ks_dht_message_t *message,
											 uint32_t transactionid,
											 const char *query,
											 struct bencode **args);

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_message_response(ks_dht_message_t *message,
												uint8_t *transactionid,
												ks_size_t transactionid_length,
												struct bencode **args);

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_message_error(ks_dht_message_t *message,
											 uint8_t *transactionid,
											 ks_size_t transactionid_length,
											 struct bencode **args);


/**
 * route table methods
 *
 */
KS_DECLARE(ks_status_t) ks_dhtrt_initroute(ks_dhtrt_routetable_t **tableP, ks_pool_t *pool);
KS_DECLARE(void) ks_dhtrt_deinitroute(ks_dhtrt_routetable_t **table);

KS_DECLARE(ks_status_t)        ks_dhtrt_create_node(ks_dhtrt_routetable_t* table,
													ks_dht_nodeid_t nodeid,
													enum ks_dht_nodetype_t type,
													char* ip, unsigned short port,
													ks_dht_node_t** node);

KS_DECLARE(ks_status_t)        ks_dhtrt_delete_node(ks_dhtrt_routetable_t* table, ks_dht_node_t* node);

KS_DECLARE(ks_status_t)        ks_dhtrt_touch_node(ks_dhtrt_routetable_t* table,  ks_dht_nodeid_t nodeid);
KS_DECLARE(ks_status_t)        ks_dhtrt_expire_node(ks_dhtrt_routetable_t* table,  ks_dht_nodeid_t nodeid);

KS_DECLARE(uint8_t)            ks_dhtrt_findclosest_nodes(ks_dhtrt_routetable_t* table, ks_dhtrt_querynodes_t* query);
KS_DECLARE(ks_dht_node_t*)     ks_dhtrt_find_node(ks_dhtrt_routetable_t* table, ks_dht_nodeid_t id);

KS_DECLARE(ks_status_t)        ks_dhtrt_release_node(ks_dht_node_t* node);
KS_DECLARE(ks_status_t)        ks_dhtrt_release_querynodes(ks_dhtrt_querynodes_t* query);

KS_DECLARE(void)               ks_dhtrt_process_table(ks_dhtrt_routetable_t* table);

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
