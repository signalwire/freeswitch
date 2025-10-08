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

		FST_TEST_BEGIN(test_mod_opus_switch_status_noop)
		{
			signed char outbuf[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };
			uint32_t decoded_len = 0;
			uint32_t decoded_rate = 48000;
			unsigned int flags = 0;
			switch_codec_t orig_codec = { 0 };
			switch_status_t status;
			switch_codec_settings_t codec_settings = { { 0 } };
			status = switch_core_codec_init(&orig_codec,
											"OPUS",
											"mod_opus",
											NULL,
											48000,
											20,
											1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
											&codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			status = switch_core_codec_decode(&orig_codec, NULL, "test", 5, 48000, outbuf, &decoded_len, &decoded_rate, &flags);
			// On ENGDESK-42254 we have change SWITCH_STATUS_FALSE to SWITCH_STATUS_NOOP to fix customer issue, so this test should check SWITCH_STATUS_NOOP
			fst_check_int_equals(status, SWITCH_STATUS_NOOP);
			switch_core_codec_destroy(&orig_codec);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_l24_codec_init)
		{
			switch_codec_t codec = { 0 };
			switch_status_t status;
			switch_codec_settings_t codec_settings = {{ 0 }};

			/* Test L24 at 48000 Hz */
			status = switch_core_codec_init(&codec,
											"L24",
											NULL,
											NULL,
											48000,
											20,
											1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
											&codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			fst_check(codec.implementation != NULL);
			fst_check(codec.implementation->ianacode == 96);
			fst_check(codec.implementation->actual_samples_per_second == 48000);
			fst_check(codec.implementation->decoded_bytes_per_packet == 1920); /* 960 samples * 2 bytes (16-bit internal) */
			fst_check(codec.implementation->encoded_bytes_per_packet == 2880); /* 960 samples * 3 bytes (24-bit wire) */
			switch_core_codec_destroy(&codec);

			/* Test L24 at 8000 Hz */
			status = switch_core_codec_init(&codec,
											"L24",
											NULL,
											NULL,
											8000,
											20,
											1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
											&codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			fst_check(codec.implementation->actual_samples_per_second == 8000);
			fst_check(codec.implementation->decoded_bytes_per_packet == 320); /* 160 samples * 2 bytes */
			fst_check(codec.implementation->encoded_bytes_per_packet == 480); /* 160 samples * 3 bytes */
			switch_core_codec_destroy(&codec);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_l24_encode_decode)
		{
			switch_codec_t codec = { 0 };
			switch_status_t status;
			switch_codec_settings_t codec_settings = {{ 0 }};
			int16_t input_samples[960] = { 0 };
			uint8_t encoded_data[2880] = { 0 };
			int16_t decoded_samples[960] = { 0 };
			uint32_t encoded_len = sizeof(encoded_data);
			uint32_t decoded_len = sizeof(decoded_samples);
			uint32_t encoded_rate = 48000;
			uint32_t decoded_rate = 48000;
			unsigned int flag = 0;
			int i;

			/* Initialize codec */
			status = switch_core_codec_init(&codec,
											"L24",
											NULL,
											NULL,
											48000,
											20,
											1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
											&codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			/* Create test pattern: ascending 16-bit values */
			for (i = 0; i < 960; i++) {
				input_samples[i] = (int16_t)(i * 32);
			}

			/* Encode 16-bit samples to 24-bit */
			status = switch_core_codec_encode(&codec,
											  NULL,
											  input_samples,
											  1920, /* 960 samples * 2 bytes */
											  48000,
											  encoded_data,
											  &encoded_len,
											  &encoded_rate,
											  &flag);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			fst_check(encoded_len == 2880); /* 960 samples * 3 bytes */

			/* Verify 24-bit encoding (big-endian format) */
			/* First sample: input_samples[0] = 0x0000 -> 24-bit = 0x000000 */
			fst_check(encoded_data[0] == 0x00);
			fst_check(encoded_data[1] == 0x00);
			fst_check(encoded_data[2] == 0x00);

			/* Second sample: input_samples[1] = 0x0020 (32) -> 24-bit = 0x002000 */
			fst_check(encoded_data[3] == 0x00);
			fst_check(encoded_data[4] == 0x20);
			fst_check(encoded_data[5] == 0x00);

			/* Decode 24-bit samples back to 16-bit */
			status = switch_core_codec_decode(&codec,
											  NULL,
											  encoded_data,
											  encoded_len,
											  48000,
											  decoded_samples,
											  &decoded_len,
											  &decoded_rate,
											  &flag);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			fst_check(decoded_len == 1920); /* 960 samples * 2 bytes */

			/* Verify decoded samples match input */
			for (i = 0; i < 960; i++) {
				fst_check(decoded_samples[i] == input_samples[i]);
			}

			switch_core_codec_destroy(&codec);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_l24_negative_samples)
		{
			switch_codec_t codec = { 0 };
			switch_status_t status;
			switch_codec_settings_t codec_settings = {{ 0 }};
			int16_t input_samples[160] = { 0 };
			uint8_t encoded_data[480] = { 0 };
			int16_t decoded_samples[160] = { 0 };
			uint32_t encoded_len = sizeof(encoded_data);
			uint32_t decoded_len = sizeof(decoded_samples);
			uint32_t encoded_rate = 8000;
			uint32_t decoded_rate = 8000;
			unsigned int flag = 0;

			/* Initialize codec at 8kHz */
			status = switch_core_codec_init(&codec,
											"L24",
											NULL,
											NULL,
											8000,
											20,
											1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
											&codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			/* Test negative values and edge cases */
			input_samples[0] = 0x7FFF;   /* Max positive */
			input_samples[1] = -32768;   /* Min negative (0x8000) */
			input_samples[2] = -1;       /* 0xFFFF */
			input_samples[3] = 1000;
			input_samples[4] = -1000;

			/* Encode */
			status = switch_core_codec_encode(&codec,
											  NULL,
											  input_samples,
											  320,
											  8000,
											  encoded_data,
											  &encoded_len,
											  &encoded_rate,
											  &flag);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			/* Verify sign extension in 24-bit format */
			/* Max positive: 0x7FFF -> 0x7FFF00 */
			fst_check(encoded_data[0] == 0x7F);
			fst_check(encoded_data[1] == 0xFF);
			fst_check(encoded_data[2] == 0x00);

			/* Min negative: 0x8000 -> 0x800000 */
			fst_check(encoded_data[3] == 0x80);
			fst_check(encoded_data[4] == 0x00);
			fst_check(encoded_data[5] == 0x00);

			/* Decode */
			status = switch_core_codec_decode(&codec,
											  NULL,
											  encoded_data,
											  encoded_len,
											  8000,
											  decoded_samples,
											  &decoded_len,
											  &decoded_rate,
											  &flag);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			/* Verify decoded values */
			fst_check(decoded_samples[0] == 0x7FFF);
			fst_check(decoded_samples[1] == -32768);
			fst_check(decoded_samples[2] == -1);
			fst_check(decoded_samples[3] == 1000);
			fst_check(decoded_samples[4] == -1000);

			switch_core_codec_destroy(&codec);
		}
		FST_TEST_END()

	}
	FST_SUITE_END()
}
FST_CORE_END()
