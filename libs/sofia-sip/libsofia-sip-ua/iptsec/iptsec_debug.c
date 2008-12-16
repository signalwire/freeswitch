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

/**@internal @file iptsec_debug.c
 * @brief Debug log for IPTSEC module.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Dec 19 15:55:30 2002 ppessi
 */

#include "config.h"

#include <stddef.h>

#include "iptsec_debug.h"

#if DOXYGEN_ONLY
/** @defgroup iptsec_env  Environment Variables Used by iptsec Module
 *
 * @brief Environment variables used by @iptsec module are listed here.
 *
 * The #IPTSEC_DEBUG variable sets the debug level.
 */

/**@ingroup iptsec_env
 *
 * Environment variable determining the debug log level for @iptsec
 * module.
 *
 * The IPTSEC_DEBUG environment variable is used to determine the debug
 * logging level for @iptsec module. The default level is 3.
 *
 * @sa <sofia-sip/su_debug.h>, #iptsec_log, #SOFIA_DEBUG
 */
extern IPTSEC_DEBUG;
#endif

#ifndef SU_DEBUG
#define SU_DEBUG 3
#endif

/** Common log for client and server components.
 *
 * The iptsec_log is the log object used by @iptsec module. The level of
 * #iptsec_log is set using #IPTSEC_DEBUG environment variable.
 */
su_log_t iptsec_log[] = { SU_LOG_INIT("iptsec", "IPTSEC_DEBUG", SU_DEBUG) };

