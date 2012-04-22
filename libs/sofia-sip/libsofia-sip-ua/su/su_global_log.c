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

/**@ingroup su_log
 * @CFILE su_global_log.c
 *
 * Global SU debug log.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Mon May  7 11:08:36 2001 ppessi
 */

#include <stdio.h>
#include <stdarg.h>

#include <sofia-sip/su_log.h>
#include <sofia-sip/su_debug.h>

#ifdef DOXYGEN_ONLY
/**@var SU_DEBUG
 *
 * Environment variable determining the debug log level for @b su module.
 *
 * The SU_DEBUG environment variable is used to determine the debug logging
 * level for @b su module. The default level is 3.
 *
 * @sa <sofia-sip/su_debug.h>, su_log_global
 */
extern char const SU_DEBUG[];
#endif

#ifdef SU_DEBUG
#define SU_DEBUG_ SU_DEBUG
#else
#define SU_DEBUG_ 3
#endif

/**Debug log for @b su module.
 *
 * The su_log_global is the log object used by @b su module. The level of
 * #su_log_global is set using #SU_DEBUG environment variable.
 */
su_log_t su_log_global[1] = {{
  sizeof(su_log_t),
  "su",
  "SU_DEBUG",
  SU_DEBUG_,
  SU_LOG_MAX,
  0,
  NULL,
  NULL
}};
