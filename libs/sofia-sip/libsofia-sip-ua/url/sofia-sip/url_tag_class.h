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

#ifndef URL_TAG_CLASS_H
/**Defined when <sofia-sip/url_tag_class.h> has been included. */
#define URL_TAG_CLASS_H
/**@file  url_tag_class.h
 * @brief Tag classes for URLs
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Feb 21 11:01:45 2001 ppessi
 */

#ifndef SU_TAG_H
#include <sofia-sip/su_tag.h>
#endif
#ifndef SU_TAG_CLASS_H
#include <sofia-sip/su_tag_class.h>
#endif
#ifndef SU_TAG_CLASS_H
#include <sofia-sip/su_tag_class.h>
#endif

SOFIA_BEGIN_DECLS

SOFIAPUBVAR tag_class_t url_tag_class[1];

#define URLTAG_TYPEDEF(t) TAG_TYPEDEF(t, url)

SOFIA_END_DECLS

#endif


