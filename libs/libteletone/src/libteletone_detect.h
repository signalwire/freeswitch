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

#define FALSE   0
#ifndef TRUE
#define TRUE    (!FALSE)
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

#define DTMF_THRESHOLD              8.0e7
#define DTMF_NORMAL_TWIST           6.3     /* 8dB */
#define DTMF_REVERSE_TWIST          2.5     /* 4dB */
#define DTMF_RELATIVE_PEAK_ROW      6.3     /* 8dB */
#define DTMF_RELATIVE_PEAK_COL      6.3     /* 8dB */
#define DTMF_2ND_HARMONIC_ROW       2.5     /* 4dB */
#define DTMF_2ND_HARMONIC_COL       63.1    /* 18dB */
#define GRID_FACTOR 4
#define BLOCK_LEN 102
#define M_TWO_PI 2.0*M_PI

/*! \brief A continer for the elements of a Goertzel Algorithm (The names are from his formula) */
typedef struct {
	teletone_process_t v2;
	teletone_process_t v3;
	teletone_process_t fac;
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
    teletone_process_t energy;
    
    int current_sample;
    char digits[TELETONE_MAX_DTMF_DIGITS + 1];
    int current_digits;
    int detected_digits;
    int lost_digits;
    int digit_hits[16];
} teletone_dtmf_detect_state_t;

/*! \brief An abstraction to store the coefficient of a tone frequency */
typedef struct {
    teletone_process_t fac;
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

	teletone_process_t energy;
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
void teletone_multi_tone_init(teletone_multi_tone_t *mt, teletone_tone_map_t *map);

/*! 
  \brief Check a sample buffer for the presence of the mulit-frequency tone described by mt
  \param mt the multi-frequency tone descriptor
  \param sample_buffer an array aof 16 bit signed linear samples
  \param samples the number of samples present in sample_buffer
  \return true when the tone was detected or false when it is not
*/
int teletone_multi_tone_detect (teletone_multi_tone_t *mt,
								int16_t sample_buffer[],
								int samples);

/*! 
  \brief Initilize a DTMF detection state object
  \param dtmf_detect_state the DTMF detection state to initilize
  \param sample_rate the desired sample rate
*/
void teletone_dtmf_detect_init (teletone_dtmf_detect_state_t *dtmf_detect_state, int sample_rate);

/*! 
  \brief Check a sample buffer for the presence of DTMF digits
  \param dtmf_detect_state the detection state object to check
  \param sample_buffer an array aof 16 bit signed linear samples
  \param samples the number of samples present in sample_buffer
  \return true when DTMF was detected or false when it is not
*/
int teletone_dtmf_detect (teletone_dtmf_detect_state_t *dtmf_detect_state,
						  int16_t sample_buffer[],
						  int samples);
/*! 
  \brief retrieve any collected digits into a string buffer
  \param dtmf_detect_state the detection state object to check
  \param buf the string buffer to write to
  \param max the maximum length of buf
  \return the number of characters written to buf
*/
int teletone_dtmf_get (teletone_dtmf_detect_state_t *dtmf_detect_state,
					   char *buf,
					   int max);

/*! 
  \brief Step through the Goertzel Algorithm for each sample in a buffer
  \param goertzel_state the goertzel state to step the samples through
  \param sample_buffer an array aof 16 bit signed linear samples
  \param samples the number of samples present in sample_buffer
*/
void teletone_goertzel_update(teletone_goertzel_state_t *goertzel_state,
							  int16_t sample_buffer[],
							  int samples);

/*! 
  \brief Compute the result of the last applied step of the Goertzel Algorithm
  \param goertzel_state the goertzel state to retrieve from
  \return the computed value for consideration in furthur audio tests
*/
teletone_process_t teletone_goertzel_result (teletone_goertzel_state_t *goertzel_state);



#ifdef __cplusplus
}
#endif

#endif
