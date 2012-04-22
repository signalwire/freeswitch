/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2007 Nokia Corporation.
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

/**@CFILE check_nua.c
 *
 * @brief Check-driven tester for Sofia SIP User Agent library
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @copyright (C) 2007 Nokia Corporation.
 */

#include "config.h"

#undef NDEBUG

#include "check_nua.h"
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
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <time.h>

static void usage(int exitcode)
{
  fprintf(exitcode ? stderr : stdout,
	  "usage: %s [--xml=logfile] case,...\n", s2_tester);
  exit(exitcode);
}

int main(int argc, char *argv[])
{
  int i, failed = 0, selected = 0;
  int threading, single_thread, multi_thread;
  char const *xml = NULL;
  Suite *suite = suite_create("Unit tests for Sofia-SIP UA Engine");
  SRunner *runner;

  s2_tester = "check_nua";

  s2_suite("N2");

  if (getenv("CHECK_NUA_VERBOSE"))
    s2_start_stop = strtoul(getenv("CHECK_NUA_VERBOSE"), NULL, 10);

  for (i = 1; argv[i]; i++) {
    if (su_strnmatch(argv[i], "--xml=", strlen("--xml="))) {
      xml = argv[i] + strlen("--xml=");
    }
    else if (su_strmatch(argv[i], "--xml")) {
      if (!(xml = argv[++i]))
	usage(2);
    }
    else if (su_strmatch(argv[i], "-v")) {
      s2_start_stop = 1;
    }
    else if (su_strmatch(argv[i], "-?") ||
	     su_strmatch(argv[i], "-h") ||
	     su_strmatch(argv[i], "--help"))
      usage(0);
    else {
      s2_select_tests(argv[i]);
      selected = 1;
    }
  }

  if (!selected)
    s2_select_tests(getenv("CHECK_NUA_CASES"));

  if (getenv("CHECK_NUA_THREADING")) {
    single_thread = strcmp(getenv("CHECK_NUA_THREADING"), "no");
    multi_thread = !single_thread;
  }
  else {
    single_thread = multi_thread = 1;
  }

  if (single_thread) {
    check_register_cases(suite, threading = 0);
    check_simple_cases(suite, threading = 0);
    check_session_cases(suite, threading = 0);
    check_etsi_cases(suite, threading = 0);
  }

  if (multi_thread) {
    check_register_cases(suite, threading = 1);
    check_session_cases(suite, threading = 1);
    check_etsi_cases(suite, threading = 1);
    check_simple_cases(suite, threading = 1);
  }

  runner = srunner_create(suite);

  if (xml)
    srunner_set_xml(runner, argv[1]);

  srunner_run_all(runner, CK_ENV);
  failed = srunner_ntests_failed(runner);
  srunner_free(runner);

  exit(failed ? EXIT_FAILURE : EXIT_SUCCESS);
}

/* ---------------------------------------------------------------------- */

/* -- Globals -------------------------------------------------------------- */

struct s2nua *s2;

int s2_nua_thread = 0;

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

    su_root_step(s2base->root, 100);
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

    su_root_step(s2base->root, 100);
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


/* ====================================================================== */

SOFIAPUBVAR su_log_t nua_log[];
SOFIAPUBVAR su_log_t soa_log[];
SOFIAPUBVAR su_log_t nea_log[];
SOFIAPUBVAR su_log_t nta_log[];
SOFIAPUBVAR su_log_t tport_log[];
SOFIAPUBVAR su_log_t su_log_default[];

void
s2_setup_logs(int level)
{
  su_log_soft_set_level(nua_log, level);
  su_log_soft_set_level(soa_log, level);
  su_log_soft_set_level(su_log_default, level);
  su_log_soft_set_level(nea_log, level);
  su_log_soft_set_level(nta_log, level);
  su_log_soft_set_level(tport_log, level);

  if (getenv("TPORT_LOG") == NULL && getenv("S2_TPORT_LOG") == NULL) {
    if (s2sip)
      tport_set_params(s2sip->master, TPTAG_LOG(level > 1), TAG_END());
  }
}

nua_t *s2_nua_setup(char const *label,
		    tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;

  s2_setup(label);

  s2 = su_home_new(sizeof *s2);

  s2_dns_setup(s2base->root);

  s2_setup_logs(0);
  s2_sip_setup("example.org", NULL, TAG_END());
  assert(s2sip->contact);

  s2_dns_domain("example.org", 1,
		"s2", 1, s2sip->udp.contact->m_url,
		"s2", 1, s2sip->tcp.contact->m_url,
		NULL);

  /* enable/disable multithreading */
  su_root_threading(s2base->root, s2_nua_thread);

  ta_start(ta, tag, value);
  s2->nua =
    nua_create(s2base->root,
	       s2_nua_callback,
	       s2,
	       SIPTAG_FROM_STR("Alice <sip:alice@example.org>"),
	       /* NUTAG_PROXY((url_string_t *)s2sip->contact->m_url), */
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

void
s2_nua_fast_forward(unsigned long seconds,
		    su_root_t *steproot)
{
  s2_fast_forward(seconds, NULL);

  if (s2_nua_thread)
    /* Wake up nua thread */
    nua_handle_by_call_id(s2->nua, NULL);

  if (steproot)
    su_root_step(steproot, 0);
}

void s2_nua_teardown(void)
{
  if (s2) {
    struct s2nua *zap = s2;
    nua_destroy(s2->nua), s2->nua = NULL;
    s2 = NULL;
    su_home_unref(zap->home);
  }

  s2_dns_teardown();
  s2_sip_teardown();
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

  m = s2_sip_wait_for_request(SIP_METHOD_REGISTER);
  assert(m);

  s2_save_register(m);

  s2_sip_respond_to(m, NULL,
		SIP_200_OK,
		SIPTAG_CONTACT(s2->registration->contact),
		TAG_END());
  s2_sip_free_message(m);

  assert(s2->registration->contact != NULL);
  fail_unless_event(nua_r_register, 200);

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

    m = s2_sip_wait_for_request(SIP_METHOD_REGISTER); assert(m);
    s2_save_register(m);
    s2_sip_respond_to(m, NULL,
		  SIP_200_OK,
		  SIPTAG_CONTACT(s2->registration->contact),
		  TAG_END());
    assert(s2->registration->contact == NULL);

    s2_sip_free_message(m);

    fail_unless_event(nua_r_unregister, 200);

    nua_handle_destroy(nh);
    s2->registration->nh = NULL;
  }
}

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

  s2sip->sut.aor = s2->registration->aor;
  s2sip->sut.contact = s2->registration->contact;
  s2sip->sut.tport = s2->registration->tport;

  return 0;
}
