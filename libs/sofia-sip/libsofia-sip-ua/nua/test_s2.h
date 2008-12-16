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

#ifndef S2TESTER_H
#define S2TESTER_H

#define TP_STACK_T struct tester
#define SU_ROOT_MAGIC_T struct tester

#include <sofia-sip/su_wait.h>
#include <sofia-sip/sip.h>
#include <sofia-sip/tport.h>
#include <sofia-sip/nua.h>

struct tester
{
  su_home_t home[1];

  su_root_t *root;
  msg_mclass_t const *mclass;
  int flags;

  char const *hostname;
  tport_t *master;

  sip_to_t *local;
  sip_contact_t *contact;
  struct {
    sip_contact_t *contact;
    tport_t *tport;
  } udp, tcp, tls;

  struct message {
    struct message *next, **prev;
    msg_t *msg;
    sip_t *sip;
    tport_t *tport;
    su_time_t when;
  } *received;

  struct {
    su_socket_t socket;
    su_wait_t wait[1];
    int reg;
  } dns;

  nua_t *nua;

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

  unsigned long tid;

  /* Settings */
  int server_uses_rport;
};

struct dialog
{
  su_home_t home[1];
  sip_from_t *local;
  sip_to_t *remote;
  sip_call_id_t *call_id;
  uint32_t lseq, rseq;
  sip_contact_t *target;
  sip_route_t *route;
  sip_contact_t *contact;

  tport_t *tport;
  msg_t *invite;		/* latest invite sent */
};

extern struct tester *s2;
extern tp_stack_class_t const s2_stack[1];

extern unsigned s2_default_registration_duration;
extern char const s2_auth_digest_str[];
extern char const s2_auth_credentials[];

extern char const s2_auth2_digest_str[];
extern char const s2_auth2_credentials[];

extern char const s2_auth3_digest_str[];
extern char const s2_auth3_credentials[];

extern int s2_nua_thread;

void s2_fast_forward(unsigned long seconds);

void s2_case(char const *tag,
	    char const *title,
	    char const *description);

struct event *s2_remove_event(struct event *);
void s2_free_event(struct event *);
void s2_flush_events(void);

struct event *s2_next_event(void);
struct event *s2_wait_for_event(nua_event_t event, int status);
int s2_check_event(nua_event_t event, int status);
int s2_check_callstate(enum nua_callstate state);

struct message *s2_remove_message(struct message *m);
void s2_free_message(struct message *m);
void s2_flush_messages(void);

struct message *s2_next_response(void);
struct message *s2_wait_for_response(int status, sip_method_t , char const *);
int s2_check_response(int status, sip_method_t method, char const *name);

struct message *s2_next_request(void);
struct message *s2_wait_for_request(sip_method_t method, char const *name);
int s2_check_request(sip_method_t method, char const *name);

#define SIP_METHOD_UNKNOWN sip_method_unknown, NULL

struct message *s2_respond_to(struct message *m, struct dialog *d,
			      int status, char const *phrase,
			      tag_type_t tag, tag_value_t value, ...);

int s2_request_to(struct dialog *d,
		  sip_method_t method, char const *name,
		  tport_t *tport,
		  tag_type_t tag, tag_value_t value, ...);

int s2_update_dialog(struct dialog *d, struct message *response);

int s2_save_register(struct message *m);

void s2_flush_all(void);

void s2_setup_base(char const *hostname);
void s2_setup_logs(int level);
void s2_setup_tport(char const * const *protocols,
		    tag_type_t tag, tag_value_t value, ...);
void s2_teardown(void);

nua_t *s2_nua_setup(tag_type_t tag, tag_value_t value, ...);
void s2_nua_teardown(void);

void s2_register_setup(void);
void s2_register_teardown(void);

#endif
