/*
 * Copyright (c) 2007-2014, Anthony Minessale II
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

#ifndef _KWS_H
#define _KWS_H

#define WEBSOCKET_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define B64BUFFLEN 1024
#include "ks.h"

KS_BEGIN_EXTERN_C

typedef enum {
	WS_NONE = 0,
	WS_NORMAL = 1000,
	WS_PROTO_ERR = 1002,
	WS_DATA_TOO_BIG = 1009
} kws_cause_t;

typedef enum {
	WSOC_CONTINUATION = 0x0,
	WSOC_TEXT = 0x1,
	WSOC_BINARY = 0x2,
	WSOC_CLOSE = 0x8,
	WSOC_PING = 0x9,
	WSOC_PONG = 0xA
} kws_opcode_t;

typedef enum {
	KWS_CLIENT,
	KWS_SERVER
} kws_type_t;

typedef enum {
	KWS_CLOSE_SOCK,
	KWS_BLOCK,
	KWS_STAY_OPEN
} kws_flag_t;

struct kws_s;
typedef struct kws_s kws_t;


KS_DECLARE(ks_ssize_t) kws_read_frame(kws_t *kws, kws_opcode_t *oc, uint8_t **data); 
KS_DECLARE(ks_ssize_t) kws_write_frame(kws_t *kws, kws_opcode_t oc, void *data, ks_size_t bytes);
KS_DECLARE(ks_ssize_t) kws_raw_read(kws_t *kws, void *data, ks_size_t bytes, int block);
KS_DECLARE(ks_ssize_t) kws_raw_write(kws_t *kws, void *data, ks_size_t bytes);
KS_DECLARE(ks_status_t) kws_init(kws_t **kwsP, ks_socket_t sock, SSL_CTX *ssl_ctx, const char *client_data, kws_flag_t flags, ks_pool_t *pool);
KS_DECLARE(ks_ssize_t) kws_close(kws_t *kws, int16_t reason);
KS_DECLARE(void) kws_destroy(kws_t **kwsP);
KS_DECLARE(ks_status_t) kws_get_buffer(kws_t *kws, char **bufP, ks_size_t *buflen);



#if 0
static inline uint64_t get_unaligned_uint64(const void *p)
{   
    const struct { uint64_t d; } __attribute__((packed)) *pp = p;
    return pp->d;
}
#endif

KS_END_EXTERN_C

#endif /* defined(_KWS_H_) */

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
