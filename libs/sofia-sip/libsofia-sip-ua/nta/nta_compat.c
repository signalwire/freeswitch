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

/**@CFILE nta_compat.c 
 * @brief Compatibility functions for Nokia SIP Transaction API 
 *
 * These functions are deprecated and should not be used anymore.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Jul 24 22:28:34 2001 ppessi
 */

#include "config.h"

#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include <sofia-sip/su_tagarg.h>

#include "sofia-sip/nta.h"
#include "nta_compat.h"
#include "nta_internal.h"

#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_util.h>

/** Set UAS flag value. 
 * 
 * The function nta_agent_set_uas() is used to set or clear User Agent
 * Server flag.  
 *
 * Currently, the flag determines how the agent handles 2XX replies to an
 * incoming INVITE request.  If flag is set, the agent resends the 2XX final
 * responses to an INVITE.
 *
 * @deprecated Use nta_agent_set_params() and NTATAG_UA() instead.
 */
int nta_agent_set_uas(nta_agent_t *agent, int value)
{
  int retval = 1;

  nta_agent_set_params(agent, NTATAG_UA(value != 0), TAG_END());
  nta_agent_get_params(agent, NTATAG_UA_REF(retval), TAG_END());

  return retval;
}

/** Set branch key.
 *
 * @deprecated Use nta_agent_set_params() and NTATAG_BRANCH_KEY() instead.
 */
msg_param_t nta_agent_set_branch(nta_agent_t *agent, 
				 msg_param_t branch)
{
  msg_param_t retval = "";

  nta_agent_set_params(agent, NTATAG_BRANCH_KEY(branch), TAG_END());
  nta_agent_get_params(agent, NTATAG_BRANCH_KEY_REF(retval), TAG_END());

  return retval;
}

/** Set default proxy. 
 *
 * @deprecated Use nta_agent_set_params() and NTATAG_DEFAULT_PROXY() instead.
 */
int nta_agent_set_proxy(nta_agent_t *agent, url_string_t const *u)
{
  if (agent)
    nta_agent_set_params(agent, NTATAG_DEFAULT_PROXY(u), TAG_END());
  return 0;
}

/**Return default @b Tag.
 *
 * The function nta_agent_tag() returns the default @To @b tag
 * which is used by NTA agent.
 *
 * @param agent NTA agent object
 *
 * @return The default tag used by NTA agent.
 *
 * @deprecated Use nta_agent_newtag() to generate a new, unique tag value.
 *
 * @sa NTATAG_TAG_3261().
 */
msg_param_t nta_agent_tag(nta_agent_t const *agent)
{
  return 
    (agent && agent->sa_2543_tag)
    ? agent->sa_2543_tag + strlen("tag=") 
    : NULL;
}

/** Reply to the request message. */
int nta_msg_reply(nta_agent_t *agent,
		  msg_t *msg,
		  int status, char const *phrase,
		  void *extra, ...)
{
  int retval;
  va_list(headers);
  va_start(headers, extra);
  retval = nta_msg_vreply(agent, msg, status, phrase, extra, headers);
  va_end(headers);
  return retval;
}

/** Reply to the request message (stdarg version of nta_msg_reply()). */
int nta_msg_vreply(nta_agent_t *agent,
		  msg_t *req_msg,
		  int status, char const *phrase,
		  void *extra, va_list headers)
{
  msg_t *reply = nta_msg_create(agent, 0);
  sip_t *sip = sip_object(reply);

  if (sip_add_headers(reply, sip, extra, headers) < 0)
    sip = NULL;

  return nta_msg_mreply(agent, reply, sip,
			status, phrase, req_msg, TAG_END());
}

/** Send the message (stdarg version of nta_msg_send()). */
int nta_msg_vsend(nta_agent_t *agent, msg_t *msg, url_string_t const *u,
		  void *extra, va_list headers)
{
  sip_t *sip = sip_object(msg);

  if (extra && sip_add_headers(msg, sip, extra, headers) < 0) {
    msg_destroy(msg);
    return -1;
  }

  return nta_msg_tsend(agent, msg, u, TAG_END());
}

/** Send the message. */
int nta_msg_send(nta_agent_t *agent, msg_t *msg, url_string_t const *u,
		 void *extra, ...)
{
  int retval;
  va_list headers;
  va_start(headers, extra);
  retval = nta_msg_vsend(agent, msg, u, extra, headers);
  va_end(headers);

  return retval;
}

/**
 * Create a new leg object for incoming request message.
 *
 * @param agent    agent object
 * @param callback function which is called for each 
 *                 incoming request belonging to this leg
 * @param magic    call leg context
 * @param msg      a request message
 *
 * @note The ownership of @a msg will pass back to NTA upon successful call
 * to the function nta_msg_leg(). In other words, if the call to @a
 * nta_msg_leg() is successful, the application may not do anything with @a
 * msg anymore.  Instead of that, NTA will create of a new incoming request
 * object for the @a msg and eventually return the request to application by
 * calling the @a callback function.
 *
 * @deprecated Use nta_leg_stateful() instead.
 */
nta_leg_t *nta_msg_leg(nta_agent_t *agent,
		       msg_t *msg,
		       nta_request_f *callback,
		       nta_leg_magic_t *magic,
		       ...)
{
  nta_leg_t *leg;
  sip_t *sip = sip_object(msg);

  SU_DEBUG_9(("\tnta_msg_leg(): called\n"));

  assert(msg && sip && sip->sip_request);

  if (!msg || !sip || !sip->sip_request || !callback)
    return NULL;

  leg = nta_leg_tcreate(agent, callback, magic,
			SIPTAG_CALL_ID(sip->sip_call_id),
			SIPTAG_FROM(sip->sip_to), /* local address */
			SIPTAG_TO(sip->sip_from), /* remote address */
			TAG_END());
  if (!leg) 
    /* xyzzy */;
  else if (nta_leg_server_route(leg, sip->sip_record_route, 
				sip->sip_contact) < 0)
    nta_leg_destroy(leg), leg = NULL;
  else if (nta_leg_stateful(leg, msg) < 0)
    nta_leg_destroy(leg), leg = NULL;

  SU_DEBUG_9(("\tnta_msg_leg(): returns %p\n", leg));

  return leg;
}

static void sm_leg_recv(su_root_magic_t *rm,
			su_msg_r msg,
			union sm_arg_u *u);

/** Process msg statefully using the leg. */
int nta_leg_stateful(nta_leg_t *leg, msg_t *msg)
{
  su_msg_r su_msg = SU_MSG_RINITIALIZER;
  nta_agent_t *agent = leg->leg_agent;
  su_root_t *root = agent->sa_root;
  struct leg_recv_s *a;

  /* Create a su message that is passed to NTA network thread */
  if (su_msg_create(su_msg,
		    su_root_task(root),
		    su_root_task(root),
		    sm_leg_recv, /* Function to call */
		    sizeof(struct leg_recv_s)) == SU_FAILURE)
    return -1;

  agent->sa_stats->as_trless_to_tr++;

  a = su_msg_data(su_msg)->a_leg_recv;

  a->leg = leg;
  a->msg = msg;

  a->tport = tport_incref(tport_delivered_by(agent->sa_tports, msg));

  return su_msg_send(su_msg);
}

/** @internal Delayed leg_recv(). */
static
void sm_leg_recv(su_root_magic_t *rm,
		 su_msg_r msg,
		 union sm_arg_u *u)
{
  struct leg_recv_s *a = u->a_leg_recv;
  leg_recv(a->leg, a->msg, sip_object(a->msg), a->tport);
  tport_decref(&a->tport);
}

/**Create a new leg object
 *
 * @param agent    agent object
 * @param callback function which is called for each
 *                 incoming request belonging to this leg
 * @param magic    call leg context
 * @param i        optional @CallID
 *                 (if @c NULL, an ID generated by @b NTA is used)
 * @param from     optional @From (local address)
 * @param to       optional @To (remote address)
 * @param extra, ... optional extra headers, terminated with a @c NULL
 *
 * @deprecated Use nta_leg_tcreate() instead.
 */
nta_leg_t *nta_leg_create(nta_agent_t *agent,
			  nta_request_f *callback,
			  nta_leg_magic_t *magic,
			  sip_call_id_t const *i,
			  sip_from_t const *from,
			  sip_to_t const *to,
			  void const *extra, ...)
{
  nta_leg_t *leg;
  va_list headers;
  va_start(headers, extra);
  leg = nta_leg_vcreate(agent, callback, magic,
			i, from, to,
			extra, headers);
  va_end(headers);
  return leg;
}

/**
 * Create a new leg object
 *
 * @param agent    agent object
 * @param callback function which is called for each
 *                 incoming request belonging to this leg
 * @param magic    call leg context
 * @param i        optional @CallID
 *                 (if @c NULL, an ID generated by @b NTA is used)
 * @param from     optional @From (local address)
 * @param to       optional @To (remote address)
 * @param extra    optional extra header
 * @param headers  va_list of optional extra headers
 *
 * @deprecated Use nta_leg_tcreate() instead.
 */
nta_leg_t *nta_leg_vcreate(nta_agent_t *agent,
			   nta_request_f *callback,
			   nta_leg_magic_t *magic,
			   sip_call_id_t const *i,
			   sip_from_t const *from,
			   sip_to_t const *to,
			   void const *extra, va_list headers)
{
  sip_route_t const *route = NULL;
  sip_cseq_t const *cseq = NULL;

  for (; extra ; extra = va_arg(headers, void *)) {
    sip_header_t const *h = (sip_header_t const *)extra;

    if (h == SIP_NONE)
      continue;
    else if (sip_call_id_p(h)) {
      if (i == NULL) i = h->sh_call_id;
    }
    else if (sip_from_p(h)) {
      if (from == NULL) from = h->sh_from;
    }
    else if (sip_to_p(h)) {
      if (to == NULL) to = h->sh_to;
    }
    else if (sip_route_p(h)) {
      route = h->sh_route;
    }
    else if (sip_cseq_p(h)) {
      cseq = h->sh_cseq;
    }
    else {
      SU_DEBUG_3(("nta_leg_create: extra header %s\n", 
		  sip_header_name(h, 0)));
    }
  }

  return nta_leg_tcreate(agent, callback, magic,
			 NTATAG_NO_DIALOG(i == SIP_NONE->sh_call_id),
			 TAG_IF(i != SIP_NONE->sh_call_id, SIPTAG_CALL_ID(i)),
			 TAG_IF(from != SIP_NONE->sh_from, SIPTAG_FROM(from)),
			 TAG_IF(to != SIP_NONE->sh_to, SIPTAG_TO(to)),
			 SIPTAG_ROUTE(route),
			 SIPTAG_CSEQ(cseq),
			 TAG_END());
}

/**Mark leg as branchable.
 *
 * This function does currently absolutely nothing.
 * 
 * @param leg leg to be marked branchable.
 *
 * @note Currently, all legs @b are branchable.
 *
 * @deprecated Do not use.
 */
int nta_leg_branch(nta_leg_t *leg)
{
  return 0;
}

/** Add route from final response 
 *
 * @deprecated Use nta_leg_client_route().
 */
int nta_leg_route(nta_leg_t *leg, 
		   sip_record_route_t const *route, 
		   sip_contact_t const *contact,
		   url_string_t const *url)
{
  return nta_leg_client_route(leg, route, contact);
}

#if 0
/**Get response message.
 *
 * The function nta_incoming_getresponse() retrieves a copy of the latest
 * outgoing response message.  The response message is copied; the original
 * copy is kept by the transaction.
 *
 * @param irq incoming (server) transaction handle
 *
 * @retval
 * A pointer to the copy of the response message is returned, or NULL if an
 * error occurred.
 */
msg_t *nta_incoming_getresponse(nta_incoming_t *irq)
{
  if (irq && irq->irq_response) {
    msg_t *msg = nta_msg_create(irq->irq_agent, 0);
    sip_t *sip = sip_object(msg);

    msg_clone(msg, irq->irq_response);

    /* Copy the SIP headers from the old message */
    if (msg_copy_all(msg, sip, sip_object(irq->irq_response)) >= 0)
      return msg;

    msg_destroy(msg);
  }

  return NULL;
}

/**Get request message.
 *
 * The function nta_outgoing_getrequest() retrieves the request message sent
 * to the network. The request message is copied; the original copy is kept
 * by the transaction.
 *
 * @param orq outgoing transaction handle
 *
 * @retval
 * A pointer to the copy of the request message is returned, or NULL if an
 * error occurred.
 */
msg_t *nta_outgoing_getrequest(nta_outgoing_t *orq)
{
  if (orq && orq->orq_request) {
    msg_t *msg = nta_msg_create(orq->orq_agent, 0);
    sip_t *sip = sip_object(msg);

    msg_clone(msg, orq->orq_request);

    /* Copy the SIP headers from the old message */
    if (sip_copy_all(msg, sip, sip_object(orq->orq_request)) >= 0)
      return msg;

    msg_destroy(msg);
  }

  return NULL;
}

/**Get latest response message.
 *
 * The function nta_outgoing_getresponse() retrieves the latest incoming
 * response message to the outgoing transaction.  Note that the message is
 * not copied, but removed from the transaction.
 *
 * @param orq outgoing transaction handle
 *
 * @retval
 * A pointer to response message is returned, or NULL if no response message
 * has been received or the response message has already been retrieved.
 */
msg_t *nta_outgoing_getresponse(nta_outgoing_t *orq)
{
  msg_t *msg = NULL;

  if (orq && orq->orq_response)
    msg = orq->orq_response, orq->orq_response = NULL;

  return msg;
}
#endif

/** Create an outgoing request belonging to the leg.
 *
 * The function nta_outgoing_create() creates an outgoing transaction and
 * sends the request.  The request is sent to the @a route_url (if
 * non-NULL), default proxy (as specified by nta_agent_set_proxy()), or to
 * the address specified by @a request_uri.  In the latest case, all the
 * tranport parameters are stripped from the request_uri.  If no @a
 * request_uri is specified, it is taken from the @To header.
 *
 * When NTA receives response to the request, it invokes the @c callback
 * function.  
 *
 * @param leg         call leg object
 * @param callback    callback function (may be @c NULL)
 * @param magic       application context pointer
 * @param route_url   URL used to route transaction requests
 * @param method      method type
 * @param name        method name 
 * @param request_uri Request-URI
 * @param extra, ...  list of extra headers
 *
 * @return
 * The function nta_outgoing_create() returns a pointer to newly created
 * outgoing transaction object if successful, and NULL otherwise.
 *
 * @deprecated
 * Use nta_outgoing_tcreate() or nta_outgoing_mcreate() instead.
 */
nta_outgoing_t *nta_outgoing_create(nta_leg_t *leg,
				    nta_response_f *callback,
				    nta_outgoing_magic_t *magic,
				    url_string_t const *route_url,
				    sip_method_t method,
				    char const *name,
				    url_string_t const *request_uri,
				    void const *extra, ...)
{
  nta_outgoing_t *orq;
  va_list headers;

  va_start(headers, extra);
  orq = nta_outgoing_vcreate(leg, callback, magic,
			     route_url, method, name, request_uri,
			     extra, headers);
  va_end(headers);
  return orq;
}

/**Create a request belonging to the leg
 * (stdarg version of nta_outgoing_create()). 
 *
 * @deprecated
 * Use nta_outgoing_tcreate() or nta_outgoing_mcreate() instead.
 */
nta_outgoing_t *nta_outgoing_vcreate(nta_leg_t *leg,
				     nta_response_f *callback,
				     nta_outgoing_magic_t *magic,
				     url_string_t const *route_url,
				     sip_method_t method,
				     char const *name,
				     url_string_t const *request_uri,
				     void const *extra,
				     va_list headers)
{
  nta_agent_t *agent = leg->leg_agent;
  msg_t *msg = nta_msg_create(agent, 0);
  sip_t *sip = sip_object(msg);
  nta_outgoing_t *orq;

  if (extra && 
      sip_add_headers(msg, sip, extra, headers) < 0)
    orq = NULL;
  else if (route_url && leg->leg_route && !sip->sip_route &&
	   sip_add_dup(msg, sip, (sip_header_t *)leg->leg_route) < 0)
    orq = NULL;
  else if (nta_msg_request_complete(msg, leg, method, name, request_uri) < 0)
    orq = NULL;
  else
    orq = nta_outgoing_mcreate(agent, callback, magic, route_url, msg);

  if (!orq)
    msg_destroy(msg);

  return orq;
}

/**Forward a request.
 *
 * @deprecated
 * Use nta_outgoing_mcreate() instead.
 */
nta_outgoing_t *nta_outgoing_tclone(nta_agent_t *agent,
				    nta_response_f *callback,
				    nta_outgoing_magic_t *magic,
				    url_string_t const *route_url,
				    msg_t *parent,
				    tag_type_t tag, tag_value_t value, ...)
{
  ta_list ta;
  msg_t *msg;
  nta_outgoing_t *orq = NULL;

  if (parent == NULL)
    return NULL;
  if ((msg = nta_msg_create(agent, 0)) == NULL)
    return NULL;

  ta_start(ta, tag, value);

  msg_clone(msg, parent);

  if (parent && sip_copy_all(msg, sip_object(msg), sip_object(parent)) < 0)
    ;
  else if (sip_add_tl(msg, sip_object(msg), ta_tags(ta)) < 0)
    ;
  else
    orq = nta_outgoing_mcreate(agent, callback, magic, route_url, msg); 

  ta_end(ta);

  if (!orq)
    msg_destroy(msg);

  return orq;
  
}

/** Forward a request belonging to the leg.
 * 
 * The function nta_outgoing_forward() will create an outgoing transaction
 * based on the incoming transaction.  The forwarded message is specified by
 * @a isip parameter.  The caller rewrite the URL by giving non-NULL @a
 * request_url. 
 *
 * The request is sent to the server specified by @a route_url if it is
 * non-NULL.
 *
 * @deprecated
 * Use nta_outgoing_mcreate() instead.
 */
nta_outgoing_t *nta_outgoing_forward(nta_leg_t *leg,
				     nta_response_f *callback,
				     nta_outgoing_magic_t *magic,
				     url_string_t const *route_url,
				     url_string_t const *request_uri,
				     nta_incoming_t *ireq,
				     sip_t const *isip,
				     void const *extra, ...)
{
  nta_outgoing_t *orq;
  va_list headers;

  va_start(headers, extra);
  orq = nta_outgoing_vforward(leg, callback, magic, route_url, request_uri,
			      ireq, isip, extra, headers);
  va_end(headers);
  return orq;
}

/**Forward a request belonging to the leg 
 * (stdarg version of nta_outgoing_forward()).
 *
 * @deprecated
 * Use nta_outgoing_mcreate() instead.
 */
nta_outgoing_t *nta_outgoing_vforward(nta_leg_t *leg,
				      nta_response_f *callback,
				      nta_outgoing_magic_t *magic,
				      url_string_t const *route_url,
				      url_string_t const *request_uri,
				      nta_incoming_t const *ireq,
				      sip_t const *isip,
				      void const *extra,
				      va_list headers)
{
  nta_agent_t *agent = leg->leg_agent;
  nta_outgoing_t *orq = NULL;
  msg_t *msg, *imsg;
  sip_t *sip;
  su_home_t *home;

  assert(leg); assert(ireq); 

  if (isip == NULL) 
    imsg = ireq->irq_request, isip = sip_object(ireq->irq_request);
  else if (isip == sip_object(ireq->irq_request))
    imsg = ireq->irq_request;
  else if (isip == sip_object(ireq->irq_request2))
    imsg = ireq->irq_request2;
  else {
    SU_DEBUG_3(("nta_outgoing_forward: invalid arguments\n"));
    return NULL;
  }

  assert(isip); assert(isip->sip_request);

  if (!route_url)
    route_url = (url_string_t *)agent->sa_default_proxy;

  if (!(msg = nta_msg_create(agent, 0)))
    return NULL;

  msg_clone(msg, imsg);

  sip = sip_object(msg); 
  home = msg_home(msg);
  
  /* Copy the SIP headers from the @c imsg message */
  do {
    if (sip_copy_all(msg, sip, isip) < 0)
      break;
    if (sip_add_headers(msg, sip, extra, headers) < 0)
      break;
    if (!route_url && sip->sip_route) {
      request_uri = (url_string_t *)sip->sip_route->r_url;
      if (!sip_route_remove(msg, sip))
	break;
    }
    if (request_uri) {
      sip_request_t *rq;

      rq = sip_request_create(home,
			      sip->sip_request->rq_method, 
			      sip->sip_request->rq_method_name, 
			      request_uri,
			      NULL);

      if (!rq || sip_header_insert(msg, sip, (sip_header_t *)rq) < 0)
	break;
    }
    
    if ((orq = nta_outgoing_mcreate(agent, callback, magic, route_url, msg)))
      return orq;

  } while (0);

  msg_destroy(msg);
  return NULL;
}


/** Fork an outgoing request toward another destination.
 *
 * @deprecated
 * Use nta_outgoing_mcreate() instead.
 */
nta_outgoing_t *nta_outgoing_fork(nta_outgoing_t *old_orq,
				  nta_response_f *callback,
				  nta_outgoing_magic_t *magic,
				  url_string_t const *route_url,
				  url_string_t const *request_uri,
				  void const *extra, ...)
{
  nta_outgoing_t *orq;
  va_list headers;

  va_start(headers, extra);
  orq = nta_outgoing_vfork(old_orq, callback, magic, route_url,
			   request_uri, extra, headers);
  va_end(headers);
  return orq;
}

/** Fork an outgoing request (stdarg version of nta_outgoing_fork()). 
 *
 * @deprecated
 * Use nta_outgoing_mcreate() instead.
 */
nta_outgoing_t *nta_outgoing_vfork(nta_outgoing_t *old_orq,
				   nta_response_f *callback,
				   nta_outgoing_magic_t *magic,
				   url_string_t const *route_url,
				   url_string_t const *request_uri,
				   void const *extra,
				   va_list headers)
{
  nta_outgoing_t * orq;
  msg_t *msg, *imsg;
  sip_t *sip, *isip;
  nta_agent_t *agent;
  su_home_t *home;

  if (!old_orq || !old_orq->orq_request || !request_uri)
    return NULL;

  agent = old_orq->orq_agent;
  imsg = old_orq->orq_request;
  
  if (!(msg = nta_msg_create(agent, 0)))
    return NULL;

  msg_clone(msg, imsg);

  sip = sip_object(msg); isip = sip_object(imsg);
  home = msg_home(msg);

  /* Copy the SIP headers from the imsg message */
  if (sip_copy_all(msg, sip, isip) < 0)
    orq = NULL;
  else if (sip_via_remove(msg, sip) == NULL)
    orq = NULL;
  else if (sip_add_dup(msg, sip_object(msg), 
		       (sip_header_t const *)
		       sip_request_create(home,
					  sip->sip_request->rq_method, 
					  sip->sip_request->rq_method_name, 
					  request_uri,
					  NULL)) < 0)
    orq = NULL;
  else if (sip_add_headers(msg, sip, extra, headers) < 0)
    orq = NULL;
  else
    orq = nta_outgoing_mcreate(agent, callback, magic, route_url, msg);

  if (!orq)
    msg_destroy(msg);

  return orq;
}

/**
 * Reply to an incoming transaction request.
 *
 * This function creates and sends a response message to an incoming request.
 * It is possible to send several non-final (1xx) responses, but only one
 * final response.
 * 
 * @param irq    incoming request
 * @param status status code
 * @param phrase status phrase (may be NULL if status code is well-known)
 * @param extra, ...    optional additional headers terminated by NULL
 *
 * @deprecated
 * Use nta_incoming_treply() instead.
 */
int nta_incoming_reply(nta_incoming_t *irq,
		       int status,
		       char const *phrase,
		       void const *extra,
		       ...)
{
  int retval;
  va_list headers;
  va_start(headers, extra);
  retval = nta_incoming_vreply(irq, status, phrase, extra, headers);
  va_end(headers);
  return retval;
}

/**Reply to an incoming transaction request (stdarg version).
 *
 * @deprecated
 * Use nta_incoming_treply() instead.
 */
int nta_incoming_vreply(nta_incoming_t *irq,
			int status,
			char const *phrase,
			void const *extra, va_list headers)
{
  if (irq->irq_status < 200 || status < 200 || 
      (irq->irq_method == sip_method_invite && status < 300)) {
    msg_t *msg = nta_msg_create(irq->irq_agent, 0);
    sip_t *sip = sip_object(msg);

    if (!msg) 
      return -1;
    else if (nta_msg_response_complete(msg, irq, status, phrase) < 0) 
      msg_destroy(msg);      
    else if (sip_add_headers(msg, sip, extra, headers) < 0 )
      msg_destroy(msg);
    else if (sip_message_complete(msg) < 0)
      msg_destroy(msg);      
    else if (nta_incoming_mreply(irq, msg) < 0)
      msg_destroy(msg);
    else
      return 0;
  }

  return -1;
}


/**
 * Forward a response to incoming transaction.
 *
 * This function forwards a response message from outgoing request to an
 * incoming request.  It copies the message save the first via field, and
 * send the response message to the address specified in the second via.
 *
 * It is possible to send several non-final (1xx) responses, but only one
 * final response.
 *
 * @param irq    incoming request
 * @param orq
 * @param sip    message structure for outgoing transaction
 * @param extra, ...  list of optional additional headers terminated by NULL
 *
 * @bug Adding extra headers is unimplemented. 
 *
 * @deprecated Use nta_incoming_mreply() instead.
 */
int nta_incoming_forward(nta_incoming_t *irq,
			 nta_outgoing_t *orq,
			 sip_t const *sip,
			 void const *extra, ...)
{
  msg_t *msg = nta_outgoing_response(orq);
  int status;

  if (irq == NULL || sip == NULL || msg == NULL || sip != sip_object(msg))
    return -1;

  status = sip->sip_status->st_status;

  sip_via_remove(msg, (sip_t *)sip);  /* Remove topmost via */

  if (sip_message_complete(msg) < 0)
    msg_destroy(msg);      
  if (nta_incoming_mreply(irq, msg) < 0)
    msg_destroy(msg);
  else
    return 0;

  return -1;
}

/** Send a BYE to an INVITE.
 *
 * @deprecated
 * This function should used only if application requires 
 * RFC2543 compatibility.
 */
nta_outgoing_t *nta_outgoing_tbye(nta_outgoing_t *orq,
				  nta_response_f *callback,
				  nta_outgoing_magic_t *magic,
				  url_string_t const *route_url,
				  tag_type_t tag, tag_value_t value, ...)
{
  msg_t *msg;
  sip_t *sip, *inv;
  sip_cseq_t *cs;
  sip_request_t *rq;
  su_home_t *home;
  url_string_t *url;

  if (orq == NULL || orq->orq_method != sip_method_invite)
    return NULL;

  inv = sip_object(orq->orq_request);
  msg = nta_msg_create(orq->orq_agent, 0);
  home = msg_home(msg);
  sip = sip_object(msg);

  if (inv == NULL || sip == NULL) {
    msg_destroy(msg);
    return NULL;
  }

  sip_add_tl(msg, sip,
	     SIPTAG_TO(inv->sip_to),
	     SIPTAG_FROM(inv->sip_from),
	     SIPTAG_CALL_ID(inv->sip_call_id),
	     SIPTAG_ROUTE(inv->sip_route),
	     TAG_END());

  url = (url_string_t *)inv->sip_request->rq_url;

  rq = sip_request_create(home, SIP_METHOD_BYE, url, NULL);
  sip_header_insert(msg, sip, (sip_header_t*)rq);

  cs = sip_cseq_create(home, inv->sip_cseq->cs_seq + 1, SIP_METHOD_BYE);
  sip_header_insert(msg, sip, (sip_header_t*)cs);

  return nta_outgoing_mcreate(orq->orq_agent, callback, magic, 
			      route_url, msg);
}
