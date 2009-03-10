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

#ifndef NTA_TPORT_H
/** Defined when <sofia-sip/nta_tport.h> has been included. */
#define NTA_TPORT_H

/**
 * @file sofia-sip/nta_tport.h
 * @brief Transport and SigComp handling
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Oct  7 20:04:39 2004 ppessi
 *
 */

#ifndef NTA_H
#include <sofia-sip/nta.h>
#endif

SOFIA_BEGIN_DECLS

struct tport_s;

#ifndef TPORT_T
#define TPORT_T struct tport_s
typedef TPORT_T tport_t;
#endif

#ifndef NTA_UPDATE_MAGIC_T
#define NTA_UPDATE_MAGIC_T void
#endif
typedef NTA_UPDATE_MAGIC_T nta_update_magic_t;

struct sigcomp_compartment;
struct sigcomp_udvm;

#define nta_transport nta_incoming_transport

SOFIAPUBFUN tport_t *nta_agent_tports(nta_agent_t *agent);

SOFIAPUBFUN
tport_t *nta_incoming_transport(nta_agent_t *, nta_incoming_t *, msg_t *msg);

SOFIAPUBFUN
struct sigcomp_compartment *nta_incoming_compartment(nta_incoming_t *irq);

SOFIAPUBFUN tport_t *nta_outgoing_transport(nta_outgoing_t *orq);

SOFIAPUBFUN
struct sigcomp_compartment *
nta_outgoing_compartment(nta_outgoing_t *orq);

SOFIAPUBFUN void nta_compartment_decref(struct sigcomp_compartment **);

typedef void nta_update_tport_f(nta_update_magic_t *, nta_agent_t *);

SOFIAPUBFUN
int nta_agent_bind_tport_update(nta_agent_t *agent,
				nta_update_magic_t *magic,
				nta_update_tport_f *);

SOFIA_END_DECLS

#endif /* !defined NTA_TPORT_H */
