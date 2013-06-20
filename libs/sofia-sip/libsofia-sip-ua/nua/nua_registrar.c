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

#define TP_CLIENT_T struct nua_handle_s
#define TP_STACK_T struct nta_agent_s

#include <sofia-sip/su_string.h>
#include <sofia-sip/sip_protos.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sip_util.h>

#define NTA_INCOMING_MAGIC_T struct nua_handle_s
#define NTA_RELIABLE_MAGIC_T struct nua_handle_s

#include "nua_stack.h"

#include <sofia-sip/tport.h>
#include <sofia-sip/nta_tport.h>

/* ---------------------------------------------------------------------- */
/* Registrar usage */

struct registrar_usage
{
  tport_t *tport;		 /**<  */
  int pending;			 /**< Waiting for tport to close */
};

static char const *nua_registrar_usage_name(nua_dialog_usage_t const *du)
{
  return "registrar";
}

static int nua_registrar_usage_add(nua_handle_t *nh,
				   nua_dialog_state_t *ds,
				   nua_dialog_usage_t *du)
{
  return 0;
}

static void nua_registrar_usage_remove(nua_handle_t *nh,
				       nua_dialog_state_t *ds,
				       nua_dialog_usage_t *du,
				       nua_client_request_t *cr,
				       nua_server_request_t *sr)
{
  struct registrar_usage *ru;

  ru = nua_dialog_usage_private(du);

  if (ru->pending)
    tport_release(ru->tport, ru->pending, NULL, NULL, nh, 0), ru->pending = 0;

  tport_unref(ru->tport), ru->tport = NULL;
}

static void nua_registrar_usage_refresh(nua_handle_t *nh,
					nua_dialog_state_t *ds,
					nua_dialog_usage_t *du,
					sip_time_t now)
{
}

/** Terminate registration usage.
 *
 * @retval >0  shutdown done
 * @retval 0   shutdown in progress
 * @retval <0  try again later
 */
static int nua_registrar_usage_shutdown(nua_handle_t *nh,
					nua_dialog_state_t *ds,
					nua_dialog_usage_t *du)
{
  return 1;
}

static nua_usage_class const nua_registrar_usage[1] = {
  {
    sizeof (struct registrar_usage), sizeof nua_registrar_usage,
    nua_registrar_usage_add,
    nua_registrar_usage_remove,
    nua_registrar_usage_name,
    nua_base_usage_update_params,
    NULL,
    nua_registrar_usage_refresh,
    nua_registrar_usage_shutdown
  }};


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

static int nua_registrar_server_preprocess(nua_server_request_t *sr);
static int nua_registrar_server_report(nua_server_request_t *, tagi_t const *);

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
    nua_registrar_server_preprocess,
    nua_base_server_params,
    nua_base_server_respond,
    nua_registrar_server_report,
  };

static void
registrar_tport_error(nta_agent_t *nta, nua_handle_t *nh,
		      tport_t *tp, msg_t *msg, int error)
{
  nua_dialog_state_t *ds = nh->nh_ds;
  nua_dialog_usage_t *du;
  struct registrar_usage *ru;

  SU_DEBUG_3(("tport error %d: %s\n", error, su_strerror(error)));

  du = nua_dialog_usage_get(ds, nua_registrar_usage, NULL);

  if (du == NULL)
    return;

  ru = nua_dialog_usage_private(du);
  if (ru->tport) {
    tport_release(ru->tport, ru->pending, NULL, NULL, nh, 0), ru->pending = 0;
    tport_unref(ru->tport), ru->tport = NULL;
  }

  nua_stack_event(nh->nh_nua, nh, NULL,
		  nua_i_media_error, 500, "Transport error detected",
		  NULL);
}

static int
nua_registrar_server_preprocess(nua_server_request_t *sr)
{
  nua_handle_t *nh = sr->sr_owner;
  nua_dialog_state_t *ds = sr->sr_owner->nh_ds;
  nua_dialog_usage_t *du;
  struct registrar_usage *ru;
  tport_t *tport;

  tport = nta_incoming_transport(nh->nh_nua->nua_nta, sr->sr_irq, sr->sr_request.msg);

  if (!tport_is_tcp(tport)) {
	  tport_unref(tport);
	  return 0;
  }

  du = nua_dialog_usage_get(ds, nua_registrar_usage, NULL);
  if (du == NULL)
    du = nua_dialog_usage_add(nh, ds, nua_registrar_usage, NULL);

  if (du == NULL)
    return SR_STATUS1(sr, SIP_500_INTERNAL_SERVER_ERROR);

  ru = nua_dialog_usage_private(du);

  if (ru->tport && ru->tport != tport) {
    tport_release(ru->tport, ru->pending, NULL, NULL, nh, 0), ru->pending = 0;
    tport_unref(ru->tport), ru->tport = NULL;
  }

  ru->tport = tport;
  ru->pending = tport_pend(tport, NULL, registrar_tport_error, nh);

  tport_set_params(tport,
		   TPTAG_SDWN_ERROR(1),
		   TAG_END());

  return 0;
}

static int
nua_registrar_server_report(nua_server_request_t *sr, tagi_t const *tags)
{
  return nua_base_server_report(sr, tags);
}
