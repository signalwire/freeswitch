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

/**@CFILE check_nta.c
 * @brief 2nd test Suite for Sofia SIP Transaction Engine
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Apr 30 12:48:27 EEST 2008 ppessi
 */

#include "config.h"

#undef NDEBUG

#include "check_nta.h"

#include "s2dns.h"
#include "s2base.h"
#include "s2sip.h"

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

/* -- Globals -------------------------------------------------------------- */

struct s2nta *s2;

/* -- main ----------------------------------------------------------------- */

static void usage(int exitcode)
{
  fprintf(exitcode ? stderr : stdout,
	  "usage: %s [--xml=logfile] case,...\n", s2_tester);
  exit(exitcode);
}

int main(int argc, char *argv[])
{
  int i, failed = 0, selected = 0;
  char const *xml = NULL;
  Suite *suite;
  SRunner *runner;

  s2_tester = "check_nta";
  s2_suite("NTA");

  if (getenv("CHECK_NTA_VERBOSE"))
    s2_start_stop = strtoul(getenv("CHECK_NTA_VERBOSE"), NULL, 10);

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
    s2_select_tests(getenv("CHECK_NTA_CASES"));

  suite = suite_create("Unit tests for nta (Sofia-SIP Transaction Engine)");

  suite_add_tcase(suite, check_nta_api_1_0());
  suite_add_tcase(suite, check_nta_client_2_0());
  suite_add_tcase(suite, check_nta_client_2_1());
  suite_add_tcase(suite, check_nta_client_2_2());

  runner = srunner_create(suite);

  if (xml)
    srunner_set_xml(runner, argv[1]);

  srunner_run_all(runner, CK_ENV);
  failed = srunner_ntests_failed(runner);
  srunner_free(runner);

  exit(failed ? EXIT_FAILURE : EXIT_SUCCESS);
}


/* -- NTA callbacks -------------------------------------------------------- */

struct event *
s2_nta_remove_event(struct event *e)
{
  if ((*e->prev = e->next))
    e->next->prev = e->prev;
  e->prev = NULL, e->next = NULL;
  return e;
}

void
s2_nta_free_event(struct event *e)
{
  if (e) {
    if (e->prev) {
      if ((*e->prev = e->next))
	e->next->prev = e->prev;
    }
    if (e->msg)
      msg_destroy(e->msg);
    free(e);
  }
}

void
s2_nta_flush_events(void)
{
  while (s2->events) {
    s2_nta_free_event(s2->events);
  }
}

struct event *
s2_nta_next_event(void)
{
  for (;;) {
    if (s2->events)
      return s2_nta_remove_event(s2->events);
    su_root_step(s2->root, 1);
  }
}

struct event *
s2_nta_vwait_for(enum wait_for wait_for0,
		 void const *value0,
		 va_list va0)
{
  struct event *e;

  for (;;) {
    for (e = s2->events; e; e = e->next) {
      va_list va;
      enum wait_for wait_for;
      void const *value;

      va_copy(va, va0);

      for (wait_for = wait_for0, value = value0;
	   wait_for;
	   wait_for = va_arg(va, enum wait_for),
	     value = va_arg(va, void const *)) {
	switch (wait_for) {
	case wait_for_amagic:
	  if (value != e->amagic)
	    goto next;
	  break;
	case wait_for_omagic:
	  if (value != e->omagic)
	    goto next;
	  break;
	case wait_for_orq:
	  if (value != e->orq)
	    goto next;
	  break;
	case wait_for_lmagic:
	  if (value != e->lmagic)
	    goto next;
	  break;
	case wait_for_leg:
	  if (value != e->leg)
	    goto next;
	  break;
	case wait_for_imagic:
	  if (value != e->imagic)
	    goto next;
	  break;
	case wait_for_irq:
	  if (value != e->irq)
	    goto next;
	  break;
	case wait_for_method:
	  if ((sip_method_t)value != e->method)
	    goto next;
	  break;
	case wait_for_method_name:
	  if (!su_strmatch(value, e->method_name))
	    goto next;
	  break;
	case wait_for_status:
	  if ((int)value != e->status)
	    goto next;
	  break;
	case wait_for_phrase:
	  if (!su_casematch(value, e->phrase))
	    goto next;
	  break;
	}
      }

    next:
      va_end(va);

      if (!wait_for)
	return s2_nta_remove_event(e);
    }
    su_root_step(s2->root, 1);
  }
}

struct event *
s2_nta_wait_for(enum wait_for wait_for,
		void const *value,
		...)
{
  struct event *e;
  va_list va;

  va_start(va, value);
  e = s2_nta_vwait_for(wait_for, value, va);
  va_end(va);

  return e;
}

int
s2_nta_check_request(enum wait_for wait_for,
		     void const *value,
		     ...)
{
  struct event *e;
  va_list va;

  va_start(va, value);
  e = s2_nta_vwait_for(wait_for, value, va);
  va_end(va);

  s2_nta_free_event(e);
  return e != NULL;
}

int
s2_nta_msg_callback(nta_agent_magic_t *magic,
		    nta_agent_t *nta,
		    msg_t *msg,
		    sip_t *sip)
{
  struct event *e, **prev;

  e = calloc(1, sizeof *e);

  e->amagic = magic;
  e->msg = msg;
  e->sip = sip;

  if (sip->sip_request) {
    e->method = sip->sip_request->rq_method;
    e->method_name = sip->sip_request->rq_method_name;
  }
  else {
    e->status = sip->sip_status->st_status;
    e->phrase = sip->sip_status->st_phrase;
  }

  for (prev = &s2->events; *prev; prev = &(*prev)->next)
    ;
  *prev = e, e->prev = prev;

  return 0;
}

int
s2_nta_orq_callback(nta_outgoing_magic_t *magic,
		    nta_outgoing_t *orq,
		    sip_t const *sip)
{
  struct event *e, **prev;

  e = calloc(1, sizeof *e);

  e->omagic = magic;
  e->orq = orq;
  e->msg = nta_outgoing_getresponse(orq);
  e->sip = sip_object(e->msg);

  e->status = nta_outgoing_status(orq);
  e->phrase = sip ? sip->sip_status->st_phrase : "";

  for (prev = &s2->events; *prev; prev = &(*prev)->next)
    ;
  *prev = e, e->prev = prev;

  return 0;
}

int
s2_nta_leg_callback(nta_leg_magic_t *magic,
		    nta_leg_t *leg,
		    nta_incoming_t *irq,
		    sip_t const *sip)
{
  struct event *e, **prev;

  e = calloc(1, sizeof *e);

  e->lmagic = magic;
  e->leg = leg;
  e->irq = irq;

  e->msg = nta_incoming_getrequest(irq);
  e->sip = sip_object(e->msg);

  e->method = e->sip->sip_request->rq_method;
  e->method_name = e->sip->sip_request->rq_method_name;

  for (prev = &s2->events; *prev; prev = &(*prev)->next)
    ;
  *prev = e, e->prev = prev;

  return 0;
}

int
s2_nta_irq_callback(nta_incoming_magic_t *magic,
		    nta_incoming_t *irq,
		    sip_t const *sip)
{
  struct event *e, **prev;

  e = calloc(1, sizeof *e);

  e->imagic = magic;
  e->irq = irq;
  e->msg = nta_incoming_getrequest_ackcancel(irq);
  e->sip = sip_object(e->msg);

  e->method = e->sip ? e->sip->sip_request->rq_method : 0;
  e->method_name = e->sip ? e->sip->sip_request->rq_method_name : NULL;

  for (prev = &s2->events; *prev; prev = &(*prev)->next)
    ;
  *prev = e, e->prev = prev;

  return 0;
}

/* ====================================================================== */

SOFIAPUBVAR su_log_t nta_log[];
SOFIAPUBVAR su_log_t sresolv_log[];
SOFIAPUBVAR su_log_t tport_log[];
SOFIAPUBVAR su_log_t su_log_default[];

void
s2_nta_setup_logs(int level)
{
  su_log_soft_set_level(su_log_default, level);
  su_log_soft_set_level(tport_log, level);
  su_log_soft_set_level(nta_log, level);
  su_log_soft_set_level(sresolv_log, level);

  if (getenv("TPORT_LOG") == NULL && getenv("S2_TPORT_LOG") == NULL) {
    if (s2sip)
      tport_set_params(s2sip->master, TPTAG_LOG(level > 1), TAG_END());
  }
}

void
s2_nta_setup(char const *label,
	     char const * const *transports,
	     tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;

  s2_setup(label);
  s2_nta_setup_logs(0);
  s2_dns_setup(s2base->root);
  ta_start(ta, tag, value);
  s2_sip_setup("example.org", transports, ta_tags(ta));
  ta_end(ta);
  assert(s2sip->contact);

  s2_dns_domain("example.org", 1,
		"s2", 1, s2sip->udp.contact ? s2sip->udp.contact->m_url : NULL,
		"s2", 1, s2sip->tcp.contact ? s2sip->tcp.contact->m_url : NULL,
		"s2", 1, s2sip->tls.contact ? s2sip->tls.contact->m_url : NULL,
		NULL);
}

nta_agent_t *
s2_nta_agent_setup(url_string_t const *bind_url,
		   nta_message_f *callback,
		   nta_agent_magic_t *magic,
		   tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;

  assert(s2base);

  s2 = su_home_new(sizeof *s2); assert(s2);
  s2->root = su_root_clone(s2base->root, s2); assert(s2->root);
  fail_unless(s2->root != NULL);

  ta_start(ta, tag, value);
  s2->nta =
    nta_agent_create(s2->root,
		     bind_url ? bind_url : URL_STRING_MAKE("sip:*:*"),
		     callback,
		     magic,
		     /* Use internal DNS server */
		     /* Force sresolv to use localhost and s2dns as DNS server */
#if HAVE_WIN32
		     SRESTAG_RESOLV_CONF("NUL"),
#else
		     SRESTAG_RESOLV_CONF("/dev/null"),
#endif
		     ta_tags(ta));
  ta_end(ta);

  assert(s2->nta);

  if (callback == NULL)
    s2->default_leg = nta_leg_tcreate(s2->nta, s2_nta_leg_callback, NULL,
				      NTATAG_NO_DIALOG(1),
				      TAG_END());

  return s2->nta;
}

void s2_nta_teardown(void)
{
  if (s2) {
    s2_nta_flush_events();

    if (s2->default_leg)
      nta_leg_destroy(s2->default_leg), s2->default_leg = NULL;
    nta_agent_destroy(s2->nta), s2->nta = NULL;
    su_root_destroy(s2->root), s2->root = NULL;
    su_home_unref(s2->home);
    s2 = NULL;
  }

  s2_dns_teardown();
  s2_sip_teardown();
  s2_teardown();
}
