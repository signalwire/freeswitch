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
 * libteletone.h -- Tone Generator/Detector
 *
 */
#ifndef LIBTELETONE_H
#define LIBTELETONE_H

#ifdef __cplusplus
extern "C" {
#endif
#define	TELETONE_MAX_DTMF_DIGITS 128
#define TELETONE_MAX_TONES 6
#define TELETONE_TONE_RANGE 127

typedef double teletone_process_t;

/*! \file libteletone.h
    \brief Top level include file

	This file should be included by applications using the library
*/

/*! \brief An abstraction to store a tone mapping */
typedef struct {
	/*! An array of tone frequencies */
	teletone_process_t freqs[TELETONE_MAX_TONES];
} teletone_tone_map_t;

#if !defined(M_PI)
/* C99 systems may not define M_PI */
#define M_PI 3.14159265358979323846264338327
#endif

#ifdef _MSC_VER
#define int16_t __int16
#endif

#include <libteletone_generate.h>
#include <libteletone_detect.h>

#ifdef __cplusplus
}
#endif

#endif
