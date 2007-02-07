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

/**@file nua.c High-Level User Agent Library - "nua" Implementation.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <Kai.Vehmanen@nokia.com>
 * @author Pasi Rinne-Rahkola
 *
 * @date Created: Wed Feb 14 18:32:58 2001 ppessi
 */

#include "config.h"

#include <sofia-sip/su_tag.h>
#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_tagarg.h>

#include <sofia-sip/su_tag_io.h>

#define SU_LOG (nua_log)
#include <sofia-sip/su_debug.h>

#define SU_ROOT_MAGIC_T   struct nua_s
#define SU_MSG_ARG_T      struct event_s
#define NUA_SAVED_EVENT_T su_msg_t *

#include <sofia-sip/sip_status.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/nta.h>

#include "sofia-sip/nua.h"
#include "sofia-sip/nua_tag.h"
#include "nua_stack.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* From AM_INIT/AC_INIT in our "config.h" */
char const nua_version[] = VERSION;

/**Environment variable determining the debug log level for @nua module.
 *
 * The NUA_DEBUG environment variable is used to determine the debug logging
 * level for @nua module. The default level is 3.
 *
 * @sa <sofia-sip/su_debug.h>, nua_log, SOFIA_DEBUG
 */
extern char const NUA_DEBUG[];

#ifndef SU_DEBUG
#define SU_DEBUG 3
#endif

/**Debug log for @nua module.
 *
 * The nua_log is the log object used by @nua module. The level of
 * #nua_log is set using #NUA_DEBUG environment variable.
 */
su_log_t nua_log[] = { SU_LOG_INIT("nua", "NUA_DEBUG", SU_DEBUG) };

/**Create a @nua agent.
 *
 * This function creates a Sofia-SIP User Agent stack object (@nua) and
 * initializes its parameters by given tagged values.
 *
 * @param root            Pointer to a root object
 * @param callback        Pointer to event callback function
 * @param magic           Pointer to callback context
 * @param tag, value, ... List of tagged parameters
 *
 * @retval !=NULL a pointer to a @nua stack object \n
 * @retval NULL upon an error
 *
 * @par Related tags:
 *     NUTAG_PROXY()            \n
 *     NUTAG_URL()              \n
 *     NUTAG_SIPS_URL()         \n
 *     NUTAG_SIP_PARSER()       \n
 *     NUTAG_UICC()             \n
 *     NUTAG_CERTIFICATE_DIR()  \n
 *     and all tags listed in nua_set_params(), \n
 *     and all relevant NTATAG_* are passed to NTA \n
 *     and all tport tags listed in <sofia-sip/tport_tag.h>
 *
 * @note
 * From the @VERSION_1_12_2 all the nua_set_params() tags are processed.
 * Previously all nutags except NUTAG_SOA_NAME() and NUTAG_MEDIA_ENABLE()
 * were ignored.
 *
 * @note
 * Both the NUTAG_URL() and NUTAG_SIPS_URL() are used to pass arguments to
 * nta_agent_add_tport().
 *
 * @par Events:
 *     none
 *
 * @sa nua_shutdown(), nua_destroy(), nua_handle()
 */
nua_t *nua_create(su_root_t *root,
		  nua_callback_f callback,
		  nua_magic_t *magic,
		  tag_type_t tag, tag_value_t value, ...)
{
  nua_t *nua = NULL;

  enter;

  if (callback == NULL)
    return (void)(errno = EFAULT), NULL;

  if (root == NULL)
    return (void)(errno = EFAULT), NULL;

  if ((nua = su_home_new(sizeof(*nua)))) {
    ta_list ta;

    su_home_threadsafe(nua->nua_home);
    nua->nua_api_root = root;

    ta_start(ta, tag, value);

    nua->nua_args = tl_adup(nua->nua_home, ta_args(ta));

    su_task_copy(nua->nua_client, su_root_task(root));

    /* XXX: where to put this in the nua_server case? */
#if HAVE_SMIME		/* Start NRC Boston */
      nua->sm = sm_create();
#endif                  /* End NRC Boston */

#ifndef NUA_SERVER
    if (su_clone_start(root,
		       nua->nua_clone,
		       nua,
		       nua_stack_init,
		       nua_stack_deinit) == SU_SUCCESS) {
      su_task_copy(nua->nua_server, su_clone_task(nua->nua_clone));
      nua->nua_callback = callback;
      nua->nua_magic = magic;
    }
    else {
      su_home_unref(nua->nua_home);
      nua = NULL;
    }
#endif

    ta_end(ta);
  }

  return nua;
}

/* nua_shutdown() is documented with nua_stack_shutdown() */

void nua_shutdown(nua_t *nua)
{
  enter;

  if (nua)
    nua->nua_shutdown_started = 1;
  nua_signal(nua, NULL, NULL, 1, nua_r_shutdown, 0, NULL, TAG_END());
}

/** Destroy the @nua stack.
 *
 * Before calling nua_destroy() the application
 * should call nua_shutdown and wait for successful #nua_r_shutdown event.
 * Shuts down and destroys the @nua stack. Ongoing calls, registrations,
 * and subscriptions are left as they are.
 *
 * @param nua         Pointer to @nua stack object
 *
 * @return
 *     nothing
 *
 * @par Related tags:
 *     none
 *
 * @par Events:
 *     none
 *
 * @sa nua_shutdown(), nua_create(), nua_handle_destroy(), nua_handle_unref()
 */
void nua_destroy(nua_t *nua)
{
  enter;

  if (nua) {
    if (!nua->nua_shutdown_final) {
      SU_DEBUG_0(("nua_destroy(%p): FATAL: nua_shutdown not completed\n",
		  (void *)nua));
      assert(nua->nua_shutdown);
      return;
    }

    su_task_deinit(nua->nua_server);
    su_task_deinit(nua->nua_client);

    su_clone_wait(nua->nua_api_root, nua->nua_clone);
#if HAVE_SMIME		/* Start NRC Boston */
    sm_destroy(nua->sm);
#endif			/* End NRC Boston */
    su_home_unref(nua->nua_home);
  }
}

/** Fetch callback context from nua.
 *
 * @param nua         Pointer to @nua stack object
 *
 * @return Callback context pointer.
 *
 * @NEW_1_12_4.
 */
nua_magic_t *nua_magic(nua_t *nua)
{
  return nua ? nua->nua_magic : NULL;
}

/** Obtain default operation handle of the @nua stack object.
 *
 * A default operation can be used for operations where the
 * ultimate result is not important or can be discarded.
 *
 * @param nua         Pointer to @nua stack object
 *
 * @retval !=NULL Pointer to @nua operation handle
 * @retval NULL   No default operation exists
 *
 * @par Related tags:
 *    none
 *
 * @par Events:
 *    none
 *
 */
nua_handle_t *nua_default(nua_t *nua)
{
  return nua ? nua->nua_handles : NULL;
}

/** Create an operation handle
 *
 * Allocates a new operation handle and associated storage.
 *
 * @param nua         Pointer to @nua stack object
 * @param hmagic      Pointer to callback context
 * @param tag, value, ... List of tagged parameters
 *
 * @retval !=NULL  Pointer to operation handle
 * @retval NULL    Creation failed
 *
 * @par Related tags:
 *     Duplicates the provided tags for use with every operation. Note that
 *     NUTAG_URL() is converted to SIPTAG_TO() if there is no SIPTAG_TO().
 *     And also vice versa, request-URI is taken from SIPTAG_TO() if there
 *     is no NUTAG_URL(). Note that certain SIP headers cannot be saved with
 *     the handle. They include @ContentLength, @CSeq, @RSeq, @RAck, and
 *     @Timestamp.
 *
 * @par
 *     nua_handle() accepts all the tags accepted by nua_set_hparams(), too.
 *
 *
 * @par Events:
 *     none
 *
 * @sa nua_handle_bind(), nua_handle_destroy(), nua_handle_ref(),
 * nua_handle_unref().
 */
nua_handle_t *nua_handle(nua_t *nua, nua_hmagic_t *hmagic,
			 tag_type_t tag, tag_value_t value, ...)
{
  nua_handle_t *nh = NULL;

  if (nua) {
    ta_list ta;

    ta_start(ta, tag, value);

    nh = nh_create_handle(nua, hmagic, ta_args(ta));

    if (nh)
      nh->nh_ref_by_user = 1;

    ta_end(ta);
  }

  return nh;
}

/** Bind a callback context to an operation handle.
 *
 * @param nh          Pointer to operation handle
 * @param hmagic      Pointer to callback context
 *
 * @return
 *     nothing
 *
 * @par Related tags:
 *     none
 *
 * @par Events:
 *     none
 */
void nua_handle_bind(nua_handle_t *nh, nua_hmagic_t *hmagic)
{
  enter;

  if (NH_IS_VALID(nh))
    nh->nh_magic = hmagic;
}

/** Fetch a callback context from an operation handle.
 *
 * @param nh          Pointer to operation handle
 *
 * @return
 *     Pointer to callback context
 *
 * @par Related tags:
 *     none
 *
 * @par Events:
 *     none
 *
 * @NEW_1_12_4.
 */
nua_hmagic_t *nua_handle_magic(nua_handle_t *nh)
{
  nua_hmagic_t *magic = NULL;
  enter;

  if (NH_IS_VALID(nh))
    magic = nh->nh_magic;

  return magic;
}

/* ---------------------------------------------------------------------- */

/** Check if operation handle is used for INVITE
 *
 * Check if operation handle has been used with either outgoing or incoming
 * INVITE request.
 *
 * @param nh          Pointer to operation handle
 *
 * @retval 0 no invite in operation or operation handle is invalid
 * @retval 1 operation has invite
 *
 * @par Related tags:
 *     none
 *
 * @par Events:
 *     none
 */
int nua_handle_has_invite(nua_handle_t const *nh)
{
  return nh ? nh->nh_has_invite : 0;
}

/**Check if operation handle has active event subscriptions.
 *
 * Active subscription can be established either by nua_subscribe() or
 * nua_refer() calls.
 *
 * @param nh          Pointer to operation handle
 *
 * @retval 0    no event subscriptions in operation or
 *              operation handle is invalid
 * @retval !=0  operation has event subscriptions
 *
 * @par Related tags:
 *     none
 *
 * @par Events:
 *     none
 */
int nua_handle_has_events(nua_handle_t const *nh)
{
  return nh ? nh->nh_ds->ds_has_events : 0;
}

/** Check if operation handle has active registrations
 *
 * A registration is active when either when a REGISTER operation is going
 * on or when it has successfully completed so that @nua stack is expected to
 * refresh the registration in the future. Normally, a handle has active
 * registration after nua_register() until nua_unregister() completes,
 * unless the initial nua_register() had either expiration time of 0 or it
 * had SIPTAG_CONTACT(NULL) as an argument.
 *
 * @param nh          Pointer to operation handle
 *
 * @retval 0 no active registration in operation or
 *           operation handle is invalid
 * @retval 1 operation has registration
 *
 * @par Related tags:
 *     none
 *
 * @par Events:
 *     none
 *
 * @sa nua_register(), nua_unregister(), #nua_r_register, #nua_r_unregister
 */
int nua_handle_has_registrations(nua_handle_t const *nh)
{
  return nh && nh->nh_ds->ds_has_register;
}

/** Check if operation handle has been used with outgoing SUBSCRIBE of REFER request.
 *
 * @param nh          Pointer to operation handle
 *
 * @retval 0 no active subscription in operation or
 *           operation handle is invalid
 * @retval 1 operation has subscription.
 *
 * @par Related tags:
 *     none
 *
 * @par Events:
 *     none
 */
int nua_handle_has_subscribe(nua_handle_t const *nh)
{
  return nh ? nh->nh_has_subscribe : 0;
}

/** Check if operation handle has been used with nua_register() or nua_unregister().
 *
 * @param nh          Pointer to operation handle
 *
 * @retval 0 no active register in operation or operation handle is invalid
 * @retval 1 operation has been used with nua_register() or nua-unregister()
 *
 * @par Related tags:
 *     none
 *
 * @par Events:
 *     none
 */
int nua_handle_has_register(nua_handle_t const *nh)
{
  return nh ? nh->nh_has_register : 0;
}

/** Check if operation handle has an active call
 *
 * @param nh          Pointer to operation handle
 *
 * @retval 0 no active call in operation or operation handle is invalid
 * @retval 1 operation has established call or pending call request.
 *
 * @par Related tags:
 *     none
 *
 * @par Events:
 *     none
 */
int nua_handle_has_active_call(nua_handle_t const *nh)
{
  return nh ? nh->nh_active_call : 0;
}

/** Check if operation handle has a call on hold
 *
 * Please note that this status is not affected by remote end putting
 * this end on hold. Remote end can put each media separately on hold
 * and status is reflected on SOATAG_ACTIVE_AUDIO(), SOATAG_ACTIVE_VIDEO()
 * and SOATAG_ACTIVE_CHAT() tag values in #nua_i_state event.
 *
 * @param nh          Pointer to operation handle
 *
 * @retval 0  if no call on hold in operation or operation handle is invalid
 * @retval 1  if operation has call on hold, for example nua_invite() or
 *            nua_update() has been called with SOATAG_HOLD() with non-NULL
 *            argument.
 *
 * @par Related tags:
 *     none
 *
 * @par Events:
 *     none
 */
int nua_handle_has_call_on_hold(nua_handle_t const *nh)
{
  return nh ? nh->nh_hold_remote : 0;
}

/** Get the remote address (From/To header) of operation handle
 *
 * Remote address is used as To header in outgoing operations and
 * derived from From: header in incoming operations.
 *
 * @param nh          Pointer to operation handle
 *
 * @retval NULL   no remote address for operation or operation handle invalid
 * @retval !=NULL pointer to remote address for operation
 *
 * @par Related tags:
 *     none
 *
 * @par Events:
 *     none
 */
sip_to_t const *nua_handle_remote(nua_handle_t const *nh)
{
  return nh ? nh->nh_ds->ds_remote : NULL;
}

/** Get the local address (From/To header) of operation handle
 *
 * Local address is used as From header in outgoing operations and
 * derived from To: header in incoming operations.
 *
 * @param nh          Pointer to operation handle
 *
 * @retval NULL   no local address for operation or operation handle invalid
 * @retval !=NULL pointer to local address for operation
 *
 * @par Related tags:
 *     none
 *
 * @par Events:
 *     none
 */
sip_to_t const *nua_handle_local(nua_handle_t const *nh)
{
  return nh ? nh->nh_ds->ds_local : NULL;
}

/* Documented with nua_stack_set_params() */
void nua_set_params(nua_t *nua, tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  ta_start(ta, tag, value);

  enter;

  nua_signal(nua, NULL, NULL, 0, nua_r_set_params, 0, NULL, ta_tags(ta));

  ta_end(ta);
}

/* Documented with nua_stack_get_params() */
void nua_get_params(nua_t *nua, tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  ta_start(ta, tag, value);

  enter;

  nua_signal(nua, NULL, NULL, 0, nua_r_get_params, 0, NULL, ta_tags(ta));

  ta_end(ta);
}

#define NUA_SIGNAL(nh, event, tag, value) \
  enter; \
  if (NH_IS_VALID((nh))) { \
    ta_list ta; \
    ta_start(ta, tag, value); \
    nua_signal((nh)->nh_nua, nh, NULL, 0, event, 0, NULL, ta_tags(ta));	\
    ta_end(ta); \
  } \
  else { \
    SU_DEBUG_1(("nua: " #event " with invalid handle %p\n", (void *)nh)); \
  }

/* Documented with nua_stack_set_params() */
void nua_set_hparams(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_set_params, tag, value);
}

/* Documented with nua_stack_get_params() */
void nua_get_hparams(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_get_params, tag, value);
}

/* Documented with nua_stack_register() */
void nua_register(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_register, tag, value);
}

void nua_unregister(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_unregister, tag, value);
}

/* Documented with nua_stack_invite() */
void nua_invite(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_invite, tag, value);
}

/* Documented with nua_stack_ack() */
void nua_ack(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_ack, tag, value);
}

/* Documented with nua_stack_bye() */
void nua_bye(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_bye, tag, value);
}

/* Documented with nua_stack_cancel() */
void nua_cancel(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_cancel, tag, value);
}

/* Documented with nua_stack_options() */
void nua_options(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_options, tag, value);
}

/* Documented with nua_stack_message() */
void nua_message(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_message, tag, value);
}

/* Documented with nua_stack_method() */
void nua_method(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_method, tag, value);
}

/** Send a chat message.
 *
 * A chat channel can be established during call setup using "message" media.
 * An active chat channel is indicated using #nua_i_state event containing
 * SOATAG_ACTIVE_CHAT() tag. Chat messages can be sent using this channel with
 * nua_chat() function. Currently this is implemented using SIP MESSAGE
 * requests but in future MSRP (message session protocol) will replace it.
*
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *    nothing
 *
 * @par Related Tags:
 *    SIPTAG_CONTENT_TYPE() \n
 *    SIPTAG_PAYLOAD()      \n
 *    SIPTAG_FROM()         \n
 *    SIPTAG_TO()           \n
 *    Use of other SIP tags is deprecated
 *
 * @par Events:
 *    #nua_r_chat
 */
void nua_chat(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_chat, tag, value);
}

/* Documented with nua_stack_subscribe() */
void nua_subscribe(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_subscribe, tag, value);
}

/* Documented with nua_stack_subscribe() */
void nua_unsubscribe(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_unsubscribe, tag, value);
}

/* Documented with nua_stack_notify() */
void nua_notify(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_notify, tag, value);
}

/* nua_r_notify is documented with process_response_to_notify() */

/** Create an event server.
 *
 * This function create an event server taking care of sending NOTIFY
 * requests and responding to further SUBSCRIBE requests. The event
 * server can accept multiple subscriptions from several sources and
 * takes care for distributing the notifications. Unlike other functions
 * this call only accepts the SIP tags listed below.
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *    nothing
 *
 * @par Related Tags:
 *    NUTAG_URL() \n
 *    SIPTAG_EVENT() or SIPTAG_EVENT_STR() \n
 *    SIPTAG_CONTENT_TYPE() or SIPTAG_CONTENT_TYPE_STR() \n
 *    SIPTAG_PAYLOAD() or SIPTAG_PAYLOAD_STR() \n
 *    SIPTAG_ACCEPT() or SIPTAG_ACCEPT_STR() \n
 *
 * @par Events:
 *    #nua_r_notify
 */
void nua_notifier(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_notifier, tag, value);
}

/** Terminate an event server.
 *
 * Terminate an event server with matching event and content type. The event
 * server was created earlier with nua_notifier() function.
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *    nothing
 *
 * @par Related Tags:
 *    SIPTAG_EVENT() \n
 *    SIPTAG_CONTENT_TYPE() \n
 *    SIPTAG_PAYLOAD() \n
 *    NEATAG_REASON()
 *
 * @par Events:
 *    #nua_r_terminate
 *
 * @sa nua_notifier(), nua_authorize().
 */
void nua_terminate(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_terminate, tag, value);
}

/* Documented with nua_stack_refer() */
void nua_refer(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_refer, tag, value);
}

/* Documented with nua_stack_publish() */
void nua_publish(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_publish, tag, value);
}

/* Documented with nua_stack_publish() */
void nua_unpublish(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_unpublish, tag, value);
}

/* Documented with nua_stack_info() */
void nua_info(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_info, tag, value);
}

/* Documented with nua_stack_prack() */
void nua_prack(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_prack, tag, value);
}

/* Documented with nua_stack_update() */
void nua_update(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_update, tag, value);
}

/** Authenticate an operation.
 *
 * - 401 / 407 response with www-authenticate header/ proxy-authenticate header
 * - application should provide stack with username&password for each realm
 *   with NUTAG_AUTH() tag
 * - restarts operation
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *    nothing
 *
 * @par Related Tags:
 *    NUTAG_AUTH()
 *
 * @par Events:
 *    (any operation events)
 */
void nua_authenticate(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_authenticate, tag, value);
}

/** Authorize a subscriber.
 *
 * After creating a local presence server by nua_notifier(), an incoming
 * SUBSCRIBE request causes #nua_i_subscription event. Each subscriber is
 * identified with NEATAG_SUB() tag in the #nua_i_subscription event.
 * Application can either authorize the subscriber with
 * NUTAG_SUBSTATE(#nua_substate_active) or terminate the subscription with
 * NUTAG_SUBSTATE(#nua_substate_terminated).
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *    nothing
 *
 * @par Related Tags:
 *    NEATAG_SUB() \n
 *    NUTAG_SUBSTATE()
 *
 * @par Events:
 *    #nua_i_subscription
 *
 * @sa nua_notifier(), nua_terminate()
 */
void nua_authorize(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_authorize, tag, value);
}

/*# Redirect an operation. */
void nua_redirect(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...)
{
  NUA_SIGNAL(nh, nua_r_redirect, tag, value);
}

/* Documented with nua_stack_respond() */

void nua_respond(nua_handle_t *nh,
		 int status, char const *phrase,
		 tag_type_t tag, tag_value_t value,
		 ...)
{
  enter;

  if (NH_IS_VALID(nh)) {
    ta_list ta;
    ta_start(ta, tag, value);
    nua_signal(nh->nh_nua, nh, NULL, 0, nua_r_respond,
	       status, phrase, ta_tags(ta));
    ta_end(ta);
  }
  else {
    SU_DEBUG_1(("nua: respond with invalid handle %p\n", (void *)nh));
  }
}

/** Destroy a handle
 *
 * Terminate the protocol state associated with an operation handle. The
 * stack discards resources and terminates the ongoing dialog usage,
 * sessions and transactions associated with this handle. For example, calls
 * are terminated with BYE request. Also, the reference count for the handle
 * is also decremented.
 *
 * The handles use reference counting for memory management. In order to
 * make it more convenient for programmer, nua_handle_destroy() decreases
 * the reference count, too.
 *
 * @param nh              Pointer to operation handle
 *
 * @return
 *    nothing
 *
 * @par Related Tags:
 *    none
 *
 * @par Events:
 *    none
 *
 * @sa nua_handle(), nua_handle_bind(), nua_handle_ref(), nua_handle_unref(),
 * nua_unregister(), nua_unpublish(), nua_unsubscribe(), nua_bye().
 */
void nua_handle_destroy(nua_handle_t *nh)
{
  enter;

  if (NH_IS_VALID(nh) && !NH_IS_DEFAULT(nh)) {
    nh->nh_valid = NULL;	/* Events are no more delivered to appl. */
    nua_signal(nh->nh_nua, nh, NULL, 1, nua_r_destroy, 0, NULL, TAG_END());
  }
}

/*# Send a request to the protocol thread */
void nua_signal(nua_t *nua, nua_handle_t *nh, msg_t *msg, int always,
		nua_event_t event,
		int status, char const *phrase,
		tag_type_t tag, tag_value_t value, ...)
{
  su_msg_r sumsg = SU_MSG_R_INIT;
  size_t len, xtra, e_len, l_len = 0, l_xtra = 0;
  ta_list ta;

  if (nua == NULL || (nua->nua_shutdown_started && event != nua_r_shutdown))
    return;

  ta_start(ta, tag, value);

  e_len = offsetof(event_t, e_tags);
  len = tl_len(ta_args(ta));
  xtra = tl_xtra(ta_args(ta), len);

  if (su_msg_create(sumsg, nua->nua_server, su_task_null,
		    nua_stack_signal,
		    e_len + len + l_len + xtra + l_xtra) == 0) {
    event_t *e = su_msg_data(sumsg);
    tagi_t *t = e->e_tags;
    void *b = (char *)t + len + l_len;

    tagi_t *tend = (tagi_t *)b;
    char *bend = (char *)b + xtra + l_xtra;

    t = tl_dup(t, ta_args(ta), &b);

    assert(tend == t); (void)tend; assert(b == bend); (void)bend;

    e->e_always = always;
    e->e_event = event;
    e->e_nh = event == nua_r_destroy ? nh : nua_handle_ref(nh);
    e->e_status = status;
    e->e_phrase = phrase;

    SU_DEBUG_7(("nua(%p): signal %s\n", (void *)nh,
		nua_event_name(event) + 4));

    if (su_msg_send(sumsg) != 0 && event != nua_r_destroy)
      nua_handle_unref(nh);
  }
  else {
    /* XXX  - we should return error code to application but we just abort() */
    assert(ENOMEM == 0);
  }

  ta_end(ta);
}

/*# Receive event from protocol machine and hand it over to application */
void nua_event(nua_t *root_magic, su_msg_r sumsg, event_t *e)
{
  nua_t *nua;
  nua_handle_t *nh = e->e_nh;

  enter;

  if (nh) {
    if (!nh->nh_ref_by_user && nh->nh_valid) {
      nh->nh_ref_by_user = 1;
      nua_handle_ref(nh);
    }
  }

  if (!nh || !nh->nh_valid) {	/* Handle has been destroyed */
    if (nua_log->log_level >= 7) {
      char const *name = nua_event_name(e->e_event) + 4;
      SU_DEBUG_7(("nua(%p): event %s dropped\n", (void *)nh, name));
    }
    if (nh && !NH_IS_DEFAULT(nh) && nua_handle_unref(nh)) {
#if HAVE_NUA_HANDLE_DEBUG
      SU_DEBUG_0(("nua(%p): freed by application\n", (void *)nh));
#else
      SU_DEBUG_9(("nua(%p): freed by application\n", (void *)nh));
#endif
    }
    if (e->e_msg)
      msg_destroy(e->e_msg), e->e_msg = NULL;
    return;
  }

  nua = nh->nh_nua; assert(nua);

  if (e->e_event == nua_r_shutdown && e->e_status >= 200)
    nua->nua_shutdown_final = 1;

  if (!nua->nua_callback)
    return;

  if (NH_IS_DEFAULT(nh))
    nh = NULL;

  su_msg_save(nua->nua_current, sumsg);

  e->e_nh = NULL;

  nua->nua_callback(e->e_event, e->e_status, e->e_phrase,
		    nua, nua->nua_magic,
		    nh, nh ? nh->nh_magic : NULL,
		    e->e_msg ? sip_object(e->e_msg) : NULL,
		    e->e_tags);

  if (nh && !NH_IS_DEFAULT(nh) && nua_handle_unref(nh)) {
#if HAVE_NUA_HANDLE_DEBUG
    SU_DEBUG_0(("nua(%p): freed by application\n", (void *)nh));
#else
    SU_DEBUG_9(("nua(%p): freed by application\n", (void *)nh));
#endif
  }

  if (!su_msg_is_non_null(nua->nua_current))
    return;

  if (e->e_msg)
    msg_destroy(e->e_msg), e->e_msg = NULL;

  su_msg_destroy(nua->nua_current);
}

/** Get current request message. @NEW_1_12_4. */
msg_t *nua_current_request(nua_t const *nua)
{
  return nua && nua->nua_current ? su_msg_data(nua->nua_current)->e_msg : NULL;
}

/** Get request message from saved nua event. @NEW_1_12_4. */
msg_t *nua_saved_event_request(nua_saved_event_t const *saved)
{
  return saved ? su_msg_data(saved)->e_msg : NULL;
}

/** Save nua event and its arguments */
int nua_save_event(nua_t *nua, nua_saved_event_t return_saved[1])
{
  if (nua && return_saved) {
    su_msg_save(return_saved, nua->nua_current);
    if (su_msg_is_non_null(return_saved)) {
      /* Remove references to tasks */
      su_msg_remove_refs(return_saved);
      return 1;
    }
  }
  return 0;
}

/** Get event data */
nua_event_data_t const *nua_event_data(nua_saved_event_t const saved[1])
{
  return saved ? su_msg_data(saved) : NULL;
}

/** Destroy saved event */
void nua_destroy_event(nua_saved_event_t saved[1])
{
  if (su_msg_is_non_null(saved)) {
    event_t *e = su_msg_data(saved);
    nua_handle_t *nh = e->e_nh;

    if (e->e_msg)
      msg_destroy(e->e_msg), e->e_msg = NULL;

    if (nh && !NH_IS_DEFAULT(nh) && nua_handle_unref(nh)) {
      SU_DEBUG_9(("nua(%p): freed by application\n", (void *)nh));
    }

    su_msg_destroy(saved);
  }
}

/* ---------------------------------------------------------------------- */

struct nua_stack_handle_make_replaces_args {
  sip_replaces_t *retval;
  nua_handle_t *nh;
  su_home_t *home;
  int early_only;
};

static int nua_stack_handle_make_replaces_call(void *arg)
{
  struct nua_stack_handle_make_replaces_args *a = arg;

  a->retval = nua_stack_handle_make_replaces(a->nh, a->home, a->early_only);

  return 0;
}


/**Generate a @Replaces header for handle.
 *
 * A @Replaces header contains the @CallID value, @From and @To tags
 * corresponding to SIP dialog associated with handle @a nh. Note that the
 * @Replaces matches with dialog of the remote peer,
 * nua_handle_by_replaces() does not return same handle (unless you swap
 * rp_from_tag and rp_to_tag in @Replaces header).
 *
 * A @Replaces header is used in attended transfer, among other things.
 *
 * @param nh pointer to operation handle
 * @param home memory home used to allocate the header
 * @param early_only if true, include "early-only" parameter in @Replaces, too
 *
 * @return A newly created @Replaces header.
 *
 * @since New in @VERSION_1_12_4.
 *
 * @sa nua_handle_by_replaces(), @Replaces, @RFC3891, @RFC3515, nua_refer(),
 * #nua_i_refer(), @ReferTo, nta_leg_make_replaces(),
 * sip_headers_as_url_query()
 */
sip_replaces_t *nua_handle_make_replaces(nua_handle_t *nh,
					 su_home_t *home,
					 int early_only)
{
  if (nh && nh->nh_valid && nh->nh_nua) {
    struct nua_stack_handle_make_replaces_args a = { NULL, nh, home, early_only };

    if (su_task_execute(nh->nh_nua->nua_server,
			nua_stack_handle_make_replaces_call, (void *)&a,
			NULL) == 0) {
      return a.retval;
    }
  }
  return NULL;
}

struct nua_stack_handle_by_replaces_args {
  nua_handle_t *retval;
  nua_t *nua;
  sip_replaces_t const *r;
};

static int nua_stack_handle_by_replaces_call(void *arg)
{
  struct nua_stack_handle_by_replaces_args *a = arg;

  a->retval = nua_stack_handle_by_replaces(a->nua, a->r);

  return 0;
}

/** Obtain a new reference to an existing handle based on @Replaces header.
 *
 * @since New in @VERSION_1_12_4.
 *
 * @note
 * You should release the reference with nua_handle_unref() when you are
 * done with the handle.
 *
 * @sa nua_handle_make_replaces(), @Replaces, @RFC3891, nua_refer(),
 * #nua_i_refer, @ReferTo, nta_leg_by_replaces()
 */
nua_handle_t *nua_handle_by_replaces(nua_t *nua, sip_replaces_t const *r)
{
  if (nua) {
    struct nua_stack_handle_by_replaces_args a = { NULL, nua, r };

    if (su_task_execute(nua->nua_server,
			nua_stack_handle_by_replaces_call, (void *)&a,
			NULL) == 0) {
      nua_handle_t *nh = a.retval;

      if (nh && !NH_IS_DEFAULT(nh) && nh->nh_valid)
	return nua_handle_ref(nh);
    }
  }
  return NULL;
}
