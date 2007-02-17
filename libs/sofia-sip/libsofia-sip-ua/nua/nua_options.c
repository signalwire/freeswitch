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

/**@CFILE nua_options.c
 * @brief Implementation of OPTIONS client.
 *
 * OPTIONS server is in nua_session.c.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Mar  8 17:02:19 EET 2006 ppessi
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

/**@fn void nua_options(nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...);
 *
 * Query capabilities from server with OPTIONS request.
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return 
 *    nothing
 *
 * @par Related Tags:
 *    Tags in <sip_tag.h>
 *
 * @par Events:
 *    #nua_r_options
 *
 * @sa #nua_i_options, @RFC3261 section 10
 */

static int process_response_to_options(nua_handle_t *nh,
				       nta_outgoing_t *orq,
				       sip_t const *sip);

int
nua_stack_options(nua_t *nua, nua_handle_t *nh, nua_event_t e, tagi_t const *tags)
{
  nua_client_request_t *cr = nh->nh_ds->ds_cr;
  msg_t *msg;

  if (nh_is_special(nh)) {
    return UA_EVENT2(e, 900, "Invalid handle for OPTIONS");
  }
  else if (cr->cr_orq) {
    return UA_EVENT2(e, 900, "Request already in progress");
  }

  nua_stack_init_handle(nua, nh, TAG_NEXT(tags));

  msg = nua_creq_msg(nua, nh, cr, cr->cr_retry_count,
			 SIP_METHOD_OPTIONS, 
			 TAG_NEXT(tags));

  cr->cr_orq = nta_outgoing_mcreate(nua->nua_nta,
				    process_response_to_options, nh, NULL,
				    msg,
				    SIPTAG_END(), TAG_NEXT(tags));
  if (!cr->cr_orq) {
    msg_destroy(msg);
    return UA_EVENT1(e, NUA_INTERNAL_ERROR);
  }

  return cr->cr_event = e;
}

void restart_options(nua_handle_t *nh, tagi_t *tags)
{
  nua_creq_restart(nh, nh->nh_ds->ds_cr, process_response_to_options, tags);
}

/** @NUA_EVENT nua_r_options
 *
 * Answer to outgoing OPTIONS.
 *
 * @param status response status code
 *               (if the request is retried the @a status is 100 and the @a
 *               sip->sip_status->st_status contain the real status code
 *               from the response message, e.g., 302, 401, or 407)
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the incoming OPTIONS request
 * @param hmagic application context associated with the handle
 * @param sip    response to OPTIONS request or NULL upon an error
 *               (status code is in @a status and 
 *                descriptive message in @a phrase parameters)
 * @param tags   empty
 *
 * @sa nua_options(), @RFC3261 section 11, #nua_i_options
 *
 * @END_NUA_EVENT
 */

static int process_response_to_options(nua_handle_t *nh,
				       nta_outgoing_t *orq,
				       sip_t const *sip)
{
  if (nua_creq_check_restart(nh, nh->nh_ds->ds_cr, orq, sip, restart_options))
    return 0;
  return nua_stack_process_response(nh, nh->nh_ds->ds_cr, orq, sip, TAG_END());
}
