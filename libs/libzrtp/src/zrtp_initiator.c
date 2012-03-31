/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include "zrtp.h"

#define _ZTU_ "zrtp initiator"

extern zrtp_status_t _zrtp_machine_start_initiating_secure(zrtp_stream_t *stream);

/*! These functions set constructs and start ZRTP messages replays */
static zrtp_status_t _zrtp_machine_start_send_and_resend_commit(zrtp_stream_t *stream);
static zrtp_status_t _zrtp_machine_start_send_and_resend_dhpart2(zrtp_stream_t *stream);
static zrtp_status_t _zrtp_machine_start_send_and_resend_confirm2(zrtp_stream_t *stream);

/*!
 * We need to know the contents of the DH2 packet before we send the Commit to
 * compute the hash value. So, we construct DH packet but don't send it till
 * WAITING_FOR_CONFIRM1 state.
*/
static void _prepare_dhpart2(zrtp_stream_t *stream);

/*
 * Parses DH packet: check for MitM1 attack and makes a copy of the packet for
 * later.  \exception: Handles all exceptions -- informs user and switches to
 * CLEAR.(MITM attacks)
 */
static zrtp_status_t _zrtp_machine_process_incoming_dhpart1( zrtp_stream_t *stream,
															 zrtp_rtp_info_t *packet);
/*
 * Just a wrapper over the protocol::_zrtp_machine_process_confirm().
 * \exception: Handles all exceptions -- informs user and switches to
 * CLEAR. (SOFTWARE)
 */
static zrtp_status_t _zrtp_machine_process_incoming_confirm1( zrtp_stream_t *stream,
															  zrtp_rtp_info_t *packet);


/*===========================================================================*/
/*		State handlers														 */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_process_while_in_initiatingsecure( zrtp_stream_t* stream,
															   zrtp_rtp_info_t* packet)
{
	zrtp_status_t s = zrtp_status_ok;

	switch (packet->type)
	{
	case ZRTP_COMMIT:
		if (ZRTP_STATEMACHINE_RESPONDER == _zrtp_machine_preparse_commit(stream, packet)) {
			_zrtp_cancel_send_packet_later(stream, ZRTP_COMMIT);
			s = _zrtp_machine_enter_pendingsecure(stream, packet);
		}
		break;

	case ZRTP_DHPART1:
		if (ZRTP_IS_STREAM_DH(stream)) {
			_zrtp_cancel_send_packet_later(stream, ZRTP_COMMIT);

			s = _zrtp_machine_process_incoming_dhpart1(stream, packet);
			if (zrtp_status_ok != s) {
				ZRTP_LOG(1,(_ZTU_,"\tERROR! _zrtp_machine_process_incoming_dhpart1() failed with status=%d ID=%u\n.", s, stream->id));
				break;
			}

			_zrtp_machine_start_send_and_resend_dhpart2(stream);

			/* Perform Key generation according to draft 5.6 */
			s = _zrtp_set_public_value(stream, 1);
			if (zrtp_status_ok != s) {
				ZRTP_LOG(1,(_ZTU_,"\tERROR! set_public_value1() failed with status=%d ID=%u.\n", s, stream->id));
				_zrtp_machine_enter_initiatingerror(stream, zrtp_error_software, 1);
				break;
			}

			_zrtp_change_state(stream, ZRTP_STATE_WAIT_CONFIRM1);
		}
		break;

	case ZRTP_CONFIRM1:
		if (ZRTP_IS_STREAM_FAST(stream)) {
			s = _zrtp_set_public_value(stream, 1);
			if (zrtp_status_ok != s) {
				break;
			}

			s = _zrtp_machine_process_incoming_confirm1(stream, packet);
			if (zrtp_status_ok != s) {
				ZRTP_LOG(1,(_ZTU_,"\tERROR! process_incoming_confirm1() failed with status=%d ID=%u.\n", s, stream->id));
				break;
			}

			_zrtp_cancel_send_packet_later(stream, ZRTP_COMMIT);
			_zrtp_change_state(stream, ZRTP_STATE_WAIT_CONFIRMACK);
			s = _zrtp_machine_start_send_and_resend_confirm2(stream);
		}
		break;

	case ZRTP_NONE:
		s = zrtp_status_drop;
		break;

	default:
		break;
	}

	return s;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_process_while_in_waitconfirm1( zrtp_stream_t* stream,
														   zrtp_rtp_info_t* packet)
{
	zrtp_status_t s = zrtp_status_ok;

	switch (packet->type)
	{
	case ZRTP_CONFIRM1:
		s = _zrtp_machine_process_incoming_confirm1(stream, packet);
		if (zrtp_status_ok != s) {
			ZRTP_LOG(1,(_ZTU_,"\tERROR! process_incoming_confirm1() failed with status=%d ID=%u.\n", s, stream->id));
			break;
		}

		_zrtp_change_state(stream, ZRTP_STATE_WAIT_CONFIRMACK);
		_zrtp_cancel_send_packet_later(stream, ZRTP_DHPART2);
		s = _zrtp_machine_start_send_and_resend_confirm2(stream);
		break;

	case ZRTP_NONE:
		s = zrtp_status_drop;
		break;

	default:
		break;
	}

	return s;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_process_while_in_waitconfirmack( zrtp_stream_t* stream,
															 zrtp_rtp_info_t* packet)
{
	zrtp_status_t s = zrtp_status_ok;

	switch (packet->type)
	{	
	case ZRTP_NONE:			
		s = _zrtp_protocol_decrypt(stream->protocol, packet, 1);
		if (s == zrtp_status_ok) { 
			/*
			 * High level functions triggers mutexes for protocol messages only.
			 * We have manually protect this transaction triggered by media packet, not protocol packet.
			 */
			zrtp_mutex_lock(stream->stream_protector);
			
			ZRTP_LOG(3,(_ZTU_, "Received FIRST VALID SRTP packet - switching to SECURE state. ID=%u\n", stream->id));
			_zrtp_cancel_send_packet_later(stream, ZRTP_CONFIRM2);
			_zrtp_machine_enter_secure(stream);
			
			zrtp_mutex_unlock(stream->stream_protector);
		}
		break;
	
	case ZRTP_CONFIRM2ACK:		
		_zrtp_cancel_send_packet_later(stream, ZRTP_CONFIRM2);
		s = _zrtp_machine_enter_secure(stream);
		break;

	default:
		break;
	}

	return s;
}


/*===========================================================================*/
/*		State switchers														 */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_enter_initiatingsecure(zrtp_stream_t* stream)
{
	zrtp_status_t s = zrtp_status_ok;
	
	ZRTP_LOG(3,(_ZTU_,"\tENTER STATE INITIATING SECURE for ID=%u mode=%s state=%s.\n",
				stream->id, zrtp_log_mode2str(stream->mode), zrtp_log_state2str(stream->state)));

	if (!ZRTP_IS_STREAM_MULT(stream)) {
		uint8_t id = ZRTP_COMP_UNKN;
		zrtp_session_t *session = stream->session;
		zrtp_packet_Hello_t *peer_hello = &stream->messages.peer_hello;

		/*
		 * ZRTP specification provides that default crypto components may be
		 * omitted from the Hello message, so we initialize components with
		 * default values.
		 */
		session->hash = zrtp_comp_find(ZRTP_CC_HASH, ZRTP_HASH_SHA256, session->zrtp);
		session->blockcipher = zrtp_comp_find(ZRTP_CC_CIPHER, ZRTP_CIPHER_AES128, session->zrtp);
		session->authtaglength = zrtp_comp_find(ZRTP_CC_ATL, ZRTP_ATL_HS32, session->zrtp);
		session->sasscheme = zrtp_comp_find(ZRTP_CC_SAS, ZRTP_SAS_BASE32, session->zrtp);

		id = _zrtp_choose_best_comp(&session->profile, peer_hello, ZRTP_CC_HASH);
		if (id != ZRTP_COMP_UNKN) {
			session->hash = zrtp_comp_find(ZRTP_CC_HASH, id, session->zrtp);
		}
		id = _zrtp_choose_best_comp(&session->profile, peer_hello, ZRTP_CC_CIPHER);
		if (id != ZRTP_COMP_UNKN) {
			session->blockcipher = zrtp_comp_find(ZRTP_CC_CIPHER, id, session->zrtp);
		}
		id = _zrtp_choose_best_comp(&session->profile, peer_hello, ZRTP_CC_ATL);
		if (id != ZRTP_COMP_UNKN) {
			session->authtaglength = zrtp_comp_find(ZRTP_CC_ATL, id, session->zrtp);
		}
		id = _zrtp_choose_best_comp(&session->profile, peer_hello, ZRTP_CC_SAS);
		if (id != ZRTP_COMP_UNKN) {
			session->sasscheme = zrtp_comp_find(ZRTP_CC_SAS, id, session->zrtp);
		}
		
		ZRTP_LOG(3,(_ZTU_,"\tInitiator selected following options:\n"));
		ZRTP_LOG(3,(_ZTU_,"\t      Hash: %.4s\n", session->hash->base.type));
		ZRTP_LOG(3,(_ZTU_,"\t    Cipher: %.4s\n", session->blockcipher->base.type));
		ZRTP_LOG(3,(_ZTU_,"\t       ATL: %.4s\n", session->authtaglength->base.type));
		ZRTP_LOG(3,(_ZTU_,"\tVAD scheme: %.4s\n", session->sasscheme->base.type));
	}

	do{
		/* Allocate resources for Initiator's state-machine */
		s = _zrtp_protocol_init(stream, 1, &stream->protocol);
		if (zrtp_status_ok != s) {
			break;	/* Software error */
		}

		_zrtp_change_state(stream, ZRTP_STATE_INITIATINGSECURE);

		/* Prepare DHPart2 message to compute hvi. For DH and Preshared streams only*/
		if (ZRTP_IS_STREAM_DH(stream)) {
			_prepare_dhpart2(stream);
		}

		s = _zrtp_machine_start_send_and_resend_commit(stream);
		if (zrtp_status_ok != s) {
			break; /* EH: Software error */
		}
		
		if (stream->zrtp->cb.event_cb.on_zrtp_protocol_event) {
			stream->zrtp->cb.event_cb.on_zrtp_protocol_event(stream, ZRTP_EVENT_IS_INITIATINGSECURE);
		}
	} while (0);

	if (zrtp_status_ok != s) {
		if (stream->protocol) {
			_zrtp_protocol_destroy(stream->protocol);
			stream->protocol = NULL;
		}
		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_software, 1);
	}

	if (ZRTP_IS_STREAM_DH(stream)) {
		/*
		 * If stream->concurrent is set this means that we stopped a concurrent
		 * DH stream to break a tie.  This can happen when Commit messages are
		 * sent by both ZRTP endpoints at the same time, but are received in
		 * different media streams. Now current stream has finished DH setup and
		 * we can resume the other one.
		 */
		if (stream->concurrent) {
			zrtp_stream_t* tctx = stream->concurrent;
			stream->concurrent = NULL;
			ZRTP_LOG(3,(_ZTU_,"\tRelease Concurrent Stream ID=%u. ID=%u\n", tctx->id, stream->id));
			_zrtp_machine_start_initiating_secure(tctx);
		}
	}


	return s;
}


/*===========================================================================*/
/*		Packet handlers														 */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
static zrtp_status_t _zrtp_machine_process_incoming_dhpart1( zrtp_stream_t *stream,
															 zrtp_rtp_info_t *packet)
{
	zrtp_status_t s = zrtp_status_ok;
	zrtp_packet_DHPart_t *dhpart1 = (zrtp_packet_DHPart_t*) packet->message;

	/* Validating DH (pvr is 1 or p-1) */
	bnInsertBigBytes(&stream->dh_cc.peer_pv, dhpart1->pv, 0, stream->pubkeyscheme->pv_length);

	s = stream->pubkeyscheme->validate(stream->pubkeyscheme, &stream->dh_cc.peer_pv);
	if (zrtp_status_ok != s) {
		ZRTP_LOG(2,(_ZTU_,"\tERROR! " ZRTP_MITM1_WARNING_STR " ID=%u\n", stream->id));
		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_possible_mitm1, 1);
		return s;
	}	

	/* Copy DH Part1 packet for further hashing */
	zrtp_memcpy(&stream->messages.peer_dhpart, dhpart1, zrtp_ntoh16(dhpart1->hdr.length)*4);
	
    return s;
}

/*----------------------------------------------------------------------------*/
static zrtp_status_t _zrtp_machine_process_incoming_confirm1( zrtp_stream_t *stream,
															  zrtp_rtp_info_t *packet)
{
	return _zrtp_machine_process_confirm(stream, (zrtp_packet_Confirm_t*) packet->message);
}


/*===========================================================================*/
/*		Packet senders														 */
/*===========================================================================*/

static void _send_and_resend_commit(zrtp_stream_t *stream, zrtp_retry_task_t* task)
{
	if (task->_retrys >= ZRTP_T2_MAX_COUNT) {
		ZRTP_LOG(2,(_ZTU_,"WARNING! COMMIT Max retransmissions count reached. ID=%u\n", stream->id));
	    _zrtp_machine_enter_initiatingerror(stream, zrtp_error_timeout, 0);
	} else if (task->_is_enabled) {
		zrtp_status_t s = zrtp_status_fail;
		zrtp_packet_Commit_t* commit = (zrtp_packet_Commit_t*) &stream->messages.commit;		

		s = _zrtp_packet_send_message(stream, ZRTP_COMMIT, commit);
		task->timeout = _zrtp_get_timeout((uint32_t)task->timeout, ZRTP_COMMIT);
		if (s == zrtp_status_ok) {
			task->_retrys++;
		}
		if (stream->zrtp->cb.sched_cb.on_call_later) {
			stream->zrtp->cb.sched_cb.on_call_later(stream, task);
		}
	}
}

/*---------------------------------------------------------------------------*/
static zrtp_status_t _zrtp_machine_start_send_and_resend_commit(zrtp_stream_t *stream)
{
	zrtp_proto_crypto_t* cc		 = stream->protocol->cc;
	zrtp_packet_Commit_t* commit = &stream->messages.commit;
	zrtp_retry_task_t* task		 = &stream->messages.commit_task;
	uint8_t hmac_offset			 = ZRTP_COMMIT_STATIC_SIZE;
	zrtp_session_t *session		 = stream->session;	

	zrtp_memcpy(commit->zid, stream->messages.hello.zid, sizeof(zrtp_zid_t));

	zrtp_memcpy(commit->hash_type, session->hash->base.type, ZRTP_COMP_TYPE_SIZE);
	zrtp_memcpy(commit->cipher_type, session->blockcipher->base.type, ZRTP_COMP_TYPE_SIZE);
	zrtp_memcpy(commit->auth_tag_length, session->authtaglength->base.type, ZRTP_COMP_TYPE_SIZE );
	zrtp_memcpy(commit->public_key_type, stream->pubkeyscheme->base.type, ZRTP_COMP_TYPE_SIZE);
	zrtp_memcpy(commit->sas_type, session->sasscheme->base.type, ZRTP_COMP_TYPE_SIZE);

	/*
	 * According to the last version of the internet draft 08b., hvi should be
	 * computed as:
	 * a) hvi=hash(initiator's DHPart2 message | responder's Hello message) for DH stream.
	 * b) For Multistream it just a 128 bit random nonce.
	 * c) For Preshared streams it keyID = HMAC(preshared_key, "Prsh") truncated to 64 bits
	 */
	switch (stream->mode)
	{
	case ZRTP_STREAM_MODE_DH:
	{
		void *hash_ctx = session->hash->hash_begin(session->hash);
		if (!hash_ctx) {	
			return zrtp_status_alloc_fail;
		}
		
		session->hash->hash_update( session->hash,
									hash_ctx,
									(const int8_t*)&stream->messages.dhpart,
									zrtp_ntoh16(stream->messages.dhpart.hdr.length)*4);
		session->hash->hash_update( session->hash,
									hash_ctx,
									(const int8_t*)&stream->messages.peer_hello,
									zrtp_ntoh16(stream->messages.peer_hello.hdr.length)*4);
		
		session->hash->hash_end(session->hash, hash_ctx, ZSTR_GV(cc->hv));
		zrtp_memcpy(commit->hv, cc->hv.buffer, ZRTP_HV_SIZE);
		hmac_offset += ZRTP_HV_SIZE;
	} break;
			
	case ZRTP_STREAM_MODE_PRESHARED:
	{		
		zrtp_string8_t  key_id	= ZSTR_INIT_EMPTY(key_id);
		zrtp_status_t s			= zrtp_status_ok;
		
		/* Generate random 4 word nonce */
		if (ZRTP_HV_NONCE_SIZE !=  zrtp_randstr(session->zrtp, (unsigned char*)cc->hv.buffer, ZRTP_HV_NONCE_SIZE)) {
			return zrtp_status_rng_fail;
		}
		cc->hv.length = ZRTP_HV_NONCE_SIZE;
		
		/*
		 * Generate Preshared_key:
		 * hash(len(rs1) | rs1 | len(auxsecret) | auxsecret | len(pbxsecret) | pbxsecret)
		 */
		s = _zrtp_compute_preshared_key( session,								 
										 ZSTR_GV(session->secrets.rs1->value),
										 (session->secrets.auxs->_cachedflag) ? ZSTR_GV(session->secrets.auxs->value) : NULL,
										 (session->secrets.pbxs->_cachedflag) ? ZSTR_GV(session->secrets.pbxs->value) : NULL,
										 NULL,
										 ZSTR_GV(key_id));
		if (zrtp_status_ok != s) {
			return s;
		}
		
		/* Copy 4 word nonce and add 2 word keyID */
		zrtp_memcpy(commit->hv, cc->hv.buffer, ZRTP_HV_NONCE_SIZE);
		hmac_offset += ZRTP_HV_NONCE_SIZE;
										
		zrtp_memcpy(commit->hv+ZRTP_HV_NONCE_SIZE, key_id.buffer, ZRTP_HV_KEY_SIZE);
		hmac_offset += ZRTP_HV_KEY_SIZE;
	} break;
	
	case ZRTP_STREAM_MODE_MULT:
	{
		if(ZRTP_HV_NONCE_SIZE != zrtp_randstr(session->zrtp, (unsigned char*)cc->hv.buffer, ZRTP_HV_NONCE_SIZE)) {
			return zrtp_status_rng_fail;
		}
		
		cc->hv.length = ZRTP_HV_NONCE_SIZE;
		zrtp_memcpy(commit->hv, cc->hv.buffer, ZRTP_HV_NONCE_SIZE);
		hmac_offset += ZRTP_HV_NONCE_SIZE;
	}break;
	default: break;
	}

	_zrtp_packet_fill_msg_hdr(stream, ZRTP_COMMIT, hmac_offset + ZRTP_HMAC_SIZE, &commit->hdr);
	
	{
		char buff[256];
		ZRTP_LOG(3,(_ZTU_,"\tStart Sending COMMIT ID=%u mode=%s state=%s:\n",
					stream->id, zrtp_log_mode2str(stream->mode), zrtp_log_state2str(stream->state)));
		ZRTP_LOG(3,(_ZTU_,"\t      Hash: %.4s\n", commit->hash_type));
		ZRTP_LOG(3,(_ZTU_,"\t    Cipher: %.4s\n", commit->cipher_type));
		ZRTP_LOG(3,(_ZTU_,"\t       ATL: %.4s\n", commit->auth_tag_length));
		ZRTP_LOG(3,(_ZTU_,"\t PK scheme: %.4s\n", commit->public_key_type));
		ZRTP_LOG(3,(_ZTU_,"\tVAD scheme: %.4s\n", commit->sas_type));

		ZRTP_LOG(3,(_ZTU_,"\t        hv: %s\n", hex2str((const char*)commit->hv, ZRTP_HV_SIZE, (char*)buff, sizeof(buff))));
	}

	task->_is_enabled = 1;
	task->callback = _send_and_resend_commit;
	task->_retrys = 0;
	_send_and_resend_commit(stream, task);

	return zrtp_status_ok;
}

/*----------------------------------------------------------------------------*/
static void _send_and_resend_dhpart2(zrtp_stream_t *stream, zrtp_retry_task_t* task)
{
    if (task->_retrys >= ZRTP_T2_MAX_COUNT)
    {
		ZRTP_LOG(1,(_ZTU_,"WARNING! DH2 Max retransmissions count reached. ID=%u\n", stream->id));
		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_timeout, 0);
	} else if (task->_is_enabled) {
		zrtp_status_t s = _zrtp_packet_send_message(stream, ZRTP_DHPART2, &stream->messages.dhpart);
		task->timeout = _zrtp_get_timeout((uint32_t)task->timeout, ZRTP_DHPART2);
		if (zrtp_status_ok == s) {
			task->_retrys++;
		}
		if (stream->zrtp->cb.sched_cb.on_call_later) {
			stream->zrtp->cb.sched_cb.on_call_later(stream, task);
		}
	}
}

static void _prepare_dhpart2(zrtp_stream_t *stream)
{
	zrtp_proto_crypto_t* cc = stream->protocol->cc;
	zrtp_packet_DHPart_t *dh2 = &stream->messages.dhpart;
	uint16_t	dh_length = (uint16_t)stream->pubkeyscheme->pv_length;

	zrtp_memcpy(dh2->rs1ID, cc->rs1.id.buffer, ZRTP_RSID_SIZE);
	zrtp_memcpy(dh2->rs2ID, cc->rs2.id.buffer, ZRTP_RSID_SIZE);
	zrtp_memcpy(dh2->auxsID, cc->auxs.id.buffer, ZRTP_RSID_SIZE);
	zrtp_memcpy(dh2->pbxsID, cc->pbxs.id.buffer, ZRTP_RSID_SIZE);

	bnExtractBigBytes(&stream->dh_cc.pv, dh2->pv, 0, dh_length);

	_zrtp_packet_fill_msg_hdr( stream,
						ZRTP_DHPART2,
						dh_length + ZRTP_DH_STATIC_SIZE + ZRTP_HMAC_SIZE,
						&dh2->hdr );
}

static zrtp_status_t _zrtp_machine_start_send_and_resend_dhpart2(zrtp_stream_t *stream)
{
	zrtp_retry_task_t* task = &stream->messages.dhpart_task;

	task->_is_enabled = 1;
	task->callback = _send_and_resend_dhpart2;
	task->_retrys = 0;
	_send_and_resend_dhpart2(stream, task);

	return zrtp_status_ok;
}


/*---------------------------------------------------------------------------*/
static void _send_and_resend_confirm2(zrtp_stream_t *stream, zrtp_retry_task_t* task)
{
    if (task->_retrys >= ZRTP_T2_MAX_COUNT) {
		ZRTP_LOG(1,(_ZTU_,"WARNING! CONFIRM2 Max retransmissions count reached. ID=%u\n", stream->id));
		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_timeout, 0);
	} else if (task->_is_enabled) {
		zrtp_status_t s = zrtp_status_ok;
	    s = _zrtp_packet_send_message(stream, ZRTP_CONFIRM2, &stream->messages.confirm);
		task->timeout = _zrtp_get_timeout((uint32_t)task->timeout, ZRTP_CONFIRM2);
		if (zrtp_status_ok == s) {
			task->_retrys++;
		}
		if (stream->zrtp->cb.sched_cb.on_call_later) {
			stream->zrtp->cb.sched_cb.on_call_later(stream, task);
		}
	}
}

static zrtp_status_t _zrtp_machine_start_send_and_resend_confirm2(zrtp_stream_t *stream)
{
	zrtp_retry_task_t* task = &stream->messages.confirm_task;

	zrtp_status_t s = _zrtp_machine_create_confirm(stream, &stream->messages.confirm);
	if (zrtp_status_ok != s) {		
		return s;
	}

	s = _zrtp_packet_fill_msg_hdr( stream,
								   ZRTP_CONFIRM2,
								   sizeof(zrtp_packet_Confirm_t) - sizeof(zrtp_msg_hdr_t),
								   &stream->messages.confirm.hdr);
	
	if (zrtp_status_ok == s) {
		task->_is_enabled = 1;
		task->callback = _send_and_resend_confirm2;
		task->_retrys = 0;
		_send_and_resend_confirm2(stream, task);
	}

	return s;
}
