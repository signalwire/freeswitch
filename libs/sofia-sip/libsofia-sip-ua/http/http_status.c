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

/**@CFILE http_status.c   HTTP status codes.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Sep 18 18:58:21 2001 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <sofia-sip/http_status.h>

char const
  http_100_continue[]             = "Continue",
  http_101_switching[]            = "Switching Protocols",
  http_200_ok[]                   = "OK",
  http_201_created[]              = "Created",
  http_202_accepted[]             = "Accepted",
  http_203_non_auth_info[]        = "Non-Authoritative Information",
  http_204_no_content[]           = "No Content",
  http_205_reset_content[]        = "Reset Content",
  http_206_partial_content[]      = "Partial Content",
  http_300_multiple_choices[]     = "Multiple Choices",
  http_301_moved_permanently[]    = "Moved Permanently",
  http_302_found[]                = "Found",
  http_303_see_other[]            = "See Other",
  http_304_not_modified[]         = "Not Modified",
  http_305_use_proxy[]            = "Use Proxy",
  http_307_temporary_redirect[]   = "Temporary Redirect",
  http_400_bad_request[]          = "Bad Request",
  http_401_unauthorized[]         = "Unauthorized",
  http_402_payment_required[]     = "Payment Required",
  http_403_forbidden[]            = "Forbidden",
  http_404_not_found[]            = "Not Found",
  http_405_not_allowed[]          = "Method Not Allowed",
  http_406_not_acceptable[]       = "Not Acceptable",
  http_407_proxy_auth[]           = "Proxy Authentication Required",
  http_408_timeout[]              = "Request Timeout",
  http_409_conflict[]             = "Conflict",
  http_410_gone[]                 = "Gone",
  http_411_no_length[]            = "Length Required",
  http_412_precondition[]         = "Precondition Failed",
  http_413_entity_too_large[]     = "Request Entity Too Large",
  http_414_uri_too_long[]         = "Request-URI Too Long",
  http_415_media_type[]           = "Unsupported Media Type",
  http_416_requested_range[]      = "Requested Range Not Satisfiable",
  http_417_expectation[]          = "Expectation Failed",
  http_426_upgrade[]              = "Upgrade Required",
  http_500_internal_server[]      = "Internal Server Error",
  http_501_not_implemented[]      = "Not Implemented",
  http_502_bad_gateway[]          = "Bad Gateway",
  http_503_no_service[]           = "Service Unavailable",
  http_504_gateway_timeout[]      = "Gateway Timeout",
  http_505_http_version[]         = "HTTP Version Not Supported";

char const *http_status_phrase(int status)
{
  if (status < 100 || status > 699)
    return NULL;

  switch (status) {
  case 100: return http_100_continue;
  case 101: return http_101_switching;
  case 200: return http_200_ok;
  case 201: return http_201_created;
  case 202: return http_202_accepted;
  case 203: return http_203_non_auth_info;
  case 204: return http_204_no_content;
  case 205: return http_205_reset_content;
  case 206: return http_206_partial_content;
  case 300: return http_300_multiple_choices;
  case 301: return http_301_moved_permanently;
  case 302: return http_302_found;
  case 303: return http_303_see_other;
  case 304: return http_304_not_modified;
  case 305: return http_305_use_proxy;
  case 307: return http_307_temporary_redirect;
  case 400: return http_400_bad_request;
  case 401: return http_401_unauthorized;
  case 402: return http_402_payment_required;
  case 403: return http_403_forbidden;
  case 404: return http_404_not_found;
  case 405: return http_405_not_allowed;
  case 406: return http_406_not_acceptable;
  case 407: return http_407_proxy_auth;
  case 408: return http_408_timeout;
  case 409: return http_409_conflict;
  case 410: return http_410_gone;
  case 411: return http_411_no_length;
  case 412: return http_412_precondition;
  case 413: return http_413_entity_too_large;
  case 414: return http_414_uri_too_long;
  case 415: return http_415_media_type;
  case 416: return http_416_requested_range;
  case 417: return http_417_expectation;
  case 426: return http_426_upgrade;
  case 500: return http_500_internal_server;
  case 501: return http_501_not_implemented;
  case 502: return http_502_bad_gateway;
  case 503: return http_503_no_service;
  case 504: return http_504_gateway_timeout;
  case 505: return http_505_http_version;
  }

  return " ";
}
