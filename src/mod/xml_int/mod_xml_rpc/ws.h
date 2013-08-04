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
#define snprintf _snprintf
#endif
#include <string.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <openssl/ssl.h>
#include <../lib/abyss/src/session.h>
#include <../lib/abyss/src/conn.h>

typedef TSession ws_tsession_t;
typedef int issize_t;

struct globals_s {
	const SSL_METHOD *ssl_method;
	SSL_CTX *ssl_ctx;
	char cert[512];
	char key[512];
};

// extern struct globals_s globals;

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
	issize_t datalen;
	issize_t wdatalen;
	char *payload;
	issize_t plen;
	issize_t rplen;
	SSL *ssl;
	int handshake;
	uint8_t down;
	int secure;
	uint8_t close_sock;
	ws_tsession_t *tsession;
} wsh_t;

issize_t ws_send_buf(wsh_t *wsh, ws_opcode_t oc);
issize_t ws_feed_buf(wsh_t *wsh, void *data, size_t bytes);


issize_t ws_raw_read(wsh_t *wsh, void *data, size_t bytes);
issize_t ws_raw_write(wsh_t *wsh, void *data, size_t bytes);
issize_t ws_read_frame(wsh_t *wsh, ws_opcode_t *oc, uint8_t **data);
issize_t ws_write_frame(wsh_t *wsh, ws_opcode_t oc, void *data, size_t bytes);
int ws_init(wsh_t *wsh, ws_tsession_t *tsession, SSL_CTX *ssl_ctx, int close_sock);
issize_t ws_close(wsh_t *wsh, int16_t reason);
void ws_destroy(wsh_t *wsh);
void init_ssl(void);
void deinit_ssl(void);
int ws_handshake_kvp(wsh_t *wsh, char *key, char *version, char *proto);


#ifndef _MSC_VER
static inline uint64_t get_unaligned_uint64(const void *p)
{   
    const struct { uint64_t d; } __attribute__((packed)) *pp = p;
    return pp->d;
}
#endif

#endif
