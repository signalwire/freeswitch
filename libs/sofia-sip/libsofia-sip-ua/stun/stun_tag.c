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

/**@internal @CFILE stun_tag.c  Tags and tag lists for Offer/Answer Engine
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 * @author Kai Vehmanen <Kai.Vehmanen@nokia.com>
 *
 * @date Created: Wed Aug  3 20:28:17 EEST 2005
 */

#include "config.h"

#define TAG_NAMESPACE "stun"

#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/stun_tag.h>

/**@def STUNTAG_ANY()
 *
 * Filter tag matching any STUNTAG_*().
 */
tag_typedef_t stuntag_any = NSTAG_TYPEDEF(*);

/**@def STUNTAG_DOMAIN(x)
 *
 * The domain to use in DNS-SRV based STUN server discovery.
 * Note: this is commonly the domain part of a public SIP
 * address (AOR). See sect 9.1 of RFC3489.
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_get_params() \n
 *    nua_invite() \n
 *    nua_respond()
 *
 * @par Parameter type
 *    char const *

 */
tag_typedef_t stuntag_domain = STRTAG_TYPEDEF(domain);

/**@def STUNTAG_SERVER(x)
 *
 * Fully qualified host name, or dotted IP address of the STUN server
 * address. If defined, the DNS-SRV based discovery (@see STUNTAG_DOMAIN())
 * will be skipped.
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_get_params() \n
 *    nua_invite() \n
 *    nua_respond()
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *    NULL terminated character string containing a domain name,
 *    IPv4 address, or IPv6 address.
 *
 * Corresponding tag taking reference parameter is STUNTAG_SERVER_REF()
*/
tag_typedef_t stuntag_server = STRTAG_TYPEDEF(server);

/**@def STUNTAG_REQUIRE_INTEGRITY(x)
 *
 * Whether to require support for shared-secret based packet
 * authentication and integrity checks (see sect 9.2 of RFC3489).
 * If false, integrity checks are performed only when server supports it.
 *
 * @par Used with
 *    nua_create() \n
 *
 * @par Parameter type
 *    int (boolean)
 *
 * @par Values
 *    @c !=0 enable
 *    @c 0 disable
 *
 * Corresponding tag taking reference parameter is STUNTAG_INTEGRITY_REF()
 */
tag_typedef_t stuntag_require_integrity = BOOLTAG_TYPEDEF(require_integrity);

/**@def STUNTAG_INTEGRITY(x)
 *
 * XXX: should this tag be deprecated in favor of just supporting
 * STUNTAG_REQURIE_INTEGRITY() instead...?
 */
tag_typedef_t stuntag_integrity = BOOLTAG_TYPEDEF(integrity);

/**@def STUNTAG_SOCKET(x)
 *
 * Bind socket for STUN.
 *
 * @par Used with
 *    stun_handle_bind() \n
 *
 * @par Parameter type
 *    int (su_socket_t)
 *
 * @par Values
 *    IPv4 (AF_INET) socket
 *
 * Corresponding tag taking reference parameter is STUNTAG_SOCKET_REF()
 */
tag_typedef_t stuntag_socket = SOCKETTAG_TYPEDEF(socket);

/**@def STUNTAG_REGISTER_EVENTS(x)
 *
 * Register socket events for eventloop owned by STUN.
 *
 * @par Used with
 *    stun_bind() \n
 *    stun_get_lifetime() \n
 *    stun_get_nattype() \n
 *    stun_keepalive() \n
 *
 * @par Parameter type
 *    bool
 *
 * @par Values
 *    false (0) or true (nonzero)
 *
 * Corresponding tag taking reference parameter is STUNTAG_REGISTER_EVENTS_REF()
 */
tag_typedef_t stuntag_register_events = BOOLTAG_TYPEDEF(register_events);

/**@def STUNTAG_ACTION(x)
 *
 * Command action for STUN request.
 *
 * @par Used with
 *    stun_handle_bind() \n
 *
 * @par Parameter type
 *    int (stun_action_t)
 *
 * @par Values
 *    See types for stun_action_t in <sofia-sip/stun.h>
 *
 * Corresponding tag taking reference parameter is STUNTAG_ACTION_REF()
 */
tag_typedef_t stuntag_action = INTTAG_TYPEDEF(action);

/* ---------------------------------------------------------------------- */

/**@def STUNTAG_CHANGE_IP(x)
 *
 * Add CHANGE-REQUEST attribute with "change IP" flag to the request.
 *
 * @par Used with
 *    stun_make_binding_req() \n
 *
 * @par Parameter type
 *    bool
 *
 * Corresponding tag taking reference parameter is STUNTAG_CHANGE_IP_REF()
 */
tag_typedef_t stuntag_change_ip = BOOLTAG_TYPEDEF(change_ip);

/**@def STUNTAG_CHANGE_PORT(x)
 *
 * Add CHANGE-REQUEST attribute with "change port" flag to the request.
 *
 * @par Used with
 *    stun_make_binding_req() \n
 *
 * @par Parameter type
 *    bool
 *
 * Corresponding tag taking reference parameter is STUNTAG_CHANGE_PORT_REF()
 */
tag_typedef_t stuntag_change_port = BOOLTAG_TYPEDEF(change_port);

/* ---------------------------------------------------------------------- */

/**@def STUNTAG_TIMEOUT(x)
 *
 * Timeout controls the launching of the STUN keepalive timer.
 *
 * @par Used with
 *    stun_keepalive() \n
 *
 * @par Parameter type
 *    int
 *
 * Corresponding tag taking reference parameter is STUNTAG_TIMEOUT_REF()
 */
tag_typedef_t stuntag_timeout = INTTAG_TYPEDEF(timeout);
