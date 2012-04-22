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

#include <sofia-sip/su_string.h>
#include <sofia-sip/sip_protos.h>
#include <sofia-sip/sip_status.h>

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
 *    Header tags defined in <sofia-sip/sip_tag.h>
 *
 * @par Events:
 *    #nua_r_options
 *
 * @sa #nua_i_options, @RFC3261 section 10
 */

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

static nua_client_methods_t const nua_options_client_methods = {
  SIP_METHOD_OPTIONS,		/* crm_method, crm_method_name */
  0,				/* crm_extra */
  {				/* crm_flags */
    /* create_dialog */ 0,
    /* in_dialog */ 0,
    /* target refresh */ 0
  },
  NULL,				/* crm_template */
  NULL,				/* crm_init */
  NULL,				/* crm_send */
  NULL,				/* crm_check_restart */
  NULL,				/* crm_recv */
  NULL,				/* crm_preliminary */
  NULL,				/* crm_report */
  NULL,				/* crm_complete */
};

int nua_stack_options(nua_t *nua,
		      nua_handle_t *nh,
		      nua_event_t e,
		      tagi_t const *tags)
{
  return nua_client_create(nh, e, &nua_options_client_methods, tags);
}
