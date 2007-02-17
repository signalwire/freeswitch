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

#include <sofia-sip/string0.h>
#include <sofia-sip/sip_protos.h>
#include <sofia-sip/sip_status.h>

#define NTA_LEG_MAGIC_T      struct nua_handle_s
#define NTA_OUTGOING_MAGIC_T struct nua_handle_s

#include "nua_stack.h"

/* ====================================================================== */
/* Publish usage */

struct publish_usage {
  sip_etag_t *pu_etag;
};

static char const *nua_publish_usage_name(nua_dialog_usage_t const *du);
static int nua_publish_usage_add(nua_handle_t *nh,
				  nua_dialog_state_t *ds,
				  nua_dialog_usage_t *du);
static void nua_publish_usage_remove(nua_handle_t *nh,
				      nua_dialog_state_t *ds,
				      nua_dialog_usage_t *du);
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
			       nua_dialog_usage_t *du)
{
  struct publish_usage *pu = nua_dialog_usage_private(du);

  su_free(nh->nh_home, pu->pu_etag);

  ds->ds_has_publish = 0;	/* There can be only one */
}

/* ======================================================================== */
/* PUBLISH */

static int nua_stack_publish2(nua_t *nua, nua_handle_t *nh, nua_event_t e,
			      int refresh, tagi_t const *tags);

static int process_response_to_publish(nua_handle_t *nh,
				       nta_outgoing_t *orq,
				       sip_t const *sip);


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
 *    Tags in <sip_tag.h>
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
 *    Tags in <sip_tag.h>
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

int nua_stack_publish(nua_t *nua, nua_handle_t *nh, nua_event_t e,
		      tagi_t const *tags)
{
  return nua_stack_publish2(nua, nh, e, 0, tags);
}

static
int nua_stack_publish2(nua_t *nua, nua_handle_t *nh, nua_event_t e,
		       int refresh,
		       tagi_t const *tags)
{
  nua_dialog_usage_t *du;
  struct publish_usage *pu;
  nua_client_request_t *cr = nh->nh_ds->ds_cr;
  msg_t *msg = NULL;
  sip_t *sip;
  int remove_body = 0;

  if (nua_stack_set_handle_special(nh, nh_has_nothing, nua_r_publish) < 0)
    return UA_EVENT2(e, 900, "Invalid handle for PUBLISH");

  if (cr->cr_orq) {
    return UA_EVENT2(e, 900, "Request already in progress");
  }

  nua_stack_init_handle(nua, nh, TAG_NEXT(tags));

  if (e == nua_r_unpublish) {
    du = nua_dialog_usage_get(nh->nh_ds, nua_publish_usage, NULL);
    if (du)
      refresh = 1;
    else
      du = nua_dialog_usage_add(nh, nh->nh_ds, nua_publish_usage, NULL);
  }
  else if (!refresh)
    du = nua_dialog_usage_add(nh, nh->nh_ds, nua_publish_usage, NULL);
  else
    du = nua_dialog_usage_get(nh->nh_ds, nua_publish_usage, NULL);

  if (!du)
    return UA_EVENT1(e, NUA_INTERNAL_ERROR);

  nua_dialog_usage_reset_refresh(du);
  pu = nua_dialog_usage_private(du); assert(pu);

  if (refresh) {
    if (cr->cr_msg)
      msg_destroy(cr->cr_msg);
    cr->cr_msg = msg_copy(du->du_msg);
    remove_body = pu->pu_etag != NULL;
  }

  msg = nua_creq_msg(nua, nh, cr, cr->cr_retry_count || refresh,
		     SIP_METHOD_PUBLISH,
		     NUTAG_ADD_CONTACT(0),
		     TAG_NEXT(tags));
  sip = sip_object(msg);

  if (!msg || !sip) 
    goto error;

  du->du_terminating =
    e != nua_r_publish ||
    (sip->sip_expires && sip->sip_expires->ex_delta == 0);

  if (!du->du_terminating && !refresh) {
    /* Save template */
    if (du->du_msg)
      msg_destroy(du->du_msg);
    du->du_msg = msg_ref_create(cr->cr_msg);
  }

  cr->cr_orq =
    nta_outgoing_mcreate(nua->nua_nta,
			 process_response_to_publish, nh, NULL,
			 msg,
			 SIPTAG_IF_MATCH(pu->pu_etag),
			 TAG_IF(remove_body, SIPTAG_PAYLOAD(NONE)),
			 TAG_IF(remove_body, SIPTAG_CONTENT_TYPE(NONE)),
			 TAG_IF(e != nua_r_publish,
				SIPTAG_EXPIRES_STR("0")),
			 SIPTAG_END(), TAG_NEXT(tags));
  if (!cr->cr_orq)
    goto error;

  cr->cr_usage = du;

  return cr->cr_event = e;

 error:
  msg_destroy(msg);
  if (!du->du_ready == 0)
    nua_dialog_usage_remove(nh, nh->nh_ds, du);
  return UA_EVENT1(e, NUA_INTERNAL_ERROR);
}


static void
restart_publish(nua_handle_t *nh, tagi_t *tags)
{
  nua_creq_restart(nh, nh->nh_ds->ds_cr, process_response_to_publish, tags);
}


static
int process_response_to_publish(nua_handle_t *nh,
				nta_outgoing_t *orq,
				sip_t const *sip)
{
  int status = sip->sip_status->st_status;
  nua_client_request_t *cr = nh->nh_ds->ds_cr;
  nua_dialog_usage_t *du = cr->cr_usage;
  struct publish_usage *pu = nua_dialog_usage_private(du);
  unsigned saved_retry_count = cr->cr_retry_count + 1;

  if (nua_creq_check_restart(nh, cr, orq, sip, restart_publish))
    return 0;

  if (status < 200 || pu == NULL)
    return nua_stack_process_response(nh, cr, orq, sip, TAG_END());

  if (pu->pu_etag)
    su_free(nh->nh_home, pu->pu_etag), pu->pu_etag = NULL;

  if (!du->du_terminating) {
    int retry = 0, invalid_expiration = 0;

    if (status < 300) {
      if (!sip->sip_expires)
	invalid_expiration = 1;
      else if (sip->sip_expires->ex_delta == 0)
	retry = 1, invalid_expiration = 1;
    }
    else if (status == 412)
      retry = 1;

    if (status < 300 && !invalid_expiration && !retry) {
      pu->pu_etag = sip_etag_dup(nh->nh_home, sip->sip_etag);
      du->du_ready = 1;
      nua_dialog_usage_set_expires(du, sip->sip_expires->ex_delta);
    }
    else if (retry && saved_retry_count < NH_PGET(nh, retry_count)) {
      msg_t *response = nta_outgoing_getresponse(orq);
      nua_stack_event(nh->nh_nua, nh, response, cr->cr_event,
      		100, "Trying re-PUBLISH",
      		TAG_END());
      nua_creq_deinit(cr, orq);
      nua_stack_publish2(nh->nh_nua, nh, cr->cr_event, 1, NULL);
      cr->cr_retry_count = saved_retry_count;
      return 0;
    }
    else if (invalid_expiration) {
      msg_t *response = nta_outgoing_getresponse(orq);
      nua_stack_event(nh->nh_nua, nh, response, cr->cr_event,
      		900, "Received Invalid Expiration Time",
      		TAG_END());
      nua_dialog_usage_remove(nh, nh->nh_ds, cr->cr_usage);
      nua_creq_deinit(cr, orq);
      cr->cr_usage = NULL;
      return 0;
    }
  }

  return nua_stack_process_response(nh, cr, orq, sip, TAG_END());
}


static void nua_publish_usage_refresh(nua_handle_t *nh,
				      nua_dialog_state_t *ds,
				      nua_dialog_usage_t *du,
				      sip_time_t now)
{
  if (ds->ds_cr->cr_usage == du) /* Already publishing. */
    return;
  nua_stack_publish2(nh->nh_nua, nh, nua_r_publish, 1, NULL);
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
  nua_client_request_t *cr = ds->ds_cr;

  if (!cr->cr_usage) {
    /* Unpublish */
    nua_stack_publish2(nh->nh_nua, nh, nua_r_destroy, 1, NULL);
    return cr->cr_usage != du;
  }

  if (!du->du_ready && !cr->cr_orq)
    return 1;			/* had unauthenticated initial request */

  return -1;  /* Request in progress */
}

/* ---------------------------------------------------------------------- */
/* Server side */

static
int respond_to_publish(nua_server_request_t *sr, tagi_t const *tags);

/** @NUA_EVENT nua_i_publish
 *
 * Incoming PUBLISH request.
 *
 * In order to receive #nua_i_publish events, the application must enable
 * both the PUBLISH method with NUTAG_ALLOW() tag and the acceptable SIP
 * events with nua_set_params() tag NUTAG_ALLOW_EVENTS(). 
 *
 * The nua_response() call responding to a PUBLISH request must have
 * NUTAG_WITH() (or NUTAG_WITH_CURRENT()/NUTAG_WITH_SAVED()) tag. Note that
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

int nua_stack_process_publish(nua_t *nua,
			      nua_handle_t *nh,
			      nta_incoming_t *irq,
			      sip_t const *sip)
{
  nua_server_request_t *sr, sr0[1];
  sip_allow_events_t *allow_events = NUA_PGET(nua, nh, allow_events);
  sip_event_t *o = sip->sip_event;
  char const *event = o ? o->o_type : NULL;
  
  sr = SR_INIT(sr0);
  
  if (!allow_events)
    SR_STATUS1(sr, SIP_501_NOT_IMPLEMENTED);
  else if (!event || !msg_header_find_param(allow_events->k_common, event))
    SR_STATUS1(sr, SIP_489_BAD_EVENT);

  sr = nua_server_request(nua, nh, irq, sip, sr, sizeof *sr,
			  respond_to_publish, 0);

  return nua_stack_server_event(nua, sr, nua_i_publish, TAG_END());
}

static
int respond_to_publish(nua_server_request_t *sr, tagi_t const *tags)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_t *nua = nh->nh_nua;
  msg_t *msg;

  msg = nua_server_response(sr, sr->sr_status, sr->sr_phrase, TAG_NEXT(tags));

  if (msg) {
    nta_incoming_mreply(sr->sr_irq, msg);
  }
  else {
    SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);
    nta_incoming_treply(sr->sr_irq, sr->sr_status, sr->sr_phrase, TAG_END());
    nua_stack_event(nua, nh, NULL,
		    nua_i_error, 900, "PUBLISH Response Fails",
		    TAG_END());
  }
  
  return sr->sr_status >= 200 ? sr->sr_status : 0;
}
