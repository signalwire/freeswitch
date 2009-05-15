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

/**@internal @file nea_server.c
 * @brief Nokia Event API - event notifier implementation.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 *
 * @date Created: Wed Feb 14 18:37:04 EET 2001 ppessi
 */

#include "config.h"

#include <sofia-sip/sip.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_util.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/su_tagarg.h>

#include "nea_debug.h"

#define NONE ((void *)- 1)

#define SU_ROOT_MAGIC_T      struct nea_server_s
#define SU_MSG_ARG_T         tagi_t

#define NTA_AGENT_MAGIC_T    struct nea_server_s
#define NTA_LEG_MAGIC_T      struct nea_sub_s
#define NTA_INCOMING_MAGIC_T struct nea_sub_s
#define NTA_OUTGOING_MAGIC_T struct nea_sub_s

#include <sofia-sip/nea.h>
#include <sofia-sip/htable.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>

/** Number of primary views (with different MIME type or content) */
#define NEA_VIEW_MAX (8)

/** @internal Server object, created for every notifier.
 */
struct nea_server_s {
  su_home_t                 nes_home[1];
  su_root_t                *nes_root;
  su_timer_t               *nes_timer;

  nta_agent_t              *nes_agent;
  nta_leg_t                *nes_leg;

  nea_sub_t                *nes_subscribers;

  sip_require_t            *nes_require;

  sip_time_t                nes_min_expires;
  sip_time_t                nes_expires;
  sip_time_t                nes_max_expires;

  int                       nes_max_subs;
  unsigned                  nes_throttle; /**< Default throttle */
  unsigned                  nes_min_throttle; /**< Minimum throttle */
  unsigned                  nes_eventlist:1; /**< Eventlist only */
  unsigned                  nes_in_callback : 1;
  unsigned                  nes_pending_destroy : 1;
  unsigned                  nes_pending_flush : 1;
  unsigned                  nes_202_before_notify:1;

  unsigned                  nes_in_list;

  unsigned                  nes_throttled; /**< Throttled notifications? */

  char const               *nes_server;

  sip_contact_t            *nes_eventity_uri;
  sip_allow_events_t       *nes_allow_events;

  sip_allow_t              *nes_allow_methods;

  nea_new_event_f          *nes_callback;
  nea_smagic_t             *nes_context;

  /** Events.
   * Each subscriber will be added to one of these. */
  nea_event_t              *nes_events;
};


/** @internal Supported events and their subscribers  */
struct nea_event_s {
  nea_event_t              *ev_next;
  nea_event_t             **ev_prev;

  nea_watcher_f            *ev_callback;
  nea_emagic_t             *ev_magic;

  unsigned                  ev_throttle; /**< Default throttle */
  unsigned                  ev_min_throttle; /**< Minimum throttle */
  unsigned                  ev_eventlist:1; /**< Eventlist is supported */
  unsigned                  ev_reliable:1;  /**< Keep all notifications */
  unsigned                  :0;

  /** Sequence number of first unsent update */
  unsigned                  ev_throttling;
  unsigned                  ev_updated;	/**< Sequence number for updates */
  sip_require_t            *ev_require; /**< Required features */
  sip_supported_t          *ev_supported; /**< Supported features */

  sip_event_t              *ev_event;
  sip_accept_t const       *ev_default;/**< Default content type */
  sip_accept_t const       *ev_accept; /**< Supported content types */

  nea_event_view_t         *ev_views[NEA_VIEW_MAX + 1];
};

typedef struct nea_event_queue_s nea_event_queue_t;

/** @internal Object representing particular view of event */
struct nea_event_view_s
{
  nea_event_view_t *evv_next;
  nea_event_view_t *evv_primary; 	/**< Backpointer to the primary view */

  nea_evmagic_t    *evv_magic;

  unsigned          evv_throttle; /**< Default throttle */
  unsigned          evv_min_throttle; /**< Minimum throttle */
  unsigned          evv_fake:1;		/**< This is "fake" (ie. default) view */
  unsigned          evv_private:1;	/**< This is private view */
  unsigned          evv_reliable:1;     /**< Keep all notifications */
  unsigned:0;

  /** @internal Queued notification */
  struct nea_event_queue_s
  {
    nea_event_queue_t  *evq_next;
    unsigned            evq_updated;
    unsigned            evq_version;
    sip_content_type_t *evq_content_type;
    sip_payload_t      *evq_payload;
  } evv_head[1];
};

#define evv_version evv_head->evq_version
#define evv_updated evv_head->evq_updated
#define evv_content_type evv_head->evq_content_type
#define evv_payload evv_head->evq_payload


/** @internal Subscription object.
 */
struct nea_sub_s {
  nea_sub_t        *s_next;
  nea_sub_t       **s_prev;

  nta_leg_t        *s_leg;
  nta_incoming_t   *s_irq;
  nta_outgoing_t   *s_oreq;

  nea_server_t     *s_nes;

  sip_contact_t    *s_local;	/**< Local contact */

  sip_from_t       *s_from;
  sip_contact_t    *s_remote;	/**< Remote contact  */
  /* sip_accept_t  *s_accept; */
  sip_event_t      *s_id;

  nea_event_t      *s_event;
  nea_event_view_t *s_view;
  nea_state_t       s_state;

  char const       *s_extended;

  sip_content_type_t *s_content_type; /** Content-Type of SUBSCRIBE body. */
  sip_payload_t    *s_payload;      /**< Body of SUBSCRIBE. */

  unsigned          s_reported :1 ; /**< Made watcher report upon un-SUBSCRIBE */

  unsigned          s_processing : 1;
  unsigned          s_rejected : 1;
  unsigned          s_pending_flush : 1;
  unsigned          s_garbage : 1;
  unsigned          s_fake : 1; /**< Do not send real information to user */
  unsigned          s_eventlist : 1; /**< Subscriber supported eventlist */

  sip_time_t        s_subscribed; /**< When first SUBSCRIBE was recv */
  sip_time_t        s_notified; /**< When last notification was sent */
  sip_time_t        s_expires;  /**< Expiration time. */

  unsigned          s_version;	/**< Version number set by application */

  unsigned          s_latest;	/**< External version of latest payload */
  unsigned          s_updated;	/**< Internal version of latest payload */
  unsigned          s_throttle;	/**< Minimum time between notifications */
};

/* Prototypes */

static void nea_server_pending_flush(nea_server_t *nes);

static int nea_view_update(nea_server_t *nes,
			   nea_event_t *ev,
			   nea_event_view_t **evvp,
			   int private,
			   int fake,
			   tag_type_t tag,
			   tag_value_t value,
			   ...);

static nea_sub_t *nea_sub_create(nea_server_t *nes);
static int nea_sub_is_removed(nea_sub_t const *s);
static void nea_sub_remove(nea_sub_t *s);
static void nea_sub_destroy(nea_sub_t *s);

static
int nea_server_callback(nea_sub_t *nes_as_sub,
			nta_leg_t *leg,
			nta_incoming_t *irq,
			sip_t const *sip);

static int nea_sub_process_incoming(nea_sub_t *s,
				    nta_leg_t *leg,
				    nta_incoming_t *irq,
				    sip_t const *sip);

static int nea_sub_process_subscribe(nea_sub_t *s,
				     nta_leg_t *leg,
				     nta_incoming_t *irq,
				     sip_t const *sip);

static int nea_sub_notify(nea_server_t *nes,
			  nea_sub_t *s,
			  sip_time_t now,
			  tag_type_t tag, tag_value_t value, ...);

static int response_to_notify(nea_sub_t *s,
			      nta_outgoing_t *oreq,
			      sip_t const *sip);

static void nes_event_timer(nea_server_t *nes,
			    su_timer_t *timer,
			    su_timer_arg_t *arg);

static int nea_view_queue(nea_server_t *nes,
			  nea_event_view_t *evv,
			  nea_event_queue_t *evq);

/** Assign an event view to subscriber. */
su_inline
void nea_sub_assign_view(nea_sub_t *s, nea_event_view_t *evv)
{
  if (s->s_view != evv)
    /* Make sure we send a notification */
    s->s_updated = evv->evv_updated - 1;
  s->s_view = evv;
  s->s_throttle = evv->evv_throttle;
}

su_inline
void nea_subnode_init(nea_subnode_t *sn, nea_sub_t *s, sip_time_t now)
{
  sn->sn_state = s->s_state;
  sn->sn_fake = s->s_fake;
  sn->sn_subscriber = s;
  sn->sn_event = s->s_event;
  sn->sn_remote = s->s_from;
  sn->sn_contact = s->s_remote;
  sn->sn_content_type = s->s_content_type;
  sn->sn_payload = s->s_payload;
  if (s->s_expires != 0 && (int)(s->s_expires - now) > 0)
    sn->sn_expires = s->s_expires - now;
  else
    sn->sn_expires = 0;
  sn->sn_latest = s->s_latest;
  sn->sn_throttle = s->s_throttle;
  sn->sn_eventlist = s->s_eventlist;
  sn->sn_version = s->s_version;
  sn->sn_subscribed = now - s->s_subscribed;
  sn->sn_notified = s->s_notified;
  sn->sn_view = s->s_view;
}

/** Create an event server.
 *
 * The function nea_server_create() initializes an event server object and
 * registers it with @b nta. An event server object takes care of all events
 * for a particular URI (@em eventity).
 *
 * @param agent       pointer to an @b nta agent object
 * @param root        pointer to an @b root object
 * @param url         url of the server to be created
 * @param max_subs    maximum number of subscriptions
 * @param callback    authorization function,
 *                    or @c NULL if no authorization is required
 * @param context     server context (pointer to application data)
 * @param tag, value, ... optional list of tag parameters
 *
 * @TAGS
 * The function nea_server_create() takes the following tag values as its
 * arguments:
 * <dl>
 *
 * <dt>SIPTAG_CONTACT() or SIPTAG_CONTACT_STR()
 * <dd>The target address of the event server.
 *
 * <dt>SIPTAG_ALLOW_EVENTS()
 * <dd>The initial list of events supported by eventity. This list is
 * extended whenever a new event is created with nea_event_tcreate().
 *
 * <dt>SIPTAG_SERVER_STR()
 * <dd>The @b Server header for the event server.
 *
 * <dt>NEATAG_MINSUB()
 * <dd>Minimum duration of a subscription.
 *
 * <dt>NEATAG_THROTTLE()
 * <dd>Default value for event throttle (by default, 5 seconds).
 * Throttle determines the minimum interval betweeen notifications. Note
 * that the notification indicating that the subscription has terminated
 * will be sent regardless of throttle.
 *
 * The default throttle value is used if the subscriber does not include
 * a throttle parameter in @ref sip_event "Event" header of SUBSCRIBE request.
 *
 * <dt>NEATAG_MINTHROTTLE()
 * <dd>Minimum allowed throttle value (by default, 5 seconds).
 *
 * <dt>NEATAG_EVENTLIST()
 * <dd>If true, the subscribers must support eventlists. If SIPTAG_REQUIRE()
 * is given, it must contain the "eventlist" feature.
 *
 * <dt>NEATAG_DIALOG()
 * <dd>Give an optional NTA destination leg to event server.
 *
 * <dt>SIPTAG_REQUIRE()/SIPTAG_REQUIRE_STR()
 * <dd>The @b Require header for the event server. The subscribers must
 * indicate support the specified features.
 *
 * </dl>
 *
 * @return
 * The function nea_server_create() returns a pointer to an event server
 * object, or @c NULL upon an error.
 */
nea_server_t *nea_server_create(nta_agent_t *agent,
				su_root_t *root,
				url_t const *url,
				int max_subs,
				nea_new_event_f *callback,
				nea_smagic_t *context,
				tag_type_t tag, tag_value_t value, ...)
{
  nea_server_t *nes = NULL;
  sip_contact_t const *contact = NULL;
  sip_allow_events_t const *allow_events = NULL;
  sip_require_t const *rq = NULL;
  char const *contact_str = NULL;
  char const *server_str = NULL;
  char const *rq_str = NULL;
  unsigned
    min_expires = 15 * 60,
    expires = NEA_DEFAULT_EXPIRES,
    max_expires = 24 * 60 * 60;
  nta_leg_t *leg = NONE;
  unsigned throttle = 5, min_throttle = throttle;
  int eventlist = 0;

  {
    ta_list ta;

    ta_start(ta, tag, value);

    tl_gets(ta_args(ta),
	    SIPTAG_CONTACT_REF(contact),
	    SIPTAG_CONTACT_STR_REF(contact_str),
	    SIPTAG_ALLOW_EVENTS_REF(allow_events),
	    SIPTAG_SERVER_STR_REF(server_str),
	    SIPTAG_REQUIRE_REF(rq),
	    SIPTAG_REQUIRE_STR_REF(rq_str),
	    NEATAG_MIN_EXPIRES_REF(min_expires),
	    NEATAG_EXPIRES_REF(expires),
	    NEATAG_MAX_EXPIRES_REF(max_expires),
	    NEATAG_DIALOG_REF(leg),
	    NEATAG_THROTTLE_REF(throttle),
	    NEATAG_MINTHROTTLE_REF(min_throttle),
	    NEATAG_EVENTLIST_REF(eventlist),
	    TAG_NULL());

    ta_end(ta);
  }

  if (throttle < min_throttle)
    throttle = min_throttle;

  if (!url) {
    SU_DEBUG_5(("nea_server_create(): invalid url\n"));
    return NULL;
  }

  if (min_expires > expires || expires > max_expires) {
    SU_DEBUG_5(("nea_server_create(): invalid expiration range\n"));
    return NULL;
  }

  nes = su_home_new(sizeof(nea_server_t));

  if (nes) {
    su_home_t *home = nes->nes_home;

    nes->nes_root = root;
    nes->nes_agent = agent;

    nes->nes_max_subs = max_subs;

    nes->nes_min_expires = min_expires;
    nes->nes_expires = expires;
    nes->nes_max_expires = max_expires;

    nes->nes_throttle = throttle;
    nes->nes_min_throttle = min_throttle;

    if (allow_events)
      nes->nes_allow_events = sip_allow_events_dup(home, allow_events);
    else
      nes->nes_allow_events = sip_allow_events_make(home, "");

    nes->nes_allow_methods = sip_allow_make(home, "SUBSCRIBE");

    nes->nes_server =
      su_sprintf(home, "%s%snea/" NEA_VERSION_STR " %s",
		 server_str ? server_str : "",
		 server_str ? " " : "",
		 nta_agent_version(agent));

    if (contact)
      nes->nes_eventity_uri = sip_contact_dup(home, contact);
    else if (contact_str)
      nes->nes_eventity_uri = sip_contact_make(home, contact_str);
    else
      nes->nes_eventity_uri = sip_contact_create(home, (url_string_t *)url, NULL);

    if (leg != NONE) {
      nes->nes_leg = leg;
      if (leg != NULL)
	nta_leg_bind(leg, nea_server_callback, (nea_sub_t*)nes);
    } else {
      nes->nes_leg = nta_leg_tcreate(agent,
				     nea_server_callback,
				     (nea_sub_t*)nes,
				     NTATAG_NO_DIALOG(1),
				     NTATAG_METHOD("SUBSCRIBE"),
				     URLTAG_URL(url),
				     TAG_END());
    }

    nes->nes_eventlist = eventlist; /* Every event is a list */
    if (eventlist && rq == NULL && rq_str == NULL)
      rq_str = "eventlist";

    if (rq)
      nes->nes_require = sip_require_dup(nes->nes_home, rq);
    else if (rq_str)
      nes->nes_require = sip_require_make(nes->nes_home, rq_str);

    nes->nes_timer = su_timer_create(su_root_task(nes->nes_root),
				     nes->nes_min_throttle
				     ? 500L * nes->nes_min_throttle
				     : 500L);

    if (nes->nes_allow_events &&
	nes->nes_eventity_uri &&
	(nes->nes_leg || leg == NULL) &&
	nes->nes_timer) {
      SU_DEBUG_5(("nea_server_create(%p): success\n", (void *)nes));
      su_timer_set(nes->nes_timer, nes_event_timer, nes);

      nes->nes_callback = callback;
      nes->nes_context = context;
    }
    else {
      SU_DEBUG_5(("nea_server_create(%p): failed\n", (void *)nes));
      nea_server_destroy(nes), nes = NULL;
    }
  }

  return nes;
}

/** Invoke the new event callback.
 *
 * The function nes_event_callback() calls the callback provided by the
 * application using the notifier object.
 *
 * @param nes pointer to notifier object
 * @param ev  pointer to event view
 * @param s   pointer to subscription object
 * @param sip pointer to subscribe request
 *
 * @return
 * The function nes_event_callback() returns -1 if the notifier object
 * has been destroyed by the callback function, 0 otherwise.
 */
static
int nes_new_event_callback(nea_server_t *nes,
			   nea_event_t **ev_p,
			   nea_event_view_t **view_p,
			   nta_incoming_t *irq,
			   sip_t const *sip)
{
  if (nes->nes_callback)
    return nes->nes_callback(nes->nes_context, nes, ev_p, view_p, irq, sip);
  else
    return -1;
}

/** Shutdown event server.
 */
int nea_server_shutdown(nea_server_t *nes,
			int retry_after)
{
  nea_sub_t *s;
  int status = 200;
  int in_callback;

  if (nes == NULL)
    return 500;

  if (nes->nes_in_callback) {
    SU_DEBUG_5(("nea_server_shutdown(%p) while in callback\n", (void *)nes));
    return 100;
  }

  SU_DEBUG_5(("nea_server_shutdown(%p)\n", (void *)nes));

  in_callback = nes->nes_in_callback; nes->nes_in_callback = 1;

  for (s = nes->nes_subscribers; s; s = s->s_next) {
    if (s->s_state == nea_terminated)
      continue;
    if (s->s_pending_flush)
      continue;
    if (s->s_oreq == NULL)
      nea_sub_auth(s, nea_terminated,
		   TAG_IF(retry_after, NEATAG_REASON("probation")),
		   TAG_IF(!retry_after, NEATAG_REASON("deactivated")),
		   TAG_IF(retry_after, NEATAG_RETRY_AFTER(retry_after)),
		   TAG_END());
    else
      status = 180;
  }

  nes->nes_in_callback = in_callback;

  return 200;
}

void nea_server_destroy(nea_server_t *nes)
{
  if (nes == NULL)
    return;

  if (nes->nes_in_callback) {
    SU_DEBUG_5(("nea_server_destroy(%p) while in callback\n", (void *)nes));
    nes->nes_pending_destroy = 1;
    return;
  }

  SU_DEBUG_5(("nea_server_destroy(%p)\n", (void *)nes));

  nta_leg_destroy(nes->nes_leg), nes->nes_leg = NULL;

  while (nes->nes_subscribers)
    nea_sub_destroy(nes->nes_subscribers);

  su_timer_destroy(nes->nes_timer), nes->nes_timer = NULL;

  su_home_unref(nes->nes_home);
}

/* ----------------------------------------------------------------- */

/**Update server payload.
 *
 * A nea event server has typed content that is delivered to the
 * subscribers. Different content types are each assigned a separate primary
 * view. There can be also primary views with "fake" content, content
 * delivered to politely blocked subscribers.
 *
 * In addition to primary views, there can be secondary views, views
 * assigned to a single subscriber only.
 *
 * @TAGS
 * The following tagged arguments are accepted:
 * <dl>
 *
 * <dt>SIPTAG_PAYLOAD() or SIPTAG_PAYLOAD_STR()
 * <dd>Updated event content.
 *
 * <dt>SIPTAG_CONTENT_TYPE() or SIPTAG_CONTENT_TYPE_STR().
 * <dd>MIME type of the content.
 *
 * <dt>NEATAG_FAKE(fak)
 * <dd>If @a fake is true, 'fake' view is updated.
 *
 * <dt>NEATAG_VIEW(view)
 * <dd>If included in tagged arguments, @a view is * updated. Used when
 * updating secondary view.
 *
 * <dt>NEATAG_VERSION(version)
 * <dd>The application-provided @a version for
 * event content. After updated content has been sent to subscriber, @a
 * version is copied to subscriber information structure.
 *
 * <dt>NEATAG_EVMAGIC(context)
 * <dd>Application-provided @a context pointer.
 * The @a context pointer is returned by nea_view_magic() function.
 *
 * <dt>NEATAG_RELIABLE(reliable)
 * <dd>The @a reliable flag determines how overlapping updates are handled.
 * If @a reliable is true, all updates are delivered to the subscribers.
 *
 * <dt>NEATAG_THROTTLE(throttl)
 * <dd>Default value for event throttle for updated event view. Throttle
 * determines the minimum interval in seconds betweeen notifications. Note
 * that the notification indicating that the subscription has terminated
 * will be sent regardless of throttle.
 *
 * The default throttle value is used if the subscriber does not include
 * a throttle parameter in @ref sip_event "Event" header of SUBSCRIBE request.
 *
 * <dt>NEATAG_MINTHROTTLE()
 * <dd>Minimum allowed throttle value for updated event view.
 *
 * </dl>
 *
 * @retval -1 upon an error.
 * @retval 0  if event document was not updated.
 * @retval 1  if event document was updated.
 */
int nea_server_update(nea_server_t *nes,
		      nea_event_t *ev,
		      tag_type_t tag,
		      tag_value_t value,
		      ...)
{
  nea_event_view_t *evv = NULL;
  int fake = 0, updated;

  ta_list ta;

  if (ev == NULL)
    ev = nes->nes_events;

  ta_start(ta, tag, value);

  tl_gets(ta_args(ta),
	  NEATAG_FAKE_REF(fake),
	  NEATAG_VIEW_REF(evv),
	  TAG_NULL());

  updated = nea_view_update(nes, ev, &evv, 0, fake, ta_tags(ta));

  ta_end(ta);

  return updated;
}

static
int nea_view_update(nea_server_t *nes,
		    nea_event_t *ev,
		    nea_event_view_t **evvp,
		    int private,
		    int fake,
		    tag_type_t tag,
		    tag_value_t value,
		    ...)
{
  ta_list ta;

  su_home_t *home = nes->nes_home;

  sip_content_type_t const *ct = NULL;
  char const *cts = NULL, *pls = NULL;
  sip_payload_t const *pl = NULL;
  sip_payload_t *new_pl;
  nea_event_view_t *evv, **eevv = &evv;
  nea_event_view_t *primary = NULL, **primary_p = &primary;
  unsigned version = UINT_MAX;
  nea_evmagic_t *evmagic = NULL;
  int reliable = ev->ev_reliable;
  unsigned throttle = ev->ev_throttle;
  unsigned min_throttle = ev->ev_min_throttle;

  nea_event_queue_t evq[1] = {{ NULL }};

  ta_start(ta, tag, value);

  tl_gets(ta_args(ta),
	  SIPTAG_CONTENT_TYPE_REF(ct),
	  SIPTAG_CONTENT_TYPE_STR_REF(cts),
	  SIPTAG_PAYLOAD_REF(pl),
	  SIPTAG_PAYLOAD_STR_REF(pls),
	  NEATAG_VERSION_REF(version),
	  NEATAG_EVMAGIC_REF(evmagic),
	  NEATAG_RELIABLE_REF(reliable),
	  NEATAG_THROTTLE_REF(throttle),
	  NEATAG_MINTHROTTLE_REF(min_throttle),
	  TAG_NULL());

  ta_end(ta);

  if (min_throttle < throttle)
    min_throttle = throttle;

  if (ct == NULL && cts == NULL)
    return -1;

  if (ct)
    cts = ct->c_type;

  evv = *evvp;

  if (!evv) {
    int i;

    /* Check if the payload type already exists */
    for (i = 0; (evv = ev->ev_views[i]); i++)
      if (su_casematch(cts, evv->evv_content_type->c_type))
	break;

    if (private && evv == NULL) /* No private view without primary view. */
      return -1;

    if (i == NEA_VIEW_MAX)	/* Too many primary views. */
      return -1;

    primary_p = eevv = ev->ev_views + i;

    /* Search for fakeness/eventlist/private view */
    if (evv && (private || evv->evv_private || evv->evv_fake != (unsigned)fake)) {
      for (eevv = &evv->evv_next; (evv = *eevv); eevv = &evv->evv_next) {
	if (private || evv->evv_private)
	  continue;
	if (evv->evv_fake == (unsigned)fake)
	  break;
      }
    }
  }

  /* New event view, allocate and link to chain */
  if (!evv) {
    sip_content_type_t *new_ct;

    evv = su_zalloc(home, sizeof (*evv));
    if (!evv)
      return -1;

    new_pl = pl ? sip_payload_dup(home, pl)
      : sip_payload_make(home, pls);

    new_ct = ct ? sip_content_type_dup(home, ct)
      : sip_content_type_make(home, cts);

    if ((!new_pl && pl) || !new_ct) {
      su_free(home, evv); su_free(home, new_pl);
      return -1;
    }

    *evvp = *eevv = evv;

    evv->evv_primary = *primary_p;
    evv->evv_private = private != 0;
    evv->evv_fake = fake != 0;
    evv->evv_reliable = reliable != 0;
    evv->evv_magic = evmagic;
    evv->evv_content_type = new_ct;
    evv->evv_payload = new_pl;
    evv->evv_throttle = throttle;
    evv->evv_min_throttle = min_throttle;

    assert(evv->evv_content_type);
  }
  else {
    if (pl &&
	evv->evv_payload &&
	evv->evv_payload->pl_len == pl->pl_len &&
	memcmp(evv->evv_payload->pl_data, pl->pl_data, pl->pl_len) == 0)
      return 0;
    if (!pl && pls && evv->evv_payload &&
	evv->evv_payload->pl_len == strlen(pls) &&
	memcmp(evv->evv_payload->pl_data, pls, evv->evv_payload->pl_len) == 0)
      return 0;
    if (!pl && !pls && !evv->evv_payload)
      return 0;

    *evq = *evv->evv_head;

    new_pl = pl ? sip_payload_dup(home, pl) : sip_payload_make(home, pls);

    if (!new_pl && (pl || pls))
      return -1;

    evv->evv_payload = new_pl;
  }

  if (version != UINT_MAX)
    evv->evv_version = version;

  if (!fake)
    evv->evv_updated = ++ev->ev_updated;

  if (evq->evq_content_type)
    nea_view_queue(nes, evv, evq);

  SU_DEBUG_7(("nea_server_update(%p): %s (%s)\n", (void *)nes,
	      ev->ev_event->o_type, evv->evv_content_type->c_type));

  return 1;
}

nea_event_view_t *nea_view_create(nea_server_t *nes,
				  nea_event_t *ev,
				  nea_evmagic_t *magic,
				  tag_type_t tag,
				  tag_value_t value,
				  ...)
{
  nea_event_view_t *evv = NULL;
  ta_list ta;

  if (ev == NULL)
    return NULL;

  ta_start(ta, tag, value);

  nea_view_update(nes, ev, &evv, 1, 0, ta_tags(ta));

  ta_end(ta);

  return evv;
}

void nea_view_destroy(nea_server_t *nes, nea_event_view_t *evv)
{
  nea_event_view_t **evvp;
  nea_sub_t *s;

  if (nes == NULL || evv == NULL || !evv->evv_private)
    return;

  assert(evv->evv_primary && evv != evv->evv_primary);

  for (evvp = &evv->evv_primary->evv_next; *evvp; evvp = &(*evvp)->evv_next)
    if (*evvp == evv) {
      *evvp = evv->evv_next;
      break;
    }

  for (s = nes->nes_subscribers; s; s = s->s_next)
    if (s->s_view == evv)
      nea_sub_assign_view(s, evv->evv_primary);

  su_free(nes->nes_home, evv->evv_content_type);
  su_free(nes->nes_home, evv->evv_payload);
  su_free(nes->nes_home, evv);
}

nea_evmagic_t *nea_view_magic(nea_event_view_t const *evv)
{
  return evv ? evv->evv_magic : NULL;
}

void nea_view_set_magic(nea_event_view_t *evv, nea_evmagic_t *magic)
{
  if (evv)
    evv->evv_magic = magic;
}

unsigned nea_view_version(nea_event_view_t const *evv)
{
  return evv ? evv->evv_version : 0;
}

/** Get primary, non-fake event view for given content type  */
nea_event_view_t *nea_event_view(nea_event_t *ev, char const *content_type)
{
  int i;
  nea_event_view_t *evv;

  /* Check if the payload type already exists */
  for (i = 0; ev->ev_views[i]; i++)
    if (su_casematch(content_type, ev->ev_views[i]->evv_content_type->c_type))
      break;

  for (evv = ev->ev_views[i]; evv; evv = evv->evv_next)
    if (!evv->evv_fake)
      return evv;

  return ev->ev_views[i];
}

/** Get the content type for event view */
sip_content_type_t const *nea_view_content_type(nea_event_view_t const *evv)
{
  return evv ? evv->evv_content_type : NULL;
}


/** Queue an old notification if needed. */
static
int nea_view_queue(nea_server_t *nes,
		   nea_event_view_t *evv,
		   nea_event_queue_t *evq)
{
  nea_sub_t *s = NULL;

  assert(nes && evv && evq);

  if (evv->evv_reliable)
    for (s = nes->nes_subscribers; s; s = s->s_next) {
      if (s->s_view != evv)
	continue;
      if (s->s_updated > evq->evq_updated)
	continue;
      if (s->s_updated == evq->evq_updated && s->s_oreq == NULL)
	continue;
      break;			/* This  */
    }

  if (s) {
    nea_event_queue_t *evq0 = su_alloc(nes->nes_home, sizeof *evq);

    if (evq0 == NULL)
      return -1;

    *evq0 = *evq, evq = evq0;

    /* evq should be copy of old head but with changed payload  */
    assert(evq->evq_next == evv->evv_head->evq_next);

    evv->evv_head->evq_next = evq;     /* insert to the queue */

    return 0;
  }

  su_free(nes->nes_home, (void *)evq->evq_payload);

  return 0;
}

/** Remove old unneeded notifications. */
static
int nea_view_dequeue(nea_server_t *nes,
		     nea_event_t *ev)
{
  int i;
  nea_event_view_t *evv;
  nea_event_queue_t **prev, *evq;;

  assert(nes && ev);

  for (i = 0; ev->ev_views[i]; i++) {
    for (evv = ev->ev_views[i]; evv; evv = evv->evv_next) {
      if (!evv->evv_reliable)
	continue;

      for (prev = &evv->evv_head->evq_next; *prev; prev = &(*prev)->evq_next)
	if (ev->ev_throttling >= (*prev)->evq_updated)
	  break;

      /* Free from evq onwards */
      for (evq = *prev; evq; evq = *prev) {
	*prev = evq->evq_next;
	su_free(nes->nes_home, evq->evq_payload);
	su_free(nes->nes_home, evq);
      }
    }
  }

  return 0;
}

/* ----------------------------------------------------------------- */

/** Notify watchers.
 *
 * @return
 * The function nea_server_notify() returns number of subscribers that the
 * notification could be sent, or -1 upon an error.
 */
int nea_server_notify(nea_server_t *nes, nea_event_t *ev)
{
  sip_time_t now = sip_now();
  nea_sub_t *s;
  int notified = 0, throttled = nes->nes_throttled;

  SU_DEBUG_7(("nea_server_notify(%p): %s\n", (void *)nes,
	      ev ? ev->ev_event->o_type: ""));

  ++nes->nes_in_list;

  nes->nes_throttled = 0;

  if (ev == NULL)
    for (ev = nes->nes_events; ev; ev = ev->ev_next)
      ev->ev_throttling = UINT_MAX;
  else
    ev->ev_throttling = UINT_MAX;

  for (s = nes->nes_subscribers; s; s = s->s_next) {
    if ((ev == NULL || ev == s->s_event) && s->s_state != nea_terminated) {
      notified += nea_sub_notify(nes, s, now, TAG_END());
    }
  }

  if (throttled) {
    /* Dequeue throttled updates */
    if (ev == NULL)
      for (ev = nes->nes_events; ev; ev = ev->ev_next) {
	nea_view_dequeue(nes, ev);
	SU_DEBUG_3(("nea_server(): notified %u, throttling at %u\n",
		    notified, ev->ev_throttling));
      }
    else {
      SU_DEBUG_3(("nea_server(): notified %u, throttling at %u\n",
		  notified, ev->ev_throttling));
      nea_view_dequeue(nes, ev);
    }
  }

  if (--nes->nes_in_list == 0 && nes->nes_pending_flush)
    nea_server_pending_flush(nes);

  return notified;
}


/* ----------------------------------------------------------------- */
void nea_server_flush(nea_server_t *nes, nea_event_t *event)
{
  nea_sub_t *s, **ss;
  sip_time_t now;

  if (nes == NULL)
    return;

  now = sip_now();

  for (ss = &nes->nes_subscribers; (s = *ss);) {
    if ((event == NULL || s->s_event == event) &&
	(s->s_state == nea_terminated || s->s_expires < now)) {
      /** On first flush, mark as garbage, remove on second flush */
      if (!s->s_garbage)
	s->s_garbage = 1;
      else if (nes->nes_in_callback || nes->nes_in_list) {
	nes->nes_pending_flush = 1;
	(*ss)->s_pending_flush = 1;
      }
      else {
	nea_sub_destroy(*ss);
	continue;
      }
    }
    ss = &((*ss)->s_next);
  }
}


/* ----------------------------------------------------------------- */
static
void nea_server_pending_flush(nea_server_t *nes)
{
  nea_sub_t **ss;

  for (ss = &nes->nes_subscribers; *ss;) {
    if ((*ss)->s_pending_flush && !(*ss)->s_processing) {
      nea_sub_destroy(*ss);
    } else {
      ss = &((*ss)->s_next);
    }
  }

  nes->nes_pending_flush = 0;
}

/* ----------------------------------------------------------------- */
nea_sub_t *nea_sub_create(nea_server_t *nes)
{
  nea_sub_t *s;

  assert(nes);

  s = su_zalloc(nes->nes_home, sizeof (*s));

  if (s) {
    s->s_nes = nes;
    if ((s->s_next = nes->nes_subscribers))
      s->s_next->s_prev = &s->s_next;
    s->s_prev = &nes->nes_subscribers;
    nes->nes_subscribers = s;

    /* Copy default values */
    s->s_throttle = nes->nes_throttle;
  }

  return s;
}

/* ----------------------------------------------------------------- */
nta_incoming_t *nea_subnode_get_incoming(nea_subnode_t *sn)
{
  assert(sn);

  if (sn->sn_subscriber) {
    return sn->sn_subscriber->s_irq;
  }
  return NULL;
}

/* ----------------------------------------------------------------- */
void nea_sub_remove(nea_sub_t *s)
{
  if (s) {
    assert(s->s_prev);

    if ((*s->s_prev = s->s_next))
      s->s_next->s_prev = s->s_prev;

    s->s_prev = NULL;
    s->s_next = NULL;
  }
}

/* ----------------------------------------------------------------- */
/**Check if subscriber has been removed from list */
static int nea_sub_is_removed(nea_sub_t const *s)
{
  return s->s_prev == NULL;
}

/* ----------------------------------------------------------------- */
void nea_sub_destroy(nea_sub_t *s)
{
  if (s) {
    nea_sub_t *del =  s;
    su_home_t *home = del->s_nes->nes_home;

    if (!nea_sub_is_removed(del))
      nea_sub_remove(del);

    del->s_event = NULL;

    su_free(home, del->s_local), del->s_local = NULL;
    su_free(home, del->s_remote), del->s_remote = NULL;

    if (del->s_oreq)
      nta_outgoing_destroy(del->s_oreq), del->s_oreq = NULL;
    if (del->s_leg)
      nta_leg_destroy(del->s_leg), del->s_leg = NULL;
    if (del->s_from)
      su_free(home, del->s_from), del->s_from = NULL;

    su_free(home, del);
  }
}

/** Create a new event.
 *
 * The function nea_event_create() creates a new event for the event server.
 */
nea_event_t *nea_event_create(nea_server_t *nes,
			      nea_watcher_f *callback,
			      nea_emagic_t *context,
			      char const *name,
			      char const *subname,
			      char const *default_content_type,
			      char const *accept)
{
  return nea_event_tcreate(nes, callback, context,
			   name, subname,
			   SIPTAG_CONTENT_TYPE_STR(default_content_type),
			   SIPTAG_ACCEPT_STR(accept),
			   TAG_END());
}

/** Create a new event (or subevent) with tags */
nea_event_t *nea_event_tcreate(nea_server_t *nes,
			       nea_watcher_f *callback,
			       nea_emagic_t *context,
			       char const *name,
			       char const *subname,
			       tag_type_t tag, tag_value_t value, ...)
{
  nea_event_t *ev, **pev;
  size_t len = strlen(name);
  ta_list ta;

  if (nes == NULL || callback == NULL || name == NULL)
    return NULL;

  /* Find a matching event */
  if (subname == NULL) {
    for (pev = &nes->nes_events; (ev = *pev); pev = &(*pev)->ev_next) {
      if (strcmp(ev->ev_event->o_type, name) != 0)
	continue;
      SU_DEBUG_5(("nea_event_create(): already event %s\n", name));
      return NULL;
    }
  }
  else {
    for (pev = &nes->nes_events; (ev = *pev); pev = &(*pev)->ev_next) {
      if (strncmp(ev->ev_event->o_type, name, len) != 0 ||
	  ev->ev_event->o_type[len] != '.' ||
	  strcmp(subname, ev->ev_event->o_type + len + 1) != 0)
	continue;
      SU_DEBUG_5(("nea_event_create(): already event %s.%s\n", name, subname));
      return NULL;
    }
  }

  ta_start(ta, tag, value);

  ev = su_zalloc(nes->nes_home, sizeof (*ev));

  if (ev) {
    int reliable = 0;
    sip_content_type_t const *ct = NULL;
    sip_accept_t const *ac = NULL;
    sip_supported_t const *k = NULL;
    sip_require_t const *rq = NULL;
    char const *ct_str = NULL, *ac_str = NULL, *k_str = NULL, *rq_str = NULL;

    unsigned throttle = nes->nes_throttle, min_throttle = nes->nes_min_throttle;
    int eventlist = nes->nes_eventlist;

    tl_gets(ta_args(ta),
	    NEATAG_RELIABLE_REF(reliable),
	    NEATAG_THROTTLE_REF(throttle),
	    NEATAG_MINTHROTTLE_REF(min_throttle),
	    NEATAG_EVENTLIST_REF(eventlist),
	    SIPTAG_CONTENT_TYPE_REF(ct),
	    SIPTAG_CONTENT_TYPE_STR_REF(ct_str),
	    SIPTAG_ACCEPT_REF(ac),
	    SIPTAG_ACCEPT_STR_REF(ac_str),
	    SIPTAG_SUPPORTED_REF(k),
	    SIPTAG_SUPPORTED_STR_REF(k_str),
	    SIPTAG_REQUIRE_REF(rq),
	    SIPTAG_REQUIRE_STR_REF(rq_str),
	    TAG_END());

    ev->ev_callback = callback;
    ev->ev_magic = context;
    ev->ev_event = sip_event_format(nes->nes_home, "%s%s%s",
				    name,
				    subname ? "." : "",
				    subname ? subname : "");

    ev->ev_reliable = reliable != 0;
    ev->ev_throttle = throttle;
    ev->ev_min_throttle = min_throttle;
    ev->ev_eventlist = eventlist;

    if (eventlist && rq == NULL && rq_str == NULL)
      rq_str = "eventlist";

    if (rq)
      ev->ev_require = sip_require_dup(nes->nes_home, rq);
    else if (rq_str)
      ev->ev_require = sip_require_make(nes->nes_home, rq_str);

    if (ev->ev_event) {
#define sip_allow_events_find(k, i) sip_params_find(k->k_items, i)
      if (!sip_allow_events_find(nes->nes_allow_events,
				 ev->ev_event->o_type))
	sip_allow_events_add(nes->nes_home, nes->nes_allow_events,
			     ev->ev_event->o_type);
    }

    if (ct)
      ev->ev_default = sip_accept_make(nes->nes_home, ct->c_type);
    else
      ev->ev_default = sip_accept_make(nes->nes_home, ct_str);

    if (ac == NULL && ac_str == NULL)
      ac_str = ct ? ct->c_type : ct_str;

    if (ac)
      ev->ev_accept = sip_accept_dup(nes->nes_home, ac);
    else
      ev->ev_accept = sip_accept_make(nes->nes_home, ac_str ? ac_str : "");

    if (k)
      ev->ev_supported = sip_supported_dup(nes->nes_home, k);
    else if (k_str)
      ev->ev_supported = sip_supported_make(nes->nes_home, k_str);

    ev->ev_prev = pev;
    *pev = ev;
  }

  ta_end(ta);

  return ev;
}


/* ----------------------------------------------------------------- */
/** Return magic context bound to nea_event.
 *
 * The function returns the magic context bound to the event.
 *
 * @param ev pointer to event object
 *
 * @return
 * The function nea_emagic_get() returns the magic context
 * bound to the event.
 */
nea_emagic_t *nea_emagic_get(nea_event_t *ev)
{
  assert(ev);

  return ev->ev_magic;
}


/* ----------------------------------------------------------------- */
/** Get named event */
nea_event_t *nea_event_get(nea_server_t const *nes, char const *e)
{
  nea_event_t *ev = NULL;

  for (ev = nes->nes_events; ev; ev = ev->ev_next)
    if (e == NULL || strcmp(ev->ev_event->o_type, e) == 0)
      break;

  return ev;
}

/* ----------------------------------------------------------------- */
nta_incoming_t *nea_sub_get_request(nea_sub_t *sub)
{
  assert(sub);

  return sub->s_irq;
}

/** Invoke the event callback.
 *
 * The function nes_watcher_callback() calls the callback provided by the
 * application using the notifier object.
 *
 * @param nes pointer to notifier object
 * @param ev  pointer to event view
 * @param s   pointer to subscription object
 * @param sip pointer to subscribe request
 *
 * @return
 * The function nes_watcher_callback() returns -1 if the notifier object
 * has been destroyed by the callback function, 0 otherwise.
 */
static
int nes_watcher_callback(nea_server_t *nes,
			 nea_event_t *ev,
			 nea_sub_t *s,
			 sip_t const *sip,
			 sip_time_t now)
{
  if (!nes->nes_in_callback) {
    nes->nes_in_callback = 1;
    if (ev->ev_callback && !s->s_reported) {
      nea_subnode_t sn[1];

      nea_subnode_init(sn, s, now);

      if (sn->sn_expires == 0  || sn->sn_state == nea_terminated)
	s->s_reported = 1;

      ev->ev_callback(nes, ev->ev_magic, ev, sn, sip);
    }
    nes->nes_in_callback = 0;

    if (nes->nes_in_list)
      return 0;

    if (nes->nes_pending_destroy) {
      nea_server_destroy(nes);
      return -2;
    }

    if (sip == NULL && nes->nes_pending_flush) {
      int flushed = s->s_pending_flush;
      nea_server_pending_flush(nes);
      if (flushed)
	return -1;
    }
  }

  return 0;
}

/* ----------------------------------------------------------------- */

#if 0
/** Process incoming SUBSCRIBE message.
 *
 * The function nea_server_add() is called when the notifier receives a
 * SUBSCRIBE request without existing event dialog.
 *
 * @param nes pointer to notifier
 * @param local_target optional contact header
 * @param msg pointer to request message
 * @param sip pointer to SIP view to request message
 *
 * @return
 * The function nea_server_add() returns 0 if successful, -1 upon an
 * error.
 *
 */
int nea_server_add(nea_server_t *nes,
		   sip_contact_t const *local_target,
		   msg_t *msg, sip_t *sip)
{
  su_home_t *home = nes->nes_home;
  nea_sub_t *s = NULL;
  url_t target[1];

  s = nea_sub_create(nes);

  s->s_from = sip_from_dup(home, sip->sip_from);

  if (local_target == NULL)
    local_target = nes->nes_eventity_uri;

  s->s_local = sip_contact_dup(nes->nes_home, local_target);

  *target = *local_target->m_url;

  s->s_leg = nta_leg_tcreate(nes->nes_agent, nea_sub_process_incoming, s,
			     SIPTAG_CALL_ID(sip->sip_call_id),
			     SIPTAG_FROM(sip->sip_to), /* local address */
			     SIPTAG_TO(sip->sip_from), /* remote address */
			     URLTAG_URL(target),
			     TAG_END());

  if (s->s_local && s->s_leg) {
    nta_leg_tag(s->s_leg, NULL);
    return 0;
  }
  else {
    nea_sub_destroy(s);
    return -1;
  }
}
#endif

static
int nea_server_callback(nea_sub_t *nes_as_sub,
			nta_leg_t *leg,
			nta_incoming_t *irq,
			sip_t const *sip)
{
  return nea_server_add_irq((nea_server_t *)nes_as_sub, leg, NULL, irq, sip);
}

/** Process incoming request */
int nea_server_add_irq(nea_server_t *nes,
		       nta_leg_t *leg,
		       sip_contact_t const *local_target,
		       nta_incoming_t *irq,
		       sip_t const *sip)
{
  nea_sub_t *s = nea_sub_create(nes);
  if (s == NULL)
    return 500;

  s->s_from = sip_from_dup(nes->nes_home, sip->sip_from);

  if (local_target == NULL)
    local_target = nes->nes_eventity_uri;

  s->s_local = sip_contact_dup(nes->nes_home, local_target);

  if (leg == NULL || leg == nes->nes_leg) {
    url_t target[1];

    *target = *local_target->m_url;

    s->s_leg = nta_leg_tcreate(nes->nes_agent, nea_sub_process_incoming, s,
			       SIPTAG_FROM(sip->sip_to),
			       SIPTAG_TO(sip->sip_from),
			       SIPTAG_CALL_ID(sip->sip_call_id),
			       URLTAG_URL((url_string_t *)target),
			       TAG_NULL());
  }
  else {
    nta_leg_bind(s->s_leg = leg, nea_sub_process_incoming, s);
  }

  if (s->s_leg) {
    if (sip->sip_to->a_tag == NULL) {
      nta_leg_tag(s->s_leg, NULL);
      nta_incoming_tag(irq, nta_leg_get_tag(s->s_leg));
    }
    nta_leg_server_route(s->s_leg, sip->sip_record_route, sip->sip_contact);

    return nea_sub_process_incoming(s, s->s_leg, irq, sip);
  }
  else {
    nea_sub_destroy(s);
    return 500;
  }
}


/* ----------------------------------------------------------------- */

/**Process incoming transactions for event dialog.
 *
 * The nea_sub_process_incoming() processes the transactions for event
 * dialog. Currently, no other methods allowed beside SUBSCRIBE. The
 * SUBSCRIBE is processed by nea_sub_process_subscribe().
 *
 * @param s   pointer to subscriber object
 * @param leg pointer to NTA dialog object
 * @param irq pointer to NTA server transaction
 * @param sip pointer to structure containing SIP headers of the request
 *
 * The nea_sub_process_incoming() returns 0 if successful, SIP error code
 * otherwise.
 */
int nea_sub_process_incoming(nea_sub_t *s,
			     nta_leg_t *leg,
			     nta_incoming_t *irq,
			     sip_t const *sip)
{
  int retval;

  s->s_processing = 1;
  s->s_irq = irq;

  switch(sip->sip_request->rq_method) {
  case sip_method_subscribe:
    retval = nea_sub_process_subscribe(s, leg, irq, sip);
    break;

  default:
    nta_incoming_treply(irq,
			retval = SIP_405_METHOD_NOT_ALLOWED,
			SIPTAG_ALLOW_STR("SUBSCRIBE"),
			TAG_END());
    retval = 405;
  }

  s->s_processing = 0;

  if (s->s_irq)
    nta_incoming_destroy(irq), s->s_irq = NULL;

  if (s->s_pending_flush || s->s_state == nea_embryonic)
    nea_sub_destroy(s);

  return retval;
}


/* ----------------------------------------------------------------- */

/**Process incoming SUBSCRIBE transactions for event dialog.
 *
 * The function nea_sub_process_subscribe() processes the SUBSCRIBE
 * transactions for (possible) event dialog.
 *
 * @param s   pointer to subscriber object
 * @param leg pointer to NTA dialog object
 * @param irq pointer to NTA server transaction
 * @param sip pointer to structure containing SIP headers of the request
 *
 * @return
 * The function nea_sub_process_subscribe() returns 0 if successful, and a
 * SIP error code otherwise.
 */
int nea_sub_process_subscribe(nea_sub_t *s,
			      nta_leg_t *leg,
			      nta_incoming_t *irq,
			      sip_t const *sip)
{
  nea_server_t *nes = s->s_nes;
  su_home_t *home = nes->nes_home;
  nea_event_t *ev = NULL, *ev_maybe = NULL;
  nea_event_view_t *evv = NULL, *evv_maybe = NULL;
  sip_time_t delta = 0, now = sip_now();
  sip_expires_t expires[1] = { SIP_EXPIRES_INIT() };
  sip_unsupported_t *unsupported;
  sip_event_t const *o;
  sip_accept_t const *ac = NULL, *accept = NULL;
  sip_accept_t *a0 = NULL, *a, *a_next, **aa;
  sip_accept_t accept_default[1];
  unsigned proposed_throttle;
  char const *type, *throttle;
  int once, what, supported_eventlist, require_eventlist;

  if (sip->sip_payload && !sip->sip_content_type) {
    nta_incoming_treply(irq, 400, "Missing Content-Type",
			SIPTAG_SERVER_STR(nes->nes_server),
			SIPTAG_ALLOW_EVENTS(nes->nes_allow_events),
			SIPTAG_ALLOW(nes->nes_allow_methods),
			TAG_NULL());
    return 0;
  }

  if (sip->sip_expires &&
      sip->sip_expires->ex_delta > 0 &&
      sip->sip_expires->ex_delta < nes->nes_min_expires) {
    sip_min_expires_t me[1];

    sip_min_expires_init(me);

    me->me_delta = nes->nes_min_expires;

    nta_incoming_treply(irq, 423, "Subscription Interval Too Small",
			SIPTAG_ACCEPT(accept),
			SIPTAG_MIN_EXPIRES(me),
			SIPTAG_SERVER_STR(nes->nes_server),
			SIPTAG_ALLOW_EVENTS(nes->nes_allow_events),
			SIPTAG_ALLOW(nes->nes_allow_methods),
			TAG_NULL());
    return 0;
  }

  /* Check features */
  if (nes->nes_require) {
    unsupported = sip_has_unsupported2(nes->nes_home,
				       sip->sip_supported,
				       sip->sip_require,
				       nes->nes_require);

    if (unsupported) {
      nta_incoming_treply(irq, SIP_421_EXTENSION_REQUIRED,
			  SIPTAG_REQUIRE(nes->nes_require),
			  SIPTAG_UNSUPPORTED(unsupported),
			  SIPTAG_SERVER_STR(nes->nes_server),
			  SIPTAG_ALLOW_EVENTS(nes->nes_allow_events),
			  SIPTAG_ALLOW(nes->nes_allow_methods),
			  TAG_NULL());
      su_free(nes->nes_home, unsupported);

      return 0;
    }
  }

  supported_eventlist = sip_has_feature(sip->sip_supported, "eventlist");
  require_eventlist = sip_has_feature(sip->sip_require, "eventlist");
  supported_eventlist = supported_eventlist || require_eventlist;

  if (s->s_id && (!sip->sip_event ||
		  str0cmp(s->s_id->o_type, sip->sip_event->o_type) != 0 ||
		  str0cmp(s->s_id->o_id, sip->sip_event->o_id))) {
    /* Multiple subscriptions per dialog are not supported. */
    return nta_incoming_treply(irq, 501,
			       "Multiple subscriptions not implemented",
			       SIPTAG_SERVER_STR(nes->nes_server),
			       TAG_NULL());
  }

  /* Check that subscriber asks for a supported event  */
  for (once = 0; ev == NULL ;once++) {
    o = sip->sip_event;

    /* Check that we have a matching event */
    if (o && o->o_type) {
      for (ev = nes->nes_events; ev; ev = ev->ev_next) {
	if (strcmp(o->o_type, ev->ev_event->o_type) == 0) {
	  ev_maybe = ev;

	  if (ev->ev_eventlist) {
	    if (supported_eventlist)
	      break;
	  } else {
	    if (!supported_eventlist)
	      break;
	  }
	}
      }
    }

    if (!ev && !require_eventlist)
      ev = ev_maybe;

    if (ev || once)
      break;

    /* Ask the application either to
       1) add a new event or assing us an event/payload (0),
       2) take care of transaction (positive), or
       3) drop request (negative).
    */
    if ((what = nes_new_event_callback(nes, &ev, &evv, irq, sip)) < 0)
      break;
    if (what > 0) {
      s->s_irq = NULL;
      return 0;
    }
  }

  if (ev_maybe == NULL && ev == NULL) {
    nta_incoming_treply(irq, SIP_489_BAD_EVENT,
			SIPTAG_SERVER_STR(nes->nes_server),
			SIPTAG_ALLOW_EVENTS(nes->nes_allow_events),
			SIPTAG_ALLOW(nes->nes_allow_methods),
			NULL);
    return 0;
  } else if (ev == NULL) {
    ev = ev_maybe;

    unsupported = sip_has_unsupported(nes->nes_home, ev->ev_supported,
				      sip->sip_require);

    nta_incoming_treply(irq, SIP_420_BAD_EXTENSION,
			SIPTAG_UNSUPPORTED(unsupported),
			SIPTAG_REQUIRE(ev->ev_require),
			SIPTAG_SUPPORTED(ev->ev_supported),
			SIPTAG_SERVER_STR(nes->nes_server),
			SIPTAG_ALLOW_EVENTS(nes->nes_allow_events),
			SIPTAG_ALLOW(nes->nes_allow_methods),
			TAG_NULL());

    su_free(nes->nes_home, unsupported);

    return 0;
  }

  if (sip->sip_accept)
    accept = sip->sip_accept;
  else if (evv && evv->evv_content_type) {
    /* Generate accept header from event view specified by application */
    sip_accept_init(accept_default);
    accept_default->ac_type = evv->evv_content_type->c_type;
    accept_default->ac_subtype = evv->evv_content_type->c_subtype;

    accept = a0;
  }
  else
    accept = ev->ev_default;

  for (once = 0; evv == NULL ;once++) {
    /* If there are multiple accept values with different Q values,
       insertion sort by Q value */
    for (ac = accept->ac_next; ac; ac = ac->ac_next) {
      if (ac->ac_q != accept->ac_q) {
	if ((a0 = sip_accept_dup(home, accept))) {
	  /* Sort the accept list by Q values */
	  for (a = a0, accept = NULL; a; a = a_next) {
	    a_next = a->ac_next;

	    for (aa = (sip_accept_t **)&accept;
		 *aa && sip_q_value((*aa)->ac_q) >= sip_q_value(a->ac_q);
		 aa = &(*aa)->ac_next)
	      ;

	    a->ac_next = *aa; *aa = a; 	/* Insert */
	  }
	}

	break;
      }
    }

    /* Check that subscriber asks for a supported content type */
    for (ac = accept; ac; ac = ac->ac_next) {
      int i;

      if (ac->ac_type == NULL || ac->ac_subtype == NULL)
	continue;

      /* Check all supported content types v. accept */
      for (i = 0; (evv = ev->ev_views[i]); i++) {
	assert(evv->evv_content_type && evv->evv_content_type->c_type);

	if (strcmp(ac->ac_type, "*/*") == 0)
	  break;

	type = evv->evv_content_type->c_type;

	if ((su_casematch(ac->ac_type, type)) ||
	    (su_casematch(ac->ac_subtype, "*") &&
	     su_casenmatch(ac->ac_type, type,
			 ac->ac_subtype - ac->ac_type))) {
	  if (evv_maybe == NULL)
	    evv_maybe = evv;
	}
      }

      if (evv)			/* Found */
	break;
    }

    /* Free the sorted Accept list */
    for (a = a0; a; a = a_next)
      a_next = a->ac_next, su_free(home, a);

    if (!evv)
      evv = evv_maybe;

    if (evv || once)
      break;

    /* Ask the application either to
       1) add a new event view or assign us an event view (0),
       2) take care of transaction (positive), or
       3) drop request (negative).
    */
    if ((what = nes_new_event_callback(nes, &ev, &evv, irq, sip)) < 0)
      break;
    if (what > 0) {
      s->s_irq = NULL;
      return 0;
    }
  }

  if (evv == NULL) {
    SU_DEBUG_3(("nea_server: event %s rejected %u %s\n",
		ev->ev_event->o_type, SIP_406_NOT_ACCEPTABLE));

    /* There is no media acceptable to watcher */
    return nta_incoming_treply(irq, SIP_406_NOT_ACCEPTABLE,
			       SIPTAG_ACCEPT(ev->ev_accept),
			       SIPTAG_SERVER_STR(nes->nes_server),
			       SIPTAG_ALLOW_EVENTS(nes->nes_allow_events),
			       SIPTAG_ALLOW(nes->nes_allow_methods),
			       TAG_NULL());
  }

  /* Do not change private view */
  if (s->s_view && s->s_view->evv_primary == evv)
    evv = s->s_view;

  /* Set throttle */
  if (sip->sip_event &&
      (throttle = sip_params_find(sip->sip_event->o_params, "throttle="))) {
    proposed_throttle = strtoul(throttle, NULL, 10);

    if (proposed_throttle < evv->evv_min_throttle)
      proposed_throttle = evv->evv_min_throttle;
  } else
    proposed_throttle = evv->evv_throttle;

  s->s_throttle = proposed_throttle;

  /* Update route, store remote contact */
  nta_leg_server_route(leg, sip->sip_record_route, sip->sip_contact);
  su_free(home, s->s_remote);
  s->s_remote = sip_contact_dup(home, sip->sip_contact);

  /* Store content-type and body */
  if (sip->sip_content_type) {
    su_free(home, s->s_content_type);
    s->s_content_type = sip_content_type_dup(home, sip->sip_content_type);
    su_free(home, s->s_payload);
    s->s_payload = sip_payload_dup(home, sip->sip_payload);
  }

  /* Calculate expiration time for subscription */
  delta = sip_contact_expires(NULL, sip->sip_expires, sip->sip_date,
			      nes->nes_expires, now);
  if (delta > nes->nes_max_expires)
    delta = nes->nes_max_expires;
  expires->ex_delta = delta;

  if (s->s_subscribed == 0)
    s->s_subscribed = now;
  s->s_expires = now + delta;
  /* s->s_accept = sip_accept_dup(home, accept); */
  if (s->s_id == NULL)
    s->s_id = sip_event_dup(home, sip->sip_event);
  s->s_event = ev;
  s->s_eventlist = supported_eventlist;
  nea_sub_assign_view(s, evv);
  s->s_updated = evv->evv_updated - 1;  /* Force notify */

  if (nes->nes_202_before_notify) {
    nta_incoming_treply(irq, SIP_202_ACCEPTED,
			SIPTAG_SERVER_STR(nes->nes_server),
			SIPTAG_ALLOW_EVENTS(nes->nes_allow_events),
			SIPTAG_ALLOW(nes->nes_allow_methods),
			SIPTAG_REQUIRE(ev->ev_require),
			SIPTAG_SUPPORTED(ev->ev_supported),
			SIPTAG_EXPIRES(expires),
			SIPTAG_CONTACT(s->s_local),
			TAG_END());
    nta_incoming_destroy(irq), s->s_irq = irq = NULL;
  }

  /* Callback for checking subscriber authorization */
  if (nes_watcher_callback(nes, ev, s, sip, now) < 0) {
    if (irq) {
      nta_incoming_treply(irq, SIP_503_SERVICE_UNAVAILABLE, TAG_END());
      nta_incoming_destroy(irq);
    }
    return -1;
  }



  evv = s->s_view;  /* Callback can change event view */

  if (s->s_state == nea_embryonic)
    nea_sub_auth(s, nea_pending, NEATAG_FAKE(1), TAG_END());

  if (s->s_updated != evv->evv_updated && !(irq && s->s_rejected))
    nea_sub_notify(nes, s, now, TAG_END());

  if (irq) {
    if (s->s_rejected)
      nta_incoming_treply(irq, SIP_403_FORBIDDEN,
			  SIPTAG_SERVER_STR(nes->nes_server),
			  TAG_END());
    else if (s->s_state == nea_active)
      nta_incoming_treply(irq, SIP_200_OK,
			  SIPTAG_REQUIRE(ev->ev_require),
			  SIPTAG_SUPPORTED(ev->ev_supported),
			  SIPTAG_EXPIRES(expires),
			  SIPTAG_SERVER_STR(nes->nes_server),
			  SIPTAG_CONTACT(s->s_local),
			  SIPTAG_ALLOW_EVENTS(nes->nes_allow_events),
			  SIPTAG_ALLOW(nes->nes_allow_methods),
			  TAG_END());
    else
      nta_incoming_treply(irq, SIP_202_ACCEPTED,
			  SIPTAG_REQUIRE(ev->ev_require),
			  SIPTAG_SUPPORTED(ev->ev_supported),
			  SIPTAG_EXPIRES(expires),
			  SIPTAG_SERVER_STR(nes->nes_server),
			  SIPTAG_ALLOW_EVENTS(nes->nes_allow_events),
			  SIPTAG_ALLOW(nes->nes_allow_methods),
			  SIPTAG_CONTACT(s->s_local),
			  TAG_END());
  }

  return 0;
}

/* ----------------------------------------------------------------- */
/**Notify subscriber
 *
 * The function nea_sub_notify() sends a notification to the subscriber. The
 * event type is specified by subscriber event, payload type and payload in
 * the event view. The responses to the NOTIFY transaction are
 * processed by response_to_notify().
 *
 * @param nes pointer to the notifier object
 * @param s   pointer to the subscription object
 * @param now current SIP time (if 0, no body is sent,
 *            but updated Subscription-State header only
 * @param tag,value,... tag list
 *
 */
int nea_sub_notify(nea_server_t *nes, nea_sub_t *s,
		   sip_time_t now,
		   tag_type_t tag, tag_value_t value, ...)
{
  int notified = 0;
  ta_list ta;
  int suppress = now != 0;
  nea_event_t *ev = s->s_event;
  nea_state_t substate = s->s_state;

  if (s->s_pending_flush || (s->s_oreq && substate != nea_terminated)) {
    if (ev && ev->ev_throttling > s->s_updated)
      ev->ev_throttling = s->s_updated;
    return 0;
  }

  if (s->s_oreq)
    nta_outgoing_destroy(s->s_oreq), s->s_oreq = NULL;

  assert(s->s_view); assert(ev);

  if (suppress && s->s_view->evv_updated == s->s_updated)
    return 0;

  if (now == 0)
    now = sip_now();

  if (s->s_notified + s->s_throttle > now &&
      /* Do not throttle state termination notification */
      substate != nea_terminated &&
      (long)(s->s_expires - now) > 0) {
    if (ev->ev_throttling > s->s_updated && !s->s_fake)
      ev->ev_throttling = s->s_updated;
    nes->nes_throttled++;
    return 0;
  }

  ta_start(ta, tag, value);
  {
    sip_subscription_state_t ss[1];
    char expires[32];
    sip_param_t params[] = { NULL, NULL, NULL };
    char const *reason = NULL;
    int fake = 0;
    char reason_buf[64];
    unsigned retry_after = (unsigned)-1;
    char retry_after_buf[64];
    int i = 0;
    nta_response_f *callback;
    nea_event_view_t *evv = s->s_view;
    nea_event_queue_t *evq, *n_evq;

    assert(ev);

    sip_subscription_state_init(ss);

    tl_gets(ta_args(ta),
	    NEATAG_REASON_REF(reason),
	    NEATAG_FAKE_REF(fake), /* XXX - semantics??? */
	    NEATAG_RETRY_AFTER_REF(retry_after),
	    TAG_END());

    if (substate == nea_terminated) {
      if (reason)
	snprintf(reason_buf, sizeof(reason_buf),
		 "reason=%s", reason), params[i++] = reason_buf;
      if (retry_after != (unsigned)-1)
	snprintf(retry_after_buf, sizeof(retry_after_buf),
		 "retry-after=%u", retry_after), params[i++] = retry_after_buf;
    }
    else if ((long)(s->s_expires - now) <= 0) {
      substate = nea_terminated;
      params[i++] = "reason=timeout";
    }
    else {
      snprintf(expires, sizeof(expires),
	       "expires=%lu", (unsigned long)(s->s_expires - now));
      params[i++] = expires;
    }

    ss->ss_params = params;

    switch (substate) {
    case nea_extended: ss->ss_substate = s->s_extended; break;
    case nea_pending:  ss->ss_substate = "pending"; break;
    case nea_active:   ss->ss_substate = "active"; break;
    case nea_terminated: ss->ss_substate = "terminated"; break;
      /* Do not send notifys for embryonic subscriptions */
    case nea_embryonic:
      ta_end(ta);
      return 0;
    }

    callback = substate != nea_terminated ? response_to_notify : NULL;

    for (evq = evv->evv_head; evq->evq_next; evq = evq->evq_next) {
      if (evq->evq_next->evq_updated <= s->s_updated)
	break;
    }

    suppress = (s->s_view->evv_updated == s->s_updated);

    n_evq = evq->evq_payload ? evq : evv->evv_primary->evv_head;

    s->s_oreq =
      nta_outgoing_tcreate(s->s_leg,
			   callback, s, NULL,
			   SIP_METHOD_NOTIFY, NULL,
			   SIPTAG_SUBSCRIPTION_STATE(ss),
			   SIPTAG_REQUIRE(ev->ev_require),
			   SIPTAG_SUPPORTED(ev->ev_supported),
			   SIPTAG_USER_AGENT_STR(nes->nes_server),
			   SIPTAG_CONTACT(s->s_local),
			   SIPTAG_EVENT(s->s_id),
			   TAG_IF(!suppress,
				  SIPTAG_CONTENT_TYPE(n_evq->evq_content_type)),
			   TAG_IF(!suppress,
				  SIPTAG_PAYLOAD(n_evq->evq_payload)),
			   ta_tags(ta));


    notified = s->s_oreq != 0;

    if (notified) {
      s->s_notified = now;
      s->s_state = substate; /* XXX - we need state for "waiting" */
      s->s_latest = evq->evq_version;  /* Application version */
      s->s_updated = evq->evq_updated; /* Internal version */
      if (ev->ev_throttling > s->s_updated)
	ev->ev_throttling = s->s_updated;
    }

    if (callback == NULL) {
      nta_outgoing_destroy(s->s_oreq), s->s_oreq = NULL;
      /* Inform the application of a subscriber leaving the subscription. */
      nes_watcher_callback(nes, ev, s, NULL, now);
    }
  }
  ta_end(ta);

  return notified;
}

/* ----------------------------------------------------------------- */
/**Process responses to the NOTIFY.
 *
 * The response_to_notify() processes the responses to the NOTIFY request.
 * If there was an error with delivering the NOTIFY, the subscription is
 * considered terminated.
 *
 * @param s   pointer to subscription object
 */
int response_to_notify(nea_sub_t *s,
		       nta_outgoing_t *oreq,
		       sip_t const *sip)
{
  nea_server_t *nes = s->s_nes;
  int status = sip->sip_status->st_status;
  sip_time_t now = sip_now();

  if (status < 200)
    return 0;

  nta_outgoing_destroy(s->s_oreq), s->s_oreq = NULL;

  if (status < 300) {
    if (s->s_view->evv_updated != s->s_updated) {
      if (s->s_notified + s->s_throttle <= now)
	nea_sub_notify(nes, s, now, TAG_END());
      else
	nes->nes_throttled++;
    }
  }

  if (s->s_state == nea_terminated || status >= 300) {
    SU_DEBUG_5(("nea_server: removing subscriber " URL_PRINT_FORMAT "\n",
		URL_PRINT_ARGS(s->s_from->a_url)));
    /* Inform the application of a subscriber leaving the subscription. */
    nes_watcher_callback(nes, s->s_event, s, NULL, now);
  }

  return 0;
}

/* ----------------------------------------------------------------- */

/** Get number of active subscribers.
 *
 * The function nea_server_active() counts the number of active subscribers
 * watching the specified view. If the view is not specified (@a ev is @c
 * NULL), it counts the number of all subscribers.
 *
 * @param nes notifier
 * @param ev  event
 *
 * The function nea_server_active() returns number of active subscribers.
 */
int nea_server_active(nea_server_t *nes, nea_event_t const *ev)
{
  int n = 0;
  nea_sub_t *s = NULL;

  /* Count the number of subscribers watching this event */
  for (s = nes->nes_subscribers; s ; s = s->s_next)
    if (!s->s_pending_flush && s->s_state == nea_active
	&& (ev == NULL || ev == s->s_event))
      n++;

  return n;
}

/** Get number of non-embryonic subscribers.
 *
 * The function nea_server_non_embryonic() counts the number of pending,
 * active or terminated subscribers watching the specified view. If the view
 * is not specified (@a ev is @c NULL), it counts the number of all
 * subscribers.
 *
 * @param nes notifier
 * @param ev  event view
 *
 * The function nea_server_active() returns number of active subscribers.
 */
int nea_server_non_embryonic(nea_server_t *nes, nea_event_t const *ev)
{
  int n = 0;
  nea_sub_t *s = NULL;

  /* Count the number of subscribers watching this event */
  for (s = nes->nes_subscribers; s ; s = s->s_next)
    if (!s->s_pending_flush && s->s_state != nea_embryonic
	&& (ev == NULL || ev == s->s_event))
      n++;

  return n;
}

/** Set application version number */
int nea_sub_version(nea_sub_t *s, unsigned version)
{
  if (s)
    return s->s_version = version;
  return 0;
}

/** Authorize a subscription.
 *
 * Application can modify the subscription state and authorize the user.
 * The subscription state has following simple state diagram:
 *
 * @code
 *               +---------------+ +------------------+
 *               |	         | |       	      |
 * +-----------+ |  +---------+  V |  +------------+  V  +------------+
 * | embryonic |-+->| pending |--+-+->| authorized |--+->| terminated |
 * +-----------+    +---------+       +------------+     +------------+
 *
 * @endcode
 *
 * @TAGS
 * IF NEATAG_VIEW(view) is included in tagged arguments, @a view is assigned
 * to the subscriber and the content from the view is delivered to the
 * subscriber.
 *
 * If NEATAG_FAKE(1) is included in tags, content marked as 'fake' is
 * delivered to the subscriber.
 *
 * @retval 0 if successful
 * @retval -1 upon an error
 */
int nea_sub_auth(nea_sub_t *s,
		 nea_state_t state,
		 tag_type_t tag, tag_value_t value, ...)
{

  ta_list ta;
  int retval, embryonic, rejected = 0;
  int fake = 0;
  char const *reason = NULL;
  nea_event_view_t *evv = NULL;

  if (s == NULL)
    return -1;
  if (state == nea_embryonic)
    return -1;
  if (state < s->s_state)
    return -1;

  ta_start(ta, tag, value);

  embryonic = s->s_state == nea_embryonic;

  s->s_state = state;

  if (tl_gets(ta_args(ta), NEATAG_VIEW_REF(evv), TAG_END()) && evv) {
    nea_sub_assign_view(s, evv);
  }
  else {
    if (tl_gets(ta_args(ta), NEATAG_FAKE_REF(fake), TAG_END()))
      s->s_fake = fake;

    if (s->s_view && s->s_view->evv_fake != s->s_fake) {
      for (evv = s->s_view->evv_primary; evv; evv = evv->evv_next)
	if (!evv->evv_private && evv->evv_fake == s->s_fake) {
	  nea_sub_assign_view(s, evv);
	  break;
	}
    }
  }

  tl_gets(ta_args(ta), NEATAG_REASON_REF(reason), TAG_END());

  rejected = su_casematch(reason, "rejected");

  if (state == nea_terminated && embryonic && rejected && s->s_irq)
    retval = 0, s->s_rejected = 1;
  else
    retval = nea_sub_notify(s->s_nes, s, 0, ta_tags(ta));

  ta_end(ta);

  return retval;
}

/** Obtain a list of subscribers */
nea_subnode_t const **nea_server_get_subscribers(nea_server_t *nes,
						 nea_event_t const *ev)
{
  nea_sub_t *s;
  nea_subnode_t **sn_list, *sn;
  int i, n;
  sip_time_t now = sip_now();

  n = nea_server_non_embryonic(nes, ev);
  if (n == 0)
    return NULL;

  sn_list = su_zalloc(nes->nes_home,
		      (n + 1) * sizeof(sn) + n * sizeof(*sn));
  if (sn_list) {
    sn = (nea_subnode_t *)(sn_list + n + 1);

    for (i = 0, s = nes->nes_subscribers; s; s = s->s_next) {
      if (!s->s_pending_flush && s->s_state != nea_embryonic
	  && (ev == NULL || ev == s->s_event)) {
	assert(i < n);
	nea_subnode_init(sn, s, now);
	sn_list[i++] = sn++;
      }
    }

    nes->nes_in_list++;

    sn_list[i] = NULL;
  }

  return (nea_subnode_t const **)sn_list;
}

/** Free a list of subscriptions. */
void nea_server_free_subscribers(nea_server_t *nes,
				 nea_subnode_t const **sn_list)
{
  if (sn_list) {
    su_free(nes->nes_home, (void *)sn_list);
    if (--nes->nes_in_list == 0 && nes->nes_pending_flush)
      nea_server_pending_flush(nes);
  }
}

/* ----------------------------------------------------------------- */
void nes_event_timer(nea_server_t *srvr,
		     su_timer_t *timer,
		     su_timer_arg_t *arg)
{
  nea_server_t *nes = (nea_server_t *) arg;
  sip_time_t now = sip_now();
  nea_sub_t *s = NULL, *s_next = NULL;
  su_root_t *root = su_timer_root(timer);

  su_timer_set(timer, nes_event_timer, nes);

  nes->nes_in_list++;

  /* Notify and terminate expired subscriptions */
  for (s = nes->nes_subscribers; s; s = s_next) {
    s_next = s->s_next;
    if (s->s_state == nea_terminated)
      continue;
    if ((int)(now - s->s_expires) >= 0) {
      nea_sub_notify(nes, s, now, TAG_END());
      /* Yield so we can handle received packets */
      su_root_yield(root);
    }
  }

  if (--nes->nes_in_list == 0 && nes->nes_pending_flush)
    nea_server_pending_flush(nes);

  if (nes->nes_throttled)
    nea_server_notify(nes, NULL);

  return;
}
