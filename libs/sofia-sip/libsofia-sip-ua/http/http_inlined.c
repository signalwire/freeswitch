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

/**@CFILE http_inlined.c
 *
 * Expand inlined http functions non-inline.
 *
 */

#include "config.h"

#include <sofia-sip/su_config.h>

#if SU_HAVE_INLINE

extern int xyzzy;

#else

#include "sofia-sip/msg_header.h"
#include "sofia-sip/su_tag.h"

#undef SU_HAVE_INLINE
#undef su_inline

#define SU_HAVE_INLINE 1
#define su_inline

#include "sofia-sip/http_header.h"

#endif
