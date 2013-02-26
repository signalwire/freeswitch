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

	zrtp_test_channel_info_t a2pbx_channel_info, b2pbx_channel_info;
	zrtp_test_session_cfg_t session_config, session_config_enroll;
	zrtp_test_session_config_defaults(&session_config);
	zrtp_test_session_config_defaults(&session_config_enroll);

	session_config_enroll.is_enrollment = 1;

	/**************************************************************************
	 * Enroll both Alice and Bob to PBX
	 */
	prepare_alice_pbx_bob_setup(&session_config, &session_config, &session_config_enroll, &session_config_enroll);

	/* Everything is ready. Let's start the stream and give it few seconds to switch secure. */
	s = zrtp_test_channel_start(g_alice2pbx_channel);
	assert_int_equal(zrtp_status_ok, s);
	s = zrtp_test_channel_start(g_bob2pbx_channel);
	assert_int_equal(zrtp_status_ok, s);

	int i = 30;
	for (; i>0; i--) {
		usleep(100*1000);
	}

	s = zrtp_test_channel_get(g_alice2pbx_channel, &a2pbx_channel_info);
	assert_int_equal(zrtp_status_ok, s);
	s = zrtp_test_channel_get(g_bob2pbx_channel, &b2pbx_channel_info);
	assert_int_equal(zrtp_status_ok, s);

	/* Both, Alice and Bob should switch secure and ready for enrollment */
	assert_true(a2pbx_channel_info.is_secure);
	assert_true(b2pbx_channel_info.is_secure);

	/* Confirm enrollment for both, Alice and Bob */
	zrtp_test_id_t alice2pbx_stream = zrtp_test_session_get_stream_by_idx(g_alice_sid, 0);
	zrtp_test_id_t bob2pbx_stream = zrtp_test_session_get_stream_by_idx(g_bob_sid, 0);

	s = zrtp_register_with_trusted_mitm(zrtp_stream_for_test_stream(alice2pbx_stream));
	assert_int_equal(zrtp_status_ok, s);
	s = zrtp_register_with_trusted_mitm(zrtp_stream_for_test_stream(bob2pbx_stream));
	assert_int_equal(zrtp_status_ok, s);

	/* Clean-up */
	cleanup_alice_pbx_bob_setup();

	/**************************************************************************
	 * Now, when we have two enrolled parties, make one more call and initiate
	 * SAS Relay at the PBX side. Both endpoints should received SASRelay, but
	 * just one should get ZRTP_EVENT_LOCAL_SAS_UPDATED event.
	 */

	prepare_alice_pbx_bob_setup(&session_config, &session_config, &session_config, &session_config);

	/* Everything is ready. Let's start the stream and give it few seconds to switch secure. */
	s = zrtp_test_channel_start(g_alice2pbx_channel);
	assert_int_equal(zrtp_status_ok, s);
	s = zrtp_test_channel_start(g_bob2pbx_channel);
	assert_int_equal(zrtp_status_ok, s);

	i = 30;
	for (; i>0; i--) {
		usleep(100*1000);
	}

	s = zrtp_test_channel_get(g_alice2pbx_channel, &a2pbx_channel_info);
	assert_int_equal(zrtp_status_ok, s);
	s = zrtp_test_channel_get(g_bob2pbx_channel, &b2pbx_channel_info);
	assert_int_equal(zrtp_status_ok, s);

	/* Both, Alice and Bob should switch secure */
	assert_true(a2pbx_channel_info.is_secure);
	assert_true(b2pbx_channel_info.is_secure);

	zrtp_test_id_t pbx2alice_stream = zrtp_test_session_get_stream_by_idx(g_pbxa_sid, 0);
	zrtp_test_id_t pbx2bob_stream = zrtp_test_session_get_stream_by_idx(g_pbxb_sid, 0);
	alice2pbx_stream = zrtp_test_session_get_stream_by_idx(g_alice_sid, 0);
	bob2pbx_stream = zrtp_test_session_get_stream_by_idx(g_bob_sid, 0);

	/* Resolve MiTM call! */
	s = zrtp_resolve_mitm_call(zrtp_stream_for_test_stream(pbx2alice_stream),
							   zrtp_stream_for_test_stream(pbx2bob_stream));

	i = 20;
	for (; i>0; i--) {
		usleep(100*1000);
	}

	/* Alice and Bob should receive Enrollment notification */
	unsigned sas_update1 = zrtp_stream_did_event_receive(alice2pbx_stream, ZRTP_EVENT_LOCAL_SAS_UPDATED);
	unsigned sas_update2 = zrtp_stream_did_event_receive(bob2pbx_stream, ZRTP_EVENT_LOCAL_SAS_UPDATED);
	assert_true(sas_update1 ^ sas_update2);

	/* Clean-up */
	cleanup_alice_pbx_bob_setup();
}

int main(void) {
	const UnitTest tests[] = {
		unit_test_setup_teardown(enrollment_test, pbx_setup, pbx_teardown),
  	};

	return run_tests(tests);
}
