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

/**@CFILE test_nat_tags.c
 * @brief Tags for simulated NAT
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Mar  8 19:54:28 EET 2006
 */

#include "config.h"

#include "test_nat.h"

tag_typedef_t testnattag_symmetric = BOOLTAG_TYPEDEF(symmetric);
tag_typedef_t testnattag_symmetric_ref = REFTAG_TYPEDEF(testnattag_symmetric);
tag_typedef_t testnattag_logging = BOOLTAG_TYPEDEF(symmetric);
tag_typedef_t testnattag_logging_ref = REFTAG_TYPEDEF(testnattag_logging);
