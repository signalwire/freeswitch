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
 * switch_caller.h -- Caller Identification
 *
 */
/*! \file switch_resample.h
    \brief Audio Resample Code
*/

#ifndef SWITCH_RESAMPLE_H
#define SWITCH_RESAMPLE_H

#ifdef __cplusplus
extern "C" {
#endif

struct switch_audio_resampler {
	void *resampler;
	int from_rate;
	int to_rate;
	double factor;
	float *from;
	int from_len;
	size_t from_size;
	float *to;
	int to_len;
	size_t to_size;
};

#include <switch.h>
SWITCH_DECLARE(switch_status) switch_resample_create(switch_audio_resampler **new_resampler,
													 int from_rate,
													 size_t from_size,
													 int to_rate,
													 size_t to_size,
													 switch_memory_pool *pool);

SWITCH_DECLARE(int) switch_resample_process(switch_audio_resampler *resampler, float *src, int srclen, float *dst, int dstlen, int last);

SWITCH_DECLARE(size_t) switch_float_to_short(float *f, short *s, size_t len);
SWITCH_DECLARE(int) switch_char_to_float(char *c, float *f, int len);
SWITCH_DECLARE(int) switch_float_to_char(float *f, char *c, int len);
SWITCH_DECLARE(int) switch_short_to_float(short *s, float *f, int len);
SWITCH_DECLARE(void) switch_swap_linear(int16_t *buf, int len);
SWITCH_DECLARE(void) switch_resample_destroy(switch_audio_resampler *resampler);

#ifdef __cplusplus
}
#endif


#endif


