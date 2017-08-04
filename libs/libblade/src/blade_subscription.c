/*
 * Copyright (c) 2017, Shane Bryldt
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "blade.h"

struct blade_subscription_s {
	const char *protocol;
	const char *realm;
	const char *channel;
	ks_hash_t *subscribers;

	blade_rpc_request_callback_t callback;
	void *callback_data;
};


static void blade_subscription_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_subscription_t *bsub = (blade_subscription_t *)ptr;

	ks_assert(bsub);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		if (bsub->protocol) ks_pool_free(&bsub->protocol);
		if (bsub->realm) ks_pool_free(&bsub->subscribers);
		if (bsub->channel) ks_pool_free(&bsub->channel);
		if (bsub->subscribers) ks_hash_destroy(&bsub->subscribers);
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_subscription_create(blade_subscription_t **bsubP, ks_pool_t *pool, const char *protocol, const char *realm, const char *channel)
{
	blade_subscription_t *bsub = NULL;

	ks_assert(bsubP);
	ks_assert(pool);
	ks_assert(protocol);
	ks_assert(realm);
	ks_assert(channel);

	bsub = ks_pool_alloc(pool, sizeof(blade_subscription_t));
	bsub->protocol = ks_pstrdup(pool, protocol);
	bsub->realm = ks_pstrdup(pool, realm);
	bsub->channel = ks_pstrdup(pool, channel);

	ks_hash_create(&bsub->subscribers, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_NOLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, pool);
	ks_assert(bsub->subscribers);

	ks_pool_set_cleanup(bsub, NULL, blade_subscription_cleanup);

	*bsubP = bsub;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_subscription_destroy(blade_subscription_t **bsubP)
{
	blade_subscription_t *bsub = NULL;
	
	ks_assert(bsubP);
	ks_assert(*bsubP);

	bsub = *bsubP;

	ks_pool_free(bsubP);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(const char *) blade_subscription_protocol_get(blade_subscription_t *bsub)
{
	ks_assert(bsub);

	return bsub->protocol;

}

KS_DECLARE(const char *) blade_subscription_realm_get(blade_subscription_t *bsub)
{
	ks_assert(bsub);

	return bsub->realm;

}

KS_DECLARE(const char *) blade_subscription_channel_get(blade_subscription_t *bsub)
{
	ks_assert(bsub);

	return bsub->channel;

}

KS_DECLARE(ks_hash_t *) blade_subscription_subscribers_get(blade_subscription_t *bsub)
{
	ks_assert(bsub);

	return bsub->subscribers;

}

KS_DECLARE(ks_status_t) blade_subscription_subscribers_add(blade_subscription_t *bsub, const char *nodeid)
{
	char *key = NULL;

	ks_assert(bsub);
	ks_assert(nodeid);

	key = ks_pstrdup(ks_pool_get(bsub), nodeid);
	ks_hash_insert(bsub->subscribers, (void *)key, (void *)KS_TRUE);

	return KS_STATUS_SUCCESS;

}

KS_DECLARE(ks_status_t) blade_subscription_subscribers_remove(blade_subscription_t *bsub, const char *nodeid)
{
	ks_assert(bsub);
	ks_assert(nodeid);

	ks_hash_remove(bsub->subscribers, (void *)nodeid);

	return KS_STATUS_SUCCESS;

}

KS_DECLARE(blade_rpc_request_callback_t) blade_subscription_callback_get(blade_subscription_t *bsub)
{
	ks_assert(bsub);

	return bsub->callback;
}

KS_DECLARE(void) blade_subscription_callback_set(blade_subscription_t *bsub, blade_rpc_request_callback_t callback)
{
	ks_assert(bsub);

	bsub->callback = callback;
}

KS_DECLARE(void *) blade_subscription_callback_data_get(blade_subscription_t *bsub)
{
	ks_assert(bsub);

	return bsub->callback_data;
}

KS_DECLARE(void) blade_subscription_callback_data_set(blade_subscription_t *bsub, void *data)
{
	ks_assert(bsub);

	bsub->callback_data = data;
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
