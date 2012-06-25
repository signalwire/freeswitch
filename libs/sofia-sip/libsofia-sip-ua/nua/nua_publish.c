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

/**@CFILE nua_publish.c
 * @brief PUBLISH and publications
 *
 * @sa @RFC3903
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Mar  8 17:01:32 EET 2006 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <assert.h>

#include <sofia-sip/su_string.h>
#include <sofia-sip/sip_protos.h>
#include <sofia-sip/sip_status.h>

#include "nua_stack.h"

/* ====================================================================== */
/* Publish usage */

struct publish_usage {
  sip_etag_t *pu_etag;
  int pu_published;
};

static char const *nua_publish_usage_name(nua_dialog_usage_t const *du);
static int nua_publish_usage_add(nua_handle_t *nh,
				  nua_dialog_state_t *ds,
				  nua_dialog_usage_t *du);
static void nua_publish_usage_remove(nua_handle_t *nh,
				     nua_dialog_state_t *ds,
				     nua_dialog_usage_t *du,
				     nua_client_request_t *cr,
				     nua_server_request_t *sr);
static void nua_publish_usage_refresh(nua_handle_t *nh,
				      nua_dialog_state_t *ds,
				      nua_dialog_usage_t *du,
				      sip_time_t now);
static int nua_publish_usage_shutdown(nua_handle_t *nh,
				      nua_dialog_state_t *ds,
				      nua_dialog_usage_t *du);

static nua_usage_class const nua_publish_usage[1] = {
  {
    sizeof (struct publish_usage),
    sizeof nua_publish_usage,
    nua_publish_usage_add,
    nua_publish_usage_remove,
    nua_publish_usage_name,
    nua_base_usage_update_params,
    NULL,
    nua_publish_usage_refresh,
    nua_publish_usage_shutdown,
  }};

static
char const *nua_publish_usage_name(nua_dialog_usage_t const *du)
{
  return "publish";
}

static
int nua_publish_usage_add(nua_handle_t *nh,
			   nua_dialog_state_t *ds,
			   nua_dialog_usage_t *du)
{
  if (ds->ds_has_publish)
    return -1;			/* There can be only one */
  ds->ds_has_publish = 1;
  return 0;
}

static
void nua_publish_usage_remove(nua_handle_t *nh,
			      nua_dialog_state_t *ds,
			      nua_dialog_usage_t *du,
			      nua_client_request_t *cr,
			      nua_server_request_t *sr
)
{
  struct publish_usage *pu = NUA_DIALOG_USAGE_PRIVATE(du);

  su_free(nh->nh_home, pu->pu_etag);

  ds->ds_has_publish = 0;	/* There can be only one */
}

/* ======================================================================== */
/* PUBLISH */

/**@fn \
 * void nua_publish(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...);
 *
 * Send PUBLISH request to publication server.
 *
 * Request status will be delivered to the application using #nua_r_publish
 * event. When successful the publication will be updated periodically until
 * nua_unpublish() is called or handle is destroyed. Note that the periodic
 * updates and unpublish do not include the original message body nor the @b
 * Content-Type header. Instead, the periodic update will include the
 * @SIPIfMatch header, which was generated from the latest @SIPETag
 * header received in response to @b PUBLISH request.
 *
 * The handle used for publication cannot be used for any other purposes.
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *    nothing
 *
 * @par Related Tags:
 *    NUTAG_URL() \n
 *    Tags of nua_set_hparams() \n
 *    Header tags defined in <sofia-sip/sip_tag.h>
 *
 * @par Events:
 *    #nua_r_publish
 *
 * @sa #nua_r_publish, @RFC3903, @SIPIfMatch,
 * nua_unpublish(), #nua_r_unpublish, #nua_i_publish
 */

/** @NUA_EVENT nua_r_publish
 *
 * Response to an outgoing PUBLISH.
 *
 * The PUBLISH request may be sent explicitly by nua_publish() or implicitly
 * by NUA state machine.
 *
 * @param status status code of PUBLISH request
 *               (if the request is retried, @a status is 100, the @a
 *               sip->sip_status->st_status contain the real status code
 *               from the response message, e.g., 302, 401, or 407)
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the publication
 * @param hmagic application context associated with the handle
 * @param sip    response to PUBLISH request or NULL upon an error
 *               (status code is in @a status and
 *                descriptive message in @a phrase parameters)
 * @param tags   empty
 *
 * @sa nua_publish(), @RFC3903, @SIPETag, @Expires,
 * nua_unpublish(), #nua_r_unpublish, #nua_i_publish
 *
 * @END_NUA_EVENT
 */

/**@fn \
void nua_unpublish(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...);
 *
 * Send un-PUBLISH request to publication server. Un-PUBLISH request is just
 * a PUBLISH request with @Expires set to 0. It is possible to un-publish a
 * publication not associated with the handle by providing correct ETag in
 * SIPTAG_IF_MATCH() or SIPTAG_IF_MATCH_STR() tags.
 *
 * Response to the un-PUBLISH request will be delivered to the application
 * using #nua_r_unpublish event.
 *
 * The handle used for publication cannot be used for any other purposes.
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *    nothing
 *
 * @par Related Tags:
 *    NUTAG_URL() \n
 *    SIPTAG_IF_MATCH(), SIPTAG_IF_MATCH_STR() \n
 *    SIPTAG_EVENT(), SIPTAG_EVENT_STR() \n
 *    Tags of nua_set_hparams() \n
 *    Other header tags defined in <sofia-sip/sip_tag.h> except SIPTAG_EXPIRES() or SIPTAG_EXPIRES_STR()
 *
 * @par Events:
 *    #nua_r_unpublish
 *
 * @sa #nua_r_unpublish, @RFC3903, @SIPIfMatch,
 * #nua_i_publish, nua_publish(), #nua_r_publish
 */

/** @NUA_EVENT nua_r_unpublish
 *
 * Response to an outgoing un-PUBLISH.
 *
 * @param status response status code
 *               (if the request is retried, @a status is 100, the @a
 *               sip->sip_status->st_status contain the real status code
 *               from the response message, e.g., 302, 401, or 407)
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the publication
 * @param hmagic application context associated with the handle
 * @param sip    response to PUBLISH request or NULL upon an error
 *               (status code is in @a status and
 *                descriptive message in @a phrase parameters)
 * @param tags   empty
 *
 * @sa nua_unpublish(), @RFC3903, @SIPETag, @Expires,
 * nua_publish(), #nua_r_publish, #nua_i_publish
 *
 * @END_NUA_EVENT
 */

static int nua_publish_client_template(nua_client_request_t *cr,
				       msg_t **return_msg,
				       tagi_t const *tags);
static int nua_publish_client_init(nua_client_request_t *cr,
				   msg_t *, sip_t *,
				   tagi_t const *tags);
static int nua_publish_client_request(nua_client_request_t *cr,
				      msg_t *, sip_t *,
				      tagi_t const *tags);
static int nua_publish_client_check_restart(nua_client_request_t *cr,
					    int status, char const *phrase,
					    sip_t const *sip);
static int nua_publish_client_response(nua_client_request_t *cr,
				       int status, char const *phrase,
				       sip_t const *sip);

static nua_client_methods_t const nua_publish_client_methods = {
  SIP_METHOD_PUBLISH,		/* crm_method, crm_method_name */
  0,				/* crm_extra */
  {				/* crm_flags */
    /* create_dialog */ 0,
    /* in_dialog */ 0,
    /* target refresh */ 0
  },
  nua_publish_client_template,	/* crm_template */
  nua_publish_client_init,	/* crm_init */
  nua_publish_client_request,	/* crm_send */
  nua_publish_client_check_restart, /* crm_check_restart */
  nua_publish_client_response,	/* crm_recv */
  NULL,				/* crm_preliminary */
  NULL,				/* crm_report */
  NULL,				/* crm_complete */
};

/**@internal Send PUBLISH. */
int nua_stack_publish(nua_t *nua,
		     nua_handle_t *nh,
		     nua_event_t e,
		     tagi_t const *tags)
{
  return nua_client_create(nh, e, &nua_publish_client_methods, tags);
}

static int nua_publish_client_template(nua_client_request_t *cr,
				       msg_t **return_msg,
				       tagi_t const *tags)
{
  nua_dialog_usage_t *du;

  if (cr->cr_event == nua_r_publish)
    return 0;

  du = nua_dialog_usage_get(cr->cr_owner->nh_ds, nua_publish_usage, NULL);
  if (du && du->du_cr) {
    if (nua_client_set_target(cr, du->du_cr->cr_target) < 0)
      return -1;
    *return_msg = msg_copy(du->du_cr->cr_msg);
    return 1;
  }

  return 0;
}

static int nua_publish_client_init(nua_client_request_t *cr,
				   msg_t *msg, sip_t *sip,
				   tagi_t const *tags)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du;
  struct publish_usage *pu;

  if (cr->cr_event == nua_r_publish) {
    du = nua_dialog_usage_add(nh, nh->nh_ds, nua_publish_usage, NULL);
    if (!du)
      return -1;
    pu = nua_dialog_usage_private(du);
    pu->pu_published = 0;
    if (sip->sip_if_match) {
      pu->pu_etag = sip_etag_dup(nh->nh_home, sip->sip_if_match);
      if (!pu->pu_etag)
	return -1;
      sip_header_remove(msg, sip, (sip_header_t *)sip->sip_if_match);
    }
  }
  else
    du = nua_dialog_usage_get(nh->nh_ds, nua_publish_usage, NULL);

  cr->cr_usage = du;

  return 0;
}

static
int nua_publish_client_request(nua_client_request_t *cr,
			       msg_t *msg, sip_t *sip,
			       tagi_t const *tags)
{
  nua_dialog_usage_t *du = cr->cr_usage;
  int un, done;
  sip_etag_t const *etag = NULL;

  un = cr->cr_terminating ||
    cr->cr_event != nua_r_publish ||
    (du && du->du_shutdown) ||
    (sip->sip_expires && sip->sip_expires->ex_delta == 0);
  nua_client_set_terminating(cr, un);
  done = un;

  if (du) {
    struct publish_usage *pu = nua_dialog_usage_private(du);

    if (nua_client_bind(cr, du) < 0)
      return -1;
    if (pu->pu_published)
      done = 1;
    etag = pu->pu_etag;
  }

  return nua_base_client_trequest(cr, msg, sip,
				  SIPTAG_IF_MATCH(etag),
				  TAG_IF(done, SIPTAG_PAYLOAD(NONE)),
				  TAG_IF(done, SIPTAG_CONTENT_TYPE(NONE)),
				  TAG_IF(un, SIPTAG_EXPIRES_STR("0")),
				  TAG_NEXT(tags));
}

static int nua_publish_client_check_restart(nua_client_request_t *cr,
					    int status, char const *phrase,
					    sip_t const *sip)
{
  char const *restarting = NULL;

  if (cr->cr_terminating || !cr->cr_usage)
    ;
  else if (status == 412)
    restarting = phrase;
  else if (200 <= status && status < 300 &&
	   sip->sip_expires && sip->sip_expires->ex_delta == 0)
    restarting = "Immediate re-PUBLISH";

  if (restarting) {
    struct publish_usage *pu = nua_dialog_usage_private(cr->cr_usage);

    if (pu) {
      pu->pu_published = 0;
      su_free(cr->cr_owner->nh_home, pu->pu_etag), pu->pu_etag = NULL;
      if (nua_client_restart(cr, 100, restarting))
	return 0;
    }
  }

  return nua_base_client_check_restart(cr, status, phrase, sip);
}

static int nua_publish_client_response(nua_client_request_t *cr,
				       int status, char const *phrase,
				       sip_t const *sip)
{
  nua_handle_t *nh = cr->cr_owner;
  nua_dialog_usage_t *du = cr->cr_usage;

  if (!cr->cr_terminated && du && sip) {
    struct publish_usage *pu = nua_dialog_usage_private(du);
    sip_expires_t const *ex = sip->sip_expires;

    /* Reset state */
    pu->pu_published = 0;
    if (pu->pu_etag)
      su_free(nh->nh_home, pu->pu_etag), pu->pu_etag = NULL;

    if (status < 300) {
      pu->pu_published = 1;
      pu->pu_etag = sip_etag_dup(nh->nh_home, sip->sip_etag);

      if (!ex || ex->ex_delta == 0 || !pu->pu_etag) {
	cr->cr_terminated = 1;

	if (!ex || ex->ex_delta == 0)
	  SET_STATUS(900, "Received Invalid Expiration Time");
	else
	  SET_STATUS(900, _NUA_INTERNAL_ERROR_AT(__FILE__, __LINE__));
      }
      else
	nua_dialog_usage_set_refresh(du, ex->ex_delta);
    }
  }

  return nua_base_client_response(cr, status, phrase, sip, NULL);
}

static void nua_publish_usage_refresh(nua_handle_t *nh,
				     nua_dialog_state_t *ds,
				     nua_dialog_usage_t *du,
				     sip_time_t now)
{
  nua_client_request_t *cr = du->du_cr;

  if (cr) {
    if (nua_client_resend_request(cr, 0) >= 0)
      return;
  }

  nua_stack_event(nh->nh_nua, nh, NULL,
		  nua_r_publish, NUA_ERROR_AT(__FILE__, __LINE__),
		  NULL);

  nua_dialog_usage_remove(nh, ds, du, NULL, NULL);
}

/** @interal Shut down PUBLISH usage.
 *
 * @retval >0  shutdown done
 * @retval 0   shutdown in progress
 * @retval <0  try again later
 */
static int nua_publish_usage_shutdown(nua_handle_t *nh,
				     nua_dialog_state_t *ds,
				     nua_dialog_usage_t *du)
{
  nua_client_request_t *cr = du->du_cr;

  if (cr) {
    if (nua_client_resend_request(cr, 1) >= 0)
      return 0;
  }

  /* XXX - report to user */
  nua_dialog_usage_remove(nh, ds, du, NULL, NULL);
  return 200;
}

/* ---------------------------------------------------------------------- */
/* Server side */

/** @NUA_EVENT nua_i_publish
 *
 * Incoming PUBLISH request.
 *
 * In order to receive #nua_i_publish events, the application must enable
 * both the PUBLISH method with NUTAG_ALLOW() tag and the acceptable SIP
 * events with nua_set_params() tag NUTAG_ALLOW_EVENTS().
 *
 * The nua_response() call responding to a PUBLISH request must have
 * NUTAG_WITH() (or NUTAG_WITH_THIS()/NUTAG_WITH_SAVED()) tag. Note that
 * a successful response to PUBLISH @b MUST include @Expires and @SIPETag
 * headers.
 *
 * The PUBLISH request does not create a dialog. Currently the processing
 * of incoming PUBLISH creates a new handle for each incoming request which
 * is not assiciated with an existing dialog. If the handle @a nh is not
 * bound, you should probably destroy it after responding to the PUBLISH
 * request.
 *
 * @param status status code of response sent automatically by stack
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the incoming request
 * @param hmagic application context associated with the call
 *               (usually NULL)
 * @param sip    incoming PUBLISH request
 * @param tags   empty
 *
 * @sa @RFC3903, nua_respond(),
 * @Expires, @SIPETag, @SIPIfMatch, @Event,
 * nua_subscribe(), #nua_i_subscribe,
 * nua_notifier(), #nua_i_subscription,
 *
 * @since First used in @VERSION_1_12_4
 *
 * @END_NUA_EVENT
 */

int nua_publish_server_init(nua_server_request_t *sr);

nua_server_methods_t const nua_publish_server_methods =
  {
    SIP_METHOD_PUBLISH,
    nua_i_publish,		/* Event */
    {
      0,			/* Do not create dialog */
      0,			/* Initial request */
      0,			/* Not a target refresh request  */
      1,			/* Add Contact */
    },
    nua_publish_server_init,
    nua_base_server_preprocess,
    nua_base_server_params,
    nua_base_server_respond,
    nua_base_server_report,
  };

int nua_publish_server_init(nua_server_request_t *sr)
{
  sip_allow_events_t *allow_events = NH_PGET(sr->sr_owner, allow_events);
  sip_event_t *o = sr->sr_request.sip->sip_event;
  char const *event = o ? o->o_type : NULL;

  if (!allow_events)
    return SR_STATUS1(sr, SIP_501_NOT_IMPLEMENTED);
  else if (!event || !msg_header_find_param(allow_events->k_common, event))
    return SR_STATUS1(sr, SIP_489_BAD_EVENT);

  return 0;
}
