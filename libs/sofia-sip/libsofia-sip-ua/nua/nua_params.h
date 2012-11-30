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

#ifndef NUA_PARAMS_H
/** Defined when <nua_params.h> has been included. */
#define NUA_PARAMS_H

/**@internal @file nua_params.h
 * @brief Parameters and their handling
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <Kai.Vehmanen@nokia.com>
 *
 * @date Created: Wed Mar  8 11:38:18 EET 2006  ppessi
 */

#include <nua_types.h>

#ifndef NUA_TAG_H
#include <sofia-sip/nua_tag.h>
#endif

/**@internal @brief NUA preferences.
 *
 * This structure contains values for various preferences and a separate
 * bitmap (nhp_set) for each preference. Preferences are set using
 * nua_set_params() or nua_set_hparams() or a handle-specific operation
 * setting the preferences, including nua_invite(), nua_respond(),
 * nua_ack(), nua_prack(), nua_update(), nua_info(), nua_bye(),
 * nua_options(), nua_message(), nua_register(), nua_publish(), nua_refer(),
 * nua_subscribe(), nua_notify(), nua_refer(), and nua_notifier().
 *
 * The stack uses preference value if corresponding bit in bitmap is set,
 * otherwise it uses preference value from default handle.
 *
 * @see NHP_GET(), NH_PGET(), NHP_ISSET(), NH_PISSET()
 */
struct nua_handle_preferences
{
  unsigned         nhp_retry_count;	/**< times to retry a request */
  unsigned         nhp_max_subscriptions;

  /* Session-related preferences */
  char const      *nhp_soa_name;
  unsigned         nhp_media_enable:1;
  unsigned     	   nhp_invite_enable:1;
  unsigned     	   nhp_auto_alert:1;
  unsigned         nhp_early_answer:1; /**< Include answer in 1XX */
  unsigned         nhp_early_media:1; /**< Establish early media with 100rel */
  unsigned         nhp_only183_100rel:1;/**< Only 100rel 183. */
  unsigned         nhp_auto_answer:1;
  unsigned         nhp_auto_ack:1; /**< Automatically ACK a final response */
  unsigned         :0;

  /** INVITE timeout.
   *
   * If no response is received in nhp_invite_timeout seconds,
   * INVITE client transaction times out
   */
  unsigned         nhp_invite_timeout;
  /** Default session timer (in seconds, 0 disables) */
  unsigned         nhp_session_timer;
  /** Default Min-SE Delta value */
  unsigned         nhp_min_se;
  /** no (preference), local or remote */
  enum nua_session_refresher nhp_refresher;
  unsigned         nhp_update_refresh:1; /**< Use UPDATE to refresh */

  /**< Accept refreshes without SDP */
  unsigned         nhp_refresh_without_sdp:1;

  /* Messaging preferences */
  unsigned     	   nhp_message_enable : 1;
  /** Be bug-compatible with Windows Messenger */
  unsigned     	   nhp_win_messenger_enable : 1;
  /** PIM-IW hack */
  unsigned         nhp_message_auto_respond : 1;

  /* Preferences for registration (and dialog establishment) */
  unsigned         nhp_callee_caps:1; /**< Add callee caps to contact */
  unsigned         nhp_media_features:1;/**< Add media features to caps*/
  /** Enable Service-Route */
  unsigned         nhp_service_route_enable:1;
  /** Enable Path */
  unsigned         nhp_path_enable:1;
  /** Authentication cache policy */
  unsigned         nhp_auth_cache:1;

  /** Always include id with Event: refer */
  unsigned         nhp_refer_with_id:1;

  unsigned         nhp_timer_autorequire:1;

  /** Enable Retry-After */
  unsigned         nhp_retry_after_enable:1;

  unsigned:0;

  /* Default lifetime for implicit subscriptions created by REFER */
  unsigned         nhp_refer_expires;

  /* Subscriber state, i.e. nua_substate_pending */
  unsigned         nhp_substate;
  unsigned         nhp_sub_expires;

  /* REGISTER keepalive intervals */
  unsigned         nhp_keepalive, nhp_keepalive_stream;
  char const      *nhp_registrar;

  sip_allow_t        *nhp_allow;
  sip_supported_t    *nhp_supported;
  sip_allow_events_t *nhp_allow_events;
  char const         *nhp_user_agent;
  char const         *nhp_organization;
  char const         *nhp_via;

  char const         *nhp_m_display;
  char const         *nhp_m_username;
  char const         *nhp_m_params;
  char const         *nhp_m_features;
  char const         *nhp_instance;

  /** Outbound OPTIONS */
  char const         *nhp_outbound;

  sip_allow_t        *nhp_appl_method;

  /** Initial route set */
  sip_route_t        *nhp_initial_route;

  /** Next hop URI (used instead of route). */
  url_string_t       *nhp_proxy;

  union { struct {
    /* A bit for each feature set by application */
    /* NOTE:
       Some compilers behave weird if there are bitfields
       together with width > 32
       So there should be a padding field (unsigned:0;)
       every 32 bits.
    */
    unsigned nhb_retry_count:1;
    unsigned nhb_max_subscriptions:1;

    unsigned nhb_soa_name:1;
    unsigned nhb_media_enable:1;
    unsigned nhb_invite_enable:1;
    unsigned nhb_auto_alert:1;
    unsigned nhb_early_answer:1;
    unsigned nhb_early_media:1;
    unsigned nhb_only183_100rel:1;
    unsigned nhb_auto_answer:1;
    unsigned nhb_auto_ack:1;
    unsigned nhb_invite_timeout:1;

    unsigned nhb_session_timer:1;
    unsigned nhb_min_se:1;
    unsigned nhb_refresher:1;
    unsigned nhb_update_refresh:1;
    unsigned nhb_refresh_without_sdp:1;
    unsigned nhb_message_enable:1;
    unsigned nhb_win_messenger_enable:1;
    unsigned nhb_message_auto_respond:1;
    unsigned nhb_callee_caps:1;
    unsigned nhb_media_features:1;
    unsigned nhb_service_route_enable:1;
    unsigned nhb_path_enable:1;
    unsigned nhb_auth_cache:1;
    unsigned nhb_refer_with_id:1;
    unsigned nhb_refer_expires:1;
    unsigned nhb_substate:1;
    unsigned nhb_sub_expires:1;
    unsigned nhb_keepalive:1;
    unsigned nhb_keepalive_stream:1;
    unsigned nhb_registrar:1;
    unsigned :0;		/* at most 32 bits before this point */

    unsigned nhb_allow:1;
    unsigned nhb_supported:1;

    unsigned nhb_allow_events:1;
    unsigned nhb_user_agent:1;
    unsigned nhb_organization:1;
    unsigned nhb_via:1;

    unsigned nhb_m_display:1;
    unsigned nhb_m_username:1;
    unsigned nhb_m_params:1;
    unsigned nhb_m_features:1;
    unsigned nhb_instance:1;
    unsigned nhb_outbound:1;
    unsigned nhb_appl_method:1;
    unsigned nhb_initial_route:1;
    unsigned nhb_proxy:1;
    unsigned nhb_timer_autorequire:1;
    unsigned nhb_retry_after_enable:1;
    unsigned :0;
  } set_bits;
    unsigned set_unsigned[2];
  } nhp_set_;
};

#define nhp_set nhp_set_.set_bits

/** Global preferences for nua. */
struct nua_global_preferences {
  /** Network detection: NONE, INFORMAL, TRY_FULL */
  signed int ngp_detect_network_updates:3;
  /** Pass events during shutdown, too */
  int ngp_shutdown_events:1;

  unsigned :0;			/* pad */
  union { struct {
    /* A bit for each feature set by application */
    unsigned ngp_detect_network_updates:1;
    unsigned ngp_shutdown_events:1;
    unsigned :0;
  } set_bits;
    unsigned set_unsigned[2];
  } ngp_set_;
};

#define ngp_set ngp_set_.set_bits

#define DNHP_GET(dnhp, pref) ((dnhp)->nhp_##pref)

#define NHP_GET(nhp, dnhp, pref)					\
  ((nhp)->nhp_set.nhb_##pref					\
   ? (nhp)->nhp_##pref : (dnhp)->nhp_##pref)

#define NHP_SET(nhp, pref, value)					\
  ((nhp)->nhp_##pref = (value),						\
   (nhp)->nhp_set.nhb_##pref = 1)

/* Check if preference is set */
#define NHP_ISSET(nhp, pref)						\
  ((nhp)->nhp_set.nhb_##pref)

#define NHP_UNSET_ALL(nhp) (memset(&(nhp)->nhp_set, 0, sizeof (nhp)->nhp_set))
#define NHP_SET_ALL(nhp) (memset(&(nhp)->nhp_set, 255, sizeof (nhp)->nhp_set))

/* Get preference from handle, if set, otherwise from default handle */
#define NH_PGET(nh, pref)						\
  NHP_GET((nh)->nh_prefs, (nh)->nh_dprefs, pref)

/* Get preference from handle, if exists and set,
   otherwise from default handle */
#define NUA_PGET(nua, nh, pref)						\
  NHP_GET((nh) ? (nh)->nh_prefs : (nua)->nua_dhandle->nh_prefs,		\
	  (nua)->nua_dhandle->nh_prefs,					\
	  pref)

/* Get preference from default handle */
#define DNH_PGET(dnh, pref)						\
  DNHP_GET((dnh)->nh_prefs, pref)
/* Check if preference is set in the handle */
#define NH_PISSET(nh, pref)						\
  (NHP_ISSET((nh)->nh_prefs, pref) &&					\
   (nh)->nh_nua->nua_dhandle->nh_prefs != (nh)->nh_prefs)

/* Check if preference has been set by application */
#define NUA_PISSET(nua, nh, pref)					\
  (NHP_ISSET((nua)->nua_dhandle->nh_prefs, pref) ||			\
   ((nh) && NHP_ISSET((nh)->nh_prefs, pref)))

#endif /* NUA_PARAMS_H */
