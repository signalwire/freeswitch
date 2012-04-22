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

#ifndef NTA_COMPAT_H
/** Defined when <nta_compat.h> has been included. */
#define NTA_COMPAT_H;

/**@file nta_compat.h   
 * @brief Deprecated NTA functions and types.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Sep  4 15:54:57 2001 ppessi
 */

#ifndef NTA_H
#include <sofia-sip/nta.h>
#endif

typedef msg_t nta_msg_t;

sip_param_t nta_agent_set_branch(nta_agent_t *agent, sip_param_t branch);

sip_param_t nta_agent_tag(nta_agent_t const *a);

int nta_agent_set_uas(nta_agent_t *agent, int value);

int nta_agent_set_proxy(nta_agent_t *agent, url_string_t const *u);

int nta_msg_send(nta_agent_t *agent, msg_t *msg, 
		 url_string_t const *route_url,
		 void *extra, ...);

int nta_msg_reply(nta_agent_t *self, 
		  msg_t *request_msg,
		  int status, char const *phrase,
		  void *extra, ...);

nta_leg_t *nta_msg_leg(nta_agent_t *agent,  
		       msg_t *msg,
		       nta_request_f *req_callback,
		       nta_leg_magic_t *magic, ...);

nta_leg_t *nta_leg_create(nta_agent_t *agent,  
			  nta_request_f *req_callback,
			  nta_leg_magic_t *magic,
			  sip_call_id_t const *i,
			  sip_from_t const *from,
			  sip_to_t const *to,
			  void const *extra, ...);

int nta_leg_branch(nta_leg_t *leg);

int nta_leg_route(nta_leg_t *, sip_record_route_t const *, 
		  sip_contact_t const *, url_string_t const *);

int nta_incoming_reply(nta_incoming_t *irq, 
		       int status, char const *phrase, 
		       void const *extra, ...);

int nta_incoming_forward(nta_incoming_t *ireq,
			 nta_outgoing_t *request,
			 sip_t const *sip,
			 void const *extra, ...);

int nta_incoming_tforward(nta_incoming_t *ireq,
			  nta_outgoing_t *request,
			  sip_t const *sip,
			  tag_type_t tag, tag_value_t value, ...);

nta_outgoing_t *nta_outgoing_create(nta_leg_t *leg, 
				    nta_response_f *callback,
				    nta_outgoing_magic_t *magic,
				    url_string_t const *route_url, 
				    sip_method_t method, 
				    char const *method_name,
				    url_string_t const *req_url,
				    void const *extra_headers, ...);

/** Create a new outgoing request with old contents, but new url */
nta_outgoing_t *nta_outgoing_fork(nta_outgoing_t *,
				  nta_response_f *callback,
				  nta_outgoing_magic_t *magic,
				  url_string_t const *route_url,
				  url_string_t const *request_url,
				  void const *extra, ...);

nta_outgoing_t *nta_outgoing_forward(nta_leg_t *leg,
				     nta_response_f *callback,
				     nta_outgoing_magic_t *magic,
				     url_string_t const *route_url,
				     url_string_t const *request_url,
				     nta_incoming_t *ireq,
				     sip_t const *sip,
				     void const *extra, ...);

nta_outgoing_t *nta_outgoing_tclone(nta_agent_t *agent,
				    nta_response_f *callback,
				    nta_outgoing_magic_t *magic,
				    url_string_t const *route_url,
				    msg_t *parent,
				    tag_type_t tag, tag_value_t value, ...);

nta_outgoing_t *nta_outgoing_tbye(nta_outgoing_t *orq,
				  nta_response_f *callback,
				  nta_outgoing_magic_t *magic,
				  url_string_t const *route_url,
				  tag_type_t tag, tag_value_t value, ...);

/** Process message statefully using @a leg. */
int nta_leg_stateful(nta_leg_t *leg, msg_t *msg);

typedef nta_ack_cancel_f nta_incoming_f;

#define nta_incoming_request  nta_incoming_getrequest
#define nta_outgoing_response nta_outgoing_getresponse 

#define nta_get_params nta_agent_get_params
#define nta_set_params nta_agent_set_params

int nta_msg_vsend(nta_agent_t *agent, msg_t *msg, url_string_t const *u,
		  void *extra, va_list headers);

int nta_msg_vreply(nta_agent_t *self, 
		   msg_t *msg,
		   int status, char const *phrase,
		   void *extra, va_list headers);

nta_leg_t *nta_leg_vcreate(nta_agent_t *agent,  
			   nta_request_f *req_callback,
			   nta_leg_magic_t *magic,
			   sip_call_id_t const *i,
			   sip_from_t const *from,
			   sip_to_t const *to,
			   void const *extra, va_list header);

int nta_incoming_vreply(nta_incoming_t *irq, 
			int status, char const *phrase, 
			void const *extra, va_list header);

int nta_incoming_vforward(nta_incoming_t *ireq,
			  nta_outgoing_t *request,
			  sip_t const *sip,
			  void const *extra, va_list header);

nta_outgoing_t *nta_outgoing_vcreate(nta_leg_t *leg,
				     nta_response_f *callback,
				     nta_outgoing_magic_t *magic,
				     url_string_t const *route_url,
				     sip_method_t method,
				     char const *method_name,
				     url_string_t const *request_uri,
				     void const *extra,
				     va_list headers);

nta_outgoing_t *nta_outgoing_vforward(nta_leg_t *leg,
				      nta_response_f *callback,
				      nta_outgoing_magic_t *magic,
				      url_string_t const *route_url,
				      url_string_t const *request_url,
				      nta_incoming_t const *ireq,
				      sip_t const *isip,
				      void const *extra,
				      va_list headers);
nta_outgoing_t *nta_outgoing_vfork(nta_outgoing_t *old_orq,
				   nta_response_f *callback,
				   nta_outgoing_magic_t *magic,
				   url_string_t const *route_url,
				   url_string_t const *request_url,
				   void const *extra, va_list headers);

enum {
  NTA_RETRY_TIMER_INI = NTA_SIP_T1,
  NTA_RETRY_TIMER_MAX = NTA_SIP_T2,
  NTA_LINGER_TIMER = NTA_SIP_T4,
  NTA_RETRY_COUNT = 11,
  NTA_INVITE_COUNT = 7,
};

#define NTATAG_RETRY_TIMER_INI     NTATAG_SIP_T1
#define NTATAG_RETRY_TIMER_INI_REF NTATAG_SIP_T1_REF
#define NTATAG_RETRY_TIMER_MAX     NTATAG_SIP_T2
#define NTATAG_RETRY_TIMER_MAX_REF NTATAG_SIP_T2_REF
#define NTATAG_LINGER_TIMER        NTATAG_SIP_T4
#define NTATAG_LINGER_TIMER_REF    NTATAG_SIP_T4_REF

#define NTATAG_RETRY_COUNT(x)     tag_skip, (tag_value_t)0
#define NTATAG_RETRY_COUNT_REF(x) tag_skip, (tag_value_t)0

#define NTATAG_INVITE_COUNT(x)     tag_skip, (tag_value_t)0
#define NTATAG_INVITE_COUNT_REF(x) tag_skip, (tag_value_t)0

#endif /* !defined(NTA_COMPAT_H) */
