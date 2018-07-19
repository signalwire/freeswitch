/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2018, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 * Seven Du <dujinfang@gmail.com>
 *
 *
 * switch_vad.c VAD code with optional libfvad
 *
 */

#include <switch.h>

#ifdef SWITCH_HAVE_FVAD
#include <fvad.h>
#endif

struct switch_vad_s {
	int talking;
	int talked;
	int talk_hits;
	int hangover;
	int hangover_len;
	int divisor;
	int thresh;
	int timeout_len;
	int timeout;
	int channels;
	int sample_rate;

	int _hangover_len;
	int _thresh;
	int _timeout_len;
#ifdef SWITCH_HAVE_FVAD
	Fvad *fvad;
#endif
};

SWITCH_DECLARE(switch_vad_t *) switch_vad_init(int sample_rate, int channels)
{
	switch_vad_t *vad = malloc(sizeof(switch_vad_t));

	if (!vad) return NULL;

	memset(vad, 0, sizeof(*vad));
	vad->sample_rate = sample_rate ? sample_rate : 8000;
	vad->channels = channels;
	vad->_hangover_len = 25;
	vad->_thresh = 100;
	vad->_timeout_len = 25;

	switch_vad_reset(vad);

	return vad;
}

SWITCH_DECLARE(int) switch_vad_set_mode(switch_vad_t *vad, int mode)
{
#ifdef SWITCH_HAVE_FVAD
	int ret = 0;

	if (mode < 0 && vad->fvad) {
		fvad_free(vad->fvad);
		vad->fvad = NULL;
		return ret;
	} else if (mode > 3) {
		mode = 3;
	}

	if (!vad->fvad) {
		vad->fvad = fvad_new();

		if (!vad->fvad) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "libfvad init error\n");
		}
	}

	if (vad->fvad) {
		ret = fvad_set_mode(vad->fvad, mode);
		fvad_set_sample_rate(vad->fvad, vad->sample_rate);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "libfvad started, mode = %d\n", mode);
	return ret;
#else
	return 0;
#endif
}

SWITCH_DECLARE(void) switch_vad_set_param(switch_vad_t *vad, const char *key, int val)
{
	if (!key) return;

	if (!strcmp(key, "hangover_len")) {
		vad->hangover_len = vad->_hangover_len = val;
	} else if (!strcmp(key, "thresh")) {
		vad->thresh = vad->_thresh = val;
	} else if (!strcmp(key, "timeout_len")) {
		vad->timeout = vad->timeout_len = vad->_timeout_len = val;
	}
}

SWITCH_DECLARE(void) switch_vad_reset(switch_vad_t *vad)
{
#ifdef SWITCH_HAVE_FVAD
	if (vad->fvad) {
		fvad_reset(vad->fvad);
		return;
	}
#endif

	vad->talking = 0;
	vad->talked = 0;
	vad->talk_hits = 0;
	vad->hangover = 0;
	vad->hangover_len = vad->_hangover_len;
	vad->divisor = vad->sample_rate / 8000;
	vad->thresh = vad->_thresh;
	vad->timeout_len = vad->_timeout_len;
	vad->timeout = vad->timeout_len;
}

SWITCH_DECLARE(switch_vad_state_t) switch_vad_process(switch_vad_t *vad, int16_t *data, unsigned int samples)
{
	int energy = 0, j = 0, count = 0;
	int score = 0;
	switch_vad_state_t vad_state = SWITCH_VAD_STATE_NONE;

#ifdef SWITCH_HAVE_FVAD
	if (vad->fvad) {
		int ret = fvad_process(vad->fvad, data, samples);

		// printf("%d ", ret); fflush(stdout);

		score = vad->thresh + ret - 1;
	} else {
#endif

	for (energy = 0, j = 0, count = 0; count < samples; count++) {
		energy += abs(data[j]);
		j += vad->channels;
	}

	score = (uint32_t) (energy / (samples / vad->divisor));

#ifdef SWITCH_HAVE_FVAD
	}
#endif

	// printf("%d ", score); fflush(stdout);
	// printf("yay %d %d %d\n", score, vad->hangover, vad->talking);

	if (vad->talking && score < vad->thresh) {
		if (vad->hangover > 0) {
			vad->hangover--;
		} else {// if (hangover <= 0) {
			vad->talking = 0;
			vad->talk_hits = 0;
			vad->hangover = 0;
		}
	} else {
		if (score >= vad->thresh) {
			vad_state = vad->talking ? SWITCH_VAD_STATE_TALKING : SWITCH_VAD_STATE_START_TALKING;
			vad->talking = 1;
			vad->hangover = vad->hangover_len;
		}
	}

	// printf("WTF %d %d %d\n", score, vad->talked, vad->talking);

	if (vad->talking) {
		vad->talk_hits++;
		// printf("WTF %d %d %d\n", vad->talking, vad->talk_hits, vad->talked);
		if (vad->talk_hits > 10) {
			vad->talked = 1;
			vad_state = SWITCH_VAD_STATE_TALKING;
		}
	} else {
		vad->talk_hits = 0;
	}

	if (vad->timeout > 0 && !vad->talking) {
		vad->timeout--;
	}

	if ((vad->talked && !vad->talking)) {
		// printf("NOT TALKING ANYMORE\n");
		vad->talked = 0;
		vad->timeout = vad->timeout_len;
		vad_state = SWITCH_VAD_STATE_STOP_TALKING;
	} else {
		// if (vad->skip > 0) {
		// 	vad->skip--;
		// }
	}

	if (vad_state) return vad_state;

	return SWITCH_VAD_STATE_NONE;
}

SWITCH_DECLARE(void) switch_vad_destroy(switch_vad_t **vad)
{
	if (*vad) {

#ifdef SWITCH_HAVE_FVAD
		if ((*vad)->fvad) fvad_free ((*vad)->fvad);
#endif

		free(*vad);
		*vad = NULL;
	}
}
