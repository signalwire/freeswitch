#ifndef KS_DHT_MESSAGE_H
#define KS_DHT_MESSAGE_H

#include "ks.h"

KS_BEGIN_EXTERN_C

#define KS_DHT_MESSAGE_TRANSACTIONID_MAX_SIZE 20
#define KS_DHT_MESSAGE_TYPE_MAX_SIZE 20

typedef struct ks_dht2_message_s ks_dht2_message_t;
struct ks_dht2_message_s {
	ks_pool_t *pool;
	struct bencode *data;
    uint8_t transactionid[KS_DHT_MESSAGE_TRANSACTIONID_MAX_SIZE];
	ks_size_t transactionid_length;
	char type[KS_DHT_MESSAGE_TYPE_MAX_SIZE];
};

KS_DECLARE(ks_status_t) ks_dht2_message_alloc(ks_dht2_message_t **message, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht2_message_prealloc(ks_dht2_message_t *message, ks_pool_t *pool);
KS_DECLARE(ks_status_t) ks_dht2_message_free(ks_dht2_message_t *message);

KS_DECLARE(ks_status_t) ks_dht2_message_init(ks_dht2_message_t *message, const uint8_t *buffer, ks_size_t buffer_length);
KS_DECLARE(ks_status_t) ks_dht2_message_deinit(ks_dht2_message_t *message);

KS_END_EXTERN_C

#endif /* KS_DHT_MESSAGE_H */

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

