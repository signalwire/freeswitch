/*
 * Copyright (c) 2012-2013, Anthony Minessale II
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

#ifndef _SCGI_H_
#define _SCGI_H_

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */
#if EMACS_BUG
}
#endif

#ifdef _MSC_VER
#define FD_SETSIZE 8192
#define SCGI_USE_SELECT
#else 
#define SCGI_USE_POLL
#endif



#ifdef SCGI_USE_POLL
#include <poll.h>
#endif

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
typedef SOCKET scgi_socket_t;
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8 uint8_t;
typedef __int64 int64_t;
typedef __int32 int32_t;
typedef __int16 int16_t;
typedef __int8 int8_t;
typedef intptr_t scgi_ssize_t;
typedef int scgi_filehandle_t;
#define SCGI_SOCK_INVALID INVALID_SOCKET
#define strerror_r(num, buf, size) strerror_s(buf, size, num)
#if defined(SCGI_DECLARE_STATIC)
#define SCGI_DECLARE(type)			type __stdcall
#define SCGI_DECLARE_NONSTD(type)		type __cdecl
#define SCGI_DECLARE_DATA
#elif defined(SCGI_EXPORTS)
#define SCGI_DECLARE(type)			__declspec(dllexport) type __stdcall
#define SCGI_DECLARE_NONSTD(type)		__declspec(dllexport) type __cdecl
#define SCGI_DECLARE_DATA				__declspec(dllexport)
#else
#define SCGI_DECLARE(type)			__declspec(dllimport) type __stdcall
#define SCGI_DECLARE_NONSTD(type)		__declspec(dllimport) type __cdecl
#define SCGI_DECLARE_DATA				__declspec(dllimport)
#endif
#else
#define SCGI_DECLARE(type) type
#define SCGI_DECLARE_NONSTD(type) type
#define SCGI_DECLARE_DATA
#include <stdint.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#define SCGI_SOCK_INVALID -1
typedef int scgi_socket_t;
typedef ssize_t scgi_ssize_t;
typedef int scgi_filehandle_t;
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
#define scgi_assert(expr) assert(expr);__analysis_assume( expr )
#endif

#ifndef scgi_assert
#define scgi_assert(_x) assert(_x)
#endif

#define scgi_safe_free(_x) if (_x) free(_x); _x = NULL
#define scgi_strlen_zero(s) (!s || *(s) == '\0')
#define scgi_strlen_zero_buf(s) (*(s) == '\0')
#define end_of(_s) *(*_s == '\0' ? _s : _s + strlen(_s) - 1)
#define end_of_p(_s) (*_s == '\0' ? _s : _s + strlen(_s) - 1)

typedef enum {
	SCGI_POLL_READ = (1 << 0),
	SCGI_POLL_WRITE = (1 << 1),
	SCGI_POLL_ERROR = (1 << 2)
} scgi_poll_t;


typedef struct scgi_param_s {
	char *name;
	char *value;
	struct scgi_param_s *next;
} scgi_param_t;

typedef struct scgi_handle_s {
	scgi_param_t *params;
	char *body;
	struct sockaddr_in sockaddr;
	struct hostent hostent;
	char hostbuf[256];
	scgi_socket_t sock;
	char err[256];
	int errnum;
	int connected;
	struct sockaddr_in addr;
	int destroyed;
} scgi_handle_t;


typedef int16_t scgi_port_t;
typedef size_t scgi_size_t;

typedef enum {
	SCGI_SUCCESS,
	SCGI_FAIL,
	SCGI_BREAK,
	SCGI_DISCONNECTED,
	SCGI_GENERR
} scgi_status_t;

typedef void (*scgi_listen_callback_t)(scgi_socket_t server_sock, scgi_socket_t *client_sock, struct sockaddr_in *addr);

SCGI_DECLARE(scgi_status_t) scgi_connect(scgi_handle_t *handle, const char *host, scgi_port_t port, uint32_t timeout);
SCGI_DECLARE(scgi_status_t) scgi_disconnect(scgi_handle_t *handle);
SCGI_DECLARE(scgi_status_t) scgi_parse(scgi_socket_t sock, scgi_handle_t *handle);
SCGI_DECLARE(int) scgi_wait_sock(scgi_socket_t sock, uint32_t ms, scgi_poll_t flags);
SCGI_DECLARE(ssize_t) scgi_recv(scgi_handle_t *handle, unsigned char *buf, size_t buflen);
SCGI_DECLARE(scgi_status_t) scgi_send_request(scgi_handle_t *handle);
SCGI_DECLARE(scgi_status_t) scgi_add_param(scgi_handle_t *handle, const char *name, const char *value);
SCGI_DECLARE(scgi_status_t) scgi_add_body(scgi_handle_t *handle, const char *value);
SCGI_DECLARE(size_t) scgi_build_message(scgi_handle_t *handle, char **buffer);
SCGI_DECLARE(scgi_status_t) scgi_destroy_params(scgi_handle_t *handle);
SCGI_DECLARE(scgi_status_t) scgi_listen(const char *host, scgi_port_t port, scgi_listen_callback_t callback);
SCGI_DECLARE(const char *) scgi_get_body(scgi_handle_t *handle);
SCGI_DECLARE(const char *) scgi_get_param(scgi_handle_t *handle, const char *name);
SCGI_DECLARE(scgi_status_t) scgi_bind(const char *host, scgi_port_t port, scgi_socket_t *socketp);
SCGI_DECLARE(scgi_status_t) scgi_accept(scgi_socket_t server_sock, scgi_socket_t *client_sock_p, struct sockaddr_in *echoClntAddr);

#ifndef WIN32
#define closesocket(x) close(x)
#endif

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */


#endif /* defined(_SCGI_H_) */

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
