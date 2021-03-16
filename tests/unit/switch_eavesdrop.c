#include <switch.h>
#include <test/switch_test.h>

static switch_status_t test_detect_long_tone_in_file(const char *filepath, int rate, int freq, int ptime) {
	teletone_multi_tone_t mt;
	teletone_tone_map_t map;
	int16_t data[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };
	switch_size_t len = (rate * ptime / 1000) /*packet len in samples */ * 8; /*length of chunk that must contain tone*/
	switch_size_t fin = 0;
	switch_status_t status;
	switch_file_handle_t fh = { 0 };
	uint8_t fail = 0, gaps = 0, audio = 0;

	status = switch_core_file_open(&fh, filepath, 1, rate, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL);
	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot open file [%s]\n", filepath);
		return SWITCH_STATUS_FALSE;
	} 

	mt.sample_rate = rate;
	map.freqs[0] = (teletone_process_t)freq;

	teletone_multi_tone_init(&mt, &map);

	len = (rate * 2 / 100) /*packet len in samples */ * 8;

	while (switch_core_file_read(&fh, &data, &len) == SWITCH_STATUS_SUCCESS) {
		fin += len;
		/*skip silence at the beginning of the file, 1 second max. */
		if (!teletone_multi_tone_detect(&mt, data, len)) {
			if ((fin > rate && !audio) || gaps > 30) { 
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Too many gaps in audio or no tone detected 1st second. [%" SWITCH_SIZE_T_FMT "][%d]\n", fin, gaps);
				fail = 1;
				break;
			}
			gaps++;
			continue;
		} else {
			audio++;
		}
	}

	switch_core_file_close(&fh);

	if (fail) {
		return SWITCH_STATUS_FALSE;
	}
	return SWITCH_STATUS_SUCCESS;
}

FST_CORE_BEGIN("./conf_eavesdrop")

{
FST_SUITE_BEGIN(switch_eavesdrop)
{
	FST_SETUP_BEGIN()
	{
		fst_requires_module("mod_loopback");
		fst_requires_module("mod_sofia");
		switch_core_set_variable("link_ip", switch_core_get_variable("local_ip_v4"));
	}
	FST_SETUP_END()

	FST_TEARDOWN_BEGIN()
	{
	}
	FST_TEARDOWN_END()

	FST_TEST_BEGIN(test_eavesdrop_bridged_same_ptime_20ms)
	{
		switch_core_session_t *session1 = NULL;
		switch_core_session_t *session2 = NULL;
		switch_core_session_t *session3 = NULL;

		switch_channel_t *channel1 = NULL;
		switch_channel_t *channel2 = NULL;
		switch_channel_t *channel3 = NULL;

		switch_status_t status;
		switch_call_cause_t cause;
		switch_stream_handle_t stream = { 0 };
		char eavesdrop_command[256] = { 0 };
		char rec_path[1024];
		char rec_uuid[SWITCH_UUID_FORMATTED_LENGTH + 1] = { 0 };
		char eaves_dialstr[512] = { 0 };

		switch_uuid_str(rec_uuid, sizeof(rec_uuid));

		/*parked 20 ms ptime */
		status = switch_ivr_originate(NULL, &session1, &cause, "{ignore_early_media=true}sofia/gateway/eavestest/+15553332220", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
		fst_requires(session1);
		fst_check(status == SWITCH_STATUS_SUCCESS);
		channel1 = switch_core_session_get_channel(session1);
		fst_requires(channel1);

		snprintf(eaves_dialstr, sizeof(eaves_dialstr), "{ignore_early_media=true}{sip_h_X-UnitTestRecfile=%s}sofia/gateway/eavestest/+15553332230", rec_uuid);

		/*eavesdropper 20 ms ptime*/
		status = switch_ivr_originate(NULL, &session2, &cause, eaves_dialstr, 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
		fst_requires(session2);
		fst_check(status == SWITCH_STATUS_SUCCESS);
		channel2 = switch_core_session_get_channel(session2);
		fst_requires(channel2);

		/*milliwatt tone 20 ms ptime*/
		status = switch_ivr_originate(NULL, &session3, &cause, "{ignore_early_media=true}sofia/gateway/eavestest/+15553332226", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
		fst_requires(session3);
		fst_check(status == SWITCH_STATUS_SUCCESS);
		channel3 = switch_core_session_get_channel(session3);
		fst_requires(channel3);

		SWITCH_STANDARD_STREAM(stream);
		switch_snprintf(eavesdrop_command, sizeof(eavesdrop_command), "uuid_bridge %s %s", switch_core_session_get_uuid(session1), switch_core_session_get_uuid(session2));
		switch_api_execute("bgapi", eavesdrop_command, session1, &stream);
		memset(eavesdrop_command, 0, sizeof(eavesdrop_command));
		switch_snprintf(eavesdrop_command, sizeof(eavesdrop_command),"uuid_setvar_multi %s eavesdrop_enable_dtmf=false;eavesdrop_whisper_bleg=true;eavesdrop_whisper_aleg=false", switch_core_session_get_uuid(session3));
		switch_api_execute("bgapi", eavesdrop_command, session3, &stream);
		memset(eavesdrop_command, 0, sizeof(eavesdrop_command));
		switch_snprintf(eavesdrop_command, sizeof(eavesdrop_command), "uuid_transfer %s 'eavesdrop:%s' inline", switch_core_session_get_uuid(session3), switch_core_session_get_uuid(session2)); 
		switch_api_execute("bgapi", eavesdrop_command, session3, &stream);
		switch_safe_free(stream.data);

		sleep(5); // it will record ~ 5 secs

		snprintf(rec_path, sizeof(rec_path), "/tmp/eaves-%s.wav", rec_uuid);

		fst_requires(switch_file_exists(rec_path, fst_pool) == SWITCH_STATUS_SUCCESS);

		fst_requires(test_detect_long_tone_in_file(rec_path, 8000, 300, 20) == SWITCH_STATUS_SUCCESS);

		unlink(rec_path);

		switch_channel_hangup(channel1, SWITCH_CAUSE_NORMAL_CLEARING);
		switch_channel_hangup(channel2, SWITCH_CAUSE_NORMAL_CLEARING);
		switch_channel_hangup(channel3, SWITCH_CAUSE_NORMAL_CLEARING);

		switch_core_session_rwunlock(session1);
		switch_core_session_rwunlock(session2);
		switch_core_session_rwunlock(session3);

	}
	FST_TEST_END()

	FST_TEST_BEGIN(test_eavesdrop_bridged_ptime_mismatch_20ms_30ms)
	{
		switch_core_session_t *session1 = NULL;
		switch_core_session_t *session2 = NULL;
		switch_core_session_t *session3 = NULL;

		switch_channel_t *channel1 = NULL;
		switch_channel_t *channel2 = NULL;
		switch_channel_t *channel3 = NULL;

		switch_status_t status;
		switch_call_cause_t cause;
		switch_stream_handle_t stream = { 0 };
		char eavesdrop_command[256] = { 0 };
		char rec_path[1024];
		char rec_uuid[SWITCH_UUID_FORMATTED_LENGTH + 1] = { 0 };
		char eaves_dialstr[512] = { 0 };

		switch_uuid_str(rec_uuid, sizeof(rec_uuid));

		/*parked 20 ms ptime */
		status = switch_ivr_originate(NULL, &session1, &cause, "{ignore_early_media=true}sofia/gateway/eavestest/+15553332220", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
		fst_requires(session1);
		fst_check(status == SWITCH_STATUS_SUCCESS);
		channel1 = switch_core_session_get_channel(session1);
		fst_requires(channel1);

		snprintf(eaves_dialstr, sizeof(eaves_dialstr), "{ignore_early_media=true}{sip_h_X-UnitTestRecfile=%s}sofia/gateway/eavestest/+15553332230", rec_uuid);

		/*eavesdropper 20 ms ptime*/
		status = switch_ivr_originate(NULL, &session2, &cause, eaves_dialstr, 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
		fst_requires(session2);
		fst_check(status == SWITCH_STATUS_SUCCESS);
		channel2 = switch_core_session_get_channel(session2);
		fst_requires(channel2);

		/*milliwatt tone 30 ms ptime*/
		status = switch_ivr_originate(NULL, &session3, &cause, "{ignore_early_media=true}sofia/gateway/eavestest/+15553332222", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
		fst_requires(session3);
		fst_check(status == SWITCH_STATUS_SUCCESS);
		channel3 = switch_core_session_get_channel(session3);
		fst_requires(channel3);

		SWITCH_STANDARD_STREAM(stream);
		switch_snprintf(eavesdrop_command, sizeof(eavesdrop_command), "uuid_bridge %s %s", switch_core_session_get_uuid(session1), switch_core_session_get_uuid(session2));
		switch_api_execute("bgapi", eavesdrop_command, session1, &stream);
		memset(eavesdrop_command, 0, sizeof(eavesdrop_command));
		switch_snprintf(eavesdrop_command, sizeof(eavesdrop_command),"uuid_setvar_multi %s eavesdrop_enable_dtmf=false;eavesdrop_whisper_bleg=true;eavesdrop_whisper_aleg=false", switch_core_session_get_uuid(session3));
		switch_api_execute("bgapi", eavesdrop_command, session3, &stream);
		memset(eavesdrop_command, 0, sizeof(eavesdrop_command));
		switch_snprintf(eavesdrop_command, sizeof(eavesdrop_command), "uuid_transfer %s 'eavesdrop:%s' inline", switch_core_session_get_uuid(session3), switch_core_session_get_uuid(session2)); 
		switch_api_execute("bgapi", eavesdrop_command, session3, &stream);
		switch_safe_free(stream.data);

		sleep(5); // it will record ~ 5 secs

		snprintf(rec_path, sizeof(rec_path), "/tmp/eaves-%s.wav", rec_uuid);

		fst_requires(switch_file_exists(rec_path, fst_pool) == SWITCH_STATUS_SUCCESS);

		fst_requires(test_detect_long_tone_in_file(rec_path, 8000, 300, 20) == SWITCH_STATUS_SUCCESS);

		unlink(rec_path);

		switch_channel_hangup(channel1, SWITCH_CAUSE_NORMAL_CLEARING);
		switch_channel_hangup(channel2, SWITCH_CAUSE_NORMAL_CLEARING);
		switch_channel_hangup(channel3, SWITCH_CAUSE_NORMAL_CLEARING);

		switch_core_session_rwunlock(session1);
		switch_core_session_rwunlock(session2);
		switch_core_session_rwunlock(session3);

	}
	FST_TEST_END()

	FST_TEST_BEGIN(test_eavesdrop_bridged_ptime_mismatch_30ms_20ms) 
	{
		switch_core_session_t *session1 = NULL;
		switch_core_session_t *session2 = NULL;
		switch_core_session_t *session3 = NULL;

		switch_channel_t *channel1 = NULL;
		switch_channel_t *channel2 = NULL;
		switch_channel_t *channel3 = NULL;

		switch_status_t status;
		switch_call_cause_t cause;
		switch_stream_handle_t stream = { 0 };
		char eavesdrop_command[256] = { 0 };
		char rec_path[1024];
		char rec_uuid[SWITCH_UUID_FORMATTED_LENGTH + 1] = { 0 };
		char eaves_dialstr[512] = { 0 };

		switch_uuid_str(rec_uuid, sizeof(rec_uuid));

		/*parked 30 ms ptime */
		status = switch_ivr_originate(NULL, &session1, &cause, "{ignore_early_media=true}sofia/gateway/eavestest/+15553332231", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
		fst_requires(session1);
		fst_check(status == SWITCH_STATUS_SUCCESS);
		channel1 = switch_core_session_get_channel(session1);
		fst_requires(channel1);

		snprintf(eaves_dialstr, sizeof(eaves_dialstr), "{ignore_early_media=true}{sip_h_X-UnitTestRecfile=%s}sofia/gateway/eavestest/+15553332240", rec_uuid);

		/*eavesdropper 30 ms ptime*/
		status = switch_ivr_originate(NULL, &session2, &cause, eaves_dialstr, 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
		fst_requires(session2);
		fst_check(status == SWITCH_STATUS_SUCCESS);
		channel2 = switch_core_session_get_channel(session2);
		fst_requires(channel2);

		/*milliwatt tone 20 ms ptime*/
		status = switch_ivr_originate(NULL, &session3, &cause, "{ignore_early_media=true}sofia/gateway/eavestest/+15553332226", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
		fst_requires(session3);
		fst_check(status == SWITCH_STATUS_SUCCESS);
		channel3 = switch_core_session_get_channel(session3);
		fst_requires(channel3);

		SWITCH_STANDARD_STREAM(stream);
		switch_snprintf(eavesdrop_command, sizeof(eavesdrop_command), "uuid_bridge %s %s", switch_core_session_get_uuid(session1), switch_core_session_get_uuid(session2));
		switch_api_execute("bgapi", eavesdrop_command, session1, &stream);
		memset(eavesdrop_command, 0, sizeof(eavesdrop_command));
		switch_snprintf(eavesdrop_command, sizeof(eavesdrop_command),"uuid_setvar_multi %s eavesdrop_enable_dtmf=false;eavesdrop_whisper_bleg=true;eavesdrop_whisper_aleg=false", switch_core_session_get_uuid(session3));
		switch_api_execute("bgapi", eavesdrop_command, session3, &stream);
		memset(eavesdrop_command, 0, sizeof(eavesdrop_command));
		switch_snprintf(eavesdrop_command, sizeof(eavesdrop_command), "uuid_transfer %s 'eavesdrop:%s' inline", switch_core_session_get_uuid(session3), switch_core_session_get_uuid(session2)); 
		switch_api_execute("bgapi", eavesdrop_command, session3, &stream);
		switch_safe_free(stream.data);

		sleep(5); // it will record ~ 5 secs

		snprintf(rec_path, sizeof(rec_path), "/tmp/eaves-%s.wav", rec_uuid);

		fst_requires(switch_file_exists(rec_path, fst_pool) == SWITCH_STATUS_SUCCESS);

		fst_requires(test_detect_long_tone_in_file(rec_path, 8000, 300, 30) == SWITCH_STATUS_SUCCESS);

		unlink(rec_path);

		switch_channel_hangup(channel1, SWITCH_CAUSE_NORMAL_CLEARING);
		switch_channel_hangup(channel2, SWITCH_CAUSE_NORMAL_CLEARING);
		switch_channel_hangup(channel3, SWITCH_CAUSE_NORMAL_CLEARING);

		switch_core_session_rwunlock(session1);
		switch_core_session_rwunlock(session2);
		switch_core_session_rwunlock(session3);

	}
	FST_TEST_END()

	FST_TEST_BEGIN(test_eavesdrop_bridged_ptime_mismatch_reneg) 
	{
		switch_core_session_t *session1 = NULL;
		switch_core_session_t *session2 = NULL;
		switch_core_session_t *session3 = NULL;

		switch_channel_t *channel1 = NULL;
		switch_channel_t *channel2 = NULL;
		switch_channel_t *channel3 = NULL;

		switch_status_t status;
		switch_call_cause_t cause;
		switch_stream_handle_t stream = { 0 };
		char eavesdrop_command[256] = { 0 };
		char rec_path[1024];
		char rec_uuid[SWITCH_UUID_FORMATTED_LENGTH + 1] = { 0 };
		char eaves_dialstr[512] = { 0 };

		switch_uuid_str(rec_uuid, sizeof(rec_uuid));

		/*parked 30 ms ptime */
		status = switch_ivr_originate(NULL, &session1, &cause, "{ignore_early_media=true}sofia/gateway/eavestest/+15553332231", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
		fst_requires(session1);
		fst_check(status == SWITCH_STATUS_SUCCESS);
		channel1 = switch_core_session_get_channel(session1);
		fst_requires(channel1);

		snprintf(eaves_dialstr, sizeof(eaves_dialstr), "{ignore_early_media=true}{sip_h_X-UnitTestRecfile=%s}sofia/gateway/eavestest/+15553332240", rec_uuid);

		/*eavesdropper 30 ms ptime*/
		status = switch_ivr_originate(NULL, &session2, &cause, eaves_dialstr, 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
		fst_requires(session2);
		fst_check(status == SWITCH_STATUS_SUCCESS);
		channel2 = switch_core_session_get_channel(session2);
		fst_requires(channel2);

		/*milliwatt tone 20 ms ptime*/
		status = switch_ivr_originate(NULL, &session3, &cause, "{ignore_early_media=true}sofia/gateway/eavestest/+15553332226", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
		fst_requires(session3);
		fst_check(status == SWITCH_STATUS_SUCCESS);
		channel3 = switch_core_session_get_channel(session3);
		fst_requires(channel3);

		SWITCH_STANDARD_STREAM(stream);
		switch_snprintf(eavesdrop_command, sizeof(eavesdrop_command), "uuid_bridge %s %s", switch_core_session_get_uuid(session1), switch_core_session_get_uuid(session2));
		switch_api_execute("bgapi", eavesdrop_command, session1, &stream);
		memset(eavesdrop_command, 0, sizeof(eavesdrop_command));
		switch_snprintf(eavesdrop_command, sizeof(eavesdrop_command),"uuid_setvar_multi %s eavesdrop_enable_dtmf=false;eavesdrop_whisper_bleg=true;eavesdrop_whisper_aleg=false", switch_core_session_get_uuid(session3));
		switch_api_execute("bgapi", eavesdrop_command, session3, &stream);
		memset(eavesdrop_command, 0, sizeof(eavesdrop_command));
		switch_snprintf(eavesdrop_command, sizeof(eavesdrop_command), "uuid_transfer %s 'eavesdrop:%s' inline", switch_core_session_get_uuid(session3), switch_core_session_get_uuid(session2)); 
		switch_api_execute("bgapi", eavesdrop_command, session3, &stream);

		sleep(2); 

		// codec reneg for eavesdropper
		memset(eavesdrop_command, 0, sizeof(eavesdrop_command));
		switch_snprintf(eavesdrop_command, sizeof(eavesdrop_command), "uuid_media_reneg %s = PCMU@20i", switch_core_session_get_uuid(session2)); 
		switch_api_execute("bgapi", eavesdrop_command, session3, &stream);

		sleep(1);

		// codec reneg for eavesdroppee
		memset(eavesdrop_command, 0, sizeof(eavesdrop_command));
		switch_snprintf(eavesdrop_command, sizeof(eavesdrop_command), "uuid_media_reneg %s = PCMU@30i", switch_core_session_get_uuid(session3)); 
		switch_api_execute("bgapi", eavesdrop_command, session3, &stream);
		switch_safe_free(stream.data);
		
		sleep(2);

		snprintf(rec_path, sizeof(rec_path), "/tmp/eaves-%s.wav", rec_uuid);

		fst_requires(switch_file_exists(rec_path, fst_pool) == SWITCH_STATUS_SUCCESS);

		fst_requires(test_detect_long_tone_in_file(rec_path, 8000, 300, 30) == SWITCH_STATUS_SUCCESS);

		unlink(rec_path);

		switch_channel_hangup(channel1, SWITCH_CAUSE_NORMAL_CLEARING);
		switch_channel_hangup(channel2, SWITCH_CAUSE_NORMAL_CLEARING);
		switch_channel_hangup(channel3, SWITCH_CAUSE_NORMAL_CLEARING);

		switch_core_session_rwunlock(session1);
		switch_core_session_rwunlock(session2);
		switch_core_session_rwunlock(session3);

	}
	FST_TEST_END()

}
FST_SUITE_END()
}
FST_CORE_END()

