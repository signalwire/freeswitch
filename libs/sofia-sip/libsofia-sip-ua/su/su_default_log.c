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
 * @CFILE su_default_log.c
 *
 * Default debug log object.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri Feb 23 17:30:46 2001 ppessi
 */

#include <stdio.h>
#include <stdarg.h>

#include <sofia-sip/su_log.h>
#include <sofia-sip/su_debug.h>

/** Log into FILE, by default stderr. */
static void default_logger(void *stream, char const *fmt, va_list ap)
{
  FILE *f = stream ? (FILE *)stream : stderr;

  vfprintf(f, fmt, ap);
}

/**@var SOFIA_DEBUG
 *
 * Environment variable determining the default debug log level.
 *
 * The SOFIA_DEBUG environment variable is used to determine the default
 * debug logging level. The normal level is 3.
 *
 * @sa <sofia-sip/su_debug.h>, su_log_global
 */
extern char const SOFIA_DEBUG[];

#ifdef SU_DEBUG
#define SOFIA_DEBUG_ SU_DEBUG
#else
#define SOFIA_DEBUG_ 3
#endif

/**Default debug log.
 *
 * If a source module does not define a log object, the output from su_log()
 * function or SU_DEBUG_X() macros use this log object. Also, if a log
 * function references log object with NULL pointer, the su_log_default
 * object is used.
 *
 * If output from another log object is not redirected with
 * su_log_redirect(), the output can be redirected via this log object.
 *
 * If the logging level of a log object is not set with su_log_set_level(),
 * or the environment variable directing its level is not set, the log level
 * from the #su_log_default object is used.
 *
 * The level of #su_log_default is set using SOFIA_DEBUG environment
 * variable.
 */
su_log_t su_log_default[1] = {{
  sizeof(su_log_t),
  "sofia",		/* Log name */
  "SOFIA_DEBUG",	/* Environment variable controlling logging level */
  SOFIA_DEBUG_,		/* Default level */
  SU_LOG_MAX,		/* Maximum log level */
  0,
  default_logger,
  NULL
}};
