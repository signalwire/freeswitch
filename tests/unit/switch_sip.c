#include <switch.h>
#include <test/switch_test.h>

int timeout_sec = 10;
switch_interval_time_t delay_start_ms = 5000;

FST_CORE_DB_BEGIN("./conf_sip")
{
FST_SUITE_BEGIN(switch_sip)
{
	FST_SETUP_BEGIN()
	{
		/* Give mod_sofia time to spinup profile threads */
		if (delay_start_ms) {
			switch_sleep(delay_start_ms * 1000);
			delay_start_ms = 0;
		}

		fst_requires_module("mod_sofia");
		fst_requires_module("mod_hash");
	}
	FST_SETUP_END()

	FST_TEARDOWN_BEGIN()
	{
	}
	FST_TEARDOWN_END()

	FST_TEST_BEGIN(identity_compact_check)
	{
		switch_core_session_t *session = NULL;
		switch_call_cause_t cause;
		const char *data = "eyJhbGciOiJFUzI1NiIsInBwdCI6InNoYWtlbiI;info=<https://cert.sticr.att.net:8443/certs/att/a937bb15-38b9-45f1-aac0-8cd3f8fe0648>";
		char *originate_str = switch_mprintf("{sip_h_Identity=%s}sofia/gateway/test_gateway/+15553332901", data);

		switch_ivr_originate(NULL, &session, &cause, originate_str, timeout_sec, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
		switch_safe_free(originate_str);
		fst_requires(session);

		if (session) {
			switch_channel_t *channel = switch_core_session_get_channel(session);
			const char *uuid = switch_core_session_get_uuid(session);

			fst_requires(channel);
			if (uuid) {
				switch_stream_handle_t stream = { 0 };
				SWITCH_STANDARD_STREAM(stream);

				switch_api_execute("hash", "select/realm/identity_check", NULL, &stream);
				fst_check_string_equals(stream.data, data);
				switch_safe_free(stream.data);

				SWITCH_STANDARD_STREAM(stream);
				switch_api_execute("hash", "delete/realm/identity_check", NULL, &stream);
				switch_safe_free(stream.data);
			}

			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			switch_core_session_rwunlock(session);
		}

	}
	FST_TEST_END()

	FST_TEST_BEGIN(identity_full_check)
	{
		switch_core_session_t *session = NULL;
		switch_call_cause_t cause;
		const char *data = "eyJhbGciOiJFUzI1NiIsInBwdCI6InNoYWtlbiI;info=<https://cert.sticr.att.net:8443/certs/att/a937bb15-38b9-45f1-aac0-8cd3f8fe0648>;alg=ES256;ppt=shaken";
		char *originate_str = switch_mprintf("{sip_h_Identity=%s}sofia/gateway/test_gateway/+15553332901", data);

		switch_ivr_originate(NULL, &session, &cause, originate_str, timeout_sec, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
		switch_safe_free(originate_str);
		fst_requires(session);

		if (session) {
			switch_channel_t *channel = switch_core_session_get_channel(session);
			const char *uuid = switch_core_session_get_uuid(session);

			fst_requires(channel);
			if (uuid) {
				switch_stream_handle_t stream = { 0 };
				SWITCH_STANDARD_STREAM(stream);

				switch_api_execute("hash", "select/realm/identity_check", NULL, &stream);
				fst_check_string_equals(stream.data, data);
				switch_safe_free(stream.data);

				SWITCH_STANDARD_STREAM(stream);
				switch_api_execute("hash", "delete/realm/identity_check", NULL, &stream);
				switch_safe_free(stream.data);
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

