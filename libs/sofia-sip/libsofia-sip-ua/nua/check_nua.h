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

#ifndef CHECK_NUA_H

#include <sofia-sip/su_wait.h>
#include <sofia-sip/sip.h>
#include <sofia-sip/tport.h>
#include <sofia-sip/nua.h>
#include <sofia-sip/su_string.h>

#include "s2base.h"
#include "s2util.h"
#include "s2sip.h"

struct s2nua
{
  su_home_t home[1];

  nua_t *nua;

  int shutdown;

  struct event {
    struct event *next, **prev;
    nua_saved_event_t event[1];
    nua_handle_t *nh;
    nua_event_data_t const *data;
    su_time_t when;
  } *events;

  struct {
    nua_handle_t *nh;
    sip_to_t *aor;
    sip_contact_t *contact;
    tport_t *tport;
  } registration[1];
};

extern unsigned s2_default_registration_duration;
extern char const s2_auth_digest_str[];
extern char const s2_auth_credentials[];

extern char const s2_auth2_digest_str[];
extern char const s2_auth2_credentials[];

extern char const s2_auth3_digest_str[];
extern char const s2_auth3_credentials[];

extern int s2_nua_thread;

extern struct s2nua *s2;

void s2_setup_logs(int level);

struct event *s2_remove_event(struct event *);
void s2_free_event(struct event *);
void s2_flush_events(void);

struct event *s2_next_event(void);
struct event *s2_wait_for_event(nua_event_t event, int status);
int s2_check_event(nua_event_t event, int status);
int s2_check_callstate(enum nua_callstate state);
int s2_check_substate(struct event *e, enum nua_substate state);

#define fail_unless_event(event, status) \
  fail_unless(s2_check_event(event, status))

#define SIP_METHOD_UNKNOWN sip_method_unknown, NULL

void s2_flush_all(void);

nua_t *s2_nua_setup(char const *label, tag_type_t tag, tag_value_t value, ...);

void s2_nua_teardown(void);

void s2_nua_fast_forward(unsigned long seconds,
			 su_root_t *steproot);

int s2_save_register(struct message *m);

void s2_register_setup(void);
void s2_register_teardown(void);

#include <s2check.h>

void check_session_cases(Suite *suite, int threading);
void check_register_cases(Suite *suite, int threading);
void check_etsi_cases(Suite *suite, int threading);
void check_simple_cases(Suite *suite, int threading);

#endif

