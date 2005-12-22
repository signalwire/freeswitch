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
 * switch.h -- Main Library Header
 *
 */
#ifndef SWITCH_H
#define SWITCH_H

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

#include <apr.h>
#include <apr_network_io.h>
#include <apr_errno.h>
#include <apr_general.h>
#include <apr_thread_proc.h>
#include <apr_thread_mutex.h>
#include <apr_thread_cond.h>
#include <apr_thread_rwlock.h>
#include <apr_file_io.h>
#include <apr_poll.h>
#include <apr_dso.h>
#include <apr_hash.h>
#include <apr_strings.h>
#include <apr_network_io.h>
#include <apr_poll.h>
#include <apr_queue.h>
#include <apr_uuid.h>
#include <apr_strmatch.h>
#define APR_WANT_STDIO
#define APR_WANT_STRFUNC
#include <apr_want.h>
#include <assert.h>
#include <sqlite3.h>

#ifdef WIN32
#if defined(SWITCH_CORE_DECLARE_STATIC)
#define SWITCH_DECLARE(type)            type __stdcall
#define SWITCH_DECLARE_NONSTD(type)     type __cdecl
#define SWITCH_DECLARE_DATA
#elif defined(FREESWITCHCORE_EXPORTS)
#define SWITCH_DECLARE(type)            __declspec(dllexport) type __stdcall
#define SWITCH_DECLARE_NONSTD(type)     __declspec(dllexport) type __cdecl
#define SWITCH_DECLARE_DATA             __declspec(dllexport)
#else
#define SWITCH_DECLARE(type)            __declspec(dllimport) type __stdcall
#define SWITCH_DECLARE_NONSTD(type)     __declspec(dllimport) type __cdecl
#define SWITCH_DECLARE_DATA             __declspec(dllimport)
#endif

#if defined(SWITCH_MOD_DECLARE_STATIC)
#define SWITCH_MOD_DECLARE(type)            type __cdecl
#elif defined(MOD_EXPORTS)
#define SWITCH_MOD_DECLARE(type)     __declspec(dllexport) type __cdecl
#else
#define SWITCH_MOD_DECLARE(type)     __declspec(dllimport) type __cdecl
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

#include <switch_types.h>
#include <switch_core.h>
#include <switch_loadable_module.h>
#include <switch_console.h>
#include <switch_utils.h>
#include <switch_caller.h>
#include <switch_mutex.h>
#include <switch_config.h>
#include <switch_frame.h>
#include <switch_module_interfaces.h>
#include <switch_channel.h>
#include <switch_buffer.h>
#include <switch_event.h>

#ifndef WIN32
#include <config.h>
#endif


#ifdef __cplusplus
}
#endif

#endif
