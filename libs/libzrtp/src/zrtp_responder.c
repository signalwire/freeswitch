/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include "zrtp.h"

#define _ZTU_ "zrtp responder"

extern zrtp_status_t _zrtp_machine_start_initiating_secure(zrtp_stream_t *stream);

/* These functions construct packets for further replies. */
static zrtp_status_t _prepare_dhpart1(zrtp_stream_t *stream);
static zrtp_status_t _prepare_confirm1(zrtp_stream_t *stream);

/* Functions which are used to answer the Initiator's requests */
static void _send_dhpart1(zrtp_stream_t *stream);
static void _send_confirm1(zrtp_stream_t *stream);

/*
 * Parses crypto-components list chosen by the initiator. doesn't perform any
 * tests. Commit was fully checked by previous call of _zrtp_machine_preparse_commit().
 * \exception: Handles all exceptions -- informs user and switches to CLEAR.
 * (zrtp_error_XXX_unsp and zrtp_error_software errors.)
 */
static zrtp_status_t _zrtp_machine_process_commit( zrtp_stream_t* stream,
												   zrtp_rtp_info_t* packet);

/*
 * Parses DH packet: check for MitM1, MitM2 attacks and makes a copy of it for further usage.
 * \exception: (MITM attacks, SOFTWARE) Informs user and switches to CLEAR.
 */
static zrtp_status_t _zrtp_machine_process_dhpart2( zrtp_stream_t *stream,
												    zrtp_rtp_info_t *packet);

/*
 * Just a wrapper over the protocol::_zrtp_machine_process_confirm().
 * \exception: (AUTH attacks, SOFTWARE) Informs user and switches to CLEAR.
 */
static zrtp_status_t _zrtp_machine_process_confirm2( zrtp_stream_t *stream,
													 zrtp_rtp_info_t *packet);


/*===========================================================================*/
/*		State handlers														 */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_process_while_in_pendingsecure( zrtp_stream_t* stream,
														    zrtp_rtp_info_t* packet)
{
	zrtp_status_t s = zrtp_status_ok;
	
	switch (packet->type)
	{
	case ZRTP_COMMIT:
		_send_dhpart1(stream);
		break;
	
	case ZRTP_DHPART2:
		s = _zrtp_machine_process_dhpart2(stream, packet);
		if (zrtp_status_ok != s) {
			break;
		}
		
		/* Perform Keys generation according to draft 5.6 */
		s = _zrtp_set_public_value(stream, 0);
		if (zrtp_status_ok != s) {
			_zrtp_machine_enter_initiatingerror(stream, zrtp_error_software, 1);
			break;
		}

		s = _prepare_confirm1(stream);
		if (zrtp_status_ok != s) {
			_zrtp_machine_enter_initiatingerror(stream, zrtp_error_software, 1);
			break;
		}

		_zrtp_change_state(stream, ZRTP_STATE_WAIT_CONFIRM2);
		_send_confirm1(stream);
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
zrtp_status_t _zrtp_machine_process_while_in_waitconfirm2( zrtp_stream_t* stream,
														   zrtp_rtp_info_t* packet)
{
	zrtp_status_t status = zrtp_status_ok;

	switch (packet->type)
	{
	case ZRTP_DHPART2:
		if (ZRTP_IS_STREAM_DH(stream)) {
			_send_confirm1(stream);
		}
		break;
	
	case ZRTP_COMMIT:
		if (ZRTP_IS_STREAM_FAST(stream)) {
			_send_confirm1(stream);
		}
		break;

	case ZRTP_CONFIRM2:
		status = _zrtp_machine_process_confirm2(stream, packet);
		if (zrtp_status_ok == status) {
			_zrtp_packet_send_message(stream, ZRTP_CONFIRM2ACK, NULL);
			status = _zrtp_machine_enter_secure(stream);
		}
		break;
	
	case ZRTP_NONE:
		status = zrtp_status_drop;
		break;
	
	default:
		break;
	}

	return status;
}


/*===========================================================================*/
/*		States switchers 													 */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_enter_pendingsecure( zrtp_stream_t* stream,
												 zrtp_rtp_info_t* packet)
{	
	zrtp_status_t s = zrtp_status_ok;
	
	ZRTP_LOG(3,(_ZTU_,"\tENTER STATE PENDING SECURE for ID=%u mode=%s state=%s.\n",
				stream->id, zrtp_log_mode2str(stream->mode), zrtp_log_state2str(stream->state)));
	
	do
	{
	if (!ZRTP_IS_STREAM_MULT(stream)) {
		zrtp_packet_Commit_t *commit = (zrtp_packet_Commit_t*) packet->message;

		stream->session->hash = zrtp_comp_find( ZRTP_CC_HASH,
												zrtp_comp_type2id(ZRTP_CC_HASH, (char*)commit->hash_type),
												stream->zrtp);
		stream->session->blockcipher = zrtp_comp_find( ZRTP_CC_CIPHER,
													   zrtp_comp_type2id(ZRTP_CC_CIPHER, (char*)commit->cipher_type),
													   stream->zrtp);
		stream->session->authtaglength = zrtp_comp_find( ZRTP_CC_ATL,
														 zrtp_comp_type2id(ZRTP_CC_ATL, (char*)commit->auth_tag_length),
														 stream->zrtp);	
		stream->session->sasscheme = zrtp_comp_find( ZRTP_CC_SAS,
													 zrtp_comp_type2id(ZRTP_CC_SAS, (char*)commit->sas_type),
													 stream->zrtp);					 
		
		ZRTP_LOG(3,(_ZTU_,"\tRemote COMMIT specified following options:\n"));
		ZRTP_LOG(3,(_ZTU_,"\t      Hash: %.4s\n", commit->hash_type));
		ZRTP_LOG(3,(_ZTU_,"\t    Cipher: %.4s\n", commit->cipher_type));
		ZRTP_LOG(3,(_ZTU_,"\t       ATL: %.4s\n", commit->auth_tag_length));
		ZRTP_LOG(3,(_ZTU_,"\t PK scheme: %.4s\n", commit->public_key_type));
		ZRTP_LOG(3,(_ZTU_,"\tVAD scheme: %.4s\n", commit->sas_type));
	}

	if (ZRTP_IS_STREAM_DH(stream)) {		
		_zrtp_change_state(stream, ZRTP_STATE_PENDINGSECURE);

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
			ZRTP_LOG(3,(_ZTU_,"\tRelease2 Concurrent stream=%u ID=%u\n", tctx->id, stream->id));
			_zrtp_machine_start_initiating_secure(tctx);
		}		

		s = _zrtp_protocol_init(stream, 0, &stream->protocol);
		if (zrtp_status_ok != s) {
			break;
		}

		s = _zrtp_machine_process_commit(stream, packet); /* doesn't throw exception */
		if (zrtp_status_ok != s) {
			break; /* Software error */	
		}
	
		s = _prepare_dhpart1(stream);
		if (zrtp_status_ok != s) {
			break; /* EH: Always successful */
		}
		
		_zrtp_machine_process_while_in_pendingsecure(stream, packet);				
		
		if (stream->zrtp->cb.event_cb.on_zrtp_protocol_event) {
			stream->zrtp->cb.event_cb.on_zrtp_protocol_event(stream, ZRTP_EVENT_IS_PENDINGSECURE);
		}
	}
	else 
	{
		_zrtp_change_state(stream, ZRTP_STATE_WAIT_CONFIRM2);

		s = _zrtp_protocol_init(stream, 0, &stream->protocol);
		if (zrtp_status_ok != s) {
			break;
		}

		s = _zrtp_machine_process_commit(stream, packet); /* doesn't throw exception */
		if (zrtp_status_ok != s) {
			break; /* Software error */
		}

		s = _zrtp_set_public_value(stream, 0);
		if (zrtp_status_ok != s) {
			break; /* Software error */
		}

		s = _prepare_confirm1(stream);
		if (zrtp_status_ok != s) {
			break; /* Software error */
		}

		_send_confirm1(stream);
	}
	} while (0);

	if (zrtp_status_ok != s) {
		if (stream->protocol) {
			_zrtp_protocol_destroy(stream->protocol);
			stream->protocol = NULL;
		}
		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_software, 1);
	}
	
	return s;
}


/*===========================================================================*/
/*		Packets handlers													 */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
static zrtp_status_t _check_commit(zrtp_stream_t *stream, zrtp_packet_Commit_t *commit)
{
	do {
	/* check PUBLIC KEY TYPE */
	if (0 > zrtp_profile_find( &stream->session->profile,
								   ZRTP_CC_PKT,
								   zrtp_comp_type2id(ZRTP_CC_PKT, (char*)commit->public_key_type)))
	{
    	/* Can't talk to them. ZRTP public key type not supported by current profile */
		ZRTP_LOG(2,(_ZTU_,"\tINFO: PKExch %.4s isn't supported by profile. ID=%u\n",
					commit->public_key_type, stream->id));
		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_pktype_unsp, 1);
   		break;
	}

	/* check HASH scheme */
	if ( 0 > zrtp_profile_find( &stream->session->profile,
								   ZRTP_CC_HASH,
								   zrtp_comp_type2id(ZRTP_CC_HASH, (char*)commit->hash_type)) )
	{
    	/* Can't talk to them. ZRTP hash type not supported by current profile */
		ZRTP_LOG(2,(_ZTU_,"\tINFO: Hash %.4s isn't supported by profile. ID=%u\n",
					commit->hash_type, stream->id));
		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_hash_unsp, 1);
		break;
	}
	
	/* check CIPHER type */
	if ( 0 > zrtp_profile_find( &stream->session->profile,
								   ZRTP_CC_CIPHER,
								   zrtp_comp_type2id(ZRTP_CC_CIPHER, (char*)commit->cipher_type)) )
	{
    	/* Can't talk to them. ZRTP cipher type not supported by current profile */
		ZRTP_LOG(2,(_ZTU_,"\tINFO: Cipher  %.4s isn't supported by profile. ID=%u\n",
					commit->cipher_type, stream->id));
		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_cipher_unsp, 1);
		break;
	}
		
	/* check AUTH TAG LENGTH */
	if ( 0 > zrtp_profile_find( &stream->session->profile,
								   ZRTP_CC_ATL,
								   zrtp_comp_type2id(ZRTP_CC_ATL, (char*)commit->auth_tag_length)) )
	{
		/* Can't talk to them. ZRTP auth tag length not supported by current profile */
		ZRTP_LOG(2,(_ZTU_,"\tINFO: Authtag %.4s isn't supported by profile. ID=%u\n",
					commit->auth_tag_length, stream->id));
    	_zrtp_machine_enter_initiatingerror(stream, zrtp_error_auth_unsp, 1);
    	break;			
	}
		
	/* check SAS scheme */
	if ( 0 > zrtp_profile_find( &stream->session->profile,
								   ZRTP_CC_SAS,
								   zrtp_comp_type2id(ZRTP_CC_SAS, (char*)commit->sas_type)) )
	{
		/* Can't talk to them. ZRTP SAS scheme not supported by current profile */
		ZRTP_LOG(2,(_ZTU_,"\tINFO: SAS %.4s isn't supported by profile. ID=%u\n",
					commit->sas_type, stream->id));
    	_zrtp_machine_enter_initiatingerror(stream, zrtp_error_sas_unsp, 1);
		break;
	}

	return zrtp_status_ok;
	} while (0);
	
	return zrtp_status_fail;
}

/*---------------------------------------------------------------------------*/
zrtp_statemachine_type_t _zrtp_machine_preparse_commit( zrtp_stream_t *stream,
													    zrtp_rtp_info_t* packet)
{	
	zrtp_packet_Commit_t *commit = (zrtp_packet_Commit_t*) packet->message;
	zrtp_statemachine_type_t res = ZRTP_STATEMACHINE_RESPONDER;
	
	zrtp_pktype_id_t	his_pkt  = zrtp_comp_type2id(ZRTP_CC_PKT, (char*)commit->public_key_type);	
	zrtp_stream_mode_t	his_mode = (his_pkt == ZRTP_PKTYPE_PRESH) ? ZRTP_STREAM_MODE_PRESHARED : (his_pkt == ZRTP_PKTYPE_MULT) ? ZRTP_STREAM_MODE_MULT : ZRTP_STREAM_MODE_DH;

	ZRTP_LOG(3,(_ZTU_,"\tPreparse incoming COMMIT. Remote peer wants %.4s:%d mode lic=%d peer M=%d.\n",
				commit->public_key_type, his_mode, stream->zrtp->lic_mode, stream->peer_mitm_flag));
	
	/*
	 * Checking crypto components chosen by other peer for stream establishment
	 */
	if (zrtp_status_ok  != _check_commit(stream, commit)) {
		return ZRTP_STATEMACHINE_NONE;
	}
	
	/*
	 * Passive ZRTP endpoint can't talk to ZRTP MiTM endpoints.
	 */
	if (!ZRTP_PASSIVE3_TEST(stream)) {
		ZRTP_LOG(2,(_ZTU_,"\tERROR: The endpoint is in passive mode and can't handle"
					" connections with MiTM endpoints. ID=%u\n", stream->id));
		if (stream->zrtp->cb.event_cb.on_zrtp_protocol_event ) {
			stream->zrtp->cb.event_cb.on_zrtp_protocol_event(stream, ZRTP_EVENT_IS_PASSIVE_RESTRICTION);
		}
		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_service_unavail, 1);
		return ZRTP_STATEMACHINE_NONE;
	}	

	/*
	 * Both sides are in "Initiating" state we need to break the tie:
	 *  - if both sides wants to use the same scheme - side  with lower vh switches to
	 *    "Responder" state.
	 *	- if both sides wants to use Preshared scheme and one of the sides are in MiTM mode it
	 *    should switch to Responder state
     *  - if one side wants Preshared and onother one DH - DH should win.
	 *  - rest of the combinations (DH - Multistream, Preshared - Multistream) are deperecated by the RFC
	 */
	if (ZRTP_STATE_INITIATINGSECURE == stream->state)
	{
		zrtp_pktype_id_t	my_pkt  =  stream->pubkeyscheme->base.id;
		zrtp_stream_mode_t	my_mode = (my_pkt == ZRTP_PKTYPE_PRESH) ? ZRTP_STREAM_MODE_PRESHARED : (my_pkt == ZRTP_PKTYPE_MULT) ? ZRTP_STREAM_MODE_MULT : ZRTP_STREAM_MODE_DH;
		
		ZRTP_LOG(2,(_ZTU_,"\tBoth sides are in INITIATINGSECURE State - BREACK the TIE. ID=%u\n", stream->id));
				
		if (his_mode == my_mode) {
			if ( (his_mode == ZRTP_STREAM_MODE_PRESHARED) && (stream->peer_mitm_flag || stream->zrtp->is_mitm)) {
				if (stream->peer_mitm_flag) {
					ZRTP_LOG(3,(_ZTU_,"\tWe running in Gneral ZRTP Endpoint mode, but the"
								" remote side is in MiTM - stay Initiating state.\n"));
					res = ZRTP_STATEMACHINE_INITIATOR;
				}
			} else {
				if (zrtp_memcmp( stream->protocol->cc->hv.buffer,
								 commit->hv,
								 (his_mode == ZRTP_STREAM_MODE_DH) ? ZRTP_HV_SIZE : ZRTP_HV_NONCE_SIZE) > 0) {
					ZRTP_LOG(3,(_ZTU_,"\tWe have Commit with greater HV so stay Initiating state.\n"));
					res = ZRTP_STATEMACHINE_INITIATOR;
				}
			}
		} else {
			if (my_mode == ZRTP_STREAM_MODE_DH) {
				ZRTP_LOG(3,(_ZTU_,"\tOther peer sent Non DH Commit but we want DH - stay Initiating state.\n"));
				res = ZRTP_STATEMACHINE_INITIATOR;
			}
		}
	}

	if (res == ZRTP_STATEMACHINE_RESPONDER)
	{
		/*
		 * If other peer wants to switch "Preshared" we must be ready for this. Check
		 * for secrets availability and if we can't use "Preshared" we should force other
		 * peer to switch to "DH" mode. For this purpose we use our own Commit with DHxK
		 * in it. Such Commit should win competition in any case.
		 */
		if ((his_mode == ZRTP_STREAM_MODE_PRESHARED) && !stream->session->secrets.rs1->_cachedflag) {
			ZRTP_LOG(3,(_ZTU_, "\tOther peer wants Preshared mode but we have no secrets.\n"));
			res = ZRTP_STATEMACHINE_INITIATOR;
		}

		/*
		 * If other peer wants to switch "Multistream" we must be ready for this. Check
		 * for ZRTPSess key availability. If we can't use "Multistream" we should force other
		 * peer to switch to "DH" mode. For this purpose we use our own Commit with DHxK
		 * in it. Such Commit should win competition in any case.
		 */
		if ((his_mode == ZRTP_STREAM_MODE_MULT) && !stream->session->zrtpsess.length) {
			ZRTP_LOG(3,(_ZTU_,"\tOther peer wants Preshared mode but we have no secrets.\n"));
			res = ZRTP_STATEMACHINE_INITIATOR;
		}

		/*
		 * If other peer wants "Full DH" exchange but ZRTP Session key have been already
		 * computed - there is no sense in doing this. What is more, ZRTP Specification
		 * doesn't allow doing this.
		 */		 
		if ((his_mode == ZRTP_STREAM_MODE_DH) && (stream->session->zrtpsess.length > 0)) {
			ZRTP_LOG(3,(_ZTU_,"\tOther peer wants DH mode but we have ZRTP session and ready for Multistream.\n"));
			res = ZRTP_STATEMACHINE_NONE;
		}
	}

	/*
	 * If we decided to use Responder's state-machine - only one DH or Preshared stream
	 * can be run at the moment so check states.
	 */
	if ((res == ZRTP_STATEMACHINE_RESPONDER) && !_zrtp_can_start_stream(stream, &stream->concurrent, his_mode))
	{
		ZRTP_LOG(3,(_ZTU_,"\tCan't handle COMMIT another DH with ID=%u is in progress.\n", stream->concurrent->id));

		if ( (stream->concurrent->state <= ZRTP_STATE_INITIATINGSECURE) &&
			 (zrtp_memcmp(stream->concurrent->protocol->cc->hv.buffer, commit->hv, ZRTP_HV_SIZE) < 0) )
		{
			ZRTP_LOG(3,(_ZTU_,"\tPossible DEADLOCK Resolving. STOP CONCURRENT"
						" Stream with ID=%u\n",stream->concurrent->id));
			_zrtp_cancel_send_packet_later(stream->concurrent, ZRTP_NONE);
		} else {
			res = ZRTP_STATEMACHINE_NONE;
		}
	}
		
	if (res == ZRTP_STATEMACHINE_RESPONDER) {
		ZRTP_LOG(3,(_ZTU_,"\tChosen Responder State-Machine. Change Mode to %s,"
					" pkt to %.4s\n", zrtp_log_mode2str(his_mode), commit->public_key_type));
		stream->mode = his_mode;
		stream->pubkeyscheme = zrtp_comp_find(ZRTP_CC_PKT, his_pkt, stream->zrtp);
	} else {
		ZRTP_LOG(3,(_ZTU_,"\tChosen Initiator State-Machine. Stay in current Mode\n"));
	}

	return res;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_process_commit(zrtp_stream_t* stream, zrtp_rtp_info_t* packet)
{	
	zrtp_packet_Commit_t *commit = (zrtp_packet_Commit_t*) packet->message;
	
	switch (stream->mode)
	{
	case ZRTP_STREAM_MODE_DH:
		zrtp_zstrncpyc( ZSTR_GV(stream->protocol->cc->peer_hv),
						(const char*)commit->hv,
						ZRTP_HV_SIZE);
		break;
	case ZRTP_STREAM_MODE_PRESHARED:		
		zrtp_zstrncpyc( ZSTR_GV(stream->protocol->cc->peer_hv),
						(const char*)commit->hv + ZRTP_HV_NONCE_SIZE,
						ZRTP_HV_NONCE_SIZE);
	case ZRTP_STREAM_MODE_MULT:
		zrtp_zstrncpyc( ZSTR_GV(stream->protocol->cc->peer_hv),
						(const char*)commit->hv,
						ZRTP_HV_NONCE_SIZE);
		break;
	default: break;
	}

	/* Copy Commit packet for further hashing */
	zrtp_memcpy(&stream->messages.peer_commit, commit, zrtp_ntoh16(commit->hdr.length)*4);
    
    return zrtp_status_ok;
}


/*----------------------------------------------------------------------------*/
static zrtp_status_t _zrtp_machine_process_dhpart2( zrtp_stream_t *stream,
												    zrtp_rtp_info_t *packet)
{
	zrtp_status_t s = zrtp_status_ok;
	zrtp_proto_crypto_t* cc = stream->protocol->cc;
	zrtp_packet_DHPart_t *dhpart2 = (zrtp_packet_DHPart_t*) packet->message;
	void *hash_ctx = NULL;

	/*
	 * Verify hash commitment. (Compare hvi calculated from DH with peer hvi from COMMIT)
	 * According to the last version of the internet draft 04a. Hvi should be
	 * computed as: hvi=hash(initiator's DHPart2 message | responder's Hello message)
	 */
	hash_ctx = stream->session->hash->hash_begin(stream->session->hash);
	if (!hash_ctx) {
		return zrtp_status_fail;
	}
	
	stream->session->hash->hash_update( stream->session->hash,
										hash_ctx,
										(const int8_t*)dhpart2,
										zrtp_ntoh16(dhpart2->hdr.length)*4);
	stream->session->hash->hash_update( stream->session->hash,
										hash_ctx,
										(const int8_t*)&stream->messages.hello,
										zrtp_ntoh16(stream->messages.hello.hdr.length)*4);
	stream->session->hash->hash_end( stream->session->hash,
									 hash_ctx,
									 ZSTR_GV(cc->hv));
	
	/* Truncate comuted hvi to 256 bit. The same length as transferred in Commit message.*/
	cc->hv.length = ZRTP_HASH_SIZE;
	
	if (0 != zrtp_zstrcmp(ZSTR_GV(cc->hv), ZSTR_GV(cc->peer_hv))) {
    	ZRTP_LOG(1,(_ZTU_,"\tERROR!" ZRTP_MIM2_WARNING_STR " ID=%u\n", stream->id));
		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_possible_mitm2, 1);
		return zrtp_status_fail;
	}

	/* Validate DH exchange (pvi is 1 or p-1). For DH streams only */		
	bnInsertBigBytes(&stream->dh_cc.peer_pv, dhpart2->pv, 0, stream->pubkeyscheme->pv_length);

	s = stream->pubkeyscheme->validate(stream->pubkeyscheme, &stream->dh_cc.peer_pv);
	if (zrtp_status_ok != s) {
		ZRTP_LOG(1,(_ZTU_,"\tERROR!" ZRTP_MITM1_WARNING_STR " ID=%u\n", stream->id));
		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_possible_mitm1, 1);
		return s;
	}
	
	/* Copy DH Part2 packet for future hashing */
	zrtp_memcpy(&stream->messages.peer_dhpart, dhpart2, zrtp_ntoh16(dhpart2->hdr.length)*4);

    return s;
}

/*----------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_process_confirm2( zrtp_stream_t *stream,
											  zrtp_rtp_info_t *packet)
{
	zrtp_packet_Confirm_t *confirm2 = (zrtp_packet_Confirm_t*) packet->message;
	return _zrtp_machine_process_confirm(stream, confirm2);
}


/*===========================================================================*/
/*		Packets senders														 */
/*===========================================================================*/

/*----------------------------------------------------------------------------*/
static void _send_dhpart1(zrtp_stream_t *stream)
{
	_zrtp_packet_send_message(stream, ZRTP_DHPART1, &stream->messages.dhpart);
}

static zrtp_status_t _prepare_dhpart1(zrtp_stream_t *stream)
{	
    zrtp_proto_crypto_t* cc = stream->protocol->cc;
	zrtp_packet_DHPart_t *dh1 = &stream->messages.dhpart;
	uint16_t dh_length = (uint16_t)stream->pubkeyscheme->pv_length;
	
	zrtp_memcpy(dh1->rs1ID, cc->rs1.id.buffer, ZRTP_RSID_SIZE);	
	zrtp_memcpy(dh1->rs2ID, cc->rs2.id.buffer, ZRTP_RSID_SIZE);		
	zrtp_memcpy(dh1->auxsID, cc->auxs.id.buffer, ZRTP_RSID_SIZE);
	zrtp_memcpy(dh1->pbxsID, cc->pbxs.id.buffer, ZRTP_RSID_SIZE);	
		
	bnExtractBigBytes(&stream->dh_cc.pv, dh1->pv, 0, dh_length);
	
	_zrtp_packet_fill_msg_hdr( stream,
							   ZRTP_DHPART1,
							   dh_length + ZRTP_DH_STATIC_SIZE + ZRTP_HMAC_SIZE,
							   &dh1->hdr);

	return zrtp_status_ok;
}

/*----------------------------------------------------------------------------*/
static void _send_confirm1(zrtp_stream_t *stream)
{		
	_zrtp_packet_send_message(stream, ZRTP_CONFIRM1, &stream->messages.confirm);
}

static zrtp_status_t _prepare_confirm1(zrtp_stream_t *stream)
{
	zrtp_status_t s = _zrtp_machine_create_confirm(stream, &stream->messages.confirm);
	if (zrtp_status_ok == s) {
		s = _zrtp_packet_fill_msg_hdr( stream,
									   ZRTP_CONFIRM1,
									   sizeof(zrtp_packet_Confirm_t) - sizeof(zrtp_msg_hdr_t),
									   &stream->messages.confirm.hdr);
	}

	return s;
}
