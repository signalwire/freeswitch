/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2006 Nokia Corporation.
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

/**@CFILE tport_type_connect.c Transport using HTTP CONNECT.
 *
 * See tport.docs for more detailed description of tport interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 *
 * @date Created: Fri Mar 24 08:45:49 EET 2006 ppessi
 */

#include "config.h"

#include "tport_internal.h"

#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>

/* ---------------------------------------------------------------------- */
/* TCP using HTTP CONNECT */

#include <sofia-sip/http.h>
#include <sofia-sip/http_header.h>

static int tport_http_connect_init_primary(tport_primary_t *,
					   tp_name_t tpn[1],
					   su_addrinfo_t *,
					   tagi_t const *,
					   char const **return_culprit);

static void tport_http_connect_deinit_primary(tport_primary_t *);

static tport_t *tport_http_connect(tport_primary_t *pri, su_addrinfo_t *ai,
				   tp_name_t const *tpn);

static void tport_http_deliver(tport_t *self, msg_t *msg, su_time_t now);

typedef struct
{
  tport_primary_t thc_primary[1];
  su_addrinfo_t  *thc_proxy;
} tport_http_connect_t;

typedef struct
{
  tport_t thci_tport[1];
  msg_t *thci_response;
  msg_t *thci_stackmsg;
} tport_http_connect_instance_t;

tport_vtable_t const tport_http_connect_vtable =
{
  /* vtp_name 		     */ "TCP",
  /* vtp_public              */ tport_type_connect,
  /* vtp_pri_size            */ sizeof (tport_http_connect_t),
  /* vtp_init_primary        */ tport_http_connect_init_primary,
  /* vtp_deinit_primary      */ tport_http_connect_deinit_primary,
  /* vtp_wakeup_pri          */ NULL,
  /* vtp_connect             */ tport_http_connect,
  /* vtp_secondary_size      */ sizeof (tport_http_connect_instance_t),
  /* vtp_init_secondary      */ NULL,
  /* vtp_deinit_secondary    */ NULL,
  /* vtp_shutdown            */ NULL,
  /* vtp_set_events          */ NULL,
  /* vtp_wakeup              */ NULL,
  /* vtp_recv                */ tport_recv_stream,
  /* vtp_send                */ tport_send_stream,
  /* vtp_deliver             */ tport_http_deliver,
  /* vtp_prepare             */ NULL,
  /* vtp_keepalive           */ NULL,
  /* vtp_stun_response       */ NULL,
  /* vtp_next_secondary_timer*/ NULL,
  /* vtp_secondary_timer     */ NULL,
};

static int tport_http_connect_init_primary(tport_primary_t *pri,
					   tp_name_t tpn[1],
					   su_addrinfo_t *ai,
					   tagi_t const *tags,
					   char const **return_culprit)
{
  tport_http_connect_t *thc = (tport_http_connect_t *)pri;
  char const *http_connect = NULL;
  url_t *http_proxy;
  int error;
  char const *host, *port;
  su_addrinfo_t hints[1];

  tl_gets(tags,
	  TPTAG_HTTP_CONNECT_REF(http_connect),
	  TAG_END());
  if (!http_connect)
    return *return_culprit = "missing proxy url", -1;

  http_proxy = url_hdup(pri->pri_home, URL_STRING_MAKE(http_connect)->us_url);
  if (!http_proxy || !http_proxy->url_host)
    return *return_culprit = "invalid proxy url", -1;

  host = http_proxy->url_host;
  port = http_proxy->url_port;
  if (!port || !port[0])
    port = "8080";

  memcpy(hints, ai, sizeof hints);

  hints->ai_flags = 0;
  hints->ai_addr = NULL;
  hints->ai_addrlen = 0;
  hints->ai_next = NULL;
  hints->ai_canonname = NULL;

  error = su_getaddrinfo(host, port, hints, &thc->thc_proxy);
  if (error)
    return *return_culprit = "su_getaddrinfo", -1;

  return tport_tcp_init_client(pri, tpn, ai, tags, return_culprit);
}

static void tport_http_connect_deinit_primary(tport_primary_t *pri)
{
  tport_http_connect_t *thc = (tport_http_connect_t *)pri;

  su_freeaddrinfo(thc->thc_proxy), thc->thc_proxy = NULL;
}

static tport_t *tport_http_connect(tport_primary_t *pri, su_addrinfo_t *ai,
				   tp_name_t const *tpn)
{
  tport_http_connect_t *thc = (tport_http_connect_t *)pri;
  tport_http_connect_instance_t *thci;
  tport_master_t *mr = pri->pri_master;

  msg_t *msg, *response;

  char hostport[TPORT_HOSTPORTSIZE];

  tport_t *tport;
  http_request_t *rq;

  msg = msg_create(http_default_mclass(), 0);

  if (!msg)
    return NULL;

  tport_hostport(hostport, sizeof hostport, (void *)ai->ai_addr, 1);

  rq = http_request_format(msg_home(msg), "CONNECT %s HTTP/1.1", hostport);

  if (msg_header_insert(msg, NULL, (void *)rq) < 0
      || msg_header_add_str(msg, NULL,
			    "User-Agent: Sofia-SIP/" VERSION "\n") < 0
      || msg_header_add_str(msg, NULL, "Proxy-Connection: keepalive\n") < 0
      || msg_header_add_make(msg, NULL, http_host_class, hostport) < 0
      || msg_header_add_make(msg, NULL, http_separator_class, "\r\n") < 0
      || msg_serialize(msg, NULL) < 0
      || msg_prepare(msg) < 0)
    return (void)msg_destroy(msg), NULL;

  /*
   * Create a response message that ignores the body
   * if there is no Content-Length
   */
  response = msg_create(http_default_mclass(), mr->mr_log | MSG_FLG_MAILBOX);

  tport = tport_base_connect(pri, thc->thc_proxy, ai, tpn);
  if (!tport) {
    msg_destroy(msg); msg_destroy(response);
    return tport;
  }

  thci = (tport_http_connect_instance_t*)tport;

  thci->thci_response = response;
  tport->tp_msg = response;
  msg_set_next(response, thci->thci_stackmsg = tport_msg_alloc(tport, 512));

  if (tport_send_msg(tport, msg, tpn, NULL) < 0) {
    SU_DEBUG_9(("tport_send_msg failed in tpot_http_connect\n" VA_NONE));
    msg_destroy(msg);
    tport_zap_secondary(tport);
    return NULL;
  }

  tport_set_secondary_timer(tport);

  return tport;
}

#include <sofia-sip/msg_buffer.h>

static void tport_http_deliver(tport_t *self, msg_t *msg, su_time_t now)
{
  tport_http_connect_instance_t *thci = (tport_http_connect_instance_t*)self;

  if (msg && thci->thci_response == msg) {
    tport_http_connect_t *thc = (tport_http_connect_t *)self->tp_pri;
    http_t *http = http_object(msg);

    if (http && http->http_status) {
      SU_DEBUG_0(("tport_http_connect: %u %s\n",
		  http->http_status->st_status,
		  http->http_status->st_phrase));
      if (http->http_status->st_status < 300) {
	msg_buf_move(thci->thci_stackmsg, msg);
	thci->thci_response = NULL;
	thci->thci_stackmsg = NULL;
	return;
      }
    }

    msg_destroy(msg);
    thci->thci_response = NULL;
    tport_error_report(self, EPROTO, (void *)thc->thc_proxy->ai_addr);
    tport_close(self);
    return;
  }

  tport_base_deliver(self, msg, now);
}
