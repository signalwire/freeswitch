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

#ifndef OPENZAP_H
#define OPENZAP_H

#define _XOPEN_SOURCE 500

#ifndef HAVE_STRINGS_H
#define HAVE_STRINGS_H 1
#endif
#ifndef HAVE_SYS_SOCKET_H
#define HAVE_SYS_SOCKET_H 1
#endif

#ifndef __WINDOWS__
#if defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32)
#define __WINDOWS__
#endif
#endif

#ifdef _MSC_VER
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
#undef HAVE_STRINGS_H
#undef HAVE_SYS_SOCKET_H
/* disable warning for zero length array in a struct */
/* this will cause errors on c99 and ansi compliant compilers and will need to be fixed in the wanpipe header files */
#pragma warning(disable:4706)
#endif

#define ZAP_ENUM_NAMES(_NAME, _STRINGS) static char * _NAME [] = { _STRINGS , NULL };
#define ZAP_STR2ENUM_P(_FUNC1, _FUNC2, _TYPE) _TYPE _FUNC1 (char *name); char * _FUNC2 (_TYPE type);
#define ZAP_STR2ENUM(_FUNC1, _FUNC2, _TYPE, _STRINGS, _MAX)	\
	_TYPE _FUNC1 (char *name)								\
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
	char * _FUNC2 (_TYPE type)								\
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

#ifndef WIN32
#include <time.h>
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
#include "Q931.h"
#include "Q921.h"

#define XX if (0)

#ifdef  NDEBUG
#undef assert
#define assert(_Expression)     ((void)(_Expression))
#endif

#define ZAP_MAX_CHANNELS_SPAN 513
#define ZAP_MAX_SPANS_INTERFACE 33

#define GOTO_STATUS(label,st) status = st; goto label ;

#define zap_copy_string(x,y,z) strncpy(x, y, z - 1) 


#define zap_channel_test_feature(obj, flag) ((obj)->features & flag)
#define zap_channel_set_feature(obj, flag) (obj)->features |= (flag)
#define zap_channel_clear_feature(obj, flag) (obj)->features &= ~(flag)

/*!
  \brief Test for the existance of a flag on an arbitary object
  \command obj the object to test
  \command flag the or'd list of flags to test
  \return true value if the object has the flags defined
*/
#define zap_test_flag(obj, flag) ((obj)->flags & flag)

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

/*!
  \brief Clear a flag on an arbitrary object while locked
  \command obj the object to test
  \command flag the or'd list of flags to clear
*/
#define zap_clear_flag(obj, flag) (obj)->flags &= ~(flag)

#define zap_clear_flag_locked(obj, flag) assert(obj->mutex != NULL); zap_mutex_lock(obj->mutex); (obj)->flags &= ~(flag); zap_mutex_unlock(obj->mutex);

#define zap_set_state_locked(obj, s) assert(obj->mutex != NULL); zap_mutex_lock(obj->mutex); obj->state = s; zap_mutex_unlock(obj->mutex);

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


struct zap_event {
	zap_event_type_t e_type;
	uint32_t enum_id;
	zap_channel_t *channel;
	void *data;
};

struct zap_channel {
	uint32_t span_id;
	uint32_t chan_id;
	zap_chan_type_t type;
	zap_socket_t sockfd;
	zap_channel_flag_t flags;
	zap_channel_feature_t features;
	zap_codec_t effective_codec;
	zap_codec_t native_codec;
	uint32_t effective_interval;
	uint32_t native_interval;
	uint32_t packet_len;
	zap_channel_state_t state;
	zap_mutex_t *mutex;
	teletone_dtmf_detect_state_t dtmf_detect;
	zap_event_t event_header;
	char last_error[256];
	zio_event_cb_t event_callback;
	void *mod_data;
	uint32_t skip_read_frames;
	zap_buffer_t *dtmf_buffer;
	zap_buffer_t *digit_buffer;
	uint32_t dtmf_on;
	uint32_t dtmf_off;
	teletone_generation_session_t tone_session;
	zap_time_t last_event_time;
	struct zap_span *span;
	struct zap_io_interface *zio;
};


struct zap_sigmsg {
	zap_signal_event_t event_id;
	uint32_t chan_id;
	char cid_name[80];
	char ani[25];
	char aniII[25];
	char dnis[25];
};


struct zap_isdn_data {
	Q921Data_t q921;
	Q931_TrunkInfo_t q931;
	zap_channel_t *dchan;
	zap_channel_t *dchans[2];
	struct zap_sigmsg sigmsg;
	zio_signal_cb_t sig_cb;
	uint32_t flags;
};

struct zap_analog_data {
	uint32_t flags;
};

struct zap_span {
	uint32_t span_id;
	uint32_t chan_count;
	zap_span_flag_t flags;
	struct zap_io_interface *zio;
	zio_event_cb_t event_callback;
	zap_mutex_t *mutex;
	zap_trunk_type_t trunk_type;
	zap_signal_type_t signal_type;
	struct zap_isdn_data *isdn_data;
	struct zap_analog_data *analog_data;
	zap_event_t event_header;
	char last_error[256];
	char tone_map[ZAP_TONEMAP_INVALID+1][ZAP_TONEMAP_LEN];
	zap_channel_t channels[ZAP_MAX_CHANNELS_SPAN];
};


extern zap_logger_t zap_log;

struct zap_io_interface {
	const char *name;
	zio_configure_t configure;
	zio_open_t open;
	zio_close_t close;
	zio_command_t command;
	zio_wait_t wait;
	zio_read_t read;
	zio_write_t write;
	zio_span_poll_event_t poll_event;
	zio_span_next_event_t next_event;
	uint32_t span_index;
	struct zap_span spans[ZAP_MAX_SPANS_INTERFACE];
};


zap_status_t zap_span_load_tones(zap_span_t *span, char *mapname);
zap_size_t zap_channel_dequeue_dtmf(zap_channel_t *zchan, char *dtmf, zap_size_t len);
zap_status_t zap_channel_queue_dtmf(zap_channel_t *zchan, const char *dtmf);
zap_time_t zap_current_time_in_ms(void);
zap_status_t zap_span_poll_event(zap_span_t *span, uint32_t ms);
zap_status_t zap_span_next_event(zap_span_t *span, zap_event_t **event);
zap_status_t zap_span_find(const char *name, uint32_t id, zap_span_t **span);
zap_status_t zap_span_create(zap_io_interface_t *zio, zap_span_t **span);
zap_status_t zap_span_close_all(zap_io_interface_t *zio);
zap_status_t zap_span_add_channel(zap_span_t *span, zap_socket_t sockfd, zap_chan_type_t type, zap_channel_t **chan);
zap_status_t zap_span_set_event_callback(zap_span_t *span, zio_event_cb_t event_callback);
zap_status_t zap_channel_set_event_callback(zap_channel_t *zchan, zio_event_cb_t event_callback);
zap_status_t zap_channel_open(const char *name, uint32_t span_id, uint32_t chan_id, zap_channel_t **zchan);
zap_status_t zap_channel_open_chan(zap_channel_t *zchan);
zap_status_t zap_channel_open_any(const char *name, uint32_t span_id, zap_direction_t direction, zap_channel_t **zchan);
zap_status_t zap_channel_close(zap_channel_t **zchan);
zap_status_t zap_channel_command(zap_channel_t *zchan, zap_command_t command, void *obj);
zap_status_t zap_channel_wait(zap_channel_t *zchan, zap_wait_flag_t *flags, int32_t to);
zap_status_t zap_channel_read(zap_channel_t *zchan, void *data, zap_size_t *datalen);
zap_status_t zap_channel_write(zap_channel_t *zchan, void *data, zap_size_t *datalen);
zap_status_t zap_global_init(void);
zap_status_t zap_global_destroy(void);
void zap_global_set_logger(zap_logger_t logger);
void zap_global_set_default_logger(int level);
uint32_t zap_separate_string(char *buf, char delim, char **array, int arraylen);
void print_bits(uint8_t *b, int bl, char *buf, int blen, int e);


#endif
