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
 * @CFILE su_log.c
 *
 * Implementation of generic logging interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri Feb 23 17:30:13 2001 ppessi
 */

#include "config.h"

#include <sofia-sip/su_log.h>
#include <sofia-sip/su_errno.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>

#if SU_HAVE_PTHREADS
#include <pthread.h>
#define SU_LOG_IS_INIT(log) pthread_mutex_trylock((log)->log_init)
#define SU_LOG_DO_INIT(log)
#define SU_LOG_LOCK(log)    pthread_mutex_lock((log)->log_lock)
#define SU_LOG_UNLOCK(log)  pthread_mutex_unlock((log)->log_lock)
#else
#define SU_LOG_IS_INIT(log) ((log)->log_init)
#define SU_LOG_DO_INIT(log) ((log)->log_init = 1)
#define SU_LOG_LOCK(log)
#define SU_LOG_UNLOCK(log)
#endif

/**@defgroup su_log Logging Interface
 *
 * Generic logging interface.
 *
 * The @b su_log submodule contains a generic logging interface. The
 * interface provides means for redirecting the log and filtering log
 * messages based on message priority.
 *
 * @sa @ref debug_logs, <sofia-sip/su_log.h>,
 * su_llog(), su_vllog(), #su_log_t, #SU_DEBUG,
 * SU_DEBUG_0(), SU_DEBUG_1(), SU_DEBUG_2(), SU_DEBUG_3(), SU_DEBUG_5(),
 * SU_DEBUG_7(), SU_DEBUG_9()
 */

/** Log message of socket error @errcode at level 0. */
void su_perror2(const char *s, int errcode)
{
  su_log("%s: %s\n", s, su_strerror(errcode));
}

/** Log socket error message at level 0. */
void su_perror(const char *s)
{
  su_perror2(s, su_errno());
}

/** Log a message to default log.
 *
 * This function is a replacement for printf().
 *
 * Messages are always logged to the default log.
 */
void su_log(char const *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  su_vllog(su_log_default, 0, fmt, ap);
  va_end(ap);
}

/** Log a message with level.
 *
 * @note This function is used mainly by SU_DEBUG_n() macros.
 */
void _su_llog(su_log_t *log, unsigned level, const char *file, const char *func, int line,
			 char const *fmt, ...)
{
  va_list ap;
  char buf[512];
  va_start(ap, fmt);


  snprintf(buf, sizeof(buf), "%s:%d %s() %s", file, line, func, fmt);

  _su_vllog(log, level, file, func, line, buf, ap);
  va_end(ap);
}

/** Log a message with level (stdarg version). */
void _su_vllog(su_log_t *log, unsigned level, const char *file, const char *func, int line,
			  char const *fmt, va_list ap)
{
  su_logger_f *logger;
  void *stream;

  assert(log);

  if (!log->log_init)
    su_log_init(log);

  if (log->log_init > 1 ?
      level > log->log_level :
      level > su_log_default->log_level)
    return;

  logger = log->log_logger;
  stream = log->log_stream;

  if (!logger) {
    logger = su_log_default->log_logger;
    stream = su_log_default->log_stream;
  }

  if (logger) {
	  logger(stream, fmt, ap);
  }
}

static char const not_initialized[1];
static char const *explicitly_initialized = not_initialized;

/** Initialize a log */
void su_log_init(su_log_t *log)
{
  char *env;

  if (log->log_init)
    return;

  if (explicitly_initialized == not_initialized)
    explicitly_initialized = getenv("SHOW_DEBUG_LEVELS");

  if (log != su_log_default && !su_log_default->log_init)
    su_log_init(su_log_default);

  if (log->log_env && (env = getenv(log->log_env))) {
    int level = atoi(env);

    /* Why? */
    /* if (level == 0) level = 1; */
    log->log_level = level;
    log->log_init = 2;

    if (explicitly_initialized)
      su_llog(log, 0, "%s: initialized log to level %u (%s=%s)\n",
	      log->log_name, log->log_level, log->log_env, env);
  }
  else {
    log->log_level = log->log_default;
    log->log_init = 1;
    if (explicitly_initialized) {
      if (log != su_log_default)
	su_llog(log, 0, "%s: logging at default level %u\n",
		log->log_name, su_log_default->log_level);
      else
	su_llog(log, 0, "%s: initialized log to level %u (default)\n",
		log->log_name, log->log_level);
    }
  }
}

/**Redirect a log.
 *
 * The function su_log_redirect() redirects the su_log() output to
 * @a logger function. The @a logger function has following prototype:
 *
 * @code
 * void logger(void *logarg, char const *format, va_list ap);
 * @endcode
 *
 * If @a logger is NULL, the default logger will be used. If @a log is NULL,
 * the default logger is changed.
 */
void su_log_redirect(su_log_t *log, su_logger_f *logger, void *logarg)
{
  if (log == NULL)
    log = su_log_default;
  /* XXX - locking ? */
  log->log_logger = logger;
  log->log_stream = logarg;
}

/** Set log level.
 *
 * The function su_log_set_level() sets the logging level.  The log events
 * have certain level (0..9); if logging level is lower than the level of
 * the event, the log message is ignored.
 *
 * If @a log is NULL, the default log level is changed.
 */
void su_log_set_level(su_log_t *log, unsigned level)
{
  if (log == NULL)
    log = su_log_default;

  log->log_level = level;
  log->log_init = 2;

  if (explicitly_initialized == not_initialized)
    explicitly_initialized = getenv("SHOW_DEBUG_LEVELS");

  if (explicitly_initialized)
    su_llog(log, 0, "%s: set log to level %u\n",
	    log->log_name, log->log_level);
}

/** Set log level.
 *
 * The function su_log_soft_set_level() sets the logging level if it is not
 * already set, or the environment variable controlling the log level is not
 * set.
 *
 * The log events have certain level (0..9); if logging level is lower than
 * the level of the event, the log message is ignored.
 *
 * If @a log is NULL, the default log level is changed.
 */
void su_log_soft_set_level(su_log_t *log, unsigned level)
{
  if (log == NULL)
    log = su_log_default;
  if (log->log_init == 1)
    return;

  if (log->log_env && getenv(log->log_env)) {
    su_log_init(log);
  }
  else {
    log->log_level = level;
    log->log_init = 2;

    if (explicitly_initialized == not_initialized)
      explicitly_initialized = getenv("SHOW_DEBUG_LEVELS");

    if (explicitly_initialized)
      su_llog(log, 0, "%s: soft set log to level %u\n",
	      log->log_name, log->log_level);
  }
}
