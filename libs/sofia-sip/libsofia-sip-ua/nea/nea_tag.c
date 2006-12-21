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

/**@CFILE nea_tag.c
 * @brief Tags for Nokia SIP Transaction API
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti Mela@nokia.com>
 *
 * @date Created: Tue Jul 24 22:28:34 2001 ppessi
 */

#include "config.h"

#include <string.h>
#include <assert.h>

#define TAG_NAMESPACE "nea"

#include "sofia-sip/nea.h"
#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/sip_tag_class.h>
#include <sofia-sip/url_tag_class.h>

tag_typedef_t neatag_any = NSTAG_TYPEDEF(*);
tag_typedef_t neatag_min_expires = UINTTAG_TYPEDEF(min_expires);
tag_typedef_t neatag_expires = UINTTAG_TYPEDEF(expires);
tag_typedef_t neatag_max_expires = UINTTAG_TYPEDEF(max_expires);
tag_typedef_t neatag_throttle = UINTTAG_TYPEDEF(throttle);
tag_typedef_t neatag_minthrottle = UINTTAG_TYPEDEF(minthrottle);
tag_typedef_t neatag_dialog = PTRTAG_TYPEDEF(dialog);
tag_typedef_t neatag_eventlist = BOOLTAG_TYPEDEF(eventlist);
tag_typedef_t neatag_fake = BOOLTAG_TYPEDEF(fake);
tag_typedef_t neatag_reason = STRTAG_TYPEDEF(reason);
tag_typedef_t neatag_retry_after = UINTTAG_TYPEDEF(retry_after);
tag_typedef_t neatag_exstate = STRTAG_TYPEDEF(exstate);
tag_typedef_t neatag_version = INTTAG_TYPEDEF(version);
tag_typedef_t neatag_view = PTRTAG_TYPEDEF(view);
tag_typedef_t neatag_evmagic = PTRTAG_TYPEDEF(evmagic);
tag_typedef_t neatag_reliable = BOOLTAG_TYPEDEF(reliable);
tag_typedef_t neatag_sub = PTRTAG_TYPEDEF(sub);

tag_typedef_t neatag_strict_3265 = BOOLTAG_TYPEDEF(strict_3265);
