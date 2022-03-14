/* 
 * libteletone
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * The Original Code is libteletone
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
 * libteletone.h -- Tone Generator
 *
 *
 *
 * Exception:
 * The author hereby grants the use of this source code under the 
 * following license if and only if the source code is distributed
 * as part of the OpenZAP or FreeTDM library.	Any use or distribution of this
 * source code outside the scope of the OpenZAP or FreeTDM library will nullify the
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
#ifndef LIBTELETONE_GENERATE_H
#define LIBTELETONE_GENERATE_H
#ifdef __cplusplus
extern "C" {
#ifdef _doh
}
#endif
#endif

#include <stdio.h>
#include <stdlib.h>

#if  defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#ifndef __inline__
#define __inline__ inline
#endif
#endif

#ifdef _MSC_VER
#ifndef __inline__
#define __inline__ __inline
#endif

typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8 uint8_t;
typedef __int64 int64_t;
typedef __int32 int32_t;
typedef __int16 int16_t;
typedef __int8 int8_t;
#else
#include <stdint.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#if !defined(powf) && !defined(_WIN64)
extern float powf (float, float);
#endif
#include <string.h>
#include <errno.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <assert.h>
#include <stdarg.h>
#include "libteletone.h"

#define TELETONE_VOL_DB_MAX 0
#define TELETONE_VOL_DB_MIN -63
#define MAX_PHASE_TONES 4

struct teletone_dds_state {
	uint32_t phase_rate[MAX_PHASE_TONES];
	uint32_t scale_factor;
	uint32_t phase_accumulator;
	teletone_process_t tx_level;
};
typedef struct teletone_dds_state teletone_dds_state_t;

#define SINE_TABLE_MAX 128
#define SINE_TABLE_LEN (SINE_TABLE_MAX - 1)
#define MAX_PHASE_ACCUMULATOR 0x10000 * 0x10000
/* 3.14 == the max power on ulaw (alaw is 3.17) */
/* 3.02 represents twice the power */
#define DBM0_MAX_POWER (3.14f + 3.02f)

TELETONE_API_DATA extern int16_t TELETONE_SINES[SINE_TABLE_MAX];

static __inline__ int32_t teletone_dds_phase_rate(teletone_process_t tone, uint32_t rate)
{
	return (int32_t) ((tone * MAX_PHASE_ACCUMULATOR) / rate);
}

static __inline__ int16_t teletone_dds_state_modulate_sample(teletone_dds_state_t *dds, uint32_t pindex)
{
	int32_t bitmask = dds->phase_accumulator, sine_index = (bitmask >>= 23) & SINE_TABLE_LEN;
	int16_t sample;

	if (pindex >= MAX_PHASE_TONES)	{
		pindex = 0;
	}

	if (bitmask & SINE_TABLE_MAX) {
		sine_index = SINE_TABLE_LEN - sine_index;
	}

	sample = TELETONE_SINES[sine_index];
	
	if (bitmask & (SINE_TABLE_MAX * 2)) {
		sample *= -1;
	}

	dds->phase_accumulator += dds->phase_rate[pindex];
	return (int16_t) (sample * dds->scale_factor >> 15);
}

static __inline__ void teletone_dds_state_set_tx_level(teletone_dds_state_t *dds, float tx_level)
{
	dds->scale_factor = (int) (powf(10.0f, (tx_level - DBM0_MAX_POWER) / 20.0f) * (32767.0f * 1.414214f));
	dds->tx_level = tx_level;
}

static __inline__ void teletone_dds_state_reset_accum(teletone_dds_state_t *dds)
{
	dds->phase_accumulator = 0;
}

static __inline__ int teletone_dds_state_set_tone(teletone_dds_state_t *dds, teletone_process_t tone, uint32_t rate, uint32_t pindex)
{
	if (pindex < MAX_PHASE_TONES)  {
		dds->phase_rate[pindex] = teletone_dds_phase_rate(tone, rate);
		return 0;
	}
	
	return -1;
}



/*! \file libteletone_generate.h
  \brief Tone Generation Routines

  This module is responsible for tone generation specifics
*/

typedef int16_t teletone_audio_t;
struct teletone_generation_session;
typedef int (*tone_handler)(struct teletone_generation_session *ts, teletone_tone_map_t *map);

/*! \brief An abstraction to store a tone generation session */
struct teletone_generation_session {
	/*! An array of tone mappings to character mappings */
	teletone_tone_map_t TONES[TELETONE_TONE_RANGE];
	/*! The number of channels the output audio should be in */
	int channels;
	/*! The Rate in hz of the output audio */
	int rate;
	/*! The duration (in samples) of the output audio */
	int duration;
	/*! The duration of silence to append after the initial audio is generated */
	int wait;
	/*! The duration (in samples) of the output audio (takes prescedence over actual duration value) */
	int tmp_duration;
	/*! The duration of silence to append after the initial audio is generated (takes prescedence over actual wait value)*/
	int tmp_wait;
	/*! Number of loops to repeat a single instruction*/
	int loops;
	/*! Number of loops to repeat the entire set of instructions*/
	int LOOPS;
	/*! Number to mutiply total samples by to determine when to begin ascent or decent e.g. 0=beginning 4=(last 25%) */
	float decay_factor;
	/*! Direction to perform volume increase/decrease 1/-1*/
	int decay_direction;
	/*! Number of samples between increase/decrease of volume */
	int decay_step;
	/*! Volume factor of the tone */
	float volume;
	/*! Debug on/off */
	int debug;
	/*! FILE stream to write debug data to */
	FILE *debug_stream;
	/*! Extra user data to attach to the session*/
	void *user_data;
	/*! Buffer for storing sample data (dynamic) */
	teletone_audio_t *buffer;
	/*! Size of the buffer */
	int datalen;
	/*! In-Use size of the buffer */
	int samples;
	/*! Callback function called during generation */
	int dynamic;
	tone_handler handler;
};

typedef struct teletone_generation_session teletone_generation_session_t;


/*! 
  \brief Assign a set of tones to a tone_session indexed by a paticular index/character
  \param ts the tone generation session
  \param index the index to map the tone to
  \param ... up to TELETONE_MAX_TONES frequencies terminated by 0.0
  \return 0
*/
TELETONE_API(int) teletone_set_tone(teletone_generation_session_t *ts, int index, ...);

/*! 
  \brief Assign a set of tones to a single tone map
  \param map the map to assign the tones to
  \param ... up to TELETONE_MAX_TONES frequencies terminated by 0.0
  \return 0
*/
TELETONE_API(int) teletone_set_map(teletone_tone_map_t *map, ...);

/*! 
  \brief Initilize a tone generation session
  \param ts the tone generation session to initilize
  \param buflen the size of the buffer(in samples) to dynamically allocate
  \param handler a callback function to execute when a tone generation instruction is complete
  \param user_data optional user data to send
  \return 0
*/
TELETONE_API(int) teletone_init_session(teletone_generation_session_t *ts, int buflen, tone_handler handler, void *user_data);

/*! 
  \brief Free the buffer allocated by a tone generation session
  \param ts the tone generation session to destroy
  \return 0
*/
TELETONE_API(int) teletone_destroy_session(teletone_generation_session_t *ts);

/*! 
  \brief Execute a single tone generation instruction
  \param ts the tone generation session to consult for parameters
  \param map the tone mapping to use for the frequencies
  \return 0
*/
TELETONE_API(int) teletone_mux_tones(teletone_generation_session_t *ts, teletone_tone_map_t *map);

/*! 
  \brief Execute a tone generation script and call callbacks after each instruction
  \param ts the tone generation session to execute on
  \param cmd the script to execute
  \return 0
*/
TELETONE_API(int) teletone_run(teletone_generation_session_t *ts, const char *cmd);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
