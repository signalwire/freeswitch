/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include "zrtp.h"

#define _ZTU_ "zrtp utils"

/*----------------------------------------------------------------------------*/
static uint32_t _estimate_index(uint32_t seq, uint32_t s_l)
{
    uint32_t v;
    uint32_t roc = (s_l >> 16) & 0xffff;
    
	/* from RFC 3711, Appendix A */
    if (0 == s_l) {
		return seq;
    }

    s_l &= 0xfffful;
    if (s_l < 32768ul) {
		v = (seq < s_l) ? roc : ((seq - s_l > 32768ul) ? (roc ? (roc - 1) : 0) : roc);
    } else {
		v = (s_l - 32768ul > seq) ? (roc + 1) : roc;
    }

    return seq | (v << 16);
}

/**
 * @brief Converts RTP sequence number to implicit representation.
 * @sa section 3.3.1 of RFC 3711
 * @param self - ZRTP stream context associated with the packet;
 * @param packet - RTP packet for converting;
 * @param is_media - 1 - assumes RTP media packet and 0 - ZRTP protocol message;
 * @param is_input - 1 assumes incoming and 0 - outgoing packet direction.
 * @return resulting sequence number.
 */
static uint32_t _convert_seq_to_implicit_seq( zrtp_stream_t *ctx,
											  char *packet,
											  uint8_t is_media,
											  uint8_t is_input)
{
    uint32_t header_seq = 0;
	uint32_t ctx_seq = 0;
	ZRTP_UNALIGNED(zrtp_rtp_hdr_t) *rtp_hdr = (zrtp_rtp_hdr_t*)packet;

	if (is_input) {
		ctx_seq = is_media ? ctx->media_ctx.high_in_media_seq : ctx->media_ctx.high_in_zrtp_seq;
	}
	else {
		ctx_seq = is_media ? ctx->media_ctx.high_out_media_seq : ctx->media_ctx.high_out_zrtp_seq;
	}
	
	header_seq = _estimate_index(zrtp_ntoh16(rtp_hdr->seq), ctx_seq);

	if (0 == ctx_seq || header_seq > ctx_seq) /* as per section 3.3.1 of RFC 3711 */
    {
		if (is_input) {
			if (is_media) {
    			ctx->media_ctx.high_in_media_seq = header_seq;
			} else {
				ctx->media_ctx.high_in_zrtp_seq = header_seq;
			}
		} else {
			if (is_media) {
    			ctx->media_ctx.high_out_media_seq = header_seq;
			} else {
				ctx->media_ctx.high_out_zrtp_seq = header_seq;
			}
		}
    }
    
	return header_seq;
}

/*----------------------------------------------------------------------------*/
zrtp_status_t _zrtp_packet_fill_msg_hdr( zrtp_stream_t *stream,								  
										 zrtp_msg_type_t type,
										 uint16_t body_length,
										 zrtp_msg_hdr_t* hdr)
{	
	char *key = NULL;

	switch (type)
    {
	case ZRTP_HELLO:
		zrtp_memcpy(hdr->type, "Hello   ", ZRTP_PACKET_TYPE_SIZE);
		key = (char*)stream->messages.commit.hash;
	    break;
	case ZRTP_HELLOACK:
	    zrtp_memcpy(hdr->type, "HelloACK", ZRTP_PACKET_TYPE_SIZE);
	    break;
	case ZRTP_COMMIT:
	    zrtp_memcpy(hdr->type, "Commit  ", ZRTP_PACKET_TYPE_SIZE);
		key = (char*)stream->messages.dhpart.hash;
	    break;
	case ZRTP_DHPART1:
	    zrtp_memcpy(hdr->type, "DHPart1 ", ZRTP_PACKET_TYPE_SIZE);
		key = stream->messages.h0.buffer;
	    break;
	case ZRTP_DHPART2:
	    zrtp_memcpy(hdr->type, "DHPart2 ", ZRTP_PACKET_TYPE_SIZE);
		key = stream->messages.h0.buffer;
	    break;
	case ZRTP_CONFIRM2ACK:
	    zrtp_memcpy(hdr->type, "Conf2ACK", ZRTP_PACKET_TYPE_SIZE);
	    break;
	case ZRTP_GOCLEAR:
	    zrtp_memcpy(hdr->type, "GoClear ", ZRTP_PACKET_TYPE_SIZE);
	    break;
	case ZRTP_GOCLEARACK:
	    zrtp_memcpy(hdr->type, "ClearACK", ZRTP_PACKET_TYPE_SIZE);
	    break;
	case ZRTP_ERROR:
	    zrtp_memcpy(hdr->type, "Error   ", ZRTP_PACKET_TYPE_SIZE);
	    break;
	case ZRTP_ERRORACK:
	    zrtp_memcpy(hdr->type, "ErrorACK", ZRTP_PACKET_TYPE_SIZE);
	    break;
	case ZRTP_CONFIRM1:
	    zrtp_memcpy(hdr->type, "Confirm1", ZRTP_PACKET_TYPE_SIZE);		
	    break;
	case ZRTP_CONFIRM2:
	    zrtp_memcpy(hdr->type, "Confirm2", ZRTP_PACKET_TYPE_SIZE);		
	    break;
	case ZRTP_SASRELAY:
	    zrtp_memcpy(hdr->type, "SASrelay", ZRTP_PACKET_TYPE_SIZE);
		break;
	case ZRTP_RELAYACK:
		zrtp_memcpy(hdr->type, "RelayACK", ZRTP_PACKET_TYPE_SIZE);
		break;
	case ZRTP_ZFONEPINGACK:
		zrtp_memcpy(hdr->type, "PingACK ", ZRTP_PACKET_TYPE_SIZE);
		break;
		
	default:
	    return zrtp_status_bad_param;
    }


	hdr->magic = zrtp_hton16(ZRTP_MESSAGE_MAGIC);
								/* message type + length intelf */
	hdr->length = zrtp_hton16((ZRTP_PACKET_TYPE_SIZE + 4 + body_length) / 4);

	if (key)
	{
		char *hmac = (char*)hdr + ZRTP_PACKET_TYPE_SIZE + 4 + body_length - ZRTP_HMAC_SIZE;
		zrtp_hash_t *hash = zrtp_comp_find(ZRTP_CC_HASH, ZRTP_HASH_SHA256, stream->zrtp);
		zrtp_string32_t hmac_buff = ZSTR_INIT_EMPTY(hmac_buff);

		hash->hmac_truncated_c( hash,
								(const char*)key,
								ZRTP_MESSAGE_HASH_SIZE,
								(char*)hdr,
								ZRTP_PACKET_TYPE_SIZE + 4 + body_length - ZRTP_HMAC_SIZE,
								ZRTP_HMAC_SIZE,
								ZSTR_GV(hmac_buff) );
		zrtp_memcpy(hmac, hmac_buff.buffer, ZRTP_HMAC_SIZE);
	}
	
	return zrtp_status_ok;
}

/*----------------------------------------------------------------------------*/
zrtp_msg_type_t _zrtp_packet_get_type(ZRTP_UNALIGNED(zrtp_rtp_hdr_t) *hdr,  uint32_t length)
{
	char *type = NULL;

	if (ZRTP_PACKETS_MAGIC != zrtp_ntoh32(hdr->ts)) {
		/* This is non ZRTP packet */
		return ZRTP_NONE;
	} else if (length < (ZRTP_MIN_PACKET_LENGTH)) {		
		/* Malformed packet: ZRTP MAGIC is present, but size is too small */
		return ZRTP_UNPARSED;
	}
	
	/* Shifting to ZRTP packet type field: <RTP header> + <extension header> */
    type = (char*)(hdr) + sizeof(zrtp_rtp_hdr_t) + 4;

    switch (*type++)
    {
	case 'C':
	case 'c':
		if (0 == zrtp_memcmp(type, "ommit  ", ZRTP_PACKET_TYPE_SIZE-1))
	    	return ZRTP_COMMIT;
		if (0 == zrtp_memcmp(type, "onf2ACK", ZRTP_PACKET_TYPE_SIZE-1))
			return ZRTP_CONFIRM2ACK;
		if (0 == zrtp_memcmp(type, "onfirm1", ZRTP_PACKET_TYPE_SIZE-1))
	    	return ZRTP_CONFIRM1;
	    if (0 == zrtp_memcmp(type, "onfirm2", ZRTP_PACKET_TYPE_SIZE-1))
	    	return ZRTP_CONFIRM2;
		if (0 == zrtp_memcmp(type, "learACK", ZRTP_PACKET_TYPE_SIZE-1))
	    	return ZRTP_GOCLEARACK;		
		break;
	
	case 'D':
	case 'd':
		if (0 == zrtp_memcmp(type, "HPart1 ", ZRTP_PACKET_TYPE_SIZE-1))
	    	return ZRTP_DHPART1;
		if (0 == zrtp_memcmp(type, "HPart2 ", ZRTP_PACKET_TYPE_SIZE-1))
	    	return ZRTP_DHPART2;
		break;
	
	case 'E':
	case 'e':
		if (0 == zrtp_memcmp(type, "rror   ", ZRTP_PACKET_TYPE_SIZE-1))
	    	return ZRTP_ERROR;
		if (0 == zrtp_memcmp(type, "rrorACK", ZRTP_PACKET_TYPE_SIZE-1))
	    	return ZRTP_ERRORACK;
		break;
	
	case 'G':
	case 'g':
	    if (0 == zrtp_memcmp(type, "oClear ", ZRTP_PACKET_TYPE_SIZE-1))
			return ZRTP_GOCLEAR;
	    break;
	
	case 'H':
	case 'h':
		if (0 == zrtp_memcmp(type, "ello   ", ZRTP_PACKET_TYPE_SIZE-1))
	    	return ZRTP_HELLO;
		if (0 == zrtp_memcmp(type, "elloACK", ZRTP_PACKET_TYPE_SIZE-1))	    
    		return ZRTP_HELLOACK;
	    break;
	
	case 'P':
	case 'p':
	    if (0 == zrtp_memcmp(type, "ing    ", ZRTP_PACKET_TYPE_SIZE-1))
	    	return ZRTP_ZFONEPING;
		if (0 == zrtp_memcmp(type, "ingACK ", ZRTP_PACKET_TYPE_SIZE-1))
	    	return ZRTP_ZFONEPINGACK;
		break;

	case 'R':
	case 'r':
	    if (0 == zrtp_memcmp(type, "elayACK", ZRTP_PACKET_TYPE_SIZE-1))		
	    	return ZRTP_RELAYACK;
		break;

	case 'S':
	case 's':
	    if (0 == zrtp_memcmp(type, "ASrelay", ZRTP_PACKET_TYPE_SIZE-1))		
	    	return ZRTP_SASRELAY;
		break;
    }

    return ZRTP_NONE;
}

/*----------------------------------------------------------------------------*/
int _zrtp_packet_send_message(zrtp_stream_t* stream, zrtp_msg_type_t type, const void* message)
{
	ZRTP_UNALIGNED(zrtp_rtp_hdr_t) *rtp_hdr = NULL;

	zrtp_msg_hdr_t* zrtp_hdr = NULL;
    uint32_t packet_length = sizeof(zrtp_rtp_hdr_t);	
	zrtp_status_t s = zrtp_status_ok;

#if (defined(ZRTP_USE_STACK_MINIM) && (ZRTP_USE_STACK_MINIM == 1))
    char* buffer = zrtp_sys_alloc(1500);
	if (!buffer) {
		return zrtp_status_alloc_fail;
	}
#else
	char buffer[1500];
#endif

	rtp_hdr = (zrtp_rtp_hdr_t*)buffer;
    
    /* Fill main RTP packet fields */
    zrtp_memset(rtp_hdr, 0, sizeof(zrtp_rtp_hdr_t));
	rtp_hdr->x = 1;
    rtp_hdr->ssrc = stream->media_ctx.ssrc;
    
	/* Increment ZRTP RTP sequences space */
    rtp_hdr->seq = zrtp_hton16((++stream->media_ctx.high_out_zrtp_seq) & 0xffff); 
    if (stream->media_ctx.high_out_zrtp_seq >= 0xffff)  {
		stream->media_ctx.high_out_zrtp_seq = 0;
    }
    
    /* Set ZRTP MAGIC instead of timestamp and as a extension type */
    rtp_hdr->ts = zrtp_hton32(ZRTP_PACKETS_MAGIC);
	
	if (message) {
		zrtp_memcpy( buffer + RTP_HDR_SIZE,
					 (char*)message,
					 zrtp_ntoh16(((zrtp_msg_hdr_t*) message)->length)*4 );					 
	} else {
		/* May be it's a primitive packet and we should fill ZRTP header there */
		zrtp_hdr = (zrtp_msg_hdr_t*) (buffer + RTP_HDR_SIZE);
		if (zrtp_status_ok != _zrtp_packet_fill_msg_hdr(stream, type, 0, zrtp_hdr)) {
#if (defined(ZRTP_USE_STACK_MINIM) && (ZRTP_USE_STACK_MINIM == 1))
			zrtp_sys_free(buffer);
#endif
			return zrtp_status_bad_param;
		}
	}

	zrtp_hdr = (zrtp_msg_hdr_t*) (buffer + RTP_HDR_SIZE);
	packet_length += (zrtp_ntoh16(zrtp_hdr->length)*4 + 4); /* add ZRTP message header and CRC */		

	/*
     * Why do we add our own extra CRC in the ZRTP key agreement packets?   
     * If we warn the user of a man-in-the-middle attack, we must be  
     * highly confident it's a real attack, not triggered by accidental  
     * line noise, or we risk unnecessary user panic and an inappropriate  
     * security response.  Extra error detection is needed to reliably  
     * distinguish between a real attack and line noise, because unlike  
     * TCP, UDP does not have enough built-in error detection.  It only  
     * has a 16 bit checksum, and in some UDP stacks it's not always  
     * present.    
     */
	_zrtp_packet_insert_crc(buffer, packet_length);
	
	ZRTP_LOG(3,(_ZTU_, "\tSend <%.8s> ssrc=%u seq=%u size=%d. Stream %u:%s:%s\n",					
					zrtp_log_pkt2str(type),
					zrtp_ntoh32(rtp_hdr->ssrc),
					zrtp_ntoh16(rtp_hdr->seq),
					packet_length,
					stream->id,
					zrtp_log_mode2str(stream->mode),
					zrtp_log_state2str(stream->state)));
    
	s = stream->zrtp->cb.misc_cb.on_send_packet(stream, buffer, packet_length);

#if (defined(ZRTP_USE_STACK_MINIM) && (ZRTP_USE_STACK_MINIM == 1))
	zrtp_sys_free(buffer);
#endif

	return s;
}

/*----------------------------------------------------------------------------*/
zrtp_status_t _zrtp_packet_preparse( zrtp_stream_t* stream,
									 char* packet,
									 uint32_t *length,
									 zrtp_rtp_info_t* info,
									 uint8_t is_input )
{
	zrtp_status_t s = zrtp_status_fail;
	uint8_t is_correct = 1;

	do
	{
	ZRTP_UNALIGNED(zrtp_rtp_hdr_t) *rtpHdr = NULL;

	if (*length < sizeof(zrtp_rtp_hdr_t)) {
		ZRTP_LOG(1,(_ZTU_,"WARNING! Incoming packet is too small %d.ID=%u\n", *length, stream->id));
		s = zrtp_status_bad_param;
		break;
	}

	rtpHdr = (zrtp_rtp_hdr_t*) packet;
	info->type = _zrtp_packet_get_type(rtpHdr, *length);
	if (ZRTP_UNPARSED == info->type) {
		ZRTP_LOG(1,(_ZTU_,"WARNING! Can't determinate packet type. ID=%u\n", stream->id));
		s = zrtp_status_bad_param;
		break;
	}
		
	info->packet	= packet;
	info->message	= packet + RTP_HDR_SIZE;
	info->length	= length;
	info->ssrc		= rtpHdr->ssrc;
	info->seq		= _convert_seq_to_implicit_seq(stream, packet, info->type == ZRTP_NONE, is_input);

	/*
	 * Check ZRTP message correctness:
	 * - CRC
	 * - length according to type
	 * - hash (DOS attack)
	 */
	if (is_input && (info->type != ZRTP_NONE) && (info->type != ZRTP_UNPARSED))
	{
		zrtp_string32_t hash_str = ZSTR_INIT_EMPTY(hash_str);			
		zrtp_hash_t *hash = zrtp_comp_find(ZRTP_CC_HASH, ZRTP_HASH_SHA256, stream->zrtp);
		char *hash2compare = NULL, *rechash = NULL;
		zrtp_string32_t tmp_hash_str = ZSTR_INIT_EMPTY(tmp_hash_str);

		ZRTP_LOG(3,(_ZTU_, "Received <%.8s> packet with ssrc=%u seq=%u/%u size=%d. Stream%u:%s:%s.\n",					
					packet + sizeof(zrtp_rtp_hdr_t) + 4,
					zrtp_ntoh32(info->ssrc),
					zrtp_ntoh16(rtpHdr->seq),
					info->seq,
					*info->length,
					stream->id,
					zrtp_log_mode2str(stream->mode),
					zrtp_log_state2str(stream->state)));
			
	   /*
		* Why do we add our own extra CRC in the ZRTP key agreement packets?   
		* If we warn the user of a man-in-the-middle attack, we must be  
		* highly confident it's a real attack, not triggered by accidental  
		* line noise, or we risk unnecessary user panic and an inappropriate  
		* security response.  Extra error detection is needed to reliably  
		* distinguish between a real attack and line noise, because unlike  
		* TCP, UDP does not have enough built-in error detection.  It only  
		* has a 16 bit checksum, and in some UDP stacks it's not always  
		* present.
		*/
		if (_zrtp_packet_validate_crc(info->packet, *info->length) != 0) {
			ZRTP_LOG(2,(_ZTU_,"\tWARNING! Incoming ZRTP CRC validation fails. ID=%u\n", stream->id));
			s = zrtp_status_crc_fail;
			break;
		}

		/* Check length field correctness */
		if (zrtp_ntoh16(((zrtp_msg_hdr_t*)info->message)->length)*4 != (*length - 4 - RTP_HDR_SIZE))
		{
			ZRTP_LOG(2,(_ZTU_,"\tWARNING! Wrong length field for Incoming message %d packet=%d. ID=%u\n",
							zrtp_ntoh16(((zrtp_msg_hdr_t*)info->message)->length)*4,
							*length, stream->id));
			s = zrtp_status_bad_param;
			break;
		}			

		/* Check packet size according to its type */
		switch (info->type)
		{
		case ZRTP_COMMIT:
		{
			switch (stream->mode)
			{
			case ZRTP_STREAM_MODE_DH:
				is_correct = !(*length < (RTP_HDR_SIZE + ZRTP_COMMIT_STATIC_SIZE + ZRTP_HV_SIZE + ZRTP_HMAC_SIZE));
				break;
			case ZRTP_STREAM_MODE_MULT:
				is_correct = !(*length < (RTP_HDR_SIZE + ZRTP_COMMIT_STATIC_SIZE + ZRTP_HV_NONCE_SIZE + ZRTP_HMAC_SIZE));
				break;
			case ZRTP_STREAM_MODE_PRESHARED:
				is_correct = !(*length < (RTP_HDR_SIZE + ZRTP_COMMIT_STATIC_SIZE + ZRTP_HV_NONCE_SIZE + ZRTP_HV_KEY_SIZE + ZRTP_HMAC_SIZE));
				break;
			default:
				break;
			};
			break;
		}
		case ZRTP_DHPART1:
		case ZRTP_DHPART2:				
			if (stream->pubkeyscheme) {
				is_correct = (*length == (ZRTP_MIN_PACKET_LENGTH + ZRTP_DH_STATIC_SIZE + stream->pubkeyscheme->pv_length + ZRTP_HMAC_SIZE));
			}
			break;			
		case ZRTP_CONFIRM1:
		case ZRTP_CONFIRM2:
			is_correct = !(*length < (RTP_HDR_SIZE + sizeof(zrtp_packet_Confirm_t)));
			break;
		case ZRTP_SASRELAY:
			is_correct = !(*length < (RTP_HDR_SIZE + sizeof(zrtp_packet_SASRelay_t)));
			break;
		case ZRTP_GOCLEAR:
			is_correct = !(*length < (RTP_HDR_SIZE + sizeof(zrtp_packet_GoClear_t)));
			break;
		case ZRTP_ERROR:
			is_correct = !(*length < (RTP_HDR_SIZE + sizeof(zrtp_packet_Error_t)));
			break;
		case ZRTP_ZFONEPING:
		case ZRTP_ZFONEPINGACK:
			is_correct = !(*length < (RTP_HDR_SIZE + sizeof(zrtp_packet_zfoneping_t)));
			break;
		default:
			break;
		}
		/* If CRC have been verified but packet size is wrong - it looks like a stupid attack */
		if (!is_correct) {
			ZRTP_LOG(2,(_ZTU_,"\tWARNING! Incoming ZRTP message %d:%d is corrupted. ID=%u\n",
						info->type, *length, stream->id));				
			_zrtp_machine_enter_initiatingerror(stream, zrtp_error_invalid_packet, 1);
			s = zrtp_status_attack;
			break;
		}

		/*
		 * Check hash to prevent DOS attacks
		 */
		switch (info->type)
		{
		case ZRTP_HELLO:
			if (stream->messages.signaling_hash.length)
			{					
				hash->hash_c( hash,
							 (const char*) info->message,
							  zrtp_ntoh16(((zrtp_packet_Hello_t*) info->message)->hdr.length)*4,
							  ZSTR_GV(hash_str) );
				if (zrtp_memcmp(stream->messages.signaling_hash.buffer, hash_str.buffer, ZRTP_MESSAGE_HASH_SIZE)) {
					if (stream->zrtp->cb.event_cb.on_zrtp_security_event) {
						stream->zrtp->cb.event_cb.on_zrtp_security_event(stream, ZRTP_EVENT_WRONG_SIGNALING_HASH);
					}
				}
			} break;
		case ZRTP_COMMIT:								
			rechash = (char*)((zrtp_packet_Commit_t*) info->message)->hash;
			hash2compare = (char*)stream->messages.peer_hello.hash;
			break;
		case ZRTP_DHPART1:								
			hash->hash_c( hash,
						  (const char*)((zrtp_packet_DHPart_t*) info->message)->hash,
						  ZRTP_MESSAGE_HASH_SIZE,
						  ZSTR_GV(tmp_hash_str) );
			rechash = (char*)tmp_hash_str.buffer;
			hash2compare = (char*)stream->messages.peer_hello.hash;
			break;
		case ZRTP_DHPART2:
			rechash = (char*)((zrtp_packet_DHPart_t*) info->message)->hash;
			hash2compare = (char*)stream->messages.peer_commit.hash;					
			break;
		default:
			break;
		}

		if (rechash)
		{
			hash->hash_c(hash, rechash, ZRTP_MESSAGE_HASH_SIZE, ZSTR_GV(hash_str));
			is_correct = !zrtp_memcmp(hash2compare, hash_str.buffer, ZRTP_MESSAGE_HASH_SIZE);
			if (!is_correct)
			{
				ZRTP_LOG(2,(_ZTU_,"\tWARNING! ZRTP Message hashes don't mach %s! ID=%u\n",
							zrtp_log_pkt2str(info->type), stream->id));
				s = zrtp_status_attack;
				break;
			} /* hashes check */
		}


		/*
		 * Check messages HMAC
		 */
		{
		zrtp_msg_hdr_t *hdr = NULL;
		switch (info->type)
		{
		case ZRTP_COMMIT:
		case ZRTP_DHPART1:
			hdr = &stream->messages.peer_hello.hdr;
			break;
		case ZRTP_DHPART2:
			hdr = &stream->messages.peer_commit.hdr;
			break;
		default:
			break;
		}
		if (hdr)
			if (0 != _zrtp_validate_message_hmac(stream, hdr, rechash)) {
				return zrtp_status_fail;
			}
		}

// TODO: check this replay protection logic!
//		if (info->seq != stream->media_ctx.high_in_zrtp_seq) {				
//			s = zrtp_status_zrp_fail;
//			break;
//		}
	} /* for incoming ZRTP messages only only */

	s = zrtp_status_ok;
	} while(0);

	return s;	
}

/*----------------------------------------------------------------------------*/
void _zrtp_cancel_send_packet_later( zrtp_stream_t* stream,
									 zrtp_msg_type_t type)
{	
	zrtp_retry_task_t* task = NULL;	

	switch (type)
	{
	case ZRTP_HELLO:
		task = &stream->messages.hello_task;		
		break;
	case ZRTP_COMMIT:		
		task = &stream->messages.commit_task;
		break;
	case ZRTP_DHPART2:		
		task = &stream->messages.dhpart_task;
		break;
	case ZRTP_CONFIRM2:
		task = &stream->messages.confirm_task;
		break;
	case ZRTP_GOCLEAR:
		task = &stream->messages.goclear_task;		
		break;
	case ZRTP_ERROR:
		task = &stream->messages.error_task;
		break;
	case ZRTP_PROCESS:
		task = &stream->messages.dh_task;
		break;
	case ZRTP_SASRELAY:
		task = &stream->messages.sasrelay_task;
		break;

	case ZRTP_NONE:
		stream->messages.hello_task._is_enabled = 0;
		stream->messages.goclear_task._is_enabled = 0;		
		stream->messages.commit_task._is_enabled = 0;		
		stream->messages.confirm_task._is_enabled = 0;
		stream->messages.dhpart_task._is_enabled = 0;
		stream->messages.error_task._is_enabled = 0;
		stream->messages.dh_task._is_enabled = 0;
		stream->messages.sasrelay_task._is_enabled = 0;
		break;
	
	default:
		return;
	}

	if(task) {
		task->_is_enabled = 0;
	}

	if (stream->zrtp->cb.sched_cb.on_cancel_call_later) {
		stream->zrtp->cb.sched_cb.on_cancel_call_later(stream, task);
	}
}

void _zrtp_change_state( zrtp_stream_t* stream, zrtp_state_t state)
{
	stream->prev_state = stream->state;
	stream->state		 = state;
	ZRTP_LOG(3,("zrtp","\tStream ID=%u %s switching <%s> ---> <%s>.\n", 
				stream->id, zrtp_log_mode2str(stream->mode), zrtp_log_state2str(stream->prev_state), zrtp_log_state2str(stream->state)));
}
