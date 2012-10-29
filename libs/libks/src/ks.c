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


/* Use select on windows and poll everywhere else.
   Select is the devil.  Especially if you are doing a lot of small socket connections.
   If your FD number is bigger than 1024 you will silently create memory corruption.

   If you have build errors on your platform because you don't have poll find a way to detect it and #define KS_USE_SELECT and #undef KS_USE_POLL
   All of this will be upgraded to autoheadache eventually.
*/

/* TBD for win32 figure out how to tell if you have WSAPoll (vista or higher) and use it when available by #defining KS_USE_WSAPOLL (see below) */

#ifdef _MSC_VER
#define FD_SETSIZE 8192
#define KS_USE_SELECT
#else 
#define KS_USE_POLL
#endif

#include <ks.h>
#ifndef WIN32
#define closesocket(x) shutdown(x, 2); close(x)
#include <fcntl.h>
#include <errno.h>
#else
#pragma warning (disable:6386)
/* These warnings need to be ignored warning in sdk header */
#include <Ws2tcpip.h>
#include <windows.h>
#ifndef errno
#define errno WSAGetLastError()
#endif
#ifndef EINTR
#define EINTR WSAEINTR
#endif
#pragma warning (default:6386)
#endif

#ifdef KS_USE_POLL
#include <poll.h>
#endif

#ifndef KS_MIN
#define KS_MIN(x,y)	((x) < (y) ? (x) : (y))
#endif
#ifndef KS_MAX
#define KS_MAX(x,y)	((x) > (y) ? (x) : (y))
#endif
#ifndef KS_CLAMP
#define KS_CLAMP(min,max,val)	(KS_MIN(max,KS_MAX(val,min)))
#endif


/* Written by Marc Espie, public domain */
#define KS_CTYPE_NUM_CHARS       256

const short _ks_C_toupper_[1 + KS_CTYPE_NUM_CHARS] = {
	EOF,
	0x00,	0x01,	0x02,	0x03,	0x04,	0x05,	0x06,	0x07,
	0x08,	0x09,	0x0a,	0x0b,	0x0c,	0x0d,	0x0e,	0x0f,
	0x10,	0x11,	0x12,	0x13,	0x14,	0x15,	0x16,	0x17,
	0x18,	0x19,	0x1a,	0x1b,	0x1c,	0x1d,	0x1e,	0x1f,
	0x20,	0x21,	0x22,	0x23,	0x24,	0x25,	0x26,	0x27,
	0x28,	0x29,	0x2a,	0x2b,	0x2c,	0x2d,	0x2e,	0x2f,
	0x30,	0x31,	0x32,	0x33,	0x34,	0x35,	0x36,	0x37,
	0x38,	0x39,	0x3a,	0x3b,	0x3c,	0x3d,	0x3e,	0x3f,
	0x40,	0x41,	0x42,	0x43,	0x44,	0x45,	0x46,	0x47,
	0x48,	0x49,	0x4a,	0x4b,	0x4c,	0x4d,	0x4e,	0x4f,
	0x50,	0x51,	0x52,	0x53,	0x54,	0x55,	0x56,	0x57,
	0x58,	0x59,	0x5a,	0x5b,	0x5c,	0x5d,	0x5e,	0x5f,
	0x60,	'A',	'B',	'C',	'D',	'E',	'F',	'G',
	'H',	'I',	'J',	'K',	'L',	'M',	'N',	'O',
	'P',	'Q',	'R',	'S',	'T',	'U',	'V',	'W',
	'X',	'Y',	'Z',	0x7b,	0x7c,	0x7d,	0x7e,	0x7f,
	0x80,	0x81,	0x82,	0x83,	0x84,	0x85,	0x86,	0x87,
	0x88,	0x89,	0x8a,	0x8b,	0x8c,	0x8d,	0x8e,	0x8f,
	0x90,	0x91,	0x92,	0x93,	0x94,	0x95,	0x96,	0x97,
	0x98,	0x99,	0x9a,	0x9b,	0x9c,	0x9d,	0x9e,	0x9f,
	0xa0,	0xa1,	0xa2,	0xa3,	0xa4,	0xa5,	0xa6,	0xa7,
	0xa8,	0xa9,	0xaa,	0xab,	0xac,	0xad,	0xae,	0xaf,
	0xb0,	0xb1,	0xb2,	0xb3,	0xb4,	0xb5,	0xb6,	0xb7,
	0xb8,	0xb9,	0xba,	0xbb,	0xbc,	0xbd,	0xbe,	0xbf,
	0xc0,	0xc1,	0xc2,	0xc3,	0xc4,	0xc5,	0xc6,	0xc7,
	0xc8,	0xc9,	0xca,	0xcb,	0xcc,	0xcd,	0xce,	0xcf,
	0xd0,	0xd1,	0xd2,	0xd3,	0xd4,	0xd5,	0xd6,	0xd7,
	0xd8,	0xd9,	0xda,	0xdb,	0xdc,	0xdd,	0xde,	0xdf,
	0xe0,	0xe1,	0xe2,	0xe3,	0xe4,	0xe5,	0xe6,	0xe7,
	0xe8,	0xe9,	0xea,	0xeb,	0xec,	0xed,	0xee,	0xef,
	0xf0,	0xf1,	0xf2,	0xf3,	0xf4,	0xf5,	0xf6,	0xf7,
	0xf8,	0xf9,	0xfa,	0xfb,	0xfc,	0xfd,	0xfe,	0xff
};

const short *_ks_toupper_tab_ = _ks_C_toupper_;

KS_DECLARE(int) ks_toupper(int c)
{
	if ((unsigned int)c > 255)
		return(c);
	if (c < -1)
		return EOF;
	return((_ks_toupper_tab_ + 1)[c]);
}

const short _ks_C_tolower_[1 + KS_CTYPE_NUM_CHARS] = {
	EOF,
	0x00,	0x01,	0x02,	0x03,	0x04,	0x05,	0x06,	0x07,
	0x08,	0x09,	0x0a,	0x0b,	0x0c,	0x0d,	0x0e,	0x0f,
	0x10,	0x11,	0x12,	0x13,	0x14,	0x15,	0x16,	0x17,
	0x18,	0x19,	0x1a,	0x1b,	0x1c,	0x1d,	0x1e,	0x1f,
	0x20,	0x21,	0x22,	0x23,	0x24,	0x25,	0x26,	0x27,
	0x28,	0x29,	0x2a,	0x2b,	0x2c,	0x2d,	0x2e,	0x2f,
	0x30,	0x31,	0x32,	0x33,	0x34,	0x35,	0x36,	0x37,
	0x38,	0x39,	0x3a,	0x3b,	0x3c,	0x3d,	0x3e,	0x3f,
	0x40,	'a',	'b',	'c',	'd',	'e',	'f',	'g',
	'h',	'i',	'j',	'k',	'l',	'm',	'n',	'o',
	'p',	'q',	'r',	's',	't',	'u',	'v',	'w',
	'x',	'y',	'z',	0x5b,	0x5c,	0x5d,	0x5e,	0x5f,
	0x60,	0x61,	0x62,	0x63,	0x64,	0x65,	0x66,	0x67,
	0x68,	0x69,	0x6a,	0x6b,	0x6c,	0x6d,	0x6e,	0x6f,
	0x70,	0x71,	0x72,	0x73,	0x74,	0x75,	0x76,	0x77,
	0x78,	0x79,	0x7a,	0x7b,	0x7c,	0x7d,	0x7e,	0x7f,
	0x80,	0x81,	0x82,	0x83,	0x84,	0x85,	0x86,	0x87,
	0x88,	0x89,	0x8a,	0x8b,	0x8c,	0x8d,	0x8e,	0x8f,
	0x90,	0x91,	0x92,	0x93,	0x94,	0x95,	0x96,	0x97,
	0x98,	0x99,	0x9a,	0x9b,	0x9c,	0x9d,	0x9e,	0x9f,
	0xa0,	0xa1,	0xa2,	0xa3,	0xa4,	0xa5,	0xa6,	0xa7,
	0xa8,	0xa9,	0xaa,	0xab,	0xac,	0xad,	0xae,	0xaf,
	0xb0,	0xb1,	0xb2,	0xb3,	0xb4,	0xb5,	0xb6,	0xb7,
	0xb8,	0xb9,	0xba,	0xbb,	0xbc,	0xbd,	0xbe,	0xbf,
	0xc0,	0xc1,	0xc2,	0xc3,	0xc4,	0xc5,	0xc6,	0xc7,
	0xc8,	0xc9,	0xca,	0xcb,	0xcc,	0xcd,	0xce,	0xcf,
	0xd0,	0xd1,	0xd2,	0xd3,	0xd4,	0xd5,	0xd6,	0xd7,
	0xd8,	0xd9,	0xda,	0xdb,	0xdc,	0xdd,	0xde,	0xdf,
	0xe0,	0xe1,	0xe2,	0xe3,	0xe4,	0xe5,	0xe6,	0xe7,
	0xe8,	0xe9,	0xea,	0xeb,	0xec,	0xed,	0xee,	0xef,
	0xf0,	0xf1,	0xf2,	0xf3,	0xf4,	0xf5,	0xf6,	0xf7,
	0xf8,	0xf9,	0xfa,	0xfb,	0xfc,	0xfd,	0xfe,	0xff
};

const short *_ks_tolower_tab_ = _ks_C_tolower_;

KS_DECLARE(int) ks_tolower(int c)
{
	if ((unsigned int)c > 255)
		return(c);
	if (c < -1)
		return EOF;
	return((_ks_tolower_tab_ + 1)[c]);
}

KS_DECLARE(const char *)ks_stristr(const char *instr, const char *str)
{
/*
** Rev History:  16/07/97  Greg Thayer		Optimized
**               07/04/95  Bob Stout		ANSI-fy
**               02/03/94  Fred Cole		Original
**               09/01/03  Bob Stout		Bug fix (lines 40-41) per Fred Bulback
**
** Hereby donated to public domain.
*/
	const char *pptr, *sptr, *start;

	if (!str || !instr)
		return NULL;

	for (start = str; *start; start++) {
		/* find start of pattern in string */
		for (; ((*start) && (ks_toupper(*start) != ks_toupper(*instr))); start++);

		if (!*start)
			return NULL;

		pptr = instr;
		sptr = start;

		while (ks_toupper(*sptr) == ks_toupper(*pptr)) {
			sptr++;
			pptr++;

			/* if end of pattern then pattern was found */
			if (!*pptr)
				return (start);

			if (!*sptr)
				return NULL;
		}
	}
	return NULL;
}

#ifdef WIN32
#ifndef vsnprintf
#define vsnprintf _vsnprintf
#endif
#endif


int vasprintf(char **ret, const char *format, va_list ap);

KS_DECLARE(int) ks_vasprintf(char **ret, const char *fmt, va_list ap)
{
#if !defined(WIN32) && !defined(__sun)
	return vasprintf(ret, fmt, ap);
#else
	char *buf;
	int len;
	size_t buflen;
	va_list ap2;
	char *tmp = NULL;

#ifdef _MSC_VER
#if _MSC_VER >= 1500
	/* hack for incorrect assumption in msvc header files for code analysis */
	__analysis_assume(tmp);
#endif
	ap2 = ap;
#else
	va_copy(ap2, ap);
#endif

	len = vsnprintf(tmp, 0, fmt, ap2);

	if (len > 0 && (buf = malloc((buflen = (size_t) (len + 1)))) != NULL) {
		len = vsnprintf(buf, buflen, fmt, ap);
		*ret = buf;
	} else {
		*ret = NULL;
		len = -1;
	}

	va_end(ap2);
	return len;
#endif
}




KS_DECLARE(int) ks_snprintf(char *buffer, size_t count, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vsnprintf(buffer, count-1, fmt, ap);
	if (ret < 0)
		buffer[count-1] = '\0';
	va_end(ap);
	return ret;
}

static void null_logger(const char *file, const char *func, int line, int level, const char *fmt, ...)
{
	if (file && func && line && level && fmt) {
		return;
	}
	return;
}


static const char *LEVEL_NAMES[] = {
	"EMERG",
	"ALERT",
	"CRIT",
	"ERROR",
	"WARNING",
	"NOTICE",
	"INFO",
	"DEBUG",
	NULL
};

static int ks_log_level = 7;

static const char *cut_path(const char *in)
{
	const char *p, *ret = in;
	char delims[] = "/\\";
	char *i;

	for (i = delims; *i; i++) {
		p = in;
		while ((p = strchr(p, *i)) != 0) {
			ret = ++p;
		}
	}
	return ret;
}


static void default_logger(const char *file, const char *func, int line, int level, const char *fmt, ...)
{
	const char *fp;
	char *data;
	va_list ap;
	int ret;
	
	if (level < 0 || level > 7) {
		level = 7;
	}
	if (level > ks_log_level) {
		return;
	}
	
	fp = cut_path(file);

	va_start(ap, fmt);

	ret = ks_vasprintf(&data, fmt, ap);

	if (ret != -1) {
		fprintf(stderr, "[%s] %s:%d %s() %s", LEVEL_NAMES[level], fp, line, func, data);
		free(data);
	}

	va_end(ap);

}

ks_logger_t ks_log = null_logger;

KS_DECLARE(void) ks_global_set_logger(ks_logger_t logger)
{
	if (logger) {
		ks_log = logger;
	} else {
		ks_log = null_logger;
	}
}

KS_DECLARE(void) ks_global_set_default_logger(int level)
{
	if (level < 0 || level > 7) {
		level = 7;
	}

	ks_log = default_logger;
	ks_log_level = level;
}

KS_DECLARE(size_t) ks_url_encode(const char *url, char *buf, size_t len)
{
	const char *p;
	size_t x = 0;
	const char urlunsafe[] = "\r\n \"#%&+:;<=>?@[\\]^`{|}";
	const char hex[] = "0123456789ABCDEF";

	if (!buf) {
		return 0;
	}

	if (!url) {
		return 0;
	}

	len--;

	for (p = url; *p; p++) {
		if (x >= len) {
			break;
		}
		if (*p < ' ' || *p > '~' || strchr(urlunsafe, *p)) {
			if ((x + 3) >= len) {
				break;
			}
			buf[x++] = '%';
			buf[x++] = hex[*p >> 4];
			buf[x++] = hex[*p & 0x0f];
		} else {
			buf[x++] = *p;
		}
	}
	buf[x] = '\0';

	return x;
}

KS_DECLARE(char *)ks_url_decode(char *s)
{
	char *o;
	unsigned int tmp;

	for (o = s; *s; s++, o++) {
		if (*s == '%' && strlen(s) > 2 && sscanf(s + 1, "%2x", &tmp) == 1) {
			*o = (char) tmp;
			s += 2;
		} else {
			*o = *s;
		}
	}
	*o = '\0';
	return s;
}


static int ks_socket_reuseaddr(ks_socket_t socket) 
{
#ifdef WIN32
	BOOL reuse_addr = TRUE;
	return setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse_addr, sizeof(reuse_addr));
#else
	int reuse_addr = 1;
	return setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
#endif
}


struct thread_handler {
	ks_listen_callback_t callback;
	ks_socket_t server_sock;
	ks_socket_t client_sock;
	struct sockaddr_in addr;
};

static void *client_thread(ks_thread_t *me, void *obj)
{
	struct thread_handler *handler = (struct thread_handler *) obj;

	handler->callback(handler->server_sock, handler->client_sock, &handler->addr);
	free(handler);

	return NULL;

}

KS_DECLARE(ks_status_t) ks_listen(const char *host, ks_port_t port, ks_listen_callback_t callback)
{
	ks_socket_t server_sock = KS_SOCK_INVALID;
	struct sockaddr_in addr;
	ks_status_t status = KS_SUCCESS;
	
	if ((server_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		return KS_FAIL;
	}

	ks_socket_reuseaddr(server_sock);
		   
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
	
    if (bind(server_sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		status = KS_FAIL;
		goto end;
	}

    if (listen(server_sock, 10000) < 0) {
		status = KS_FAIL;
		goto end;
	}

	for (;;) {
		int client_sock;                    
		struct sockaddr_in echoClntAddr;
#ifdef WIN32
		int clntLen;
#else
		unsigned int clntLen;
#endif

		clntLen = sizeof(echoClntAddr);
    
		if ((client_sock = accept(server_sock, (struct sockaddr *) &echoClntAddr, &clntLen)) == KS_SOCK_INVALID) {
			status = KS_FAIL;
			goto end;
		}
		
		callback(server_sock, client_sock, &echoClntAddr);
	}

 end:

	if (server_sock != KS_SOCK_INVALID) {
		closesocket(server_sock);
		server_sock = KS_SOCK_INVALID;
	}

	return status;

}

KS_DECLARE(ks_status_t) ks_listen_threaded(const char *host, ks_port_t port, ks_listen_callback_t callback, int max)
{
	ks_socket_t server_sock = KS_SOCK_INVALID;
	struct sockaddr_in addr;
	ks_status_t status = KS_SUCCESS;
	struct thread_handler *handler = NULL;

	if ((server_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		return KS_FAIL;
	}

	ks_socket_reuseaddr(server_sock);
		   
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
	
    if (bind(server_sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		status = KS_FAIL;
		goto end;
	}

    if (listen(server_sock, max) < 0) {
		status = KS_FAIL;
		goto end;
	}

	for (;;) {
		int client_sock;                    
		struct sockaddr_in echoClntAddr;
#ifdef WIN32
		int clntLen;
#else
		unsigned int clntLen;
#endif

		clntLen = sizeof(echoClntAddr);
    
		if ((client_sock = accept(server_sock, (struct sockaddr *) &echoClntAddr, &clntLen)) == KS_SOCK_INVALID) {
			status = KS_FAIL;
			goto end;
		}
		
		handler = malloc(sizeof(*handler));
		ks_assert(handler);

		memset(handler, 0, sizeof(*handler));
		handler->callback = callback;
		handler->server_sock = server_sock;
		handler->client_sock = client_sock;
		handler->addr = echoClntAddr;

		ks_thread_create_detached(client_thread, handler);
	}

 end:

	if (server_sock != KS_SOCK_INVALID) {
		closesocket(server_sock);
		server_sock = KS_SOCK_INVALID;
	}

	return status;

}


/* USE WSAPoll on vista or higher */
#ifdef KS_USE_WSAPOLL
KS_DECLARE(int) ks_wait_sock(ks_socket_t sock, uint32_t ms, ks_poll_t flags)
{
}
#endif


#ifdef KS_USE_SELECT
#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 6262 ) /* warning C6262: Function uses '98348' bytes of stack: exceeds /analyze:stacksize'16384'. Consider moving some data to heap */
#endif
KS_DECLARE(int) ks_wait_sock(ks_socket_t sock, uint32_t ms, ks_poll_t flags)
{
	int s = 0, r = 0;
	fd_set rfds;
	fd_set wfds;
	fd_set efds;
	struct timeval tv;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&efds);

#ifndef WIN32
	/* Wouldn't you rather know?? */
	assert(sock <= FD_SETSIZE);
#endif
	
	if ((flags & KS_POLL_READ)) {

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4127 )
	FD_SET(sock, &rfds);
#pragma warning( pop ) 
#else
	FD_SET(sock, &rfds);
#endif
	}

	if ((flags & KS_POLL_WRITE)) {

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4127 )
	FD_SET(sock, &wfds);
#pragma warning( pop ) 
#else
	FD_SET(sock, &wfds);
#endif
	}

	if ((flags & KS_POLL_ERROR)) {

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4127 )
	FD_SET(sock, &efds);
#pragma warning( pop ) 
#else
	FD_SET(sock, &efds);
#endif
	}

	tv.tv_sec = ms / 1000;
	tv.tv_usec = (ms % 1000) * ms;
	
	s = select(sock + 1, (flags & KS_POLL_READ) ? &rfds : NULL, (flags & KS_POLL_WRITE) ? &wfds : NULL, (flags & KS_POLL_ERROR) ? &efds : NULL, &tv);

	if (s < 0) {
		r = s;
	} else if (s > 0) {
		if ((flags & KS_POLL_READ) && FD_ISSET(sock, &rfds)) {
			r |= KS_POLL_READ;
		}

		if ((flags & KS_POLL_WRITE) && FD_ISSET(sock, &wfds)) {
			r |= KS_POLL_WRITE;
		}

		if ((flags & KS_POLL_ERROR) && FD_ISSET(sock, &efds)) {
			r |= KS_POLL_ERROR;
		}
	}

	return r;

}
#ifdef WIN32
#pragma warning( pop ) 
#endif
#endif

#ifdef KS_USE_POLL
KS_DECLARE(int) ks_wait_sock(ks_socket_t sock, uint32_t ms, ks_poll_t flags)
{
	struct pollfd pfds[2] = { { 0 } };
	int s = 0, r = 0;
	
	pfds[0].fd = sock;

	if ((flags & KS_POLL_READ)) {
		pfds[0].events |= POLLIN;
	}

	if ((flags & KS_POLL_WRITE)) {
		pfds[0].events |= POLLOUT;
	}

	if ((flags & KS_POLL_ERROR)) {
		pfds[0].events |= POLLERR;
	}
	
	s = poll(pfds, 1, ms);

	if (s < 0) {
		r = s;
	} else if (s > 0) {
		if ((pfds[0].revents & POLLIN)) {
			r |= KS_POLL_READ;
		}
		if ((pfds[0].revents & POLLOUT)) {
			r |= KS_POLL_WRITE;
		}
		if ((pfds[0].revents & POLLERR)) {
			r |= KS_POLL_ERROR;
		}
	}

	return r;

}
#endif


KS_DECLARE(unsigned int) ks_separate_string_string(char *buf, const char *delim, char **array, unsigned int arraylen)
{
	unsigned int count = 0;
	char *d;
	size_t dlen = strlen(delim);

	array[count++] = buf;

	while (count < arraylen && array[count - 1]) {
		if ((d = strstr(array[count - 1], delim))) {
			*d = '\0';
			d += dlen;
			array[count++] = d;
		} else
			break;
	}

	return count;
}

