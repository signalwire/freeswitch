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

#include "enroll_test_helpers.c"

static void enrollment_test() {
	zrtp_status_t s;

	zrtp_test_channel_info_t a2pbx_channel_info;
	zrtp_test_session_cfg_t session_config, session_config_enroll;
	zrtp_test_session_config_defaults(&session_config);
	zrtp_test_session_config_defaults(&session_config_enroll);

	session_config_enroll.is_enrollment = 1;

	/**************************************************************************
	 * Enroll Alice to PBX and check triggered events.
	 */
	prepare_alice_pbx_bob_setup(&session_config, NULL, &session_config_enroll, NULL);

	/* Everything is ready. Let's start the stream and give it few seconds to switch secure. */
	s = zrtp_test_channel_start(g_alice2pbx_channel);
	assert_int_equal(zrtp_status_ok, s);

	int i = 30;
	for (; i>0; i--) {
		usleep(100*1000);
	}

	s = zrtp_test_channel_get(g_alice2pbx_channel, &a2pbx_channel_info);
	assert_int_equal(zrtp_status_ok, s);

	/* Both, Alice and PBX should switch secure */
	assert_true(a2pbx_channel_info.is_secure);

	/* Alice should receive Enrollment notification */
	zrtp_test_id_t alice2pbx_stream = zrtp_test_session_get_stream_by_idx(g_alice_sid, 0);
	assert_true(zrtp_stream_did_event_receive(alice2pbx_stream, ZRTP_EVENT_IS_CLIENT_ENROLLMENT));

	/* PBX streams should receive incoming enrollment notification */
	zrtp_test_id_t pbx2alice_stream = zrtp_test_session_get_stream_by_idx(g_pbxa_sid, 0);
	assert_true(zrtp_stream_did_event_receive(pbx2alice_stream, ZRTP_EVENT_NEW_USER_ENROLLED));

	/* Confirm enrollment at the PBX side */
	s = zrtp_register_with_trusted_mitm(zrtp_stream_for_test_stream(alice2pbx_stream));
	assert_int_equal(zrtp_status_ok, s);

	/* Clean-up */
	cleanup_alice_pbx_bob_setup();

	/**************************************************************************
	 * Try to make one more enrollment call. This time it should say "Already enrolled"
	 */
	prepare_alice_pbx_bob_setup(&session_config, NULL, &session_config_enroll, NULL);

	/* Everything is ready. Let's start the stream and give it few seconds to switch secure. */
	s = zrtp_test_channel_start(g_alice2pbx_channel);
	assert_int_equal(zrtp_status_ok, s);

	i = 30;
	for (; i>0; i--) {
		usleep(100*1000);
	}

	s = zrtp_test_channel_get(g_alice2pbx_channel, &a2pbx_channel_info);
	assert_int_equal(zrtp_status_ok, s);

	assert_true(a2pbx_channel_info.is_secure);

	/* Alice should receive Enrollment notification */
	alice2pbx_stream = zrtp_test_session_get_stream_by_idx(g_alice_sid, 0);
	assert_true(zrtp_stream_did_event_receive(alice2pbx_stream, ZRTP_EVENT_IS_CLIENT_ENROLLMENT));

	/* PBX streams should receive incoming enrollment notification */
	pbx2alice_stream = zrtp_test_session_get_stream_by_idx(g_pbxa_sid, 0);
	assert_true(zrtp_stream_did_event_receive(pbx2alice_stream, ZRTP_EVENT_USER_ALREADY_ENROLLED));

	// TODO: check if we have PBX secret cached
	// TODO: test zrtp_is_user_enrolled()
}

int main(void) {
	const UnitTest tests[] = {
		unit_test_setup_teardown(enrollment_test, pbx_setup, pbx_teardown),
  	};

	return run_tests(tests);
}
