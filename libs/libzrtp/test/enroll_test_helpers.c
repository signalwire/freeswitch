
static zrtp_test_id_t g_alice, g_bob, g_pbx;
static zrtp_test_id_t g_alice_sid, g_bob_sid, g_pbxa_sid, g_pbxb_sid;
static zrtp_test_id_t g_alice2pbx_channel, g_bob2pbx_channel;

static void pbx_setup() {
	zrtp_status_t s;

	zrtp_test_endpoint_cfg_t endpoint_cfg;
	zrtp_test_endpoint_config_defaults(&endpoint_cfg);

	s = zrtp_test_endpoint_create(&endpoint_cfg, "Alice", &g_alice);
	assert_int_equal(zrtp_status_ok, s);
	assert_int_not_equal(ZRTP_TEST_UNKNOWN_ID, g_alice);

	s = zrtp_test_endpoint_create(&endpoint_cfg, "Bob", &g_bob);
	assert_int_equal(zrtp_status_ok, s);
	assert_int_not_equal(ZRTP_TEST_UNKNOWN_ID, g_bob);

	endpoint_cfg.zrtp.is_mitm = 1;
	s = zrtp_test_endpoint_create(&endpoint_cfg, "PBX", &g_pbx);
	assert_int_equal(zrtp_status_ok, s);
	assert_int_not_equal(ZRTP_TEST_UNKNOWN_ID, g_pbx);
}

static void pbx_teardown() {
	zrtp_test_endpoint_destroy(g_alice);
	zrtp_test_endpoint_destroy(g_bob);
	zrtp_test_endpoint_destroy(g_pbx);
}


static void prepare_alice_pbx_bob_setup(zrtp_test_session_cfg_t *alice_sconfig,
										zrtp_test_session_cfg_t *bob_sconfig,
										zrtp_test_session_cfg_t *pbxa_sconfig,
										zrtp_test_session_cfg_t *pbxb_sconfig) {
	zrtp_status_t s;

	if (alice_sconfig) {
		assert_non_null(pbxa_sconfig);

		s = zrtp_test_session_create(g_alice, alice_sconfig, &g_alice_sid);
		assert_int_equal(zrtp_status_ok, s);
		assert_int_not_equal(ZRTP_TEST_UNKNOWN_ID, g_alice_sid);

		s = zrtp_test_session_create(g_pbx, pbxa_sconfig, &g_pbxa_sid);
		assert_int_equal(zrtp_status_ok, s);
		assert_int_not_equal(ZRTP_TEST_UNKNOWN_ID, g_pbxa_sid);

		s = zrtp_test_channel_create2(g_alice_sid, g_pbxa_sid, 0, &g_alice2pbx_channel);
		assert_int_equal(zrtp_status_ok, s);
		assert_int_not_equal(ZRTP_TEST_UNKNOWN_ID, g_alice2pbx_channel);
	}

	if (bob_sconfig) {
		assert_non_null(pbxb_sconfig);

		s = zrtp_test_session_create(g_bob, bob_sconfig, &g_bob_sid);
		assert_int_equal(zrtp_status_ok, s);
		assert_int_not_equal(ZRTP_TEST_UNKNOWN_ID, g_bob_sid);

		s = zrtp_test_session_create(g_pbx,  pbxb_sconfig, &g_pbxb_sid);
		assert_int_equal(zrtp_status_ok, s);
		assert_int_not_equal(ZRTP_TEST_UNKNOWN_ID, g_pbxb_sid);

		s = zrtp_test_channel_create2(g_bob_sid, g_pbxb_sid, 0, &g_bob2pbx_channel);
		assert_int_equal(zrtp_status_ok, s);
		assert_int_not_equal(ZRTP_TEST_UNKNOWN_ID, g_bob2pbx_channel);
	}
}

static void cleanup_alice_pbx_bob_setup() {
	zrtp_test_session_destroy(g_alice_sid);
	zrtp_test_session_destroy(g_bob_sid);
	zrtp_test_session_destroy(g_pbxa_sid);
	zrtp_test_session_destroy(g_pbxb_sid);

	zrtp_test_channel_destroy(g_alice2pbx_channel);
	zrtp_test_channel_destroy(g_bob2pbx_channel);
}

