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

/**@CFILE nua_extension.c
 * @brief Extension method
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Mon Nov 13 15:18:54 EET 2006
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

static int process_response_to_method(nua_handle_t *nh,
				       nta_outgoing_t *orq,
				       sip_t const *sip);
static void restart_method(nua_handle_t *nh, tagi_t *tags);
static int respond_to_method(nua_server_request_t *sr, tagi_t const *tags);

/** Send an extension request. 
 *
 * Send an entension request message.
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return 
 *    nothing
 *
 * @par Related Tags:
 *    NUTAG_METHOD() \n
 *    NUTAG_URL() \n
 *    Tags of nua_set_hparams() \n
 *    Tags in <sip_tag.h>
 *
 * @par Events:
 *    #nua_r_method
 *
 * @sa SIP_METHOD_UNKNOWN(), #nua_r_method, #nua_i_method
 *
 * @since New in @VERSION_1_12_4.
 */

int 
nua_stack_method(nua_t *nua, nua_handle_t *nh, nua_event_t e, tagi_t const *tags)
{ 
  nua_client_request_t *cr = nh->nh_ds->ds_cr;
  msg_t *msg;

  if (cr->cr_orq)
    return UA_EVENT2(e, 900, "Request already in progress");

  nua_stack_init_handle(nua, nh, TAG_NEXT(tags));

  msg = nua_creq_msg(nua, nh, cr, cr->cr_retry_count,
		     SIP_METHOD_UNKNOWN,
		     TAG_NEXT(tags));
  if (msg)
    cr->cr_orq = nta_outgoing_mcreate(nua->nua_nta,
				      process_response_to_method, nh, NULL,
				      msg,
				      SIPTAG_END(),
				      TAG_NEXT(tags));
  if (!cr->cr_orq) {
    msg_destroy(msg);
    return UA_EVENT1(e, NUA_INTERNAL_ERROR);
  }

  return cr->cr_event = e;
}

/** @NUA_EVENT nua_r_method
 *
 * Response to an outgoing extension request.
 *
 * @param status response status code
 *               (if the request is retried, @a status is 100, the @a
 *               sip->sip_status->st_status contain the real status code
 *               from the response method, e.g., 302, 401, or 407)
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the method
 * @param hmagic application context associated with the handle
 * @param sip    response to the extension request or NULL upon an error
 *               (status code is in @a status and 
 *                descriptive method in @a phrase parameters)
 * @param tags   empty
 *
 * @sa nua_method(), #nua_i_method, @RFC3428
 *
 * @END_NUA_EVENT
 */

static int process_response_to_method(nua_handle_t *nh,
				       nta_outgoing_t *orq,
				       sip_t const *sip)
{
  if (nua_creq_check_restart(nh, nh->nh_ds->ds_cr, orq, sip, restart_method))
    return 0;
  return nua_stack_process_response(nh, nh->nh_ds->ds_cr, orq, sip, TAG_END());
}

void restart_method(nua_handle_t *nh, tagi_t *tags)
{
  nua_creq_restart(nh, nh->nh_ds->ds_cr, process_response_to_method, tags);
}

/** @NUA_EVENT nua_i_method
 *
 * @brief Incoming extension request.
 *
 * The extension request does not create a dialog. If the incoming request
 * was not assiciated with an existing dialog the stack creates a new handle
 * for it. If the handle @a nh is not bound, you should probably destroy it
 * after responding to the request.
 *
 * @param status status code of response sent automatically by stack
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the method
 * @param hmagic application context associated with the handle
 *               (maybe NULL if outside session)
 * @param sip    incoming request
 * @param tags   NUTAG_METHOD()
 *
 * The extension name is in sip->sip_request->rq_method_name, too.
 *
 * @sa nua_method(), #nua_r_method
 *
 * @END_NUA_EVENT
 */

int nua_stack_process_method(nua_t *nua,
			      nua_handle_t *nh,
			      nta_incoming_t *irq,
			      sip_t const *sip)
{
  nua_server_request_t *sr, sr0[1];
  
  sr = SR_INIT(sr0);
  
  sr = nua_server_request(nua, nh, irq, sip, sr, sizeof *sr,
			  respond_to_method, 0);

  return nua_stack_server_event(nua, sr, nua_i_method, TAG_END());
}

static
int respond_to_method(nua_server_request_t *sr, tagi_t const *tags)
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
		    nua_i_error, 900, "Response to Extension Method Fails",
		    TAG_END());
  }
  
  return sr->sr_status >= 200 ? sr->sr_status : 0;
}
