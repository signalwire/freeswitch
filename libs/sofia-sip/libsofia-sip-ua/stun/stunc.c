/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005,2006 Nokia Corporation.
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

/**
 * STUN test client
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 * @author Kai Vehmanen <Kai.Vehmanen@nokia.com>
 *
 * @date Created: Thu Jul 24 17:21:00 2003 ppessi
 */

/**@page stunc STUN test client.
 *
 * @section stunc_synopsis Synopsis
 * <tt>stunc [OPTIONS] \<stun-server-address\></tt>
 *
 * @section stunc_description Description
 * The @em stunc utility can be used to gather information about possible
 * NAT devices that are located between the client and STUN server.
 *
 * @em stunc can provide the following information: the IP address and
 * port as seen by the STUN server, detecting presence of NATs, and
 * hints on the type of address translation done. It should be noted
 * that the results of NAT type and life-time detection should be
 * considered as hints. There is no guarantee that NAT(s) will handle
 * future packets in the same way.
 *
 * @section stunc_options Command Line Options
 * The @em stunc utility accepts following command line options:
 *
 * <dl>
 *
 * <dt>-b</dt>
 * <dd>Perform a STUN binding discovery. @em stunc will report the
 * client transport address (IP:port) as seen by the STUN server. In
 * the presence of NATs, this address is allocated by the NAT closest
 * to the STUN server.
 * </dd>
 *
 * <dt>-l</dt>
 * <dd>Perform a STUN binding life-time check.
 * </dd>
 *
 * <dt>-n</dt>
 * <dd>Perform a STUN binding type check. Notice that the results
 * are only hints. Nondeterministic behaviour, resource exhaustion,
 * or reboots of network elements can cause changes in NAT behaviour
 * between successive runs of stunc.
 * </dd>
 *
 * <dt>-r</dt>
 * <dd>Randomize the local port. Otherwise @em stunc let's the
 * operating system select a free port.
 * </dd>
 *
 * <dt>-s</dt>
 * <dd>Request a shared-secret over TLS. Tests whether the STUN server
 * supports the shared-secret mechanism (needed to protect message
 * integrity). Can be combined with @em -b, @em -l and @em -n.
 * </dd>
 *
 * </dl>
 *
 * @section stunc_return Return Codes
 * <table>
 * <tr><td>0</td><td>when successful</td></tr>
 * <tr><td>1</td><td>when any errors detected</td></tr>
 * </table>
 *
 * @section stunc_examples Examples
 *
 * Discover the NAT binding, use a random local port:
 * @code
 * $ stunc stunserver.org -b -r
 * @endcode
 *
 * @section stunc_environment Environment
 * #STUN_DEBUG
 *
 * @section stunc_bugs Reporting Bugs
 * Report bugs to <sofia-sip-devel@lists.sourceforge.net>.
 *
 * @section stunc_author Authors
 * - Pekka Pessi <pekka -dot pessi -at- nokia -dot- com>
 * - Martti Mela <martti -dot mela -at- nokia -dot- com>
 * - Kai Vehmanen <kai -dot vehmanen -at- nokia -dot- com>
 *
 * @section stunc_copyright Copyright
 * Copyright (C) 2005,2006 Nokia Corporation.
 *
 * This program is free software; see the source for copying conditions.
 * There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

typedef struct stunc_s stunc_t;
#define SU_ROOT_MAGIC  stunc_t
#define STUN_MAGIC_T   stunc_t
#define STUN_DISCOVERY_MAGIC_T  stunc_t

#include "sofia-sip/stun.h"
#include "sofia-sip/stun_tag.h"
#include "sofia-sip/sofia_features.h"
#include <sofia-sip/su.h>

enum {
  do_secret = 1,
  do_bind = 2,
  do_nat_check = 4,
  do_life_check = 8,
  do_randomize_port = 16
};

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
static char const __func__[] = "stunc";
#endif

#ifndef SU_DEBUG
#define SU_DEBUG 0
#endif
#define SU_LOG (stun_log)
#include <sofia-sip/su_debug.h>

void usage(char *name)
{
  fprintf(stderr,
	  "stunc (%s)\n"
	  "usage: %s <server> [-b] [-n] [-l] [-r] [-s]\n"
	  "  -b\tmake a binding request\n"
	  "  -l\tperform NAT lifetime check\n"
	  "  -n\tperform NAT type check\n"
	  "  -r\trandomize the local port\n",
	  "  -s\trequest shared-secret over TLS (combined with -[bln])\n"
	  SOFIA_SIP_NAME_VERSION, name);
  exit(1);
}

struct stunc_s {
  su_socket_t  sc_socket;
  int          sc_flags;
};


static
void stunc_lifetime_cb(stunc_t *stunc,
		       stun_handle_t *sh,
		       stun_discovery_t *sd,
		       stun_action_t action,
		       stun_state_t event);

static
void stunc_nattype_cb(stunc_t *stunc,
		      stun_handle_t *sh,
		      stun_discovery_t *sd,
		      stun_action_t action,
		      stun_state_t event);

static
void stunc_bind_cb(stunc_t *stunc,
		   stun_handle_t *sh,
		   stun_discovery_t *sd,
		   stun_action_t action,
		   stun_state_t event);

static
void stunc_ss_cb(stunc_t *stunc,
		 stun_handle_t *sh,
		 stun_discovery_t *sd,
		 stun_action_t action,
		 stun_state_t event)
{
  int err;
  SU_DEBUG_3(("%s: %s\n", __func__, stun_str_state(event)));

  stunc->sc_flags &= ~do_secret;
  if (!stunc->sc_flags)
    su_root_break(stun_root(sh));

  switch (event) {
  case stun_tls_done:
    if (stunc->sc_flags & do_bind) {
      err = stun_bind(sh, stunc_bind_cb, stunc,
		      STUNTAG_SOCKET(stunc->sc_socket),
		      STUNTAG_REGISTER_EVENTS(1),
		      TAG_NULL());

      if (err < 0) {
	SU_DEBUG_0(("%s: %s  failed\n", __func__, "stun_handle_bind()"));
	su_root_break(stun_root(sh));
      }
    }
    break;

  case stun_tls_connection_failed:
    SU_DEBUG_0(("%s: Obtaining shared secret failed.\n",
		__func__));
    stunc->sc_flags &= ~do_bind;
    if (!stunc->sc_flags)
      su_root_break(stun_root(sh));

    break;

  case stun_tls_connection_timeout:
    SU_DEBUG_0(("%s: Timeout when obtaining shared secret.\n",
		__func__));
    stunc->sc_flags &= ~do_bind;
    break;

  default:
    break;
  }

  return;
}


static
void stunc_bind_cb(stunc_t *stunc,
		   stun_handle_t *sh,
		   stun_discovery_t *sd,
		   stun_action_t action,
		   stun_state_t event)
{
  su_sockaddr_t sa[1];
  char ipaddr[48];
  socklen_t addrlen;

  SU_DEBUG_3(("%s: %s\n", __func__, stun_str_state(event)));

  stunc->sc_flags &= ~do_bind;

  if (!stunc->sc_flags)
    su_root_break(stun_root(sh));

  switch (event) {
  case stun_discovery_done:
    addrlen = sizeof(*sa);
    memset(sa, 0, addrlen);

    if (stun_discovery_get_address(sd, sa, &addrlen) < 0) {
      SU_DEBUG_0(("%s: stun_discovery_get_address() failed", __func__));
      return;
    }

    SU_DEBUG_0(("%s: local address NATed as %s:%u\n", __func__,
		su_inet_ntop(sa->su_family, SU_ADDR(sa),
			     ipaddr, sizeof(ipaddr)),
		(unsigned) ntohs(sa->su_port)));

  break;

  case stun_discovery_timeout:
  case stun_discovery_error:
  case stun_error:
  default:
    break;
  }

  return;
}


static
void stunc_nattype_cb(stunc_t *stunc,
		      stun_handle_t *sh,
		      stun_discovery_t *sd,
		      stun_action_t action,
		      stun_state_t event)
{
  SU_DEBUG_3(("%s: %s\n", __func__, stun_str_state(event)));

  stunc->sc_flags &= ~do_nat_check;

  if (!stunc->sc_flags)
    su_root_break(stun_root(sh));

  switch (event) {
  case stun_discovery_timeout:
    SU_DEBUG_3(("%s: NAT type determination timeout.\n", __func__));
    break;

  case stun_discovery_done:
    SU_DEBUG_3(("%s: NAT type determined to be '%s' (%d).\n",
		__func__, stun_nattype_str(sd), (int)stun_nattype(sd)));
    break;

  case stun_error:
  default:
    break;
  }

  return;
}


static
void stunc_lifetime_cb(stunc_t *stunc,
		       stun_handle_t *sh,
		       stun_discovery_t *sd,
		       stun_action_t action,
		       stun_state_t event)
{
  SU_DEBUG_3(("%s: %s\n", __func__, stun_str_state(event)));

  stunc->sc_flags &= ~do_life_check;

  if (!stunc->sc_flags)
    su_root_break(stun_root(sh));

  switch (event) {
  case stun_discovery_timeout:
    SU_DEBUG_3(("%s: Lifetime determination timeout.\n", __func__));
    break;

  case stun_discovery_done:
    SU_DEBUG_3(("%s: Lifetime determined to be %d.\n", __func__, stun_lifetime(sd)));
    break;

  case stun_error:
  default:
    break;
  }

  return;
}


int main(int argc, char *argv[])
{
  int err = 0, i, sflags = 0;
  stunc_t stunc[1];
  su_root_t *root;
  stun_handle_t *sh;
  su_socket_t s;

  if (su_init() != 0)
    return -1;

  root = su_root_create(stunc);

  if (argc < 3)
    usage(argv[0]);

  for (i = 2; argv[i]; i++) {
    if (strcmp(argv[i], "-s") == 0)
      sflags |= do_secret;
    else if (strcmp(argv[i], "-b") == 0)
      sflags |= do_bind;
    else if (strcmp(argv[i], "-n") == 0)
      sflags |= do_nat_check;
    else if (strcmp(argv[i], "-l") == 0)
      sflags |= do_life_check;
    else if (strcmp(argv[i], "-r") == 0)
      sflags |= do_randomize_port;
    else {
      fprintf(stderr, "Unable to parse option %s.\n", argv[i]);
      usage(argv[0]);
    }
  }

  /* Running this test requires a local STUN server on default port */
  sh = stun_handle_init(root,
			STUNTAG_SERVER(argv[1]),
			STUNTAG_REQUIRE_INTEGRITY(sflags & do_secret),
			TAG_NULL());

  if (!sh) {
    SU_DEBUG_0(("%s: %s failed\n", __func__, "stun_handle_init()"));
    return -1;
  }

  s = su_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s == -1) {
    SU_DEBUG_0(("%s: %s  failed: %s\n", __func__,
		"su_socket()", su_gli_strerror(errno)));
    return -1;
  }

  stunc->sc_socket = s;
  stunc->sc_flags = sflags;

  if (sflags & do_randomize_port) {
    su_sockaddr_t sockaddr;
    char ipaddr[SU_ADDRSIZE + 2] = { 0 };
    socklen_t socklen = sizeof(sockaddr);

    srand((unsigned int)time((time_t *)NULL));

    memset(&sockaddr, 0, sizeof(su_sockaddr_t));
    sockaddr.su_port = htons((rand() % (65536 - 1024)) + 1024);
    sockaddr.su_family = AF_INET;

    SU_DEBUG_3(("stunc: Binding to local port %u.\n", ntohs(sockaddr.su_port)));

    err = bind(s, (struct sockaddr *)&sockaddr, socklen);
    if (err < 0) {
      SU_DEBUG_1(("%s: Error %d binding to %s:%u\n", __func__, err,
		  su_inet_ntop(sockaddr.su_family, SU_ADDR(&sockaddr),
			       ipaddr, sizeof(ipaddr)),
		  (unsigned) ntohs(sockaddr.su_port)));
      return -1;
    }

    stunc->sc_flags &= ~do_randomize_port;
  }

  if (sflags & do_secret) {
    if (stun_obtain_shared_secret(sh, stunc_ss_cb, stunc, TAG_NULL()) < 0) {
      SU_DEBUG_3(("%s: %s failed\n", __func__,
		  "stun_handle_request_shared_secret()"));
      return -1;
    }
  }


  /* If we want to bind and no integrity required */
  if ((sflags & do_bind) && !(sflags & do_secret)) {
    err = stun_bind(sh, stunc_bind_cb, stunc,
		    STUNTAG_SOCKET(s),
		    STUNTAG_REGISTER_EVENTS(1),
		    TAG_NULL());

    if (err < 0) {
      SU_DEBUG_0(("%s: %s  failed\n", __func__, "stun_bind()"));
      return -1;
    }
  }

  if (sflags & do_nat_check) {
    err = stun_test_nattype(sh, stunc_nattype_cb, stunc,
			    STUNTAG_REGISTER_EVENTS(1),
			    STUNTAG_SOCKET(stunc->sc_socket),
			    TAG_NULL());

    if (err < 0) {
      SU_DEBUG_0(("%s: %s  failed\n", __func__, "stun_test_nattype()"));
      su_root_break(stun_root(sh));
    }
  }

  if (sflags & do_life_check) {
    err = stun_test_lifetime(sh, stunc_lifetime_cb, stunc,
			     STUNTAG_REGISTER_EVENTS(1),
			     STUNTAG_SOCKET(stunc->sc_socket),
			     TAG_NULL());

    if (err < 0) {
      SU_DEBUG_0(("%s: %s  failed\n", __func__, "stun_test_lifetime()"));
      su_root_break(stun_root(sh));
    }
  }

  if (err == 0)
    su_root_run(root);

  stun_handle_destroy(sh);
  su_root_destroy(root);

  return 0;
}
