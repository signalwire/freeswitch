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

/**@CFILE nua_dialog.c
 * @brief Dialog and dialog usage handling
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Mar  8 11:48:49 EET 2006 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <assert.h>

#include <sofia-sip/su_string.h>
#include <sofia-sip/su_uniqueid.h>

#include <sofia-sip/sip_protos.h>
#include <sofia-sip/sip_status.h>

#define NUA_OWNER_T su_home_t

#include "nua_dialog.h"

#define SU_LOG (nua_log)
#include <sofia-sip/su_debug.h>

#ifndef NONE

#ifndef _MSC_VER
#define NONE ((void *)-1)
#else
#define NONE ((void *)(INT_PTR)-1)
#endif
#endif

/* ======================================================================== */
/* Dialog handling */

static void nua_dialog_usage_remove_at(nua_owner_t*, nua_dialog_state_t*,
				       nua_dialog_usage_t**,
				       nua_client_request_t *cr,
				       nua_server_request_t *sr);
static void nua_dialog_log_usage(nua_owner_t *, nua_dialog_state_t *);

/**@internal
 * UAS tag and route.
 *
 * Update dialog tags and route on the UAS side.
 *
 * @param own  dialog owner
 * @param ds   dialog state
 * @param sip  SIP message containing response used to update dialog
 * @param rtag if true, set remote tag within the leg
 */
void nua_dialog_uas_route(nua_owner_t *own,
			  nua_dialog_state_t *ds,
			  sip_t const *sip,
			  int rtag)
{
  int established = nua_dialog_is_established(ds);

  if (!established && sip->sip_from->a_tag)
    ds->ds_remote_tag = su_strdup(own, sip->sip_from->a_tag);

  if (ds->ds_leg == NULL)
    return;

  nta_leg_server_route(ds->ds_leg, sip->sip_record_route, sip->sip_contact);
  ds->ds_route = ds->ds_route || sip->sip_record_route || sip->sip_contact;

  if (rtag && !established && sip->sip_from->a_tag)
    nta_leg_rtag(ds->ds_leg, sip->sip_from->a_tag);
}

/**@internal
 * UAC tag and route.
 *
 * Update dialog tags and route on the UAC side.
 *
 * @param own  dialog owner
 * @param ds   dialog state
 * @param sip  SIP message containing response used to update dialog
 * @param rtag if true, set remote tag within the leg
 * @param initial if true, @a sip is response to initial transaction
 */
void nua_dialog_uac_route(nua_owner_t *own,
			  nua_dialog_state_t *ds,
			  sip_t const *sip,
			  int rtag,
			  int initial)
{
  int established = nua_dialog_is_established(ds);
  int status = sip->sip_status->st_status;

  if (!established && sip->sip_to->a_tag)
    ds->ds_remote_tag = su_strdup(own, sip->sip_to->a_tag);

  if (ds->ds_leg == NULL)
    return;

  if (initial && status >= 200)
    nta_leg_client_reroute(ds->ds_leg, sip->sip_record_route, sip->sip_contact, 1);
  else
    nta_leg_client_reroute(ds->ds_leg, sip->sip_record_route, sip->sip_contact, 0);

  ds->ds_route = ds->ds_route || sip->sip_record_route || sip->sip_contact;

  if (rtag && !established && sip->sip_to->a_tag)
    nta_leg_rtag(ds->ds_leg, sip->sip_to->a_tag);
}

/**@internal Store information from remote endpoint. */
void nua_dialog_store_peer_info(nua_owner_t *own,
				nua_dialog_state_t *ds,
				sip_t const *sip)
{
  nua_dialog_peer_info_t *nr = ds->ds_remote_ua;
  nua_dialog_usage_t *du;
  nua_dialog_peer_info_t old[1];

  *old = *nr;

  if (sip && sip->sip_status &&
      sip->sip_status->st_status >= 300 &&
      sip->sip_status->st_status <= 399)
    sip = NULL;			/* Redirected */

  if (sip == NULL) {
    nr->nr_via = NULL, su_free(own, old->nr_via);
    nr->nr_allow = NULL, su_free(own, old->nr_allow);
    nr->nr_accept = NULL, su_free(own, old->nr_accept);
    nr->nr_require = NULL, su_free(own, old->nr_require);
    nr->nr_supported = NULL, su_free(own, old->nr_supported);
    nr->nr_user_agent = NULL, su_free(own, old->nr_user_agent);
    return;
  }

  if (sip->sip_allow) {
    nr->nr_allow = sip_allow_dup(own, sip->sip_allow);
    su_free(own, old->nr_allow);
  }

  if (sip->sip_accept) {
    nr->nr_accept = sip_accept_dup(own, sip->sip_accept);
    su_free(own, old->nr_accept);
  }

  if (sip->sip_require) {
    nr->nr_require = sip_require_dup(own, sip->sip_require);
    su_free(own, old->nr_require);
  }

  if (sip->sip_supported) {
    nr->nr_supported = sip_supported_dup(own, sip->sip_supported);
    su_free(own, old->nr_supported);
  }

  if (sip->sip_via) {
    nr->nr_via = sip_via_dup(own, sip->sip_via);
    su_free(own, old->nr_via);
  }

  if (sip->sip_user_agent) {
    nr->nr_user_agent = sip_user_agent_dup(own, sip->sip_user_agent);
    su_free(own, old->nr_user_agent);
  }
  else if (sip->sip_server) {
    nr->nr_user_agent = sip_user_agent_dup(own, sip->sip_server);
    su_free(own, old->nr_user_agent);
  }

  for (du = ds->ds_usage; du; du = du->du_next) {
    if (du->du_class->usage_peer_info)
      du->du_class->usage_peer_info(du, ds, sip);
  }
}

/** Remove dialog information. */
int nua_dialog_zap(nua_owner_t *own,
		   nua_dialog_state_t *ds)
{
  /* zap peer info */
  nua_dialog_store_peer_info(own, ds, NULL);
  /* Local Contact */
  msg_header_free(own, (msg_header_t *)ds->ds_ltarget), ds->ds_ltarget = NULL;
  /* Leg */
  nta_leg_destroy(ds->ds_leg), ds->ds_leg = NULL;
  /* Remote tag */
  su_free(own, (void *)ds->ds_remote_tag), ds->ds_remote_tag = NULL;
  /* Ready to set route/remote target */
  ds->ds_route = 0;

  return 0;
}

/** Remove dialog (if there is no other usages). */
int nua_dialog_remove(nua_owner_t *own,
		      nua_dialog_state_t *ds,
		      nua_dialog_usage_t *usage)
{
  if (ds->ds_usage == usage && (usage == NULL || usage->du_next == NULL)) {
    return nua_dialog_zap(own, ds);
  }
  return 0;
}

/** @internal Get dialog usage slot. */
nua_dialog_usage_t **
nua_dialog_usage_at(nua_dialog_state_t const *ds,
		    nua_usage_class const *kind,
		    sip_event_t const *event)
{
  static nua_dialog_usage_t *none = NULL;

  if (ds) {
    nua_dialog_usage_t *du, * const * prev;
    sip_event_t const *o;

    for (prev = &ds->ds_usage; (du = *prev); prev = &du->du_next) {
      if (du->du_class != kind)
	continue;

      if (event == NONE)
	return (nua_dialog_usage_t **)prev;

      o = du->du_event;

      if (!event && !o)
	return (nua_dialog_usage_t **)prev;

      if (event != o) {
	if (event == NULL || o == NULL)
	  continue;
	if (!su_strmatch(event->o_type, o->o_type))
	  continue;
	if (!su_casematch(event->o_id, o->o_id)) {
	  if (event->o_id || !su_strmatch(event->o_type, "refer"))
	    continue;
	}
      }

      return (nua_dialog_usage_t **)prev;
    }
  }

  return &none;
}

/** @internal Get a dialog usage */
nua_dialog_usage_t *nua_dialog_usage_get(nua_dialog_state_t const *ds,
					 nua_usage_class const *kind,
					 sip_event_t const *event)
{
  return *nua_dialog_usage_at(ds, kind, event);
}

/** @internal Get dialog usage name */
char const *nua_dialog_usage_name(nua_dialog_usage_t const *du)
{
  if (du == NULL)
    return "<NULL>";
  return du->du_class->usage_name(du);
}

/** @internal Add dialog usage */
nua_dialog_usage_t *nua_dialog_usage_add(nua_owner_t *own,
					 struct nua_dialog_state *ds,
					 nua_usage_class const *uclass,
					 sip_event_t const *event)
{
  if (ds) {
    sip_event_t *o;
    nua_dialog_usage_t *du, **prev_du;

    prev_du = nua_dialog_usage_at(ds, uclass, event);
    du = *prev_du;
    if (du) {		/* Already exists */
      SU_DEBUG_5(("nua(%p): adding already existing %s usage%s%s\n",
		  (void *)own, nua_dialog_usage_name(du),
		  event ? "  with event " : "", event ? event->o_type : ""));

      if (prev_du != &ds->ds_usage) {
	/* Move as a first usage in the list */
	*prev_du = du->du_next;
	du->du_next = ds->ds_usage;
	ds->ds_usage = du;
      }
      return du;
    }

    o = event ? sip_event_dup(own, event) : NULL;

    if (o != NULL || event == NULL)
      du = su_zalloc(own, sizeof *du + uclass->usage_size);

    if (du) {
      su_home_ref(own);
      du->du_dialog = ds;
      du->du_class = uclass;
      du->du_event = o;

      if (uclass->usage_add(own, ds, du) < 0) {
	su_free(own, o);
	su_free(own, du);
	return NULL;
      }

      SU_DEBUG_5(("nua(%p): adding %s usage%s%s\n",
		  (void *)own, nua_dialog_usage_name(du),
		  o ? " with event " : "", o ? o->o_type :""));

      du->du_next = ds->ds_usage, ds->ds_usage = du;

      return du;
    }

    su_free(own, o);
  }

  return NULL;
}

/** @internal Remove dialog usage. */
void nua_dialog_usage_remove(nua_owner_t *own,
			     nua_dialog_state_t *ds,
			     nua_dialog_usage_t *du,
			     nua_client_request_t *cr,
			     nua_server_request_t *sr)
{
  nua_dialog_usage_t **at;

  assert(own); assert(ds); assert(du);

  for (at = &ds->ds_usage; *at; at = &(*at)->du_next)
    if (du == *at)
      break;

  assert(*at);

  nua_dialog_usage_remove_at(own, ds, at, cr, sr);
}

/** @internal Remove dialog usage.
 *
 * Zap dialog state (leg, tag and route) if no usages remain.
*/
static void
nua_dialog_usage_remove_at(nua_owner_t *own,
			   nua_dialog_state_t *ds,
			   nua_dialog_usage_t **at,
			   nua_client_request_t *cr0,
			   nua_server_request_t *sr0)
{
	int unref = 0;
	nua_dialog_usage_t *du = NULL;

  if (*at) {
    sip_event_t const *o = NULL;
    nua_client_request_t *cr, *cr_next;
    nua_server_request_t *sr, *sr_next;
    du = *at;

    *at = du->du_next;

    o = du->du_event;

    SU_DEBUG_5(("nua(%p): removing %s usage%s%s\n",
		(void *)own, nua_dialog_usage_name(du),
		o ? " with event " : "", o ? o->o_type :""));
    du->du_class->usage_remove(own, ds, du, cr0, sr0);

    /* Clean reference to saved client request */
    if (du->du_cr)
      nua_client_bind(du->du_cr, NULL);

    /* Clean references from queued client requests */
    for (cr = ds->ds_cr; cr; cr = cr_next) {
      cr_next = cr->cr_next;
      if (cr->cr_usage == du)
	cr->cr_usage = NULL;
    }

    /* Clean references from queued server requests */
    for (sr = ds->ds_sr; sr; sr = sr_next) {
      sr_next = sr->sr_next;
      if (sr->sr_usage == du) {
	sr->sr_usage = NULL;
	if (sr != sr0)
	  nua_server_request_destroy(sr);
      }
    }

	unref = 1;
  }

  /* Zap dialog if there are no more usages */
  if (ds->ds_terminating)
    ;
  else if (ds->ds_usage == NULL) {
    nua_dialog_remove(own, ds, NULL);
    ds->ds_has_events = 0;
	if (unref) {
		su_home_unref(own);
		su_free(own, du);
	}
    return;
  }
  else {
    nua_dialog_log_usage(own, ds);
  }

  if (unref) {
    su_home_unref(own);
    su_free(own, du);
  }
}

static
void nua_dialog_log_usage(nua_owner_t *own, nua_dialog_state_t *ds)
{
  nua_dialog_usage_t *du;

  if (SU_LOG->log_level >= 3) {
    char buffer[160];
    size_t l = 0, N = sizeof buffer;
    ssize_t n;

    buffer[0] = '\0';

    for (du = ds->ds_usage; du; du = du->du_next) {
      msg_header_t const *h = (void *)du->du_event;

      if (!h)
	continue;

      n = sip_event_e(buffer + l, N - l, h, 0);
      if (n == -1)
	break;
      l += (size_t)n;
      if (du->du_next && l + 2 < sizeof(buffer)) {
	strcpy(buffer + l, ", ");
	l += 2;
      }
    }

    SU_DEBUG_3(("nua(%p): handle with %s%s%s\n", (void *)own,
		ds->ds_has_session ? "session and " : "",
		ds->ds_has_events ? "events " : "",
		buffer));
  }
}

/** Deinitialize dialog and its usage. @internal */
void nua_dialog_deinit(nua_owner_t *own,
		       nua_dialog_state_t *ds)
{
  ds->ds_terminating = 1;

  while (ds->ds_usage) {
    nua_dialog_usage_remove_at(own, ds, &ds->ds_usage, NULL, NULL);
  }

  nua_dialog_remove(own, ds, NULL);

  ds->ds_has_events = 0;
  ds->ds_terminating = 0;
}

void nua_dialog_update_params(nua_dialog_state_t *ds,
			      nua_handle_preferences_t const *changed,
			      nua_handle_preferences_t const *params,
			      nua_handle_preferences_t const *defaults)
{
  nua_dialog_usage_t *usage;

  for (usage = ds->ds_usage; usage; usage = usage->du_next) {
    usage->du_class->usage_update_params(usage, changed, params, defaults);
  }
}

void nua_base_usage_update_params(nua_dialog_usage_t const *du,
				  nua_handle_preferences_t const *changed,
				  nua_handle_preferences_t const *params,
				  nua_handle_preferences_t const *defaults)
{
  (void)du, (void)changed, (void)params, (void)defaults;
}

/**@internal
 * Set refresh value suitably.
 *
 * The refresh time is set either around half of the @a delta interval or,
 * if @a delta is less than 5 minutes but longer than 90 seconds, 30..60
 * seconds before end of interval.
 *
 * If @a delta is 0, the dialog usage is never refreshed.
 */
void nua_dialog_usage_set_refresh(nua_dialog_usage_t *du, unsigned delta)
{
  if (delta == 0)
    nua_dialog_usage_reset_refresh(du);
  else if (delta > 90 && delta < 5 * 60)
    /* refresh 30..60 seconds before deadline */
    nua_dialog_usage_set_refresh_range(du, delta - 60, delta - 30);
  else {
    /* By default, refresh around half time before deadline */
    unsigned min = (delta + 2) / 4;
    unsigned max = (delta + 2) / 4 + (delta + 1) / 2;
    if (min == 0)
      min = 1;
    nua_dialog_usage_set_refresh_range(du, min, max);
  }
}

/**@internal Set refresh in range min..max seconds in the future. */
void nua_dialog_usage_set_refresh_range(nua_dialog_usage_t *du,
					unsigned min, unsigned max)
{
  sip_time_t now = sip_now(), target;
  unsigned delta;

  if (max < min)
    max = min;

  if (min != max)
    delta = su_randint(min, max);
  else
    delta = min;

  if (now + delta >= now)
    target = now + delta;
  else
    target = SIP_TIME_MAX;

  SU_DEBUG_7(("nua(): refresh %s after %lu seconds (in [%u..%u])\n",
	      nua_dialog_usage_name(du), target - now, min, max));

  du->du_refquested = now;

  du->du_refresh = target;
}

/** Set absolute refresh time */
void nua_dialog_usage_set_refresh_at(nua_dialog_usage_t *du,
				     sip_time_t target)
{
  SU_DEBUG_7(("nua(): refresh %s after %lu seconds\n",
	      nua_dialog_usage_name(du), target - sip_now()));
  du->du_refresh = target;
}

/**@internal Do not refresh. */
void nua_dialog_usage_reset_refresh(nua_dialog_usage_t *du)
{
  if (du) {
    du->du_refquested = sip_now();
    du->du_refresh = 0;
  }
}

/** @internal Refresh usage. */
void nua_dialog_usage_refresh(nua_owner_t *owner,
			      nua_dialog_state_t *ds,
			      nua_dialog_usage_t *du,
			      sip_time_t now)
{
  assert(du && du->du_class->usage_refresh);
  du->du_class->usage_refresh(owner, ds, du, now);
}

/** Terminate all dialog usages gracefully. */
int nua_dialog_shutdown(nua_owner_t *owner, nua_dialog_state_t *ds)
{
  nua_dialog_usage_t *du;

  ds->ds_terminating = 1;

  do {
    for (du = ds->ds_usage; du; du = du->du_next) {
      if (!du->du_shutdown) {
	nua_dialog_usage_shutdown(owner, ds, du);
	break;
      }
    }
  } while (du);

  return 1;
}

/** Shutdown (gracefully terminate) usage.
 *
 * @retval >0  shutdown done
 * @retval 0   shutdown in progress
 * @retval <0  try again later
 */
int nua_dialog_usage_shutdown(nua_owner_t *owner,
			      nua_dialog_state_t *ds,
			      nua_dialog_usage_t *du)
{
  if (du) {
    nua_dialog_usage_reset_refresh(du);
    du->du_shutdown = 1;
    assert(du->du_class->usage_shutdown);
    return du->du_class->usage_shutdown(owner, ds, du);
  }
  else
    return 200;
}

/** Repeat shutdown of all usages.
 *
 * @note Caller must have a reference to nh
 */
int nua_dialog_repeat_shutdown(nua_owner_t *owner, nua_dialog_state_t *ds)
{
  nua_dialog_usage_t *du;
  nua_server_request_t *sr, *sr_next;

  for (sr = ds->ds_sr; sr; sr = sr_next) {
    sr_next = sr->sr_next;

    if (nua_server_request_is_pending(sr)) {
      SR_STATUS1(sr, SIP_410_GONE); /* 410 terminates dialog */
      nua_server_respond(sr, NULL);
      nua_server_report(sr);
    }
  }

  for (du = ds->ds_usage; du ;) {
    nua_dialog_usage_t *du_next = du->du_next;

    nua_dialog_usage_shutdown(owner, ds, du);

    if (du_next == NULL)
      break;

    for (du = ds->ds_usage; du; du = du->du_next) {
      if (du == du_next)
	break;
      else if (!du->du_shutdown)
	break;
    }
  }

  return ds->ds_usage != NULL;
}
