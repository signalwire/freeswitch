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
 * Chris Rienzo <chris@signalwire.com>
 * Seven Du <dujinfang@gmail.com>
 * Dragos Oancea <dragos@signalwire.com>
 *
 * switch_core_codec.c -- tests codec core functions
 *
 */
#include <switch.h>
#include <stdlib.h>

#include <test/switch_test.h>

FST_CORE_BEGIN("./conf")
{
	FST_SUITE_BEGIN(switch_core_codec)
	{
		FST_SETUP_BEGIN()
		{
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_SETUP_BEGIN()
		{
			fst_requires_module("mod_opus");
			fst_requires_module("mod_spandsp");
		}
		FST_SETUP_END()

		FST_TEST_BEGIN(test_switch_core_codec_copy)
		{
			switch_codec_t orig_codec = { 0 };
			switch_codec_t new_codec = { 0 };
			switch_status_t status;
			switch_codec_settings_t codec_settings = {{ 0 }};
			status = switch_core_codec_init(&orig_codec,
			"OPUS",
			"mod_opus",
			NULL,
			48000,
			20,
			1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
			&codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			switch_core_codec_copy(&orig_codec, &new_codec, NULL, NULL);
			fst_check(orig_codec.implementation->samples_per_second == new_codec.implementation->samples_per_second);
			fst_check(orig_codec.implementation->actual_samples_per_second == new_codec.implementation->actual_samples_per_second);
			switch_core_codec_destroy(&orig_codec);
			switch_core_codec_destroy(&new_codec);

			status = switch_core_codec_init(&orig_codec,
			"OPUS",
			"mod_opus",
			NULL,
			16000,
			20,
			1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
			&codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			switch_core_codec_copy(&orig_codec, &new_codec, NULL, NULL);
			fst_check(orig_codec.implementation->samples_per_second == new_codec.implementation->samples_per_second);
			fst_check(orig_codec.implementation->actual_samples_per_second == new_codec.implementation->actual_samples_per_second);
			switch_core_codec_destroy(&orig_codec);
			switch_core_codec_destroy(&new_codec);

			status = switch_core_codec_init(&orig_codec,
			"OPUS",
			"mod_opus",
			NULL,
			8000,
			20,
			1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
			&codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			switch_core_codec_copy(&orig_codec, &new_codec, NULL, NULL);
			fst_check(orig_codec.implementation->samples_per_second == new_codec.implementation->samples_per_second);
			fst_check(orig_codec.implementation->actual_samples_per_second == new_codec.implementation->actual_samples_per_second);
			switch_core_codec_destroy(&orig_codec);
			switch_core_codec_destroy(&new_codec);
 
			status = switch_core_codec_init(&orig_codec,
			"G722",
			"mod_spandsp",
			NULL,
			8000,
			20,
			1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
			&codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			switch_core_codec_copy(&orig_codec, &new_codec, NULL, NULL);
			fst_check(orig_codec.implementation->samples_per_second == new_codec.implementation->samples_per_second);
			fst_check(orig_codec.implementation->actual_samples_per_second == new_codec.implementation->actual_samples_per_second);
			switch_core_codec_destroy(&orig_codec);
			switch_core_codec_destroy(&new_codec);

		}
		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()
