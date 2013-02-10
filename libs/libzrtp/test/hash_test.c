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

static void sha1_hash_test() {
	zrtp_hash_t *hash =  zrtp_comp_find(ZRTP_CC_HASH, ZRTP_SRTP_HASH_HMAC_SHA1, zrtp);
	assert_non_null(hash);
	hash->hash_self_test(hash);
}

static void sha1_hmac_test() {
	zrtp_hash_t *hash =  zrtp_comp_find(ZRTP_CC_HASH, ZRTP_SRTP_HASH_HMAC_SHA1, zrtp);
	assert_non_null(hash);
	hash->hmac_self_test(hash);
}

static void sha256_hash_test() {
	zrtp_hash_t *hash =  zrtp_comp_find(ZRTP_CC_HASH, ZRTP_HASH_SHA256, zrtp);
	assert_non_null(hash);
	hash->hash_self_test(hash);
}

static void sha256_hmac_test() {
	zrtp_hash_t *hash =  zrtp_comp_find(ZRTP_CC_HASH, ZRTP_HASH_SHA256, zrtp);
	assert_non_null(hash);
	hash->hmac_self_test(hash);
}

static void sha384_hash_test() {
	zrtp_hash_t *hash =  zrtp_comp_find(ZRTP_CC_HASH, ZRTP_HASH_SHA384, zrtp);
	assert_non_null(hash);
	hash->hash_self_test(hash);
}

static void sha384_hmac_test() {
	zrtp_hash_t *hash =  zrtp_comp_find(ZRTP_CC_HASH, ZRTP_HASH_SHA384, zrtp);
	assert_non_null(hash);
	hash->hmac_self_test(hash);
}


int main(void) {
	const UnitTest tests[] = {
		unit_test_setup_teardown(sha1_hash_test, setup, teardown),
		unit_test_setup_teardown(sha1_hmac_test, setup, teardown),
		unit_test_setup_teardown(sha256_hash_test, setup, teardown),
		unit_test_setup_teardown(sha256_hmac_test, setup, teardown),
		unit_test_setup_teardown(sha384_hash_test, setup, teardown),
		unit_test_setup_teardown(sha384_hmac_test, setup, teardown),
  	};

	return run_tests(tests);
}
