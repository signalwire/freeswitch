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
 * The Original Code is libteletone
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
 * libteletone.h -- Tone Generator
 *
 */
#ifndef LIBTELETONE_GENERATE_H
#define LIBTELETONE_GENERATE_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <libteletone.h>



/*! \file libteletone_generate.h
    \brief Tone Generation Routines

	This module is responsible for tone generation specifics
*/

typedef short teletone_audio_t;
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
	int decay_factor;
	/*! Direction to perform volume increase/decrease 1/-1*/
	int decay_direction;
	/*! Number of samples between increase/decrease of volume */
	int decay_step;
	/*! Volume factor of the tone */
	int volume;
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
int teletone_set_tone(teletone_generation_session_t *ts, int index, ...);

/*! 
  \brief Assign a set of tones to a single tone map
  \param map the map to assign the tones to
  \param ... up to TELETONE_MAX_TONES frequencies terminated by 0.0
  \return 0
*/
int teletone_set_map(teletone_tone_map_t *map, ...);

/*! 
  \brief Initilize a tone generation session
  \param ts the tone generation session to initilize
  \param buflen the size of the buffer(in samples) to dynamically allocate
  \param handler a callback function to execute when a tone generation instruction is complete
  \param user_data optional user data to send
  \return 0
*/
int teletone_init_session(teletone_generation_session_t *ts, int buflen, tone_handler handler, void *user_data);

/*! 
  \brief Free the buffer allocated by a tone generation session
  \param ts the tone generation session to destroy
  \return 0
*/
int teletone_destroy_session(teletone_generation_session_t *ts);

/*! 
  \brief Execute a single tone generation instruction
  \param ts the tone generation session to consult for parameters
  \param map the tone mapping to use for the frequencies
  \return 0
*/
int teletone_mux_tones(teletone_generation_session_t *ts, teletone_tone_map_t *map);

/*! 
  \brief Execute a tone generation script and call callbacks after each instruction
  \param ts the tone generation session to execute on
  \param cmd the script to execute
  \return 0
*/
int teletone_run(teletone_generation_session_t *ts, char *cmd);

#ifdef __cplusplus
}
#endif

#endif
