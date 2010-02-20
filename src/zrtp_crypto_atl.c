/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */
 
#include "zrtp.h"


/*============================================================================*/
/*     SRTP Auth Tag Length support											  */
/*============================================================================*/

zrtp_status_t zrtp_defaults_atl(zrtp_global_t* global_ctx)
{
    zrtp_auth_tag_length_t* atl32 = zrtp_sys_alloc(sizeof(zrtp_auth_tag_length_t));
    zrtp_auth_tag_length_t* atl80 = zrtp_sys_alloc(sizeof(zrtp_auth_tag_length_t));

	if (!atl32 || !atl80) {
		if(atl32) zrtp_sys_free(atl32);
		if(atl80) zrtp_sys_free(atl80);
		return zrtp_status_alloc_fail;
	}
    
    zrtp_memset(atl32, 0, sizeof(zrtp_auth_tag_length_t));
    zrtp_memcpy(atl32->base.type, ZRTP_HS32, ZRTP_COMP_TYPE_SIZE);
	atl32->base.id			= ZRTP_ATL_HS32;
    atl32->base.zrtp	= global_ctx;
    atl32->tag_length		= 4;
        
    zrtp_memset(atl80, 0, sizeof(zrtp_auth_tag_length_t));
    zrtp_memcpy(atl80->base.type, ZRTP_HS80, ZRTP_COMP_TYPE_SIZE);
	atl80->base.id			= ZRTP_ATL_HS80;
	atl80->base.zrtp	= global_ctx;
    atl80->tag_length		= 10;
    
    zrtp_comp_register(ZRTP_CC_ATL, atl32, global_ctx);
    zrtp_comp_register(ZRTP_CC_ATL, atl80, global_ctx);
    
    return zrtp_status_ok;
}
