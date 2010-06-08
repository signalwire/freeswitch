/*
 * Copyright (c) 2010, Sangoma Technologies
 * Moises Silva <moy@sangoma.com>
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

#include "freetdm.h"

#ifndef __PRIVATE_FTDM_CORE__
#define __PRIVATE_FTDM_CORE__

#if !defined(_XOPEN_SOURCE) && !defined(__FreeBSD__)
#define _XOPEN_SOURCE 600
#endif

#ifndef HAVE_STRINGS_H
#define HAVE_STRINGS_H 1
#endif
#ifndef HAVE_SYS_SOCKET_H
#define HAVE_SYS_SOCKET_H 1
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
#include "ftdm_sched.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#define ftdm_set_state(obj, s) ftdm_channel_set_state(__FILE__, __FUNCTION__, __LINE__, obj, s, 0);									\

#define ftdm_set_state_locked(obj, s) \
	do { \
		ftdm_channel_lock(obj); \
		ftdm_channel_set_state(__FILE__, __FUNCTION__, __LINE__, obj, s, 0);									\
		ftdm_channel_unlock(obj); \
	} while(0);

#define ftdm_set_state_r(obj, s, r) r = ftdm_channel_set_state(__FILE__, __FUNCTION__, __LINE__, obj, s, 0);

#ifdef _MSC_VER
/* The while(0) below throws a conditional expression is constant warning */
#pragma warning(disable:4127) 
#endif

/* this macro assumes obj is locked! */
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
#define FTDM_STANDARD_STREAM(s) memset(&s, 0, sizeof(s)); s.data = ftdm_malloc(FTDM_CMD_CHUNK_LEN); \
	assert(s.data);														\
	memset(s.data, 0, FTDM_CMD_CHUNK_LEN);								\
	s.end = s.data;														\
	s.data_size = FTDM_CMD_CHUNK_LEN;									\
	s.write_function = ftdm_console_stream_write;						\
	s.raw_write_function = ftdm_console_stream_raw_write;				\
	s.alloc_len = FTDM_CMD_CHUNK_LEN;									\
	s.alloc_chunk = FTDM_CMD_CHUNK_LEN

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

FT_DECLARE_DATA extern ftdm_queue_handler_t g_ftdm_queue_handler;

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


typedef enum {
	FTDM_TYPE_NONE,
	FTDM_TYPE_SPAN = 0xFF,
	FTDM_TYPE_CHANNEL
} ftdm_data_type_t;

/* 2^8 table size, one for each byte (sample) value */
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
	void *user_private;
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

FT_DECLARE_DATA extern ftdm_crash_policy_t g_ftdm_crash_policy;

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
FT_DECLARE(ftdm_status_t) ftdm_channel_send_fsk_data(ftdm_channel_t *ftdmchan, ftdm_fsk_data_state_t *fsk_data, float db_level);

FT_DECLARE(ftdm_status_t) ftdm_channel_set_state(const char *file, const char *func, int line,
		ftdm_channel_t *ftdmchan, ftdm_channel_state_t state, int wait);

FT_DECLARE(ftdm_status_t) ftdm_span_load_tones(ftdm_span_t *span, const char *mapname);
FT_DECLARE(ftdm_time_t) ftdm_current_time_in_ms(void);

FT_DECLARE(ftdm_status_t) ftdm_channel_use(ftdm_channel_t *ftdmchan);

FT_DECLARE(void) ftdm_generate_sln_silence(int16_t *data, uint32_t samples, uint32_t divisor);

FT_DECLARE(uint32_t) ftdm_separate_string(char *buf, char delim, char **array, int arraylen);
FT_DECLARE(void) print_bits(uint8_t *b, int bl, char *buf, int blen, int e, uint8_t ss);
FT_DECLARE(void) print_hex_bytes(uint8_t *data, ftdm_size_t dlen, char *buf, ftdm_size_t blen);

FT_DECLARE_NONSTD(int) ftdm_hash_equalkeys(void *k1, void *k2);
FT_DECLARE_NONSTD(uint32_t) ftdm_hash_hashfromstring(void *ky);

FT_DECLARE(ftdm_status_t) ftdm_channel_complete_state(ftdm_channel_t *ftdmchan);

FT_DECLARE(int) ftdm_load_modules(void);

FT_DECLARE(ftdm_status_t) ftdm_unload_modules(void);

FT_DECLARE(ftdm_status_t) ftdm_span_send_signal(ftdm_span_t *span, ftdm_sigmsg_t *sigmsg);

FT_DECLARE(void) ftdm_channel_clear_needed_tones(ftdm_channel_t *ftdmchan);
FT_DECLARE(void) ftdm_channel_rotate_tokens(ftdm_channel_t *ftdmchan);

FT_DECLARE(int) ftdm_load_module(const char *name);
FT_DECLARE(int) ftdm_load_module_assume(const char *name);
FT_DECLARE(int) ftdm_vasprintf(char **ret, const char *fmt, va_list ap);
FT_DECLARE(ftdm_status_t) ftdm_channel_done(ftdm_channel_t *ftdmchan);

FT_DECLARE(ftdm_status_t) ftdm_span_close_all(void);
FT_DECLARE(ftdm_status_t) ftdm_channel_open_chan(ftdm_channel_t *ftdmchan);

/*! 
 * \brief Retrieves an event from the span
 *
 * \note
 * 	This function is non-reentrant and not thread-safe. 
 * 	The event returned may be modified if the function is called again 
 * 	from a different thread or even the same. It is recommended to
 * 	handle events from the same span in a single thread.
 * 	WARNING: this function used to be public ( in freetdm.h )
 * 	but since is really of no use to users better keep it here
 *
 * \param span The span to retrieve the event from
 * \param event Pointer to store the pointer to the event
 *
 * \retval FTDM_SUCCESS success (at least one event available)
 * \retval FTDM_TIMEOUT Timed out waiting for events
 * \retval FTDM_FAIL failure
 */
FT_DECLARE(ftdm_status_t) ftdm_span_next_event(ftdm_span_t *span, ftdm_event_t **event);

/*! 
 * \brief Enqueue a DTMF string into the channel
 *
 * \param ftdmchan The channel to enqueue the dtmf string to
 * \param dtmf null-terminated DTMF string
 *
 * \retval FTDM_SUCCESS success
 * \retval FTDM_FAIL failure
 */
FT_DECLARE(ftdm_status_t) ftdm_channel_queue_dtmf(ftdm_channel_t *ftdmchan, const char *dtmf);


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
  \brief Socket the given socket
  \command it the socket
*/
#define ftdm_socket_close(it) if (it > -1) { close(it); it = -1;}

#define ftdm_array_len(array) sizeof(array)/sizeof(array[0])

#define ftdm_channel_lock(chan) ftdm_mutex_lock(chan->mutex)
#define ftdm_channel_unlock(chan) ftdm_mutex_unlock(chan->mutex)
#define ftdm_log_chan_ex(fchan, file, func, line, level, format, ...) ftdm_log(file, func, line, level, "s%dc%d " format, fchan->span_id, fchan->chan_id, __VA_ARGS__)
#define ftdm_log_chan(fchan, level, format, ...) ftdm_log(level, "s%dc%d " format, fchan->span_id, fchan->chan_id, __VA_ARGS__)
#define ftdm_log_chan_msg(fchan, level, msg) ftdm_log(level, "s%dc%d " msg, fchan->span_id, fchan->chan_id)

FT_DECLARE_DATA extern const char *FTDM_LEVEL_NAMES[9];

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
		if (!FTDM_IS_DCHAN(span->channels[j])) {
			ftdm_set_state_locked((span->channels[j]), state);
		}
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
}
#endif

#endif /* endif __PRIVATE_FTDM_CORE__ */
