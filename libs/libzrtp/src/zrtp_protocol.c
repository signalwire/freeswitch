/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include "zrtp.h"

#define _ZTU_ "zrtp protocol"


/*===========================================================================*/
/*	PROTOCOL Logic														     */
/*===========================================================================*/

/*----------------------------------------------------------------------------*/
static zrtp_status_t _attach_secret( zrtp_session_t *session,
									 zrtp_proto_secret_t* psec,
									 zrtp_shared_secret_t* sec,
									 uint8_t is_initiator)
{
	zrtp_uchar32_t buff;
	static const zrtp_string16_t initiator	= ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_ROLE_INITIATOR);
	static const zrtp_string16_t responder	= ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_ROLE_RESPONDER);

	const zrtp_string16_t* role				= is_initiator ? &initiator : &responder;
	const zrtp_string16_t* his_role			= is_initiator ? &responder : &initiator;

	ZSTR_SET_EMPTY(psec->id);
	ZSTR_SET_EMPTY(psec->peer_id);
	psec->secret = sec;

	/*
	 * If secret's value is available (from the cache or from SIP) - use hmac;
	 * use zero-strings in other case.
	 */
	if (psec->secret) {
		session->hash->hmac_truncated( session->hash,
									   ZSTR_GV(sec->value),
									   ZSTR_GVP(role),
									   ZRTP_RSID_SIZE,
									   ZSTR_GV(psec->id));

		session->hash->hmac_truncated( session->hash,
									   ZSTR_GV(sec->value),
									   ZSTR_GVP(his_role),
									   ZRTP_RSID_SIZE,
									   ZSTR_GV(psec->peer_id));
	} else {
		psec->id.length = ZRTP_RSID_SIZE;
		zrtp_memset(psec->id.buffer, 0, psec->id.length);

		psec->peer_id.length = ZRTP_RSID_SIZE;
		zrtp_memset(psec->peer_id.buffer, 0, psec->peer_id.length);
	}

	ZRTP_LOG(3,(_ZTU_,"\tAttach RS id=%s.\n",
				hex2str((const char*)psec->id.buffer, psec->id.length, (char*)buff, sizeof(buff))));
	ZRTP_LOG(3,(_ZTU_,"\tAttach RS peer_id=%s.\n",
				hex2str((const char*)psec->peer_id.buffer, psec->peer_id.length, (char*)buff, sizeof(buff))));

	return zrtp_status_ok;
}

zrtp_status_t _zrtp_protocol_init(zrtp_stream_t *stream, uint8_t is_initiator, zrtp_protocol_t **protocol)
{
	zrtp_protocol_t	*new_proto = NULL;
	zrtp_status_t s = zrtp_status_ok;

	ZRTP_LOG(3,(_ZTU_,"\tInit %s Protocol ID=%u mode=%s...\n",
				is_initiator ? "INITIATOR's" : "RESPONDER's", stream->id, zrtp_log_mode2str(stream->mode)));

	/* Destroy previous protocol structure (Responder or Preshared) */
    if (*protocol) {
		_zrtp_protocol_destroy(*protocol);
		*protocol = NULL;
    }

	/* Allocate memory for all branching structures */
	do
	{
		new_proto = zrtp_sys_alloc(sizeof(zrtp_protocol_t));
		if (!new_proto) {
			s = zrtp_status_alloc_fail;
			break;
		}
		zrtp_memset(new_proto, 0, sizeof(zrtp_protocol_t));

		new_proto->cc = zrtp_sys_alloc(sizeof(zrtp_proto_crypto_t));
		if (!new_proto->cc) {
			s = zrtp_status_alloc_fail;
			break;
		}
		zrtp_memset(new_proto->cc, 0, sizeof(zrtp_proto_crypto_t));

		/* Create and Initialize DH crypto context	(for DH streams only) */
		if (ZRTP_IS_STREAM_DH(stream)) {
			if (stream->dh_cc.initialized_with != stream->pubkeyscheme->base.id) {				
				stream->pubkeyscheme->initialize(stream->pubkeyscheme, &stream->dh_cc);
				stream->dh_cc.initialized_with = stream->pubkeyscheme->base.id;
			}
		}

		/* Initialize main structure at first: functions pointers and generate nonce */
		new_proto->type		= is_initiator ? ZRTP_STATEMACHINE_INITIATOR : ZRTP_STATEMACHINE_RESPONDER;
		new_proto->context = stream;

		/* Initialize protocol crypto context and prepare it for further usage */
		ZSTR_SET_EMPTY(new_proto->cc->kdf_context);
		ZSTR_SET_EMPTY(new_proto->cc->s0);
		ZSTR_SET_EMPTY(new_proto->cc->mes_hash);
		ZSTR_SET_EMPTY(new_proto->cc->hv);
		ZSTR_SET_EMPTY(new_proto->cc->peer_hv);

		if (ZRTP_IS_STREAM_DH(stream)) {
			_attach_secret(stream->session, &new_proto->cc->rs1, stream->session->secrets.rs1, is_initiator);
			_attach_secret(stream->session, &new_proto->cc->rs2, stream->session->secrets.rs2, is_initiator);		
			_attach_secret(stream->session, &new_proto->cc->auxs, stream->session->secrets.auxs, is_initiator);
			_attach_secret(stream->session, &new_proto->cc->pbxs, stream->session->secrets.pbxs, is_initiator);
		}
		
		s = zrtp_status_ok;
		*protocol = new_proto;
	} while (0);

	if (s != zrtp_status_ok) {
		ZRTP_LOG(1,(_ZTU_,"\tERROR! _zrtp_protocol_attach() with code %s.\n", zrtp_log_status2str(s)));
		if (new_proto && new_proto->cc) {
			zrtp_sys_free(new_proto->cc);
		}
		if (new_proto) {
			zrtp_sys_free(new_proto);
		}
		*protocol = NULL;
	}

    return s;
}

/*----------------------------------------------------------------------------*/
static void clear_crypto_sources(zrtp_stream_t* stream)
{
	zrtp_protocol_t* proto = stream->protocol;
	if (proto && proto->cc) {
		zrtp_memset(proto->cc, 0, sizeof(zrtp_proto_crypto_t));
		zrtp_sys_free(proto->cc);
		proto->cc = 0;
	}
}

void _zrtp_protocol_destroy(zrtp_protocol_t *proto)
{
	/* Clear protocol crypto values, destroy SRTP unit, clear and release memory. */
	if (proto) {
		/* if protocol is being destroyed by exception, ->context may be NULL */
		if (proto->context) {
			_zrtp_cancel_send_packet_later(proto->context, ZRTP_NONE);
			if (proto->_srtp) {
				zrtp_srtp_destroy(proto->context->zrtp->srtp_global, proto->_srtp);
			}
		}

		clear_crypto_sources(proto->context);
		zrtp_memset(proto, 0, sizeof(zrtp_protocol_t));
		zrtp_sys_free(proto);
	}
}

/*----------------------------------------------------------------------------*/
zrtp_status_t _zrtp_protocol_encrypt( zrtp_protocol_t *proto,
									  zrtp_rtp_info_t *packet,
									  uint8_t is_rtp)
{
	zrtp_status_t s = zrtp_status_ok;

	if (is_rtp) {
		s = zrtp_srtp_protect(proto->context->zrtp->srtp_global, proto->_srtp, packet);
	} else {
		s = zrtp_srtp_protect_rtcp(proto->context->zrtp->srtp_global, proto->_srtp, packet);
	}

	if (zrtp_status_ok != s) {
		ZRTP_UNALIGNED(zrtp_rtp_hdr_t) *hdr = (zrtp_rtp_hdr_t*) packet->packet;

		ZRTP_LOG(2,(_ZTU_,"ERROR! Encrypt failed. ID=%u:%s s=%s (%s size=%d ssrc=%u seq=%d pt=%d)\n",
					    proto->context->id,
						zrtp_log_mode2str(proto->context->mode),
						zrtp_log_status2str(s),
						is_rtp ? "RTP" : "RTCP",
						*packet->length,
						zrtp_ntoh32(hdr->ssrc),
						zrtp_ntoh16(hdr->seq),
						hdr->pt));
    }

	return s;
}

/*----------------------------------------------------------------------------*/
zrtp_status_t _zrtp_protocol_decrypt( zrtp_protocol_t *proto,
									  zrtp_rtp_info_t *packet,
									  uint8_t is_rtp)
{
	zrtp_status_t s = zrtp_status_ok;

	if (is_rtp) {
		s = zrtp_srtp_unprotect(proto->context->zrtp->srtp_global, proto->_srtp, packet);
	} else {
		s = zrtp_srtp_unprotect_rtcp(proto->context->zrtp->srtp_global, proto->_srtp, packet);
	}

	if (zrtp_status_ok != s) {
		ZRTP_UNALIGNED(zrtp_rtp_hdr_t) *hdr = (zrtp_rtp_hdr_t*) packet->packet;
		ZRTP_LOG(2,(_ZTU_,"ERROR! Decrypt failed. ID=%u:%s s=%s (%s size=%d ssrc=%u seq=%u/%u pt=%d)\n",
					    proto->context->id,
						zrtp_log_mode2str(proto->context->mode),
						zrtp_log_status2str(s),
						is_rtp ? "RTP" : "RTCP",
						*packet->length,
						zrtp_ntoh32(hdr->ssrc),
						zrtp_ntoh16(hdr->seq),
						packet->seq,
						hdr->pt));
    }

	return s;
}


/*===========================================================================*/
/*	CRYPTO Utilites														     */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
static zrtp_status_t _derive_s0(zrtp_stream_t* stream, int is_initiator)
{
	static const zrtp_string32_t zrtp_kdf_label	= ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_KDF_STR);
	static const zrtp_string32_t zrtp_sess_label = ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_SESS_STR);
	static const zrtp_string32_t zrtp_multi_label = ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_MULTI_STR);
	static const zrtp_string32_t zrtp_presh_label = ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_PRESH_STR);
	
	zrtp_session_t *session = stream->session;
	zrtp_secrets_t* secrets  = &session->secrets;
	zrtp_proto_crypto_t* cc  = stream->protocol->cc;
	void* hash_ctx = NULL;
	char print_buff[256];

	switch (stream->mode)
	{
	/*
	 * S0 computing for FULL DH exchange	 
	 * S0 computing.  s0 is the master shared secret used for all
	 * cryptographic operations.  In particular, note the inclusion
	 * of "total_hash", a hash of all packets exchanged up to this
	 * point.  This belatedly detects any tampering with earlier
	 * packets, e.g. bid-down attacks.
	 *
	 * s0 = hash( 1 | DHResult | "ZRTP-HMAC-KDF" | ZIDi | ZIDr |
	 *                        total_hash | len(s1) | s1 | len(s2) | s2 | len(s3) | s3 )
	 * The constant 1 and all lengths are 32 bits big-endian values.
	 * The fields without length prefixes are fixed-witdh:
	 * - DHresult is fixed to the width of the DH prime.
	 * - The hash type string and ZIDs are fixed width.
	 * - total_hash is fixed by the hash negotiation.
	 * The constant 1 is per NIST SP 800-56A section 5.8.1, and is
	 * a counter which can be incremented to generate more than 256
	 * bits of key material.
	 * ========================================================================
	 */
	case ZRTP_STREAM_MODE_DH:
	{
		zrtp_proto_secret_t *C[3] = { 0, 0, 0};
		int i = 0;
		uint32_t comp_length = 0;
		zrtp_stringn_t *zidi = NULL, *zidr = NULL;
		struct BigNum dhresult;
#if (defined(ZRTP_USE_STACK_MINIM) && (ZRTP_USE_STACK_MINIM == 1))
		zrtp_uchar1024_t* buffer = zrtp_sys_alloc( sizeof(zrtp_uchar1024_t) );
		if (!buffer) {
			return zrtp_status_alloc_fail;
		}
#else
		zrtp_uchar1024_t holder;
		zrtp_uchar1024_t* buffer = &holder;
#endif

		ZRTP_LOG(3,(_ZTU_,"\tDERIVE S0 from DH exchange and RS secrets...\n"));
		ZRTP_LOG(3,(_ZTU_,"\t       my rs1ID:%s\n", hex2str(cc->rs1.id.buffer, cc->rs1.id.length, print_buff, sizeof(print_buff))));
		ZRTP_LOG(3,(_ZTU_,"\t      his rs1ID:%s\n", hex2str((const char*)stream->messages.peer_dhpart.rs1ID, ZRTP_RSID_SIZE, print_buff, sizeof(print_buff))));
		ZRTP_LOG(3,(_ZTU_,"\t his rs1ID comp:%s\n", hex2str(cc->rs1.peer_id.buffer, cc->rs1.peer_id.length, print_buff, sizeof(print_buff))));

		ZRTP_LOG(3,(_ZTU_,"\t       my rs2ID:%s\n", hex2str(cc->rs2.id.buffer, cc->rs2.id.length, print_buff, sizeof(print_buff))));
		ZRTP_LOG(3,(_ZTU_,"\t      his rs2ID:%s\n", hex2str((const char*)stream->messages.peer_dhpart.rs2ID, ZRTP_RSID_SIZE, print_buff, sizeof(print_buff))));
		ZRTP_LOG(3,(_ZTU_,"\t his rs2ID comp:%s\n", hex2str(cc->rs2.peer_id.buffer, cc->rs2.peer_id.length, print_buff, sizeof(print_buff))));

		ZRTP_LOG(3,(_ZTU_,"\t      my pbxsID:%s\n", hex2str(cc->pbxs.id.buffer, cc->pbxs.id.length, print_buff, sizeof(print_buff))));
		ZRTP_LOG(3,(_ZTU_,"\t     his pbxsID:%s\n", hex2str((const char*)stream->messages.peer_dhpart.pbxsID, ZRTP_RSID_SIZE, print_buff, sizeof(print_buff))));
		ZRTP_LOG(3,(_ZTU_,"\this pbxsID comp:%s\n", hex2str(cc->pbxs.peer_id.buffer, cc->pbxs.peer_id.length, print_buff, sizeof(print_buff))));

		hash_ctx = session->hash->hash_begin(session->hash);
		if (0 == hash_ctx) {
			ZRTP_LOG(1,(_ZTU_, "\tERROR! can't start hash calculation for S0 computing. ID=%u.\n", stream->id));
			return zrtp_status_fail;
		}

		/*
		 * NIST requires a 32-bit big-endian integer counter to be included
		 * in the hash each time the hash is computed, which we have set to
		 * the fixed value of 1, because we only compute the hash once.
		 */
		comp_length = zrtp_hton32(1L);
		session->hash->hash_update(session->hash, hash_ctx, (const int8_t*)&comp_length, 4);

		
		switch (stream->pubkeyscheme->base.id) {
			case ZRTP_PKTYPE_DH2048:
			case ZRTP_PKTYPE_DH3072:
			case ZRTP_PKTYPE_DH4096:
				comp_length = stream->pubkeyscheme->pv_length;
				ZRTP_LOG(3,(_ZTU_,"DH comp_length=%u\n", comp_length));
				break;
			case ZRTP_PKTYPE_EC256P:
			case ZRTP_PKTYPE_EC384P:
			case ZRTP_PKTYPE_EC521P:
				comp_length = stream->pubkeyscheme->pv_length/2;
				ZRTP_LOG(3,(_ZTU_,"ECDH comp_length=%u\n", comp_length));
				break;
			default:
				break;
		}
		
		bnBegin(&dhresult);
		stream->pubkeyscheme->compute(stream->pubkeyscheme,
									  &stream->dh_cc,
									  &dhresult,
									  &stream->dh_cc.peer_pv);
				
		bnExtractBigBytes(&dhresult, (uint8_t *)buffer, 0, comp_length);
		session->hash->hash_update(session->hash, hash_ctx, (const int8_t*)buffer, comp_length);
		bnEnd(&dhresult);

#if (defined(ZRTP_USE_STACK_MINIM) && (ZRTP_USE_STACK_MINIM == 1))
		zrtp_sys_free(buffer);
#endif
		
		/* Add "ZRTP-HMAC-KDF" to the S0 hash */		
		session->hash->hash_update( session->hash, hash_ctx,
									(const int8_t*)&zrtp_kdf_label.buffer,
									zrtp_kdf_label.length);

		/* Then Initiator's and Responder's ZIDs */
		if (stream->protocol->type == ZRTP_STATEMACHINE_INITIATOR) {
			zidi = ZSTR_GV(stream->session->zid);
			zidr = ZSTR_GV(stream->session->peer_zid);
		} else {
			zidr = ZSTR_GV(stream->session->zid);
			zidi = ZSTR_GV(stream->session->peer_zid);
		}
		
		session->hash->hash_update(session->hash, hash_ctx, (const int8_t*)&zidi->buffer, zidi->length);
		session->hash->hash_update(session->hash, hash_ctx, (const int8_t*)&zidr->buffer, zidr->length);
		session->hash->hash_update(session->hash, hash_ctx, (const int8_t*)&cc->mes_hash.buffer, cc->mes_hash.length);

		/* If everything is OK - RS1 should much */
		if (!zrtp_memcmp(cc->rs1.peer_id.buffer, stream->messages.peer_dhpart.rs1ID, ZRTP_RSID_SIZE))
		{
			C[0] = &cc->rs1;
			secrets->matches |= ZRTP_BIT_RS1;
		}
		/* If we have lost our RS1 - remote party should use backup (RS2) instead */
		else if (!zrtp_memcmp(cc->rs1.peer_id.buffer, stream->messages.peer_dhpart.rs2ID, ZRTP_RSID_SIZE))
		{
			C[0] = &cc->rs1;
			secrets->matches |= ZRTP_BIT_RS1;
			ZRTP_LOG(2,(_ZTU_,"\tINFO! We have lost our RS1 from previous broken exchange"
						" - remote party will use RS2 backup. ID=%u\n", stream->id));
		}
		/* If remote party lost it's secret - we will use backup */
		else if (!zrtp_memcmp(cc->rs2.peer_id.buffer, stream->messages.peer_dhpart.rs1ID, ZRTP_RSID_SIZE))
		{
			C[0] = &cc->rs2;
			cc->rs1 = cc->rs2;
			secrets->matches |= ZRTP_BIT_RS1;
			secrets->cached  |= ZRTP_BIT_RS1;
			ZRTP_LOG(2,(_ZTU_,"\tINFO! Remote party has lost it's RS1 - use RS2 backup. ID=%u\n", stream->id));
		}
		else
		{			
			secrets->matches &= ~ZRTP_BIT_RS1;
			if (session->zrtp->cb.cache_cb.on_set_verified) {
				session->zrtp->cb.cache_cb.on_set_verified( ZSTR_GV(session->zid),
															ZSTR_GV(session->peer_zid),
															0);
			}
			
			if (session->zrtp->cb.cache_cb.on_reset_since) {
				session->zrtp->cb.cache_cb.on_reset_since(ZSTR_GV(session->zid), ZSTR_GV(session->peer_zid));
			}

			ZRTP_LOG(2,(_ZTU_,"\tINFO! Our RS1 doesn't equal to other-side's one %s. ID=%u\n",
						cc->rs1.secret->_cachedflag ? " - drop verified!" : "", stream->id));
		}

		if (!zrtp_memcmp(cc->rs2.peer_id.buffer, stream->messages.peer_dhpart.rs2ID, ZRTP_RSID_SIZE)) {
			secrets->matches |= ZRTP_BIT_RS2;
			if (0 == C[0]) {
				C[0] = &cc->rs2;
			}
		}
		

		if (secrets->auxs &&
			(!zrtp_memcmp(stream->messages.peer_dhpart.auxsID, cc->auxs.peer_id.buffer, ZRTP_RSID_SIZE)) ) {
			C[1] =&cc->auxs;
	    	secrets->matches |= ZRTP_BIT_AUX;
		}

		if ( secrets->pbxs &&
			(!zrtp_memcmp(stream->messages.peer_dhpart.pbxsID, cc->pbxs.peer_id.buffer, ZRTP_RSID_SIZE)) ) {	
			C[2] = &cc->pbxs;
			secrets->matches |= ZRTP_BIT_PBX;
		}

		/* Finally hashing matched shared secrets */
		for (i=0; i<3; i++) {
			/*
			 * Some of the shared secrets s1 through s5 may have lengths of zero
			 * if they are null (not shared), and are each preceded by a 4-octet
			 * length field. For example, if s4 is null, len(s4) is 00 00 00 00,
			 * and s4 itself would be absent from the hash calculation, which
			 * means len(s5) would immediately follow len(s4).
			 */
			comp_length = C[i] ? zrtp_hton32(ZRTP_RS_SIZE) : 0;
			session->hash->hash_update(session->hash, hash_ctx, (const int8_t*)&comp_length, 4);
			if (C[i]) {
				session->hash->hash_update( session->hash,
											 hash_ctx,
											 (const int8_t*)C[i]->secret->value.buffer,
											 C[i]->secret->value.length );
				ZRTP_LOG(3,(_ZTU_,"\tUse S%d in calculations.\n", i+1));
			}
		}

		session->hash->hash_end(session->hash, hash_ctx, ZSTR_GV(cc->s0));
	} break; /* S0 for for DH and Preshared streams */

	/*
	 * Compute all possible combinations of preshared_key:
	 * hash(len(rs1) | rs1 | len(auxsecret) | auxsecret | len(pbxsecret) | pbxsecret)	 
	 * Find matched preshared_key and derive S0 from it:
	 * s0 = KDF(preshared_key, "ZRTP Stream Key", KDF_Context, negotiated hash length) 
	 *
	 * INFO: Take into account that RS1 and RS2 may be swapped.
	 * If no matched were found - generate DH commit.
	 * ========================================================================
	 */
	case ZRTP_STREAM_MODE_PRESHARED:
	{
		zrtp_status_t s				= zrtp_status_ok;
		zrtp_string32_t presh_key	= ZSTR_INIT_EMPTY(presh_key);		

		ZRTP_LOG(3,(_ZTU_,"\tDERIVE S0 for PRESHARED from cached secret. ID=%u\n", stream->id));

		/* Use the same hash as we used for Commitment */
		if (is_initiator)
		{
			s = _zrtp_compute_preshared_key( session,											 
											 ZSTR_GV(session->secrets.rs1->value),
											 (session->secrets.auxs->_cachedflag) ? ZSTR_GV(session->secrets.auxs->value) : NULL,
											 (session->secrets.pbxs->_cachedflag) ? ZSTR_GV(session->secrets.pbxs->value) : NULL,
											 ZSTR_GV(presh_key),
											 NULL);
			if (zrtp_status_ok != s) {
				return s;
			}
			
			secrets->matches |= ZRTP_BIT_RS1;
			if (session->secrets.auxs->_cachedflag) {				
				secrets->matches |= ZRTP_BIT_AUX;
			}
			if (session->secrets.pbxs->_cachedflag) {			
				secrets->matches |= ZRTP_BIT_PBX;
			}
		}
		/*
		 * Let's find appropriate hv key for Responder:
		 * <RS1, 0, 0>, <RS1, AUX, 0>, <RS1, 0, PBX>, <RS1, AUX, PBX>.
		 */
		else
		{
			int res=-1;
			char* peer_key_id		= (char*)stream->messages.peer_commit.hv+ZRTP_HV_NONCE_SIZE;
			zrtp_string8_t key_id	= ZSTR_INIT_EMPTY(key_id);
			
			do {
				/* RS1 MUST be available at this stage.*/
				s = _zrtp_compute_preshared_key( session,							 
												 ZSTR_GV(secrets->rs1->value),
												 NULL,
												 NULL,
												 ZSTR_GV(presh_key),
												 ZSTR_GV(key_id));
				if (zrtp_status_ok == s) {
					res = zrtp_memcmp(peer_key_id, key_id.buffer, ZRTP_HV_KEY_SIZE);
					if (0 == res) {
						secrets->matches |= ZRTP_BIT_RS1;
						break;
					}
				}				
				
				if (session->secrets.pbxs->_cachedflag)
				{
					s = _zrtp_compute_preshared_key( session,											 
													 ZSTR_GV(secrets->rs1->value),
													 NULL,
													 ZSTR_GV(secrets->pbxs->value),
													 ZSTR_GV(presh_key),
													 ZSTR_GV(key_id));
					if (zrtp_status_ok == s) {
						res = zrtp_memcmp(peer_key_id, key_id.buffer, ZRTP_HV_KEY_SIZE);
						if (0 == res) {
							secrets->matches |= ZRTP_BIT_PBX;
							break;
						}
					}
				}
				
				if (session->secrets.auxs->_cachedflag)
				{
					s = _zrtp_compute_preshared_key( session,													 
													 ZSTR_GV(secrets->rs1->value),
													 ZSTR_GV(secrets->auxs->value),
													 NULL,
													 ZSTR_GV(presh_key),
													 ZSTR_GV(key_id));
					if (zrtp_status_ok == s) {
						res = zrtp_memcmp(peer_key_id, key_id.buffer, ZRTP_HV_KEY_SIZE);
						if (0 == res) {
							secrets->matches |= ZRTP_BIT_AUX;
							break;
						}
					}
				}
				
				if ((session->secrets.pbxs->_cachedflag) && (session->secrets.auxs->_cachedflag))
				{
					s = _zrtp_compute_preshared_key( session,													 
													 ZSTR_GV(secrets->rs1->value),
													 ZSTR_GV(secrets->auxs->value),
													 ZSTR_GV(secrets->pbxs->value),
													 ZSTR_GV(presh_key),
													 ZSTR_GV(key_id));
					if (zrtp_status_ok == s) {
						res = zrtp_memcmp(peer_key_id, key_id.buffer, ZRTP_HV_KEY_SIZE);
						if (0 == res) {
							secrets->matches |= ZRTP_BIT_AUX;
							secrets->matches |= ZRTP_BIT_PBX;
							break;
						}
					}
				}
				
			} while (0);
			
			if (0 != res) {
				ZRTP_LOG(3,(_ZTU_,"\tINFO! Matched Key wasn't found - initate DH exchange.\n"));
				secrets->cached = 0;
				secrets->rs1->_cachedflag = 0;
				
				_zrtp_machine_start_initiating_secure(stream);
				return zrtp_status_ok;				
			}
		}
		
		ZRTP_LOG(3,(_ZTU_,"\tUse RS1, %s, %s in calculations.\n", 
					   (session->secrets.matches & ZRTP_BIT_AUX) ? "AUX" : "NULL",
					   (session->secrets.matches & ZRTP_BIT_PBX) ? "PBX" : "NULL"));		
		
		_zrtp_kdf( stream,
				   ZSTR_GV(presh_key),
				   ZSTR_GV(zrtp_presh_label),
				   ZSTR_GV(stream->protocol->cc->kdf_context),
				   session->hash->digest_length,
				   ZSTR_GV(cc->s0));
	} break;

		
	/*
	 * For FAST Multistream:
	 * s0n = KDF(ZRTPSess, "ZRTP Multistream Key", KDF_Context, negotiated hash length) 
	 * ========================================================================
	 */
	case ZRTP_STREAM_MODE_MULT:
	{
		ZRTP_LOG(3,(_ZTU_,"\tDERIVE S0 for MULTISTREAM from ZRTP Session key... ID=%u\n", stream->id));
		_zrtp_kdf( stream,
				   ZSTR_GV(session->zrtpsess),
				   ZSTR_GV(zrtp_multi_label),
				   ZSTR_GV(stream->protocol->cc->kdf_context),
				   session->hash->digest_length,
				   ZSTR_GV(cc->s0));
	} break;
		
	default: break;
	}
	
	
	/*
	 * Compute ZRTP session key for FULL streams only:
	 * ZRTPSess = KDF(s0, "ZRTP Session Key", KDF_Context, negotiated hash length)
	 */
	if (!ZRTP_IS_STREAM_MULT(stream)) {
		if (session->zrtpsess.length == 0) {
			_zrtp_kdf( stream,
					   ZSTR_GV(cc->s0),
					   ZSTR_GV(zrtp_sess_label),
					   ZSTR_GV(stream->protocol->cc->kdf_context),
					   session->hash->digest_length,
					   ZSTR_GV(session->zrtpsess));
		}
	}
	
	return zrtp_status_ok;
}


/*----------------------------------------------------------------------------*/
zrtp_status_t _zrtp_set_public_value( zrtp_stream_t *stream,
									  int is_initiator)
{
	/*
	 * This function performs the following actions according to ZRTP draft 5.6
	 * a) Computes total hash;
	 * b) Calculates DHResult;
	 * c) Computes final stream key S0, based on DHSS and retained secrets;
	 * d) Computes HMAC Key and ZRTP key;
	 * e) Computes srtp keys and salts and creates srtp session.
	 */

	zrtp_session_t *session = stream->session;
	zrtp_proto_crypto_t* cc = stream->protocol->cc;
	void* hash_ctx = NULL;

	static const zrtp_string32_t hmac_keyi_label = ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_INITIATOR_HMAKKEY_STR);
	static const zrtp_string32_t hmac_keyr_label = ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_RESPONDER_HMAKKEY_STR);

    static const zrtp_string32_t srtp_mki_label	= ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_INITIATOR_KEY_STR);
    static const zrtp_string32_t srtp_msi_label	= ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_INITIATOR_SALT_STR);
    static const zrtp_string32_t srtp_mkr_label	= ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_RESPONDER_KEY_STR);
    static const zrtp_string32_t srtp_msr_label	= ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_RESPONDER_SALT_STR);

	static const zrtp_string32_t zrtp_keyi_label = ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_INITIATOR_ZRTPKEY_STR);
	static const zrtp_string32_t zrtp_keyr_label = ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_RESPONDER_ZRTPKEY_STR);

	uint32_t cipher_key_length = (ZRTP_CIPHER_AES128 == session->blockcipher->base.id) ? 16 : 32;

	const zrtp_string32_t *output_mk_label;
    const zrtp_string32_t *output_ms_label;
    const zrtp_string32_t *input_mk_label;
    const zrtp_string32_t *input_ms_label;
	const zrtp_string32_t *hmac_key_label;
	const zrtp_string32_t *peer_hmac_key_label;
	const zrtp_string32_t *zrtp_key_label;
	const zrtp_string32_t *peer_zrtp_key_label;

    /* Define roles and prepare structures */
    if (is_initiator) {
		output_mk_label		= &srtp_mki_label;
		output_ms_label		= &srtp_msi_label;
		input_mk_label		= &srtp_mkr_label;
		input_ms_label		= &srtp_msr_label;
		hmac_key_label		= &hmac_keyi_label;
		peer_hmac_key_label	= &hmac_keyr_label;
		zrtp_key_label		= &zrtp_keyi_label;
		peer_zrtp_key_label	= &zrtp_keyr_label;
    } else {
		output_mk_label		= &srtp_mkr_label;
		output_ms_label		= &srtp_msr_label;
		input_mk_label		= &srtp_mki_label;
		input_ms_label		= &srtp_msi_label;
		hmac_key_label		= &hmac_keyr_label;
		peer_hmac_key_label	= &hmac_keyi_label;
		zrtp_key_label		= &zrtp_keyr_label;
		peer_zrtp_key_label	= &zrtp_keyi_label;
    }

	ZRTP_LOG(3, (_ZTU_,"---------------------------------------------------\n"));
	ZRTP_LOG(3,(_ZTU_,"\tSWITCHING TO SRTP. ID=%u\n", zrtp_log_mode2str(stream->mode), stream->id));
	ZRTP_LOG(3,(_ZTU_,"\tI %s\n", is_initiator ? "Initiator" : "Responder"));
	
	/*
	 * Compute total messages hash:
	 * total_hash = hash(Hello of responder | Commit | DHPart1 | DHPart2) for DH streams
	 * total_hash = hash(Hello of responder | Commit ) for Fast modes.
	 */
	{
		uint8_t* tok	 = NULL;
		uint16_t tok_len = 0;

		hash_ctx = session->hash->hash_begin(session->hash);
		if (0 == hash_ctx) {			
			return zrtp_status_fail;
		}

		tok		= is_initiator ? (uint8_t*)&stream->messages.peer_hello : (uint8_t*) &stream->messages.hello;
		tok_len = is_initiator ? stream->messages.peer_hello.hdr.length : stream->messages.hello.hdr.length;
		tok_len = zrtp_ntoh16(tok_len)*4;
		session->hash->hash_update(session->hash, hash_ctx, (const int8_t*)tok, tok_len);

		tok		= is_initiator ? (uint8_t*)&stream->messages.commit : (uint8_t*)&stream->messages.peer_commit;
		tok_len	= is_initiator ? stream->messages.commit.hdr.length : stream->messages.peer_commit.hdr.length;
		tok_len = zrtp_ntoh16(tok_len)*4;
		session->hash->hash_update(session->hash, hash_ctx, (const int8_t*)tok, tok_len);		

		if (ZRTP_IS_STREAM_DH(stream))
		{
			tok = (uint8_t*) (is_initiator ? &stream->messages.peer_dhpart : &stream->messages.dhpart);
			tok_len	= is_initiator ? stream->messages.peer_dhpart.hdr.length : stream->messages.dhpart.hdr.length;
			tok_len = zrtp_ntoh16(tok_len)*4;
			session->hash->hash_update(session->hash, hash_ctx, (const int8_t*)tok, tok_len);

			tok = (uint8_t*)(is_initiator ? &stream->messages.dhpart :  &stream->messages.peer_dhpart);
			tok_len	= is_initiator ? stream->messages.dhpart.hdr.length : stream->messages.peer_dhpart.hdr.length;
			tok_len = zrtp_ntoh16(tok_len)*4;
			session->hash->hash_update(session->hash, hash_ctx, (const int8_t*)tok, tok_len);
		}

		session->hash->hash_end(session->hash, hash_ctx, ZSTR_GV(cc->mes_hash));
		hash_ctx = NULL;
	} /* total hash computing */
	
	/* Total Hash is ready and we can create KDF_Context */
	zrtp_zstrcat(ZSTR_GV(cc->kdf_context), is_initiator ? ZSTR_GV(session->zid) : ZSTR_GV(session->peer_zid));
	zrtp_zstrcat(ZSTR_GV(cc->kdf_context), is_initiator ? ZSTR_GV(session->peer_zid) : ZSTR_GV(session->zid));
	zrtp_zstrcat(ZSTR_GV(cc->kdf_context), ZSTR_GV(cc->mes_hash));

	/* Derive stream key S0 according to key exchange scheme */
	if (zrtp_status_ok != _derive_s0(stream, is_initiator)) {
		return zrtp_status_fail;
	}

    /*
	 * Compute HMAC keys. These values will be used after confirmation:
	 * hmackeyi = KDF(s0, "Initiator HMAC key", KDF_Context, negotiated hash length)
	 * hmackeyr = KDF(s0, "Responder HMAC key", KDF_Context, negotiated hash length)
	 */
	_zrtp_kdf( stream,
			   ZSTR_GV(cc->s0),
			   ZSTR_GVP(hmac_key_label),
			   ZSTR_GV(stream->protocol->cc->kdf_context),
			   session->hash->digest_length,
			   ZSTR_GV(stream->cc.hmackey));
	_zrtp_kdf( stream,
			   ZSTR_GV(cc->s0),
			   ZSTR_GVP(peer_hmac_key_label),
			   ZSTR_GV(stream->protocol->cc->kdf_context),
			   session->hash->digest_length,
			   ZSTR_GV(stream->cc.peer_hmackey));
	
	/*
	 * Computing ZRTP keys for protection of the Confirm packet:
	 * zrtpkeyi = KDF(s0, "Initiator ZRTP key", KDF_Context, negotiated AES key length)	 
	 * zrtpkeyr = KDF(s0, "Responder ZRTP key", KDF_Context, negotiated AES key length)
	 */
	_zrtp_kdf( stream,
			   ZSTR_GV(cc->s0),
			   ZSTR_GVP(zrtp_key_label),
			   ZSTR_GV(stream->protocol->cc->kdf_context),
			   cipher_key_length,
			   ZSTR_GV(stream->cc.zrtp_key));
	_zrtp_kdf( stream,
			   ZSTR_GV(cc->s0),
			   ZSTR_GVP(peer_zrtp_key_label),
			   ZSTR_GV(stream->protocol->cc->kdf_context),
			   cipher_key_length,
			   ZSTR_GV(stream->cc.peer_zrtp_key));
#if (defined(ZRTP_DEBUG_ZRTP_KEYS) && ZRTP_DEBUG_ZRTP_KEYS == 1)
	{
	char print_buff[256];
	ZRTP_LOG(3,(_ZTU_,"\t  Messages hash:%s\n", hex2str(cc->mes_hash.buffer, cc->mes_hash.length, print_buff, sizeof(print_buff))));
    ZRTP_LOG(3,(_ZTU_,"\t             S0:%s\n", hex2str(cc->s0.buffer, cc->s0.length, print_buff, sizeof(print_buff))));
	ZRTP_LOG(3,(_ZTU_,"\t      ZRTP Sess:%s\n", hex2str(session->zrtpsess.buffer, session->zrtpsess.length, print_buff, sizeof(print_buff))));
	ZRTP_LOG(3,(_ZTU_,"\t        hmackey:%s\n", hex2str(stream->cc.hmackey.buffer, stream->cc.hmackey.length, print_buff, sizeof(print_buff))));
	ZRTP_LOG(3,(_ZTU_,"\t  peer_hmackeyr:%s\n", hex2str(stream->cc.peer_hmackey.buffer, stream->cc.peer_hmackey.length, print_buff, sizeof(print_buff))));
	ZRTP_LOG(3,(_ZTU_,"\t       ZRTP key:%s\n", hex2str(stream->cc.zrtp_key.buffer, stream->cc.zrtp_key.length, print_buff, sizeof(print_buff))));
	ZRTP_LOG(3,(_ZTU_,"\t  Peer ZRTP key:%s\n", hex2str(stream->cc.peer_zrtp_key.buffer, stream->cc.peer_zrtp_key.length, print_buff, sizeof(print_buff))));
	}
#endif
	/*
	 * Preparing SRTP crypto engine:
	 * srtpkeyi = KDF(s0, "Initiator SRTP master key", KDF_Context, negotiated AES key length)	 
	 * srtpsalti = KDF(s0, "Initiator SRTP master salt", KDF_Context, 112)
	 * srtpkeyr = KDF(s0, "Responder SRTP master key", KDF_Context, negotiated AES key length)	 
	 * srtpsaltr = KDF(s0, "Responder SRTP master salt", KDF_Context, 112)	 
	 */
	{
		zrtp_srtp_profile_t iprof;
		zrtp_srtp_profile_t oprof;

		ZSTR_SET_EMPTY(iprof.salt);
		ZSTR_SET_EMPTY(iprof.key);

		iprof.rtp_policy.cipher			= session->blockcipher;
		iprof.rtp_policy.auth_tag_len	= session->authtaglength;
		iprof.rtp_policy.hash			= zrtp_comp_find(ZRTP_CC_HASH, ZRTP_SRTP_HASH_HMAC_SHA1, session->zrtp);
		iprof.rtp_policy.auth_key_len	= 20;
		iprof.rtp_policy.cipher_key_len = cipher_key_length;

		zrtp_memcpy(&iprof.rtcp_policy, &iprof.rtp_policy, sizeof(iprof.rtcp_policy));
		iprof.dk_cipher = session->blockcipher;

		zrtp_memcpy(&oprof, &iprof, sizeof(iprof));

		_zrtp_kdf( stream,
				   ZSTR_GV(cc->s0),
				   ZSTR_GVP(input_mk_label),
				   ZSTR_GV(stream->protocol->cc->kdf_context),
				   cipher_key_length,
				   ZSTR_GV(iprof.key));
		_zrtp_kdf( stream,
				   ZSTR_GV(cc->s0),
				   ZSTR_GVP(input_ms_label),
				   ZSTR_GV(stream->protocol->cc->kdf_context),
				   14,
				   ZSTR_GV(iprof.salt));
		_zrtp_kdf( stream,
				   ZSTR_GV(cc->s0),
				   ZSTR_GVP(output_mk_label),
				   ZSTR_GV(stream->protocol->cc->kdf_context),
				   cipher_key_length,
				   ZSTR_GV(oprof.key));
		_zrtp_kdf( stream,
				   ZSTR_GV(cc->s0),
				   ZSTR_GVP(output_ms_label),
				   ZSTR_GV(stream->protocol->cc->kdf_context),
				   14,
				   ZSTR_GV(oprof.salt));

		stream->protocol->_srtp = zrtp_srtp_create(session->zrtp->srtp_global, &iprof, &oprof);

		/* Profiles and keys in them are not needed anymore - clear them */
		zrtp_memset(&iprof, 0, sizeof(iprof));
		zrtp_memset(&oprof, 0, sizeof(oprof));

		if (!stream->protocol->_srtp) {
			ZRTP_LOG(1,(_ZTU_,"\tERROR! Can't initialize SRTP engine. ID=%u\n", stream->id));
			return zrtp_status_fail;
		}
	} /* SRTP initialization */

    return zrtp_status_ok;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_enter_secure(zrtp_stream_t* stream)
{
	/*
     * When switching to SECURE all ZRTP crypto values were already computed by
	 * state-machine. Then we need to have logic to manage SAS value and shared
	 * secrets only. So: we compute SAS, refresh secrets flags and save the
	 * secrets to the cache after RS2 and RS1 swapping.  We don't need any
	 * crypto sources any longer - destroy them.
     */

	zrtp_status_t s				= zrtp_status_ok;
	zrtp_proto_crypto_t* cc		= stream->protocol->cc;
	zrtp_session_t *session		= stream->session;
	zrtp_secrets_t *secrets		= &stream->session->secrets;
	uint8_t was_exp   = 0;
	uint64_t exp_date = 0;

	ZRTP_LOG(3,(_ZTU_,"\tEnter state SECURE (%s).\n", zrtp_log_mode2str(stream->mode)));

	_zrtp_cancel_send_packet_later(stream, ZRTP_NONE);

	/*
	 * Compute the SAS value if it isn't computed yet. If there are several
	 * streams running in parallel - stream with the biggest hvi should
	 * generate the SAS.
	 */
	if (!session->sas1.length) {
		s = session->sasscheme->compute(session->sasscheme, stream, session->hash, 0);
		if (zrtp_status_ok != s) {
			_zrtp_machine_enter_initiatingerror(stream, zrtp_error_software, 1);
			return s;
		}


		ZRTP_LOG(3,(_ZTU_,"\tThis is the very first stream in sID GENERATING SAS value.\n", session->id));
		ZRTP_LOG(3,(_ZTU_,"\tSAS computed: <%.16s> <%.16s>.\n", session->sas1.buffer, session->sas2.buffer));
	}

	/*
	 * Compute a new value for RS1 and store the prevoious one.
	 * Compute result secrets' flags.
	 */
	if (ZRTP_IS_STREAM_DH(stream))
	{
		ZRTP_LOG(3,(_ZTU_,"\tCheck expiration interval: last_use=%u ttl=%u new_ttl=%u exp=%u now=%u\n",
					secrets->rs1->lastused_at,
					secrets->rs1->ttl,
					stream->cache_ttl,
					(secrets->rs1->lastused_at + secrets->rs1->ttl),
					zrtp_time_now()/1000));
		
		if (secrets->rs1->ttl != 0xFFFFFFFF) {
			exp_date = secrets->rs1->lastused_at;
			exp_date += secrets->rs1->ttl;						
			
			if (ZRTP_IS_STREAM_DH(stream) && (exp_date < zrtp_time_now()/1000)) {
				ZRTP_LOG(3,(_ZTU_,"\tUsing EXPIRED secrets: last_use=%u ttl=%u exp=%u now=%u\n",
								secrets->rs1->lastused_at,
								secrets->rs1->ttl,
								(secrets->rs1->lastused_at + secrets->rs1->ttl),
								zrtp_time_now()/1000));
				was_exp = 1;
			}
		}
		
		if (!was_exp) {
			secrets->wrongs = secrets->matches ^ secrets->cached;
			secrets->wrongs &= ~ZRTP_BIT_RS2;
			secrets->wrongs &= ~ZRTP_BIT_PBX;
		}
	}
	
	/*
	 * We going to update RS1 and change appropriate secrets flags. Let's back-up current values.
	 * Back-upped values could be used in debug purposes and in the GUI to reflect current state of the call
	 */
	if (!ZRTP_IS_STREAM_MULT(stream)) {
		secrets->cached_curr = secrets->cached;
		secrets->matches_curr = secrets->matches;
		secrets->wrongs_curr = secrets->wrongs;
	}
	
	
	ZRTP_LOG(3,(_ZTU_,"\tFlags C=%x M=%x W=%x ID=%u\n",
				secrets->cached, secrets->matches, secrets->wrongs, stream->id));

	_zrtp_change_state(stream, ZRTP_STATE_SECURE);
	/*
	 * Alarm user if the following condition is TRUE for both RS1 and RS2:
	 * "secret is wrong if it has been restored from the cache but hasn't matched
	 * with the remote one".
	 */	
	if (session->zrtp->cb.event_cb.on_zrtp_protocol_event) {
		session->zrtp->cb.event_cb.on_zrtp_protocol_event(stream, ZRTP_EVENT_IS_SECURE);
	}
	if (session->zrtp->cb.event_cb.on_zrtp_secure) {
		session->zrtp->cb.event_cb.on_zrtp_secure(stream);
	}
	
	/* Alarm user if possible MiTM attack detected */
	if (secrets->wrongs) {
		session->mitm_alert_detected = 1;
		
		if (session->zrtp->cb.event_cb.on_zrtp_security_event) {
			session->zrtp->cb.event_cb.on_zrtp_security_event(stream, ZRTP_EVENT_MITM_WARNING);
		}
	}

	/* Check for unenrollemnt first */
	if ((secrets->cached & ZRTP_BIT_PBX) && !(secrets->matches & ZRTP_BIT_PBX)) {
		ZRTP_LOG(2,(_ZTU_,"\tINFO! The user requires new un-enrolment - the nedpint may clear"
					" the cache or perform other action. ID=%u\n", stream->id));

		if (session->zrtp->cb.event_cb.on_zrtp_protocol_event) {
			session->zrtp->cb.event_cb.on_zrtp_protocol_event(stream, ZRTP_EVENT_USER_UNENROLLED);
		}
	}

    /*
	 * Handle PBX registration, if required: If PBX already had a shared secret
	 * for the ZID it leaves the cache entry unmodified. Else, it computes a new
	 * one. If the PBX detects cache entry for the static shared secret, but the
	 * phone does not have a matching cache entry - the PBX generates a new one.
	 */
	if (ZRTP_MITM_MODE_REG_SERVER == stream->mitm_mode)
	{
		if (secrets->matches & ZRTP_BIT_PBX) {
			ZRTP_LOG(2,(_ZTU_,"\tINFO! User have been already registered - skip enrollment ritual. ID=%u\n", stream->id));
			if (session->zrtp->cb.event_cb.on_zrtp_protocol_event) {
				session->zrtp->cb.event_cb.on_zrtp_protocol_event(stream, ZRTP_EVENT_USER_ALREADY_ENROLLED);
			}
		} else {			
			ZRTP_LOG(2,(_ZTU_,"\tINFO! The user requires new enrolment - generate new MiTM secret. ID=%u\n", stream->id));
			zrtp_register_with_trusted_mitm(stream);
			if (session->zrtp->cb.event_cb.on_zrtp_protocol_event) {
				stream->zrtp->cb.event_cb.on_zrtp_protocol_event(stream, ZRTP_EVENT_NEW_USER_ENROLLED);
			}
			
		}		
	}
	else if (ZRTP_MITM_MODE_REG_CLIENT == stream->mitm_mode)
	{
		if (session->zrtp->cb.event_cb.on_zrtp_protocol_event) {
			session->zrtp->cb.event_cb.on_zrtp_protocol_event(stream, ZRTP_EVENT_IS_CLIENT_ENROLLMENT);
		}
	}	

	/*
	 * Compute new RS for FULL DH streams only. Don't update RS1 if cache TTL is 0
	 */
	if (ZRTP_IS_STREAM_DH(stream))
	{
		static const zrtp_string32_t rss_label = ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_RS_STR);
		
		if (stream->cache_ttl > 0) {			
			/* Replace RS2 with RS1 */
			zrtp_sys_free(secrets->rs2);
			secrets->rs2 = secrets->rs1;

			secrets->rs1 = _zrtp_alloc_shared_secret(session);
			if (!secrets->rs1) {
				_zrtp_machine_enter_initiatingerror(stream, zrtp_error_software, 1);
				return zrtp_status_fail;
			}

			/*
			 * Compute new RS1 based on previous one and S0:
			 * rs1 = KDF(s0, "retained secret", KDF_Context, negotiated hash length)
			 */
			_zrtp_kdf( stream,
					   ZSTR_GV(cc->s0),
					   ZSTR_GV(rss_label),
					   ZSTR_GV(cc->kdf_context),
					   ZRTP_HASH_SIZE,
					   ZSTR_GV(secrets->rs1->value));

			/*
			 * Mark secrets as cached: RS1 have been just generated and cached;
			 * RS2 is cached if previous secret was cached as well.
			 */
			secrets->rs1->_cachedflag = 1;
			secrets->cached |= ZRTP_BIT_RS1;
			secrets->matches |= ZRTP_BIT_RS1;
			if (secrets->rs2->_cachedflag) {
				secrets->cached |= ZRTP_BIT_RS2;
			}

			/* Let's update the TTL interval for the new secret */
			secrets->rs1->ttl = stream->cache_ttl;
			secrets->rs1->lastused_at = (uint32_t)(zrtp_time_now()/1000);

			/* If possible MiTM attach detected - postpone storing the cache until after the user verify the SAS */
			if (!session->mitm_alert_detected) {
				if (session->zrtp->cb.cache_cb.on_put) {
					session->zrtp->cb.cache_cb.on_put( ZSTR_GV(session->zid),
													   ZSTR_GV(session->peer_zid),
													   secrets->rs1);
				}
			}

			{
			uint32_t verifiedflag = 0;
			char buff[128];
			if (session->zrtp->cb.cache_cb.on_get_verified) {
				session->zrtp->cb.cache_cb.on_get_verified( ZSTR_GV(session->zid),
															ZSTR_GV(session->peer_zid),
															&verifiedflag);
			}

			ZRTP_LOG(3,(_ZTU_,"\tNew secret was generated:\n"));
			ZRTP_LOG(3,(_ZTU_,"\t\tRS1 value:<%s>\n",
						hex2str(secrets->rs1->value.buffer, secrets->rs1->value.length, buff, sizeof(buff))));
			ZRTP_LOG(3,(_ZTU_,"\t\tTTL=%u, flags C=%x M=%x W=%x V=%d\n",
						secrets->rs1->ttl, secrets->cached, secrets->matches, secrets->wrongs, verifiedflag));
			}
		} /* for TTL > 0 only */
		else {
			if (session->zrtp->cb.cache_cb.on_put) {
				secrets->rs1->ttl = 0;
				session->zrtp->cb.cache_cb.on_put( ZSTR_GV(session->zid),
												   ZSTR_GV(session->peer_zid),
												   secrets->rs1);
			}		
		}
	} /* For DH mode only */

	
	if (session->zrtp->cb.event_cb.on_zrtp_protocol_event) {
		session->zrtp->cb.event_cb.on_zrtp_protocol_event(stream, ZRTP_EVENT_IS_SECURE_DONE);
	}	

	/* We have computed all subkeys from S0 and don't need it any longer. */
	zrtp_wipe_zstring(ZSTR_GV(cc->s0));

	/* Clear DH crypto context */
	if (ZRTP_IS_STREAM_DH(stream)) {
		bnEnd(&stream->dh_cc.peer_pv);
		bnEnd(&stream->dh_cc.pv);
		bnEnd(&stream->dh_cc.sv);
		zrtp_wipe_zstring(ZSTR_GV(stream->dh_cc.dhss));
	}
	
	/*
	 * Now, let's check if the transition to CLEAR was caused by Active/Passive rules.
	 * If local endpoint is a MitM and peer MiTM linked stream is Unlimited, we
	 * could break the rules and send commit to Passive endpoint.
	 */
	if (stream->zrtp->is_mitm && stream->peer_super_flag) {
		if (stream->linked_mitm && stream->linked_mitm->peer_passive) {
			if (stream->linked_mitm->state == ZRTP_STATE_CLEAR) {
				ZRTP_LOG(2,(_ZTU_,"INFO: Linked Peer stream id=%u suspended in CLEAR-state due to"
							" Active/Passive restrictions, but we are running in MiTM mode and "
							"current peer endpoint is Super-Active. Let's Go Secure for the linked stream.\n", stream->id));
				
				/* @note: don't use zrtp_secure_stream() wrapper as it checks for Active/Passive stuff. */
				_zrtp_machine_start_initiating_secure(stream->linked_mitm);
			}
		}
	}
	
	/*
	 * Increase calls counter for Preshared mode and reset it on DH
	 */
	if (session->zrtp->cb.cache_cb.on_presh_counter_get && session->zrtp->cb.cache_cb.on_presh_counter_set) {
		uint32_t calls_counter = 0;
		session->zrtp->cb.cache_cb.on_presh_counter_get( ZSTR_GV(session->zid),
														ZSTR_GV(session->peer_zid),
														&calls_counter);
		if (ZRTP_IS_STREAM_DH(stream)) {
			session->zrtp->cb.cache_cb.on_presh_counter_set( ZSTR_GV(session->zid),
															ZSTR_GV(session->peer_zid),
															0);
		} else if ZRTP_IS_STREAM_PRESH(stream) {
			session->zrtp->cb.cache_cb.on_presh_counter_set( ZSTR_GV(session->zid),
															ZSTR_GV(session->peer_zid),
															++calls_counter);
		}
	}
	
	clear_crypto_sources(stream);

	return zrtp_status_ok;
}


/*===========================================================================*/
/*		Shared functions													 */
/*===========================================================================*/

/*----------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_create_confirm( zrtp_stream_t *stream,
										    zrtp_packet_Confirm_t* confirm)
{
	void* cipher_ctx = NULL;
	zrtp_status_t s = zrtp_status_fail;
	zrtp_session_t *session = stream->session;
	uint32_t verifiedflag = 0;

	/* hash + (padding + sig_len + flags) + ttl */
	const uint8_t encrypted_body_size = ZRTP_MESSAGE_HASH_SIZE + (2 + 1 + 1) + 4;

	/*
	 * Create the Confirm packet according to draft 6.7
	 * AES CFB vector at first, SIG length and flags octet and cache TTL at the end
	 * This version doesn't support signatures so sig_length=0
	 */
	if (ZRTP_CFBIV_SIZE != zrtp_randstr(session->zrtp, confirm->iv, ZRTP_CFBIV_SIZE)) {
		return zrtp_status_fail;
	}

	zrtp_memcpy(confirm->hash, stream->messages.h0.buffer, ZRTP_MESSAGE_HASH_SIZE);

	if (session->zrtp->cb.cache_cb.on_get_verified) {
		session->zrtp->cb.cache_cb.on_get_verified( ZSTR_GV(session->zid),
												    ZSTR_GV(session->peer_zid),
												    &verifiedflag);
	}

	confirm->expired_interval = zrtp_hton32(session->profile.cache_ttl);
	confirm->flags = 0;
	confirm->flags |= session->profile.disclose_bit ? 0x01 : 0x00;
	confirm->flags |= session->profile.allowclear ? 0x02 : 0x00;
	confirm->flags |= verifiedflag ? 0x04 : 0x00;
	confirm->flags |= (ZRTP_MITM_MODE_REG_SERVER == stream->mitm_mode) ? 0x08 : 0x00;

	/* Then we need to encrypt Confirm before Hmac computing. Use AES CFB */
	do
	{
		cipher_ctx = session->blockcipher->start( session->blockcipher,
												  (uint8_t*)stream->cc.zrtp_key.buffer,
												  NULL,
												  ZRTP_CIPHER_MODE_CFB);
		if (!cipher_ctx) {
			break;
		}

		s = session->blockcipher->set_iv(session->blockcipher, cipher_ctx, (zrtp_v128_t*)confirm->iv);
		if (zrtp_status_ok != s) {
			break;
		}

		s = session->blockcipher->encrypt( session->blockcipher,
										    cipher_ctx,
										    (uint8_t*)&confirm->hash,
										    encrypted_body_size );
	} while(0);
	if (cipher_ctx) {
		session->blockcipher->stop(session->blockcipher, cipher_ctx);
	}
	if (zrtp_status_ok != s) {
		ZRTP_LOG(1,(_ZTU_,"ERROR! failed to encrypt Confirm. s=%d ID=%u\n", s, stream->id));
		return s;
	}

	/* Compute Hmac over encrypted part of Confirm */
	{
		zrtp_string128_t hmac = ZSTR_INIT_EMPTY(hmac);
		s = session->hash->hmac_c( session->hash,
								    stream->cc.hmackey.buffer,
								    stream->cc.hmackey.length,
								    (const char*)&confirm->hash,
								    encrypted_body_size,
								    ZSTR_GV(hmac) );
		if (zrtp_status_ok != s) {
			ZRTP_LOG(1,(_ZTU_,"ERROR! failed to compute Confirm hmac. s=%d ID=%u\n", s, stream->id));
			return s;
		}
        
        zrtp_memcpy(confirm->hmac, hmac.buffer, ZRTP_HMAC_SIZE);
        
        {
            char buff[512];
            ZRTP_LOG(3,(_ZTU_,"HMAC TRACE. COMPUTE.\n"));
            ZRTP_LOG(3,(_ZTU_,"\tcipher text:%s. size=%u\n",
                        hex2str((const char*)&confirm->hash, encrypted_body_size, buff, sizeof(buff)), encrypted_body_size));
            ZRTP_LOG(3,(_ZTU_,"\t        key:%s.\n",
                        hex2str(stream->cc.hmackey.buffer, stream->cc.hmackey.length, buff, sizeof(buff))));
            ZRTP_LOG(3,(_ZTU_,"\t comp hmac:%s.\n",
                        hex2str(hmac.buffer, hmac.length, buff, sizeof(buff))));
            ZRTP_LOG(3,(_ZTU_,"\t      hmac:%s.\n",
                        hex2str((const char*)confirm->hmac, ZRTP_HMAC_SIZE, buff, sizeof(buff))));
        }
	}

	return zrtp_status_ok;
}

/*----------------------------------------------------------------------------*/
zrtp_status_t _zrtp_machine_process_confirm( zrtp_stream_t *stream,
											 zrtp_packet_Confirm_t *confirm)
{
	/* Compute Hmac over encrypted part of Confirm and reject malformed packets */
	void* cipher_ctx = NULL;
	zrtp_status_t s = zrtp_status_fail;
	zrtp_session_t *session = stream->session;
	zrtp_string128_t hmac = ZSTR_INIT_EMPTY(hmac);

	/* hash + (padding + sig_len + flags) + ttl */
	const uint8_t encrypted_body_size = ZRTP_MESSAGE_HASH_SIZE + (2 + 1 + 1) + 4;
	s = session->hash->hmac_c( session->hash,
							    stream->cc.peer_hmackey.buffer,
							    stream->cc.peer_hmackey.length,
							    (const char*)&confirm->hash,
							    encrypted_body_size,
							    ZSTR_GV(hmac) );
	if (zrtp_status_ok != s) {
		ZRTP_LOG(1,(_ZTU_,"\tERROR! failed to compute Incoming Confirm hmac. s=%d ID=%u\n", s, stream->id));
		return zrtp_status_fail;
	}
    
    
    // MARK: TRACE CONFIRM HMAC ERROR
#if 0
    {
        char buff[512];
        ZRTP_LOG(3,(_ZTU_,"HMAC TRACE. VERIFY\n"));
        ZRTP_LOG(3,(_ZTU_,"\tcipher text:%s. size=%u\n",
                    hex2str((const char*)&confirm->hash, encrypted_body_size, buff, sizeof(buff)), encrypted_body_size));
        ZRTP_LOG(3,(_ZTU_,"\t        key:%s.\n",
                    hex2str(stream->cc.peer_hmackey.buffer, stream->cc.peer_hmackey.length, buff, sizeof(buff))));
        ZRTP_LOG(3,(_ZTU_,"\t comp hmac:%s.\n",
                    hex2str(hmac.buffer, hmac.length, buff, sizeof(buff))));
        ZRTP_LOG(3,(_ZTU_,"\t      hmac:%s.\n",
                    hex2str((const char*)confirm->hmac, ZRTP_HMAC_SIZE, buff, sizeof(buff))));
    }
#endif
    

	if (0 != zrtp_memcmp(confirm->hmac, hmac.buffer, ZRTP_HMAC_SIZE)) {
		/*
		 * Weird. Perhaps a bug in our code or our peer's code. Or it could be an attacker
		 * who doesn't realize that Man-In-The-Middling the Diffie-Hellman key generation
		 * but allowing the correct rsIds to pass through accomplishes nothing more than
		 * forcing us to fallback to cleartext mode. If this attacker had gone ahead and deleted
		 * or replaced the rsIds, then he would have been able to stay in the middle (although
		 * he would of course still face the threat of a Voice Authentication Check).  On the
		 * other hand if this attacker wanted to force us to fallback to cleartext mode, he could
		 * have done that more simply, for example by intercepting our ZRTP HELLO packet and
		 * replacing it with a normal non-ZRTP comfort noise packet.  In any case, we'll do our
		 * "switch to cleartext fallback" behavior.
		 */

		ZRTP_LOG(2,(_ZTU_,"\tWARNING!" ZRTP_VERIFIED_RESP_WARNING_STR "ID=%u\n", stream->id));

		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_auth_decrypt, 1);
		return zrtp_status_fail;
	}

	/* Then we need to decrypt Confirm body */
	do {
		cipher_ctx = session->blockcipher->start( session->blockcipher,
												   (uint8_t*)stream->cc.peer_zrtp_key.buffer,
												   NULL,
												   ZRTP_CIPHER_MODE_CFB);
		if (!cipher_ctx) {
			break;
		}

		s = session->blockcipher->set_iv( session->blockcipher,
										   cipher_ctx,
										   (zrtp_v128_t*)confirm->iv);
		if (zrtp_status_ok != s) {
			break;
		}

		s = session->blockcipher->decrypt( session->blockcipher,
										    cipher_ctx,
										    (uint8_t*)&confirm->hash,
										    encrypted_body_size);
	} while(0);
	if (cipher_ctx) {
		session->blockcipher->stop(session->blockcipher, cipher_ctx);
	}	
	if (zrtp_status_ok != s) {
		ZRTP_LOG(3,(_ZTU_,"\tERROR! failed to decrypt incoming  Confirm. s=%d ID=%u\n", s, stream->id));
		return s;
	}

	/* We have access to hash field and can check hmac of the previous message */
	{
		zrtp_msg_hdr_t *hdr = NULL;
		char *key=NULL;
		zrtp_string32_t tmphash_str = ZSTR_INIT_EMPTY(tmphash_str);
		zrtp_hash_t *hash = zrtp_comp_find( ZRTP_CC_HASH, ZRTP_HASH_SHA256, stream->zrtp);

		if (ZRTP_IS_STREAM_DH(stream)) {
			hdr = &stream->messages.peer_dhpart.hdr;
			key = (char*)confirm->hash;
		} else {
			hash->hash_c(hash, (char*)confirm->hash, ZRTP_MESSAGE_HASH_SIZE, ZSTR_GV(tmphash_str));

			if (ZRTP_STATEMACHINE_INITIATOR == stream->protocol->type) {
				hdr = &stream->messages.peer_hello.hdr;
				hash->hash_c( hash,
							  tmphash_str.buffer,
						      ZRTP_MESSAGE_HASH_SIZE,
							  ZSTR_GV(tmphash_str) );
			} else {
				hdr = &stream->messages.peer_commit.hdr;
			}
			key = tmphash_str.buffer;
		}

		if (0 != _zrtp_validate_message_hmac(stream, hdr, key)) {
			return zrtp_status_fail;
		}
	}

	/* Set evil bit if other-side shared session key */
	stream->peer_disclose_bit = (confirm->flags & 0x01);

	/* Enable ALLOWCLEAR option if only both sides support it */
	stream->allowclear = (confirm->flags & 0x02) && session->profile.allowclear;

	/* Drop RS1 VERIFIED flag if other side didn't verified key exchange */
	if (0 == (confirm->flags & 0x04)) {
		ZRTP_LOG(2,(_ZTU_,"\tINFO: Other side Confirm V=0 - set verified to 0! ID=%u\n", stream->id));
		zrtp_verified_set(session->zrtp, &session->zid, &session->peer_zid, 0);
	}

    /* Look for Enrollment replay flag */
	if (confirm->flags & 0x08)
	{
		ZRTP_LOG(2,(_ZTU_,"\tINFO: Confirm PBX Enrolled flag is set - it is a Registration call! ID=%u\n", stream->id));

		if (stream->mitm_mode != ZRTP_MITM_MODE_CLIENT) {
			ZRTP_LOG(2,(_ZTU_,"\tERROR: PBX enrollment flag was received in wrong MiTM mode %s."
						" ID=%u\n", zrtp_log_mode2str(stream->mode), stream->id));			
			_zrtp_machine_enter_initiatingerror(stream, zrtp_error_invalid_packet, 1);
			return zrtp_status_fail;
		}
		
		/* Passive endpoint should ignore PBX Enrollment. */
		if (ZRTP_LICENSE_MODE_PASSIVE != stream->zrtp->lic_mode) {
			stream->mitm_mode = ZRTP_MITM_MODE_REG_CLIENT;
		} else {
			ZRTP_LOG(2,(_ZTU_,"\tINFO: Ignore PBX Enrollment flag as we are Passive ID=%u\n", stream->id));			
		}
	}

	stream->cache_ttl = ZRTP_MIN(session->profile.cache_ttl, zrtp_ntoh32(confirm->expired_interval));

	/* Copy packet for future hashing */
	zrtp_memcpy(&stream->messages.peer_confirm, confirm, zrtp_ntoh16(confirm->hdr.length)*4);

	return zrtp_status_ok;
}
