#include "ks_dht.h"
#include "ks_dht-int.h"

KS_DECLARE(ks_status_t) ks_dht_transaction_create(ks_dht_transaction_t **transaction,
												  ks_pool_t *pool,
												  ks_sockaddr_t *raddr,
												  uint32_t transactionid,
												  ks_dht_message_callback_t callback)
{
	ks_dht_transaction_t *t;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(transaction);
	ks_assert(pool);
	ks_assert(raddr);

	*transaction = t = ks_pool_alloc(pool, sizeof(ks_dht_transaction_t));
	ks_assert(t);

	t->pool = pool;
	t->raddr = *raddr;
	t->transactionid = transactionid;
	t->callback = callback;
	t->expiration = ks_time_now() + (KS_DHT_TRANSACTION_EXPIRATION * 1000);

	// done:
	if (ret != KS_STATUS_SUCCESS) {
		if (t) ks_dht_transaction_destroy(&t);
		*transaction = NULL;
	}
	return ret;
}

KS_DECLARE(void) ks_dht_transaction_destroy(ks_dht_transaction_t **transaction)
{
	ks_dht_transaction_t *t;

	ks_assert(transaction);
	ks_assert(*transaction);

	t = *transaction;

	ks_pool_free(t->pool, t);

	*transaction = NULL;
}

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
