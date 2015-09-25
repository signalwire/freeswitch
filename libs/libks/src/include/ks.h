/*
 * Copyright (c) 2007-2014, Anthony Minessale II
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

#ifndef _KS_H_
#define _KS_H_

#include <stdarg.h>
#include <ks_types.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

#define ks_copy_string(_x, _y, _z) strncpy(_x, _y, _z - 1)
#define ks_set_string(_x, _y) ks_copy_string(_x, _y, sizeof(_x))


#if (_MSC_VER >= 1400)			// VC8+
#define ks_assert(expr) assert(expr);__analysis_assume( expr )
#endif

#ifndef ks_assert
#define ks_assert(_x) assert(_x)
#endif

#define ks_safe_free(_x) if (_x) free(_x); _x = NULL
#define ks_strlen_zero(s) (!s || *(s) == '\0')
#define ks_strlen_zero_buf(s) (*(s) == '\0')
#define end_of(_s) *(*_s == '\0' ? _s : _s + strlen(_s) - 1)

#include "math.h"
#include "ks_json.h"

#define BUF_CHUNK 65536 * 50
#define BUF_START 65536 * 100

#include <ks_threadmutex.h>
#include <ks_buffer.h>

#define ks_test_flag(obj, flag) ((obj)->flags & flag)
#define ks_set_flag(obj, flag) (obj)->flags |= (flag)
#define ks_clear_flag(obj, flag) (obj)->flags &= ~(flag)


KS_DECLARE(int) ks_vasprintf(char **ret, const char *fmt, va_list ap);

KS_DECLARE_DATA extern ks_logger_t ks_log;

/*! Sets the logger for libks. Default is the null_logger */
KS_DECLARE(void) ks_global_set_logger(ks_logger_t logger);
/*! Sets the default log level for libks */
KS_DECLARE(void) ks_global_set_default_logger(int level);


#include "ks_threadmutex.h"
#include "ks_config.h"
#include "ks_buffer.h"
#include "mpool.h"
#include "simclist.h"
#include "table.h"

KS_DECLARE(size_t) ks_url_encode(const char *url, char *buf, size_t len);
KS_DECLARE(char *)ks_url_decode(char *s);
KS_DECLARE(const char *)ks_stristr(const char *instr, const char *str);
KS_DECLARE(int) ks_toupper(int c);
KS_DECLARE(int) ks_tolower(int c);
KS_DECLARE(int) ks_snprintf(char *buffer, size_t count, const char *fmt, ...);


KS_DECLARE(int) ks_wait_sock(ks_socket_t sock, uint32_t ms, ks_poll_t flags);

KS_DECLARE(unsigned int) ks_separate_string_string(char *buf, const char *delim, char **array, unsigned int arraylen);

#define ks_recv(_h) ks_recv_event(_h, 0, NULL)
#define ks_recv_timed(_h, _ms) ks_recv_event_timed(_h, _ms, 0, NULL)

static __inline__ int ks_safe_strcasecmp(const char *s1, const char *s2)
{
	if (!(s1 && s2)) {
		return 1;
	}

	return strcasecmp(s1, s2);
}

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */


#endif /* defined(_KS_H_) */

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
