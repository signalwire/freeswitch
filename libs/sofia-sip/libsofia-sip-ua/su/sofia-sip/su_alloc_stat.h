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

#ifndef SU_ALLOC_STAT_H
/** Defined when <sofia-sip/su_alloc_stat.h> has been included. */
#define SU_ALLOC_STAT_H

/**@ingroup su_alloc
 *
 * @file sofia-sip/su_alloc_stat.h Home-based memory management statistics
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Apr  9 10:24:05 2002 ppessi
 */

#ifndef SU_ALLOC_H
#include <sofia-sip/su_alloc.h>
#endif

#ifndef SU_TYPES_H
#include <sofia-sip/su_types.h>
#endif

SOFIA_BEGIN_DECLS

typedef struct su_home_stat_t su_home_stat_t;

SU_DLL void su_home_init_stats(su_home_t *h);
SU_DLL void su_home_get_stats(su_home_t *, int include_clones,
			      su_home_stat_t *stats, isize_t statssize);

SU_DLL void su_home_stat_add(su_home_stat_t *total,
			     su_home_stat_t const *hs);

struct su_home_stat_t
{
  int      hs_size;
  usize_t hs_clones;		/**< Number of clones */
  usize_t hs_rehash;		/**< Number of (re)allocations of hash table. */
  usize_t hs_blocksize; 	/**< Current size of hash table */

  struct {
    unsigned hsp_size;		/**< Size of preload area */
    unsigned hsp_used;		/**< Number of bytes used from preload */
  } hs_preload;

  struct {
    uint64_t hsa_number;
    uint64_t hsa_bytes;
    uint64_t hsa_rbytes;
    uint64_t hsa_maxrbytes;
    uint64_t hsa_preload;	/**< Number of allocations from preload area */
  } hs_allocs;

  struct {
    uint64_t hsf_number;
    uint64_t hsf_bytes;
    uint64_t hsf_rbytes;
    uint64_t hsf_preload;	/**< Number of free()s from preload area */
  } hs_frees;

  struct {
    uint64_t hsb_number;
    uint64_t hsb_bytes;
    uint64_t hsb_rbytes;
  } hs_blocks;
};

SOFIA_END_DECLS

#endif /* ! defined(SU_ALLOC_H) */
