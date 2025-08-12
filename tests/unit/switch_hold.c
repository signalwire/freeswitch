#include <switch.h>
#include <test/switch_test.h>

int timeout_sec = 10;
switch_interval_time_t delay_start_ms = 5000;

FST_CORE_DB_BEGIN("./conf_test")
{
FST_SUITE_BEGIN(switch_hold)
{
	FST_SETUP_BEGIN()
	{
		/* Give mod_sofia time to spinup profile threads */
		if (delay_start_ms) {
			switch_sleep(delay_start_ms * 1000);
			delay_start_ms = 0;
		}

		fst_requires_module("mod_sofia");
		fst_requires_module("mod_commands");
	}
	FST_SETUP_END()

	FST_TEARDOWN_BEGIN()
	{
	}
	FST_TEARDOWN_END()

	FST_TEST_BEGIN(hold_unhold_restriction)
	{
		switch_core_session_t *session = NULL;
		switch_status_t status;
		switch_call_cause_t cause;

		status = switch_ivr_originate(NULL, &session, &cause, "{ignore_early_media=true}sofia/gateway/test_gateway/+15553332900", timeout_sec, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
		fst_requires(session);
		fst_check(status == SWITCH_STATUS_SUCCESS);

		if (session) {
			const char *uuid = switch_core_session_get_uuid(session);
			switch_channel_t *channel = NULL;

			channel = switch_core_session_get_channel(session);
			fst_requires(channel);

			if (uuid) {
				char *off_uuid = switch_mprintf("off %s", uuid);
				char *toggle_uuid = switch_mprintf("toggle %s", uuid);

				switch_stream_handle_t stream = { 0 };
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "firing the api.\n");

				SWITCH_STANDARD_STREAM(stream);
				switch_api_execute("uuid_hold", off_uuid, NULL, &stream);
				fst_check_string_equals(stream.data, "-ERR Operation failed\n");
				switch_safe_free(stream.data);
				switch_sleep(200000);

				SWITCH_STANDARD_STREAM(stream);
				switch_api_execute("uuid_hold", uuid, NULL, &stream);
				fst_check_string_equals(stream.data, "+OK Success\n");
				switch_safe_free(stream.data);
				switch_sleep(200000);

				SWITCH_STANDARD_STREAM(stream);
				switch_api_execute("uuid_hold", uuid, NULL, &stream);
				fst_check_string_equals(stream.data, "-ERR Operation failed\n");
				switch_safe_free(stream.data);
				switch_sleep(200000);

				SWITCH_STANDARD_STREAM(stream);
				switch_api_execute("uuid_hold", uuid, NULL, &stream);
				fst_check_string_equals(stream.data, "-ERR Operation failed\n");
				switch_safe_free(stream.data);
				switch_sleep(200000);

				SWITCH_STANDARD_STREAM(stream);
				switch_api_execute("uuid_hold", toggle_uuid, NULL, &stream);
				fst_check_string_equals(stream.data, "+OK Success\n");
				switch_safe_free(stream.data);
				switch_sleep(200000);

				SWITCH_STANDARD_STREAM(stream);
				switch_api_execute("uuid_hold", off_uuid, NULL, &stream);
				fst_check_string_equals(stream.data, "-ERR Operation failed\n");
				switch_safe_free(stream.data);
				switch_sleep(200000);

				SWITCH_STANDARD_STREAM(stream);
				switch_api_execute("uuid_hold", toggle_uuid, NULL, &stream);
				fst_check_string_equals(stream.data, "+OK Success\n");
				switch_safe_free(stream.data);
				switch_sleep(200000);

				SWITCH_STANDARD_STREAM(stream);
				switch_api_execute("uuid_hold", uuid, NULL, &stream);
				fst_check_string_equals(stream.data, "-ERR Operation failed\n");
				switch_safe_free(stream.data);
				switch_sleep(200000);

				switch_safe_free(off_uuid);
				switch_safe_free(toggle_uuid);
			}
			
			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			switch_core_session_rwunlock(session);
		}
	}
	FST_TEST_END()
}
FST_SUITE_END()
}
FST_CORE_END()

