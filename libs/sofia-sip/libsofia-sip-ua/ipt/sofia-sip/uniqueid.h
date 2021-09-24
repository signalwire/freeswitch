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

#ifndef UNIQUEID_H
/** Defined when <sofia-sip/uniqueid.h> has been included. */
#define UNIQUEID_H

/**@file sofia-sip/uniqueid.h
 *
 * Compatibility functions to handle GloballyUniqueID.
 *
 * This file just includes <su_uniqueid.h>.
 *
 * @author Pekka Pessi <pessi@research.nokia.com>
 *
 * @date Created: Tue Apr 15 06:31:41 1997 pessi
 */

/* Compatibility functionality */
#define guid_t su_guid_t
#define guid_generate su_guid_generate
#define guid_sprintf su_guid_sprintf
#define guid_strlen su_guid_strlen
#define randint su_randint
#define randmem su_randmem

#include <sofia-sip/su_uniqueid.h>


#endif
