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

/**@CFILE tport_tag.c
 * @brief Tags for transport module
 *
 * @note This file is used to automatically generate
 * tport_tag_ref.c and tport_tag_dll.c
 *
 * Copyright (c) 2002 Nokia Research Center.  All rights reserved.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Jun  6 00:38:07 2002 ppessi
 */

#include "config.h"

#include <string.h>
#include <assert.h>

#define TAG_NAMESPACE "tp"

#include "sofia-sip/tport.h"
#include <sofia-sip/su_tag_class.h>

/* ==== Globals ========================================================== */

/** Filter for tport tags. */
tagi_t tport_tags[] = { { tptag_any, 0 }, { TAG_END() } };

tag_typedef_t tptag_any = NSTAG_TYPEDEF(*);

/**@def TPTAG_IDENT(cstr)
 *
 * Identify a transport interface.
 *
 * Use with tport_tbind(), tport_tsend(), nua_create(), nta_agent_create(),
 * nta_agent_add_tport(), nta_outgoing_tcreate(), nta_outgoing_mcreate(),
 * nth_engine_create(), nth_client_tcreate(), or initial nth_site_create().
 */
tag_typedef_t tptag_ident = CSTRTAG_TYPEDEF(ident);

/**@def TPTAG_REUSE(boolean)
 *
 * Allow reusing transport connection (true by default).
 *
 * Use with tport_tcreate(), tport_tbind(), tport_set_params(),
 * tport_tsend(), nua_create(), nta_agent_create(), nta_agent_add_tport(),
 * nta_outgoing_tcreate(), nta_outgoing_mcreate(), nth_engine_create(),
 * nth_client_tcreate(), or initial nth_site_create().
 */
tag_typedef_t tptag_reuse = BOOLTAG_TYPEDEF(reuse);

/**@def TPTAG_FRESH(boolean)
 *
 * Create new connection (but allow other messages to reuse the new one).
 *
 * Use with tport_tsend(), nta_outgoing_tcreate(), nta_outgoing_mcreate(),
 * or nth_client_tcreate().
 */
tag_typedef_t tptag_fresh = BOOLTAG_TYPEDEF(fresh);

/**@def TPTAG_SERVER(boolean)
 *
 * Bind server sockets (true by default, disable with TPTAG_SERVER(0)).
 *
 * Use with tport_tbind().
 */
tag_typedef_t tptag_server = BOOLTAG_TYPEDEF(server);

/**@def TPTAG_PUBLIC(tport_via)
 *
 * Define how the public transport connects to Internet.
 *
 * Use with tport_tbind(), nua_create(), nta_agent_create(),
 * nta_agent_add_tport(), nth_engine_create(), or initial nth_site_create().
 *
 * @sa TPTAG_PUBLIC(), tport_is_public().
 */
tag_typedef_t tptag_public = INTTAG_TYPEDEF(public);

/**@def TPTAG_MTU(usize_t)
 *
 * Specify MTU.
 *
 * Use with tport_tcreate(), tport_tbind(), tport_set_params(),
 * tport_tsend(), nua_create(), nta_agent_create(), nta_agent_add_tport(),
 * nta_outgoing_tcreate(), nta_outgoing_mcreate(), nth_engine_create(),
 * nth_client_tcreate(), or initial nth_site_create().
 */
tag_typedef_t tptag_mtu = USIZETAG_TYPEDEF(mtu);

/**@def TPTAG_CONNECT(x)
 *
 * Specify that tport must always use connections (even with UDP).
 *
 * @note Unimplemented (?).
 *
 * Use with tport_tcreate(), tport_tbind(), tport_set_params(), nua_create(),
 * nta_agent_create(), nta_agent_add_tport(), nth_engine_create(), or
 * initial nth_site_create().
 */
tag_typedef_t tptag_connect = BOOLTAG_TYPEDEF(connect);

/**@def TPTAG_SDWN_ERROR(x)
 *
 * If true, half close of a connection by remote is considered as an error.
 *
 * Use with tport_tcreate(), tport_tbind(), tport_set_params(), nua_create(),
 * nta_agent_create(), nta_agent_add_tport(), nth_engine_create(), or
 * initial nth_site_create().
 */
tag_typedef_t tptag_sdwn_error = BOOLTAG_TYPEDEF(sdwn_error);

/**@def TPTAG_SDWN_AFTER(x)
 *
 * Half-close (shutdown(c, 1)) after sending the message.
 *
 * Use with tport_tsend(), nta_outgoing_tcreate(), nta_outgoing_mcreate(),
 * or nth_client_tcreate().
 */
tag_typedef_t tptag_sdwn_after = BOOLTAG_TYPEDEF(sdwn_after);

/**@def TPTAG_CLOSE_AFTER(x)
 *
 * Close of a connection after sending the message.
 *
 * Use with tport_tsend(), nta_outgoing_tcreate(), nta_outgoing_mcreate(),
 * or nth_client_tcreate().
 */
tag_typedef_t tptag_close_after = BOOLTAG_TYPEDEF(sdwn_after);

/**@def TPTAG_IDLE(x)
 *
 * How long transports may be idle (value in milliseconds).
 *
 * If 0, zap immediately,
 * if UINT_MAX, leave them there (default value for now).
 *
 * @par Use With
 * tport_tcreate(), tport_tbind(), tport_set_params(), nua_create(),
 * nta_agent_create(), nta_agent_add_tport(), nth_engine_create(), or
 * initial nth_site_create().
 */
tag_typedef_t tptag_idle = UINTTAG_TYPEDEF(idle);

/**@def TPTAG_TIMEOUT(x)
 *
 * Timeout for incomplete incoming message  (value in milliseconds).
 *
 * If UINT_MAX, leave the incomplete messages there for ever.
 * Default value for now is UINT_MAX.
 *
 * Use with tport_tcreate(), tport_tbind(), tport_set_params(), nua_create(),
 * nta_agent_create(), nta_agent_add_tport(), nth_engine_create(), or
 * initial nth_site_create().
 */
tag_typedef_t tptag_timeout = UINTTAG_TYPEDEF(timeout);

/**@def TPTAG_SOCKET_KEEPALIVE(x)
 *
 * Keepalive interval set on socket (where supported) in seconds.
 *
 * If 0 or UINT_MAX, do not use keepalives. Default value is 30.
 */
tag_typedef_t tptag_socket_keepalive = UINTTAG_TYPEDEF(socket_keepalive);

/**@def TPTAG_KEEPALIVE(x)
 *
 * Keepalive interval in milliseconds.
 *
 * If 0 or UINT_MAX, do not use keepalives. Default value is 0.
 *
 * On TCP, the keepalive if a CR-LF-CR-LF sequence.
 *
 * Use with tport_tcreate(), tport_tbind(), tport_set_params(), nua_create(),
 * nta_agent_create(), nta_agent_add_tport(), nth_engine_create(), or
 * initial nth_site_create().
 *
 * @sa TPTAG_PINGPONG(), TPTAG_PONG2PING(), TPTAG_TIMEOUT(), TPTAG_IDLE()
 *
 * @NEW_1_12_7.
 */
tag_typedef_t tptag_keepalive = UINTTAG_TYPEDEF(keepalive);

/**@def TPTAG_PINGPONG(x)
 *
 * Ping-pong interval in milliseconds.
 *
 * If 0 or UINT_MAX, do not check for PONGs. Default value is 0.
 *
 * If set, the ping-pong protocol is used on TCP connections. If pinger
 * sends a ping and receives no data in the specified ping-pong interval, it
 * considers the connection failed and closes it. The value recommended in
 * draft-ietf-sip-outbound-10 is 10 seconds (10000 milliseconds).
 *
 * Use with tport_tcreate(), tport_tbind(), tport_set_params(), nua_create(),
 * nta_agent_create(), nta_agent_add_tport(), nth_engine_create(), or
 * initial nth_site_create().
 *
 * @sa TPTAG_PONG2PING(), TPTAG_KEEPALIVE(), TPTAG_TIMEOUT(), TPTAG_IDLE(),
 * draft-ietf-sip-outbound-10.txt
 *
 * @NEW_1_12_7.
 */
tag_typedef_t tptag_pingpong = UINTTAG_TYPEDEF(pingpong);

/**@def TPTAG_PONG2PING(x)
 *
 * Respond PING with PONG.
 *
 * If true, respond with PONG to PING. Default value is 0 (false).
 *
 * If set, the ping-pong protocol is used on TCP connections. If a ping (at
 * least 4 whitespace characters) is received between messages, a pong
 * (CR-LF) is sent in response.
 *
 * Use with tport_tcreate(), tport_tbind(), tport_set_params(), nua_create(),
 * nta_agent_create(), nta_agent_add_tport(), nth_engine_create(), or
 * initial nth_site_create().
 *
 * @sa TPTAG_PINGPONG(), TPTAG_KEEPALIVE(), TPTAG_TIMEOUT(), TPTAG_IDLE()
 *
 * @NEW_1_12_7.
 */
tag_typedef_t tptag_pong2ping = BOOLTAG_TYPEDEF(pong2ping);

/**@def TPTAG_SIGCOMP_LIFETIME(x)
 *
 * Default SigComp lifetime in seconds.
 *
 * If value is UINT_MAX, keep SigComp compartments around for ever.
 *
 * @note Experimental.
 *
 * Use with tport_tcreate(), tport_tbind(), tport_set_params(), nua_create(),
 * nta_agent_create(), nta_agent_add_tport(), nth_engine_create(), or
 * initial nth_site_create().
 */
tag_typedef_t tptag_sigcomp_lifetime = UINTTAG_TYPEDEF(sigcomp_lifetime);

/**@def TPTAG_CERTIFICATE(x)
 *
 * Path to the public key certificate directory.
 *
 * Use with tport_tbind(), nua_create(), nta_agent_create(),
 * nta_agent_add_tport(), nth_engine_create(), or initial nth_site_create().
 */
tag_typedef_t tptag_certificate = STRTAG_TYPEDEF(certificate);

/**@def TPTAG_COMPARTMENT(x)
 *
 * Pointer to SigComp compartment.
 *
 * @note Not used.
 */
tag_typedef_t tptag_compartment = PTRTAG_TYPEDEF(compartment);

/**@def TPTAG_TLS_CIPHERS(x)
 *
 * Sets the supported TLS cipher suites.
 *
 * Use with tport_tbind(), nua_create(), nta_agent_create(),
 * nta_agent_add_tport(), nth_engine_create(), or initial nth_site_create().
 */
tag_typedef_t tptag_tls_ciphers = STRTAG_TYPEDEF(tls_ciphers);

/**@def TPTAG_TLS_VERSION(x)
 *
 * Sets the TLS version (version 0 implies SSL2/SSL3).
 *
 * Use with tport_tbind(), nua_create(), nta_agent_create(),
 * nta_agent_add_tport(), nth_engine_create(), or initial nth_site_create().
 */
tag_typedef_t tptag_tls_version = UINTTAG_TYPEDEF(tls_version);

/**@def TPTAG_TLS_TIMEOUT(x)
 *
 * Sets the maximum TLS session lifetime in seconds.
 *
 * The default value is 300 seconds.
 *
 * Use with tport_tbind(), nua_create(), nta_agent_create(),
 * nta_agent_add_tport(), nth_engine_create(), or initial nth_site_create().
 *
 * @NEW_UNRELEASED.
 */
tag_typedef_t tptag_tls_timeout = UINTTAG_TYPEDEF(tls_timeout);

/**@def TPTAG_TLS_VERIFY_PEER(x)
 * @par Depreciated:
 *    Alias for TPTAG_TLS_VERIFY_POLICY(TPTLS_VERIFY_IN|TPTLS_VERIFY_OUT)
 *
 * @NEW_1_12_10.
 */
tag_typedef_t tptag_tls_verify_peer = UINTTAG_TYPEDEF(tls_verify_peer);

/**@def TPTAG_TLS_PASSPHRASE(x)
 *
 * Sets the passphrase password to be used by openSSL to encrypt/decrypt
 * private key files.
 *
 * @NEW_1_12_11.
 */
tag_typedef_t tptag_tls_passphrase = STRTAG_TYPEDEF(tls_passphrase);


/**@def TPTAG_TLS_VERIFY_POLICY(x)
 *
 * The verification of certificates can be controlled:
 * @par Values:
 *    - #TPTLS_VERIFY_NONE: 
 *          Do not verify Peer Certificates.
 *    - #TPTLS_VERIFY_IN: 
 *          Drop incoming connections which fail signature verification 
 *          against trusted certificate authorities. Peers must provide a 
 *          certificate during the initial TLS Handshake.
 *    - #TPTLS_VERIFY_OUT: 
 *          Drop outgoing connections which fail signature verification 
 *          against trusted certificate authorities.
 *    - #TPTLS_VERIFY_ALL: 
 *          Alias for (TPTLS_VERIFY_IN|TPTLS_VERIFY_OUT)
 *    - #TPTLS_VERIFY_SUBJECTS_IN: 
 *          Match the certificate subject on incoming connections against 
 *          a provided list.  If no match is found, the connection is 
 *          rejected. If no list is provided, subject checking is bypassed.
 *          Note: Implies #TPTLS_VERIFY_IN.
 *    - #TPTLS_VERIFY_SUBJECTS_OUT: 
 *          Match the certificate subject on outgoing connections against 
 *          a provided list.  If no match is found, the connection is 
 *          rejected.
 *          Note: Implies #TPTLS_VERIFY_OUT.
 *    - #TPTLS_VERIFY_SUBJECTS_ALL:
 *          Alias for (TPTLS_VERIFY_SUBJECTS_IN|TPTLS_VERIFY_SUBJECTS_OUT)
 *
 * @par Used with
 *   tport_tbind(), nua_create(), nta_agent_create(), nta_agent_add_tport(),
 *   nth_engine_create(), initial nth_site_create(),
 *   TPTAG_TLS_VERIFY_SUBJECTS(), TPTAG_TLS_VERIFY_DEPTH().
 *
 * @NEW_1_12_11.
 */
tag_typedef_t tptag_tls_verify_policy = UINTTAG_TYPEDEF(tls_verify_policy);

/**@def TPTAG_TLS_VERIFY_DEPTH(x)
 *
 * Define the maximum length of a valid certificate chain.
 * 
 * @par Default
 *   2
 *
 * @par Used with
 *   tport_tbind(), nua_create(), nta_agent_create(), nta_agent_add_tport(), 
 *   nth_engine_create(), or initial nth_site_create().
 *
 * @par Parameter Type:
 *   unsigned int
 *
 * @NEW_1_12_11.
 */
tag_typedef_t tptag_tls_verify_depth = UINTTAG_TYPEDEF(tls_verify_depth);

/**@def TPTAG_TLS_VERIFY_DATE(x)
 *
 * Enable/Disable verification of notBefore and notAfter parameters of
 * X.509 Certificates.
 *
 * @par Default
 *   Enabled
 *
 * @par Values
 *   - 0 - Disable date verification.
 *   - Non-Zero - Enable date verification.
 *
 * @par Used with
 *   tport_tbind(), nua_create(), nta_agent_create(), nta_agent_add_tport(), 
 *   nth_engine_create(), or initial nth_site_create().
 *
 * @par Parameter Type:
 *   unsigned int
 *
 * @par Note
 *   This tag should be only used on devices which lack accurate timekeeping.
 *
 * @NEW_1_12_11.
 */
tag_typedef_t tptag_tls_verify_date = UINTTAG_TYPEDEF(tls_verify_date);

/**@def TPTAG_TLS_VERIFY_SUBJECTS(x)
 *
 * Incoming TLS connections must provide a trusted X.509 certificate.
 * The character strings provided with this tag are matched against
 * the subjects from the trusted certificate.  If a match is not found,
 * the connection is automatically rejected.
 *
 * @par Used with
 *   tport_tbind(), nua_create(), nta_agent_create(), nta_agent_add_tport(), 
 *   nth_engine_create(), initial nth_site_create(),
 *   TPTLS_VERIFY_SUBJECTS_IN
 *
 * @par Parameter Type:
 *   void const * (actually su_strlst_t const *)
 *
 * @par Values
 *   - SIP Identity - sip:example.com or sip:username@example.com
 *   - DNS - sip.example.com
 *   - IP Address - Both IPv4 and IPv6 Supported
 *
 * @NEW_1_12_11.
 */
tag_typedef_t tptag_tls_verify_subjects = PTRTAG_TYPEDEF(tls_verify_subjects);

#if 0
/**@def TPTAG_X509_SUBJECT(x)
 *
 * Requires that a message be sent over a TLS transport with trusted X.509
 * certificate.  The character string provided must match against a subject 
 * from the trusted certificate.
 *
 * @par Used with
 *   tport_tsend(), TPTLS_VERIFY_SUBJECTS_OUT
 *
 * @par Parameter Type:
 *   char const *
 *
 * @par Values
 *   - Refer to TPTAG_TLS_VERIFY_SUBJECTS()
 *
 * @note Not Implemented.
 */
#endif

/**@def TPTAG_QUEUESIZE(x)
 *
 * Specify the number of messages that can be queued per connection.
 *
 * Use with tport_tcreate(), tport_tbind(), tport_set_params(), nua_create(),
 * nta_agent_create(), nta_agent_add_tport(), nth_engine_create(), or
 * initial nth_site_create().
 */
tag_typedef_t tptag_queuesize = UINTTAG_TYPEDEF(queuesize);

/**@def TPTAG_DEBUG_DROP(x)
 *
 * Sets the drop propability for incoming/outgoing packets.
 *
 * The incoming/outgoing packets are dropped with the given probablity
 * (in the range 0..1000) on unreliable transports.
 *
 * This is a parameter suitable for debugging only.
 *
 * Use with tport_tcreate(), tport_tbind(), tport_set_params(), nua_create(),
 * nta_agent_create(), nta_agent_add_tport(), nth_engine_create(), or
 * initial nth_site_create().
 */
tag_typedef_t tptag_debug_drop = UINTTAG_TYPEDEF(debug_drop);

/**@def TPTAG_UDP_RMEM(x)
 *
 * Sets the maximum receive buffer in bytes for primary UDP socket.
 *
 * This is a parameter suitable for tuning.
 *
 * On Linux systems, the default value for receive buffer is set with
 * the sysctl "net.core.rmem_default", and the maximum value is set with
 * the sysctl "net.core.rmem_max".
 *
 * Use with tport_tbind(), nua_create(), nta_agent_create(),
 * nta_agent_add_tport(), nth_engine_create(), or initial nth_site_create().
 */
tag_typedef_t tptag_udp_rmem = UINTTAG_TYPEDEF(udp_rmem);

/**@def TPTAG_UDP_WMEM(x)
 *
 * Sets the maximum send buffer in bytes for primary UDP socket.
 *
 * This is a parameter suitable for tuning.
 *
 * On Linux systems, the default value for receive buffer is set with
 * the sysctl "net.core.wmem_default", and the maximum value is set with
 * the sysctl "net.core.wmem_max".
 *
 * Use with tport_tbind(), nua_create(), nta_agent_create(),
 * nta_agent_add_tport(), nth_engine_create(), or initial nth_site_create().
 */
tag_typedef_t tptag_udp_wmem = UINTTAG_TYPEDEF(udp_wmem);

/**@def TPTAG_THRPSIZE(x)
 *
 * Determines the number of threads in the pool.
 *
 * The thread pools can have multiple threads receiving, uncompressing,
 * parsing, compressing, and sending messages.
 *
 * This is a parameter suitable for tuning.
 *
 * @note Thread pools are currently broken.
 *
 * Use with tport_tcreate(), tport_tbind(), tport_set_params(), nua_create(),
 * nta_agent_create(), nta_agent_add_tport(), nth_engine_create(), or
 * initial nth_site_create().
 */
tag_typedef_t tptag_thrpsize = UINTTAG_TYPEDEF(thrpsize);

/**@def TPTAG_THRPRQSIZE(x)
 *
 * Length of per-thread receive queue (as messages).
 *
 * This is a parameter suitable for tuning.
 *
 * Use with tport_tcreate(), tport_tbind(), tport_set_params(), nua_create(),
 * nta_agent_create(), nta_agent_add_tport(), nth_engine_create(), or
 * initial nth_site_create().
 */
tag_typedef_t tptag_thrprqsize = UINTTAG_TYPEDEF(thrprqsize);

/**@def TPTAG_HTTP_CONNECT(x)
 *
 * Specify that tport can use HTTP connect method.
 *
 * Use with tport_tbind(), nua_create(), nta_agent_create(),
 * nta_agent_add_tport(), nth_engine_create(), or initial nth_site_create().
 */
tag_typedef_t tptag_http_connect = STRTAG_TYPEDEF(http_connect);

/**@def TPTAG_STUN_SERVER(x)
 *
 * Enable STUN server.
 *
 * Use with tport_tcreate(), tport_tbind(), tport_set_params(), nua_create(),
 * nta_agent_create(), nta_agent_add_tport(), nth_engine_create(), or
 * initial nth_site_create().
 */
tag_typedef_t tptag_stun_server = BOOLTAG_TYPEDEF(stun_server);

/**@def TPTAG_TOS(x)
 *
 * Sets the IP TOS for the socket.
 *
 * Use with tport_tcreate(), tport_tbind(), tport_set_params(), nua_create(),
 * nta_agent_create(), nta_agent_add_tport(), nth_engine_create(), or
 * initial nth_site_create().
 *
 * @NEW_1_12_5.
 */
tag_typedef_t tptag_tos = INTTAG_TYPEDEF(tos);

/**@def TPTAG_LOG(x)
 *
 * If set, print out parsed or sent messages at transport layer.
 *
 * Use with tport_tcreate(), nua_create(), nta_agent_create(),
 * nth_engine_create(), or initial nth_site_create().
 *
 * @sa #TPORT_LOG environment variable, TPTAG_DUMP()
 *
 * @NEW_1_12_5.
 */
tag_typedef_t tptag_log = INTTAG_TYPEDEF(log);

/**@def TPTAG_DUMP(x)
 *
 * Filename for dumping unparsed messages from transport.
 *
 * Use with tport_tcreate(), nta_agent_create(), nua_create(),
 * nth_engine_create(), or initial nth_site_create().
 *
 * @sa #TPORT_DUMP environment variable, TPTAG_LOG().
 *
 * @NEW_1_12_5.
 */
tag_typedef_t tptag_dump = STRTAG_TYPEDEF(dump);

/**@def TPTAG_CAPT(x)
 *
 * URL for capturing unparsed messages from transport.
 *
 * Use with tport_tcreate(), nta_agent_create(), nua_create(),
 * nth_engine_create(), or initial nth_site_create().
 *
 * @sa #TPORT_CAPT environment variable, TPTAG_LOG().
 *
 */
tag_typedef_t tptag_capt = STRTAG_TYPEDEF(capt);


/** Mark transport as trusted.
 *
 * @note Not implemented by tport module.
 *
 * Use with tport_tcreate(), tport_tbind(), tport_set_params(), nua_create(),
 * nta_agent_create(), nta_agent_add_tport(), nth_engine_create(), or
 * initial nth_site_create().
 */
tag_typedef_t tptag_trusted = BOOLTAG_TYPEDEF(trusted);
