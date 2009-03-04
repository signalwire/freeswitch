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

#ifndef S2SIP_H
#define S2SIP_H

#include <sofia-sip/su_wait.h>
#include <sofia-sip/sip.h>
#include <sofia-sip/tport.h>

#include "s2util.h"

extern struct s2sip
{
  su_home_t home[1];

  su_root_t *root;
  msg_mclass_t const *mclass;
  int flags;

  int server_uses_rport;

  char const *hostname;
  tport_t *master;

  sip_to_t *aor;
  sip_contact_t *contact;

  struct {
    sip_contact_t *contact;
    tport_t *tport;
  } udp, tcp, tls;

  struct {
    sip_to_t *aor;
    sip_contact_t *contact;
    tport_t *tport;
  } sut;

  struct message {
    struct message *next, **prev;
    msg_t *msg;
    sip_t *sip;
    tport_t *tport;
    su_time_t when;
  } *received;

  unsigned long tid;
} *s2sip;

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

extern tp_stack_class_t const s2_sip_stack[1];

char *s2_sip_generate_tag(su_home_t *home);

struct message *s2_sip_remove_message(struct message *m);
void s2_sip_free_message(struct message *m);
void s2_sip_flush_messages(void);

struct message *s2_sip_next_response(void);
struct message *s2_sip_wait_for_response(int status, sip_method_t , char const *);
int s2_sip_check_response(int status, sip_method_t method, char const *name);

struct message *s2_sip_next_request(sip_method_t method, char const *name);
struct message *s2_sip_wait_for_request(sip_method_t method, char const *name);
struct message *s2_sip_wait_for_request_timeout(sip_method_t, char const *,
					    unsigned timeout);
int s2_sip_check_request(sip_method_t method, char const *name);
int s2_sip_check_request_timeout(sip_method_t method, char const *, unsigned timeout);

void s2_sip_save_uas_dialog(struct dialog *d, sip_t *sip);

struct message *s2_sip_respond_to(struct message *m, struct dialog *d,
			      int status, char const *phrase,
			      tag_type_t tag, tag_value_t value, ...);

int s2_sip_request_to(struct dialog *d,
		  sip_method_t method, char const *name,
		  tport_t *tport,
		  tag_type_t tag, tag_value_t value, ...);

int s2_sip_update_dialog(struct dialog *d, struct message *response);

void s2_sip_setup(char const *hostname,
		 char const * const *protocols,
		 tag_type_t tag, tag_value_t value, ...);
void s2_sip_teardown(void);

#endif
