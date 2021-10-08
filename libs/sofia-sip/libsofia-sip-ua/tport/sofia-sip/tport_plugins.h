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

#ifndef TPORT_PLUGINS_H
/** Defined when <sofia-sip/tport_plugins.h> has been included. */
#define TPORT_PLUGINS_H

/**@file sofia-sip/tport_plugins.h
 * @brief Transport plugin interface
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri Mar 31 12:22:22 EEST 2006 ppessi
 */

/* -- STUN Plugin ------------------------------------------------------- */

#ifndef TPORT_STUN_SERVER_T
#define TPORT_STUN_SERVER_T struct tport_stun_server_s
#endif
/** Safe type for tport server object */
typedef TPORT_STUN_SERVER_T tport_stun_server_t;

typedef struct {
  int vst_size;
  tport_stun_server_t *(*vst_create)(su_root_t *root, tagi_t const *tags);
  void (*vst_destroy)(tport_stun_server_t *);
  int (*vst_add_socket)(tport_stun_server_t *, su_socket_t socket);
  int (*vst_remove_socket)(tport_stun_server_t *, su_socket_t socket);
  void (*vst_request)(tport_stun_server_t *server, su_socket_t socket,
		     void *msg, ssize_t msglen,
		     void *addr, socklen_t addrlen);
} tport_stun_server_vtable_t;

SOFIAPUBFUN int tport_plug_in_stun_server(tport_stun_server_vtable_t const *);


/* -- SigComp Plugin ---------------------------------------------------- */

/* We already use these SigComp types in applications */

struct sigcomp_udvm;
struct sigcomp_compartment;

typedef struct tport_comp_vtable_s tport_comp_vtable_t;

struct tport_comp_vtable_s {
  /* NOTE: this will change! Unstable! Do not use! */
  int vsc_size;

  char const *vsc_compname;
  size_t vsc_sizeof_context;

  int (*vsc_init_comp)(tp_stack_t *,
		       tport_t *,
		       tport_compressor_t *,
		       char const *comp_name,
		       tagi_t const *tags);

  void (*vsc_deinit_comp)(tp_stack_t *,
			  tport_t *,
			  tport_compressor_t *);

  char const *(*vsc_comp_name)(tport_compressor_t const *master_sc,
			       char const *compression,
			       tagi_t const *tags);

  /* Mapping of public tport API */

  int (*vsc_can_send_comp)(tport_compressor_t const *);
  int (*vsc_can_recv_comp)(tport_compressor_t const *);

  int (*vsc_set_comp_name)(tport_t const *self,
			   tport_compressor_t const *return_sc,
			   char const *comp);

  int (*vsc_sigcomp_option)(tport_t const *self,
			    struct sigcomp_compartment *cc,
			    char const *option);

  struct sigcomp_compartment *
  (*vsc_sigcomp_compartment)(tport_t *self,
			     char const *name, int namelen,
			     int create_if_needed);

  struct sigcomp_compartment *
  (*vsc_compartment_incref)(struct sigcomp_compartment *cc);

  void (*vsc_compartment_decref)(struct sigcomp_compartment **pointer_to_cc);

  int (*vsc_set_compartment)(tport_t *self,
			     tport_compressor_t *,
			     struct sigcomp_compartment *);

  struct sigcomp_compartment *
  (*vsc_get_compartment)(tport_t const *self,
			 tport_compressor_t const *);

  int (*vsc_has_sigcomp_assigned)(tport_compressor_t const *comp);

  int (*vsc_sigcomp_accept)(tport_t *self,
			    tport_compressor_t const *comp,
			    struct sigcomp_compartment *cc,
			    msg_t *msg);

  int (*vsc_delivered_using_udvm)(tport_t *tp,
				  msg_t const *msg,
				  struct sigcomp_udvm **return_pointer_to_udvm,
				  int remove);

  int (*vsc_sigcomp_close)(tport_t *self,
			   struct sigcomp_compartment *cc,
			   int how);

  int (*vsc_sigcomp_lifetime)(tport_t *self,
			      struct sigcomp_compartment *,
			      unsigned lifetime_in_ms,
			      int only_expand);

  /* Internal API */

  struct sigcomp_udvm **(*vsc_get_udvm_slot)(tport_t *self);

  struct sigcomp_compartment *
  (*vsc_sigcomp_assign_if_needed)(tport_t *self,
				  struct sigcomp_compartment *cc);

  void (*vsc_accept_incomplete)(tport_t const *self,
				tport_compressor_t *sc,
				msg_t *msg);

  int (*vsc_recv_comp)(tport_t const *self,
		       tport_compressor_t *sc,
		       msg_t **in_out_msg,
		       su_sockaddr_t *from,
		       socklen_t fromlen);

  ssize_t (*vsc_send_comp)(tport_t const *self,
		       msg_t *msg,
		       msg_iovec_t iov[],
		       size_t iovused,
		       struct sigcomp_compartment *cc,
		       tport_compressor_t *sc);


};

SOFIAPUBFUN int tport_plug_in_comp(tport_comp_vtable_t const *);

#endif /* !defined(TPORT_PLUGINS_H) */
