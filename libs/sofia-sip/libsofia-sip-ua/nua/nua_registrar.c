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

/**@CFILE nua_registrar.c
 * @brief REGISTER UAS
 *
 * @author Michael Jerris
 *
 * @date Created: Tue Oct  3 10:14:54 EEST 2006 ppessi
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
#include <sofia-sip/sip_util.h>

#define NTA_INCOMING_MAGIC_T struct nua_handle_s
#define NTA_RELIABLE_MAGIC_T struct nua_handle_s

#include "nua_stack.h"

/* ======================================================================== */
/* REGISTER */

/** @NUA_EVENT nua_i_register
 *
 * Incoming REGISTER request.
 *
 * In order to receive #nua_i_register events, the application must enable
 * the REGISTER method with NUTAG_ALLOW() tag, e.g.,
 * @verbatim
 * nua_set_params(nua;
 *    NUTAG_APPL_METHOD("REGISTER"),
 *    NUTAG_ALLOW("REGISTER"),
 *    TAG_END());
 * @endverbatim
 *
 * The nua_response() call responding to a REGISTER request must include
 * NUTAG_WITH() (or NUTAG_WITH_THIS()/NUTAG_WITH_SAVED()) tag. Note that
 * a successful response to REGISTER @b MUST include the @Contact header
 * bound to the the AoR URI (in @To header).
 *
 * The REGISTER request does not create a dialog. Currently the processing
 * of incoming REGISTER creates a new handle for each incoming request which
 * is not assiciated with an existing dialog. If the handle @a nh is not
 * bound, you should probably destroy it after responding to the REGISTER
 * request.
 *
 * @param status status code of response sent automatically by stack
 * @param phrase a short textual description of @a status code
 * @param nh     operation handle associated with the request
 * @param hmagic application context associated with the handle
 *               (usually NULL)
 * @param sip    incoming REGISTER request
 * @param tags   empty
 *
 * @sa nua_respond(), @RFC3261 section 10.3,
 * @Expires, @Contact, @CallID, @CSeq,
 * @Path, @RFC3327, @ServiceRoute, @RFC3608, @RFC3680,
 * nua_register(), #nua_i_register, nua_unregister(), #nua_i_unregister
 *
 * @since New in @VERSION_1_12_4
 * @END_NUA_EVENT
 */

nua_server_methods_t const nua_register_server_methods =
  {
    SIP_METHOD_REGISTER,
    nua_i_register,		/* Event */
    {
      0,			/* Do not create dialog */
      0,			/* Initial request */
      0,			/* Not a target refresh request  */
      0,			/* Do not add Contact */
    },
    nua_base_server_init,
    nua_base_server_preprocess,
    nua_base_server_params,
    nua_base_server_respond,
    nua_base_server_report,
  };
