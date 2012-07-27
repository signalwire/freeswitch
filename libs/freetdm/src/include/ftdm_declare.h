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

#ifndef __FTDM_DECLARE_H__
#define __FTDM_DECLARE_H__

#ifdef __cplusplus
extern "C" {
#endif

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
#define FT_DECLARE_INLINE(type)		extern __inline__ type /* why extern? see http://support.microsoft.com/kb/123768 */
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
#define FT_DECLARE_INLINE(type)		__inline__ type
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

/*
 * Compiler-specific format checking attributes
 * use these on custom functions that use printf/scanf-style
 * format strings (e.g. ftdm_log())
 */
#if defined(__GNUC__)
/**
 * Enable compiler-specific printf()-style format and argument checks on a function
 * @param	fmtp	Position of printf()-style format string parameter
 * @param	argp	Position of variable argument list ("...") parameter
 * @code
 *	void log(const int level, const char *fmt, ...) __ftdm_check_printf(2, 3);
 * @endcode
 */
#define __ftdm_check_printf(fmtp, argp) __attribute__((format (printf, fmtp, argp)))
/**
 * Enable compiler-specific scanf()-style format and argument checks on a function
 * @param	fmtp	Position of scanf()-style format string parameter
 * @param	argp	Position of variable argument list ("...") parameter
 * @code
 *	void parse(struct foo *ctx, const char *fmt, ...) __ftdm_check_scanf(2, 3);
 * @endcode
 */
#define __ftdm_check_scanf(fmtp, argp) __attribute__((format (scanf, fmtp, argp)))
#else
#define __ftdm_check_printf(fmtp, argp)
#define __ftdm_check_scanf(fmtp, argp)
#endif


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

#ifdef __WINDOWS__
#include <stdio.h>
#include <windows.h>
#define FTDM_INVALID_SOCKET INVALID_HANDLE_VALUE
typedef HANDLE ftdm_socket_t;
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8 uint8_t;
typedef __int64 int64_t;
typedef __int32 int32_t;
typedef __int16 int16_t;
typedef __int8 int8_t;
#define FTDM_O_BINARY O_BINARY
#define FTDM_SIZE_FMT "Id"
#define FTDM_INT64_FMT "lld"
#define FTDM_UINT64_FMT "llu"
#define FTDM_XINT64_FMT "llx"
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif /* _MSC_VER */
#else /* __WINDOWS__ */
#define FTDM_O_BINARY 0
#define FTDM_SIZE_FMT "zd"
#if (defined(__SIZEOF_LONG__) && (__SIZEOF_LONG__ == 8)) || defined(__LP64__) || defined(__LLP64__)
#define FTDM_INT64_FMT "ld"
#define FTDM_UINT64_FMT "lu"
#define FTDM_XINT64_FMT "lx"
#else
#define FTDM_INT64_FMT "lld"
#define FTDM_UINT64_FMT "llu"
#define FTDM_XINT64_FMT "llx"
#endif
#define FTDM_INVALID_SOCKET -1
typedef int ftdm_socket_t;
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#endif

/*! \brief FreeTDM APIs possible return codes */
typedef enum {
	FTDM_SUCCESS, /*!< Success */
	FTDM_FAIL, /*!< Failure, generic error return code when no more specific return code can be used */

	FTDM_MEMERR, /*!< Allocation failure */
	FTDM_ENOMEM = FTDM_MEMERR,

	FTDM_TIMEOUT, /*!< Operation timed out (ie: polling on a device)*/
	FTDM_ETIMEDOUT = FTDM_TIMEOUT,

	FTDM_NOTIMPL, /*!< Operation not implemented */
	FTDM_ENOSYS = FTDM_NOTIMPL, /*!< The function is not implemented */

	FTDM_BREAK, /*!< Request the caller to perform a break (context-dependant, ie: stop getting DNIS/ANI) */

	/*!< Any new return codes should try to mimc unix style error codes, no need to reinvent */
	FTDM_EINVAL, /*!< Invalid argument */
	FTDM_ECANCELED, /*!< Operation cancelled */
	FTDM_EBUSY, /*!< Device busy */
} ftdm_status_t;

/*! \brief FreeTDM bool type. */
typedef enum {
	FTDM_FALSE,
	FTDM_TRUE
} ftdm_bool_t;

/*! \brief I/O waiting flags */
typedef enum {
	FTDM_NO_FLAGS = 0,
	FTDM_READ =  (1 << 0),
	FTDM_WRITE = (1 << 1),
	FTDM_EVENTS = (1 << 2)
} ftdm_wait_flag_t;

/*! 
 * \brief FreeTDM channel.
 *        This is the basic data structure used to place calls and I/O operations
 */
typedef struct ftdm_channel ftdm_channel_t;

/*! 
 * \brief FreeTDM span.
 *        Channel and signaling configuration container.
 *        This is a logical span structure, a span may ( or may note ) contain channels
 *        of other physical spans, depending on configuration (freetdm.conf) or if you
 *        are not using configuration depends on how you call ftdm_span_add_channel
 */
typedef struct ftdm_span ftdm_span_t;

typedef struct ftdm_event ftdm_event_t;
typedef struct ftdm_conf_node ftdm_conf_node_t;
typedef struct ftdm_group ftdm_group_t;
typedef size_t ftdm_size_t;
typedef struct ftdm_sigmsg ftdm_sigmsg_t;
typedef struct ftdm_usrmsg ftdm_usrmsg_t;
typedef struct ftdm_io_interface ftdm_io_interface_t;
typedef struct ftdm_stream_handle ftdm_stream_handle_t;
typedef struct ftdm_queue ftdm_queue_t;
typedef struct ftdm_memory_handler ftdm_memory_handler_t;

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
