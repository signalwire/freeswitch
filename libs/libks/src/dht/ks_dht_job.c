#include "ks_dht.h"
#include "ks_dht-int.h"

KS_DECLARE(ks_status_t) ks_dht_job_create(ks_dht_job_t **job,
										  ks_pool_t *pool,
										  const ks_sockaddr_t *raddr,
										  int32_t attempts,
										  void *data)
{
	ks_dht_job_t *j;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(job);
	ks_assert(pool);
	//ks_assert(dht);
	ks_assert(attempts > 0 && attempts <= 10);

	*job = j = ks_pool_alloc(pool, sizeof(ks_dht_job_t));
	ks_assert(j);

	j->pool = pool;
	j->state = KS_DHT_JOB_STATE_QUERYING;
	if (raddr) j->raddr = *raddr;
	j->attempts = attempts;
	j->data = data;

	// done:
	if (ret != KS_STATUS_SUCCESS) {
		if (j) ks_dht_job_destroy(job);
	}
	return ret;
}

KS_DECLARE(void) ks_dht_job_build_ping(ks_dht_job_t *job, ks_dht_job_callback_t query_callback, ks_dht_job_callback_t finish_callback)
{
	ks_assert(job);
	ks_assert(query_callback);

	job->query_callback = query_callback;
	job->finish_callback = finish_callback;
}

KS_DECLARE(void) ks_dht_job_build_findnode(ks_dht_job_t *job,
										   ks_dht_job_callback_t query_callback,
										   ks_dht_job_callback_t finish_callback,
										   ks_dht_nodeid_t *target)
{
	ks_assert(job);
	ks_assert(query_callback);
	ks_assert(target);

	job->query_callback = query_callback;
	job->finish_callback = finish_callback;
	job->query_target = *target;
}

KS_DECLARE(void) ks_dht_job_build_get(ks_dht_job_t *job,
									  ks_dht_job_callback_t query_callback,
									  ks_dht_job_callback_t finish_callback,
									  ks_dht_nodeid_t *target,
									  const uint8_t *salt,
									  ks_size_t salt_length)
{
	ks_assert(job);
	ks_assert(query_callback);
	ks_assert(target);

	job->query_callback = query_callback;
	job->finish_callback = finish_callback;
	job->query_target = *target;
	if (salt && salt_length > 0) job->query_salt = ben_blob(salt, salt_length);
}

KS_DECLARE(void) ks_dht_job_build_put(ks_dht_job_t *job,
									  ks_dht_job_callback_t query_callback,
									  ks_dht_job_callback_t finish_callback,
									  ks_dht_token_t *token,
									  int64_t cas,
									  ks_dht_storageitem_t *item)
{
	ks_assert(job);
	ks_assert(query_callback);
	ks_assert(token);
	ks_assert(item);

	job->query_callback = query_callback;
	job->finish_callback = finish_callback;
	job->query_token = *token;
	job->query_cas = cas;
	job->query_storageitem = item;
	ks_dht_storageitem_reference(job->query_storageitem);
}

KS_DECLARE(void) ks_dht_job_build_search(ks_dht_job_t *job,
										 ks_dht_job_callback_t query_callback,
										 ks_dht_job_callback_t finish_callback)
{
	ks_assert(job);
	ks_assert(query_callback);

	job->query_callback = query_callback;
	job->finish_callback = finish_callback;
}

KS_DECLARE(void) ks_dht_job_destroy(ks_dht_job_t **job)
{
	ks_dht_job_t *j;

	ks_assert(job);
	ks_assert(*job);

	j = *job;

	if (j->query_salt) ben_free(j->query_salt);
	if (j->response_id) ks_dhtrt_release_node(j->response_id);
	for (int32_t i = 0; i < j->response_nodes_count; ++i) ks_dhtrt_release_node(j->response_nodes[i]);
	for (int32_t i = 0; i < j->response_nodes6_count; ++i) ks_dhtrt_release_node(j->response_nodes6[i]);

	if (j->query_storageitem) ks_dht_storageitem_dereference(j->query_storageitem);
	if (j->response_storageitem) ks_dht_storageitem_dereference(j->response_storageitem);

	if (j->error_description) ben_free(j->error_description);

	ks_pool_free(j->pool, job);
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
