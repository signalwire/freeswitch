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

	ks_socket_t sock;
	ks_bool_t shutdown;
	ks_bool_t disconnecting;
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

KS_DECLARE(ks_status_t) blade_peer_startup(blade_peer_t *bp, ks_socket_t sock)
{
	kws_t *kws = NULL;
	
	ks_assert(bp);
	ks_assert(kws);

	// @todo: consider using a recycle queue for blade_peer_t in blade_service_t, just need to call startup then
	
	blade_peer_shutdown(bp);
	
	bp->sock = sock;

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
	blade_message_t *message = NULL;
	
	ks_assert(bp);

	bp->shutdown = KS_TRUE;
	
	if (bp->kws_thread) {
		ks_thread_join(bp->kws_thread);
		ks_pool_free(bp->pool, &bp->kws_thread);
	}

	while (ks_q_trypop(bp->messages_sending, (void **)&message) == KS_STATUS_SUCCESS && message) blade_message_discard(&message);
	while (ks_q_trypop(bp->messages_receiving, (void **)&message) == KS_STATUS_SUCCESS && message) blade_message_discard(&message);
	
	if (bp->kws) kws_destroy(&bp->kws);
	else if (bp->sock != KS_SOCK_INVALID) ks_socket_close(&bp->sock);
	bp->sock = KS_SOCK_INVALID;
	
	bp->shutdown = KS_FALSE;
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(void) blade_peer_disconnect(blade_peer_t *bp)
{
	ks_assert(bp);

	bp->disconnecting = KS_TRUE;
}

KS_DECLARE(ks_bool_t) blade_peer_disconnecting(blade_peer_t *bp)
{
	ks_assert(bp);
	return bp->disconnecting;
}

KS_DECLARE(ks_status_t) blade_peer_message_pop(blade_peer_t *peer, blade_message_t **message)
{
	ks_assert(peer);
	ks_assert(message);

	*message = NULL;
	return ks_q_trypop(peer->messages_receiving, (void **)message);
}

KS_DECLARE(ks_status_t) blade_peer_message_push(blade_peer_t *peer, void *data, ks_size_t data_length)
{
	blade_message_t *message = NULL;

	ks_assert(peer);
	ks_assert(data);
	ks_assert(data_length > 0);
	
	if (blade_handle_message_claim(blade_service_handle(peer->service), &message, data, data_length) != KS_STATUS_SUCCESS || !message) {
		// @todo error handling
		// just drop the peer for now, the only failure scenarios are asserted OOM, or if the discard queue pop fails
		peer->disconnecting = KS_TRUE;
		return KS_STATUS_FAIL;
	}
	ks_q_push(peer->messages_sending, message);
	return KS_STATUS_SUCCESS;
}

void *blade_peer_kws_thread(ks_thread_t *thread, void *data)
{
	blade_peer_t *peer = NULL;
	kws_opcode_t opcode;
	uint8_t *frame_data = NULL;
	ks_size_t frame_data_len = 0;
	blade_message_t *message = NULL;
	int32_t poll_flags = 0;

	ks_assert(thread);
	ks_assert(data);

	peer = (blade_peer_t *)data;

	// @todo: SSL init stuffs based on data from peer->service->config_websockets_ssl to pass into kws_init
	
	if (kws_init(&peer->kws, peer->sock, NULL, NULL, KWS_BLOCK, peer->pool) != KS_STATUS_SUCCESS) {
		peer->disconnecting = KS_TRUE;
		return NULL;
	}
	
	while (!peer->shutdown) {
		// @todo get exact timeout from service config?
		poll_flags = ks_wait_sock(peer->sock, 100, KS_POLL_READ | KS_POLL_ERROR);

		if (poll_flags & KS_POLL_ERROR) {
			// @todo switch this (and others below) to the enum for the state callback, called during the service connected peer cleanup
			peer->disconnecting = KS_TRUE;
			break;
		}

		if (poll_flags & KS_POLL_READ) {
			frame_data_len = kws_read_frame(peer->kws, &opcode, &frame_data);
			
			if (frame_data_len <= 0) {
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

			if (blade_handle_message_claim(blade_service_handle(peer->service), &message, frame_data, frame_data_len) != KS_STATUS_SUCCESS || !message) {
				// @todo error handling
				// just drop the peer for now, the only failure scenarios are asserted OOM, or if the discard queue pop fails
				peer->disconnecting = KS_TRUE;
				break;
			}
		
			ks_q_push(peer->messages_receiving, message);
			// @todo callback up the stack to indicate a message has been received and can be popped (more efficient than constantly polling by popping)?
			// might not perfectly fit the traditional state callback, but it could work if it only sends the state temporarily and does NOT actually change
			// the internal state to receiving
		}

		// @todo consider only sending one message at a time and use shorter polling timeout to prevent any considerable blocking if send buffers get full
		while (ks_q_trypop(peer->messages_sending, (void **)&message) == KS_STATUS_SUCCESS && message) {
			blade_message_get(message, (void **)&frame_data, &frame_data_len);
			kws_write_frame(peer->kws, WSOC_TEXT, frame_data, frame_data_len);
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
