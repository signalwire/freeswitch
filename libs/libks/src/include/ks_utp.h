/*
 * Copyright (c) 2007-2015, Anthony Minessale II
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

#ifndef _KS_UTP_H_
#define _KS_UTP_H_

#include "ks.h"

KS_BEGIN_EXTERN_C

typedef struct utp_socket  utp_socket_t;
typedef struct utp_context utp_context_t;

enum {
	UTP_UDP_DONTFRAG = 2,	// Used to be a #define as UDP_IP_DONTFRAG
};

enum {
	/* socket has reveived syn-ack (notification only for outgoing connection completion) this implies writability */
	UTP_STATE_CONNECT = 1,

	/* socket is able to send more data */
	UTP_STATE_WRITABLE = 2,

	/* connection closed */
	UTP_STATE_EOF = 3,

	/* socket is being destroyed, meaning all data has been sent if possible. it is not valid to refer to the socket after this state change occurs */
	UTP_STATE_DESTROYING = 4,
};

/* Errors codes that can be passed to UTP_ON_ERROR callback */
enum {
	UTP_ECONNREFUSED = 0,
	UTP_ECONNRESET,
	UTP_ETIMEDOUT,
};

enum {
	/* callback names */
	UTP_ON_FIREWALL = 0,
	UTP_ON_ACCEPT,
	UTP_ON_CONNECT,
	UTP_ON_ERROR,
	UTP_ON_READ,
	UTP_ON_OVERHEAD_STATISTICS,
	UTP_ON_STATE_CHANGE,
	UTP_GET_READ_BUFFER_SIZE,
	UTP_ON_DELAY_SAMPLE,
	UTP_GET_UDP_MTU,
	UTP_GET_UDP_OVERHEAD,
	UTP_GET_MILLISECONDS,
	UTP_GET_MICROSECONDS,
	UTP_GET_RANDOM,
	UTP_LOG,
	UTP_SENDTO,

	/* context and socket options that may be set/queried */
    UTP_LOG_NORMAL,
    UTP_LOG_MTU,
    UTP_LOG_DEBUG,
	UTP_SNDBUF,
	UTP_RCVBUF,
	UTP_TARGET_DELAY,

	/* must be last */
	UTP_ARRAY_SIZE,
};

typedef struct {
	utp_context_t *context;
	utp_socket_t *socket;
	size_t len;
	uint32_t flags;
	int callback_type;
	const uint8_t *buf;

	union {
		const struct sockaddr *address;
		int send;
		int sample_ms;
		int error_code;
		int state;
	} d1;

	union {
		socklen_t address_len;
		int type;
	} d2;
} utp_callback_arguments;

typedef uint64_t utp_callback_t (utp_callback_arguments *);

/* Returned by utp_get_context_stats() */
typedef struct {
	uint32_t _nraw_recv[5];	// total packets recieved less than 300/600/1200/MTU bytes fpr all connections (context-wide)
	uint32_t _nraw_send[5];	// total packets sent     less than 300/600/1200/MTU bytes for all connections (context-wide)
} utp_context_stats;

// Returned by utp_get_stats()
typedef struct {
	uint64_t nbytes_recv;	// total bytes received
	uint64_t nbytes_xmit;	// total bytes transmitted
	uint32_t rexmit;		// retransmit counter
	uint32_t fastrexmit;	// fast retransmit counter
	uint32_t nxmit;		// transmit counter
	uint32_t nrecv;		// receive counter (total)
	uint32_t nduprecv;	// duplicate receive counter
	uint32_t mtu_guess;	// Best guess at MTU
} utp_socket_stats;

#define UTP_IOV_MAX 1024

/* For utp_writev, to writes data from multiple buffers */
struct utp_iovec {
	void *iov_base;
	size_t iov_len;
};

// Public Functions
utp_context_t*	utp_init						(int version);
void			utp_destroy						(utp_context_t *ctx);
void			utp_set_callback				(utp_context_t *ctx, int callback_name, utp_callback_t *proc);
void*			utp_context_set_userdata		(utp_context_t *ctx, void *userdata);
void*			utp_context_get_userdata		(utp_context_t *ctx);
int				utp_context_set_option			(utp_context_t *ctx, int opt, int val);
int				utp_context_get_option			(utp_context_t *ctx, int opt);
int				utp_process_udp					(utp_context_t *ctx, const uint8_t *buf, size_t len, const struct sockaddr *to, socklen_t tolen);
int				utp_process_icmp_error			(utp_context_t *ctx, const uint8_t *buffer, size_t len, const struct sockaddr *to, socklen_t tolen);
int				utp_process_icmp_fragmentation	(utp_context_t *ctx, const uint8_t *buffer, size_t len, const struct sockaddr *to, socklen_t tolen, uint16_t next_hop_mtu);
void			utp_check_timeouts				(utp_context_t *ctx);
void			utp_issue_deferred_acks			(utp_context_t *ctx);
utp_context_stats* utp_get_context_stats		(utp_context_t *ctx);
utp_socket_t*	utp_create_socket				(utp_context_t *ctx);
void*			utp_set_userdata				(utp_socket_t *s, void *userdata);
void*			utp_get_userdata				(utp_socket_t *s);
int				utp_setsockopt					(utp_socket_t *s, int opt, int val);
int				utp_getsockopt					(utp_socket_t *s, int opt);
int				utp_connect						(utp_socket_t *s, const struct sockaddr *to, socklen_t tolen);
ssize_t			utp_write						(utp_socket_t *s, void *buf, size_t count);
ssize_t			utp_writev						(utp_socket_t *s, struct utp_iovec *iovec, size_t num_iovecs);
int				utp_getpeername					(utp_socket_t *s, struct sockaddr *addr, socklen_t *addrlen);
void			utp_read_drained				(utp_socket_t *s);
int				utp_get_delays					(utp_socket_t *s, uint32_t *ours, uint32_t *theirs, uint32_t *age);
utp_socket_stats* utp_get_stats					(utp_socket_t *s);
utp_context_t*	utp_get_context					(utp_socket_t *s);
void			utp_close						(utp_socket_t *s);

KS_END_EXTERN_C

#endif							/* defined(_KS_UTP_H_) */

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
