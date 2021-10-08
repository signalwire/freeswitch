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
 * @file lookup_stun_server.c
 * @brief Test app for STUN DNS-SRV lookups.
 *
 * @author Kai Vehmanen <Kai.Vehmanen@nokia.com>
 *
 * @todo TODO
 * - picks one UDP and TLS target:port
 * - does not pick up A/AAAA records that might be delivered
 *   in 'Additional Data' section as defined in RFC2782
 */

#include <stdio.h>

#include <sofia-sip/su.h>
#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_wait.h>
#define STUN_MAGIC_T su_root_t
#include <sofia-sip/stun.h>

static char* g_domain = NULL;

static void lookup_cb(stun_dns_lookup_t *self,
		      su_root_t *root)
{
# define RESULT(x) (x == 0 ? "OK" : "ERRORS")

  const char *tcp_target = NULL, *udp_target = NULL, *stp_target = NULL;
  uint16_t tcp_port = 0, udp_port = 0, stp_port = 0;
  int res = 0;

  printf("STUN DNS-SRV lookup results:\n");

  res = stun_dns_lookup_udp_addr(self, &udp_target, &udp_port);
  printf(" _stun._udp.%s:     %s:%u (%s).\n", g_domain, udp_target, udp_port, RESULT(res));

  res = stun_dns_lookup_tcp_addr(self, &tcp_target, &tcp_port);
  printf(" _stun._tcp.%s:     %s:%u (%s).\n", g_domain, tcp_target, tcp_port, RESULT(res));

  res = stun_dns_lookup_stp_addr(self, &stp_target, &stp_port);
  printf(" _stun-tls._tcp.%s: %s:%u (%s).\n", g_domain, stp_target, stp_port, RESULT(res));

  su_root_break(root);
}

int main(int argc, char *argv[])
{
  su_root_t *root;
  stun_dns_lookup_t *lookup;

  if (argc < 2) {
    printf("usage: ./lookup_stun_server <domain>\n");
    return -1;
  }

  g_domain = argv[1];

  /* step: initialize sofia su OS abstraction layer */
  su_init();

  /* step: create a su event loop and connect it to glib */
  root = su_root_create(NULL);

  /* step: initiate the DNS-SRV lookup */
  lookup = stun_dns_lookup(root, root, lookup_cb, g_domain);

  if (lookup) {
    /* step: enter the main loop (break fro lookup_cb()) */
    su_root_run(root);

    /* step: free any allocated resources */
    stun_dns_lookup_destroy(lookup);
  }
  else {
    printf("ERROR: failed to create lookup object.\n");
  }

  su_root_destroy(root);
  su_deinit();

  return 0;
}
