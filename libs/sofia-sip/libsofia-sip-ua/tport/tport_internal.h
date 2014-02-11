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

#ifndef TPORT_INTERNAL_H
/** Defined when <tport_internal.h> has been included. */
#define TPORT_INTERNAL_H

/**@internal
 * @file tport_internal.h
 * @brief Internal implementation of transport interface
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Jun 29 15:58:06 2000 ppessi
 */

#ifndef SU_H
#include <sofia-sip/su.h>
#endif

#include <sofia-sip/su_uniqueid.h>
#include <sofia-sip/su_strlst.h>

#ifndef MSG_ADDR_H
#include <sofia-sip/msg_addr.h>
#endif
#ifndef TPORT_H
#include <sofia-sip/tport.h>
#endif

#if HAVE_SOFIA_STUN
#include "sofia-sip/stun.h"
#include "sofia-sip/stun_tag.h"
#endif

#include <sofia-sip/tport_plugins.h>

#ifndef SU_DEBUG
#define SU_DEBUG 3
#endif
#define SU_LOG   tport_log

#include <sofia-sip/su_debug.h>

#if !defined(MSG_NOSIGNAL) || defined(__CYGWIN__) || defined(SYMBIAN)
#undef MSG_NOSIGNAL
#define MSG_NOSIGNAL (0)
#endif

#if (_WIN32_WINNT >= 0x0600)
#ifndef HAVE_MSG_TRUNC
#define HAVE_MSG_TRUNC 1
#endif
#endif

#if !HAVE_MSG_TRUNC
#define MSG_TRUNC (0)
#endif

#ifndef NONE
#ifndef _MSC_VER
#define NONE ((void *)-1)
#else
#define NONE ((void *)(INT_PTR)-1)
#endif
#endif

SOFIA_BEGIN_DECLS

typedef struct tport_master tport_master_t;
typedef struct tport_pending_s tport_pending_t;
typedef struct tport_primary tport_primary_t;
typedef struct tport_vtable tport_vtable_t;

struct sigcomp_state_handler;
struct sigcomp_algorithm;
struct sigcomp_udvm;
struct sigcomp_magic;
struct sigcomp_compartment;

typedef long unsigned LU; 	/* for printf() and friends */

/** @internal Transport parameters */
typedef struct {
  unsigned tpp_mtu;		/**< Maximum packet size */
  unsigned tpp_idle;		/**< Allowed connection idle time. */
  unsigned tpp_timeout;		/**< Allowed idle time for message. */
  unsigned tpp_socket_keepalive;/**< Socket keepalive interval */
  unsigned tpp_keepalive;	/**< Keepalive PING interval */
  unsigned tpp_pingpong;	/**< PONG-to-PING interval */

  unsigned tpp_sigcomp_lifetime;  /**< SigComp compartment lifetime  */
  unsigned tpp_thrpsize;	/**< Size of thread pool */

  unsigned tpp_thrprqsize;	/**< Length of per-thread recv queue */
  unsigned tpp_qsize;		/**< Size of queue */

  unsigned tpp_drop;		/**< Packet drop probablity */
  int      tpp_tos;         	/**< IP TOS */

  unsigned tpp_conn_orient:1;   /**< Connection-orienteded */
  unsigned tpp_sdwn_error:1;	/**< If true, shutdown is error. */
  unsigned tpp_stun_server:1;	/**< If true, use stun server */
  unsigned tpp_pong2ping:1;	/**< If true, respond with pong to ping */

  unsigned :0;

} tport_params_t;


/** @internal Transport object.
 *
 * A transport object can be used in three roles, to represent transport
 * list (aka master transport), to represent available transports (aka
 * primary transport) and to represent actual transport connections (aka
 * secondary transport).
 */
struct tport_s {
  su_home_t           tp_home[1];       /**< Memory home */

  ssize_t             tp_refs;		/**< Number of references to tport */

  unsigned            tp_black:1;       /**< Used by red-black-tree */

  unsigned            tp_accepted:1;    /**< Originally server? */
  unsigned            tp_conn_orient:1;	/**< Is connection-oriented */
  unsigned            tp_has_connection:1; /**< Has real connection */
  unsigned            tp_reusable:1;    /**< Can this connection be reused */
  unsigned            tp_closed : 1;
  /**< This transport is closed.
   *
   * A closed transport is inserted into pri_closed list.
   */

  /** Remote end has sent FIN (2) or we should not just read */
  unsigned            tp_recv_close:2;
  /** We will send FIN (1) or have sent FIN (2) */
  unsigned            tp_send_close:2;
  unsigned            tp_has_keepalive:1;
  unsigned            tp_has_stun_server:1;
  unsigned            tp_trunc:1;
  unsigned            tp_is_connected:1; /**< Connection is established */
  unsigned            tp_verified:1;     /**< Certificate Chain was verified */
  unsigned            tp_pre_framed:1;   /** Data is pre-framed **/
  unsigned:0;

  tport_t *tp_left, *tp_right, *tp_dad; /**< Links in tport tree */

  tport_master_t     *tp_master;        /**< Master transport */
  tport_primary_t    *tp_pri;           /**< Primary transport */

  tport_params_t     *tp_params;        /**< Transport parameters */

  tp_magic_t         *tp_magic; 	/**< Context provided by consumer */

  su_timer_t         *tp_timer;	        /**< Timer object */

  su_time_t           tp_ktime;	        /**< Keepalive timer updated */
  su_time_t           tp_ptime;	        /**< Ping sent */

  tp_name_t           tp_name[1];	/**< Transport name.
					 *
					 * This is either our name (if primary)
					 * or peer name (if secondary).
					 */

  su_strlst_t        *tp_subjects;      /**< Transport Subjects.
                                         *
                                         * Subject Name(s) provided by the peer
					 * in a TLS connection (if secondary).
					 * or matched against incoming 
					 * connections (if primary).
                                         */

#define tp_protoname tp_name->tpn_proto
#define tp_canon     tp_name->tpn_canon
#define tp_host      tp_name->tpn_host
#define tp_port      tp_name->tpn_port
#define tp_ident     tp_name->tpn_ident

  su_socket_t  	      tp_socket;	/**< Socket of this tport*/
  int                 tp_index;		/**< Root registration index */
  int                 tp_events;        /**< Subscribed events */

  su_addrinfo_t       tp_addrinfo[1];   /**< Peer/own address info */
  su_sockaddr_t       tp_addr[1];	/**< Peer/own address */
#define tp_addrlen tp_addrinfo->ai_addrlen

  /* ==== Receive queue ================================================== */

  msg_t   	     *tp_msg;		/**< Message being received */
  msg_t const        *tp_rlogged;       /**< Last logged when receiving */
  su_time_t           tp_rtime;	        /**< Last time received data */
  unsigned short      tp_ping;	        /**< Whitespace ping being received */

  /* ==== Pending messages =============================================== */

  unsigned short      tp_reported;      /**< Report counter */
  unsigned            tp_plen;          /**< Size of tp_pending */
  unsigned            tp_pused;         /**< Used pends */
  tport_pending_t    *tp_pending;       /**< Pending requests */
  tport_pending_t    *tp_released;      /**< Released pends */

  /* ==== Send queue ===================================================== */

  msg_t             **tp_queue;		/**< Messages being sent */
  unsigned short      tp_qhead;		/**< Head of queue */

  msg_iovec_t        *tp_unsent;	/**< Pointer to first unsent iovec */
  size_t              tp_unsentlen;	/**< Number of unsent iovecs */

  msg_iovec_t        *tp_iov;		/**< Iovecs allocated for sending */
  size_t              tp_iovlen;	/**< Number of allocated iovecs */

  msg_t const        *tp_slogged;       /**< Last logged when sending */
  su_time_t           tp_stime;	        /**< Last time sent message */

  /* ==== Extensions  ===================================================== */

  tport_compressor_t *tp_comp;

  /* ==== Statistics  ===================================================== */

  struct {
    uint64_t sent_msgs, sent_errors, sent_bytes, sent_on_line;
    uint64_t recv_msgs, recv_errors, recv_bytes, recv_on_line;
  } tp_stats;
};

/** @internal Primary structure */
struct tport_primary {
  tport_t             pri_primary[1];   /**< Transport part */
#if DOXYGEN_ONLY
  su_home_t           pri_home[1];
#else
#define pri_home      pri_primary->tp_home
#define pri_master    pri_primary->tp_master
#define pri_protoname pri_primary->tp_name->tpn_proto
#endif
  tport_vtable_t const
                     *pri_vtable;
  int                 pri_public;       /**< Type of primary transport;
					 * tport_type_local,
					 * tport_type_stun, etc.
					 */

  tport_primary_t    *pri_next;	        /**< Next primary tport */

  tport_t            *pri_open;		/**< Open secondary tports */
  tport_t            *pri_closed;       /**< Closed secondary tports */

  unsigned            pri_updating:1;   /**< Currently updating address */
  unsigned            pri_natted:1;	/**< Using natted address  */
  unsigned            pri_has_tls:1;	/**< Supports tls  */
  unsigned:0;

  void               *pri_stun_handle;

  tport_params_t      pri_params[1];      /**< Transport parameters */
};

/** @internal Master structure */
struct tport_master {
  tport_t             mr_master[1];
#if DOXYGEN_ONLY
  su_home_t           mr_home[1];
#else
#define mr_home mr_master->tp_home
#endif

  int                 mr_stun_step_ready; /**< for stun's callback */

  tp_stack_t  	     *mr_stack;         /**< Transport consumer */
  tp_stack_class_t
               const *mr_tpac;		/**< Methods provided by stack */
  int                 mr_log;	        /**< Do logging of parsed messages */
  su_root_t    	     *mr_root;		/**< SU root pointer */

  /**< Timer reclaiming unused connections and compartment */
  su_timer_t         *mr_timer;
  /** FILE to dump received and sent data */
  FILE               *mr_dump_file;
  char               *mr_dump;	/**< Filename for dumping received/sent data */
  /** SOCK to dump received and sent data */
  su_socket_t         mr_capt_sock;
  char               *mr_capt_name;	/**< Servername for capturing received/sent data */  
  tport_primary_t    *mr_primaries;        /**< List of primary contacts */

  tport_params_t      mr_params[1];

  unsigned            mr_boundserver:1; /**< Server has been bound */
  unsigned            mr_bindv6only:1; /**< We can bind separately to IPv6/4 */
  unsigned :0;

  /* Delivery context */
  struct tport_delivery {
    tport_t              *d_tport;
    msg_t                *d_msg;
    tp_name_t             d_from[1];
    tport_compressor_t   *d_comp;
  } mr_delivery[1];

  tport_stun_server_t *mr_stun_server;

#if 0
  struct tport_nat_s {
    int initialized;
    int bound;
    int stun_enabled;
    char *external_ip_address;
#if HAVE_UPNP || HAVE_SOFIA_STUN
    int try_stun;
#endif
#if HAVE_UPNP
#endif
#if HAVE_SOFIA_STUN
    tport_master_t *tport;
    char *stun_server;
    /* stun_socket_t *stun_socket; */
    stun_handle_t *stun;
    su_socket_t stun_socket;
    su_sockaddr_t sockaddr;
#endif
  }                   mr_nat[1];
#endif
};

/** @internal Virtual function table for transports */
struct tport_vtable
{
  char const *vtp_name;
  enum tport_via vtp_public;

  size_t vtp_pri_size;		/* Size of primary tport */
  int (*vtp_init_primary)(tport_primary_t *pri,
			  tp_name_t tpn[1],
			  su_addrinfo_t *ai, tagi_t const *,
			  char const **return_culprit);
  void (*vtp_deinit_primary)(tport_primary_t *pri);
  int (*vtp_wakeup_pri)(tport_primary_t *pri, int events);
  tport_t *(*vtp_connect)(tport_primary_t *pri, su_addrinfo_t *ai,
			  tp_name_t const *tpn);

  size_t vtp_secondary_size;	/* Size of secondary tport */

  int (*vtp_init_secondary)(tport_t *, int socket, int accepted,
			    char const **return_reason);
  void (*vtp_deinit_secondary)(tport_t *);
  void (*vtp_shutdown)(tport_t *, int how);
  int (*vtp_set_events)(tport_t const *self);
  int (*vtp_wakeup)(tport_t *self, int events);
  int (*vtp_recv)(tport_t *self);
  ssize_t (*vtp_send)(tport_t const *self, msg_t *msg,
		      msg_iovec_t iov[], size_t iovused);
  void (*vtp_deliver)(tport_t *self,  msg_t *msg, su_time_t now);
  int (*vtp_prepare)(tport_t *self, msg_t *msg,
		     tp_name_t const *tpn,
		     struct sigcomp_compartment *cc,
		     unsigned mtu);
  int (*vtp_keepalive)(tport_t *self, su_addrinfo_t const *ai,
		       tagi_t const *taglist);
  int (*vtp_stun_response)(tport_t const *self,
			   void *msg, size_t msglen,
			   void *addr, socklen_t addrlen);
  int (*vtp_next_secondary_timer)(tport_t *self, su_time_t *,
				  char const **return_why);
  void (*vtp_secondary_timer)(tport_t *self, su_time_t);
};

int tport_register_type(tport_vtable_t const *vtp);

/** @internal Test if transport needs connect() before sending. */
su_inline int tport_is_connection_oriented(tport_t const *self)
{
  return self->tp_conn_orient;
}

/** @internal Test if transport involves connection. @NEW_1_12_5. */
su_inline int tport_has_connection(tport_t const *self)
{
  return self->tp_has_connection;
}

void tport_has_been_updated(tport_t *tport);

int tport_primary_compression(tport_primary_t *pri,
			      char const *compression,
			      tagi_t const *tl);

void tport_set_tos(su_socket_t socket, su_addrinfo_t *ai, int tos);

tport_t *tport_base_connect(tport_primary_t *pri,
			    su_addrinfo_t *ai,
			    su_addrinfo_t *name,
			    tp_name_t const *tpn);

int tport_stream_init_primary(tport_primary_t *pri,
			      su_socket_t socket,
			      tp_name_t tpn[1],
			      su_addrinfo_t *ai,
			      tagi_t const *tags,
			      char const **return_reason);

tport_t *tport_alloc_secondary(tport_primary_t *pri,
			       int socket,
			       int accepted,
			       char const **return_reason);

int tport_accept(tport_primary_t *pri, int events);
int tport_register_secondary(tport_t *self, su_wakeup_f wakeup, int events);
void tport_zap_secondary(tport_t *self);

int tport_set_secondary_timer(tport_t *self);
void tport_base_timer(tport_t *self, su_time_t now);

int tport_bind_socket(int socket,
		      su_addrinfo_t *ai,
		      char const **return_culprit);
void tport_close(tport_t *self);
int tport_shutdown0(tport_t *self, int how);

int tport_has_queued(tport_t const *self);

int tport_error_event(tport_t *self);
void tport_recv_event(tport_t *self);
void tport_send_event(tport_t *self);
void tport_hup_event(tport_t *self);
int tport_setname(tport_t *, char const *, su_addrinfo_t const *, char const *);

int tport_wakeup(su_root_magic_t *magic, su_wait_t *w, tport_t *self);

ssize_t tport_recv_iovec(tport_t const *self,
			 msg_t **mmsg,
			 msg_iovec_t iovec[msg_n_fragments], size_t N,
			 int exact);

msg_t *tport_msg_alloc(tport_t const *self, usize_t size);

int tport_prepare_and_send(tport_t *self, msg_t *msg,
			   tp_name_t const *tpn,
			   struct sigcomp_compartment *cc,
			   unsigned mtu);
int tport_send_msg(tport_t *self, msg_t *msg,
		   tp_name_t const *tpn,
		   struct sigcomp_compartment *cc);

void tport_send_queue(tport_t *self);

void tport_deliver(tport_t *self, msg_t *msg, msg_t *next,
		   tport_compressor_t *comp,
		   su_time_t now);
void tport_base_deliver(tport_t *self, msg_t *msg, su_time_t now);

int tport_recv_error_report(tport_t *self);
void tport_error_report(tport_t *self, int errcode,
			su_sockaddr_t const *addr);

int tport_open_log(tport_master_t *mr, tagi_t *tags);
void tport_log_msg(tport_t *tp, msg_t *msg, char const *what,
		   char const *via, su_time_t now);
void tport_dump_iovec(tport_t const *self, msg_t *msg,
		      size_t n, su_iovec_t const iov[], size_t iovused,
		      char const *what, char const *how);

void tport_capt_msg(tport_t const *self, msg_t *msg, size_t n,
                    su_iovec_t const iov[], size_t iovused, char const *what);

int tport_tcp_ping(tport_t *self, su_time_t now);
int tport_tcp_pong(tport_t *self);

extern tport_vtable_t const tport_udp_vtable;
extern tport_vtable_t const tport_udp_client_vtable;

int tport_udp_init_primary(tport_primary_t *,
			   tp_name_t tpn[1],
			   su_addrinfo_t *,
			   tagi_t const *,
			   char const **return_culprit);
void tport_udp_deinit_primary(tport_primary_t *);
int tport_recv_dgram(tport_t *self);
ssize_t tport_send_dgram(tport_t const *self, msg_t *msg,
			 msg_iovec_t iov[], size_t iovused);
int tport_udp_error(tport_t const *self, su_sockaddr_t name[1]);

extern tport_vtable_t const tport_tcp_vtable;
extern tport_vtable_t const tport_tcp_client_vtable;

int tport_tcp_init_primary(tport_primary_t *,
 			  tp_name_t  tpn[1],
 			  su_addrinfo_t *, tagi_t const *,
 			  char const **return_culprit);
int tport_tcp_init_client(tport_primary_t *,
 			 tp_name_t tpn[1],
 			 su_addrinfo_t *, tagi_t const *,
 			 char const **return_culprit);
int tport_tcp_init_secondary(tport_t *self, int socket, int accepted,
			     char const **return_reason);
int tport_recv_stream(tport_t *self);
ssize_t tport_send_stream(tport_t const *self, msg_t *msg,
			  msg_iovec_t iov[], size_t iovused);

int tport_tcp_next_timer(tport_t *self, su_time_t *, char const **);
void tport_tcp_timer(tport_t *self, su_time_t);

int tport_next_recv_timeout(tport_t *, su_time_t *, char const **);
void tport_recv_timeout_timer(tport_t *self, su_time_t now);

int tport_next_keepalive(tport_t *self, su_time_t *, char const **);
void tport_keepalive_timer(tport_t *self, su_time_t now);

extern tport_vtable_t const tport_ws_vtable;
extern tport_vtable_t const tport_ws_client_vtable;
extern tport_vtable_t const tport_wss_vtable;
extern tport_vtable_t const tport_wss_client_vtable;
extern tport_vtable_t const tport_sctp_vtable;
extern tport_vtable_t const tport_sctp_client_vtable;
extern tport_vtable_t const tport_tls_vtable;
extern tport_vtable_t const tport_tls_client_vtable;
extern tport_vtable_t const tport_stun_vtable;
extern tport_vtable_t const tport_http_connect_vtable;
extern tport_vtable_t const tport_threadpool_vtable;

typedef struct tport_descriptor_s {
  char const *tpd_name;
  tport_vtable_t *tpd_vtable;
  su_addrinfo_t *tpd_hints;
  int tpd_is_client_only;
} tport_descriptor_t;

typedef int const *(tport_set_f)(tport_master_t *mr,
				 tp_name_t const *tpn,
				 tagi_t const *taglist,
				 tport_descriptor_t **return_set,
				 int return_set_size);

/* STUN plugin */

int tport_init_stun_server(tport_master_t *mr, tagi_t const *tags);
void tport_deinit_stun_server(tport_master_t *mr);
int tport_recv_stun_dgram(tport_t const *self, msg_t **in_out_msg,
			  su_sockaddr_t *from, socklen_t fromlen);

int tport_stun_server_add_socket(tport_t *tp);
int tport_stun_server_remove_socket(tport_t *tp);

void tport_recv_bytes(tport_t *self, ssize_t bytes, ssize_t on_line);
void tport_recv_message(tport_t *self, msg_t *msg, int error);

void tport_sent_bytes(tport_t *self, ssize_t bytes, ssize_t on_line);
void tport_sent_message(tport_t *self, msg_t *msg, int error);

/* ---------------------------------------------------------------------- */
/* Compressor plugin */
extern tport_comp_vtable_t const *tport_comp_vtable;

char const *tport_canonize_comp(char const *comp);

int tport_init_compressor(tport_t *,
			  char const *comp_name,
			  tagi_t const *tags);
void tport_deinit_compressor(tport_t *);

struct sigcomp_compartment *
tport_sigcomp_assign_if_needed(tport_t *self,
			       struct sigcomp_compartment *cc);

struct sigcomp_udvm **tport_get_udvm_slot(tport_t *self);

void tport_sigcomp_accept_incomplete(tport_t *self, msg_t *msg);

int tport_recv_comp_dgram(tport_t const *self,
			  tport_compressor_t *sc,
			  msg_t **in_out_msg,
			  su_sockaddr_t *from,
			  socklen_t fromlen);

ssize_t tport_send_comp(tport_t const *self,
		    msg_t *msg,
		    msg_iovec_t iov[],
		    size_t iovused,
		    struct sigcomp_compartment *cc,
		    tport_compressor_t *sc);

SOFIA_END_DECLS

#endif /* TPORT_INTERNAL_H */
