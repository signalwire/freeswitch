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

#include <sofia-sip/su_string.h>
#include <sofia-sip/sip_protos.h>
#include <sofia-sip/sip_status.h>

#include "nua_stack.h"

/* ======================================================================== */
/* MESSAGE */

/**@fn void nua_message( \
 *       nua_handle_t *nh, tag_type_t tag, tag_value_t value, ...);
 *
 * Send an instant message.
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
 *    Header tags defined in <sofia-sip/sip_tag.h>
 *
 * @par Events:
 *    #nua_r_message
 *
 * @sa #nua_i_message, @RFC3428
 */

static int nua_message_client_init(nua_client_request_t *cr,
				   msg_t *, sip_t *,
				   tagi_t const *tags);

static nua_client_methods_t const nua_message_client_methods = {
  SIP_METHOD_MESSAGE,		/* crm_method, crm_method_name */
  0,				/* crm_extra */
  {				/* crm_flags */
    /* create_dialog */ 0,
    /* in_dialog */ 0,
    /* target refresh */ 0
  },
  NULL,				/* crm_template */
  nua_message_client_init,	/* crm_init */
  NULL,				/* crm_send */
  NULL,				/* crm_check_restart */
  NULL,				/* crm_recv */
  NULL,				/* crm_preliminary */
  NULL,				/* crm_report */
  NULL,				/* crm_complete */

};

int
nua_stack_message(nua_t *nua,
		  nua_handle_t *nh,
		  nua_event_t e,
		  tagi_t const *tags)
{
  return nua_client_create(nh, e, &nua_message_client_methods, tags);
}

static int nua_message_client_init(nua_client_request_t *cr,
				   msg_t *msg, sip_t *sip,
				   tagi_t const *tags)
{
  if (NH_PGET(cr->cr_owner, win_messenger_enable))
    cr->cr_contactize = 1;
  return 0;
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

int nua_message_server_init(nua_server_request_t *sr);
int nua_message_server_params(nua_server_request_t *, tagi_t const *);

nua_server_methods_t const nua_message_server_methods =
  {
    SIP_METHOD_MESSAGE,
    nua_i_message,		/* Event */
    {
      0,			/* Do not create dialog */
      0,			/* Can be initial request */
      0,			/* Perhaps a target refresh request? */
      0,			/* Do not add contact by default */
    },
    nua_message_server_init,
    nua_base_server_preprocess,
    nua_message_server_params,
    nua_base_server_respond,
    nua_base_server_report,
  };

int nua_message_server_init(nua_server_request_t *sr)
{
  if (!NH_PGET(sr->sr_owner, message_enable))
    return SR_STATUS1(sr, SIP_403_FORBIDDEN);

  return 0;
}

int nua_message_server_params(nua_server_request_t *sr,
			      tagi_t const *tags)
{
  if (NH_PGET(sr->sr_owner, win_messenger_enable))
    sr->sr_add_contact = 1;

  return 0;
}
