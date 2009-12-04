/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/**@CFILE tport_tls.c
 * @brief TLS interface
 *
 * @author Mikko Haataja <ext-Mikko.A.Haataja@nokia.com>
 * @author Pekka Pessi <ext-Pekka.Pessi@nokia.com>
 * @author Jarod Neuner <janeuner@networkharbor.com>
 *
 */

#include "config.h"

#define OPENSSL_NO_KRB5 oh-no
#define SU_WAKEUP_ARG_T  struct tport_s

#include <sofia-sip/su_types.h>
#include <sofia-sip/su.h>
#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_wait.h>
#include <sofia-sip/su_string.h>

#include <openssl/lhash.h>
#include <openssl/bn.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#include <openssl/opensslv.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
static char const __func__[] = "tport_tls";
#endif

#if HAVE_SIGPIPE
#include <signal.h>
#endif

#if SU_HAVE_PTHREADS

#include <pthread.h>

#if __sun
#undef PTHREAD_ONCE_INIT
#define PTHREAD_ONCE_INIT {{ 0, 0, 0, PTHREAD_ONCE_NOTDONE }}
#endif

static pthread_once_t once = PTHREAD_ONCE_INIT;
#define ONCE_INIT(f) pthread_once(&once, f)

#else

static int once;
#define ONCE_INIT(f) (!once ? (once = 1), f() : (void)0)

#endif

#include "tport_tls.h"

char const tls_version[] = OPENSSL_VERSION_TEXT;
static int tls_ex_data_idx = -1; /* see SSL_get_ex_new_index(3ssl) */

static void
tls_init_once(void)
{
  SSL_library_init();
  SSL_load_error_strings();
  tls_ex_data_idx = SSL_get_ex_new_index(0, "sofia-sip private data", NULL, NULL, NULL);
}

enum  { tls_master = 0, tls_slave = 1};

struct tls_s {
  su_home_t home[1];
  SSL_CTX *ctx;
  SSL *con;
  BIO *bio_con;
  unsigned int type:1,
               accept:1,
               verify_incoming:1,
               verify_outgoing:1,
	       verify_subj_in:1,
	       verify_subj_out:1,
	       verify_date:1,
               x509_verified:1;

  /* Receiving */
  int read_events;
  void *read_buffer;
  size_t read_buffer_len;

  /* Sending */
  int   write_events;
  void *write_buffer;
  size_t write_buffer_len;

  /* Host names */
  su_strlst_t *subjects;
};

enum { tls_buffer_size = 16384 };

/** Log TLS error(s).
 *
 * Log the TLS error specified by the error code @a e and all the errors in
 * the queue. The error code @a e implies no error, and it is not logged.
 */
static
void tls_log_errors(unsigned level, char const *s, unsigned long e)
{
  if (e == 0)
    e = ERR_get_error();

  if (!tport_log->log_init)
    su_log_init(tport_log);

  if (s == NULL) s = "tls";

  for (; e != 0; e = ERR_get_error()) {
    if (level <= tport_log->log_level) {
      const char *error = ERR_lib_error_string(e);
      const char *func = ERR_func_error_string(e);
      const char *reason = ERR_reason_error_string(e);

      su_llog(tport_log, level, "%s: %08lx:%s:%s:%s\n",
	      s, e, error, func, reason);
    }
  }
}


static
tls_t *tls_create(int type)
{
  tls_t *tls = su_home_new(sizeof(*tls));

  if (tls)
    tls->type = type == tls_master ? tls_master : tls_slave;

  return tls;
}


static
void tls_set_default(tls_issues_t *i)
{
  i->verify_depth = i->verify_depth == 0 ? 2 : i->verify_depth;
  i->cert = i->cert ? i->cert : "agent.pem";
  i->key = i->key ? i->key : i->cert;
  i->randFile = i->randFile ? i->randFile : "tls_seed.dat";
  i->CAfile = i->CAfile ? i->CAfile : "cafile.pem";
  i->cipher = i->cipher ? i->cipher : "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH";
  /* Default SIP cipher */
  /* "RSA-WITH-AES-128-CBC-SHA"; */
  /* RFC-2543-compatibility ciphersuite */
  /* TLS_RSA_WITH_3DES_EDE_CBC_SHA; */
}


static
int tls_verify_cb(int ok, X509_STORE_CTX *store)
{
  if (!ok)
  {
    char data[256];

    X509 *cert = X509_STORE_CTX_get_current_cert(store);
    int  depth = X509_STORE_CTX_get_error_depth(store);
    int  err = X509_STORE_CTX_get_error(store);
    int  sslidx = SSL_get_ex_data_X509_STORE_CTX_idx();
    SSL  *ssl = X509_STORE_CTX_get_ex_data(store, sslidx);
    tls_t *tls = SSL_get_ex_data(ssl, tls_ex_data_idx);

    assert(tls);

#define TLS_VERIFY_CB_CLEAR_ERROR(OK,ERR,STORE) \
                   do {\
                     OK = 1;\
		     ERR = X509_V_OK;\
		     X509_STORE_CTX_set_error(STORE,ERR);\
		   } while (0)

    if (tls->accept && !tls->verify_incoming)
      TLS_VERIFY_CB_CLEAR_ERROR(ok, err, store);
    else if (!tls->accept && !tls->verify_outgoing)
      TLS_VERIFY_CB_CLEAR_ERROR(ok, err, store);
    else switch (err) {
      case X509_V_ERR_CERT_NOT_YET_VALID:
      case X509_V_ERR_CERT_HAS_EXPIRED:
      case X509_V_ERR_CRL_NOT_YET_VALID:
      case X509_V_ERR_CRL_HAS_EXPIRED:
        if (!tls->verify_date)
	  TLS_VERIFY_CB_CLEAR_ERROR(ok, err, store);

      default:
        break;
    }

    if (!ok) {
      SU_DEBUG_3(("-Error with certificate at depth: %i\n", depth));
      X509_NAME_oneline(X509_get_issuer_name(cert), data, 256);
      SU_DEBUG_3(("  issuer   = %s\n", data));
      X509_NAME_oneline(X509_get_subject_name(cert), data, 256);
      SU_DEBUG_3(("  subject  = %s\n", data));
      SU_DEBUG_3(("  err %i:%s\n", err, X509_verify_cert_error_string(err)));
    }

  }

  return ok;
}

static
int tls_init_context(tls_t *tls, tls_issues_t const *ti)
{
  int verify;
  static int random_loaded;

  ONCE_INIT(tls_init_once);

  if (!random_loaded) {
    random_loaded = 1;

    if (ti->randFile &&
	!RAND_load_file(ti->randFile, 1024 * 1024)) {
      if (ti->configured > 1) {
	SU_DEBUG_3(("%s: cannot open randFile %s\n",
		   "tls_init_context", ti->randFile));
	tls_log_errors(3, "tls_init_context", 0);
      }
      /* errno = EIO; */
      /* return -1; */
    }
  }

#if HAVE_SIGPIPE
  /* Avoid possible SIGPIPE when sending close_notify */
  signal(SIGPIPE, SIG_IGN);
#endif

  if (tls->ctx == NULL) {
    const SSL_METHOD *meth;

    /* meth = SSLv3_method(); */
    /* meth = SSLv23_method(); */

    if (ti->version)
      meth = TLSv1_method();
    else
      meth = SSLv23_method();

    tls->ctx = SSL_CTX_new((SSL_METHOD*)meth);
  }

  if (tls->ctx == NULL) {
    tls_log_errors(1, "tls_init_context", 0);
    errno = EIO;
    return -1;
  }

  if (!SSL_CTX_use_certificate_file(tls->ctx,
				    ti->cert,
				    SSL_FILETYPE_PEM)) {
    if (ti->configured > 0) {
      SU_DEBUG_1(("%s: invalid local certificate: %s\n",
		 "tls_init_context", ti->cert));
      tls_log_errors(3, "tls_init_context", 0);
#if require_client_certificate
      errno = EIO;
      return -1;
#endif
    }
  }

  if (!SSL_CTX_use_PrivateKey_file(tls->ctx,
                                   ti->key,
                                   SSL_FILETYPE_PEM)) {
    if (ti->configured > 0) {
      SU_DEBUG_1(("%s: invalid private key: %s\n",
		 "tls_init_context", ti->key));
      tls_log_errors(3, "tls_init_context(key)", 0);
#if require_client_certificate
      errno = EIO;
      return -1;
#endif
    }
  }

  if (!SSL_CTX_check_private_key(tls->ctx)) {
    if (ti->configured > 0) {
      SU_DEBUG_1(("%s: private key does not match the certificate public key\n",
		  "tls_init_context"));
    }
#if require_client_certificate
    errno = EIO;
    return -1;
#endif
  }

  if (!SSL_CTX_load_verify_locations(tls->ctx,
                                     ti->CAfile,
                                     ti->CApath)) {
    SU_DEBUG_1(("%s: error loading CA list: %s\n",
		 "tls_init_context", ti->CAfile));
    if (ti->configured > 0)
      tls_log_errors(3, "tls_init_context(CA)", 0);
    errno = EIO;
    return -1;
  }

  /* corresponds to (enum tport_tls_verify_policy) */
  tls->verify_incoming = (ti->policy & 0x1) ? 1 : 0;
  tls->verify_outgoing = (ti->policy & 0x2) ? 1 : 0;
  tls->verify_subj_in  = (ti->policy & 0x4) ? tls->verify_incoming : 0;
  tls->verify_subj_out = (ti->policy & 0x8) ? tls->verify_outgoing : 0;
  tls->verify_date     = (ti->verify_date)  ? 1 : 0;

  if (tls->verify_incoming)
    verify = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
  else
    verify = SSL_VERIFY_NONE;

  SSL_CTX_set_verify_depth(tls->ctx, ti->verify_depth);
  SSL_CTX_set_verify(tls->ctx, verify, tls_verify_cb);

  if (!SSL_CTX_set_cipher_list(tls->ctx, ti->cipher)) {
    SU_DEBUG_1(("%s: error setting cipher list\n", "tls_init_context"));
    tls_log_errors(3, "tls_init_context", 0);
    errno = EIO;
    return -1;
  }

  return 0;
}

void tls_free(tls_t *tls)
{
  if (!tls)
    return;

  if (tls->con != NULL)
    SSL_shutdown(tls->con);

  if (tls->ctx != NULL && tls->type != tls_slave)
    SSL_CTX_free(tls->ctx);

  if (tls->bio_con != NULL)
    BIO_free(tls->bio_con);

  su_home_unref(tls->home);
}

int tls_get_socket(tls_t *tls)
{
  int sock = -1;

  if (tls != NULL && tls->bio_con != NULL)
    BIO_get_fd(tls->bio_con, &sock);

  return sock;
}

tls_t *tls_init_master(tls_issues_t *ti)
{
  /* Default id in case RAND fails */
  unsigned char sessionId[32] = "sofia/tls";
  tls_t *tls;

#if HAVE_SIGPIPE
  signal(SIGPIPE, SIG_IGN);  /* Ignore spurios SIGPIPE from OpenSSL */
#endif

  tls_set_default(ti);

  if (!(tls = tls_create(tls_master)))
    return NULL;

  if (tls_init_context(tls, ti) < 0) {
    int err = errno;
    tls_free(tls);
    errno = err;
    return NULL;
  }

  RAND_pseudo_bytes(sessionId, sizeof(sessionId));

  SSL_CTX_set_session_id_context(tls->ctx,
                                 (void*) sessionId,
				 sizeof(sessionId));

  if (ti->CAfile != NULL)
    SSL_CTX_set_client_CA_list(tls->ctx,
                               SSL_load_client_CA_file(ti->CAfile));

#if 0
  if (sock != -1) {
    tls->bio_con = BIO_new_socket(sock, BIO_NOCLOSE);

    if (tls->bio_con == NULL) {
      tls_log_errors(1, "tls_init_master", 0);
      tls_free(tls);
      errno = EIO;
      return NULL;
    }
  }
#endif

  return tls;
}

tls_t *tls_init_secondary(tls_t *master, int sock, int accept)
{
  tls_t *tls = tls_create(tls_slave);

  if (tls) {
    tls->ctx = master->ctx;
    tls->type = master->type;
    tls->accept = accept ? 1 : 0;
    tls->verify_outgoing = master->verify_outgoing;
    tls->verify_incoming = master->verify_incoming;
    tls->verify_subj_out = master->verify_subj_out;
    tls->verify_subj_in  = master->verify_subj_in;
    tls->verify_date     = master->verify_date;
    tls->x509_verified   = master->x509_verified;

    if (!(tls->read_buffer = su_alloc(tls->home, tls_buffer_size)))
      su_home_unref(tls->home), tls = NULL;
  }
  if (!tls)
    return tls;

  assert(sock != -1);

  tls->bio_con = BIO_new_socket(sock, BIO_NOCLOSE);
  tls->con = SSL_new(tls->ctx);

  if (tls->con == NULL) {
    tls_log_errors(1, "tls_init_secondary", 0);
    tls_free(tls);
    errno = EIO;
    return NULL;
  }

  SSL_set_bio(tls->con, tls->bio_con, tls->bio_con);
  SSL_set_mode(tls->con, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
  SSL_set_ex_data(tls->con, tls_ex_data_idx, tls);

  su_setblocking(sock, 0);

  return tls;
}

su_inline
int tls_post_connection_check(tport_t *self, tls_t *tls)
{
  X509 *cert;
  int extcount;
  int i, j, error;

  if (!tls) return -1;

  cert = SSL_get_peer_certificate(tls->con);
  if (!cert) {
    SU_DEBUG_7(("%s(%p): Peer did not provide X.509 Certificate.\n", 
				__func__, (void *) self));
    if (self->tp_accepted && tls->verify_incoming)
      return X509_V_ERR_CERT_UNTRUSTED;
    else if (!self->tp_accepted && tls->verify_outgoing)
      return X509_V_ERR_CERT_UNTRUSTED;
    else 
      return X509_V_OK;
  }

  tls->subjects = su_strlst_create(tls->home);
  if (!tls->subjects)
    return X509_V_ERR_OUT_OF_MEM;

  extcount = X509_get_ext_count(cert);

  /* Find matching subjectAltName.DNS */
  for (i = 0; i < extcount; i++) {
    X509_EXTENSION *ext;
    char const *name;
#if OPENSSL_VERSION_NUMBER >  0x10000000L
    const X509V3_EXT_METHOD *vp;
#else
    X509V3_EXT_METHOD *vp;
#endif
    STACK_OF(CONF_VALUE) *values;
    CONF_VALUE *value;
    void *d2i;

    ext = X509_get_ext(cert, i);
    name = OBJ_nid2sn(OBJ_obj2nid(X509_EXTENSION_get_object(ext)));

    if (strcmp(name, "subjectAltName") != 0)
      continue;

    vp = X509V3_EXT_get(ext); if (!vp) continue;
    d2i = X509V3_EXT_d2i(ext);
    values = vp->i2v(vp, d2i, NULL);

    for (j = 0; j < sk_CONF_VALUE_num(values); j++) {
      value = sk_CONF_VALUE_value(values, j);
      if (strcmp(value->name, "DNS") == 0)
        su_strlst_dup_append(tls->subjects, value->value);
      if (strcmp(value->name, "IP") == 0)
        su_strlst_dup_append(tls->subjects, value->value);
      else if (strcmp(value->name, "URI") == 0)
        su_strlst_dup_append(tls->subjects, value->value);
    }
  }

  {
    X509_NAME *subject;
    char name[256];

    subject = X509_get_subject_name(cert);

    if (subject) {
      if (X509_NAME_get_text_by_NID(subject, NID_commonName,
				    name, sizeof name) > 0) {
	usize_t k, N = su_strlst_len(tls->subjects);
	name[(sizeof name) - 1] = '\0';

	for (k = 0; k < N; k++)
	  if (su_casematch(su_strlst_item(tls->subjects, k), name) == 0)
	    break;

	if (k >= N)
	  su_strlst_dup_append(tls->subjects, name);
      }
    }
  }

  X509_free(cert);

  error = SSL_get_verify_result(tls->con);

  if (cert && error == X509_V_OK)
    tls->x509_verified = 1;

  if (tport_log->log_level >= 7) {
    int i, len = su_strlst_len(tls->subjects);
    for (i=0; i < len; i++)
      SU_DEBUG_7(("%s(%p): Peer Certificate Subject %i: %s\n", \
				  __func__, (void *)self, i, su_strlst_item(tls->subjects, i)));
    if (i == 0)
      SU_DEBUG_7(("%s(%p): Peer Certificate provided no usable subjects.\n",
				  __func__, (void *)self));
  }

  /* Verify incoming connections */
  if (self->tp_accepted) {
    if (!tls->verify_incoming)
      return X509_V_OK;

    if (!tls->x509_verified)
      return error;

    if (tls->verify_subj_in) {
      su_strlst_t const *subjects = self->tp_pri->pri_primary->tp_subjects;
      int i, items;

      items = subjects ? su_strlst_len(subjects) : 0;
      if (items == 0)
        return X509_V_OK;

      for (i=0; i < items; i++) {
	if (tport_subject_search(su_strlst_item(subjects, i), tls->subjects))
	  return X509_V_OK;
      }
      SU_DEBUG_3(("%s(%p): Peer Subject Mismatch (incoming connection)\n", \
				  __func__, (void *)self));

      return X509_V_ERR_CERT_UNTRUSTED;
    }
  }
  /* Verify outgoing connections */
  else {
    char const *subject = self->tp_canon;
    if (!tls->verify_outgoing)
      return X509_V_OK;

    if (!tls->x509_verified || !subject)
      return error;

    if (tls->verify_subj_out) {
      if (tport_subject_search(subject, tls->subjects))
        return X509_V_OK; /* Subject match found in verified certificate chain */
      SU_DEBUG_3(("%s(%p): Peer Subject Mismatch (%s)\n", \
				  __func__, (void *)self, subject));

      return X509_V_ERR_CERT_UNTRUSTED;
    }
  }

  return error;
}

static
int tls_error(tls_t *tls, int ret, char const *who,
	      void *buf, int size)
{
  int events = 0;
  int err = SSL_get_error(tls->con, ret);

  switch (err) {
  case SSL_ERROR_WANT_WRITE:
    events = SU_WAIT_OUT;
    break;

  case SSL_ERROR_WANT_READ:
    events = SU_WAIT_IN;
    break;

  case SSL_ERROR_ZERO_RETURN:
    return 0;

  case SSL_ERROR_SYSCALL:
    if (SSL_get_shutdown(tls->con) & SSL_RECEIVED_SHUTDOWN)
      return 0;			/* EOS */
    if (errno == 0)
      return 0;			/* EOS */
    return -1;

  default:
    tls_log_errors(1, who, err);
    errno = EIO;
    return -1;
  }

  if (buf) {
    tls->write_events = events;
    tls->write_buffer = buf, tls->write_buffer_len = size;
  }
  else {
    tls->read_events = events;
  }

  errno = EAGAIN;
  return -1;
}

ssize_t tls_read(tls_t *tls)
{
  ssize_t ret;

  if (tls == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (0)
    SU_DEBUG_1(("tls_read(%p) called on %s (events %u)\n", (void *)tls,
	    tls->type ? "master" : "slave",
	    tls->read_events));

  if (tls->read_buffer_len)
    return (ssize_t)tls->read_buffer_len;

  tls->read_events = SU_WAIT_IN;

  ret = SSL_read(tls->con, tls->read_buffer, tls_buffer_size);
  if (ret <= 0)
    return tls_error(tls, ret, "tls_read: SSL_read", NULL, 0);

  return (ssize_t)(tls->read_buffer_len = ret);
}

void *tls_read_buffer(tls_t *tls, size_t N)
{
  assert(N == tls->read_buffer_len);
  tls->read_buffer_len = 0;
  return tls->read_buffer;
}

int tls_pending(tls_t const *tls)
{
  return tls && tls->con && SSL_pending(tls->con);
}

/** Check if data is available in TCP connection.
 *
 *
 *
 * @retval -1 upon an error
 * @retval 0 end-of-stream
 * @retval 1 nothing to read
 * @retval 2 there is data to read
 */
int tls_want_read(tls_t *tls, int events)
{
  if (tls && (events & tls->read_events)) {
    int ret = tls_read(tls);
    if (ret > 0)
      return 2;
    else if (ret == 0)
      return 0;
    else if (errno == EAGAIN)
      return 3;			/* ??? */
    else
      return -1;
  }

  return 1;
}

ssize_t tls_write(tls_t *tls, void *buf, size_t size)
{
  ssize_t ret;

  if (0)
    SU_DEBUG_1(("tls_write(%p, %p, "MOD_ZU") called on %s\n",
	    (void *)tls, buf, size,
	    tls && tls->type == tls_slave ? "master" : "slave"));

  if (tls == NULL || buf == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (tls->write_buffer) {
    assert(buf == tls->write_buffer);
    assert(size >= tls->write_buffer_len);
    assert(tls->write_events == 0);

    if (tls->write_events ||
	buf != tls->write_buffer ||
	size < tls->write_buffer_len) {
      errno = EIO;
      return -1;
    }

    ret = tls->write_buffer_len;

    tls->write_buffer = NULL;
    tls->write_buffer_len = 0;

    return ret;
  }

  if (size == 0)
    return 0;

  tls->write_events = 0;

  ret = SSL_write(tls->con, buf, size);
  if (ret < 0)
    return tls_error(tls, ret, "tls_write: SSL_write", buf, size);

  return ret;
}

int tls_want_write(tls_t *tls, int events)
{
  if (tls && (events & tls->write_events)) {
    int ret;
    void *buf = tls->write_buffer;
    size_t size = tls->write_buffer_len;

    tls->write_events = 0;

    /* remove buf */
    tls->write_buffer = NULL;
    tls->write_buffer_len = 0;

    ret = tls_write(tls, buf, size);

    if (ret >= 0)
      /* Restore buf */
      return tls->write_buffer = buf, tls->write_buffer_len = ret;
    else if (errno == EAGAIN)
      return 0;
    else
      return -1;
  }
  return 0;
}

int tls_events(tls_t const *tls, int mask)
{

  if (!tls)
    return mask;

  if (tls->type == tls_master)
    return mask;

  return
    (mask & ~(SU_WAIT_IN|SU_WAIT_OUT)) |
    ((mask & SU_WAIT_IN) ? tls->read_events : 0) |
    ((mask & SU_WAIT_OUT) ? tls->write_events : 0);
}

int tls_connect(su_root_magic_t *magic, su_wait_t *w, tport_t *self)
{
  tport_master_t *mr = self->tp_master;
  tport_tls_t *tlstp = (tport_tls_t *)self;
  tls_t *tls;
  int events = su_wait_events(w, self->tp_socket);
  int error;

  SU_DEBUG_7(("%s(%p): events%s%s%s%s\n", __func__, (void *)self,
              events & (SU_WAIT_CONNECT) ? " CONNECTING" : "",
              events & SU_WAIT_IN  ? " NEGOTIATING" : "",
              events & SU_WAIT_ERR ? " ERROR" : "",
              events & SU_WAIT_HUP ? " HANGUP" : ""));

#if HAVE_POLL
  assert(w->fd == self->tp_socket);
#endif

  if (events & SU_WAIT_ERR)
    tport_error_event(self);

  if (events & SU_WAIT_HUP && !self->tp_closed)
    tport_hup_event(self);

  if (self->tp_closed)
    return 0;

  error = su_soerror(self->tp_socket);
  if (error) {
    tport_error_report(self, error, NULL);
    return 0;
  }

  if ((tls = tlstp->tlstp_context) == NULL) {
    SU_DEBUG_3(("%s(%p): Error: no TLS context data for connected socket.\n",
                __func__, (void *)tlstp));
    tport_close(self);
    tport_set_secondary_timer(self);
    return 0;
  }

  if (self->tp_is_connected == 0) {
    int ret, status;

    ret = self->tp_accepted ? SSL_accept(tls->con) : SSL_connect(tls->con);
    status = SSL_get_error(tls->con, ret);

    switch (status) {
      case SSL_ERROR_WANT_READ:
        /* OpenSSL is waiting for the peer to send handshake data */
        self->tp_events = SU_WAIT_IN | SU_WAIT_ERR | SU_WAIT_HUP;
        su_root_eventmask(mr->mr_root, self->tp_index,
		          self->tp_socket, self->tp_events);
        return 0;

      case SSL_ERROR_WANT_WRITE:
        /* OpenSSL is waiting for the peer to receive handshake data */
        self->tp_events = SU_WAIT_IN | SU_WAIT_ERR | SU_WAIT_HUP | SU_WAIT_OUT;
        su_root_eventmask(mr->mr_root, self->tp_index,
                           self->tp_socket, self->tp_events);
        return 0;

      case SSL_ERROR_NONE:
        /* TLS Handshake complete */
	status = tls_post_connection_check(self, tls);
        if ( status == X509_V_OK ) {
          su_wait_t wait[1] = {SU_WAIT_INIT};
          tport_master_t *mr = self->tp_master;

          su_root_deregister(mr->mr_root, self->tp_index);
          self->tp_index = -1;
          self->tp_events = SU_WAIT_IN | SU_WAIT_ERR | SU_WAIT_HUP;

          if ((su_wait_create(wait, self->tp_socket, self->tp_events) == -1) ||
             ((self->tp_index = su_root_register(mr->mr_root, wait, tport_wakeup,
                                                           self, 0)) == -1)) {
            tport_close(self);
            tport_set_secondary_timer(self);
	    return 0;
          }

          tls->read_events = SU_WAIT_IN;
          tls->write_events = 0;
          self->tp_is_connected = 1;
	  self->tp_verified = tls->x509_verified;
	  self->tp_subjects = tls->subjects;

	  if (tport_has_queued(self))
            tport_send_event(self);
          else
            tport_set_secondary_timer(self);

          return 0;
	}
	break;

      default:
        {
	  char errbuf[64];
	  ERR_error_string_n(status, errbuf, 64);
          SU_DEBUG_3(("%s(%p): TLS setup failed (%s)\n",
					  __func__, (void *)self, errbuf));
        }
        break;
    }
  }

  /* TLS Handshake Failed or Peer Certificate did not Verify */
  tport_close(self);
  tport_set_secondary_timer(self);

  return 0;
}
