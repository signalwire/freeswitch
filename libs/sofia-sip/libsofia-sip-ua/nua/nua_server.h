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

#ifndef NUA_SERVER_H
/** Defined when <nua_server.h> has been included. */
#define NUA_SERVER_H

/**@IFILE nua_server.h
 * @brief Dialog and dialog usage handling
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <Kai.Vehmanen@nokia.com>
 *
 * @date Created: Wed Mar  8 11:38:18 EET 2006  ppessi
 */

#include <nua_types.h>

#ifndef NTA_H
#include <sofia-sip/nta.h>
#endif

typedef struct {
  sip_method_t sm_method;
  char const *sm_method_name;

  int sm_event;

  struct {
    unsigned create_dialog:1, in_dialog:1, target_refresh:1, add_contact:1;
    unsigned :0;
  } sm_flags;

  /** Initialize server-side request. */
  int (*sm_init)(nua_server_request_t *sr);

  /** Preprocess server-side request (after handle has been created). */
  int (*sm_preprocess)(nua_server_request_t *sr);

  /** Update server-side request parameters */
  int (*sm_params)(nua_server_request_t *sr, tagi_t const *tags);

  /** Respond to server-side request. */
  int (*sm_respond)(nua_server_request_t *sr, tagi_t const *tags);

  /** Report server-side request to application. */
  int (*sm_report)(nua_server_request_t *sr, tagi_t const *tags);

} nua_server_methods_t;

/* Server side transaction */
struct nua_server_request {
  struct nua_server_request *sr_next, **sr_prev;

  nua_server_methods_t const *sr_methods;

  nua_owner_t *sr_owner;	/**< Backpointer to handle */
  nua_dialog_usage_t *sr_usage;	/**< Backpointer to usage */

  nta_incoming_t *sr_irq;	/**< Server transaction object */

  struct {
    msg_t *msg;			/**< Request message */
    sip_t const *sip;		/**< Headers in request message */
  } sr_request;

  struct {
    msg_t *msg;			/**< Response message */
    sip_t *sip;			/**< Headers in response message */
  } sr_response;

  sip_method_t sr_method;	/**< Request method */

  int sr_application;		/**< Status by application */

  int sr_status;		/**< Status code */
  char const *sr_phrase;	/**< Status phrase */

  unsigned sr_event:1;		/**< Reported to application */
  unsigned sr_initial:1;	/**< Handle was created by this request */
  unsigned sr_add_contact:1;	/**< Add Contact header to the response */
  unsigned sr_target_refresh:1;	/**< Refresh target */
  unsigned sr_terminating:1;	/**< Terminate usage after final response */
  unsigned sr_gracefully:1;	/**< Terminate usage gracefully */

  unsigned sr_neutral:1;	/**< No effect on session or other usage */

  /* Flags used with 100rel */
  unsigned sr_100rel:1, sr_pracked:1;

  /* Flags used with offer-answer */
  unsigned sr_offer_recv:1;	/**< We have received an offer */
  unsigned sr_answer_sent:2;	/**< We have answered (reliably, if >1) */

  unsigned sr_offer_sent:2;	/**< We have offered SDP (reliably, if >1) */
  unsigned sr_answer_recv:1;	/**< We have received SDP answer */

  unsigned :0;

  char const *sr_sdp;		/**< SDP received from client */
  size_t sr_sdp_len;		/**< SDP length */

  /**< Save 200 OK nua_respond() signal until PRACK has been received */
  nua_saved_signal_t sr_signal[1];
};

#define SR_STATUS(sr, status, phrase) \
  ((sr)->sr_phrase = (phrase), (sr)->sr_status = (status))

#define SR_STATUS1(sr, statusphrase)					\
  sr_status(sr, statusphrase)

#define SR_HAS_SAVED_SIGNAL(sr) ((sr)->sr_signal[0] != NULL)

su_inline
int sr_status(nua_server_request_t *sr, int status, char const *phrase)
{
  return (void)(sr->sr_phrase = phrase), (sr->sr_status = status);
}

extern nua_server_methods_t const
  nua_extension_server_methods,
  nua_invite_server_methods,	/**< INVITE */
  nua_bye_server_methods,	/**< BYE */
  nua_options_server_methods,	/**< OPTIONS */
  nua_register_server_methods,	/**< REGISTER */
  nua_info_server_methods,	/**< INFO */
  nua_prack_server_methods,	/**< PRACK */
  nua_update_server_methods,	/**< UPDATE */
  nua_message_server_methods,	/**< MESSAGE */
  nua_subscribe_server_methods, /**< SUBSCRIBE */
  nua_notify_server_methods,	/**< NOTIFY */
  nua_refer_server_methods,	/**< REFER */
  nua_publish_server_methods;	/**< PUBLISH */

/** Return true if we have not sent final response to request */
su_inline
int nua_server_request_is_pending(nua_server_request_t const *sr)
{
  return sr && sr->sr_response.msg;
}

su_inline
int nua_server_request_status(nua_server_request_t const *sr)
{
  return sr ? nta_incoming_status(sr->sr_irq) : 500;
}

void nua_server_request_destroy(nua_server_request_t *sr);

int nua_base_server_init(nua_server_request_t *sr);

#define nua_base_server_init NULL

int nua_base_server_preprocess(nua_server_request_t *sr);

#define nua_base_server_preprocess NULL

int nua_server_params(nua_server_request_t *sr, tagi_t const *tags);

int nua_base_server_params(nua_server_request_t *sr, tagi_t const *tags);

#define nua_base_server_params NULL

int nua_server_trespond(nua_server_request_t *sr,
			tag_type_t tag, tag_value_t value, ...);
int nua_server_respond(nua_server_request_t *sr, tagi_t const *tags);

int nua_base_server_trespond(nua_server_request_t *sr,
			     tag_type_t tag, tag_value_t value, ...);
int nua_base_server_respond(nua_server_request_t *sr,
			    tagi_t const *tags);

int nua_server_report(nua_server_request_t *sr);

int nua_base_server_treport(nua_server_request_t *sr,
			    tag_type_t tag, tag_value_t value, ...);
int nua_base_server_report(nua_server_request_t *sr, tagi_t const *tags);

#endif /* NUA_SERVER_H */
