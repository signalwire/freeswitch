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

extern zrtp_dk_ctx *zrtp_dk_init(zrtp_cipher_t *cipher, zrtp_stringn_t *key, zrtp_stringn_t *salt);
extern zrtp_status_t zrtp_derive_key(zrtp_dk_ctx *ctx, zrtp_srtp_prf_label label, zrtp_stringn_t *result_key);
extern void zrtp_dk_deinit(zrtp_dk_ctx *ctx);

static uint8_t dk_master_key[16] = {
	0xE1, 0xF9, 0x7A, 0x0D, 0x3E, 0x01, 0x8B, 0xE0,
	0xD6, 0x4F, 0xA3, 0x2C, 0x06, 0xDE, 0x41, 0x39
};

static uint8_t dk_master_salt[14] = {
	0x0E, 0xC6, 0x75, 0xAD, 0x49, 0x8A, 0xFE, 0xEB,
	0xB6, 0x96, 0x0B, 0x3A, 0xAB, 0xE6
};


static uint8_t dk_cipher_key[16] = {
	0xC6, 0x1E, 0x7A, 0x93, 0x74, 0x4F, 0x39, 0xEE,
	0x10, 0x73, 0x4A, 0xFE, 0x3F, 0xF7, 0xA0, 0x87
};

static uint8_t dk_cipher_salt[14] = {
	0x30, 0xCB, 0xBC, 0x08, 0x86, 0x3D, 0x8C, 0x85,
	0xD4, 0x9D, 0xB3, 0x4A, 0x9A, 0xE1
};

static uint8_t dk_auth_key[94] = {
	0xCE, 0xBE, 0x32, 0x1F, 0x6F, 0xF7, 0x71, 0x6B,
	0x6F, 0xD4, 0xAB, 0x49, 0xAF, 0x25, 0x6A, 0x15,
	0x6D, 0x38, 0xBA, 0xA4, 0x8F, 0x0A, 0x0A, 0xCF,
	0x3C, 0x34, 0xE2, 0x35, 0x9E, 0x6C, 0xDB, 0xCE,
	0xE0, 0x49, 0x64, 0x6C, 0x43, 0xD9, 0x32, 0x7A,
	0xD1, 0x75, 0x57, 0x8E, 0xF7, 0x22, 0x70, 0x98,
	0x63, 0x71, 0xC1, 0x0C, 0x9A, 0x36, 0x9A, 0xC2,
	0xF9, 0x4A, 0x8C, 0x5F, 0xBC, 0xDD, 0xDC, 0x25,
	0x6D, 0x6E, 0x91, 0x9A, 0x48, 0xB6, 0x10, 0xEF,
	0x17, 0xC2, 0x04, 0x1E, 0x47, 0x40, 0x35, 0x76,
	0x6B, 0x68, 0x64, 0x2C, 0x59, 0xBB, 0xFC, 0x2F,
	0x34, 0xDB, 0x60, 0xDB, 0xDF, 0xB2
};


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

zrtp_status_t hex_cmp(uint8_t *a, uint8_t *b, uint32_t len)
{
	uint32_t i;
	zrtp_status_t res = zrtp_status_ok;
	for (i = 0; i<len; i++) {
		if (a[i] != b[i]) {
			res = zrtp_status_fail;
			break;
		}
	}
	return res;
}

static void dk_test() {
	
	zrtp_status_t res;
	zrtp_string16_t master_key, master_salt, cipher_key, cipher_salt;
	zrtp_string128_t auth_key;
	zrtp_dk_ctx *ctx;

	zrtp_cipher_t *cipher = zrtp_comp_find(ZRTP_CC_CIPHER, ZRTP_CIPHER_AES128, zrtp);
	assert_non_null(cipher);
	
	master_key.length = master_key.max_length = 16;
	zrtp_memcpy(master_key.buffer, dk_master_key, 16);
	
	master_salt.length = 14;
	master_salt.max_length = 16;
	zrtp_memcpy(master_salt.buffer, dk_master_salt, 14);
	

	ctx = zrtp_dk_init(cipher, (zrtp_stringn_t*)&master_key, (zrtp_stringn_t*)&master_salt);
	assert_non_null(ctx);

	cipher_key.length = 16;
	cipher_key.max_length = 16;

	zrtp_derive_key(ctx, label_rtp_encryption, (zrtp_stringn_t*)&cipher_key);
	res = hex_cmp((uint8_t*)cipher_key.buffer, dk_cipher_key, cipher_key.length);
	assert_int_equal(res, zrtp_status_ok);
	

	cipher_salt.length = 14;
	cipher_salt.max_length = 16;

	zrtp_derive_key(ctx, label_rtp_salt, (zrtp_stringn_t*)&cipher_salt);
	res = hex_cmp((uint8_t*)cipher_salt.buffer, dk_cipher_salt, cipher_salt.length);
	assert_int_equal(res, zrtp_status_ok);
	
	
	auth_key.length = 94;
	auth_key.max_length = 128;
	
	zrtp_derive_key(ctx, label_rtp_msg_auth, (zrtp_stringn_t*)&auth_key);
	res = hex_cmp((uint8_t*)auth_key.buffer, dk_auth_key, auth_key.length);
	assert_int_equal(res, zrtp_status_ok);
	
	zrtp_dk_deinit(ctx);
}


int main(void) {
	const UnitTest tests[] = {
		unit_test_setup_teardown(dk_test, setup, teardown),
  	};

	return run_tests(tests);
}
