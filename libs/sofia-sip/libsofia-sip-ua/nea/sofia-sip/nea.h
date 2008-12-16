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

#ifndef NEA_H
/** Defined when <sofia-sip/nea.h> has been included. */
#define NEA_H
/**@file sofia-sip/nea.h
 * @brief Event API for SIP
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 *
 * @date Created: Fri Feb  7 13:23:44 EET 2003 ppessi
 */

#ifndef SU_ALLOC_H
#include <sofia-sip/su_alloc.h>
#endif

#include <sofia-sip/su_tag.h>

#ifndef NTA_H
#include <sofia-sip/nta.h>
#endif

#ifndef NEA_TAG_H
#include <sofia-sip/nea_tag.h>
#endif

SOFIA_BEGIN_DECLS

#define NEA_VERSION      3.0
#define NEA_VERSION_STR "3.0"

#define NEA_DEFAULT_EXPIRES 3600

/** Event notifier object. */
typedef struct nea_server_s     nea_server_t;

/** Subscription object. */
typedef struct nea_sub_s        nea_sub_t;

/** Event. */
typedef struct nea_event_s      nea_event_t;

/** Event view. */
typedef struct nea_event_view_s nea_event_view_t;

#ifndef NEA_SMAGIC_T
#define NEA_SMAGIC_T            struct nea_smagic_t
#endif
/** NEA server context */
typedef NEA_SMAGIC_T nea_smagic_t;

#ifndef NEA_EMAGIC_T
#define NEA_EMAGIC_T            struct nea_emagic_t
#endif
/** NEA server event context */
typedef NEA_EMAGIC_T nea_emagic_t;

#ifndef NEA_EVMAGIC_T
#define NEA_EVMAGIC_T           struct nea_evmagic_t
#endif
/** Event view context */
typedef NEA_EVMAGIC_T nea_evmagic_t;

/** Description of subscription */
typedef struct nea_subnode_t {
  nea_state_t          sn_state;       	/**< Subscription state */
  unsigned             sn_fake;	       	/**< True if subscriber is given
				       	 *   fake contents.
				       	 */
  unsigned             sn_eventlist;    /**< Subscriber supports eventlist */
  nea_sub_t           *sn_subscriber;  	/**< Pointer to subscriber object */
  nea_event_t         *sn_event;       	/**< Event */
  sip_from_t const    *sn_remote;      	/**< Identity of subscriber */
  sip_contact_t const *sn_contact;     	/**< Contact of subscriber */

  /** Content-Type of SUBSCRIBE body (filter). */
  sip_content_type_t const *sn_content_type;
  sip_payload_t const *sn_payload;      /**< Body of subscribe*/

  unsigned             sn_expires;     	/**< When subscription expires */
  unsigned             sn_latest;      	/**< Latest notification version */
  unsigned             sn_throttle;    	/**< Throttle value */
  unsigned             sn_version;      /**< Latest notified version # by application */
  sip_time_t           sn_notified;     /**< When latest notify was sent */
  sip_time_t           sn_subscribed;   /**< When first SUBSCRIBE was recv */
  nea_event_view_t    *sn_view;		/**< Event view for this subscriber */
} nea_subnode_t;

/** Multiple content types per event. */
typedef struct nea_payloads_s   nea_payloads_t;

/**Unknown event callback.
 *
 * The event server invokes this callback function when it has received a
 * request for an unknown event or event with unknown content type.
 *
 * The callback may be called twice for one watcher, once for an unknown
 * event, another time for an unknown content type.
 *
 * @retval 1 application takes care of responding to request
 * @retval 0 application has added new event or payload format
 * @retval -1 nea server rejects request
 */
typedef int (nea_new_event_f)(nea_smagic_t *context,
			      nea_server_t *nes,
			      nea_event_t **event_p,
			      nea_event_view_t **view_p,
			      nta_incoming_t *irq,
			      sip_t const *sip);

/** Create a notifier server */
SOFIAPUBFUN
nea_server_t *nea_server_create(nta_agent_t *agent,
				su_root_t *root,
				url_t const *url,
				int max_subs,
				nea_new_event_f *callback,
				nea_smagic_t *context,
				tag_type_t tag, tag_value_t value,
				...);


/** Shutdown an event server */
SOFIAPUBFUN int nea_server_shutdown(nea_server_t *nes, int retry_after);

/** Destroy a server */
SOFIAPUBFUN void nea_server_destroy(nea_server_t *nes);

/** Zap terminated subscribtions. */
SOFIAPUBFUN void nea_server_flush(nea_server_t *nes, nea_event_t *event);

/** Update event information */
SOFIAPUBFUN
int nea_server_update(nea_server_t *nes,
		      nea_event_t *ev,
		      tag_type_t tag,
		      tag_value_t value,
		      ...);

/** Add a new subscriber from subscribe transaction to an existing notifier. */
SOFIAPUBFUN
int nea_server_add_irq(nea_server_t *nes,
		       nta_leg_t *leg,
		       sip_contact_t const *local_target,
		       nta_incoming_t *irq,
		       sip_t const *sip);

/** QAUTH callback function type.
 *
 * The event server invokes this callback function upon each incoming
 * SUBSCRIBE transaction when the subscription has expired.  The @a sip is
 * NULL if the subscription has expired.
 *
 * The application determines if the subscription is authorized and relays
 * the decision to event server via nea_server_auth() function.
 */
typedef void (nea_watcher_f)(nea_server_t *nes,
			     nea_emagic_t *context,
			     nea_event_t *event,
			     nea_subnode_t *subnode,
			     sip_t const *sip);

/** Create a new event (or subevent) */
SOFIAPUBFUN
nea_event_t *nea_event_create(nea_server_t *nes,
			      nea_watcher_f *callback,
			      nea_emagic_t *context,
			      char const *name,
			      char const *subname,
			      char const *default_content_type,
			      char const *accept);

/** Create a new event (or subevent) with tags */
SOFIAPUBFUN
nea_event_t *nea_event_tcreate(nea_server_t *nes,
			       nea_watcher_f *callback,
			       nea_emagic_t *context,
			       char const *name,
			       char const *subname,
			       tag_type_t, tag_value_t, ...);

/** Return magic context bind to nea_event */
SOFIAPUBFUN nea_emagic_t *nea_emagic_get(nea_event_t *event);

/** Find a nea event object with given event name */
SOFIAPUBFUN nea_event_t *nea_event_get(nea_server_t const *, char const *name);

/** Get number of active subscribers */
SOFIAPUBFUN int nea_server_active(nea_server_t *nes, nea_event_t const *ev);

/** Get number of (non-embryonic) subscribers. */
int nea_server_non_embryonic(nea_server_t *nes, nea_event_t const *ev);

/** Obtain a list of subscriptions.
 */
SOFIAPUBFUN
nea_subnode_t const **nea_server_get_subscribers(nea_server_t *nes,
						 nea_event_t const *ev);

/** Free a list of subscriptions. */
SOFIAPUBFUN
void nea_server_free_subscribers(nea_server_t *nes, nea_subnode_t const **);

/** Notify subscribers */
SOFIAPUBFUN
int nea_server_notify(nea_server_t *nes,
		      nea_event_t *ev);

/** Notify a subscriber */
SOFIAPUBFUN
int nea_server_notify_one(nea_server_t *nes,
			  nea_event_t *ev,
			  nea_sub_t *ns);

#define nea_server_auth nea_sub_auth

/** Get nta_incoming_t from nea_sub_t */
SOFIAPUBFUN nta_incoming_t *nea_sub_get_request(nea_sub_t *sub);

/** Authorize a subscription */
SOFIAPUBFUN
int nea_sub_auth(nea_sub_t *, nea_state_t state,
		 tag_type_t, tag_value_t, ...);

/** Get nta_incoming_t from sn->sn_subscriber */
SOFIAPUBFUN nta_incoming_t *nea_subnode_get_incoming(nea_subnode_t *sn);
/** Set subscriber version sequence */
SOFIAPUBFUN int nea_sub_version(nea_sub_t *, unsigned);

/** Return time until next notification can be sent */
SOFIAPUBFUN unsigned nea_sub_pending(nea_sub_t const *);

#if 0
/** Do a remote qauth.
 *
 * The function nea_server_qauth() is given as q_callback pointer
 * to nea_server_create() if remote authentication from url is desired.
 */
void nea_server_qauth(nea_server_t *nes,
		      nea_emagic_t *context,
		      nea_sub_t *subscriber,
		      sip_t const *sip);
#endif

/** Get primary event view for given content type  */
SOFIAPUBFUN
nea_event_view_t *nea_event_view(nea_event_t *, char const *content_type);

/** Get a content type for event's payload */
SOFIAPUBFUN
sip_content_type_t const *nea_view_content_type(nea_event_view_t const *);

/** Get actual payload for an event */
SOFIAPUBFUN sip_payload_t const *nea_view_payload(nea_event_view_t *);

/** Create a private event view */
SOFIAPUBFUN nea_event_view_t *nea_view_create(nea_server_t *nes,
					      nea_event_t *ev,
					      nea_evmagic_t *magic,
					      tag_type_t tag,
					      tag_value_t value,
					      ...);

/** Destroy a private event view */
SOFIAPUBFUN void nea_view_destroy(nea_server_t *nes, nea_event_view_t *ev);

SOFIAPUBFUN nea_evmagic_t *nea_view_magic(nea_event_view_t const *);

SOFIAPUBFUN void nea_view_set_magic(nea_event_view_t *, nea_evmagic_t *magic);

SOFIAPUBFUN unsigned nea_view_version(nea_event_view_t const *);

/** Reliable notify */
#define NEATAG_RELIABLE(x)    neatag_reliable, tag_bool_v((x))
SOFIAPUBVAR tag_typedef_t neatag_reliable;

#define NEATAG_RELIABLE_REF(x) neatag_reliable_ref, tag_bool_vr((&x))
SOFIAPUBVAR tag_typedef_t neatag_reliable_ref;

/** Event view handle */
#define NEATAG_VIEW(x)     neatag_view, tag_ptr_v((x))
SOFIAPUBVAR tag_typedef_t neatag_view;

#define NEATAG_VIEW_REF(x) neatag_view_ref, tag_ptr_vr((&x), (x))
SOFIAPUBVAR tag_typedef_t neatag_view_ref;

/** Event view magic. */
#define NEATAG_EVMAGIC(x)     neatag_evmagic, tag_ptr_v((x))
SOFIAPUBVAR tag_typedef_t neatag_evmagic;

#define NEATAG_EVMAGIC_REF(x) neatag_evmagic_ref, tag_ptr_vr((&x), (x))
SOFIAPUBVAR tag_typedef_t neatag_evmagic_ref;

/** tag for nea_sub_t */
#define NEATAG_SUB(x)     neatag_sub, tag_ptr_v((x))
SOFIAPUBVAR tag_typedef_t neatag_sub;

#define NEATAG_SUB_REF(x) neatag_sub_ref, tag_ptr_vr((&x), (x))
SOFIAPUBVAR tag_typedef_t neatag_sub_ref;


/* ====================================================================== */
/* Watcher side */

/** NEA Event Watcher */
typedef struct nea_s     nea_t;

#ifndef NEA_MAGIC_T
#define NEA_MAGIC_T struct nea_magic_t
#endif

/** NEA Event Agent context */
typedef NEA_MAGIC_T          nea_magic_t;

/** Event notification callback type.
 *
 * This callback is called also when initial or refresh subscribe transaction
 * completes with the transaction result in @a sip.
 */
typedef int (*nea_notify_f)(nea_t *nea,
			    nea_magic_t *context,
			    sip_t const *sip);

/* ====================================================================== */
/* Client side */

/** Create a subscription agent. */
SOFIAPUBFUN
nea_t *nea_create(nta_agent_t *agent,
		  su_root_t *root,
		  nea_notify_f no_callback,
		  nea_magic_t *context,
		  tag_type_t tag,
		  tag_value_t value,
		  ...);

/** Update SUBSCRIBE payload (filter rules) */
SOFIAPUBFUN
int nea_update(nea_t *nea,
	       tag_type_t tag,
	       tag_value_t value,
	       ...);

/** Unsubscribe agent. */
SOFIAPUBFUN void nea_end(nea_t *agent);

/** Destroy a subscription agent. */
SOFIAPUBFUN void nea_destroy(nea_t *agent);

SOFIAPUBFUN char const *nea_default_content_type(char const *event);

SOFIA_END_DECLS

#endif /* !defined(NEA_H) */
