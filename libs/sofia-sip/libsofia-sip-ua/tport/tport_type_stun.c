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

/**@CFILE tport_type_stun.c Transport using stun.
 *
 * See tport.docs for more detailed description of tport interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 *
 * @date Created: Fri Mar 24 08:45:49 EET 2006 ppessi
 */

#include "config.h"

#define STUN_DISCOVERY_MAGIC_T  struct tport_primary

#include "tport_internal.h"

#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>

/* ---------------------------------------------------------------------- */
/* STUN */

#include <sofia-sip/stun.h>

static int tport_udp_init_stun(tport_primary_t *,
			       tp_name_t tpn[1],
			       su_addrinfo_t *,
			       tagi_t const *,
			       char const **return_culprit);

static void tport_udp_deinit_stun(tport_primary_t *pri);

static
void tport_stun_bind_cb(tport_primary_t *pri,
			stun_handle_t *sh,
			stun_discovery_t *sd,
			stun_action_t action,
			stun_state_t event);
static
void tport_stun_bind_done(tport_primary_t *pri,
			  stun_handle_t *sh,
			  stun_discovery_t *sd);
static
int tport_stun_keepalive(tport_t *tp, su_addrinfo_t const *ai,
			 tagi_t const *taglist);

static int tport_stun_response(tport_t const *self,
			       void *dgram, size_t n,
			       void *from, socklen_t fromlen);

tport_vtable_t const tport_stun_vtable =
{
  /* vtp_name 		     */ "UDP",
  /* vtp_public              */ tport_type_stun,
  /* vtp_pri_size            */ sizeof (tport_primary_t),
  /* vtp_init_primary        */ tport_udp_init_stun,
  /* vtp_deinit_primary      */ tport_udp_deinit_stun,
  /* vtp_wakeup_pri          */ NULL,
  /* vtp_connect             */ NULL,
  /* vtp_secondary_size      */ sizeof (tport_t),
  /* vtp_init_secondary      */ NULL,
  /* vtp_deinit_secondary    */ NULL,
  /* vtp_shutdown            */ NULL,
  /* vtp_set_events          */ NULL,
  /* vtp_wakeup              */ NULL,
  /* vtp_recv                */ tport_recv_dgram,
  /* vtp_send                */ tport_send_dgram,
  /* vtp_deliver             */ NULL,
  /* vtp_prepare             */ NULL,
  /* vtp_keepalive           */ tport_stun_keepalive,
  /* vtp_stun_response       */ tport_stun_response,
  /* vtp_next_secondary_timer*/ NULL,
  /* vtp_secondary_timer     */ NULL,
};

static int tport_udp_init_stun(tport_primary_t *pri,
			       tp_name_t tpn[1],
			       su_addrinfo_t *ai,
			       tagi_t const *tags,
			       char const **return_culprit)
{
  stun_handle_t *sh;

#if 0
  if (!stun_is_requested(TAG_NEXT(tags)))
    return -1;
#endif

  sh = stun_handle_init(pri->pri_master->mr_root, TAG_NEXT(tags));
  if (!sh)
    return *return_culprit = "stun_handle_init", -1;

  pri->pri_stun_handle = sh;
  tpn->tpn_canon = NULL;

  if (tport_udp_init_primary(pri, tpn, ai, tags, return_culprit) < 0)
    return -1;

#if 0
  if (stun_obtain_shared_secret(sh, tport_stun_tls_cb, pri,
				TAG_NEXT(tags)) < 0) {
    return *return_culprit = "stun_request_shared_secret()", -1;
  }
#endif

  if (stun_bind(sh, tport_stun_bind_cb, pri,
		STUNTAG_SOCKET(pri->pri_primary->tp_socket),
		STUNTAG_REGISTER_EVENTS(0),
		TAG_NULL()) < 0) {
    return *return_culprit = "stun_bind()", -1;
  }

  pri->pri_updating = 1;

  return 0;
}


static void tport_udp_deinit_stun(tport_primary_t *pri)
{
  if (pri->pri_stun_handle)
    stun_handle_destroy(pri->pri_stun_handle);
  pri->pri_stun_handle = NULL;
}


static int tport_stun_response(tport_t const *self,
			       void *dgram, size_t n,
			       void *from, socklen_t fromlen)
{
  stun_process_message(self->tp_pri->pri_stun_handle, self->tp_socket,
		       from, fromlen, (void *)dgram, n);

  return 3;
}


/**Callback for STUN bind */
static
void tport_stun_bind_cb(tport_primary_t *pri,
			stun_handle_t *sh,
			stun_discovery_t *sd,
			stun_action_t action,
			stun_state_t event)
{
  tport_master_t *mr;
  SU_DEBUG_3(("%s: %s\n", __func__, stun_str_state(event)));

  mr = pri->pri_master;

  if (event == stun_discovery_done) {
    tport_stun_bind_done(pri, sh, sd);
  }
}


static
void tport_stun_bind_done(tport_primary_t *pri,
			  stun_handle_t *sh,
			  stun_discovery_t *sd)
{
  tport_t *self = pri->pri_primary;
  su_socket_t socket;
  su_sockaddr_t *su = self->tp_addr;
  su_addrinfo_t *ai = self->tp_addrinfo;

  socket = stun_discovery_get_socket(sd);
  assert(pri->pri_primary->tp_socket == socket);

  if (stun_discovery_get_address(sd, su, &ai->ai_addrlen) == 0) {
    char ipname[SU_ADDRSIZE + 2] = { 0 };
    ai->ai_addr = (void *)su;

    SU_DEBUG_5(("%s: stun_bind() ok: local address NATed as %s:%u\n",
		__func__,
		su_inet_ntop(su->su_family, SU_ADDR(su),
			     ipname, sizeof(ipname)),
		(unsigned) ntohs(su->su_port)));
  }

  /* Send message to calling application indicating
   * there's a new public address available
   */
  tport_has_been_updated(self);

  return;
}

/** Initialize STUN keepalives.
 *
 *@retval 0
 */
static
int tport_stun_keepalive(tport_t *tp, su_addrinfo_t const *ai,
			 tagi_t const *taglist)
{
  tport_primary_t *pri = tp->tp_pri;
  int err;

  err = stun_keepalive(pri->pri_stun_handle,
		       (su_sockaddr_t *)ai->ai_addr,
		       STUNTAG_SOCKET(tp->tp_socket),
		       STUNTAG_TIMEOUT(10000),
		       TAG_NEXT(taglist));

  if (err < 0)
    return -1;

  tp->tp_has_keepalive = 1;

  return 0;
}

