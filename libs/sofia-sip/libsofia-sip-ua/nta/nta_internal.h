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

#ifndef NTA_INTERNAL_H
/** Defined when <nta_internal.h> has been included. */
#define NTA_INTERNAL_H

/**@internal @file nta_internal.h
 *
 * @brief NTA internal interfaces.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Jul 18 09:18:32 2000 ppessi
 */

SOFIA_BEGIN_DECLS

/** A sip_flag telling that this message is internally generated. */
#define NTA_INTERNAL_MSG (1<<15)

#include <sofia-sip/nta.h>
#include <sofia-sip/nta_tport.h>
#include <sofia-sip/nta_stateless.h>
#include <sofia-sip/tport.h>
#if HAVE_SOFIA_SRESOLV
#include <sofia-sip/sresolv.h>
#endif

typedef struct nta_compressor nta_compressor_t;

/* Virtual function table for plugging in SigComp */
typedef struct
{
  int ncv_size;
  char const *ncv_name;

  nta_compressor_t *(*ncv_init_agent)(nta_agent_t *sa,
				     char const * const *options);

  void (*ncv_deinit_agent)(nta_agent_t *sa, nta_compressor_t *);

  struct sigcomp_compartment *(*ncv_compartment)(nta_agent_t *sa,
						 tport_t *tport,
						 nta_compressor_t *msc,
						 tp_name_t const *tpn,
						 char const * const *options,
						 int new_if_needed);

  int (*ncv_accept_compressed)(nta_agent_t *sa,
			       nta_compressor_t *msc,
			       tport_compressor_t *sc,
			       msg_t *msg,
			       struct sigcomp_compartment *cc);

  int (*ncv_close_compressor)(nta_agent_t *sa,
			      struct sigcomp_compartment *cc);
  int (*ncv_zap_compressor)(nta_agent_t *sa,
			    struct sigcomp_compartment *cc);

  struct sigcomp_compartment *(*ncv_compartment_ref)
    (struct sigcomp_compartment *);

  void (*ncv_compartment_unref)(struct sigcomp_compartment *);

} nta_compressor_vtable_t;

SOFIAPUBVAR nta_compressor_vtable_t *nta_compressor_vtable;

SOFIAPUBFUN nta_compressor_t *nta_agent_init_sigcomp(nta_agent_t *sa);
SOFIAPUBFUN void nta_agent_deinit_sigcomp(nta_agent_t *sa);

/* ====================================================================== */
/* Debug log settings */

#define SU_LOG   nta_log

#ifdef SU_DEBUG_H
#error <su_debug.h> included directly.
#endif
#include <sofia-sip/su_debug.h>
SOFIAPUBVAR su_log_t nta_log[];

SOFIA_END_DECLS

#endif /* NTA_INTERNAL_H */
