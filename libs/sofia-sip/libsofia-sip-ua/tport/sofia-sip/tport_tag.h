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

#ifndef TPORT_TAG_H
/** Defined when <sofia-sip/tport_tag.h> has been included. */
#define TPORT_TAG_H

/**@file sofia-sip/tport_tag.h
 * @brief Tags for tport module.
 * 
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *  
 * @date Created: Sat Oct 12 18:39:48 2002 ppessi
 */

#ifndef SU_TAG_H
#include <sofia-sip/su_tag.h>
#endif

SOFIA_BEGIN_DECLS

/** List of all tport tags. */
TPORT_DLL extern tagi_t tport_tag_list[];

/** Filter list matching any tport tag. */
TPORT_DLL extern tagi_t tport_tags[];

/** Filter tag matching any tport tag. */
#define TPTAG_ANY()         tptag_any, ((tag_value_t)0)
TPORT_DLL extern tag_typedef_t tptag_any;

TPORT_DLL extern tag_typedef_t tptag_ident;
/** Ident transport connection (true by default). */
#define TPTAG_IDENT(x) tptag_ident, tag_str_v((x))

TPORT_DLL extern tag_typedef_t tptag_ident_ref;
#define TPTAG_IDENT_REF(x) tptag_ident_ref, tag_str_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_reuse;
/** Allow reusing transport connection (true by default). */
#define TPTAG_REUSE(x) tptag_reuse, tag_bool_v((x))

TPORT_DLL extern tag_typedef_t tptag_reuse_ref;
#define TPTAG_REUSE_REF(x) tptag_reuse_ref, tag_bool_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_fresh;
/** Create new connection (but allow reusing new one). */
#define TPTAG_FRESH(x) tptag_fresh, tag_bool_v((x))

TPORT_DLL extern tag_typedef_t tptag_fresh_ref;
#define TPTAG_FRESH_REF(x) tptag_fresh_ref, tag_bool_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_server;
/** Bind server sockets (true by default, disable with TPTAG_SERVER(0)). */
#define TPTAG_SERVER(x) tptag_server, tag_bool_v((x))

TPORT_DLL extern tag_typedef_t tptag_server_ref;
#define TPTAG_SERVER_REF(x) tptag_server_ref, tag_bool_vr(&(x))

/** Define how the public transport connects to Internet. 
 *
 * @sa TPTAG_PUBLIC(), tport_is_public().
 */
typedef enum tport_via {
  tport_type_local = 0,
  tport_type_server = 0,
  tport_type_client = 1,
  tport_type_stun = 2,
  tport_type_upnp = 3,
  tport_type_connect = 4,
  tport_type_socks = 5,
} tport_pri_type_t;

TPORT_DLL extern tag_typedef_t tptag_public;
/** Use a transport reaching to public Internet. */
#define TPTAG_PUBLIC(x) tptag_public, tag_int_v((x))

TPORT_DLL extern tag_typedef_t tptag_public_ref;
#define TPTAG_PUBLIC_REF(x) tptag_public_ref, tag_int_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_mtu;
/** Specify MTU. */
#define TPTAG_MTU(x) tptag_mtu, tag_usize_v((x))

TPORT_DLL extern tag_typedef_t tptag_mtu_ref;
#define TPTAG_MTU_REF(x) tptag_mtu_ref, tag_usize_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_connect;
/** Specify that tport must always use connections. */
#define TPTAG_CONNECT(x) tptag_connect, tag_bool_v((x))

TPORT_DLL extern tag_typedef_t tptag_connect_ref;
#define TPTAG_CONNECT_REF(x) tptag_connect_ref, tag_bool_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_queuesize;
/** Specify the number of messages that can be queued per connection. */
#define TPTAG_QUEUESIZE(x) tptag_queuesize, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_queuesize_ref;
#define TPTAG_QUEUESIZE_REF(x) tptag_queuesize_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_sdwn_error;
/** If true, half close of a connection by remote is considered as an error. */
#define TPTAG_SDWN_ERROR(x) tptag_sdwn_error, tag_bool_v((x))

TPORT_DLL extern tag_typedef_t tptag_sdwn_error_ref;
#define TPTAG_SDWN_ERROR_REF(x) tptag_sdwn_error_ref, tag_bool_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_sdwn_after;
/** Half-close (shutdown(c, 1) after sending the message. */
#define TPTAG_SDWN_AFTER(x) tptag_sdwn_after, tag_bool_v((x))

TPORT_DLL extern tag_typedef_t tptag_sdwn_after_ref;
#define TPTAG_SDWN_AFTER_REF(x) tptag_sdwn_after_ref, tag_bool_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_close_after;
/** Close of a connection after sending the message. */
#define TPTAG_CLOSE_AFTER(x) tptag_close_after, tag_bool_v((x))

TPORT_DLL extern tag_typedef_t tptag_close_after_ref;
#define TPTAG_CLOSE_AFTER_REF(x) tptag_close_after_ref, tag_bool_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_idle;
/** How long transports may be idle (value in milliseconds). 
 * If 0, zap immediately, 
 * if UINT_MAX, leave them there (default value for now).
 */
#define TPTAG_IDLE(x) tptag_idle, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_idle_ref;
#define TPTAG_IDLE_REF(x) tptag_idle_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_timeout;
/**Timeout for incomplete incoming message  (value in milliseconds).
 *
 * If UINT_MAX, leave the incomplete messages there for ever.
 * Default value for now is UINT_MAX.
 */
#define TPTAG_TIMEOUT(x) tptag_timeout, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_timeout_ref;
#define TPTAG_TIMEOUT_REF(x) tptag_timeout_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_sigcomp_lifetime;
/**Default SigComp lifetime.
 *
 * If UINT_MAX, keep SigComp compartments around for ever.
 *
 * @note Experimental.
 */
#define TPTAG_SIGCOMP_LIFETIME(x) tptag_sigcomp_lifetime, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_sigcomp_lifetime_ref;
#define TPTAG_SIGCOMP_LIFETIME_REF(x) \
tptag_sigcomp_lifetime_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_compartment;
/** Pointer to SigComp compartment. */
#define TPTAG_COMPARTMENT(x) tptag_compartment, tag_ptr_v((x))

TPORT_DLL extern tag_typedef_t tptag_compartment_ref;
#define TPTAG_COMPARTMENT_REF(x) \
  tptag_compartment_ref, tag_ptr_vr(&(x), x)

TPORT_DLL extern tag_typedef_t tptag_certificate;
/** Path to the public key certificate directory.
 */
#define TPTAG_CERTIFICATE(x) tptag_certificate, tag_str_v((x))

TPORT_DLL extern tag_typedef_t tptag_certificate_ref;
#define TPTAG_CERTIFICATE_REF(x) tptag_certificate_ref, tag_str_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_tls_version;
/** Sets the TLS version (version 0 implies SSL2/SSL3).
 */
#define TPTAG_TLS_VERSION(x) tptag_tls_version, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_tls_version_ref;
#define TPTAG_TLS_VERSION_REF(x) tptag_tls_version_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_trusted;
/** Mark transport as trusted. */
#define TPTAG_TRUSTED(x) tptag_trusted, tag_bool_v((x))

TPORT_DLL extern tag_typedef_t tptag_trusted_ref;
#define TPTAG_TRUSTED_REF(x) tptag_trusted_ref, tag_bool_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_debug_drop;
/** Sets the drop propability for (0..1000) incoming/outgoing packets. */
#define TPTAG_DEBUG_DROP(x) tptag_debug_drop, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_debug_drop_ref;
#define TPTAG_DEBUG_DROP_REF(x) tptag_debug_drop_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_udp_rmem;
/** Sets the maximum receive buffer in bytes for primary UDP socket. */
#define TPTAG_UDP_RMEM(x) tptag_udp_rmem, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_udp_rmem_ref;
#define TPTAG_UDP_RMEM_REF(x) tptag_udp_rmem_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_udp_wmem;
/** Sets the maximum send buffer in bytes for primary UDP socket. */
#define TPTAG_UDP_WMEM(x) tptag_udp_wmem, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_udp_wmem_ref;
#define TPTAG_UDP_WMEM_REF(x) tptag_udp_wmem_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_thrpsize;
/** Determines the number of threads in the pool receiving, uncompressing,
 * parsing, compressing, and sending messages.
 */
#define TPTAG_THRPSIZE(x) tptag_thrpsize, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_thrpsize_ref;
#define TPTAG_THRPSIZE_REF(x) tptag_thrpsize_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_thrprqsize;
/** Length of per-thread receive queue (as messages)
 */
#define TPTAG_THRPRQSIZE(x) tptag_thrprqsize, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_thrprqsize_ref;
#define TPTAG_THRPRQSIZE_REF(x) tptag_thrprqsize_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_http_connect;
/** Specify that tport can use HTTP connect method. */
#define TPTAG_HTTP_CONNECT(x) tptag_http_connect, tag_str_v((x))

TPORT_DLL extern tag_typedef_t tptag_http_connect_ref;
#define TPTAG_HTTP_CONNECT_REF(x) tptag_http_connect_ref, tag_str_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_stun_server;
/** Enable STUN server. */
#define TPTAG_STUN_SERVER(x) tptag_stun_server, tag_bool_v((x))

TPORT_DLL extern tag_typedef_t tptag_stun_server_ref;
#define TPTAG_STUN_SERVER_REF(x) tptag_stun_server_ref, tag_bool_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_tos;
/** Sets the IP TOS for the socket. */
#define TPTAG_TOS(x) tptag_tos, tag_int_v((x))

TPORT_DLL extern tag_typedef_t tptag_tos_ref;
#define TPTAG_TOS_REF(x) tptag_tos_ref, tag_int_vr(&(x))

SOFIA_END_DECLS

#endif /* !defined TPORT_TAG_H */
