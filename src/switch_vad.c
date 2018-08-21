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
	int listen_hits;
	int hangover;
	int hangover_len;
	int divisor;
	int thresh;
	int channels;
	int sample_rate;
	int debug;
	int _hangover_len;
	int _thresh;
	int _listen_hits;
	switch_vad_state_t vad_state;
#ifdef SWITCH_HAVE_FVAD
	Fvad *fvad;
#endif
};

SWITCH_DECLARE(const char *) switch_vad_state2str(switch_vad_state_t state)
{
	switch(state) {
	case SWITCH_VAD_STATE_NONE:                                                                                                                                      return "none";
		return "none";
	case SWITCH_VAD_STATE_START_TALKING:                                                                                                                             return "start_talking";
		return "start-talking";
	case SWITCH_VAD_STATE_TALKING:                                                                                                                                   return "talking";
		return "talking";
	case SWITCH_VAD_STATE_STOP_TALKING:                                                                                                                              return "stop_talking";
		return "stop-talking";
	default:
		return "error";
	}
}

SWITCH_DECLARE(switch_vad_t *) switch_vad_init(int sample_rate, int channels)
{
	switch_vad_t *vad = malloc(sizeof(switch_vad_t));

	if (!vad) return NULL;

	memset(vad, 0, sizeof(*vad));
	vad->sample_rate = sample_rate ? sample_rate : 8000;
	vad->channels = channels;
	vad->_hangover_len = 25;
	vad->_thresh = 100;
	vad->_listen_hits = 10;
	switch_vad_reset(vad);

	return vad;
}

SWITCH_DECLARE(int) switch_vad_set_mode(switch_vad_t *vad, int mode)
{
#ifdef SWITCH_HAVE_FVAD
	int ret = 0;

	if (mode < 0) {
		if (vad->fvad) fvad_free(vad->fvad);

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
	} else if (!strcmp(key, "debug")) {
		vad->debug = val;
	} else if (!strcmp(key, "listen_hits")) {
		vad->listen_hits = vad->_listen_hits = val;
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
	vad->listen_hits = vad->_listen_hits;
	vad->hangover_len = vad->_hangover_len;
	vad->divisor = vad->sample_rate / 8000;
	vad->thresh = vad->_thresh;
	vad->vad_state = SWITCH_VAD_STATE_NONE;
}

SWITCH_DECLARE(switch_vad_state_t) switch_vad_process(switch_vad_t *vad, int16_t *data, unsigned int samples)
{
	int energy = 0, j = 0, count = 0;
	int score = 0;

	if (vad->vad_state == SWITCH_VAD_STATE_STOP_TALKING) {
		vad->vad_state = SWITCH_VAD_STATE_NONE;
	}

	if (vad->vad_state == SWITCH_VAD_STATE_START_TALKING) {
		vad->vad_state = SWITCH_VAD_STATE_TALKING;
	}
	
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

	//printf("%d ", score); fflush(stdout);
	//printf("yay %d %d %d\n", score, vad->hangover, vad->talking);

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
			vad->vad_state = vad->talking ? SWITCH_VAD_STATE_TALKING : SWITCH_VAD_STATE_START_TALKING;
			vad->talking = 1;
			vad->hangover = vad->hangover_len;
		}
	}

	// printf("WTF %d %d %d\n", score, vad->talked, vad->talking);

	if (vad->talking) {
		vad->talk_hits++;
		// printf("WTF %d %d %d\n", vad->talking, vad->talk_hits, vad->talked);
		if (vad->talk_hits > vad->listen_hits) {
			vad->talked = 1;
			vad->vad_state = SWITCH_VAD_STATE_TALKING;
		}
	} else {
		vad->talk_hits = 0;
	}

	if ((vad->talked && !vad->talking)) {
		// printf("NOT TALKING ANYMORE\n");
		vad->talked = 0;
		vad->vad_state = SWITCH_VAD_STATE_STOP_TALKING;
	}

	if (vad->debug > 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "VAD DEBUG energy: %d state %s\n", score, switch_vad_state2str(vad->vad_state));
	}
	
	return vad->vad_state;
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
