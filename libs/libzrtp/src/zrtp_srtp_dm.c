/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#if (defined(ZRTP_USE_EXTERN_SRTP) && (ZRTP_USE_EXTERN_SRTP == 1))

/* exactly in this order (for winsock) */
#include <srtp.h>
#include "zrtp.h"

struct zrtp_srtp_ctx
{
	srtp_t	outgoing_srtp;
	srtp_t	incoming_srtp;
};

/*---------------------------------------------------------------------------*/
void init_policy(crypto_policy_t *sp, zrtp_srtp_policy_t *zp)
{
	//TODO: make incoming policy crypto algorithm check for David A. McGrew's implementation support

	/* there are no another appropriate ciphers in the David A. McGrew's implementation yet */
	sp->cipher_type		= AES_128_ICM;
	sp->cipher_key_len  = zp->cipher_key_len;
	sp->auth_type       = HMAC_SHA1;
	sp->auth_key_len    = zp->auth_key_len;
	sp->auth_tag_len    = zp->auth_tag_len->tag_length ? zp->auth_tag_len->tag_length : 10;
	sp->sec_serv        = sec_serv_conf_and_auth;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t create_srtp_stream( srtp_t *srtp_stream,
								  zrtp_srtp_profile_t *profile,
								  ssrc_type_t ssrc_type )
{
	srtp_policy_t policy;
	uint8_t *tmp_key;

	init_policy(&policy.rtp, &profile->rtp_policy);
	init_policy(&policy.rtcp, &profile->rtcp_policy);

	policy.ssrc.type  = ssrc_type;
	policy.ssrc.value = 0;
	
	/* David A. McGrew's implementation uses key and salt as whole buffer, so let's make it */
	tmp_key = (uint8_t*)zrtp_sys_alloc(profile->key.length + profile->salt.length);
	if(NULL == tmp_key){
		return zrtp_status_fail;
	}
	zrtp_memcpy(tmp_key, profile->key.buffer, profile->key.length);
	zrtp_memcpy(tmp_key+profile->key.length, profile->salt.buffer, profile->salt.length);
			
	policy.key = tmp_key;
	policy.next = NULL;

	/* add salt length to the key length of each policy */
	policy.rtp.cipher_key_len += 14;
	policy.rtcp.cipher_key_len += 14;

	if(err_status_ok != srtp_create(srtp_stream, &policy)){
		zrtp_sys_free(tmp_key);
		return zrtp_status_fail;
	}

	zrtp_sys_free(tmp_key);
	return zrtp_status_ok;
}


/*===========================================================================*/
/* Public interface															 */
/*===========================================================================*/


/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_srtp_init(zrtp_global_ctx_t *zrtp_global)
{
	err_status_t  s = srtp_init();
	return (err_status_ok == s) ? zrtp_status_ok : s;
}

zrtp_status_t zrtp_srtp_down( zrtp_global_ctx_t *zrtp_global )
{
	return zrtp_status_ok;
}

/*---------------------------------------------------------------------------*/
zrtp_srtp_ctx_t * zrtp_srtp_create(	zrtp_srtp_global_t *srtp_global,
									zrtp_srtp_profile_t *inc_profile, 
									zrtp_srtp_profile_t *out_profile)
{
	zrtp_status_t res = zrtp_status_ok;
	zrtp_srtp_ctx_t *srtp_ctx = NULL;

	if(NULL == inc_profile || NULL == out_profile){
		return NULL;
	}
	
	do{
		srtp_policy_t *policy_head, *policy_next;
		
		srtp_ctx = zrtp_sys_alloc(sizeof(zrtp_srtp_ctx_t));
		if(NULL == srtp_ctx){
			break;
		}
		
		res = create_srtp_stream(&srtp_ctx->incoming_srtp, inc_profile, ssrc_any_inbound);
		if(zrtp_status_ok != res){
			zrtp_sys_free(srtp_ctx);
			srtp_ctx = NULL;
			break;
		}

		res = create_srtp_stream(&srtp_ctx->outgoing_srtp, out_profile, ssrc_any_outbound);
		if(zrtp_status_ok != res){
			srtp_dealloc(srtp_ctx->incoming_srtp);
			zrtp_sys_free(srtp_ctx);
			srtp_ctx = NULL;
			break;
		}

	}while(0);
	
	return srtp_ctx;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_srtp_destroy( zrtp_srtp_global_t *zrtp_srtp_global,
					    zrtp_srtp_ctx_t *srtp_ctx )
{
	srtp_dealloc(srtp_ctx->incoming_srtp);
	srtp_dealloc(srtp_ctx->outgoing_srtp);
	zrtp_sys_free(srtp_ctx);
	return zrtp_status_ok;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_srtp_protect( zrtp_srtp_global_t *srtp_global,
								 zrtp_srtp_ctx_t *srtp_ctx,
								 zrtp_rtp_info_t *packet)
{
	err_status_t res;
	res = srtp_protect(srtp_ctx->outgoing_srtp, packet->packet, packet->length);
	if(err_status_ok != res){
		return zrtp_status_fail;
	}else{
		return zrtp_status_ok;
	}
}

/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_srtp_unprotect( zrtp_srtp_global_t *srtp_global,
								   zrtp_srtp_ctx_t *srtp_ctx,
								   zrtp_rtp_info_t *packet)
{
	err_status_t res;
	res = srtp_unprotect(srtp_ctx->incoming_srtp, packet->packet, packet->length);
	if(err_status_ok != res){
		return zrtp_status_fail;
	}else{
		return zrtp_status_ok;
	}
}

/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_srtp_protect_rtcp( zrtp_srtp_global_t *srtp_global, 
									  zrtp_srtp_ctx_t *srtp_ctx,
									  zrtp_rtp_info_t *packet)
{
	err_status_t res;
	res = srtp_protect_rtcp(srtp_ctx->outgoing_srtp, packet->packet, packet->length);
	if(err_status_ok != res){
		return zrtp_status_fail;
	}else{
		return zrtp_status_ok;
	}
}

/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_srtp_unprotect_rtcp(	zrtp_srtp_global_t *srtp_global,
										zrtp_srtp_ctx_t	*srtp_ctx,
										zrtp_rtp_info_t *packet)
{
	err_status_t res;
	res = srtp_unprotect_rtcp(srtp_ctx->incoming_srtp, packet->packet, packet->length);
	if(err_status_ok != res){
		return zrtp_status_fail;
	}else{
		return zrtp_status_ok;
	}
}

/*----------------------------------------------------------------------------*/
uint64_t make64(uint32_t high, uint32_t low)
{
	uint64_t_ res;
	uint32_t *p = (uint32_t*)&res;

#if ZRTP_BYTE_ORDER == ZBO_LITTLE_ENDIAN
	*p++ = low;
	*p = high;
#else
	*p++ = high;
	*p = low;
#endif
	return res;
}

uint32_t high32(uint64_t x)
{
	uint32_t *p = &x;
#if ZRTP_BYTE_ORDER == ZBO_LITTLE_ENDIAN
	p++;
#endif
	return *p;
}

uint32_t low32(uint64_t x)
{
	uint32_t *p = &x;
#if ZRTP_BYTE_ORDER == ZBO_BIG_ENDIAN
	p++;
#endif
	return *p;
}

#endif /*ZRTP_USE_EXTERN_SRTP*/
