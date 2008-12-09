/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * switch_caller.c -- Caller Identification
 *
 */

#include <switch.h>
#include <switch_resample.h>
#ifndef WIN32
#include <switch_private.h>
#endif
#ifndef DISABLE_RESAMPLE
#include <libresample.h>
#endif
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

SWITCH_DECLARE(switch_status_t) switch_resample_create(switch_audio_resampler_t **new_resampler,
													   int from_rate, switch_size_t from_size, int to_rate, uint32_t to_size, switch_memory_pool_t *pool)
{
#ifdef DISABLE_RESAMPLE
	*new_resampler = NULL;
	return SWITCH_STATUS_NOTIMPL;
#else
	switch_audio_resampler_t *resampler;
	double lto_rate, lfrom_rate;

	if ((resampler = switch_core_alloc(pool, sizeof(*resampler))) == 0) {
		return SWITCH_STATUS_MEMERR;
	}

	resampler->from_rate = from_rate;
	resampler->to_rate = to_rate;
	lto_rate = (double) resampler->to_rate;
	lfrom_rate = (double) resampler->from_rate;
	resampler->factor = (lto_rate / lfrom_rate);
	resampler->rfactor = (lfrom_rate / lto_rate);

	resampler->resampler = resample_open(QUALITY, resampler->factor, resampler->factor);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Activate Resampler %d->%d %f\n", resampler->from_rate, resampler->to_rate,
					  resampler->factor);
	resampler->from_size = resample_buffer(to_rate, from_rate, (uint32_t) from_size);
	resampler->from = (float *) switch_core_alloc(pool, resampler->from_size * sizeof(float));
	resampler->to_size = resample_buffer(to_rate, from_rate, (uint32_t) to_size);;
	resampler->to = (float *) switch_core_alloc(pool, resampler->to_size * sizeof(float));

	*new_resampler = resampler;
	return SWITCH_STATUS_SUCCESS;
#endif
}

SWITCH_DECLARE(uint32_t) switch_resample_process(switch_audio_resampler_t *resampler, float *src, int srclen, float *dst, uint32_t dstlen, int last)
{
#ifdef DISABLE_RESAMPLE
	return 0;
#else
	int o = 0, srcused = 0, srcpos = 0, out = 0;

	for (;;) {
		int srcBlock = MIN(srclen - srcpos, srclen);
		int lastFlag = (last && (srcBlock == srclen - srcpos));
		o = resample_process(resampler->resampler, resampler->factor, &src[srcpos], srcBlock, lastFlag, &srcused, &dst[out], dstlen - out);
		/* printf("resampling %d/%d (%d) %d %f\n",  srcpos, srclen,  MIN(dstlen-out, dstlen), srcused, factor); */

		srcpos += srcused;
		if (o >= 0) {
			out += o;
		}
		if (o < 0 || (o == 0 && srcpos == srclen)) {
			break;
		}
	}
	return out;
#endif
}

SWITCH_DECLARE(void) switch_resample_destroy(switch_audio_resampler_t **resampler)
{

	if (resampler && *resampler) {
#ifndef DISABLE_RESAMPLE
		if ((*resampler)->resampler) {
			resample_close((*resampler)->resampler);
		}
#endif
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
			s[i] = (short) MAXSAMPLE;
		if (s[i] < (short) -MAXSAMPLE)
			s[i] = (short) -MAXSAMPLE;
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

SWITCH_DECLARE(void) switch_generate_sln_silence(int16_t *data, uint32_t samples, uint32_t divisor)
{
	int16_t x;
	uint32_t i;
	int sum_rnd = 0;
	int16_t rnd2 = (int16_t) switch_timestamp_now();

	assert(divisor);



	for (i = 0; i < samples; i++, sum_rnd = 0) {
		for (x = 0; x < 6; x++) {
			rnd2 = rnd2 * 31821U + 13849U;
			sum_rnd += rnd2;
		}
		switch_normalize_to_16bit(sum_rnd);
		*data = (int16_t) ((int16_t) sum_rnd / (int) divisor);

		data++;
	}
}

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

SWITCH_DECLARE(void) switch_change_sln_volume(int16_t *data, uint32_t samples, int32_t vol)
{
	double newrate = 0;
	int div = 0;

	switch_normalize_volume(vol);

	if (vol > 0) {
		vol++;
	} else if (vol < 0) {
		vol--;
	}

	newrate = vol * 1.3;

	if (vol < 0) {
		newrate *= -1;
		div++;
	}

	if (newrate) {
		int32_t tmp;
		uint32_t x;
		int16_t *fp = data;

		for (x = 0; x < samples; x++) {
			tmp = (int32_t) (div ? fp[x] / newrate : fp[x] * newrate);
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
