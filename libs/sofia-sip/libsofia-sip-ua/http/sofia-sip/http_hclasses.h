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

#ifndef HTTP_HCLASSES_H
/** Defined when <sofia-sip/http_hclasses.h> has been included. */
#define HTTP_HCLASSES_H

/**@file sofia-sip/http_hclasses.h
 * @brief HTTP header classes.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 */

#ifndef MSG_TYPES_H
#include <sofia-sip/msg_types.h>
#endif

SOFIA_BEGIN_DECLS

/* Use directly these header classes */

#define http_accept_class              msg_accept_class
#define http_accept_charset_class      msg_accept_charset_class
#define http_accept_encoding_class     msg_accept_encoding_class
#define http_accept_language_class     msg_accept_language_class
#define http_content_encoding_class    msg_content_encoding_class
#define http_content_length_class      msg_content_length_class
#define http_content_md5_class         msg_content_md5_class
#define http_content_type_class        msg_content_type_class
#define http_content_id_class          msg_content_id_class
#define http_content_location_class    msg_content_location_class
#define http_content_language_class    msg_content_language_class
#define http_mime_version_class        msg_mime_version_class

#define http_error_class               msg_error_class
#define http_unknown_class             msg_unknown_class
#define http_separator_class           msg_separator_class
#define http_payload_class             msg_payload_class

SOFIA_END_DECLS

#ifndef HTTP_PROTOS_H
#define HTTP_HCLASSES_ONLY
#include <sofia-sip/http_protos.h>
#undef HTTP_HCLASSES_ONLY
#endif

#endif /* !defined HTTP_HCLASSES_H */
