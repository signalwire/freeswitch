#include "ks_dht.h"
#include "ks_dht-int.h"

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_transaction_alloc(ks_dht_transaction_t **transaction, ks_pool_t *pool)
{
	ks_dht_transaction_t *tran;

	ks_assert(transaction);
	ks_assert(pool);

	*transaction = tran = ks_pool_alloc(pool, sizeof(ks_dht_transaction_t));
	tran->pool = pool;

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_transaction_prealloc(ks_dht_transaction_t *transaction, ks_pool_t *pool)
{
	ks_assert(transaction);
	ks_assert(pool);

	memset(transaction, 0, sizeof(ks_dht_transaction_t));

	transaction->pool = pool;

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_transaction_free(ks_dht_transaction_t **transaction)
{
	ks_assert(transaction);
	ks_assert(*transaction);

	ks_dht_transaction_deinit(*transaction);
	ks_pool_free((*transaction)->pool, *transaction);

	*transaction = NULL;

	return KS_STATUS_SUCCESS;
}
												

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_transaction_init(ks_dht_transaction_t *transaction,
												ks_sockaddr_t *raddr,
												uint32_t transactionid,
												ks_dht_message_callback_t callback)
{
	ks_assert(transaction);
	ks_assert(raddr);
	ks_assert(transaction->pool);
	ks_assert(callback);

	transaction->raddr = *raddr;
	transaction->transactionid = transactionid;
	transaction->callback = callback;
	transaction->expiration = ks_time_now_sec() + KS_DHT_TRANSACTION_EXPIRATION_DELAY;
	transaction->finished = KS_FALSE;

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht_transaction_deinit(ks_dht_transaction_t *transaction)
{
	ks_assert(transaction);

	transaction->raddr = (const ks_sockaddr_t){ 0 };
	transaction->transactionid = 0;
	transaction->callback = NULL;
	transaction->expiration = 0;
	transaction->finished = KS_FALSE;

	return KS_STATUS_SUCCESS;
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
