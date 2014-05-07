#include "ws.h"
#include <pthread.h>

#ifndef _MSC_VER
#include <fcntl.h>
#endif

#ifndef _MSC_VER
#define ms_sleep(x)	usleep( x * 1000);
#else
#define ms_sleep(x) Sleep( x );
#endif				

#define WS_BLOCK 1
#define WS_NOBLOCK 0

#define SHA1_HASH_SIZE 20
struct ws_globals_s ws_globals;

#ifndef WSS_STANDALONE

void init_ssl(void) 
{
	SSL_library_init();
}
void deinit_ssl(void) 
{
	return;
}

#else
static unsigned long pthreads_thread_id(void);
static void pthreads_locking_callback(int mode, int type, const char *file, int line);

static pthread_mutex_t *lock_cs;
static long *lock_count;



static void thread_setup(void)
{
	int i;

	lock_cs = OPENSSL_malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));
	lock_count = OPENSSL_malloc(CRYPTO_num_locks() * sizeof(long));

	for (i = 0; i < CRYPTO_num_locks(); i++) {
		lock_count[i] = 0;
		pthread_mutex_init(&(lock_cs[i]), NULL);
	}

	CRYPTO_set_id_callback(pthreads_thread_id);
	CRYPTO_set_locking_callback(pthreads_locking_callback);
}

static void thread_cleanup(void)
{
	int i;

	CRYPTO_set_locking_callback(NULL);

	for (i=0; i<CRYPTO_num_locks(); i++) {
		pthread_mutex_destroy(&(lock_cs[i]));
	}
	OPENSSL_free(lock_cs);
	OPENSSL_free(lock_count);

}

static void pthreads_locking_callback(int mode, int type, const char *file, int line)
{

	if (mode & CRYPTO_LOCK) {
		pthread_mutex_lock(&(lock_cs[type]));
		lock_count[type]++;
	} else {
		pthread_mutex_unlock(&(lock_cs[type]));
	}
}



static unsigned long pthreads_thread_id(void)
{
	return (unsigned long) pthread_self();
}


void init_ssl(void) {
	SSL_library_init();


	OpenSSL_add_all_algorithms();   /* load & register cryptos */
	SSL_load_error_strings();     /* load all error messages */
	ws_globals.ssl_method = TLSv1_server_method();   /* create server instance */
	ws_globals.ssl_ctx = SSL_CTX_new(ws_globals.ssl_method);         /* create context */
	assert(ws_globals.ssl_ctx);
	
	/* set the local certificate from CertFile */
	SSL_CTX_use_certificate_file(ws_globals.ssl_ctx, ws_globals.cert, SSL_FILETYPE_PEM);
	/* set the private key from KeyFile */
	SSL_CTX_use_PrivateKey_file(ws_globals.ssl_ctx, ws_globals.key, SSL_FILETYPE_PEM);
	/* verify private key */
	if ( !SSL_CTX_check_private_key(ws_globals.ssl_ctx) ) {
		abort();
    }

	SSL_CTX_set_cipher_list(ws_globals.ssl_ctx, "HIGH:!DSS:!aNULL@STRENGTH");

	thread_setup();
}


void deinit_ssl(void) {
	thread_cleanup();
}

#endif

static const char c64[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


static int cheezy_get_var(char *data, char *name, char *buf, size_t buflen)
{
  char *p=data;

  /* the old way didnt make sure that variable values were used for the name hunt
   * and didnt ensure that only a full match of the variable name was used
   */

  do {
    if(!strncmp(p,name,strlen(name)) && *(p+strlen(name))==':') break;
  } while((p = (strstr(p,"\n")+1))!=(char *)1);


  if (p != (char *)1 && *p!='\0') {
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
	size_t len = e - v;
	
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

static int b64encode(unsigned char *in, size_t ilen, unsigned char *out, size_t olen) 
{
	int y=0,bytes=0;
	size_t x=0;
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

#ifdef NO_OPENSSL
static void sha1_digest(char *digest, unsigned char *in)
{
	SHA1Context sha;
	char *p;
	int x;


	SHA1Init(&sha);
	SHA1Update(&sha, in, strlen(in));
	SHA1Final(&sha, digest);
}
#else 

static void sha1_digest(unsigned char *digest, char *in)
{
	SHA_CTX sha;

	SHA1_Init(&sha);
	SHA1_Update(&sha, in, strlen(in));
	SHA1_Final(digest, &sha);

}

#endif

int ws_handshake(wsh_t *wsh)
{
	char key[256] = "";
	char version[5] = "";
	char proto[256] = "";
	char proto_buf[384] = "";
	char uri[256] = "";
	char input[256] = "";
	unsigned char output[SHA1_HASH_SIZE] = "";
	char b64[256] = "";
	char respond[512] = "";
	ssize_t bytes;
	char *p, *e = 0;

	if (wsh->sock == ws_sock_invalid) {
		return -3;
	}

	while((bytes = ws_raw_read(wsh, wsh->buffer + wsh->datalen, wsh->buflen - wsh->datalen, WS_BLOCK)) > 0) {
		wsh->datalen += bytes;
		if (strstr(wsh->buffer, "\r\n\r\n") || strstr(wsh->buffer, "\n\n")) {
			break;
		}
	}

	if (bytes > sizeof(wsh->buffer) -1) {
		goto err;
	}

	*(wsh->buffer+bytes) = '\0';
	
	if (strncasecmp(wsh->buffer, "GET ", 4)) {
		goto err;
	}
	
	p = wsh->buffer + 4;
	
	e = strchr(p, ' ');
	if (!e) {
		goto err;
	}
	
	strncpy(uri, p, e-p);
	
	cheezy_get_var(wsh->buffer, "Sec-WebSocket-Key", key, sizeof(key));
	cheezy_get_var(wsh->buffer, "Sec-WebSocket-Version", version, sizeof(version));
	cheezy_get_var(wsh->buffer, "Sec-WebSocket-Protocol", proto, sizeof(proto));
	
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


	ws_raw_write(wsh, respond, strlen(respond));
	wsh->handshake = 1;

	return 0;

 err:

	snprintf(respond, sizeof(respond), "HTTP/1.1 400 Bad Request\r\n"
			 "Sec-WebSocket-Version: 13\r\n\r\n");

	//printf("ERR:\n%s\n", respond);


	ws_raw_write(wsh, respond, strlen(respond));

	ws_close(wsh, WS_NONE);

	return -1;

}

ssize_t ws_raw_read(wsh_t *wsh, void *data, size_t bytes, int block)
{
	ssize_t r;
	int err = 0;

	if (wsh->ssl) {
		do {
			r = SSL_read(wsh->ssl, data, bytes);

			ms_sleep(10);

			if (r == -1) {
				err = SSL_get_error(wsh->ssl, r);

				if (!block && err == SSL_ERROR_WANT_READ) {
					r = -2;
					goto end;
				}
			}

		} while (r == -1 && err == SSL_ERROR_WANT_READ && wsh->x < 100);

		goto end;
	}

	do {
		r = recv(wsh->sock, data, bytes, 0);
		ms_sleep(10);
	} while (r == -1 && xp_is_blocking(xp_errno()) && wsh->x < 100);
	
	if (wsh->x >= 100) {
		r = -1;
	}

 end:

	if (r > 0) {
		*((char *)data + r) = '\0';
	}

	if (r >= 0) {
		wsh->x = 0;
	}
	
	return r;
}

ssize_t ws_raw_write(wsh_t *wsh, void *data, size_t bytes)
{
	size_t r;

	if (wsh->ssl) {
		do {
			r = SSL_write(wsh->ssl, data, bytes);
		} while (r == -1 && SSL_get_error(wsh->ssl, r) == SSL_ERROR_WANT_WRITE);

		return r;
	}

	do {
		r = send(wsh->sock, data, bytes, 0);
	} while (r == -1 && xp_is_blocking(xp_errno()));

	//if (r<0) {
		//printf("wRITE FAIL: %s\n", strerror(errno));
	//}

	return r;
}

#ifdef _MSC_VER
static int setup_socket(ws_socket_t sock)
{
	unsigned long v = 1;

	if (ioctlsocket(sock, FIONBIO, &v) == SOCKET_ERROR) {
		return -1;
	}

	return 0;

}

static int restore_socket(ws_socket_t sock)
{
	unsigned long v = 0;

	if (ioctlsocket(sock, FIONBIO, &v) == SOCKET_ERROR) {
		return -1;
	}

	return 0;

}

#else

static int setup_socket(ws_socket_t sock)
{
	int flags = fcntl(sock, F_GETFL, 0);
	return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

static int restore_socket(ws_socket_t sock)
{
	int flags = fcntl(sock, F_GETFL, 0);

	flags &= ~O_NONBLOCK;

	return fcntl(sock, F_SETFL, flags);

}

#endif


static int establish_logical_layer(wsh_t *wsh)
{

	if (!wsh->sanity) {
		return -1;
	}

	if (wsh->logical_established) {
		return 0;
	}

	if (wsh->secure && !wsh->secure_established) {
		int code;

		if (!wsh->ssl) {
			wsh->ssl = SSL_new(wsh->ssl_ctx);
			assert(wsh->ssl);

			SSL_set_fd(wsh->ssl, wsh->sock);
		}

		do {
			code = SSL_accept(wsh->ssl);

			if (code == 1) {
				wsh->secure_established = 1;
				break;
			}

			if (code == 0) {
				return -1;
			}
			
			if (code < 0) {
				if (code == -1 && SSL_get_error(wsh->ssl, code) != SSL_ERROR_WANT_READ) {
					return -1;
				}
			}

			if (wsh->block) {
				ms_sleep(10);
			} else {
				ms_sleep(1);
			}

			wsh->sanity--;

			if (!wsh->block) {
				return -2;
			}

		} while (wsh->sanity > 0);
		
		if (!wsh->sanity) {
			return -1;
		}
		
	}

	while (!wsh->down && !wsh->handshake) {
		int r = ws_handshake(wsh);

		if (r < 0) {
			wsh->down = 1;
			return -1;
		}

		if (!wsh->handshake && !wsh->block) {
			return -2;
		}

	}

	wsh->logical_established = 1;
	
	return 0;
}


int ws_init(wsh_t *wsh, ws_socket_t sock, SSL_CTX *ssl_ctx, int close_sock, int block)
{
	memset(wsh, 0, sizeof(*wsh));

	wsh->sock = sock;
	wsh->block = block;
	wsh->sanity = 5000;
	wsh->ssl_ctx = ssl_ctx;

	if (!ssl_ctx) {
		ssl_ctx = ws_globals.ssl_ctx;
	}

	if (close_sock) {
		wsh->close_sock = 1;
	}

	wsh->buflen = sizeof(wsh->buffer);
	wsh->secure = ssl_ctx ? 1 : 0;

	setup_socket(sock);

	if (establish_logical_layer(wsh) == -1) {
		return -1;
	}

	if (wsh->down) {
		return -1;
	}

	return 0;
}

void ws_destroy(wsh_t *wsh)
{

	if (!wsh) {
		return;
	}

	if (!wsh->down) {
		ws_close(wsh, WS_NONE);
	}

	if (wsh->down > 1) {
		return;
	}
	
	wsh->down = 2;

	if (wsh->ssl) {
		int code;
		do {
			code = SSL_shutdown(wsh->ssl);
		} while (code == -1 && SSL_get_error(wsh->ssl, code) == SSL_ERROR_WANT_READ);

		SSL_free(wsh->ssl);
		wsh->ssl = NULL;
	}
}

ssize_t ws_close(wsh_t *wsh, int16_t reason) 
{
	
	if (wsh->down) {
		return -1;
	}

	wsh->down = 1;
	
	if (reason && wsh->sock != ws_sock_invalid) {
		uint16_t *u16;
		uint8_t fr[4] = {WSOC_CLOSE | 0x80, 2, 0};

		u16 = (uint16_t *) &fr[2];
		*u16 = htons((int16_t)reason);
		ws_raw_write(wsh, fr, 4);
	}

	restore_socket(wsh->sock);

	if (wsh->close_sock && wsh->sock != ws_sock_invalid) {
		close(wsh->sock);
	}

	wsh->sock = ws_sock_invalid;

	return reason * -1;
	
}

ssize_t ws_read_frame(wsh_t *wsh, ws_opcode_t *oc, uint8_t **data)
{
	
	ssize_t need = 2;
	char *maskp;
	int ll = 0;

 again:
	need = 2;
	maskp = NULL;
	*data = NULL;

	ll = establish_logical_layer(wsh);

	if (ll < 0) {
		return ll;
	}

	if (wsh->down) {
		return -1;
	}

	if (!wsh->handshake) {
		return ws_close(wsh, WS_PROTO_ERR);
	}

	if ((wsh->datalen = ws_raw_read(wsh, wsh->buffer, 9, wsh->block)) < 0) {
		if (wsh->datalen == -2) {
			return -2;
		}
		return ws_close(wsh, WS_PROTO_ERR);
	}
	
	if (wsh->datalen < need) {
		if ((wsh->datalen += ws_raw_read(wsh, wsh->buffer + wsh->datalen, 9 - wsh->datalen, WS_BLOCK)) < need) {
			/* too small - protocol err */
			return ws_close(wsh, WS_PROTO_ERR);
		}
	}

	*oc = *wsh->buffer & 0xf;

	switch(*oc) {
	case WSOC_CLOSE:
		{
			wsh->plen = wsh->buffer[1] & 0x7f;
			*data = (uint8_t *) &wsh->buffer[2];
			return ws_close(wsh, 1000);
		}
		break;
	case WSOC_CONTINUATION:
	case WSOC_TEXT:
	case WSOC_BINARY:
	case WSOC_PING:
	case WSOC_PONG:
		{
			//int fin = (wsh->buffer[0] >> 7) & 1;
			int mask = (wsh->buffer[1] >> 7) & 1;

			if (mask) {
				need += 4;
				
				if (need > wsh->datalen) {
					/* too small - protocol err */
					*oc = WSOC_CLOSE;
					return ws_close(wsh, WS_PROTO_ERR);
				}
			}

			wsh->plen = wsh->buffer[1] & 0x7f;
			wsh->payload = &wsh->buffer[2];

			if (wsh->plen == 127) {
				uint64_t *u64;

				need += 8;

				if (need > wsh->datalen) {
					/* too small - protocol err */
					*oc = WSOC_CLOSE;
					return ws_close(wsh, WS_PROTO_ERR);
				}

				u64 = (uint64_t *) wsh->payload;
				wsh->payload += 8;

				wsh->plen = ntohl((u_long)*u64);

			} else if (wsh->plen == 126) {
				uint16_t *u16;

				need += 2;

				if (need > wsh->datalen) {
					/* too small - protocol err */
					*oc = WSOC_CLOSE;
					return ws_close(wsh, WS_PROTO_ERR);
				}

				u16 = (uint16_t *) wsh->payload;
				wsh->payload += 2;
				wsh->plen = ntohs(*u16);
			}

			if (mask) {
				maskp = (char *)wsh->payload;
				wsh->payload += 4;
			}

			need = (wsh->plen - (wsh->datalen - need));

			if (need < 0) {
				/* invalid read - protocol err .. */
				*oc = WSOC_CLOSE;
				return ws_close(wsh, WS_PROTO_ERR);
			}

			if ((need + wsh->datalen) > (ssize_t)wsh->buflen) {
				/* too big - Ain't nobody got time fo' dat */
				*oc = WSOC_CLOSE;
				return ws_close(wsh, WS_DATA_TOO_BIG);				
			}

			wsh->rplen = wsh->plen - need;

			while(need) {
				ssize_t r = ws_raw_read(wsh, wsh->payload + wsh->rplen, need, WS_BLOCK);

				if (r < 1) {
					/* invalid read - protocol err .. */
					*oc = WSOC_CLOSE;
					return ws_close(wsh, WS_PROTO_ERR);
				}

				wsh->datalen += r;
				wsh->rplen += r;
				need -= r;
			}
			
			if (mask && maskp) {
				ssize_t i;

				for (i = 0; i < wsh->datalen; i++) {
					wsh->payload[i] ^= maskp[i % 4];
				}
			}
			

			if (*oc == WSOC_PING) {
				ws_write_frame(wsh, WSOC_PONG, wsh->payload, wsh->rplen);
				goto again;
			}
			

			*(wsh->payload+wsh->rplen) = '\0';
			*data = (uint8_t *)wsh->payload;

			//printf("READ[%ld][%d]-----------------------------:\n[%s]\n-------------------------------\n", wsh->rplen, *oc, (char *)*data);


			return wsh->rplen;
		}
		break;
	default:
		{
			/* invalid op code - protocol err .. */
			*oc = WSOC_CLOSE;
			return ws_close(wsh, WS_PROTO_ERR);
		}
		break;
	}
}

ssize_t ws_feed_buf(wsh_t *wsh, void *data, size_t bytes)
{

	if (bytes + wsh->wdatalen > wsh->buflen) {
		return -1;
	}

	memcpy(wsh->wbuffer + wsh->wdatalen, data, bytes);
	
	wsh->wdatalen += bytes;

	return bytes;
}

ssize_t ws_send_buf(wsh_t *wsh, ws_opcode_t oc)
{
	ssize_t r = 0;

	if (!wsh->wdatalen) {
		return -1;
	}
	
	r = ws_write_frame(wsh, oc, wsh->wbuffer, wsh->wdatalen);
	
	wsh->wdatalen = 0;

	return r;
}


ssize_t ws_write_frame(wsh_t *wsh, ws_opcode_t oc, void *data, size_t bytes)
{
	uint8_t hdr[14] = { 0 };
	size_t hlen = 2;

	if (wsh->down) {
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
		*u64 = htonl(bytes);
	}

	if (ws_raw_write(wsh, (void *) &hdr[0], hlen) != (ssize_t)hlen) {
		return -1;
	}

	if (ws_raw_write(wsh, data, bytes) != (ssize_t)bytes) {
		return -2;
	}
	
	return bytes;
}

#ifdef _MSC_VER

int xp_errno(void)
{
	return WSAGetLastError();
}

int xp_is_blocking(int errcode)
{
	return errcode == WSAEWOULDBLOCK || errcode == WSAEINPROGRESS;
}

#else

int xp_errno(void)
{
	return errno;
}

int xp_is_blocking(int errcode)
{
  return errcode == EAGAIN || errcode == EWOULDBLOCK || errcode == EINPROGRESS || errcode == EINTR || errcode == ETIMEDOUT;
}

#endif
