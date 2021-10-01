/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2021, Anthony Minessale II <anthm@freeswitch.org>
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
 * test_amr.c -- tests mod_amr
 *
 */

#ifndef AMR_PASSTHROUGH
#include <switch.h>
#include <stdlib.h>

#include <test/switch_test.h>
FST_CORE_BEGIN(".")
{
	FST_SUITE_BEGIN(test_amr)
	{
		FST_SETUP_BEGIN()
		{
			fst_requires_module("mod_loopback");
			fst_requires_module("mod_amr");
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_TEST_BEGIN(amr_decode) 
		{
			switch_codec_t read_codec = { 0 };
			switch_status_t status;
			switch_codec_settings_t codec_settings = {{ 0 }};
			uint32_t flags = 0;
			uint32_t rate;
			/*amr frame types*/
			static char no_data[] = "\x77\xc0";
			static char fail[] = "\x76\xc0";
			/*decode*/
			uint32_t decoded_len;
			unsigned char decbuf[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };
			switch_stream_handle_t stream = { 0 };

			status = switch_core_codec_init(&read_codec,
			"AMR",
			"mod_amr",
			NULL,
			8000,
			20,
			1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
			&codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			SWITCH_STANDARD_STREAM(stream);

			switch_api_execute("amr_debug", "on", NULL, &stream);

			switch_safe_free(stream.data);

			/*NO DATA = 0xf*/
			status = switch_core_codec_decode(&read_codec, NULL, &no_data, 2, 8000, &decbuf, &decoded_len, &rate, &flags);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			/*Invalid frame type*/
			status = switch_core_codec_decode(&read_codec, NULL, &fail, 2, 8000, &decbuf, &decoded_len, &rate, &flags);
			fst_check(status != SWITCH_STATUS_SUCCESS);

			switch_core_codec_destroy(&read_codec);
		}

		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()
#endif 
