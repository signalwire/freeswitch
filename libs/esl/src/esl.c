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

#include <esl.h>

#ifndef HAVE_GETHOSTBYNAME_R
extern int gethostbyname_r (const char *__name,
                            struct hostent *__result_buf,
                            char *__buf, size_t __buflen,
                            struct hostent **__result,
                            int *__h_errnop);
#endif



size_t esl_url_encode(const char *url, char *buf, size_t len)
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

char *esl_url_decode(char *s)
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



esl_status_t esl_connect(esl_handle_t *handle, const char *host, esl_port_t port, const char *password)
{

	struct hostent *result;
	char sendbuf[256];
	char recvbuf[256];
	int rval;
	const char *hval;

	handle->sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	
	if (handle->sock == ESL_SOCK_INVALID) {
		snprintf(handle->err, sizeof(handle->err), "Socket Error");
		return ESL_FAIL;
	}

    memset(&handle->sockaddr, 0, sizeof(handle->sockaddr));
	handle->sockaddr.sin_family = AF_INET;
    handle->sockaddr.sin_port = htons(port);

    memset(&handle->hostent, 0, sizeof(handle->hostent));

#ifdef HAVE_GETHOSTBYNAME_R_FIVE
	rval = gethostbyname_r(host, &handle->hostent, handle->hostbuf, sizeof(handle->hostbuf), &handle->errno);
	result = handle->hostent;
#else
	rval = gethostbyname_r(host, &handle->hostent, handle->hostbuf, sizeof(handle->hostbuf), &result, &handle->errno);
#endif
	
	if (rval) {
		strerror_r(handle->errno, handle->err, sizeof(handle->err));
		goto fail;
	}

	memcpy(&handle->sockaddr.sin_addr, result->h_addr, result->h_length);
	
	rval = connect(handle->sock, (struct sockaddr *) &handle->sockaddr, sizeof(handle->sockaddr));
	
	if (rval) {
		strerror_r(handle->errno, handle->err, sizeof(handle->err));
		goto fail;
	}
	
	if (esl_recv(handle)) {
		snprintf(handle->err, sizeof(handle->err), "Connection Error");
		goto fail;
	}

	hval = esl_event_get_header(handle->last_event, "content-type");

	if (strcasecmp(hval, "auth/request")) {
		snprintf(handle->err, sizeof(handle->err), "Connection Error");
		goto fail;
	}

	snprintf(sendbuf, sizeof(sendbuf), "auth %s\n\n", password);
	esl_send(handle, sendbuf);

	
	if (esl_recv(handle)) {
		snprintf(handle->err, sizeof(handle->err), "Connection Error");
		goto fail;
	}
	

	hval = esl_event_get_header(handle->last_event, "reply-text");

	if (strcasecmp(hval, "+OK accepted")) {
		snprintf(handle->err, sizeof(handle->err), "Connection Error");
		goto fail;
	}
	
	handle->connected = 1;

	return ESL_SUCCESS;

 fail:
	
	esl_disconnect(handle);
	return ESL_FAIL;
}

esl_status_t esl_disconnect(esl_handle_t *handle)
{
	esl_event_safe_destroy(&handle->last_event);
	
	if (handle->sock != ESL_SOCK_INVALID) {
		close(handle->sock);
		handle->sock = ESL_SOCK_INVALID;
		return ESL_SUCCESS;
	}
	
	handle->connected = 0;

	return ESL_FAIL;
}

esl_status_t esl_recv(esl_handle_t *handle)
{
	char *c;
	esl_ssize_t rrval;
	int crc = 0;
	esl_event_t *revent = NULL;
	char *beg;
	char *hname, *hval;
	char *col;
	char *cl;
	ssize_t len;

	esl_event_safe_destroy(&handle->last_event);
	memset(handle->header_buf, 0, sizeof(handle->header_buf));

	c = handle->header_buf;
	beg = c;

	for(;;) {
		rrval = recv(handle->sock, c, 1, 0);

		if (rrval < 0) {
			strerror_r(handle->errno, handle->err, sizeof(handle->err));
			goto fail;
		} else if (rrval > 0) {
			if (*c == '\n') {
				if (++crc == 2) {
					break;
				}
				
				if (!revent) {
					esl_event_create(&revent, ESL_EVENT_COMMAND);
				}
				
				hname = beg;
				hval = col = NULL;
				
				if (hname && (col = strchr(hname, ':'))) {
					hval = col + 1;
					*col = '\0';
					while(*hval == ' ') hval++;
				}
			
				*c = '\0';
				
				if (hname && hval) {
					if (handle->debug > 1) {
						printf("RECV HEADER [%s] = [%s]\n", hname, hval);
					}
					esl_event_add_header_string(revent, ESL_STACK_BOTTOM, hname, hval);
				}

				beg = c+1;
				
				
			} else {
				crc = 0;
			}

			c++;
		}
	}

	if (!revent) {
		goto fail;
	}

	if ((cl = esl_event_get_header(revent, "content-length"))) {
		char *body;
		esl_ssize_t sofar = 0;
		
		len = atol(cl);
		body = malloc(len+1);
		esl_assert(body);
		*(body + len) = '\0';
		
		do {
			esl_ssize_t r;
			if ((r = recv(handle->sock, body + sofar, len - sofar, 0)) < 0) {
				strerror_r(handle->errno, handle->err, sizeof(handle->err));	
				goto fail;
			}
			sofar += r;
		} while (sofar < len);
		
		revent->body = body;
	}



	handle->last_event = revent;
	
	if (handle->last_event) {
		const char *reply = esl_event_get_header(handle->last_event, "reply-text");
		if (!esl_strlen_zero(reply)) {
			strncpy(handle->last_reply, reply, sizeof(handle->last_reply));
		}
	}


	if (handle->debug) {
		char *foo;
		esl_event_serialize(handle->last_event, &foo, ESL_FALSE);
		printf("RECV MESSAGE\n%s\n", foo);
		free(foo);
	}

	return ESL_SUCCESS;

 fail:

	esl_disconnect(handle);
	return ESL_FAIL;

}

esl_status_t esl_send(esl_handle_t *handle, const char *cmd)
{
	if (handle->debug) {
		printf("SEND\n%s\n", cmd);
	}
	
	send(handle->sock, cmd, strlen(cmd), 0);
}


esl_status_t esl_send_recv(esl_handle_t *handle, const char *cmd)
{
	esl_send(handle, cmd);
	esl_recv(handle);
}




