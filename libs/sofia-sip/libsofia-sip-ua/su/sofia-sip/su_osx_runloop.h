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

#ifndef SU_OSX_RUNLOOP_H
/** Defined when <sofia-sip/su_osx_runloop.h> has been included. */
#define SU_OSX_RUNLOOP_H

/**
 * @file sofia-sip/su_osx_runloop.h
 * @brief OSX Run Loop integration.
 *
 * @author Martti Mela <Martti.Mela@nokia.com>
 *
 * @date Created: Fri Sep 22 16:49:51 EEST 2006 mela
 *
 * @NEW_1_12_4.
 */

#ifndef SU_WAIT_H
#include <sofia-sip/su_wait.h>
#endif

#include <CoreFoundation/CoreFoundation.h>

SOFIA_BEGIN_DECLS

su_root_t *su_root_osx_runloop_create(su_root_magic_t *) __attribute__((__malloc__));
void       su_root_osx_prepare_run(su_root_t *);

SOFIA_END_DECLS

#endif /* !defined SU_OSX_RUNLOOP_H */
