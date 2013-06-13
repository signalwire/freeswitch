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

/**@CFILE nta.c
 * @brief Sofia SIP Transaction API implementation
 *
 * This source file has been divided into sections as follows:
 * 1) agent
 * 2) tport handling
 * 3) dispatching messages received from network
 * 4) message creation and message utility functions
 * 5) stateless operation
 * 6) dialogs (legs)
 * 7) server transactions (incoming)
 * 8) client transactions (outgoing)
 * 9) resolving URLs for client transactions
 * 10) 100rel reliable responses (reliable)
 * 11) SigComp handling and public transport interface
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Jun 13 02:57:51 2000 ppessi
 *
 * @sa
 * @RFC3261, @RFC4320
 */

#include "config.h"

#include <sofia-sip/su_string.h>

/** @internal SU message argument structure type */
#define SU_MSG_ARG_T   union sm_arg_u
/** @internal SU timer argument pointer type */
#define SU_TIMER_ARG_T struct nta_agent_s

#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su.h>
#include <sofia-sip/su_time.h>
#include <sofia-sip/su_wait.h>
#include <sofia-sip/su_tagarg.h>

#include <sofia-sip/base64.h>
#include <sofia-sip/su_uniqueid.h>

#include <sofia-sip/sip.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_util.h>
#include <sofia-sip/sip_status.h>

#include <sofia-sip/hostdomain.h>
#include <sofia-sip/url_tag.h>

#include <sofia-sip/msg_addr.h>
#include <sofia-sip/msg_parser.h>
#include <sofia-sip/htable.h>

/* Resolver context type */
#define SRES_CONTEXT_T    nta_outgoing_t

/* We are customer of tport_t */
#define TP_AGENT_T        nta_agent_t
#define TP_MAGIC_T        sip_via_t
#define TP_CLIENT_T       nta_outgoing_t

#include "nta_internal.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>

/* From AM_INIT/AC_INIT in our "config.h" */
char const nta_version[] = PACKAGE_VERSION;

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
static char const __func__[] = "nta";
#endif

#ifndef _MSC_VER
#define NONE ((void *)-1)
#else
#define NONE ((void *)(INT_PTR)-1)
#endif
/* ------------------------------------------------------------------------- */

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

struct nta_agent_s
{
  su_home_t             sa_home[1];
  su_root_t            *sa_root;
  su_timer_t           *sa_timer;
  nta_agent_magic_t    *sa_magic;
  nta_message_f        *sa_callback;

  nta_update_magic_t   *sa_update_magic;
  nta_update_tport_f   *sa_update_tport;

  nta_error_magic_t   *sa_error_magic;
  nta_error_tport_f   *sa_error_tport;

  su_time_t             sa_now;	 /**< Timestamp in microsecond resolution. */
  uint32_t              sa_next; /**< Timestamp for next agent_timer. */
  uint32_t              sa_millisec; /**< Timestamp in milliseconds. */

  msg_mclass_t const   *sa_mclass;
  uint32_t sa_flags;		/**< SIP message flags */
  unsigned sa_preload;		/**< Memory preload for SIP messages. */

  tport_t              *sa_tports;
  sip_contact_t        *sa_contact;
  sip_via_t            *sa_vias;   /**< @Via headers for all transports */
  sip_via_t            *sa_public_vias;   /**< @Vias for public transports */
  sip_contact_t        *sa_aliases;/**< List of aliases for agent */

  uint64_t              sa_branch; /**< Generator for branch parameters */
  uint64_t              sa_tags;   /**< Generator for tag parameters */

#if HAVE_SOFIA_SRESOLV
  sres_resolver_t      *sa_resolver; /**< DNS resolver */
  enum nta_res_order_e  sa_res_order;  /** Resolving order (AAAA/A) */
#endif

  url_t   *sa_default_proxy;	/**< Default outbound proxy */
  unsigned sa_bad_req_mask;     /**< Request error mask */
  unsigned sa_bad_resp_mask;	/**< Response error mask */
  usize_t  sa_maxsize;		/**< Maximum size of incoming messages */
  usize_t  sa_max_proceeding;	/**< Maximum size of proceeding queue */

  unsigned sa_udp_mtu;		/**< Maximum size of outgoing UDP requests */

  unsigned sa_t1;  /**< SIP T1 - initial retransmit interval (500 ms) */
  unsigned sa_t2;  /**< SIP T2 - maximum retransmit interval (4000 ms) */
  unsigned sa_t4;  /**< SIP T4 - clear message time (5000 ms) */


  unsigned sa_t1x64; /**< SIP T1X64 - transaction lifetime (32 s) */

  unsigned sa_progress;		/**< Progress timer.
				   Interval between retransmitting
				   provisional responses. */

  unsigned sa_timer_c;		/**< SIP timer C.
				   Maximum interval between receiving
				   provisional responses. */

  unsigned sa_graylist;		/**< Graylisting period */
  unsigned sa_blacklist;	/**< Blacklisting period */

  unsigned sa_drop_prob : 10;	/**< NTA is used to test packet drop */
  unsigned sa_is_a_uas : 1;	/**< NTA is acting as an User Agent server */
  unsigned sa_is_stateless : 1;	/**< Process requests statelessly
				 *   unless they match existing dialog.
				 */
  unsigned sa_user_via:1;	/**< Let application provide @Via headers */
  unsigned sa_extra_100:1;	/**< Allow NTA to return "100 Trying" response
				 * even if application has not responded.
				 */
  unsigned sa_pass_100:1;	/**< Pass the "100 Trying"
				 * provisional responses to the application
				 */
  unsigned sa_timeout_408:1;	/**< A "408 Request Timeout" message
				 * is generated when outgoing request expires.
				 */
  unsigned sa_pass_408:1;	/**< A "408 Request Timeout" responses
				 * are passed to client.
				 */
  unsigned sa_merge_482 : 1;	/**< A "482 Request Merged" response is returned
				 * to merged requests.
				 */
  unsigned sa_cancel_2543 : 1;  /**< Send a CANCEL to an INVITE without
				 * waiting for an provisional response.
				 */
  unsigned sa_cancel_487 : 1;	/**< Return 487 response automatically when
				 * a CANCEL is received.
				 */

  unsigned sa_invite_100rel:1;	/**< Include 100rel in INVITE requests. */
  unsigned sa_timestamp : 1;	/**< Insert @Timestamp in requests. */

  unsigned sa_tport_ip4 : 1;	/**< Transports support IPv4. */
  unsigned sa_tport_ip6 : 1;	/**< Transports support IPv6. */
  unsigned sa_tport_udp : 1;	/**< Transports support UDP. */
  unsigned sa_tport_tcp : 1;	/**< Transports support TCP. */
  unsigned sa_tport_sctp : 1;	/**< Transports support SCTP. */
  unsigned sa_tport_tls : 1;	/**< Transports support TLS. */
  unsigned sa_tport_ws : 1;	    /**< Transports support WS. */
  unsigned sa_tport_wss : 1;	    /**< Transports support WSS. */

  unsigned sa_use_naptr : 1;	/**< Use NAPTR lookup */
  unsigned sa_use_srv : 1;	/**< Use SRV lookup */

  unsigned sa_srv_503 : 1;     /**<  SRV: choice another destination on 503 RFC 3263 */
  
  unsigned sa_tport_threadpool:1; /**< Transports use threadpool */

  unsigned sa_rport:1;		/**< Use rport at client */
  unsigned sa_server_rport:2;	/**< Use rport at server */
  unsigned sa_tcp_rport:1;	/**< Use rport with tcp, too */
  unsigned sa_tls_rport:1;	/**< Use rport with tls, too */

  unsigned sa_auto_comp:1;	/**< Automatically create compartments */
  unsigned sa_in_timer:1;	/**< Set when executing timers */
  unsigned sa_use_timer_c:1;	/**< Application has set value for timer C */

  unsigned :0;

#if HAVE_SMIME
  sm_object_t          *sa_smime;
#else
  void                 *sa_smime;
#endif

  /** @MaxForwards */
  sip_max_forwards_t    sa_max_forwards[1];

  /** Name of SigComp algorithm */
  char const           *sa_algorithm;
  /** Options for SigComp. */
  char const           *sa_sigcomp_options;
  char const* const    *sa_sigcomp_option_list;
  char const           *sa_sigcomp_option_free;

  nta_compressor_t     *sa_compressor;

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

    outgoing_queue_t  trying[1];	/* Timer F / Timer E */
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
    size_t            re_length;        /**< Length of sa_in.re_list */

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

  unsigned leg_dialog : 1;
  unsigned leg_stateless : 1;   /**< Process requests statelessly */
#ifdef NTA_STRICT_ROUTING
  unsigned leg_contact_set : 1;
#else
  unsigned leg_loose_route : 1; /**< Topmost route in set is LR */
#endif
  unsigned leg_route_set : 1;	/**< Route set has been saved */
  unsigned leg_local_is_to : 1; /**< Backwards-compatibility. */
  unsigned leg_tagged : 1;	/**< Tagged after creation.
				 *
				 * Request missing @To tag matches
				 * a tagged leg even after tagging.
				 */
  unsigned:0;
  nta_request_f    *leg_callback;
  nta_leg_magic_t  *leg_magic;
  nta_agent_t      *leg_agent;

  url_t const      *leg_url;	/**< Match incoming requests. */
  char const       *leg_method;	/**< Match incoming requests. */

  uint32_t	    leg_seq;    /**< Sequence number for next transaction */
  uint32_t	    leg_rseq;   /**< Remote sequence number */
  sip_call_id_t	   *leg_id;	/**< Call ID */
  sip_from_t   	   *leg_remote;	/**< Remote address (@To/@From) */
  sip_to_t     	   *leg_local;	/**< Local address (@From/@To) */

  sip_route_t      *leg_route;  /**< @Route for outgoing requests. */
  sip_contact_t    *leg_target; /**< Remote destination (from @Contact). */
};

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

  unsigned irq_retries:8;
  unsigned irq_default:1;	/**< Default transaction */
  unsigned irq_canceled:1;	/**< Transaction is canceled */
  unsigned irq_completed:1;	/**< Transaction is completed */
  unsigned irq_confirmed:1;	/**< Response has been acked */
  unsigned irq_terminated:1;	/**< Transaction is terminated */
  unsigned irq_final_failed:1;	/**< Sending final response failed */
  unsigned irq_destroyed :1;	/**< Transaction is destroyed */
  unsigned irq_in_callback:1;	/**< Callback is being invoked */
  unsigned irq_reliable_tp:1;	/**< Transport is reliable */
  unsigned irq_sigcomp_zap:1;	/**< Reset SigComp */
  unsigned irq_must_100rel:1;	/**< 100rel is required */
  unsigned irq_extra_100:1;	/**< 100 Trying should be sent */
  unsigned irq_tag_set:1;	/**< Tag is not from request */
  unsigned :0;

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
  unsigned              rel_pracked:1;
  unsigned              rel_precious:1;
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

  unsigned orq_default:1;	        /**< This is default transaction */
  unsigned orq_inserted:1;
  unsigned orq_resolved:1;
  unsigned orq_via_added:1;
  unsigned orq_prepared:1;
  unsigned orq_canceled:1;
  unsigned orq_terminated:1;
  unsigned orq_destroyed:1;
  unsigned orq_completed:1;
  unsigned orq_delayed:1;
  unsigned orq_user_tport:1;	/**< Application provided tport - don't retry */
  unsigned orq_try_tcp_instead:1;
  unsigned orq_try_udp_instead:1;
  unsigned orq_reliable:1; /**< Transport is reliable */

  unsigned orq_forked:1;	/**< Tagged fork  */

  /* Attributes */
  unsigned orq_sips:1;
  unsigned orq_uas:1;		/**< Running this transaction as UAS */
  unsigned orq_user_via:1;
  unsigned orq_stateless:1;
  unsigned orq_pass_100:1;
  unsigned orq_sigcomp_new:1;	/**< Create compartment if needed */
  unsigned orq_sigcomp_zap:1;	/**< Reset SigComp after completing */
  unsigned orq_must_100rel:1;
  unsigned orq_timestamp:1;	/**< Insert @Timestamp header. */
  unsigned orq_100rel:1;	/**< Support 100rel */
  unsigned:0;	/* pad */

#if HAVE_SOFIA_SRESOLV
  sipdns_resolver_t    *orq_resolver;
#endif
  url_t                *orq_route;      /**< Route URL */
  tp_name_t             orq_tpn[1];     /**< Where to send request */

  tport_t              *orq_tport;
  struct sigcomp_compartment *orq_cc;
  tagi_t               *orq_tags;       /**< Tport tag items */

  char const           *orq_branch;	/**< Transaction branch */
  char const           *orq_via_branch;	/**< @Via branch */

  int                  *orq_status2b;   /**< Delayed response */

  nta_outgoing_t       *orq_cancel;     /**< Delayed CANCEL transaction */

  nta_outgoing_t       *orq_forking;    /**< Untagged transaction */
  nta_outgoing_t       *orq_forks;	/**< Tagged transactions */
  uint32_t              orq_rseq;       /**< Latest incoming rseq */
  int                   orq_pending;    /**< Request is pending in tport */
};

/* ------------------------------------------------------------------------- */

/* Internal tags */

/* Delay sending of request */
#define NTATAG_DELAY_SENDING(x) ntatag_delay_sending, tag_bool_v((x))
#define NTATAG_DELAY_SENDING_REF(x) \
ntatag_delay_sending_ref, tag_bool_vr(&(x))

extern tag_typedef_t ntatag_delay_sending;
extern tag_typedef_t ntatag_delay_sending_ref;

/* Allow sending incomplete responses */
#define NTATAG_INCOMPLETE(x) ntatag_incomplete, tag_bool_v((x))
#define NTATAG_INCOMPLETE_REF(x) \
ntatag_incomplete_ref, tag_bool_vr(&(x))

extern tag_typedef_t ntatag_incomplete;
extern tag_typedef_t ntatag_incomplete_ref;

nta_compressor_vtable_t *nta_compressor_vtable = NULL;

/* Agent */
static int agent_tag_init(nta_agent_t *self);
static int agent_timer_init(nta_agent_t *agent);
static void agent_timer(su_root_magic_t *rm, su_timer_t *, nta_agent_t *);
static int agent_launch_terminator(nta_agent_t *agent);
static void agent_kill_terminator(nta_agent_t *agent);
static int agent_set_params(nta_agent_t *agent, tagi_t *tags);
static void agent_set_udp_params(nta_agent_t *self, usize_t udp_mtu);
static int agent_get_params(nta_agent_t *agent, tagi_t *tags);

/* Transport interface */
static sip_via_t const *agent_tport_via(tport_t *tport);
static int outgoing_insert_via(nta_outgoing_t *orq, sip_via_t const *);
static int nta_tpn_by_via(tp_name_t *tpn, sip_via_t const *v, int *using_rport);

static msg_t *nta_msg_create_for_transport(nta_agent_t *agent, int flags,
					   char const data[], usize_t dlen,
					   tport_t const *tport,
					   tp_client_t *via);

static int complete_response(msg_t *response,
			     int status, char const *phrase,
			     msg_t *request);

static int mreply(nta_agent_t *agent,
		  msg_t *reply,
		  int status, char const *phrase,
		  msg_t *req_msg,
		  tport_t *tport,
		  int incomplete,
		  int sdwn_after,
		  char const *to_tag,
		  tag_type_t tag, tag_value_t value, ...);

#define IF_SIGCOMP_TPTAG_COMPARTMENT(cc)     TAG_IF(cc && cc != NONE, TPTAG_COMPARTMENT(cc)),
#define IF_SIGCOMP_TPTAG_COMPARTMENT_REF(cc) TPTAG_COMPARTMENT_REF(cc),

struct sigcomp_compartment;

struct sigcomp_compartment *
nta_compartment_ref(struct sigcomp_compartment *cc);

static
struct sigcomp_compartment *
agent_compression_compartment(nta_agent_t *sa, tport_t *tp, tp_name_t const *tpn,
			      int new_if_needed);

static
int agent_accept_compressed(nta_agent_t *sa, msg_t *msg,
			    struct sigcomp_compartment *cc);

static int agent_close_compressor(nta_agent_t *sa,
				  struct sigcomp_compartment *cc);

static int agent_zap_compressor(nta_agent_t *sa,
				struct sigcomp_compartment *cc);


static char const * stateful_branch(su_home_t *home, nta_agent_t *);
static char const * stateless_branch(nta_agent_t *, msg_t *, sip_t const *,
				    tp_name_t const *tp);

#define NTA_BRANCH_PRIME SU_U64_C(0xB9591D1C361C6521)
#define NTA_TAG_PRIME    SU_U64_C(0xB9591D1C361C6521)

#ifndef UINT32_MAX
#define UINT32_MAX (0xffffffffU)
#endif

HTABLE_PROTOS_WITH(leg_htable, lht, nta_leg_t, size_t, hash_value_t);
static nta_leg_t *leg_find(nta_agent_t const *sa,
			   char const *method_name,
			   url_t const *request_uri,
			   sip_call_id_t const *i,
			   char const *from_tag,
			   char const *to_tag);
static nta_leg_t *dst_find(nta_agent_t const *sa, url_t const *u0,
			   char const *method);
static void leg_recv(nta_leg_t *, msg_t *, sip_t *, tport_t *);
static void leg_free(nta_agent_t *sa, nta_leg_t *leg);

#define NTA_HASH(i, cs) ((i)->i_hash + 26839U * (uint32_t)(cs))

HTABLE_PROTOS_WITH(incoming_htable, iht, nta_incoming_t, size_t, hash_value_t);
static nta_incoming_t *incoming_create(nta_agent_t *agent,
				       msg_t *request,
				       sip_t *sip,
				       tport_t *tport,
				       char const *tag);
static int incoming_callback(nta_leg_t *leg, nta_incoming_t *irq, sip_t *sip);
static void incoming_free(nta_incoming_t *irq);
su_inline void incoming_cut_off(nta_incoming_t *irq);
su_inline void incoming_reclaim(nta_incoming_t *irq);
static void incoming_queue_init(incoming_queue_t *,
				unsigned timeout);
static void incoming_queue_adjust(nta_agent_t *sa,
				  incoming_queue_t *queue,
				  unsigned timeout);

static nta_incoming_t *incoming_find(nta_agent_t const *agent,
				     sip_t const *sip,
				     sip_via_t const *v,
				     nta_incoming_t **merge,
				     nta_incoming_t **ack,
				     nta_incoming_t **cancel);
static int incoming_reply(nta_incoming_t *irq, msg_t *msg, sip_t *sip);
su_inline int incoming_recv(nta_incoming_t *irq, msg_t *msg, sip_t *sip,
				tport_t *tport);
su_inline int incoming_ack(nta_incoming_t *irq, msg_t *msg, sip_t *sip,
			       tport_t *tport);
su_inline int incoming_cancel(nta_incoming_t *irq, msg_t *msg, sip_t *sip,
				  tport_t *tport);
static void request_merge(nta_agent_t *,
			  msg_t *msg, sip_t *sip, tport_t *tport,
			  char const *to_tag);
su_inline int incoming_timestamp(nta_incoming_t *, msg_t *, sip_t *);
static void _nta_incoming_timer(nta_agent_t *);

static nta_reliable_t *reliable_mreply(nta_incoming_t *,
				       nta_prack_f *, nta_reliable_magic_t *,
				       msg_t *, sip_t *);
static int reliable_send(nta_incoming_t *, nta_reliable_t *, msg_t *, sip_t *);
static int reliable_final(nta_incoming_t *irq, msg_t *msg, sip_t *sip);
static msg_t *reliable_response(nta_incoming_t *irq);
static nta_reliable_t *reliable_find(nta_agent_t const *, sip_t const *);
static int reliable_recv(nta_reliable_t *rel, msg_t *, sip_t *, tport_t *);
static void reliable_flush(nta_incoming_t *irq);
static void reliable_timeout(nta_incoming_t *irq, int timeout);

HTABLE_PROTOS_WITH(outgoing_htable, oht, nta_outgoing_t, size_t, hash_value_t);
static nta_outgoing_t *outgoing_create(nta_agent_t *agent,
				       nta_response_f *callback,
				       nta_outgoing_magic_t *magic,
				       url_string_t const *route_url,
				       tp_name_t const *tpn,
				       msg_t *msg,
				       tag_type_t tag, tag_value_t value, ...);
static void outgoing_queue_init(outgoing_queue_t *,
				unsigned timeout);
static void outgoing_queue_adjust(nta_agent_t *sa,
				  outgoing_queue_t *queue,
				  unsigned timeout);
static void outgoing_free(nta_outgoing_t *orq);
su_inline void outgoing_cut_off(nta_outgoing_t *orq);
su_inline void outgoing_reclaim(nta_outgoing_t *orq);
static nta_outgoing_t *outgoing_find(nta_agent_t const *sa,
				     msg_t const *msg,
				     sip_t const *sip,
				     sip_via_t const *v);
static int outgoing_recv(nta_outgoing_t *orq, int status, msg_t *, sip_t *);
static void outgoing_default_recv(nta_outgoing_t *, int, msg_t *, sip_t *);
static void _nta_outgoing_timer(nta_agent_t *);
static int outgoing_recv_reliable(nta_outgoing_t *orq, msg_t *msg, sip_t *sip);

/* Internal message passing */
union sm_arg_u {
  struct outgoing_recv_s {
    nta_outgoing_t *orq;
    msg_t          *msg;
    sip_t          *sip;
    int             status;
  } a_outgoing_recv[1];

  incoming_queue_t a_incoming_queue[1];
  outgoing_queue_t a_outgoing_queue[1];
};

/* Global module data */

/**@var char const NTA_DEBUG[];
 *
 * Environment variable determining the default debug log level.
 *
 * The NTA_DEBUG environment variable is used to determine the default
 * debug logging level. The normal level is 3.
 *
 * @sa <sofia-sip/su_debug.h>, #su_log_global, #SOFIA_DEBUG
 */
#ifdef DOXYGEN
extern char const NTA_DEBUG[]; /* dummy declaration for Doxygen */
#endif

#ifndef SU_DEBUG
#define SU_DEBUG 3
#endif

/**Debug log for @b nta module.
 *
 * The nta_log is the log object used by @b nta module. The level of
 * nta_log is set using #NTA_DEBUG environment variable.
 */
su_log_t nta_log[] = { SU_LOG_INIT("nta", "NTA_DEBUG", SU_DEBUG) };

/* ====================================================================== */
/* 1) Agent */

/**
 * Create an NTA agent object.
 *
 * Create an NTA agent object.  The agent
 * object creates and binds a server socket with address specified in @e url.
 * If the @e host portion of the @e url is @c "*", the agent listens to all
 * addresses available on the host.
 *
 * When a message is received, the agent object parses it.  If the result is
 * a valid SIP message, the agent object passes the message to the
 * application by invoking the nta_message_f @e callback function.
 *
 * @note
 * The @e url can be either parsed url (of type url_t ()), or a valid
 * SIP URL as a string.
 *
 * @note
 * If @e url is @c NULL, the default @e url @c "sip:*" is used.
 * @par
 * If @e url is @c NONE (iow, (void*)-1), no server sockets are bound.
 * @par
 * If @p transport parameters are specified in @a url, agent uses only
 * specified transport type.
 *
 * @par
 * If an @p maddr parameter is specified in @e url, agent binds to the
 * specified address, but uses @e host part of @e url when it generates
 * @Contact and @Via headers. The @p maddr parameter is also included,
 * unless it equals to @c INADDR_ANY (@p 0.0.0.0 or @p [::]).
 *
 * @param root          pointer to a su_root_t used for synchronization
 * @param contact_url   URL that agent uses to bind the server sockets
 * @param callback      pointer to callback function
 * @param magic         pointer to user data
 * @param tag,value,... tagged arguments
 *
 * @TAGS
 * NTATAG_ALIASES(),
 * NTATAG_BAD_REQ_MASK(), NTATAG_BAD_RESP_MASK(), NTATAG_BLACKLIST(),
 * NTATAG_CANCEL_2543(), NTATAG_CANCEL_487(), NTATAG_CLIENT_RPORT(),
 * NTATAG_DEBUG_DROP_PROB(), NTATAG_DEFAULT_PROXY(),
 * NTATAG_EXTRA_100(), NTATAG_GRAYLIST(),
 * NTATAG_MAXSIZE(), NTATAG_MAX_FORWARDS(), NTATAG_MERGE_482(), NTATAG_MCLASS()
 * NTATAG_PASS_100(), NTATAG_PASS_408(), NTATAG_PRELOAD(), NTATAG_PROGRESS(),
 * NTATAG_REL100(),
 * NTATAG_SERVER_RPORT(),
 * NTATAG_SIPFLAGS(),
 * NTATAG_SIP_T1X64(), NTATAG_SIP_T1(), NTATAG_SIP_T2(), NTATAG_SIP_T4(),
 * NTATAG_STATELESS(),
 * NTATAG_TAG_3261(), NTATAG_TCP_RPORT(), NTATAG_TIMEOUT_408(),
 * NTATAG_TLS_RPORT(),
 * NTATAG_TIMER_C(), NTATAG_MAX_PROCEEDING(),
 * NTATAG_UA(), NTATAG_UDP_MTU(), NTATAG_USER_VIA(),
 * NTATAG_USE_NAPTR(), NTATAG_USE_SRV() and NTATAG_USE_TIMESTAMP().
 *
 * @note The value from following tags are stored, but they currently do nothing:
 * NTATAG_SIGCOMP_ALGORITHM(), NTATAG_SIGCOMP_OPTIONS(), NTATAG_SMIME()
 *
 * @note It is possible to provide @c (url_string_t*)-1 as @a contact_url.
 * In that case, no server sockets are bound.
 *
 * @retval handle to the agent when successful,
 * @retval NULL upon an error.
 *
 * @sa NUTAG_
 */
nta_agent_t *nta_agent_create(su_root_t *root,
			      url_string_t const *contact_url,
			      nta_message_f *callback,
			      nta_agent_magic_t *magic,
			      tag_type_t tag, tag_value_t value, ...)
{
  nta_agent_t *agent;
  ta_list ta;

  if (root == NULL)
    return su_seterrno(EINVAL), NULL;

  ta_start(ta, tag, value);

  if ((agent = su_home_new(sizeof(*agent)))) {
    unsigned timer_c = 0, timer_d = 32000;

    agent->sa_root = root;
    agent->sa_callback = callback;
    agent->sa_magic = magic;
    agent->sa_flags = MSG_DO_CANONIC;

    agent->sa_maxsize         = 2 * 1024 * 1024; /* 2 MB */
    agent->sa_bad_req_mask    =
      /*
       * Bit-wise not of these - what is left is suitable for UAs with
       * 100rel, timer, events, publish
       */
      (unsigned) ~(sip_mask_response | sip_mask_proxy | sip_mask_registrar |
		   sip_mask_pref | sip_mask_privacy);
    agent->sa_bad_resp_mask   =
      (unsigned) ~(sip_mask_request | sip_mask_proxy | sip_mask_registrar |
		   sip_mask_pref | sip_mask_privacy);
    agent->sa_t1 	      = NTA_SIP_T1;
    agent->sa_t2 	      = NTA_SIP_T2;
    agent->sa_t4              = NTA_SIP_T4;
    agent->sa_t1x64 	      = 64 * NTA_SIP_T1;
    agent->sa_timer_c         = 185 * 1000;
    agent->sa_graylist        = 600;
    agent->sa_drop_prob       = 0;
    agent->sa_is_a_uas        = 0;
    agent->sa_progress        = 60 * 1000;
    agent->sa_user_via        = 0;
    agent->sa_extra_100       = 0;
    agent->sa_pass_100        = 0;
    agent->sa_timeout_408     = 1;
    agent->sa_pass_408        = 0;
    agent->sa_merge_482       = 0;
    agent->sa_cancel_2543     = 0;
    agent->sa_cancel_487      = 1;
    agent->sa_invite_100rel   = 0;
    agent->sa_timestamp       = 0;
    agent->sa_use_naptr       = 1;
    agent->sa_use_srv         = 1;
    agent->sa_srv_503         = 1;
    agent->sa_auto_comp       = 0;
    agent->sa_server_rport    = 1;

    /* RFC 3261 section 8.1.1.6 */
    sip_max_forwards_init(agent->sa_max_forwards);

    if (getenv("SIPCOMPACT"))
      agent->sa_flags |= MSG_DO_COMPACT;

    agent_set_params(agent, ta_args(ta));

    if (agent->sa_mclass == NULL)
      agent->sa_mclass = sip_default_mclass();

    agent->sa_in.re_t1 = &agent->sa_in.re_list;

    incoming_queue_init(agent->sa_in.proceeding, 0);
    incoming_queue_init(agent->sa_in.preliminary, agent->sa_t1x64); /* P1 */
    incoming_queue_init(agent->sa_in.inv_completed, agent->sa_t1x64); /* H */
    incoming_queue_init(agent->sa_in.inv_confirmed, agent->sa_t4); /* I */
    incoming_queue_init(agent->sa_in.completed, agent->sa_t1x64); /* J */
    incoming_queue_init(agent->sa_in.terminated, 0);
    incoming_queue_init(agent->sa_in.final_failed, 0);

    agent->sa_out.re_t1 = &agent->sa_out.re_list;

    if (agent->sa_use_timer_c || !agent->sa_is_a_uas)
      timer_c = agent->sa_timer_c;
    if (timer_d < agent->sa_t1x64)
      timer_d = agent->sa_t1x64;

    outgoing_queue_init(agent->sa_out.delayed, 0);
    outgoing_queue_init(agent->sa_out.resolving, 0);
    outgoing_queue_init(agent->sa_out.trying, agent->sa_t1x64); /* F */
    outgoing_queue_init(agent->sa_out.completed, agent->sa_t4); /* K */
    outgoing_queue_init(agent->sa_out.terminated, 0);
    /* Special queues (states) for outgoing INVITE transactions */
    outgoing_queue_init(agent->sa_out.inv_calling, agent->sa_t1x64); /* B */
    outgoing_queue_init(agent->sa_out.inv_proceeding, timer_c); /* C */
    outgoing_queue_init(agent->sa_out.inv_completed, timer_d); /* D */

    if (leg_htable_resize(agent->sa_home, agent->sa_dialogs, 0) < 0 ||
	leg_htable_resize(agent->sa_home, agent->sa_defaults, 0) < 0 ||
	outgoing_htable_resize(agent->sa_home, agent->sa_outgoing, 0) < 0 ||
	incoming_htable_resize(agent->sa_home, agent->sa_incoming, 0) < 0) {
      SU_DEBUG_0(("nta_agent_create: failure with %s\n", "hash tables"));
      goto deinit;
    }
    SU_DEBUG_9(("nta_agent_create: initialized %s\n", "hash tables"));

    if (contact_url != (url_string_t *)-1 &&
	nta_agent_add_tport(agent, contact_url, ta_tags(ta)) < 0) {
      SU_DEBUG_7(("nta_agent_create: failure with %s\n", "transport"));
      goto deinit;
    }
    SU_DEBUG_9(("nta_agent_create: initialized %s\n", "transports"));

    if (agent_tag_init(agent) < 0) {
      SU_DEBUG_3(("nta_agent_create: failure with %s\n", "random identifiers"));
      goto deinit;
    }
    SU_DEBUG_9(("nta_agent_create: initialized %s\n", "random identifiers"));

    if (agent_timer_init(agent) < 0) {
      SU_DEBUG_0(("nta_agent_create: failure with %s\n", "timer"));
      goto deinit;
    }
    SU_DEBUG_9(("nta_agent_create: initialized %s\n", "timer"));

    if (agent_launch_terminator(agent) == 0)
      SU_DEBUG_9(("nta_agent_create: initialized %s\n", "threads"));

#if HAVE_SOFIA_SRESOLV
    agent->sa_resolver = sres_resolver_create(root, NULL, ta_tags(ta));
    if (!agent->sa_resolver) {
      SU_DEBUG_0(("nta_agent_create: failure with %s\n", "resolver"));
    }
    SU_DEBUG_9(("nta_agent_create: initialized %s\n", "resolver"));
#endif

    ta_end(ta);

    return agent;

  deinit:
    nta_agent_destroy(agent);
  }

  ta_end(ta);

  return NULL;
}

/**
 * Destroy an NTA agent object.
 *
 * @param agent the NTA agent object to be destroyed.
 *
 */
void nta_agent_destroy(nta_agent_t *agent)
{
  if (agent) {
    size_t i;
    outgoing_htable_t *oht = agent->sa_outgoing;
    incoming_htable_t *iht = agent->sa_incoming;
    /* Currently, this is pretty pointless, as legs don't keep any resources */
    leg_htable_t *lht;
    nta_leg_t *leg;

    for (i = 0, lht = agent->sa_dialogs; i < lht->lht_size; i++) {
      if ((leg = lht->lht_table[i])) {
	SU_DEBUG_3(("nta_agent_destroy: destroying dialog with <"
		    URL_PRINT_FORMAT ">\n",
		    URL_PRINT_ARGS(leg->leg_remote->a_url)));
	leg_free(agent, leg);
      }
    }

    for (i = 0, lht = agent->sa_defaults; i < lht->lht_size; i++) {
      if ((leg = lht->lht_table[i])) {
	SU_DEBUG_3(("%s: destroying leg for <"
		    URL_PRINT_FORMAT ">\n",
		    __func__, URL_PRINT_ARGS(leg->leg_url)));
	leg_free(agent, leg);
      }
    }

    if (agent->sa_default_leg)
      leg_free(agent, agent->sa_default_leg);

    for (i = iht->iht_size; i-- > 0; )
      while (iht->iht_table[i]) {
	nta_incoming_t *irq = iht->iht_table[i];

	if (!irq->irq_destroyed)
	  SU_DEBUG_3(("%s: destroying %s server transaction from <"
		      URL_PRINT_FORMAT ">\n",
		      __func__, irq->irq_rq->rq_method_name,
		      URL_PRINT_ARGS(irq->irq_from->a_url)));

	incoming_free(irq);
      }

    for (i = oht->oht_size; i-- > 0;)
      while (oht->oht_table[i]) {
	nta_outgoing_t *orq = oht->oht_table[i];

	if (!orq->orq_destroyed)
	  SU_DEBUG_3(("%s: destroying %s%s client transaction to <"
		      URL_PRINT_FORMAT ">\n",
		      __func__,
		      (orq->orq_forking || orq->orq_forks) ? "forked " : "forking",
		      orq->orq_method_name,
		      URL_PRINT_ARGS(orq->orq_to->a_url)));

	orq->orq_forks = NULL, orq->orq_forking = NULL;
	outgoing_free(orq);
      }

    su_timer_destroy(agent->sa_timer), agent->sa_timer = NULL;

#   if HAVE_SOFIA_SRESOLV
    sres_resolver_destroy(agent->sa_resolver), agent->sa_resolver = NULL;
#   endif

    tport_destroy(agent->sa_tports), agent->sa_tports = NULL;

    agent_kill_terminator(agent);

    su_home_unref(agent->sa_home);
  }
}

/** Return agent context. */
nta_agent_magic_t *nta_agent_magic(nta_agent_t const *agent)
{
  return agent ? agent->sa_magic : NULL;
}

/** Return @Contact header.
 *
 * Get a @Contact header, which can be used to reach @a agent.
 *
 * @param agent NTA agent object
 *
 * User agents can insert the @Contact header in the outgoing REGISTER,
 * INVITE, and ACK requests and replies to incoming INVITE and OPTIONS
 * transactions.
 *
 * Proxies can use the @Contact header to create appropriate @RecordRoute
 * headers:
 * @code
 * r_r = sip_record_route_create(msg_home(msg),
 *	 			 sip->sip_request->rq_url,
 *				 contact->m_url);
 * @endcode
 *
 * @return A sip_contact_t object corresponding to the @a agent.
 */
sip_contact_t *nta_agent_contact(nta_agent_t const *agent)
{
  return agent ? agent->sa_contact : NULL;
}

/** Return a list of @Via headers.
 *
 * Get @Via headers for all activated transport.
 *
 * @param agent NTA agent object
 *
 * @return A list of #sip_via_t objects used by the @a agent.
 */
sip_via_t *nta_agent_via(nta_agent_t const *agent)
{
  return agent ? agent->sa_vias : NULL;
}

/** Return a list of public (UPnP, STUN) @Via headers.
 *
 * Get public @Via headers for all activated transports.
 *
 * @param agent NTA agent object
 *
 * @return A list of #sip_via_t objects used by the @a agent.
 */
sip_via_t *nta_agent_public_via(nta_agent_t const *agent)
{
  return agent ? agent->sa_public_vias : NULL;
}

/** Match a @Via header @a v with @Via headers in @a agent.
 *
 */
static
sip_via_t *agent_has_via(nta_agent_t const *agent, sip_via_t const *via)
{
  sip_via_t const *v;

  for (v = agent->sa_public_vias; v; v = v->v_next) {
    if (!su_casematch(via->v_host, v->v_host))
      continue;
    if (!su_strmatch(via->v_port, v->v_port))
      continue;
    if (!su_casematch(via->v_protocol, v->v_protocol))
      continue;
    return (sip_via_t *)v;
  }

  for (v = agent->sa_vias; v; v = v->v_next) {
    if (!su_casematch(via->v_host, v->v_host))
      continue;
    if (!su_strmatch(via->v_port, v->v_port))
      continue;
    if (!su_casematch(via->v_protocol, v->v_protocol))
      continue;
    return (sip_via_t *)v;
  }

  return NULL;
}

/** Return @UserAgent header.
 *
 * Get @UserAgent information with NTA version.
 *
 * @param agent NTA agent object (may be NULL)
 *
 * @return A string containing the @a agent version.
 */
char const *nta_agent_version(nta_agent_t const *agent)
{
  return "nta" "/" VERSION;
}

/** Initialize default tag */
static int agent_tag_init(nta_agent_t *self)
{
  sip_contact_t *m = self->sa_contact;
  uint32_t hash = su_random();

  if (m) {
    if (m->m_url->url_user)
      hash = 914715421U * hash + msg_hash_string(m->m_url->url_user);
    if (m->m_url->url_host)
      hash = 914715421U * hash + msg_hash_string(m->m_url->url_host);
    if (m->m_url->url_port)
      hash = 914715421U * hash + msg_hash_string(m->m_url->url_port);
    if (m->m_url->url_params)
      hash = 914715421U * hash + msg_hash_string(m->m_url->url_params);
  }

  if (hash == 0)
    hash = 914715421U;

  self->sa_branch = NTA_BRANCH_PRIME * (uint64_t)su_nanotime(NULL);
  self->sa_branch *= hash;

  self->sa_tags = NTA_TAG_PRIME * self->sa_branch;

  return 0;
}

/** Initialize agent timer. */
static
int agent_timer_init(nta_agent_t *agent)
{
  agent->sa_timer = su_timer_create(su_root_task(agent->sa_root),
				    NTA_SIP_T1 / 8);
#if 0
  return su_timer_set(agent->sa_timer,
		      agent_timer,
		      agent);
#endif
  return -(agent->sa_timer == NULL);
}

/**
 * Agent timer routine.
 */
static
void agent_timer(su_root_magic_t *rm, su_timer_t *timer, nta_agent_t *agent)
{
  su_time_t stamp = su_now();
  uint32_t now = su_time_ms(stamp), next, latest;

  now += now == 0;

  agent->sa_next = 0;

  agent->sa_now = stamp;
  agent->sa_millisec = now;
  agent->sa_in_timer = 1;

  _nta_outgoing_timer(agent);
  _nta_incoming_timer(agent);

  /* agent->sa_now is used only if sa_millisec != 0 */
  agent->sa_millisec = 0;
  agent->sa_in_timer = 0;

  /* Calculate next timeout */
  next = latest = now + NTA_TIME_MAX + 1;

#define NEXT_TIMEOUT(next, p, f, now) \
  (void)(p && (int32_t)(p->f - (next)) < 0 && \
	 ((next) = ((int32_t)(p->f - (now)) > 0 ? p->f : (now))))

  NEXT_TIMEOUT(next, agent->sa_out.re_list, orq_retry, now);
  NEXT_TIMEOUT(next, agent->sa_out.inv_completed->q_head, orq_timeout, now);
  NEXT_TIMEOUT(next, agent->sa_out.completed->q_head, orq_timeout, now);
  NEXT_TIMEOUT(next, agent->sa_out.inv_calling->q_head, orq_timeout, now);
  if (agent->sa_out.inv_proceeding->q_timeout)
    NEXT_TIMEOUT(next, agent->sa_out.inv_proceeding->q_head, orq_timeout, now);
  NEXT_TIMEOUT(next, agent->sa_out.trying->q_head, orq_timeout, now);

  NEXT_TIMEOUT(next, agent->sa_in.preliminary->q_head, irq_timeout, now);
  NEXT_TIMEOUT(next, agent->sa_in.inv_completed->q_head, irq_timeout, now);
  NEXT_TIMEOUT(next, agent->sa_in.inv_confirmed->q_head, irq_timeout, now);
  NEXT_TIMEOUT(next, agent->sa_in.completed->q_head, irq_timeout, now);
  NEXT_TIMEOUT(next, agent->sa_in.re_list, irq_retry, now);

  if (agent->sa_next)
    NEXT_TIMEOUT(next, agent, sa_next, now);

#undef NEXT_TIMEOUT

  if (next == latest) {
    /* Do not set timer? */
    SU_DEBUG_9(("nta: timer not set\n" VA_NONE));
    assert(!agent->sa_out.completed->q_head);
    assert(!agent->sa_out.trying->q_head);
    assert(!agent->sa_out.inv_calling->q_head);
    assert(!agent->sa_out.re_list);
    assert(!agent->sa_in.inv_confirmed->q_head);
    assert(!agent->sa_in.preliminary->q_head);
    assert(!agent->sa_in.completed->q_head);
    assert(!agent->sa_in.inv_completed->q_head);
    assert(!agent->sa_in.re_list);
    return;
  }

  if (next == now) if (++next == 0) ++next;

  SU_DEBUG_9(("nta: timer %s to %ld ms\n", "set next", (long)(next - now)));

  agent->sa_next = next;

  su_timer_set_at(timer, agent_timer, agent, su_time_add(stamp, next - now));
}

/** Add uin32_t milliseconds to the time. */
static su_time_t add_milliseconds(su_time_t t0, uint32_t ms)
{
  unsigned long sec = ms / 1000, usec = (ms % 1000) * 1000;

  t0.tv_usec += usec;
  t0.tv_sec += sec;

  if (t0.tv_usec >= 1000000) {
    t0.tv_sec += 1;
    t0.tv_usec -= 1000000;
  }

  return t0;
}

/** Calculate nonzero value for timeout.
 *
 * Sets or adjusts agent timer when needed.
 *
 * @retval 0 if offset is 0
 * @retval timeout (millisecond counter) otherwise
 */
static
uint32_t set_timeout(nta_agent_t *agent, uint32_t offset)
{
  su_time_t now;
  uint32_t next, ms;

  if (offset == 0)
    return 0;

  if (agent->sa_millisec) /* Avoid expensive call to su_now() */
    now = agent->sa_now, ms = agent->sa_millisec;
  else
    now = su_now(), ms = su_time_ms(now);

  next = ms + offset; if (next == 0) next = 1;

  if (agent->sa_in_timer)	/* Currently executing timer */
    return next;

  if (agent->sa_next == 0 || (int32_t)(agent->sa_next - next - 5L) > 0) {
    /* Set timer */
    if (agent->sa_next)
      SU_DEBUG_9(("nta: timer %s to %ld ms\n", "shortened", (long)offset));
    else
      SU_DEBUG_9(("nta: timer %s to %ld ms\n", "set", (long)offset));

    su_timer_set_at(agent->sa_timer, agent_timer, agent,
		    add_milliseconds(now, offset));
    agent->sa_next = next;
  }

  return next;
}


/** Return current timeval. */
static
su_time_t agent_now(nta_agent_t const *agent)
{
  if (agent && agent->sa_millisec != 0)
    return agent->sa_now;
  else
    return su_now();
}


/** Launch transaction terminator task */
static
int agent_launch_terminator(nta_agent_t *agent)
{
#ifdef TPTAG_THRPSIZE
  if (agent->sa_tport_threadpool) {
    su_home_threadsafe(agent->sa_home);
    return su_clone_start(agent->sa_root,
			  agent->sa_terminator,
			  NULL,
			  NULL,
			  NULL);
  }
#endif
  return -1;
}

/** Kill transaction terminator task */
static
void agent_kill_terminator(nta_agent_t *agent)
{
  su_clone_wait(agent->sa_root, agent->sa_terminator);
}


/**Set NTA Parameters.
 *
 * The nta_agent_set_params() function sets the stack parameters. The
 * parameters determine the way NTA handles the retransmissions, how long
 * NTA keeps transactions alive, does NTA apply proxy or user-agent logic to
 * INVITE transactions, or how the @Via headers are generated.
 *
 * @note
 * Setting the parameters NTATAG_MAXSIZE(), NTATAG_UDP_MTU(), NTATAG_MAX_PROCEEDING(),
 * NTATAG_SIP_T1X64(), NTATAG_SIP_T1(), NTATAG_SIP_T2(), NTATAG_SIP_T4() to
 * 0 selects the default value.
 *
 * @TAGS
 * NTATAG_ALIASES(),
 * NTATAG_BAD_REQ_MASK(), NTATAG_BAD_RESP_MASK(), NTATAG_BLACKLIST(),
 * NTATAG_CANCEL_2543(), NTATAG_CANCEL_487(), NTATAG_CLIENT_RPORT(),
 * NTATAG_DEBUG_DROP_PROB(), NTATAG_DEFAULT_PROXY(),
 * NTATAG_EXTRA_100(), NTATAG_GRAYLIST(),
 * NTATAG_MAXSIZE(), NTATAG_MAX_FORWARDS(), NTATAG_MERGE_482(), NTATAG_MCLASS()
 * NTATAG_PASS_100(), NTATAG_PASS_408(), NTATAG_PRELOAD(), NTATAG_PROGRESS(),
 * NTATAG_REL100(),
 * NTATAG_SERVER_RPORT(),
 * NTATAG_SIPFLAGS(),
 * NTATAG_SIP_T1X64(), NTATAG_SIP_T1(), NTATAG_SIP_T2(), NTATAG_SIP_T4(),
 * NTATAG_STATELESS(),
 * NTATAG_TAG_3261(), NTATAG_TCP_RPORT(), NTATAG_TIMEOUT_408(),
 * NTATAG_TLS_RPORT(),
 * NTATAG_TIMER_C(), NTATAG_MAX_PROCEEDING(),
 * NTATAG_UA(), NTATAG_UDP_MTU(), NTATAG_USER_VIA(),
 * NTATAG_USE_NAPTR(), NTATAG_USE_SRV() and NTATAG_USE_TIMESTAMP().
 *
 * @note The value from following tags are stored, but they currently do nothing:
 * NTATAG_SIGCOMP_ALGORITHM(), NTATAG_SIGCOMP_OPTIONS(), NTATAG_SMIME()
 */
int nta_agent_set_params(nta_agent_t *agent,
			 tag_type_t tag, tag_value_t value, ...)
{
  int retval;

  if (agent) {
    ta_list ta;
    ta_start(ta, tag, value);
    retval = agent_set_params(agent, ta_args(ta));
    ta_end(ta);
  } else {
    su_seterrno(EINVAL);
    retval = -1;
  }

  return retval;
}

/** Internal function for setting tags */
static
int agent_set_params(nta_agent_t *agent, tagi_t *tags)
{
  int n, nC, m;
  unsigned bad_req_mask = agent->sa_bad_req_mask;
  unsigned bad_resp_mask = agent->sa_bad_resp_mask;
  usize_t  maxsize    = agent->sa_maxsize;
  usize_t  max_proceeding = agent->sa_max_proceeding;
  unsigned max_forwards = agent->sa_max_forwards->mf_count;
  unsigned udp_mtu    = agent->sa_udp_mtu;
  unsigned sip_t1     = agent->sa_t1;
  unsigned sip_t2     = agent->sa_t2;
  unsigned sip_t4     = agent->sa_t4;
  unsigned sip_t1x64  = agent->sa_t1x64;
  unsigned timer_c    = agent->sa_timer_c;
  unsigned timer_d    = 32000;
  unsigned graylist   = agent->sa_graylist;
  unsigned blacklist  = agent->sa_blacklist;
  int ua              = agent->sa_is_a_uas;
  unsigned progress   = agent->sa_progress;
  int stateless       = agent->sa_is_stateless;
  unsigned drop_prob  = agent->sa_drop_prob;
  int user_via        = agent->sa_user_via;
  int extra_100       = agent->sa_extra_100;
  int pass_100        = agent->sa_pass_100;
  int timeout_408     = agent->sa_timeout_408;
  int pass_408        = agent->sa_pass_408;
  int merge_482       = agent->sa_merge_482;
  int cancel_2543     = agent->sa_cancel_2543;
  int cancel_487      = agent->sa_cancel_487;
  int invite_100rel   = agent->sa_invite_100rel;
  int use_timestamp   = agent->sa_timestamp;
  int use_naptr       = agent->sa_use_naptr;
  int use_srv         = agent->sa_use_srv;
  int srv_503         = agent->sa_srv_503;
  void *smime         = agent->sa_smime;
  uint32_t flags      = agent->sa_flags;
  int rport           = agent->sa_rport;
  int server_rport    = agent->sa_server_rport;
  int tcp_rport       = agent->sa_tcp_rport;
  int tls_rport       = agent->sa_tls_rport;
  unsigned preload         = agent->sa_preload;
  unsigned threadpool      = agent->sa_tport_threadpool;
  char const *sigcomp = agent->sa_sigcomp_options;
  char const *algorithm = NONE;
  msg_mclass_t const *mclass = NONE;
  sip_contact_t const *aliases = NONE;
  url_string_t const *proxy = NONE;
  tport_t *tport;

  su_home_t *home = agent->sa_home;

  n = tl_gets(tags,
	      NTATAG_ALIASES_REF(aliases),
	      NTATAG_BAD_REQ_MASK_REF(bad_req_mask),
	      NTATAG_BAD_RESP_MASK_REF(bad_resp_mask),
	      NTATAG_BLACKLIST_REF(blacklist),
	      NTATAG_CANCEL_2543_REF(cancel_2543),
	      NTATAG_CANCEL_487_REF(cancel_487),
	      NTATAG_DEBUG_DROP_PROB_REF(drop_prob),
	      NTATAG_DEFAULT_PROXY_REF(proxy),
	      NTATAG_EXTRA_100_REF(extra_100),
	      NTATAG_GRAYLIST_REF(graylist),
	      NTATAG_MAXSIZE_REF(maxsize),
	      NTATAG_MAX_PROCEEDING_REF(max_proceeding),
	      NTATAG_MAX_FORWARDS_REF(max_forwards),
	      NTATAG_MCLASS_REF(mclass),
	      NTATAG_MERGE_482_REF(merge_482),
	      NTATAG_PASS_100_REF(pass_100),
	      NTATAG_PASS_408_REF(pass_408),
	      NTATAG_PRELOAD_REF(preload),
	      NTATAG_PROGRESS_REF(progress),
	      NTATAG_REL100_REF(invite_100rel),
	      NTATAG_RPORT_REF(rport),
	      NTATAG_SERVER_RPORT_REF(server_rport),
	      NTATAG_SIGCOMP_ALGORITHM_REF(algorithm),
	      NTATAG_SIGCOMP_OPTIONS_REF(sigcomp),
	      NTATAG_SIPFLAGS_REF(flags),
	      NTATAG_SIP_T1X64_REF(sip_t1x64),
	      NTATAG_SIP_T1_REF(sip_t1),
	      NTATAG_SIP_T2_REF(sip_t2),
	      NTATAG_SIP_T4_REF(sip_t4),
#if HAVE_SOFIA_SMIME
	      NTATAG_SMIME_REF(smime),
#endif
	      NTATAG_STATELESS_REF(stateless),
	      NTATAG_TCP_RPORT_REF(tcp_rport),
	      NTATAG_TLS_RPORT_REF(tls_rport),
	      NTATAG_TIMEOUT_408_REF(timeout_408),
	      NTATAG_UA_REF(ua),
	      NTATAG_UDP_MTU_REF(udp_mtu),
	      NTATAG_USER_VIA_REF(user_via),
	      NTATAG_USE_NAPTR_REF(use_naptr),
	      NTATAG_USE_SRV_REF(use_srv),
	      NTATAG_USE_TIMESTAMP_REF(use_timestamp),
#ifdef TPTAG_THRPSIZE
	      /* If threadpool is enabled, start a separate "reaper thread" */
	      TPTAG_THRPSIZE_REF(threadpool),
#endif
              NTATAG_SRV_503_REF(srv_503),
	      TAG_END());
  nC = tl_gets(tags,
	       NTATAG_TIMER_C_REF(timer_c),
	       TAG_END());
  n += nC;

  if (mclass != NONE)
    agent->sa_mclass = mclass ? mclass : sip_default_mclass();

  m = 0;
  for (tport = agent->sa_tports; tport; tport = tport_next(tport)) {
    int m0 = tport_set_params(tport, TAG_NEXT(tags));
    if (m0 < 0)
      return m0;
    if (m0 > m)
      m = m0;
  }

  n += m;

  if (aliases != NONE) {
    sip_contact_t const *m, *m_next;

    m = agent->sa_aliases;
    agent->sa_aliases = sip_contact_dup(home, aliases);

    for (; m; m = m_next) {	/* Free old aliases */
      m_next = m->m_next;
      su_free(home, (void *)m);
    }
  }

  if (proxy != NONE) {
    url_t *dp = url_hdup(home, proxy->us_url);

    url_sanitize(dp);

    if (dp == NULL || dp->url_type == url_sip || dp->url_type == url_sips) {
      if (agent->sa_default_proxy)
	su_free(home, agent->sa_default_proxy);
      agent->sa_default_proxy = dp;
    }
    else
      n = -1;
  }

  if (algorithm != NONE)
    agent->sa_algorithm = su_strdup(home, algorithm);

  if (!su_strmatch(sigcomp, agent->sa_sigcomp_options)) {
    msg_param_t const *l = NULL;
    char *s = su_strdup(home, sigcomp);
    char *s1 = su_strdup(home, s), *s2 = s1;

    if (s && s2 && msg_avlist_d(home, &s2, &l) == 0 && *s2 == '\0') {
      su_free(home, (void *)agent->sa_sigcomp_options);
      su_free(home, (void *)agent->sa_sigcomp_option_list);
      agent->sa_sigcomp_options = s;
      agent->sa_sigcomp_option_free = s1;
      agent->sa_sigcomp_option_list = l;
    } else {
      su_free(home, s);
      su_free(home, s1);
      su_free(home, (void *)l);
      n = -1;
    }
  }

  if (maxsize == 0) maxsize = 2 * 1024 * 1024;
  if (maxsize > UINT32_MAX) maxsize = UINT32_MAX;
  agent->sa_maxsize = maxsize;

  if (max_proceeding == 0) max_proceeding = USIZE_MAX;
  agent->sa_max_proceeding = max_proceeding;

  if (max_forwards == 0) max_forwards = 70; /* Default value */
  agent->sa_max_forwards->mf_count = max_forwards;

  if (udp_mtu == 0) udp_mtu = 1300;
  if (udp_mtu > 65535) udp_mtu = 65535;
  if (agent->sa_udp_mtu != udp_mtu) {
    agent->sa_udp_mtu = udp_mtu;
    agent_set_udp_params(agent, udp_mtu);
  }

  if (sip_t1 == 0) sip_t1 = NTA_SIP_T1;
  if (sip_t1 > NTA_TIME_MAX) sip_t1 = NTA_TIME_MAX;
  agent->sa_t1 = sip_t1;

  if (sip_t2 == 0) sip_t2 = NTA_SIP_T2;
  if (sip_t2 > NTA_TIME_MAX) sip_t2 = NTA_TIME_MAX;
  agent->sa_t2 = sip_t2;

  if (sip_t4 == 0) sip_t4 = NTA_SIP_T4;
  if (sip_t4 > NTA_TIME_MAX) sip_t4 = NTA_TIME_MAX;
  if (agent->sa_t4 != sip_t4) {
    incoming_queue_adjust(agent, agent->sa_in.inv_confirmed, sip_t4);
    outgoing_queue_adjust(agent, agent->sa_out.completed, sip_t4);
  }
  agent->sa_t4 = sip_t4;

  if (sip_t1x64 == 0) sip_t1x64 = NTA_SIP_T1 * 64;
  if (sip_t1x64 > NTA_TIME_MAX) sip_t1x64 = NTA_TIME_MAX;
  if (agent->sa_t1x64 != sip_t1x64) {
    incoming_queue_adjust(agent, agent->sa_in.preliminary, sip_t1x64);
    incoming_queue_adjust(agent, agent->sa_in.completed, sip_t1x64);
    incoming_queue_adjust(agent, agent->sa_in.inv_completed, sip_t1x64);
    outgoing_queue_adjust(agent, agent->sa_out.trying, sip_t1x64);
    outgoing_queue_adjust(agent, agent->sa_out.inv_calling, sip_t1x64);
  }
  agent->sa_t1x64 = sip_t1x64;
  if (nC == 1) {
    agent->sa_use_timer_c = 1;
    if (timer_c == 0)
      timer_c = 185 * 1000;
    agent->sa_timer_c = timer_c;
    outgoing_queue_adjust(agent, agent->sa_out.inv_proceeding, timer_c);
  }
  if (timer_d < sip_t1x64)
    timer_d = sip_t1x64;
  outgoing_queue_adjust(agent, agent->sa_out.inv_completed, timer_d);

  if (graylist > 24 * 60 * 60)
    graylist = 24 * 60 * 60;
  agent->sa_graylist = graylist;

  if (blacklist > 24 * 60 * 60)
    blacklist = 24 * 60 * 60;
  agent->sa_blacklist = blacklist;

  if (progress == 0)
    progress = 60 * 1000;
  agent->sa_progress = progress;

  if (server_rport > 3)
    server_rport = 1;
  else if (server_rport < 0)
    server_rport = 1;
  agent->sa_server_rport = server_rport;

  agent->sa_bad_req_mask = bad_req_mask;
  agent->sa_bad_resp_mask = bad_resp_mask;

  agent->sa_is_a_uas = ua != 0;
  agent->sa_is_stateless = stateless != 0;
  agent->sa_drop_prob = drop_prob < 1000 ? drop_prob : 1000;
  agent->sa_user_via = user_via != 0;
  agent->sa_extra_100 = extra_100 != 0;
  agent->sa_pass_100 = pass_100 != 0;
  agent->sa_timeout_408 = timeout_408 != 0;
  agent->sa_pass_408 = pass_408 != 0;
  agent->sa_merge_482 = merge_482 != 0;
  agent->sa_cancel_2543 = cancel_2543 != 0;
  agent->sa_cancel_487 = cancel_487 != 0;
  agent->sa_invite_100rel = invite_100rel != 0;
  agent->sa_timestamp = use_timestamp != 0;
  agent->sa_use_naptr = use_naptr != 0;
  agent->sa_use_srv = use_srv != 0;
  agent->sa_srv_503 = srv_503 != 0;
  agent->sa_smime = smime;
  agent->sa_flags = flags & MSG_FLG_USERMASK;
  agent->sa_rport = rport != 0;
  agent->sa_tcp_rport = tcp_rport != 0;
  agent->sa_tls_rport = tls_rport != 0;
  agent->sa_preload = preload;
  agent->sa_tport_threadpool = threadpool;

  return n;
}

static
void agent_set_udp_params(nta_agent_t *self, usize_t udp_mtu)
{
  tport_t *tp;

  /* Set via fields for the tports */
  for (tp = tport_primaries(self->sa_tports); tp; tp = tport_next(tp)) {
    if (tport_is_udp(tp))
      tport_set_params(tp,
		       TPTAG_TIMEOUT(2 * self->sa_t1x64),
		       TPTAG_MTU(udp_mtu),
		       TAG_END());
  }
}

/**Get NTA Parameters.
 *
 * The nta_agent_get_params() function retrieves the stack parameters. The
 * parameters determine the way NTA handles the retransmissions, how long
 * NTA keeps transactions alive, does NTA apply proxy or user-agent logic to
 * INVITE transactions, or how the @Via headers are generated.
 *
 * @TAGS
 * NTATAG_ALIASES_REF(), NTATAG_BLACKLIST_REF(),
 * NTATAG_CANCEL_2543_REF(), NTATAG_CANCEL_487_REF(),
 * NTATAG_CLIENT_RPORT_REF(), NTATAG_CONTACT_REF(),
 * NTATAG_DEBUG_DROP_PROB_REF(), NTATAG_DEFAULT_PROXY_REF(),
 * NTATAG_EXTRA_100_REF(), NTATAG_GRAYLIST_REF(),
 * NTATAG_MAXSIZE_REF(), NTATAG_MAX_FORWARDS_REF(), NTATAG_MCLASS_REF(),
 * NTATAG_MERGE_482_REF(), NTATAG_MAX_PROCEEDING_REF(),
 * NTATAG_PASS_100_REF(), NTATAG_PASS_408_REF(), NTATAG_PRELOAD_REF(),
 * NTATAG_PROGRESS_REF(),
 * NTATAG_REL100_REF(),
 * NTATAG_SERVER_RPORT_REF(),
 * NTATAG_SIGCOMP_ALGORITHM_REF(), NTATAG_SIGCOMP_OPTIONS_REF(),
 * NTATAG_SIPFLAGS_REF(),
 * NTATAG_SIP_T1_REF(), NTATAG_SIP_T1X64_REF(), NTATAG_SIP_T2_REF(),
 * NTATAG_SIP_T4_REF(), NTATAG_SMIME_REF(), NTATAG_STATELESS_REF(),
 * NTATAG_TAG_3261_REF(), NTATAG_TIMEOUT_408_REF(), NTATAG_TIMER_C_REF(),
 * NTATAG_UA_REF(), NTATAG_UDP_MTU_REF(), NTATAG_USER_VIA_REF(),
 * NTATAG_USE_NAPTR_REF(), NTATAG_USE_SRV_REF(),
 * and NTATAG_USE_TIMESTAMP_REF().
 *
 */
int nta_agent_get_params(nta_agent_t *agent,
			 tag_type_t tag, tag_value_t value, ...)
{
  int n;
  ta_list ta;

  if (agent) {
    ta_start(ta, tag, value);
    n = agent_get_params(agent, ta_args(ta));
    ta_end(ta);
    return n;
  }

  su_seterrno(EINVAL);
  return -1;
}

/** Get NTA parameters */
static
int agent_get_params(nta_agent_t *agent, tagi_t *tags)
{
  return
    tl_tgets(tags,
	     NTATAG_ALIASES(agent->sa_aliases),
	     NTATAG_BLACKLIST(agent->sa_blacklist),
	     NTATAG_CANCEL_2543(agent->sa_cancel_2543),
	     NTATAG_CANCEL_487(agent->sa_cancel_487),
	     NTATAG_CLIENT_RPORT(agent->sa_rport),
	     NTATAG_CONTACT(agent->sa_contact),
	     NTATAG_DEBUG_DROP_PROB(agent->sa_drop_prob),
	     NTATAG_DEFAULT_PROXY(agent->sa_default_proxy),
	     NTATAG_EXTRA_100(agent->sa_extra_100),
	     NTATAG_GRAYLIST(agent->sa_graylist),
	     NTATAG_MAXSIZE(agent->sa_maxsize),
	     NTATAG_MAX_PROCEEDING(agent->sa_max_proceeding),
	     NTATAG_MAX_FORWARDS(agent->sa_max_forwards->mf_count),
	     NTATAG_MCLASS(agent->sa_mclass),
	     NTATAG_MERGE_482(agent->sa_merge_482),
	     NTATAG_PASS_100(agent->sa_pass_100),
	     NTATAG_PASS_408(agent->sa_pass_408),
	     NTATAG_PRELOAD(agent->sa_preload),
	     NTATAG_PROGRESS(agent->sa_progress),
	     NTATAG_REL100(agent->sa_invite_100rel),
	     NTATAG_SERVER_RPORT((int)(agent->sa_server_rport)),
	     NTATAG_SIGCOMP_ALGORITHM(agent->sa_algorithm),
	     NTATAG_SIGCOMP_OPTIONS(agent->sa_sigcomp_options ?
				    agent->sa_sigcomp_options :
				    "sip"),
	     NTATAG_SIPFLAGS(agent->sa_flags),
	     NTATAG_SIP_T1(agent->sa_t1),
	     NTATAG_SIP_T1X64(agent->sa_t1x64),
	     NTATAG_SIP_T2(agent->sa_t2),
	     NTATAG_SIP_T4(agent->sa_t4),
#if HAVE_SOFIA_SMIME
	     NTATAG_SMIME(agent->sa_smime),
#else
	     NTATAG_SMIME(NULL),
#endif
	     NTATAG_STATELESS(agent->sa_is_stateless),
	     NTATAG_TAG_3261(1),
	     NTATAG_TIMEOUT_408(agent->sa_timeout_408),
	     NTATAG_TIMER_C(agent->sa_timer_c),
	     NTATAG_UA(agent->sa_is_a_uas),
	     NTATAG_UDP_MTU(agent->sa_udp_mtu),
	     NTATAG_USER_VIA(agent->sa_user_via),
	     NTATAG_USE_NAPTR(agent->sa_use_naptr),
	     NTATAG_USE_SRV(agent->sa_use_srv),
	     NTATAG_USE_TIMESTAMP(agent->sa_timestamp),
	     NTATAG_SRV_503(agent->sa_srv_503),
	     TAG_END());
}

/**Get NTA statistics.
 *
 * The nta_agent_get_stats() function retrieves the stack statistics.
 *
 * @TAGS
 * NTATAG_S_ACKED_TR_REF(),
 * NTATAG_S_BAD_MESSAGE_REF(),
 * NTATAG_S_BAD_REQUEST_REF(),
 * NTATAG_S_BAD_RESPONSE_REF(),
 * NTATAG_S_CANCELED_TR_REF(),
 * NTATAG_S_CLIENT_TR_REF(),
 * NTATAG_S_DIALOG_TR_REF(),
 * NTATAG_S_DROP_REQUEST_REF(),
 * NTATAG_S_DROP_RESPONSE_REF(),
 * NTATAG_S_IRQ_HASH_REF(),
 * NTATAG_S_IRQ_HASH_USED_REF(),
 * NTATAG_S_LEG_HASH_REF(),
 * NTATAG_S_LEG_HASH_USED_REF(),
 * NTATAG_S_MERGED_REQUEST_REF(),
 * NTATAG_S_ORQ_HASH_REF(),
 * NTATAG_S_ORQ_HASH_USED_REF(),
 * NTATAG_S_RECV_MSG_REF(),
 * NTATAG_S_RECV_REQUEST_REF(),
 * NTATAG_S_RECV_RESPONSE_REF(),
 * NTATAG_S_RECV_RETRY_REF(),
 * NTATAG_S_RETRY_REQUEST_REF(),
 * NTATAG_S_RETRY_RESPONSE_REF(),
 * NTATAG_S_SENT_MSG_REF(),
 * NTATAG_S_SENT_REQUEST_REF(),
 * NTATAG_S_SENT_RESPONSE_REF(),
 * NTATAG_S_SERVER_TR_REF(),
 * NTATAG_S_TOUT_REQUEST_REF(),
 * NTATAG_S_TOUT_RESPONSE_REF(),
 * NTATAG_S_TRLESS_200_REF(),
 * NTATAG_S_TRLESS_REQUEST_REF(),
 * NTATAG_S_TRLESS_RESPONSE_REF(), and
 * NTATAG_S_TRLESS_TO_TR_REF(),
 */
int nta_agent_get_stats(nta_agent_t *agent,
			tag_type_t tag, tag_value_t value, ...)
{
  int n;
  ta_list ta;

  if (!agent)
    return su_seterrno(EINVAL), -1;

  ta_start(ta, tag, value);

  n = tl_tgets(ta_args(ta),
	       NTATAG_S_IRQ_HASH(agent->sa_incoming->iht_size),
	       NTATAG_S_ORQ_HASH(agent->sa_outgoing->oht_size),
	       NTATAG_S_LEG_HASH(agent->sa_dialogs->lht_size),
	       NTATAG_S_IRQ_HASH_USED(agent->sa_incoming->iht_used),
	       NTATAG_S_ORQ_HASH_USED(agent->sa_outgoing->oht_used),
	       NTATAG_S_LEG_HASH_USED(agent->sa_dialogs->lht_used),
	       NTATAG_S_RECV_MSG(agent->sa_stats->as_recv_msg),
	       NTATAG_S_RECV_REQUEST(agent->sa_stats->as_recv_request),
	       NTATAG_S_RECV_RESPONSE(agent->sa_stats->as_recv_response),
	       NTATAG_S_BAD_MESSAGE(agent->sa_stats->as_bad_message),
	       NTATAG_S_BAD_REQUEST(agent->sa_stats->as_bad_request),
	       NTATAG_S_BAD_RESPONSE(agent->sa_stats->as_bad_response),
	       NTATAG_S_DROP_REQUEST(agent->sa_stats->as_drop_request),
	       NTATAG_S_DROP_RESPONSE(agent->sa_stats->as_drop_response),
	       NTATAG_S_CLIENT_TR(agent->sa_stats->as_client_tr),
	       NTATAG_S_SERVER_TR(agent->sa_stats->as_server_tr),
	       NTATAG_S_DIALOG_TR(agent->sa_stats->as_dialog_tr),
	       NTATAG_S_ACKED_TR(agent->sa_stats->as_acked_tr),
	       NTATAG_S_CANCELED_TR(agent->sa_stats->as_canceled_tr),
	       NTATAG_S_TRLESS_REQUEST(agent->sa_stats->as_trless_request),
	       NTATAG_S_TRLESS_TO_TR(agent->sa_stats->as_trless_to_tr),
	       NTATAG_S_TRLESS_RESPONSE(agent->sa_stats->as_trless_response),
	       NTATAG_S_TRLESS_200(agent->sa_stats->as_trless_200),
	       NTATAG_S_MERGED_REQUEST(agent->sa_stats->as_merged_request),
	       NTATAG_S_SENT_MSG(agent->sa_stats->as_sent_msg),
	       NTATAG_S_SENT_REQUEST(agent->sa_stats->as_sent_request),
	       NTATAG_S_SENT_RESPONSE(agent->sa_stats->as_sent_response),
	       NTATAG_S_RETRY_REQUEST(agent->sa_stats->as_retry_request),
	       NTATAG_S_RETRY_RESPONSE(agent->sa_stats->as_retry_response),
	       NTATAG_S_RECV_RETRY(agent->sa_stats->as_recv_retry),
	       NTATAG_S_TOUT_REQUEST(agent->sa_stats->as_tout_request),
	       NTATAG_S_TOUT_RESPONSE(agent->sa_stats->as_tout_response),
	       TAG_END());

  ta_end(ta);

  return n;
}

/**Calculate a new unique tag.
 *
 * This function generates a series of 2**64 unique tags for @From or @To
 * headers. The start of the tag series is derived from the NTP time the NTA
 * agent was initialized.
 *
 */
char const *nta_agent_newtag(su_home_t *home, char const *fmt, nta_agent_t *sa)
{
  char tag[(8 * 8 + 4)/ 5 + 1];

  if (sa == NULL)
    return su_seterrno(EINVAL), NULL;

  /* XXX - use a cryptographically safe func here? */
  sa->sa_tags += NTA_TAG_PRIME;

  msg_random_token(tag, sizeof(tag) - 1, &sa->sa_tags, sizeof(sa->sa_tags));

  if (fmt && fmt[0])
    return su_sprintf(home, fmt, tag);
  else
    return su_strdup(home, tag);
}

/**
 * Calculate branch value.
 */
static char const *stateful_branch(su_home_t *home, nta_agent_t *sa)
{
  char branch[(8 * 8 + 4)/ 5 + 1];

  /* XXX - use a cryptographically safe func here? */
  sa->sa_branch += NTA_BRANCH_PRIME;

  msg_random_token(branch, sizeof(branch) - 1,
		   &sa->sa_branch, sizeof(sa->sa_branch));

  return su_sprintf(home, "branch=z9hG4bK%s", branch);
}

#include <sofia-sip/su_md5.h>

/**
 * Calculate branch value for stateless operation.
 *
 * XXX - should include HMAC of previous @Via line.
 */
static
char const *stateless_branch(nta_agent_t *sa,
			     msg_t *msg,
			     sip_t const *sip,
			     tp_name_t const *tpn)
{
  su_md5_t md5[1];
  uint8_t digest[SU_MD5_DIGEST_SIZE];
  char branch[(SU_MD5_DIGEST_SIZE * 8 + 4)/ 5 + 1];
  sip_route_t const *r;

  assert(sip->sip_request);

  if (!sip->sip_via)
    return stateful_branch(msg_home(msg), sa);

  su_md5_init(md5);

  su_md5_str0update(md5, tpn->tpn_host);
  su_md5_str0update(md5, tpn->tpn_port);

  url_update(md5, sip->sip_request->rq_url);
  if (sip->sip_call_id) {
    su_md5_str0update(md5, sip->sip_call_id->i_id);
  }
  if (sip->sip_from) {
    url_update(md5, sip->sip_from->a_url);
    su_md5_stri0update(md5, sip->sip_from->a_tag);
  }
  if (sip->sip_to) {
    url_update(md5, sip->sip_to->a_url);
    /* XXX - some broken implementations include To tag in CANCEL */
    /* su_md5_str0update(md5, sip->sip_to->a_tag); */
  }
  if (sip->sip_cseq) {
    uint32_t cseq = htonl(sip->sip_cseq->cs_seq);
    su_md5_update(md5, &cseq, sizeof(cseq));
  }

  for (r = sip->sip_route; r; r = r->r_next)
    url_update(md5, r->r_url);

  su_md5_digest(md5, digest);

  msg_random_token(branch, sizeof(branch) - 1, digest, sizeof(digest));

  return su_sprintf(msg_home(msg), "branch=z9hG4bK.%s", branch);
}

/* ====================================================================== */
/* 2) Transport interface */

/* Local prototypes */
static int agent_create_master_transport(nta_agent_t *self, tagi_t *tags);
static int agent_init_via(nta_agent_t *self, tport_t *primaries, int use_maddr);
static int agent_init_contact(nta_agent_t *self);
static void agent_recv_message(nta_agent_t *agent,
			       tport_t *tport,
			       msg_t *msg,
			       sip_via_t *tport_via,
			       su_time_t now);
static void agent_tp_error(nta_agent_t *agent,
			   tport_t *tport,
			   int errcode,
			   char const *remote);
static void agent_update_tport(nta_agent_t *agent, tport_t *);

/**For each transport, we have name used by tport module, SRV prefixes used
 * for resolving, and NAPTR service/conversion.
 */
static
struct sipdns_tport {
  char name[6];			/**< Named used by tport module */
  char port[6];			/**< Default port number */
  char prefix[14];		/**< Prefix for SRV domains */
  char service[10];		/**< NAPTR service */
}
#define SIPDNS_TRANSPORTS (6)
const sipdns_tports[SIPDNS_TRANSPORTS] = {
  { "udp",  "5060", "_sip._udp.",  "SIP+D2U"  },
  { "tcp",  "5060", "_sip._tcp.",  "SIP+D2T"  },
  { "sctp", "5060", "_sip._sctp.", "SIP+D2S"  },
  { "tls",  "5061", "_sips._tcp.", "SIPS+D2T" },
  { "ws",   "5080",   "_sips._ws.",  "SIP+D2W"  },
  { "wss",  "5081",  "_sips._wss.", "SIPS+D2W" },
};

static char const * const tports_sip[] =
  {
	"udp", "tcp", "sctp", "ws", NULL
  };

static char const * const tports_sips[] =
  {
	  "tls", "wss", "ws", NULL
  };

static tport_stack_class_t nta_agent_class[1] =
  {{
    sizeof(nta_agent_class),
    agent_recv_message,
    agent_tp_error,
    nta_msg_create_for_transport,
    agent_update_tport,
  }};


/** Add a transport to the agent.
 *
 * Creates a new transport and binds it
 * to the port specified by the @a uri. The @a uri must have sip: or sips:
 * scheme or be a wildcard uri ("*"). The @a uri syntax allowed is as
 * follows:
 *
 * @code url <scheme>:<host>[:<port>]<url-params> @endcode
 * where <url-params> may be
 * @code
 * ;transport=<xxx>
 * ;maddr=<actual addr>
 * ;comp=sigcomp
 * @endcode
 *
 * The scheme part determines which transports are used. "sip" implies UDP
 * and TCP, "sips" TLS over TCP. In the future, more transports can be
 * supported, for instance, "sip" can use SCTP or DCCP, "sips" DTLS or TLS
 * over SCTP.
 *
 * The "host" part determines what address/domain name is used in @Contact.
 * An "*" in "host" part is shorthand for any local IP address. 0.0.0.0
 * means that the only the IPv4 addresses are used. [::] means that only
 * the IPv6 addresses are used. If a domain name or a specific IP address
 * is given as "host" part, an additional "maddr" parameter can be used to
 * control which addresses are used by the stack when binding listen
 * sockets for incoming requests.
 *
 * The "port" determines what port is used in contact, and to which port the
 * stack binds in order to listen for incoming requests. Empty or missing
 * port means that default port should be used (5060 for sip, 5061 for
 * sips). An "*" in "port" part means any port, i.e., the stack binds to an
 * ephemeral port.
 *
 * The "transport" parameter determines the transport protocol that is used
 * and how they are preferred. If no protocol is specified, both UDP and TCP
 * are used for SIP URL and TLS for SIPS URL. The preference can be
 * indicated with a comma-separated list of transports, for instance,
 * parameter @code transport=tcp,udp @endcode indicates that TCP is
 * preferred to UDP.
 *
 * The "maddr" parameter determines to which address the stack binds in
 * order to listen for incoming requests. An "*" in "maddr" parameter is
 * shorthand for any local IP address. 0.0.0.0 means that only IPv4 sockets
 * are created. [::] means that only IPv6 sockets are created.
 *
 * The "comp" parameter determines the supported compression protocol.
 * Currently only sigcomp is supported (with suitable library).
 *
 * @par Examples:
 * @code sip:172.21.40.24;maddr=* @endcode \n
 * @code sip:172.21.40.24:50600;transport=TCP,UDP;comp=sigcomp @endcode \n
 * @code sips:* @endcode
 *
 * @return
 * On success, zero is returned. On error, -1 is returned, and @a errno is
 * set appropriately.
 */
int nta_agent_add_tport(nta_agent_t *self,
			url_string_t const *uri,
			tag_type_t tag, tag_value_t value, ...)
{
  url_t *url;
  char tp[32];
  char maddr[256];
  char comp[32];
  tp_name_t tpn[1] = {{ NULL }};
  char const * const * tports = tports_sip;
  int error;
  ta_list ta;

  if (self == NULL) {
    su_seterrno(EINVAL);
    return -1;
  }

  if (uri == NULL)
    uri = (url_string_t *)"sip:*";
  else if (url_string_p(uri) ?
	   strcmp(uri->us_str, "*") == 0 :
	   uri->us_url->url_type == url_any) {
    uri = (url_string_t *)"sip:*:*";
  }

  if (!(url = url_hdup(self->sa_home, uri->us_url)) ||
      (url->url_type != url_sip && url->url_type != url_sips)) {
    if (url_string_p(uri))
      SU_DEBUG_1(("nta: %s: invalid bind URL\n", uri->us_str));
    else
      SU_DEBUG_1(("nta: invalid bind URL\n" VA_NONE));
    su_seterrno(EINVAL);
    return -1;
  }

  tpn->tpn_canon = url->url_host;
  tpn->tpn_host = url->url_host;
  tpn->tpn_port = url_port(url);

  if (url->url_type == url_sip) {
    tpn->tpn_proto = "*";
    tports = tports_sip;
    if (!tpn->tpn_port || !tpn->tpn_port[0])
      tpn->tpn_port = SIP_DEFAULT_SERV;
  }
  else {
    assert(url->url_type == url_sips);
    tpn->tpn_proto = "*";
    tports = tports_sips;
    if (!tpn->tpn_port || !tpn->tpn_port[0])
      tpn->tpn_port = SIPS_DEFAULT_SERV;
  }

  if (url->url_params) {
    if (url_param(url->url_params, "transport", tp, sizeof(tp)) > 0) {
      if (strchr(tp, ',')) {
		  int i; char *t, *tps[9] = {0};

	/* Split tp into transports */
	for (i = 0, t = tp; t && i < 8; i++) {
	  tps[i] = t;
	  if ((t = strchr(t, ',')))
	    *t++ = '\0';
	}

	tps[i] = NULL;
	tports = (char const * const *)tps;
      } else {
	tpn->tpn_proto = tp;
      }
    }
    if (url_param(url->url_params, "maddr", maddr, sizeof(maddr)) > 0)
      tpn->tpn_host = maddr;
    if (url_param(url->url_params, "comp", comp, sizeof(comp)) > 0)
      tpn->tpn_comp = comp;

    if (tpn->tpn_comp &&
	(nta_compressor_vtable == NULL ||
	 !su_casematch(tpn->tpn_comp, nta_compressor_vtable->ncv_name))) {
      SU_DEBUG_1(("nta(%p): comp=%s not supported for " URL_PRINT_FORMAT "\n",
		  (void *)self, tpn->tpn_comp, URL_PRINT_ARGS(url)));
    }
  }

  ta_start(ta, tag, value);

  if (self->sa_tports == NULL) {
    if (agent_create_master_transport(self, ta_args(ta)) < 0) {
      error = su_errno();
      SU_DEBUG_1(("nta: cannot create master transport: %s\n",
		  su_strerror(error)));
      goto error;
    }
  }

  if (tport_tbind(self->sa_tports, tpn, tports, ta_tags(ta)) < 0) {
    error = su_errno();
    SU_DEBUG_1(("nta: bind(%s:%s;transport=%s%s%s%s%s): %s\n",
		tpn->tpn_canon, tpn->tpn_port, tpn->tpn_proto,
		tpn->tpn_canon != tpn->tpn_host ? ";maddr=" : "",
		tpn->tpn_canon != tpn->tpn_host ? tpn->tpn_host : "",
		tpn->tpn_comp ? ";comp=" : "",
		tpn->tpn_comp ? tpn->tpn_comp : "",
		su_strerror(error)));
    goto error;
  }
  else
    SU_DEBUG_5(("nta: bound to (%s:%s;transport=%s%s%s%s%s)\n",
		tpn->tpn_canon, tpn->tpn_port, tpn->tpn_proto,
		tpn->tpn_canon != tpn->tpn_host ? ";maddr=" : "",
		tpn->tpn_canon != tpn->tpn_host ? tpn->tpn_host : "",
		tpn->tpn_comp ? ";comp=" : "",
		tpn->tpn_comp ? tpn->tpn_comp : ""));

  /* XXX - when to use maddr? */
  if ((agent_init_via(self, tport_primaries(self->sa_tports), 0)) < 0) {
    error = su_errno();
    SU_DEBUG_1(("nta: cannot create Via headers\n" VA_NONE));
    goto error;
  }
  else
    SU_DEBUG_9(("nta: Via fields initialized\n" VA_NONE));

  if ((agent_init_contact(self)) < 0) {
    error = su_errno();
    SU_DEBUG_1(("nta: cannot create Contact header\n" VA_NONE));
    goto error;
  }
  else
    SU_DEBUG_9(("nta: Contact header created\n" VA_NONE));

  su_free(self->sa_home, url);
  ta_end(ta);

  return 0;

 error:
  ta_end(ta);
  su_seterrno(error);
  return -1;
}

static
int agent_create_master_transport(nta_agent_t *self, tagi_t *tags)
{
  self->sa_tports =
    tport_tcreate(self, nta_agent_class, self->sa_root,
		  TPTAG_IDLE(1800000),
		  TAG_NEXT(tags));

  if (!self->sa_tports)
    return -1;

  SU_DEBUG_9(("nta: master transport created\n" VA_NONE));

  return 0;
}


/** Initialize @Via headers. */
static
int agent_init_via(nta_agent_t *self, tport_t *primaries, int use_maddr)
{
  sip_via_t *via = NULL, *new_via, *dup_via, *v, **vv = &via;
  sip_via_t *new_vias, **next_new_via, *new_publics, **next_new_public;
  tport_t *tp;
  su_addrinfo_t const *ai;

  su_home_t autohome[SU_HOME_AUTO_SIZE(2048)];

  su_home_auto(autohome, sizeof autohome);

  self->sa_tport_ip4 = 0;
  self->sa_tport_ip6 = 0;
  self->sa_tport_udp = 0;
  self->sa_tport_tcp = 0;
  self->sa_tport_sctp = 0;
  self->sa_tport_tls = 0;
  self->sa_tport_ws = 0;
  self->sa_tport_wss = 0;

  /* Set via fields for the tports */
  for (tp = primaries; tp; tp = tport_next(tp)) {
    int maddr;
    tp_name_t tpn[1];
    char const *comp = NULL;

    *tpn = *tport_name(tp);

    assert(tpn->tpn_proto);
    assert(tpn->tpn_canon);
    assert(tpn->tpn_host);
    assert(tpn->tpn_port);

#if 0
    if (getenv("SIP_UDP_CONNECT")
	&& strcmp(tpn->tpn_proto, "udp") == 0)
      tport_set_params(tp, TPTAG_CONNECT(1), TAG_END());
#endif

    if (tport_has_ip4(tp)) self->sa_tport_ip4 = 1;

#if SU_HAVE_IN6
    if (tport_has_ip6(tp)) self->sa_tport_ip6 = 1;
#endif

    if (su_casematch(tpn->tpn_proto, "udp"))
      self->sa_tport_udp = 1;
    else if (su_casematch(tpn->tpn_proto, "tcp"))
      self->sa_tport_tcp = 1;
    else if (su_casematch(tpn->tpn_proto, "sctp"))
      self->sa_tport_sctp = 1;
    else if (su_casematch(tpn->tpn_proto, "ws"))
      self->sa_tport_ws = 1;
    else if (su_casematch(tpn->tpn_proto, "wss"))
      self->sa_tport_wss = 1;

    if (tport_has_tls(tp)) self->sa_tport_tls = 1;

    ai = tport_get_address(tp);

    for (; ai; ai = ai->ai_next) {
      char host[TPORT_HOSTPORTSIZE] = "";
      char sport[8];
      char const *canon = ai->ai_canonname;
      su_sockaddr_t *su = (void *)ai->ai_addr;
      int port;

      if (su) {
	su_inet_ntop(su->su_family, SU_ADDR(su), host, sizeof host);
	maddr = use_maddr && !su_casematch(canon, host);
	port = ntohs(su->su_port);
      }
      else {
	msg_random_token(host, 16, NULL, 0);
	canon = strcat(host, ".is.invalid");
	maddr = 0;
	port = 0;
      }

      if (su_casenmatch(tpn->tpn_proto, "tls", 3)
	  ? port == SIPS_DEFAULT_PORT
	  : port == SIP_DEFAULT_PORT)
	port = 0;

      snprintf(sport, sizeof sport, ":%u", port);

      comp = tpn->tpn_comp;

      SU_DEBUG_9(("nta: agent_init_via: "
		  "%s/%s %s%s%s%s%s%s (%s)\n",
		  SIP_VERSION_CURRENT, tpn->tpn_proto,
		  canon, port ? sport : "",
		  maddr ? ";maddr=" : "", maddr ? host : "",
		  comp ? ";comp=" : "", comp ? comp : "",
		  tpn->tpn_ident ? tpn->tpn_ident : "*"));

      v = sip_via_format(autohome,
			 "%s/%s %s%s%s%s%s%s",
			 SIP_VERSION_CURRENT, tpn->tpn_proto,
			 canon, port ? sport : "",
			 maddr ? ";maddr=" : "", maddr ? host : "",
			 comp ? ";comp=" : "", comp ? comp : "");
      if (v == NULL)
	goto error;

      v->v_comment = tpn->tpn_ident;
      v->v_common->h_data = tp;	/* Nasty trick */
      *vv = v; vv = &(*vv)->v_next;
    }
  }

  /* Duplicate the list bind to the transports */
  new_via = sip_via_dup(self->sa_home, via);
  /* Duplicate the complete list shown to the application */
  dup_via = sip_via_dup(self->sa_home, via);

  if (via && (!new_via || !dup_via)) {
    msg_header_free(self->sa_home, (void *)new_via);
    msg_header_free(self->sa_home, (void *)dup_via);
    goto error;
  }

  new_vias = NULL, next_new_via = &new_vias;
  new_publics = NULL, next_new_public = &new_publics;

  /* Set via field magic for the tports */
  for (tp = primaries; tp; tp = tport_next(tp)) {
    assert(via->v_common->h_data == tp);
    v = tport_magic(tp);
    tport_set_magic(tp, new_via);
    msg_header_free(self->sa_home, (void *)v);

    if (tport_is_public(tp))
      *next_new_public = dup_via;
    else
      *next_new_via = dup_via;

    while (via->v_next && via->v_next->v_common->h_data == tp)
      via = via->v_next, new_via = new_via->v_next, dup_via = dup_via->v_next;

    via = via->v_next;
    /* Break the link in via list between transports */
    vv = &new_via->v_next, new_via = *vv, *vv = NULL;
    vv = &dup_via->v_next, dup_via = *vv, *vv = NULL;

    if (tport_is_public(tp))
      while (*next_new_public) next_new_public = &(*next_new_public)->v_next;
    else
      while (*next_new_via) next_new_via = &(*next_new_via)->v_next;
  }

  assert(dup_via == NULL);
  assert(new_via == NULL);

  if (self->sa_tport_udp)
    agent_set_udp_params(self, self->sa_udp_mtu);

  v = self->sa_vias;
  self->sa_vias = new_vias;
  msg_header_free(self->sa_home, (void *)v);

  v = self->sa_public_vias;
  self->sa_public_vias = new_publics;
  msg_header_free(self->sa_home, (void *)v);

  su_home_deinit(autohome);

  return 0;

 error:
  su_home_deinit(autohome);
  return -1;
}


/** Initialize main contact header. */
static
int agent_init_contact(nta_agent_t *self)
{
  sip_via_t const *v1, *v2;
  char const *tp;

  if (self->sa_contact)
    return 0;

  for (v1 = self->sa_vias ? self->sa_vias : self->sa_public_vias;
       v1;
       v1 = v1->v_next) {
    if (host_is_ip_address(v1->v_host)) {
      if (!host_is_local(v1->v_host))
	break;
    }
    else if (!host_has_domain_invalid(v1->v_host)) {
      break;
    }
  }

  if (v1 == NULL)
    v1 = self->sa_vias ? self->sa_vias : self->sa_public_vias;

  if (!v1)
    return -1;

  tp = strrchr(v1->v_protocol, '/');
  if (!tp++)
    return -1;

  v2 = v1->v_next;

  if (v2 &&
      su_casematch(v1->v_host, v2->v_host) &&
      su_casematch(v1->v_port, v2->v_port)) {
    char const *p1 = v1->v_protocol, *p2 = v2->v_protocol;

    if (!su_casematch(p1, sip_transport_udp))
      p1 = v2->v_protocol, p2 = v1->v_protocol;

    if (su_casematch(p1, sip_transport_udp) &&
	su_casematch(p2, sip_transport_tcp))
      /* Do not include transport if we have both UDP and TCP */
      tp = NULL;
  }

  self->sa_contact =
    sip_contact_create_from_via_with_transport(self->sa_home, v1, NULL, tp);

  if (!self->sa_contact)
    return -1;

  agent_tag_init(self);

  return 0;
}

/** Return @Via line corresponging to tport. */
static
sip_via_t const *agent_tport_via(tport_t *tport)
{
  sip_via_t *v = tport_magic(tport);
  while (v && v->v_next)
    v = v->v_next;
  return v;
}

/** Insert @Via to a request message */
static
int outgoing_insert_via(nta_outgoing_t *orq,
			sip_via_t const *via)
{
  nta_agent_t *self = orq->orq_agent;
  msg_t *msg = orq->orq_request;
  sip_t *sip = sip_object(msg);
  char const *branch = orq->orq_via_branch;
  int already = orq->orq_user_via || orq->orq_via_added;
  int user_via = orq->orq_user_via;
  sip_via_t *v;
  int clear = 0;

  assert(sip); assert(via);

  if (already && sip->sip_via) {
    /* Use existing @Via */
    v = sip->sip_via;
  }
  else if (msg && via && sip->sip_request &&
	   (v = sip_via_copy(msg_home(msg), via))) {
    if (msg_header_insert(msg, (msg_pub_t *)sip, (msg_header_t *)v) < 0)
      return -1;
    orq->orq_via_added = 1;
  }
  else
    return -1;

  if (!v->v_rport &&
      ((self->sa_rport && v->v_protocol == sip_transport_udp) ||
       (self->sa_tcp_rport && v->v_protocol == sip_transport_tcp) ||
       (self->sa_tls_rport && v->v_protocol == sip_transport_tls)))
    msg_header_add_param(msg_home(msg), v->v_common, "rport");

  if (!orq->orq_tpn->tpn_comp)
    msg_header_remove_param(v->v_common, "comp");

  if (branch && branch != v->v_branch) {
    char const *bvalue = branch + strcspn(branch, "=");
    if (*bvalue) bvalue++;
    if (!v->v_branch || !su_casematch(bvalue, v->v_branch))
      msg_header_replace_param(msg_home(msg), v->v_common, branch);
  }

  if (!su_casematch(via->v_protocol, v->v_protocol))
    clear = 1, v->v_protocol = via->v_protocol;

  /* XXX - should we do this? */
  if ((!user_via || !v->v_host) &&
      !su_strmatch(via->v_host, v->v_host))
    clear = 1, v->v_host = via->v_host;

  if ((!user_via || !v->v_port ||
       /* Replace port in user Via only if we use udp and no rport */
       (v->v_protocol == sip_transport_udp && !v->v_rport &&
	!orq->orq_stateless)) &&
      !su_strmatch(via->v_port, v->v_port))
    clear = 1, v->v_port = via->v_port;

  if (clear)
    msg_fragment_clear(v->v_common);

  return 0;
}

/** Get destination name from @Via.
 *
 * If @a using_rport is non-null, try rport.
 * If *using_rport is non-zero, try rport even if <protocol> is not UDP.
 * If <protocol> is UDP, set *using_rport to zero.
 */
static
int nta_tpn_by_via(tp_name_t *tpn, sip_via_t const *v, int *using_rport)
{
  if (!v)
    return -1;

  tpn->tpn_proto = sip_via_transport(v);
  tpn->tpn_canon = v->v_host;

  if (v->v_maddr)
    tpn->tpn_host = v->v_maddr;
  else if (v->v_received)
    tpn->tpn_host = v->v_received;
  else
    tpn->tpn_host = v->v_host;

  tpn->tpn_port = sip_via_port(v, using_rport);
  tpn->tpn_comp = v->v_comp;
  tpn->tpn_ident = NULL;

  return 0;
}

/** Get transport name from URL. */
static int
nta_tpn_by_url(su_home_t *home,
	       tp_name_t *tpn,
	       char const **scheme,
	       char const **port,
	       url_string_t const *us)
{
  url_t url[1];
  isize_t n;
  char *b;

  n = url_xtra(us->us_url);
  b = su_alloc(home, n);

  if (b == NULL || url_dup(b, n, url, us->us_url) < 0) {
    su_free(home, b);
    return -1;
  }

  if (url->url_type != url_sip &&
      url->url_type != url_sips &&
      url->url_type != url_im &&
      url->url_type != url_pres) {
    su_free(home, b);
    return -1;
  }

  SU_DEBUG_7(("nta: selecting scheme %s\n", url->url_scheme));

  *scheme = url->url_scheme;

  tpn->tpn_proto = NULL;
  tpn->tpn_canon = url->url_host;
  tpn->tpn_host = url->url_host;

  if (url->url_params) {
    for (b = (char *)url->url_params; b[0]; b += n) {
      n = strcspn(b, ";");

      if (n > 10 && su_casenmatch(b, "transport=", 10))
	tpn->tpn_proto = b + 10;
      else if (n > 5 && su_casenmatch(b, "comp=", 5))
	tpn->tpn_comp = b + 5;
      else if (n > 6 && su_casenmatch(b, "maddr=", 6))
	tpn->tpn_host = b + 6;

      if (b[n])
	b[n++] = '\0';
    }
  }

  if ((*port = url->url_port))
    tpn->tpn_port = url->url_port;

  tpn->tpn_ident = NULL;

  if (tpn->tpn_proto) {
	  if (su_casematch(url->url_scheme, "sips") && su_casematch(tpn->tpn_proto, "ws")) {
		  tpn->tpn_proto = "wss";
	  }
    return 1;
  }

  if (su_casematch(url->url_scheme, "sips"))
    tpn->tpn_proto = "tls";
  else
    tpn->tpn_proto = "*";

  return 0;
}

/** Handle transport errors. */
static
void agent_tp_error(nta_agent_t *agent,
		    tport_t *tport,
		    int errcode,
		    char const *remote)
{
  su_llog(nta_log, 1,
	  "nta_agent: tport: %s%s%s\n",
	  remote ? remote : "", remote ? ": " : "",
	  su_strerror(errcode));

  if (agent->sa_error_tport) {
    agent->sa_error_tport(agent->sa_error_magic, agent, tport);
  }
}

/** Handle updated transport addresses */
static void agent_update_tport(nta_agent_t *self, tport_t *tport)
{
  /* Initialize local Vias first */
  agent_init_via(self, tport_primaries(self->sa_tports), 0);

  if (self->sa_update_tport) {
    self->sa_update_tport(self->sa_update_magic, self);
  }
  else {
    /* XXX - we should do something else? */
    SU_DEBUG_3(("%s(%p): %s\n", "nta", (void *)self,
		"transport address updated"));
  }
}

/* ====================================================================== */
/* 3) Message dispatch */

static void agent_recv_request(nta_agent_t *agent,
			       msg_t *msg,
			       sip_t *sip,
			       tport_t *tport);
static int agent_check_request_via(nta_agent_t *agent,
				   msg_t *msg,
				   sip_t *sip,
				   sip_via_t *v,
				   tport_t *tport);
static int agent_aliases(nta_agent_t const *, url_t [], tport_t *);
static void agent_recv_response(nta_agent_t*, msg_t *, sip_t *,
				sip_via_t *, tport_t*);
static void agent_recv_garbage(nta_agent_t*, msg_t*, tport_t*);

#if HAVE_SOFIA_SRESOLV
static void outgoing_resolve(nta_outgoing_t *orq,
			     int explicit_transport,
			     enum nta_res_order_e order);
su_inline void outgoing_cancel_resolver(nta_outgoing_t *orq);
su_inline void outgoing_destroy_resolver(nta_outgoing_t *orq);
static int outgoing_other_destinations(nta_outgoing_t const *orq);
static int outgoing_try_another(nta_outgoing_t *orq);
#else
#define outgoing_other_destinations(orq) (0)
#define outgoing_try_another(orq) (0)
#endif

/** Handle incoming message. */
static
void agent_recv_message(nta_agent_t *agent,
			tport_t *tport,
			msg_t *msg,
			sip_via_t *tport_via,
			su_time_t now)
{
  sip_t *sip = sip_object(msg);

  agent->sa_millisec = su_time_ms(agent->sa_now = now);

  if (sip && sip->sip_request) {
    agent_recv_request(agent, msg, sip, tport);
  }
  else if (sip && sip->sip_status) {
    agent_recv_response(agent, msg, sip, tport_via, tport);
  }
  else {
    agent_recv_garbage(agent, msg, tport);
  }

  agent->sa_millisec = 0;
}

/** @internal Handle incoming requests. */
static
void agent_recv_request(nta_agent_t *agent,
			msg_t *msg,
			sip_t *sip,
			tport_t *tport)
{
  nta_leg_t *leg;
  nta_incoming_t *irq, *merge = NULL, *ack = NULL, *cancel = NULL;
  sip_method_t method = sip->sip_request->rq_method;
  char const *method_name = sip->sip_request->rq_method_name;
  url_t url[1];
  unsigned cseq = sip->sip_cseq ? sip->sip_cseq->cs_seq : 0;
  int insane, errors, stream;

  agent->sa_stats->as_recv_msg++;
  agent->sa_stats->as_recv_request++;

  SU_DEBUG_5(("nta: received %s " URL_PRINT_FORMAT " %s (CSeq %u)\n",
	      method_name,
	      URL_PRINT_ARGS(sip->sip_request->rq_url),
	      sip->sip_request->rq_version, cseq));

  if (agent->sa_drop_prob && !tport_is_reliable(tport)) {
    if ((unsigned)su_randint(0, 1000) < agent->sa_drop_prob) {
      SU_DEBUG_5(("nta: %s (%u) is %s\n",
		  method_name, cseq, "dropped simulating packet loss"));
      agent->sa_stats->as_drop_request++;
      msg_destroy(msg);
      return;
    }
  }

  stream = tport_is_stream(tport);

  /* Try to use compression on reverse direction if @Via has comp=sigcomp  */
  if (stream &&
      sip->sip_via && sip->sip_via->v_comp &&
      tport_can_send_sigcomp(tport) &&
      tport_name(tport)->tpn_comp == NULL &&
      tport_has_compression(tport_parent(tport), sip->sip_via->v_comp)) {
    tport_set_compression(tport, sip->sip_via->v_comp);
  }

  if (sip->sip_flags & MSG_FLG_TOOLARGE) {
    SU_DEBUG_5(("nta: %s (%u) is %s\n",
		method_name, cseq, sip_413_Request_too_large));
    agent->sa_stats->as_bad_request++;
    mreply(agent, NULL, SIP_413_REQUEST_TOO_LARGE, msg,
	   tport, 1, stream, NULL,
	   TAG_END());
    return;
  }

  insane = 0;

  if (agent->sa_bad_req_mask != ~0U)
    errors = msg_extract_errors(msg) & agent->sa_bad_req_mask;
  else
    errors = sip->sip_error != NULL;

  if (errors ||
      (sip->sip_flags & MSG_FLG_ERROR) /* Fatal error */ ||
      (insane = (sip_sanity_check(sip) < 0))) {
    sip_header_t const *h;
    char const *badname = NULL, *phrase;

    agent->sa_stats->as_bad_message++;
    agent->sa_stats->as_bad_request++;

    if (insane)
      SU_DEBUG_5(("nta: %s (%u) %s\n", method_name, cseq,
		  "failed sanity check"));

    for (h = (sip_header_t const *)sip->sip_error; h; h = h->sh_next) {
      char const *bad;

      if (h->sh_class == sip_error_class)
	bad = h->sh_error->er_name;
      else
	bad = h->sh_class->hc_name;

      if (bad)
	SU_DEBUG_5(("nta: %s has bad %s header\n", method_name, bad));

      if (!badname)
	badname = bad;
    }

    if (sip->sip_via && method != sip_method_ack) {
      msg_t *reply = nta_msg_create(agent, 0);

      agent_check_request_via(agent, msg, sip, sip->sip_via, tport);

      if (badname && reply)
	phrase = su_sprintf(msg_home(reply), "Bad %s Header", badname);
      else
	phrase = sip_400_Bad_request;

      SU_DEBUG_5(("nta: %s (%u) is %s\n", method_name, cseq, phrase));

      mreply(agent, reply, 400, phrase, msg,
	     tport, 1, stream, NULL,
	     TAG_END());
    }
    else {
      msg_destroy(msg);
      if (stream)		/* Send FIN */
	tport_shutdown(tport, 1);
    }

    return;
  }

  if (!su_casematch(sip->sip_request->rq_version, sip_version_2_0)) {
    agent->sa_stats->as_bad_request++;
    agent->sa_stats->as_bad_message++;

    SU_DEBUG_5(("nta: bad version %s for %s (%u)\n",
		sip->sip_request->rq_version, method_name, cseq));

    mreply(agent, NULL, SIP_505_VERSION_NOT_SUPPORTED, msg,
	   tport, 0, stream, NULL,
	   TAG_END());

    return;
  }

  if (agent_check_request_via(agent, msg, sip, sip->sip_via, tport) < 0) {
    agent->sa_stats->as_bad_message++;
    agent->sa_stats->as_bad_request++;
    SU_DEBUG_5(("nta: %s (%u) %s\n", method_name, cseq, "has invalid Via"));
    msg_destroy(msg);
    return;
  }

  /* First, try existing incoming requests */
  irq = incoming_find(agent, sip, sip->sip_via,
		      agent->sa_merge_482 &&
		      !sip->sip_to->a_tag &&
		      method != sip_method_ack
		      ? &merge
		      : NULL,
		      method == sip_method_ack ? &ack : NULL,
		      method == sip_method_cancel ? &cancel : NULL);

  if (irq) {
    /* Match - this is a retransmission */
    SU_DEBUG_5(("nta: %s (%u) going to existing %s transaction\n",
		method_name, cseq, irq->irq_rq->rq_method_name));
    if (incoming_recv(irq, msg, sip, tport) >= 0)
      return;
  }
  else if (ack) {
    SU_DEBUG_5(("nta: %s (%u) is going to %s (%u)\n",
		method_name, cseq,
		ack->irq_cseq->cs_method_name, ack->irq_cseq->cs_seq));
    if (incoming_ack(ack, msg, sip, tport) >= 0)
      return;
  }
  else if (cancel) {
    SU_DEBUG_5(("nta: %s (%u) is going to %s (%u)\n",
		method_name, cseq,
		cancel->irq_cseq->cs_method_name, cancel->irq_cseq->cs_seq));
    if (incoming_cancel(cancel, msg, sip, tport) >= 0)
      return;
  }
  else if (merge) {
    SU_DEBUG_5(("nta: %s (%u) %s\n",
		method_name, cseq, "is a merged request"));
    request_merge(agent, msg, sip, tport, merge->irq_tag);
    return;
  }

  if (method == sip_method_prack && sip->sip_rack) {
    nta_reliable_t *rel = reliable_find(agent, sip);
    if (rel) {
      SU_DEBUG_5(("nta: %s (%u) is going to %s (%u)\n",
		  method_name, cseq,
		  rel->rel_irq->irq_cseq->cs_method_name,
		  rel->rel_irq->irq_cseq->cs_seq));
      reliable_recv(rel, msg, sip, tport);
      return;
    }
  }

  *url = *sip->sip_request->rq_url;
  url->url_params = NULL;
  agent_aliases(agent, url, tport); /* canonize urls */

  if (method != sip_method_subscribe && (leg = leg_find(agent,
		      method_name, url,
		      sip->sip_call_id,
		      sip->sip_from->a_tag,
		      sip->sip_to->a_tag))) {
    /* Try existing dialog */
    SU_DEBUG_5(("nta: %s (%u) %s\n",
		method_name, cseq, "going to existing leg"));
    leg_recv(leg, msg, sip, tport);
    return;
  }
  else if (!agent->sa_is_stateless &&
	   (leg = dst_find(agent, url, method_name))) {
    /* Dialogless legs - let application process transactions statefully */
    SU_DEBUG_5(("nta: %s (%u) %s\n",
		method_name, cseq, "going to a dialogless leg"));
    leg_recv(leg, msg, sip, tport);
  }
  else if (!agent->sa_is_stateless && (leg = agent->sa_default_leg)) {
    if (method == sip_method_invite &&
	agent->sa_in.proceeding->q_length >= agent->sa_max_proceeding) {
      SU_DEBUG_5(("nta: proceeding queue full for %s (%u)\n",
		  method_name, cseq));
      mreply(agent, NULL, SIP_503_SERVICE_UNAVAILABLE, msg,
	     tport, 0, 0, NULL,
	     TAG_END());
      return;
    }
    else {
      SU_DEBUG_5(("nta: %s (%u) %s\n",
		  method_name, cseq, "going to a default leg"));
      leg_recv(leg, msg, sip, tport);
    }
  }
  else if (agent->sa_callback) {
    /* Stateless processing for request */
    agent->sa_stats->as_trless_request++;
    SU_DEBUG_5(("nta: %s (%u) %s\n",
		method_name, cseq, "to message callback"));
    (void)agent->sa_callback(agent->sa_magic, agent, msg, sip);
  }
  else {
    agent->sa_stats->as_trless_request++;
    SU_DEBUG_5(("nta: %s (%u) %s\n",
		method_name, cseq,
		"not processed by application: returning 501"));
    if (method != sip_method_ack)
      mreply(agent, NULL, SIP_501_NOT_IMPLEMENTED, msg,
	     tport, 0, 0, NULL,
	     TAG_END());
    else
      msg_destroy(msg);
  }
}

/** Check @Via header.
 *
 */
static
int agent_check_request_via(nta_agent_t *agent,
			    msg_t *msg,
			    sip_t *sip,
			    sip_via_t *v,
			    tport_t *tport)
{
  enum { receivedlen = sizeof("received=") - 1 };
  char received[receivedlen + TPORT_HOSTPORTSIZE];
  char *hostport = received + receivedlen;
  char const *rport;
  su_sockaddr_t const *from;
  sip_via_t const *tpv = agent_tport_via(tport);

  assert(tport); assert(msg); assert(sip);
  assert(sip->sip_request); assert(tpv);

  from = msg_addr(msg);

  if (v == NULL) {
    /* Make up a via line */
    v = sip_via_format(msg_home(msg), "SIP/2.0/%s %s",
		       tport_name(tport)->tpn_proto,
		       tport_hostport(hostport, TPORT_HOSTPORTSIZE, from, 1));
    msg_header_insert(msg, (msg_pub_t *)sip, (msg_header_t *)v);

    return v ? 0 : -1;
  }

  if (!su_strmatch(v->v_protocol, tpv->v_protocol)) {
    tport_hostport(hostport, TPORT_HOSTPORTSIZE, from, 1);
    SU_DEBUG_1(("nta: Via check: invalid transport \"%s\" from %s\n",
		v->v_protocol, hostport));
    return -1;
  }

  if (v->v_received) {
    /* Nasty, nasty */
    tport_hostport(hostport, TPORT_HOSTPORTSIZE, from, 1);
    SU_DEBUG_1(("nta: Via check: extra received=%s from %s\n",
		v->v_received, hostport));
    msg_header_remove_param(v->v_common, "received");
  }

  if (!tport_hostport(hostport, TPORT_HOSTPORTSIZE, from, 0))
    return -1;

  if (!su_casematch(hostport, v->v_host)) {
    size_t rlen;
    /* Add the "received" field */
    memcpy(received, "received=", receivedlen);

    if (hostport[0] == '[') {
      rlen = strlen(hostport + 1) - 1;
      memmove(hostport, hostport + 1, rlen);
      hostport[rlen] = '\0';
    }

    msg_header_replace_param(msg_home(msg), v->v_common,
			     su_strdup(msg_home(msg), received));
    SU_DEBUG_5(("nta: Via check: %s\n", received));
  }

  if (!agent->sa_server_rport) {
    /*Xyzzy*/;
  }
  else if (v->v_rport) {
    rport = su_sprintf(msg_home(msg), "rport=%u", ntohs(from->su_port));
    msg_header_replace_param(msg_home(msg), v->v_common, rport);
  }
  else if (tport_is_tcp(tport)) {
    rport = su_sprintf(msg_home(msg), "rport=%u", ntohs(from->su_port));
    msg_header_replace_param(msg_home(msg), v->v_common, rport);
  }
  else if (agent->sa_server_rport == 2 ||
		   (agent->sa_server_rport == 3 && sip && sip->sip_user_agent &&
			sip->sip_user_agent->g_string &&
			(!strncasecmp(sip->sip_user_agent->g_string, "Polycom", 7) || !strncasecmp(sip->sip_user_agent->g_string, "KIRK Wireless Server", 20)))) {
    rport = su_sprintf(msg_home(msg), "rport=%u", ntohs(from->su_port));
    msg_header_replace_param(msg_home(msg), v->v_common, rport);
  }

  return 0;
}

/** @internal Handle aliases of local node.
 *
 * Return true if @a url is modified.
 */
static
int agent_aliases(nta_agent_t const *agent, url_t url[], tport_t *tport)
{
  sip_contact_t *m;
  sip_via_t const *lv;
  char const *tport_port = "";

  if (!url->url_host)
    return 0;

  if (tport)
    tport_port = tport_name(tport)->tpn_port;

  assert(tport_port);

  for (m = agent->sa_aliases ? agent->sa_aliases : agent->sa_contact;
       m;
       m = m->m_next) {
    if (url->url_type != m->m_url->url_type)
      continue;

    if (host_cmp(url->url_host, m->m_url->url_host))
      continue;

    if (url->url_port == NULL)
      break;

    if (m->m_url->url_port) {
      if (strcmp(url->url_port, m->m_url->url_port))
	continue;
    } else {
      if (strcmp(url->url_port, tport_port))
	continue;
    }

    break;
  }

  if (!m)
    return 0;

  SU_DEBUG_7(("nta: canonizing " URL_PRINT_FORMAT " with %s\n",
	      URL_PRINT_ARGS(url),
	      agent->sa_aliases ? "aliases" : "contact"));

  url->url_host = "%";

  if (agent->sa_aliases) {
    url->url_type = agent->sa_aliases->m_url->url_type;
    url->url_scheme = agent->sa_aliases->m_url->url_scheme;
    url->url_port = agent->sa_aliases->m_url->url_port;
    return 1;
  }
  else {
    /* Canonize the request URL port */
    if (tport) {
      lv = agent_tport_via(tport_parent(tport)); assert(lv);
      if (lv->v_port)
	/* Add non-default port */
	url->url_port = lv->v_port;
      return 1;
    }
    if (su_strmatch(url->url_port, url_port_default((enum url_type_e)url->url_type)) ||
	su_strmatch(url->url_port, ""))
      /* Remove default or empty port */
      url->url_port = NULL;

    return 0;
  }
}

/** @internal Handle incoming responses. */
static
void agent_recv_response(nta_agent_t *agent,
                         msg_t *msg,
                         sip_t *sip,
                         sip_via_t *tport_via,
                         tport_t *tport)
{
  int status = sip->sip_status->st_status;
  int errors;
  char const *phrase = sip->sip_status->st_phrase;
  char const *method =
    sip->sip_cseq ? sip->sip_cseq->cs_method_name : "<UNKNOWN>";
  uint32_t cseq = sip->sip_cseq ? sip->sip_cseq->cs_seq : 0;
  nta_outgoing_t *orq;
  su_home_t *home;
  char const *branch = NONE;


  agent->sa_stats->as_recv_msg++;
  agent->sa_stats->as_recv_response++;

  SU_DEBUG_5(("nta: received %03d %s for %s (%u)\n",
	      status, phrase, method, cseq));

  if (agent->sa_drop_prob && !tport_is_reliable(tport)) {
    if ((unsigned)su_randint(0, 1000) < agent->sa_drop_prob) {
      SU_DEBUG_5(("nta: %03d %s %s\n",
		  status, phrase, "dropped simulating packet loss"));
      agent->sa_stats->as_drop_response++;
      msg_destroy(msg);
      return;
    }
  }

  if (agent->sa_bad_resp_mask)
    errors = msg_extract_errors(msg) & agent->sa_bad_resp_mask;
  else
    errors = sip->sip_error != NULL;

  if (errors ||
      sip_sanity_check(sip) < 0) {
    sip_header_t const *h;

    agent->sa_stats->as_bad_response++;
    agent->sa_stats->as_bad_message++;

    SU_DEBUG_5(("nta: %03d %s %s\n", status, phrase,
		errors
		? "has fatal syntax errors"
		: "failed sanity check"));

    for (h = (sip_header_t const *)sip->sip_error; h; h = h->sh_next) {
      if (h->sh_class->hc_name) {
	SU_DEBUG_5(("nta: %03d has bad %s header\n", status,
		    h->sh_class->hc_name));
      }
    }

    msg_destroy(msg);
    return;
  }

  if (!su_casematch(sip->sip_status->st_version, sip_version_2_0)) {
    agent->sa_stats->as_bad_response++;
    agent->sa_stats->as_bad_message++;

    SU_DEBUG_5(("nta: bad version %s %03d %s\n",
		sip->sip_status->st_version, status, phrase));
    msg_destroy(msg);
    return;
  }

  if (sip->sip_cseq->cs_method == sip_method_ack) {
    /* Drop response messages to ACK */
    agent->sa_stats->as_bad_response++;
    agent->sa_stats->as_bad_message++;
    SU_DEBUG_5(("nta: %03d %s %s\n", status, phrase, "is response to ACK"));
    msg_destroy(msg);
    return;
  }

  /* XXX - should check if msg should be discarded based on via? */

  if ((orq = outgoing_find(agent, msg, sip, sip->sip_via))) {
    SU_DEBUG_5(("nta: %03d %s %s\n",
		status, phrase, "is going to a transaction"));
      /* RFC3263 4.3 "503 error response" */
      if(agent->sa_srv_503 && status == 503 && outgoing_other_destinations(orq)) {
              SU_DEBUG_5(("%s(%p): <%03d> for <%s>, %s\n", "nta", (void *)orq, status, method, "try next after timeout"));
              home = msg_home(msg);
              if (agent->sa_is_stateless)
                    branch = stateless_branch(agent, msg, sip, orq->orq_tpn);
              else
                    branch = stateful_branch(home, agent);

             orq->orq_branch = branch;
             orq->orq_via_branch = branch;
             outgoing_try_another(orq);
             return;
      }						
      		
     if (outgoing_recv(orq, status, msg, sip) == 0)
      return;
  }


  agent->sa_stats->as_trless_response++;

  if ((orq = agent->sa_default_outgoing)) {
    SU_DEBUG_5(("nta: %03d %s %s\n", status, phrase,
		"to the default transaction"));
    outgoing_default_recv(orq, status, msg, sip);
    return;
  }
  else if (agent->sa_callback) {
    SU_DEBUG_5(("nta: %03d %s %s\n", status, phrase, "to message callback"));
    /*
     * Store message and transport to hook for the duration of the callback
     * so that the transport can be obtained by nta_transport().
     */
    (void)agent->sa_callback(agent->sa_magic, agent, msg, sip);
    return;
  }

  if (sip->sip_cseq->cs_method == sip_method_invite
      && 200 <= sip->sip_status->st_status
      && sip->sip_status->st_status < 300
      /* Exactly one Via header, belonging to us */
      && sip->sip_via && !sip->sip_via->v_next
      && agent_has_via(agent, sip->sip_via)) {
    agent->sa_stats->as_trless_200++;
  }

  SU_DEBUG_5(("nta: %03d %s %s\n", status, phrase, "was discarded"));
  msg_destroy(msg);
}

/** @internal Agent receives garbage */
static
void agent_recv_garbage(nta_agent_t *agent,
			msg_t *msg,
			tport_t *tport)
{
  agent->sa_stats->as_recv_msg++;
  agent->sa_stats->as_bad_message++;

#if SU_DEBUG >= 3
  if (nta_log->log_level >= 3) {
    tp_name_t tpn[1];

    tport_delivered_from(tport, msg, tpn);

    SU_DEBUG_3(("nta_agent: received garbage from " TPN_FORMAT "\n",
		TPN_ARGS(tpn)));
  }
#endif

  msg_destroy(msg);
}

/* ====================================================================== */
/* 4) Message handling - create, complete, destroy */

/** Create a new message belonging to the agent */
msg_t *nta_msg_create(nta_agent_t *agent, int flags)
{
  msg_t *msg;

  if (agent == NULL)
    return su_seterrno(EINVAL), NULL;

  msg = msg_create(agent->sa_mclass, agent->sa_flags | flags);

  if (agent->sa_preload)
    su_home_preload(msg_home(msg), 1, agent->sa_preload);

  return msg;
}

/** Create a new message for transport */
msg_t *nta_msg_create_for_transport(nta_agent_t *agent, int flags,
				    char const data[], usize_t dlen,
				    tport_t const *tport, tp_client_t *via)
{
  msg_t *msg = msg_create(agent->sa_mclass, agent->sa_flags | flags);

  msg_maxsize(msg, agent->sa_maxsize);

  if (agent->sa_preload)
    su_home_preload(msg_home(msg), 1, dlen + agent->sa_preload);

  return msg;
}

/** Complete a message. */
int nta_msg_complete(msg_t *msg)
{
  return sip_complete_message(msg);
}

/** Discard a message */
void nta_msg_discard(nta_agent_t *agent, msg_t *msg)
{
  msg_destroy(msg);
}

/** Check if the headers are from response generated locally by NTA. */
int  nta_sip_is_internal(sip_t const *sip)
{
  return
    sip == NULL		/* No message generated */
    || (sip->sip_flags & NTA_INTERNAL_MSG) == NTA_INTERNAL_MSG;
}

/** Check if the message is internally generated by NTA. */
int nta_msg_is_internal(msg_t const *msg)
{
  return msg_get_flags(msg, NTA_INTERNAL_MSG) == NTA_INTERNAL_MSG;
}

/** Check if the message is internally generated by NTA.
 *
 * @deprecated Use nta_msg_is_internal() instead
 */
int  nta_is_internal_msg(msg_t const *msg) { return nta_msg_is_internal(msg); }

/* ====================================================================== */
/* 5) Stateless operation */

/**Forward a request or response message.
 *
 * @note
 * The ownership of @a msg is taken over by the function even if the
 * function fails.
 */
int nta_msg_tsend(nta_agent_t *agent, msg_t *msg, url_string_t const *u,
		  tag_type_t tag, tag_value_t value, ...)
{
  int retval = -1;
  ta_list ta;
  sip_t *sip = sip_object(msg);
  tp_name_t tpn[1] = {{ NULL }};
  char const *what;

  if (!sip) {
    msg_destroy(msg);
    return -1;
  }

  what =
    sip->sip_status ? "nta_msg_tsend(response)" :
    sip->sip_request ? "nta_msg_tsend(request)" :
    "nta_msg_tsend()";

  ta_start(ta, tag, value);

  if (sip_add_tl(msg, sip, ta_tags(ta)) < 0)
    SU_DEBUG_3(("%s: cannot add headers\n", what));
  else if (sip->sip_status) {
    tport_t *tport = NULL;
    int *use_rport = NULL;
    int retry_without_rport = 0;

    struct sigcomp_compartment *cc; cc = NONE;

    if (agent->sa_server_rport)
      use_rport = &retry_without_rport, retry_without_rport = 1;

    tl_gets(ta_args(ta),
	    NTATAG_TPORT_REF(tport),
	    IF_SIGCOMP_TPTAG_COMPARTMENT_REF(cc)
	    /* NTATAG_INCOMPLETE_REF(incomplete), */
	    TAG_END());

    if (!sip->sip_separator &&
	!(sip->sip_separator = sip_separator_create(msg_home(msg))))
      SU_DEBUG_3(("%s: cannot create sip_separator\n", what));
    else if (msg_serialize(msg, (msg_pub_t *)sip) != 0)
      SU_DEBUG_3(("%s: sip_serialize() failed\n", what));
    else if (!sip_via_remove(msg, sip))
      SU_DEBUG_3(("%s: cannot remove Via\n", what));
    else if (nta_tpn_by_via(tpn, sip->sip_via, use_rport) < 0)
      SU_DEBUG_3(("%s: bad via\n", what));
    else {
      if (!tport)
	tport = tport_by_name(agent->sa_tports, tpn);
      if (!tport)
	tport = tport_by_protocol(agent->sa_tports, tpn->tpn_proto);

      if (retry_without_rport)
	tpn->tpn_port = sip_via_port(sip->sip_via, NULL);

      if (tport && tpn->tpn_comp && cc == NONE)
	cc = agent_compression_compartment(agent, tport, tpn, -1);

      if (tport_tsend(tport, msg, tpn,
		      IF_SIGCOMP_TPTAG_COMPARTMENT(cc)
		      TPTAG_MTU(INT_MAX), ta_tags(ta), TAG_END())) {
	agent->sa_stats->as_sent_msg++;
	agent->sa_stats->as_sent_response++;
	retval = 0;
      }
      else {
	SU_DEBUG_3(("%s: send fails\n", what));
      }
    }
  }
  else {
    /* Send request */
    if (outgoing_create(agent, NULL, NULL, u, NULL, msg_ref_create(msg),
			NTATAG_STATELESS(1),
			ta_tags(ta)))
      retval = 0;
  }

  if (retval == 0)
    SU_DEBUG_5(("%s\n", what));

  ta_end(ta);

  msg_destroy(msg);

  return retval;
}

/** Reply to a request message.
 *
 * @param agent    nta agent object
 * @param req_msg  request message
 * @param status   status code
 * @param phrase   status phrase (may be NULL if status code is well-known)
 * @param tag, value, ... optional additional headers terminated by TAG_END()
 *
 * @retval 0 when succesful
 * @retval -1 upon an error
 *
 * @note
 * The ownership of @a msg is taken over by the function even if the
 * function fails.
 */
int nta_msg_treply(nta_agent_t *agent,
		   msg_t *req_msg,
		   int status, char const *phrase,
		   tag_type_t tag, tag_value_t value, ...)
{
  int retval;
  ta_list ta;

  ta_start(ta, tag, value);

  retval = mreply(agent, NULL, status, phrase, req_msg,
		  NULL, 0, 0, NULL,
		  ta_tags(ta));
  ta_end(ta);

  return retval;
}

/**Reply to the request message.
 *
 * @note
 * The ownership of @a msg is taken over by the function even if the
 * function fails.
 */
int nta_msg_mreply(nta_agent_t *agent,
		   msg_t *reply, sip_t *sip,
		   int status, char const *phrase,
		   msg_t *req_msg,
		   tag_type_t tag, tag_value_t value, ...)
{
  int retval = -1;
  ta_list ta;

  ta_start(ta, tag, value);

  retval = mreply(agent, reply, status, phrase, req_msg,
		  NULL, 0, 0, NULL,
		  ta_tags(ta));
  ta_end(ta);

  return retval;
}

static
int mreply(nta_agent_t *agent,
	   msg_t *reply,
	   int status, char const *phrase,
	   msg_t *req_msg,
	   tport_t *tport,
	   int incomplete,
	   int sdwn_after,
	   char const *to_tag,
	   tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  sip_t *sip;
  int *use_rport = NULL;
  int retry_without_rport = 0;
  tp_name_t tpn[1];
  int retval = -1;

  if (!agent)
    return -1;

  if (agent->sa_server_rport)
    use_rport = &retry_without_rport, retry_without_rport = 1;

  ta_start(ta, tag, value);

  tl_gets(ta_args(ta), NTATAG_TPORT_REF(tport), TAG_END());

  if (reply == NULL) {
    reply = nta_msg_create(agent, 0);
  }
  sip = sip_object(reply);

  if (!sip) {
    SU_DEBUG_3(("%s: cannot create response msg\n", __func__));
  }
  else if (sip_add_tl(reply, sip, ta_tags(ta)) < 0) {
    SU_DEBUG_3(("%s: cannot add user headers\n", __func__));
  }
  else if (complete_response(reply, status, phrase, req_msg) < 0 &&
	   !incomplete) {
    SU_DEBUG_3(("%s: cannot complete message\n", __func__));
  }
  else if (sip->sip_status && sip->sip_status->st_status > 100 &&
	   sip->sip_to && !sip->sip_to->a_tag &&
	   (to_tag == NONE ? 0 :
	    to_tag != NULL
	    ? sip_to_tag(msg_home(reply), sip->sip_to, to_tag) < 0
	    : sip_to_tag(msg_home(reply), sip->sip_to,
			 nta_agent_newtag(msg_home(reply), "tag=%s", agent)) < 0)) {
    SU_DEBUG_3(("%s: cannot add To tag\n", __func__));
  }
  else if (nta_tpn_by_via(tpn, sip->sip_via, use_rport) < 0) {
    SU_DEBUG_3(("%s: no Via\n", __func__));
  }
  else {
    struct sigcomp_compartment *cc = NONE;

    if (tport == NULL)
      tport = tport_delivered_by(agent->sa_tports, req_msg);

    if (!tport) {
      tport_t *primary = tport_by_protocol(agent->sa_tports, tpn->tpn_proto);

      tport = tport_by_name(primary, tpn);

      if (!tport)
	tport = primary;
    }

    if (retry_without_rport)
      tpn->tpn_port = sip_via_port(sip->sip_via, NULL);

    if (tport && tpn->tpn_comp) {
      tl_gets(ta_args(ta), TPTAG_COMPARTMENT_REF(cc),
	      /* XXX - should also check ntatag_sigcomp_close() */
	      TAG_END());
      if (cc == NONE)
	cc = agent_compression_compartment(agent, tport, tpn, -1);

      if (cc != NULL && cc != NONE &&
	  tport_delivered_with_comp(tport, req_msg, NULL) != -1) {
	agent_accept_compressed(agent, req_msg, cc);
      }
    }

    if (tport_tsend(tport, reply, tpn,
		    IF_SIGCOMP_TPTAG_COMPARTMENT(cc)
		    TPTAG_MTU(INT_MAX),
		    TPTAG_SDWN_AFTER(sdwn_after),
		    ta_tags(ta))) {
      agent->sa_stats->as_sent_msg++;
      agent->sa_stats->as_sent_response++;
      retval = 0;			/* Success! */
    }
    else {
      SU_DEBUG_3(("%s: send fails\n", __func__));
    }
  }

  msg_destroy(reply);
  msg_destroy(req_msg);

  ta_end(ta);

  return retval;
}

/** Add headers from the request to the response message. */
static
int complete_response(msg_t *response,
		      int status, char const *phrase,
		      msg_t *request)
{
  su_home_t *home = msg_home(response);
  sip_t *response_sip = sip_object(response);
  sip_t const *request_sip = sip_object(request);

  int incomplete = 0;

  if (!response_sip || !request_sip || !request_sip->sip_request)
    return -1;

  if (!response_sip->sip_status)
    response_sip->sip_status = sip_status_create(home, status, phrase, NULL);
  if (!response_sip->sip_via)
    response_sip->sip_via = sip_via_dup(home, request_sip->sip_via);
  if (!response_sip->sip_from)
    response_sip->sip_from = sip_from_dup(home, request_sip->sip_from);
  if (!response_sip->sip_to)
    response_sip->sip_to = sip_to_dup(home, request_sip->sip_to);
  if (!response_sip->sip_call_id)
    response_sip->sip_call_id =
      sip_call_id_dup(home, request_sip->sip_call_id);
  if (!response_sip->sip_cseq)
    response_sip->sip_cseq = sip_cseq_dup(home, request_sip->sip_cseq);

  if (!response_sip->sip_record_route && request_sip->sip_record_route)
    sip_add_dup(response, response_sip, (void*)request_sip->sip_record_route);

  incomplete = sip_complete_message(response) < 0;

  msg_serialize(response, (msg_pub_t *)response_sip);

  if (incomplete ||
      !response_sip->sip_status ||
      !response_sip->sip_via ||
      !response_sip->sip_from ||
      !response_sip->sip_to ||
      !response_sip->sip_call_id ||
      !response_sip->sip_cseq ||
      !response_sip->sip_content_length ||
      !response_sip->sip_separator ||
      (request_sip->sip_record_route && !response_sip->sip_record_route))
    return -1;

  return 0;
}

/** ACK and BYE an unknown 200 OK response to INVITE.
 *
 * A UAS may still return a 2XX series response to client request after the
 * client transactions has been terminated. In that case, the UAC can not
 * really accept the call. This function was used to accept and immediately
 * terminate such a call.
 *
 * @deprecated This was a bad idea: see sf.net bug #1750691. It can be used
 * to amplify DoS attacks. Let UAS take care of retransmission timeout and
 * let it terminate the session. As of @VERSION_1_12_7, this function just
 * returns -1.
 */
int nta_msg_ackbye(nta_agent_t *agent, msg_t *msg)
{
  sip_t *sip = sip_object(msg);
  msg_t *amsg = nta_msg_create(agent, 0);
  sip_t *asip = sip_object(amsg);
  msg_t *bmsg = NULL;
  sip_t *bsip;
  url_string_t const *ruri;
  nta_outgoing_t *ack = NULL, *bye = NULL;
  sip_cseq_t *cseq;
  sip_request_t *rq;
  sip_route_t *route = NULL, *r, r0[1];
  su_home_t *home = msg_home(amsg);

  if (asip == NULL)
    return -1;

  sip_add_tl(amsg, asip,
	     SIPTAG_TO(sip->sip_to),
	     SIPTAG_FROM(sip->sip_from),
	     SIPTAG_CALL_ID(sip->sip_call_id),
	     TAG_END());

  if (sip->sip_contact) {
    ruri = (url_string_t const *)sip->sip_contact->m_url;
  } else {
    ruri = (url_string_t const *)sip->sip_to->a_url;
  }

  /* Reverse (and fix) record route */
  route = sip_route_reverse(home, sip->sip_record_route);

  if (route && !url_has_param(route->r_url, "lr")) {
    for (r = route; r->r_next; r = r->r_next)
      ;

    /* Append r-uri */
    *sip_route_init(r0)->r_url = *ruri->us_url;
    r->r_next = sip_route_dup(home, r0);

    /* Use topmost route as request-uri */
    ruri = (url_string_t const *)route->r_url;
    route = route->r_next;
  }

  msg_header_insert(amsg, (msg_pub_t *)asip, (msg_header_t *)route);

  bmsg = msg_copy(amsg); bsip = sip_object(bmsg);

  if (!(cseq = sip_cseq_create(home, sip->sip_cseq->cs_seq, SIP_METHOD_ACK)))
    goto err;
  else
    msg_header_insert(amsg, (msg_pub_t *)asip, (msg_header_t *)cseq);

  if (!(rq = sip_request_create(home, SIP_METHOD_ACK, ruri, NULL)))
    goto err;
  else
    msg_header_insert(amsg, (msg_pub_t *)asip, (msg_header_t *)rq);

  if (!(ack = nta_outgoing_mcreate(agent, NULL, NULL, NULL, amsg,
				   NTATAG_ACK_BRANCH(sip->sip_via->v_branch),
				   NTATAG_STATELESS(1),
				   TAG_END())))
    goto err;
  else
    nta_outgoing_destroy(ack);

  home = msg_home(bmsg);

  if (!(cseq = sip_cseq_create(home, 0x7fffffff, SIP_METHOD_BYE)))
    goto err;
  else
    msg_header_insert(bmsg, (msg_pub_t *)bsip, (msg_header_t *)cseq);

  if (!(rq = sip_request_create(home, SIP_METHOD_BYE, ruri, NULL)))
    goto err;
  else
    msg_header_insert(bmsg, (msg_pub_t *)bsip, (msg_header_t *)rq);

  if (!(bye = nta_outgoing_mcreate(agent, NULL, NULL, NULL, bmsg,
				   NTATAG_STATELESS(1),
				   TAG_END())))
    goto err;

  msg_destroy(msg);
  return 0;

 err:
  msg_destroy(amsg);
  msg_destroy(bmsg);
  return -1;
}

/**Complete a request with values from dialog.
 *
 * Complete a request message @a msg belonging to a dialog associated with
 * @a leg. It increments the local @CSeq value, adds @CallID, @To, @From and
 * @Route headers (if there is such headers present in @a leg), and creates
 * a new request line object from @a method, @a method_name and @a
 * request_uri.
 *
 * @param msg          pointer to a request message object
 * @param leg          pointer to a #nta_leg_t object
 * @param method       request method number or #sip_method_unknown
 * @param method_name  method name (if @a method == #sip_method_unknown)
 * @param request_uri  request URI
 *
 * If @a request_uri contains query part, the query part is converted as SIP
 * headers and added to the request.
 *
 * @retval 0  when successful
 * @retval -1 upon an error
 *
 * @sa nta_outgoing_mcreate(), nta_outgoing_tcreate()
 */
int nta_msg_request_complete(msg_t *msg,
			     nta_leg_t *leg,
			     sip_method_t method,
			     char const *method_name,
			     url_string_t const *request_uri)
{
  su_home_t *home = msg_home(msg);
  sip_t *sip = sip_object(msg);
  sip_to_t const *to;
  uint32_t seq;
  url_t reg_url[1];
  url_string_t const *original = request_uri;

  if (!leg || !msg || !sip)
    return -1;

  if (!sip->sip_route && leg->leg_route) {
    if (leg->leg_loose_route) {
      if (leg->leg_target) {
	request_uri = (url_string_t *)leg->leg_target->m_url;
      }
      sip->sip_route = sip_route_dup(home, leg->leg_route);
    }
    else {
      sip_route_t **rr;

      request_uri = (url_string_t *)leg->leg_route->r_url;
      sip->sip_route = sip_route_dup(home, leg->leg_route->r_next);

      for (rr = &sip->sip_route; *rr; rr = &(*rr)->r_next)
	;

      if (leg->leg_target)
	*rr = sip_route_dup(home, (sip_route_t *)leg->leg_target);
    }
  }
  else if (leg->leg_target)
    request_uri = (url_string_t *)leg->leg_target->m_url;

  if (!request_uri && sip->sip_request)
    request_uri = (url_string_t *)sip->sip_request->rq_url;

  to = sip->sip_to ? sip->sip_to : leg->leg_remote;

  if (!request_uri && to) {
    if (method != sip_method_register)
      request_uri = (url_string_t *)to->a_url;
    else {
      /* Remove user part from REGISTER requests */
      *reg_url = *to->a_url;
      reg_url->url_user = reg_url->url_password = NULL;
      request_uri = (url_string_t *)reg_url;
    }
  }

  if (!request_uri)
    return -1;

  if (method || method_name) {
    sip_request_t *rq = sip->sip_request;
    int use_headers =
      request_uri == original || (url_t *)request_uri == rq->rq_url;

    if (!rq
	|| request_uri != (url_string_t *)rq->rq_url
	|| method != rq->rq_method
	|| !su_strmatch(method_name, rq->rq_method_name))
      rq = NULL;

    if (rq == NULL) {
      rq = sip_request_create(home, method, method_name, request_uri, NULL);
      if (msg_header_insert(msg, (msg_pub_t *)sip, (msg_header_t *)rq) < 0)
	return -1;
    }

    /* @RFC3261 table 1 (page 152):
     * Req-URI cannot contain method parameter or headers
     */
    if (rq->rq_url->url_params) {
      rq->rq_url->url_params =
	url_strip_param_string((char *)rq->rq_url->url_params, "method");
      sip_fragment_clear(rq->rq_common);
    }

    if (rq->rq_url->url_headers) {
      if (use_headers) {
	char *s = url_query_as_header_string(msg_home(msg),
					     rq->rq_url->url_headers);
	if (!s)
	  return -1;
	msg_header_parse_str(msg, (msg_pub_t*)sip, s);
      }
      rq->rq_url->url_headers = NULL, sip_fragment_clear(rq->rq_common);
    }
  }

  if (!sip->sip_request)
    return -1;

  if (!sip->sip_max_forwards)
    sip_add_dup(msg, sip, (sip_header_t *)leg->leg_agent->sa_max_forwards);

  if (!sip->sip_from)
    sip->sip_from = sip_from_dup(home, leg->leg_local);
  else if (leg->leg_local && leg->leg_local->a_tag &&
	   (!sip->sip_from->a_tag ||
	    !su_casematch(sip->sip_from->a_tag, leg->leg_local->a_tag)))
    sip_from_tag(home, sip->sip_from, leg->leg_local->a_tag);

  if (sip->sip_from && !sip->sip_from->a_tag) {
    sip_fragment_clear(sip->sip_from->a_common);
    sip_from_add_param(home, sip->sip_from,
		       nta_agent_newtag(home, "tag=%s", leg->leg_agent));
  }

  if (sip->sip_to) {
    if (leg->leg_remote && leg->leg_remote->a_tag)
      sip_to_tag(home, sip->sip_to, leg->leg_remote->a_tag);
  }
  else if (leg->leg_remote) {
    sip->sip_to = sip_to_dup(home, leg->leg_remote);
  }
  else {
    sip_to_t *to = sip_to_create(home, request_uri);
    if (to) sip_aor_strip(to->a_url);
    sip->sip_to = to;
  }

  if (!sip->sip_from || !sip->sip_from || !sip->sip_to)
    return -1;

  method = sip->sip_request->rq_method;
  method_name = sip->sip_request->rq_method_name;

  if (!leg->leg_id && sip->sip_cseq)
    seq = sip->sip_cseq->cs_seq;
  else if (method == sip_method_ack || method == sip_method_cancel)
    /* Dangerous - we may do PRACK/UPDATE meanwhile */
    seq = sip->sip_cseq ? sip->sip_cseq->cs_seq : leg->leg_seq;
  else if (leg->leg_seq)
    seq = ++leg->leg_seq;
  else if (sip->sip_cseq) /* Obtain initial value from existing CSeq header */
    seq = leg->leg_seq = sip->sip_cseq->cs_seq;
  else
    seq = leg->leg_seq = (sip_now() >> 1) & 0x7ffffff;

  if (!sip->sip_call_id) {
    if (leg->leg_id)
      sip->sip_call_id = sip_call_id_dup(home, leg->leg_id);
    else
      sip->sip_call_id = sip_call_id_create(home, NULL);
  }

  if (!sip->sip_cseq ||
      seq != sip->sip_cseq->cs_seq ||
      method != sip->sip_cseq->cs_method ||
      !su_strmatch(method_name, sip->sip_cseq->cs_method_name)) {
    sip_cseq_t *cseq = sip_cseq_create(home, seq, method, method_name);
    if (msg_header_insert(msg, (msg_pub_t *)sip, (msg_header_t *)cseq) < 0)
      return -1;
  }

  return 0;
}

/* ====================================================================== */
/* 6) Dialogs (legs) */

static void leg_insert(nta_agent_t *agent, nta_leg_t *leg);
static int leg_route(nta_leg_t *leg,
		     sip_record_route_t const *route,
		     sip_record_route_t const *reverse,
		     sip_contact_t const *contact,
		     int reroute);
static int leg_callback_default(nta_leg_magic_t*, nta_leg_t*,
				nta_incoming_t*, sip_t const *);
#define HTABLE_HASH_LEG(leg) ((leg)->leg_hash)
HTABLE_BODIES_WITH(leg_htable, lht, nta_leg_t, HTABLE_HASH_LEG, size_t, hash_value_t);
su_inline
hash_value_t hash_istring(char const *, char const *, hash_value_t);

/**@typedef nta_request_f
 *
 * Callback for incoming requests
 *
 * This is a callback function invoked by NTA for each incoming SIP request.
 *
 * @param lmagic call leg context
 * @param leg    call leg handle
 * @param ireq   incoming request
 * @param sip    incoming request contents
 *
 * @retval 100..699
 * NTA constructs a reply message with given error code and corresponding
 * standard phrase, then sends the reply.
 *
 * @retval 0
 * The application takes care of sending (or not sending) the reply.
 *
 * @retval other
 * All other return values will be interpreted as
 * @e 500 @e Internal @e server @e error.
 */


/**
 * Create a new leg object.
 *
 * Creates a leg object, which is used to represent dialogs, partial dialogs
 * (for example, in case of REGISTER), and destinations within a particular
 * NTA object.
 *
 * When a leg is created, a callback pointer and a application context is
 * provided. All other parameters are optional.
 *
 * @param agent    agent object
 * @param callback function which is called for each
 *                 incoming request belonging to this leg
 * @param magic    call leg context
 * @param tag,value,... optional extra headers in taglist
 *
 * When a leg representing dialog is created, the tags SIPTAG_CALL_ID(),
 * SIPTAG_FROM(), SIPTAG_TO(), and SIPTAG_CSEQ() (for local @CSeq number) are used
 * to establish dialog context. The SIPTAG_FROM() is used to pass local
 * address (@From header when making a call, @To header when answering
 * to a call) to the newly created leg. Respectively, the SIPTAG_TO() is
 * used to pass remote address (@To header when making a call, @From
 * header when answering to a call).
 *
 * If there is a (preloaded) route associated with the leg, SIPTAG_ROUTE()
 * and NTATAG_TARGET() can be used. A client or server can also set the
 * route using @RecordRoute and @Contact headers from a response or
 * request message with the functions nta_leg_client_route() and
 * nta_leg_server_route(), respectively.
 *
 * When a leg representing a local destination is created, the tags
 * NTATAG_NO_DIALOG(1), NTATAG_METHOD(), and URLTAG_URL() are used. When a
 * request with matching request-URI (URLTAG_URL()) and method
 * (NTATAG_METHOD()) is received, it is passed to the callback function
 * provided with the leg.
 *
 * @sa nta_leg_stateful(), nta_leg_bind(),
 *     nta_leg_tag(), nta_leg_rtag(),
 *     nta_leg_client_route(), nta_leg_server_route(),
 *     nta_leg_destroy(), nta_outgoing_tcreate(), and nta_request_f().
 *
 * @TAGS
 * NTATAG_NO_DIALOG(), NTATAG_STATELESS(), NTATAG_METHOD(),
 * URLTAG_URL(), SIPTAG_CALL_ID(), SIPTAG_CALL_ID_STR(), SIPTAG_FROM(),
 * SIPTAG_FROM_STR(), SIPTAG_TO(), SIPTAG_TO_STR(), SIPTAG_ROUTE(),
 * NTATAG_TARGET() and SIPTAG_CSEQ().
 *
 */
nta_leg_t *nta_leg_tcreate(nta_agent_t *agent,
			   nta_request_f *callback,
			   nta_leg_magic_t *magic,
			   tag_type_t tag, tag_value_t value, ...)
{
  sip_route_t const *route = NULL;
  sip_contact_t const *contact = NULL;
  sip_cseq_t const *cs = NULL;
  sip_call_id_t const *i = NULL;
  sip_from_t const *from = NULL;
  sip_to_t const *to = NULL;
  char const *method = NULL;
  char const *i_str = NULL, *to_str = NULL, *from_str = NULL, *cs_str = NULL;
  url_string_t const *url_string = NULL;
  int no_dialog = 0;
  unsigned rseq = 0;
  /* RFC 3261 section 12.2.1.1 */
  uint32_t seq = 0;
  ta_list ta;
  nta_leg_t *leg;
  su_home_t *home;
  url_t *url;
  char const *what = NULL;

  if (agent == NULL)
    return su_seterrno(EINVAL), NULL;

  ta_start(ta, tag, value);

  tl_gets(ta_args(ta),
	  NTATAG_NO_DIALOG_REF(no_dialog),
	  NTATAG_METHOD_REF(method),
	  URLTAG_URL_REF(url_string),
	  SIPTAG_CALL_ID_REF(i),
	  SIPTAG_CALL_ID_STR_REF(i_str),
	  SIPTAG_FROM_REF(from),
	  SIPTAG_FROM_STR_REF(from_str),
	  SIPTAG_TO_REF(to),
	  SIPTAG_TO_STR_REF(to_str),
	  SIPTAG_ROUTE_REF(route),
	  NTATAG_TARGET_REF(contact),
	  NTATAG_REMOTE_CSEQ_REF(rseq),
	  SIPTAG_CSEQ_REF(cs),
	  SIPTAG_CSEQ_STR_REF(cs_str),
	  TAG_END());

  ta_end(ta);

  if (cs)
    seq = cs->cs_seq;
  else if (cs_str)
    seq = strtoul(cs_str, (char **)&cs_str, 10);

  if (i == NONE) /* Magic value, used for compatibility */
    no_dialog = 1;

  if (!(leg = su_home_clone(NULL, sizeof(*leg))))
    return NULL;
  home = leg->leg_home;

  leg->leg_agent = agent;
  nta_leg_bind(leg, callback, magic);

  if (from) {
    /* Now this is kludge */
    leg->leg_local_is_to = sip_is_to((sip_header_t*)from);
    leg->leg_local = sip_to_dup(home, from);
  }
  else if (from_str)
    leg->leg_local = sip_to_make(home, from_str);

  if (to && no_dialog) {
    /* Remove tag, if any */
    sip_to_t to0[1]; *to0 = *to; to0->a_params = NULL;
    leg->leg_remote = sip_from_dup(home, to0);
  }
  else if (to)
    leg->leg_remote = sip_from_dup(home, to);
  else if (to_str)
    leg->leg_remote = sip_from_make(home, to_str);

  if (route && route != NONE)
    leg->leg_route = sip_route_dup(home, route), leg->leg_route_set = 1;

  if (contact && contact != NONE) {
    sip_contact_t m[1];
    sip_contact_init(m);
    *m->m_url = *contact->m_url;
    m->m_url->url_headers = NULL;
    leg->leg_target = sip_contact_dup(home, m);
  }

  url = url_hdup(home, url_string->us_url);

  /* Match to local hosts */
  if (url && agent_aliases(agent, url, NULL)) {
    url_t *changed = url_hdup(home, url);
    su_free(home, url);
    url = changed;
  }

  leg->leg_rseq = rseq;
  leg->leg_seq = seq;
  leg->leg_url = url;

  if (from && from != NONE && leg->leg_local == NULL) {
    what = "cannot duplicate local address";
    goto err;
  }
  else if (to && to != NONE && leg->leg_remote == NULL) {
    what = "cannot duplicate remote address";
    goto err;
  }
  else if (route && route != NONE && leg->leg_route == NULL) {
    what = "cannot duplicate route";
    goto err;
  }
  else if (contact && contact != NONE && leg->leg_target == NULL) {
    what = "cannot duplicate target";
    goto err;
  }
  else if (url_string && leg->leg_url == NULL) {
    what = "cannot duplicate local destination";
    goto err;
  }

  if (!no_dialog) {
    if (!leg->leg_local || !leg->leg_remote) {
      /* To and/or From header missing */
      if (leg->leg_remote)
	what = "Missing local dialog address";
      else if (leg->leg_local)
	what = "Missing remote dialog address";
      else
	what = "Missing dialog addresses";
      goto err;
    }

    leg->leg_dialog = 1;

    if (i != NULL)
      leg->leg_id = sip_call_id_dup(home, i);
    else if (i_str != NULL)
      leg->leg_id = sip_call_id_make(home, i_str);
    else
      leg->leg_id = sip_call_id_create(home, NULL);

    if (!leg->leg_id) {
      what = "cannot create Call-ID";
      goto err;
    }

    leg->leg_hash = leg->leg_id->i_hash;
  }
  else if (url) {
    /* This is "default leg" with a destination URL. */
    hash_value_t hash = 0;

    if (method) {
      leg->leg_method = su_strdup(home, method);
    }
#if 0
    else if (url->url_params) {
      int len = url_param(url->url_params, "method", NULL, 0);
      if (len) {
	char *tmp = su_alloc(home, len);
	leg->leg_method = tmp;
	url_param(url->url_params, "method", tmp, len);
      }
    }
#endif

    if (url->url_user && strcmp(url->url_user, "") == 0)
      url->url_user = "%";	/* Match to any user */

    hash = hash_istring(url->url_scheme, ":", 0);
    hash = hash_istring(url->url_host, "", hash);
    hash = hash_istring(url->url_user, "@", hash);

    leg->leg_hash = hash;
  }
  else {
    /* This is "default leg" without a destination URL. */
    if (agent->sa_default_leg) {
      SU_DEBUG_1(("%s(): %s\n", "nta_leg_tcreate", "tried to create second default leg"));
      su_seterrno(EEXIST);
      goto err;
    }
    else {
      agent->sa_default_leg = leg;
    }
    return leg;
  }

  if (url) {
    /* Parameters are ignored when comparing incoming URLs */
    url->url_params = NULL;
  }

  leg_insert(agent, leg);

  SU_DEBUG_9(("%s(%p)\n", "nta_leg_tcreate", (void *)leg));

  return leg;

 err:
  if (what)
    SU_DEBUG_9(("%s(): %s\n", "nta_leg_tcreate", what));

  su_home_zap(leg->leg_home);

  return NULL;
}

/** Return the default leg, if any */
nta_leg_t *nta_default_leg(nta_agent_t const *agent)
{
  return agent ? agent->sa_default_leg : NULL;
}


/**
 * Insert a call leg to agent.
 */
static
void leg_insert(nta_agent_t *sa, nta_leg_t *leg)
{
  leg_htable_t *leg_hash;
  assert(leg);
  assert(sa);

  if (leg->leg_dialog)
    leg_hash = sa->sa_dialogs;
  else
    leg_hash = sa->sa_defaults;

  if (leg_htable_is_full(leg_hash)) {
    leg_htable_resize(sa->sa_home, leg_hash, 0);
    assert(leg_hash->lht_table);
    SU_DEBUG_7(("nta: resized%s leg hash to "MOD_ZU"\n",
		leg->leg_dialog ? "" : " default", leg_hash->lht_size));
  }

  /* Insert entry into hash table (before other legs with same hash) */
  leg_htable_insert(leg_hash, leg);
}

/**
 * Destroy a leg.
 *
 * @param leg leg to be destroyed
 */
void nta_leg_destroy(nta_leg_t *leg)
{
  SU_DEBUG_9(("nta_leg_destroy(%p)\n", (void *)leg));

  if (leg) {
    leg_htable_t *leg_hash;
    nta_agent_t *sa = leg->leg_agent;

    assert(sa);

    if (leg->leg_dialog) {
      assert(sa->sa_dialogs);
      leg_hash = sa->sa_dialogs;
    }
    else if (leg != sa->sa_default_leg) {
      assert(sa->sa_defaults);
      leg_hash = sa->sa_defaults;
    }
    else {
      sa->sa_default_leg = NULL;
      leg_hash = NULL;
    }

    if (leg_hash)
      leg_htable_remove(leg_hash, leg);

    leg_free(sa, leg);
  }
}

static
void leg_free(nta_agent_t *sa, nta_leg_t *leg)
{
	//su_free(sa->sa_home, leg);
	su_home_unref((su_home_t *)leg);
}

/** Return application context for the leg */
nta_leg_magic_t *nta_leg_magic(nta_leg_t const *leg,
			       nta_request_f *callback)
{
  if (leg)
    if (!callback || leg->leg_callback == callback)
      return leg->leg_magic;

  return NULL;
}

/**Bind a callback function and context to a leg object.
 *
 * Change the callback function and context pointer attached to a leg
 * object.
 *
 * @param leg      leg object to be bound
 * @param callback new callback function (or NULL if no callback is desired)
 * @param magic    new context pointer
 */
void nta_leg_bind(nta_leg_t *leg,
		  nta_request_f *callback,
		  nta_leg_magic_t *magic)
{
  if (leg) {
    if (callback)
      leg->leg_callback = callback;
    else
      leg->leg_callback = leg_callback_default;
    leg->leg_magic = magic;
  }
}

/** Add a local tag to the leg.
 *
 * @param leg leg to be tagged
 * @param tag tag to be added (if NULL, a tag generated by @b NTA is added)
 *
 * @return
 * Pointer to tag if successful, NULL otherwise.
 */
char const *nta_leg_tag(nta_leg_t *leg, char const *tag)
{
  if (!leg || !leg->leg_local)
    return su_seterrno(EINVAL), NULL;

  if (tag && strchr(tag, '='))
    tag = strchr(tag, '=') + 1;

  /* If there already is a tag,
     return NULL if it does not match with new one */
  if (leg->leg_local->a_tag) {
    if (tag == NULL || su_casematch(tag, leg->leg_local->a_tag))
      return leg->leg_local->a_tag;
    else
      return NULL;
  }

  if (tag) {
    if (sip_to_tag(leg->leg_home, leg->leg_local, tag) < 0)
      return NULL;
    leg->leg_tagged = 1;
    return leg->leg_local->a_tag;
  }

  tag = nta_agent_newtag(leg->leg_home, "tag=%s", leg->leg_agent);

  if (!tag || sip_to_add_param(leg->leg_home, leg->leg_local, tag) < 0)
    return NULL;

  leg->leg_tagged = 1;

  return leg->leg_local->a_tag;
}

/** Get local tag. */
char const *nta_leg_get_tag(nta_leg_t const *leg)
{
  if (leg && leg->leg_local)
    return leg->leg_local->a_tag;
  else
    return NULL;
}

/** Add a remote tag to the leg.
 *
 * @note No remote tag is ever generated.
 *
 * @param leg leg to be tagged
 * @param tag tag to be added (@b must be non-NULL)
 *
 * @return
 * Pointer to tag if successful, NULL otherwise.
 */
char const *nta_leg_rtag(nta_leg_t *leg, char const *tag)
{
  /* Add a tag parameter, unless there already is a tag */
  if (leg && leg->leg_remote && tag) {
    if (sip_from_tag(leg->leg_home, leg->leg_remote, tag) < 0)
      return NULL;
  }

  if (leg && leg->leg_remote)
    return leg->leg_remote->a_tag;
  else
    return NULL;
}

/** Get remote tag. */
char const *nta_leg_get_rtag(nta_leg_t const *leg)
{
  if (leg && leg->leg_remote)
    return leg->leg_remote->a_tag;
  else
    return NULL;
}

/** Get local request sequence number. */
uint32_t nta_leg_get_seq(nta_leg_t const *leg)
{
  return leg ? leg->leg_seq : 0;
}

/** Get remote request sequence number. */
uint32_t nta_leg_get_rseq(nta_leg_t const *leg)
{
  return leg ? leg->leg_rseq : 0;
}

/** Save target and route set at UAC side.
 *
 * @sa nta_leg_client_reroute(), nta_leg_server_route(), @RFC3261 section 12.1.2
 *
 * @bug Allows modifying the route set after initial transaction, if initial
 * transaction had no @RecordRoute headers.
 *
 * @deprecated Use nta_leg_client_reroute() instead.
 */
int nta_leg_client_route(nta_leg_t *leg,
			 sip_record_route_t const *route,
			 sip_contact_t const *contact)
{
  return leg_route(leg, NULL, route, contact, 0);
}

/** Save target and route set at UAC side.
 *
 * If @a initial is true, the route set is modified even if it has been set
 * earlier.
 *
 * @param leg pointer to dialog leg
 * @param route @RecordRoute headers from response
 * @param contact @Contact header from response
 * @param initial true if response to initial transaction
 *
 * @sa nta_leg_client_route(), nta_leg_server_route(), @RFC3261 section 12.1.2
 *
 * @NEW_1_12_11
 */
int nta_leg_client_reroute(nta_leg_t *leg,
			   sip_record_route_t const *route,
			   sip_contact_t const *contact,
			   int initial)
{
  return leg_route(leg, NULL, route, contact, initial ? 2 : 1);
}

/** Save target and route set at UAS side.
 *
 * @param leg pointer to dialog leg
 * @param route @RecordRoute headers from request
 * @param contact @Contact header from request
 *
 * @sa nta_leg_client_reroute(), @RFC3261 section 12.1.1
 */
int nta_leg_server_route(nta_leg_t *leg,
			 sip_record_route_t const *route,
			 sip_contact_t const *contact)
{
  return leg_route(leg, route, NULL, contact, 1);
}

/** Return route components. */
int nta_leg_get_route(nta_leg_t *leg,
		      sip_route_t const **return_route,
		      sip_contact_t const **return_target)
{
  if (!leg)
    return -1;

  if (return_route)
    *return_route = leg->leg_route;

  if (return_target)
    *return_target = leg->leg_target;

  return 0;
}

/** Generate @Replaces header.
 *
 * @since New in @VERSION_1_12_2.
 */
sip_replaces_t *
nta_leg_make_replaces(nta_leg_t *leg,
		      su_home_t *home,
		      int early_only)
{
  char const *from_tag, *to_tag;

  if (!leg)
    return NULL;
  if (!leg->leg_dialog || !leg->leg_local || !leg->leg_remote || !leg->leg_id)
    return NULL;

  from_tag = leg->leg_local->a_tag; if (!from_tag) from_tag = "0";
  to_tag = leg->leg_remote->a_tag; if (!to_tag) to_tag = "0";

  return sip_replaces_format(home, "%s;from-tag=%s;to-tag=%s%s",
			     leg->leg_id->i_id, from_tag, to_tag,
			     early_only ? ";early-only" : "");
}

/** Get dialog leg by @Replaces header.
 *
 * @since New in @VERSION_1_12_2.
 */
nta_leg_t *
nta_leg_by_replaces(nta_agent_t *sa, sip_replaces_t const *rp)
{
  nta_leg_t *leg = NULL;

  if (sa && rp && rp->rp_call_id && rp->rp_from_tag && rp->rp_to_tag) {
    char const *from_tag = rp->rp_from_tag, *to_tag = rp->rp_to_tag;
    sip_call_id_t id[1];
    sip_call_id_init(id);

    id->i_hash = msg_hash_string(id->i_id = rp->rp_call_id);

    leg = leg_find(sa, NULL, NULL, id, from_tag, to_tag);

    if (leg == NULL && strcmp(from_tag, "0") == 0)
      leg = leg_find(sa, NULL, NULL, id, NULL, to_tag);
    if (leg == NULL && strcmp(to_tag, "0") == 0)
      leg = leg_find(sa, NULL, NULL, id, from_tag, NULL);
  }

  return leg;
}

/**@internal
 * Find a leg corresponding to the request message.
 *
 */
static nta_leg_t *
leg_find_call_id(nta_agent_t const *sa,
		 sip_call_id_t const *i)
{
  hash_value_t hash = i->i_hash;
  leg_htable_t const *lht = sa->sa_dialogs;
  nta_leg_t **ll, *leg = NULL;

  for (ll = leg_htable_hash(lht, hash);
       (leg = *ll);
       ll = leg_htable_next(lht, ll)) {
    sip_call_id_t const *leg_i = leg->leg_id;

	if (leg->leg_hash != hash)
      continue;
    if (strcmp(leg_i->i_id, i->i_id) != 0)
      continue;

	return leg;
  }

  return leg;
}

/** Get dialog leg by @CallID.
 *
 * @note Usually there should be only single dialog per @CallID on
 * User-Agents. However, proxies may fork requests initiating the dialog and
 * result in multiple calls per @CallID.
 *
 * @since New in @VERSION_1_12_9.
 */
nta_leg_t *
nta_leg_by_call_id(nta_agent_t *sa, const char *call_id)
{
  nta_leg_t *leg = NULL;

  if (call_id) {
    sip_call_id_t id[1];
    sip_call_id_init(id);

    id->i_hash = msg_hash_string(id->i_id = call_id);

    leg = leg_find_call_id(sa, id);
  }

  return leg;
}

/** Calculate a simple case-insensitive hash over a string */
su_inline
hash_value_t hash_istring(char const *s, char const *term, hash_value_t hash)
{
  if (s) {
    for (; *s; s++) {
      unsigned char c = *s;
      if ('A' <= c && c <= 'Z')
	c += 'a' - 'A';
      hash = 38501U * (hash + c);
    }
    for (s = term; *s; s++) {
      unsigned char c = *s;
      hash = 38501U * (hash + c);
    }
  }

  return hash;
}

/** @internal Handle requests intended for this leg. */
static
void leg_recv(nta_leg_t *leg, msg_t *msg, sip_t *sip, tport_t *tport)
{
  nta_agent_t *agent = leg->leg_agent;
  nta_incoming_t *irq;
  sip_method_t method = sip->sip_request->rq_method;
  char const *method_name = sip->sip_request->rq_method_name;
  char const *tag = NULL;
  int status;

  if (leg->leg_local)
    tag = leg->leg_local->a_tag;

  if (leg->leg_dialog)
    agent->sa_stats->as_dialog_tr++;

  /* RFC-3262 section 3 (page 4) */
  if (agent->sa_is_a_uas && method == sip_method_prack) {
    mreply(agent, NULL, 481, "No such response", msg,
	   tport, 0, 0, NULL,
	   TAG_END());
    return;
  }

  if (!(irq = incoming_create(agent, msg, sip, tport, tag))) {
    SU_DEBUG_3(("nta: leg_recv(%p): cannot create transaction for %s\n",
		(void *)leg, method_name));
    mreply(agent, NULL, SIP_500_INTERNAL_SERVER_ERROR, msg,
	   tport, 0, 0, NULL,
	   TAG_END());
    return;
  }

  irq->irq_in_callback = 1;
  status = incoming_callback(leg, irq, sip);
  irq->irq_in_callback = 0;

  if (irq->irq_destroyed) {
    if (irq->irq_terminated) {
      incoming_free(irq);
      return;
    }
    if (status < 200)
      status = 500;
  }

  if (status == 0)
    return;

  if (status < 100 || status > 699) {
    SU_DEBUG_3(("nta_leg(%p): invalid status %03d from callback\n",
		(void *)leg, status));
    status = 500;
  }
  else if (method == sip_method_invite && status >= 200 && status < 300) {
    SU_DEBUG_3(("nta_leg(%p): invalid INVITE status %03d from callback\n",
		(void *)leg, status));
    status = 500;
  }

  if (status >= 100 && irq->irq_status < 200)
    nta_incoming_treply(irq, status, NULL, TAG_END());

  if (status >= 200)
    nta_incoming_destroy(irq);
}

/**Compare two SIP from/to fields.
 *
 * @retval nonzero if matching.
 * @retval zero if not matching.
 */
su_inline
int addr_cmp(url_t const *a, url_t const *b)
{
  if (b == NULL)
    return 0;
  else
    return
      host_cmp(a->url_host, b->url_host) ||
      su_strcmp(a->url_port, b->url_port) ||
      su_strcmp(a->url_user, b->url_user);
}

/** Get a leg by dialog.
 *
 * Search for a dialog leg from agent's hash table. The matching rules based
 * on parameters are as follows:
 *
 * @param agent        pointer to agent object
 * @param request_uri  if non-NULL, and there is destination URI
 *                     associated with the dialog, these URIs must match
 * @param call_id      if non-NULL, must match with @CallID header contents
 * @param remote_tag   if there is remote tag
 *                     associated with dialog, @a remote_tag must match
 * @param remote_uri   ignored
 * @param local_tag    if non-NULL and there is local tag associated with leg,
 *                     it must math
 * @param local_uri    ignored
 *
 * @note
 * If @a remote_tag or @a local_tag is an empty string (""), the tag is
 * ignored when matching legs.
 */
nta_leg_t *nta_leg_by_dialog(nta_agent_t const *agent,
			     url_t const *request_uri,
			     sip_call_id_t const *call_id,
			     char const *remote_tag,
			     url_t const *remote_uri,
			     char const *local_tag,
			     url_t const *local_uri)
{
  void *to_be_freed = NULL;
  url_t *url;
  url_t url0[1];
  nta_leg_t *leg;

  if (!agent || !call_id)
    return su_seterrno(EINVAL), NULL;

  if (request_uri == NULL) {
    url = NULL;
  }
  else if (URL_IS_STRING(request_uri)) {
    /* accept a string as URL */
    to_be_freed = url = url_hdup(NULL, request_uri);
  }
  else {
    *url0 = *request_uri, url = url0;
  }

  if (url) {
    url->url_params = NULL;
    agent_aliases(agent, url, NULL); /* canonize url */
  }

  if (remote_tag && remote_tag[0] == '\0')
    remote_tag = NULL;
  if (local_tag && local_tag[0] == '\0')
    local_tag = NULL;

  leg = leg_find(agent,
		 NULL, url,
		 call_id,
		 remote_tag,
		 local_tag);

  if (to_be_freed) su_free(NULL, to_be_freed);

  return leg;
}

/**@internal
 * Find a leg corresponding to the request message.
 *
 * A leg matches to message if leg_match_request() returns true ("Call-ID",
 * "To"-tag, and "From"-tag match).
 */
static
nta_leg_t *leg_find(nta_agent_t const *sa,
		    char const *method_name,
		    url_t const *request_uri,
		    sip_call_id_t const *i,
		    char const *from_tag,
		    char const *to_tag)
{
  hash_value_t hash = i->i_hash;
  leg_htable_t const *lht = sa->sa_dialogs;
  nta_leg_t  **ll, *leg, *loose_match = NULL;

  for (ll = leg_htable_hash(lht, hash);
       (leg = *ll);
       ll = leg_htable_next(lht, ll)) {
    sip_call_id_t const *leg_i = leg->leg_id;
    char const *remote_tag = leg->leg_remote->a_tag;
    char const *local_tag = leg->leg_local->a_tag;

    url_t const *leg_url = leg->leg_url;
    char const *leg_method = leg->leg_method;

    if (leg->leg_hash != hash)
      continue;
    if (strcmp(leg_i->i_id, i->i_id) != 0)
      continue;

    /* Do not match if the incoming To has tag, but the local does not */
    if (!local_tag && to_tag)
      continue;

    /*
     * Do not match if incoming To has no tag and we have local tag
     * and the tag has been there from the beginning.
     */
    if (local_tag && !to_tag && !leg->leg_tagged)
      continue;

    /* Do not match if incoming From has no tag but remote has a tag */
    if (remote_tag && !from_tag)
      continue;

    /* Avoid matching with itself */
    if (!remote_tag != !from_tag && !local_tag != !to_tag)
      continue;

    if (local_tag && to_tag && !su_casematch(local_tag, to_tag) && to_tag[0])
      continue;
    if (remote_tag && from_tag && !su_casematch(remote_tag, from_tag) && from_tag[0])
      continue;

    if (leg_url && request_uri && url_cmp(leg_url, request_uri))
      continue;
    if (leg_method && method_name && !su_casematch(method_name, leg_method))
      continue;

    /* Perfect match if both local and To have tag
     * or local does not have tag.
     */
    if ((!local_tag || to_tag))
      return leg;

    if (loose_match == NULL)
      loose_match = leg;
  }

  return loose_match;
}

/** Get leg by destination */
nta_leg_t *nta_leg_by_uri(nta_agent_t const *agent, url_string_t const *us)
{
  url_t *url;
  nta_leg_t *leg = NULL;

  if (!agent)
    return NULL;

  if (!us)
    return agent->sa_default_leg;

  url = url_hdup(NULL, us->us_url);

  if (url) {
    agent_aliases(agent, url, NULL);
    leg = dst_find(agent, url, NULL);
    su_free(NULL, url);
  }

  return leg;
}

/** Find a non-dialog leg corresponding to the request uri u0 */
static
nta_leg_t *dst_find(nta_agent_t const *sa,
		    url_t const *u0,
		    char const *method_name)
{
  hash_value_t hash, hash2;
  leg_htable_t const *lht = sa->sa_defaults;
  nta_leg_t **ll, *leg, *loose_match = NULL;
   int again;
  url_t url[1];

  *url = *u0;
  hash = hash_istring(url->url_scheme, ":", 0);
  hash = hash_istring(url->url_host, "", hash);
  hash2 = hash_istring("%", "@", hash);
  hash = hash_istring(url->url_user, "@", hash);

  /* First round, search with user name */
  /* Second round, search without user name */
  do {
    for (ll = leg_htable_hash(lht, hash);
	 (leg = *ll);
	 ll = leg_htable_next(lht, ll)) {
      if (leg->leg_hash != hash)
	continue;
      if (url_cmp(url, leg->leg_url))
	continue;
      if (!method_name) {
	if (leg->leg_method)
	  continue;
	return leg;
      }
      else if (leg->leg_method) {
	if (!su_casematch(method_name, leg->leg_method))
	  continue;
	return leg;
      }
      loose_match = leg;
    }
    if (loose_match)
      return loose_match;

    again = 0;

    if (url->url_user && strcmp(url->url_user, "%")) {
      url->url_user = "%";
      hash = hash2;
      again = 1;
    }
  } while (again);

  return NULL;
}

/** Set leg route and target URL.
 *
 * Sets the leg route and contact using the @RecordRoute and @Contact
 * headers.
 *
 * @param reroute - allow rerouting
 * - if 1, follow @RFC3261 semantics
 * - if 2, response to initial transaction)
 */
static
int leg_route(nta_leg_t *leg,
	      sip_record_route_t const *route,
	      sip_record_route_t const *reverse,
	      sip_contact_t const *contact,
	      int reroute)
{
  su_home_t *home = leg->leg_home;
  sip_route_t *r, r0[1], *old;
  int route_is_set;

  if (!leg)
    return -1;

  if (route == NULL && reverse == NULL && contact == NULL)
    return 0;

  sip_route_init(r0);

  route_is_set = reroute ? leg->leg_route_set : leg->leg_route != NULL;

  if (route_is_set && reroute <= 1) {
    r = leg->leg_route;
  }
  else if (route) {
    r = sip_route_fixdup(home, route); if (!r) return -1;
  }
  else if (reverse) {
    r = sip_route_reverse(home, reverse); if (!r) return -1;
  }
  else
    r = NULL;

#ifdef NTA_STRICT_ROUTING
  /*
   * Handle Contact according to the RFC2543bis04 sections 16.1, 16.2 and 16.4.
   */
  if (contact) {
    *r0->r_url = *contact->m_url;

    if (!(m_r = sip_route_dup(leg->leg_home, r0)))
      return -1;

    /* Append, but replace last entry if it was generated from contact */
    for (rr = &r; *rr; rr = &(*rr)->r_next)
      if (leg->leg_contact_set && (*rr)->r_next == NULL)
	break;
  }
  else
    rr = NULL;

  if (rr) {
    if (*rr)
      su_free(leg->leg_home, *rr);
    *rr = m_r;
  }
  if (m_r != NULL)
    leg->leg_contact_set = 1;

#else
  if (r && r->r_url->url_params)
    leg->leg_loose_route = url_has_param(r->r_url, "lr");

  if (contact) {
    sip_contact_t *target, m[1], *m0;

    sip_contact_init(m);
    *m->m_url = *contact->m_url;
    m->m_url->url_headers = NULL;
    target = sip_contact_dup(leg->leg_home, m);

    if (target && target->m_url->url_params) {
      /* Remove ttl, method. @RFC3261 table 1, page 152 */
      char *p = (char *)target->m_url->url_params;
      p = url_strip_param_string(p, "method");
      p = url_strip_param_string(p, "ttl");
      target->m_url->url_params = p;
    }

    m0 = leg->leg_target, leg->leg_target = target;

    if (m0)
      su_free(leg->leg_home, m0);
  }
#endif

  old = leg->leg_route;
  leg->leg_route = r;

  if (old && old != r)
    msg_header_free(leg->leg_home, (msg_header_t *)old);

  leg->leg_route_set = 1;

  return 0;
}

/** @internal Default leg callback. */
static int
leg_callback_default(nta_leg_magic_t *magic,
		     nta_leg_t  *leg,
		     nta_incoming_t *irq,
		     sip_t const *sip)
{
  nta_incoming_treply(irq,
		      SIP_501_NOT_IMPLEMENTED,
		      TAG_END());
  return 501;
}

/* ====================================================================== */
/* 7) Server-side (incoming) transactions */

#define HTABLE_HASH_IRQ(irq) ((irq)->irq_hash)
HTABLE_BODIES_WITH(incoming_htable, iht, nta_incoming_t, HTABLE_HASH_IRQ,
		   size_t, hash_value_t);

static void incoming_insert(nta_agent_t *agent,
			    incoming_queue_t *queue,
			    nta_incoming_t *irq);

su_inline int incoming_is_queued(nta_incoming_t const *irq);
su_inline void incoming_queue(incoming_queue_t *queue, nta_incoming_t *);
su_inline void incoming_remove(nta_incoming_t *irq);
su_inline void incoming_set_timer(nta_incoming_t *, uint32_t interval);
su_inline void incoming_reset_timer(nta_incoming_t *);
su_inline size_t incoming_mass_destroy(nta_agent_t *, incoming_queue_t *);

static int incoming_set_params(nta_incoming_t *irq, tagi_t const *tags);
su_inline
int incoming_set_compartment(nta_incoming_t *irq, tport_t *tport, msg_t *msg,
			     int create_if_needed);

su_inline nta_incoming_t
  *incoming_call_callback(nta_incoming_t *, msg_t *, sip_t *);
su_inline int incoming_final_failed(nta_incoming_t *irq, msg_t *);
static void incoming_retransmit_reply(nta_incoming_t *irq, tport_t *tport);

/** Create a default server transaction.
 *
 * The default server transaction is used by a proxy to forward responses
 * statelessly.
 *
 * @param agent pointer to agent object
 *
 * @retval pointer to default server transaction object
 * @retval NULL if failed
 */
nta_incoming_t *nta_incoming_default(nta_agent_t *agent)
{
  msg_t *msg;
  su_home_t *home;
  nta_incoming_t *irq;

  if (agent == NULL)
    return su_seterrno(EFAULT), NULL;
  if (agent->sa_default_incoming)
    return su_seterrno(EEXIST), NULL;

  msg = nta_msg_create(agent, 0);
  if (!msg)
    return NULL;

  irq = su_zalloc(home = msg_home(msg), sizeof(*irq));
  if (!irq)
    return (void)msg_destroy(msg), NULL;

  irq->irq_home = home;
  irq->irq_request = NULL;
  irq->irq_agent = agent;
  irq->irq_received = agent_now(agent);
  irq->irq_method = sip_method_invalid;

  irq->irq_default = 1;
  agent->sa_default_incoming = irq;

  return irq;
}

/** Create a server transaction.
 *
 * Create a server transaction for a request message. This function is used
 * when an element processing requests statelessly wants to process a
 * particular request statefully.
 *
 * @param agent pointer to agent object
 * @param leg  pointer to leg object (either @a agent or @a leg may be NULL)
 * @param msg  pointer to message object
 * @param sip  pointer to SIP structure (may be NULL)
 * @param tag,value,... optional tagged parameters
 *
 * @note
 * The ownership of @a msg is taken over by the function even if the
 * function fails.
 *
 * @TAGS
 * @TAG NTATAG_TPORT() specifies the transport used to receive the request
 *      and also default transport for sending the response.
 *
 * @retval nta_incoming_t pointer to the newly created server transaction
 * @retval NULL if failed
 */
nta_incoming_t *nta_incoming_create(nta_agent_t *agent,
				    nta_leg_t *leg,
				    msg_t *msg,
				    sip_t *sip,
				    tag_type_t tag, tag_value_t value, ...)
{
  char const *to_tag = NULL;
  tport_t *tport = NULL;
  ta_list ta;
  nta_incoming_t *irq;

  if (msg == NULL)
    return NULL;

  if (agent == NULL && leg != NULL)
    agent = leg->leg_agent;

  if (sip == NULL)
    sip = sip_object(msg);

  if (agent == NULL || sip == NULL || !sip->sip_request || !sip->sip_cseq)
    return msg_destroy(msg), NULL;

  ta_start(ta, tag, value);

  tl_gets(ta_args(ta),
	  NTATAG_TPORT_REF(tport),
	  TAG_END());
  ta_end(ta);

  if (leg && leg->leg_local)
    to_tag = leg->leg_local->a_tag;

  if (tport == NULL)
    tport = tport_delivered_by(agent->sa_tports, msg);

  irq = incoming_create(agent, msg, sip, tport, to_tag);

  if (!irq)
    msg_destroy(msg);

  return irq;
}

/** @internal Create a new incoming transaction object. */
static
nta_incoming_t *incoming_create(nta_agent_t *agent,
				msg_t *msg,
				sip_t *sip,
				tport_t *tport,
				char const *tag)
{
  nta_incoming_t *irq = su_zalloc(msg_home(msg), sizeof(*irq));

  agent->sa_stats->as_server_tr++;

  if (irq) {
    su_home_t *home;
    incoming_queue_t *queue;
    sip_method_t method = sip->sip_request->rq_method;

    irq->irq_request = msg;
    irq->irq_home = home = msg_home(msg_ref_create(msg));
    irq->irq_agent = agent;

    irq->irq_received = agent_now(agent); /* Timestamp originally from tport */

    irq->irq_method = method;
    irq->irq_rq = sip_request_copy(home, sip->sip_request);
    irq->irq_from = sip_from_copy(home, sip->sip_from);
    irq->irq_to = sip_to_copy(home, sip->sip_to);
    irq->irq_call_id = sip_call_id_copy(home, sip->sip_call_id);
    irq->irq_cseq = sip_cseq_copy(home, sip->sip_cseq);
    irq->irq_via = sip_via_copy(home, sip->sip_via);
    switch (method) {
    case sip_method_ack:
    case sip_method_cancel:
    case sip_method_bye:
    case sip_method_options:
    case sip_method_register:	/* Handling Path is up to application */
    case sip_method_info:
    case sip_method_prack:
    case sip_method_publish:
      break;
    default:
      irq->irq_record_route =
	sip_record_route_copy(home, sip->sip_record_route);
    }
    irq->irq_branch  = sip->sip_via->v_branch;
    irq->irq_reliable_tp = tport_is_reliable(tport);
    irq->irq_extra_100 = 0; /* Sending extra 100 trying false by default */

    if (sip->sip_timestamp)
      irq->irq_timestamp = sip_timestamp_copy(home, sip->sip_timestamp);

    /* Tag transaction */
    if (tag)
      sip_to_tag(home, irq->irq_to, tag);
    irq->irq_tag = irq->irq_to->a_tag;

    if (method != sip_method_ack) {
      int *use_rport = NULL;
      int retry_without_rport = 0;

      if (agent->sa_server_rport)
	use_rport = &retry_without_rport, retry_without_rport = 1;

      if (nta_tpn_by_via(irq->irq_tpn, irq->irq_via, use_rport) < 0)
	SU_DEBUG_1(("%s: bad via\n", __func__));
    }

    incoming_set_compartment(irq, tport, msg, 0);

    if (method == sip_method_invite) {
      irq->irq_must_100rel =
	sip->sip_require && sip_has_feature(sip->sip_require, "100rel");

      if (irq->irq_must_100rel ||
	  (sip->sip_supported &&
	   sip_has_feature(sip->sip_supported, "100rel"))) {
	irq->irq_rseq = su_randint(1, 0x7fffffff); /* Initialize rseq */
      }

      queue = agent->sa_in.proceeding;

      if (irq->irq_reliable_tp)
	incoming_set_timer(irq, agent->sa_t2 / 2); /* N1 = T2 / 2 */
      else
	incoming_set_timer(irq, 200); /* N1 = 200 ms */

      irq->irq_tport = tport_ref(tport);
    }
    else if (method == sip_method_ack) {
      irq->irq_status = 700;	/* Never send reply to ACK */
      irq->irq_completed = 1;
      if (irq->irq_reliable_tp || !agent->sa_is_a_uas) {
	queue = agent->sa_in.terminated;
	irq->irq_terminated = 1;
      }
      else {
	queue = agent->sa_in.completed;	/* Timer J */
      }
    }
    else {
      queue = agent->sa_in.proceeding;
	/* RFC 4320 (nit-actions-03):

   Blacklisting on a late response occurs even over reliable transports.
   Thus, if an element processing a request received over a reliable
   transport is delaying its final response at all, sending a 100 Trying
   well in advance of the timeout will prevent blacklisting.  Sending a
   100 Trying immediately will not harm the transaction as it would over
   UDP, but a policy of always sending such a message results in
   unneccessary traffic.  A policy of sending a 100 Trying after the
   period of time in which Timer E reaches T2 had this been a UDP hop is
   one reasonable compromise.

	 */
      if (agent->sa_extra_100 && irq->irq_reliable_tp)
	incoming_set_timer(irq, agent->sa_t2 / 2); /* T2 / 2 */

      irq->irq_tport = tport_ref(tport);
    }

    irq->irq_hash = NTA_HASH(irq->irq_call_id, irq->irq_cseq->cs_seq);

    incoming_insert(agent, queue, irq);
  }

  return irq;
}

/** @internal
 * Insert incoming transaction to hash table.
 */
static void
incoming_insert(nta_agent_t *agent,
		incoming_queue_t *queue,
		nta_incoming_t *irq)
{
  incoming_queue(queue, irq);

  if (incoming_htable_is_full(agent->sa_incoming))
    incoming_htable_resize(agent->sa_home, agent->sa_incoming, 0);

  if (irq->irq_method != sip_method_ack)
    incoming_htable_insert(agent->sa_incoming, irq);
  else
    /* ACK is appended - final response with tags match with it,
     * not with the original INVITE transaction */
    /* XXX - what about rfc2543 servers, which do not add tag? */
    incoming_htable_append(agent->sa_incoming, irq);
}

/** Call callback for incoming request */
static
int incoming_callback(nta_leg_t *leg, nta_incoming_t *irq, sip_t *sip)
{
  sip_method_t method = sip->sip_request->rq_method;
  char const *method_name = sip->sip_request->rq_method_name;

  /* RFC-3261 section 12.2.2 (page 76) */
  if (leg->leg_dialog &&
      irq->irq_agent->sa_is_a_uas &&
      method != sip_method_ack) {
    uint32_t seq = sip->sip_cseq->cs_seq;

    if (leg->leg_rseq > sip->sip_cseq->cs_seq) {
      SU_DEBUG_3(("nta_leg(%p): out-of-order %s (%u < %u)\n",
		  (void *)leg, method_name, seq, leg->leg_rseq));
      return 500;
    }

    leg->leg_rseq = seq;
  }

  return leg->leg_callback(leg->leg_magic, leg, irq, sip);
}

/**
 * Destroy an incoming transaction.
 *
 * This function does not actually free transaction object, but marks it as
 * disposable. The object is freed after a timeout.
 *
 * @param irq incoming request object to be destroyed
 */
void nta_incoming_destroy(nta_incoming_t *irq)
{
  if (irq) {
    irq->irq_callback = NULL;
    irq->irq_magic = NULL;
    irq->irq_destroyed = 1;
    if (!irq->irq_in_callback) {
      if (irq->irq_terminated || irq->irq_default)
	incoming_free(irq);
      else if (irq->irq_status < 200)
	nta_incoming_treply(irq, SIP_500_INTERNAL_SERVER_ERROR, TAG_END());
    }
  }
}

/** @internal
 * Initialize a queue for incoming transactions.
 */
static void
incoming_queue_init(incoming_queue_t *queue, unsigned timeout)
{
  memset(queue, 0, sizeof *queue);
  queue->q_tail = &queue->q_head;
  queue->q_timeout = timeout;
}

/** Change the timeout value of a queue */
static void
incoming_queue_adjust(nta_agent_t *sa,
		      incoming_queue_t *queue,
		      uint32_t timeout)
{
  nta_incoming_t *irq;
  uint32_t latest;

  if (timeout >= queue->q_timeout || !queue->q_head) {
    queue->q_timeout = timeout;
    return;
  }

  latest = set_timeout(sa, queue->q_timeout = timeout);

  for (irq = queue->q_head; irq; irq = irq->irq_next) {
    if ((int32_t)(irq->irq_timeout - latest) > 0)
      irq->irq_timeout = latest;
  }
}

/** @internal
 * Test if an incoming transaction is in a queue.
 */
su_inline
int incoming_is_queued(nta_incoming_t const *irq)
{
  return irq && irq->irq_queue;
}

/** @internal
 * Insert an incoming transaction into a queue.
 *
 * Insert a server transaction into a queue, and sets the corresponding
 * timeout at the same time.
 */
su_inline
void incoming_queue(incoming_queue_t *queue,
		    nta_incoming_t *irq)
{
  if (irq->irq_queue == queue) {
    assert(queue->q_timeout == 0);
    return;
  }

  if (incoming_is_queued(irq))
    incoming_remove(irq);

  assert(*queue->q_tail == NULL);

  irq->irq_timeout = set_timeout(irq->irq_agent, queue->q_timeout);

  irq->irq_queue = queue;
  irq->irq_prev = queue->q_tail;
  *queue->q_tail = irq;
  queue->q_tail = &irq->irq_next;
  queue->q_length++;
}

/** @internal
 * Remove an incoming transaction from a queue.
 */
su_inline
void incoming_remove(nta_incoming_t *irq)
{
  assert(incoming_is_queued(irq));
  assert(irq->irq_queue->q_length > 0);

  if ((*irq->irq_prev = irq->irq_next))
    irq->irq_next->irq_prev = irq->irq_prev;
  else
    irq->irq_queue->q_tail = irq->irq_prev, assert(!*irq->irq_queue->q_tail);

  irq->irq_queue->q_length--;
  irq->irq_next = NULL;
  irq->irq_prev = NULL;
  irq->irq_queue = NULL;
  irq->irq_timeout = 0;
}

su_inline
void incoming_set_timer(nta_incoming_t *irq, uint32_t interval)
{
  nta_incoming_t **rq;

  assert(irq);

  if (interval == 0) {
    incoming_reset_timer(irq);
    return;
  }

  if (irq->irq_rprev) {
    if ((*irq->irq_rprev = irq->irq_rnext))
      irq->irq_rnext->irq_rprev = irq->irq_rprev;
    if (irq->irq_agent->sa_in.re_t1 == &irq->irq_rnext)
      irq->irq_agent->sa_in.re_t1 = irq->irq_rprev;
  } else {
    irq->irq_agent->sa_in.re_length++;
  }

  irq->irq_retry = set_timeout(irq->irq_agent, irq->irq_interval = interval);

  rq = irq->irq_agent->sa_in.re_t1;

  if (!(*rq) || (int32_t)((*rq)->irq_retry - irq->irq_retry) > 0)
    rq = &irq->irq_agent->sa_in.re_list;

  while (*rq && (int32_t)((*rq)->irq_retry - irq->irq_retry) <= 0)
    rq = &(*rq)->irq_rnext;

  if ((irq->irq_rnext = *rq))
    irq->irq_rnext->irq_rprev = &irq->irq_rnext;
  *rq = irq;
  irq->irq_rprev = rq;

  /* Optimization: keep special place for transactions with T1 interval */
  if (interval == irq->irq_agent->sa_t1)
    irq->irq_agent->sa_in.re_t1 = rq;
}

su_inline
void incoming_reset_timer(nta_incoming_t *irq)
{
  if (irq->irq_rprev) {
    if ((*irq->irq_rprev = irq->irq_rnext))
      irq->irq_rnext->irq_rprev = irq->irq_rprev;
    if (irq->irq_agent->sa_in.re_t1 == &irq->irq_rnext)
      irq->irq_agent->sa_in.re_t1 = irq->irq_rprev;
    irq->irq_agent->sa_in.re_length--;
  }

  irq->irq_interval = 0, irq->irq_retry = 0;
  irq->irq_rnext = NULL, irq->irq_rprev = NULL;
}

/** @internal
 * Free an incoming transaction.
 */
static
void incoming_free(nta_incoming_t *irq)
{
  SU_DEBUG_9(("nta: incoming_free(%p)\n", (void *)irq));

  incoming_cut_off(irq);
  incoming_reclaim(irq);
}

/** Remove references to the irq */
su_inline
void incoming_cut_off(nta_incoming_t *irq)
{
  nta_agent_t *agent = irq->irq_agent;

  assert(agent);

  if (irq->irq_default) {
    if (irq == agent->sa_default_incoming)
      agent->sa_default_incoming = NULL;
    irq->irq_default = 0;
    return;
  }

  if (incoming_is_queued(irq))
    incoming_remove(irq);

  incoming_reset_timer(irq);

  incoming_htable_remove(agent->sa_incoming, irq);

  if (irq->irq_cc)
    nta_compartment_decref(&irq->irq_cc);

  if (irq->irq_tport)
    tport_decref(&irq->irq_tport);
}

/** Reclaim the memory used by irq */
su_inline
void incoming_reclaim(nta_incoming_t *irq)
{
  su_home_t *home = irq->irq_home;
  nta_reliable_t *rel, *rel_next;

  if (irq->irq_request)
    msg_destroy(irq->irq_request), irq->irq_request = NULL;
  if (irq->irq_request2)
    msg_destroy(irq->irq_request2), irq->irq_request2 = NULL;
  if (irq->irq_response)
    msg_destroy(irq->irq_response), irq->irq_response = NULL;

  for (rel = irq->irq_reliable; rel; rel = rel_next) {
    rel_next = rel->rel_next;
    if (rel->rel_unsent)
      msg_destroy(rel->rel_unsent);
    su_free(irq->irq_agent->sa_home, rel);
  }

  irq->irq_home = NULL;

  su_free(home, irq);

  msg_destroy((msg_t *)home);
}

/** Queue request to be freed */
su_inline
void incoming_free_queue(incoming_queue_t *q, nta_incoming_t *irq)
{
  incoming_cut_off(irq);
  incoming_queue(q, irq);
}

/** Reclaim memory used by queue of requests */
static
void incoming_reclaim_queued(su_root_magic_t *rm,
			     su_msg_r msg,
			     union sm_arg_u *u)
{
  incoming_queue_t *q = u->a_incoming_queue;
  nta_incoming_t *irq, *irq_next;

  SU_DEBUG_9(("incoming_reclaim_all(%p, %p, %p)\n",
	      (void *)rm, (void *)msg, (void *)u));

  for (irq = q->q_head; irq; irq = irq_next) {
    irq_next = irq->irq_next;
    incoming_reclaim(irq);
  }
}

/**Bind a callback and context to an incoming transaction object
 *
 * Set the callback function and context pointer attached to an incoming
 * request object. The callback function will be invoked if the incoming
 * request is cancelled, or if the final response to an incoming @b INVITE
 * request has been acknowledged.
 *
 * If the callback is NULL, or no callback has been bound, NTA invokes the
 * request callback of the call leg.
 *
 * @param irq      incoming transaction
 * @param callback callback function
 * @param magic    application context
 */
void nta_incoming_bind(nta_incoming_t *irq,
		       nta_ack_cancel_f *callback,
		       nta_incoming_magic_t *magic)
{
  if (irq) {
    irq->irq_callback = callback;
    irq->irq_magic = magic;
  }
}

/** Add a @To tag to incoming request if needed.
 *
 * If @a tag is NULL, a new tag is generated.
 */
char const *nta_incoming_tag(nta_incoming_t *irq, char const *tag)
{
  if (!irq)
    return su_seterrno(EFAULT), NULL;

  if (irq->irq_default)
    return su_seterrno(EINVAL), NULL;

  if (tag && strchr(tag, '='))
    tag = strchr(tag, '=') + 1;

  if (tag && irq->irq_tag && !su_casematch(tag, irq->irq_tag))
    return NULL;

  if (!irq->irq_tag) {
    if (tag)
      tag = su_strdup(irq->irq_home, tag);
    else
      tag = nta_agent_newtag(irq->irq_home, NULL, irq->irq_agent);

    if (!tag)
      return tag;

    irq->irq_tag = tag;
    irq->irq_tag_set = 1;
  }

  return irq->irq_tag;
}


/**Get request message.
 *
 * Retrieve the incoming request message of the incoming transaction. Note
 * that the message is not copied, but a new reference to it is created.
 *
 * @param irq incoming transaction handle
 *
 * @retval
 * A pointer to request message is returned.
 */
msg_t *nta_incoming_getrequest(nta_incoming_t *irq)
{
  msg_t *msg = NULL;

  if (irq && !irq->irq_default)
    msg = msg_ref_create(irq->irq_request);

  return msg;
}

/**Get ACK or CANCEL message.
 *
 * Retrieve the incoming ACK or CANCEL request message of the incoming
 * transaction. Note that the ACK or CANCEL message is not copied, but a new
 * reference to it is created.
 *
 * @param irq incoming transaction handle
 *
 * @retval A pointer to request message is returned, or NULL if there is no
 * CANCEL or ACK received.
 */
msg_t *nta_incoming_getrequest_ackcancel(nta_incoming_t *irq)
{
  msg_t *msg = NULL;

  if (irq && irq->irq_request2)
    msg = msg_ref_create(irq->irq_request2);

  return msg;
}

/**Get response message.
 *
 * Retrieve the response message latest sent by the server transaction. Note
 * that the message is not copied, but a new reference to it is created. Use
 * msg_dup() or msg_copy() to make a copy of it.
 *
 * @param irq incoming transaction handle
 *
 * @retval
 * A pointer to a response message is returned.
 */
msg_t *nta_incoming_getresponse(nta_incoming_t *irq)
{
  msg_t *msg = NULL;

  if (irq && irq->irq_response)
    msg = msg_ref_create(irq->irq_response);

  return msg;
}

/** Get method of a server transaction. */
sip_method_t nta_incoming_method(nta_incoming_t const *irq)
{
  return irq ? irq->irq_method : sip_method_invalid;
}

/** Get method name of a server transaction. */
char const *nta_incoming_method_name(nta_incoming_t const *irq)
{
  if (irq == NULL)
    return NULL;
  else if (irq->irq_rq)
    return irq->irq_rq->rq_method_name;
  else
    return "*";
}

/** Get Request-URI of a server transaction */
url_t const *nta_incoming_url(nta_incoming_t const *irq)
{
  return irq && irq->irq_rq ? irq->irq_rq->rq_url : NULL;
}

/** Get sequence number of a server transaction.
 */
uint32_t nta_incoming_cseq(nta_incoming_t const *irq)
{
  return irq && irq->irq_cseq ? irq->irq_cseq->cs_seq : 0;
}

/** Get local tag for incoming request */
char const *nta_incoming_gettag(nta_incoming_t const *irq)
{
  return irq ? irq->irq_tag : 0;
}

/**
 * Get status code of a server transaction.
 */
int nta_incoming_status(nta_incoming_t const *irq)
{
  return irq ? irq->irq_status : 400;
}

/** Get application context for a server transaction.
 *
 * @param irq server transaction
 * @param callback callback pointer
 *
 * Return the application context bound to the server transaction. If the @a
 * callback function pointer is given, return application context only if
 * the callback matches with the callback bound to the server transaction.
 *
 */
nta_incoming_magic_t *nta_incoming_magic(nta_incoming_t *irq,
					 nta_ack_cancel_f *callback)
{
  return irq && (callback == NULL || irq->irq_callback == callback)
    ? irq->irq_magic : NULL;
}

/** When received.
 *
 * Return timestamp from the reception of the initial request.
 *
 * @NEW_1_12_7.
 */
sip_time_t nta_incoming_received(nta_incoming_t *irq,
				 su_nanotime_t *return_nano)
{
  su_time_t tv = { 0, 0 };

  if (irq)
    tv = irq->irq_received;

  if (return_nano)
    *return_nano = (su_nanotime_t)tv.tv_sec * 1000000000 + tv.tv_usec * 1000;

  return tv.tv_sec;
}

/** Find incoming transaction. */
nta_incoming_t *nta_incoming_find(nta_agent_t const *agent,
				  sip_t const *sip,
				  sip_via_t const *v)
{
  if (agent && sip && v)
    return incoming_find(agent, sip, v, NULL, NULL, NULL);
  else
    return NULL;
}

/** Find a matching server transaction object.
 *
 * Check also for requests to merge, to ACK, or to CANCEL.
 */
static nta_incoming_t *incoming_find(nta_agent_t const *agent,
				     sip_t const *sip,
				     sip_via_t const *v,
				     nta_incoming_t **return_merge,
				     nta_incoming_t **return_ack,
				     nta_incoming_t **return_cancel)
{
  sip_cseq_t const *cseq = sip->sip_cseq;
  sip_call_id_t const *i = sip->sip_call_id;
  sip_to_t const *to = sip->sip_to;
  sip_from_t const *from = sip->sip_from;
  sip_request_t *rq = sip->sip_request;
  incoming_htable_t const *iht = agent->sa_incoming;
  hash_value_t hash = NTA_HASH(i, cseq->cs_seq);
  char const *magic_branch;

  nta_incoming_t **ii, *irq;

  int is_uas_ack = return_ack && agent->sa_is_a_uas;

  if (v->v_branch && su_casenmatch(v->v_branch, "z9hG4bK", 7))
    magic_branch = v->v_branch + 7;
  else
    magic_branch = NULL;

  for (ii = incoming_htable_hash(iht, hash);
       (irq = *ii);
       ii = incoming_htable_next(iht, ii)) {
    if (hash != irq->irq_hash ||
	irq->irq_call_id->i_hash != i->i_hash ||
	strcmp(irq->irq_call_id->i_id, i->i_id))
      continue;
    if (irq->irq_cseq->cs_seq != cseq->cs_seq)
      continue;
    if (su_strcasecmp(irq->irq_from->a_tag, from->a_tag))
      continue;

    if (is_uas_ack &&
	irq->irq_method == sip_method_invite &&
	200 <= irq->irq_status && irq->irq_status < 300 &&
	su_casematch(irq->irq_tag, to->a_tag)) {
      *return_ack = irq;
      return NULL;
    }

    if (magic_branch) {
      /* RFC3261 17.2.3:
       *
       * The request matches a transaction if branch and sent-by in topmost
       * the method of the request matches the one that created the
       * transaction, except for ACK, where the method of the request
       * that created the transaction is INVITE.
       */
      if (irq->irq_via->v_branch &&
	  su_casematch(irq->irq_via->v_branch + 7, magic_branch) &&
	  su_casematch(irq->irq_via->v_host, v->v_host) &&
	  su_strmatch(irq->irq_via->v_port, v->v_port)) {
	if (irq->irq_method == cseq->cs_method &&
	    strcmp(irq->irq_cseq->cs_method_name,
		   cseq->cs_method_name) == 0)
	  return irq;
	if (return_ack && irq->irq_method == sip_method_invite)
	  return *return_ack = irq, NULL;
	if (return_cancel && irq->irq_method != sip_method_ack)
	  return *return_cancel = irq, NULL;
      }
    }
    else {
      /* No magic branch */

      /* INVITE request matches a transaction if
	 the Request-URI, To tag, From tag, Call-ID, CSeq, and
	 top Via header match */

      /* From tag, Call-ID, and CSeq number has been matched above */

      /* Match top Via header field */
      if (!su_casematch(irq->irq_via->v_branch, v->v_branch) ||
	  !su_casematch(irq->irq_via->v_host, v->v_host) ||
	  !su_strmatch(irq->irq_via->v_port, v->v_port))
	;
      /* Match Request-URI */
      else if (url_cmp(irq->irq_rq->rq_url, rq->rq_url))
	;
      else {
	/* Match CSeq */
	if (irq->irq_method == cseq->cs_method &&
	    su_strmatch(irq->irq_cseq->cs_method_name, cseq->cs_method_name)) {
	  /* Match To tag  */
	  if (!su_strcasecmp(irq->irq_to->a_tag, to->a_tag))
	    return irq;		/* found */
	}
	else if (
	  /* Tag set by UAS */
	  su_strcasecmp(irq->irq_tag, to->a_tag) &&
	  /* Original tag */
	  su_strcasecmp(irq->irq_to->a_tag, to->a_tag))
	  ;
	else if (return_ack && irq->irq_method == sip_method_invite)
	  return *return_ack = irq, NULL;
	else if (return_cancel && irq->irq_method != sip_method_ack)
	  return *return_cancel = irq, NULL;
      }
    }

    /* RFC3261 - section 8.2.2.2 Merged Requests */
    if (return_merge) {
      if (irq->irq_cseq->cs_method == cseq->cs_method &&
	  strcmp(irq->irq_cseq->cs_method_name,
		 cseq->cs_method_name) == 0)
	*return_merge = irq, return_merge = NULL;
    }
  }

  return NULL;
}

/** Process retransmitted requests. */
su_inline
int
incoming_recv(nta_incoming_t *irq, msg_t *msg, sip_t *sip, tport_t *tport)
{
  nta_agent_t *agent = irq->irq_agent;

  agent->sa_stats->as_recv_retry++;

  if (irq->irq_status >= 100) {
    SU_DEBUG_5(("nta: re-received %s request, retransmitting %u reply\n",
		sip->sip_request->rq_method_name, irq->irq_status));
	 incoming_retransmit_reply(irq, tport);
  }
  else if (irq->irq_agent->sa_extra_100 &&
           irq->irq_extra_100) {
    /* Agent and Irq configured to answer automatically with 100 Trying */
    if (irq->irq_method == sip_method_invite ||
	/*
	 * Send 100 trying to non-invite if at least half of T2 has expired
	 * since the transaction was created.
	 */
	su_duration(agent_now(irq->irq_agent), irq->irq_received) * 2U >
	irq->irq_agent->sa_t2) {
      SU_DEBUG_5(("nta: re-received %s request, sending 100 Trying\n",
		  sip->sip_request->rq_method_name));
      nta_incoming_treply(irq, SIP_100_TRYING, NTATAG_TPORT(tport), TAG_END());
    }
  }

  msg_destroy(msg);

  return 0;
}

su_inline
int incoming_ack(nta_incoming_t *irq, msg_t *msg, sip_t *sip, tport_t *tport)
{
  nta_agent_t *agent = irq->irq_agent;

  /* Process ACK separately? */
  if (irq->irq_status >= 200 && irq->irq_status < 300 && !agent->sa_is_a_uas)
    return -1;

  if (irq->irq_queue == agent->sa_in.inv_completed) {
    if (!irq->irq_confirmed)
      agent->sa_stats->as_acked_tr++;

    irq->irq_confirmed = 1;
    incoming_reset_timer(irq); /* Reset timer G */

    if (!irq->irq_reliable_tp) {
      incoming_queue(agent->sa_in.inv_confirmed, irq); /* Timer I */
    }
    else {
      irq->irq_terminated = 1;
      incoming_queue(agent->sa_in.terminated, irq);
    }

    if (!irq->irq_destroyed) {
      if (!irq->irq_callback)	/* Process ACK normally */
	return -1;

      incoming_call_callback(irq, msg, sip); /* ACK callback */
    }
  } else if (irq->irq_queue == agent->sa_in.proceeding ||
	     irq->irq_queue == agent->sa_in.preliminary)
    return -1;
  else
    assert(irq->irq_queue == agent->sa_in.inv_confirmed ||
	   irq->irq_queue == agent->sa_in.terminated);

  msg_destroy(msg);

  return 0;
}

/** Respond to the CANCEL. */
su_inline
int incoming_cancel(nta_incoming_t *irq, msg_t *msg, sip_t *sip,
		    tport_t *tport)
{
  nta_agent_t *agent = irq->irq_agent;

  /* According to the RFC 3261, this INVITE has been destroyed */
  if (irq->irq_method == sip_method_invite &&
      200 <= irq->irq_status && irq->irq_status < 300) {
    mreply(agent, NULL, SIP_481_NO_TRANSACTION, msg,
	   tport, 0, 0, NULL,
	   TAG_END());
    return 0;
  }

  /* UAS MUST use same tag in final response to CANCEL and INVITE */
  if (agent->sa_is_a_uas && irq->irq_tag == NULL) {
    nta_incoming_tag(irq, NULL);
  }

  mreply(agent, NULL, SIP_200_OK, msg_ref_create(msg),
	 tport, 0, 0, irq->irq_tag,
	 TAG_END());

  /* We have already sent final response */
  if (irq->irq_completed || irq->irq_method != sip_method_invite) {
    msg_destroy(msg);
    return 0;
  }

  if (!irq->irq_canceled) {
    irq->irq_canceled = 1;
    agent->sa_stats->as_canceled_tr++;
    irq = incoming_call_callback(irq, msg, sip);
  }

  if (irq && !irq->irq_completed && agent->sa_cancel_487)
    /* Respond to the cancelled request */
    nta_incoming_treply(irq, SIP_487_REQUEST_CANCELLED, TAG_END());

  msg_destroy(msg);

  return 0;
}

/** Merge request */
static
void request_merge(nta_agent_t *agent,
		   msg_t *msg, sip_t *sip, tport_t *tport,
		   char const *to_tag)
{
  nta_incoming_t *irq;

  agent->sa_stats->as_merged_request++;

  irq = incoming_create(agent, msg, sip, tport, to_tag);

  if (irq) {
    nta_incoming_treply(irq, 482, "Request merged", TAG_END());
    nta_incoming_destroy(irq);
  } else {
    SU_DEBUG_3(("nta: request_merge(): cannot create transaction for %s\n",
		sip->sip_request->rq_method_name));
    mreply(agent, NULL, 482, "Request merged", msg,
	   tport, 0, 0, NULL,
	   TAG_END());
  }
}

/**@typedef nta_ack_cancel_f
 *
 * Callback function prototype for CANCELed/ACKed requests
 *
 * This is a callback function is invoked by NTA when an incoming request
 * has been cancelled or an response to an incoming INVITE request has been
 * acknowledged.
 *
 * @param magic   incoming request context
 * @param ireq    incoming request
 * @param sip     ACK/CANCEL message
 *
 * @retval 0
 * This callback function should return always 0.
 */

/** Call callback of incoming transaction */
su_inline
nta_incoming_t *
incoming_call_callback(nta_incoming_t *irq, msg_t *msg, sip_t *sip)
{
  if (irq->irq_callback) {
    irq->irq_in_callback = 1;
    irq->irq_request2 = msg;
    irq->irq_callback(irq->irq_magic, irq, sip);
    irq->irq_request2 = NULL;
    irq->irq_in_callback = 0;

    if (irq->irq_terminated && irq->irq_destroyed)
      incoming_free(irq), irq = NULL;
  }
  return irq;
}

/**Set server transaction parameters.
 *
 * Sets the server transaction parameters. Among others, parameters determine the way
 * the SigComp compression is handled.
 *
 * @TAGS
 * NTATAG_COMP(), NTATAG_SIGCOMP_CLOSE() and NTATAG_EXTRA_100().
 *
 * @retval number of set parameters when succesful
 * @retval -1 upon an error
 */
int nta_incoming_set_params(nta_incoming_t *irq,
			    tag_type_t tag, tag_value_t value, ...)
{
  int retval = -1;

  if (irq) {
    ta_list ta;
    ta_start(ta, tag, value);
    retval = incoming_set_params(irq, ta_args(ta));
    ta_end(ta);
  }
  else {
    su_seterrno(EINVAL);
  }

  return retval;
}

static
int incoming_set_params(nta_incoming_t *irq, tagi_t const *tags)
{
  int retval = 0;

  tagi_t const *t;
  char const *comp = NONE;
  struct sigcomp_compartment *cc = NONE;

  if (irq->irq_default)
    return retval;

  for (t = tags; t; t = tl_next(t)) {
    tag_type_t tt = t->t_tag;

    if (ntatag_comp == tt)
      comp = (char const *)t->t_value, retval++;

    else if (ntatag_sigcomp_close == tt)
      irq->irq_sigcomp_zap = t->t_value != 0, retval++;

    else if (tptag_compartment == tt)
      cc = (void *)t->t_value, retval++;

    else if (ntatag_extra_100 == tt)
      irq->irq_extra_100 = t->t_value != 0, retval++;
  }

  if (cc != NONE) {
    if (cc)
      agent_accept_compressed(irq->irq_agent, irq->irq_request, cc);
    if (irq->irq_cc)
      nta_compartment_decref(&irq->irq_cc);
    irq->irq_cc = nta_compartment_ref(cc);
  }
  else if (comp != NULL && comp != NONE && irq->irq_cc == NULL) {
    incoming_set_compartment(irq, irq->irq_tport, irq->irq_request, 1);
  }

  else if (comp == NULL) {
    irq->irq_tpn->tpn_comp = NULL;
  }

  return retval;
}

su_inline
int incoming_set_compartment(nta_incoming_t *irq, tport_t *tport, msg_t *msg,
			     int create_if_needed)
{
  if (!nta_compressor_vtable)
    return 0;

  if (irq->irq_cc == NULL
      || irq->irq_tpn->tpn_comp
      || tport_delivered_with_comp(tport, msg, NULL) != -1) {
    struct sigcomp_compartment *cc;

    cc = agent_compression_compartment(irq->irq_agent, tport, irq->irq_tpn,
				       create_if_needed);

    if (cc)
      agent_accept_compressed(irq->irq_agent, msg, cc);

    irq->irq_cc = cc;
  }

  return 0;
}

/** Add essential headers to the response message */
static int nta_incoming_response_headers(nta_incoming_t *irq,
					 msg_t *msg,
					 sip_t *sip)
{
  int clone = 0;
  su_home_t *home = msg_home(msg);

  if (!sip->sip_from)
    clone = 1, sip->sip_from = sip_from_copy(home, irq->irq_from);
  if (!sip->sip_to)
    clone = 1, sip->sip_to = sip_to_copy(home, irq->irq_to);
  if (!sip->sip_call_id)
    clone = 1, sip->sip_call_id = sip_call_id_copy(home, irq->irq_call_id);
  if (!sip->sip_cseq)
    clone = 1, sip->sip_cseq = sip_cseq_copy(home, irq->irq_cseq);
  if (!sip->sip_via)
    clone = 1, sip->sip_via = sip_via_copy(home, irq->irq_via);

  if (clone)
    msg_set_parent(msg, (msg_t *)irq->irq_home);

  if (!sip->sip_from || !sip->sip_to || !sip->sip_call_id || !sip->sip_cseq || !sip->sip_via)
    return -1;

  return 0;
}

/** Complete a response message.
 *
 * @param irq     server transaction object
 * @param msg     response message to be completed
 * @param status  status code (in range 100 - 699)
 * @param phrase  status phrase (may be NULL)
 * @param tag,value,... taged argument list
 *
 * Generate status structure based on @a status and @a phrase.
 * Add essential headers to the response message:
 * @From, @To, @CallID, @CSeq, @Via, and optionally
 * @RecordRoute.
 */
int nta_incoming_complete_response(nta_incoming_t *irq,
				   msg_t *msg,
				   int status,
				   char const *phrase,
				   tag_type_t tag, tag_value_t value, ...)
{
  su_home_t *home = msg_home(msg);
  sip_t *sip = sip_object(msg);
  int retval;
  ta_list ta;

  if (irq == NULL || sip == NULL)
    return su_seterrno(EFAULT), -1;

  if (status != 0 && (status < 100 || status > 699))
    return su_seterrno(EINVAL), -1;

  if (status != 0 && !sip->sip_status)
    sip->sip_status = sip_status_create(home, status, phrase, NULL);

  ta_start(ta, tag, value);
  retval = sip_add_tl(msg, sip, ta_tags(ta));
  ta_end(ta);

  if (retval < 0)
    return -1;

  if (irq->irq_default)
    return sip_complete_message(msg);

  if (status > 100 && !irq->irq_tag) {
    if (sip->sip_to)
      nta_incoming_tag(irq, sip->sip_to->a_tag);
    else
      nta_incoming_tag(irq, NULL);
  }

  if (nta_incoming_response_headers(irq, msg, sip) < 0)
    return -1;

  if (sip->sip_status && sip->sip_status->st_status > 100 &&
      irq->irq_tag && sip->sip_to && !sip->sip_to->a_tag)
    if (sip_to_tag(home, sip->sip_to, irq->irq_tag) < 0)
      return -1;

  if (status < 300 && !sip->sip_record_route && irq->irq_record_route)
    if (sip_add_dup(msg, sip, (sip_header_t *)irq->irq_record_route) < 0)
      return -1;

  return sip_complete_message(msg);
}


/** Create a response message for request.
 *
 * @NEW_1_12_5.
 */
msg_t *nta_incoming_create_response(nta_incoming_t *irq,
				    int status, char const *phrase)
{
  msg_t *msg = NULL;
  sip_t *sip;

  if (irq) {
    msg = nta_msg_create(irq->irq_agent, 0);
    sip = sip_object(msg);

    if (sip) {
      if (status != 0)
	sip->sip_status = sip_status_create(msg_home(msg), status, phrase, NULL);

      if (nta_incoming_response_headers(irq, msg, sip) < 0)
	msg_destroy(msg), msg = NULL;
    }
  }

  return msg;
}


/**Reply to an incoming transaction request.
 *
 * This function creates a response message to an incoming request and sends
 * it to the client.
 *
 * @note
 * It is possible to send several non-final (1xx) responses, but only one
 * final response.
 *
 * @param irq    incoming request
 * @param status status code
 * @param phrase status phrase (may be NULL if status code is well-known)
 * @param tag,value,... optional additional headers terminated by TAG_END()
 *
 * @retval 0 when succesful
 * @retval -1 upon an error
 */
int nta_incoming_treply(nta_incoming_t *irq,
			int status,
			char const *phrase,
			tag_type_t tag, tag_value_t value, ...)
{
  int retval = -1;

  if (irq &&
      (irq->irq_status < 200 || status < 200 ||
       (irq->irq_method == sip_method_invite && status < 300))) {
    ta_list ta;
    msg_t *msg = nta_msg_create(irq->irq_agent, 0);

    ta_start(ta, tag, value);

    if (!msg)
      ;
    else if (nta_incoming_complete_response(irq, msg, status, phrase,
					    ta_tags(ta)) < 0)
      msg_destroy(msg);
    else if (incoming_set_params(irq, ta_args(ta)) < 0)
      msg_destroy(msg);
    else
      retval = nta_incoming_mreply(irq, msg);

    ta_end(ta);

    if (retval < 0 && status >= 200)
      incoming_final_failed(irq, NULL);
  }

  return retval;
}

/**
 * Return a response message to client.
 *
 * @note
 * The ownership of @a msg is taken over by the function even if the
 * function fails.
 *
 * @retval 0 when succesful
 * @retval -1 upon an error
 */
int nta_incoming_mreply(nta_incoming_t *irq, msg_t *msg)
{
  sip_t *sip = sip_object(msg);

  int status;

  if (irq == NULL) {
    msg_destroy(msg);
    return -1;
  }

  if (msg == NULL || sip == NULL)
    return -1;

  if (msg == irq->irq_response)
    return 0;

  if (!sip->sip_status || !sip->sip_via || !sip->sip_cseq)
    return incoming_final_failed(irq, msg);

  assert (sip->sip_cseq->cs_method == irq->irq_method || irq->irq_default);

  status = sip->sip_status->st_status;

  if (!irq->irq_tag && status > 100 && !irq->irq_default)
    nta_incoming_tag(irq, NULL);

  if (/* (irq->irq_confirmed && status >= 200) || */
      (irq->irq_completed && status >= 300)) {
    SU_DEBUG_3(("%s: already %s transaction\n", __func__,
		irq->irq_confirmed ? "confirmed" : "completed"));
    msg_destroy(msg);
    return -1;
  }

  if (irq->irq_must_100rel && !sip->sip_rseq && status > 100 && status < 200) {
    /* This nta_reliable_t object will be destroyed by PRACK or timeout */
    if (nta_reliable_mreply(irq, NULL, NULL, msg))
      return 0;

    return -1;
  }

  if (status >= 200 && irq->irq_reliable && irq->irq_reliable->rel_unsent) {
    if (reliable_final(irq, msg, sip) == 0)
      return 0;
  }

  return incoming_reply(irq, msg, sip);
}



/** Send the response message.
 *
 * @note The ownership of msg is handled to incoming_reply().
 */
int incoming_reply(nta_incoming_t *irq, msg_t *msg, sip_t *sip)
{
  nta_agent_t *agent = irq->irq_agent;
  int status = sip->sip_status->st_status;
  int sending = 1;
  int *use_rport = NULL;
  int retry_without_rport = 0;
  tp_name_t *tpn, default_tpn[1];

  if (status == 408 &&
      irq->irq_method != sip_method_invite &&
      !agent->sa_pass_408 &&
      !irq->irq_default) {
    /* RFC 4320 nit-actions-03 Action 2:

   A transaction-stateful SIP element MUST NOT send a response with
   Status-Code of 408 to a non-INVITE request.  As a consequence, an
   element that can not respond before the transaction expires will not
   send a final response at all.
    */
    sending = 0;
  }

  if (irq->irq_status == 0 && irq->irq_timestamp && !sip->sip_timestamp)
    incoming_timestamp(irq, msg, sip);

  if (irq->irq_default) {
    if (agent->sa_server_rport)
      use_rport = &retry_without_rport, retry_without_rport = 1;
    tpn = default_tpn;
    if (nta_tpn_by_via(tpn, sip->sip_via, use_rport) < 0)
      tpn = NULL;
  }
  else {
    tpn = irq->irq_tpn;
  }

  if (sip_complete_message(msg) < 0)
    SU_DEBUG_1(("%s: sip_complete_message() failed\n", __func__));
  else if (msg_serialize(msg, (msg_pub_t *)sip) < 0)
    SU_DEBUG_1(("%s: sip_serialize() failed\n", __func__));
  else if (!(irq->irq_tport) &&
	   !(tport_decref(&irq->irq_tport),
	     irq->irq_tport = tpn ? tport_by_name(agent->sa_tports, tpn) : 0))
    SU_DEBUG_1(("%s: no tport\n", __func__));
  else {
    int i, err = 0;
    tport_t *tp = NULL;
    incoming_queue_t *queue;

    char const *method_name;
    uint32_t cseq;

    if (irq->irq_default) {
      assert(sip->sip_cseq);
      method_name = sip->sip_cseq->cs_method_name, cseq = sip->sip_cseq->cs_seq;
    }
    else {
      method_name = irq->irq_rq->rq_method_name, cseq = irq->irq_cseq->cs_seq;
    }

    if (sending) {
      for (i = 0; i < 3; i++) {
	tp = tport_tsend(irq->irq_tport, msg, tpn,
			 IF_SIGCOMP_TPTAG_COMPARTMENT(irq->irq_cc)
			 TPTAG_MTU(INT_MAX),
			 TAG_END());
	if (tp)
	  break;

	err = msg_errno(msg);
	SU_DEBUG_5(("%s: tport_tsend: %s%s\n",
		    __func__, su_strerror(err),
		    err == EPIPE ? "(retrying)" : ""));

	if (err != EPIPE && err != ECONNREFUSED)
	  break;
	tport_decref(&irq->irq_tport);
	irq->irq_tport = tport_ref(tport_by_name(agent->sa_tports, tpn));
      }

      if (!tp) {
	SU_DEBUG_3(("%s: tport_tsend: "
		    "error (%s) while sending %u %s for %s (%u)\n",
		    __func__, su_strerror(err),
		    status, sip->sip_status->st_phrase, method_name, cseq));
	if (status < 200)
	  msg_destroy(msg);
	else
	  incoming_final_failed(irq, msg);
	return 0;
      }

      agent->sa_stats->as_sent_msg++;
      agent->sa_stats->as_sent_response++;
    }

    SU_DEBUG_5(("nta: %s %u %s for %s (%u)\n",
		sending ? "sent" : "not sending",
		status, sip->sip_status->st_phrase, method_name, cseq));

    if (irq->irq_default) {
      msg_destroy(msg);
      return 0;
    }

    incoming_reset_timer(irq);

    if (status < 200) {
      queue = agent->sa_in.proceeding;

      if (irq->irq_method == sip_method_invite && status > 100 &&
	  agent->sa_progress != UINT_MAX && agent->sa_is_a_uas) {
	/* Retransmit preliminary responses in regular intervals */
	incoming_set_timer(irq, agent->sa_progress); /* N2 */
      }
    }
    else {
      irq->irq_completed = 1;

      /* XXX - we should do this only after message has actually been sent! */
      if (irq->irq_sigcomp_zap && irq->irq_cc)
	agent_close_compressor(irq->irq_agent, irq->irq_cc);

      if (irq->irq_method != sip_method_invite) {
	irq->irq_confirmed = 1;

	if (irq->irq_reliable_tp) {
	  irq->irq_terminated = 1;
	  queue = agent->sa_in.terminated ; /* J - set for 0 seconds */
	} else {
	  queue = agent->sa_in.completed; /* J */
	}

	tport_decref(&irq->irq_tport);
      }
      else if (status >= 300 || agent->sa_is_a_uas) {
	if (status < 300 || !irq->irq_reliable_tp)
	  incoming_set_timer(irq, agent->sa_t1); /* G */
	queue = agent->sa_in.inv_completed; /* H */
      }
      else {
#if 1
	/* Avoid bug in @RFC3261:
	  Keep INVITE transaction around in order to catch
	  retransmitted INVITEs
	*/
	irq->irq_confirmed = 1;
	queue = agent->sa_in.inv_confirmed; /* H */
#else
	irq->irq_terminated = 1;
	queue = agent->sa_in.terminated;
#endif
      }
    }

    if (irq->irq_queue != queue)
      incoming_queue(queue, irq);

    if (status >= 200 || irq->irq_status < 200) {
      if (irq->irq_response)
	msg_destroy(irq->irq_response);
      assert(msg_home(msg) != irq->irq_home);
      irq->irq_response = msg;
    }
    else {
      msg_destroy(msg);
    }

    if (sip->sip_cseq->cs_method == irq->irq_method &&
	irq->irq_status < 200 && status > irq->irq_status)
      irq->irq_status = status;

    return 0;
  }

  /*
   *  XXX - handling error is very problematic.
   * Nobody checks return code from nta_incoming_*reply()
   */
  if (status < 200) {
    msg_destroy(msg);
    return -1;
  }

  /* We could not send final response. */
  return incoming_final_failed(irq, msg);
}


/** @internal Sending final response has failed.
 *
 * Put transaction into its own queue, try later to send the response.
 */
su_inline
int incoming_final_failed(nta_incoming_t *irq, msg_t *msg)
{
  msg_destroy(msg);

  if (!irq->irq_default) {
    irq->irq_final_failed = 1;
    incoming_queue(irq->irq_agent->sa_in.final_failed, irq);
  }

  return -1;
}

/** @internal Retransmit the reply */
static
void incoming_retransmit_reply(nta_incoming_t *irq, tport_t *tport)
{
  msg_t *msg = NULL;

  if (irq->irq_final_failed)
    return;

  if (tport == NULL)
    tport = irq->irq_tport;

  /* Answer with existing reply */
  if (irq->irq_reliable && !irq->irq_reliable->rel_pracked)
    msg = reliable_response(irq);
  else
    msg = irq->irq_response;

  if (msg && tport) {
    irq->irq_retries++;

    if (irq->irq_retries == 2 && irq->irq_tpn->tpn_comp) {
      irq->irq_tpn->tpn_comp = NULL;

      if (irq->irq_cc) {
	agent_close_compressor(irq->irq_agent, irq->irq_cc);
	nta_compartment_decref(&irq->irq_cc);
      }
    }

    tport = tport_tsend(tport, msg, irq->irq_tpn,
			IF_SIGCOMP_TPTAG_COMPARTMENT(irq->irq_cc)
			TPTAG_MTU(INT_MAX), TAG_END());
    irq->irq_agent->sa_stats->as_sent_msg++;
    irq->irq_agent->sa_stats->as_sent_response++;
  }
}

/** @internal Create timestamp header for response */
static
int incoming_timestamp(nta_incoming_t *irq, msg_t *msg, sip_t *sip)
{
  sip_timestamp_t ts[1];
  su_time_t now = su_now();
  char delay[32];
  double diff = su_time_diff(now, irq->irq_received);

  snprintf(delay, sizeof delay, "%.06f", diff);

  *ts = *irq->irq_timestamp;
  ts->ts_delay = delay;

  return sip_add_dup(msg, sip, (sip_header_t *)ts);
}

enum {
  timer_max_retransmit = 30,
  timer_max_terminate = 100000,
  timer_max_timeout = 100
};

/** @internal Timer routine for the incoming request. */
static void
_nta_incoming_timer(nta_agent_t *sa)
{
  uint32_t now = sa->sa_millisec;
  nta_incoming_t *irq, *irq_next;
  size_t retransmitted = 0, timeout = 0, terminated = 0, destroyed = 0;
  size_t unconfirmed =
    sa->sa_in.inv_completed->q_length +
    sa->sa_in.preliminary->q_length;
  size_t unterminated =
    sa->sa_in.inv_confirmed->q_length +
    sa->sa_in.completed->q_length;
  size_t total = sa->sa_incoming->iht_used;

  incoming_queue_t rq[1];

  incoming_queue_init(rq, 0);

  /* Handle retry queue */
  while ((irq = sa->sa_in.re_list)) {
    if ((int32_t)(irq->irq_retry - now) > 0)
      break;
    if (retransmitted >= timer_max_retransmit)
      break;

    if (irq->irq_method == sip_method_invite && irq->irq_status >= 200) {
      /* Timer G */
      assert(irq->irq_queue == sa->sa_in.inv_completed);

      retransmitted++;

      SU_DEBUG_5(("nta: timer %s fired, retransmitting %u reply\n",
		  "G", irq->irq_status));

      incoming_retransmit_reply(irq, irq->irq_tport);

      if (2U * irq->irq_interval < sa->sa_t2)
	incoming_set_timer(irq, 2U * irq->irq_interval); /* G */
      else
	incoming_set_timer(irq, sa->sa_t2); /* G */
    }
    else if (irq->irq_method == sip_method_invite && irq->irq_status >= 100) {
      if (irq->irq_queue == sa->sa_in.preliminary) {
	/* Timer P1 - PRACK timer */
	retransmitted++;
	SU_DEBUG_5(("nta: timer %s fired, retransmitting %u reply\n",
		    "P1", irq->irq_status));

	incoming_retransmit_reply(irq, irq->irq_tport);

	incoming_set_timer(irq, 2 * irq->irq_interval); /* P1 */
      }
      else {
	/* Retransmitting provisional responses */
	SU_DEBUG_5(("nta: timer %s fired, retransmitting %u reply\n",
		    "N2", irq->irq_status));
	incoming_set_timer(irq, sa->sa_progress);
	retransmitted++;
	incoming_retransmit_reply(irq, irq->irq_tport);
      }
    }
    else {
      /* Timer N1 */
      incoming_reset_timer(irq);

      if(irq->irq_extra_100) {
        SU_DEBUG_5(("nta: timer N1 fired, sending %u %s\n", SIP_100_TRYING));
        nta_incoming_treply(irq, SIP_100_TRYING, TAG_END());
      }
      else {
        SU_DEBUG_5(("nta: timer N1 fired, but avoided sending %u %s\n",
                    SIP_100_TRYING));
      }
    }
  }

  while ((irq = sa->sa_in.final_failed->q_head)) {
    incoming_remove(irq);
    irq->irq_final_failed = 0;

    /* Report error to application */
    SU_DEBUG_5(("nta: sending final response failed, timeout %u response\n",
		irq->irq_status));
    reliable_timeout(irq, 0);

    nta_incoming_treply(irq, SIP_500_INTERNAL_SERVER_ERROR, TAG_END());

    if (!irq->irq_final_failed)	/* We have taken care of the error... */
      continue;

    if (irq->irq_destroyed) {
      incoming_free_queue(rq, irq);
      continue;
    }

    incoming_reset_timer(irq);
    irq->irq_confirmed = 1;
    irq->irq_terminated = 1;
    incoming_queue(sa->sa_in.terminated, irq);
  }

  /* Timeouts.
   * For each state the request is in, there is always a queue of its own
   */
  while ((irq = sa->sa_in.preliminary->q_head)) {
    assert(irq->irq_status < 200);
    assert(irq->irq_timeout);

    if ((int32_t)(irq->irq_timeout - now) > 0)
      break;
    if (timeout >= timer_max_timeout)
      break;

    timeout++;

    /* Timer P2 - PRACK timer */
    SU_DEBUG_5(("nta: timer %s fired, %s %u response\n",
		"P2", "timeout", irq->irq_status));
    incoming_reset_timer(irq);
    irq->irq_timeout = 0;
    reliable_timeout(irq, 1);
  }

  while ((irq = sa->sa_in.inv_completed->q_head)) {
    assert(irq->irq_status >= 200);
    assert(irq->irq_timeout);
    assert(irq->irq_method == sip_method_invite);

    if ((int32_t)(irq->irq_timeout - now) > 0 ||
	timeout >= timer_max_timeout ||
	terminated >= timer_max_terminate)
      break;

    /* Timer H */
    SU_DEBUG_5(("nta: timer %s fired, %s %u response\n",
		"H", "timeout and terminate", irq->irq_status));
    irq->irq_confirmed = 1;
    irq->irq_terminated = 1;
    incoming_reset_timer(irq);
    if (!irq->irq_destroyed) {
      timeout++;
      incoming_queue(sa->sa_in.terminated, irq);
      /* report timeout error to user */
      incoming_call_callback(irq, NULL, NULL);
    } else {
      timeout++;
      terminated++;
      incoming_free_queue(rq, irq);
    }
  }

  while ((irq = sa->sa_in.inv_confirmed->q_head)) {
    assert(irq->irq_timeout);
    assert(irq->irq_status >= 200);
    assert(irq->irq_method == sip_method_invite);

    if ((int32_t)(irq->irq_timeout - now) > 0 ||
	terminated >= timer_max_terminate)
      break;

    /* Timer I */
    SU_DEBUG_5(("nta: timer %s fired, %s %u response\n",
		"I", "terminate", irq->irq_status));

    terminated++;
    irq->irq_terminated = 1;

    if (!irq->irq_destroyed)
      incoming_queue(sa->sa_in.terminated, irq);
    else
      incoming_free_queue(rq, irq);
  }

  while ((irq = sa->sa_in.completed->q_head)) {
    assert(irq->irq_status >= 200);
    assert(irq->irq_timeout);
    assert(irq->irq_method != sip_method_invite);

    if ((int32_t)(irq->irq_timeout - now) > 0 ||
	terminated >= timer_max_terminate)
      break;

    /* Timer J */

    SU_DEBUG_5(("nta: timer %s fired, %s %u response\n",
		"J", "terminate", irq->irq_status));

    terminated++;
    irq->irq_terminated = 1;

    if (!irq->irq_destroyed)
      incoming_queue(sa->sa_in.terminated, irq);
    else
      incoming_free_queue(rq, irq);
  }

  for (irq = sa->sa_in.terminated->q_head; irq; irq = irq_next) {
    irq_next = irq->irq_next;
    if (irq->irq_destroyed)
      incoming_free_queue(rq, irq);
  }

  destroyed = incoming_mass_destroy(sa, rq);

  if (retransmitted || timeout || terminated || destroyed)
    SU_DEBUG_5(("nta_incoming_timer: "
		MOD_ZU"/"MOD_ZU" resent, "
		MOD_ZU"/"MOD_ZU" tout, "
		MOD_ZU"/"MOD_ZU" term, "
		MOD_ZU"/"MOD_ZU" free\n",
		retransmitted, unconfirmed,
		timeout, unconfirmed,
		terminated, unterminated,
		destroyed, total));
}

/** Mass destroy server transactions */
su_inline
size_t incoming_mass_destroy(nta_agent_t *sa, incoming_queue_t *q)
{
  size_t destroyed = q->q_length;

  if (destroyed > 2 && *sa->sa_terminator) {
    su_msg_r m = SU_MSG_R_INIT;

    if (su_msg_create(m,
		      su_clone_task(sa->sa_terminator),
		      su_root_task(sa->sa_root),
		      incoming_reclaim_queued,
		      sizeof(incoming_queue_t)) == SU_SUCCESS) {
      incoming_queue_t *mq = su_msg_data(m)->a_incoming_queue;

      *mq = *q;

      if (su_msg_send(m) == SU_SUCCESS)
	q->q_length = 0;
    }
  }

  if (q->q_length > 0)
    incoming_reclaim_queued(NULL, NULL, (void *)q);

  return destroyed;
}

/* ====================================================================== */
/* 8) Client-side (outgoing) transactions */

#define HTABLE_HASH_ORQ(orq) ((orq)->orq_hash)

HTABLE_BODIES_WITH(outgoing_htable, oht, nta_outgoing_t, HTABLE_HASH_ORQ,
		   size_t, hash_value_t);

static int outgoing_features(nta_agent_t *agent, nta_outgoing_t *orq,
			      msg_t *msg, sip_t *sip,
			      tagi_t *tags);
static void outgoing_prepare_send(nta_outgoing_t *orq);
static void outgoing_send_via(nta_outgoing_t *orq, tport_t *tp);
static void outgoing_send(nta_outgoing_t *orq, int retransmit);
static void outgoing_try_tcp_instead(nta_outgoing_t *orq);
static void outgoing_try_udp_instead(nta_outgoing_t *orq, int timeout);
static void outgoing_tport_error(nta_agent_t *agent, nta_outgoing_t *orq,
				 tport_t *tp, msg_t *msg, int error);
static void outgoing_print_tport_error(nta_outgoing_t *orq,
				       int level, char *todo,
				       tp_name_t const *, msg_t *, int error);
static void outgoing_insert(nta_agent_t *sa, nta_outgoing_t *orq);
static void outgoing_destroy(nta_outgoing_t *orq);
su_inline int outgoing_is_queued(nta_outgoing_t const *orq);
su_inline void outgoing_queue(outgoing_queue_t *queue,
			      nta_outgoing_t *orq);
su_inline void outgoing_remove(nta_outgoing_t *orq);
su_inline void outgoing_set_timer(nta_outgoing_t *orq, uint32_t interval);
static void outgoing_reset_timer(nta_outgoing_t *orq);
static size_t outgoing_timer_dk(outgoing_queue_t *q,
				char const *timer,
				uint32_t now);
static size_t outgoing_timer_bf(outgoing_queue_t *q,
				char const *timer,
				uint32_t now);
static size_t outgoing_timer_c(outgoing_queue_t *q,
			       char const *timer,
			       uint32_t now);

static void outgoing_ack(nta_outgoing_t *orq, sip_t *sip);
static msg_t *outgoing_ackmsg(nta_outgoing_t *, sip_method_t, char const *,
			      tag_type_t tag, tag_value_t value, ...);
static void outgoing_retransmit(nta_outgoing_t *orq);
static void outgoing_trying(nta_outgoing_t *orq);
static void outgoing_timeout(nta_outgoing_t *orq, uint32_t now);
static int outgoing_complete(nta_outgoing_t *orq);
static void outgoing_terminate_invite(nta_outgoing_t *);
static void outgoing_remove_fork(nta_outgoing_t *orq);
static int outgoing_terminate(nta_outgoing_t *orq);
static size_t outgoing_mass_destroy(nta_agent_t *sa, outgoing_queue_t *q);
static void outgoing_estimate_delay(nta_outgoing_t *orq, sip_t *sip);
static int outgoing_duplicate(nta_outgoing_t *orq,
			      msg_t *msg,
			      sip_t *sip);
static int outgoing_reply(nta_outgoing_t *orq,
			  int status, char const *phrase,
			  int delayed);

static int outgoing_default_cb(nta_outgoing_magic_t *magic,
			       nta_outgoing_t *request,
			       sip_t const *sip);


/** Create a default outgoing transaction.
 *
 * The default outgoing transaction is used when agent receives responses
 * not belonging to any transaction.
 *
 * @sa nta_leg_default(), nta_incoming_default().
 */
nta_outgoing_t *nta_outgoing_default(nta_agent_t *agent,
				     nta_response_f *callback,
				     nta_outgoing_magic_t *magic)
{
  nta_outgoing_t *orq;

  if (agent == NULL)
    return NULL;

  if (agent->sa_default_outgoing)
    return NULL;

  orq = su_zalloc(agent->sa_home, sizeof *orq);
  if (!orq)
    return NULL;

  orq->orq_agent     = agent;
  orq->orq_callback  = callback;
  orq->orq_magic     = magic;
  orq->orq_method    = sip_method_invalid;
  orq->orq_method_name = "*";
  orq->orq_default   = 1;
  orq->orq_stateless = 1;
  orq->orq_delay     = UINT_MAX;

  return agent->sa_default_outgoing = orq;
}

/**Create an outgoing request and client transaction belonging to the leg.
 *
 * Create a request message and pass the request message to an outgoing
 * client transaction object. The request is sent to the @a route_url (if
 * non-NULL), default proxy (if defined by NTATAG_DEFAULT_PROXY()), or to
 * the address specified by @a request_uri. If no @a request_uri is
 * specified, it is taken from route-set target or from the @To header.
 *
 * When NTA receives response to the request, it invokes the @a callback
 * function.
 *
 * @param leg         call leg object
 * @param callback    callback function (may be @c NULL)
 * @param magic       application context pointer
 * @param route_url   optional URL used to route transaction requests
 * @param method      method type
 * @param name        method name
 * @param request_uri Request-URI
 * @param tag, value, ... list of tagged arguments
 *
 * @return
 * A pointer to a newly created outgoing transaction object if successful,
 * and NULL otherwise.
 *
 * @note If NTATAG_STATELESS(1) tag is given and the @a callback is NULL,
 * the transaction object is marked as destroyed from the beginning. In that
 * case, the function may return @code (nta_outgoing_t *)-1 @endcode if the
 * transaction is freed before returning from the function.
 *
 * @sa
 * nta_outgoing_mcreate(), nta_outgoing_tcancel(), nta_outgoing_destroy().
 *
 * @TAGS
 * NTATAG_STATELESS(), NTATAG_DELAY_SENDING(), NTATAG_BRANCH_KEY(),
 * NTATAG_ACK_BRANCH(), NTATAG_DEFAULT_PROXY(), NTATAG_PASS_100(),
 * NTATAG_USE_TIMESTAMP(), NTATAG_USER_VIA(), TPTAG_IDENT(), NTATAG_TPORT(). All
 * SIP tags from <sofia-sip/sip_tag.h> can be used to manipulate the request message.
 * SIP tags after SIPTAG_END() are ignored, however.
 */
nta_outgoing_t *nta_outgoing_tcreate(nta_leg_t *leg,
				     nta_response_f *callback,
				     nta_outgoing_magic_t *magic,
				     url_string_t const *route_url,
				     sip_method_t method,
				     char const *name,
				     url_string_t const *request_uri,
				     tag_type_t tag, tag_value_t value, ...)
{
  nta_agent_t *agent;
  msg_t *msg;
  sip_t *sip;
  nta_outgoing_t *orq = NULL;
  ta_list ta;
  tagi_t const *tagi;

  if (leg == NULL)
    return NULL;

  agent = leg->leg_agent;
  msg = nta_msg_create(agent, 0);
  sip = sip_object(msg);

  if (route_url == NULL)
    route_url = (url_string_t *)agent->sa_default_proxy;

  ta_start(ta, tag, value);

  tagi = ta_args(ta);

  if (sip_add_tagis(msg, sip, &tagi) < 0) {
    if (tagi && tagi->t_tag) {
      tag_type_t t = tagi->t_tag;
      SU_DEBUG_5(("%s(): bad tag %s::%s\n", __func__,
		  t->tt_ns ? t->tt_ns : "", t->tt_name ? t->tt_name : ""));
    }
  }
  else if (route_url == NULL && leg->leg_route &&
	   leg->leg_loose_route &&
	   !(route_url = (url_string_t *)leg->leg_route->r_url))
    ;
  else if (nta_msg_request_complete(msg, leg, method, name, request_uri) < 0)
    ;
  else
    orq = outgoing_create(agent, callback, magic, route_url, NULL, msg,
			  ta_tags(ta));

  ta_end(ta);

  if (!orq)
    msg_destroy(msg);

  return orq;
}

/**Create an outgoing client transaction.
 *
 * Create an outgoing transaction object. The request message is passed to
 * the transaction object, which sends the request to the network. The
 * request is sent to the @a route_url (if non-NULL), default proxy (if
 * defined by NTATAG_DEFAULT_PROXY()), or to the address specified by @a
 * request_uri. If no @a request_uri is specified, it is taken from
 * route-set target or from the @To header.
 *
 * When NTA receives response to the request, it invokes the @a callback
 * function.
 *
 * @param agent       NTA agent object
 * @param callback    callback function (may be @c NULL)
 * @param magic       application context pointer
 * @param route_url   optional URL used to route transaction requests
 * @param msg         request message
 * @param tag, value, ... tagged parameter list
 *
 * @return
 * Returns a pointer to newly created outgoing transaction object if
 * successful, and NULL otherwise.
 *
 * @note The caller is responsible for destroying the request message @a msg
 * upon failure.
 *
 * @note If NTATAG_STATELESS(1) tag is given and the @a callback is NULL,
 * the transaction object is marked as destroyed from the beginning. In that
 * case, the function may return @code (nta_outgoing_t *)-1 @endcode if the
 * transaction is freed before returning from the function.
 *
 * @sa
 * nta_outgoing_tcreate(), nta_outgoing_tcancel(), nta_outgoing_destroy().
 *
 * @TAGS
 * NTATAG_STATELESS(), NTATAG_DELAY_SENDING(), NTATAG_BRANCH_KEY(),
 * NTATAG_ACK_BRANCH(), NTATAG_DEFAULT_PROXY(), NTATAG_PASS_100(),
 * NTATAG_USE_TIMESTAMP(), NTATAG_USER_VIA(), TPTAG_IDENT(), NTATAG_TPORT(). All
 * SIP tags from <sofia-sip/sip_tag.h> can be used to manipulate the request message.
 * SIP tags after SIPTAG_END() are ignored, however.
 */
nta_outgoing_t *nta_outgoing_mcreate(nta_agent_t *agent,
				     nta_response_f *callback,
				     nta_outgoing_magic_t *magic,
				     url_string_t const *route_url,
				     msg_t *msg,
				     tag_type_t tag, tag_value_t value, ...)
{
  nta_outgoing_t *orq = NULL;
  int cleanup = 0;

  if (msg == NONE)
    msg = nta_msg_create(agent, 0), cleanup = 1;

  if (msg && agent) {
    ta_list ta;
    ta_start(ta, tag, value);
    if (sip_add_tl(msg, sip_object(msg), ta_tags(ta)) >= 0)
      orq = outgoing_create(agent, callback, magic, route_url, NULL, msg,
			    ta_tags(ta));
    ta_end(ta);
  }

  if (!orq && cleanup)
    msg_destroy(msg);

  return orq;
}

/** Cancel the request. */
int nta_outgoing_cancel(nta_outgoing_t *orq)
{
  nta_outgoing_t *cancel =
    nta_outgoing_tcancel(orq, NULL, NULL, TAG_NULL());

  return (cancel != NULL) - 1;
}

/** Cancel the request.
 *
 * Initiate a cancel transaction for client transaction @a orq.
 *
 * @param orq      client transaction to cancel
 * @param callback    callback function (may be @c NULL)
 * @param magic       application context pointer
 * @param tag, value, ... list of extra arguments
 *
 * @note The function may return @code (nta_outgoing_t *)-1 @endcode (NONE)
 * if callback is NULL.
 *
 * @TAGS
 * NTATAG_CANCEL_2534(), NTATAG_CANCEL_408() and all the tags that are
 * accepted by nta_outgoing_tcreate().
 *
 * If NTATAG_CANCEL_408(1) or NTATAG_CANCEL_2543(1) is given, the stack
 * generates a 487 response to the request internally. If
 * NTATAG_CANCEL_408(1) is given, no CANCEL request is actually sent.
 *
 * @note
 * nta_outgoing_tcancel() refuses to send a CANCEL request for non-INVITE
 * requests.
 */
nta_outgoing_t *nta_outgoing_tcancel(nta_outgoing_t *orq,
				     nta_response_f *callback,
				     nta_outgoing_magic_t *magic,
				     tag_type_t tag, tag_value_t value, ...)
{
  msg_t *msg;
  int cancel_2543, cancel_408;
  ta_list ta;
  int delay_sending;

  if (orq == NULL || orq == NONE)
    return NULL;

  if (orq->orq_destroyed) {
    SU_DEBUG_3(("%s: trying to cancel destroyed request\n", __func__));
    return NULL;
  }
  if (orq->orq_method != sip_method_invite) {
    SU_DEBUG_3(("%s: trying to cancel non-INVITE request\n", __func__));
    return NULL;
  }

  if (orq->orq_forking)
    orq = orq->orq_forking;

  if (orq->orq_status >= 200
      /* && orq->orq_method != sip_method_invite ... !multicast */) {
    SU_DEBUG_3(("%s: trying to cancel completed request\n", __func__));
    return NULL;
  }
  if (orq->orq_canceled) {
    SU_DEBUG_3(("%s: trying to cancel cancelled request\n", __func__));
    return NULL;
  }
  orq->orq_canceled = 1;

#if HAVE_SOFIA_SRESOLV
  if (!orq->orq_resolved) {
    outgoing_destroy_resolver(orq);
    outgoing_reply(orq, SIP_487_REQUEST_CANCELLED, 1);
    return NULL;		/* XXX - Does anyone care about reply? */
  }
#endif

  cancel_408 = 0;		/* Don't really CANCEL, this is timeout. */
  cancel_2543 = orq->orq_agent->sa_cancel_2543;
  /* CANCEL may be sent only after a provisional response has been received. */
  delay_sending = orq->orq_status < 100;

  ta_start(ta, tag, value);

  tl_gets(ta_args(ta),
	  NTATAG_CANCEL_408_REF(cancel_408),
	  NTATAG_CANCEL_2543_REF(cancel_2543),
	  TAG_END());

  if (!cancel_408)
    msg = outgoing_ackmsg(orq, SIP_METHOD_CANCEL, ta_tags(ta));
  else
    msg = NULL;

  ta_end(ta);

  if ((cancel_2543 || cancel_408) && !orq->orq_stateless)
    outgoing_reply(orq, SIP_487_REQUEST_CANCELLED, 1);

  if (msg) {
    nta_outgoing_t *cancel;
    if (cancel_2543)		/* Follow RFC 2543 semantics for CANCEL */
      delay_sending = 0;

    cancel = outgoing_create(orq->orq_agent, callback, magic,
			     NULL, orq->orq_tpn, msg,
			     NTATAG_BRANCH_KEY(orq->orq_branch),
			     NTATAG_DELAY_SENDING(delay_sending),
			     NTATAG_USER_VIA(1),
			     TAG_END());

    if (delay_sending)
      orq->orq_cancel = cancel;

    if (cancel) {
      if (!delay_sending)
	outgoing_complete(orq);
      return cancel;
    }

    msg_destroy(msg);
  }

  return NULL;
}

/**Bind callback and application context to a client transaction.
 *
 * @param orq       outgoing client transaction
 * @param callback  callback function (may be NULL)
 * @param magic     application context pointer
 *                  (given as argument to @a callback)
 *
 * @NEW_1_12_9
 */
int
nta_outgoing_bind(nta_outgoing_t *orq,
		  nta_response_f *callback,
		  nta_outgoing_magic_t *magic)
{
  if (orq && !orq->orq_destroyed) {
    if (callback == NULL)
      callback = outgoing_default_cb;
    orq->orq_callback = callback;
    orq->orq_magic = magic;
    return 0;
  }
  return -1;
}

/**Get application context bound to a client transaction.
 *
 * @param orq       outgoing client transaction
 * @param callback  callback function (may be NULL)
 *
 * Return the application context bound to a client transaction. If the @a
 * callback function pointer is given, return application context only if
 * the callback matches with the callback bound to the client transaction.
 *
 * @NEW_1_12_11
 */
nta_outgoing_magic_t *
nta_outgoing_magic(nta_outgoing_t const *orq,
		   nta_response_f *callback)
{
  if (orq && (callback == NULL || callback == orq->orq_callback))
    return orq->orq_magic;
  else
    return NULL;
}


/**
 * Destroy a request object.
 *
 * @note
 * This function does not actually free the object, but marks it as
 * disposable. The object is freed after a timeout.
 */
void nta_outgoing_destroy(nta_outgoing_t *orq)
{
  if (orq == NULL || orq == NONE)
    return;

  if (orq->orq_destroyed) {
    SU_DEBUG_1(("%s(%p): %s\n", "nta_outgoing_destroy", (void *)orq,
		"already destroyed"));
    return;
  }

  outgoing_destroy(orq);
}

/** Return the request URI */
url_t const *nta_outgoing_request_uri(nta_outgoing_t const *orq)
{
  return orq != NULL && orq != NONE ? orq->orq_url : NULL;
}

/** Return the URI used to route the request */
url_t const *nta_outgoing_route_uri(nta_outgoing_t const *orq)
{
  return orq != NULL && orq != NONE ? orq->orq_route : NULL;
}

/** Return method of the client transaction */
sip_method_t nta_outgoing_method(nta_outgoing_t const *orq)
{
  return orq != NULL && orq != NONE ? orq->orq_method : sip_method_invalid;
}

/** Return method name of the client transaction */
char const *nta_outgoing_method_name(nta_outgoing_t const *orq)
{
  return orq != NULL && orq != NONE ? orq->orq_method_name : NULL;
}

/** Get sequence number of a client transaction.
 */
uint32_t nta_outgoing_cseq(nta_outgoing_t const *orq)
{
  return orq != NULL && orq != NONE && orq->orq_cseq
    ? orq->orq_cseq->cs_seq : 0;
}

/**
 * Get the status code of a client transaction.
 */
int nta_outgoing_status(nta_outgoing_t const *orq)
{
  /* Return 500 Internal server error for invalid handles. */
  return orq != NULL && orq != NONE ? orq->orq_status : 500;
}

/** Get the RTT delay measured using @Timestamp header. */
unsigned nta_outgoing_delay(nta_outgoing_t const *orq)
{
  return orq != NULL && orq != NONE ? orq->orq_delay : UINT_MAX;
}

/** Get the branch parameter. @NEW_1_12_7. */
char const *nta_outgoing_branch(nta_outgoing_t const *orq)
{
  return orq != NULL && orq != NONE && orq->orq_branch
    ? orq->orq_branch + strlen("branch=")
    : NULL;
}

/**Get reference to response message.
 *
 * Retrieve the latest incoming response message to the outgoing
 * transaction. Note that the message is not copied, but a new reference to
 * it is created instead.
 *
 * @param orq outgoing transaction handle
 *
 * @retval
 * A pointer to response message is returned, or NULL if no response message
 * has been received.
 */
msg_t *nta_outgoing_getresponse(nta_outgoing_t *orq)
{
  if (orq != NULL && orq != NONE)
    return msg_ref_create(orq->orq_response);
  else
    return NULL;
}

/**Get request message.
 *
 * Retrieves the request message sent to the network. Note that the request
 * message is @b not copied, but a new reference to it is created.
 *
 * @retval
 * A pointer to the request message is returned, or NULL if an error
 * occurred.
 */
msg_t *nta_outgoing_getrequest(nta_outgoing_t *orq)
{
  if (orq != NULL && orq != NONE)
    return msg_ref_create(orq->orq_request);
  else
    return NULL;
}

/**Create an outgoing request.
 *
 * Create an outgoing transaction object and send the request to the
 * network. The request is sent to the @a route_url (if non-NULL), default
 * proxy (if defined by NTATAG_DEFAULT_PROXY()), or to the address specified
 * by @a sip->sip_request->rq_url.
 *
 * When NTA receives response to the request, it invokes the @a callback
 * function.
 *
 * @param agent       nta agent object
 * @param callback    callback function (may be @c NULL)
 * @param magic       application context pointer
 * @param route_url   optional URL used to route transaction requests
 * @param msg         request message
 * @param tpn         (optional) transport name
 * @param msg         request message to
 * @param tag, value, ... tagged arguments
 *
 * @return
 * Returns a pointer to newly created outgoing transaction object if
 * successful, and NULL otherwise.
 *
 * @note If NTATAG_STATELESS(1) tag is given and the @a callback is NULL,
 * the transaction object is marked as destroyed from the beginning. In that
 * case, the function may return @code (nta_outgoing_t *)-1 @endcode if the
 * transaction is freed before returning from the function.
 *
 * @TAG NTATAG_TPORT must point to an existing transport object for
 *      'agent' (the passed tport is otherwise ignored).
 *
 * @sa
 * nta_outgoing_tcreate(), nta_outgoing_tcancel(), nta_outgoing_destroy().
 */
nta_outgoing_t *outgoing_create(nta_agent_t *agent,
				nta_response_f *callback,
				nta_outgoing_magic_t *magic,
				url_string_t const *route_url,
				tp_name_t const *tpn,
				msg_t *msg,
				tag_type_t tag, tag_value_t value, ...)
{
  nta_outgoing_t *orq;
  sip_t *sip;
  su_home_t *home;
  char const *comp = NONE;
  char const *branch = NONE;
  char const *ack_branch = NONE;
  char const *tp_ident;
  int delay_sending = 0, sigcomp_zap = 0;
  int pass_100 = agent->sa_pass_100, use_timestamp = agent->sa_timestamp;
  enum nta_res_order_e res_order = agent->sa_res_order;
  struct sigcomp_compartment *cc = NULL;
  ta_list ta;
  char const *scheme = NULL;
  char const *port = NULL;
  int invalid, resolved = 0, stateless = 0, user_via = agent->sa_user_via;
  int invite_100rel = agent->sa_invite_100rel;
  int explicit_transport = 1;

  tagi_t const *t;
  tport_t *override_tport = NULL;

  if (!agent->sa_tport_ip6)
    res_order = nta_res_ip4_only;
  else if (!agent->sa_tport_ip4)
    res_order = nta_res_ip6_only;

  if (!callback)
    callback = outgoing_default_cb;
  if (!route_url)
    route_url = (url_string_t *)agent->sa_default_proxy;

  sip = sip_object(msg);
  home = msg_home(msg);

  if (!sip->sip_request || sip_complete_message(msg) < 0) {
    SU_DEBUG_3(("nta: outgoing_create: incomplete request\n" VA_NONE));
    return NULL;
  }

  if (!route_url && !tpn && sip->sip_route &&
      sip->sip_route->r_url->url_params &&
      url_param(sip->sip_route->r_url->url_params, "lr", NULL, 0))
    route_url = (url_string_t *)sip->sip_route->r_url;

  if (!(orq = su_zalloc(agent->sa_home, sizeof(*orq))))
    return NULL;

  tp_ident = tpn ? tpn->tpn_ident : NULL;

  ta_start(ta, tag, value);

  /* tl_gets() is a bit too slow here... */
  for (t = ta_args(ta); t; t = tl_next(t)) {
    tag_type_t tt = t->t_tag;

    if (ntatag_stateless == tt)
      stateless = t->t_value != 0;
    else if (ntatag_delay_sending == tt)
      delay_sending = t->t_value != 0;
    else if (ntatag_branch_key == tt)
      branch = (void *)t->t_value;
    else if (ntatag_pass_100 == tt)
      pass_100 = t->t_value != 0;
    else if (ntatag_use_timestamp == tt)
      use_timestamp = t->t_value != 0;
    else if (ntatag_user_via == tt)
      user_via = t->t_value != 0;
    else if (ntatag_ack_branch == tt)
      ack_branch = (void *)t->t_value;
    else if (ntatag_default_proxy == tt)
      route_url = (void *)t->t_value;
    else if (tptag_ident == tt)
      tp_ident = (void *)t->t_value;
    else if (ntatag_comp == tt)
      comp = (char const *)t->t_value;
    else if (ntatag_sigcomp_close == tt)
      sigcomp_zap = t->t_value != 0;
    else if (tptag_compartment == tt)
      cc = (void *)t->t_value;
    else if (ntatag_tport == tt) {
      override_tport = (tport_t *)t->t_value;
    }
    else if (ntatag_rel100 == tt) {
      invite_100rel = t->t_value != 0;
    }
  }

  orq->orq_agent    = agent;
  orq->orq_callback = callback;
  orq->orq_magic    = magic;
  orq->orq_method   = sip->sip_request->rq_method;
  orq->orq_method_name = sip->sip_request->rq_method_name;
  orq->orq_cseq     = sip->sip_cseq;
  orq->orq_to       = sip->sip_to;
  orq->orq_from     = sip->sip_from;
  orq->orq_call_id  = sip->sip_call_id;
  orq->orq_tags     = tl_afilter(home, tport_tags, ta_args(ta));
  orq->orq_delayed  = delay_sending != 0;
  orq->orq_pass_100 = pass_100 != 0;
  orq->orq_sigcomp_zap = sigcomp_zap;
  orq->orq_sigcomp_new = comp != NONE && comp != NULL;
  orq->orq_timestamp = use_timestamp;
  orq->orq_delay     = UINT_MAX;
  orq->orq_stateless = stateless != 0;
  orq->orq_user_via  = user_via != 0 && sip->sip_via;
  orq->orq_100rel    = invite_100rel;
  orq->orq_uas       = !stateless && agent->sa_is_a_uas;

  if (cc)
    orq->orq_cc = nta_compartment_ref(cc);

  /* Add supported features */
  outgoing_features(agent, orq, msg, sip, ta_args(ta));

  ta_end(ta);

  /* select the tport to use for the outgoing message  */
  if (override_tport) {
    /* note: no ref taken to the tport as its only used once here */
    if (tport_is_secondary(override_tport)) {
      tpn = tport_name(override_tport);
      orq->orq_user_tport = 1;
    }
  }

  if (tpn) {
    /* CANCEL or ACK to [3456]XX */
    invalid = tport_name_dup(home, orq->orq_tpn, tpn);
#if 0 //HAVE_SOFIA_SRESOLV
    /* We send ACK or CANCEL only if original request was really sent */
    assert(tport_name_is_resolved(orq->orq_tpn));
#endif
    resolved = tport_name_is_resolved(orq->orq_tpn);
    orq->orq_url = url_hdup(home, sip->sip_request->rq_url);
  }
  else if (route_url && !orq->orq_user_tport) {
    invalid = nta_tpn_by_url(home, orq->orq_tpn, &scheme, &port, route_url);
    if (invalid >= 0) {
      explicit_transport = invalid > 0;
      if (override_tport) {	/* Use transport protocol name from transport  */
	if (strcmp(orq->orq_tpn->tpn_proto, "*") == 0)
	  orq->orq_tpn->tpn_proto = tport_name(override_tport)->tpn_proto;
      }

      resolved = tport_name_is_resolved(orq->orq_tpn);
      orq->orq_url = url_hdup(home, sip->sip_request->rq_url);
      if (route_url != (url_string_t *)agent->sa_default_proxy)
	orq->orq_route = url_hdup(home, route_url->us_url);
    }
  }
  else {
    invalid = nta_tpn_by_url(home, orq->orq_tpn, &scheme, &port,
			     (url_string_t *)sip->sip_request->rq_url);
    if (invalid >= 0) {
      explicit_transport = invalid > 0;
      resolved = tport_name_is_resolved(orq->orq_tpn);
      sip_fragment_clear(sip->sip_request->rq_common);
    }
    orq->orq_url = url_hdup(home, sip->sip_request->rq_url);
  }

  if (!override_tport)
    orq->orq_tpn->tpn_ident = tp_ident;
  else
    orq->orq_tpn->tpn_ident = tport_name(override_tport)->tpn_ident;

  if (comp == NULL)
    orq->orq_tpn->tpn_comp = comp;

  if (orq->orq_user_via && su_strmatch(orq->orq_tpn->tpn_proto, "*")) {
    char const *proto = sip_via_transport(sip->sip_via);
    if (proto) orq->orq_tpn->tpn_proto = proto;
  }

  if (branch && branch != NONE) {
    if (su_casenmatch(branch, "branch=", 7))
      branch = su_strdup(home, branch);
    else
      branch = su_sprintf(home, "branch=%s", branch);
  }
  else if (orq->orq_user_via && sip->sip_via->v_branch && orq->orq_method != sip_method_invite )
    branch = su_sprintf(home, "branch=%s", sip->sip_via->v_branch);
  else if (stateless)
    branch = stateless_branch(agent, msg, sip, orq->orq_tpn);
  else
    branch = stateful_branch(home, agent);

  orq->orq_branch = branch;
  orq->orq_via_branch = branch;

  if (orq->orq_method == sip_method_ack) {
    /* Find the original INVITE which we are ACKing */
    if (ack_branch != NULL && ack_branch != NONE) {
      if (su_casenmatch(ack_branch, "branch=", 7))
	orq->orq_branch = su_strdup(home, ack_branch);
      else
	orq->orq_branch = su_sprintf(home, "branch=%s", ack_branch);
    }
    else if (orq->orq_uas) {
      /*
       * ACK redirects further 2XX messages to it.
       *
       * Use orq_branch from INVITE, but put a different branch in topmost Via.
       */
      nta_outgoing_t *invite = outgoing_find(agent, msg, sip, NULL);

      if (invite) {
	sip_t const *inv = sip_object(invite->orq_request);

	orq->orq_branch = su_strdup(home, invite->orq_branch);

	/* @RFC3261 section 13.2.2.4 -
	 * The ACK MUST contain the same credentials as the INVITE.
	 */
	if (!sip->sip_proxy_authorization && !sip->sip_authorization) {
	  if (inv->sip_proxy_authorization)
	    sip_add_dup(msg, sip, (void *)inv->sip_proxy_authorization);
	  if (inv->sip_authorization)
	    sip_add_dup(msg, sip, (void *)inv->sip_authorization);
	}
      }
      else {
	SU_DEBUG_1(("outgoing_create: ACK without INVITE\n" VA_NONE));
	assert(!"INVITE found for ACK");
      }
    }
  }

#if HAVE_SOFIA_SRESOLV
  if (!resolved)
    orq->orq_tpn->tpn_port = port;
  orq->orq_resolved = resolved;
#else
  orq->orq_resolved = resolved = 1;
#endif
  orq->orq_sips = su_casematch(scheme, "sips");

  if (invalid < 0 || !orq->orq_branch || msg_serialize(msg, (void *)sip) < 0) {
    SU_DEBUG_3(("nta outgoing create: %s\n",
		invalid < 0 ? "invalid URI" :
		!orq->orq_branch ? "no branch" : "invalid message"));
    outgoing_free(orq);
    return NULL;
  }

  /* Now we are committed in sending the transaction */
  orq->orq_request = msg;
  agent->sa_stats->as_client_tr++;
  orq->orq_hash = NTA_HASH(sip->sip_call_id, sip->sip_cseq->cs_seq);

  if (orq->orq_user_tport)
    outgoing_send_via(orq, override_tport);
  else if (resolved)
    outgoing_prepare_send(orq);
#if HAVE_SOFIA_SRESOLV
  else
    outgoing_resolve(orq, explicit_transport, res_order);
#endif

  if (stateless &&
      orq->orq_status >= 200 &&
      callback == outgoing_default_cb) {
    void *retval;

    if (orq->orq_status < 300)
      retval = (void *)-1;	/* NONE */
    else
      retval = NULL, orq->orq_request = NULL;

    outgoing_free(orq);

    return retval;
  }

  assert(orq->orq_queue);

  outgoing_insert(agent, orq);

  return orq;
}

/** Prepare sending a request */
static void
outgoing_prepare_send(nta_outgoing_t *orq)
{
  nta_agent_t *sa = orq->orq_agent;
  tport_t *tp;
  tp_name_t *tpn = orq->orq_tpn;

  /* Select transport by scheme */
  if (orq->orq_sips && strcmp(tpn->tpn_proto, "*") == 0)
    tpn->tpn_proto = "tls";

  if (!tpn->tpn_port)
    tpn->tpn_port = "";

  tp = tport_by_name(sa->sa_tports, tpn);

  if (tpn->tpn_port[0] == '\0') {
    if (orq->orq_sips || tport_has_tls(tp))
      tpn->tpn_port = "5061";
    else
      tpn->tpn_port = "5060";
  }

  if (tp) {
    outgoing_send_via(orq, tp);
  }
  else if (orq->orq_sips) {
    SU_DEBUG_3(("nta outgoing create: no secure transport\n" VA_NONE));
    outgoing_reply(orq, SIP_416_UNSUPPORTED_URI, 1);
  }
  else {
    SU_DEBUG_3(("nta outgoing create: no transport protocol\n" VA_NONE));
    outgoing_reply(orq, 503, "No transport", 1);
  }
}

/** Send request using given transport */
static void
outgoing_send_via(nta_outgoing_t *orq, tport_t *tp)
{
  tport_t *old_tp = orq->orq_tport;

  orq->orq_tport = tport_ref(tp);

  if (orq->orq_pending && tp != old_tp) {
    tport_release(old_tp, orq->orq_pending,
		  orq->orq_request, NULL, orq, 0);
    orq->orq_pending = 0;
  }

  if (old_tp) tport_unref(old_tp);

  if (outgoing_insert_via(orq, agent_tport_via(tp)) < 0) {
    SU_DEBUG_3(("nta outgoing create: cannot insert Via line\n" VA_NONE));
    outgoing_reply(orq, 503, "Cannot insert Via", 1);
    return;
  }

#if HAVE_SOFIA_SMIME
  {
    sm_object_t *smime = sa->sa_smime;
    sip_t *sip = sip_object(orq->orq_request);

    if (sa->sa_smime &&
	(sip->sip_request->rq_method == sip_method_invite ||
	 sip->sip_request->rq_method == sip_method_message)) {
      msg_prepare(orq->orq_request);
      if (sm_encode_message(smime, msg, sip, SM_ID_NULL) < 0) {
	outgoing_tport_error(sa, orq, NULL,
			     orq->orq_request, su_errno());
	return;
      }
    }
  }
#endif

  orq->orq_prepared = 1;

  if (orq->orq_delayed) {
    SU_DEBUG_5(("nta: delayed sending %s (%u)\n",
		orq->orq_method_name, orq->orq_cseq->cs_seq));
    outgoing_queue(orq->orq_agent->sa_out.delayed, orq);
    return;
  }

  outgoing_send(orq, 0);
}


/** Send a request */
static void
outgoing_send(nta_outgoing_t *orq, int retransmit)
{
  int err;
  tp_name_t const *tpn = orq->orq_tpn;
  msg_t *msg = orq->orq_request;
  nta_agent_t *agent = orq->orq_agent;
  tport_t *tp;
  int once = 0;
  su_time_t now = su_now();
  tag_type_t tag = tag_skip;
  tag_value_t value = 0;
  struct sigcomp_compartment *cc; cc = NULL;

  /* tport can be NULL if we are just switching network */
  if (orq->orq_tport == NULL) {
    outgoing_tport_error(agent, orq, NULL, orq->orq_request, ENETRESET);
    return;
  }

  if (orq->orq_user_tport && !tport_is_clear_to_send(orq->orq_tport)) {
    outgoing_tport_error(agent, orq, NULL, orq->orq_request, EPIPE);
    return;
  }

  if (!retransmit)
    orq->orq_sent = now;

  if (orq->orq_timestamp) {
    sip_t *sip = sip_object(msg);
    sip_timestamp_t *ts =
      sip_timestamp_format(msg_home(msg), "%lu.%06lu",
			   now.tv_sec, now.tv_usec);

    if (ts) {
      if (sip->sip_timestamp)
	msg_header_remove(msg, (msg_pub_t *)sip, (msg_header_t *)sip->sip_timestamp);
      msg_header_insert(msg, (msg_pub_t *)sip, (msg_header_t *)ts);
    }
  }

  for (;;) {
    if (tpn->tpn_comp == NULL) {
      /* xyzzy */
    }
    else if (orq->orq_cc) {
      cc = orq->orq_cc, orq->orq_cc = NULL;
    }
    else {
      cc = agent_compression_compartment(agent, orq->orq_tport, tpn,
					 orq->orq_sigcomp_new);
    }

    if (orq->orq_try_udp_instead)
      tag = tptag_mtu, value = 65535;

    if (orq->orq_pending) {
      tport_release(orq->orq_tport, orq->orq_pending,
		    orq->orq_request, NULL, orq, 0);
      orq->orq_pending = 0;
    }

    tp = tport_tsend(orq->orq_tport, msg, tpn,
		     tag, value,
		     IF_SIGCOMP_TPTAG_COMPARTMENT(cc)
		     TAG_NEXT(orq->orq_tags));
    if (tp)
      break;

    err = msg_errno(orq->orq_request);

    if (cc)
      nta_compartment_decref(&cc);

    if (orq->orq_user_tport)
      /* No retries */;
    /* RFC3261, 18.1.1 */
    else if (err == EMSGSIZE && !orq->orq_try_tcp_instead) {
      if (su_casematch(tpn->tpn_proto, "udp") ||
	  su_casematch(tpn->tpn_proto, "*")) {
	outgoing_try_tcp_instead(orq);
	continue;
      }
    }
    else if (err == ECONNREFUSED && orq->orq_try_tcp_instead) {
      if (su_casematch(tpn->tpn_proto, "tcp") && msg_size(msg) <= 65535) {
	outgoing_try_udp_instead(orq, 0);
	continue;
      }
    }
    else if (err == EPIPE) {
      /* Connection was closed */
      if (!once++) {
	orq->orq_retries++;
	continue;
      }
    }

    outgoing_tport_error(agent, orq, NULL, orq->orq_request, err);

    return;
  }

  agent->sa_stats->as_sent_msg++;
  agent->sa_stats->as_sent_request++;
  if (retransmit)
    agent->sa_stats->as_retry_request++;

  SU_DEBUG_5(("nta: %ssent %s (%u) to " TPN_FORMAT "\n",
	      retransmit ? "re" : "",
	      orq->orq_method_name, orq->orq_cseq->cs_seq,
	      TPN_ARGS(tpn)));

  if (cc) {
    if (orq->orq_cc)
      nta_compartment_decref(&orq->orq_cc);
  }

  if (orq->orq_pending) {
    assert(orq->orq_tport);
    tport_release(orq->orq_tport, orq->orq_pending,
		  orq->orq_request, NULL, orq, 0);
    orq->orq_pending = 0;
  }

  if (orq->orq_stateless) {
    outgoing_reply(orq, 202, NULL, 202);
    return;
  }

  if (orq->orq_method != sip_method_ack) {
    orq->orq_pending = tport_pend(tp, orq->orq_request,
				  outgoing_tport_error, orq);
    if (orq->orq_pending < 0)
      orq->orq_pending = 0;
  }

  if (tp != orq->orq_tport) {
    tport_decref(&orq->orq_tport);
    orq->orq_tport = tport_ref(tp);
  }

  orq->orq_reliable = tport_is_reliable(tp);

  if (retransmit)
    return;

  outgoing_trying(orq);		/* Timer B / F */

  if (orq->orq_method == sip_method_ack)
    ;
  else if (!orq->orq_reliable) {
    /* race condition on initial t1 timer timeout, set minimum initial timeout to 1000ms */
	unsigned t1_timer = agent->sa_t1;
	if (t1_timer < 1000) t1_timer = 1000;
    outgoing_set_timer(orq, t1_timer); /* Timer A/E */
  } else if (orq->orq_try_tcp_instead && !tport_is_connected(tp))
    outgoing_set_timer(orq, agent->sa_t4); /* Timer N3 */
}

static void
outgoing_try_tcp_instead(nta_outgoing_t *orq)
{
  tport_t *tp;
  tp_name_t tpn[1];

  assert(orq->orq_pending == 0);

  *tpn = *orq->orq_tpn;
  tpn->tpn_proto = "tcp";
  orq->orq_try_tcp_instead = 1;

  tp = tport_by_name(orq->orq_agent->sa_tports, tpn);
  if (tp && tp != orq->orq_tport) {
    sip_t *sip = sip_object(orq->orq_request);
    sip_fragment_clear(sip->sip_via->v_common);
    sip->sip_via->v_protocol = sip_transport_tcp;

    SU_DEBUG_5(("nta: %s (%u) too large for UDP, trying TCP\n",
		orq->orq_method_name, orq->orq_cseq->cs_seq));

    orq->orq_tpn->tpn_proto = "tcp";
    tport_decref(&orq->orq_tport);
    orq->orq_tport = tport_ref(tp);

    return;
  }

  /* No TCP - try again with UDP without SIP MTU limit */
  tpn->tpn_proto = "udp";
  orq->orq_try_udp_instead = 1;

  tp = tport_by_name(orq->orq_agent->sa_tports, tpn);
  if (tp && tp != orq->orq_tport) {
    SU_DEBUG_5(("nta: %s (%u) exceed normal UDP size limit\n",
		orq->orq_method_name, orq->orq_cseq->cs_seq));

    tport_decref(&orq->orq_tport);
    orq->orq_tport = tport_ref(tp);
  }
}

static void
outgoing_try_udp_instead(nta_outgoing_t *orq, int timeout)
{
  tport_t *tp;
  tp_name_t tpn[1];

  if (orq->orq_pending) {
    tport_release(orq->orq_tport, orq->orq_pending,
		  orq->orq_request, NULL, orq, 0);
    orq->orq_pending = 0;
  }

  *tpn = *orq->orq_tpn;
  tpn->tpn_proto = "udp";
  orq->orq_try_udp_instead = 1;

  tp = tport_by_name(orq->orq_agent->sa_tports, tpn);
  if (tp && tp != orq->orq_tport) {
    sip_t *sip = sip_object(orq->orq_request);

    sip_fragment_clear(sip->sip_via->v_common);
    sip->sip_via->v_protocol = sip_transport_udp;

    SU_DEBUG_5(("nta: %s (%u) TCP %s, trying UDP\n",
		orq->orq_method_name, orq->orq_cseq->cs_seq,
		timeout ? "times out" : "refused"));

    orq->orq_tpn->tpn_proto = "udp";
    tport_decref(&orq->orq_tport);
    orq->orq_tport = tport_ref(tp);
  }
}


/** @internal Report transport errors. */
void
outgoing_tport_error(nta_agent_t *agent, nta_outgoing_t *orq,
		     tport_t *tp, msg_t *msg, int error)
{
  tp_name_t const *tpn = tp ? tport_name(tp) : orq->orq_tpn;

  if (orq->orq_pending) {
    assert(orq->orq_tport);
    tport_release(orq->orq_tport, orq->orq_pending, orq->orq_request,
		  NULL, orq, 0);
    orq->orq_pending = 0;
  }

  if (error == EPIPE && orq->orq_retries++ == 0) {
    /* XXX - we should retry only if the transport is not newly created */
    outgoing_print_tport_error(orq, 5, "retrying once after ",
			       tpn, msg, error);
    outgoing_send(orq, 1);
    return;
  }
  else if (error == ECONNREFUSED && orq->orq_try_tcp_instead) {
    /* RFC3261, 18.1.1 */
    if (su_casematch(tpn->tpn_proto, "tcp") && msg_size(msg) <= 65535) {
      outgoing_print_tport_error(orq, 5, "retrying with UDP after ",
				 tpn, msg, error);
      outgoing_try_udp_instead(orq, 0);
      outgoing_remove(orq);	/* Reset state - this is no resend! */
      outgoing_send(orq, 0);	/* Send */
      return;
    }
  }
  else if (error == 0) {
    /*
     * Server closed connection. RFC3261:
     * "there is no coupling between TCP connection state and SIP
     * processing."
     */
    return;
  }

  if (outgoing_other_destinations(orq)) {
    outgoing_print_tport_error(orq, 5, "trying alternative server after ",
			       tpn, msg, error);
    outgoing_try_another(orq);
    return;
  }

  outgoing_print_tport_error(orq, 3, "", tpn, msg, error);

  outgoing_reply(orq, SIP_503_SERVICE_UNAVAILABLE, 0);
}

static
void
outgoing_print_tport_error(nta_outgoing_t *orq, int level, char *todo,
			   tp_name_t const *tpn, msg_t *msg, int error)
{
  su_sockaddr_t const *su = msg_addr(msg);
  char addr[SU_ADDRSIZE];

  su_llog(nta_log, level,
	  "nta: %s (%u): %s%s (%u) with %s/[%s]:%u\n",
	  orq->orq_method_name, orq->orq_cseq->cs_seq,
	  todo, su_strerror(error), error,
	  tpn->tpn_proto,
	  su_inet_ntop(su->su_family, SU_ADDR(su), addr, sizeof(addr)),
	  htons(su->su_port));
}

/**@internal
 * Add features supported.
 */
static
int outgoing_features(nta_agent_t *agent, nta_outgoing_t *orq,
		      msg_t *msg, sip_t *sip,
		      tagi_t *tags)
{
  char const *supported[8];
  int i;

  if (orq->orq_method != sip_method_invite) /* fast path for now */
    return 0;

  supported[i = 0] = NULL;

  if (orq->orq_method == sip_method_invite) {
    int require_100rel = sip_has_feature(sip->sip_require, "100rel");

    if (require_100rel) {
      orq->orq_must_100rel = 1;
      orq->orq_100rel = 1;
    }
    else if (sip_has_feature(sip->sip_supported, "100rel")) {
      orq->orq_100rel = 1;
    }
    else if (orq->orq_100rel) {
	supported[i++] = "100rel";
    }
  }

  if (i) {
    supported[i] = NULL;

    if (sip->sip_supported) {
      su_home_t *home = msg_home(msg);
      return msg_list_append_items(home, sip->sip_supported, supported);
    }
    else {
      sip_supported_t s[1];
      sip_supported_init(s);
      s->k_items = supported;
      return sip_add_dup(msg, sip, (sip_header_t *)s);
    }
  }

  return 0;
}


/**@internal
 * Insert outgoing request to agent hash table
 */
static
void outgoing_insert(nta_agent_t *agent, nta_outgoing_t *orq)
{
  if (outgoing_htable_is_full(agent->sa_outgoing))
    outgoing_htable_resize(agent->sa_home, agent->sa_outgoing, 0);
  outgoing_htable_insert(agent->sa_outgoing, orq);
  orq->orq_inserted = 1;
}

/** @internal
 * Initialize a queue for outgoing transactions.
 */
static void
outgoing_queue_init(outgoing_queue_t *queue, unsigned timeout)
{
  memset(queue, 0, sizeof *queue);
  queue->q_tail = &queue->q_head;
  queue->q_timeout = timeout;
}

/** Change the timeout value of a queue */
static void
outgoing_queue_adjust(nta_agent_t *sa,
		      outgoing_queue_t *queue,
		      unsigned timeout)
{
  nta_outgoing_t *orq;
  uint32_t latest;

  if (timeout >= queue->q_timeout || !queue->q_head) {
    queue->q_timeout = timeout;
    return;
  }

  latest = set_timeout(sa, queue->q_timeout = timeout);

  for (orq = queue->q_head; orq; orq = orq->orq_next) {
    if (orq->orq_timeout == 0 ||
	(int32_t)(orq->orq_timeout - latest) > 0)
      orq->orq_timeout = latest;
  }
}

/** @internal
 * Test if an outgoing transaction is in a queue.
 */
su_inline int
outgoing_is_queued(nta_outgoing_t const *orq)
{
  return orq && orq->orq_queue;
}

/** @internal
 * Insert an outgoing transaction into a queue.
 *
 * Insert a client transaction into a queue and set the corresponding
 * timeout at the same time.
 */
static void
outgoing_queue(outgoing_queue_t *queue,
	       nta_outgoing_t *orq)
{
  if (orq->orq_queue == queue) {
	//assert(queue->q_timeout == 0);
    return;
  }

  assert(!orq->orq_forked);

  if (outgoing_is_queued(orq))
    outgoing_remove(orq);

  orq->orq_timeout = set_timeout(orq->orq_agent, queue->q_timeout);

  orq->orq_queue = queue;
  orq->orq_prev = queue->q_tail;
  *queue->q_tail = orq;
  queue->q_tail = &orq->orq_next;
  queue->q_length++;
}

/** @internal
 * Remove an outgoing transaction from a queue.
 */
su_inline
void outgoing_remove(nta_outgoing_t *orq)
{
  assert(outgoing_is_queued(orq));
  assert(orq->orq_queue->q_length > 0);

  if ((*orq->orq_prev = orq->orq_next))
    orq->orq_next->orq_prev = orq->orq_prev;
  else
    orq->orq_queue->q_tail = orq->orq_prev;

  orq->orq_queue->q_length--;
  orq->orq_next = NULL;
  orq->orq_prev = NULL;
  orq->orq_queue = NULL;
  orq->orq_timeout = 0;
}

/** Set retransmit timer (orq_retry).
 *
 * Set the retry timer (B/D) on the outgoing request (client transaction).
 */
su_inline
void outgoing_set_timer(nta_outgoing_t *orq, uint32_t interval)
{
  nta_outgoing_t **rq;

  assert(orq);

  if (interval == 0) {
    outgoing_reset_timer(orq);
    return;
  }

  if (orq->orq_rprev) {
    /* Remove transaction from retry dequeue, re-insert it later. */
    if ((*orq->orq_rprev = orq->orq_rnext))
      orq->orq_rnext->orq_rprev = orq->orq_rprev;
    if (orq->orq_agent->sa_out.re_t1 == &orq->orq_rnext)
      orq->orq_agent->sa_out.re_t1 = orq->orq_rprev;
  }
  else {
    orq->orq_agent->sa_out.re_length++;
  }

  orq->orq_retry = set_timeout(orq->orq_agent, orq->orq_interval = interval);

  /* Shortcut into queue at SIP T1 */
  rq = orq->orq_agent->sa_out.re_t1;

  if (!(*rq) || (int32_t)((*rq)->orq_retry - orq->orq_retry) > 0)
    rq = &orq->orq_agent->sa_out.re_list;

  while (*rq && (int32_t)((*rq)->orq_retry - orq->orq_retry) <= 0)
    rq = &(*rq)->orq_rnext;

  if ((orq->orq_rnext = *rq))
    orq->orq_rnext->orq_rprev = &orq->orq_rnext;
  *rq = orq;
  orq->orq_rprev = rq;

  if (interval == orq->orq_agent->sa_t1)
    orq->orq_agent->sa_out.re_t1 = rq;
}

static
void outgoing_reset_timer(nta_outgoing_t *orq)
{
  if (orq->orq_rprev) {
    if ((*orq->orq_rprev = orq->orq_rnext))
      orq->orq_rnext->orq_rprev = orq->orq_rprev;
    if (orq->orq_agent->sa_out.re_t1 == &orq->orq_rnext)
      orq->orq_agent->sa_out.re_t1 = orq->orq_rprev;
    orq->orq_agent->sa_out.re_length--;
  }

  orq->orq_interval = 0, orq->orq_retry = 0;
  orq->orq_rnext = NULL, orq->orq_rprev = NULL;
}

/** @internal
 * Free resources associated with the request.
 */
static
void outgoing_free(nta_outgoing_t *orq)
{
  SU_DEBUG_9(("nta: outgoing_free(%p)\n", (void *)orq));
  assert(orq->orq_forks == NULL && orq->orq_forking == NULL);
  outgoing_cut_off(orq);
  outgoing_reclaim(orq);
}

/** Remove outgoing request from hash tables */
su_inline void
outgoing_cut_off(nta_outgoing_t *orq)
{
  nta_agent_t *agent = orq->orq_agent;

  if (orq->orq_default)
    agent->sa_default_outgoing = NULL;

  if (orq->orq_inserted)
    outgoing_htable_remove(agent->sa_outgoing, orq), orq->orq_inserted = 0;

  if (outgoing_is_queued(orq))
    outgoing_remove(orq);

#if 0
  if (orq->orq_forked)
    outgoing_remove_fork(orq);
#endif

  outgoing_reset_timer(orq);

  if (orq->orq_pending) {
    tport_release(orq->orq_tport, orq->orq_pending,
		  orq->orq_request, NULL, orq, 0);
  }
  orq->orq_pending = 0;

  if (orq->orq_cc)
    nta_compartment_decref(&orq->orq_cc);

  if (orq->orq_tport)
    tport_decref(&orq->orq_tport);
}

/** Reclaim outgoing request */
su_inline
void outgoing_reclaim(nta_outgoing_t *orq)
{
  if (orq->orq_status2b)
    *orq->orq_status2b = -1;

  if (orq->orq_request)
    msg_destroy(orq->orq_request), orq->orq_request = NULL;
  if (orq->orq_response)
    msg_destroy(orq->orq_response), orq->orq_response = NULL;
#if HAVE_SOFIA_SRESOLV
  if (orq->orq_resolver)
    outgoing_destroy_resolver(orq);
#endif
  su_free(orq->orq_agent->sa_home, orq);
}

/** Queue request to be freed */
su_inline
void outgoing_free_queue(outgoing_queue_t *q, nta_outgoing_t *orq)
{
  outgoing_cut_off(orq);
  outgoing_queue(q, orq);
}

/** Reclaim memory used by queue of requests */
static
void outgoing_reclaim_queued(su_root_magic_t *rm,
			     su_msg_r msg,
			     union sm_arg_u *u)
{
  outgoing_queue_t *q = u->a_outgoing_queue;
  nta_outgoing_t *orq, *orq_next;

  SU_DEBUG_9(("outgoing_reclaim_all(%p, %p, %p)\n",
	      (void *)rm, (void *)msg, (void *)u));

  for (orq = q->q_head; orq; orq = orq_next) {
    orq_next = orq->orq_next;
    outgoing_reclaim(orq);
  }
}

/** @internal Default callback for request */
int outgoing_default_cb(nta_outgoing_magic_t *magic,
			nta_outgoing_t *orq,
			sip_t const *sip)
{
  if (sip == NULL || sip->sip_status->st_status >= 200)
    outgoing_destroy(orq);
  return 0;
}

/** @internal Destroy an outgoing transaction */
void outgoing_destroy(nta_outgoing_t *orq)
{
  if (orq->orq_terminated || orq->orq_default) {
    if (!orq->orq_forking && !orq->orq_forks) {
      outgoing_free(orq);
      return;
    }
  }
  /* Application is expected to handle 200 OK statelessly
     => kill transaction immediately */
  else if (orq->orq_method == sip_method_invite && !orq->orq_completed
	   /* (unless transaction has been canceled) */
	   && !orq->orq_canceled
	   /* or it has been forked */
	   && !orq->orq_forking && !orq->orq_forks) {
    orq->orq_destroyed = 1;
    outgoing_terminate(orq);
    return;
  }

  orq->orq_destroyed = 1;
  orq->orq_callback = outgoing_default_cb;
  orq->orq_magic = NULL;
}

/** @internal Outgoing transaction timer routine.
 *
 */
static void
_nta_outgoing_timer(nta_agent_t *sa)
{
  uint32_t now = sa->sa_millisec;
  nta_outgoing_t *orq;
  outgoing_queue_t rq[1];
  size_t retransmitted = 0, terminated = 0, timeout = 0, destroyed;
  size_t total = sa->sa_outgoing->oht_used;
  size_t trying = sa->sa_out.re_length;
  size_t pending = sa->sa_out.trying->q_length +
    sa->sa_out.inv_calling->q_length;
  size_t completed = sa->sa_out.completed->q_length +
    sa->sa_out.inv_completed->q_length;

  outgoing_queue_init(sa->sa_out.free = rq, 0);

  while ((orq = sa->sa_out.re_list)) {
    if ((int32_t)(orq->orq_retry - now) > 0)
      break;
    if (retransmitted >= timer_max_retransmit)
      break;

    if (orq->orq_reliable) {
      outgoing_reset_timer(orq);

      if (!tport_is_connected(orq->orq_tport)) {
	/*
	 * Timer N3: try to use UDP if trying to send via TCP
	 * but no connection is established within SIP T4
	 */
	SU_DEBUG_5(("nta: timer %s fired, %s %s (%u)\n", "N3",
		    "try UDP instead for",
		    orq->orq_method_name, orq->orq_cseq->cs_seq));
	outgoing_try_udp_instead(orq, 1);
	outgoing_remove(orq);	/* Reset state - this is no resend! */
	outgoing_send(orq, 0);	/* Send */
      }
      continue;
    }

    assert(!orq->orq_reliable && orq->orq_interval != 0);

    SU_DEBUG_5(("nta: timer %s fired, %s %s (%u)\n",
		orq->orq_method == sip_method_invite ? "A" : "E",
		"retransmit", orq->orq_method_name, orq->orq_cseq->cs_seq));

    outgoing_retransmit(orq);

    if (orq->orq_method == sip_method_invite ||
	2U * orq->orq_interval < sa->sa_t2)
      outgoing_set_timer(orq, 2U * orq->orq_interval);
    else
      outgoing_set_timer(orq, sa->sa_t2);

    if (++retransmitted % 5 == 0)
      su_root_yield(sa->sa_root);	/* Handle received packets */
  }

  terminated
    = outgoing_timer_dk(sa->sa_out.inv_completed, "D", now)
    + outgoing_timer_dk(sa->sa_out.completed, "K", now);

  timeout
    = outgoing_timer_bf(sa->sa_out.inv_calling, "B", now)
    + outgoing_timer_c(sa->sa_out.inv_proceeding, "C", now)
    + outgoing_timer_bf(sa->sa_out.trying, "F", now);

  destroyed = outgoing_mass_destroy(sa, rq);

  sa->sa_out.free = NULL;

  if (retransmitted || timeout || terminated || destroyed) {
    SU_DEBUG_5(("nta_outgoing_timer: "
		MOD_ZU"/"MOD_ZU" resent, "
		MOD_ZU"/"MOD_ZU" tout, "
		MOD_ZU"/"MOD_ZU" term, "
		MOD_ZU"/"MOD_ZU" free\n",
		retransmitted, trying,
		timeout, pending,
		terminated, completed,
		destroyed, total));
  }
}

/** @internal Retransmit the outgoing request. */
void outgoing_retransmit(nta_outgoing_t *orq)
{
  if (orq->orq_prepared && !orq->orq_delayed) {
    orq->orq_retries++;

    if (orq->orq_retries >= 4 && orq->orq_cc) {
      orq->orq_tpn->tpn_comp = NULL;
      if (orq->orq_retries == 4) {
	agent_close_compressor(orq->orq_agent, orq->orq_cc);
	nta_compartment_decref(&orq->orq_cc);
      }
    }

    outgoing_send(orq, 1);
  }
}

/** Trying a client transaction. */
static
void outgoing_trying(nta_outgoing_t *orq)
{
  if (orq->orq_forked)
    ;
  else if (orq->orq_method == sip_method_invite)
    outgoing_queue(orq->orq_agent->sa_out.inv_calling, orq);
  else
    outgoing_queue(orq->orq_agent->sa_out.trying, orq);
}

/** Handle timers B and F */
static
size_t outgoing_timer_bf(outgoing_queue_t *q,
			 char const *timer,
			 uint32_t now)
{
  nta_outgoing_t *orq;
  size_t timeout = 0;

  while ((orq = q->q_head)) {
    if ((int32_t)(orq->orq_timeout - now) > 0 ||
	timeout >= timer_max_timeout)
      break;

    timeout++;

    SU_DEBUG_5(("nta: timer %s fired, %s %s (%u)\n",
		timer,
		orq->orq_method != sip_method_ack ? "timeout" : "terminating",
		orq->orq_method_name, orq->orq_cseq->cs_seq));

    if (orq->orq_method != sip_method_ack)
      outgoing_timeout(orq, now);
    else
      outgoing_terminate(orq);

    assert(q->q_head != orq || (int32_t)(orq->orq_timeout - now) > 0);
  }

  return timeout;
}

/** Handle timer C */
static
size_t outgoing_timer_c(outgoing_queue_t *q,
			char const *timer,
			uint32_t now)
{
  nta_outgoing_t *orq;
  size_t timeout = 0;

  if (q->q_timeout == 0)
    return 0;

  while ((orq = q->q_head)) {
    if ((int32_t)(orq->orq_timeout - now) > 0 || timeout >= timer_max_timeout)
      break;

    timeout++;

    SU_DEBUG_5(("nta: timer %s fired, %s %s (%u)\n",
		timer, "CANCEL and timeout",
		orq->orq_method_name, orq->orq_cseq->cs_seq));
    /*
     * If the client transaction has received a provisional response, the
     * proxy MUST generate a CANCEL request matching that transaction.
     */
    nta_outgoing_tcancel(orq, NULL, NULL, TAG_NULL());
  }

  return timeout;
}

/** @internal Signal transaction timeout to the application. */
void outgoing_timeout(nta_outgoing_t *orq, uint32_t now)
{
  nta_outgoing_t *cancel = NULL;

  if (orq->orq_status || orq->orq_canceled)
    ;
  else if (outgoing_other_destinations(orq)) {
    SU_DEBUG_5(("%s(%p): %s\n", "nta", (void *)orq,
		"try next after timeout"));
    outgoing_try_another(orq);
    return;
  }

  cancel = orq->orq_cancel, orq->orq_cancel = NULL;
  orq->orq_agent->sa_stats->as_tout_request++;

  outgoing_reply(orq, SIP_408_REQUEST_TIMEOUT, 0);

  if (cancel)
    outgoing_timeout(cancel, now);
}

/** Complete a client transaction.
 *
 * @return True if transaction was free()d.
 */
static int
outgoing_complete(nta_outgoing_t *orq)
{
  orq->orq_completed = 1;

  outgoing_reset_timer(orq); /* Timer A / Timer E */

  if (orq->orq_stateless)
    return outgoing_terminate(orq);

  if (orq->orq_forked) {
    outgoing_remove_fork(orq);
    return outgoing_terminate(orq);
  }

  if (orq->orq_reliable) {
    if (orq->orq_method != sip_method_invite || !orq->orq_uas)
      return outgoing_terminate(orq);
  }

  if (orq->orq_method == sip_method_invite) {
    if (orq->orq_queue != orq->orq_agent->sa_out.inv_completed)
      outgoing_queue(orq->orq_agent->sa_out.inv_completed, orq); /* Timer D */
  }
  else {
    outgoing_queue(orq->orq_agent->sa_out.completed, orq); /* Timer K */
  }

  return 0;
}

/** Handle timers D and K */
static
size_t outgoing_timer_dk(outgoing_queue_t *q,
			 char const *timer,
			 uint32_t now)
{
  nta_outgoing_t *orq;
  size_t terminated = 0;

  while ((orq = q->q_head)) {
    if ((int32_t)(orq->orq_timeout - now) > 0 ||
	terminated >= timer_max_terminate)
      break;

    terminated++;

    SU_DEBUG_5(("nta: timer %s fired, %s %s (%u)\n", timer,
		"terminate", orq->orq_method_name, orq->orq_cseq->cs_seq));

    if (orq->orq_method == sip_method_invite)
      outgoing_terminate_invite(orq);
    else
      outgoing_terminate(orq);
  }

  return terminated;
}


/** Terminate an INVITE client transaction. */
static void
outgoing_terminate_invite(nta_outgoing_t *original)
{
  nta_outgoing_t *orq = original;

  while (original->orq_forks) {
    orq = original->orq_forks;
    original->orq_forks = orq->orq_forks;

    assert(orq->orq_forking == original);

    SU_DEBUG_5(("nta: timer %s fired, %s %s (%u);tag=%s\n", "D",
		"terminate", orq->orq_method_name, orq->orq_cseq->cs_seq,
		orq->orq_tag));

    orq->orq_forking = NULL, orq->orq_forks = NULL, orq->orq_forked = 0;

    if (outgoing_terminate(orq))
      continue;

    if (orq->orq_status < 200) {
      /* Fork has timed out */
      orq->orq_agent->sa_stats->as_tout_request++;
      outgoing_reply(orq, SIP_408_REQUEST_TIMEOUT, 0);
    }
  }

  if (outgoing_terminate(orq = original))
    return;

  if (orq->orq_status < 200) {
    /* Original INVITE has timed out */
    orq->orq_agent->sa_stats->as_tout_request++;
    outgoing_reply(orq, SIP_408_REQUEST_TIMEOUT, 0);
  }
}

static void
outgoing_remove_fork(nta_outgoing_t *orq)
{
  nta_outgoing_t **slot;

  for (slot = &orq->orq_forking->orq_forks;
       *slot;
       slot = &(*slot)->orq_forks) {
    if (orq == *slot) {
      *slot = orq->orq_forks;
      orq->orq_forks = NULL;
      orq->orq_forking = NULL;
      orq->orq_forked = 0;
    }
  }

  assert(orq == NULL);
}

/** Terminate a client transaction. */
static
int outgoing_terminate(nta_outgoing_t *orq)
{
  orq->orq_terminated = 1;

  if (!orq->orq_destroyed) {
    outgoing_queue(orq->orq_agent->sa_out.terminated, orq);
    return 0;
  }
  else if (orq->orq_agent->sa_out.free) {
    outgoing_free_queue(orq->orq_agent->sa_out.free, orq);
    return 1;
  }
  else {
    outgoing_free(orq);
    return 1;
  }
}

/** Mass destroy client transactions */
static
size_t outgoing_mass_destroy(nta_agent_t *sa, outgoing_queue_t *q)
{
  size_t destroyed = q->q_length;

  if (destroyed > 2 && *sa->sa_terminator) {
    su_msg_r m = SU_MSG_R_INIT;

    if (su_msg_create(m,
		      su_clone_task(sa->sa_terminator),
		      su_root_task(sa->sa_root),
		      outgoing_reclaim_queued,
		      sizeof(outgoing_queue_t)) == SU_SUCCESS) {
      outgoing_queue_t *mq = su_msg_data(m)->a_outgoing_queue;

      *mq = *q;

      if (su_msg_send(m) == SU_SUCCESS)
	q->q_length = 0;
    }
  }

  if (q->q_length)
    outgoing_reclaim_queued(NULL, NULL, (void*)q);

  return destroyed;
}

/** Find an outgoing request corresponging to a message and @Via line.
 *
 * Return an outgoing request object based on a message and the @Via line
 * given as argument. This function is used when doing loop checking: if we
 * have sent the request and it has been routed back to us.
 *
 * @param agent
 * @param msg
 * @param sip
 * @param v
 */
nta_outgoing_t *nta_outgoing_find(nta_agent_t const *agent,
				  msg_t const *msg,
				  sip_t const *sip,
				  sip_via_t const *v)
{
  if (agent == NULL || msg == NULL || sip == NULL || v == NULL) {
    su_seterrno(EFAULT);
    return NULL;
  }

  return outgoing_find(agent, msg, sip, v);
}

/**@internal
 *
 * Find an outgoing request corresponging to a message and @Via line.
 *
 */
nta_outgoing_t *outgoing_find(nta_agent_t const *sa,
			      msg_t const *msg,
			      sip_t const *sip,
			      sip_via_t const *v)
{
  nta_outgoing_t **oo, *orq;
  outgoing_htable_t const *oht = sa->sa_outgoing;
  sip_cseq_t const *cseq = sip->sip_cseq;
  sip_call_id_t const *i = sip->sip_call_id;
  hash_value_t hash;
  sip_method_t method, method2;
  unsigned short status = sip->sip_status ? sip->sip_status->st_status : 0;

  if (cseq == NULL)
    return NULL;

  hash = NTA_HASH(i, cseq->cs_seq);

  method = cseq->cs_method;

  /* Get original invite when ACKing */
  if (sip->sip_request && method == sip_method_ack && v == NULL)
    method = sip_method_invite, method2 = sip_method_invalid;
  else if (sa->sa_is_a_uas && 200 <= status && status < 300 && method == sip_method_invite)
    method2 = sip_method_ack;
  else
    method2 = method;

  for (oo = outgoing_htable_hash(oht, hash);
       (orq = *oo);
       oo = outgoing_htable_next(oht, oo)) {
    if (orq->orq_stateless)
      continue;
    /* Accept terminated transactions when looking for original INVITE */
    if (orq->orq_terminated && method2 != sip_method_invalid)
      continue;
    if (hash != orq->orq_hash)
      continue;
    if (orq->orq_call_id->i_hash != i->i_hash ||
	strcmp(orq->orq_call_id->i_id, i->i_id))
      continue;
    if (orq->orq_cseq->cs_seq != cseq->cs_seq)
      continue;
    if (method == sip_method_unknown &&
	strcmp(orq->orq_cseq->cs_method_name, cseq->cs_method_name))
      continue;
    if (orq->orq_method != method && orq->orq_method != method2)
      continue;
    if (su_strcasecmp(orq->orq_from->a_tag, sip->sip_from->a_tag))
      continue;
    if (orq->orq_to->a_tag &&
	su_strcasecmp(orq->orq_to->a_tag, sip->sip_to->a_tag))
      continue;

    if (orq->orq_method == sip_method_ack && 300 <= status)
      continue;

    if (v && !su_casematch(orq->orq_branch + strlen("branch="), v->v_branch))
      continue;

    break;			/* match */
  }

  return orq;
}

/** Process a response message. */
int outgoing_recv(nta_outgoing_t *_orq,
		  int status,
		  msg_t *msg,
		  sip_t *sip)
{
  nta_outgoing_t *orq = _orq->orq_forking ? _orq->orq_forking : _orq;
  nta_agent_t *sa = orq->orq_agent;
  int internal = sip == NULL || (sip->sip_flags & NTA_INTERNAL_MSG) != 0;

  assert(!internal || status >= 300);
  assert(orq == _orq || orq->orq_method == sip_method_invite);

  if (status < 100) status = 100;

  if (!internal && orq->orq_delay == UINT_MAX)
    outgoing_estimate_delay(orq, sip);

  if (orq->orq_cc)
    agent_accept_compressed(orq->orq_agent, msg, orq->orq_cc);

  if (orq->orq_cancel) {
    nta_outgoing_t *cancel;
    cancel = orq->orq_cancel; orq->orq_cancel = NULL;
    cancel->orq_delayed = 0;

    if (status < 200) {
      outgoing_send(cancel, 0);
      outgoing_complete(orq);
    }
    else {
      outgoing_reply(cancel, SIP_481_NO_TRANSACTION, 0);
    }
  }

  if (orq->orq_pending) {
    tport_release(orq->orq_tport, orq->orq_pending, orq->orq_request,
		  msg, orq, status < 200);
    if (status >= 200)
      orq->orq_pending = 0;
  }

  /* The state machines */
  if (orq->orq_method == sip_method_invite) {
    nta_outgoing_t *original = orq;

    orq = _orq;

    if (orq->orq_destroyed && 200 <= status && status < 300) {
      if (orq->orq_uas && su_strcasecmp(sip->sip_to->a_tag, orq->orq_tag) != 0) {
        /* Orphan 200 Ok to INVITE. ACK and BYE it */
		  SU_DEBUG_5(("nta: Orphan 200 Ok send ACK&BYE %p\n", (void *)orq));
        return nta_msg_ackbye(sa, msg);
      }
      return -1;  /* Proxy statelessly (RFC3261 section 16.11) */
    }

    outgoing_reset_timer(original); /* Retransmission */

    if (status < 200) {
      if (original->orq_status < 200)
	original->orq_status = status;
      if (orq->orq_status < 200)
	orq->orq_status = status;

      if (original->orq_queue == sa->sa_out.inv_calling) {
	outgoing_queue(sa->sa_out.inv_proceeding, original);
      }
      else if (original->orq_queue == sa->sa_out.inv_proceeding) {
	if (sa->sa_out.inv_proceeding->q_timeout) {
	  outgoing_remove(original);
	  outgoing_queue(sa->sa_out.inv_proceeding, original);
	}
      }

      /* Handle 100rel */
      if (sip && sip->sip_rseq) {
	if (outgoing_recv_reliable(orq, msg, sip) < 0) {
	  msg_destroy(msg);
	  return 0;
	}
      }
    }
    else {
      /* Final response */
      if (status >= 300 && !internal)
	outgoing_ack(original, sip);

      if (!original->orq_completed) {
	if (outgoing_complete(original))
	  return 0;

	if (orq->orq_uas && sip && orq == original) {
	  /*
	   * We silently discard duplicate final responses to INVITE below
	   * with outgoing_duplicate()
	   */
	  su_home_t *home = msg_home(orq->orq_request);
	  orq->orq_tag = su_strdup(home, sip->sip_to->a_tag);
	}
      }
      /* Retransmission or response from another fork */
      else if (orq->orq_status >= 200) {
	/* Once 2xx has been received, non-2xx will not be forwarded */
	if (status >= 300)
	  return outgoing_duplicate(orq, msg, sip);

	if (orq->orq_uas) {
	  if (su_strcasecmp(sip->sip_to->a_tag, orq->orq_tag) == 0)
	    /* Catch retransmission */
	    return outgoing_duplicate(orq, msg, sip);

          /* Orphan 200 Ok to INVITE. ACK and BYE it */
          SU_DEBUG_5(("nta: Orphan 200 Ok send ACK&BYE" VA_NONE));
          return nta_msg_ackbye(sa, msg);
	}
      }

      orq->orq_status = status;
    }
  }
  else if (orq->orq_method != sip_method_ack) {
    /* Non-INVITE */
    if (orq->orq_queue == sa->sa_out.trying ||
	orq->orq_queue == sa->sa_out.resolving) {
	  /* hacked by freeswitch, this is being hit by options 404 status with 404 orq->orq_status and orq_destroyed = 1, orq_completed = 1 */  
	  /*      assert(orq->orq_status < 200); */
	  if (orq->orq_status >= 200) {msg_destroy(msg); return 0;}

      if (status < 200) {
	/* @RFC3261 17.1.2.1:
	 * retransmissions continue for unreliable transports,
	 * but at an interval of T2.
	 *
         * @RFC4321 1.2:
         * Note that Timer E is not altered during the transition
         * to Proceeding.
         */
 	if (!orq->orq_reliable)
	  orq->orq_interval = sa->sa_t2;
      }
      else if (!outgoing_complete(orq)) {
	if (orq->orq_sigcomp_zap && orq->orq_tport && orq->orq_cc)
	  agent_zap_compressor(orq->orq_agent, orq->orq_cc);
      }
      else /* outgoing_complete */ {
	msg_destroy(msg);
	return 0;
      }
    }
    else {
      /* Already completed or terminated */
      assert(orq->orq_queue == sa->sa_out.completed ||
	     orq->orq_queue == sa->sa_out.terminated);
      assert(orq->orq_status >= 200);
      return outgoing_duplicate(orq, msg, sip);
    }

    orq->orq_status = status;
  }
  else {
    /* ACK */
    if (sip && (sip->sip_flags & NTA_INTERNAL_MSG) == 0)
      /* Received re-transmitted final reply to INVITE, retransmit ACK */
      outgoing_retransmit(orq);
    msg_destroy(msg);
    return 0;
  }

  if (100 >= status + orq->orq_pass_100) {
    msg_destroy(msg);
    return 0;
  }

  if (orq->orq_destroyed) {
    msg_destroy(msg);
    return 0;
  }

  if (orq->orq_response)
    msg_destroy(orq->orq_response);
  orq->orq_response = msg;
  /* Call callback */
  orq->orq_callback(orq->orq_magic, orq, sip);
  return 0;
}

static void outgoing_default_recv(nta_outgoing_t *orq,
				 int status,
				 msg_t *msg,
				 sip_t *sip)
{
  assert(sip->sip_cseq);

  orq->orq_status = status;
  orq->orq_response = msg;
  orq->orq_callback(orq->orq_magic, orq, sip);
  orq->orq_response = NULL;
  orq->orq_status = 0;
  msg_destroy(msg);
}

static void outgoing_estimate_delay(nta_outgoing_t *orq, sip_t *sip)
{
  su_time_t now = su_now();
  double diff = 1000 * su_time_diff(now, orq->orq_sent);

  if (orq->orq_timestamp && sip->sip_timestamp) {
    double diff2, delay = 0.0;
    su_time_t timestamp = { 0, 0 };
    char const *bad;

    sscanf(sip->sip_timestamp->ts_stamp, "%lu.%lu",
	   &timestamp.tv_sec, &timestamp.tv_usec);

    diff2 = 1000 * su_time_diff(now, timestamp);

    if (diff2 < 0)
      bad = "negative";
    else if (diff2 > diff + 1e-3)
      bad = "too large";
    else {
      if (sip->sip_timestamp->ts_delay)
	sscanf(sip->sip_timestamp->ts_delay, "%lg", &delay);

      if (1000 * delay <= diff2) {
	diff = diff2 - 1000 * delay;
	orq->orq_delay = (unsigned)diff;
	SU_DEBUG_7(("nta_outgoing: RTT is %g ms, now is %lu.%06lu, "
		    "Timestamp was %s %s\n",
		    diff, now.tv_sec, now.tv_usec,
		    sip->sip_timestamp->ts_stamp,
		    sip->sip_timestamp->ts_delay ?
		    sip->sip_timestamp->ts_delay : ""));
	return;
      }
      bad = "delay";
    }

    SU_DEBUG_3(("nta_outgoing: %s Timestamp %lu.%06lu %g "
		"(sent %lu.%06lu, now is %lu.%06lu)\n",
		bad,
		timestamp.tv_sec, timestamp.tv_usec,
		delay,
		orq->orq_sent.tv_sec, orq->orq_sent.tv_usec,
		now.tv_sec, now.tv_usec));
  }

  if (diff >= 0 && diff < (double)UINT_MAX) {
    orq->orq_delay = (unsigned)diff;
    SU_DEBUG_7(("nta_outgoing: RTT is %g ms\n", diff));
  }
}

/**@typedef nta_response_f
 *
 * Callback for replies to outgoing requests.
 *
 * This is a callback function invoked by NTA when it has received a new
 * reply to an outgoing request.
 *
 * @param magic   request context
 * @param request request handle
 * @param sip     received status message
 *
 * @return
 * This callback function should return always 0.
 *
 */

/** Process duplicate responses */
static int outgoing_duplicate(nta_outgoing_t *orq,
			      msg_t *msg,
			      sip_t *sip)
{
  sip_via_t *v;

  if (sip && (sip->sip_flags & NTA_INTERNAL_MSG) == 0) {
    v = sip->sip_via;

    SU_DEBUG_5(("nta: %u %s is duplicate response to %d %s\n",
		sip->sip_status->st_status, sip->sip_status->st_phrase,
		orq->orq_cseq->cs_seq, orq->orq_cseq->cs_method_name));
    if (v)
      SU_DEBUG_5(("\tVia: %s %s%s%s%s%s%s%s%s%s\n",
		  v->v_protocol, v->v_host,
		  SIP_STRLOG(":", v->v_port),
		  SIP_STRLOG(" ;received=", v->v_received),
		  SIP_STRLOG(" ;maddr=", v->v_maddr),
		  SIP_STRLOG(" ;branch=", v->v_branch)));
  }

  msg_destroy(msg);
  return 0;
}

/** @internal ACK to a final response (300..699).
 * These messages are ACK'ed via the original URL (and tport)
 */
void outgoing_ack(nta_outgoing_t *orq, sip_t *sip)
{
  msg_t *ackmsg;

  assert(orq);

  /* Do not ack internally generated messages... */
  if (sip == NULL || sip->sip_flags & NTA_INTERNAL_MSG)
    return;

  assert(sip); assert(sip->sip_status);
  assert(sip->sip_status->st_status >= 300);
  assert(orq->orq_tport);

  ackmsg = outgoing_ackmsg(orq, SIP_METHOD_ACK, SIPTAG_TO(sip->sip_to), TAG_END());
  if (!ackmsg)
    return;

  if (!outgoing_create(orq->orq_agent, NULL, NULL,
		      NULL, orq->orq_tpn, ackmsg,
		      NTATAG_BRANCH_KEY(sip->sip_via->v_branch),
		      NTATAG_USER_VIA(1),
		      NTATAG_STATELESS(1),
		      TAG_END()))
    msg_destroy(ackmsg);
}

/** Generate messages for hop-by-hop ACK or CANCEL.
 */
msg_t *outgoing_ackmsg(nta_outgoing_t *orq, sip_method_t m, char const *mname,
		       tag_type_t tag, tag_value_t value, ...)
{
  msg_t *msg = nta_msg_create(orq->orq_agent, 0);
  su_home_t *home = msg_home(msg);
  sip_t *sip = sip_object(msg);
  sip_t *old = sip_object(orq->orq_request);
  sip_via_t via[1];

  if (!sip)
    return NULL;

  if (tag) {
    ta_list ta;

    ta_start(ta, tag, value);

    sip_add_tl(msg, sip, ta_tags(ta));
    /* Bug sf.net # 173323:
     * Ensure that request-URI, topmost Via, From, To, Call-ID, CSeq,
     * Max-Forward, Route, Accept-Contact, Reject-Contact and
     * Request-Disposition are copied from original request
     */
    if (sip->sip_from)
      sip_header_remove(msg, sip, (void *)sip->sip_from);
    if (sip->sip_to && m != sip_method_ack)
      sip_header_remove(msg, sip, (void *)sip->sip_to);
    if (sip->sip_call_id)
      sip_header_remove(msg, sip, (void *)sip->sip_call_id);
    while (sip->sip_route)
      sip_header_remove(msg, sip, (void *)sip->sip_route);
    while (sip->sip_accept_contact)
      sip_header_remove(msg, sip, (void *)sip->sip_accept_contact);
    while (sip->sip_reject_contact)
      sip_header_remove(msg, sip, (void *)sip->sip_reject_contact);
    if (sip->sip_request_disposition)
      sip_header_remove(msg, sip, (void *)sip->sip_request_disposition);
    while (sip->sip_via)
      sip_header_remove(msg, sip, (void *)sip->sip_via);
    if (sip->sip_max_forwards)
      sip_header_remove(msg, sip, (void *)sip->sip_max_forwards);

    ta_end(ta);
  }

  sip->sip_request =
    sip_request_create(home, m, mname, (url_string_t *)orq->orq_url, NULL);

  if (sip->sip_to == NULL)
    sip_add_dup(msg, sip, (sip_header_t *)old->sip_to);
  sip_add_dup(msg, sip, (sip_header_t *)old->sip_from);
  sip_add_dup(msg, sip, (sip_header_t *)old->sip_call_id);
  sip_add_dup(msg, sip, (sip_header_t *)old->sip_route);
  /* @RFC3841. Bug #1326727. */
  sip_add_dup(msg, sip, (sip_header_t *)old->sip_accept_contact);
  sip_add_dup(msg, sip, (sip_header_t *)old->sip_reject_contact);
  sip_add_dup(msg, sip, (sip_header_t *)old->sip_request_disposition);
  sip_add_dup(msg, sip, (sip_header_t *)old->sip_max_forwards);

  if (old->sip_via) {
    /* Add only the topmost Via header */
    *via = *old->sip_via; via->v_next = NULL;
    sip_add_dup(msg, sip, (sip_header_t *)via);
  }

  sip->sip_cseq = sip_cseq_create(home, old->sip_cseq->cs_seq, m, mname);

  if (sip->sip_request &&
      sip->sip_to &&
      sip->sip_from &&
      sip->sip_call_id &&
      (!old->sip_route || sip->sip_route) &&
      sip->sip_cseq)
    return msg;

  msg_destroy(msg);

  return NULL;
}

static
void outgoing_delayed_recv(su_root_magic_t *rm,
			   su_msg_r msg,
			   union sm_arg_u *u);

/** Respond internally to a transaction. */
int outgoing_reply(nta_outgoing_t *orq, int status, char const *phrase,
		   int delayed)
{
  nta_agent_t *agent = orq->orq_agent;
  msg_t *msg = NULL;
  sip_t *sip = NULL;

  assert(status == 202 || status >= 400);

  if (orq->orq_pending)
    tport_release(orq->orq_tport, orq->orq_pending,
		  orq->orq_request, NULL, orq, 0);
  orq->orq_pending = 0;

  orq->orq_delayed = 0;

  if (orq->orq_method == sip_method_ack) {
    if (status != delayed)
      SU_DEBUG_3(("nta(%p): responding %u %s to ACK!\n",
		  (void *)orq, status, phrase));
    orq->orq_status = status;
    if (orq->orq_queue == NULL)
      outgoing_trying(orq);	/* Timer F */
    return 0;
  }

  if (orq->orq_destroyed) {
    if (orq->orq_status < 200)
      orq->orq_status = status;
    outgoing_complete(orq);	/* Timer D / Timer K */
    return 0;
  }

  if (orq->orq_stateless)
    ;
  else if (orq->orq_queue == NULL ||
	   orq->orq_queue == orq->orq_agent->sa_out.resolving ||
	   orq->orq_queue == orq->orq_agent->sa_out.delayed)
    outgoing_trying(orq);

  /** Insert a dummy Via header */
  if (!orq->orq_prepared) {
    tport_t *tp = tport_primaries(orq->orq_agent->sa_tports);
    outgoing_insert_via(orq, agent_tport_via(tp));
  }

  /* Create response message, if needed */
  if (!orq->orq_stateless &&
      !(orq->orq_callback == outgoing_default_cb) &&
      !(status == 408 &&
	orq->orq_method != sip_method_invite &&
	!orq->orq_agent->sa_timeout_408)) {
    char const *to_tag;

    msg = nta_msg_create(agent, NTA_INTERNAL_MSG);

    if (complete_response(msg, status, phrase, orq->orq_request) < 0) {
      assert(!"complete message");
      return -1;
    }

    sip = sip_object(msg); assert(sip->sip_flags & NTA_INTERNAL_MSG);
    to_tag = nta_agent_newtag(msg_home(msg), "tag=%s", agent);

    if (status > 100 &&
	sip->sip_to && !sip->sip_to->a_tag &&
	sip->sip_cseq->cs_method != sip_method_cancel &&
	sip_to_tag(msg_home(msg), sip->sip_to, to_tag) < 0) {
      assert(!"adding tag");
      return -1;
    }

    if (status > 400 && agent->sa_blacklist) {
      sip_retry_after_t af[1];
      sip_retry_after_init(af)->af_delta = agent->sa_blacklist;

      sip_add_dup(msg, sip, (sip_header_t *)af);
    }
  }

  if (orq->orq_inserted && !delayed) {
    outgoing_recv(orq, status, msg, sip);
    return 0;
  }
  else if (orq->orq_stateless && orq->orq_callback == outgoing_default_cb) {
    /* Xyzzy */
    orq->orq_status = status;
    outgoing_complete(orq);
  }
  else {
    /*
     * The thread creating outgoing transaction must return to application
     * before transaction callback can be invoked. Therefore processing an
     * internally generated response message must be delayed until
     * transaction creation is completed.
     *
     * The internally generated message is transmitted using su_msg_send()
     * and it is delivered back to NTA when the application next time
     * executes the su_root_t event loop.
     */
    nta_agent_t *agent = orq->orq_agent;
    su_root_t *root = agent->sa_root;
    su_msg_r su_msg = SU_MSG_R_INIT;

    if (su_msg_create(su_msg,
		      su_root_task(root),
		      su_root_task(root),
		      outgoing_delayed_recv,
		      sizeof(struct outgoing_recv_s)) == SU_SUCCESS) {
      struct outgoing_recv_s *a = su_msg_data(su_msg)->a_outgoing_recv;

      a->orq = orq;
      a->msg = msg;
      a->sip = sip;
      a->status = status;

      orq->orq_status2b = &a->status;

      if (su_msg_send(su_msg) == SU_SUCCESS) {
	return 0;
      }
    }
  }

  if (msg)
    msg_destroy(msg);

  return -1;
}

static
void outgoing_delayed_recv(su_root_magic_t *rm,
			   su_msg_r msg,
			   union sm_arg_u *u)
{
  struct outgoing_recv_s *a = u->a_outgoing_recv;

  if (a->status > 0) {
    a->orq->orq_status2b = 0;
    if (outgoing_recv(a->orq, a->status, a->msg, a->sip) >= 0)
      return;
  }

  msg_destroy(a->msg);
}


/* ====================================================================== */
/* 9) Resolving (SIP) URL */

#if HAVE_SOFIA_SRESOLV

struct sipdns_query;

/** DNS resolving for (SIP) URLs */
struct sipdns_resolver
{
  tp_name_t             sr_tpn[1];     	/**< Copy of original transport name */
  sres_query_t         *sr_query;      	/**< Current DNS Query */
  char const           *sr_target;     	/**< Target for current query */

  struct sipdns_query  *sr_current;    	/**< Current query (with results) */
  char                **sr_results;    	/**< A/AAAA results to be used */

  struct sipdns_query  *sr_head;       	/**< List of intermediate results */
  struct sipdns_query **sr_tail;       	/**< End of intermediate result list */

  struct sipdns_query  *sr_done;       	/**< Completed intermediate results */

  struct sipdns_tport const *sr_tport;  /**< Selected transport */

  /** Transports to consider for this request */
  struct sipdns_tport const *sr_tports[SIPDNS_TRANSPORTS + 1];

  uint16_t sr_a_aaaa1, sr_a_aaaa2;     /**< Order of A and/or AAAA queries. */

  unsigned
    sr_use_naptr:1,
    sr_use_srv:1,
    sr_use_a_aaaa:1;
};

/** Intermediate queries */
struct sipdns_query
{
  struct sipdns_query *sq_next;

  char const *sq_proto;
  char const *sq_domain;
  char     sq_port[6];		/* port number */
  uint16_t sq_otype;		/* origin type of query data (0 means request) */
  uint16_t sq_type;		/* query type */
  uint16_t sq_priority;		/* priority or preference  */
  uint16_t sq_weight;		/* preference or weight */
  uint16_t sq_grayish;		/* candidate for graylisting */
};

static int outgoing_resolve_next(nta_outgoing_t *orq);
static int outgoing_resolving(nta_outgoing_t *orq);
static int outgoing_resolving_error(nta_outgoing_t *,
				    int status, char const *phrase);
static void outgoing_graylist(nta_outgoing_t *orq, struct sipdns_query *sq);
static int outgoing_query_naptr(nta_outgoing_t *orq, char const *domain);
static void outgoing_answer_naptr(sres_context_t *orq, sres_query_t *q,
				  sres_record_t *answers[]);
struct sipdns_tport const *outgoing_naptr_tport(nta_outgoing_t *orq,
						sres_record_t *answers[]);

static int outgoing_make_srv_query(nta_outgoing_t *orq);
static int outgoing_make_a_aaaa_query(nta_outgoing_t *orq);

static void outgoing_query_all(nta_outgoing_t *orq);

static int outgoing_query_srv(nta_outgoing_t *orq, struct sipdns_query *);
static void outgoing_answer_srv(sres_context_t *orq, sres_query_t *q,
				sres_record_t *answers[]);

#if SU_HAVE_IN6
static int outgoing_query_aaaa(nta_outgoing_t *orq, struct sipdns_query *);
static void outgoing_answer_aaaa(sres_context_t *orq, sres_query_t *q,
				 sres_record_t *answers[]);
#endif

static int outgoing_query_a(nta_outgoing_t *orq, struct sipdns_query *);
static void outgoing_answer_a(sres_context_t *orq, sres_query_t *q,
			      sres_record_t *answers[]);

static void outgoing_query_results(nta_outgoing_t *orq,
				   struct sipdns_query *sq,
				   char *results[],
				   size_t rlen);


#define SIPDNS_503_ERROR 503, "DNS Error"

/** Resolve a request destination */
static void
outgoing_resolve(nta_outgoing_t *orq,
		 int explicit_transport,
		 enum nta_res_order_e res_order)
{
  struct sipdns_resolver *sr = NULL;
  char const *tpname = orq->orq_tpn->tpn_proto;
  int tport_known = strcmp(tpname, "*") != 0;

  if (orq->orq_agent->sa_resolver)
    orq->orq_resolver = sr = su_zalloc(orq->orq_agent->sa_home, (sizeof *sr));

  if (!sr) {
    outgoing_resolving_error(orq, SIP_500_INTERNAL_SERVER_ERROR);
    return;
  }

  *sr->sr_tpn = *orq->orq_tpn;
  sr->sr_use_srv = orq->orq_agent->sa_use_srv;
  sr->sr_use_naptr = orq->orq_agent->sa_use_naptr && sr->sr_use_srv;
  sr->sr_use_a_aaaa = 1;
  sr->sr_tail = &sr->sr_head;

  /* RFC 3263:
     If the TARGET was not a numeric IP address, but a port is present in
     the URI, the client performs an A or AAAA record lookup of the domain
     name.  The result will be a list of IP addresses, each of which can
     be contacted at the specific port from the URI and transport protocol
     determined previously.  The client SHOULD try the first record.  If
     an attempt should fail, based on the definition of failure in Section
     4.3, the next SHOULD be tried, and if that should fail, the next
     SHOULD be tried, and so on.

     This is a change from RFC 2543.  Previously, if the port was
     explicit, but with a value of 5060, SRV records were used.  Now, A
     or AAAA records will be used.
  */
  if (sr->sr_tpn->tpn_port)
    sr->sr_use_naptr = 0, sr->sr_use_srv = 0;

  /* RFC3263:
     If [...] a transport was specified explicitly, the client performs an
     SRV query for that specific transport,
  */
  if (explicit_transport)
    sr->sr_use_naptr = 0;

  {
    /* Initialize sr_tports */
    tport_t *tport;
    char const *ident = sr->sr_tpn->tpn_ident;
    int i, j;

    for (tport = tport_primary_by_name(orq->orq_agent->sa_tports, orq->orq_tpn);
	 tport;
	 tport = tport_next(tport)) {
      tp_name_t const *tpn = tport_name(tport);
      if (tport_known && !su_casematch(tpn->tpn_proto, tpname))
	continue;
      if (ident && (tpn->tpn_ident == NULL || strcmp(ident, tpn->tpn_ident)))
	continue;

      for (j = 0; j < SIPDNS_TRANSPORTS; j++)
	if (su_casematch(tpn->tpn_proto, sipdns_tports[j].name))
	  break;

      assert(j < SIPDNS_TRANSPORTS);
      if (j == SIPDNS_TRANSPORTS)
	/* Someone added transport but did not update sipdns_tports */
	continue;

      for (i = 0; i < SIPDNS_TRANSPORTS; i++) {
	if (sipdns_tports + j == sr->sr_tports[i] || sr->sr_tports[i] == NULL)
	  break;
      }
      sr->sr_tports[i] = sipdns_tports + j;

      if (tport_known) /* Looking for only one transport */ {
	sr->sr_tport = sipdns_tports + j;
	break;
      }
    }

    /* Nothing found */
    if (!sr->sr_tports[0]) {
      SU_DEBUG_3(("nta(%p): transport %s is not supported%s%s\n", (void *)orq,
		  tpname, ident ? " by interface " : "", ident ? ident : ""));
      outgoing_resolving_error(orq, SIPDNS_503_ERROR);
      return;
    }
  }

  switch (res_order) {
  default:
  case nta_res_ip6_ip4:
    sr->sr_a_aaaa1 = sres_type_aaaa, sr->sr_a_aaaa2 = sres_type_a;
    break;
  case nta_res_ip4_ip6:
    sr->sr_a_aaaa1 = sres_type_a, sr->sr_a_aaaa2 = sres_type_aaaa;
    break;
  case nta_res_ip6_only:
    sr->sr_a_aaaa1 = sres_type_aaaa, sr->sr_a_aaaa2 = sres_type_aaaa;
    break;
  case nta_res_ip4_only:
    sr->sr_a_aaaa1 = sres_type_a, sr->sr_a_aaaa2 = sres_type_a;
    break;
  }

  outgoing_resolve_next(orq);
}

/** Resolve next destination. */
static int
outgoing_resolve_next(nta_outgoing_t *orq)
{
  struct sipdns_resolver *sr = orq->orq_resolver;

  if (sr == NULL) {
    outgoing_resolving_error(orq, SIP_500_INTERNAL_SERVER_ERROR);
    return 0;
  }

  if (sr->sr_results) {
    /* Use existing A/AAAA results */
    su_free(msg_home(orq->orq_request), sr->sr_results[0]);
    sr->sr_results++;
    if (sr->sr_results[0]) {
      struct sipdns_query *sq = sr->sr_current; assert(sq);

      if (sq->sq_proto)
	orq->orq_tpn->tpn_proto = sq->sq_proto;
      if (sq->sq_port[0])
	  orq->orq_tpn->tpn_port = sq->sq_port;

      orq->orq_tpn->tpn_host = sr->sr_results[0];

      outgoing_reset_timer(orq);
      outgoing_queue(orq->orq_agent->sa_out.resolving, orq);
      outgoing_prepare_send(orq);
      return 1;
    }
    else {
      sr->sr_current = NULL;
      sr->sr_results = NULL;
    }
  }

  if (sr->sr_head)
    outgoing_query_all(orq);
  else if (sr->sr_use_naptr)
    outgoing_query_naptr(orq, sr->sr_tpn->tpn_host); /* NAPTR */
  else if (sr->sr_use_srv)
    outgoing_make_srv_query(orq);	/* SRV */
  else if (sr->sr_use_a_aaaa)
    outgoing_make_a_aaaa_query(orq);	/* A/AAAA */
  else
    return outgoing_resolving_error(orq, SIPDNS_503_ERROR);

  return 1;
}

/** Check if can we retry other destinations? */
static int
outgoing_other_destinations(nta_outgoing_t const *orq)
{
  struct sipdns_resolver *sr = orq->orq_resolver;

  if (!sr)
    return 0;

  if (sr->sr_use_a_aaaa || sr->sr_use_srv || sr->sr_use_naptr)
    return 1;

  if (sr->sr_results && sr->sr_results[1])
    return 1;

  if (sr->sr_head)
    return 1;

  return 0;
}

/** Resolve a request destination */
static int
outgoing_try_another(nta_outgoing_t *orq)
{
  struct sipdns_resolver *sr = orq->orq_resolver;

  if (sr == NULL)
    return 0;

  *orq->orq_tpn = *sr->sr_tpn;
  orq->orq_try_tcp_instead = 0, orq->orq_try_udp_instead = 0;
  outgoing_reset_timer(orq);
  outgoing_queue(orq->orq_agent->sa_out.resolving, orq);

  if (orq->orq_status > 0)
    /* PP: don't hack priority if a preliminary response has been received */
    ;
  else if (orq->orq_agent->sa_graylist == 0)
    /* PP: priority hacking disabled */
    ;
  /* NetModule hack:
   * Move server that did not work to end of queue in sres cache
   *
   * the next request does not try to use the server that is currently down
   *
   * @TODO: fix cases with only A or AAAA answering, or all servers down.
   */
  else if (sr && sr->sr_target) {
    struct sipdns_query *sq;

    /* find latest A/AAAA record */
    sq = sr->sr_head;
    if (sq && sq->sq_type == sr->sr_a_aaaa2 && sr->sr_a_aaaa1 != sr->sr_a_aaaa2) {
      sq->sq_grayish = 1;
    }
    else {
      outgoing_graylist(orq, sr->sr_done);
    }
  }

  return outgoing_resolve_next(orq);
}

/** Graylist SRV records */
static void outgoing_graylist(nta_outgoing_t *orq, struct sipdns_query *sq)
{
  struct sipdns_resolver *sr = orq->orq_resolver;
  char const *target = sq->sq_domain, *proto = sq->sq_proto;
  unsigned prio = sq->sq_priority, maxprio = prio;

  /* Don't know how to graylist but SRV records */
  if (sq->sq_otype != sres_type_srv)
    return;

  SU_DEBUG_5(("nta: graylisting %s:%s;transport=%s\n", target, sq->sq_port, proto));

  for (sq = sr->sr_head; sq; sq = sq->sq_next)
    if (sq->sq_otype == sres_type_srv && sq->sq_priority > maxprio)
      maxprio = sq->sq_priority;

  for (sq = sr->sr_done; sq; sq = sq->sq_next)
    if (sq->sq_otype == sres_type_srv && sq->sq_priority > maxprio)
      maxprio = sq->sq_priority;

  for (sq = sr->sr_done; sq; sq = sq->sq_next) {
    int modified;

    if (sq->sq_type != sres_type_srv || strcmp(proto, sq->sq_proto))
      continue;

    /* modify the SRV record(s) corresponding to the latest A/AAAA record */
    modified = sres_set_cached_srv_priority(
      orq->orq_agent->sa_resolver,
      sq->sq_domain,
      target,
      sq->sq_port[0] ? (uint16_t)strtoul(sq->sq_port, NULL, 10) : 0,
      orq->orq_agent->sa_graylist,
      maxprio + 1);

    if (modified >= 0)
      SU_DEBUG_3(("nta: reduced priority of %d %s SRV records (increase value to %u)\n",
		  modified, sq->sq_domain, maxprio + 1));
    else
      SU_DEBUG_3(("nta: failed to reduce %s SRV priority\n", sq->sq_domain));
  }
}

/** Cancel resolver query */
su_inline void outgoing_cancel_resolver(nta_outgoing_t *orq)
{
  struct sipdns_resolver *sr = orq->orq_resolver;

  assert(orq->orq_resolver);

  if (sr->sr_query)    /* Cancel resolver query */
      sres_query_bind(sr->sr_query, NULL, NULL), sr->sr_query = NULL;
}

/** Destroy resolver */
su_inline void outgoing_destroy_resolver(nta_outgoing_t *orq)
{
  struct sipdns_resolver *sr = orq->orq_resolver;

  assert(orq->orq_resolver);

  if (sr->sr_query)    /* Cancel resolver query */
    sres_query_bind(sr->sr_query, NULL, NULL), sr->sr_query = NULL;

  su_free(orq->orq_agent->sa_home, sr);

  orq->orq_resolver = NULL;
}

/** Check if we are resolving. If not, return 503 response. */
static
int outgoing_resolving(nta_outgoing_t *orq)
{
  struct sipdns_resolver *sr = orq->orq_resolver;

  assert(orq->orq_resolver);

  if (!sr->sr_query) {
    return outgoing_resolving_error(orq, SIPDNS_503_ERROR);
  }
  else {
    outgoing_queue(orq->orq_agent->sa_out.resolving, orq);
    return 0;
  }
}

/** Return 503 response */
static
int outgoing_resolving_error(nta_outgoing_t *orq, int status, char const *phrase)
{
  orq->orq_resolved = 1;
  outgoing_reply(orq, status, phrase, 0);
  return -1;
}

/* Query SRV records (with the given tport). */
static
int outgoing_make_srv_query(nta_outgoing_t *orq)
{
  struct sipdns_resolver *sr = orq->orq_resolver;
  su_home_t *home = msg_home(orq->orq_request);
  struct sipdns_query *sq;
  char const *host, *prefix;
  int i;
  size_t hlen, plen;

  sr->sr_use_srv = 0;

  host = sr->sr_tpn->tpn_host;
  hlen = strlen(host) + 1;

  for (i = 0; sr->sr_tports[i]; i++) {
    if (sr->sr_tport && sr->sr_tports[i] != sr->sr_tport)
      continue;

    prefix = sr->sr_tports[i]->prefix;
    plen = strlen(prefix);

    sq = su_zalloc(home, (sizeof *sq) + plen + hlen);
    if (sq) {
      *sr->sr_tail = sq, sr->sr_tail = &sq->sq_next;
      sq->sq_domain = memcpy(sq + 1, prefix, plen);
      memcpy((char *)sq->sq_domain + plen, host, hlen);
      sq->sq_proto = sr->sr_tports[i]->name;
      sq->sq_type = sres_type_srv;
      sq->sq_priority = 1;
      sq->sq_weight = 1;
    }
  }

  outgoing_query_all(orq);

  return 0;
}

/* Query A/AAAA records.  */
static
int outgoing_make_a_aaaa_query(nta_outgoing_t *orq)
{
  struct sipdns_resolver *sr = orq->orq_resolver;
  su_home_t *home = msg_home(orq->orq_request);
  tp_name_t *tpn = orq->orq_tpn;
  struct sipdns_query *sq;

  assert(sr);

  sr->sr_use_a_aaaa = 0;

  sq = su_zalloc(home, 2 * (sizeof *sq));
  if (!sq)
    return outgoing_resolving(orq);

  sq->sq_type = sr->sr_a_aaaa1;
  sq->sq_domain = tpn->tpn_host;
  sq->sq_priority = 1;

  /* Append */
  *sr->sr_tail = sq, sr->sr_tail = &sq->sq_next;

  outgoing_query_all(orq);

  return 0;
}


/** Start SRV/A/AAAA queries */
static
void outgoing_query_all(nta_outgoing_t *orq)
{
  struct sipdns_resolver *sr = orq->orq_resolver;
  struct sipdns_query *sq = sr->sr_head;

  if (sq == NULL) {
    outgoing_resolving_error(orq, SIP_500_INTERNAL_SERVER_ERROR);
    return;
  }

  /* Remove from intermediate list */
  if (!(sr->sr_head = sq->sq_next))
    sr->sr_tail = &sr->sr_head;

  if (sq->sq_type == sres_type_srv)
    outgoing_query_srv(orq, sq);
#if SU_HAVE_IN6
  else if (sq->sq_type == sres_type_aaaa)
    outgoing_query_aaaa(orq, sq);
#endif
  else if (sq->sq_type == sres_type_a)
    outgoing_query_a(orq, sq);
  else
    outgoing_resolving_error(orq, SIP_500_INTERNAL_SERVER_ERROR);
}

/** Query NAPTR record. */
static
int outgoing_query_naptr(nta_outgoing_t *orq, char const *domain)
{
  struct sipdns_resolver *sr = orq->orq_resolver;
  sres_record_t **answers;

  sr->sr_use_naptr = 0;

  sr->sr_target = domain;

  answers = sres_cached_answers(orq->orq_agent->sa_resolver,
				sres_type_naptr, domain);

  SU_DEBUG_5(("nta: for \"%s\" query \"%s\" %s%s\n",
              orq->orq_tpn->tpn_host, domain, "NAPTR",
              answers ? " (cached)" : ""));

  if (answers) {
    outgoing_answer_naptr(orq, NULL, answers);
    return 0;
  }
  else {
    sr->sr_query = sres_query(orq->orq_agent->sa_resolver,
			      outgoing_answer_naptr, orq,
			      sres_type_naptr, domain);
    return outgoing_resolving(orq);
  }
}

/* Process NAPTR records */
static
void outgoing_answer_naptr(sres_context_t *orq,
			   sres_query_t *q,
			   sres_record_t *answers[])
{
  int i, order = -1;
  size_t rlen;
  su_home_t *home = msg_home(orq->orq_request);
  struct sipdns_resolver *sr = orq->orq_resolver;
  tp_name_t tpn[1];
  struct sipdns_query *sq, *selected = NULL, **tail = &selected, **at;

  assert(sr);

  sr->sr_query = NULL;

  *tpn = *sr->sr_tpn;

  /* The NAPTR results are sorted first by Order then by Preference */
  sres_sort_answers(orq->orq_agent->sa_resolver, answers);

  if (sr->sr_tport == NULL)
    sr->sr_tport = outgoing_naptr_tport(orq, answers);

  for (i = 0; answers && answers[i]; i++) {
    sres_naptr_record_t const *na = answers[i]->sr_naptr;
    uint16_t type;
    int valid_tport;

    if (na->na_record->r_status)
      continue;
    if (na->na_record->r_type != sres_type_naptr)
      continue;

    /* Check if NAPTR matches our target */
    if (!su_casenmatch(na->na_services, "SIP+", 4) &&
	!su_casenmatch(na->na_services, "SIPS+", 5))
      /* Not a SIP/SIPS service */
      continue;

    /* Use NAPTR results, don't try extra SRV/A/AAAA records */
    sr->sr_use_srv = 0, sr->sr_use_a_aaaa = 0;

    valid_tport = sr->sr_tport &&
      su_casematch(na->na_services, sr->sr_tport->service);

    SU_DEBUG_5(("nta: %s IN NAPTR %u %u \"%s\" \"%s\" \"%s\" %s%s\n",
		na->na_record->r_name,
		na->na_order, na->na_prefer,
		na->na_flags, na->na_services,
		na->na_regexp, na->na_replace,
		order >= 0 && order != na->na_order ? " (out of order)" :
		valid_tport ? "" : " (tport not used)"));

    /* RFC 2915 p 4:
     * Order
     *    A 16-bit unsigned integer specifying the order in which the
     *    NAPTR records MUST be processed to ensure the correct ordering
     *    of rules. Low numbers are processed before high numbers, and
     *    once a NAPTR is found whose rule "matches" the target, the
     *    client MUST NOT consider any NAPTRs with a higher value for
     *    order (except as noted below for the Flags field).
     */
    if (order >= 0 && order != na->na_order)
      continue;
    if (!valid_tport)
      continue;

    /* OK, we found matching NAPTR */
    order = na->na_order;

    /*
     * The "S" flag means that the next lookup should be for SRV records
     * ... "A" means that the next lookup should be for either an A, AAAA,
     * or A6 record.
     */
    if (na->na_flags[0] == 's' || na->na_flags[0] == 'S')
      type = sres_type_srv; /* SRV */
    else if (na->na_flags[0] == 'a' || na->na_flags[0] == 'A')
      type = sr->sr_a_aaaa1; /* A / AAAA */
    else
      continue;

    rlen = strlen(na->na_replace) + 1;
    sq = su_zalloc(home, (sizeof *sq) + rlen);

    if (sq == NULL)
      continue;

    *tail = sq, tail = &sq->sq_next;
    sq->sq_otype = sres_type_naptr;
    sq->sq_priority = na->na_prefer;
    sq->sq_weight = 1;
    sq->sq_type = type;
    sq->sq_domain = memcpy(sq + 1, na->na_replace, rlen);
    sq->sq_proto = sr->sr_tport->name;
  }

  sres_free_answers(orq->orq_agent->sa_resolver, answers);

  /* RFC2915:
     Preference [...] specifies the order in which NAPTR
     records with equal "order" values SHOULD be processed, low
     numbers being processed before high numbers. */
  at = sr->sr_tail;
  while (selected) {
    sq = selected, selected = sq->sq_next;

    for (tail = at; *tail; tail = &(*tail)->sq_next) {
      if (sq->sq_priority < (*tail)->sq_priority)
	break;
      if (sq->sq_priority == (*tail)->sq_priority &&
	  sq->sq_weight < (*tail)->sq_weight)
	break;
    }
    /* Insert */
    sq->sq_next = *tail, *tail = sq;

    if (!sq->sq_next)		/* Last one */
      sr->sr_tail = &sq->sq_next;
  }

  outgoing_resolve_next(orq);
}

/* Find first supported protocol in order and preference */
struct sipdns_tport const *
outgoing_naptr_tport(nta_outgoing_t *orq, sres_record_t *answers[])
{
  int i, j, order, pref;
  int orders[SIPDNS_TRANSPORTS], prefs[SIPDNS_TRANSPORTS];
  struct sipdns_tport const *tport;

  struct sipdns_resolver *sr = orq->orq_resolver;

  for (j = 0; sr->sr_tports[j]; j++) {
    tport = sr->sr_tports[j];

    orders[j] = 65536, prefs[j] = 65536;

    /* Find transport order */
    for (i = 0; answers && answers[i]; i++) {
      sres_naptr_record_t const *na = answers[i]->sr_naptr;
      if (na->na_record->r_status)
	continue;
      if (na->na_record->r_type != sres_type_naptr)
	continue;
      /* Check if NAPTR matches transport */
      if (!su_casematch(na->na_services, tport->service))
	continue;
      orders[j] = na->na_order;
      prefs[j] = na->na_prefer;
      break;
    }
  }

  tport = sr->sr_tports[0], order = orders[0], pref = prefs[0];

  for (j = 1; sr->sr_tports[j]; j++) {
    if (orders[j] <= order && prefs[j] < pref) {
      tport = sr->sr_tports[j], order = orders[j], pref = prefs[j];
    }
  }

  return tport;
}


/* Query SRV records */
static
int outgoing_query_srv(nta_outgoing_t *orq,
		       struct sipdns_query *sq)
{
  struct sipdns_resolver *sr = orq->orq_resolver;

  sres_record_t **answers;

  sr->sr_target = sq->sq_domain;
  sr->sr_current = sq;

  answers = sres_cached_answers(orq->orq_agent->sa_resolver,
				sres_type_srv, sq->sq_domain);

  SU_DEBUG_5(("nta: for \"%s\" query \"%s\" %s%s\n",
              orq->orq_tpn->tpn_host, sq->sq_domain, "SRV",
              answers ? " (cached)" : ""));

  if (answers) {
    outgoing_answer_srv(orq, NULL, answers);
    return 0;
  }
  else {
    sr->sr_query = sres_query(orq->orq_agent->sa_resolver,
			      outgoing_answer_srv, orq,
			      sres_type_srv, sq->sq_domain);
    return outgoing_resolving(orq);
  }
}

/* Process SRV records */
static
void
outgoing_answer_srv(sres_context_t *orq, sres_query_t *q,
		    sres_record_t *answers[])
{
  struct sipdns_resolver *sr = orq->orq_resolver;
  su_home_t *home = msg_home(orq->orq_request);
  struct sipdns_query *sq0, *sq, *selected = NULL, **tail = &selected, **at;
  int i;
  size_t tlen;

  sr->sr_query = NULL;

  sq0 = sr->sr_current;
  assert(sq0 && sq0->sq_type == sres_type_srv);
  assert(sq0->sq_domain); assert(sq0->sq_proto);

  /* Sort by priority, weight? */
  sres_sort_answers(orq->orq_agent->sa_resolver, answers);

  for (i = 0; answers && answers[i]; i++) {
    sres_srv_record_t const *srv = answers[i]->sr_srv;

    if (srv->srv_record->r_status /* There was an error */ ||
        srv->srv_record->r_type != sres_type_srv)
      continue;

    tlen = strlen(srv->srv_target) + 1;

    sq = su_zalloc(home, (sizeof *sq) + tlen);

    if (sq) {
      *tail = sq, tail = &sq->sq_next;

      sq->sq_otype = sres_type_srv;
      sq->sq_type = sr->sr_a_aaaa1;
      sq->sq_proto = sq0->sq_proto;
      sq->sq_domain = memcpy(sq + 1, srv->srv_target, tlen);
      snprintf(sq->sq_port, sizeof(sq->sq_port), "%u", srv->srv_port);
      sq->sq_priority = srv->srv_priority;
      sq->sq_weight = srv->srv_weight;
    }
  }

  sres_free_answers(orq->orq_agent->sa_resolver, answers);

  at = &sr->sr_head;

  /* Insert sorted by priority, randomly select by weigth */
  while (selected) {
    unsigned long weight = 0;
    unsigned N = 0;
    uint16_t priority = selected->sq_priority;

    /* Total weight of entries with same priority */
    for (sq = selected; sq && priority == sq->sq_priority; sq = sq->sq_next) {
      weight += sq->sq_weight;
      N ++;
    }

    tail = &selected;

    /* Select by weighted random. Entries with weight 0 are kept in order */
    if (N > 1 && weight > 0) {
      unsigned rand = su_randint(0,  weight - 1);

      while (rand >= (*tail)->sq_weight) {
	rand -= (*tail)->sq_weight;
	tail = &(*tail)->sq_next;
      }
    }

    /* Remove selected */
    sq = *tail; *tail = sq->sq_next; assert(sq->sq_priority == priority);

    /* Append at *at */
    sq->sq_next = *at; *at = sq; at = &sq->sq_next; if (!*at) sr->sr_tail = at;

    SU_DEBUG_5(("nta: %s IN SRV %u %u  %s %s (%s)\n",
		sq0->sq_domain,
		(unsigned)sq->sq_priority, (unsigned)sq->sq_weight,
		sq->sq_port, sq->sq_domain, sq->sq_proto));
  }

  /* This is not needed anymore (?) */
  sr->sr_current = NULL;
  sq0->sq_next = sr->sr_done; sr->sr_done = sq0;

  outgoing_resolve_next(orq);
}

#if SU_HAVE_IN6
/* Query AAAA records */
static
int outgoing_query_aaaa(nta_outgoing_t *orq, struct sipdns_query *sq)
{
  struct sipdns_resolver *sr = orq->orq_resolver;
  sres_record_t **answers;

  sr->sr_target = sq->sq_domain;
  sr->sr_current = sq;

  answers = sres_cached_answers(orq->orq_agent->sa_resolver,
				sres_type_aaaa, sq->sq_domain);

  SU_DEBUG_5(("nta: for \"%s\" query \"%s\" %s%s\n",
              orq->orq_tpn->tpn_host, sq->sq_domain, "AAAA",
              answers ? " (cached)" : ""));

  if (answers) {
    outgoing_answer_aaaa(orq, NULL, answers);
    return 0;
  }

  sr->sr_query = sres_query(orq->orq_agent->sa_resolver,
			      outgoing_answer_aaaa, orq,
			      sres_type_aaaa, sq->sq_domain);

  return outgoing_resolving(orq);
}

/* Process AAAA records */
static
void outgoing_answer_aaaa(sres_context_t *orq, sres_query_t *q,
			  sres_record_t *answers[])
{
  struct sipdns_resolver *sr = orq->orq_resolver;
  su_home_t *home = msg_home(orq->orq_request);
  struct sipdns_query *sq = sr->sr_current;

  size_t i, j, found;
  char *result, **results = NULL;

  assert(sq); assert(sq->sq_type == sres_type_aaaa);

  sr->sr_query = NULL;

  for (i = 0, found = 0; answers && answers[i]; i++) {
    sres_aaaa_record_t const *aaaa = answers[i]->sr_aaaa;
    if (aaaa->aaaa_record->r_status == 0 &&
        aaaa->aaaa_record->r_type == sres_type_aaaa)
      found++;
  }

  if (found > 1)
    results = su_zalloc(home, (found + 1) * (sizeof *results));
  else if (found)
    results = &result;

  for (i = j = 0; results && answers && answers[i]; i++) {
    char addr[SU_ADDRSIZE];
    sres_aaaa_record_t const *aaaa = answers[i]->sr_aaaa;

    if (aaaa->aaaa_record->r_status ||
        aaaa->aaaa_record->r_type != sres_type_aaaa)
      continue;			      /* There was an error */

    su_inet_ntop(AF_INET6, &aaaa->aaaa_addr, addr, sizeof(addr));

    if (j == 0)
      SU_DEBUG_5(("nta(%p): %s IN AAAA %s\n", (void *)orq,
		  aaaa->aaaa_record->r_name, addr));
    else
      SU_DEBUG_5(("nta(%p):  AAAA %s\n", (void *)orq, addr));

    assert(j < found);
    results[j++] = su_strdup(home, addr);
  }

  sres_free_answers(orq->orq_agent->sa_resolver, answers);

  outgoing_query_results(orq, sq, results, found);
}
#endif /* SU_HAVE_IN6 */

/* Query A records */
static
int outgoing_query_a(nta_outgoing_t *orq, struct sipdns_query *sq)
{
  struct sipdns_resolver *sr = orq->orq_resolver;
  sres_record_t **answers;

  sr->sr_target = sq->sq_domain;
  sr->sr_current = sq;

  answers = sres_cached_answers(orq->orq_agent->sa_resolver,
				sres_type_a, sq->sq_domain);

  SU_DEBUG_5(("nta: for \"%s\" query \"%s\" %s%s\n",
	      orq->orq_tpn->tpn_host, sq->sq_domain, "A",
	      answers ? " (cached)" : ""));

  if (answers) {
    outgoing_answer_a(orq, NULL, answers);
    return 0;
  }

  sr->sr_query = sres_query(orq->orq_agent->sa_resolver,
			      outgoing_answer_a, orq,
			      sres_type_a, sq->sq_domain);

  return outgoing_resolving(orq);
}

/* Process A records */
static
void outgoing_answer_a(sres_context_t *orq, sres_query_t *q,
		       sres_record_t *answers[])
{
  struct sipdns_resolver *sr = orq->orq_resolver;
  su_home_t *home = msg_home(orq->orq_request);
  struct sipdns_query *sq = sr->sr_current;

  int i, j, found;
  char *result, **results = NULL;

  assert(sq); assert(sq->sq_type == sres_type_a);

  sr->sr_query = NULL;

  for (i = 0, found = 0; answers && answers[i]; i++) {
    sres_a_record_t const *a = answers[i]->sr_a;
    if (a->a_record->r_status == 0 &&
        a->a_record->r_type == sres_type_a)
      found++;
  }

  if (found > 1)
    results = su_zalloc(home, (found + 1) * (sizeof *results));
  else if (found)
    results = &result;

  for (i = j = 0; answers && answers[i]; i++) {
    char addr[SU_ADDRSIZE];
    sres_a_record_t const *a = answers[i]->sr_a;

    if (a->a_record->r_status ||
	a->a_record->r_type != sres_type_a)
      continue;			      /* There was an error */

    su_inet_ntop(AF_INET, &a->a_addr, addr, sizeof(addr));

    if (j == 0)
      SU_DEBUG_5(("nta: %s IN A %s\n", a->a_record->r_name, addr));
    else
      SU_DEBUG_5(("nta(%p):  A %s\n", (void *)orq, addr));

    assert(j < found);
    results[j++] = su_strdup(home, addr);
  }

  sres_free_answers(orq->orq_agent->sa_resolver, answers);

  outgoing_query_results(orq, sq, results, found);
}

/** Store A/AAAA query results */
static void
outgoing_query_results(nta_outgoing_t *orq,
		       struct sipdns_query *sq,
		       char *results[],
		       size_t rlen)
{
  struct sipdns_resolver *sr = orq->orq_resolver;

  if (sq->sq_type == sr->sr_a_aaaa1 &&
      sq->sq_type != sr->sr_a_aaaa2) {
    sq->sq_type = sr->sr_a_aaaa2;

    SU_DEBUG_7(("nta(%p): %s %s record still unresolved\n", (void *)orq,
		sq->sq_domain, sq->sq_type == sres_type_a ? "A" : "AAAA"));

    /*
     * Three possible policies:
     * 1) try each host for AAAA/A, then A/AAAA
     * 2) try everything first for AAAA/A, then everything for A/AAAA
     * 3) try one SRV record results for AAAA/A, then for A/AAAA,
     *    then next SRV record
     */

    /* We use now policy #1 */
    if (!(sq->sq_next = sr->sr_head))
      sr->sr_tail = &sq->sq_next;
    sr->sr_head = sq;
  }
  else {
    sq->sq_next = sr->sr_done, sr->sr_done = sq;

    if (rlen == 0 && sq->sq_grayish)
      outgoing_graylist(orq, sq);
  }

  if (rlen > 1)
    sr->sr_results = results;
  else
    sr->sr_current = NULL;

  if (rlen > 0) {
    orq->orq_resolved = 1;
    orq->orq_tpn->tpn_host = results[0];
    if (sq->sq_proto) orq->orq_tpn->tpn_proto = sq->sq_proto;
    if (sq->sq_port[0]) orq->orq_tpn->tpn_port = sq->sq_port;
    outgoing_prepare_send(orq);
  } else {
    outgoing_resolve_next(orq);
  }
}


#endif

/* ====================================================================== */
/* 10) Reliable responses */

static nta_prack_f nta_reliable_destroyed;

/**
 * Check that server transaction can be used to send reliable provisional
 * responses.
 */
su_inline
int reliable_check(nta_incoming_t *irq)
{
  if (irq == NULL || irq->irq_status >= 200 || !irq->irq_agent)
    return 0;

  if (irq->irq_reliable && irq->irq_reliable->rel_status >= 200)
    return 0;

  /* @RSeq is initialized to nonzero when request requires/supports 100rel */
  if (irq->irq_rseq == 0)
    return 0;

  if (irq->irq_rseq == 0xffffffffU) /* already sent >> 2**31 responses */
    return 0;

  return 1;
}

/** Respond reliably.
 *
 * @param irq
 * @param callback
 * @param rmagic
 * @param status
 * @param phrase
 * @param tag, value, ..
 */
nta_reliable_t *nta_reliable_treply(nta_incoming_t *irq,
				    nta_prack_f *callback,
				    nta_reliable_magic_t *rmagic,
				    int status, char const *phrase,
				    tag_type_t tag,
				    tag_value_t value, ...)
{
  ta_list ta;
  msg_t *msg;
  sip_t *sip;
  nta_reliable_t *retval = NULL;

  if (!reliable_check(irq) || (status <= 100 || status >= 200))
    return NULL;

  msg = nta_msg_create(irq->irq_agent, 0);
  sip = sip_object(msg);

  if (!sip)
    return NULL;

  ta_start(ta, tag, value);

  if (0 > nta_incoming_complete_response(irq, msg, status, phrase,
					 ta_tags(ta)))
    msg_destroy(msg);
  else if (!(retval = reliable_mreply(irq, callback, rmagic, msg, sip)))
    msg_destroy(msg);

  ta_end(ta);

  return retval;
}

/** Respond reliably with @a msg.
 *
 * @note
 * The stack takes over the ownership of @a msg. (It is destroyed even if
 * sending the response fails.)
 *
 * @param irq
 * @param callback
 * @param rmagic
 * @param msg
 */
nta_reliable_t *nta_reliable_mreply(nta_incoming_t *irq,
				    nta_prack_f *callback,
				    nta_reliable_magic_t *rmagic,
				    msg_t *msg)
{
  sip_t *sip = sip_object(msg);

  if (!reliable_check(irq)) {
    msg_destroy(msg);
    return NULL;
  }

  if (sip == NULL || !sip->sip_status || sip->sip_status->st_status <= 100) {
    msg_destroy(msg);
    return NULL;
  }

  if (sip->sip_status->st_status >= 200) {
    incoming_final_failed(irq, msg);
    return NULL;
  }

  return reliable_mreply(irq, callback, rmagic, msg, sip);
}

static
nta_reliable_t *reliable_mreply(nta_incoming_t *irq,
				nta_prack_f *callback,
				nta_reliable_magic_t *rmagic,
				msg_t *msg,
				sip_t *sip)
{
  nta_reliable_t *rel;
  nta_agent_t *agent;

  agent = irq->irq_agent;

  if (callback == NULL)
    callback = nta_reliable_destroyed;

  rel = su_zalloc(agent->sa_home, sizeof(*rel));
  if (rel) {
    rel->rel_irq = irq;
    rel->rel_callback = callback;
    rel->rel_magic = rmagic;
    rel->rel_unsent = msg;
    rel->rel_status = sip->sip_status->st_status;
    rel->rel_precious = sip->sip_payload != NULL;
    rel->rel_next = irq->irq_reliable;

    /*
     * If there already is a un-pr-acknowledged response, queue this one
     * until at least one response is pr-acknowledged.
     */
    if (irq->irq_reliable &&
	(irq->irq_reliable->rel_next == NULL ||
	 irq->irq_reliable->rel_rseq == 0)) {
      return irq->irq_reliable = rel;
    }

    if (reliable_send(irq, rel, msg_ref_create(msg), sip) < 0) {
      msg_destroy(msg);
      su_free(agent->sa_home, rel);
      return NULL;
    }

    irq->irq_reliable = rel;

    return callback ? rel : (nta_reliable_t *)-1;
  }

  msg_destroy(msg);
  return NULL;
}

static
int reliable_send(nta_incoming_t *irq,
		  nta_reliable_t *rel,
		  msg_t *msg,
		  sip_t *sip)
{
  nta_agent_t *sa = irq->irq_agent;
  su_home_t *home = msg_home(msg);
  sip_rseq_t rseq[1];
  sip_rseq_init(rseq);

  if (sip->sip_require)
    msg_header_replace_param(home, sip->sip_require->k_common, "100rel");
  else
    sip_add_make(msg, sip, sip_require_class, "100rel");

  rel->rel_rseq = rseq->rs_response = irq->irq_rseq;
  sip_add_dup(msg, sip, (sip_header_t *)rseq);

  if (!sip->sip_rseq || incoming_reply(irq, msg, sip) < 0) {
    msg_destroy(msg);
    return -1;
  }

  irq->irq_rseq++;

  if (irq->irq_queue == sa->sa_in.preliminary)
    /* Make sure we are moved to the tail */
    incoming_remove(irq);

  incoming_queue(sa->sa_in.preliminary, irq); /* P1 */
  incoming_set_timer(irq, sa->sa_t1); /* P2 */

  return 0;
}

/** Queue final response when there are unsent precious preliminary responses */
static
int reliable_final(nta_incoming_t *irq, msg_t *msg, sip_t *sip)
{
  nta_reliable_t *r;
  unsigned already_in_callback;
  /*
   * We delay sending final response if it's 2XX and
   * an unpracked reliable response contains session description
   */
  /* Get last unpracked response from queue */
  if (sip->sip_status->st_status < 300)
    for (r = irq->irq_reliable; r; r = r->rel_next)
      if (r->rel_unsent && r->rel_precious) {
	/* Delay sending 2XX */
	reliable_mreply(irq, NULL, NULL, msg, sip);
	return 0;
      }

  /* Flush unsent responses. */
  already_in_callback = irq->irq_in_callback;
  irq->irq_in_callback = 1;
  reliable_flush(irq);
  irq->irq_in_callback = already_in_callback;

  if (!already_in_callback && irq->irq_terminated && irq->irq_destroyed) {
    incoming_free(irq);
    msg_destroy(msg);
    return 0;
  }

  return 1;
}

/** Get latest reliably sent response */
static
msg_t *reliable_response(nta_incoming_t *irq)
{
  nta_reliable_t *r, *rel;

  /* Get last unpracked response from queue */
  for (rel = NULL, r = irq->irq_reliable; r; r = r->rel_next)
    if (!r->rel_pracked)
      rel = r;

  assert(rel);

  return rel->rel_unsent;
}

/* Find un-PRACKed responses */
static
nta_reliable_t *reliable_find(nta_agent_t const *agent,
			      sip_t const *sip)
{
  incoming_htable_t const *iht = agent->sa_incoming;
  nta_incoming_t *irq, **ii;
  sip_call_id_t const *i = sip->sip_call_id;
  sip_rack_t const *rack = sip->sip_rack;
  hash_value_t hash = NTA_HASH(i, rack->ra_cseq);

  /* XXX - add own hash table for 100rel */

  for (ii = incoming_htable_hash(iht, hash);
       (irq = *ii);
       ii = incoming_htable_next(iht, ii)) {

    if (hash == irq->irq_hash &&
	irq->irq_call_id->i_hash == i->i_hash &&
	irq->irq_cseq->cs_seq == rack->ra_cseq &&
	irq->irq_method == sip_method_invite &&
	strcmp(irq->irq_call_id->i_id, i->i_id) == 0 &&
	(irq->irq_to->a_tag == NULL ||
	 su_casematch(irq->irq_to->a_tag, sip->sip_to->a_tag)) &&
	su_casematch(irq->irq_from->a_tag, sip->sip_from->a_tag)) {

      nta_reliable_t const *rel;

      /* Found matching INVITE */
      for (rel = irq->irq_reliable; rel; rel = rel->rel_next)
	if (rel->rel_rseq == rack->ra_response)
	  return (nta_reliable_t  *)rel;

      return NULL;
    }
  }

  return NULL;
}

/** Process incoming PRACK with matching @RAck field */
static
int reliable_recv(nta_reliable_t *rel, msg_t *msg, sip_t *sip, tport_t *tp)
{
  nta_incoming_t *irq = rel->rel_irq;
  nta_incoming_t *pr_irq;
  int status;

  rel->rel_pracked = 1;
  msg_ref_destroy(rel->rel_unsent), rel->rel_unsent = NULL;

  pr_irq = incoming_create(irq->irq_agent, msg, sip, tp, irq->irq_tag);
  if (!pr_irq) {
    mreply(irq->irq_agent, NULL,
	   SIP_500_INTERNAL_SERVER_ERROR, msg,
	   tp, 0, 0, NULL,
	   TAG_END());
    return 0;
  }

  if (irq->irq_status < 200) {
    incoming_queue(irq->irq_agent->sa_in.proceeding, irq); /* Reset P1 */
    incoming_reset_timer(irq);	/* Reset P2 */
  }

  irq->irq_in_callback = pr_irq->irq_in_callback = 1;
  status = rel->rel_callback(rel->rel_magic, rel, pr_irq, sip); rel = NULL;
  irq->irq_in_callback = pr_irq->irq_in_callback = 0;

  if (pr_irq->irq_completed) {	/* Already sent final response */
    if (pr_irq->irq_terminated && pr_irq->irq_destroyed)
      incoming_free(pr_irq);
  }
  else if (status != 0) {
    if (status < 200 || status > 299) {
      SU_DEBUG_3(("nta_reliable(): invalid status %03d from callback\n",
		  status));
      status = 200;
    }
    nta_incoming_treply(pr_irq, status, "OK", TAG_END());
    nta_incoming_destroy(pr_irq);
  }

  /* If there are queued unsent reliable responses, send them all. */
  while (irq->irq_reliable && irq->irq_reliable->rel_rseq == 0) {
    nta_reliable_t *r;

    for (r = irq->irq_reliable; r; r = r->rel_next)
      if (r->rel_rseq == 0)
	rel = r;

    msg = rel->rel_unsent, sip = sip_object(msg);

    if (sip->sip_status->st_status < 200) {
      if (reliable_send(irq, rel, msg_ref_create(msg), sip) < 0) {
	assert(!"send reliable response");
      }
    }
    else {
      /*
       * XXX
       * Final response should be delayed until a reliable provisional
       * response has been pracked
       */
      rel->rel_unsent = NULL, rel->rel_rseq = (uint32_t)-1;
      if (incoming_reply(irq, msg, sip) < 0) {
	assert(!"send delayed final response");
      }
    }
  }

  return 0;
}

/** Flush unacknowledged and unsent reliable responses */
void reliable_flush(nta_incoming_t *irq)
{
  nta_reliable_t *r, *rel;

  do {
    for (r = irq->irq_reliable, rel = NULL; r; r = r->rel_next)
      if (r->rel_unsent)
	rel = r;

    if (rel) {
      rel->rel_pracked = 1;
      msg_ref_destroy(rel->rel_unsent), rel->rel_unsent = NULL;
      rel->rel_callback(rel->rel_magic, rel, NULL, NULL);
    }
  } while (rel);
}

void reliable_timeout(nta_incoming_t *irq, int timeout)
{
  if (timeout)
    SU_DEBUG_5(("nta: response timeout with %u\n", irq->irq_status));

  irq->irq_in_callback = 1;

  reliable_flush(irq);

  if (irq->irq_callback)
    irq->irq_callback(irq->irq_magic, irq, NULL);

  irq->irq_in_callback = 0;

  if (!timeout)
    return;

  if (irq->irq_completed && irq->irq_destroyed)
    incoming_free(irq), irq = NULL;
  else if (irq->irq_status < 200)
    nta_incoming_treply(irq, 503, "Reliable Response Time-Out", TAG_END());
}

#if 0 /* Not needed, yet. */
/** Use this callback when normal leg callback is supposed to
 *  process incoming PRACK requests
 */
int nta_reliable_leg_prack(nta_reliable_magic_t *magic,
			   nta_reliable_t *rel,
			   nta_incoming_t *irq,
			   sip_t const *sip)
{
  nta_agent_t *agent;
  nta_leg_t *leg;
  char const *method_name;
  url_t url[1];
  int retval;

  if (irq == NULL || sip == NULL || rel == NULL ||
      sip_object(irq->irq_request) != sip)
    return 500;

  agent = irq->irq_agent;
  method_name = sip->sip_request->rq_method_name;
  *url = *sip->sip_request->rq_url; url->url_params = NULL;
  agent_aliases(agent, url, irq->irq_tport); /* canonize urls */

  if ((leg = leg_find(irq->irq_agent,
		      method_name, url,
		      sip->sip_call_id,
		      sip->sip_from->a_tag,
		      sip->sip_to->a_tag))) {
    /* Use existing dialog */
    SU_DEBUG_5(("nta: %s (%u) %s\n",
		method_name, sip->sip_cseq->cs_seq,
		"PRACK processed by default callback, too"));
    retval = leg->leg_callback(leg->leg_magic, leg, irq, sip);
  }
  else {
    retval = 500;
  }

  nta_reliable_destroy(rel);

  return retval;
}
#endif

/** Destroy a reliable response.
 *
 * Mark a reliable response object for destroyal and free it if possible.
 */
void nta_reliable_destroy(nta_reliable_t *rel)
{
  if (rel == NULL || rel == NONE)
    return;

  if (rel->rel_callback == nta_reliable_destroyed)
    SU_DEBUG_1(("%s(%p): %s\n", __func__, (void *)rel, "already destroyed"));

  rel->rel_callback = nta_reliable_destroyed;

  if (rel->rel_response)
    return;

  nta_reliable_destroyed(NULL, rel, NULL, NULL);
}

/** Free and unallocate the nta_reliable_t structure. */
static
int nta_reliable_destroyed(nta_reliable_magic_t *rmagic,
			   nta_reliable_t *rel,
			   nta_incoming_t *prack,
			   sip_t const *sip)
{
  nta_reliable_t **prev;

  assert(rel); assert(rel->rel_irq);

  for (prev = &rel->rel_irq->irq_reliable; *prev; prev = &(*prev)->rel_next)
    if (*prev == rel)
      break;

  if (!*prev) {
    assert(*prev);
    SU_DEBUG_1(("%s(%p): %s\n", __func__, (void *)rel, "not linked"));
    return 200;
  }

  *prev = rel->rel_next;

  if (rel->rel_unsent)
    msg_destroy(rel->rel_unsent), rel->rel_unsent = NULL;

  su_free(rel->rel_irq->irq_agent->sa_home, rel);

  return 200;
}

/** Validate a reliable response. */
int outgoing_recv_reliable(nta_outgoing_t *orq,
			   msg_t *msg,
			   sip_t *sip)
{
  short status = sip->sip_status->st_status;
  char const *phrase = sip->sip_status->st_phrase;
  uint32_t rseq = sip->sip_rseq->rs_response;

  SU_DEBUG_7(("nta: %03u %s is reliably received with RSeq: %u\n",
	      status, phrase, rseq));

  /* Cannot handle reliable responses unless we have a full dialog */
  if (orq->orq_rseq == 0 && !orq->orq_to->a_tag) {
    SU_DEBUG_5(("nta: %03u %s with initial RSeq: %u outside dialog\n",
		status, phrase, rseq));
    return 0;
  }

  if (rseq <= orq->orq_rseq) {
    SU_DEBUG_3(("nta: %03u %s already received (RSeq: %u, expecting %u)\n",
		status, phrase, rseq, orq->orq_rseq + 1));
    return -1;
  }

  if (orq->orq_rseq && orq->orq_rseq + 1 != rseq) {
    SU_DEBUG_3(("nta: %03d %s is not expected (RSeq: %u, expecting %u)\n",
		status, sip->sip_status->st_phrase,
		rseq, orq->orq_rseq + 1));
    return -1;
  }

  return 0;
}

/** Create a tagged fork of outgoing request.
 *
 * When a dialog-creating INVITE request is forked, each response from
 * diffent fork will create an early dialog with a distinct tag in @To
 * header. When each fork should be handled separately, a tagged INVITE
 * request can be used. It will only receive responses from the specified
 * fork. Please note that the tagged transaction should be terminated with
 * the final response from another fork, too.
 *
 * @param orq
 * @param callback
 * @param magic
 * @param to_tag
 * @param rseq
 *
 * @bug Fix the memory leak - either one of the requests is left unreleased
 * for ever.
 */
nta_outgoing_t *nta_outgoing_tagged(nta_outgoing_t *orq,
				    nta_response_f *callback,
				    nta_outgoing_magic_t *magic,
				    char const *to_tag,
				    sip_rseq_t const *rseq)
{
  nta_agent_t *agent;
  su_home_t *home;
  nta_outgoing_t *tagged;
  sip_to_t *to;

  if (orq == NULL || to_tag == NULL)
    return NULL;

  if (orq->orq_to->a_tag) {
    SU_DEBUG_1(("%s: transaction %p (CSeq: %s %u) already in dialog\n", __func__,
		(void *)orq, orq->orq_cseq->cs_method_name, orq->orq_cseq->cs_seq));
    return NULL;
  }
  if (orq->orq_method != sip_method_invite) {
    SU_DEBUG_1(("%s: transaction %p (CSeq: %s %u) cannot be tagged\n", __func__,
		(void *)orq, orq->orq_cseq->cs_method_name, orq->orq_cseq->cs_seq));
    return NULL;
  }
  if (orq->orq_status < 100) {
    SU_DEBUG_1(("%s: transaction %p (CSeq: %s %u) still calling\n", __func__,
		(void *)orq, orq->orq_cseq->cs_method_name, orq->orq_cseq->cs_seq));
    return NULL;
  }

  assert(orq->orq_agent); assert(orq->orq_request);

  agent = orq->orq_agent;
  tagged = su_zalloc(agent->sa_home, sizeof(*tagged));

  home = msg_home((msg_t *)orq->orq_request);

  tagged->orq_hash = orq->orq_hash;
  tagged->orq_agent = orq->orq_agent;
  tagged->orq_callback = callback;
  tagged->orq_magic = magic;

  tagged->orq_method = orq->orq_method;
  tagged->orq_method_name = orq->orq_method_name;
  tagged->orq_url = orq->orq_url;
  tagged->orq_from = orq->orq_from;

  sip_to_tag(home, to = sip_to_copy(home, orq->orq_to), to_tag);

  tagged->orq_to = to;
  tagged->orq_tag = to->a_tag;
  tagged->orq_cseq = orq->orq_cseq;
  tagged->orq_call_id = orq->orq_call_id;

  tagged->orq_request = msg_ref_create(orq->orq_request);
  tagged->orq_response = msg_ref_create(orq->orq_response);

  tagged->orq_status = orq->orq_status;
  tagged->orq_via_added = orq->orq_via_added;
  tagged->orq_prepared = orq->orq_prepared;
  tagged->orq_reliable = orq->orq_reliable;
  tagged->orq_sips = orq->orq_sips;
  tagged->orq_uas = orq->orq_uas;
  tagged->orq_pass_100 = orq->orq_pass_100;
  tagged->orq_must_100rel = orq->orq_must_100rel;
  tagged->orq_100rel = orq->orq_100rel;
  tagged->orq_route = orq->orq_route;
  *tagged->orq_tpn = *orq->orq_tpn;
  tagged->orq_tport = tport_ref(orq->orq_tport);
  if (orq->orq_cc)
    tagged->orq_cc = nta_compartment_ref(orq->orq_cc);
  tagged->orq_branch = orq->orq_branch;
  tagged->orq_via_branch = orq->orq_via_branch;

  if (tagged->orq_uas) {
    tagged->orq_forking = orq;
    tagged->orq_forks = orq->orq_forks;
    tagged->orq_forked = 1;
    orq->orq_forks = tagged;
  }

  outgoing_insert(agent, tagged);

  return tagged;
}

/**PRACK a provisional response.
 *
 * Create and send a PRACK request used to acknowledge a provisional
 * response.
 *
 * The request is sent using the route of the original request @a oorq.
 *
 * When NTA receives response to the prack request, it invokes the @a
 * callback function.
 *
 * @param leg         dialog object
 * @param oorq        original transaction request
 * @param callback    callback function (may be @c NULL)
 * @param magic       application context pointer
 * @param route_url   optional URL used to route transaction requests
 * @param resp        (optional) response message to be acknowledged
 * @param tag,value,... optional
 *
 * @return
 * If successful, return a pointer to newly created client transaction
 * object for PRACK request, NULL otherwise.
 *
 * @sa
 * nta_outgoing_tcreate(), nta_outgoing_tcancel(), nta_outgoing_destroy().
 */
nta_outgoing_t *nta_outgoing_prack(nta_leg_t *leg,
				   nta_outgoing_t *oorq,
				   nta_response_f *callback,
				   nta_outgoing_magic_t *magic,
				   url_string_t const *route_url,
				   sip_t const *resp,
				   tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  msg_t *msg;
  su_home_t *home;
  sip_t *sip;
  sip_to_t const *to = NULL;
  sip_route_t *route = NULL, r0[1];
  nta_outgoing_t *orq = NULL;
  sip_rack_t *rack = NULL, rack0[1];

  if (!leg || !oorq) {
    SU_DEBUG_1(("%s: invalid arguments\n", __func__));
    return NULL;
  }

  sip_rack_init(rack0);

  if (resp) {
    if (!resp->sip_status) {
      SU_DEBUG_1(("%s: invalid arguments\n", __func__));
      return NULL;
    }

    if (resp->sip_status->st_status <= 100 ||
	resp->sip_status->st_status >= 200) {
      SU_DEBUG_1(("%s: %u response cannot be PRACKed\n",
		  __func__, resp->sip_status->st_status));
      return NULL;
    }

    if (!resp->sip_rseq) {
      SU_DEBUG_1(("%s: %u response missing RSeq\n",
		__func__, resp->sip_status->st_status));
      return NULL;
    }

    if (resp->sip_rseq->rs_response <= oorq->orq_rseq) {
      SU_DEBUG_1(("%s: %u response RSeq does not match received RSeq\n",
		  __func__, resp->sip_status->st_status));
      return NULL;
    }
    if (!oorq->orq_must_100rel &&
	!sip_has_feature(resp->sip_require, "100rel")) {
      SU_DEBUG_1(("%s: %u response does not require 100rel\n",
		  __func__, resp->sip_status->st_status));
      return NULL;
    }

    if (!resp->sip_to->a_tag) {
      SU_DEBUG_1(("%s: %u response has no To tag\n",
		  __func__, resp->sip_status->st_status));
      return NULL;
    }
    if (su_strcasecmp(resp->sip_to->a_tag, leg->leg_remote->a_tag) ||
	su_strcasecmp(resp->sip_to->a_tag, oorq->orq_to->a_tag)) {
      SU_DEBUG_1(("%s: %u response To tag does not agree with dialog tag\n",
		  __func__, resp->sip_status->st_status));
      return NULL;
    }

    to = resp->sip_to;
    rack = rack0;

    rack->ra_response    = resp->sip_rseq->rs_response;
    rack->ra_cseq        = resp->sip_cseq->cs_seq;
    rack->ra_method      = resp->sip_cseq->cs_method;
    rack->ra_method_name = resp->sip_cseq->cs_method_name;
  }

  msg = nta_msg_create(leg->leg_agent, 0);
  sip = sip_object(msg); home = msg_home(msg);

  if (!sip)
    return NULL;

  if (!leg->leg_route && resp) {
    /* Insert contact into route */
    if (resp->sip_contact) {
      sip_route_init(r0)->r_url[0] = resp->sip_contact->m_url[0];
      route = sip_route_dup(home, r0);
    }

    /* Reverse record route */
    if (resp->sip_record_route) {
      sip_route_t *r, *r_next;
      for (r = sip_route_dup(home, resp->sip_record_route); r; r = r_next) {
	r_next = r->r_next, r->r_next = route, route = r;
      }
    }
  }

  ta_start(ta, tag, value);

  if (!resp) {
    tagi_t const *t;

    if ((t = tl_find(ta_args(ta), ntatag_rseq)) && t->t_value) {
      rack = rack0;
      rack->ra_response = (uint32_t)t->t_value;
    }

    if (rack) {
      rack->ra_cseq = oorq->orq_cseq->cs_seq;
      rack->ra_method = oorq->orq_cseq->cs_method;
      rack->ra_method_name = oorq->orq_cseq->cs_method_name;
    }
  }

  if (sip_add_tl(msg, sip,
		 TAG_IF(rack, SIPTAG_RACK(rack)),
		 TAG_IF(to, SIPTAG_TO(to)),
		 ta_tags(ta)) < 0)
    ;
  else if (route && sip_add_dup(msg, sip, (sip_header_t *)route) < 0)
    ;
  else if (!sip->sip_rack)
    SU_DEBUG_1(("%s: RAck header missing\n", __func__));
  else if (nta_msg_request_complete(msg, leg,
				    SIP_METHOD_PRACK,
				    (url_string_t *)oorq->orq_url) < 0)
    ;
  else
    orq = outgoing_create(leg->leg_agent, callback, magic,
			  route_url, NULL, msg, ta_tags(ta));

  ta_end(ta);

  if (!orq)
    msg_destroy(msg);
  else if (rack)
    oorq->orq_rseq = rack->ra_response;
  else if (sip->sip_rack)
    oorq->orq_rseq = sip->sip_rack->ra_response;

  return orq;
}

/** Get @RSeq value stored with client transaction. */
uint32_t nta_outgoing_rseq(nta_outgoing_t const *orq)
{
  return orq ? orq->orq_rseq : 0;
}

/** Set @RSeq value stored with client transaction.
 *
 * @return 0 if rseq was set successfully
 * @return -1 if rseq is invalid or orq is NULL.
 */
int nta_outgoing_setrseq(nta_outgoing_t *orq, uint32_t rseq)
{
  if (orq && orq->orq_rseq <= rseq) {
    orq->orq_rseq = rseq;
    return 0;
  }

  return -1;
}

/* ------------------------------------------------------------------------ */
/* 11) SigComp handling and public transport interface */

#include <sofia-sip/nta_tport.h>

/** Return the master transport for the agent.
 *
 * @NEW_1_12_11
 */
tport_t *
nta_agent_tports(nta_agent_t *agent)
{
  return agent ? agent->sa_tports : NULL;
}

su_inline tport_t *
nta_transport_(nta_agent_t *agent,
	       nta_incoming_t *irq,
	       msg_t *msg)
{
  if (irq)
    return irq->irq_tport;
  else if (agent && msg)
    return tport_delivered_by(agent->sa_tports, msg);

  errno = EINVAL;
  return NULL;
}


/** Return a new reference to the transaction transport.
 *
 * @note The referenced transport must be unreferenced with tport_unref()
 */
tport_t *
nta_incoming_transport(nta_agent_t *agent,
		       nta_incoming_t *irq,
		       msg_t *msg)
{
  return tport_ref(nta_transport_(agent, irq, msg));
}

nta_compressor_t *nta_agent_init_sigcomp(nta_agent_t *sa)
{
  if (!nta_compressor_vtable || !sa)
    return NULL;

  if (sa->sa_compressor == NULL) {
    char const * const *l = sa->sa_sigcomp_option_list;
    nta_compressor_t *comp;
    comp = nta_compressor_vtable->ncv_init_agent(sa, l);
    sa->sa_compressor = comp;
  }

  return sa->sa_compressor;
}

void nta_agent_deinit_sigcomp(nta_agent_t *sa)
{
  if (nta_compressor_vtable && sa && sa->sa_compressor) {
    nta_compressor_vtable->ncv_deinit_agent(sa, sa->sa_compressor);
    sa->sa_compressor = NULL;
  }
}

struct sigcomp_compartment *
nta_incoming_compartment(nta_incoming_t *irq)
{
  if (nta_compressor_vtable && irq && irq->irq_cc)
    return nta_compressor_vtable->ncv_compartment_ref(irq->irq_cc);
  else
    return NULL;
}

tport_t *
nta_outgoing_transport(nta_outgoing_t *orq)
{
  if (orq)
    return tport_ref(orq->orq_tport);
  else
    return NULL;
}


struct sigcomp_compartment *
nta_outgoing_compartment(nta_outgoing_t *orq)
{
  if (nta_compressor_vtable && orq && orq->orq_cc)
    return nta_compressor_vtable->ncv_compartment_ref(orq->orq_cc);
  else
    return NULL;
}


struct sigcomp_compartment *
nta_compartment_ref(struct sigcomp_compartment *cc)
{
  if (nta_compressor_vtable)
    return nta_compressor_vtable->ncv_compartment_ref(cc);
  else
    return NULL;
}

void
nta_compartment_decref(struct sigcomp_compartment **pcc)
{
  if (nta_compressor_vtable && pcc && *pcc)
    nta_compressor_vtable->ncv_compartment_unref(*pcc), *pcc = NULL;
}


/** Get compartment for connection, create it when needed. */
static
struct sigcomp_compartment *
agent_compression_compartment(nta_agent_t *sa,
			      tport_t *tp,
			      tp_name_t const *tpn,
			      int new_if_needed)
{
  if (nta_compressor_vtable) {
    char const * const *l = sa->sa_sigcomp_option_list;
    return nta_compressor_vtable->
      ncv_compartment(sa, tp, sa->sa_compressor, tpn, l, new_if_needed);
  }
  else
    return NULL;
}

static
int agent_accept_compressed(nta_agent_t *sa, msg_t *msg,
			    struct sigcomp_compartment *cc)
{
  if (nta_compressor_vtable) {
    nta_compressor_t *msc = sa->sa_compressor;
    tport_compressor_t *sc = NULL;
    if (tport_delivered_with_comp(sa->sa_tports, msg, &sc) < 0)
      return 0;
    return nta_compressor_vtable->ncv_accept_compressed(sa, msc, sc, msg, cc);
  }
  else
    return 0;
}

/** Close compressor (lose its state). */
static
int agent_close_compressor(nta_agent_t *sa,
			   struct sigcomp_compartment *cc)
{
  if (nta_compressor_vtable)
    return nta_compressor_vtable->ncv_close_compressor(sa, cc);
  return 0;
}

/** Close both compressor and decompressor */
static
int agent_zap_compressor(nta_agent_t *sa,
			 struct sigcomp_compartment *cc)
{
  if (nta_compressor_vtable)
    return nta_compressor_vtable->ncv_zap_compressor(sa, cc);
  return 0;
}

/** Bind transport update callback */
int nta_agent_bind_tport_update(nta_agent_t *agent,
				nta_update_magic_t *magic,
				nta_update_tport_f *callback)
{
  if (!agent)
    return su_seterrno(EFAULT), -1;
  agent->sa_update_magic = magic;
  agent->sa_update_tport = callback;
  return 0;
}

/** Bind transport error callback */
int nta_agent_bind_tport_error(nta_agent_t *agent,
				nta_error_magic_t *magic,
				nta_error_tport_f *callback)
{
  if (!agent)
    return su_seterrno(EFAULT), -1;
  agent->sa_error_magic = magic;
  agent->sa_error_tport = callback;
  return 0;
}

/** Check if public transport binding is in progress */
int nta_agent_tport_is_updating(nta_agent_t *agent)
{
  return agent && tport_is_updating(agent->sa_tports);
}

/** Initiate STUN keepalive controller to TPORT */
int nta_tport_keepalive(nta_outgoing_t *orq)
{
  assert(orq);

#if HAVE_SOFIA_STUN
  return tport_keepalive(orq->orq_tport, msg_addrinfo(orq->orq_request),
			 TAG_END());
#else
  return -1;
#endif
}

/** Close all transports. @since Experimental in @VERSION_1_12_2. */
int nta_agent_close_tports(nta_agent_t *agent)
{
  size_t i;
  outgoing_htable_t *oht = agent->sa_outgoing;
  incoming_htable_t *iht = agent->sa_incoming;

  for (i = oht->oht_size; i-- > 0;)
    /* while */ if (oht->oht_table[i]) {
      nta_outgoing_t *orq = oht->oht_table[i];

      if (orq->orq_pending && orq->orq_tport)
	tport_release(orq->orq_tport, orq->orq_pending, orq->orq_request,
		      NULL, orq, 0);

      orq->orq_pending = 0;
      tport_unref(orq->orq_tport), orq->orq_tport = NULL;
    }


  for (i = iht->iht_size; i-- > 0;)
    /* while */ if (iht->iht_table[i]) {
      nta_incoming_t *irq = iht->iht_table[i];
      tport_unref(irq->irq_tport), irq->irq_tport = NULL;
    }

  tport_destroy(agent->sa_tports), agent->sa_tports = NULL;

  msg_header_free(agent->sa_home, (void *)agent->sa_vias);
  agent->sa_vias = NULL;
  msg_header_free(agent->sa_home, (void *)agent->sa_public_vias);
  agent->sa_public_vias = NULL;

  return 0;
}
