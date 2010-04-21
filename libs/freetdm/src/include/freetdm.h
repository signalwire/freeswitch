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

#define FTDM_MAX_CHANNELS_PHYSICAL_SPAN 32
#define FTDM_MAX_PHYSICAL_SPANS_PER_LOGICAL_SPAN 32
#define FTDM_MAX_CHANNELS_SPAN FTDM_MAX_CHANNELS_PHYSICAL_SPAN * FTDM_MAX_PHYSICAL_SPANS_PER_LOGICAL_SPAN
#define FTDM_MAX_SPANS_INTERFACE 128

#define FTDM_MAX_CHANNELS_GROUP 1024
#define FTDM_MAX_GROUPS_INTERFACE FTDM_MAX_SPANS_INTERFACE


typedef enum {
	FTDM_SUCCESS,
	FTDM_FAIL,
	FTDM_MEMERR,
	FTDM_TIMEOUT,
	FTDM_NOTIMPL,
	FTDM_CHECKSUM_ERROR,
	FTDM_STATUS_COUNT,
	FTDM_BREAK
} ftdm_status_t;

/* Thread/Mutex OS abstraction */
#include "ftdm_os.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FTDM_MAX_NAME_STR_SZ 80
#define FTDM_MAX_NUMBER_STR_SZ 20

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

typedef enum {
	FTDM_TOP_DOWN,
	FTDM_BOTTOM_UP
} ftdm_direction_t;

typedef enum {
	FTDM_EVENT_NONE,
	FTDM_EVENT_DTMF,
	FTDM_EVENT_OOB,
	FTDM_EVENT_COUNT
} ftdm_event_type_t;

typedef enum {
	FTDM_STATE_CHANGE_FAIL,
	FTDM_STATE_CHANGE_SUCCESS,
	FTDM_STATE_CHANGE_SAME,
} ftdm_state_change_result_t;

struct ftdm_event {
	ftdm_event_type_t e_type;
	uint32_t enum_id;
	ftdm_channel_t *channel;
	void *data;
};

typedef enum {
	FTDM_CHAN_TYPE_B,
	FTDM_CHAN_TYPE_DQ921,
	FTDM_CHAN_TYPE_DQ931,
	FTDM_CHAN_TYPE_FXS,
	FTDM_CHAN_TYPE_FXO,
	FTDM_CHAN_TYPE_EM,
	FTDM_CHAN_TYPE_CAS,
	FTDM_CHAN_TYPE_COUNT
} ftdm_chan_type_t;
#define CHAN_TYPE_STRINGS "B", "DQ921", "DQ931", "FXS", "FXO", "EM", "CAS", "INVALID"
FTDM_STR2ENUM_P(ftdm_str2ftdm_chan_type, ftdm_chan_type2str, ftdm_chan_type_t)

#define FTDM_IS_VOICE_CHANNEL(ftdm_chan) ((ftdm_chan)->type != FTDM_CHAN_TYPE_DQ921 && (ftdm_chan)->type != FTDM_CHAN_TYPE_DQ931)
#define FTDM_IS_DCHAN(ftdm_chan) ((ftdm_chan)->type == FTDM_CHAN_TYPE_DQ921 || (ftdm_chan)->type == FTDM_CHAN_TYPE_DQ931)

typedef void (*ftdm_logger_t)(const char *file, const char *func, int line, int level, const char *fmt, ...);

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

/**
 * Type Of Number (TON)
 */
typedef enum {
	FTDM_TON_UNKNOWN = 0,
	FTDM_TON_INTERNATIONAL,
	FTDM_TON_NATIONAL,
	FTDM_TON_NETWORK_SPECIFIC,
	FTDM_TON_SUBSCRIBER_NUMBER,
	FTDM_TON_ABBREVIATED_NUMBER,
	FTDM_TON_RESERVED,
	FTDM_TON_INVALID = 255
} ftdm_ton_t;

/**
 * Numbering Plan Identification (NPI)
 */
typedef enum {
	FTDM_NPI_UNKNOWN = 0,
	FTDM_NPI_ISDN = 1,
	FTDM_NPI_DATA = 3,
	FTDM_NPI_TELEX = 4,
	FTDM_NPI_NATIONAL = 8,
	FTDM_NPI_PRIVATE = 9,
	FTDM_NPI_RESERVED = 10,
	FTDM_NPI_INVALID = 255
} ftdm_npi_t;

typedef struct {
	char digits[25];
	uint8_t type;
	uint8_t plan;
} ftdm_number_t;

typedef enum {
	FTDM_CALLER_STATE_DIALING,
	FTDM_CALLER_STATE_SUCCESS,
	FTDM_CALLER_STATE_FAIL
} ftdm_caller_state_t;

typedef struct ftdm_caller_data {
	char cid_date[8];
	char cid_name[80];
	ftdm_number_t cid_num;
	ftdm_number_t ani;
	ftdm_number_t dnis;
	ftdm_number_t rdnis;
	char aniII[25];
	uint8_t screen;
	uint8_t pres;
	char collected[25];
	int CRV;
	int hangup_cause;	
	uint8_t raw_data[1024];
	uint32_t raw_data_len;
	uint32_t flags;
	ftdm_caller_state_t call_state;
	uint32_t chan_id;
} ftdm_caller_data_t;

typedef enum {
	FTDM_TONE_DTMF = (1 << 0)
} ftdm_tone_type_t;

typedef enum {
	FTDM_SIGEVENT_START,
	FTDM_SIGEVENT_STOP,
	FTDM_SIGEVENT_TRANSFER,
	FTDM_SIGEVENT_ANSWER,
	FTDM_SIGEVENT_UP,
	FTDM_SIGEVENT_FLASH,
	FTDM_SIGEVENT_PROGRESS,
	FTDM_SIGEVENT_PROGRESS_MEDIA,
	FTDM_SIGEVENT_NOTIFY,
	FTDM_SIGEVENT_TONE_DETECTED,
	FTDM_SIGEVENT_ALARM_TRAP,
	FTDM_SIGEVENT_ALARM_CLEAR,
	FTDM_SIGEVENT_MISC,
	FTDM_SIGEVENT_COLLECTED_DIGIT,
	FTDM_SIGEVENT_ADD_CALL,
	FTDM_SIGEVENT_RESTART,
	/* Signaling status changed (D-chan up, down, R2 blocked etc) */
	FTDM_SIGEVENT_SIGSTATUS_CHANGED,
	FTDM_SIGEVENT_INVALID
} ftdm_signal_event_t;
#define SIGNAL_STRINGS "START", "STOP", "TRANSFER", "ANSWER", "UP", "FLASH", "PROGRESS", \
		"PROGRESS_MEDIA", "NOTIFY", "TONE_DETECTED", "ALARM_TRAP", "ALARM_CLEAR", "MISC", \
		"COLLECTED_DIGIT", "ADD_CALL", "RESTART", "SIGLINK_CHANGED", "INVALID"
FTDM_STR2ENUM_P(ftdm_str2ftdm_signal_event, ftdm_signal_event2str, ftdm_signal_event_t)

struct ftdm_sigmsg {
	ftdm_signal_event_t event_id;
	uint32_t chan_id;
	uint32_t span_id;
	ftdm_channel_t *channel;
	void *raw_data;
	uint32_t raw_data_len;
};

typedef enum {
	FTDM_CRASH_NEVER = 0,
	FTDM_CRASH_ON_ASSERT
} ftdm_crash_policy_t;

/*!
  \brief Signaling status on a given span or specific channel on protocols that support it
 */
typedef enum {
	/* The signaling link is down (no d-chans up in the span/group, MFC-R2 bit pattern unidentified) */
	FTDM_SIG_STATE_DOWN,
	/* The signaling link is suspended (MFC-R2 bit pattern blocked, ss7 blocked?) */
	FTDM_SIG_STATE_SUSPENDED,
	/* The signaling link is ready and calls can be placed */
	FTDM_SIG_STATE_UP,
	/* Invalid status */
	FTDM_SIG_STATE_INVALID
} ftdm_signaling_status_t;
#define SIGSTATUS_STRINGS "DOWN", "SUSPENDED", "UP", "INVALID"
FTDM_STR2ENUM_P(ftdm_str2ftdm_signaling_status, ftdm_signaling_status2str, ftdm_signaling_status_t)

typedef enum {
	FTDM_NO_FLAGS = 0,
	FTDM_READ =  (1 << 0),
	FTDM_WRITE = (1 << 1),
	FTDM_EVENTS = (1 << 2)
} ftdm_wait_flag_t;

typedef struct ftdm_conf_parameter {
	const char *var;
	const char *val;
} ftdm_conf_parameter_t;

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
	FTDM_COMMAND_TRACE_INPUT,
	FTDM_COMMAND_TRACE_OUTPUT,
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
	FTDM_COMMAND_SET_PRE_BUFFER_SIZE,
	FTDM_COMMAND_SET_LINK_STATUS,
	FTDM_COMMAND_GET_LINK_STATUS,
	FTDM_COMMAND_ENABLE_LOOP,
	FTDM_COMMAND_DISABLE_LOOP,
	FTDM_COMMAND_COUNT
} ftdm_command_t;



typedef void *(*ftdm_malloc_func_t)(void *pool, ftdm_size_t len);
typedef void *(*ftdm_calloc_func_t)(void *pool, ftdm_size_t elements, ftdm_size_t len);
typedef void *(*ftdm_realloc_func_t)(void *pool, void *buff, ftdm_size_t len);
typedef void (*ftdm_free_func_t)(void *pool, void *ptr);
typedef struct ftdm_memory_handler {
	void *pool;
	ftdm_malloc_func_t malloc;
	ftdm_calloc_func_t calloc;
	ftdm_realloc_func_t realloc;
	ftdm_free_func_t free;
} ftdm_memory_handler_t;

#define FIO_CHANNEL_REQUEST_ARGS (ftdm_span_t *span, uint32_t chan_id, ftdm_direction_t direction, ftdm_caller_data_t *caller_data, ftdm_channel_t **ftdmchan)
#define FIO_CHANNEL_OUTGOING_CALL_ARGS (ftdm_channel_t *ftdmchan)
#define FIO_CHANNEL_SET_SIG_STATUS_ARGS (ftdm_channel_t *ftdmchan, ftdm_signaling_status_t status)
#define FIO_CHANNEL_GET_SIG_STATUS_ARGS (ftdm_channel_t *ftdmchan, ftdm_signaling_status_t *status)
#define FIO_SPAN_SET_SIG_STATUS_ARGS (ftdm_span_t *span, ftdm_signaling_status_t status)
#define FIO_SPAN_GET_SIG_STATUS_ARGS (ftdm_span_t *span, ftdm_signaling_status_t *status)
#define FIO_SPAN_POLL_EVENT_ARGS (ftdm_span_t *span, uint32_t ms)
#define FIO_SPAN_NEXT_EVENT_ARGS (ftdm_span_t *span, ftdm_event_t **event)
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

typedef ftdm_status_t (*fio_channel_request_t) FIO_CHANNEL_REQUEST_ARGS ;
typedef ftdm_status_t (*fio_channel_outgoing_call_t) FIO_CHANNEL_OUTGOING_CALL_ARGS ;
typedef ftdm_status_t (*fio_channel_set_sig_status_t) FIO_CHANNEL_SET_SIG_STATUS_ARGS;
typedef ftdm_status_t (*fio_channel_get_sig_status_t) FIO_CHANNEL_GET_SIG_STATUS_ARGS;
typedef ftdm_status_t (*fio_span_set_sig_status_t) FIO_SPAN_SET_SIG_STATUS_ARGS;
typedef ftdm_status_t (*fio_span_get_sig_status_t) FIO_SPAN_GET_SIG_STATUS_ARGS;
typedef ftdm_status_t (*fio_span_poll_event_t) FIO_SPAN_POLL_EVENT_ARGS ;
typedef ftdm_status_t (*fio_span_next_event_t) FIO_SPAN_NEXT_EVENT_ARGS ;
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


#define FIO_CHANNEL_REQUEST_FUNCTION(name) ftdm_status_t name FIO_CHANNEL_REQUEST_ARGS
#define FIO_CHANNEL_OUTGOING_CALL_FUNCTION(name) ftdm_status_t name FIO_CHANNEL_OUTGOING_CALL_ARGS
#define FIO_CHANNEL_SET_SIG_STATUS_FUNCTION(name) ftdm_status_t name FIO_CHANNEL_SET_SIG_STATUS_ARGS
#define FIO_CHANNEL_GET_SIG_STATUS_FUNCTION(name) ftdm_status_t name FIO_CHANNEL_GET_SIG_STATUS_ARGS
#define FIO_SPAN_SET_SIG_STATUS_FUNCTION(name) ftdm_status_t name FIO_SPAN_SET_SIG_STATUS_ARGS
#define FIO_SPAN_GET_SIG_STATUS_FUNCTION(name) ftdm_status_t name FIO_SPAN_GET_SIG_STATUS_ARGS
#define FIO_SPAN_POLL_EVENT_FUNCTION(name) ftdm_status_t name FIO_SPAN_POLL_EVENT_ARGS
#define FIO_SPAN_NEXT_EVENT_FUNCTION(name) ftdm_status_t name FIO_SPAN_NEXT_EVENT_ARGS
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

struct ftdm_io_interface {
	const char *name;
	fio_configure_span_t configure_span;
	fio_configure_t configure;
	fio_open_t open;
	fio_close_t close;
	fio_channel_destroy_t channel_destroy;
	fio_span_destroy_t span_destroy;
	fio_get_alarms_t get_alarms;
	fio_command_t command;
	fio_wait_t wait;
	fio_read_t read;
	fio_write_t write;
	fio_span_poll_event_t poll_event;
	fio_span_next_event_t next_event;
	fio_api_t api;
};

typedef enum {
	FTDM_CODEC_ULAW = 0,
	FTDM_CODEC_ALAW = 8,
	FTDM_CODEC_SLIN = 10,
	FTDM_CODEC_NONE = (1 << 30)
} ftdm_codec_t;

typedef enum {
	FTDM_CHANNEL_INDICATE_RING,
	FTDM_CHANNEL_INDICATE_PROCEED,
	FTDM_CHANNEL_INDICATE_PROGRESS,
	FTDM_CHANNEL_INDICATE_PROGRESS_MEDIA,
	FTDM_CHANNEL_INDICATE_BUSY,
} ftdm_channel_indication_t;

typedef enum {
	FTDM_FALSE,
	FTDM_TRUE
} ftdm_bool_t;

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

FT_DECLARE(ftdm_status_t) ftdm_channel_call_answer(ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_channel_call_place(ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_channel_call_indicate(ftdm_channel_t *ftdmchan, ftdm_channel_indication_t indication);
FT_DECLARE(ftdm_status_t) ftdm_channel_call_hangup(ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_channel_call_hangup_with_cause(ftdm_channel_t *ftdmchan, ftdm_call_cause_t);
FT_DECLARE(ftdm_status_t) ftdm_channel_call_hold(ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_channel_call_unhold(ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_bool_t) ftdm_channel_call_check_answered(const ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_bool_t) ftdm_channel_call_check_busy(const ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_bool_t) ftdm_channel_call_check_hangup(const ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_bool_t) ftdm_channel_call_check_done(const ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_bool_t) ftdm_channel_call_check_hold(const ftdm_channel_t *ftdmchan);

FT_DECLARE(ftdm_status_t) ftdm_channel_set_sig_status(ftdm_channel_t *ftdmchan, ftdm_signaling_status_t status);
FT_DECLARE(ftdm_status_t) ftdm_channel_get_sig_status(ftdm_channel_t *ftdmchan, ftdm_signaling_status_t *status);
FT_DECLARE(ftdm_status_t) ftdm_span_set_sig_status(ftdm_span_t *span, ftdm_signaling_status_t status);
FT_DECLARE(ftdm_status_t) ftdm_span_get_sig_status(ftdm_span_t *span, ftdm_signaling_status_t *status);

FT_DECLARE(void) ftdm_channel_clear_detected_tones(ftdm_channel_t *ftdmchan);
FT_DECLARE(void) ftdm_channel_clear_needed_tones(ftdm_channel_t *ftdmchan);

FT_DECLARE(void) ftdm_channel_rotate_tokens(ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_channel_clear_token(ftdm_channel_t *ftdmchan, const char *token);
FT_DECLARE(void) ftdm_channel_replace_token(ftdm_channel_t *ftdmchan, const char *old_token, const char *new_token);
FT_DECLARE(ftdm_status_t) ftdm_channel_add_token(ftdm_channel_t *ftdmchan, char *token, int end);
FT_DECLARE(const char *) ftdm_channel_get_token(const ftdm_channel_t *ftdmchan, uint32_t tokenid);
FT_DECLARE(uint32_t) ftdm_channel_get_token_count(const ftdm_channel_t *ftdmchan);

FT_DECLARE(uint32_t) ftdm_channel_get_io_interval(const ftdm_channel_t *ftdmchan);
FT_DECLARE(uint32_t) ftdm_channel_get_io_packet_len(const ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_codec_t) ftdm_channel_get_codec(const ftdm_channel_t *ftdmchan);

FT_DECLARE(const char *) ftdm_channel_get_last_error(const ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_channel_get_alarms(ftdm_channel_t *ftdmchan, ftdm_alarm_flag_t *alarmbits);
FT_DECLARE(ftdm_chan_type_t) ftdm_channel_get_type(const ftdm_channel_t *ftdmchan);

FT_DECLARE(ftdm_size_t) ftdm_channel_dequeue_dtmf(ftdm_channel_t *ftdmchan, char *dtmf, ftdm_size_t len);
FT_DECLARE(ftdm_status_t) ftdm_channel_queue_dtmf(ftdm_channel_t *ftdmchan, const char *dtmf);
FT_DECLARE(void) ftdm_channel_flush_dtmf(ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_span_poll_event(ftdm_span_t *span, uint32_t ms);
FT_DECLARE(ftdm_status_t) ftdm_span_next_event(ftdm_span_t *span, ftdm_event_t **event);
FT_DECLARE(ftdm_status_t) ftdm_span_find(uint32_t id, ftdm_span_t **span);
FT_DECLARE(const char *) ftdm_span_get_last_error(const ftdm_span_t *ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_span_create(ftdm_io_interface_t *fio, ftdm_span_t **span, const char *name);
FT_DECLARE(ftdm_status_t) ftdm_span_close_all(void);
FT_DECLARE(ftdm_status_t) ftdm_span_add_channel(ftdm_span_t *span, ftdm_socket_t sockfd, ftdm_chan_type_t type, ftdm_channel_t **chan);
FT_DECLARE(ftdm_status_t) ftdm_span_set_event_callback(ftdm_span_t *span, fio_event_cb_t event_callback);
FT_DECLARE(ftdm_status_t) ftdm_channel_add_to_group(const char* name, ftdm_channel_t* ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_channel_remove_from_group(ftdm_group_t* group, ftdm_channel_t* ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_group_find(uint32_t id, ftdm_group_t **group);
FT_DECLARE(ftdm_status_t) ftdm_group_find_by_name(const char *name, ftdm_group_t **group);
FT_DECLARE(ftdm_status_t) ftdm_group_create(ftdm_group_t **group, const char *name);
FT_DECLARE(ftdm_status_t) ftdm_channel_set_event_callback(ftdm_channel_t *ftdmchan, fio_event_cb_t event_callback);
FT_DECLARE(ftdm_status_t) ftdm_channel_open(uint32_t span_id, uint32_t chan_id, ftdm_channel_t **ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_channel_open_chan(ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_span_channel_use_count(ftdm_span_t *span, uint32_t *count);
FT_DECLARE(ftdm_status_t) ftdm_group_channel_use_count(ftdm_group_t *group, uint32_t *count);
FT_DECLARE(uint32_t) ftdm_group_get_id(const ftdm_group_t *group);
FT_DECLARE(ftdm_status_t) ftdm_channel_open_by_span(uint32_t span_id, ftdm_direction_t direction, ftdm_caller_data_t *caller_data, ftdm_channel_t **ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_channel_open_by_group(uint32_t group_id, ftdm_direction_t direction, ftdm_caller_data_t *caller_data, ftdm_channel_t **ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_channel_close(ftdm_channel_t **ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_channel_command(ftdm_channel_t *ftdmchan, ftdm_command_t command, void *obj);
FT_DECLARE(ftdm_status_t) ftdm_channel_wait(ftdm_channel_t *ftdmchan, ftdm_wait_flag_t *flags, int32_t to);
FT_DECLARE(ftdm_status_t) ftdm_channel_read(ftdm_channel_t *ftdmchan, void *data, ftdm_size_t *datalen);
FT_DECLARE(ftdm_status_t) ftdm_channel_write(ftdm_channel_t *ftdmchan, void *data, ftdm_size_t datasize, ftdm_size_t *datalen);
FT_DECLARE(ftdm_status_t) ftdm_channel_add_var(ftdm_channel_t *ftdmchan, const char *var_name, const char *value);
FT_DECLARE(const char *) ftdm_channel_get_var(ftdm_channel_t *ftdmchan, const char *var_name);
FT_DECLARE(ftdm_status_t) ftdm_channel_clear_vars(ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_span_t *) ftdm_channel_get_span(const ftdm_channel_t *ftdmchan);
FT_DECLARE(uint32_t) ftdm_channel_get_span_id(const ftdm_channel_t *ftdmchan);
FT_DECLARE(uint32_t) ftdm_channel_get_ph_span_id(const ftdm_channel_t *ftdmchan);
FT_DECLARE(const char *) ftdm_channel_get_span_name(const ftdm_channel_t *ftdmchan);
FT_DECLARE(uint32_t) ftdm_channel_get_id(const ftdm_channel_t *ftdmchan);
FT_DECLARE(const char *) ftdm_channel_get_name(const ftdm_channel_t *ftdmchan);
FT_DECLARE(const char *) ftdm_channel_get_number(const ftdm_channel_t *ftdmchan);
FT_DECLARE(uint32_t) ftdm_channel_get_ph_id(const ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_global_init(void);
FT_DECLARE(ftdm_status_t) ftdm_global_configuration(void);
FT_DECLARE(ftdm_status_t) ftdm_global_destroy(void);
FT_DECLARE(ftdm_status_t) ftdm_global_set_memory_handler(ftdm_memory_handler_t *handler);
FT_DECLARE(void) ftdm_global_set_crash_policy(ftdm_crash_policy_t policy);
FT_DECLARE(void) ftdm_global_set_logger(ftdm_logger_t logger);
FT_DECLARE(void) ftdm_global_set_default_logger(int level);
FT_DECLARE(ftdm_bool_t) ftdm_running(void);
FT_DECLARE(ftdm_status_t) ftdm_configure_span(const char *type, ftdm_span_t *span, fio_signal_cb_t sig_cb, ...);
FT_DECLARE(ftdm_status_t) ftdm_configure_span_signaling(const char *type, ftdm_span_t *span, fio_signal_cb_t sig_cb, ftdm_conf_parameter_t *parameters);
FT_DECLARE(ftdm_status_t) ftdm_span_start(ftdm_span_t *span);
FT_DECLARE(ftdm_status_t) ftdm_span_stop(ftdm_span_t *span);
FT_DECLARE(ftdm_status_t) ftdm_global_add_io_interface(ftdm_io_interface_t *io_interface);
FT_DECLARE(ftdm_status_t) ftdm_span_find_by_name(const char *name, ftdm_span_t **span);
FT_DECLARE(uint32_t) ftdm_span_get_id(const ftdm_span_t *span);
FT_DECLARE(const char *) ftdm_span_get_name(const ftdm_span_t *span);
FT_DECLARE(char *) ftdm_api_execute(const char *type, const char *cmd);
FT_DECLARE(void) ftdm_cpu_monitor_disable(void);
FT_DECLARE(ftdm_status_t) ftdm_conf_node_create(const char *name, ftdm_conf_node_t **node, ftdm_conf_node_t *parent);
FT_DECLARE(ftdm_status_t) ftdm_conf_node_add_param(ftdm_conf_node_t *node, const char *param, const char *val);
FT_DECLARE(ftdm_status_t) ftdm_conf_node_destroy(ftdm_conf_node_t *node);
FT_DECLARE(ftdm_status_t) ftdm_configure_span_channels(ftdm_span_t *span, const char *str, ftdm_channel_config_t *chan_config, unsigned *configured);
FT_DECLARE(ftdm_status_t) ftdm_channel_done(ftdm_channel_t *ftdmchan);

FT_DECLARE(ftdm_channel_t *) ftdm_span_get_channel(const ftdm_span_t *span, uint32_t chanid);
FT_DECLARE(uint32_t) ftdm_span_get_chan_count(const ftdm_span_t *span);

FT_DECLARE(ftdm_status_t) ftdm_channel_set_caller_data(ftdm_channel_t *ftdmchan, ftdm_caller_data_t *caller_data);
FT_DECLARE(ftdm_caller_data_t *) ftdm_channel_get_caller_data(ftdm_channel_t *channel);
FT_DECLARE(const char *) ftdm_channel_get_state_str(const ftdm_channel_t *channel);
FT_DECLARE(const char *) ftdm_channel_get_last_state_str(const ftdm_channel_t *channel);

/* TODO: try to get rid of this API */
FT_DECLARE(ftdm_status_t) ftdm_channel_init(ftdm_channel_t *ftdmchan);

#define FIO_CODEC_ARGS (void *data, ftdm_size_t max, ftdm_size_t *datalen)
#define FIO_CODEC_FUNCTION(name) FT_DECLARE_NONSTD(ftdm_status_t) name FIO_CODEC_ARGS
typedef ftdm_status_t (*fio_codec_t) FIO_CODEC_ARGS ;
FIO_CODEC_FUNCTION(fio_slin2ulaw);
FIO_CODEC_FUNCTION(fio_ulaw2slin);
FIO_CODEC_FUNCTION(fio_slin2alaw);
FIO_CODEC_FUNCTION(fio_alaw2slin);
FIO_CODEC_FUNCTION(fio_ulaw2alaw);
FIO_CODEC_FUNCTION(fio_alaw2ulaw);


FT_DECLARE_DATA extern ftdm_logger_t ftdm_log;

#define FTDM_PRE __FILE__, __FUNCTION__, __LINE__
#define FTDM_LOG_LEVEL_DEBUG 7
#define FTDM_LOG_LEVEL_INFO 6
#define FTDM_LOG_LEVEL_NOTICE 5
#define FTDM_LOG_LEVEL_WARNING 4
#define FTDM_LOG_LEVEL_ERROR 3
#define FTDM_LOG_LEVEL_CRIT 2
#define FTDM_LOG_LEVEL_ALERT 1
#define FTDM_LOG_LEVEL_EMERG 0

#define FTDM_LOG_DEBUG FTDM_PRE, FTDM_LOG_LEVEL_DEBUG
#define FTDM_LOG_INFO FTDM_PRE, FTDM_LOG_LEVEL_INFO
#define FTDM_LOG_NOTICE FTDM_PRE, FTDM_LOG_LEVEL_NOTICE
#define FTDM_LOG_WARNING FTDM_PRE, FTDM_LOG_LEVEL_WARNING
#define FTDM_LOG_ERROR FTDM_PRE, FTDM_LOG_LEVEL_ERROR
#define FTDM_LOG_CRIT FTDM_PRE, FTDM_LOG_LEVEL_CRIT
#define FTDM_LOG_ALERT FTDM_PRE, FTDM_LOG_LEVEL_ALERT
#define FTDM_LOG_EMERG FTDM_PRE, FTDM_LOG_LEVEL_EMERG

#define FTDM_TAG_END NULL

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
