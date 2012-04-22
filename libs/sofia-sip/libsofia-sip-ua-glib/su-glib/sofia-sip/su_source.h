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

#ifndef SU_SOURCE_H /** Defined when su_source.h has been included. */
#define SU_SOURCE_H

/**
 * @file su_source.h
 * @brief
 *
 * NOTE: This file (su_source.h) is DEPRECATED as of 1.12.2 release.
 *       Please use su_glib.h instead.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <first.surname@nokia.com>
 *
 * @date Created: Thu Mar  4 19:58:50 2004 ppessi
 *
 */

#ifndef SU_WAIT_H
#include <sofia-sip/su_wait.h>
#endif
#ifndef __GLIB_H__
#include <glib.h>
#endif

SOFIA_BEGIN_DECLS

SOFIAPUBFUN su_root_t *su_root_source_create(su_root_magic_t *) __attribute__((__malloc__));
SOFIAPUBFUN GSource *su_root_source(su_root_t *);

SOFIA_END_DECLS

#endif /* !defined SU_SOURCE_H */
