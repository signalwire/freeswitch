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
#else
#include <stdint.h>
typedef int zap_socket_t;
#endif

typedef size_t zap_size_t;
struct zap_software_interface;

#define ZAP_COMMAND_OBJ_INT *((int *)obj)
#define ZAP_COMMAND_OBJ_CHAR_P (char *)obj

typedef enum {
	ZAP_TOP_DOWN,
	ZAP_BOTTOM_UP
} zap_direction_t;

typedef enum {
	ZAP_SUCCESS,
	ZAP_FAIL,
	ZAP_MEMERR,
	ZAP_TIMEOUT
} zap_status_t;

typedef enum {
	ZAP_NO_FLAGS = 0,
	ZAP_READ =  (1 <<  0),
	ZAP_WRITE = (1 <<  1),
	ZAP_ERROR = (1 <<  2)
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
	ZAP_COMMAND_GET_DTMF_OFF_PERIOD
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
	ZAP_CHAN_TYPE_FXO
} zap_chan_type_t;

typedef enum {
	ZAP_CHANNEL_FEATURE_DTMF = (1 << 0),
	ZAP_CHANNEL_FEATURE_CODECS = (1 << 1)
} zap_channel_feature_t;

typedef enum {
	ZAP_CHANNEL_CONFIGURED = (1 << 0),
	ZAP_CHANNEL_READY = (1 << 1),
	ZAP_CHANNEL_OPEN = (1 << 2),
	ZAP_CHANNEL_DTMF_DETECT = (1 << 3),
	ZAP_CHANNEL_SUPRESS_DTMF = (1 << 4),
	ZAP_CHANNEL_TRANSCODE = (1 << 5)
} zap_channel_flag_t;

typedef struct zap_channel zap_channel_t;
typedef struct zap_event zap_event_t;

#define ZINT_EVENT_CB_ARGS (zap_channel_t *zchan, zap_event_t *event)
#define ZINT_CODEC_ARGS (void *data, zap_size_t max, zap_size_t *datalen)
#define ZINT_CONFIGURE_ARGS (struct zap_software_interface *zint)
#define ZINT_OPEN_ARGS (zap_channel_t *zchan)
#define ZINT_CLOSE_ARGS (zap_channel_t *zchan)
#define ZINT_COMMAND_ARGS (zap_channel_t *zchan, zap_command_t command, void *obj)
#define ZINT_WAIT_ARGS (zap_channel_t *zchan, zap_wait_flag_t *flags, uint32_t to)
#define ZINT_READ_ARGS (zap_channel_t *zchan, void *data, zap_size_t *datalen)
#define ZINT_WRITE_ARGS (zap_channel_t *zchan, void *data, zap_size_t *datalen)

typedef zap_status_t (*zint_event_cb_t) ZINT_EVENT_CB_ARGS ;
typedef zap_status_t (*zint_codec_t) ZINT_CODEC_ARGS ;
typedef zap_status_t (*zint_configure_t) ZINT_CONFIGURE_ARGS ;
typedef zap_status_t (*zint_open_t) ZINT_OPEN_ARGS ;
typedef zap_status_t (*zint_close_t) ZINT_CLOSE_ARGS ;
typedef zap_status_t (*zint_command_t) ZINT_COMMAND_ARGS ;
typedef zap_status_t (*zint_wait_t) ZINT_WAIT_ARGS ;
typedef zap_status_t (*zint_read_t) ZINT_READ_ARGS ;
typedef zap_status_t (*zint_write_t) ZINT_WRITE_ARGS ;

#define ZINT_EVENT_CB_FUNCTION(name) zap_status_t name ZINT_EVENT_CB_ARGS
#define ZINT_CODEC_FUNCTION(name) zap_status_t name ZINT_CODEC_ARGS
#define ZINT_CONFIGURE_FUNCTION(name) zap_status_t name ZINT_CONFIGURE_ARGS
#define ZINT_OPEN_FUNCTION(name) zap_status_t name ZINT_OPEN_ARGS
#define ZINT_CLOSE_FUNCTION(name) zap_status_t name ZINT_CLOSE_ARGS
#define ZINT_COMMAND_FUNCTION(name) zap_status_t name ZINT_COMMAND_ARGS
#define ZINT_WAIT_FUNCTION(name) zap_status_t name ZINT_WAIT_ARGS
#define ZINT_READ_FUNCTION(name) zap_status_t name ZINT_READ_ARGS
#define ZINT_WRITE_FUNCTION(name) zap_status_t name ZINT_WRITE_ARGS

#define ZINT_CONFIGURE_MUZZLE assert(zint != NULL)
#define ZINT_OPEN_MUZZLE assert(zchan != NULL)
#define ZINT_CLOSE_MUZZLE assert(zchan != NULL)
#define ZINT_COMMAND_MUZZLE assert(zchan != NULL); assert(command != 0); assert(obj != NULL)
#define ZINT_WAIT_MUZZLE assert(zchan != NULL); assert(flags != 0); assert(to != 0)
#define ZINT_READ_MUZZLE assert(zchan != NULL); assert(data != NULL); assert(datalen != NULL)
#define ZINT_WRITE_MUZZLE assert(zchan != NULL); assert(data != NULL); assert(datalen != NULL)

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

typedef enum {
	ZAP_EVENT_NONE,
	ZAP_EVENT_DTMF
} zap_event_type_t;

typedef struct zap_span zap_span_t;
typedef void (*zap_logger_t)(char *file, const char *func, int line, int level, char *fmt, ...);
typedef struct zap_software_interface zap_software_interface_t;
typedef struct hashtable zap_hash_t;
typedef struct hashtable_itr zap_hash_itr_t;
typedef struct key zap_hash_key_t;
typedef struct value zap_hash_val_t;
#endif
