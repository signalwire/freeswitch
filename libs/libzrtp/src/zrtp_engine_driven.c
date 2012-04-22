/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include "zrtp.h"

#define _ZTU_ "zrtp dengine"


#if (defined(ZRTP_BUILD_FOR_CSD) && (ZRTP_BUILD_FOR_CSD == 1))

extern zrtp_status_t _zrtp_machine_process_hello(zrtp_stream_t* stream, zrtp_rtp_info_t* packet);
extern zrtp_status_t start_send_and_resend_hello(zrtp_stream_t* stream);
extern zrtp_status_t start_initiating_secure(zrtp_stream_t *stream);
extern zrtp_status_t _zrtp_machine_start_send_and_resend_hello(zrtp_stream_t* stream);


/*----------------------------------------------------------------------------*/
void zrtp_driven_stream_start(zrtp_stream_t* stream, zrtp_statemachine_type_t role)
{	
	
	ZRTP_LOG(3,(_ZTU_,"START Driven %s Stream ID=%u mode=%s state=%s.",
				(ZRTP_STATEMACHINE_INITIATOR == role)?"INITIATOR":"RESPONDER",
				stream->id, zrtp_log_mode2str(stream->mode), zrtp_log_state2str(stream->state)));
		
	/* This function can be called in parallel to the main processing loop protect internal stream data. */
	zrtp_mutex_lock(stream->stream_protector);
	
	if ( (ZRTP_STATE_ACTIVE != stream->state) && 
		 (ZRTP_STATE_ERROR != stream->state) &&
	     (ZRTP_STATE_NO_ZRTP != stream->state))
	{
		ZRTP_LOG(1,(_ZTU_,"ERROR! can't start stream ID=%u from state %d.", stream->id, stream->state));
	}
	else
	{		
		if (ZRTP_STATEMACHINE_INITIATOR == role) {
			_zrtp_change_state(stream, ZRTP_STATE_DRIVEN_INITIATOR);
			_zrtp_machine_start_send_and_resend_hello(stream);
		} else if (ZRTP_STATEMACHINE_RESPONDER == role) {
			_zrtp_change_state(stream, ZRTP_STATE_DRIVEN_RESPONDER);
		}
	}
	
	zrtp_mutex_unlock(stream->stream_protector);
}

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_process_while_in_driven_initiator( zrtp_stream_t* stream,
															   zrtp_rtp_info_t* packet)
{
	zrtp_status_t s = zrtp_status_ok;
	
	switch (packet->type)
	{
	case ZRTP_HELLO: {
		s = _zrtp_machine_process_hello(stream, packet);
		if (zrtp_status_ok != s) {
			ZRTP_LOG(1,(_ZTU_,"ERROR! _zrtp_machine_process_hello()4 failed with status=%d. ID=%u",s, stream->id));
			break; /* Just stay in DRIVEN_INITIATOR state. */
		}
		
		/* Now we have ZIDs for both sides and can upload secrets from the cache */
		s = _zrtp_prepare_secrets(stream->session);
		if (zrtp_status_ok != s) {
			ZRTP_LOG(1,(_ZTU_,"ERROR! _zrtp_prepare_secrets()3 failed with status=%d. ID=%u",s, stream->id));
			break; /* Just stay in START state. */
		}
		
		// TODO: handle autosecure and licensing modes there
		_zrtp_cancel_send_packet_later(stream, ZRTP_HELLO);							
		stream->mode = _zrtp_define_stream_mode(stream);		
		s = _zrtp_machine_enter_initiatingsecure(stream);
	} break;
			
	default:
		break;
	}
	
	return s;
}

zrtp_status_t _zrtp_machine_process_while_in_driven_responder( zrtp_stream_t* stream,
															   zrtp_rtp_info_t* packet)
{
	zrtp_status_t s = zrtp_status_ok;
	
	switch (packet->type)
	{
	case ZRTP_HELLO: {
		s = _zrtp_machine_process_hello(stream, packet);
		if (zrtp_status_ok != s) {
			ZRTP_LOG(1,(_ZTU_,"ERROR! _zrtp_machine_process_hello()5 failed with status=%d. ID=%u", s, stream->id));
			break; /* Just stay in DRIVEN_INITIATOR state. */
		}
		
		/* Now we have ZIDs for both sides and can upload secrets from the cache */
		s = _zrtp_prepare_secrets(stream->session);
		if (zrtp_status_ok != s) {
			ZRTP_LOG(1,(_ZTU_,"ERROR! _zrtp_prepare_secrets()4 failed with status=%d. ID=%u", s, stream->id));
			break; /* Just stay in START state. */
		}
		
		// TODO: handle autosecure and licensing modes there				
		s = _zrtp_packet_send_message(stream, ZRTP_HELLO, &stream->messages.hello);
		if (zrtp_status_ok == s) {
			_zrtp_change_state(stream, ZRTP_STATE_DRIVEN_PENDING);
		}
	} break;
		
	default:
		break;
	}
	
	return s;
}

zrtp_status_t _zrtp_machine_process_while_in_driven_pending( zrtp_stream_t* stream,
															 zrtp_rtp_info_t* packet)
{
	zrtp_status_t s = zrtp_status_ok;
	
	switch (packet->type)
	{
	case ZRTP_HELLO: {
		s = _zrtp_packet_send_message(stream, ZRTP_HELLO, &stream->messages.hello);
	} break;
	
	case ZRTP_COMMIT: {
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

#endif /* ZRTP_BUILD_FOR_CSD */
