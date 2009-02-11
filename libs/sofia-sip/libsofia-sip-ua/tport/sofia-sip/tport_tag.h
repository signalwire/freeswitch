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
#define TPTAG_IDENT(x) tptag_ident, tag_str_v((x))

TPORT_DLL extern tag_typedef_t tptag_ident_ref;
#define TPTAG_IDENT_REF(x) tptag_ident_ref, tag_str_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_reuse;
#define TPTAG_REUSE(x) tptag_reuse, tag_bool_v((x))

TPORT_DLL extern tag_typedef_t tptag_reuse_ref;
#define TPTAG_REUSE_REF(x) tptag_reuse_ref, tag_bool_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_fresh;
#define TPTAG_FRESH(x) tptag_fresh, tag_bool_v((x))

TPORT_DLL extern tag_typedef_t tptag_fresh_ref;
#define TPTAG_FRESH_REF(x) tptag_fresh_ref, tag_bool_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_server;
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
#define TPTAG_PUBLIC(x) tptag_public, tag_int_v((x))

TPORT_DLL extern tag_typedef_t tptag_public_ref;
#define TPTAG_PUBLIC_REF(x) tptag_public_ref, tag_int_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_mtu;
#define TPTAG_MTU(x) tptag_mtu, tag_usize_v((x))

TPORT_DLL extern tag_typedef_t tptag_mtu_ref;
#define TPTAG_MTU_REF(x) tptag_mtu_ref, tag_usize_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_connect;
#define TPTAG_CONNECT(x) tptag_connect, tag_bool_v((x))

TPORT_DLL extern tag_typedef_t tptag_connect_ref;
#define TPTAG_CONNECT_REF(x) tptag_connect_ref, tag_bool_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_queuesize;
#define TPTAG_QUEUESIZE(x) tptag_queuesize, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_queuesize_ref;
#define TPTAG_QUEUESIZE_REF(x) tptag_queuesize_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_sdwn_error;
#define TPTAG_SDWN_ERROR(x) tptag_sdwn_error, tag_bool_v((x))

TPORT_DLL extern tag_typedef_t tptag_sdwn_error_ref;
#define TPTAG_SDWN_ERROR_REF(x) tptag_sdwn_error_ref, tag_bool_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_sdwn_after;
#define TPTAG_SDWN_AFTER(x) tptag_sdwn_after, tag_bool_v((x))

TPORT_DLL extern tag_typedef_t tptag_sdwn_after_ref;
#define TPTAG_SDWN_AFTER_REF(x) tptag_sdwn_after_ref, tag_bool_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_close_after;
#define TPTAG_CLOSE_AFTER(x) tptag_close_after, tag_bool_v((x))

TPORT_DLL extern tag_typedef_t tptag_close_after_ref;
#define TPTAG_CLOSE_AFTER_REF(x) tptag_close_after_ref, tag_bool_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_idle;
#define TPTAG_IDLE(x) tptag_idle, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_idle_ref;
#define TPTAG_IDLE_REF(x) tptag_idle_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_timeout;
#define TPTAG_TIMEOUT(x) tptag_timeout, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_timeout_ref;
#define TPTAG_TIMEOUT_REF(x) tptag_timeout_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_keepalive;
#define TPTAG_KEEPALIVE(x) tptag_keepalive, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_keepalive_ref;
#define TPTAG_KEEPALIVE_REF(x) tptag_keepalive_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_pingpong;
#define TPTAG_PINGPONG(x) tptag_pingpong, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_pingpong_ref;
#define TPTAG_PINGPONG_REF(x) tptag_pingpong_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_pong2ping;
#define TPTAG_PONG2PING(x) tptag_pong2ping, tag_bool_v((x))

TPORT_DLL extern tag_typedef_t tptag_pong2ping_ref;
#define TPTAG_PONG2PING_REF(x) tptag_pong2ping_ref, tag_bool_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_sigcomp_lifetime;
#define TPTAG_SIGCOMP_LIFETIME(x) tptag_sigcomp_lifetime, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_sigcomp_lifetime_ref;
#define TPTAG_SIGCOMP_LIFETIME_REF(x) \
tptag_sigcomp_lifetime_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_compartment;
#define TPTAG_COMPARTMENT(x) tptag_compartment, tag_ptr_v((x))

TPORT_DLL extern tag_typedef_t tptag_compartment_ref;
#define TPTAG_COMPARTMENT_REF(x) \
  tptag_compartment_ref, tag_ptr_vr(&(x), x)

TPORT_DLL extern tag_typedef_t tptag_certificate;
#define TPTAG_CERTIFICATE(x) tptag_certificate, tag_str_v((x))

TPORT_DLL extern tag_typedef_t tptag_certificate_ref;
#define TPTAG_CERTIFICATE_REF(x) tptag_certificate_ref, tag_str_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_tls_version;
#define TPTAG_TLS_VERSION(x) tptag_tls_version, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_tls_version_ref;
#define TPTAG_TLS_VERSION_REF(x) tptag_tls_version_ref, tag_uint_vr(&(x))

enum tport_tls_verify_policy {
  TPTLS_VERIFY_NONE         = 0x0,
  TPTLS_VERIFY_INCOMING     = 0x1,
  TPTLS_VERIFY_IN           = 0x1,
  TPTLS_VERIFY_OUTGOING     = 0x2,
  TPTLS_VERIFY_OUT          = 0x2,
  TPTLS_VERIFY_ALL          = 0x3,
  TPTLS_VERIFY_SUBJECTS_IN  = 0x5, /* 0x4 | TPTLS_VERIFY_INCOMING */
  TPTLS_VERIFY_SUBJECTS_OUT = 0xA, /* 0x8 | TPTLS_VERIFY_OUTGOING */
  TPTLS_VERIFY_SUBJECTS_ALL = 0xF,
};

TPORT_DLL extern tag_typedef_t tptag_tls_verify_policy;
#define TPTAG_TLS_VERIFY_POLICY(x) tptag_tls_verify_policy, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_tls_verify_policy_ref;
#define TPTAG_TLS_VERIFY_POLICY_REF(x) tptag_tls_verify_policy_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_tls_verify_depth;
#define TPTAG_TLS_VERIFY_DEPTH(x) tptag_tls_verify_depth, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_tls_verify_depth_ref;
#define TPTAG_TLS_VERIFY_DEPTH_REF(x) \
             tptag_tls_verify_depth_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_tls_verify_date;
#define TPTAG_TLS_VERIFY_DATE(x) tptag_tls_verify_date, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_tls_verify_date_ref;
#define TPTAG_TLS_VERIFY_DATE_REF(x) \
             tptag_tls_verify_date_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_tls_verify_subjects;
#define TPTAG_TLS_VERIFY_SUBJECTS(x) tptag_tls_verify_subjects, tag_cptr_v((x))

TPORT_DLL extern tag_typedef_t tptag_tls_verify_subjects_ref;
#define TPTAG_TLS_VERIFY_SUBJECTS_REF(x) \
             tptag_tls_verify_subjects_ref, tag_cptr_vr(&(x), (x))

/* TPTAG_TLS_VERIFY_PEER is depreciated - Use TPTAG_TLS_VERIFY_POLICY */
TPORT_DLL extern tag_typedef_t tptag_tls_verify_peer;
#define TPTAG_TLS_VERIFY_PEER(x) TPTAG_TLS_VERIFY_POLICY( (x) ? \
           TPTLS_VERIFY_ALL : TPTLS_VERIFY_NONE)

TPORT_DLL extern tag_typedef_t tptag_tls_verify_peer_ref;
#define TPTAG_TLS_VERIFY_PEER_REF(x) tptag_tls_verify_peer_ref, tag_uint_vr(&(x))

#if 0
TPORT_DLL extern tag_typedef_t tport_x509_subject;
#define TPTAG_X509_SUBJECT(x) tptag_x509_subject, tag_str_v((x))

TPORT_DLL extern tag_typedef_t tptag_x509_subject_ref;
#define TPTAG_X509_SUBJECT_REF(x) tptag_x509_subject_ref, tag_str_vr(&(x))
#endif

TPORT_DLL extern tag_typedef_t tptag_debug_drop;
#define TPTAG_DEBUG_DROP(x) tptag_debug_drop, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_debug_drop_ref;
#define TPTAG_DEBUG_DROP_REF(x) tptag_debug_drop_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_udp_rmem;
#define TPTAG_UDP_RMEM(x) tptag_udp_rmem, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_udp_rmem_ref;
#define TPTAG_UDP_RMEM_REF(x) tptag_udp_rmem_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_udp_wmem;
#define TPTAG_UDP_WMEM(x) tptag_udp_wmem, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_udp_wmem_ref;
#define TPTAG_UDP_WMEM_REF(x) tptag_udp_wmem_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_thrpsize;
#define TPTAG_THRPSIZE(x) tptag_thrpsize, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_thrpsize_ref;
#define TPTAG_THRPSIZE_REF(x) tptag_thrpsize_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_thrprqsize;
#define TPTAG_THRPRQSIZE(x) tptag_thrprqsize, tag_uint_v((x))

TPORT_DLL extern tag_typedef_t tptag_thrprqsize_ref;
#define TPTAG_THRPRQSIZE_REF(x) tptag_thrprqsize_ref, tag_uint_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_http_connect;
#define TPTAG_HTTP_CONNECT(x) tptag_http_connect, tag_str_v((x))

TPORT_DLL extern tag_typedef_t tptag_http_connect_ref;
#define TPTAG_HTTP_CONNECT_REF(x) tptag_http_connect_ref, tag_str_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_stun_server;
#define TPTAG_STUN_SERVER(x) tptag_stun_server, tag_bool_v((x))

TPORT_DLL extern tag_typedef_t tptag_stun_server_ref;
#define TPTAG_STUN_SERVER_REF(x) tptag_stun_server_ref, tag_bool_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_tos;
#define TPTAG_TOS(x) tptag_tos, tag_int_v((x))

TPORT_DLL extern tag_typedef_t tptag_tos_ref;
#define TPTAG_TOS_REF(x) tptag_tos_ref, tag_int_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_log;
#define TPTAG_LOG(x) tptag_log, tag_bool_v((x))

TPORT_DLL extern tag_typedef_t tptag_log_ref;
#define TPTAG_LOG_REF(x) tptag_log_ref, tag_bool_vr(&(x))

TPORT_DLL extern tag_typedef_t tptag_dump;
#define TPTAG_DUMP(x) tptag_dump, tag_str_v((x))

TPORT_DLL extern tag_typedef_t tptag_dump_ref;
#define TPTAG_DUMP_REF(x) tptag_dump_ref, tag_str_vr(&(x))

SOFIA_END_DECLS

#endif /* !defined TPORT_TAG_H */
