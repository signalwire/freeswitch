/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
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

/**@CFILE nta_check.c
 * @brief Checks for features, MIME types, session timer.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Mar  8 16:35:05 EET 2006 ppessi
 */

#include "config.h"

#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sip_util.h>
#include <sofia-sip/nta.h>

/* ======================================================================== */
/* Request validation */

/**Check that we support all features which UAC requires.
 *
 * The list of supported features is compared with the list of features
 * required by the UAC. If some features are not listed as supported, return
 * 420. If @a irq is non-NULL, the 420 response message is sent to the
 * client along with list of unsupported features in the @Unsupported
 * header, too.
 *
 * @param   irq incoming transaction object (may be NULL).
 * @param   sip contents of the SIP message
 * @param supported   list of protocol features supported
 * @param tag, value, ... optional list of tagged arguments used
 *                        when responding to the transaction
 *
 * @return 0 if successful.
 * 420 if any of the required features is not supported.
 */
int nta_check_required(nta_incoming_t *irq,
		       sip_t const *sip,
		       sip_supported_t const *supported,
		       tag_type_t tag, tag_value_t value, ...)
{
  int status = 0;

  if (sip->sip_require) {
    su_home_t home[SU_HOME_AUTO_SIZE(512)];
    sip_unsupported_t *us;

    su_home_auto(home, sizeof home);

    us = sip_has_unsupported(home, supported, sip->sip_require);

    if (us) {
      status = 420;
      if (irq) {
	ta_list ta;
	ta_start(ta, tag, value);
	nta_incoming_treply(irq,
			    SIP_420_BAD_EXTENSION,
			    SIPTAG_UNSUPPORTED(us),
			    SIPTAG_SUPPORTED(supported),
			    ta_tags(ta));
	ta_end(ta);
      }
    }

    su_home_deinit(home);
  }

  return status;
}

/** Check that UAC supports all the required features.
 *
 * The list of required features is compared with the features supported by
 * the UAC. If some features are not supported, return 421. If @a irq is
 * non-NULL, the 421 response message is sent to the client, too.
 *
 * @param irq incoming transaction object (may be NULL).
 * @param sip contents of the SIP message
 * @param require   list of required protocol features
 * @param tag, value, ... optional list of tagged arguments used
 *                        when responding to the transaction
 *
 * @return 0 if successful.
 * 421 if any of the required features is not supported.
 */
int nta_check_supported(nta_incoming_t *irq,
			sip_t const *sip,
			sip_require_t *require,
			tag_type_t tag, tag_value_t value, ...)
{
  if (!sip_has_unsupported(NULL, sip->sip_supported, require))
    return 0;

  if (irq) {
    ta_list ta;
    ta_start(ta, tag, value);
    nta_incoming_treply(irq,
			SIP_421_EXTENSION_REQUIRED,
			SIPTAG_REQUIRE(require),
			ta_tags(ta));
    ta_end(ta);
  }

  return 421;
}

/** Check that we allow the request method.
 *
 * The request-method is compared with the list of supported methods in @a
 * allow. If match is found, 0 is is returned. Otherwise, if the
 * request-method is well-known, 405 is returned. If the request-method is
 * unknown, 501 is returned. If @a irq is non-NULL, the 405 or 501 response
 * message is sent to the client, too.
 *
 * @param irq 	incoming transaction object (may be NULL).
 * @param sip 	contents of the SIP message
 * @param allow   list of allowed methods
 * @param tag, value, ...   optional list of tagged arguments used
 *                          when responding to the transaction
 *
 * @return 0 if successful, 405 is request-method is not allowed, 501 if
 * request-method is unknown.
 */
int nta_check_method(nta_incoming_t *irq,
		     sip_t const *sip,
		     sip_allow_t const *allow,
		     tag_type_t tag, tag_value_t value, ...)
{
  /* Check extensions */
  sip_method_t method = sip->sip_request->rq_method;
  char const *name = sip->sip_request->rq_method_name;

  if (sip_is_allowed(allow, method, name))
    return 0;

  if (irq) {
    ta_list ta;
    ta_start(ta, tag, value);

    if (method != sip_method_unknown)
      /* Well-known method */
      nta_incoming_treply(irq,
			  SIP_405_METHOD_NOT_ALLOWED,
			  SIPTAG_ALLOW(allow),
			  ta_tags(ta));
    else
      /* Completeley unknown method */
      nta_incoming_treply(irq,
			  SIP_501_NOT_IMPLEMENTED,
			  SIPTAG_ALLOW(allow),
			  ta_tags(ta));
    ta_end(ta);
  }

  return method != sip_method_unknown ? 405 : 501;
}

static char const application_sdp[] = "application/sdp";

/** Check that we understand session content in the request.
 *
 * If there is no @ContentDisposition header or the @ContentDisposition
 * header indicated "session", the message body and content-type is compared
 * with the acceptable session content-types listed in @a session_accepts.
 * (typically, @c "application/sdp"). If no match is found, a 415 is
 * returned. If @a irq is non-NULL, the 415 response message is sent to the
 * client, too.
 *
 * If the @ContentDisposition header indicates something else but "session",
 * and it does not contain "handling=optional" parameter, a 415 response is
 * returned, too.
 *
 * Also, the @ContentEncoding header is checked. If it is not empty
 * (indicating no content-encoding), a 415 response is returned, too.
 *
 * @param irq 	incoming (server) transaction object (may be NULL).
 * @param sip 	contents of the SIP message
 * @param session_accepts   list of acceptable content-types for "session"
 *                          content disposition
 * @param tag, value, ...   optional list of tagged arguments used
 *                          when responding to the transaction
 *
 * @return 0 if successful, 415 if content-type is not acceptable.
 */
int nta_check_session_content(nta_incoming_t *irq,
			      sip_t const *sip,
			      sip_accept_t const *session_accepts,
			      tag_type_t tag, tag_value_t value, ...)
{
  sip_content_type_t const *c = sip->sip_content_type;
  sip_content_disposition_t const *cd = sip->sip_content_disposition;
  int acceptable_type = 0, acceptable_encoding = 0;

  if (sip->sip_payload == NULL)
    return 0;

  if (cd == NULL || su_casematch(cd->cd_type, "session")) {
    sip_accept_t const *ab = session_accepts;
    char const *c_type;

    if (c)
      c_type = c->c_type;
    else if (sip->sip_payload->pl_len > 3 &&
	     su_casenmatch(sip->sip_payload->pl_data, "v=0", 3))
      /* Missing Content-Type, but it looks like SDP  */
      c_type = application_sdp;
    else
      /* No chance */
      ab = NULL, c_type = NULL;

    for (; ab; ab = ab->ac_next) {
      if (su_casematch(c_type, ab->ac_type))
	break;
    }

    if (ab)
      acceptable_type = 1;
  }
  else if (cd->cd_optional)
    acceptable_type = 1;

  /* Empty or missing Content-Encoding */
  if (!sip->sip_content_encoding ||
      !sip->sip_content_encoding->k_items ||
      !sip->sip_content_encoding->k_items[0] ||
      !sip->sip_content_encoding->k_items[0][0] ||
	  !strcasecmp(sip->sip_content_encoding->k_items[0], "gzip") ||
	  !strcasecmp(sip->sip_content_encoding->k_items[0], "deflate"))
    acceptable_encoding = 1;

  if (acceptable_type && acceptable_encoding)
    return 0;

  if (irq) {
    ta_list ta;
    ta_start(ta, tag, value);
    nta_incoming_treply(irq,
			SIP_415_UNSUPPORTED_MEDIA,
			SIPTAG_ACCEPT(session_accepts),
			ta_tags(ta));
    ta_end(ta);
  }

  return 415;
}


/**Check that UAC accepts one of listed MIME content-types.
 *
 * The list of acceptable content-types are compared with the acceptable
 * content-types. If match is found, it is returned in @a return_acceptable.
 * If no match is found, a 406 is returned. If @a irq is non-NULL, the 406
 * response message is sent to the client, too.
 *
 * @param irq incoming transaction object (may be NULL).
 * @param sip contents of the SIP message
 * @param acceptable list of acceptable content types
 * @param return_acceptable optional return-value parameter for
 *                          matched content-type
 * @param tag, value, ... optional list of tagged arguments used
 *                        when responding to the transaction
 *
 * @return 406 if no content-type is acceptable by client, 0 if successful.
 */
int nta_check_accept(nta_incoming_t *irq,
		     sip_t const *sip,
		     sip_accept_t const *acceptable,
		     sip_accept_t const **return_acceptable,
		     tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  sip_accept_t const *ac, *ab;
  sip_method_t method;

  if (!acceptable)
    return 0;

  if (sip->sip_request)
    method = sip->sip_request->rq_method;
  else /* if (sip->sip_cseq) */
    method = sip->sip_cseq->cs_method;

  /* Missing Accept header implies support for SDP in INVITE and OPTIONS
   * (and PRACK and UPDATE?)
   */
  if (!sip->sip_accept && (method == sip_method_invite ||
			   method == sip_method_options ||
			   method == sip_method_prack ||
			   method == sip_method_update)) {
    for (ab = acceptable; ab; ab = ab->ac_next)
      if (su_casematch(application_sdp, ab->ac_type)) {
	if (return_acceptable) *return_acceptable = ab;
	return 0;
      }
  }

  for (ac = sip->sip_accept; ac; ac = ac->ac_next) {
    if (sip_q_value(ac->ac_q) == 0 || !ac->ac_type)
      continue;

    for (ab = acceptable; ab; ab = ab->ac_next)
      if (su_casematch(ac->ac_type, ab->ac_type)) {
	if (return_acceptable) *return_acceptable = ab;
	return 0;
      }
  }

  if (irq) {
    ta_start(ta, tag, value);
    nta_incoming_treply(irq,
			SIP_406_NOT_ACCEPTABLE,
			SIPTAG_ACCEPT(acceptable),
			ta_tags(ta));
    ta_end(ta);
  }

  return 406;
}

/**Check @SessionExpires header.
 *
 * If the proposed session-expiration time is smaller than @MinSE or our
 * minimal session expiration time, respond with 422 containing shortest
 * acceptable session expiration time in @MinSE header.
 *
 * @param irq 	incoming transaction object (may be NULL).
 * @param sip 	contents of the SIP message
 * @param my_min_se   minimal session expiration time in seconds
 * @param tag, value, ...   optional list of tagged arguments used
 *                          when responding to the transaction
 *
 * @return 422 if session expiration time is too small, 0 when successful.
 */
int nta_check_session_expires(nta_incoming_t *irq,
			      sip_t const *sip,
			      sip_time_t my_min_se,
			      tag_type_t tag, tag_value_t value, ...)
{
  unsigned long min_se = my_min_se;

  if (sip->sip_min_se && min_se < sip->sip_min_se->min_delta)
    min_se = sip->sip_min_se->min_delta;

  if (sip->sip_session_expires->x_delta >= min_se)
    return 0;

  if (irq) {
    ta_list ta;
    sip_min_se_t min_se0[1];

    ta_start(ta, tag, value);

    sip_min_se_init(min_se0)->min_delta = min_se;

    nta_incoming_treply(irq,
			SIP_422_SESSION_TIMER_TOO_SMALL,
			SIPTAG_MIN_SE(min_se0),
			ta_tags(ta));
    ta_end(ta);
  }

  return 422;
}
