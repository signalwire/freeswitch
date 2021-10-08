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
 * Willie Zhang <zhanghong905011@163.com>
 *
 * switch_core_asr.c -- tests file core functions
 *
 */
#include <switch.h>
#include <stdlib.h>

#include <test/switch_test.h>

FST_CORE_BEGIN("./conf")
{
	FST_SUITE_BEGIN(switch_core_asr)
	{
		FST_SETUP_BEGIN()
		{
			fst_requires_module("mod_test");
			fst_requires_module("mod_tone_stream");
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_TEST_BEGIN(core_asr_feed_resample)
		{
			uint8_t *buf;
			size_t len = 960;
			char input_filename[1024];
			const char* session_id = "123456";
			switch_asr_handle_t ah = { 0 };
			switch_file_handle_t file_handle = { 0 };
			switch_asr_flag_t flags = SWITCH_ASR_FLAG_NONE;
			char *grammar = switch_core_sprintf(fst_pool, "{start-input-timers=true,no-input-timeout=5000,speech-timeout=10000,channel-uuid=%s}default", session_id);
			fst_requires(fst_core > 1)
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Open recognizer\n");
			fst_requires(switch_core_asr_open(&ah, "test", "L16", 48000, "", &flags, fst_pool) == SWITCH_STATUS_SUCCESS);
			sprintf(input_filename, "%s", "silence_stream://100,0");
			file_handle.channels = 1;
			file_handle.native_rate = 48000;
			fst_requires(switch_core_asr_load_grammar(&ah, grammar, "") == SWITCH_STATUS_SUCCESS);
			fst_requires(switch_core_file_open(&file_handle, input_filename, file_handle.channels, 48000, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) == SWITCH_STATUS_SUCCESS);
			buf = (uint8_t *)switch_core_alloc(fst_pool, sizeof(uint8_t) * 960 * sizeof(uint16_t) * file_handle.channels);
			fst_requires(switch_core_file_read(&file_handle, buf, &len) == SWITCH_STATUS_SUCCESS);
			fst_requires(switch_core_asr_feed(&ah, buf, len * sizeof(int16_t), &flags) == SWITCH_STATUS_SUCCESS);
			fst_check(ah.resampler->to_len == 320);
			fst_requires(switch_core_file_close(&file_handle) == SWITCH_STATUS_SUCCESS);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Close recognizer\n");
			flags = SWITCH_ASR_FLAG_NONE;
			fst_requires(switch_core_asr_close(&ah, &flags) == SWITCH_STATUS_SUCCESS);
		}
		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()

