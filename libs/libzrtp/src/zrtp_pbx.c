/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include "zrtp.h"

#define _ZTU_ "zrtp mitm"

extern zrtp_status_t _zrtp_machine_process_goclear(zrtp_stream_t* stream, zrtp_rtp_info_t* packet);


/*===========================================================================*/
/* State-Machine related functions                                           */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
static void _send_and_resend_sasrelay(zrtp_stream_t *stream, zrtp_retry_task_t* task)
{
	if (task->_retrys >= ZRTP_T2_MAX_COUNT) {
		ZRTP_LOG(1,(_ZTU_,"WARNING! SASRELAY Max retransmissions count reached. ID=%u\n", stream->id));
		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_timeout, 0);
	} else if (task->_is_enabled) {

		zrtp_status_t s = _zrtp_packet_send_message(stream, ZRTP_SASRELAY, &stream->messages.sasrelay);
		task->timeout = _zrtp_get_timeout((uint32_t)task->timeout, ZRTP_SASRELAY);
		if (zrtp_status_ok == s) {
			task->_retrys++;
		}
		if (stream->zrtp->cb.sched_cb.on_call_later) {
			stream->zrtp->cb.sched_cb.on_call_later(stream, task);
		}
	}
}

/*----------------------------------------------------------------------------*/
static zrtp_status_t _create_sasrelay( zrtp_stream_t *stream,
									   zrtp_sas_id_t transf_sas_scheme,
									   zrtp_string32_t* transf_sas_value,
									   uint8_t transf_ac_flag,
									   uint8_t transf_d_flag,
									   zrtp_packet_SASRelay_t* sasrelay )
{
	zrtp_session_t *session = stream->session;
	zrtp_status_t s = zrtp_status_fail;
	void* cipher_ctx = NULL;

	/* (padding + sig_len + flags) + SAS scheme and SASHash */
	const uint8_t encrypted_body_size = (2 + 1 + 1) + 4 + 32;

	zrtp_memset(sasrelay, 0, sizeof(zrtp_packet_SASRelay_t));

	/* generate a random initialization vector for CFB cipher  */
	if (ZRTP_CFBIV_SIZE != zrtp_randstr(session->zrtp, sasrelay->iv, ZRTP_CFBIV_SIZE)) {
		return zrtp_status_rp_fail;
	}

	sasrelay->flags |= (session->profile.disclose_bit || transf_d_flag) ? 0x01 : 0x00;
	sasrelay->flags |= (session->profile.allowclear && transf_ac_flag) ? 0x02 : 0x00;
	sasrelay->flags |= 0x04;

	zrtp_memcpy( sasrelay->sas_scheme,
				 zrtp_comp_id2type(ZRTP_CC_SAS, transf_sas_scheme),
				 ZRTP_COMP_TYPE_SIZE );
	if (transf_sas_value)
		zrtp_memcpy(sasrelay->sashash, transf_sas_value->buffer, transf_sas_value->length);

	/* Then we need to encrypt Confirm before computing Hmac. Use AES CFB */
	do {
		cipher_ctx = session->blockcipher->start( session->blockcipher,
												   (uint8_t*)stream->cc.zrtp_key.buffer,
												   NULL,
												   ZRTP_CIPHER_MODE_CFB );
		if (!cipher_ctx) {
			break;
		}

		s = session->blockcipher->set_iv( session->blockcipher,
										  cipher_ctx,
										  (zrtp_v128_t*)sasrelay->iv);
		if (zrtp_status_ok != s) {
			break;
		}

		s = session->blockcipher->encrypt( session->blockcipher,
										    cipher_ctx,
										    (uint8_t*)sasrelay->pad,
										    encrypted_body_size );
	} while(0);
	if (cipher_ctx) {
		session->blockcipher->stop(session->blockcipher, cipher_ctx);
	}



	if (zrtp_status_ok != s) {
		ZRTP_LOG(1,(_ZTU_,"\tERROR! Failed to encrypt SASRELAY Message status=%d. ID=%u\n", s, stream->id));
		return s;
	}

	/* Compute Hmac over encrypted part of Confirm */
	{
		zrtp_string128_t hmac = ZSTR_INIT_EMPTY(hmac);
		s = session->hash->hmac_c( session->hash,
									stream->cc.hmackey.buffer,
									stream->cc.hmackey.length,
									(const char*)&sasrelay->pad,
									encrypted_body_size,
									ZSTR_GV(hmac) );
		if (zrtp_status_ok != s) {
			ZRTP_LOG(1,(_ZTU_,"\tERROR! Failed to compute CONFIRM hmac status=%d. ID=%u\n", s, stream->id));
			return s;
		}
		zrtp_memcpy(sasrelay->hmac, hmac.buffer, ZRTP_HMAC_SIZE);
	}

	return s;
}

/*----------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_process_sasrelay(zrtp_stream_t *stream, zrtp_rtp_info_t *packet)
{
	zrtp_session_t *session = stream->session;
	zrtp_packet_SASRelay_t *sasrelay = (zrtp_packet_SASRelay_t*) packet->message;
	void* cipher_ctx = NULL;
	zrtp_sas_id_t rendering_id = ZRTP_COMP_UNKN;
	zrtp_status_t s = zrtp_status_fail;
	zrtp_string128_t hmac = ZSTR_INIT_EMPTY(hmac);
	char zerosashash[32];
	unsigned sas_scheme_did_change = 0;
	unsigned sas_hash_did_change = 0;

	/* (padding + sig_len + flags) + SAS scheme and SAS hash */
	const uint8_t encrypted_body_size = (2 + 1 + 1) + 4 + 32;

	zrtp_memset(zerosashash, 0, sizeof(zerosashash));

	/* Check if the remote endpoint is assigned to relay the SAS values */
	if (!stream->peer_mitm_flag) {
		ZRTP_LOG(2,(_ZTU_, ZRTP_RELAYED_SAS_FROM_NONMITM_STR));
		return zrtp_status_fail;
	}

	/* Check the HMAC */
	s = session->hash->hmac_c( session->hash,
								stream->cc.peer_hmackey.buffer,
								stream->cc.peer_hmackey.length,
								(const char*)&sasrelay->pad,
								encrypted_body_size,
								ZSTR_GV(hmac) );
	if (zrtp_status_ok != s ) {
		ZRTP_LOG(1,(_ZTU_,"\tERROR! Failed to compute CONFIRM hmac. status=%d ID=%u\n", s, stream->id));
		return zrtp_status_fail;
	}

	if (0 != zrtp_memcmp(sasrelay->hmac, hmac.buffer, ZRTP_HMAC_SIZE)) {
		ZRTP_LOG(2,(_ZTU_, ZRTP_VERIFIED_RESP_WARNING_STR));
		return zrtp_status_fail;
	}

	ZRTP_LOG(3,(_ZTU_, "\tHMAC value for the SASRELAY is correct - decrypting...\n"));

	/* Then we need to decrypt Confirm body */
	do
	{
		cipher_ctx = session->blockcipher->start( session->blockcipher,
												   (uint8_t*)stream->cc.peer_zrtp_key.buffer,
												   NULL,
												   ZRTP_CIPHER_MODE_CFB );
		 if (!cipher_ctx) {
			 break;
		 }

		s = session->blockcipher->set_iv(session->blockcipher, cipher_ctx, (zrtp_v128_t*)sasrelay->iv);
		if (zrtp_status_ok != s) {
			break;
		}

		s = session->blockcipher->decrypt( session->blockcipher,
										    cipher_ctx,
										    (uint8_t*)sasrelay->pad,
										    encrypted_body_size);
	} while(0);
	if (cipher_ctx) {
		session->blockcipher->stop(session->blockcipher, cipher_ctx);
	}

	if (zrtp_status_ok != s) {
		ZRTP_LOG(1,(_ZTU_,"\tERROR! Failed to decrypt Confirm. status=%d ID=%u\n", s, stream->id));
		return s;
	}

	ZRTP_LOG(2,(_ZTU_,"\tSasRelay FLAGS old/new A=%d/%d, D=%d/%d.\n",
					stream->allowclear, (uint8_t)(sasrelay->flags & 0x02),
					stream->peer_disclose_bit, (uint8_t)(sasrelay->flags & 0x01)));

	/* Set evil bit if other-side disclosed session key */
	stream->peer_disclose_bit = (sasrelay->flags & 0x01);

	/* Enable ALLOWCLEAR option only if both sides support it */
	stream->allowclear = (sasrelay->flags & 0x02) && session->profile.allowclear;

	/*
	 * We don't handle verified flag in SASRelaying because it makes no
	 * sense in implementation of the ZRTP Internet Draft.
	 */

	/*
	 * Only enrolled users can do SAS transferring. (Non-enrolled users can
	 * only change the SAS rendering scheme).
	 */

	rendering_id = zrtp_comp_type2id(ZRTP_CC_SAS, (char*)sasrelay->sas_scheme);
	if (-1 == zrtp_profile_find(&session->profile, ZRTP_CC_SAS, rendering_id)) {
		ZRTP_LOG(1,(_ZTU_,"\tERROR! PBX Confirm packet with transferred SAS have unknown or"
					" unsupported rendering scheme %.4s.ID=%u\n", sasrelay->sas_scheme, stream->id));

		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_invalid_packet, 1);
		return zrtp_status_fail;
	}

	/* Check is SAS rendering did change */
	if (rendering_id != session->sasscheme->base.id) {
		session->sasscheme = zrtp_comp_find(ZRTP_CC_SAS, rendering_id, session->zrtp );

		sas_scheme_did_change = 1;
		ZRTP_LOG(3,(_ZTU_,"\tSasrelay: Rendering scheme was updated to %.4s.\n", session->sasscheme->base.type));
	}

	if (session->secrets.matches & ZRTP_BIT_PBX) {
		if ( (((uint32_t) *sasrelay->sas_scheme) != (uint32_t)0x0L) &&
			 (0 != zrtp_memcmp(sasrelay->sashash, zerosashash, sizeof(sasrelay->sashash))) )
		{
			char buff[256];
			session->sasbin.length = ZRTP_MITM_SAS_SIZE;
			/* First 32 bits if sashash includes sasvalue */
			zrtp_memcpy(session->sasbin.buffer, sasrelay->sashash, session->sasbin.length);
			stream->mitm_mode = ZRTP_MITM_MODE_RECONFIRM_CLIENT;

			sas_hash_did_change = 1;
			ZRTP_LOG(3,(_ZTU_,"\tSasRelay: SAS value was updated to bin=%s.\n",
							hex2str(session->sasbin.buffer, session->sasbin.length, buff, sizeof(buff))));
		}
	} else if (0 != zrtp_memcmp(sasrelay->sashash, zerosashash, sizeof(sasrelay->sashash))) {
		ZRTP_LOG(1,(_ZTU_,"\tWARNING! SAS Value was received from NOT Trusted MiTM. ID=%u\n", stream->id));
		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_possible_mitm3, 1);
		return zrtp_status_fail;
	} else {
		ZRTP_LOG(1,(_ZTU_, "\rERROR! For SasRelay Other secret doesn't match. ID=%u\n", stream->id));
	}


	/* Generate new SAS if hash or rendering scheme did change.
	 * Note: latest libzrtp may send "empty" SasRelay with the same SAS rendering
	 *       scheme and empty Hello hash for consistency reasons, we should ignore
	 *       such packets.
	 */
	if (sas_scheme_did_change || sas_hash_did_change) {
		s = session->sasscheme->compute(session->sasscheme, stream, session->hash, 1);
		if (zrtp_status_ok != s) {
			_zrtp_machine_enter_initiatingerror(stream, zrtp_error_software, 1);
			return s;
		}

		ZRTP_LOG(3,(_ZTU_,"\tSasRelay: Updated SAS is <%s> <%s>.\n", session->sas1.buffer, session->sas2.buffer));

		if (session->zrtp->cb.event_cb.on_zrtp_protocol_event) {
			session->zrtp->cb.event_cb.on_zrtp_protocol_event(stream, ZRTP_EVENT_LOCAL_SAS_UPDATED);
		}
	}

	return zrtp_status_ok;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_process_while_in_sasrelaying( zrtp_stream_t* stream,
														  zrtp_rtp_info_t* packet)
{
	zrtp_status_t s = zrtp_status_ok;

	switch (packet->type)
	{
	case ZRTP_RELAYACK:
		_zrtp_cancel_send_packet_later(stream, ZRTP_SASRELAY);
		_zrtp_change_state(stream, ZRTP_STATE_SECURE);
		if (stream->zrtp->cb.event_cb.on_zrtp_protocol_event) {
			stream->zrtp->cb.event_cb.on_zrtp_protocol_event(stream, ZRTP_EVENT_REMOTE_SAS_UPDATED);
		}
		break;

	case ZRTP_GOCLEAR:
		s = _zrtp_machine_process_goclear(stream, packet);
		if (zrtp_status_ok == s) {
			s = _zrtp_machine_enter_pendingclear(stream);
		}
		break;

	case ZRTP_NONE:
		s = _zrtp_protocol_decrypt(stream->protocol, packet, 1);
		break;

	default:
		break;
	}

	return s;
}


/*===========================================================================*/
/* ZRTP API for PBX                                                          */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_stream_registration_start(zrtp_stream_t* stream, uint32_t ssrc)
{
	if (!stream) {
		return zrtp_status_bad_param;
	}

	ZRTP_LOG(3,(_ZTU_,"START REGISTRATION STREAM ID=%u mode=%s state=%s.\n",
				stream->id, zrtp_log_mode2str(stream->mode), zrtp_log_state2str(stream->state)));

	if (NULL == stream->zrtp->cb.cache_cb.on_get_mitm) {
		ZRTP_LOG(2,(_ZTU_,"WARNING: Can't use MiTM Functions with no ZRTP Cache.\n"));
		return zrtp_status_notavailable;
	}

	stream->mitm_mode = ZRTP_MITM_MODE_REG_SERVER;
	return zrtp_stream_start(stream, ssrc);
}

zrtp_status_t zrtp_stream_registration_secure(zrtp_stream_t* stream)
{
	if (!stream) {
		return zrtp_status_bad_param;
	}

	ZRTP_LOG(3,(_ZTU_,"SECURE REGISTRATION STREAM ID=%u mode=%s state=%s.\n",
				stream->id, zrtp_log_mode2str(stream->mode), zrtp_log_state2str(stream->state)));

	if (NULL == stream->zrtp->cb.cache_cb.on_get_mitm) {
		ZRTP_LOG(2,(_ZTU_,"WARNING: Can't use MiTM Functions with no ZRTP Cache.\n"));
		return zrtp_status_notavailable;
	}

	stream->mitm_mode = ZRTP_MITM_MODE_REG_SERVER;
	return zrtp_stream_secure(stream);
}

/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_register_with_trusted_mitm(zrtp_stream_t* stream)
{
	zrtp_session_t *session = stream->session;
	zrtp_status_t s = zrtp_status_bad_param;

	if (!stream) {
		return zrtp_status_bad_param;
	}

	ZRTP_LOG(3,(_ZTU_,"MARKING this call as REGISTRATION ID=%u\n", stream->id));

	if (NULL == stream->zrtp->cb.cache_cb.on_get_mitm) {
		ZRTP_LOG(2,(_ZTU_,"WARNING: Can't use MiTM Functions with no ZRTP Cache.\n"));
		return zrtp_status_notavailable;
	}

	if (!stream->protocol) {
		return zrtp_status_bad_param;
	}

	/* Passive Client endpoint should NOT generate PBX Secret. */
	if ((stream->mitm_mode == ZRTP_MITM_MODE_REG_CLIENT) &&
		(ZRTP_LICENSE_MODE_PASSIVE == stream->zrtp->lic_mode)) {
		ZRTP_LOG(2,(_ZTU_,"WARNING: Passive Client endpoint should NOT generate PBX Secret.\n"));
		return zrtp_status_bad_param;
	}

	/*
	 * Generate new MitM cache:
	 * pbxsecret = KDF(ZRTPSess, "Trusted MiTM key", (ZIDi | ZIDr), negotiated hash length)
	 */
	if ( (stream->state == ZRTP_STATE_SECURE) &&
		 ((stream->mitm_mode == ZRTP_MITM_MODE_REG_CLIENT) || (stream->mitm_mode == ZRTP_MITM_MODE_REG_SERVER)) )
	{
		zrtp_string32_t kdf_context = ZSTR_INIT_EMPTY(kdf_context);
		static const zrtp_string32_t trusted_mitm_key_label = ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_TRUSTMITMKEY_STR);
		zrtp_string16_t *zidi, *zidr;

		if (stream->protocol->type == ZRTP_STATEMACHINE_INITIATOR) {
			zidi = &session->zid;
			zidr = &session->peer_zid;
		} else {
			zidi = &session->peer_zid;
			zidr = &session->zid;
		}

		zrtp_zstrcat(ZSTR_GV(kdf_context), ZSTR_GVP(zidi));
		zrtp_zstrcat(ZSTR_GV(kdf_context), ZSTR_GVP(zidr));

		_zrtp_kdf( stream,
				   ZSTR_GV(session->zrtpsess),
				   ZSTR_GV(trusted_mitm_key_label),
				   ZSTR_GV(kdf_context),
				   ZRTP_HASH_SIZE,
				   ZSTR_GV(session->secrets.pbxs->value));

		session->secrets.pbxs->_cachedflag = 1;
		session->secrets.pbxs->lastused_at = (uint32_t)(zrtp_time_now()/1000);
		session->secrets.cached |= ZRTP_BIT_PBX;
		session->secrets.matches |= ZRTP_BIT_PBX;

		s = zrtp_status_ok;
		if (session->zrtp->cb.cache_cb.on_put_mitm) {
			s = session->zrtp->cb.cache_cb.on_put_mitm( ZSTR_GV(session->zid),
														ZSTR_GV(session->peer_zid),
														session->secrets.pbxs);
		}

		ZRTP_LOG(3,(_ZTU_,"Makring this call as REGISTRATION - DONE\n"));
	}

	return s;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_link_mitm_calls(zrtp_stream_t *stream1, zrtp_stream_t *stream2)
{
	if (!stream1 || !stream2) {
		return zrtp_status_bad_param;
	}

	ZRTP_LOG(3,(_ZTU_,"Link to MiTM call together stream1=%u stream2=%u.\n", stream1->id, stream2->id));

	/* This APi is for MiTM endpoints only. */
	if (stream1->zrtp->is_mitm) {
		return zrtp_status_bad_param;
	}

	stream1->linked_mitm = stream2;
	stream2->linked_mitm = stream1;

	{
		zrtp_stream_t *passive = NULL;
		zrtp_stream_t *unlimited = NULL;

		/* Check if we have at least one Unlimited endpoint. */
		if (stream1->peer_super_flag)
			unlimited = stream1;
		else if (stream2->peer_super_flag)
			unlimited = stream2;

		/* Check if the peer stream is Passive */
		if (unlimited) {
			passive = (stream1 == unlimited) ? stream2 : stream1;
			if (!passive->peer_passive)
				passive = NULL;
		}

		/* Ok, we haver Unlimited and Passive at two ends, let's make an exception and switch Passive to Secure. */
		if (unlimited && passive) {
			if (passive->state == ZRTP_STATE_CLEAR) {
				ZRTP_LOG(2,(_ZTU_,"INFO: zrtp_link_mitm_calls() stream with id=%u is Unlimited and"
							" Peer stream with id=%u is Passive in CLEAR state, switch the passive one to SECURE.\n"));

				/* @note: don't use zrtp_secure_stream() wrapper as it checks for Active/Passive stuff. */
				_zrtp_machine_start_initiating_secure(passive);
			}
		}
	}

	return zrtp_status_ok;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_update_remote_options( zrtp_stream_t* stream,
										  zrtp_sas_id_t transf_sas_scheme,
										  zrtp_string32_t* transf_sas_value,
										  uint8_t transf_ac_flag,
										  uint8_t transf_d_flag )
{
	zrtp_retry_task_t* task = &stream->messages.sasrelay_task;
	zrtp_status_t s = zrtp_status_ok;
	char buff[256];

	if (!stream) {
		return zrtp_status_bad_param;
	}

	ZRTP_LOG(3,(_ZTU_,"UPDATE REMOTE SAS OPTIONS mode. ID=%u\n", stream->id));
	ZRTP_LOG(3,(_ZTU_,"transf_sas=%s scheme=%d.\n", transf_sas_value ?
				hex2str((const char*)transf_sas_value->buffer, transf_sas_value->length, (char*)buff, sizeof(buff)) : "NULL",
				transf_sas_scheme));

	if (NULL == stream->zrtp->cb.cache_cb.on_get_mitm) {
		ZRTP_LOG(2,(_ZTU_,"WARNING: Can't use MiTM Functions with no ZRTP Cache.\n"));
		return zrtp_status_notavailable;
	}

	/* The TRANSFERRING option is only available from the SECURE state. */
	if (stream->state != ZRTP_STATE_SECURE) {
		return zrtp_status_bad_param;
	}

	/* Don't transfer an SAS to a non-enrolled user */
	if (transf_sas_value && !(stream->session->secrets.matches & ZRTP_BIT_PBX)) {
		return zrtp_status_bad_param;
	}

	/* Don't allow to transfer the SAS if the library wasn't initialized as MiTM endpoint */
	if (!stream->zrtp->is_mitm) {
		ZRTP_LOG(3,(_ZTU_,"\tERROR! The endpoint can't transfer SAS values to other endpoints"
					" without introducing itself by M-flag in Hello. see zrtp_init().\n"));
		return zrtp_status_wrong_state;
	}

	s = _create_sasrelay( stream,
						  transf_sas_scheme,
						  transf_sas_value,
						  transf_ac_flag,
						  transf_d_flag,
						  &stream->messages.sasrelay);
	if(zrtp_status_ok != s) {
		return s;
	}

	s = _zrtp_packet_fill_msg_hdr( stream,
								   ZRTP_SASRELAY,
								   sizeof(zrtp_packet_SASRelay_t) - sizeof(zrtp_msg_hdr_t),
								   &stream->messages.sasrelay.hdr);
	if(zrtp_status_ok != s) {
		return s;
	}

	_zrtp_change_state(stream, ZRTP_STATE_SASRELAYING);

	task->_is_enabled = 1;
	task->callback = _send_and_resend_sasrelay;
	task->_retrys = 0;
	_send_and_resend_sasrelay(stream, task);

	return s;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_resolve_mitm_call( zrtp_stream_t* stream1,
									  zrtp_stream_t* stream2)
{
	zrtp_stream_t* enrolled = NULL;
	zrtp_stream_t* non_enrolled = NULL;
	zrtp_sas_id_t mitm_sas_scheme = ZRTP_COMP_UNKN;
	zrtp_status_t s = zrtp_status_ok;

	if (!stream1 || !stream2) {
		return zrtp_status_bad_param;
	}

	ZRTP_LOG(3,(_ZTU_,"RESOLVE MITM CALL s1=%u, s2=%u...\n", stream1->id, stream2->id));

	if (NULL == stream1->zrtp->cb.cache_cb.on_get_mitm) {
		ZRTP_LOG(2,(_ZTU_,"WARNING: Can't use MiTM Functions with no ZRTP Cache.\n"));
		return zrtp_status_notavailable;
	}

	/*
     * Both sides must be in the Secure state and at least one should be
     * enrolled.
	 */
	if ((stream1->state != ZRTP_STATE_SECURE) || (stream2->state != ZRTP_STATE_SECURE)) {
		return zrtp_status_bad_param;
	}

	/* Check the stream enrollment options and choose one for transferring the call. */
	if (zrtp_is_user_enrolled(stream1)) {
		if (zrtp_is_user_enrolled(stream2)) {
			ZRTP_LOG(3,(_ZTU_,"\tBoth streams are enrolled - choose one with bigger ZID.\n"));
			enrolled = zrtp_choose_one_enrolled(stream1, stream2);
		} else {
			enrolled = stream1;
		}
	} else if (zrtp_is_user_enrolled(stream2)) {
		enrolled = stream2;
	}

	if (!enrolled) {
		return zrtp_status_bad_param;
	}
	else {
		non_enrolled = (stream1 == enrolled) ? stream2 : stream1;
	}

	ZRTP_LOG(3,(_ZTU_,"\tAfter Resolving: S1 is %s and S2 is %s.\n",
					(stream1 == enrolled) ? "ENROLLED" : "NON-ENROLLED",
					(stream2 == enrolled) ? "ENROLLED" : "NON-ENROLLED"));

	/*
     * Choose the best SAS rendering scheme supported by both peers.  Find the
     * stream that can change it.
	 */
	{
		uint8_t i=0;

		zrtp_packet_Hello_t *enhello = &enrolled->messages.peer_hello;
		char *encp = (char*)enhello->comp + (enhello->hc +
											 enhello->cc +
											 enhello->ac +
											 enhello->kc)* ZRTP_COMP_TYPE_SIZE;


		for (i=0; i<enhello->sc; i++, encp+=ZRTP_COMP_TYPE_SIZE)
		{
			uint8_t j=0;
			zrtp_packet_Hello_t *nonenhello = &non_enrolled->messages.peer_hello;
			char *nonencp = (char*)nonenhello->comp + (nonenhello->hc +
												   nonenhello->cc +
												   nonenhello->ac +
												   nonenhello->kc)* ZRTP_COMP_TYPE_SIZE;

			for (j=0; j<nonenhello->sc; j++, nonencp+=ZRTP_COMP_TYPE_SIZE)
			{
				if (0 == zrtp_memcmp(encp, nonencp, ZRTP_COMP_TYPE_SIZE)) {
					mitm_sas_scheme =  zrtp_comp_type2id(ZRTP_CC_SAS, encp);
					ZRTP_LOG(3,(_ZTU_,"\tMITM SAS scheme=%.4s was choosen.\n", encp));
					break;
				}
			}
			if (j != nonenhello->sc) {
				break;
			}
		}
	}
	if (ZRTP_COMP_UNKN == mitm_sas_scheme) {
		ZRTP_LOG(1,(_ZTU_,"\tERROR! Can't find matched SAS schemes on MiTM Resolving.\n"
					" s1=%u s2=$u", stream1->id, stream2->id));
		return zrtp_status_algo_fail;
	}

	s = zrtp_update_remote_options( enrolled,
									mitm_sas_scheme,
									&non_enrolled->session->sasbin,
									non_enrolled->allowclear,
									non_enrolled->peer_disclose_bit );
	if (zrtp_status_ok != s) {
		return s;
	}

	/* NOTE: new request from Philip Zimmermann - always send SASRelay to BOTH parties. */
	/* If non-enrolled party has SAS scheme different from chosen one - update */
	/*if (non_enrolled->session->sasscheme->base.id != mitm_sas_scheme) { */
		s = zrtp_update_remote_options( non_enrolled,
										mitm_sas_scheme,
										NULL,
										enrolled->allowclear,
										enrolled->peer_disclose_bit );
		if (zrtp_status_ok != s) {
			return s;
		}
	/*}*/

	return s;
}

/*---------------------------------------------------------------------------*/
uint8_t zrtp_is_user_enrolled(zrtp_stream_t* stream)
{
	if (!stream) {
		return zrtp_status_bad_param;
	}

	return ( (stream->session->secrets.cached & ZRTP_BIT_PBX) &&
		     (stream->session->secrets.matches & ZRTP_BIT_PBX) );
}

zrtp_stream_t* zrtp_choose_one_enrolled(zrtp_stream_t* stream1, zrtp_stream_t* stream2)
{
	if (!stream1 || !stream2) {
		return NULL;
	}

	if (zrtp_memcmp( stream1->session->zid.buffer,
					 stream2->session->zid.buffer,
					 stream1->session->zid.length) > 0) {
		return stream1;
	} else {
		return stream2;
	}
}
