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
 *
 *
 * Exception:
 * The author hereby grants the use of this source code under the 
 * following license if and only if the source code is distributed
 * as part of the openzap library.  Any use or distribution of this
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
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
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

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
