#include "ks_dht.h"
#include "ks_dht-int.h"

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_transaction_alloc(ks_dht2_transaction_t **transaction, ks_pool_t *pool)
{
	ks_dht2_transaction_t *tran;

	ks_assert(transaction);
	ks_assert(pool);
	
	*transaction = tran = ks_pool_alloc(pool, sizeof(ks_dht2_transaction_t));
	tran->pool = pool;

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_transaction_prealloc(ks_dht2_transaction_t *transaction, ks_pool_t *pool)
{
	ks_assert(transaction);
	ks_assert(pool);
	
	transaction->pool = pool;

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_transaction_free(ks_dht2_transaction_t *transaction)
{
	ks_assert(transaction);

	ks_dht2_transaction_deinit(transaction);
	ks_pool_free(transaction->pool, transaction);

	return KS_STATUS_SUCCESS;
}
												

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_transaction_init(ks_dht2_transaction_t *transaction,
												 uint16_t transactionid,
												 ks_dht2_message_callback_t callback)
{
	ks_assert(transaction);
	ks_assert(transaction->pool);
	ks_assert(callback);

	transaction->transactionid = transactionid;
	transaction->callback = callback;

	return KS_STATUS_SUCCESS;
}

/**
 *
 */
KS_DECLARE(ks_status_t) ks_dht2_transaction_deinit(ks_dht2_transaction_t *transaction)
{
	ks_assert(transaction);

	transaction->transactionid = 0;
	transaction->callback = NULL;

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
