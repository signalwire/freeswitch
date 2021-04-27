/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2020, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 *
 * switch_vad.c -- VAD tests
 *
 */
#include <switch.h>
#include <stdlib.h>

#include <test/switch_test.h>


static float next_tone_frame(int16_t *buf, unsigned int samples, float pos)
{
	// make sine wave of amplitude 7000 for 8000Hz sample rate VAD
	float step = 600.0 / 8000.0 * 2.0 * M_PI;
	unsigned int i;
	for (i = 0; i < samples; i++) {
		buf[i] = (int16_t)(7000.0 * sinf(pos));
		pos += step;
	}
	return pos;
}

static void next_silence_frame(int16_t *buf, unsigned int samples)
{
	unsigned int i;
	for (i = 0; i < samples; i++) {
		buf[i] = 0;
	}
}

FST_CORE_BEGIN("./conf")
{
	FST_SUITE_BEGIN(switch_vad)
	{
		FST_SETUP_BEGIN()
		{
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

#ifdef SWITCH_HAVE_FVAD
		FST_TEST_BEGIN(fvad_mode_0)
		{
			int16_t *buf = malloc(sizeof(int16_t) * 160);
			int duration;
			float pos = 0.0;
			int got_transition = 0;
			int res;
			switch_vad_state_t cur_state = SWITCH_VAD_STATE_NONE;

			switch_vad_t *vad = switch_vad_init(8000, 1);
			fst_requires(vad);
			res = switch_vad_set_mode(vad, 0); // tone is detected as speech in mode 0
			fst_requires(res == 0);
			switch_vad_set_param(vad, "silence_ms", 400);
			switch_vad_set_param(vad, "voice_ms", 80);
			switch_vad_set_param(vad, "debug", 10);

			// generate a tone and pump it into the vad
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Start 200 ms tone\n");
			duration = 200 / 20; // 200 ms
			while (--duration >= 0) {
				switch_vad_state_t new_state;
				pos = next_tone_frame(buf, 160, pos);
				new_state = switch_vad_process(vad, buf, 160);
				if (new_state != cur_state) got_transition++;
				cur_state = new_state;
			}
			fst_requires(got_transition == 2);
			fst_requires(cur_state == SWITCH_VAD_STATE_TALKING);

			// feed silence frames into the vad
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Start 1000 ms silence\n");
			duration = 1000 / 20; // 1000 ms
			got_transition = 0;
			next_silence_frame(buf, 160);
			while (--duration >= 0) {
				switch_vad_state_t new_state = switch_vad_process(vad, buf, 160);
				if (new_state != cur_state) got_transition++;
				cur_state = new_state;
			}
			fst_requires(got_transition == 2);
			fst_requires(cur_state == SWITCH_VAD_STATE_NONE);

			// generate a tone < voice_ms
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Start 40 ms tone\n");
			duration = 40 / 20; // 40 ms
			got_transition = 0;
			while (--duration >= 0) {
				switch_vad_state_t new_state;
				pos = next_tone_frame(buf, 160, pos);
				new_state = switch_vad_process(vad, buf, 160);
				if (new_state != cur_state) got_transition++;
				cur_state = new_state;
			}
			fst_requires(got_transition == 0);
			fst_requires(cur_state == SWITCH_VAD_STATE_NONE);

			// continue tone > voice_ms
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Continue with 60 ms tone\n");
			duration = 60 / 20; // 60 ms
			got_transition = 0;
			while (--duration >= 0) {
				switch_vad_state_t new_state;
				pos = next_tone_frame(buf, 160, pos);
				new_state = switch_vad_process(vad, buf, 160);
				if (new_state != cur_state) got_transition++;
				cur_state = new_state;
			}
			fst_requires(got_transition == 1);
			fst_requires(cur_state == SWITCH_VAD_STATE_START_TALKING);

			free(buf);
			switch_vad_destroy(&vad);
			fst_check(vad == NULL);
		}
		FST_TEST_END()
#endif

		FST_TEST_BEGIN(energy)
		{
			int res;
			int16_t *buf = malloc(sizeof(int16_t) * 160);
			int duration;
			float pos = 0.0;
			int got_transition = 0;
			switch_vad_state_t cur_state = SWITCH_VAD_STATE_NONE;

			switch_vad_t *vad = switch_vad_init(8000, 1);
			fst_requires(vad);
			res = switch_vad_set_mode(vad, -1);
			fst_requires(res == 0);
			switch_vad_set_param(vad, "silence_ms", 400);
			switch_vad_set_param(vad, "voice_ms", 80);
			switch_vad_set_param(vad, "debug", 10);

			// generate a tone and pump it into the vad
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Start 200 ms tone\n");
			duration = 200 / 20; // 200 ms
			while (--duration >= 0) {
				switch_vad_state_t new_state;
				pos = next_tone_frame(buf, 160, pos);
				new_state = switch_vad_process(vad, buf, 160);
				if (new_state != cur_state) got_transition++;
				cur_state = new_state;
			}
			fst_requires(got_transition == 2);
			fst_requires(cur_state == SWITCH_VAD_STATE_TALKING);

			// feed silence frames into the vad
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Start 1000 ms silence\n");
			duration = 1000 / 20; // 1000 ms
			got_transition = 0;
			next_silence_frame(buf, 160);
			while (--duration >= 0) {
				switch_vad_state_t new_state = switch_vad_process(vad, buf, 160);
				if (new_state != cur_state) got_transition++;
				cur_state = new_state;
			}
			fst_requires(got_transition == 2);
			fst_requires(cur_state == SWITCH_VAD_STATE_NONE);

			// generate a tone < voice_ms
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Start 40 ms tone\n");
			duration = 40 / 20; // 40 ms
			got_transition = 0;
			while (--duration >= 0) {
				switch_vad_state_t new_state;
				pos = next_tone_frame(buf, 160, pos);
				new_state = switch_vad_process(vad, buf, 160);
				if (new_state != cur_state) got_transition++;
				cur_state = new_state;
			}
			fst_requires(got_transition == 0);
			fst_requires(cur_state == SWITCH_VAD_STATE_NONE);

			// continue tone > voice_ms
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Continue with 60 ms tone\n");
			duration = 60 / 20; // 60 ms
			got_transition = 0;
			while (--duration >= 0) {
				switch_vad_state_t new_state;
				pos = next_tone_frame(buf, 160, pos);
				new_state = switch_vad_process(vad, buf, 160);
				if (new_state != cur_state) got_transition++;
				cur_state = new_state;
			}
			fst_requires(got_transition == 1);
			fst_requires(cur_state == SWITCH_VAD_STATE_START_TALKING);

			free(buf);
			switch_vad_destroy(&vad);
			fst_check(vad == NULL);
		}
		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()

