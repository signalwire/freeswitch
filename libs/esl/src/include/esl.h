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

#ifndef _ESL_H_
#define _ESL_H_

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

#define esl_copy_string(_x, _y, _z) strncpy(_x, _y, _z - 1)
#define esl_set_string(_x, _y) esl_copy_string(_x, _y, sizeof(_x))
#define ESL_VA_NONE "%s", ""

typedef struct esl_event_header esl_event_header_t;
typedef struct esl_event esl_event_t;

typedef enum {
	ESL_POLL_READ = (1 << 0),
	ESL_POLL_WRITE = (1 << 1),
	ESL_POLL_ERROR = (1 << 2)
} esl_poll_t;

typedef enum {
	ESL_EVENT_TYPE_PLAIN,
	ESL_EVENT_TYPE_XML,
	ESL_EVENT_TYPE_JSON
} esl_event_type_t;

#ifdef WIN32
#define ESL_SEQ_FWHITE FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY
#define ESL_SEQ_BWHITE FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE
#define ESL_SEQ_FRED FOREGROUND_RED | FOREGROUND_INTENSITY
#define ESL_SEQ_BRED FOREGROUND_RED
#define ESL_SEQ_FMAGEN FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY
#define ESL_SEQ_BMAGEN FOREGROUND_BLUE | FOREGROUND_RED
#define ESL_SEQ_FCYAN FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY
#define ESL_SEQ_BCYAN FOREGROUND_GREEN | FOREGROUND_BLUE
#define ESL_SEQ_FGREEN FOREGROUND_GREEN | FOREGROUND_INTENSITY
#define ESL_SEQ_BGREEN FOREGROUND_GREEN 
#define ESL_SEQ_FYELLOW FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY
#define ESL_SEQ_BYELLOW FOREGROUND_RED | FOREGROUND_GREEN
#define ESL_SEQ_DEFAULT_COLOR ESL_SEQ_FWHITE
#define ESL_SEQ_FBLUE FOREGROUND_BLUE | FOREGROUND_INTENSITY
#define ESL_SEQ_BBLUE FOREGROUND_BLUE 
#define ESL_SEQ_FBLACK 0 | FOREGROUND_INTENSITY
#define ESL_SEQ_BBLACK 0 
#else
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
#define esl_assert(expr) assert(expr);__analysis_assume( expr )
#endif

#ifndef esl_assert
#define esl_assert(_x) assert(_x)
#endif

#define esl_safe_free(_x) if (_x) free(_x); _x = NULL
#define esl_strlen_zero(s) (!s || *(s) == '\0')
#define esl_strlen_zero_buf(s) (*(s) == '\0')
#define end_of(_s) *(*_s == '\0' ? _s : _s + strlen(_s) - 1)

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
typedef SOCKET esl_socket_t;
#if !defined(_STDINT) && !defined(uint32_t)
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8 uint8_t;
typedef __int64 int64_t;
typedef __int32 int32_t;
typedef __int16 int16_t;
typedef __int8 int8_t;
#endif
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

#include "math.h"
#include "esl_json.h"

typedef int16_t esl_port_t;
typedef size_t esl_size_t;

typedef enum {
	ESL_SUCCESS,
	ESL_FAIL,
	ESL_BREAK,
	ESL_DISCONNECTED,
	ESL_GENERR
} esl_status_t;

#define BUF_CHUNK 65536 * 50
#define BUF_START 65536 * 100

#include <esl_threadmutex.h>
#include <esl_buffer.h>

/*! \brief A handle that will hold the socket information and
           different events received. */
typedef struct {
	struct sockaddr_storage sockaddr;
	struct hostent hostent;
	char hostbuf[256];
	esl_socket_t sock;
	/*! In case of socket error, this will hold the error description as reported by the OS */
	char err[256];
	/*! The error number reported by the OS */
	int errnum;
	/*! The inner contents received by the socket. Used only internally. */
	esl_buffer_t *packet_buf;
	char socket_buf[65536];
	/*! Last command reply */
	char last_reply[1024];
	/*! Las command reply when called with esl_send_recv */
	char last_sr_reply[1024];
	/*! Last event received. Only populated when **save_event is NULL */
	esl_event_t *last_event;
	/*! Last event received when called by esl_send_recv */
	esl_event_t *last_sr_event;
	/*! This will hold already processed events queued by esl_recv_event */
	esl_event_t *race_event;
	/*! Events that have content-type == text/plain and a body */
	esl_event_t *last_ievent;
	/*! For outbound socket. Will hold reply information when connect\n\n is sent */
	esl_event_t *info_event;
	/*! Socket is connected or not */
	int connected;
	struct sockaddr_in addr;
	/*! Internal mutex */
	esl_mutex_t *mutex;
	int async_execute;
	int event_lock;
	int destroyed;
} esl_handle_t;

#define esl_test_flag(obj, flag) ((obj)->flags & flag)
#define esl_set_flag(obj, flag) (obj)->flags |= (flag)
#define esl_clear_flag(obj, flag) (obj)->flags &= ~(flag)

/*! \brief Used internally for truth test */
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


ESL_DECLARE(int) esl_vasprintf(char **ret, const char *fmt, va_list ap);

ESL_DECLARE_DATA extern esl_logger_t esl_log;

/*! Sets the logger for libesl. Default is the null_logger */
ESL_DECLARE(void) esl_global_set_logger(esl_logger_t logger);
/*! Sets the default log level for libesl */
ESL_DECLARE(void) esl_global_set_default_logger(int level);

#include "esl_event.h"
#include "esl_threadmutex.h"
#include "esl_config.h"

ESL_DECLARE(size_t) esl_url_encode(const char *url, char *buf, size_t len);
ESL_DECLARE(char *)esl_url_decode(char *s);
ESL_DECLARE(const char *)esl_stristr(const char *instr, const char *str);
ESL_DECLARE(int) esl_toupper(int c);
ESL_DECLARE(int) esl_tolower(int c);
ESL_DECLARE(int) esl_snprintf(char *buffer, size_t count, const char *fmt, ...);


typedef void (*esl_listen_callback_t)(esl_socket_t server_sock, esl_socket_t client_sock, struct sockaddr_in *addr);
/*!
    \brief Attach a handle to an established socket connection
    \param handle Handle to be attached
    \param socket Socket to which the handle will be attached
    \param addr Structure that will contain the connection descritption (look up your os info)
*/
ESL_DECLARE(esl_status_t) esl_attach_handle(esl_handle_t *handle, esl_socket_t socket, struct sockaddr_in *addr);
/*!
    \brief Will bind to host and callback when event is received. Used for outbound socket.
    \param host Host to bind to
    \param port Port to bind to
    \param callback Callback that will be called upon data received
*/

ESL_DECLARE(esl_status_t) esl_listen(const char *host, esl_port_t port, esl_listen_callback_t callback, esl_socket_t *server_sockP);
ESL_DECLARE(esl_status_t) esl_listen_threaded(const char *host, esl_port_t port, esl_listen_callback_t callback, int max);
/*!
    \brief Executes application with sendmsg to a specific UUID. Used for outbound socket.
    \param handle Handle that the msg will be sent
    \param app Application to execute
    \param arg Application arguments
    \param uuid Target UUID for the application
*/
ESL_DECLARE(esl_status_t) esl_execute(esl_handle_t *handle, const char *app, const char *arg, const char *uuid);
/*!
    \brief Send an event
    \param handle Handle to which the event should be sent
    \param event Event to be sent
*/
ESL_DECLARE(esl_status_t) esl_sendevent(esl_handle_t *handle, esl_event_t *event);

/*!
    \brief Send an event as a message to be parsed
    \param handle Handle to which the event should be sent
    \param event Event to be sent
	\param uuid a specific uuid if not the default
*/
ESL_DECLARE(esl_status_t) esl_sendmsg(esl_handle_t *handle, esl_event_t *event, const char *uuid);

/*!
    \brief Connect a handle to a host/port with a specific password. This will also authenticate against the server
    \param handle Handle to connect
    \param host Host to be connected
    \param port Port to be connected
    \param password FreeSWITCH server username (optional)
    \param password FreeSWITCH server password
	\param timeout Connection timeout, in miliseconds
*/
ESL_DECLARE(esl_status_t) esl_connect_timeout(esl_handle_t *handle, const char *host, esl_port_t port, const char *user, const char *password, uint32_t timeout);
#define esl_connect(_handle, _host, _port, _user, _password) esl_connect_timeout(_handle, _host, _port, _user, _password, 0)

/*!
    \brief Disconnect a handle
    \param handle Handle to be disconnected
*/
ESL_DECLARE(esl_status_t) esl_disconnect(esl_handle_t *handle);
/*!
    \brief Send a raw command using specific handle
    \param handle Handle to send the command to
    \param cmd Command to send
*/
ESL_DECLARE(esl_status_t) esl_send(esl_handle_t *handle, const char *cmd);
/*!
    \brief Poll the handle's socket until an event is received or a connection error occurs 
    \param handle Handle to poll
    \param check_q If set to 1, will check the handle queue (handle->race_event) and return the last event from it
    \param[out] save_event If this is not NULL, will return the event received
*/
ESL_DECLARE(esl_status_t) esl_recv_event(esl_handle_t *handle, int check_q, esl_event_t **save_event);
/*!
    \brief Poll the handle's socket until an event is received, a connection error occurs or ms expires
    \param handle Handle to poll
    \param ms Maximum time to poll
    \param check_q If set to 1, will check the handle queue (handle->race_event) and return the last event from it
    \param[out] save_event If this is not NULL, will return the event received
*/
ESL_DECLARE(esl_status_t) esl_recv_event_timed(esl_handle_t *handle, uint32_t ms, int check_q, esl_event_t **save_event);
/*!
    \brief This will send a command and place its response event on handle->last_sr_event and handle->last_sr_reply
    \param handle Handle to be used
    \param cmd Raw command to send 
*/
ESL_DECLARE(esl_status_t) esl_send_recv_timed(esl_handle_t *handle, const char *cmd, uint32_t ms);
#define esl_send_recv(_handle, _cmd) esl_send_recv_timed(_handle, _cmd, 0)
/*!
    \brief Applies a filter to received events
    \param handle Handle to apply the filter to
    \param header Header that the filter will be based on
    \param value The value of the header to filter
*/
ESL_DECLARE(esl_status_t) esl_filter(esl_handle_t *handle, const char *header, const char *value);
/*!
    \brief Will subscribe to events on the server
    \param handle Handle to which we will subscribe to events
    \param etype Event type to subscribe
    \param value Which event to subscribe to 
*/
ESL_DECLARE(esl_status_t) esl_events(esl_handle_t *handle, esl_event_type_t etype, const char *value);

ESL_DECLARE(int) esl_wait_sock(esl_socket_t sock, uint32_t ms, esl_poll_t flags);

ESL_DECLARE(unsigned int) esl_separate_string_string(char *buf, const char *delim, char **array, unsigned int arraylen);

#define esl_recv(_h) esl_recv_event(_h, 0, NULL)
#define esl_recv_timed(_h, _ms) esl_recv_event_timed(_h, _ms, 0, NULL)

static __inline__ int esl_safe_strcasecmp(const char *s1, const char *s2)
{
	if (!(s1 && s2)) {
		return 1;
	}

	return strcasecmp(s1, s2);
}

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */


#endif /* defined(_ESL_H_) */

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
