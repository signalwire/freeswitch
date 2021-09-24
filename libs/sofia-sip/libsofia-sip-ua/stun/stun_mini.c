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

/**@internal
 * @file stun_mini.c
 * @brief Minimal stun server
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Tat Chan <Tat.Chan@nokia.com>
 * @author Kai Vehmanen <kai.vehmanen@nokia.com>
 *
 * @date Created: Fri Oct  3 13:40:41 2003 ppessi
 *
 */

#include "config.h"

#include "stun_internal.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

typedef struct stun_bound_s stun_bound_t;

struct stun_bound_s
{
  stun_bound_t *ss_next;
  su_socket_t ss_socket;
  int ss_scope;			/* LI_SCOPE */
  socklen_t ss_addrlen;
  union {
    struct sockaddr_in sin[1];
    struct sockaddr_storage storage[1];
    char array[sizeof (struct sockaddr_storage)];
  } ss_addr;
};

struct stun_mini_s
{
  stun_bound_t *sockets;
};

static int process_3489_request(stun_mini_t *mini,
				stun_msg_t *request, stun_msg_t *response,
				su_socket_t socket, void *, socklen_t);

static int process_bis_request(stun_mini_t *mini,
			       stun_msg_t *request, stun_msg_t *response,
			       su_socket_t socket, void *, socklen_t);

int send_stun_error(stun_msg_t *response,
		    int error,
		    int socket,
		    void *transaction_id,
		    void *from,
		    socklen_t fromlen);

/** Create a stun miniserver */
stun_mini_t *stun_mini_create(void)
{
  return calloc(1, sizeof (stun_mini_t));
}

/** Destroy a stun miniserver */
void stun_mini_destroy(stun_mini_t *mini)
{
  if (mini) {
    stun_bound_t *ss, **next;
    for (next = &mini->sockets; *next; ) {
      ss = *next;
      *next = ss->ss_next;
      free(ss);
    }
    free(mini);
  }
}

/** Add a socket to stun miniserver. */
int stun_mini_add_socket(stun_mini_t *mini, su_socket_t socket)
{
  stun_bound_t *ss, **next;
  struct sockaddr_storage addr[1];
  socklen_t addrlen = sizeof addr;

  if (mini == NULL)
    return su_seterrno(EFAULT);

  for (next = &mini->sockets; *next; next = &(*next)->ss_next)
    if (socket == (*next)->ss_socket)
      return su_seterrno(EEXIST);

  if (getsockname(socket, (void *)addr, &addrlen) < 0)
    return -1;

  if (addr->ss_family != AF_INET)
    return su_seterrno(EAFNOSUPPORT);

  ss = calloc(1, offsetof(stun_bound_t, ss_addr.array[addrlen]));

  ss->ss_socket = socket;
  ss->ss_scope = su_sockaddr_scope((void *)addr, addrlen);

  memcpy(ss->ss_addr.array, addr, ss->ss_addrlen = addrlen);

  *next = ss;

  return 0;
}

/** Remove socket from stun miniserver */
int stun_mini_remove_socket(stun_mini_t *mini, su_socket_t socket)
{
  stun_bound_t *ss, **next;

  if (mini == NULL)
    return errno = EFAULT, -1;

  for (next = &mini->sockets; *next; next = &(*next)->ss_next)
    if (socket == (*next)->ss_socket) {
      ss = *next;
      *next = ss->ss_next;
      free(ss);
      return 0;
    }

  return errno = ENOENT, -1;
}

void stun_mini_request(stun_mini_t *mini,
		       su_socket_t socket,
		       void *msg, ssize_t msglen,
		       void *from, socklen_t fromlen)
{
  int error;
  struct {
    stun_msg_t in[1];
    stun_msg_t out[1];
    stun_msg_t error[1];
  } m;
  char const *verdict = NULL;
  uint8_t *data = msg;
  char buffer[80];
  uint8_t const magic_cookie[4] = { 0x21, 0x12, 0xA4, 0x42 };

  memset(&m, 0, sizeof m);

  if (mini == NULL || msg == NULL || from == NULL)
    return;

  if (msglen < 20)
    verdict = "runt";
  else if (data[0] == 1)
    verdict = "response";
  else if (data[0] != 0)
    verdict = "garbage";
  else if (data[1] == 2)
    verdict = "shared secret request";
  else if (data[1] != 1)
    verdict = "garbage";

  {
    struct sockaddr_in const *sin = from;

    if (sin->sin_family == AF_INET)
      su_inet_ntop(sin->sin_family, &sin->sin_addr, buffer, sizeof buffer);
    else
      sprintf(buffer, "<af=%u>", (unsigned)sin->sin_family);

    fprintf(stderr, "stun %s from %s:%u\n",
	    verdict ? verdict : "request", buffer, ntohs(sin->sin_port));

    if (verdict)
      return;
  }

  m.in->enc_buf.data = msg;
  m.in->enc_buf.size = msglen;

  if (memcmp(data + 4, magic_cookie, sizeof magic_cookie) == 0)
    error = process_3489_request(mini, m.in, m.out, socket, from, fromlen);
  else
    error = process_bis_request(mini, m.in, m.out, socket, from, fromlen);

  if (error)
    send_stun_error(m.error, error, socket, data + 4, from, fromlen);

  m.in->enc_buf.data = NULL;

  stun_free_message(m.in);
  stun_free_message(m.out);
  stun_free_message(m.error);
}

static
int process_3489_request(stun_mini_t *mini,
			 stun_msg_t *request,
			 stun_msg_t *response,
			 su_socket_t socket,
			 void *from,
			 socklen_t fromlen)
{
  stun_bound_t *ss, *changed = NULL, ss0[1];
  stun_attr_t *a, **next;
  stun_attr_sockaddr_t *addr;
  int change_address = 0;

  if (stun_parse_message(request) < 0) {
    fprintf(stderr, "stun: error parsing request\n");
    return STUN_400_BAD_REQUEST;
  }

  if (request->stun_hdr.msg_type != BINDING_REQUEST) {
    fprintf(stderr, "stun: not binding request\n");
    return 0;
  }

  a = stun_get_attr(request->stun_attr, RESPONSE_ADDRESS);
  if (a)
    return STUN_600_GLOBAL_FAILURE;

  /* compose header */
  response->stun_hdr.msg_type = BINDING_RESPONSE;
  memcpy(response->stun_hdr.tran_id, request->stun_hdr.tran_id,
	 sizeof response->stun_hdr.tran_id);

  next = &response->stun_attr;

  /* MAPPED-ADDRESS */
  a = malloc(sizeof *a); if (!a) return STUN_500_SERVER_ERROR;
  a->attr_type = MAPPED_ADDRESS;
  addr = malloc(sizeof *addr); if (!addr) return STUN_500_SERVER_ERROR;
  memcpy(addr, from, sizeof *addr);
  a->pattr = addr;
  a->next = NULL;
  *next = a; next = &a->next;

  /* SOURCE-ADDRESS */ /* depends on CHANGE_REQUEST */
  a = stun_get_attr(request->stun_attr, CHANGE_REQUEST);
  if (a)
    change_address = ((stun_attr_changerequest_t *)a->pattr)->value;

  if (change_address) {
    struct sockaddr_in const *sin, *sin2;
    int scope = su_sockaddr_scope(from, fromlen);
    stun_bound_t *changed_ip = NULL, *same_scope = NULL;

    sin = from;

    for (changed = mini->sockets; changed; changed = changed->ss_next) {
      sin2 = changed->ss_addr.sin;

      if (scope != LI_SCOPE_HOST && changed->ss_scope == LI_SCOPE_HOST)
	continue;

      if (scope != LI_SCOPE_SITE && changed->ss_scope == LI_SCOPE_SITE)
	continue;

      if (same_scope == NULL)
	same_scope = changed;

      if (change_address & STUN_CR_CHANGE_IP)
	if (!memcmp(&sin->sin_addr, &sin2->sin_addr, sizeof sin->sin_addr))
	  continue;

      if (changed_ip == NULL)
	changed_ip = changed;

      if (change_address & STUN_CR_CHANGE_PORT)
	if (sin->sin_port == sin2->sin_port)
	  continue;
    }

    if (changed == NULL && (change_address & STUN_CR_CHANGE_IP))
      /* We don't have socekt with both changed port and ip */
      changed = changed_ip;

    if (changed == NULL)
      changed = same_scope;
  }

  for (ss = mini->sockets; ss; ss = ss->ss_next)
    if (socket == ss->ss_socket)
      break;

  if (ss == NULL) {
    memset(ss = ss0, 0, sizeof ss0);
    ss->ss_socket = socket;
    ss->ss_addrlen = sizeof ss->ss_addr;
    if (getsockname(socket, (void *)ss->ss_addr.array, &ss->ss_addrlen) < 0)
      return STUN_500_SERVER_ERROR;
  }

  a = malloc(sizeof *a); if (!a) return STUN_500_SERVER_ERROR;
  a->attr_type = SOURCE_ADDRESS;
  addr = malloc(sizeof *addr); if (!addr) return STUN_500_SERVER_ERROR;
  memcpy(addr, ss->ss_addr.array, sizeof *addr);
  a->pattr = addr;
  a->next = NULL;
  *next = a; next = &(a->next);

  if (changed) {
    socket = changed->ss_socket;

    /* CHANGED-ADDRESS */
    a = malloc(sizeof *a); if (!a) return STUN_500_SERVER_ERROR;
    a->attr_type = CHANGED_ADDRESS;
    addr = malloc(sizeof *addr); if (!addr) return STUN_500_SERVER_ERROR;
    memcpy(addr, changed->ss_addr.array, sizeof *addr);

    a->pattr = addr;
    a->next = NULL;
    *next = a; next = &(a->next);
  }

  stun_send_message(socket, (void *)from, response, NULL);

  return 0;
}

static int process_bis_request(stun_mini_t *mini,
			       stun_msg_t *request, stun_msg_t *response,
			       su_socket_t socket,
			       void *from, socklen_t fromlen)
{
  return process_3489_request(mini, request, response, socket, from, fromlen);
}

int send_stun_error(stun_msg_t *response,
		    int error,
		    int socket,
		    void *transaction_id,
		    void *from,
		    socklen_t fromlen)
{
  stun_attr_t *attr;
  stun_attr_errorcode_t *errorcode;
  char const *phrase = stun_response_phrase(error);

  if (!phrase)
    error = STUN_500_SERVER_ERROR, phrase = "Internal Server Error";

  stun_init_message(response);

  response->stun_hdr.msg_type = BINDING_ERROR_RESPONSE;
  response->stun_hdr.msg_len = 0; /* actual len computed later */

  memcpy(response->stun_hdr.tran_id, transaction_id, 16);

  /* ERROR-CODE */
  attr = malloc(sizeof *attr); if (!attr) return -1;
  response->stun_attr = attr;
  attr->attr_type = ERROR_CODE;
  attr->next = NULL;

  errorcode = malloc(sizeof(*errorcode));
  if (!errorcode)
    return -1;
  errorcode->code = error;
  errorcode->phrase = malloc(strlen(phrase) + 1);
  if (!errorcode->phrase)
    return -1;
  strcpy(errorcode->phrase, phrase);
  attr->pattr = errorcode;

  stun_send_message(socket, from, response, NULL);

  return 0;
}

