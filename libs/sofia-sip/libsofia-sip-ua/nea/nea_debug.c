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

/*
 */

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

/**@CFILE nea_debug.c  Debug Log for Nokia Event Client API
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 */

#include <stddef.h>

#include "nea_debug.h"

/**@var NEA_DEBUG
 *
 * Environment variable determining the debug log level for @b nea
 * module.
 *
 * The NEA_DEBUG environment variable is used to determine the debug
 * logging level for @b nea module. The default level is 3.
 *
 * @sa <sofia-sip/su_debug.h>, nea_log, SOFIA_DEBUG
 */
extern char const NEA_DEBUG[];

#ifndef SU_DEBUG
#define SU_DEBUG 3
#endif

/**Debug log for @b nea module.
 *
 * The nea_log is the log object used by @b nea module. The level of
 * #nea_log is set using #NEA_DEBUG environment variable.
 */
su_log_t nea_log[] = { SU_LOG_INIT("nea", "NEA_DEBUG", SU_DEBUG) };
