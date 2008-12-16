#ifndef APPS_UTILS_H /** Defined when apps_utils.h has been included. */
#define APPS_UTILS_H

/**
 * @nofile apps_utils.h
 * @brief
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Written by Pekka Pessi <pekka -dot pessi -at- nokia -dot- com>
 *
 * @STARTLGPL@
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
 * @ENDLGPL@
 *
 * @date Created: Thu Apr  8 15:55:15 2004 ppessi
 *
 */

SOFIA_BEGIN_DECLS

static inline
int proxy_authenticate(context_t *c,
		       nta_outgoing_t *oreq,
		       sip_t const *sip,
		       nta_response_f response_function)
{
  if (sip && sip->sip_status->st_status == 407 &&
      sip->sip_proxy_authenticate &&
      c->c_proxy_auth_retries++ < 3 &&
      c->c_proxy && c->c_proxy->url_user && c->c_proxy->url_password) {
    url_t *u = c->c_proxy;
    msg_t *rmsg = nta_outgoing_getrequest(oreq);
    sip_t *rsip = sip_object(rmsg);

    if (auc_challenge(&c->c_proxy_auth, c->c_home, sip->sip_proxy_authenticate,
		      sip_proxy_authorization_class) >= 0
	&&
	auc_all_credentials(&c->c_proxy_auth, NULL, NULL,
			    u->url_user, u->url_password) > 0
	&&
	auc_authorization(&c->c_proxy_auth, rmsg, (msg_pub_t *)rsip,
			  rsip->sip_request->rq_method_name,
			  rsip->sip_request->rq_url,
			  rsip->sip_payload) > 0) {
      nta_outgoing_destroy(c->c_orq);
      sip_header_remove(rmsg, rsip, (sip_header_t *)rsip->sip_via);
      nta_msg_request_complete(rmsg, c->c_leg, 0, NULL, NULL);
      c->c_orq = nta_outgoing_tmcreate(c->c_agent, response_function, c, NULL,
				       rmsg, TAG_END());
      return 1;
    }
  }

  return 0;
}

static inline
int server_authenticate(context_t *c,
			nta_outgoing_t *oreq,
			sip_t const *sip,
			nta_response_f response_function)
{
  if (sip && sip->sip_status->st_status == 401 &&
      sip->sip_www_authenticate &&
      c->c_auth_retries++ < 3 &&
      c->c_password && c->c_username) {
    msg_t *rmsg = nta_outgoing_getrequest(oreq);
    sip_t *rsip = sip_object(rmsg);

    if (auc_challenge(&c->c_auth, c->c_home, sip->sip_www_authenticate,
		      sip_authorization_class) >= 0
	&&
	auc_all_credentials(&c->c_auth, NULL, NULL, c->c_username, c->c_password) > 0
	&&
	auc_authorization(&c->c_auth, rmsg, (msg_pub_t *)rsip,
			  rsip->sip_request->rq_method_name,
			  rsip->sip_request->rq_url,
			  rsip->sip_payload) > 0) {
      nta_outgoing_destroy(c->c_orq);
      sip_header_remove(rmsg, rsip, (sip_header_t *)rsip->sip_via);
      nta_msg_request_complete(rmsg, c->c_leg, 0, NULL, NULL);
      c->c_orq = nta_outgoing_tmcreate(c->c_agent, response_function, c, NULL,
				       rmsg, TAG_END());
      return 1;
    }
  }

  return 0;
}

static inline
int tag_from_header(nta_agent_t *nta,
		    su_home_t *home,
		    sip_from_t *f)
{
  char *env = getenv("SIPTAG");
  char *t = (void *)nta_agent_newtag(home, NULL, nta);
  int retval;

  if (env) {
    char *t1 = su_sprintf(home, "tag=%s-%s", env, t);
    su_free(home, t);
    t = t1;
  }

  retval = sip_from_tag(home, f, t);

  su_free(home, t);

  return retval;
}

SOFIA_END_DECLS

#endif /* !defined APPS_UTILS_H */
