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

/**@CFILE tport_stub_sigcomp.c Stub interface for SigComp
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri Mar 31 12:31:36 EEST 2006
 */

#include "config.h"

#include "tport_internal.h"

#include <string.h>
#include <sofia-sip/su_string.h>

tport_comp_vtable_t const *tport_comp_vtable = NULL;

int tport_plug_in_compress(tport_comp_vtable_t const *vsc)
{
  return -1;
}

char const tport_sigcomp_name[] = "sigcomp";

/** Canonize compression string */
char const *tport_canonize_comp(char const *comp)
{
  if (tport_comp_vtable && su_casematch(comp, tport_sigcomp_name))
    return tport_sigcomp_name;
  return NULL;
}

int tport_init_compressor(tport_t *tp,
			  char const *comp_name,
			  tagi_t const *tags)
{
  tport_comp_vtable_t const *vsc = tport_comp_vtable;
  tport_master_t *mr = tp ? tp->tp_master : NULL;
  tport_compressor_t *tcc;

  if (vsc == NULL)
    return -1;
  if (mr == NULL)
    return -1;

  if (tp->tp_comp)
    return 0;

  comp_name = tport_canonize_comp(comp_name);
  if (comp_name == NULL)
    return 0;

  tcc = su_zalloc(tp->tp_home, vsc->vsc_sizeof_context);

  if (tcc == NULL)
    return -1;

  if (vsc->vsc_init_comp(mr->mr_stack, tp, tcc, comp_name, tags) < 0) {
    vsc->vsc_deinit_comp(mr->mr_stack, tp, tcc);
    return -1;
  }

  tp->tp_comp = tcc;

  return 0;
}

void tport_deinit_compressor(tport_t *tp)
{
  tport_comp_vtable_t const *vsc = tport_comp_vtable;
  tport_master_t *mr = tp ? tp->tp_master : NULL;

  if (vsc && mr && tp->tp_comp) {
    vsc->vsc_deinit_comp(mr->mr_stack, tp, tp->tp_comp);
    su_free(tp->tp_home, tp->tp_comp), tp->tp_comp = NULL;
  }
}


char const *tport_comp_name(tport_primary_t *pri,
			    char const *name,
			    tagi_t const *tags)
{
  tport_comp_vtable_t const *vsc = tport_comp_vtable;
  tport_compressor_t const *comp = pri->pri_master->mr_master->tp_comp;

  if (vsc)
    return vsc->vsc_comp_name(comp, name, tags);

  return NULL;
}


/** Check if transport can receive compressed messages */
int tport_can_recv_sigcomp(tport_t const *self)
{
  tport_comp_vtable_t const *vsc = tport_comp_vtable;

  if (vsc)
    return vsc->vsc_can_recv_comp(self->tp_comp);

  return 0;
}

/** Check if transport can send compressed messages */
int tport_can_send_sigcomp(tport_t const *self)
{
  tport_comp_vtable_t const *vsc = tport_comp_vtable;

  if (vsc)
    return vsc->vsc_can_send_comp(self->tp_comp);

  return 0;
}

/** Check if transport supports named compression */
int tport_has_compression(tport_t const *self, char const *comp)
{
  return
    self && comp &&
    self->tp_name->tpn_comp == tport_canonize_comp(comp);
}

/** Set the compression protocol as @a comp. */
int tport_set_compression(tport_t *self, char const *comp)
{
  tport_comp_vtable_t const *vsc = tport_comp_vtable;

  if (vsc)
    return vsc->vsc_set_comp_name(self, self->tp_comp, comp);

  return (self == NULL || comp) ? -1 : 0;
}

/** Set SigComp options. */
int tport_sigcomp_option(tport_t const *self,
			 struct sigcomp_compartment *cc,
			 char const *option)
{
  tport_comp_vtable_t const *vsc = tport_comp_vtable;

  if (vsc)
    return vsc->vsc_sigcomp_option(self, cc, option);

  return -1;
}


/** Assign a SigComp compartment (to a possibly connected tport).
 *
 * @related tport_s
 */
int tport_sigcomp_assign(tport_t *self, struct sigcomp_compartment *cc)
{
  tport_comp_vtable_t const *vsc = tport_comp_vtable;

  if (!vsc)
    return 0;

  if (tport_is_connection_oriented(self) &&
      tport_is_secondary(self) &&
      self->tp_socket != INVALID_SOCKET) {
    return vsc->vsc_set_compartment(self, self->tp_comp, cc);
  }

  return 0;
}

struct sigcomp_compartment *
tport_sigcomp_assign_if_needed(tport_t *self,
			       struct sigcomp_compartment *cc)
{
  tport_comp_vtable_t const *vsc = tport_comp_vtable;

  if (!vsc)
    return NULL;

  if (!self->tp_name->tpn_comp)
    return NULL;

  if (cc) {
    tport_sigcomp_assign(self, cc);
    return cc;
  }

  return vsc->vsc_get_compartment(self, self->tp_comp);
}


/** Test if tport has a SigComp compartment assigned to it. */
int tport_has_sigcomp_assigned(tport_t const *self)
{
  tport_comp_vtable_t const *vsc = tport_comp_vtable;

  if (vsc && self)
    return vsc->vsc_has_sigcomp_assigned(self->tp_comp);

  return 0;
}

int
tport_sigcomp_accept(tport_t *self,
		     struct sigcomp_compartment *cc,
		     msg_t *msg)
{
  tport_comp_vtable_t const *vsc = tport_comp_vtable;

  if (self == NULL)
    return su_seterrno(EFAULT);

  if (vsc)
    return vsc->vsc_sigcomp_accept(self, self->tp_comp, cc, msg);

  return 0;
}

/* Internal API */

/** This function is called when the application message is still incomplete
    but a complete SigComp message could have been received */
void tport_sigcomp_accept_incomplete(tport_t *self, msg_t *msg)
{
  tport_comp_vtable_t const *vsc = tport_comp_vtable;

  if (vsc && self->tp_comp)
    vsc->vsc_accept_incomplete(self, self->tp_comp, msg);
}

struct sigcomp_udvm **tport_get_udvm_slot(tport_t *self)
{
  tport_comp_vtable_t const *vsc = tport_comp_vtable;

  if (vsc && self)
    return vsc->vsc_get_udvm_slot(self);

  return NULL;
}

/** Receive data from datagram using SigComp. */
int tport_recv_comp_dgram(tport_t const *self,
			  tport_compressor_t *sc,
			  msg_t **in_out_msg,
			  su_sockaddr_t *from,
			  socklen_t fromlen)
{
  tport_comp_vtable_t const *vsc = tport_comp_vtable;

  if (vsc)
    return vsc->vsc_recv_comp(self, sc, in_out_msg, from, fromlen);

  msg_destroy(*in_out_msg), *in_out_msg = NULL;

  return su_seterrno(EBADMSG);
}


ssize_t tport_send_comp(tport_t const *self,
			msg_t *msg,
			msg_iovec_t iov[],
			size_t iovused,
			struct sigcomp_compartment *cc,
			tport_compressor_t *comp)
{
  tport_comp_vtable_t const *vsc = tport_comp_vtable;

  if (vsc)
    vsc->vsc_send_comp(self, msg, iov, iovused, cc, comp);

  msg_addrinfo(msg)->ai_flags &= ~TP_AI_COMPRESSED;
  return self->tp_pri->pri_vtable->vtp_send(self, msg, iov, iovused);
}
