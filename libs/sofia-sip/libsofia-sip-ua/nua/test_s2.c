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

/**@CFILE s2tester.c
 * @brief 2nd test Suite for Sofia SIP User Agent Engine
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Apr 30 12:48:27 EEST 2008 ppessi
 */

#include "config.h"

#undef NDEBUG

#define TP_MAGIC_T struct tp_magic_s

#include "test_s2.h"
#include "s2dns.h"

#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_status.h>
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

char const *s2_tester = "s2_tester";
int s2_start_stop;

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
static char *s2_generate_tag(su_home_t *home);

/* -- Module globals ---------------------------------------------------- */

struct tester *s2;

static char const *_s2case = "0.0";
static unsigned s2_tag_generator = 0;

/* -- Globals ----------------------------------------------------------- */

unsigned s2_default_registration_duration = 3600;

char const s2_auth_digest_str[] =
  "Digest realm=\"s2test\", "
  "nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\", "
  "qop=\"auth\", "
  "algorithm=\"MD5\"";

char const s2_auth_credentials[] = "Digest:\"s2test\":abc:abc";

char const s2_auth2_digest_str[] =
  "Digest realm=\"s2test2\", "
  "nonce=\"fb0c093dcd98b7102dd2f0e8b11d0f600b\", "
  "qop=\"auth\", "
  "algorithm=\"MD5\"";

char const s2_auth2_credentials[] = "Digest:\"s2test2\":abc:abc";

char const s2_auth3_digest_str[] =
  "Digest realm=\"s2test3\", "
  "nonce=\"e8b11d0f600bfb0c093dcd98b7102dd2f0\", "
  "qop=\"auth-int\", "
  "algorithm=\"MD5-sess\"";

char const s2_auth3_credentials[] = "Digest:\"s2test3\":abc:abc";

int s2_nua_thread = 0;

/* -- NUA events -------------------------------------------------------- */

struct event *s2_remove_event(struct event *e)
{
  if ((*e->prev = e->next))
    e->next->prev = e->prev;

  e->prev = NULL, e->next = NULL;

  return e;
}

void s2_free_event(struct event *e)
{
  if (e) {
    if (e->prev) {
      if ((*e->prev = e->next))
	e->next->prev = e->prev;
    }
    nua_destroy_event(e->event);
    nua_handle_unref(e->nh);
    free(e);
  }
}

void s2_flush_events(void)
{
  while (s2->events) {
    s2_free_event(s2->events);
  }
}

struct event *s2_next_event(void)
{
  for (;;) {
    if (s2->events)
      return s2_remove_event(s2->events);

    su_root_step(s2->root, 100);
  }
}

struct event *s2_wait_for_event(nua_event_t event, int status)
{
  struct event *e;

  for (;;) {
    for (e = s2->events; e; e = e->next) {
      if (event != nua_i_none && event != e->data->e_event)
	continue;
      if (status && e->data->e_status != status)
	continue;
      return s2_remove_event(e);
    }

    su_root_step(s2->root, 100);
  }
}

int s2_check_event(nua_event_t event, int status)
{
  struct event *e = s2_wait_for_event(event, status);
  s2_free_event(e);
  return e != NULL;
}

int s2_check_callstate(enum nua_callstate state)
{
  int retval = 0;
  tagi_t const *tagi;
  struct event *e;

  e = s2_wait_for_event(nua_i_state, 0);
  if (e) {
    tagi = tl_find(e->data->e_tags, nutag_callstate);
    if (tagi) {
      retval = (tag_value_t)state == tagi->t_value;
    }
  }
  s2_free_event(e);
  return retval;
}

int s2_check_substate(struct event *e, enum nua_substate state)
{
  int retval = 0;
  tagi_t const *tagi;

  tagi = tl_find(e->data->e_tags, nutag_substate);
  if (tagi) {
    retval = (tag_value_t)state == tagi->t_value;
  }

  return retval;
}

static void
s2_nua_callback(nua_event_t event,
		int status, char const *phrase,
		nua_t *nua, nua_magic_t *_t,
		nua_handle_t *nh, nua_hmagic_t *hmagic,
		sip_t const *sip,
		tagi_t tags[])
{
  struct event *e, **prev;

  if (event == nua_i_active || event == nua_i_terminated)
    return;

  e = calloc(1, sizeof *e);
  nua_save_event(nua, e->event);
  e->nh = nua_handle_ref(nh);
  e->data = nua_event_data(e->event);

  for (prev = &s2->events; *prev; prev = &(*prev)->next)
    ;

  *prev = e, e->prev = prev;
}

/* ---------------------------------------------------------------------- */
/* SIP messages sent by nua */

struct message *
s2_remove_message(struct message *m)
{
  if ((*m->prev = m->next))
    m->next->prev = m->prev;

  m->prev = NULL, m->next = NULL;

  return m;
}

void
s2_free_message(struct message *m)
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

void s2_flush_messages(void)
{
  while (s2->received) {
    s2_free_message(s2->received);
  }
}

struct message *
s2_next_response(void)
{
  struct message *m;

  for (;;) {
    for (m = s2->received; m; m = m->next) {
      if (m->sip->sip_status)
	return s2_remove_message(m);
    }
    su_root_step(s2->root, 100);
  }
}

struct message *
s2_wait_for_response(int status, sip_method_t method, char const *name)
{
  struct message *m;

  for (;;) {
    for (m = s2->received; m; m = m->next) {
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
      return s2_remove_message(m);

    su_root_step(s2->root, 100);
  }
}

int
s2_check_response(int status, sip_method_t method, char const *name)
{
  struct message *m = s2_wait_for_response(status, method, name);
  s2_free_message(m);
  return m != NULL;
}


struct message *
s2_next_request(void)
{
  struct message *m;

  for (;;) {
    for (m = s2->received; m; m = m->next) {
      if (m->sip->sip_request)
	return s2_remove_message(m);
    }

    su_root_step(s2->root, 100);
  }

  return NULL;
}

struct message *
s2_wait_for_request(sip_method_t method, char const *name)
{
  return s2_wait_for_request_timeout(method, name, -1);
}

struct message *
s2_wait_for_request_timeout(sip_method_t method, char const *name, int timeout)
{
  struct message *m;
  int t = timeout;

  for (;;) {
    for (m = s2->received; m; m = m->next) {
      if (m->sip->sip_request) {
	if (method == sip_method_unknown && name == NULL)
	  return s2_remove_message(m);

	if (m->sip->sip_request->rq_method == method &&
	    strcmp(m->sip->sip_request->rq_method_name, name) == 0)
	  return s2_remove_message(m);
      }
    }

    su_root_step(s2->root, 100);
    if (timeout != -1 && ((t -= 100) <= 0))
        break;
  }

  return NULL;
}

int
s2_check_request(sip_method_t method, char const *name)
{
  struct message *m = s2_wait_for_request(method, name);
  s2_free_message(m);
  return m != NULL;
}

int
s2_check_request_timeout(sip_method_t method, char const *name, int timeout)
{
  struct message *m = s2_wait_for_request_timeout(method, name, timeout);
  s2_free_message(m);
  return m != NULL;
}

void
s2_save_uas_dialog(struct dialog *d, sip_t *sip)
{
  if (d && !d->local) {
    assert(sip->sip_request);
    d->local = sip_from_dup(d->home, sip->sip_to);
    if (d->local->a_tag == NULL)
      sip_from_tag(d->home, d->local, s2_generate_tag(d->home));
    d->remote = sip_to_dup(d->home, sip->sip_from);
    d->call_id = sip_call_id_dup(d->home, sip->sip_call_id);
    d->rseq = sip->sip_cseq->cs_seq;
    /* d->route = sip_route_dup(d->home, sip->sip_record_route); */
    d->target = sip_contact_dup(d->home, sip->sip_contact);
  }
}

struct message *
s2_respond_to(struct message *m, struct dialog *d,
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

  s2_save_uas_dialog(d, m->sip);

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
      ltag = s2_generate_tag(home);

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

  if (s2->server_uses_rport &&
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
s2_request_to(struct dialog *d,
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
  else if (s2->registration->contact)
    target = s2->registration->contact->m_url;
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
    d->local = sip_from_dup(d->home, s2->local);
  if (!d->contact)
    d->contact = sip_contact_dup(d->home, s2->contact);
  if (!d->remote)
    d->remote = sip_to_dup(d->home, s2->registration->aor);
  if (!d->call_id)
    d->call_id = sip_call_id_create(d->home, NULL);
  assert(d->local && d->contact);
  assert(d->remote && d->target);
  assert(d->call_id);

  if (tport == NULL)
    tport = d->tport;

  if (tport == NULL)
    tport = s2->registration->tport;

  if (tport == NULL && d->target->m_url->url_type == url_sips)
    tport = s2->tls.tport;

  if (tport == NULL)
    tport = s2->udp.tport;
  else if (tport == NULL)
    tport = s2->tcp.tport;
  else if (tport == NULL)
    tport = s2->tls.tport;

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
    v_params[0] = su_sprintf(msg_home(msg), "branch=z9hG4bK%lx", ++s2->tid);
    v_params[1] = NULL;
  }

  sip_content_length_init(l);
  if (sip->sip_payload)
    l->l_length = sip->sip_payload->pl_len;

  if (d->local->a_tag == NULL) {
    char const *ltag = s2_generate_tag(d->home);

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
int s2_update_dialog(struct dialog *d, struct message *m)
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

int
s2_save_register(struct message *rm)
{
  sip_contact_t *contact, *m, **m_prev;
  sip_expires_t const *ex;
  sip_date_t const *date;
  sip_time_t now = rm->when.tv_sec, expires;

  msg_header_free_all(s2->home, (msg_header_t *)s2->registration->aor);
  msg_header_free_all(s2->home, (msg_header_t *)s2->registration->contact);
  tport_unref(s2->registration->tport);

  s2->registration->aor = NULL;
  s2->registration->contact = NULL;
  s2->registration->tport = NULL;

  if (rm == NULL)
    return 0;

  assert(rm && rm->sip && rm->sip->sip_request);
  assert(rm->sip->sip_request->rq_method == sip_method_register);

  ex = rm->sip->sip_expires;
  date = rm->sip->sip_date;

  contact = sip_contact_dup(s2->home, rm->sip->sip_contact);

  for (m_prev = &contact; *m_prev;) {
    m = *m_prev;

    expires = sip_contact_expires(m, ex, date,
				  s2_default_registration_duration,
				  now);
    if (expires) {
      char *p = su_sprintf(s2->home, "expires=%lu", (unsigned long)expires);
      msg_header_add_param(s2->home, m->m_common, p);
      m_prev = &m->m_next;
    }
    else {
      *m_prev = m->m_next;
      m->m_next = NULL;
      msg_header_free(s2->home, (msg_header_t *)m);
    }
  }

  if (contact == NULL)
    return 0;

  s2->registration->aor = sip_to_dup(s2->home, rm->sip->sip_to);
  s2->registration->contact = contact;
  s2->registration->tport = tport_ref(rm->tport);

  return 0;
}

/* ---------------------------------------------------------------------- */

static char *
s2_generate_tag(su_home_t *home)
{
  s2_tag_generator += 1;

  return su_sprintf(home, "tag=N2-%s/%u", _s2case, s2_tag_generator);
}

void s2_case(char const *number,
	     char const *title,
	     char const *description)
{
  _s2case = number;

  if (s2_start_stop)
    printf("%s - starting %s %s\n", s2_tester, number, title);
}

/* ---------------------------------------------------------------------- */
/* tport interface */
static void
s2_stack_recv(struct tester *s2,
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
    printf("nua sent: %s\n", next->sip->sip_request->rq_method_name);
  else
    printf("nua sent: SIP/2.0 %u %s\n",
	   next->sip->sip_status->st_status,
	   next->sip->sip_status->st_phrase);
#endif

  for (prev = &s2->received; *prev; prev = &(*prev)->next)
    ;

  next->prev = prev, *prev = next;
}

static void
s2_stack_error(struct tester *s2,
	       tport_t *tp,
	       int errcode,
	       char const *remote)
{
  fprintf(stderr, "%s(%p): error %d (%s) from %s\n",
	  "nua_tester_error",
	  (void *)tp, errcode, su_strerror(errcode),
	  remote ? remote : "<unknown destination>");
}

static msg_t *
s2_stack_alloc(struct tester *s2, int flags,
	       char const data[], usize_t size,
	       tport_t const *tport,
	       tp_client_t *tpc)
{
  return msg_create(s2->mclass, flags | s2->flags);
}

static msg_t *
s2_msg(int flags)
{
  return msg_create(s2->mclass, flags | s2->flags);
}

tp_stack_class_t const s2_stack[1] =
  {{
      /* tpac_size */ (sizeof s2_stack),
      /* tpac_recv */  s2_stack_recv,
      /* tpac_error */ s2_stack_error,
      /* tpac_alloc */ s2_stack_alloc,
  }};

/** Basic setup for test cases */
void s2_setup_base(char const *label, char const *hostname)
{
  assert(s2 == NULL);

  if (s2_start_stop > 1) {
    printf("%s - setup %s test case\n", s2_tester, label ? label : "next");
  }

  su_init();

  s2 = su_home_new(sizeof *s2);

  assert(s2 != NULL);

  s2->root = su_root_create(s2);

  assert(s2->root != NULL);

  s2->local = sip_from_format(s2->home, "Bob <sip:bob@%s>",
			     hostname ? hostname : "example.net");

  if (hostname == NULL)
    hostname = "127.0.0.1";

  s2->hostname = hostname;
  s2->tid = (unsigned long)time(NULL) * 510633671UL;

}

SOFIAPUBVAR su_log_t nua_log[];
SOFIAPUBVAR su_log_t soa_log[];
SOFIAPUBVAR su_log_t nea_log[];
SOFIAPUBVAR su_log_t nta_log[];
SOFIAPUBVAR su_log_t tport_log[];
SOFIAPUBVAR su_log_t su_log_default[];

void
s2_setup_logs(int level)
{
  assert(s2);

  su_log_soft_set_level(nua_log, level);
  su_log_soft_set_level(soa_log, level);
  su_log_soft_set_level(su_log_default, level);
  su_log_soft_set_level(nea_log, level);
  su_log_soft_set_level(nta_log, level);
  su_log_soft_set_level(tport_log, level);
}

static char const * default_protocols[] = { "udp", "tcp", NULL };

void
s2_setup_tport(char const * const *protocols,
	       tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  tp_name_t tpn[1];
  int bound;
  tport_t *tp;

  assert(s2 != NULL);

  ta_start(ta, tag, value);

  if (s2->master == NULL) {
    s2->master = tport_tcreate(s2, s2_stack, s2->root,
			       TPTAG_LOG(getenv("S2_TPORT_LOG") != NULL),
			       ta_tags(ta));

    if (s2->master == NULL) {
      assert(s2->master);
    }
    s2->mclass = sip_default_mclass();
    s2->flags = 0;
  }

  memset(tpn, 0, (sizeof tpn));
  tpn->tpn_proto = "*";
  tpn->tpn_host = s2->hostname;
  tpn->tpn_port = "*";

  if (protocols == NULL)
    protocols = default_protocols;

  bound = tport_tbind(s2->master, tpn, protocols,
		      TPTAG_SERVER(1),
		      ta_tags(ta));
  assert(bound != -1);

  tp = tport_primaries(s2->master);

  if (protocols == default_protocols && s2->contact == NULL) {
    *tpn = *tport_name(tp);
    s2->contact = sip_contact_format(s2->home, "<sip:%s:%s>",
				    tpn->tpn_host,
				    tpn->tpn_port);
  }

  for (;tp; tp = tport_next(tp)) {
    sip_via_t *v;
    sip_contact_t *m;
    tp_magic_t *magic;

    if (tport_magic(tp))
      continue;

    *tpn = *tport_name(tp);

    v = sip_via_format(s2->home, "SIP/2.0/%s %s:%s",
		       tpn->tpn_proto,
		       tpn->tpn_host,
		       tpn->tpn_port);
    assert(v != NULL);
    if (!su_casenmatch(tpn->tpn_proto, "tls", 3)) {
      m = sip_contact_format(s2->home, "<sip:%s:%s;transport=%s>",
			     tpn->tpn_host,
			     tpn->tpn_port,
			     tpn->tpn_proto);
      if (s2->udp.contact == NULL && su_casematch(tpn->tpn_proto, "udp")) {
	s2->udp.tport = tport_ref(tp);
	s2->udp.contact = m;
      }
      if (s2->tcp.contact == NULL && su_casematch(tpn->tpn_proto, "tcp")) {
	s2->tcp.tport = tport_ref(tp);
	s2->tcp.contact = m;
      }
    }
    else if (!su_casematch(tpn->tpn_proto, "tls")) {
      m = sip_contact_format(s2->home, "<sips:%s:%s;transport=%s>",
			     tpn->tpn_host,
			     tpn->tpn_port,
			     tpn->tpn_proto);
    }
    else {
      m = sip_contact_format(s2->home, "<sips:%s:%s>",
			     tpn->tpn_host,
			     tpn->tpn_port);
      if (s2->tls.contact == NULL) {
	s2->tls.tport = tport_ref(tp);
	s2->tls.contact = m;
      }
    }
    assert(m != NULL);

    magic = su_zalloc(s2->home, (sizeof *magic));
    magic->via = v, magic->contact = m;

    if (s2->contact == NULL)
      s2->contact = m;

    tport_set_magic(tp, magic);
  }
}

static char const *s2_teardown_label = NULL;

void
s2_teardown_started(char const *label)
{
  if (!s2_teardown_label) {
    s2_teardown_label = label;
    if (s2_start_stop > 1) {
      printf("%s - tearing down %s test case\n", s2_tester, label);
    }
  }
}

void
s2_teardown(void)
{
  s2 = NULL;
  su_deinit();

  if (s2_start_stop > 1) {
    printf("%s - %s test case tore down\n", s2_tester,
	   s2_teardown_label ? s2_teardown_label : "previous");
  }

  s2_teardown_label = NULL;
}

/* ====================================================================== */

nua_t *s2_nua_setup(char const *label,
		    tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;

  s2_setup_base(label, NULL);
  s2_dns_setup(s2->root);

  s2_setup_logs(0);
  s2_setup_tport(NULL, TAG_END());
  assert(s2->contact);
  s2_dns_domain("example.org", 1,
		"s2", 1, s2->udp.contact->m_url,
		"s2", 1, s2->tcp.contact->m_url,
		NULL);

  /* enable/disable multithreading */
  su_root_threading(s2->root, s2_nua_thread);

  ta_start(ta, tag, value);
  s2->nua =
    nua_create(s2->root,
	       s2_nua_callback,
	       s2,
	       SIPTAG_FROM_STR("Alice <sip:alice@example.org>"),
	       /* NUTAG_PROXY((url_string_t *)s2->contact->m_url), */
	       /* Use internal DNS server */
	       NUTAG_PROXY("sip:example.org"),
	       /* Force sresolv to use localhost and s2dns as DNS server */
#if HAVE_WIN32
	       SRESTAG_RESOLV_CONF("NUL"),
#else
	       SRESTAG_RESOLV_CONF("/dev/null"),
#endif
	       ta_tags(ta));
  ta_end(ta);

  return s2->nua;
}

void s2_nua_teardown(void)
{
  nua_destroy(s2->nua);
  s2->nua = NULL;
  s2_dns_teardown();
  s2_teardown();
}

/* ====================================================================== */

/** Register NUA user.
 *
 * <pre>
 *  A                  B
 *  |-----REGISTER---->|
 *  |<-----200 OK------|
 *  |                  |
 * </pre>
 */
void s2_register_setup(void)
{
  nua_handle_t *nh;
  struct message *m;

  assert(s2 && s2->nua);
  assert(!s2->registration->nh);

  nh = nua_handle(s2->nua, NULL, TAG_END());

  nua_register(nh, TAG_END());

  m = s2_wait_for_request(SIP_METHOD_REGISTER);
  assert(m);
  s2_save_register(m);

  s2_respond_to(m, NULL,
		SIP_200_OK,
		SIPTAG_CONTACT(s2->registration->contact),
		TAG_END());
  s2_free_message(m);

  assert(s2->registration->contact != NULL);
  s2_check_event(nua_r_register, 200);

  s2->registration->nh = nh;
}

/** Un-register NUA user.
 *
 * <pre>
 *  A                  B
 *  |-----REGISTER---->|
 *  |<-----200 OK------|
 *  |                  |
 * </pre>
 */
void s2_register_teardown(void)
{
  if (s2 && s2->registration->nh) {
    nua_handle_t *nh = s2->registration->nh;
    struct message *m;

    nua_unregister(nh, TAG_END());

    m = s2_wait_for_request(SIP_METHOD_REGISTER); assert(m);
    s2_save_register(m);
    s2_respond_to(m, NULL,
		  SIP_200_OK,
		  SIPTAG_CONTACT(s2->registration->contact),
		  TAG_END());
    assert(s2->registration->contact == NULL);

    s2_free_message(m);

    s2_check_event(nua_r_unregister, 200);

    nua_handle_destroy(nh);
    s2->registration->nh = NULL;
  }
}
