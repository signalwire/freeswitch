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
 */

#ifndef ZAP_TYPES_H
#define ZAP_TYPES_H

#ifdef WIN32
#include <windows.h>
typedef HANDLE zap_socket_t;
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef __int32 int32_t;
#else
#include <stdint.h>
typedef int zap_socket_t;
#endif

typedef size_t zap_size_t;
struct zap_io_interface;

#define ZAP_COMMAND_OBJ_INT *((int *)obj)
#define ZAP_COMMAND_OBJ_CHAR_P (char *)obj

typedef uint64_t zap_time_t; 

#define ZAP_TONEMAP_LEN 128
typedef enum {
	ZAP_TONEMAP_DIAL,
	ZAP_TONEMAP_RING,
	ZAP_TONEMAP_BUSY,
	ZAP_TONEMAP_ATTN,
	ZAP_TONEMAP_INVALID
} zap_tonemap_t;
#define TONEMAP_STRINGS "DIAL", "RING", "BUSY", "ATTN", "INVALID"
ZAP_STR2ENUM_P(zap_str2zap_tonemap, zap_tonemap2str, zap_tonemap_t)

typedef enum {
	ZAP_TRUNK_E1,
	ZAP_TRUNK_T1,
	ZAP_TRUNK_J1,
	ZAP_TRUNK_BRI,
	ZAP_TRUNK_NONE
} zap_trunk_type_t;
#define TRUNK_STRINGS "E1", "T1", "J1", "BRI", "NONE"
ZAP_STR2ENUM_P(zap_str2zap_trunk_type, zap_trunk_type2str, zap_trunk_type_t)

typedef enum {
	ZAP_OOB_DTMF, ZAP_OOB_ONHOOK,
	ZAP_OOB_OFFHOOK,
	ZAP_OOB_WINK,
	ZAP_OOB_FLASH,
	ZAP_OOB_RING_START,
	ZAP_OOB_RING_STOP,
	ZAP_OOB_INVALID
} zap_oob_event_t;
#define OOB_STRINGS "DTMF", "ONHOOK", "OFFHOOK", "WINK", "FLASH", "RING_START", "RING_STOP", "INVALID"
ZAP_STR2ENUM_P(zap_str2zap_oob_event, zap_oob_event2str, zap_oob_event_t)

typedef enum {
	ZAP_SIGTYPE_NONE,
	ZAP_SIGTYPE_ISDN,
	ZAP_SIGTYPE_RBS,
	ZAP_SIGTYPE_ANALOG
} zap_signal_type_t;

typedef enum {
	ZAP_SIGEVENT_CALL_START,
	ZAP_SIGEVENT_CALL_STOP,
	ZAP_SIGEVENT_CALL_TRANSFER,
	ZAP_SIGEVENT_ANSWER,
	ZAP_SIGEVENT_PROGRESS,
	ZAP_SIGEVENT_PROGRESS_MEDIA,
	ZAP_SIGEVENT_NOTIFY,
	ZAP_SIGEVENT_MISC
} zap_signal_event_t;

typedef enum {
	ZAP_EVENT_NONE,
	ZAP_EVENT_DTMF,
	ZAP_EVENT_OOB,
	ZAP_EVENT_COUNT
} zap_event_type_t;

typedef enum {
	ZAP_TOP_DOWN,
	ZAP_BOTTOM_UP
} zap_direction_t;

typedef enum {
	ZAP_SUCCESS,
	ZAP_FAIL,
	ZAP_MEMERR,
	ZAP_TIMEOUT,
	ZAP_NOTIMPL,

	ZAP_STATUS_COUNT
} zap_status_t;

typedef enum {
	ZAP_NO_FLAGS = 0,
	ZAP_READ =  (1 << 0),
	ZAP_WRITE = (1 << 1),
	ZAP_EVENTS = (1 << 2)
} zap_wait_flag_t;

typedef enum {
	ZAP_CODEC_ULAW = 0,
	ZAP_CODEC_ALAW = 8,
	ZAP_CODEC_SLIN = 10,
	ZAP_CODEC_NONE = (1 << 31)
} zap_codec_t;

typedef enum {
	ZAP_TONE_DTMF = (1 << 0)
} zap_tone_type_t;

typedef enum {
	ZAP_COMMAND_NOOP,
	ZAP_COMMAND_SET_INTERVAL,
	ZAP_COMMAND_GET_INTERVAL,
	ZAP_COMMAND_SET_CODEC,
	ZAP_COMMAND_GET_CODEC,
	ZAP_COMMAND_ENABLE_TONE_DETECT,
	ZAP_COMMAND_DISABLE_TONE_DETECT,
	ZAP_COMMAND_SEND_DTMF,
	ZAP_COMMAND_SET_DTMF_ON_PERIOD,
	ZAP_COMMAND_GET_DTMF_ON_PERIOD,
	ZAP_COMMAND_SET_DTMF_OFF_PERIOD,
	ZAP_COMMAND_GET_DTMF_OFF_PERIOD,

	ZAP_COMMAND_COUNT
} zap_command_t;

typedef enum {
	ZAP_SPAN_CONFIGURED = (1 << 0),
	ZAP_SPAN_READY = (1 << 1)
} zap_span_flag_t;

typedef enum {
	ZAP_CHAN_TYPE_B,
	ZAP_CHAN_TYPE_DQ921,
	ZAP_CHAN_TYPE_DQ931,
	ZAP_CHAN_TYPE_FXS,
	ZAP_CHAN_TYPE_FXO,
	ZAP_CHAN_TYPE_COUNT
} zap_chan_type_t;

typedef enum {
	ZAP_CHANNEL_FEATURE_DTMF = (1 << 0),
	ZAP_CHANNEL_FEATURE_CODECS = (1 << 1),
	ZAP_CHANNEL_FEATURE_INTERVAL = (1 << 2)
} zap_channel_feature_t;

typedef enum {
	ZAP_CHANNEL_STATE_DOWN,
	ZAP_CHANNEL_STATE_UP,
	ZAP_CHANNEL_STATE_DIALTONE,
	ZAP_CHANNEL_STATE_COLLECT
} zap_channel_state_t;

typedef enum {
	ZAP_CHANNEL_CONFIGURED = (1 << 0),
	ZAP_CHANNEL_READY = (1 << 1),
	ZAP_CHANNEL_OPEN = (1 << 2),
	ZAP_CHANNEL_DTMF_DETECT = (1 << 3),
	ZAP_CHANNEL_SUPRESS_DTMF = (1 << 4),
	ZAP_CHANNEL_TRANSCODE = (1 << 5),
	ZAP_CHANNEL_BUFFER = (1 << 6),
	ZAP_CHANNEL_EVENT = (1 << 7),
	ZAP_CHANNEL_INTHREAD = (1 << 8),
	ZAP_CHANNEL_WINK = (1 << 9),
	ZAP_CHANNEL_FLASH = (1 << 10)
} zap_channel_flag_t;


typedef struct zap_channel zap_channel_t;
typedef struct zap_event zap_event_t;
typedef struct zap_sigmsg zap_sigmsg_t;
typedef struct zap_span zap_span_t;

#define ZIO_SPAN_POLL_EVENT_ARGS (zap_span_t *span, uint32_t ms)
#define ZIO_SPAN_NEXT_EVENT_ARGS (zap_span_t *span, zap_event_t **event)
#define ZIO_SIGNAL_CB_ARGS (zap_span_t *span, zap_sigmsg_t *sigmsg, void *raw_data, uint32_t raw_data_len)
#define ZIO_EVENT_CB_ARGS (zap_channel_t *zchan, zap_event_t *event)
#define ZIO_CODEC_ARGS (void *data, zap_size_t max, zap_size_t *datalen)
#define ZIO_CONFIGURE_ARGS (struct zap_io_interface *zio)
#define ZIO_OPEN_ARGS (zap_channel_t *zchan)
#define ZIO_CLOSE_ARGS (zap_channel_t *zchan)
#define ZIO_COMMAND_ARGS (zap_channel_t *zchan, zap_command_t command, void *obj)
#define ZIO_WAIT_ARGS (zap_channel_t *zchan, zap_wait_flag_t *flags, int32_t to)
#define ZIO_READ_ARGS (zap_channel_t *zchan, void *data, zap_size_t *datalen)
#define ZIO_WRITE_ARGS (zap_channel_t *zchan, void *data, zap_size_t *datalen)

typedef zap_status_t (*zio_span_poll_event_t) ZIO_SPAN_POLL_EVENT_ARGS ;
typedef zap_status_t (*zio_span_next_event_t) ZIO_SPAN_NEXT_EVENT_ARGS ;
typedef zap_status_t (*zio_signal_cb_t) ZIO_SIGNAL_CB_ARGS ;
typedef zap_status_t (*zio_event_cb_t) ZIO_EVENT_CB_ARGS ;
typedef zap_status_t (*zio_codec_t) ZIO_CODEC_ARGS ;
typedef zap_status_t (*zio_configure_t) ZIO_CONFIGURE_ARGS ;
typedef zap_status_t (*zio_open_t) ZIO_OPEN_ARGS ;
typedef zap_status_t (*zio_close_t) ZIO_CLOSE_ARGS ;
typedef zap_status_t (*zio_command_t) ZIO_COMMAND_ARGS ;
typedef zap_status_t (*zio_wait_t) ZIO_WAIT_ARGS ;
typedef zap_status_t (*zio_read_t) ZIO_READ_ARGS ;
typedef zap_status_t (*zio_write_t) ZIO_WRITE_ARGS ;

#define ZIO_SPAN_POLL_EVENT_FUNCTION(name) zap_status_t name ZIO_SPAN_POLL_EVENT_ARGS
#define ZIO_SPAN_NEXT_EVENT_FUNCTION(name) zap_status_t name ZIO_SPAN_NEXT_EVENT_ARGS
#define ZIO_SIGNAL_CB_FUNCTION(name) zap_status_t name ZIO_SIGNAL_CB_ARGS
#define ZIO_EVENT_CB_FUNCTION(name) zap_status_t name ZIO_EVENT_CB_ARGS
#define ZIO_CODEC_FUNCTION(name) zap_status_t name ZIO_CODEC_ARGS
#define ZIO_CONFIGURE_FUNCTION(name) zap_status_t name ZIO_CONFIGURE_ARGS
#define ZIO_OPEN_FUNCTION(name) zap_status_t name ZIO_OPEN_ARGS
#define ZIO_CLOSE_FUNCTION(name) zap_status_t name ZIO_CLOSE_ARGS
#define ZIO_COMMAND_FUNCTION(name) zap_status_t name ZIO_COMMAND_ARGS
#define ZIO_WAIT_FUNCTION(name) zap_status_t name ZIO_WAIT_ARGS
#define ZIO_READ_FUNCTION(name) zap_status_t name ZIO_READ_ARGS
#define ZIO_WRITE_FUNCTION(name) zap_status_t name ZIO_WRITE_ARGS

#define ZIO_CONFIGURE_MUZZLE assert(zio != NULL)
#define ZIO_OPEN_MUZZLE assert(zchan != NULL)
#define ZIO_CLOSE_MUZZLE assert(zchan != NULL)
#define ZIO_COMMAND_MUZZLE assert(zchan != NULL); assert(command != 0); assert(obj != NULL)
#define ZIO_WAIT_MUZZLE assert(zchan != NULL); assert(flags != 0); assert(to != 0)
#define ZIO_READ_MUZZLE assert(zchan != NULL); assert(data != NULL); assert(datalen != NULL)
#define ZIO_WRITE_MUZZLE assert(zchan != NULL); assert(data != NULL); assert(datalen != NULL)

#define ZAP_PRE __FILE__, __FUNCTION__, __LINE__

#define ZAP_LOG_LEVEL_DEBUG 7
#define ZAP_LOG_LEVEL_INFO 6
#define ZAP_LOG_LEVEL_NOTICE 5
#define ZAP_LOG_LEVEL_WARNING 4
#define ZAP_LOG_LEVEL_ERROR 3
#define ZAP_LOG_LEVEL_CRIT 2
#define ZAP_LOG_LEVEL_ALERT 1
#define ZAP_LOG_LEVEL_EMERG 0

#define ZAP_LOG_DEBUG ZAP_PRE, ZAP_LOG_LEVEL_DEBUG
#define ZAP_LOG_INFO ZAP_PRE, ZAP_LOG_LEVEL_INFO
#define ZAP_LOG_NOTICE ZAP_PRE, ZAP_LOG_LEVEL_NOTICE
#define ZAP_LOG_WARNING ZAP_PRE, ZAP_LOG_LEVEL_WARNING
#define ZAP_LOG_ERROR ZAP_PRE, ZAP_LOG_LEVEL_ERROR
#define ZAP_LOG_CRIT ZAP_PRE, ZAP_LOG_LEVEL_CRIT
#define ZAP_LOG_ALERT ZAP_PRE, ZAP_LOG_LEVEL_ALERT
#define ZAP_LOG_EMERG ZAP_PRE, ZAP_LOG_LEVEL_EMERG


typedef void (*zap_logger_t)(char *file, const char *func, int line, int level, char *fmt, ...);
typedef struct zap_io_interface zap_io_interface_t;
typedef struct hashtable zap_hash_t;
typedef struct hashtable_itr zap_hash_itr_t;
typedef struct key zap_hash_key_t;
typedef struct value zap_hash_val_t;
#endif
