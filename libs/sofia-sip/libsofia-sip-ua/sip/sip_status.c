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

/**@ingroup sip_status_codes
 * @CFILE sip_status.c
 *
 * SIP status codes and standard phrases.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date  Created: Fri Aug 11 18:03:33 2000 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <sofia-sip/sip_status.h>

char const
  sip_100_Trying[] =                   "Trying",
  sip_180_Ringing[] =                  "Ringing",
  sip_181_Call_is_being_forwarded[] =  "Call Is Being Forwarded",
  sip_182_Queued[] =                   "Queued",
  sip_183_Session_progress[] =         "Session Progress",

  sip_200_OK[] =                       "OK",
  sip_202_Accepted[] =                 "Accepted",

  sip_300_Multiple_choices[] =         "Multiple Choices",
  sip_301_Moved_permanently[] =        "Moved Permanently",
  sip_302_Moved_temporarily[] =        "Moved Temporarily",
  sip_305_Use_proxy[] =                "Use Proxy",
  sip_380_Alternative_service[] =      "Alternative Service",

  sip_400_Bad_request[] =              "Bad Request",
  sip_401_Unauthorized[] =             "Unauthorized",
  sip_402_Payment_required[] =         "Payment Required",
  sip_403_Forbidden[] =                "Forbidden",
  sip_404_Not_found[] =                "Not Found",
  sip_405_Method_not_allowed[] =       "Method Not Allowed",
  sip_406_Not_acceptable[] =           "Not Acceptable",
  sip_407_Proxy_auth_required[] =      "Proxy Authentication Required",
  sip_408_Request_timeout[] =          "Request Timeout",
  sip_409_Conflict[] =                 "Conflict",
  sip_410_Gone[] =                     "Gone",
  sip_411_Length_required[] =          "Length Required",
  sip_412_Precondition_failed[] =      "Precondition Failed",
  sip_413_Request_too_large[] =        "Request Entity Too Large",
  sip_414_Request_uri_too_long[] =     "Request-URI Too Long",
  sip_415_Unsupported_media[] =        "Unsupported Media Type",
  sip_416_Unsupported_uri[] =          "Unsupported URI Scheme",
  sip_417_Resource_priority[]=         "Unknown Resource-Priority",
  sip_420_Bad_extension[] =            "Bad Extension",
  sip_421_Extension_required[] =       "Extension Required",
  sip_422_Session_timer[] =            "Session Interval Too Small",
  sip_423_Interval_too_brief[] =       "Interval Too Brief",

  sip_480_Temporarily_unavailable[] =  "Temporarily Unavailable",
  sip_481_No_transaction[] =           "Call/Transaction Does Not Exist",
  sip_482_Loop_detected[] =            "Loop Detected",
  sip_483_Too_many_hops[] =            "Too Many Hops",
  sip_484_Address_incomplete[] =       "Address Incomplete",
  sip_485_Ambiguous[] =                "Ambiguous",
  sip_486_Busy_here[] =                "Busy Here",
  sip_487_Request_terminated[] =       "Request Terminated",
  sip_488_Not_acceptable[] =           "Not Acceptable Here",
  sip_489_Bad_event[] =                "Bad Event",
  sip_490_Request_updated[] =          "Request Updated",
  sip_491_Request_pending[] =          "Request Pending",
  sip_493_Undecipherable[] =           "Undecipherable",
  sip_494_Secagree_required [] =       "Security Agreement Required",

  sip_500_Internal_server_error[] =    "Internal Server Error",
  sip_501_Not_implemented[] =          "Not Implemented",
  sip_502_Bad_gateway[] =              "Bad Gateway",
  sip_503_Service_unavailable[] =      "Service Unavailable",
  sip_504_Gateway_time_out[] =         "Gateway Time-out",
  sip_505_Version_not_supported[] =    "Version Not Supported",
  sip_513_Message_too_large[] =        "Message Too Large",
  sip_580_Precondition[] =             "Precondition Failure",

  sip_600_Busy_everywhere[] =          "Busy Everywhere",
  sip_603_Decline[] =                  "Decline",
  sip_604_Does_not_exist_anywhere[] =  "Does Not Exist Anywhere",
  sip_606_Not_acceptable[] =           "Not Acceptable",
  sip_607_Unwanted[] =                 "Unwanted",
  sip_687_Dialog_terminated[] =        "Dialog Terminated"
  ;

/** Convert a SIP status code to a status phrase.
 *
 * Convert a SIP status code to a status phrase. If the status code is not
 * in the range 100..699, NULL is returned. If the status code is not known,
 * empty string "" is returned.
 *
 * @param status well-known status code in range 100..699
 *
 * @return
 * A response message corresponding to status code, or NULL upon an error.
 */
char const *sip_status_phrase(int status)
{
  if (status < 100 || status > 699)
    return NULL;

  switch (status) {
  case 100: return sip_100_Trying;
  case 180: return sip_180_Ringing;
  case 181: return sip_181_Call_is_being_forwarded;
  case 182: return sip_182_Queued;
  case 183: return sip_183_Session_progress;

  case 200: return sip_200_OK;
  case 202: return sip_202_Accepted;

  case 300: return sip_300_Multiple_choices;
  case 301: return sip_301_Moved_permanently;
  case 302: return sip_302_Moved_temporarily;
  case 305: return sip_305_Use_proxy;
  case 380: return sip_380_Alternative_service;

  case 400: return sip_400_Bad_request;
  case 401: return sip_401_Unauthorized;
  case 402: return sip_402_Payment_required;
  case 403: return sip_403_Forbidden;
  case 404: return sip_404_Not_found;
  case 405: return sip_405_Method_not_allowed;
  case 406: return sip_406_Not_acceptable;
  case 407: return sip_407_Proxy_auth_required;
  case 408: return sip_408_Request_timeout;
  case 409: return sip_409_Conflict;
  case 410: return sip_410_Gone;
  case 411: return sip_411_Length_required;
  case 412: return sip_412_Precondition_failed;
  case 413: return sip_413_Request_too_large;
  case 414: return sip_414_Request_uri_too_long;
  case 415: return sip_415_Unsupported_media;
  case 416: return sip_416_Unsupported_uri;
  case 417: return sip_417_Resource_priority;

  case 420: return sip_420_Bad_extension;
  case 421: return sip_421_Extension_required;
  case 422: return sip_422_Session_timer;
  case 423: return sip_423_Interval_too_brief;

  case 480: return sip_480_Temporarily_unavailable;
  case 481: return sip_481_No_transaction;
  case 482: return sip_482_Loop_detected;
  case 483: return sip_483_Too_many_hops;
  case 484: return sip_484_Address_incomplete;
  case 485: return sip_485_Ambiguous;
  case 486: return sip_486_Busy_here;
  case 487: return sip_487_Request_terminated;
  case 488: return sip_488_Not_acceptable;
  case 489: return sip_489_Bad_event;
  case 490: return sip_490_Request_updated;
  case 491: return sip_491_Request_pending;
  case 493: return sip_493_Undecipherable;
  case 494: return sip_494_Secagree_required;

  case 500: return sip_500_Internal_server_error;
  case 501: return sip_501_Not_implemented;
  case 502: return sip_502_Bad_gateway;
  case 503: return sip_503_Service_unavailable;
  case 504: return sip_504_Gateway_time_out;
  case 505: return sip_505_Version_not_supported;
  case 513: return sip_513_Message_too_large;
  case 580: return sip_580_Precondition;

  case 600: return sip_600_Busy_everywhere;
  case 603: return sip_603_Decline;
  case 604: return sip_604_Does_not_exist_anywhere;
  case 606: return sip_606_Not_acceptable;
  case 607: return sip_607_Unwanted;
  case 687: return sip_687_Dialog_terminated;
  }

  return "";
}
