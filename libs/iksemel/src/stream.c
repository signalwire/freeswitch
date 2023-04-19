/* iksemel (XML parser for Jabber)
** Copyright (C) 2000-2007 Gurer Ozen <madcat@e-kolay.net>
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/

#include "config.h"
#ifdef HAVE_GNUTLS
#define _XOPEN_SOURCE 500
#define _GNU_SOURCE
#include <pthread.h>
#endif


#include "common.h"
#include "iksemel.h"

#ifdef HAVE_GNUTLS
#include <gnutls/gnutls.h>
#endif

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#ifdef WIN32
#include "Winsock2.h"
typedef unsigned __int32 uint32_t;
#else
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <poll.h>
#endif
#endif

#define SF_FOREIGN 1
#define SF_TRY_SECURE 2
#define SF_SECURE 4
#define SF_SERVER 8

struct stream_data {
	iksparser *prs;
	ikstack *s;
	ikstransport *trans;
	char *name_space;
	void *user_data;
	const char *server;
	iksStreamHook *streamHook;
	iksLogHook *logHook;
	iks *current;
	char *buf;
	void *sock;
	unsigned int flags;
	char *auth_username;
	char *auth_pass;
	char *cert_file;
	char *key_file;
#ifdef HAVE_GNUTLS
	gnutls_session sess;
	gnutls_certificate_credentials cred;
#elif HAVE_SSL
	SSL* ssl;
	SSL_CTX* ssl_ctx; 
#endif
};

#define WANT_READ 1
#define WANT_WRITE 0

#ifdef HAVE_SSL
#ifdef WIN32
static int sock_ready(struct stream_data *data, int ms, int w_read)
{
	int r = 0, e = 0;
	fd_set rfds, wfds, efds, *fds;
	struct timeval tv;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&efds);

	if (w_read) {
		fds = &rfds;
	} else {
		fds = &wfds;
	}

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4127 )
	FD_SET(SSL_get_fd(data->ssl), fds);
#pragma warning( pop ) 
#else
	FD_SET(SSL_get_fd(data->ssl), fds);
#endif

	tv.tv_sec = ms / 1000;
	tv.tv_usec = (ms % 1000) * ms;
	
	r = select (SSL_get_fd(data->ssl) + 1, w_read ? &rfds : NULL, w_read ? NULL : &wfds, &efds, &tv); 

	if (r > 0) {
	
#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4127 )
		e = FD_ISSET(SSL_get_fd(data->ssl), &efds);
#pragma warning( pop ) 
#else
		e = FD_ISSET(SSL_get_fd(data->ssl), &efds);
#endif		
		
		if (e) {
			r = -1;
		} else {
		
#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4127 )
			r = FD_ISSET(SSL_get_fd(data->ssl), fds);
#pragma warning( pop ) 
#else
			r = FD_ISSET(SSL_get_fd(data->ssl), fds);
#endif	
			
		}
	}
		
	return r;
}
#else
static int sock_ready(struct stream_data *data, int ms, int w_read)
{
	struct pollfd pfds[2] = { { 0 } };
	int s = 0, r = 0, w = w_read ? POLLIN : POLLOUT;
	
	pfds[0].fd = SSL_get_fd(data->ssl);
	pfds[0].events = w | POLLHUP | POLLERR;
	
	s = poll(pfds, 1, ms);


	if (s < 0) {
		r = s;
	} else if (s > 0) {
		if ((pfds[0].revents & w)) {
			r = 1;
		} else if ((pfds[0].revents & POLLHUP) || (pfds[0].revents & POLLERR)) {
			r = -1;
		}
	}

	return r;
}
#endif
#endif

#ifdef HAVE_GNUTLS
#ifndef WIN32
#include <gcrypt.h>
GCRY_THREAD_OPTION_PTHREAD_IMPL;
#endif

static size_t
tls_push (iksparser *prs, const char *buffer, size_t len)
{
	struct stream_data *data = iks_user_data (prs);
	int ret;
	ret = data->trans->send (data->sock, buffer, len);
	if (ret) return (size_t) -1;
	return len;
}

static size_t
tls_pull (iksparser *prs, char *buffer, size_t len)
{
	struct stream_data *data = iks_user_data (prs);
	int ret;
	ret = data->trans->recv (data->sock, buffer, len, -1);
	if (ret == -1) return (size_t) -1;
	return ret;
}

static int
handshake (struct stream_data *data)
{
	const int protocol_priority[] = { GNUTLS_TLS1, GNUTLS_SSL3, 0 };
	const int kx_priority[] = { GNUTLS_KX_RSA, 0 };
	const int cipher_priority[] = { GNUTLS_CIPHER_3DES_CBC, GNUTLS_CIPHER_ARCFOUR, 0};
	const int comp_priority[] = { GNUTLS_COMP_ZLIB, GNUTLS_COMP_NULL, 0 };
	const int mac_priority[] = { GNUTLS_MAC_SHA, GNUTLS_MAC_MD5, 0 };
	int ret;

#ifndef WIN32
	gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
#endif

	if (gnutls_global_init () != 0)
		return IKS_NOMEM;

	if (gnutls_certificate_allocate_credentials (&data->cred) < 0)
		return IKS_NOMEM;

	if (gnutls_init (&data->sess, GNUTLS_CLIENT) != 0) {
		gnutls_certificate_free_credentials (data->cred);
		return IKS_NOMEM;
	}

	gnutls_protocol_set_priority (data->sess, protocol_priority);
	gnutls_cipher_set_priority(data->sess, cipher_priority);
	gnutls_compression_set_priority(data->sess, comp_priority);
	gnutls_kx_set_priority(data->sess, kx_priority);
	gnutls_mac_set_priority(data->sess, mac_priority);
	gnutls_credentials_set (data->sess, GNUTLS_CRD_CERTIFICATE, data->cred);


	gnutls_transport_set_push_function (data->sess, (gnutls_push_func) tls_push);
	gnutls_transport_set_pull_function (data->sess, (gnutls_pull_func) tls_pull);
	
	gnutls_transport_set_ptr (data->sess, data->prs);

	ret = gnutls_handshake (data->sess);
	if (ret != 0) {
		gnutls_deinit (data->sess);
		gnutls_certificate_free_credentials (data->cred);
		return IKS_NET_TLSFAIL;
	}

	data->flags &= (~SF_TRY_SECURE);
	data->flags |= SF_SECURE;

	if (!(data->flags & SF_SERVER)) {
		iks_send_header (data->prs, data->server);
	}

	return IKS_OK;
} // HAVE_GNUTLS
#elif HAVE_SSL
static int wait_for_data(struct stream_data *data, int ret, int timeout)
{
	int err;
	int retval = IKS_OK;

	err = SSL_get_error(data->ssl, ret);
	
	switch(err)
	{
		case SSL_ERROR_WANT_READ:
			ret = sock_ready(data, timeout*1000, WANT_READ);
			break;
		case SSL_ERROR_WANT_WRITE:
			ret = sock_ready(data, timeout*1000, WANT_WRITE);
			break;
		default:
			if (data->logHook) {
				data->logHook(data->user_data, ERR_error_string(err, NULL), strlen(ERR_error_string(err, NULL)), 1); 
			}
			ret = -1;
			break;

			

	}

	if (ret == -1) {
		retval = IKS_NET_TLSFAIL;
	}
	
	
	ERR_clear_error();
				
	return retval;
}


#ifndef WIN32
#include <fcntl.h>
#endif

int iks_set_blocking(void *fd, int blocking)
{
#ifdef WIN32
    unsigned long mode = !blocking;
#endif
    if (!fd) {
		return -1;
	}
    
#ifdef WIN32
    if (ioctlsocket((SOCKET)(intptr_t) fd, FIONBIO, &mode)) {
        return -1;
    }

#else
    int flags;
	int mfd = (int)(intptr_t)fd;

    if ((flags = fcntl(mfd, F_GETFL, 0)) < 0) {
        return -1;
    }

    if (fcntl(mfd, F_SETFL, flags | (!blocking ? O_NONBLOCK : ~O_NONBLOCK)) < 0) {
        return -1;
    }
#endif

    return 0;
}



static int
handshake (struct stream_data *data)
{
	int ret;
	int finished;

	SSL_library_init();
	SSL_load_error_strings();
	
	if (data->flags & SF_SERVER) {
		data->ssl_ctx = SSL_CTX_new(TLSv1_server_method());
		if(!data->ssl_ctx) return IKS_NOMEM;

		if (SSL_CTX_use_certificate_file(data->ssl_ctx, data->cert_file, SSL_FILETYPE_PEM) <= 0) {
			return IKS_NET_TLSFAIL;
		}
		if (SSL_CTX_use_PrivateKey_file(data->ssl_ctx, data->key_file, SSL_FILETYPE_PEM) <= 0) {
			return IKS_NET_TLSFAIL;
		}
		SSL_CTX_set_verify(data->ssl_ctx, SSL_VERIFY_NONE, NULL);
	} else {
		data->ssl_ctx = SSL_CTX_new(TLSv1_method());
		if(!data->ssl_ctx) return IKS_NOMEM;
	}
	
	data->ssl = SSL_new(data->ssl_ctx);
	if(!data->ssl) return IKS_NOMEM;
	
	iks_set_blocking(data->sock, 0);

	if( SSL_set_fd(data->ssl, (int)(intptr_t)data->sock) != 1 ) return IKS_NOMEM;
	
	/* Set both the read and write BIO's to non-blocking mode */
	BIO_set_nbio(SSL_get_rbio(data->ssl), 1);
	BIO_set_nbio(SSL_get_wbio(data->ssl), 1);

	finished = 0;
	
	do
	{
		if (data->flags & SF_SERVER) {
			ret = SSL_accept(data->ssl);
		} else {
			ret = SSL_connect(data->ssl);
		}
		
		if( ret != 1 ) 
		{
			if( wait_for_data(data, ret, 1) != IKS_OK ) 
			{
				finished = 1; 
				SSL_free(data->ssl);
			}
		}
	} while( ret != 1 && finished != 1 );
	
	if( ret == 1 )
	{
		data->flags &= (~SF_TRY_SECURE);
		data->flags |= SF_SECURE;
	
		if (!(data->flags & SF_SERVER)) {
			iks_send_header (data->prs, data->server);
		}
	}
	
	return ret == 1 ? IKS_OK : IKS_NET_TLSFAIL;
}
#endif

static void
insert_attribs (iks *x, char **atts)
{
	if (atts) {
		int i = 0;
		while (atts[i]) {
			iks_insert_attrib (x, atts[i], atts[i+1]);
			i += 2;
		}
	}
}

#define CNONCE_LEN 4

static void
parse_digest (char *message, const char *key, char **value_ptr, char **value_end_ptr)
{
	char *t;

	*value_ptr = NULL;
	*value_end_ptr = NULL;

	t = strstr(message, key);
	if (t) {
		t += strlen(key);
		*value_ptr = t;
		while (t[0] != '\0') {
			if (t[0] != '\\' && t[1] == '"') {
				++t;
				*value_end_ptr = t;
				return;
			}
			++t;
		}
	}
}

static iks *
make_sasl_response (struct stream_data *data, char *message)
{
	iks *x = NULL;
	char *realm, *realm_end;
	char *nonce, *nonce_end;
	char cnonce[CNONCE_LEN*8 + 1];
	iksmd5 *md5;
	unsigned char a1_h[16], a1[33], a2[33], response_value[33];
	char *response, *response_coded;
	int i;

	parse_digest(message, "realm=\"", &realm, &realm_end);
	parse_digest(message, "nonce=\"", &nonce, &nonce_end);

	/* nonce is necessary for auth */
	if (!nonce || !nonce_end) return NULL;
	*nonce_end = '\0';

	/* if no realm is given use the server hostname */
	if (realm) {
		if (!realm_end) return NULL;
		*realm_end = '\0';
	} else {
		realm = (char *) data->server;
	}

	/* generate random client challenge */
	for (i = 0; i < CNONCE_LEN; ++i)
		sprintf (cnonce + i*8, "%08x", rand());

	md5 = iks_md5_new();
	if (!md5) return NULL;

	iks_md5_hash (md5, (const unsigned char*)data->auth_username, iks_strlen (data->auth_username), 0);
	iks_md5_hash (md5, (const unsigned char*)":", 1, 0);
	iks_md5_hash (md5, (const unsigned char*)realm, iks_strlen (realm), 0);
	iks_md5_hash (md5, (const unsigned char*)":", 1, 0);
	iks_md5_hash (md5, (const unsigned char*)data->auth_pass, iks_strlen (data->auth_pass), 1);
	iks_md5_digest (md5, a1_h);

	iks_md5_reset (md5);
	iks_md5_hash (md5, (const unsigned char*)a1_h, 16, 0);
	iks_md5_hash (md5, (const unsigned char*)":", 1, 0);
	iks_md5_hash (md5, (const unsigned char*)nonce, iks_strlen (nonce), 0);
	iks_md5_hash (md5, (const unsigned char*)":", 1, 0);
	iks_md5_hash (md5, (const unsigned char*)cnonce, iks_strlen (cnonce), 1);
	iks_md5_print (md5, (char*)a1);

	iks_md5_reset (md5);
	iks_md5_hash (md5, (const unsigned char*)"AUTHENTICATE:xmpp/", 18, 0);
	iks_md5_hash (md5, (const unsigned char*)data->server, iks_strlen (data->server), 1);
	iks_md5_print (md5, (char*)a2);

	iks_md5_reset (md5);
	iks_md5_hash (md5, (const unsigned char*)a1, 32, 0);
	iks_md5_hash (md5, (const unsigned char*)":", 1, 0);
	iks_md5_hash (md5, (const unsigned char*)nonce, iks_strlen (nonce), 0);
	iks_md5_hash (md5, (const unsigned char*)":00000001:", 10, 0);
	iks_md5_hash (md5, (const unsigned char*)cnonce, iks_strlen (cnonce), 0);
	iks_md5_hash (md5, (const unsigned char*)":auth:", 6, 0);
	iks_md5_hash (md5, (const unsigned char*)a2, 32, 1);
	iks_md5_print (md5, (char*)response_value);

	iks_md5_delete (md5);

	i = iks_strlen (data->auth_username) + iks_strlen (realm) +
		iks_strlen (nonce) + iks_strlen (data->server) +
		CNONCE_LEN*8 + 136;
	response = iks_malloc (i);
	if (!response) return NULL;

	sprintf (response, "username=\"%s\",realm=\"%s\",nonce=\"%s\""
		",cnonce=\"%s\",nc=00000001,qop=auth,digest-uri=\""
		"xmpp/%s\",response=%s,charset=utf-8",
		data->auth_username, realm, nonce, cnonce,
		data->server, response_value);

	response_coded = iks_base64_encode (response, 0);
	if (response_coded) {
		x = iks_new ("response");
		iks_insert_cdata (x, response_coded, 0);
		iks_free (response_coded);
	}
	iks_free (response);

	return x;
}

static void
iks_sasl_challenge (struct stream_data *data, iks *challenge)
{
	char *message;
	iks *x;
	char *tmp;

	tmp = iks_cdata (iks_child (challenge));
	if (!tmp) return;

	/* decode received blob */
	message = iks_base64_decode (tmp);
	if (!message) return;

	/* reply the challenge */
	if (strstr (message, "rspauth")) {
		x = iks_new ("response");
	} else {
		x = make_sasl_response (data, message);
	}
	if (x) {
		iks_insert_attrib (x, "xmlns", IKS_NS_XMPP_SASL);
		iks_send (data->prs, x);
		iks_delete (x);
	}
	iks_free (message);
}

static int
tagHook (struct stream_data *data, char *name, char **atts, int type)
{
	iks *x;
	int err;

	switch (type) {
		case IKS_OPEN:
		case IKS_SINGLE:
#ifdef HAVE_GNUTLS
			if (data->flags & SF_TRY_SECURE) {
				if (strcmp (name, "proceed") == 0) {
					err = handshake (data);
					return err;
				} else if (strcmp (name, "failure") == 0){
					return IKS_NET_TLSFAIL;
				}
			}
#elif HAVE_SSL
			if (data->flags & SF_TRY_SECURE) {
				if (strcmp (name, "proceed") == 0) {
					err = handshake (data);
					return err;
				} else if (strcmp (name, "failure") == 0){
					return IKS_NET_TLSFAIL;
				}
			}
#endif
			if (data->current) {
				x = iks_insert (data->current, name);
				insert_attribs (x, atts);
			} else {
				x = iks_new (name);
				insert_attribs (x, atts);
				if (iks_strcmp (name, "stream:stream") == 0) {
					err = data->streamHook (data->user_data, IKS_NODE_START, x);
					if (err != IKS_OK) return err;
					break;
				}
			}
			data->current = x;
			if (IKS_OPEN == type) break;
		case IKS_CLOSE:
			x = data->current;
			if (NULL == x) {
				err = data->streamHook (data->user_data, IKS_NODE_STOP, NULL);
				if (err != IKS_OK) return err;
				break;
			}
			if (NULL == iks_parent (x)) {
				data->current = NULL;
				if (iks_strcmp (name, "challenge") == 0)
					iks_sasl_challenge(data, x);
				else if (iks_strcmp (name, "stream:error") == 0) {
					err = data->streamHook (data->user_data, IKS_NODE_ERROR, x);
					if (err != IKS_OK) return err;
				} else {
					err = data->streamHook (data->user_data, IKS_NODE_NORMAL, x);
					if (err != IKS_OK) return err;
				}
				break;
			}
			data->current = iks_parent (x);
	}
	return IKS_OK;
}

static int
cdataHook (struct stream_data *data, char *cdata, size_t len)
{
	if (data->current) iks_insert_cdata (data->current, cdata, len);
	return IKS_OK;
}

static void
deleteHook (struct stream_data *data)
{
#ifdef HAVE_GNUTLS
	if (data->flags & SF_SECURE) {
		gnutls_bye (data->sess, GNUTLS_SHUT_WR);
		gnutls_deinit (data->sess);
		gnutls_certificate_free_credentials (data->cred);
	}
#elif HAVE_SSL
	if (data->flags & SF_SECURE) {
		if( SSL_shutdown(data->ssl) == 0 ) SSL_shutdown(data->ssl);
		SSL_free(data->ssl);
	}
#endif
	if (data->trans) data->trans->close (data->sock);
	data->trans = NULL;
	if (data->current) iks_delete (data->current);
	data->current = NULL;
	data->flags = 0;
}

iksparser *
iks_stream_new (char *name_space, void *user_data, iksStreamHook *streamHook)
{
	ikstack *s;
	struct stream_data *data;

	s = iks_stack_new (DEFAULT_STREAM_CHUNK_SIZE, 0);
	if (NULL == s) return NULL;
	data = iks_stack_alloc (s, sizeof (struct stream_data));
	memset (data, 0, sizeof (struct stream_data));
	data->s = s;
	data->prs = iks_sax_extend (s, data, (iksTagHook *)tagHook, (iksCDataHook *)cdataHook, (iksDeleteHook *)deleteHook);
	data->name_space = name_space;
	data->user_data = user_data;
	data->streamHook = streamHook;
	return data->prs;
}

void *
iks_stream_user_data (iksparser *prs)
{
	struct stream_data *data = iks_user_data (prs);

	return data->user_data;
}

void
iks_set_log_hook (iksparser *prs, iksLogHook *logHook)
{
	struct stream_data *data = iks_user_data (prs);

	data->logHook = logHook;
}

int
iks_connect_tcp (iksparser *prs, const char *server, int port)
{
#ifdef USE_DEFAULT_IO
	return iks_connect_with (prs, server, port, server, &iks_default_transport);
#else
	return IKS_NET_NOTSUPP;
#endif
}

int
iks_connect_via (iksparser *prs, const char *server, int port, const char *server_name)
{
#ifdef USE_DEFAULT_IO
	return iks_connect_with (prs, server, port, server_name, &iks_default_transport);
#else
	return IKS_NET_NOTSUPP;
#endif
}

int
iks_connect_with (iksparser *prs, const char *server, int port, const char *server_name, ikstransport *trans)
{
	struct stream_data *data = iks_user_data (prs);
	int ret;

	if (!trans->connect) return IKS_NET_NOTSUPP;

	if (!data->buf) {
		data->buf = iks_stack_alloc (data->s, NET_IO_BUF_SIZE);
		if (NULL == data->buf) return IKS_NOMEM;
	}

	ret = trans->connect (prs, &data->sock, server, port);
	if (ret) return ret;

	data->trans = trans;

	return iks_send_header (prs, server_name);
}

int
iks_connect_async (iksparser *prs, const char *server, int port, void *notify_data, iksAsyncNotify *notify_func)
{
#ifdef USE_DEFAULT_IO
	return iks_connect_async_with (prs, server, port, server, &iks_default_transport, notify_data, notify_func);
#else
	return IKS_NET_NOTSUPP;
#endif
}

int
iks_connect_async_with (iksparser *prs, const char *server, int port, const char *server_name, ikstransport *trans, void *notify_data, iksAsyncNotify *notify_func)
{
	struct stream_data *data = iks_user_data (prs);
	int ret;

	if (NULL == trans->connect_async)
		return IKS_NET_NOTSUPP;

	if (!data->buf) {
		data->buf = iks_stack_alloc (data->s, NET_IO_BUF_SIZE);
		if (NULL == data->buf) return IKS_NOMEM;
	}

	ret = trans->connect_async (prs, &data->sock, server, server_name, port, notify_data, notify_func);
	if (ret) return ret;

	data->trans = trans;
	data->server = server_name;

	return IKS_OK;
}

int
iks_connect_fd (iksparser *prs, int fd)
{
#ifdef USE_DEFAULT_IO
	struct stream_data *data = iks_user_data (prs);

	if (!data->buf) {
		data->buf = iks_stack_alloc (data->s, NET_IO_BUF_SIZE);
		if (NULL == data->buf) return IKS_NOMEM;
	}

	data->sock = (void *) (intptr_t) fd;
	data->flags |= SF_FOREIGN;
	data->trans = &iks_default_transport;

	return IKS_OK;
#else
	return IKS_NET_NOTSUPP;
#endif
}

int
iks_fd (iksparser *prs)
{
	struct stream_data *data;

	if (prs) {
		data = iks_user_data (prs);
		if (data) {
			return (int)(intptr_t) data->sock;
		}
	}
	return -1;
}

int
iks_recv (iksparser *prs, int timeout)
{
	struct stream_data *data = iks_user_data (prs);
	int len, ret;
	
#ifdef HAVE_SSL
	int   err;
#endif

	while (1) {
#ifdef HAVE_GNUTLS
		if (data->flags & SF_SECURE) {
			len = gnutls_record_recv (data->sess, data->buf, NET_IO_BUF_SIZE - 1);
			if (len > 0 && len < 5) {
				len += gnutls_record_recv (data->sess, data->buf + len, NET_IO_BUF_SIZE - 1 - len);
			} else if (len == 0) len = -1;
		} else
#elif HAVE_SSL
		if (data->flags & SF_SECURE) {
			ret = sock_ready(data, timeout*1000, WANT_READ);

			if (ret == -1) {
				return IKS_NET_TLSFAIL;
			} else if( ret == 0 ) {
				return IKS_OK;
			} else {
				len = SSL_read(data->ssl, data->buf, NET_IO_BUF_SIZE - 1);
				if (len > 0 && len < 5) {
					len += SSL_read(data->ssl, data->buf + len, NET_IO_BUF_SIZE - 1 - len);
				}
			}
			
			if( len <= 0 ) 
			{
				switch( err = SSL_get_error(data->ssl, len) ) 
				{
					case SSL_ERROR_WANT_READ:
					case SSL_ERROR_WANT_WRITE:
						return IKS_OK;
						break;
					default:
						if(data->logHook)
							data->logHook(data->user_data, ERR_error_string(err, NULL), strlen(ERR_error_string(err, NULL)), 1); 
						return IKS_NET_TLSFAIL;
						break;
				}
			}
		} else
#endif
		{
			len = data->trans->recv (data->sock, data->buf, NET_IO_BUF_SIZE - 1, timeout);
		}
		if (len < 0) return IKS_NET_RWERR;
		if (len == 0) break;
		data->buf[len] = '\0';
		if (data->logHook) data->logHook (data->user_data, data->buf, len, 1);
		ret = iks_parse (prs, data->buf, len, 0);

		if (ret != IKS_OK) return ret;
		if (!data->trans) {
			/* stream hook called iks_disconnect */
			return IKS_NET_NOCONN;
		}
		timeout = 0;
	}
	return IKS_OK;
}

int
iks_send_header (iksparser *prs, const char *to)
{
	struct stream_data *data = iks_user_data (prs);
	char *msg;
	int len, err;

	len = 91 + strlen (data->name_space) + 6 + strlen (to) + 16 + 1;
	msg = iks_malloc (len);
	if (!msg) return IKS_NOMEM;
	sprintf (msg, "<?xml version='1.0'?>"
		"<stream:stream xmlns:stream='http://etherx.jabber.org/streams' xmlns='"
		"%s' to='%s' version='1.0'>", data->name_space, to);
	err = iks_send_raw (prs, msg);
	iks_free (msg);
	if (err) return err;
	data->server = to;
	return IKS_OK;
}

int
iks_send (iksparser *prs, iks *x)
{
	return iks_send_raw (prs, iks_string (iks_stack (x), x));
}

int
iks_send_raw (iksparser *prs, const char *xmlstr)
{
	struct stream_data *data = iks_user_data (prs);
	int ret;

#ifdef HAVE_GNUTLS
	if (data->flags & SF_SECURE) {
		if (gnutls_record_send (data->sess, xmlstr, strlen (xmlstr)) < 0) return IKS_NET_RWERR;
	} else
#elif HAVE_SSL
	if (data->flags & SF_SECURE) {
		int r;

		do {
			r = SSL_write(data->ssl, xmlstr, strlen (xmlstr));
		} while (r == -1 && SSL_get_error(data->ssl, r) == SSL_ERROR_WANT_WRITE);
		
		if (r < 0) {
			return IKS_NET_RWERR;
		}

	} else
#endif
	{
		ret = data->trans->send (data->sock, xmlstr, strlen (xmlstr));
		if (ret) return ret;
	}
	if (data->logHook) data->logHook (data->user_data, xmlstr, strlen (xmlstr), 0);
	return IKS_OK;
}

void
iks_disconnect (iksparser *prs)
{
	iks_parser_reset (prs);
}

/*****  tls api  *****/

int
iks_has_tls (void)
{
#ifdef HAVE_GNUTLS
	return 1;
#elif HAVE_SSL
	return 1;
#else
	return 0;
#endif
}

int
iks_is_secure (iksparser *prs)
{
#ifdef HAVE_GNUTLS
	struct stream_data *data = iks_user_data (prs);

	return data->flags & SF_SECURE;
#elif HAVE_SSL
	struct stream_data *data = iks_user_data (prs);
	
	return data->flags & SF_SECURE;
#else
	return 0;
#endif
}
#ifdef HAVE_GNUTLS


int
iks_init(void)
{
	int ok = 0;
	
#ifndef WIN32
	gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
#endif

	if (gnutls_global_init () != 0)
		return IKS_NOMEM;

	return ok;

}
#else
int
iks_init(void)
{
	return 0;
}
#endif


int
iks_start_tls (iksparser *prs)
{
#ifdef HAVE_GNUTLS
	int ret;
	struct stream_data *data = iks_user_data (prs);

	ret = iks_send_raw (prs, "<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>");
	if (ret) return ret;
	data->flags |= SF_TRY_SECURE;
	return IKS_OK;
#elif HAVE_SSL
	int ret;
	struct stream_data *data = iks_user_data (prs);
	
	ret = iks_send_raw (prs, "<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>");
	if (ret) return ret;
	data->flags |= SF_TRY_SECURE;
	return IKS_OK;
#else
	return IKS_NET_NOTSUPP;
#endif
}

int
iks_proceed_tls (iksparser *prs, const char *cert_file, const char *key_file)
{
#ifdef HAVE_GNUTLS
	int ret;
	struct stream_data *data = iks_user_data (prs);

	ret = iks_send_raw (prs, "<proceed xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>");
	if (ret) return ret;
	data->cert_file = iks_stack_strdup(data->s, cert_file, 0);
	data->key_file = iks_stack_strdup(data->s, key_file, 0);
	data->flags |= SF_TRY_SECURE | SF_SERVER;
	return handshake (data);
#elif HAVE_SSL
	int ret;
	struct stream_data *data = iks_user_data (prs);

	ret = iks_send_raw (prs, "<proceed xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>");
	if (ret) return ret;
	data->cert_file = iks_stack_strdup(data->s, cert_file, 0);
	data->key_file = iks_stack_strdup(data->s, key_file, 0);
	data->flags |= SF_TRY_SECURE | SF_SERVER;
	return handshake (data);
#else
	return IKS_NET_NOTSUPP;
#endif
}

/*****  sasl  *****/

int
iks_start_sasl (iksparser *prs, enum ikssasltype type, char *username, char *pass)
{
	iks *x;

	x = iks_new ("auth");
	iks_insert_attrib (x, "xmlns", IKS_NS_XMPP_SASL);
	switch (type) {
		case IKS_SASL_PLAIN: {
			int len = iks_strlen (username) + iks_strlen (pass) + 2;
			char *s = iks_malloc (80+len);
			char *base64;

			iks_insert_attrib (x, "mechanism", "PLAIN");
			sprintf (s, "%c%s%c%s", 0, username, 0, pass);
			base64 = iks_base64_encode (s, len);
			iks_insert_cdata (x, base64, 0);
			iks_free (base64);
			iks_free (s);
			break;
		}
		case IKS_SASL_DIGEST_MD5: {
			struct stream_data *data = iks_user_data (prs);

			iks_insert_attrib (x, "mechanism", "DIGEST-MD5");
			data->auth_username = username;
			data->auth_pass = pass;
			break;
		}
		default:
			iks_delete (x);
			return IKS_NET_NOTSUPP;
	}
	iks_send (prs, x);
	iks_delete (x);
	return IKS_OK;
}
