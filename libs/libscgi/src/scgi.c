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

#include <scgi.h>

#ifndef WIN32
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

static scgi_status_t scgi_push_param(scgi_handle_t *handle, const char *name, const char *value);

static int sock_setup(scgi_handle_t *handle)
{

	if (handle->sock == SCGI_SOCK_INVALID) {
        return SCGI_FAIL;
    }

#ifdef WIN32
	{
		BOOL bOptVal = TRUE;
		int bOptLen = sizeof(BOOL);
		setsockopt(handle->sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&bOptVal, bOptLen);
	}
#else
	{
		int x = 1;
		setsockopt(handle->sock, IPPROTO_TCP, TCP_NODELAY, &x, sizeof(x));	
	}
#endif

	return SCGI_SUCCESS;
}


SCGI_DECLARE(size_t) scgi_build_message(scgi_handle_t *handle, char **bufferp)
{
	scgi_param_t *pp;
	size_t len = 0, plen = 0, ctlen = 0;
	char *s, *bp;
	char *buffer = NULL;
	char tmp[128] = "";

	scgi_push_param(handle, "SCGI", "1");

	if (handle->body) {
		ctlen = strlen(handle->body);
	}

	snprintf(tmp, sizeof(tmp), "%d", (int)ctlen);

	scgi_push_param(handle, "CONTENT_LENGTH", tmp);
	

	for(pp = handle->params; pp; pp = pp->next) {
		plen += (strlen(pp->name) + strlen(pp->value) + 2);
	}

	snprintf(tmp, sizeof(tmp), "%d", (int) (plen + ctlen));
	
	len = plen + ctlen + strlen(tmp) + 2;

	buffer = malloc(len);
	memset(buffer, 0, len);

	snprintf(buffer, len, "%d:", (int)plen);
	bp = buffer + strlen(buffer);

	for(pp = handle->params; pp; pp = pp->next) {
		
		for (s = pp->name; s && *s; s++) {
			*bp++ = *s;
		}

		*bp++ = '\0';

		for (s = pp->value; s && *s; s++) {
			*bp++ = *s;
		}

		*bp++ = '\0';
	}

	*bp++ = ',';

	if (handle->body) {
		for (s = handle->body; s && *s; s++) {
			*bp++ = *s;
		}
	}
	
	*bufferp = buffer;

	return len;
}

SCGI_DECLARE(scgi_status_t) scgi_destroy_params(scgi_handle_t *handle)
{
	scgi_param_t *param, *pp;

	pp = handle->params;

	while(pp) {
		param = pp;
		pp = pp->next;

		free(param->name);
		free(param->value);
		free(param);
	}

	handle->params = NULL;
	
	return SCGI_SUCCESS;
}

SCGI_DECLARE(scgi_status_t) scgi_add_body(scgi_handle_t *handle, const char *value)
{
	handle->body = strdup(value);

	return SCGI_SUCCESS;
}

SCGI_DECLARE(const char *) scgi_get_body(scgi_handle_t *handle)
{
	return handle->body;
}

SCGI_DECLARE(scgi_status_t) scgi_add_param(scgi_handle_t *handle, const char *name, const char *value)
{
	scgi_param_t *param, *pp;

	for(pp = handle->params; pp && pp->next; pp = pp->next) {
		if (!strcasecmp(pp->name, name)) {
			return SCGI_FAIL;
		}
	}

	param = malloc(sizeof(*param));
	memset(param, 0, sizeof(*param));
	
	param->name = strdup(name);
	param->value = strdup(value);

	if (!pp) {
		handle->params = param;
	} else {
		pp->next = param;
	}

	return SCGI_SUCCESS;
}

SCGI_DECLARE(const char *) scgi_get_param(scgi_handle_t *handle, const char *name)
{
	scgi_param_t *pp;

	for(pp = handle->params; pp; pp = pp->next) { 
		if (!strcasecmp(pp->name, name)) {
			return pp->value;
		}
	}

	return NULL;
}

static scgi_status_t scgi_push_param(scgi_handle_t *handle, const char *name, const char *value)
{
	scgi_param_t *param;

	param = malloc(sizeof(*param));
	memset(param, 0, sizeof(*param));
	
	param->name = strdup(name);
	param->value = strdup(value);

	param->next = handle->params;
	handle->params = param;

	return SCGI_SUCCESS;
}

SCGI_DECLARE(scgi_status_t) scgi_send_request(scgi_handle_t *handle)
{
	scgi_status_t status = SCGI_SUCCESS;
	char *buffer = NULL;
	size_t bytes = 0;
	ssize_t sent = 0;

	if (handle->connected != 1) {
		return SCGI_FAIL;
	}

	bytes = scgi_build_message(handle, &buffer);
	sent = send(handle->sock, buffer, bytes, 0);
	
	if (sent <= 0) {
		handle->connected = -1;
	}

	scgi_safe_free(buffer);

	return status;
}


SCGI_DECLARE(ssize_t) scgi_recv(scgi_handle_t *handle, unsigned char *buf, size_t buflen)
{
	ssize_t recvd;

	if (handle->connected != 1) {
		return -1;
	}

	recvd = recv(handle->sock, buf, buflen, 0);

	if (recvd == 0) {
		handle->connected = -1;
	}


	return recvd;
}

SCGI_DECLARE(scgi_status_t) scgi_connect(scgi_handle_t *handle, const char *host, scgi_port_t port, uint32_t timeout)
{
	int rval = 0;

	struct addrinfo hints = { 0 }, *result;
#ifndef WIN32
	int fd_flags = 0;
#else
	WORD wVersionRequested = MAKEWORD(2, 0);
	WSADATA wsaData;
	int err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		snprintf(handle->err, sizeof(handle->err), "WSAStartup Error");
		return SCGI_FAIL;
	}

#endif

	handle->sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	
	if (handle->sock == SCGI_SOCK_INVALID) {
		snprintf(handle->err, sizeof(handle->err), "Socket Error");
		return SCGI_FAIL;
	}


	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	
	if (getaddrinfo(host, NULL, &hints, &result)) {
		strncpy(handle->err, "Cannot resolve host", sizeof(handle->err));
		goto fail;
	}

	memcpy(&handle->sockaddr, result->ai_addr, sizeof(handle->sockaddr));	
	handle->sockaddr.sin_family = AF_INET;
	handle->sockaddr.sin_port = htons(port);
	freeaddrinfo(result);

	if (timeout) {
#ifdef WIN32
		u_long arg = 1;
		if (ioctlsocket(handle->sock, FIONBIO, &arg) == SOCKET_ERROR) {
			snprintf(handle->err, sizeof(handle->err), "Socket Connection Error");
			goto fail;
		}
#else
		fd_flags = fcntl(handle->sock, F_GETFL, 0);
		if (fcntl(handle->sock, F_SETFL, fd_flags | O_NONBLOCK)) {
			snprintf(handle->err, sizeof(handle->err), "Socket Connection Error");
			goto fail;
		}
#endif
	}

	rval = connect(handle->sock, (struct sockaddr*)&handle->sockaddr, sizeof(handle->sockaddr));
	
	if (timeout) {
		int r;


		r = scgi_wait_sock(handle->sock, timeout, SCGI_POLL_WRITE);
		
		if (r <= 0) {
			snprintf(handle->err, sizeof(handle->err), "Connection timed out");
			goto fail;
		}

		if (!(r & SCGI_POLL_WRITE)) {
			snprintf(handle->err, sizeof(handle->err), "Connection timed out");
			goto fail;
		}

#ifdef WIN32
		{
			u_long arg = 0;
			if (ioctlsocket(handle->sock, FIONBIO, &arg) == SOCKET_ERROR) {
				snprintf(handle->err, sizeof(handle->err), "Socket Connection Error");
				goto fail;
			}
		}
#else
		fcntl(handle->sock, F_SETFL, fd_flags);
#endif	
		rval = 0;
	}
	
	result = NULL;
	
	if (rval) {
		snprintf(handle->err, sizeof(handle->err), "Socket Connection Error");
		goto fail;
	}

	sock_setup(handle);

	handle->connected = 1;


	return SCGI_SUCCESS;

 fail:
	
	handle->connected = 0;
	scgi_disconnect(handle);

	return SCGI_FAIL;
}



SCGI_DECLARE(scgi_status_t) scgi_disconnect(scgi_handle_t *handle)
{
	scgi_status_t status = SCGI_FAIL;
	
	if (handle->destroyed) {
		return SCGI_FAIL;
	}

	if (handle->sock != SCGI_SOCK_INVALID) {
		closesocket(handle->sock);
		handle->sock = SCGI_SOCK_INVALID;
		status = SCGI_SUCCESS;
	}
	
	handle->destroyed = 1;
	handle->connected = 0;

	scgi_destroy_params(handle);
	scgi_safe_free(handle->body);

	return status;
}


/* USE WSAPoll on vista or higher */
#ifdef SCGI_USE_WSAPOLL
SCGI_DECLARE(int) scgi_wait_sock(scgi_socket_t sock, uint32_t ms, scgi_poll_t flags)
{
}
#endif


#ifdef SCGI_USE_SELECT
#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 6262 ) /* warning C6262: Function uses '98348' bytes of stack: exceeds /analyze:stacksize'16384'. Consider moving some data to heap */
#endif
SCGI_DECLARE(int) scgi_wait_sock(scgi_socket_t sock, uint32_t ms, scgi_poll_t flags)
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
	
	if ((flags & SCGI_POLL_READ)) {

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4127 )
	FD_SET(sock, &rfds);
#pragma warning( pop ) 
#else
	FD_SET(sock, &rfds);
#endif
	}

	if ((flags & SCGI_POLL_WRITE)) {

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4127 )
	FD_SET(sock, &wfds);
#pragma warning( pop ) 
#else
	FD_SET(sock, &wfds);
#endif
	}

	if ((flags & SCGI_POLL_ERROR)) {

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
	
	s = select(sock + 1, (flags & SCGI_POLL_READ) ? &rfds : NULL, (flags & SCGI_POLL_WRITE) ? &wfds : NULL, (flags & SCGI_POLL_ERROR) ? &efds : NULL, &tv);

	if (s < 0) {
		r = s;
	} else if (s > 0) {
		if ((flags & SCGI_POLL_READ) && FD_ISSET(sock, &rfds)) {
			r |= SCGI_POLL_READ;
		}

		if ((flags & SCGI_POLL_WRITE) && FD_ISSET(sock, &wfds)) {
			r |= SCGI_POLL_WRITE;
		}

		if ((flags & SCGI_POLL_ERROR) && FD_ISSET(sock, &efds)) {
			r |= SCGI_POLL_ERROR;
		}
	}

	return r;

}
#ifdef WIN32
#pragma warning( pop ) 
#endif
#endif

#ifdef SCGI_USE_POLL
SCGI_DECLARE(int) scgi_wait_sock(scgi_socket_t sock, uint32_t ms, scgi_poll_t flags)
{
	struct pollfd pfds[2] = { { 0 } };
	int s = 0, r = 0;
	
	pfds[0].fd = sock;

	if ((flags & SCGI_POLL_READ)) {
		pfds[0].events |= POLLIN;
	}

	if ((flags & SCGI_POLL_WRITE)) {
		pfds[0].events |= POLLOUT;
	}

	if ((flags & SCGI_POLL_ERROR)) {
		pfds[0].events |= POLLERR;
	}
	
	s = poll(pfds, 1, ms);

	if (s < 0) {
		r = s;
	} else if (s > 0) {
		if ((pfds[0].revents & POLLIN)) {
			r |= SCGI_POLL_READ;
		}
		if ((pfds[0].revents & POLLOUT)) {
			r |= SCGI_POLL_WRITE;
		}
		if ((pfds[0].revents & POLLERR)) {
			r |= SCGI_POLL_ERROR;
		}
	}

	return r;

}
#endif

static int scgi_socket_reuseaddr(scgi_socket_t socket) 
{
#ifdef WIN32
	BOOL reuse_addr = TRUE;
	return setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse_addr, sizeof(reuse_addr));
#else
	int reuse_addr = 1;
	return setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
#endif
}

SCGI_DECLARE(scgi_status_t) scgi_bind(const char *host, scgi_port_t port, scgi_socket_t *socketp)
{
	scgi_socket_t server_sock = SCGI_SOCK_INVALID;
	struct sockaddr_in addr;
	scgi_status_t status = SCGI_SUCCESS;
	
	if ((server_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		return SCGI_FAIL;
	}

	scgi_socket_reuseaddr(server_sock);
		   
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
	
    if (bind(server_sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		status = SCGI_FAIL;
		goto end;
	}

    if (listen(server_sock, 10000) < 0) {
		status = SCGI_FAIL;
		goto end;
	}	

 end:

	if (status == SCGI_FAIL) {
		if (server_sock != SCGI_SOCK_INVALID) {
			closesocket(server_sock);
			server_sock = SCGI_SOCK_INVALID;
		} 
	} else {
		*socketp = server_sock;
	}

	return status;
}

SCGI_DECLARE(scgi_status_t) scgi_accept(scgi_socket_t server_sock, scgi_socket_t *client_sock_p, struct sockaddr_in *echoClntAddr)
{
	scgi_status_t status = SCGI_SUCCESS;
	int client_sock;                    
	struct sockaddr_in local_echoClntAddr;
#ifdef WIN32
	int clntLen;
#else
	unsigned int clntLen;
#endif
	
	if (!echoClntAddr) {
		echoClntAddr = &local_echoClntAddr;
	}
	
	clntLen = sizeof(*echoClntAddr);
    
	if ((client_sock = accept(server_sock, (struct sockaddr *) echoClntAddr, &clntLen)) == SCGI_SOCK_INVALID) {
		//printf("FRICK %s\n", strerror(errno));
		status = SCGI_FAIL;
	} else {
		*client_sock_p = client_sock;
	}
	
	return status;
}

SCGI_DECLARE(scgi_status_t) scgi_listen(const char *host, scgi_port_t port, scgi_listen_callback_t callback)
{

	scgi_socket_t server_sock = SCGI_SOCK_INVALID, client_sock = SCGI_SOCK_INVALID;
	scgi_status_t status = SCGI_FAIL;
	struct sockaddr_in echoClntAddr;

	if ((status = scgi_bind(host, port, &server_sock)) == SCGI_SUCCESS) {

		while(scgi_accept(server_sock, &client_sock, &echoClntAddr) == SCGI_SUCCESS) {
			callback(server_sock, &client_sock, &echoClntAddr);
			
			if (client_sock != SCGI_SOCK_INVALID) {
				closesocket(client_sock);
				client_sock = SCGI_SOCK_INVALID;
			}
		}
	}

	return status;

}

#define next_str(_e, _s) _s = _e; while(*_s) _s++; _s++

SCGI_DECLARE(scgi_status_t) scgi_parse(scgi_socket_t sock, scgi_handle_t *handle)
{
	char sbuf[128] = "";
	char *p = sbuf, *e, *end;
	ssize_t r = 0;
	scgi_status_t status = SCGI_FAIL;
	ssize_t bytes = 0;
	char *headers = NULL;
	int loops = 0;
	ssize_t clen = 0;
	char *body = NULL;
	char comma = 0;

	memset(handle, 0, sizeof(*handle));

	handle->sock = sock;
	
	handle->connected = 1;
	sock_setup(handle);

	for(;;) {

		if ((r = recv(sock, p, 1, 0)) < 1)  {
			break;
		}

		if (*p == ':') {
			*p = '\0';
			break;
		}
		p++;
	}

	if (r <= 0) goto end;

	bytes = atoi(sbuf);

	if (bytes <= 0) goto end;

	headers = malloc(bytes);
	r = recv(sock, headers, bytes, 0);
	if (r <= 0) goto end;

	r = recv(sock, &comma, 1, 0);
	if (r <= 0 || comma != ',') goto end;
	

	p = headers;
	end = p + bytes;
	e = NULL;

	while(p < end) {
		next_str(p, e);

		if (!e) break;

		if (!loops++) {
			if (!strcasecmp(p, "CONTENT_LENGTH") && e) {
				clen = atoi(e);
				if (clen) {
					body = malloc(clen+1);
					r = recv(sock, body, clen, 0);
					*(body + clen) = '\0';
					if (r <= 0) goto end;
					scgi_add_body(handle, body);
					scgi_safe_free(body);
				}

				status = SCGI_SUCCESS;
				
			} else {
				goto end;
			}
		}

		scgi_add_param(handle, p, e);
		next_str(e, p);
	}

 end:

	scgi_safe_free(headers);

	return status;
}


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
