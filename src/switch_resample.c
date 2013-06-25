/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 *
 * switch_resample.c -- Resampler
 *
 */

#include <switch.h>
#include <switch_resample.h>
#ifndef WIN32
#include <switch_private.h>
#endif
#include <speex/speex_resampler.h>

#define NORMFACT (float)0x8000
#define MAXSAMPLE (float)0x7FFF
#define MAXSAMPLEC (char)0x7F
#define QUALITY 0

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define resample_buffer(a, b, c) a > b ? ((a / 1000) / 2) * c : ((b / 1000) / 2) * c

SWITCH_DECLARE(switch_status_t) switch_resample_perform_create(switch_audio_resampler_t **new_resampler,
															   uint32_t from_rate, uint32_t to_rate,
															   uint32_t to_size,
															   int quality, uint32_t channels, const char *file, const char *func, int line)
{
	int err = 0;
	switch_audio_resampler_t *resampler;
	double lto_rate, lfrom_rate;

	switch_zmalloc(resampler, sizeof(*resampler));

	resampler->resampler = speex_resampler_init(channels ? channels : 1, from_rate, to_rate, quality, &err);

	if (!resampler->resampler) {
		free(resampler);
		return SWITCH_STATUS_GENERR;
	}

	*new_resampler = resampler;
	lto_rate = (double) resampler->to_rate;
	lfrom_rate = (double) resampler->from_rate;
	resampler->from_rate = from_rate;
	resampler->to_rate = to_rate;
	resampler->factor = (lto_rate / lfrom_rate);
	resampler->rfactor = (lfrom_rate / lto_rate);
	resampler->to_size = resample_buffer(to_rate, from_rate, (uint32_t) to_size);
	resampler->to = malloc(resampler->to_size * sizeof(int16_t));

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(uint32_t) switch_resample_process(switch_audio_resampler_t *resampler, int16_t *src, uint32_t srclen)
{
	resampler->to_len = resampler->to_size;
	speex_resampler_process_interleaved_int(resampler->resampler, src, &srclen, resampler->to, &resampler->to_len);
	return resampler->to_len;
}

SWITCH_DECLARE(void) switch_resample_destroy(switch_audio_resampler_t **resampler)
{

	if (resampler && *resampler) {
		if ((*resampler)->resampler) {
			speex_resampler_destroy((*resampler)->resampler);
		}
		free((*resampler)->to);
		free(*resampler);
		*resampler = NULL;
	}
}

SWITCH_DECLARE(switch_size_t) switch_float_to_short(float *f, short *s, switch_size_t len)
{
	switch_size_t i;
	float ft;
	for (i = 0; i < len; i++) {
		ft = f[i] * NORMFACT;
		if (ft >= 0) {
			s[i] = (short) (ft + 0.5);
		} else {
			s[i] = (short) (ft - 0.5);
		}
		if ((float) s[i] > MAXSAMPLE)
			s[i] = (short) MAXSAMPLE / 2;
		if (s[i] < (short) -MAXSAMPLE)
			s[i] = (short) -MAXSAMPLE / 2;
	}
	return len;
}

SWITCH_DECLARE(int) switch_char_to_float(char *c, float *f, int len)
{
	int i;

	if (len % 2) {
		return (-1);
	}

	for (i = 1; i < len; i += 2) {
		f[(int) (i / 2)] = (float) (((c[i]) * 0x100) + c[i - 1]);
		f[(int) (i / 2)] /= NORMFACT;
		if (f[(int) (i / 2)] > MAXSAMPLE)
			f[(int) (i / 2)] = MAXSAMPLE;
		if (f[(int) (i / 2)] < -MAXSAMPLE)
			f[(int) (i / 2)] = -MAXSAMPLE;
	}
	return len / 2;
}

SWITCH_DECLARE(int) switch_float_to_char(float *f, char *c, int len)
{
	int i;
	float ft;
	long l;
	for (i = 0; i < len; i++) {
		ft = f[i] * NORMFACT;
		if (ft >= 0) {
			l = (long) (ft + 0.5);
		} else {
			l = (long) (ft - 0.5);
		}
		c[i * 2] = (unsigned char) ((l) & 0xff);
		c[i * 2 + 1] = (unsigned char) (((l) >> 8) & 0xff);
	}
	return len * 2;
}

SWITCH_DECLARE(int) switch_short_to_float(short *s, float *f, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		f[i] = (float) (s[i]) / NORMFACT;
		/* f[i] = (float) s[i]; */
	}
	return len;
}


SWITCH_DECLARE(void) switch_swap_linear(int16_t *buf, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		buf[i] = ((buf[i] >> 8) & 0x00ff) | ((buf[i] << 8) & 0xff00);
	}
}

#if 1
SWITCH_DECLARE(void) switch_generate_sln_silence(int16_t *data, uint32_t samples, uint32_t divisor)
{
	int16_t x;
	uint32_t i;
	int sum_rnd = 0;
	int16_t rnd2 = (int16_t) switch_micro_time_now() + (int16_t) (intptr_t) data;

	assert(divisor);

	if (divisor == (uint32_t)-1) {
		memset(data, 0, samples * 2);
		return;
	}

	for (i = 0; i < samples; i++, sum_rnd = 0) {
		for (x = 0; x < 6; x++) {
			rnd2 = rnd2 * 31821U + 13849U;
			sum_rnd += rnd2;
		}
		//switch_normalize_to_16bit(sum_rnd);
		*data = (int16_t) ((int16_t) sum_rnd / (int) divisor);

		data++;
	}
}
#else

SWITCH_DECLARE(void) switch_generate_sln_silence(int16_t *data, uint32_t samples, uint32_t divisor)
{
	int16_t rnd = 0, rnd2, x;
	uint32_t i;
	int sum_rnd = 0;

	assert(divisor);

	rnd2 = (int16_t) (intptr_t) (&data + switch_epoch_time_now(NULL));

	for (i = 0; i < samples; i++, sum_rnd = 0) {
		for (x = 0; x < 10; x++) {
			rnd = rnd + (int16_t) ((x + i) * rnd2);
			sum_rnd += rnd;
		}
		switch_normalize_to_16bit(sum_rnd);
		*data = (int16_t) ((int16_t) sum_rnd / (int) divisor);

		data++;
	}
}

#endif

SWITCH_DECLARE(uint32_t) switch_merge_sln(int16_t *data, uint32_t samples, int16_t *other_data, uint32_t other_samples)
{
	int i;
	int32_t x, z;

	if (samples > other_samples) {
		x = other_samples;
	} else {
		x = samples;
	}

	for (i = 0; i < x; i++) {
		z = data[i] + other_data[i];
		switch_normalize_to_16bit(z);
		data[i] = (int16_t) z;
	}

	return x;
}


SWITCH_DECLARE(uint32_t) switch_unmerge_sln(int16_t *data, uint32_t samples, int16_t *other_data, uint32_t other_samples)
{
	int i;
	int32_t x;

	if (samples > other_samples) {
		x = other_samples;
	} else {
		x = samples;
	}

	for (i = 0; i < x; i++) {
		data[i] -= other_data[i];
	}

	return x;
}

SWITCH_DECLARE(void) switch_mux_channels(int16_t *data, switch_size_t samples, uint32_t channels)
{
	switch_size_t i = 0;
	uint32_t j = 0;

	for (i = 0; i < samples; i++) {
		int32_t z = 0;
		for (j = 0; j < channels; j++) {
			z += data[i * channels + j];
			switch_normalize_to_16bit(z);
			data[i] = (int16_t) z;
		}
	}
}

SWITCH_DECLARE(void) switch_change_sln_volume_granular(int16_t *data, uint32_t samples, int32_t vol)
{
	double newrate = 0;
	double pos[12] = {1.25, 1.50, 1.75, 2.0, 2.25, 2.50, 2.75, 3.0, 3.25, 3.50, 3.75, 4.0};
	double neg[12] = {.917, .834, .751, .668, .585, .502, .419, .336, .253, .017, .087, .004};
	double *chart;
	uint32_t i;

	if (vol == 0) return;

	switch_normalize_volume_granular(vol);

	if (vol > 0) {
		chart = pos;
	} else {
		chart = neg;
	}
	
	i = abs(vol) - 1;
	
	switch_assert(i < 12);

	newrate = chart[i];

	if (newrate) {
		int32_t tmp;
		uint32_t x;
		int16_t *fp = data;

		for (x = 0; x < samples; x++) {
			tmp = (int32_t) (fp[x] * newrate);
			switch_normalize_to_16bit(tmp);
			fp[x] = (int16_t) tmp;
		}
	}
}

SWITCH_DECLARE(void) switch_change_sln_volume(int16_t *data, uint32_t samples, int32_t vol)
{
	double newrate = 0;
	double pos[4] = {1.3, 2.3, 3.3, 4.3};
	double neg[4] = {.80, .60, .40, .20};
	double *chart;
	uint32_t i;

	if (vol == 0) return;

	switch_normalize_volume(vol);

	if (vol > 0) {
		chart = pos;
	} else {
		chart = neg;
	}
	
	i = abs(vol) - 1;
	
	switch_assert(i < 4);

	newrate = chart[i];

	if (newrate) {
		int32_t tmp;
		uint32_t x;
		int16_t *fp = data;

		for (x = 0; x < samples; x++) {
			tmp = (int32_t) (fp[x] * newrate);
			switch_normalize_to_16bit(tmp);
			fp[x] = (int16_t) tmp;
		}
	}
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
