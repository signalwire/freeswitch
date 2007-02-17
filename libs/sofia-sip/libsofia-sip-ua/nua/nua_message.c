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

/**@CFILE nua_message.c
 * @brief MESSAGE method
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Mar  8 17:01:22 EET 2006 ppessi
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

/* ======================================================================== */
/* MESSAGE */

/** Send an instant message. 
 *
 * Send an instant message using SIP MESSAGE method.
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
 *    #nua_r_message
 *
 * @sa #nua_i_message, @RFC3428
 */

static int process_response_to_message(nua_handle_t *nh,
				       nta_outgoing_t *orq,
				       sip_t const *sip);

int 
nua_stack_message(nua_t *nua, nua_handle_t *nh, nua_event_t e, tagi_t const *tags)
{ 
  nua_client_request_t *cr = nh->nh_ds->ds_cr;
  msg_t *msg;
  sip_t *sip;

  if (nh_is_special(nh)) {
    return UA_EVENT2(e, 900, "Invalid handle for MESSAGE");
  }
  else if (cr->cr_orq) {
    return UA_EVENT2(e, 900, "Request already in progress");
  }

  nua_stack_init_handle(nua, nh, TAG_NEXT(tags));

  msg = nua_creq_msg(nua, nh, cr, cr->cr_retry_count,
			 SIP_METHOD_MESSAGE,
			 NUTAG_ADD_CONTACT(NH_PGET(nh, win_messenger_enable)),
			 TAG_NEXT(tags));
  sip = sip_object(msg);

  if (sip)
    cr->cr_orq = nta_outgoing_mcreate(nua->nua_nta,
				      process_response_to_message, nh, NULL,
				      msg,
				      SIPTAG_END(), TAG_NEXT(tags));
  if (!cr->cr_orq) {
    msg_destroy(msg);
    return UA_EVENT1(e, NUA_INTERNAL_ERROR);
  }

  return cr->cr_event = e;
}

void restart_message(nua_handle_t *nh, tagi_t *tags)
{
  nua_creq_restart(nh, nh->nh_ds->ds_cr, process_response_to_message, tags);
}

/** @NUA_EVENT nua_r_message
 *
 * Response to an outgoing @b MESSAGE request.
 *
 * @param status response status code
 *               (if the request is retried, @a status is 100, the @a
 *               sip->sip_status->st_status contain the real status code
 *               from the response message, e.g., 302, 401, or 407)
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the message
 * @param hmagic application context associated with the handle
 * @param sip    response to MESSAGE request or NULL upon an error
 *               (status code is in @a status and 
 *                descriptive message in @a phrase parameters)
 * @param tags   empty
 *
 * @sa nua_message(), #nua_i_message, @RFC3428
 *
 * @END_NUA_EVENT
 */

static int process_response_to_message(nua_handle_t *nh,
				       nta_outgoing_t *orq,
				       sip_t const *sip)
{
  if (nua_creq_check_restart(nh, nh->nh_ds->ds_cr, orq, sip, restart_message))
    return 0;
  return nua_stack_process_response(nh, nh->nh_ds->ds_cr, orq, sip, TAG_END());
}

/** @NUA_EVENT nua_i_message
 *
 * @brief Incoming @b MESSAGE request.
 *
 * The @b MESSAGE request does not create a dialog. If the incoming @b
 * MESSAGE request is not assiciated with an existing dialog the stack
 * creates a new handle for it. If the handle @a nh is not bound, you should
 * probably destroy it after responding to the MESSAGE request.
 *
 * @param status status code of response sent automatically by stack
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the message
 * @param hmagic application context associated with the handle
 *               (maybe NULL if outside session)
 * @param sip    incoming MESSAGE request
 * @param tags   empty
 *
 * @sa nua_message(), #nua_r_message, @RFC3428, @RFC3862
 *
 * @END_NUA_EVENT
 */

int nua_stack_process_message(nua_t *nua,
			      nua_handle_t *nh,
			      nta_incoming_t *irq,
			      sip_t const *sip)
{
  msg_t *msg;

  if (nh
      ? !NH_PGET(nh, message_enable)
      : !DNH_PGET(nua->nua_dhandle, message_enable))
    return 403;

  if (nh == NULL)
    if (!(nh = nua_stack_incoming_handle(nua, irq, sip, 0)))
      return 500;		/* respond with 500 Internal Server Error */

  msg = nta_incoming_getrequest(irq);

  nua_stack_event(nh->nh_nua, nh, msg, nua_i_message, SIP_200_OK, TAG_END());

#if 0 /* XXX */
  if (nh->nh_nua->nua_messageRespond) {	
    nh->nh_irq = irq;
    return 0;
  }
#endif

  return 200;
}
