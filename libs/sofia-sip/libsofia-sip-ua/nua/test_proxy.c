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

/**@CFILE test_proxy.c
 * @brief Extremely simple proxy and registrar for testing nua
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Nov  3 22:49:46 EET 2005
 */

#include "config.h"

#include <string.h>

struct proxy;
struct proxy_transaction;
struct registration_entry;
struct binding;

#define SU_ROOT_MAGIC_T struct proxy
#define NTA_LEG_MAGIC_T struct proxy
#define NTA_OUTGOING_MAGIC_T struct proxy_transaction
#define NTA_INCOMING_MAGIC_T struct proxy_transaction

#include <sofia-sip/su_wait.h>
#include <sofia-sip/nta.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sip_util.h>
#include <sofia-sip/auth_module.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/msg_addr.h>
#include <sofia-sip/hostdomain.h>

#include <stdlib.h>
#include <assert.h>

#define LIST_PROTOS(STORAGE, PREFIX, T)			 \
STORAGE void PREFIX ##_insert(T **list, T *node),	 \
        PREFIX ##_remove(T *node)			 

#define LIST_BODIES(STORAGE, PREFIX, T, NEXT, PREV)	  \
STORAGE void PREFIX ##_insert(T **list, T *node)   \
{							 \
  if ((node->NEXT = *list)) {				 \
    node->PREV = node->NEXT->PREV;			 \
    node->NEXT->PREV = &node->NEXT;			 \
  }							 \
  else							 \
    node->PREV = list;					 \
  *list = node;						 \
}							 \
STORAGE void PREFIX ##_remove(T *node)			 \
{							 \
  if (node->PREV)					 \
    if ((*node->PREV = node->NEXT))			 \
      node->NEXT->PREV = node->PREV;			 \
  node->PREV = NULL;					 \
}							 \
extern int LIST_DUMMY_VARIABLE

#include <test_proxy.h>

struct proxy {
  su_home_t    home[1];
  su_root_t   *parent;
  su_clone_r   clone;
  tagi_t      *tags;

  su_root_t   *root;
  auth_mod_t  *auth;
 
  nta_agent_t *agent;
  url_t const *uri;
  
  nta_leg_t *defleg;

  nta_leg_t *example_net;
  nta_leg_t *example_org;
  nta_leg_t *example_com;

  sip_contact_t *transport_contacts;

  struct proxy_transaction *stateless;
  struct proxy_transaction *transactions;
  struct registration_entry *entries;

  struct {
    sip_time_t min_expires, expires, max_expires;
    
    sip_time_t session_expires, min_se;
  } prefs;
}; 

LIST_PROTOS(static, registration_entry, struct registration_entry);
static struct registration_entry *registration_entry_new(struct proxy *,
							 url_t const *);
static void registration_entry_destroy(struct registration_entry *e);


struct registration_entry
{
  struct registration_entry *next, **prev;
  struct proxy *proxy;		/* backpointer */
  url_t *aor;			/* address-of-record */
  struct binding *bindings;	/* list of bindings */
  sip_contact_t *contacts;
};

struct binding
{
  struct binding *next, **prev;
  sip_contact_t *contact;	/* bindings */
  sip_time_t registered, expires; /* When registered and when expires */
  sip_call_id_t *call_id;	
  uint32_t cseq;
};

static struct binding *binding_new(su_home_t *home, 
				   sip_contact_t *contact,
				   sip_call_id_t *call_id,
				   uint32_t cseq,
				   sip_time_t registered, 
				   sip_time_t expires);
static void binding_destroy(su_home_t *home, struct binding *b);
static int binding_is_active(struct binding const *b)
{
  return b->expires > sip_now();
}

LIST_PROTOS(static, proxy_transaction, struct proxy_transaction);
struct proxy_transaction *proxy_transaction_new(struct proxy *);
static void proxy_transaction_destroy(struct proxy_transaction *t);

struct proxy_transaction
{
  struct proxy_transaction *next, **prev;

  struct proxy *proxy;		/* backpointer */
  sip_request_t *rq;		/* request line */
  nta_incoming_t *server;	/* server transaction */
  nta_outgoing_t *client;	/* client transaction */
};

static sip_contact_t *create_transport_contacts(struct proxy *p);

static int proxy_request(struct proxy *proxy,
			 nta_leg_t *leg,
			 nta_incoming_t *irq,
			 sip_t const *sip);

static int proxy_ack_cancel(struct proxy_transaction *t,
			    nta_incoming_t *irq,
			    sip_t const *sip);

static int proxy_response(struct proxy_transaction *t,
			  nta_outgoing_t *client,
			  sip_t const *sip);

static int process_register(struct proxy *proxy,
			    nta_incoming_t *irq,
			    sip_t const *sip);

static int domain_request(struct proxy *proxy,
			  nta_leg_t *leg,
			  nta_incoming_t *irq,
			  sip_t const *sip);

static int process_options(struct proxy *proxy,
			   nta_incoming_t *irq,
			   sip_t const *sip);

static struct registration_entry *
registration_entry_find(struct proxy const *proxy, url_t const *uri);

static auth_challenger_t registrar_challenger[1];
static auth_challenger_t proxy_challenger[1];

/* Proxy entry point */
static int 
test_proxy_init(su_root_t *root, struct proxy *proxy)
{
  struct proxy_transaction *t;

  auth_challenger_t _proxy_challenger[1] = 
  {{ 
      SIP_407_PROXY_AUTH_REQUIRED,
      sip_proxy_authenticate_class,
      sip_proxy_authentication_info_class
    }};

  auth_challenger_t _registrar_challenger[1] = 
  {{ 
      SIP_401_UNAUTHORIZED,
      sip_www_authenticate_class,
      sip_authentication_info_class
    }};

  *proxy_challenger = *_proxy_challenger;
  *registrar_challenger = *_registrar_challenger;

  proxy->root = root;

  proxy->auth = auth_mod_create(root, TAG_NEXT(proxy->tags));

  proxy->agent = nta_agent_create(root,
				  URL_STRING_MAKE("sip:0.0.0.0:*"),
				  NULL, NULL,
				  NTATAG_UA(0),
				  NTATAG_CANCEL_487(0),
				  NTATAG_SERVER_RPORT(1),
				  NTATAG_CLIENT_RPORT(1),
				  TAG_NEXT(proxy->tags));

  proxy->transport_contacts = create_transport_contacts(proxy);

  proxy->defleg = nta_leg_tcreate(proxy->agent,
				  proxy_request,
				  proxy,
				  NTATAG_NO_DIALOG(1),
				  TAG_END());

  proxy->example_net = nta_leg_tcreate(proxy->agent,
				       domain_request,
				       proxy,
				       NTATAG_NO_DIALOG(1),
				       URLTAG_URL("sip:example.net"),
				       TAG_END());
  proxy->example_org = nta_leg_tcreate(proxy->agent,
				       domain_request,
				       proxy,
				       NTATAG_NO_DIALOG(1),
				       URLTAG_URL("sip:example.org"),
				       TAG_END());
  proxy->example_com = nta_leg_tcreate(proxy->agent,
				       domain_request,
				       proxy,
				       NTATAG_NO_DIALOG(1),
				       URLTAG_URL("sip:example.com"),
				       TAG_END());

  proxy->prefs.min_expires = 30;
  proxy->prefs.expires = 3600;
  proxy->prefs.max_expires = 3600;

  proxy->prefs.session_expires = 180;
  proxy->prefs.min_se = 90;

  if (!proxy->defleg || 
      !proxy->example_net || !proxy->example_org || !proxy->example_com)
    return -1;

  t = su_zalloc(proxy->home, sizeof *t); 

  if (!t)
    return -1;

  proxy->stateless = t;
  t->proxy = proxy;
  t->server = nta_incoming_default(proxy->agent);
  t->client = nta_outgoing_default(proxy->agent, proxy_response, t);

  if (!t->client || !t->server)
    return -1;

  proxy->uri = nta_agent_contact(proxy->agent)->m_url;
				  
  return 0;
}

static void
test_proxy_deinit(su_root_t *root, struct proxy *proxy)
{
  struct proxy_transaction *t;
  
  auth_mod_destroy(proxy->auth);

  if ((t = proxy->stateless)) {
    nta_incoming_destroy(t->server), t->server = NULL;
    nta_outgoing_destroy(t->client), t->client = NULL;
  }

  nta_agent_destroy(proxy->agent);

  while (proxy->entries)
    registration_entry_destroy(proxy->entries);

  free(proxy->tags);
}

/* Create test proxy object */
struct proxy *test_proxy_create(su_root_t *root,
				tag_type_t tag, tag_value_t value, ...)
{
  struct proxy *p = su_home_new(sizeof *p);

  if (p) {
    ta_list ta;

    p->parent = root;

    ta_start(ta, tag, value);
    p->tags = tl_llist(ta_tags(ta));
    ta_end(ta);
    
    if (su_clone_start(root,
		       p->clone,
		       p,
		       test_proxy_init,
		       test_proxy_deinit) == -1)
      su_home_unref(p->home), p = NULL;
  }

  return p;
}

/* Destroy the proxy object */
void test_proxy_destroy(struct proxy *p)
{
  if (p) {
    su_clone_wait(p->parent, p->clone);
    su_home_unref(p->home);
  }
}

/* Return the proxy URI */
url_t const *test_proxy_uri(struct proxy const *p)
{
  return p ? p->uri : NULL;
}

void test_proxy_set_expiration(struct proxy *p,
			       sip_time_t min_expires, 
			       sip_time_t expires, 
			       sip_time_t max_expires)
{
  if (p) {
    p->prefs.min_expires = min_expires;
    p->prefs.expires = expires;
    p->prefs.max_expires = max_expires;
  }
}

void test_proxy_get_expiration(struct proxy *p,
			       sip_time_t *return_min_expires,
			       sip_time_t *return_expires,
			       sip_time_t *return_max_expires)
{
  if (p) {
    if (return_min_expires) *return_min_expires = p->prefs.min_expires;
    if (return_expires) *return_expires = p->prefs.expires;
    if (return_max_expires) *return_max_expires = p->prefs.max_expires;
  }
}

void test_proxy_set_session_timer(struct proxy *p,
				  sip_time_t session_expires, 
				  sip_time_t min_se)
{
  if (p) {
    p->prefs.session_expires = session_expires;
    p->prefs.min_se = min_se;
  }
}

void test_proxy_get_session_timer(struct proxy *p,
				  sip_time_t *return_session_expires,
				  sip_time_t *return_min_se)
{
  if (p) {
    if (return_session_expires)
      *return_session_expires = p->prefs.session_expires;
    if (return_min_se) *return_min_se = p->prefs.min_se;
  }
}

/* ---------------------------------------------------------------------- */

static sip_contact_t *create_transport_contacts(struct proxy *p)
{
  su_home_t *home = p->home;
  sip_via_t *v;
  sip_contact_t *retval = NULL, **mm = &retval;

  if (!p->agent)
    return NULL;

  for (v = nta_agent_via(p->agent); v; v = v->v_next) {
    char const *proto = v->v_protocol;

    if (v->v_next && 
	strcasecmp(v->v_host, v->v_next->v_host) == 0 &&
	str0cmp(v->v_port, v->v_next->v_port) == 0 &&
	((proto == sip_transport_udp &&
	  v->v_next->v_protocol == sip_transport_tcp) ||
	 (proto == sip_transport_tcp &&
	  v->v_next->v_protocol == sip_transport_udp)))
      /* We have udp/tcp pair, insert URL without tport parameter */
      *mm = sip_contact_create_from_via_with_transport(home, v, NULL, NULL);
    if (*mm) mm = &(*mm)->m_next;

    *mm = sip_contact_create_from_via_with_transport(home, v, NULL, proto);

    if (*mm) mm = &(*mm)->m_next;
  }

  return retval;
}

/* ---------------------------------------------------------------------- */

static int challenge_request(struct proxy *, nta_incoming_t *, sip_t const *);

/** Forward request */
static
int proxy_request(struct proxy *proxy,
		  nta_leg_t *leg,
		  nta_incoming_t *irq,
		  sip_t const *sip)
{
  url_t const *request_uri, *target;
  struct proxy_transaction *t = NULL;
  sip_request_t *rq = NULL;
  sip_max_forwards_t *mf;
  sip_method_t method = sip->sip_request->rq_method;

  sip_session_expires_t *x = NULL, x0[1];
  sip_min_se_t *min_se = NULL, min_se0[1];
  char const *require = NULL;

  mf = sip->sip_max_forwards;

  if (mf && mf->mf_count <= 1) {
    if (sip->sip_request->rq_method == sip_method_options) {
      return process_options(proxy, irq, sip);
    }
    nta_incoming_treply(irq, SIP_483_TOO_MANY_HOPS, TAG_END());
    return 483;
  }

  if (method != sip_method_ack && method != sip_method_cancel && 
      str0casecmp(sip->sip_from->a_url->url_host, "example.net") == 0) {
    /* Challenge everything but CANCEL and ACK coming from Mr. C */
    int status = challenge_request(proxy, irq, sip);
    if (status)
      return status;
  }

  if (method == sip_method_invite) {
    if (proxy->prefs.min_se) {
      if (!sip->sip_min_se || 
	  sip->sip_min_se->min_delta < proxy->prefs.min_se) {
	min_se = sip_min_se_init(min_se0);
	min_se->min_delta = proxy->prefs.min_se;
      }

      if (sip->sip_session_expires
	  && sip->sip_session_expires->x_delta < proxy->prefs.min_se
	  && sip_has_supported(sip->sip_supported, "timer")) {
	if (min_se == NULL)
	  min_se = sip->sip_min_se; assert(min_se);
	nta_incoming_treply(irq, SIP_422_SESSION_TIMER_TOO_SMALL,
			    SIPTAG_MIN_SE(min_se),
			    TAG_END());
	return 422;
      }
    }

    if (proxy->prefs.session_expires) {
      if (!sip->sip_session_expires ||
	  sip->sip_session_expires->x_delta > proxy->prefs.session_expires) {
	x = sip_session_expires_init(x0);
	x->x_delta = proxy->prefs.session_expires;
	if (!sip_has_supported(sip->sip_supported, "timer"))
	  require = "timer";
      }
    }
  }

  /* We don't do any route processing */
  request_uri = sip->sip_request->rq_url;

  if (!request_uri->url_host || 
      (strcasecmp(request_uri->url_host, "example.org") &&
       strcasecmp(request_uri->url_host, "example.net") &&
       strcasecmp(request_uri->url_host, "example.com"))) {
    target = request_uri;
  }
  else {
    struct registration_entry *e;
    struct binding *b;

    if (sip->sip_request->rq_method == sip_method_register) 
      return process_register(proxy, irq, sip);

    e = registration_entry_find(proxy, request_uri);
    if (e == NULL) {
      nta_incoming_treply(irq, SIP_404_NOT_FOUND, TAG_END());
      return 404;
    }

    for (b = e->bindings; b; b = b->next)
      if (binding_is_active(b))
	break;

    if (b == NULL) {
      nta_incoming_treply(irq, SIP_480_TEMPORARILY_UNAVAILABLE, TAG_END());
      return 480;
    }
    
    target = b->contact->m_url;
  }

  t = proxy_transaction_new(proxy);
  if (t == NULL) {
    nta_incoming_treply(irq, SIP_500_INTERNAL_SERVER_ERROR, TAG_END());
    return 500;
  }
  nta_incoming_bind(t->server = irq, proxy_ack_cancel, t);
  
  rq = sip_request_create(proxy->home,
			  sip->sip_request->rq_method,
			  sip->sip_request->rq_method_name,
			  (url_string_t *)target,
			  NULL);
  if (rq == NULL) {
    nta_incoming_treply(irq, SIP_500_INTERNAL_SERVER_ERROR, TAG_END());
    proxy_transaction_destroy(t);
    return 500;
  }
  t->rq = rq;

  /* Forward request */
  t->client = nta_outgoing_mcreate(proxy->agent, proxy_response, t, NULL,
				   nta_incoming_getrequest(irq),
				   /* rewrite request */
				   SIPTAG_REQUEST(rq),
				   SIPTAG_SESSION_EXPIRES(x),
				   SIPTAG_MIN_SE(min_se),
				   SIPTAG_REQUIRE_STR(require),
				   TAG_END());
  if (t->client == NULL) {
    proxy_transaction_destroy(t);
    nta_incoming_treply(irq, SIP_500_INTERNAL_SERVER_ERROR, TAG_END());
    return 500;
  }
  else if (sip->sip_request->rq_method == sip_method_ack)
    proxy_transaction_destroy(t);

  return 0;
}

static
int challenge_request(struct proxy *p,
		     nta_incoming_t *irq,
		     sip_t const *sip)
{
  int status;
  auth_status_t *as;
  msg_t *msg;

  as = auth_status_new(p->home);
  if (!as)
    return 500;

  as->as_method = sip->sip_request->rq_method_name;
  msg = nta_incoming_getrequest(irq);
  as->as_source = msg_addrinfo(msg);

  as->as_user_uri = sip->sip_from->a_url;
  as->as_display = sip->sip_from->a_display;

  if (sip->sip_payload)
    as->as_body = sip->sip_payload->pl_data,
      as->as_bodylen = sip->sip_payload->pl_len;

  auth_mod_check_client(p->auth, as, sip->sip_proxy_authorization,
			proxy_challenger);

  if ((status = as->as_status)) {
    nta_incoming_treply(irq,
			as->as_status, as->as_phrase,
			SIPTAG_HEADER((void *)as->as_info),
			SIPTAG_HEADER((void *)as->as_response),
			TAG_END());
  }
  else if (as->as_match) {
    msg_header_remove(msg, NULL, as->as_match);
  }

  msg_destroy(msg);
  su_home_unref(as->as_home);

  return status;
}		      

int proxy_ack_cancel(struct proxy_transaction *t,
		     nta_incoming_t *irq,
		     sip_t const *sip)
{
  if (sip == NULL) {
    proxy_transaction_destroy(t);
    return 0;
  }

  if (sip->sip_request->rq_method == sip_method_cancel) {
    /* We don't care about response to CANCEL (or ACK)
     * so we give NULL as callback pointer (and nta immediately 
     * destroys transaction object or marks it disposable)
     */
    if (nta_outgoing_tcancel(t->client, NULL, NULL, TAG_END()))
      return 200;
    else
      return 500;
  }
  else {
    return 500;
  }
}

int proxy_response(struct proxy_transaction *t,
		   nta_outgoing_t *client,
		   sip_t const *sip)
{
  int final;

  if (sip) {
    msg_t *response = nta_outgoing_getresponse(client);
    final = sip->sip_status->st_status >= 200;
    sip_via_remove(response, sip_object(response));
    nta_incoming_mreply(t->server, response);
  }
  else {
    final = 1;
    nta_incoming_treply(t->server, SIP_408_REQUEST_TIMEOUT, TAG_END());
  }

  if (final)
    proxy_transaction_destroy(t);

  return 0;
}

struct proxy_transaction *
proxy_transaction_new(struct proxy *proxy)
{
  struct proxy_transaction *t;

  t = su_zalloc(proxy->home, sizeof *t);
  if (t) {
    t->proxy = proxy;
    proxy_transaction_insert(&proxy->transactions, t);
  }
  return t;
}

static
void proxy_transaction_destroy(struct proxy_transaction *t)
{
  if (t == t->proxy->stateless)
    return;
  proxy_transaction_remove(t);
  nta_incoming_destroy(t->server);
  nta_outgoing_destroy(t->client);
  su_free(t->proxy->home, t->rq);
  su_free(t->proxy->home, t);
}

LIST_BODIES(static, proxy_transaction, struct proxy_transaction, next, prev);

/* ---------------------------------------------------------------------- */


static
int domain_request(struct proxy *proxy,
		   nta_leg_t *leg,
		   nta_incoming_t *irq,
		   sip_t const *sip)
{
  sip_method_t method = sip->sip_request->rq_method;

  if (method == sip_method_register)
    return process_register(proxy, irq, sip);

  if (method == sip_method_options) 
    return process_options(proxy, irq, sip);

  return 501;
}

static
int process_options(struct proxy *proxy,
		    nta_incoming_t *irq,
		    sip_t const *sip)
{
  nta_incoming_treply(irq, SIP_200_OK,
		      SIPTAG_CONTACT(proxy->transport_contacts),
		      TAG_END());
  return 200;
}

/* ---------------------------------------------------------------------- */


static int process_register2(struct proxy *p, auth_status_t *as,
			      nta_incoming_t *irq, sip_t const *sip);

static int set_status(auth_status_t *as, int status, char const *phrase);

static int validate_contacts(struct proxy *p, auth_status_t *as,
			     sip_t const *sip);
static int check_out_of_order(struct proxy *p, auth_status_t *as,
			      struct registration_entry *e, sip_t const *);
static int binding_update(struct proxy *p,
       		   auth_status_t *as,
       		   struct registration_entry *e,
       		   sip_t const *sip);

sip_contact_t *binding_contacts(su_home_t *home, struct binding *bindings);

int process_register(struct proxy *proxy,
		     nta_incoming_t *irq,
		     sip_t const *sip)
{
  auth_status_t *as;
  msg_t *msg;
  int status;

  as = auth_status_new(proxy->home);
  if (!as)
    return 500;

  as->as_method = sip->sip_request->rq_method_name;
  msg = nta_incoming_getrequest(irq);
  as->as_source = msg_addrinfo(msg);
  msg_destroy(msg);

  as->as_user_uri = sip->sip_from->a_url;
  as->as_display = sip->sip_from->a_display;

  if (sip->sip_payload)
    as->as_body = sip->sip_payload->pl_data,
      as->as_bodylen = sip->sip_payload->pl_len;

  process_register2(proxy, as, irq, sip);
  assert(as->as_status >= 200);

  nta_incoming_treply(irq,
       	       as->as_status, as->as_phrase,
       	       SIPTAG_HEADER((void *)as->as_info),
       	       SIPTAG_HEADER((void *)as->as_response),
       	       TAG_END());
  status = as->as_status;

  su_home_unref(as->as_home);

  return status;
}

static int process_register2(struct proxy *p,
			     auth_status_t *as,
			     nta_incoming_t *irq,
			     sip_t const *sip)
{
  struct registration_entry *e = NULL;
  sip_contact_t *m = sip->sip_contact;
  sip_via_t *v = sip->sip_via;

  if (m && v && v->v_received && m->m_url->url_host
      && strcasecmp(v->v_received, m->m_url->url_host) 
      && host_is_ip_address(m->m_url->url_host))
    return set_status(as, 406, "Unacceptable Contact");

  auth_mod_check_client(p->auth, as, sip->sip_authorization,
			registrar_challenger);
  if (as->as_status)
    return as->as_status;
  assert(as->as_response == NULL);

  if (validate_contacts(p, as, sip))
    return as->as_status;

  e = registration_entry_find(p, sip->sip_to->a_url);
  if (!sip->sip_contact) {
    as->as_response = (msg_header_t *)e->contacts;
    return set_status(as, SIP_200_OK);
  }

  if (e && check_out_of_order(p, as, e, sip))
    return as->as_status;
  
  if (!e) 
    e = registration_entry_new(p, sip->sip_to->a_url);
  if (!e)
    return set_status(as, SIP_500_INTERNAL_SERVER_ERROR);

  if (binding_update(p, as, e, sip))
    return as->as_status;

  msg_header_free(p->home, (void *)e->contacts);
  e->contacts = binding_contacts(p->home, e->bindings);

  as->as_response = (msg_header_t *)e->contacts;

  return set_status(as, SIP_200_OK);
}

static int set_status(auth_status_t *as, int status, char const *phrase)
{
  return as->as_phrase = phrase, as->as_status = status;
}

static int validate_contacts(struct proxy *p,
			     auth_status_t *as,
			     sip_t const *sip)
{
  sip_contact_t const *m;
  sip_time_t expires;
  sip_time_t now = sip_now();

  for (m = sip->sip_contact; m; m = m->m_next) {
    if (m->m_url->url_type == url_any) {
      if (!sip->sip_expires ||
	  sip->sip_expires->ex_delta || 
	  sip->sip_expires->ex_time ||
	  sip->sip_contact->m_next)
	return set_status(as, SIP_400_BAD_REQUEST);
      else
	return 0;
    }

    expires = sip_contact_expires(m, sip->sip_expires, sip->sip_date,
				  p->prefs.expires, now);
    
    if (expires > 0 && expires < p->prefs.min_expires) {
      as->as_response = (msg_header_t *)
	sip_min_expires_format(as->as_home, "%u", 
			       (unsigned)p->prefs.min_expires);
      return set_status(as, SIP_423_INTERVAL_TOO_BRIEF);
    }
  }

  return 0;
}

/** Check for out-of-order register request */
static
int check_out_of_order(struct proxy *p,
		       auth_status_t *as,
		       struct registration_entry *e,
		       sip_t const *sip)
{
  struct binding const *b;
  sip_call_id_t const *id;
  sip_contact_t *m;

  if (e == NULL || !sip->sip_contact)
    return 0;

  id = sip->sip_call_id;
  
  /* RFC 3261 subsection 10.3 step 6 and step 7 (p. 66): */
  /* Check for reordered register requests */
  for (b = e->bindings; b; b = b->next) {
    if (binding_is_active(b) &&
	strcmp(sip->sip_call_id->i_id, b->call_id->i_id) == 0 &&
	sip->sip_cseq->cs_seq <= b->cseq) {
      for (m = sip->sip_contact; m; m = m->m_next) {
	if (m->m_url->url_type == url_any ||
	    url_cmp_all(m->m_url, b->contact->m_url) == 0)
	  return set_status(as, SIP_500_INTERNAL_SERVER_ERROR);
      }
    }
  }

  return 0;
}


static struct registration_entry *
registration_entry_find(struct proxy const *proxy, url_t const *uri)
{
  struct registration_entry *e;

  /* Our routing table */
  for (e = proxy->entries; e; e = e->next) {
    if (url_cmp(uri, e->aor) == 0)
      return e;
  }
  return NULL;
}

static struct registration_entry *
registration_entry_new(struct proxy *proxy, url_t const *aor)
{
  struct registration_entry *e;

  e = su_zalloc(proxy->home, sizeof *e); 
  if (!e) 
    return NULL;

  e->proxy = proxy;
  e->aor = url_hdup(proxy->home, aor);
  if (!e->aor) {
    su_free(proxy->home, e);
    return NULL;
  }

  registration_entry_insert(&proxy->entries, e);

  return e;
}

static void
registration_entry_destroy(struct registration_entry *e)
{
  if (e) {
    registration_entry_remove(e);
    su_free(e->proxy->home, e->aor);
    while (e->bindings)
      binding_destroy(e->proxy->home, e->bindings);
    msg_header_free(e->proxy->home, (void *)e->contacts);
    su_free(e->proxy->home, e);
  }
}

LIST_BODIES(static, registration_entry, struct registration_entry, next, prev);

/* ---------------------------------------------------------------------- */
/* Bindings */

static
struct binding *binding_new(su_home_t *home, 
			    sip_contact_t *contact,
			    sip_call_id_t *call_id,
			    uint32_t cseq,
			    sip_time_t registered, 
			    sip_time_t expires)
{
  struct binding *b;
  
  b = su_zalloc(home, sizeof *b);

  if (b) {
    sip_contact_t m[1];
    *m = *contact; m->m_next = NULL;

    b->contact = sip_contact_dup(home, m);
    b->call_id = sip_call_id_dup(home, call_id);
    b->cseq = cseq;
    b->registered = registered;
    b->expires = expires;

    if (!b->contact || !b->call_id)
      binding_destroy(home, b), b = NULL;

    if (b)
      msg_header_remove_param(b->contact->m_common, "expires");
  }
  
  return b;
}

static
void binding_destroy(su_home_t *home, struct binding *b)
{
  if (b->prev) {
    if ((*b->prev = b->next))
      b->next->prev = b->prev;
  }
  msg_header_free(home, (void *)b->contact);
  msg_header_free(home, (void *)b->call_id);
  su_free(home, b);
}

static
int binding_update(struct proxy *p,
		   auth_status_t *as,
		   struct registration_entry *e,
		   sip_t const *sip)
{
  struct binding *b, *old, *next, *last, *bindings = NULL, **bb = &bindings;
  sip_contact_t *m;
  sip_time_t expires;

  sip_time_t now = sip_now();

  assert(sip->sip_contact);

  /* Create new bindings */
  for (m = sip->sip_contact; m; m = m->m_next) {
    if (m->m_url->url_type == url_any)
      break;
    
    expires = sip_contact_expires(m, sip->sip_expires, sip->sip_date,
				  p->prefs.expires, now);

    if (expires > p->prefs.max_expires)
      expires = p->prefs.max_expires;

    msg_header_remove_param(m->m_common, "expires");

    b = binding_new(p->home, m, sip->sip_call_id, sip->sip_cseq->cs_seq, 
		    now, now + expires);
    if (!b)
      break;

    *bb = b, b->prev = bb, bb = &b->next;
  }

  last = NULL;

  if (m == NULL) {
    /* Merge new bindings with old ones */
    for (old = e->bindings; old; old = next) {
      next = old->next;

      for (b = bindings; b != last; b = b->next) {
	if (url_cmp_all(old->contact->m_url, b->contact->m_url) != 0) 
	  continue;

	if (strcmp(old->call_id->i_id, b->call_id->i_id) == 0) {
	  b->registered = old->registered;
	}
	binding_destroy(p->home, old);
	break;
      }
    }

    for (bb = &e->bindings; *bb; bb = &(*bb)->next)
      ;

    if ((*bb = bindings))
      bindings->prev = bb;
  }
  else if (m->m_url->url_type == url_any) {
    /* Unregister all */
    for (b = e->bindings; b; b = b->next) {
      b->expires = now;
    }
  }
  else {
    /* Infernal error */

    for (old = bindings; old; old = next) {
      next = old->next;
      binding_destroy(p->home, old);
    }

    return set_status(as, SIP_500_INTERNAL_SERVER_ERROR);
  }

  return 0;
}

sip_contact_t *binding_contacts(su_home_t *home, struct binding *bindings)
{
  sip_contact_t *retval = NULL, **mm = &retval; 
  struct binding *b;
  sip_time_t now = sip_now();

  for (b = bindings; b; b = b->next) {
    char const *expires;
    if (b->expires <= now)
      continue;
    *mm = sip_contact_copy(home, b->contact);
    if (*mm) {
      expires = su_sprintf(home, "expires=%u", (unsigned)(b->expires - now));
      msg_header_add_param(home, (*mm)->m_common, expires);
      mm = &(*mm)->m_next;
    }
  }

  return retval;
}
