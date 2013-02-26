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


static void zrtp_hash_export_import_sunny_test() {
	zrtp_status_t s;
	char alice_zrtp_hash[ZRTP_SIGN_ZRTP_HASH_LENGTH];
	char bob_zrtp_hash[ZRTP_SIGN_ZRTP_HASH_LENGTH];
	zrtp_stream_t *alice_zrtp_stream, *bob_zrtp_stream;

	/* Create two test sessions, one for Alice and one for Bob and link them into test secure channel */
	prepare_alice_bob();

	alice_zrtp_stream = zrtp_stream_for_test_stream(zrtp_test_session_get_stream_by_idx(g_alice_sid, 0));
	bob_zrtp_stream = zrtp_stream_for_test_stream(zrtp_test_session_get_stream_by_idx(g_bob_sid, 0));
	assert_non_null(alice_zrtp_stream); assert_non_null(bob_zrtp_stream);

	/* Take Alice's hash and give it to Bob */
	s = zrtp_signaling_hash_get(alice_zrtp_stream, alice_zrtp_hash, sizeof(alice_zrtp_hash));
	assert_int_equal(zrtp_status_ok, s);

	s = zrtp_signaling_hash_set(bob_zrtp_stream, alice_zrtp_hash, ZRTP_SIGN_ZRTP_HASH_LENGTH);
	assert_int_equal(zrtp_status_ok, s);

	/* Take Bob's hash and give it to Alice */
	s = zrtp_signaling_hash_get(bob_zrtp_stream, bob_zrtp_hash, sizeof(bob_zrtp_hash));
	assert_int_equal(zrtp_status_ok, s);

	s = zrtp_signaling_hash_set(alice_zrtp_stream, bob_zrtp_hash, ZRTP_SIGN_ZRTP_HASH_LENGTH);
	assert_int_equal(zrtp_status_ok, s);

	/* Start and wait for Secure */
	start_alice_bob_and_wait4secure();

	/* Check if ZRTP_EVENT_WRONG_SIGNALING_HASH was not triggered for any of test endpoints */
	assert_false(zrtp_stream_did_event_receive(zrtp_test_session_get_stream_by_idx(g_alice_sid, 0),
			ZRTP_EVENT_WRONG_SIGNALING_HASH));

	assert_false(zrtp_stream_did_event_receive(zrtp_test_session_get_stream_by_idx(g_bob_sid, 0),
				ZRTP_EVENT_WRONG_SIGNALING_HASH));

	/* Release test setup */
	release_alice_bob();
}

static void zrtp_hash_import_wrong_test() {
	zrtp_status_t s;
	char wrong_alice_zrtp_hash[ZRTP_SIGN_ZRTP_HASH_LENGTH];
	zrtp_stream_t *bob_zrtp_stream;

	/* Create two test sessions, one for Alice and one for Bob and link them into test secure channel */
	prepare_alice_bob();

	bob_zrtp_stream = zrtp_stream_for_test_stream(zrtp_test_session_get_stream_by_idx(g_bob_sid, 0));
	assert_non_null(bob_zrtp_stream);

	/* Let's provide wrong hash to bob */
	zrtp_memset(wrong_alice_zrtp_hash, 6, ZRTP_SIGN_ZRTP_HASH_LENGTH);

	s = zrtp_signaling_hash_set(bob_zrtp_stream, wrong_alice_zrtp_hash, ZRTP_SIGN_ZRTP_HASH_LENGTH);
	assert_int_equal(zrtp_status_ok, s);

	/* Start and wait for Secure */
	start_alice_bob_and_wait4secure();

	/* Check if Alice don't receive ZRTP_EVENT_WRONG_SIGNALING_HASH, but Bob should get one */
	assert_false(zrtp_stream_did_event_receive(zrtp_test_session_get_stream_by_idx(g_alice_sid, 0),
			ZRTP_EVENT_WRONG_SIGNALING_HASH));

	assert_true(zrtp_stream_did_event_receive(zrtp_test_session_get_stream_by_idx(g_bob_sid, 0),
				ZRTP_EVENT_WRONG_SIGNALING_HASH));

	/* Release test setup */
	release_alice_bob();
}


int main(void) {
	const UnitTest tests[] = {
		unit_test_setup_teardown(zrtp_hash_export_import_sunny_test, setup, teardown),
		unit_test_setup_teardown(zrtp_hash_import_wrong_test, setup, teardown),
  	};

	return run_tests(tests);
}
