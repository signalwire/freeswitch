/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2006 Nokia Corporation.
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

/**@CFILE tport_stub_stun.c Stub interface for STUN
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri Mar 31 12:31:36 EEST 2006
 */

#include "config.h"

#include <sofia-sip/stun.h>
#include <sofia-sip/su_tagarg.h>

#define TPORT_STUN_SERVER_T stun_mini_t
#include "tport_internal.h"
#include "sofia-sip/msg_buffer.h"
#include "sofia-sip/msg_addr.h"

#include <assert.h>
#include <string.h>

/* ---------------------------------------------------------------------- */
/* Plugin pointer */

tport_stun_server_vtable_t const *tport_stun_server_vtable = NULL;


static
tport_stun_server_t *vst_create(su_root_t *root, tagi_t const *tags)
{
  return stun_mini_create();
}

static
tport_stun_server_vtable_t const stun_mini_vtable =
  {
    sizeof stun_mini_vtable,
    vst_create,
    stun_mini_destroy,
    stun_mini_add_socket,
    stun_mini_remove_socket,
    stun_mini_request
  };

/** Initialize stun server */
int tport_init_stun_server(tport_master_t *mr, tagi_t const *tags)
{
  tport_stun_server_vtable_t const *vst = tport_stun_server_vtable;

  if (vst == NULL)
    /* Nobody has plugged better server in, use miniserver */
    tport_stun_server_vtable = vst = &stun_mini_vtable;

  if (!vst)
    return 0;

  if (mr->mr_params->tpp_stun_server)
    mr->mr_stun_server = vst->vst_create(mr->mr_root, tags);
  mr->mr_master->tp_has_stun_server = mr->mr_stun_server != NULL;
  return 0;
}

/** Deinit stun server */
void tport_deinit_stun_server(tport_master_t *mr)
{
  tport_stun_server_vtable_t const *vst = tport_stun_server_vtable;

  if (mr->mr_stun_server) {
    assert(vst);
    vst->vst_destroy(mr->mr_stun_server), mr->mr_stun_server = NULL;
  }
}

int tport_stun_server_add_socket(tport_t *tp)
{
  tport_stun_server_t *stun_server = tp->tp_master->mr_stun_server;

  if (tport_stun_server_vtable &&
      stun_server &&
      tp->tp_params->tpp_stun_server) {
    if (tport_stun_server_vtable->vst_add_socket(stun_server,
						 tp->tp_socket) == 0)
      tp->tp_has_stun_server = 1;
  }
  return 0;
}

int tport_stun_server_remove_socket(tport_t *tp)
{
  tport_stun_server_t *stun_server = tp->tp_master->mr_stun_server;

  if (tport_stun_server_vtable &&
      stun_server &&
      tp->tp_has_stun_server) {
    tport_stun_server_vtable->vst_remove_socket(stun_server, tp->tp_socket);
    tp->tp_has_stun_server = 0;
  }
  return 0;
}

/**Process stun messagee.
 *
 * @retval -1 error
 * @retval 3  stun message received, ignore
 */
int tport_recv_stun_dgram(tport_t const *self,
			  msg_t **in_out_msg,
			  su_sockaddr_t *from,
			  socklen_t fromlen)
{
  int retval = -1;
  msg_t *msg;
  uint8_t *request;
  size_t n;

  assert(in_out_msg); assert(*in_out_msg);

  msg = *in_out_msg;

  request = msg_buf_committed_data(msg);
  n = msg_buf_committed(msg);

  if (n < 20 || request == NULL) {
    su_seterrno(EBADMSG);
    retval = -1;
  }
  else if (request[0] == 1) {
    /* This is a response. */
    if (self->tp_pri->pri_vtable->vtp_stun_response) {
      if (self->tp_pri->pri_vtable->vtp_stun_response(self, request, n,
						      from, fromlen) < 0)
	retval = -1;
    }
    else
      SU_DEBUG_7(("tport(%p): recv_stun_dgram(): "
		  "ignoring request with "MOD_ZU" bytes\n", (void *)self, n));
  }
  else if (request[0] == 0 && self->tp_master->mr_stun_server) {
    tport_stun_server_vtable_t const *vst = tport_stun_server_vtable;
    vst->vst_request(self->tp_master->mr_stun_server,
		     self->tp_socket, request, n,
		     (void *)from, fromlen);
  }
  else if (request[0] == 0) {
    /* Respond to stun request with a simple error message. */
    int const status = 600;
    char const *error = "Not Implemented";
    size_t unpadded = strlen(error);
    uint16_t elen;
    uint8_t dgram[128];

    if (unpadded > sizeof(dgram) - 28)
      unpadded = sizeof(dgram) - 28;

    elen = (uint16_t)unpadded;
    elen = (elen + 3) & -4;	/* Round up to 4 */

    SU_DEBUG_7(("tport(%p): recv_stun_dgram(): "
		"responding %u %s\n", (void *)self, status, error));
  /*
     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      STUN Message Type        |         Message Length        |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                             Transaction ID
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                                                                    |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    */

#define set16(b, offset, value)			\
  (((b)[(offset) + 0] = ((value) >> 8) & 255),	\
   ((b)[(offset) + 1] = (value) & 255))

    /* Respond to request */
    dgram[0] = 1; /* Mark as response */
    dgram[1] = request[1] | 0x10; /* Mark as error response */
    set16(dgram, 2, elen + 4 + 4);

    /* TransactionID is there at bytes 4..19 */
    memcpy(dgram + 4, request + 4, 16);

    /*
    TLV At 20:
     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |         Type                  |            Length             |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    */
    set16(dgram, 20, 0x0009); /* ERROR-CODE */
    set16(dgram, 22, elen + 4);
    /*
    ERROR-CODE at 24:
     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                   0                     |Class|     Number    |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      Reason Phrase (variable)                                ..
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     */
    dgram[24] = 0, dgram[25] = 0;
    dgram[26] = status / 100, dgram[27] = status % 100;
    memcpy(dgram + 28, error, unpadded);
    memset(dgram + 28 + unpadded, 0, elen - unpadded);

    sendto(self->tp_socket, (void *)dgram, 28 + elen, 0,
	   (void *)from, fromlen);
#undef set16
  }
  else {
    SU_DEBUG_0(("tport(%p): recv_stun_dgram(): internal error\n", (void *)self));
    su_seterrno(EBADMSG);
    retval = -1;
  }

  *in_out_msg = NULL, msg_destroy(msg);

  return retval;
}


/** Activate (STUN) keepalive for transport */
int tport_keepalive(tport_t *tp, su_addrinfo_t const *ai,
		    tag_type_t tag, tag_value_t value, ...)
{
  if (tp && tp->tp_pri && tp->tp_pri->pri_vtable->vtp_keepalive) {
    int retval;
    ta_list ta;
    ta_start(ta, tag, value);
    retval = tp->tp_pri->pri_vtable->vtp_keepalive(tp, ai, ta_args(ta));
    ta_end(ta);
    return retval;
  }
  return -1;
}

/* ---------------------------------------------------------------------- */
/* Plugin interface */

/** Plug in stun server.
 *
 * @note This function @b must be called before any transport is initialized.
 */
int tport_plug_in_stun_server(tport_stun_server_vtable_t const *vtable)
{
  if (!vtable)
    return 0;

  if (vtable->vst_size <= (int)sizeof *vtable)
    return su_seterrno(EINVAL);

  if (!vtable->vst_create ||
      !vtable->vst_destroy ||
      !vtable->vst_add_socket ||
      !vtable->vst_remove_socket ||
      !vtable->vst_request)
    return su_seterrno(EFAULT);

  if (tport_stun_server_vtable)
    return su_seterrno(EEXIST);

  tport_stun_server_vtable = vtable;

  return 0;
}
