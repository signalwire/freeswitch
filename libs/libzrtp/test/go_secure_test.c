/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 *
 * Viktor Krykun <v.krikun at zfoneproject.com>
 */

#include "engine_helpers.c"

static void setup() {
	zrtp_status_t s;

	zrtp_test_endpoint_cfg_t endpoint_cfg;
	zrtp_test_endpoint_config_defaults(&endpoint_cfg);

	s = zrtp_test_endpoint_create(&endpoint_cfg, "Alice", &g_alice);
	assert_int_equal(zrtp_status_ok, s);
	assert_int_not_equal(ZRTP_TEST_UNKNOWN_ID, g_alice);

	s = zrtp_test_endpoint_create(&endpoint_cfg, "Bob", &g_bob);
	assert_int_equal(zrtp_status_ok, s);
	assert_int_not_equal(ZRTP_TEST_UNKNOWN_ID, g_bob);
}

static void teardown() {
	zrtp_test_endpoint_destroy(g_alice);
	zrtp_test_endpoint_destroy(g_bob);
}


static void go_secure_test() {
	/*
	 * Create two test sessions, one for Alice and one for Bob and link them
	 * into test secure channel
	 */
	prepare_alice_bob();
	start_alice_bob_and_wait4secure();
	release_alice_bob();
}

static void go_secure_flags_test() {
	zrtp_status_t s;
	zrtp_test_session_info_t alice_ses_info;

	prepare_alice_bob();

	start_alice_bob_and_wait4secure();

	/* All flags should be clear */
	s = zrtp_test_session_get(g_alice_sid, &alice_ses_info);
	assert_int_equal(zrtp_status_ok, s);

	assert_int_equal(0, alice_ses_info.zrtp.matches_flags);
	assert_int_equal(0, alice_ses_info.zrtp.cached_flags);
	assert_int_equal(0, alice_ses_info.zrtp.wrongs_flags);

	/*
	 * Now let's make one more call, RS1 should match and cached
	 */
	release_alice_bob();

	prepare_alice_bob();

	start_alice_bob_and_wait4secure();

	s = zrtp_test_session_get(g_alice_sid, &alice_ses_info);
	assert_int_equal(zrtp_status_ok, s);

	assert_int_equal((int)ZRTP_BIT_RS1, alice_ses_info.zrtp.matches_flags);
	assert_int_equal((int)ZRTP_BIT_RS1, alice_ses_info.zrtp.cached_flags);
	assert_int_equal(0, alice_ses_info.zrtp.wrongs_flags);

	/*
	 * And one more time.. both RS1 and RS2 should be cached and should match.
	 */
	release_alice_bob();

	prepare_alice_bob();

	start_alice_bob_and_wait4secure();

	s = zrtp_test_session_get(g_alice_sid, &alice_ses_info);
	assert_int_equal(zrtp_status_ok, s);

	assert_int_equal((int)(ZRTP_BIT_RS1 | ZRTP_BIT_RS2) , alice_ses_info.zrtp.matches_flags);
	assert_int_equal((int)(ZRTP_BIT_RS1 | ZRTP_BIT_RS2), alice_ses_info.zrtp.cached_flags);
	assert_int_equal(0, alice_ses_info.zrtp.wrongs_flags);
}

int main(void) {
	const UnitTest tests[] = {
		unit_test_setup_teardown(go_secure_test, setup, teardown),
		unit_test_setup_teardown(go_secure_flags_test, setup, teardown),
  	};

	return run_tests(tests);
}
