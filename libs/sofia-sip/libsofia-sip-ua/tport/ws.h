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
#include <unistd.h>
#else
#pragma warning(disable:4996)
#endif
#include <string.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
//#include "sha1.h"
#include <openssl/ssl.h>

#if defined(_MSC_VER) || defined(__APPLE__) || defined(__FreeBSD__) || (defined(__SVR4) && defined(__sun)) 
#define __bswap_64(x) \
  x = (x>>56) | \
    ((x<<40) & 0x00FF000000000000) | \
    ((x<<24) & 0x0000FF0000000000) | \
    ((x<<8)  & 0x000000FF00000000) | \
    ((x>>8)  & 0x00000000FF000000) | \
    ((x>>24) & 0x0000000000FF0000) | \
    ((x>>40) & 0x000000000000FF00) | \
    (x<<56)
#endif
#ifdef _MSC_VER
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#if _MSC_VER < 1900
#define snprintf _snprintf
#endif
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

#ifndef WIN32
typedef int ws_socket_t;
#else
typedef SOCKET ws_socket_t;
#endif
#define ws_sock_invalid (ws_socket_t)-1


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
	char *buffer;
	char *bbuffer;
	char *body;
	char *uri;
	size_t buflen;
	size_t bbuflen;
	ssize_t datalen;
	char *payload;
	ssize_t plen;
	ssize_t rplen;
	ssize_t packetlen;
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
	size_t write_buffer_len;
} wsh_t;

ssize_t ws_send_buf(wsh_t *wsh, ws_opcode_t oc);
ssize_t ws_feed_buf(wsh_t *wsh, void *data, size_t bytes);


ssize_t ws_raw_read(wsh_t *wsh, void *data, size_t bytes, int block);
ssize_t ws_raw_write(wsh_t *wsh, void *data, size_t bytes);
ssize_t ws_read_frame(wsh_t *wsh, ws_opcode_t *oc, uint8_t **data);
ssize_t ws_write_frame(wsh_t *wsh, ws_opcode_t oc, void *data, size_t bytes);
int ws_init(wsh_t *wsh, ws_socket_t sock, SSL_CTX *ssl_ctx, int close_sock, int block, int stay_open);
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
