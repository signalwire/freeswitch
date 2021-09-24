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
    sip_via_t        *nr_via;
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
			  sip_t const *sip, int rtag, int initial);
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

#include "nua_client.h"
#include "nua_server.h"

#endif /* NUA_DIALOG_H */
