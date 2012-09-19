/*
 * Copyright (c) 2007-2012, Anthony Minessale II
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

#ifndef _KS_H_
#define _KS_H_

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

#define ks_copy_string(_x, _y, _z) strncpy(_x, _y, _z - 1)
#define ks_set_string(_x, _y) ks_copy_string(_x, _y, sizeof(_x))
#define KS_VA_NONE "%s", ""


typedef enum {
	KS_POLL_READ = (1 << 0),
	KS_POLL_WRITE = (1 << 1),
	KS_POLL_ERROR = (1 << 2)
} ks_poll_t;

#ifdef WIN32
#define KS_SEQ_FWHITE FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY
#define KS_SEQ_BWHITE FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE
#define KS_SEQ_FRED FOREGROUND_RED | FOREGROUND_INTENSITY
#define KS_SEQ_BRED FOREGROUND_RED
#define KS_SEQ_FMAGEN FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY
#define KS_SEQ_BMAGEN FOREGROUND_BLUE | FOREGROUND_RED
#define KS_SEQ_FCYAN FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY
#define KS_SEQ_BCYAN FOREGROUND_GREEN | FOREGROUND_BLUE
#define KS_SEQ_FGREEN FOREGROUND_GREEN | FOREGROUND_INTENSITY
#define KS_SEQ_BGREEN FOREGROUND_GREEN 
#define KS_SEQ_FYELLOW FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY
#define KS_SEQ_BYELLOW FOREGROUND_RED | FOREGROUND_GREEN
#define KS_SEQ_DEFAULT_COLOR KS_SEQ_FWHITE
#define KS_SEQ_FBLUE FOREGROUND_BLUE | FOREGROUND_INTENSITY
#define KS_SEQ_BBLUE FOREGROUND_BLUE 
#define KS_SEQ_FBLACK 0 | FOREGROUND_INTENSITY
#define KS_SEQ_BBLACK 0 
#else
#define KS_SEQ_ESC "\033["
/* Ansi Control character suffixes */
#define KS_SEQ_HOME_CHAR 'H'
#define KS_SEQ_HOME_CHAR_STR "H"
#define KS_SEQ_CLEARLINE_CHAR '1'
#define KS_SEQ_CLEARLINE_CHAR_STR "1"
#define KS_SEQ_CLEARLINEEND_CHAR "K"
#define KS_SEQ_CLEARSCR_CHAR0 '2'
#define KS_SEQ_CLEARSCR_CHAR1 'J'
#define KS_SEQ_CLEARSCR_CHAR "2J"
#define KS_SEQ_DEFAULT_COLOR KS_SEQ_ESC KS_SEQ_END_COLOR	/* Reset to Default fg/bg color */
#define KS_SEQ_AND_COLOR ";"	/* To add multiple color definitions */
#define KS_SEQ_END_COLOR "m"	/* To end color definitions */
/* Foreground colors values */
#define KS_SEQ_F_BLACK "30"
#define KS_SEQ_F_RED "31"
#define KS_SEQ_F_GREEN "32"
#define KS_SEQ_F_YELLOW "33"
#define KS_SEQ_F_BLUE "34"
#define KS_SEQ_F_MAGEN "35"
#define KS_SEQ_F_CYAN "36"
#define KS_SEQ_F_WHITE "37"
/* Background colors values */
#define KS_SEQ_B_BLACK "40"
#define KS_SEQ_B_RED "41"
#define KS_SEQ_B_GREEN "42"
#define KS_SEQ_B_YELLOW "43"
#define KS_SEQ_B_BLUE "44"
#define KS_SEQ_B_MAGEN "45"
#define KS_SEQ_B_CYAN "46"
#define KS_SEQ_B_WHITE "47"
/* Preset escape sequences - Change foreground colors only */
#define KS_SEQ_FBLACK KS_SEQ_ESC KS_SEQ_F_BLACK KS_SEQ_END_COLOR
#define KS_SEQ_FRED KS_SEQ_ESC KS_SEQ_F_RED KS_SEQ_END_COLOR
#define KS_SEQ_FGREEN KS_SEQ_ESC KS_SEQ_F_GREEN KS_SEQ_END_COLOR
#define KS_SEQ_FYELLOW KS_SEQ_ESC KS_SEQ_F_YELLOW KS_SEQ_END_COLOR
#define KS_SEQ_FBLUE KS_SEQ_ESC KS_SEQ_F_BLUE KS_SEQ_END_COLOR
#define KS_SEQ_FMAGEN KS_SEQ_ESC KS_SEQ_F_MAGEN KS_SEQ_END_COLOR
#define KS_SEQ_FCYAN KS_SEQ_ESC KS_SEQ_F_CYAN KS_SEQ_END_COLOR
#define KS_SEQ_FWHITE KS_SEQ_ESC KS_SEQ_F_WHITE KS_SEQ_END_COLOR
#define KS_SEQ_BBLACK KS_SEQ_ESC KS_SEQ_B_BLACK KS_SEQ_END_COLOR
#define KS_SEQ_BRED KS_SEQ_ESC KS_SEQ_B_RED KS_SEQ_END_COLOR
#define KS_SEQ_BGREEN KS_SEQ_ESC KS_SEQ_B_GREEN KS_SEQ_END_COLOR
#define KS_SEQ_BYELLOW KS_SEQ_ESC KS_SEQ_B_YELLOW KS_SEQ_END_COLOR
#define KS_SEQ_BBLUE KS_SEQ_ESC KS_SEQ_B_BLUE KS_SEQ_END_COLOR
#define KS_SEQ_BMAGEN KS_SEQ_ESC KS_SEQ_B_MAGEN KS_SEQ_END_COLOR
#define KS_SEQ_BCYAN KS_SEQ_ESC KS_SEQ_B_CYAN KS_SEQ_END_COLOR
#define KS_SEQ_BWHITE KS_SEQ_ESC KS_SEQ_B_WHITE KS_SEQ_END_COLOR
/* Preset escape sequences */
#define KS_SEQ_HOME KS_SEQ_ESC KS_SEQ_HOME_CHAR_STR
#define KS_SEQ_CLEARLINE KS_SEQ_ESC KS_SEQ_CLEARLINE_CHAR_STR
#define KS_SEQ_CLEARLINEEND KS_SEQ_ESC KS_SEQ_CLEARLINEEND_CHAR
#define KS_SEQ_CLEARSCR KS_SEQ_ESC KS_SEQ_CLEARSCR_CHAR KS_SEQ_HOME
#endif

#if !defined(_XOPEN_SOURCE) && !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__OpenBSD__)
#define _XOPEN_SOURCE 600
#endif

#ifndef HAVE_STRINGS_H
#define HAVE_STRINGS_H 1
#endif
#ifndef HAVE_SYS_SOCKET_H
#define HAVE_SYS_SOCKET_H 1
#endif

#ifndef __WINDOWS__
#if defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32)
#define __WINDOWS__
#endif
#endif

#ifdef _MSC_VER
#ifndef __inline__
#define __inline__ __inline
#endif
#if (_MSC_VER >= 1400)			/* VC8+ */
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif
#endif
#ifndef strcasecmp
#define strcasecmp(s1, s2) _stricmp(s1, s2)
#endif
#ifndef strncasecmp
#define strncasecmp(s1, s2, n) _strnicmp(s1, s2, n)
#endif
#ifndef snprintf
#define snprintf _snprintf
#endif
#ifndef S_IRUSR
#define S_IRUSR _S_IREAD
#endif
#ifndef S_IWUSR
#define S_IWUSR _S_IWRITE
#endif
#undef HAVE_STRINGS_H
#undef HAVE_SYS_SOCKET_H
#endif

#include <time.h>
#ifndef WIN32
#include <sys/time.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <sys/types.h>
#include <sys/select.h>
#include <netinet/tcp.h>
#include <sys/signal.h>
#include <unistd.h>
#include <ctype.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <assert.h>

#if (_MSC_VER >= 1400)			// VC8+
#define ks_assert(expr) assert(expr);__analysis_assume( expr )
#endif

#ifndef ks_assert
#define ks_assert(_x) assert(_x)
#endif

#define ks_safe_free(_x) if (_x) free(_x); _x = NULL
#define ks_strlen_zero(s) (!s || *(s) == '\0')
#define ks_strlen_zero_buf(s) (*(s) == '\0')
#define end_of(_s) *(*_s == '\0' ? _s : _s + strlen(_s) - 1)

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
typedef SOCKET ks_socket_t;
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8 uint8_t;
typedef __int64 int64_t;
typedef __int32 int32_t;
typedef __int16 int16_t;
typedef __int8 int8_t;
typedef intptr_t ks_ssize_t;
typedef int ks_filehandle_t;
#define KS_SOCK_INVALID INVALID_SOCKET
#define strerror_r(num, buf, size) strerror_s(buf, size, num)
#if defined(KS_DECLARE_STATIC)
#define KS_DECLARE(type)			type __stdcall
#define KS_DECLARE_NONSTD(type)		type __cdecl
#define KS_DECLARE_DATA
#elif defined(KS_EXPORTS)
#define KS_DECLARE(type)			__declspec(dllexport) type __stdcall
#define KS_DECLARE_NONSTD(type)		__declspec(dllexport) type __cdecl
#define KS_DECLARE_DATA				__declspec(dllexport)
#else
#define KS_DECLARE(type)			__declspec(dllimport) type __stdcall
#define KS_DECLARE_NONSTD(type)		__declspec(dllimport) type __cdecl
#define KS_DECLARE_DATA				__declspec(dllimport)
#endif
#else
#define KS_DECLARE(type) type
#define KS_DECLARE_NONSTD(type) type
#define KS_DECLARE_DATA
#include <stdint.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#define KS_SOCK_INVALID -1
typedef int ks_socket_t;
typedef ssize_t ks_ssize_t;
typedef int ks_filehandle_t;
#endif

#include "math.h"
#include "ks_json.h"

typedef int16_t ks_port_t;
typedef size_t ks_size_t;

typedef enum {
	KS_SUCCESS,
	KS_FAIL,
	KS_BREAK,
	KS_DISCONNECTED,
	KS_GENERR
} ks_status_t;

#define BUF_CHUNK 65536 * 50
#define BUF_START 65536 * 100

#include <ks_threadmutex.h>
#include <ks_buffer.h>

#define ks_test_flag(obj, flag) ((obj)->flags & flag)
#define ks_set_flag(obj, flag) (obj)->flags |= (flag)
#define ks_clear_flag(obj, flag) (obj)->flags &= ~(flag)

/*! \brief Used internally for truth test */
typedef enum {
	KS_TRUE = 1,
	KS_FALSE = 0
} ks_bool_t;

#ifndef __FUNCTION__
#define __FUNCTION__ (const char *)__func__
#endif

#define KS_PRE __FILE__, __FUNCTION__, __LINE__
#define KS_LOG_LEVEL_DEBUG 7
#define KS_LOG_LEVEL_INFO 6
#define KS_LOG_LEVEL_NOTICE 5
#define KS_LOG_LEVEL_WARNING 4
#define KS_LOG_LEVEL_ERROR 3
#define KS_LOG_LEVEL_CRIT 2
#define KS_LOG_LEVEL_ALERT 1
#define KS_LOG_LEVEL_EMERG 0

#define KS_LOG_DEBUG KS_PRE, KS_LOG_LEVEL_DEBUG
#define KS_LOG_INFO KS_PRE, KS_LOG_LEVEL_INFO
#define KS_LOG_NOTICE KS_PRE, KS_LOG_LEVEL_NOTICE
#define KS_LOG_WARNING KS_PRE, KS_LOG_LEVEL_WARNING
#define KS_LOG_ERROR KS_PRE, KS_LOG_LEVEL_ERROR
#define KS_LOG_CRIT KS_PRE, KS_LOG_LEVEL_CRIT
#define KS_LOG_ALERT KS_PRE, KS_LOG_LEVEL_ALERT
#define KS_LOG_EMERG KS_PRE, KS_LOG_LEVEL_EMERG
typedef void (*ks_logger_t)(const char *file, const char *func, int line, int level, const char *fmt, ...);
typedef void (*ks_listen_callback_t)(ks_socket_t server_sock, ks_socket_t client_sock, struct sockaddr_in *addr);


KS_DECLARE(int) ks_vasprintf(char **ret, const char *fmt, va_list ap);

KS_DECLARE_DATA extern ks_logger_t ks_log;

/*! Sets the logger for libks. Default is the null_logger */
KS_DECLARE(void) ks_global_set_logger(ks_logger_t logger);
/*! Sets the default log level for libks */
KS_DECLARE(void) ks_global_set_default_logger(int level);


#include "ks_threadmutex.h"
#include "ks_config.h"
#include "ks_buffer.h"
#include "mpool.h"
#include "simclist.h"
#include "table.h"

KS_DECLARE(size_t) ks_url_encode(const char *url, char *buf, size_t len);
KS_DECLARE(char *)ks_url_decode(char *s);
KS_DECLARE(const char *)ks_stristr(const char *instr, const char *str);
KS_DECLARE(int) ks_toupper(int c);
KS_DECLARE(int) ks_tolower(int c);
KS_DECLARE(int) ks_snprintf(char *buffer, size_t count, const char *fmt, ...);


KS_DECLARE(int) ks_wait_sock(ks_socket_t sock, uint32_t ms, ks_poll_t flags);

KS_DECLARE(unsigned int) ks_separate_string_string(char *buf, const char *delim, char **array, unsigned int arraylen);

#define ks_recv(_h) ks_recv_event(_h, 0, NULL)
#define ks_recv_timed(_h, _ms) ks_recv_event_timed(_h, _ms, 0, NULL)

static __inline__ int ks_safe_strcasecmp(const char *s1, const char *s2)
{
	if (!(s1 && s2)) {
		return 1;
	}

	return strcasecmp(s1, s2);
}

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */


#endif /* defined(_KS_H_) */

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
