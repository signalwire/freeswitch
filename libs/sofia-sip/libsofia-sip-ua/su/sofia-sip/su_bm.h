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

#ifndef SU_BM_H
/** Defined when <sofia-sip/su_bm.h> has been included. */
#define SU_BM_H

/**
 * @file sofia-sip/su_bm.h
 * @brief Boyer-Moore search algorithm
 *
 * @author Pekka.Pessi@nokia.com
 *
 * @date Created: Mon Apr 11 17:03:13 2005 ppessi
 */

#ifndef SU_TYPES_H
#include <sofia-sip/su_types.h>
#endif

SOFIA_BEGIN_DECLS

typedef struct bw_fwd_table bm_fwd_table_t;

SOFIAPUBFUN bm_fwd_table_t *bm_memmem_study(char const *needle, size_t nlen);

SOFIAPUBFUN char *bm_memmem(char const *haystack, size_t hlen,
			    char const *needle, size_t nlen,
			    bm_fwd_table_t *fwd);

SOFIAPUBFUN bm_fwd_table_t *bm_memcasemem_study(char const *needle, size_t);

SOFIAPUBFUN char *bm_memcasemem(char const *haystack, size_t hlen,
				char const *needle, size_t nlen,
				bm_fwd_table_t *fwd);

SOFIA_END_DECLS

#endif /* !defined SU_MEMMEM_H */
