/* 
 * libteletone
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * libteletone.h -- Tone Generator/Detector
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
 * Copyright (c) 2005-2012, Anthony Minessale II
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
#ifndef LIBTELETONE_H
#define LIBTELETONE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>

#define	TELETONE_MAX_DTMF_DIGITS 128
#define TELETONE_MAX_TONES 18
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
typedef __int16 int16_t;
#endif

#if (_MSC_VER >= 1400)			// VC8+
#define teletone_assert(expr) assert(expr);__analysis_assume( expr )
#else
#define teletone_assert(expr) assert(expr)
#endif

#ifdef _MSC_VER
#if defined(TT_DECLARE_STATIC)
#define TELETONE_API(type)			type __stdcall
#define TELETONE_API_NONSTD(type)		type __cdecl
#define TELETONE_API_DATA
#elif defined(TELETONE_EXPORTS)
#define TELETONE_API(type)			__declspec(dllexport) type __stdcall
#define TELETONE_API_NONSTD(type)		__declspec(dllexport) type __cdecl
#define TELETONE_API_DATA				__declspec(dllexport)
#else
#define TELETONE_API(type)			__declspec(dllimport) type __stdcall
#define TELETONE_API_NONSTD(type)		__declspec(dllimport) type __cdecl
#define TELETONE_API_DATA				__declspec(dllimport)
#endif
#else
#if (defined(__GNUC__) || defined(__SUNPRO_CC) || defined (__SUNPRO_C)) && defined(HAVE_VISIBILITY)
#define TELETONE_API(type)		__attribute__((visibility("default"))) type
#define TELETONE_API_NONSTD(type)	__attribute__((visibility("default"))) type
#define TELETONE_API_DATA		__attribute__((visibility("default")))
#else
#define TELETONE_API(type)		type
#define TELETONE_API_NONSTD(type)	type
#define TELETONE_API_DATA
#endif
#endif

#include <libteletone_generate.h>
#include <libteletone_detect.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

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
