#ifndef KS_DHT_H
#define KS_DHT_H

#include "ks.h"
#include "ks_bencode.h"
#include "sodium.h"

KS_BEGIN_EXTERN_C


#define KS_DHT_DEFAULT_PORT 5309
#define KS_DHT_RECV_BUFFER_SIZE 0xFFFF

#define KS_DHT_NODEID_SIZE 20

#define KS_DHT_MESSAGE_TRANSACTIONID_MAX_SIZE 20
#define KS_DHT_MESSAGE_TYPE_MAX_SIZE 20
#define KS_DHT_MESSAGE_QUERY_MAX_SIZE 20
#define KS_DHT_MESSAGE_ERROR_MAX_SIZE 256

#define KS_DHT_TRANSACTION_EXPIRATION_DELAY 30

#define KS_DHT_STORAGEITEM_KEY_SIZE crypto_sign_PUBLICKEYBYTES
#define KS_DHT_STORAGEITEM_SALT_MAX_SIZE 64
#define KS_DHT_STORAGEITEM_SIGNATURE_SIZE crypto_sign_BYTES

#define KS_DHT_TOKEN_SIZE SHA_DIGEST_LENGTH
#define KS_DHT_TOKENSECRET_EXPIRATION 300

typedef struct ks_dht_s ks_dht_t;
typedef struct ks_dht_nodeid_s ks_dht_nodeid_t;
typedef struct ks_dht_token_s ks_dht_token_t;
typedef struct ks_dht_storageitem_key_s ks_dht_storageitem_key_t;
typedef struct ks_dht_storageitem_signature_s ks_dht_storageitem_signature_t;
typedef struct ks_dht_message_s ks_dht_message_t;
typedef struct ks_dht_endpoint_s ks_dht_endpoint_t;
typedef struct ks_dht_transaction_s ks_dht_transaction_t;
typedef struct ks_dht_node_s ks_dht_node_t;
typedef struct ks_dhtrt_routetable_s ks_dhtrt_routetable_t;
typedef struct ks_dhtrt_querynodes_s ks_dhtrt_querynodes_t;
typedef struct ks_dht_storageitem_s ks_dht_storageitem_t;


typedef ks_status_t (*ks_dht_message_callback_t)(ks_dht_t *dht, ks_dht_message_t *message);

/**
 * Note: This must remain a structure for casting from raw data
 */
struct ks_dht_nodeid_s {
	uint8_t id[KS_DHT_NODEID_SIZE];
};

enum ipfamily { ifv4=AF_INET, ifv6=AF_INET6, ifboth=AF_INET+AF_INET6};
enum ks_dht_nodetype_t { ks_dht_remote_t=0x01, 
                         ks_dht_local_t=0x02, 
                         ks_dht_both_t=ks_dht_remote_t+ks_dht_local_t };

struct ks_dht_node_s {
    ks_dht_nodeid_t  nodeid;
    ks_sockaddr_t    addr;
    enum ipfamily    family;                  /* AF_INET or AF_INET6 */
    enum ks_dht_nodetype_t type;              /* local or remote */
    ks_dhtrt_routetable_t* table;
};

struct ks_dhtrt_routetable_s {
    void*       internal;                       
    ks_pool_t*  pool;                           
    ks_logger_t logger;
};

struct ks_dhtrt_querynodes_s {
    ks_dht_nodeid_t nodeid;                   /* in: id to query                   */
    enum ipfamily  family;                    /* in: AF_INET or AF_INET6 or both   */
    enum ks_dht_nodetype_t type;              /* remote, local, or  both           */
    uint8_t        max;                       /* in: maximum to return             */
    uint8_t        count;                     /* out: number returned              */
    ks_dht_node_t* nodes[ KS_DHT_MESSAGE_QUERY_MAX_SIZE]; /* out: array of peers (ks_dht_node_t* nodes[incount]) */
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
	char type[KS_DHT_MESSAGE_TYPE_MAX_SIZE];
	struct bencode *args;
};

struct ks_dht_endpoint_s {
	ks_pool_t *pool;
	ks_dht_nodeid_t nodeid;
	ks_sockaddr_t addr;
	ks_socket_t sock;
};

struct ks_dht_transaction_s {
	ks_pool_t *pool;
	ks_sockaddr_t raddr;
	uint32_t transactionid;
	ks_dht_message_callback_t callback;
	ks_time_t expiration;
	ks_bool_t finished;
};

struct ks_dht_storageitem_s {
	ks_pool_t *pool;
	ks_dht_nodeid_t id;

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

	ks_q_t *send_q;
	ks_dht_message_t *send_q_unsent;
	uint8_t recv_buffer[KS_DHT_RECV_BUFFER_SIZE];
	ks_size_t recv_buffer_length;

	volatile uint32_t transactionid_next;
	ks_hash_t *transactions_hash;

	ks_dhtrt_routetable_t *rt_ipv4;
	ks_dhtrt_routetable_t *rt_ipv6;

	volatile uint32_t token_secret_current;
	volatile uint32_t token_secret_previous;
	ks_time_t token_secret_expiration;
	ks_hash_t *storage_hash;
};

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_alloc(ks_dht_t **dht, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht_prealloc(ks_dht_t *dht, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht_free(ks_dht_t *dht);


KS_DECLARE(ks_status_t) ks_dht_init(ks_dht_t *dht);
KS_DECLARE(ks_status_t) ks_dht_deinit(ks_dht_t *dht);

KS_DECLARE(ks_status_t) ks_dht_autoroute(ks_dht_t *dht, ks_bool_t autoroute, ks_port_t port);

KS_DECLARE(ks_status_t) ks_dht_bind(ks_dht_t *dht, const ks_dht_nodeid_t *nodeid, const ks_sockaddr_t *addr, ks_dht_endpoint_t **endpoint);
KS_DECLARE(void) ks_dht_pulse(ks_dht_t *dht, int32_t timeout);


KS_DECLARE(ks_status_t) ks_dht_register_type(ks_dht_t *dht, const char *value, ks_dht_message_callback_t callback);
KS_DECLARE(ks_status_t) ks_dht_register_query(ks_dht_t *dht, const char *value, ks_dht_message_callback_t callback);

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_message_alloc(ks_dht_message_t **message, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht_message_prealloc(ks_dht_message_t *message, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht_message_free(ks_dht_message_t *message);

KS_DECLARE(ks_status_t) ks_dht_message_init(ks_dht_message_t *message, ks_dht_endpoint_t *ep, ks_sockaddr_t *raddr, ks_bool_t alloc_data);
KS_DECLARE(ks_status_t) ks_dht_message_deinit(ks_dht_message_t *message);

KS_DECLARE(ks_status_t) ks_dht_message_parse(ks_dht_message_t *message, const uint8_t *buffer, ks_size_t buffer_length);

KS_DECLARE(ks_status_t) ks_dht_message_query(ks_dht_message_t *message,
											 uint32_t transactionid,
											 const char *query,
											 struct bencode **args);
KS_DECLARE(ks_status_t) ks_dht_message_response(ks_dht_message_t *message,
												uint8_t *transactionid,
												ks_size_t transactionid_length,
												struct bencode **args);
KS_DECLARE(ks_status_t) ks_dht_message_error(ks_dht_message_t *message,
											 uint8_t *transactionid,
											 ks_size_t transactionid_length,
											 struct bencode **args);

/**
 *
 */

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_transaction_alloc(ks_dht_transaction_t **transaction, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht_transaction_prealloc(ks_dht_transaction_t *transaction, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht_transaction_free(ks_dht_transaction_t *transaction);

KS_DECLARE(ks_status_t) ks_dht_transaction_init(ks_dht_transaction_t *transaction,
												 ks_sockaddr_t *raddr,
												 uint32_t transactionid,
												 ks_dht_message_callback_t callback);
KS_DECLARE(ks_status_t) ks_dht_transaction_deinit(ks_dht_transaction_t *transaction);


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
