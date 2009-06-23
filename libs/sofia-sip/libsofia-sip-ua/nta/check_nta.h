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
 */

#ifndef CHECK_NTA_H
#define CHECK_NTA_H

#include <s2check.h>

#include <sofia-sip/sip.h>
#include <sofia-sip/tport.h>
#include <sofia-sip/nta.h>

#include <stdarg.h>

#include "s2sip.h"

extern struct s2nta {
  su_home_t home[1];

  nta_agent_t *nta;

  su_root_t *root;

  nta_leg_t *default_leg;

  struct event {
    struct event *next, **prev;

    nta_agent_magic_t *amagic;

    nta_outgoing_magic_t *omagic;
    nta_outgoing_t *orq;

    nta_leg_magic_t *lmagic;
    nta_leg_t *leg;

    nta_incoming_magic_t *imagic;
    nta_incoming_t *irq;

    sip_method_t method;
    char const *method_name;

    int status;
    char const *phrase;

    msg_t *msg;
    sip_t *sip;
  } *events;
} *s2;

struct event *s2_nta_remove_event(struct event *e);
void s2_nta_free_event(struct event *e);
void s2_nta_flush_events(void);
struct event *s2_nta_next_event(void);

enum wait_for {
  wait_for_amagic = 1,
  wait_for_omagic,
  wait_for_orq,
  wait_for_lmagic,
  wait_for_leg,
  wait_for_imagic,
  wait_for_irq,
  wait_for_method,
  wait_for_method_name,
  wait_for_status,
  wait_for_phrase
};

struct event *s2_nta_vwait_for(enum wait_for,
			       void const *value,
			       va_list va);

struct event *s2_nta_wait_for(enum wait_for,
			      void const *value,
			      ...);

int s2_nta_check_for(enum wait_for,
		     void const *value,
		     ...);

int s2_nta_msg_callback(nta_agent_magic_t *magic,
			nta_agent_t *nta,
			msg_t *msg,
			sip_t *sip);
int s2_nta_orq_callback(nta_outgoing_magic_t *magic,
			nta_outgoing_t *orq,
			sip_t const *sip);
int s2_nta_leg_callback(nta_leg_magic_t *magic,
			nta_leg_t *leg,
			nta_incoming_t *irq,
			sip_t const *sip);
int s2_nta_irq_callback(nta_incoming_magic_t *magic,
			nta_incoming_t *irq,
			sip_t const *sip);

void s2_nta_setup_logs(int level);
void s2_nta_setup(char const *label,
		  char const * const *transports,
		  tag_type_t tag, tag_value_t value, ...);

nta_agent_t *s2_nta_agent_setup(url_string_t const *bind_url,
				nta_message_f *callback,
				nta_agent_magic_t *magic,
				tag_type_t tag, tag_value_t value, ...);
void s2_nta_teardown(void);

TCase *check_nta_api_1_0(void);
TCase *check_nta_client_2_0(void);
TCase *check_nta_client_2_1(void);
TCase *check_nta_client_2_2(void);

#endif
