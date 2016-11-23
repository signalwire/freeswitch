/*
 * Copyright (c) 2007-2015, Anthony Minessale II
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

#ifndef _KS_TYPES_H_
#define _KS_TYPES_H_

#include "ks.h"

KS_BEGIN_EXTERN_C

#define KS_STR2ENUM_P(_FUNC1, _FUNC2, _TYPE) KS_DECLARE(_TYPE) _FUNC1 (const char *name); KS_DECLARE(const char *) _FUNC2 (_TYPE type);                       

#define KS_STR2ENUM(_FUNC1, _FUNC2, _TYPE, _STRINGS, _MAX)  \
    KS_DECLARE(_TYPE) _FUNC1 (const char *name)             \
    {                                                       \
        int i;                                              \
        _TYPE t = _MAX ;                                    \
                                                            \
        for (i = 0; i < _MAX ; i++) {                       \
            if (!strcasecmp(name, _STRINGS[i])) {           \
                t = (_TYPE) i;                              \
                break;                                      \
            }                                               \
        }                                                   \
                                                            \
        return t;                                           \
    }                                                       \
    KS_DECLARE(const char *) _FUNC2 (_TYPE type)            \
    {                                                       \
        if (type > _MAX) {                                  \
            type = _MAX;                                    \
        }                                                   \
        return _STRINGS[(int)type];                         \
    }                                                       \

#define KS_ENUM_NAMES(_NAME, _STRINGS) static const char * _NAME [] = { _STRINGS , NULL };  

#define KS_VA_NONE "%s", ""

	typedef enum {
		KS_POLL_READ = (1 << 0),
		KS_POLL_WRITE = (1 << 1),
		KS_POLL_ERROR = (1 << 2)
	} ks_poll_t;

	typedef uint16_t ks_port_t;
	typedef size_t ks_size_t;

	typedef enum {
		KS_STATUS_SUCCESS,
		KS_STATUS_FAIL,
		KS_STATUS_BREAK,
		KS_STATUS_DISCONNECTED,
		KS_STATUS_GENERR,
		KS_STATUS_INACTIVE,
		KS_STATUS_TIMEOUT,
		/* Memory pool errors */
		KS_STATUS_ARG_NULL,        /* function argument is null */
		KS_STATUS_ARG_INVALID,     /* function argument is invalid */
		KS_STATUS_PNT,	           /* invalid ks_pool pointer */
		KS_STATUS_POOL_OVER,	   /* ks_pool structure was overwritten */
		KS_STATUS_PAGE_SIZE,	   /* could not get system page-size */
		KS_STATUS_OPEN_ZERO,	   /* could not open /dev/zero */
		KS_STATUS_NO_MEM,	       /* no memory available */
		KS_STATUS_MMAP,	           /* problems with mmap */
		KS_STATUS_SIZE,	           /* error processing requested size */
		KS_STATUS_TOO_BIG,	       /* allocation exceeded max size */
		KS_STATUS_MEM,	           /* invalid memory address */
		KS_STATUS_MEM_OVER,	       /* memory lower bounds overwritten */
		KS_STATUS_NOT_FOUND,	   /* memory block not found in pool */
		KS_STATUS_IS_FREE,	       /* memory block already free */
		KS_STATUS_BLOCK_STAT,      /* invalid internal block status */
		KS_STATUS_FREE_ADDR,	   /* invalid internal free address */
		KS_STATUS_NO_PAGES,	       /* ran out of pages in pool */
		KS_STATUS_ALLOC,	       /* calloc,malloc,free,realloc failed */
		KS_STATUS_PNT_OVER,	       /* pointer structure was overwritten */
		KS_STATUS_INVALID_POINTER, /* address is not valid */
		/* Always insert new entries above this line*/
		KS_STATUS_COUNT
	} ks_status_t;

#define STATUS_STRINGS\
	"SUCCESS",\
	"FAIL",\
	"BREAK",\
	"DISCONNECTED",\
	"GENERR",\
	"INACTIVE",\
	"TIMEOUT",\
	"ARG_NULL",\
	"ARG_INVALID",\
	"PNT",\
	"POOL_OVER",\
	"PAGE_SIZE",\
	"OPEN_ZERO",\
	"NO_MEM",\
	"MMAP",\
	"SIZE",\
	"TOO_BIG",\
	"MEM",\
	"MEM_OVER",\
	"NOT_FOUN",\
	"IS_FREE",\
	"BLOCK_STAT",\
	"FREE_ADDR",\
	"NO_PAGES",\
	"ALLOC",\
	"PNT_OVER",\
	"INVALID_POINTER",\
	/* insert new entries before this */\
	"COUNT"

	KS_STR2ENUM_P(ks_str2ks_status, ks_status2str, ks_status_t)  

/*! \brief Used internally for truth test */
	typedef enum {
		KS_TRUE = 1,
		KS_FALSE = 0
	} ks_bool_t;

#ifndef __FUNCTION__
#define __FUNCTION__ (const char *)__func__
#endif

#define KS_PRE __FILE__, __FUNCTION__, __LINE__
#define KS_LOG_LEVEL_DEBUG 7
#define KS_LOG_LEVEL_INFO 6
#define KS_LOG_LEVEL_NOTICE 5
#define KS_LOG_LEVEL_WARNING 4
#define KS_LOG_LEVEL_ERROR 3
#define KS_LOG_LEVEL_CRIT 2
#define KS_LOG_LEVEL_ALERT 1
#define KS_LOG_LEVEL_EMERG 0

#define KS_LOG_DEBUG KS_PRE, KS_LOG_LEVEL_DEBUG
#define KS_LOG_INFO KS_PRE, KS_LOG_LEVEL_INFO
#define KS_LOG_NOTICE KS_PRE, KS_LOG_LEVEL_NOTICE
#define KS_LOG_WARNING KS_PRE, KS_LOG_LEVEL_WARNING
#define KS_LOG_ERROR KS_PRE, KS_LOG_LEVEL_ERROR
#define KS_LOG_CRIT KS_PRE, KS_LOG_LEVEL_CRIT
#define KS_LOG_ALERT KS_PRE, KS_LOG_LEVEL_ALERT
#define KS_LOG_EMERG KS_PRE, KS_LOG_LEVEL_EMERG

struct ks_pool_s;

typedef struct ks_pool_s ks_pool_t;
typedef void (*ks_hash_destructor_t)(void *ptr);

typedef enum {
	KS_MPCL_ANNOUNCE,
	KS_MPCL_TEARDOWN,
	KS_MPCL_DESTROY
} ks_pool_cleanup_action_t;

typedef enum {
	KS_MPCL_FREE,
	KS_MPCL_GLOBAL_FREE,
} ks_pool_cleanup_type_t;

typedef union {
	struct sockaddr_in v4;
	struct sockaddr_in6 v6;
} ks_sockaddr_in_t;

typedef struct {
	int family;
	ks_sockaddr_in_t v;
	ks_port_t port;
	char host[48];
} ks_sockaddr_t;

typedef void (*ks_pool_cleanup_fn_t) (ks_pool_t *mpool, void *ptr, void *arg, int type, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t ctype);

typedef void (*ks_logger_t) (const char *file, const char *func, int line, int level, const char *fmt, ...);
typedef void (*ks_listen_callback_t) (ks_socket_t server_sock, ks_socket_t client_sock, ks_sockaddr_t *addr, void *user_data);

typedef int64_t ks_time_t;

struct ks_q_s;
typedef struct ks_q_s ks_q_t;
typedef void (*ks_flush_fn_t)(ks_q_t *q, void *ptr, void *flush_data);

KS_END_EXTERN_C

#endif							/* defined(_KS_TYPES_H_) */

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
