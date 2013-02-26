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


static void session_init_fails_with_no_dh2k() {
	zrtp_profile_t profile;
	zrtp_status_t s;

	zrtp_session_t *new_session;

	/* Let's initialize ZRTP session with default profile first */
	zrtp_profile_defaults(&profile, zrtp);

	new_session = NULL;
	s = zrtp_session_init(zrtp,
			&profile,
			ZRTP_SIGNALING_ROLE_INITIATOR,
			&new_session);

	assert_int_equal(zrtp_status_ok, s);
	assert_non_null(new_session);

	/* Then disable DH2K and leave just mandatory parameters  */
	profile.pk_schemes[0] = ZRTP_PKTYPE_DH3072;
	profile.pk_schemes[1] = ZRTP_PKTYPE_MULT;
	profile.pk_schemes[2] = 0;

	new_session = NULL;
	s = zrtp_session_init(zrtp,
			&profile,
			ZRTP_SIGNALING_ROLE_INITIATOR,
			&new_session);

	assert_int_equal(zrtp_status_ok, s);
	assert_non_null(new_session);

	/* Let's try to disable Multi key exchange, it should produce an error. */
	profile.pk_schemes[0] = ZRTP_PKTYPE_DH3072;
	profile.pk_schemes[1] = 0;

	new_session = NULL;
	s = zrtp_session_init(zrtp,
			&profile,
			ZRTP_SIGNALING_ROLE_INITIATOR,
			&new_session);

	assert_int_not_equal(zrtp_status_ok, s);
	assert_null(new_session);

	/* Profile checking with one of mandatory components missing should return error too. */
	s = zrtp_profile_check(&profile, zrtp);
	assert_int_not_equal(zrtp_status_ok, s);

	/* NOTE: we ignore memory leaks and don't destroy ZRTP sessions to make test sources cleaner */
}


int main(void) {
	const UnitTest tests[] = {
		unit_test_setup_teardown(session_init_fails_with_no_dh2k, setup, teardown),
  	};

	return run_tests(tests);
}
