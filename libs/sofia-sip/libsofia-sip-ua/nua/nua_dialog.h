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

typedef struct nua_dialog_state nua_dialog_state_t;
typedef struct nua_dialog_usage nua_dialog_usage_t;
typedef struct nua_remote_s nua_remote_t;

#ifndef NUA_OWNER_T
#define NUA_OWNER_T struct nua_owner_s
#endif
typedef NUA_OWNER_T nua_owner_t;

#ifndef NTA_H
#include <sofia-sip/nta.h>
#endif

typedef struct nua_server_request nua_server_request_t; 
typedef struct nua_client_request nua_client_request_t; 

/** Respond to an incoming request. */
typedef int nua_server_respond_f(nua_server_request_t *, tagi_t const *);

/** Restart an outgoing request. */
typedef void nua_creq_restart_f(nua_owner_t *, tagi_t *tags);

/** Server side transaction */
struct nua_server_request {
  struct nua_server_request *sr_next, **sr_prev;

  nua_owner_t *sr_owner;	/**< Backpointer to handle */
  nua_dialog_usage_t *sr_usage;	/**< Backpointer to usage */

  /** When the application responds to an request with
   * nua_respond(), the sr_respond() is called
   */
  nua_server_respond_f *sr_respond;
  
  nta_incoming_t *sr_irq;	/**< Server transaction object */
  msg_t *sr_msg;		/**< Request message */

  sip_method_t sr_method;	/**< Request method */
  int sr_status;		/**< Status code */
  char const *sr_phrase;	/**< Status phrase */

  unsigned sr_auto:1;		/**< Autoresponse - no event has been sent */
  unsigned sr_initial:1;	/**< Handle was created by this request */

  /* Flags used with offer-answer */
  unsigned sr_offer_recv:1;	/**< We have received an offer */
  unsigned sr_answer_sent:2;	/**< We have answered (reliably, if >1) */

  unsigned sr_offer_sent:1;	/**< We have offered SDP */
  unsigned sr_answer_recv:1;	/**< We have received SDP answer */
};

#define SR_INIT(sr)			     \
  ((void)memset((sr), 0, sizeof (sr)[0]),    \
   (void)(SR_STATUS1((sr), SIP_100_TRYING)), \
   sr)

#define SR_STATUS(sr, status, phrase) \
  ((sr)->sr_phrase = (phrase), (sr)->sr_status = (status))

#define SR_STATUS1(sr, statusphrase)					\
  sr_status(sr, statusphrase)

su_inline 
int sr_status(nua_server_request_t *sr, int status, char const *phrase)
{
  return (void)(sr->sr_phrase = phrase), (sr->sr_status = status);
}

struct nua_client_request
{
  nua_client_request_t *cr_next;        /**< Linked list of requests */
  /*nua_event_t*/ int cr_event;		/**< Request event */
  nua_creq_restart_f *cr_restart;
  nta_outgoing_t     *cr_orq;
  msg_t              *cr_msg;
  nua_dialog_usage_t *cr_usage;
  unsigned short      cr_retry_count;   /**< Retry count for this request */

  /* Flags used with offer-answer */
  unsigned short      cr_answer_recv;   /**< Recv answer in response 
					 *  with this status.
					 */
  unsigned            cr_offer_sent:1;  /**< Sent offer in this request */

  unsigned            cr_offer_recv:1;  /**< Recv offer in a response */
  unsigned            cr_answer_sent:1; /**< Sent answer in (PR)ACK */

  unsigned            cr_has_contact:1; /**< Request has application contact */
};


struct nua_dialog_state
{
  nua_client_request_t ds_cr[1];
  nua_server_request_t *ds_sr;

  /** Dialog usages. */
  nua_dialog_usage_t     *ds_usage;

  /* Dialog and subscription state */
  unsigned ds_route:1;		/**< We have route */
  unsigned ds_terminated:1;	/**< Being terminated */

  unsigned ds_has_session:1;	/**< We have session */
  unsigned ds_has_register:1;	/**< We have registration */
  unsigned ds_has_publish:1;	/**< We have publish */

  unsigned ds_has_referrals:1;	/**< We have (or have had) referrals */

  unsigned :0;

  unsigned ds_has_events;	/**< We have events */
  unsigned ds_has_subscribes;   /**< We have subscriptions */
  unsigned ds_has_notifys;	/**< We have notifiers */

  sip_from_t const *ds_local;		/**< Local address */
  sip_to_t const *ds_remote;		/**< Remote address */
  nta_leg_t      *ds_leg;
  char const     *ds_remote_tag;	/**< Remote tag (if any). 
					 * Should be non-NULL 
					 * if dialog is established.
					 */

  struct nua_remote_s {
    sip_allow_t      *nr_allow;
    sip_accept_t     *nr_accept;
    sip_require_t    *nr_require;
    sip_supported_t  *nr_supported;
    sip_user_agent_t *nr_user_agent;
  } ds_remote_ua[1];
};

typedef void nh_pending_f(nua_owner_t *, 
			  nua_dialog_usage_t *du,
			  sip_time_t now);

/** Virtual function pointer table for dialog usage. */
typedef struct {
  unsigned usage_size, usage_class_size;
  int (*usage_add)(nua_owner_t *, 
		   nua_dialog_state_t *ds,
		   nua_dialog_usage_t *du);
  void (*usage_remove)(nua_owner_t *, 
		       nua_dialog_state_t *ds,
		       nua_dialog_usage_t *du);
  char const *(*usage_name)(nua_dialog_usage_t const *du);
  void (*usage_peer_info)(nua_dialog_usage_t *du,
			  nua_dialog_state_t const *ds,
			  sip_t const *sip);
  void (*usage_refresh)(nua_owner_t *, nua_dialog_state_t *ds,
			nua_dialog_usage_t *, sip_time_t now);
  int (*usage_shutdown)(nua_owner_t *, nua_dialog_state_t *ds, 
			nua_dialog_usage_t *);
} nua_usage_class;


/** Base structure for dialog usage. */
struct nua_dialog_usage {
  nua_dialog_usage_t *du_next;
  nua_usage_class const *du_class;

  unsigned     du_terminating:1;	/**< Now trying to terminate usage */
  unsigned     du_ready:1;	        /**< Established usage */
  unsigned     du_shutdown:1;	        /**< Shutdown in progress */
  unsigned:0;

  /** When usage expires.
   * Non-zero if the usage is established, SIP_TIME_MAX if there no
   * expiration time.
   */
  sip_time_t      du_expires;		

  sip_time_t      du_refresh;		/**< When to refresh */

  sip_event_t const *du_event;		/**< Event of usage */

  msg_t *du_msg;			/**< Template message */
};

void nua_dialog_uac_route(nua_owner_t *, nua_dialog_state_t *ds,
			  sip_t const *sip, int rtag);
void nua_dialog_uas_route(nua_owner_t *, nua_dialog_state_t *ds,
			  sip_t const *sip, int rtag);
void nua_dialog_store_peer_info(nua_owner_t *, nua_dialog_state_t *ds,
				sip_t const *sip);
int nua_dialog_remove(nua_owner_t *own,
		      nua_dialog_state_t *ds,
		      nua_dialog_usage_t *usage);

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
			     nua_dialog_usage_t *du);

void nua_dialog_deinit(nua_owner_t *own,
		       nua_dialog_state_t *ds);

void nua_dialog_terminated(nua_owner_t *,
			   struct nua_dialog_state *ds,
			   int status,
			   char const *phrase);

void nua_dialog_usage_set_expires(nua_dialog_usage_t *du, unsigned delta);

void nua_dialog_usage_set_refresh(nua_dialog_usage_t *du, unsigned delta);

void nua_dialog_usage_refresh_range(nua_dialog_usage_t *du, 
				    unsigned min, unsigned max);

void nua_dialog_usage_reset_refresh(nua_dialog_usage_t *du);

void nua_dialog_usage_refresh(nua_owner_t *owner,
			      nua_dialog_state_t *ds,
			      nua_dialog_usage_t *du, 
			      sip_time_t now);

static inline
int nua_dialog_is_established(nua_dialog_state_t const *ds)
{
  return ds->ds_remote_tag != NULL;
}

#if 0
static inline
void *nua_dialog_usage_private(nua_dialog_usage_t const *du)
{
  return du ? (void *)(du + 1) : NULL;
}

static inline
nua_dialog_usage_t *nua_dialog_usage_public(void const *p)
{
  return p ? (nua_dialog_usage_t *)p - 1 : NULL;
}
#else
#define nua_dialog_usage_private(du) ((du) ? (void*)((du) + 1) : NULL)
#define nua_dialog_usage_public(p) ((p) ? (nua_dialog_usage_t*)(p) - 1 : NULL)
#endif

/* ---------------------------------------------------------------------- */

void nua_server_request_destroy(nua_server_request_t *sr);

int nua_server_respond(nua_server_request_t *sr,
		       int status, char const *phrase,
		       tag_type_t tag, tag_value_t value, ...);

msg_t *nua_server_response(nua_server_request_t *sr,
			   int status, char const *phrase,
			   tag_type_t tag, tag_value_t value, ...);

int nua_default_respond(nua_server_request_t *sr,
			tagi_t const *tags);


#endif /* NUA_DIALOG_H */
