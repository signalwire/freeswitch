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

#ifndef STUN_H
/** Defined when <sofia-sip/stun.h> has been included. */
#define STUN_H

/**@file sofia-sip/stun.h STUN module public interface
 *
 * @author Martti Mela <Martti.Mela@nokia.com>
 * @author Tat Chan <Tat.Chan@nokia.com>
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <kai.vehmanen@nokia.com>
 */

#include <sofia-sip/stun_common.h>

#ifndef SU_WAIT_H
#include <sofia-sip/su_wait.h>
#endif

#ifndef SU_TAG_H
#include <sofia-sip/su_tag.h>
#endif

#include <sofia-sip/su_localinfo.h>


SOFIA_BEGIN_DECLS

typedef struct stun_handle_s     stun_handle_t;
typedef struct stun_request_s    stun_request_t;
typedef struct stun_discovery_s  stun_discovery_t;
typedef struct stun_dns_lookup_s stun_dns_lookup_t;

typedef struct stun_mini_s     stun_mini_t;

#ifndef STUN_MAGIC_T
#define STUN_MAGIC_T            struct stun_magic_t
#endif
/** STUN server context */
typedef STUN_MAGIC_T stun_magic_t;

#ifndef STUN_DISCOVERY_MAGIC_T
#define STUN_DISCOVERY_MAGIC_T            struct stun_discovery_magic_t
#endif
/** STUN discovery_ context */
typedef STUN_DISCOVERY_MAGIC_T stun_discovery_magic_t;

/** Name and version of STUN software */
SOFIAPUBVAR char const stun_version[];

/**
 * STUN Action types. These define the current discovery process.
 * Defined as a bitmap.
 */
typedef enum stun_action_s {
  stun_action_no_action = 1,
  stun_action_tls_query = 2,
  stun_action_binding_request = 4,
  stun_action_keepalive = 8,
  stun_action_test_nattype = 16,
  stun_action_test_lifetime = 32,
} stun_action_t;

/**
 * NAT types
 *
 * XXX: should be extended to distinguish between filtering and
 *      mapping allocation behaviour (see IETF BEHAVE documents)
 *
 * Note: the NAT type detection algorithm can fail in
 *       case where the NAT behaves in a nondeterministic
 *       fashion.
 **/
typedef enum stun_nattype_e {
  stun_nat_unknown = 0,

  /* no NAT between client and STUN server */
  stun_open_internet,

  /* UDP communication blocked by FW */
  stun_udp_blocked,

  /* No NAT, but a FW element is performing address and port
   * restricted filtering. */
  stun_sym_udp_fw,

  /* Endpoint independent filtering (endpoint independent mapping)
   * RFC3489 full cone NAT. */
  stun_nat_full_cone,

  /* Address restricted filtering (endpoint independent mapping),
   * RFC3489 restricted cone NAT. */
  stun_nat_res_cone,

  /* Address and port restricted filtering (endpoint
   * independent mapping), RFC3489 port restricted cone */
  stun_nat_port_res_cone,

  /* Endpoint independent filtering, endpoint dependent mapping. */
  stun_nat_ei_filt_ad_map,

  /* Address dependent filtering, endpoint dependent mapping. */
  stun_nat_ad_filt_ad_map,

  /* Address and port dependent filtering, endpoint dependent mapping
   * RFC3489 symmetric NAT). */
  stun_nat_adp_filt_ad_map,

} stun_nattype_t;

/**
 * States of the STUN client->server query process.
 *
 * @see stun_bind()
 * @see stun_obtain_shared_secret()
 * @see stun_test_nattype()
 * @see stun_test_lifetime()
 */
typedef enum stun_state_e {

  stun_no_assigned_event,

  /* TLS events; see stun_obtain_shared_request() */
  stun_tls_connecting,          /**< Connecting to TLS port */
  stun_tls_ssl_connecting,      /**< Started the TLS/SSL handshake */
  stun_tls_writing,             /**< Next step: send request */
  stun_tls_closing,             /**< Closing TLS connection */
  stun_tls_reading,             /**< Request send, waiting for response */
  stun_tls_done,                /**< Shared-secret acquired */

  /* STUN discovery events */
  stun_discovery_done,          /**< Discovery process done */

  /* STUN errors */
  /* Do not change the order! Errors need to be after stun_error */

  stun_error,                   /**< Generic error in discovery process */
  stun_tls_connection_timeout,  /**< No response to connect attempt */
  stun_tls_connection_failed,   /**< No response from TLS/SSL server  */
  stun_tls_ssl_connect_failed,  /**< TLS/SSL handshake failed */

  stun_discovery_error,         /**< Error in discovery process */
  stun_discovery_timeout,       /**< No response to discovery request */

} stun_state_t;

/* -------------------------------------------------------------------
 * Calback function prototypes (signals emitted by the stack) */

/* Per discovery */
typedef void (*stun_discovery_f)(stun_discovery_magic_t *magic,
				 stun_handle_t *sh,
				 stun_discovery_t *sd,
				 stun_action_t action,
				 stun_state_t event);

/** Callback invoked by stun handle when it has a message to send. */
typedef int (*stun_send_callback)(stun_magic_t *magic,
				  stun_handle_t *sh,
				  int socket,
				  void *data,
				  unsigned len,
				  int only_a_keepalive);

/** Callback for delivering DNS lookup results */
typedef void (*stun_dns_lookup_f)(stun_dns_lookup_t *self,
				  stun_magic_t *magic);

/* -------------------------------------------------------------------
 * Functions for managing STUN handles. */

SOFIAPUBFUN stun_handle_t *stun_handle_init(su_root_t *root,
					    tag_type_t, tag_value_t, ...);

SOFIAPUBFUN void stun_handle_destroy(stun_handle_t *sh);

SOFIAPUBFUN su_root_t *stun_root(stun_handle_t *sh);
SOFIAPUBFUN int stun_is_requested(tag_type_t tag, tag_value_t value, ...);
SOFIAPUBFUN char const *stun_str_state(stun_state_t state);
SOFIAPUBFUN su_addrinfo_t const *stun_server_address(stun_handle_t *sh);

SOFIAPUBFUN
int stun_process_message(stun_handle_t *sh, su_socket_t s,
			 su_sockaddr_t *sa, socklen_t salen,
			 void *data, isize_t len);
SOFIAPUBFUN
int stun_process_request(su_socket_t s, stun_msg_t *req,
			 int sid, su_sockaddr_t *from_addr,
			 socklen_t from_len);

/* -------------------------------------------------------------------
 * Functions for 'Binding Discovery' usage (RFC3489/3489bis) */

SOFIAPUBFUN
int stun_obtain_shared_secret(stun_handle_t *sh, stun_discovery_f,
			      stun_discovery_magic_t *magic,
			      tag_type_t tag, tag_value_t value, ...);

SOFIAPUBFUN
int stun_bind(stun_handle_t *sh,
	      stun_discovery_f, stun_discovery_magic_t *magic,
	      tag_type_t tag, tag_value_t value, ...);

SOFIAPUBFUN
int stun_discovery_get_address(stun_discovery_t *sd,
			       void *addr,
			       socklen_t *return_addrlen);
SOFIAPUBFUN su_socket_t stun_discovery_get_socket(stun_discovery_t *sd);
SOFIAPUBFUN int stun_discovery_release_socket(stun_discovery_t *sd);

SOFIAPUBFUN
int stun_test_nattype(stun_handle_t *sh,
		       stun_discovery_f, stun_discovery_magic_t *magic,
		       tag_type_t tag, tag_value_t value, ...);
SOFIAPUBFUN char const *stun_nattype_str(stun_discovery_t *sd);
SOFIAPUBFUN stun_nattype_t stun_nattype(stun_discovery_t *sd);

SOFIAPUBFUN
int stun_test_lifetime(stun_handle_t *sh,
		       stun_discovery_f, stun_discovery_magic_t *magic,
		       tag_type_t tag, tag_value_t value, ...);
SOFIAPUBFUN int stun_lifetime(stun_discovery_t *sd);

/* -------------------------------------------------------------------
 * Functions for 'Connectivity Check' and 'NAT Keepalives' usages (RFC3489bis) */

SOFIAPUBFUN
int stun_set_uname_pwd(stun_handle_t *sh,
		       const char *uname,
		       isize_t len_uname,
		       const char *pwd,
		       isize_t len_pwd);

SOFIAPUBFUN int stun_msg_is_keepalive(uint16_t data);
SOFIAPUBFUN int stun_message_length(void *data, isize_t len, int end_of_message);

/* Create a keepalive dispatcher for bound SIP sockets */

SOFIAPUBFUN
int stun_keepalive(stun_handle_t *sh,
		   su_sockaddr_t *sa,
		   tag_type_t tag, tag_value_t value,
		   ...);
SOFIAPUBFUN int stun_keepalive_destroy(stun_handle_t *sh, su_socket_t s);

/* -------------------------------------------------------------------
 * Functions for 'Short-Term password' usage (RFC3489bis) */

/* (not implemented, see stun_request_shared_secret()) */

/* -------------------------------------------------------------------
 * Functions for STUN server discovery using DNS (RFC3489/3489bis) */

SOFIAPUBFUN
stun_dns_lookup_t *stun_dns_lookup(stun_magic_t *magic,
				   su_root_t *root,
				   stun_dns_lookup_f func,
				   const char *domain);
SOFIAPUBFUN void stun_dns_lookup_destroy(stun_dns_lookup_t *self);

SOFIAPUBFUN int stun_dns_lookup_udp_addr(stun_dns_lookup_t *,
					 const char **target, uint16_t *port);
SOFIAPUBFUN int stun_dns_lookup_tcp_addr(stun_dns_lookup_t *self,
					 const char **target, uint16_t *port);
SOFIAPUBFUN int stun_dns_lookup_stp_addr(stun_dns_lookup_t *self,
					 const char **target, uint16_t *port);

/* -------------------------------------------------------------------
 * Functions for minimal STUN server */

SOFIAPUBFUN stun_mini_t *stun_mini_create(void);
SOFIAPUBFUN void stun_mini_destroy(stun_mini_t *);

SOFIAPUBFUN int stun_mini_add_socket(stun_mini_t *server,
				     su_socket_t socket);
SOFIAPUBFUN int stun_mini_remove_socket(stun_mini_t *server,
					su_socket_t socket);

SOFIAPUBFUN void stun_mini_request(stun_mini_t *server, su_socket_t socket,
				   void *msg, ssize_t msglen,
				   void *addr, socklen_t addrlen);

SOFIA_END_DECLS

#endif /* !defined(STUN_H) */
