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
 * @file sofia-sip/su_log.h   SU logging interface
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Thu Feb 22 18:09:02 2001 ppessi
 *
 */

#ifndef SU_LOG_T
/** Defined when the type #su_log_t has been defined. */
#define SU_LOG_T
/** Type of log structure. */
typedef struct su_log_s su_log_t;
#endif

#ifndef SU_LOG_H_NEED_SU_LOG_T

#ifndef SU_LOG_H
/** Defined when <sofia-sip/su_log.h> has been included. */
#define SU_LOG_H

#include <stdarg.h>

#ifndef SU_CONFIG_H
#include <sofia-sip/su_config.h>
#endif

SOFIA_BEGIN_DECLS

/** Prototype for logging function */
typedef void (su_logger_f)(void *stream, char const *fmt, va_list ap);

/** Log object. */
struct su_log_s {
  int          log_size;
  char const  *log_name;
  char const  *log_env;
  unsigned     log_default;
  unsigned     log_level;
  int          log_init;

  su_logger_f *log_logger;
  void        *log_stream;
};

enum { SU_LOG_MAX = 9 };

/** Initialize a su_log_t structure */
#define SU_LOG_INIT(name, env, level) \
  { sizeof(su_log_t), name, env, level, SU_LOG_MAX, 0, NULL, NULL, }

SOFIAPUBFUN void su_log(char const *fmt, ...)
  __attribute__ ((__format__ (printf, 1, 2)));

SOFIAPUBFUN void su_llog(su_log_t *log, unsigned level, char const *fmt, ...)
  __attribute__ ((__format__ (printf, 3, 4)));
SOFIAPUBFUN void su_vllog(su_log_t *log, unsigned level,
			  char const *fmt, va_list ap);
SOFIAPUBFUN void su_log_redirect(su_log_t *log, su_logger_f *f, void *stream);
SOFIAPUBFUN void su_log_set_level(su_log_t *log, unsigned level);
SOFIAPUBFUN void su_log_soft_set_level(su_log_t *log, unsigned level);
SOFIAPUBFUN void su_log_init(su_log_t *log);

SOFIAPUBVAR su_log_t su_log_default[];
SOFIAPUBVAR su_log_t su_log_global[];

/** Log the latest su error message */
SOFIAPUBFUN void su_perror(char const *s);

/** Log the su error message. */
SOFIAPUBFUN void su_perror2(char const *s, int errcode);

SOFIA_END_DECLS

#endif

#endif /* !defined(SU_LOG_H_NEED_SU_LOG_T) */
