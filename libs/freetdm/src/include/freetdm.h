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
#if defined(FT_DECLARE_STATIC)
#define FT_DECLARE(type)			type __stdcall
#define FT_DECLARE_NONSTD(type)		type __cdecl
#define FT_DECLARE_DATA
#elif defined(FREETDM_EXPORTS)
#define FT_DECLARE(type)			__declspec(dllexport) type __stdcall
#define FT_DECLARE_NONSTD(type)		__declspec(dllexport) type __cdecl
#define FT_DECLARE_DATA				__declspec(dllexport)
#else
#define FT_DECLARE(type)			__declspec(dllimport) type __stdcall
#define FT_DECLARE_NONSTD(type)		__declspec(dllimport) type __cdecl
#define FT_DECLARE_DATA				__declspec(dllimport)
#endif
#define EX_DECLARE_DATA				__declspec(dllexport)
#else
#if (defined(__GNUC__) || defined(__SUNPRO_CC) || defined (__SUNPRO_C)) && defined(HAVE_VISIBILITY)
#define FT_DECLARE(type)		__attribute__((visibility("default"))) type
#define FT_DECLARE_NONSTD(type)	__attribute__((visibility("default"))) type
#define FT_DECLARE_DATA		__attribute__((visibility("default")))
#else
#define FT_DECLARE(type)		type
#define FT_DECLARE_NONSTD(type)	type
#define FT_DECLARE_DATA
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

#define FTDM_THREAD_STACKSIZE 240 * 1024
#define FTDM_ENUM_NAMES(_NAME, _STRINGS) static const char * _NAME [] = { _STRINGS , NULL };
#define FTDM_STR2ENUM_P(_FUNC1, _FUNC2, _TYPE) FT_DECLARE(_TYPE) _FUNC1 (const char *name); FT_DECLARE(const char *) _FUNC2 (_TYPE type);
#define FTDM_STR2ENUM(_FUNC1, _FUNC2, _TYPE, _STRINGS, _MAX)	\
	FT_DECLARE(_TYPE) _FUNC1 (const char *name)							\
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
	FT_DECLARE(const char *) _FUNC2 (_TYPE type)						\
	{														\
		if (type > _MAX) {									\
			type = _MAX;									\
		}													\
		return _STRINGS[(int)type];							\
	}														\
	
#define ftdm_true(expr)							\
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
#include "ftdm_types.h"
#include "hashtable.h"
#include "ftdm_config.h"
#include "g711.h"
#include "libteletone.h"
#include "ftdm_buffer.h"
#include "ftdm_threadmutex.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __WINDOWS__
#define ftdm_sleep(x) Sleep(x)
#else
#define ftdm_sleep(x) usleep(x * 1000)
#endif

#ifdef NDEBUG
#undef assert
#define assert(_Expression)     ((void)(_Expression))
#endif

#define FTDM_MAX_CHANNELS_PHYSICAL_SPAN 32
#define FTDM_MAX_PHYSICAL_SPANS_PER_LOGICAL_SPAN 32
#define FTDM_MAX_CHANNELS_SPAN FTDM_MAX_CHANNELS_PHYSICAL_SPAN * FTDM_MAX_PHYSICAL_SPANS_PER_LOGICAL_SPAN
#define FTDM_MAX_SPANS_INTERFACE 128

#define FTDM_MAX_CHANNELS_GROUP 1024
#define FTDM_MAX_GROUPS_INTERFACE FTDM_MAX_SPANS_INTERFACE

#define GOTO_STATUS(label,st) status = st; goto label ;

#define ftdm_copy_string(x,y,z) strncpy(x, y, z - 1) 
#define ftdm_set_string(x,y) strncpy(x, y, sizeof(x)-1) 
#define ftdm_strlen_zero(s) (!s || *s == '\0')
#define ftdm_strlen_zero_buf(s) (*s == '\0')


#define ftdm_channel_test_feature(obj, flag) ((obj)->features & flag)
#define ftdm_channel_set_feature(obj, flag) (obj)->features |= (flag)
#define ftdm_channel_clear_feature(obj, flag) (obj)->features &= ~(flag)
#define ftdm_channel_set_member_locked(obj, _m, _v) ftdm_mutex_lock(obj->mutex); obj->_m = _v; ftdm_mutex_unlock(obj->mutex)

/*!
  \brief Test for the existance of a flag on an arbitary object
  \command obj the object to test
  \command flag the or'd list of flags to test
  \return true value if the object has the flags defined
*/
#define ftdm_test_flag(obj, flag) ((obj)->flags & flag)
#define ftdm_test_pflag(obj, flag) ((obj)->pflags & flag)
#define ftdm_test_sflag(obj, flag) ((obj)->sflags & flag)


#define ftdm_set_alarm_flag(obj, flag) (obj)->alarm_flags |= (flag)
#define ftdm_clear_alarm_flag(obj, flag) (obj)->alarm_flags &= ~(flag)
#define ftdm_test_alarm_flag(obj, flag) ((obj)->alarm_flags & flag)

/*!
  \brief Set a flag on an arbitrary object
  \command obj the object to set the flags on
  \command flag the or'd list of flags to set
*/
#define ftdm_set_flag(obj, flag) (obj)->flags |= (flag)
#define ftdm_set_flag_locked(obj, flag) assert(obj->mutex != NULL);	\
	ftdm_mutex_lock(obj->mutex);										\
	(obj)->flags |= (flag);          	   	                        \
	ftdm_mutex_unlock(obj->mutex);

#define ftdm_set_pflag(obj, flag) (obj)->pflags |= (flag)
#define ftdm_set_pflag_locked(obj, flag) assert(obj->mutex != NULL);	\
	ftdm_mutex_lock(obj->mutex);										\
	(obj)->pflags |= (flag);											\
	ftdm_mutex_unlock(obj->mutex);

#define ftdm_set_sflag(obj, flag) (obj)->sflags |= (flag)
#define ftdm_set_sflag_locked(obj, flag) assert(obj->mutex != NULL);	\
	ftdm_mutex_lock(obj->mutex);										\
	(obj)->sflags |= (flag);											\
	ftdm_mutex_unlock(obj->mutex);

/*!
  \brief Clear a flag on an arbitrary object while locked
  \command obj the object to test
  \command flag the or'd list of flags to clear
*/
#define ftdm_clear_flag(obj, flag) (obj)->flags &= ~(flag)

#define ftdm_clear_flag_locked(obj, flag) assert(obj->mutex != NULL); ftdm_mutex_lock(obj->mutex); (obj)->flags &= ~(flag); ftdm_mutex_unlock(obj->mutex);

#define ftdm_clear_pflag(obj, flag) (obj)->pflags &= ~(flag)

#define ftdm_clear_pflag_locked(obj, flag) assert(obj->mutex != NULL); ftdm_mutex_lock(obj->mutex); (obj)->pflags &= ~(flag); ftdm_mutex_unlock(obj->mutex);

#define ftdm_clear_sflag(obj, flag) (obj)->sflags &= ~(flag)

#define ftdm_clear_sflag_locked(obj, flag) assert(obj->mutex != NULL); ftdm_mutex_lock(obj->mutex); (obj)->sflags &= ~(flag); ftdm_mutex_unlock(obj->mutex);


#define ftdm_set_state_locked(obj, s) if ( obj->state == s ) {			\
		ftdm_log(FTDM_LOG_WARNING, "Why bother changing state on %d:%d from %s to %s\n", obj->span_id, obj->chan_id, ftdm_channel_state2str(obj->state), ftdm_channel_state2str(s)); \
	} else if (ftdm_test_flag(obj, FTDM_CHANNEL_READY)) {									\
		ftdm_channel_state_t st = obj->state;											\
		ftdm_channel_set_state(obj, s, 1);									\
		if (obj->state == s) ftdm_log(FTDM_LOG_DEBUG, "Changing state on %d:%d from %s to %s\n", obj->span_id, obj->chan_id, ftdm_channel_state2str(st), ftdm_channel_state2str(s)); \
		else ftdm_log(FTDM_LOG_WARNING, "VETO Changing state on %d:%d from %s to %s\n", obj->span_id, obj->chan_id, ftdm_channel_state2str(st), ftdm_channel_state2str(s)); \
	}

#define ftdm_set_state(obj, s) if ( obj->state == s ) {			\
		ftdm_log(FTDM_LOG_WARNING, "Why bother changing state on %d:%d from %s to %s\n", obj->span_id, obj->chan_id, ftdm_channel_state2str(obj->state), ftdm_channel_state2str(s)); \
	} else if (ftdm_test_flag(obj, FTDM_CHANNEL_READY)) {									\
		ftdm_channel_state_t st = obj->state;											\
		ftdm_channel_set_state(obj, s, 0);									\
		if (obj->state == s) ftdm_log(FTDM_LOG_DEBUG, "Changing state on %d:%d from %s to %s\n", obj->span_id, obj->chan_id, ftdm_channel_state2str(st), ftdm_channel_state2str(s)); \
		else ftdm_log(FTDM_LOG_WARNING, "VETO Changing state on %d:%d from %s to %s\n", obj->span_id, obj->chan_id, ftdm_channel_state2str(st), ftdm_channel_state2str(s)); \
	}

#ifdef _MSC_VER
/* The while(0) below throws a conditional expression is constant warning */
#pragma warning(disable:4127) 
#endif

#define ftdm_set_state_locked_wait(obj, s) 						\
	do {										\
		int __safety = 100;							\
		ftdm_set_state_locked(obj, s);						\
		while(__safety-- && ftdm_test_flag(obj, FTDM_CHANNEL_STATE_CHANGE)) {	\
			ftdm_sleep(10);							\
		}									\
		if(!__safety) {								\
			ftdm_log(FTDM_LOG_CRIT, "State change not completed\n");		\
		}									\
	} while(0);

#define ftdm_wait_for_flag_cleared(obj, flag, time) 					\
	do {										\
		int __safety = time;							\
		while(__safety-- && ftdm_test_flag(obj, flag)) { 			\
			ftdm_mutex_unlock(obj->mutex);					\
			ftdm_sleep(10);							\
			ftdm_mutex_lock(obj->mutex);					\
		}									\
		if(!__safety) {								\
			ftdm_log(FTDM_LOG_CRIT, "flag %d was never cleared\n", flag);	\
		}									\
	} while(0);

#define ftdm_set_state_wait(obj, s) 						\
	do {										\
		ftdm_channel_set_state(obj, s, 0);					\
		ftdm_wait_for_flag_cleared(obj, FTDM_CHANNEL_STATE_CHANGE, 100);     \
	} while(0);


typedef enum {
	FTDM_STATE_CHANGE_FAIL,
	FTDM_STATE_CHANGE_SUCCESS,
	FTDM_STATE_CHANGE_SAME,
} ftdm_state_change_result_t;

#define ftdm_set_state_r(obj, s, l, r) if ( obj->state == s ) {	\
		if (s != FTDM_CHANNEL_STATE_HANGUP) ftdm_log(FTDM_LOG_WARNING, "Why bother changing state on %d:%d from %s to %s\n", obj->span_id, obj->chan_id, ftdm_channel_state2str(obj->state), ftdm_channel_state2str(s)); r = FTDM_STATE_CHANGE_SAME; \
	} else if (ftdm_test_flag(obj, FTDM_CHANNEL_READY)) {					\
		int st = obj->state;											\
		r = (ftdm_channel_set_state(obj, s, l) == FTDM_SUCCESS) ? FTDM_STATE_CHANGE_SUCCESS : FTDM_STATE_CHANGE_FAIL; \
		if (obj->state == s) {ftdm_log(FTDM_LOG_DEBUG, "Changing state on %d:%d from %s to %s\n", obj->span_id, obj->chan_id, ftdm_channel_state2str(st), ftdm_channel_state2str(s));} \
		else ftdm_log(FTDM_LOG_WARNING, "VETO Changing state on %d:%d from %s to %s\n", obj->span_id, obj->chan_id, ftdm_channel_state2str(st), ftdm_channel_state2str(s)); \
	}


#define ftdm_is_dtmf(key)  ((key > 47 && key < 58) || (key > 64 && key < 69) || (key > 96 && key < 101) || key == 35 || key == 42 || key == 87 || key == 119)

/*!
  \brief Copy flags from one arbitrary object to another
  \command dest the object to copy the flags to
  \command src the object to copy the flags from
  \command flags the flags to copy
*/
#define ftdm_copy_flags(dest, src, flags) (dest)->flags &= ~(flags);	(dest)->flags |= ((src)->flags & (flags))

struct ftdm_stream_handle {
	ftdm_stream_handle_write_function_t write_function;
	ftdm_stream_handle_raw_write_function_t raw_write_function;
	void *data;
	void *end;
	ftdm_size_t data_size;
	ftdm_size_t data_len;
	ftdm_size_t alloc_len;
	ftdm_size_t alloc_chunk;
};


FT_DECLARE_NONSTD(ftdm_status_t) ftdm_console_stream_raw_write(ftdm_stream_handle_t *handle, uint8_t *data, ftdm_size_t datalen);
FT_DECLARE_NONSTD(ftdm_status_t) ftdm_console_stream_write(ftdm_stream_handle_t *handle, const char *fmt, ...);

#define FTDM_CMD_CHUNK_LEN 1024
#define FTDM_STANDARD_STREAM(s) memset(&s, 0, sizeof(s)); s.data = malloc(FTDM_CMD_CHUNK_LEN); \
	assert(s.data);														\
	memset(s.data, 0, FTDM_CMD_CHUNK_LEN);								\
	s.end = s.data;														\
	s.data_size = FTDM_CMD_CHUNK_LEN;									\
	s.write_function = ftdm_console_stream_write;						\
	s.raw_write_function = ftdm_console_stream_raw_write;				\
	s.alloc_len = FTDM_CMD_CHUNK_LEN;									\
	s.alloc_chunk = FTDM_CMD_CHUNK_LEN

struct ftdm_event {
	ftdm_event_type_t e_type;
	uint32_t enum_id;
	ftdm_channel_t *channel;
	void *data;
};

typedef struct ftdm_queue ftdm_queue_t;
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
FT_DECLARE_DATA extern ftdm_queue_handler_t g_ftdm_queue_handler;

/*! brief create a new queue */
#define ftdm_queue_create(queue, capacity) g_ftdm_queue_handler.create(queue, capacity)

/*! Enqueue an object */
#define ftdm_queue_enqueue(queue, obj) g_ftdm_queue_handler.enqueue(queue, obj)

/*! dequeue an object from the queue */
#define ftdm_queue_dequeue(queue) g_ftdm_queue_handler.dequeue(queue)

/*! wait ms milliseconds for a queue to have available objects, -1 to wait forever */
#define ftdm_queue_wait(queue, ms) g_ftdm_queue_handler.wait(queue, ms)

/*! get the internal interrupt object (to wait for elements to be added from the outside bypassing ftdm_queue_wait) */
#define ftdm_queue_get_interrupt(queue, ms) g_ftdm_queue_handler.get_interrupt(queue, ms)

/*! destroy the queue */ 
#define ftdm_queue_destroy(queue) g_ftdm_queue_handler.destroy(queue)


#define FTDM_TOKEN_STRLEN 128
#define FTDM_MAX_TOKENS 10

static __inline__ char *ftdm_clean_string(char *s)
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

struct ftdm_bitstream {
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

struct ftdm_fsk_data_state {
	dsp_fsk_handle_t *fsk1200_handle;
	uint8_t init;
	uint8_t *buf;
	size_t bufsize;
	ftdm_size_t blen;
	ftdm_size_t bpos;
	ftdm_size_t dlen;
	ftdm_size_t ppos;
	int checksum;
};

struct ftdm_fsk_modulator {
	teletone_dds_state_t dds;
	ftdm_bitstream_t bs;
	uint32_t carrier_bits_start;
	uint32_t carrier_bits_stop;
	uint32_t chan_sieze_bits;
	uint32_t bit_factor;
	uint32_t bit_accum;
	uint32_t sample_counter;
	int32_t samples_per_bit;
	int32_t est_bytes;
	fsk_modem_types_t modem_type;
	ftdm_fsk_data_state_t *fsk_data;
	ftdm_fsk_write_sample_t write_sample_callback;
	void *user_data;
	int16_t sample_buffer[64];
};

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

struct ftdm_caller_data {
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
};

typedef enum {
	FTDM_TYPE_NONE,
	FTDM_TYPE_SPAN = 0xFF,
	FTDM_TYPE_CHANNEL
} ftdm_data_type_t;

/* 2^8 table size, one for each byte value */
#define FTDM_GAINS_TABLE_SIZE 256
struct ftdm_channel {
	ftdm_data_type_t data_type;
	uint32_t span_id;
	uint32_t chan_id;
	uint32_t physical_span_id;
	uint32_t physical_chan_id;
	uint32_t rate;
	uint32_t extra_id;
	ftdm_chan_type_t type;
	ftdm_socket_t sockfd;
	uint32_t flags;
	uint32_t pflags;
	uint32_t sflags;
	ftdm_alarm_flag_t alarm_flags;
	ftdm_channel_feature_t features;
	ftdm_codec_t effective_codec;
	ftdm_codec_t native_codec;
	uint32_t effective_interval;
	uint32_t native_interval;
	uint32_t packet_len;
	ftdm_channel_state_t state;
	ftdm_channel_state_t last_state;
	ftdm_channel_state_t init_state;
	ftdm_mutex_t *mutex;
	teletone_dtmf_detect_state_t dtmf_detect;
	uint32_t buffer_delay;
	ftdm_event_t event_header;
	char last_error[256];
	fio_event_cb_t event_callback;
	uint32_t skip_read_frames;
	ftdm_buffer_t *dtmf_buffer;
	ftdm_buffer_t *gen_dtmf_buffer;
	ftdm_buffer_t *pre_buffer;
	ftdm_buffer_t *digit_buffer;
	ftdm_buffer_t *fsk_buffer;
	ftdm_mutex_t *pre_buffer_mutex;
	uint32_t dtmf_on;
	uint32_t dtmf_off;
	char *dtmf_hangup_buf;
	teletone_generation_session_t tone_session;
	ftdm_time_t last_event_time;
	ftdm_time_t ring_time;
	char tokens[FTDM_MAX_TOKENS+1][FTDM_TOKEN_STRLEN];
	uint8_t needed_tones[FTDM_TONEMAP_INVALID];
	uint8_t detected_tones[FTDM_TONEMAP_INVALID];
	ftdm_tonemap_t last_detected_tone;	
	uint32_t token_count;
	char chan_name[128];
	char chan_number[32];
	ftdm_filehandle_t fds[2];
	ftdm_fsk_data_state_t fsk;
	uint8_t fsk_buf[80];
	uint32_t ring_count;
	void *mod_data;
	void *call_data;
	struct ftdm_caller_data caller_data;
	struct ftdm_span *span;
	struct ftdm_io_interface *fio;
	ftdm_hash_t *variable_hash;
	unsigned char rx_cas_bits;
	uint32_t pre_buffer_size;
	uint8_t rxgain_table[FTDM_GAINS_TABLE_SIZE];
	uint8_t txgain_table[FTDM_GAINS_TABLE_SIZE];
	float rxgain;
	float txgain;
};


struct ftdm_sigmsg {
	ftdm_signal_event_t event_id;
	uint32_t chan_id;
	uint32_t span_id;
	ftdm_channel_t *channel;
	void *raw_data;
	uint32_t raw_data_len;
};


struct ftdm_span {
	ftdm_data_type_t data_type;
	char *name;
	uint32_t span_id;
	uint32_t chan_count;
	ftdm_span_flag_t flags;
	struct ftdm_io_interface *fio;
	fio_event_cb_t event_callback;
	ftdm_mutex_t *mutex;
	ftdm_trunk_type_t trunk_type;
	ftdm_analog_start_type_t start_type;
	ftdm_signal_type_t signal_type;
	void *signal_data;
	fio_signal_cb_t signal_cb;
	ftdm_event_t event_header;
	char last_error[256];
	char tone_map[FTDM_TONEMAP_INVALID+1][FTDM_TONEMAP_LEN];
	teletone_tone_map_t tone_detect_map[FTDM_TONEMAP_INVALID+1];
	teletone_multi_tone_t tone_finder[FTDM_TONEMAP_INVALID+1];
	ftdm_channel_t *channels[FTDM_MAX_CHANNELS_SPAN+1];
	fio_channel_outgoing_call_t outgoing_call;
	fio_channel_set_sig_status_t set_channel_sig_status;
	fio_channel_get_sig_status_t get_channel_sig_status;
	fio_span_set_sig_status_t set_span_sig_status;
	fio_span_get_sig_status_t get_span_sig_status;
	fio_channel_request_t channel_request;
	ftdm_span_start_t start;
	ftdm_span_stop_t stop;
	void *mod_data;
	char *type;
	char *dtmf_hangup;
	size_t dtmf_hangup_len;
	ftdm_state_map_t *state_map;
	ftdm_caller_data_t default_caller_data;
	ftdm_queue_t *pendingchans;
	struct ftdm_span *next;
};

struct ftdm_group {
	char *name;
	uint32_t group_id;
	uint32_t chan_count;
	ftdm_channel_t *channels[FTDM_MAX_CHANNELS_GROUP];
	uint32_t last_used_index;
	ftdm_mutex_t *mutex;
	struct ftdm_group *next;
};

FT_DECLARE_DATA extern ftdm_logger_t ftdm_log;

typedef enum {
	FTDM_CRASH_NEVER = 0,
	FTDM_CRASH_ON_ASSERT
} ftdm_crash_policy_t;

FT_DECLARE_DATA extern ftdm_crash_policy_t g_ftdm_crash_policy;

typedef void *(*ftdm_malloc_func_t)(void *pool, ftdm_size_t len);
typedef void *(*ftdm_calloc_func_t)(void *pool, ftdm_size_t elements, ftdm_size_t len);
typedef void (*ftdm_free_func_t)(void *pool, void *ptr);
typedef struct ftdm_memory_handler {
	void *pool;
	ftdm_malloc_func_t malloc;
	ftdm_calloc_func_t calloc;
	ftdm_free_func_t free;
} ftdm_memory_handler_t;

FT_DECLARE_DATA extern ftdm_memory_handler_t g_ftdm_mem_handler;

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

/*! \brief Override the default queue handler */
FT_DECLARE(ftdm_status_t) ftdm_global_set_queue_handler(ftdm_queue_handler_t *handler);

/*! \brief Duplicate string */
FT_DECLARE(char *) ftdm_strdup(const char *str);
FT_DECLARE(char *) ftdm_strndup(const char *str, ftdm_size_t inlen);

FT_DECLARE(ftdm_size_t) ftdm_fsk_modulator_generate_bit(ftdm_fsk_modulator_t *fsk_trans, int8_t bit, int16_t *buf, ftdm_size_t buflen);
FT_DECLARE(int32_t) ftdm_fsk_modulator_generate_carrier_bits(ftdm_fsk_modulator_t *fsk_trans, uint32_t bits);
FT_DECLARE(void) ftdm_fsk_modulator_generate_chan_sieze(ftdm_fsk_modulator_t *fsk_trans);
FT_DECLARE(void) ftdm_fsk_modulator_send_data(ftdm_fsk_modulator_t *fsk_trans);
#define ftdm_fsk_modulator_send_all(_it) ftdm_fsk_modulator_generate_chan_sieze(_it); \
	ftdm_fsk_modulator_generate_carrier_bits(_it, _it->carrier_bits_start); \
	ftdm_fsk_modulator_send_data(_it); \
	ftdm_fsk_modulator_generate_carrier_bits(_it, _it->carrier_bits_stop)

FT_DECLARE(ftdm_status_t) ftdm_fsk_modulator_init(ftdm_fsk_modulator_t *fsk_trans,
									fsk_modem_types_t modem_type,
									uint32_t sample_rate,
									ftdm_fsk_data_state_t *fsk_data,
									float db_level,
									uint32_t carrier_bits_start,
									uint32_t carrier_bits_stop,
									uint32_t chan_sieze_bits,
									ftdm_fsk_write_sample_t write_sample_callback,
									void *user_data);
FT_DECLARE(int8_t) ftdm_bitstream_get_bit(ftdm_bitstream_t *bsp);
FT_DECLARE(void) ftdm_bitstream_init(ftdm_bitstream_t *bsp, uint8_t *data, uint32_t datalen, ftdm_endian_t endian, uint8_t ss);
FT_DECLARE(ftdm_status_t) ftdm_fsk_data_parse(ftdm_fsk_data_state_t *state, ftdm_size_t *type, char **data, ftdm_size_t *len);
FT_DECLARE(ftdm_status_t) ftdm_fsk_demod_feed(ftdm_fsk_data_state_t *state, int16_t *data, size_t samples);
FT_DECLARE(ftdm_status_t) ftdm_fsk_demod_destroy(ftdm_fsk_data_state_t *state);
FT_DECLARE(int) ftdm_fsk_demod_init(ftdm_fsk_data_state_t *state, int rate, uint8_t *buf, size_t bufsize);
FT_DECLARE(ftdm_status_t) ftdm_fsk_data_init(ftdm_fsk_data_state_t *state, uint8_t *data, uint32_t datalen);
FT_DECLARE(ftdm_status_t) ftdm_fsk_data_add_mdmf(ftdm_fsk_data_state_t *state, ftdm_mdmf_type_t type, const uint8_t *data, uint32_t datalen);
FT_DECLARE(ftdm_status_t) ftdm_fsk_data_add_checksum(ftdm_fsk_data_state_t *state);
FT_DECLARE(ftdm_status_t) ftdm_fsk_data_add_sdmf(ftdm_fsk_data_state_t *state, const char *date, char *number);
FT_DECLARE(ftdm_status_t) ftdm_channel_outgoing_call(ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_channel_set_sig_status(ftdm_channel_t *ftdmchan, ftdm_signaling_status_t status);
FT_DECLARE(ftdm_status_t) ftdm_channel_get_sig_status(ftdm_channel_t *ftdmchan, ftdm_signaling_status_t *status);
FT_DECLARE(ftdm_status_t) ftdm_span_set_sig_status(ftdm_span_t *span, ftdm_signaling_status_t status);
FT_DECLARE(ftdm_status_t) ftdm_span_get_sig_status(ftdm_span_t *span, ftdm_signaling_status_t *status);
FT_DECLARE(void) ftdm_channel_rotate_tokens(ftdm_channel_t *ftdmchan);
FT_DECLARE(void) ftdm_channel_clear_detected_tones(ftdm_channel_t *ftdmchan);
FT_DECLARE(void) ftdm_channel_clear_needed_tones(ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_channel_get_alarms(ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_channel_send_fsk_data(ftdm_channel_t *ftdmchan, ftdm_fsk_data_state_t *fsk_data, float db_level);
FT_DECLARE(ftdm_status_t) ftdm_channel_clear_token(ftdm_channel_t *ftdmchan, const char *token);
FT_DECLARE(void) ftdm_channel_replace_token(ftdm_channel_t *ftdmchan, const char *old_token, const char *new_token);
FT_DECLARE(ftdm_status_t) ftdm_channel_add_token(ftdm_channel_t *ftdmchan, char *token, int end);
FT_DECLARE(ftdm_status_t) ftdm_channel_set_state(ftdm_channel_t *ftdmchan, ftdm_channel_state_t state, int lock);
FT_DECLARE(ftdm_status_t) ftdm_span_load_tones(ftdm_span_t *span, const char *mapname);
FT_DECLARE(ftdm_size_t) ftdm_channel_dequeue_dtmf(ftdm_channel_t *ftdmchan, char *dtmf, ftdm_size_t len);
FT_DECLARE(ftdm_status_t) ftdm_channel_queue_dtmf(ftdm_channel_t *ftdmchan, const char *dtmf);
FT_DECLARE(void) ftdm_channel_flush_dtmf(ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_time_t) ftdm_current_time_in_ms(void);
FT_DECLARE(ftdm_status_t) ftdm_span_poll_event(ftdm_span_t *span, uint32_t ms);
FT_DECLARE(ftdm_status_t) ftdm_span_next_event(ftdm_span_t *span, ftdm_event_t **event);
FT_DECLARE(ftdm_status_t) ftdm_span_find(uint32_t id, ftdm_span_t **span);
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
FT_DECLARE(ftdm_status_t) ftdm_channel_open_by_span(uint32_t span_id, ftdm_direction_t direction, ftdm_caller_data_t *caller_data, ftdm_channel_t **ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_channel_open_by_group(uint32_t group_id, ftdm_direction_t direction, ftdm_caller_data_t *caller_data, ftdm_channel_t **ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_channel_close(ftdm_channel_t **ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_channel_done(ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_channel_use(ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_channel_command(ftdm_channel_t *ftdmchan, ftdm_command_t command, void *obj);
FT_DECLARE(ftdm_status_t) ftdm_channel_wait(ftdm_channel_t *ftdmchan, ftdm_wait_flag_t *flags, int32_t to);
FT_DECLARE(ftdm_status_t) ftdm_channel_read(ftdm_channel_t *ftdmchan, void *data, ftdm_size_t *datalen);
FT_DECLARE(void) ftdm_generate_sln_silence(int16_t *data, uint32_t samples, uint32_t divisor);
FT_DECLARE(ftdm_status_t) ftdm_channel_write(ftdm_channel_t *ftdmchan, void *data, ftdm_size_t datasize, ftdm_size_t *datalen);
FT_DECLARE(ftdm_status_t) ftdm_channel_add_var(ftdm_channel_t *ftdmchan, const char *var_name, const char *value);
FT_DECLARE(const char *) ftdm_channel_get_var(ftdm_channel_t *ftdmchan, const char *var_name);
FT_DECLARE(ftdm_status_t) ftdm_channel_clear_vars(ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_global_init(void);
FT_DECLARE(ftdm_status_t) ftdm_global_configuration(void);
FT_DECLARE(ftdm_status_t) ftdm_global_destroy(void);
FT_DECLARE(ftdm_status_t) ftdm_global_set_memory_handler(ftdm_memory_handler_t *handler);
FT_DECLARE(void) ftdm_global_set_crash_policy(ftdm_crash_policy_t policy);
FT_DECLARE(void) ftdm_global_set_logger(ftdm_logger_t logger);
FT_DECLARE(void) ftdm_global_set_default_logger(int level);
FT_DECLARE(uint32_t) ftdm_separate_string(char *buf, char delim, char **array, int arraylen);
FT_DECLARE(void) print_bits(uint8_t *b, int bl, char *buf, int blen, int e, uint8_t ss);
FT_DECLARE(void) print_hex_bytes(uint8_t *data, ftdm_size_t dlen, char *buf, ftdm_size_t blen);
FT_DECLARE_NONSTD(int) ftdm_hash_equalkeys(void *k1, void *k2);
FT_DECLARE_NONSTD(uint32_t) ftdm_hash_hashfromstring(void *ky);
FT_DECLARE(uint32_t) ftdm_running(void);
FT_DECLARE(ftdm_status_t) ftdm_channel_complete_state(ftdm_channel_t *ftdmchan);
FT_DECLARE(ftdm_status_t) ftdm_channel_init(ftdm_channel_t *ftdmchan);
FT_DECLARE(int) ftdm_load_modules(void);
FT_DECLARE(ftdm_status_t) ftdm_unload_modules(void);
FT_DECLARE(ftdm_status_t) ftdm_configure_span(const char *type, ftdm_span_t *span, fio_signal_cb_t sig_cb, ...);
FT_DECLARE(ftdm_status_t) ftdm_configure_span_signaling(const char *type, ftdm_span_t *span, fio_signal_cb_t sig_cb, ftdm_conf_parameter_t *parameters);
FT_DECLARE(ftdm_status_t) ftdm_span_start(ftdm_span_t *span);
FT_DECLARE(ftdm_status_t) ftdm_span_stop(ftdm_span_t *span);
FT_DECLARE(ftdm_status_t) ftdm_span_send_signal(ftdm_span_t *span, ftdm_sigmsg_t *sigmsg);
FT_DECLARE(char *) ftdm_build_dso_path(const char *name, char *path, ftdm_size_t len);
FT_DECLARE(ftdm_status_t) ftdm_global_add_io_interface(ftdm_io_interface_t *io_interface);
FT_DECLARE(int) ftdm_load_module(const char *name);
FT_DECLARE(int) ftdm_load_module_assume(const char *name);
FT_DECLARE(ftdm_status_t) ftdm_span_find_by_name(const char *name, ftdm_span_t **span);
FT_DECLARE(char *) ftdm_api_execute(const char *type, const char *cmd);
FT_DECLARE(int) ftdm_vasprintf(char **ret, const char *fmt, va_list ap);
FT_DECLARE(ftdm_status_t) ftdm_channel_set_caller_data(ftdm_channel_t *ftdmchan, ftdm_caller_data_t *caller_data);
FT_DECLARE(void) ftdm_cpu_monitor_disable(void);

FIO_CODEC_FUNCTION(fio_slin2ulaw);
FIO_CODEC_FUNCTION(fio_ulaw2slin);
FIO_CODEC_FUNCTION(fio_slin2alaw);
FIO_CODEC_FUNCTION(fio_alaw2slin);
FIO_CODEC_FUNCTION(fio_ulaw2alaw);
FIO_CODEC_FUNCTION(fio_alaw2ulaw);

#ifdef DEBUG_LOCKS
#define ftdm_mutex_lock(_x) printf("++++++lock %s:%d\n", __FILE__, __LINE__) && _ftdm_mutex_lock(_x)
#define ftdm_mutex_trylock(_x) printf("++++++try %s:%d\n", __FILE__, __LINE__) && _ftdm_mutex_trylock(_x)
#define ftdm_mutex_unlock(_x) printf("------unlock %s:%d\n", __FILE__, __LINE__) && _ftdm_mutex_unlock(_x)
#else 
#define ftdm_mutex_lock(_x) _ftdm_mutex_lock(_x)
#define ftdm_mutex_trylock(_x) _ftdm_mutex_trylock(_x)
#define ftdm_mutex_unlock(_x) _ftdm_mutex_unlock(_x)
#endif

/*!
  \brief Assert condition
*/
#define ftdm_assert(assertion, msg) \
	if (!(assertion)) { \
		ftdm_log(FTDM_LOG_CRIT, msg); \
		if (g_ftdm_crash_policy & FTDM_CRASH_ON_ASSERT) { \
			ftdm_abort();  \
		} \
	}

/*!
  \brief Assert condition and return
*/
#define ftdm_assert_return(assertion, retval, msg) \
	if (!(assertion)) { \
		ftdm_log(FTDM_LOG_CRIT, msg); \
		if (g_ftdm_crash_policy & FTDM_CRASH_ON_ASSERT) { \
			ftdm_abort();  \
		} else { \
			return retval; \
		} \
	}

/*!
  \brief Allocate uninitialized memory
  \command chunksize the chunk size
*/
#define ftdm_malloc(chunksize) g_ftdm_mem_handler.malloc(g_ftdm_mem_handler.pool, chunksize)

/*!
  \brief Allocate initialized memory
  \command chunksize the chunk size
*/
#define ftdm_calloc(elements, chunksize) g_ftdm_mem_handler.calloc(g_ftdm_mem_handler.pool, elements, chunksize)

/*!
  \brief Free chunk of memory
  \command chunksize the chunk size
*/
#define ftdm_free(chunk) g_ftdm_mem_handler.free(g_ftdm_mem_handler.pool, chunk)

/*!
  \brief Free a pointer and set it to NULL unless it already is NULL
  \command it the pointer
*/
#define ftdm_safe_free(it) if (it) { ftdm_free(it); it = NULL; }

/*!
  \brief Socket the given socket
  \command it the socket
*/
#define ftdm_socket_close(it) if (it > -1) { close(it); it = -1;}

#define ftdm_array_len(array) sizeof(array)/sizeof(array[0])

static __inline__ void ftdm_abort(void)
{
#ifdef __cplusplus
	::abort();
#else
	abort();
#endif
}

static __inline__ void ftdm_set_state_all(ftdm_span_t *span, ftdm_channel_state_t state)
{
	uint32_t j;
	ftdm_mutex_lock(span->mutex);
	for(j = 1; j <= span->chan_count; j++) {
		ftdm_set_state_locked((span->channels[j]), state);
	}
	ftdm_mutex_unlock(span->mutex);
}

static __inline__ int ftdm_check_state_all(ftdm_span_t *span, ftdm_channel_state_t state)
{
	uint32_t j;
	for(j = 1; j <= span->chan_count; j++) {
		if (span->channels[j]->state != state || ftdm_test_flag(span->channels[j], FTDM_CHANNEL_STATE_CHANGE)) {
			return 0;
		}
	}

	return 1;
}

static __inline__ void ftdm_set_flag_all(ftdm_span_t *span, uint32_t flag)
{
	uint32_t j;
	ftdm_mutex_lock(span->mutex);
	for(j = 1; j <= span->chan_count; j++) {
		ftdm_set_flag_locked((span->channels[j]), flag);
	}
	ftdm_mutex_unlock(span->mutex);
}

static __inline__ void ftdm_clear_flag_all(ftdm_span_t *span, uint32_t flag)
{
	uint32_t j;
	ftdm_mutex_lock(span->mutex);
	for(j = 1; j <= span->chan_count; j++) {
		ftdm_clear_flag_locked((span->channels[j]), flag);
	}
	ftdm_mutex_unlock(span->mutex);
}

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
