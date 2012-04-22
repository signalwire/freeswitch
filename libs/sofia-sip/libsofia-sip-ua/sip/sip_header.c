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

/**@CFILE sip_header.c
 *
 * SIP header handling.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Tue Jun 13 02:57:51 2000 ppessi
 */

#include "config.h"

/* Avoid casting sip_t to msg_pub_t and sip_header_t to msg_header_t */
#define MSG_PUB_T       struct sip_s
#define MSG_HDR_T       union sip_header_u

/* Get bodies of inlined functions included in library */
#define SIP_STATIC_INLINE

#include <sofia-sip/su_alloc.h>
#include "sofia-sip/sip_parser.h"
#include <sofia-sip/sip_status.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdarg.h>

#include <assert.h>

/** Copy a SIP header.
 *
 * @deprecated Use msg_header_copy() instead.
 */
sip_header_t *sip_header_copy(su_home_t *home, sip_header_t const *h)
{
  if (h == NULL || h == SIP_NONE)
    return NULL;
  return msg_header_copy_as(home, h->sh_class, h);
}

/** Duplicate a SIP header.
 *
 * @deprecated Use msg_header_dup() instead.
 */
sip_header_t *sip_header_dup(su_home_t *home, sip_header_t const *h)
{
  if (h == NULL || h == SIP_NONE)
    return NULL;
  return msg_header_dup_as(home, h->sh_class, h);

}

/** Decode a SIP header.
 *
 * @deprecated Use msg_header_d() instead.
 */
sip_header_t *sip_header_d(su_home_t *home, msg_t const *msg, char const *b)
{
  return msg_header_d(home, msg, b);
}

/** Encode a SIP header.
 *
 * @deprecated Use msg_header_e() instead.
 */
issize_t sip_header_e(char b[], isize_t bsiz, sip_header_t const *h, int flags)
{
  return msg_header_e(b, bsiz, (msg_header_t const *)h, flags);
}

sip_header_t *sip_header_format(su_home_t *home,
				msg_hclass_t *hc,
				char const *fmt,
				...)
{
  sip_header_t *h;
  va_list ap;

  va_start(ap, fmt);

  h = msg_header_vformat(home, hc, fmt, ap);

  va_end(ap);

  return h;
}

/** Add a duplicate of header object to a SIP message. */
int sip_add_dup(msg_t *msg,
		sip_t *sip,
		sip_header_t const *o)
{
  return msg_header_add_dup(msg, (msg_pub_t *)sip, o);
}

int sip_add_dup_as(msg_t *msg,
		   sip_t *sip,
		   msg_hclass_t *hc,
		   sip_header_t const *o)
{
  return msg_header_add_dup_as(msg, (msg_pub_t *)sip, hc, o);
}

int sip_add_make(msg_t *msg,
		 sip_t *sip,
		 msg_hclass_t *hc,
		 char const *s)
{
  return msg_header_add_make(msg, sip, hc, s);
}
