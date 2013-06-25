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
 */

#ifndef OPENZAP_H
#define OPENZAP_H


#if !defined(_XOPEN_SOURCE) && !defined(__FreeBSD__)
#define _XOPEN_SOURCE 600
#endif

#ifndef HAVE_STRINGS_H
#define HAVE_STRINGS_H 1
#endif
#ifndef HAVE_SYS_SOCKET_H
#define HAVE_SYS_SOCKET_H 1
#endif

#ifndef __WINDOWS__
#if defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32) || defined(_WIN64)
#define __WINDOWS__
#endif
#endif

#ifdef _MSC_VER
#if defined(OZ_DECLARE_STATIC)
#define OZ_DECLARE(type)			type __stdcall
#define OZ_DECLARE_NONSTD(type)		type __cdecl
#define OZ_DECLARE_DATA
#elif defined(OPENZAP_EXPORTS)
#define OZ_DECLARE(type)			__declspec(dllexport) type __stdcall
#define OZ_DECLARE_NONSTD(type)		__declspec(dllexport) type __cdecl
#define OZ_DECLARE_DATA				__declspec(dllexport)
#else
#define OZ_DECLARE(type)			__declspec(dllimport) type __stdcall
#define OZ_DECLARE_NONSTD(type)		__declspec(dllimport) type __cdecl
#define OZ_DECLARE_DATA				__declspec(dllimport)
#endif
#define EX_DECLARE_DATA				__declspec(dllexport)
#else
#if (defined(__GNUC__) || defined(__SUNPRO_CC) || defined (__SUNPRO_C)) && defined(HAVE_VISIBILITY)
#define OZ_DECLARE(type)		__attribute__((visibility("default"))) type
#define OZ_DECLARE_NONSTD(type)	__attribute__((visibility("default"))) type
#define OZ_DECLARE_DATA		__attribute__((visibility("default")))
#else
#define OZ_DECLARE(type)		type
#define OZ_DECLARE_NONSTD(type)	type
#define OZ_DECLARE_DATA
#endif
#define EX_DECLARE_DATA
#endif

#ifdef _MSC_VER
#ifndef __inline__
#define __inline__ __inline
#endif
#if (_MSC_VER >= 1400)			/* VC8+ */
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif
#endif
#ifndef strcasecmp
#define strcasecmp(s1, s2) _stricmp(s1, s2)
#endif
#ifndef strncasecmp
#define strncasecmp(s1, s2, n) _strnicmp(s1, s2, n)
#endif
#ifndef snprintf
#define snprintf _snprintf
#endif
#ifndef S_IRUSR
#define S_IRUSR _S_IREAD
#endif
#ifndef S_IWUSR
#define S_IWUSR _S_IWRITE
#endif
#undef HAVE_STRINGS_H
#undef HAVE_SYS_SOCKET_H
/* disable warning for zero length array in a struct */
/* this will cause errors on c99 and ansi compliant compilers and will need to be fixed in the wanpipe header files */
#pragma warning(disable:4706)
#pragma comment(lib, "Winmm")
#endif

#define ZAP_THREAD_STACKSIZE 240 * 1024
#define ZAP_ENUM_NAMES(_NAME, _STRINGS) static const char * _NAME [] = { _STRINGS , NULL };
#define ZAP_STR2ENUM_P(_FUNC1, _FUNC2, _TYPE) OZ_DECLARE(_TYPE) _FUNC1 (const char *name); OZ_DECLARE(const char *) _FUNC2 (_TYPE type);
#define ZAP_STR2ENUM(_FUNC1, _FUNC2, _TYPE, _STRINGS, _MAX)	\
	OZ_DECLARE(_TYPE) _FUNC1 (const char *name)							\
	{														\
		int i;												\
		_TYPE t = _MAX ;									\
															\
		for (i = 0; i < _MAX ; i++) {						\
			if (!strcasecmp(name, _STRINGS[i])) {			\
				t = (_TYPE) i;								\
				break;										\
			}												\
		}													\
															\
		return t;											\
	}														\
	OZ_DECLARE(const char *) _FUNC2 (_TYPE type)						\
	{														\
		if (type > _MAX) {									\
			type = _MAX;									\
		}													\
		return _STRINGS[(int)type];							\
	}														\
	
#define zap_true(expr)							\
	(expr && ( !strcasecmp(expr, "yes") ||		\
			   !strcasecmp(expr, "on") ||		\
			   !strcasecmp(expr, "true") ||		\
			   !strcasecmp(expr, "enabled") ||	\
			   !strcasecmp(expr, "active") ||	\
			   atoi(expr))) ? 1 : 0


#include <time.h>
#ifndef __WINDOWS__
#include <sys/time.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <assert.h>
#include "zap_types.h"
#include "hashtable.h"
#include "zap_config.h"
#include "g711.h"
#include "libteletone.h"
#include "zap_buffer.h"
#include "zap_threadmutex.h"

#ifdef __WINDOWS__
#define zap_sleep(x) Sleep(x)
#else
#define zap_sleep(x) usleep(x * 1000)
#endif

#ifdef NDEBUG
#undef assert
#define assert(_Expression)     ((void)(_Expression))
#endif

#define ZAP_MAX_CHANNELS_PHYSICAL_SPAN 32
#define ZAP_MAX_PHYSICAL_SPANS_PER_LOGICAL_SPAN 32
#define ZAP_MAX_CHANNELS_SPAN ZAP_MAX_CHANNELS_PHYSICAL_SPAN * ZAP_MAX_PHYSICAL_SPANS_PER_LOGICAL_SPAN
#define ZAP_MAX_SPANS_INTERFACE 128

#define GOTO_STATUS(label,st) status = st; goto label ;

#define zap_copy_string(x,y,z) strncpy(x, y, z - 1) 
#define zap_set_string(x,y) strncpy(x, y, sizeof(x)-1) 
#define zap_strlen_zero(s) (!s || *s == '\0')
#define zap_strlen_zero_buf(s) (*s == '\0')


#define zap_channel_test_feature(obj, flag) ((obj)->features & flag)
#define zap_channel_set_feature(obj, flag) (obj)->features |= (flag)
#define zap_channel_clear_feature(obj, flag) (obj)->features &= ~(flag)
#define zap_channel_set_member_locked(obj, _m, _v) zap_mutex_lock(obj->mutex); obj->_m = _v; zap_mutex_unlock(obj->mutex)

/*!
  \brief Test for the existance of a flag on an arbitary object
  \command obj the object to test
  \command flag the or'd list of flags to test
  \return true value if the object has the flags defined
*/
#define zap_test_flag(obj, flag) ((obj)->flags & flag)
#define zap_test_pflag(obj, flag) ((obj)->pflags & flag)
#define zap_test_sflag(obj, flag) ((obj)->sflags & flag)


#define zap_set_alarm_flag(obj, flag) (obj)->alarm_flags |= (flag)
#define zap_clear_alarm_flag(obj, flag) (obj)->alarm_flags &= ~(flag)
#define zap_test_alarm_flag(obj, flag) ((obj)->alarm_flags & flag)

/*!
  \brief Set a flag on an arbitrary object
  \command obj the object to set the flags on
  \command flag the or'd list of flags to set
*/
#define zap_set_flag(obj, flag) (obj)->flags |= (flag)
#define zap_set_flag_locked(obj, flag) assert(obj->mutex != NULL);	\
	zap_mutex_lock(obj->mutex);										\
	(obj)->flags |= (flag);											\
	zap_mutex_unlock(obj->mutex);

#define zap_set_pflag(obj, flag) (obj)->pflags |= (flag)
#define zap_set_pflag_locked(obj, flag) assert(obj->mutex != NULL);	\
	zap_mutex_lock(obj->mutex);										\
	(obj)->pflags |= (flag);											\
	zap_mutex_unlock(obj->mutex);

#define zap_set_sflag(obj, flag) (obj)->sflags |= (flag)
#define zap_set_sflag_locked(obj, flag) assert(obj->mutex != NULL);	\
	zap_mutex_lock(obj->mutex);										\
	(obj)->sflags |= (flag);											\
	zap_mutex_unlock(obj->mutex);

/*!
  \brief Clear a flag on an arbitrary object while locked
  \command obj the object to test
  \command flag the or'd list of flags to clear
*/
#define zap_clear_flag(obj, flag) (obj)->flags &= ~(flag)

#define zap_clear_flag_locked(obj, flag) assert(obj->mutex != NULL); zap_mutex_lock(obj->mutex); (obj)->flags &= ~(flag); zap_mutex_unlock(obj->mutex);

#define zap_clear_pflag(obj, flag) (obj)->pflags &= ~(flag)

#define zap_clear_pflag_locked(obj, flag) assert(obj->mutex != NULL); zap_mutex_lock(obj->mutex); (obj)->pflags &= ~(flag); zap_mutex_unlock(obj->mutex);

#define zap_clear_sflag(obj, flag) (obj)->sflags &= ~(flag)

#define zap_clear_sflag_locked(obj, flag) assert(obj->mutex != NULL); zap_mutex_lock(obj->mutex); (obj)->sflags &= ~(flag); zap_mutex_unlock(obj->mutex);

#define zap_log_chan(zchan, level, format, ...) zap_log(level, "[s%dc%d][%d:%d] " format, zchan->span_id, zchan->chan_id, zchan->physical_span_id, zchan->physical_chan_id, __VA_ARGS__)

#define zap_log_chan_msg(zchan, level, msg) zap_log(level, "[s%dc%d][%d:%d] " msg, zchan->span_id, zchan->chan_id, zchan->physical_span_id, zchan->physical_chan_id)

#define zap_set_state_locked(obj, s) if ( obj->state == s ) {			\
		if (s != ZAP_CHANNEL_STATE_HANGUP) zap_log(ZAP_LOG_WARNING, "Why bother changing state on %d:%d from %s to %s\n", obj->span_id, obj->chan_id, zap_channel_state2str(obj->state), zap_channel_state2str(s)); \
	} else if (zap_test_flag(obj, ZAP_CHANNEL_READY)) {									\
		int st = obj->state;											\
		zap_channel_set_state(obj, s, 1);									\
		if (obj->state == s) zap_log(ZAP_LOG_DEBUG, "Changing state on %d:%d from %s to %s\n", obj->span_id, obj->chan_id, zap_channel_state2str(st), zap_channel_state2str(s)); \
		else if (!((obj->state == ZAP_CHANNEL_STATE_HANGUP && s == ZAP_CHANNEL_STATE_TERMINATING) || (obj->state == ZAP_CHANNEL_STATE_HANGUP_COMPLETE && s == ZAP_CHANNEL_STATE_HANGUP) || (obj->state == ZAP_CHANNEL_STATE_TERMINATING && s == ZAP_CHANNEL_STATE_HANGUP))) zap_log(ZAP_LOG_WARNING, "VETO Changing state on %d:%d from %s to %s\n", obj->span_id, obj->chan_id, zap_channel_state2str(st), zap_channel_state2str(s)); \
	}

#ifdef _MSC_VER
/* The while(0) below throws a conditional expression is constant warning */
#pragma warning(disable:4127) 
#endif

#define zap_set_state_locked_wait(obj, s) 						\
	do {										\
		int __safety = 100;							\
		zap_set_state_locked(obj, s);						\
		while(__safety-- && zap_test_flag(obj, ZAP_CHANNEL_STATE_CHANGE)) {	\
			zap_sleep(10);							\
		}									\
		if(!__safety) {								\
			zap_log(ZAP_LOG_CRIT, "State change not completed\n");		\
		}									\
	} while(0);

#define zap_wait_for_flag_cleared(obj, flag, time) 					\
	do {										\
		int __safety = time;							\
		while(__safety-- && zap_test_flag(obj, flag)) { 			\
			zap_mutex_unlock(obj->mutex);					\
			zap_sleep(10);							\
			zap_mutex_lock(obj->mutex);					\
		}									\
		if(!__safety) {								\
			zap_log(ZAP_LOG_CRIT, "flag %d was never cleared\n", flag);	\
		}									\
	} while(0);

#define zap_set_state_wait(obj, s) 						\
	do {										\
		zap_channel_set_state(obj, s, 0);					\
		zap_wait_for_flag_cleared(obj, ZAP_CHANNEL_STATE_CHANGE, 100);     \
	} while(0);


typedef enum {
	ZAP_STATE_CHANGE_FAIL,
	ZAP_STATE_CHANGE_SUCCESS,
	ZAP_STATE_CHANGE_SAME,
} zap_state_change_result_t;

#define zap_set_state_r(obj, s, l, r) if ( obj->state == s ) {	\
		if (s != ZAP_CHANNEL_STATE_HANGUP) zap_log(ZAP_LOG_WARNING, "Why bother changing state on %d:%d from %s to %s\n", obj->span_id, obj->chan_id, zap_channel_state2str(obj->state), zap_channel_state2str(s)); r = ZAP_STATE_CHANGE_SAME; \
	} else if (zap_test_flag(obj, ZAP_CHANNEL_READY)) {					\
		int st = obj->state;											\
		r = (zap_channel_set_state(obj, s, l) == ZAP_SUCCESS) ? ZAP_STATE_CHANGE_SUCCESS : ZAP_STATE_CHANGE_FAIL; \
		if (obj->state == s) {zap_log(ZAP_LOG_DEBUG, "Changing state on %d:%d from %s to %s\n", obj->span_id, obj->chan_id, zap_channel_state2str(st), zap_channel_state2str(s));} \
		else { if (!((obj->state == ZAP_CHANNEL_STATE_HANGUP && s == ZAP_CHANNEL_STATE_TERMINATING) || (obj->state == ZAP_CHANNEL_STATE_HANGUP_COMPLETE && s == ZAP_CHANNEL_STATE_HANGUP) || (obj->state == ZAP_CHANNEL_STATE_TERMINATING && s == ZAP_CHANNEL_STATE_HANGUP))) zap_log(ZAP_LOG_WARNING, "VETO Changing state on %d:%d from %s to %s\n", obj->span_id, obj->chan_id, zap_channel_state2str(st), zap_channel_state2str(s)); } \
	}


#define zap_is_dtmf(key)  ((key > 47 && key < 58) || (key > 64 && key < 69) || (key > 96 && key < 101) || key == 35 || key == 42 || key == 87 || key == 119)

/*!
  \brief Copy flags from one arbitrary object to another
  \command dest the object to copy the flags to
  \command src the object to copy the flags from
  \command flags the flags to copy
*/
#define zap_copy_flags(dest, src, flags) (dest)->flags &= ~(flags);	(dest)->flags |= ((src)->flags & (flags))

/*!
  \brief Free a pointer and set it to NULL unless it already is NULL
  \command it the pointer
*/
#define zap_safe_free(it) if (it) {free(it);it=NULL;}

#define zap_socket_close(it) if (it > -1) { close(it); it = -1;}


struct zap_stream_handle {
	zap_stream_handle_write_function_t write_function;
	zap_stream_handle_raw_write_function_t raw_write_function;
	void *data;
	void *end;
	zap_size_t data_size;
	zap_size_t data_len;
	zap_size_t alloc_len;
	zap_size_t alloc_chunk;
};


OZ_DECLARE_NONSTD(zap_status_t) zap_console_stream_raw_write(zap_stream_handle_t *handle, uint8_t *data, zap_size_t datalen);
OZ_DECLARE_NONSTD(zap_status_t) zap_console_stream_write(zap_stream_handle_t *handle, const char *fmt, ...);

#define ZAP_CMD_CHUNK_LEN 1024
#define ZAP_STANDARD_STREAM(s) memset(&s, 0, sizeof(s)); s.data = malloc(ZAP_CMD_CHUNK_LEN); \
	assert(s.data);														\
	memset(s.data, 0, ZAP_CMD_CHUNK_LEN);								\
	s.end = s.data;														\
	s.data_size = ZAP_CMD_CHUNK_LEN;									\
	s.write_function = zap_console_stream_write;						\
	s.raw_write_function = zap_console_stream_raw_write;				\
	s.alloc_len = ZAP_CMD_CHUNK_LEN;									\
	s.alloc_chunk = ZAP_CMD_CHUNK_LEN

struct zap_event {
	zap_event_type_t e_type;
	uint32_t enum_id;
	zap_channel_t *channel;
	void *data;
};

#define ZAP_TOKEN_STRLEN 128
#define ZAP_MAX_TOKENS 10

static __inline__ char *zap_clean_string(char *s)
{
	char *p;

	for (p = s; p && *p; p++) {
		uint8_t x = (uint8_t) *p;
		if (x < 32 || x > 127) {
			*p = ' ';
		}
	}

	return s;
}

struct zap_bitstream {
	uint8_t *data;
	uint32_t datalen;
	uint32_t byte_index;
	uint8_t bit_index;
	int8_t endian;
	uint8_t top;
	uint8_t bot;
	uint8_t ss;
	uint8_t ssv;
};

struct zap_fsk_data_state {
	dsp_fsk_handle_t *fsk1200_handle;
	uint8_t init;
	uint8_t *buf;
	size_t bufsize;
	zap_size_t blen;
	zap_size_t bpos;
	zap_size_t dlen;
	zap_size_t ppos;
	int checksum;
};

struct zap_fsk_modulator {
	teletone_dds_state_t dds;
	zap_bitstream_t bs;
	uint32_t carrier_bits_start;
	uint32_t carrier_bits_stop;
	uint32_t chan_sieze_bits;
	uint32_t bit_factor;
	uint32_t bit_accum;
	uint32_t sample_counter;
	int32_t samples_per_bit;
	int32_t est_bytes;
	fsk_modem_types_t modem_type;
	zap_fsk_data_state_t *fsk_data;
	zap_fsk_write_sample_t write_sample_callback;
	void *user_data;
	int16_t sample_buffer[64];
};

/**
 * Type Of Number (TON)
 */
typedef enum {
	ZAP_TON_UNKNOWN = 0,
	ZAP_TON_INTERNATIONAL,
	ZAP_TON_NATIONAL,
	ZAP_TON_NETWORK_SPECIFIC,
	ZAP_TON_SUBSCRIBER_NUMBER,
	ZAP_TON_ABBREVIATED_NUMBER,
	ZAP_TON_RESERVED,
	ZAP_TON_INVALID = 255
} zap_ton_t;

/**
 * Numbering Plan Identification (NPI)
 */
typedef enum {
	ZAP_NPI_UNKNOWN = 0,
	ZAP_NPI_ISDN = 1,
	ZAP_NPI_DATA = 3,
	ZAP_NPI_TELEX = 4,
	ZAP_NPI_NATIONAL = 8,
	ZAP_NPI_PRIVATE = 9,
	ZAP_NPI_RESERVED = 10,
	ZAP_NPI_INVALID = 255
} zap_npi_t;

typedef struct {
	char digits[25];
	uint8_t type;
	uint8_t plan;
} zap_number_t;

typedef enum {
	ZAP_CALLER_STATE_DIALING,
	ZAP_CALLER_STATE_SUCCESS,
	ZAP_CALLER_STATE_FAIL
} zap_caller_state_t;

struct zap_caller_data {
	char cid_date[8];
	char cid_name[80];
	zap_number_t cid_num;
	zap_number_t ani;
	zap_number_t dnis;
	zap_number_t rdnis;
	char aniII[25];
	uint8_t screen;
	uint8_t pres;
	char collected[25];
	int CRV;
	int hangup_cause;	
	char raw_data[1024];
	uint32_t raw_data_len;
	uint32_t flags;
	zap_caller_state_t call_state;
	uint32_t chan_id;
};

typedef enum {
	ZAP_TYPE_NONE,
	ZAP_TYPE_SPAN = 0xFF,
	ZAP_TYPE_CHANNEL
} zap_data_type_t;

#ifdef ZAP_DEBUG_DTMF
/* number of bytes for the circular buffer (5 seconds worth of audio) */
#define DTMF_DEBUG_SIZE 8 * 5000
/* number of 20ms cycles before timeout and close the debug dtmf file (5 seconds) */
#define DTMF_DEBUG_TIMEOUT 250
typedef struct {
	FILE *file;
	char buffer[DTMF_DEBUG_SIZE];
	int windex;
	int wrapped;
	int closetimeout;
	zap_mutex_t *mutex;
} zap_dtmf_debug_t;
#endif

/* 2^8 table size, one for each byte value */
#define ZAP_GAINS_TABLE_SIZE 256
struct zap_channel {
	zap_data_type_t data_type;
	uint32_t span_id;
	uint32_t chan_id;
	uint32_t physical_span_id;
	uint32_t physical_chan_id;
	uint32_t rate;
	uint32_t extra_id;
	zap_chan_type_t type;
	zap_socket_t sockfd;
	zap_channel_flag_t flags;
	uint32_t pflags;
	uint32_t sflags;
	zap_alarm_flag_t alarm_flags;
	zap_channel_feature_t features;
	zap_codec_t effective_codec;
	zap_codec_t native_codec;
	uint32_t effective_interval;
	uint32_t native_interval;
	uint32_t packet_len;
	zap_channel_state_t state;
	zap_channel_state_t last_state;
	zap_channel_state_t init_state;
	zap_mutex_t *mutex;
	teletone_dtmf_detect_state_t dtmf_detect;
	uint32_t buffer_delay;
	zap_event_t event_header;
	char last_error[256];
	zio_event_cb_t event_callback;
	uint32_t skip_read_frames;
	zap_buffer_t *dtmf_buffer;
	zap_buffer_t *gen_dtmf_buffer;
	zap_buffer_t *pre_buffer;
	zap_buffer_t *digit_buffer;
	zap_buffer_t *fsk_buffer;
	zap_mutex_t *pre_buffer_mutex;
	uint32_t dtmf_on;
	uint32_t dtmf_off;
	char *dtmf_hangup_buf;
	teletone_generation_session_t tone_session;
	zap_time_t last_event_time;
	zap_time_t ring_time;
	char tokens[ZAP_MAX_TOKENS+1][ZAP_TOKEN_STRLEN];
	uint8_t needed_tones[ZAP_TONEMAP_INVALID];
	uint8_t detected_tones[ZAP_TONEMAP_INVALID];
	zap_tonemap_t last_detected_tone;	
	uint32_t token_count;
	char chan_name[128];
	char chan_number[32];
	zap_filehandle_t fds[2];
	zap_fsk_data_state_t fsk;
	uint8_t fsk_buf[80];
	uint32_t ring_count;
	void *mod_data;
	void *call_data;
	struct zap_caller_data caller_data;
	struct zap_span *span;
	struct zap_io_interface *zio;
	zap_hash_t *variable_hash;
	unsigned char rx_cas_bits;
	uint32_t pre_buffer_size;
	unsigned char rxgain_table[ZAP_GAINS_TABLE_SIZE];
	unsigned char txgain_table[ZAP_GAINS_TABLE_SIZE];
	float rxgain;
	float txgain;
#ifdef ZAP_DEBUG_DTMF
	zap_dtmf_debug_t dtmfdbg;
#endif
};


struct zap_sigmsg {
	zap_signal_event_t event_id;
	uint32_t chan_id;
	uint32_t span_id;
	zap_channel_t *channel;
	void *raw_data;
	uint32_t raw_data_len;
};


struct zap_span {
	zap_data_type_t data_type;
	char *name;
	uint32_t span_id;
	uint32_t chan_count;
	zap_span_flag_t flags;
	struct zap_io_interface *zio;
	zio_event_cb_t event_callback;
	zap_mutex_t *mutex;
	zap_trunk_type_t trunk_type;
	zap_analog_start_type_t start_type;
	zap_signal_type_t signal_type;
	void *signal_data;
	zio_signal_cb_t signal_cb;
	zap_event_t event_header;
	char last_error[256];
	char tone_map[ZAP_TONEMAP_INVALID+1][ZAP_TONEMAP_LEN];
	teletone_tone_map_t tone_detect_map[ZAP_TONEMAP_INVALID+1];
	teletone_multi_tone_t tone_finder[ZAP_TONEMAP_INVALID+1];
	zap_channel_t *channels[ZAP_MAX_CHANNELS_SPAN+1];
	zio_channel_outgoing_call_t outgoing_call;
	zio_channel_request_t channel_request;
	zap_span_start_t start;
	zap_span_stop_t stop;
	void *mod_data;
	char *type;
	char *dtmf_hangup;
	size_t dtmf_hangup_len;
	int suggest_chan_id;
	zap_state_map_t *state_map;
	zap_caller_data_t default_caller_data;
	struct zap_span *next;
};


OZ_DECLARE_DATA extern zap_logger_t zap_log;

struct zap_io_interface {
	const char *name;
	zio_configure_span_t configure_span;
	zio_configure_t configure;
	zio_open_t open;
	zio_close_t close;
	zio_channel_destroy_t channel_destroy;
	zio_span_destroy_t span_destroy;
	zio_get_alarms_t get_alarms;
	zio_command_t command;
	zio_wait_t wait;
	zio_read_t read;
	zio_write_t write;
	zio_span_poll_event_t poll_event;
	zio_span_next_event_t next_event;
	zio_api_t api;
};


OZ_DECLARE(zap_size_t) zap_fsk_modulator_generate_bit(zap_fsk_modulator_t *fsk_trans, int8_t bit, int16_t *buf, zap_size_t buflen);
OZ_DECLARE(int32_t) zap_fsk_modulator_generate_carrier_bits(zap_fsk_modulator_t *fsk_trans, uint32_t bits);
OZ_DECLARE(void) zap_fsk_modulator_generate_chan_sieze(zap_fsk_modulator_t *fsk_trans);
OZ_DECLARE(void) zap_fsk_modulator_send_data(zap_fsk_modulator_t *fsk_trans);
#define zap_fsk_modulator_send_all(_it) zap_fsk_modulator_generate_chan_sieze(_it); \
	zap_fsk_modulator_generate_carrier_bits(_it, _it->carrier_bits_start); \
	zap_fsk_modulator_send_data(_it); \
	zap_fsk_modulator_generate_carrier_bits(_it, _it->carrier_bits_stop)

OZ_DECLARE(zap_status_t) zap_fsk_modulator_init(zap_fsk_modulator_t *fsk_trans,
									fsk_modem_types_t modem_type,
									uint32_t sample_rate,
									zap_fsk_data_state_t *fsk_data,
									float db_level,
									uint32_t carrier_bits_start,
									uint32_t carrier_bits_stop,
									uint32_t chan_sieze_bits,
									zap_fsk_write_sample_t write_sample_callback,
									void *user_data);
OZ_DECLARE(int8_t) zap_bitstream_get_bit(zap_bitstream_t *bsp);
OZ_DECLARE(void) zap_bitstream_init(zap_bitstream_t *bsp, uint8_t *data, uint32_t datalen, zap_endian_t endian, uint8_t ss);
OZ_DECLARE(zap_status_t) zap_fsk_data_parse(zap_fsk_data_state_t *state, zap_size_t *type, char **data, zap_size_t *len);
OZ_DECLARE(zap_status_t) zap_fsk_demod_feed(zap_fsk_data_state_t *state, int16_t *data, size_t samples);
OZ_DECLARE(zap_status_t) zap_fsk_demod_destroy(zap_fsk_data_state_t *state);
OZ_DECLARE(int) zap_fsk_demod_init(zap_fsk_data_state_t *state, int rate, uint8_t *buf, size_t bufsize);
OZ_DECLARE(zap_status_t) zap_fsk_data_init(zap_fsk_data_state_t *state, uint8_t *data, uint32_t datalen);
OZ_DECLARE(zap_status_t) zap_fsk_data_add_mdmf(zap_fsk_data_state_t *state, zap_mdmf_type_t type, const uint8_t *data, uint32_t datalen);
OZ_DECLARE(zap_status_t) zap_fsk_data_add_checksum(zap_fsk_data_state_t *state);
OZ_DECLARE(zap_status_t) zap_fsk_data_add_sdmf(zap_fsk_data_state_t *state, const char *date, char *number);
OZ_DECLARE(zap_status_t) zap_channel_outgoing_call(zap_channel_t *zchan);
OZ_DECLARE(void) zap_channel_rotate_tokens(zap_channel_t *zchan);
OZ_DECLARE(void) zap_channel_clear_detected_tones(zap_channel_t *zchan);
OZ_DECLARE(void) zap_channel_clear_needed_tones(zap_channel_t *zchan);
OZ_DECLARE(zap_status_t) zap_channel_get_alarms(zap_channel_t *zchan);
OZ_DECLARE(zap_status_t) zap_channel_send_fsk_data(zap_channel_t *zchan, zap_fsk_data_state_t *fsk_data, float db_level);
OZ_DECLARE(zap_status_t) zap_channel_clear_token(zap_channel_t *zchan, const char *token);
OZ_DECLARE(void) zap_channel_replace_token(zap_channel_t *zchan, const char *old_token, const char *new_token);
OZ_DECLARE(zap_status_t) zap_channel_add_token(zap_channel_t *zchan, char *token, int end);
OZ_DECLARE(zap_status_t) zap_channel_set_state(zap_channel_t *zchan, zap_channel_state_t state, int lock);
OZ_DECLARE(zap_status_t) zap_span_load_tones(zap_span_t *span, const char *mapname);
OZ_DECLARE(zap_size_t) zap_channel_dequeue_dtmf(zap_channel_t *zchan, char *dtmf, zap_size_t len);
OZ_DECLARE(zap_status_t) zap_channel_queue_dtmf(zap_channel_t *zchan, const char *dtmf);
OZ_DECLARE(void) zap_channel_flush_dtmf(zap_channel_t *zchan);
OZ_DECLARE(zap_time_t) zap_current_time_in_ms(void);
OZ_DECLARE(zap_status_t) zap_span_poll_event(zap_span_t *span, uint32_t ms);
OZ_DECLARE(zap_status_t) zap_span_next_event(zap_span_t *span, zap_event_t **event);
OZ_DECLARE(zap_status_t) zap_span_find(uint32_t id, zap_span_t **span);
OZ_DECLARE(zap_status_t) zap_span_create(zap_io_interface_t *zio, zap_span_t **span, const char *name);
OZ_DECLARE(zap_status_t) zap_span_close_all(void);
OZ_DECLARE(zap_status_t) zap_span_add_channel(zap_span_t *span, zap_socket_t sockfd, zap_chan_type_t type, zap_channel_t **chan);
OZ_DECLARE(zap_status_t) zap_span_set_event_callback(zap_span_t *span, zio_event_cb_t event_callback);
OZ_DECLARE(zap_status_t) zap_channel_set_event_callback(zap_channel_t *zchan, zio_event_cb_t event_callback);
OZ_DECLARE(zap_status_t) zap_channel_open(uint32_t span_id, uint32_t chan_id, zap_channel_t **zchan);
OZ_DECLARE(zap_status_t) zap_channel_open_chan(zap_channel_t *zchan);
OZ_DECLARE(zap_status_t) zap_span_channel_use_count(zap_span_t *span, uint32_t *count);
OZ_DECLARE(zap_status_t) zap_channel_open_any(uint32_t span_id, zap_direction_t direction, zap_caller_data_t *caller_data, zap_channel_t **zchan);
OZ_DECLARE(zap_status_t) zap_channel_close(zap_channel_t **zchan);
OZ_DECLARE(zap_status_t) zap_channel_done(zap_channel_t *zchan);
OZ_DECLARE(zap_status_t) zap_channel_use(zap_channel_t *zchan);
OZ_DECLARE(zap_status_t) zap_channel_command(zap_channel_t *zchan, zap_command_t command, void *obj);
OZ_DECLARE(zap_status_t) zap_channel_wait(zap_channel_t *zchan, zap_wait_flag_t *flags, int32_t to);
OZ_DECLARE(zap_status_t) zap_channel_read(zap_channel_t *zchan, void *data, zap_size_t *datalen);
OZ_DECLARE(void) zap_generate_sln_silence(int16_t *data, uint32_t samples, uint32_t divisor);
OZ_DECLARE(zap_status_t) zap_channel_write(zap_channel_t *zchan, void *data, zap_size_t datasize, zap_size_t *datalen);
OZ_DECLARE(zap_status_t) zap_channel_add_var(zap_channel_t *zchan, const char *var_name, const char *value);
OZ_DECLARE(const char *) zap_channel_get_var(zap_channel_t *zchan, const char *var_name);
OZ_DECLARE(zap_status_t) zap_channel_clear_vars(zap_channel_t *zchan);
OZ_DECLARE(zap_status_t) zap_global_init(void);
OZ_DECLARE(zap_status_t) zap_global_destroy(void);
OZ_DECLARE(void) zap_global_set_logger(zap_logger_t logger);
OZ_DECLARE(void) zap_global_set_default_logger(int level);
OZ_DECLARE(uint32_t) zap_separate_string(char *buf, char delim, char **array, int arraylen);
OZ_DECLARE(void) print_bits(uint8_t *b, int bl, char *buf, int blen, int e, uint8_t ss);
OZ_DECLARE(void) print_hex_bytes(uint8_t *data, zap_size_t dlen, char *buf, zap_size_t blen);
OZ_DECLARE_NONSTD(int) zap_hash_equalkeys(void *k1, void *k2);
OZ_DECLARE_NONSTD(uint32_t) zap_hash_hashfromstring(void *ky);
OZ_DECLARE(uint32_t) zap_running(void);
OZ_DECLARE(zap_status_t) zap_channel_complete_state(zap_channel_t *zchan);
OZ_DECLARE(zap_status_t) zap_channel_init(zap_channel_t *zchan);
OZ_DECLARE(int) zap_load_modules(void);
OZ_DECLARE(zap_status_t) zap_unload_modules(void);
OZ_DECLARE(zap_status_t) zap_configure_span(const char *type, zap_span_t *span, zio_signal_cb_t sig_cb, ...);
OZ_DECLARE(zap_status_t) zap_span_start(zap_span_t *span);
OZ_DECLARE(zap_status_t) zap_span_stop(zap_span_t *span);
OZ_DECLARE(zap_status_t) zap_span_send_signal(zap_span_t *span, zap_sigmsg_t *sigmsg);
OZ_DECLARE(int) zap_load_module(const char *name);
OZ_DECLARE(int) zap_load_module_assume(const char *name);
OZ_DECLARE(zap_status_t) zap_span_find_by_name(const char *name, zap_span_t **span);
OZ_DECLARE(char *) zap_api_execute(const char *type, const char *cmd);
OZ_DECLARE(int) zap_vasprintf(char **ret, const char *fmt, va_list ap);
OZ_DECLARE(zap_status_t) zap_channel_set_caller_data(zap_channel_t *zchan, zap_caller_data_t *caller_data);
OZ_DECLARE(void) zap_cpu_monitor_disable(void);

ZIO_CODEC_FUNCTION(zio_slin2ulaw);
ZIO_CODEC_FUNCTION(zio_ulaw2slin);
ZIO_CODEC_FUNCTION(zio_slin2alaw);
ZIO_CODEC_FUNCTION(zio_alaw2slin);
ZIO_CODEC_FUNCTION(zio_ulaw2alaw);
ZIO_CODEC_FUNCTION(zio_alaw2ulaw);

#ifdef DEBUG_LOCKS
#define zap_mutex_lock(_x) printf("++++++lock %s:%d\n", __FILE__, __LINE__) && _zap_mutex_lock(_x)
#define zap_mutex_trylock(_x) printf("++++++try %s:%d\n", __FILE__, __LINE__) && _zap_mutex_trylock(_x)
#define zap_mutex_unlock(_x) printf("------unlock %s:%d\n", __FILE__, __LINE__) && _zap_mutex_unlock(_x)
#else 
#define zap_mutex_lock(_x) _zap_mutex_lock(_x)
#define zap_mutex_trylock(_x) _zap_mutex_trylock(_x)
#define zap_mutex_unlock(_x) _zap_mutex_unlock(_x)
#endif


static __inline__ void zap_set_state_all(zap_span_t *span, zap_channel_state_t state)
{
	uint32_t j;
	zap_mutex_lock(span->mutex);
	for(j = 1; j <= span->chan_count; j++) {
		zap_set_state_locked((span->channels[j]), state);
	}
	zap_mutex_unlock(span->mutex);
}

static __inline__ int zap_check_state_all(zap_span_t *span, zap_channel_state_t state)
{
	uint32_t j;
	for(j = 1; j <= span->chan_count; j++) {
		if (span->channels[j]->state != state || zap_test_flag(span->channels[j], ZAP_CHANNEL_STATE_CHANGE)) {
			return 0;
		}
	}

	return 1;
}

static __inline__ void zap_set_flag_all(zap_span_t *span, uint32_t flag)
{
	uint32_t j;
	zap_mutex_lock(span->mutex);
	for(j = 1; j <= span->chan_count; j++) {
		zap_set_flag_locked((span->channels[j]), flag);
	}
	zap_mutex_unlock(span->mutex);
}

static __inline__ void zap_clear_flag_all(zap_span_t *span, uint32_t flag)
{
	uint32_t j;
	zap_mutex_lock(span->mutex);
	for(j = 1; j <= span->chan_count; j++) {
		zap_clear_flag_locked((span->channels[j]), flag);
	}
	zap_mutex_unlock(span->mutex);
}

#endif

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
