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

#ifndef STUN_TAG_H
#define STUN_TAG_H
/**@file sofia-sip/stun_tag.h  Tags for STUN.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 * @author Kai Vehmanen <Kai.Vehmanen@nokia.com>
 *
 * @date Created: Tue Oct 18 20:13:50 EEST 2005 ppessi
 */

#ifndef SU_TAG_H
#include <sofia-sip/su_tag.h>
#endif
#ifndef SU_TAG_IO_H
#include <sofia-sip/su_tag_io.h>
#endif

SOFIA_BEGIN_DECLS

/*****************************************
 * Note: see documentation in stun_tag.c *
 *****************************************/

#define STUNTAG_ANY()         stuntag_any, ((tag_value_t)0)
SOFIAPUBVAR tag_typedef_t stuntag_any;

#define STUNTAG_DOMAIN(x)  stuntag_domain, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t stuntag_domain;
#define STUNTAG_DOMAIN_REF(x) stuntag_domain_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t stuntag_domain_ref;

#define STUNTAG_SERVER(x)  stuntag_server, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t stuntag_server;
#define STUNTAG_SERVER_REF(x) stuntag_server_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t stuntag_server_ref;

#define STUNTAG_REQUIRE_INTEGRITY(x) stuntag_require_integrity, tag_int_v(x)
SOFIAPUBVAR tag_typedef_t stuntag_require_integrity;
#define STUNTAG_REQUIRE_INTEGRITY_REF(x) stuntag_require_integrity_ref, tag_int_vr(&(x))
SOFIAPUBVAR tag_typedef_t stuntag_require_integrity_ref;

#define STUNTAG_INTEGRITY(x) stuntag_integrity, tag_int_v(x)
SOFIAPUBVAR tag_typedef_t stuntag_integrity;
#define STUNTAG_INTEGRITY_REF(x) stuntag_integrity_ref, tag_int_vr(&(x))
SOFIAPUBVAR tag_typedef_t stuntag_integrity_ref;

#define STUNTAG_SOCKET(x) stuntag_socket, tag_socket_v(x)
SOFIAPUBVAR tag_typedef_t stuntag_socket;
#define STUNTAG_SOCKET_REF(x) stuntag_socket_ref, tag_socket_vr(&(x))
SOFIAPUBVAR tag_typedef_t stuntag_socket_ref;

#define STUNTAG_REGISTER_EVENTS(x) stuntag_register_events, tag_int_v(x)
SOFIAPUBVAR tag_typedef_t stuntag_register_events;
#define STUNTAG_REGISTER_EVENTS_REF(x) stuntag_register_events_ref, tag_int_vr(&(x))
SOFIAPUBVAR tag_typedef_t stuntag_register_events_ref;

#define STUNTAG_ACTION(x) stuntag_action, tag_int_v(x)
SOFIAPUBVAR tag_typedef_t stuntag_action;
#define STUNTAG_ACTION_REF(x) stuntag_action_ref, tag_int_vr(&(x))
SOFIAPUBVAR tag_typedef_t stuntag_action_ref;

#define STUNTAG_CHANGE_IP(x) stuntag_change_ip, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t stuntag_change_ip;
#define STUNTAG_CHANGE_IP_REF(x) stuntag_change_ip_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t stuntag_change_ip_ref;

#define STUNTAG_CHANGE_PORT(x) stuntag_change_port, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t stuntag_change_port;
#define STUNTAG_CHANGE_PORT_REF(x) stuntag_change_port_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t stuntag_change_port_ref;

#define STUNTAG_TIMEOUT(x) stuntag_timeout, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t stuntag_timeout;
#define STUNTAG_TIMEOUT_REF(x) stuntag_timeout_ref, tag_uint_vr(&(x))
SOFIAPUBVAR tag_typedef_t stuntag_timeout_ref;


SOFIA_END_DECLS

#endif /* STUN_TAG_H */
