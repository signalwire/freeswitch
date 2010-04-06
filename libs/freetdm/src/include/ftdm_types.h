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

#ifndef FTDM_TYPES_H
#define FTDM_TYPES_H
#include "fsk.h"

#ifdef WIN32
#define FTDM_INVALID_SOCKET INVALID_HANDLE_VALUE
#include <windows.h>
typedef HANDLE ftdm_socket_t;
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8 uint8_t;
typedef __int64 int64_t;
typedef __int32 int32_t;
typedef __int16 int16_t;
typedef __int8 int8_t;
typedef intptr_t ftdm_ssize_t;
typedef int ftdm_filehandle_t;
#else
#define FTDM_INVALID_SOCKET -1
#include <stdint.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdarg.h>
typedef int ftdm_socket_t;
typedef ssize_t ftdm_ssize_t;
typedef int ftdm_filehandle_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif
#define TAG_END NULL

typedef size_t ftdm_size_t;
struct ftdm_io_interface;

#define FTDM_COMMAND_OBJ_INT *((int *)obj)
#define FTDM_COMMAND_OBJ_CHAR_P (char *)obj
#define FTDM_COMMAND_OBJ_FLOAT *(float *)obj
#define FTDM_FSK_MOD_FACTOR 0x10000
#define FTDM_DEFAULT_DTMF_ON 250
#define FTDM_DEFAULT_DTMF_OFF 50

#define FTDM_END -1
#define FTDM_ANY_STATE -1

typedef uint64_t ftdm_time_t; 

typedef enum {
	FTDM_ENDIAN_BIG = 1,
	FTDM_ENDIAN_LITTLE = -1
} ftdm_endian_t;

typedef enum {
	FTDM_CID_TYPE_SDMF = 0x04,
	FTDM_CID_TYPE_MDMF = 0x80
} ftdm_cid_type_t;

typedef enum {
	MDMF_DATETIME = 1,
	MDMF_PHONE_NUM = 2,
	MDMF_DDN = 3,
	MDMF_NO_NUM = 4,
	MDMF_PHONE_NAME = 7,
	MDMF_NO_NAME = 8,
	MDMF_ALT_ROUTE = 9,
	MDMF_INVALID = 10
} ftdm_mdmf_type_t;
#define MDMF_STRINGS "X", "DATETIME", "PHONE_NUM", "DDN", "NO_NUM", "X", "X", "PHONE_NAME", "NO_NAME", "ALT_ROUTE", "INVALID"
FTDM_STR2ENUM_P(ftdm_str2ftdm_mdmf_type, ftdm_mdmf_type2str, ftdm_mdmf_type_t)

#define FTDM_TONEMAP_LEN 128
typedef enum {
	FTDM_TONEMAP_NONE,
	FTDM_TONEMAP_DIAL,
	FTDM_TONEMAP_RING,
	FTDM_TONEMAP_BUSY,
	FTDM_TONEMAP_FAIL1,
	FTDM_TONEMAP_FAIL2,
	FTDM_TONEMAP_FAIL3,
	FTDM_TONEMAP_ATTN,
	FTDM_TONEMAP_CALLWAITING_CAS,
	FTDM_TONEMAP_CALLWAITING_SAS,
	FTDM_TONEMAP_CALLWAITING_ACK,
	FTDM_TONEMAP_INVALID
} ftdm_tonemap_t;
#define TONEMAP_STRINGS "NONE", "DIAL", "RING", "BUSY", "FAIL1", "FAIL2", "FAIL3", "ATTN", "CALLWAITING-CAS", "CALLWAITING-SAS", "CALLWAITING-ACK", "INVALID"
FTDM_STR2ENUM_P(ftdm_str2ftdm_tonemap, ftdm_tonemap2str, ftdm_tonemap_t)

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
FTDM_STR2ENUM_P(ftdm_str2ftdm_trunk_type, ftdm_trunk_type2str, ftdm_trunk_type_t)

typedef enum {
	FTDM_ANALOG_START_KEWL,
	FTDM_ANALOG_START_LOOP,
	FTDM_ANALOG_START_GROUND,
	FTDM_ANALOG_START_WINK,
	FTDM_ANALOG_START_NA
} ftdm_analog_start_type_t;
#define START_TYPE_STRINGS "KEWL", "LOOP", "GROUND", "WINK", "NA"
FTDM_STR2ENUM_P(ftdm_str2ftdm_analog_start_type, ftdm_analog_start_type2str, ftdm_analog_start_type_t)

typedef enum {
	FTDM_OOB_ONHOOK,
	FTDM_OOB_OFFHOOK,
	FTDM_OOB_WINK,
	FTDM_OOB_FLASH,
	FTDM_OOB_RING_START,
	FTDM_OOB_RING_STOP,
	FTDM_OOB_ALARM_TRAP,
	FTDM_OOB_ALARM_CLEAR,
	FTDM_OOB_NOOP,
	FTDM_OOB_CAS_BITS_CHANGE,
	FTDM_OOB_INVALID
} ftdm_oob_event_t;
#define OOB_STRINGS "DTMF", "ONHOOK", "OFFHOOK", "WINK", "FLASH", "RING_START", "RING_STOP", "ALARM_TRAP", "ALARM_CLEAR", "NOOP", "CAS_BITS_CHANGE", "INVALID"
FTDM_STR2ENUM_P(ftdm_str2ftdm_oob_event, ftdm_oob_event2str, ftdm_oob_event_t)

typedef enum {
	FTDM_ALARM_NONE = 0,
	FTDM_ALARM_RECOVER = (1 << 0),
	FTDM_ALARM_LOOPBACK = (1 << 2),
	FTDM_ALARM_YELLOW = (1 << 3),
	FTDM_ALARM_RED = (1 << 4),
	FTDM_ALARM_BLUE = (1 << 5),
	FTDM_ALARM_NOTOPEN = ( 1 << 6),
	FTDM_ALARM_AIS = ( 1 << 7),
	FTDM_ALARM_RAI = ( 1 << 8),
	FTDM_ALARM_GENERAL = ( 1 << 30)
} ftdm_alarm_flag_t;

typedef enum {
	FTDM_SIGTYPE_NONE,
	FTDM_SIGTYPE_ISDN,
	FTDM_SIGTYPE_RBS,
	FTDM_SIGTYPE_ANALOG,
	FTDM_SIGTYPE_SANGOMABOOST,
	FTDM_SIGTYPE_M3UA,
	FTDM_SIGTYPE_R2
} ftdm_signal_type_t;

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
		"COLLECTED_DIGIT", "ADD_CALL", "RESTART", "SIGLINK_CHANGED", "HWSTATUS_CHANGED", "INVALID"
FTDM_STR2ENUM_P(ftdm_str2ftdm_signal_event, ftdm_signal_event2str, ftdm_signal_event_t)

typedef enum {
	FTDM_EVENT_NONE,
	FTDM_EVENT_DTMF,
	FTDM_EVENT_OOB,
	FTDM_EVENT_COUNT
} ftdm_event_type_t;

typedef enum {
	FTDM_TOP_DOWN,
	FTDM_BOTTOM_UP
} ftdm_direction_t;

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

typedef enum {
	FTDM_NO_FLAGS = 0,
	FTDM_READ =  (1 << 0),
	FTDM_WRITE = (1 << 1),
	FTDM_EVENTS = (1 << 2)
} ftdm_wait_flag_t;

typedef enum {
	FTDM_CODEC_ULAW = 0,
	FTDM_CODEC_ALAW = 8,
	FTDM_CODEC_SLIN = 10,
	FTDM_CODEC_NONE = (1 << 30)
} ftdm_codec_t;

typedef enum {
	FTDM_TONE_DTMF = (1 << 0)
} ftdm_tone_type_t;

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

typedef enum {
	FTDM_SPAN_CONFIGURED = (1 << 0),
	FTDM_SPAN_READY = (1 << 1),
	FTDM_SPAN_STATE_CHANGE = (1 << 2),
	FTDM_SPAN_SUSPENDED = (1 << 3),
	FTDM_SPAN_IN_THREAD = (1 << 4),
	FTDM_SPAN_STOP_THREAD = (1 << 5),
	FTDM_SPAN_USE_CHAN_QUEUE = (1 << 6),
	FTDM_SPAN_SUGGEST_CHAN_ID = (1 << 7),
} ftdm_span_flag_t;

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

#define FTDM_IS_VOICE_CHANNEL(ftdm_chan) ((ftdm_chan)->type != FTDM_CHAN_TYPE_DQ921 && (ftdm_chan)->type != FTDM_CHAN_TYPE_DQ931)

#define CHAN_TYPE_STRINGS "B", "DQ921", "DQ931", "FXS", "FXO", "EM", "CAS", "INVALID"
FTDM_STR2ENUM_P(ftdm_str2ftdm_chan_type, ftdm_chan_type2str, ftdm_chan_type_t)

typedef enum {
	FTDM_CHANNEL_FEATURE_DTMF_DETECT = (1 << 0),
	FTDM_CHANNEL_FEATURE_DTMF_GENERATE = (1 << 1),
	FTDM_CHANNEL_FEATURE_CODECS = (1 << 2),
	FTDM_CHANNEL_FEATURE_INTERVAL = (1 << 3),
	FTDM_CHANNEL_FEATURE_CALLERID = (1 << 4),
	FTDM_CHANNEL_FEATURE_PROGRESS = (1 << 5)
} ftdm_channel_feature_t;

typedef enum {
	FTDM_CHANNEL_STATE_DOWN,
	FTDM_CHANNEL_STATE_HOLD,
	FTDM_CHANNEL_STATE_SUSPENDED,
	FTDM_CHANNEL_STATE_DIALTONE,
	FTDM_CHANNEL_STATE_COLLECT,
	FTDM_CHANNEL_STATE_RING,
	FTDM_CHANNEL_STATE_BUSY,
	FTDM_CHANNEL_STATE_ATTN,
	FTDM_CHANNEL_STATE_GENRING,
	FTDM_CHANNEL_STATE_DIALING,
	FTDM_CHANNEL_STATE_GET_CALLERID,
	FTDM_CHANNEL_STATE_CALLWAITING,
	FTDM_CHANNEL_STATE_RESTART,
	FTDM_CHANNEL_STATE_PROGRESS,
	FTDM_CHANNEL_STATE_PROGRESS_MEDIA,
	FTDM_CHANNEL_STATE_UP,
	FTDM_CHANNEL_STATE_IDLE,
	FTDM_CHANNEL_STATE_TERMINATING,
	FTDM_CHANNEL_STATE_CANCEL,
	FTDM_CHANNEL_STATE_HANGUP,
	FTDM_CHANNEL_STATE_HANGUP_COMPLETE,
	FTDM_CHANNEL_STATE_IN_LOOP,
	FTDM_CHANNEL_STATE_INVALID
} ftdm_channel_state_t;
#define CHANNEL_STATE_STRINGS "DOWN", "HOLD", "SUSPENDED", "DIALTONE", "COLLECT", \
		"RING", "BUSY", "ATTN", "GENRING", "DIALING", "GET_CALLERID", "CALLWAITING", \
		"RESTART", "PROGRESS", "PROGRESS_MEDIA", "UP", "IDLE", "TERMINATING", "CANCEL", "HANGUP", "HANGUP_COMPLETE", "INVALID"
FTDM_STR2ENUM_P(ftdm_str2ftdm_channel_state, ftdm_channel_state2str, ftdm_channel_state_t)

typedef enum {
	FTDM_CHANNEL_CONFIGURED = (1 << 0),
	FTDM_CHANNEL_READY = (1 << 1),
	FTDM_CHANNEL_OPEN = (1 << 2),
	FTDM_CHANNEL_DTMF_DETECT = (1 << 3),
	FTDM_CHANNEL_SUPRESS_DTMF = (1 << 4),
	FTDM_CHANNEL_TRANSCODE = (1 << 5),
	FTDM_CHANNEL_BUFFER = (1 << 6),
	FTDM_CHANNEL_EVENT = (1 << 7),
	FTDM_CHANNEL_INTHREAD = (1 << 8),
	FTDM_CHANNEL_WINK = (1 << 9),
	FTDM_CHANNEL_FLASH = (1 << 10),
	FTDM_CHANNEL_STATE_CHANGE = (1 << 11),
	FTDM_CHANNEL_HOLD = (1 << 12),
	FTDM_CHANNEL_INUSE = (1 << 13),
	FTDM_CHANNEL_OFFHOOK = (1 << 14),
	FTDM_CHANNEL_RINGING = (1 << 15),
	FTDM_CHANNEL_PROGRESS_DETECT = (1 << 16),
	FTDM_CHANNEL_CALLERID_DETECT = (1 << 17),
	FTDM_CHANNEL_OUTBOUND = (1 << 18),
	FTDM_CHANNEL_SUSPENDED = (1 << 19),
	FTDM_CHANNEL_3WAY = (1 << 20),
	FTDM_CHANNEL_PROGRESS = (1 << 21),
	FTDM_CHANNEL_MEDIA = (1 << 22),
	FTDM_CHANNEL_ANSWERED = (1 << 23),
	FTDM_CHANNEL_MUTE = (1 << 24),
	FTDM_CHANNEL_USE_RX_GAIN = (1 << 25),
	FTDM_CHANNEL_USE_TX_GAIN = (1 << 26),
	FTDM_CHANNEL_IN_ALARM = (1 << 27),
} ftdm_channel_flag_t;
#if defined(__cplusplus) && defined(WIN32) 
    // fix C2676 
__inline__ ftdm_channel_flag_t operator|=(ftdm_channel_flag_t a, int32_t b) {
    a = (ftdm_channel_flag_t)(a | b);
    return a;
}
__inline__ ftdm_channel_flag_t operator&=(ftdm_channel_flag_t a, int32_t b) {
    a = (ftdm_channel_flag_t)(a & b);
    return a;
}
#endif

typedef enum {
	ZSM_NONE,
	ZSM_UNACCEPTABLE,
	ZSM_ACCEPTABLE
} ftdm_state_map_type_t;

typedef enum {
	ZSD_INBOUND,
	ZSD_OUTBOUND,
} ftdm_state_direction_t;

#define FTDM_MAP_NODE_SIZE 512
#define FTDM_MAP_MAX FTDM_CHANNEL_STATE_INVALID+2

struct ftdm_state_map_node {
	ftdm_state_direction_t direction;
	ftdm_state_map_type_t type;
	ftdm_channel_state_t check_states[FTDM_MAP_MAX];
	ftdm_channel_state_t states[FTDM_MAP_MAX];
};
typedef struct ftdm_state_map_node ftdm_state_map_node_t;

struct ftdm_state_map {
	ftdm_state_map_node_t nodes[FTDM_MAP_NODE_SIZE];
};
typedef struct ftdm_state_map ftdm_state_map_t;

typedef enum ftdm_channel_hw_link_status {
	FTDM_HW_LINK_DISCONNECTED = 0,
	FTDM_HW_LINK_CONNECTED
} ftdm_channel_hw_link_status_t;

typedef struct ftdm_conf_parameter_s {
	const char *var;
	const char *val;
} ftdm_conf_parameter_t;

typedef struct ftdm_channel ftdm_channel_t;
typedef struct ftdm_event ftdm_event_t;
typedef struct ftdm_sigmsg ftdm_sigmsg_t;
typedef struct ftdm_span ftdm_span_t;
typedef struct ftdm_group ftdm_group_t;
typedef struct ftdm_caller_data ftdm_caller_data_t;
typedef struct ftdm_io_interface ftdm_io_interface_t;

struct ftdm_stream_handle;
typedef struct ftdm_stream_handle ftdm_stream_handle_t;

typedef ftdm_status_t (*ftdm_stream_handle_raw_write_function_t) (ftdm_stream_handle_t *handle, uint8_t *data, ftdm_size_t datalen);
typedef ftdm_status_t (*ftdm_stream_handle_write_function_t) (ftdm_stream_handle_t *handle, const char *fmt, ...);

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
#define FIO_CODEC_ARGS (void *data, ftdm_size_t max, ftdm_size_t *datalen)
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
typedef ftdm_status_t (*fio_codec_t) FIO_CODEC_ARGS ;
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
#define FIO_CODEC_FUNCTION(name) FT_DECLARE_NONSTD(ftdm_status_t) name FIO_CODEC_ARGS
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

#include "ftdm_dso.h"

typedef struct {
	char name[256];
	fio_io_load_t io_load;
	fio_io_unload_t io_unload;
	fio_sig_load_t sig_load;
	fio_sig_configure_t sig_configure;
	fio_sig_unload_t sig_unload;
	/*! 
	  \brief configure a given span signaling 
	  \see sig_configure
	  This is just like sig_configure but receives
	  an array of paramters instead of va_list
	  I'd like to deprecate sig_configure and move
	  all modules to use sigparam_configure
	 */
	fio_configure_span_signaling_t configure_span_signaling;
	ftdm_dso_lib_t lib;
	char path[256];
} ftdm_module_t;

#ifndef __FUNCTION__
#define __FUNCTION__ (const char *)__func__
#endif

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

typedef struct ftdm_fsk_data_state ftdm_fsk_data_state_t;
typedef int (*ftdm_fsk_data_decoder_t)(ftdm_fsk_data_state_t *state);
typedef ftdm_status_t (*ftdm_fsk_write_sample_t)(int16_t *buf, ftdm_size_t buflen, void *user_data);
typedef void (*ftdm_logger_t)(const char *file, const char *func, int line, int level, const char *fmt, ...);
typedef struct hashtable ftdm_hash_t;
typedef struct hashtable_iterator ftdm_hash_iterator_t;
typedef struct key ftdm_hash_key_t;
typedef struct value ftdm_hash_val_t;
typedef struct ftdm_bitstream ftdm_bitstream_t;
typedef struct ftdm_fsk_modulator ftdm_fsk_modulator_t;
typedef ftdm_status_t (*ftdm_span_start_t)(ftdm_span_t *span);
typedef ftdm_status_t (*ftdm_span_stop_t)(ftdm_span_t *span);

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

#ifdef __cplusplus
}
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

