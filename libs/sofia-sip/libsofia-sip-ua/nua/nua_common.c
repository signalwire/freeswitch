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

/**@CFILE nua_common.c
 * @brief Function common to both stack and application side.
 *
 * @author Pekka.Pessi@nokia.com
 *
 * @date Created: Tue Apr 26 13:23:17 2005 ppessi
 *
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_uniqueid.h>

#include <stdio.h>

#include <sofia-sip/su_tag_io.h>

#define SU_LOG (nua_log)
#include <sofia-sip/su_debug.h>

#define SU_ROOT_MAGIC_T   struct nua_s

#include <sofia-sip/su_wait.h>

#include <sofia-sip/su_string.h>
#include <sofia-sip/su_strlst.h>

#include "sofia-sip/nua.h"
#include "sofia-sip/nua_tag.h"

#include <sofia-sip/sip_protos.h>
#include <sofia-sip/nta.h>
#include <sofia-sip/nea.h>

#include <sofia-sip/auth_client.h>
#if HAVE_SMIME 		/* Start NRC Boston */
#include "smimec.h"
#endif                  /* End NRC Boston */

#include <sofia-sip/sdp.h>

#include "nua_stack.h"

static void nh_destructor(void *arg);

/**@internal
 * Create an operation handle
 *
 * Allocates a new operation handle and associated storage.
 *
 * @param nua         Pointer to NUA stack object
 * @param hmagic      Pointer to callback context
 * @param tags        List of tagged parameters
 *
 * @retval non-NULL  Pointer to operation handle
 * @retval NULL    Creation failed
 *
 * @par Related tags:
 *     Creates a copy of the provided tags which will
 *     be used with every operation.
 *
 * @par Events:
 *     none
 *
 * @note
 * This function is called by both stack and application sides.
 */
nua_handle_t *nh_create_handle(nua_t *nua,
			       nua_hmagic_t *hmagic,
			       tagi_t *tags)
{
  nua_handle_t *nh;
  static int8_t _handle_lifetime = 1;

  enter;

  assert(nua->nua_home);

  //if ((nh = su_home_clone(nua->nua_home, sizeof(*nh)))) {
  if ((nh = su_home_new(sizeof(*nh)))) {
    nh->nh_valid = nua_valid_handle_cookie;
    nh->nh_nua = nua;
    nh->nh_magic = hmagic;
    nh->nh_prefs = nua->nua_dhandle->nh_prefs;
    nh->nh_ds->ds_owner = nh;

    if (nua_handle_save_tags(nh, tags) < 0) {
      SU_DEBUG_5(("nua(%p): creating handle %p failed\n",
		  (void *)nua, (void *)nh));
      su_home_unref(nh->nh_home), nh = NULL;
    }

    if (nh && su_home_is_threadsafe(nua->nua_home)) {
      if (su_home_threadsafe(nh->nh_home) < 0) {
	su_home_unref(nh->nh_home);
	nh = NULL;
      }
    }

    if (nh && _handle_lifetime) {
      /* This far, we have nothing real to destruct but
       * when _NUA_HANDLE_DEBUG is set, we add destructor
       * and get more entertaining debugging output */
      if (_handle_lifetime == 1 && !getenv("_NUA_HANDLE_DEBUG")) {
	_handle_lifetime = 0;
      }
      else {
	_handle_lifetime = 2;
	SU_DEBUG_0(("nh_handle_create(%p)\n", (void *)nh));
	su_home_destructor(nh->nh_home, nh_destructor);
      }
    }
  }

  return nh;
}

/**@var _NUA_HANDLE_DEBUG
 *
 * If this environment variable is set, nua stack logs a message whenever a
 * handle is created and when it is destroyed. This is mainly useful when
 * debugging #nua_handle_t leaks.
 *
 * @sa nua_handle(), nua_handle_destroy()
 */
extern char const _NUA_HANDLE_DEBUG[];

/* nua handle destructor. It does nothing. */
static void nh_destructor(void *arg)
{
  nua_handle_t *nh = arg;
  SU_DEBUG_0(("nh_destructor(%p)\n", (void *)nh));
}

#if HAVE_MEMLEAK_LOG

nua_handle_t *
_nua_handle_ref_by(nua_handle_t *nh,
		   char const *file, unsigned line,
		   char const *function)
{

#if (HAVE_MEMLEAK_LOG == 1) 
	if (nh)
		SU_DEBUG_0(("%p - nua_handle_ref() => "MOD_ZU" by %s:%u: %s()\n",
					nh, su_home_refcount((su_home_t *)nh) + 1, file, line, function));
	return (nua_handle_t *)su_home_ref((su_home_t *)nh);
#else

	return (nua_handle_t *)_su_home_ref_by((su_home_t *)nh, file, line, function);
#endif
}

int
_nua_handle_unref_by(nua_handle_t *nh,
		    char const *file, unsigned line,
		    char const *function)
{

#if (HAVE_MEMLEAK_LOG == 1)

	if (nh) {
		size_t refcount = su_home_refcount((su_home_t *)nh) - 1;
		int freed =  su_home_unref((su_home_t *)nh);

		if (freed) refcount = 0;
		SU_DEBUG_0(("%p - nua_handle_unref() => "MOD_ZU" by %s:%u: %s()\n",
					nh, refcount, file, line, function));
		return freed;
	}

	return 0;
#else
	return _su_home_unref_by((su_home_t *)nh, file, line, function);
#endif
}

#else

/** Make a new reference to handle.
 *
 * The handles use reference counting for memory management. In addition to
 * the memory management, there is protocol state associated with the
 * handles. The protocol state is terminated with nua_handle_destroy(). In
 * order to make it more convenient for programmer, nua_handle_destroy()
 * decreases the reference count, too.
 *
 * @note All handle references are destroyed when the nua object is destroyed.
 *
 * @sa nua_handle_unref(), nua_handle(), nua_handle_destroy().
 */
nua_handle_t *nua_handle_ref(nua_handle_t *nh)
{
  return (nua_handle_t *)su_home_ref(nh->nh_home);
}

/** Destroy reference to handle.
 *
 * The handles use reference counting for memory management. In addition to
 * the memory management, there is protocol state associated with the
 * handles. The protocol state is terminated with nua_handle_destroy(). In
 * order to make it more convenient for programmer, nua_handle_destroy()
 * decreases the reference count, too.
 *
 * @sa nua_handle_ref(), nua_handle(), nua_handle_destroy().
 */
int nua_handle_unref(nua_handle_t *nh)
{
  return su_home_unref(nh->nh_home);
}

#endif

/** Generate an instance identifier. */
char const *nua_generate_instance_identifier(su_home_t *home)
{
  char str[su_guid_strlen + 1];
  su_guid_t guid[1];

  su_guid_generate(guid);
  /*
   * Guid looks like "NNNNNNNN-NNNN-NNNN-NNNN-XXXXXXXXXXXX"
   * where NNNNNNNN-NNNN-NNNN-NNNN is timestamp and XX is MAC address
   * (but we use usually random ID for MAC because we do not have
   *  guid generator available for all processes within node)
   */
  su_guid_sprintf(str, su_guid_strlen + 1, guid);

  return su_strdup(home, str);
}

/** Check if event is a request that can be responded with nua_respond().
 *
 * Note that if event status is 200 or greater, it already has been
 * responded. This function is provided for compatibility with future
 * versions of nua. An unknown event can always be handled in the event
 * callback like this:
 * @code
 * switch (event) {
 *   ...
 *   default:
 *     if (status < 200 && nua_event_is_incoming_request(event))
 *       nua_respond(nh, SIP_501_NOT_IMPLEMENTED,
 *                   NUTAG_WITH_THIS(nua), TAG_END());
 *     if (hmagic == NULL)
 *       nua_handle_destroy(nh);
 *     return;
 *   ...
 * @endcode
 *
 * @sa nua_respond(), #nua_event_e, #nua_event_t, nua_event_name()
 *
 * @NEW_1_12_6.
 */
int nua_event_is_incoming_request(nua_event_t event)
{
  switch (event) {
  case nua_i_invite: return 1;
  case nua_i_cancel: return 1;
  case nua_i_register: return 1;
  case nua_i_bye: return 1;
  case nua_i_options: return 1;
  case nua_i_refer: return 1;
  case nua_i_publish: return 1;
  case nua_i_prack: return 1;
  case nua_i_info: return 1;
  case nua_i_update: return 1;
  case nua_i_message: return 1;
  case nua_i_subscribe: return 1;
  case nua_i_notify: return 1;
  case nua_i_method: return 1;
  default: return 0;
  }
}

/** Get name for a NUA event.
 *
 * @sa #nua_event_e, #nua_event_t, nua_callstate_name(), nua_substate_name()
 */
char const *nua_event_name(nua_event_t event)
{
  switch (event) {
  case nua_i_none: return "nua_i_none";

  case nua_i_error: return "nua_i_error";
  case nua_i_invite: return "nua_i_invite";
  case nua_i_cancel: return "nua_i_cancel";
  case nua_i_ack: return "nua_i_ack";

  case nua_i_register: return "nua_i_register";
  case nua_i_fork: return "nua_i_fork";
  case nua_i_active: return "nua_i_active";
  case nua_i_terminated: return "nua_i_terminated";
  case nua_i_state: return "nua_i_state";
  case nua_i_outbound: return "nua_i_outbound";

  case nua_i_bye: return "nua_i_bye";
  case nua_i_options: return "nua_i_options";
  case nua_i_refer: return "nua_i_refer";
  case nua_i_publish: return "nua_i_publish";
  case nua_i_prack: return "nua_i_prack";
  case nua_i_info: return "nua_i_info";
  case nua_i_update: return "nua_i_update";
  case nua_i_message: return "nua_i_message";
  case nua_i_chat: return "nua_i_chat";
  case nua_i_subscribe: return "nua_i_subscribe";
  case nua_i_subscription: return "nua_i_subscription";
  case nua_i_notify: return "nua_i_notify";
  case nua_i_method: return "nua_i_method";

  case nua_i_media_error: return "nua_i_media_error";

  /* Responses */
  case nua_r_get_params: return "nua_r_get_params";
  case nua_r_shutdown: return "nua_r_shutdown";
  case nua_r_notifier: return "nua_r_notifier";
  case nua_r_terminate: return "nua_r_terminate";

  case nua_r_register: return "nua_r_register";
  case nua_r_unregister: return "nua_r_unregister";
  case nua_r_invite: return "nua_r_invite";
  case nua_r_bye: return "nua_r_bye";
  case nua_r_options: return "nua_r_options";
  case nua_r_refer: return "nua_r_refer";
  case nua_r_publish: return "nua_r_publish";
  case nua_r_unpublish: return "nua_r_unpublish";
  case nua_r_info: return "nua_r_info";
  case nua_r_prack: return "nua_r_prack";
  case nua_r_update: return "nua_r_update";
  case nua_r_message: return "nua_r_message";
  case nua_r_chat: return "nua_r_chat";
  case nua_r_subscribe: return "nua_r_subscribe";
  case nua_r_unsubscribe: return "nua_r_unsubscribe";
  case nua_r_notify: return "nua_r_notify";
  case nua_r_method: return "nua_r_method";

  case nua_r_cancel: return "nua_r_cancel";
  case nua_r_authenticate: return "nua_r_authenticate";
  case nua_r_authorize: return "nua_r_authorize";
  case nua_r_redirect: return "nua_r_redirect";
  case nua_r_destroy: return "nua_r_destroy";
  case nua_r_respond: return "nua_r_respond";
  case nua_r_nit_respond: return "nua_r_nit_respond";
  case nua_r_set_params: return "nua_r_set_params";
  case nua_r_ack: return "nua_r_ack";
  default: return "NUA_UNKNOWN";
  }
}

/** Return name of call state.
 *
 * @sa enum #nua_callstate, nua_event_name(), nua_substate_name()
 */
char const *nua_callstate_name(enum nua_callstate state)
{
  switch (state) {
  case nua_callstate_init: return "init";
  case nua_callstate_authenticating: return "authenticating";
  case nua_callstate_calling: return "calling";
  case nua_callstate_proceeding: return "proceeding";
  case nua_callstate_completing: return "completing";
  case nua_callstate_received: return "received";
  case nua_callstate_early: return "early";
  case nua_callstate_completed: return "completed";
  case nua_callstate_ready: return "ready";
  case nua_callstate_terminating: return "terminating";
  case nua_callstate_terminated: return "terminated";
  default: return "UNKNOWN";
  }
}

/** Return name of subscription state. @NEW_1_12_5.
 *
 * @sa enum #nua_substate, nua_event_name(), nua_callstate_name()
 */
char const *nua_substate_name(enum nua_substate substate)
{
  switch (substate) {
  case nua_substate_embryonic:
      /*FALLTHROUGH*/
  case nua_substate_pending:
    return "pending";
  case nua_substate_terminated:
    return "terminated";
  case nua_substate_active:
      /*FALLTHROUGH*/
  default:
    return "active";
  }
}

/** Convert string to enum nua_substate. @NEW_1_12_5. */
enum nua_substate nua_substate_make(char const *sip_substate)
{
  if (sip_substate == NULL)
    return nua_substate_active;
  else if (su_casematch(sip_substate, "terminated"))
    return nua_substate_terminated;
  else if (su_casematch(sip_substate, "pending"))
    return nua_substate_pending;
  else /* if (su_casematch(sip_substate, "active")) */
    return nua_substate_active;
}

