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

#ifndef NTA_INTERNAL_H
/** Defined when <nta_internal.h> has been included. */
#define NTA_INTERNAL_H 

/**@internal @file nta_internal.h
 *
 * @brief Internals of NTA objects.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Jul 18 09:18:32 2000 ppessi
 */

/* Resolver context type */
#define SRES_CONTEXT_T    nta_outgoing_t

/* We are customer of tport_t */
#define TP_AGENT_T        nta_agent_t
#define TP_MAGIC_T        sip_via_t 
#define TP_CLIENT_T       nta_outgoing_t

#include <sofia-sip/nta.h>
#include <sofia-sip/nta_tport.h>
#include <sofia-sip/tport.h>

#if HAVE_SOFIA_SRESOLV
#include <sofia-sip/sresolv.h>
#endif

#include <sofia-sip/htable.h>

#if HAVE_SMIME
#include "smimec.h"
#endif

SOFIA_BEGIN_DECLS

/** A sip_flag telling that this message is internally generated. */
#define NTA_INTERNAL_MSG (1<<15)

/** Resolving order */
enum nta_res_order_e
{
  nta_res_ip6_ip4,
  nta_res_ip4_ip6,
  nta_res_ip6_only,
  nta_res_ip4_only
};

HTABLE_DECLARE_WITH(leg_htable, lht, nta_leg_t, size_t, hash_value_t);
HTABLE_DECLARE_WITH(outgoing_htable, oht, nta_outgoing_t, size_t, hash_value_t);
HTABLE_DECLARE_WITH(incoming_htable, iht, nta_incoming_t, size_t, hash_value_t);

typedef struct outgoing_queue_t {
  nta_outgoing_t **q_tail;
  nta_outgoing_t  *q_head;
  size_t           q_length;
  unsigned         q_timeout;
} outgoing_queue_t;

typedef struct incoming_queue_t {
  nta_incoming_t **q_tail;
  nta_incoming_t  *q_head;
  size_t           q_length;
  unsigned         q_timeout;
} incoming_queue_t;

typedef struct nta_compressor nta_compressor_t;

struct nta_agent_s
{
  su_home_t             sa_home[1];
  su_root_t            *sa_root;
  su_timer_t           *sa_timer;
  nta_agent_magic_t    *sa_magic;
  nta_message_f        *sa_callback;     

  nta_update_magic_t   *sa_update_magic;
  nta_update_tport_f   *sa_update_tport;

  su_time_t             sa_now;	 /**< Timestamp in microsecond resolution. */
  uint32_t              sa_next; /**< Timestamp for next agent_timer. */
  uint32_t              sa_millisec; /**< Timestamp in milliseconds. */

  uint32_t              sa_nw_updates; /* Shall we enable network detector? */

  uint32_t              sa_flags;	/**< Message flags */
  msg_mclass_t const   *sa_mclass;

  sip_contact_t        *sa_contact;
  sip_via_t            *sa_vias;   /**< @Via headers for all transports */
  sip_via_t            *sa_public_vias;   /**< @Vias for public transports */
  sip_contact_t        *sa_aliases;/**< List of aliases for agent */

  uint64_t              sa_branch; /**< Counter for generating branch parameter */
  uint64_t              sa_tags;   /**< Counter for generating tag parameters */

  char const           *sa_2543_tag; /**< Fixed tag added to @To when responding */

#if HAVE_SOFIA_SRESOLV
  sres_resolver_t      *sa_resolver; /**< DNS resolver */
#endif

  tport_t              *sa_tports;
  
  /* Default outbound proxy */
  url_t                *sa_default_proxy;

#if HAVE_SMIME
  sm_object_t          *sa_smime;
#else
  void                 *sa_smime;
#endif


  /** Request error mask */
  unsigned              sa_bad_req_mask;
  /** Response error mask */
  unsigned              sa_bad_resp_mask;

  /** Maximum size of incoming messages */
  size_t                sa_maxsize;
  
  /** Maximum size of proceeding queue */
  size_t                sa_max_proceeding;
  
  /** Maximum size of outgoing UDP requests */
  unsigned              sa_udp_mtu;

  /** SIP T1 - initial interval of retransmissions (500 ms) */
  unsigned              sa_t1;
  /** SIP T2 - maximum interval of retransmissions (4000 ms) */
  unsigned              sa_t2;
  /** SIP T4 - clear message time (5000 ms) */
  unsigned              sa_t4;

  /** SIP T1X64 - transaction lifetime (32 s) */
  unsigned              sa_t1x64;

  /** Progress timer - interval between provisional responses sent */
  unsigned              sa_progress;

  /** SIP timer C - interval between provisional responses receivedxs */
  unsigned              sa_timer_c;

  /** Graylisting period */
  unsigned              sa_graylist;
  /** Blacklisting period */
  unsigned              sa_blacklist;

    /** NTA is used to test packet drop */
  unsigned              sa_drop_prob : 10;
  /** NTA is acting as an User Agent server */
  unsigned              sa_is_a_uas : 1;
  /** Process requests outside dialog statelessly */
  unsigned              sa_is_stateless : 1;
  /** Let application provide @Via headers */
  unsigned              sa_user_via:1;
  /** Respond with "100 Trying" if application has not responded. */
  unsigned              sa_extra_100:1;
  /** The "100 Trying" provisional answers are passed to the application */
  unsigned              sa_pass_100:1;
  /** If true, a "408 Request Timeout" message is generated when outgoing
      request expires. */
  unsigned              sa_timeout_408:1;
  /** If true, a "408 Request Timeout" responses are passed to client. */
  unsigned              sa_pass_408:1;
  /** If true, a "482 Request Merged" response is sent to merged requests. */
  unsigned              sa_merge_482 : 1;
  /** If true, send a CANCEL to an INVITE without an provisional response. */
  unsigned              sa_cancel_2543 : 1;
  /** If true, reply with 487 response when a CANCEL is received. */
  unsigned              sa_cancel_487 : 1;
  /** If true, use unique tags. */
  unsigned              sa_tag_3261 : 1;
  /** If true, include 100rel in INVITE requests. */
  unsigned              sa_invite_100rel : 1;
  /** If true, insert @Timestamp in requests. */
  unsigned              sa_timestamp : 1;

  /** If true, transports support IPv4. */
  unsigned              sa_tport_ip4 : 1;
  /** If true, transports support IPv6. */
  unsigned              sa_tport_ip6 : 1;
  /** If true, transports support UDP. */
  unsigned              sa_tport_udp : 1;
  /** If true, transports support TCP. */
  unsigned              sa_tport_tcp : 1;
  /** If true, transports support SCTP. */
  unsigned              sa_tport_sctp : 1;
  /** If true, transports support TLS. */
  unsigned              sa_tport_tls : 1;

  /** If true, use NAPTR lookup */
  unsigned              sa_use_naptr : 1;
  /** If true, use SRV lookup */
  unsigned              sa_use_srv : 1;

  /** If true, transports use threadpool */
  unsigned              sa_tport_threadpool : 1;

  /** If true, use rport at client */
  unsigned              sa_rport:1;
  /** If true, use rport at server */
  unsigned              sa_server_rport:2;
  /** If true, use rport with tcp, too */
  unsigned              sa_tcp_rport:1;

  /** If true, automatically create compartments */
  unsigned              sa_auto_comp:1;

  /** Set when executing timer */
  unsigned              sa_in_timer:1;
  
  /** Set if application has set value for timer C */
  unsigned              sa_use_timer_c:1;

  unsigned              :0;

  /** Messages memory preload. */
  unsigned              sa_preload;

  /** Name of SigComp algorithm */
  char const           *sa_algorithm;
  /** Options for SigComp. */
  char const           *sa_sigcomp_options;
  char const* const    *sa_sigcomp_option_list;
  char const           *sa_sigcomp_option_free;

  nta_compressor_t     *sa_compressor;

  /** Resolving order (AAAA/A) */
  enum nta_res_order_e  sa_res_order;

  /** @MaxForwards */

  sip_max_forwards_t    sa_max_forwards[1];

  /* Statistics */
  struct {
    usize_t as_recv_msg;
    usize_t as_recv_request;
    usize_t as_recv_response;
    usize_t as_bad_message;
    usize_t as_bad_request;
    usize_t as_bad_response;
    usize_t as_drop_request;
    usize_t as_drop_response;
    usize_t as_client_tr;
    usize_t as_server_tr;
    usize_t as_dialog_tr;
    usize_t as_acked_tr;
    usize_t as_canceled_tr;
    usize_t as_trless_request;
    usize_t as_trless_to_tr;
    usize_t as_trless_response;
    usize_t as_trless_200;
    usize_t as_merged_request;
    usize_t as_sent_msg;
    usize_t as_sent_request;
    usize_t as_sent_response;
    usize_t as_retry_request;
    usize_t as_retry_response;
    usize_t as_recv_retry;
    usize_t as_tout_request;
    usize_t as_tout_response;
  }                  sa_stats[1];

  /** Hash of dialogs. */
  leg_htable_t          sa_dialogs[1];
  /** Default leg */
  nta_leg_t            *sa_default_leg;
  /** Hash of legs without dialogs. */
  leg_htable_t          sa_defaults[1];
  /** Hash table for outgoing transactions */
  outgoing_htable_t     sa_outgoing[1];
  nta_outgoing_t       *sa_default_outgoing;
  /** Hash table for incoming transactions */
  incoming_htable_t     sa_incoming[1]; 
  nta_incoming_t       *sa_default_incoming;

  /* Queues (states) for outgoing client transactions */
  struct {
    /** Queue for retrying client transactions */
    nta_outgoing_t   *re_list;
    nta_outgoing_t  **re_t1;	        /**< Special place for T1 timer */
    size_t            re_length;	/**< Length of sa_out.re_list */

    outgoing_queue_t  delayed[1]; 
    outgoing_queue_t  resolving[1]; 

    outgoing_queue_t  trying[1];	/* Timer F/E */
    outgoing_queue_t  completed[1];	/* Timer K */
    outgoing_queue_t  terminated[1];

    /* Special queues (states) for outgoing INVITE transactions */
    outgoing_queue_t  inv_calling[1];	/* Timer B/A */
    outgoing_queue_t  inv_proceeding[1]; /* Timer C */
    outgoing_queue_t  inv_completed[1];	/* Timer D */

    /* Temporary queue for transactions waiting to be freed */
    outgoing_queue_t *free;
  } sa_out;

  /* Queues (states) for incoming server transactions */
  struct {
    /** Queue for retransmitting response of server transactions */
    nta_incoming_t   *re_list;
    nta_incoming_t  **re_t1;	        /**< Special place for T1 timer */
    size_t            re_length;

    incoming_queue_t  proceeding[1];	/**< Request received */
    incoming_queue_t  preliminary[1];   /**< 100rel sent  */
    incoming_queue_t  completed[1];	/**< Final answer sent (non-invite). */
    incoming_queue_t  inv_completed[1];	/**< Final answer sent (INVITE). */
    incoming_queue_t  inv_confirmed[1];	/**< Final answer sent, ACK recvd. */
    incoming_queue_t  terminated[1];	/**< Terminated, ready to free. */
    incoming_queue_t  final_failed[1];   
  } sa_in;

  /* Special task for freeing memory */
  su_clone_r          sa_terminator;
};

struct nta_leg_s
{
  su_home_t         leg_home[1];
  hash_value_t      leg_hash;
  unsigned          leg_dialog : 1;
  unsigned          leg_stateless : 1;   /**< Process requests statelessly */
#ifdef NTA_STRICT_ROUTING
  unsigned          leg_contact_set : 1;
#else
  unsigned          leg_loose_route : 1; /**< Topmost route in set is LR */
#endif
  unsigned          leg_local_is_to : 1; /**< Backwards-compatibility. */
  unsigned          leg_tagged : 1; /**< Tagged after creation.
				     *
				     * Request missing To tag matches it
				     * even after tagging.
				     */
  unsigned:0;
  nta_request_f    *leg_callback;
  nta_leg_magic_t  *leg_magic;
  nta_agent_t      *leg_agent;
  /** Leg URL.
   *
   * This is the URL used to match incoming requests.
   */
  url_t const      *leg_url;
  char const       *leg_method;	/**< Method for this dialog. */

  uint32_t	    leg_seq;    /**< Sequence number for next transaction */
  uint32_t	    leg_rseq;   /**< Remote sequence number */
  sip_call_id_t	   *leg_id;	/**< Call ID */
  sip_from_t   	   *leg_remote;	/**< Remote address (@To/@From) */
  sip_to_t     	   *leg_local;	/**< Local address (@From/@To) */

  sip_route_t      *leg_route;  /**< @Route for outgoing requests. */
  sip_contact_t    *leg_target; /**< Remote destination (from @Contact). */
};

#define leg_has_id(leg) ((leg)->leg_id != NULL)

struct nta_incoming_s
{
  su_home_t            *irq_home;
  hash_value_t          irq_hash;
  nta_agent_t          *irq_agent;
  nta_ack_cancel_f     *irq_callback;
  nta_incoming_magic_t *irq_magic;

  /* Timeout/state queue */
  nta_incoming_t      **irq_prev;
  nta_incoming_t       *irq_next;
  incoming_queue_t     *irq_queue;
  
  /* Retry queue */
  nta_incoming_t      **irq_rprev;
  nta_incoming_t       *irq_rnext;

  sip_method_t        	irq_method;
  sip_request_t        *irq_rq;
  sip_from_t           *irq_from;
  sip_to_t             *irq_to;
  char const           *irq_tag;
  sip_cseq_t           *irq_cseq;
  sip_call_id_t        *irq_call_id;
  sip_via_t            *irq_via;
  sip_record_route_t   *irq_record_route;
  char const           *irq_branch;

  uint32_t              irq_rseq;

  sip_timestamp_t      *irq_timestamp;
  su_time_t             irq_received;

  uint32_t       	irq_timeout;    /**< Timer H, I, J */
  uint32_t       	irq_retry;      /**< Timer G */
  unsigned short      	irq_interval;	/**< Next timer  */

  short               	irq_status;

  unsigned              irq_retries : 8;
  unsigned              irq_default : 1;    /**< Default transaction */
  unsigned              irq_canceled : 1;   /**< Transaction is canceled */
  unsigned              irq_completed : 1;  /**< Transaction is completed */
  unsigned              irq_confirmed : 1;  /**< Response has been acked */
  unsigned              irq_terminated :1;  /**< Transaction is terminated */
  unsigned              irq_final_failed:1; /**< Sending final response failed */
  unsigned              irq_destroyed :1;   /**< Transaction is destroyed */
  unsigned              irq_in_callback:1;  /**< Callback is being invoked */
  unsigned              irq_reliable_tp:1;  /**< Transport is reliable */
  unsigned              irq_sigcomp_zap:1;  /**< Reset SigComp */
  unsigned              irq_must_100rel:1;  /**< 100rel is required */
  unsigned              irq_tag_set:1;      /**< Tag is not from request */
  unsigned              :0;

  tp_name_t             irq_tpn[1];
  tport_t              *irq_tport;
  struct sigcomp_compartment *irq_cc;
  msg_t		       *irq_request;
  msg_t		       *irq_request2;       /**< ACK/CANCEL */
  msg_t		       *irq_response;

  nta_reliable_t       *irq_reliable;       /**< List of reliable responses */
};

struct nta_reliable_s
{
  nta_reliable_t       *rel_next;
  nta_incoming_t       *rel_irq;
  nta_prack_f          *rel_callback;
  nta_reliable_magic_t *rel_magic;
  uint32_t              rel_rseq;
  unsigned short        rel_status;
  unsigned              rel_pracked : 1;
  unsigned              rel_precious : 1;
  msg_t                *rel_response;
  msg_t                *rel_unsent;
};

typedef struct sipdns_resolver sipdns_resolver_t;

struct nta_outgoing_s
{
  hash_value_t          orq_hash;    /**< Hash value */
  nta_agent_t          *orq_agent;
  nta_response_f       *orq_callback;
  nta_outgoing_magic_t *orq_magic;

  /* Timeout/state queue */
  nta_outgoing_t      **orq_prev;
  nta_outgoing_t       *orq_next;
  outgoing_queue_t     *orq_queue;
  
  /* Retry queue */
  nta_outgoing_t      **orq_rprev;
  nta_outgoing_t       *orq_rnext;

  sip_method_t        	orq_method;
  char const           *orq_method_name;
  url_t const          *orq_url;        /**< Original RequestURI */

  sip_from_t const     *orq_from;
  sip_to_t const       *orq_to;
  char const           *orq_tag;        /**< Tag from final response. */

  sip_cseq_t const     *orq_cseq;
  sip_call_id_t const  *orq_call_id;

  msg_t		       *orq_request;
  msg_t                *orq_response;

  su_time_t             orq_sent;       /**< When request was sent? */
  unsigned              orq_delay;      /**< RTT estimate */

  uint32_t		orq_retry;	/**< Timer A, E */
  uint32_t		orq_timeout;	/**< Timer B, D, F, K */

  unsigned short      	orq_interval;	/**< Next timer A/E */

  unsigned short      	orq_status;
  unsigned char         orq_retries;    /**< Number of tries this far */
  unsigned orq_default : 1;	        /**< This is default transaction */
  unsigned orq_inserted : 1;
  unsigned orq_resolved : 1;
  unsigned orq_prepared : 1; /**< outgoing_prepare() called */
  unsigned orq_canceled : 1;
  unsigned orq_terminated : 1;
  unsigned orq_destroyed : 1;
  unsigned orq_completed : 1;
  unsigned orq_delayed : 1;
  unsigned orq_stripped_uri : 1;
  unsigned orq_user_tport : 1;	/**< Application provided tport - don't retry */
  unsigned orq_try_tcp_instead : 1;
  unsigned orq_try_udp_instead : 1;
  unsigned orq_reliable : 1; /**< Transport is reliable */
  unsigned orq_ack_error : 1; /**< ACK is sent by NTA */
  /* Attributes */
  unsigned orq_user_via : 1;
  unsigned orq_stateless : 1;
  unsigned orq_pass_100 : 1;
  unsigned orq_sigcomp_new:1;	/**< Create compartment if needed */
  unsigned orq_sigcomp_zap:1;	/**< Reset SigComp after completing */
  unsigned orq_must_100rel : 1;
  unsigned orq_timestamp : 1;	/**< Insert @Timestamp header. */
  unsigned orq_100rel:1;	/**< Support 100rel */
  unsigned : 0;	/* pad */

#if HAVE_SOFIA_SRESOLV
  sipdns_resolver_t    *orq_resolver;
#endif
  enum nta_res_order_e  orq_res_order;  /**< AAAA/A first? */

  url_t                *orq_route;      /**< Route URL */
  tp_name_t             orq_tpn[1];     /**< Where to send request */
  char const           *orq_scheme;     /**< Transport URL type */

  tport_t              *orq_tport;
  struct sigcomp_compartment *orq_cc;
  tagi_t               *orq_tags;       /**< Tport tag items */
  int                   orq_pending;    /**< Request is pending in tport */

  char const           *orq_branch;	/**< Transaction branch */
  char const           *orq_via_branch;	/**< @Via branch */

  int                  *orq_status2b;   /**< Delayed response */

  nta_outgoing_t       *orq_cancel;     /**< CANCEL transaction */

  uint32_t              orq_rseq;       /**< Latest incoming rseq */
};

/* Virtual function table for plugging in SigComp */
typedef struct
{
  int ncv_size;
  char const *ncv_name;

  nta_compressor_t *(*ncv_init_agent)(nta_agent_t *sa, 
				     char const * const *options);

  void (*ncv_deinit_agent)(nta_agent_t *sa, nta_compressor_t *);

  struct sigcomp_compartment *(*ncv_compartment)(nta_agent_t *sa,
						 tport_t *tport, 
						 nta_compressor_t *msc,
						 tp_name_t const *tpn,
						 char const * const *options,
						 int new_if_needed);

  int (*ncv_accept_compressed)(nta_agent_t *sa,
			       nta_compressor_t *msc,
			       tport_compressor_t *sc,
			       msg_t *msg,
			       struct sigcomp_compartment *cc);

  int (*ncv_close_compressor)(nta_agent_t *sa,
			      struct sigcomp_compartment *cc);
  int (*ncv_zap_compressor)(nta_agent_t *sa,
			    struct sigcomp_compartment *cc);

  struct sigcomp_compartment *(*ncv_compartment_ref)
    (struct sigcomp_compartment *);

  void (*ncv_compartment_unref)(struct sigcomp_compartment *);
 
} nta_compressor_vtable_t;

extern nta_compressor_vtable_t *nta_compressor_vtable;

SOFIAPUBFUN nta_compressor_t *nta_agent_init_sigcomp(nta_agent_t *sa);
SOFIAPUBFUN void nta_agent_deinit_sigcomp(nta_agent_t *sa);

/* ====================================================================== */
/* Debug log settings */

#define SU_LOG   nta_log

#ifdef SU_DEBUG_H
#error <su_debug.h> included directly.
#endif
#include <sofia-sip/su_debug.h>
SOFIAPUBVAR su_log_t nta_log[];

SOFIA_END_DECLS

#endif /* NTA_INTERNAL_H */
