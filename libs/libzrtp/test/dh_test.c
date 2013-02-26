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

static void dh2k_test() {
	zrtp_pk_scheme_t *pks = zrtp_comp_find(ZRTP_CC_PKT, ZRTP_PKTYPE_DH2048, zrtp);
	assert_non_null(pks);
	pks->self_test(pks);
}

static void dh3k_test() {
	zrtp_pk_scheme_t *pks = zrtp_comp_find(ZRTP_CC_PKT, ZRTP_PKTYPE_DH3072, zrtp);
	assert_non_null(pks);
	pks->self_test(pks);
}


int main(void) {
	const UnitTest tests[] = {
		unit_test_setup_teardown(dh2k_test, setup, teardown),
		unit_test_setup_teardown(dh3k_test, setup, teardown),
  	};

	return run_tests(tests);
}
