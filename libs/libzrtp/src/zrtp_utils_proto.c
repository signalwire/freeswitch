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
zrtp_status_t _zrtp_prepare_secrets(zrtp_session_t* session)
{
	zrtp_secrets_t* sec = &session->secrets;
	zrtp_status_t s = zrtp_status_ok;
	
	/* Protect Secrets from race conditions on multistream calls. */
	zrtp_mutex_lock(session->streams_protector);
	
	if (!sec->is_ready) {
		do {
			uint32_t verifiedflag = 0;
			
			session->secrets.rs1->_cachedflag  = 0;
			session->secrets.rs2->_cachedflag  = 0;		
			if (session->zrtp->cb.cache_cb.on_get) {
				s = session->zrtp->cb.cache_cb.on_get( ZSTR_GV(session->zid),
													   ZSTR_GV(session->peer_zid),
													   session->secrets.rs1,
													   0);
				session->secrets.rs1->_cachedflag = (zrtp_status_ok == s);
				
				s = session->zrtp->cb.cache_cb.on_get( ZSTR_GV(session->zid),
													   ZSTR_GV(session->peer_zid),
													   session->secrets.rs2,
													   1);
				session->secrets.rs2->_cachedflag = (zrtp_status_ok == s);			 			 
			}
			
			if (session->zrtp->cb.cache_cb.on_get_verified) {
				s = session->zrtp->cb.cache_cb.on_get_verified( ZSTR_GV(session->zid),
															   ZSTR_GV(session->peer_zid),
															   &verifiedflag);
			}

			if (session->zrtp->cb.cache_cb.on_get_mitm) {
				s = session->zrtp->cb.cache_cb.on_get_mitm( ZSTR_GV(session->zid),
															ZSTR_GV(session->peer_zid),
															session->secrets.pbxs);
				session->secrets.pbxs->_cachedflag = (zrtp_status_ok == s);
			} else {			
				session->secrets.pbxs->_cachedflag = 0;
			}
			
			/* Duplicate all secrets flags to zrtp-context */
			session->secrets.cached |= session->secrets.rs1->_cachedflag ? ZRTP_BIT_RS1 : 0;
			session->secrets.cached |= session->secrets.rs2->_cachedflag ? ZRTP_BIT_RS2 : 0;
			session->secrets.cached |= session->secrets.pbxs->_cachedflag ? ZRTP_BIT_PBX : 0;
			
			{
			char buff[128];
			char buff2[128];
			ZRTP_LOG(3,(_ZTU_,"\tRestoring Secrets: lZID=%s rZID=%s. V=%d sID=%u\n",
						hex2str(session->zid.buffer, session->zid.length, buff, sizeof(buff)),
						hex2str(session->peer_zid.buffer, session->peer_zid.length, buff2, sizeof(buff2)),
						verifiedflag,
						session->id));
			ZRTP_LOG(3,(_ZTU_,"\t\tRS1 <%s>\n",
						session->secrets.rs1->_cachedflag ?
						hex2str( session->secrets.rs1->value.buffer,
								session->secrets.rs1->value.length,
								buff, sizeof(buff) )     : "EMPTY"));
			ZRTP_LOG(3,(_ZTU_,"\t\tRS2 <%s>\n",
						session->secrets.rs2->_cachedflag ?
						hex2str( session->secrets.rs2->value.buffer,
								session->secrets.rs2->value.length,
								buff, sizeof(buff) )     : "EMPTY"));
			ZRTP_LOG(3,(_ZTU_,"\t\tPBX <%s>\n",
						session->secrets.pbxs->_cachedflag ?
						hex2str( session->secrets.pbxs->value.buffer,
								session->secrets.pbxs->value.length,
								buff, sizeof(buff) )     : "EMPTY"));
			}
			
			sec->is_ready = 1;
			s = zrtp_status_ok;
		} while (0);
	}
	
	zrtp_mutex_unlock(session->streams_protector);
	
	return s;
}

/*----------------------------------------------------------------------------*/
zrtp_shared_secret_t *_zrtp_alloc_shared_secret(zrtp_session_t* session)
{
    zrtp_shared_secret_t *ss = zrtp_sys_alloc(sizeof(zrtp_shared_secret_t));
    if (ss) {		
    	zrtp_memset(ss, 0, sizeof(zrtp_shared_secret_t));
		ZSTR_SET_EMPTY(ss->value);
		ss->value.length = ZRTP_MIN(ss->value.max_length, ZRTP_RS_SIZE);
		
		ss->lastused_at  = (uint32_t)(zrtp_time_now()/1000);
		ss->ttl			 = 0xFFFFFFFF;
		ss->_cachedflag	 = 0;
    	ss->value.length = ZRTP_MIN(ss->value.max_length, ZRTP_RS_SIZE);
		
		if (ss->value.length != zrtp_randstr( session->zrtp,
											 (unsigned char*)ss->value.buffer,
											 ss->value.length)) 
		{
			zrtp_sys_free(ss);
			ss = NULL;
		}
    }
    
    return ss;
}

/*----------------------------------------------------------------------------*/
int _zrtp_can_start_stream(zrtp_stream_t* stream, zrtp_stream_t **conc, zrtp_stream_mode_t mode)
{
	uint8_t deny = 0;
    mlist_t* node = NULL;
    
	zrtp_mutex_lock(stream->zrtp->sessions_protector);
	
    mlist_for_each(node, &stream->zrtp->sessions_head)
    {
		zrtp_session_t* tmp_sctx = mlist_get_struct(zrtp_session_t, _mlist, node);
		
		if ( !zrtp_zstrcmp(ZSTR_GV(tmp_sctx->zid), ZSTR_GV(stream->session->zid)) &&
			!zrtp_zstrcmp(ZSTR_GV(tmp_sctx->peer_zid), ZSTR_GV(stream->session->peer_zid)) )
		{
			int i = 0;
			
			zrtp_mutex_lock(tmp_sctx->streams_protector);
			
			for (i=0; i<ZRTP_MAX_STREAMS_PER_SESSION; i++)
			{
				zrtp_stream_t* tmp_stctx = &tmp_sctx->streams[i];
				
				/*
				 * We don't need to lock the stream because it have been already locked
				 * by high level function: zrtp_process_srtp() or _initiating_secure()
				 */
				if ((stream != tmp_stctx) && (tmp_stctx->state != ZRTP_STATE_NONE)) {
					deny = ( (tmp_stctx->state > ZRTP_STATE_START_INITIATINGSECURE) &&
							(tmp_stctx->state < ZRTP_STATE_SECURE) );
					
					if ((mode == ZRTP_STREAM_MODE_MULT) && deny) {
						deny = !(tmp_stctx->mode == ZRTP_STREAM_MODE_MULT);
					}
					
					if (deny) {
						*conc = tmp_stctx;						
						break;
					}
				}
			}
			
			zrtp_mutex_unlock(tmp_sctx->streams_protector);
		    
			if (deny) {
				break;
			}
		}
    }
	
	zrtp_mutex_unlock(stream->zrtp->sessions_protector);
	
	if (!deny){
		*conc = NULL;
	}
	
    return !deny;
}

/*----------------------------------------------------------------------------*/
uint8_t _zrtp_choose_best_comp( zrtp_profile_t *profile,
							   zrtp_packet_Hello_t* peer_hello,
							   zrtp_crypto_comp_t type )
{
	uint8_t* prof_elem = NULL;
    int i=0, j=0;
	int offset = 0;
	int count = 0;
	
    switch (type)
    {
		case ZRTP_CC_PKT:
		{
			uint8_t pref_peer_pk = ZRTP_COMP_UNKN;
			uint8_t pref_pk = ZRTP_COMP_UNKN;
			char *cp = NULL;
			
			prof_elem = (uint8_t*)profile->pk_schemes;
			offset = (peer_hello->hc + peer_hello->cc + peer_hello->ac) * ZRTP_COMP_TYPE_SIZE;
			count = peer_hello->kc;
			
			/* Looking for peer preferable DH scheme */
			cp = (char*)peer_hello->comp + offset;
			for (i=0; i<count; i++, cp+=ZRTP_COMP_TYPE_SIZE) {
				uint8_t tmp_pref_peer_pk = zrtp_comp_type2id(type, cp);
				j = 0;
				while (prof_elem[j]) {
					if (prof_elem[j++] == tmp_pref_peer_pk) {
						pref_peer_pk = tmp_pref_peer_pk;
						break;
					}
				}
				if (ZRTP_COMP_UNKN != pref_peer_pk) {
					break;
				}
			}
			
			/* Looking for local preferable DH scheme */
			i=0;
			while (prof_elem[i]) {
				uint8_t tmp_pref_pk = prof_elem[i++];
				cp = (char*)peer_hello->comp + offset;
				for (j=0; j<count; j++, cp+=ZRTP_COMP_TYPE_SIZE) {
					if(tmp_pref_pk == zrtp_comp_type2id(type, cp)) {
						pref_pk = tmp_pref_pk;
						break;
					}
				}
				if (ZRTP_COMP_UNKN != pref_pk) {
					break;
				}
			}
			
			ZRTP_LOG(3,(_ZTU_,"\t_zrtp_choose_best_comp() for PKT. local=%s remote=%s, choosen=%s\n",
						zrtp_comp_id2type(type, pref_pk), zrtp_comp_id2type(type, pref_peer_pk), zrtp_comp_id2type(type, ZRTP_MIN(pref_peer_pk, pref_pk))));
			
			/* Choose the fastest one. */
			return ZRTP_MIN(pref_peer_pk, pref_pk);
		} break;
		case ZRTP_CC_HASH:
			prof_elem = (uint8_t*)&profile->hash_schemes;
			offset = 0;
			count = peer_hello->hc;
			break;
		case ZRTP_CC_SAS:
			prof_elem = (uint8_t*)profile->sas_schemes;		
			offset = (peer_hello->hc + peer_hello->cc + peer_hello->ac + peer_hello->kc)* ZRTP_COMP_TYPE_SIZE;
			count = peer_hello->sc;
			break;
		case ZRTP_CC_CIPHER:
			prof_elem = (uint8_t*)profile->cipher_types;		
			offset = peer_hello->hc * ZRTP_COMP_TYPE_SIZE;
			count = peer_hello->cc;
			break;		
		case ZRTP_CC_ATL:
			prof_elem = (uint8_t*)profile->auth_tag_lens;
			offset = (peer_hello->hc + peer_hello->cc)*ZRTP_COMP_TYPE_SIZE;
			count = peer_hello->ac;
			break;
		default:		
			return ZRTP_COMP_UNKN;
    }
	
	while (prof_elem[i]) 
	{
		char *cp = (char*)peer_hello->comp + offset;
		uint8_t comp_id = prof_elem[i++];
		
		for (j=0; j<count; j++, cp+=ZRTP_COMP_TYPE_SIZE) {
			if (comp_id ==  zrtp_comp_type2id(type, cp)) {
				return comp_id;
			}
		}		
    }
	
	return ZRTP_COMP_UNKN;	
}

/*----------------------------------------------------------------------------*/
static int _is_presh_in_hello(zrtp_packet_Hello_t* hello)
{
	int i = 0;
	char* cp = (char*)hello->comp + (hello->hc + hello->cc + hello->ac) * ZRTP_COMP_TYPE_SIZE;
	for (i=0; i < hello->kc; i++, cp+=ZRTP_COMP_TYPE_SIZE) {
		if (!zrtp_memcmp(cp, ZRTP_PRESHARED, ZRTP_COMP_TYPE_SIZE)) {
			return i;
		}
	}
	
	return -1;
}

int _zrtp_is_dh_in_session(zrtp_stream_t* stream)
{
	uint8_t i = 0;
	for (i=0; i< ZRTP_MAX_STREAMS_PER_SESSION; i++) {
		zrtp_stream_t *tmp_stream = &stream->session->streams[i];
		if ((tmp_stream != stream) && ZRTP_IS_STREAM_DH(tmp_stream)) {
			return 0;
		}
	}
	return -1;
}

zrtp_stream_mode_t _zrtp_define_stream_mode(zrtp_stream_t* stream)
{	
	zrtp_session_t* session = stream->session;
	
	/*
	 * If ZRTP Session key is available - use Multistream mode.
	 * If both sides ready for Preshared and we have RS1 and it has Verified flag - try Preshared.
	 * Use DH in other cases
	 */ 
	if (session->zrtpsess.length > 0) {
		stream->pubkeyscheme = zrtp_comp_find(ZRTP_CC_PKT, ZRTP_PKTYPE_MULT, session->zrtp);
		return ZRTP_STREAM_MODE_MULT;		
	} else {
		/* If both sides ready for Preshared and we have RSes in our cache - try Preshared. */
		if (ZRTP_PKTYPE_PRESH == stream->pubkeyscheme->base.id)
		{	
			do {
				uint32_t verifiedflag = 0;
				uint32_t calls_counter = 0;
				
				if (_is_presh_in_hello(&stream->messages.peer_hello) < 0) {
					break;
				}
				
				if (ZRTP_IS_STREAM_PRESH(stream) && session->zrtp->cb.cache_cb.on_presh_counter_get) {					
					session->zrtp->cb.cache_cb.on_presh_counter_get( ZSTR_GV(session->zid),
																	ZSTR_GV(session->peer_zid),
																	&calls_counter);
					if (calls_counter >= ZRTP_PRESHARED_MAX_ALLOWED) {
						ZRTP_LOG(3,(_ZTU_,"\tDefine stream mode: user wants PRESHARED but Preshared"
									"calls counter reached the maximum value (ID=%u) -  Reset to DH.\n", stream->id));
						break;
					}
				}
				
				if (session->zrtp->cb.cache_cb.on_get_verified) {
					session->zrtp->cb.cache_cb.on_get_verified( ZSTR_GV(session->zid),
															   ZSTR_GV(session->peer_zid),
															   &verifiedflag);
				}
				
				if (!session->secrets.rs1->_cachedflag || !verifiedflag) {
					ZRTP_LOG(3,(_ZTU_,"\tDefine stream mode: user wants PRESHARED but we HAVE "
								"RS1=%d and V=%d. Reset to DH. ID=%u\n", session->secrets.rs1->_cachedflag, verifiedflag, stream->id));
					break;
				}
				
				ZRTP_LOG(3,(_ZTU_,"\tDefine stream mode: user wants PRESHARED and we have RS1,"
							" calls_counter=%d. Use preshared. ID=%u\n", calls_counter, stream->id));
				
				return ZRTP_STREAM_MODE_PRESHARED;				
			} while (0);
		}
		
		/* If Preshared not accepted by some reaseon - choose appropriate DH scheme. */
		if ( (ZRTP_PKTYPE_PRESH == stream->pubkeyscheme->base.id) ||
			(ZRTP_PKTYPE_MULT == stream->pubkeyscheme->base.id) )
		{
			int i=0, j=0;
			zrtp_packet_Hello_t* phello = &stream->messages.peer_hello;
			uint8_t comp_id = ZRTP_COMP_UNKN;
			
			while (session->profile.pk_schemes[i])
			{
				char *cp = (char*)phello->comp + (phello->hc + phello->cc + phello->ac) * ZRTP_COMP_TYPE_SIZE;
				comp_id = session->profile.pk_schemes[i++];
				if ((comp_id != ZRTP_PKTYPE_PRESH) && (comp_id != ZRTP_PKTYPE_MULT))
				{
					for (j=0; j<phello->kc; j++, cp+=ZRTP_COMP_TYPE_SIZE) {
						if (comp_id == zrtp_comp_type2id(ZRTP_CC_PKT, cp)) {
							break;
						}
					}
					if (j != phello->kc) {
						break;
					}
				}
			}
			
			stream->pubkeyscheme = zrtp_comp_find(ZRTP_CC_PKT, comp_id, session->zrtp);
		}
		
		return ZRTP_STREAM_MODE_DH;
	}	
}

/*---------------------------------------------------------------------------*/
int _zrtp_validate_message_hmac( zrtp_stream_t *stream,
								 zrtp_msg_hdr_t* msg2check,
								 char* hmackey)
{
	zrtp_string32_t hash_str = ZSTR_INIT_EMPTY(hash_str);
	zrtp_hash_t *hash = zrtp_comp_find(ZRTP_CC_HASH, ZRTP_HASH_SHA256, stream->session->zrtp);
	
	hash->hmac_truncated_c( hash,
						    hmackey,
						    ZRTP_MESSAGE_HASH_SIZE,
						    (char*)msg2check,
						    zrtp_ntoh16(msg2check->length)*4 - ZRTP_HMAC_SIZE,
						    ZRTP_HMAC_SIZE,
						    ZSTR_GV(hash_str));
	
	if (0 != zrtp_memcmp((char*)msg2check + (zrtp_ntoh16(msg2check->length)*4 - ZRTP_HMAC_SIZE), hash_str.buffer, ZRTP_HMAC_SIZE))
	{
		if (stream->zrtp->cb.event_cb.on_zrtp_security_event) {
			stream->zrtp->cb.event_cb.on_zrtp_security_event(stream, ZRTP_EVENT_WRONG_MESSAGE_HMAC);	
		}
		_zrtp_machine_enter_initiatingerror(stream, zrtp_error_wrong_meshmac, 0);
		return -1;
	}
	
	return 0;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_compute_preshared_key( zrtp_session_t *session,
										   zrtp_stringn_t* rs1,
										   zrtp_stringn_t* auxs,
										   zrtp_stringn_t* pbxs,
										   zrtp_stringn_t* key,
										   zrtp_stringn_t* key_id)
{
	static const zrtp_string8_t presh_key_str	= ZSTR_INIT_WITH_CONST_CSTRING(ZRTP_COMMIT_HV_KEY_STR);
	zrtp_string32_t preshared_key = ZSTR_INIT_EMPTY(preshared_key);
	static uint32_t length_rs = ZRTP_RS_SIZE;
	static const uint32_t length_zero = 0;		
	
	void *hash_ctx = session->hash->hash_begin(session->hash);
	if (!hash_ctx) {
		return zrtp_status_alloc_fail;
	}
	
	length_rs = zrtp_hton32(length_rs);
	
	if (rs1) {
		session->hash->hash_update(session->hash, hash_ctx, (const int8_t*)&length_rs, 4);
		session->hash->hash_update(session->hash, hash_ctx, (const int8_t*)rs1->buffer, ZRTP_RS_SIZE);
	} else {
		session->hash->hash_update(session->hash, hash_ctx, (const int8_t*)&length_zero, 4);
	}
	
	if (auxs) {
		session->hash->hash_update(session->hash, hash_ctx, (const int8_t*)&length_rs, 4);
		session->hash->hash_update(session->hash, hash_ctx, (const int8_t*)auxs->buffer, ZRTP_RS_SIZE);
	} else {
		session->hash->hash_update(session->hash, hash_ctx, (const int8_t*)&length_zero, 4);
	}
	
	if (pbxs) {
		session->hash->hash_update(session->hash, hash_ctx, (const int8_t*)&length_rs, 4);
		session->hash->hash_update(session->hash, hash_ctx, (const int8_t*)pbxs->buffer, ZRTP_RS_SIZE);
	} else {
		session->hash->hash_update(session->hash, hash_ctx, (const int8_t*)&length_zero, 4);
	}
	
	session->hash->hash_end(session->hash, hash_ctx, ZSTR_GV(preshared_key));
	if (key) {
		zrtp_zstrcpy(ZSTR_GVP(key), ZSTR_GV(preshared_key));
	}
	
	if (key_id) {
		session->hash->hmac_truncated( session->hash,
									   ZSTR_GV(preshared_key),
									   ZSTR_GV(presh_key_str),									   
									   ZRTP_HV_KEY_SIZE,
									   ZSTR_GVP(key_id));
	}
	
	return zrtp_status_ok;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t _zrtp_kdf( zrtp_stream_t* stream,
						 zrtp_stringn_t* ki,
						 zrtp_stringn_t* label, 
						 zrtp_stringn_t* context,
						 uint32_t length,
						 zrtp_stringn_t* digest)
{
	/*KDF(KI, Label, Context, L) = HMAC(KI, i | Label | 0x00 | Context | L) */
	uint32_t i = 1;	
	uint8_t o = 0;
	uint32_t L = zrtp_hton32(length*8);
	zrtp_hash_t* hash = stream->session->hash;	
	void* ctx = hash->hmac_begin(hash, ki);
	if (!ctx) {
		return zrtp_status_alloc_fail;
	}
	
	i = zrtp_hton32(i);
	hash->hmac_update(hash, ctx, (const char*)&i, sizeof(i));
	hash->hmac_update(hash, ctx, label->buffer, label->length);
	hash->hmac_update(hash, ctx, (const char*)&o, sizeof(o));
	hash->hmac_update(hash, ctx, context->buffer, context->length);
	hash->hmac_update(hash, ctx, (const char*)&L, sizeof(L));
	
	hash->hmac_end(hash, ctx, digest, length);
	
	return zrtp_status_ok;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_verified_set( zrtp_global_t *zrtp,
								 zrtp_string16_t *zid1,
								 zrtp_string16_t *zid2,
								 uint8_t verified )
{	
	mlist_t *node = NULL;
	
	if (!zrtp) {			  
		return zrtp_status_bad_param;
	}
	
	zrtp_mutex_lock(zrtp->sessions_protector);
	
	mlist_for_each(node, &zrtp->sessions_head)
	{
		zrtp_session_t *session = mlist_get_struct(zrtp_session_t, _mlist, node);
		if ( ( !zrtp_zstrcmp(ZSTR_GV(session->zid), ZSTR_GVP(zid1)) ||
			  !zrtp_zstrcmp(ZSTR_GV(session->zid), ZSTR_GVP(zid2)) ) &&
			( !zrtp_zstrcmp(ZSTR_GV(session->peer_zid), ZSTR_GVP(zid1)) ||
			 !zrtp_zstrcmp(ZSTR_GV(session->peer_zid), ZSTR_GVP(zid2)) ) )
		{
			if (session->zrtp->cb.cache_cb.on_set_verified) {
				session->zrtp->cb.cache_cb.on_set_verified(ZSTR_GVP(zid1), ZSTR_GVP(zid2), verified);
			}
			
			if (session->mitm_alert_detected) {
				session->mitm_alert_detected = 0;
				if (session->zrtp->cb.cache_cb.on_put) {
					session->zrtp->cb.cache_cb.on_put( ZSTR_GV(session->zid),
													   ZSTR_GV(session->peer_zid),
													   session->secrets.rs1);
				}
			}
		}
	}
	
	zrtp_mutex_unlock(zrtp->sessions_protector);
	return zrtp_status_ok;
}

/*----------------------------------------------------------------------------*/
uint32_t _zrtp_get_timeout(uint32_t curr_timeout, zrtp_msg_type_t msg)
{
	uint32_t timeout = curr_timeout;
	uint32_t base_interval = 0;
	uint32_t capping = 0;
#if (defined(ZRTP_BUILD_FOR_CSD) && (ZRTP_BUILD_FOR_CSD == 1))
	uint8_t  is_lineral = 1;
	capping				= 10000;
#else
	uint8_t  is_lineral = 0;
#endif
	switch (msg)
	{
		case ZRTP_NONE:
		case ZRTP_HELLOACK:
		case ZRTP_DHPART1:
		case ZRTP_CONFIRM1:
		case ZRTP_CONFIRM2ACK:
		case ZRTP_GOCLEARACK:
		case ZRTP_RELAYACK:
			return 0;
#if (defined(ZRTP_BUILD_FOR_CSD) && (ZRTP_BUILD_FOR_CSD == 1))
		case ZRTP_HELLO:
			base_interval = ZRTP_CSD_T1;
			break;
		case ZRTP_COMMIT:
			base_interval = ZRTP_CSD_T2;
			break;
		case ZRTP_DHPART2:
			base_interval = ZRTP_CSD_T3;
			break;
		case ZRTP_CONFIRM2:
			base_interval = ZRTP_CSD_T4;
			break;
		case ZRTP_GOCLEAR:		
		case ZRTP_SASRELAY:
			base_interval = ZRTP_CSD_T2;
			break;
		case ZRTP_ERROR:
			base_interval = ZRTP_CSD_ET;
			break;
#else
		case ZRTP_HELLO:
			base_interval = ZRTP_T1;
			capping = ZRTP_T1_CAPPING;
			break;
		case ZRTP_COMMIT:
		case ZRTP_DHPART2:
		case ZRTP_CONFIRM2:
		case ZRTP_GOCLEAR:		
		case ZRTP_SASRELAY:
			base_interval = ZRTP_T2;
			capping = ZRTP_T2_CAPPING;
			break;
		case ZRTP_ERROR:
		case ZRTP_ERRORACK:
			base_interval = ZRTP_ET;
			capping = ZRTP_T2_CAPPING;
			break;
#endif
		case ZRTP_PROCESS:
			base_interval = ZRTP_PROCESS_T1;
			break;
		default:
			return 0;
	}
	
	if (0 == timeout) {
		timeout = base_interval;
	} else if (!is_lineral) {
		timeout *= 2;
	} else {
		timeout += base_interval;
	}
	
	if (timeout > capping) {
		return capping;
	} else {
		return timeout;
	}
}

