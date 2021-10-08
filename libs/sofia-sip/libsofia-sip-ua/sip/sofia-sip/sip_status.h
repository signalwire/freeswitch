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

#ifndef SIP_STATUS_H
/** Defined when <sofia-sip/sip_status.h> has been included. */
#define SIP_STATUS_H

/**@addtogroup sip_status_codes
 * @{
 */
/**@file sofia-sip/sip_status.h
 *
 * SIP status codes and standard phrases.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Tue Jun  6 17:43:46 2000 ppessi
 */

#include <sofia-sip/su_config.h>

SOFIA_BEGIN_DECLS

SOFIAPUBFUN char const *sip_status_phrase(int status);

/** 100 Trying @HIDE */
#define SIP_100_TRYING                  100, sip_100_Trying
/** 180 Ringing @HIDE */
#define SIP_180_RINGING                 180, sip_180_Ringing
/** 181 Call Is Being Forwarded @HIDE */
#define SIP_181_CALL_IS_BEING_FORWARDED 181, sip_181_Call_is_being_forwarded
/** 182 Queued @HIDE */
#define SIP_182_QUEUED                  182, sip_182_Queued
/** 183 Session Progress @HIDE */
#define SIP_183_SESSION_PROGRESS        183, sip_183_Session_progress
/** 200 OK @HIDE */
#define SIP_200_OK                      200, sip_200_OK
/** 202 Accepted @HIDE */
#define SIP_202_ACCEPTED                202, sip_202_Accepted
/** 300 Multiple Choices @HIDE */
#define SIP_300_MULTIPLE_CHOICES        300, sip_300_Multiple_choices
/** 301 Moved Permanently @HIDE */
#define SIP_301_MOVED_PERMANENTLY       301, sip_301_Moved_permanently
/** 302 Moved Temporarily @HIDE */
#define SIP_302_MOVED_TEMPORARILY       302, sip_302_Moved_temporarily
/** 305 Use Proxy @HIDE */
#define SIP_305_USE_PROXY               305, sip_305_Use_proxy
/** 380 Alternative Service @HIDE */
#define SIP_380_ALTERNATIVE_SERVICE     380, sip_380_Alternative_service
/** 400 Bad Request @HIDE */
#define SIP_400_BAD_REQUEST             400, sip_400_Bad_request
/** 401 Unauthorized @HIDE */
#define SIP_401_UNAUTHORIZED            401, sip_401_Unauthorized
/** 402 Payment Required @HIDE */
#define SIP_402_PAYMENT_REQUIRED        402, sip_402_Payment_required
/** 403 Forbidden @HIDE */
#define SIP_403_FORBIDDEN               403, sip_403_Forbidden
/** 404 Not Found @HIDE */
#define SIP_404_NOT_FOUND               404, sip_404_Not_found
/** 405 Method Not Allowed @HIDE */
#define SIP_405_METHOD_NOT_ALLOWED      405, sip_405_Method_not_allowed
/** 406 Not Acceptable @HIDE */
#define SIP_406_NOT_ACCEPTABLE          406, sip_406_Not_acceptable
/** 407 Proxy Authentication Required @HIDE */
#define SIP_407_PROXY_AUTH_REQUIRED     407, sip_407_Proxy_auth_required
/** 408 Request Timeout @HIDE */
#define SIP_408_REQUEST_TIMEOUT         408, sip_408_Request_timeout
/** 409 Conflict @HIDE */
#define SIP_409_CONFLICT                409, sip_409_Conflict
/** 410 Gone @HIDE */
#define SIP_410_GONE                    410, sip_410_Gone
/** 411 Length Required @HIDE */
#define SIP_411_LENGTH_REQUIRED         411, sip_411_Length_required
/** 412 Precondition Failed @HIDE */
#define SIP_412_PRECONDITION_FAILED     412, sip_412_Precondition_failed
/** 413 Request Entity Too Large @HIDE */
#define SIP_413_REQUEST_TOO_LARGE       413, sip_413_Request_too_large
/** 414 Request-URI Too Long @HIDE */
#define SIP_414_REQUEST_URI_TOO_LONG    414, sip_414_Request_uri_too_long
/** 415 Unsupported Media Type @HIDE */
#define SIP_415_UNSUPPORTED_MEDIA       415, sip_415_Unsupported_media
/** 416 Unsupported URI Scheme @HIDE */
#define SIP_416_UNSUPPORTED_URI         416, sip_416_Unsupported_uri
/** 417 Unknown Resource-Priority @HIDE */
#define SIP_417_RESOURCE_PRIORITY       417, sip_417_Resource_priority
/** 420 Bad Extension @HIDE */
#define SIP_420_BAD_EXTENSION           420, sip_420_Bad_extension
/** 421 Extension Required @HIDE */
#define SIP_421_EXTENSION_REQUIRED      421, sip_421_Extension_required
/** 422 Session Timer Too Small @HIDE */
#define SIP_422_SESSION_TIMER_TOO_SMALL 422, sip_422_Session_timer
/** 423 Interval Too Brief @HIDE */
#define SIP_423_INTERVAL_TOO_BRIEF      423, sip_423_Interval_too_brief
#define SIP_423_REGISTRATION_TOO_BRIEF  423, sip_423_Interval_too_brief
/** 480 Temporarily Unavailable @HIDE */
#define SIP_480_TEMPORARILY_UNAVAILABLE 480, sip_480_Temporarily_unavailable
/** 481 Call/Transaction Does Not Exist @HIDE */
#define SIP_481_NO_TRANSACTION          481, sip_481_No_transaction
#define SIP_481_NO_CALL                 481, sip_481_No_transaction
/** 482 Loop Detected @HIDE */
#define SIP_482_LOOP_DETECTED           482, sip_482_Loop_detected
/** 483 Too Many Hops @HIDE */
#define SIP_483_TOO_MANY_HOPS           483, sip_483_Too_many_hops
/** 484 Address Incomplete @HIDE */
#define SIP_484_ADDRESS_INCOMPLETE      484, sip_484_Address_incomplete
/** 485 Ambiguous @HIDE */
#define SIP_485_AMBIGUOUS               485, sip_485_Ambiguous
/** 486 Busy Here @HIDE */
#define SIP_486_BUSY_HERE               486, sip_486_Busy_here
/** 487 Request Terminated @HIDE */
#define SIP_487_REQUEST_TERMINATED      487, sip_487_Request_terminated
#define SIP_487_REQUEST_CANCELLED       487, sip_487_Request_terminated
/** 488 Not acceptable here @HIDE */
#define SIP_488_NOT_ACCEPTABLE          488, sip_488_Not_acceptable
/** 489 Bad Event @HIDE */
#define SIP_489_BAD_EVENT               489, sip_489_Bad_event
/** 490 Request Updated @HIDE */
#define SIP_490_REQUEST_UPDATED         490, sip_490_Request_updated
/** 491 Request Pending @HIDE */
#define SIP_491_REQUEST_PENDING         491, sip_491_Request_pending
/** 493 Undecipherable @HIDE */
#define SIP_493_UNDECIPHERABLE          493, sip_493_Undecipherable
/** 494 Security Agreement Required @HIDE */
#define SIP_494_SECAGREE_REQUIRED       494, sip_494_Secagree_required

/** 500 Internal Server Error @HIDE */
#define SIP_500_INTERNAL_SERVER_ERROR   500, sip_500_Internal_server_error
/** 501 Not Implemented @HIDE */
#define SIP_501_NOT_IMPLEMENTED         501, sip_501_Not_implemented
/** 502 Bad Gateway @HIDE */
#define SIP_502_BAD_GATEWAY             502, sip_502_Bad_gateway
/** 503 Service Unavailable @HIDE */
#define SIP_503_SERVICE_UNAVAILABLE     503, sip_503_Service_unavailable
/** 504 Gateway Time-out @HIDE */
#define SIP_504_GATEWAY_TIME_OUT        504, sip_504_Gateway_time_out
/** 505 Version Not Supported @HIDE */
#define SIP_505_VERSION_NOT_SUPPORTED   505, sip_505_Version_not_supported
/** 513 Message Too Large @HIDE */
#define SIP_513_MESSAGE_TOO_LARGE       513, sip_513_Message_too_large
/** 580 Precondition Failure @HIDE */
#define SIP_580_PRECONDITION            580, sip_580_Precondition

/** 600 Busy Everywhere @HIDE */
#define SIP_600_BUSY_EVERYWHERE         600, sip_600_Busy_everywhere
/** 603 Decline @HIDE */
#define SIP_603_DECLINE                 603, sip_603_Decline
/** 604 Does Not Exist Anywhere @HIDE */
#define SIP_604_DOES_NOT_EXIST_ANYWHERE 604, sip_604_Does_not_exist_anywhere
/** 606 Not Acceptable @HIDE */
#define SIP_606_NOT_ACCEPTABLE          606, sip_606_Not_acceptable
/** 607 Unwanted @HIDE */
#define SIP_607_UNWANTED                607, sip_607_Unwanted
/** 687 Dialog terminated @HIDE */
#define SIP_687_DIALOG_TERMINATED       687, sip_687_Dialog_terminated

SOFIAPUBVAR char const sip_100_Trying[];

SOFIAPUBVAR char const sip_180_Ringing[];
SOFIAPUBVAR char const sip_181_Call_is_being_forwarded[];
SOFIAPUBVAR char const sip_182_Queued[];
SOFIAPUBVAR char const sip_183_Session_progress[];

SOFIAPUBVAR char const sip_200_OK[];
SOFIAPUBVAR char const sip_202_Accepted[];

SOFIAPUBVAR char const sip_300_Multiple_choices[];
SOFIAPUBVAR char const sip_301_Moved_permanently[];
SOFIAPUBVAR char const sip_302_Moved_temporarily[];
SOFIAPUBVAR char const sip_305_Use_proxy[];
SOFIAPUBVAR char const sip_380_Alternative_service[];

SOFIAPUBVAR char const sip_400_Bad_request[];
SOFIAPUBVAR char const sip_401_Unauthorized[];
SOFIAPUBVAR char const sip_402_Payment_required[];
SOFIAPUBVAR char const sip_403_Forbidden[];
SOFIAPUBVAR char const sip_404_Not_found[];
SOFIAPUBVAR char const sip_405_Method_not_allowed[];
SOFIAPUBVAR char const sip_406_Not_acceptable[];
SOFIAPUBVAR char const sip_407_Proxy_auth_required[];
SOFIAPUBVAR char const sip_408_Request_timeout[];
SOFIAPUBVAR char const sip_409_Conflict[];
SOFIAPUBVAR char const sip_410_Gone[];
SOFIAPUBVAR char const sip_411_Length_required[];
SOFIAPUBVAR char const sip_412_Precondition_failed[];
SOFIAPUBVAR char const sip_413_Request_too_large[];
SOFIAPUBVAR char const sip_414_Request_uri_too_long[];
SOFIAPUBVAR char const sip_415_Unsupported_media[];
SOFIAPUBVAR char const sip_416_Unsupported_uri[];
SOFIAPUBVAR char const sip_417_Resource_priority[];
SOFIAPUBVAR char const sip_420_Bad_extension[];
SOFIAPUBVAR char const sip_421_Extension_required[];
SOFIAPUBVAR char const sip_422_Session_timer[];
SOFIAPUBVAR char const sip_423_Interval_too_brief[];
SOFIAPUBVAR char const sip_480_Temporarily_unavailable[];
SOFIAPUBVAR char const sip_481_No_transaction[];
SOFIAPUBVAR char const sip_482_Loop_detected[];
SOFIAPUBVAR char const sip_483_Too_many_hops[];
SOFIAPUBVAR char const sip_484_Address_incomplete[];
SOFIAPUBVAR char const sip_485_Ambiguous[];
SOFIAPUBVAR char const sip_486_Busy_here[];
SOFIAPUBVAR char const sip_487_Request_terminated[];
SOFIAPUBVAR char const sip_488_Not_acceptable[];
SOFIAPUBVAR char const sip_489_Bad_event[];
SOFIAPUBVAR char const sip_490_Request_updated[];
SOFIAPUBVAR char const sip_491_Request_pending[];
SOFIAPUBVAR char const sip_493_Undecipherable[];
SOFIAPUBVAR char const sip_494_Secagree_required[];

SOFIAPUBVAR char const sip_500_Internal_server_error[];
SOFIAPUBVAR char const sip_501_Not_implemented[];
SOFIAPUBVAR char const sip_502_Bad_gateway[];
SOFIAPUBVAR char const sip_503_Service_unavailable[];
SOFIAPUBVAR char const sip_504_Gateway_time_out[];
SOFIAPUBVAR char const sip_505_Version_not_supported[];
SOFIAPUBVAR char const sip_513_Message_too_large[];
SOFIAPUBVAR char const sip_580_Precondition[];

SOFIAPUBVAR char const sip_600_Busy_everywhere[];
SOFIAPUBVAR char const sip_603_Decline[];
SOFIAPUBVAR char const sip_604_Does_not_exist_anywhere[];
SOFIAPUBVAR char const sip_606_Not_acceptable[];
SOFIAPUBVAR char const sip_607_Unwanted[];
SOFIAPUBVAR char const sip_687_Dialog_terminated[];

SOFIA_END_DECLS

#endif /** @} !defined(SIP_STATUS_H) */
