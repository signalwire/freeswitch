#include "ks_dht.h"
#include "ks_dht-int.h"

KS_DECLARE(ks_status_t) ks_dht_transaction_create(ks_dht_transaction_t **transaction,
												  ks_pool_t *pool,
												  ks_dht_job_t *job,
												  uint32_t transactionid,
												  ks_dht_job_callback_t callback)
{
	ks_dht_transaction_t *t;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(transaction);
	ks_assert(pool);
	ks_assert(job);

	*transaction = t = ks_pool_alloc(pool, sizeof(ks_dht_transaction_t));
	ks_assert(t);

	t->pool = pool;
	t->job = job;
	t->transactionid = transactionid;
	t->callback = callback;
	t->expiration = ks_time_now() + ((ks_time_t)KS_DHT_TRANSACTION_EXPIRATION * KS_USEC_PER_SEC);

	// done:
	if (ret != KS_STATUS_SUCCESS) {
		if (t) ks_dht_transaction_destroy(transaction);
	}
	return ret;
}

KS_DECLARE(void) ks_dht_transaction_destroy(ks_dht_transaction_t **transaction)
{
	ks_dht_transaction_t *t;

	ks_assert(transaction);
	ks_assert(*transaction);

	t = *transaction;

	ks_pool_free(t->pool, transaction);
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
