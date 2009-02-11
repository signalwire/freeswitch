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

#ifndef TPORT_H
/** Defined when <sofia-sip/tport.h> has been included. */
#define TPORT_H
/**@file sofia-sip/tport.h
 * @brief Transport interface
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Jun 29 15:58:06 2000 ppessi
 */

#ifndef SU_H
#include <sofia-sip/su.h>
#endif
#ifndef SU_STRLST_H
#include <sofia-sip/su_strlst.h>
#endif
#ifndef SU_WAIT_H
#include <sofia-sip/su_wait.h>
#endif
#ifndef MSG_H
#include <sofia-sip/msg.h>
#endif
#ifndef URL_H
#include <sofia-sip/url.h>
#endif
#ifndef TPORT_TAG_H
#include <sofia-sip/tport_tag.h>
#endif

SOFIA_BEGIN_DECLS

struct tport_s;
#ifndef TPORT_T
#define TPORT_T struct tport_s
typedef TPORT_T tport_t;
#endif

#ifndef TP_STACK_T
#ifndef TP_AGENT_T
#define TP_STACK_T struct tp_stack_s
#else
#define TP_STACK_T TP_AGENT_T
#endif
#endif
/** Type of stack object */
typedef TP_STACK_T tp_stack_t;

#ifndef TP_MAGIC_T
/** Type of transport-protocol-specific context.  @sa @ref tp_magic */
#define TP_MAGIC_T struct tp_magic_s
#endif
/** Type of transport-protocol-specific context object. */
typedef TP_MAGIC_T tp_magic_t;

#ifndef TP_CLIENT_T
#define TP_CLIENT_T struct tp_client_s
#endif
/** Transaction object given as a reference to the transport.
 *
 *  This type is used when transport reports errors with pending requests.
 */
typedef TP_CLIENT_T tp_client_t;

struct sigcomp_compartment;
struct sigcomp_udvm;

/** Interface towards stack. */
typedef struct {
  int      tpac_size;

  /** Function used to pass a received message to the protocol stack */
  void   (*tpac_recv)(tp_stack_t *, tport_t *, msg_t *msg, tp_magic_t *magic,
		      su_time_t received);

  /** Function used to indicate an error to the protocol stack */
  void   (*tpac_error)(tp_stack_t *, tport_t *,
		       int errcode, char const *remote);

  /** Ask stack to allocate a message. */
  msg_t *(*tpac_alloc)(tp_stack_t *, int flags,
		       char const [], usize_t,
		       tport_t const *, tp_client_t *);

  /** Indicate stack that address has changed */
  void (*tpac_address)(tp_stack_t *, tport_t *);

} tport_stack_class_t;

/* Compatibility */
typedef tport_stack_class_t tp_stack_class_t;

/** Callback to report error by pending requests. */
typedef void tport_pending_error_f(tp_stack_t *, tp_client_t *,
				   tport_t *, msg_t *msg, int error);

enum {
  /** Maximum number of messages in send queue. */
  TPORT_QUEUESIZE = 64
};


/* AI extension flags - these must not overlap with existing AI flags. */

/** Message is to be sent/received compressed */
#define TP_AI_COMPRESSED 0x01000
/** Message is to be sent/received on secure connection */
#define TP_AI_SECURE     0x02000

/** Halfclose (shutdown(c, 1)) connection after sending message */
#define TP_AI_SHUTDOWN   0x04000
/** Close connection (shutdown(c, 2)) after sending message */
#define TP_AI_CLOSE      0x08000

/** Address was inaddr_any */
#define TP_AI_ANY        0x80000

#define TP_AI_MASK       0xff000

/** Maximum size of a @e host:port string, including final NUL. */
#define TPORT_HOSTPORTSIZE (55)

/** Transport name.
 *
 * This structure represents the address of the transport in textual format.
 * For primary transports, the transport name contains the local address,
 * for secondary transports, the peer address.
 *
 * The tpn_ident specifies the transport identifier used to make difference
 * between connectivity domains.
 */
typedef struct {
  char const *tpn_proto;	/**< Protocol name ("udp", "tcp", etc.) */
  char const *tpn_canon;	/**< Node DNS name (if known). */
  char const *tpn_host;		/**< Node address in textual format */
  char const *tpn_port;		/**< Port number in textual format. */
  char const *tpn_comp;		/**< Compression algorithm (NULL if none) */
  char const *tpn_ident;	/**< Transport identifier (NULL if none) */
} tp_name_t;

#define TPN_FORMAT "%s/%s:%s%s%s%s%s"

#define TPN_ARGS(n)							\
  (n)->tpn_proto, (n)->tpn_host, (n)->tpn_port,				\
  (n)->tpn_comp ? ";comp=" : "", (n)->tpn_comp ? (n)->tpn_comp : "",    \
  (n)->tpn_ident ? "/" : "", (n)->tpn_ident ? (n)->tpn_ident : ""

/**Create master transport. */
TPORT_DLL tport_t *tport_tcreate(tp_stack_t *stack,
				 tport_stack_class_t const *tpac,
				 su_root_t *root,
				 tag_type_t tag, tag_value_t value, ...);

/** Bind transports to network. */
TPORT_DLL int tport_tbind(tport_t *self,
			  tp_name_t const *tpn,
			  char const * const transports[],
			  tag_type_t tag, tag_value_t value, ...);

/** Get transport parameters. */
TPORT_DLL int tport_get_params(tport_t const *, tag_type_t tag, tag_value_t value, ...);

/** Set transport parameters. */
TPORT_DLL int tport_set_params(tport_t *self, tag_type_t tag, tag_value_t value, ...);

/** Destroy transport(s). */
TPORT_DLL void tport_destroy(tport_t *tport);

/** Shutdown a transport connection. */
TPORT_DLL int tport_shutdown(tport_t *tport, int how);

/** Create a new reference to a transport object. */
TPORT_DLL tport_t *tport_ref(tport_t *tp);

/** Destroy reference to a transport object. */
TPORT_DLL void tport_unref(tport_t *tp);

/** Create a new transport reference. @deprecated Use tport_ref(). */
TPORT_DLL tport_t *tport_incref(tport_t *tp);

/** Destroy a transport reference. @deprecated Use tport_unref(). */
TPORT_DLL void tport_decref(tport_t **tp);

/** Send a message using transport. */
TPORT_DLL tport_t *tport_tsend(tport_t *, msg_t *, tp_name_t const *,
			       tag_type_t, tag_value_t, ...);

/** Queue a message to transport. */
TPORT_DLL int tport_tqueue(tport_t *, msg_t *, tag_type_t, tag_value_t, ...);

/** Return number of queued messages. */
TPORT_DLL isize_t tport_queuelen(tport_t const *self);

/** Send a queued message (and queue another, if required). */
TPORT_DLL int tport_tqsend(tport_t *, msg_t *, msg_t *,
			   tag_type_t, tag_value_t, ...);

/** Stop reading from socket until tport_continue() is called. */
TPORT_DLL int tport_stall(tport_t *self);

/** Continue reading from socket. */
TPORT_DLL int tport_continue(tport_t *self);

/** Mark message as waiting for a response. */
TPORT_DLL int tport_pend(tport_t *self, msg_t *msg,
			 tport_pending_error_f *callback, tp_client_t *client);

/** Do not wait for response anymore. */
TPORT_DLL int tport_release(tport_t *self, int pendd,
			    msg_t *msg, msg_t *reply, tp_client_t *client,
			    int still_pending);

/** Return true if transport is master. */
TPORT_DLL int tport_is_master(tport_t const *self);

/** Return true if transport is primary. */
TPORT_DLL int tport_is_primary(tport_t const *self);

/** Return nonzero if transport is public. */
TPORT_DLL int tport_is_public(tport_t const *self);

/** Return true if transport is secondary. */
TPORT_DLL int tport_is_secondary(tport_t const *self);

/** Return true if transport is reliable, false otherwise */
TPORT_DLL int tport_is_reliable(tport_t const *tport);

/** Return true if transport is a stream (no message boundaries). */
TPORT_DLL int tport_is_stream(tport_t const *tport);

/** Return true if transport is dgram-based. */
TPORT_DLL int tport_is_dgram(tport_t const *tport);

/** Return true if transport supports IPv4 */
TPORT_DLL int tport_has_ip4(tport_t const *tport);

/** Return true if transport supports IPv6 */
TPORT_DLL int tport_has_ip6(tport_t const *tport);

/** Test if transport is udp. */
TPORT_DLL int tport_is_udp(tport_t const *self);

/** Test if transport is tcp. */
TPORT_DLL int tport_is_tcp(tport_t const *self);

/** Test if transport has TLS. */
TPORT_DLL int tport_has_tls(tport_t const *tport);

/** Test if transport provided a verified certificate chain (TLS only) */
TPORT_DLL int tport_is_verified(tport_t const *tport);

/** Return true if transport is being updated. */
TPORT_DLL int tport_is_updating(tport_t const *self);

/** Test if transport has been closed. @NEW_1_12_4. */
TPORT_DLL int tport_is_closed(tport_t const *self);

/** Test if transport has been shut down. @NEW_1_12_4. */
TPORT_DLL int tport_is_shutdown(tport_t const *self);

/** Test if transport is connected. @NEW_1_12_5. */
TPORT_DLL int tport_is_connected(tport_t const *self);

/** Test if transport can be used to send message. @NEW_1_12_7. */
TPORT_DLL int tport_is_clear_to_send(tport_t const *self);

/** Set transport magic. */
TPORT_DLL void tport_set_magic(tport_t *self, tp_magic_t *magic);

/** Get transport magic. */
TPORT_DLL tp_magic_t *tport_magic(tport_t const *tport);

/** Get transport name. */
TPORT_DLL tp_name_t const *tport_name(tport_t const *tport);

/** Get transport address list. */
TPORT_DLL su_addrinfo_t const *tport_get_address(tport_t const *tport);

/** Get transport ident. */
TPORT_DLL char const *tport_ident(tport_t const *self);

/** Get primary transport (or self, if already parent) */
TPORT_DLL tport_t *tport_parent(tport_t const *self);

/** Flush idle connections */
TPORT_DLL int tport_flush(tport_t *);

/** Get primary transports */
TPORT_DLL tport_t *tport_primaries(tport_t const *tport);

/** Get next transport */
TPORT_DLL tport_t *tport_next(tport_t const *tport);

/** Get secondary transports. */
TPORT_DLL tport_t *tport_secondary(tport_t const *tport);

/** Get a protocol corresponding to the protocol name. */
TPORT_DLL tport_t *tport_by_protocol(tport_t const *self, char const *proto);

/** Get transport by interface identifier and protocol name. */
TPORT_DLL tport_t *tport_primary_by_name(tport_t const *self, tp_name_t const *tpn);

/** Get a transport corresponding to the name */
TPORT_DLL tport_t *tport_by_name(tport_t const *self, tp_name_t const  *);

/** Create a transport name corresponding to the URL. */
TPORT_DLL int tport_name_by_url(su_home_t *, tp_name_t *,
				url_string_t const *us);

/** Return source transport object for delivered message */
TPORT_DLL tport_t *tport_delivered_by(tport_t const *tp, msg_t const *msg);

/** Return source transport name for delivered message */
TPORT_DLL int tport_delivered_from(tport_t *tp, msg_t const *msg,
				   tp_name_t name[1]);

/** Return TLS Subjects provided by the source transport */
TPORT_DLL su_strlst_t const *tport_delivered_from_subjects(tport_t *tp, 
                                                           msg_t const *msg);

/** Check if the given subject string is found in su_strlst_t */
TPORT_DLL int tport_subject_search(char const *, su_strlst_t const *);

/** Check if transport named is already resolved */
TPORT_DLL int tport_name_is_resolved(tp_name_t const *);

/** Duplicate a transport name. */
TPORT_DLL int tport_name_dup(su_home_t *,
			     tp_name_t *dst, tp_name_t const *src);

/** Convert a socket address to a transport name. */
TPORT_DLL int tport_convert_addr(su_home_t *home,
				 tp_name_t *tpn,
				 char const *protoname,
				 char const *canon,
				 su_sockaddr_t const *su);

/** Print host and port separated with ':' to a string. */
TPORT_DLL char *tport_hostport(char buf[], isize_t bufsize,
			       su_sockaddr_t const *su, int with_port);

/** Initialize STUN keepalives. */
TPORT_DLL int tport_keepalive(tport_t *tp, su_addrinfo_t const *ai,
			      tag_type_t tag, tag_value_t value, ...);

/* ---------------------------------------------------------------------- */
/* SigComp-related functions */

#ifndef TPORT_COMPRESSOR
#define TPORT_COMPRESSOR struct tport_compressor
#endif

typedef TPORT_COMPRESSOR tport_compressor_t;

TPORT_DLL int tport_can_send_sigcomp(tport_t const *self);
TPORT_DLL int tport_can_recv_sigcomp(tport_t const *self);

TPORT_DLL int tport_has_compression(tport_t const *self, char const *comp);
TPORT_DLL int tport_set_compression(tport_t *self, char const *comp);

/** Set SigComp option. */
TPORT_DLL
int tport_sigcomp_option(tport_t const *self,
			 struct sigcomp_compartment *cc,
			 char const *option);

/** Obtain a SigComp compartment with given name. */
TPORT_DLL struct sigcomp_compartment *
tport_sigcomp_compartment(tport_t *self,
			  char const *name, isize_t namelen,
			  int create_if_needed);

/** Assign a SigComp compartment to a connection-oriented tport. */
TPORT_DLL int
tport_sigcomp_assign(tport_t *self, struct sigcomp_compartment *);

/** Test if a SigComp compartment is assigned to a tport. */
TPORT_DLL int tport_has_sigcomp_assigned(tport_t const *self);

/** Accept SigComp message */
TPORT_DLL int
tport_sigcomp_accept(tport_t *self,
		     struct sigcomp_compartment *cc,
		     msg_t *msg);

/** Get compressor context with which the request was delivered */
TPORT_DLL int
tport_delivered_with_comp(tport_t *tp, msg_t const *msg,
			  tport_compressor_t **return_compressor);

/** Shutdown SigComp compartment */
TPORT_DLL int
tport_sigcomp_close(tport_t *self,
		    struct sigcomp_compartment *cc,
		    int how);

/** Set SigComp compartment lifetime. */
TPORT_DLL int
tport_sigcomp_lifetime(tport_t *self,
		       struct sigcomp_compartment *,
		       unsigned lifetime_in_ms,
		       int only_expand);


SOFIA_END_DECLS

#endif /* TPORT_H */
