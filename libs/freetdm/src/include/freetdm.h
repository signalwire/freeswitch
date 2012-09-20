/*
 * Copyright (c) 2007-2012, Anthony Minessale II
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Contributors:
 *
 * Moises Silva <moy@sangoma.com>
 * David Yat Sin <dyatsin@sangoma.com>
 *
 */

#ifndef FREETDM_H
#define FREETDM_H

#include "ftdm_declare.h"
#include "ftdm_call_utils.h"

/*! \brief Max number of channels per physical span */
#define FTDM_MAX_CHANNELS_PHYSICAL_SPAN 32

/*! \brief Max number of physical spans per logical span */
#define FTDM_MAX_PHYSICAL_SPANS_PER_LOGICAL_SPAN 128

/*! \brief Max number of channels a logical span can contain */
#define FTDM_MAX_CHANNELS_SPAN FTDM_MAX_CHANNELS_PHYSICAL_SPAN * FTDM_MAX_PHYSICAL_SPANS_PER_LOGICAL_SPAN

/*! \brief Max number of logical spans */
#define FTDM_MAX_SPANS_INTERFACE 128

/*! \brief Max number of channels per hunting group */
#define FTDM_MAX_CHANNELS_GROUP 2048

/*! \brief Max number of groups */
#define FTDM_MAX_GROUPS_INTERFACE FTDM_MAX_SPANS_INTERFACE

/*! \brief Max number of key=value pairs to be sent as signaling stack parameters */
#define FTDM_MAX_SIG_PARAMETERS 30

#define FTDM_INVALID_INT_PARM 0xFF

/*! \brief Thread/Mutex OS abstraction API. */
#include "ftdm_os.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \brief Limit to span names */
#define FTDM_MAX_NAME_STR_SZ 80

/*! \brief Limit to channel number strings */
#define FTDM_MAX_NUMBER_STR_SZ 20

/*! \brief Hangup cause codes */
typedef enum {
	FTDM_CAUSE_NONE = 0,
	FTDM_CAUSE_UNALLOCATED = 1,
	FTDM_CAUSE_NO_ROUTE_TRANSIT_NET = 2,
	FTDM_CAUSE_NO_ROUTE_DESTINATION = 3,
	FTDM_CAUSE_SEND_SPECIAL_INFO_TONE = 4,
 	FTDM_CAUSE_MISDIALED_TRUNK_PREFIX = 5,
	FTDM_CAUSE_CHANNEL_UNACCEPTABLE = 6,
	FTDM_CAUSE_CALL_AWARDED_DELIVERED = 7,
	FTDM_CAUSE_PREEMPTION = 8,
	FTDM_CAUSE_PREEMPTION_CIRCUIT_RESERVED = 9,
	FTDM_CAUSE_NORMAL_CLEARING = 16,
	FTDM_CAUSE_USER_BUSY = 17,
	FTDM_CAUSE_NO_USER_RESPONSE = 18,
	FTDM_CAUSE_NO_ANSWER = 19,
	FTDM_CAUSE_SUBSCRIBER_ABSENT = 20,
	FTDM_CAUSE_CALL_REJECTED = 21,
	FTDM_CAUSE_NUMBER_CHANGED = 22,
	FTDM_CAUSE_REDIRECTION_TO_NEW_DESTINATION = 23,
	FTDM_CAUSE_EXCHANGE_ROUTING_ERROR = 25,
	FTDM_CAUSE_DESTINATION_OUT_OF_ORDER = 27,
	FTDM_CAUSE_INVALID_NUMBER_FORMAT = 28,
	FTDM_CAUSE_FACILITY_REJECTED = 29,
	FTDM_CAUSE_RESPONSE_TO_STATUS_ENQUIRY = 30,
	FTDM_CAUSE_NORMAL_UNSPECIFIED = 31,
	FTDM_CAUSE_NORMAL_CIRCUIT_CONGESTION = 34,
	FTDM_CAUSE_NETWORK_OUT_OF_ORDER = 38,
	FTDM_CAUSE_PERMANENT_FRAME_MODE_CONNECTION_OOS = 39,
	FTDM_CAUSE_PERMANENT_FRAME_MODE_OPERATIONAL = 40,
	FTDM_CAUSE_NORMAL_TEMPORARY_FAILURE = 41,
	FTDM_CAUSE_SWITCH_CONGESTION = 42,
	FTDM_CAUSE_ACCESS_INFO_DISCARDED = 43,
	FTDM_CAUSE_REQUESTED_CHAN_UNAVAIL = 44,
	FTDM_CAUSE_PRE_EMPTED = 45,
	FTDM_CAUSE_PRECEDENCE_CALL_BLOCKED = 46,
	FTDM_CAUSE_RESOURCE_UNAVAILABLE_UNSPECIFIED = 47,
	FTDM_CAUSE_QOS_NOT_AVAILABLE = 49,
	FTDM_CAUSE_FACILITY_NOT_SUBSCRIBED = 50,
	FTDM_CAUSE_OUTGOING_CALL_BARRED = 53,
	FTDM_CAUSE_INCOMING_CALL_BARRED = 55,
	FTDM_CAUSE_BEARERCAPABILITY_NOTAUTH = 57,
	FTDM_CAUSE_BEARERCAPABILITY_NOTAVAIL = 58,
	FTDM_CAUSE_INCONSISTENCY_IN_INFO = 62,
	FTDM_CAUSE_SERVICE_UNAVAILABLE = 63,
	FTDM_CAUSE_BEARERCAPABILITY_NOTIMPL = 65,
	FTDM_CAUSE_CHAN_NOT_IMPLEMENTED = 66,
	FTDM_CAUSE_FACILITY_NOT_IMPLEMENTED = 69,
	FTDM_CAUSE_ONLY_DIGITAL_INFO_BC_AVAIL = 70,
	FTDM_CAUSE_SERVICE_NOT_IMPLEMENTED = 79,
	FTDM_CAUSE_INVALID_CALL_REFERENCE = 81,
	FTDM_CAUSE_IDENTIFIED_CHAN_NOT_EXIST = 82,
	FTDM_CAUSE_SUSPENDED_CALL_EXISTS_BUT_CALL_ID_DOES_NOT = 83,
	FTDM_CAUSE_CALL_ID_IN_USE = 84,
	FTDM_CAUSE_NO_CALL_SUSPENDED = 85,
	FTDM_CAUSE_CALL_WITH_CALL_ID_CLEARED = 86,
	FTDM_CAUSE_USER_NOT_CUG = 87,
	FTDM_CAUSE_INCOMPATIBLE_DESTINATION = 88,
	FTDM_CAUSE_NON_EXISTENT_CUG = 90,
	FTDM_CAUSE_INVALID_TRANSIT_NETWORK_SELECTION = 91,
	FTDM_CAUSE_INVALID_MSG_UNSPECIFIED = 95,
	FTDM_CAUSE_MANDATORY_IE_MISSING = 96,
	FTDM_CAUSE_MESSAGE_TYPE_NONEXIST = 97,
	FTDM_CAUSE_WRONG_MESSAGE = 98,
	FTDM_CAUSE_IE_NONEXIST = 99,
	FTDM_CAUSE_INVALID_IE_CONTENTS = 100,
	FTDM_CAUSE_WRONG_CALL_STATE = 101,
	FTDM_CAUSE_RECOVERY_ON_TIMER_EXPIRE = 102,
	FTDM_CAUSE_MANDATORY_IE_LENGTH_ERROR = 103,
	FTDM_CAUSE_MSG_WITH_UNRECOGNIZED_PARAM_DISCARDED = 110,
	FTDM_CAUSE_PROTOCOL_ERROR = 111,
	FTDM_CAUSE_INTERWORKING = 127,
	FTDM_CAUSE_SUCCESS = 142,
	FTDM_CAUSE_ORIGINATOR_CANCEL = 487,
	FTDM_CAUSE_CRASH = 500,
	FTDM_CAUSE_SYSTEM_SHUTDOWN = 501,
	FTDM_CAUSE_LOSE_RACE = 502,
	FTDM_CAUSE_MANAGER_REQUEST = 503,
	FTDM_CAUSE_BLIND_TRANSFER = 600,
	FTDM_CAUSE_ATTENDED_TRANSFER = 601,
	FTDM_CAUSE_ALLOTTED_TIMEOUT = 602,
	FTDM_CAUSE_USER_CHALLENGE = 603,
	FTDM_CAUSE_MEDIA_TIMEOUT = 604
} ftdm_call_cause_t;

/*! \brief Hunting direction (when hunting for free channels) */
typedef enum {
	FTDM_TOP_DOWN,
	FTDM_BOTTOM_UP,
	FTDM_RR_DOWN,
	FTDM_RR_UP,
} ftdm_direction_t;

/*! \brief I/O channel type */
typedef enum {
	FTDM_CHAN_TYPE_B, /*!< Bearer channel */
	FTDM_CHAN_TYPE_DQ921, /*!< DQ921 channel (D-channel) */
	FTDM_CHAN_TYPE_DQ931, /*!< DQ931 channel */
	FTDM_CHAN_TYPE_FXS, /*!< FXS analog channel */
	FTDM_CHAN_TYPE_FXO, /*!< FXO analog channel */
	FTDM_CHAN_TYPE_EM, /*!< E & M channel */
	FTDM_CHAN_TYPE_CAS, /*!< CAS channel */
	FTDM_CHAN_TYPE_COUNT /*!< Count of channel types */
} ftdm_chan_type_t;
#define CHAN_TYPE_STRINGS "B", "DQ921", "DQ931", "FXS", "FXO", "EM", "CAS", "INVALID"
/*! \brief transform from channel type to string and from string to channel type 
 * ftdm_str2ftdm_chan_type transforms a channel string (ie: "FXO" to FTDM_CHAN_TYPE_FXO) 
 * ftdm_chan_type2str transforms a channel type to string (ie: FTDM_CHAN_TYPE_B to "B")
 */
FTDM_STR2ENUM_P(ftdm_str2ftdm_chan_type, ftdm_chan_type2str, ftdm_chan_type_t)

/*! \brief Test if a channel is a voice channel */
#define FTDM_IS_VOICE_CHANNEL(fchan) ((fchan)->type != FTDM_CHAN_TYPE_DQ921 && (fchan)->type != FTDM_CHAN_TYPE_DQ931)

/*! \brief Test if a channel is a D-channel */
#define FTDM_IS_DCHAN(fchan) ((fchan)->type == FTDM_CHAN_TYPE_DQ921 || (fchan)->type == FTDM_CHAN_TYPE_DQ931)

/*! \brief Test if a channel is digital channel */
#define FTDM_IS_DIGITAL_CHANNEL(fchan) ((fchan)->span->trunk_type == FTDM_TRUNK_E1 || \
	                                (fchan)->span->trunk_type == FTDM_TRUNK_T1 || \
	                                (fchan)->span->trunk_type == FTDM_TRUNK_J1 || \
	                                (fchan)->span->trunk_type == FTDM_TRUNK_BRI || \
	                                (fchan)->span->trunk_type == FTDM_TRUNK_BRI_PTMP)

/*! \brief Test if a span is digital */
#define FTDM_SPAN_IS_DIGITAL(span) \
	((span)->trunk_type == FTDM_TRUNK_E1 || \
	 (span)->trunk_type == FTDM_TRUNK_T1 || \
	 (span)->trunk_type == FTDM_TRUNK_J1 || \
	 (span)->trunk_type == FTDM_TRUNK_BRI || \
	 (span)->trunk_type == FTDM_TRUNK_BRI_PTMP)


/*! \brief Logging function prototype to be used for all FreeTDM logs 
 *  you should use ftdm_global_set_logger to set your own logger
 */
typedef void (*ftdm_logger_t)(const char *file, const char *func, int line, int level, const char *fmt, ...) __ftdm_check_printf(5, 6);

/*! \brief Data queue operation functions
 *  you can use ftdm_global_set_queue_handler if you want to override the default implementation (not recommended)
 */
typedef ftdm_status_t (*ftdm_queue_create_func_t)(ftdm_queue_t **queue, ftdm_size_t capacity);
typedef ftdm_status_t (*ftdm_queue_enqueue_func_t)(ftdm_queue_t *queue, void *obj);
typedef void *(*ftdm_queue_dequeue_func_t)(ftdm_queue_t *queue);
typedef ftdm_status_t (*ftdm_queue_wait_func_t)(ftdm_queue_t *queue, int ms);
typedef ftdm_status_t (*ftdm_queue_get_interrupt_func_t)(ftdm_queue_t *queue, ftdm_interrupt_t **interrupt);
typedef ftdm_status_t (*ftdm_queue_destroy_func_t)(ftdm_queue_t **queue);

typedef struct ftdm_queue_handler {
	ftdm_queue_create_func_t create;
	ftdm_queue_enqueue_func_t enqueue;
	ftdm_queue_dequeue_func_t dequeue;
	ftdm_queue_wait_func_t wait;
	ftdm_queue_get_interrupt_func_t get_interrupt;
	ftdm_queue_destroy_func_t destroy;
} ftdm_queue_handler_t;

/*! \brief Type Of Number (TON) */
typedef enum {
	FTDM_TON_UNKNOWN = 0,
	FTDM_TON_INTERNATIONAL,
	FTDM_TON_NATIONAL,
	FTDM_TON_NETWORK_SPECIFIC,
	FTDM_TON_SUBSCRIBER_NUMBER,
	FTDM_TON_ABBREVIATED_NUMBER,
	FTDM_TON_RESERVED,
	FTDM_TON_INVALID
} ftdm_ton_t;
#define TON_STRINGS "unknown", "international", "national", "network-specific", "subscriber-number", "abbreviated-number", "reserved", "invalid"
FTDM_STR2ENUM_P(ftdm_str2ftdm_ton, ftdm_ton2str, ftdm_ton_t)

/*! Numbering Plan Identification (NPI) */
typedef enum {
	FTDM_NPI_UNKNOWN = 0,
	FTDM_NPI_ISDN,
	FTDM_NPI_DATA,
	FTDM_NPI_TELEX,
	FTDM_NPI_NATIONAL,
	FTDM_NPI_PRIVATE,
	FTDM_NPI_RESERVED,
	FTDM_NPI_INVALID
} ftdm_npi_t;
#define NPI_STRINGS "unknown", "ISDN", "data", "telex", "national", "private", "reserved", "invalid"
FTDM_STR2ENUM_P(ftdm_str2ftdm_npi, ftdm_npi2str, ftdm_npi_t)

/*! Presentation Ind */
typedef enum {
	FTDM_PRES_ALLOWED,
	FTDM_PRES_RESTRICTED,
	FTDM_PRES_NOT_AVAILABLE,
	FTDM_PRES_RESERVED,
	FTDM_PRES_INVALID
} ftdm_presentation_t;
#define PRESENTATION_STRINGS "presentation-allowed", "presentation-restricted", "number-not-available", "reserved", "Invalid"
FTDM_STR2ENUM_P(ftdm_str2ftdm_presentation, ftdm_presentation2str, ftdm_presentation_t)

/*! Screening Ind */
typedef enum {
	FTDM_SCREENING_NOT_SCREENED,
	FTDM_SCREENING_VERIFIED_PASSED,
	FTDM_SCREENING_VERIFIED_FAILED,
	FTDM_SCREENING_NETWORK_PROVIDED,
	FTDM_SCREENING_INVALID
} ftdm_screening_t;
#define SCREENING_STRINGS "user-provided-not-screened", "user-provided-verified-and-passed", "user-provided-verified-and-failed", "network-provided", "invalid"
FTDM_STR2ENUM_P(ftdm_str2ftdm_screening, ftdm_screening2str, ftdm_screening_t)

/*! \brief bearer capability */
typedef enum {
	FTDM_BEARER_CAP_SPEECH = 0x00,	/* Speech */
	FTDM_BEARER_CAP_UNRESTRICTED,	/* Unrestricted Digital */
	FTDM_BEARER_CAP_RESTRICTED,	/* Restricted Digital */
	FTDM_BEARER_CAP_3_1KHZ_AUDIO,	/* 3.1 Khz Audio */
	FTDM_BEARER_CAP_7KHZ_AUDIO,	/* 7 Khz Audio or Unrestricted digital w tones */
	FTDM_BEARER_CAP_15KHZ_AUDIO,	/* 15 Khz Audio */
	FTDM_BEARER_CAP_VIDEO,		/* Video */
	FTDM_BEARER_CAP_INVALID
} ftdm_bearer_cap_t;
#define BEARER_CAP_STRINGS "speech", "unrestricted-digital-information", "restricted-digital-information", "3.1-Khz-audio", "7-Khz-audio", "15-Khz-audio", "video", "invalid"
FTDM_STR2ENUM_P(ftdm_str2ftdm_bearer_cap, ftdm_bearer_cap2str, ftdm_bearer_cap_t)

/*! \brief user information layer 1 protocol */
typedef enum {
	FTDM_USER_LAYER1_PROT_V110 = 0x01,
	FTDM_USER_LAYER1_PROT_ULAW = 0x02,
	FTDM_USER_LAYER1_PROT_ALAW = 0x03,
	FTDM_USER_LAYER1_PROT_INVALID
} ftdm_user_layer1_prot_t;
#define USER_LAYER1_PROT_STRINGS "V.110", "ulaw", "alaw", "Invalid"
FTDM_STR2ENUM_P(ftdm_str2ftdm_usr_layer1_prot, ftdm_user_layer1_prot2str, ftdm_user_layer1_prot_t)

/*! Calling Party Category */
typedef enum {
	FTDM_CPC_UNKNOWN,
	FTDM_CPC_OPERATOR,
	FTDM_CPC_OPERATOR_FRENCH,
	FTDM_CPC_OPERATOR_ENGLISH,
	FTDM_CPC_OPERATOR_GERMAN,
	FTDM_CPC_OPERATOR_RUSSIAN,
	FTDM_CPC_OPERATOR_SPANISH,
	FTDM_CPC_ORDINARY,
	FTDM_CPC_PRIORITY,
	FTDM_CPC_DATA,
	FTDM_CPC_TEST,
	FTDM_CPC_PAYPHONE,
	FTDM_CPC_INVALID
} ftdm_calling_party_category_t;
#define CALLING_PARTY_CATEGORY_STRINGS "unknown", "operator", "operator-french", "operator-english", "operator-german", "operator-russian", "operator-spanish", "ordinary", "priority", "data-call", "test-call", "payphone", "invalid"
FTDM_STR2ENUM_P(ftdm_str2ftdm_calling_party_category, ftdm_calling_party_category2str, ftdm_calling_party_category_t)

/*! Network responses to transfer requests */
typedef enum {
	FTDM_TRANSFER_RESPONSE_OK,					/* Call is being transferred */
	FTDM_TRANSFER_RESPONSE_CP_DROP_OFF,			/* Calling Party drop off */
	FTDM_TRANSFER_RESPONSE_LIMITS_EXCEEDED,		/* Cannot redirect, limits exceeded */
	FTDM_TRANSFER_RESPONSE_INVALID_NUM,			/* Network did not receive or recognize dialed number */
	FTDM_TRANSFER_RESPONSE_INVALID_COMMAND,		/* Network received an invalid command */
	FTDM_TRANSFER_RESPONSE_TIMEOUT,				/* We did not receive a response from Network */
	FTDM_TRANSFER_RESPONSE_INVALID,
} ftdm_transfer_response_t;
#define TRANSFER_RESPONSE_STRINGS "transfer-ok", "cp-drop-off", "limits-exceeded", "invalid-num", "invalid-command", "timeout", "invalid"
FTDM_STR2ENUM_P(ftdm_str2ftdm_transfer_response, ftdm_transfer_response2str, ftdm_transfer_response_t)

/*! \brief Digit limit used in DNIS/ANI */
#define FTDM_DIGITS_LIMIT 25

#define FTDM_SILENCE_VALUE(fchan) (fchan)->native_codec == FTDM_CODEC_ULAW ? 255 : (fchan)->native_codec == FTDM_CODEC_ALAW ? 0xD5 : 0x00

/*! \brief Number abstraction */
typedef struct {
	char digits[FTDM_DIGITS_LIMIT];
	uint8_t type;
	uint8_t plan;
} ftdm_number_t;

typedef struct {
	char from[FTDM_MAX_NUMBER_STR_SZ];  	
	char body[FTDM_MAX_NAME_STR_SZ];
} ftdm_sms_data_t;

/*! \brief Caller information */
typedef struct ftdm_caller_data {
	char cid_date[8]; /*!< Caller ID date */
	char cid_name[80]; /*!< Caller ID name */
	ftdm_number_t cid_num; /*!< Caller ID number */
	ftdm_number_t ani; /*!< ANI (Automatic Number Identification) */
	ftdm_number_t dnis; /*!< DNIS (Dialed Number Identification Service) */
	ftdm_number_t rdnis; /*!< RDNIS (Redirected Dialed Number Identification Service) */
	ftdm_number_t loc; /*!< LOC (Location Reference Code) */
	char aniII[FTDM_DIGITS_LIMIT]; /*! ANI II */
	uint8_t screen; /*!< Screening */
	uint8_t pres; /*!< Presentation*/
	char collected[FTDM_DIGITS_LIMIT]; /*!< Collected digits so far */
	int hangup_cause; /*!< Hangup cause */
	/* these 2 are undocumented right now, only used by boost: */
	/* bearer capability */
	ftdm_bearer_cap_t bearer_capability;
	/* user information layer 1 protocol */
	ftdm_user_layer1_prot_t bearer_layer1;
	ftdm_calling_party_category_t cpc; /*!< Calling party category */
	uint32_t call_reference;

	ftdm_channel_t *fchan; /*!< FreeTDM channel associated (can be NULL) */

	/*
	 * We need call_id inside caller_data for the user to be able to retrieve
	 * the call_id when ftdm_channel_call_place is called. This is the only time
	 * that the user can use caller_data.call_id to obtain the call_id. The user
	 * should use the call_id from sigmsg otherwise
	 */
	uint32_t call_id; /*!< Unique call ID for this call */

	void *priv; /*!< Private data for the FreeTDM user */
} ftdm_caller_data_t;

/*! \brief Hunting mode */
typedef enum {
	FTDM_HUNT_SPAN, /*!< Hunt channels in a given span */
	FTDM_HUNT_GROUP, /*!< Hunt channels in a given group */
	FTDM_HUNT_CHAN, /*!< Hunt for a specific channel */
} ftdm_hunt_mode_t;

/*! \brief Structure used for FTDM_HUNT_SPAN mode */
typedef struct {
	uint32_t span_id;
	ftdm_direction_t direction;
} ftdm_span_hunt_t;

/*! \brief Structure used for FTDM_HUNT_GROUP mode */
typedef struct {
	uint32_t group_id;
	ftdm_direction_t direction;
} ftdm_group_hunt_t;

/*! \brief Structure used for FTDM_HUNT_CHAN mode */
typedef struct {
	uint32_t span_id;
	uint32_t chan_id;
} ftdm_chan_hunt_t;

/*! \brief Function called before placing the call in the hunted channel
 *         The user can have a last saying in whether to proceed or abort 
 *         the call attempt. Be aware that this callback will be called with 
 *         the channel lock and you must not do any blocking operations during 
 *         its execution.
 *  \param fchan The channel that will be used to place the call
 *  \param caller_data The caller data provided to ftdm_call_place
 *  \return FTDM_SUCCESS to proceed or FTDM_BREAK to abort the hunting
 */
typedef ftdm_status_t (*ftdm_hunt_result_cb_t)(ftdm_channel_t *fchan, ftdm_caller_data_t *caller_data);

/*! \brief Channel Hunting provided to ftdm_call_place() */
typedef struct {
	ftdm_hunt_mode_t mode;
	union {
		ftdm_span_hunt_t span;
		ftdm_group_hunt_t group;
		ftdm_chan_hunt_t chan;
	} mode_data;
	ftdm_hunt_result_cb_t result_cb; 
} ftdm_hunting_scheme_t;


/*! \brief Tone type */
typedef enum {
	FTDM_TONE_DTMF = (1 << 0)
} ftdm_tone_type_t;

/*! \brief Signaling messages sent by the stacks */
typedef enum {
	FTDM_SIGEVENT_START,/*!< Incoming call (ie: incoming SETUP msg or Ring) */
	FTDM_SIGEVENT_STOP, /*!< Hangup */
	FTDM_SIGEVENT_RELEASED, /*!< Channel is completely released and available */
	FTDM_SIGEVENT_UP, /*!< Outgoing call has been answered */
	FTDM_SIGEVENT_FLASH, /*!< Flash event  (typically on-hook/off-hook for analog devices) */
	FTDM_SIGEVENT_PROCEED, /*!< Outgoing call got an initial positive response from the other end */
	FTDM_SIGEVENT_RINGING, /*!< Remote side is in ringing state */
	FTDM_SIGEVENT_PROGRESS, /*!< Outgoing call is making progress */
	FTDM_SIGEVENT_PROGRESS_MEDIA, /*!< Outgoing call is making progress and there is media available */
	FTDM_SIGEVENT_ALARM_TRAP, /*!< Hardware alarm ON */
	FTDM_SIGEVENT_ALARM_CLEAR, /*!< Hardware alarm OFF */
	FTDM_SIGEVENT_COLLECTED_DIGIT, /*!< Digit collected (in signalings where digits are collected one by one) */
	FTDM_SIGEVENT_ADD_CALL, /*!< New call should be added to the channel */
	FTDM_SIGEVENT_RESTART, /*!< Restart has been requested. Typically you hangup your call resources here */
	FTDM_SIGEVENT_SIGSTATUS_CHANGED, /*!< Signaling protocol status changed (ie: D-chan up), see new status in raw_data ftdm_sigmsg_t member */
	FTDM_SIGEVENT_FACILITY, /*!< In call facility event */
	FTDM_SIGEVENT_TRACE, /*!<Interpreted trace event */
	FTDM_SIGEVENT_TRACE_RAW, /*!<Raw trace event */
	FTDM_SIGEVENT_INDICATION_COMPLETED, /*!< Last requested indication was completed */
	FTDM_SIGEVENT_DIALING, /*!< Outgoing call just started */
	FTDM_SIGEVENT_TRANSFER_COMPLETED, /*!< Transfer request is completed */
	FTDM_SIGEVENT_SMS,
	FTDM_SIGEVENT_INVALID, /*!<Invalid */
} ftdm_signal_event_t;
#define SIGNAL_STRINGS "START", "STOP", "RELEASED", "UP", "FLASH", "PROCEED", "RINGING", "PROGRESS", \
		"PROGRESS_MEDIA", "ALARM_TRAP", "ALARM_CLEAR", \
		"COLLECTED_DIGIT", "ADD_CALL", "RESTART", "SIGSTATUS_CHANGED", "FACILITY", \
		"TRACE", "TRACE_RAW", "INDICATION_COMPLETED", "DIALING", "TRANSFER_COMPLETED", "SMS", "INVALID"
/*! \brief Move from string to ftdm_signal_event_t and viceversa */
FTDM_STR2ENUM_P(ftdm_str2ftdm_signal_event, ftdm_signal_event2str, ftdm_signal_event_t)

/*! \brief Span trunk types */
typedef enum {
	FTDM_TRUNK_E1,
	FTDM_TRUNK_T1,
	FTDM_TRUNK_J1,
	FTDM_TRUNK_BRI,
	FTDM_TRUNK_BRI_PTMP,
	FTDM_TRUNK_FXO,
	FTDM_TRUNK_FXS,
	FTDM_TRUNK_EM,
	FTDM_TRUNK_GSM,
	FTDM_TRUNK_NONE
} ftdm_trunk_type_t;
#define TRUNK_TYPE_STRINGS "E1", "T1", "J1", "BRI", "BRI_PTMP", "FXO", "FXS", "EM", "GSM", "NONE"

/*! \brief Move from string to ftdm_trunk_type_t and viceversa */
FTDM_STR2ENUM_P(ftdm_str2ftdm_trunk_type, ftdm_trunk_type2str, ftdm_trunk_type_t)

/*! \brief Span trunk modes */
typedef enum {
	FTDM_TRUNK_MODE_CPE,
	FTDM_TRUNK_MODE_NET,
	FTDM_TRUNK_MODE_INVALID
} ftdm_trunk_mode_t;
#define TRUNK_MODE_STRINGS "CPE", "NET", "INVALID"

/*! \brief Move from string to ftdm_trunk_mode_t and viceversa */
FTDM_STR2ENUM_P(ftdm_str2ftdm_trunk_mode, ftdm_trunk_mode2str, ftdm_trunk_mode_t)


/*! \brief Basic channel configuration provided to ftdm_configure_span_channels */
typedef struct ftdm_channel_config {
	char name[FTDM_MAX_NAME_STR_SZ];
	char number[FTDM_MAX_NUMBER_STR_SZ];
	char group_name[FTDM_MAX_NAME_STR_SZ];
	ftdm_chan_type_t type;
	float rxgain;
	float txgain;
	uint8_t debugdtmf;
	uint8_t dtmf_on_start;
	uint32_t dtmfdetect_ms;
	uint8_t iostats;
} ftdm_channel_config_t;

/*!
  \brief Signaling status on a given span or specific channel on protocols that support it
  \note see docs/sigstatus.txt for more extensive documentation on signaling status
 */
typedef enum {
	/* The signaling link is down (no d-chans up in the span/group, MFC-R2 bit pattern unidentified) */
	FTDM_SIG_STATE_DOWN,
	/* The signaling link is suspended (MFC-R2 bit pattern blocked, ss7 blocked?) */
	FTDM_SIG_STATE_SUSPENDED,
	/* The signaling link is ready and calls can be placed (ie: d-chan up) */
	FTDM_SIG_STATE_UP,
	/* Invalid status */
	FTDM_SIG_STATE_INVALID
} ftdm_signaling_status_t;
#define SIGSTATUS_STRINGS "DOWN", "SUSPENDED", "UP", "INVALID"

/*! \brief Move from string to ftdm_signaling_status_t and viceversa */
FTDM_STR2ENUM_P(ftdm_str2ftdm_signaling_status, ftdm_signaling_status2str, ftdm_signaling_status_t)


typedef struct {
	ftdm_signaling_status_t status;
} ftdm_event_sigstatus_t;

typedef enum {
	/* This is an received frame */
	FTDM_TRACE_DIR_INCOMING,
	/* This is a transmitted frame */
	FTDM_TRACE_DIR_OUTGOING,
	/* Invalid */
 	FTDM_TRACE_DIR_INVALID,
} ftdm_trace_dir_t;
#define TRACE_DIR_STRINGS "INCOMING", "OUTGOING", "INVALID"

/*! \brief Move string to ftdm_trace_dir_t and viceversa */
FTDM_STR2ENUM_P(ftdm_str2ftdm_trace_dir, ftdm_trace_dir2str, ftdm_trace_dir_t)

typedef enum {
	FTDM_TRACE_TYPE_Q931,
	FTDM_TRACE_TYPE_Q921,
	FTDM_TRACE_TYPE_INVALID,
} ftdm_trace_type_t;
#define TRACE_TYPE_STRINGS "Q931", "Q921", "INVALID"

/*! \brief Move string to ftdm_trace_type_t and viceversa */
FTDM_STR2ENUM_P(ftdm_str2ftdm_trace_type, ftdm_trace_type2str, ftdm_trace_type_t)

typedef struct {
	/* Direction - incoming or outgoing */
	ftdm_trace_dir_t dir;
	ftdm_trace_type_t type;
} ftdm_event_trace_t;

typedef struct {
	/* Digits collected */
	char digits[FTDM_DIGITS_LIMIT];
} ftdm_event_collected_t;

/*! \brief FreeTDM supported indications.
 * This is used during incoming calls when you want to request the signaling stack
 * to notify about indications occurring locally. See ftdm_channel_call_indicate for more info */
typedef enum {
	FTDM_CHANNEL_INDICATE_NONE,
	FTDM_CHANNEL_INDICATE_RINGING,
	FTDM_CHANNEL_INDICATE_PROCEED,
	FTDM_CHANNEL_INDICATE_PROGRESS,
	FTDM_CHANNEL_INDICATE_PROGRESS_MEDIA,
	FTDM_CHANNEL_INDICATE_BUSY,
	/* Using this indication is equivalent to call ftdm_channel_call_answer API */
	FTDM_CHANNEL_INDICATE_ANSWER,
	FTDM_CHANNEL_INDICATE_FACILITY,
	FTDM_CHANNEL_INDICATE_TRANSFER,
	FTDM_CHANNEL_INDICATE_INVALID,
} ftdm_channel_indication_t;
#define INDICATION_STRINGS "NONE", "RINGING", "PROCEED", "PROGRESS", "PROGRESS_MEDIA", "BUSY", "ANSWER", "FACILITY", "TRANSFER", "INVALID"

/*! \brief Move from string to ftdm_channel_indication_t and viceversa */
FTDM_STR2ENUM_P(ftdm_str2ftdm_channel_indication, ftdm_channel_indication2str, ftdm_channel_indication_t)

typedef struct {
	/* The indication that was completed */
	ftdm_channel_indication_t indication;
	/* Completion status of the indication */
	ftdm_status_t status;
} ftdm_event_indication_completed_t;

typedef struct {
	ftdm_transfer_response_t response;
} ftdm_event_transfer_completed_t;

typedef void * ftdm_variable_container_t;

typedef struct {
	ftdm_size_t len;
	void *data;
} ftdm_raw_data_t;

/*! \brief Generic signaling message received from the stack */
struct ftdm_sigmsg {
	ftdm_signal_event_t event_id; /*!< The type of message */
	ftdm_channel_t *channel; /*!< Related channel */
	uint32_t chan_id; /*!< easy access to chan id */
	uint32_t span_id; /*!< easy access to span_id */
	uint32_t call_id; /*!< unique call id for this call */
	void *call_priv; /*!< Private data for the FreeTDM user from ftdm_caller_data->priv */
	ftdm_variable_container_t variables;
	union {
		ftdm_event_sigstatus_t sigstatus; /*!< valid if event_id is FTDM_SIGEVENT_SIGSTATUS_CHANGED */
		ftdm_event_trace_t trace;	/*!< valid if event_id is FTDM_SIGEVENT_TRACE or FTDM_SIGEVENT_TRACE_RAW */
		ftdm_event_collected_t collected; /*!< valid if event_id is FTDM_SIGEVENT_COLLECTED_DIGIT */
		ftdm_event_indication_completed_t indication_completed; /*!< valid if the event_id is FTDM_SIGEVENT_INDICATION_COMPLETED */
		ftdm_event_transfer_completed_t transfer_completed;
	} ev_data;
	ftdm_raw_data_t raw;
};

/*! \brief Generic user message sent to the stack */
struct ftdm_usrmsg {
	ftdm_variable_container_t variables;
	ftdm_raw_data_t raw;
};

/*! \brief Crash policy 
 *  Useful for debugging only, default policy is never, if you wish to crash on asserts then use ftdm_global_set_crash_policy */
typedef enum {
	FTDM_CRASH_NEVER = 0,
	FTDM_CRASH_ON_ASSERT
} ftdm_crash_policy_t;

/*! \brief Signaling configuration parameter for the stacks (variable=value pair) */
typedef struct ftdm_conf_parameter {
	const char *var;
	const char *val;
	void *ptr;
} ftdm_conf_parameter_t;

/*! \brief Opaque general purpose iterator */
typedef struct ftdm_iterator ftdm_iterator_t;

/*! \brief Channel commands that can be executed through ftdm_channel_command() */
typedef enum {
	FTDM_COMMAND_NOOP = 0,
	FTDM_COMMAND_SET_INTERVAL = 1,
	FTDM_COMMAND_GET_INTERVAL = 2,
	FTDM_COMMAND_SET_CODEC = 3,
	FTDM_COMMAND_GET_CODEC = 4,
	FTDM_COMMAND_SET_NATIVE_CODEC = 5,
	FTDM_COMMAND_GET_NATIVE_CODEC = 6,
	FTDM_COMMAND_ENABLE_DTMF_DETECT = 7,
	FTDM_COMMAND_DISABLE_DTMF_DETECT = 8,
	FTDM_COMMAND_SEND_DTMF = 9,
	FTDM_COMMAND_SET_DTMF_ON_PERIOD = 10,
	FTDM_COMMAND_GET_DTMF_ON_PERIOD = 11,
	FTDM_COMMAND_SET_DTMF_OFF_PERIOD = 12,
	FTDM_COMMAND_GET_DTMF_OFF_PERIOD = 13,
	FTDM_COMMAND_GENERATE_RING_ON = 14,
	FTDM_COMMAND_GENERATE_RING_OFF = 15,
	FTDM_COMMAND_OFFHOOK = 16,
	FTDM_COMMAND_ONHOOK = 17,
	FTDM_COMMAND_FLASH = 18,
	FTDM_COMMAND_WINK = 19,
	FTDM_COMMAND_ENABLE_PROGRESS_DETECT = 20,
	FTDM_COMMAND_DISABLE_PROGRESS_DETECT = 21,

	/*!< Start tracing input and output from channel to the given file */
	FTDM_COMMAND_TRACE_INPUT = 22,
	FTDM_COMMAND_TRACE_OUTPUT = 23,

	/*!< Stop both Input and Output trace, closing the files */
	FTDM_COMMAND_TRACE_END_ALL = 24,

	/*!< Enable DTMF debugging */
	FTDM_COMMAND_ENABLE_DEBUG_DTMF = 25,

	/*!< Disable DTMF debugging (if not disabled explicitly, it is disabled automatically when calls hangup) */
	FTDM_COMMAND_DISABLE_DEBUG_DTMF = 26,

	/*!< Start dumping all input to a circular buffer. The size of the circular buffer can be specified, default used otherwise */
	FTDM_COMMAND_ENABLE_INPUT_DUMP = 27,

	/*!< Stop dumping all input to a circular buffer. */
	FTDM_COMMAND_DISABLE_INPUT_DUMP = 28,

	/*!< Start dumping all output to a circular buffer. The size of the circular buffer can be specified, default used otherwise */
	FTDM_COMMAND_ENABLE_OUTPUT_DUMP = 29,

	/*!< Stop dumping all output to a circular buffer. */
	FTDM_COMMAND_DISABLE_OUTPUT_DUMP = 30,

	/*!< Dump the current input circular buffer to the specified FILE* structure */
	FTDM_COMMAND_DUMP_INPUT = 31,

	/*!< Dump the current output circular buffer to the specified FILE* structure */
	FTDM_COMMAND_DUMP_OUTPUT = 32,

	FTDM_COMMAND_ENABLE_CALLERID_DETECT = 33,
	FTDM_COMMAND_DISABLE_CALLERID_DETECT = 34,
	FTDM_COMMAND_ENABLE_ECHOCANCEL = 35,
	FTDM_COMMAND_DISABLE_ECHOCANCEL = 36,
	FTDM_COMMAND_ENABLE_ECHOTRAIN = 37,
	FTDM_COMMAND_DISABLE_ECHOTRAIN = 38,
	FTDM_COMMAND_SET_CAS_BITS = 39,
	FTDM_COMMAND_GET_CAS_BITS = 40,
	FTDM_COMMAND_SET_RX_GAIN = 41,
	FTDM_COMMAND_GET_RX_GAIN = 42,
	FTDM_COMMAND_SET_TX_GAIN = 43,
	FTDM_COMMAND_GET_TX_GAIN = 44,
	FTDM_COMMAND_FLUSH_TX_BUFFERS = 45,
	FTDM_COMMAND_FLUSH_RX_BUFFERS = 46,
	FTDM_COMMAND_FLUSH_BUFFERS = 47,

	/*!< Flush IO statistics */
	FTDM_COMMAND_FLUSH_IOSTATS = 48,

	FTDM_COMMAND_SET_PRE_BUFFER_SIZE = 49,
	FTDM_COMMAND_SET_LINK_STATUS = 50,
	FTDM_COMMAND_GET_LINK_STATUS = 51,
	FTDM_COMMAND_ENABLE_LOOP = 52,
	FTDM_COMMAND_DISABLE_LOOP = 53,
	FTDM_COMMAND_SET_RX_QUEUE_SIZE = 54,
	FTDM_COMMAND_SET_TX_QUEUE_SIZE = 55,
	FTDM_COMMAND_SET_POLARITY = 56,
	FTDM_COMMAND_START_MF_PLAYBACK = 57,
	FTDM_COMMAND_STOP_MF_PLAYBACK = 58,

	/*!< Get a copy of the current IO stats */
	FTDM_COMMAND_GET_IOSTATS = 59,
	/*!< Enable/disable IO stats in the channel */
	FTDM_COMMAND_SWITCH_IOSTATS =  60,

	/*!< Enable/disable DTMF removal */
	FTDM_COMMAND_ENABLE_DTMF_REMOVAL = 61,
	FTDM_COMMAND_DISABLE_DTMF_REMOVAL = 62,

	FTDM_COMMAND_COUNT,
} ftdm_command_t;

typedef enum {
	FTDM_POLARITY_FORWARD = 0,
	FTDM_POLARITY_REVERSE = 1
} ftdm_polarity_t;

/*! \brief Custom memory handler hooks. Not recommended to use unless you need memory allocation customizations */
typedef void *(*ftdm_malloc_func_t)(void *pool, ftdm_size_t len);
typedef void *(*ftdm_calloc_func_t)(void *pool, ftdm_size_t elements, ftdm_size_t len);
typedef void *(*ftdm_realloc_func_t)(void *pool, void *buff, ftdm_size_t len);
typedef void (*ftdm_free_func_t)(void *pool, void *ptr);
struct ftdm_memory_handler {
	void *pool;
	ftdm_malloc_func_t malloc;
	ftdm_calloc_func_t calloc;
	ftdm_realloc_func_t realloc;
	ftdm_free_func_t free;
};

/*! \brief FreeTDM I/O layer interface argument macros 
 * You don't need these unless your implementing an I/O interface module (most users don't) */
#define FIO_CHANNEL_REQUEST_ARGS (ftdm_span_t *span, uint32_t chan_id, ftdm_direction_t direction, ftdm_caller_data_t *caller_data, ftdm_channel_t **ftdmchan)
#define FIO_CHANNEL_OUTGOING_CALL_ARGS (ftdm_channel_t *ftdmchan)
#define FIO_CHANNEL_INDICATE_ARGS (ftdm_channel_t *ftdmchan, ftdm_channel_indication_t indication)
#define FIO_CHANNEL_SET_SIG_STATUS_ARGS (ftdm_channel_t *ftdmchan, ftdm_signaling_status_t status)
#define FIO_CHANNEL_GET_SIG_STATUS_ARGS (ftdm_channel_t *ftdmchan, ftdm_signaling_status_t *status)
#define FIO_SPAN_SET_SIG_STATUS_ARGS (ftdm_span_t *span, ftdm_signaling_status_t status)
#define FIO_SPAN_GET_SIG_STATUS_ARGS (ftdm_span_t *span, ftdm_signaling_status_t *status)
#define FIO_SPAN_POLL_EVENT_ARGS (ftdm_span_t *span, uint32_t ms, short *poll_events)
#define FIO_SPAN_NEXT_EVENT_ARGS (ftdm_span_t *span, ftdm_event_t **event)
#define FIO_CHANNEL_NEXT_EVENT_ARGS (ftdm_channel_t *ftdmchan, ftdm_event_t **event)
#define FIO_SIGNAL_CB_ARGS (ftdm_sigmsg_t *sigmsg)
#define FIO_EVENT_CB_ARGS (ftdm_channel_t *ftdmchan, ftdm_event_t *event)
#define FIO_CONFIGURE_SPAN_ARGS (ftdm_span_t *span, const char *str, ftdm_chan_type_t type, char *name, char *number)
#define FIO_CONFIGURE_ARGS (const char *category, const char *var, const char *val, int lineno)
#define FIO_OPEN_ARGS (ftdm_channel_t *ftdmchan)
#define FIO_CLOSE_ARGS (ftdm_channel_t *ftdmchan)
#define FIO_CHANNEL_DESTROY_ARGS (ftdm_channel_t *ftdmchan)
#define FIO_SPAN_DESTROY_ARGS (ftdm_span_t *span)
#define FIO_COMMAND_ARGS (ftdm_channel_t *ftdmchan, ftdm_command_t command, void *obj)
#define FIO_WAIT_ARGS (ftdm_channel_t *ftdmchan, ftdm_wait_flag_t *flags, int32_t to)
#define FIO_GET_ALARMS_ARGS (ftdm_channel_t *ftdmchan)
#define FIO_READ_ARGS (ftdm_channel_t *ftdmchan, void *data, ftdm_size_t *datalen)
#define FIO_WRITE_ARGS (ftdm_channel_t *ftdmchan, void *data, ftdm_size_t *datalen)
#define FIO_IO_LOAD_ARGS (ftdm_io_interface_t **fio)
#define FIO_IO_UNLOAD_ARGS (void)
#define FIO_SIG_LOAD_ARGS (void)
#define FIO_SIG_CONFIGURE_ARGS (ftdm_span_t *span, fio_signal_cb_t sig_cb, va_list ap)
#define FIO_CONFIGURE_SPAN_SIGNALING_ARGS (ftdm_span_t *span, fio_signal_cb_t sig_cb, ftdm_conf_parameter_t *ftdm_parameters)
#define FIO_SIG_UNLOAD_ARGS (void)
#define FIO_API_ARGS (ftdm_stream_handle_t *stream, const char *data)
#define FIO_SPAN_START_ARGS (ftdm_span_t *span)
#define FIO_SPAN_STOP_ARGS (ftdm_span_t *span)

/*! \brief FreeTDM I/O layer interface function typedefs
 * You don't need these unless your implementing an I/O interface module (most users don't) */
typedef ftdm_status_t (*fio_channel_request_t) FIO_CHANNEL_REQUEST_ARGS ;
typedef ftdm_status_t (*fio_channel_outgoing_call_t) FIO_CHANNEL_OUTGOING_CALL_ARGS ;
typedef ftdm_status_t (*fio_channel_indicate_t) FIO_CHANNEL_INDICATE_ARGS;
typedef ftdm_status_t (*fio_channel_set_sig_status_t) FIO_CHANNEL_SET_SIG_STATUS_ARGS;
typedef ftdm_status_t (*fio_channel_get_sig_status_t) FIO_CHANNEL_GET_SIG_STATUS_ARGS;
typedef ftdm_status_t (*fio_span_set_sig_status_t) FIO_SPAN_SET_SIG_STATUS_ARGS;
typedef ftdm_status_t (*fio_span_get_sig_status_t) FIO_SPAN_GET_SIG_STATUS_ARGS;
typedef ftdm_status_t (*fio_span_poll_event_t) FIO_SPAN_POLL_EVENT_ARGS ;
typedef ftdm_status_t (*fio_span_next_event_t) FIO_SPAN_NEXT_EVENT_ARGS ;
typedef ftdm_status_t (*fio_channel_next_event_t) FIO_CHANNEL_NEXT_EVENT_ARGS ;

/*! \brief Callback for signal delivery (FTDM_SIGEVENT_START and friends) 
 *  \note This callback is provided by the user during ftdm_configure_span_signaling
 *
 *  \note You must NOT do any blocking during this callback since this function is
 *        most likely called in an internal signaling thread that can potentially be
 *        shared for all the channels in a span and blocking will delay processing
 *        (sometimes even audio processing) for other channels
 *
 *  \note Although some simple FreeTDM APIs can work (ie: ftdm_span_get_id etc), the
 *        use of any FreeTDM call API (ie ftdm_channel_call_answer) is discouraged
 */
typedef ftdm_status_t (*fio_signal_cb_t) FIO_SIGNAL_CB_ARGS ;

typedef ftdm_status_t (*fio_event_cb_t) FIO_EVENT_CB_ARGS ;
typedef ftdm_status_t (*fio_configure_span_t) FIO_CONFIGURE_SPAN_ARGS ;
typedef ftdm_status_t (*fio_configure_t) FIO_CONFIGURE_ARGS ;
typedef ftdm_status_t (*fio_open_t) FIO_OPEN_ARGS ;
typedef ftdm_status_t (*fio_close_t) FIO_CLOSE_ARGS ;
typedef ftdm_status_t (*fio_channel_destroy_t) FIO_CHANNEL_DESTROY_ARGS ;
typedef ftdm_status_t (*fio_span_destroy_t) FIO_SPAN_DESTROY_ARGS ;
typedef ftdm_status_t (*fio_get_alarms_t) FIO_GET_ALARMS_ARGS ;
typedef ftdm_status_t (*fio_command_t) FIO_COMMAND_ARGS ;
typedef ftdm_status_t (*fio_wait_t) FIO_WAIT_ARGS ;
typedef ftdm_status_t (*fio_read_t) FIO_READ_ARGS ;
typedef ftdm_status_t (*fio_write_t) FIO_WRITE_ARGS ;
typedef ftdm_status_t (*fio_io_load_t) FIO_IO_LOAD_ARGS ;
typedef ftdm_status_t (*fio_sig_load_t) FIO_SIG_LOAD_ARGS ;
typedef ftdm_status_t (*fio_sig_configure_t) FIO_SIG_CONFIGURE_ARGS ;
typedef ftdm_status_t (*fio_configure_span_signaling_t) FIO_CONFIGURE_SPAN_SIGNALING_ARGS ;
typedef ftdm_status_t (*fio_io_unload_t) FIO_IO_UNLOAD_ARGS ;
typedef ftdm_status_t (*fio_sig_unload_t) FIO_SIG_UNLOAD_ARGS ;
typedef ftdm_status_t (*fio_api_t) FIO_API_ARGS ;
typedef ftdm_status_t (*fio_span_start_t) FIO_SPAN_START_ARGS ;
typedef ftdm_status_t (*fio_span_stop_t) FIO_SPAN_STOP_ARGS ;


/*! \brief FreeTDM I/O layer interface function prototype wrapper macros
 * You don't need these unless your implementing an I/O interface module (most users don't) */
#define FIO_CHANNEL_REQUEST_FUNCTION(name) ftdm_status_t name FIO_CHANNEL_REQUEST_ARGS
#define FIO_CHANNEL_OUTGOING_CALL_FUNCTION(name) ftdm_status_t name FIO_CHANNEL_OUTGOING_CALL_ARGS
#define FIO_CHANNEL_INDICATE_FUNCTION(name) ftdm_status_t name FIO_CHANNEL_INDICATE_ARGS
#define FIO_CHANNEL_SET_SIG_STATUS_FUNCTION(name) ftdm_status_t name FIO_CHANNEL_SET_SIG_STATUS_ARGS
#define FIO_CHANNEL_GET_SIG_STATUS_FUNCTION(name) ftdm_status_t name FIO_CHANNEL_GET_SIG_STATUS_ARGS
#define FIO_SPAN_SET_SIG_STATUS_FUNCTION(name) ftdm_status_t name FIO_SPAN_SET_SIG_STATUS_ARGS
#define FIO_SPAN_GET_SIG_STATUS_FUNCTION(name) ftdm_status_t name FIO_SPAN_GET_SIG_STATUS_ARGS
#define FIO_SPAN_POLL_EVENT_FUNCTION(name) ftdm_status_t name FIO_SPAN_POLL_EVENT_ARGS
#define FIO_SPAN_NEXT_EVENT_FUNCTION(name) ftdm_status_t name FIO_SPAN_NEXT_EVENT_ARGS
#define FIO_CHANNEL_NEXT_EVENT_FUNCTION(name) ftdm_status_t name FIO_CHANNEL_NEXT_EVENT_ARGS
#define FIO_SIGNAL_CB_FUNCTION(name) ftdm_status_t name FIO_SIGNAL_CB_ARGS
#define FIO_EVENT_CB_FUNCTION(name) ftdm_status_t name FIO_EVENT_CB_ARGS
#define FIO_CONFIGURE_SPAN_FUNCTION(name) ftdm_status_t name FIO_CONFIGURE_SPAN_ARGS
#define FIO_CONFIGURE_FUNCTION(name) ftdm_status_t name FIO_CONFIGURE_ARGS
#define FIO_OPEN_FUNCTION(name) ftdm_status_t name FIO_OPEN_ARGS
#define FIO_CLOSE_FUNCTION(name) ftdm_status_t name FIO_CLOSE_ARGS
#define FIO_CHANNEL_DESTROY_FUNCTION(name) ftdm_status_t name FIO_CHANNEL_DESTROY_ARGS
#define FIO_SPAN_DESTROY_FUNCTION(name) ftdm_status_t name FIO_SPAN_DESTROY_ARGS
#define FIO_GET_ALARMS_FUNCTION(name) ftdm_status_t name FIO_GET_ALARMS_ARGS
#define FIO_COMMAND_FUNCTION(name) ftdm_status_t name FIO_COMMAND_ARGS
#define FIO_WAIT_FUNCTION(name) ftdm_status_t name FIO_WAIT_ARGS
#define FIO_READ_FUNCTION(name) ftdm_status_t name FIO_READ_ARGS
#define FIO_WRITE_FUNCTION(name) ftdm_status_t name FIO_WRITE_ARGS
#define FIO_IO_LOAD_FUNCTION(name) ftdm_status_t name FIO_IO_LOAD_ARGS
#define FIO_SIG_LOAD_FUNCTION(name) ftdm_status_t name FIO_SIG_LOAD_ARGS
#define FIO_SIG_CONFIGURE_FUNCTION(name) ftdm_status_t name FIO_SIG_CONFIGURE_ARGS
#define FIO_CONFIGURE_SPAN_SIGNALING_FUNCTION(name) ftdm_status_t name FIO_CONFIGURE_SPAN_SIGNALING_ARGS
#define FIO_IO_UNLOAD_FUNCTION(name) ftdm_status_t name FIO_IO_UNLOAD_ARGS
#define FIO_SIG_UNLOAD_FUNCTION(name) ftdm_status_t name FIO_SIG_UNLOAD_ARGS
#define FIO_API_FUNCTION(name) ftdm_status_t name FIO_API_ARGS
#define FIO_SPAN_START_FUNCTION(name) ftdm_status_t name FIO_SPAN_START_ARGS
#define FIO_SPAN_STOP_FUNCTION(name) ftdm_status_t name FIO_SPAN_STOP_ARGS

/*! \brief FreeTDM I/O layer function prototype wrapper macros
 * You don't need these unless your implementing an I/O interface module (most users don't) */
struct ftdm_io_interface {
	const char *name; /*!< I/O module name */
	fio_configure_span_t configure_span; /*!< Configure span I/O */
	fio_configure_t configure; /*!< Configure the module */
	fio_open_t open; /*!< Open I/O channel */
	fio_close_t close; /*!< Close I/O channel */
	fio_channel_destroy_t channel_destroy; /*!< Destroy I/O channel */
	fio_span_destroy_t span_destroy; /*!< Destroy span I/O */
	fio_get_alarms_t get_alarms; /*!< Get hardware alarms */
	fio_command_t command; /*!< Execute an I/O command on the channel */
	fio_wait_t wait; /*!< Wait for events on the channel */
	fio_read_t read; /*!< Read data from the channel */
	fio_write_t write; /*!< Write data to the channel */
	fio_span_poll_event_t poll_event; /*!< Poll for events on the whole span */
	fio_span_next_event_t next_event; /*!< Retrieve an event from the span */
	fio_channel_next_event_t channel_next_event; /*!< Retrieve an event from channel */
	fio_api_t api; /*!< Execute a text command */
	fio_span_start_t span_start; /*!< Start span I/O */
	fio_span_stop_t span_stop; /*!< Stop span I/O */
};

/*! \brief FreeTDM supported I/O codecs */
typedef enum {
	FTDM_CODEC_ULAW = 0,
	FTDM_CODEC_ALAW = 8,
	FTDM_CODEC_SLIN = 10,
	FTDM_CODEC_NONE = (1 << 30)
} ftdm_codec_t;

/*! \brief FreeTDM supported hardware alarms. */
typedef enum {
	FTDM_ALARM_NONE    = 0,
	FTDM_ALARM_RED     = (1 << 0),
	FTDM_ALARM_YELLOW  = (1 << 1),
	FTDM_ALARM_RAI     = (1 << 2),
	FTDM_ALARM_BLUE    = (1 << 3),
	FTDM_ALARM_AIS     = (1 << 4),
	FTDM_ALARM_GENERAL = (1 << 30)
} ftdm_alarm_flag_t;

/*! \brief MF generation direction flags 
 *  \note Used in bitwise OR with channel ID as argument to MF_PLAYBACK I/O command, so value must be higher that 255
 *  \see FTDM_COMMAND_START_MF_PLAYBACK
 * */

typedef enum {
	FTDM_MF_DIRECTION_FORWARD =  (1 << 8),
	FTDM_MF_DIRECTION_BACKWARD = (1 << 9)
} ftdm_mf_direction_flag_t;

/*! \brief IO Error statistics */
typedef enum {
	FTDM_IOSTATS_ERROR_CRC		= (1 << 0),
	FTDM_IOSTATS_ERROR_FRAME	= (1 << 1),
	FTDM_IOSTATS_ERROR_ABORT 	= (1 << 2),
	FTDM_IOSTATS_ERROR_FIFO 	= (1 << 3),
	FTDM_IOSTATS_ERROR_DMA		= (1 << 4),
	FTDM_IOSTATS_ERROR_QUEUE_THRES	= (1 << 5), /* Queue reached high threshold */
	FTDM_IOSTATS_ERROR_QUEUE_FULL	= (1 << 6), /* Queue is full */
} ftdm_iostats_error_type_t;

/*! \brief IO statistics */
typedef struct {
	struct {
		uint64_t packets;
		uint32_t errors;
		uint16_t flags;
		uint8_t	 queue_size;	/*!< max queue size configured */
		uint8_t	 queue_len;	/*!< Current number of elements in queue */
	} rx;

	struct {
		uint64_t idle_packets;
		uint64_t packets;
		uint32_t errors;
		uint16_t flags;
		uint8_t	 queue_size;	/*!< max queue size configured */
		uint8_t	 queue_len;	/*!< Current number of elements in queue */
	} tx;
} ftdm_channel_iostats_t;

/*! \brief Override the default queue handler */
FT_DECLARE(ftdm_status_t) ftdm_global_set_queue_handler(ftdm_queue_handler_t *handler);

/*! \brief Return the availability rate for a channel 
 * \param ftdmchan Channel to get the availability from
 *
 * \retval > 0 if availability is supported
 * \retval -1 if availability is not supported
 */
FT_DECLARE(int) ftdm_channel_get_availability(ftdm_channel_t *ftdmchan);

/*! \brief Answer call. This can also be accomplished by ftdm_channel_call_indicate with FTDM_CHANNEL_INDICATE_ANSWER, in both
 *         cases you will get a FTDM_SIGEVENT_INDICATION_COMPLETED when the indication is sent (or an error occurs).
 *         Just as with ftdm_channel_call_indicate you won't receive FTDM_SIGEVENT_INDICATION_COMPLETED when this function
 *         returns anything else than FTDM_SUCCESS
 *  \note Although this API may result in FTDM_SIGEVENT_INDICATION_COMPLETED event being delivered,
 *        there is no guarantee of whether the event will arrive after or before your execution thread returns
 *        from ftdm_channel_call_answer 
 */
#define ftdm_channel_call_answer(ftdmchan) _ftdm_channel_call_answer(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), NULL)
#define ftdm_channel_call_answer_ex(ftdmchan, usrmsg) _ftdm_channel_call_answer(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), (usrmsg))

/*! \brief Answer call recording the source code point where the it was called (see ftdm_channel_call_answer for an easy to use macro) */
FT_DECLARE(ftdm_status_t) _ftdm_channel_call_answer(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_usrmsg_t *usrmsg);

/*! \brief Place an outgoing call in the given channel 
 *  \deprecated This macro is deprecated since leaves the door open to glare issues, use ftdm_call_place instead
 */
#define ftdm_channel_call_place(ftdmchan) _ftdm_channel_call_place(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), NULL)
#define ftdm_channel_call_place_ex(ftdmchan, usrmsg) _ftdm_channel_call_place_ex(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), (usrmsg))

/*! \brief Place an outgoing call recording the source code point where it was called (see ftdm_channel_call_place for an easy to use macro)
 *  \deprecated This function is deprecated since leaves the door open to glare issues, use ftdm_call_place instead
 */
FT_DECLARE(ftdm_status_t) _ftdm_channel_call_place(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_usrmsg_t *usrmsg);

/*! \brief Place an outgoing call with the given caller data in a channel according to the hunting scheme provided */
#define ftdm_call_place(callerdata, hunting) _ftdm_call_place(__FILE__, __FUNCTION__, __LINE__, (callerdata), (hunting), NULL)
#define ftdm_call_place_ex(callerdata, hunting, usrmsg) _ftdm_call_place(__FILE__, __FUNCTION__, __LINE__, (callerdata), (hunting), (usrmsg))

/*! \brief Place an outgoing call with the given caller data in a channel according to the hunting scheme provided and records
 *         the place where it was called. See ftdm_call_place for an easy to use macro
 *  \return FTDM_SUCCESS if the call attempt was successful 
 *          FTDM_FAIL if there was an unspecified error
 *          FTDM_EBUSY if the channel was busy 
 *          FTDM_BREAK if glare was detected and you must try again
 *  \note Even when FTDM_SUCCESS is returned, the call may still fail later on due to glare, in such case FTDM_SIGEVENT_STOP
 *        will be sent with the hangup cause field set to FTDM_CAUSE_REQUESTED_CHAN_UNAVAIL
 *
 *  \note When this function returns FTDM_SUCCESS, the member .fchan from caller_data will be set to the channel used to place the call
 *        and .call_id to the generated call id for that call
 *
 *  \note When this function is successful you are guaranteed to receive FTDM_SIGEVENT_DIALING, this event could even be delivered
 *        before your execution thread returns from this function
 */
FT_DECLARE(ftdm_status_t) _ftdm_call_place(const char *file, const char *func, int line, ftdm_caller_data_t *caller_data, ftdm_hunting_scheme_t *hunting, ftdm_usrmsg_t *usrmsg);

/*! \brief Indicate a new condition in an incoming call 
 *
 *  \note Every indication request will result in FTDM_SIGEVENT_INDICATION_COMPLETED event being delivered with
 *        the proper status that will inform you if the request was successful or not. The exception is if this
 *        function returns something different to FTDM_SUCCESS, in which case the request failed right away and no
 *        further FTDM_SIGEVENT_INDICATION_COMPLETED will be delivered
 *        Be aware there is no guarantee of whether the completion event will arrive after or before your execution 
 *        thread returns from ftdm_channel_call_indicate. This means you could get FTDM_SIGEVENT_INDICATION_COMPLETED 
 *        even before your execution thread returns from the ftdm_channel_call_indicate() API
 *
 * \note  You cannot send more than one indication at the time. You must wait for the completed event before 
 *        calling this function again (unless the return code was different than FTDM_SUCCESS)
 */
#define ftdm_channel_call_indicate(ftdmchan, indication) _ftdm_channel_call_indicate(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), (indication), NULL)
#define ftdm_channel_call_indicate_ex(ftdmchan, indication, usrmsg) _ftdm_channel_call_indicate(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), (indication), (usrmsg))

/*! \brief Indicate a new condition in an incoming call recording the source code point where it was called (see ftdm_channel_call_indicate for an easy to use macro) */
FT_DECLARE(ftdm_status_t) _ftdm_channel_call_indicate(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_channel_indication_t indication, ftdm_usrmsg_t *usrmsg);

/*! \brief Hangup the call without cause */
#define ftdm_channel_call_hangup(ftdmchan) _ftdm_channel_call_hangup(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), NULL)
#define ftdm_channel_call_hangup_ex(ftdmchan, usrmsg) _ftdm_channel_call_hangup(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), (usrmsg))

/*! \brief Hangup the call without cause recording the source code point where it was called (see ftdm_channel_call_hangup for an easy to use macro)*/
FT_DECLARE(ftdm_status_t) _ftdm_channel_call_hangup(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_usrmsg_t *usrmsg);

/*! \brief Hangup the call with cause */
#define ftdm_channel_call_hangup_with_cause(ftdmchan, cause) _ftdm_channel_call_hangup_with_cause(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), (cause), NULL)
#define ftdm_channel_call_hangup_with_cause_ex(ftdmchan, cause, usrmsg) _ftdm_channel_call_hangup_with_cause(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), (cause), (usrmsg))

/*! \brief Hangup the call with cause recording the source code point where it was called (see ftdm_channel_call_hangup_with_cause for an easy to use macro) */
FT_DECLARE(ftdm_status_t) _ftdm_channel_call_hangup_with_cause(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_call_cause_t, ftdm_usrmsg_t *usrmsg);

/*! \brief Transfer call. This can also be accomplished by ftdm_channel_call_indicate with FTDM_CHANNEL_INDICATE_TRANSFER, in both
 *         cases you will get a FTDM_SIGEVENT_INDICATION_COMPLETED when the indication is sent (or an error occurs).
 *         Just as with ftdm_channel_call_indicate you won't receive FTDM_SIGEVENT_INDICATION_COMPLETED when this function
 *         returns anything else than FTDM_SUCCESS
 *  \note Although this API may result in FTDM_SIGEVENT_INDICATION_COMPLETED event being delivered,
 *        there is no guarantee of whether the event will arrive after or before your execution thread returns
 *        from ftdm_channel_call_transfer
 */
#define ftdm_channel_call_transfer(ftdmchan, arg) _ftdm_channel_call_transfer(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), (arg), NULL)
#define ftdm_channel_call_transfer_ex(ftdmchan, arg, usrmsg) _ftdm_channel_call_transfer(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), (arg), (usrmsg))

/*! \brief Answer call recording the source code point where the it was called (see ftdm_channel_call_tranasfer for an easy to use macro) */
FT_DECLARE(ftdm_status_t) _ftdm_channel_call_transfer(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, const char* arg, ftdm_usrmsg_t *usrmsg);

/*! \brief Reset the channel */
#define ftdm_channel_reset(ftdmchan) _ftdm_channel_reset(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), NULL)
#define ftdm_channel_reset_ex(ftdmchan, usrmsg) _ftdm_channel_reset(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), usrmsg)

/*! \brief Reset the channel (see _ftdm_channel_reset for an easy to use macro) 
 *  \note if there was a call on this channel, call will be cleared without any notifications to the user
 */
FT_DECLARE(ftdm_status_t) _ftdm_channel_reset(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_usrmsg_t *usrmsg);

/*! \brief Put a call on hold (if supported by the signaling stack) */
#define ftdm_channel_call_hold(ftdmchan) _ftdm_channel_call_hold(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), NULL)
#define ftdm_channel_call_hold_ex(ftdmchan, usrmsg) _ftdm_channel_call_hold(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), (usrmsg))

/*! \brief Put a call on hold recording the source code point where it was called (see ftdm_channel_call_hold for an easy to use macro) */
FT_DECLARE(ftdm_status_t) _ftdm_channel_call_hold(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_usrmsg_t *usrmsg);

/*! \brief Unhold a call */
#define ftdm_channel_call_unhold(ftdmchan) _ftdm_channel_call_unhold(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), NULL)
#define ftdm_channel_call_unhold_ex(ftdmchan, usrmsg) _ftdm_channel_call_unhold(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), (usrmsg))

/*! \brief Unhold a call recording the source code point where it was called (see ftdm_channel_call_unhold for an easy to use macro) */
FT_DECLARE(ftdm_status_t) _ftdm_channel_call_unhold(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_usrmsg_t *usrmsg);

/*! \brief Check if the call is answered already */
FT_DECLARE(ftdm_bool_t) ftdm_channel_call_check_answered(const ftdm_channel_t *ftdmchan);

/*! \brief Check if the call is busy */
FT_DECLARE(ftdm_bool_t) ftdm_channel_call_check_busy(const ftdm_channel_t *ftdmchan);

/*! \brief Check if the call is hangup */
FT_DECLARE(ftdm_bool_t) ftdm_channel_call_check_hangup(const ftdm_channel_t *ftdmchan);

/*! \brief Check if the call is done (final state for a call, just after hangup) */
FT_DECLARE(ftdm_bool_t) ftdm_channel_call_check_done(const ftdm_channel_t *ftdmchan);

/*! \brief Check if the call is in hold */
FT_DECLARE(ftdm_bool_t) ftdm_channel_call_check_hold(const ftdm_channel_t *ftdmchan);

/*! \brief Set channel signaling status (ie: put specific circuit down) only if supported by the signaling */
FT_DECLARE(ftdm_status_t) ftdm_channel_set_sig_status(ftdm_channel_t *ftdmchan, ftdm_signaling_status_t status);

/*! \brief Get channel signaling status (ie: whether protocol layer is up or down) */
FT_DECLARE(ftdm_status_t) ftdm_channel_get_sig_status(ftdm_channel_t *ftdmchan, ftdm_signaling_status_t *status);

/*! \brief Set span signaling status (ie: put the whole span protocol layer down) only if supported by the signaling */
FT_DECLARE(ftdm_status_t) ftdm_span_set_sig_status(ftdm_span_t *span, ftdm_signaling_status_t status);

/*! \brief Get span signaling status (ie: whether protocol layer is up or down) */
FT_DECLARE(ftdm_status_t) ftdm_span_get_sig_status(ftdm_span_t *span, ftdm_signaling_status_t *status);


/*! 
 * \brief Set user private data in the channel
 *
 * \param ftdmchan The channel where the private data will be stored
 * \param pvt The private pointer to store
 *
 */
FT_DECLARE(void) ftdm_channel_set_private(ftdm_channel_t *ftdmchan, void *pvt);

/*! 
 * \brief Get user private data in the channel
 *
 * \param ftdmchan The channel to retrieve the private data
 * \retval The private data (if any or NULL if no data has been stored)
 *
 */
FT_DECLARE(void *) ftdm_channel_get_private(const ftdm_channel_t *ftdmchan);

/*! 
 * \brief Remove the given token from the channel
 *
 * \param ftdmchan The channel where the token is
 * \param token The token string. If NULL, all tokens in the channel are cleared
 *
 * \retval FTDM_SUCCESS success
 * \retval FTDM_FAIL failure
 */
FT_DECLARE(ftdm_status_t) ftdm_channel_clear_token(ftdm_channel_t *ftdmchan, const char *token);

/*! 
 * \brief Replace the given token with the new token
 *
 * \param ftdmchan The channel where the token is
 * \param old_token The token to replace
 * \param new_token The token to put in place
 */
FT_DECLARE(void) ftdm_channel_replace_token(ftdm_channel_t *ftdmchan, const char *old_token, const char *new_token);

/*! 
 * \brief Add a new token to the channel
 *
 * \param ftdmchan The channel where the token will be added
 * \param token The token string to add
 * \param end if 0, the token will be added at the beginning of the token list, to the end otherwise
 *
 * \retval FTDM_SUCCESS success
 * \retval FTDM_FAIL failure
 */
FT_DECLARE(ftdm_status_t) ftdm_channel_add_token(ftdm_channel_t *ftdmchan, char *token, int end);

/*! 
 * \brief Get the requested token
 *
 * \param ftdmchan The channel where the token is
 * \param tokenid The id of the token
 *
 * \retval The token character string
 * \retval NULL token not found
 */
FT_DECLARE(const char *) ftdm_channel_get_token(const ftdm_channel_t *ftdmchan, uint32_t tokenid);

/*! 
 * \brief Get the token count
 *
 * \param ftdmchan The channel to get the token count from
 *
 * \retval The token count
 */
FT_DECLARE(uint32_t) ftdm_channel_get_token_count(const ftdm_channel_t *ftdmchan);

/*! 
 * \brief Get the I/O read/write interval
 *
 * \param ftdmchan The channel to get the interval from
 *
 * \retval The interval in milliseconds
 */
FT_DECLARE(uint32_t) ftdm_channel_get_io_interval(const ftdm_channel_t *ftdmchan);

/*! 
 * \brief Get the I/O read/write packet length per interval
 *
 * \param ftdmchan The channel to get the packet length from
 *
 * \retval The packet length interval in bytes
 */
FT_DECLARE(uint32_t) ftdm_channel_get_io_packet_len(const ftdm_channel_t *ftdmchan);

/*! 
 * \brief Get the I/O read/write codec
 *
 * \param ftdmchan The channel to get the codec from
 *
 * \retval The codec type
 */
FT_DECLARE(ftdm_codec_t) ftdm_channel_get_codec(const ftdm_channel_t *ftdmchan);

/*! 
 * \brief Get the last error string for the channel
 *
 * \deprecated This API will disappear in the future and not every 
 *             FreeTDM API set the last error value
 *
 * \param ftdmchan The channel to get the error from
 *
 * \retval The error string (not thread-safe, the string is per channel, not per thread)
 */
FT_DECLARE(const char *) ftdm_channel_get_last_error(const ftdm_channel_t *ftdmchan);

/*! 
 * \brief Get the current alarm bitmask for the channel
 *
 * \param ftdmchan The channel to get the alarm bitmask from
 * \param alarmbits The alarm bitmask pointer to store the current alarms (you are responsible for allocation/deallocation)
 *
 * \retval FTDM_SUCCESS success
 * \retval FTDM_FAIL failure
 */
FT_DECLARE(ftdm_status_t) ftdm_channel_get_alarms(ftdm_channel_t *ftdmchan, ftdm_alarm_flag_t *alarmbits);

/*! 
 * \brief Get the channel type
 *
 * \param ftdmchan The channel to get the type from
 *
 * \retval channel type (FXO, FXS, B-channel, D-channel, etc)
 */
FT_DECLARE(ftdm_chan_type_t) ftdm_channel_get_type(const ftdm_channel_t *ftdmchan);

/*! 
 * \brief Dequeue DTMF from the given channel
 * \note To transmit DTMF use ftdm_channel_command with command FTDM_COMMAND_SEND_DTMF
 *
 * \param ftdmchan The channel to dequeue DTMF from
 * \param dtmf DTMF buffer to store the dtmf (you are responsible for its allocation and deallocation)
 * \param len The size of the provided DTMF buffer
 *
 * \retval The size of the dequeued DTMF (it might be zero if there is no DTMF in the queue)
 */
FT_DECLARE(ftdm_size_t) ftdm_channel_dequeue_dtmf(ftdm_channel_t *ftdmchan, char *dtmf, ftdm_size_t len);

/*! 
 * \brief Flush the DTMF queue
 *
 * \param ftdmchan The channel to flush the dtmf queue of
 */
FT_DECLARE(void) ftdm_channel_flush_dtmf(ftdm_channel_t *ftdmchan);

/*! 
 * \brief Wait for an event in the span
 *
 * \param span The span to wait events for
 * \param ms Milliseconds timeout
 * \param poll_events Array of events to poll for, for each channel on the span
 *
 * \retval FTDM_SUCCESS success (at least one event available)
 * \retval FTDM_TIMEOUT Timed out waiting for events
 * \retval FTDM_FAIL failure
 */
FT_DECLARE(ftdm_status_t) ftdm_span_poll_event(ftdm_span_t *span, uint32_t ms, short *poll_events);

/*! 
 * \brief Find a span by its id
 *
 * \param id The span id 
 * \param span Pointer to store the span if found
 *
 * \retval FTDM_SUCCESS success (span is valid)
 * \retval FTDM_FAIL failure (span is not valid)
 */
FT_DECLARE(ftdm_status_t) ftdm_span_find(uint32_t id, ftdm_span_t **span);

/*! 
 * \brief Get the last error string for the given span
 *
 * \deprecated This API will disappear in the future and not every 
 *             FreeTDM API set the last error value
 *
 * \param span The span to get the last error from
 *
 * \retval character string for the last error
 */
FT_DECLARE(const char *) ftdm_span_get_last_error(const ftdm_span_t *span);

/*! 
 * \brief Create a new span (not needed if you are using freetdm.conf)
 *
 * \param iotype The I/O interface type this span will use. 
 *               This depends on the available I/O modules 
 *               ftmod_wanpipe = "wanpipe" (Sangoma)
 *               ftmod_zt = "zt" (DAHDI or Zaptel)
 *               ftmod_pika "pika" (this one is most likely broken)
 * \param name Name for the span
 * \param span Pointer to store the create span
 *
 * \retval FTDM_SUCCESS success (the span was created)
 * \retval FTDM_FAIL failure (span was not created)
 */
FT_DECLARE(ftdm_status_t) ftdm_span_create(const char *iotype, const char *name, ftdm_span_t **span);

/*! 
 * \brief Add a new channel to a span
 *
 * \param span Where to add the new channel
 * \param sockfd The socket device associated to the channel (ie: sangoma device, dahdi device etc)
 * \param type Channel type
 * \param chan Pointer to store the newly allocated channel
 *
 * \retval FTDM_SUCCESS success (the channel was created)
 * \retval FTDM_FAIL failure (span was not created)
 */
FT_DECLARE(ftdm_status_t) ftdm_span_add_channel(ftdm_span_t *span, ftdm_socket_t sockfd, ftdm_chan_type_t type, ftdm_channel_t **chan);

/*! \brief Add the channel to a hunt group */
FT_DECLARE(ftdm_status_t) ftdm_channel_add_to_group(const char* name, ftdm_channel_t* ftdmchan);

/*! \brief Remove the channel from a hunt group */
FT_DECLARE(ftdm_status_t) ftdm_channel_remove_from_group(ftdm_group_t* group, ftdm_channel_t* ftdmchan);

/*!
 * \brief Retrieves an event from the span
 *
 * \note
 * 	This function is non-reentrant and not thread-safe. 
 * 	The event returned may be modified if the function is called again 
 * 	from a different thread or even the same. It is recommended to
 * 	handle events from the same span in a single thread.
 *
 * \param ftdmchan The channel to retrieve the event from
 * \param event Pointer to store the pointer to the event
 *
 * \retval FTDM_SUCCESS success (at least one event available)
 * \retval FTDM_FAIL failure
 */
FT_DECLARE(ftdm_status_t) ftdm_channel_read_event(ftdm_channel_t *ftdmchan, ftdm_event_t **event);

/*! \brief Find a hunt group by id */
FT_DECLARE(ftdm_status_t) ftdm_group_find(uint32_t id, ftdm_group_t **group);

/*! \brief Find a hunt group by name */
FT_DECLARE(ftdm_status_t) ftdm_group_find_by_name(const char *name, ftdm_group_t **group);

/*! \brief Create a group with the given name */
FT_DECLARE(ftdm_status_t) ftdm_group_create(ftdm_group_t **group, const char *name);

/*! \brief Get the number of channels in use on a span */
FT_DECLARE(ftdm_status_t) ftdm_span_channel_use_count(ftdm_span_t *span, uint32_t *count);

/*! \brief Get the number of channels in use on a group */
FT_DECLARE(ftdm_status_t) ftdm_group_channel_use_count(ftdm_group_t *group, uint32_t *count);

/*! \brief Get the id of a group */
FT_DECLARE(uint32_t) ftdm_group_get_id(const ftdm_group_t *group);

/*! 
 * \brief Open a channel specifying the span id and chan id (required before placing a call on the channel)
 *
 * \warning Try using ftdm_call_place instead if you plan to place a call after opening the channel
 *
 * \note You must call ftdm_channel_close() or ftdm_channel_call_hangup() to release the channel afterwards
 * 	Only use ftdm_channel_close if there is no call (incoming or outgoing) in the channel
 *
 * \param span_id The span id the channel belongs to
 * \param chan_id Logical channel id of the channel you want to open
 * \param ftdmchan Pointer to store the channel once is open
 *
 * \retval FTDM_SUCCESS success (the channel was found and is available)
 * \retval FTDM_FAIL failure (channel was not found or not available)
 */
FT_DECLARE(ftdm_status_t) ftdm_channel_open(uint32_t span_id, uint32_t chan_id, ftdm_channel_t **ftdmchan);

/*! 
 * \brief Open a channel specifying the span id and physical chan id (required before placing a call on the channel)
 *
 * \warning Try using ftdm_call_place instead if you plan to place a call after opening the channel
 *
 * \note You must call ftdm_channel_close() or ftdm_channel_call_hangup() to release the channel afterwards
 * 	Only use ftdm_channel_close if there is no call (incoming or outgoing) in the channel
 *
 * \param span_id The span id the channel belongs to
 * \param chan_id Physical channel id of the channel you want to open
 * \param ftdmchan Pointer to store the channel once is open
 *
 * \retval FTDM_SUCCESS success (the channel was found and is available)
 * \retval FTDM_FAIL failure (channel was not found or not available)
 */
FT_DECLARE(ftdm_status_t) ftdm_channel_open_ph(uint32_t span_id, uint32_t chan_id, ftdm_channel_t **ftdmchan);

/*! 
 * \brief Hunts and opens a channel specifying the span id only
 *
 * \warning Try using ftdm_call_place instead if you plan to place a call after opening the channel
 *
 * \note You must call ftdm_channel_close() or ftdm_channel_call_hangup() to release the channel afterwards
 * 	Only use ftdm_channel_close if there is no call (incoming or outgoing) in the channel
 *
 * \param span_id The span id to hunt for a channel
 * \param direction The hunting direction
 * \param caller_data The calling party information
 * \param ftdmchan The channel pointer to store the available channel
 *
 * \retval FTDM_SUCCESS success (a suitable channel was found available)
 * \retval FTDM_FAIL failure (no suitable channel was found available)
 */
FT_DECLARE(ftdm_status_t) ftdm_channel_open_by_span(uint32_t span_id, ftdm_direction_t direction, ftdm_caller_data_t *caller_data, ftdm_channel_t **ftdmchan);

/*! 
 * \brief Hunts and opens a channel specifying group id
 *
 * \warning Try using ftdm_call_place instead if you plan to place a call after opening the channel
 *
 * \note You must call ftdm_channel_close() or ftdm_channel_call_hangup() to release the channel afterwards
 * 	Only use ftdm_channel_close if there is no call (incoming or outgoing) in the channel
 *
 * \param group_id The group id to hunt for a channel
 * \param direction The hunting direction
 * \param caller_data The calling party information
 * \param ftdmchan The channel pointer to store the available channel
 *
 * \retval FTDM_SUCCESS success (a suitable channel was found available)
 * \retval FTDM_FAIL failure (no suitable channel was found available)
 */
FT_DECLARE(ftdm_status_t) ftdm_channel_open_by_group(uint32_t group_id, ftdm_direction_t direction, ftdm_caller_data_t *caller_data, ftdm_channel_t **ftdmchan);

/*! 
 * \brief Close a previously open channel
 *
 * \warning FreeTDM is more and more a signaling API rather than just a plane IO API, unless you are using
 *          FreeTDM as a pure IO API without its signaling modules, you should not use this function
 *
 * \note If you placed a call in this channel use ftdm_channel_call_hangup(), you MUST NOT call this function, 
 *       the signaling stack will close the channel when the call is done.
 *
 * \param ftdmchan pointer to the channel to close
 *
 * \retval FTDM_SUCCESS success (a suitable channel was found available)
 * \retval FTDM_FAIL failure (no suitable channel was found available)
 */
FT_DECLARE(ftdm_status_t) ftdm_channel_close(ftdm_channel_t **ftdmchan);

/*! 
 * \brief Execute a command in a channel (same semantics as the ioctl() unix system call)
 *
 * \param ftdmchan The channel to execute the command
 * \param command The command to execute
 * \param arg The argument for the command
 *
 * \retval FTDM_SUCCESS success (a suitable channel was found available)
 * \retval FTDM_FAIL failure (no suitable channel was found available)
 */
FT_DECLARE(ftdm_status_t) ftdm_channel_command(ftdm_channel_t *ftdmchan, ftdm_command_t command, void *arg);

/*! 
 * \brief Wait for I/O events in a channel
 *
 * \param ftdmchan The channel to wait I/O for
 * \param flags The wait I/O flags
 * \param timeout The timeout in milliseconds
 *
 * \retval FTDM_SUCCESS success (a suitable channel was found available)
 * \retval FTDM_FAIL failure (no suitable channel was found available)
 */
FT_DECLARE(ftdm_status_t) ftdm_channel_wait(ftdm_channel_t *ftdmchan, ftdm_wait_flag_t *flags, int32_t timeout);

/*! 
 * \brief Read data from a channel
 *
 * \param ftdmchan The channel to read data from
 * \param data The pointer to the buffer to store the read data
 * \param datalen The size in bytes of the provided buffer
 *
 * \retval FTDM_SUCCESS success (a suitable channel was found available)
 * \retval FTDM_FAIL failure (no suitable channel was found available)
 */
FT_DECLARE(ftdm_status_t) ftdm_channel_read(ftdm_channel_t *ftdmchan, void *data, ftdm_size_t *datalen);

/*! 
 * \brief Write data to a channel
 *
 * \note The difference between data and datasize is subtle but important.
 *
 *       datalen is a pointer to the size of the actual data that you want to write. This pointer
 *       will be updated with the number of bytes actually written.
 *       
 *       datasize on the other hand is the size of the entire buffer provided in data, whether 
 *       all of that buffer is in use or not is a different matter. The difference becomes
 *       important only if you are using FreeTDM doing transcoding, for example, providing
 *       a ulaw frame of 160 bytes but where the I/O device accepts input in signed linear,
 *       the data to write will be 320 bytes, therefore datasize is expected to be at least
 *       320 where datalen would be just 160.
 *
 * \param ftdmchan The channel to write data to
 * \param data The pointer to the buffer to write
 * \param datasize The maximum number of bytes in data that can be used (in case transcoding is necessary)
 * \param datalen The size of the actual data
 *
 * \retval FTDM_SUCCESS success (a suitable channel was found available)
 * \retval FTDM_FAIL failure (no suitable channel was found available)
 */
FT_DECLARE(ftdm_status_t) ftdm_channel_write(ftdm_channel_t *ftdmchan, void *data, ftdm_size_t datasize, ftdm_size_t *datalen);

/*! \brief Get a custom variable from the sigmsg
 *  \note The variable pointer returned is only valid while the before the event is processed and it'll be destroyed once the event is processed. */
FT_DECLARE(const char *) ftdm_sigmsg_get_var(ftdm_sigmsg_t *sigmsg, const char *var_name);

/*! \brief Get an iterator to iterate over the sigmsg variables
 *  \param sigmsg The message structure containing the variables
 *  \param iter Optional iterator. You can reuse an old iterator (not previously freed) to avoid the extra allocation of a new iterator.
 *  \note The iterator pointer returned is only valid while the channel is open and it'll be destroyed when the channel is closed. 
 *        This iterator is completely non-thread safe, if you are adding variables or removing variables while iterating 
 *        results are unpredictable
 */
FT_DECLARE(ftdm_iterator_t *) ftdm_sigmsg_get_var_iterator(const ftdm_sigmsg_t *sigmsg, ftdm_iterator_t *iter);

/*! \brief Get raw data from sigmsg
 *  \param sigmsg The message structure containing the variables
 *  \param data	data will point to available data pointer if available
 *  \param datalen datalen will be set to length of data available
 *  \retval FTDM_SUCCESS data is available
 *  \retval FTDM_FAIL no data available
 *  \note data is only valid within the duration of the callback, to receive a data pointer that does not get
 *  \note destroyed when callback returns, see ftdm_sigmsg_get_raw_data_detached
 */
FT_DECLARE(ftdm_status_t) ftdm_sigmsg_get_raw_data(ftdm_sigmsg_t *sigmsg, void **data, ftdm_size_t *datalen);

/*! \brief Get raw data from event
 *  \param sigmsg The message structure containing the variables
 *  \param data	data will point to available data pointer if available
 *  \param datalen datalen will be set to length of data available
 *  \retval FTDM_SUCCESS data is available
 *  \retval FTDM_FAIL no data available
 *  \note Once this function returns, User owns data, and is responsible to free data using ftdm_safe_free();
 */
FT_DECLARE(ftdm_status_t) ftdm_sigmsg_get_raw_data_detached(ftdm_sigmsg_t *sigmsg, void **data, ftdm_size_t *datalen);

/*! \brief Add a custom variable to the user message
 *  \note This variables may be used by signaling modules to override signaling parameters
 *  \todo Document which signaling variables are available
 * */
FT_DECLARE(ftdm_status_t) ftdm_usrmsg_add_var(ftdm_usrmsg_t *usrmsg, const char *var_name, const char *value);

/*! \brief Attach raw data to usrmsg
 *  \param usrmsg The message structure containing the variables
 *  \param data pointer to data
 *  \param datalen datalen length of data
 *  \retval FTDM_SUCCESS success, data was successfully saved
 *  \retval FTDM_FAIL failed, event already had data attached to it.
 *  \note data must have been allocated using ftdm_calloc, FreeTDM will free data once the usrmsg is processed.
 */
FT_DECLARE(ftdm_status_t) ftdm_usrmsg_set_raw_data(ftdm_usrmsg_t *usrmsg, void *data, ftdm_size_t datalen);

/*! \brief Get iterator current value (depends on the iterator type)
 *  \note Channel iterators return a pointer to ftdm_channel_t
 *        Variable iterators return a pointer to the variable name (not the variable value)
 */
FT_DECLARE(void *) ftdm_iterator_current(ftdm_iterator_t *iter);

/*! \brief Get variable name and value for the current iterator position */
FT_DECLARE(ftdm_status_t) ftdm_get_current_var(ftdm_iterator_t *iter, const char **var_name, const char **var_val);

/*! \brief Advance iterator */
FT_DECLARE(ftdm_iterator_t *) ftdm_iterator_next(ftdm_iterator_t *iter);

/*! \brief Free iterator 
 *  \note You must free an iterator after using it unless you plan to reuse it
 */
FT_DECLARE(ftdm_status_t) ftdm_iterator_free(ftdm_iterator_t *iter);

/*! \brief Get the span pointer associated to the channel */
FT_DECLARE(ftdm_span_t *) ftdm_channel_get_span(const ftdm_channel_t *ftdmchan);

/*! \brief Get the span pointer associated to the channel */
FT_DECLARE(uint32_t) ftdm_channel_get_span_id(const ftdm_channel_t *ftdmchan);

/*! \brief Get the physical span id associated to the channel */
FT_DECLARE(uint32_t) ftdm_channel_get_ph_span_id(const ftdm_channel_t *ftdmchan);

/*! \brief Get the span name associated to the channel */
FT_DECLARE(const char *) ftdm_channel_get_span_name(const ftdm_channel_t *ftdmchan);

/*! \brief Get the id associated to the channel */
FT_DECLARE(uint32_t) ftdm_channel_get_id(const ftdm_channel_t *ftdmchan);

/*! \brief Get the name associated to the channel */
FT_DECLARE(const char *) ftdm_channel_get_name(const ftdm_channel_t *ftdmchan);

/*! \brief Get the number associated to the channel */
FT_DECLARE(const char *) ftdm_channel_get_number(const ftdm_channel_t *ftdmchan);

/*! \brief Get the number physical id associated to the channel */
FT_DECLARE(uint32_t) ftdm_channel_get_ph_id(const ftdm_channel_t *ftdmchan);

/*! 
 * \brief Configure span with a signaling type 
 *
 * \deprecated use ftdm_configure_span_signaling instead
 *
 * \note This function does the same as ftdm_configure_span_signaling
 *
 * \param span The span to configure
 * \param type The signaling type ("boost", "isdn" and others, this depends on the available signaling modules)
 * \param sig_cb The callback that the signaling stack will use to notify about events
 * \param ... variable argument list with "var", value sequence, the variable and values are signaling type dependant
 *        the last argument must be FTDM_TAG_END
 *
 * \retval FTDM_SUCCESS success 
 * \retval FTDM_FAIL failure 
 */
FT_DECLARE(ftdm_status_t) ftdm_configure_span(ftdm_span_t *span, const char *type, fio_signal_cb_t sig_cb, ...);
#define FTDM_TAG_END NULL


/*! 
 * \brief Configure span with a signaling type
 *
 * \param span The span to configure
 * \param type The signaling type ("boost", "isdn" and others, this depends on the available signaling modules)
 * \param sig_cb The callback that the signaling stack will use to notify about events
 * \param parameters The array if signaling-specific parameters (the last member of the array MUST have its var member set to NULL, ie: .var = NULL)
 *
 * \retval FTDM_SUCCESS success 
 * \retval FTDM_FAIL failure 
 */
FT_DECLARE(ftdm_status_t) ftdm_configure_span_signaling(ftdm_span_t *span, const char *type, fio_signal_cb_t sig_cb, ftdm_conf_parameter_t *parameters);

/*! 
 * \brief Register callback to listen for incoming events
 * \note  This function should only be used when there is no signalling module
 * \param span The span to register to
 * \param sig_cb The callback that the signaling stack will use to notify about events
 *
 * \retval FTDM_SUCCESS success
 * \retval FTDM_FAIL failure
 */
FT_DECLARE(ftdm_status_t) ftdm_span_register_signal_cb(ftdm_span_t *span, fio_signal_cb_t sig_cb);

/*! 
 * \brief Start the span signaling (must call ftdm_configure_span_signaling first)
 *
 * \note Even before this function returns you may receive signaling events!
 * 	 Never block in the signaling callback since it might be called in a thread
 * 	 that handles more than 1 call and therefore you would be blocking all the
 * 	 calls handled by that thread!
 *
 * \param span The span to start
 *
 * \retval FTDM_SUCCESS success 
 * \retval FTDM_FAIL failure 
 */
FT_DECLARE(ftdm_status_t) ftdm_span_start(ftdm_span_t *span);

/*! 
 * \brief Stop the span signaling (must call ftdm_span_start first)
 * \note certain signalings (boost signaling) does not support granular span start/stop
 * so it is recommended to always configure all spans and then starting them all and finally
 * stop them all (or call ftdm_global_destroy which takes care of stopping and destroying the spans at once).
 *
 * \param span The span to stop
 *
 * \retval FTDM_SUCCESS success 
 * \retval FTDM_FAIL failure 
 */
FT_DECLARE(ftdm_status_t) ftdm_span_stop(ftdm_span_t *span);

/*! 
 * \brief Register a custom I/O interface with the FreeTDM core
 *
 * \param io_interface the Interface to register
 *
 * \retval FTDM_SUCCESS success 
 * \retval FTDM_FAIL failure 
 */
FT_DECLARE(ftdm_status_t) ftdm_global_add_io_interface(ftdm_io_interface_t *io_interface);

/*! \brief Find a span by name */
FT_DECLARE(ftdm_status_t) ftdm_span_find_by_name(const char *name, ftdm_span_t **span);

/*! \brief Get the span id */
FT_DECLARE(uint32_t) ftdm_span_get_id(const ftdm_span_t *span);

/*! \brief Get the span name */
FT_DECLARE(const char *) ftdm_span_get_name(const ftdm_span_t *span);

/*! \brief Get iterator for the span channels
 *  \param span The span containing the channels
 *  \param iter Optional iterator. You can reuse an old iterator (not previously freed) to avoid the extra allocation of a new iterator.
 */
FT_DECLARE(ftdm_iterator_t *) ftdm_span_get_chan_iterator(const ftdm_span_t *span, ftdm_iterator_t *iter);

/*! 
 * \brief Execute a text command. The text command output will be returned and must be free'd 
 *
 * \param cmd The command to execute
 *
 * \retval FTDM_SUCCESS success 
 * \retval FTDM_FAIL failure 
 */
FT_DECLARE(char *) ftdm_api_execute(const char *cmd);

/*! 
 * \brief Create a configuration node
 *
 * \param name The name of the configuration node
 * \param node The node pointer to store the new node
 * \param parent The parent node if any, or NULL if no parent
 *
 * \return FTDM_SUCCESS success
 * \return FTDM_FAIL failure
 */
FT_DECLARE(ftdm_status_t) ftdm_conf_node_create(const char *name, ftdm_conf_node_t **node, ftdm_conf_node_t *parent);

/*! 
 * \brief Adds a new parameter to the specified configuration node
 *
 * \param node The configuration node to add the param-val pair to
 * \param param The parameter name
 * \param val The parameter value
 *
 * \return FTDM_SUCCESS success
 * \return FTDM_FAIL failure
 */
FT_DECLARE(ftdm_status_t) ftdm_conf_node_add_param(ftdm_conf_node_t *node, const char *param, const char *val);

/*! 
 * \brief Destroy the memory allocated for a configuration node (and all of its descendance)
 *
 * \param node The node to destroy
 *
 * \return FTDM_SUCCESS success
 * \return FTDM_FAIL failure
 */
FT_DECLARE(ftdm_status_t) ftdm_conf_node_destroy(ftdm_conf_node_t *node);

/*! 
 * \brief Create and configure channels in the given span
 *
 * \param span The span container
 * \param str The channel range null terminated string. "1-10", "24" etc
 * \param chan_config The basic channel configuration for each channel within the range
 * \param configured Pointer where the number of channels configured will be stored
 *
 * \return FTDM_SUCCESS success
 * \return FTDM_FAIL failure
 */
FT_DECLARE(ftdm_status_t) ftdm_configure_span_channels(ftdm_span_t *span, const char *str, ftdm_channel_config_t *chan_config, unsigned *configured);

/*! 
 * \brief Set the trunk type for a span
 *        This must be called before configuring any channels within the span
 *
 * \param span The span 
 * \param type The trunk type
 *
 */
FT_DECLARE(void) ftdm_span_set_trunk_type(ftdm_span_t *span, ftdm_trunk_type_t type);

/*! 
 * \brief Get the trunk type for a span
 *
 * \param span The span 
 *
 * \return The span trunk type
 */
FT_DECLARE(ftdm_trunk_type_t) ftdm_span_get_trunk_type(const ftdm_span_t *span);

/*! \brief For display debugging purposes you can display this string which describes the trunk type of a span */
FT_DECLARE(const char *) ftdm_span_get_trunk_type_str(const ftdm_span_t *span);

/*!
 * Set the trunk mode for a span
 * \note	This must be called before configuring any channels within the span!
 * \param[in]	span	The span
 * \param[in]	type	The trunk mode
 */
FT_DECLARE(void) ftdm_span_set_trunk_mode(ftdm_span_t *span, ftdm_trunk_mode_t mode);

/*!
 * Get the trunk mode for a span
 * \param[in]	span	The span
 * \return	Span trunk mode
 */
FT_DECLARE(ftdm_trunk_mode_t) ftdm_span_get_trunk_mode(const ftdm_span_t *span);

/*!
 * Get the trunk mode of a span in textual form
 * \param[in]	span	The span
 * \return	Span mode name as a string
 */
FT_DECLARE(const char *) ftdm_span_get_trunk_mode_str(const ftdm_span_t *span);

/*! 
 * \brief Return the channel identified by the provided logical id
 *
 * \param span The span where the channel belongs
 * \param chanid The logical channel id within the span
 *
 * \return The channel pointer if found, NULL otherwise
 */
FT_DECLARE(ftdm_channel_t *) ftdm_span_get_channel(const ftdm_span_t *span, uint32_t chanid);

/*! 
 * \brief Return the channel identified by the provided physical id
 *
 * \param span The span where the channel belongs
 * \param chanid The physical channel id within the span
 *
 * \return The channel pointer if found, NULL otherwise
 */
FT_DECLARE(ftdm_channel_t *) ftdm_span_get_channel_ph(const ftdm_span_t *span, uint32_t chanid);

/*! \brief Return the channel count number for the given span */
FT_DECLARE(uint32_t) ftdm_span_get_chan_count(const ftdm_span_t *span);

/*! \brief Set the caller data for a channel. Be sure to call this before ftdm_channel_call_place() */
FT_DECLARE(ftdm_status_t) ftdm_channel_set_caller_data(ftdm_channel_t *ftdmchan, ftdm_caller_data_t *caller_data);

/*! \brief Get the caller data for a channel, typically you need this when receiving FTDM_SIGEVENT_START */
FT_DECLARE(ftdm_caller_data_t *) ftdm_channel_get_caller_data(ftdm_channel_t *channel);

/*! \brief Get current state of a channel */
FT_DECLARE(int) ftdm_channel_get_state(const ftdm_channel_t *ftdmchan);

/*! \brief Get last state of a channel */
FT_DECLARE(int) ftdm_channel_get_last_state(const ftdm_channel_t *ftdmchan);

/*! \brief For display debugging purposes you can display this string which describes the current channel internal state */
FT_DECLARE(const char *) ftdm_channel_get_state_str(const ftdm_channel_t *channel);

/*! \brief For display debugging purposes you can display this string which describes the last channel internal state */
FT_DECLARE(const char *) ftdm_channel_get_last_state_str(const ftdm_channel_t *channel);

/*! \brief For display debugging purposes you can display this string which describes the history of the channel 
 *  \param channel The channel to get the history from
 *  \return History string for the channel. You must free the string with ftdm_free
 */
FT_DECLARE(char *) ftdm_channel_get_history_str(const ftdm_channel_t *channel);

/*! \brief Enable/disable blocking mode in the channels for this span */
FT_DECLARE(ftdm_status_t) ftdm_span_set_blocking_mode(const ftdm_span_t *span, ftdm_bool_t enabled);

/*! \brief Initialize the library */
FT_DECLARE(ftdm_status_t) ftdm_global_init(void);

/*! \brief Create spans and channels reading the freetdm.conf file */
FT_DECLARE(ftdm_status_t) ftdm_global_configuration(void);

/*! \brief Shutdown the library */
FT_DECLARE(ftdm_status_t) ftdm_global_destroy(void);

/*! \brief Set memory handler for the library */
FT_DECLARE(ftdm_status_t) ftdm_global_set_memory_handler(ftdm_memory_handler_t *handler);

/*! \brief Set the crash policy for the library */
FT_DECLARE(void) ftdm_global_set_crash_policy(ftdm_crash_policy_t policy);

/*! \brief Set the logger handler for the library */
FT_DECLARE(void) ftdm_global_set_logger(ftdm_logger_t logger);

/*! \brief Set the default logger level */
FT_DECLARE(void) ftdm_global_set_default_logger(int level);

/*! \brief Set the directory to look for modules */
FT_DECLARE(void) ftdm_global_set_mod_directory(const char *path);

/*! \brief Set the directory to look for configs */
FT_DECLARE(void) ftdm_global_set_config_directory(const char *path);

/*! \brief Check if the FTDM library is initialized and running */
FT_DECLARE(ftdm_bool_t) ftdm_running(void);

/**
 * Generate a stack trace and invoke a callback function for each entry
 * \param[in]	callback	Callback function, that is invoked for each stack symbol
 * \param[in]	priv		(User-)Private data passed to the callback
 * \retval
 *	FTDM_SUCCESS	On success
 *	FTDM_NOTIMPL	Backtraces are not available
 *	FTDM_EINVAL	Invalid arguments (callback was NULL)
 */
FT_DECLARE(ftdm_status_t) ftdm_backtrace_walk(void (* callback)(const int tid, const void *addr, const char *symbol, void *priv), void *priv);

/**
 * Convenience function to print a backtrace for a span.
 * \note	The backtrace is generated with FTDM_LOG_DEBUG log level.
 * \param[in]	span	Span object
 * \retval
 *	FTDM_SUCCESS	On success
 *	FTDM_NOTIMPL	Backtraces are not available
 *	FTDM_EINVAL	Invalid arguments (e.g. span was NULL)
 */
FT_DECLARE(ftdm_status_t) ftdm_backtrace_span(ftdm_span_t *span);

/**
 * Convenience function to print a backtrace for a channel.
 * \note	The backtrace is generated with FTDM_LOG_DEBUG log level.
 * \param[in]	chan	Channel object
 * \retval
 *	FTDM_SUCCESS	On success
 *	FTDM_NOTIMPL	Backtraces are not available
 *	FTDM_EINVAL	Invalid arguments (e.g. chan was NULL)
 */
FT_DECLARE(ftdm_status_t) ftdm_backtrace_chan(ftdm_channel_t *chan);


FT_DECLARE_DATA extern ftdm_logger_t ftdm_log;

/*! \brief Basic transcoding function prototype */
#define FIO_CODEC_ARGS (void *data, ftdm_size_t max, ftdm_size_t *datalen)
#define FIO_CODEC_FUNCTION(name) FT_DECLARE_NONSTD(ftdm_status_t) name FIO_CODEC_ARGS
typedef ftdm_status_t (*fio_codec_t) FIO_CODEC_ARGS ;

/*! \brief Basic transcoding functions */
FIO_CODEC_FUNCTION(fio_slin2ulaw);
FIO_CODEC_FUNCTION(fio_ulaw2slin);
FIO_CODEC_FUNCTION(fio_slin2alaw);
FIO_CODEC_FUNCTION(fio_alaw2slin);
FIO_CODEC_FUNCTION(fio_ulaw2alaw);
FIO_CODEC_FUNCTION(fio_alaw2ulaw);

#define FTDM_PRE __FILE__, __FUNCTION__, __LINE__
#define FTDM_LOG_LEVEL_DEBUG 7
#define FTDM_LOG_LEVEL_INFO 6
#define FTDM_LOG_LEVEL_NOTICE 5
#define FTDM_LOG_LEVEL_WARNING 4
#define FTDM_LOG_LEVEL_ERROR 3
#define FTDM_LOG_LEVEL_CRIT 2
#define FTDM_LOG_LEVEL_ALERT 1
#define FTDM_LOG_LEVEL_EMERG 0

/*! \brief Log levels  */
#define FTDM_LOG_DEBUG FTDM_PRE, FTDM_LOG_LEVEL_DEBUG
#define FTDM_LOG_INFO FTDM_PRE, FTDM_LOG_LEVEL_INFO
#define FTDM_LOG_NOTICE FTDM_PRE, FTDM_LOG_LEVEL_NOTICE
#define FTDM_LOG_WARNING FTDM_PRE, FTDM_LOG_LEVEL_WARNING
#define FTDM_LOG_ERROR FTDM_PRE, FTDM_LOG_LEVEL_ERROR
#define FTDM_LOG_CRIT FTDM_PRE, FTDM_LOG_LEVEL_CRIT
#define FTDM_LOG_ALERT FTDM_PRE, FTDM_LOG_LEVEL_ALERT
#define FTDM_LOG_EMERG FTDM_PRE, FTDM_LOG_LEVEL_EMERG

#ifdef __cplusplus
} /* extern C */
#endif

#endif

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
