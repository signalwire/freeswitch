/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#ifndef __ZRTP_TEST_CORE_H__
#define __ZRTP_TEST_CORE_H__

#include "zrtp.h"

extern zrtp_global_t* zrtp_global;

typedef uint32_t		zrtp_test_channel_id_t;

typedef struct zrtp_test_channel_config
{
	unsigned			streams_count;
	unsigned char		is_autosecure;
	unsigned char		is_preshared;
} zrtp_test_channel_config_t;

void zrtp_test_crypto(zrtp_global_t* zrtp);

int zrtp_test_zrtp_init();
int zrtp_test_zrtp_down();

int zrtp_test_channel_create( const zrtp_test_channel_config_t* config,
							  zrtp_test_channel_id_t* chan_id);
int zrtp_test_channel_delete(zrtp_test_channel_id_t chan_id);
int zrtp_test_channel_start(zrtp_test_channel_id_t chan_id);
int zrtp_test_channel_secure(zrtp_test_channel_id_t chan_id);
int zrtp_test_channel_clear(zrtp_test_channel_id_t chan_id);

#endif /*__ZRTP_TEST_CORE_H__*/
