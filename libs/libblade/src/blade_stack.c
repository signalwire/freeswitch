/*
 * Copyright (c) 2007-2014, Anthony Minessale II
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

typedef enum {
	BH_NONE = 0,
	BH_MYPOOL = (1 << 0),
	BH_MYTPOOL = (1 << 1)
} bhpvt_flag_t;

struct blade_handle_s {
	bhpvt_flag_t flags;
	ks_pool_t *pool;
	ks_thread_pool_t *tpool;
	blade_peer_t *peer;
};


KS_DECLARE(ks_status_t) blade_handle_destroy(blade_handle_t **bhP)
{
	blade_handle_t *bh = NULL;
	bhpvt_flag_t flags;
	ks_pool_t *pool;

	ks_assert(bhP);

	bh = *bhP;
	*bhP = NULL;

	ks_assert(bh);

	flags = bh->flags;
	pool = bh->pool;

	blade_peer_destroy(&bh->peer);
    if (bh->tpool && (flags & BH_MYTPOOL)) ks_thread_pool_destroy(&bh->tpool);

	ks_pool_free(bh->pool, &bh);

	if (pool && (flags & BH_MYPOOL)) {
		ks_pool_close(&pool);
	}

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_handle_create(blade_handle_t **bhP, ks_pool_t *pool, ks_thread_pool_t *tpool, const char *nodeid)
{
	bhpvt_flag_t newflags = BH_NONE;
	blade_handle_t *bh = NULL;
	ks_dht_nodeid_t nid;

	ks_assert(nodeid);
	ks_assert(strlen(nodeid) == (KS_DHT_NODEID_SIZE * 2));

	if (!pool) {
		newflags |= BH_MYPOOL;
		ks_pool_open(&pool);
	}
    if (!tpool) {
		newflags |= BH_MYTPOOL;
		ks_thread_pool_create(&tpool, BLADE_HANDLE_TPOOL_MIN, BLADE_HANDLE_TPOOL_MAX, BLADE_HANDLE_TPOOL_STACK, KS_PRI_NORMAL, BLADE_HANDLE_TPOOL_IDLE);
		ks_assert(tpool);
	}
	
	bh = ks_pool_alloc(pool, sizeof(*bh));
	bh->flags = newflags;
	bh->pool = pool;
	bh->tpool = tpool;
	ks_dht_dehex(nid.id, nodeid, KS_DHT_NODEID_SIZE);
	blade_peer_create(&bh->peer, bh->pool, bh->tpool, &nid);

	*bhP = bh;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(void) blade_handle_myid(blade_handle_t *bh, char *buffer)
{
	ks_dht_nodeid_t *nodeid = NULL;

	ks_assert(bh);
	ks_assert(bh->peer);

	nodeid = blade_peer_myid(bh->peer);
	ks_dht_hex(nodeid->id, buffer, KS_DHT_NODEID_SIZE);
}

KS_DECLARE(void) blade_handle_autoroute(blade_handle_t *bh, ks_bool_t autoroute, ks_port_t port)
{
	ks_assert(bh);
	ks_assert(bh->peer);

	blade_peer_autoroute(bh->peer, autoroute, port);
}

KS_DECLARE(ks_status_t) blade_handle_bind(blade_handle_t *bh, const char *ip, ks_port_t port, ks_dht_endpoint_t **endpoint)
{
	ks_sockaddr_t addr;
	int family = AF_INET;
	
	ks_assert(bh);
	ks_assert(ip);
	ks_assert(port);

	if (ip[1] != '.' && ip[2] != '.' && ip[3] != '.') family = AF_INET6;
	
	ks_addr_set(&addr, ip, port, family);
	return blade_peer_bind(bh->peer, &addr, endpoint);
}

KS_DECLARE(void) blade_handle_pulse(blade_handle_t *bh, int32_t timeout)
{
	ks_assert(bh);
	ks_assert(timeout >= 0);

	blade_peer_pulse(bh->peer, timeout);
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
