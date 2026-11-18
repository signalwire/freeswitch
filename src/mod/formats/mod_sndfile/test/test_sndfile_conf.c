/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2018, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Dragos Oancea <dragos@signalwire.com>
 *
 * test_sndfile_conf.c -- tests mod_sndfile config
 *
 */
#include <switch.h>
#include <stdlib.h>

#include <test/switch_test.h>

char *extensions_will_fail[] = {  // not allowed through conf file.
	"ul", "gsm", "vox", "ogg"
};

char *extensions_will_succeed[] = {  // allowed through conf file.
	"wav", "raw", "r8", "r16"
};

FST_CORE_BEGIN("test_conf")
{
	FST_SUITE_BEGIN(test_sndfile)
	{
		FST_SETUP_BEGIN()
		{
			fst_requires_module("mod_loopback");
			fst_requires_module("mod_sndfile");
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_TEST_BEGIN(sndfile_exten_not_allowed)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			char *recording;
			int i, exlen, timeout_sec = 2;
			switch_stream_handle_t stream = { 0 };
			  
			SWITCH_STANDARD_STREAM(stream);

			switch_api_execute("sndfile_debug", "on", session, &stream);

			switch_safe_free(stream.data);

			exlen = (sizeof(extensions_will_fail) / sizeof(extensions_will_fail[0]));

			status = switch_ivr_originate(NULL, &session, &cause, "null/+15553334444", timeout_sec, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
			fst_requires(session);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			for (i = 0; i < exlen; i++) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Testing media file extension: [%s]\n", extensions_will_fail[i]);

				recording = switch_mprintf("/tmp/%s.%s", switch_core_session_get_uuid(session), extensions_will_fail[i]);
				status = switch_ivr_record_session(session, recording, 3000, NULL);
				fst_check(status == SWITCH_STATUS_GENERR);
				if (status == SWITCH_STATUS_SUCCESS) {
					// not expected
					unlink(recording);
				}

				switch_safe_free(recording);
			}

			channel = switch_core_session_get_channel(session);
			fst_requires(channel);

			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			fst_check(!switch_channel_ready(channel));

			switch_core_session_rwunlock(session);

			switch_sleep(1000000);
		}
		FST_TEST_END()
		FST_TEST_BEGIN(sndfile_exten_allowed)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			char *recording;
			int i, exlen, timeout_sec = 2;
			switch_stream_handle_t stream = { 0 };
			  
			SWITCH_STANDARD_STREAM(stream);

			switch_api_execute("sndfile_debug", "on", session, &stream);

			switch_safe_free(stream.data);

			exlen = (sizeof(extensions_will_succeed) / sizeof(extensions_will_succeed[0]));

			status = switch_ivr_originate(NULL, &session, &cause, "null/+15553334444", timeout_sec, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
			fst_requires(session);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			for (i = 0; i < exlen; i++) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Testing media file extension: [%s]\n", extensions_will_succeed[i]);

				recording = switch_mprintf("/tmp/%s.%s", switch_core_session_get_uuid(session), extensions_will_succeed[i]);
				status = switch_ivr_record_session(session, recording, 3000, NULL);
				fst_check(status == SWITCH_STATUS_SUCCESS);

				unlink(recording);

				switch_safe_free(recording);
			}

			channel = switch_core_session_get_channel(session);
			fst_requires(channel);

			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			fst_check(!switch_channel_ready(channel));

			switch_core_session_rwunlock(session);

			switch_sleep(1000000);
		}
		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()
