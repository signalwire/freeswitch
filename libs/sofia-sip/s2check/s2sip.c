/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2008 Nokia Corporation.
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

/**@CFILE s2sip.c
 * @brief Check-Based Test Suite for Sofia SIP
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Apr 30 12:48:27 EEST 2008 ppessi
 */

#include "config.h"

#undef NDEBUG

#define TP_STACK_T struct s2sip
#define TP_MAGIC_T struct tp_magic_s
#define SU_ROOT_MAGIC_T struct s2sip

#include "s2sip.h"
#include "s2base.h"
#include "s2dns.h"

#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sip_tag.h>
#include <sofia-sip/msg_addr.h>
#include <sofia-sip/su_log.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_string.h>
#include <sofia-sip/sresolv.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <time.h>


/* -- Module types ------------------------------------------------------ */

struct tp_magic_s
{
  sip_via_t *via;
  sip_contact_t *contact;
};

/* -- Module prototypes ------------------------------------------------- */

static msg_t *s2_msg(int flags);
static int s2_complete_response(msg_t *response,
				int status, char const *phrase,
				msg_t *request);

/* -- Module globals ---------------------------------------------------- */

struct s2sip *s2sip;

/* ---------------------------------------------------------------------- */

char *
s2_sip_generate_tag(su_home_t *home)
{
  static unsigned s2_tag_generator = 0;

  return su_sprintf(home, "tag=%s-%s/%u", _s2_suite, _s2_case, ++s2_tag_generator);
}

/* ---------------------------------------------------------------------- */
/* SIP messages */

struct message *
s2_sip_remove_message(struct message *m)
{
  if ((*m->prev = m->next))
    m->next->prev = m->prev;

  m->prev = NULL, m->next = NULL;

  return m;
}

void
s2_sip_free_message(struct message *m)
{
  if (m) {
    if (m->prev) {
      if ((*m->prev = m->next))
	m->next->prev = m->prev;
    }
    msg_destroy(m->msg);
    tport_unref(m->tport);
    free(m);
  }
}

void s2_sip_flush_messages(void)
{
  while (s2sip->received) {
    s2_sip_free_message(s2sip->received);
  }
}

struct message *
s2_sip_next_response(void)
{
  struct message *m;

  for (;;) {
    for (m = s2sip->received; m; m = m->next) {
      if (m->sip->sip_status)
	return s2_sip_remove_message(m);
    }
    s2_step();
  }
}

struct message *
s2_sip_wait_for_response(int status, sip_method_t method, char const *name)
{
  struct message *m;

  for (;;) {
    for (m = s2sip->received; m; m = m->next) {
      if (!m->sip->sip_status)
	continue;

      if (status != 0 && m->sip->sip_status->st_status != status)
	continue;

      if (method == sip_method_unknown && name == NULL)
	break;

      if (m->sip->sip_cseq == NULL)
	continue;

      if (m->sip->sip_cseq->cs_method != method)
	continue;
      if (name == NULL)
	break;
      if (strcmp(m->sip->sip_cseq->cs_method_name, name) == 0)
	break;
    }

    if (m)
      return s2_sip_remove_message(m);

    s2_step();
  }
}

int
s2_sip_check_response(int status, sip_method_t method, char const *name)
{
  struct message *m = s2_sip_wait_for_response(status, method, name);
  s2_sip_free_message(m);
  return m != NULL;
}

int
s2_check_in_response(struct dialog *dialog,
		     int status, sip_method_t method, char const *name)
{
  struct message *m = s2_sip_wait_for_response(status, method, name);
  s2_sip_update_dialog(dialog, m);
  s2_sip_free_message(m);
  return m != NULL;
}

struct message *
s2_sip_next_request(sip_method_t method, char const *name)
{
  struct message *m;

  for (m = s2sip->received; m; m = m->next) {
    if (m->sip->sip_request)
      if (method == sip_method_unknown && name == NULL)
	return s2_sip_remove_message(m);

    if (m->sip->sip_request->rq_method == method &&
	strcmp(m->sip->sip_request->rq_method_name, name) == 0)
      return s2_sip_remove_message(m);
  }

  return NULL;
}

struct message *
s2_sip_wait_for_request(sip_method_t method, char const *name)
{
  return s2_sip_wait_for_request_timeout(method, name, (unsigned)-1);
}

struct message *
s2_sip_wait_for_request_timeout(sip_method_t method, char const *name, unsigned steps)
{
  struct message *m;

  for (; steps > 0; steps--, s2_step()) {
    for (m = s2sip->received; m; m = m->next) {
      if (m->sip->sip_request) {
	if (method == sip_method_unknown && name == NULL)
	  return s2_sip_remove_message(m);

	if (m->sip->sip_request->rq_method == method &&
	    strcmp(m->sip->sip_request->rq_method_name, name) == 0)
	  return s2_sip_remove_message(m);
      }
    }
  }

  return NULL;
}

int
s2_sip_check_request(sip_method_t method, char const *name)
{
  struct message *m = s2_sip_wait_for_request(method, name);
  if (m) s2_sip_free_message(m);
  return m != NULL;
}

int
s2_sip_check_request_timeout(sip_method_t method,
			     char const *name,
			     unsigned timeout)
{
  struct message *m = s2_sip_wait_for_request_timeout(method, name, timeout);
  if (m) s2_sip_free_message(m);
  return m != NULL;
}

void
s2_sip_save_uas_dialog(struct dialog *d, sip_t *sip)
{
  if (d && !d->local) {
    assert(sip->sip_request);
    d->local = sip_from_dup(d->home, sip->sip_to);
    if (d->local->a_tag == NULL)
      sip_from_tag(d->home, d->local, s2_sip_generate_tag(d->home));
    d->remote = sip_to_dup(d->home, sip->sip_from);
    d->call_id = sip_call_id_dup(d->home, sip->sip_call_id);
    d->rseq = sip->sip_cseq->cs_seq;
    /* d->route = sip_route_dup(d->home, sip->sip_record_route); */
    d->target = sip_contact_dup(d->home, sip->sip_contact);
  }
}

struct message *
s2_sip_respond_to(struct message *m, struct dialog *d,
		  int status, char const *phrase,
		  tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  msg_t *reply;
  sip_t *sip;
  su_home_t *home;
  tp_name_t tpn[1];
  char *rport;

  assert(m); assert(m->msg); assert(m->tport);
  assert(100 <= status && status < 700);

  s2_sip_save_uas_dialog(d, m->sip);

  ta_start(ta, tag, value);

  reply = s2_msg(0); sip = sip_object(reply); home = msg_home(reply);

  assert(reply && home && sip);

  if (sip_add_tl(reply, sip, ta_tags(ta)) < 0) {
    abort();
  }

  s2_complete_response(reply, status, phrase, m->msg);

  if (sip->sip_status && sip->sip_status->st_status > 100 &&
      sip->sip_to && !sip->sip_to->a_tag &&
      sip->sip_cseq && sip->sip_cseq->cs_method != sip_method_cancel) {
    char const *ltag = NULL;

    if (d && d->local)
      ltag = d->local->a_tag;

    if (ltag == NULL)
      ltag = s2_sip_generate_tag(home);

    if (sip_to_tag(msg_home(reply), sip->sip_to, ltag) < 0) {
      assert(!"add To tag");
    }
  }

  if (d && !d->contact) {
    d->contact = sip_contact_dup(d->home, sip->sip_contact);
  }

  *tpn = *tport_name(m->tport);

  rport = su_sprintf(home, "rport=%u",
		     ntohs(((su_sockaddr_t *)
			    msg_addrinfo(m->msg)->ai_addr)->su_port));

  if (s2sip->server_uses_rport &&
      sip->sip_via->v_rport &&
      sip->sip_via->v_rport[0] == '\0') {
    msg_header_add_param(home, sip->sip_via->v_common, rport);
  }

  tpn->tpn_port = rport + strlen("rport=");

  tport_tsend(m->tport, reply, tpn, TPTAG_MTU(INT_MAX), ta_tags(ta));
  msg_destroy(reply);

  ta_end(ta);

  return m;
}

/** Add headers from the request to the response message. */
static int
s2_complete_response(msg_t *response,
		     int status, char const *phrase,
		     msg_t *request)
{
  su_home_t *home = msg_home(response);
  sip_t *response_sip = sip_object(response);
  sip_t const *request_sip = sip_object(request);

  int incomplete = 0;

  if (!response_sip || !request_sip || !request_sip->sip_request)
    return -1;

  if (!response_sip->sip_status)
    response_sip->sip_status = sip_status_create(home, status, phrase, NULL);
  if (!response_sip->sip_via)
    response_sip->sip_via = sip_via_dup(home, request_sip->sip_via);
  if (!response_sip->sip_from)
    response_sip->sip_from = sip_from_dup(home, request_sip->sip_from);
  if (!response_sip->sip_to)
    response_sip->sip_to = sip_to_dup(home, request_sip->sip_to);
  if (!response_sip->sip_call_id)
    response_sip->sip_call_id =
      sip_call_id_dup(home, request_sip->sip_call_id);
  if (!response_sip->sip_cseq)
    response_sip->sip_cseq = sip_cseq_dup(home, request_sip->sip_cseq);

  if (!response_sip->sip_record_route && request_sip->sip_record_route)
    sip_add_dup(response, response_sip, (void*)request_sip->sip_record_route);

  incomplete = sip_complete_message(response) < 0;

  msg_serialize(response, (msg_pub_t *)response_sip);

  if (incomplete ||
      !response_sip->sip_status ||
      !response_sip->sip_via ||
      !response_sip->sip_from ||
      !response_sip->sip_to ||
      !response_sip->sip_call_id ||
      !response_sip->sip_cseq ||
      !response_sip->sip_content_length ||
      !response_sip->sip_separator ||
      (request_sip->sip_record_route && !response_sip->sip_record_route))
    return -1;

  return 0;
}

/* Send request (updating dialog).
 *
 * Return zero upon success, nonzero upon failure.
 */
int
s2_sip_request_to(struct dialog *d,
		  sip_method_t method, char const *name,
		  tport_t *tport,
		  tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  tagi_t const *tags;

  msg_t *msg = s2_msg(0);
  sip_t *sip = sip_object(msg);
  url_t const *target = NULL;
  sip_cseq_t cseq[1];
  sip_via_t via[1]; char const *v_params[8];
  sip_content_length_t l[1];
  tp_name_t tpn[1];
  tp_magic_t *magic;
  int user_via = 0;

  ta_start(ta, tag, value);
  tags = ta_args(ta);

  if (sip_add_tagis(msg, sip, &tags) < 0)
    goto error;

  if (sip->sip_request)
    target = sip->sip_request->rq_url;
  else if (d->target)
    target = d->target->m_url;
  else if (s2sip->sut.contact)
    target = s2sip->sut.contact->m_url;
  else
    target = NULL;

  if (target == NULL)
    goto error;

  if (!sip->sip_request) {
    sip_request_t *rq;
    rq = sip_request_create(msg_home(msg), method, name,
			    (url_string_t *)target, NULL);
    sip_header_insert(msg, sip, (sip_header_t *)rq);
  }

  if (!d->local && sip->sip_from)
    d->local = sip_from_dup(d->home, sip->sip_from);
  if (!d->contact && sip->sip_contact)
    d->contact = sip_contact_dup(d->home, sip->sip_contact);
  if (!d->remote && sip->sip_to)
    d->remote = sip_to_dup(d->home, sip->sip_to);
  if (!d->target && sip->sip_request)
    d->target = sip_contact_create(d->home,
				   (url_string_t *)sip->sip_request->rq_url,
				   NULL);
  if (!d->call_id && sip->sip_call_id)
    d->call_id = sip_call_id_dup(d->home, sip->sip_call_id);
  if (!d->lseq && sip->sip_cseq)
    d->lseq = sip->sip_cseq->cs_seq;

  if (!d->local)
    d->local = sip_from_dup(d->home, s2sip->aor);
  if (!d->contact)
    d->contact = sip_contact_dup(d->home, s2sip->contact);
  if (!d->remote)
    d->remote = sip_to_dup(d->home, s2sip->sut.aor);
  if (!d->call_id)
    d->call_id = sip_call_id_create(d->home, NULL);
  assert(d->local && d->contact);
  assert(d->remote && d->target);
  assert(d->call_id);

  if (tport == NULL)
    tport = d->tport;

  if (tport == NULL)
    tport = s2sip->sut.tport;

  if (tport == NULL && d->target->m_url->url_type == url_sips)
    tport = s2sip->tls.tport;

  if (tport == NULL)
    tport = s2sip->udp.tport;
  else if (tport == NULL)
    tport = s2sip->tcp.tport;
  else if (tport == NULL)
    tport = s2sip->tls.tport;

  assert(tport);

  *tpn = *tport_name(tport);

  if (tport_is_primary(tport)) {
    tpn->tpn_host = target->url_host;
    tpn->tpn_port = url_port(target);
    if (!tpn->tpn_port || !tpn->tpn_port[0])
      tpn->tpn_port = url_port_default(target->url_type);
  }

  magic = tport_magic(tport);
  assert(magic != NULL);

  sip_cseq_init(cseq);
  cseq->cs_method = method;
  cseq->cs_method_name = name;

  if (d->invite && (method == sip_method_ack || method == sip_method_cancel)) {
    cseq->cs_seq = sip_object(d->invite)->sip_cseq->cs_seq;
  }
  else {
    cseq->cs_seq = ++d->lseq;
  }

  if (sip->sip_via) {
    user_via = 1;
  }
  else if (d->invite && method == sip_method_cancel) {
    *via = *sip_object(d->invite)->sip_via;
  }
  else {
    *via = *magic->via;
    via->v_params = v_params;
    v_params[0] = su_sprintf(msg_home(msg), "branch=z9hG4bK%lx", ++s2sip->tid);
    v_params[1] = NULL;
  }

  sip_content_length_init(l);
  if (sip->sip_payload)
    l->l_length = sip->sip_payload->pl_len;

  if (d->local->a_tag == NULL) {
    char const *ltag = s2_sip_generate_tag(d->home);

    if (sip_from_tag(d->home, d->local, ltag) < 0) {
      assert(!"add To tag");
    }

    if (sip->sip_from && sip->sip_from->a_tag == NULL) {
      if (sip_from_tag(msg_home(msg), sip->sip_from, ltag) < 0) {
	assert(!"add To tag");
      }
    }
  }

  sip_add_tl(msg, sip,
	     TAG_IF(!sip->sip_from, SIPTAG_FROM(d->local)),
	     TAG_IF(!sip->sip_contact, SIPTAG_CONTACT(d->contact)),
	     TAG_IF(!sip->sip_to, SIPTAG_TO(d->remote)),
	     TAG_IF(!sip->sip_call_id, SIPTAG_CALL_ID(d->call_id)),
	     TAG_IF(!sip->sip_cseq, SIPTAG_CSEQ(cseq)),
	     TAG_IF(!user_via, SIPTAG_VIA(via)),
	     TAG_IF(!sip->sip_content_length, SIPTAG_CONTENT_LENGTH(l)),
	     TAG_IF(!sip->sip_separator, SIPTAG_SEPARATOR_STR("\r\n")),
	     TAG_END());

  msg_serialize(msg, NULL);

  if (method == sip_method_invite) {
    msg_destroy(d->invite);
    d->invite = msg_ref_create(msg);
  }

  tport = tport_tsend(tport, msg, tpn, ta_tags(ta));
  ta_end(ta);

  if (d->tport != tport) {
    tport_unref(d->tport);
    d->tport = tport_ref(tport);
  }

  return tport ? 0 : -1;

 error:
  ta_end(ta);
  return -1;
}

/** Save information from response.
 *
 * Send ACK for error messages to INVITE.
 */
int s2_sip_update_dialog(struct dialog *d, struct message *m)
{
  int status = 0;

  if (m->sip->sip_status)
    status = m->sip->sip_status->st_status;

  if (100 < status && status < 300) {
    d->remote = sip_to_dup(d->home, m->sip->sip_to);
    if (m->sip->sip_contact)
      d->contact = sip_contact_dup(d->home, m->sip->sip_contact);
  }

  if (300 <= status && m->sip->sip_cseq &&
      m->sip->sip_cseq->cs_method == sip_method_invite &&
      d->invite) {
    msg_t *ack = s2_msg(0);
    sip_t *sip = sip_object(ack);
    sip_t *invite = sip_object(d->invite);
    sip_request_t rq[1];
    sip_cseq_t cseq[1];
    tp_name_t tpn[1];

    *rq = *invite->sip_request;
    rq->rq_method = sip_method_ack, rq->rq_method_name = "ACK";
    *cseq = *invite->sip_cseq;
    cseq->cs_method = sip_method_ack, cseq->cs_method_name = "ACK";

    sip_add_tl(ack, sip,
	       SIPTAG_REQUEST(rq),
	       SIPTAG_VIA(invite->sip_via),
	       SIPTAG_FROM(invite->sip_from),
	       SIPTAG_TO(invite->sip_to),
	       SIPTAG_CALL_ID(invite->sip_call_id),
	       SIPTAG_CSEQ(cseq),
	       SIPTAG_CONTENT_LENGTH_STR("0"),
	       SIPTAG_SEPARATOR_STR("\r\n"),
	       TAG_END());

    *tpn = *tport_name(d->tport);
    if (!tport_is_secondary(d->tport) ||
	!tport_is_clear_to_send(d->tport)) {
      tpn->tpn_host = rq->rq_url->url_host;
      tpn->tpn_port = rq->rq_url->url_port;
    }

    msg_serialize(ack, NULL);
    tport_tsend(d->tport, ack, tpn, TAG_END());
  }

  return 0;
}

/* ---------------------------------------------------------------------- */
/* tport interface */
static void
s2_sip_stack_recv(struct s2sip *s2,
	      tport_t *tp,
	      msg_t *msg,
	      tp_magic_t *magic,
	      su_time_t now)
{
  struct message *next = calloc(1, sizeof *next), **prev;

  next->msg = msg;
  next->sip = sip_object(msg);
  next->when = now;
  next->tport = tport_ref(tp);

#if 0
  if (next->sip->sip_request)
    printf("%s: sent: %s\n", s2tester, next->sip->sip_request->rq_method_name);
  else
    printf("%s: sent: SIP/2.0 %u %s\n", s2tester,
	   next->sip->sip_status->st_status,
	   next->sip->sip_status->st_phrase);
#endif

  for (prev = &s2->received; *prev; prev = &(*prev)->next)
    ;

  next->prev = prev, *prev = next;
}

static void
s2_sip_stack_error(struct s2sip *s2,
	       tport_t *tp,
	       int errcode,
	       char const *remote)
{
  fprintf(stderr, "%s(%p): error %d (%s) from %s\n",
	  "s2_sip_error",
	  (void *)tp, errcode, su_strerror(errcode),
	  remote ? remote : "<unknown destination>");
}

static msg_t *
s2_sip_stack_alloc(struct s2sip *s2sip, int flags,
	       char const data[], usize_t size,
	       tport_t const *tport,
	       tp_client_t *tpc)
{
  return msg_create(s2sip->mclass, flags | s2sip->flags);
}

static msg_t *
s2_msg(int flags)
{
  return msg_create(s2sip->mclass, flags | s2sip->flags);
}

tp_stack_class_t const s2_sip_stack[1] =
  {{
      /* tpac_size */ (sizeof s2_sip_stack),
      /* tpac_recv */  s2_sip_stack_recv,
      /* tpac_error */ s2_sip_stack_error,
      /* tpac_alloc */ s2_sip_stack_alloc,
  }};

static char const *default_protocols[] = { "udp", "tcp", NULL };

/** Setup for SIP transports  */
void s2_sip_setup(char const *hostname,
		 char const * const *protocols,
		 tag_type_t tag, tag_value_t value, ...)
{
  su_home_t *home;
  ta_list ta;
  tp_name_t tpn[1];
  int bound;
  tport_t *tp;

  assert(s2base != NULL);
  assert(s2sip == NULL);

  s2sip = su_home_new(sizeof *s2sip);
  home = s2sip->home;

  s2sip->root = su_root_clone(s2base->root, s2sip);

  s2sip->aor = sip_from_format(home, "Bob <sip:bob@%s>",
			       hostname ? hostname : "example.net");
  if (hostname == NULL)
    hostname = "127.0.0.1";
  s2sip->hostname = hostname;
  s2sip->tid = (unsigned long)time(NULL) * 510633671UL;

  ta_start(ta, tag, value);

  s2sip->master = tport_tcreate(s2sip, s2_sip_stack, s2sip->root,
				TPTAG_LOG(getenv("S2_TPORT_LOG") != NULL),
				ta_tags(ta));
  assert(s2sip->master);
  s2sip->mclass = sip_default_mclass();
  s2sip->flags = 0;

  memset(tpn, 0, (sizeof tpn));
  tpn->tpn_proto = "*";
  tpn->tpn_host = "*";
  tpn->tpn_port = "*";

  if (protocols == NULL)
    protocols = default_protocols;

  bound = tport_tbind(s2sip->master, tpn, protocols,
		      TPTAG_SERVER(1),
		      ta_tags(ta));
  assert(bound != -1);

  tp = tport_primaries(s2sip->master);

  *tpn = *tport_name(tp);
  s2sip->contact = sip_contact_format(home, "<sip:%s:%s>",
				      tpn->tpn_host,
				      tpn->tpn_port);

  for (;tp; tp = tport_next(tp)) {
    sip_via_t *v;
    sip_contact_t *m;
    tp_magic_t *magic;

    if (tport_magic(tp))
      continue;

    *tpn = *tport_name(tp);

    v = sip_via_format(home, "SIP/2.0/%s %s:%s",
		       tpn->tpn_proto,
		       tpn->tpn_host,
		       tpn->tpn_port);
    assert(v != NULL);
    if (!su_casenmatch(tpn->tpn_proto, "tls", 3)) {
      m = sip_contact_format(home, "<sip:%s:%s;transport=%s>",
			     tpn->tpn_host,
			     tpn->tpn_port,
			     tpn->tpn_proto);
      if (s2sip->udp.contact == NULL && su_casematch(tpn->tpn_proto, "udp")) {
	s2sip->udp.tport = tport_ref(tp);
	s2sip->udp.contact = m;
      }
      if (s2sip->tcp.contact == NULL && su_casematch(tpn->tpn_proto, "tcp")) {
	s2sip->tcp.tport = tport_ref(tp);
	s2sip->tcp.contact = m;
      }
    }
    else if (!su_casematch(tpn->tpn_proto, "tls")) {
      m = sip_contact_format(s2sip->home, "<sips:%s:%s;transport=%s>",
			     tpn->tpn_host,
			     tpn->tpn_port,
			     tpn->tpn_proto);
    }
    else {
      m = sip_contact_format(s2sip->home, "<sips:%s:%s>",
			     tpn->tpn_host,
			     tpn->tpn_port);
      if (s2sip->tls.contact == NULL) {
	s2sip->tls.tport = tport_ref(tp);
	s2sip->tls.contact = m;
      }
    }
    assert(m != NULL);

    magic = su_zalloc(home, (sizeof *magic));
    magic->via = v, magic->contact = m;

    if (s2sip->contact == NULL)
      s2sip->contact = m;

    tport_set_magic(tp, magic);
  }
}

void
s2_sip_teardown(void)
{
  if (s2sip) {
    tport_destroy(s2sip->master), s2sip->master = NULL;
    su_root_destroy(s2sip->root), s2sip->root = NULL;
    su_home_unref(s2sip->home);
    s2sip = NULL;
  }
}
