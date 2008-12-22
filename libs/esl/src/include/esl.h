/*
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

#ifndef _ESL_H_
#define _ESL_H_

#include <stdarg.h>

#define esl_copy_string(_x, _y, _z) strncpy(_x, _y, _z - 1)
#define esl_set_string(_x, _y) esl_copy_string(_x, _y, sizeof(_x))

typedef struct esl_event_header esl_event_header_t;
typedef struct esl_event esl_event_t;

#define ESL_SEQ_ESC "\033["
/* Ansi Control character suffixes */
#define ESL_SEQ_HOME_CHAR 'H'
#define ESL_SEQ_HOME_CHAR_STR "H"
#define ESL_SEQ_CLEARLINE_CHAR '1'
#define ESL_SEQ_CLEARLINE_CHAR_STR "1"
#define ESL_SEQ_CLEARLINEEND_CHAR "K"
#define ESL_SEQ_CLEARSCR_CHAR0 '2'
#define ESL_SEQ_CLEARSCR_CHAR1 'J'
#define ESL_SEQ_CLEARSCR_CHAR "2J"
#define ESL_SEQ_DEFAULT_COLOR ESL_SEQ_ESC ESL_SEQ_END_COLOR	/* Reset to Default fg/bg color */
#define ESL_SEQ_AND_COLOR ";"	/* To add multiple color definitions */
#define ESL_SEQ_END_COLOR "m"	/* To end color definitions */
/* Foreground colors values */
#define ESL_SEQ_F_BLACK "30"
#define ESL_SEQ_F_RED "31"
#define ESL_SEQ_F_GREEN "32"
#define ESL_SEQ_F_YELLOW "33"
#define ESL_SEQ_F_BLUE "34"
#define ESL_SEQ_F_MAGEN "35"
#define ESL_SEQ_F_CYAN "36"
#define ESL_SEQ_F_WHITE "37"
/* Background colors values */
#define ESL_SEQ_B_BLACK "40"
#define ESL_SEQ_B_RED "41"
#define ESL_SEQ_B_GREEN "42"
#define ESL_SEQ_B_YELLOW "43"
#define ESL_SEQ_B_BLUE "44"
#define ESL_SEQ_B_MAGEN "45"
#define ESL_SEQ_B_CYAN "46"
#define ESL_SEQ_B_WHITE "47"
/* Preset escape sequences - Change foreground colors only */
#define ESL_SEQ_FBLACK ESL_SEQ_ESC ESL_SEQ_F_BLACK ESL_SEQ_END_COLOR
#define ESL_SEQ_FRED ESL_SEQ_ESC ESL_SEQ_F_RED ESL_SEQ_END_COLOR
#define ESL_SEQ_FGREEN ESL_SEQ_ESC ESL_SEQ_F_GREEN ESL_SEQ_END_COLOR
#define ESL_SEQ_FYELLOW ESL_SEQ_ESC ESL_SEQ_F_YELLOW ESL_SEQ_END_COLOR
#define ESL_SEQ_FBLUE ESL_SEQ_ESC ESL_SEQ_F_BLUE ESL_SEQ_END_COLOR
#define ESL_SEQ_FMAGEN ESL_SEQ_ESC ESL_SEQ_F_MAGEN ESL_SEQ_END_COLOR
#define ESL_SEQ_FCYAN ESL_SEQ_ESC ESL_SEQ_F_CYAN ESL_SEQ_END_COLOR
#define ESL_SEQ_FWHITE ESL_SEQ_ESC ESL_SEQ_F_WHITE ESL_SEQ_END_COLOR
#define ESL_SEQ_BBLACK ESL_SEQ_ESC ESL_SEQ_B_BLACK ESL_SEQ_END_COLOR
#define ESL_SEQ_BRED ESL_SEQ_ESC ESL_SEQ_B_RED ESL_SEQ_END_COLOR
#define ESL_SEQ_BGREEN ESL_SEQ_ESC ESL_SEQ_B_GREEN ESL_SEQ_END_COLOR
#define ESL_SEQ_BYELLOW ESL_SEQ_ESC ESL_SEQ_B_YELLOW ESL_SEQ_END_COLOR
#define ESL_SEQ_BBLUE ESL_SEQ_ESC ESL_SEQ_B_BLUE ESL_SEQ_END_COLOR
#define ESL_SEQ_BMAGEN ESL_SEQ_ESC ESL_SEQ_B_MAGEN ESL_SEQ_END_COLOR
#define ESL_SEQ_BCYAN ESL_SEQ_ESC ESL_SEQ_B_CYAN ESL_SEQ_END_COLOR
#define ESL_SEQ_BWHITE ESL_SEQ_ESC ESL_SEQ_B_WHITE ESL_SEQ_END_COLOR
/* Preset escape sequences */
#define ESL_SEQ_HOME ESL_SEQ_ESC ESL_SEQ_HOME_CHAR_STR
#define ESL_SEQ_CLEARLINE ESL_SEQ_ESC ESL_SEQ_CLEARLINE_CHAR_STR
#define ESL_SEQ_CLEARLINEEND ESL_SEQ_ESC ESL_SEQ_CLEARLINEEND_CHAR
#define ESL_SEQ_CLEARSCR ESL_SEQ_ESC ESL_SEQ_CLEARSCR_CHAR ESL_SEQ_HOME

#if !defined(_XOPEN_SOURCE) && !defined(__FreeBSD__) && !defined(__NetBSD__)
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
#include <sys/signal.h>
#include <unistd.h>
#include <ctype.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <assert.h>

#if (_MSC_VER >= 1400)			// VC8+
#define esl_assert(expr) assert(expr);__analysis_assume( expr )
#endif

#ifndef esl_assert
#define esl_assert(_x) assert(_x)
#endif

#define esl_safe_free(_x) if (_x) free(_x); _x = NULL
#define esl_strlen_zero(s) (!s || *(s) == '\0')
#define esl_strlen_zero_buf(s) (*(s) == '\0')

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
typedef SOCKET esl_socket_t;
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8 uint8_t;
typedef __int64 int64_t;
typedef __int32 int32_t;
typedef __int16 int16_t;
typedef __int8 int8_t;
typedef intptr_t esl_ssize_t;
typedef int esl_filehandle_t;
#define ESL_SOCK_INVALID INVALID_SOCKET
#define strerror_r(num, buf, size) strerror_s(buf, size, num)
#if defined(ESL_DECLARE_STATIC)
#define ESL_DECLARE(type)			type __stdcall
#define ESL_DECLARE_NONSTD(type)		type __cdecl
#define ESL_DECLARE_DATA
#elif defined(ESL_EXPORTS)
#define ESL_DECLARE(type)			__declspec(dllexport) type __stdcall
#define ESL_DECLARE_NONSTD(type)		__declspec(dllexport) type __cdecl
#define ESL_DECLARE_DATA				__declspec(dllexport)
#else
#define ESL_DECLARE(type)			__declspec(dllimport) type __stdcall
#define ESL_DECLARE_NONSTD(type)		__declspec(dllimport) type __cdecl
#define ESL_DECLARE_DATA				__declspec(dllimport)
#endif
#else
#define ESL_DECLARE(type) type
#define ESL_DECLARE_NONSTD(type) type
#define ESL_DECLARE_DATA
#include <stdint.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#define ESL_SOCK_INVALID -1
typedef int esl_socket_t;
typedef ssize_t esl_ssize_t;
typedef int esl_filehandle_t;
#endif

typedef int16_t esl_port_t;

typedef enum {
	ESL_SUCCESS,
	ESL_FAIL,
	ESL_BREAK
} esl_status_t;

#include <esl_threadmutex.h>

typedef struct {
	struct sockaddr_in sockaddr;
	struct hostent hostent;
	char hostbuf[256];
	esl_socket_t sock;
	char err[256];
	int errnum;
	char header_buf[4196];
	char last_reply[1024];
	char last_sr_reply[1024];
	esl_event_t *last_event;
	esl_event_t *last_sr_event;
	esl_event_t *last_ievent;
	esl_event_t *info_event;
	int debug;
	int connected;
	struct sockaddr_in addr;
	esl_mutex_t *mutex;
} esl_handle_t;

typedef enum {
	ESL_TRUE = 1,
	ESL_FALSE = 0
} esl_bool_t;

#ifndef __FUNCTION__
#define __FUNCTION__ (const char *)__func__
#endif

#define ESL_PRE __FILE__, __FUNCTION__, __LINE__
#define ESL_LOG_LEVEL_DEBUG 7
#define ESL_LOG_LEVEL_INFO 6
#define ESL_LOG_LEVEL_NOTICE 5
#define ESL_LOG_LEVEL_WARNING 4
#define ESL_LOG_LEVEL_ERROR 3
#define ESL_LOG_LEVEL_CRIT 2
#define ESL_LOG_LEVEL_ALERT 1
#define ESL_LOG_LEVEL_EMERG 0

#define ESL_LOG_DEBUG ESL_PRE, ESL_LOG_LEVEL_DEBUG
#define ESL_LOG_INFO ESL_PRE, ESL_LOG_LEVEL_INFO
#define ESL_LOG_NOTICE ESL_PRE, ESL_LOG_LEVEL_NOTICE
#define ESL_LOG_WARNING ESL_PRE, ESL_LOG_LEVEL_WARNING
#define ESL_LOG_ERROR ESL_PRE, ESL_LOG_LEVEL_ERROR
#define ESL_LOG_CRIT ESL_PRE, ESL_LOG_LEVEL_CRIT
#define ESL_LOG_ALERT ESL_PRE, ESL_LOG_LEVEL_ALERT
#define ESL_LOG_EMERG ESL_PRE, ESL_LOG_LEVEL_EMERG
typedef void (*esl_logger_t)(const char *file, const char *func, int line, int level, const char *fmt, ...);

extern esl_logger_t esl_log;

void esl_global_set_logger(esl_logger_t logger);
void esl_global_set_default_logger(int level);

#include "esl_event.h"
#include "esl_threadmutex.h"
#include "esl_config.h"

ESL_DECLARE(size_t) esl_url_encode(const char *url, char *buf, size_t len);
ESL_DECLARE(char *)esl_url_decode(char *s);
ESL_DECLARE(const char *)esl_stristr(const char *instr, const char *str);
ESL_DECLARE(int) esl_toupper(int c);
ESL_DECLARE(int) esl_tolower(int c);
ESL_DECLARE(int) esl_snprintf(char *buffer, size_t count, const char *fmt, ...);


typedef void (*esl_listen_callback_t)(esl_socket_t server_sock, esl_socket_t client_sock, struct sockaddr_in addr);

ESL_DECLARE(esl_status_t) esl_attach_handle(esl_handle_t *handle, esl_socket_t socket, struct sockaddr_in addr);
ESL_DECLARE(esl_status_t) esl_listen(const char *host, esl_port_t port, esl_listen_callback_t callback);
ESL_DECLARE(esl_status_t) esl_execute(esl_handle_t *handle, const char *app, const char *arg, const char *uuid);
ESL_DECLARE(esl_status_t) esl_sendevent(esl_handle_t *handle, esl_event_t *event);

ESL_DECLARE(esl_status_t) esl_connect(esl_handle_t *handle, const char *host, esl_port_t port, const char *password);
ESL_DECLARE(esl_status_t) esl_disconnect(esl_handle_t *handle);
ESL_DECLARE(esl_status_t) esl_send(esl_handle_t *handle, const char *cmd);
ESL_DECLARE(esl_status_t) esl_recv_event(esl_handle_t *handle, esl_event_t **save_event);
ESL_DECLARE(esl_status_t) esl_recv_event_timed(esl_handle_t *handle, uint32_t ms, esl_event_t **save_event);
ESL_DECLARE(esl_status_t) esl_send_recv(esl_handle_t *handle, const char *cmd);
#define esl_recv(_h) esl_recv_event(_h, NULL)
#define esl_recv_timed(_h, _ms) esl_recv_event_timed(_h, _ms, NULL)

#endif



