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
#include <switch_resample.h>
#include <libresample.h>
#define NORMFACT (float)0x8000
#define MAXSAMPLE (float)0x7FFF
#define MAXSAMPLEC (char)0x7F
#define QUALITY 1

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif



SWITCH_DECLARE(switch_status) switch_resample_create(switch_audio_resampler **new_resampler,
													 int from_rate,
													 size_t from_size,
													 int to_rate, size_t to_size, switch_memory_pool *pool)
{
	switch_audio_resampler *resampler;

	if (!(resampler = switch_core_alloc(pool, sizeof(*resampler)))) {
		return SWITCH_STATUS_MEMERR;
	}

	resampler->from_rate = from_rate;
	resampler->to_rate = to_rate;
	resampler->factor = ((double) resampler->to_rate / (double) resampler->from_rate);

	resampler->resampler = resample_open(QUALITY, resampler->factor, resampler->factor);
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Activate Resampler %d->%d %f\n", resampler->from_rate,
						  resampler->to_rate, resampler->factor);
	resampler->from_size = from_size;
	resampler->from = (float *) switch_core_alloc(pool, resampler->from_size);
	resampler->to_size = to_size;
	resampler->to = (float *) switch_core_alloc(pool, resampler->to_size);

	*new_resampler = resampler;
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(int) switch_resample_process(switch_audio_resampler *resampler, float *src, int srclen, float *dst,
											int dstlen, int last)
{
	int o = 0, srcused = 0, srcpos = 0, out = 0;

	for (;;) {
		int srcBlock = MIN(srclen - srcpos, srclen);
		int lastFlag = (last && (srcBlock == srclen - srcpos));
		o = resample_process(resampler->resampler, resampler->factor, &src[srcpos], srcBlock, lastFlag, &srcused,
							 &dst[out], dstlen - out);
		//printf("resampling %d/%d (%d) %d %f\n",  srcpos, srclen,  MIN(dstlen-out, dstlen), srcused, factor);

		srcpos += srcused;
		if (o >= 0) {
			out += o;
		}
		if (o < 0 || (o == 0 && srcpos == srclen)) {
			break;
		}
	}
	return out;
}

SWITCH_DECLARE(void) switch_resample_destroy(switch_audio_resampler *resampler)
{
	resample_close(resampler->resampler);
}


SWITCH_DECLARE(size_t) switch_float_to_short(float *f, short *s, size_t len)
{
	size_t i;
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
		//f[i] = (float) s[i];
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
