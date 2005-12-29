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
 * switch_platform.h -- Platform Specific Header
 *
 */
#ifndef SWITCH_PLATFORM_H
#define SWITCH_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#if (_MSC_VER >= 1400) // VC8+
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif
#endif // VC8+

#ifdef WIN32
#if defined(SWITCH_CORE_DECLARE_STATIC)
#define SWITCH_DECLARE(type)			type __stdcall
#define SWITCH_DECLARE_NONSTD(type)		type __cdecl
#define SWITCH_DECLARE_DATA
#elif defined(FREESWITCHCORE_EXPORTS)
#define SWITCH_DECLARE(type)			__declspec(dllexport) type __stdcall
#define SWITCH_DECLARE_NONSTD(type)		__declspec(dllexport) type __cdecl
#define SWITCH_DECLARE_DATA				__declspec(dllexport)
#else
#define SWITCH_DECLARE(type)			__declspec(dllimport) type __stdcall
#define SWITCH_DECLARE_NONSTD(type)		__declspec(dllimport) type __cdecl
#define SWITCH_DECLARE_DATA				__declspec(dllimport)
#endif

#if defined(SWITCH_MOD_DECLARE_STATIC)
#define SWITCH_MOD_DECLARE(type)		type __cdecl
#elif defined(MOD_EXPORTS)
#define SWITCH_MOD_DECLARE(type)		__declspec(dllexport) type __cdecl
#else
#define SWITCH_MOD_DECLARE(type)		__declspec(dllimport) type __cdecl
#endif
#else //not win32
#define SWITCH_DECLARE(type) type
#define SWITCH_DECLARE_NONSTD(type) type
#define SWITCH_MOD_DECLARE(type) type
#define SWITCH_DECLARE_DATA
#ifndef getpid
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#endif
#endif

#ifndef uint32_t
#ifdef WIN32
typedef unsigned __int8		uint8_t;
typedef unsigned __int16	uint16_t;
typedef unsigned __int32	uint32_t;
typedef unsigned __int64    uint64_t;
typedef __int8		int8_t;
typedef __int16		int16_t;
typedef __int32		int32_t;
typedef __int64		int64_t;
typedef unsigned long	in_addr_t;
#else
#include <sys/types.h>
#endif
#endif

#if defined(_MSC_VER) && _MSC_VER < 1300
#ifndef __FUNCTION__
#define __FUNCTION__ ""
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif
