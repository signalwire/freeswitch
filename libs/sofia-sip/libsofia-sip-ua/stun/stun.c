/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005-2006 Nokia Corporation.
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

/**@internal
 * @file stun.c STUN client module
 *
 * See RFC 3489/3489bis for further information.
 *
 * @author Martti Mela <Martti.Mela@nokia.com>
 * @author Tat Chan <Tat.Chan@nokia.com>
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <Kai.Vehmanen@nokia.com>
 *
 * @date Created: Thu Jul 24 17:21:00 2003 ppessi
 */

#include "config.h"

#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#define SU_ROOT_MAGIC_T struct stun_magic_t

#include <sofia-sip/stun.h>
#include "stun_internal.h"
#include <sofia-sip/stun_tag.h>

#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_log.h>
#include <sofia-sip/su.h>
#include <sofia-sip/su_localinfo.h>

#if HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

#if HAVE_OPENSSL
#include <openssl/opensslv.h>
#endif

/* Missing socket symbols */
#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
static char const __func__[] = "stun";
#endif

#ifndef SU_DEBUG
#define SU_DEBUG 3
#endif

/** STUN log. */
su_log_t stun_log[] = { SU_LOG_INIT("stun", "STUN_DEBUG", SU_DEBUG) };

/**@var char const STUN_DEBUG[]
 *
 * Environment variable determining the debug log level for @b stun module.
 *
 * The STUN_DEBUG environment variable is used to determine the debug logging
 * level for @b stun module. The default level is 3.
 *
 * @sa <sofia-sip/su_debug.h>, stun_log, SOFIA_DEBUG
 */
extern char const STUN_DEBUG[];

enum {
  STUN_SENDTO_TIMEOUT = 1000,
  STUN_TLS_CONNECT_TIMEOUT = 8000,
};

/**
 * States of STUN requests. See stun_state_e for states
 * discovery processes.
 */
typedef enum stun_req_state_e {
  stun_req_no_assigned_event,
  stun_req_dispose_me,            /**< request can be disposed */
  stun_req_discovery_init,
  stun_req_discovery_processing,

  stun_req_error,
  stun_req_timeout
} stun_req_state_t;

#define CHG_IP		0x001
#define CHG_PORT	0x004

#define x_insert(l, n, x) \
 ((l) ? (l)->x##_prev = &(n)->x##_next : 0, \
  (n)->x##_next = (l), (n)->x##_prev = &(l), (l) = (n))

#define x_remove(n, x) \
  ((*(n)->x##_prev = (n)->x##_next) ? \
   (n)->x##_next->x##_prev = (n)->x##_prev : 0)

#define x_is_inserted(n, x) ((n)->x##_prev != NULL)

struct stun_discovery_s {
  stun_discovery_t   *sd_next, **sd_prev; /**< Linked list */

  stun_handle_t          *sd_handle;
  stun_discovery_f        sd_callback;
  stun_discovery_magic_t *sd_magic;

  tagi_t          *sd_tags;          /** stored tags for the discovery */


  su_addrinfo_t    sd_pri_info;      /**< server primary info */
  su_sockaddr_t    sd_pri_addr[1];   /**< server primary address */

  su_addrinfo_t    sd_sec_info;      /**< server secondary info */
  su_sockaddr_t    sd_sec_addr[1];   /**< server secondary address */

  stun_action_t    sd_action;        /**< My action */
  stun_state_t     sd_state;         /**< Progress states */

  su_socket_t      sd_socket;        /**< target socket */
  su_sockaddr_t    sd_bind_addr[1]; /**< local address */

  su_socket_t      sd_socket2;       /**< Alternative socket */

  int              sd_index;         /**< root_register index */

  /* Binding discovery */
  su_sockaddr_t    sd_addr_seen_outside[1];   /**< local address */

  /* NAT type related */
  stun_nattype_t   sd_nattype;       /**< Determined NAT type */
  int              sd_mapped_addr_match; /** Mapped addresses match? */
  int              sd_first;         /**< These are the requests  */
  int              sd_second;
  int              sd_third;
  int              sd_fourth;

  /* Life time related */
  int              sd_lt_cur;
  int              sd_lt;
  int              sd_lt_max;

  /* Keepalive timeout */
  unsigned int     sd_timeout;
  su_timer_t      *sd_timer;
};

struct stun_request_s {
  su_timer_t       *sr_timer;
  stun_request_t   *sr_next, **sr_prev; /**< Linked list */
  stun_msg_t       *sr_msg;             /**< STUN message pointer */
  stun_handle_t    *sr_handle;          /**< backpointer, STUN object */

  su_socket_t       sr_socket;          /**< Alternative socket */
  su_localinfo_t    sr_localinfo;       /**< local addrinfo */
  su_sockaddr_t     sr_local_addr[1];   /**< local address */
  su_sockaddr_t     sr_destination[1];

  stun_req_state_t  sr_state;           /**< Progress states */
  int               sr_retry_count;     /**< current retry number */
  long              sr_timeout;         /**< timeout for next sendto() */

  int               sr_from_y;
  int               sr_request_mask;    /**< Mask consisting of chg_ip and chg_port */
  stun_discovery_t *sr_discovery;
};

struct stun_handle_s
{
  su_home_t       sh_home[1];
  su_root_t      *sh_root;          /**< event loop */
  int             sh_root_index;    /**< object index of su_root_register() */

  stun_request_t *sh_requests; /**< outgoing requests list */
  stun_discovery_t *sh_discoveries; /**< Actions list */

  int             sh_max_retries;   /**< max resend for sendto() */

  su_addrinfo_t   sh_pri_info;      /**< server primary info */
  su_sockaddr_t   sh_pri_addr[1];   /**< server primary address */

  su_addrinfo_t   sh_sec_info;      /**< server secondary info */
  su_sockaddr_t   sh_sec_addr[1];   /**< server secondary address */

  su_localinfo_t  sh_localinfo;     /**< local addrinfo */
  su_sockaddr_t   sh_local_addr[1]; /**< local address */

  char           *sh_domain;        /**< domain address for DNS-SRV lookups */

  stun_dns_lookup_t  *sh_dns_lookup;
  stun_action_t       sh_dns_pend_action;
  stun_discovery_f    sh_dns_pend_cb;
  stun_discovery_magic_t *sh_dns_pend_ctx;
  tagi_t             *sh_dns_pend_tags;

#if HAVE_OPENSSL
  SSL_CTX        *sh_ctx;           /**< SSL context for TLS */
  SSL            *sh_ssl;           /**< SSL handle for TLS */
#else
  void           *sh_ctx;           /**< SSL context for TLS */
  void           *sh_ssl;           /**< SSL handle for TLS */
#endif

  stun_msg_t      sh_tls_request;
  stun_msg_t      sh_tls_response;
  int             sh_nattype;       /**< NAT-type, see stun_common.h */

  stun_buffer_t   sh_username;
  stun_buffer_t   sh_passwd;

  int             sh_use_msgint;    /**< try message integrity (TLS) */
  int             sh_req_msgint;    /**< require use of msg-int (TLS) */
};


#define STUN_STATE_STR(x) case x: return #x

char const *stun_str_state(stun_state_t state)
{
  switch (state) {
  STUN_STATE_STR(stun_no_assigned_event);
  /* STUN_STATE_STR(stun_req_dispose_me); */
  STUN_STATE_STR(stun_tls_connecting);
  STUN_STATE_STR(stun_tls_writing);
  STUN_STATE_STR(stun_tls_closing);
  STUN_STATE_STR(stun_tls_reading);
  STUN_STATE_STR(stun_tls_done);
  /* STUN_STATE_STR(stun_req_discovery_init); */
  /* STUN_STATE_STR(stun_req_discovery_processing); */
  STUN_STATE_STR(stun_discovery_done);
  STUN_STATE_STR(stun_tls_connection_timeout);
  STUN_STATE_STR(stun_tls_connection_failed);
  STUN_STATE_STR(stun_tls_ssl_connect_failed);
  STUN_STATE_STR(stun_discovery_timeout);
  /* STUN_STATE_STR(stun_req_timeout); */

  case stun_error:
  default: return "stun_error";
  }
}

/**
 * Returns the NAT type attached to STUN discovery handle.
 *
 * @see stun_nattype_str().
 * @see stun_test_nattype().
 */
char const *stun_nattype_str(stun_discovery_t *sd)
{
  char const *stun_nattype_str[] = {
    "NAT type undetermined",
    "Open Internet",
    "UDP traffic is blocked or server unreachable",
    "Symmetric UDP Firewall",
    "Full-Cone NAT (endpoint independent filtering and mapping)",
    "Restricted Cone NAT (endpoint independent mapping)",
    "Port Restricted Cone NAT (endpoint independent mapping)",
    "Endpoint independent filtering, endpoint dependent mapping",
    "Address dependent filtering, endpoint dependent mapping",
    "Symmetric NAT (address and port dependent filtering, endpoint dependent mapping)",
  };

  if (sd)
    return stun_nattype_str[sd->sd_nattype];
  else
    return stun_nattype_str[stun_nat_unknown];
}

/**
 * Returns the detected NAT type.
 *
 * @see stun_nattype_str().
 * @see stun_test_nattype().
 */
stun_nattype_t stun_nattype(stun_discovery_t *sd)
{
  if (!sd)
    return stun_nat_unknown;

  return sd->sd_nattype;
}

su_addrinfo_t const *stun_server_address(stun_handle_t *sh)
{
  return &sh->sh_pri_info;
}

int stun_lifetime(stun_discovery_t *sd)
{
  return sd ? sd->sd_lt_cur : -1;
}


#if HAVE_OPENSSL
char const stun_version[] =
 "sofia-sip-stun using " OPENSSL_VERSION_TEXT;
#else
char const stun_version[] =
 "sofia-sip-stun";
#endif

static int do_action(stun_handle_t *sh, stun_msg_t *binding_response);
#if HAVE_OPENSSL
static int stun_tls_callback(su_root_magic_t *m, su_wait_t *w, su_wakeup_arg_t *arg);
#endif
static int process_binding_request(stun_request_t *req, stun_msg_t *binding_response);
static stun_discovery_t *stun_discovery_create(stun_handle_t *sh,
					       stun_action_t action,
					       stun_discovery_f sdf,
					       stun_discovery_magic_t *magic);
static int stun_discovery_destroy(stun_discovery_t *sd);
static int action_bind(stun_request_t *req, stun_msg_t *binding_response);
static int action_determine_nattype(stun_request_t *req, stun_msg_t *binding_response);
static int process_test_lifetime(stun_request_t *req, stun_msg_t *binding_response);

static stun_request_t *stun_request_create(stun_discovery_t *sd);
static int stun_send_binding_request(stun_request_t *req,
			      su_sockaddr_t *srvr_addr);
static int stun_bind_callback(stun_magic_t *m, su_wait_t *w, su_wakeup_arg_t *arg);

#if defined (__CYGWIN__)
static int get_localinfo(int family, su_sockaddr_t *su, socklen_t *return_len);
#endif

/* timers */
static void stun_sendto_timer_cb(su_root_magic_t *magic,
				 su_timer_t *t,
				 su_timer_arg_t *arg);
#if HAVE_OPENSSL
static void stun_tls_connect_timer_cb(su_root_magic_t *magic,
				      su_timer_t *t,
				      su_timer_arg_t *arg);
#endif
static void stun_test_lifetime_timer_cb(su_root_magic_t *magic,
					su_timer_t *t,
					su_timer_arg_t *arg);
static void stun_keepalive_timer_cb(su_root_magic_t *magic,
				    su_timer_t *t,
				    su_timer_arg_t *arg);


static int priv_stun_bind_send(stun_handle_t *sh, stun_request_t *req, stun_discovery_t *sd);
static int priv_dns_queue_action(stun_handle_t *sh,
				 stun_action_t action,
				 stun_discovery_f sdf,
				 stun_discovery_magic_t *magic,
				 tag_type_t tag, tag_value_t value, ...);

/**
 * Return su_root_t assigned to stun_handle_t.
 *
 * @param self stun_handle_t object
 * @return su_root_t object, NULL if self not given.
 */
su_root_t *stun_root(stun_handle_t *self)
{
  return self ? self->sh_root : NULL;
}


/**
 * Check if a STUN handle should be created.
 *
 * Return true if STUNTAG_SERVER() or STUNTAG_DOMAIN() tags have
 * been specified, or otherwise if STUN_SERVER environment variable
 * is set.
 *
 * @TAGS
 * @TAG STUNTAG_DOMAIN() domain to use in DNS-SRV based STUN server
 * @TAG STUNTAG_SERVER() stun server hostname or dotted IPv4 address
 *
 * @param tag,value,... tag-value list
 */
int stun_is_requested(tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  tagi_t const *t, *t2;
  char const *stun_server;

  enter;

  ta_start(ta, tag, value);
  t = tl_find(ta_args(ta), stuntag_server);
  t2 = tl_find(ta_args(ta), stuntag_domain);
  if (t && t->t_value)
    stun_server = (char *)t->t_value;
  else if (t2 && t2->t_value)
    stun_server = (char *)t2->t_value;
  else
    stun_server = getenv("STUN_SERVER");
  ta_end(ta);

  return stun_server != NULL;
}


/**
 * Creates a STUN handle.
 *
 * The created handles can be used for STUN binding discovery,
 * keepalives, and other STUN usages.
 *
 * @param root eventloop to used by the stun state machine
 * @param tag,value,... tag-value list
 *
 * @TAGS
 * @TAG STUNTAG_DOMAIN() domain to use in DNS-SRV based STUN server
 * @TAG STUNTAG_SERVER() stun server hostname or dotted IPv4 address
 * @TAG STUNTAG_REQUIRE_INTEGRITY() true if msg integrity should be
 * used enforced
 *
 */
stun_handle_t *stun_handle_init(su_root_t *root,
				tag_type_t tag, tag_value_t value, ...)
{
  stun_handle_t *stun = NULL;
  char const *server = NULL, *domain = NULL;
  int req_msg_integrity = 1;
  int err;
  ta_list ta;

  enter;

  ta_start(ta, tag, value);

  tl_gets(ta_args(ta),
	  STUNTAG_SERVER_REF(server),
	  STUNTAG_DOMAIN_REF(domain),
	  STUNTAG_REQUIRE_INTEGRITY_REF(req_msg_integrity),
	  TAG_END());

  ta_end(ta);

  stun = su_home_clone(NULL, sizeof(*stun));

  if (!stun) {
    SU_DEBUG_3(("%s: %s failed\n", __func__, "su_home_clone()"));
    return NULL;
  }

  /* Enviroment overrides */
  if (getenv("STUN_SERVER")) {
    server = getenv("STUN_SERVER");
    SU_DEBUG_5(("%s: using STUN_SERVER=%s\n", __func__, server));
  }

  SU_DEBUG_5(("%s(\"%s\"): called\n",
	      __func__, server));

  /* fail, if no server or a domain for a DNS-SRV lookup is specified */
  if (!server && !domain) {
    errno = ENOENT;
    return NULL;
  }

  stun->sh_pri_info.ai_addrlen = 16;
  stun->sh_pri_info.ai_addr = &stun->sh_pri_addr->su_sa;

  stun->sh_sec_info.ai_addrlen = 16;
  stun->sh_sec_info.ai_addr = &stun->sh_sec_addr->su_sa;

  stun->sh_localinfo.li_addrlen = 16;
  stun->sh_localinfo.li_addr = stun->sh_local_addr;

  stun->sh_domain = su_strdup(stun->sh_home, domain);
  stun->sh_dns_lookup = NULL;

  if (server) {
    err = stun_atoaddr(stun->sh_home, AF_INET, &stun->sh_pri_info, server);
    if (err < 0) {
      errno = ENOENT;
      return NULL;
    }
  }

  stun->sh_nattype = stun_nat_unknown;

  stun->sh_root     = root;
  /* always try TLS: */
  stun->sh_use_msgint = 1;
  /* whether use of shared-secret msgint is required */
  stun->sh_req_msgint = req_msg_integrity;

  stun->sh_max_retries = STUN_MAX_RETRX;

  /* initialize username and password */
  stun_init_buffer(&stun->sh_username);
  stun_init_buffer(&stun->sh_passwd);

  stun->sh_nattype = stun_nat_unknown;

  /* initialize random number generator */
  srand(time(NULL));

  return stun;
}

/**
 * Performs shared secret request/response processing.
 * Result will be trigged in STUN handle callback (state
 * one of stun_tls_*).
 **/
int stun_obtain_shared_secret(stun_handle_t *sh,
			      stun_discovery_f sdf,
			      stun_discovery_magic_t *magic,
			      tag_type_t tag, tag_value_t value, ...)
{
#if HAVE_OPENSSL
  int events = -1;
  int one, err = -1;
  su_wait_t wait[1] = { SU_WAIT_INIT };
  su_socket_t s = INVALID_SOCKET;
  int family;
  su_addrinfo_t *ai = NULL;
  stun_discovery_t *sd;
  /* stun_request_t *req; */

  ta_list ta;

  assert(sh);

  enter;

  if (!sh->sh_pri_addr[0].su_port) {
    /* no STUN server address, perform a DNS-SRV lookup */

    ta_list ta;
    ta_start(ta, tag, value);
    SU_DEBUG_5(("Delaying STUN shared-secret req. for DNS-SRV query.\n" VA_NONE));
    err = priv_dns_queue_action(sh, stun_action_tls_query, sdf, magic, ta_tags(ta));
    ta_end(ta);

    return err;
  }

  ai = &sh->sh_pri_info;

  if (sh->sh_use_msgint == 1) {
    SU_DEBUG_3(("%s: Obtaining shared secret.\n", __func__));
  }
  else {
    SU_DEBUG_3(("No message integrity enabled.\n" VA_NONE));
    return errno = EFAULT, -1;
  }

  /* open tcp connection to server */
  s = su_socket(family = AF_INET, SOCK_STREAM, 0);

  if (s == INVALID_SOCKET) {
    return STUN_ERROR(errno, socket);
  }

  /* asynchronous connect() */
  if (su_setblocking(s, 0) < 0) {
    return STUN_ERROR(errno, su_setblocking);
  }

  if (setsockopt(s, SOL_TCP, TCP_NODELAY,
		 (void *)&one, sizeof one) == -1) {
    return STUN_ERROR(errno, setsockopt);
  }

  /* Do an asynchronous connect(). Three error codes are ok,
   * others cause return -1. */
  if (connect(s, (struct sockaddr *) &sh->sh_pri_addr,
	      ai->ai_addrlen) == SOCKET_ERROR) {
    err = su_errno();
    if (err != EINPROGRESS && err != EAGAIN && err != EWOULDBLOCK) {
      return STUN_ERROR(err, connect);
    }
  }

  SU_DEBUG_9(("%s: %s: %s\n", __func__, "connect",
	      su_strerror(err)));

  sd = stun_discovery_create(sh, stun_action_tls_query, sdf, magic);
  sd->sd_socket = s;
  /* req = stun_request_create(sd); */

  events = SU_WAIT_CONNECT | SU_WAIT_ERR;
  if (su_wait_create(wait, s, events) == -1)
    return STUN_ERROR(errno, su_wait_create);

  /* su_root_eventmask(sh->sh_root, sh->sh_root_index, s, events); */

  if ((sd->sd_index =
       su_root_register(sh->sh_root, wait, stun_tls_callback, (su_wakeup_arg_t *) sd, 0)) == -1) {
    return STUN_ERROR(errno, su_root_register);
  }

  ta_start(ta, tag, value);
  sd->sd_tags = tl_adup(sh->sh_home, ta_args(ta));
  ta_end(ta);

  sd->sd_state = stun_tls_connecting;

  /* Create and start timer for connect() timeout */
  SU_DEBUG_3(("%s: creating timeout timer for connect()\n", __func__));

  sd->sd_timer = su_timer_create(su_root_task(sh->sh_root),
				 STUN_TLS_CONNECT_TIMEOUT);

  su_timer_set(sd->sd_timer, stun_tls_connect_timer_cb, (su_wakeup_arg_t *) sd);

  return 0;
#else /* !HAVE_OPENSSL */
  return -1;
#endif
}

static stun_request_t *stun_request_create(stun_discovery_t *sd)
{
  stun_handle_t *sh = sd->sd_handle;
  stun_request_t *req = NULL;

  enter;

  req = calloc(sizeof(stun_request_t), 1);
  if (!req)
    return NULL;

  req->sr_handle = sh;
  req->sr_discovery = sd;

  /* This is the default */
  req->sr_socket = sd->sd_socket;

  req->sr_localinfo.li_addrlen = sizeof(su_sockaddr_t);
  req->sr_localinfo.li_addr = req->sr_local_addr;

  /* default timeout for next sendto() */
  req->sr_timeout = STUN_SENDTO_TIMEOUT;
  req->sr_retry_count = 0;
  /* req->sr_action = action; */
  req->sr_request_mask = 0;

  req->sr_msg = calloc(sizeof(stun_msg_t), 1);

  req->sr_state = stun_req_discovery_init;
  memcpy(req->sr_local_addr, sd->sd_bind_addr, sizeof(su_sockaddr_t));

  /* Insert this request to the request queue */
  x_insert(sh->sh_requests, req, sr);

  return req;
}

void stun_request_destroy(stun_request_t *req)
{
  //stun_handle_t *sh;
  assert(req);

  enter;

  //sh = req->sr_handle;

  if (x_is_inserted(req, sr))
    x_remove(req, sr);

  req->sr_handle = NULL;
  req->sr_discovery = NULL;
  /* memset(req->sr_destination, 0, sizeof(su_sockaddr_t)); */

  if (req->sr_timer) {
    su_timer_destroy(req->sr_timer);
    req->sr_timer = NULL;
    SU_DEBUG_7(("%s: timer destroyed.\n", __func__));
  }

  if (req->sr_msg) {
    free(req->sr_msg);
    req->sr_msg = NULL;
  }

  free(req);

  SU_DEBUG_9(("%s: request destroyed.\n", __func__));

  return;
}


/** Destroy a STUN client */
void stun_handle_destroy(stun_handle_t *sh)
{
  stun_discovery_t *sd = NULL, *kill = NULL;
  stun_request_t *a, *b;

  enter;

  if (sh->sh_dns_lookup)
    stun_dns_lookup_destroy(sh->sh_dns_lookup);

  if (sh->sh_dns_pend_tags)
    su_free(sh->sh_home, sh->sh_dns_pend_tags);

  for (a = sh->sh_requests; a; ) {
    b = a->sr_next;
    stun_request_destroy(a);
    a = b;
  }


  /* There can be several discoveries using the same socket. It is
     still enough to deregister the socket in first of them */
  for (sd = sh->sh_discoveries; sd; ) {
    kill = sd;
    sd = sd->sd_next;

    /* Index has same value as sockfd, right? ... or not? */
    if (kill->sd_index != -1)
      su_root_deregister(sh->sh_root, kill->sd_index);
    if (kill->sd_action == stun_action_tls_query)
      su_close(kill->sd_socket);

    stun_discovery_destroy(kill);
  }

  stun_free_message(&sh->sh_tls_request);
  stun_free_message(&sh->sh_tls_response);
  stun_free_buffer(&sh->sh_username);
  stun_free_buffer(&sh->sh_passwd);

  su_home_zap(sh->sh_home);
}


/** Create wait object and register it to the handle callback */
static
int assign_socket(stun_discovery_t *sd, su_socket_t s, int register_socket)
{
  stun_handle_t *sh = sd->sd_handle;
  int events;
  stun_discovery_t *tmp;
  /* su_localinfo_t clientinfo[1]; */
  su_sockaddr_t su[1];
  socklen_t sulen;
  char addr[SU_ADDRSIZE];
  int err;

  su_wait_t wait[1] = { SU_WAIT_INIT };

  enter;

  if (s == INVALID_SOCKET) {
    SU_DEBUG_3(("%s: invalid socket\n", __func__));
    return errno = EINVAL, -1;
  }

  for (tmp = sh->sh_discoveries; tmp; tmp = tmp->sd_next) {
    if (tmp->sd_socket == s) {
      sd->sd_socket = s;
      sd->sd_index = tmp->sd_index;
      memcpy(sd->sd_bind_addr, tmp->sd_bind_addr, sizeof(su_sockaddr_t));
      return 0;
    }
  }
  sd->sd_socket = s;

  if (!register_socket)
    return 0;

  /* set socket asynchronous */
  if (su_setblocking(s, 0) < 0) {
    return STUN_ERROR(errno, su_setblocking);
  }

  /* xxx -- check if socket is already assigned to this root */

  memset(su, 0, sulen = sizeof su);

  /* Try to bind it if not already bound */
  if (getsockname(s, &su->su_sa, &sulen) == -1 || su->su_port == 0) {

    sulen = su->su_len = sizeof su->su_sin;
    su->su_family = AF_INET;

#if defined (__CYGWIN__)
    get_localinfo(AF_INET, su, &sulen);
#endif

    if ((err = bind(s, &su->su_sa, sulen)) < 0) {
      SU_DEBUG_3(("%s: bind(%s:%u): %s\n",  __func__,
		  su_inet_ntop(su->su_family, SU_ADDR(su), addr, sizeof(addr)),
		  (unsigned) ntohs(su->su_port),
		  su_strerror(su_errno())));
      return -1;
    }

    if (getsockname(s, &su->su_sa, &sulen) == -1) {
      return STUN_ERROR(errno, getsockname);
    }
  }

  memcpy(&sd->sd_bind_addr, su, sulen);

  SU_DEBUG_3(("%s: local socket is bound to %s:%u\n", __func__,
	      su_inet_ntop(su->su_family, SU_ADDR(su), addr, sizeof(addr)),
	      (unsigned) ntohs(su->su_port)));

  events = SU_WAIT_IN | SU_WAIT_ERR;

  if (su_wait_create(wait, s, events) == -1) {
    return STUN_ERROR(su_errno(), su_wait_create);
  }

  /* Register receiving function with events specified above */
  if ((sd->sd_index = su_root_register(sh->sh_root,
				       wait, stun_bind_callback,
				       (su_wakeup_arg_t *) sd, 0)) < 0) {
    return STUN_ERROR(errno, su_root_register);
  }

  SU_DEBUG_7(("%s: socket registered.\n", __func__));

  return 0;
}


/**
 * Helper function needed by Cygwin builds.
 */
#if defined (__CYGWIN__)
static int get_localinfo(int family, su_sockaddr_t *su, socklen_t *return_len)
{
  su_localinfo_t hints[1] = {{ LI_CANONNAME | LI_NUMERIC }}, *li, *res = NULL;
  int i, error;
  char addr[SU_ADDRSIZE];

  hints->li_family = family;

  if ((error = su_getlocalinfo(hints, &res)) == 0) {
    /* try to bind to the first available address */
    for (i = 0, li = res; li; li = li->li_next) {
      if (li->li_family != family)
	continue;
      if (li->li_scope == LI_SCOPE_HOST)
	continue;

      assert(*return_len >= li->li_addrlen);

      memcpy(su, li->li_addr, *return_len = li->li_addrlen);

      SU_DEBUG_3(("%s: using local address %s\n", __func__,
		  su_inet_ntop(family, SU_ADDR(su), addr, sizeof(addr))));
      break;
    }

    su_freelocalinfo(res);

    if (!li) {			/* Not found */
      return STUN_ERROR(error, su_getlocalinfo);
    }

    return 0;
  }
  else {
    return STUN_ERROR(error, su_getlocalinfo);
  }
}
#endif

static void priv_lookup_cb(stun_dns_lookup_t *self,
			   stun_magic_t *magic)
{
  const char *udp_target = NULL;
  uint16_t udp_port = 0;
  int res;
  stun_handle_t *sh = (stun_handle_t *)magic;

  res = stun_dns_lookup_udp_addr(self, &udp_target, &udp_port);
  if (res == 0 && udp_target) {
    /* XXX: assumption that same host and port used for UDP/TLS */
    stun_atoaddr(sh->sh_home, AF_INET, &sh->sh_pri_info, udp_target);

    if (udp_port)
      sh->sh_pri_addr[0].su_port = htons(udp_port);
    else
      sh->sh_pri_addr[0].su_port = htons(STUN_DEFAULT_PORT);

    /* step: now that server address is known, continue
     *       the pending action */

    SU_DEBUG_5(("STUN server address found, running queue actions (%d).\n",
		sh->sh_dns_pend_action));

    switch(sh->sh_dns_pend_action) {
    case stun_action_tls_query:
      stun_obtain_shared_secret(sh, sh->sh_dns_pend_cb, sh->sh_dns_pend_ctx, TAG_NEXT(sh->sh_dns_pend_tags));
      break;

    case stun_action_binding_request:
      stun_bind(sh, sh->sh_dns_pend_cb, sh->sh_dns_pend_ctx, TAG_NEXT(sh->sh_dns_pend_tags));
      break;

    case stun_action_test_lifetime:
      stun_test_lifetime(sh, sh->sh_dns_pend_cb, sh->sh_dns_pend_ctx, TAG_NEXT(sh->sh_dns_pend_tags));
      break;

    case stun_action_test_nattype:
      stun_test_nattype(sh, sh->sh_dns_pend_cb, sh->sh_dns_pend_ctx, TAG_NEXT(sh->sh_dns_pend_tags));
      break;

    default:
      SU_DEBUG_5(("Warning: unknown pending STUN DNS-SRV action.\n" VA_NONE));
    }
      }
  else {
    /* DNS lookup failed */
    SU_DEBUG_5(("Warning: STUN DNS-SRV lookup failed.\n" VA_NONE));
    if (sh->sh_dns_pend_cb) {
      sh->sh_dns_pend_cb(sh->sh_dns_pend_ctx, sh, NULL,
			 sh->sh_dns_pend_action, stun_error);
    }
  }

  su_free(sh->sh_home, sh->sh_dns_pend_tags), sh->sh_dns_pend_tags = NULL;
  sh->sh_dns_pend_action = 0;
  sh->sh_dns_pend_cb = NULL;
  sh->sh_dns_pend_ctx = NULL;
}

/**
 * Queus a discovery process for later execution when DNS-SRV lookup
 * has been completed.
 */
static int priv_dns_queue_action(stun_handle_t *sh,
				 stun_action_t action,
				 stun_discovery_f sdf,
				 stun_discovery_magic_t *magic,
				 tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;

  if (!sh->sh_dns_pend_action) {
    if (!sh->sh_dns_lookup) {
      sh->sh_dns_lookup = stun_dns_lookup((stun_magic_t*)sh, sh->sh_root, priv_lookup_cb, sh->sh_domain);
      ta_start(ta, tag, value);
      assert(sh->sh_dns_pend_tags == NULL);
      sh->sh_dns_pend_tags = tl_tlist(sh->sh_home,
				      ta_tags(ta));
      ta_end(ta);
      sh->sh_dns_pend_cb = sdf;
      sh->sh_dns_pend_ctx = magic;

    }
    sh->sh_dns_pend_action |= action;

    return 0;
  }

  return -1;
}

static int priv_stun_bind_send(stun_handle_t *sh, stun_request_t *req, stun_discovery_t *sd)
{
  int res = stun_send_binding_request(req, sh->sh_pri_addr);
  if (res < 0) {
    stun_free_message(req->sr_msg);
    stun_discovery_destroy(sd);
  }
  return res;
}

/**
 * Performs a STUN Binding Discovery (see RFC3489/3489bis) process
 *
 * To integrity protect the discovery process, first call
 * stun_request_shared_secret() on the handle 'sh'.
 *
 * If STUNTAG_REGISTER_SOCKET() is omitted, or set to false, the
 * client is responsible for socket i/o. Other stun module will
 * perform the whole discovery process and return the results
 * via callback 'sdf'.
 *
 * @param sh       pointer to valid stun handle
 * @param sdf      callback to signal process progress
 * @param magic    context pointer attached to 'sdf'
 * @param tag, value, ... list of tagged parameters.
 *
 * @TAGS
 * @TAG STUNTAG_SOCKET() Bind socket handle to STUN (socket handle).
 * @TAG STUNTAG_REGISTER_SOCKET() Register socket for eventloop owned by STUN (boolean)
 *
 * @return
 * On success, zero is returned.  Upon error, -1 is returned, and @e errno is
 * set appropriately.
 *
 * @ERRORS
 * @ERROR EFAULT          An invalid address is given as argument
 * @ERROR EPROTONOSUPPORT Not a UDP socket.
 * @ERROR EINVAL          The socket is already bound to an address.
 * @ERROR EACCESS   	  The address is protected, and the user is not
 *                  	  the super-user.
 * @ERROR ENOTSOCK  	  Argument is a descriptor for a file, not a socket.
 * @ERROR EAGAIN          Operation in progress. Application should call
 *                        stun_bind() again when there is data available on
 *                        the socket.
 */
int stun_bind(stun_handle_t *sh,
	      stun_discovery_f sdf,
	      stun_discovery_magic_t *magic,
	      tag_type_t tag, tag_value_t value,
	      ...)
{
  su_socket_t s = INVALID_SOCKET;
  stun_request_t *req = NULL;
  stun_discovery_t *sd = NULL;
  ta_list ta;
  int s_reg = 0;

  enter;

  if (sh == NULL)
    return errno = EFAULT, -1;

  if (!sh->sh_pri_addr[0].su_port) {
    /* no STUN server address, perform a DNS-SRV lookup */
    int err;
    ta_list ta;
    ta_start(ta, tag, value);
    SU_DEBUG_5(("Delaying STUN bind for DNS-SRV query.\n" VA_NONE));
    err = priv_dns_queue_action(sh, stun_action_binding_request, sdf, magic, ta_tags(ta));
    ta_end(ta);
    return err;
  }

  ta_start(ta, tag, value);

  tl_gets(ta_args(ta),
	  STUNTAG_SOCKET_REF(s),
	  STUNTAG_REGISTER_EVENTS_REF(s_reg),
	  TAG_END());

  ta_end(ta);

  sd = stun_discovery_create(sh, stun_action_binding_request, sdf, magic);
  if (assign_socket(sd, s, s_reg) < 0)
    return -1;

  req = stun_request_create(sd);

  if (stun_make_binding_req(sh, req, req->sr_msg, 0, 0) < 0) {
    stun_discovery_destroy(sd);
    stun_free_message(req->sr_msg);
    return -1;
  }

  /* note: we always report success if bind() succeeds */
  return priv_stun_bind_send(sh, req, sd);
}


/**
 * Returns the address of the public binding allocated by the NAT.
 *
 * In case of multiple on path NATs, the binding allocated by
 * the outermost NAT is returned.
 *
 * This function returns the local address seen from outside.
 * Note that the address is not valid until the event stun_clien_done is launched.
 */
int stun_discovery_get_address(stun_discovery_t *sd,
			       void *addr,
			       socklen_t *return_addrlen)
{
  socklen_t siz;

  enter;

  assert(sd && addr);

  siz = SU_SOCKADDR_SIZE(sd->sd_addr_seen_outside);

  /* Check if enough memory provided */
  if (siz > *return_addrlen)
    return errno = EFAULT, -1;
  else
    *return_addrlen = siz;

  memcpy(addr, sd->sd_addr_seen_outside, siz);

  return 0;
}

static stun_discovery_t *stun_discovery_create(stun_handle_t *sh,
					       stun_action_t action,
					       stun_discovery_f sdf,
					       stun_discovery_magic_t *magic)
{
  stun_discovery_t *sd = NULL;

  enter;

  sd = calloc(1, sizeof(stun_discovery_t));

  sd->sd_action = action;
  sd->sd_handle = sh;
  sd->sd_callback = sdf;
  sd->sd_magic = magic;

  sd->sd_lt_cur = 0;
  sd->sd_lt = STUN_LIFETIME_EST;
  sd->sd_lt_max = STUN_LIFETIME_MAX;

  sd->sd_pri_info.ai_addrlen = sizeof sd->sd_pri_addr->su_sin;
  sd->sd_pri_info.ai_addr = &sd->sd_pri_addr->su_sa;

  /* Insert this action to the discovery queue */
  x_insert(sh->sh_discoveries, sd, sd);

  return sd;
}

static int stun_discovery_destroy(stun_discovery_t *sd)
{
  //stun_handle_t *sh;

  enter;

  if (!sd)
    return errno = EFAULT, -1;

  //sh = sd->sd_handle;

  if (sd->sd_timer)
    su_timer_destroy(sd->sd_timer), sd->sd_timer = NULL;

  /* if we are in the queue*/
  if (x_is_inserted(sd, sd))
    x_remove(sd, sd);

  sd->sd_next = NULL;

  free(sd);
  return 0;
}

/**
 * Initiates STUN discovery process to find out NAT
 * characteristics.
 *
 * Process partly follows the algorithm defined in RFC3489 section
 * 10.1. Due the known limitations of RFC3489, some of the tests
 * are done.
 *
 * Note: does not support STUNTAG_DOMAIN() even if specified to
 * stun_handle_init().
 *
 * @TAGS
 * @TAG STUNTAG_SOCKET Bind socket for STUN
 * @TAG STUNTAG_REGISTER_SOCKET Register socket for eventloop owned by STUN
 * @TAG STUNTAG_SERVER() stun server hostname or dotted IPv4 address
 *
 * @return 0 on success, non-zero on error
 */
int stun_test_nattype(stun_handle_t *sh,
		       stun_discovery_f sdf,
		       stun_discovery_magic_t *magic,
		       tag_type_t tag, tag_value_t value,
		       ...)
{
  int err = 0, index = 0, s_reg = 0;
  ta_list ta;
  char const *server = NULL;
  stun_request_t *req = NULL;
  stun_discovery_t *sd = NULL;
  su_socket_t s = INVALID_SOCKET;
  su_sockaddr_t *destination = NULL;

  enter;

  if (!sh->sh_pri_addr[0].su_port) {
    /* no STUN server address, perform a DNS-SRV lookup */

    ta_list ta;
    ta_start(ta, tag, value);
    SU_DEBUG_5(("Delaying STUN get-nat-type req. for DNS-SRV query.\n" VA_NONE));
    err = priv_dns_queue_action(sh, stun_action_test_nattype, sdf, magic, ta_tags(ta));
    ta_end(ta);

    return err;
  }

  ta_start(ta, tag, value);
  tl_gets(ta_args(ta),
	  STUNTAG_SOCKET_REF(s),
	  STUNTAG_REGISTER_EVENTS_REF(s_reg),
	  STUNTAG_SERVER_REF(server),
	  TAG_END());

  ta_end(ta);

  if (s < 0)
    return errno = EFAULT, -1;

  sd = stun_discovery_create(sh, stun_action_test_nattype, sdf, magic);
  sd->sd_mapped_addr_match = -1;

  if ((index = assign_socket(sd, s, s_reg)) < 0)
    return errno = EFAULT, -1;

  /* If no server given, use default address from stun_handle_init() */
  if (!server) {
    /* memcpy(&sd->sd_pri_info, &sh->sh_pri_info, sizeof(su_addrinfo_t)); */
    memcpy(sd->sd_pri_addr, sh->sh_pri_addr, sizeof(su_sockaddr_t));
  }
  else {
    err = stun_atoaddr(sh->sh_home, AF_INET, &sd->sd_pri_info, server);
    memcpy(sd->sd_pri_addr, &sd->sd_pri_info.ai_addr, sizeof(su_sockaddr_t));
  }
  destination = (su_sockaddr_t *) sd->sd_pri_addr;

  req = stun_request_create(sd);
  if (stun_make_binding_req(sh, req, req->sr_msg,
			    STUNTAG_CHANGE_IP(0),
			    STUNTAG_CHANGE_PORT(0),
			    TAG_END()) < 0)
    return -1;

  err = stun_send_binding_request(req, destination);
  if (err < 0) {
    stun_free_message(req->sr_msg);
    return -1;
  }

  /* Same Public IP and port, Test III, server ip 0 or 1 should be
     the same */
  req = stun_request_create(sd);
  if (stun_make_binding_req(sh, req, req->sr_msg,
			    STUNTAG_CHANGE_IP(0),
			    STUNTAG_CHANGE_PORT(1),
			    TAG_END()) < 0)
    return -1;

  err = stun_send_binding_request(req, destination);
  if (err < 0) {
    stun_free_message(req->sr_msg);
    return -1;
  }
  req = NULL;

  req = stun_request_create(sd);
  if (stun_make_binding_req(sh, req, req->sr_msg,
			    STUNTAG_CHANGE_IP(1),
			    STUNTAG_CHANGE_PORT(1),
			    TAG_END()) < 0)
    return -1;

  err = stun_send_binding_request(req, destination);
  if (err < 0) {
    stun_free_message(req->sr_msg);
  }

  return err;
}

/********************************************************************
 * Internal functions
 *******************************************************************/

#if HAVE_OPENSSL
static
int stun_tls_callback(su_root_magic_t *m, su_wait_t *w, su_wakeup_arg_t *arg)
{
  stun_discovery_t *sd = arg;
  stun_handle_t *self = sd->sd_handle;
  stun_msg_t *msg_req, *resp;
  int z, err = -1;
  SSL_CTX* ctx;
  SSL *ssl;
  X509* server_cert;
  unsigned char buf[512];
  stun_attr_t *password, *username;
  int state;
  int events = su_wait_events(w, sd->sd_socket), one = 0;
  socklen_t onelen;

  enter;

  SU_DEBUG_7(("%s(%p): events%s%s%s%s\n", __func__, (void *)self,
	      events & SU_WAIT_CONNECT ? " CONNECTED" : "",
	      events & SU_WAIT_ERR     ? " ERR"       : "",
	      events & SU_WAIT_IN      ? " IN"        : "",
	      events & SU_WAIT_OUT     ? " OUT"       : ""));

  getsockopt(sd->sd_socket, SOL_SOCKET, SO_ERROR,
	     (void *)&one, &onelen);
  if (one != 0) {
    STUN_ERROR(one, SO_ERROR);
  }

  if (one || events & SU_WAIT_ERR) {
    su_wait_destroy(w);
    su_root_deregister(self->sh_root, sd->sd_index);
    sd->sd_index = -1; /* mark index as deregistered */

    su_timer_reset(sd->sd_timer);

    SU_DEBUG_3(("%s: shared secret not obtained from server. "	\
		"Proceed without username/password.\n", __func__));

    sd->sd_state = stun_tls_connection_failed;

    if (sd->sd_callback)
      sd->sd_callback(sd->sd_magic, self, sd, sd->sd_action, sd->sd_state);

    return 0;
  }

  /* Can be NULL, too */
  ssl  = self->sh_ssl;
  msg_req  = &self->sh_tls_request;
  resp = &self->sh_tls_response;

  state = sd->sd_state;
  switch (state) {
  case stun_tls_connecting:

    /* compose shared secret request */
    if (stun_make_sharedsecret_req(msg_req) != 0) {
      STUN_ERROR(errno, stun_make_sharedsecret_req);
      stun_free_buffer(&msg_req->enc_buf);
      return -1;
    }

    /* openssl initiation */
    SSLeay_add_ssl_algorithms();
    SSL_load_error_strings();
    ctx = SSL_CTX_new(TLSv1_client_method());
    self->sh_ctx = ctx;

    if (ctx == NULL) {
      STUN_ERROR(errno, SSL_CTX_new);
      stun_free_buffer(&msg_req->enc_buf);
      return -1;
    }

    if (SSL_CTX_set_cipher_list(ctx, "AES128-SHA") == 0) {
      STUN_ERROR(errno, SSL_CTX_set_cipher_list);
      stun_free_buffer(&msg_req->enc_buf);
      return -1;
    }

    /* Start TLS negotiation */
    ssl = SSL_new(ctx);
    self->sh_ssl = ssl;

    if (SSL_set_fd(ssl, sd->sd_socket) == 0) {
      STUN_ERROR(err, connect);
      stun_free_buffer(&msg_req->enc_buf);
      return -1;
    }

    /* No break here! Continue to SSL_connect. If SSL_continue returns
     * less than 1 because of nonblocking, have a different state
     * (ssl_connecting) for it */

  case stun_tls_ssl_connecting:
    events = SU_WAIT_ERR | SU_WAIT_IN;
    su_root_eventmask(self->sh_root, sd->sd_index,
		      sd->sd_socket, events);

    z = SSL_connect(ssl);
    err = SSL_get_error(ssl, z);
    if (z < 1 && (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)) {
      sd->sd_state = stun_tls_ssl_connecting;
      return 0;
    }
    else if (z < 1) {
      su_wait_destroy(w);
      su_root_deregister(self->sh_root, sd->sd_index);
      sd->sd_index = -1; /* mark index as deregistered */

      stun_free_buffer(&msg_req->enc_buf);
      sd->sd_state = stun_tls_connection_failed;

      if (sd->sd_callback)
	sd->sd_callback(sd->sd_magic, self, sd, sd->sd_action, sd->sd_state);

      return -1;
    }

    /* Inform application about the progress  */
    sd->sd_state = stun_tls_writing;
    /* self->sh_callback(self->sh_context, self, self->sh_state); */

    events = SU_WAIT_ERR | SU_WAIT_OUT;
    su_root_eventmask(self->sh_root, sd->sd_index,
		      sd->sd_socket, events);

    break;

  case stun_tls_writing:

    events = SU_WAIT_ERR | SU_WAIT_IN;
    su_root_eventmask(self->sh_root, sd->sd_index,
		      sd->sd_socket, events);

    SU_DEBUG_3(("TLS connection using %s\n", SSL_get_cipher(ssl)));

    server_cert = SSL_get_peer_certificate(ssl);
    if(server_cert) {
      SU_DEBUG_3(("\t subject: %s\n", X509_NAME_oneline(X509_get_subject_name(server_cert), 0, 0)));
      SU_DEBUG_3(("\t issuer: %s\n", X509_NAME_oneline(X509_get_issuer_name(server_cert), 0, 0)));
    }
    X509_free(server_cert);

    z = SSL_write(ssl, msg_req->enc_buf.data, msg_req->enc_buf.size);

    if (z < 0) {
      err = SSL_get_error(ssl, z);
      if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
	return 0;
      else {
	STUN_ERROR(errno, SSL_write);
	stun_free_buffer(&msg_req->enc_buf);
	return -1;
      }
    }
    sd->sd_state = stun_tls_reading;

    break;

  case stun_tls_reading:
    events = SU_WAIT_ERR | SU_WAIT_OUT;
    su_root_eventmask(self->sh_root, sd->sd_index,
		      sd->sd_socket, events);

    SU_DEBUG_5(("Shared Secret Request sent to server:\n" VA_NONE));
    debug_print(&msg_req->enc_buf);

    z = SSL_read(ssl, buf, sizeof(buf));
    if (z <= 0) {
      err = SSL_get_error(ssl, z);
      if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
	return 0;
      else {
	stun_free_buffer(&msg_req->enc_buf);
	return -1;
      }
    }

    /* We end up here after there's something to read from the
     * socket */
    resp->enc_buf.size = z;
    resp->enc_buf.data = malloc(z);
    memcpy(resp->enc_buf.data, buf, z);
    SU_DEBUG_5(("Shared Secret Response received from server:\n" VA_NONE));
    debug_print(&resp->enc_buf);

    /* closed TLS connection */
    SSL_shutdown(ssl);

    su_close(sd->sd_socket), sd->sd_socket = INVALID_SOCKET;

    SSL_free(self->sh_ssl), ssl = NULL;
    SSL_CTX_free(self->sh_ctx), ctx = NULL;

    stun_free_buffer(&msg_req->enc_buf);

    /* process response */
    if (stun_parse_message(resp) < 0) {
      perror("stun_parse_message");
      stun_free_buffer(&resp->enc_buf);
      return -1;
    }

    switch(resp->stun_hdr.msg_type) {
    case SHARED_SECRET_RESPONSE:
      username = stun_get_attr(resp->stun_attr, USERNAME);
      password = stun_get_attr(resp->stun_attr, PASSWORD);
      if (username != NULL && password != NULL) {
	/* move result to se */
	stun_copy_buffer(&self->sh_username, username->pattr);
	stun_copy_buffer(&self->sh_passwd, password->pattr);
      }
      break;

    case SHARED_SECRET_ERROR_RESPONSE:
      if (stun_process_error_response(resp) < 0) {
	SU_DEBUG_5(("Error in Shared Secret Error Response.\n" VA_NONE));
      }
      stun_free_buffer(&resp->enc_buf);
      return -1;

      break;

    default:
      break;
    }

    su_wait_destroy(w);
    su_root_deregister(self->sh_root, sd->sd_index);
    sd->sd_index = -1; /* mark index as deregistered */

    self->sh_use_msgint = 1;
    sd->sd_state = stun_tls_done;

    if (sd->sd_callback)
      sd->sd_callback(sd->sd_magic, self, sd, sd->sd_action, sd->sd_state);

    break;

  default:
    return -1;
  }

  return 0;
}

#endif /* HAVE_OPENSSL */


#if HAVE_OPENSSL
static void stun_tls_connect_timer_cb(su_root_magic_t *magic,
				      su_timer_t *t,
				      su_timer_arg_t *arg)
{
  stun_discovery_t *sd = arg;
  stun_handle_t *sh = sd->sd_handle;

  enter;

  su_timer_destroy(t);
  if (t == sd->sd_timer) {
    sd->sd_timer = NULL;
  }

  SU_DEBUG_7(("%s: timer destroyed.\n", __func__));

  if (sd->sd_state != stun_tls_connecting)
    return;

  SU_DEBUG_7(("%s: connect() timeout.\n", __func__));

  su_root_deregister(sh->sh_root, sd->sd_index);
  sd->sd_index = -1; /* mark index as deregistered */

  sd->sd_state = stun_tls_connection_timeout;
  sd->sd_callback(sd->sd_magic, sh, sd, sd->sd_action, sd->sd_state);

  return;
}

#endif /* HAVE_OPENSSL */

/** Compose a STUN message of the format defined by stun_msg_t
 *  result encoded in enc_buf ready for sending as well.
 */
int stun_make_sharedsecret_req(stun_msg_t *msg)
{

  int i, len;
  uint16_t tmp;

  /* compose header */
  msg->stun_hdr.msg_type = SHARED_SECRET_REQUEST;
  msg->stun_hdr.msg_len = 0; /* actual len computed by
				stun_send_message */
  for (i = 0; i < STUN_TID_BYTES; i++) {
    msg->stun_hdr.tran_id[i] = (1 + rand() % RAND_MAX_16);
  }

  /* no buffer assigned yet */
  stun_init_buffer(&msg->enc_buf);

  msg->enc_buf.data = malloc(20);
  msg->enc_buf.size = 20;

  tmp = htons(msg->stun_hdr.msg_type);
  len = 0;

  memcpy(msg->enc_buf.data, &tmp, sizeof(tmp));
  len+=sizeof(tmp);

  tmp = htons(msg->stun_hdr.msg_len);
  memcpy(msg->enc_buf.data+len, &tmp, sizeof(tmp));
  len+=sizeof(tmp);

  memcpy(msg->enc_buf.data+len, msg->stun_hdr.tran_id, STUN_TID_BYTES);
  len+=STUN_TID_BYTES;

  return 0;
}


/* Return action of the request. If no request, return default value */
su_inline
stun_action_t get_action(stun_request_t *req)
{
  stun_discovery_t *sd = NULL;

  /* XXX -- if no sr_handle something is leaking... */
  if (!req || !req->sr_discovery || !req->sr_handle)
    return stun_action_no_action;

  sd = req->sr_discovery;
  return sd->sd_action;
}


/* Find request from the request queue, based on TID */
su_inline
stun_request_t *find_request(stun_handle_t *self, void *id)
{
  void *match;
  stun_request_t *req = NULL;
  int len = STUN_TID_BYTES;

  for (req = self->sh_requests; req; req = req->sr_next) {
    match = req->sr_msg->stun_hdr.tran_id;
    if (memcmp(match, id, len) == 0) {
      return req;
    }
  }

  return NULL;
}

/** Process socket event */
static int stun_bind_callback(stun_magic_t *m, su_wait_t *w, su_wakeup_arg_t *arg)
{
  stun_discovery_t *sd = arg;
  stun_handle_t *self = sd->sd_handle;

  int retval = -1, err = -1, dgram_len;
  char addr[SU_ADDRSIZE];
  stun_msg_t binding_response, *msg;
  unsigned char dgram[512] = { 0 };
  su_sockaddr_t recv;
  socklen_t recv_len;
  su_socket_t s = sd->sd_socket;
  int events = su_wait_events(w, s);

  enter;

  SU_DEBUG_7(("%s(%p): events%s%s%s\n", __func__, (void *)self,
	      events & SU_WAIT_IN ? " IN" : "",
	      events & SU_WAIT_OUT ? " OUT" : "",
	      events & SU_WAIT_ERR ? " ERR" : ""));

  if (!(events & SU_WAIT_IN || events & SU_WAIT_OUT)) {
    /* su_wait_destroy(w); */
    /* su_root_deregister(self->sh_root, self->ss_root_index); */
    /* self->sh_state = stun_bind_error; */
    return 0;
  }

  /* receive response */
  recv_len = sizeof(recv);
  dgram_len = su_recvfrom(s, dgram, sizeof(dgram), 0, &recv, &recv_len);
  err = errno;
  if ((dgram_len < 0) && (err != EAGAIN)) {
    /* su_wait_destroy(w); */
    /* su_root_deregister(self->sh_root, self->ss_root_index); */
    STUN_ERROR(err, recvfrom);
    /* stun_free_message(binding_request); */
    return err;
  }
  else if (dgram_len <= 0) {
    STUN_ERROR(err, recvfrom);
    /* No data available yet, wait for the event. */
    return 0;
  }

  /* Message received. */
  binding_response.enc_buf.data = (unsigned char *) malloc(dgram_len);
  binding_response.enc_buf.size = dgram_len;
  memcpy(binding_response.enc_buf.data, dgram, dgram_len);


  SU_DEBUG_5(("%s: response from server %s:%u\n", __func__,
	      su_inet_ntop(recv.su_family, SU_ADDR(&recv), addr, sizeof(addr)),
	      ntohs(recv.su_port)));

  debug_print(&binding_response.enc_buf);

  /* Parse here the incoming message. */
  if (stun_parse_message(&binding_response) < 0) {
    stun_free_message(&binding_response);
    SU_DEBUG_5(("%s: Error parsing response.\n", __func__));
    return retval;
  }

  /* Based on the decoded payload, find the corresponding request
   * (based on TID). */

  do_action(self, &binding_response);

  if (binding_response.enc_buf.size)
    free(binding_response.enc_buf.data);

  {
    stun_attr_t **a, *b;

    msg = &binding_response;
    for (a = &msg->stun_attr; *a;) {

      if ((*a)->pattr)
	free((*a)->pattr);

      if ((*a)->enc_buf.data)
	free((*a)->enc_buf.data);

      b = *a;
      b = b->next;
      free(*a);
      *a = NULL;
      *a = b;
    }
  }

  return 0;
}

/** Choose the right state machine */
static int do_action(stun_handle_t *sh, stun_msg_t *msg)
{
  stun_request_t *req = NULL;
  stun_action_t action = stun_action_no_action;
  void *id;

  enter;

  if (!sh)
    return errno = EFAULT, -1;

  id = msg->stun_hdr.tran_id;
  req = find_request(sh, id);
  if (!req) {
    SU_DEBUG_7(("warning: unable to find matching TID for response\n" VA_NONE));
    return 0;
  }

  action = get_action(req);

  /* Based on the action, use different state machines */
  switch (action) {
  case stun_action_binding_request:
    action_bind(req, msg);
    break;

  case stun_action_test_nattype:
    action_determine_nattype(req, msg);
    break;

  case stun_action_test_lifetime:
    process_test_lifetime(req, msg);
    break;

  case stun_action_keepalive:
    SU_DEBUG_3(("%s: Response to keepalive received.\n", __func__));
    req->sr_state = stun_req_dispose_me;
    break;

  case stun_action_no_action:
    SU_DEBUG_3(("%s: Unknown response. No matching request found.\n", __func__));
    req->sr_state = stun_req_dispose_me;
    break;

  default:
    SU_DEBUG_3(("%s: bad action.\n", __func__));
    req->sr_state = stun_req_error;

    req->sr_state = stun_req_dispose_me;
    break;
  }

  return 0;
}

static int process_binding_request(stun_request_t *req, stun_msg_t *binding_response)
{
  int retval = -1, clnt_addr_len;
  stun_attr_t *mapped_addr, *chg_addr;
  stun_handle_t *self = req->sr_handle;
  su_localinfo_t *clnt_info = &req->sr_localinfo;
  su_sockaddr_t *clnt_addr = clnt_info->li_addr;
  stun_msg_t *binding_request;
  stun_discovery_t *sd = req->sr_discovery;

  enter;

  binding_request = req->sr_msg;

  switch (binding_response->stun_hdr.msg_type) {
  case BINDING_RESPONSE:
    if (stun_validate_message_integrity(binding_response, &self->sh_passwd) < 0) {
      stun_free_message(binding_request);
      stun_free_message(binding_response);
      return retval;
    }

    memset(clnt_addr, 0, sizeof(su_sockaddr_t));
    clnt_addr_len = sizeof(su_sockaddr_t);
    mapped_addr = stun_get_attr(binding_response->stun_attr, MAPPED_ADDRESS);

    if (mapped_addr != NULL) {
      memcpy(clnt_addr, mapped_addr->pattr, clnt_addr_len);
      retval = 0;
    }

    /* update alternative server address */
    if (sd->sd_sec_addr->su_family == 0) {
      /* alternative server address not present */
      chg_addr = stun_get_attr(binding_response->stun_attr, CHANGED_ADDRESS);

      if (chg_addr != NULL)
	memcpy(sd->sd_sec_addr, chg_addr->pattr, sizeof(struct sockaddr_in));
    }

    break;

  case BINDING_ERROR_RESPONSE:
  default:
    if (stun_process_error_response(binding_response) < 0) {
      SU_DEBUG_3(("%s: Error in Binding Error Response.\n", __func__));
    }
    req->sr_state = stun_req_error;

    break;
  }

  return retval;

}

static void stun_test_lifetime_timer_cb(su_root_magic_t *magic,
				       su_timer_t *t,
				       su_timer_arg_t *arg)
{
  stun_request_t *req = arg;
  stun_discovery_t *sd = req->sr_discovery;
  su_sockaddr_t *destination;

  int err;

  enter;

  su_timer_destroy(t);

  destination = (su_sockaddr_t *) sd->sd_pri_addr;
  err = stun_send_binding_request(req, destination);
  if (err < 0) {
    stun_free_message(req->sr_msg);
    return;
  }


  return;

}

static int process_test_lifetime(stun_request_t *req, stun_msg_t *binding_response)
{
  stun_discovery_t *sd = req->sr_discovery;
  stun_request_t *new;
  stun_handle_t *sh = req->sr_handle;
  //su_localinfo_t *li;
  su_sockaddr_t *sa;
  su_timer_t *sockfdy_timer = NULL;
  su_socket_t sockfdy = sd->sd_socket2;
  int err;
  stun_action_t action = get_action(req);
  su_sockaddr_t *destination;

  /* Even the first message could not be delivered */
  if ((req->sr_state == stun_req_timeout) && (req->sr_from_y == -1)) {
    SU_DEBUG_0(("%s: lifetime determination failed.\n", __func__));
    sd->sd_state = stun_discovery_timeout;
    req->sr_state = stun_req_dispose_me;

    /* Use per discovery specific callback */
    if (sd->sd_callback)
      sd->sd_callback(sd->sd_magic, sh, sd, action, sd->sd_state);

    return 0;
  }

  if (abs(sd->sd_lt_cur - sd->sd_lt) <= STUN_LIFETIME_CI) {
    sd->sd_state = stun_discovery_done;
    req->sr_state = stun_req_dispose_me;

    /* Use per discovery specific callback */
    if (sd->sd_callback)
      sd->sd_callback(sd->sd_magic, sh, sd, action, sd->sd_state);

    return 0;
  }

  /* We come here as a response to a request send from the sockfdy */
  if (req->sr_from_y == 1) {
    req->sr_state = stun_req_dispose_me, req = NULL;

    new = stun_request_create(sd);
    new->sr_from_y = 0;
    if (stun_make_binding_req(sh, new, new->sr_msg, 0, 0) < 0)
      return -1;

    destination = (su_sockaddr_t *) sd->sd_pri_addr;
    err = stun_send_binding_request(new, destination);
    if (err < 0) {
      stun_free_message(new->sr_msg);
      return -1;
    }
    return 0;
  }
  else if (req->sr_from_y == 0) {
    if (req->sr_state != stun_req_timeout) {
      /* mapping with X still valid */
      sd->sd_lt_cur = sd->sd_lt;
      sd->sd_lt = (int) (sd->sd_lt + sd->sd_lt_max) / 2;

      SU_DEBUG_1(("%s: Response received from socket X, " \
		  "lifetime at least %d sec, next trial: %d sec\n",
		  __func__, sd->sd_lt_cur, sd->sd_lt));
    }
    else {
      sd->sd_lt_max = sd->sd_lt;
      sd->sd_lt = (int) (sd->sd_lt + sd->sd_lt_cur) / 2;
      SU_DEBUG_1(("%s: No response received from socket X, " \
		  "lifetime at most %d sec, next trial: %d sec\n",
		  __func__, sd->sd_lt_max, sd->sd_lt));
    }
  }

  /* Rock, we come from sockfdx */
  process_binding_request(req, binding_response);

  //li = &req->sr_localinfo;
  sa = req->sr_local_addr;
  stun_free_message(binding_response);

  /* Destroy me with the bad mofo timer */
  req->sr_state = stun_req_dispose_me, req = NULL;

  new = stun_request_create(sd);
  /* Use sockfdy */
  new->sr_socket = sockfdy;
  new->sr_from_y = 1;

  if (stun_make_binding_req(sh, new, new->sr_msg, 0, 0) < 0)
    return -1;

  stun_add_response_address(new->sr_msg, (struct sockaddr_in *) sa);

  /* Create and start timer */
  sockfdy_timer = su_timer_create(su_root_task(sh->sh_root), sd->sd_lt);
  su_timer_set(sockfdy_timer, stun_test_lifetime_timer_cb, (su_wakeup_arg_t *) new);

  return 0;
}


static int action_bind(stun_request_t *req, stun_msg_t *binding_response)
{
  //su_localinfo_t *li = NULL;
  su_sockaddr_t *sa = NULL;
  stun_discovery_t *sd = req->sr_discovery;
  stun_handle_t *sh = req->sr_handle;
  stun_action_t action;

  enter;
  action = get_action(req);

  process_binding_request(req, binding_response);

  //li = &req->sr_localinfo;
  sa = req->sr_local_addr;

  memcpy(sd->sd_addr_seen_outside, sa, sizeof(su_sockaddr_t));

  sd->sd_state = stun_discovery_done;
  req->sr_state = stun_req_dispose_me;

  if (sd->sd_callback)
    sd->sd_callback(sd->sd_magic, sh, sd, action, sd->sd_state);

  return 0;
}

/**
 * Returns a non-zero value if some local interface address
 * matches address in su.
 */
static int priv_find_matching_localadress(su_sockaddr_t *su)
{
  su_localinfo_t hints[1] = {{ LI_CANONNAME | LI_NUMERIC }}, *li, *res = NULL;
  int af;
  char addr[SU_ADDRSIZE];

  SU_DEBUG_5(("%s: checking if %s is a local address.\n", __func__,
	      su_inet_ntop(AF_INET, SU_ADDR(su), addr, sizeof(addr))));

  hints->li_family = af = su->su_family;

  if (su_getlocalinfo(hints, &res) != 0)
    return 0;

  /* check if any of the address match 'sockaddr'  */
  for (li = res; li; li = li->li_next) {
    if (li->li_family != af)
      continue;

    if (memcmp(SU_ADDR(su), SU_ADDR(li->li_addr), SU_ADDRLEN(su)) == 0) {
      SU_DEBUG_5(("%s: found matching local address\n", __func__));
      break;
    }

    SU_DEBUG_9(("%s: skipping local address %s.\n", __func__,
		su_inet_ntop(af, SU_ADDR(li->li_addr), addr, sizeof(addr))));
  }

  su_freelocalinfo(res);

  return li != NULL;
}

/**
 * Helper function for action_determine_nattype().
 */
static void priv_mark_discovery_done(stun_discovery_t *sd,
				     stun_handle_t *sh,
				     stun_action_t action,
				     stun_request_t *req)
{
  sd->sd_state = stun_discovery_done;
  req->sr_state = stun_req_dispose_me;
  if (sd->sd_callback)
    sd->sd_callback(sd->sd_magic, sh, sd, action, sd->sd_state);
}

/**
 * Handles responses related to the NAT type discovery process.
 *
 * The requests for Tests I-III are sent in parallel, so the
 * callback has to keep track of which requests have been received
 * and postpone decisions until enough responses have been processed.
 *
 * @see stun_test_nattype().
 */
static int action_determine_nattype(stun_request_t *req, stun_msg_t *binding_response)
{
  stun_handle_t *sh = req->sr_handle;
  su_localinfo_t *li = NULL;
  stun_discovery_t *sd = req->sr_discovery;
  stun_action_t action;
  int err;
  /* test status: 0 not received, -1 timeout, 1 response received */
  int reply_res;

  enter;

  action = get_action(req);

  /* if the NAT type is already detected, ignore this request */
  if (!sd || (sd->sd_nattype != stun_nat_unknown)) {
    req->sr_state = stun_req_dispose_me;
    /* stun_request_destroy(req); */
    return 0;
  }

  /* parse first the response payload */
  if (binding_response)
    process_binding_request(req, binding_response);

  /* get pointer to MAPPED-ADDRESS of req */
  li = &req->sr_localinfo;

  /* check whether the response timed out or not */
  if (req->sr_state == stun_req_timeout)
    reply_res = -1;
  else
    reply_res = 1;

  /* note: Test I completed - reply from the destination address
   *       where we sent our packet  */
  if (req->sr_request_mask == 0 && sd->sd_first == 0) {
    sd->sd_first = reply_res;
    if (reply_res)
      memcpy(sd->sd_addr_seen_outside, li->li_addr, sizeof(su_sockaddr_t));
  }

  /* note: Test II completed - reply from another address and port  */
  else if ((req->sr_request_mask & CHG_IP) &&
	   (req->sr_request_mask & CHG_PORT))
    sd->sd_second = reply_res;

  /* note: Test III completed -  reply from another port  */
  else if (req->sr_request_mask & CHG_PORT)
    sd->sd_third = reply_res;

  /* note: Test IV completed - request and reply to another port */
  else if (req->sr_request_mask == 0 && sd->sd_fourth == 2) {
    sd->sd_fourth = reply_res;

    /* SU_DEBUG_5(("Comparing reported MAPPED ADDRESSES %s:%u vs %s:%u.\n",
       inet_ntoa(sd->sd_addr_seen_outside[0].su_sin.sin_addr),
       sd->sd_addr_seen_outside[0].su_port,
       inet_ntoa(li->li_addr->su_sin.sin_addr),
       li->li_addr->su_port)); */

    /* note: check whether MAPPED-ADDRESS address has changed (when
     *       sending to a different IP:port -> NAT mapping behaviour) */
    if (su_cmp_sockaddr(sd->sd_addr_seen_outside, li->li_addr) != 0) {
      sd->sd_mapped_addr_match = 0;
    }
    else {
      sd->sd_mapped_addr_match = 1;
    }
  }

  SU_DEBUG_9(("stun natcheck status: 1st=%d, 2nd=%d, 3rd=%d, 4th=%d, mask=%d, sr_state=%d (timeout=%d, done=%d)..\n",
	      sd->sd_first, sd->sd_second, sd->sd_third, sd->sd_fourth, req->sr_request_mask, req->sr_state, stun_req_timeout, stun_discovery_done));

  /* case 1: no response to Test-I (symmetric response)
   *         a FW must be blocking us */
  if (sd->sd_first < 0) {
    sd->sd_nattype = stun_udp_blocked;
    priv_mark_discovery_done(sd, sh, action, req);
  }
  /* case 2: mapped address matches our local address
   *         not behind a NAT, result of test two determinces
   *         whether we are behind a symmetric FW or not */
  else if (sd->sd_first > 0 &&
	   sd->sd_second &&
	   priv_find_matching_localadress(sd->sd_addr_seen_outside)) {
    if (sd->sd_second > 0)
      sd->sd_nattype = stun_open_internet;
    else
      sd->sd_nattype = stun_sym_udp_fw;
    priv_mark_discovery_done(sd, sh, action, req);
  }
  /* case 3: response ok to Test-II, and behind a NAT
   *         do not make conclusions until Test-IV has been scheduled */
  else if (sd->sd_first > 0 &&
	   sd->sd_second > 0 &&
	   sd->sd_fourth) {
    if (sd->sd_mapped_addr_match == 1)
      sd->sd_nattype = stun_nat_full_cone;
    else
      sd->sd_nattype = stun_nat_ei_filt_ad_map;
    priv_mark_discovery_done(sd, sh, action, req);
  }
  /* case 4: tests I-III done, perform IV
   *         see notes below */
  else if (sd->sd_first > 0 &&
	   sd->sd_second &&
	   sd->sd_third &&
	   sd->sd_fourth == 0) {

    /*
     * No response received, so we now perform Test IV using the address
     * learnt from response to Test-I.
     *
     * Unfortunately running  this test will potentially affect
     * results of a subsequent Test-II (depends on NAT binding timeout
     * values). To get around this, the STUN server would ideally have
     * a dedicated IP:port for Test-IV. But within the currents specs,
     * we need to reuse one of the IP:port addresses already used in
     * Test-II by the STUN server to send us packets.
     */

    SU_DEBUG_7(("Sending STUN NAT type Test-IV request to %s.\n",
	       inet_ntoa(sd->sd_sec_addr[0].su_sin.sin_addr)));

    sd->sd_fourth = 2; /* request, -1, 0, 1 reserved for results */
    req->sr_state = stun_req_dispose_me;
    req = stun_request_create(sd);
    err = stun_make_binding_req(sh, req, req->sr_msg,
				STUNTAG_CHANGE_IP(0),
				STUNTAG_CHANGE_PORT(0),
				TAG_END());

    if (err == 0) {
      err = stun_send_binding_request(req, sd->sd_sec_addr);
    }

    if (err < 0) {
      SU_DEBUG_0(("WARNING: Failure in performing STUN Test-IV check. "
		  "The results related to mapping characteristics may be incorrect." VA_NONE));
      stun_free_message(req->sr_msg);
      sd->sd_fourth = -1;
      /* call function again, sd_fourth stops the recursion */
      action_determine_nattype(req, binding_response);
      return -1;
    }

    return 0; /* we don't want to dispose this req */
  }
  /* case 5: no response Test-II, and success with III
   *         the NAT is filtering packets from different IP
   *         do not make conclusions until Test-IV has been scheduled */
  else if (sd->sd_first > 0 &&
	   sd->sd_second < 0 &&
	   sd->sd_third > 0 &&
	   sd->sd_fourth) {
    if (sd->sd_mapped_addr_match == 1)
      sd->sd_nattype = stun_nat_res_cone;
    else
      sd->sd_nattype = stun_nat_ad_filt_ad_map;
    priv_mark_discovery_done(sd, sh, action, req);
  }
  /* case 6: no response to Test-II nor III
   *         the NAT is filtering packets from different port
   *         do not make conclusions until Test-IV has been scheduled */
  else if (sd->sd_first > 0 &&
	   sd->sd_second < 0 &&
	   sd->sd_third < 0 &&
	   sd->sd_fourth) {
    if (sd->sd_mapped_addr_match == 1)
      sd->sd_nattype = stun_nat_port_res_cone;
    else
      sd->sd_nattype = stun_nat_adp_filt_ad_map;
    priv_mark_discovery_done(sd, sh, action, req);
  }

  /* this request of the discovery process can be disposed */
  req->sr_state = stun_req_dispose_me;

  return 0;
}


static void stun_sendto_timer_cb(su_root_magic_t *magic,
				 su_timer_t *t,
				 su_timer_arg_t *arg)
{
  stun_request_t *req = arg;
  stun_handle_t *sh = req->sr_handle;
  stun_discovery_t *sd = req->sr_discovery;
  stun_action_t action = get_action(req);
  long timeout = 0;

  enter;

  if (req->sr_state == stun_req_dispose_me) {
    stun_request_destroy(req);
    SU_DEBUG_7(("%s: timer destroyed.\n", __func__));
    return;
  }

  ++req->sr_retry_count;

  /* check if max retry count has been exceeded; or if
   * action type is NAT type check (XXX: the request attributes
   * are not passed correctly to resend function) */
  if (req->sr_retry_count >= sh->sh_max_retries ||
      action == stun_action_test_nattype) {
    errno = ETIMEDOUT;
    STUN_ERROR(errno, stun_sendto_timer_cb);

    stun_free_message(req->sr_msg);
    free(req->sr_msg), req->sr_msg = NULL;

    /* Either the server was dead, address wrong or STUN_UDP_BLOCKED */
    /* sd->sd_nattype = stun_udp_blocked; */
    req->sr_state = stun_req_timeout;

    /* If the action is binding request, we are done. If action was
       NAT type determination, process with the state machine. */
    switch (action) {
    case stun_action_binding_request:
      sd->sd_state = stun_discovery_timeout;
      req->sr_state = stun_req_dispose_me;

      /* Use per discovery specific callback */
      if (sd->sd_callback)
	sd->sd_callback(sd->sd_magic, sh, sd, action, sd->sd_state);

      break;

    case stun_action_test_nattype:
      action_determine_nattype(req, NULL);
      break;

    case stun_action_test_lifetime:
      process_test_lifetime(req, NULL);
      break;

    case stun_action_keepalive:
      sd->sd_state = stun_discovery_timeout;

      /* Use per discovery specific callback */
      if (sd->sd_callback)
	sd->sd_callback(sd->sd_magic, sh, sd, action, sd->sd_state);

      stun_keepalive_destroy(sh, sd->sd_socket);

      break;

    default:
      break;
    }

    /* Destroy me immediately */
    req->sr_state = stun_req_dispose_me;
    timeout = 0;
  }
  else {
    SU_DEBUG_3(("%s: Timeout no. %d, retransmitting.\n",
		__func__, req->sr_retry_count));

    /* Use pre-defined destination address for re-sends */
    if (stun_send_message(req->sr_socket, req->sr_destination,
			  req->sr_msg, &(sh->sh_passwd)) < 0) {
      stun_free_message(req->sr_msg);
      free(req->sr_msg), req->sr_msg = NULL;
      return;
    }
    timeout = req->sr_timeout *= 2;

  }

  su_timer_set_at(t, stun_sendto_timer_cb, (su_wakeup_arg_t *) req,
		  su_time_add(su_now(), timeout));

  return;
}


/** This function sends a binding request to the address at serv (ip,
 *  port). which could be the original or alternative socket addresses
 *  of the STUN server. Local address is provided in cli, and
 *  resulting mapped address is also saved in cli.
 *  Return 0 if successful, -1 if failed
 *
 * @return
 * On success, zero is returned.  Upon error, -1 is returned, and @e errno is
 * set appropriately.
 *
 * @ERRORS
 * @ERROR EBADF           @a sockfd is not a valid deseriptor.
 * @ERROR EPROTONOSUPPORT @a sockfd is not an UDP socket.
 * @ERROR EINVAL          The socket is already bound to an address.
 * @ERROR EACCESS   	  The address is protected, and the user is not
 *                  	  the super-user.
 * @ERROR ENOTSOCK  	  Argument is a descriptor for a file, not a socket.
 * @ERROR EAGAIN          Operation in progress. Application should call
 *                        stun_bind() again when there is data available on
 *                        the socket.
 * @ERROR ETIMEDOUT       Request timed out.
 *
 */
static int stun_send_binding_request(stun_request_t *req,
				     su_sockaddr_t  *srvr_addr)
{
  su_timer_t *sendto_timer = NULL;
  su_socket_t s;
  stun_handle_t *sh = req->sr_handle;
  stun_msg_t *msg =  req->sr_msg;

  assert (sh && srvr_addr);

  enter;

  s = req->sr_socket;
  memcpy(req->sr_destination, srvr_addr, sizeof(su_sockaddr_t));

  if (stun_send_message(s, srvr_addr, msg, &(sh->sh_passwd)) < 0) {
    return -1;
  }

  /* Create and start timer */
  sendto_timer = su_timer_create(su_root_task(sh->sh_root), STUN_SENDTO_TIMEOUT);
  su_timer_set(sendto_timer, stun_sendto_timer_cb, (su_wakeup_arg_t *) req);

  req->sr_timer = sendto_timer;
  req->sr_state = stun_req_discovery_processing;

  return 0;
}


/** Compose a STUN message of the format defined by stun_msg_t */
int stun_make_binding_req(stun_handle_t *sh,
			  stun_request_t *req,
			  stun_msg_t *msg,
			  tag_type_t tag, tag_value_t value, ...)
{
  int i;
  stun_attr_t *tmp, **p;
  int bits = 0;
  int chg_ip = 0, chg_port = 0;

  ta_list ta;

  enter;

  ta_start(ta, tag, value);

  tl_gets(ta_args(ta),
	  STUNTAG_CHANGE_IP_REF(chg_ip),
	  STUNTAG_CHANGE_PORT_REF(chg_port),
	  TAG_END());

  ta_end(ta);

  if (chg_ip)
    bits |= CHG_IP;
  if (chg_port)
    bits |= CHG_PORT;

  if (req)
    req->sr_request_mask = bits;

  /* compose header */
  msg->stun_hdr.msg_type = BINDING_REQUEST;
  msg->stun_hdr.msg_len = 0; /* actual len computed by
				stun_send_message */
  for (i = 0; i < STUN_TID_BYTES; i++) {
    msg->stun_hdr.tran_id[i] = (1 + rand() % RAND_MAX_16);
  }

  /* optional attributes:
   * - Response Address
   * - Change Request X
   * - Username
   * - Message-Integrity */
  msg->stun_attr = NULL;
  /* CHANGE_REQUEST */
  p = &(msg->stun_attr);

  if (chg_ip || chg_port) {
    stun_attr_changerequest_t *attr_cr;
    tmp = (stun_attr_t *) malloc(sizeof(stun_attr_t));
    tmp->attr_type = CHANGE_REQUEST;
    attr_cr = (stun_attr_changerequest_t *) malloc(sizeof(stun_attr_changerequest_t));
    attr_cr->value =
      (chg_ip ? STUN_CR_CHANGE_IP : 0) | (chg_port ? STUN_CR_CHANGE_PORT : 0);

    tmp->pattr = attr_cr;
    tmp->next = NULL;
    *p = tmp; p = &(tmp->next);
  }

  /* USERNAME */
  if (sh->sh_use_msgint &&
      sh->sh_username.data &&
      sh->sh_passwd.data) {
    stun_buffer_t *a_pattr = (stun_buffer_t*)malloc(sizeof(stun_buffer_t));
    tmp = (stun_attr_t *) malloc(sizeof(stun_attr_t));
    tmp->attr_type = USERNAME;

    /* copy USERNAME from STUN handle */
    a_pattr->data = (void*)malloc(sh->sh_username.size);
    memcpy(a_pattr->data, sh->sh_username.data, sh->sh_username.size);
    a_pattr->size = sh->sh_username.size;
    tmp->pattr = a_pattr;

    tmp->next = NULL;
    *p = tmp; p = &(tmp->next);

    /* dummy MESSAGE_INTEGRITY attribute, computed later */
    tmp = (stun_attr_t *) malloc(sizeof(stun_attr_t));
    tmp->attr_type = MESSAGE_INTEGRITY;
    tmp->pattr = NULL;
    tmp->next = NULL;
    *p = tmp; p = &(tmp->next);
  }

  /* no buffer assigned yet */
  msg->enc_buf.data = NULL;
  msg->enc_buf.size = 0;

  return 0;
}

int stun_process_response(stun_msg_t *msg)
{

  enter;

  /* parse msg first */
  if (stun_parse_message(msg) < 0) {
    SU_DEBUG_3(("%s: Error parsing response.\n", __func__));
    return -1;
  }

  /* check message digest if exists */
  switch (msg->stun_hdr.msg_type) {
  case BINDING_RESPONSE:
    if (stun_process_binding_response(msg) < 0)
      return -1;
    break;
  case BINDING_ERROR_RESPONSE:
    if (stun_process_error_response(msg) < 0)
      return -1;
    break;
  default:
    return -1;
  }

  return 0;
}


/** process binding response */
int stun_process_binding_response(stun_msg_t *msg) {
  /* currently not needed. */
  return 0;
}


/** process binding error response
 *  Report error and return
 */
int stun_process_error_response(stun_msg_t *msg)
{
  stun_attr_t *attr;
  stun_attr_errorcode_t *ec;

  enter;

  attr = stun_get_attr(msg->stun_attr, ERROR_CODE);
  if (attr == NULL) {
    perror("stun_process_error_response");
    return -1;
  }

  ec = (stun_attr_errorcode_t *)attr->pattr;

  SU_DEBUG_5(("%s: Received Binding Error Response:\n", __func__));
  SU_DEBUG_5(("%s: Error: %d %s\n", __func__, ec->code, ec->phrase));

  return 0;
}

/**
 * Sets values for USERNAME and PASSWORD stun fields
 * for the handle.
 */
int stun_set_uname_pwd(stun_handle_t *sh,
		       const char *uname,
		       isize_t len_uname,
		       const char *pwd,
		       isize_t len_pwd)
{
  enter;

  sh->sh_username.data = malloc(len_uname);
  if (sh->sh_username.data) {
    memcpy(sh->sh_username.data, uname, len_uname);
    sh->sh_username.size = (unsigned)len_uname;
  }
  else
    return -1;

  sh->sh_passwd.data = malloc(len_pwd);
  if (sh->sh_passwd.data) {
    memcpy(sh->sh_passwd.data, pwd, len_pwd);
    sh->sh_passwd.size = (unsigned)len_pwd;
  }
  else
    return -1;

  sh->sh_use_msgint = 1; /* turn on message integrity ussage */

  return 0;
}


/**
 * Converts character address format to sockaddr_in
 */
int stun_atoaddr(su_home_t *home,
		 int ai_family,
		 su_addrinfo_t *info,
		 char const *in)
{
  su_addrinfo_t *res = NULL, *ai, hints[1] = {{ 0 }};
  char const *host;
  char *port = NULL, tmp[SU_ADDRSIZE];
  int err;
  su_sockaddr_t *dstaddr;

  assert(info && in);

  enter;

  dstaddr = (su_sockaddr_t *) info->ai_addr;

  /* note: works only for IPv4 */
  if (ai_family != AF_INET)
    return -1;

  hints->ai_family = ai_family;

  port = strstr(in, ":");
  if (port == NULL) {
    host = in;
  }
  else {
    assert((size_t)(port - in) < strlen(in) + 1);
    memcpy(tmp, in, port - in);
    tmp[port - in] = 0;
    host = tmp;
    ++port;
  }

  err = su_getaddrinfo(host, NULL, hints, &res);
  if (err == 0) {
    for (ai = res; ai; ai = ai->ai_next) {
      if (ai->ai_family != AF_INET)
	continue;

      info->ai_flags = ai->ai_flags;
      info->ai_family = ai->ai_family;
      info->ai_socktype = ai->ai_socktype;
      info->ai_protocol = ai->ai_protocol;
      info->ai_addrlen = ai->ai_addrlen;
      info->ai_canonname = su_strdup(home, host);

      memcpy(&dstaddr->su_sa, res->ai_addr, sizeof(struct sockaddr));
      break;
    }

    if (port)
      dstaddr->su_port = htons(atoi(port));
    else
      dstaddr->su_port = htons(STUN_DEFAULT_PORT);
  }
  else {
    SU_DEBUG_5(("stun_atoaddr: %s(%s): %s\n", "su_getaddrinfo", in,
		su_gai_strerror(err)));
  }

  if (res)
    su_freeaddrinfo(res);

  return err;
}

/**
 * Initiates STUN discovery process to find out NAT
 * binding life-time settings.
 *
 * @TAGS
 * @TAG STUNTAG_SOCKET Bind socket for STUN
 * @TAG STUNTAG_REGISTER_EVENTS Register socket for eventloop owned by STUN
 * @TAG STUNTAG_SERVER() stun server hostname or dotted IPv4 address
 *
 * @return 0 on success, non-zero on error
 */
int stun_test_lifetime(stun_handle_t *sh,
		      stun_discovery_f sdf,
		      stun_discovery_magic_t *magic,
		      tag_type_t tag, tag_value_t value,
		      ...)
{
  stun_request_t *req = NULL;
  stun_discovery_t *sd = NULL;
  ta_list ta;
  su_socket_t s = INVALID_SOCKET;
  int err, index = 0, s_reg = 0;
  char addr[SU_ADDRSIZE];
  char const *server = NULL;
  su_socket_t sockfdy;
  socklen_t y_len;
  su_sockaddr_t y_addr;
  su_sockaddr_t *destination;

  assert(sh);

  enter;

  if (!sh->sh_pri_addr[0].su_port) {
    /* no STUN server address, perform a DNS-SRV lookup */

    ta_list ta;
    ta_start(ta, tag, value);
    SU_DEBUG_5(("Delaying STUN get-lifetime req. for DNS-SRV query.\n" VA_NONE));
    err = priv_dns_queue_action(sh, stun_action_test_lifetime, sdf, magic, ta_tags(ta));
    ta_end(ta);

    return err;
  }

  ta_start(ta, tag, value);

  tl_gets(ta_args(ta),
	  STUNTAG_SOCKET_REF(s),
	  STUNTAG_REGISTER_EVENTS_REF(s_reg),
	  STUNTAG_SERVER_REF(server),
	  TAG_END());

  sd = stun_discovery_create(sh, stun_action_test_lifetime, sdf, magic);
  if ((index = assign_socket(sd, s, s_reg)) < 0)
      return errno = EFAULT, -1;

  /* If no server given, use default address from stun_handle_init() */
  if (!server) {
    /* memcpy(&sd->sd_pri_info, &sh->sh_pri_info, sizeof(su_addrinfo_t)); */
    memcpy(sd->sd_pri_addr, sh->sh_pri_addr, sizeof(su_sockaddr_t));
  }
  else {
    err = stun_atoaddr(sh->sh_home, AF_INET, &sd->sd_pri_info, server);
    memcpy(sd->sd_pri_addr, &sd->sd_pri_info.ai_addr, sizeof(su_sockaddr_t));
  }
  destination = (su_sockaddr_t *) sd->sd_pri_addr;

  req = stun_request_create(sd);

  /* ci = &req->sr_localinfo; */

  /* get local ip address */
  /* get_localinfo(); */

  /* initialize socket y */
  sockfdy = su_socket(AF_INET, SOCK_DGRAM, 0);

  /* set socket asynchronous */
  if (su_setblocking(sockfdy, 0) < 0) {
    STUN_ERROR(errno, su_setblocking);

    su_close(sockfdy);
    return errno = EFAULT, -1;
  }

  sd->sd_socket2 = sockfdy;

  memset(&y_addr, 0, sizeof(y_addr));
  memcpy(&y_addr, sd->sd_bind_addr, sizeof(y_addr));
  y_addr.su_port = 0;

  y_len = sizeof(y_addr);
  if (bind(sockfdy, (struct sockaddr *) &y_addr, y_len) < 0) {
    return -1;
  }

  if (getsockname(sockfdy, (struct sockaddr *) &y_addr, &y_len) < 0) {
    STUN_ERROR(errno, getsockname);
    return -1;
  }

  SU_DEBUG_3(("%s: socket y bound to %s:%u\n", __func__,
	      su_inet_ntop(y_addr.su_family, SU_ADDR(&y_addr), addr, sizeof(addr)),
	      (unsigned) ntohs(y_addr.su_port)));

  req->sr_from_y = -1;

  SU_DEBUG_1(("%s: determining binding life time, this may take a while.\n", __func__));

  if (stun_make_binding_req(sh, req, req->sr_msg, 0, 0) < 0)
    return -1;

  err = stun_send_binding_request(req, destination);
  if (err < 0) {
    stun_free_message(req->sr_msg);
    return -1;
  }

  ta_end(ta);

  return 0;
}

int stun_add_response_address(stun_msg_t *req, struct sockaddr_in *mapped_addr)
{
  stun_attr_sockaddr_t *addr;
  stun_attr_t *tmp;

  enter;

  tmp = malloc(sizeof(stun_attr_t));
  tmp->attr_type = RESPONSE_ADDRESS;
  addr = malloc(sizeof(stun_attr_sockaddr_t));
  memcpy(addr, mapped_addr, sizeof(stun_attr_sockaddr_t));
  tmp->pattr = addr;

  if (req->stun_attr == NULL) {
    tmp->next = NULL;
  }
  else {
    tmp->next = req->stun_attr;
  }
  req->stun_attr = tmp;

  return 0;
}


/**
 * Determines if the message is STUN message (-1 if not stun).
 */
int stun_msg_is_keepalive(uint16_t data)
{
  uint16_t msg_type;
  /* parse header */
  msg_type = ntohs(data);

  if (msg_type == BINDING_REQUEST ||
      msg_type == BINDING_RESPONSE ||
      msg_type == BINDING_ERROR_RESPONSE) {
    return 0;
  }

  return -1;
}

/**
 * Determines length of STUN message (0 (-1?) if not stun).
 */
int stun_message_length(void *data, isize_t len, int end_of_message)
{
  unsigned char *p;
  uint16_t msg_type;

  if (len < 4)
    return -1;

  /* parse header first */
  p = data;
  msg_type = (p[0] << 8) | p[1];

  if (msg_type == BINDING_REQUEST ||
      msg_type == BINDING_RESPONSE ||
      msg_type == BINDING_ERROR_RESPONSE) {
    /* return message length */
    return (p[0] << 8) | p[1];
  }
  else
    return -1;
}

/** Process incoming message */
int stun_process_message(stun_handle_t *sh, su_socket_t s,
			 su_sockaddr_t *sa, socklen_t salen,
			 void *data, isize_t len)
{
  int retval = -1;
  stun_msg_t msg;

  enter;

  if (len >= 65536)
    len = 65536;

  /* Message received. */
  msg.enc_buf.data = data;
  msg.enc_buf.size = (unsigned)len;

  debug_print(&msg.enc_buf);

  /* Parse here the incoming message. */
  if (stun_parse_message(&msg) < 0) {
    stun_free_message(&msg);
    SU_DEBUG_5(("%s: Error parsing response.\n", __func__));
    return retval;
  }

  if (msg.stun_hdr.msg_type == BINDING_REQUEST) {
    return stun_process_request(s, &msg, 0, sa, salen);
  }
  else if (msg.stun_hdr.msg_type == BINDING_RESPONSE) {
    /* Based on the decoded payload, find the corresponding request
     * (based on TID). */
    return do_action(sh, &msg);
  }

  return -1;
}


int stun_discovery_release_socket(stun_discovery_t *sd)
{
  stun_handle_t *sh = sd->sd_handle;

  if (su_root_deregister(sh->sh_root, sd->sd_index) >= 0) {
    SU_DEBUG_3(("%s: socket deregistered from STUN \n", __func__));
    sd->sd_index = -1; /* mark index as deregistered */

    return 0;
  }

  return -1;
}


/**
 * Creates a keepalive dispatcher for bound SIP sockets
 */
int stun_keepalive(stun_handle_t *sh,
		   su_sockaddr_t *sa,
		   tag_type_t tag, tag_value_t value,
		   ...)
{
  su_socket_t s = INVALID_SOCKET;
  unsigned int timeout = 0;
  ta_list ta;
  stun_discovery_t *sd;
  stun_request_t *req;
  stun_action_t action = stun_action_keepalive;
  char addr[SU_ADDRSIZE];

  enter;

  ta_start(ta, tag, value);

  tl_gets(ta_args(ta),
	  STUNTAG_SOCKET_REF(s),
	  STUNTAG_TIMEOUT_REF(timeout),
	  TAG_END());

  ta_end(ta);

  if (s < 1 || !sa || timeout == 0)
    return errno = EFAULT, -1;

  /* If there already is keepalive associated with the given socket,
   * destroy it. */
  stun_keepalive_destroy(sh, s);

  sd = stun_discovery_create(sh, action, NULL, NULL); /* XXX --
							 specify last
							 params if
							 necessary */
  sd->sd_socket = s;
  sd->sd_timeout = timeout;
  memcpy(sd->sd_pri_addr, sa, sizeof(*sa));

  req = stun_request_create(sd);

  SU_DEBUG_3(("%s: Starting to send STUN keepalives to %s:%u\n", __func__,
	      su_inet_ntop(sa->su_family, SU_ADDR(sa), addr, sizeof(addr)),
	      (unsigned) ntohs(sa->su_port)));

  if (stun_make_binding_req(sh, req, req->sr_msg, 0, 0) < 0 ||
      stun_send_binding_request(req, sa) < 0) {
    stun_request_destroy(req);
    stun_discovery_destroy(sd);
    return -1;
  }

  sd->sd_timer = su_timer_create(su_root_task(sh->sh_root), timeout);
  su_timer_set(sd->sd_timer, stun_keepalive_timer_cb, (su_wakeup_arg_t *) sd);

  return 0;
}

/** Send SIP keepalives */
static void stun_keepalive_timer_cb(su_root_magic_t *magic,
				    su_timer_t *t,
				    su_timer_arg_t *arg)
{
  stun_discovery_t *sd = arg;
  stun_handle_t *sh = sd->sd_handle;
  int timeout = sd->sd_timeout;
  su_sockaddr_t *destination = sd->sd_pri_addr;
  stun_request_t *req;

  enter;

  su_timer_destroy(t);

  if (sd->sd_state == stun_discovery_timeout)
    return;

  req = stun_request_create(sd);

  if (stun_make_binding_req(sh, req, req->sr_msg, 0, 0) < 0 ||
      stun_send_binding_request(req, destination) < 0) {
    stun_request_destroy(req);
    stun_discovery_destroy(sd);
    return;
  }

  sd->sd_timer = su_timer_create(su_root_task(sh->sh_root), timeout);
  su_timer_set(sd->sd_timer, stun_keepalive_timer_cb, (su_wakeup_arg_t *) sd);

  return;
}

/**
 * Destroys the keepalive dispatcher without touching the socket
 */
int stun_keepalive_destroy(stun_handle_t *sh, su_socket_t s)
{
  stun_discovery_t *sd = NULL;
  stun_request_t *req;
  stun_action_t action = stun_action_keepalive;

  if (sh == NULL)
    return 1;

  /* Go through the request queue and destroy keepalive requests
   * associated with the given socket. */
  for (req = sh->sh_requests; req; req = req->sr_next) {
    if (req->sr_socket == s && req->sr_discovery->sd_action == action) {
      req->sr_state = stun_req_dispose_me;
      if (!sd)
	sd = req->sr_discovery;
    }
  }

  /* No keepalive found */
  if (!sd)
    return 1;

  su_timer_destroy(sd->sd_timer), sd->sd_timer = NULL;
  stun_discovery_destroy(sd);

  return 0;
}


int stun_process_request(su_socket_t s, stun_msg_t *req,
			 int sid, su_sockaddr_t *from_addr,
			 socklen_t from_len)
{
  stun_msg_t resp;
  su_sockaddr_t mod_addr[1] = {{ 0 }}, src_addr[1] = {{ 0 }}, chg_addr[1] = {{ 0 }};
  stun_attr_t *tmp, m_attr[1], s_attr[1], c_attr[1], **p;
  su_sockaddr_t to_addr;
  int i;

  tmp = stun_get_attr(req->stun_attr, RESPONSE_ADDRESS);

  if (tmp) {
    memcpy(&to_addr, tmp->pattr, sizeof(to_addr));
  }
  else {
    memcpy(&to_addr, from_addr, sizeof(to_addr));
  }

  /* compose header */
  stun_init_message(&resp);
  resp.stun_hdr.msg_type = BINDING_RESPONSE;
  resp.stun_hdr.msg_len = 0; /* actual len computed later */

  for (i = 0; i < STUN_TID_BYTES; i++) {
    resp.stun_hdr.tran_id[i] = req->stun_hdr.tran_id[i];
  }
  p = &(resp.stun_attr);

  /* MAPPED-ADDRESS */
  tmp = m_attr;
  tmp->attr_type = MAPPED_ADDRESS;
  memcpy(mod_addr, from_addr, sizeof(*mod_addr));
  tmp->pattr = mod_addr;
  tmp->next = NULL;
  *p = tmp; p = &(tmp->next);

  /* SOURCE-ADDRESS depends on CHANGE_REQUEST */
  tmp = stun_get_attr(req->stun_attr, CHANGE_REQUEST);
  if (!tmp) {
    //c = 0;
  }
  else {
    switch (((stun_attr_changerequest_t *) tmp->pattr)->value) {
    case STUN_CR_CHANGE_IP:
      //c = 1;
      break;

    case STUN_CR_CHANGE_PORT:
      //c = 2;
      break;

    case STUN_CR_CHANGE_IP | STUN_CR_CHANGE_PORT: /* bitwise or */
      //c = 3;
      break;

    default:
      return -1;
    }
  }

  tmp = s_attr;
  tmp->attr_type = SOURCE_ADDRESS;

  /* memcpy(src_addr, &stun_change_map[c][sid], sizeof(*src_addr)); */
  tmp->pattr = src_addr;
  tmp->next = NULL;
  *p = tmp; p = &(tmp->next);

  /* CHANGED-ADDRESS */ /* depends on sid */
  tmp = c_attr;
  tmp->attr_type = CHANGED_ADDRESS;
  /* memcpy(chg_addr, &stun_change_map[3][sid], sizeof(*chg_addr)); */
  tmp->pattr = chg_addr;
  tmp->next = NULL;
  *p = tmp; p = &(tmp->next);


  /* no buffer assigned yet */
  resp.enc_buf.data = NULL;
  resp.enc_buf.size = 0;

  stun_send_message(s, &to_addr, &resp, NULL);
  return 0;
}

/**
 * Returns socket attached to the discovery object
 */
su_socket_t stun_discovery_get_socket(stun_discovery_t *sd)
{
  assert(sd);
  return sd->sd_socket;
}

/* -------------------------------------------------------------------
 * DEPRECATED functions
 * -------------------------------------------------------------------
 */
