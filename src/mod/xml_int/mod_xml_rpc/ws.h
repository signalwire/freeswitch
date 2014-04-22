#ifndef _WS_H
#define _WS_H

//#define WSS_STANDALONE 1

#define WEBSOCKET_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define B64BUFFLEN 1024

#include <sys/types.h>
#ifndef _MSC_VER
#include <arpa/inet.h>
#include <sys/wait.h> 
#include <sys/socket.h>
#else
#pragma warning(disable:4996)
#endif
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
//#include "sha1.h"
#include <openssl/ssl.h>

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define snprintf _snprintf
#ifdef _WIN64
#define WS_SSIZE_T __int64
#elif _MSC_VER >= 1400
#define WS_SSIZE_T __int32 __w64
#else
#define WS_SSIZE_T __int32
#endif
typedef WS_SSIZE_T ssize_t;
#endif


struct ws_globals_s {
	const SSL_METHOD *ssl_method;
	SSL_CTX *ssl_ctx;
	char cert[512];
	char key[512];
};

extern struct ws_globals_s ws_globals;

typedef int ws_socket_t;
#define ws_sock_invalid -1


typedef enum {
	WS_NONE = 0,
	WS_NORMAL = 1000,
	WS_PROTO_ERR = 1002,
	WS_DATA_TOO_BIG = 1009
} ws_cause_t;

typedef enum {
	WSOC_CONTINUATION = 0x0,
	WSOC_TEXT = 0x1,
	WSOC_BINARY = 0x2,
	WSOC_CLOSE = 0x8,
	WSOC_PING = 0x9,
	WSOC_PONG = 0xA
} ws_opcode_t;

typedef struct wsh_s {
	ws_socket_t sock;
	char buffer[65536];
	char wbuffer[65536];
	size_t buflen;
	ssize_t datalen;
	ssize_t wdatalen;
	char *payload;
	ssize_t plen;
	ssize_t rplen;
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
	int x;
} wsh_t;

ssize_t ws_send_buf(wsh_t *wsh, ws_opcode_t oc);
ssize_t ws_feed_buf(wsh_t *wsh, void *data, size_t bytes);


ssize_t ws_raw_read(wsh_t *wsh, void *data, size_t bytes);
ssize_t ws_raw_write(wsh_t *wsh, void *data, size_t bytes);
ssize_t ws_read_frame(wsh_t *wsh, ws_opcode_t *oc, uint8_t **data);
ssize_t ws_write_frame(wsh_t *wsh, ws_opcode_t oc, void *data, size_t bytes);
int ws_init(wsh_t *wsh, ws_socket_t sock, SSL_CTX *ssl_ctx, int close_sock, int block);
ssize_t ws_close(wsh_t *wsh, int16_t reason);
void ws_destroy(wsh_t *wsh);
void init_ssl(void);
void deinit_ssl(void);
int xp_errno(void);
int xp_is_blocking(int errcode);



#ifndef _MSC_VER
static inline uint64_t get_unaligned_uint64(const void *p)
{   
    const struct { uint64_t d; } __attribute__((packed)) *pp = p;
    return pp->d;
}
#endif

#endif
