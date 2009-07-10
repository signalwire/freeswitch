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

/**@CFILE nth_client.c
 * @brief HTTP Client implementhtion
 *
 * Copyright (c) 2002 Nokia Research Center.  All rights reserved.
 *
 * This source file has been divided into following sections:
 * 1) engine
 * 2) tport handling
 * 3) client transactions
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Jun 13 02:57:51 2000 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>

#include <sofia-sip/su_string.h>

/** @internal SU message argument structure type */
#define SU_MSG_ARG_T   union sm_arg_u
/** @internal SU timer argument pointer type */
#define SU_TIMER_ARG_T struct nth_engine_s

#define MSG_HDR_T union http_header_u
#define MSG_PUB_T struct http_s

#include "sofia-sip/nth.h"
#include <sofia-sip/http_header.h>
#include <sofia-sip/http_tag.h>
#include <sofia-sip/http_status.h>

#include <sofia-sip/hostdomain.h>

#include <sofia-sip/msg_addr.h>
#include <sofia-sip/su_tagarg.h>

#include <sofia-sip/auth_client.h>

/* We are customer of tport_t */
#define TP_STACK_T   nth_engine_t
#define TP_MAGIC_T   void
#define TP_CLIENT_T  nth_client_t

#ifndef TPORT_H
#include <sofia-sip/tport.h>
#endif
#include <sofia-sip/htable.h>

#define HE_TIMER HE_TIMER
enum { HE_TIMER = 1000 };

/** @c http_flag telling that this message is internally generated. */
#define NTH_INTERNAL_MSG (1<<16)

HTABLE_DECLARE_WITH(hc_htable, hct, nth_client_t, uintptr_t, size_t);

struct nth_engine_s {
  su_home_t he_home[1];
  su_root_t *he_root;
  su_timer_t *he_timer;
  int he_mflags;			/**< Message flags */
  msg_mclass_t const *he_mclass;

  tport_t *he_tports;

  url_t *he_default_proxy;

  unsigned he_now;
  unsigned he_expires;

  /* Attributes */
  unsigned he_streaming:1;		/**< Enable streaming */
  unsigned he_error_msg:1;
  unsigned:0;

  /* Statistics */
  struct {
    uint32_t st_requests;		/**< Sent requests */
    uint32_t st_1xxresponses;		/**< Received 1XX responses */
    uint32_t st_responses;		/**< Received responses */
    uint32_t st_tp_errors;		/**< Transport errors */
    uint32_t st_timeouts;		/**< Timeouts */
    uint32_t st_bad;			/**< Bad responses*/
  } he_stats[1];

  /** Table for client transactions */
  hc_htable_t he_clients[1];
};

struct nth_client_s {
  nth_engine_t *hc_engine;
  nth_response_f *hc_callback;
  nth_client_magic_t *hc_magic;

  http_method_t hc_method;
  char const *hc_method_name;
  url_t const *hc_url;			/**< Original RequestURI  */

  unsigned hc_timeout;		        /**< Client timeout */
  unsigned hc_expires;		        /**< Client expires */

  /* Request state */
  unsigned short hc_status;
  unsigned hc_destroyed:1;
  unsigned hc_completed:1;
  unsigned hc_inserted:1;
  unsigned hc_is_streaming:1;		/**< Currently streaming response */

  /* Attributes */
  unsigned hc_streaming:1;		/**< Enable streaming */
  unsigned hc_error_msg:1;
  unsigned /* pad */:0;

  url_string_t const *hc_route_url;
  tp_name_t hc_tpn[1];			/**< Where to send requests */
  tport_t *hc_tport;
  int hc_pending;			/**< Request is pending in tport */
  tagi_t *hc_tags;			/**< Transport tags */

  auth_client_t **hc_auc;		/**< List of authenticators */

  msg_t *hc_request;
  msg_t *hc_response;
};


/* ====================================================================== */
/* Debug log settings */

#define SU_LOG   nth_client_log

#ifdef SU_DEBUG_H
#error <su_debug.h> included directly.
#endif
#include <sofia-sip/su_debug.h>

/**@var NTH_DEBUG
 *
 * Environment variable determining the debug log level for @b nth
 * module.
 *
 * The NTH_DEBUG environment variable is used to determine the debug
 * logging level for @b nth module. The default level is 1.
 *
 * @sa <sofia-sip/su_debug.h>, nth_client_log, #SOFIA_DEBUG
 */
extern char const NTH_DEBUG[];

#ifndef SU_DEBUG
#define SU_DEBUG 1
#endif

/**Debug log for @b nth module.
 *
 * The nth_client_log is the log object used by @b nth client. The level of
 * #nth_client_log is set using #NTH_DEBUG environment variable.
 */
su_log_t nth_client_log[] = { SU_LOG_INIT("nth", "NTH_DEBUG", SU_DEBUG) };

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
static char const __func__[] = "nth";
#endif

/* ====================================================================== */
/* Internal message passing */

union sm_arg_u {
  struct hc_recv_s {
    nth_client_t *hc;
    msg_t *msg;
    http_t *http;
  } hc_recv[1];
};

/* ====================================================================== */
/* Internal prototypes */

tagi_t nth_client_tags[] = {
  {nthtag_mclass, 0},
  {nthtag_message, 0},
  {nthtag_mflags, 0},

  {nthtag_proxy, 0},
  {nthtag_error_msg, 0},
  {nthtag_template, 0},
  {nthtag_authentication, 0},

  {TAG_NEXT(tport_tags)}
};

/* ====================================================================== */
/* Internal prototypes */

static int he_create_tports(nth_engine_t * he, tagi_t *tags);
static int he_timer_init(nth_engine_t * he);
static void he_timer(su_root_magic_t *, su_timer_t *, nth_engine_t * he);
static void hc_timer(nth_engine_t * he, nth_client_t * hc, uint32_t now);
static uint32_t he_now(nth_engine_t const *he);
static void he_recv_message(nth_engine_t * he, tport_t * tport,
			    msg_t *msg, void *arg, su_time_t now);
static msg_t *he_msg_create(nth_engine_t * he, int flags,
			    char const data[], usize_t dlen,
			    tport_t const *tport, nth_client_t * hc);
static void he_tp_error(nth_engine_t * he,
			tport_t * tport, int errcode, char const *remote);
static int hc_recv(nth_client_t * hc, msg_t *msg, http_t * http);

HTABLE_PROTOS_WITH(hc_htable, hct, nth_client_t, uintptr_t, size_t);

#define HTABLE_HASH_CLIENT(hc) ((uintptr_t)(hc)->hc_tport)
HTABLE_BODIES_WITH(hc_htable, hct, nth_client_t, HTABLE_HASH_CLIENT,
		   uintptr_t, size_t);

static url_string_t const *hc_request_complete(nth_client_t * hc,
					       msg_t *msg, http_t * http,
					       http_method_t method,
					       char const *name,
					       url_string_t const *url,
					       char const *version,
					       url_t const *parent);
static
int hc_request_authenticate(nth_client_t * hc,
			    msg_t *msg, http_t * http,
			    url_string_t const *uri, auth_client_t **auc);
static
nth_client_t *hc_create(nth_engine_t * he,
			nth_response_f * callback,
			nth_client_magic_t * magic,
			msg_t *msg, tag_type_t tag, tag_value_t value, ...);
static int hc_resolve_and_send(nth_client_t * hc);
static nth_client_t *hc_send(nth_client_t * hc);
static void hc_insert(nth_engine_t * he, nth_client_t * hc);
static void hc_free(nth_client_t * hc);
static void hc_tport_error(nth_engine_t *, nth_client_t * hc,
			   tport_t * tp, msg_t *msg, int error);
static int hc_reply(nth_client_t * hc, int status, char const *phrase);
static int hc_default_cb(nth_client_magic_t * magic,
			 nth_client_t * request, http_t const *http);

/* ---------------------------------------------------------------------- */

char const *nth_engine_version(void)
{
  return "sofia-http-client/" NTH_CLIENT_VERSION;
}

/* ---------------------------------------------------------------------- */

nth_engine_t *nth_engine_create(su_root_t *root,
				tag_type_t tag, tag_value_t value, ...)
{
  nth_engine_t *he;
  ta_list ta;

  if ((he = su_home_new(sizeof(*he)))) {
    he->he_root = root;
    he->he_mflags = MSG_DO_CANONIC;
    he->he_mclass = http_default_mclass();
    he->he_expires = 32000;

    ta_start(ta, tag, value);

    if (hc_htable_resize(he->he_home, he->he_clients, 0) < 0 ||
	he_create_tports(he, ta_args(ta)) < 0 ||
	he_timer_init(he) < 0 || nth_engine_set_params(he, ta_tags(ta)) < 0) {
      nth_engine_destroy(he), he = NULL;
    }

    ta_end(ta);
  }

  return he;
}

void nth_engine_destroy(nth_engine_t * he)
{
  if (he) {
    size_t i;
    hc_htable_t *hct = he->he_clients;

    for (i = 0; i < hct->hct_size; i++)
      hc_free(hct->hct_table[i]);

    tport_destroy(he->he_tports);

    su_timer_destroy(he->he_timer), he->he_timer = NULL;

    su_home_unref(he->he_home);
  }
}

int nth_engine_set_params(nth_engine_t * he,
			  tag_type_t tag, tag_value_t value, ...)
{
  int n;
  ta_list ta;
  unsigned expires;
  int error_msg;
  msg_mclass_t const *mclass;
  int mflags;
  int streaming;
  url_string_t const *proxy;

  if (he == NULL)
    return (errno = EINVAL), -1;

  ta_start(ta, tag, value);

  expires = he->he_expires;
  error_msg = he->he_error_msg;
  mclass = he->he_mclass;
  mflags = he->he_mflags;
  streaming = he->he_streaming;
  proxy = (void *) he->he_default_proxy;

  n = tl_gets(ta_args(ta),
	      NTHTAG_EXPIRES_REF(expires),
	      NTHTAG_ERROR_MSG_REF(error_msg),
	      NTHTAG_MCLASS_REF(mclass),
	      NTHTAG_MFLAGS_REF(mflags),
	      NTHTAG_STREAMING_REF(streaming),
	      NTHTAG_PROXY_REF(proxy), TAG_END());

  if (n > 0) {
    if (proxy->us_url != he->he_default_proxy) {
      url_t *copy = url_hdup(he->he_home, proxy->us_url);

      if (proxy && !copy) {
	n = -1;
      } else {
	su_free(he->he_home, (void *) he->he_default_proxy);
	he->he_default_proxy = copy;
      }
    }
  }

  if (n > 0) {
    he->he_expires = expires;
    he->he_error_msg = error_msg != 0;
    if (mclass)
      he->he_mclass = mclass;
    else
      he->he_mclass = http_default_mclass();
    he->he_mflags = mflags;
    he->he_streaming = streaming != 0;
  }

  ta_end(ta);

  return n;
}

int nth_engine_get_params(nth_engine_t const *he,
			  tag_type_t tag, tag_value_t value, ...)
{
  int n;
  ta_list ta;
  msg_mclass_t const *mclass;

  if (he == NULL)
    return (errno = EINVAL), -1;

  if (he->he_mclass != http_default_mclass())
    mclass = he->he_mclass;
  else
    mclass = NULL;

  ta_start(ta, tag, value);

  n = tl_tgets(ta_args(ta),
	       NTHTAG_ERROR_MSG(he->he_error_msg),
	       NTHTAG_MCLASS(mclass),
	       NTHTAG_MFLAGS(he->he_mflags),
	       NTHTAG_EXPIRES(he->he_expires),
	       NTHTAG_STREAMING(he->he_streaming),
	       NTHTAG_PROXY((url_string_t *) he->he_default_proxy),
	       TAG_END());

  ta_end(ta);

  return n;
}

int nth_engine_get_stats(nth_engine_t const *he,
			 tag_type_t tag, tag_value_t value, ...)
{
  int n;
  ta_list ta;

  if (he == NULL)
    return (errno = EINVAL), -1;

  ta_start(ta, tag, value);

  n = tl_tgets(ta_args(ta), TAG_END());

  ta_end(ta);

  return n;
}

static tp_name_t he_name[1] = { {"*", "*", "*", "*"} };

static char const *const he_tports[] = {
  "tcp", "tls", NULL
};

static char const *const he_no_tls_tports[] = { "tcp", NULL };

static tp_stack_class_t http_client_class[1] = { {
						  sizeof(http_client_class),
						  he_recv_message,
						  he_tp_error,
						  he_msg_create}
};

/** Create transports for client engine */
static
int he_create_tports(nth_engine_t * he, tagi_t *tags)
{
  he->he_tports = tport_tcreate(he, http_client_class, he->he_root, TAG_END());

  if (!he->he_tports)
    return -1;

  if (tport_tbind(he->he_tports, he_name, he_tports,
		  TPTAG_SERVER(0), TAG_NEXT(tags)) >= 0)
    return 0;

  return tport_tbind(he->he_tports, he_name, he_no_tls_tports,
		     TPTAG_SERVER(0), TAG_NEXT(tags));
}

/** Initialize engine timer. */
static
int he_timer_init(nth_engine_t * he)
{
  he->he_timer = su_timer_create(su_root_task(he->he_root), HE_TIMER);
  return su_timer_set(he->he_timer, he_timer, he);
}

/**
 * Engine timer routine.
 */
static
void he_timer(su_root_magic_t *rm, su_timer_t *timer, nth_engine_t * he)
{
  unsigned i;
  uint32_t now;
  hc_htable_t *hct = he->he_clients;

  now = su_time_ms(su_now());
  now += now == 0;
  he->he_now = now;

  if (hct)
    for (i = hct->hct_size; i > 0;)
      if (hct->hct_table[--i])
	hc_timer(he, hct->hct_table[i], now);

  su_timer_set(timer, he_timer, he);

  he->he_now = 0;
}

/** Get current timestamp in milliseconds */
static
uint32_t he_now(nth_engine_t const *he)
{
  if (he->he_now)
    return he->he_now;
  else
    return su_time_ms(su_now());
}

static
void he_recv_message(nth_engine_t * he,
		     tport_t * tport, msg_t *msg, void *arg, su_time_t now)
{
  nth_client_t *hc, **hcp;
  tp_name_t const *tpn;

  for (hcp = hc_htable_hash(he->he_clients, (hash_value_t)(uintptr_t) tport);
       (hc = *hcp); hcp = hc_htable_next(he->he_clients, hcp)) {
    if (hc->hc_tport == tport) {
      if (hc_recv(hc, msg, http_object(msg)) < 0)
	msg_destroy(msg);
      return;
    }
  }

  /* Extra response? Framing error? */

  tpn = tport_name(tport);

  if (msg_size(msg))
    SU_DEBUG_3(("nth client: received extra data ("MOD_ZU" bytes) "
		"from %s/%s:%s\n",
		(size_t)msg_size(msg),
		tpn->tpn_proto, tpn->tpn_host, tpn->tpn_port));
  else
    SU_DEBUG_3(("nth client: received extra data from %s/%s:%s\n",
		tpn->tpn_proto, tpn->tpn_host, tpn->tpn_port));

  msg_destroy(msg);
  tport_shutdown(tport, 2);
}

/** Report error from transport */
static void he_tp_error(nth_engine_t * he,
			tport_t * tport, int errcode, char const *remote)
{
  su_log("\nth: tport: %s%s%s\n",
	 remote ? remote : "", remote ? ": " : "", su_strerror(errcode));
}

/** Create a new message. */
msg_t *nth_engine_msg_create(nth_engine_t * he, int flags)
{
  if (he == NULL) {
    errno = EINVAL;
    return NULL;

  }

  flags |= he->he_mflags;

  if (he->he_streaming)
    flags |= MSG_FLG_STREAMING;
  else
    flags &= ~MSG_FLG_STREAMING;

  return msg_create(he->he_mclass, flags);
}


/** Create a new message for transport */
static
msg_t *he_msg_create(nth_engine_t * he, int flags,
		     char const data[], usize_t dlen,
		     tport_t const *tport, nth_client_t * hc)
{

  flags |= he->he_mflags;

  if (he->he_streaming)
    flags |= MSG_FLG_STREAMING;
  else
    flags &= ~MSG_FLG_STREAMING;

  if (hc == NULL) {
    nth_client_t **slot;
    for (slot = hc_htable_hash(he->he_clients, (hash_value_t)(uintptr_t) tport);
	 (hc = *slot); slot = hc_htable_next(he->he_clients, slot))
      if (hc->hc_tport == tport)
	break;
  }

  if (hc) {
    if (hc->hc_method == http_method_head) {
      flags &= ~MSG_FLG_STREAMING;
      flags |= HTTP_FLG_NO_BODY;
    }
  }

  return msg_create(he->he_mclass, flags);
}

/** Get destination name from Host header and request URI. */
static
int tpn_by_host(tp_name_t * tpn, http_host_t const *h, url_t const *url)
{
  if (!h || !url)
    return -1;

  tpn->tpn_proto = url_tport_default((enum url_type_e)url->url_type);
  tpn->tpn_canon = h->h_host;
  tpn->tpn_host = h->h_host;
  if (h->h_port)
    tpn->tpn_port = h->h_port;
  else
    tpn->tpn_port = url_port_default((enum url_type_e)url->url_type);

  return 0;
}


/* ---------------------------------------------------------------------- */

nth_client_t *nth_client_tcreate(nth_engine_t * engine,
				 nth_response_f * callback,
				 nth_client_magic_t * magic,
				 http_method_t method, char const *name,
				 url_string_t const *uri,
				 tag_type_t tag, tag_value_t value, ...)
{
  nth_client_t *hc = NULL;
  ta_list ta;

  if (engine) {
    void *none = &none;
    msg_t *msg = none;
    http_t *http;
    char const *version = http_version_1_1;
    nth_client_t const *template = NULL;
    auth_client_t **auc = none;
    unsigned expires = engine->he_expires;
    int ok = 0;

    ta_start(ta, tag, value);

    tl_gets(ta_args(ta),
	    NTHTAG_TEMPLATE_REF(template),
	    NTHTAG_AUTHENTICATION_REF(auc),
	    NTHTAG_MESSAGE_REF(msg),
	    NTHTAG_EXPIRES_REF(expires),
	    HTTPTAG_VERSION_REF(version),
	    TAG_END());

    if (msg == none) {
      if (template && template->hc_request)
	msg = msg_copy(template->hc_request);
      else
	msg = msg_create(engine->he_mclass, engine->he_mflags);
    }
    http = http_object(msg);

    if (template) {
      if (callback == NULL)
	callback = template->hc_callback;
      if (magic == NULL)
	magic = template->hc_magic;
      if (name == NULL)
	method = template->hc_method, name = template->hc_method_name;
      if (uri == NULL)
	uri = (url_string_t *) template->hc_url;

      if (auc == none)
	auc = template->hc_auc;
    } else if (auc == none) {
      auc = NULL;
    }

    hc = hc_create(engine, callback, magic, msg, ta_tags(ta));

    if (hc)
      hc->hc_expires = expires;

    if (hc == NULL)
      ;
    else if (http_add_tl(msg, http, ta_tags(ta)) < 0)
      ;
    else if (!(uri = hc_request_complete(hc, msg, http,
					 method, name, uri, version,
					 nth_client_url(template))))
      ;
    else if (auc && hc_request_authenticate(hc, msg, http, uri, auc) <= 0)
      ;
    else if (hc_resolve_and_send(hc) < 0)
      ;
    else
      ok = 1;

    if (!ok) {
      if (hc)
	hc_free(hc);
      else
	msg_destroy(msg);
      hc = NULL;
    }

    ta_end(ta);
  }

  return hc;
}

static
url_string_t const *hc_request_complete(nth_client_t * hc,
					msg_t *msg, http_t * http,
					http_method_t method,
					char const *name,
					url_string_t const *uri,
					char const *version,
					url_t const *parent)
{
  su_home_t *home = msg_home(msg);
  http_host_t *host = http->http_host;
  void *tbf = NULL;
  url_t const *url;
  url_t u[1];

  if (uri == NULL && http->http_request)
    uri = (url_string_t *) http->http_request->rq_url;

  if (uri == NULL)
    uri = (url_string_t *) parent;

  url = url_string_p(uri) ? (tbf = url_hdup(NULL, uri->us_url)) : uri->us_url;

  if (!url)
    return NULL;

  *u = *url;

  if (u->url_type == url_unknown && u->url_path && !u->url_host) {
    if (parent) {
      *u = *parent;
      u->url_path = url->url_path;	/* XXX - relative URLs! */
      u->url_params = url->url_params;
      u->url_headers = url->url_headers;	/* Query */
    }
  }

  if (!hc->hc_route_url && u->url_type != url_http
      && u->url_type != url_https)
    hc->hc_route_url = (url_string_t *) u;

  if (host &&
      (host_cmp(host->h_host, u->url_host) ||
       su_strcmp(host->h_port, u->url_port)))
    host = NULL;

  if (host == NULL && u->url_host) {
    host = http_host_create(home, u->url_host, u->url_port);
    msg_header_insert(msg, http, (http_header_t *) host);
  }

  if (u->url_host || hc->hc_route_url || host)
    hc->hc_url = url_hdup(home, u);

  if (hc->hc_route_url == (url_string_t *) u)
    hc->hc_route_url = (url_string_t *) hc->hc_url;

  if (hc->hc_url) {
    http_request_t *rq = http->http_request;

    if (rq && !method && !name)
      method = rq->rq_method, name = rq->rq_method_name;
    else if (rq && method && method != rq->rq_method)
      rq = NULL;
    else if (rq && name && strcmp(name, rq->rq_method_name))
      rq = NULL;

    if (rq && version && !su_casematch(version, rq->rq_version))
      rq = NULL;

    if (!hc->hc_route_url) {
      u->url_type = url_unknown, u->url_scheme = NULL;
      u->url_user = NULL, u->url_password = NULL;
      u->url_host = NULL, u->url_port = NULL;
      u->url_root = '/';
      if (!u->url_path)
	u->url_path = "";
      u->url_fragment = NULL;
    }

    if (rq && http_url_cmp(u, rq->rq_url))
      rq = NULL;

    if (!rq) {
      if (http->http_request)
	msg_header_remove(msg, http, (msg_header_t *) http->http_request);

      http->http_request =
	http_request_create(home, method, name, (url_string_t *) u, version);

      if (!http->http_request)
	uri = NULL;
    }
  } else {
    uri = NULL;
  }

  if (http_message_complete(msg, http) < 0)
    uri = NULL;

  if (tbf)
    su_free(NULL, tbf);

  if (uri) {
    hc->hc_method = http->http_request->rq_method;
    hc->hc_method_name = http->http_request->rq_method_name;
  }

  return uri;
}

static
int hc_request_authenticate(nth_client_t * hc,
			    msg_t *msg,
			    http_t * http,
			    url_string_t const *uri, auth_client_t **auc)
{
  return auc_authorization(auc, msg, http,
			   http->http_request->rq_method_name,
			   uri->us_url, http->http_payload);
}

static
nth_client_t *hc_create(nth_engine_t * he,
			nth_response_f * callback,
			nth_client_magic_t * magic,
			msg_t *msg, tag_type_t tag, tag_value_t value, ...)
{
  nth_client_t *hc;
  su_home_t *home = msg_home(msg);

  if (!(hc = su_zalloc(he->he_home, sizeof(*hc))))
    return NULL;

  if (!callback)
    callback = hc_default_cb;

  {
    int error_msg = he->he_error_msg;
    int streaming = he->he_streaming;
    url_string_t const *route_url = NULL;

    ta_list ta;
    ta_start(ta, tag, value);

    route_url = (url_string_t *) he->he_default_proxy;

    tl_gets(ta_args(ta),
	    NTHTAG_PROXY_REF(route_url),
	    NTHTAG_ERROR_MSG_REF(error_msg),
	    NTHTAG_STREAMING_REF(streaming), TAG_END());

    hc->hc_engine = he;
    hc->hc_callback = callback;
    hc->hc_magic = magic;
    hc->hc_tags = tl_afilter(home, tport_tags, ta_args(ta));
    hc->hc_error_msg = error_msg;
    hc->hc_streaming = streaming;
    hc->hc_route_url = route_url;

    ta_end(ta);
  }

  hc->hc_request = msg;

  return hc;
}


static
int hc_resolve_and_send(nth_client_t * hc)
{
  msg_t *msg = hc->hc_request;
  http_t *http = http_object(msg);
  su_home_t *home = msg_home(msg);
  int resolved = -1;

  if (hc->hc_route_url) {
    resolved = tport_name_by_url(home, hc->hc_tpn, hc->hc_route_url);
  } else {
    resolved = tpn_by_host(hc->hc_tpn, http->http_host, hc->hc_url);
  }

  if (resolved < 0) {
    SU_DEBUG_3(("nth client resolve: %s\n", "cannot resolve URL"));
    return -1;
  }

  hc->hc_route_url = NULL;

  hc->hc_tport = tport_by_name(hc->hc_engine->he_tports, hc->hc_tpn);

  if (!hc->hc_tport) {
    assert(hc->hc_tport);
    SU_DEBUG_3(("nth client create: %s\n",
		!hc->hc_tport ? "no transport" : "invalid message"));
    return -1;
  }

  if (msg_serialize(msg, http) < 0) {
    assert(hc->hc_tport);
    SU_DEBUG_3(("nth client create: invalid message"));
    return -1;
  }

  hc_send(hc);

  hc_insert(hc->hc_engine, hc);

  return 0;
}

/**@internal
 * Insert client request to the hash table
 */
static
void hc_insert(nth_engine_t * he, nth_client_t * hc)
{
  if (hc_htable_is_full(he->he_clients))
    hc_htable_resize(he->he_home, he->he_clients, 0);
  hc_htable_insert(he->he_clients, hc);
  hc->hc_inserted = 1;
}

/**@internal
 * Remove client request from the hash table
 */
static
void hc_remove(nth_engine_t * he, nth_client_t * hc)
{
  if (hc->hc_inserted)
    hc_htable_remove(he->he_clients, hc);
  hc->hc_inserted = 0;
}

/** Destroy client request. */
void nth_client_destroy(nth_client_t * hc)
{
  if (hc == NULL)
    ;
  else if (hc->hc_completed)
    hc_free(hc);
  else
    hc->hc_callback = hc_default_cb;
}

/**@internal Free client request. */
void hc_free(nth_client_t * hc)
{
  if (hc) {
    if (hc->hc_pending)
      tport_release(hc->hc_tport, hc->hc_pending, hc->hc_request, NULL, hc,
		    0);
    tport_decref(&hc->hc_tport);
    msg_destroy(hc->hc_request);
    msg_destroy(hc->hc_response);
    su_free(hc->hc_engine->he_home, hc);
  }
}

/**
 * Gets client status.
 *
 * @param hc pointer to a nth client object
 *
 * @return
 * Returns the status code from the response message if it has been
 * received. A status code below 100 indicates that no response has been
 * received. If request timeouts, the connection is closed and the status
 * code is set to 408. If @a hc is NULL, returns 400 (Bad Request).
 */
int nth_client_status(nth_client_t const *hc)
{
  return hc ? hc->hc_status : 400;
}

/**
 * Gets client method.
 *
 * @param hc pointer to a nth client object
 *
 * @return
 * Returns the HTTP method from the request.
 * If @a hc is NULL, returns #http_method_invalid.
 */
http_method_t nth_client_method(nth_client_t const *hc)
{
  return hc ? hc->hc_method : http_method_invalid;
}

/** Get original Request-URI */
url_t const *nth_client_url(nth_client_t const *hc)
{
  return hc ? hc->hc_url : NULL;
}

/** Get request message. */
msg_t *nth_client_request(nth_client_t * hc)
{
  msg_t *request = NULL;

  if (hc)
    request = hc->hc_request, hc->hc_request = NULL;

  return request;
}

/** Get response message. */
msg_t *nth_client_response(nth_client_t const *hc)
{
  if (hc)
    return msg_ref_create(hc->hc_response);
  else
    return NULL;
}

/** Is client streaming response? */
int nth_client_is_streaming(nth_client_t const *hc)
{
  return hc && hc->hc_is_streaming;
}

/** Send request. */
static nth_client_t *hc_send(nth_client_t * hc)
{
  nth_engine_t *he = hc->hc_engine;
  tport_t *tp;

  he->he_stats->st_requests++;

  tp = tport_tsend(hc->hc_tport, hc->hc_request, hc->hc_tpn,
		   TAG_NEXT(hc->hc_tags));

  if (tp == NULL) {
    he->he_stats->st_tp_errors++;
    hc_reply(hc, HTTP_503_NO_SERVICE);
    return hc;
  }

  hc->hc_tport = tport_incref(tp);

  hc->hc_pending = tport_pend(tp, hc->hc_request, hc_tport_error, hc);
  if (hc->hc_pending == -1)
    hc->hc_pending = 0;

  if (hc->hc_expires) {
    hc->hc_timeout = he_now(he) + hc->hc_expires;	/* XXX */
    if (hc->hc_timeout == 0)
      hc->hc_timeout++;
  }

  return hc;
}

/** @internal Report transport errors. */
void hc_tport_error(nth_engine_t * he, nth_client_t * hc,
		    tport_t * tp, msg_t *msg, int error)
{
  su_sockaddr_t const *su = msg_addr(msg);
  tp_name_t const *tpn = tp ? tport_name(tp) : hc->hc_tpn;
  char addr[SU_ADDRSIZE];
  char const *errmsg;

  if (error)
    errmsg = su_strerror(error);
  else
    errmsg = "Remote end closed connection";
  su_log("nth: %s: %s (%u) with %s@%s:%u\n",
	 hc->hc_method_name,
	 errmsg, error,
	 tpn->tpn_proto,
	 su_inet_ntop(su->su_family, SU_ADDR(su), addr, sizeof(addr)),
	 htons(su->su_port));

  he->he_stats->st_tp_errors++;
  hc_reply(hc, HTTP_503_NO_SERVICE);
}

static
void hc_delayed_recv(su_root_magic_t *rm, su_msg_r msg, union sm_arg_u *u);

/** Respond internally to a transaction. */
int hc_reply(nth_client_t * hc, int status, char const *phrase)
{
  nth_engine_t *he = hc->hc_engine;
  msg_t *msg = NULL;
  http_t *http = NULL;

  assert(status >= 400);

  SU_DEBUG_5(("nth: hc_reply(%p, %u, %s)\n", (void *)hc, status, phrase));

  if (hc->hc_pending) {
    tport_release(hc->hc_tport, hc->hc_pending, hc->hc_request, NULL, hc,
		  status < 200);
    if (status >= 200)
      hc->hc_pending = 0;
  }

  tport_shutdown(hc->hc_tport, 2);

  hc->hc_completed = 1;
  hc->hc_timeout = 0;

  if (hc->hc_callback == hc_default_cb) {
    hc_free(hc);
    return 0;
  }

  /* Create response message, if needed */
  if (hc->hc_error_msg) {
    msg = he_msg_create(he, NTH_INTERNAL_MSG, NULL, 0, NULL, hc);
    http = http_object(msg);
    http_complete_response(msg, status, phrase, http_object(hc->hc_request));
  } else
    hc->hc_status = status;

  if (hc->hc_inserted) {
    hc_recv(hc, msg, http);
    return 0;
  } else {
    /*
     * The thread creating outgoing transaction must return to application
     * before transaction callback can be invoked. Processing an internally
     * generated response message must be delayed until transaction creation
     * is completed.
     *
     * The internally generated message is transmitted using su_msg_send()
     * and it is delivered back to NTA when the application next time
     * executes the su_root_t event loop.
     */
    su_root_t *root = he->he_root;
    su_msg_r su_msg = SU_MSG_R_INIT;

    if (su_msg_create(su_msg,
		      su_root_task(root),
		      su_root_task(root),
		      hc_delayed_recv,
		      sizeof(struct hc_recv_s)) == SU_SUCCESS) {
      struct hc_recv_s *a = su_msg_data(su_msg)->hc_recv;

      a->hc = hc;
      a->msg = msg;
      a->http = http;

      if (su_msg_send(su_msg) == SU_SUCCESS)
	return 0;
    }
  }

  if (msg)
    msg_destroy(msg);

  return -1;
}

static
void hc_delayed_recv(su_root_magic_t *rm, su_msg_r msg, union sm_arg_u *u)
{
  struct hc_recv_s *a = u->hc_recv;

  if (hc_recv(a->hc, a->msg, a->http) < 0 && a->msg)
    msg_destroy(a->msg);
}

/** Receive response to transaction. */
int hc_recv(nth_client_t * hc, msg_t *msg, http_t * http)
{
  short status;
  int streaming = msg_is_streaming(msg);
  int shutdown = 0;

  if (http && http->http_status) {
    status = http->http_status->st_status;
    if (status < 100)
      status = 100;

    if (streaming && !hc->hc_streaming) {
      /* Disable streaming for this msg */
      msg_set_streaming(msg, (enum msg_streaming_status)0);

      return 0;			/* Wait for complete message */
    }

    hc->hc_status = status;
  } else if (http)
    status = hc->hc_status = 500, streaming = 0, http = NULL;
  else
    status = hc->hc_status, streaming = 0;

  if (status == 400 || (http && (http->http_flags & MSG_FLG_ERROR)))
    shutdown = 2;

  if (!streaming || shutdown)
    msg_set_streaming(msg, (enum msg_streaming_status)0);

  if (hc->hc_pending) {
    tport_release(hc->hc_tport, hc->hc_pending, hc->hc_request, msg, hc,
		  streaming || status < 200);
    if (!streaming && status >= 200)
      hc->hc_pending = 0;
  }

  if (!streaming && status >= 200) {
    /* Completed. */
    hc->hc_completed = 1;
    hc_remove(hc->hc_engine, hc);

    if (shutdown ||
	!http ||
	(http->http_status->st_version == http_version_1_1 &&
	 http->http_connection &&
	 msg_params_find(http->http_connection->k_items, "close")) ||
	(http->http_status->st_version == http_version_1_0))
      shutdown = 2;
  }

  if (shutdown) {
    if (status < 200)
      status = 400;
    tport_shutdown(hc->hc_tport, shutdown);
  }

  if (msg_is_complete(msg)) {
    if (status < 200)
      hc->hc_engine->he_stats->st_1xxresponses++;
    else
      hc->hc_engine->he_stats->st_responses++;
  }

  if (hc->hc_response)
    msg_destroy(hc->hc_response);
  hc->hc_response = msg;
  hc->hc_is_streaming = streaming;

  /* Call callback */
  hc->hc_callback(hc->hc_magic, hc, http);

  return 0;
}

/** @internal Default callback for request */
int hc_default_cb(nth_client_magic_t * magic,
		  nth_client_t * hc, http_t const *http)
{
  if (http == NULL || http->http_status->st_status >= 200)
    hc_free(hc);
  return 0;
}

/** @internal Client transaction timer routine. */
static
void hc_timer(nth_engine_t * he, nth_client_t * hc, uint32_t now)
{
  if (hc->hc_timeout == 0)
    return;

  if ((int)hc->hc_timeout - (int)now > 0)
    return;

  hc_reply(hc, HTTP_408_TIMEOUT);
}
