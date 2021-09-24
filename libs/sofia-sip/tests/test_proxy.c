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
struct domain;
union proxy_or_domain;
struct proxy_tr;
struct client_tr;
struct registration_entry;
struct binding;

#define SU_ROOT_MAGIC_T struct proxy
#define NTA_LEG_MAGIC_T union proxy_or_domain
#define NTA_OUTGOING_MAGIC_T struct client_tr
#define NTA_INCOMING_MAGIC_T struct proxy_tr
#define SU_TIMER_ARG_T struct proxy_tr

#include <sofia-sip/su_wait.h>
#include <sofia-sip/nta.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sip_util.h>
#include <sofia-sip/auth_module.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/msg_addr.h>
#include <sofia-sip/hostdomain.h>
#include <sofia-sip/tport.h>
#include <sofia-sip/nta_tport.h>

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

#include "test_proxy.h"
#include <sofia-sip/auth_module.h>

struct proxy {
  su_home_t    home[1];
  void        *magic;
  su_root_t   *parent;
  su_clone_r   clone;
  tagi_t      *tags;

  su_root_t   *root;

  nta_agent_t *agent;
  url_t const *uri;
  sip_route_t *lr;
  char const *lr_str;
  url_t const *rr_uri;

  nta_leg_t *defleg;

  sip_contact_t *transport_contacts;

  struct proxy_tr *stateless;
  struct proxy_tr *transactions;
  struct proxy_tr *invite_waiting;

  struct domain *domains;

  struct {
    unsigned t1x64;
    sip_time_t session_expires, min_se;
  } prefs;
};

struct domain {
  su_home_t home[1];
  void *magic;
  struct proxy *proxy;
  struct domain *next, **prev;

  url_t *uri;

  nta_leg_t *rleg, *uleg;

  auth_mod_t *auth;
  struct registration_entry *entries;

  struct {
    sip_time_t min_expires, expires, max_expires;
    int outbound_tcp;		/**< Use inbound TCP connection as outbound */
    char const *authorize;	/**< Authorization realm to use */
    int record_route;
  } prefs;

  tagi_t *tags;
};

LIST_PROTOS(static, domain, struct domain);
static int _domain_init(void *_d);
static int  domain_init(struct domain *domain);
static void domain_destroy(struct domain *domain);

LIST_BODIES(static, domain, struct domain, next, prev);

LIST_PROTOS(static, registration_entry, struct registration_entry);
static struct registration_entry *registration_entry_new(struct domain *,
							 url_t const *);
static void registration_entry_destroy(struct registration_entry *e);

struct registration_entry
{
  struct registration_entry *next, **prev;
  struct domain *domain;	/* backpointer */
  url_t *aor;			/* address-of-record */
  struct binding *bindings;	/* list of bindings */
  sip_contact_t *contacts;
};

struct binding
{
  struct binding *next, **prev;
  sip_contact_t *contact;	/* binding */
  sip_time_t registered, expires; /* When registered and when expires */
  sip_call_id_t *call_id;
  uint32_t cseq;
  tport_t *tport;		/**< Reference to tport */
};

static struct binding *binding_new(su_home_t *home,
				   sip_contact_t *contact,
				   tport_t *tport,
				   sip_call_id_t const *call_id,
				   uint32_t cseq,
				   sip_time_t registered,
				   sip_time_t expires);
static void binding_destroy(su_home_t *home, struct binding *b);
static int binding_is_active(struct binding const *b)
{
  return
    b->expires > sip_now() &&
    (b->tport == NULL || tport_is_clear_to_send(b->tport));
}

LIST_PROTOS(static, proxy_tr, struct proxy_tr);
struct proxy_tr *proxy_tr_new(struct proxy *);
static void proxy_tr_timeout(struct proxy_tr *t);
static void proxy_tr_destroy(struct proxy_tr *t);

struct proxy_tr
{
  struct proxy_tr *next, **prev;

  struct proxy *proxy;		/**< Backpointer to proxy */

  struct domain *origin;	/**< Originating domain */
  struct domain *domain;	/**< Destination domain */

  sip_time_t now;		/**< When received */

  nta_incoming_t *server;	/**< server transaction */
  msg_t *msg;			/**< request message */
  sip_t *sip;			/**< request headers */

  sip_method_t method;		/**< request method */
  char const *method_name;
  int status;			/**< best status */
  url_t *target;		/**< request-URI */

  struct client_tr *clients;	/**< Client transactions */

  struct registration_entry *entry;
				/**< Registration entry */

  auth_mod_t *am;		/**< Authentication module */
  auth_status_t *as;		/**< Authentication status */
  char const *realm;		/**< Authentication realm to use */
  unsigned use_auth;		/**< Authentication method (401/407) to use */

  su_timer_t *timer;		/**< Timer */

  unsigned rr:1;
};

LIST_PROTOS(static, client_tr, struct client_tr);

struct client_tr
{
  struct client_tr *next, **prev;
  struct proxy_tr *t;

  int status;			/* response status */
  sip_request_t *rq;		/* request line */
  msg_t *msg;			/* request message */
  sip_t *sip;			/* request headers */
  nta_outgoing_t *client;	/* transaction */
};

LIST_BODIES(static, client_tr, struct client_tr, next, prev);

static sip_contact_t *create_transport_contacts(struct proxy *p);

union proxy_or_domain { struct proxy proxy[1]; struct domain domain[1]; };

static int proxy_request(union proxy_or_domain *proxy,
			 nta_leg_t *leg,
			 nta_incoming_t *irq,
			 sip_t const *sip);

static int domain_request(union proxy_or_domain *domain,
			  nta_leg_t *leg,
			  nta_incoming_t *irq,
			  sip_t const *sip);

static int proxy_response(struct client_tr *client,
			   nta_outgoing_t *orq,
			   sip_t const *sip);

static int close_tports(void *proxy);

static auth_challenger_t registrar_challenger[1];
static auth_challenger_t proxy_challenger[1];

/* Proxy entry point */
static int
test_proxy_init(su_root_t *root, struct proxy *proxy)
{
  struct proxy_tr *t;
  struct client_tr *c;

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

  proxy->agent = nta_agent_create(root,
				  URL_STRING_MAKE("sip:0.0.0.0:*"),
				  NULL, NULL,
				  NTATAG_UA(0),
				  NTATAG_CANCEL_487(0),
				  NTATAG_SERVER_RPORT(1),
				  NTATAG_CLIENT_RPORT(1),
				  TAG_NEXT(proxy->tags));

  if (!proxy->agent)
    return -1;

  proxy->transport_contacts = create_transport_contacts(proxy);

  proxy->defleg = nta_leg_tcreate(proxy->agent,
				  proxy_request,
				  (union proxy_or_domain *)proxy,
				  NTATAG_NO_DIALOG(1),
				  TAG_END());

  proxy->prefs.session_expires = 180;
  proxy->prefs.min_se = 90;
  proxy->prefs.t1x64 = 64 * 500;

  nta_agent_get_params(proxy->agent,
		       NTATAG_SIP_T1X64_REF(proxy->prefs.t1x64),
		       TAG_END());

  if (!proxy->defleg)
    return -1;
  /* if (!proxy->example_net || !proxy->example_org || !proxy->example_com)
     return -1; */

  /* Create stateless client */
  t = su_zalloc(proxy->home, sizeof *t);
  c = su_zalloc(proxy->home, sizeof *c);

  if (!t || !c)
    return -1;

  proxy->stateless = t;
  t->proxy = proxy;
  c->t = t, client_tr_insert(&t->clients, c);
  t->server = nta_incoming_default(proxy->agent);
  c->client = nta_outgoing_default(proxy->agent, proxy_response, c);

  if (!c->client || !t->server)
    return -1;

  proxy->uri = nta_agent_contact(proxy->agent)->m_url;
  proxy->lr_str = su_sprintf(proxy->home, "<" URL_PRINT_FORMAT ";lr>", URL_PRINT_ARGS(proxy->uri));
  proxy->lr = sip_route_make(proxy->home, proxy->lr_str);

  if (!proxy->lr)
    return -1;

  return 0;
}

static void
test_proxy_deinit(su_root_t *root, struct proxy *proxy)
{
  struct proxy_tr *t;

  while (proxy->transactions)
    proxy_tr_destroy(proxy->transactions);

  if ((t = proxy->stateless)) {
    proxy->stateless = NULL;
    proxy_tr_destroy(t);
  }

  while (proxy->domains)
    domain_destroy(proxy->domains);

  nta_agent_destroy(proxy->agent);

  free(proxy->tags);
}

/* Create test proxy object */
struct proxy *test_proxy_create(su_root_t *root,
				tag_type_t tag, tag_value_t value, ...)
{
  struct proxy *p = su_home_new(sizeof *p);

  if (p) {
    ta_list ta;

    p->magic = test_proxy_create;

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

/* Return the proxy route URI */
char const *test_proxy_route_uri(struct proxy const *p,
				 sip_route_t const **return_route)
{
  if (p == NULL)
    return NULL;

  if (return_route)
    *return_route = p->lr;

  return p->lr_str;
}

struct _set_logging {
  struct proxy *p;
  int logging;
};

static int _set_logging(void *_a)
{
  struct _set_logging *a = _a;
  return nta_agent_set_params(a->p->agent, TPTAG_LOG(a->logging), TAG_END());
}

void test_proxy_set_logging(struct proxy *p, int logging)
{
  if (p) {
    struct _set_logging a[1] = {{ p, logging }};
    su_task_execute(su_clone_task(p->clone), _set_logging, a, NULL);
  }
}

void test_proxy_domain_set_expiration(struct domain *d,
				      sip_time_t min_expires,
				      sip_time_t expires,
				      sip_time_t max_expires)
{
  if (d) {
    d->prefs.min_expires = min_expires;
    d->prefs.expires = expires;
    d->prefs.max_expires = max_expires;
  }
}

void test_proxy_domain_get_expiration(struct domain *d,
				      sip_time_t *return_min_expires,
				      sip_time_t *return_expires,
				      sip_time_t *return_max_expires)
{
  if (d) {
    if (return_min_expires) *return_min_expires = d->prefs.min_expires;
    if (return_expires) *return_expires = d->prefs.expires;
    if (return_max_expires) *return_max_expires = d->prefs.max_expires;
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

void test_proxy_domain_set_outbound(struct domain *d,
				    int use_outbound)
{
  if (d) {
    d->prefs.outbound_tcp = use_outbound;
  }
}

void test_proxy_domain_get_outbound(struct domain *d,
				    int *return_use_outbound)
{
  if (d) {
    if (return_use_outbound)
      *return_use_outbound = d->prefs.outbound_tcp;
  }
}

void test_proxy_domain_set_record_route(struct domain *d,
					int use_record_route)
{
  if (d) {
    d->prefs.record_route = use_record_route;
  }
}

void test_proxy_domain_get_record_route(struct domain *d,
					int *return_use_record_route)
{
  if (d) {
    if (return_use_record_route)
      *return_use_record_route = d->prefs.record_route;
  }
}

int test_proxy_domain_set_authorize(struct domain *d,
				     char const *realm)
{
  if (d) {
    if (realm) {
      realm = su_strdup(d->home, realm);
      if (!realm)
	return -1;
    }

    d->prefs.authorize = realm;

    return 0;
  }
  return -1;
}

int test_proxy_domain_get_authorize(struct domain *d,
				     char const **return_realm)
{
  if (d) {
    if (return_realm) {
      *return_realm = d->prefs.authorize;
      return 0;
    }
  }
  return -1;
}

int test_proxy_close_tports(struct proxy *p)
{
  if (p) {
    int retval = -EPROTO;

    su_task_execute(su_clone_task(p->clone), close_tports, p, &retval);

    if (retval < 0)
      return errno = -retval, -1;
    else
      return 0;
  }
  return errno = EFAULT, -1;
}

/* ---------------------------------------------------------------------- */

struct domain *test_proxy_add_domain(struct proxy *p,
				     url_t const *uri,
				     tag_type_t tag, tag_value_t value, ...)
{
  struct domain *d;

  if (p == NULL || uri == NULL)
    return NULL;

  d = su_home_clone(p->home, sizeof *d);

  if (d) {
    ta_list ta;
    int init = 0;

    ta_start(ta, tag, value);

    d->magic = domain_init;

    d->proxy = p;
    d->uri = url_hdup(d->home, uri);
    d->tags = tl_adup(d->home, ta_args(ta));

    d->prefs.min_expires = 300;
    d->prefs.expires = 3600;
    d->prefs.max_expires = 36000;
    d->prefs.outbound_tcp = 0;
    d->prefs.authorize = NULL;

    if (d->uri && d->tags &&
	!su_task_execute(su_clone_task(p->clone), _domain_init, d, &init)) {
      if (init == 0)
	/* OK */;
      else
	d = NULL;
    }
    else
      su_home_unref(d->home);
  }

  return d;
}

static int _domain_init(void *_d)
{
  return domain_init(_d);
}

static int domain_init(struct domain *d)
{
  struct proxy *p = d->proxy;
  url_t uri[1];

  *uri = *d->uri;

  d->auth = auth_mod_create(p->root, TAG_NEXT(d->tags));

  /* Leg for URIs without userpart */
  d->rleg = nta_leg_tcreate(d->proxy->agent,
			    domain_request,
			    (union proxy_or_domain *)d,
			    NTATAG_NO_DIALOG(1),
			    URLTAG_URL(uri),
			    TAG_END());

  /* Leg for URIs with wildcard userpart */
  uri->url_user = "%";
  d->uleg = nta_leg_tcreate(d->proxy->agent,
			    domain_request,
			    (union proxy_or_domain *)d,
			    NTATAG_NO_DIALOG(1),
			    URLTAG_URL(uri),
			    TAG_END());

  if (d->auth && d->rleg && d->uleg) {
    domain_insert(&p->domains, d);
    return 0;
  }

  domain_destroy(d);

  return -1;
}

static void domain_destroy(struct domain *d)
{
  while (d->entries)
    registration_entry_destroy(d->entries);

  nta_leg_destroy(d->rleg), d->rleg = NULL;
  nta_leg_destroy(d->uleg), d->uleg = NULL;
  auth_mod_destroy(d->auth), d->auth = NULL;

  domain_remove(d);

  su_home_unref(d->home);
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
	su_casematch(v->v_host, v->v_next->v_host) &&
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

static int proxy_tr_with(struct proxy *proxy,
			 struct domain *domain,
			 nta_incoming_t *irq,
			 sip_t const *sip,
			 int (*process)(struct proxy_tr *));
static int proxy_transaction(struct proxy_tr *t);
static int respond_transaction(struct proxy_tr *t,
			       int status, char const *phrase,
			       tag_type_t tag, tag_value_t value,
			       ...);
static int validate_transaction(struct proxy_tr *t);
static int originating_transaction(struct proxy_tr *t);
static int challenge_transaction(struct proxy_tr *t);
static int session_timers(struct proxy_tr *t);
static int incoming_transaction(struct proxy_tr *t);
static int target_transaction(struct proxy_tr *t,
			      url_t const *target,
			      tport_t *tport);
static int process_register(struct proxy_tr *t);
static int process_options(struct proxy_tr *t);

static int proxy_ack_cancel(struct proxy_tr *t,
			    nta_incoming_t *irq,
			    sip_t const *sip);

static struct registration_entry *
registration_entry_find(struct domain const *domain, url_t const *uri);

static int proxy_request(union proxy_or_domain *pod,
			 nta_leg_t *leg,
			 nta_incoming_t *irq,
			 sip_t const *sip)
{
  assert(pod->proxy->magic = test_proxy_init);

  return proxy_tr_with(pod->proxy, NULL, irq, sip, proxy_transaction);
}

static int domain_request(union proxy_or_domain *pod,
			  nta_leg_t *leg,
			  nta_incoming_t *irq,
			  sip_t const *sip)
{
  int (*process)(struct proxy_tr *) = NULL;
  sip_method_t method = sip->sip_request->rq_method;

  assert(pod->domain->magic = domain_init);

  if (leg == pod->domain->uleg)
    process = proxy_transaction;
  else if (method == sip_method_register)
    process = process_register;
  else if (method == sip_method_options)
    process = process_options;

  if (process == NULL)
    return 501;			/* Not implemented */

  return proxy_tr_with(pod->domain->proxy, pod->domain, irq, sip, process);
}

static int proxy_tr_with(struct proxy *proxy,
			 struct domain *domain,
			 nta_incoming_t *irq,
			 sip_t const *sip,
			 int (*process)(struct proxy_tr *))
{
  struct proxy_tr *t = NULL;
  int status = 500;

  assert(proxy->magic = test_proxy_init);

  t = proxy_tr_new(proxy);
  if (t) {
    t->proxy = proxy, t->domain = domain, t->server = irq;
    t->msg = nta_incoming_getrequest(irq);
    t->sip = sip_object(t->msg);

    t->method = sip->sip_request->rq_method;
    t->method_name = sip->sip_request->rq_method_name;
    t->target = sip->sip_request->rq_url;
    t->now = nta_incoming_received(irq, NULL);

    if (t->method != sip_method_ack && t->method != sip_method_cancel)
      nta_incoming_bind(irq, proxy_ack_cancel, t);

    if (domain && domain->prefs.record_route)
      t->rr = 1;

    if (process(t) < 200)
      return 0;

    proxy_tr_destroy(t);
  }
  else {
    nta_incoming_treply(irq, SIP_500_INTERNAL_SERVER_ERROR, TAG_END());
  }

  return status;
}

/** Forward request */
static int proxy_transaction(struct proxy_tr *t)
{
  if (originating_transaction(t))
    return t->status;

  if (validate_transaction(t))
    return t->status;

  if (session_timers(t))
    return t->status;

  if (t->domain)
    return incoming_transaction(t);

  return target_transaction(t, t->target, NULL);
}

static int respond_transaction(struct proxy_tr *t,
			       int status, char const *phrase,
			       tag_type_t tag, tag_value_t value,
			       ...)
{
  ta_list ta;
  void *info = NULL, *response = NULL;

  ta_start(ta, tag, value);

  if (t->as)
    info = t->as->as_info, response = t->as->as_response;

  if (nta_incoming_treply(t->server, t->status = status, phrase,
			  SIPTAG_HEADER(info),
			  SIPTAG_HEADER(response),
			  ta_tags(ta)) < 0)
    t->status = status = 500;

  ta_end(ta);

  return status;
}

static int originating_transaction(struct proxy_tr *t)
{
  struct domain *o;
  char const *host;

  host = t->sip->sip_from->a_url->url_host;
  if (!host)
    return 0;

  for (o = t->proxy->domains; o; o = o->next)
    if (su_casematch(host, o->uri->url_host))
      break;

  t->origin = o;

  if (o && o->auth && o->prefs.authorize) {
    t->am = o->auth;
    t->realm = o->prefs.authorize;
    t->use_auth = 407;
  }

  if (o && o->prefs.record_route)
    t->rr = 1;

  return 0;
}

static int validate_transaction(struct proxy_tr *t)
{
  sip_max_forwards_t *mf;

  mf = t->sip->sip_max_forwards;

  if (mf && mf->mf_count <= 1) {
    if (t->method == sip_method_options)
      return process_options(t);

    return respond_transaction(t, SIP_483_TOO_MANY_HOPS, TAG_END());
  }

  /* Remove our routes */
  while (t->sip->sip_route &&
	 url_has_param(t->sip->sip_route->r_url, "lr") &&
	 (url_cmp(t->proxy->lr->r_url, t->sip->sip_route->r_url) == 0 ||
	  url_cmp(t->proxy->rr_uri, t->sip->sip_route->r_url) == 0)) {
    sip_route_remove(t->msg, t->sip);
    /* add record-route also to the forwarded request  */
  }

  if (t->use_auth)
    return challenge_transaction(t);

  return 0;
}

static int session_timers(struct proxy_tr *t)
{
  sip_t *sip = t->sip;
  sip_session_expires_t *x = NULL, x0[1];
  sip_min_se_t *min_se = NULL, min_se0[1];
  char const *require = NULL;

  if (t->method == sip_method_invite) {
    if (t->proxy->prefs.min_se) {
      if (!sip->sip_min_se ||
	  sip->sip_min_se->min_delta < t->proxy->prefs.min_se) {
	min_se = sip_min_se_init(min_se0);
	min_se->min_delta = t->proxy->prefs.min_se;
      }

      if (sip->sip_session_expires
	  && sip->sip_session_expires->x_delta < t->proxy->prefs.min_se
	  && sip_has_supported(sip->sip_supported, "timer")) {
	if (min_se == NULL)
	  min_se = sip->sip_min_se; assert(min_se);
	return respond_transaction(t, SIP_422_SESSION_TIMER_TOO_SMALL,
				   SIPTAG_MIN_SE(min_se),
				   TAG_END());
      }
    }

    if (t->proxy->prefs.session_expires) {
      if (!sip->sip_session_expires ||
	  sip->sip_session_expires->x_delta > t->proxy->prefs.session_expires) {
	x = sip_session_expires_init(x0);
	x->x_delta = t->proxy->prefs.session_expires;
	if (!sip_has_supported(sip->sip_supported, "timer"))
	  require = "timer";
      }
    }

    if (x || min_se || require)
      sip_add_tl(t->msg, t->sip,
		 SIPTAG_REQUIRE_STR(require),
		 SIPTAG_MIN_SE(min_se),
		 SIPTAG_SESSION_EXPIRES(x),
		 TAG_END());
  }

  return 0;
}

static int incoming_transaction(struct proxy_tr *t)
{
  struct registration_entry *e;
  struct binding *b;

#if 0
  if (sip->sip_request->rq_method == sip_method_register)
    return process_register(proxy, irq, sip);
#endif

  t->entry = e = registration_entry_find(t->domain, t->target);
  if (e == NULL)
    return respond_transaction(t, SIP_404_NOT_FOUND, TAG_END());

  for (b = e->bindings; b; b = b->next) {
    if (binding_is_active(b))
      target_transaction(t, b->contact->m_url, b->tport);

    if (t->clients)		/* XXX - enable forking */
      break;
  }

  if (t->clients != NULL)
    return 0;

  return respond_transaction(t, SIP_480_TEMPORARILY_UNAVAILABLE, TAG_END());
}

static int target_transaction(struct proxy_tr *t,
			      url_t const *target,
			      tport_t *tport)
{
  struct client_tr *c = su_zalloc(t->proxy->home, sizeof *c);
  int stateless = t->method == sip_method_ack;

  if (c == NULL)
    return 500;

  c->t = t;
  c->msg = msg_copy(t->msg);
  c->sip = sip_object(c->msg);

  if (c->msg)
    c->rq = sip_request_create(msg_home(c->msg),
			       t->method, t->method_name,
			       (url_string_t *)target,
			       NULL);

  msg_header_insert(c->msg, (msg_pub_t *)c->sip, (msg_header_t *)c->rq);

  if (t->rr) {
    sip_record_route_t rr[1];

    if (t->proxy->rr_uri) {
      *sip_record_route_init(rr)->r_url = *t->proxy->rr_uri;
      msg_header_add_dup(c->msg, (msg_pub_t *)c->sip, (msg_header_t *)rr);
    }
    else if (t->proxy->lr) {
      *sip_record_route_init(rr)->r_url = *t->proxy->lr->r_url;
      msg_header_add_dup(c->msg, (msg_pub_t *)c->sip, (msg_header_t *)rr);
    }
  }

  if (c->rq)
    /* Forward request */
    c->client = nta_outgoing_mcreate(t->proxy->agent,
				     proxy_response, c,
				     NULL,
				     msg_ref_create(c->msg),
				     NTATAG_TPORT(tport),
				     NTATAG_STATELESS(stateless),
				     TAG_END());

  if (!c->client) {
    msg_destroy(c->msg);
    su_free(t->proxy->home, c);
    return 500;
  }

  client_tr_insert(&t->clients, c);

  return stateless ? 200 : 0;
}

static int challenge_transaction(struct proxy_tr *t)
{
  auth_status_t *as;
  sip_t *sip = t->sip;

  assert(t->am);

  t->as = as = auth_status_new(t->proxy->home);
  if (!as)
    return respond_transaction(t, SIP_500_INTERNAL_SERVER_ERROR, TAG_END());

  as->as_method = sip->sip_request->rq_method_name;
  as->as_source = msg_addrinfo(t->msg);
  as->as_realm = t->realm;

  as->as_user_uri = sip->sip_from->a_url;
  as->as_display = sip->sip_from->a_display;

  if (sip->sip_payload)
    as->as_body = sip->sip_payload->pl_data,
      as->as_bodylen = sip->sip_payload->pl_len;

  if (t->use_auth == 401)
    auth_mod_check_client(t->am, as, sip->sip_authorization,
			  registrar_challenger);
  else
    auth_mod_check_client(t->am, as, sip->sip_proxy_authorization,
			  proxy_challenger);

  if (as->as_status)
    return respond_transaction(t, as->as_status, as->as_phrase, TAG_END());

  if (as->as_match)
    msg_header_remove(t->msg, (msg_pub_t *)sip, as->as_match);

  return 0;
}

int proxy_ack_cancel(struct proxy_tr *t,
		     nta_incoming_t *irq,
		     sip_t const *sip)
{
  struct client_tr *c;
  int status;

  if (sip == NULL) {		/* timeout */
    proxy_tr_destroy(t);
    return 0;
  }

  if (sip->sip_request->rq_method != sip_method_cancel)
    return 500;

  status = 200;

  for (c = t->clients; c; c = c->next) {
    if (c->client && c->status < 200)
      /*
       * We don't care about response to CANCEL (or ACK)
       * so we give NULL as callback pointer (and nta immediately
       * destroys transaction object or marks it disposable)
       */
      if (nta_outgoing_tcancel(c->client, NULL, NULL, TAG_END()) == NULL)
	status = 500;
  }

  return status;
}

int proxy_response(struct client_tr *c,
		   nta_outgoing_t *client,
		   sip_t const *sip)
{
  int final, timeout = 0;

  assert(c->t);

  if (sip) {
    msg_t *response = nta_outgoing_getresponse(client);
    if (c->t->method == sip_method_invite)
      final = sip->sip_status->st_status >= 300,
	timeout = sip->sip_status->st_status >= 200;
    else
      final = sip->sip_status->st_status >= 200;
    sip_via_remove(response, sip_object(response));
    nta_incoming_mreply(c->t->server, response);
  }
  else {
    int status = nta_outgoing_status(c->client);
    char const *phrase;

    if (status < 300 || status > 699)
      status = 500;
    phrase = sip_status_phrase(status);
    respond_transaction(c->t, status, phrase, TAG_END());
    final = 1;
  }

  if (final)
    proxy_tr_destroy(c->t);
  else if (timeout)
    proxy_tr_timeout(c->t);

  return 0;
}

int proxy_late_response(struct client_tr *c,
			nta_outgoing_t *client,
			sip_t const *sip)
{
  assert(c->t);

  if (sip &&
      sip->sip_status->st_status >= 200 &&
      sip->sip_status->st_status < 300) {
    msg_t *response = nta_outgoing_getresponse(client);
    sip_via_remove(response, sip_object(response));
    nta_incoming_mreply(c->t->server, response);
  }

  return 0;
}

static void proxy_tr_remove_late(su_root_magic_t *magic,
				 su_timer_t *timer,
				 struct proxy_tr *t)
{
  proxy_tr_destroy(t);
}


/** Proxy only late responses
 *
 * Keeping the invite transactions live
 */
static void proxy_tr_timeout(struct proxy_tr *t)
{
  struct client_tr *c;

  if (t == t->proxy->stateless)
    return;

  for (c = t->clients; c; c = c->next) {
    if (c->client && c->status < 300) {
      nta_outgoing_bind(c->client, proxy_late_response, c);
      if (c->status < 200) {
	nta_outgoing_tcancel(c->client, NULL, NULL, TAG_END());
      }
    }
  }

  t->timer = su_timer_create(su_root_task(t->proxy->root), t->proxy->prefs.t1x64);
  if (su_timer_set(t->timer, proxy_tr_remove_late, t) < 0) {
    proxy_tr_destroy(t);
  }
}

struct proxy_tr *
proxy_tr_new(struct proxy *proxy)
{
  struct proxy_tr *t;

  t = su_zalloc(proxy->home, sizeof *t);
  if (t) {
    t->proxy = proxy;
    proxy_tr_insert(&proxy->transactions, t);
  }
  return t;
}

static
void proxy_tr_destroy(struct proxy_tr *t)
{
  struct client_tr *c;

  if (t == t->proxy->stateless)
    return;

  proxy_tr_remove(t);

  if (t->as)
    su_home_unref(t->as->as_home), t->as = NULL;

  while (t->clients) {
    client_tr_remove(c = t->clients);
    nta_outgoing_destroy(c->client), c->client = NULL;
    msg_destroy(c->msg), c->msg = NULL;
    su_free(t->proxy->home, c);
  }

  su_timer_destroy(t->timer), t->timer = NULL;

  msg_destroy(t->msg);

  nta_incoming_destroy(t->server);

  su_free(t->proxy->home, t);
}

LIST_BODIES(static, proxy_tr, struct proxy_tr, next, prev);

/* ---------------------------------------------------------------------- */

static int process_options(struct proxy_tr *t)
{
  return respond_transaction(t, SIP_200_OK,
			     SIPTAG_CONTACT(t->proxy->transport_contacts),
			     TAG_END());
}

/* ---------------------------------------------------------------------- */

static int check_received_contact(struct proxy_tr *t);
static int validate_contacts(struct proxy_tr *t);
static int check_out_of_order(struct proxy_tr *t);
static int update_bindings(struct proxy_tr *t);

int process_register(struct proxy_tr *t)
{
  /* This is before authentication because we want to be bug-compatible */
  if (check_received_contact(t))
    return t->status;

  if (t->domain->auth) {
    t->am = t->domain->auth, t->use_auth = 401;
    if (t->domain->prefs.authorize)
      t->realm = t->domain->prefs.authorize;
    if (challenge_transaction(t))
      return t->status;
  }

  if (validate_contacts(t))
    return t->status;

  t->entry = registration_entry_find(t->domain, t->sip->sip_to->a_url);

  if (check_out_of_order(t))
    return t->status;

  return update_bindings(t);
}

static int check_received_contact(struct proxy_tr *t)
{
  sip_t *sip = t->sip;
  sip_contact_t *m = sip->sip_contact;
  sip_via_t *v = sip->sip_via;

  if (m && v && v->v_received && m->m_url->url_host
      && !su_casematch(v->v_received, m->m_url->url_host)
      && host_is_ip_address(m->m_url->url_host))
    return respond_transaction(t, 406, "Unacceptable Contact", TAG_END());

  return 0;
}

/* Validate expiration times */
static int validate_contacts(struct proxy_tr *t)
{
  sip_contact_t const *m = t->sip->sip_contact;
  sip_expires_t const *ex = t->sip->sip_expires;
  sip_date_t const *date = t->sip->sip_date;
  sip_time_t expires;

  if (m && m->m_url->url_type == url_any) {
    if (!ex || ex->ex_delta || ex->ex_time || m->m_next)
      return respond_transaction(t, SIP_400_BAD_REQUEST, TAG_END());
    return 0;
  }

  for (; m; m = m->m_next) {
    expires = sip_contact_expires(m, ex, date, t->domain->prefs.expires, t->now);

    if (expires > 0 && expires < t->domain->prefs.min_expires) {
      sip_min_expires_t me[1];

      sip_min_expires_init(me)->me_delta = t->domain->prefs.min_expires;

      return respond_transaction(t, SIP_423_INTERVAL_TOO_BRIEF,
				 SIPTAG_MIN_EXPIRES(me),
				 TAG_END());
    }
  }

  return 0;
}

/** Check for out-of-order register request */
static int check_out_of_order(struct proxy_tr *t)
{
  struct binding const *b;
  sip_call_id_t const *id = t->sip->sip_call_id;
  uint32_t cseq = t->sip->sip_cseq->cs_seq;
  sip_contact_t *m;

  if (t->entry == NULL || !t->sip->sip_contact)
    return 0;

  /* RFC 3261 subsection 10.3 step 6 and step 7 (p. 66): */
  /* Check for reordered register requests */
  for (b = t->entry->bindings; b; b = b->next) {
    if (binding_is_active(b) &&
	strcmp(id->i_id, b->call_id->i_id) == 0 &&
	cseq <= b->cseq) {
      for (m = t->sip->sip_contact; m; m = m->m_next) {
	if (m->m_url->url_type == url_any ||
	    url_cmp_all(m->m_url, b->contact->m_url) == 0)
	  return respond_transaction(t, SIP_500_INTERNAL_SERVER_ERROR,
				     TAG_END());
      }
    }
  }

  return 0;
}

static struct registration_entry *
registration_entry_find(struct domain const *d, url_t const *uri)
{
  struct registration_entry *e;

  /* Our routing table */
  for (e = d->entries; e; e = e->next) {
    if (url_cmp(uri, e->aor) == 0)
      return e;
  }

  return NULL;
}

static struct registration_entry *
registration_entry_new(struct domain *d, url_t const *aor)
{
  struct registration_entry *e;

  if (d == NULL)
    return NULL;

  e = su_zalloc(d->home, sizeof *e);
  if (!e)
    return NULL;

  e->domain = d;
  e->aor = url_hdup(d->home, aor);
  if (!e->aor) {
    su_free(d->home, e);
    return NULL;
  }

  registration_entry_insert(&d->entries, e);

  return e;
}

static void
registration_entry_destroy(struct registration_entry *e)
{
  if (e) {
    registration_entry_remove(e);
    su_free(e->domain->home, e->aor);
    while (e->bindings)
      binding_destroy(e->domain->home, e->bindings);
    msg_header_free(e->domain->home, (void *)e->contacts);
    su_free(e->domain->home, e);
  }
}

sip_contact_t *entry_contacts(struct registration_entry *entry)
{
  return entry ? entry->contacts : NULL;
}

LIST_BODIES(static, registration_entry, struct registration_entry, next, prev);

/* ---------------------------------------------------------------------- */
/* Bindings */

static
struct binding *binding_new(su_home_t *home,
			    sip_contact_t *contact,
			    tport_t *tport,
			    sip_call_id_t const *call_id,
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
    b->tport = tport_ref(tport);
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
  tport_unref(b->tport);
  su_free(home, b);
}

static int update_bindings(struct proxy_tr *t)
{
  struct domain *d = t->domain;
  struct binding *b, *old, *next, *last, *bindings = NULL, **bb = &bindings;
  sip_contact_t *m;
  sip_call_id_t const *id = t->sip->sip_call_id;
  uint32_t cseq = t->sip->sip_cseq->cs_seq;
  sip_expires_t *ex = t->sip->sip_expires;
  sip_date_t *date = t->sip->sip_date;
  sip_time_t expires;
  tport_t *tport = NULL;
  sip_contact_t *contacts = NULL, **mm = &contacts;
  void *tbf;

  if (t->sip->sip_contact == NULL) {
    if (t->entry)
      contacts = t->entry->contacts;
    goto ok200;
  }

  if (t->entry == NULL)
    t->entry = registration_entry_new(d, t->sip->sip_to->a_url);
  if (t->entry == NULL)
    return respond_transaction(t, SIP_500_INTERNAL_SERVER_ERROR, TAG_END());

  if (d->prefs.outbound_tcp &&
      str0casecmp(t->sip->sip_via->v_protocol, sip_transport_tcp) == 0)
    tport = nta_incoming_transport(t->proxy->agent, t->server, NULL);

  /* Create new bindings */
  for (m = t->sip->sip_contact; m; m = m->m_next) {
    if (m->m_url->url_type == url_any)
      break;

    expires = sip_contact_expires(m, ex, date, d->prefs.expires, t->now);

    if (expires > d->prefs.max_expires)
      expires = d->prefs.max_expires;

    msg_header_remove_param(m->m_common, "expires");

    b = binding_new(d->home, m, tport, id, cseq, t->now, t->now + expires);
    if (!b)
      break;

    *bb = b, b->prev = bb, bb = &b->next;
  }

  tport_unref(tport);

  last = NULL;

  if (m == NULL) {
    /* Merge new bindings with old ones */
    for (old = t->entry->bindings; old; old = next) {
      next = old->next;

      for (b = bindings; b != last; b = b->next) {
	if (url_cmp_all(old->contact->m_url, b->contact->m_url) != 0)
	  continue;

	if (strcmp(old->call_id->i_id, b->call_id->i_id) == 0) {
	  b->registered = old->registered;
	}
	binding_destroy(d->home, old);
	break;
      }
    }

    for (bb = &t->entry->bindings; *bb; bb = &(*bb)->next)
      ;

    if ((*bb = bindings))
      bindings->prev = bb;
  }
  else if (m->m_url->url_type == url_any) {
    /* Unregister all */
    for (b = t->entry->bindings; b; b = b->next) {
      b->expires = t->now;
    }
  }
  else {
    /* Infernal error */

    for (old = bindings; old; old = next) {
      next = old->next;
      binding_destroy(d->home, old);
    }

    return respond_transaction(t, SIP_500_INTERNAL_SERVER_ERROR, TAG_END());
  }

  for (b = t->entry->bindings; b; b = b->next) {
    char const *expires;

    if (b->expires <= t->now)
      continue;

    *mm = sip_contact_copy(d->home, b->contact);
    if (*mm) {
      expires = su_sprintf(d->home, "expires=%u",
			   (unsigned)(b->expires - t->now));
      msg_header_add_param(d->home, (*mm)->m_common, expires);
      mm = &(*mm)->m_next;
    }
  }

  tbf = t->entry->contacts;
  t->entry->contacts = contacts;
  msg_header_free(d->home, tbf);

 ok200:
  return respond_transaction(t, SIP_200_OK,
			     SIPTAG_CONTACT(contacts),
			     TAG_END());
}

/* ---------------------------------------------------------------------- */

static int close_tports(void *_proxy)
{
  struct proxy *p = _proxy;
  struct domain *d;
  struct registration_entry *e;
  struct binding *b;

  /* Close all outbound transports */
  for (d = p->domains; d; d = d->next) {
    for (e = d->entries; e; e = e->next) {
      for (b = e->bindings; b; b = b->next) {
	if (b->tport) {
	  tport_shutdown(b->tport, 1);
	  tport_unref(b->tport);
	  b->tport = NULL;
	}
      }
    }
  }

  return 0;
}
