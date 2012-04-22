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

#ifndef SU_UNIQUEID_H
/** Defined when <sofia-sip/su_uniqueid.h> has been included. */
#define SU_UNIQUEID_H


/**@ingroup su_uniqueid
 * @file sofia-sip/su_uniqueid.h
 *
 * Functions to handle GloballyUniqueIDs.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Apr 15 06:31:41 1997 pessi
 *
 */

#ifndef SU_TYPES_H
#include <sofia-sip/su_types.h>
#endif

SOFIA_BEGIN_DECLS

/** Globally unique identifier type. */
typedef union GloballyUniqueIdentifier {
  unsigned char id[16];
  struct {
    uint32_t  time_low;
    uint16_t  time_mid;
    uint16_t  time_high_and_version;
    uint8_t   clock_seq_hi_and_reserved;
    uint8_t   clock_seq_low;
    uint8_t   node[6];
  } s;
} su_guid_t;

/** Return node identifier */
SOFIAPUBFUN size_t su_node_identifier(void *address, size_t addrlen);

/** Generate a GUID
 *
 * The function guid_generate() generates a new globally unique identifier
 * for an IP telephony call.  The guid follows the structure specified in
 * the ITU-T recommendation H.225.0 v2.  The guid is usable also in SIP
 * @b Call-ID header.
 *
 * @param guid [out] pointer to structure for new call identifier
 */
SOFIAPUBFUN void su_guid_generate(su_guid_t *guid);

/** Print guid.
 *
 * The function guid_sprintf() formats the IP telephony call identifier
 * according the human-readable format specified in the ITU-T recommendation
 * H.225.0 v2.  The printed identifier can be used as a SIP @b Call-ID if
 * the colons in IEEE MAC address are replaced with '-', '+' or other
 * character allowed in SIP @e token.
 *
 * @param buf  [out] buffer to store the formatted globally unique identifier
 * @param len  [in] size of buffer @a buf (should be at least guid_strlen bytes)
 * @param guid [in] pointer to structure containing globally unique identifier
 *
 * @retval
 * The function guid_sprintf() returns length of the formatted
 * globally unique identifier excluding the final NUL.
 */
SOFIAPUBFUN isize_t su_guid_sprintf(char* buf, size_t len, su_guid_t const *guid);

enum {
  /** Length of guid in hex format */
  su_guid_strlen = 8 + 5 + 5 + 5 + 13
};

/** Random integer in range [lb, ub] (inclusive).
 *
 * The function randint() generates a pseudo-random integer in the range
 * [ln, ub] (inclusive).
 *
 * @param lb [in] lower bound
 * @param ub [in] upper bound
 *
 * @return
* The function randint() returns a pseudo-random integer.
 */
SOFIAPUBFUN int su_randint(int lb, int ub);

/** Fill memory with random values.
 *
 * The function randmem() fills the given memory range with pseudo-random data.
 *
 * @param mem [out] pointer to the beginning of the memory area to be filled
 * @param siz [in] size fo the memory area in bytes
 */
SOFIAPUBFUN void *su_randmem(void *mem, size_t siz);

/** Generate a random 32-bit integer. */
SOFIAPUBFUN uint32_t su_random(void);

SOFIA_END_DECLS

#endif
