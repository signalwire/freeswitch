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

#include "ks.h"


#ifdef _MSC_VER
/* warning C4706: assignment within conditional expression*/
#pragma warning(disable: 4706)
#endif

#define WS_BLOCK 1
#define WS_NOBLOCK 0

#define SHA1_HASH_SIZE 20

static const char c64[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

//static ks_ssize_t ws_send_buf(kws_t *kws, kws_opcode_t oc);
//static ks_ssize_t ws_feed_buf(kws_t *kws, void *data, ks_size_t bytes);


struct kws_s {
	ks_pool_t *pool;
	ks_socket_t sock;
	kws_type_t type;
	char *buffer;
	char *bbuffer;
	char *body;
	char *uri;
	ks_size_t buflen;
	ks_size_t bbuflen;
	ks_ssize_t datalen;
	ks_ssize_t wdatalen;
	char *payload;
	ks_ssize_t plen;
	ks_ssize_t rplen;
	ks_ssize_t packetlen;
	SSL *ssl;
	int handshake;
	uint8_t down;
	int secure;
	uint8_t close_sock;
	SSL_CTX *ssl_ctx;
	int block;
	int sanity;
	int secure_established;
	int logical_established;
	int stay_open;
	int x;
	void *write_buffer;
	ks_size_t write_buffer_len;
	char *req_uri;
	char *req_host;
	char *req_proto;
};



static int cheezy_get_var(char *data, char *name, char *buf, ks_size_t buflen)
{
  char *p=data;

  /* the old way didnt make sure that variable values were used for the name hunt
   * and didnt ensure that only a full match of the variable name was used
   */

  do {
    if(!strncmp(p,name,strlen(name)) && *(p+strlen(name))==':') break;
  } while((p = (strstr(p,"\n")+1))!=(char *)1);


  if (p && p != (char *)1 && *p!='\0') {
    char *v, *e = 0;

    v = strchr(p, ':');
    if (v) {
      v++;
      while(v && *v == ' ') {
	v++;
      }
      if (v)  {
	e = strchr(v, '\r');
	if (!e) {
	  e = strchr(v, '\n');
	}
      }
			
      if (v && e) {
	int cplen;
	ks_size_t len = e - v;
	
	if (len > buflen - 1) {
	  cplen = buflen -1;
	} else {
	  cplen = len;
	}
	
	strncpy(buf, v, cplen);
	*(buf+cplen) = '\0';
	return 1;
      }
      
    }
  }
  return 0;
}

static int b64encode(unsigned char *in, ks_size_t ilen, unsigned char *out, ks_size_t olen) 
{
	int y=0,bytes=0;
	ks_size_t x=0;
	unsigned int b=0,l=0;

	if(olen) {
	}

	for(x=0;x<ilen;x++) {
		b = (b<<8) + in[x];
		l += 8;
		while (l >= 6) {
			out[bytes++] = c64[(b>>(l-=6))%64];
			if(++y!=72) {
				continue;
			}
			//out[bytes++] = '\n';
			y=0;
		}
	}

	if (l > 0) {
		out[bytes++] = c64[((b%16)<<(6-l))%64];
	}
	if (l != 0) while (l < 6) {
		out[bytes++] = '=', l += 2;
	}

	return 0;
}

static void sha1_digest(unsigned char *digest, char *in)
{
	SHA_CTX sha;

	SHA1_Init(&sha);
	SHA1_Update(&sha, in, strlen(in));
	SHA1_Final(digest, &sha);

}

/* fix me when we get real rand funcs in ks */
static void gen_nonce(unsigned char *buf, uint16_t len)
{
	int max = 255;
	uint16_t x;
	ks_time_t time_now = ks_time_now();
	srand((unsigned int)(((time_now >> 32) ^ time_now) & 0xffffffff));

	for (x = 0; x < len; x++) {
		int j = (int) (max * 1.0 * rand() / (RAND_MAX + 1.0));
		buf[x] = (char) j;
	}
}

static int verify_accept(kws_t *kws, const unsigned char *enonce, const char *accept)
{
	char input[256] = "";
	unsigned char output[SHA1_HASH_SIZE] = "";
	char b64[256] = "";

	snprintf(input, sizeof(input), "%s%s", enonce, WEBSOCKET_GUID);
	sha1_digest(output, input);
	b64encode((unsigned char *)output, SHA1_HASH_SIZE, (unsigned char *)b64, sizeof(b64));

	return !strcmp(b64, accept);
}

static int ws_client_handshake(kws_t *kws)
{
	unsigned char nonce[16];
	unsigned char enonce[128] = "";
	char req[256] = "";

	gen_nonce(nonce, sizeof(nonce));
	b64encode(nonce, sizeof(nonce), enonce, sizeof(enonce));
	
	ks_snprintf(req, sizeof(req), 
				"GET %s HTTP/1.1\r\n"
				"Host: %s\n"
				"Upgrade: websocket\r\n"
				"Connection: Upgrade\r\n"
				"Sec-WebSocket-Key: %s\r\n"
				"Sec-WebSocket-Protocol: %s\r\n"
				"Sec-WebSocket-Version: 13\r\n"
				"\r\n",
				kws->req_uri, kws->req_host, enonce, kws->req_proto);

	kws_raw_write(kws, req, strlen(req));

	ks_ssize_t bytes;

	do {
		bytes = kws_raw_read(kws, kws->buffer + kws->datalen, kws->buflen - kws->datalen, WS_BLOCK);
	} while (bytes > 0 && !strstr((char *)kws->buffer, "\r\n\r\n"));
	
	char accept[128] = "";

	cheezy_get_var(kws->buffer, "Sec-WebSocket-Accept", accept, sizeof(accept));

	if (zstr_buf(accept) || !verify_accept(kws, enonce, (char *)accept)) {
		return -1;
	}

	kws->handshake = 1;

	return 0;
}

static int ws_server_handshake(kws_t *kws)
{
	char key[256] = "";
	char version[5] = "";
	char proto[256] = "";
	char proto_buf[384] = "";
	char input[256] = "";
	unsigned char output[SHA1_HASH_SIZE] = "";
	char b64[256] = "";
	char respond[512] = "";
	ks_ssize_t bytes;
	char *p, *e = 0;

	if (kws->sock == KS_SOCK_INVALID) {
		return -3;
	}

	while((bytes = kws_raw_read(kws, kws->buffer + kws->datalen, kws->buflen - kws->datalen, WS_BLOCK)) > 0) {
		kws->datalen += bytes;
		if (strstr(kws->buffer, "\r\n\r\n") || strstr(kws->buffer, "\n\n")) {
			break;
		}
	}

	if (bytes > kws->buflen -1) {
		goto err;
	}

	*(kws->buffer + kws->datalen) = '\0';
	
	if (strncasecmp(kws->buffer, "GET ", 4)) {
		goto err;
	}
	
	p = kws->buffer + 4;
	
	e = strchr(p, ' ');
	if (!e) {
		goto err;
	}

	kws->uri = ks_pool_alloc(kws->pool, (e-p) + 1);
	strncpy(kws->uri, p, e-p);
	*(kws->uri + (e-p)) = '\0';

	cheezy_get_var(kws->buffer, "Sec-WebSocket-Key", key, sizeof(key));
	cheezy_get_var(kws->buffer, "Sec-WebSocket-Version", version, sizeof(version));
	cheezy_get_var(kws->buffer, "Sec-WebSocket-Protocol", proto, sizeof(proto));
	
	if (!*key) {
		goto err;
	}
		
	snprintf(input, sizeof(input), "%s%s", key, WEBSOCKET_GUID);
	sha1_digest(output, input);
	b64encode((unsigned char *)output, SHA1_HASH_SIZE, (unsigned char *)b64, sizeof(b64));

	if (*proto) {
		snprintf(proto_buf, sizeof(proto_buf), "Sec-WebSocket-Protocol: %s\r\n", proto);
	}

	snprintf(respond, sizeof(respond), 
			 "HTTP/1.1 101 Switching Protocols\r\n"
			 "Upgrade: websocket\r\n"
			 "Connection: Upgrade\r\n"
			 "Sec-WebSocket-Accept: %s\r\n"
			 "%s\r\n",
			 b64,
			 proto_buf);
	respond[511] = 0;

	if (kws_raw_write(kws, respond, strlen(respond)) != (ks_ssize_t)strlen(respond)) {
		goto err;
	}

	kws->handshake = 1;

	return 0;

 err:

	if (!kws->stay_open) {

		snprintf(respond, sizeof(respond), "HTTP/1.1 400 Bad Request\r\n"
				 "Sec-WebSocket-Version: 13\r\n\r\n");
		respond[511] = 0;

		kws_raw_write(kws, respond, strlen(respond));

		kws_close(kws, WS_NONE);
	}

	return -1;

}

KS_DECLARE(ks_ssize_t) kws_raw_read(kws_t *kws, void *data, ks_size_t bytes, int block)
{
	ks_ssize_t r;
	int err = 0;

	kws->x++;
	if (kws->x > 250) ks_sleep_ms(1);

	if (kws->ssl) {
		do {
			r = SSL_read(kws->ssl, data, bytes);

			if (r == -1) {
				err = SSL_get_error(kws->ssl, r);
				
				if (err == SSL_ERROR_WANT_READ) {
					if (!block) {
						r = -2;
						goto end;
					}
					kws->x++;
					ks_sleep_ms(10);
				} else {
					r = -1;
					goto end;
				}
			}

		} while (r == -1 && err == SSL_ERROR_WANT_READ && kws->x < 1000);

		goto end;
	}

	do {

		r = recv(kws->sock, data, bytes, 0);

		if (r == -1) {
			if (!block && ks_errno_is_blocking(ks_errno())) {
				r = -2;
				goto end;
			}

			if (block) {
				kws->x++;
				ks_sleep_ms(10);
			}
		}
	} while (r == -1 && ks_errno_is_blocking(ks_errno()) && kws->x < 1000);

 end:
	
	if (kws->x >= 10000 || (block && kws->x >= 1000)) {
		r = -1;
	}

	if (r > 0) {
		*((char *)data + r) = '\0';
	}

	if (r >= 0) {
		kws->x = 0;
	}
	
	return r;
}

KS_DECLARE(ks_ssize_t) kws_raw_write(kws_t *kws, void *data, ks_size_t bytes)
{
	ks_ssize_t r;
	int sanity = 2000;
	int ssl_err = 0;
	ks_size_t wrote = 0;

	if (kws->ssl) {
		do {
			r = SSL_write(kws->ssl, (void *)((unsigned char *)data + wrote), bytes - wrote);

			if (r > 0) {
				wrote += r;
			}

			if (sanity < 2000) {
				ks_sleep_ms(1);
			}

			if (r == -1) {
				ssl_err = SSL_get_error(kws->ssl, r);
			}

		} while (--sanity > 0 && ((r == -1 && ssl_err == SSL_ERROR_WANT_WRITE) || (kws->block && wrote < bytes)));

		if (ssl_err) {
			r = ssl_err * -1;
		}
		
		return r;
	}

	do {
		r = send(kws->sock, (void *)((unsigned char *)data + wrote), bytes - wrote, 0);
		
		if (r > 0) {
			wrote += r;
		}

		if (sanity < 2000) {
			ks_sleep_ms(1);
		}
		
	} while (--sanity > 0 && ((r == -1 && ks_errno_is_blocking(ks_errno())) || (kws->block && wrote < bytes)));
	
	//if (r<0) {
		//printf("wRITE FAIL: %s\n", strerror(errno));
	//}

	return r;
}

static void setup_socket(ks_socket_t sock)
{
	ks_socket_option(sock, KS_SO_NONBLOCK, KS_TRUE);
}

static void restore_socket(ks_socket_t sock)
{
	ks_socket_option(sock, KS_SO_NONBLOCK, KS_FALSE);
}

static int establish_client_logical_layer(kws_t *kws)
{

	if (!kws->sanity) {
		return -1;
	}

	if (kws->logical_established) {
		return 0;
	}

	if (kws->secure && !kws->secure_established) {
		int code;

		if (!kws->ssl) {
			kws->ssl = SSL_new(kws->ssl_ctx);
			assert(kws->ssl);

			SSL_set_fd(kws->ssl, kws->sock);
		}

		do {
			code = SSL_connect(kws->ssl);

			if (code == 1) {
				kws->secure_established = 1;
				break;
			}

			if (code == 0) {
				return -1;
			}
			
			if (code < 0) {
				if (code == -1 && SSL_get_error(kws->ssl, code) != SSL_ERROR_WANT_READ) {
					return -1;
				}
			}

			if (kws->block) {
				ks_sleep_ms(10);
			} else {
				ks_sleep_ms(1);
			}

			kws->sanity--;

			if (!kws->block) {
				return -2;
			}

		} while (kws->sanity > 0);
		
		if (!kws->sanity) {
			return -1;
		}		
	}

	while (!kws->down && !kws->handshake) {
		int r = ws_client_handshake(kws);

		if (r < 0) {
			kws->down = 1;
			return -1;
		}

		if (!kws->handshake && !kws->block) {
			return -2;
		}

	}

	kws->logical_established = 1;

	return 0;
}

static int establish_server_logical_layer(kws_t *kws)
{

	if (!kws->sanity) {
		return -1;
	}

	if (kws->logical_established) {
		return 0;
	}

	if (kws->secure && !kws->secure_established) {
		int code;

		if (!kws->ssl) {
			kws->ssl = SSL_new(kws->ssl_ctx);
			assert(kws->ssl);

			SSL_set_fd(kws->ssl, kws->sock);
		}

		do {
			code = SSL_accept(kws->ssl);

			if (code == 1) {
				kws->secure_established = 1;
				break;
			}

			if (code == 0) {
				return -1;
			}
			
			if (code < 0) {
				if (code == -1 && SSL_get_error(kws->ssl, code) != SSL_ERROR_WANT_READ) {
					return -1;
				}
			}

			if (kws->block) {
				ks_sleep_ms(10);
			} else {
				ks_sleep_ms(1);
			}

			kws->sanity--;

			if (!kws->block) {
				return -2;
			}

		} while (kws->sanity > 0);
		
		if (!kws->sanity) {
			return -1;
		}
		
	}

	while (!kws->down && !kws->handshake) {
		int r = ws_server_handshake(kws);

		if (r < 0) {
			kws->down = 1;
			return -1;
		}

		if (!kws->handshake && !kws->block) {
			return -2;
		}

	}

	kws->logical_established = 1;
	
	return 0;
}

static int establish_logical_layer(kws_t *kws)
{
	if (kws->type == KWS_CLIENT) {
		return establish_client_logical_layer(kws);
	} else {
		return establish_server_logical_layer(kws);
	}
}


KS_DECLARE(ks_status_t) kws_init(kws_t **kwsP, ks_socket_t sock, SSL_CTX *ssl_ctx, const char *client_data, kws_flag_t flags, ks_pool_t *pool)
{
	kws_t *kws;

	kws = ks_pool_alloc(pool, sizeof(*kws));
	kws->pool = pool;

	if ((flags & KWS_CLOSE_SOCK)) {
		kws->close_sock = 1;
	}

	if ((flags & KWS_STAY_OPEN)) {
		kws->stay_open = 1;
	}

	if ((flags & KWS_BLOCK)) {
		kws->block = 1;
	}

	if (client_data) {
		char *p = NULL;
		kws->req_uri = ks_pstrdup(kws->pool, client_data);

		if ((p = strchr(kws->req_uri, ':'))) {
			*p++ = '\0';
			kws->req_host = p;
			if ((p = strchr(kws->req_host, ':'))) {
				*p++ = '\0';
				kws->req_proto = p;
			}
		}

		kws->type = KWS_CLIENT;
	} else {
		kws->type = KWS_SERVER;
	}

	kws->sock = sock;
	kws->sanity = 5000;
	kws->ssl_ctx = ssl_ctx;

	kws->buflen = 1024 * 64;
	kws->bbuflen = kws->buflen;

	kws->buffer = ks_pool_alloc(kws->pool, kws->buflen);
	kws->bbuffer = ks_pool_alloc(kws->pool, kws->bbuflen);
	//printf("init %p %ld\n", (void *) kws->bbuffer, kws->bbuflen);
	//memset(kws->buffer, 0, kws->buflen);
	//memset(kws->bbuffer, 0, kws->bbuflen);

	kws->secure = ssl_ctx ? 1 : 0;

	setup_socket(sock);

	if (establish_logical_layer(kws) == -1) {
		goto err;
	}

	if (kws->down) {
		goto err;
	}

	*kwsP = kws;

	return KS_STATUS_SUCCESS;

 err:
	
	kws_destroy(&kws);
	
	return KS_STATUS_FAIL;
}

KS_DECLARE(void) kws_destroy(kws_t **kwsP)
{
	kws_t *kws;
	ks_assert(kwsP);

	if (!(kws = *kwsP)) {
		return;
	}

	*kwsP = NULL;
	
	if (!kws->down) {
		kws_close(kws, WS_NONE);
	}

	if (kws->down > 1) {
		return;
	}
	
	kws->down = 2;

	if (kws->write_buffer) {
		ks_pool_free(kws->pool, kws->write_buffer);
		kws->write_buffer = NULL;
		kws->write_buffer_len = 0;
	}

	if (kws->ssl) {
		int code;
		do {
			code = SSL_shutdown(kws->ssl);
		} while (code == -1 && SSL_get_error(kws->ssl, code) == SSL_ERROR_WANT_READ);

		SSL_free(kws->ssl);
		kws->ssl = NULL;
	}

	if (kws->buffer) ks_pool_free(kws->pool, kws->buffer);
	if (kws->bbuffer) ks_pool_free(kws->pool, kws->bbuffer);

	kws->buffer = kws->bbuffer = NULL;

	ks_pool_free(kws->pool, kws);
	kws = NULL;
}

KS_DECLARE(ks_ssize_t) kws_close(kws_t *kws, int16_t reason) 
{
	
	if (kws->down) {
		return -1;
	}

	kws->down = 1;
	
	if (kws->uri) {
		ks_pool_free(kws->pool, kws->uri);
		kws->uri = NULL;
	}

	if (reason && kws->sock != KS_SOCK_INVALID) {
		uint16_t *u16;
		uint8_t fr[4] = {WSOC_CLOSE | 0x80, 2, 0};

		u16 = (uint16_t *) &fr[2];
		*u16 = htons((int16_t)reason);
		kws_raw_write(kws, fr, 4);
	}

	restore_socket(kws->sock);

	if (kws->close_sock && kws->sock != KS_SOCK_INVALID) {
#ifndef WIN32
		close(kws->sock);
#else
		closesocket(kws->sock);
#endif
	}

	kws->sock = KS_SOCK_INVALID;

	return reason * -1;
	
}

#ifndef WIN32
#if defined(HAVE_BYTESWAP_H)
#include <byteswap.h>
#elif defined(HAVE_SYS_ENDIAN_H)
#include <sys/endian.h>
#elif defined (__APPLE__)
#include <libkern/OSByteOrder.h>
#define bswap_16 OSSwapInt16
#define bswap_32 OSSwapInt32
#define bswap_64 OSSwapInt64
#elif defined (__UCLIBC__)
#else
#define bswap_16(value) ((((value) & 0xff) << 8) | ((value) >> 8))
#define bswap_32(value) (((uint32_t)bswap_16((uint16_t)((value) & 0xffff)) << 16) | (uint32_t)bswap_16((uint16_t)((value) >> 16)))
#define bswap_64(value) (((uint64_t)bswap_32((uint32_t)((value) & 0xffffffff)) << 32) | (uint64_t)bswap_32((uint32_t)((value) >> 32)))
#endif
#endif

uint64_t hton64(uint64_t val)
{
#if __BYTE_ORDER == __BIG_ENDIAN
	return (val);
#else
	return bswap_64(val);
#endif
}

uint64_t ntoh64(uint64_t val)
{
#if __BYTE_ORDER == __BIG_ENDIAN
	return (val);
#else
	return bswap_64(val);
#endif
}


KS_DECLARE(ks_ssize_t) kws_read_frame(kws_t *kws, kws_opcode_t *oc, uint8_t **data)
{
	
	ks_ssize_t need = 2;
	char *maskp;
	int ll = 0;
	int frag = 0;
	int blen;

	kws->body = kws->bbuffer;
	kws->packetlen = 0;

 again:
	need = 2;
	maskp = NULL;
	*data = NULL;

	ll = establish_logical_layer(kws);

	if (ll < 0) {
		return ll;
	}

	if (kws->down) {
		return -1;
	}

	if (!kws->handshake) {
		return kws_close(kws, WS_PROTO_ERR);
	}

	if ((kws->datalen = kws_raw_read(kws, kws->buffer, 9, kws->block)) < 0) {
		if (kws->datalen == -2) {
			return -2;
		}
		return kws_close(kws, WS_PROTO_ERR);
	}
	
	if (kws->datalen < need) {
		if ((kws->datalen += kws_raw_read(kws, kws->buffer + kws->datalen, 9 - kws->datalen, WS_BLOCK)) < need) {
			/* too small - protocol err */
			return kws_close(kws, WS_PROTO_ERR);
		}
	}

	*oc = *kws->buffer & 0xf;

	switch(*oc) {
	case WSOC_CLOSE:
		{
			kws->plen = kws->buffer[1] & 0x7f;
			*data = (uint8_t *) &kws->buffer[2];
			return kws_close(kws, 1000);
		}
		break;
	case WSOC_CONTINUATION:
	case WSOC_TEXT:
	case WSOC_BINARY:
	case WSOC_PING:
	case WSOC_PONG:
		{
			int fin = (kws->buffer[0] >> 7) & 1;
			int mask = (kws->buffer[1] >> 7) & 1;
			

			if (!fin && *oc != WSOC_CONTINUATION) {
				frag = 1;
			} else if (fin && *oc == WSOC_CONTINUATION) {
				frag = 0;
			}

			if (mask) {
				need += 4;
				
				if (need > kws->datalen) {
					/* too small - protocol err */
					*oc = WSOC_CLOSE;
					return kws_close(kws, WS_PROTO_ERR);
				}
			}

			kws->plen = kws->buffer[1] & 0x7f;
			kws->payload = &kws->buffer[2];
			
			if (kws->plen == 127) {
				uint64_t *u64;
				int more = 0;

				need += 8;

				if (need > kws->datalen) {
					/* too small - protocol err */
					//*oc = WSOC_CLOSE;
					//return kws_close(kws, WS_PROTO_ERR);

					more = kws_raw_read(kws, kws->buffer + kws->datalen, need - kws->datalen, WS_BLOCK);

					if (more < need - kws->datalen) {
						*oc = WSOC_CLOSE;
						return kws_close(kws, WS_PROTO_ERR);
					} else {
						kws->datalen += more;
					}


				}
				
				u64 = (uint64_t *) kws->payload;
				kws->payload += 8;
				kws->plen = ntoh64(*u64);
			} else if (kws->plen == 126) {
				uint16_t *u16;

				need += 2;

				if (need > kws->datalen) {
					/* too small - protocol err */
					*oc = WSOC_CLOSE;
					return kws_close(kws, WS_PROTO_ERR);
				}

				u16 = (uint16_t *) kws->payload;
				kws->payload += 2;
				kws->plen = ntohs(*u16);
			}

			if (mask) {
				maskp = (char *)kws->payload;
				kws->payload += 4;
			}

			need = (kws->plen - (kws->datalen - need));

			if (need < 0) {
				/* invalid read - protocol err .. */
				*oc = WSOC_CLOSE;
				return kws_close(kws, WS_PROTO_ERR);
			}

			blen = kws->body - kws->bbuffer;

			if (need + blen > (ks_ssize_t)kws->bbuflen) {
				void *tmp;
				
				kws->bbuflen = need + blen + kws->rplen;

				if ((tmp = ks_pool_resize(kws->pool, kws->bbuffer, kws->bbuflen))) {
					kws->bbuffer = tmp;
				} else {
					abort();
				}

				kws->body = kws->bbuffer + blen;
			}

			kws->rplen = kws->plen - need;
			
			if (kws->rplen) {
				memcpy(kws->body, kws->payload, kws->rplen);
			}
			
			while(need) {
				ks_ssize_t r = kws_raw_read(kws, kws->body + kws->rplen, need, WS_BLOCK);

				if (r < 1) {
					/* invalid read - protocol err .. */
					*oc = WSOC_CLOSE;
					return kws_close(kws, WS_PROTO_ERR);
				}

				kws->datalen += r;
				kws->rplen += r;
				need -= r;
			}
			
			if (mask && maskp) {
				ks_ssize_t i;

				for (i = 0; i < kws->datalen; i++) {
					kws->body[i] ^= maskp[i % 4];
				}
			}
			

			if (*oc == WSOC_PING) {
				kws_write_frame(kws, WSOC_PONG, kws->body, kws->rplen);
				goto again;
			}

			*(kws->body+kws->rplen) = '\0';
			kws->packetlen += kws->rplen;
			kws->body += kws->rplen;

			if (frag) {
				goto again;
			}

			*data = (uint8_t *)kws->bbuffer;
			
			//printf("READ[%ld][%d]-----------------------------:\n[%s]\n-------------------------------\n", kws->packetlen, *oc, (char *)*data);


			return kws->packetlen;
		}
		break;
	default:
		{
			/* invalid op code - protocol err .. */
			*oc = WSOC_CLOSE;
			return kws_close(kws, WS_PROTO_ERR);
		}
		break;
	}
}

#if 0
static ks_ssize_t ws_feed_buf(kws_t *kws, void *data, ks_size_t bytes)
{

	if (bytes + kws->wdatalen > kws->buflen) {
		return -1;
	}

	memcpy(kws->wbuffer + kws->wdatalen, data, bytes);
	
	kws->wdatalen += bytes;

	return bytes;
}

static ks_ssize_t ws_send_buf(kws_t *kws, kws_opcode_t oc)
{
	ks_ssize_t r = 0;

	if (!kws->wdatalen) {
		return -1;
	}
	
	r = ws_write_frame(kws, oc, kws->wbuffer, kws->wdatalen);
	
	kws->wdatalen = 0;

	return r;
}
#endif

KS_DECLARE(ks_ssize_t) kws_write_frame(kws_t *kws, kws_opcode_t oc, void *data, ks_size_t bytes)
{
	uint8_t hdr[14] = { 0 };
	ks_size_t hlen = 2;
	uint8_t *bp;
	ks_ssize_t raw_ret = 0;

	if (kws->down) {
		return -1;
	}

	//printf("WRITE[%ld]-----------------------------:\n[%s]\n-----------------------------------\n", bytes, (char *) data);

	hdr[0] = (uint8_t)(oc | 0x80);

	if (bytes < 126) {
		hdr[1] = (uint8_t)bytes;
	} else if (bytes < 0x10000) {
		uint16_t *u16;

		hdr[1] = 126;
		hlen += 2;

		u16 = (uint16_t *) &hdr[2];
		*u16 = htons((uint16_t) bytes);

	} else {
		uint64_t *u64;

		hdr[1] = 127;
		hlen += 8;
		
		u64 = (uint64_t *) &hdr[2];
		*u64 = hton64(bytes);
	}

	if (kws->write_buffer_len < (hlen + bytes + 1)) {
		void *tmp;

		kws->write_buffer_len = hlen + bytes + 1;
		if ((tmp = ks_pool_resize(kws->pool, kws->write_buffer, kws->write_buffer_len))) {
			kws->write_buffer = tmp;
		} else {
			abort();
		}
	}
	
	bp = (uint8_t *) kws->write_buffer;
	memcpy(bp, (void *) &hdr[0], hlen);
	memcpy(bp + hlen, data, bytes);
	
	raw_ret = kws_raw_write(kws, bp, (hlen + bytes));

	if (raw_ret != (ks_ssize_t) (hlen + bytes)) {
		return raw_ret;
	}
	
	return bytes;
}

KS_DECLARE(ks_status_t) kws_get_buffer(kws_t *kws, char **bufP, ks_size_t *buflen)
{
	*bufP = kws->buffer;
	*buflen = kws->datalen;

	return KS_STATUS_SUCCESS;
}
