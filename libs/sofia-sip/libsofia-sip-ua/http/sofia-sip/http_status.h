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

#ifndef HTTP_STATUS_H
#define HTTP_STATUS_H

/**@file sofia-sip/http_status.h
 *
 * HTTP status codes.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Sep 18 18:55:09 2001 ppessi
 */

#include <sofia-sip/su_config.h>

SOFIA_BEGIN_DECLS

SOFIAPUBFUN char const *http_status_phrase(int status);

#define HTTP_100_CONTINUE	       100, http_100_continue
#define HTTP_101_SWITCHING	       101, http_101_switching
#define HTTP_200_OK		       200, http_200_ok
#define HTTP_201_CREATED	       201, http_201_created
#define HTTP_202_ACCEPTED	       202, http_202_accepted
#define HTTP_203_NON_AUTH_INFO	       203, http_203_non_auth_info
#define HTTP_204_NO_CONTENT	       204, http_204_no_content
#define HTTP_205_RESET_CONTENT	       205, http_205_reset_content
#define HTTP_206_PARTIAL_CONTENT       206, http_206_partial_content
#define HTTP_300_MULTIPLE_CHOICES      300, http_300_multiple_choices
#define HTTP_301_MOVED_PERMANENTLY     301, http_301_moved_permanently
#define HTTP_302_FOUND		       302, http_302_found
#define HTTP_303_SEE_OTHER	       303, http_303_see_other
#define HTTP_304_NOT_MODIFIED	       304, http_304_not_modified
#define HTTP_305_USE_PROXY	       305, http_305_use_proxy
#define HTTP_307_TEMPORARY_REDIRECT    307, http_307_temporary_redirect
#define HTTP_400_BAD_REQUEST	       400, http_400_bad_request
#define HTTP_401_UNAUTHORIZED	       401, http_401_unauthorized
#define HTTP_402_PAYMENT_REQUIRED      402, http_402_payment_required
#define HTTP_403_FORBIDDEN	       403, http_403_forbidden
#define HTTP_404_NOT_FOUND	       404, http_404_not_found
#define HTTP_405_NOT_ALLOWED	       405, http_405_not_allowed
#define HTTP_406_NOT_ACCEPTABLE	       406, http_406_not_acceptable
#define HTTP_407_PROXY_AUTH	       407, http_407_proxy_auth
#define HTTP_408_TIMEOUT	       408, http_408_timeout
#define HTTP_409_CONFLICT	       409, http_409_conflict
#define HTTP_410_GONE		       410, http_410_gone
#define HTTP_411_NO_LENGTH	       411, http_411_no_length
#define HTTP_412_PRECONDITION	       412, http_412_precondition
#define HTTP_413_ENTITY_TOO_LARGE      413, http_413_entity_too_large
#define HTTP_414_URI_TOO_LONG	       414, http_414_uri_too_long
#define HTTP_415_MEDIA_TYPE	       415, http_415_media_type
#define HTTP_416_REQUESTED_RANGE       416, http_416_requested_range
#define HTTP_417_EXPECTATION	       417, http_417_expectation
#define HTTP_426_UPGRADE               426, http_426_upgrade
#define HTTP_500_INTERNAL_SERVER       500, http_500_internal_server
#define HTTP_501_NOT_IMPLEMENTED       501, http_501_not_implemented
#define HTTP_502_BAD_GATEWAY	       502, http_502_bad_gateway
#define HTTP_503_NO_SERVICE	       503, http_503_no_service
#define HTTP_504_GATEWAY_TIMEOUT       504, http_504_gateway_timeout
#define HTTP_505_HTTP_VERSION	       505, http_505_http_version

SOFIAPUBVAR char const http_100_continue[];
SOFIAPUBVAR char const http_101_switching[];
SOFIAPUBVAR char const http_200_ok[];
SOFIAPUBVAR char const http_201_created[];
SOFIAPUBVAR char const http_202_accepted[];
SOFIAPUBVAR char const http_203_non_auth_info[];
SOFIAPUBVAR char const http_204_no_content[];
SOFIAPUBVAR char const http_205_reset_content[];
SOFIAPUBVAR char const http_206_partial_content[];
SOFIAPUBVAR char const http_300_multiple_choices[];
SOFIAPUBVAR char const http_301_moved_permanently[];
SOFIAPUBVAR char const http_302_found[];
SOFIAPUBVAR char const http_303_see_other[];
SOFIAPUBVAR char const http_304_not_modified[];
SOFIAPUBVAR char const http_305_use_proxy[];
SOFIAPUBVAR char const http_307_temporary_redirect[];
SOFIAPUBVAR char const http_400_bad_request[];
SOFIAPUBVAR char const http_401_unauthorized[];
SOFIAPUBVAR char const http_402_payment_required[];
SOFIAPUBVAR char const http_403_forbidden[];
SOFIAPUBVAR char const http_404_not_found[];
SOFIAPUBVAR char const http_405_not_allowed[];
SOFIAPUBVAR char const http_406_not_acceptable[];
SOFIAPUBVAR char const http_407_proxy_auth[];
SOFIAPUBVAR char const http_408_timeout[];
SOFIAPUBVAR char const http_409_conflict[];
SOFIAPUBVAR char const http_410_gone[];
SOFIAPUBVAR char const http_411_no_length[];
SOFIAPUBVAR char const http_412_precondition[];
SOFIAPUBVAR char const http_413_entity_too_large[];
SOFIAPUBVAR char const http_414_uri_too_long[];
SOFIAPUBVAR char const http_415_media_type[];
SOFIAPUBVAR char const http_416_requested_range[];
SOFIAPUBVAR char const http_417_expectation[];
SOFIAPUBVAR char const http_426_upgrade[];
SOFIAPUBVAR char const http_500_internal_server[];
SOFIAPUBVAR char const http_501_not_implemented[];
SOFIAPUBVAR char const http_502_bad_gateway[];
SOFIAPUBVAR char const http_503_no_service[];
SOFIAPUBVAR char const http_504_gateway_timeout[];
SOFIAPUBVAR char const http_505_http_version[];

SOFIA_END_DECLS

#endif /* HTTP_STATUS_H */
