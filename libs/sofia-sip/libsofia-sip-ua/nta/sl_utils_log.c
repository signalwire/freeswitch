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

/**@ingroup sl_utils
 *
 * @CFILE sl_utils_log.c
 * @brief Implementation of SIP library utility logging functions.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created:  Thu Oct  5 15:38:39 2000 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <sofia-sip/su_log.h>

#include <sofia-sip/sip_header.h>
#include "sofia-sip/sl_utils.h"

/**Log a SIP message.
 *
 * The function sl_message_log() logs shorthand information identifying
 * the SIP message to the given @a log at level @a level.  The shorthand
 * information include the method and URL by default.  If @a details is
 * nonzero, topmost @Via, @CSeq, @To @b and @@From is included, too.
 *
 * @param log      output log (if @c NULL, su_default_log() is used).
 * @param level    log level
 * @param prefix   string logged before the first line.
 * @param sip      message to be logged.
 * @param details  flag specifying if detailed output is desired.
 */
void sl_sip_log(su_log_t *log,
		int level,
		char const *prefix,
		sip_t const *sip,
		int details)
{
  sip_cseq_t const *cs = sip->sip_cseq;

  if (log == NULL)
    log = su_log_default;

  assert(cs);

  if (sip->sip_request) {
    su_llog(log, level,
	    "%s%s "URL_FORMAT_STRING" (CSeq %d %s)\n",
	    prefix,
	    sip->sip_request->rq_method_name,
	    URL_PRINT_ARGS(sip->sip_request->rq_url),
	    cs->cs_seq,
	    cs->cs_method_name);

    if (!details)
      return;

    if (sip->sip_via) {
      char const *received = sip->sip_via->v_received;
      char const *port = sip->sip_via->v_port;

      su_llog(log, level,
	      "\tvia %s%s%s%s%s%s\n",
	      sip->sip_via->v_host,
	      port ? ":" : "", port ? port : "",
	      received ? " (" : "", received ? received : "",
	      received ? ")" : "");
    }
  }
  else {
    su_llog(log, level,
	    "%s%03u %s (CSeq %d %s)\n",
	    prefix,
	    sip->sip_status->st_status,
	    sip->sip_status->st_phrase,
	    cs->cs_seq,
	    cs->cs_method_name);
    if (!details)
      return;
  }

  if (sip->sip_from)
    sl_from_log(log, level, "\tFrom: %s\n", sip->sip_from);

  if (sip->sip_to)
    sl_to_log(log, level, "\tTo: %s\n", sip->sip_to);
}

/**Log a @From header.
 *
 * The function sl_from_log() logs the contents of @a from header to
 * the output @a log.  The @a fmt specifies the output format, where %s
 * is replaced with header contents. If @a fmt is @c NULL, only the header
 * contents are logged.
 *
 * @param log      output log
 * @param level    logging level of output
 * @param fmt      output format
 * @param from     @From header
 */
void sl_from_log(su_log_t *log, int level,
		 char const *fmt, sip_from_t const *from)
{
  sip_addr_t a[1];

  if (from == NULL)
    return;

  memcpy(a, from, sizeof a);
  a->a_params = NULL;
  if (!a->a_display) a->a_display = "";

  sl_header_log(log, level, fmt, (sip_header_t *)a);
}

/**Log a @To header.
 *
 * The function sl_to_log() logs the contents of @a to header to the
 * log @a log with given @a level.  The @a fmt specifies the output format,
 * where %s is replaced with header contents. If @a fmt is @c NULL, only the
 * header contents are logged.
 *
 * @param log      output log
 * @param level    logging level of output
 * @param fmt      output format
 * @param to       @To header
 */
void sl_to_log(su_log_t *log, int level, char const *fmt, sip_to_t const *to)
{
  sl_from_log(log, level, fmt, (sip_from_t const *)to);
}

/**Log a @Contact header.
 *
 * The function sl_contact_log() logs the contents of @a contact header
 * to the log @a log with given @a level.  The @a fmt specifies the output
 * format, where %s is replaced with header contents. If @a fmt is @c NULL,
 * only the header contents are logged.
 *
 * @param log      output log
 * @param level    logging level of output
 * @param fmt      output format
 * @param contact  @Contact header
 */
void sl_contact_log(su_log_t *log, int level,
		     char const *fmt, sip_contact_t const *m)
{
  sl_from_log(log, level, fmt, (sip_from_t const *)m);
}

/**Log an @Allow header(s).
 *
 * The function sl_allow_log() logs the contents of @a allow header to
 * the log @a log with given @a level.  The @a fmt specifies the output
 * format, where %s is replaced with header contents. If @a fmt is @c NULL,
 * only the header contents are logged.
 *
 * @param log      output log
 * @param level    logging level of output
 * @param fmt      output format
 * @param allow    @Allow header
 *
 */
void sl_allow_log(su_log_t *log, int level,
		  char const *fmt, sip_allow_t const *allow)
{
  sl_header_log(log, level, fmt, (sip_header_t *)allow);
}


/**Log a @Via header.
 *
 * The function sl_via_log() logs the contents of @a via header to
 * the @a log.  The @a fmt specifies the output format, where %s
 * is replaced with header contents. If @a fmt is @c NULL, only the header
 * contents are logged.
 *
 * @param log  output log
 * @param fmt  format used when logging
 * @param v    via header
 */
void sl_via_log(su_log_t *log, int level, char const *fmt, sip_via_t const *v)
{
  sl_header_log(log, level, fmt, (sip_header_t *)v);
}


/**Log message payload.
 *
 * The function sl_payload_log() logs the contents of @a payload object
 * to the output @a log.  Each line in the payload is prepended with the
 * @a prefix.  If @a prefix is @c NULL, only the header contents are logged.
 * For each line in payload, only first 70 charactes are logged, rest is
 * replaced with "...".
 *
 * @param log      output log
 * @param level    logging level of output
 * @param prefix   prefix appended to each payload line
 * @param pl       payload object
 */
void sl_payload_log(su_log_t *log, int level,
		    char const *prefix,
		    sip_payload_t const *pl)
{
  char *s = pl->pl_data, *end = pl->pl_data + pl->pl_len;
  char line[74];

  if (log == NULL)
    log = su_log_default;

  while (s < end && *s != '\0') {
    size_t n = su_strncspn(s, end - s, "\r\n");
    size_t crlf = su_strnspn(s + n, end - s - n, "\r\n");
    if (n < 70) {
      memcpy(line, s, n);
      line[n] = '\0';
    }
    else {
      memcpy(line, s, 70);
      strcpy(line + 70, "...");
    }
    su_llog(log, level, "%s%s\n", prefix, line);
    s += n + crlf;
  }
}

/** Log a header.
 *
 * Logs the contents of an header to the output @a stream. The @a fmt
 * specifies the output format, where %s is replaced with header contents.
 * If @a fmt is @c NULL, only the header contents are logged.
 *
 * @param stream   output stream
 * @param fmt      output format
 * @param h        a SIP header object
 */
void sl_header_log(su_log_t *log, int level, char const *fmt,
		   sip_header_t const *h)
{
  char *s, b[1024];
  issize_t len;

  len = sip_header_field_e(s = b, sizeof b, h, 0);
  if (len == -1)
    return;

  if ((size_t)len >= sizeof b) {
    s = malloc(len + 1); if (!s) return;
    sip_header_field_e(s, len + 1, h, 0);
  }
  s[len] = '\0';

  if (fmt == NULL)
    fmt = "%s\n";
  su_llog(log, level, fmt, s);

  if (s != b)
    free(s);
}
