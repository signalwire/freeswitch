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

/**@CFILE outbound.c
 * @brief Implementation of SIP NAT traversal and outbound
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <Kai.Vehmanen@nokia.com>
 * @author Martti Mela <Martti Mela@nokia.com>
 *
 * @date Created: Wed May 10 12:11:54 EEST 2006 ppessi
 */

#include "config.h"

#define NTA_OUTGOING_MAGIC_T struct outbound

#include "outbound.h"

#include <sofia-sip/hostdomain.h>
#include <sofia-sip/sip.h>
#include <sofia-sip/sip_protos.h>
#include <sofia-sip/sip_util.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/tport.h>
#include <sofia-sip/nta_tport.h>

#include <sofia-sip/su_md5.h>
#include <sofia-sip/su_uniqueid.h>
#include <sofia-sip/token64.h>

#define SU_LOG (nua_log)
#include <sofia-sip/su_debug.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

struct outbound {
  su_home_t ob_home[1];
  outbound_owner_vtable
  const *ob_oo;			/**< Callbacks */
  outbound_owner_t *ob_owner;	/**< Backpointer */

  su_root_t *ob_root;		/**< Root for timers and stuff */
  nta_agent_t *ob_nta;		/**< SIP transactions */

  char ob_cookie[32];		/**< Our magic cookie */

  struct outbound_prefs {
    unsigned interval;	/**< Default keepalive interval for datagram */
    unsigned stream_interval;	/**< Default keepalive interval for streams */
    unsigned gruuize:1;		/**< Establish a GRUU */
    unsigned outbound:1;	/**< Try to use outbound */
    unsigned natify:1;		/**< Try to detect NAT */
    signed okeepalive:2;	/**< Connection keepalive with OPTIONS */
    unsigned validate:1;	/**< Validate registration with OPTIONS */
    /* How to detect NAT binding or connect to outbound: */
    unsigned use_connect:1;	/**< Use HTTP connect */
    unsigned use_rport:1;	/**< Use received/rport */
    unsigned use_socks:1;	/**< Detect and use SOCKS V5 */
    unsigned use_upnp:1;	/**< Detect and use UPnP */
    unsigned use_stun:1;	/**< Detect and try to use STUN */
    unsigned :0;
  } ob_prefs;

  struct outbound_info {
    /* See enum outbound_feature: */
    /* 0 do not support, 1 - perhaps supports, 2 - supports, 3 - requires */
    unsigned gruu:2, outbound:2, pref:2;
  } ob_info;

  /** Source of Contact header */
  unsigned ob_by_stack:1;
  /** Self-generated contacts */
  unsigned ob_contacts:1;

  /* The registration state machine. */
  /** Initial REGISTER containing ob_rcontact has been sent */
  unsigned ob_registering:1;
  /** 2XX response to REGISTER containing ob_rcontact has been received */
  unsigned ob_registered:1;
  /** The registration has been validated:
   *  We have successfully sent OPTIONS to ourselves.
   */
  unsigned ob_validated:1;
  /** The registration has been validated once.
   *   We have successfully sent OPTIONS to ourselves, so do not give
   *   up if OPTIONS probe fails.
   */
  unsigned ob_once_validated:1;

  unsigned ob_proxy_override:1;	/**< Override stack default proxy */
  unsigned :0;

  url_string_t *ob_proxy;	/**< Outbound-specific proxy */
  char const *ob_instance;	/**< Our instance ID */
  int32_t ob_reg_id;		/**< Flow-id */
  sip_contact_t *ob_rcontact;	/**< Our contact */
  sip_contact_t *ob_dcontact;	/**< Contact for dialogs */
  sip_contact_t *ob_previous;	/**< Stale contact */
  sip_contact_t *ob_gruu;	/**< Contact added to requests */
  sip_via_t *ob_via;		/**< Via header used to generate contacts */

  sip_contact_t *ob_obp;	/**< Contacts from outbound proxy */

  char *ob_nat_detected;	/**< Our public address */
  char *ob_nat_port;		/**< Our public port number */

  void *ob_stun;		/**< Stun context */
  void *ob_upnp;		/**< UPnP context  */

  struct {
    char *sipstun;		/**< Stun server usable for keep-alives */
    unsigned interval;		/**< Interval. */
    su_timer_t *timer;		/**< Keep-alive timer */
    msg_t *msg;			/**< Keep-alive OPTIONS message */
    nta_outgoing_t *orq;	/**< Keep-alive OPTIONS transaction */
    auth_client_t *auc[1];	/**< Authenticator for OPTIONS */
    /** Progress of registration validation */
    unsigned validating:1, validated:1,:0;
  } ob_keepalive;		/**< Keepalive informatio */
};

static
int outbound_nat_detect(outbound_t *ob,
       			 sip_t const *request,
       			 sip_t const *response);

/** Return values for outbound_nat_detect(). */
enum {
  ob_nat_error = -1,		/* or anything below zero */
  ob_no_nat = 0,
  ob_nat_detected = 1,
  ob_nat_changed = 2
};

/* ---------------------------------------------------------------------- */

#define SIP_METHOD_UNKNOWN sip_method_unknown, NULL

/** Content-Type sent in OPTIONS probing connectivity */
char const * const outbound_content_type = "application/vnd.nokia-register-usage";

static
int outbound_check_for_nat(outbound_t *ob,
			   sip_t const *request,
			   sip_t const *response);

enum outbound_feature {
  outbound_feature_unsupported = 0,
  outbound_feature_unsure = 1,
  outbound_feature_supported = 2,
  outbound_feature_required = 3
};

static enum outbound_feature feature_level(sip_t const *sip,
					   char const *tag, int level);

static int outbound_contacts_from_via(outbound_t *ob,
				      sip_via_t const *via);

/* ---------------------------------------------------------------------- */

/** Create a new outbound object */
outbound_t *
outbound_new(outbound_owner_t *owner,
	     outbound_owner_vtable const *owner_methods,
	     su_root_t *root,
	     nta_agent_t *agent,
	     char const *instance)
{
  outbound_t *ob;

  if (!owner || !owner_methods || !root || !agent)
    return NULL;

  ob = su_home_clone((su_home_t *)owner, sizeof *ob);

  if (ob) {
    su_md5_t md5[1];
    uint8_t digest[SU_MD5_DIGEST_SIZE];
    su_guid_t guid[1];

    ob->ob_owner = owner;
    ob->ob_oo = owner_methods;
    ob->ob_root = root;
    ob->ob_nta = agent;

    if (instance)
      ob->ob_instance =
	su_sprintf(ob->ob_home, "+sip.instance=\"<%s>\"", instance);
    ob->ob_reg_id = 0;

    outbound_peer_info(ob, NULL);

    /* Generate a random cookie (used as Call-ID) for us */
    su_md5_init(md5);
    su_guid_generate(guid);
    if (instance)
      su_md5_update(md5, (void *)instance, strlen(instance));
    su_md5_update(md5, (void *)guid, sizeof guid);
    su_md5_digest(md5, digest);
    token64_e(ob->ob_cookie, sizeof ob->ob_cookie, digest, sizeof digest);

    if (instance && !ob->ob_instance)
      su_home_unref(ob->ob_home), ob = NULL;
  }

  return ob;
}

void outbound_unref(outbound_t *ob)
{
  if (ob->ob_keepalive.timer)
    su_timer_destroy(ob->ob_keepalive.timer), ob->ob_keepalive.timer = NULL;

  if (ob->ob_keepalive.orq)
    nta_outgoing_destroy(ob->ob_keepalive.orq), ob->ob_keepalive.orq = NULL;

  if (ob->ob_keepalive.msg)
    msg_destroy(ob->ob_keepalive.msg), ob->ob_keepalive.msg = NULL;

  su_home_unref(ob->ob_home);
}

#include <sofia-sip/bnf.h>

/** Set various outbound and nat-traversal related options. */
int outbound_set_options(outbound_t *ob,
			 char const *_options,
			 unsigned interval,
			 unsigned stream_interval)
{
  struct outbound_prefs prefs[1] = {{ 0 }};
  char *s, *options = su_strdup(NULL, _options);
  int invalid;

  prefs->interval = interval;
  prefs->stream_interval = stream_interval;

#define MATCH(v) (len == sizeof(#v) - 1 && su_casenmatch(#v, s, len))

  if (options) {
    for (s = options; s[0]; s++) if (s[0] == '-') s[0] = '_';
  }

  prefs->gruuize = 1;
  prefs->outbound = 0;
  prefs->natify = 1;
  prefs->okeepalive = -1;
  prefs->validate = 1;
  prefs->use_rport = 1;

  for (s = options; s && s[0]; ) {
    size_t len = span_token(s);
    int value = 1;

    if (len > 3 && su_casenmatch(s, "no_", 3))
      value = 0, s += 3, len -= 3;
    else if (len > 4 && su_casenmatch(s, "not_", 4))
      value = 0, s += 4, len -= 4;

    if (len == 0)
      break;
    else if (MATCH(gruuize)) prefs->gruuize = value;
    else if (MATCH(outbound)) prefs->outbound = value;
    else if (MATCH(natify)) prefs->natify = value;
    else if (MATCH(validate)) prefs->validate = value;
    else if (MATCH(options_keepalive)) prefs->okeepalive = value;
    else if (MATCH(use_connect)) prefs->use_connect = value;
    else if (MATCH(use_rport)) prefs->use_rport = value;
    else if (MATCH(use_socks)) prefs->use_socks = value;
    else if (MATCH(use_upnp)) prefs->use_upnp = value;
    else if (MATCH(use_stun)) prefs->use_stun = value;
    else
      SU_DEBUG_1(("outbound(%p): unknown option \"%.*s\"\n",
		  (void *)ob->ob_owner, (int)len, s));

    s += len;
    len = strspn(s, " \t\n\r,;");
    if (len == 0)
      break;
    s += len;
  }

  invalid = s && s[0];
  if (invalid)
    SU_DEBUG_1(("outbound(%p): invalid options \"%s\"\n",
		(void *)ob->ob_owner, options));
  su_free(NULL, options);
  if (invalid)
    return -1;

  if (prefs->natify &&
      !(prefs->outbound ||
	prefs->use_connect ||
	prefs->use_rport ||
	prefs->use_socks ||
	prefs->use_upnp ||
	prefs->use_stun)) {
    SU_DEBUG_1(("outbound(%p): no nat traversal method given\n",
		(void *)ob->ob_owner));
  }

  ob->ob_prefs = *prefs;
  ob->ob_reg_id = prefs->outbound ? 1 : 0;

  return 0;
}

/** Override stack default proxy for outbound */
int outbound_set_proxy(outbound_t *ob,
		       url_string_t *proxy)
{
  url_string_t *new_proxy = NULL, *old_proxy = ob->ob_proxy;

  if (proxy)
    new_proxy = (url_string_t *)url_as_string(ob->ob_home, proxy->us_url);

  if (proxy == NULL || new_proxy != NULL) {
    ob->ob_proxy_override = 1;
    ob->ob_proxy = new_proxy;
    su_free(ob->ob_home, old_proxy);
    return 0;
  }

  return -1;
}

/* ---------------------------------------------------------------------- */

/** Obtain contacts for REGISTER */
int outbound_get_contacts(outbound_t *ob,
			  sip_contact_t **return_current_contact,
			  sip_contact_t **return_previous_contact)
{
  if (ob) {
    if (ob->ob_contacts)
      *return_current_contact = ob->ob_rcontact;
    *return_previous_contact = ob->ob_previous;
  }
  return 0;
}

/** REGISTER request has been sent */
int outbound_start_registering(outbound_t *ob)
{
  if (ob)
    ob->ob_registering = 1;
  return 0;
}

/** Process response to REGISTER request */
int outbound_register_response(outbound_t *ob,
			       int terminating,
			       sip_t const *request,
			       sip_t const *response)
{
  int status, reregister;

  if (!ob)
    return 0;

  if (terminating) {
    ob->ob_registering = ob->ob_registered = 0;
    return 0;			/* Cleanup is done separately */
  }

  if (!response || !request)
    return 0;

  assert(request->sip_request); assert(response->sip_status);

  status = response->sip_status->st_status;

  if (status < 300) {
    if (request->sip_contact && response->sip_contact) {
      if (ob->ob_rcontact != NULL)
        msg_header_free(ob->ob_home, (msg_header_t *)ob->ob_rcontact);
      ob->ob_rcontact = sip_contact_dup(ob->ob_home, request->sip_contact);
      ob->ob_registered = ob->ob_registering;
    } else
      ob->ob_registered = 0;
  }

  reregister = outbound_check_for_nat(ob, request, response);
  if (reregister)
    return reregister;

  if (ob->ob_previous && status < 300) {
    msg_header_free(ob->ob_home, (void *)ob->ob_previous);
    ob->ob_previous = NULL;
  }

  return 0;
}


/** @internal Check if there is a NAT between us and registrar.
 *
 * @retval -1 upon an error
 * @retval #ob_register_ok (0) if the registration was OK
 * @retval #ob_reregister (1) if client needs to re-register
 * @retval #ob_reregister_now (2) if client needs to re-register immediately
 */
static
int outbound_check_for_nat(outbound_t *ob,
			   sip_t const *request,
			   sip_t const *response)
{
  int binding_changed;
  sip_contact_t *m = ob->ob_rcontact;

  /* Update NAT information */
  binding_changed = outbound_nat_detect(ob, request, response);

  if (!ob->ob_nat_detected)
    return ob_no_nat;

  /* Contact was set by application, do not change it */
  if (!ob->ob_by_stack)
    return ob_no_nat;

  /* Application does not want us to do any NAT traversal */
  if (!ob->ob_prefs.natify)
    return ob_no_nat;

  /* We have detected NAT. Now, what to do?
   * 1) do nothing - register as usual and let proxy take care of it?
   * 2) try to detect our public nat binding and use it
   * 2A) use public vias from nta generated by STUN or UPnP
   * 2B) use SIP Via header
   */

  /* Do we have to ask for reregistration */
  if (!m || binding_changed >= ob_nat_changed) {
    if (ob->ob_stun) {
      /* Use STUN? */
      return ob_reregister;
    }
    else if (ob->ob_upnp) {
      /* Use UPnP */
      return ob_reregister;
    }
    else {
      if (outbound_contacts_from_via(ob, response->sip_via) < 0)
        return -1;
    }

    return ob_reregister_now;
  }

  return 0;
}

/**@internal
 *
 * Detect NAT.
 *
 * Based on "received" and possible "rport" parameters in the top-most Via,
 * check and update our NAT status.
 *
 * @retval ob_nat_changed (2) change in public NAT binding detected
 * @retval ob_nat_detected (1) NAT binding detected
 * @retval ob_no_nat (0) no NAT binding detected
 * @retval -1 an error occurred
 */
static
int outbound_nat_detect(outbound_t *ob,
			sip_t const *request,
			sip_t const *response)
{
  sip_via_t const *v;
  int one = 1;
  char const *received, *rport;
  char *nat_detected, *nat_port;
  char *new_detected, *new_port;

  assert(request && request->sip_request);
  assert(response && response->sip_status);

  if (!ob || !response || !response->sip_via || !request->sip_via)
    return -1;

  v = response->sip_via;

  received = v->v_received;
  if (!received || !strcmp(received, request->sip_via->v_host))
    return 0;

  if (!host_is_ip_address(received)) {
    if (received[0])
      SU_DEBUG_3(("outbound(%p): Via with invalid received=%s\n",
		  (void *)ob->ob_owner, received));
    return 0;
  }

  rport = sip_via_port(v, &one); assert(rport);

  nat_detected = ob->ob_nat_detected;
  nat_port = ob->ob_nat_port;

  if (nat_detected && host_cmp(received, nat_detected) == 0) {
    if (nat_port && su_casematch(rport, nat_port))
      return 1;
    if (!v->v_rport || !v->v_rport[0])
      return 1;
  }

  if (!nat_detected) {
    SU_DEBUG_5(("outbound(%p): detected NAT: %s != %s\n",
		(void *)ob->ob_owner, v->v_host, received));
    if (ob->ob_oo && ob->ob_oo->oo_status)
      ob->ob_oo->oo_status(ob->ob_owner, ob, 101, "NAT detected", TAG_END());
  }
  else {
    SU_DEBUG_5(("outbound(%p): NAT binding changed: "
		"[%s]:%s != [%s]:%s\n",
		(void *)ob->ob_owner, nat_detected, nat_port, received, rport));
    if (ob->ob_oo && ob->ob_oo->oo_status)
      ob->ob_oo->oo_status(ob->ob_owner, ob, 102, "NAT binding changed", TAG_END());
  }

  /* Save our nat binding */
  new_detected = su_strdup(ob->ob_home, received);
  new_port = su_strdup(ob->ob_home, rport);

  if (!new_detected || !new_port) {
    su_free(ob->ob_home, new_detected);
    su_free(ob->ob_home, new_port);
    return -1;
  }

  ob->ob_nat_detected = new_detected;
  ob->ob_nat_port = new_port;

  su_free(ob->ob_home, nat_detected);
  su_free(ob->ob_home, nat_port);

  return 2;
}

/* ---------------------------------------------------------------------- */

/** Convert "gruu" parameter returned by registrar to Contact header. */
int outbound_gruuize(outbound_t *ob, sip_t const *sip)
{
  sip_contact_t *m = NULL;
  char *gruu;

  if (!ob)
    return 0;

  if (ob->ob_rcontact == NULL)
    return -1;

  if (!ob->ob_prefs.gruuize && ob->ob_instance) {
    char const *my_instance, *my_reg_id = NULL;
    char const *instance, *reg_id;

    m = ob->ob_rcontact;
    my_instance = msg_header_find_param(m->m_common, "+sip.instance=");
    if (my_instance)
      my_reg_id = msg_header_find_param(m->m_common, "reg-id=");

    for (m = sip->sip_contact; m; m = m->m_next) {
      if (my_instance) {
	instance = msg_header_find_param(m->m_common, "+sip.instance=");
	if (!instance || strcmp(instance, my_instance))
	  continue;
	if (my_reg_id) {
	  reg_id = msg_header_find_param(m->m_common, "reg-id=");
	  if (!reg_id || strcmp(reg_id, my_reg_id))
	    continue;
	}
      }

      if (url_cmp_all(ob->ob_rcontact->m_url, m->m_url) == 0)
	break;
    }
  }

  if (m == NULL) {
    if (ob->ob_gruu)
      msg_header_free(ob->ob_home, (void *)ob->ob_gruu), ob->ob_gruu = NULL;
    return 0;
  }

  gruu = (char *)msg_header_find_param(m->m_common, "pub-gruu=");

  if (gruu == NULL || gruu[0] == '\0')
    gruu = (char *)msg_header_find_param(m->m_common, "gruu=");

  if (gruu == NULL || gruu[0] == '\0')
    return 0;

  gruu = msg_unquote_dup(NULL, gruu);
  m = gruu ? sip_contact_format(ob->ob_home, "<%s>", gruu) : NULL;
  su_free(NULL, gruu);

  if (!m)
    return -1;

  if (ob->ob_gruu)
    msg_header_free(ob->ob_home, (void *)ob->ob_gruu);
  ob->ob_gruu = m;

  return 0;
}

/* ---------------------------------------------------------------------- */

static int create_keepalive_message(outbound_t *ob,
				    sip_t const *register_request);

static int keepalive_options(outbound_t *ob);
static int keepalive_options_with_registration_probe(outbound_t *ob);

static int response_to_keepalive_options(outbound_t *ob,
					 nta_outgoing_t *orq,
					 sip_t const *sip);
static int process_response_to_keepalive_options(outbound_t *ob,
						 nta_outgoing_t *orq,
						 sip_t const *sip,
						 int status,
						 char const *phrase);

static void keepalive_timer(su_root_magic_t *root_magic,
			    su_timer_t *t,
			    su_timer_arg_t *ob_as_timer_arg);

/** Start OPTIONS keepalive or contact validation process */
void outbound_start_keepalive(outbound_t *ob,
			      nta_outgoing_t *register_transaction)
{
  unsigned interval = 0;
  int need_to_validate, udp;

  if (!ob)
    return;

  udp = ob->ob_via && ob->ob_via->v_protocol == sip_transport_udp;

  if (/* ob->ob_prefs.natify && */
      /* On UDP, use OPTIONS keepalive by default */
      (udp ? ob->ob_prefs.okeepalive != 0
       /* Otherwise, only if requested */
       : ob->ob_prefs.okeepalive > 0))
    interval = ob->ob_prefs.interval;
  need_to_validate = ob->ob_prefs.validate && !ob->ob_validated;

  if (!register_transaction ||
      !(need_to_validate || interval != 0)) {
    outbound_stop_keepalive(ob);
    return;
  }

  if (ob->ob_keepalive.timer)
    su_timer_destroy(ob->ob_keepalive.timer), ob->ob_keepalive.timer = NULL;

  if (interval) {
    su_duration_t max_defer;

    max_defer = su_root_get_max_defer(ob->ob_root);
    if ((su_duration_t)interval >= max_defer) {
      interval -= max_defer - 100;
    }

    ob->ob_keepalive.timer =
      su_timer_create(su_root_task(ob->ob_root), interval);

    su_timer_deferrable(ob->ob_keepalive.timer, 1);
  }

  ob->ob_keepalive.interval = interval;

  if (!ob->ob_validated && ob->ob_keepalive.sipstun
      && 0 /* Stun is disabled for now */) {
    nta_tport_keepalive(register_transaction);
  }
  else {
    if (register_transaction) {
      msg_t *msg = nta_outgoing_getrequest(register_transaction);
      sip_t const *register_request = sip_object(msg);
      create_keepalive_message(ob, register_request);
      msg_destroy(msg);
    }

    keepalive_options(ob);
  }
}

void outbound_stop_keepalive(outbound_t *ob)
{
  if (!ob)
    return;

  ob->ob_keepalive.interval = 0;

  if (ob->ob_keepalive.timer)
    su_timer_destroy(ob->ob_keepalive.timer), ob->ob_keepalive.timer = NULL;

  if (ob->ob_keepalive.orq)
    nta_outgoing_destroy(ob->ob_keepalive.orq), ob->ob_keepalive.orq = NULL;

  if (ob->ob_keepalive.msg)
    msg_destroy(ob->ob_keepalive.msg), ob->ob_keepalive.msg = NULL;
}

/** @internal Create a message template for keepalive. */
static int create_keepalive_message(outbound_t *ob, sip_t const *regsip)
{
  msg_t *msg = nta_msg_create(ob->ob_nta, MSG_FLG_COMPACT), *previous;
  sip_t *osip = sip_object(msg);
  sip_contact_t *m = ob->ob_rcontact;

  unsigned d = ob->ob_keepalive.interval;

  if (msg == NULL)
    return -1;

  assert(regsip); assert(regsip->sip_request);

  if (m && m->m_params) {
    sip_accept_contact_t *ac;
    size_t i;
    int features = 0;

    ac = sip_accept_contact_make(msg_home(msg), "*;require;explicit");

    for (i = 0; m->m_params[i]; i++) {
      char const *s = m->m_params[i];
      if (!sip_is_callerpref(s))
	continue;
      features++;
      s = su_strdup(msg_home(msg), s);
      msg_header_add_param(msg_home(msg), ac->cp_common, s);
    }

    if (features)
      msg_header_insert(msg, NULL, (void *)ac);
    else
      msg_header_free(msg_home(msg), (void *)ac);
  }

  if (0 >
      /* Duplicate essential headers from REGISTER request: */
      sip_add_tl(msg, osip,
		 SIPTAG_TO(regsip->sip_to),
		 SIPTAG_FROM(regsip->sip_from),
		 /* XXX - we should only use loose routing here */
		 /* XXX - if we used strict routing,
		    the route header/request_uri must be restored
		 */
		 SIPTAG_ROUTE(regsip->sip_route),
		 /* Add Max-Forwards 0 */
		 TAG_IF(d, SIPTAG_MAX_FORWARDS_STR("0")),
		 TAG_IF(d, SIPTAG_SUBJECT_STR("KEEPALIVE")),
		 SIPTAG_CALL_ID_STR(ob->ob_cookie),
		 SIPTAG_ACCEPT_STR(outbound_content_type),
		 TAG_END()) ||
      /* Create request-line, Call-ID, CSeq */
      nta_msg_request_complete(msg,
       			nta_default_leg(ob->ob_nta),
       			SIP_METHOD_OPTIONS,
       			(void *)regsip->sip_to->a_url) < 0 ||
      msg_serialize(msg, (void *)osip) < 0 ||
      msg_prepare(msg) < 0)
    return msg_destroy(msg), -1;

  previous = ob->ob_keepalive.msg;
  ob->ob_keepalive.msg = msg;
  msg_destroy(previous);

  return 0;
}

static int keepalive_options(outbound_t *ob)
{
  msg_t *req;
  sip_t *sip;

  if (ob->ob_keepalive.orq)
    return 0;

  if (ob->ob_prefs.validate && ob->ob_registered && !ob->ob_validated)
    return keepalive_options_with_registration_probe(ob);

  req = msg_copy(ob->ob_keepalive.msg);
  if (!req)
    return -1;
  sip = sip_object(req); assert(sip); assert(sip->sip_request);

  if (nta_msg_request_complete(req, nta_default_leg(ob->ob_nta),
			       SIP_METHOD_UNKNOWN, NULL) < 0)
    return msg_destroy(req), -1;

  if (ob->ob_keepalive.auc[0])
    auc_authorization(ob->ob_keepalive.auc, req, (void *)sip,
		      "OPTIONS", sip->sip_request->rq_url, sip->sip_payload);

  ob->ob_keepalive.orq =
    nta_outgoing_mcreate(ob->ob_nta,
			 response_to_keepalive_options,
			 ob,
			 NULL,
			 req,
			 TAG_IF(ob->ob_proxy_override,
				NTATAG_DEFAULT_PROXY(ob->ob_proxy)),
			 TAG_END());

  if (!ob->ob_keepalive.orq)
    return msg_destroy(req), -1;

  return 0;
}

static int response_to_keepalive_options(outbound_t *ob,
					 nta_outgoing_t *orq,
					 sip_t const *sip)
{
  int status = 408;
  char const *phrase = sip_408_Request_timeout;

  if (sip && sip->sip_status) {
    status = sip->sip_status->st_status;
    phrase = sip->sip_status->st_phrase;
  }

  if (status == 100) {
    /* This probably means that we are in trouble. whattodo, whattodo */
  }

  if (status >= 200) {
    if (orq == ob->ob_keepalive.orq)
      ob->ob_keepalive.orq = NULL;
    process_response_to_keepalive_options(ob, orq, sip, status, phrase);
    nta_outgoing_destroy(orq);
  }

  return 0;
}

static int process_response_to_keepalive_options(outbound_t *ob,
						 nta_outgoing_t *orq,
						 sip_t const *sip,
						 int status,
						 char const *phrase)
{
  int binding_check;
  int challenged = 0, credentials = 0;
  msg_t *_reqmsg = nta_outgoing_getrequest(orq);
  sip_t *request = sip_object(_reqmsg); msg_destroy(_reqmsg);

  if (sip == NULL) {
    SU_DEBUG_3(("outbound(%p): keepalive %u %s\n", (void *)ob->ob_owner,
		status, phrase));
    ob->ob_oo->oo_keepalive_error(ob->ob_owner, ob, status, phrase, TAG_END());
    return 0;
  }

  if (status == 401 || status == 407) {
    if (sip->sip_www_authenticate)
      challenged += auc_challenge(ob->ob_keepalive.auc,
				  ob->ob_home,
				  sip->sip_www_authenticate,
				  sip_authorization_class) > 0;
    if (sip->sip_proxy_authenticate)
      challenged += auc_challenge(ob->ob_keepalive.auc,
				  ob->ob_home,
				  sip->sip_proxy_authenticate,
				  sip_proxy_authorization_class) > 0;
    if (ob->ob_oo->oo_credentials)
      credentials = ob->ob_oo->oo_credentials(ob->ob_owner,
					      ob->ob_keepalive.auc);
  }

  binding_check = outbound_nat_detect(ob, request, sip);

  if (binding_check > 1) {
    /* Bindings have changed */
    if (outbound_contacts_from_via(ob, sip->sip_via) == 0) {
      /* XXX - Destroy old keepalive template message */

      /* re-REGISTER */
      ob->ob_oo->oo_refresh(ob->ob_owner, ob);
      return 0;
    }
  }

  if (binding_check <= 1 && ob->ob_registered && ob->ob_keepalive.validating) {
    int failed = 0;
    unsigned loglevel = 3;

    if (challenged > 0 && credentials > 0) {
      keepalive_options_with_registration_probe(ob);
      return 0;
    }

    if (status < 300 && ob->ob_keepalive.validated) {
      loglevel = 5;
      if (ob->ob_validated)
	loglevel = 99;		/* only once */
      ob->ob_validated = ob->ob_once_validated = 1;
    }
    else if (status == 401 || status == 407 || status == 403)
      loglevel = 5, failed = 1;
    else
      loglevel = 3, failed = 1;

    if (loglevel >= SU_LOG->log_level) {
      sip_contact_t const *m = ob->ob_rcontact;

      if  (m)
	su_llog(SU_LOG, loglevel,
		"outbound(%p): %s <" URL_PRINT_FORMAT ">\n",
		(void *)ob->ob_owner,
		failed ? "FAILED to validate" : "validated",
		URL_PRINT_ARGS(m->m_url));
      else
	su_llog(SU_LOG, loglevel,
		"outbound(%p): %s registration\n",
		(void *)ob->ob_owner,
		failed ? "FAILED to validate" : "validated");

      if (failed)
	su_llog(SU_LOG, loglevel, "outbound(%p): FAILED with %u %s\n",
		(void *)ob->ob_owner, status, phrase);
    }

    if (failed)
      ob->ob_oo->oo_probe_error(ob->ob_owner, ob, status, phrase, TAG_END());
  }
  else if (status == 408) {
    SU_DEBUG_3(("outbound(%p): keepalive timeout\n", (void *)ob->ob_owner));
    ob->ob_oo->oo_keepalive_error(ob->ob_owner, ob, status, phrase, TAG_END());
    return 0;
  }

  ob->ob_keepalive.validating = 0;

  if (ob->ob_keepalive.timer)
    su_timer_set(ob->ob_keepalive.timer, keepalive_timer, ob);

  return 0;
}

static void keepalive_timer(su_root_magic_t *root_magic,
			    su_timer_t *t,
			    su_timer_arg_t *ob_casted_as_timer_arg)
{
  outbound_t *ob = (outbound_t *)ob_casted_as_timer_arg;

  (void)root_magic;

  if (keepalive_options(ob) < 0)
    su_timer_set(t, keepalive_timer, ob_casted_as_timer_arg);	/* XXX */
}


/** @internal Send a keepalive OPTIONS that probes the registration */
static int keepalive_options_with_registration_probe(outbound_t *ob)
{
  msg_t *req;
  sip_t *sip;
  void *request_uri;

  if (ob->ob_keepalive.orq)
    return 0;

  req = msg_copy(ob->ob_keepalive.msg);
  if (!req)
    return -1;

  sip = sip_object(req); assert(sip);
  request_uri = sip->sip_to->a_url;

  if (nta_msg_request_complete(req, nta_default_leg(ob->ob_nta),
       			SIP_METHOD_OPTIONS, request_uri) < 0)
    return msg_destroy(req), -1;

  if (ob->ob_keepalive.auc[0])
    auc_authorization(ob->ob_keepalive.auc, req, (void *)sip,
		      "OPTIONS", request_uri, sip->sip_payload);

  ob->ob_keepalive.orq =
    nta_outgoing_mcreate(ob->ob_nta,
			 response_to_keepalive_options,
			 ob,
			 NULL,
			 req,
			 TAG_IF(ob->ob_proxy_override,
				NTATAG_DEFAULT_PROXY(ob->ob_proxy)),
			 SIPTAG_SUBJECT_STR("REGISTRATION PROBE"),
			 /* NONE is used to remove
			    Max-Forwards: 0 found in ordinary keepalives */
			 SIPTAG_MAX_FORWARDS(SIP_NONE),
			 TAG_END());

  if (!ob->ob_keepalive.orq)
    return msg_destroy(req), -1;

  ob->ob_keepalive.validating = 1;
  ob->ob_keepalive.validated = 0;

  return 0;
}

/** Check if request should be processed by outbound */
int outbound_targeted_request(sip_t const *sip)
{
  return
    sip && sip->sip_request &&
    sip->sip_request->rq_method == sip_method_options &&
    sip->sip_accept &&
    sip->sip_accept->ac_type &&
    su_casematch(sip->sip_accept->ac_type, outbound_content_type);
}

/** Answer to the connectivity probe OPTIONS */
int outbound_process_request(outbound_t *ob,
			     nta_incoming_t *irq,
			     sip_t const *sip)
{
  /* XXX - We assume that Call-ID is not modified. */
  if (strcmp(sip->sip_call_id->i_id, ob->ob_cookie))
    return 0;

  if (ob->ob_keepalive.validating) {
    SU_DEBUG_5(("outbound(%p): registration check OPTIONS received\n",
		(void *)ob->ob_owner));
    ob->ob_keepalive.validated = 1;
  }

  nta_incoming_treply(irq, SIP_200_OK,
		      SIPTAG_CONTENT_TYPE_STR(outbound_content_type),
		      SIPTAG_PAYLOAD_STR(ob->ob_cookie),
		      TAG_END());
  return 200;
}


/* ---------------------------------------------------------------------- */

/**@internal
 * Create contacts for outbound.
 *
 * There are two contacts:
 * one suitable for registrations (ob_rcontact) and another that can be used
 * in dialogs (ob_dcontact).
 */
int outbound_contacts_from_via(outbound_t *ob, sip_via_t const *via)
{
  su_home_t *home = ob->ob_home;
  sip_contact_t *rcontact, *dcontact;
  char reg_id_param[20] = "";
  sip_contact_t *previous_previous, *previous_rcontact, *previous_dcontact;
  sip_via_t *v, v0[1], *previous_via;
  int contact_uri_changed;

  if (!via)
    return -1;

  v = v0; *v0 = *via; v0->v_next = NULL;

  dcontact = ob->ob_oo->oo_contact(ob->ob_owner, home, 1,
				   v, v->v_protocol, NULL);

  if (ob->ob_instance && ob->ob_reg_id != 0)
    snprintf(reg_id_param, sizeof reg_id_param, ";reg-id=%u", ob->ob_reg_id);

  rcontact = ob->ob_oo->oo_contact(ob->ob_owner, home, 0,
				   v, v->v_protocol,
				   ob->ob_instance, reg_id_param, NULL);

  v = sip_via_dup(home, v);

  if (!rcontact || !dcontact || !v) {
    msg_header_free(home, (void *)dcontact);
    if (rcontact != dcontact)
      msg_header_free(home, (void *)rcontact);
    msg_header_free(home, (void *)v);
    return -1;
  }

  contact_uri_changed = !ob->ob_rcontact ||
    url_cmp_all(ob->ob_rcontact->m_url, rcontact->m_url);

  if (contact_uri_changed) {
    previous_previous = ob->ob_previous;
    previous_dcontact = ob->ob_dcontact;
    previous_via = ob->ob_via;

    if (ob->ob_registered
        /* && (ob->ob_reg_id == 0 || ob->ob_info.outbound < outbound_feature_supported)
         * XXX - multiple connections not yet supported
	 */)
      previous_rcontact = NULL, ob->ob_previous = ob->ob_rcontact;
    else
      previous_rcontact = ob->ob_rcontact, ob->ob_previous = NULL;

    if (ob->ob_previous)
      msg_header_replace_param(home, (void*)ob->ob_previous, "expires=0");
  }
  else {
    previous_previous = ob->ob_rcontact;
    previous_rcontact = NULL;
    previous_dcontact = ob->ob_dcontact;
    previous_via = ob->ob_via;
  }

  ob->ob_contacts = 1;

  ob->ob_rcontact = rcontact;
  ob->ob_dcontact = dcontact;
  ob->ob_via = v;

  if (contact_uri_changed) {
    ob->ob_registering = 0;
    ob->ob_registered = 0;
    ob->ob_validated = 0;
  }

  msg_header_free(home, (void *)previous_rcontact);
  msg_header_free(home, (void *)previous_previous);
  if (previous_dcontact != ob->ob_previous &&
      previous_dcontact != previous_rcontact &&
      previous_dcontact != previous_previous)
    msg_header_free(home, (void *)previous_dcontact);
  msg_header_free(home, (void *)previous_via);

  return 0;
}

/**Set new contact.
 *
 * @retval 0 when successful
 * @retval -1 error setting contact
 */
int outbound_set_contact(outbound_t *ob,
			 sip_contact_t const *application_contact,
			 sip_via_t const *v,
			 int terminating)
{
  su_home_t *home = ob->ob_home;
  sip_contact_t *rcontact = NULL, *dcontact = NULL, *previous = NULL;
  sip_contact_t *m1, *m2, *m3;
  int contact_uri_changed = 0;

  m1 = ob->ob_rcontact;
  m2 = ob->ob_dcontact;
  m3 = ob->ob_previous;

  if (terminating) {
    if (ob->ob_by_stack && application_contact == NULL)
      return 0;

    if (ob->ob_contacts)
      previous = ob->ob_rcontact;
  }
  else if (application_contact) {
    rcontact = sip_contact_dup(home, application_contact);

    if (!ob->ob_rcontact ||
	url_cmp_all(ob->ob_rcontact->m_url, application_contact->m_url)) {
      contact_uri_changed = 1;
      previous = ob->ob_contacts ? ob->ob_rcontact : NULL;
    }
  }
  else if (ob->ob_by_stack) {
    return 0;    /* Xyzzy - nothing happens */
  }
  else if (v) {
    char const *tport = !v->v_next ? v->v_protocol : NULL;
    char reg_id_param[20];

    dcontact = ob->ob_oo->oo_contact(ob->ob_owner, home, 1,
				     v, tport, NULL);
    if (!dcontact)
      return -1;

    if (ob->ob_instance && ob->ob_reg_id != 0)
      snprintf(reg_id_param, sizeof reg_id_param, ";reg-id=%u", ob->ob_reg_id);

    rcontact = ob->ob_oo->oo_contact(ob->ob_owner, home, 0,
				     v, v->v_protocol,
				     ob->ob_instance, reg_id_param, NULL);
    if (!rcontact)
      return -1;

    if (!ob->ob_rcontact ||
	url_cmp_all(ob->ob_rcontact->m_url, rcontact->m_url)) {
      contact_uri_changed = 1;
      previous = ob->ob_contacts ? ob->ob_rcontact : NULL;
    }
  }

  ob->ob_by_stack = application_contact == NULL;

  ob->ob_contacts = rcontact != NULL;

  ob->ob_rcontact = rcontact;
  ob->ob_dcontact = dcontact;
  ob->ob_previous = previous;

  if (contact_uri_changed) {
    ob->ob_registering = 0;
    ob->ob_registered = 0;
    ob->ob_validated = 0;
    ob->ob_once_validated = 0;
  }

  if (m1 != previous)
    msg_header_free(home, (void *)m1);
  if (m2 != m1 && m2 != m3)
    msg_header_free(home, (void *)m2);
  msg_header_free(home, (void *)m3);

  return 0;
}

sip_contact_t const *outbound_dialog_contact(outbound_t const *ob)
{
  if (ob == NULL)
    return NULL;
  else if (ob->ob_gruu)
    return ob->ob_gruu;
  else
    return ob->ob_dcontact;
}

sip_contact_t const *outbound_dialog_gruu(outbound_t const *ob)
{
  return ob ? ob->ob_gruu : NULL;
}

/* ---------------------------------------------------------------------- */


static enum outbound_feature
feature_level(sip_t const *sip, char const *tag, int level)
{
  if (sip_has_feature(sip->sip_require, tag))
    return outbound_feature_required;
  else if (sip_has_feature(sip->sip_supported, tag))
    return outbound_feature_supported;
  else if (sip_has_feature(sip->sip_unsupported, tag))
    return outbound_feature_unsupported;
  else
    return (enum outbound_feature)level;
}


void outbound_peer_info(outbound_t *ob, sip_t const *sip)
{
  if (sip == NULL) {
    ob->ob_info.outbound = outbound_feature_unsure;
    ob->ob_info.gruu = outbound_feature_unsure;
    ob->ob_info.pref = outbound_feature_unsure;
    return;
  }

  ob->ob_info.outbound = feature_level(sip, "outbound", ob->ob_info.outbound);
  ob->ob_info.gruu = feature_level(sip, "gruu", ob->ob_info.gruu);
  ob->ob_info.pref = feature_level(sip, "pref", ob->ob_info.pref);
}
