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

#include <sofia-sip/su_string.h>
#include <sofia-sip/sip_protos.h>
#include <sofia-sip/sip_status.h>

#include "nua_stack.h"

/**Send a request message with an extension method.
 *
 * Send a request message with the request method specified with
 * NUTAG_METHOD().
 *
 * @param nh              Pointer to operation handle
 * @param tag, value, ... List of tagged parameters
 *
 * @return
 *    nothing
 *
 * Note that it is possible to send a request with any method (except
 * perhaps @b INVITE, @b ACK or @b CANCEL) using this function.
 *
 * @par Related Tags:
 *    NUTAG_METHOD() \n
 *    NUTAG_URL() \n
 *    Tags of nua_set_hparams() \n
 *    Header tags defined in <sofia-sip/sip_tag.h>
 *
 * @par Events:
 *    #nua_r_method
 *
 * @sa SIP_METHOD_UNKNOWN(), #nua_r_method, #nua_i_method
 *
 * @since New in @VERSION_1_12_4.
 */

static nua_client_methods_t const nua_method_client_methods = {
  SIP_METHOD_UNKNOWN,		/* crm_method, crm_method_name */
  0,				/* crm_extra */
  {				/* crm_flags */
    /* create_dialog */ 0,
    /* in_dialog */ 0,
    /* target_refresh */ 1,
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

int
nua_stack_method(nua_t *nua, nua_handle_t *nh, nua_event_t e, tagi_t const *tags)
{
  return nua_client_create(nh, e, &nua_method_client_methods, tags);
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
 * @param sip    headers in incoming request (see also nua_current_request())
 * @param tags   NUTAG_METHOD()
 *
 * The extension method name is in sip->sip_request->rq_method_name, too.
 *
 * @note If the @a status is < 200, it is up to application to respond to
 * the request with nua_respond(). If the handle is destroyed, the stack
 * returns a <i>500 Internal Server Error</i> response to any unresponded
 * request.
 *
 * @sa nua_method(), #nua_r_method, NUTAG_ALLOW(), NUTAG_APPL_METHOD(),
 * nua_respond(), NUTAG_WITH(), NUTAG_WITH_THIS(), NUTAG_
 *
 * @END_NUA_EVENT
 */

nua_server_methods_t const nua_extension_server_methods =
  {
    SIP_METHOD_UNKNOWN,
    nua_i_method,		/* Event */
    {
      1,			/* Do create dialog */
      0,			/* Can be an initial request */
      1,			/* Perhaps a target refresh request? */
      1,			/* Add a contact? */
    },
    nua_base_server_init,
    nua_base_server_preprocess,
    nua_base_server_params,
    nua_base_server_respond,
    nua_base_server_report,
  };
