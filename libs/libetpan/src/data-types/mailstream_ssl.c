/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2005 - DINH Viet Hoa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the libEtPan! project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id: mailstream_ssl.c,v 1.41 2006/09/25 17:33:03 colinleroy Exp $
 */

/*
  NOTE :

  The user has to call himself SSL_library_init() if he wants to
  use SSL.
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailstream_ssl.h"

#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#	include <stdlib.h>
#endif
#include <fcntl.h>

/*
  these 3 headers MUST be included before <sys/select.h>
  to insure compatibility with Mac OS X (this is true for 10.2)
*/
#ifndef _MSC_VER
#	include <sys/time.h>
#	include <sys/types.h>
#	ifdef HAVE_SYS_SELECT_H
#		include <sys/select.h>
#	endif
#endif

/* mailstream_low, ssl */

#ifdef USE_SSL
# ifndef USE_GNUTLS
#  include <openssl/ssl.h>
# else
#  include <errno.h>
#  include <gnutls/gnutls.h>
#  include <gnutls/x509.h>
# endif
# ifdef LIBETPAN_REENTRANT
#  include <pthread.h>
# endif
#endif

#ifdef USE_SSL
#ifndef USE_GNUTLS
struct mailstream_ssl_data {
  int fd;
  SSL * ssl_conn;
  SSL_CTX * ssl_ctx;
};
#else
struct mailstream_ssl_data {
  int fd;
  gnutls_session session;
};
#endif
#endif

#ifdef USE_SSL
#ifdef LIBETPAN_REENTRANT
static pthread_mutex_t ssl_lock = PTHREAD_MUTEX_INITIALIZER;
#endif
static int gnutls_init_done = 0;
static int openssl_init_done = 0;
#endif

void mailstream_gnutls_init_not_required(void)
{
#ifdef USE_SSL
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_lock(&ssl_lock);
#endif
  gnutls_init_done = 1;
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_unlock(&ssl_lock);
#endif
#endif
}

void mailstream_openssl_init_not_required(void)
{
#ifdef USE_SSL
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_lock(&ssl_lock);
#endif
  openssl_init_done = 1;
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_unlock(&ssl_lock);
#endif
#endif
}

void mailstream_ssl_init_not_required(void)
{
  mailstream_gnutls_init_not_required();
  mailstream_openssl_init_not_required();
}

static inline void mailstream_ssl_init(void)
{
#ifdef USE_SSL
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_lock(&ssl_lock);
#endif
#ifndef USE_GNUTLS
  if (!openssl_init_done) {
    SSL_library_init();
    openssl_init_done = 1;
  }
#else
  if (!gnutls_init_done) {
    gnutls_global_init();
    gnutls_init_done = 1;
  }
#endif
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_unlock(&ssl_lock);
#endif
#endif
}

#ifdef USE_SSL
static inline int mailstream_prepare_fd(int fd)
{
#ifndef WIN32
  int fd_flags;
  int r;
  
  fd_flags = fcntl(fd, F_GETFL, 0);
  fd_flags |= O_NDELAY;
  r = fcntl(fd, F_SETFL, fd_flags);
  if (r < 0)
    return -1;
#endif
  
  return 0;
}
#endif


#ifdef USE_SSL
static int mailstream_low_ssl_close(mailstream_low * s);
static ssize_t mailstream_low_ssl_read(mailstream_low * s,
				       void * buf, size_t count);
static ssize_t mailstream_low_ssl_write(mailstream_low * s,
					const void * buf, size_t count);
static void mailstream_low_ssl_free(mailstream_low * s);
static int mailstream_low_ssl_get_fd(mailstream_low * s);

#if defined (_MSC_VER) || defined (__SUNPRO_CC) || defined (__SUNPRO_C)
static mailstream_low_driver local_mailstream_ssl_driver = {
  /* mailstream_read */ mailstream_low_ssl_read,
  /* mailstream_write */ mailstream_low_ssl_write,
  /* mailstream_close */ mailstream_low_ssl_close,
  /* mailstream_free */ mailstream_low_ssl_free,
  /* mailstream_get_fd */ mailstream_low_ssl_get_fd,
#else
static mailstream_low_driver local_mailstream_ssl_driver = {
  mailstream_read: mailstream_low_ssl_read,
  mailstream_write: mailstream_low_ssl_write,
  mailstream_close: mailstream_low_ssl_close,
  mailstream_free: mailstream_low_ssl_free,
  mailstream_get_fd: mailstream_low_ssl_get_fd,
#endif
};

mailstream_low_driver * mailstream_ssl_driver = &local_mailstream_ssl_driver;
#endif

/* file descriptor must be given in (default) blocking-mode */

#ifdef USE_SSL
#ifndef USE_GNUTLS
static struct mailstream_ssl_data * ssl_data_new_full(int fd, SSL_METHOD *method)
{
  struct mailstream_ssl_data * ssl_data;
  SSL * ssl_conn;
  int r;
  SSL_CTX * tmp_ctx;
  
  mailstream_ssl_init();
  
  tmp_ctx = SSL_CTX_new(method);
  if (tmp_ctx == NULL)
    goto err;
  
  ssl_conn = (SSL *) SSL_new(tmp_ctx);
  if (ssl_conn == NULL)
    goto free_ctx;
  
  if (SSL_set_fd(ssl_conn, fd) == 0)
    goto free_ssl_conn;
  
  r = SSL_connect(ssl_conn);
  if (r <= 0)
    goto free_ssl_conn;
  
  r = mailstream_prepare_fd(fd);
  if (r < 0)
    goto free_ssl_conn;
  
  ssl_data = malloc(sizeof(* ssl_data));
  if (ssl_data == NULL)
    goto free_ctx;
  
  ssl_data->fd = fd;
  ssl_data->ssl_conn = ssl_conn;
  ssl_data->ssl_ctx = tmp_ctx;

  return ssl_data;

 free_ctx:
  SSL_CTX_free(tmp_ctx);
 free_ssl_conn:
  SSL_free(ssl_conn);
 err:
  return NULL;
}
static struct mailstream_ssl_data * ssl_data_new(int fd)
{
  return ssl_data_new_full(fd, SSLv23_client_method());
}

static struct mailstream_ssl_data * tls_data_new(int fd)
{
  return ssl_data_new_full(fd, TLSv1_client_method());
}

#else
static struct mailstream_ssl_data * ssl_data_new(int fd)
{
  struct mailstream_ssl_data * ssl_data;
  gnutls_session session;
  
  const int cipher_prio[] = { GNUTLS_CIPHER_AES_128_CBC,
		  		GNUTLS_CIPHER_3DES_CBC,
		  		GNUTLS_CIPHER_AES_256_CBC,
		  		GNUTLS_CIPHER_ARCFOUR_128, 0 };
  const int kx_prio[] = { GNUTLS_KX_DHE_RSA,
		  	   GNUTLS_KX_RSA, 
		  	   GNUTLS_KX_DHE_DSS, 0 };
  const int mac_prio[] = { GNUTLS_MAC_SHA1,
		  		GNUTLS_MAC_MD5, 0 };
  const int proto_prio[] = { GNUTLS_TLS1,
		  		  GNUTLS_SSL3, 0 };

  gnutls_certificate_credentials_t xcred;
  int r;
  
  mailstream_ssl_init();
  
  if (gnutls_certificate_allocate_credentials (&xcred) != 0)
    return NULL;

  r = gnutls_init(&session, GNUTLS_CLIENT);
  if (session == NULL || r != 0)
    return NULL;
  
  gnutls_set_default_priority(session);
  gnutls_protocol_set_priority (session, proto_prio);
  gnutls_cipher_set_priority (session, cipher_prio);
  gnutls_kx_set_priority (session, kx_prio);
  gnutls_mac_set_priority (session, mac_prio);

  gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, xcred);

  gnutls_transport_set_ptr(session, (gnutls_transport_ptr) fd);

  do {
    r = gnutls_handshake(session);
  } while (r == GNUTLS_E_AGAIN || r == GNUTLS_E_INTERRUPTED);

  if (r < 0) {
    goto free_ssl_conn;
  }

  r = mailstream_prepare_fd(fd);
  if (r < 0)
    goto free_ssl_conn;
  
  ssl_data = malloc(sizeof(* ssl_data));
  if (ssl_data == NULL)
    goto err;
  
  ssl_data->fd = fd;
  ssl_data->session = session;

  return ssl_data;

 free_ssl_conn:
  gnutls_deinit(session);
 err:
  return NULL;
}
static struct mailstream_ssl_data * tls_data_new(int fd)
{
  return ssl_data_new(fd);
}
#endif

static void  ssl_data_free(struct mailstream_ssl_data * ssl_data)
{
  free(ssl_data);
}

#ifndef USE_GNUTLS
static void  ssl_data_close(struct mailstream_ssl_data * ssl_data)
{
  SSL_free(ssl_data->ssl_conn);
  ssl_data->ssl_conn = NULL;
  SSL_CTX_free(ssl_data->ssl_ctx);
  ssl_data->ssl_ctx  = NULL;
  close(ssl_data->fd);
  ssl_data->fd = -1;
}
#else
static void  ssl_data_close(struct mailstream_ssl_data * ssl_data)
{
  gnutls_deinit(ssl_data->session);
  ssl_data->session = NULL;
  close(ssl_data->fd);
  ssl_data->fd = -1;
}
#endif

#endif

mailstream_low * mailstream_low_ssl_open_full(int fd, int starttls)
{
#ifdef USE_SSL
  mailstream_low * s;
  struct mailstream_ssl_data * ssl_data;

  if (starttls)
    ssl_data = tls_data_new(fd);
  else
    ssl_data = ssl_data_new(fd);

  if (ssl_data == NULL)
    goto err;

  s = mailstream_low_new(ssl_data, mailstream_ssl_driver);
  if (s == NULL)
    goto free_ssl_data;

  return s;

 free_ssl_data:
  ssl_data_free(ssl_data);
 err:
  return NULL;
#else
  return NULL;
#endif
}

mailstream_low * mailstream_low_ssl_open(int fd)
{
  return mailstream_low_ssl_open_full(fd, 0);
}

mailstream_low * mailstream_low_tls_open(int fd)
{
  return mailstream_low_ssl_open_full(fd, 1);
}

#ifdef USE_SSL
static int mailstream_low_ssl_close(mailstream_low * s)
{
  struct mailstream_ssl_data * ssl_data;

  ssl_data = (struct mailstream_ssl_data *) s->data;
  ssl_data_close(ssl_data);

  return 0;
}

static void mailstream_low_ssl_free(mailstream_low * s)
{
  struct mailstream_ssl_data * ssl_data;

  ssl_data = (struct mailstream_ssl_data *) s->data;
  ssl_data_free(ssl_data);
  s->data = NULL;

  free(s);
}

static int mailstream_low_ssl_get_fd(mailstream_low * s)
{
  struct mailstream_ssl_data * ssl_data;

  ssl_data = (struct mailstream_ssl_data *) s->data;
  return ssl_data->fd;
}

#ifndef USE_GNUTLS
static ssize_t mailstream_low_ssl_read(mailstream_low * s,
				       void * buf, size_t count)
{
  struct mailstream_ssl_data * ssl_data;
  int r;

  ssl_data = (struct mailstream_ssl_data *) s->data;
  
  while (1) {
    int ssl_r;
    fd_set fds_read;
    struct timeval timeout;
    
    r = SSL_read(ssl_data->ssl_conn, buf, count);
    if (r > 0)
      return r;
    
    ssl_r = SSL_get_error(ssl_data->ssl_conn, r);
    switch (ssl_r) {
    case SSL_ERROR_NONE:
      return r;
      
    case SSL_ERROR_ZERO_RETURN:
      return r;
      
    case SSL_ERROR_WANT_READ:
      timeout = mailstream_network_delay;
      
      FD_ZERO(&fds_read);
      FD_SET(ssl_data->fd, &fds_read);
      r = select(ssl_data->fd + 1, &fds_read, NULL, NULL, &timeout);
      if (r == 0)
        return -1;
      break;
      
    default:
      return -1;
    }
  }
}
#else
static ssize_t mailstream_low_ssl_read(mailstream_low * s,
				       void * buf, size_t count)
{
  struct mailstream_ssl_data * ssl_data;
  int r;

  ssl_data = (struct mailstream_ssl_data *) s->data;
  
  while (1) {
    int ssl_r;
    fd_set fds_read;
    struct timeval timeout;
    
    r = gnutls_record_recv(ssl_data->session, buf, count);
    if (r > 0)
      return r;
    
    switch (r) {
    case 0: /* closed connection */
      return -1;
      
    case GNUTLS_E_AGAIN:
    case GNUTLS_E_INTERRUPTED:
      timeout = mailstream_network_delay;
      
      FD_ZERO(&fds_read);
      FD_SET(ssl_data->fd, &fds_read);
      r = select(ssl_data->fd + 1, &fds_read, NULL, NULL, &timeout);
      if (r == 0)
        return -1;
      break;
      
    default:
      return -1;
    }
  }
}
#endif

#ifndef USE_GNUTLS
static ssize_t mailstream_low_ssl_write(mailstream_low * s,
					const void * buf, size_t count)
{
  struct mailstream_ssl_data * ssl_data;
  int ssl_r;
  fd_set fds_write;
  fd_set fds_excp;
  struct timeval timeout;
  int r;
  
  ssl_data = (struct mailstream_ssl_data *) s->data;
  
  timeout = mailstream_network_delay;
  
  FD_ZERO(&fds_write);
  FD_SET(ssl_data->fd, &fds_write);
  FD_ZERO(&fds_excp);
  FD_SET(ssl_data->fd, &fds_excp);
  
  r = select(ssl_data->fd + 1, NULL, &fds_write, &fds_excp, &timeout);
  if (r == 0)
    return -1;
  
  if (FD_ISSET(ssl_data->fd, &fds_excp))
    return -1;
  
  if (!FD_ISSET(ssl_data->fd, &fds_write))
    return 0;
  
  r = SSL_write(ssl_data->ssl_conn, buf, count);
  if (r > 0)
    return r;
  
  ssl_r = SSL_get_error(ssl_data->ssl_conn, r);
  switch (ssl_r) {
  case SSL_ERROR_NONE:
    return r;
    
  case SSL_ERROR_ZERO_RETURN:
    return -1;
    
  case SSL_ERROR_WANT_WRITE:
    return 0;
    
  default:
    return r;
  }
}
#else
static ssize_t mailstream_low_ssl_write(mailstream_low * s,
					const void * buf, size_t count)
{
  struct mailstream_ssl_data * ssl_data;
  int ssl_r;
  fd_set fds_write;
  fd_set fds_excp;
  struct timeval timeout;
  int r;
  
  ssl_data = (struct mailstream_ssl_data *) s->data;
  
  timeout = mailstream_network_delay;
  
  FD_ZERO(&fds_write);
  FD_SET(ssl_data->fd, &fds_write);
  FD_ZERO(&fds_excp);
  FD_SET(ssl_data->fd, &fds_excp);
  
  r = select(ssl_data->fd + 1, NULL, &fds_write, &fds_excp, &timeout);
  if (r == 0)
    return -1;
  
  if (FD_ISSET(ssl_data->fd, &fds_excp))
    return -1;
  
  if (!FD_ISSET(ssl_data->fd, &fds_write))
    return 0;
  
  r = gnutls_record_send(ssl_data->session, buf, count);
  if (r > 0)
    return r;
  
  switch (r) {
  case 0:
    return -1;
    
  case GNUTLS_E_AGAIN:
  case GNUTLS_E_INTERRUPTED:
    return 0;
    
  default:
    return r;
  }
}
#endif
#endif

/* mailstream */

mailstream * mailstream_ssl_open(int fd)
{
#ifdef USE_SSL
  mailstream_low * low;
  mailstream * s;

  low = mailstream_low_ssl_open(fd);
  if (low == NULL)
    goto err;

  s = mailstream_new(low, 8192);
  if (s == NULL)
    goto free_low;

  return s;

 free_low:
  mailstream_low_close(low);
 err:
  return NULL;
#else
  return NULL;
#endif
}

int mailstream_ssl_get_certificate(mailstream *stream, unsigned char **cert_DER)
{
#ifdef USE_SSL
  struct mailstream_ssl_data *data = NULL;
  int len = 0;
#ifndef USE_GNUTLS
  SSL *ssl_conn = NULL;
  X509 *cert = NULL;
#else
  gnutls_session session = NULL;
  const gnutls_datum *raw_cert_list;
  unsigned int raw_cert_list_length;
  char *exported_cert = NULL;
  gnutls_x509_crt cert = NULL;
  char output[10*1024];
#endif

  if (cert_DER == NULL || stream == NULL || stream->low == NULL)
    return -1;

  data = stream->low->data;
  
  if (data == NULL)
    return -1;

#ifndef USE_GNUTLS
  ssl_conn = data->ssl_conn;
  if (ssl_conn == NULL)
    return -1;
  cert = SSL_get_peer_certificate(ssl_conn);
  if (cert == NULL)
    return -1;
  *cert_DER = NULL;
  len = i2d_X509(cert, cert_DER);
  return len;
#else
  session = data->session;
  raw_cert_list = gnutls_certificate_get_peers(session, &raw_cert_list_length);

  if (raw_cert_list 
  && gnutls_certificate_type_get(session) == GNUTLS_CRT_X509
  &&  gnutls_x509_crt_init(&cert) >= 0
  &&  gnutls_x509_crt_import(cert, &raw_cert_list[0], GNUTLS_X509_FMT_DER) >= 0) {
    len = 10*1024;
    if (gnutls_x509_crt_export(cert, GNUTLS_X509_FMT_DER, output, &len) < 0)
      return -1;
    if (len >= 0) {
      *cert_DER = malloc (len + 1);
      if (*cert_DER == NULL)
	return -1;
      memcpy (*cert_DER, output, len);
    }
    gnutls_x509_crt_deinit(cert);
    return len;
  }
#endif
#endif
  return -1;
}
