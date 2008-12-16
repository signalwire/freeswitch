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

#ifndef MSG_MCLASS_HASH_H
/** Defined when <sofia-sip/msg_mclass_hash.h> has been included. */
#define MSG_MCLASS_HASH_H

/**@ingroup msg_parser
 * @file sofia-sip/msg_mclass_hash.h
 *
 * Hash function for header names.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Aug 21 16:03:45 2001 ppessi
 *
 */

#include <sofia-sip/su_config.h>

#ifndef BNF_H
#include <sofia-sip/bnf.h>
#endif

SOFIA_BEGIN_DECLS

/** Hash the header name */
#define MC_HASH(s, n)     (msg_header_name_hash(s, NULL) % (unsigned)(n))

/** Hash header name */
su_inline
unsigned short msg_header_name_hash(char const *s, isize_t *llen)
{
  unsigned short hash = 0;
  size_t i;

  for (i = 0; s[i]; i++) {
    unsigned char c = s[i];
    if (!(_bnf_table[c] & bnf_token))
      break;
    if (c >= 'A' && c <= 'Z')
      hash += (c + 'a' - 'A');
    else
      hash += c;
    hash *= 38501U;
  }

  if (llen)
    *llen = i;

  return hash;
}

SOFIA_END_DECLS

#endif /** MSG_MCLASS_HASH_H */
