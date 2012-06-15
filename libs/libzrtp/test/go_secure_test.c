/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 *
 * Viktor Krykun <v.krikun at zfoneproject.com>
 */

#include <setjmp.h>		/*chmockery dependency*/
#include <stdio.h>		/*chmockery dependency*/
#include <unistd.h> 	/*for usleep*/

#include "cmockery/cmockery.h"
#include "test_engine.h"

static zrtp_test_id_t g_alice, g_bob;
static zrtp_test_id_t g_alice_sid, g_bob_sid;
static zrtp_test_id_t g_secure_audio_channel;

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

static void prepare_alice_bob() {
	zrtp_status_t s;

	//zrtp_test_channel_info_t channel_info;
	zrtp_test_session_cfg_t session_config;
	zrtp_test_session_config_defaults(&session_config);

	/*
	 * Create two test sessions, one for Alice and one for Bob and link them
	 * into test secure channel
	 */
	s = zrtp_test_session_create(g_alice, &session_config, &g_alice_sid);
	assert_int_equal(zrtp_status_ok, s);
	assert_int_not_equal(ZRTP_TEST_UNKNOWN_ID, g_alice_sid);

	s = zrtp_test_session_create(g_bob, &session_config, &g_bob_sid);
	assert_int_equal(zrtp_status_ok, s);
	assert_int_not_equal(ZRTP_TEST_UNKNOWN_ID, g_bob_sid);

	s = zrtp_test_channel_create2(g_alice_sid, g_bob_sid, 0, &g_secure_audio_channel);
	assert_int_equal(zrtp_status_ok, s);
	assert_int_not_equal(ZRTP_TEST_UNKNOWN_ID, g_secure_audio_channel);
}

static void release_alice_bob() {
	zrtp_test_session_destroy(g_alice_sid);
	zrtp_test_session_destroy(g_bob_sid);

	zrtp_test_channel_destroy(g_secure_audio_channel);
}

static void start_alice_bob_and_wait4secure() {
	zrtp_status_t s;
	zrtp_test_channel_info_t channel_info;

	/* Everything is ready. Let's start the stream and give it few seconds to switch secure. */
	s = zrtp_test_channel_start(g_secure_audio_channel);
	assert_int_equal(zrtp_status_ok, s);

	unsigned i = 30;
	for (; i>0; i--) {
		usleep(100*1000);
	}

	s = zrtp_test_channel_get(g_secure_audio_channel, &channel_info);
	assert_int_equal(zrtp_status_ok, s);

	assert_true(channel_info.is_secure);
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
