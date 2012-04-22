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

/**@CFILE sip_time.c
 * @brief SIP time handling
 *
 * Functions for handling time and dates in SIP.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Wed Apr 11 18:57:06 2001 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include "sofia-sip/sip_parser.h"
#include <sofia-sip/sip_util.h>
#include <sofia-sip/msg_date.h>
#include <sofia-sip/su_time.h>

/** Return current time as seconds since Epoch. */
sip_time_t sip_now(void)
{
  return su_now().tv_sec;
}

/**@ingroup sip_expires
 *
 * Calculate the expiration time for a SIP @Contact.
 *
 * @param m     @Contact header
 * @param ex    @Expires header
 * @param date  @Date header
 * @param def   default expiration time
 * @param now   current time.
 *
 * @note If @a m is NULL, the function calculates the expiration time
 *       based on the @Expires and @Date headers.
 *
 * @note If @a now is 0, the function gets the current time using sip_now().
 *
 * @return
 *   The expiration time in seconds.
 */
sip_time_t sip_contact_expires(sip_contact_t const *m,
			       sip_expires_t const *ex,
			       sip_date_t const *date,
			       sip_time_t def,
			       sip_time_t now)
{
  sip_time_t time = 0, delta = def;

  /* "Contact: *" */
  if (m && m->m_url->url_type == url_any)
    return 0;

  if (m && m->m_expires) {
    msg_param_t expires = m->m_expires;
    if (msg_date_delta_d(&expires, &time, &delta) < 0)
      return def;
  }
  else if (ex) {
    time = ex->ex_date;
    delta = ex->ex_delta;
  }

  if (time) {
    if (date)
      now = date->d_time;
    else if (now == 0)
      now = sip_now();

    if (time > now)
      delta = time - now;
    else
      delta = 0;
  }

  return delta;
}
