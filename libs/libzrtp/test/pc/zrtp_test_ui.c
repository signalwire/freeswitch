/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include "zrtp_test_core.h"

#ifndef ZRTP_TEST_ENABLE_CRYPTO_SELFTESTS
#define ZRTP_TEST_ENABLE_CRYPTO_SELFTESTS 0
#endif

static zrtp_test_channel_id_t tmp_id;

void do_create()
{
	zrtp_test_channel_config_t	config;
	int status = 0;
	
	config.is_autosecure	= 1;
	config.is_preshared		= 0;
	config.streams_count	= 1;
	
	status  = zrtp_test_channel_create(&config, &tmp_id);
}

void do_delete()
{
	zrtp_test_channel_delete(tmp_id);
}

void do_quit()
{
	zrtp_test_zrtp_down();
}

int main()
{
	int status;
	
	status = zrtp_test_zrtp_init();
	if (0 != status) {
		return status;
	}
#if (ZRTP_TEST_ENABLE_CRYPTO_SELFTESTS == 1)
	zrtp_test_crypto(zrtp_global);
#endif
	
	{
		zrtp_test_channel_id_t id;
		zrtp_test_channel_config_t sconfig;
		
		sconfig.is_autosecure = 0;
		sconfig.is_preshared  = 0;
		sconfig.streams_count = 1;
		
		status = zrtp_test_channel_create(&sconfig, &id);
		
		if (0 == status) {
			zrtp_test_channel_start(id);
		}
	}
	
	while (1) {
		zrtp_sleep(1000);
	}
	
	
	do_quit();
	
	return 0;
}
