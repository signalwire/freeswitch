/* 
 * libteletone
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
 * The Original Code is tone_detect.c - General telephony tone detection, and specific detection of DTMF.
 *
 *
 * The Initial Developer of the Original Code is
 * Stephen Underwood <steveu@coppice.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * The the original interface designed by Steve Underwood was preserved to retain 
 *the optimizations when considering DTMF tones though the names were changed in the interest 
 * of namespace.
 *
 * Much less efficient expansion interface was added to allow for the detection of 
 * a single arbitrary tone combination which may also exceed 2 simultaneous tones.
 * (controlled by compile time constant TELETONE_MAX_TONES)
 *
 * Copyright (C) 2006 Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * libteletone_detect.c Tone Detection Code
 *
 *
 ********************************************************************************* 
 *
 * Derived from tone_detect.h - General telephony tone detection, and specific
 * detection of DTMF.
 *
 * Copyright (C) 2001  Steve Underwood <steveu@coppice.org>
 *
 * Despite my general liking of the GPL, I place this code in the
 * public domain for the benefit of all mankind - even the slimy
 * ones who might try to proprietize my work and use it to my
 * detriment.
 *
 *
 * Exception:
 * The author hereby grants the use of this source code under the 
 * following license if and only if the source code is distributed
 * as part of the openzap library.	Any use or distribution of this
 * source code outside the scope of the openzap library will nullify the
 * following license and reinact the MPL 1.1 as stated above.
 *
 * Copyright (c) 2007, Anthony Minessale II
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.	 IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LIBTELETONE_DETECT_H
#define LIBTELETONE_DETECT_H

#ifdef __cplusplus
extern "C" {
#endif
#include <libteletone.h>

	/*! \file libteletone_detect.h
	  \brief Tone Detection Routines

	  This module is responsible for tone detection specifics
	*/

#ifndef FALSE
#define FALSE	0
#ifndef TRUE
#define TRUE	(!FALSE)
#endif
#endif

	/* Basic DTMF specs:
	 *
	 * Minimum tone on = 40ms
	 * Minimum tone off = 50ms
	 * Maximum digit rate = 10 per second
	 * Normal twist <= 8dB accepted
	 * Reverse twist <= 4dB accepted
	 * S/N >= 15dB will detect OK
	 * Attenuation <= 26dB will detect OK
	 * Frequency tolerance +- 1.5% will detect, +-3.5% will reject
	 */

#define DTMF_THRESHOLD				8.0e7
#define DTMF_NORMAL_TWIST			6.3		/* 8dB */
#define DTMF_REVERSE_TWIST			2.5		/* 4dB */
#define DTMF_RELATIVE_PEAK_ROW		6.3		/* 8dB */
#define DTMF_RELATIVE_PEAK_COL		6.3		/* 8dB */
#define DTMF_2ND_HARMONIC_ROW		2.5		/* 4dB */
#define DTMF_2ND_HARMONIC_COL		63.1	/* 18dB */
#define GRID_FACTOR 4
#define BLOCK_LEN 102
#define M_TWO_PI 2.0*M_PI

	/*! \brief A continer for the elements of a Goertzel Algorithm (The names are from his formula) */
	typedef struct {
		float v2;
		float v3;
		double fac;
	} teletone_goertzel_state_t;
	
	/*! \brief A container for a DTMF detection state.*/
	typedef struct {
		int hit1;
		int hit2;
		int hit3;
		int hit4;
		int mhit;

		teletone_goertzel_state_t row_out[GRID_FACTOR];
		teletone_goertzel_state_t col_out[GRID_FACTOR];
		teletone_goertzel_state_t row_out2nd[GRID_FACTOR];
		teletone_goertzel_state_t col_out2nd[GRID_FACTOR];
		float energy;
	
		int current_sample;
		char digits[TELETONE_MAX_DTMF_DIGITS + 1];
		int current_digits;
		int detected_digits;
		int lost_digits;
		int digit_hits[16];
	} teletone_dtmf_detect_state_t;

	/*! \brief An abstraction to store the coefficient of a tone frequency */
	typedef struct {
		float fac;
	} teletone_detection_descriptor_t;

	/*! \brief A container for a single multi-tone detection 
	  TELETONE_MAX_TONES dictates the maximum simultaneous tones that can be present
	  in a multi-tone representation.
	*/
	typedef struct {
		int sample_rate;

		teletone_detection_descriptor_t tdd[TELETONE_MAX_TONES];
		teletone_goertzel_state_t gs[TELETONE_MAX_TONES];
		teletone_goertzel_state_t gs2[TELETONE_MAX_TONES];
		int tone_count;

		float energy;
		int current_sample;
	
		int min_samples;
		int total_samples;

		int positives;
		int negatives;
		int hits;

		int positive_factor;
		int negative_factor;
		int hit_factor;

	} teletone_multi_tone_t;


	/*! 
	  \brief Initilize a multi-frequency tone detector
	  \param mt the multi-frequency tone descriptor
	  \param map a representation of the multi-frequency tone
	*/
TELETONE_API(void) teletone_multi_tone_init(teletone_multi_tone_t *mt, teletone_tone_map_t *map);

	/*! 
	  \brief Check a sample buffer for the presence of the mulit-frequency tone described by mt
	  \param mt the multi-frequency tone descriptor
	  \param sample_buffer an array aof 16 bit signed linear samples
	  \param samples the number of samples present in sample_buffer
	  \return true when the tone was detected or false when it is not
	*/
TELETONE_API(int) teletone_multi_tone_detect (teletone_multi_tone_t *mt,
									int16_t sample_buffer[],
									int samples);

	/*! 
	  \brief Initilize a DTMF detection state object
	  \param dtmf_detect_state the DTMF detection state to initilize
	  \param sample_rate the desired sample rate
	*/
TELETONE_API(void) teletone_dtmf_detect_init (teletone_dtmf_detect_state_t *dtmf_detect_state, int sample_rate);

	/*! 
	  \brief Check a sample buffer for the presence of DTMF digits
	  \param dtmf_detect_state the detection state object to check
	  \param sample_buffer an array aof 16 bit signed linear samples
	  \param samples the number of samples present in sample_buffer
	  \return true when DTMF was detected or false when it is not
	*/
TELETONE_API(int) teletone_dtmf_detect (teletone_dtmf_detect_state_t *dtmf_detect_state,
							  int16_t sample_buffer[],
							  int samples);
	/*! 
	  \brief retrieve any collected digits into a string buffer
	  \param dtmf_detect_state the detection state object to check
	  \param buf the string buffer to write to
	  \param max the maximum length of buf
	  \return the number of characters written to buf
	*/
TELETONE_API(int) teletone_dtmf_get (teletone_dtmf_detect_state_t *dtmf_detect_state,
						   char *buf,
						   int max);

	/*! 
	  \brief Step through the Goertzel Algorithm for each sample in a buffer
	  \param goertzel_state the goertzel state to step the samples through
	  \param sample_buffer an array aof 16 bit signed linear samples
	  \param samples the number of samples present in sample_buffer
	*/
TELETONE_API(void) teletone_goertzel_update(teletone_goertzel_state_t *goertzel_state,
								  int16_t sample_buffer[],
								  int samples);



#ifdef __cplusplus
}
#endif

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
