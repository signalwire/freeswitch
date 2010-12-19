/*
 * Copyright (c) 2007, Anthony Minessale II
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
 *
 */

#ifndef FREETDM_H
#define FREETDM_H


#include "ftdm_declare.h"
#include "ftdm_call_utils.h"

/*! \brief Max number of channels per physical span */
#define FTDM_MAX_CHANNELS_PHYSICAL_SPAN 32

/*! \brief Max number of physical spans per logical span */
#define FTDM_MAX_PHYSICAL_SPANS_PER_LOGICAL_SPAN 32

/*! \brief Max number of channels a logical span can contain */
#define FTDM_MAX_CHANNELS_SPAN FTDM_MAX_CHANNELS_PHYSICAL_SPAN * FTDM_MAX_PHYSICAL_SPANS_PER_LOGICAL_SPAN

/*! \brief Max number of logical spans */
#define FTDM_MAX_SPANS_INTERFACE 128

/*! \brief Max number of channels per hunting group */
#define FTDM_MAX_CHANNELS_GROUP 1024

/*! \brief Max number of groups */
#define FTDM_MAX_GROUPS_INTERFACE FTDM_MAX_SPANS_INTERFACE

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
	FTDM_CAUSE_CHANNEL_UNACCEPTABLE = 6,
	FTDM_CAUSE_CALL_AWARDED_DELIVERED = 7,
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
	FTDM_CAUSE_NORMAL_TEMPORARY_FAILURE = 41,
	FTDM_CAUSE_SWITCH_CONGESTION = 42,
	FTDM_CAUSE_ACCESS_INFO_DISCARDED = 43,
	FTDM_CAUSE_REQUESTED_CHAN_UNAVAIL = 44,
	FTDM_CAUSE_PRE_EMPTED = 45,
	FTDM_CAUSE_FACILITY_NOT_SUBSCRIBED = 50,
	FTDM_CAUSE_OUTGOING_CALL_BARRED = 52,
	FTDM_CAUSE_INCOMING_CALL_BARRED = 54,
	FTDM_CAUSE_BEARERCAPABILITY_NOTAUTH = 57,
	FTDM_CAUSE_BEARERCAPABILITY_NOTAVAIL = 58,
	FTDM_CAUSE_SERVICE_UNAVAILABLE = 63,
	FTDM_CAUSE_BEARERCAPABILITY_NOTIMPL = 65,
	FTDM_CAUSE_CHAN_NOT_IMPLEMENTED = 66,
	FTDM_CAUSE_FACILITY_NOT_IMPLEMENTED = 69,
	FTDM_CAUSE_SERVICE_NOT_IMPLEMENTED = 79,
	FTDM_CAUSE_INVALID_CALL_REFERENCE = 81,
	FTDM_CAUSE_INCOMPATIBLE_DESTINATION = 88,
	FTDM_CAUSE_INVALID_MSG_UNSPECIFIED = 95,
	FTDM_CAUSE_MANDATORY_IE_MISSING = 96,
	FTDM_CAUSE_MESSAGE_TYPE_NONEXIST = 97,
	FTDM_CAUSE_WRONG_MESSAGE = 98,
	FTDM_CAUSE_IE_NONEXIST = 99,
	FTDM_CAUSE_INVALID_IE_CONTENTS = 100,
	FTDM_CAUSE_WRONG_CALL_STATE = 101,
	FTDM_CAUSE_RECOVERY_ON_TIMER_EXPIRE = 102,
	FTDM_CAUSE_MANDATORY_IE_LENGTH_ERROR = 103,
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
	FTDM_CHAN_TYPE_DQ921, /*< DQ921 channel (D-channel) */
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
#define FTDM_IS_VOICE_CHANNEL(ftdm_chan) ((ftdm_chan)->type != FTDM_CHAN_TYPE_DQ921 && (ftdm_chan)->type != FTDM_CHAN_TYPE_DQ931)

/*! \brief Test if a channel is a D-channel */
#define FTDM_IS_DCHAN(ftdm_chan) ((ftdm_chan)->type == FTDM_CHAN_TYPE_DQ921 || (ftdm_chan)->type == FTDM_CHAN_TYPE_DQ931)

/*! \brief Logging function prototype to be used for all FreeTDM logs 
 *  you should use ftdm_global_set_logger to set your own logger
 */
typedef void (*ftdm_logger_t)(const char *file, const char *func, int line, int level, const char *fmt, ...);

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
	FTDM_NPI_ISDN = 1,
	FTDM_NPI_DATA = 3,
	FTDM_NPI_TELEX = 4,
	FTDM_NPI_NATIONAL = 8,
	FTDM_NPI_PRIVATE = 9,
	FTDM_NPI_RESERVED = 10,
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
	FTDM_BEARER_CAP_SPEECH = 0x00,
	FTDM_BEARER_CAP_64K_UNRESTRICTED = 0x02,
	FTDM_BEARER_CAP_3_1KHZ_AUDIO = 0x03,
	FTDM_BEARER_CAP_INVALID
} ftdm_bearer_cap_t;
#define BEARER_CAP_STRINGS "speech", "unrestricted-digital-information", "3.1-Khz-audio", "invalid"
FTDM_STR2ENUM_P(ftdm_str2ftdm_bearer_cap, ftdm_bearer_cap2str, ftdm_bearer_cap_t)

/*! \brief user information layer 1 protocol */
typedef enum {
	FTDM_USER_LAYER1_PROT_V110 = 0x01,
	FTDM_USER_LAYER1_PROT_ULAW = 0x02,
	FTDM_USER_LAYER1_PROT_ALAW = 0x03,
	FTDM_USER_LAYER1_PROT_INVALID
} ftdm_user_layer1_prot_t;
#define USER_LAYER1_PROT_STRINGS "V.110", "u-law", "a-law", "Invalid"
FTDM_STR2ENUM_P(ftdm_str2ftdm_usr_layer1_prot, ftdm_user_layer1_prot2str, ftdm_user_layer1_prot_t)

/*! Calling Party Category */
typedef enum {
	FTDM_CPC_UNKNOWN,
	FTDM_CPC_OPERATOR,
	FTDM_CPC_ORDINARY,
	FTDM_CPC_PRIORITY,
	FTDM_CPC_DATA,
	FTDM_CPC_TEST,
	FTDM_CPC_PAYPHONE,
	FTDM_CPC_INVALID
} ftdm_calling_party_category_t;
#define CALLING_PARTY_CATEGORY_STRINGS "unknown", "operator", "ordinary", "priority", "data-call", "test-call", "payphone", "invalid"
FTDM_STR2ENUM_P(ftdm_str2ftdm_calling_party_category, ftdm_calling_party_category2str, ftdm_calling_party_category_t)


/*! \brief Number abstraction */
typedef struct {
	char digits[25];
	uint8_t type;
	uint8_t plan;
} ftdm_number_t;

typedef void * ftdm_variable_container_t; 
									 
/*! \brief Caller information */
typedef struct ftdm_caller_data {
	char cid_date[8]; /*!< Caller ID date */
	char cid_name[80]; /*!< Caller ID name */
	ftdm_number_t cid_num; /*!< Caller ID number */
	ftdm_number_t ani; /*!< ANI (Automatic Number Identification) */
	ftdm_number_t dnis; /*!< DNIS (Dialed Number Identification Service) */
	ftdm_number_t rdnis; /*!< RDNIS (Redirected Dialed Number Identification Service) */
	char aniII[25]; /*! ANI II */
	uint8_t screen; /*!< Screening */
	uint8_t pres; /*!< Presentation*/
	char collected[25]; /*!< Collected digits so far */
	int hangup_cause; /*!< Hangup cause */
	char raw_data[1024]; /*!< Protocol specific raw caller data */
	uint32_t raw_data_len; /*!< Raw data length */
	/* these 2 are undocumented right now, only used by boost: */
	/* bearer capability */
	ftdm_bearer_cap_t bearer_capability;
	/* user information layer 1 protocol */
	ftdm_user_layer1_prot_t bearer_layer1;
	ftdm_calling_party_category_t cpc; /*!< Calling party category */
	ftdm_variable_container_t variables; /*!< Variables attached to this call */
	/* We need call_id inside caller_data for the user to be able to retrieve 
	 * the call_id when ftdm_channel_call_place is called. This is the only time
	 * that the user can use caller_data.call_id to obtain the call_id. The user
	 * should use the call_id from sigmsg otherwise */
	uint32_t call_id; /*!< Unique call ID for this call */
} ftdm_caller_data_t;

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
	FTDM_SIGEVENT_PROCEED, /*!< Outgoing call got a response */
	FTDM_SIGEVENT_RINGING, /*!< Remote side is in ringing state */
	FTDM_SIGEVENT_PROGRESS, /*!< Outgoing call is making progress */
	FTDM_SIGEVENT_PROGRESS_MEDIA, /*!< Outgoing call is making progress and there is media available */
	FTDM_SIGEVENT_ALARM_TRAP, /*!< Hardware alarm ON */
	FTDM_SIGEVENT_ALARM_CLEAR, /*!< Hardware alarm OFF */
	FTDM_SIGEVENT_COLLECTED_DIGIT, /*!< Digit collected (in signalings where digits are collected one by one) */
	FTDM_SIGEVENT_ADD_CALL, /*!< New call should be added to the channel */
	FTDM_SIGEVENT_RESTART, /*!< Restart has been requested. Typically you hangup your call resources here */
	FTDM_SIGEVENT_SIGSTATUS_CHANGED, /*!< Signaling protocol status changed (ie: D-chan up), see new status in raw_data ftdm_sigmsg_t member */
	FTDM_SIGEVENT_COLLISION, /*!< Outgoing call was dropped because an incoming call arrived at the same time */
	FTDM_SIGEVENT_FACILITY, /*!< In call facility event */
	FTDM_SIGEVENT_TRACE, /*!<Interpreted trace event */
	FTDM_SIGEVENT_TRACE_RAW, /*!<Raw trace event */
	FTDM_SIGEVENT_INVALID, /*!<Invalid */
} ftdm_signal_event_t;
#define SIGNAL_STRINGS "START", "STOP", "RELEASED", "UP", "FLASH", "PROCEED", "RINGING", "PROGRESS", \
		"PROGRESS_MEDIA", "ALARM_TRAP", "ALARM_CLEAR", \
		"COLLECTED_DIGIT", "ADD_CALL", "RESTART", "SIGSTATUS_CHANGED", "COLLISION", "FACILITY", "TRACE", "TRACE_RAW", "INVALID"

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
	FTDM_TRUNK_NONE
} ftdm_trunk_type_t;
#define TRUNK_STRINGS "E1", "T1", "J1", "BRI", "BRI_PTMP", "FXO", "FXS", "EM", "NONE"

/*! \brief Move from string to ftdm_trunk_type_t and viceversa */
FTDM_STR2ENUM_P(ftdm_str2ftdm_trunk_type, ftdm_trunk_type2str, ftdm_trunk_type_t)

/*! \brief Basic channel configuration provided to ftdm_configure_span_channels */
typedef struct ftdm_channel_config {    
	char name[FTDM_MAX_NAME_STR_SZ];
	char number[FTDM_MAX_NUMBER_STR_SZ];
	char group_name[FTDM_MAX_NAME_STR_SZ];
	ftdm_chan_type_t type;
	float rxgain;
	float txgain;
	uint8_t debugdtmf;
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
	FTDM_TRACE_INCOMING,
	/* This is a transmitted frame */
	FTDM_TRACE_OUTGOING,
	/* Invalid */
 	FTDM_TRACE_INVALID,
} ftdm_trace_dir_t;
#define TRACE_DIR_STRINGS "INCOMING", "OUTGOING", "INVALID"

/*! \brief Move string to ftdm_trace_dir_t and viceversa */
FTDM_STR2ENUM_P(ftdm_str2ftdm_trace_dir, ftdm_trace_dir2str, ftdm_trace_dir_t)

typedef struct {
	/* Direction - incoming or outgoing */
	ftdm_trace_dir_t dir;
	uint8_t level; /* 1 for phy layer, 2 for q921/mtp2, 3 for q931/mtp3 */
} ftdm_event_trace_t;

/*! \brief Generic signaling message */
struct ftdm_sigmsg {
	ftdm_signal_event_t event_id; /*!< The type of message */
	ftdm_channel_t *channel; /*!< Related channel */
	uint32_t chan_id; /*!< easy access to chan id */
	uint32_t span_id; /*!< easy access to span_id */
	void *raw_data; /*!< Message specific data if any */
	uint32_t raw_data_len; /*!< Data len in case is needed */
	uint32_t call_id; /*!< unique call id for this call */
	union {
		ftdm_event_sigstatus_t sigstatus; /*!< valid if event_id is FTDM_SIGEVENT_SIGSTATUS_CHANGED */
		ftdm_event_trace_t logevent;	/*!< valid if event_id is FTDM_SIGEVENT_TRACE or FTDM_SIGEVENT_TRACE_RAW */
	} ev_data;
};

/*! \brief Crash policy 
 *  Useful for debugging only, default policy is never, if you wish to crash on asserts then use ftdm_global_set_crash_policy */
typedef enum {
	FTDM_CRASH_NEVER = 0,
	FTDM_CRASH_ON_ASSERT
} ftdm_crash_policy_t;

/*! \brief I/O waiting flags */
typedef enum {
	FTDM_NO_FLAGS = 0,
	FTDM_READ =  (1 << 0),
	FTDM_WRITE = (1 << 1),
	FTDM_EVENTS = (1 << 2)
} ftdm_wait_flag_t;

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
	FTDM_COMMAND_NOOP,
	FTDM_COMMAND_SET_INTERVAL,
	FTDM_COMMAND_GET_INTERVAL,
	FTDM_COMMAND_SET_CODEC,
	FTDM_COMMAND_GET_CODEC,
	FTDM_COMMAND_SET_NATIVE_CODEC,
	FTDM_COMMAND_GET_NATIVE_CODEC,
	FTDM_COMMAND_ENABLE_DTMF_DETECT,
	FTDM_COMMAND_DISABLE_DTMF_DETECT,
	FTDM_COMMAND_SEND_DTMF,
	FTDM_COMMAND_SET_DTMF_ON_PERIOD,
	FTDM_COMMAND_GET_DTMF_ON_PERIOD,
	FTDM_COMMAND_SET_DTMF_OFF_PERIOD,
	FTDM_COMMAND_GET_DTMF_OFF_PERIOD,
	FTDM_COMMAND_GENERATE_RING_ON,
	FTDM_COMMAND_GENERATE_RING_OFF,
	FTDM_COMMAND_OFFHOOK,
	FTDM_COMMAND_ONHOOK,
	FTDM_COMMAND_FLASH,
	FTDM_COMMAND_WINK,
	FTDM_COMMAND_ENABLE_PROGRESS_DETECT,
	FTDM_COMMAND_DISABLE_PROGRESS_DETECT,

	/*!< Start tracing input and output from channel to the given file */
	FTDM_COMMAND_TRACE_INPUT,
	FTDM_COMMAND_TRACE_OUTPUT,

	/*!< Stop both Input and Output trace, closing the files */
	FTDM_COMMAND_TRACE_END_ALL,

	/*!< Enable DTMF debugging */
	FTDM_COMMAND_ENABLE_DEBUG_DTMF,

	/*!< Disable DTMF debugging (if not disabled explicitly, it is disabled automatically when calls hangup) */
	FTDM_COMMAND_DISABLE_DEBUG_DTMF,

	/*!< Start dumping all input to a circular buffer. The size of the circular buffer can be specified, default used otherwise */
	FTDM_COMMAND_ENABLE_INPUT_DUMP,

	/*!< Stop dumping all input to a circular buffer. */
	FTDM_COMMAND_DISABLE_INPUT_DUMP,

	/*!< Start dumping all output to a circular buffer. The size of the circular buffer can be specified, default used otherwise */
	FTDM_COMMAND_ENABLE_OUTPUT_DUMP,

	/*!< Stop dumping all output to a circular buffer. */
	FTDM_COMMAND_DISABLE_OUTPUT_DUMP,

	/*!< Dump the current input circular buffer to the specified FILE* structure */
	FTDM_COMMAND_DUMP_INPUT,

	/*!< Dump the current output circular buffer to the specified FILE* structure */
	FTDM_COMMAND_DUMP_OUTPUT,

	FTDM_COMMAND_ENABLE_CALLERID_DETECT,
	FTDM_COMMAND_DISABLE_CALLERID_DETECT,
	FTDM_COMMAND_ENABLE_ECHOCANCEL,
	FTDM_COMMAND_DISABLE_ECHOCANCEL,
	FTDM_COMMAND_ENABLE_ECHOTRAIN,
	FTDM_COMMAND_DISABLE_ECHOTRAIN,
	FTDM_COMMAND_SET_CAS_BITS,
	FTDM_COMMAND_GET_CAS_BITS,
	FTDM_COMMAND_SET_RX_GAIN,
	FTDM_COMMAND_GET_RX_GAIN,
	FTDM_COMMAND_SET_TX_GAIN,
	FTDM_COMMAND_GET_TX_GAIN,
	FTDM_COMMAND_FLUSH_TX_BUFFERS,
	FTDM_COMMAND_FLUSH_RX_BUFFERS,
	FTDM_COMMAND_FLUSH_BUFFERS,
	FTDM_COMMAND_FLUSH_IOSTATS,
	FTDM_COMMAND_SET_PRE_BUFFER_SIZE,
	FTDM_COMMAND_SET_LINK_STATUS,
	FTDM_COMMAND_GET_LINK_STATUS,
	FTDM_COMMAND_ENABLE_LOOP,
	FTDM_COMMAND_DISABLE_LOOP,
	FTDM_COMMAND_COUNT,
	FTDM_COMMAND_SET_RX_QUEUE_SIZE,
	FTDM_COMMAND_SET_TX_QUEUE_SIZE,
} ftdm_command_t;

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
#define FIO_CHANNEL_SEND_MSG_ARGS (ftdm_channel_t *ftdmchan, ftdm_sigmsg_t *sigmsg)
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

/*! \brief FreeTDM I/O layer interface function typedefs
 * You don't need these unless your implementing an I/O interface module (most users don't) */
typedef ftdm_status_t (*fio_channel_request_t) FIO_CHANNEL_REQUEST_ARGS ;
typedef ftdm_status_t (*fio_channel_outgoing_call_t) FIO_CHANNEL_OUTGOING_CALL_ARGS ;
typedef ftdm_status_t (*fio_channel_send_msg_t) FIO_CHANNEL_SEND_MSG_ARGS;
typedef ftdm_status_t (*fio_channel_set_sig_status_t) FIO_CHANNEL_SET_SIG_STATUS_ARGS;
typedef ftdm_status_t (*fio_channel_get_sig_status_t) FIO_CHANNEL_GET_SIG_STATUS_ARGS;
typedef ftdm_status_t (*fio_span_set_sig_status_t) FIO_SPAN_SET_SIG_STATUS_ARGS;
typedef ftdm_status_t (*fio_span_get_sig_status_t) FIO_SPAN_GET_SIG_STATUS_ARGS;
typedef ftdm_status_t (*fio_span_poll_event_t) FIO_SPAN_POLL_EVENT_ARGS ;
typedef ftdm_status_t (*fio_span_next_event_t) FIO_SPAN_NEXT_EVENT_ARGS ;
typedef ftdm_status_t (*fio_channel_next_event_t) FIO_CHANNEL_NEXT_EVENT_ARGS ;
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


/*! \brief FreeTDM I/O layer interface function prototype wrapper macros
 * You don't need these unless your implementing an I/O interface module (most users don't) */
#define FIO_CHANNEL_REQUEST_FUNCTION(name) ftdm_status_t name FIO_CHANNEL_REQUEST_ARGS
#define FIO_CHANNEL_OUTGOING_CALL_FUNCTION(name) ftdm_status_t name FIO_CHANNEL_OUTGOING_CALL_ARGS
#define FIO_CHANNEL_SEND_MSG_FUNCTION(name) ftdm_status_t name FIO_CHANNEL_SEND_MSG_ARGS
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
};

/*! \brief FreeTDM supported I/O codecs */
typedef enum {
	FTDM_CODEC_ULAW = 0,
	FTDM_CODEC_ALAW = 8,
	FTDM_CODEC_SLIN = 10,
	FTDM_CODEC_NONE = (1 << 30)
} ftdm_codec_t;

/*! \brief FreeTDM supported indications.
 * This is used during incoming calls when you want to request the signaling stack
 * to notify about indications occurring locally */
typedef enum {
	FTDM_CHANNEL_INDICATE_RINGING,
	FTDM_CHANNEL_INDICATE_PROCEED,
	FTDM_CHANNEL_INDICATE_PROGRESS,
	FTDM_CHANNEL_INDICATE_PROGRESS_MEDIA,
	FTDM_CHANNEL_INDICATE_BUSY,
} ftdm_channel_indication_t;

/*! \brief FreeTDM supported hardware alarms. */
typedef enum {
	FTDM_ALARM_NONE    = 0,
	FTDM_ALARM_RED     = (1 << 1),
	FTDM_ALARM_YELLOW  = (1 << 2),
	FTDM_ALARM_RAI     = (1 << 3),
	FTDM_ALARM_BLUE    = (1 << 4),
	FTDM_ALARM_AIS     = (1 << 5),
	FTDM_ALARM_GENERAL = (1 << 30)
} ftdm_alarm_flag_t;

/*! \brief Override the default queue handler */
FT_DECLARE(ftdm_status_t) ftdm_global_set_queue_handler(ftdm_queue_handler_t *handler);

/*! \brief Return the availability rate for a channel 
 * \param ftdmchan Channel to get the availability from
 *
 * \retval > 0 if availability is supported
 * \retval -1 if availability is not supported
 */
FT_DECLARE(int) ftdm_channel_get_availability(ftdm_channel_t *ftdmchan);

/*! \brief Answer call */
#define ftdm_channel_call_answer(ftdmchan) _ftdm_channel_call_answer(__FILE__, __FUNCTION__, __LINE__, (ftdmchan))

/*! \brief Answer call recording the source code point where the it was called (see ftdm_channel_call_answer for an easy to use macro) */
FT_DECLARE(ftdm_status_t) _ftdm_channel_call_answer(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan);

/*! \brief Place an outgoing call */
#define ftdm_channel_call_place(ftdmchan) _ftdm_channel_call_place(__FILE__, __FUNCTION__, __LINE__, (ftdmchan))

/*! \brief Place an outgoing call recording the source code point where it was called (see ftdm_channel_call_place for an easy to use macro) */
FT_DECLARE(ftdm_status_t) _ftdm_channel_call_place(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan);

/*! \brief Indicate a new condition in an incoming call */
#define ftdm_channel_call_indicate(ftdmchan, indication) _ftdm_channel_call_indicate(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), (indication))

/*! \brief Indicate a new condition in an incoming call recording the source code point where it was called (see ftdm_channel_call_indicate for an easy to use macro) */
FT_DECLARE(ftdm_status_t) _ftdm_channel_call_indicate(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_channel_indication_t indication);

/*! \brief Send a message on a call */
#define ftdm_channel_call_send_msg(ftdmchan, sigmsg) _ftdm_channel_call_send_msg(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), (sigmsg))

/*! \brief Send a signal on a call recording the source code point where it was called (see ftdm_channel_call_send_msg for an easy to use macro) */
FT_DECLARE(ftdm_status_t) _ftdm_channel_call_send_msg(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_sigmsg_t *sigmsg);

/*! \brief Hangup the call without cause */
#define ftdm_channel_call_hangup(ftdmchan) _ftdm_channel_call_hangup(__FILE__, __FUNCTION__, __LINE__, (ftdmchan))

/*! \brief Hangup the call without cause recording the source code point where it was called (see ftdm_channel_call_hangup for an easy to use macro)*/
FT_DECLARE(ftdm_status_t) _ftdm_channel_call_hangup(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan);

/*! \brief Hangup the call with cause */
#define ftdm_channel_call_hangup_with_cause(ftdmchan, cause) _ftdm_channel_call_hangup_with_cause(__FILE__, __FUNCTION__, __LINE__, (ftdmchan), (cause))

/*! \brief Hangup the call with cause recording the source code point where it was called (see ftdm_channel_call_hangup_with_cause for an easy to use macro) */
FT_DECLARE(ftdm_status_t) _ftdm_channel_call_hangup_with_cause(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan, ftdm_call_cause_t);

/*! \brief Reset the channel */
#define ftdm_channel_reset(ftdmchan) _ftdm_channel_reset(__FILE__, __FUNCTION__, __LINE__, (ftdmchan))

/*! \brief Reset the channel (see _ftdm_channel_reset for an easy to use macro) 
 *  \note if there was a call on this channel, call will be cleared without any notifications to the user
 */
FT_DECLARE(ftdm_status_t) _ftdm_channel_reset(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan);

/*! \brief Put a call on hold (if supported by the signaling stack) */
#define ftdm_channel_call_hold(ftdmchan) _ftdm_channel_call_hold(__FILE__, __FUNCTION__, __LINE__, (ftdmchan))

/*! \brief Put a call on hold recording the source code point where it was called (see ftdm_channel_call_hold for an easy to use macro) */
FT_DECLARE(ftdm_status_t) _ftdm_channel_call_hold(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan);

/*! \brief Unhold a call */
#define ftdm_channel_call_unhold(ftdmchan) _ftdm_channel_call_unhold(__FILE__, __FUNCTION__, __LINE__, (ftdmchan))

/*! \brief Unhold a call recording the source code point where it was called (see ftdm_channel_call_unhold for an easy to use macro) */
FT_DECLARE(ftdm_status_t) _ftdm_channel_call_unhold(const char *file, const char *func, int line, ftdm_channel_t *ftdmchan);

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
 * \note You must call ftdm_channel_close() or ftdm_channel_call_hangup() to release the channel afterwards
 * 	Only use ftdm_channel_close if there is no call (incoming or outgoing) in the channel
 *
 * \param span_id The span id the channel belongs to
 * \param chan_id Channel id of the channel you want to open
 * \param ftdmchan Pointer to store the channel once is open
 *
 * \retval FTDM_SUCCESS success (the channel was found and is available)
 * \retval FTDM_FAIL failure (channel was not found or not available)
 */
FT_DECLARE(ftdm_status_t) ftdm_channel_open(uint32_t span_id, uint32_t chan_id, ftdm_channel_t **ftdmchan);

/*! 
 * \brief Hunts and opens a channel specifying the span id only
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
 * \note If you call ftdm_channel_call_hangup() you MUST NOT call this function, the signaling
 *       stack will close the channel.
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

/*! \brief Add a custom variable to the channel
 *  \note This variables may be used by signaling modules to override signaling parameters
 *  \todo Document which signaling variables are available
 * */
FT_DECLARE(ftdm_status_t) ftdm_channel_add_var(ftdm_channel_t *ftdmchan, const char *var_name, const char *value);

/*! \brief Get a custom variable from the channel. 
 *  \note The variable pointer returned is only valid while the channel is open and it'll be destroyed when the channel is closed. */
FT_DECLARE(const char *) ftdm_channel_get_var(ftdm_channel_t *ftdmchan, const char *var_name);

/*! \brief Get an iterator to iterate over the channel variables
 *  \param ftdmchan The channel structure containing the variables
 *  \param iter Optional iterator. You can reuse an old iterator (not previously freed) to avoid the extra allocation of a new iterator.
 *  \note The iterator pointer returned is only valid while the channel is open and it'll be destroyed when the channel is closed. 
 *        This iterator is completely non-thread safe, if you are adding variables or removing variables while iterating 
 *        results are unpredictable
 */
FT_DECLARE(ftdm_iterator_t *) ftdm_channel_get_var_iterator(const ftdm_channel_t *ftdmchan, ftdm_iterator_t *iter);

/*! \brief Get iterator current value (depends on the iterator type)
 *  \note Channel iterators return a pointer to ftdm_channel_t
 *        Variable iterators return a pointer to the variable name (not the variable value)
 */
FT_DECLARE(void *) ftdm_iterator_current(ftdm_iterator_t *iter);

/*! \brief Get variable name and value for the current iterator position */
FT_DECLARE(ftdm_status_t) ftdm_channel_get_current_var(ftdm_iterator_t *iter, const char **var_name, const char **var_val);

/*! \brief Advance iterator */
FT_DECLARE(ftdm_iterator_t *) ftdm_iterator_next(ftdm_iterator_t *iter);

/*! \brief Free iterator 
 *  \note You must free an iterator after using it unless you plan to reuse it
 */
FT_DECLARE(ftdm_status_t) ftdm_iterator_free(ftdm_iterator_t *iter);

/*! \brief Add a custom variable to the call
 *  \note This variables may be used by signaling modules to override signaling parameters
 *  \todo Document which signaling variables are available
 * */
FT_DECLARE(ftdm_status_t) ftdm_call_add_var(ftdm_caller_data_t *caller_data, const char *var_name, const char *value);

/*! \brief Get a custom variable from the call.
 *  \note The variable pointer returned is only valid during the callback receiving SIGEVENT. */
FT_DECLARE(const char *) ftdm_call_get_var(ftdm_caller_data_t *caller_data, const char *var_name);

/*! \brief Get an iterator to iterate over the channel variables
 *  \param caller_data The signal msg structure containing the variables
 *  \param iter Optional iterator. You can reuse an old iterator (not previously freed) to avoid the extra allocation of a new iterator.
 *  \note The iterator pointer returned is only valid while the signal message and it'll be destroyed when the signal message is processed.
 *        This iterator is completely non-thread safe, if you are adding variables or removing variables while iterating
 *        results are unpredictable
 */
FT_DECLARE(ftdm_iterator_t *) ftdm_call_get_var_iterator(const ftdm_caller_data_t *caller_data, ftdm_iterator_t *iter);

/*! \brief Get variable name and value for the current iterator position */
FT_DECLARE(ftdm_status_t) ftdm_call_get_current_var(ftdm_iterator_t *iter, const char **var_name, const char **var_val);

/*! \brief Clear all variables  attached to the call
 *  \note Variables are cleared at the end of each call back, so it is not necessary for the user to call this function.
 *  \todo Document which signaling variables are available
 * */
FT_DECLARE(ftdm_status_t) ftdm_call_clear_vars(ftdm_caller_data_t *caller_data);

/*! \brief Remove a variable attached to the call
 *  \note Removes a variable that was attached to the call.
 *  \todo Document which call variables are available
 * */
FT_DECLARE(ftdm_status_t) ftdm_call_remove_var(ftdm_caller_data_t *caller_data, const char *var_name);

/*! \brief Clears all the temporary data attached to this call
 *  \note Clears caller_data->variables and caller_data->raw_data.
 * */
FT_DECLARE(void) ftdm_call_clear_data(ftdm_caller_data_t *caller_data);
		
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
 * \brief Configure span with a signaling type (deprecated use ftdm_configure_span_signaling instead)
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
 * \brief Return the channel identified by the provided id
 *
 * \param span The span where the channel belongs
 * \param chanid The channel id within the span
 *
 * \return The channel pointer if found, NULL otherwise
 */
FT_DECLARE(ftdm_channel_t *) ftdm_span_get_channel(const ftdm_span_t *span, uint32_t chanid);

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

/*! \brief Initialize channel state for an outgoing call */
FT_DECLARE(ftdm_status_t) ftdm_channel_init(ftdm_channel_t *ftdmchan);

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
