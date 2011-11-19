/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include "zrtp.h"

#define _ZTU_ "zrtp engine"

/*!
 * Data type for state-handlers: every state has a state handler
 * function which is called by zrtp_process_srtp().
 */
typedef zrtp_status_t state_handler_t( zrtp_stream_t* stream, zrtp_rtp_info_t* packet );
extern state_handler_t* state_handler[ZRTP_STATE_COUNT];

extern zrtp_status_t _zrtp_machine_process_sasrelay(zrtp_stream_t *stream, zrtp_rtp_info_t *packet);

static void _zrtp_machine_switch_to_error(zrtp_stream_t* stream);
static zrtp_status_t _zrtp_machine_enter_initiatingclear(zrtp_stream_t* stream);
static zrtp_status_t _zrtp_machine_enter_clear(zrtp_stream_t* stream);
static zrtp_status_t _zrtp_machine_enter_pendingerror(zrtp_stream_t *stream, zrtp_protocol_error_t code);

zrtp_status_t _zrtp_machine_process_hello(zrtp_stream_t* stream, zrtp_rtp_info_t* packet);
zrtp_status_t _zrtp_machine_process_goclear(zrtp_stream_t* stream, zrtp_rtp_info_t* packet);

static void _send_helloack(zrtp_stream_t* stream);
static void _send_goclearack(zrtp_stream_t* stream);

zrtp_status_t _zrtp_machine_start_send_and_resend_hello(zrtp_stream_t* stream);
static zrtp_status_t _zrtp_machine_start_send_and_resend_goclear(zrtp_stream_t* stream);
static zrtp_status_t _zrtp_machine_start_send_and_resend_errorack(zrtp_stream_t* stream);
static zrtp_status_t _zrtp_machine_start_send_and_resend_error(zrtp_stream_t* stream);

void _clear_stream_crypto(zrtp_stream_t* stream);


/*===========================================================================*/
// MARK: ===> Main ZRTP interfaces
/*===========================================================================*/

/*----------------------------------------------------------------------------*/
zrtp_status_t zrtp_process_rtcp(zrtp_stream_t *stream, char* packet, unsigned int* length)
{

    /*
	 * In transition states, drop outgoing packets. In SECURE state, encrypt
       outgoing packets.  In all other states leave them unchanged.
	 */

    if (stream) {
		switch (stream->state)
		{
		case ZRTP_STATE_START_INITIATINGSECURE:
		case ZRTP_STATE_INITIATINGSECURE:
		case ZRTP_STATE_WAIT_CONFIRM1:
		case ZRTP_STATE_WAIT_CONFIRMACK:
		case ZRTP_STATE_PENDINGSECURE:
		case ZRTP_STATE_WAIT_CONFIRM2:
		case ZRTP_STATE_PENDINGCLEAR:
			return zrtp_status_drop;

		case ZRTP_STATE_SASRELAYING:
		case ZRTP_STATE_SECURE:
		{
			zrtp_rtp_info_t info;

			if (*length < RTCP_HDR_SIZE) {
				return zrtp_status_fail;
			}

			zrtp_memset(&info, 0, sizeof(info));
			info.packet = packet;
			info.length = length;
			info.seq = 0; /*sequence number will be generated in zrtp_srtp_protect_rtcp()*/
			info.ssrc = (uint32_t) *(packet+sizeof(uint32_t));

			return _zrtp_protocol_encrypt(stream->protocol, &info, 0);
		}

		default:
		return zrtp_status_ok;
		}
    }

    return zrtp_status_ok;
}

/*----------------------------------------------------------------------------*/
zrtp_status_t zrtp_process_srtcp(zrtp_stream_t *stream, char* packet, unsigned int* length)
{

    /*
	 * In transition states, drop incoming packets. In SECURE state, decrypt
	 * incoming packets. In all other states leave them unchanged.
	 */

    if (stream) {
		switch (stream->state)
		{
		case ZRTP_STATE_INITIATINGCLEAR:
			case ZRTP_STATE_PENDINGCLEAR:
			case ZRTP_STATE_INITIATINGSECURE:
			case ZRTP_STATE_PENDINGSECURE:
				return zrtp_status_drop;

			case ZRTP_STATE_SECURE:
			case ZRTP_STATE_SASRELAYING:
			{
				zrtp_rtp_info_t info;

				if (*length < RTCP_HDR_SIZE) {
					return zrtp_status_fail;
				}

				zrtp_memset(&info, 0, sizeof(info));
				info.packet = packet;
				info.length = length;
				info.seq = 0; /*sequence number will be determined from packet in zrtp_srtp_unprotect_rtcp()*/
				info.ssrc = (uint32_t) *(packet+sizeof(uint32_t));

				return _zrtp_protocol_decrypt(stream->protocol, &info, 0);
			}

			default:
				return zrtp_status_ok;
		}
    }

    return zrtp_status_ok;
}

/*----------------------------------------------------------------------------*/
zrtp_status_t zrtp_process_rtp(zrtp_stream_t *stream, char* packet, unsigned int* length)
{
	zrtp_rtp_info_t info;

	if (!stream || !packet || !length) {
		return zrtp_status_bad_param;
	}

	/* Skip packet processing within uninitiated stream */
	if ((stream->state < ZRTP_STATE_START) || (stream->state > ZRTP_STATE_NO_ZRTP)) {
		return zrtp_status_ok;
	}

	/* Prepare RTP packet: detect type and other options */
	if (zrtp_status_ok != _zrtp_packet_preparse(stream, packet, length, &info, 0)) {
		return zrtp_status_fail;
	}
	
	/* Drop packets in transition states and encrypt in SECURE state */
	switch (stream->state)
	{
	case ZRTP_STATE_START_INITIATINGSECURE:
	case ZRTP_STATE_INITIATINGSECURE:
	case ZRTP_STATE_WAIT_CONFIRM1:
	case ZRTP_STATE_WAIT_CONFIRMACK:
	case ZRTP_STATE_PENDINGSECURE:
	case ZRTP_STATE_WAIT_CONFIRM2:
	case ZRTP_STATE_PENDINGCLEAR:
		if (ZRTP_NONE == info.type) {	
			/* Add dropped media to the entropy hash */
			ZRTP_LOG(1,(_ZTU_,"Add %d bytes of entropy to the RNG pool.\n", *length));
			zrtp_entropy_add(stream->zrtp, (unsigned char*)packet, *length);
			
			return zrtp_status_drop;
		}
		break;

	case ZRTP_STATE_SASRELAYING:
	case ZRTP_STATE_SECURE:
		if (ZRTP_NONE == info.type) {
			return _zrtp_protocol_encrypt(stream->protocol, &info, 1);
		}
		break;

	default:
		break;
	}

	return zrtp_status_ok;
}

/*----------------------------------------------------------------------------*/
extern int _send_message(zrtp_stream_t* stream, zrtp_msg_type_t type, const void* message, uint32_t ssrc);
zrtp_status_t zrtp_process_srtp(zrtp_stream_t *stream, char* packet, unsigned int* length)
{
    zrtp_rtp_info_t info;
	zrtp_status_t s = zrtp_status_ok;

    if (!stream || !packet || !length) {
		return zrtp_status_bad_param;
	}
	
	if (*length <= RTP_HDR_SIZE) {
		return zrtp_status_bad_param;
	}
	
	/* Preparse RTP packet: detect type and other options */
	s = _zrtp_packet_preparse(stream, packet, length, &info, 1);
	if (zrtp_status_ok != s) {
		return s;
	}
	
	/*************************************************************************/
	/* For Zfone3 Compatibility */
	if (ZRTP_ZFONEPING == info.type) {
		zrtp_packet_zfoneping_t* ping = (zrtp_packet_zfoneping_t*) info.message;
		zrtp_packet_zfonepingack_t pingack;
		
		zrtp_memcpy(pingack.version, ZRTP_ZFONE_PROTOCOL_VERSION, 4);
		zrtp_memcpy(pingack.endpointhash, stream->session->zid.buffer, sizeof(pingack.endpointhash));
		zrtp_memcpy(pingack.peerendpointhash, ping->endpointhash, sizeof(pingack.endpointhash));
		pingack.peerssrc = info.ssrc;
		
		_zrtp_packet_fill_msg_hdr( stream,
								   ZRTP_ZFONEPINGACK,
								   sizeof(zrtp_packet_zfonepingack_t) - sizeof(zrtp_msg_hdr_t),
								   &pingack.hdr);
		
		_zrtp_packet_send_message(stream, ZRTP_ZFONEPINGACK, &pingack);
		return zrtp_status_drop;
	}
	/*************************************************************************/
	
	/* Skip packet processing within non-started stream */
	if ((stream->state < ZRTP_STATE_START) || (stream->state > ZRTP_STATE_NO_ZRTP)) {		
		return (ZRTP_NONE == info.type) ? zrtp_status_ok : zrtp_status_drop;
	}

	/*
	 * This mutex should protect stream data against asynchr. calls e.g.:
	 * zrtp_stream_secure(), zrtp_stream_clear() etc. Media packet handlers
	 * don't change any internal data, so this applies only to ZRTP messages.
	 */
	if (info.type != ZRTP_NONE) {
		zrtp_mutex_lock(stream->stream_protector);
	}

	/* Extra protection. We need protocol to handle ZRTP messages in following states. */
	switch (stream->state)
	{
	case ZRTP_STATE_INITIATINGSECURE:
	case ZRTP_STATE_WAIT_CONFIRM1:
	case ZRTP_STATE_WAIT_CONFIRMACK:
	case ZRTP_STATE_PENDINGSECURE:
	case ZRTP_STATE_WAIT_CONFIRM2:
	case ZRTP_STATE_SECURE:
	case ZRTP_STATE_SASRELAYING:
		if (!stream->protocol) {
			if (info.type != ZRTP_NONE) {
				zrtp_mutex_unlock(stream->stream_protector);
			}
			return zrtp_status_fail;
		}
	default:
		break;
	}

	/* Handle Error packet from any state */
	if (ZRTP_ERROR == info.type && stream->state > ZRTP_STATE_START)
	{
		switch (stream->state)
		{
		case ZRTP_STATE_NONE:
		case ZRTP_STATE_ACTIVE:
		case ZRTP_STATE_SECURE:
		case ZRTP_STATE_PENDINGERROR:
		case ZRTP_STATE_INITIATINGERROR:
		case ZRTP_STATE_NO_ZRTP:
		    break;
		default:
			{
				zrtp_packet_Error_t* error = (zrtp_packet_Error_t*) info.message;
				_zrtp_machine_enter_pendingerror(stream, zrtp_ntoh32(error->code));
			} break;
		}
	}

	/* Process packet by state-machine according to packet type and current protocol state */
	if (state_handler[stream->state]) {
		s = state_handler[stream->state](stream, &info);
	}

	/* Unlock stream mutex for a ZRTP message packet. See comments above. */
	if (info.type != ZRTP_NONE) {
		s = zrtp_status_drop;
		zrtp_mutex_unlock(stream->stream_protector);
	}

	return s;
}

/*----------------------------------------------------------------------------*/
zrtp_status_t zrtp_stream_start(zrtp_stream_t* stream, uint32_t ssrc)
{
	zrtp_status_t s = zrtp_status_ok;
	 /*
	  * (ZRTP stream starts from START state and HELLO packets resending.
	  * Stream can be started from START, ERROR or NOZRTP states only.)
	  */
	ZRTP_LOG(3,(_ZTU_,"START STREAM ID=%u mode=%s state=%s.\n",
				stream->id, zrtp_log_mode2str(stream->mode), zrtp_log_state2str(stream->state)));

	if ( (ZRTP_STATE_ACTIVE != stream->state) &&
		 (ZRTP_STATE_ERROR != stream->state) &&
		 (ZRTP_STATE_NO_ZRTP != stream->state)) {
		ZRTP_LOG(1,(_ZTU_,"ERROR! Can't start Stream ID=%u from %s state.\n",
					stream->id, zrtp_log_state2str(stream->state)));
		s = zrtp_status_wrong_state;
	} else {
		stream->media_ctx.ssrc = zrtp_hton32(ssrc);
		
		_zrtp_change_state(stream, ZRTP_STATE_START);
		_zrtp_machine_start_send_and_resend_hello(stream);
	}
	
	return s;
}

/*----------------------------------------------------------------------------*/
zrtp_status_t zrtp_stream_stop(zrtp_stream_t* stream)
{
	zrtp_status_t s = zrtp_status_ok;
	/*
	 * Stop all packet replays, deinitialize crypto data and prepare the stream
	 * for the next use. The stream can be terminated from any protocol state.
	 */
	 ZRTP_LOG(3,(_ZTU_,"STOP STREAM ID=%u mode=%s state=%s.\n",
				stream->id, zrtp_log_mode2str(stream->mode), zrtp_log_state2str(stream->state)));
	
	/*
	 * Unlink deleted stream for the peer MiTM stream if necessary. It may
	 * prevent some recae-conditions as we always test for NULL before
	 * accessing linked_mitm.
	 */
	if (stream->linked_mitm) {
		stream->linked_mitm->linked_mitm = NULL;
	}

    if (stream->state != ZRTP_STATE_NONE) {
		/*
		 * This function can be called in parallel to the main processing loop
		 * - protect internal stream data.
		 */
		zrtp_mutex_lock(stream->stream_protector);
		
		_zrtp_cancel_send_packet_later(stream, ZRTP_NONE);
		if (stream->zrtp->cb.sched_cb.on_wait_call_later) {
			stream->zrtp->cb.sched_cb.on_wait_call_later(stream);
		}
		
		_clear_stream_crypto(stream);

		zrtp_mutex_unlock(stream->stream_protector);
		zrtp_mutex_destroy(stream->stream_protector);

		zrtp_memset(stream, 0, sizeof(zrtp_stream_t));
		
		stream->mode = ZRTP_STREAM_MODE_UNKN;
		
		_zrtp_change_state(stream, ZRTP_STATE_NONE);
    } else {
		s = zrtp_status_wrong_state;
	}
	
	return s;
}

/*----------------------------------------------------------------------------*/
zrtp_status_t zrtp_stream_clear(zrtp_stream_t *stream)
{
	/*
	 * This function can be called for two reasons: either our user is
	 * initiating the go-clear ritual or we accepting that ritual as
	 * initiated by the other end of the line. If our user initiates the
	 * go-clear process libzrtp switches to INITIATING_CLEAR and runs
	 * GoClear replays. The go-clear ritual can be started from SECURE state
	 * only. If the other end of the line is initiating and this function is
	 * being called to accept the go-clear procedure - protocol transites to
	 * CLEAR state imediately. One can accept go-clear from PENDING CLEAR
	 * state only. See state-macine diagram for more information.
	 */
	zrtp_status_t s = zrtp_status_fail;

	/* This function can be called in parallel to the main processing loop - protect stream data. */
	zrtp_mutex_lock(stream->stream_protector);

	ZRTP_LOG(3,(_ZTU_,"CLEAR STREAM ID=%u mode=%s state=%s.\n",
				stream->id, zrtp_log_mode2str(stream->mode), zrtp_log_state2str(stream->state)));

    switch (stream->state)
    {
	case ZRTP_STATE_SECURE:
		/* Clearing ritual can't be started if "allow clear" is disabled */
		if (stream->session->profile.allowclear) {
			s = _zrtp_machine_enter_initiatingclear(stream);
		}
		break;
	case ZRTP_STATE_PENDINGCLEAR:
		s = _zrtp_machine_enter_clear(stream);
		break;
	default:
		break;
    }

	zrtp_mutex_unlock(stream->stream_protector);

	return s;
}

/*----------------------------------------------------------------------------*/
void _initiating_secure(zrtp_stream_t *stream, zrtp_retry_task_t* task)
{
	/*
	 * In accordance with the ZRTP standard, there can be multiple simultaneous
	 * DH streams, as well as preshared streams.
	 *
	 * Before entering the INITIATING_SECURE state, we check several conditions.
	 * For details see \doc\img\odg\zrtp_streams.odg and zrtp_statemach.odg)
	 */

	/* The first call to this function is already protected by a mutex in zrtp_process_srtp() */
	uint8_t use_mutex = (task->_retrys > 0);

	if (!task->_is_enabled) {
		return;
	}

	if (use_mutex) {
		zrtp_mutex_lock(stream->stream_protector);
	}
	
	ZRTP_LOG(3,(_ZTU_,"\tInitiating Secure iteration... ID=%u.\n", stream->id));

	/* Skip the last replay after switching to another state to avoid unwanted replays */
	if (stream->state <= ZRTP_STATE_START_INITIATINGSECURE)
	{
		stream->mode = _zrtp_define_stream_mode(stream);
		ZRTP_LOG(3,(_ZTU_,"\tGot mode=%s. Check approval of starting.\n", zrtp_log_mode2str(stream->mode)));
		if (!_zrtp_can_start_stream(stream, &stream->concurrent, stream->mode))
		{
			if (task->_retrys > ZRTP_PROCESS_T1_MAX_COUNT) {
				ZRTP_LOG(3,(_ZTU_,"\tInitiating Secure. Max retransmissions count reached"
							 "for stream ID=%u.\n", stream->id));
				
				_zrtp_machine_enter_initiatingerror(stream, zrtp_error_timeout, 0);
			} else {
				ZRTP_LOG(3,(_ZTU_,"\tInitiating Secure. stream ID=%u is DH but one more DH"
							" stream is in progress - waiting...\n", stream->id));

				task->_retrys++;
				if (stream->zrtp->cb.sched_cb.on_call_later) {
					stream->zrtp->cb.sched_cb.on_call_later(stream, task);
				}
			}
		}
		else
		{
			ZRTP_LOG(3,(_ZTU_,"\tMode=%s Cccepted. Starting ZRTP Initiator Protocol.\n", zrtp_log_mode2str(stream->mode)));
			_zrtp_cancel_send_packet_later(stream, ZRTP_PROCESS);
			_zrtp_machine_enter_initiatingsecure(stream);
		}
	}

	if (use_mutex) {
		zrtp_mutex_unlock(stream->stream_protector);
	}
}

zrtp_status_t _zrtp_machine_start_initiating_secure(zrtp_stream_t *stream)
{
	/*
	 * This function creates a task to do retries of the first packet in the
	 * "Going secure" procedure, and then _initiating_secure() will start
	 * protocol.
	 */
	zrtp_retry_task_t* task = &stream->messages.dh_task;
	task->_is_enabled = 1;
	task->_retrys = 0;
	task->callback = _initiating_secure;
	task->timeout = ZRTP_PROCESS_T1;

	/*
	 * Prevent race conditions on starting multiple streams.
	 */
	zrtp_mutex_lock(stream->session->init_protector);

	_zrtp_change_state(stream, ZRTP_STATE_START_INITIATINGSECURE);
	_initiating_secure(stream, task);

	zrtp_mutex_unlock(stream->session->init_protector);

	return zrtp_status_ok;
}


zrtp_status_t zrtp_stream_secure(zrtp_stream_t *stream)
{
	/*
	 * Wrapper function for going into secure mode.  It can be initiated in
	 * parallel to the main processing loop.  The internal stream data has to
	 * be protected by mutex.
	 */

	zrtp_status_t s = zrtp_status_fail;

	ZRTP_LOG(3,(_ZTU_,"SECURE STREAM ID=%u mode=%s state=%s.\n",
				stream->id, zrtp_log_mode2str(stream->mode), zrtp_log_state2str(stream->state)));

	zrtp_mutex_lock(stream->stream_protector);

    /* Limit ZRTP Session initiation procedure according to the license */
	if ( (stream->state == ZRTP_STATE_CLEAR) && ZRTP_PASSIVE1_TEST(stream)) {
		s = _zrtp_machine_start_initiating_secure(stream);
	} else {
		ZRTP_LOG(1,(_ZTU_,"\tWARNING! Can't Start Stream from %s state and with %d license mode. ID=%u\n",
					zrtp_log_state2str(stream->state), stream->zrtp->lic_mode, stream->id));
		
		if (!ZRTP_PASSIVE1_TEST(stream)) {
			if (stream->zrtp->cb.event_cb.on_zrtp_protocol_event ) {
				stream->zrtp->cb.event_cb.on_zrtp_protocol_event(stream, ZRTP_EVENT_IS_PASSIVE_RESTRICTION);
			}
		}
	}

	zrtp_mutex_unlock(stream->stream_protector);

    return s;
}


/*===========================================================================*/
/*		State handlers														 */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_process_while_in_start( zrtp_stream_t* stream,
													zrtp_rtp_info_t* packet)
{
	zrtp_status_t s = zrtp_status_ok;

	switch (packet->type)
	{
	case ZRTP_HELLO:
		s = _zrtp_machine_process_hello(stream, packet);
		if (zrtp_status_ok != s) {
			ZRTP_LOG(1,(_ZTU_,"\tERROR! _zrtp_machine_process_hello() failed with status=%d. ID=%u\n", s, stream->id));
			break; /* Just stay in START state. */
		}

		/* Now we have ZIDs for both sides and can upload secrets from the cache */
		s = _zrtp_prepare_secrets(stream->session);
		if (zrtp_status_ok != s) {
			ZRTP_LOG(1,(_ZTU_,"\tERROR! _zrtp_prepare_secrets() failed with status=%d. ID=%u\n", s, stream->id));
			break; /* Just stay in START state. */
		}

		_send_helloack(stream);
		_zrtp_change_state(stream, ZRTP_STATE_WAIT_HELLOACK);
		break;

	case ZRTP_HELLOACK:
		_zrtp_cancel_send_packet_later(stream, ZRTP_HELLO);
		_zrtp_change_state(stream, ZRTP_STATE_WAIT_HELLO);
		break;

	default:
		break;
	}

	return s;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_process_while_in_wait4hello( zrtp_stream_t* stream,
														 zrtp_rtp_info_t* packet)
{
	zrtp_status_t s = zrtp_status_ok;

	switch (packet->type)
	{
	case ZRTP_HELLO:
		s = _zrtp_machine_process_hello(stream, packet);
		if (zrtp_status_ok != s) {
			ZRTP_LOG(1,(_ZTU_,"\tERROR! _zrtp_machine_process_hello()2 failed with status=%d. ID=%u\n", s, stream->id));
			break; /* Just stay in the current state. */
		}

		/* Now we have ZIDs for both sides and can upload secrets from the cache */
		s = _zrtp_prepare_secrets(stream->session);
		if (zrtp_status_ok != s) {
			ZRTP_LOG(1,(_ZTU_,"\tERROR! _zrtp_prepare_secrets()2 failed with status=%d. ID=%u\n", s, stream->id));
			break; /* Just stay in the current state. */
		}

		/* Start initiating the secure state if "autosecure" is enabled */
		if ((stream->session->profile.autosecure) && ZRTP_PASSIVE1_TEST(stream)) {			
			if (!stream->session->profile.discovery_optimization) {
				_send_helloack(stream); /* Response with HelloAck before start computing DH value */
			}
			s = _zrtp_machine_start_initiating_secure(stream);
		} else {			
			_send_helloack(stream);
			
			if (!ZRTP_PASSIVE1_TEST(stream)) {
				if (stream->zrtp->cb.event_cb.on_zrtp_protocol_event) {
					stream->zrtp->cb.event_cb.on_zrtp_protocol_event(stream, ZRTP_EVENT_IS_PASSIVE_RESTRICTION);
				}
				ZRTP_LOG(2,(_ZTU_,"\tINFO: Switching to Clear due to Active/Passive restrictions.\n"));
			}
			
			s = _zrtp_machine_enter_clear(stream);
		}

		break;

	default:
		break;
	}

	return s;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_process_while_in_wait4helloack( zrtp_stream_t* stream,
														    zrtp_rtp_info_t* packet)
{
	zrtp_status_t status = zrtp_status_ok;

	switch (packet->type)
	{
	case ZRTP_HELLO:
		_send_helloack(stream);
		break;

	case ZRTP_COMMIT:
	{
		/* Passive Initiator can't talk to anyone */
		if (ZRTP_PASSIVE2_TEST(stream))
		{
			zrtp_statemachine_type_t role = _zrtp_machine_preparse_commit(stream, packet);
			if (ZRTP_STATEMACHINE_RESPONDER == role) {
				_zrtp_cancel_send_packet_later(stream, ZRTP_HELLO);
				status = _zrtp_machine_enter_pendingsecure(stream, packet);
			} else if (ZRTP_STATEMACHINE_INITIATOR == role) {
				_zrtp_cancel_send_packet_later(stream, ZRTP_HELLO);
				status = _zrtp_machine_start_initiating_secure(stream);
			} else {
				status = zrtp_status_fail;
			}
		} else {
			ZRTP_LOG(2,(_ZTU_,"\tERROR: The endpoint is in passive mode and Signaling Initiator -"
						" can't handle connections from anyone. ID=%u\n", stream->id));
			if (stream->zrtp->cb.event_cb.on_zrtp_protocol_event) {
				stream->zrtp->cb.event_cb.on_zrtp_protocol_event(stream, ZRTP_EVENT_IS_PASSIVE_RESTRICTION);
			}
			_zrtp_machine_enter_initiatingerror(stream, zrtp_error_service_unavail, 1);												
		}
	} break;

	case ZRTP_HELLOACK:
		_zrtp_cancel_send_packet_later(stream, ZRTP_HELLO);

		/* Start initiating the secure state if "autosecure" is enabled */
		if ((stream->session->profile.autosecure) && ZRTP_PASSIVE1_TEST(stream)) {
			status = _zrtp_machine_start_initiating_secure(stream);
		} else {
			if (!ZRTP_PASSIVE1_TEST(stream)) {
				if (stream->zrtp->cb.event_cb.on_zrtp_protocol_event) {
					stream->zrtp->cb.event_cb.on_zrtp_protocol_event(stream, ZRTP_EVENT_IS_PASSIVE_RESTRICTION);
				}
				ZRTP_LOG(2,(_ZTU_,"\tINFO: Switching to Clear due to Active/Passive restrictions.\n"));
			}
			status = _zrtp_machine_enter_clear(stream);
		}

		break;

	default:
		break;
	}

	return status;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_process_while_in_clear( zrtp_stream_t* stream,
												    zrtp_rtp_info_t* packet)
{
	zrtp_status_t s = zrtp_status_ok;

	switch (packet->type)
	{
	case ZRTP_GOCLEAR:
		_send_goclearack(stream);
		break;

	case ZRTP_HELLO:
		_send_helloack(stream);
		break;

	case ZRTP_COMMIT:
	{
		zrtp_statemachine_type_t role = _zrtp_machine_preparse_commit(stream, packet);
		if (ZRTP_STATEMACHINE_RESPONDER == role) {
			s = _zrtp_machine_enter_pendingsecure(stream, packet);
		} else if (ZRTP_STATEMACHINE_INITIATOR == role) {
			s = _zrtp_machine_start_initiating_secure(stream);
		} else {
			s = zrtp_status_fail;
		}
	} break;

	default:
		break;
	}

	return s;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_process_while_in_initiatingclear( zrtp_stream_t* stream,
															  zrtp_rtp_info_t* packet)
{
	zrtp_status_t s = zrtp_status_ok;

	switch (packet->type)
	{
	case ZRTP_GOCLEARACK:
	case ZRTP_COMMIT:
		s = _zrtp_machine_enter_clear(stream);
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
zrtp_status_t _zrtp_machine_process_while_in_pendingclear( zrtp_stream_t* stream,
														   zrtp_rtp_info_t* packet)
{
	zrtp_status_t s = zrtp_status_ok;

	switch (packet->type)
	{
	case ZRTP_GOCLEAR:
		_send_goclearack(stream);
		break;

	case ZRTP_COMMIT:
	{
		zrtp_statemachine_type_t role = _zrtp_machine_preparse_commit(stream, packet);
		if (ZRTP_STATEMACHINE_RESPONDER == role) {
			s = _zrtp_machine_enter_pendingsecure(stream, packet);
		} else if (ZRTP_STATEMACHINE_INITIATOR == role) {
			s = _zrtp_machine_start_initiating_secure(stream);
		} else {
			s = zrtp_status_fail;
		}
	} break;

	default:
		break;
	}

	return s;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_process_while_in_start_initiatingsecure( zrtp_stream_t* stream,
																	 zrtp_rtp_info_t* packet)
{
	zrtp_status_t s = zrtp_status_ok;

	switch (packet->type)
	{
	case ZRTP_HELLO:
		_send_helloack(stream);
		break;
			
	case ZRTP_COMMIT:
	{
		zrtp_statemachine_type_t role = _zrtp_machine_preparse_commit(stream, packet);
		if (ZRTP_STATEMACHINE_RESPONDER == role) {
			_zrtp_cancel_send_packet_later(stream, ZRTP_PROCESS);
			s = _zrtp_machine_enter_pendingsecure(stream, packet);
		} else {
			s = zrtp_status_fail;
		}
	} break;

	default:
		break;
	}

	return s;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_process_while_in_secure( zrtp_stream_t* stream,
													 zrtp_rtp_info_t* packet)
{
	zrtp_status_t s = zrtp_status_ok;

	switch (packet->type)
	{
	case ZRTP_CONFIRM2:
		_zrtp_packet_send_message(stream, ZRTP_CONFIRM2ACK, NULL);
		break;

	case ZRTP_SASRELAY:
		/*
		 * _zrtp_machine_process_sasrelay() updates SAS, sends events and does
		 * other things if SAS transferring is allowed
		 */
		s = _zrtp_machine_process_sasrelay(stream, packet);
		if (zrtp_status_ok == s) {
			_zrtp_packet_send_message(stream, ZRTP_RELAYACK, NULL);
		}
		break;

	case ZRTP_GOCLEAR:
		s = _zrtp_machine_process_goclear(stream, packet);
		if (zrtp_status_ok == s) {			
			s = _zrtp_machine_enter_pendingclear(stream);
			_send_goclearack(stream);
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

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_process_while_in_initiatingerror( zrtp_stream_t* stream,
															  zrtp_rtp_info_t* packet)
{
	switch (packet->type)
	{
	case ZRTP_ERROR:
		_zrtp_machine_enter_pendingerror(stream, ((zrtp_packet_Error_t*) packet->message)->code );
		break;
			 
	case ZRTP_ERRORACK:
		_zrtp_machine_switch_to_error(stream);
		break;
			 
	default:
		break;
	}

	return zrtp_status_ok;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_process_while_in_nozrtp( zrtp_stream_t* stream,
													 zrtp_rtp_info_t* packet)
{
	zrtp_status_t s = zrtp_status_ok;

	switch (packet->type)
	{
		case ZRTP_HELLO:
			s = _zrtp_machine_process_hello(stream, packet);
			if (zrtp_status_ok != s) {
				ZRTP_LOG(1,(_ZTU_,"\tERROR! _zrtp_machine_process_hello()3 failed with status=%d ID=%u.\n", s, stream->id));
				break;
			}
				
			_zrtp_change_state(stream, ZRTP_STATE_START);
			_zrtp_machine_start_send_and_resend_hello(stream);		
			break;
		
		case ZRTP_COMMIT: /* this logic should be similar to Commit handler in ZRTP_STATE_WAIT_HELLOACK state */
		{						
			/* Passive Initiator can't talk to anyone */
			if (ZRTP_PASSIVE2_TEST(stream))
			{
				zrtp_statemachine_type_t role = _zrtp_machine_preparse_commit(stream, packet);
				if (ZRTP_STATEMACHINE_RESPONDER == role) {					
					s = _zrtp_machine_enter_pendingsecure(stream, packet);
				} else if (ZRTP_STATEMACHINE_INITIATOR == role) {					
					s = _zrtp_machine_start_initiating_secure(stream);
				} else {
					s = zrtp_status_fail;
				}
			} else {
				ZRTP_LOG(2,(_ZTU_,"\tERROR: The endpoint is in passive mode and Signaling Initiator -"
							" can't handle connections from anyone. ID=%u\n", stream->id));				
				if (stream->zrtp->cb.event_cb.on_zrtp_protocol_event ) {
					stream->zrtp->cb.event_cb.on_zrtp_protocol_event(stream, ZRTP_EVENT_IS_PASSIVE_RESTRICTION);
				}				
				_zrtp_machine_enter_initiatingerror(stream, zrtp_error_service_unavail, 1);
			}
		} break;
			
		default:
			break;
	}

	return s;
}


/* Initiator logic */
extern zrtp_status_t _zrtp_machine_process_while_in_initiatingsecure(zrtp_stream_t* stream, zrtp_rtp_info_t* packet);
extern zrtp_status_t _zrtp_machine_process_while_in_waitconfirmack(zrtp_stream_t* stream, zrtp_rtp_info_t* packet);
extern zrtp_status_t _zrtp_machine_process_while_in_waitconfirm1(zrtp_stream_t* stream, zrtp_rtp_info_t* packet);

/* Responder logic */
extern zrtp_status_t _zrtp_machine_process_while_in_pendingsecure(zrtp_stream_t* stream, zrtp_rtp_info_t* packet);
extern zrtp_status_t _zrtp_machine_process_while_in_waitconfirm2(zrtp_stream_t* stream, zrtp_rtp_info_t* packet);

/* PBX transferring logic */
extern zrtp_status_t _zrtp_machine_process_while_in_sasrelaying(zrtp_stream_t* stream, zrtp_rtp_info_t* packet);

#if (defined(ZRTP_BUILD_FOR_CSD) && (ZRTP_BUILD_FOR_CSD == 1))
/* Driven Discovery state-machine */
extern zrtp_status_t _zrtp_machine_process_while_in_driven_initiator(zrtp_stream_t* stream, zrtp_rtp_info_t* packet);
extern zrtp_status_t _zrtp_machine_process_while_in_driven_responder(zrtp_stream_t* stream, zrtp_rtp_info_t* packet);
extern zrtp_status_t _zrtp_machine_process_while_in_driven_pending(zrtp_stream_t* stream, zrtp_rtp_info_t* packet);
#endif

state_handler_t* state_handler[ZRTP_STATE_COUNT] =
{
	NULL,
	NULL,
	_zrtp_machine_process_while_in_start,
	_zrtp_machine_process_while_in_wait4helloack,
	_zrtp_machine_process_while_in_wait4hello,
	_zrtp_machine_process_while_in_clear,
	_zrtp_machine_process_while_in_start_initiatingsecure,
	_zrtp_machine_process_while_in_initiatingsecure,
	_zrtp_machine_process_while_in_waitconfirm1,
	_zrtp_machine_process_while_in_waitconfirmack,
	_zrtp_machine_process_while_in_pendingsecure,
	_zrtp_machine_process_while_in_waitconfirm2,
	_zrtp_machine_process_while_in_secure,
	_zrtp_machine_process_while_in_sasrelaying,
	_zrtp_machine_process_while_in_initiatingclear,
	_zrtp_machine_process_while_in_pendingclear,
	_zrtp_machine_process_while_in_initiatingerror,
	NULL,
	NULL,
#if (defined(ZRTP_BUILD_FOR_CSD) && (ZRTP_BUILD_FOR_CSD == 1))
	_zrtp_machine_process_while_in_driven_initiator,
	_zrtp_machine_process_while_in_driven_responder,
	_zrtp_machine_process_while_in_driven_pending,
#endif
	_zrtp_machine_process_while_in_nozrtp
};
			 
			 
/*===========================================================================*/
/*		State switchers													     */
/*===========================================================================*/

static void _zrtp_machine_switch_to_error(zrtp_stream_t* stream)
{
	_zrtp_cancel_send_packet_later(stream, ZRTP_NONE);
	_clear_stream_crypto(stream);
	
	_zrtp_change_state(stream, ZRTP_STATE_ERROR);
	
	if (stream->zrtp->cb.event_cb.on_zrtp_security_event) {		
		stream->zrtp->cb.event_cb.on_zrtp_security_event(stream, ZRTP_EVENT_PROTOCOL_ERROR);
	}
	if (stream->zrtp->cb.event_cb.on_zrtp_not_secure) {		
		stream->zrtp->cb.event_cb.on_zrtp_not_secure(stream);
	}
    stream->last_error = 0;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_enter_pendingclear(zrtp_stream_t* stream)
{
	_zrtp_cancel_send_packet_later(stream, ZRTP_NONE);
	_zrtp_change_state(stream, ZRTP_STATE_PENDINGCLEAR);

	/*
	 * We have to destroy the ZRTP Session Key because user may not press "clear
	 * button", and the remote endpoint may subsequently initiate a new secure
	 * session.  Other secret values will be destroyed in Clear state or
	 * rewritten with new.
	 */
	{
		zrtp_string64_t new_zrtpsess = ZSTR_INIT_EMPTY(new_zrtpsess);
		// TODO: hash
		stream->session->hash->hash( stream->session->hash,
									 ZSTR_GV(stream->session->zrtpsess),
									 ZSTR_GV(new_zrtpsess));
		zrtp_zstrcpy(ZSTR_GV(stream->session->zrtpsess), ZSTR_GV(new_zrtpsess));
	}

	if (stream->zrtp->cb.event_cb.on_zrtp_protocol_event) {
		stream->zrtp->cb.event_cb.on_zrtp_protocol_event(stream, ZRTP_EVENT_IS_PENDINGCLEAR);
	}

	return zrtp_status_ok;
}

/*---------------------------------------------------------------------------*/
static zrtp_status_t _zrtp_machine_enter_initiatingclear(zrtp_stream_t* stream)
{	
	
	_zrtp_cancel_send_packet_later(stream, ZRTP_NONE);
	_zrtp_change_state(stream, ZRTP_STATE_INITIATINGCLEAR);
	
	{
	zrtp_string64_t new_zrtpsess = ZSTR_INIT_EMPTY(new_zrtpsess);
	// TODO: hash
	stream->session->hash->hash( stream->session->hash,
								 ZSTR_GV(stream->session->zrtpsess),
								 ZSTR_GV(new_zrtpsess));
	zrtp_zstrcpy(ZSTR_GV(stream->session->zrtpsess), ZSTR_GV(new_zrtpsess));
	}

	return _zrtp_machine_start_send_and_resend_goclear(stream);
}

/*---------------------------------------------------------------------------*/
static zrtp_status_t _zrtp_machine_enter_clear(zrtp_stream_t* stream)
{
	_zrtp_cancel_send_packet_later(stream, ZRTP_NONE);
	_clear_stream_crypto(stream);
	_zrtp_change_state(stream, ZRTP_STATE_CLEAR);

	if (stream->zrtp->cb.event_cb.on_zrtp_protocol_event) {
		stream->zrtp->cb.event_cb.on_zrtp_protocol_event(stream, ZRTP_EVENT_IS_CLEAR);
	}
	
	/*
	 * Now, let's check if the transition to CLEAR was caused by Active/Passive rules.
	 * If local endpoint is a MitM and peer MiTM linked stream is Unlimited, we
	 * could break the rules and send commit to Passive endpoint.
	 */
	if (stream->zrtp->is_mitm && stream->peer_passive) {
		if (stream->linked_mitm && stream->linked_mitm->peer_super_flag) {
			ZRTP_LOG(2,(_ZTU_,"INFO: Current stream ID=%u was switched to CLEAR-mode due to Active/Passive"
						" restrictions, but we are running in MiTM mode and peer linked stream is"
						" Super-active. Go Secure!\n", stream->id));
			
			/* @note: don't use zrtp_secure_stream() wrapper as it checks for Active/Passive stuff. */
			_zrtp_machine_start_initiating_secure(stream);
		}
	}

	return zrtp_status_ok;
}


/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_enter_initiatingerror( zrtp_stream_t *stream,
												   zrtp_protocol_error_t code,
												   uint8_t notif)
{
	if ( (ZRTP_STATE_ERROR != stream->state) &&
		 (ZRTP_STATE_INITIATINGERROR != stream->state) &&
		 (ZRTP_STATE_PENDINGERROR != stream->state) )
	{
		stream->last_error = code;
		
		ZRTP_LOG(3,(_ZTU_,"\tEnter InitiatingError State with ERROR:<%s>, notification %s. ID=%u\n",
				zrtp_log_error2str(stream->last_error), (notif?"Enabled":"Disabled"), stream->id));

		/* If we can't deliver a ZRTP message, just switch to the ERROR state. */
		if (notif) {
			_zrtp_cancel_send_packet_later(stream, ZRTP_NONE);
			_zrtp_change_state(stream, ZRTP_STATE_INITIATINGERROR);
			_zrtp_machine_start_send_and_resend_error(stream);
		} else {
			_zrtp_machine_switch_to_error(stream);
		}
	}
	
	return zrtp_status_ok;
}

zrtp_status_t _zrtp_machine_enter_pendingerror(zrtp_stream_t *stream, zrtp_protocol_error_t code)
{
	ZRTP_LOG(3,(_ZTU_,"\tEnter PendingError State with ERROR:<%s>. ID=%u\n",
				zrtp_log_error2str(stream->last_error), stream->id));
				
	_zrtp_cancel_send_packet_later(stream, ZRTP_NONE);
	_zrtp_change_state(stream, ZRTP_STATE_PENDINGERROR);

	stream->last_error = code;
	_zrtp_machine_start_send_and_resend_errorack(stream);
	return zrtp_status_ok;
}


/*===========================================================================*/
/*		Packet handlers														 */
/*===========================================================================*/

zrtp_status_t _zrtp_machine_process_goclear(zrtp_stream_t* stream, zrtp_rtp_info_t* packet)
{
	zrtp_packet_GoClear_t *goclear	= (zrtp_packet_GoClear_t*) packet->message;
	zrtp_string128_t clear_hmac = ZSTR_INIT_EMPTY(clear_hmac);
	static const zrtp_string16_t clear_hmac_str	= ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_CLEAR_HMAC_STR);

	if (!stream->allowclear) {
		ZRTP_LOG(2, (_ZTU_,"\tWARNING! Allowclear is disabled but GoClear was received. ID=%u.\n", stream->id));		
		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_goclear_unsp, 1);
		return zrtp_status_fail;
	}

	stream->session->hash->hmac( stream->session->hash,
								 ZSTR_GV(stream->cc.peer_hmackey),
								 ZSTR_GV(clear_hmac_str),
								 ZSTR_GV(clear_hmac));
	clear_hmac.length = ZRTP_HMAC_SIZE;

	if (0 != zrtp_memcmp(clear_hmac.buffer, goclear->clear_hmac, ZRTP_HMAC_SIZE)) {
		ZRTP_LOG(2, (_ZTU_,"\tWARNING! Wrong GoClear hmac. ID=%u.\n", stream->id));
		return zrtp_status_fail; /* EH: Just ignore malformed packets */
	}

	return zrtp_status_ok;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_process_hello(zrtp_stream_t* stream, zrtp_rtp_info_t* packet)
{
	zrtp_session_t* session = stream->session;
    zrtp_packet_Hello_t* peer_hello = NULL;
	uint32_t comp_block_len = 0;
	uint8_t id = 0;

	/* Size of HELLO packet must be bigger then <RTP+static HELLO part>. */
	if (*(packet->length) < (ZRTP_MIN_PACKET_LENGTH + ZRTP_HELLO_STATIC_SIZE + ZRTP_HMAC_SIZE)) {
		ZRTP_LOG(2,(_ZTU_,"\tWARNING! Wrong HELLO static size=%d must be=%d. ID=%u\n", *packet->length,
					ZRTP_MIN_PACKET_LENGTH + ZRTP_HELLO_STATIC_SIZE + ZRTP_HMAC_SIZE, stream->id));

		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_invalid_packet, 1);
		return zrtp_status_fail;
	}

	peer_hello = (zrtp_packet_Hello_t*) packet->message;

	/* Now we can verify packet size according to size of its parts */
	comp_block_len = ( peer_hello->hc + peer_hello->cc +
					   peer_hello->ac + peer_hello->kc +
					   peer_hello->sc) * ZRTP_COMP_TYPE_SIZE;

	if (*packet->length < (ZRTP_MIN_PACKET_LENGTH + ZRTP_HELLO_STATIC_SIZE + comp_block_len + ZRTP_HMAC_SIZE))
	{
		ZRTP_LOG(2,(_ZTU_,"\tWARNING! Wrong HELLO dynamic size=%d must be=%d. ID=%u\n", *packet->length,
					comp_block_len+ ZRTP_MIN_PACKET_LENGTH + ZRTP_HELLO_STATIC_SIZE + ZRTP_HMAC_SIZE, stream->id));

		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_invalid_packet, 1);
		return zrtp_status_fail;
	}

	/* Every component quantity must be less than or equal to 7 */
	if ( (peer_hello->hc > ZRTP_MAX_COMP_COUNT) || (peer_hello->cc > ZRTP_MAX_COMP_COUNT) ||
		 (peer_hello->ac > ZRTP_MAX_COMP_COUNT) || (peer_hello->kc > ZRTP_MAX_COMP_COUNT) ||
		 (peer_hello->sc > ZRTP_MAX_COMP_COUNT) )
	{
		ZRTP_LOG(2,(_ZTU_,"\tWARNING! Wrong HELLO packet data. Components count can't be greater"
					" then 7. ID=%u\n", stream->id));

		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_invalid_packet, 1);
		return zrtp_status_fail;
	}
	
	/* Print out ZRTP Hello message for debug purposes */	
	{
	char print_buffer[ZRTP_MAX_COMP_COUNT*20];	
	zrtp_memcpy(print_buffer, peer_hello->comp, comp_block_len);
	print_buffer[comp_block_len] = 0;
	ZRTP_LOG(3,(_ZTU_,"\tProcessing HELLO from %.16s V=%.4s, P=%d, M=%d.\n",
				peer_hello->cliend_id, peer_hello->version, peer_hello->pasive, peer_hello->mitmflag));
	ZRTP_LOG(3,(_ZTU_,"\t\tac=%d cc=%d sc=%d kc=%d\n",
				peer_hello->ac, peer_hello->cc, peer_hello->sc, peer_hello->kc));
	ZRTP_LOG(3,(_ZTU_,"\t\t%s\n", print_buffer));
	}
	
	/*
	 * Check protocol version. Try to resolve versions missmatch according to ZRTP Draft sec. 5.1
	 */
	{
		uint32_t peer_version = 0;
		peer_version = (char)((*peer_hello->version) - '0') *10; /* only 3 first octets are significant */
		peer_version += (char)(*(peer_hello->version+2) - '0');
				
		if ((ZRTP_PROTOCOL_VERSION_VALUE/10) == peer_version) {
			ZRTP_LOG(3,(_ZTU_,"\tReceived HELLO had the same protocol V.\n"));
		}
		else if ((ZRTP_PROTOCOL_VERSION_VALUE/10) < peer_version) {
			ZRTP_LOG(2,(_ZTU_,"\tWARNING! Received HELLO greater ZRTP V=%d - wait for other party"
						" to resolve this issue. ID=%u.\n", peer_version, stream->id));
		} else {
			ZRTP_LOG(2,(_ZTU_,"\tWARNING! Received a ZRTP_HELLO smaller ZRTP V=%d and we don't"
						" support it - terminate session. ID=%u\n", peer_version, stream->id));
			
			_zrtp_machine_enter_initiatingerror(stream, zrtp_error_version, 1);
			return zrtp_status_fail;
		}
	}
	
	/* Close session if ZID duplication */
	if (!zrtp_memcmp(stream->messages.hello.zid, peer_hello->zid, sizeof(zrtp_zid_t))) {
		ZRTP_LOG(2,(_ZTU_,ZRTP_EQUAL_ZID_WARNING_STR));
		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_equal_zid, 1);
		return zrtp_status_fail;
	}	

	/* All streams within a single session MUST have the same ZID */
	if (session->peer_zid.length > 0) {
		if (0 != zrtp_memcmp(session->peer_zid.buffer, peer_hello->zid, sizeof(zrtp_zid_t))) {
			ZRTP_LOG(2,(_ZTU_,"\tWARNING! Received HELLO which had a different ZID from that of the"
						" previous stream within the same session. sID=%u ID=%u\n", session->id, stream->id));

			_zrtp_machine_enter_initiatingerror(stream, zrtp_error_wrong_zid, 1);
			return zrtp_status_fail;
		}
	} else {
		zrtp_zstrncpyc(ZSTR_GV(session->peer_zid), (const char*) peer_hello->zid, sizeof(zrtp_zid_t));
	}

	/*
	 * Process Remote flags.
	 */
	if (peer_hello->pasive && peer_hello->uflag) {
		ZRTP_LOG(2,(_ZTU_,"\tWARNING! Received HELLO which both P and U flags set.\n"));
		return zrtp_status_fail;
	}
	
	stream->peer_passive = peer_hello->pasive;		
	stream->peer_super_flag = peer_hello->uflag;
	
	stream->peer_mitm_flag = peer_hello->mitmflag;
	if (stream->peer_mitm_flag) {
		stream->mitm_mode = ZRTP_MITM_MODE_CLIENT;
	}
	
	/* Current version doesn't support Digital Signatures. Ignore peer Hello with S flag enabled. */
	if (peer_hello->sigflag) {
		ZRTP_LOG(2,(_ZTU_,"\tWARNING! Received a ZRTP_HELLO with S flag enabled. We don't support Digital Signatures - ignore message.\n"));
		return zrtp_status_fail;
	}
	
	/* Copy packet for future hashing */
	zrtp_memcpy(&stream->messages.peer_hello, peer_hello, zrtp_ntoh16(peer_hello->hdr.length)*4);
	stream->is_hello_received = 1;

	/*
	 * Choose PK exchange scheme and PK mode.
	 * We do this right after receiving Hello to speedup DH calculations.
	 */
	stream->pubkeyscheme = zrtp_comp_find(ZRTP_CC_PKT, ZRTP_PKTYPE_DH3072, session->zrtp);
	id = _zrtp_choose_best_comp(&session->profile, peer_hello, ZRTP_CC_PKT);
	if (id != ZRTP_COMP_UNKN) {
		stream->pubkeyscheme = zrtp_comp_find(ZRTP_CC_PKT, id, session->zrtp);
	}
	
	ZRTP_LOG(3,(_ZTU_,"\tReceived HELLO Accepted\n"));
	
    return zrtp_status_ok;
}


/*===========================================================================*/
/*		Packet senders														 */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
static void _send_and_resend_hello(zrtp_stream_t* stream, zrtp_retry_task_t* task)
{	
	if ((task->_retrys == ZRTP_NO_ZRTP_FAST_COUNT) && !stream->is_hello_received) {
		ZRTP_LOG(2,(_ZTU_,"WARNING! HELLO have been resent %d times without a response."
					" Raising ZRTP_EVENT_NO_ZRTP_QUICK event. ID=%u\n", task->_retrys, stream->id));

		if (stream->zrtp->cb.event_cb.on_zrtp_protocol_event) {
			stream->zrtp->cb.event_cb.on_zrtp_protocol_event(stream, ZRTP_EVENT_NO_ZRTP_QUICK);
		}
	}
	
	if (task->_retrys >= (uint32_t)((ZRTP_STATE_WAIT_HELLOACK==stream->state)?ZRTP_T1_MAX_COUNT_EXT:ZRTP_T1_MAX_COUNT)) {				
		ZRTP_LOG(2,(_ZTU_,"WARNING! HELLO Max retransmissions count reached (%d retries). ID=%u\n", task->_retrys, stream->id));

		_zrtp_cancel_send_packet_later(stream, ZRTP_NONE);
		_clear_stream_crypto(stream);
		_zrtp_change_state(stream, ZRTP_STATE_NO_ZRTP);
		
		if (stream->zrtp->cb.event_cb.on_zrtp_protocol_event) {
			stream->zrtp->cb.event_cb.on_zrtp_protocol_event(stream, ZRTP_EVENT_NO_ZRTP);
		}
	} else if (task->_is_enabled) {		
		zrtp_status_t s = _zrtp_packet_send_message(stream, ZRTP_HELLO, &stream->messages.hello);
		task->timeout = _zrtp_get_timeout((uint32_t)task->timeout, ZRTP_HELLO);
		if (zrtp_status_ok == s) {
			task->_retrys++;
		}
		
		
		if (stream->zrtp->cb.sched_cb.on_call_later) {
			stream->zrtp->cb.sched_cb.on_call_later(stream, task);
		}
	}
}

zrtp_status_t _zrtp_machine_start_send_and_resend_hello(zrtp_stream_t* stream)
{
	zrtp_retry_task_t* task = &stream->messages.hello_task;
	
	task->_is_enabled = 1;
	task->callback = _send_and_resend_hello;
	task->_retrys = 0;
	
	_send_and_resend_hello(stream, task);
	
	return zrtp_status_ok;
}

static void _send_helloack(zrtp_stream_t* stream)
{
	_zrtp_packet_send_message(stream, ZRTP_HELLOACK, NULL);
}


/*---------------------------------------------------------------------------*/
static void _send_and_resend_goclear(zrtp_stream_t* stream, zrtp_retry_task_t* task)
{
	if (task->_is_enabled) {
		if (task->_retrys > ZRTP_T2_MAX_COUNT) {
			ZRTP_LOG(2,(_ZTU_,"\tWARNING!: GOCLEAR Nax retransmissions count reached. ID=%u\n", stream->id));
			_zrtp_machine_enter_clear(stream);
		} else {
			zrtp_packet_GoClear_t* goclear = (zrtp_packet_GoClear_t*) &stream->messages.goclear;

			_zrtp_packet_send_message(stream, ZRTP_GOCLEAR, goclear);
			task->_retrys++;
			if (stream->zrtp->cb.sched_cb.on_call_later) {
				stream->zrtp->cb.sched_cb.on_call_later(stream, task);
			}
		}
	}
}

static zrtp_status_t  _zrtp_machine_start_send_and_resend_goclear(zrtp_stream_t* stream)
{
	zrtp_retry_task_t* task = &stream->messages.goclear_task;
	zrtp_string128_t clear_hmac = ZSTR_INIT_EMPTY(clear_hmac);
	static const zrtp_string16_t clear_hmac_str	= ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_CLEAR_HMAC_STR);
	
	zrtp_memset(&stream->messages.goclear, 0, sizeof(zrtp_packet_GoClear_t));
	
	/* Compute Clear HMAC as: HMAC(hmackey, "Clear hmac") */
	stream->session->hash->hmac( stream->session->hash,
								 ZSTR_GV(stream->cc.hmackey),
								 ZSTR_GV(clear_hmac_str),
								 ZSTR_GV(clear_hmac));
	clear_hmac.length = ZRTP_HMAC_SIZE;
	
	zrtp_memcpy(stream->messages.goclear.clear_hmac, clear_hmac.buffer, clear_hmac.length);	
	_zrtp_packet_fill_msg_hdr( stream,
							   ZRTP_GOCLEAR,
							   sizeof(zrtp_packet_GoClear_t) - sizeof(zrtp_msg_hdr_t),
							   &stream->messages.goclear.hdr);
	
	task->_is_enabled	= 1;
	task->callback		= _send_and_resend_goclear;
	task->timeout		= ZRTP_T2;
	task->_retrys		= 0;
	
	_send_and_resend_goclear(stream, task);
	
	return zrtp_status_ok;
}


static void _send_goclearack(zrtp_stream_t* stream)
{
	_zrtp_packet_send_message(stream, ZRTP_GOCLEARACK, NULL);
}

/*---------------------------------------------------------------------------*/
static void _send_and_resend_error(zrtp_stream_t* stream, zrtp_retry_task_t* task)
{
	if (task->_retrys >= ZRTP_ETI_MAX_COUNT) {
		ZRTP_LOG(2,(_ZTU_,"\tWARNING! ERROR Max retransmissions count reached. ID=%u\n", stream->id));
		_zrtp_machine_switch_to_error(stream);
	} else if (task->_is_enabled) {
		if (zrtp_status_ok == _zrtp_packet_send_message(stream, ZRTP_ERROR, &stream->messages.error)) {
			task->_retrys++;
		}
		if (stream->zrtp->cb.sched_cb.on_call_later) {
			stream->zrtp->cb.sched_cb.on_call_later(stream, task);
		}
	}
}

static zrtp_status_t  _zrtp_machine_start_send_and_resend_error(zrtp_stream_t* stream)
{
	zrtp_retry_task_t* task = &stream->messages.error_task;
	
	zrtp_memset(&stream->messages.error, 0, sizeof(zrtp_packet_Error_t));
	stream->messages.error.code = zrtp_hton32(stream->last_error);
	
	_zrtp_packet_fill_msg_hdr( stream,
							   ZRTP_ERROR,
							   sizeof(zrtp_packet_Error_t) - sizeof(zrtp_msg_hdr_t),
							   &stream->messages.error.hdr);
	
	task->_is_enabled	= 1;
	task->callback		= _send_and_resend_error;
	task->timeout		= ZRTP_ET;
	task->_retrys		= 0;
	
	_send_and_resend_error(stream, task);
	
	return zrtp_status_ok;
}

/*---------------------------------------------------------------------------*/
static void _send_and_resend_errorack(zrtp_stream_t* stream, zrtp_retry_task_t* task)
{
	if (task->_retrys >= ZRTP_ETR_MAX_COUNT) {
		ZRTP_LOG(2,(_ZTU_,"\tWARNING! ERRORACK Max retransmissions count reached. ID=%u\n", stream->id));
		_zrtp_machine_switch_to_error(stream);
	} else if (task->_is_enabled) {
		if (zrtp_status_ok == _zrtp_packet_send_message(stream, ZRTP_ERRORACK, NULL)) {
			task->_retrys++;
		}
		if (stream->zrtp->cb.sched_cb.on_call_later) {
			stream->zrtp->cb.sched_cb.on_call_later(stream, task);
		}
	}
}

static zrtp_status_t  _zrtp_machine_start_send_and_resend_errorack(zrtp_stream_t* stream)
{
	zrtp_retry_task_t* task = &stream->messages.errorack_task;
	
	task->_is_enabled	= 1;
	task->callback		= _send_and_resend_errorack;
	task->timeout		= ZRTP_ET;
	task->_retrys		= 0;
	
	_send_and_resend_errorack(stream, task);
	
	return zrtp_status_ok;
}


void _clear_stream_crypto(zrtp_stream_t* stream)
{
	if (stream->protocol) {
		_zrtp_protocol_destroy(stream->protocol);
		stream->protocol = 0;
	}

	zrtp_wipe_zstring(ZSTR_GV(stream->cc.hmackey));
	zrtp_wipe_zstring(ZSTR_GV(stream->cc.peer_hmackey));
	zrtp_wipe_zstring(ZSTR_GV(&stream->cc.zrtp_key));
	zrtp_wipe_zstring(ZSTR_GV(stream->cc.peer_zrtp_key));
}
