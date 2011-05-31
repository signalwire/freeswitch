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

/**@internal
 * @file stun_dns.c
 * @brief Functins to discover STUN server address using DNS-SRV.
 *
 * Refs:
 *   - RFC3489/3489bis
 *   - RFC2782
 *
 * @author Kai Vehmanen <kai.vehmanen@nokia.com>
 */

#include "config.h"

#define STUN_SRV_SERVICE_TCP "_stun._tcp"
#define STUN_SRV_SERVICE_UDP "_stun._udp"

#include <sofia-sip/stun.h>
#include <sofia-sip/su.h>
#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_wait.h>
#define SRES_CONTEXT_T stun_dns_lookup_t
#include <sofia-sip/sresolv.h>

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
static char const __func__[] = "stun_dns";
#endif

#include "stun_internal.h"

#include <string.h>

struct stun_dns_lookup_s {
  su_home_t          stun_home[1];
  su_root_t         *stun_root;
  stun_magic_t      *stun_magic;
  sres_resolver_t   *stun_sres;
  su_socket_t        stun_socket;
  stun_dns_lookup_f  stun_cb;
  char              *stun_tcp_target;
  char              *stun_udp_target;
  uint16_t           stun_tcp_port;
  uint16_t           stun_udp_port;
  unsigned           stun_state:2;       /**< bit0:udp, bit1:tcp */
};

enum stun_dns_state {
  stun_dns_udp = 1,
  stun_dns_tls = 2,
  stun_dns_done = stun_dns_udp | stun_dns_tls
};

/**
 * Internal callback used for gathering DNS replies.
 */
static void priv_sres_cb(stun_dns_lookup_t *self,
			 sres_query_t *q,
			 sres_record_t **answer)
{
  int i;

  sres_sort_answers(self->stun_sres, answer);

  /* note: picks the first ones (sort puts records with most
   *       weight at start */

  for (i = 0; answer && answer[i] != NULL; i++) {
    sres_srv_record_t *rr = (sres_srv_record_t *) answer[i]->sr_srv;
    if (rr && rr->srv_record && rr->srv_record->r_type == sres_type_srv) {
      const char *tcp_name = STUN_SRV_SERVICE_TCP;
      const char *udp_name = STUN_SRV_SERVICE_UDP;
      if ((self->stun_state & stun_dns_tls) == 0 &&
	  strncmp(rr->srv_record->r_name, tcp_name, strlen(tcp_name)) == 0) {
	self->stun_tcp_target = su_strdup(self->stun_home, rr->srv_target);
	self->stun_tcp_port = rr->srv_port;
	self->stun_state |= stun_dns_tls;
	SU_DEBUG_5(("%s: stun (tcp) for domain %s is at %s:%u.\n",
		    __func__, rr->srv_record->r_name, self->stun_tcp_target, self->stun_tcp_port));
      }
      else if ((self->stun_state & stun_dns_udp) == 0 &&
	       strncmp(rr->srv_record->r_name, udp_name, strlen(udp_name)) == 0) {
	self->stun_udp_target = su_strdup(self->stun_home, rr->srv_target);
	self->stun_udp_port = rr->srv_port;
	self->stun_state |= stun_dns_udp;
	SU_DEBUG_5(("%s: stun (udp) for domain %s is at %s:%u.\n",
		    __func__, rr->srv_record->r_name, self->stun_udp_target, self->stun_udp_port));
      }
    }
  }

  if (self->stun_state == stun_dns_done) {
    self->stun_cb(self, self->stun_magic);

    sres_resolver_timer(self->stun_sres, -1);
  }

  sres_free_answers(self->stun_sres, answer);
}

/**
 * Performs a DNS-SRV check for STUN 'stun' (tcp) and
 * 'stun' (udp) services for 'domain'.
 *
 * The result will be delivered asynchronously in the
 * 'func' callback. 'root' will be used as the event loop.
 */
stun_dns_lookup_t *stun_dns_lookup(stun_magic_t *magic,
				   su_root_t *root,
				   stun_dns_lookup_f func,
				   const char *domain)
{
  stun_dns_lookup_t *self;
  sres_query_t *query;

  if (!domain ||
      strlen(domain) + strlen(STUN_SRV_SERVICE_UDP ".") + 1>= SRES_MAXDNAME)
    return NULL;

  self = su_home_new(sizeof(stun_dns_lookup_t));

  /* see nta.c:outgoing_answer_srv() */
  self->stun_magic = magic;
  self->stun_cb = func;
  self->stun_root = root;
  self->stun_sres = sres_resolver_create(root, NULL, TAG_END());
  if (self->stun_sres) {
    char srvname[SRES_MAXDNAME + 1];

    snprintf(srvname, sizeof srvname, "%s.%s", STUN_SRV_SERVICE_UDP, domain);

    query = sres_query(self->stun_sres, priv_sres_cb, self,
		       sres_type_srv,
		       srvname);

    snprintf(srvname, sizeof srvname, "%s.%s", STUN_SRV_SERVICE_TCP, domain);

    query = sres_query(self->stun_sres, priv_sres_cb, self,
		       sres_type_srv,
		       srvname);
    if (query) {
      /* TODO Just so compiler doesn't give error */
    }
}
  else {
    su_free(NULL, self), self = NULL;
  }

  return self;
}

/**
 * Destroys the 'self' object created by stun_dns_lookup_destroy().
 */
void stun_dns_lookup_destroy(stun_dns_lookup_t *self)
{
  if (self->stun_sres)
    sres_resolver_destroy(self->stun_sres);

  su_home_unref(self->stun_home);
}

/**
 * Fetches the results of a completed STUN DNS-SRV lookup
 * for "_stun._udp" service (RFC3489/3489bis).
 *
 * @param self context pointer
 * @param target location where to stored the 'target'
 *        SRV field for stun service
 * @param port location where to store port number
 *
 * @return 0 on success, non-zero otherwise
 */
int stun_dns_lookup_udp_addr(stun_dns_lookup_t *self, const char **target, uint16_t *port)
{
  int result = -1;
  if (self->stun_state == stun_dns_done) {
    if (target) *target = self->stun_udp_target;
    if (port) *port = self->stun_udp_port;
    result = 0;
  }

  return result;
}

/**
 * Fetches the results of a completed STUN DNS-SRV lookup
 * for "_stun._tcp" service (RFC3489).
 *
 * @param self context pointer
 * @param target location where to stored the 'target'
 *        SRV field for stun service
 * @param port location where to store port number
 *
 * @return 0 on success, non-zero otherwise
 */
int stun_dns_lookup_tcp_addr(stun_dns_lookup_t *self, const char **target, uint16_t *port)
{
  int result = -1;
  if (self->stun_state == stun_dns_done) {
    if (target) *target = self->stun_tcp_target;
    if (port) *port = self->stun_tcp_port;
    result = 0;
  }

  return result;
}

/**
 * Fetches the results of a completed STUN DNS-SRV lookup
 * for "_stun._udp" service (3489bis, "Short-Term Password").
 *
 * @param self context pointer
 * @param target location where to stored the 'target'
 *        SRV field for stun service
 * @param port location where to store port number
 *
 * @return 0 on success, non-zero otherwise
 */
int stun_dns_lookup_stp_addr(stun_dns_lookup_t *self, const char **target, uint16_t *port)
{
  /* XXX: not implemented */
  return -1;
}


