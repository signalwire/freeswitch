/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2007 Nokia Corporation.
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

/**@CFILE test_inlined.c
 *
 * Expand inlined test functions non-inline.
 *
 */

#include "config.h"

#include <sofia-sip/su_config.h>

#if SU_HAVE_INLINE
extern int xyzzy;
#else
#undef SU_HAVE_INLINE
#undef su_inline

#define SU_HAVE_INLINE 1
#define su_inline

#include "test_protos.h"

#endif
