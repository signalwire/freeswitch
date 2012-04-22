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

/**@CFILE features.c
 * Provide features available through the sofia-sip library.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Mon Oct 24 14:51:32 2005 ppessi
 */

#include "config.h"

#include <stddef.h>

#include <sofia-sip/su_configure.h>
#include "tport_tls.h"
#include "sofia-sip/sofia_features.h"

/** The name and version of software package providing Sofia-SIP-UA library.
 * @showinitializer
 */
char const * const sofia_sip_name_version = SOFIA_SIP_NAME_VERSION;

/** The name and version of software package providing S/MIME functionality,
 *  NULL if none.
 */
char const * sofia_sip_has_smime;


/** The name and version of software package providing TLS functionality,
 *  NULL if none.
 *
 * TLS over TCP is used as transport for SIP messages when using SIPS
 * scheme. Using TLS over TCP with SIP is described in @RFC3261.
 */
#if HAVE_OPENSSL
char const * sofia_sip_has_tls = tls_version;
#else
char const * sofia_sip_has_tls;
#endif

/** The name and version of software package providing DTLS functionality,
 *  NULL if none.
 *
 * DTLS or TLS over datagram transport (UDP) can be used as transport for
 * SIP messages.
 */
char const * sofia_sip_has_dtls;

/** The name and version of software package providing TLS over SCTP functionality,
 *  NULL if none.
 *
 * TLS over SCTP can be used as transport for SIP messages.
 */
char const * sofia_sip_has_tls_sctp;

#if HAVE_SOFIA_SIGCOMP
#include <sigcomp.h>
#endif

/** The name and version of software package providing SigComp functionality,
 *  NULL if none.
 *
 * SigComp can be used to compress SIP messages.
 */
#if HAVE_SOFIA_SIGCOMP
char const * sofia_sip_has_sigcomp = sigcomp_package_version;
#else
char const * sofia_sip_has_sigcomp;
#endif

/** The name and version of software package providing STUN functionality,
 *  NULL if none.
 *
 * STUN is a protocol used to traverse NATs with UDP.
 */
#if HAVE_SOFIA_STUN
extern char const stun_version[];
char const * sofia_sip_has_stun = stun_version;
#else
char const * sofia_sip_has_stun;
#endif

/** The name and version of software package providing TURN functionality,
 *  NULL if none.
 *
 * TURN is a protocol used to traverse NATs or firewalls with TCP or UDP.
 */
char const * sofia_sip_has_turn;

/** The name and version of software package providing UPnP functionality,
 *  NULL if none.
 *
 * UPnP (Universal Plug and Play) can be used to traverse NATs or firewalls.
 */
char const * sofia_sip_has_upnp;

/** The name and version of software package providing SCTP functionality,
 *  NULL if none.
 *
 * SCTP can be used as transport for SIP messages. The software providing it
 * can be, for example, LKSCTP (Linux kernel SCTP) for Linux.
 */
char const * sofia_sip_has_sctp;
/* We don't have viable SCTP transport interface */

/** The name and version of software package providing IPv6 functionality,
 *  NULL if none.
 *
 * IPv6 can be used to send SIP messages.
 */
#if SU_HAVE_IN6
char const * sofia_sip_has_ipv6 = "IPv6";
#else
char const * sofia_sip_has_ipv6;
#endif
