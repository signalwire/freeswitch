/*
 * STFU (S)ort (T)ransportable (F)ramed (U)tterances
 * Copyright (c) 2007-2014 Anthony Minessale II <anthm@freeswitch.org>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE. 
 *
 * THOSE WHO DISAGREE MAY CERTAINLY STFU
 */

#ifndef STFU_H
#define STFU_H

#include <switch.h> 

#ifdef __cplusplus
extern "C" {
#endif
#ifdef __STUPIDFORMATBUG__
}
#endif

#if !defined(MACOSX) && !defined(_XOPEN_SOURCE) && !defined(__OpenBSD__) && !defined(__NetBSD__) && !defined(__cplusplus)
#define _XOPEN_SOURCE 600
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#if (_MSC_VER >= 1400)			// VC8+
#define stfu_assert(expr) assert(expr);__analysis_assume( expr )
#endif

#ifndef stfu_assert
#define stfu_assert(_x) assert(_x)
#endif

#ifdef  _MSC_VER
#if !defined(_STDINT) && !defined(uint32_t)
typedef unsigned __int8     uint8_t;
typedef unsigned __int16    uint16_t;
typedef unsigned __int32    uint32_t;
typedef unsigned __int64    uint64_t;
typedef __int8      int8_t;
typedef __int16     int16_t;
typedef __int32     int32_t;
typedef __int64     int64_t;
typedef unsigned long   in_addr_t;
#endif
#define snprintf _snprintf
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <inttypes.h>
#endif
#include <assert.h>

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
typedef SOCKET stfu_socket_t;
#ifndef _STDINT
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8 uint8_t;
typedef __int64 int64_t;
typedef __int32 int32_t;
typedef __int16 int16_t;
typedef __int8 int8_t;
#endif
typedef intptr_t stfu_ssize_t;
typedef int stfu_filehandle_t;
#define STFU_SOCK_INVALID INVALID_SOCKET
#define strerror_r(num, buf, size) strerror_s(buf, size, num)
#else
#include <stdint.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#define STFU_SOCK_INVALID -1
typedef int stfu_socket_t;
typedef ssize_t stfu_ssize_t;
typedef int stfu_filehandle_t;
#endif


#define STFU_PRE __FILE__, __SWITCH_FUNC__, __LINE__
#define STFU_LOG_LEVEL_DEBUG 7
#define STFU_LOG_LEVEL_INFO 6
#define STFU_LOG_LEVEL_NOTICE 5
#define STFU_LOG_LEVEL_WARNING 4
#define STFU_LOG_LEVEL_ERROR 3
#define STFU_LOG_LEVEL_CRIT 2
#define STFU_LOG_LEVEL_ALERT 1
#define STFU_LOG_LEVEL_EMERG 0

#define STFU_LOG_DEBUG STFU_PRE, STFU_LOG_LEVEL_DEBUG
#define STFU_LOG_INFO STFU_PRE, STFU_LOG_LEVEL_INFO
#define STFU_LOG_NOTICE STFU_PRE, STFU_LOG_LEVEL_NOTICE
#define STFU_LOG_WARNING STFU_PRE, STFU_LOG_LEVEL_WARNING
#define STFU_LOG_ERROR STFU_PRE, STFU_LOG_LEVEL_ERROR
#define STFU_LOG_CRIT STFU_PRE, STFU_LOG_LEVEL_CRIT
#define STFU_LOG_ALERT STFU_PRE, STFU_LOG_LEVEL_ALERT
#define STFU_LOG_EMERG STFU_PRE, STFU_LOG_LEVEL_EMERG
typedef void (*stfu_logger_t)(const char *file, const char *func, int line, int level, const char *fmt, ...);


int stfu_vasprintf(char **ret, const char *fmt, va_list ap);

extern stfu_logger_t stfu_log;

/*! Sets the logger for libstfu. Default is the null_logger */
void stfu_global_set_logger(stfu_logger_t logger);
/*! Sets the default log level for libstfu */
void stfu_global_set_default_logger(int level);

#define STFU_DATALEN 16384
#define STFU_QLEN 300
#define STFU_MAX_TRACK 256

typedef enum {
	STFU_IT_FAILED,
	STFU_IT_WORKED,
	STFU_IM_DONE,
	STFU_ITS_TOO_LATE
} stfu_status_t;

struct stfu_frame {
	uint32_t ts;
	uint16_t seq;
	uint32_t pt;
	uint8_t data[STFU_DATALEN];
	size_t dlen;
	uint8_t was_read;
	uint8_t plc;
};
typedef struct stfu_frame stfu_frame_t;

struct stfu_instance;
typedef struct stfu_instance stfu_instance_t;

typedef struct {
	uint32_t qlen;
	uint32_t packet_in_count;
	uint32_t clean_count;
	uint32_t consecutive_good_count;
	uint32_t consecutive_bad_count;
    double period_jitter_percent;
    double period_missing_percent;
} stfu_report_t;

typedef void (*stfu_n_call_me_t)(stfu_instance_t *i, void *);

void stfu_n_report(stfu_instance_t *i, stfu_report_t *r);
void stfu_n_destroy(stfu_instance_t **i);
stfu_instance_t *stfu_n_init(uint32_t qlen, uint32_t max_qlen, uint32_t samples_per_packet, uint32_t samples_per_second, uint32_t max_drift_ms);
stfu_status_t _stfu_n_resize(stfu_instance_t *i, uint32_t qlen, int line);
#define stfu_n_resize(_i, _ql) _stfu_n_resize(_i, _ql, __LINE__)
stfu_status_t stfu_n_add_data(stfu_instance_t *i, uint32_t ts, uint16_t seq, uint32_t pt, void *data, size_t datalen, uint32_t timer_ts, int last);
stfu_frame_t *stfu_n_read_a_frame(stfu_instance_t *i);
SWITCH_DECLARE(int32_t) stfu_n_copy_next_frame(stfu_instance_t *jb, uint32_t timestamp, uint16_t seq, uint16_t distance, stfu_frame_t *next_frame);
void _stfu_n_reset(stfu_instance_t *i, const char *file, const char *func, int line);
#define stfu_n_reset(_i) _stfu_n_reset(_i, STFU_PRE)
stfu_status_t stfu_n_sync(stfu_instance_t *i, uint32_t packets);
void stfu_n_call_me(stfu_instance_t *i, stfu_n_call_me_t callback, void *udata);
void stfu_n_debug(stfu_instance_t *i, const char *name);
int32_t stfu_n_get_drift(stfu_instance_t *i);
int32_t stfu_n_get_most_qlen(stfu_instance_t *i);

#define stfu_im_done(i) stfu_n_add_data(i, 0, 0, NULL, 0, 0, 1)
#define stfu_n_eat(i,t,s,p,d,l,tt) stfu_n_add_data(i, t, s, p, d, l, tt, 0)

#ifdef __cplusplus
}
#endif
#endif /*STFU_H*/

