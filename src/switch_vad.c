/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2018-2020, Anthony Minessale II <anthm@freeswitch.org>
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
 * Chris Rienzo <chris@signalwire.com>
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
	// configs
	int channels;
	int sample_rate;
	int debug;
	int divisor;
	int thresh;
	int voice_samples_thresh;
	int silence_samples_thresh;

	// VAD state
	int voice_samples;
	int silence_samples;
	switch_vad_state_t vad_state;
#ifdef SWITCH_HAVE_FVAD
	Fvad *fvad;
#endif
};

SWITCH_DECLARE(const char *) switch_vad_state2str(switch_vad_state_t state)
{
	switch(state) {
	case SWITCH_VAD_STATE_NONE:
		return "none";
	case SWITCH_VAD_STATE_START_TALKING:
		return "start_talking";
	case SWITCH_VAD_STATE_TALKING:
		return "talking";
	case SWITCH_VAD_STATE_STOP_TALKING:
		return "stop_talking";
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
	vad->silence_samples_thresh = 500 * (vad->sample_rate / 1000);
	vad->voice_samples_thresh = 200 * (vad->sample_rate / 1000);
	vad->thresh = 100;
	vad->divisor = vad->sample_rate / 8000;
	if (vad->divisor <= 0) {
		vad->divisor = 1;
	}
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
	if (vad->debug) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "set vad mode = %d\n", mode);

	return 0;
#endif
}

SWITCH_DECLARE(void) switch_vad_set_param(switch_vad_t *vad, const char *key, int val)
{
	if (!key) return;

	if (!strcmp(key, "hangover_len")) {
		/* convert old-style hits to samples assuming 20ms ptime */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "hangover_len is deprecated, setting silence_ms to %d\n", 20 * val);
		switch_vad_set_param(vad, "silence_ms", val * 20);
	} else if (!strcmp(key, "silence_ms")) {
		if (val > 0) {
			vad->silence_samples_thresh = val * (vad->sample_rate / 1000);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Ignoring invalid silence_ms of %d\n", val);
		}
	} else if (!strcmp(key, "thresh")) {
		vad->thresh = val;
	} else if (!strcmp(key, "debug")) {
		vad->debug = val;
	} else if (!strcmp(key, "voice_ms")) {
		if (val > 0) {
			vad->voice_samples_thresh = val * (vad->sample_rate / 1000);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Ignoring invalid voice_ms of %d\n", val);
		}
	} else if (!strcmp(key, "listen_hits")) {
		/* convert old-style hits to samples assuming 20ms ptime */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "listen_hits is deprecated, setting voice_ms to %d\n", 20 * val);
		switch_vad_set_param(vad, "voice_ms", 20 * val);
	}

	if (vad->debug) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "set %s to %d\n", key, val);
	}
}

SWITCH_DECLARE(void) switch_vad_reset(switch_vad_t *vad)
{
#ifdef SWITCH_HAVE_FVAD
	if (vad->fvad) {
		fvad_reset(vad->fvad);
	}
#endif
	vad->vad_state = SWITCH_VAD_STATE_NONE;
	vad->voice_samples = 0;
	vad->silence_samples = 0;

	if (vad->debug) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "reset vad state\n");
}

SWITCH_DECLARE(switch_vad_state_t) switch_vad_process(switch_vad_t *vad, int16_t *data, unsigned int samples)
{
	int score = 0;

	// Each frame has 2 possible outcomes- voice or not voice.
	// The VAD has 2 real states- talking / not talking with
	// begin talking and stop talking as events to mark transitions


	// determine if this is a voice or non-voice frame
#ifdef SWITCH_HAVE_FVAD
	if (vad->fvad) {
		// fvad returns -1, 0, or 1
		// -1: error
		//  0: non-voice frame
		//  1: voice frame
		int ret = fvad_process(vad->fvad, data, samples);

		// if voice frame set score > threshold
		score = ret > 0 ? vad->thresh + 100 : 0;
	} else {
#endif
		int energy = 0, j = 0, count = 0;
		for (energy = 0, j = 0, count = 0; count < samples; count++) {
			energy += abs(data[j]);
			j += vad->channels;
		}

		score = (uint32_t) (energy / (samples / vad->divisor));
#ifdef SWITCH_HAVE_FVAD
	}
#endif

	if (vad->debug > 9) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "score: %d\n", score);
	}

	// clear the STOP/START TALKING events
	if (vad->vad_state == SWITCH_VAD_STATE_STOP_TALKING) {
		vad->vad_state = SWITCH_VAD_STATE_NONE;
	} else if (vad->vad_state == SWITCH_VAD_STATE_START_TALKING) {
		vad->vad_state = SWITCH_VAD_STATE_TALKING;
	}

	// adjust voice/silence run length counters
	if (score > vad->thresh) {
		vad->silence_samples = 0;
		vad->voice_samples += samples;
	} else {
		vad->silence_samples += samples;
		vad->voice_samples = 0;
	}

	// check for state transitions
	if (vad->vad_state == SWITCH_VAD_STATE_TALKING && vad->silence_samples > vad->silence_samples_thresh) {
		vad->vad_state = SWITCH_VAD_STATE_STOP_TALKING;
		if (vad->debug) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "vad state STOP_TALKING\n");
	} else if (vad->vad_state == SWITCH_VAD_STATE_NONE && vad->voice_samples > vad->voice_samples_thresh) {
		vad->vad_state = SWITCH_VAD_STATE_START_TALKING;
		if (vad->debug) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "vad state START_TALKING\n");
	}

	if (vad->debug > 9) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "vad state %s\n", switch_vad_state2str(vad->vad_state));

	return vad->vad_state;
}

SWITCH_DECLARE(switch_vad_state_t) switch_vad_get_state(switch_vad_t *vad) 
{

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
