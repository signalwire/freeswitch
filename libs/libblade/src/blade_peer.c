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
	BP_NONE = 0,
	BP_MYPOOL = (1 << 0),
	BP_MYTPOOL = (1 << 1)
} bppvt_flag_t;

struct blade_peer_s {
	bppvt_flag_t flags;
	ks_pool_t *pool;
	ks_thread_pool_t *tpool;
	blade_service_t *service;

	ks_bool_t shutdown;
	kws_t *kws;
	ks_thread_t *kws_thread;

	ks_q_t *messages_sending;
	ks_q_t *messages_receiving;
};


void *blade_peer_kws_thread(ks_thread_t *thread, void *data);


KS_DECLARE(ks_status_t) blade_peer_destroy(blade_peer_t **bpP)
{
	blade_peer_t *bp = NULL;
	bppvt_flag_t flags;
	ks_pool_t *pool;

	ks_assert(bpP);

	bp = *bpP;
	*bpP = NULL;

	ks_assert(bp);

	flags = bp->flags;
	pool = bp->pool;

	blade_peer_shutdown(bp);

	ks_q_destroy(&bp->messages_sending);
	ks_q_destroy(&bp->messages_receiving);

	if (bp->tpool && (flags & BP_MYTPOOL)) ks_thread_pool_destroy(&bp->tpool);
	
	ks_pool_free(bp->pool, &bp);

	if (pool && (flags & BP_MYPOOL)) ks_pool_close(&pool);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_peer_create(blade_peer_t **bpP, ks_pool_t *pool, ks_thread_pool_t *tpool, blade_service_t *service)
{
	bppvt_flag_t newflags = BP_NONE;
	blade_peer_t *bp = NULL;

	ks_assert(bpP);
	ks_assert(service);

	if (!pool) {
		newflags |= BP_MYPOOL;
		ks_pool_open(&pool);
		ks_assert(pool);
	}
	if (!tpool) {
		newflags |= BP_MYTPOOL;
		ks_thread_pool_create(&tpool, BLADE_PEER_TPOOL_MIN, BLADE_PEER_TPOOL_MAX, BLADE_PEER_TPOOL_STACK, KS_PRI_NORMAL, BLADE_PEER_TPOOL_IDLE);
		ks_assert(tpool);
	}

	bp = ks_pool_alloc(pool, sizeof(*bp));
	bp->flags = newflags;
	bp->pool = pool;
	bp->tpool = tpool;
	bp->service = service;
	ks_q_create(&bp->messages_sending, pool, 0);
	ks_q_create(&bp->messages_receiving, pool, 0);
	*bpP = bp;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_peer_startup(blade_peer_t *bp, kws_t *kws)
{
	ks_assert(bp);
	ks_assert(kws);

	// @todo: consider using a recycle queue for blade_peer_t in blade_service_t, just need to call startup then
	
	blade_peer_shutdown(bp);
	
	bp->kws = kws;

    if (ks_thread_create_ex(&bp->kws_thread,
							blade_peer_kws_thread,
							bp,
							KS_THREAD_FLAG_DEFAULT,
							KS_THREAD_DEFAULT_STACK,
							KS_PRI_NORMAL,
							bp->pool) != KS_STATUS_SUCCESS) return KS_STATUS_FAIL;
	
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_peer_shutdown(blade_peer_t *bp)
{
	ks_assert(bp);

	bp->shutdown = KS_TRUE;
	
	if (bp->kws_thread) {
		ks_thread_join(bp->kws_thread);
		ks_pool_free(bp->pool, &bp->kws_thread);
	}
	
	if (bp->kws) kws_destroy(&bp->kws);
	
	bp->shutdown = KS_FALSE;
	return KS_STATUS_SUCCESS;
}

void *blade_peer_kws_thread(ks_thread_t *thread, void *data)
{
	blade_peer_t *peer;
	kws_opcode_t opcode;
	uint8_t *data;
	ks_size_t data_len;
	blade_message_t *message;

	ks_assert(thread);
	ks_assert(data);

	peer = (blade_peer_t *)data;

	while (!peer->shutdown) {
		// @todo use nonblocking kws mode so that if no data at all is available yet we can still do other things such as sending messages before trying again
		// or easier alternative, just use ks_poll (or select) to check if there is a POLLIN event pending, but this requires direct access to the socket, or
		// kws can be updated to add a function to poll the inner socket for events (IE, kws_poll(kws, &inbool, NULL, &errbool, timeout))
		data_len = kws_read_frame(peer->kws, &opcode, &data);

		if (data_len <= 0) {
			// @todo error handling, strerror(ks_errno())
			// 0 means socket closed with WS_NONE, which closes websocket with no additional reason
			// -1 means socket closed with a general failure
			// -2 means nonblocking wait
			// other values are based on WS_XXX reasons
			// negative values are based on reasons, except for -1 is but -2 is nonblocking wait, and
			
			// @todo: this way of disconnecting would have the service periodically check the list of connected peers for those that are disconnecting,
			// remove them from the connected peer list, and then call peer destroy which will wait for this thread to rejoin which it already will have,
			// and then destroy the inner kws and finish any cleanup of the actual socket if neccessary, and can still call an ondisconnected callback
			// at the service level
			peer->disconnecting = KS_TRUE;
			break;
		}

		// @todo this will check the discarded queue first and realloc if there is not enough space, otherwise allocate a message, and finally copy the data
		if (blade_handle_message_claim(peer->service->handle, &message, data, data_len) != KS_STATUS_SUCCESS || !message) {
			// @todo error handling
			// just drop the peer for now, the only failure scenarios are asserted OOM, or if the discard queue pop fails
			peer->disconnecting = KS_TRUE;
			break;
		}
		
		ks_q_push(peer->messages_receiving, message);
		// @todo callback up the stack to indicate a message has been received and can be popped (more efficient than constantly polling by popping)?


		if (ks_q_trypop(peer->messages_sending, &message) == KS_STATUS_SUCCESS) {
		}
	}
	
	return NULL;
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
