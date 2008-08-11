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

	This module implements a generic interface for doing audio resampling it currently uses libresample but can be ported to
	any resample library with a little effort.  I decided against making this interface pluggable because there are not many
	options in terms of resample libraries so it seemed like a waste but I did opt to frontend the interface in case a better 
	way comes along some day. =D
	
*/
#define switch_normalize_volume(x) if(x > 4) x = 4; if (x < -4) x = -4;

#ifndef SWITCH_RESAMPLE_H
#define SWITCH_RESAMPLE_H

#include <switch.h>
SWITCH_BEGIN_EXTERN_C
/*!
  \defgroup resamp Audio Resample Functions
  \ingroup core1
  \{ 
*/
/*! \brief An audio resampling handle */
	typedef struct {
	/*! a pointer to store the resampler object */
	void *resampler;
	/*! the rate to resample from in hz */
	int from_rate;
	/*! the rate to resample to in hz */
	int to_rate;
	/*! the factor to resample by (from / to) */
	double factor;
	double rfactor;
	/*! a pointer to store a float buffer for data to be resampled */
	float *from;
	/*! the size of the from buffer used */
	int from_len;
	/*! the total size of the from buffer */
	switch_size_t from_size;
	/*! a pointer to store a float buffer for resampled data */
	float *to;
	/*! the size of the to buffer used */
	uint32_t to_len;
	/*! the total size of the to buffer */
	uint32_t to_size;
} switch_audio_resampler_t;

/*!
  \brief Prepare a new resampler handle
  \param new_resampler NULL pointer to aim at the new handle
  \param from_rate the rate to transfer from in hz
  \param from_size the size of the buffer to allocate for the from data
  \param to_rate the rate to transfer to in hz
  \param to_size the size of the buffer to allocate for the to data
  \param pool the memory pool to use for buffer allocation
  \return SWITCH_STATUS_SUCCESS if the handle was created
 */
SWITCH_DECLARE(switch_status_t) switch_resample_create(switch_audio_resampler_t **new_resampler,
													   int from_rate, switch_size_t from_size, int to_rate, uint32_t to_size, switch_memory_pool_t *pool);

/*!
  \brief Destroy an existing resampler handle
  \param resampler the resampler handle to destroy
 */
SWITCH_DECLARE(void) switch_resample_destroy(switch_audio_resampler_t **resampler);

/*!
  \brief Resample one float buffer into another using specifications of a given handle
  \param resampler the resample handle
  \param src the source data
  \param srclen the length of the source data
  \param dst the destination data
  \param dstlen the length of the destination data
  \param last parameter denoting the last sample is being resampled
  \return the used size of dst
 */
SWITCH_DECLARE(uint32_t) switch_resample_process(switch_audio_resampler_t *resampler, float *src, int srclen, float *dst, uint32_t dstlen, int last);

/*!
  \brief Convert an array of floats to an array of shorts
  \param f the float buffer
  \param s the short buffer
  \param len the length of the buffers
  \return the size of the converted buffer
 */
SWITCH_DECLARE(switch_size_t) switch_float_to_short(float *f, short *s, switch_size_t len);

/*!
  \brief Convert an array of chars to an array of floats
  \param c the char buffer
  \param f the float buffer
  \param len the length of the buffers
  \return the size of the converted buffer
 */
SWITCH_DECLARE(int) switch_char_to_float(char *c, float *f, int len);

/*!
  \brief Convert an array of floats to an array of chars
  \param f an array of floats
  \param c an array of chars
  \param len the length of the buffers
  \return the size of the converted buffer
 */
SWITCH_DECLARE(int) switch_float_to_char(float *f, char *c, int len);

/*!
  \brief Convert an array of shorts to an array of floats
  \param s an array of shorts
  \param f an array of floats
  \param len the size of the buffers
  \return the size of the converted buffer
 */
SWITCH_DECLARE(int) switch_short_to_float(short *s, float *f, int len);

/*!
  \brief Perform a byteswap on a buffer of 16 bit samples
  \param buf an array of samples
  \param len the size of the array
 */
SWITCH_DECLARE(void) switch_swap_linear(int16_t *buf, int len);

/*!
  \brief Generate static noise
  \param data the audio data buffer
  \param samples the number of 2 byte samples
  \param divisor the volume factor
 */
SWITCH_DECLARE(void) switch_generate_sln_silence(int16_t *data, uint32_t samples, uint32_t divisor);

/*!
  \brief Change the volume of a signed linear audio frame
  \param data the audio data
  \param samples the number of 2 byte samples
  \param vol the volume factor -4 -> 4
 */
SWITCH_DECLARE(void) switch_change_sln_volume(int16_t *data, uint32_t samples, int32_t vol);
///\}

SWITCH_DECLARE(uint32_t) switch_merge_sln(int16_t *data, uint32_t samples, int16_t *other_data, uint32_t other_samples);

SWITCH_END_EXTERN_C
#endif
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
