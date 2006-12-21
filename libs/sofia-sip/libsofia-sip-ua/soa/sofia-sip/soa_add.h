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

#ifndef SOA_ADD_H
#define SOA_ADD_H
/**@file sofia-sip/soa_add.h  Register SDP Offer/Answer Interface Instances.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Mon Aug 1 15:43:53 EEST 2005 ppessi
 */

#include <sofia-sip/su_config.h>

SOFIA_BEGIN_DECLS

struct soa_session_actions;

SOFIAPUBVAR struct soa_session_actions const soa_default_actions;

SOFIAPUBFUN int soa_add(char const *name, struct soa_session_actions const *handler);

SOFIAPUBFUN struct soa_session_actions const *soa_find(char const *name);

SOFIA_END_DECLS

#endif /* SOA_ADD_H */
