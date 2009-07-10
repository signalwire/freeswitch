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

/**@internal @file nth_server.c
 * @brief HTTP server.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Sat Oct 19 01:37:36 2002 ppessi
 */

#include "config.h"

#include <sofia-sip/su_string.h>
#include <sofia-sip/su.h>

typedef struct server_s server_t;

/** @internal SU timer argument pointer type */
#define SU_TIMER_ARG_T server_t

#include <sofia-sip/http_header.h>
#include <sofia-sip/http_status.h>
#include <sofia-sip/http_tag.h>

#include "sofia-sip/nth.h"

#include <sofia-sip/msg_date.h>
#include <sofia-sip/msg_addr.h>
#include <sofia-sip/su_tagarg.h>

#include <sofia-sip/hostdomain.h>

/* We are customer of tport_t */
#define TP_STACK_T   server_t
#define TP_MAGIC_T   void

#include <sofia-sip/tport.h>
#include <sofia-sip/htable.h>

#include <sofia-sip/auth_module.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>

#ifndef UINT32_MAX
#define UINT32_MAX (0xffffffffU)
#endif

enum { SERVER_TICK = 1000 };

#define SERVER_VERSION "nth/" NTH_VERSION

HTABLE_DECLARE(hc_htable, hct, nth_client_t);

struct server_s
{
  su_home_t          srv_home[1];
  su_root_t         *srv_root;

  su_timer_t        *srv_timer;
  unsigned           srv_now;

  msg_mclass_t const*srv_mclass;
  int                srv_mflags;	/**< Message flags */

  tport_t           *srv_tports;
  unsigned           srv_queuesize;	/**< Maximum number of queued responses */

  size_t             srv_max_bodylen;	/**< Maximum accepted length */

  unsigned           srv_persistent:1;	/**< Allow persistent connections */

  /** Sites */
  nth_site_t        *srv_sites;

  /* Statistics */
  struct {
    uint32_t           st_requests;     /**< Received requests */
    uint32_t           st_responses;    /**< Sent responses */
    uint32_t           st_bad;		/**< Bad requests */
  }                  srv_stats[1];

  http_server_t     *srv_server;      /**< Server header */
};

struct nth_site_s
{
  nth_site_t          *site_next, **site_prev;

  nth_site_t          *site_kids;

  server_t            *site_server;
  auth_mod_t          *site_auth;

  url_t               *site_url;
  char const          *site_path;
  size_t               site_path_len;

  nth_request_f       *site_callback;
  nth_site_magic_t    *site_magic;

  su_time_t            site_access; /**< Last request served */

  /** Host header must match with server name */
  unsigned             site_strict : 1;
  /** Site can have kids */
  unsigned             site_isdir : 1;
  /** Site does not have domain name */
  unsigned             site_wildcard : 1;
};

struct nth_request_s
{
  server_t              *req_server;

  http_method_t        	req_method;
  char const           *req_method_name;
  url_t const          *req_url;         /**< RequestURI  */
  char const           *req_version;

  tport_t              *req_tport;
  msg_t		       *req_request;
  msg_t                *req_response;

  auth_status_t        *req_as;

  unsigned short      	req_status;
  unsigned              req_close : 1; /**< Client asked for close */
  unsigned              req_in_callback : 1;
  unsigned              req_destroyed : 1;
};

/* ====================================================================== */
/* Debug log settings */

#define SU_LOG   nth_server_log

#ifdef SU_DEBUG_H
#error <su_debug.h> included directly.
#endif
#include <sofia-sip/su_debug.h>

/**Environment variable determining the debug log level for @b nth
 * module.
 *
 * The NTH_DEBUG environment variable is used to determine the debug
 * logging level for @b nth module. The default level is 1.
 *
 * @sa <sofia-sip/su_debug.h>, nth_server_log, SOFIA_DEBUG
 */
extern char const NTH_DEBUG[];

#ifndef SU_DEBUG
#define SU_DEBUG 1
#endif

/**Debug log for @b nth module.
 *
 * The nth_server_log is the log object used by @b nth module. The level of
 * #nth_server_log is set using #NTH_DEBUG environment variable.
 */
su_log_t nth_server_log[] = { SU_LOG_INIT("nth", "NTH_DEBUG", SU_DEBUG) };

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
static char const __func__[] = "nth";
#endif

/* ====================================================================== */
/** Server side
 */
static server_t *server_create(url_t const *url,
			       tag_type_t tag, tag_value_t value, ...);
void server_destroy(server_t *srv);
su_inline int server_timer_init(server_t *srv);
static void server_timer(su_root_magic_t *rm, su_timer_t *timer, server_t *srv);
su_inline uint32_t server_now(server_t const *srv);
static void server_request(server_t *srv, tport_t *tport, msg_t *msg,
				    void *arg, su_time_t now);
static nth_site_t **site_get_host(nth_site_t **, char const *host, char const *port);
static nth_site_t **site_get_rslot(nth_site_t *parent, char *path,
				   char **return_rest);
static nth_site_t *site_get_subdir(nth_site_t *parent, char const *path, char const **res);
static void server_tport_error(server_t *srv, tport_t *tport,
			       int errcode, char const *remote);
static msg_t *server_msg_create(server_t *srv, int flags,
				char const data[], usize_t dlen,
				tport_t const *tp, tp_client_t *tpc);

static void server_reply(server_t *srv, tport_t *tport,
			 msg_t *request, msg_t *response,
			 int status, char const *phrase);

static
void nth_site_request(server_t *srv,
		      nth_site_t *site,
		      tport_t *tport,
		      msg_t *request,
		      http_t *http,
		      char const *path,
		      msg_t *response);

/* ----------------------------------------------------------------------
 * 5) Site functions
 */

/** Create a http site object.
 *
 * The function nth_site_create() allocates and initializes a web site
 * object. A web site object can be either
 * - a primary http server (@a parent is NULL),
 * - a virtual http server (@a address contains hostpart), or
 * - a site within a server
 *   (@a address does not have hostpart, only path part).
 *
 * @param parent pointer to parent site
 *               (NULL when creating a primary server object)
 * @param callback pointer to callback function called when
 *                 a request is received
 * @param magic    application context included in callback parameters
 * @param address  absolute or relative URI specifying the address of
 *                 site
 * @param tag, value, ... list of tagged parameters
 *
 * @TAGS
 * If the @a parent is NULL, the list of tagged parameters must contain
 * NTHTAG_ROOT() used to create the server engine. Tags supported when @a
 * parent is NULL are NTHTAG_ROOT(), NTHTAG_MCLASS(), TPTAG_REUSE(),
 * HTTPTAG_SERVER(), and HTTPTAG_SERVER_STR(). All the tags are passed to
 * tport_tcreate() and tport_tbind(), too.
 *
 * @since Support for multiple sites was added to @VERSION_1_12_4
 */
nth_site_t *nth_site_create(nth_site_t *parent,
			    nth_request_f *callback,
			    nth_site_magic_t *magic,
			    url_string_t const *address,
			    tag_type_t tag, tag_value_t value,
			    ...)
{
  nth_site_t *site = NULL, **prev = NULL;
  su_home_t home[SU_HOME_AUTO_SIZE(256)];
  url_t *url, url0[1];
  server_t *srv = NULL;
  ta_list ta;
  char *path = NULL;
  size_t usize;
  int is_host, is_path, wildcard = 0;

  su_home_auto(home, sizeof home);

  if (parent && url_is_string(address)) {
    char const *s = (char const *)address;
    size_t sep = strcspn(s, "/:");

    if (parent->site_path) {
      /* subpath */
      url_init(url0, (enum url_type_e)parent->site_url->url_type);
      url0->url_path = s;
      address = (url_string_t*)url0;
    }
    else if (s[sep] == ':')
      /* absolute URL with scheme */;
    else if (s[sep] == '\0' && strchr(s, '.') && host_is_valid(s)) {
      /* looks like a domain name */;
      url_init(url0, (enum url_type_e)parent->site_url->url_type);
      url0->url_host = s;
      address = (url_string_t*)url0;
    }
    else {
      /* looks like a path */
      url_init(url0, (enum url_type_e)parent->site_url->url_type);
      url0->url_path = s;
      address = (url_string_t*)url0;
    }
  }

  url = url_hdup(home, address->us_url);

  if (!url || !callback)
    return NULL;

  is_host = url->url_host != NULL;
  is_path = url->url_path != NULL;

  if (is_host && is_path) {
    SU_DEBUG_3(("nth_site_create(): virtual host and path simultanously\n"));
    errno = EINVAL;
    goto error;
  }

  if (!parent && !is_host) {
    SU_DEBUG_3(("nth_site_create(): host is required\n"));
    errno = EINVAL;
    goto error;
  }

  if (parent) {
    if (!parent->site_isdir) {
      SU_DEBUG_3(("nth_site_create(): invalid parent resource \n"));
      errno = EINVAL;
      goto error;
    }

    srv = parent->site_server; assert(srv);
    if (is_host) {
      prev = site_get_host(&srv->srv_sites, url->url_host, url->url_port);

      if (prev == NULL) {
	SU_DEBUG_3(("nth_site_create(): host %s:%s already exists\n",
		    url->url_host, url->url_port ? url->url_port : ""));
	errno = EEXIST;
	goto error;
      }
    }
    else {
      size_t i, j;

      path = (char *)url->url_path;
      while (path[0] == '/')
	path++;

      /* Remove duplicate // */
      for (i = j = 0; path[i];) {
	while (path[i] == '/' && path[i + 1] == '/')
	  i++;
	path[j++] = path[i++];
      }
      path[j] = path[i];

      url = url0, *url = *parent->site_url;

      if (url->url_path) {
	url->url_path = su_strcat(home, url->url_path, path);
	if (!url->url_path)
	  goto error;
	path = (char *)url->url_path + strlen(parent->site_url->url_path);
      }
      else
	url->url_path = path;

      prev = site_get_rslot(parent, path, &path);

      if (!prev || path[0] == '\0') {
	SU_DEBUG_3(("nth_site_create(): directory \"%s\" already exists\n",
		    url->url_path));
	errno = EEXIST;
	goto error;
      }
    }
  }

  if (!parent) {
    if (strcmp(url->url_host, "*") == 0 ||
	host_cmp(url->url_host, "0.0.0.0") == 0 ||
	host_cmp(url->url_host, "::") == 0)
      wildcard = 1, url->url_host = "*";
  }

  usize = sizeof(*url) + url_xtra(url);

  ta_start(ta, tag, value);

  if (!parent) {
    srv = server_create(url, ta_tags(ta));
    prev = &srv->srv_sites;
  }

  if (srv && (site = su_zalloc(srv->srv_home, (sizeof *site) + usize))) {
    site->site_url = (url_t *)(site + 1);
    url_dup((void *)(site->site_url + 1), usize - sizeof(*url),
	    site->site_url, url);

    assert(prev);
    if ((site->site_next = *prev))
      site->site_next->site_prev = &site->site_next;
    *prev = site, site->site_prev = prev;
    site->site_server = srv;

    if (path) {
      size_t path_len;

      site->site_path = site->site_url->url_path + (path - url->url_path);
      path_len = strlen(site->site_path); assert(path_len > 0);
      if (path_len > 0 && site->site_path[path_len - 1] == '/')
	path_len--, site->site_isdir = 1;
      site->site_path_len = path_len;
    }
    else {
      site->site_isdir = is_host;
      site->site_path = "";
      site->site_path_len = 0;
    }

    site->site_wildcard = wildcard;
    site->site_callback = callback;
    site->site_magic = magic;

    if (parent)
      site->site_auth = parent->site_auth;

    nth_site_set_params(site, ta_tags(ta));
  }

  ta_end(ta);

 error:
  su_home_deinit(home);
  return site;
}

void nth_site_destroy(nth_site_t *site)
{
  if (site == NULL)
    return;

  if (site->site_auth)
    auth_mod_unref(site->site_auth), site->site_auth = NULL;

  if (site->site_server->srv_sites == site) {
    server_destroy(site->site_server);
  }
}


nth_site_magic_t *nth_site_magic(nth_site_t const *site)
{
  return site ? site->site_magic : NULL;
}


void nth_site_bind(nth_site_t *site,
		   nth_request_f *callback,
		   nth_site_magic_t *magic)
{
  if (site) {
    site->site_callback = callback;
    site->site_magic = magic;
  }
}


/** Get the site URL. @NEW_1_12_4. */
url_t const *nth_site_url(nth_site_t const *site)
{
  return site ? site->site_url : NULL;
}

/** Return server name and version */
char const *nth_site_server_version(void)
{
  return "nth/" NTH_VERSION;
}

/** Get the time last time served. @NEW_1_12_4. */
su_time_t nth_site_access_time(nth_site_t const *site)
{
  su_time_t const never = { 0, 0 };

  return site ? site->site_access : never;
}

int nth_site_set_params(nth_site_t *site,
			tag_type_t tag, tag_value_t value, ...)
{
  int n;
  ta_list ta;

  server_t *server;
  int master;
  msg_mclass_t const *mclass;
  int mflags;
  auth_mod_t *am;

  if (site == NULL)
    return (errno = EINVAL), -1;

  server = site->site_server;
  master = site == server->srv_sites;
  am = site->site_auth;

  mclass = server->srv_mclass;
  mflags = server->srv_mflags;

  ta_start(ta, tag, value);

  n = tl_gets(ta_args(ta),
	      TAG_IF(master, NTHTAG_MCLASS_REF(mclass)),
	      TAG_IF(master, NTHTAG_MFLAGS_REF(mflags)),
	      NTHTAG_AUTH_MODULE_REF(am),
	      TAG_END());

  if (n > 0) {
    if (mclass)
      server->srv_mclass = mclass;
    else
      server->srv_mclass = http_default_mclass();
    server->srv_mflags = mflags;
    auth_mod_ref(am), auth_mod_unref(site->site_auth), site->site_auth = am;
  }

  ta_end(ta);

  return n;
}

int nth_site_get_params(nth_site_t const *site,
			tag_type_t tag, tag_value_t value, ...)
{
  int n;
  ta_list ta;
  server_t *server;
  int master;
  msg_mclass_t const *mclass;

  if (site == NULL)
    return (errno = EINVAL), -1;

  server = site->site_server;
  master = site == server->srv_sites;

  if (master && server->srv_mclass != http_default_mclass())
    mclass = server->srv_mclass;
  else
    mclass = NULL;

  ta_start(ta, tag, value);

  n = tl_tgets(ta_args(ta),
	       TAG_IF(master, NTHTAG_MCLASS(mclass)),
	       TAG_IF(master, NTHTAG_MFLAGS(server->srv_mflags)),
	       TAG_END());

  ta_end(ta);

  return n;
}

int nth_site_get_stats(nth_site_t const *site,
		       tag_type_t tag, tag_value_t value, ...)
{
  int n;
  ta_list ta;

  if (site == NULL)
    return (errno = EINVAL), -1;

  ta_start(ta, tag, value);

  n = tl_tgets(ta_args(ta),
	       TAG_END());

  ta_end(ta);

  return n;
}

static
nth_site_t **site_get_host(nth_site_t **list, char const *host, char const *port)
{
  nth_site_t *site;

  assert(host);

  for (; (site = *list); list = &site->site_next) {
    if (host_cmp(host, site->site_url->url_host) == 0 &&
	su_strcmp(port, site->site_url->url_port) == 0) {
      break;
    }
  }

  return list;
}

/** Find a place to insert site from the hierarchy.
 *
 * A resource can be either a 'dir' (name ends with '/') or 'file'.
 * When a resource
 */
static
nth_site_t **site_get_rslot(nth_site_t *parent, char *path,
			    char **return_rest)
{
  nth_site_t *site, **prev;
  size_t len;
  int cmp;

  assert(path);

  if (path[0] == '\0')
    return errno = EEXIST, NULL;

  for (prev = &parent->site_kids; (site = *prev); prev = &site->site_next) {
    cmp = strncmp(path, site->site_path, len = site->site_path_len);
    if (cmp > 0)
      break;
    if (cmp < 0)
      continue;
    if (path[len] == '\0') {
      if (site->site_isdir)
	return *return_rest = path, prev;
      return errno = EEXIST, NULL;
    }
    if (path[len] != '/' || site->site_path[len] != '/')
      continue;

    while (path[++len] == '/')
      ;

    return site_get_rslot(site, path + len, return_rest);
  }

  *return_rest = path;

  return prev;
}

static char const site_nodir_match[] = "";

/** Find a subdir from site hierarchy */
static
nth_site_t *site_get_subdir(nth_site_t *parent,
			    char const *path,
			    char const **return_rest)
{
  nth_site_t *site;
  size_t len;
  int cmp;

  assert(path);

  while (path[0] == '/')	/* Skip multiple slashes */
    path++;

  if (path[0] == '\0')
    return *return_rest = path, parent;

  for (site = parent->site_kids; site; site = site->site_next) {
    cmp = strncmp(path, site->site_path, len = site->site_path_len);
    if (cmp > 0)
      break;
    if (cmp < 0)
      continue;
    if (path[len] == '\0')
      return *return_rest = site_nodir_match, site;
    if (site->site_path[len] == '/' && path[len] == '/')
      return site_get_subdir(site, path + len + 1, return_rest);
  }

  return *return_rest = path, parent;
}


/* ----------------------------------------------------------------------
 * Server functions
 */

static char const * const http_tports[] =
  {
    "tcp", "tls", NULL
  };

static char const * const http_no_tls_tports[] = { "tcp", NULL };

static tp_stack_class_t nth_server_class[1] =
  {{
    sizeof(nth_server_class),
    server_request,
    server_tport_error,
    server_msg_create
  }};

server_t *server_create(url_t const *url,
			tag_type_t tag, tag_value_t value, ...)
{
  server_t *srv;
  msg_mclass_t const *mclass = NULL;
  tp_name_t tpn[1] = {{ NULL }};
  su_root_t *root = NULL;
  http_server_t const *server = NULL;
  int persistent = 0;
  char const *server_str = SERVER_VERSION;
  ta_list ta;

  ta_start(ta, tag, value);
  tl_gets(ta_args(ta),
	  NTHTAG_ROOT_REF(root),
	  NTHTAG_MCLASS_REF(mclass),
	  TPTAG_REUSE_REF(persistent),
	  HTTPTAG_SERVER_REF(server),
	  HTTPTAG_SERVER_STR_REF(server_str),
	  TAG_END());

  if (!root || !url ||
      (url->url_type != url_http && url->url_type != url_https)
      || !(srv = su_home_new(sizeof(*srv)))) {
    ta_end(ta);
    return NULL;
  }

  tpn->tpn_proto = url_tport_default((enum url_type_e)url->url_type);
  tpn->tpn_canon = url->url_host;
  tpn->tpn_host =  url->url_host;
  tpn->tpn_port = url_port(url);

  srv->srv_tports = tport_tcreate(srv, nth_server_class, root,
				  TPTAG_IDLE(600000),
				  TPTAG_TIMEOUT(300000),
				  ta_tags(ta));

  srv->srv_persistent = persistent;
  srv->srv_max_bodylen = 1 << 30; /* 1 GB */

  if (tport_tbind(srv->srv_tports, tpn, http_tports,
		  TAG_END()) >= 0 ||
      tport_tbind(srv->srv_tports, tpn, http_no_tls_tports,
		  TAG_END()) >= 0) {
    srv->srv_root = root;
    srv->srv_mclass = mclass ? mclass : http_default_mclass();
    srv->srv_mflags = MSG_DO_CANONIC;

    if (server)
      srv->srv_server = http_server_dup(srv->srv_home, server);
    else
      srv->srv_server = http_server_make(srv->srv_home, server_str);

    tport_get_params(srv->srv_tports,
		     TPTAG_QUEUESIZE_REF(srv->srv_queuesize),
		     TAG_END());
  }
  else {
    SU_DEBUG_1(("nth_server_create: cannot bind transports "
		URL_FORMAT_STRING "\n",
		URL_PRINT_ARGS(url)));
    server_destroy(srv), srv = NULL;
  }

  ta_end(ta);

  return srv;
}

void server_destroy(server_t *srv)
{
  tport_destroy(srv->srv_tports);
  su_timer_destroy(srv->srv_timer);
  su_home_unref(srv->srv_home);
}

/** Initialize server timer. */
su_inline
int server_timer_init(server_t *srv)
{
  if (0) {
    srv->srv_timer = su_timer_create(su_root_task(srv->srv_root), SERVER_TICK);
    return su_timer_set(srv->srv_timer, server_timer, srv);
  }
  return 0;
}

/**
 * Server timer routine.
 */
static
void server_timer(su_root_magic_t *rm, su_timer_t *timer, server_t *srv)
{
  uint32_t now;

  su_timer_set(timer, server_timer, srv);

  now = su_time_ms(su_now()); now += now == 0; srv->srv_now = now;

  /* Xyzzy */

  srv->srv_now = 0;
}

/** Get current timestamp in milliseconds */
su_inline
uint32_t server_now(server_t const *srv)
{
  if (srv->srv_now)
    return srv->srv_now;
  else
    return su_time_ms(su_now());
}

/** Process incoming request message */
static
void server_request(server_t *srv,
		    tport_t *tport,
		    msg_t *request,
		    void *arg,
		    su_time_t now)
{
  nth_site_t *site = NULL, *subsite = NULL;
  msg_t *response;
  http_t *http = http_object(request);
  http_host_t *h;
  char const *host, *port, *path, *subpath = NULL;

  /* Disable streaming */
  if (msg_is_streaming(request)) {
    msg_set_streaming(request, (enum msg_streaming_status)0);
    return;
  }

  /* Create a response message */
  response = server_msg_create(srv, 0, NULL, 0, NULL, NULL);
  tport_tqueue(tport, response, TAG_END());

  if (http && http->http_flags & MSG_FLG_TIMEOUT) {
    server_reply(srv, tport, request, response, 400, "Request timeout");
    return;
  } else if (http && http->http_flags & MSG_FLG_TOOLARGE) {
    server_reply(srv, tport, request, response, HTTP_413_ENTITY_TOO_LARGE);
    return;
  } else if (!http || !http->http_request ||
	     (http->http_flags & MSG_FLG_ERROR)) {
    server_reply(srv, tport, request, response, HTTP_400_BAD_REQUEST);
    return;
  }

  if (http->http_request->rq_version != http_version_1_0 &&
      http->http_request->rq_version != http_version_1_1) {
    server_reply(srv, tport, request, response, HTTP_505_HTTP_VERSION);
    return;
  }

  h = http->http_host;

  if (h) {
    host = h->h_host, port = h->h_port;
  }
  else if (http->http_request->rq_url->url_host) {
    host = http->http_request->rq_url->url_host;
    port = http->http_request->rq_url->url_port;
  }
  else
    host = NULL, port = NULL;

  path = http->http_request->rq_url->url_path;

  if (host)
    site = *site_get_host(&srv->srv_sites, host, port);

  if (site == NULL && !srv->srv_sites->site_strict)
    site = srv->srv_sites;

  if (path == NULL)
    path = "";

  if (path[0])
    subsite = site_get_subdir(site, path, &subpath);

  if (subsite)
    subsite->site_access = now;
  else
    site->site_access = now;

  if (subsite && subsite->site_isdir && subpath == site_nodir_match) {
    /* Answer with 301 */
    http_location_t loc[1];
    http_location_init(loc);

    *loc->loc_url = *site->site_url;

    if (site->site_wildcard) {
      if (http->http_host) {
	loc->loc_url->url_host = http->http_host->h_host;
	loc->loc_url->url_port = http->http_host->h_port;
      }
      else {
	tp_name_t const *tpn = tport_name(tport); assert(tpn);
	loc->loc_url->url_host = tpn->tpn_canon;
	if (strcmp(url_port_default((enum url_type_e)loc->loc_url->url_type), tpn->tpn_port))
	  loc->loc_url->url_port = tpn->tpn_port;
      }
    }

    loc->loc_url->url_root = 1;
    loc->loc_url->url_path = subsite->site_url->url_path;

    msg_header_add_dup(response, NULL, (msg_header_t *)loc);

    server_reply(srv, tport, request, response, HTTP_301_MOVED_PERMANENTLY);
  }
  else if (subsite)
    nth_site_request(srv, subsite, tport, request, http, subpath, response);
  else if (site)
    nth_site_request(srv, site, tport, request, http, path, response);
  else
    /* Answer with 404 */
    server_reply(srv, tport, request, response, HTTP_404_NOT_FOUND);
}

static void server_tport_error(server_t *srv,
				   tport_t *tport,
				   int errcode,
				   char const *remote)
{
  su_log("\nth: tport: %s%s%s\n",
	 remote ? remote : "", remote ? ": " : "",
	 su_strerror(errcode));
}

/** Respond without creating a request structure */
static void server_reply(server_t *srv, tport_t *tport,
			 msg_t *request, msg_t *response,
			 int status, char const *phrase)
{
  http_t *http;
  http_payload_t *pl;
  int close;
  http_status_t st[1];
  char const *req_version = NULL;

  if (status < 200 || status >= 600)
    status = 500, phrase = http_500_internal_server;

  http = http_object(request);

  if (http && http->http_request)
    req_version = http->http_request->rq_version;

  close = status >= 200 &&
    (!srv->srv_persistent
     || status == 400
     || (http && http->http_request &&
	 http->http_request->rq_version != http_version_1_1)
     || (http && http->http_connection &&
	 msg_params_find(http->http_connection->k_items, "close")));

  msg_destroy(request);

  http = http_object(response);

  pl = http_payload_format(msg_home(response),
			   "<html>\n"
			   "<head><title>%u %s</title></head>\n"
			   "<body><h2>%u %s</h2></body>\n"
			   "</html>\n", status, phrase, status, phrase);

  msg_header_insert(response, (msg_pub_t *)http, (msg_header_t *)pl);

  if (req_version != http_version_0_9) {
    http_status_init(st);
    st->st_version = http_version_1_1;
    st->st_status = status;
    st->st_phrase = phrase;

    http_add_tl(response, http,
		HTTPTAG_STATUS(st),
		HTTPTAG_SERVER(srv->srv_server),
		HTTPTAG_CONTENT_TYPE_STR("text/html"),
		HTTPTAG_SEPARATOR_STR("\r\n"),
		TAG_IF(close, HTTPTAG_CONNECTION_STR("close")),
		TAG_END());

    msg_serialize(response, (msg_pub_t *)http);
  } else {
    /* Just send the response */
    *msg_chain_head(response) = (msg_header_t *)pl;
    close = 1;
  }

  if (tport_tqsend(tport, response, NULL,
		   TPTAG_CLOSE_AFTER(close),
		   TAG_END()) == -1) {
    SU_DEBUG_3(("server_reply(): cannot queue response\n"));
    tport_shutdown(tport, 2);
  }

  msg_destroy(response);
}

/** Create a new message for transport */
static
msg_t *server_msg_create(server_t *srv, int flags,
			 char const data[], usize_t dlen,
			 tport_t const *tp, tp_client_t *tpc)
{
  msg_t *msg = msg_create(srv->srv_mclass, srv->srv_mflags | flags);

  return msg;
}

/* ----------------------------------------------------------------------
 * 6) Server transactions
 */

struct auth_info
{
  nth_site_t *site;
  nth_request_t *req;
  http_t const *http;
  char const *path;
};

static void nth_authentication_result(void *ai0, auth_status_t *as);

static
void nth_site_request(server_t *srv,
		      nth_site_t *site,
		      tport_t *tport,
		      msg_t *request,
		      http_t *http,
		      char const *path,
		      msg_t *response)
{
  auth_mod_t *am = site->site_auth;
  nth_request_t *req;
  auth_status_t *as;
  struct auth_info *ai;
  size_t size = (am ? (sizeof *as) + (sizeof *ai) : 0) + (sizeof *req);
  int status;

  req = su_zalloc(srv->srv_home, size);

  if (req == NULL) {
    server_reply(srv, tport, request, response, HTTP_500_INTERNAL_SERVER);
    return;
  }

  if (am)
    as = auth_status_init(req + 1, sizeof *as), ai = (void *)(as + 1);
  else
    as = NULL, ai = NULL;

  req->req_server = srv;
  req->req_method = http->http_request->rq_method;
  req->req_method_name = http->http_request->rq_method_name;
  req->req_url = http->http_request->rq_url;
  req->req_version = http->http_request->rq_version;

  req->req_tport = tport_incref(tport);
  req->req_request = request;
  req->req_response = response;

  req->req_status = 100;
  req->req_close =
    !srv->srv_persistent
    || http->http_request->rq_version != http_version_1_1
    || (http->http_connection &&
	msg_params_find(http->http_connection->k_items, "close"));

  if (am) {
    static auth_challenger_t const http_server_challenger[] =
      {{ HTTP_401_UNAUTHORIZED, http_www_authenticate_class }};

    req->req_as = as;

    as->as_method = http->http_request->rq_method_name;
    as->as_uri = path;

    if (http->http_payload) {
      as->as_body = http->http_payload->pl_data;
      as->as_bodylen = http->http_payload->pl_len;
    }

    auth_mod_check_client(am, as,
			  http->http_authorization,
			  http_server_challenger);

    if (as->as_status == 100) {
      /* Stall transport - do not read more requests */
      if (tport_queuelen(tport) * 2 >= srv->srv_queuesize)
	tport_stall(tport);

      as->as_callback = nth_authentication_result;
      as->as_magic = ai;
      ai->site = site;
      ai->req = req;
      ai->http = http;
      ai->path = path;
      return;
    }
    else if (as->as_status) {
      assert(as->as_status >= 200);
      nth_request_treply(req, as->as_status, as->as_phrase,
			 HTTPTAG_HEADER((http_header_t *)as->as_response),
			 HTTPTAG_HEADER((http_header_t *)as->as_info),
			 TAG_END());
      nth_request_destroy(req);
      return;
    }
  }

  req->req_in_callback = 1;
  status = site->site_callback(site->site_magic, site, req, http, path);
  req->req_in_callback = 0;

  if (status != 0 && (status < 100 || status >= 600))
    status = 500;

  if (status != 0 && req->req_status < 200) {
    nth_request_treply(req, status, NULL, TAG_END());
  }

  if (req->req_status < 100) {
    /* Stall transport - do not read more requests */
    if (tport_queuelen(tport) * 2 >= srv->srv_queuesize)
      tport_stall(tport);
  }

  if (status >= 200 || req->req_destroyed)
    nth_request_destroy(req);
}

static void nth_authentication_result(void *ai0, auth_status_t *as)
{
  struct auth_info *ai = ai0;
  nth_request_t *req = ai->req;
  int status;

  if (as->as_status != 0) {
    assert(as->as_status >= 300);
    nth_request_treply(req, status = as->as_status, as->as_phrase,
		       HTTPTAG_HEADER((http_header_t *)as->as_response),
		       TAG_END());
  }
  else {
    req->req_in_callback = 1;
    status = ai->site->site_callback(ai->site->site_magic,
				     ai->site,
				     ai->req,
				     ai->http,
				     ai->path);
    req->req_in_callback = 0;

    if (status != 0 && (status < 100 || status >= 600))
      status = 500;

    if (status != 0 && req->req_status < 200) {
      nth_request_treply(req, status, NULL, TAG_END());
    }
  }

  if (status >= 200 || req->req_destroyed)
    nth_request_destroy(req);
}

void nth_request_destroy(nth_request_t *req)
{
  if (req == NULL)
    return;

  if (req->req_status < 200)
    nth_request_treply(req, HTTP_500_INTERNAL_SERVER, TAG_END());

  req->req_destroyed = 1;

  if (req->req_in_callback)
    return;

  if (req->req_as)
    su_home_deinit(req->req_as->as_home);

  tport_decref(&req->req_tport), req->req_tport = NULL;
  msg_destroy(req->req_request), req->req_request = NULL;
  msg_destroy(req->req_response), req->req_response = NULL;
  su_free(req->req_server->srv_home, req);
}

/** Return request authentication status.
 *
 * @param req pointer to HTTP request object
 *
 * @retval Status code
 *
 * @since New in @VERSION_1_12_4
 */
int nth_request_status(nth_request_t const *req)
{
  return req ? req->req_status : 400;
}

/** Return request authentication status.
 *
 * @param req pointer to HTTP request object
 *
 * @retval Pointer to authentication status struct
 *
 * @note The authentication status struct is freed when the #nth_request_t
 * object is destroyed.
 *
 * @since New in @VERSION_1_12_4
 *
 * @sa AUTH
 */
auth_status_t *nth_request_auth(nth_request_t const *req)
{
  return req ? req->req_as : NULL;
}

http_method_t nth_request_method(nth_request_t const *req)
{
  return req ? req->req_method : http_method_invalid;
}

msg_t *nth_request_message(nth_request_t *req)
{
  msg_t *retval = NULL;

  if (req)
    retval = msg_ref_create(req->req_request);

  return retval;
}

int nth_request_treply(nth_request_t *req,
		       int status, char const *phrase,
		       tag_type_t tag, tag_value_t value, ...)
{
  msg_t *response, *next = NULL;
  http_t *http;
  int retval = -1;
  int req_close, close;
  ta_list ta;
  http_header_t const *as_info = NULL;

  if (req == NULL || status < 100 || status >= 600) {
    return -1;
  }

  response = req->req_response;
  http = http_object(response);

  if (status >= 200 && req->req_as)
    as_info = (http_header_t const *)req->req_as->as_info;

  ta_start(ta, tag, value);

  http_add_tl(response, http,
	      HTTPTAG_SERVER(req->req_server->srv_server),
	      HTTPTAG_HEADER(as_info),
	      ta_tags(ta));

  if (http->http_payload && !http->http_content_length) {
    http_content_length_t *l;
    http_payload_t *pl;
    size_t len = 0;

    for (pl = http->http_payload; pl; pl = pl->pl_next)
      len += pl->pl_len;

    if (len > UINT32_MAX)
      goto fail;

    l = http_content_length_create(msg_home(response), (uint32_t)len);

    msg_header_insert(response, (msg_pub_t *)http, (msg_header_t *)l);
  }

  if (req->req_method == http_method_head && http->http_payload) {
    http_payload_t *pl;

    for (pl = http->http_payload; pl; pl = pl->pl_next)
      msg_header_remove(response, (msg_pub_t *)http, (msg_header_t *)pl);
  }

  http_complete_response(response, status, phrase,
			 http_object(req->req_request));

  if (!http->http_date) {
    http_date_t date[1];
    http_date_init(date)->d_time = msg_now();
    msg_header_add_dup(response, (msg_pub_t *)http, (msg_header_t*)date);
  }

  if (status < 200) {
    close = 0;
    next = server_msg_create(req->req_server, 0, NULL, 0, NULL, NULL);
  }
  else {
    req_close = req->req_close;

    close = (http->http_connection &&
	     msg_params_find(http->http_connection->k_items, "close"));

    if (req_close && !close && status >= 200) {
      close = 1;
      http_add_tl(response, http, HTTPTAG_CONNECTION_STR("close"), TAG_END());
    }
  }

  msg_serialize(response, (msg_pub_t *)http);

  retval = tport_tqsend(req->req_tport, response, next,
			TAG_IF(close, TPTAG_CLOSE_AFTER(1)),
			ta_tags(ta));

 fail:
  ta_end(ta);

  if (retval == 0)
    req->req_status = status;

  return retval;
}
