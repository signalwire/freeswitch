/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2006 Nokia Corporation.
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

#ifndef NUA_DIALOG_H
/** Defined when <nua_dialog.h> has been included. */
#define NUA_DIALOG_H

/**@IFILE nua_dialog.h
 * @brief Dialog and dialog usage handling
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <Kai.Vehmanen@nokia.com>
 *
 * @date Created: Wed Mar  8 11:38:18 EET 2006  ppessi
 */

#include <nua_types.h>

#ifndef NTA_H
#include <sofia-sip/nta.h>
#endif

typedef struct {
  sip_method_t sm_method;
  char const *sm_method_name;

  int sm_event;

  struct {
    unsigned create_dialog:1, in_dialog:1, target_refresh:1, add_contact:1;
    unsigned :0;
  } sm_flags;

  /** Initialize server-side request. */
  int (*sm_init)(nua_server_request_t *sr);

  /** Preprocess server-side request (after handle has been created). */
  int (*sm_preprocess)(nua_server_request_t *sr);

  /** Update server-side request parameters */
  int (*sm_params)(nua_server_request_t *sr, tagi_t const *tags);

  /** Respond to server-side request. */
  int (*sm_respond)(nua_server_request_t *sr, tagi_t const *tags);

  /** Report server-side request to application. */
  int (*sm_report)(nua_server_request_t *sr, tagi_t const *tags);

} nua_server_methods_t;


/* Server side transaction */
struct nua_server_request {
  struct nua_server_request *sr_next, **sr_prev;

  nua_server_methods_t const *sr_methods;

  nua_owner_t *sr_owner;	/**< Backpointer to handle */
  nua_dialog_usage_t *sr_usage;	/**< Backpointer to usage */

  nta_incoming_t *sr_irq;	/**< Server transaction object */

  struct {
    msg_t *msg;			/**< Request message */
    sip_t const *sip;		/**< Headers in request message */
  } sr_request;

  struct {
    msg_t *msg;			/**< Response message */
    sip_t *sip;			/**< Headers in response message */
  } sr_response;

  sip_method_t sr_method;	/**< Request method */

  int sr_application;		/**< Status by application */

  int sr_status;		/**< Status code */
  char const *sr_phrase;	/**< Status phrase */

  unsigned sr_event:1;		/**< Reported to application */
  unsigned sr_initial:1;	/**< Handle was created by this request */
  unsigned sr_add_contact:1;	/**< Add Contact header to the response */
  unsigned sr_target_refresh:1;	/**< Refresh target */
  unsigned sr_terminating:1;	/**< Terminate usage after final response */
  unsigned sr_gracefully:1;	/**< Terminate usage gracefully */

  unsigned sr_neutral:1;	/**< No effect on session or other usage */

  /* Flags used with 100rel */
  unsigned sr_100rel:1, sr_pracked:1;

  /* Flags used with offer-answer */
  unsigned sr_offer_recv:1;	/**< We have received an offer */
  unsigned sr_answer_sent:2;	/**< We have answered (reliably, if >1) */

  unsigned sr_offer_sent:2;	/**< We have offered SDP (reliably, if >1) */
  unsigned sr_answer_recv:1;	/**< We have received SDP answer */

  unsigned :0;

  char const *sr_sdp;		/**< SDP received from client */
  size_t sr_sdp_len;		/**< SDP length */

  /**< Save 200 OK nua_respond() signal until PRACK has been received */
  nua_saved_signal_t sr_signal[1];
};

#define SR_STATUS(sr, status, phrase) \
  ((sr)->sr_phrase = (phrase), (sr)->sr_status = (status))

#define SR_STATUS1(sr, statusphrase)					\
  sr_status(sr, statusphrase)

#define SR_HAS_SAVED_SIGNAL(sr) ((sr)->sr_signal[0] != NULL)

su_inline
int sr_status(nua_server_request_t *sr, int status, char const *phrase)
{
  return (void)(sr->sr_phrase = phrase), (sr->sr_status = status);
}

/* Methods for client request. @internal */
typedef struct {
  sip_method_t crm_method;
  char const *crm_method_name;
  size_t crm_extra;		/**< Size of private data */

  struct {
    unsigned create_dialog:1, in_dialog:1, target_refresh:1;
    unsigned:0;
  } crm_flags;

  /** Generate a request message.
   *
   * @retval 1 when request message has been created
   * @retval 0 when request message should be created in normal fashion
   * @retval -1 upon an error
   */
  int (*crm_template)(nua_client_request_t *cr,
		      msg_t **return_msg,
		      tagi_t const *tags);

  /**@a crm_init is called when a client request is sent first time.
   *
   * @retval 1 when request has been responded
   * @retval 0 when request should be sent in normal fashion
   * @retval -1 upon an error
   */
  int (*crm_init)(nua_client_request_t *, msg_t *msg, sip_t *sip,
		  tagi_t const *tags);

  /** @a crm_send is called each time when a client request is sent.
   *
   * @retval 1 when request has been responded
   * @retval 0 when request has been sent
   * @retval -1 upon an error (request message has not been destroyed)
   * @retval -2 upon an error (request message has been destroyed)
   */
  int (*crm_send)(nua_client_request_t *,
		  msg_t *msg, sip_t *sip,
		  tagi_t const *tags);

  /** @a crm_check_restart is called each time when a response is received.
   *
   * It is used to restart reqquest after responses with method-specific
   * status code or method-specific way of restarting the request.
   *
   * @retval 1 when request has been restarted
   * @retval 0 when response should be processed normally
   */
  int (*crm_check_restart)(nua_client_request_t *,
			   int status, char const *phrase,
			   sip_t const *sip);

  /** @a crm_recv is called each time a final response is received.
   *
   * A final response is in range 200 .. 699 (or internal response) and it
   * cannot be restarted.
   *
   * crm_recv() should call nua_base_client_response() or
   * nua_base_client_tresponse(). The return values below are documented with
   * nua_base_client_response(), too.
   *
   * @retval 0 if response was preliminary
   * @retval 1 if response was final
   * @retval 2 if response destroyed the handle, too.
   */
  int (*crm_recv)(nua_client_request_t *,
		  int status, char const *phrase,
		  sip_t const *sip);

  /** @a crm_preliminary is called each time a preliminary response is received.
   *
   * A preliminary response is in range 101 .. 199.
   *
   * crm_preliminary() should call nua_base_client_response() or
   * nua_base_client_tresponse().
   *
   * @retval 0 if response was preliminary
   * @retval 1 if response was final
   * @retval 2 if response destroyed the handle, too.
   */
  int (*crm_preliminary)(nua_client_request_t *,
			 int status, char const *phrase,
			 sip_t const *sip);

  /** @a crm_report is called each time a response is received and it is
   * reported to the application.
   *
   * The status and phrase may be different from the status and phrase
   * received from the network, e.g., when the request is restarted.
   *
   * @return The return value should be 0. It is currently ignored.
   */
  int (*crm_report)(nua_client_request_t *,
		    int status, char const *phrase,
		    sip_t const *sip,
		    nta_outgoing_t *orq,
		    tagi_t const *tags);

  /** @a crm_complete is called when a client-side request is destroyed.
   *
   * @return The return value should be 0. It is currently ignored.
   */
  int (*crm_complete)(nua_client_request_t *);

} nua_client_methods_t;

/* Client-side request. Documented by nua_client_create() */
struct nua_client_request
{
  nua_client_request_t *cr_next, **cr_prev; /**< Linked list of requests */
  nua_owner_t        *cr_owner;
  nua_dialog_usage_t *cr_usage;

  nua_saved_signal_t cr_signal[1];
  tagi_t const      *cr_tags;

  nua_client_methods_t const *cr_methods;

  msg_t              *cr_msg;
  sip_t              *cr_sip;

  nta_outgoing_t     *cr_orq;

  su_timer_t         *cr_timer;	        /**< Expires or retry timer */

  /*nua_event_t*/ int cr_event;		/**< Request event */
  sip_method_t        cr_method;
  char const         *cr_method_name;

  url_t              *cr_target;

  char const         *cr_phrase;        /**< Latest status phrase */
  unsigned short      cr_status;        /**< Latest status */
  unsigned short      cr_retry_count;   /**< Retry count for this request */

  uint32_t            cr_seq;

  unsigned            cr_refs;	 /**< References to client request */

  /* Flags used with offer-answer */
  unsigned short      cr_answer_recv;   /**< Recv answer in response
					 *  with this status.
					 */
  unsigned cr_offer_sent:1;	/**< Sent offer in this request */

  unsigned cr_offer_recv:1;	/**< Recv offer in a response */
  unsigned cr_answer_sent:1;	/**< Sent answer in (PR)ACK */

  /* Flags with usage */
  unsigned cr_neutral:1;	/**< No effect on session or other usage */

  /* Lifelong flags? */
  unsigned cr_auto:1;		/**< Request was generated by stack */
  unsigned cr_has_contact:1;	/**< Request has user Contact */
  unsigned cr_contactize:1;	/**< Request needs Contact */
  unsigned cr_dialog:1;		/**< Request can initiate dialog */

  /* Current state */
  unsigned cr_acked:1;		/**< Final response to the request has been ACKed */
  unsigned cr_waiting:1;	/**< Request is waiting */
  unsigned cr_challenged:1;	/**< Request was challenged */
  unsigned cr_wait_for_cred:1;	/**< Request is pending authentication */
  unsigned cr_restarting:1;	/**< Request is being restarted */
  unsigned cr_reporting:1;	/**< Reporting in progress */
  unsigned cr_terminating:1;	/**< Request terminates the usage */
  signed int cr_terminated:2;	/**< Response terminated usage (1) or
				    whole dialog (-1) */
  unsigned cr_graceful:1;	/**< Graceful termination required */
};


struct nua_dialog_state
{
  /** Dialog owner */
  nua_owner_t            *ds_owner;

  /** Dialog usages. */
  nua_dialog_usage_t     *ds_usage;

  /** Client requests */
  nua_client_request_t   *ds_cr;
  /** Server requests */
  nua_server_request_t *ds_sr;

  /* Dialog and subscription state */
  unsigned ds_reporting:1;	/**< We are reporting */

  unsigned ds_route:1;		/**< We have route */
  unsigned ds_terminating:1;	/**< Being terminated */

  unsigned ds_has_session:1;	/**< We have session */
  unsigned ds_has_register:1;	/**< We have registration */
  unsigned ds_has_publish:1;	/**< We have publish */

  unsigned ds_got_session:1;	/**< We have (or have had) session */
  unsigned ds_got_referrals:1;	/**< We have (or have had) referrals */

  unsigned :0;

  unsigned ds_has_events;	/**< We have events */
  unsigned ds_has_subscribes;   /**< We have subscriptions */
  unsigned ds_has_notifys;	/**< We have notifiers */

  sip_from_t const *ds_local;		/**< Local address */
  sip_to_t const *ds_remote;		/**< Remote address */
  nta_leg_t      *ds_leg;
  sip_contact_t  *ds_ltarget;	        /**< Local target */
  char const     *ds_remote_tag;	/**< Remote tag (if any).
					 * Should be non-NULL
					 * if dialog is established.
					 */

  struct nua_dialog_peer_info {
    sip_allow_t      *nr_allow;
    sip_accept_t     *nr_accept;
    sip_require_t    *nr_require;
    sip_supported_t  *nr_supported;
    sip_user_agent_t *nr_user_agent;
  } ds_remote_ua[1];
};

/* Virtual function pointer table for dialog usage. */
typedef struct {
  unsigned usage_size, usage_class_size;
  int (*usage_add)(nua_owner_t *,
		   nua_dialog_state_t *ds,
		   nua_dialog_usage_t *du);
  void (*usage_remove)(nua_owner_t *,
		       nua_dialog_state_t *ds,
		       nua_dialog_usage_t *du,
		       nua_client_request_t *cr,
		       nua_server_request_t *sr);
  char const *(*usage_name)(nua_dialog_usage_t const *du);
  void (*usage_update_params)(nua_dialog_usage_t const *du,
			      nua_handle_preferences_t const *changed,
			      nua_handle_preferences_t const *params,
			      nua_handle_preferences_t const *defaults);
  void (*usage_peer_info)(nua_dialog_usage_t *du,
			  nua_dialog_state_t const *ds,
			  sip_t const *sip);
  void (*usage_refresh)(nua_owner_t *, nua_dialog_state_t *ds,
			nua_dialog_usage_t *, sip_time_t now);
  int (*usage_shutdown)(nua_owner_t *, nua_dialog_state_t *ds,
			nua_dialog_usage_t *);
} nua_usage_class;


/* Base structure for dialog usage. */
struct nua_dialog_usage {
  nua_dialog_usage_t *du_next;
  nua_usage_class const *du_class;
  nua_dialog_state_t *du_dialog;
  nua_client_request_t *du_cr;	        /**< Client request bound with usage */
  sip_time_t   du_refquested;	        /**< When refreshed was requested */
  sip_time_t   du_refresh;		/**< When to refresh */

  unsigned     du_ready:1;	        /**< Established usage */
  unsigned     du_shutdown:1;	        /**< Shutdown in progress */
  unsigned:0;

  /** When usage expires.
   * Non-zero if the usage is established, SIP_TIME_MAX if there no
   * expiration time.
   */

  sip_event_t const *du_event;		/**< Event of usage */

};

void nua_dialog_uac_route(nua_owner_t *, nua_dialog_state_t *ds,
			  sip_t const *sip, int rtag);
void nua_dialog_uas_route(nua_owner_t *, nua_dialog_state_t *ds,
			  sip_t const *sip, int rtag);
void nua_dialog_store_peer_info(nua_owner_t *, nua_dialog_state_t *ds,
				sip_t const *sip);
int nua_dialog_zap(nua_owner_t *own,
		   nua_dialog_state_t *ds);
int nua_dialog_remove(nua_owner_t *own,
		      nua_dialog_state_t *ds,
		      nua_dialog_usage_t *usage);

su_inline int nua_dialog_is_reporting(nua_dialog_state_t const *ds)
{
  return ds && ds->ds_reporting;
}

char const *nua_dialog_usage_name(nua_dialog_usage_t const *du);

nua_dialog_usage_t *nua_dialog_usage_add(nua_owner_t *,
					 struct nua_dialog_state *ds,
					 nua_usage_class const *uclass,
					 sip_event_t const *event);

nua_dialog_usage_t *nua_dialog_usage_get(nua_dialog_state_t const *ds,
					 nua_usage_class const *uclass,
					 sip_event_t const *event);

void nua_dialog_usage_remove(nua_owner_t *,
			     nua_dialog_state_t *ds,
			     nua_dialog_usage_t *du,
			     nua_client_request_t *cr,
			     nua_server_request_t *sr);

void nua_dialog_update_params(nua_dialog_state_t *ds,
			      nua_handle_preferences_t const *changed,
			      nua_handle_preferences_t const *params,
			      nua_handle_preferences_t const *defaults);

void nua_base_usage_update_params(nua_dialog_usage_t const *du,
				  nua_handle_preferences_t const *changed,
				  nua_handle_preferences_t const *params,
				  nua_handle_preferences_t const *defaults);

void nua_dialog_deinit(nua_owner_t *own,
		       nua_dialog_state_t *ds);

int nua_dialog_shutdown(nua_owner_t *owner, nua_dialog_state_t *ds);

int nua_dialog_repeat_shutdown(nua_owner_t *owner,
			       nua_dialog_state_t *ds);

void nua_dialog_usage_set_refresh(nua_dialog_usage_t *du, unsigned delta);

void nua_dialog_usage_set_refresh_range(nua_dialog_usage_t *du,
					unsigned min, unsigned max);

void nua_dialog_usage_set_refresh_at(nua_dialog_usage_t *du,
				     sip_time_t target);

void nua_dialog_usage_reset_refresh(nua_dialog_usage_t *du);

void nua_dialog_usage_refresh(nua_owner_t *owner,
			      nua_dialog_state_t *ds,
			      nua_dialog_usage_t *du,
			      sip_time_t now);

int nua_dialog_usage_shutdown(nua_owner_t *owner,
			      nua_dialog_state_t *ds,
			      nua_dialog_usage_t *du);

su_inline
int nua_dialog_is_established(nua_dialog_state_t const *ds)
{
  return ds->ds_remote_tag != NULL;
}

#if 0
su_inline
void *nua_dialog_usage_private(nua_dialog_usage_t const *du)
{
  return du ? (void *)(du + 1) : NULL;
}

su_inline
nua_dialog_usage_t *nua_dialog_usage_public(void const *p)
{
  return p ? (nua_dialog_usage_t *)p - 1 : NULL;
}
#else
#define nua_dialog_usage_private(du) ((du) ? (void*)((du) + 1) : NULL)
#define nua_dialog_usage_public(p) ((p) ? (nua_dialog_usage_t*)(p) - 1 : NULL)
#endif

#define NUA_DIALOG_USAGE_PRIVATE(du) ((void *)((du) + 1))
#define NUA_DIALOG_USAGE_PUBLIC(pu) ((void *)((nua_dialog_usage_t *)(pu) - 1))

/* ---------------------------------------------------------------------- */

int nua_client_create(nua_owner_t *owner,
		      int event,
		      nua_client_methods_t const *methods,
		      tagi_t const *tags);

int nua_client_tcreate(nua_owner_t *nh,
		       int event,
		       nua_client_methods_t const *methods,
		       tag_type_t tag, tag_value_t value, ...);

su_inline
void *nua_private_client_request(nua_client_request_t const *cr)
{
  return (void *)(cr + 1);
}

nua_client_request_t *nua_client_request_ref(nua_client_request_t *);
int nua_client_request_unref(nua_client_request_t *);

#if HAVE_MEMLEAK_LOG

#define nua_client_request_ref(cr) \
  nua_client_request_ref_by((cr), __FILE__, __LINE__, __func__)
#define nua_client_request_unref(cr) \
  nua_client_request_unref_by((cr), __FILE__, __LINE__, __func__)

nua_client_request_t *nua_client_request_ref_by(nua_client_request_t *,
						char const *file, unsigned line,
						char const *who);
int nua_client_request_unref_by(nua_client_request_t *,
				char const *file, unsigned line, char const *who);

#endif

int nua_client_request_queue(nua_client_request_t *cr);

su_inline int nua_client_is_queued(nua_client_request_t const *cr)
{
  return cr && cr->cr_prev;
}

int nua_client_request_remove(nua_client_request_t *cr);
int nua_client_request_clean(nua_client_request_t *cr);
int nua_client_bind(nua_client_request_t *cr, nua_dialog_usage_t *du);

su_inline int nua_client_is_bound(nua_client_request_t const *cr)
{
  return cr && cr->cr_usage && cr->cr_usage->du_cr == cr;
}

su_inline int nua_client_is_reporting(nua_client_request_t const *cr)
{
  return cr && cr->cr_reporting;
}

/** Mark client request as a terminating one */
su_inline void nua_client_set_terminating(nua_client_request_t *cr, int value)
{
  cr->cr_terminating = value != 0;
}

int nua_client_init_request(nua_client_request_t *cr);

msg_t *nua_client_request_template(nua_client_request_t *cr);

int nua_client_restart_request(nua_client_request_t *cr,
			       int terminating,
			       tagi_t const *tags);

int nua_client_resend_request(nua_client_request_t *cr,
			      int terminating);

int nua_base_client_request(nua_client_request_t *cr,
			    msg_t *msg,
			    sip_t *sip,
			    tagi_t const *tags);

int nua_base_client_trequest(nua_client_request_t *cr,
			     msg_t *msg,
			     sip_t *sip,
			     tag_type_t tag, tag_value_t value, ...);

extern nta_response_f nua_client_orq_response;

int nua_client_return(nua_client_request_t *cr,
		      int status,
		      char const *phrase,
		      msg_t *to_be_destroyed);

int nua_client_response(nua_client_request_t *cr,
			int status,
			char const *phrase,
			sip_t const *sip);

int nua_client_check_restart(nua_client_request_t *cr,
			     int status,
			     char const *phrase,
			     sip_t const *sip);

int nua_base_client_check_restart(nua_client_request_t *cr,
				  int status,
				  char const *phrase,
				  sip_t const *sip);

int nua_client_restart(nua_client_request_t *cr,
		       int status, char const *phrase);

int nua_base_client_response(nua_client_request_t *cr,
			     int status, char const *phrase,
			     sip_t const *sip,
			     tagi_t const *tags);

int nua_base_client_tresponse(nua_client_request_t *cr,
			      int status, char const *phrase,
			      sip_t const *sip,
			      tag_type_t tag, tag_value_t value, ...);

int nua_client_set_target(nua_client_request_t *cr, url_t const *target);

int nua_client_report(nua_client_request_t *cr,
		      int status, char const *phrase,
		      sip_t const *sip,
		      nta_outgoing_t *orq,
		      tagi_t const *tags);

nua_client_request_t *nua_client_request_pending(nua_client_request_t const *);

int nua_client_next_request(nua_client_request_t *cr, int invite);

/* ---------------------------------------------------------------------- */

extern nua_server_methods_t const
  nua_extension_server_methods,
  nua_invite_server_methods,	/**< INVITE */
  nua_bye_server_methods,	/**< BYE */
  nua_options_server_methods,	/**< OPTIONS */
  nua_register_server_methods,	/**< REGISTER */
  nua_info_server_methods,	/**< INFO */
  nua_prack_server_methods,	/**< PRACK */
  nua_update_server_methods,	/**< UPDATE */
  nua_message_server_methods,	/**< MESSAGE */
  nua_subscribe_server_methods, /**< SUBSCRIBE */
  nua_notify_server_methods,	/**< NOTIFY */
  nua_refer_server_methods,	/**< REFER */
  nua_publish_server_methods;	/**< PUBLISH */

/** Return true if we have not sent final response to request */
su_inline
int nua_server_request_is_pending(nua_server_request_t const *sr)
{
  return sr && sr->sr_response.msg;
}

su_inline
int nua_server_request_status(nua_server_request_t const *sr)
{
  return sr ? nta_incoming_status(sr->sr_irq) : 500;
}

void nua_server_request_destroy(nua_server_request_t *sr);

int nua_base_server_init(nua_server_request_t *sr);

#define nua_base_server_init NULL

int nua_base_server_preprocess(nua_server_request_t *sr);

#define nua_base_server_preprocess NULL

int nua_server_params(nua_server_request_t *sr, tagi_t const *tags);

int nua_base_server_params(nua_server_request_t *sr, tagi_t const *tags);

#define nua_base_server_params NULL

int nua_server_trespond(nua_server_request_t *sr,
			tag_type_t tag, tag_value_t value, ...);
int nua_server_respond(nua_server_request_t *sr, tagi_t const *tags);

int nua_base_server_trespond(nua_server_request_t *sr,
			     tag_type_t tag, tag_value_t value, ...);
int nua_base_server_respond(nua_server_request_t *sr,
			    tagi_t const *tags);

int nua_server_report(nua_server_request_t *sr);

int nua_base_server_treport(nua_server_request_t *sr,
			    tag_type_t tag, tag_value_t value, ...);
int nua_base_server_report(nua_server_request_t *sr, tagi_t const *tags);

/* ---------------------------------------------------------------------- */

#endif /* NUA_DIALOG_H */
