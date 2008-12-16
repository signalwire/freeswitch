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

/**@file sofia-sip/nua.h
 * @brief Sofia-SIP User Agent Library API
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Feb 14 17:09:44 2001 ppessi
 */

#ifndef NUA_H
/** Defined when <sofia-sip/nua.h> has been included. */
#define NUA_H

#ifndef SU_WAIT_H
#include <sofia-sip/su_wait.h>
#endif


#ifndef URL_H
#include <sofia-sip/url.h>
#endif

#ifndef SIP_H
#include <sofia-sip/sip.h>
#endif

#ifndef NUA_TAG_H
#include <sofia-sip/nua_tag.h>
#endif

SOFIA_BEGIN_DECLS

#ifndef NUA_MAGIC_T
#define NUA_MAGIC_T void
#endif
/** Application context for NUA agent. */
typedef NUA_MAGIC_T nua_magic_t;

#ifndef NUA_HMAGIC_T
#define NUA_HMAGIC_T void
#endif
/** Application context for NUA handle. */
typedef NUA_HMAGIC_T nua_hmagic_t;

/**Network change event levels given to NUTAG_DETECT_NETWORK_UPDATES().
 *
 * @sa NUTAG_DETECT_NETWORK_UPDATES(), #nua_i_network_changed
 *
 * @since New in @VERSION_1_12_2.
 */
typedef enum nua_nw_detector_e {
  NUA_NW_DETECT_NOTHING = 0,
  NUA_NW_DETECT_ONLY_INFO,
  NUA_NW_DETECT_TRY_FULL
} nua_nw_detector_t;

/** Events */
typedef enum nua_event_e {
  /* Event used by stack internally */
  nua_i_none = -1,

  /* Indications */
  nua_i_error,			/**< Error indication */

  nua_i_invite,			/**< Incoming call INVITE */
  nua_i_cancel,			/**< Incoming INVITE has been cancelled */
  nua_i_ack,			/**< Final response to INVITE has been ACKed */
  nua_i_fork,			/**< Outgoing call has been forked */
  nua_i_active,			/**< A call has been activated */
  nua_i_terminated,		/**< A call has been terminated */
  nua_i_state,		        /**< Call state has changed */

  nua_i_outbound,		/**< Status from outbound processing */

  nua_i_bye,			/**< Incoming BYE call hangup */
  nua_i_options,		/**< Incoming OPTIONS */
  nua_i_refer,			/**< Incoming REFER call transfer */
  nua_i_publish,		/**< Incoming PUBLISH */
  nua_i_prack,			/**< Incoming PRACK */
  nua_i_info,			/**< Incoming session INFO */
  nua_i_update,			/**< Incoming session UPDATE */
  nua_i_message,		/**< Incoming MESSAGE */
  nua_i_chat,			/**< Incoming chat MESSAGE  */
  nua_i_subscribe,		/**< Incoming SUBSCRIBE  */
  nua_i_subscription,		/**< Incoming subscription to be authorized */
  nua_i_notify,			/**< Incoming event NOTIFY */
  nua_i_method,			/**< Incoming, unknown method */

  nua_i_media_error,		/**< Offer-answer error indication */

  /* Responses */
  nua_r_set_params,		/**< Answer to nua_set_params() or
				 * nua_get_hparams(). */
  nua_r_get_params,		/**< Answer to nua_get_params() or
				 * nua_get_hparams(). */
  nua_r_shutdown,		/**< Answer to nua_shutdown() */
  nua_r_notifier,		/**< Answer to nua_notifier() */
  nua_r_terminate,		/**< Answer to nua_terminate() */
  nua_r_authorize,		/**< Answer to nua_authorize()  */

  /* SIP responses */
  nua_r_register,		/**< Answer to outgoing REGISTER */
  nua_r_unregister,		/**< Answer to outgoing un-REGISTER */
  nua_r_invite,		        /**< Answer to outgoing INVITE */
  nua_r_cancel,			/**< Answer to outgoing CANCEL */
  nua_r_bye,			/**< Answer to outgoing BYE */
  nua_r_options,		/**< Answer to outgoing OPTIONS */
  nua_r_refer,			/**< Answer to outgoing REFER */
  nua_r_publish,		/**< Answer to outgoing PUBLISH */
  nua_r_unpublish,		/**< Answer to outgoing un-PUBLISH */
  nua_r_info,		        /**< Answer to outgoing INFO */
  nua_r_prack,			/**< Answer to outgoing PRACK */
  nua_r_update,		        /**< Answer to outgoing UPDATE */
  nua_r_message,		/**< Answer to outgoing MESSAGE */
  nua_r_chat,			/**< Answer to outgoing chat message */
  nua_r_subscribe,		/**< Answer to outgoing SUBSCRIBE */
  nua_r_unsubscribe,		/**< Answer to outgoing un-SUBSCRIBE */
  nua_r_notify,			/**< Answer to outgoing NOTIFY */
  nua_r_method,			/**< Answer to unknown outgoing method */

  nua_r_authenticate,		/**< Answer to nua_authenticate() */

  /* Internal events: nua hides them from application */
  nua_r_redirect,
  nua_r_destroy,
  nua_r_respond,
  nua_r_nit_respond,
  nua_r_ack,			/*#< Answer to ACK */

  /* NOTE: Post 1.12 release events come here (below) to keep ABI
     compatibility! */
  nua_i_network_changed,        /**< Local IP(v6) address has changed.
				   @NEW_1_12_2 */
  nua_i_register		/**< Incoming REGISTER. @NEW_1_12_4. */
} nua_event_t;

typedef struct event_s {
  nua_handle_t *e_nh;
  int           e_event;
  short         e_always;
  short         e_status;
  char const   *e_phrase;
  msg_t        *e_msg;
  tagi_t        e_tags[1];
} nua_event_data_t;

/** NUA API version */
#define NUA_VERSION "2.0"
/** NUA module version */
SOFIAPUBVAR char const nua_version[];

/** Typedef of NUA event callback. */
typedef void (*nua_callback_f)(nua_event_t event,
			       int status, char const *phrase,
			       nua_t *nua, nua_magic_t *magic,
			       nua_handle_t *nh, nua_hmagic_t *hmagic,
			       sip_t const *sip,
			       tagi_t tags[]);

/** Create a NUA agent. */
SOFIAPUBFUN nua_t *nua_create(su_root_t *root,
			      nua_callback_f callback,
			      nua_magic_t *magic,
			      tag_type_t tag, tag_value_t value,
			      ...);

/** Shutdown NUA stack. */
SOFIAPUBFUN void nua_shutdown(nua_t *nua);

/** Destroy the NUA stack. */
SOFIAPUBFUN void nua_destroy(nua_t *nua);

/** Fetch callback context from nua. */
SOFIAPUBFUN nua_magic_t *nua_magic(nua_t *nua);

/** Set NUA parameters. */
SOFIAPUBFUN void nua_set_params(nua_t *, tag_type_t, tag_value_t, ...);

/** Get NUA parameters. */
SOFIAPUBFUN void nua_get_params(nua_t *nua, tag_type_t, tag_value_t, ...);

/** Obtain default operation handle of the NUA stack object. */
SOFIAPUBFUN nua_handle_t *nua_default(nua_t *nua);

/** Create an operation handle */
SOFIAPUBFUN nua_handle_t *nua_handle(nua_t *nua, nua_hmagic_t *hmagic,
				     tag_type_t, tag_value_t, ...);

/** Destroy a handle */
SOFIAPUBFUN void nua_handle_destroy(nua_handle_t *h);

/** Make a new reference to handle */
SOFIAPUBFUN nua_handle_t *nua_handle_ref(nua_handle_t *);

/** Destroy reference to handle */
SOFIAPUBFUN int nua_handle_unref(nua_handle_t *);

/** Bind a callback context to an operation handle. */
SOFIAPUBFUN void nua_handle_bind(nua_handle_t *nh, nua_hmagic_t *magic);

/** Fetch a callback context from an operation handle. */
SOFIAPUBFUN nua_hmagic_t *nua_handle_magic(nua_handle_t *nh);

/** Set handle parameters. */
SOFIAPUBFUN void nua_set_hparams(nua_handle_t *, tag_type_t, tag_value_t, ...);

/** Get handle parameters. */
SOFIAPUBFUN void nua_get_hparams(nua_handle_t *, tag_type_t, tag_value_t, ...);

/** Check if operation handle is used for INVITE */
SOFIAPUBFUN int nua_handle_has_invite(nua_handle_t const *nh);

/** Check if operation handle has been used with outgoing SUBSCRIBE of REFER request. */
SOFIAPUBFUN int nua_handle_has_subscribe(nua_handle_t const *nh);

/** Check if operation handle has been used with nua_register() or nua_unregister(). */
SOFIAPUBFUN int nua_handle_has_register(nua_handle_t const *nh);

/** Check if operation handle has an active call */
SOFIAPUBFUN int nua_handle_has_active_call(nua_handle_t const *nh);

/** Check if operation handle has a call on hold */
SOFIAPUBFUN int nua_handle_has_call_on_hold(nua_handle_t const *nh);

/** Check if handle has active event subscriptions (refers sent). */
SOFIAPUBFUN int nua_handle_has_events(nua_handle_t const *nh);

/** Check if operation handle has active registrations */
SOFIAPUBFUN int nua_handle_has_registrations(nua_handle_t const *nh);

/** Get the remote address (From/To header) of operation handle */
SOFIAPUBFUN sip_to_t const *nua_handle_remote(nua_handle_t const *nh);

/** Get the local address (From/To header) of operation handle  */
SOFIAPUBFUN sip_to_t const *nua_handle_local(nua_handle_t const *nh);

/** Get name for NUA event. */
SOFIAPUBFUN char const *nua_event_name(nua_event_t event);

/** Get name for NUA callstate. */
SOFIAPUBFUN char const *nua_callstate_name(enum nua_callstate state);

/** Return name of subscription state. @NEW_1_12_5. */
SOFIAPUBFUN char const *nua_substate_name(enum nua_substate substate);

/** Convert string to enum nua_substate. @NEW_1_12_5. */
SOFIAPUBFUN enum nua_substate nua_substate_make(char const *sip_substate);

/** Send SIP REGISTER request to the registrar. */
SOFIAPUBFUN void nua_register(nua_handle_t *nh, tag_type_t, tag_value_t, ...);

/** Unregister. */
SOFIAPUBFUN void nua_unregister(nua_handle_t *nh, tag_type_t, tag_value_t, ...);

/** Place a call using SIP INVITE method. */
SOFIAPUBFUN void nua_invite(nua_handle_t *nh, tag_type_t, tag_value_t, ...);

/** Acknowledge a succesfull response to INVITE request. */
SOFIAPUBFUN void nua_ack(nua_handle_t *nh, tag_type_t, tag_value_t, ...);

/** Acknowledge a reliable preliminary response to INVITE request. */
SOFIAPUBFUN void nua_prack(nua_handle_t *nh, tag_type_t, tag_value_t, ...);

/** Query capabilities from server */
SOFIAPUBFUN void nua_options(nua_handle_t *nh, tag_type_t, tag_value_t, ...);

/** Send PUBLISH request to publication server. */
SOFIAPUBFUN void nua_publish(nua_handle_t *nh, tag_type_t, tag_value_t, ...);

/** Send un-PUBLISH request to publication server. */
SOFIAPUBFUN void nua_unpublish(nua_handle_t *nh, tag_type_t, tag_value_t, ...);

/** Send an instant message. */
SOFIAPUBFUN void nua_message(nua_handle_t *nh, tag_type_t, tag_value_t, ...);

/** Send a chat message. */
SOFIAPUBFUN void nua_chat(nua_handle_t *nh, tag_type_t, tag_value_t, ...);

/** Send an INFO request. */
SOFIAPUBFUN void nua_info(nua_handle_t *nh, tag_type_t, tag_value_t, ...);

/** Subscribe a SIP event. */
SOFIAPUBFUN void nua_subscribe(nua_handle_t *nh, tag_type_t, tag_value_t, ...);

/** Unsubscribe an event. */
SOFIAPUBFUN void nua_unsubscribe(nua_handle_t *, tag_type_t, tag_value_t, ...);

/** Send a NOTIFY message. */
SOFIAPUBFUN void nua_notify(nua_handle_t *, tag_type_t, tag_value_t, ...);

/** Create an event server. */
SOFIAPUBFUN void nua_notifier(nua_handle_t *, tag_type_t, tag_value_t, ...);

/** Terminate an event server. */
SOFIAPUBFUN void nua_terminate(nua_handle_t *, tag_type_t, tag_value_t, ...);

/** Transfer a call. */
SOFIAPUBFUN void nua_refer(nua_handle_t *, tag_type_t, tag_value_t, ...);

/** Update a call */
SOFIAPUBFUN void nua_update(nua_handle_t *, tag_type_t, tag_value_t, ...);

/** Hangdown a call. */
SOFIAPUBFUN void nua_bye(nua_handle_t *, tag_type_t, tag_value_t, ...);

/** Cancel an INVITE operation */
SOFIAPUBFUN void nua_cancel(nua_handle_t *, tag_type_t, tag_value_t, ...);

/** Authenticate an operation. */
SOFIAPUBFUN void nua_authenticate(nua_handle_t *, tag_type_t, tag_value_t, ...);

/** Authorize a subscriber. */
SOFIAPUBFUN void nua_authorize(nua_handle_t *, tag_type_t, tag_value_t, ...);

/*# Redirect an operation. @deprecated */
SOFIAPUBFUN void nua_redirect(nua_handle_t *, tag_type_t, tag_value_t, ...);

/** Send a request message with an extension method. */
SOFIAPUBFUN void nua_method(nua_handle_t *, tag_type_t, tag_value_t, ...);

/** Respond to a request with given status code and phrase. */
SOFIAPUBFUN void nua_respond(nua_handle_t *nh,
			     int status, char const *phrase,
			     tag_type_t, tag_value_t,
			     ...);

/** Check if event can be responded with nua_respond() */
SOFIAPUBFUN int nua_event_is_incoming_request(nua_event_t e);

/** Cast a #nua_handle_t pointer to a #su_home_t. */
#define nua_handle_home(nh) ((su_home_t *)(nh))

/** Generate an instance identifier. */
SOFIAPUBFUN char const *nua_generate_instance_identifier(su_home_t *);

#ifndef NUA_SAVED_EVENT_T
#define NUA_SAVED_EVENT_T struct nua_saved_event *
#endif
/** Abstract type for saved nua events. */
typedef NUA_SAVED_EVENT_T nua_saved_event_t;

/** Save last nua event */
SOFIAPUBFUN int nua_save_event(nua_t *nua, nua_saved_event_t return_saved[1]);

/** Get information from saved event */
SOFIAPUBFUN nua_event_data_t const *nua_event_data(nua_saved_event_t const saved[1]);

/** Destroy a save nua event */
SOFIAPUBFUN void nua_destroy_event(nua_saved_event_t *saved);

/** Get request message from saved nua event. */
SOFIAPUBFUN msg_t *nua_saved_event_request(nua_saved_event_t const *saved);

/** Get current request message. */
SOFIAPUBFUN  msg_t *nua_current_request(nua_t const *nua);

SOFIAPUBFUN sip_replaces_t *nua_handle_make_replaces(nua_handle_t *nh,
						     su_home_t *home,
						     int early_only);

SOFIAPUBFUN nua_handle_t *nua_handle_by_replaces(nua_t *nua,
						 sip_replaces_t const *rp);

nua_handle_t *nua_handle_by_call_id(nua_t *nua, const char *call_id);

SOFIA_END_DECLS

#endif
