/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2006, 2009 Nokia Corporation.
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

#ifndef NUA_CLIENT_H
/** Defined when <nua_client.h> has been included. */
#define NUA_CLIENT_H

/**@IFILE nua_client.h
 * @brief Client requests
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <Kai.Vehmanen@nokia.com>
 *
 * @date Created: Tue Feb  3 15:50:35 EET 2009 ppessi
 */

#include <nua_types.h>

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
   * @retval -1 upon an error (but request message has not been destroyed)
   * @retval -2 upon an error
   */
  int (*crm_send)(nua_client_request_t *,
		  msg_t *msg, sip_t *sip,
		  tagi_t const *tags);

  /** @a crm_check_restart is called each time when a response is received.
   *
   * It is used to restart request after responses with method-specific
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
  unsigned cr_initial:1;	/**< Initial request of a dialog */
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

int nua_client_request_in_progress(nua_client_request_t const *cr);

int nua_client_request_complete(nua_client_request_t *cr);
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

#endif /* NUA_CLIENT_H */
