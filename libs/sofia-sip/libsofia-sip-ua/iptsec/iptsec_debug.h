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

#ifndef IPTSEC_DEBUG_H
/** Defined when <iptsec_debug.h> has been included. */
#define IPTSEC_DEBUG_H

/**@internal
 * @file iptsec_debug.h
 * @brief Debug log for IPTSEC module.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Dec 19 15:56:35 2002 ppessi
 */

#include <sofia-sip/su_log.h>

SOFIA_BEGIN_DECLS

/** Common log for client and server components. */
SOFIAPUBVAR su_log_t iptsec_log[];

SOFIA_END_DECLS

#define SU_LOG (iptsec_log)

#include <sofia-sip/su_debug.h>

#endif /* !defined IPTSEC_DEBUG_H */
