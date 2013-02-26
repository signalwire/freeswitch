/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include <setjmp.h>
#include <stdio.h>

#include "zrtp.h"
#include "cmockery/cmockery.h"

zrtp_global_t *zrtp;

void setup() {
	zrtp_status_t s;
	zrtp_config_t zrtp_config;
	
	zrtp_config_defaults(&zrtp_config);
	
	s = zrtp_init(&zrtp_config, &zrtp);
	assert_int_equal(s, zrtp_status_ok);
}

void teardown() {
	zrtp_down(zrtp);
}

static void aes128_ctr_test() {
	zrtp_cipher_t *cipher = zrtp_comp_find(ZRTP_CC_CIPHER, ZRTP_CIPHER_AES128, zrtp);
	assert_non_null(cipher);
	cipher->self_test(cipher, ZRTP_CIPHER_MODE_CTR);
}

static void aes128_cfb_test() {
	zrtp_cipher_t *cipher = zrtp_comp_find(ZRTP_CC_CIPHER, ZRTP_CIPHER_AES128, zrtp);
	assert_non_null(cipher);
	cipher->self_test(cipher, ZRTP_CIPHER_MODE_CFB);
}

static void aes256_ctr_test() {
	zrtp_cipher_t *cipher = zrtp_comp_find(ZRTP_CC_CIPHER, ZRTP_CIPHER_AES256, zrtp);
	assert_non_null(cipher);
	cipher->self_test(cipher, ZRTP_CIPHER_MODE_CTR);
}

static void aes256_cfb_test() {
	zrtp_cipher_t *cipher = zrtp_comp_find(ZRTP_CC_CIPHER, ZRTP_CIPHER_AES256, zrtp);
	assert_non_null(cipher);
	cipher->self_test(cipher, ZRTP_CIPHER_MODE_CFB);
}

int main(void) {
	const UnitTest tests[] = {
		unit_test_setup_teardown(aes128_ctr_test, setup, teardown),
		unit_test_setup_teardown(aes128_cfb_test, setup, teardown),
		unit_test_setup_teardown(aes256_ctr_test, setup, teardown),
		unit_test_setup_teardown(aes256_cfb_test, setup, teardown),
  	};

	return run_tests(tests);
}
