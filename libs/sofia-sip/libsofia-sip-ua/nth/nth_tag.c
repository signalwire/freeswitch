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

/**@CFILE nth_tag.c
 * @brief Tags for HTTP Transaction API
 *
 * @note This file is used to automatically generate
 * nth_tag_ref.c and nth_tag_dll.c
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Jul 24 22:28:34 2001 ppessi
 */

#include "config.h"

#include <string.h>
#include <assert.h>

#define TAG_NAMESPACE "nth"

#include "sofia-sip/nth_tag.h"
#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/http_tag_class.h>
#include <sofia-sip/url_tag_class.h>

#include <sofia-sip/http_protos.h>

tag_typedef_t nthtag_any = NSTAG_TYPEDEF(*);

/* Common */
tag_typedef_t nthtag_mclass = PTRTAG_TYPEDEF(mclass);
tag_typedef_t nthtag_message = PTRTAG_TYPEDEF(message);
tag_typedef_t nthtag_mflags = INTTAG_TYPEDEF(mflags);
tag_typedef_t nthtag_streaming = BOOLTAG_TYPEDEF(streaming);

/* Client */
tag_typedef_t nthtag_proxy = URLTAG_TYPEDEF(proxy);
tag_typedef_t nthtag_expires = UINTTAG_TYPEDEF(expires);
tag_typedef_t nthtag_error_msg = BOOLTAG_TYPEDEF(error_msg);
tag_typedef_t nthtag_template = PTRTAG_TYPEDEF(template);
tag_typedef_t nthtag_authentication = PTRTAG_TYPEDEF(authentication);

/* Server */
tag_typedef_t nthtag_root = PTRTAG_TYPEDEF(root);
tag_typedef_t nthtag_strict_host = BOOLTAG_TYPEDEF(scrict_host);

/**@def NTHTAG_AUTH_MODULE()
 *
 * Pointer to authentication module.
 *
 * A site requires authentication from the clients if passed an
 * authentication module pointer with NTHTAG_AUTH_MODULE(). Incoming client
 * request is challenged with 401, upon successful authentication the
 * authenticated username is stored in the #auth_status_t structure
 * associated with the #nth_request_t object. It is up to application to
 * authorize the user.
 *
 * @sa nth_site_create(), nth_site_set_params(), nth_request_auth().
 */
tag_typedef_t nthtag_auth_module = PTRTAG_TYPEDEF(auth_module);
