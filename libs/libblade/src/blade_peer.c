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

struct blade_peer_s {
	ks_pool_t *pool;
	ks_thread_pool_t *tpool;
	blade_service_t *service;

	ks_socket_t sock;
	ks_bool_t shutdown;
	blade_peerstate_t state;
	blade_peerreason_t reason;
	
	kws_t *kws;
	ks_thread_t *kws_thread;

	ks_q_t *messages_sending;
	ks_q_t *messages_receiving;
};


void *blade_peer_kws_thread(ks_thread_t *thread, void *data);


KS_DECLARE(ks_status_t) blade_peer_destroy(blade_peer_t **bpP)
{
	blade_peer_t *bp = NULL;

	ks_assert(bpP);

	bp = *bpP;
	*bpP = NULL;

	ks_assert(bp);

	blade_peer_shutdown(bp);

	ks_q_destroy(&bp->messages_sending);
	ks_q_destroy(&bp->messages_receiving);

	ks_pool_free(bp->pool, &bp);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_peer_create(blade_peer_t **bpP, ks_pool_t *pool, ks_thread_pool_t *tpool, blade_service_t *service)
{
	blade_peer_t *bp = NULL;

	ks_assert(bpP);
	ks_assert(pool);
	ks_assert(tpool);
	ks_assert(service);

	bp = ks_pool_alloc(pool, sizeof(*bp));
	bp->pool = pool;
	bp->tpool = tpool;
	bp->service = service;
	bp->state = BLADE_PEERSTATE_CONNECTING;
	bp->reason = BLADE_PEERREASON_NORMAL;
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
	bp->state = BLADE_PEERSTATE_CONNECTING;
	bp->reason = BLADE_PEERREASON_NORMAL;

    if (ks_thread_create_ex(&bp->kws_thread,
							blade_peer_kws_thread,
							bp,
							KS_THREAD_FLAG_DEFAULT,
							KS_THREAD_DEFAULT_STACK,
							KS_PRI_NORMAL,
							bp->pool) != KS_STATUS_SUCCESS) {
		// @todo error logging
		blade_peer_disconnect(bp, BLADE_PEERREASON_ERROR);
		return KS_STATUS_FAIL;
	}
	
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

KS_DECLARE(void) blade_peer_disconnect(blade_peer_t *bp, blade_peerreason_t reason)
{
	ks_assert(bp);

	// @todo check if already disconnecting for another reason, avoid resetting to get initial reason for disconnect?
	bp->reason = reason;
	bp->state = BLADE_PEERSTATE_DISCONNECTING;
}

KS_DECLARE(blade_peerstate_t) blade_peer_state(blade_peer_t *bp)
{
	ks_assert(bp);
	return bp->state;
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
		// @todo error logging
		blade_peer_disconnect(peer, BLADE_PEERREASON_ERROR);
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

	// @todo consider using an INITIALIZING state to track when there is problems during initialization (specifically SSL negotiations)?
	// @todo should stack be notified with an internal event callback here before logic layer initialization starts (IE, before SSL negotiations)?
	peer->state = BLADE_PEERSTATE_RUNNING;
	
	// @todo: SSL init stuffs based on data from peer->service->config_websockets_ssl to pass into kws_init
	
	if (kws_init(&peer->kws, peer->sock, NULL, NULL, KWS_BLOCK, peer->pool) != KS_STATUS_SUCCESS) {
		// @todo error logging
		blade_peer_disconnect(peer, BLADE_PEERREASON_ERROR);
		return NULL;
	}

	blade_service_peer_state_callback(peer->service, peer, BLADE_PEERSTATE_RUNNING);
	
	while (!peer->shutdown) {
		// @todo get exact timeout from service config?
		poll_flags = ks_wait_sock(peer->sock, 100, KS_POLL_READ | KS_POLL_ERROR);

		if (poll_flags & KS_POLL_ERROR) {
			// @todo error logging
			blade_peer_disconnect(peer, BLADE_PEERREASON_ERROR);
			break;
		}

		if (poll_flags & KS_POLL_READ) {
			frame_data_len = kws_read_frame(peer->kws, &opcode, &frame_data);
			
			if (frame_data_len <= 0) {
				// @todo error logging, strerror(ks_errno())
				// 0 means socket closed with WS_NONE, which closes websocket with no additional reason
				// -1 means socket closed with a general failure
				// -2 means nonblocking wait
				// other values are based on WS_XXX reasons
				// negative values are based on reasons, except for -1 is but -2 is nonblocking wait, and
			
				blade_peer_disconnect(peer, BLADE_PEERREASON_ERROR);
				break;
			}

			if (blade_handle_message_claim(blade_service_handle(peer->service), &message, frame_data, frame_data_len) != KS_STATUS_SUCCESS || !message) {
				// @todo error logging
				blade_peer_disconnect(peer, BLADE_PEERREASON_ERROR);
				break;
			}
		
			ks_q_push(peer->messages_receiving, message);
			blade_service_peer_state_callback(peer->service, peer, BLADE_PEERSTATE_RECEIVING);
		}

		// @todo consider only sending one message at a time and use shorter polling timeout to prevent any considerable blocking if send buffers get full
		while (ks_q_trypop(peer->messages_sending, (void **)&message) == KS_STATUS_SUCCESS && message) {
			blade_message_get(message, (void **)&frame_data, &frame_data_len);
			// @todo may need to get the WSOC_TEXT from the message if using WSOC_BINARY is desired later
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
