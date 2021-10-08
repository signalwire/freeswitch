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
 *
 * test_opusfile.c -- tests mod_opusfile
 *
 */
#include <switch.h>
#include <stdlib.h>

#include <test/switch_test.h>


FST_CORE_BEGIN(".")
{
	FST_SUITE_BEGIN(test_opusfile)
	{
		FST_SETUP_BEGIN()
		{
			fst_requires_module("mod_loopback");
			fst_requires_module("mod_opusfile");
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_TEST_BEGIN(opusfile_read)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			/*
			$ mediainfo hi.opus
			General
			Complete name                            : hi.opus
			Format                                   : OGG
			File size                                : 8.55 KiB
			Duration                                 : 2s 157ms
			Overall bit rate                         : 32.5 Kbps
			Writing application                      : opusenc from opus-tools 0.1.10

			Audio
			ID                                       : 277454932 (0x1089A054)
			Format                                   : Opus
			Duration                                 : 2s 157ms
			Channel(s)                               : 1 channel
			Channel positions                        : Front: C
			Sampling rate                            : 16.0 KHz
			Compression mode                         : Lossy
			Writing library                          : libopus 1.2~alpha2
			*/
			static char filename[] = "sounds/hi.opus"; // duration in samples: 103200
			char path[4096];
			switch_file_handle_t fh = { 0 };
			int16_t *audiobuf;
			switch_size_t len; 

			sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, filename);

			status = switch_ivr_originate(NULL, &session, &cause, "null/+15553334444", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
			fst_requires(session);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			status = switch_core_file_open(&fh, path, 1, 48000, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			len = 128 * 1024 * sizeof(*audiobuf);
			switch_zmalloc(audiobuf, len);

			status = switch_core_file_read(&fh, audiobuf, &len);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			/* [INFO] mod_opusfile.c:292 [OGG/OPUS File] Duration (samples): 103200 */
			/* compare the read sample count with the one in the OGG/OPUS header. */
			fst_check(len == 103200);

			status = switch_core_file_close(&fh);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			switch_safe_free(audiobuf);
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
